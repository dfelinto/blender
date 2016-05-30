/* -------- Utils Functions --------- */
vec2 sample_disk(float nsample, float invsamplenbr)
{
	vec3 Xi = hammersley_3d(nsample, invsamplenbr);

	float x = Xi.x * Xi.y;
	float y = Xi.x * Xi.z;

	return vec2(x, y);
}

vec3 sample_hemisphere(float nsample, float invsamplenbr, vec3 N, vec3 T, vec3 B)
{
	vec3 Xi = hammersley_3d(nsample, invsamplenbr);

	float z = Xi.x; /* cos theta */
	float r = sqrt( 1.0f - z*z ); /* sin theta */
	float x = r * Xi.y;
	float y = r * Xi.z;

	Ht = vec3(x, y, z); /* Global variable */

	return from_tangent_to_world(Ht, N, T, B);
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

#ifdef USE_SSAO

#if 0 /* Cheap */

float ssao(vec3 viewpos, vec3 viewnor, out float result)
{
	float dist = unfssaoparam.z;
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	/* get uv of the shading point */
	vec4 projvec = gl_ProjectionMatrix * vec4(viewpos, 1.0);
	vec2 uv = (projvec.xy / projvec.w) * 0.5 + 0.5;

	vec2 offset;
	offset.x = gl_ProjectionMatrix[0][0] * dist / projvec.w;
	offset.y = gl_ProjectionMatrix[1][1] * dist / projvec.w;
	/* convert from -1.0...1.0 range to 0.0..1.0 for easy use with texture coordinates */
	offset *= 0.5;

	float factor = 0.0;
	/* We don't need as much samples for ssao */
	for (float i = 0; i < unfssaoparam.x; i++) {
		vec2 Xi = sample_disk(i, 1/unfssaoparam.x);
		vec2 uvsample = uv + Xi * offset;

		if (uvsample.x > 1.0 || uvsample.x < 0.0 || uvsample.y > 1.0 || uvsample.y < 0.0)
			continue;

		float sampledepth = texture2D(unfscenebuf, uvsample).a;

		/* Background Case */
		if (sampledepth == 1.0)
			continue;

		vec3 samplepos = position_from_depth(uvsample, sampledepth);

		vec3 dir = samplepos - viewpos;
		float len = length(dir);
		float f = dot(dir, viewnor);

		/* use minor bias here to cancel self shadowing */
		if (f > 0.05 * len + 0.0001)
			factor += f / (len + len * len * len);
	}

	result = saturate(1.0 - factor / unfssaoparam.x);
}

#else /* Expensive */

void ssao(vec3 viewpos, vec3 viewnor, out float result)
{
	float dist = unfssaoparam.z;
	setup_noise(gl_FragCoord.xy); /* Noise to dither the samples */

	vec3 T;
	make_orthonormals(viewnor, T, B); /* Generate tangent space */

	float homcoord = (gl_ProjectionMatrix[3][3] == 0.0) ? gl_ProjectionMatrix[2][3] * viewpos.z : dist;

	float factor = 0.0;
	float weight = 0.0;

	for (float i = 0; i < unfssaoparam.x; i++) {
		vec3 Xi = sample_hemisphere(i, 1/unfssaoparam.x, viewnor, T, B);

		float pdf = dot(Xi, viewnor);
		weight += pdf;

		/* Raymarch */
		/* We jitter the starting position by a percentage of the normal 
		 * offset to a v o i d banding artifact for thin objects but
		 * we still finish at the same final position to a v o i d
		 * less sampling at the edges of the occlusion effect */
		vec3 offset = dist * Xi / unfssaoparam.y;
		vec3 ray = (jitternoise.y - 0.5) * -offset;
		vec3 endoffset = offset - ray;

		for (float j = unfssaoparam.y; j > 0.0; j--) {

			ray += (j == 1) ? endoffset : offset;

			vec4 co = unfpixelprojmat * vec4(ray + viewpos, 1.0);
			co.xy /= co.w;

			/* Discard ray leaving screen */
			if (co.x > unfclip.z || co.x < 0.0 || co.y > unfclip.w || co.y < 0.0)
				break;

			float sampledepth = frontface_depth(ivec2(co.xy));

			/* Background Case */
			if (sampledepth == 1.0)
				continue;

			/* We have a hit */
			if (sampledepth > ray.z + viewpos.z + homcoord * 0.002
#ifdef USE_BACKFACE
			 && backface_depth(ivec2(co.xy)) < ray.z + viewpos.z
#endif
			 )
			{
				factor += pdf;
				break;
			}
		}
	}

	result = 1.0 - (factor / weight);
}
#endif
#else
void ssao(vec3 viewpos, vec3 viewnor, out float result)
{
	result = 1.0;
}
#endif


/* -------- BSDF --------- */

/* -------- Preview Lights --------- */

/* -------- Physical Lights --------- */

/* -------- Image Based Lighting --------- */

void env_sampling_ambient_occlusion(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	float ao_factor, out vec3 result)
{
	result = vec3(ao_factor);
}