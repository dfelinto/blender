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
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_dynamic_override.c
 *  \ingroup RNA
 */

#include "BLT_translation.h"

#include "BLI_math.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"

#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

const EnumPropertyItem rna_enum_dynamic_override_property_type_items[] = {
	{DYN_OVERRIDE_PROP_TYPE_SCENE, "SCENE", 0, "Scene", ""},
	{DYN_OVERRIDE_PROP_TYPE_COLLECTION, "COLLECTION", 0, "Collection", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "RNA_access.h"

#include "BKE_layer.h"

static void rna_DynamicOverrideProperty_name_get(PointerRNA *ptr, char *value)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	/* TODO: Get the name from data_path. */
	strcpy(value, dyn_prop->rna_path);
}

static int rna_DynamicOverrideProperty_length(PointerRNA *ptr)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	/* TODO: Get the name from data_path. */
	return strlen(dyn_prop->rna_path);
}

static PropertyRNA *rna_DynamicOverride_property_get(PointerRNA *ptr)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;

	/* Lazy building of RNA path elements if needed. */
	if (dyn_prop->data_path.first == NULL) {
		PointerRNA ptr_data;
		if (dyn_prop->root == NULL) {
			PointerRNA ptr_dummy;
			ID id_dummy;
			*((short *)id_dummy.name) = dyn_prop->id_type;
			RNA_id_pointer_create(&id_dummy, &ptr_dummy);
			RNA_pointer_create(NULL, rna_ID_refine(&ptr_dummy), NULL, &ptr_data);
		}
		else {
			RNA_id_pointer_create(dyn_prop->root, &ptr_data);
		}

		RNA_path_resolve_elements_no_data(&ptr_data, dyn_prop->rna_path, &dyn_prop->data_path);
	}

	PropertyElemRNA *prop_elem = dyn_prop->data_path.last;

	/* Note that we have no 'invalid' value for PropertyRNA type... :/ */
	return prop_elem ? prop_elem->prop : NULL;

}

static int rna_DynamicOverrideProperty_data_type_get(PointerRNA *ptr)
{
	PropertyRNA *prop = rna_DynamicOverride_property_get(ptr);
	return prop ? RNA_property_type(prop) : -1;
}

static int rna_DynamicOverrideProperty_data_subtype_get(PointerRNA *ptr)
{
	PropertyRNA *prop = rna_DynamicOverride_property_get(ptr);
	return prop ? RNA_property_subtype(prop) : -1;
}

static int rna_DynamicOverrideProperty_value_boolean_get(PointerRNA *ptr)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	return dyn_prop->data.i[0];
}

static void rna_DynamicOverrideProperty_value_boolean_set(PointerRNA *ptr, int value)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	dyn_prop->data.i[0] = value;
}

static int rna_DynamicOverrideProperty_value_int_get(PointerRNA *ptr)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	return dyn_prop->data.i[0];
}

static void rna_DynamicOverrideProperty_value_int_set(PointerRNA *ptr, int value)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	dyn_prop->data.i[0] = value;
}

static void rna_DynamicOverrideProperty_value_int_range(
        PointerRNA *ptr, int *min, int *max,
        int *UNUSED(softmin), int *UNUSED(softmax))
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	(void)dyn_prop;
	*min = INT_MIN;
	*max = INT_MAX;
}

static float rna_DynamicOverrideProperty_value_float_get(PointerRNA *ptr)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	return dyn_prop->data.f[0];
}

static void rna_DynamicOverrideProperty_value_float_set(PointerRNA *ptr, float value)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	dyn_prop->data.f[0] = value;
}

static void rna_DynamicOverrideProperty_value_float_range(
        PointerRNA *ptr, float *min, float *max,
        float *UNUSED(softmin), float *UNUSED(softmax))
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	(void)dyn_prop;
	*min = FLT_MIN;
	*max = FLT_MAX;
}

static int rna_DynamicOverrideProperty_value_color_get_length(PointerRNA *ptr, int UNUSED(length[RNA_MAX_ARRAY_DIMENSION]))
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	return dyn_prop->array_len;
}

static void rna_DynamicOverrideProperty_value_color_get(PointerRNA *ptr, float *values)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	for (int i = 0; i < dyn_prop->array_len; i++) {
		values[i] = dyn_prop->data.f[i];
	}
}

static void rna_DynamicOverrideProperty_value_color_set(PointerRNA *ptr, const float *values)
{
	DynamicOverrideProperty *dyn_prop = ptr->data;
	for (int i = 0; i < dyn_prop->array_len; i++) {
		dyn_prop->data.f[i] = values[i];
	}
}

#else

void RNA_def_dynamic_override(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem rna_enum_dynamic_override_mode_items[] = {
		{IDOVERRIDESTATIC_OP_REPLACE, "REPLACE", 0, "Replace", ""},
		{IDOVERRIDESTATIC_OP_MULTIPLY, "MULTIPLY", 0, "Multiply", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "DynamicOverrideProperty", NULL);
	RNA_def_struct_ui_text(srna, "Dynamic Override Property", "Properties overridden by override set");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop,
	                              "rna_DynamicOverrideProperty_name_get",
	                              "rna_DynamicOverrideProperty_length",
	                              NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "Property name");

	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", DYN_OVERRIDE_PROP_USE);
	RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the overridden property");
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "property_type");
	RNA_def_property_enum_items(prop, rna_enum_dynamic_override_property_type_items);
	RNA_def_property_ui_text(prop, "Type",
	                         "Whether the property affects the entire scene or the collection objects only");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "owner_id", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "root");
	RNA_def_property_ui_text(prop, "ID",
	                         "ID owner");

	prop = RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_id_type_items);
	RNA_def_property_ui_text(prop, "ID Type",
	                         "Type of ID block that owns this property");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "override_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "operation");
	RNA_def_property_enum_items(prop, rna_enum_dynamic_override_mode_items);
	RNA_def_property_ui_text(prop, "Override Mode",
	                         "Method of override the original values");
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);

	prop = RNA_def_property(srna, "multiply_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10.0f, 10.0f, 5, 3);
	RNA_def_property_ui_text(prop, "Factor", "Multiplication factor");
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);

	prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_property_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_funcs(prop, "rna_DynamicOverrideProperty_data_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Type", "Data type of the property");

	prop = RNA_def_property(srna, "data_subtype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_property_subtype_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_funcs(prop, "rna_DynamicOverrideProperty_data_subtype_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Subtype", "Data subtype of the property");

	/* Accessors for the different value types. */
	prop = RNA_def_property(srna, "value_boolean", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop,
	                               "rna_DynamicOverrideProperty_value_boolean_get",
	                               "rna_DynamicOverrideProperty_value_boolean_set");
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);

	prop = RNA_def_property(srna, "value_int", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop,
	                           "rna_DynamicOverrideProperty_value_int_get",
	                           "rna_DynamicOverrideProperty_value_int_set",
	                           "rna_DynamicOverrideProperty_value_int_range");
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);

	prop = RNA_def_property(srna, "value_float", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop,
	                             "rna_DynamicOverrideProperty_value_float_get",
	                             "rna_DynamicOverrideProperty_value_float_set",
	                             "rna_DynamicOverrideProperty_value_float_range");
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);

	prop = RNA_def_property(srna, "value_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_multi_array(prop, 1, NULL);
	RNA_def_property_dynamic_array_funcs(prop, "rna_DynamicOverrideProperty_value_color_get_length");
	RNA_def_property_float_funcs(prop,
	                             "rna_DynamicOverrideProperty_value_color_get",
	                             "rna_DynamicOverrideProperty_value_color_set",
	                             NULL);
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);
}

#endif
