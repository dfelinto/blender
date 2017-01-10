uniform ivec2 screenres;
uniform sampler2D depthtex;
uniform mat4 WinMatrix;

/* Matcap */
uniform sampler2DArray matcaps;
uniform int matcap_index;
uniform vec2 matcap_rotation;
uniform float matcap_hue;

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

/* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer
 * we change the factors from the article to fit the OpenGL model.  */

/* perspective camera code */

vec3 get_view_space_from_depth(in vec2 uvcoords, in vec3 viewvec_origin, in vec3 viewvec_diff, in float depth)
{
	float d = 2.0 * depth - 1.0;

	float zview = -WinMatrix[3][2] / (d + WinMatrix[2][2]);

	return zview * (viewvec_origin + vec3(uvcoords, 0.0) * viewvec_diff);
}

vec3 calculate_view_space_normal(in vec3 viewposition)
{
	vec3 normal = cross(normalize(dFdx(viewposition)), dfdy_sign * normalize(dFdy(viewposition)));

	return normalize(normal);
}

float calculate_ssao_factor(float depth, vec3 normal, vec3 position)
{
	vec2 uvs = vec2(gl_FragCoord.xy) / vec2(screenres);

	/* take the normalized ray direction here */
	vec2 rotX = texture2D(ssao_jitter, uvs.xy * jitter_tilling).rg;
	vec2 rotY = vec2(-rotX.y, rotX.x);

	/* occlusion is zero in full depth */
	if (depth == 1.0)
		return 1.0;

	/* find the offset in screen space by multiplying a point
	 * in camera space at the depth of the point by the projection matrix. */
	vec2 offset;
	float homcoord = WinMatrix[2][3] * position.z + WinMatrix[3][3];
	offset.x = WinMatrix[0][0] * ssao_distance / homcoord;
	offset.y = WinMatrix[1][1] * ssao_distance / homcoord;
	/* convert from -1.0...1.0 range to 0.0..1.0 for easy use with texture coordinates */
	offset *= 0.5;

	float obscurance_cavity = 0.0;
	float obscurance_edge = 0.0;
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
			float f_cavity = dot(dir, normal);
			float f_edge = dot(dir, -normal);
			float f_bias = 0.05 * len + 0.0001;

			float attenuation = 1.0 / (len * (1.0 + len * len * ssao_attenuation));

			/* use minor bias here to avoid self shadowing */
			if (f_cavity > f_bias)
				obscurance_cavity += f_cavity * attenuation;

			if (f_edge > f_bias)
				obscurance_edge += f_edge * attenuation;
		}
	}

	obscurance_cavity /= ssao_samples_num;
	obscurance_edge /= ssao_samples_num;

	obscurance_cavity = clamp(obscurance_cavity * ssao_factor_cavity, 0.0, 1.0);
	obscurance_edge = clamp(obscurance_edge * ssao_factor_edge, 0.0, 1.0);

	return 1.0 - obscurance_cavity + obscurance_edge;
}


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

	vec2 texco = abs(normal.xy * .49 + 0.5);
#ifdef USE_ROTATION
	/* Rotate texture coordinates */
	vec2 rotY = vec2(-matcap_roation.y, matcap_roation.x);
	texco = vec2(dot(texco, matcap_roation), dot(texco, rotY));
#endif
	vec4 col = texture(matcaps, vec3(texco, float(matcap_index)));

#ifdef USE_AO
	col *= calculate_ssao_factor(depth, normal, position);
#endif

	fragColor = vec4(col.rgb, 1.0);
}