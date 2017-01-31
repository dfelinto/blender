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

#include "BKE_global.h"

#include "draw_mode_pass.h"

/* ************************** OBJECT MODE ******************************* */

/* Store list of shading group for easy access*/

/* Empties */
static DRWShadingGroup *empty_wire;
static DRWShadingGroup *empty_active;
static DRWShadingGroup *empty_select;
static DRWShadingGroup *empty_transform;
static DRWShadingGroup *empty_group;
static DRWShadingGroup *empty_group_active;

/* Helpers */
static DRWShadingGroup *relationship_lines;

/* Objects Centers */
static DRWShadingGroup *center_active;
static DRWShadingGroup *center_selected;
static DRWShadingGroup *center_deselected;

/* This Function setup the passes needed for the mode rendering.
 * The passes are populated by the rendering engine using the DRW_shgroup_* functions. */
void DRW_pass_setup_common(DRWPass **wire_overlay, DRWPass **wire_outline, DRWPass **non_meshes, DRWPass **ob_center)
{
	if (wire_overlay) {
		/* This pass can draw mesh edges top of Shaded Meshes without any Z fighting */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND;
		*wire_overlay = DRW_pass_create("Wire Overlays Pass", state);
	}

	if (wire_outline) {
		/* This pass can draw mesh outlines and/or fancy wireframe */
		/* Fancy wireframes are not meant to be occluded (without Z offset) */
		/* Outlines and Fancy Wires use the same VBO */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		*wire_outline = DRW_pass_create("Wire + Outlines Pass", state);
	}

	if (non_meshes) {
		static float colorWire[4], colorWireEdit[4];
		static float colorActive[4], colorSelect[4], colorTransform[4], colorGroup[4], colorGroupActive[4];
		static float colorEmpty[4], colorLamp[4], colorCamera[4], colorSpeaker[4];

		UI_GetThemeColor4fv(TH_WIRE, colorWire);
		UI_GetThemeColor4fv(TH_WIRE_EDIT, colorWireEdit);
		UI_GetThemeColor4fv(TH_ACTIVE, colorActive);
		UI_GetThemeColor4fv(TH_SELECT, colorSelect);
		UI_GetThemeColor4fv(TH_TRANSFORM, colorTransform);
		UI_GetThemeColor4fv(TH_GROUP_ACTIVE, colorGroupActive);
		UI_GetThemeColor4fv(TH_GROUP, colorGroup);
		UI_GetThemeColor4fv(OB_LAMP, colorLamp);
		UI_GetThemeColor4fv(OB_SPEAKER, colorSpeaker);
		UI_GetThemeColor4fv(OB_CAMERA, colorCamera);
		UI_GetThemeColor4fv(OB_EMPTY, colorEmpty);

		/* Non Meshes Pass (Camera, empties, lamps ...) */
		DRWShadingGroup *grp;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		state |= DRW_STATE_WIRE ;//| DRW_STATE_LINE_SMOOTH;
		*non_meshes = DRW_pass_create("Non Meshes Pass", state);

		GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
		GPUShader *sh_inst = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR_INSTANCE);

		/* Solid Wires */
		grp = DRW_shgroup_create(sh, *non_meshes);

		/* Points */
		grp = DRW_shgroup_create(sh, *non_meshes);

		/* Empties */
		{
			grp = DRW_shgroup_create(sh_inst, *non_meshes);
			DRW_shgroup_uniform_vec4(grp, "color", colorEmpty, 1);
			DRW_shgroup_dyntype_set(grp, DRW_DYN_INSTANCE);
			empty_wire = grp;

			grp = DRW_shgroup_create(sh_inst, *non_meshes);
			DRW_shgroup_uniform_vec4(grp, "color", colorActive, 1);
			DRW_shgroup_dyntype_set(grp, DRW_DYN_INSTANCE);
			empty_active = grp;

			grp = DRW_shgroup_create(sh_inst, *non_meshes);
			DRW_shgroup_uniform_vec4(grp, "color", colorSelect, 1);
			DRW_shgroup_dyntype_set(grp, DRW_DYN_INSTANCE);
			empty_select = grp;

			grp = DRW_shgroup_create(sh_inst, *non_meshes);
			DRW_shgroup_uniform_vec4(grp, "color", colorTransform, 1);
			DRW_shgroup_dyntype_set(grp, DRW_DYN_INSTANCE);
			empty_transform = grp;

			grp = DRW_shgroup_create(sh_inst, *non_meshes);
			DRW_shgroup_uniform_vec4(grp, "color", colorGroup, 1);
			DRW_shgroup_dyntype_set(grp, DRW_DYN_INSTANCE);
			empty_group = grp;

			grp = DRW_shgroup_create(sh_inst, *non_meshes);
			DRW_shgroup_uniform_vec4(grp, "color", colorGroupActive, 1);
			DRW_shgroup_dyntype_set(grp, DRW_DYN_INSTANCE);
			empty_group_active = grp;
		}

		/* Stipple Wires */
		grp = DRW_shgroup_create(sh, *non_meshes);
		DRW_shgroup_state_set(grp, DRW_STATE_STIPPLE_2);

		grp = DRW_shgroup_create(sh, *non_meshes);
		DRW_shgroup_state_set(grp, DRW_STATE_STIPPLE_3);

		grp = DRW_shgroup_create(sh, *non_meshes);
		DRW_shgroup_state_set(grp, DRW_STATE_STIPPLE_4);

		/* Relationship Lines */
		{
			grp = DRW_shgroup_create(sh, *non_meshes);
			DRW_shgroup_uniform_vec4(grp, "color", colorWire, 1);
			DRW_shgroup_state_set(grp, DRW_STATE_STIPPLE_3);
			DRW_shgroup_dyntype_set(grp, DRW_DYN_LINES);
			relationship_lines = grp;
		}
	}

	if (ob_center) {
		/* Object Center pass grouped by State */
		DRWShadingGroup *grp;
		static float colorActive[4], colorSelect[4], colorDeselect[4], outlineColor[4];
		static float outlineWidth, size;

		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND | DRW_STATE_POINT;
		*ob_center = DRW_pass_create("Obj Center Pass", state);

		outlineWidth = 1.0f * U.pixelsize;
		size = U.obcenter_dia * U.pixelsize + outlineWidth;
		UI_GetThemeColorShadeAlpha4fv(TH_ACTIVE, 0, -80, colorActive);
		UI_GetThemeColorShadeAlpha4fv(TH_SELECT, 0, -80, colorSelect);
		UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, colorDeselect);
		UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, outlineColor);

		GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_SMOOTH);

		/* Active */
		grp = DRW_shgroup_create(sh, *ob_center);
		DRW_shgroup_dyntype_set(grp, DRW_DYN_POINTS);
		DRW_shgroup_uniform_float(grp, "size", &size, 1);
		DRW_shgroup_uniform_float(grp, "outlineWidth", &outlineWidth, 1);
		DRW_shgroup_uniform_vec4(grp, "color", colorActive, 1);
		DRW_shgroup_uniform_vec4(grp, "outlineColor", outlineColor, 1);
		center_active = grp;

		/* Select */
		grp = DRW_shgroup_create(sh, *ob_center);
		DRW_shgroup_dyntype_set(grp, DRW_DYN_POINTS);
		DRW_shgroup_uniform_vec4(grp, "color", colorSelect, 1);
		center_selected = grp;

		/* Deselect */
		grp = DRW_shgroup_create(sh, *ob_center);
		DRW_shgroup_dyntype_set(grp, DRW_DYN_POINTS);
		DRW_shgroup_uniform_vec4(grp, "color", colorDeselect, 1);
		center_deselected = grp;
	}
}

/* ******************************************** WIRES *********************************************** */

void DRW_shgroup_wire_overlay(DRWPass *wire_overlay, Object *ob)
{
	struct Batch *geom = DRW_cache_wire_outline_get(ob);
	GPUShader *sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_OVERLAY);

	DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_overlay);
	DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);

	DRW_shgroup_call_add(grp, geom, ob->obmat);
}

void DRW_shgroup_wire_outline(DRWPass *wire_outline, Object *ob,
                              const bool do_front, const bool do_back, const bool do_outline)
{
	GPUShader *sh;
	struct Batch *geom = DRW_cache_wire_outline_get(ob);

	/* Get color */
	/* TODO get the right color depending on ob state (Groups, overides etc..) */
	static float frontcol[4], backcol[4], color[4];
	UI_GetThemeColor4fv(TH_ACTIVE, color);
	copy_v4_v4(frontcol, color);
	copy_v4_v4(backcol, color);
	backcol[3] = 0.333f;
	frontcol[3] = 0.667f;

#if 1 /* New wire */

	bool is_perps = DRW_viewport_is_persp_get();
	static bool bTrue = true;
	static bool bFalse = false;

	/* Note (TODO) : this requires cache to be discarded on ortho/perp switch
	 * It may be preferable (or not depending on performance implication)
	 * to introduce a shader uniform switch */
	if (is_perps) {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_PERSP);
	}
	else {
		sh = GPU_shader_get_builtin_shader(GPU_SHADER_EDGES_FRONT_BACK_ORTHO);
	}

	if (do_front || do_back) {
		bool *bFront = (do_front) ? &bTrue : &bFalse;
		bool *bBack = (do_back) ? &bTrue : &bFalse;

		DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_outline);
		DRW_shgroup_state_set(grp, DRW_STATE_WIRE);
		DRW_shgroup_uniform_vec4(grp, "frontColor", frontcol, 1);
		DRW_shgroup_uniform_vec4(grp, "backColor", backcol, 1);
		DRW_shgroup_uniform_bool(grp, "drawFront", bFront, 1);
		DRW_shgroup_uniform_bool(grp, "drawFront", bBack, 1);
		DRW_shgroup_uniform_bool(grp, "drawBack", &bFalse, 1);
		DRW_shgroup_uniform_bool(grp, "drawSilhouette", &bFalse, 1);

		DRW_shgroup_call_add(grp, geom, ob->obmat);
	}

	if (do_outline) {
		DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_outline);
		DRW_shgroup_state_set(grp, DRW_STATE_WIRE_LARGE);
		DRW_shgroup_uniform_vec4(grp, "silhouetteColor", color, 1);
		DRW_shgroup_uniform_bool(grp, "drawFront", &bFalse, 1);
		DRW_shgroup_uniform_bool(grp, "drawBack", &bFalse, 1);
		DRW_shgroup_uniform_bool(grp, "drawSilhouette", &bTrue, 1);

		DRW_shgroup_call_add(grp, geom, ob->obmat);
	}

#else /* Old (flat) wire */

	sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	DRWShadingGroup *grp = DRW_shgroup_create(sh, wire_outline);
	DRW_shgroup_state_set(grp, DRW_STATE_WIRE_LARGE);
	DRW_shgroup_uniform_vec4(grp, "color", frontcol, 1);

	DRW_shgroup_call_add(grp, geom, ob->obmat);
#endif

}

/* ***************************** NON MESHES ********************** */

/* TODO FINISH */
static int draw_object_wire_theme(ObjectBase *base)
{
	Object *ob = base->object;
	const bool is_edit = (ob->mode & OB_MODE_EDIT) != 0;
	/* confusing logic here, there are 2 methods of setting the color
	 * 'colortab[colindex]' and 'theme_id', colindex overrides theme_id.
	 *
	 * note: no theme yet for 'colindex' */
	int theme_id = is_edit ? TH_WIRE_EDIT : TH_WIRE;

	if (//(scene->obedit == NULL) &&
	    (G.moving & G_TRANSFORM_OBJ) &&
	    (base->flag & BASE_SELECTED))
	{
		theme_id = TH_TRANSFORM;
	}
	else {
		/* Sets the 'theme_id' or fallback to wire */
		if ((ob->flag & OB_FROMGROUP) != 0) {
			if (base->flag & BASE_SELECTED) {
				/* uses darker active color for non-active + selected */
				theme_id = TH_GROUP_ACTIVE;

				// if (scene->basact != base) {
				// 	theme_shade = -16;
				// }
			}
			else {
				theme_id = TH_GROUP;
			}
		}
		else {
			if (base->flag & BASE_SELECTED) {
				theme_id = //scene->basact == base ? TH_ACTIVE :
				TH_SELECT;
			}
			else {
				if (ob->type == OB_LAMP) theme_id = TH_LAMP;
				else if (ob->type == OB_SPEAKER) theme_id = TH_SPEAKER;
				else if (ob->type == OB_CAMERA) theme_id = TH_CAMERA;
				else if (ob->type == OB_EMPTY) theme_id = TH_EMPTY;
				/* fallback to TH_WIRE */
			}
		}
	}

	return theme_id;
}

void DRW_shgroup_non_meshes(DRWPass *UNUSED(non_meshes), ObjectBase *base)
{
	struct Batch *geom;
	DRWShadingGroup *grp;
	Object *ob = base->object;
	int theme_id = draw_object_wire_theme(base);

	switch (ob->type) {
		case OB_LAMP:
		case OB_CAMERA:
		case OB_EMPTY:
		default:
			if (theme_id == TH_ACTIVE)
				grp = empty_active;
			else if (theme_id == TH_SELECT)
				grp = empty_select;
			else if (theme_id == TH_GROUP_ACTIVE)
				grp = empty_group_active;
			else if (theme_id == TH_GROUP)
				grp = empty_group;
			else if (theme_id == TH_TRANSFORM)
				grp = empty_transform;
			else
				grp = empty_wire;

			geom = DRW_cache_plain_axes_get();
			DRW_shgroup_call_add(grp, geom, ob->obmat);
			break;
	}
}

void DRW_shgroup_relationship_lines(DRWPass *UNUSED(non_meshes), ObjectBase *base)
{
	Object *ob = base->object;
	if (ob->parent) {
		struct Batch *geom = DRW_cache_single_vert_get();
		DRW_shgroup_call_add(relationship_lines, geom, ob->obmat);
		DRW_shgroup_call_add(relationship_lines, geom, ob->parent->obmat);
	}
}

/* ***************************** COMMON **************************** */

void DRW_shgroup_object_center(DRWPass *UNUSED(ob_center), ObjectBase *base)
{
	Object *ob = base->object;
	struct Batch *geom = DRW_cache_single_vert_get();

	if (base->flag & BASE_SELECTED)
		DRW_shgroup_call_add(center_selected, geom, ob->obmat);
	else
		DRW_shgroup_call_add(center_deselected, geom, ob->obmat);
}