/* -------- Utils Functions --------- */

vec3 sample_ggx(float nsample, float a2, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);

	float z = sqrt( (1.0 - Xi.x) / ( 1.0 + a2 * Xi.x - Xi.x ) ); /* cos theta */
	float r = sqrt( 1.0 - z * z ); /* sin theta */
	float x = r * Xi.y;
	float y = r * Xi.z;

	/* Global variable */
	Ht = vec3(x, y, z);

	return from_tangent_to_world(Ht, N, T, B);
}

vec3 sample_beckmann(float nsample, float a2, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);

	float z = sqrt( 1.0 / ( 1.0 - a2 * log(1.0 - Xi.x) ) ); /* cos theta */
	float r = sqrt( 1.0 - z * z ); /* sin theta */
	float x = r * Xi.y;
	float y = r * Xi.z;

	/* Global variable */
	Ht = vec3(x, y, z);

	return from_tangent_to_world(Ht, N, T, B);
}

vec3 sample_ashikhmin_shirley(float nsample, float n_x, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);

	float z = pow( Xi.x, 1.0 / (n_x + 1.0) ); /* cos theta */
	float r = sqrt( 1.0 - z * z ); /* sin theta */
	float x = r * Xi.y;
	float y = r * Xi.z;

	/* Global variable */
	Ht = vec3(x, y, z);

	return from_tangent_to_world(Ht, N, T, B);
}

float D_ggx_opti(float NH, float a2)
{
	float tmp = (NH * a2 - NH) * NH + 1.0;
	return M_PI * tmp*tmp; /* Doing RCP and mul a2 at the end */
}

float D_beckman(float NH, float a2)
{
	float NH2 = NH * NH;
	return exp((NH2 - 1) / (NH2 * a2)) / (M_PI * a2 * NH2 * NH2);
}

float pdf_ggx_reflect(float NH, float a2)
{
	return NH * a2 / D_ggx_opti(NH, a2);
}

float pdf_beckmann_reflect(float NH, float a2)
{
	return NH * D_beckman(NH, a2);
}

float pdf_ashikhmin_shirley_reflect(float NH, float VH, float n_x)
{
	float lobe = pow(NH, n_x);
	float norm = (n_x + 1.0) * 0.125 * M_1_PI;

	return norm * lobe / VH;
}

void prepare_glossy(float roughness, out float a, out float a2)
{
	/* Artifacts appear with roughness below this threshold */
	/* XXX TODO : find why flooring is necessary */
	a  = clamp(roughness, 2e-4, 0.9999999);
	a2 = max(1e-8, a*a);
}

float G1_Smith_GGX(float NX, float a2)
{
	/* Using Brian Karis approach and refactoring by NX/NX
	 * this way the (2*NL)*(2*NV) in G = G1(V) * G1(L) gets canceled by the brdf denominator 4*NL*NV
	 * Rcp is done on the whole G later
	 * Note that this is not convenient for the transmition formula */
	return NX + sqrt( NX * (NX - NX * a2) + a2 );
	/* return 2 / (1 + sqrt(1 + a2 * (1 - NX*NX) / (NX*NX) ) ); /* Reference function */
}

float G1_Smith_beckmann(float NX, float a2)
{
	float tmp = 1 / (sqrt(a2 * (1 - NX * NX) / (NX * NX)));
	return (tmp < 1.6) ? (3.535 * tmp + 2.181 * tmp * tmp) / (1 + 2.276 * tmp + 2.577 * tmp * tmp) : 1.0;
}

/* -------- BSDF --------- */

float bsdf_ggx(vec3 N, vec3 L, vec3 V, float roughness)
{
	float a, a2; prepare_glossy(roughness, a, a2);

	vec3 H = normalize(L + V);
	float NH = max(dot(N, H), 1e-8);
	float NL = max(dot(N, L), 1e-8);
	float NV = max(dot(N, V), 1e-8);

	float G = G1_Smith_GGX(NV, a2) * G1_Smith_GGX(NL, a2); /* Doing RCP at the end */
	float D = D_ggx_opti(NH, a2);

	/* Denominator is canceled by G1_Smith */
	/* bsdf = D * G / (4.0 * NL * NV); /* Reference function */
	return NL * a2 / (D * G); /* NL to Fit cycles Equation : line. 345 in bsdf_microfacet.h */
}

/* This one returns the brdf already divided by the pdf */
float bsdf_ggx_pdf(float a2, float NH, float NL, float VH, float G1_V)
{
	float G = G1_V * G1_Smith_GGX(NL, a2); /* Doing RCP at the end */

	/* Denominator is canceled by G1_Smith
	 * brdf = D * G / (4.0 * NL * NV) [denominator canceled by G]
	 * pdf = D * NH / (4 * VH) [D canceled later by D in brdf] */
	return 4.0 * VH / (NH * G); /* brdf / pdf */
}

float bsdf_beckmann(vec3 N, vec3 L, vec3 V, float roughness)
{
	float a, a2; prepare_glossy(roughness, a, a2);

	vec3 H = normalize(L + V);
	float NH = max(dot(N, H), 1e-8);
	float NL = max(dot(N, L), 1e-8);
	float NV = max(dot(N, V), 1e-8);

	float G = G1_Smith_beckmann(NV, a2) * G1_Smith_beckmann(NL, a2);
	float D = D_beckman(NH, a2);

	return NL * D * G * 0.25 / (NL * NV);
}

/* This one returns the brdf already divided by the pdf */
float bsdf_beckmann_pdf(float a2, float NH, float NV, float NL, float VH, float G1_V)
{
	float G = G1_V * G1_Smith_beckmann(NL, a2);

	/* brdf = D * G / (4.0 * NL * NV)
	 * pdf = D * NH / (4 * VH) [D canceled later by D in brdf] */
	return G * VH / (NH * NV * NL); /* brdf / pdf */
}

float bsdf_ashikhmin_shirley(vec3 N, vec3 L, vec3 V, float roughness)
{
	float a, a2; prepare_glossy(roughness, a, a2);

	vec3 H = normalize(L + V);
	float NL = max(dot(N, L), 1e-6);
	float NV = max(dot(N, V), 1e-6);
	float NH = max(dot(N, H), 1e-6);
	float VH = max(abs(dot(V, H)), 1e-6);

	float pump = 1.0 / max(1e-6, VH * max(NL, NV));
	float n_x = 2.0 / a2 - 2.0;
	float lobe = pow(NH, n_x);
	float norm = (n_x + 1.0f) * 0.125 * M_1_PI;

	return NL * norm * lobe * pump;
}

/* This one returns the brdf already divided by the pdf */
float bsdf_ashikhmin_shirley_pdf(float NV, float NL, float VH)
{
	float pump = 1.0 / max(1e-6, VH * max(NL, NV));

	return VH * pump;
}

/* -------- Preview Lights --------- */

void node_bsdf_glossy_lights(vec4 color, float roughness, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	vec3 accumulator = ambient_light.rgb;

	if (roughness <= 1e-4) {
		result = vec4(accumulator * color.rgb, 1.0);
		return;
	}

	shade_view(V, V); V = -V;
	N = normalize(N);

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_color = gl_LightSource[i].specular.rgb;

		accumulator += light_color * bsdf_ggx(N, L, V, roughness);
	}

	result = vec4(accumulator * color.rgb, 1.0);
}

/* -------- Physical Lights --------- */

/* GLOSSY SHARP */

void bsdf_glossy_sharp_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float l_radius = l_areasizex;
	L = l_distance * L;
	vec3 R = -reflect(V, N);

	vec3 P = line_aligned_plane_intersect(vec3(0.0), R, L);
	bsdf = (distance_squared(P, L) < l_radius * l_radius) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	bsdf *= sphere_energy(l_radius);
}

void bsdf_glossy_sharp_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
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
	vec3 R = reflect(V, N);
	vec3 E = line_plane_intersect(vec3(0.0), R, L, lampz);

	/* Project it onto the light plane */
	vec3 projection = E - L;
	float A = dot(lampx, projection);
	float B = dot(lampy, projection);

	bsdf = (abs(A) < halfsize.x && abs(B) < halfsize.y) ? 1.0 : 0.0;

	/* Masking */
	bsdf *= (dot(-L, lampz) > 0.0) ? 1.0 : 0.0;
	bsdf *= (dot(R, lampz) > 0.0) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	bsdf *= rectangle_energy(l_areasizex, l_areasizey);
}

void bsdf_glossy_sharp_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	vec3 R = reflect(V, N);

	float l_radius = l_areasizex;
	float angle = atan(l_radius);

	float costheta = dot(-L, R);
	float cosangle = cos(angle);

	bsdf = (costheta > cosangle) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	bsdf *= disk_energy(l_radius);
	bsdf /= costheta * costheta * costheta;
	bsdf *= M_PI;
}

/* GLOSSY GGX */

void bsdf_glossy_ggx_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	if (roughness < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

#if 1
	/* MRP is twice as fast as LTC, does not exhibit crucial artifacts and is better looking */
	float l_radius = l_areasizex;

	shade_view(V, V);
	vec3 R = reflect(V, N);

	float energy_conservation = 1.0;
	most_representative_point(l_radius, 0.0, vec3(0.0), l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_ggx(N, L, V, roughness);

	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= sphere_energy(l_radius) * max(l_radius * l_radius, 1e-16); /* l_radius is already inside energy_conservation */
	bsdf *= M_PI;
#else
	/* LTC */
	float l_radius = max(0.007, l_areasizex);

	vec3 pos = V;
	shade_view(V, V);
	vec3 R = -reflect(V, N);
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

void bsdf_glossy_ggx_area_light(
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
	shade_view(V, V);
	V = -V;
	N = -N;

	vec3 lampx, lampy, lampz;
	vec2 halfsize = area_light_prepass(l_mat, l_areasizex, l_areasizey, l_areascale, lampx, lampy, lampz);

	vec3 points[4];
	area_light_points(l_coords, halfsize, lampx, lampy, points);

	float NV = max(dot(N, V), 1e-8);
	vec2 uv = ltc_coords(NV, sqrt(roughness));
	mat3 ltcmat = ltc_matrix(uv);

	bsdf = ltc_evaluate(N, V, pos, ltcmat, points);
	bsdf *= texture2D(unfltcmag, uv).r; /* Bsdf matching */

	bsdf *= step(0.0, -dot(L, lampz));
	bsdf *= M_1_2PI;
	bsdf *= rectangle_energy(l_areasizex, l_areasizey);
}

void bsdf_glossy_ggx_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	/* Correct ligth shape but uniform intensity
	 * Does not take into account the division by costheta^3 */
	if (roughness < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	vec3 R = reflect(V, N);

	float l_radius = l_areasizex;
	float angle = atan(l_radius);

	float costheta = dot(-L, R);
	float cosangle = cos(angle);

	float energy_conservation = 1.0;
	most_representative_point_disk(l_radius, 1.0, R, L, roughness, energy_conservation);

	bsdf = bsdf_ggx(N, L, V, roughness);
	bsdf *= energy_conservation;
}

/* GLOSSY BECKMANN */

void bsdf_glossy_beckmann_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	if (roughness < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	/* MRP is twice as fast as LTC, does not exhibit crucial artifacts and is better looking */
	float l_radius = l_areasizex;

	shade_view(V, V);
	vec3 R = reflect(V, N);

	float energy_conservation = 1.0; /* XXX TODO : Energy conservation is done for GGX */
	most_representative_point(l_radius, 0.0, vec3(0.0), l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_beckmann(N, L, V, roughness);

	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= sphere_energy(l_radius) * max(l_radius * l_radius, 1e-16); /* l_radius is already inside energy_conservation */
	bsdf *= M_PI;
}

void bsdf_glossy_beckmann_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	/* TODO Make the other ltc luts */
	bsdf_glossy_ggx_area_light( N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, 
		l_mat, roughness, ior, sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, bsdf);
}

void bsdf_glossy_beckmann_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	/* Correct ligth shape but uniform intensity
	 * Does not take into account the division by costheta^3 */
	if (roughness < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	vec3 R = reflect(V, N);

	float l_radius = l_areasizex;
	float angle = atan(l_radius);

	float costheta = dot(-L, R);
	float cosangle = cos(angle);

	float energy_conservation = 1.0;
	most_representative_point_disk(l_radius, 1.0, R, L, roughness, energy_conservation);

	bsdf = bsdf_beckmann(N, L, V, roughness);
	bsdf *= energy_conservation;
}

/* GLOSSY ASHIKhMIN SHIRLEY */

void bsdf_glossy_ashikhmin_shirley_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	if (roughness < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	/* MRP is twice as fast as LTC, does not exhibit crucial artifacts and is better looking */
	float l_radius = l_areasizex;

	shade_view(V, V);
	vec3 R = reflect(V, N);

	float energy_conservation = 1.0; /* XXX TODO : Energy conservation is done for GGX */
	most_representative_point(l_radius, 0.0, vec3(0.0), l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_ashikhmin_shirley(N, L, V, roughness);

	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= sphere_energy(l_radius) * max(l_radius * l_radius, 1e-16); /* l_radius is already inside energy_conservation */
	bsdf *= M_PI;
}

void bsdf_glossy_ashikhmin_shirley_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	/* TODO Make the other ltc luts */
	bsdf_glossy_ggx_area_light( N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, 
		l_mat, roughness, ior, sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, bsdf);
}

void bsdf_glossy_ashikhmin_shirley_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	/* Correct ligth shape but uniform intensity
	 * Does not take into account the division by costheta^3 */
	if (roughness < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	vec3 R = reflect(V, N);

	float l_radius = l_areasizex;
	float angle = atan(l_radius);

	float costheta = dot(-L, R);
	float cosangle = cos(angle);

	float energy_conservation = 1.0;
	most_representative_point_disk(l_radius, 1.0, R, L, roughness, energy_conservation);

	bsdf = bsdf_ashikhmin_shirley(N, L, V, roughness);
	bsdf *= energy_conservation;
}

/* -------- Image Based Lighting --------- */

void env_sampling_glossy_sharp(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	float ao_factor, out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);

	/* Precomputation */
	vec3 L = reflect(I, N);
	vec3 vL = (viewmat * vec4(L, 0.0)).xyz;

	/* Probe */
	vec4 sample_probe = sample_reflect(L) * specular_occlusion(dot(N, -I), ao_factor, 0.0);

#ifdef USE_SSR
	/* SSR */
	//ivec2 c = ivec2(gl_FragCoord.xy);
	//float jitter = float((c.x+c.y)&1) * 0.5; // Number between 0 and 1 for how far to bump the ray in stride units to conceal banding artifacts
	//setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */
	//float jitter = jitternoise.y * 1.0;
	float jitter = 0.0;

	vec2 hitpixel; vec3 hitco; float hitstep;

	bool hit = raycast(viewpos, vL, jitter, hitstep, hitpixel, hitco);
	float contrib = ssr_contribution(viewpos, hitstep, hit, hitco);

	//vec4 sample_ssr = texture2DLod(unfscenebuf, hitco.xy, 0);
	vec4 sample_ssr = texelFetch(unfscenebuf, ivec2(hitpixel.xy), 0);
	srgb_to_linearrgb(sample_ssr, sample_ssr);

	result = mix(sample_probe.rgb, sample_ssr.rgb, contrib);
	//result = mix(vec3(0.0), sample_ssr.rgb, contrib);
	//result = vec3(texelFetch(unfdepthbuf, ivec2(gl_FragCoord.xy) / int(pow(2,unfssrparam.x-1)), int(unfssrparam.x-1)).r);
#else
	result = sample_probe.rgb;
#endif
}

void env_sampling_glossy_ggx(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	float ao_factor, out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	make_orthonormals(N, T, B); /* Generate tangent space */
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */
	float a, a2; prepare_glossy(roughness, a, a2);

	/* Precomputation */
	float NV = max(1e-8, abs(dot(I, N)));
	float G1_V = G1_Smith_GGX(NV, a2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < unfbsdfsamples.x; i++) {
		vec3 H = sample_ggx(i, a2, N, T, B); /* Microfacet normal */
		vec3 L = reflect(I, H);
		float NL = dot(N, L);

		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = max(1e-8, dot(N, H)); /* cosTheta */
			float VH = max(1e-8, -dot(I, H));

			float pdf = pdf_ggx_reflect(NH, a2);

			vec4 sample = sample_reflect_pdf(L, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float brdf_pdf = bsdf_ggx_pdf(a2, NH, NL, VH, G1_V);

			out_radiance += NL * sample * brdf_pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y * specular_occlusion(NV, ao_factor, a2);
}

void env_sampling_glossy_beckmann(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	float ao_factor, out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	make_orthonormals(N, T, B); /* Generate tangent space */
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */
	float a, a2; prepare_glossy(roughness, a, a2);

	/* Precomputation */
	float NV = max(1e-8, abs(dot(I, N)));
	float G1_V = G1_Smith_beckmann(NV, a2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < unfbsdfsamples.x; i++) {
		vec3 H = sample_beckmann(i, a2, N, T, B); /* Microfacet normal */
		vec3 L = reflect(I, H);
		float NL = dot(N, L);

		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = max(1e-8, dot(N, H)); /* cosTheta */
			float VH = max(1e-8, -dot(I, H));

			float pdf = pdf_beckmann_reflect(NH, a2);

			vec4 sample = sample_reflect_pdf(L, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float brdf_pdf = bsdf_beckmann_pdf(a2, NH, NV, NL, VH, G1_V);

			out_radiance += NL * sample * brdf_pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y * specular_occlusion(NV, ao_factor, a2);
}

void env_sampling_glossy_ashikhmin_shirley(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	float ao_factor, out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	make_orthonormals(N, T, B); /* Generate tangent space */
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */
	float a, a2; prepare_glossy(roughness, a, a2);

	/* Precomputation */
	float NV = max(1e-8, abs(dot(I, N)));
	float n_x = 2.0 / a2 - 2.0;

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < unfbsdfsamples.x; i++) {
		vec3 H = sample_ashikhmin_shirley(i, n_x, N, T, B); /* Microfacet normal */
		float VH = dot(H, -I);
		if (VH < 0.0) H = -H;
		/* reflect I on H to get omega_in */
		vec3 L = I + (2.0 * VH) * H;
		float NL = dot(N, L);

		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = max(1e-8, dot(N, H)); /* cosTheta */
			VH = max(1e-8, abs(VH));
			NL = max(1e-8, NL);

			float pdf = pdf_ashikhmin_shirley_reflect(NH, VH, n_x);

			vec4 sample = sample_reflect_pdf(L, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float brdf_pdf = bsdf_ashikhmin_shirley_pdf(NV, NL, VH);

			out_radiance += NL * sample * brdf_pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y * specular_occlusion(NV, ao_factor, a2);
}

