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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/versioning_280.c
 *  \ingroup blenloader
 */

/* for MinGW32 definition of NULL, could use BLI_blenlib.h instead too */
#include <stddef.h>

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_genfile.h"

#include "BKE_main.h"
#include "BKE_scene.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "MEM_guardedalloc.h"

void blo_do_versions_280(FileData *fd, Library *UNUSED(lib), Main *main)
{
	if (!DNA_struct_elem_find(fd->filesdna, "Scene", "ListBase", "layers")) {
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {
			SceneLayer *sl = MEM_callocN(sizeof(SceneLayer), "initial scene layer");
			BLI_strncpy(sl->name, "Layer", sizeof(sl->name));

			LayerCollection *collections[20] = {NULL};
			scene->active_layer = 0;

			BLI_addhead(&scene->layers, sl);
			int lay_used = 0;

			for (int i = 0; i < 20; i++) {
				LayerCollection *collection = MEM_callocN(sizeof(LayerCollection), "doversion layer collections");
				collection->flag = COLLECTION_SELECTABLE;

				if (scene->lay & (1 << i)) {
					collection->flag |= COLLECTION_VISIBLE;
				}

				BLI_snprintf(collection->name, sizeof(collection->name), "%d", i + 1);
				collections[i] = collection;
			}

			Base *base = scene->base.first;
			while (base) {
				Base *base_new = MEM_dupallocN(base);
				BLI_addtail(&sl->base, base_new);

				if (base == scene->basact) {
					sl->basact = base_new;
				}

				lay_used |= base->lay & ((1 << 20) - 1); /* ignore localview */

				for (int i = 0; i < 20; i++) {
					if ((base->lay & (1 << i)) != 0) {
						BLI_addtail(&collections[i]->elements, BLI_genericNodeN(base_new));
					}
				}
				base = base->next;
			}

			/* We should always have at least one layer, and one collection at all times */
			if (lay_used == 0) {
				lay_used  = (1 << 0);
			}

			/* Cleanup */
			for (int i = 0; i < 20; i++) {
				if ((lay_used & (1 << i)) != 0) {
					BLI_addtail(&sl->collections, collections[i]);
				}
				else {
					MEM_freeN(collections[i]);
				}
			}

			/*
			 * TODO handle existing SceneRenderLayer
			 * for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next);
			 */
		}
	}

}
