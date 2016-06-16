/* ------- Defines -------- */

/* Number of default opengl lights */
#define NUM_LIGHTS 3

/* SSR Maximum iterations */
#define MAX_SSR_REFINE_STEPS 8

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
uniform sampler2D unfscenebuf;
uniform sampler2D unfdepthbuf;
uniform sampler2D unfbackfacebuf;
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
uniform vec3 unfssrparam;
uniform vec4 unfssaoparam;
uniform vec4 unfclip;
uniform mat4 unfprobecorrectionmat;
uniform mat4 unfplanarreflectmat;
uniform mat4 unfpixelprojmat;

/* ------- Global Variables -------- */

vec3 worldpos, refpos;
vec3 viewnor, viewi;
vec3 planarfac, planarvec;
vec3 I, B, Ht;
vec2 jitternoise = vec2(0.0);

/* ------- Convenience functions --------- */

vec3 mul(mat3 m, vec3 v) { return m * v; }
mat3 mul(mat3 m1, mat3 m2) { return m1 * m2; }

float saturate(float a) { return clamp(a, 0.0, 1.0); }

float distance_squared(vec2 a, vec2 b) { a -= b; return dot(a, a); }
float distance_squared(vec3 a, vec3 b) { a -= b; return dot(a, a); }

float hypot(float x, float y) { return sqrt(x*x + y*y); }

float inverse_distance(vec3 V) { return max( 1 / length(V), 1e-8); }

vec4 bufferFetch(sampler2D buffer, ivec2 texelpos, int lod)
{
#if __VERSION__ < 130
	return texture2DLod(buffer, (vec2(texelpos) + 0.5) / (unfclip.zw / pow(2.0, float(lod))), float(lod));
#else
	return texelFetch(buffer, texelpos, lod);
#endif
}

/* --------- Noise Utils Functions --------- */

void generated_from_orco(vec3 orco, out vec3 generated)
{
	generated = orco * 0.5 + vec3(0.5);
}

int floor_to_int(float x)
{
	return int(floor(x));
}

int quick_floor(float x)
{
	return int(x) - ((x < 0) ? 1 : 0);
}

#ifdef BIT_OPERATIONS
float integer_noise(int n)
{
	int nn;
	n = (n + 1013) & 0x7fffffff;
	n = (n >> 13) ^ n;
	nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 0.5 * (float(nn) / 1073741824.0);
}

uint hash(uint kx, uint ky, uint kz)
{
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
#define final(a, b, c) \
{ \
	c ^= b; c -= rot(b, 14); \
	a ^= c; a -= rot(c, 11); \
	b ^= a; b -= rot(a, 25); \
	c ^= b; c -= rot(b, 16); \
	a ^= c; a -= rot(c, 4);  \
	b ^= a; b -= rot(a, 14); \
	c ^= b; c -= rot(b, 24); \
}
	// now hash the data!
	uint a, b, c, len = 3u;
	a = b = c = 0xdeadbeefu + (len << 2u) + 13u;

	c += kz;
	b += ky;
	a += kx;
	final (a, b, c);

	return c;
#undef rot
#undef final
}

uint hash(int kx, int ky, int kz)
{
	return hash(uint(kx), uint(ky), uint(kz));
}

float bits_to_01(uint bits)
{
	float x = float(bits) * (1.0 / float(0xffffffffu));
	return x;
}

float cellnoise(vec3 p)
{
	int ix = quick_floor(p.x);
	int iy = quick_floor(p.y);
	int iz = quick_floor(p.z);

	return bits_to_01(hash(uint(ix), uint(iy), uint(iz)));
}

vec3 cellnoise_color(vec3 p)
{
	float r = cellnoise(p);
	float g = cellnoise(vec3(p.y, p.x, p.z));
	float b = cellnoise(vec3(p.y, p.z, p.x));

	return vec3(r, g, b);
}
#endif  // BIT_OPERATIONS

float floorfrac(float x, out int i)
{
	i = floor_to_int(x);
	return x - i;
}

/* --------- Geometric Utils Functions --------- */

float linear_depth(float z)
{
	if (gl_ProjectionMatrix[3][3] == 0.0) {
		float zn = unfclip.x; // camera z near
		float zf = unfclip.y; // camera z far
		return (zn  * zf) / (z * (zn - zf) + zf);
	}
	else {
		float zf = unfclip.y; // camera z far
		return z * 2.0 * zf - zf;
	}
}

float backface_depth(ivec2 texelpos, int lod)
{
	float depth = linear_depth(bufferFetch(unfbackfacebuf, texelpos, lod).r);

	/* background case */
	if (depth == 1.0)
		return -1e16;
	else
		return -depth;
}

float frontface_depth(ivec2 texelpos, int lod)
{
	float depth = linear_depth(bufferFetch(unfdepthbuf, texelpos, lod).r);

	/* background case */
	if (depth == 1.0)
		return -1e16;
	else
		return -depth;
}

vec3 position_from_depth(vec2 uv, float depth)
{
	vec3 pos;
	float homcoord = gl_ProjectionMatrix[2][3] * depth + gl_ProjectionMatrix[3][3];
	pos.x = gl_ProjectionMatrixInverse[0][0] * (uv.x * 2.0 - 1.0) * homcoord;
	pos.y = gl_ProjectionMatrixInverse[1][1] * (uv.y * 2.0 - 1.0) * homcoord;
	pos.z = depth;
	return pos;
}

vec2 uv_from_position(vec3 position)
{
	vec4 projvec = gl_ProjectionMatrix * vec4(position, 1.0);
	return (projvec.xy / projvec.w) * 0.5 + 0.5;
}

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

/* from cycles ray_aligned_disk_intersect */
float line_aligned_plane_intersect_dist(vec3 lineorigin, vec3 linedirection, vec3 planeorigin)
{
	/* aligned plane normal */
	vec3 L = planeorigin - lineorigin;
	float diskdist = length(L);
	vec3 planenormal = -normalize(L);
	return -diskdist / dot(planenormal, linedirection);
}

vec3 line_aligned_plane_intersect(vec3 lineorigin, vec3 linedirection, vec3 planeorigin)
{
	float dist = line_aligned_plane_intersect_dist(lineorigin, linedirection, planeorigin);
	if (dist < 0) {
		/* if intersection is behind we fake the intersection to be
		 * really far and (hopefully) not inside the radius of interest */
		dist = 1e16;
	}
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
		//float dist = line_aligned_plane_intersect_dist(vec3(0.0), R, L);
		float dist = dot(L, R);
		vec3 closest_point_on_ray = dist * R;
		vec3 center_to_ray = closest_point_on_ray - L;
		/* closest point on sphere */
		L = L + center_to_ray * saturate(l_radius * inverse_distance(center_to_ray));
	}

	L = normalize(L);
}

void most_representative_point_disk(float l_radius, float l_distance, vec3 R, inout vec3 L,
                                    inout float roughness, inout float energy_conservation)
{
	L = l_distance * L;

	/* Sphere Light */
	if(l_radius>0){
		roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

		/* energy preservation */
		float SphereAngle = saturate(l_radius / l_distance);
		energy_conservation *= pow(roughness / saturate(roughness + 0.5 * SphereAngle), 2.0);

		/* sphere light */
		vec3 closest_point_on_ray = line_plane_intersect(vec3(0.0), R, L, -normalize(L));
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
	/* detect clipping config */
	int config = 0;
	if (L[0].z > 0.0) config += 1;
	if (L[1].z > 0.0) config += 2;
	if (L[2].z > 0.0) config += 4;
	if (L[3].z > 0.0) config += 8;

	/* clip */
	int n = 0;

	if (config == 0)
	{
		/* clip all */
	}
	else if (config == 1) /* V1 clip V2 V3 V4 */
	{
		n = 3;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 2) /* V2 clip V1 V3 V4 */
	{
		n = 3;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 3) /* V1 V2 clip V3 V4 */
	{
		n = 4;
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
		L[3] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 4) /* V3 clip V1 V2 V4 */
	{
		n = 3;
		L[0] = -L[3].z * L[2] + L[2].z * L[3];
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
	}
	else if (config == 5) /* V1 V3 clip V2 V4) impossible */
	{
		n = 0;
	}
	else if (config == 6) /* V2 V3 clip V1 V4 */
	{
		n = 4;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 7) /* V1 V2 V3 clip V4 */
	{
		n = 5;
		L[4] = -L[3].z * L[0] + L[0].z * L[3];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 8) /* V4 clip V1 V2 V3 */
	{
		n = 3;
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
		L[1] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] =  L[3];
	}
	else if (config == 9) /* V1 V4 clip V2 V3 */
	{
		n = 4;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[2].z * L[3] + L[3].z * L[2];
	}
	else if (config == 10) /* V2 V4 clip V1 V3) impossible */
	{
		n = 0;
	}
	else if (config == 11) /* V1 V2 V4 clip V3 */
	{
		n = 5;
		L[4] = L[3];
		L[3] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 12) /* V3 V4 clip V1 V2 */
	{
		n = 4;
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
	}
	else if (config == 13) /* V1 V3 V4 clip V2 */
	{
		n = 5;
		L[4] = L[3];
		L[3] = L[2];
		L[2] = -L[1].z * L[2] + L[2].z * L[1];
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
	}
	else if (config == 14) /* V2 V3 V4 clip V1 */
	{
		n = 5;
		L[4] = -L[0].z * L[3] + L[3].z * L[0];
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
	}
	else if (config == 15) /* V1 V2 V3 V4 */
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

	/* scale and bias coordinates, for correct filtered lookup */
	return coords * (LTC_LUT_SIZE - 1.0) / LTC_LUT_SIZE + 0.5 / LTC_LUT_SIZE;
}

mat3 ltc_matrix(vec2 coord)
{
	/* load inverse matrix */
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
	/* construct orthonormal basis around N */
	vec3 T1, T2;
	T1 = normalize(V - N*dot(V, N));
	T2 = cross(N, T1);

	/* rotate area light in (T1, T2, R) basis */
	Minv = mul(Minv, transpose(mat3(T1, T2, N)));

	/* polygon (allocate 5 vertices for clipping) */
	vec3 L[5];
	L[0] = mul(Minv, points[0] - P);
	L[1] = mul(Minv, points[1] - P);
	L[2] = mul(Minv, points[2] - P);
	L[3] = mul(Minv, points[3] - P);

	int n = clip_quad_to_horizon(L);

	if (n == 0)
		return 0.0;

	/* project onto sphere */
	L[0] = normalize(L[0]);
	L[1] = normalize(L[1]);
	L[2] = normalize(L[2]);
	L[3] = normalize(L[3]);
	L[4] = normalize(L[4]);

	/* integrate */
	float sum = 0.0;

	sum += integrate_edge(L[0], L[1]);
	sum += integrate_edge(L[1], L[2]);
	sum += integrate_edge(L[2], L[3]);
	if (n >= 4)
		sum += integrate_edge(L[3], L[4]);
	if (n == 5)
		sum += integrate_edge(L[4], L[0]);

	return abs(sum);
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

/* ------- Ambient Occlusion ------- */
/* from Sebastien Lagarde
 * course_notes_moving_frostbite_to_pbr.pdf */

float specular_occlusion(float NV, float AO, float roughness)
{
#ifdef USE_SSAO
	return saturate(pow(NV + AO, roughness) - 1.0 + AO);
#else
	return 1.0;
#endif
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

vec2 get_distorded_refl_uv(sampler2D planarprobe, vec2 Xi)
{
	vec2 distortion = vec2(dot(viewnor, vec3(1.0, 0.0, 0.0)) - planarfac.x,
	                       dot(viewnor, vec3(0.0, 1.0, 0.0)) - planarfac.y);
	distortion += Xi;

	/* modulate intensity by distance to the viewer and by distance to the reflected object */
	float dist_view_to_shaded = planarfac.z;
	float dist_intersection_to_reflectioncam = texture2D(planarprobe, refpos.xy + Xi / dist_view_to_shaded).a;
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
	vec2 co = get_distorded_refl_uv(planarprobe, Ht.xy);
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
	vec2 co = get_distorded_refl_uv(planarprobe, vec2(0.0));
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

void setup_noise(vec2 fragcoord)
{
	const int NOISE_SIZE = 64;
	ivec2 texel = ivec2(mod(fragcoord.x, NOISE_SIZE), mod(fragcoord.y, NOISE_SIZE));
#if __VERSION__ < 130
	jitternoise = texture2DLod(unfjitter, (vec2(texel) + 0.5) / float(NOISE_SIZE), 0.0).rg; /* Global variable */
#else
	jitternoise = texelFetch(unfjitter, texel, 0).rg; /* Global variable */
#endif
}

vec3 hammersley_3d(float i, float invsamplenbr)
{
	vec3 Xi; /* Theta, cos(Phi), sin(Phi) */

	Xi.x = i * invsamplenbr; /* i/samples */
	Xi.x = fract(Xi.x + jitternoise.x);

	int u = int(mod(i + jitternoise.y * BSDF_SAMPLES, BSDF_SAMPLES));

#if __VERSION__ < 130
	Xi.yz = texture1DLod(unflutsamples, (float(u) + 0.5) / float(BSDF_SAMPLES), 0.0).rg; /* Global variable */
#else
	Xi.yz = texelFetch(unflutsamples, u, 0).rg; /* Global variable */
#endif
	return Xi;
}

vec3 hammersley_3d(float i)
{
	return hammersley_3d(i, unfbsdfsamples.y);
}

/* ------- Screen Space Raycasting ---------*/

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
bool raycast(vec3 ray_origin, vec3 ray_dir, out float hitstep, out vec2 hitpixel, out vec3 hitco)
{
	/* ssr_parameters */
	float nearz = -unfclip.x; /* Near plane distance (Negative number) */
	float farz = -unfclip.y; /* Far plane distance (Negative number) */

	/* Clip ray to a near plane in 3D */
	float ray_length = 1e16;
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

	/* Slide each value from the start of the ray to the end */
	vec4 pqk = vec4(P0, Q0.z, k0);

	/* Scale derivatives by the desired pixel stride */
	vec4 dPQK = vec4(dP, dQ.z, dk) * 1.0;

	bool hit = false;

#if 1 /* Linear 2D raymarching */

	/* We track the ray depth at +/- 1/2 pixel to treat pixels as clip-space solid
	 * voxels. Because the depth at -1/2 for a given pixel will be the same as at
	 * +1/2 for the previous iteration, we actually only have to compute one value
	 * per iteration. */
	float prev_zmax = ray_origin.z;
	float zmax, zmin;

	/* P1.x is never modified after this point, so pre-scale it by
	 * the step direction for a signed comparison */
	float end = P1.x * step_sign;

	for (hitstep = 0.0; hitstep < unfssrparam.x && !hit; hitstep++)
	{
		/* Ray finished & no hit*/
		if ((pqk.x * step_sign) > end) break;

		/* step through current cell */
		pqk += dPQK;

		hitpixel = permute ? pqk.yx : pqk.xy;
		zmin = prev_zmax;
		zmax = (dPQK.z * 0.5 + pqk.z) / (dPQK.w * 0.5 + pqk.w);
		prev_zmax = zmax;
		swapIfBigger(zmin, zmax);

		float frontface = frontface_depth(ivec2(hitpixel), 0);

		if (zmax < frontface) {
			/* Below surface */
#ifdef USE_BACKFACE
			float backface = backface_depth(ivec2(hitpixel), 0);
#else
			float backface = frontface - unfssrparam.z;
#endif
			hit = (zmin > backface);
		}
	}

	/* Hit coordinate in 3D */
	hitco = (Q0 + dQ * hitstep) / pqk.w;

#else /* Hierarchical raymarching */

	float z, mip, mippow, s;
	float level = 1.0;
	float steps = 0.0;
	float dir = 1.0;
	float lastdir = 1.0;
	for (hitstep = 0.0; hitstep < unfssrparam.x && level > 0.0; hitstep++) {
		mip = level - 1.0;
		mippow = pow(2, mip);

		/* step through current cell */
		//s = dir * max(1.0, mippow / 2);
		s = dir * mippow;
		//s = dir * level;
		P += dP * s; Q.z += dQ.z * s; k += dk * s; steps += s;

		hitpixel = permute ? P.yx : P;
		z = dQ.z / k;

		float frontface = frontface_depth(ivec2(hitpixel / mippow), int(mip));

		if (z < frontface) {
			/* Below surface */
#ifdef USE_BACKFACE
			float backface = backface_depth(ivec2(hitpixel), 0);
#else
			float backface = -1e16; /* Todo find a good thickness */
#endif
			if (z > backface) {
				/* Hit */
				/* This will step back the current step */
				//s = -1.0 * level;
				//P += dP * s; Q.z += dQ.z * s; k += dk * s; steps += s;
				dir = -1.0;
				level--;
				continue;
			}
		}

		/* Above surface */
		if (dir != -1.0 && lastdir != -1.0) {
			/* Only step up in mip if the last iteration was positive
			 * This way we don't skip potential occluders */
			level++;
			level = min(level, 5.0);
		}
		lastdir = dir;
		dir = 1.0;
	}

	hitstep = 1.0;

	hit = (level == 0.0);

	/* Hit coordinate in 3D */
	hitco = (Q0 + dQ * steps) / k;
#endif

	return hit;
}

float ssr_contribution(vec3 ray_origin, float hitstep, bool hit, inout vec3 hitco)
{
	/* ssr_parameters */
	float maxstep = 	unfssrparam.x; /* Maximum number of iteration when raymarching */
	float attenuation = unfssrparam.y; /* Attenuation factor for screen edges and ray step fading */

	/* ray step fade */
	float stepfade = saturate((1.0 - hitstep / maxstep) * attenuation);

	/* screen edges fade */
	vec4 co = gl_ProjectionMatrix * vec4(hitco, 1.0);
	co.xy /= co.w;
	hitco.xy = co.xy * 0.5 + 0.5;
	float maxdimension = saturate(max(abs(co.x), abs(co.y)));
	float screenfade = saturate((0.999999 - maxdimension) * attenuation);

	return smoothstep(0.0, 1.0, stepfade * screenfade) * float(hit);
}
