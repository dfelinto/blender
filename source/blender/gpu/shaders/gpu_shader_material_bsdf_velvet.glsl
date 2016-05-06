/* -------- Utils Functions --------- */

void prepare_velvet(float sigma, out float m_1_sig2)
{
	sigma = max(sigma, 1e-2);
	m_1_sig2 = 1 / (sigma * sigma);
}

/* -------- BSDF --------- */

float bsdf_ashikhmin_velvet(float NL, float NV, float NH, float VH, float m_1_sig2)
{
	float NHdivVH = NH / VH;
	NHdivVH = max(NHdivVH, 1e-5);

	float fac1 = 2 * abs(NHdivVH * NV);
	float fac2 = 2 * abs(NHdivVH * NL);

	float sinNH2 = 1 - NH * NH;
	float sinNH4 = sinNH2 * sinNH2;
	float cotan2 = (NH * NH) / sinNH2;

	float D = exp(-cotan2 * m_1_sig2) * m_1_sig2 * M_1_PI / sinNH4;
	float G = min(1.0, min(fac1, fac2)); // TODO: derive G from D analytically

	return 0.25 * (D * G) / NV;
}

float bsdf_ashikhmin_velvet(vec3 N, vec3 L, vec3 V, float m_1_sig2)
{
	vec3 H = normalize(L + V);

	float NL = max(dot(N, L), 0.0);
	float NV = max(dot(N, V), 1e-5);
	float NH = dot(N, H);
	float VH = max(abs(dot(V, H)), 1e-5);

	if(abs(NH) < 1.0-1e-5) {
		return bsdf_ashikhmin_velvet(NL, NV, NH, VH, m_1_sig2);
	}

	return 0.0;
}

/* -------- Preview Lights --------- */

void node_bsdf_velvet_lights(vec4 color, float sigma, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	shade_view(V, V); V = -V;
	float m_1_sig2; prepare_velvet(sigma, m_1_sig2);

	/* ambient light */
	vec3 accumulator = ambient_light.rgb;

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_color = gl_LightSource[i].specular.rgb;

		accumulator += light_color * bsdf_ashikhmin_velvet(N, L, V, m_1_sig2);
	}

	result = vec4(accumulator * color.rgb, 1.0);
}

/* -------- Physical Lights --------- */

/* VELVET */

void bsdf_velvet_sphere_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                          float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                          float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                          out float bsdf)
{
	float m_1_sig2; prepare_velvet(sigma, m_1_sig2);

	bsdf = bsdf_ashikhmin_velvet(N, L, V, m_1_sig2);

	/* Energy conservation + cycle matching */
	bsdf *= 8.0 / (l_distance * l_distance);
	/* bsdf *= sphere_energy(l_radius); Velvet is using only point lights for now */
}

void bsdf_velvet_area_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                        float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                        float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                        out float bsdf)
{
	if (max(l_areasizex, l_areasizey) < 1e-6) {
		bsdf = 0.0;
		return;
	}

	float m_1_sig2; prepare_velvet(sigma, m_1_sig2);

	bsdf = bsdf_ashikhmin_velvet(N, L, V, m_1_sig2);

	/* Energy conservation + cycle matching */
	bsdf *= 8.0 / (l_distance * l_distance);

	/* l_areasizex *= scale.x;
	 * l_areasizey *= scale.y;
	 * bsdf *= rectangle_energy(l_areasizex, l_areasizey); Velvet is using only point lights for now*/
}

void bsdf_velvet_sun_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                       float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                       float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                       out float bsdf)
{
	float m_1_sig2; prepare_velvet(sigma, m_1_sig2);

	bsdf = bsdf_ashikhmin_velvet(N, L, V, m_1_sig2);
}


/* -------- Image Based Lighting --------- */

void env_sampling_velvet(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	vector_prepass(viewpos, N, invviewmat, viewmat);
	make_orthonormals(N, T, B); /* Generate tangent space */
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	/* Precomputation */
	float m_1_sig2; prepare_velvet(sigma, m_1_sig2);
	float NV = max(1e-5, abs(dot(I, N)));

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < BSDF_SAMPLES && i < unfbsdfsamples.x; i++) {
		vec3 L = sample_hemisphere(i, N, T, B);
		vec3 H = normalize(L - I);

		float NL = max(0.0, dot(N, L));
		float NH = dot(N, H); /* cosTheta */

		if (NL != 0.0 && abs(NH) < 1.0-1e-5) {
			/* Step 1 : Sampling Environment */
			float pdf = pdf_hemisphere();
			vec4 irradiance = sample_probe_pdf(L, pdf);

			/* Step 2 : Integrating BRDF*/
			float VH = max(abs(dot(I, H)), 1e-5);
			float brdf = bsdf_ashikhmin_velvet(NL, NV, NH, VH, m_1_sig2);

			out_radiance += irradiance * brdf / pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y;
}
