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

/* ************************** OBJECT MODE ******************************* */


void DRW_mode_object_setup(DRWPass **wire_pass, DRWPass **center_pass)
{
	{
		/* Object Wires */
		*wire_pass = DRW_pass_create("Wire Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
	}

	{
		/* Object Center */
		DRWShadingGroup *grp;

		/* Static variables so they stay in memory */
		static float colorActive[4], colorSelect[4], colorDeselect[4];
		static float outlineColor[4];
		static float outlineWidth;
		static float size;

		outlineWidth = 1.0f * U.pixelsize;
		size = U.obcenter_dia * U.pixelsize + outlineWidth;
		UI_GetThemeColorShadeAlpha4fv(TH_ACTIVE, 0, -80, colorActive);
		UI_GetThemeColorShadeAlpha4fv(TH_SELECT, 0, -80, colorSelect);
		UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, colorDeselect);
		UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, outlineColor);

		GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_SMOOTH);
		*center_pass = DRW_pass_create("Obj Center Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_POINT);

		/* Active */
		grp = DRW_shgroup_create(sh, *center_pass);
		DRW_shgroup_uniform_float(grp, "size", &size, 1);
		DRW_shgroup_uniform_float(grp, "outlineWidth", &outlineWidth, 1);
		DRW_shgroup_uniform_vec4(grp, "color", colorActive, 1);
		DRW_shgroup_uniform_vec4(grp, "outlineColor", outlineColor, 1);

		/* Select */
		grp = DRW_shgroup_create(sh, *center_pass);
		DRW_shgroup_uniform_vec4(grp, "color", colorSelect, 1);

		/* Deselect */
		grp = DRW_shgroup_create(sh, *center_pass);
		DRW_shgroup_uniform_vec4(grp, "color", colorDeselect, 1);
	}
}

/* ******************************************** WIRES *********************************************** */

void DRW_shgroup_wire(struct DRWPass *pass, float frontcol[4], float backcol[4], struct Batch *geom, const float **obmat)
{
	bool is_perps = DRW_viewport_is_persp_get();
	GPUShader *sh;
	static bool bTrue = true;
	static bool bFalse = false;

	if (is_perps) {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_PERSP);
	}
	else {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_ORTHO);
	}

	DRWShadingGroup *batch = DRW_shgroup_create(sh, pass);
	DRW_shgroup_state_set(batch, DRW_STATE_WIRE);
	DRW_shgroup_uniform_vec4(batch, "frontColor", frontcol, 1);
	DRW_shgroup_uniform_vec4(batch, "backColor", backcol, 1);
	DRW_shgroup_uniform_bool(batch, "drawFront", &bTrue, 1);
	DRW_shgroup_uniform_bool(batch, "drawBack", &bTrue, 1);
	DRW_shgroup_uniform_bool(batch, "drawSilhouette", &bFalse, 1);

	DRW_shgroup_call_add(batch, geom, obmat);
}

void DRW_shgroup_outline(struct DRWPass *pass, float color[4], struct Batch *geom, const float **obmat)
{
	bool is_perps = DRW_viewport_is_persp_get();
	GPUShader *sh;
	static bool bTrue = true;
	static bool bFalse = false;

	if (is_perps) {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_PERSP);
	}
	else {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_ORTHO);
	}

	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_state_set(grp, DRW_STATE_WIRE_LARGE);
	DRW_shgroup_uniform_vec4(grp, "silhouetteColor", color, 1);
	DRW_shgroup_uniform_bool(grp, "drawFront", &bFalse, 1);
	DRW_shgroup_uniform_bool(grp, "drawBack", &bFalse, 1);
	DRW_shgroup_uniform_bool(grp, "drawSilhouette", &bTrue, 1);

	DRW_shgroup_call_add(grp, geom, obmat);
}

/* ******************************************** OBJECT MODE *********************************** */

void DRW_mode_object_add(struct DRWPass *wire_pass, struct DRWPass *UNUSED(center_pass), Object *ob)
{
	bool draw_wire = true;
	bool draw_outline = true;

	/* Get color */
	static float bg[3], low[4], med[4], base[4];
	UI_GetThemeColor3fv(TH_BACK, bg);
	//UI_GetThemeColor4fv(TH_WIRE, base);
	UI_GetThemeColor4fv(TH_ACTIVE, base);

	interp_v3_v3v3(low, bg, base, 0.333f);
	interp_v3_v3v3(med, bg, base, 0.667f);
	low[3] = base[3];
	med[3] = base[3];

	if (draw_wire) {
		struct Batch *geom = DRW_cache_wire_get(ob);
		DRW_shgroup_wire(wire_pass, med, low, geom, (float **)&ob->obmat);
	}

	if (draw_outline) {
		struct Batch *geom = DRW_cache_wire_get(ob);
		DRW_shgroup_outline(wire_pass, base, geom, (float **)&ob->obmat);
	}

	/* We need a way to populate a Batch with theses */
	// if (true) {
	// 	DRW_shgroup_point_add(center_active, ob->obmat[3]);
	// 	DRW_shgroup_point_add(center_select, ob->obmat[3]);
	// 	DRW_shgroup_point_add(center_deselect, ob->obmat[3]);
	// }
}