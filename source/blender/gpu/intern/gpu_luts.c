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

#include "BLI_rand.h"
#include "BLI_math.h"

#include "GPU_texture.h"
#include "GPU_framebuffer.h"
#include "GPU_luts.h"

#include "gpu_codegen.h"

#include <string.h>

/* Structs */

/* Functions */

/* Van der Corput sequence */
 /* From http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html */
static float radical_inverse(int i) {
	unsigned int bits = (unsigned int)i;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return (float)bits * 2.3283064365386963e-10f;
}

static GPUTexture *create_hammersley_sample_texture(int samples)
{
	GPUTexture *tex;
	float (*texels)[2] = MEM_mallocN(sizeof(float[2]) * samples, "hammersley_tex");
	const float samples_inv = 1.0f / samples;
	int i;

	for (i = 0; i < samples; i++) {
		float phi = radical_inverse(i) * 2.0f * M_PI;
		texels[i][0] = cos(phi);
		texels[i][1] = sinf(phi);
	}

	tex = GPU_texture_create_1D_procedural(samples, (float *)texels, NULL);
	MEM_freeN(texels);
	return tex;
}

GPUTexture *create_jitter_texture(void)
{
	float jitter[64 * 64][2];
	int i;

	for (i = 0; i < 64 * 64; i++) {
		jitter[i][0] = 2.0f * BLI_frand() - 1.0f;
		jitter[i][1] = 2.0f * BLI_frand() - 1.0f;
		normalize_v2(jitter[i]);
	}

	return GPU_texture_create_2D_procedural(64, 64, &jitter[0][0], true, NULL);
}

#if 0
/* concentric mapping, see "A Low Distortion Map Between Disk and Square" and
 * http://psgraphics.blogspot.nl/2011/01/improved-code-for-concentric-map.html */
static GPUTexture * create_concentric_sample_texture(int side)
{
	GPUTexture *tex;
	float midpoint = 0.5f * (side - 1);
	float *texels = (float *)MEM_mallocN(sizeof(float) * 2 * side * side, "concentric_tex");
	int i, j;

	for (i = 0; i < side; i++) {
		for (j = 0; j < side; j++) {
			int index = (i * side + j) * 2;
			float a = 1.0f - i / midpoint;
			float b = 1.0f - j / midpoint;
			float phi, r;
			if (a * a > b * b) {
				r = a;
				phi = (M_PI_4) * (b / a);
			}
			else {
				r = b;
				phi = M_PI_2 - (M_PI_4) * (a / b);
			}
			texels[index] = r * cos(phi);
			texels[index + 1] = r * sin(phi);
		}
	}

	tex = GPU_texture_create_1D_procedural(side * side, texels, NULL);
	MEM_freeN(texels);
	return tex;
}
#endif

GPUTexture *create_spiral_sample_texture(int numsamples)
{
	GPUTexture *tex;
	float (*texels)[2] = MEM_mallocN(sizeof(float[2]) * numsamples, "concentric_tex");
	const float numsamples_inv = 1.0f / numsamples;
	int i;
	/* arbitrary number to ensure we don't get conciding samples every circle */
	const float spirals = 7.357;

	for (i = 0; i < numsamples; i++) {
		float r = (i + 0.5f) * numsamples_inv;
		float phi = r * spirals * (float)(2.0 * M_PI);
		texels[i][0] = r * cosf(phi);
		texels[i][1] = r * sinf(phi);
	}

	tex = GPU_texture_create_1D_procedural(numsamples, (float *)texels, NULL);
	MEM_freeN(texels);
	return tex;
}

GPUPBR *GPU_pbr_create(void)
{
	GPUPBR *pbr = MEM_callocN(sizeof(GPUPBR), "GPUPBR");

	/* 1024 is BSDF_SAMPLES in gpu_shader_material_utils.glsl */
	pbr->hammersley = create_hammersley_sample_texture(1024);
	pbr->jitter = create_jitter_texture();

	return pbr;
}

void GPU_pbr_free(GPUPBR *pbr)
{
	if (pbr->hammersley) {
		GPU_texture_free(pbr->hammersley);
		pbr->hammersley = NULL;
	}
	if (pbr->jitter) {
		GPU_texture_free(pbr->jitter);
		pbr->jitter = NULL;
	}
	MEM_freeN(pbr);
	pbr = NULL;
}
