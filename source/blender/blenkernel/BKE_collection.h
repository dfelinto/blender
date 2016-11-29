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
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_COLLECTION_H__
#define __BKE_COLLECTION_H__

/** \file blender/blenkernel/BKE_collection.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Collection;
struct ListBase;
struct Object;
struct Scene;

struct Collection *BKE_collection_add(struct Scene *scene, struct ListBase *lb, const char *name);
void BKE_collection_remove(struct Scene *scene, struct Collection *collection);
struct Collection *BKE_collection_master(struct Scene *scene);
void BKE_collection_object_add(struct Scene *scene, struct Collection *collection, struct Object *object);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_COLLECTION_H__ */

