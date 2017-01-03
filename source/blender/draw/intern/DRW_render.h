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

/** \file DRW_render.h
 *  \ingroup draw
 */

/* This is the Render Functions used by Realtime engines to draw with OpenGL */

#ifndef __DRW_RENDER_H__
#define __DRW_RENDER_H__

#include "BKE_context.h"
#include "BKE_scene.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "RE_engine.h"

typedef struct DRWBatch {
	struct DRWBatch *next, *prev;
	struct GPUShader *shader;        // Shader to bind
	struct DRWInterface *interface;  // Uniforms values
	ListBase objects;               // List with all objects and transform
} DRWBatch;

typedef enum {
	DRW_STATE_WRITE_DEPTH = (1 << 0),
	DRW_STATE_WRITE_COLOR = (1 << 1),
	DRW_STATE_DEPTH_LESS  = (1 << 2),
	DRW_STATE_DEPTH_EQUAL = (1 << 3),
	DRW_STATE_CULL_BACK   = (1 << 4),
	DRW_STATE_CULL_FRONT  = (1 << 5)
	/* TODO GL_BLEND */
} DRWState;

typedef struct DRWPass {
	ListBase batches;
	DRWState state;
} DRWPass;

struct GPUFrameBuffer;

/* Shaders */
struct GPUShader *DRW_shader_create(const char *vert, const char *geom, const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_2D(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_3D(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_3D_depth_only(void);

struct DRWInterface *DRW_interface_create(struct GPUShader *shader);
void DRW_interface_uniform_texture(struct GPUShader *shader, struct DRWInterface *uniforms, const char *name, const struct GPUTexture *tex, int loc);
void DRW_interface_uniform_buffer(struct GPUShader *shader, struct DRWInterface *interface, const char *name, const int value, int loc);
void DRW_interface_uniform_float(struct GPUShader *shader, struct DRWInterface *uniforms, const char *name, const float *value, int length);
void DRW_interface_uniform_int(struct GPUShader *shader, struct DRWInterface *uniforms, const char *name, const int *value, int length);
void DRW_interface_uniform_mat3(struct GPUShader *shader, struct DRWInterface *interface, const char *name, const float *value);
void DRW_interface_uniform_mat4(struct GPUShader *shader, struct DRWInterface *interface, const char *name, const float *value);

/* DRWFboTexture->format */
#define DRW_BUF_DEPTH_16		1
#define DRW_BUF_DEPTH_24		2
#define DRW_BUF_R_8				3
#define DRW_BUF_R_16			4
#define DRW_BUF_R_32			5
#define DRW_BUF_RG_8			6
#define DRW_BUF_RG_16			7
#define DRW_BUF_RG_32			8
#define DRW_BUF_RGB_8			9
#define DRW_BUF_RGB_16			10
#define DRW_BUF_RGB_32			11
#define DRW_BUF_RGBA_8			12
#define DRW_BUF_RGBA_16			13
#define DRW_BUF_RGBA_32			14

#define MAX_FBO_TEX			5

typedef struct DRWFboTexture {
	struct GPUTexture **tex;
	int format;
} DRWFboTexture;

/* Buffers */
void DRW_framebuffer_init(struct GPUFrameBuffer **fb, int width, int height, DRWFboTexture textures[MAX_FBO_TEX], int texnbr);
void DRW_framebuffer_bind(struct GPUFrameBuffer *fb);

void DRW_init_viewport(const bContext *C, void **buffers, void **textures);

int *DRW_get_viewport_size(void);

/* Draw commands */
void DRW_draw_background(void);
void DRW_draw_pass(DRWPass *pass, const struct bContext *context);
void DRW_draw_pass_fullscreen(DRWPass *pass);

void DRW_reset_state(void);

#endif /* __DRW_RENDER_H__ */