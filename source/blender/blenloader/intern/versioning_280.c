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
 *
 */

/** \file blender/blenloader/intern/versioning_280.c
 *  \ingroup blenloader
 */

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_object_types.h"
#include "DNA_layer_types.h"
#include "DNA_scene_types.h"
#include "DNA_genfile.h"

#include "BKE_collection.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLO_readfile.h"
#include "readfile.h"

#include "MEM_guardedalloc.h"

void blo_do_versions_280(FileData *fd, Library *UNUSED(lib), Main *main)
{
	if (!DNA_struct_elem_find(fd->filesdna, "Scene", "ListBase", "render_layers")) {
		for (Scene *scene = main->scene.first; scene; scene = scene->id.next) {

			BKE_collection_add(scene, &scene->collections, "Master Collection");
			RenderLayer *rl = BKE_render_layer_add(scene, "Render Layer");

			rl->active_collection = 0;
			rl->actbase = NULL;

			Collection *collections[20] = {NULL};
			bool is_visible[20];

			int lay_used = 0;
			for (int i = 0; i < 20; i++) {
				char name[MAX_NAME];

				BLI_snprintf(name, sizeof(collections[i]->name), "%d", i + 1);
				collections[i] = BKE_collection_add(scene, NULL, name);

				is_visible[i] = (scene->lay & (1 << i));
			}

			for (Base *base = scene->base.first; base; base = base->next) {
				lay_used |= base->lay & ((1 << 20) - 1); /* ignore localview */

				for (int i = 0; i < 20; i++) {
					if ((base->lay & (1 << i)) != 0) {
						BKE_collection_object_add(scene, collections[i], base->object);
					}
				}
			}

			/* TODO
			 * we need to go over all the CollectionBases created for this rl, and the original rls,
			 * and set visibility of collections accordingly
			 * */

			/* TODO
			 * handle existing SceneRenderLayer
			 * for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next);
			 */

			/* Cleanup */
			for (int i = 0; i < 20; i++) {
				if ((lay_used & (1 << i)) == 0) {
					BKE_collection_remove(scene, collections[i]);
				}
			}
		}
	}
}

