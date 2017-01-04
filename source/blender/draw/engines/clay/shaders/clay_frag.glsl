uniform vec4 color;
uniform ivec2 screenres;
uniform sampler2D depthtex;
uniform sampler2DArray matcaps;
uniform mat4 WinMatrix;

/* store the view space vectors for the corners of the view frustum here.
 * It helps to quickly reconstruct view space vectors by using uv coordinates,
 * see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
uniform vec4 viewvecs[3];
uniform vec4 ssao_params;
uniform vec3 ssao_sample_params;

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
	vec3 normal = cross(normalize(dFdx(viewposition)), ssao_params.w * normalize(dFdy(viewposition)));
	normalize(normal);
	return normal;
}

void main() {
	vec2 uvs = vec2(gl_FragCoord.xy) / vec2(screenres);
	float depth = texture(depthtex, uvs).r;

	/* Manual Depth test */
	if (gl_FragCoord.z > depth + 1e-5)
		discard;

	vec3 position = get_view_space_from_depth(uvs, viewvecs[0].xyz, viewvecs[1].xyz, depth);
	vec3 normal = calculate_view_space_normal(position);
	// vec4 col = texture(matcaps, vec3(uvs, step(0.5, uvs.x)));
	vec4 col = texture(matcaps, vec3(abs(normal.xy * .5 + 0.5), step(0.5, uvs.x)));
	fragColor = col;
}