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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../node_shader_util.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "GPU_material.h"
#include "DNA_material_types.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_bsdf_glass_in[] = {
	{	SOCK_RGBA, 1, N_("Color"),		1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Roughness"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("IOR"),		1.45f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_VECTOR, 1, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_bsdf_glass_out[] = {
	{	SOCK_SHADER, 0, N_("BSDF")},
	{	-1, 0, ""	}
};

#define IN_COLOR 0
#define IN_ROUGHNESS 1
#define IN_IOR 2
#define IN_NORMAL 3

static void node_shader_init_glass(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->custom1 = SHD_GLOSSY_BECKMANN;
}

/* XXX this is also done as a local static function in gpu_codegen.c,
 * but we need this to hack around the crappy material node.
 */
static GPUNodeLink *gpu_get_input_link(GPUNodeStack *in)
{
	if (in->link)
		return in->link;
	else
		return GPU_uniform(in->vec);
}

static int node_shader_gpu_bsdf_glass(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (!in[IN_NORMAL].link)
		in[IN_NORMAL].link = GPU_builtin(GPU_VIEW_NORMAL);
	else {
		/* Convert to view space normal in case a Normal is plugged. This is because cycles uses world normals */
		GPU_link(mat, "direction_transform_m4v3", in[IN_NORMAL].link, GPU_builtin(GPU_VIEW_MATRIX), &in[IN_NORMAL].link);
	}

	if (GPU_material_get_type(mat) == GPU_MATERIAL_TYPE_MESH_REAL_SH) {
		GPUBrdfInput brdf;

		GPU_brdf_input_initialize(&brdf);

		/* Refraction */
		brdf.mat       = mat;
		brdf.color     = gpu_get_input_link(&in[IN_COLOR]);
		brdf.roughness = gpu_get_input_link(&in[IN_ROUGHNESS]);
		brdf.ior       = gpu_get_input_link(&in[IN_IOR]);
		brdf.normal    = gpu_get_input_link(&in[IN_NORMAL]);

		if (node->custom1 == SHD_GLOSSY_BECKMANN)
			brdf.type = GPU_BRDF_GLASS_BECKMANN;
		else if (node->custom1 == SHD_GLOSSY_GGX)
			brdf.type = GPU_BRDF_GLASS_GGX;
		else
			brdf.type = GPU_BRDF_GLASS_SHARP;

		GPU_shade_BRDF(&brdf);

		out[0].link = brdf.output;
		return 1;
	}
	else
		return GPU_stack_link(mat, "node_bsdf_glass_lights", in, out, GPU_builtin(GPU_VIEW_POSITION), GPU_get_world_horicol());
}

/* node type definition */
void register_node_type_sh_bsdf_glass(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_BSDF_GLASS, "Glass BSDF", NODE_CLASS_SHADER, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_bsdf_glass_in, sh_node_bsdf_glass_out);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_init(&ntype, node_shader_init_glass);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_bsdf_glass);

	nodeRegisterType(&ntype);
}
