#define MAX_SSR_STEPS 128
#define MAX_SSR_REFINE_STEPS 16

#if 0 /* 2D raymarch (More efficient) */

/* By Morgan McGuire and Michael Mara at Williams College 2014
 * Released as open source under the BSD 2-Clause License
 * http://opensource.org/licenses/BSD-2-Clause
 * http://casual-effects.blogspot.fr/2014/08/screen-space-ray-tracing.html */
 
float distanceSquared(vec2 a, vec2 b) { a -= b; return dot(a, a); }

void swapIfBigger(inout float a, inout float b) { 
	if (a > b) {
		float temp = a; 
		a = b; 
		b = temp;
	}
}

/* 2D raycast : still have some bug that needs to be sorted out before being usable */
bool raycast(vec3 ray_origin, vec3 ray_dir, mat4 projpixel, sampler2D ssr_buffer, float thickness,
             float nearz, float stride, float jitter, float maxstep, float ray_length, 
             out float hitstep, out vec2 hitpixel, out vec3 hitco)
{
	/* Clip ray to a near plane in 3D */
	if ((ray_origin.z + ray_dir.z * ray_length) > nearz)
		ray_length = (nearz - ray_origin.z) / ray_dir.z;

	vec3 ray_end = ray_dir * ray_length + ray_origin;

	/* Project into screen space */
	vec4 H0 = projpixel * vec4(ray_origin, 1.0);
	vec4 H1 = projpixel * vec4(ray_end, 1.0);

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
	 * to avoid handling zero-pixel extent as a special case later */
	P1 += vec2((distanceSquared(P0, P1) < 0.0001) ? 0.01 : 0.0);

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

		scene_zmax = texelFetch(ssr_buffer, ivec2(hitpixel), 0).a;

		/* background case */
		if (scene_zmax == 1.0) scene_zmax = -1e16;

		hit = ((zmax >= scene_zmax - thickness) && (zmin <= scene_zmax));
	}

	hitco = (Q0 + dQ * hitstep) / k;
	return hit;
}

#else /* 3D raymarch */

float getDeltaDepth(sampler2D ssr_buffer, mat4 projpixel, vec3 hitco) {	
	vec4 co = projpixel * vec4(hitco, 1.0);
	co.xy /= co.w;
	float depth = texelFetch(ssr_buffer, ivec2(co.xy), 0).a;

	/* background case */
	if (depth == 1.0) depth = -1e16;

	return depth - hitco.z;
}

void binary_search(vec3 ray_dir, sampler2D ssr_buffer, mat4 projpixel, inout vec3 hitco) {	
    for (int i = 0; i < MAX_SSR_REFINE_STEPS; i++) {
		ray_dir *= 0.5;
        hitco -= ray_dir;
        float delta = getDeltaDepth(ssr_buffer, projpixel, hitco);
        if (delta < 0.0) hitco += ray_dir;
    }
}

/* 3D raycast */
bool raycast(vec3 ray_origin, vec3 ray_dir, mat4 projpixel, sampler2D ssr_buffer, float thickness,
             float nearz, float ray_step, float jitter, float maxstep, float ray_length, 
             out float hitstep, out vec2 hitpixel, out vec3 hitco)
{
	ray_dir *= ray_step;

	//thickness = 2 * ray_step;

	hitco = ray_origin;
	hitco += ray_dir * jitter;
	hitpixel = vec2(0.0);
	
    for (hitstep = 0.0; hitstep < MAX_SSR_STEPS && hitstep < maxstep; hitstep++) {
        hitco += ray_dir;
        float delta = getDeltaDepth(ssr_buffer, projpixel, hitco);
        if (delta > 0.0 && delta < thickness) {
        	/* Refine hit */
        	binary_search(ray_dir, ssr_buffer, projpixel, hitco);
        	/* Find texel coord */
    	    vec4 pix = projpixel * vec4(hitco, 1.0);
			hitpixel = pix.xy / pix.w;

        	return true;
    	}
    }

    /* No hit */
    return false;
}
#endif

float saturate(float a) { return clamp(a, 0.0, 1.0); }

float ssr_contribution(vec3 ray_origin, float hitstep, float maxstep, float maxdistance, float attenuation, bool hit, inout vec3 hitco)
{
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
