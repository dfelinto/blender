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
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 * Contributors: Vertex color baking, Copyright 2011 AutoCRC
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/bake.c
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


typedef struct BakeData {
	BakePixel *pixel_array;
	int width;
	int primitive_id;
	ZSpan zspan;
} BakeData;

/* ************************* bake ************************ */
static void store_bake_pixel(void *handle, int x, int y, float u, float v)
{
	BakeData *bd = (BakeData *)handle;
	BakePixel *pixel_array = bd->pixel_array;
	const int width = bd->width;
	const int i = y * width + x;

	pixel_array[i].primitive_id = bd->primitive_id;
	pixel_array[i].u = u;
	pixel_array[i].v = v;

	pixel_array[i].dudx =
	pixel_array[i].dudy =
	pixel_array[i].dvdx =
	pixel_array[i].dvdy =
	0.f;
}

void RE_bake_margin(BakePixel pixel_array[], ImBuf *ibuf, const int margin, const int width, const int height)
{
	char *mask_buffer = NULL;
	const int num_pixels = width * height;
	int i;

	if (margin < 1)
		return;

	mask_buffer = MEM_callocN(sizeof(char) * num_pixels, "BakeMask");

	/* only extend to pixels outside the mask area */
	for (i=0; i < num_pixels; i++) {
		if (pixel_array[i].primitive_id != -1) {
			mask_buffer[i] = FILTER_MASK_USED;
		}
	}

	RE_bake_ibuf_filter(ibuf, mask_buffer, margin);

	MEM_freeN(mask_buffer);
}

typedef struct TriTessFace
{
	MVert *v1;
	MVert *v2;
	MVert *v3;
} TriTessFace;

/*
 * This function returns the coordinate and normal of a barycentric u,v for a face defined by the primitive_id index.
 */

static void get_point_from_barycentric(TriTessFace *triangles, int primitive_id, float u, float v, float cage_extrusion, float r_co[3], float r_dir[3])
{
	float data[3][3];
	float coord[3];
	float dir[3];
	float cage[3];

	TriTessFace *mverts = &triangles[primitive_id];

	copy_v3_v3(data[0], mverts->v1->co);
	copy_v3_v3(data[1], mverts->v2->co);
	copy_v3_v3(data[2], mverts->v3->co);

	interp_barycentric_tri_v3(data, u, v, coord);

	normal_short_to_float_v3(data[0], mverts->v1->no);
	normal_short_to_float_v3(data[1], mverts->v2->no);
	normal_short_to_float_v3(data[2], mverts->v3->no);

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
 * Transcribed from Christer Ericson's Real-Time Collision Detection
 *
 * Compute barycentric coordinates (u, v, w) for
 * point p with respect to triangle (a, b, c)
 *
 */
static void Barycentric(float p[3], float a[3], float b[3], float c[3], float *u, float *v, float *w)
{
	float v0[3], v1[3], v2[3];
	float d00, d01, d11, d20, d21, denom;

	sub_v3_v3v3(v0, b, a);
	sub_v3_v3v3(v1, c, a);
	sub_v3_v3v3(v2, p, a);

	d00 = dot_v3v3(v0, v0);
	d01 = dot_v3v3(v0, v1);
	d11 = dot_v3v3(v1, v1);
	d20 = dot_v3v3(v2, v0);
	d21 = dot_v3v3(v2, v1);
	denom = d00 * d11 - d01 * d01;

	*v = (d11 * d20 - d01 * d21) / denom;
	*w = (d00 * d21 - d01 * d20) / denom;

	*u = 1.0f - *v - *w;
}

/*
 * This function returns the barycentric u,v of a face for a coordinate. The face is defined by its index.
 */
static void get_barycentric_from_point(TriTessFace *triangles, int index, float co[3], int *primitive_id, float *u, float *v)
{
	float w;
	TriTessFace *tri = &triangles[index];
	Barycentric(co, tri->v1->co, tri->v2->co, tri->v3->co, u, v, &w);
	*primitive_id = index;
}

/*
 * This function populates pixel_array and returns TRUE if things are correct
 */
static bool cast_ray_highpoly(BVHTreeFromMesh *treeData, TriTessFace *triangles, BakePixel *pixel_array, float co[3], float dir[3])
{
	int primitive_id;
	float u;
	float v;

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

		get_barycentric_from_point(triangles, hit.index, hit.co, &primitive_id, &u, &v);

		pixel_array->primitive_id = primitive_id;
		pixel_array->u = u;
		pixel_array->v = v;
		return true;
	}

	pixel_array->primitive_id = -1;
	return false;
}

/*
 * This function populates an array of verts for the triangles of a mesh
 */
static void calculateTriTessFace(TriTessFace *triangles, Mesh *me, int (*lookup_id)[2])
{
	int i;
	int p_id;
	MFace *mface;
	MVert *mvert;

	mface = CustomData_get_layer(&me->fdata, CD_MFACE);
	mvert = CustomData_get_layer(&me->vdata, CD_MVERT);

	p_id = -1;
	for (i = 0; i < me->totface; i++) {
		MFace *mf = &mface[i];

		++p_id;

		if (lookup_id)
			lookup_id[i][0] = p_id;

		triangles[p_id].v1 = &mvert[mf->v1];
		triangles[p_id].v2 = &mvert[mf->v2];
		triangles[p_id].v3 = &mvert[mf->v3];

		/* 4 vertices in the face */
		if (mf->v4 != 0) {
			++p_id;

			if (lookup_id)
				lookup_id[i][1] = p_id;

			triangles[p_id].v1 = &mvert[mf->v1];
			triangles[p_id].v2 = &mvert[mf->v3];
			triangles[p_id].v3 = &mvert[mf->v4];
		}
		else {
			if (lookup_id)
				lookup_id[i][1] = -1;
		}
	}

	BLI_assert(p_id < me->totface * 2);
}

void RE_populate_bake_pixels_from_object(Mesh *me_low, Mesh *me_high,
                                         BakePixel pixel_array_from[], BakePixel pixel_array_to[],
                                         const int num_pixels, const float cage_extrusion)
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

	calculateTriTessFace(tris_low, me_low, NULL);
	calculateTriTessFace(tris_high, me_high, NULL);

	dm_high = CDDM_from_mesh(me_high);

	/* Create a bvh-tree of the given target */
	bvhtree_from_mesh_faces(&treeData, dm_high, 0.0, 2, 6);
	if (treeData.tree == NULL) {
		printf("Baking: Out of memory\n");

		dm_high->release(dm_high);

		MEM_freeN(tris_low);
		MEM_freeN(tris_high);
		return;
	}

	for (i=0; i < num_pixels; i++) {
		float co[3];
		float dir[3];

		primitive_id = pixel_array_from[i].primitive_id;

		if (primitive_id == -1)
			continue;

		u = pixel_array_from[i].u;
		v = pixel_array_from[i].v;

		/* calculate from low poly mesh cage */
		get_point_from_barycentric(tris_low, primitive_id, u, v, cage_extrusion, co, dir);

		/* cast ray */
		cast_ray_highpoly(&treeData, tris_high, &pixel_array_to[i], co, dir);
	}

	/* garbage collection */
	free_bvhtree_from_mesh(&treeData);

	dm_high->release(dm_high);

	MEM_freeN(tris_low);
	MEM_freeN(tris_high);
}

void RE_populate_bake_pixels(Mesh *me, BakePixel pixel_array[], const int width, const int height)
{
	BakeData bd;
	const int num_pixels = width * height;
	int i, a;
	int p_id;
	MTFace *mtface;
	MFace *mface;

	/* we can't bake in edit mode */
	if (me->edit_btmesh)
		return;

	bd.pixel_array = pixel_array;
	bd.width = width;

	/* initialize all pixel arrays so we know which ones are 'blank' */
	for (i=0; i < num_pixels; i++) {
		pixel_array[i].primitive_id = -1;
	}

	zbuf_alloc_span(&bd.zspan, width, height, R.clipcrop);

	mtface = CustomData_get_layer(&me->fdata, CD_MTFACE);
	mface = CustomData_get_layer(&me->fdata, CD_MFACE);

	if (mtface == NULL)
		return;

	p_id = -1;
	for (i = 0; i < me->totface; i++) {
		float vec[4][2];
		MTFace *mtf = &mtface[i];
		MFace *mf = &mface[i];

		bd.primitive_id = ++p_id;

		for (a = 0; a < 4; a++) {
			/* Note, workaround for pixel aligned UVs which are common and can screw up our intersection tests
			 * where a pixel gets in between 2 faces or the middle of a quad,
			 * camera aligned quads also have this problem but they are less common.
			 * Add a small offset to the UVs, fixes bug #18685 - Campbell */
			vec[a][0] = mtf->uv[a][0] * (float)width - (0.5f + 0.001f);
			vec[a][1] = mtf->uv[a][1] * (float)height - (0.5f + 0.002f);
		}

		zspan_scanconvert(&bd.zspan, (void *)&bd, vec[0], vec[1], vec[2], store_bake_pixel);

		/* 4 vertices in the face */
		if (mf->v4 != 0) {
			bd.primitive_id = ++p_id;
			zspan_scanconvert(&bd.zspan, (void *)&bd, vec[0], vec[2], vec[3], store_bake_pixel);
		}
	}

	zbuf_free_span(&bd.zspan);
}

/* ******************** NORMALS ************************ */

/* convert a normalized normal to the -1.0 1.0 range
 * the input is expected to be NEG_X, NEG_Y, NEG_Z
 * the output is POS_X, POS_Y, POS_Z
 */
static void normal_uncompress(float *out, float in[3])
{
	for (int i=0; i < 3; i++)
		out[i] = - (2.0f * in[i] - 1.0f);
}

static void normal_compress(float *out, float in[3], int normal_swizzle[3]) {
	static const int swizzle[6][2] = // sign, index
	{
		{  1, 0, }, // OB_POSX
		{  1, 1, }, // OB_POSY
		{  1, 2, }, // OB_POSZ
		{ -1, 0, }, // OB_NEGX
		{ -1, 1, }, // OB_NEGY
		{ -1, 2, }, // OB_NEGZ
	};

	for (int i=0; i < 3; i++) {
		int id, sign;

		sign = swizzle[normal_swizzle[i]][0];
		id   = swizzle[normal_swizzle[i]][1];

		/*
		* There is a small 1e-5f bias for precision issues. otherwise
		* we randomly get 127 or 128 for neutral colors in tangent maps.
		* we choose 128 because it is the convention flat color. *
		*/

		out[i] = sign * in[id] / 2.0f + 0.5f + 1e-5f;
	}
}

/*
 * struct wrapping up tangent space data
 */
typedef struct {
	float tangent[3];
	float sign;
} TSpace;

/*
 * This function converts an object space normal map to a tangent space normal map for a given low poly mesh
 * XXX the mesh has to be triangulated at the moment.
 */
void RE_normal_world_to_tangent(BakePixel pixel_array[], int num_pixels, int depth, float result[], Mesh *me, int normal_swizzle[3])
{
	int i;
	TSpace *tspace;
	MFace *mface;
	MVert *mvert;
	float *fnormals;

	//TODO for now assuming it's triangulated - it will crash otherwise

	DerivedMesh *dm = CDDM_from_mesh(me);
	DM_ensure_normals(dm);
	DM_add_tangent_layer(dm);

	tspace = dm->getTessFaceDataArray(dm, CD_TANGENT);

	mface = dm->getTessFaceArray(dm);
	mvert = dm->getVertArray(dm);

	fnormals = dm->getTessFaceDataArray(dm, CD_NORMAL);

	BLI_assert(tspace);
	BLI_assert(fnormals);
	BLI_assert(num_pixels >= 3);

	for (i=0; i < num_pixels; i++) {
		MFace *mf;
		float tangents[3][3];
		float normals[3][3];
		float signs[3];
		int j;
		int verts[3];

		float tangent[3];
		float normal[3];
		float binormal[3];
		float sign;
		float u, v, w;

		float tsm[3][3]; /* tangent space matrix */
		float itsm[3][3];

		int offset;
		float nor[3]; /* texture normal */

		int primitive_id = pixel_array[i].primitive_id;

		bool smoothnormal;

		if (primitive_id == -1)
			continue;

		mf = &mface[primitive_id];
		smoothnormal = (mf->flag & ME_SMOOTH);

		verts[0] = mf->v1;
		verts[1] = mf->v2;
		verts[2] = mf->v3;

		for (j = 0; j < 3; j++) {
			TSpace *ts;
			int vert = verts[j];

			if (smoothnormal)
				normal_short_to_float_v3(normals[j], mvert[vert].no);
			else
				//XXX using co to calculate normal manually
				copy_v3_v3(normals[j], mvert[vert].co);

			ts = &tspace[vert];
			copy_v3_v3(tangents[j], ts->tangent);
			signs[j] = ts->sign;
		}

		u = pixel_array[i].u;
		v = pixel_array[i].v;
		w = 1.0f - u - v;

		/* normal */
		if (smoothnormal)
			interp_barycentric_tri_v3(normals, u, v, normal);
		else
			//copy_v3_v3(normal, &fnormals[primitive_id]);
			/* XXX not nice to calculate the normal every single time, but I had
			   some troubles to access the face normal, so this will do for now */
			normal_tri_v3(normal, normals[0], normals[1], normals[2]);

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
		offset = i * depth;
		normal_uncompress(nor, &result[offset]);

		invert_m3_m3(itsm, tsm);
		mul_m3_v3(itsm, nor);
		normalize_v3(nor);

		/* The invert of the red channel is to make
		 * the normal map compliant with the outside world.
		 * It needs to be done because in Blender
		 * the normal used in the renderer points inward. It is generated
		 * this way in calc_vertexnormals(). Should this ever change
		 * this negate must be removed.
		 */
		nor[0] = -nor[0];

		/* save back the values */
		normal_compress(&result[offset], nor, normal_swizzle);
	}

	/* garbage collection */
	if (dm)
		dm->release(dm);
}

void RE_normal_world_to_object(BakePixel pixel_array[], int num_pixels,  int depth, float result[], struct Object *ob, int normal_swizzle[3])
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

void RE_normal_world_to_world(BakePixel pixel_array[], int num_pixels,  int depth, float result[], int normal_swizzle[3])
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

/* ************************************************************* */

/* not the real UV, but the internal per-face UV instead
 I'm using it to test if everything is correct */
static bool bake_uv(BakePixel pixel_array[], int num_pixels, int depth, float result[])
{
	int i;

	for (i=0; i < num_pixels; i++) {
		int offset = i * depth;
		result[offset] = pixel_array[i].u;
		result[offset + 1] = pixel_array[i].v;
	}

	return true;
}

bool RE_internal_bake(Render *UNUSED(re), Object *UNUSED(object), BakePixel pixel_array[], int num_pixels, int depth, ScenePassType pass_type, float result[])
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

int RE_pass_depth(ScenePassType pass_type)
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
