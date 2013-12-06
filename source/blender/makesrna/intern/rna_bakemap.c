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
static EnumPropertyItem bakemap_type_items[] = {
	{BAKEMAP_TYPE_DIFFUSE, "DIFFUSE", 0, "Diffuse", "XXX"},
	{BAKEMAP_TYPE_SPECULAR, "SPECULAR", 0, "Specular", "XXX"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "BKE_bakemap.h"

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

static PointerRNA rna_Object_active_bakemap_get(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	bBakeMap *bakemap = BKE_bakemaps_get_active(&ob->bakemaps);
	return rna_pointer_inherit_refine(ptr, &RNA_BakeMap, bakemap);
}

static void rna_Object_active_bakemap_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object *)ptr->id.data;
	BKE_bakemaps_set_active(&ob->bakemaps, (bBakeMap *)value.data);
}

static bBakeMap *rna_Object_bakemap_new(Object *object, int type)
{
	//WM_main_add_notifier(NC_OBJECT | NA_ADDED, object);
	return BKE_add_ob_bakemap(object, NULL, type);
}

static void rna_Object_bakemap_remove(Object *object, ReportList *reports, PointerRNA *bakemap_ptr)
{
	bBakeMap *bmap = bakemap_ptr->data;
	if (BLI_findindex(&object->bakemaps, bmap) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Bake map '%s' not found in object '%s'", bmap->name, object->id.name + 2);
		return;
	}

	BKE_remove_bakemap(&object->bakemaps, bmap);
	RNA_POINTER_INVALIDATE(bakemap_ptr);

	//ED_object_bakemap_update(object);
	//ED_object_bakemap_set_active(object, NULL);
	//WM_main_add_notifier(NC_OBJECT | NA_REMOVED, object);
}

static int rna_Object_active_bakemap_index_get(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;

	return BLI_findindex(&ob->bakemaps, ptr->data);
}

static PointerRNA rna_Object_active_bakemap_index_set(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	return rna_pointer_inherit_refine(ptr, &RNA_BakeMap, BLI_findlink(&ob->bakemaps, ob->actbakemap - 1));
}

static void rna_Object_active_bakemap_index_range(PointerRNA *ptr, int *min, int *max,
                                                  int *UNUSED(softmin), int *UNUSED(softmax))
{
	Object *ob = (Object *)ptr->id.data;

	*min = 0;
	*max = max_ii(0, BLI_countlist(&ob->bakemaps) - 1);
}

#else

/* object.bake_maps */
static void rna_def_object_bakemaps(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "BakeMaps");
	srna = RNA_def_struct(brna, "BakeMaps", NULL);
	RNA_def_struct_sdna(srna, "Object");
	RNA_def_struct_ui_text(srna, "Bake Maps", "Collection of bake maps");

	//XXX if I enable any of those I can't build ...
#if 0
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BakeMap");
	RNA_def_property_pointer_funcs(prop, "rna_Object_active_bakemap_get",
	                               "rna_Object_active_bakemap_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Bake Map", "Bake maps of the object");
	RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Object_internal_update_data");
#endif
#if 0
	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "actdef");
	RNA_def_property_int_funcs(prop, "rna_Object_active_bakemap_index_get",
	                           "rna_Object_active_bakemap_index_set",
	                           "rna_Object_active_bakemap_index_range");
	RNA_def_property_ui_text(prop, "Active Bake Map Index", "Active index in bake map array");
	RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Object_internal_update_data");
#endif
#if 0
	/* bake maps */ /* add_bake_map */
	func = RNA_def_function(srna, "new", "rna_Object_bakemap_new");
	RNA_def_function_ui_description(func, "Add bake map to object");
	RNA_def_string(func, "name", "Bake Map", 0, "", "Bake map name"); /* optional */
	parm = RNA_def_pointer(func, "map", "BakeMap", "", "New bake map");
	RNA_def_function_return(func, parm);
#endif
#if 0
	func = RNA_def_function(srna, "remove", "rna_Object_bakemap_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Delete bake map from object");
	parm = RNA_def_pointer(func, "map", "BakeMap", "", "Bake map to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

#endif
}

void RNA_def_object_bakemaps(BlenderRNA *brna, PropertyRNA *cprop)
{
	rna_def_object_bakemaps(brna, cprop);
}

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
