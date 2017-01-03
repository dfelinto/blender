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

#include "clay.h"

/* Shaders */

extern char datatoc_clay_frag_glsl[];
extern char datatoc_clay_vert_glsl[];
extern char datatoc_clay_downsample_depth_frag_glsl[];
extern char datatoc_clay_debug_frag_glsl[];


/* Storage */

static struct CLAY_data {
	/* Depth Pre Pass */
	struct DRWPass *depth_pass;
	struct GPUShader *depth_shader;
	struct DRWInterface *depth_interface;
	/* Depth Downsample Pass */
	struct DRWPass *downsample_pass;
	struct GPUShader *downsample_shader;
	struct DRWInterface *downsample_interface;
	/* Shading Pass */
	struct DRWPass *clay_pass;
	struct GPUShader *clay_shader;
	struct DRWInterface *clay_interface;

	/* Debug Pass */
	struct DRWPass *debug_pass;
	struct GPUShader *debug_shader;
	struct DRWInterface *debug_interface;
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

/* for clarity follow the same layout as TextureList */
#define SCENE_COLOR 0
#define SCENE_DEPTH 1
#define SCENE_DEPTH_LOW 2

/* Functions */

static void clay_init_engine(void)
{
	DRWBatch *batch;

	/* Depth prepass */
	{
		data.depth_shader = DRW_shader_create_3D_depth_only();
		data.depth_interface = DRW_interface_create(data.depth_shader);

		data.depth_pass = (DRWPass *)MEM_callocN(sizeof(DRWPass), "DRWPass depth_pass");
		data.depth_pass->state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;

		batch = (DRWBatch *)MEM_callocN(sizeof(DRWBatch), "DRWBatch");
		batch->shader = data.depth_shader;
		batch->interface = data.depth_interface;
		BLI_addtail(&data.depth_pass->batches, batch);
	}

	/* Downsample pass */
	{
		data.downsample_shader = DRW_shader_create_2D(datatoc_clay_downsample_depth_frag_glsl, NULL);
		data.downsample_interface = DRW_interface_create(data.downsample_shader);
		DRW_interface_uniform_int(data.downsample_shader, data.downsample_interface, "screenres", DRW_get_viewport_size(), 2);
		DRW_interface_uniform_buffer(data.downsample_shader, data.downsample_interface, "depthtex", SCENE_DEPTH, 0);

		data.downsample_pass = (DRWPass *)MEM_callocN(sizeof(DRWPass), "DRWPass downsample_pass");
		data.downsample_pass->state = DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR;

		batch = (DRWBatch *)MEM_callocN(sizeof(DRWBatch), "DRWBatch");
		batch->shader = data.downsample_shader;
		batch->interface = data.downsample_interface;		
		BLI_addtail(&data.downsample_pass->batches, batch);		
	}

	/* Shading pass */
	{
		data.clay_shader = DRW_shader_create(datatoc_clay_vert_glsl, NULL, datatoc_clay_frag_glsl, NULL);
		data.clay_interface = DRW_interface_create(data.clay_shader);
		//DRW_interface_uniform_int(data.clay_shader, data.clay_interface, "screenres", DRW_get_viewport_size(), 2);
		//DRW_interface_uniform_float(data.clay_shader, data.clay_interface, "color", col, 4);

		data.clay_pass = (DRWPass *)MEM_callocN(sizeof(DRWPass), "DRWPass clay_pass");
		data.clay_pass->state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;

		batch = (DRWBatch *)MEM_callocN(sizeof(DRWBatch), "DRWBatch");
		batch->shader = data.clay_shader;
		batch->interface = data.clay_interface;
		BLI_addtail(&data.clay_pass->batches, batch);		
	}

	/* Debug */
	{
		data.debug_shader = DRW_shader_create_2D(datatoc_clay_debug_frag_glsl, NULL);
		data.debug_interface = DRW_interface_create(data.debug_shader);
		DRW_interface_uniform_int(data.debug_shader, data.debug_interface, "screenres", DRW_get_viewport_size(), 2);
		DRW_interface_uniform_buffer(data.debug_shader, data.debug_interface, "depthtex", SCENE_DEPTH_LOW, 0);

		data.debug_pass = (DRWPass *)MEM_callocN(sizeof(DRWPass), "DRWPass debug_pass");
		data.debug_pass->state = DRW_STATE_WRITE_COLOR;

		batch = (DRWBatch *)MEM_callocN(sizeof(DRWBatch), "DRWBatch");
		batch->shader = data.debug_shader;
		batch->interface = data.debug_interface;
		BLI_addtail(&data.debug_pass->batches, batch);		
	}
}

static void clay_init_view(CLAY_FramebufferList *buffers, CLAY_TextureList *textures)
{
	int *viewsize = DRW_get_viewport_size();

	DRWFboTexture depth = {&textures->depth_low, DRW_BUF_R_16};

	DRW_framebuffer_init(&buffers->downsample_depth, viewsize[0]/2, viewsize[1]/2, &depth, 1);
}

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

static void clay_view_draw(RenderEngine *UNUSED(engine), const struct bContext *context)
{
	/* This function may run for multiple viewports
	 * so get the current viewport buffers */
	CLAY_FramebufferList *buffers = NULL;
	CLAY_TextureList *textures = NULL;

	DRW_init_viewport(context, (void **)&buffers, (void **)&textures);
	
	if (!data.clay_shader)
		clay_init_engine();

	clay_init_view(buffers, textures);

	/* TODO : tag to refresh by the deps graph */
	/* ideally only refresh when objects are added/removed */
	/* or render properties / materials change */
	static bool first = true;
	if (first) {
		first = false;
		clay_populate_batch(context);
	}

	DRW_draw_background();

	/* Step 1 : Depth pre-pass */
	DRW_draw_pass(data.depth_pass, context);

	/* Step 2 : downsample the depth buffer to a new buffer */
	DRW_framebuffer_bind(buffers->downsample_depth);
	DRW_draw_pass_fullscreen(data.downsample_pass);

	/* Step 3 : Shading pass */
	DRW_framebuffer_bind(buffers->default_fb);
	//DRW_draw_pass(data.clay_pass, context);

	DRW_draw_pass_fullscreen(data.debug_pass);

	DRW_reset_state();
}

RenderEngineType viewport_clay_type = {
	NULL, NULL,
	"BLENDER_CLAY", N_("Clay"), RE_INTERNAL | RE_USE_OGL_PIPELINE,
	NULL, NULL, NULL, NULL, &clay_view_draw, NULL,
	{NULL, NULL, NULL}
};