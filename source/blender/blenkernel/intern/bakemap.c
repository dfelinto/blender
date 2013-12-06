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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/bakemap.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_blender.h"
#include "BKE_bakemap.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_idprop.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif


/* -------------- Naming -------------- */

/* Find the first available, non-duplicate name for a given bake map */
void BKE_unique_bakemap_name(bBakeMap *bmap, ListBase *list)
{
	BLI_uniquename(list, bmap, DATA_("BakeMap"), '.', offsetof(bBakeMap, name), sizeof(bmap->name));
}

/* finds the 'active' constraint in a constraint stack */
bBakeMap *BKE_bakemaps_get_active(ListBase *list)
{
	bBakeMap *bmap;
	
	/* search for the first bake map with the 'active' flag set */
	if (list) {
		for (bmap = list->first; bmap; bmap = bmap->next) {
			if (bmap->flag & BAKEMAP_ACTIVE)
				return bmap;
		}
	}
	
	/* no active bake map found */
	return NULL;
}

/* Set the given bake map as the active one (clearing all the others) */
void BKE_bakemaps_set_active(ListBase *list, bBakeMap *bmap)
{
	bBakeMap *b;
	
	if (list) {
		for (b = list->first; b; b = b->next) {
			if (b == bmap) 
				b->flag |= BAKEMAP_ACTIVE;
			else 
				b->flag &= ~BAKEMAP_ACTIVE;
		}
	}
}


/* Remove the specified bake map from the given constraint stack */
int BKE_remove_bakemap(ListBase *list, bBakeMap *bmap)
{
	if (bmap) {
		//BKE_free_bakemap_data(bmap);
		BLI_freelinkN(list, bmap);
		return 1;
	}
	else
		return 0;
}

static bBakeMap *add_new_bakemap_internal(const char *name, short type)
{
	bBakeMap *bmap = MEM_callocN(sizeof(bBakeMap), "BakeMap");
	const char *newName;

	/* Set up a generic constraint datablock */
	bmap->type = type;

	/* if no name is provided, use the generic "Bake Map" name */
	/* NOTE: any bake map type that gets here really shouldn't get added... */
	newName = (name && name[0]) ? name : DATA_("Bake Map");
	
	/* copy the name */
	BLI_strncpy(bmap->name, newName, sizeof(bmap->name));
	
	/* return the new bake map */
	return bmap;
}

static bBakeMap *add_new_bakemap(Object *ob, const char *name, short type)
{
	bBakeMap *bmap;
	ListBase *list;

	/* add the bake map */
	bmap = add_new_bakemap_internal(name, type);

	/* find the bake map stack - bone or object? */
	list = &ob->bakemaps;

	if (list) {
		/* add new bake map to end of list of bake maps before ensuring that it has a unique name
		 * (otherwise unique-naming code will fail, since it assumes element exists in list)
		 */
		BLI_addtail(list, bmap);
		BKE_unique_bakemap_name(bmap, list);

		/* make this bake map the active one */
		BKE_bakemaps_set_active(list, bmap);
	}

	/* set type+owner specific immutable settings */
	switch (type) {
		case BAKEMAP_TYPE_DIFFUSE:
			break;
		case BAKEMAP_TYPE_SPECULAR:
			break;
	}
	
	return bmap;
}

/* Add new bake map for the given object */
bBakeMap *BKE_add_ob_bakemap(Object *ob, const char *name, short type)
{
	return add_new_bakemap(ob, name, type);
}
