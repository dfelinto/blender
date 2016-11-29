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
#include "BLI_listbase.h"
#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_layer.h"
#include "BKE_library.h"

#include "DNA_layer_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

Collection *BKE_collection_add(Scene *scene, ListBase *lb, const char *name)
{
	Collection *collection = MEM_callocN(sizeof(Collection), "New Collection");
	BLI_strncpy(collection->name, name, sizeof(collection->name));
	BLI_uniquename(&scene->collections, collection, DATA_("Collection"), '.', offsetof(Collection, name), sizeof(collection->name));

	if (lb) {
		BLI_addtail(lb, collection);
	}
	else {
		Collection *collection_master = BKE_collection_master(scene);
		BLI_addtail(&collection_master->collections, collection);
	}

	TODO_LAYER_SYNC;
	return collection;
}

/* free the collection items recursively */
static void collection_free(Collection *collection)
{
	for (LinkData *link = collection->objects.first; link; link = link->next) {
		id_us_min(link->data);
	}

	for (LinkData *link = collection->filter_objects.first; link; link = link->next) {
		id_us_min(link->data);
	}

	for (Collection *cl = collection->collections.first; cl; cl = cl->next) {
		collection_free(cl);
	}
	BLI_freelistN(&collection->collections);
}

/* unlink the collection recursively
 * return true if unlinked */
static bool collection_unlink(Collection *collection, Collection *gone)
{
	for (Collection *nc = collection->collections.first; nc; nc = nc->next)
	{
		if (nc == gone) {
			BLI_remlink(&collection->collections, gone);
			return true;
		}

		if (collection_unlink(nc, gone)) {
			return true;
		}
	}
	return false;
}

void BKE_collection_remove(Scene *scene, Collection *collection)
{
	Collection *master = BKE_collection_master(scene);

	/* unlink from the main collection tree */
	collection_unlink(master, collection);

	/* clear the collection items */
	collection_free(collection);

	/* TODO: check all layers that use this collection and clear them */
	TODO_LAYER_SYNC;

	MEM_freeN(collection);
}

Collection *BKE_collection_master(Scene *scene)
{
	return scene->collections.first;
}

void BKE_collection_object_add(struct Scene *scene, struct Collection *collection, struct Object *object)
{
	BLI_addtail(&collection->objects, BLI_genericNodeN(object));
	id_us_plus((ID *)object);
	TODO_LAYER_SYNC;
}

