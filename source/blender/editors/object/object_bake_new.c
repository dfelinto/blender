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
 * The Original Code is Copyright (C) 2004 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_bake.c
 *  \ingroup edobj
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_defs.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_math_geom.h"
#include "BLI_path_util.h"

#include "BKE_blender.h"
#include "BKE_ccg.h"
#include "BKE_screen.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_multires.h"
#include "BKE_report.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_DerivedMesh.h"
#include "BKE_subsurf.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"

#include "RE_engine.h"
#include "RE_pipeline.h"
#include "RE_shader_ext.h"
#include "RE_multires_bake.h"

#include "PIL_time.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "GPU_draw.h" /* GPU_free_image */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "RE_bake.h"

#include "object_intern.h"

/* catch esc */
static int bake_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	/* no running blender, remove handler and pass through */
	if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_RENDER_BAKE))
		return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;

	/* running render */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_RUNNING_MODAL;
			break;
	}
	return OPERATOR_PASS_THROUGH;
}

/* for exec() when there is no render job
 * note: this wont check for the escape key being pressed, but doing so isnt threadsafe */
static int bake_break(void *UNUSED(rjv))
{
	if (G.is_break)
		return 1;
	return 0;
}

static bool write_external_bake_pixels(const char *filepath, BakePixel pixel_array[], float *buffer, const int width, const int height, const int depth, bool is_linear, const int margin)
{
	ImBuf *ibuf = NULL;
	short ok = FALSE;
	unsigned char planes;

	switch (depth) {
		case 1:
			planes = R_IMF_PLANES_BW;
			break;
		case 2:
		case 3:
			planes = R_IMF_PLANES_RGB;
			break;
		case 4:
		default:
			planes = R_IMF_PLANES_RGBA;
			break;
	}

	/* create a new ImBuf */
	ibuf = IMB_allocImBuf(width, height, planes, IB_rect);
	if (!ibuf) return false;

	/* populates the ImBuf */
	IMB_buffer_byte_from_float((unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
	                           (is_linear?IB_PROFILE_LINEAR_RGB:IB_PROFILE_SRGB), IB_PROFILE_LINEAR_RGB,
	                           FALSE, ibuf->x, ibuf->y, ibuf->x, ibuf->x);

	/* setup the Imbuf*/
	ibuf->ftype = PNG;

	/* margins */
	RE_bake_margin(pixel_array, ibuf, margin, width, height);

	if ((ok=IMB_saveiff(ibuf, filepath, IB_rect))) {
#ifndef WIN32
		chmod(filepath, S_IRUSR | S_IWUSR);
#endif
		//printf("%s saving bake map: '%s'\n", __func__, filepath);
	}

	/* garbage collection */
	IMB_freeImBuf(ibuf);

	if (ok) return true;
	return false;
}

static bool is_data_pass(ScenePassType pass_type)
{
	return ELEM7(pass_type,
				 SCE_PASS_Z,
				 SCE_PASS_NORMAL,
				 SCE_PASS_VECTOR,
				 SCE_PASS_INDEXOB,
				 SCE_PASS_UV,
				 SCE_PASS_RAYHITS,
				 SCE_PASS_INDEXMA);
}

static int bake_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	int op_result = OPERATOR_CANCELLED;
	bool ok = false;
	Scene *scene = CTX_data_scene(C);

	/* selected to active (high to low) */
	Object *ob_render = NULL;
	Object *ob_low = CTX_data_active_object(C);
	Object *ob_high = NULL;

	Object *ob_custom_cage = NULL;

	bool restrict_render_low = (ob_low->restrictflag & OB_RESTRICT_RENDER);
	bool restrict_render_high = false;
	bool restrict_render_custom_cage;

	Mesh *me_low = NULL;
	Mesh *me_high = NULL;

	ModifierData *tri_mod;
	int pass_type = RNA_enum_get(op->ptr, "type");

	Render *re = RE_NewRender(scene->id.name);

	float *result;

	BakePixel *pixel_array_low = NULL;
	BakePixel *pixel_array_high = NULL;
	BakePixel *pixel_array_render = NULL;

	const int width = RNA_int_get(op->ptr, "width");
	const int height = RNA_int_get(op->ptr, "height");
	const int num_pixels = width * height;
	const int depth = RE_pass_depth(pass_type);
	const int margin = RNA_int_get(op->ptr, "margin");
	const bool is_external = RNA_boolean_get(op->ptr, "is_save_external");
	const bool is_linear = is_data_pass(pass_type);
	const bool use_selected_to_active = RNA_boolean_get(op->ptr, "use_selected_to_active");
	const float cage_extrusion = RNA_float_get(op->ptr, "cage_extrusion");
	bool is_cage;
	bool is_tangent;

	int normal_space = RNA_enum_get(op->ptr, "normal_space");
	int normal_swizzle[] = {
		RNA_enum_get(op->ptr, "normal_r"),
		RNA_enum_get(op->ptr, "normal_g"),
		RNA_enum_get(op->ptr, "normal_b")
	};

	char custom_cage[MAX_NAME];
	char filepath[FILE_MAX];

	RNA_string_get(op->ptr, "custom_cage", custom_cage);
	RNA_string_get(op->ptr, "filepath", filepath);

	if (use_selected_to_active) {
		CTX_DATA_BEGIN(C, Object *, ob_iter, selected_editable_objects)
		{
			if (ob_iter == ob_low)
				continue;

			ob_high = ob_iter;
			break;
		}
		CTX_DATA_END;

		if (ob_high == NULL) {
			BKE_report(op->reports, RPT_ERROR, "No valid selected object");
			return OPERATOR_CANCELLED;
		}
	}

	if (custom_cage[0] != '\0') {
		ob_custom_cage = (Object *)BLI_findstring(&bmain->object, custom_cage, offsetof(ID, name) + 2);

		if (ob_custom_cage == NULL || ob_custom_cage->type != OB_MESH) {
			BKE_report(op->reports, RPT_ERROR, "No valid custom cage object");
			return OPERATOR_CANCELLED;
		}
		else {
			restrict_render_custom_cage = (ob_custom_cage->restrictflag & OB_RESTRICT_RENDER);
		}
	}

	RE_engine_bake_set_engine_parameters(re, bmain, scene);

	G.is_break = FALSE;

	RE_test_break_cb(re, NULL, bake_break);
	RE_SetReports(re, op->reports);

	pixel_array_low = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels low poly");
	result = MEM_callocN(sizeof(float) * depth * num_pixels, "bake return pixels");

	/**
	 // PSEUDO-CODE TIME

	 // for now ignore the 'bakemap' references below
	 // this is just to help thinking how this will fit
	 // in the overall design and future changes
	 // for the current bake structure just consider
	 // that there is one bakemap made with the current
	 // render selected options

	 for object in selected_objects:
 		BakePixels *pixel_array[];

	 	// doing this at this point means a bakemap won't have
		// a unique UVMap, which may be fine

	 	populate_bake_pixels(object, pixel_array);

	 	for bakemap in object.bakemaps:
	 		float *ret[]

	 		if bakemap.type in (A):
	 			extern_bake(object, pixel_array, len(bp), bakemap.type, ret);
	 		else:
	 			intern_bake(object, pixel_array, len(bp), bakemap.type, ret);

	 		if bakemap.save_external:
	 			write_image(bakemap.filepath, ret);

	 		elif bakemap.save_internal:
	 			bakemap.image(bakemap.image, ret);

	 		elif ... (vertex color?)
	 */

	is_cage = ob_high && (ob_high->type == OB_MESH);
	is_tangent = pass_type == SCE_PASS_NORMAL && normal_space == R_BAKE_SPACE_TANGENT;

	/* high-poly to low-poly baking */
	if (is_cage) {
		ModifierData *md, *nmd;
		TriangulateModifierData *tmd;
		ListBase modifiers_tmp, modifiers_original;

		if (ob_custom_cage) {
			me_low = BKE_mesh_new_from_object(bmain, scene, ob_custom_cage, 1, 2, 1, 0);
		}
		else {
			modifiers_original = ob_low->modifiers;
			BLI_listbase_clear(&modifiers_tmp);

			for (md = ob_low->modifiers.first; md; md = md->next) {
				/* Edge Split cannot be applied in the cage,
				 otherwise we loose interpolated normals */
				if (md->type == eModifierType_EdgeSplit)
					continue;

				nmd = modifier_new(md->type);
				BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));
				modifier_copyData(md, nmd);
				BLI_addtail(&modifiers_tmp, nmd);
			}

			/* temporarily replace the modifiers */
			ob_low->modifiers = modifiers_tmp;

			/* get the cage mesh as it arrives in the renderer */
			me_low = BKE_mesh_new_from_object(bmain, scene, ob_low, 1, 2, 1, 0);
		}

		/* populate the pixel array with the face data */
		RE_populate_bake_pixels(me_low, pixel_array_low, width, height);

		/* triangulating it makes life so much easier ... */
		tri_mod = ED_object_modifier_add(op->reports, bmain, scene, ob_high, "TmpTriangulate", eModifierType_Triangulate);
		tmd = (TriangulateModifierData *)tri_mod;
		tmd->quad_method = MOD_TRIANGULATE_QUAD_FIXED;
		tmd->ngon_method = MOD_TRIANGULATE_NGON_EARCLIP;

		me_high = BKE_mesh_new_from_object(bmain, scene, ob_high, 1, 2, 1, 0);

		if (is_tangent) {
			pixel_array_high = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels high poly");
			RE_populate_bake_pixels_from_object(me_low, me_high, pixel_array_low, pixel_array_high, num_pixels, cage_extrusion);
			pixel_array_render = pixel_array_high;

			/* we need the pixel array to get normals and tangents from the original mesh */
			if (ob_custom_cage) {
				BKE_libblock_free(bmain, me_low);

				me_low = BKE_mesh_new_from_object(bmain, scene, ob_low, 1, 2, 1, 0);
				RE_populate_bake_pixels(me_low, pixel_array_low, width, height);
			}
		}
		else {
			/* re-use the same BakePixel array */
			RE_populate_bake_pixels_from_object(me_low, me_high, pixel_array_low, pixel_array_low, num_pixels, cage_extrusion);
			pixel_array_render = pixel_array_low;
		}

		/* make sure low poly doesn't render, and high poly renders */
		restrict_render_high = (ob_high->restrictflag & OB_RESTRICT_RENDER);
		ob_high->restrictflag &= ~OB_RESTRICT_RENDER;
		ob_low->restrictflag |= OB_RESTRICT_RENDER;
		ob_render = ob_high;

		if (ob_custom_cage) {
			ob_custom_cage->restrictflag |= OB_RESTRICT_RENDER;
		}
		else {
			/* reverting data back */
			ob_low->modifiers = modifiers_original;

			while ((md = BLI_pophead(&modifiers_tmp))) {
				modifier_free(md);
			}
		}
	}
	else {
		/* get the mesh as it arrives in the renderer */
		me_low = BKE_mesh_new_from_object(bmain, scene, ob_low, 1, 2, 1, 0);

		/* populate the pixel array with the face data */
		RE_populate_bake_pixels(me_low, pixel_array_low, width, height);
		pixel_array_render = pixel_array_low;

		/* make sure low poly renders */
		ob_low->restrictflag &= ~OB_RESTRICT_RENDER;
		ob_render = ob_low;
	}

	if (RE_engine_has_bake(re))
		ok = RE_engine_bake(re, ob_render, pixel_array_render, num_pixels, depth, pass_type, result);
	else
		ok = RE_internal_bake(re, ob_render, pixel_array_render, num_pixels, depth, pass_type, result);

	/* normal space conversion */
	if (pass_type == SCE_PASS_NORMAL) {
		switch (normal_space) {
			case R_BAKE_SPACE_WORLD:
			{
				/* Cycles internal format */
				if (normal_swizzle[0] == OB_NEGX &&
					normal_swizzle[1] == OB_NEGY &&
					normal_swizzle[2] == OB_NEGZ)
					break;
				else
					RE_normal_world_to_world(pixel_array_low, num_pixels,  depth, result, normal_swizzle);
				break;
			}
			case R_BAKE_SPACE_OBJECT:
				RE_normal_world_to_object(pixel_array_low, num_pixels, depth, result, ob_low, normal_swizzle);
				break;
			case R_BAKE_SPACE_TANGENT:
				RE_normal_world_to_tangent(pixel_array_low, num_pixels, depth, result, me_low, normal_swizzle);
				break;
			default:
				break;
		}
	}

	if (!ok) {
		BKE_report(op->reports, RPT_ERROR, "Problem baking object map");
		op_result = OPERATOR_CANCELLED;
	}
	else {
		/* save the result */
		if (is_external) {
			BLI_path_abs(filepath, bmain->name);

			/* save it externally */
			ok = write_external_bake_pixels(filepath, pixel_array_render, result, width, height, depth, is_linear, margin);
			if (!ok) {
				char *error = NULL;
				error = BLI_sprintfN("Problem saving baked map in \"%s\".", filepath);

				BKE_report(op->reports, RPT_ERROR, error);
				MEM_freeN(error);
				op_result = OPERATOR_CANCELLED;
			}
			else {
				char *msg = NULL;
				msg = BLI_sprintfN("Baking map written to \"%s\".", filepath);

				BKE_report(op->reports, RPT_INFO, msg);
				MEM_freeN(msg);
				op_result = OPERATOR_FINISHED;
			}
		}
		else {
			/* save it internally */
			BKE_report(op->reports, RPT_ERROR, "Only external baking supported at the moment");
			op_result = OPERATOR_CANCELLED;
		}
	}

	/* restore the restrict render settings */
	if (!restrict_render_low)
		ob_low->restrictflag &= ~OB_RESTRICT_RENDER;
	else
		ob_low->restrictflag |= OB_RESTRICT_RENDER;

	if (ob_high) {
		if (restrict_render_high)
			ob_high->restrictflag |= OB_RESTRICT_RENDER;

		if (tri_mod)
			ED_object_modifier_remove(op->reports, bmain, ob_high, tri_mod);
	}

	if (ob_custom_cage) {
		if (!restrict_render_custom_cage)
			ob_custom_cage->restrictflag &= ~OB_RESTRICT_RENDER;
		else
			ob_custom_cage->restrictflag |= OB_RESTRICT_RENDER;
	}

	RE_SetReports(re, NULL);

	/* garbage collection */
	MEM_freeN(pixel_array_low);

	if (pixel_array_high) {
		MEM_freeN(pixel_array_high);
	}

	MEM_freeN(result);

	if (me_low) {
		BKE_libblock_free(bmain, me_low);
	}

	if (me_high) {
		BKE_libblock_free(bmain, me_high);
	}

#if 0
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int result = OPERATOR_CANCELLED;

	if (is_multires_bake(scene)) {
		result = multiresbake_image_exec_locked(C, op);
	}
	else {
		if (test_bake_internal(C, op->reports) == 0) {
			return OPERATOR_CANCELLED;
		}
		else {
			ListBase threads;
			BakeRender bkr = {NULL};

			init_bake_internal(&bkr, C);
			bkr.reports = op->reports;

			RE_test_break_cb(bkr.re, NULL, thread_break);
			G.is_break = FALSE;   /* blender_test_break uses this global */

			RE_Database_Baking(bkr.re, bmain, scene, scene->lay, scene->r.bake_mode, (scene->r.bake_flag & R_BAKE_TO_ACTIVE) ? OBACT : NULL);

			/* baking itself is threaded, cannot use test_break in threads  */
			BLI_init_threads(&threads, do_bake_render, 1);
			bkr.ready = 0;
			BLI_insert_thread(&threads, &bkr);

			while (bkr.ready == 0) {
				PIL_sleep_ms(50);
				if (bkr.ready)
					break;

				/* used to redraw in 2.4x but this is just for exec in 2.5 */
				if (!G.background)
					blender_test_break();
			}
			BLI_end_threads(&threads);

			if (bkr.result == BAKE_RESULT_NO_OBJECTS)
				BKE_report(op->reports, RPT_ERROR, "No valid images found to bake to");
			else if (bkr.result == BAKE_RESULT_FEEDBACK_LOOP)
				BKE_report(op->reports, RPT_ERROR, "Circular reference in texture stack");

			finish_bake_internal(&bkr);

			result = OPERATOR_FINISHED;
		}
	}

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);
	
	return result;
#endif
	return op_result;
}

static EnumPropertyItem normal_swizzle_items[] = {
	{OB_POSX, "POS_X", 0, "+X", ""},
	{OB_POSY, "POS_Y", 0, "+Y", ""},
	{OB_POSZ, "POS_Z", 0, "+Z", ""},
	{OB_NEGX, "NEG_X", 0, "-X", ""},
	{OB_NEGY, "NEG_Y", 0, "-Y", ""},
	{OB_NEGZ, "NEG_Z", 0, "-Z", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem normal_space_items[] = {
	{R_BAKE_SPACE_WORLD, "WORLD", 0, "World", "Bake the normals in world space"},
	{R_BAKE_SPACE_OBJECT, "OBJECT", 0, "Object", "Bake the normals in object space"},
	{R_BAKE_SPACE_TANGENT, "TANGENT", 0, "Tangent", "Bake the normals in tangent space"},
	{0, NULL, 0, NULL, NULL}
};

void OBJECT_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake";
	ot->description = "Bake image textures of selected objects";
	ot->idname = "OBJECT_OT_bake";

	/* api callbacks */
	ot->exec = bake_exec;
	ot->modal = bake_modal;
	ot->poll = ED_operator_object_active_editable_mesh;

	ot->prop = RNA_def_enum(ot->srna, "type", render_pass_type_items, SCE_PASS_COMBINED, "Type",
	                        "Type of pass to bake, some of them may not be supported by the current render engine");
	ot->prop = RNA_def_boolean(ot->srna, "is_save_external", true, "External", "Save the image externally (ignore face assigned Image datablocks)");
	ot->prop = RNA_def_string_file_path(ot->srna, "filepath", NULL, FILE_MAX, "Path", "Image filepath to use when saving externally");
	ot->prop = RNA_def_int(ot->srna, "width", 512, 1, INT_MAX, "Width", "Horizontal dimension of the baking map", 64, 4096);
	ot->prop = RNA_def_int(ot->srna, "height", 512, 1, INT_MAX, "Height", "Vertical dimension of the baking map", 64, 4096);
	ot->prop = RNA_def_int(ot->srna, "margin", 16, 0, INT_MAX, "Margin", "Extends the baked result as a post process filter", 0, 64);
	ot->prop = RNA_def_boolean(ot->srna, "use_selected_to_active", false, "Selected to Active", "Bake shading on the surface of selected objects to the active object");
	ot->prop = RNA_def_float(ot->srna, "cage_extrusion", 0.0, 0.0, 1.0, "Cage Extrusion", "Distance to use for the inward ray cast when using selected to active", 0.0, 1.0);
	ot->prop = RNA_def_string(ot->srna, "custom_cage", NULL, MAX_NAME, "Custom Cage", "Object to use as custom cage");
	ot->prop = RNA_def_enum(ot->srna, "normal_space", normal_space_items, R_BAKE_SPACE_WORLD, "Normal Space", "Choose normal space for baking");
	ot->prop = RNA_def_enum(ot->srna, "normal_r", normal_swizzle_items, OB_NEGX, "R", "Axis to bake in red channel");
	ot->prop = RNA_def_enum(ot->srna, "normal_g", normal_swizzle_items, OB_NEGY, "G", "Axis to bake in green channel");
	ot->prop = RNA_def_enum(ot->srna, "normal_b", normal_swizzle_items, OB_NEGZ, "B", "Axis to bake in blue channel");
}
