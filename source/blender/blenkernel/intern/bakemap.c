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
#include "BLI_listbase.h"
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
void BKE_unique_bakemap_name(BakeMap *bmap, ListBase *list)
{
	BLI_uniquename(list, bmap, DATA_("BakeMap"), '.', offsetof(BakeMap, name), sizeof(bmap->name));
}

static void free_bakemap(BakeMap *bmap)
{
//	if (bmap->object)
//		id_us_min((ID *) bmap->object);

	MEM_freeN(bmap);
}

void BKE_free_bakemaps(ListBase *lb)
{
	BakeMap *bmap;

	while ((bmap = BLI_pophead(lb))) {
		free_bakemap(bmap);
	}
}

/* Remove the specified bake map from the given constraint stack */
int BKE_remove_bakemap(ListBase *list, BakeMap *bmap)
{
	if (bmap) {
		BLI_remlink(list, bmap);
		free_bakemap(bmap);
		return 1;
	}
	else
		return 0;
}

static BakeMap *add_new_bakemap_internal(const char *name, short type)
{
	BakeMap *bmap = MEM_callocN(sizeof(BakeMap), "BakeMap");
	const char *newName;

	/* Set up a generic constraint datablock */
	bmap->type = type;
	bmap->flag |= BAKEMAP_USE;

	/* if no name is provided, use the generic "Bake Map" name */
	/* NOTE: any bake map type that gets here really shouldn't get added... */
	newName = (name && name[0]) ? name : DATA_("Bake Map");
	
	/* copy the name */
	BLI_strncpy(bmap->name, newName, sizeof(bmap->name));
	
	/* return the new bake map */
	return bmap;
}

static BakeMap *add_new_bakemap(Object *ob, const char *name, short type)
{
	BakeMap *bmap;
	ListBase *list;

	/* add the bake map */
	bmap = add_new_bakemap_internal(name, type);
	list = &ob->bakemaps;

	/* add new bake map to end of list of bake maps before ensuring that it has a unique name
	 * (otherwise unique-naming code will fail, since it assumes element exists in list)
	 */
	BLI_addtail(list, bmap);
	BKE_unique_bakemap_name(bmap, list);

	/* make this bake map the active one */
	ob->actbakemap = BLI_countlist(list);

	/* set type+owner specific immutable settings */
	switch (type) {
		case BAKEMAP_TYPE_ALL:
			break;
		case BAKEMAP_TYPE_AO:
			break;
	}
	
	return bmap;
}

/* Add new bake map for the given object */
BakeMap *BKE_add_ob_bakemap(Object *ob, const char *name, short type)
{
	return add_new_bakemap(ob, name, type);
}
