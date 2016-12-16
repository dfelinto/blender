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

/*
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

	TODO_LAYER_SYNC;
	return sc;
}

/* free the collection items recursively */
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

/* unlink the collection recursively
 * return true if unlinked */
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

/*
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

/*
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

/*
 * Returns the master collection
 */
SceneCollection *BKE_collection_master(Scene *scene)
{
	return scene->collection;
}

/*
 * Free (or release) any data used by the master collection (does not free the master collection itself).
 * Used only to clear the entire scene data since it's not doing re-syncing of the LayerCollection tree
 */
void BKE_collection_master_free(Scene *scene){
	collection_free(BKE_collection_master(scene));
}

/*
 * Add object to collection
 */
void BKE_collection_object_add(struct Scene *UNUSED(scene), struct SceneCollection *sc, struct Object *ob)
{
	BLI_addtail(&sc->objects, BLI_genericNodeN(ob));
	id_us_plus((ID *)ob);
	TODO_LAYER_SYNC;
	/* add the equivalent object base to all layers that have this collection */
}

/*
 * Remove object from collection
 */
void BKE_collection_object_remove(struct Scene *UNUSED(scene), struct SceneCollection *sc, struct Object *ob)
{

	LinkData *link = BLI_findptr(&sc->objects, ob, offsetof(LinkData, data));
	BLI_remlink(&sc->objects, link);
	MEM_freeN(link);

	id_us_min((ID *)ob);
	TODO_LAYER_SYNC;

	/* remove the equivalent object base to all layers that have this collection
	 * also remove all reference to ob in the filter_objects */
}

/*
 * Tag util functions to make sure the same object is not called twice
 */

static void object_tag(Object *ob)
{
	ob->flag |= BA_TEMP_TAG;
}

static void object_tag_clear(Object *ob, void *UNUSED(data))
{
	ob->flag &= ~BA_TEMP_TAG;
}

static bool object_tag_test(Object *ob)
{
	return (ob->flag & BA_TEMP_TAG) != 0;
}

/*
 * Recursively calls the callback function for the objects in a SceneCollection
 */
static void collection_objects_callback(SceneCollection *sc, BKE_scene_objects_Cb callback, void *data)
{
	for (LinkData *link= sc->objects.first; link; link = link->next) {
		if (object_tag_test(link->data)) {
			callback(link->data, data);
			object_tag(link->data);
		}
	}

	for (LinkData *link= sc->filter_objects.first; link; link = link->next) {
		callback(link->data, data);
	}

	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {
		collection_objects_callback(nsc, callback, data);
	}
}

/*
 * Recursively calls the callback function for the objects in a Scene
 * The same object
 */
void BKE_scene_objects_callback(Scene *scene, BKE_scene_objects_Cb callback, void *data)
{
	SceneCollection *sc = BKE_collection_master(scene);
	collection_objects_callback(sc, object_tag_clear, NULL);
	collection_objects_callback(sc, callback, data);
}


/* ---------------------------------------------------------------------- */
/* Iteractors */

/* sequence strip iterator:
 * - builds a full array, recursively into meta strips
 */

static void scene_objects_count(Object *UNUSED(ob), void *data)
{
	int *tot = data;
	(*tot)++;
}

static void scene_objects_build_array(Object *ob, void *data)
{
	Object ***array = data;
	**array = ob;
	(*array)++;
}

static void scene_objects_array(Scene *scene, Object ***objects_array, int *tot)
{
	Object **array;

	*objects_array = NULL;
	*tot = 0;

	if (scene == NULL)
		return;

	BKE_scene_objects_callback(scene, scene_objects_count, tot);

	if (*tot == 0)
		return;

	*objects_array = array = MEM_mallocN(sizeof(Object *) * (*tot), "ObjectsArray");
	BKE_scene_objects_callback(scene, scene_objects_build_array, &array);
}

/*
 * Only use this in non-performance critical situations
 * (it iterates over all scene collections twice)
 */
void BKE_scene_objects_Iterator_begin(Iterator *iter, void *data)
{
	scene_objects_array(data, (Object ***)&iter->array, &iter->tot);
}
