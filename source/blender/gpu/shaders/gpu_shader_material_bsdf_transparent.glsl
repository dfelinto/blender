/* -------- Utils Functions --------- */

vec3 sample_transparent()
{
#ifndef PLANAR_PROBE
	vec4 sample = textureCube(unfprobe, I);
#else
	vec4 sample = texture2D(unfrefract, refpos.xy);
#endif
	srgb_to_linearrgb(sample, sample);
	return sample.rgb;
}

/* -------- BSDF --------- */

/* -------- Preview Lights --------- */

/* -------- Physical Lights --------- */

/* -------- Image Based Lighting --------- */

void env_sampling_transparent(
      float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
      vec3 N, vec3 T, float roughness, float ior, float sigma,
      float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
      out vec3 result)
{
	vector_prepass(viewpos, N, invviewmat, viewmat);
	result = sample_transparent();
}
