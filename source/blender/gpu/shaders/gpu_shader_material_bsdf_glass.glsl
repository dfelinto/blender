/* -------- Utils Functions --------- */

float fresnel_blend(float transmit_bsdf, float reflect_bsdf, vec3 V, vec3 N, float ior)
{
	/* Fresnel Blend */
	float eta = (gl_FrontFacing) ? ior : 1.0/ior;
	float fresnel = fresnel_dielectric(V, N, eta);

	return mix(max(0.0, transmit_bsdf), reflect_bsdf, fresnel);
}

vec3 fresnel_blend(vec3 transmit_bsdf, vec3 reflect_bsdf, vec3 V, vec3 N, float ior)
{
	/* Fresnel Blend */
	float eta = (gl_FrontFacing) ? ior : 1.0/ior;
	float fresnel = fresnel_dielectric(V, N, eta);

	return mix(transmit_bsdf, reflect_bsdf, fresnel);
}


/* -------- BSDF --------- */

/* -------- Preview Lights --------- */

void node_bsdf_glass_lights(vec4 color, float roughness, float ior, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	node_bsdf_glossy_lights(color, roughness, N, V, ambient_light, result);
}

/* -------- Physical Lights --------- */

/* GLASS GGX */

void bsdf_glass_ggx_sphere_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                             float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                             float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                             out float bsdf)
{
	float transmit_bsdf, reflect_bsdf;

	bsdf_refract_ggx_sphere_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                              sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, transmit_bsdf);
	bsdf_glossy_ggx_sphere_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                             sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, reflect_bsdf);

	bsdf = fresnel_blend(transmit_bsdf, reflect_bsdf, V, N, ior);
}

void bsdf_glass_ggx_area_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                           float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                           float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                           out float bsdf)
{
	float transmit_bsdf, reflect_bsdf;

	bsdf_refract_ggx_area_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                            sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, transmit_bsdf);
	bsdf_glossy_ggx_area_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                           sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, reflect_bsdf);

	bsdf = fresnel_blend(transmit_bsdf, reflect_bsdf, V, N, ior);
}

void bsdf_glass_ggx_sun_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                          float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                          float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                          out float bsdf)
{
	float transmit_bsdf, reflect_bsdf;

	bsdf_refract_ggx_sun_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                           sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, transmit_bsdf);
	bsdf_glossy_ggx_sun_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                          sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, reflect_bsdf);

	bsdf = fresnel_blend(transmit_bsdf, reflect_bsdf, V, N, ior);
}

/* GLASS SHARP */

void bsdf_glass_sharp_sphere_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                               float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                               float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                               out float bsdf)
{
	float transmit_bsdf, reflect_bsdf;

	bsdf_refract_sharp_sphere_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                                sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, transmit_bsdf);
	bsdf_glossy_sharp_sphere_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                               sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, reflect_bsdf);

	bsdf = fresnel_blend(transmit_bsdf, reflect_bsdf, V, N, ior);
}

void bsdf_glass_sharp_area_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                             float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                             float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                             out float bsdf)
{
	float transmit_bsdf, reflect_bsdf;

	bsdf_refract_sharp_area_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                              sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, transmit_bsdf);
	bsdf_glossy_sharp_area_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                             sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, reflect_bsdf);

	bsdf = fresnel_blend(transmit_bsdf, reflect_bsdf, V, N, ior);
}

void bsdf_glass_sharp_sun_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                            float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                            float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                            out float bsdf)
{
	float transmit_bsdf, reflect_bsdf;

	bsdf_refract_sharp_sun_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                             sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, transmit_bsdf);
	bsdf_glossy_sharp_sun_light(N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, roughness, ior,
	                            sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, reflect_bsdf);

	bsdf = fresnel_blend(transmit_bsdf, reflect_bsdf, V, N, ior);
}


/* -------- Image Based Lighting --------- */

void env_sampling_glass_sharp(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	vector_prepass(viewpos, N, invviewmat, viewmat);

	/* reflection */
	vec3 R = reflect(I, N);
	vec4 reflect_bsdf = sample_reflect(R);

	/* transmission */
	float eta = (gl_FrontFacing) ? 1.0/ior : ior;
	vec3 Tr = refract(I, N, eta);
	vec4 transmit_bsdf = sample_refract(Tr);

	result = fresnel_blend(transmit_bsdf.rgb, reflect_bsdf.rgb, I, N, ior);
}

void env_sampling_glass_ggx(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	make_orthonormals(N, T, B); /* Generate tangent space */
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */
	float a, a2; prepare_glossy(roughness, a, a2);
	ior = max(ior, 1e-5);

	/* Precomputation */
	float NV = max(1e-8, abs(dot(I, N)));
	float G1_V = G1_Smith(NV, a2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < BSDF_SAMPLES && i < unfbsdfsamples.x; i++) {
		vec3 H = sample_ggx(i, a2, N, T, B); /* Microfacet normal */

		/* TODO : For ior < 1.0 && roughness > 0.0 fresnel becomes not accurate.*/
		float fresnel = fresnel_dielectric(I, H, (dot(H, -I) < 0.0) ? 1.0/ior : ior );

		/* reflection */
		vec3 R = reflect(I, H);
		float NL = max(0.0, dot(N, R));
		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = max(1e-8, dot(N, H)); /* cosTheta */
			float VH = max(1e-8, -dot(I, H));

			float pdf = pdf_ggx_reflect(NH, a2);

			vec4 sample = sample_reflect_pdf(R, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float brdf_pdf = bsdf_ggx_pdf(a2, NH, NL, VH, G1_V);

			/* See reflect glossy */
			out_radiance += NL * sample * brdf_pdf * fresnel;
		}

		/* transmission */
		float eta = 1.0/ior;
		if (dot(H, -I) < 0.0) {
			H = -H;
			eta = ior;
		}

		vec3 Tr = refract(I, N, eta);
		NL = -dot(N, Tr);
		if (NL > 0.0 && fresnel != 1.0) {
			/* Step 1 : Sampling Environment */
			float NH = dot(N, H); /* cosTheta */
			float VH = dot(-I, H);
			float LH = dot(Tr, H);
			float tmp = ior * VH + LH;
			float Ht2 = tmp * tmp;

			float pdf = pdf_ggx_refract(Ht2, NH, NV, VH, LH, G1_V, a2, eta);

			vec4 sample = sample_refract_pdf(Tr, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float brdf_pdf = bsdf_ggx_refract_pdf(a2, LH, NL, VH);

			out_radiance += NL * sample * brdf_pdf * (1 - fresnel);
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y;
}
