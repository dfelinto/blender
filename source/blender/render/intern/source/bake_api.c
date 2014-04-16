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
 * Contributors:
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/bake_api.c
 *  \ingroup render
 */


/* system includes */
#include <stdio.h>
#include <string.h>

/* External modules: */
#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_library.h"

#include "BKE_bvhutils.h"
#include "BKE_DerivedMesh.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "RE_bake.h"

/* local include */
#include "rayintersection.h"
#include "rayobject.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "shading.h"
#include "zbuf.h"

#include "PIL_time.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


typedef struct BakeDataZSpan {
	BakePixel *pixel_array;
	int primitive_id;
	int mat_nr;
	const BakeImage *images;
	ZSpan *zspan;
} BakeDataZSpan;

/*
 * struct wrapping up tangent space data
 */
typedef struct TSpace {
	float tangent[3];
	float sign;
} TSpace;

typedef struct TriTessFace {
	MVert *v1;
	MVert *v2;
	MVert *v3;
	TSpace *tspace[3];
	float normal[3]; /* for flat faces */
	bool smoothnormal;
} TriTessFace;

/* ************************* bake ************************ */
static void store_bake_pixel(void *handle, int x, int y, float u, float v)
{
	BakeDataZSpan *bd = (BakeDataZSpan *)handle;
	BakePixel *pixel;
	const int mat_nr = bd->mat_nr;
	const int width = bd->images[mat_nr].width;
	const int offset = bd->images[mat_nr].offset;
	const int i = offset + y * width + x;

	pixel = &bd->pixel_array[i];
	pixel->primitive_id = bd->primitive_id;

	copy_v2_fl2(pixel->uv, u, v);

	pixel->du_dx =
	pixel->du_dy =
	pixel->dv_dx =
	pixel->dv_dy =
	0.0f;
}

void RE_bake_margin(const BakePixel pixel_array[], ImBuf *ibuf, const int margin, const int width, const int height)
{
	char *mask_buffer = NULL;
	const int num_pixels = width * height;
	int i;

	if (margin < 1)
		return;

	mask_buffer = MEM_callocN(sizeof(char) * num_pixels, "BakeMask");

	/* only extend to pixels outside the mask area */
	for (i = 0; i < num_pixels; i++) {
		if (pixel_array[i].primitive_id != -1) {
			mask_buffer[i] = FILTER_MASK_USED;
		}
	}

	/* margin */
	IMB_filter_extend(ibuf, mask_buffer, margin);

	if (ibuf->planes != R_IMF_PLANES_RGBA)
		/* clear alpha added by filtering */
		IMB_rectfill_alpha(ibuf, 1.0f);

	MEM_freeN(mask_buffer);
}

/*
 * This function returns the coordinate and normal of a barycentric u,v for a face defined by the primitive_id index.
 */

static void get_point_from_barycentric(TriTessFace *triangles, int primitive_id, float u, float v, float cage_extrusion, float r_co[3], float r_dir[3])
{
	float data[3][3];
	float coord[3];
	float dir[3];
	float cage[3];

	TriTessFace *triangle = &triangles[primitive_id];

	copy_v3_v3(data[0], triangle->v1->co);
	copy_v3_v3(data[1], triangle->v2->co);
	copy_v3_v3(data[2], triangle->v3->co);

	interp_barycentric_tri_v3(data, u, v, coord);

	normal_short_to_float_v3(data[0], triangle->v1->no);
	normal_short_to_float_v3(data[1], triangle->v2->no);
	normal_short_to_float_v3(data[2], triangle->v3->no);

	interp_barycentric_tri_v3(data, u, v, dir);
	normalize_v3_v3(cage, dir);
	mul_v3_fl(cage, cage_extrusion);

	add_v3_v3(coord, cage);

	normalize_v3_v3(dir, dir);
	mul_v3_fl(dir, -1.0f);

	copy_v3_v3(r_co, coord);
	copy_v3_v3(r_dir, dir);
}

/*
 * This function returns the barycentric u,v of a face for a coordinate. The face is defined by its index.
 */
static void get_barycentric_from_point(TriTessFace *triangles, const int index, const float co[3], int *primitive_id, float uv[2])
{
	TriTessFace *triangle = &triangles[index];
	resolve_tri_uv_v3(uv, co, triangle->v1->co, triangle->v2->co, triangle->v3->co);
	*primitive_id = index;
}

/*
 * This function populates pixel_array and returns TRUE if things are correct
 */
static bool cast_ray_highpoly(BVHTreeFromMesh *treeData, TriTessFace *triangles, BakePixel *pixel_array, float co[3], float dir[3])
{
	int primitive_id;
	float uv[2];

	BVHTreeRayHit hit;
	hit.index = -1;
	hit.dist = 10000.0f; /* TODO: we should use FLT_MAX here, but sweepsphere code isn't prepared for that */

	/* cast ray */
	BLI_bvhtree_ray_cast(treeData->tree, co, dir, 0.0f, &hit, treeData->raycast_callback, treeData);

	if (hit.index != -1) {
		/* cull backface */
		const float dot = dot_v3v3(dir, hit.no);
		if (dot >= 0.0f) {
			pixel_array->primitive_id = -1;
			return false;
		}

		get_barycentric_from_point(triangles, hit.index, hit.co, &primitive_id, uv);
		pixel_array->primitive_id = primitive_id;
		copy_v2_v2(pixel_array->uv, uv);

		return true;
	}

	pixel_array->primitive_id = -1;
	return false;
}

/*
 * This function populates an array of verts for the triangles of a mesh
 * Tangent and Normals are also stored
 */
static void calculateTriTessFace(TriTessFace *triangles, Mesh *me, int (*lookup_id)[2], bool tangent, DerivedMesh *dm)
{
	int i;
	int p_id;
	MFace *mface;
	MVert *mvert;
	TSpace *tspace;
	float *precomputed_normals;
	bool calculate_normal;

	mface = CustomData_get_layer(&me->fdata, CD_MFACE);
	mvert = CustomData_get_layer(&me->vdata, CD_MVERT);

	if (tangent) {
		DM_ensure_normals(dm);
		DM_add_tangent_layer(dm);

		precomputed_normals = dm->getTessFaceDataArray(dm, CD_NORMAL);
		calculate_normal = precomputed_normals ? false : true;

		//mface = dm->getTessFaceArray(dm);
		//mvert = dm->getVertArray(dm);

		tspace = dm->getTessFaceDataArray(dm, CD_TANGENT);
		BLI_assert(tspace);
	}

	p_id = -1;
	for (i = 0; i < me->totface; i++) {
		MFace *mf = &mface[i];
		TSpace *ts = &tspace[i * 4];

		++p_id;

		if (lookup_id)
			lookup_id[i][0] = p_id;

		triangles[p_id].v1 = &mvert[mf->v1];
		triangles[p_id].v2 = &mvert[mf->v2];
		triangles[p_id].v3 = &mvert[mf->v3];
		triangles[p_id].smoothnormal = (mf->flag & ME_SMOOTH) != 0;

		if (tangent) {
			triangles[p_id].tspace[0] = &ts[0];
			triangles[p_id].tspace[1] = &ts[1];
			triangles[p_id].tspace[2] = &ts[2];

			if (calculate_normal) {
				if (mf->v4 != 0) {
					normal_quad_v3(triangles[p_id].normal,
					               mvert[mf->v1].co,
					               mvert[mf->v2].co,
					               mvert[mf->v3].co,
					               mvert[mf->v4].co);
				}
				else {
					normal_tri_v3(triangles[p_id].normal,
					              triangles[p_id].v1->co,
					              triangles[p_id].v2->co,
					              triangles[p_id].v3->co);
				}
			}
			else {
				copy_v3_v3(triangles[p_id].normal, &precomputed_normals[3 * i]);
			}
		}

		/* 4 vertices in the face */
		if (mf->v4 != 0) {
			++p_id;

			if (lookup_id)
				lookup_id[i][1] = p_id;

			triangles[p_id].v1 = &mvert[mf->v1];
			triangles[p_id].v2 = &mvert[mf->v3];
			triangles[p_id].v3 = &mvert[mf->v4];
			triangles[p_id].smoothnormal = (mf->flag & ME_SMOOTH);

			if (tangent) {
				triangles[p_id].tspace[0] = &ts[0];
				triangles[p_id].tspace[1] = &ts[2];
				triangles[p_id].tspace[2] = &ts[3];

				/* same normal as the other "triangle" */
				copy_v3_v3(triangles[p_id].normal, triangles[p_id - 1].normal);
			}
		}
		else {
			if (lookup_id)
				lookup_id[i][1] = -1;
		}
	}

	BLI_assert(p_id < me->totface * 2);
}

void RE_populate_bake_pixels_from_object(Mesh *me_low, Mesh *me_high,
                                         const BakePixel pixel_array_from[], BakePixel pixel_array_to[],
                                         const int num_pixels, const float cage_extrusion, float mat_low2high[4][4])
{
	int i;
	int primitive_id;
	float u, v;

	DerivedMesh *dm_high;
	BVHTreeFromMesh treeData = {NULL,};

	/* Note: all coordinates are in local space */
	TriTessFace *tris_low;
	TriTessFace *tris_high;

	/* assume all tessfaces can be quads */
	tris_low = MEM_callocN(sizeof(TriTessFace) * (me_low->totface * 2), "MVerts Lowpoly Mesh");
	tris_high = MEM_callocN(sizeof(TriTessFace) * (me_high->totface * 2), "MVerts Highpoly Mesh");

	calculateTriTessFace(tris_low, me_low, NULL, false, NULL);
	calculateTriTessFace(tris_high, me_high, NULL, false, NULL);

	dm_high = CDDM_from_mesh(me_high);

	/* Create a bvh-tree of the given target */
	bvhtree_from_mesh_faces(&treeData, dm_high, 0.0, 2, 6);
	if (treeData.tree == NULL) {
		printf("Baking: Out of memory\n");
		goto cleanup;
	}

	for (i=0; i < num_pixels; i++) {
		float co[3];
		float dir[3];

		primitive_id = pixel_array_from[i].primitive_id;

		if (primitive_id == -1) {
			pixel_array_to[i].primitive_id = -1;
			continue;
		}

		u = pixel_array_from[i].uv[0];
		v = pixel_array_from[i].uv[1];

		/* calculate from low poly mesh cage */
		get_point_from_barycentric(tris_low, primitive_id, u, v, cage_extrusion, co, dir);

		/* transform the ray from the low poly to the high poly space */
		mul_m4_v3(mat_low2high, co);

		/* cast ray */
		cast_ray_highpoly(&treeData, tris_high, &pixel_array_to[i], co, dir);
	}

	/* garbage collection */
cleanup:
	free_bvhtree_from_mesh(&treeData);

	dm_high->release(dm_high);

	MEM_freeN(tris_low);
	MEM_freeN(tris_high);
}

void RE_mask_bake_pixels(const BakePixel pixel_array_from[], BakePixel pixel_array_to[], const int num_pixels)
{
	int i;

	for (i = 0; i < num_pixels; i++) {
		if (pixel_array_from[i].primitive_id == -1)
			pixel_array_to[i].primitive_id = -1;
	}
}

void RE_populate_bake_pixels(Mesh *me, BakePixel pixel_array[], const int num_pixels, const BakeImage *images)
{
	BakeDataZSpan bd;
	const int tot_mat = me->totcol;
	int i, a;
	int p_id;

	MTFace *mtface;
	MFace *mface;

	/* we can't bake in edit mode */
	if (me->edit_btmesh)
		return;

	bd.pixel_array = pixel_array;
	bd.images = images;
	bd.zspan = MEM_callocN(sizeof(ZSpan) * tot_mat, "bake zspan");

	/* initialize all pixel arrays so we know which ones are 'blank' */
	for (i = 0; i < num_pixels; i++) {
		pixel_array[i].primitive_id = -1;
	}

	for (i = 0; i < tot_mat; i++) {
		zbuf_alloc_span(&bd.zspan[i], images[i].width, images[i].height, R.clipcrop);
	}

	mtface = CustomData_get_layer(&me->fdata, CD_MTFACE);
	mface = CustomData_get_layer(&me->fdata, CD_MFACE);

	if (mtface == NULL)
		return;

	p_id = -1;
	for (i = 0; i < me->totface; i++) {
		float vec[4][2];
		MTFace *mtf = &mtface[i];
		MFace *mf = &mface[i];
		int mat_nr = mf->mat_nr;

		bd.mat_nr = mat_nr;
		bd.primitive_id = ++p_id;

		for (a = 0; a < 4; a++) {
			/* Note, workaround for pixel aligned UVs which are common and can screw up our intersection tests
			 * where a pixel gets in between 2 faces or the middle of a quad,
			 * camera aligned quads also have this problem but they are less common.
			 * Add a small offset to the UVs, fixes bug #18685 - Campbell */
			vec[a][0] = mtf->uv[a][0] * (float)images[mat_nr].width - (0.5f + 0.001f);
			vec[a][1] = mtf->uv[a][1] * (float)images[mat_nr].height - (0.5f + 0.002f);
		}

		zspan_scanconvert(&bd.zspan[mat_nr], (void *)&bd, vec[0], vec[1], vec[2], store_bake_pixel);

		/* 4 vertices in the face */
		if (mf->v4 != 0) {
			bd.primitive_id = ++p_id;
			zspan_scanconvert(&bd.zspan[mat_nr], (void *)&bd, vec[0], vec[2], vec[3], store_bake_pixel);
		}
	}

	for (i = 0; i < tot_mat; i++) {
		zbuf_free_span(&bd.zspan[i]);
	}
	MEM_freeN(bd.zspan);
}

/* ******************** NORMALS ************************ */

/* convert a normalized normal to the -1.0 1.0 range
 * the input is expected to be NEG_X, NEG_Y, NEG_Z
 * the output is POS_X, POS_Y, POS_Z
 */
static void normal_uncompress(float out[3], const float in[3])
{
	int i;
	for (i = 0; i < 3; i++)
		out[i] = - (2.0f * in[i] - 1.0f);
}

static void normal_compress(float out[3], const float in[3], const BakeNormalSwizzle normal_swizzle[3])
{
	const int swizzle_index[6] =
	{
		0, // R_BAKE_POSX
		1, // R_BAKE_POSY
		2, // R_BAKE_POSZ
		0, // R_BAKE_NEGX
		1, // R_BAKE_NEGY
		2, // R_BAKE_NEGZ
	};
	const float swizzle_sign[6] =
	{
		 1.0f, // R_BAKE_POSX
		 1.0f, // R_BAKE_POSY
		 1.0f, // R_BAKE_POSZ
		-1.0f, // R_BAKE_NEGX
		-1.0f, // R_BAKE_NEGY
		-1.0f, // R_BAKE_NEGZ
	};

	int i;

	for (i = 0; i < 3; i++) {
		int index;
		float sign;

		sign  = swizzle_sign [normal_swizzle[i]];
		index = swizzle_index[normal_swizzle[i]];

		/*
		* There is a small 1e-5f bias for precision issues. otherwise
		* we randomly get 127 or 128 for neutral colors in tangent maps.
		* we choose 128 because it is the convention flat color. *
		*/

		out[i] = sign * in[index] / 2.0f + 0.5f + 1e-5f;
	}
}

/*
 * This function converts an object space normal map to a tangent space normal map for a given low poly mesh
 */
void RE_normal_world_to_tangent(const BakePixel pixel_array[], const int num_pixels, const int depth,
                                float result[], Mesh *me, const BakeNormalSwizzle normal_swizzle[3])
{
	int i;

	TriTessFace *triangles;

	DerivedMesh *dm = CDDM_from_mesh(me);

	triangles = MEM_callocN(sizeof(TriTessFace) * (me->totface * 2), "MVerts Mesh");
	calculateTriTessFace(triangles, me, NULL, true, dm);

	BLI_assert(num_pixels >= 3);

	for (i = 0; i < num_pixels; i++) {
		TriTessFace *triangle;
		float tangents[3][3];
		float normals[3][3];
		float signs[3];
		int j;
		MVert *verts[3];

		float tangent[3];
		float normal[3];
		float binormal[3];
		float sign;
		float u, v, w;

		float tsm[3][3]; /* tangent space matrix */
		float itsm[3][3];

		int offset;
		float nor[3]; /* texture normal */

		bool smoothnormal;

		int primitive_id = pixel_array[i].primitive_id;

		offset = i * depth;

		if (primitive_id == -1) {
			copy_v3_fl3(&result[offset], 0.5f, 0.5f, 1.0f);
			continue;
		}

		triangle = &triangles[primitive_id];
		smoothnormal = triangle->smoothnormal;

		verts[0] = triangle->v1;
		verts[1] = triangle->v2;
		verts[2] = triangle->v3;

		for (j = 0; j < 3; j++) {
			TSpace *ts;
			if (smoothnormal)
				normal_short_to_float_v3(normals[j], verts[j]->no);
			else
				normal[j] = triangle->normal[j];

			ts = triangle->tspace[j];
			copy_v3_v3(tangents[j], ts->tangent);
			signs[j] = ts->sign;
		}

		u = pixel_array[i].uv[0];
		v = pixel_array[i].uv[1];
		w = 1.0f - u - v;

		/* normal */
		if (smoothnormal)
			interp_barycentric_tri_v3(normals, u, v, normal);

		/* tangent */
		interp_barycentric_tri_v3(tangents, u, v, tangent);

		/* sign */
		/* The sign is the same at all face vertices for any non degenerate face.
		 * Just in case we clamp the interpolated value though. */
		sign = (signs[0]  * u + signs[1]  * v + signs[2] * w) < 0 ? (-1.0f) : 1.0f;

		/* binormal */
		/* B = sign * cross(N, T)  */
		cross_v3_v3v3(binormal, normal, tangent);
		mul_v3_fl(binormal, sign);

		/* populate tangent space matrix */
		copy_v3_v3(tsm[0], tangent);
		copy_v3_v3(tsm[1], binormal);
		copy_v3_v3(tsm[2], normal);

		/* texture values */
		normal_uncompress(nor, &result[offset]);

		invert_m3_m3(itsm, tsm);
		mul_m3_v3(itsm, nor);
		normalize_v3(nor);

		/* save back the values */
		normal_compress(&result[offset], nor, normal_swizzle);
	}

	/* garbage collection */
	MEM_freeN(triangles);

	if (dm)
		dm->release(dm);
}

void RE_normal_world_to_object(const BakePixel pixel_array[], const int num_pixels, const int depth,
                               float result[], struct Object *ob, const BakeNormalSwizzle normal_swizzle[3])
{
	int i;
	float iobmat[4][4];

	invert_m4_m4(iobmat, ob->obmat);

	for (i = 0; i < num_pixels; i++) {
		int offset;
		float nor[3];

		if (pixel_array[i].primitive_id == -1)
			continue;

		offset = i * depth;
		normal_uncompress(nor, &result[offset]);

		mul_m4_v3(iobmat, nor);
		normalize_v3(nor);

		/* save back the values */
		normal_compress(&result[offset], nor, normal_swizzle);
	}
}

void RE_normal_world_to_world(const BakePixel pixel_array[], const int num_pixels, const int depth,
                              float result[], const BakeNormalSwizzle normal_swizzle[3])
{
	int i;

	for (i = 0; i < num_pixels; i++) {
		int offset;
		float nor[3];

		if (pixel_array[i].primitive_id == -1)
			continue;

		offset = i * depth;
		normal_uncompress(nor, &result[offset]);

		/* save back the values */
		normal_compress(&result[offset], nor, normal_swizzle);
	}
}

void RE_bake_ibuf_clear(BakeImage *images, const int tot_mat, const bool is_tangent)
{
	ImBuf *ibuf;
	void *lock;
	Image *image;
	int i;

	const float vec_alpha[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	const float vec_solid[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	const float nor_alpha[4] = {0.5f, 0.5f, 1.0f, 0.0f};
	const float nor_solid[4] = {0.5f, 0.5f, 1.0f, 1.0f};

	for (i = 0; i < tot_mat; i ++) {
		image = images[i].image;

		ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);
		BLI_assert(ibuf);

		if (is_tangent)
			IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? nor_alpha : nor_solid);
		else
			IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? vec_alpha : vec_solid);

		BKE_image_release_ibuf(image, ibuf, lock);
	}
}

/* ************************************************************* */

/* not the real UV, but the internal per-face UV instead
 I'm using it to test if everything is correct */
static bool bake_uv(const BakePixel pixel_array[], const int num_pixels, const int depth, float result[])
{
	int i;

	for (i=0; i < num_pixels; i++) {
		int offset = i * depth;
		copy_v2_v2(&result[offset], pixel_array[i].uv);
	}

	return true;
}

bool RE_internal_bake(Render *UNUSED(re), Object *UNUSED(object), const BakePixel pixel_array[],
                      const int num_pixels, const int depth, const ScenePassType pass_type, float result[])
{
	switch (pass_type) {
		case SCE_PASS_UV:
		{
			return bake_uv(pixel_array, num_pixels, depth, result);
			break;
		}
		default:
			break;
	}
	return false;
}

int RE_pass_depth(const ScenePassType pass_type)
{
	/* IMB_buffer_byte_from_float assumes 4 channels
	   making it work for now - XXX */
	return 4;

	switch (pass_type) {
		case SCE_PASS_Z:
		case SCE_PASS_AO:
		case SCE_PASS_MIST:
		{
			return 1;
		}
		case SCE_PASS_UV:
		{
			return 2;
		}
		case SCE_PASS_RGBA:
		{
			return 4;
		}
		case SCE_PASS_COMBINED:
		case SCE_PASS_DIFFUSE:
		case SCE_PASS_SPEC:
		case SCE_PASS_SHADOW:
		case SCE_PASS_REFLECT:
		case SCE_PASS_NORMAL:
		case SCE_PASS_VECTOR:
		case SCE_PASS_REFRACT:
		case SCE_PASS_INDEXOB:// XXX double check
		case SCE_PASS_INDIRECT:
		case SCE_PASS_RAYHITS: // XXX double check
		case SCE_PASS_EMIT:
		case SCE_PASS_ENVIRONMENT:
		case SCE_PASS_INDEXMA:
		case SCE_PASS_DIFFUSE_DIRECT:
		case SCE_PASS_DIFFUSE_INDIRECT:
		case SCE_PASS_DIFFUSE_COLOR:
		case SCE_PASS_GLOSSY_DIRECT:
		case SCE_PASS_GLOSSY_INDIRECT:
		case SCE_PASS_GLOSSY_COLOR:
		case SCE_PASS_TRANSM_DIRECT:
		case SCE_PASS_TRANSM_INDIRECT:
		case SCE_PASS_TRANSM_COLOR:
		case SCE_PASS_SUBSURFACE_DIRECT:
		case SCE_PASS_SUBSURFACE_INDIRECT:
		case SCE_PASS_SUBSURFACE_COLOR:
		default:
		{
			return 3;
		}
	}
}
