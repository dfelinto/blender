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

/** \file blender/blenkernel/intern/layer.c
 *  \ingroup bke
 */

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_collection.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

/* prototype */
CollectionBase *collection_base_add(SceneLayer *sl, ListBase *lb, SceneCollection *sc);

/* RenderLayer */

/*
 * Add a new renderlayer
 * by default, a renderlayer has the master collection
 */
SceneLayer *BKE_scene_layer_add(Scene *scene, const char *name)
{
	SceneLayer *sl = MEM_callocN(sizeof(SceneLayer), "Scene Layer");
	BLI_strncpy(sl->name, name, sizeof(sl->name));

	SceneCollection *sc = BKE_collection_master(scene);
	collection_base_add(sl, &sl->collection_bases, sc);
	return sl;
}

bool BKE_scene_layer_remove(Main *bmain, Scene *scene, SceneLayer *sl)
{
	const int act = BLI_findindex(&scene->render_layers, sl);

	if (act == -1) {
		return false;
	}
	else if ( (scene->render_layers.first == scene->render_layers.last) &&
	          (scene->render_layers.first == sl))
	{
		/* ensure 1 layer is kept */
		return false;
	}

	BLI_remlink(&scene->render_layers, sl);

	BLI_freelistN(&sl->object_bases);
	//layer_collections_free(rl, &rl->collection_bases);
	BLI_freelistN(&sl->collection_bases);

	MEM_freeN(sl);

	/* TODO WORKSPACE: set active_layer to 0 */

	for (Scene *sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			for (node = sce->nodetree->nodes.first; node; node = node->next) {
				if (node->type == CMP_NODE_R_LAYERS && (Scene *)node->id == scene) {
					if (node->custom1 == act)
						node->custom1 = 0;
					else if (node->custom1 > act)
						node->custom1--;
				}
			}
		}
	}

	return true;
}

/*
 * Set the render engine of a renderlayer
 */
void BKE_scene_layer_engine_set(SceneLayer *sl, const char *engine)
{
	BLI_strncpy(sl->engine, engine, sizeof(sl->engine));
}

/* ObjectBase */

static void scene_layer_object_base_unref(SceneLayer* sl, ObjectBase *base)
{
	base->refcount--;

	/* It only exists in the RenderLayer */
	if (base->refcount == 1) {
		if (sl->basact == base) {
			sl->basact = NULL;
		}
	}

	BLI_remlink(&sl->object_bases, base);
	MEM_freeN(base);
}

static ObjectBase *object_base_add(SceneLayer *sl, Object *ob)
{
	ObjectBase *base = MEM_callocN(sizeof(ObjectBase), "Object Base");
	base->object = ob;
	BLI_addtail(&sl->object_bases, base);
	return base;
}

/* CollectionBase */

/*
 * Link a collection to a renderlayer
 * The collection needs to be created separately
 */
CollectionBase *BKE_collection_link(SceneLayer *sl, SceneCollection *sc)
{
	CollectionBase *base = collection_base_add(sl, &sl->collection_bases, sc);
	return base;
}

static void collection_base_free(SceneLayer *sl, CollectionBase *cb)
{
	for (CollectionBase *ncb = cb->collection_bases.first; ncb; ncb = ncb->next) {
		for (LinkData *link = ncb->object_bases.first; link; link = link->next) {
			scene_layer_object_base_unref(sl, link->data);
		}

		BLI_freelistN(&ncb->object_bases);
		collection_base_free(sl, ncb);
	}
}

/*
 * Unlink a collection base from a renderlayer
 * The corresponding collection is not removed from the master collection
 */
void BKE_collection_unlink(SceneLayer *sl, CollectionBase *cb)
{
	collection_base_free(sl, cb);

	BLI_remlink(&sl->collection_bases, cb);
	MEM_freeN(cb);
}

static void object_base_populate(SceneLayer *sl, CollectionBase *cb, ListBase *objects)
{
	for (LinkData *link = objects->first; link; link = link->next) {
		ObjectBase *base = BLI_findptr(&sl->object_bases, link->data, offsetof(ObjectBase, object));

		if (base == NULL) {
			base = object_base_add(sl, link->data);
		}
		else {
			/* only add an object once */
			if (BLI_findptr(&cb->object_bases, base, offsetof(LinkData, data))) {
				continue;
			}
		}

		base->refcount++;
		BLI_addtail(&cb->object_bases, BLI_genericNodeN(base));
	}
}

static void collection_base_populate(SceneLayer *sl, CollectionBase *cb, SceneCollection *sc)
{
	object_base_populate(sl, cb, &sc->objects);
	object_base_populate(sl, cb, &sc->filter_objects);

	for (SceneCollection *nsc = sc->collections.first; nsc; nsc = nsc->next) {
		collection_base_add(sl, &cb->collection_bases, nsc);
	}
}

CollectionBase *collection_base_add(SceneLayer *sl, ListBase *lb, SceneCollection *sc)
{
	CollectionBase *cb = MEM_callocN(sizeof(CollectionBase), "Collection Base");
	BLI_addtail(lb, cb);

	cb->collection = sc;
	cb->flag = COLLECTION_VISIBLE + COLLECTION_SELECTABLE + COLLECTION_FOLDED;

	collection_base_populate(sl, cb, sc);
	return cb;
}

/* Override */

/*
 * Add a new datablock override
 */
void BKE_collection_override_datablock_add(CollectionBase *UNUSED(cb), const char *UNUSED(data_path), ID *UNUSED(id))
{
	TODO_LAYER_OVERRIDE;
}
