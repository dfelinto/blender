/* -------- Utils Functions --------- */

/* XXX : This is improvised but it looks great */
vec3 sample_ggx_aniso(float nsample, float ax, float ay, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample);

	float min_a = min(ax,ay);
	float max_a = max(ax,ay);

	float z_max = sqrt( (1.0 - Xi.x) / ( 1.0 + pow(max_a,2) * Xi.x - Xi.x ) ); /* cos theta */
	float z_min = sqrt( (1.0 - Xi.x) / ( 1.0 + pow(min_a,2) * Xi.x - Xi.x ) ); /* cos theta */
	float r_max = sqrt( 1.0 - z_max * z_max ); /* sin theta */
	float r_min = sqrt( 1.0 - z_min * z_min ); /* sin theta */

	float x = Xi.y;
	float y = Xi.z;

	if (ax < ay) {
		x *= r_min;
		y *= r_max;
	}
	else {
		x *= r_max;
		y *= r_min;
	}

	float z = sqrt( 1.0 - x*x - y*y ); /* reconstructing Z */

	Ht = vec3(x, y, z); /* Global variable */

	return from_tangent_to_world(Ht, N, T, B);
}

float D_ggx_aniso_opti(float NH, float XH2, float YH2, float a2, float ax2, float ay2)
{
	float tmp = NH*NH + XH2/ax2 + YH2/ay2; /* Distributing NH² */
	return M_PI * a2 * tmp*tmp; /* Doing RCP at the end */
}

float pdf_ggx_aniso(float NH, float XH2, float YH2, float a2, float ax2, float ay2)
{
	float D = D_ggx_aniso_opti(NH, XH2, YH2, a2, ax2, ay2);
	return NH / D;
}

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

	float ax  = max(1e-4, roughness_x); /* Artifacts appear with roughness below this threshold */
	float ay  = max(1e-4, roughness_y); /* Artifacts appear with roughness below this threshold */
	float a2 = ax*ay;
	float ax2 = ax*ax;
	float ay2 = ay*ay;
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
	float G = G1_Smith(NV, alphaV2) * G1_Smith(NL, alphaL2); /* Doing RCP at the end */

	/* D_GGX */
	float D = D_ggx_aniso_opti(NH, XH2, YH2, a2, ax2, ay2);

	/* Denominator is canceled by G1_Smith */
	/* bsdf = D * G / (4.0 * NL * NV); /* Reference function */
	return NL / (D * G); /* NL to Fit cycles Equation : line. 345 in bsdf_microfacet.h */
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
	//R = normalize(mix(N, R, 1 - roughness)); //Dominant Direction
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
	/* TODO Find a better approximation */

	float rough_x, rough_y;
	prepare_aniso(N, roughness, aniso_rotation, T, anisotropy, rough_x, rough_y);

	if (max(rough_x, rough_y) < 1e-4 && l_areasizex == 0) {
		bsdf = 0.0;
		return;
	}

	float l_radius = l_areasizex;

	vec3 R = reflect(V, N);
	float energy_conservation = 1.0;

	/* GGX Spec */
	if(l_radius > 0.0){
		//roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

		/* energy preservation */
		// float SphereAngle = clamp( l_radius / l_distance, 0.0, 1.0);
		// energy_conservation = 2*a / (2*a + SphereAngle); //( x / (x + 0.5 * y))^2;
		// energy_conservation *= energy_conservation;

		float radius = sin(l_radius); //Disk radius
		float distance = cos(l_radius); // Distance to disk

		/* disk light */
		float LR = dot(L, R);
		vec3 S = normalize(R - LR * L);
		L = LR < distance ? normalize(distance * L + S * radius) : R;
	}

	L = normalize(L);
	bsdf = bsdf_ggx_aniso(N, T, L, V, rough_x, rough_y);
	bsdf *= energy_conservation;
}


/* -------- Image Based Lighting --------- */

/* TODO : fix this shit */
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
	float max_a2 = max_a*max_a;
	float min_a2 = min_a*min_a;
	float a2 = ax*ay;
	float NV = max(1e-8, abs(dot(I, N)));
	float VX2 = pow(dot(I, T), 2); /* cosPhiO² */
	float VY2 = pow(dot(I, B), 2); /* sinPhiO² */
	float G1_V = G1_Smith(NV, max_a2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (float i = 0; i < unfbsdfsamples.x; i++) {
		vec3 H = sample_ggx_aniso(i, ax, ay, N, T, B); /* Microfacet normal */
		vec3 L = reflect(I, H);

		vec3 H_brdf = sample_ggx(i, max_a2, N, T, B); /* Microfacet normal for the brdf */
		vec3 L_brdf = reflect(I, H_brdf);

		if (dot(N, L) > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = max(1e-8, dot(N, H));
			float XH2 = pow(dot(T, H), 2);
			float YH2 = pow(dot(B, H), 2);

			float pdf = pdf_ggx_aniso(NH, XH2, YH2, a2, ax2, ay2);

			vec4 sample = sample_reflect_pdf(L, roughness, pdf);

			/* Step 2 : Integrating BRDF */
			/* XXX : using the isotropic brdf + hack because aniso brdf fails */
			float VH = max(1e-8, -dot(I, H_brdf));
			float NL = max(0.0, dot(N, L_brdf));
			float brdf = bsdf_ggx_pdf(a2, NH, NL, VH, G1_V);

			/* XXX : this does not match for many parameters level. But Real solution fails so use custom one */
			out_radiance += sample * mix( NL * brdf, 1.3, pow(anisotropy,2)/1.3 );
		}
	}

	result = out_radiance.rgb * unfbsdfsamples.y;
}