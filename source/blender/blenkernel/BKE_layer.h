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

#ifndef __BKE_LAYER_H__
#define __BKE_LAYER_H__

/** \file blender/blenkernel/BKE_layer.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

#define TODO_LAYER_SYNC
#define TODO_LAYER_OVERRIDE

struct LayerCollection;
struct ID;
struct Main;
struct Object;
struct ObjectBase;
struct Scene;
struct SceneCollection;
struct SceneLayer;

struct SceneLayer *BKE_scene_layer_add(struct Scene *scene, const char *name);

bool BKE_scene_layer_remove(struct Main *bmain, struct Scene *scene, struct SceneLayer *sl);

void BKE_scene_layer_engine_set(struct SceneLayer *sl, const char *engine);

struct ObjectBase *BKE_scene_layer_base_find(struct SceneLayer *sl, struct Object *ob);

void BKE_layer_collection_free(struct SceneLayer *sl, struct LayerCollection *lc);

struct LayerCollection *BKE_layer_collection_active(struct SceneLayer *sl);

int BKE_layer_collection_count(struct SceneLayer *sl);

int BKE_layer_collection_findindex(struct SceneLayer *sl, struct LayerCollection *lc);

struct LayerCollection *BKE_collection_link(struct SceneLayer *sl, struct SceneCollection *sc);

void BKE_collection_unlink(struct SceneLayer *sl, struct LayerCollection *lc);

void BKE_collection_override_datablock_add(struct LayerCollection *lc, const char *data_path, struct ID *id);


#ifdef __cplusplus
}
#endif

#endif /* __BKE_LAYER_H__ */
