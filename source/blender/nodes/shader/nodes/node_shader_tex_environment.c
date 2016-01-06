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

static bNodeSocketTemplate sh_node_tex_environment_in[] = {
	{	SOCK_VECTOR, 1, N_("Vector"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_environment_out[] = {
	{	SOCK_RGBA, 0, N_("Color"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
	{	-1, 0, ""	}
};

void getVectorFromLatLong(float u, float v, float *vec)
{
	vec[0] = (float)( -sin(v*M_PI) * cos(u*2.0f*M_PI) ); /* blender X */
	vec[2] = (float)( sin(v*M_PI) * sin(u*2.0f*M_PI) ); /* blender Y */
	vec[1] = (float)( -cos(v*M_PI) ); /* blender Z */
}

static void getTexelVectorSolidAngle(int x, int y, int width, int height, float *texelVect)
{
	float u, v, x0, y0, x1, y1;
	float halfTexelSize[2];
	float cornerVect[4][3];
	float edgeVect0[3];
	float edgeVect1[3];
	float xProdVect[3];

	halfTexelSize[0] = 0.5f / (float)(width);
	halfTexelSize[1] = 0.5f / (float)(height);
	u = ((float)(x) + 0.5f) / (float)(width);
	v = ((float)(y) + 0.5f) / (float)(height);

	getVectorFromLatLong(u, v, texelVect);
	getVectorFromLatLong(u - halfTexelSize[0], v - halfTexelSize[1], cornerVect[0]);
	getVectorFromLatLong(u + halfTexelSize[0], v - halfTexelSize[1], cornerVect[1]);
	getVectorFromLatLong(u + halfTexelSize[0], v + halfTexelSize[1], cornerVect[2]);
	getVectorFromLatLong(u - halfTexelSize[0], v + halfTexelSize[1], cornerVect[3]);

	sub_v3_v3v3(edgeVect0, cornerVect[3], cornerVect[0] );
	sub_v3_v3v3(edgeVect1, cornerVect[1], cornerVect[0] );
	cross_v3_v3v3(xProdVect, edgeVect0, edgeVect1 );
	texelVect[3] = 0.5f * sqrt( dot_v3v3(xProdVect, xProdVect ) );

	sub_v3_v3v3(edgeVect0, cornerVect[3], cornerVect[2] );
	sub_v3_v3v3(edgeVect1, cornerVect[1], cornerVect[2] );
	cross_v3_v3v3(xProdVect, edgeVect0, edgeVect1 );
	texelVect[3] += 0.5f * sqrt( dot_v3v3(xProdVect, xProdVect ) );
}

static void SHFilter(ImBuf *ibuf, float *SHCoef)
{
	const int NUM_SH_COEFFICIENT = 9;
	int channels = 4, ofs;
	char *rect;
	float texelVect[4] = {0.0f};
	float SHdir[9] = {0.0f};
	float weightAccum = 0.0f;
	float weight = 0.0f;
	int x,y,i;
	float r,g,b, xV,yV,zV;

	if (ibuf->channels != 0)
		channels = ibuf->channels;

	for (y = 0; y < ibuf->y; y++)
	{
		for (x = 0; x < ibuf->x; x++)
		{
			ofs = y*ibuf->x + x;

			getTexelVectorSolidAngle(x, y, ibuf->x, ibuf->y, &texelVect);

			weight = texelVect[3];   

			r = xV = texelVect[0];
			g = yV = texelVect[1];
			b = zV = texelVect[2];

			SHdir[0] = (float)(0.282095f);

			SHdir[1] = (float)(-0.488603f * yV * 2.0f/3.0f);
			SHdir[2] = (float)(0.488603f * zV * 2.0f/3.0f);
			SHdir[3] = (float)(-0.488603f * xV * 2.0f/3.0f);

			SHdir[4] = (float)(1.092548f * xV * yV * 1.0f/4.0f);
			SHdir[5] = (float)(-1.092548f * yV * zV * 1.0f/4.0f);
			SHdir[6] = (float)(0.315392f * (3.0f * zV * zV - 1.0f) * 1.0f/4.0f);
			SHdir[7] = (float)(-1.092548f * xV * zV * 1.0f/4.0f);
			SHdir[8] = (float)(0.546274f * (xV * xV - yV * yV) * 1.0f/4.0f);
			
			if (ibuf->rect_float) {
				if (ibuf->channels > 2) {
					r = ibuf->rect_float[channels*ofs+0];
					g = ibuf->rect_float[channels*ofs+1];
					b = ibuf->rect_float[channels*ofs+2];
				}
				else {
					r = g = b = ibuf->rect_float[ofs];
				}
			}else{
				rect = (char *)(ibuf->rect + ofs);
				r = ((float)rect[0])*(1.0f/255.0f);
				g = ((float)rect[1])*(1.0f/255.0f);
				b = ((float)rect[2])*(1.0f/255.0f);
			}

			for (i = 0; i < NUM_SH_COEFFICIENT; i++)
			{
				SHCoef[i*3+0] += r * SHdir[i] * weight;
				SHCoef[i*3+1] += g * SHdir[i] * weight;
				SHCoef[i*3+2] += b * SHdir[i] * weight;
			}

			weightAccum += weight;
		}
	}
	
	/* Normalization - The sum of solid angle should be equal to the solid angle of the sphere (4 PI), so
	 * normalize in order our weightAccum exactly match 4 PI. */

	for (i = 0; i < NUM_SH_COEFFICIENT; ++i)
	{
	 	SHCoef[i*3+0] *= 4.0f * M_PI / weightAccum;
	 	SHCoef[i*3+1] *= 4.0f * M_PI / weightAccum;
	 	SHCoef[i*3+2] *= 4.0f * M_PI / weightAccum;
	}
}

static void node_shader_init_tex_environment(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeTexEnvironment *tex = MEM_callocN(sizeof(NodeTexEnvironment), "NodeTexEnvironment");
	BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
	BKE_texture_colormapping_default(&tex->base.color_mapping);
	tex->color_space = SHD_COLORSPACE_COLOR;
	tex->projection = SHD_PROJ_EQUIRECTANGULAR;
	tex->iuser.frames = 1;
	tex->iuser.sfra = 1;
	tex->iuser.fie_ima = 2;
	tex->iuser.ok = 1;

	node->storage = tex;
}

static int node_shader_gpu_tex_environment(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	Image *ima = (Image *)node->id;
	ImageUser *iuser = NULL;
	NodeTexEnvironment *tex = node->storage;
	int isdata = tex->color_space == SHD_COLORSPACE_NONE;
	int ret;
	GPUMatType type = GPU_material_get_type(mat);
	GPUNodeLink *normal_transformed;
	GPUBrdfInput *brdf = NULL;
	bool isBackground = (type == GPU_MATERIAL_TYPE_WORLD || type == GPU_MATERIAL_TYPE_WORLD_SH);

	if (type == GPU_MATERIAL_TYPE_ENV_NORMAL) {
		if (in[0].link) {
			GPU_link(mat, "set_rgb", in[0].link, &normal_transformed);
			/* Should use another bridge. Preferably a per EnvTex Node Bridge.
			 * Currently only one custom normal get back from the graph.
			 */
			GPU_material_set_normal_link(mat, normal_transformed); /* THIS IS BAD TOO */
		}

		return GPU_stack_link(mat, "node_tex_environment_empty", in, out);
	}

	if (!ima)
		return GPU_stack_link(mat, "node_tex_environment_empty", in, out);

	if (!in[0].link) {
		if (type == GPU_MATERIAL_TYPE_MESH)
			in[0].link = GPU_builtin(GPU_VIEW_POSITION);
		else if (isBackground)
			GPU_link(mat, "background_transform_to_world", GPU_builtin(GPU_VIEW_POSITION), &in[0].link);
		else
			GPU_link(mat, "background_sampling_default", GPU_builtin(GPU_VIEW_POSITION), GPU_builtin(GPU_INVERSE_VIEW_MATRIX), &in[0].link);
	}

	if (type == GPU_MATERIAL_TYPE_ENV_BRDF) {
		GPU_link(mat, "set_rgb", GPU_material_get_normal_link(mat), &normal_transformed);
		brdf = GPU_material_get_brdf_link(mat);
		if (brdf->type == GPU_BRDF_TRANSLUCENT)
			GPU_link(mat, "viewN_to_shadeN", normal_transformed, &normal_transformed);
	}

	node_shader_gpu_tex_mapping(mat, node, in, out);

	if (type == GPU_MATERIAL_TYPE_WORLD_SH || (brdf && (brdf->type == GPU_BRDF_DIFFUSE || brdf->type == GPU_BRDF_TRANSLUCENT))) {
		// SH with NORMALS
		float shCoef[9][3] = { { 0.0f } };

		ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
		if (ibuf) {
			bool projection_has_changed = (ima->last_projection && (ima->last_projection != tex->projection + 1) );

			/* If no data or data is outdated */
			if (!ima->SH_Coefs[0][0] || projection_has_changed) {
				/* Compute SH */
				SHFilter(ibuf, &shCoef);
				/* Store SH */
				memcpy(ima->SH_Coefs, shCoef, 9 * 3 * sizeof(float));
			}
			else{
				/* Get from Image Data */
				memcpy(shCoef, ima->SH_Coefs, 9 * 3 * sizeof(float));
			}

			ima->last_projection = tex->projection + 1;
		}
		BKE_image_release_ibuf(ima, ibuf, NULL);

		return GPU_link(mat, "irradianceFromSH", (isBackground) ? in[0].link : normal_transformed, GPU_uniform(shCoef[0]), 
							 GPU_uniform(shCoef[1]), GPU_uniform(shCoef[2]), GPU_uniform(shCoef[3]), GPU_uniform(shCoef[4]), 
							 GPU_uniform(shCoef[5]), GPU_uniform(shCoef[6]), GPU_uniform(shCoef[7]), GPU_uniform(shCoef[8]), 
							 &out[0].link);

	} else if (brdf && brdf->type == GPU_BRDF_GLOSSY_GGX) {
		// REFLECTION GLOSSY
		float maxLod = 0.0f;
		float precalcLodFactor = 0.0f;
		float sampleNumber = 32.0f; /* Hardcoded for now */

		ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
		if (ibuf) {
			maxLod = log((float)ibuf->y)/log(2);
			precalcLodFactor = 0.5f * log( (float)ibuf->x * (float)ibuf->y / sampleNumber) / log(2);
			precalcLodFactor -= 0.5f; /* Biasing the factor to get more accurate sampling */
		}
		BKE_image_release_ibuf(ima, ibuf, NULL);

		return GPU_link(mat, "env_sampling_reflect_glossy", in[0].link, normal_transformed, 
						brdf->roughness, GPU_uniform(&precalcLodFactor), GPU_uniform(&maxLod), GPU_builtin(GPU_INVERSE_VIEW_MATRIX), 
						GPU_image(ima, iuser, isdata, true), &out[0].link);

	} else {
		float zero = 0.0f;
		GPUNodeLink *lodLink = GPU_uniform(&zero);

		if (brdf && brdf->type == GPU_BRDF_GLOSSY_SHARP) {
			GPU_link(mat, "env_sampling_reflect_sharp", in[0].link, normal_transformed, GPU_builtin(GPU_INVERSE_VIEW_MATRIX), &in[0].link);
		}
		else if (brdf && brdf->type == GPU_BRDF_REFRACT_SHARP) {
			GPU_link(mat, "env_sampling_refract_sharp", in[0].link, normal_transformed, brdf->ior, &in[0].link);
		}
		
		if (tex->projection == SHD_PROJ_EQUIRECTANGULAR)
			ret = GPU_stack_link(mat, "node_tex_environment_equirectangular", in, out, GPU_image(ima, iuser, isdata, true), lodLink);
		else
			ret = GPU_stack_link(mat, "node_tex_environment_mirror_ball", in, out, GPU_image(ima, iuser, isdata, true), lodLink);
	}


	if (ret) {
		ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
		if (ibuf && (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) == 0 &&
			GPU_material_do_color_management(mat))
		{
			GPU_link(mat, "srgb_to_linearrgb", out[0].link, &out[0].link);
		}
		BKE_image_release_ibuf(ima, ibuf, NULL);
	}

	return ret;
}

/* node type definition */
void register_node_type_sh_tex_environment(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_TEX_ENVIRONMENT, "Environment Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_environment_in, sh_node_tex_environment_out);
	node_type_init(&ntype, node_shader_init_tex_environment);
	node_type_storage(&ntype, "NodeTexEnvironment", node_free_standard_storage, node_copy_standard_storage);
	node_type_gpu(&ntype, node_shader_gpu_tex_environment);

	nodeRegisterType(&ntype);
}
