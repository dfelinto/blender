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

/** \file blender/gpu/intern/gpu_pbr.c
 *  \ingroup gpu
 *
 * Manages lut textures and render passes for pbr rendering
 */


#include <math.h>
#include <string.h>
#include "MEM_guardedalloc.h"

#include "DNA_gpu_types.h"

#include "BLI_math.h"

#include "GPU_texture.h"
#include "GPU_framebuffer.h"
#include "GPU_pbr.h"
#include "GPU_draw.h"

#include "gpu_codegen.h"

#include <string.h>

/* Structs */

/* Functions */

void GPU_scenebuf_free(GPUScreenBuffer *buf)
{
	if (buf->fb) {
		GPU_framebuffer_free(buf->fb);
		buf->fb = NULL;
	}
	if (buf->tex) {
		GPU_texture_free(buf->tex);
		buf->tex = NULL;
	}
	if (buf->depth) {
		GPU_texture_free(buf->depth);
		buf->depth = NULL;
	}
	if (buf->downsamplingfb) {
		GPU_framebuffer_free(buf->downsamplingfb);
		buf->downsamplingfb = NULL;
	}

	MEM_freeN(buf);
}

static GPUScreenBuffer *gpu_scenebuf_create(int width, int height, bool depth_only)
{
	GPUScreenBuffer *buf = MEM_callocN(sizeof(GPUScreenBuffer), "GPUScreenBuffer");

	buf->w = width;
	buf->h = height;
	buf->depth = NULL;

	buf->fb = GPU_framebuffer_create();
	if (!buf->fb) {
		GPU_scenebuf_free(buf);
		return NULL;
	}

	/* DOWNSAMPLING FB */
	buf->downsamplingfb = GPU_framebuffer_create();
	if (!buf->downsamplingfb) {
		GPU_scenebuf_free(buf);
		return NULL;
	}

	if (depth_only) {
		buf->tex = GPU_texture_create_depth_buffer(width, height, NULL);
		if (!buf->tex) {
			GPU_scenebuf_free(buf);
			return NULL;
		}

		if (!GPU_framebuffer_texture_attach(buf->fb, buf->tex, 0)) {
			GPU_scenebuf_free(buf);
			return NULL;
		}

		/* check validity at the very end! */
		if (!GPU_framebuffer_check_valid(buf->fb, NULL)) {
			GPU_scenebuf_free(buf);
			return NULL;
		}
	}
	else {
		buf->depth = GPU_texture_create_depth_buffer(width, height, NULL);
		if (!buf->depth) {
			GPU_scenebuf_free(buf);
			return NULL;
		}

		if (!GPU_framebuffer_texture_attach(buf->fb, buf->depth, 0)) {
			GPU_scenebuf_free(buf);
			return NULL;
		}

		buf->tex = GPU_texture_create_2D(width, height, NULL, 1, NULL);
		if (!buf->tex) {
			GPU_scenebuf_free(buf);
			return NULL;
		}

		if (!GPU_framebuffer_texture_attach(buf->fb, buf->tex, 0)) {
			GPU_scenebuf_free(buf);
			return NULL;
		}

		/* check validity at the very end! */
		if (!GPU_framebuffer_check_valid(buf->fb, NULL)) {
			GPU_scenebuf_free(buf);
			return NULL;
		}
	}
	

	GPU_framebuffer_restore();

	return buf;
}

GPUScreenBuffer *GPU_pbr_scene_buffer(GPUPBR *pbr, int width, int height)
{
	if (pbr->scene) {
		if ((pbr->scene->w == width) && (pbr->scene->h == height))
			return pbr->scene;
		else
			GPU_scenebuf_free(pbr->scene);
	}

	pbr->scene = gpu_scenebuf_create(width, height, false);

	return pbr->scene;
}

GPUScreenBuffer *GPU_pbr_backface_buffer(GPUPBR *pbr, int width, int height)
{
	if (pbr->backface) {
		if ((pbr->backface->w == width) && (pbr->backface->h == height))
			return pbr->backface;
		else
			GPU_scenebuf_free(pbr->backface);
	}

	pbr->backface = gpu_scenebuf_create(width, height, true);

	return pbr->backface;
}

void GPU_scenebuf_bind(GPUScreenBuffer* buf, float winmat[4][4], int winsize[2], float clipsta, float clipend)
{
	glDisable(GL_SCISSOR_TEST);

	GPU_texture_bind_as_framebuffer(buf->tex);

	winsize[0] = buf->w;
	winsize[1] = buf->h;

	buf->clipsta = clipsta;
	buf->clipend = clipend;

	{
		float uvpix[4][4], ndcuv[4][4], tmp[4][4];
	
		/* NDC to UVs */
		unit_m4(ndcuv);
		ndcuv[0][0] = ndcuv[1][1] = ndcuv[3][0] = ndcuv[3][1] = 0.5f;
	
		/* UVs to pixels */
		unit_m4(uvpix);
		uvpix[0][0] = buf->w;
		uvpix[1][1] = buf->h;
	
		mul_m4_m4m4(tmp, uvpix, ndcuv);
		mul_m4_m4m4(buf->pixelprojmat, tmp, winmat);
	}
}

void GPU_scenebuf_unbind(GPUScreenBuffer* buf)
{
	GPU_framebuffer_texture_unbind(buf->fb, buf->tex);
	GPU_framebuffer_restore();
	glEnable(GL_SCISSOR_TEST);
}

void GPU_scenebuf_filter_texture(GPUScreenBuffer* buf)
{
	/* MinZ Pyramid for depth */
	if (buf->depth) {
		GPU_texture_bind(buf->depth, 0);
		GPU_generate_mipmap(GL_TEXTURE_2D);
		GPU_texture_unbind(buf->depth);
		GPU_framebuffer_hiz_construction(buf->downsamplingfb, buf->depth, false);
		GPU_framebuffer_texture_attach(buf->fb, buf->depth, 0);
	}
	else {
		GPU_texture_bind(buf->tex, 0);
		GPU_generate_mipmap(GL_TEXTURE_2D);
		GPU_texture_unbind(buf->tex);
		GPU_framebuffer_hiz_construction(buf->downsamplingfb, buf->tex, true);
		GPU_framebuffer_texture_attach(buf->fb, buf->tex, 0);
	}

	GPU_framebuffer_restore();
}

/* PBR core */

GPUPBR *GPU_pbr_create(void)
{
	GPUPBR *pbr = MEM_callocN(sizeof(GPUPBR), "GPUPBR");

	pbr->hammersley = GPU_create_hammersley_sample_texture(1024);
	pbr->jitter = GPU_create_random_texture();
	pbr->ltc_mat_ggx = GPU_create_ltc_mat_ggx_lut_texture();
	pbr->ltc_mag_ggx = GPU_create_ltc_mag_ggx_lut_texture();

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
	if (pbr->ltc_mat_ggx) {
		GPU_texture_free(pbr->ltc_mat_ggx);
		pbr->ltc_mat_ggx = NULL;
	}
	if (pbr->ltc_mag_ggx) {
		GPU_texture_free(pbr->ltc_mag_ggx);
		pbr->ltc_mag_ggx = NULL;
	}

	if (pbr->scene) {
		GPU_scenebuf_free(pbr->scene);
	}

	if (pbr->backface) {
		GPU_scenebuf_free(pbr->backface);
	}

	MEM_freeN(pbr);
}

/* Settings */

static void gpu_pbr_init_ssr_settings(GPUSSRSettings *ssr_settings)
{
	ssr_settings->attenuation = 6.0f;
	ssr_settings->thickness = 0.2f;
	ssr_settings->steps = 32;
}

static void gpu_pbr_init_ssao_settings(GPUSSAOSettings *ssao_settings)
{
	ssao_settings->factor = 1.0f;
	ssao_settings->attenuation = 1.0f;
	ssao_settings->distance_max = 0.2f;
	ssao_settings->samples = 16;
	ssao_settings->steps = 2;
}

static void gpu_pbr_init_brdf_settings(GPUBRDFSettings *brdf_settings)
{
	brdf_settings->lodbias = -0.5f;
	brdf_settings->samples = 32;
}

void GPU_pbr_settings_validate(GPUPBRSettings *pbr_settings)
{
	if (pbr_settings->pbr_flag & GPU_PBR_FLAG_ENABLE) {
		if (pbr_settings->brdf == NULL) {
			GPUBRDFSettings *brdf_settings;
			brdf_settings = pbr_settings->brdf = MEM_callocN(sizeof(GPUBRDFSettings), __func__);
			gpu_pbr_init_brdf_settings(brdf_settings);			
		}

		if ((pbr_settings->pbr_flag & GPU_PBR_FLAG_SSAO) && (pbr_settings->ssao == NULL)) {
			GPUSSAOSettings *ssao_settings;
			ssao_settings = pbr_settings->ssao = MEM_callocN(sizeof(GPUSSAOSettings), __func__);
			gpu_pbr_init_ssao_settings(ssao_settings);
		}

		if ((pbr_settings->pbr_flag & GPU_PBR_FLAG_SSR) && (pbr_settings->ssr == NULL)) {
			GPUSSRSettings *ssr_settings;
			ssr_settings = pbr_settings->ssr = MEM_callocN(sizeof(GPUSSRSettings), __func__);
			gpu_pbr_init_ssr_settings(ssr_settings);
		}
	}
}