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

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_math_geom.h"

#include "BKE_blender.h"
#include "BKE_ccg.h"
#include "BKE_screen.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
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

static int bake_invoke(bContext *UNUSED(C), wmOperator *op, const wmEvent *UNUSED(_event))
{
	BKE_report(op->reports, RPT_ERROR, "Baking not supported via invoke at the moment");
	return OPERATOR_CANCELLED;
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
	if (!ibuf) return NULL;

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
	Object *object = CTX_data_active_object(C);
	Mesh *me = NULL;

	int pass_type = RNA_enum_get(op->ptr, "type");

	Render *re = RE_NewRender(scene->id.name);

	float *result;
	BakePixel *pixel_array;
	const int width = RNA_int_get(op->ptr, "width");
	const int height = RNA_int_get(op->ptr, "height");
	const int num_pixels = width * height;
	const int depth = RE_pass_depth(pass_type);
	const int margin = RNA_int_get(op->ptr, "margin");
	const bool is_external = RNA_boolean_get(op->ptr, "is_save_external");
	const bool is_linear = is_data_pass(pass_type);
	char filepath[FILE_MAX];
	int need_undeformed = 0;
	RNA_string_get(op->ptr, "filepath", filepath);

	RE_engine_bake_set_engine_parameters(re, bmain, scene);

	G.is_break = FALSE;

	RE_test_break_cb(re, NULL, bake_break);
	RE_SetReports(re, op->reports);

	pixel_array = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels");
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


	/* get the mesh as it arrives in the renderer */
	//XXX Cycles needs that sometimes, will leave it always on for now
	//bool need_undeformed = mesh->need_attribute(scene, ATTR_STD_GENERATED);
	need_undeformed = 1;

	//int apply_modifiers, int settings (1=preview, 2=render), int calc_tessface, int calc_undeformed
	me = BKE_mesh_new_from_object(bmain, scene, object, 1, 2, 1, need_undeformed);
	//TODO delete the mesh afterwards

	/* populate the pixel array with the face data */
	RE_populate_bake_pixels(me, pixel_array, width, height);

	if (RE_engine_has_bake(re))
		ok = RE_engine_bake(re, object, pixel_array, num_pixels, depth, pass_type, result);
	else
		ok = RE_internal_bake(re, object, pixel_array, num_pixels, depth, pass_type, result);

	if (!ok) {
		BKE_report(op->reports, RPT_ERROR, "Problem baking object map");
		op_result = OPERATOR_CANCELLED;
	}
	else {
		/* save the result */
		if (is_external) {
			/* save it externally */
			ok = write_external_bake_pixels(filepath, pixel_array, result, width, height, depth, is_linear, margin);
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

	MEM_freeN(pixel_array);
	MEM_freeN(result);

	RE_SetReports(re, NULL);


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

void OBJECT_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake";
	ot->description = "Bake image textures of selected objects";
	ot->idname = "OBJECT_OT_bake";

	/* api callbacks */
	ot->exec = bake_exec;
	ot->invoke = bake_invoke;
	ot->modal = bake_modal;
	ot->poll = ED_operator_object_active_editable_mesh;

	ot->prop = RNA_def_enum(ot->srna, "type", render_pass_type_items, SCE_PASS_COMBINED, "Type",
	                        "Type of pass to bake, some of them may not be supported by the current render engine");
	ot->prop = RNA_def_boolean(ot->srna, "is_save_external", true, "External", "Save the image externally (ignore face assigned Image datablocks)");
	ot->prop = RNA_def_string_file_path(ot->srna, "filepath", "", FILE_MAX, "Path", "Image filepath to use when saving externally");
	ot->prop = RNA_def_int(ot->srna, "width", 512, 1, INT_MAX, "Width", "Horizontal dimension of the baking map", 64, 4096);
	ot->prop = RNA_def_int(ot->srna, "height", 512, 1, INT_MAX, "Height", "Vertical dimension of the baking map", 64, 4096);
	ot->prop = RNA_def_int(ot->srna, "margin", 16, 0, INT_MAX, "Margin", "Extends the baked result as a post process filter", 0, 64);
}
