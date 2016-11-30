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

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

/*
 * Add a collection to a collection ListBase and syncronize all render layers
 * The ListBase is NULL when the collection is to be added to the master collection
 */
Collection *BKE_collection_add(Scene *scene, ListBase *lb, const char *name)
{
	Collection *cl = MEM_callocN(sizeof(Collection), "New Collection");
	BLI_strncpy(cl->name, name, sizeof(cl->name));
	BLI_uniquename(&scene->collections, cl, DATA_("Collection"), '.', offsetof(Collection, name), sizeof(cl->name));

	if (lb) {
		BLI_addtail(lb, cl);
	}
	else {
		Collection *cl_master = BKE_collection_master(scene);
		BLI_addtail(&cl_master->collections, cl);
	}

	TODO_LAYER_SYNC;
	return cl;
}

/* free the collection items recursively */
static void collection_free(Collection *cl)
{
	for (LinkData *link = cl->objects.first; link; link = link->next) {
		id_us_min(link->data);
	}

	for (LinkData *link = cl->filter_objects.first; link; link = link->next) {
		id_us_min(link->data);
	}

	for (Collection *ncl = cl->collections.first; ncl; ncl = ncl->next) {
		collection_free(ncl);
	}
	BLI_freelistN(&cl->collections);
}

/* unlink the collection recursively
 * return true if unlinked */
static bool collection_remlink(Collection *cl, Collection *cl_gone)
{
	for (Collection *ncl = cl->collections.first; ncl; ncl = ncl->next)
	{
		if (ncl == cl_gone) {
			BLI_remlink(&cl->collections, cl_gone);
			return true;
		}

		if (collection_remlink(ncl, cl_gone)) {
			return true;
		}
	}
	return false;
}

/*
 * Remove a collection from the scene, and syncronize all render layers
 */
void BKE_collection_remove(Scene *scene, Collection *cl)
{
	Collection *cl_master = BKE_collection_master(scene);

	/* unlink from the main collection tree */
	collection_remlink(cl_master, cl);

	/* clear the collection items */
	collection_free(cl);

	/* TODO: check all layers that use this collection and clear them */
	TODO_LAYER_SYNC;

	MEM_freeN(cl);
}

/*
 * Returns the master collection
 */
Collection *BKE_collection_master(Scene *scene)
{
	return scene->collections.first;
}

/*
 * Add object to collection
 */
void BKE_collection_object_add(struct Scene *UNUSED(scene), struct Collection *cl, struct Object *ob)
{
	BLI_addtail(&cl->objects, BLI_genericNodeN(ob));
	id_us_plus((ID *)ob);
	TODO_LAYER_SYNC;
}
