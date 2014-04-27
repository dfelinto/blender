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


#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_math_geom.h"
#include "BLI_path_util.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"


#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_uvedit.h"


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
		{
			return OPERATOR_RUNNING_MODAL;
		}
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
        const int width, const int height, const bool is_linear,
        const int margin, const bool is_clear)
{
	ImBuf *ibuf;
	void *lock;
	bool is_float;
	char *mask_buffer = NULL;
	const int num_pixels = width * height;

	ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);

	if (!ibuf)
		return false;

	is_float = (ibuf->flags & IB_rectfloat);

	if (margin > 0 || !is_clear) {
		mask_buffer = MEM_callocN(sizeof(char) * num_pixels, "Bake Mask");
		RE_bake_mask_fill(pixel_array, num_pixels, mask_buffer);
	}

	/* populates the ImBuf */
	if (is_clear) {
		if (is_float) {
			IMB_buffer_float_from_float(
			        ibuf->rect_float, buffer, ibuf->channels,
			        IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, false,
			        ibuf->x, ibuf->y, ibuf->x, ibuf->y);
		}
		else {
			IMB_buffer_byte_from_float(
			        (unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
			        (is_linear ? IB_PROFILE_LINEAR_RGB : IB_PROFILE_SRGB), IB_PROFILE_LINEAR_RGB,
			        false, ibuf->x, ibuf->y, ibuf->x, ibuf->x);
		}
	}
	else {
		if (is_float) {
			IMB_buffer_float_from_float_mask(
			        ibuf->rect_float, buffer, ibuf->channels,
			        IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, false,
			        ibuf->x, ibuf->y, ibuf->x, ibuf->y, mask_buffer);
		}
		else {
			IMB_buffer_byte_from_float_mask(
			        (unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
			        (is_linear ? IB_PROFILE_LINEAR_RGB : IB_PROFILE_SRGB), IB_PROFILE_LINEAR_RGB,
			        false, ibuf->x, ibuf->y, ibuf->x, ibuf->x, mask_buffer);
		}
	}

	/* margins */
	if (margin > 0)
		RE_bake_margin(ibuf, mask_buffer, margin);

	ibuf->userflags |= IB_BITMAPDIRTY;
	BKE_image_release_ibuf(image, ibuf, NULL);

	if (mask_buffer)
		MEM_freeN(mask_buffer);

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
		        ibuf->x, ibuf->y, ibuf->x, ibuf->y);
	}
	else {
		IMB_buffer_byte_from_float(
		        (unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
		        (is_linear ? IB_PROFILE_LINEAR_RGB : IB_PROFILE_SRGB), IB_PROFILE_LINEAR_RGB,
		        false, ibuf->x, ibuf->y, ibuf->x, ibuf->x);
	}

	/* margins */
	if (margin > 0) {
		char *mask_buffer = NULL;
		int num_pixels = width * height;

		mask_buffer = MEM_callocN(sizeof(char) * num_pixels, "Bake Mask");
		RE_bake_mask_fill(pixel_array, num_pixels, mask_buffer);
		RE_bake_margin(ibuf, mask_buffer, margin);

		if (mask_buffer)
			MEM_freeN(mask_buffer);
	}

	if ((ok = BKE_imbuf_write(ibuf, filepath, im_format))) {
#ifndef WIN32
		chmod(filepath, S_IRUSR | S_IWUSR);
#endif
		//printf("%s saving bake map: '%s'\n", __func__, filepath);
	}

	/* garbage collection */
	IMB_freeImBuf(ibuf);

	return ok;
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

static bool build_image_lookup(wmOperator *op, Main *bmain, Object *ob, BakeImages *bake_images)
{
	const int tot_mat = ob->totcol;
	int i, j;
	int tot_images = 0;

	/* error handling and tag (in case multiple materials share the same image) */
	BKE_main_id_tag_idcode(bmain, ID_IM, false);

	for (i = 0; i < tot_mat; i++) {
		Image *image;
		ED_object_get_active_image(ob, i + 1, &image, NULL, NULL);

		if (!image) {
			if (ob->mat[i]) {
				BKE_reportf(op->reports, RPT_ERROR,
				            "No active image found in material %d (%s)", i, ob->mat[i]->id.name + 2);
			}
			else if (((Mesh *) ob->data)->mat[i]) {
				BKE_reportf(op->reports, RPT_ERROR,
				            "No active image found in material %d (%s)", i, ((Mesh *) ob->data)->mat[i]->id.name + 2);
			}
			else {
				BKE_reportf(op->reports, RPT_ERROR,
				            "No active image found in material %d", i);
			}
			return false;
		}

		if ((image->id.flag & LIB_DOIT)) {
			for (j = 0; j < i; j++) {
				if (bake_images->data[j].image == image) {
					bake_images->lookup[i] = j;
					break;
				}
			}
		}
		else {
			bake_images->lookup[i] = tot_images;
			bake_images->data[tot_images].image = image;
			image->id.flag |= LIB_DOIT;
			tot_images++;
		}
	}

	bake_images->size = tot_images;
	return true;
}

/*
 * returns the total number of pixels
 */
static int initialize_internal_images(wmOperator *op, BakeImages *bake_images)
{
	int i;
	int tot_size = 0;

	for (i = 0; i < bake_images->size; i++) {
		ImBuf *ibuf;
		void *lock;

		BakeImage *bk_image = &bake_images->data[i];
		ibuf = BKE_image_acquire_ibuf(bk_image->image, NULL, &lock);

		if (ibuf) {
			int num_pixels = ibuf->x * ibuf->y;

			bk_image->width = ibuf->x;
			bk_image->height = ibuf->y;
			bk_image->offset = tot_size;

			tot_size += num_pixels;
		}
		else {
			BKE_image_release_ibuf(bk_image->image, ibuf, lock);
			BKE_reportf(op->reports, RPT_ERROR, "Not initialized image %s", bk_image->image->id.name + 2);
			return 0;
		}
		BKE_image_release_ibuf(bk_image->image, ibuf, lock);
	}
	return tot_size;
}

static int bake_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	int op_result = OPERATOR_CANCELLED;
	bool ok = false;
	Scene *scene = CTX_data_scene(C);

	Object *ob_low = CTX_data_active_object(C);
	Object *ob_cage = NULL;

	BakeHighPolyData *highpoly;
	int tot_highpoly;

	char restrict_flag_low = ob_low->restrictflag;
	char restrict_flag_cage;

	Mesh *me_low = NULL;

	int pass_type = RNA_enum_get(op->ptr, "type");

	Render *re;

	float *result = NULL;

	BakePixel *pixel_array_low = NULL;

	const int depth = RE_pass_depth(pass_type);
	const int margin = RNA_int_get(op->ptr, "margin");
	const bool is_save_internal = (RNA_enum_get(op->ptr, "save_mode") == R_BAKE_SAVE_INTERNAL);
	const bool is_clear = RNA_boolean_get(op->ptr, "use_clear");
	const bool is_split_materials = (!is_save_internal) && RNA_boolean_get(op->ptr, "use_split_materials");
	const bool is_automatic_name = RNA_boolean_get(op->ptr, "use_automatic_name");
	const bool is_linear = is_data_pass(pass_type);
	const bool use_selected_to_active = RNA_boolean_get(op->ptr, "use_selected_to_active");
	const float cage_extrusion = RNA_float_get(op->ptr, "cage_extrusion");

	bool is_highpoly = false;
	bool is_tangent;

	BakeImages bake_images;

	const int normal_space = RNA_enum_get(op->ptr, "normal_space");
	const BakeNormalSwizzle normal_swizzle[] = {
	    RNA_enum_get(op->ptr, "normal_r"),
	    RNA_enum_get(op->ptr, "normal_g"),
	    RNA_enum_get(op->ptr, "normal_b")
	};

	char custom_cage[MAX_NAME];
	char filepath[FILE_MAX];

	int num_pixels;
	int tot_materials;
	int i;

	re = RE_NewRender(scene->id.name);

	RNA_string_get(op->ptr, "cage", custom_cage);
	RNA_string_get(op->ptr, "filepath", filepath);

	is_tangent = pass_type == SCE_PASS_NORMAL && normal_space == R_BAKE_SPACE_TANGENT;
	tot_materials = ob_low->totcol;

	if (tot_materials == 0) {
		if (is_save_internal) {
			BKE_report(op->reports, RPT_ERROR,
			           "No active image found. Add a material or bake to an external file");

			goto cleanup;
		}
		else if (is_split_materials) {
			BKE_report(op->reports, RPT_ERROR,
			           "No active image found. Add a material or bake without the Split Materials option");

			goto cleanup;
		}
		else {
			/* baking externally without splitting materials */
			tot_materials = 1;
		}
	}

	/* we overallocate in case there is more materials than images */
	bake_images.data = MEM_callocN(sizeof(BakeImage) * tot_materials, "bake images dimensions (width, height, offset)");
	bake_images.lookup = MEM_callocN(sizeof(int) * tot_materials, "bake images lookup (from material to BakeImage)");

	if (!build_image_lookup(op, bmain, ob_low, &bake_images))
		goto cleanup;

	if (is_save_internal) {
		num_pixels = initialize_internal_images(op, &bake_images);

		if (num_pixels == 0) {
			goto cleanup;
		}

		if (is_clear) {
			RE_bake_ibuf_clear(&bake_images, is_tangent);
		}
	}
	else {
		int width, height;

		/* when saving extenally always use the size specified in the UI */
		width = RNA_int_get(op->ptr, "width");
		height = RNA_int_get(op->ptr, "height");

		num_pixels = width * height * bake_images.size;

		for (i = 0; i < bake_images.size; i++) {
			bake_images.data[i].width = width;
			bake_images.data[i].height = height;
			bake_images.data[i].offset = (is_split_materials ? num_pixels : 0);
			bake_images.data[i].image = NULL;

			num_pixels += width * bake_images.data[i].height;
		}

		if (!is_split_materials) {
			/* saving a single image */
			for (i = 0; i < tot_materials; i++)
				bake_images.lookup[i] = 0;
		}
	}

	if (use_selected_to_active) {
		tot_highpoly = 0;

		CTX_DATA_BEGIN(C, Object *, ob_iter, selected_editable_objects)
		{
			if (ob_iter == ob_low)
				continue;

			tot_highpoly ++;
		}
		CTX_DATA_END;

		if (tot_highpoly == 0) {
			BKE_report(op->reports, RPT_ERROR, "No valid selected objects");
			op_result = OPERATOR_CANCELLED;

			goto cleanup;
		}
		else {
			is_highpoly = true;
		}
	}

	if (custom_cage[0] != '\0') {
		ob_cage = BLI_findstring(&bmain->object, custom_cage, offsetof(ID, name) + 2);

		/* TODO check if cage object has the same topology (num of triangles and a valid UV) */
		if (ob_cage == NULL || ob_cage->type != OB_MESH) {
			BKE_report(op->reports, RPT_ERROR, "No valid cage object");
			op_result = OPERATOR_CANCELLED;

			goto cleanup;
		}
		else {
			restrict_flag_cage = ob_cage->restrictflag;
		}
	}

	RE_bake_engine_set_engine_parameters(re, bmain, scene);

	G.is_break = false;

	RE_test_break_cb(re, NULL, bake_break);
	RE_SetReports(re, op->reports);

	pixel_array_low = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels low poly");
	result = MEM_callocN(sizeof(float) * depth * num_pixels, "bake return pixels");

	if (is_highpoly) {
		ModifierData *md, *nmd;
		ListBase modifiers_tmp, modifiers_original;
		float mat_low[4][4];
		int i = 0;
		highpoly = MEM_callocN(sizeof(BakeHighPolyData) * tot_highpoly, "bake high poly objects");

		/* prepare cage mesh */
		if (ob_cage) {
			me_low = BKE_mesh_new_from_object(bmain, scene, ob_cage, 1, 2, 1, 0);
			copy_m4_m4(mat_low, ob_cage->obmat);
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
			copy_m4_m4(mat_low, ob_low->obmat);
		}

		/* populate highpoly array */
		CTX_DATA_BEGIN(C, Object *, ob_iter, selected_editable_objects)
		{
			TriangulateModifierData *tmd;

			if (ob_iter == ob_low)
				continue;

			/* initialize highpoly_data */
			highpoly[i].ob = ob_iter;
			highpoly[i].me = NULL;
			highpoly[i].tri_mod = NULL;
			highpoly[i].restrict_flag = ob_iter->restrictflag;
			highpoly[i].pixel_array = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels high poly");


			/* triangulating so BVH returns the primitive_id that will be used for rendering */
			highpoly[i].tri_mod = ED_object_modifier_add(
			        op->reports, bmain, scene, highpoly[i].ob,
			        "TmpTriangulate", eModifierType_Triangulate);
			tmd = (TriangulateModifierData *)highpoly[i].tri_mod;
			tmd->quad_method = MOD_TRIANGULATE_QUAD_FIXED;
			tmd->ngon_method = MOD_TRIANGULATE_NGON_EARCLIP;

			highpoly[i].me = BKE_mesh_new_from_object(bmain, scene, highpoly[i].ob, 1, 2, 1, 0);
			highpoly[i].ob->restrictflag &= ~OB_RESTRICT_RENDER;

			/* lowpoly to highpoly transformation matrix */
			invert_m4_m4(highpoly[i].mat_lowtohigh, highpoly[i].ob->obmat);
			mul_m4_m4m4(highpoly[i].mat_lowtohigh, highpoly[i].mat_lowtohigh, mat_low);

			i++;
		}
		CTX_DATA_END;

		BLI_assert(i == tot_highpoly);

		/* populate the pixel array with the face data */
		RE_bake_pixels_populate(me_low, pixel_array_low, num_pixels, &bake_images);

		ob_low->restrictflag |= OB_RESTRICT_RENDER;

		/* populate the pixel arrays with the corresponding face data for each high poly object */
		RE_bake_pixels_populate_from_objects(
		        me_low, pixel_array_low, highpoly, tot_highpoly,
		        num_pixels, cage_extrusion);

		/* the baking itself */
		for (i = 0; i < tot_highpoly; i++) {
			if (RE_bake_has_engine(re)) {
				ok = RE_bake_engine(re, highpoly[i].ob, highpoly[i].pixel_array, num_pixels,
				                    depth, pass_type, result);
			}
			else {
				ok = RE_bake_internal(re, highpoly[i].ob, highpoly[i].pixel_array, num_pixels,
				                      depth, pass_type, result);
			}

			if (!ok)
				break;
		}

		/* reverting data back */
		if (ob_cage) {
			ob_cage->restrictflag |= OB_RESTRICT_RENDER;
		}
		else {
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
		RE_bake_pixels_populate(me_low, pixel_array_low, num_pixels, &bake_images);

		/* make sure low poly renders */
		ob_low->restrictflag &= ~OB_RESTRICT_RENDER;

		if (RE_bake_has_engine(re))
			ok = RE_bake_engine(re, ob_low, pixel_array_low, num_pixels, depth, pass_type, result);
		else
			ok = RE_bake_internal(re, ob_low, pixel_array_low, num_pixels, depth, pass_type, result);
	}

	/* normal space conversion */
	if (pass_type == SCE_PASS_NORMAL) {
		switch (normal_space) {
			case R_BAKE_SPACE_WORLD:
			{
				/* Cycles internal format */
				if ((normal_swizzle[0] == R_BAKE_NEGX) &&
				    (normal_swizzle[1] == R_BAKE_NEGY) &&
				    (normal_swizzle[2] == R_BAKE_NEGZ))
				{
					break;
				}
				else {
					RE_bake_normal_world_to_world(pixel_array_low, num_pixels,  depth, result, normal_swizzle);
				}
				break;
			}
			case R_BAKE_SPACE_OBJECT:
			{
				RE_bake_normal_world_to_object(pixel_array_low, num_pixels, depth, result, ob_low, normal_swizzle);
				break;
			}
			case R_BAKE_SPACE_TANGENT:
			{
				if (is_highpoly) {
					RE_bake_normal_world_to_tangent(pixel_array_low, num_pixels, depth, result, me_low, normal_swizzle);
				}
				else {
					/* from multiresolution */
					Mesh *me_nores = NULL;
					ModifierData *md = NULL;
					int mode;

					md = modifiers_findByType(ob_low, eModifierType_Multires);

					if (md) {
						mode = md->mode;
						md->mode &= ~eModifierMode_Render;
					}

					me_nores = BKE_mesh_new_from_object(bmain, scene, ob_low, 1, 2, 1, 0);
					RE_bake_pixels_populate(me_nores, pixel_array_low, num_pixels, &bake_images);

					RE_bake_normal_world_to_tangent(pixel_array_low, num_pixels, depth, result, me_nores, normal_swizzle);
					BKE_libblock_free(bmain, me_nores);

					if (md)
						md->mode = mode;
				}
				break;
			}
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
		for (i = 0; i < bake_images.size; i++) {
			BakeImage *bk_image = &bake_images.data[i];

			if (is_save_internal) {
				ok = write_internal_bake_pixels(
				         bk_image->image,
				         pixel_array_low + bk_image->offset,
				         result + bk_image->offset * depth,
				         bk_image->width, bk_image->height,
				         is_linear, margin, is_clear);

				if (!ok) {
					BKE_report(op->reports, RPT_ERROR,
					           "Problem saving the bake map internally, "
					           "make sure there is a Texture Image node in the current object material");
					op_result = OPERATOR_CANCELLED;
				}
				else {
					BKE_report(op->reports, RPT_INFO,
					           "Baking map saved to internal image, save it externally or pack it");
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
					if (bk_image->image) {
						BLI_path_suffix(name, FILE_MAX, bk_image->image->id.name + 2, "_");
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
				ok = write_external_bake_pixels(
				        name,
				        pixel_array_low + bk_image->offset,
				        result + bk_image->offset * depth,
				        bk_image->width, bk_image->height,
				        is_linear, margin, &bake->im_format);

				if (!ok) {
					BKE_reportf(op->reports, RPT_ERROR, "Problem saving baked map in \"%s\".", name);
					op_result = OPERATOR_CANCELLED;
				}
				else {
					BKE_reportf(op->reports, RPT_INFO, "Baking map written to \"%s\".", name);
					op_result = OPERATOR_FINISHED;
				}

				if (!is_split_materials) {
					break;
				}
			}
		}
	}


cleanup:

	if (is_highpoly) {
		int i;
		for (i = 0; i < tot_highpoly; i++) {
			highpoly[i].ob->restrictflag = highpoly[i].restrict_flag;

			if (highpoly[i].pixel_array)
				MEM_freeN(highpoly[i].pixel_array);

			if (highpoly[i].tri_mod)
				ED_object_modifier_remove(op->reports, bmain, highpoly[i].ob, highpoly[i].tri_mod);

			if (highpoly[i].me)
				BKE_libblock_free(bmain, highpoly[i].me);
		}
		MEM_freeN(highpoly);
	}

	ob_low->restrictflag = restrict_flag_low;

	if (ob_cage)
		ob_cage->restrictflag = restrict_flag_cage;

	if (pixel_array_low)
		MEM_freeN(pixel_array_low);

	if (bake_images.data)
		MEM_freeN(bake_images.data);

	if (bake_images.lookup)
		MEM_freeN(bake_images.lookup);

	if (result)
		MEM_freeN(result);

	if (me_low)
		BKE_libblock_free(bmain, me_low);

	RE_SetReports(re, NULL);

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

	RNA_def_enum(ot->srna, "type", render_pass_type_items, SCE_PASS_COMBINED, "Type",
	             "Type of pass to bake, some of them may not be supported by the current render engine");
	RNA_def_boolean(ot->srna, "is_save_external", true, "External",
	                "Save the image externally (ignore face assigned Image datablocks)");
	RNA_def_string_file_path(ot->srna, "filepath", NULL, FILE_MAX, "File Path",
	                         "Image filepath to use when saving externally");
	RNA_def_int(ot->srna, "width", 512, 1, INT_MAX, "Width",
	            "Horizontal dimension of the baking map (external only)", 64, 4096);
	RNA_def_int(ot->srna, "height", 512, 1, INT_MAX, "Height",
	            "Vertical dimension of the baking map (external only)", 64, 4096);
	RNA_def_int(ot->srna, "margin", 16, 0, INT_MAX, "Margin",
	            "Extends the baked result as a post process filter", 0, 64);
	RNA_def_boolean(ot->srna, "use_selected_to_active", false, "Selected to Active",
	                "Bake shading on the surface of selected objects to the active object");
	RNA_def_float(ot->srna, "cage_extrusion", 0.0, 0.0, 1.0, "Cage Extrusion",
	              "Distance to use for the inward ray cast when using selected to active", 0.0, 1.0);
	RNA_def_string(ot->srna, "cage", NULL, MAX_NAME, "Cage",
	               "Object to use as cage");
	RNA_def_enum(ot->srna, "normal_space", normal_space_items, R_BAKE_SPACE_TANGENT, "Normal Space",
	             "Choose normal space for baking");
	RNA_def_enum(ot->srna, "normal_r", normal_swizzle_items, R_BAKE_POSX, "R", "Axis to bake in red channel");
	RNA_def_enum(ot->srna, "normal_g", normal_swizzle_items, R_BAKE_POSY, "G", "Axis to bake in green channel");
	RNA_def_enum(ot->srna, "normal_b", normal_swizzle_items, R_BAKE_POSZ, "B", "Axis to bake in blue channel");
	RNA_def_enum(ot->srna, "save_mode", bake_save_mode_items, R_BAKE_SAVE_INTERNAL, "Save Mode",
	             "Choose how to save the baking map");
	RNA_def_boolean(ot->srna, "use_clear", false, "Clear",
	                "Clear Images before baking (only for internal saving)");
	RNA_def_boolean(ot->srna, "use_split_materials", false, "Split Materials",
	                "Split baked maps per material, using material name in output file (external only)");
	RNA_def_boolean(ot->srna, "use_automatic_name", false, "Automatic Name",
	                "Automatically name the output file with the pass type");
}
