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

/** \file GPU_ssr.h
 *  \ingroup gpu
 */

#ifndef __GPU_SSR_H__
#define __GPU_SSR_H__

#include "BLI_sys_types.h" /* for bool */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GPUSSR {
	int w, h;
	float clipsta, clipend;

	/* Matrix that project to pixel coordinate (not Normalized Device Coordinates) */
	float pixelprojmat[4][4];
	
	struct GPUTexture *tex;
	struct GPUTexture *depth;
	struct GPUFrameBuffer *fb;
} GPUSSR;

void GPU_ssr_free(GPUSSR *ssr);
bool GPU_ssr_is_valid(GPUSSR *ssr, int w, int h);
GPUSSR *GPU_ssr_create(int width, int height);
void GPU_ssr_buffer_bind(GPUSSR *ssr, float winmat[4][4], int winsize[2], float clipsta, float clipend);
void GPU_ssr_buffer_unbind(GPUSSR *ssr);
void GPU_pbr_init_ssr_settings(struct GPUSSRSettings *ssr_settings);

#ifdef __cplusplus
}
#endif

#endif /*__GPU_SSR_H__*/
