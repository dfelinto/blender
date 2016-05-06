/* -------- Utils Functions --------- */

vec3 sample_uniform_cone(float nsample, float angle, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_2d(nsample);

	float z = cos(angle * Xi.x);
	float r = sqrt(1.0 - z*z);
	float x = r * Xi.y;
	float y = r * Xi.z;

	Ht = vec3(x, y, z); /* Global variable */

	return from_tangent_to_world(Ht, N, T, B);
}

float pdf_uniform_cone(float angle)
{
	return 0.5 * M_1_PI / (1.0 - cos(angle));
}

void prepare_toon(inout float tsize, inout float tsmooth, out float sample_angle)
{
	tsize = saturate(tsize) * M_PI_2;
	tsmooth = saturate(tsmooth) * M_PI_2;
	sample_angle = min(tsize + tsmooth, M_PI_2);
}

float bsdf_toon_get_intensity(float max_angle, float tsmooth, float angle)
{
	if(angle < max_angle)
		return 1.0;
	else if(angle < (max_angle + tsmooth) && tsmooth != 0.0)
		return (1.0 - (angle - max_angle)/tsmooth);
	else
		return 0.0;
}

/* -------- BSDF --------- */

float bsdf_toon(float cosangle, float pdf, float max_angle, float tsmooth)
{
	float angle = acos(min(0.9999999, cosangle));
	float eval = bsdf_toon_get_intensity(max_angle, tsmooth, angle);
	return pdf * eval;
}

float bsdf_toon_diffuse(vec3 N, vec3 L, float sample_angle, float max_angle, float tsmooth)
{
	float NL = max(0.0, dot(N, L));

	if(NL > 0.0) {
		float pdf = pdf_uniform_cone(sample_angle);
		return bsdf_toon(NL, pdf, max_angle, tsmooth);
	}

	return 0.0;
}

float bsdf_toon_glossy(vec3 N, vec3 L, vec3 V, float sample_angle, float max_angle, float tsmooth)
{
	float NL = max(0.0, dot(N, L));
	float NV = max(0.0, dot(N, V));

	if(NV > 0.0 && NL > 0.0) {
		vec3 R = -reflect(V, N);
		float RL = max(0.0, dot(R, L));
		float pdf = pdf_uniform_cone(sample_angle);
		return bsdf_toon(RL, pdf, max_angle, tsmooth);
	}

	return 0.0;
}


/* -------- Preview Lights --------- */

void node_bsdf_toon_diffuse_lights(vec4 color, float size, float tsmooth, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	shade_view(V, V); V = -V;
	N = normalize(N);
	float sample_angle; prepare_toon(size, tsmooth, sample_angle);

	/* ambient light */
	vec3 accumulator = ambient_light.rgb;

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_color = gl_LightSource[i].diffuse.rgb;

		accumulator += light_color * bsdf_toon_diffuse(N, L, sample_angle, size, tsmooth) * M_PI; /* M_PI to make preview brighter */
	}

	result = vec4(accumulator*color.rgb, 1.0);
}

void node_bsdf_toon_glossy_lights(vec4 color, float size, float tsmooth, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	shade_view(V, V); V = -V;
	N = normalize(N);
	float sample_angle; prepare_toon(size, tsmooth, sample_angle);

	/* ambient light */
	vec3 accumulator = ambient_light.rgb;

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_color = gl_LightSource[i].specular.rgb;

		accumulator += light_color * bsdf_toon_glossy(N, L, V, sample_angle, size, tsmooth) * M_PI; /* M_PI to make preview brighter */
	}

	result = vec4(accumulator*color.rgb, 1.0);
}


/* -------- Physical Lights --------- */

/* TOON DIFFUSE */

void bsdf_toon_diffuse_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float sample_angle; prepare_toon(toon_size, toon_smooth, sample_angle);
	bsdf = bsdf_toon_diffuse(N, L, sample_angle, toon_size, toon_smooth);

	/* Energy conservation + cycle matching */
	bsdf *= 8.0 / (l_distance * l_distance);
}

void bsdf_toon_diffuse_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float sample_angle; prepare_toon(toon_size, toon_smooth, sample_angle);
	bsdf = bsdf_toon_diffuse(N, L, sample_angle, toon_size, toon_smooth);

	/* Energy conservation + cycle matching */
	bsdf *= 8.0 / (l_distance * l_distance);
}

void bsdf_toon_diffuse_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float sample_angle; prepare_toon(toon_size, toon_smooth, sample_angle);
	bsdf = bsdf_toon_diffuse(N, L, sample_angle, toon_size, toon_smooth);
}

/* TOON GLOSSY */

void bsdf_toon_glossy_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float sample_angle; prepare_toon(toon_size, toon_smooth, sample_angle);
	bsdf = bsdf_toon_glossy(N, L, V, sample_angle, toon_size, toon_smooth);
}

void bsdf_toon_glossy_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float sample_angle; prepare_toon(toon_size, toon_smooth, sample_angle);
	bsdf = bsdf_toon_glossy(N, L, V, sample_angle, toon_size, toon_smooth);

	/* Energy conservation + cycle matching */
	bsdf *= 8.0 / (l_distance * l_distance);
}

void bsdf_toon_glossy_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float sample_angle; prepare_toon(toon_size, toon_smooth, sample_angle);
	bsdf = bsdf_toon_glossy(N, L, V, sample_angle, toon_size, toon_smooth);
}


/* -------- Image Based Lighting --------- */

void env_sampling_toon_diffuse(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	make_orthonormals(N, T, B); /* Generate tangent space */
	float sample_angle; prepare_toon(toon_size, toon_smooth, sample_angle);
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	/* Precomputation */
	float NV = max(0.0, dot(I, N));

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < BSDF_SAMPLES && i < unfbsdfsamples.x; i++) {
		vec3 L = sample_uniform_cone(i, sample_angle, N, T, B);
		float NL = max(0.0, dot(N, L));

		if (NL != 0.0) {
			/* Step 1 : Sampling Environment */
			float pdf = pdf_uniform_cone(sample_angle);
			vec4 irradiance = sample_probe_pdf(L, pdf);

			/* Step 2 : Integrating BRDF*/
			float brdf = bsdf_toon(NL, pdf, toon_size, toon_smooth);

			out_radiance += irradiance * brdf / pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y;
}

void env_sampling_toon_glossy(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	float sample_angle; prepare_toon(toon_size, toon_smooth, sample_angle);
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	/* Precomputation */
	float NV = max(1e-8, dot(I, N));
	vec3 R = reflect(I, N);

	/* We are sampling aroung R so generate basis with it */
	make_orthonormals(R, T, B); /* Generate tangent space */

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < BSDF_SAMPLES && i < unfbsdfsamples.x; i++) {
		vec3 L = sample_uniform_cone(i, sample_angle, R, T, B);

		float NL = max(0.0, dot(N, L));

		if (NL != 0.0) {
			float RL = max(0.0, dot(R, L));

			/* Step 1 : Sampling Environment */
			float pdf = pdf_uniform_cone(sample_angle);
			vec4 irradiance = sample_reflect_pdf(L, 0.0, pdf);

			/* Step 2 : Integrating BRDF*/
			float brdf = bsdf_toon(RL, pdf, toon_size, toon_smooth);

			out_radiance += irradiance * brdf / pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y;
}
