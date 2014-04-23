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

/** \file blender/editors/object/object_bake_api.c
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
#include "DNA_material_types.h"

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
#include "ED_uvedit.h"

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

static bool write_internal_bake_pixels(
    Image *image, BakePixel pixel_array[], float *buffer,
    const int width, const int height, const bool is_linear, const int margin)
{
	ImBuf *ibuf;
	void *lock;
	bool is_float;

	ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);

	if (!ibuf)
		return false;

	is_float = false;

	/* populates the ImBuf */
	if (is_float) {
		IMB_buffer_float_from_float(
		    ibuf->rect_float, buffer, ibuf->channels,
		    IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, false,
		    ibuf->x, ibuf->y, ibuf->x, ibuf->y
		    );
	}
	else {
		IMB_buffer_byte_from_float(
		    (unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
		    (is_linear ? IB_PROFILE_LINEAR_RGB : IB_PROFILE_SRGB), IB_PROFILE_LINEAR_RGB,
		    false, ibuf->x, ibuf->y, ibuf->x, ibuf->x
		    );
	}

	/* margins */
	RE_bake_margin(pixel_array, ibuf, margin, width, height);

	ibuf->userflags |= IB_BITMAPDIRTY;
	BKE_image_release_ibuf(image, ibuf, NULL);
	return true;
}

static bool write_external_bake_pixels(
    const char *filepath, BakePixel pixel_array[], float *buffer,
    const int width, const int height, bool is_linear, const int margin,
    ImageFormatData *im_format)
{
	ImBuf *ibuf = NULL;
	bool ok = false;
	bool is_float;

	is_float = im_format->depth > 8;

	/* create a new ImBuf */
	ibuf = IMB_allocImBuf(width, height, im_format->planes, (is_float ? IB_rectfloat : IB_rect));
	if (!ibuf)
		return false;

	/* populates the ImBuf */
	if (is_float) {
		IMB_buffer_float_from_float(
		        ibuf->rect_float, buffer, ibuf->channels,
		        IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, false,
		        ibuf->x, ibuf->y, ibuf->x, ibuf->y
		        );
	}
	else {
		IMB_buffer_byte_from_float(
		        (unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
		        (is_linear ? IB_PROFILE_LINEAR_RGB : IB_PROFILE_SRGB), IB_PROFILE_LINEAR_RGB,
		        false, ibuf->x, ibuf->y, ibuf->x, ibuf->x
		        );
	}

	/* margins */
	RE_bake_margin(pixel_array, ibuf, margin, width, height);

	if ((ok = BKE_imbuf_write(ibuf, filepath, im_format))) {
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

static int get_save_internal_status(wmOperator *op, Object *ob, BakeImage *images)
{
	int i;
	int tot_mat = ob->totcol;
	int tot_size = 0;

	for (i = 0; i < tot_mat; i++) {
		ImBuf *ibuf;
		void *lock;
		Image *image;

		ED_object_get_active_image(ob, i + 1, &image, NULL, NULL);

		if (!image) {
			if (ob->mat[i]) {
				BKE_reportf(op->reports, RPT_ERROR,
				            "No valid image in material %d (%s)", i, ob->mat[i]->id.name + 2);
			}
			else if (((Mesh *) ob->data)->mat[i]) {
				BKE_reportf(op->reports, RPT_ERROR,
				            "No valid image in material %d (%s)", i, ((Mesh *) ob->data)->mat[i]->id.name + 2);
			}
			else {
				BKE_reportf(op->reports, RPT_ERROR,
				            "No valid image in material %d", i);
			}

			return -1;
		}

		ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);

		if (ibuf) {
			int num_pixels = ibuf->x * ibuf->y;

			images[i].width = ibuf->x;
			images[i].height = ibuf->y;
			images[i].offset = tot_size;
			images[i].image = image;

			tot_size += num_pixels;
		}
		else {
			BKE_image_release_ibuf(image, ibuf, lock);
			BKE_reportf(op->reports, RPT_ERROR, "Not inialized image in material '%d' (%s)", i, ob->mat[i]->id.name + 2);
			return -1;
		}

		BKE_image_release_ibuf(image, ibuf, lock);
	}

	return tot_size;
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

	char restrict_flag_low = ob_low->restrictflag;
	char restrict_flag_high;
	char restrict_flag_cage;

	Mesh *me_low = NULL;
	Mesh *me_high = NULL;

	ModifierData *tri_mod;
	int pass_type = RNA_enum_get(op->ptr, "type");

	Render *re = RE_NewRender(scene->id.name);

	float *result = NULL;

	BakePixel *pixel_array_low = NULL;
	BakePixel *pixel_array_high = NULL;
	BakePixel *pixel_array_render = NULL;

	const int depth = RE_pass_depth(pass_type);
	const int margin = RNA_int_get(op->ptr, "margin");
	const bool is_save_internal = (RNA_enum_get(op->ptr, "save_mode") == R_BAKE_SAVE_INTERNAL);
	const bool is_clear = RNA_boolean_get(op->ptr, "use_clear");
	const bool is_split_materials = (!is_save_internal) && RNA_boolean_get(op->ptr, "use_split_materials");
	const bool is_automatic_name = RNA_boolean_get(op->ptr, "use_automatic_name");
	const bool is_linear = is_data_pass(pass_type);
	const bool use_selected_to_active = RNA_boolean_get(op->ptr, "use_selected_to_active");
	const float cage_extrusion = RNA_float_get(op->ptr, "cage_extrusion");

	bool is_cage;
	bool is_tangent;

	BakeImage *images;

	int normal_space = RNA_enum_get(op->ptr, "normal_space");
	BakeNormalSwizzle normal_swizzle[] = {
		RNA_enum_get(op->ptr, "normal_r"),
		RNA_enum_get(op->ptr, "normal_g"),
		RNA_enum_get(op->ptr, "normal_b")
	};

	char custom_cage[MAX_NAME];
	char filepath[FILE_MAX];

	int num_pixels;
	int tot_images;
	int i;

	RNA_string_get(op->ptr, "cage", custom_cage);
	RNA_string_get(op->ptr, "filepath", filepath);

	tot_images = ob_low->totcol;
	is_tangent = pass_type == SCE_PASS_NORMAL && normal_space == R_BAKE_SPACE_TANGENT;

	images = MEM_callocN(sizeof(BakeImage) * tot_images, "bake images dimensions (width, height, offset)");

	if (is_save_internal) {
		num_pixels = get_save_internal_status(op, ob_low, images);

		if (num_pixels < 0) {
			goto cleanup;
		}
		else if (is_clear) {
			RE_bake_ibuf_clear(images, tot_images, is_tangent);
		}
	}
	else {
		num_pixels = 0;
		for (i = 0; i < tot_images; i++) {
			images[i].width = RNA_int_get(op->ptr, "width");
			images[i].height = RNA_int_get(op->ptr, "height");
			images[i].offset = (is_split_materials ? num_pixels : 0);
			images[i].image = NULL;

			num_pixels += images[i].width * images[i].height;
		}

		BLI_assert(num_pixels == tot_images * images[0].width * images[0].height);
	}

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
			op_result = OPERATOR_CANCELLED;
			goto cleanup;
		}
		else {
			restrict_flag_high = ob_high->restrictflag;
		}
	}

	if (custom_cage[0] != '\0') {
		ob_custom_cage = BLI_findstring(&bmain->object, custom_cage, offsetof(ID, name) + 2);

		/* TODO check if cage object has the same topology (num of triangles and a valid UV) */
		if (ob_custom_cage == NULL || ob_custom_cage->type != OB_MESH) {
			BKE_report(op->reports, RPT_ERROR, "No valid cage object");
			op_result = OPERATOR_CANCELLED;
			goto cleanup;
		}
		else {
			restrict_flag_cage = ob_custom_cage->restrictflag;
		}
	}

	RE_engine_bake_set_engine_parameters(re, bmain, scene);

	G.is_break = false;

	RE_test_break_cb(re, NULL, bake_break);
	RE_SetReports(re, op->reports);

	pixel_array_low = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels low poly");
	result = MEM_callocN(sizeof(float) * depth * num_pixels, "bake return pixels");

	is_cage = ob_high && (ob_high->type == OB_MESH);

	/* high-poly to low-poly baking */
	if (is_cage) {
		ModifierData *md, *nmd;
		TriangulateModifierData *tmd;
		ListBase modifiers_tmp, modifiers_original;
		float mat_low2high[4][4];

		if (ob_custom_cage) {
			me_low = BKE_mesh_new_from_object(bmain, scene, ob_custom_cage, 1, 2, 1, 0);

			invert_m4_m4(mat_low2high, ob_high->obmat);
			mul_m4_m4m4(mat_low2high, mat_low2high, ob_custom_cage->obmat);
		}
		else {
			modifiers_original = ob_low->modifiers;
			BLI_listbase_clear(&modifiers_tmp);

			for (md = ob_low->modifiers.first; md; md = md->next) {
				/* Edge Split cannot be applied in the cage,
				 * the cage is supposed to have interpolated normals
				 * between the faces unless the geometry is physically
				 * split. So we create a copy of the low poly mesh without
				 * the eventual edge split.*/

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

			invert_m4_m4(mat_low2high, ob_high->obmat);
			mul_m4_m4m4(mat_low2high, mat_low2high, ob_low->obmat);
		}

		/* populate the pixel array with the face data */
		RE_populate_bake_pixels(me_low, pixel_array_low, num_pixels, images);

		/* triangulating it makes life so much easier ... */
		tri_mod = ED_object_modifier_add(op->reports, bmain, scene, ob_high, "TmpTriangulate", eModifierType_Triangulate);
		tmd = (TriangulateModifierData *)tri_mod;
		tmd->quad_method = MOD_TRIANGULATE_QUAD_FIXED;
		tmd->ngon_method = MOD_TRIANGULATE_NGON_EARCLIP;

		me_high = BKE_mesh_new_from_object(bmain, scene, ob_high, 1, 2, 1, 0);

		if (is_tangent) {
			pixel_array_high = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels high poly");
			RE_populate_bake_pixels_from_object(me_low, me_high, pixel_array_low, pixel_array_high, num_pixels, cage_extrusion, mat_low2high);
			pixel_array_render = pixel_array_high;

			/* we need the pixel array to get normals and tangents from the original mesh */
			if (ob_custom_cage) {
				BKE_libblock_free(bmain, me_low);

				me_low = BKE_mesh_new_from_object(bmain, scene, ob_low, 1, 2, 1, 0);
				RE_populate_bake_pixels(me_low, pixel_array_low, num_pixels, images);
			}

			/* need to make sure missed rays are masked out of the result */
			RE_mask_bake_pixels(pixel_array_high, pixel_array_low, num_pixels);
		}
		else {
			/* re-use the same BakePixel array */
			RE_populate_bake_pixels_from_object(me_low, me_high, pixel_array_low, pixel_array_low, num_pixels, cage_extrusion, mat_low2high);
			pixel_array_render = pixel_array_low;
		}

		/* make sure low poly doesn't render, and high poly renders */
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
		RE_populate_bake_pixels(me_low, pixel_array_low, num_pixels, images);
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
				if (normal_swizzle[0] == R_BAKE_NEGX &&
				    normal_swizzle[1] == R_BAKE_NEGY &&
				    normal_swizzle[2] == R_BAKE_NEGZ)
				{
					break;
				}
				else {
					RE_normal_world_to_world(pixel_array_low, num_pixels,  depth, result, normal_swizzle);
				}
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
		/* save the results */
		for (i = 0; i < tot_images; i++) {
			BakeImage *image = &images[i];

			if (is_save_internal) {
				ok = write_internal_bake_pixels(image->image, pixel_array_render + image->offset, result + image->offset * depth, image->width, image->height, is_linear, margin);

				if (!ok) {
					BKE_report(op->reports, RPT_ERROR, "Problem saving the bake map internally, make sure there is a Texture Image node in the current object material");
					op_result = OPERATOR_CANCELLED;
				}
				else {
					BKE_report(op->reports, RPT_INFO, "Baking map saved to internal image, save it externally or pack it");
					op_result = OPERATOR_FINISHED;
				}
			}
			/* save externally */
			else {
				BakeData *bake = &scene->r.bake;
				char name[FILE_MAX];

				BKE_makepicstring_from_type(name, filepath, bmain->name, 0, bake->im_format.imtype, true, false);

				if (is_automatic_name) {
					const char *identifier;
					PropertyRNA *prop = RNA_struct_find_property(op->ptr, "type");

					RNA_property_enum_identifier(C, op->ptr, prop, pass_type, &identifier);
					BLI_path_suffix(name, FILE_MAX, identifier, "_");
				}

				if (is_split_materials) {
					if (image->image) {
						BLI_path_suffix(name, FILE_MAX, image->image->id.name + 2, "_");
					}
					else {
						if (ob_low->mat[i]) {
							BLI_path_suffix(name, FILE_MAX, ob_low->mat[i]->id.name + 2, "_");
						}
						else if (me_low->mat[i]) {
							BLI_path_suffix(name, FILE_MAX, me_low->mat[i]->id.name + 2, "_");
						}
						else {
							/* if everything else fails, use the material index */
							char tmp[4];
							sprintf(tmp, "%d", i % 1000);
							BLI_path_suffix(name, FILE_MAX, tmp, "_");
						}
					}
				}

				/* save it externally */
				ok = write_external_bake_pixels(name, pixel_array_render + image->offset, result + image->offset * depth, image->width, image->height, is_linear, margin, &bake->im_format);

				if (!ok) {
					BKE_reportf(op->reports, RPT_ERROR, "Problem saving baked map in \"%s\".", name);
					op_result = OPERATOR_CANCELLED;
				}
				else {
					BKE_reportf(op->reports, RPT_INFO, "Baking map written to \"%s\".", name);
					op_result = OPERATOR_FINISHED;
				}

				/**/
				if (!is_split_materials) {
					break;
				}
			}
		}
	}

cleanup:

	/* restore the restrict render settings */
	ob_low->restrictflag = restrict_flag_low;

	if (ob_high) {
		ob_high->restrictflag = restrict_flag_high;

		if (tri_mod)
			ED_object_modifier_remove(op->reports, bmain, ob_high, tri_mod);
	}

	if (ob_custom_cage)
		ob_custom_cage->restrictflag = restrict_flag_cage;

	RE_SetReports(re, NULL);

	/* garbage collection */
	if (pixel_array_low)
		MEM_freeN(pixel_array_low);

	if (pixel_array_high)
		MEM_freeN(pixel_array_high);

	if (images)
		MEM_freeN(images);

	if (result)
		MEM_freeN(result);

	if (me_low)
		BKE_libblock_free(bmain, me_low);

	if (me_high)
		BKE_libblock_free(bmain, me_high);

	return op_result;
}

static int bake_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	PropertyRNA *prop;
	Scene *scene = CTX_data_scene(C);
	BakeData *bake = &scene->r.bake;

	prop = RNA_struct_find_property(op->ptr, "filepath");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_string_set(op->ptr, prop, bake->filepath);
	}

	prop =  RNA_struct_find_property(op->ptr, "width");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_int_set(op->ptr, prop, bake->width);
	}

	prop =  RNA_struct_find_property(op->ptr, "height");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_int_set(op->ptr, prop, bake->width);
	}

	prop = RNA_struct_find_property(op->ptr, "margin");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_int_set(op->ptr, prop, bake->margin);
	}

	prop = RNA_struct_find_property(op->ptr, "use_selected_to_active");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_TO_ACTIVE));
	}

	prop = RNA_struct_find_property(op->ptr, "cage_extrusion");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set(op->ptr, prop, bake->cage_extrusion);
	}

	prop = RNA_struct_find_property(op->ptr, "cage");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_string_set(op->ptr, prop, bake->cage);
	}

	prop = RNA_struct_find_property(op->ptr, "normal_space");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->normal_space);
	}

	prop = RNA_struct_find_property(op->ptr, "normal_r");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->normal_swizzle[0]);
	}

	prop = RNA_struct_find_property(op->ptr, "normal_g");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->normal_swizzle[1]);
	}

	prop = RNA_struct_find_property(op->ptr, "normal_b");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->normal_swizzle[2]);
	}

	prop = RNA_struct_find_property(op->ptr, "save_mode");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->save_mode);
	}

	prop = RNA_struct_find_property(op->ptr, "use_clear");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_CLEAR));
	}

	prop = RNA_struct_find_property(op->ptr, "use_split_materials");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_SPLIT_MAT));
	}

	prop = RNA_struct_find_property(op->ptr, "use_automatic_name");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_AUTO_NAME));
	}

	return bake_exec(C, op);
}

void OBJECT_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake";
	ot->description = "Bake image textures of selected objects";
	ot->idname = "OBJECT_OT_bake";

	/* api callbacks */
	ot->exec = bake_exec;
	ot->modal = bake_modal;
	ot->invoke = bake_invoke;
	ot->poll = ED_operator_object_active_editable_mesh;

	ot->prop = RNA_def_enum(ot->srna, "type", render_pass_type_items, SCE_PASS_COMBINED, "Type",
	                        "Type of pass to bake, some of them may not be supported by the current render engine");
	ot->prop = RNA_def_boolean(ot->srna, "is_save_external", true, "External", "Save the image externally (ignore face assigned Image datablocks)");
	ot->prop = RNA_def_string_file_path(ot->srna, "filepath", NULL, FILE_MAX, "File Path", "Image filepath to use when saving externally");
	ot->prop = RNA_def_int(ot->srna, "width", 512, 1, INT_MAX, "Width", "Horizontal dimension of the baking map (external only)", 64, 4096);
	ot->prop = RNA_def_int(ot->srna, "height", 512, 1, INT_MAX, "Height", "Vertical dimension of the baking map (external only)", 64, 4096);
	ot->prop = RNA_def_int(ot->srna, "margin", 16, 0, INT_MAX, "Margin", "Extends the baked result as a post process filter", 0, 64);
	ot->prop = RNA_def_boolean(ot->srna, "use_selected_to_active", false, "Selected to Active", "Bake shading on the surface of selected objects to the active object");
	ot->prop = RNA_def_float(ot->srna, "cage_extrusion", 0.0, 0.0, 1.0, "Cage Extrusion", "Distance to use for the inward ray cast when using selected to active", 0.0, 1.0);
	ot->prop = RNA_def_string(ot->srna, "cage", NULL, MAX_NAME, "Cage", "Object to use as cage");
	ot->prop = RNA_def_enum(ot->srna, "normal_space", normal_space_items, R_BAKE_SPACE_TANGENT, "Normal Space", "Choose normal space for baking");
	ot->prop = RNA_def_enum(ot->srna, "normal_r", normal_swizzle_items, R_BAKE_POSX, "R", "Axis to bake in red channel");
	ot->prop = RNA_def_enum(ot->srna, "normal_g", normal_swizzle_items, R_BAKE_POSY, "G", "Axis to bake in green channel");
	ot->prop = RNA_def_enum(ot->srna, "normal_b", normal_swizzle_items, R_BAKE_POSZ, "B", "Axis to bake in blue channel");
	ot->prop = RNA_def_enum(ot->srna, "save_mode", bake_save_mode_items, R_BAKE_SAVE_EXTERNAL, "Save Mode", "Choose how to save the baking map");
	ot->prop = RNA_def_boolean(ot->srna, "use_clear", false, "Clear", "Clear Images before baking (only for internal saving)");
	ot->prop = RNA_def_boolean(ot->srna, "use_split_materials", false, "Split Materials", "Split baked maps per material, using material name in output file (external only)");
	ot->prop = RNA_def_boolean(ot->srna, "use_automatic_name", false, "Automatic Name", "Automatically name the output file with the pass type");
}
