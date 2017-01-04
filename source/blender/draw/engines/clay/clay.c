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

#include "DRW_render.h"

#include "BKE_icons.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "clay.h"

/* Shaders */

extern char datatoc_clay_frag_glsl[];
extern char datatoc_clay_vert_glsl[];

/* Storage */

static struct CLAY_data {
	/* Depth Pre Pass */
	struct DRWPass *depth_pass;
	struct GPUShader *depth_sh;
	struct DRWInterface *depth_itf;
	/* Shading Pass */
	struct DRWPass *clay_pass;
	struct GPUShader *clay_sh;
	struct DRWInterface *clay_itf;

	/* Matcap textures */
	struct GPUTexture *matcap_array;

	/* Ssao */
	float dfdyfac[2];
	float winmat[4][4];
	float viewvecs[3][4];
	float ssao_params[4];
	float sample_params[3];
} data = {NULL};

/* keep it under MAX_BUFFERS */
typedef struct CLAY_FramebufferList{
	/* default */
	struct GPUFrameBuffer *default_fb;
	/* engine specific */
	struct GPUFrameBuffer *downsample_depth;
} CLAY_FramebufferList;

/* keep it under MAX_TEXTURES */
typedef struct CLAY_TextureList{
	/* default */
	struct GPUTexture *color;
	struct GPUTexture *depth;
	/* engine specific */
	struct GPUTexture *depth_low;
} CLAY_TextureList;

/* for clarity follow the same layout as CLAY_TextureList */
#define SCENE_COLOR 0
#define SCENE_DEPTH 1
#define SCENE_DEPTH_LOW 2

/* Functions */
static void add_icon_to_rect(PreviewImage *prv, float *final_rect, int layer)
{
	int image_size = prv->w[0] * prv->h[0];
	float *new_rect = &final_rect[image_size * 4 * layer];

	IMB_buffer_float_from_byte(new_rect, prv->rect[0], IB_PROFILE_SRGB, IB_PROFILE_SRGB,
	                           false, prv->w[0], prv->h[0], prv->w[0], prv->w[0]);
}

static void clay_init_engine(void)
{
	DRWBatch *batch;

	/* Create Texture Array */
	{
		PreviewImage *prv[2];
		int layers = 2; /* For now only use the 24 internal matcaps */

		prv[0] = UI_icon_to_preview(ICON_MATCAP_02);
		float *final_rect = MEM_callocN(sizeof(float) * 4 * prv[0]->w[0] * prv[0]->h[0] * layers, "Clay Matcap array rect");
		add_icon_to_rect(prv[0], final_rect, 0);

		prv[1] = UI_icon_to_preview(ICON_MATCAP_03);
		add_icon_to_rect(prv[1], final_rect, 1);

		data.matcap_array = DRW_texture_create_2D_array(prv[1]->w[0], prv[1]->h[0], layers, final_rect);
		MEM_freeN(final_rect);
		BKE_previewimg_free(&prv[0]);
		BKE_previewimg_free(&prv[1]);
	}

	/* Depth prepass */
	{
		data.depth_sh = DRW_shader_create_3D_depth_only();
		data.depth_itf = DRW_interface_create(data.depth_sh);

		data.depth_pass = (DRWPass *)MEM_callocN(sizeof(DRWPass), "DRWPass depth_pass");
		data.depth_pass->state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;

		batch = (DRWBatch *)MEM_callocN(sizeof(DRWBatch), "DRWBatch");
		batch->shader = data.depth_sh;
		batch->interface = data.depth_itf;
		BLI_addtail(&data.depth_pass->batches, batch);
	}

	/* Shading pass */
	{
		int bindloc = 0;

		data.clay_sh = DRW_shader_create(datatoc_clay_vert_glsl, NULL, datatoc_clay_frag_glsl, NULL);
		data.clay_itf = DRW_interface_create(data.clay_sh);

		DRW_interface_uniform_ivec2(data.clay_sh, data.clay_itf, "screenres", DRW_viewport_size_get(), 1);
		DRW_interface_uniform_buffer(data.clay_sh, data.clay_itf, "depthtex", SCENE_DEPTH, bindloc++);
		DRW_interface_uniform_texture(data.clay_sh, data.clay_itf, "matcaps", data.matcap_array, bindloc++);

		/* SSAO */
		DRW_interface_uniform_mat4(data.clay_sh, data.clay_itf, "WinMatrix", data.winmat);
		DRW_interface_uniform_vec4(data.clay_sh, data.clay_itf, "viewvecs", data.viewvecs, 3);
		DRW_interface_uniform_vec4(data.clay_sh, data.clay_itf, "ssao_params", data.ssao_params, 1);
		DRW_interface_uniform_vec3(data.clay_sh, data.clay_itf, "ssao_sample_params", data.sample_params, 1);

		data.clay_pass = (DRWPass *)MEM_callocN(sizeof(DRWPass), "DRWPass clay_pass");
		data.clay_pass->state = DRW_STATE_WRITE_COLOR;

		batch = (DRWBatch *)MEM_callocN(sizeof(DRWBatch), "DRWBatch");
		batch->shader = data.clay_sh;
		batch->interface = data.clay_itf;
		BLI_addtail(&data.clay_pass->batches, batch);
	}
}

#if 0
static void clay_init_view(CLAY_FramebufferList *buffers, CLAY_TextureList *textures)
{
	int *viewsize = DRW_viewport_size_get();

	DRWFboTexture depth = {&textures->depth_low, DRW_BUF_R_16};

	DRW_framebuffer_init(&buffers->downsample_depth, viewsize[0]/2, viewsize[1]/2, &depth, 1);
}
#endif

static void clay_populate_batch(const struct bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	Scene *sce_iter;
	Base *base;
	DRWBatch *matcapbatch = data.clay_pass->batches.first;
	DRWBatch *depthbatch = data.depth_pass->batches.first;

	for (SETLOOPER(scene, sce_iter, base)) {
		/* Add everything for now */
		BLI_addtail(&matcapbatch->objects, base);
		BLI_addtail(&depthbatch->objects, base);
	}
}

static void clay_ssao_setup(void)
{
	float invproj[4][4];
	float dfdyfacs[2];
	bool is_persp = DRW_viewport_is_persp();
	/* view vectors for the corners of the view frustum. Can be used to recreate the world space position easily */
	float viewvecs[3][4] = {
	    {-1.0f, -1.0f, -1.0f, 1.0f},
	    {1.0f, -1.0f, -1.0f, 1.0f},
	    {-1.0f, 1.0f, -1.0f, 1.0f}
	};
	int i;

	DRW_get_dfdy_factors(dfdyfacs);

	data.ssao_params[0] = 1.0f; /* Max distance */
	data.ssao_params[1] = 1.0f; /* Factor */
	data.ssao_params[2] = 1.0f; /* Attenuation */
	data.ssao_params[3] = dfdyfacs[1]; /* dfdy sign */

	/* invert the view matrix */
	DRW_viewport_matrix_get(data.winmat, DRW_MAT_WIN);
	invert_m4_m4(invproj, data.winmat);

	/* convert the view vectors to view space */
	for (i = 0; i < 3; i++) {
		mul_m4_v4(invproj, viewvecs[i]);
		/* normalized trick see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
		mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
		if (is_persp)
			mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
		viewvecs[i][3] = 1.0;

		copy_v4_v4(data.viewvecs[i], viewvecs[i]);
	}

	/* we need to store the differences */
	data.viewvecs[1][0] -= data.viewvecs[0][0];
	data.viewvecs[1][1] = data.viewvecs[2][1] - data.viewvecs[0][1];

	/* calculate a depth offset as well */
	if (!is_persp) {
		float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
		mul_m4_v4(invproj, vec_far);
		mul_v3_fl(vec_far, 1.0f / vec_far[3]);
		data.viewvecs[1][2] = vec_far[2] - data.viewvecs[0][2];
	}
}

static void clay_view_draw(RenderEngine *UNUSED(engine), const struct bContext *context)
{
	/* This function may run for multiple viewports
	 * so get the current viewport buffers */
	CLAY_FramebufferList *buffers = NULL;
	CLAY_TextureList *textures = NULL;

	DRW_viewport_init(context, (void **)&buffers, (void **)&textures);
	
	if (!data.clay_sh)
		clay_init_engine();

	/* TODO : tag to refresh by the deps graph */
	/* ideally only refresh when objects are added/removed */
	/* or render properties / materials change */
	static bool first = true;
	if (first) {
		first = false;
		clay_populate_batch(context);
	}

	DRW_draw_background();

	/* Pass 1 : Depth pre-pass */
	DRW_draw_pass(data.depth_pass);

	clay_ssao_setup();

	/* Pass 2 : Shading */
	DRW_framebuffer_texture_detach(textures->depth);
	DRW_draw_pass(data.clay_pass);
	DRW_framebuffer_texture_attach(buffers->default_fb, textures->depth, 0);

	/* Always finish by this */
	DRW_state_reset();
}

RenderEngineType viewport_clay_type = {
	NULL, NULL,
	"BLENDER_CLAY", N_("Clay"), RE_INTERNAL | RE_USE_OGL_PIPELINE,
	NULL, NULL, NULL, NULL, &clay_view_draw, NULL,
	{NULL, NULL, NULL}
};