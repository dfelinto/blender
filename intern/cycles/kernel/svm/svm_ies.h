/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* IES Light */

#define I_AT(h, v) (kernel_tex_fetch(__ies, ofs+(h)*v_num+(v)))
/* Cubic interpolation along v at fixed h (symmetric at the lower boundary) */
#define V_INTERP(h, v, x) cubic_interp(I_AT(h, (((v) > 0)? (v)-1: (v)+1)), I_AT(h, v), I_AT(h, (v)+1), I_AT(h, ((((v)+2) < v_num)? (v)+2: (v)+1)), x)
ccl_device_inline float kernel_ies_interp(KernelGlobals *kg, int slot, float h_angle, float v_angle)
{
	if(kernel_data.integrator.ies_stride == 0)
		return 0.0f;

	int ofs = slot * kernel_data.integrator.ies_stride;
	int h_num = __float_as_int(kernel_tex_fetch(__ies, ofs++));
	int v_num = __float_as_int(kernel_tex_fetch(__ies, ofs++));

	if(h_angle >= kernel_tex_fetch(__ies, ofs      ) && h_angle <= kernel_tex_fetch(__ies, ofs+h_num-1      ) &&
	   v_angle >= kernel_tex_fetch(__ies, ofs+h_num) && v_angle <= kernel_tex_fetch(__ies, ofs+h_num+v_num-1)) {
		int h_i, v_i;
		for(h_i = 0; h_i < h_num-1; h_i++)
			if(kernel_tex_fetch(__ies, ofs+h_i) <= h_angle && kernel_tex_fetch(__ies, ofs+h_i+1) > h_angle)
				break;
		for(v_i = 0; v_i < v_num-1; v_i++)
			if(kernel_tex_fetch(__ies, ofs+h_num+v_i) <= v_angle && kernel_tex_fetch(__ies, ofs+h_num+v_i+1) > v_angle)
				break;

		float h_frac = (h_angle - kernel_tex_fetch(__ies, ofs+h_i)) / (kernel_tex_fetch(__ies, ofs+h_i+1) - kernel_tex_fetch(__ies, ofs+h_i));
		ofs += h_num;
		float v_frac = (v_angle - kernel_tex_fetch(__ies, ofs+v_i)) / (kernel_tex_fetch(__ies, ofs+v_i+1) - kernel_tex_fetch(__ies, ofs+v_i));
		ofs += v_num;

		/* Cubic interpolation along h (wraps around since h is longitude) */
		return cubic_interp(V_INTERP((h_i > 0)? h_i-1: h_num-2, v_i, v_frac), V_INTERP(h_i, v_i, v_frac), V_INTERP(h_i+1, v_i, v_frac), V_INTERP((h_i+2 < h_num)? h_i+2: 1, v_i, v_frac), h_frac);
	}

	return 0.0f;
}
#undef I_AT
#undef V_INTERP

ccl_device void svm_node_ies(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint vector_offset, strength_offset, fac_offset, dummy, slot = node.z;
	decode_node_uchar4(node.y, &strength_offset, &vector_offset, &fac_offset, &dummy);

	float3 vector = stack_load_float3_default(stack, vector_offset, ccl_fetch(sd, I));
	float strength = stack_load_float_default(stack, strength_offset, node.w);

	if(ccl_fetch(sd, type) == PRIMITIVE_LAMP) {
		Transform tfm = ccl_fetch(sd, ob_itfm);
		vector = transform_direction(&tfm, vector);
	}
	vector = normalize(vector);
	float v_angle = safe_acosf(-vector.z);
	float h_angle = atan2f(vector.x, vector.y) + M_PI_F;

	float fac = strength * kernel_ies_interp(kg, slot, h_angle, v_angle);

	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, fac);
}

CCL_NAMESPACE_END
