/* -------- Utils Functions --------- */

vec3 sample_hemisphere(float nsample, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_2d(nsample);

	float z = Xi.x; /* cos theta */
	float r = sqrt( 1.0f - z*z ); /* sin theta */
	float x = r * Xi.y;
	float y = r * Xi.z;

	Ht = vec3(x, y, z); /* Global variable */

	return from_tangent_to_world(Ht, N, T, B);
}

float pdf_hemisphere()
{
	return 0.5 * M_1_PI;
}

/* Second order Spherical Harmonics */
/* http://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/ */
vec3 spherical_harmonics_L2(vec3 N)
{
	vec3 sh = vec3(0.0);

	sh += 0.282095 * unfsh0;

	sh += -0.488603 * N.z * unfsh1;
	sh += 0.488603 * N.y * unfsh2;
	sh += -0.488603 * N.x * unfsh3;

	sh += 1.092548 * N.x * N.z * unfsh4;
	sh += -1.092548 * N.z * N.y * unfsh5;
	sh += 0.315392 * (3.0 * N.y * N.y - 1.0) * unfsh6;
	sh += -1.092548 * N.x * N.y * unfsh7;
	sh += 0.546274 * (N.x * N.x - N.z * N.z) * unfsh8;

	return sh;
}

/* -------- BSDF --------- */

float bsdf_lambert(float NL)
{
	return NL * M_1_PI;
}

float bsdf_oren_nayar(float NL, float LV, float NV, float sigma)
{
	float div = 1.0 / (M_PI + ((3.0 * M_PI - 4.0) / 6.0) * sigma);

	float A = 1.0 * div;
	float B = sigma * div;

	float s = LV - NL * NV;
	float t = mix(1.0, max(NL, NV), step(0.0, s));
	return NL * (A + B * s / t);
}

float bsdf_oren_nayar(vec3 N, vec3 L, vec3 V, float sigma)
{
	float NL = max(0.0, dot(N, L));
	float LV = max(0.0, dot(L, V));
	float NV = max(1e-8, dot(N, V));
	return bsdf_oren_nayar(NL, LV, NV, sigma);
}


/* -------- Preview Lights --------- */

void node_bsdf_diffuse_lights(vec4 color, float roughness, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	shade_view(V, V); V = -V;

	/* ambient light */
	vec3 accumulator = ambient_light.rgb;

	/* oren_nayar approximation for ambient */
	float NV = clamp(dot(N, V), 0.0, 0.999);
	float fac = 1.0 - pow(1.0 - NV, 1.3);
	accumulator *= mix(1.0, 0.78, fac*roughness);

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_color = gl_LightSource[i].diffuse.rgb;

		float NL = saturate(dot(N,L));
		float lambert = bsdf_lambert(NL);
		float oren_nayar = bsdf_oren_nayar(N, L, V, roughness);

		accumulator += light_color * mix(lambert, oren_nayar, roughness)  * M_PI; /* M_PI to make preview brighter */
	}

	result = vec4(accumulator*color.rgb, 1.0);
}


/* -------- Physical Lights --------- */
/* from Sebastien Lagarde
 * course_notes_moving_frostbite_to_pbr.pdf */

void bsdf_diffuse_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float l_radius = max(l_areasizex, 0.0001);
	float costheta = clamp(dot(N, L), -0.999, 0.999);
	float h = min(l_radius / l_distance , 0.9999);
	float h2 = h*h;

	bsdf = 0.0;
	if ( costheta * costheta > h2 ) {
		bsdf = M_PI * h2 * clamp(costheta, 0.0, 1.0);
	}
	else {
		float sintheta = sqrt(1.0 - costheta * costheta);
		float x = sqrt(1.0 / h2 - 1.0);
		float y = -x * ( costheta / sintheta );
		float sinthetasqrty = sintheta * sqrt(1.0 - y * y);
		bsdf = (costheta * acos(y) - x * sinthetasqrty ) * h2 + atan(sinthetasqrty / x);
	}

	/* Energy conservation + cycle matching */
	bsdf = max(bsdf, 0.0);
	bsdf *= M_1_PI;
	bsdf *= sphere_energy(l_radius);
}

void bsdf_diffuse_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	if (min(l_areasizex, l_areasizey) < 1e-6) {
		bsdf = 0.0;
		return;
	}

	vec3 pos = V;
	V = -normalize(V);
	N = -N;

	vec3 lampx, lampy, lampz;
	vec2 halfsize = area_light_prepass(l_mat, l_areasizex, l_areasizey, l_areascale, lampx, lampy, lampz);

	vec3 points[4];
	area_light_points(l_coords, halfsize, lampx, lampy, points);

	bsdf = ltc_evaluate(N, V, pos, mat3(1), points);
	bsdf *= step(0.0, -dot(L, lampz));

	/* Energy conservation + cycle matching */
	bsdf *= M_1_2PI;
	bsdf *= rectangle_energy(l_areasizex, l_areasizey);
}
float cot(float x){ return cos(x) / sin(x);}
float acot(float x){ return atan(1 / x);}
void bsdf_diffuse_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float l_radius = max(l_areasizex, 0.0001);
	float costheta = clamp(dot(N, L), -0.999, 0.999);
	float sintheta = sqrt(1.0 - costheta * costheta);
	float h = 1 / l_radius;
	float h2 = h * h;

	if (acos(costheta) < atan(h)) {
		bsdf = M_PI * (1 / (1 + h2)) * costheta;
	}
	else {
		float cottheta = costheta / sintheta;
		float x = sqrt(1 - h2 * cottheta * cottheta);
		bsdf = (-h * x + costheta * (M_PI - acos(h * cottheta))) / (1 + h2) + atan(x / h);
	}
	/* Energy conservation + cycle matching */
	float disk_energy = disk_energy(l_radius);
	bsdf = max(bsdf, 0.0) * disk_energy;
	/* TODO Refine this :
	 * We can try to add contribution of infinitely many point lights at the border of the disk if we know their intensity
	 * Border intensity should be added to the above uniform disk calculation and should be complementary */
	//bsdf += sqrt(1.0 - abs(costheta * costheta * costheta)) * saturate(M_1_2PI - disk_energy);
	bsdf *= M_1_PI;

}


/* -------- Image Based Lighting --------- */

void env_sampling_oren_nayar(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	make_orthonormals(N, T, B); /* Generate tangent space */
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	/* Precomputation */
	float NV = max(1e-8, abs(dot(I, N)));

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < BSDF_SAMPLES && i < unfbsdfsamples.x; i++) {
		vec3 L = sample_hemisphere(i, N, T, B);
		vec3 H = normalize(L - I);

		float NL = max(0.0, dot(N, L));

		if (NL != 0.0) {
			/* Step 1 : Sampling Environment */
			float pdf = pdf_hemisphere();
			vec4 irradiance = sample_probe_pdf(L, pdf);

			/* Step 2 : Integrating BRDF*/
			float LV = max(0.0, dot(L, -I) );
			float brdf = bsdf_oren_nayar(NL, LV, NV, roughness);

			out_radiance += irradiance * brdf / pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y;
}

void env_sampling_diffuse(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	/* Lambert */
	vec3 lambert_diff = spherical_harmonics_L2(N);

	/* early out */
	if (roughness < 1e-5) {
		result = lambert_diff;
		return;
	}

	/* Oren Nayar */
	vec3 oren_nayar_diff;
	env_sampling_oren_nayar(
		pbr, viewpos, invviewmat, viewmat,
		N, T, roughness, ior, sigma,
		toon_size, toon_smooth, anisotropy, aniso_rotation,
		oren_nayar_diff);

	result = mix(lambert_diff, oren_nayar_diff, roughness);
}
