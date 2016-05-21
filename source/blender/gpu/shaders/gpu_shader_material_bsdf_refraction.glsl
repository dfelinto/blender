/* -------- Utils Functions --------- */

float pdf_ggx_refract(float Ht2, float NH, float NV, float VH, float LH, float G1_V, float a2, float eta)
{
	return (VH * abs(LH)) * (eta*eta) * a2 * G1_V / (Ht2 * NV * D_ggx_opti(NH, a2));
}

/* -------- BSDF --------- */

float bsdf_ggx_refract(float Ht2, float NH, float NL, float NV, float VH, float LH, float a2, float eta)
{
	float G = G1_Smith(NV, a2) * G1_Smith(NL, a2);
	float D = D_ggx_opti(NH, a2); /* Doing RCP and mul a2 at the end */

	/* bsdf = abs(LH * VH) * (eta*eta) * G * D / (NV * Ht2); /* Reference function */
	return abs(LH * VH) * (NV * NL * 4.0) * a2 * (eta*eta) / (D * G * NV * Ht2); /* Balancing the adjustments made in G1_Smith with (NV * NL * 4.0)*/
}

float bsdf_ggx_refract(vec3 N, vec3 L, vec3 V, float eta, float roughness)
{
	/* GGX Spec Isotropic Transmited */
	float a, a2; prepare_glossy(roughness, a, a2);

	vec3 ht = -(eta * L + V);
	vec3 Ht = normalize(ht);
	float Ht2 = dot(ht, ht);
	float NH = dot(N, Ht);
	float NL = dot(N, -L);
	float NV = dot(N, V);
	float VH = dot(V, Ht);
	float LH = dot(-L, Ht);

	return bsdf_ggx_refract(Ht2, NH, NL, NV, VH, LH, a2, eta);
}

/* This one returns the brdf already divided by the pdf */
float bsdf_ggx_refract_pdf(float a2, float LH, float NL, float VH)
{
	float G1_L = NL * 2.0 / G1_Smith(NL, a2); /* Balancing the adjustments made in G1_Smith */

	/* brdf = abs(VH*LH) * (ior*ior) * D * G(V) * G(L) / (Ht2 * NV)
	 * pdf = (VH * abs(LH)) * (ior*ior) * D * G(V) / (Ht2 * NV) */
	return G1_L * abs(VH*LH) / (VH * abs(LH));
}

/* -------- Preview Lights --------- */

void node_bsdf_refraction_lights(vec4 color, float roughness, float ior, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	node_bsdf_glossy_lights(color, roughness, N, V, ambient_light, result);
}

/* -------- Physical Lights --------- */

/* REFRACT SHARP */

void bsdf_refract_sharp_sphere_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                                 float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                                 float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                                 out float bsdf)
{
	float l_radius = l_areasizex;
	L = l_distance * L;
	vec3 R = -refract(-V, N, (gl_FrontFacing) ? 1.0/ior : ior);

	vec3 P = line_aligned_plane_intersect(vec3(0.0), R, L);
	bsdf = (distance_squared(P, L) < l_radius * l_radius) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	bsdf *= sphere_energy(l_radius);
}

void bsdf_refract_sharp_area_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                               float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                               float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                               out float bsdf)
{
	if (max(l_areasizex, l_areasizey) < 1e-6) {
		bsdf = 0.0;
		return;
	}

	L = l_distance * L;

	vec3 lampx, lampy, lampz;
	vec2 halfsize = area_light_prepass(l_mat, l_areasizex, l_areasizey, l_areascale, lampx, lampy, lampz);

	/* Find the intersection point E between the reflection vector and the light plane */
	float eta = (gl_FrontFacing) ? ior : 1.0/ior;
	vec3 R = refract(-V, N, 1.0/eta);
	vec3 E = line_plane_intersect(vec3(0.0), R, L, lampz);

	/* Project it onto the light plane */
	vec3 projection = E - L;
	float A = dot(lampx, projection);
	float B = dot(lampy, projection);

	bsdf = (abs(A) < halfsize.x && abs(B) < halfsize.y) ? 1.0 : 0.0;

	/* Masking */
	bsdf *= (dot(-L, lampz) > 0.0) ? 1.0 : 0.0;
	bsdf *= (dot(-R, lampz) > 0.0) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	bsdf *= rectangle_energy(l_areasizex, l_areasizey);
}

void bsdf_refract_sharp_sun_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                              float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                              float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                              out float bsdf)
{
	float eta = (gl_FrontFacing) ? ior : 1.0/ior;
	vec3 R = refract(-V, N, 1.0/eta);

	float l_radius = l_areasizex;
	float angle = atan(l_radius);

	float costheta = dot(L, R);
	float cosangle = cos(angle);

	bsdf = (costheta > cosangle) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	bsdf *= disk_energy(l_radius);
	bsdf /= costheta * costheta * costheta;
	bsdf *= M_PI;
}

/* REFRACT GGX */

void bsdf_refract_ggx_sphere_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                               float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                               float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                               out float bsdf)
{
	if (roughness < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

#if 1
	float l_radius = l_areasizex;

	shade_view(V, V);
	float eta = (gl_FrontFacing) ? ior : 1.0/ior;
	vec3 R = refract(-V, N, 1.0/eta);

	float energy_conservation = 1.0;
	most_representative_point(l_radius, 0.0, vec3(0.0), l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_ggx_refract(N, L, V, eta, roughness);

	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= sphere_energy(l_radius) * max(l_radius * l_radius, 1e-16); /* l_radius is already inside energy_conservation */
	bsdf *= M_PI; /* XXX : !!! Very Empirical, Fit cycles power */
#else
	float l_radius = max(0.007, l_areasizex);

	vec3 pos = V;
	shade_view(V, V);
	vec3 R = -refract(-V, N, (gl_FrontFacing) ? 1.0/ior : ior);
	L = l_distance * L;
	V = -V;
	N = -N;

	vec3 P = line_aligned_plane_intersect(vec3(0.0), R, L);
	vec3 Px = normalize(P - L) * l_radius;
	vec3 Py = axis_angle_rotation(Px, R, M_PI_2);

	vec3 points[4];
	points[0] = l_coords + Px;
	points[1] = l_coords - Py;
	points[2] = l_coords - Px;
	points[3] = l_coords + Py;

	float NV = max(dot(N, V), 1e-8);
	vec2 uv = ltc_coords(NV, sqrt(roughness));
	mat3 ltcmat = ltc_matrix(uv);

	bsdf = ltc_evaluate(N, V, pos, ltcmat, points);
	bsdf *= texture2D(unfltcmag, uv).r; /* Bsdf matching */

	bsdf *= M_1_2PI;
	bsdf *= sphere_energy(l_radius);
#endif
}

void bsdf_refract_ggx_area_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                             float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                             float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                             out float bsdf)
{
	if (min(l_areasizex, l_areasizey) < 1e-6) {
		bsdf = 0.0;
		return;
	}

	l_areasizex *= l_areascale.x;
	l_areasizey *= l_areascale.y;

	/* Used later for Masking : Use the real Light Vector */
	vec3 lampz = normalize( (l_mat * vec4(0.0,0.0,1.0,0.0) ).xyz ); //lamp projection axis
	float masking = max(dot( normalize(-L), lampz), 0.0);

	float max_size = max(l_areasizex, l_areasizey);
	float min_size = min(l_areasizex, l_areasizey);
	vec3 lampVec = (l_areasizex > l_areasizey) ? normalize( (l_mat * vec4(1.0,0.0,0.0,0.0) ).xyz ) : normalize( (l_mat * vec4(0.0,1.0,0.0,0.0) ).xyz );


	float eta = (gl_FrontFacing) ? ior : 1.0/ior;
	vec3 R = refract(-V, N, 1.0/eta);
	float energy_conservation = 1.0;
	most_representative_point(min_size/2, max_size-min_size, lampVec, l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_ggx_refract(N, L, V, eta, roughness);

	/* energy_conservation */
	float LineAngle = clamp( (max_size-min_size) / l_distance, 0.0, 1.0);
	float energy_conservation_line = energy_conservation * ( roughness / clamp(roughness + 0.5 * LineAngle, 0.0, 1.1));

	/* XXX : Empirical modification for low roughness matching */
	float energy_conservation_mod = energy_conservation * (1 + roughness) / ( max_size/min_size );
	energy_conservation = mix(energy_conservation_mod, energy_conservation_line, min(roughness/0.3, 0.9*(1.1-roughness)/0.1));

	/* As we represent the Area Light by a tube light we must use a custom energy conservation */
	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= masking;
	bsdf *= 23.2; /* XXX : !!! Very Empirical, Fit cycles power */
}

void bsdf_refract_ggx_sun_light(vec3 N, vec3 T, vec3 L, vec3 V, vec3 l_coords,
	                            float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	                            float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	                            out float bsdf)
{
	/* Correct ligth shape but uniform intensity
	 * Does not take into account the division by costheta^3 */
	if (roughness < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	float eta = (gl_FrontFacing) ? ior : 1.0/ior;
	vec3 R = refract(-V, N, 1.0/eta);

	float l_radius = l_areasizex;
	float angle = atan(l_radius);

	float costheta = dot(L, R);
	float cosangle = cos(angle);

	float energy_conservation = 1.0;
	most_representative_point_disk(l_radius, 1.0, R, L, roughness, energy_conservation);

	bsdf = bsdf_ggx_refract(N, L, V, eta, roughness);
	bsdf *= energy_conservation;
}


/* -------- Image Based Lighting --------- */

void env_sampling_refract_sharp(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	float eta = (gl_FrontFacing) ? 1.0/ior : ior;

	vec3 L = refract(I, N, eta);

	result = sample_refract(L).rgb;
}

void env_sampling_refract_ggx(
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

	/* Early out */
	if (abs(ior - 1.0) < 1e-4) {
		result = sample_transparent();
		return;
	}

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < BSDF_SAMPLES && i < unfbsdfsamples.x; i++) {
		vec3 H = sample_ggx(i, a2, N, T, B); /* Microfacet normal */
		float eta = 1.0/ior;
		float fresnel = fresnel_dielectric(I, H, ior);

		if (dot(H, -I) < 0.0) {
			H = -H;
			eta = ior;
		}

		vec3 L = refract(I, H, eta);
		float NL = -dot(N, L);
		if (NL > 0.0 && fresnel != 1.0) {
			/* Step 1 : Sampling Environment */
			float NH = dot(N, H); /* cosTheta */
			float VH = dot(-I, H);
			float LH = dot(L, H);
			float tmp = ior * VH + LH;
			float Ht2 = tmp * tmp;

			float pdf = pdf_ggx_refract(Ht2, NH, NV, VH, LH, G1_V, a2, eta);

			vec4 sample = sample_refract_pdf(L, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float brdf_pdf = bsdf_ggx_refract_pdf(a2, LH, NL, VH);

			out_radiance += NL * sample * brdf_pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y;
}