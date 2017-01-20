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

/** \file draw_mode_pass.h
 *  \ingroup draw
 */

#ifndef __DRAW_MODE_PASS_H__
#define __DRAW_MODE_PASS_H__

#include "DRW_render.h"

struct DRWPass;
struct Batch;

void DRW_mode_object_setup(struct DRWPass **wire_pass, struct DRWPass **center_pass);

void DRW_shgroup_wire(struct DRWPass *pass, float frontcol[4], float backcol[4], struct Batch *geom, const float **obmat);
void DRW_shgroup_outline(struct DRWPass *pass, float color[4], struct Batch *geom, const float **obmat);

void DRW_mode_object_add(struct DRWPass *wire_pass, struct DRWPass *center_pass, Object *ob);

#endif /* __DRAW_MODE_PASS_H__ */