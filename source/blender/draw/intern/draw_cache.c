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

/** \file draw_cache.c
 *  \ingroup draw
 */


#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_mesh_render.h"

#include "GPU_batch.h"

#include "draw_cache.h"

static struct DRWShapeCache{
	Batch *drw_single_vertice;
	Batch *drw_fullscreen_quad;
	Batch *drw_plain_axes;
	Batch *drw_circle_ball;
	// Batch drw_cube;
	// Batch drw_circle;
	// Batch drw_sphere;
	// Batch drw_cone;
	// Batch drw_arrows;
} SHC = {NULL};

/* Quads */
Batch *DRW_cache_fullscreen_quad_get(void)
{
	if (!SHC.drw_fullscreen_quad) {
		float v1[2] = {-1.0f, -1.0f};
		float v2[2] = { 1.0f, -1.0f};
		float v3[2] = {-1.0f,  1.0f};
		float v4[2] = { 1.0f,  1.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 6);

		setAttrib(vbo, pos_id, 0, v1);
		setAttrib(vbo, pos_id, 1, v2);
		setAttrib(vbo, pos_id, 2, v3);

		setAttrib(vbo, pos_id, 3, v2);
		setAttrib(vbo, pos_id, 4, v3);
		setAttrib(vbo, pos_id, 5, v4);

		SHC.drw_fullscreen_quad = Batch_create(GL_TRIANGLES, vbo, NULL);
	}
	return SHC.drw_fullscreen_quad;
}

/* Common */
#define CIRCLE_RESOL 32

Batch *DRW_cache_circle_ball_get(void)
{
	if (!SHC.drw_circle_ball) {
		float v[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, CIRCLE_RESOL);

		for (int a = 0; a < CIRCLE_RESOL; a++) {
			v[0] = sinf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[1] = cosf((2.0f * M_PI * a) / ((float)CIRCLE_RESOL));
			v[2] = 0.0f;
			setAttrib(vbo, pos_id, 0, v);
		}

		SHC.drw_circle_ball = Batch_create(GL_LINE_LOOP, vbo, NULL);
	}
	return SHC.drw_circle_ball;
}

/* Empties */
Batch *DRW_cache_plain_axes_get(void)
{
	if (!SHC.drw_plain_axes) {
		int axis;
		float v1[3] = {0.0f, 0.0f, 0.0f};
		float v2[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 6);

		for (axis = 0; axis < 3; axis++) {
			v1[axis] = 1.0f;
			v2[axis] = -1.0f;

			setAttrib(vbo, pos_id, axis * 2, v1);
			setAttrib(vbo, pos_id, axis * 2 + 1, v2);

			/* reset v1 & v2 to zero for next axis */
			v1[axis] = v2[axis] = 0.0f;
		}

		SHC.drw_plain_axes = Batch_create(GL_LINES, vbo, NULL);
	}
	return SHC.drw_plain_axes;
}

/* Object Center */
Batch *DRW_cache_single_vert_get(void)
{
	if (!SHC.drw_single_vertice) {
		float v1[3] = {0.0f, 0.0f, 0.0f};

		/* Position Only 3D format */
		static VertexFormat format = { 0 };
		static unsigned pos_id;
		if (format.attrib_ct == 0) {
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		}

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, 1);

		setAttrib(vbo, pos_id, 0, v1);

		SHC.drw_single_vertice = Batch_create(GL_POINTS, vbo, NULL);
	}
	return SHC.drw_single_vertice;
}

/* Meshes */
Batch *DRW_cache_wire_overlay_get(Object *ob)
{
	Batch *overlay_wire = NULL;

	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
#if 1 /* new version not working */
	overlay_wire = BKE_mesh_batch_cache_get_overlay_edges(me);
#else
	overlay_wire = BKE_mesh_batch_cache_get_all_edges(me);
#endif
	return overlay_wire;
}

Batch *DRW_cache_wire_outline_get(Object *ob)
{
	Batch *fancy_wire = NULL;

	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	fancy_wire = BKE_mesh_batch_cache_get_fancy_edges(me);

	return fancy_wire;
}

Batch *DRW_cache_surface_get(Object *ob)
{
	Batch *surface = NULL;

	BLI_assert(ob->type == OB_MESH);

	Mesh *me = ob->data;
	surface = BKE_mesh_batch_cache_get_all_triangles(me);

	return surface;
}

#if 0 /* TODO */
struct Batch *DRW_cache_surface_material_get(Object *ob, int nr) {
	/* TODO */
	return NULL;
}
#endif