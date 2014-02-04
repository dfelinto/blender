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
 * limitations under the License
 */

#include "util_hash.h"
#include "kernel_primitive.h"

CCL_NAMESPACE_BEGIN

ccl_device void compute_light_pass(KernelGlobals *kg, ShaderData *sd, PathRadiance *L, RNG rng)
{
	int samples = kernel_data.integrator.samples;

	/* initialize master radiance accumulator */
	assert(kernel_data.film.use_light_pass);
	path_radiance_init(L, kernel_data.film.use_light_pass);

	/* take multiple samples */
	for(int sample = 0; sample < samples; sample++) {
		PathRadiance L_sample;
		PathState state;
		Ray ray;
		float3 throughput = make_float3(1.0f, 1.0f, 1.0f);

		/* init radiance */
		path_radiance_init(&L_sample, kernel_data.film.use_light_pass);

		/* init path state */
		path_state_init(kg, &state, &rng, sample);
		state.num_samples = samples;

		/* evaluate surface shader */
		float rbsdf = path_state_rng_1D(kg, &rng, &state, PRNG_BSDF);
		shader_eval_surface(kg, sd, rbsdf, state.flag, SHADER_CONTEXT_MAIN);

		/* sample light and BSDF */
		if(kernel_path_integrate_lighting(kg, &rng, sd, &throughput, &state, &L_sample, &ray)) {
#ifdef __LAMP_MIS__
			state.ray_t = 0.0f;
#endif

			/* compute indirect light */
			kernel_path_indirect(kg, &rng, ray, throughput, state.num_samples, state, &L_sample);

			/* sum and reset indirect light pass variables for the next samples */
			path_radiance_sum_indirect(&L_sample);
			path_radiance_reset_indirect(&L_sample);
		}

		/* accumulate into master L */
		path_radiance_accum_sample(L, &L_sample, samples);
	}
}

ccl_device bool is_light_pass(ShaderEvalType type)
{
	switch (type) {
		case SHADER_EVAL_COMBINED:
		case SHADER_EVAL_SHADOW:
		case SHADER_EVAL_DIFFUSE_DIRECT:
		case SHADER_EVAL_GLOSSY_DIRECT:
		case SHADER_EVAL_TRANSMISSION_DIRECT:
		case SHADER_EVAL_SUBSURFACE_DIRECT:
		case SHADER_EVAL_DIFFUSE_INDIRECT:
		case SHADER_EVAL_GLOSSY_INDIRECT:
		case SHADER_EVAL_TRANSMISSION_INDIRECT:
		case SHADER_EVAL_SUBSURFACE_INDIRECT:
			return true;
		default:
			return false;
	}
}

ccl_device void kernel_bake_evaluate(KernelGlobals *kg, ccl_global uint4 *input, ccl_global float4 *output, ShaderEvalType type, int i)
{
	ShaderData sd;
	uint4 in = input[i];
	float3 out;

	int object = in.x;
	int prim = in.y;
	float u = __uint_as_float(in.z);
	float v = __uint_as_float(in.w);

	if (prim == -1) {
		/* write output */
		if (type == SHADER_EVAL_NORMAL)
			output[i] = make_float4(0.5f, 0.5f, 1.f, 0.0f);
		else
			output[i] = make_float4(0.0f);
		return;
	}

	int shader;
	float3 P = triangle_point_MT(kg, prim, u, v);
	float3 Ng = triangle_normal_MT(kg, prim, &shader);

	/* dummy initilizations copied from SHADER_EVAL_DISPLACE */
	float3 I = Ng;
	float t = 0.0f;
	float time = TIME_INVALID;
	int bounce = 0;
	int segment = ~0;

	/* light passes */
	PathRadiance L;

	/* TODO, disable the closures we won't need */
	shader_setup_from_sample(kg, &sd, P, Ng, I, shader, object, prim, u, v, t, time, bounce, segment);

	if (is_light_pass(type)){
		RNG rng = hash_int(i);
		compute_light_pass(kg, &sd, &L, rng);
	}

	switch (type) {
		/* data passes */
		case SHADER_EVAL_NORMAL:
		{
			/* tangent space: normal = (2 * color) - 1 */
			out = sd.N * 0.5f + make_float3(0.5f);
			break;
		}
		case SHADER_EVAL_UV:
		{
			out = primitive_uv(kg, &sd);
			break;
		}
		case SHADER_EVAL_DIFFUSE_COLOR:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = shader_bsdf_diffuse(kg, &sd);
			break;
		}
		case SHADER_EVAL_GLOSSY_COLOR:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = shader_bsdf_glossy(kg, &sd);
			break;
		}
		case SHADER_EVAL_TRANSMISSION_COLOR:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = shader_bsdf_transmission(kg, &sd);
			break;
		}
		case SHADER_EVAL_SUBSURFACE_COLOR:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = shader_bsdf_subsurface(kg, &sd);
			break;
		}
		case SHADER_EVAL_EMISSION:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_EMISSION);
			out = shader_emissive_eval(kg, &sd);
			break;
		}

		/* light passes */
		case SHADER_EVAL_AO:
		{
			/* TODO */
			break;
		}
		case SHADER_EVAL_COMBINED:
		{
			out = path_radiance_sum(kg, &L);
			break;
		}
		case SHADER_EVAL_SHADOW:
		{
			out = make_float3(L.shadow.x, L.shadow.y, L.shadow.z);
			break;
		}
		case SHADER_EVAL_DIFFUSE_DIRECT:
		{
			out = L.direct_diffuse;
			break;
		}
		case SHADER_EVAL_GLOSSY_DIRECT:
		{
			out = L.direct_glossy;
			break;
		}
		case SHADER_EVAL_TRANSMISSION_DIRECT:
		{
			out = L.direct_transmission;
			break;
		}
		case SHADER_EVAL_SUBSURFACE_DIRECT:
		{
			out = L.direct_subsurface;
			break;
		}
		case SHADER_EVAL_DIFFUSE_INDIRECT:
		{
			out = L.indirect_diffuse;
			break;
		}
		case SHADER_EVAL_GLOSSY_INDIRECT:
		{
			out = L.indirect_glossy;
			break;
		}
		case SHADER_EVAL_TRANSMISSION_INDIRECT:
		{
			out = L.indirect_transmission;
			break;
		}
		case SHADER_EVAL_SUBSURFACE_INDIRECT:
		{
			out = L.indirect_subsurface;
			break;
		}

		/* extra */
		case SHADER_EVAL_ENVIRONMENT:

#if 0
		case SHADER_EVAL_COMBINED:
		{
			/* TODO it's not taking into consideration the
			   RGB inputs of the shader nodes, so it's differing
			   from the Composite COMBINED */

			int shader;
			float3 Ng = triangle_normal_MT(kg, prim, &shader);

			shader_setup_from_sample(kg, &sd, P, Ng, I, shader, object, prim, u, v, t, time, bounce, segment);
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);

#ifdef __MULTI_CLOSURE__
			float3 eval = make_float3(0.0f, 0.0f, 0.0f);

			for(int i = 0; i< sd.num_closure; i++) {
				const ShaderClosure *sc = &sd.closure[i];
				eval += sc->weight;
			}

			out = eval;
#else
			out = sd.closure.weight;
#endif
			break;
		}
		case SHADER_EVAL_DIFFUSE:
		{
			int shader;
			float3 Ng = triangle_normal_MT(kg, prim, &shader);

			shader_setup_from_sample(kg, &sd, P, Ng, I, shader, object, prim, u, v, t, time, bounce, segment);
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);

#ifdef __MULTI_CLOSURE__
			float3 eval = make_float3(0.0f, 0.0f, 0.0f);

			for(int i = 0; i< sd.num_closure; i++) {
				const ShaderClosure *sc = &sd.closure[i];
				if(sc->type == CLOSURE_BSDF_DIFFUSE_ID)
					eval += sc->weight;
			}

			out = eval;
#else
			if(sd.closure.type == CLOSURE_BSDF_DIFFUSE_ID)
				out = sd.closure.weight;
			else
				out = make_float3(0.0f, 0.0f, 0.0f);
#endif
			break;
		}
		case SHADER_EVAL_EMISSION:
		{
			int shader;
			float3 Ng = triangle_normal_MT(kg, prim, &shader);

			shader_setup_from_sample(kg, &sd, P, Ng, I, shader, object, prim, u, v, t, time, bounce, segment);
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_EMISSION);

#ifdef __MULTI_CLOSURE__
			float3 eval = make_float3(0.0f, 0.0f, 0.0f);

			for(int i = 0; i< sd.num_closure; i++) {
				const ShaderClosure *sc = &sd.closure[i];
				if(sc->type == CLOSURE_EMISSION_ID)
					eval += sc->weight;
			}

			out = eval;
#else
			if(sd.closure.type == CLOSURE_EMISSION_ID)
				out = sd.closure.weight;
			else
				out = make_float3(0.0f, 0.0f, 0.0f);
#endif
			break;
		}
		case SHADER_EVAL_AO:
		{
			/* TODO */
			break;
		}
		case SHADER_EVAL_UV:
		{
			/* TODO */
			break;
		}
		case SHADER_EVAL_ENVIRONMENT:
		{
			/* setup ray */
			Ray ray;

			ray.P = make_float3(0.0f, 0.0f, 0.0f);
			ray.D = normalize(P);
			ray.t = 0.0f;
#ifdef __CAMERA_MOTION__
			ray.time = 0.5f;
#endif

#ifdef __RAY_DIFFERENTIALS__
			ray.dD = differential3_zero();
			ray.dP = differential3_zero();
#endif

			/* setup shader data */
			shader_setup_from_background(kg, &sd, &ray, 0);

			/* evaluate */
			int flag = 0; /* we can't know which type of BSDF this is for */
			out = shader_eval_background(kg, &sd, flag, SHADER_CONTEXT_MAIN);
			break;
		}
		case SHADER_EVAL_NORMAL:
		{
			int shader;
			float3 Ng = triangle_normal_MT(kg, prim, &shader);

			shader_setup_from_sample(kg, &sd, P, Ng, I, shader, object, prim, u, v, t, time, bounce, segment);

			out = sd.N;
			break;
		}
#endif

		default:
		{
			/* no real shader, returning the position of the verts for debugging */
			out = normalize(P);
			break;
		}
	}

	/* write output */
	output[i] = make_float4(out.x, out.y, out.z, 0.0f);
	return;
}

ccl_device void kernel_shader_evaluate(KernelGlobals *kg, ccl_global uint4 *input, ccl_global float4 *output, ShaderEvalType type, int i)
{
	if (type >= SHADER_EVAL_BAKE) {
		kernel_bake_evaluate(kg, input, output, type, i);
		return;
	}

	ShaderData sd;
	uint4 in = input[i];
	float3 out;

	if(type == SHADER_EVAL_DISPLACE) {
		/* setup shader data */
		int object = in.x;
		int prim = in.y;
		float u = __uint_as_float(in.z);
		float v = __uint_as_float(in.w);

		shader_setup_from_displace(kg, &sd, object, prim, u, v);

		/* evaluate */
		float3 P = sd.P;
		shader_eval_displacement(kg, &sd, SHADER_CONTEXT_MAIN);
		out = sd.P - P;
	}
	else { // SHADER_EVAL_BACKGROUND
		/* setup ray */
		Ray ray;
		float u = __uint_as_float(in.x);
		float v = __uint_as_float(in.y);

		ray.P = make_float3(0.0f, 0.0f, 0.0f);
		ray.D = equirectangular_to_direction(u, v);
		ray.t = 0.0f;
#ifdef __CAMERA_MOTION__
		ray.time = 0.5f;
#endif

#ifdef __RAY_DIFFERENTIALS__
		ray.dD = differential3_zero();
		ray.dP = differential3_zero();
#endif

		/* setup shader data */
		shader_setup_from_background(kg, &sd, &ray, 0, 0);

		/* evaluate */
		int flag = 0; /* we can't know which type of BSDF this is for */
		out = shader_eval_background(kg, &sd, flag, SHADER_CONTEXT_MAIN);
	}
	
	/* write output */
	output[i] = make_float4(out.x, out.y, out.z, 0.0f);
}

CCL_NAMESPACE_END

