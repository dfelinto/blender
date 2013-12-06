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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_BAKEMAP_H__
#define __BKE_BAKEMAP_H__

/** \file BKE_bakemap.h
 *  \ingroup bke
 *  \author
 */

struct ID;
struct bBakeMap;
struct ListBase;
struct Object;

/* ---------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif


/* Bake Map API function prototypes */
void BKE_unique_bakemap_name(struct bBakeMap *bmap, struct ListBase *list);
struct bBakeMap *BKE_bakemaps_get_active(struct ListBase *list);
void BKE_bakemaps_set_active(ListBase *list, struct bBakeMap *bmap);

struct bBakeMap *BKE_add_ob_bakemap(struct Object *ob, const char *name, short type);

int BKE_remove_bakemap(ListBase *list, struct bBakeMap *bmap);
//void BKE_remove_bakemaps_type(ListBase *list, short type, short last_only);

#ifdef __cplusplus
}
#endif

#endif

