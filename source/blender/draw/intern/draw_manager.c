/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/viewport_engine.c
 *  \ingroup draw
 */

#include <stdio.h>

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_global.h"

#include "DRW_engine.h"
#include "DRW_render.h"

/* Clement : I need to get rid of this */
#include "DNA_screen_types.h" /* hacky */
#include "DNA_view3d_types.h" /* hacky */
#include "DNA_object_types.h" /* hacky */
#include "view3d_intern.h" /* hacky */

#include "GPU_basic_shader.h"
#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_select.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_viewport.h"

#include "RE_engine.h"

#include "UI_resources.h"

#include "clay.h"

extern char datatoc_gpu_shader_2D_vert_glsl[];
extern char datatoc_gpu_shader_3D_vert_glsl[];
extern char datatoc_gpu_shader_basic_vert_glsl[];

/* Structures */
typedef enum {
	DRW_UNIFORM_INT,
	DRW_UNIFORM_FLOAT,
	DRW_UNIFORM_TEXTURE,
	DRW_UNIFORM_BUFFER,
	DRW_UNIFORM_MAT3,
	DRW_UNIFORM_MAT4
} DRWUniformType;

struct DRWUniform {
	struct DRWUniform *next, *prev;
	DRWUniformType type;
	int location;
	int length;
	int arraysize;
	int bindloc;
	const void *value;
};

struct DRWInterface {
	ListBase uniforms;
	/* matrices locations */
	int modelview;
	int projection;
	int modelviewprojection;
	int normal;
};

struct DRWPass {
	ListBase batches;
	DRWState state;
};

struct DRWBatch {
	struct DRWBatch *next, *prev;
	struct GPUShader *shader;        /* Shader to bind */
	struct DRWInterface *interface;  /* Uniforms values */
	ListBase objects;               /* (Object *) LinkData->data - List with all objects and transform */
};

/* Render State */
static struct DRWGlobalState{
	GPUShader *shader;
	struct GPUFrameBuffer *default_framebuffer;
	FramebufferList *current_fbl;
	TextureList *current_txl;
	ListBase bound_texs;
	int tex_bind_id;
	int size[2];
	/* Current rendering context set by DRW_viewport_init */
	bContext *context;
} DST = {NULL};

/* Fullscreen Quad Buffer */
static const float fs_cos[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}};
static const float fs_uvs[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};
static unsigned int fs_quad;
static bool fs_quad_init = false;

/* ***************************************** TEXTURES ******************************************/
static void drw_texture_get_format(DRWTextureFormat format, GPUFormat *data_type, int *channels)
{
	switch (format) {
		case DRW_TEX_RGBA_8: *data_type = GPU_RGBA8; break;
		case DRW_TEX_RGBA_16: *data_type = GPU_RGBA16F; break;
		case DRW_TEX_RGBA_32: *data_type = GPU_RGBA32F; break;
		case DRW_TEX_RGB_8: *data_type = GPU_RGB8; break;
		case DRW_TEX_RGB_16: *data_type = GPU_RGB16F; break;
		case DRW_TEX_RGB_32: *data_type = GPU_RGB32F; break;
		case DRW_TEX_RG_8: *data_type = GPU_RG8; break;
		case DRW_TEX_RG_16: *data_type = GPU_RG16F; break;
		case DRW_TEX_RG_32: *data_type = GPU_RG32F; break;
		case DRW_TEX_R_8: *data_type = GPU_R8; break;
		case DRW_TEX_R_16: *data_type = GPU_R16F; break;
		case DRW_TEX_R_32: *data_type = GPU_R32F; break;
		case DRW_TEX_DEPTH_16: *data_type = GPU_DEPTH_COMPONENT16; break;
		case DRW_TEX_DEPTH_24: *data_type = GPU_DEPTH_COMPONENT24; break;
		case DRW_TEX_DEPTH_32: *data_type = GPU_DEPTH_COMPONENT32F; break;
	}

	switch (format) {
		case DRW_TEX_RGBA_8:
		case DRW_TEX_RGBA_16:
		case DRW_TEX_RGBA_32:
			*channels = 4;
			break;
		case DRW_TEX_RGB_8:
		case DRW_TEX_RGB_16:
		case DRW_TEX_RGB_32:
			*channels = 3;
			break;
		case DRW_TEX_RG_8:
		case DRW_TEX_RG_16:
		case DRW_TEX_RG_32:
			*channels = 2;
			break;
		default:
			*channels = 1;
			break;
	}
}

static void drw_texture_set_parameters(GPUTexture *tex, DRWTextureFlag flags)
{
	GPU_texture_bind(tex, 0);
	GPU_texture_filter_mode(tex, flags & DRW_TEX_FILTER);
	GPU_texture_wrap_mode(tex, flags & DRW_TEX_WRAP);
	GPU_texture_compare_mode(tex, flags & DRW_TEX_COMPARE);
	GPU_texture_unbind(tex);
}

GPUTexture *DRW_texture_create_1D(int w, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUFormat data_type;
	int channels;

	drw_texture_get_format(format, &data_type, &channels);
	tex = GPU_texture_create_1D_custom(w, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_2D(int w, int h, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUFormat data_type;
	int channels;

	drw_texture_get_format(format, &data_type, &channels);
	tex = GPU_texture_create_2D_custom(w, h, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

/* TODO make use of format */
GPUTexture *DRW_texture_create_2D_array(int w, int h, int d, DRWTextureFormat UNUSED(format), DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;

	tex = GPU_texture_create_2D_array(w, h, d, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

void DRW_texture_free(GPUTexture *tex)
{
	GPU_texture_free(tex);
}

/* ***************************************** BUFFERS ******************************************/

static void draw_fullscreen(void)
{
	if (!fs_quad_init) {
		glGenBuffers(1, &fs_quad);
		glBindBuffer(GL_ARRAY_BUFFER, fs_quad);
		glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, 8 * sizeof(float), fs_cos);
		glBufferSubData(GL_ARRAY_BUFFER, 8 * sizeof(float), 8 * sizeof(float), fs_uvs);
	}

	/* set up quad buffer */
	glBindBuffer(GL_ARRAY_BUFFER, fs_quad);
	glVertexPointer(2, GL_FLOAT, 0, NULL);
	glTexCoordPointer(2, GL_FLOAT, 0, ((GLubyte *)NULL + 8 * sizeof(float)));
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	/* Draw */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	/* Restore */
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ****************************************** SHADERS ******************************************/

GPUShader *DRW_shader_create(const char *vert, const char *geom, const char *frag, const char *defines)
{
	return GPU_shader_create(vert, frag, geom, NULL, defines, 0, 0, 0);
}

GPUShader *DRW_shader_create_2D(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_2D_vert_glsl, frag, NULL, NULL, defines, 0, 0, 0);
}

GPUShader *DRW_shader_create_3D(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_3D_vert_glsl, frag, NULL, NULL, defines, 0, 0, 0);
}

GPUShader *DRW_shader_create_3D_depth_only(void)
{
	return GPU_shader_get_builtin_shader(GPU_SHADER_3D_DEPTH_ONLY);
}

void DRW_shader_free(GPUShader *shader)
{
	GPU_shader_free(shader);
}

/* ***************************************** INTERFACE ******************************************/

static DRWInterface *DRW_interface_create(struct GPUShader *shader)
{
	DRWInterface *interface = MEM_callocN(sizeof(DRWInterface), "DRWInterface");

	int program = GPU_shader_get_program(shader);

	interface->modelview = glGetUniformLocation(program, "ModelViewMatrix");
	interface->projection = glGetUniformLocation(program, "ProjectionMatrix");
	interface->modelviewprojection = glGetUniformLocation(program, "ModelViewProjectionMatrix");
	interface->normal = glGetUniformLocation(program, "NormalMatrix");

	return interface;
}

static void DRW_interface_uniform(DRWBatch *batch, const char *name,
                                  DRWUniformType type, const void *value, int length, int arraysize, int bindloc)
{
	DRWUniform *uni = MEM_callocN(sizeof(DRWUniform), "DRWUniform");

	int loc = GPU_shader_get_uniform(batch->shader, name);

	if (loc == -1) {
		if (G.debug & G_DEBUG)
			fprintf(stderr, "Uniform '%s' not found!\n", name);

		MEM_freeN(uni);
		return;
	}

	uni->type = type;
	uni->location = loc;
	uni->value = value;
	uni->length = length;
	uni->arraysize = arraysize;
	uni->bindloc = bindloc; /* for textures */

	BLI_addtail(&batch->interface->uniforms, uni);
}

void DRW_get_dfdy_factors(float dfdyfac[2])
{
	GPU_get_dfdy_factors(dfdyfac);
}

/* ***************************************** BATCH ******************************************/

DRWBatch *DRW_batch_create(struct GPUShader *shader, DRWPass *pass)
{
	DRWBatch *batch = MEM_callocN(sizeof(DRWBatch), "DRWBatch");

	batch->shader = shader;
	batch->interface = DRW_interface_create(shader);

	BLI_addtail(&pass->batches, batch);

	return batch;
}

void DRW_batch_free(struct DRWBatch *batch)
{
	for (LinkData *link = batch->objects.first; link; link = link->next) {
		MEM_freeN(link->data);
	}
	BLI_freelistN(&batch->objects);

	BLI_freelistN(&batch->interface->uniforms);
	MEM_freeN(batch->interface);
}

/* Later use VBO */
void DRW_batch_surface_add(DRWBatch *batch, Object *ob)
{
	BLI_addtail(&batch->objects, BLI_genericNodeN(ob));
}

void DRW_batch_surface_clear(DRWBatch *batch)
{
	for (LinkData *link = batch->objects.first; link; link = link->next) {
		MEM_freeN(link->data);
	}
	BLI_freelistN(&batch->objects);
	//BLI_listbase_clear(&batch->objects);
}

void DRW_batch_uniform_texture(DRWBatch *batch, const char *name, const GPUTexture *tex, int loc)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_TEXTURE, tex, 0, 0, loc);
}

void DRW_batch_uniform_buffer(DRWBatch *batch, const char *name, const int value, int loc)
{
	/* we abuse the lenght attrib to store the buffer index */
	DRW_interface_uniform(batch, name, DRW_UNIFORM_BUFFER, NULL, value, 0, loc);
}

void DRW_batch_uniform_float(DRWBatch *batch, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_FLOAT, value, 1, arraysize, 0);
}

void DRW_batch_uniform_vec2(DRWBatch *batch, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_FLOAT, value, 2, arraysize, 0);
}

void DRW_batch_uniform_vec3(DRWBatch *batch, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_FLOAT, value, 3, arraysize, 0);
}

void DRW_batch_uniform_vec4(DRWBatch *batch, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_FLOAT, value, 4, arraysize, 0);
}

void DRW_batch_uniform_int(DRWBatch *batch, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_INT, value, 1, arraysize, 0);
}

void DRW_batch_uniform_ivec2(DRWBatch *batch, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_INT, value, 2, arraysize, 0);
}

void DRW_batch_uniform_ivec3(DRWBatch *batch, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_INT, value, 3, arraysize, 0);
}

void DRW_batch_uniform_mat3(DRWBatch *batch, const char *name, const float *value)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_MAT3, value, 9, 1, 0);
}

void DRW_batch_uniform_mat4(DRWBatch *batch, const char *name, const float *value)
{
	DRW_interface_uniform(batch, name, DRW_UNIFORM_MAT4, value, 16, 1, 0);
}

/* ***************************************** PASSES ******************************************/

DRWPass *DRW_pass_create(const char *name, DRWState state)
{
	DRWPass *pass = (DRWPass *)MEM_callocN(sizeof(DRWPass), name);
	pass->state = state;

	return pass;
}

void DRW_pass_free(DRWPass *pass)
{
	for (DRWBatch *batch = pass->batches.first; batch; batch = batch->next) {
		DRW_batch_free(batch);
	}
	BLI_freelistN(&pass->batches);
}

/* ****************************************** DRAW ******************************************/

void DRW_draw_background(void)
{
	if (UI_GetThemeValue(TH_SHOW_BACK_GRAD)) {
		/* Gradient background Color */
		gpuMatrixBegin3D(); /* TODO: finish 2D API */

		glClear(GL_DEPTH_BUFFER_BIT);

		VertexFormat *format = immVertexFormat();
		unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
		unsigned color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);
		unsigned char col_hi[3], col_lo[3];

		immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

		UI_GetThemeColor3ubv(TH_LOW_GRAD, col_lo);
		UI_GetThemeColor3ubv(TH_HIGH_GRAD, col_hi);

		immBegin(GL_QUADS, 4);
		immAttrib3ubv(color, col_lo);
		immVertex2f(pos, -1.0f, -1.0f);
		immVertex2f(pos, 1.0f, -1.0f);

		immAttrib3ubv(color, col_hi);
		immVertex2f(pos, 1.0f, 1.0f);
		immVertex2f(pos, -1.0f, 1.0f);
		immEnd();

		immUnbindProgram();

		gpuMatrixEnd();
	}
	else {
		/* Solid background Color */
		UI_ThemeClearColorAlpha(TH_HIGH_GRAD, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

typedef struct DRWBoundTexture {
	struct DRWBoundTexture *next, *prev;
	GPUTexture *tex;
} DRWBoundTexture;

static void draw_batch(DRWBatch *batch, const bool fullscreen)
{
	BLI_assert(batch->shader);
	BLI_assert(batch->interface);

	DRWInterface *interface = batch->interface;

	if (DST.shader != batch->shader) {
		if (DST.shader) GPU_shader_unbind();
		GPU_shader_bind(batch->shader);
		DST.shader = batch->shader;
	}

	/* Don't check anything, Interface should already contain the least uniform as possible */
	for (DRWUniform *uni = interface->uniforms.first; uni; uni = uni->next) {
		DRWBoundTexture *bound_tex;

		switch (uni->type) {
			case DRW_UNIFORM_INT:
				GPU_shader_uniform_vector_int(batch->shader, uni->location, uni->length, uni->arraysize, (int *)uni->value);
				break;
			case DRW_UNIFORM_FLOAT:
			case DRW_UNIFORM_MAT3:
			case DRW_UNIFORM_MAT4:
				GPU_shader_uniform_vector(batch->shader, uni->location, uni->length, uni->arraysize, (float *)uni->value);
				break;
			case DRW_UNIFORM_TEXTURE:
				GPU_texture_bind((GPUTexture *)uni->value, uni->bindloc);

				bound_tex = MEM_callocN(sizeof(DRWBoundTexture), "DRWBoundTexture");
				bound_tex->tex = (GPUTexture *)uni->value;
				BLI_addtail(&DST.bound_texs, bound_tex);

				GPU_shader_uniform_texture(batch->shader, uni->location, (GPUTexture *)uni->value);
				break;
			case DRW_UNIFORM_BUFFER:
				/* restore index from lenght we abused */
				GPU_texture_bind(DST.current_txl->textures[uni->length], uni->bindloc);
				GPU_texture_compare_mode(DST.current_txl->textures[uni->length], false);
				GPU_texture_filter_mode(DST.current_txl->textures[uni->length], false);
				
				bound_tex = MEM_callocN(sizeof(DRWBoundTexture), "DRWBoundTexture");
				bound_tex->tex = DST.current_txl->textures[uni->length];
				BLI_addtail(&DST.bound_texs, bound_tex);

				GPU_shader_uniform_texture(batch->shader, uni->location, DST.current_txl->textures[uni->length]);
				break;
		}
	}

	if (fullscreen) {
		/* step 1 : bind matrices */
		if (interface->modelviewprojection != -1) {
			float mvp[4][4];
			unit_m4(mvp);
			GPU_shader_uniform_vector(batch->shader, interface->modelviewprojection, 16, 1, (float *)mvp);
		}

		/* step 2 : bind vertex array & draw */
		draw_fullscreen();
	}
	else {
		RegionView3D *rv3d = CTX_wm_region_view3d(DST.context);

		for (LinkData *link = batch->objects.first; link; link = link->next) {
			Object *ob = link->data;

			/* Should be really simple */
			/* step 1 : bind object dependent matrices */
			if (interface->modelviewprojection != -1) {
				float mvp[4][4];
				mul_m4_m4m4(mvp, rv3d->persmat, ob->obmat);
				GPU_shader_uniform_vector(batch->shader, interface->modelviewprojection, 16, 1, (float *)mvp);
			}
			if (interface->modelview != -1) {
				float mv[4][4];
				mul_m4_m4m4(mv, rv3d->viewmat, ob->obmat);
				GPU_shader_uniform_vector(batch->shader, interface->modelview, 16, 1, (float *)mv);
			}
			if (interface->normal != -1) {
				float mv[4][4];
				float n[3][3];
				mul_m4_m4m4(mv, rv3d->viewmat, ob->obmat);
				copy_m3_m4(n, mv);
				invert_m3(n);
				transpose_m3(n);
				GPU_shader_uniform_vector(batch->shader, interface->normal, 9, 1, (float *)n);
			}

			/* step 2 : bind vertex array & draw */
			/* we won't use any function that doesn't comply to the new API, this is a short-lived exception TODO */
			draw_mesh(ob, DST.context, GPU_shader_get_program(batch->shader));
		}
	}
}

static void set_state(short flag)
{
	/* Depth Write */
	if (flag & DRW_STATE_WRITE_DEPTH)
		glDepthMask(GL_TRUE);
	else
		glDepthMask(GL_FALSE);

	/* Color Write */
	if (flag & DRW_STATE_WRITE_COLOR)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	else
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	/* Backface Culling */
	if (flag & DRW_STATE_CULL_BACK ||
	    flag & DRW_STATE_CULL_FRONT) {

		glEnable(GL_CULL_FACE);

		if (flag & DRW_STATE_CULL_BACK)
			glCullFace(GL_BACK);
		else if (flag & DRW_STATE_CULL_FRONT)
			glCullFace(GL_FRONT);
	}
	else {
		glDisable(GL_CULL_FACE);
	}

	/* Depht Test */
	if (flag & DRW_STATE_DEPTH_LESS ||
	    flag & DRW_STATE_DEPTH_EQUAL) {

		glEnable(GL_DEPTH_TEST);

		if (flag & DRW_STATE_DEPTH_LESS)
			glDepthFunc(GL_LEQUAL);
		else if (flag & DRW_STATE_DEPTH_EQUAL)
			glDepthFunc(GL_EQUAL);
	}
	else {
		glDisable(GL_DEPTH_TEST);
	}
}

void DRW_draw_pass(DRWPass *pass)
{
	/* Start fresh */
	DST.shader = NULL;
	DST.tex_bind_id = 0;
	
	set_state(pass->state);
	BLI_listbase_clear(&DST.bound_texs);

	for (DRWBatch *batch = pass->batches.first; batch; batch = batch->next) {
		draw_batch(batch, false);
	}

	/* Clear Bound textures */
	for (DRWBoundTexture *bound_tex = DST.bound_texs.first; bound_tex; bound_tex = bound_tex->next) {
		GPU_texture_unbind(bound_tex->tex);
	}
	DST.tex_bind_id = 0;
	BLI_freelistN(&DST.bound_texs);

	if (DST.shader) {
		GPU_shader_unbind();
		DST.shader = NULL;
	}
}

void DRW_draw_pass_fullscreen(DRWPass *pass)
{
	/* Start fresh */
	DST.shader = NULL;
	DST.tex_bind_id = 0;

	set_state(pass->state);
	BLI_listbase_clear(&DST.bound_texs);

	DRWBatch *batch = pass->batches.first;
	draw_batch(batch, true);

	/* Clear Bound textures */
	for (DRWBoundTexture *bound_tex = DST.bound_texs.first; bound_tex; bound_tex = bound_tex->next) {
		GPU_texture_unbind(bound_tex->tex);
	}
	DST.tex_bind_id = 0;
	BLI_freelistN(&DST.bound_texs);

	if (DST.shader) {
		GPU_shader_unbind();
		DST.shader = NULL;
	}
}

/* Reset state to not interfer with other UI drawcall */
void DRW_state_reset(void)
{
	DRWState state = 0;
	state |= DRW_STATE_WRITE_DEPTH;
	state |= DRW_STATE_WRITE_COLOR;
	state |= DRW_STATE_DEPTH_LESS;
	set_state(state);
}

/* ****************************************** Materials ******************************************/

void *DRW_material_settings(Material *ma)
{
	Scene *scene = CTX_data_scene(DST.context);

	if (STREQ(scene->r.engine, RE_engine_id_BLENDER_CLAY))
		return &ma->clay;

	BLI_assert(false);
	return NULL;
}

void *DRW_render_settings(void)
{
	Scene *scene = CTX_data_scene(DST.context);

	if (STREQ(scene->r.engine, RE_engine_id_BLENDER_CLAY))
		return &scene->claydata;

	BLI_assert(false);
	return NULL;
}

/* ****************************************** Framebuffers ******************************************/

void DRW_framebuffer_init(struct GPUFrameBuffer **fb, int width, int height, DRWFboTexture textures[MAX_FBO_TEX],
                          int texnbr)
{
	if (!*fb) {
		int color_attachment = -1;
		*fb = GPU_framebuffer_create();

		for (int i = 0; i < texnbr; ++i)
		{
			DRWFboTexture fbotex = textures[i];
			
			if (!*fbotex.tex) {
				/* TODO refine to opengl formats */
				if (fbotex.format == DRW_BUF_DEPTH_16 ||
					fbotex.format == DRW_BUF_DEPTH_24) {
					*fbotex.tex = GPU_texture_create_depth(width, height, NULL);
					GPU_texture_compare_mode(*fbotex.tex, false);
					GPU_texture_filter_mode(*fbotex.tex, false);
				}
				else {
					*fbotex.tex = GPU_texture_create_2D(width, height, NULL, NULL);
					++color_attachment;
				}
			}
			
			GPU_framebuffer_texture_attach(*fb, *fbotex.tex, color_attachment);
		}

		if (!GPU_framebuffer_check_valid(*fb, NULL)) {
			printf("Error invalid framebuffer\n");
		}

		GPU_framebuffer_bind(DST.default_framebuffer);
	}
}

void DRW_framebuffer_bind(struct GPUFrameBuffer *fb)
{
	GPU_framebuffer_bind(fb);
}

void DRW_framebuffer_texture_attach(struct GPUFrameBuffer *fb, GPUTexture *tex, int slot)
{
	GPU_framebuffer_texture_attach(fb, tex, slot);
}

void DRW_framebuffer_texture_detach(GPUTexture *tex)
{
	GPU_framebuffer_texture_detach(tex);
}

/* ****************************************** Viewport ******************************************/

int *DRW_viewport_size_get(void)
{
	return &DST.size[0];
}

void DRW_viewport_init(const bContext *C, void **buffers, void **textures, void **passes)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	GPUViewport *viewport = rv3d->viewport;

	/* Save context for all later needs */
	DST.context = C;
	GPU_viewport_get_engine_data(viewport, buffers, textures, passes);

	/* Refresh DST.size */
	DefaultTextureList *txl = (DefaultTextureList *)*textures;
	DST.size[0] = GPU_texture_width(txl->color);
	DST.size[1] = GPU_texture_height(txl->color);
	DST.current_txl = (TextureList *)*textures;

	DefaultFramebufferList *fbl = (DefaultFramebufferList *)*buffers;
	DST.default_framebuffer = fbl->default_fb;
	DST.current_fbl = (FramebufferList *)*buffers;
}

void DRW_viewport_matrix_get(float mat[4][4], DRWViewportMatrixType type)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(DST.context);

	if (type == DRW_MAT_PERS)
		copy_m4_m4(mat, rv3d->persmat);
	else if (type == DRW_MAT_WIEW)
		copy_m4_m4(mat, rv3d->viewmat);
	else if (type == DRW_MAT_WIN)
		copy_m4_m4(mat, rv3d->winmat);
}

bool DRW_viewport_is_persp(void)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(DST.context);
	return rv3d->is_persp;
}

/* ****************************************** INIT ******************************************/

void DRW_engines_init(void)
{
	BLI_addtail(&R_engines, &viewport_clay_type);
}

void DRW_engines_free(void)
{
	clay_engine_free();

	BLI_remlink(&R_engines, &viewport_clay_type);
}