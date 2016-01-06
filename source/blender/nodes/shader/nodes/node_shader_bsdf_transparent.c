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

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_bsdf_transparent_in[] = {
	{	SOCK_RGBA, 1, N_("Color"),		1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_bsdf_transparent_out[] = {
	{	SOCK_SHADER, 0, N_("BSDF")},
	{	-1, 0, ""	}
};

#define IN_COLOR 0

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

static int node_shader_gpu_bsdf_transparent(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (GPU_material_get_type(mat) == GPU_MATERIAL_TYPE_MESH_REAL_SH) {
		GPUBrdfInput brdf;

		GPU_brdf_input_initialize(&brdf);

		brdf.mat       = mat;
		brdf.type      = GPU_BRDF_TRANSPARENT;
		brdf.color     = gpu_get_input_link(&in[IN_COLOR]);

		GPU_shade_BRDF(&brdf);

		out[0].link = brdf.output;
		return 1;
	} 
	else {
		return GPU_stack_link(mat, "node_bsdf_transparent", in, out, GPU_get_world_horicol());
	}
}

/* node type definition */
void register_node_type_sh_bsdf_transparent(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_BSDF_TRANSPARENT, "Transparent BSDF", NODE_CLASS_SHADER, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_bsdf_transparent_in, sh_node_bsdf_transparent_out);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_bsdf_transparent);

	nodeRegisterType(&ntype);
}
