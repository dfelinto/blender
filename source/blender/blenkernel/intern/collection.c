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

/** \file blender/blenkernel/intern/collection.c
 *  \ingroup bke
 */

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_iterator.h"
#include "BLI_listbase.h"
#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_layer.h"
#include "BKE_library.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

/**
 * Add a collection to a collection ListBase and syncronize all render layers
 * The ListBase is NULL when the collection is to be added to the master collection
 */
SceneCollection *BKE_collection_add(Scene *scene, SceneCollection *sc_parent, const char *name)
{
	SceneCollection *sc = MEM_callocN(sizeof(SceneCollection), "New Collection");
	SceneCollection *sc_master = BKE_collection_master(scene);

	BLI_strncpy(sc->name, name, sizeof(sc->name));
	BLI_uniquename(&sc_master->scene_collections, sc, DATA_("Collection"), '.', offsetof(SceneCollection, name), sizeof(sc->name));

	BLI_addtail(&sc_parent->scene_collections, sc);

	BKE_layer_sync_new_scene_collection(scene, sc_parent, sc);
	return sc;
}

/**
 * Free the collection items recursively
 */
static void collection_free(SceneCollection *sc)
{
	for (LinkData *link = sc->objects.first; link; link = link->next) {
		id_us_min(link->data);
	}
	BLI_freelistN(&sc->objects);

	for (LinkData *link = sc->filter_objects.first; link; link = link->next) {
		id_us_min(link->data);
	}
	BLI_freelistN(&sc->filter_objects);

	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {
		collection_free(nsc);
	}
	BLI_freelistN(&sc->scene_collections);
}

/**
 * Unlink the collection recursively
 * return true if unlinked
 */
static bool collection_remlink(SceneCollection *sc_parent, SceneCollection *sc_gone)
{
	for (SceneCollection *sc = sc_parent->scene_collections.first; sc; sc = sc->next)
	{
		if (sc == sc_gone) {
			BLI_remlink(&sc_parent->scene_collections, sc_gone);
			return true;
		}

		if (collection_remlink(sc, sc_gone)) {
			return true;
		}
	}
	return false;
}

/**
 * Recursively remove any instance of this SceneCollection
 */
static void layer_collection_remove(SceneLayer *sl, ListBase *lb, const SceneCollection *sc)
{
	LayerCollection *lc = lb->first;
	while(lc) {
		if (lc->scene_collection == sc) {
			BKE_layer_collection_free(sl, lc);
			BLI_remlink(lb, lc);

			LayerCollection *lc_next = lc->next;
			MEM_freeN(lc);
			lc = lc_next;

			/* only the "top-level" layer collections may have the
			 * same SceneCollection in a sibling tree.
			 */
			if (lb != &sl->layer_collections) {
				return;
			}
		}

		else {
			layer_collection_remove(sl, &lc->layer_collections, sc);
			lc = lc->next;
		}
	}
}

/**
 * Remove a collection from the scene, and syncronize all render layers
 */
bool BKE_collection_remove(Scene *scene, SceneCollection *sc)
{
	SceneCollection *sc_master = BKE_collection_master(scene);

	/* the master collection cannot be removed */
	if (sc == sc_master) {
		return false;
	}

	/* unlink from the respective collection tree */
	if (!collection_remlink(sc_master, sc)) {
		BLI_assert(false);
	}

	/* clear the collection items */
	collection_free(sc);

	/* check all layers that use this collection and clear them */
	for (SceneLayer *sl = scene->render_layers.first; sl; sl = sl->next) {
		layer_collection_remove(sl, &sl->layer_collections, sc);
		sl->active_collection = 0;
	}

	MEM_freeN(sc);
	return true;
}

/**
 * Returns the master collection
 */
SceneCollection *BKE_collection_master(Scene *scene)
{
	return scene->collection;
}

/**
 * Free (or release) any data used by the master collection (does not free the master collection itself).
 * Used only to clear the entire scene data since it's not doing re-syncing of the LayerCollection tree
 */
void BKE_collection_master_free(Scene *scene){
	collection_free(BKE_collection_master(scene));
}

/**
 * Add object to collection
 */
void BKE_collection_object_add(struct Scene *scene, struct SceneCollection *sc, struct Object *ob)
{
	if (BLI_findptr(&sc->objects, ob, offsetof(LinkData, data))) {
		/* don't add the same object twice */
		return;
	}

	BLI_addtail(&sc->objects, BLI_genericNodeN(ob));
	id_us_plus((ID *)ob);
	BKE_layer_sync_object_link(scene, sc, ob);
}

/**
 * Remove object from collection
 */
void BKE_collection_object_remove(struct Scene *scene, struct SceneCollection *sc, struct Object *ob)
{

	LinkData *link = BLI_findptr(&sc->objects, ob, offsetof(LinkData, data));
	BLI_remlink(&sc->objects, link);
	MEM_freeN(link);

	id_us_min((ID *)ob);

	TODO_LAYER_SYNC_FILTER; /* need to remove all instances of ob in scene collections -> filter_objects */
	BKE_layer_sync_object_unlink(scene, sc, ob);
}

/* ---------------------------------------------------------------------- */
/* Iteractors */
/* scene collection iteractor */

typedef struct SceneCollectionsIteratorData {
	Scene *scene;
	void **array;
	int tot, cur;
 } SceneCollectionsIteratorData;

static void scene_collection_callback(SceneCollection *sc, BKE_scene_collections_Cb callback, void *data)
{
	callback(sc, data);

	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {
		scene_collection_callback(nsc, callback, data);
	}
}

static void scene_collections_count(SceneCollection *UNUSED(sc), void *data)
{
	int *tot = data;
	(*tot)++;
}

static void scene_collections_build_array(SceneCollection *sc, void *data)
{
	SceneCollection ***array = data;
	**array = sc;
	(*array)++;
}

static void scene_collections_array(Scene *scene, SceneCollection ***collections_array, int *tot)
{
	SceneCollection *sc = BKE_collection_master(scene);
	SceneCollection **array;

	*collections_array = NULL;
	*tot = 0;

	if (scene == NULL)
		return;

	scene_collection_callback(sc, scene_collections_count, tot);

	if (*tot == 0)
		return;

	*collections_array = array = MEM_mallocN(sizeof(SceneCollection *) * (*tot), "SceneCollectionArray");
	scene_collection_callback(sc, scene_collections_build_array, &array);
}

/**
 * Only use this in non-performance critical situations
 * (it iterates over all scene collections twice)
 */
void BKE_scene_collections_Iterator_begin(Iterator *iter, void *data_in)
{
	Scene *scene = data_in;
	SceneCollectionsIteratorData *data = MEM_callocN(sizeof(SceneCollectionsIteratorData), __FUNCTION__);

	data->scene = scene;
	iter->data = data;

	scene_collections_array(scene, (SceneCollection ***)&data->array, &data->tot);
	BLI_assert(data->tot != 0);

	data->cur = 0;
	iter->current = data->array[data->cur];
	iter->valid = true;
}

void BKE_scene_collections_Iterator_next(struct Iterator *iter)
{
	SceneCollectionsIteratorData *data = iter->data;

	if (++data->cur < data->tot) {
		iter->current = data->array[data->cur];
	}
	else {
		iter->valid = false;
	}
}

void BKE_scene_collections_Iterator_end(struct Iterator *iter)
{
	SceneCollectionsIteratorData *data = iter->data;

	if (data) {
		if (data->array) {
			MEM_freeN(data->array);
		}
		MEM_freeN(data);
	}
	iter->valid = false;
}


/* scene objects iteractor */

typedef struct SceneObjectsIteratorData {
	GSet *visited;
	LinkData *link;
	Iterator scene_collection_iter;
} SceneObjectsIteratorData;

void BKE_scene_objects_Iterator_begin(Iterator *iter, void *data_in)
{
	Scene *scene = data_in;
	SceneObjectsIteratorData *data = MEM_callocN(sizeof(SceneObjectsIteratorData), __FUNCTION__);
	iter->data = data;

	/* lookup list ot make sure each object is object called once */
	data->visited = BLI_gset_ptr_new(__func__);

	/* we wrap the scenecollection iterator here to go over the scene collections */
	BKE_scene_collections_Iterator_begin(&data->scene_collection_iter, scene);

	SceneCollection *sc = data->scene_collection_iter.current;
	iter->current = sc->objects.first;
}

/**
 * Gets the next unique object
 */
static LinkData *object_base_next(GSet *gs, LinkData *link)
{
	if (link == NULL) {
		return NULL;
	}

	LinkData *link_next = link->next;
	if (link_next) {
		Object *ob = link_next->data;
		if (!BLI_gset_haskey(gs, ob)) {
			BLI_gset_add(gs, ob);
			return link_next;
		}
		else {
			return object_base_next(gs, link_next);
		}
	}
	return NULL;
}

void BKE_scene_objects_Iterator_next(Iterator *iter)
{
	SceneObjectsIteratorData *data = iter->data;
	LinkData *link = object_base_next(data->visited, data->link);

	if (link) {
		data->link = link;
		iter->current = link->data;
	}
	else {
		/* if this is the last object of this ListBase look at the next SceneCollection */
		SceneCollection *sc;
		BKE_scene_collections_Iterator_next(&data->scene_collection_iter);
		do {
			sc = data->scene_collection_iter.current;
			/* get the first unique object of this collection */
			LinkData *new_link = object_base_next(data->visited, sc->objects.first);
			if (new_link) {
				data->link = new_link;
				iter->current = data->link->data;
				return;
			}
			BKE_scene_collections_Iterator_next(&data->scene_collection_iter);
		} while (data->scene_collection_iter.valid);

		if (!data->scene_collection_iter.valid) {
			iter->valid = false;
		}
	}
}

void BKE_scene_objects_Iterator_end(Iterator *iter)
{
	SceneObjectsIteratorData *data = iter->data;
	if (data) {
		BKE_scene_collections_Iterator_end(&data->scene_collection_iter);
		BLI_gset_free(data->visited, NULL);
		MEM_freeN(data);
	}
}
