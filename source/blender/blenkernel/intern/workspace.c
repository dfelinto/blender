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
#include "BKE_main.h"
#include "BKE_workspace.h"

#include "DNA_screen_types.h"


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
	(void)ws;
}
