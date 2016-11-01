/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Clement Foucault.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_probe.c
 *  \ingroup gpu
 *
 * Manages screen space effects inside materials and 
 * the necessary buffers:
 * - Screen space reflection
 *
 * NOTICE : These are not post process effects
 */


#include <math.h>
#include <string.h>
#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_blenlib.h"

#include "DNA_object_types.h"
#include "DNA_world_types.h"
#include "DNA_scene_types.h"

#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_draw.h"
#include "GPU_probe.h"

#include "gpu_codegen.h"

#include <string.h>

/* Structs */


/* Functions */

static void gpu_probe_buffers_free(GPUProbe *probe)
{
	if (probe->shtex) {
		GPU_texture_free(probe->shtex);
		probe->shtex = NULL;
	}
	if (probe->fbsh) {
		GPU_framebuffer_free(probe->fbsh);
		probe->fbsh = NULL;
	}
	if (probe->tex) {
		GPU_texture_free(probe->tex);
		probe->tex = NULL;
	}
	if (probe->texreflect) {
		GPU_texture_free(probe->texreflect);
		probe->texreflect = NULL;
	}
	if (probe->texrefract) {
		GPU_texture_free(probe->texrefract);
		probe->texrefract = NULL;
	}
	if (probe->depthtex) {
		GPU_texture_free(probe->depthtex);
		probe->depthtex = NULL;
	}
	if (probe->fb) {
		GPU_framebuffer_free(probe->fb);
		probe->fb = NULL;
	}
}

void GPU_probe_free(ListBase *gpuprobe)
{
	GPUProbe *probe;
	LinkData *link;

	for (link = gpuprobe->first; link; link = link->next) {
		probe = (GPUProbe *)link->data;

		gpu_probe_buffers_free(probe);

		if (probe->shcoefs) {
			MEM_freeN(probe->shcoefs);
		}

		MEM_freeN(probe);
	}

	BLI_freelistN(gpuprobe);
}

static void gpu_probe_from_blender(Scene *scene, World *wo, Object *ob, GPUProbe *probe)
{
	probe->scene = scene;
	probe->wo = wo;
	probe->ob = ob;
	probe->clipsta = 0.01f;
	probe->clipend = 1000.0f;

	if (ob) {
		probe->type = (ob->probetype == OB_PROBE_CUBEMAP) ? GPU_PROBE_CUBE : GPU_PROBE_PLANAR;
		probe->size = ob->probesize;
	}
	else {
		probe->type = GPU_PROBE_CUBE;
		probe->size = wo->probesize;
	}

	/* prevent from having an invalid framebuffer */
	CLAMP(probe->size, 4, 10240);

	if (probe->type == GPU_PROBE_CUBE) {

		/* Cubemap */
		probe->fb = GPU_framebuffer_create();
		if (!probe->fb) {
			gpu_probe_buffers_free(probe);
			return;
		}

		probe->tex = GPU_texture_create_cube_probe(probe->size, NULL);
		if (!probe->tex) {
			gpu_probe_buffers_free(probe);
			return;
		}

		if (!GPU_framebuffer_cubeface_attach(probe->fb, probe->tex, 0, 0)) {
			gpu_probe_buffers_free(probe);
			return;
		}

		/* depth texture */
		probe->depthtex = GPU_texture_create_depth(probe->size, probe->size, NULL);
		if (!probe->depthtex) {
			gpu_probe_buffers_free(probe);
			return;
		}

		if (!GPU_framebuffer_texture_attach(probe->fb, probe->depthtex, 0)) {
			gpu_probe_buffers_free(probe);
			return;
		}

		if (!GPU_framebuffer_check_valid(probe->fb, NULL)) {
			gpu_probe_buffers_free(probe);
			return;
		}


		/* Spherical harmonic texture */
		probe->fbsh = GPU_framebuffer_create();
		if (!probe->fbsh) {
			gpu_probe_buffers_free(probe);
			return;
		}

		probe->shtex = GPU_texture_create_sh_filter_target(NULL);
		if (!probe->shtex) {
			gpu_probe_buffers_free(probe);
			return;
		}

		if (!GPU_framebuffer_texture_attach(probe->fbsh, probe->shtex, 0)) {
			gpu_probe_buffers_free(probe);
			return;
		}

		GPU_texture_bind_as_framebuffer(probe->shtex);

		if (!GPU_framebuffer_check_valid(probe->fbsh, NULL)) {
			gpu_probe_buffers_free(probe);
			return;
		}

		GPU_framebuffer_texture_unbind(probe->fbsh, probe->shtex);
	
	}
	else {

		probe->fb = GPU_framebuffer_create();
		if (!probe->fb) {
			gpu_probe_buffers_free(probe);
			return;
		}

		/* Refraction */
		probe->texrefract = GPU_texture_create_planar_probe(probe->size, NULL);
		if (!probe->texrefract) {
			gpu_probe_buffers_free(probe);
			return;
		}

		if (!GPU_framebuffer_texture_attach(probe->fb, probe->texrefract, 0)) {
			gpu_probe_buffers_free(probe);
			return;
		}

		GPU_framebuffer_texture_detach(probe->texrefract);

		/* Reflection */
		probe->texreflect = GPU_texture_create_planar_probe(probe->size, NULL);
		if (!probe->texreflect) {
			gpu_probe_buffers_free(probe);
			return;
		}

		if (!GPU_framebuffer_texture_attach(probe->fb, probe->texreflect, 0)) {
			gpu_probe_buffers_free(probe);
			return;
		}

		/* depth texture */
		probe->depthtex = GPU_texture_create_depth(probe->size, probe->size, NULL);
		if (!probe->depthtex) {
			gpu_probe_buffers_free(probe);
			return;
		}

		if (!GPU_framebuffer_texture_attach(probe->fb, probe->depthtex, 0)) {
			gpu_probe_buffers_free(probe);
			return;
		}

		if (!GPU_framebuffer_check_valid(probe->fb, NULL)) {
			gpu_probe_buffers_free(probe);
			return;
		}
	}

	/* Setting up spherical harmonics coefs */
	probe->shcoefs = MEM_mallocN(sizeof(float) * 9 * 3, "Probe Spherical Harmonics");
	memset(probe->shcoefs, 0, sizeof(float) * 9 * 3); /* Avoid funky colors in the first reflection pass */

	GPU_framebuffer_restore();

	GPU_probe_set_update(probe, true);
}

GPUProbe *GPU_probe_world(Scene *scene, World *wo)
{
	GPUProbe *probe;
	LinkData *link;

	for (link = wo->gpuprobe.first; link; link = link->next)
		if (((GPUProbe *)link->data)->scene == scene)
			return link->data;

	probe = MEM_callocN(sizeof(GPUProbe), "GPUProbe");

	link = MEM_callocN(sizeof(LinkData), "GPUProbeLink");
	link->data = probe;
	BLI_addtail(&wo->gpuprobe, link);

	gpu_probe_from_blender(scene, wo, NULL, probe);

	return probe;
}

GPUProbe *GPU_probe_object(Scene *scene, Object *ob)
{
	GPUProbe *probe;
	LinkData *link;

	for (link = ob->gpuprobe.first; link; link = link->next) {
		probe = (GPUProbe *)link->data;
		if (probe->scene == scene)
			return link->data;
	}

	probe = MEM_callocN(sizeof(GPUProbe), "GPUProbe");

	link = MEM_callocN(sizeof(LinkData), "GPUProbeLink");
	link->data = probe;
	BLI_addtail(&ob->gpuprobe, link);

	gpu_probe_from_blender(scene, NULL, ob, probe);

	return probe;
}

static void envmap_transmatrix(float mat[4][4], int part)
{
	float tmat[4][4], tmat2[4][4], eul1[3], eul2[3], rotmat1[4][4], rotmat2[4][4];
	
	eul1[0] = eul1[1] = eul1[2] = 0.0;
	eul2[0] = eul2[1] = eul2[2] = 0.0;
	
	if (part == 0){ /* pos x */
		eul1[1] = -M_PI / 2.0;
		eul2[2] = M_PI;
	}
	else if (part == 1) { /* neg x */
		eul1[1] = M_PI / 2.0;
		eul2[2] = M_PI;
	}
	else if (part == 2) { /* pos y */
		eul1[0] = M_PI / 2.0;
	}
	else if (part == 3) { /* neg y */
		eul1[0] = -M_PI / 2.0;
	}
	else if (part == 4) { /* pos z */
		eul1[0] = M_PI;
	}
	else { /* neg z */
		eul1[2] = M_PI;
	}
	
	copy_m4_m4(tmat, mat);
	eul_to_mat4(rotmat1, eul1);
	eul_to_mat4(rotmat2, eul2);
	mul_m4_m4m4(tmat2, rotmat1, rotmat2);
	mul_m4_m4m4(mat, tmat, tmat2);
}

static void gpu_probe_cube_update_buffer_mats(GPUProbe *probe, int cubeface)
{
	float tempmat[4][4];

	perspective_m4(probe->winmat, -probe->clipsta, probe->clipsta, -probe->clipsta, probe->clipsta, probe->clipsta, probe->clipend);
	unit_m4(probe->obmat);

	/* Local Capture */
	if (probe->ob)
		translate_m4(probe->obmat, probe->ob->obmat[3][0], probe->ob->obmat[3][1], probe->ob->obmat[3][2]);

	/* Rotating towards the right face */
	envmap_transmatrix(probe->obmat, cubeface);

	copy_m4_m4(tempmat, probe->obmat);
	invert_m4_m4(probe->viewmat, tempmat);
	normalize_v3(probe->viewmat[0]);
	normalize_v3(probe->viewmat[1]);
	normalize_v3(probe->viewmat[2]);

	mul_m4_m4m4(probe->persmat, probe->winmat, probe->viewmat);
}

void GPU_probe_buffer_bind(GPUProbe *probe)
{
	glDisable(GL_SCISSOR_TEST);
	if (probe->type == GPU_PROBE_PLANAR) {
		GPU_texture_bind_as_framebuffer(probe->texreflect);
	}
	else
		GPU_texture_bind_as_framebuffer(probe->tex);
}

void GPU_probe_switch_fb_cubeface(GPUProbe *probe, int cubeface, float viewmat[4][4], int *winsize, float winmat[4][4])
{
	gpu_probe_cube_update_buffer_mats(probe, cubeface);
	GPU_framebuffer_cubeface_attach(probe->fb, probe->tex, 0, cubeface);

	/* set matrices */
	copy_m4_m4(viewmat, probe->viewmat);
	copy_m4_m4(winmat, probe->winmat);
	*winsize = probe->size;
}

static float point_plane_dist(float point[3], float plane[3], float normal[3])
{
	float tmp[3];
	sub_v3_v3v3(tmp, plane, point);
	return fabsf(dot_v3v3(normal, tmp));
}

void GPU_probe_attach_planar_fb(GPUProbe *probe, float camviewmat[4][4], float camwinmat[4][4], float viewmat[4][4], int *winsize, bool refraction)
{
	float rangemat[4][4];
	float plane[4] = {0.0f, 0.0f, -1.0f, 0.0f};

	if (refraction)
		GPU_framebuffer_texture_attach(probe->fb, probe->texrefract, 0);
	else
		GPU_framebuffer_texture_attach(probe->fb, probe->texreflect, 0);

	/* opengl buffer is range 0.0..1.0 instead of -1.0..1.0 in blender */
	unit_m4(rangemat);
	rangemat[0][0] = 0.5f;
	rangemat[1][1] = 0.5f;
	rangemat[2][2] = 0.5f;
	rangemat[3][0] = 0.5f;
	rangemat[3][1] = 0.5f;
	rangemat[3][2] = 0.5f;

	/* set matrices */
	if (!refraction)
		mul_m4_m4m4(probe->viewmat, camviewmat, probe->reflectmat);
	else
		copy_m4_m4(probe->viewmat, camviewmat);

	mul_m4_m4m4(probe->persmat, camwinmat, probe->viewmat);

	if (!refraction)
		mul_m4_m4m4(probe->reflectionmat, rangemat, probe->persmat);

	copy_m4_m4(viewmat, probe->viewmat);
	*winsize = probe->size;

	/* setup clip plane */
	{
		float camco[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		float planevec[4] = {0.0f, 0.0f, -1.0f, 0.0f};
		float planeco[3];
		double dplane[4];
		float dist_to_plane;
		float planetocam[3];
		float invmat[4][4];

		if (refraction)
			planevec[2] = 1.0f;

		/* finding cam position */
		invert_m4_m4(invmat, camviewmat);
		mul_v4_m4v4(camco, invmat, camco);

		/* mirror plane coordinates and normal vector */
		copy_v3_v3(planeco, probe->obmat[3]);
		mul_v4_m4v4(planevec, probe->obmat, planevec);
		normalize_v3(planevec);

		/* flipping the plane if the cam is below it */
		sub_v3_v3v3(planetocam, camco, planeco);
		if (dot_v3v3(planetocam, planevec) < 0) {
			planevec[0] = planevec[1] = planevec[3] = 0.0f;
			planevec[2] = (refraction) ? -1.0f : 1.0f;
			mul_v4_m4v4(planevec, probe->obmat, planevec);
			normalize_v3(planevec);
		}

		/* saved to use when shading */
		if (!refraction)
			copy_v3_v3(probe->planevec, planevec);

		/* finding the distance to the clip plane */
		dist_to_plane = point_plane_dist(camco, planeco, planevec);

		/* finding the orientation of the clip plane in camera space */
		mul_v4_m4v4(planevec, camviewmat, planevec);

		planevec[3] = dist_to_plane - probe->clipsta;

		negate_v4(planevec);

		copy_v4db_v4fl(dplane, planevec);
		glClipPlane(GL_CLIP_PLANE0, dplane);
		glEnable(GL_CLIP_PLANE0);
	}
}

void GPU_probe_buffer_unbind(GPUProbe *probe)
{
	glDisable(GL_CLIP_PLANE0);
	GPU_framebuffer_texture_unbind(probe->fb, probe->tex);
	GPU_framebuffer_restore();
	glEnable(GL_SCISSOR_TEST);
}

void GPU_probe_rebuild_mipmaps(GPUProbe *probe)
{
	if (probe->type == GPU_PROBE_CUBE) {
		GPU_texture_bind(probe->tex, 0);
		GPU_generate_mipmap(GL_TEXTURE_CUBE_MAP);
		GPU_texture_unbind(probe->tex);
	}
	else if (probe->type == GPU_PROBE_PLANAR) {
		GPU_texture_bind(probe->texreflect, 0);
		GPU_generate_mipmap(GL_TEXTURE_2D);
		GPU_texture_unbind(probe->texreflect);

		GPU_texture_bind(probe->texrefract, 0);
		GPU_generate_mipmap(GL_TEXTURE_2D);
		GPU_texture_unbind(probe->texrefract);
	}
}

void GPU_probe_auto_update(GPUProbe *probe)
{
	if ((probe->ob && probe->ob->probeflags & OB_PROBE_AUTO_UPDATE) ||
		(probe->wo && probe->wo->probeflags & WO_PROBE_AUTO_UPDATE))
		probe->update = true;
}

void GPU_probe_set_update(GPUProbe *probe, bool val)
{
	probe->update = val;
}

bool GPU_probe_get_update(GPUProbe *probe)
{
	return probe->update;
}

Object *GPU_probe_get_object(GPUProbe *probe)
{
	return probe->ob;
}

int GPU_probe_get_size(GPUProbe *probe)
{
	return probe->size;
}

int GPU_probe_get_type(GPUProbe *probe)
{
	return (int)(probe->type);
}

void GPU_probe_update_clip(GPUProbe *probe, float clipsta, float clipend)
{
	probe->clipsta = clipsta;
	probe->clipend = clipend;
}

void GPU_probe_update_layers(GPUProbe *probe, unsigned int lay)
{
	probe->lay = lay;
}

unsigned int GPU_probe_get_layers(GPUProbe *probe)
{
	if (probe->ob && (probe->ob->probeflags & OB_PROBE_USE_LAYERS))
		return probe->lay;
	else
		return -1;
}

void GPU_probe_update_sh_res(GPUProbe *probe, int shres)
{
	CLAMP(shres, 1, (1 << MAX_SH_SAMPLES));
	/* Finding the exponent and taking care of the float imprecision */
	probe->shres = (int)round(log((float)shres) / log(2));
}

void GPU_probe_update_parallax(GPUProbe *probe, float correctionmat[4][4], float obmat[4][4])
{
	copy_v3_v3(probe->co, obmat[3]);
	invert_m4_m4(probe->correctionmat, correctionmat);
}

void GPU_probe_update_ref_plane(GPUProbe *probe, float obmat[4][4])
{
	float mtx[4][4];
	float obmat_scale[3];

	/* mtx is the mirror transformation */
	unit_m4(mtx);
	mtx[2][2] = -1.0f; /* XY reflection plane */

	normalize_m4_m4_ex(probe->obmat, obmat, obmat_scale);
	invert_m4_m4(probe->imat, probe->obmat);

	mul_m4_m4m4(mtx, mtx, probe->imat);
	mul_m4_m4m4(probe->reflectmat, probe->obmat, mtx);
}


/* Spherical Harmonics : Diffuse lighting */

static void gpu_compute_sh(GPUProbe *probe)
{
	int probe_source_uniform;
	GPUShader *sh_shader = GPU_shader_get_builtin_shader(GPU_SHADER_COMPUTE_SH + probe->shres);

	if (!sh_shader)
		return;

	GPU_shader_bind(sh_shader);

	probe_source_uniform = GPU_shader_get_uniform(sh_shader, "probe");

	GPU_texture_bind(probe->tex, 0);
	GPU_shader_uniform_texture(sh_shader, probe_source_uniform, probe->tex);

	/* Drawing quad */
	glBegin(GL_TRIANGLE_STRIP);
	glVertex3f(-1.0, -1.0, 1.0);
	glVertex3f(1.0, -1.0, 1.0);
	glVertex3f(-1.0, 1.0, 1.0);
	glVertex3f(1.0, 1.0, 1.0);
	glEnd();

	GPU_texture_unbind(probe->tex);

	GPU_shader_unbind();
}

void GPU_probe_sh_compute(GPUProbe *probe)
{
	bool computesh = false;

	if (probe->ob && (probe->ob->probeflags & OB_PROBE_COMPUTE_SH))
		computesh = true;

	if (probe->wo && (probe->wo->probeflags & WO_PROBE_COMPUTE_SH))
		computesh = true;

	if (computesh && probe->type == GPU_PROBE_CUBE) {
		glDisable(GL_SCISSOR_TEST);
		GPU_texture_bind_as_framebuffer(probe->shtex);
		gpu_compute_sh(probe);

		GPU_framebuffer_texture_unbind(probe->fbsh, probe->shtex);

		/* Only read the 9 pixels containing the coefs */
		glReadPixels(0, 0, 3, 3, GL_RGB, GL_FLOAT, probe->shcoefs);
		GPU_framebuffer_restore();
		glEnable(GL_SCISSOR_TEST);
	}
	else if (computesh && (probe->type == GPU_PROBE_PLANAR)) {
		GPUProbe *srcprobe;
		/* Copy sh from other probe */
		if (probe->ob->probe)
			srcprobe = GPU_probe_object(probe->scene, probe->ob->probe);
		else if (probe->scene->world)
			srcprobe = GPU_probe_world(probe->scene, probe->scene->world);
		else {
			memset(probe->shcoefs, 0, sizeof(float) * 9 * 3);
			return;
		}

		memcpy(probe->shcoefs, srcprobe->shcoefs, sizeof(float) * 9 * 3);
	}
	else {
		memset(probe->shcoefs, 0, sizeof(float) * 9 * 3);
	}
}

void GPU_probe_sh_shader_bind(GPUProbe *probe)
{
	int uniformloc[9], i;
	GPUShader *sh_shader = GPU_shader_get_builtin_shader(GPU_SHADER_DISPLAY_SH);

	if (!sh_shader)
		return;

	GPU_shader_bind(sh_shader);

	uniformloc[0] = GPU_shader_get_uniform(sh_shader, "sh0");
	uniformloc[1] = GPU_shader_get_uniform(sh_shader, "sh1");
	uniformloc[2] = GPU_shader_get_uniform(sh_shader, "sh2");
	uniformloc[3] = GPU_shader_get_uniform(sh_shader, "sh3");
	uniformloc[4] = GPU_shader_get_uniform(sh_shader, "sh4");
	uniformloc[5] = GPU_shader_get_uniform(sh_shader, "sh5");
	uniformloc[6] = GPU_shader_get_uniform(sh_shader, "sh6");
	uniformloc[7] = GPU_shader_get_uniform(sh_shader, "sh7");
	uniformloc[8] = GPU_shader_get_uniform(sh_shader, "sh8");

	for (i = 0; i < 9; ++i) {
		GPU_shader_uniform_vector(sh_shader, uniformloc[i], 3, 1, &probe->shcoefs[i*3]);
	}
}

void GPU_probe_sh_shader_unbind(void)
{
	GPU_shader_unbind();
}