/* ------- Defines -------- */

/* Number of default opengl lights */
#define NUM_LIGHTS 3

/* SSR Maximum iterations */
#define MAX_SSR_STEPS 128
#define MAX_SSR_REFINE_STEPS 16

/* Importance sampling Max iterations */
#define BSDF_SAMPLES 1024

/* Linearly Transformed Cosines  */
#define LTC_LUT_SIZE 64

/* needed for uint type and bitwise operation */
#extension GL_EXT_gpu_shader4: enable

/* ------- PBR Uniform --------- */

uniform samplerCube unfprobe;
uniform sampler2D unfreflect;
uniform sampler2D unfrefract;
uniform sampler2D unfltcmat;
uniform sampler2D unfltcmag;
uniform sampler2D unfssr;
uniform sampler2D unfjitter;
uniform sampler1D unflutsamples;
uniform float unflodfactor;
uniform vec2 unfbsdfsamples;
uniform vec3 unfsh0;
uniform vec3 unfsh1;
uniform vec3 unfsh2;
uniform vec3 unfsh3;
uniform vec3 unfsh4;
uniform vec3 unfsh5;
uniform vec3 unfsh6;
uniform vec3 unfsh7;
uniform vec3 unfsh8;
uniform vec3 unfprobepos;
uniform vec3 unfplanarvec;
uniform vec4 unfssrparam;
uniform vec2 unfssrparam2;
uniform mat4 unfprobecorrectionmat;
uniform mat4 unfplanarreflectmat;
uniform mat4 unfpixelprojmat;

/* ------- Global Variables -------- */

vec3 worldpos, refpos;
vec3 viewnor, viewi;
vec3 planarfac, planarvec;
vec3 I, B, Ht;
vec2 jitternoise;

/* ------- Convenience functions --------- */

vec3 mul(mat3 m, vec3 v) { return m * v; }
mat3 mul(mat3 m1, mat3 m2) { return m1 * m2; }

float saturate(float a) { return clamp(a, 0.0, 1.0); }

float distance_squared(vec2 a, vec2 b) { a -= b; return dot(a, a); }
float distance_squared(vec3 a, vec3 b) { a -= b; return dot(a, a); }

float hypot(float x, float y) { return sqrt(x*x + y*y); }

float inverse_distance(vec3 V) { return max( 1 / length(V), 1e-8); }

/* --------- Geometric Utils Functions --------- */

vec3 axis_angle_rotation(vec3 point, vec3 axis, float angle)
{
	axis = normalize(axis);
	float s = sin(angle);
	float c = cos(angle);
	float oc = 1.0 - c;

	mat3 mat = mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
	                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
	                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c);

	return mat * point;
}

void viewN_to_shadeN(vec3 N, out vec3 shadeN)
{
	shadeN = normalize(-N);
}

void make_orthonormals(vec3 N, out vec3 T, out vec3 B)
{
	vec3 UpVector = abs(N.z) < 0.99999 ? vec3(0.0,0.0,1.0) : vec3(1.0,0.0,0.0);
	T = normalize( cross(UpVector, N) );
	B = cross(N, T);
}

void make_orthonormals_tangent(vec3 N, inout vec3 T, out vec3 B)
{
	B = normalize( cross(N, T) );
	T = cross(B, N);
}

void default_coordinates(vec3 attr_orco, out vec3 generated)
{
	generated = attr_orco * 0.5 + vec3(0.5);
}

vec3 from_tangent_to_world( vec3 vector, vec3 N, vec3 T, vec3 B)
{
	return T * vector.x + B * vector.y + N * vector.z;
}

vec3 from_world_to_tangent( vec3 vector, vec3 N, vec3 T, vec3 B)
{
	return vec3( dot(T, vector), dot(B, vector), dot(N, vector));
}

float line_plane_intersect_dist(vec3 lineorigin, vec3 linedirection, vec3 planeorigin, vec3 planenormal)
{
    return dot(planenormal, planeorigin - lineorigin) / dot(planenormal, linedirection);
}

vec3 line_plane_intersect(vec3 lineorigin, vec3 linedirection, vec3 planeorigin, vec3 planenormal)
{
	float dist = line_plane_intersect_dist(lineorigin, linedirection, planeorigin, planenormal);
	return lineorigin + linedirection * dist;
}

void most_representative_point(float l_radius, float l_lenght, vec3 l_Y,
	                                     float l_distance, vec3 R, inout vec3 L,
	                                     inout float roughness, inout float energy_conservation)
{
	L = l_distance * L;

	/* Tube Light */
	if(l_lenght>0){
		roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

		// Use Tube Light specular instead of a plane.
		// Energy conservation
		// asin(x) is angle to sphere, atan(x) is angle to disk, saturate(x) is free and in the middle
		//float LineAngle = clamp( l_lenght / l_distance, 0.0, 1.0);

		//energy_conservation *= roughness / clamp(roughness + 0.5 * LineAngle, 0.0, 1.0);

		/* Closest point on line segment to ray */
		vec3 L01 = l_Y * l_lenght;
		vec3 L0 = L - 0.5 * L01;
		vec3 L1 = L + 0.5 * L01;

		/* Shortest distance */
		float a = l_lenght * l_lenght;
		float b = dot(R, L01);
		float t = saturate(dot( L0, b*R - L01 ) / (a - b*b));
		L = L0 + t * L01;
	}

	/* Sphere Light */
	if(l_radius>0){
		roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

		/* energy preservation */
		float SphereAngle = saturate(l_radius / l_distance);
		energy_conservation *= pow(roughness / saturate(roughness + 0.5 * SphereAngle), 2.0);

		/* sphere light */
		vec3 closest_point_on_ray =  dot(L, R) * R;
		vec3 center_to_ray = closest_point_on_ray - L;
		/* closest point on sphere */
		L = L + center_to_ray * saturate(l_radius * inverse_distance(center_to_ray));
	}

	L = normalize(L);
}

vec2 area_light_prepass(mat4 lmat, inout float areasizex, inout float areasizey, vec2 areascale, out vec3 lampx, out vec3 lampy, out vec3 lampz)
{
	lampx = normalize( (lmat * vec4(1.0,0.0,0.0,0.0) ).xyz ); //lamp right axis
	lampy = normalize( (lmat * vec4(0.0,1.0,0.0,0.0) ).xyz ); //lamp up axis
	lampz = normalize( (lmat * vec4(0.0,0.0,1.0,0.0) ).xyz ); //lamp projection axis

	areasizex *= areascale.x;
	areasizey *= areascale.y;

	return vec2(areasizex / 2.0, areasizey / 2.0);
}


/* ------ Linearly Transformed Cosines ------ */
/* From https://eheitzresearch.wordpress.com/415-2/ */

void area_light_points(vec3 lco, vec2 halfsize, vec3 lampx, vec3 lampy, out vec3 points[4])
{
	vec3 ex = lampx * halfsize.x;
	vec3 ey = lampy * halfsize.y;

	points[0] = lco - ex + ey;
	points[1] = lco + ex + ey;
	points[2] = lco + ex - ey;
	points[3] = lco - ex - ey;
}

float integrate_edge(vec3 v1, vec3 v2)
{
    float cosTheta = dot(v1, v2);
    cosTheta = clamp(cosTheta, -0.9999, 0.9999);

    float theta = acos(cosTheta);
    float res = cross(v1, v2).z * theta / sin(theta);

    return res;
}

int clip_quad_to_horizon(inout vec3 L[5])
{
	// detect clipping config
	int config = 0;
	if (L[0].z > 0.0) config += 1;
	if (L[1].z > 0.0) config += 2;
	if (L[2].z > 0.0) config += 4;
	if (L[3].z > 0.0) config += 8;

	// clip
	int n = 0;

	if (config == 0)
	{
		// clip all
	}
	else if (config == 1) // V1 clip V2 V3 V4
	{
		n = 3;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 2) // V2 clip V1 V3 V4
	{
		n = 3;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 3) // V1 V2 clip V3 V4
	{
		n = 4;
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
		L[3] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 4) // V3 clip V1 V2 V4
	{
		n = 3;
		L[0] = -L[3].z * L[2] + L[2].z * L[3];
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
	}
	else if (config == 5) // V1 V3 clip V2 V4) impossible
	{
		n = 0;
	}
	else if (config == 6) // V2 V3 clip V1 V4
	{
		n = 4;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 7) // V1 V2 V3 clip V4
	{
		n = 5;
		L[4] = -L[3].z * L[0] + L[0].z * L[3];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 8) // V4 clip V1 V2 V3
	{
		n = 3;
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
		L[1] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] =  L[3];
	}
	else if (config == 9) // V1 V4 clip V2 V3
	{
		n = 4;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[2].z * L[3] + L[3].z * L[2];
	}
	else if (config == 10) // V2 V4 clip V1 V3) impossible
	{
		n = 0;
	}
	else if (config == 11) // V1 V2 V4 clip V3
	{
		n = 5;
		L[4] = L[3];
		L[3] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 12) // V3 V4 clip V1 V2
	{
		n = 4;
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
	}
	else if (config == 13) // V1 V3 V4 clip V2
	{
		n = 5;
		L[4] = L[3];
		L[3] = L[2];
		L[2] = -L[1].z * L[2] + L[2].z * L[1];
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
	}
	else if (config == 14) // V2 V3 V4 clip V1
	{
		n = 5;
		L[4] = -L[0].z * L[3] + L[3].z * L[0];
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
	}
	else if (config == 15) // V1 V2 V3 V4
	{
		n = 4;
	}

	if (n == 3)
		L[3] = L[0];
	if (n == 4)
		L[4] = L[0];

	return n;
}

vec2 ltc_coords(float cosTheta, float roughness)
{
	float theta = acos(cosTheta);
	vec2 coords = vec2(roughness, theta/(0.5*3.14159));

	// scale and bias coordinates, for correct filtered lookup
	return coords * (LTC_LUT_SIZE - 1.0) / LTC_LUT_SIZE + 0.5 / LTC_LUT_SIZE;
}

mat3 ltc_matrix(vec2 coord)
{
	// load inverse matrix
	vec4 t = texture2D(unfltcmat, coord);
	mat3 Minv = mat3(
		vec3(  1,   0, t.y),
		vec3(  0, t.z,   0),
		vec3(t.w,   0, t.x)
	);

	return Minv;
}

float ltc_evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4])
{
	// construct orthonormal basis around N
	vec3 T1, T2;
	T1 = normalize(V - N*dot(V, N));
	T2 = cross(N, T1);

	// rotate area light in (T1, T2, R) basis
	Minv = mul(Minv, transpose(mat3(T1, T2, N)));

	// polygon (allocate 5 vertices for clipping)
	vec3 L[5];
	L[0] = mul(Minv, points[0] - P);
	L[1] = mul(Minv, points[1] - P);
	L[2] = mul(Minv, points[2] - P);
	L[3] = mul(Minv, points[3] - P);

	int n = clip_quad_to_horizon(L);

	if (n == 0)
		return 0.0;

	// project onto sphere
	L[0] = normalize(L[0]);
	L[1] = normalize(L[1]);
	L[2] = normalize(L[2]);
	L[3] = normalize(L[3]);
	L[4] = normalize(L[4]);

	// integrate
	float sum = 0.0;

	sum += integrate_edge(L[0], L[1]);
	sum += integrate_edge(L[1], L[2]);
	sum += integrate_edge(L[2], L[3]);
	if (n >= 4)
		sum += integrate_edge(L[3], L[4]);
	if (n == 5)
		sum += integrate_edge(L[4], L[0]);

	return max(0.0, -sum); /* One sided */
}

/* ------- Fresnel ---------*/

float fresnel_dielectric(vec3 Incoming, vec3 Normal, float eta)
{
	/* compute fresnel reflectance without explicitly computing
	 * the refracted direction */
	float c = abs(dot(Incoming, Normal));
	float g = eta * eta - 1.0 + c * c;
	float result;

	if(g > 0.0) {
		g = sqrt(g);
		float A =(g - c)/(g + c);
		float B =(c *(g + c)- 1.0)/(c *(g - c)+ 1.0);
		result = 0.5 * A * A *(1.0 + B * B);
	}
	else {
		result = 1.0;  /* TIR (no refracted component) */
	}

	return result;
}


/* ------- Energy Conversion for lights ------- */
/* from Sebastien Lagarde
 * course_notes_moving_frostbite_to_pbr.pdf */

float sphere_energy(float radius)
{
	radius = max(radius, 1e-8);
	return 0.25 * M_1_PI2 / (radius*radius) /* 1/(4*r²*Pi²) */
		* M_PI2 * 10.0;  /* XXX : Empirical, Fit cycles power */
}

float disk_energy(float radius)
{
	radius = max(radius, 1e-8);
	return M_1_PI2 / (radius*radius); /* 1/(r²*Pi²) */
}

float tube_energy(float radius, float width)
{
	radius = max(radius, 1e-8);
	return 0.5 * M_1_PI2 / (radius * (width + 2 * radius)); /* 1/(4*r²*Pi²) + 1/(2*r*w*Pi²) */
}

float rectangle_energy(float width, float height)
{
	return M_1_PI / (width*height) /* 1/(w*h*Pi) */
		* 80.0;  /* XXX : Empirical, Fit cycles power */
}

/* ----------- Parallax Correction -------------- */

void parallax_correct_ray(inout vec3 L)
{
#ifdef CORRECTION_NONE
	return;
#else
	vec3 localray = (unfprobecorrectionmat * vec4(L, 0.0)).xyz;
	vec3 localpos = (unfprobecorrectionmat * vec4(worldpos, 1.0)).xyz;

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

	float dist = 1e15;
	float determinant = b * b - a * c;
	if (determinant >= 0)
		dist = (sqrt(determinant) - b) / a;
#endif

	/* Use Distance in WS directly to recover intersection */
	vec3 intersection = worldpos + L * dist;
	L = intersection - unfprobepos;
#endif
}

/* ----------- Probe sampling wrappers -------------- */

void vector_prepass(vec3 viewpos, vec3 worldnor, mat4 invviewmat, mat4 viewmat)
{
	worldpos = (invviewmat * vec4(viewpos, 1.0)).xyz;

	shade_view(viewpos, viewi);
	direction_transform_m4v3(viewi, invviewmat, I);

#ifdef PLANAR_PROBE
	/* View Normals */
	direction_transform_m4v3(worldnor, viewmat, viewnor);

	/* transposing plane orientation to view space */
	direction_transform_m4v3(unfplanarvec, viewmat, planarvec);

	planarfac.x = dot(planarvec, vec3(1.0, 0.0, 0.0));
	planarfac.y = dot(planarvec, vec3(0.0, 1.0, 0.0));
	planarfac.z = -viewpos.z + 1.0;

	vec4 proj = unfplanarreflectmat * vec4(worldpos, 1.0);
	refpos = proj.xyz/proj.w;
#endif
}

#if 0
float distance_based_roughness(float dist_intersection_to_shaded, float dist_intersection_to_reflectioncam, float roughness)
{
	/* from Sebastien Lagarde
	* course_notes_moving_frostbite_to_pbr.pdf */
	float newroughness = clamp(roughness * dist_intersection_to_shaded / dist_intersection_to_reflectioncam, 0, roughness);
	return mix(newroughness, roughness, roughness);
}
#endif

vec2 get_distorded_refl_uv(sampler2D planarprobe)
{
	vec2 distortion = vec2(dot(viewnor, vec3(1.0, 0.0, 0.0)) - planarfac.x,
	                       dot(viewnor, vec3(0.0, 1.0, 0.0)) - planarfac.y);
	distortion += Ht.xy;

	/* modulate intensity by distance to the viewer and by distance to the reflected object */
	float dist_view_to_shaded = planarfac.z;
	float dist_intersection_to_reflectioncam = texture2D(planarprobe, refpos.xy + Ht.xy / dist_view_to_shaded).a;
	float dist_intersection_to_shaded = dist_intersection_to_reflectioncam - dist_view_to_shaded; /* depth in alpha */

	/* test in case of background */
	if (dist_intersection_to_shaded > 0.0) {
		float distortion_scale = abs(dot(viewi * dist_intersection_to_shaded, -planarvec));
		distortion *= distortion_scale / (dist_view_to_shaded + 1.0);
	}

	return refpos.xy + distortion;
}

vec4 sample_probe_pdf(vec3 cubevec, float pdf)
{
	vec4 sample;

	float lod = unflodfactor - 0.5 * log2(pdf);

	parallax_correct_ray(cubevec);
	sample = textureCubeLod(unfprobe, cubevec, lod);

	srgb_to_linearrgb(sample, sample);
	return sample;
}

vec4 sample_probe_pdf(sampler2D planarprobe, vec3 cubevec, float roughness, float pdf)
{
	vec4 sample;

	float lod = unflodfactor - 0.5 * log2(pdf);

	parallax_correct_ray(cubevec);
	sample = textureCubeLod(unfprobe, cubevec, lod);

#ifdef PLANAR_PROBE
	vec4 sample_plane = vec4(0.0);
	vec2 co = get_distorded_refl_uv(planarprobe);
	if (co.x > 0.0 && co.x < 1.0 && co.y > 0.0 && co.y < 1.0)
		sample_plane = texture2DLod(planarprobe, co, lod);
	else
		sample_plane = sample;
	sample = mix(sample_plane, sample, clamp(roughness * 2.0 - 0.5, 0.0, 1.0));
#endif

	srgb_to_linearrgb(sample, sample);
	return sample;
}

vec4 sample_reflect_pdf(vec3 L, float roughness, float pdf)
{
	return sample_probe_pdf(unfreflect, L, roughness, pdf);
}

vec4 sample_refract_pdf(vec3 L, float roughness, float pdf)
{
	return sample_probe_pdf(unfrefract, L, roughness, pdf);
}

vec4 sample_probe(sampler2D planarprobe, vec3 cubevec)
{
	vec4 sample;

	/* Cubemap */
	parallax_correct_ray(cubevec);
	sample = textureCube(unfprobe, cubevec);

#ifdef PLANAR_PROBE
	/* Planar */
	vec2 co = get_distorded_refl_uv(planarprobe);
	if (co.x > 0.0 && co.x < 1.0 && co.y > 0.0 && co.y < 1.0)
		sample = texture2D(planarprobe, co);
#endif

	srgb_to_linearrgb(sample, sample);
	return sample;
}

vec4 sample_reflect(vec3 L)
{
	return sample_probe(unfreflect, L);
}

vec4 sample_refract(vec3 L)
{
	return sample_probe(unfrefract, L);
}

/* ------- Sampling Random Ray -------- */

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

void setup_noise(vec2 fragcoord)
{
	ivec2 texel = ivec2( mod(fragcoord.x, 64), mod(fragcoord.y, 64));
	jitternoise = texelFetch(unfjitter, texel, 0).rg * 0.5 + 1.0; /* Global variable */
}

vec3 hammersley_2d(float i)
{
	vec3 Xi; /* Theta, cos(Phi), sin(Phi) */

	Xi.x = i * unfbsdfsamples.y; /* i/samples */
	Xi.x = fract(Xi.x + jitternoise.x);

	int u = int(mod(i + jitternoise.y * BSDF_SAMPLES, BSDF_SAMPLES));
	Xi.yz = texelFetch(unflutsamples, u, 0).rg;

	return Xi;
}

/* ------- Screen Space Raycasting ---------*/

#if 0 /* 2D raymarch (More efficient) */

/* By Morgan McGuire and Michael Mara at Williams College 2014
 * Released as open source under the BSD 2-Clause License
 * http://opensource.org/licenses/BSD-2-Clause
 * http://casual-effects.blogspot.fr/2014/08/screen-space-ray-tracing.html */


void swapIfBigger(inout float a, inout float b)
{
	if (a > b) {
		float temp = a;
		a = b;
		b = temp;
	}
}

/* 2D raycast : still have some bug that needs to be sorted out before being usable */
bool raycast(vec3 ray_origin, vec3 ray_dir, float jitter, out float hitstep, out vec2 hitpixel, out vec3 hitco)
{
	/* ssr_parameters */
	float thickness = 	unfssrparam.x; /* Camera space thickness to ascribe to each pixel in the depth buffer */
	float nearz = 		unfssrparam.y; /* Near plane distance (Negative number) */
	float ray_step = 	unfssrparam.z; /* 2D : Step in horizontal or vertical pixels between samples. 3D : View space step between samples */
	float maxstep = 	unfssrparam.w; /* Maximum number of iteration when raymarching */
	float maxdistance = unfssrparam2.x; /* Maximum distance from ray origin */
	float attenuation = unfssrparam2.y; /* Attenuation factor for screen edges and ray step fading */

	/* Clip ray to a near plane in 3D */
	if ((ray_origin.z + ray_dir.z * ray_length) > nearz)
		ray_length = (nearz - ray_origin.z) / ray_dir.z;

	vec3 ray_end = ray_dir * ray_length + ray_origin;

	/* Project into screen space */
	vec4 H0 = unfpixelprojmat * vec4(ray_origin, 1.0);
	vec4 H1 = unfpixelprojmat * vec4(ray_end, 1.0);

	/* There are a lot of divisions by w that can be turned into multiplications
	* at some minor precision loss...and we need to interpolate these 1/w values
	* anyway. */
	float k0 = 1.0 / H0.w;
	float k1 = 1.0 / H1.w;

	/* Switch the original points to values that interpolate linearly in 2D */
	vec3 Q0 = ray_origin * k0;
	vec3 Q1 = ray_end * k1;

	/* Screen-space endpoints */
	vec2 P0 = H0.xy * k0;
	vec2 P1 = H1.xy * k1;

	/* [Optional clipping to frustum sides here] */

	/* Initialize to off screen */
	hitpixel = vec2(-1.0, -1.0);

	/* If the line is degenerate, make it cover at least one pixel
	 * to not have to handle zero-pixel extent as a special case later */
	P1 += vec2((distance_squared(P0, P1) < 0.0001) ? 0.01 : 0.0);

	vec2 delta = P1 - P0;

	/* Permute so that the primary iteration is in x to reduce large branches later.
	 * After this, "x" is the primary iteration direction and "y" is the secondary one
	 * If it is a more-vertical line, create a permutation that swaps x and y in the output
	 * and directly swizzle the inputs. */
	bool permute = false;
	if (abs(delta.x) < abs(delta.y)) {
		permute = true;
		delta = delta.yx;
		P1 = P1.yx;
		P0 = P0.yx;
	}

    /* Track the derivatives */
    float step_sign = sign(delta.x);
    float invdx = step_sign / delta.x;
    vec2 dP = vec2(step_sign, invdx * delta.y);
    vec3 dQ = (Q1 - Q0) * invdx;
    float dk = (k1 - k0) * invdx;

	/* Calculate pixel stride based on distance of ray origin from camera. */
	// float stride_scale = 1.0 - min( 1.0, -ray_origin.z);
	// stride = 1.0 + stride_scale * stride;

    /* Scale derivatives by the desired pixel stride */
	dP *= stride; dQ *= stride; dk *= stride;

    /* Offset the starting values by the jitter fraction */
	P0 += dP * jitter; Q0 += dQ * jitter; k0 += dk * jitter;

	/* Slide each value from the start of the ray to the end */
    vec3 Q = Q0; vec2 P = P0; float k = k0;

	/* We track the ray depth at +/- 1/2 pixel to treat pixels as clip-space solid
	 * voxels. Because the depth at -1/2 for a given pixel will be the same as at
	 * +1/2 for the previous iteration, we actually only have to compute one value
	 * per iteration. */
	float prev_zmax = ray_origin.z;
    float zmax = prev_zmax, zmin = prev_zmax;
    float scene_zmax = zmax + 1e4;

    /* P1.x is never modified after this point, so pre-scale it by
     * the step direction for a signed comparison */
    float end = P1.x * step_sign;

    bool hit = false;

	for (hitstep = 0.0; hitstep < MAX_SSR_STEPS && !hit; hitstep++)
	{
		 /* Ray finished & no hit*/
		if ((hitstep > maxstep) || ((P.x * step_sign) > end)) break;

		P += dP; Q.z += dQ.z; k += dk;

		hitpixel = permute ? P.yx : P;
		zmin = prev_zmax;
		zmax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
		prev_zmax = zmax;
		swapIfBigger(zmin, zmax);

		scene_zmax = texelFetch(unfssr, ivec2(hitpixel), 0).a;

		/* background case */
		if (scene_zmax == 1.0) scene_zmax = -1e16;

		hit = ((zmax >= scene_zmax - thickness) && (zmin <= scene_zmax));
	}

	hitco = (Q0 + dQ * hitstep) / k;
	return hit;
}

#else /* 3D raymarch */

float getDeltaDepth(sampler2D unfssr, vec3 hitco)
{
	vec4 co = unfpixelprojmat * vec4(hitco, 1.0);
	co.xy /= co.w;
	float depth = texelFetch(unfssr, ivec2(co.xy), 0).a;

	/* background case */
	if (depth == 1.0) depth = -1e16;

	return depth - hitco.z;
}

void binary_search(vec3 ray_dir, sampler2D unfssr, inout vec3 hitco)
{
    for (int i = 0; i < MAX_SSR_REFINE_STEPS; i++) {
		ray_dir *= 0.5;
        hitco -= ray_dir;
        float delta = getDeltaDepth(unfssr, hitco);
        if (delta < 0.0) hitco += ray_dir;
    }
}

/* 3D raycast */
bool raycast(vec3 ray_origin, vec3 ray_dir, float jitter, out float hitstep, out vec2 hitpixel, out vec3 hitco)
{
	/* ssr_parameters */
	float thickness = 	unfssrparam.x; /* Camera space thickness to ascribe to each pixel in the depth buffer */
	float ray_step = 	unfssrparam.z; /* 2D : Step in horizontal or vertical pixels between samples. 3D : View space step between samples */
	float maxstep = 	unfssrparam.w; /* Maximum number of iteration when raymarching */

	ray_dir *= ray_step;

	//thickness = 2 * ray_step;

	hitco = ray_origin;
	hitco += ray_dir * jitter;
	hitpixel = vec2(0.0);
	
	for (hitstep = 0.0; hitstep < MAX_SSR_STEPS && hitstep < maxstep; hitstep++) {
		hitco += ray_dir;
		float delta = getDeltaDepth(unfssr, hitco);
		if (delta > 0.0 && delta < thickness) {
			/* Refine hit */
			binary_search(ray_dir, unfssr, hitco);
			/* Find texel coord */
			vec4 pix = unfpixelprojmat * vec4(hitco, 1.0);
			hitpixel = pix.xy / pix.w;

			return true;
		}
	}

	/* No hit */
	return false;
}
#endif

float ssr_contribution(vec3 ray_origin, float hitstep, bool hit, inout vec3 hitco)
{
	/* ssr_parameters */
	float maxstep = 	unfssrparam.w; /* Maximum number of iteration when raymarching */
	float maxdistance = unfssrparam2.x; /* Maximum distance from ray origin */
	float attenuation = unfssrparam2.y; /* Attenuation factor for screen edges and ray step fading */

	/* ray step fade */
	float stepfade = saturate((1.0 - hitstep / maxstep) * attenuation);

	/* ray length fade */
	float rayfade = saturate((1.0 - length(ray_origin - hitco) / maxdistance) * attenuation);

	/* screen edges fade */
	vec4 co = gl_ProjectionMatrix * vec4(hitco, 1.0);
	co.xy /= co.w;
	hitco.xy = co.xy * 0.5 + 0.5;
	float maxdimension = saturate(max(abs(co.x), abs(co.y)));
	float screenfade = saturate((0.999999 - maxdimension) * attenuation);

	return sqrt(rayfade * screenfade) * float(hit);
}
