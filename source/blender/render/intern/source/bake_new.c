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
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_library.h"

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

void RE_populate_bake_pixels(Object *object, BakePixel pixel_array[], const int width, const int height)
{
	BakeData bd;
	const int num_pixels = width * height;
	int i, a;
	int p_id;
	MTFace *mtface;
	MFace *mface;

	Mesh *me = (Mesh *)object->data;

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

	/* remove tessface to ensure we don't hold references to invalid faces */
	BKE_mesh_tessface_clear(me);
	BKE_mesh_tessface_calc(me);

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

int RE_internal_bake(Render *UNUSED(re), Object *UNUSED(object), BakePixel UNUSED(pixel_array[]), int UNUSED(num_pixels), int UNUSED(depth), int UNUSED(pass_type), float UNUSED(result[]))
{
	return 0;
}
