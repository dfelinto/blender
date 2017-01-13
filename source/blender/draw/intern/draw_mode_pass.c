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

/** \file blender/draw/draw_mode_pass.c
 *  \ingroup draw
 */

#include "DNA_userdef_types.h"

#include "GPU_shader.h"

#include "UI_resources.h"

#include "draw_mode_pass.h"

/* ************************** OBJECT MODE ********************************/

typedef struct WireBatchStorage {
	bool drawFront;
	bool drawBack;
	bool drawSilhouette;
	float frontColor[4];
	float backColor[4];
	float silhouetteColor[4];
} WireBatchStorage;

typedef struct ObjCenterBatchStorage {
	float size;
	float color[4];
	float outlineWidth;
	float outlineColor[4];
} ObjCenterBatchStorage;

void DRW_mode_object_setup(DRWPass **wire_pass, DRWPass **center_pass)
{
	{
		/* Object Wires */
		*wire_pass = DRW_pass_create("Wire Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
	}

	{
		/* Object Center */
		const float outlineWidth = 1.0f * U.pixelsize;
		const float size = U.obcenter_dia * U.pixelsize + outlineWidth;
		DRWBatch *batch;
		ObjCenterBatchStorage *storage;
		GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_SMOOTH);

		*center_pass = DRW_pass_create("Obj Center Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_POINT);

		/* Active */
		storage = MEM_callocN(sizeof(ObjCenterBatchStorage), "ObjCenterBatchStorage");
		storage->size = size;
		storage->outlineWidth = outlineWidth;
		UI_GetThemeColorShadeAlpha4fv(TH_ACTIVE, 0, -80, storage->color);
		UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, storage->outlineColor);

		batch = DRW_batch_create(sh, *center_pass, storage);
		DRW_batch_uniform_float(batch, "size", &storage->size, 1);
		DRW_batch_uniform_float(batch, "outlineWidth", &storage->outlineWidth, 1);
		DRW_batch_uniform_vec4(batch, "color", storage->outlineColor, 1);
		DRW_batch_uniform_vec4(batch, "outlineColor", storage->outlineColor, 1);

		/* Select (size depends on previous batch) */
		storage = MEM_callocN(sizeof(ObjCenterBatchStorage), "ObjCenterBatchStorage");
		UI_GetThemeColorShadeAlpha4fv(TH_SELECT, 0, -80, storage->color);

		batch = DRW_batch_create(sh, *center_pass, storage);
		DRW_batch_uniform_vec4(batch, "color", storage->outlineColor, 1);

		/* Deselect (size depends on previous batch) */
		storage = MEM_callocN(sizeof(ObjCenterBatchStorage), "ObjCenterBatchStorage");
		UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, storage->color);

		batch = DRW_batch_create(sh, *center_pass, storage);
		DRW_batch_uniform_vec4(batch, "color", storage->outlineColor, 1);
	}
}

void DRW_batch_wire(struct DRWPass *pass, float frontcol[4], float backcol[4], struct Batch *geom, const float **obmat)
{
	bool is_perps = DRW_viewport_is_persp_get();
	GPUShader *sh;

	if (is_perps) {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_PERSP);
	}
	else {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_ORTHO);
	}

	WireBatchStorage *storage = MEM_callocN(sizeof(WireBatchStorage), "WireBatchStorage");
	storage->drawBack = true;
	storage->drawFront = true;
	storage->drawSilhouette = false;

	copy_v3_v3(storage->backColor, backcol);
	copy_v3_v3(storage->frontColor, frontcol);

	DRWBatch *batch = DRW_batch_create(sh, pass, storage);
	DRW_batch_state_set(batch, DRW_STATE_WIRE);
	DRW_batch_uniform_vec4(batch, "frontColor", storage->frontColor, 1);
	DRW_batch_uniform_vec4(batch, "backColor", storage->backColor, 1);
	DRW_batch_uniform_bool(batch, "drawFront", &storage->drawFront, 1);
	DRW_batch_uniform_bool(batch, "drawBack", &storage->drawBack, 1);
	DRW_batch_uniform_bool(batch, "drawSilhouette", &storage->drawSilhouette, 1);

	DRW_batch_call_add(batch, geom, obmat);
}

void DRW_batch_outline(struct DRWPass *pass, float color[4], struct Batch *geom, const float **obmat)
{
	bool is_perps = DRW_viewport_is_persp_get();
	GPUShader *sh;

	if (is_perps) {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_PERSP);
	}
	else {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_ORTHO);
	}

	WireBatchStorage *storage = MEM_callocN(sizeof(WireBatchStorage), "WireBatchStorage");

	storage->drawBack = storage->drawFront = false;
	storage->drawSilhouette = true;
	copy_v3_v3(storage->silhouetteColor, color);

	DRWBatch *batch = DRW_batch_create(sh, pass, storage);
	DRW_batch_state_set(batch, DRW_STATE_WIRE_LARGE);
	DRW_batch_uniform_vec4(batch, "silhouetteColor", storage->silhouetteColor, 1);
	DRW_batch_uniform_bool(batch, "drawFront", &storage->drawFront, 1);
	DRW_batch_uniform_bool(batch, "drawBack", &storage->drawBack, 1);
	DRW_batch_uniform_bool(batch, "drawSilhouette", &storage->drawSilhouette, 1);

	DRW_batch_call_add(batch, geom, obmat);
}

void DRW_mode_object_add(struct DRWPass *wire_pass, struct DRWPass *UNUSED(center_pass), Object *ob)
{
	bool draw_wire = true;
	bool draw_outline = true;

	/* Get color */
	float bg[3], low[4], med[4], base[4];
	UI_GetThemeColor3fv(TH_BACK, bg);
	//UI_GetThemeColor4fv(TH_WIRE, base);
	UI_GetThemeColor4fv(TH_ACTIVE, base);

	interp_v3_v3v3(low, bg, base, 0.333f);
	interp_v3_v3v3(med, bg, base, 0.667f);
	low[3] = base[3];
	med[3] = base[3];

	if (draw_wire) {
		struct Batch *geom = DRW_cache_wire_get(ob);
		DRW_batch_wire(wire_pass, med, low, geom, (float **)&ob->obmat);
	}

	if (draw_outline) {
		struct Batch *geom = DRW_cache_wire_get(ob);
		DRW_batch_outline(wire_pass, base, geom, (float **)&ob->obmat);
	}

	/* We need a way to populate a Batch with theses */
	// if (true) {
	// 	DRW_batch_point_add(center_active, ob->obmat[3]);
	// 	DRW_batch_point_add(center_select, ob->obmat[3]);
	// 	DRW_batch_point_add(center_deselect, ob->obmat[3]);
	// }
}