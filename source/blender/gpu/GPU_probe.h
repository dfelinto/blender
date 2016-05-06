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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Clement Foucault.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_probe.h
 *  \ingroup gpu
 */

#ifndef __GPU_PROBE_H__
#define __GPU_PROBE_H__

#include "BLI_sys_types.h" /* for bool */
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct World;
struct Scene;

typedef struct GPUProbe {
	struct Scene *scene;
	struct World *wo;
	struct Object *ob;

	int type;
	int size, shres;
	unsigned int lay;

	bool update;

	float clipsta;
	float clipend;

	float obmat[4][4];
	float imat[4][4];
	float winmat[4][4];
	float viewmat[4][4];
	float persmat[4][4];

	float co[3], planevec[3];
	float correctionmat[4][4];
	float reflectionmat[4][4];
	float reflectmat[4][4];

	struct GPUTexture *tex;
	struct GPUTexture *texreflect;
	struct GPUTexture *texrefract;
	struct GPUTexture *depthtex;
	struct GPUFrameBuffer *fb;

	struct GPUTexture *shtex;
	struct GPUFrameBuffer *fbsh;

	float *shcoefs;
} GPUProbe;

/* Keep this enum in sync with DNA_object_types.h */
typedef enum GPUProbeType {
	GPU_PROBE_CUBE		= 2,
	GPU_PROBE_PLANAR	= 3,
} GPUProbeType;

GPUProbe *GPU_probe_world(struct Scene *scene, struct World *wo);
GPUProbe *GPU_probe_object(struct Scene *scene, struct Object *ob);
void GPU_probe_free(ListBase *gpuprobe);

void GPU_probe_buffer_bind(GPUProbe *probe);
void GPU_probe_switch_fb_cubeface(GPUProbe *probe, int cubeface, float viewmat[4][4], int *winsize, float winmat[4][4]);
void GPU_probe_attach_planar_fb(GPUProbe *probe, float camviewmat[4][4], float camwinmat[4][4], float viewmat[4][4], int *winsize, bool refraction);
void GPU_probe_buffer_unbind(GPUProbe *probe);
void GPU_probe_rebuild_mipmaps(GPUProbe *probe);
void GPU_probe_sh_compute(GPUProbe *probe);
void GPU_probe_sh_shader_bind(GPUProbe *probe);
void GPU_probe_sh_shader_unbind(void);
void GPU_probe_auto_update(GPUProbe *probe);
void GPU_probe_set_update(GPUProbe *probe, bool val);
bool GPU_probe_get_update(GPUProbe *probe);
struct Object *GPU_probe_get_object(GPUProbe *probe);
int GPU_probe_get_size(GPUProbe *probe);
int GPU_probe_get_type(GPUProbe *probe);
void GPU_probe_update_clip(GPUProbe *probe, float clipsta, float clipend);
void GPU_probe_update_layers(GPUProbe *probe, unsigned int lay);
unsigned int GPU_probe_get_layers(GPUProbe *probe);
void GPU_probe_update_sh_res(GPUProbe *probe, int res);
void GPU_probe_update_parallax(GPUProbe *probe, float correctionmat[4][4], float obmat[4][4]);
void GPU_probe_update_ref_plane(GPUProbe *probe, float obmat[4][4]);

#ifdef __cplusplus
}
#endif

#endif /*__GPU_PROBE_H__*/
