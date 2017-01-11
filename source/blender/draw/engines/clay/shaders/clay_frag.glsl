uniform ivec2 screenres;
uniform sampler2D depthtex;
uniform mat4 WinMatrix;

/* Matcap */
uniform sampler2DArray matcaps;
uniform vec3 matcaps_color[24];
uniform int matcap_index;
uniform vec2 matcap_rotation;
uniform vec3 matcap_hsv;

/* Screen Space Occlusion */
/* store the view space vectors for the corners of the view frustum here.
 * It helps to quickly reconstruct view space vectors by using uv coordinates,
 * see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
uniform vec4 viewvecs[3];
uniform vec4 ssao_params_var;
uniform vec4 ssao_params;

uniform sampler2D ssao_jitter;
uniform sampler1D ssao_samples;

/* Aliases */
#define ssao_distance		ssao_params_var.x
#define ssao_factor_cavity	ssao_params_var.y
#define ssao_factor_edge	ssao_params_var.z
#define ssao_attenuation	ssao_params_var.w
#define ssao_samples_num	ssao_params.x
#define jitter_tilling		ssao_params.yz
#define dfdy_sign			ssao_params.w

#if __VERSION__ == 120
varying vec3 normal;
#define fragColor gl_FragColor
#else
in vec3 normal;
out vec4 fragColor;
#endif

#ifdef USE_HSV
void rgb_to_hsv(vec3 rgb, out vec3 outcol)
{
	float cmax, cmin, h, s, v, cdelta;
	vec3 c;

	cmax = max(rgb[0], max(rgb[1], rgb[2]));
	cmin = min(rgb[0], min(rgb[1], rgb[2]));
	cdelta = cmax - cmin;

	v = cmax;
	if (cmax != 0.0)
		s = cdelta / cmax;
	else {
		s = 0.0;
		h = 0.0;
	}

	if (s == 0.0) {
		h = 0.0;
	}
	else {
		c = (vec3(cmax, cmax, cmax) - rgb.xyz) / cdelta;

		if (rgb.x == cmax) h = c[2] - c[1];
		else if (rgb.y == cmax) h = 2.0 + c[0] -  c[2];
		else h = 4.0 + c[1] - c[0];

		h /= 6.0;

		if (h < 0.0)
			h += 1.0;
	}

	outcol = vec3(h, s, v);
}

void hsv_to_rgb(vec3 hsv, out vec3 outcol)
{
	float i, f, p, q, t, h, s, v;
	vec3 rgb;

	h = hsv[0];
	s = hsv[1];
	v = hsv[2];

	if (s == 0.0) {
		rgb = vec3(v, v, v);
	}
	else {
		if (h == 1.0)
			h = 0.0;

		h *= 6.0;
		i = floor(h);
		f = h - i;
		rgb = vec3(f, f, f);
		p = v * (1.0 - s);
		q = v * (1.0 - (s * f));
		t = v * (1.0 - (s * (1.0 - f)));

		if (i == 0.0) rgb = vec3(v, t, p);
		else if (i == 1.0) rgb = vec3(q, v, p);
		else if (i == 2.0) rgb = vec3(p, v, t);
		else if (i == 3.0) rgb = vec3(p, q, v);
		else if (i == 4.0) rgb = vec3(t, p, v);
		else rgb = vec3(v, p, q);
	}

	outcol = rgb;
}

void hue_sat(float hue, float sat, float value, inout vec3 col)
{
	vec3 hsv;

	rgb_to_hsv(col, hsv);

	hsv.x += hue;
	hsv.x -= floor(hsv.x);
	hsv.y *= sat;
	hsv.y = clamp(hsv.y, 0.0, 1.0);
	hsv.z *= value;
	hsv.z = clamp(hsv.z, 0.0, 1.0);

	hsv_to_rgb(hsv, col);
}
#endif

/* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer
 * we change the factors from the article to fit the OpenGL model.  */

/* perspective camera code */

vec3 get_view_space_from_depth(in vec2 uvcoords, in vec3 viewvec_origin, in vec3 viewvec_diff, in float depth)
{
	float d = 2.0 * depth - 1.0;

	float zview = -WinMatrix[3][2] / (d + WinMatrix[2][2]);

	return zview * (viewvec_origin + vec3(uvcoords, 0.0) * viewvec_diff);
}

/* TODO remove this when switching to geometric normals */
vec3 calculate_view_space_normal(in vec3 viewposition)
{
	vec3 normal = cross(normalize(dFdx(viewposition)), dfdy_sign * normalize(dFdy(viewposition)));

	return normalize(normal);
}

#ifdef USE_AO
void calculate_ssao_factor(in float depth, in vec3 normal, in vec3 position, out float cavities, out float edges)
{
	vec2 uvs = vec2(gl_FragCoord.xy) / vec2(screenres);

	/* take the normalized ray direction here */
	vec2 rotX = texture2D(ssao_jitter, uvs.xy * jitter_tilling).rg;
	vec2 rotY = vec2(-rotX.y, rotX.x);

	/* find the offset in screen space by multiplying a point
	 * in camera space at the depth of the point by the projection matrix. */
	vec2 offset;
	float homcoord = WinMatrix[2][3] * position.z + WinMatrix[3][3];
	offset.x = WinMatrix[0][0] * ssao_distance / homcoord;
	offset.y = WinMatrix[1][1] * ssao_distance / homcoord;
	/* convert from -1.0...1.0 range to 0.0..1.0 for easy use with texture coordinates */
	offset *= 0.5;

	cavities = edges = 0.0;
	int x;
	int num_samples = int(ssao_samples_num);

	for (x = 0; x < num_samples; x++) {
		vec2 dir_sample = texture1D(ssao_samples, (float(x) + 0.5) / ssao_samples_num).rg;

		/* rotate with random direction to get jittered result */
		vec2 dir_jittered = vec2(dot(dir_sample, rotX), dot(dir_sample, rotY));

		vec2 uvcoords = uvs.xy + dir_jittered * offset;

		if (uvcoords.x > 1.0 || uvcoords.x < 0.0 || uvcoords.y > 1.0 || uvcoords.y < 0.0)
			continue;

		float depth_new = texture2D(depthtex, uvcoords).r;
		/* Handle Background case */
		if (depth_new != 1.0) {
			vec3 pos_new = get_view_space_from_depth(uvcoords, viewvecs[0].xyz, viewvecs[1].xyz, depth_new);
			vec3 dir = pos_new - position;
			float len = length(dir);
			float f_cavities = dot(dir, normal);
			float f_edge = dot(dir, -normal);
			float f_bias = 0.05 * len + 0.0001;

			float attenuation = 1.0 / (len * (1.0 + len * len * ssao_attenuation));

			/* use minor bias here to avoid self shadowing */
			if (f_cavities > f_bias)
				cavities += f_cavities * attenuation;

			if (f_edge > f_bias)
				edges += f_edge * attenuation;
		}
	}

	cavities /= ssao_samples_num;
	edges /= ssao_samples_num;

	/* don't let cavity wash out the surface appearance */
	cavities = clamp(cavities * ssao_factor_cavity, 0.0, 0.85);
	edges = edges * ssao_factor_edge;
}
#endif

void main() {
	vec2 uvs = vec2(gl_FragCoord.xy) / vec2(screenres);
	float depth = texture(depthtex, uvs).r;

	vec3 position = get_view_space_from_depth(uvs, viewvecs[0].xyz, viewvecs[1].xyz, depth);
	vec3 normal = calculate_view_space_normal(position);

	/* Manual Depth test */
	/* Doing this test earlier gives problem with dfdx calculations
	 * TODO move this before when we have proper geometric normals */
	if (gl_FragCoord.z > depth + 1e-5)
		discard;

#ifdef USE_ROTATION
	/* Rotate texture coordinates */
	vec2 rotY = vec2(-matcap_rotation.y, matcap_rotation.x);
	vec2 texco = abs(vec2(dot(normal.xy, matcap_rotation), dot(normal.xy, rotY)) * .49 + 0.5);
#else
	vec2 texco = abs(normal.xy * .49 + 0.5);
#endif
	vec3 col = texture(matcaps, vec3(texco, float(matcap_index))).rgb;

#ifdef USE_AO
	float cavity, edges;
	calculate_ssao_factor(depth, normal, position, cavity, edges);

	col = mix(col, matcaps_color[int(matcap_index)], cavity);
	col *= edges + 1.0;
#endif

#ifdef USE_HSV
	hue_sat(matcap_hsv.x, matcap_hsv.y, matcap_hsv.z, col);
#endif

#ifdef USE_AO
	/* Apply highlights after hue shift */
	col *= edges + 1.0;
#endif

	fragColor = vec4(col, 1.0);
}