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

/** \file blender/gpu/intern/gpu_ssfx.c
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

#include "DNA_gpu_types.h"

#include "BLI_math.h"

#include "GPU_texture.h"
#include "GPU_framebuffer.h"
#include "GPU_ssfx.h"

#include "gpu_codegen.h"

#include <string.h>

/* Structs */


/* Functions */

void GPU_ssr_free(GPUSSR *ssr)
{
	if (ssr->tex) {
		GPU_texture_free(ssr->tex);
		ssr->tex = NULL;
	}
	if (ssr->depth) {
		GPU_texture_free(ssr->depth);
		ssr->depth = NULL;
	}
	if (ssr->fb) {
		GPU_framebuffer_free(ssr->fb);
		ssr->fb = NULL;
	}

	MEM_freeN(ssr);
}

bool GPU_ssr_is_valid(GPUSSR *ssr, int w, int h)
{
	return (( ssr->w == w ) && ( ssr->h == h ));
}

GPUSSR *GPU_ssr_create(int width, int height)
{
	GPUSSR *ssr = MEM_callocN(sizeof(GPUSSR), "GPUSSR");

	ssr->w = width;
	ssr->h = height;

	ssr->fb = GPU_framebuffer_create();
	if (!ssr->fb) {
		GPU_ssr_free(ssr);
		return NULL;
	}

	ssr->depth = GPU_texture_create_depth(width, height, NULL);
	if (!ssr->depth) {
		GPU_ssr_free(ssr);
		return NULL;
	}

	if (!GPU_framebuffer_texture_attach(ssr->fb, ssr->depth, 0, NULL)) {
		GPU_ssr_free(ssr);
		return NULL;
	}

	ssr->tex = GPU_texture_create_2D(width, height, NULL, 1, NULL);
	if (!ssr->tex) {
		GPU_ssr_free(ssr);
		return NULL;
	}

	if (!GPU_framebuffer_texture_attach(ssr->fb, ssr->tex, 0, NULL)) {
		GPU_ssr_free(ssr);
		return NULL;
	}
	
	/* check validity at the very end! */
	if (!GPU_framebuffer_check_valid(ssr->fb, NULL)) {
		GPU_ssr_free(ssr);
		return NULL;		
	}

	GPU_framebuffer_restore();
	
	return ssr;
}

void GPU_ssr_buffer_bind(GPUSSR *ssr, float winmat[4][4], int winsize[2], float clipsta, float clipend)
{
	glDisable(GL_SCISSOR_TEST);
	GPU_texture_bind_as_framebuffer(ssr->tex);

	winsize[0] = ssr->w;
	winsize[1] = ssr->h;

	ssr->clipsta = clipsta;
	ssr->clipend = clipend;

	{
		float uvpix[4][4], ndcuv[4][4], tmp[4][4];
	
		/* NDC to UVs */
		unit_m4(ndcuv);
		ndcuv[0][0] = ndcuv[1][1] = ndcuv[3][0] = ndcuv[3][1] = 0.5f;
	
		/* UVs to pixels */
		unit_m4(uvpix);
		uvpix[0][0] = ssr->w;
		uvpix[1][1] = ssr->h;
	
		mul_m4_m4m4(tmp, uvpix, ndcuv);
		mul_m4_m4m4(ssr->pixelprojmat, tmp, winmat);
	}
}

void GPU_ssr_buffer_unbind(GPUSSR *ssr)
{
	GPU_framebuffer_texture_unbind(ssr->fb, ssr->tex);
	GPU_framebuffer_restore();
	glEnable(GL_SCISSOR_TEST);
}

/* SSR Settings */

void GPU_pbr_init_ssr_settings(GPUSSRSettings *ssr_settings)
{
	ssr_settings->stride = 0.01f;
	ssr_settings->distance_max = 100.0f;
	ssr_settings->attenuation = 6.0f;
	ssr_settings->thickness = 0.1f;
	ssr_settings->steps = 32;
	ssr_settings->downsampling = 0;
}
