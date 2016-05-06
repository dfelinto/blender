// color buffer
uniform sampler2D colorbuffer;
uniform sampler3D lut3d_texture;

uniform float exposure;
uniform float gamma;

uniform float lut_size = 16.0;

uniform float displayspace_is_srgb;
uniform float is_offscreen;

// coordinates on framebuffer in normalized (0.0-1.0) uv space
varying vec4 uvcoordsvar;

float srgb_to_linearrgb(float c)
{
	if(c < 0.04045)
		return (c < 0.0) ? 0.0: c * (1.0 / 12.92);
	else
		return pow((c + 0.055)*(1.0/1.055), 2.4);
}

float linearrgb_to_srgb(float c)
{
	if(c < 0.0031308)
		return (c < 0.0) ? 0.0: c * 12.92;
	else
		return 1.055 * pow(c, 1.0/2.4) - 0.055;
}

void srgb_to_linearrgb(vec4 col_from, out vec4 col_to)
{
	col_to.r = srgb_to_linearrgb(col_from.r);
	col_to.g = srgb_to_linearrgb(col_from.g);
	col_to.b = srgb_to_linearrgb(col_from.b);
	col_to.a = col_from.a;
}

void linearrgb_to_srgb(vec4 col_from, out vec4 col_to)
{
	col_to.r = linearrgb_to_srgb(col_from.r);
	col_to.g = linearrgb_to_srgb(col_from.g);
	col_to.b = linearrgb_to_srgb(col_from.b);
	col_to.a = col_from.a;
}

float computeLuminance(vec3 color)
{
	return max(dot(vec3(0.30, 0.59, 0.11), color), 1e-16);
}

vec3 computeExposedColor(vec3 color, float exposure)
{
	return exp2(exposure) * color;
}

// Reinhard operator
vec3 tonemapReinhard(vec3 color, float saturation)
{
	float lum = computeLuminance(color);
	float toneMappedLuminance = lum / (lum + 1.0);
	return toneMappedLuminance * (color / lum);
}

vec3 applyGamma(vec3 color, float gamma)
{
	gamma = max(gamma, 1e-8);
	return vec3( pow(color.r, 1/gamma), pow(color.g, 1/gamma), pow(color.b, 1/gamma) );
}

void main()
{
	//float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;
	vec4 scene_col = texture2D(colorbuffer, uvcoordsvar.xy);

	if (is_offscreen == 1.0) {
		// early out for Ofs. Color management is done later by OCIO
		gl_FragColor = scene_col;
		return;
	}

	if (displayspace_is_srgb == 1.0) 
		srgb_to_linearrgb(scene_col, scene_col);

	/* Esposure */
	scene_col = vec4( computeExposedColor(scene_col.rgb, exposure), scene_col.a);

	/* LUT 3D in linear space */
	/* TODO find a way to apply it in gamma space to gain accuracy in the black areas */
	//linearrgb_to_srgb(scene_col, scene_col);
	scene_col = vec4( texture3D(lut3d_texture, scene_col.rgb * ((lut_size + 1.0f) / lut_size) + (0.5f / lut_size) ).rgb, scene_col.a);
	//srgb_to_linearrgb(scene_col, scene_col);

	/* Gamma */
	scene_col = vec4( applyGamma(scene_col.rgb, gamma), scene_col.a);

	gl_FragColor = vec4(scene_col.rgb, scene_col.a);
}
