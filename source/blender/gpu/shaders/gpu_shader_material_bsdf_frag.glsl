/* needed for uint type and bitwise operation */
#extension GL_EXT_gpu_shader4: enable

uniform mat4 rangemat = mat4(vec4(0.5, 0.0, 0.0, 0.0),
                           vec4(0.0, 0.5, 0.0, 0.0),
                           vec4(0.0, 0.0, 0.5, 0.0),
                           vec4(0.5, 0.5, 0.5, 1.0));

/* ----------- Parallax Correction -------------- */
void correct_ray(inout vec3 L, vec3 worldpos, mat4 correcmat, vec3 probevec)
{
#ifdef CORRECTION_NONE
	return;
#else
	vec3 localray = (correcmat * vec4(L, 0.0)).xyz;
	vec3 localpos = (correcmat * vec4(worldpos, 1.0)).xyz;

#ifdef CORRECTION_BOX
	/* https://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/ */
	vec3 firstplane  = (vec3( 1.0) - localpos) / localray;
	vec3 secondplane = (vec3(-1.0) - localpos) / localray;
	vec3 furthestplane = max(firstplane, secondplane);
	float dist = min(furthestplane.x, min(furthestplane.y, furthestplane.z));
#endif

#ifdef CORRECTION_ELLIPSOID
	/* ray sphere intersection */
	float a = dot(localray, localray);
	float b = dot(localray, localpos);
	float c = dot(localpos, localpos) - 1;

	float dist = 1e15f;
	float determinant = b * b - a * c;
	if (determinant >= 0)
		dist = (sqrt(determinant) - b) / a;
#endif

	/* Use Distance in WS directly to recover intersection */
	vec3 intersection = worldpos + L * dist;
	L = intersection - probevec;
#endif
}


/* ----------- Probe sampling wrappers -------------- */
#if 0
float distance_based_roughness(float dist_intersection_to_shaded, float dist_intersection_to_reflectioncam, float roughness)
{
	/* from Sebastien Lagarde
	* course_notes_moving_frostbite_to_pbr.pdf */
	float newroughness = clamp(roughness * dist_intersection_to_shaded / dist_intersection_to_reflectioncam, 0, roughness);
	return mix(newroughness, roughness, roughness);
}
#endif

vec2 calculate_distortion(sampler2D planarprobe, vec3 screenvec, vec3 worldnor, mat4 viewmat, vec3 planarfac, vec3 I, vec3 planenormal, vec2 random, float roughness, inout float lod)
{
	vec3 viewnor = (viewmat * vec4(worldnor, 0.0)).xyz;
	vec3 viewi = (viewmat * vec4(I, 0.0)).xyz;
	vec2 distortion = vec2(dot(viewnor, vec3(1.0, 0.0, 0.0)) - planarfac.x,
	                       dot(viewnor, vec3(0.0, 1.0, 0.0)) - planarfac.y);
	distortion += random;

	/* modulate intensity by distance to the viewer and by distance to the reflected object */
	float dist_view_to_shaded = planarfac.z;
	float dist_intersection_to_reflectioncam = texture2D(planarprobe, screenvec.xy + random / dist_view_to_shaded).a;
	float dist_intersection_to_shaded = dist_intersection_to_reflectioncam - dist_view_to_shaded; /* depth in alpha */

	/* test in case of background */
	if (dist_intersection_to_shaded > 0.0) {
		float distortion_scale = abs(dot(viewi * dist_intersection_to_shaded, -planenormal));
		distortion *= distortion_scale / (dist_view_to_shaded + 1.0);
	}

	return screenvec.xy + distortion;
}

vec4 sampleProbeLod(samplerCube probe, sampler2D planarprobe, vec3 cubevec, vec3 screenvec, vec3 worldnor, vec3 worldpos, mat4 viewmat, mat4 correcmat, vec3 planarfac, vec3 probevec, vec3 planarvec, vec2 random, float roughness, vec3 I, float lod)
{
	vec4 sample;
	correct_ray(cubevec, worldpos, correcmat, probevec);
	sample = textureCubeLod(probe, cubevec, min(lod, 5.0));

#ifdef PLANAR_PROBE
	vec4 sample_plane = vec4(0.0);
	vec2 co = calculate_distortion(planarprobe, screenvec, worldnor, viewmat, planarfac, I, planarvec, random, roughness, lod);

	if (co.x > 0.0 && co.x < 1.0 && co.y > 0.0 && co.y < 1.0)
		sample_plane = texture2DLod(planarprobe, co, lod);
	else
		sample_plane = sample;

	sample = mix(sample_plane, sample, clamp(roughness * 2.0 - 0.5, 0.0, 1.0));
#endif

	srgb_to_linearrgb(sample, sample);
	return sample;
}

vec4 sampleProbe(samplerCube probe, sampler2D planarprobe, vec3 cubevec, vec3 screenvec, vec3 worldnor, vec3 worldpos, mat4 viewmat, mat4 correcmat, vec3 planarfac, vec3 probevec, vec3 planarvec, vec2 random, float roughness, vec3 I)
{
	vec4 sample;
	correct_ray(cubevec, worldpos, correcmat, probevec);
	sample = textureCube(probe, cubevec);

#ifdef PLANAR_PROBE
	float lod = 0.0;
	vec2 co = calculate_distortion(planarprobe, screenvec, worldnor, viewmat, planarfac, I, planarvec, random, roughness, lod);

	if (co.x > 0.0 && co.x < 1.0 && co.y > 0.0 && co.y < 1.0)
		sample = texture2D(planarprobe, co);
#endif

	srgb_to_linearrgb(sample, sample);
	return sample;
}

vec3 vector_prepass(mat4 invviewmat, mat4 viewmat, mat4 reflectmat, vec3 viewpos, out vec3 worldpos, out vec3 refpos, inout vec3 planarvec, out vec3 planarfac)
{
	worldpos = (invviewmat * vec4(viewpos, 1.0)).xyz;

	vec3 I;
	shade_view(viewpos, I);
	I = (invviewmat * vec4(I, 0.0)).xyz;

#ifdef PLANAR_PROBE
	/* transposing plane orientation to view space */
	planarvec = (viewmat * vec4(planarvec, 0.0)).xyz;

	planarfac.x = dot(planarvec, vec3(1.0, 0.0, 0.0));
	planarfac.y = dot(planarvec, vec3(0.0, 1.0, 0.0));
	planarfac.z = -viewpos.z + 1.0;

	vec4 proj = rangemat * reflectmat * vec4(worldpos, 1.0);
	refpos = proj.xyz/proj.w;
#endif
	return I;
}

/* Tiny Encryption Algorithm (used for noise generation) */
uvec2 TEA(uvec2 v)
{
	/* set up */
	uint v0 = uint(v.x);
	uint v1 = uint(v.y);
	uint sum = 0u;

	/* cache key */
	uint k[4] = uint[4](0xA341316Cu , 0xC8013EA4u , 0xAD90777Du , 0x7E95761Eu );

	/* basic cycle start */
	for (int i = 0; i < 3; i++) {
		sum += 0x9e3779b9u;  /* a key schedule constant */
		v0 += ((v1 << 4u) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5u) + k[1]);
		v1 += ((v0 << 4u) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5u) + k[3]);
	}

	return uvec2(v0, v1);
}

/* Noise function could be replaced by a texture lookup */
uvec2 get_noise()
{
	uvec2 random = uvec2(gl_FragCoord.x, gl_FragCoord.y);
	random = TEA(random);
	random.x ^= uint( 21389u );
	random.y ^= uint( 49233u );
	return random;
}

/* From http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html */
uint radicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return bits;
}

vec2 hammersley_2d(uint i, uint N, uvec2 random) {
	float E1 = fract( float(i) / float(N) + float( random.x & uint(0xffff) ) / float(1<<16) );
	float E2 = float( radicalInverse_VdC(i) ^ uint(random.y) ) * 2.3283064365386963e-10;
	return vec2( E1, E2 );
}

vec3 importance_sample_hemisphere(vec2 Xi)
{
	float phi = M_2PI * Xi.y;

	float z = Xi.x; /* cos theta */
	float r = sqrt( 1.0f - z*z ); /* sin theta */
	float x = r * cos(phi);
	float y = r * sin(phi);

	return normalize(vec3(x, y, z));
}

vec3 importance_sample_GGX(vec2 Xi, float a2)
{
	float phi = M_2PI * Xi.y;

	float z = sqrt( (1.0 - Xi.x) / ( 1.0 + a2 * Xi.x - Xi.x ) ); /* cos theta */
	float r = sqrt( 1.0 - z * z ); /* sin theta */
	float x = r * cos(phi);
	float y = r * sin(phi);

	return normalize(vec3(x, y, z));
}

/* XXX : This is improvised but it looks great */
vec3 importance_sample_GGX_aniso(vec2 Xi, float ax, float ay)
{
	float phi = M_2PI * Xi.y;

	float min_a = min(ax,ay);
	float max_a = max(ax,ay);

	float z_max = sqrt( (1.0 - Xi.x) / ( 1.0 + pow(max_a,2) * Xi.x - Xi.x ) ); /* cos theta */
	float z_min = sqrt( (1.0 - Xi.x) / ( 1.0 + pow(min_a,2) * Xi.x - Xi.x ) ); /* cos theta */
	float r_max = sqrt( 1.0 - z_max * z_max ); /* sin theta */
	float r_min = sqrt( 1.0 - z_min * z_min ); /* sin theta */

	float x = cos(phi);
	float y = sin(phi);

	if (ax < ay) {
		x *= r_min;
		y *= r_max;
	}
	else {
		x *= r_max;
		y *= r_min;
	}

	float z = sqrt( 1.0 - x*x - y*y ); /* reconstructing Z */

	return normalize(vec3(x, y, z));
}

/* Second order Spherical Harmonics */
/* http://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/ */
vec3 spherical_harmonics_L2(vec3 N, vec3 sh0, vec3 sh1, vec3 sh2, vec3 sh3, vec3 sh4, vec3 sh5, vec3 sh6, vec3 sh7, vec3 sh8)
{
	vec3 sh = vec3(0.0);

	sh += 0.282095 * sh0;

	sh += -0.488603 * N.z * sh1;
	sh += 0.488603 * N.y * sh2;
	sh += -0.488603 * N.x * sh3;

	sh += 1.092548 * N.x * N.z * sh4;
	sh += -1.092548 * N.z * N.y * sh5;
	sh += 0.315392 * (3.0 * N.y * N.y - 1.0) * sh6;
	sh += -1.092548 * N.x * N.y * sh7;
	sh += 0.546274 * (N.x * N.x - N.z * N.z) * sh8;

	return sh;
}

void env_sampling_reflect_glossy(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N, float roughness, float precalc_lod_factor,
	                             samplerCube probe,
	                             sampler2D planar_reflection,
	                             sampler2D planar_refraction,
	                             mat4 correcmat,
	                             mat4 reflectmat,
	                             vec3 probevec,
	                             vec3 planarvec,
	                             out vec3 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
	vec3 T, B;
	make_orthonormals(N, T, B); /* Generate tangent space */

	uvec2 random = get_noise(); /* Noise to dither the samples */

	/* Precomputation */
	float a  = max(1e-8, roughness);
	float a2 = max(1e-8, a*a);
	float NV = max(1e-8, abs(dot(I, N)));

	float Vis_SmithV = G1_Smith(NV, a2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	float brdf = 0.0;
	float weight = 0.0;
	for (uint i = 0u; i < NUM_SAMPLE; i++) {
		vec2 Xi = hammersley_2d(i, NUM_SAMPLE, random);
		vec3 Ht = importance_sample_GGX(Xi, a2);
		vec3 H = from_tangent_to_world(Ht, N, T, B); /* Microfacet normal */
		vec3 L = reflect(I, H);
		float NL = max(0.0, dot(N, L));

		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = max(1e-8, dot(N, H)); /* cosTheta */

			float tmp = (NH * a2 - NH) * NH + 1.0 ;
			float D = a2 / (M_PI * tmp*tmp) ;
			float pdf = D * NH;

			float Lod = precalc_lod_factor - 0.5 * log2(pdf);

			out_radiance += sampleProbeLod(probe, planar_reflection, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, Ht.xy, roughness, I, Lod);
			weight++;

			/* Step 2 : Integrating BRDF : this must be replace by a LUT */
			float VH = max(1e-8, -dot(I, H));
			float G = Vis_SmithV * G1_Smith(NL, a2); /* rcp later */

			/* Li = SampleColor * NL
			 * brdf = D*G / (4*NL*NV) [denominator canceled by G]
			 * pdf = D * NH / (4 * VH) [D canceled later by D in brdf]
			 * out_radiance = Li * Spec / pdf; */
			brdf += NL * 4.0 * VH / (NH * G);
		}
	}

	result = (out_radiance.rgb / weight) * (brdf / NUM_SAMPLE);
}

void env_sampling_reflect_aniso(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N, float roughness, float anisotropy, float rotation, vec3 T,
                                float precalc_lod_factor,
	                            samplerCube probe,
	                            sampler2D planar_reflection,
	                            sampler2D planar_refraction,
	                            mat4 correcmat,
	                            mat4 reflectmat,
	                            vec3 probevec,
	                            vec3 planarvec,
	                            out vec3 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
	float rough_x, rough_y; vec3 B;
	prepare_aniso(N, roughness, -rotation, T, anisotropy, rough_x, rough_y);
	make_orthonormals_tangent(N, T, B);

	uvec2 random = get_noise(); /* Noise to dither the samples */
	uvec2 random2 = uvec2(gl_FragCoord.y, gl_FragCoord.x);
	random2 = TEA(random2);
	random2.x ^= uint( 49233u );
	random2.y ^= uint( 21389u );

	/* Precomputation */
	float ax  = max(1e-4, rough_x);
	float ay  = max(1e-4, rough_y);
	float ax2 = ax*ax;
	float ay2 = ay*ay;
	float max_a = min(1.0, max(ax, ay));
	float min_a = min(1.0, min(ax, ay));
	float max_a2 = max_a*max_a;
	float min_a2 = min_a*min_a;

	float NV = max(1e-8, abs(dot(I, N)));
	float VX2 = pow(dot(I, T), 2); /* cosPhiO² */
	float VY2 = pow(dot(I, B), 2); /* sinPhiO² */

	float Vis_SmithV = G1_Smith(NV, max_a2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	float brdf = 0.0;
	float weight = 0.0;
	/* Using twice the normal numbre of samples : TODO use more hammersley points */
	for (uint i = 0u; i < NUM_SAMPLE * 2u; i++) {

		if (i >= NUM_SAMPLE) random = random2;

		vec2 Xi = hammersley_2d(i % NUM_SAMPLE, NUM_SAMPLE, random);
		vec3 Ht = importance_sample_GGX_aniso(Xi, ax, ay);
		vec3 H = from_tangent_to_world(Ht, N, T, B); /* Microfacet normal */
		vec3 L = reflect(I, H);

		vec3 H_brdf = from_tangent_to_world( importance_sample_GGX(Xi, max_a2), N, T, B); /* Microfacet normal for the brdf */
		vec3 L_brdf = reflect(I, H_brdf);

		if (dot(N, L) > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = max(1e-8, dot(N, H));
			float XH2 = pow(dot(T, H), 2);
			float YH2 = pow(dot(B, H), 2);

			float tmp = NH*NH + XH2/ax2 + YH2/ay2; /* Distributing NH² */
			float D = 1 / (M_PI * min_a2 * tmp*tmp); /* XXX : using min_a2 : better results */
			float pdf = D * NH;

			float Lod = precalc_lod_factor - 0.5 * log2(pdf);

			out_radiance += sampleProbeLod(probe, planar_reflection, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, Ht.xy, roughness, I, Lod);
			weight++;

			/* Step 2 : Integrating BRDF : this must be replace by a LUT */
			/* XXX : using the isotropic brdf + hack because aniso brdf fails */
			float VH = max(1e-8, -dot(I, H_brdf));
			float NL = max(0.0, dot(N, L_brdf));
			float G = Vis_SmithV * G1_Smith(NL, max_a2); /* rcp later */

			/* Li = SampleColor * NL
			 * brdf = D*G / (4*NL*NV) [denominator canceled by G]
			 * pdf = D * NH / (4 * VH) [D canceled later by D in brdf]
			 * out_radiance = Li * Spec / pdf; */

			/* XXX : this does not match for many parameters level. But Real solution fails so use custom one */
			brdf += mix( NL * 4.0 * VH / (NH * G), 1.3, pow(anisotropy,2)/1.3 );
		}
	}

	result = (out_radiance.rgb / weight) * vec3(brdf / (NUM_SAMPLE * 2u));
}

void env_sampling_refract_glossy(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N, float ior, float roughness, float precalc_lod_factor,
	                             samplerCube probe,
	                             sampler2D planar_reflection,
	                             sampler2D planar_refraction,
	                             mat4 correcmat,
	                             mat4 reflectmat,
	                             vec3 probevec,
	                             vec3 planarvec,
	                             out vec3 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
	vec3 T, B; make_orthonormals(N, T, B); /* Generate tangent space */
	uvec2 random = get_noise(); /* Noise to dither the samples */

	ior = max(ior, 1e-5);

	/* Precomputation */
	float a  = max(1e-8, roughness);
	float a2 = max(1e-8, a*a);
	float NV = max(1e-8, abs(dot(I, N)));

	float Vis_SmithV = G1_Smith(NV, a2);

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	float brdf = 0.0;
	float weight = 1e-8;

	if (abs(ior - 1.0) < 1e-4) {
#ifndef PLANAR_PROBE
		vec4 env_sample = textureCube(probe, I);
#else
		vec4 env_sample = texture2D(planar_refraction, refpos.xy);
#endif
		srgb_to_linearrgb(env_sample, env_sample);
		out_radiance = env_sample;
		weight = 1.0;
		brdf = weight;
	}
	else {
		for (uint i = 0u; i < NUM_SAMPLE; i++) {
			vec2 Xi = hammersley_2d(i, NUM_SAMPLE, random);
			vec3 Ht = importance_sample_GGX(Xi, a2);
			vec3 H = from_tangent_to_world(Ht, N, T, B); /* Microfacet normal */
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

				float Ht2 = pow(ior * VH + LH, 2);

				float tmp = (NH * a2 - NH) * NH + 1.0 ;
				float D = a2 / (M_PI * tmp*tmp) ;
				float pdf = (VH * abs(LH)) * (ior*ior) * D * Vis_SmithV / (Ht2 * NV);

				float Lod = precalc_lod_factor - 0.5 * log2(pdf);

				out_radiance += sampleProbeLod(probe, planar_refraction, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, Ht.xy, roughness, I, Lod);
				weight++;

				/* Step 2 : Integrating BRDF : this must be replace by a LUT */
				float G_V = NL * 2.0 / G1_Smith(NL, a2); /* Balancing the adjustments made in G1_Smith */

				/* Li = SampleColor
				 * brtf = abs(VH*LH) * (ior*ior) * D * G / (Ht2 * NV)
				 * pdf = (VH * abs(LH)) * (ior*ior) * D * G(V) / (Ht2 * NV)
				 * out_radiance = Li * brtf / pdf; */

				brdf += G_V * abs(VH*LH) / (VH * abs(LH)); /* XXX : Not good enough, must be missing something */
			}
		}
	}

	result = (out_radiance.rgb / weight) * (brdf / weight); /* Dividing brdf by weight gives more brighter results */
}

void env_sampling_glass_glossy(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N, float ior, float roughness, float precalc_lod_factor,
	                           samplerCube probe,
	                           sampler2D planar_reflection,
	                           sampler2D planar_refraction,
	                           mat4 correcmat,
	                           mat4 reflectmat,
	                           vec3 probevec,
	                           vec3 planarvec,
	                           out vec3 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
	vec3 T, B; make_orthonormals(N, T, B); /* Generate tangent space */
	uvec2 random = get_noise(); /* Noise to dither the samples */

	ior = max(ior, 1e-5);

	/* Precomputation */
	float a  = max(1e-8, roughness);
	float a2 = max(1e-8, a*a);
	float NV = max(1e-8, abs(dot(I, N)));

	float Vis_SmithV = G1_Smith(NV, a2);

	/* Integrating Envmap */
	vec4 out_radiance_r = vec4(0.0);
	vec4 out_radiance_t = vec4(0.0);
	float brdf = 0.0;
	float btdf = 0.0;
	float weight_r = 1e-8;
	float weight_t = 1e-8;
	for (uint i = 0u; i < NUM_SAMPLE; i++) {
		vec4 env_sample;

		vec2 Xi = hammersley_2d(i, NUM_SAMPLE, random);
		vec3 Ht = importance_sample_GGX(Xi, a2);
		vec3 H = from_tangent_to_world(Ht, N, T, B); /* Microfacet normal */

		/* TODO : For ior < 1.0 && roughness > 0.0 fresnel becomes not accurate.*/
		float fresnel = fresnel_dielectric(I, H, (dot(H, -I) < 0.0) ? 1.0/ior : ior );

		/* reflection */
		vec3 L = reflect(I, H);
		float NL = max(0.0, dot(N, L));
		if (NL > 0.0) {
			/* Step 1 : Sampling Environment */
			float NH = max(1e-8, dot(N, H)); /* cosTheta */

			float tmp = (NH * a2 - NH) * NH + 1.0 ;
			float D = a2 / (M_PI * tmp*tmp) ;
			float pdf = D * NH;

			float Lod = precalc_lod_factor - 0.5 * log2(pdf);

			out_radiance_r += sampleProbeLod(probe, planar_reflection, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, Ht.xy, roughness, I, Lod) * fresnel;
			weight_r++;

			/* Step 2 : Integrating BRDF : this must be replace by a LUT */
			float VH = max(1e-8, -dot(I, H));
			float G = Vis_SmithV * G1_Smith(NL, a2); /* rcp later */

			/* See reflect glossy */
			brdf += NL * 4.0 * VH / (NH * G);
		}


		/* transmission */
		float eta = 1.0/ior;

		if (dot(H, -I) < 0.0) {
			H = -H;
			eta = ior;
		}

		L = refract(I, H, eta);
		NL = -dot(N, L);
		if (NL > 0.0 && fresnel != 1.0) {
			/* Step 1 : Sampling Environment */
			float NH = dot(N, H); /* cosTheta */
			float VH = dot(-I, H);
			float LH = dot(L, H);

			float Ht2 = pow(ior * VH + LH, 2);

			float tmp = (NH * a2 - NH) * NH + 1.0 ;
			float D = a2 / (M_PI * tmp*tmp) ;
			float pdf = (VH * abs(LH)) * (ior*ior) * D * Vis_SmithV / (Ht2 * NV);

			float Lod = precalc_lod_factor - 0.5 * log2(pdf);

			out_radiance_t += sampleProbeLod(probe, planar_refraction, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, Ht.xy, roughness, I, Lod) * (1.0 - fresnel);
			weight_t++;

			/* Step 2 : Integrating BRDF : this must be replace by a LUT */
			float G_V = NL * 2.0 / G1_Smith(NL, a2); /* Balancing the adjustments made in G1_Smith */

			/* See refract glossy */
			btdf += G_V * abs(VH*LH) / (VH * abs(LH)); /* XXX : Not good enough, must be missing something */
		}

	}

	result = (out_radiance_r.rgb / weight_r) * (brdf / NUM_SAMPLE) +
	       (out_radiance_t.rgb / weight_t) * (btdf / weight_t);
}

void env_sampling_glass_sharp(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N, float ior,
	                          samplerCube probe,
	                          sampler2D planar_reflection,
	                          sampler2D planar_refraction,
	                          mat4 correcmat,
	                          mat4 reflectmat,
	                          vec3 probevec,
	                          vec3 planarvec,
	                          out vec3 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);

	float eta = (gl_FrontFacing) ? 1.0/ior : ior;
	float fresnel = fresnel_dielectric(I, N, ior);
	vec4 env_sample;

	/* reflection */
	vec3 L = reflect(I, N);
	result += sampleProbe(probe, planar_reflection, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, vec2(0.0), 0.0, I).rgb * fresnel;

	/* transmission */
	L = refract(I, N, eta);
	result += sampleProbe(probe, planar_refraction, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, vec2(0.0), 0.0, I).rgb * (1.0 - fresnel);
}

void env_sampling_reflect_sharp(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N,
                                samplerCube probe,
                                sampler2D planar_reflection,
                                sampler2D planar_refraction,
                                mat4 correcmat,
                                mat4 reflectmat,
                                vec3 probevec,
                                vec3 planarvec,
                                out vec3 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
	vec3 L = reflect(I, N);

	result = sampleProbe(probe, planar_reflection, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, vec2(0.0), 0.0, I).rgb;
}

void env_sampling_refract_sharp(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N, float eta,
                                samplerCube probe,
                                sampler2D planar_reflection,
                                sampler2D planar_refraction,
                                mat4 correcmat,
                                mat4 reflectmat,
                                vec3 probevec,
                                vec3 planarvec,
	                            out vec3 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
	vec3 L = refract(I, N, (gl_FrontFacing) ? 1.0/eta : eta);

	result = sampleProbe(probe, planar_refraction, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, vec2(0.0), 0.0, I).rgb;
}

void env_sampling_velvet(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N, float sigma, float precalc_lod_factor,
                         samplerCube probe,
                         sampler2D planar_reflection,
                         sampler2D planar_refraction,
                         mat4 correcmat,
                         mat4 reflectmat,
                         vec3 probevec,
                         vec3 planarvec,
                         out vec3 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
	vec3 T, B; make_orthonormals(N, T, B); /* Generate tangent space */

	uvec2 random = get_noise(); /* Noise to dither the samples */

	/* Precomputation */
	sigma = max(sigma, 1e-2);
	float m_1_sig2 = 1 / (sigma * sigma);
	float NV = max(1e-5, abs(dot(I, N)));

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (uint i = 0u; i < NUM_SAMPLE; i++) {
		vec2 Xi = hammersley_2d(i, NUM_SAMPLE, random);
		vec3 Ht = importance_sample_hemisphere(Xi);
		vec3 L = from_tangent_to_world(Ht, N, T, B);
		vec3 H = normalize(L - I);

		float NL = max(0.0, dot(N, L));
		float NH = dot(N, H); /* cosTheta */

		if (NL != 0.0 && abs(NH) < 1.0-1e-5) {
			/* Step 1 : Sampling Environment */
			const float pdf = 0.5 * M_1_PI;
			float Lod = precalc_lod_factor - 0.5 * log2(pdf);
			vec4 irradiance = sampleProbeLod(probe, planar_reflection, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, Ht.xy, 1.0, I, Lod);

			/* Step 2 : Integrating BRDF*/
			float VH = max(abs(dot(I, H)), 1e-5);

			float NHdivVH = NH / VH;
			NHdivVH = max(NHdivVH, 1e-5);

			float fac1 = 2 * abs(NHdivVH * NV);
			float fac2 = 2 * abs(NHdivVH * NL);

			float sinNH2 = 1 - NH * NH;
			float sinNH4 = sinNH2 * sinNH2;
			float cotan2 = (NH * NH) / sinNH2;

			float D = exp(-cotan2 * m_1_sig2) * m_1_sig2 * M_1_PI / sinNH4;
			float G = min(1.0, min(fac1, fac2)); // TODO: derive G from D analytically

			/* 0.25 * (D * G) / (NV * pdf); */
			float brdf = 0.5 * M_PI * (D * G) / NV;

			out_radiance += irradiance * brdf;
		}
	}

	result = out_radiance.rgb / NUM_SAMPLE;
}



void env_sampling_diffuse(vec3 viewpos, mat4 invviewmat, mat4 viewmat, vec3 N, float roughness, float precalc_lod_factor,
                          samplerCube probe,
                          sampler2D planar_reflection,
                          sampler2D planar_refraction,
                          mat4 correcmat,
                          mat4 reflectmat,
                          vec3 probevec,
                          vec3 planarvec,
                          vec3 sh0, vec3 sh1, vec3 sh2, vec3 sh3,
                          vec3 sh4, vec3 sh5, vec3 sh6, vec3 sh7, vec3 sh8, out vec4 result)
{
	/* Lambert */
	vec3 lambert_diff = spherical_harmonics_L2(N, sh0, sh1, sh2, sh3, sh4, sh5, sh6, sh7, sh8);

	if (roughness < 1e-5) {
		/* early out */
		result = vec4(lambert_diff, 1.0);
		return;
	}

	/* Oren Nayar */
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
	vec3 T, B; make_orthonormals(N, T, B); /* Generate tangent space */

	uvec2 random = get_noise(); /* Noise to dither the samples */

	float NV = max(1e-8, abs(dot(I, N)));

	/* Integrating Envmap */
	vec4 out_radiance = vec4(0.0);
	for (uint i = 0u; i < NUM_SAMPLE; i++) {
		vec2 Xi = hammersley_2d(i, NUM_SAMPLE, random);
		vec3 Ht = importance_sample_hemisphere(Xi);
		vec3 L = from_tangent_to_world(Ht, N, T, B);
		vec3 H = normalize(L - I);

		float NL = max(0.0, dot(N, L));

		if (NL != 0.0) {
			/* Step 1 : Sampling Environment */
			const float pdf = 0.5 * M_1_PI;
			float Lod = precalc_lod_factor - 0.5 * log2(pdf);
			vec4 irradiance = sampleProbeLod(probe, planar_reflection, L, refpos, N, worldpos, viewmat, correcmat, planarfac, probevec, planarvec, Ht.xy, 1.0, I, Lod);

			/* Step 2 : Integrating BRDF*/
			float LV = max(0.0, dot(L, -I) );
			float sigma = roughness;
			float div = 1.0 / (M_PI + ((3.0 * M_PI - 4.0) / 6.0) * sigma);

			float A = 1.0 * div;
			float B = sigma * div;

			float s = LV - NL * NV;
			float t = mix(1.0, max(NL, NV), step(0.0, s));

			float brdf = NL * (A + B * s / t);

			out_radiance += irradiance * brdf * 2.0 * M_PI; /* 1/pdf */
		}
	}

	vec3 oren_nayar_diff = out_radiance.rgb / NUM_SAMPLE;

	result = vec4(mix(lambert_diff, oren_nayar_diff, roughness), 1.0);
}

void env_sampling_transparent(vec3 viewpos, mat4 invviewmat, mat4 viewmat,
                              samplerCube probe,
                              sampler2D planar_reflection,
                              sampler2D planar_refraction,
                              mat4 correcmat,
                              mat4 reflectmat,
                              vec3 probevec,
                              vec3 planarvec,
                              out vec4 result)
{
	vec3 worldpos, refpos;
	vec3 planarfac;
	vec3 I = vector_prepass(invviewmat, viewmat, reflectmat, viewpos, worldpos, refpos, planarvec, planarfac);
#ifndef PLANAR_PROBE
	result = textureCube(probe, I);
#else
	result = texture2D(planar_refraction, refpos.xy);
#endif
	srgb_to_linearrgb(result, result);
}
