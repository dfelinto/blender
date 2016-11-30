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

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

/* prototype */
CollectionBase *collection_base_add(RenderLayer *rl, ListBase *lb, Collection *collection);

/* RenderLayer */

RenderLayer *BKE_render_layer_add(Scene *scene, const char *name)
{
	RenderLayer *rl = MEM_callocN(sizeof(RenderLayer), "Render Layer");
	BLI_strncpy(rl->name, name, sizeof(rl->name));

	Collection *cl = BKE_collection_master(scene);
	collection_base_add(rl, &rl->collection_bases, cl);
	return rl;
}

void BKE_render_layer_engine_set(struct RenderLayer *rl, const char *engine)
{
	BLI_strncpy(rl->engine, engine, sizeof(rl->engine));
}

/* ObjectBase */

static void render_layer_object_base_unref(RenderLayer* rl, ObjectBase *base)
{
	base->refcount--;

	/* It only exists in the RenderLayer */
	if (base->refcount == 1) {
		if (rl->basact == base) {
			rl->basact = NULL;
		}
	}

	BLI_remlink(&rl->object_bases, base);
	MEM_freeN(base);
}

static ObjectBase *object_base_add(RenderLayer *rl, Object *ob)
{
	ObjectBase *base = MEM_callocN(sizeof(ObjectBase), "Object Base");
	base->object = ob;
	BLI_addtail(&rl->object_bases, base);
	return base;
}

/* CollectionBase */

CollectionBase *BKE_collection_link(RenderLayer *rl, Collection *cl)
{
	CollectionBase *base = collection_base_add(rl, &rl->collection_bases, cl);
	return base;
}

static void collection_base_free(RenderLayer *rl, CollectionBase *cb)
{
	for (CollectionBase *ncb = cb->collection_bases.first; ncb; ncb = ncb->next) {
		for (LinkData *link = ncb->object_bases.first; link; link = link->data) {
			render_layer_object_base_unref(rl, link->data);
		}

		BLI_freelistN(&ncb->object_bases);
		collection_base_free(rl, ncb);
	}
}

void BKE_collection_unlink(RenderLayer *rl, CollectionBase *cb)
{
	collection_base_free(rl, cb);

	BLI_remlink(&rl->collection_bases, cb);
	MEM_freeN(cb);
}

static void object_base_populate(RenderLayer *rl, CollectionBase *cb, ListBase *objects)
{
	for (LinkData *link = objects->first; link; link = link->next) {
		ObjectBase *base = BLI_findptr(&rl->object_bases, link->data, offsetof(ObjectBase, object));

		if (base == NULL) {
			base = object_base_add(rl, link->data);
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

static void collection_base_populate(RenderLayer *rl, CollectionBase *cb, Collection *cl)
{
	object_base_populate(rl, cb, &cl->objects);
	object_base_populate(rl, cb, &cl->filter_objects);

	for (Collection *ncl = cl->collections.first; ncl; ncl = ncl->next) {
		collection_base_add(rl, &cb->collection_bases, ncl);
	}
}

CollectionBase *collection_base_add(RenderLayer *rl, ListBase *lb, Collection *cl)
{
	CollectionBase *cb = MEM_callocN(sizeof(CollectionBase), "Collection Base");
	BLI_addtail(lb, cb);

	cb->collection = cl;
	cb->flag = COLLECTION_VISIBLE + COLLECTION_SELECTABLE + COLLECTION_FOLDED;

	collection_base_populate(rl, cb, cl);
	return cb;
}

/* Override */

void BKE_collection_override_datablock_add(CollectionBase *UNUSED(cb), const char *UNUSED(data_path), ID *UNUSED(id))
{
	TODO_LAYER_OVERRIDE;
}
