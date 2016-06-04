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

/** \file GPU_pbr.h
 *  \ingroup gpu
 */

#ifndef __GPU_LUTS_H__
#define __GPU_LUTS_H__

#ifdef __cplusplus
extern "C" {
#endif

struct GPUTexture;
struct GPUFrameBuffer;

typedef struct GPUScreenBuffer {
	int type;
	int w, h;
	float clipsta, clipend;
	float pixelprojmat[4][4];
	struct GPUTexture *tex;
	struct GPUTexture *depth;
	struct GPUFrameBuffer *fb;
	struct GPUFrameBuffer *downsamplingfb;
} GPUScreenBuffer;

typedef struct GPUPBR {
	struct GPUTexture *hammersley;
	struct GPUTexture *jitter;
	struct GPUTexture *ltc_mat_ggx;
	struct GPUTexture *ltc_mag_ggx;

	GPUScreenBuffer *scene;
	GPUScreenBuffer *backface;
} GPUPBR;

typedef enum GPUScreenBufferType {
	GPU_COLOR_BUFFER	= 0,
	GPU_BACKFACE_BUFFER	= 1,
} GPUScreenBufferType;

GPUPBR *GPU_pbr_create(void);
void GPU_pbr_update(GPUPBR *pbr, GPUPBRSettings *pbr_settings, struct Scene *scene, struct View3D *v3d, struct ARegion *ar);
void GPU_pbr_free(GPUPBR *pbr);

void GPU_scenebuf_free(GPUScreenBuffer *buf);
GPUScreenBuffer *GPU_pbr_scene_buffer(GPUPBR *pbr, int width, int height);
GPUScreenBuffer *GPU_pbr_backface_buffer(GPUPBR *pbr, int width, int height);
void GPU_scenebuf_bind(GPUScreenBuffer* buf, float winmat[4][4], int winsize[2], float clipsta, float clipend);
void GPU_scenebuf_unbind(GPUScreenBuffer* buf);
void GPU_scenebuf_filter_texture(GPUScreenBuffer* buf);

void GPU_pbr_settings_validate(struct GPUPBRSettings *pbr_settings);

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_LUTS_H__ */
