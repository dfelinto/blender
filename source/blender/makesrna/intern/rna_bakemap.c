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
/* Python keys (e.g., ALPHA) is used in the interface to name the bake map */
EnumPropertyItem bakemap_type_items[] = {
	{BAKEMAP_TYPE_ALPHA, "ALPHA", 0, "Alpha", "Bake Alpha values (transparency)"},
	{BAKEMAP_TYPE_AO, "AMBIENT_OCCLUSION", 0, "Ambient Occlusion", "Bake ambient occlusion"},
	{BAKEMAP_TYPE_DERIVATIVE, "DERIVATIVE", 0, "Derivative", "Bake derivative map"},
	{BAKEMAP_TYPE_DISPLACEMENT, "DISPLACEMENT", 0, "Displacement", "Bake displacement"},
	{BAKEMAP_TYPE_EMIT, "EMISSION", 0, "Emission", "Bake Emit values (glow)"},
	{BAKEMAP_TYPE_ALL, "FULL_RENDER", 0, "Full Render", "Bake everything"},
	{BAKEMAP_TYPE_MIRROR_COLOR, "MIRROR_COLOR", 0, "Mirror Colors", "Bake Mirror colors"},
	{BAKEMAP_TYPE_MIRROR_INTENSITY, "MIRROR_INTENSITY", 0, "Mirror Intensity", "Bake Mirror values"},
	{BAKEMAP_TYPE_NORMALS, "NORMALS", 0, "Normals", "Bake normals"},
	{BAKEMAP_TYPE_SHADOW, "SHADOW", 0, "Shadow", "Bake shadows"},
	{BAKEMAP_TYPE_SPEC_INTENSITY, "SPECULAR_INTENSITY", 0, "Specular Intensity", "Bake Specular values"},
	{BAKEMAP_TYPE_SPEC_COLOR, "SPECULAR_COLOR", 0, "Specular Colors", "Bake Specular colors"},
	{BAKEMAP_TYPE_TEXTURE, "TEXTURE", 0, "Textures", "Bake textures"},
	{BAKEMAP_TYPE_VERTEX_COLORS, "VERTEX_COLORS", 0, "Vertex Colors", "Bake vertex colors"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

static StructRNA *rna_BakeMap_refine(struct PointerRNA *ptr)
{
	BakeMap *bmap = (BakeMap *)ptr->data;

	switch (bmap->type) {
		case BAKEMAP_TYPE_ALL:
			return &RNA_FullRenderBakeMap;
		case BAKEMAP_TYPE_ALPHA:
			return &RNA_AlphaBakeMap;
		case BAKEMAP_TYPE_AO:
			return &RNA_AmbientOcclusionBakeMap;
		case BAKEMAP_TYPE_DERIVATIVE:
			return &RNA_DerivativeBakeMap;
		case BAKEMAP_TYPE_DISPLACEMENT:
			return &RNA_DisplacementBakeMap;
		case BAKEMAP_TYPE_EMIT:
			return &RNA_EmitBakeMap;
		case BAKEMAP_TYPE_MIRROR_COLOR:
			return &RNA_MirrorColorBakeMap;
		case BAKEMAP_TYPE_MIRROR_INTENSITY:
			return &RNA_MirrorIntensityBakeMap;
		case BAKEMAP_TYPE_NORMALS:
			return &RNA_NormalsBakeMap;
		case BAKEMAP_TYPE_SHADOW:
			return &RNA_ShadowBakeMap;
		case BAKEMAP_TYPE_SPEC_COLOR:
			return &RNA_SpecularColorBakeMap;
		case BAKEMAP_TYPE_SPEC_INTENSITY:
			return &RNA_SpecularIntensityBakeMap;
		case BAKEMAP_TYPE_TEXTURE:
			return &RNA_TextureBakeMap;
		case BAKEMAP_TYPE_VERTEX_COLORS:
			return &RNA_VertexColorsBakeMap;
		default:
			return &RNA_BakeMap;
	}
}

static void rna_BakeMap_name_set(PointerRNA *ptr, const char *value)
{
	BakeMap *bmap = (BakeMap *)ptr->data;

	BLI_strncpy_utf8(bmap->name, value, sizeof(bmap->name));

	if (ptr->id.data) {
		Object *ob = (Object *)ptr->id.data;
		BLI_uniquename(&ob->bakemaps, bmap, DATA_("BakeMap"), '.', offsetof(BakeMap, name), sizeof(bmap->name));
	}
}

#else

static StructRNA *rna_def_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BakeMap", NULL);
	RNA_def_struct_ui_text(srna, "Bake Map", "");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_refine_func(srna, "rna_BakeMap_refine");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Bake map name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_BakeMap_name_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_OBJECT, NULL);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, bakemap_type_items);
	RNA_def_property_ui_text(prop, "Type", "XXX");
	RNA_def_property_update(prop, NC_OBJECT, NULL);

    prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_USE);
    RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the bake map");
    RNA_def_property_update(prop, NC_OBJECT, NULL);

	/*
	 image/vertex color
	 image datablock/external

	 image filepath
	 uv
	 */

	/*
	prop = RNA_def_property(srna, "use_bake_multires", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_MULTIRES);
	RNA_def_property_ui_text(prop, "Bake from Multires", "Bake directly from multires object");
	RNA_def_property_update(prop, NC_OBJECT, NULL);
	 */

	//XXX bake_distance
	//XXX bake_bias

	return srna;
}

static void rna_def_alpha_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "AlphaBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Alpha Bake Map", "Bake Alpha values (transparency)");
}

static void rna_def_ambient_occlusion_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "AmbientOcclusionBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Ambient Occlusion Bake Map", "Bake ambient occlusion");

	prop = RNA_def_property(srna, "use_multires", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_MULTIRES);
	RNA_def_property_ui_text(prop, "Bake from Multires", "Bake directly from multires object");
	RNA_def_property_update(prop, NC_OBJECT, NULL);

	prop = RNA_def_property(srna, "use_normalize", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_NORMALIZE);
	RNA_def_property_ui_text(prop, "Normalized",
	                         "Normalize without using material settings");
	RNA_def_property_update(prop, NC_OBJECT, NULL);

	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samples");
	RNA_def_property_range(prop, 64, 1024);
	RNA_def_property_int_default(prop, 256);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples used for ambient occlusion baking from multires");
	RNA_def_property_update(prop, NC_OBJECT, NULL);
}

static void rna_def_derivative_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DerivativeBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Derivative Bake Map", "Bake derivative map");

	prop = RNA_def_property(srna, "use_multires", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_MULTIRES);
	RNA_def_property_ui_text(prop, "Bake from Multires", "Bake directly from multires object");
	RNA_def_property_update(prop, NC_OBJECT, NULL);

	prop = RNA_def_property(srna, "use_user_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_USERSCALE);
	RNA_def_property_ui_text(prop, "User scale", "Use a user scale for the derivative map");

	prop = RNA_def_property(srna, "user_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "user_scale");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Scale",
	                         "Instead of automatically normalizing to 0..1, "
	                         "apply a user scale to the derivative map");
}

static void rna_def_displacement_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DisplacementBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Displacement Bake Map", "Bake displacement");

	prop = RNA_def_property(srna, "use_multires", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_MULTIRES);
	RNA_def_property_ui_text(prop, "Bake from Multires", "Bake directly from multires object");
	RNA_def_property_update(prop, NC_OBJECT, NULL);

	prop = RNA_def_property(srna, "use_normalize", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_NORMALIZE);
	RNA_def_property_ui_text(prop, "Normalized",
	                         "Normalize to the distance");
	RNA_def_property_update(prop, NC_OBJECT, NULL);

	prop = RNA_def_property(srna, "use_lores_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_LORES_MESH);
	RNA_def_property_ui_text(prop, "Low Resolution Mesh",
	                         "Calculate heights against unsubdivided low resolution mesh");
	RNA_def_property_update(prop, NC_OBJECT, NULL);
}

static void rna_def_emit_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "EmitBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Emit Bake Map", "Bake Emit values (glow)");
}

static void rna_def_full_render_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "FullRenderBakeMap", "BakeMap");
	RNA_def_struct_ui_text(srna, "Full Render Bake Map", "Bake everything");
}

static void rna_def_mirror_color_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "MirrorColorBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Mirror Color Bake Map", "Bake mirror colors");
}

static void rna_def_mirror_intensity_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "MirrorIntensityBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Mirror Intensity Bake Map", "Bake mirror intensity");
}

static void rna_def_normals_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem bake_normal_space_items[] = {
		{BAKEMAP_SPACE_CAMERA, "CAMERA", 0, "Camera", "Bake the normals in camera space"},
		{BAKEMAP_SPACE_WORLD, "WORLD", 0, "World", "Bake the normals in world space"},
		{BAKEMAP_SPACE_OBJECT, "OBJECT", 0, "Object", "Bake the normals in object space"},
		{BAKEMAP_SPACE_TANGENT, "TANGENT", 0, "Tangent", "Bake the normals in tangent space"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "NormalsBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Normals Bake Map", "Bake normals");

	prop = RNA_def_property(srna, "use_multires", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BAKEMAP_MULTIRES);
	RNA_def_property_ui_text(prop, "Bake from Multires", "Bake directly from multires object");
	RNA_def_property_update(prop, NC_OBJECT, NULL);

	prop = RNA_def_property(srna, "normal_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_space");
	RNA_def_property_enum_items(prop, bake_normal_space_items);
	RNA_def_property_ui_text(prop, "Normal Space", "Choose normal space for baking");
	RNA_def_property_update(prop, NC_OBJECT, NULL);
}

static void rna_def_shadow_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "ShadowBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Shadow Bake Map", "Bake shadows");
}

static void rna_def_specular_color_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "SpecularColorBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Specular Color Bake Map", "Bake specular colors");
}

static void rna_def_specular_intensity_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "SpecularIntensityBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Specular Intensity Bake Map", "Bake specular intensity");
}

static void rna_def_texture_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "TextureBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Texture Bake Map", "Bake textures");
}

static void rna_def_vertex_colors_bakemap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "VertexColorsBakeMap", "BakeMap");
	RNA_def_struct_sdna(srna, "BakeMap");
	RNA_def_struct_ui_text(srna, "Vertex Colors Bake Map", "Bake vertex colors");
}

void RNA_def_bakemap(BlenderRNA *brna)
{
	rna_def_bakemap(brna);

	rna_def_alpha_bakemap(brna);
	rna_def_ambient_occlusion_bakemap(brna);
	rna_def_derivative_bakemap(brna);
	rna_def_displacement_bakemap(brna);
	rna_def_emit_bakemap(brna);
	rna_def_full_render_bakemap(brna);
	rna_def_mirror_color_bakemap(brna);
	rna_def_mirror_intensity_bakemap(brna);
	rna_def_normals_bakemap(brna);
	rna_def_shadow_bakemap(brna);
	rna_def_specular_color_bakemap(brna);
	rna_def_specular_intensity_bakemap(brna);
	rna_def_texture_bakemap(brna);
	rna_def_vertex_colors_bakemap(brna);
}

#endif
