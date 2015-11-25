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

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_bsdf_glossy_in[] = {
	{	SOCK_RGBA,  1, N_("Color"),		0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Roughness"),	0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_bsdf_glossy_out[] = {
	{	SOCK_SHADER, 0, N_("BSDF")},
	{	-1, 0, ""	}
};

static void node_shader_init_glossy(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->custom1 = SHD_GLOSSY_GGX;
}

static int node_shader_gpu_bsdf_glossy(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	Scene *scene = GPU_material_scene(mat);
	World *wo = scene->world;
	float horiz_col[4] = {0.2f};
	GPUNodeLink *envLink, *normalLink, *viewNormalLink, *roughnessLink;

	if (!in[2].link)
		in[2].link = GPU_builtin(GPU_VIEW_NORMAL);
	else {
		/* Convert to view space normal in case a Normal is plugged. This is because cycles uses world normals */
		GPU_link(mat, "node_vector_transform", in[2].link, GPU_builtin(GPU_VIEW_MATRIX), &in[2].link);
	}

	if (in[1].link)
		in[1].link = in[1].link;
	else
		in[1].link = GPU_uniform(in[1].vec);

	GPU_link(mat, "set_value", in[1].link, &roughnessLink);
	GPU_link(mat, "set_rgb", in[2].link, &viewNormalLink);
	GPU_link(mat, "node_vector_transform", in[2].link, GPU_builtin(GPU_INVERSE_VIEW_MATRIX), &normalLink); /* Send world normal for sampling */

	/* ENVIRONMENT PREVIEW */

	/* Use horizon color by default */
	horiz_col[0] = wo->horr;
	horiz_col[1] = wo->horg;
	horiz_col[2] = wo->horb;
	envLink = GPU_uniform(&horiz_col);

	/* If there is already a valid output do not attempt to do the world sampling. Because the output would be overwriten */
	if( GPU_material_get_output_link(mat) )
		return GPU_stack_link(mat, "node_bsdf_glossy", in, out, GPU_builtin(GPU_VIEW_POSITION), envLink);

	GPU_material_set_roughness_link(mat, roughnessLink); /* THIS IS BAD TOO */
	GPU_material_set_normal_link(mat, normalLink); /* THIS IS BAD TOO */

	/* First run the normal into the World node tree 
	 * The environment texture nodes will save the nodelink of it.
	 */
	GPU_material_set_type(mat, GPU_MATERIAL_TYPE_ENV_NORMAL); /* THIS IS BAD */

	/* XXX Memory leak here below */
	if (BKE_scene_use_new_shading_nodes(scene) && wo->nodetree && wo->use_nodes) {
		ntreeGPUMaterialNodes(wo->nodetree, mat, NODE_NEW_SHADING);
		GPU_material_empty_output_link(mat);
	} else {
		/* old fixed function world */
	}

	if (node->custom1 == SHD_GLOSSY_SHARP)
		GPU_material_set_type(mat, GPU_MATERIAL_TYPE_ENV_SAMPLING_SHARP); /* THIS IS BAD */
	else
		GPU_material_set_type(mat, GPU_MATERIAL_TYPE_ENV_SAMPLING_GLOSSY); /* THIS IS BAD */


	/* XXX Memory leak here below */
	if (BKE_scene_use_new_shading_nodes(scene) && wo->nodetree && wo->use_nodes) {
		ntreeGPUMaterialNodes(wo->nodetree, mat, NODE_NEW_SHADING);
		envLink = GPU_material_get_output_link(mat);
		GPU_material_empty_output_link(mat);
	} else {
		/* old fixed function world */
	}
	GPU_material_set_type(mat, GPU_MATERIAL_TYPE_MESH);
	
	return GPU_stack_link(mat, "node_bsdf_glossy", in, out, GPU_builtin(GPU_VIEW_POSITION), envLink);
}

/* node type definition */
void register_node_type_sh_bsdf_glossy(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_BSDF_GLOSSY, "Glossy BSDF", NODE_CLASS_SHADER, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_bsdf_glossy_in, sh_node_bsdf_glossy_out);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_init(&ntype, node_shader_init_glossy);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_bsdf_glossy);

	nodeRegisterType(&ntype);
}
