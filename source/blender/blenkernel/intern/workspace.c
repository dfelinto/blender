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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/workspace.c
 *  \ingroup bke
 */

#include "BLI_utildefines.h"

#include "BKE_library.h"
#include "BLI_listbase.h"
#include "BKE_main.h"
#include "BKE_workspace.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"


/* -------------------------------------------------------------------- */
/* Create, delete, init */

WorkSpace *BKE_workspace_add(Main *bmain, const char *name)
{
	WorkSpace *new_ws = BKE_libblock_alloc(bmain, ID_WS, name);
	return new_ws;
}

WorkSpace *BKE_workspace_duplicate(Main *bmain, const WorkSpace *from)
{
	WorkSpace *new_ws = BKE_libblock_alloc(bmain, ID_WS, from->id.name + 2);
	return new_ws;
}

void BKE_workspace_free(WorkSpace *ws)
{
	MEM_freeN(ws->act_layout);
}


/* -------------------------------------------------------------------- */
/* General Utils */

WorkSpaceLayout *BKE_workspace_layout_find(const WorkSpace *ws, const bScreen *screen)
{
	for (WorkSpaceLayout *layout = ws->layouts.first; layout; layout = layout->next) {
		if (layout->screen == screen) {
			return layout;
		}
	}

	BLI_assert(!"Couldn't find layout in this workspace. This should not happen!");
	return NULL;
}


/* -------------------------------------------------------------------- */
/* Getters/Setters */

bScreen *BKE_workspace_active_screen_get(const WorkSpace *ws)
{
	return ws->act_layout->screen;
}
void BKE_workspace_active_screen_set(WorkSpace *ws, bScreen *screen)
{
	/* we need to find the WorkspaceLayout that wraps this screen */
	ws->act_layout = BKE_workspace_layout_find(ws, screen);
}

Scene *BKE_workspace_active_scene_get(const WorkSpace *ws)
{
	return ws->act_layout->screen->scene;
}
void BKE_workspace_active_scene_set(WorkSpace *ws, Scene *scene)
{
	ws->act_layout->screen->scene = scene;
}


bScreen *BKE_workspace_layout_screen_get(WorkSpaceLayout *layout)
{
	return layout->screen;
}
