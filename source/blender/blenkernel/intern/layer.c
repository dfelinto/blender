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
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLT_translation.h"

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
LayerCollection *layer_collection_add(SceneLayer *sl, ListBase *lb, SceneCollection *sc);

/* RenderLayer */

/*
 * Add a new renderlayer
 * by default, a renderlayer has the master collection
 */
SceneLayer *BKE_scene_layer_add(Scene *scene, const char *name)
{
	SceneLayer *sl = MEM_callocN(sizeof(SceneLayer), "Scene Layer");
	BLI_addtail(&scene->render_layers, sl);

	/* unique name */
	BLI_strncpy_utf8(sl->name, name, sizeof(sl->name));
	BLI_uniquename(&scene->render_layers, sl, DATA_("SceneLayer"), '.', offsetof(SceneLayer, name), sizeof(sl->name));

	SceneCollection *sc = BKE_collection_master(scene);
	layer_collection_add(sl, &sl->collections, sc);

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
	BLI_freelistN(&sl->collections);

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
LayerCollection *BKE_collection_link(SceneLayer *sl, SceneCollection *sc)
{
	LayerCollection *lc = layer_collection_add(sl, &sl->collections, sc);
	return lc;
}

static void layer_collection_free(SceneLayer *sl, LayerCollection *lc)
{
	for (LayerCollection *nlc = lc->collections.first; nlc; nlc = nlc->next) {
		for (LinkData *link = nlc->object_bases.first; link; link = link->next) {
			scene_layer_object_base_unref(sl, link->data);
		}

		BLI_freelistN(&nlc->object_bases);
		layer_collection_free(sl, nlc);
	}
}

/*
 * Unlink a collection base from a renderlayer
 * The corresponding collection is not removed from the master collection
 */
void BKE_collection_unlink(SceneLayer *sl, LayerCollection *lc)
{
	layer_collection_free(sl, lc);

	BLI_remlink(&sl->collections, lc);
	MEM_freeN(lc);
}

static void object_base_populate(SceneLayer *sl, LayerCollection *lc, ListBase *objects)
{
	for (LinkData *link = objects->first; link; link = link->next) {
		ObjectBase *base = BLI_findptr(&sl->object_bases, link->data, offsetof(ObjectBase, object));

		if (base == NULL) {
			base = object_base_add(sl, link->data);
		}
		else {
			/* only add an object once */
			if (BLI_findptr(&lc->object_bases, base, offsetof(LinkData, data))) {
				continue;
			}
		}

		base->refcount++;
		BLI_addtail(&lc->object_bases, BLI_genericNodeN(base));
	}
}

static void layer_collection_populate(SceneLayer *sl, LayerCollection *lc, SceneCollection *sc)
{
	object_base_populate(sl, lc, &sc->objects);
	object_base_populate(sl, lc, &sc->filter_objects);

	for (SceneCollection *nsc = sc->collections.first; nsc; nsc = nsc->next) {
		layer_collection_add(sl, &lc->collections, nsc);
	}
}

LayerCollection *layer_collection_add(SceneLayer *sl, ListBase *lb, SceneCollection *sc)
{
	LayerCollection *lc = MEM_callocN(sizeof(LayerCollection), "Collection Base");
	BLI_addtail(lb, lc);

	lc->collection = sc;
	lc->flag = COLLECTION_VISIBLE + COLLECTION_SELECTABLE + COLLECTION_FOLDED;

	layer_collection_populate(sl, lc, sc);
	return lc;
}

/* Override */

/*
 * Add a new datablock override
 */
void BKE_collection_override_datablock_add(LayerCollection *UNUSED(lc), const char *UNUSED(data_path), ID *UNUSED(id))
{
	TODO_LAYER_OVERRIDE;
}
