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

#include "BKE_collection.h"
#include "BKE_layer.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

#include "DNA_layer_types.h"
#include "DNA_scene_types.h"

/* prototype */
CollectionBase *collection_base_add(RenderLayer *rl, Collection *collection);

RenderLayer *BKE_render_layer_add(Scene *scene, const char *name)
{
	RenderLayer *rl = MEM_callocN(sizeof(RenderLayer), "Render Layer");
	BLI_strncpy(rl->name, name, sizeof(rl->name));

	Collection *collection = BKE_collection_master(scene);
	CollectionBase *collection_base = collection_base_add(rl, collection);
	return rl;
}

CollectionBase *collection_base_add(RenderLayer *rl, Collection *collection)
{
	CollectionBase *base = MEM_callocN(sizeof(CollectionBase), "Collection Base");
	BLI_addhead(&rl->collection_bases, base);

	base->collection = collection;
	base->flag = COLLECTION_VISIBLE + COLLECTION_SELECTABLE + COLLECTION_FOLDED;

	TODO_LAYER_SYNC;
	/* TODO: add objects and collections to sl->object_bases, and sl->collection_bases */
	return base;
}

