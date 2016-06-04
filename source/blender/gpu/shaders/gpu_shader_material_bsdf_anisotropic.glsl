/* -------- Utils Functions --------- */

/* From 
 * Importance Sampling Microfacet-Based BSDFs with the Distribution of Visible Normals
 * Supplemental Material 2/2 */
vec3 sample_ggx_aniso(float nsample, float ax, float ay, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);

	float tmp = sqrt( Xi.x / (1.0 - Xi.x) );

	float x = ax * tmp * Xi.y;
	float y = ay * tmp * Xi.z;

	Ht = normalize(vec3(x, y, 1.0)); /* Global variable */

	return from_tangent_to_world(Ht, N, T, B);
}

vec3 sample_beckmann_aniso(float nsample, float ax, float ay, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);

	float tmp = sqrt( -log(Xi.x) );

	float x = ax * tmp * Xi.y;
	float y = ay * tmp * Xi.z;

	Ht = normalize(vec3(x, y, 1.0)); /* Global variable */

	return from_tangent_to_world(Ht, N, T, B);
}

float bsdf_ashikhmin_shirley_sample_first_quadrant(float n_x, float n_y, inout vec3 Xi, out float phi)
{
	phi = atan(sqrt((n_x + 1.0) / (n_y + 1.0)) * (Xi.z / Xi.y));
	Xi.y = cos(phi);
	Xi.z = sin(phi);
	return pow(Xi.x, 1.0 / (n_x * Xi.y*Xi.y + n_y * Xi.z*Xi.z + 1.0));
}

vec3 sample_ashikhmin_shirley_aniso(float nsample, float n_x, float n_y, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);
	float phi, z;

	if(Xi.x < 0.25) { /* first quadrant */
		Xi.x = 4.0 * Xi.x;
		z = bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, Xi, phi);
	}
	else if(Xi.x < 0.5) { /* second quadrant */
		Xi.x = 4.0 * (.5 - Xi.x);
		z = bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, Xi, phi);
		phi = M_PI - phi;
		Xi.y = cos(phi);
		Xi.z = sin(phi);
	}
	else if(Xi.x < 0.75) { /* third quadrant */
		Xi.x = 4.0 * (Xi.x - 0.5);
		z = bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, Xi, phi);
		phi = M_PI + phi;
		Xi.y = cos(phi);
		Xi.z = sin(phi);
	}
	else { /* fourth quadrant */
		Xi.x = 4.0 * (1.0 - Xi.x);
		z = bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, Xi, phi);
		phi = M_2PI - phi;
		Xi.y = cos(phi);
		Xi.z = sin(phi);
	}

	float r = sqrt( 1.0 - z * z ); /* sin theta */
	float x = r * Xi.y;
	float y = r * Xi.z;

	Ht = vec3(x, y, z); /* Global variable */

	return from_tangent_to_world(Ht, N, T, B);
}

float D_ggx_aniso_opti(float NH, float XH2, float YH2, float a2, float ax2, float ay2)
{
	float tmp = NH*NH + XH2/ax2 + YH2/ay2; /* Distributing NH² */
	return M_PI * a2 * tmp*tmp; /* Doing RCP at the end */
}

float D_beckmann_aniso(float NH, float XH, float YH, float a2, float ax, float ay)
{
	float sx = -XH / (NH * ax);
	float sy = -YH / (NH * ay);

	float NH2 = NH * NH;

	return exp(-sx*sx - sy*sy) / (M_PI * a2 * NH2 * NH2);
}

float pdf_ggx_aniso(float NH, float XH2, float YH2, float a2, float ax2, float ay2)
{
	float D = D_ggx_aniso_opti(NH, XH2, YH2, a2, ax2, ay2);
	return NH / D;
}

float pdf_beckmann_aniso(float NH, float XH, float YH, float a2, float ax, float ay)
{
	float D = D_beckmann_aniso(NH, XH, YH, a2, ax, ay);
	return NH / D;
}

float pdf_ashikhmin_shirley_aniso(float NH, float VH, float XH2, float YH2, float n_x, float n_y)
{
	float e = (n_x * XH2 + n_y * YH2) / (1.0 - NH*NH);
	float lobe = pow(NH, e);
	float norm = sqrt((n_x + 1.0)*(n_y + 1.0)) * 0.125 * M_1_PI;

	return norm * lobe / VH;
}

/* TODO : this could be precomputed */
void prepare_aniso(vec3 N, float roughness, float rotation, inout vec3 T, inout float anisotropy, out float rough_x, out float rough_y)
{
	anisotropy = clamp(anisotropy, -0.99, 0.99);

	if (anisotropy < 0.0) {
		rough_x = roughness / (1.0 + anisotropy);
		rough_y = roughness * (1.0 + anisotropy);
	}
	else {
		rough_x = roughness * (1.0 - anisotropy);
		rough_y = roughness / (1.0 - anisotropy);
	}

	T = axis_angle_rotation(T, N, rotation * M_2PI); /* rotate tangent around normal */
}

/* -------- BSDF --------- */

float bsdf_ggx_aniso(vec3 N, vec3 T, vec3 L, vec3 V, float roughness_x, float roughness_y)
{
	/* GGX Spec Anisotropic */
	/* A few note about notations :
	 * I is the cycles term for Incoming Light, Noted L here (light vector)
	 * Omega (O) is the cycles term for the Outgoing Light, Noted V here (View vector) */
	N = normalize(N);
	vec3 X = T, Y, Z = N; /* Inside cycles Z=Normal; X=Tangent; Y=Bitangent; */
	make_orthonormals_tangent(Z, X, Y);
	vec3 H = normalize(L + V);

	float ax, ax2; prepare_glossy(roughness_x, ax, ax2);
	float ay, ay2; prepare_glossy(roughness_y, ay, ay2);
	float a2 = ax*ay;

	float NH = max(1e-8, dot(N, H));
	float NL = max(1e-8, dot(N, L));
	float NV = max(1e-8, dot(N, V));
	float VX2 = pow(dot(V, X), 2); /* cosPhiO² */
	float VY2 = pow(dot(V, Y), 2); /* sinPhiO² */
	float LX2 = pow(dot(L, X), 2); /* cosPhiI² */
	float LY2 = pow(dot(L, Y), 2); /* sinPhiI² */
	float XH2 = pow(dot(X, H), 2);
	float YH2 = pow(dot(Y, H), 2);

	/* G_Smith_GGX */
	float alphaV2 = (VX2 * ax2 + VY2 * ay2) / (VX2 + VY2);
	float alphaL2 = (LX2 * ax2 + LY2 * ay2) / (LX2 + LY2);
	float G = G1_Smith_GGX(NV, alphaV2) * G1_Smith_GGX(NL, alphaL2); /* Doing RCP at the end */

	/* D_GGX */
	float D = D_ggx_aniso_opti(NH, XH2, YH2, a2, ax2, ay2);

	/* Denominator is canceled by G1_Smith */
	/* bsdf = D * G / (4.0 * NL * NV); /* Reference function */
	return NL / (D * G); /* NL to Fit cycles Equation : line. 345 in bsdf_microfacet.h */
}

/* This one returns the brdf already divided by the pdf */
float bsdf_ggx_aniso_pdf(float ax2, float ay2, float LX2, float LY2, float NH, float NV, float NL, float VH, float G1_V)
{
	float alphaL2 = (LX2 * ax2 + LY2 * ay2) / (LX2 + LY2);
	float G = G1_V * G1_Smith_GGX(NL, alphaL2);

	/* Denominator is canceled by G1_Smith
	 * brdf = D * G / (4.0 * NL * NV)
	 * pdf = D * NH / (4 * VH) [D canceled later by D in brdf] */
	 return 4.0 * VH / (NH * G); /* brdf / pdf */
}

float bsdf_beckmann_aniso(vec3 N, vec3 T, vec3 L, vec3 V, float roughness_x, float roughness_y)
{
	/* Beckmann Spec Anisotropic */
	/* A few note about notations :
	 * I is the cycles term for Incoming Light, Noted L here (light vector)
	 * Omega (O) is the cycles term for the Outgoing Light, Noted V here (View vector) */
	N = normalize(N);
	vec3 X = T, Y, Z = N; /* Inside cycles Z=Normal; X=Tangent; Y=Bitangent; */
	make_orthonormals_tangent(Z, X, Y);
	vec3 H = normalize(L + V);

	float ax, ax2; prepare_glossy(roughness_x, ax, ax2);
	float ay, ay2; prepare_glossy(roughness_y, ay, ay2);
	float a2 = ax*ay;

	float NH = max(1e-8, dot(N, H));
	float NL = max(1e-8, dot(N, L));
	float NV = max(1e-8, dot(N, V));
	float VX2 = pow(dot(V, X), 2); /* cosPhiO² */
	float VY2 = pow(dot(V, Y), 2); /* sinPhiO² */
	float LX2 = pow(dot(L, X), 2); /* cosPhiI² */
	float LY2 = pow(dot(L, Y), 2); /* sinPhiI² */
	float XH = dot(X, H);
	float YH = dot(Y, H);

	float alphaV2 = (VX2 * ax2 + VY2 * ay2) / (VX2 + VY2);
	float alphaL2 = (LX2 * ax2 + LY2 * ay2) / (LX2 + LY2);
	float G = G1_Smith_beckmann(NV, alphaV2) * G1_Smith_beckmann(NL, alphaL2);

	float D = D_beckmann_aniso(NH, XH, YH, a2, ax, ay);

	return NL * D * G * 0.25 / (NL * NV);
}

/* This one returns the brdf already divided by the pdf */
float bsdf_beckmann_aniso_pdf(float ax2, float ay2, float LX2, float LY2, float NH, float NV, float NL, float VH, float G1_V)
{
	float alphaL2 = (LX2 * ax2 + LY2 * ay2) / (LX2 + LY2);
	float G = G1_V * G1_Smith_beckmann(NL, alphaL2);

	/* brdf = D * G / (4.0 * NL * NV)
	 * pdf = D * NH / (4 * VH) [D canceled later by D in brdf] */
	return G * VH / (NH * NV * NL); /* brdf / pdf */
}

float bsdf_ashikhmin_shirley_aniso(vec3 N, vec3 T, vec3 L, vec3 V, float roughness_x, float roughness_y)
{
	/* Ashikmin Shirley Spec Anisotropic */
	/* A few note about notations :
	 * I is the cycles term for Incoming Light, Noted L here (light vector)
	 * Omega (O) is the cycles term for the Outgoing Light, Noted V here (View vector) */
	N = normalize(N);
	vec3 X = T, Y, Z = N; /* Inside cycles Z=Normal; X=Tangent; Y=Bitangent; */
	make_orthonormals_tangent(Z, X, Y);
	vec3 H = normalize(L + V);

	float ax, ax2; prepare_glossy(roughness_x, ax, ax2);
	float ay, ay2; prepare_glossy(roughness_y, ay, ay2);
	float a2 = ax*ay;
	float n_x = 2.0 / ax2 - 2.0;
	float n_y = 2.0 / ay2 - 2.0;

	float NL = max(dot(N, L), 1e-6);
	float NV = max(dot(N, V), 1e-6);
	float NH = max(dot(N, H), 1e-6);
	float VH = max(abs(dot(V, H)), 1e-6);
	float XH2 = max(pow(dot(X, H), 2), 1e-6);
	float YH2 = max(pow(dot(Y, H), 2), 1e-6);

	float pump = 1.0 / max(1e-6, VH * max(NL, NV));

	float e = max(n_x * XH2 + n_y * YH2, 1e-4) / max(1.0 - NH*NH, 1e-4); /* Precision problem here */
	float lobe = pow(NH, e);
	float norm = sqrt((n_x + 1.0)*(n_y + 1.0)) * 0.125 * M_1_PI;

	return NL * norm * lobe * pump;
}

/* -------- Preview Lights --------- */

void node_bsdf_anisotropic_lights(vec4 color, float roughness, float anisotropy, float rotation, vec3 N, vec3 T, vec3 V, vec4 ambient_light, out vec4 result)
{
	N = normalize(N);
	shade_view(V, V); V = -V;

	float rough_x, rough_y;
	prepare_aniso(N, roughness, rotation, T, anisotropy, rough_x, rough_y);
	vec3 accumulator = ambient_light.rgb;

	if (max(rough_x, rough_y) <= 1e-4) {
		result = vec4(accumulator * color.rgb, 1.0); //Should take roughness into account -> waiting LUT
		return;
	}

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_color = gl_LightSource[i].specular.rgb;

		accumulator += light_color * bsdf_ggx_aniso(N, T, L, V, rough_x, rough_y);
	}

	result = vec4(accumulator * color.rgb, 1.0);
}


/* -------- Physical Lights --------- */

/* ANISOTROPIC GGX */

void bsdf_anisotropic_ggx_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{

	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

	if (max(rough_x, rough_y) < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	float l_radius = l_areasizex;

	vec3 R = reflect(V, N);
	float energy_conservation = 1.0;
	most_representative_point(l_radius, 0.0, vec3(0.0), l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_ggx_aniso(N, T, L, V, rough_x, rough_y);

	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= sphere_energy(l_radius) * max(l_radius * l_radius, 1e-16); /* l_radius is already inside energy_conservation */
	bsdf *= M_PI; /* XXX : !!! Very Empirical, Fit cycles power */
}

void bsdf_anisotropic_ggx_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	if (max(l_areasizex, l_areasizey) < 1e-6) {
		bsdf = 0.0;
		return;
	}


	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

	l_areasizex *= l_areascale.x;
	l_areasizey *= l_areascale.y;

	/* Used later for Masking : Use the real Light Vector */
	vec3 lampz = normalize( (l_mat * vec4(0.0,0.0,1.0,0.0) ).xyz );
	float masking = max(dot( normalize(-L), lampz), 0.0);

	vec3 R = reflect(V, N);

	float energy_conservation = 1.0;
	float max_size = max(l_areasizex, l_areasizey);
	float min_size = min(l_areasizex, l_areasizey);
	vec3 lampVec = (l_areasizex > l_areasizey) ? normalize( (l_mat * vec4(1.0,0.0,0.0,0.0) ).xyz ) : normalize( (l_mat * vec4(0.0,1.0,0.0,0.0) ).xyz );

	most_representative_point(min_size/2, max_size-min_size, lampVec, l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_ggx_aniso(N, T, L, V, rough_x, rough_y);

	/* energy_conservation */
	float LineAngle = clamp( (max_size-min_size) / l_distance, 0.0, 1.0);
	float energy_conservation_line = energy_conservation * ( roughness / clamp(roughness + 0.5 * LineAngle, 0.0, 1.1));

	/* XXX : Empirical modification for low roughness matching */
	float energy_conservation_mod = energy_conservation * (1 + roughness) / ( max_size/min_size );
	energy_conservation = mix(energy_conservation_mod, energy_conservation_line, min(roughness/0.3, 0.9*(1.1-roughness)/0.1));

	/* As we represent the Area Light by a tube light we must use a custom energy conservation */
	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= masking;
	bsdf *= 23.2;  /* XXX : !!! Very Empirical, Fit cycles power */
}

void bsdf_anisotropic_ggx_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

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

	bsdf = bsdf_ggx_aniso(N, T, L, V, rough_x, rough_y);
	bsdf *= energy_conservation;
}

/* ANISOTROPIC BECKMANN */

void bsdf_anisotropic_beckmann_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{

	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

	if (max(rough_x, rough_y) < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	float l_radius = l_areasizex;

	vec3 R = reflect(V, N);
	float energy_conservation = 1.0;
	most_representative_point(l_radius, 0.0, vec3(0.0), l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_beckmann_aniso(N, T, L, V, rough_x, rough_y);

	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= sphere_energy(l_radius) * max(l_radius * l_radius, 1e-16); /* l_radius is already inside energy_conservation */
	bsdf *= M_PI; /* XXX : !!! Very Empirical, Fit cycles power */
}

void bsdf_anisotropic_beckmann_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	if (max(l_areasizex, l_areasizey) < 1e-6) {
		bsdf = 0.0;
		return;
	}

	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

	l_areasizex *= l_areascale.x;
	l_areasizey *= l_areascale.y;

	/* Used later for Masking : Use the real Light Vector */
	vec3 lampz = normalize( (l_mat * vec4(0.0,0.0,1.0,0.0) ).xyz );
	float masking = max(dot( normalize(-L), lampz), 0.0);

	vec3 R = reflect(V, N);

	float energy_conservation = 1.0;
	float max_size = max(l_areasizex, l_areasizey);
	float min_size = min(l_areasizex, l_areasizey);
	vec3 lampVec = (l_areasizex > l_areasizey) ? normalize( (l_mat * vec4(1.0,0.0,0.0,0.0) ).xyz ) : normalize( (l_mat * vec4(0.0,1.0,0.0,0.0) ).xyz );

	most_representative_point(min_size/2, max_size-min_size, lampVec, l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_beckmann_aniso(N, T, L, V, rough_x, rough_y);

	/* energy_conservation */
	float LineAngle = clamp( (max_size-min_size) / l_distance, 0.0, 1.0);
	float energy_conservation_line = energy_conservation * ( roughness / clamp(roughness + 0.5 * LineAngle, 0.0, 1.1));

	/* XXX : Empirical modification for low roughness matching */
	float energy_conservation_mod = energy_conservation * (1 + roughness) / ( max_size/min_size );
	energy_conservation = mix(energy_conservation_mod, energy_conservation_line, min(roughness/0.3, 0.9*(1.1-roughness)/0.1));

	/* As we represent the Area Light by a tube light we must use a custom energy conservation */
	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= masking;
	bsdf *= 23.2;  /* XXX : !!! Very Empirical, Fit cycles power */
}

void bsdf_anisotropic_beckmann_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

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

	bsdf = bsdf_beckmann_aniso(N, T, L, V, rough_x, rough_y);
	bsdf *= energy_conservation;
}

/* ANISOTROPIC ASHIKHMIN SHIRLEY */

void bsdf_anisotropic_ashikhmin_shirley_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{

	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

	if (max(rough_x, rough_y) < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	float l_radius = l_areasizex;

	vec3 R = reflect(V, N);
	float energy_conservation = 1.0;
	most_representative_point(l_radius, 0.0, vec3(0.0), l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_ashikhmin_shirley_aniso(N, T, L, V, rough_x, rough_y);

	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= sphere_energy(l_radius) * max(l_radius * l_radius, 1e-16); /* l_radius is already inside energy_conservation */
	bsdf *= M_PI; /* XXX : !!! Very Empirical, Fit cycles power */
}

void bsdf_anisotropic_ashikhmin_shirley_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	if (max(l_areasizex, l_areasizey) < 1e-6) {
		bsdf = 0.0;
		return;
	}

	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

	l_areasizex *= l_areascale.x;
	l_areasizey *= l_areascale.y;

	/* Used later for Masking : Use the real Light Vector */
	vec3 lampz = normalize( (l_mat * vec4(0.0,0.0,1.0,0.0) ).xyz );
	float masking = max(dot( normalize(-L), lampz), 0.0);

	vec3 R = reflect(V, N);

	float energy_conservation = 1.0;
	float max_size = max(l_areasizex, l_areasizey);
	float min_size = min(l_areasizex, l_areasizey);
	vec3 lampVec = (l_areasizex > l_areasizey) ? normalize( (l_mat * vec4(1.0,0.0,0.0,0.0) ).xyz ) : normalize( (l_mat * vec4(0.0,1.0,0.0,0.0) ).xyz );

	most_representative_point(min_size/2, max_size-min_size, lampVec, l_distance, R, L, roughness, energy_conservation);
	bsdf = bsdf_ashikhmin_shirley_aniso(N, T, L, V, rough_x, rough_y);

	/* energy_conservation */
	float LineAngle = clamp( (max_size-min_size) / l_distance, 0.0, 1.0);
	float energy_conservation_line = energy_conservation * ( roughness / clamp(roughness + 0.5 * LineAngle, 0.0, 1.1));

	/* XXX : Empirical modification for low roughness matching */
	float energy_conservation_mod = energy_conservation * (1 + roughness) / ( max_size/min_size );
	energy_conservation = mix(energy_conservation_mod, energy_conservation_line, min(roughness/0.3, 0.9*(1.1-roughness)/0.1));

	/* As we represent the Area Light by a tube light we must use a custom energy conservation */
	bsdf *= energy_conservation / (l_distance * l_distance);
	bsdf *= masking;
	bsdf *= 23.2;  /* XXX : !!! Very Empirical, Fit cycles power */
}

void bsdf_anisotropic_ashikhmin_shirley_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

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

	bsdf = bsdf_ashikhmin_shirley_aniso(N, T, L, V, rough_x, rough_y);
	bsdf *= energy_conservation;
}

/* -------- Image Based Lighting --------- */

void env_sampling_aniso_ggx(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	float ao_factor, out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	float rough_x, rough_y; prepare_aniso(N, roughness, -aniso_rotation, T, anisotropy, rough_x, rough_y);
	float ax, ax2; prepare_glossy(rough_x, ax, ax2);
	float ay, ay2; prepare_glossy(rough_y, ay, ay2);
	make_orthonormals_tangent(N, T, B);
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	/* Precomputation */
	float max_a = min(1.0, max(ax, ay));
	float min_a = min(1.0, min(ax, ay));
	float a2 = ax*ay;
	float NV = max(1e-8, abs(dot(I, N)));
	float VX2 = pow(dot(I, T), 2); /* cosPhiO² */
	float VY2 = pow(dot(I, B), 2); /* sinPhiO² */
	float alphaV2 = (VX2 * ax2 + VY2 * ay2) / (VX2 + VY2);
	float G1_V = G1_Smith_GGX(NV, alphaV2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < unfbsdfsamples.x; i++) {
		vec3 H = sample_ggx_aniso(i, ax, ay, N, T, B); /* Microfacet normal */
		vec3 L = reflect(I, H);
		float NL = dot(N, L);

		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = Ht.z;
			float XH2 = Ht.x * Ht.x;
			float YH2 = Ht.y * Ht.y;

			float pdf = pdf_ggx_aniso(NH, XH2, YH2, a2, ax, ay);

			vec4 sample = sample_reflect_pdf(L, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float VH = max(1e-8, -dot(I, H));
			float LX2 = pow(dot(L, T), 2); /* cosPhiI² */
			float LY2 = pow(dot(L, B), 2); /* sinPhiI² */
			float brdf_pdf = bsdf_ggx_aniso_pdf(ax2, ay2, LX2, LY2, NH, NV, NL, VH, G1_V);

			out_radiance += NL * sample * brdf_pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y * specular_occlusion(NV, ao_factor, a2);
}

void env_sampling_aniso_beckmann(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	float ao_factor, out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	float rough_x, rough_y; prepare_aniso(N, roughness, -aniso_rotation, T, anisotropy, rough_x, rough_y);
	float ax, ax2; prepare_glossy(rough_x, ax, ax2);
	float ay, ay2; prepare_glossy(rough_y, ay, ay2);
	make_orthonormals_tangent(N, T, B);
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	/* Precomputation */
	float a2 = ax*ay;
	float NV = max(1e-8, abs(dot(I, N)));
	float VX2 = pow(dot(I, T), 2); /* cosPhiO² */
	float VY2 = pow(dot(I, B), 2); /* sinPhiO² */
	float alphaV2 = (VX2 * ax2 + VY2 * ay2) / (VX2 + VY2);
	float G1_V = G1_Smith_beckmann(NV, alphaV2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < unfbsdfsamples.x; i++) {
		vec3 H = sample_beckmann_aniso(i, ax, ay, N, T, B); /* Microfacet normal */
		vec3 L = reflect(I, H);
		float NL = dot(N, L);

		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = Ht.z;
			float XH = Ht.x;
			float YH = Ht.y;

			float pdf = pdf_beckmann_aniso(NH, XH, YH, a2, ax, ay);

			vec4 sample = sample_reflect_pdf(L, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float VH = max(1e-8, -dot(I, H));
			float LX2 = pow(dot(L, T), 2); /* cosPhiI² */
			float LY2 = pow(dot(L, B), 2); /* sinPhiI² */
			float brdf_pdf = bsdf_beckmann_aniso_pdf(ax2, ay2, LX2, LY2, NH, NV, NL, VH, G1_V);

			out_radiance += NL * sample * brdf_pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y * specular_occlusion(NV, ao_factor, a2);
}

void env_sampling_aniso_ashikhmin_shirley(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	float ao_factor, out vec3 result)
{
	/* Setup */
	vector_prepass(viewpos, N, invviewmat, viewmat);
	float rough_x, rough_y; prepare_aniso(N, roughness, -aniso_rotation, T, anisotropy, rough_x, rough_y);
	float ax, ax2; prepare_glossy(rough_x, ax, ax2);
	float ay, ay2; prepare_glossy(rough_y, ay, ay2);
	make_orthonormals_tangent(N, T, B);
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	/* Precomputation */
	float a2 = ax*ay;
	float NV = max(1e-8, abs(dot(I, N)));
	float VX2 = pow(dot(I, T), 2); /* cosPhiO² */
	float VY2 = pow(dot(I, B), 2); /* sinPhiO² */

	float n_x = 2.0 / ax2 - 2.0;
	float n_y = 2.0 / ay2 - 2.0;

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < unfbsdfsamples.x; i++) {
		vec3 H = sample_ashikhmin_shirley_aniso(i, n_x, n_y, N, T, B); /* Microfacet normal */
		float VH = dot(H, -I);
		if (VH < 0.0) H = -H;
		/* reflect I on H to get omega_in */
		vec3 L = I + (2.0 * VH) * H;
		float NL = dot(N, L);

		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = Ht.z;
			float XH2 = Ht.x * Ht.x;
			float YH2 = Ht.y * Ht.y;
			float VH = max(1e-8, -dot(I, H));

			float pdf = pdf_ashikhmin_shirley_aniso(NH, VH, XH2, YH2, n_x, n_y);

			vec4 sample = sample_reflect_pdf(L, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			float brdf_pdf = bsdf_ashikhmin_shirley_pdf(NV, NL, VH); /* Same as isotropic */

			out_radiance += NL * sample * brdf_pdf;
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y * specular_occlusion(NV, ao_factor, a2);
}