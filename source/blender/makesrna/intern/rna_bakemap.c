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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_bakemaps.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLF_translation.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_access.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

/* Always keep in alphabetical order */
EnumPropertyItem bakemap_type_items[] = {
	{BAKEMAP_TYPE_DIFFUSE, "DIFFUSE", 0, "Diffuse", "XXX"},
	{BAKEMAP_TYPE_SPECULAR, "SPECULAR", 0, "Specular", "XXX"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

static StructRNA *rna_BakeMap_refine(struct PointerRNA *ptr)
{
	bBakeMap *bmap = (bBakeMap *)ptr->data;

	switch (bmap->type) {
		case BAKEMAP_TYPE_DIFFUSE:
			return &RNA_DiffuseBakeMap;
		case BAKEMAP_TYPE_SPECULAR:
			return &RNA_SpecularBakeMap;
		default:
			return &RNA_BakeMap;
	}
}

static void rna_BakeMap_name_set(PointerRNA *ptr, const char *value)
{
	bBakeMap *bmap = (bBakeMap *)ptr->data;

	BLI_strncpy_utf8(bmap->name, value, sizeof(bmap->name));

	if (ptr->id.data) {
		Object *ob = (Object *)ptr->id.data;
		BLI_uniquename(&ob->bakemaps, bmap, DATA_("BakeMap"), '.', offsetof(bBakeMap, name), sizeof(bmap->name));
	}
}

#else

static void rna_def_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BakeMap", NULL);
	RNA_def_struct_ui_text(srna, "Bake Map", "XXX");
	RNA_def_struct_sdna(srna, "bBakeMap");
	RNA_def_struct_refine_func(srna, "rna_BakeMap_refine");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Bake map name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_BakeMap_name_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, bakemap_type_items);
	RNA_def_property_ui_text(prop, "Type", "XXX");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_diffuse_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;
	srna = RNA_def_struct(brna, "DiffuseBakeMap", "BakeMap");
	RNA_def_struct_ui_text(srna, "Diffuse Bake Map", "XXX");
}

static void rna_def_specular_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;
	srna = RNA_def_struct(brna, "SpecularBakeMap", "BakeMap");
	RNA_def_struct_ui_text(srna, "Specular Bake Map", "XXX");
}

void RNA_def_bakemap(BlenderRNA *brna)
{
	rna_def_bakemap(brna);

	rna_def_diffuse_bakemap(brna);
	rna_def_specular_bakemap(brna);
}

#endif
