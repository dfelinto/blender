/* From http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/ */
#extension GL_EXT_gpu_shader4 : enable

uniform sampler2D lowermip;
uniform ivec2 lowermipsize;

float minmax(float a, float b, float c, float d)
{
#ifdef MIN
	return min(min(a, b), min(c, d));
#else
	return max(max(a, b), max(c, d));
#endif
}

float minmax(float a, float b, float c)
{
#ifdef MIN
	return min(min(a, b), c);
#else
	return max(max(a, b), c);
#endif
}

float minmax(float a, float b)
{
#ifdef MIN
	return min(a, b);
#else
	return max(a, b);
#endif
}

void main()
{
	vec4 texels;
	ivec2 texelPos = ivec2(gl_FragCoord.xy) * 2;
	texels.x = texelFetch(lowermip, texelPos, 0).r;
	texels.y = texelFetch(lowermip, texelPos + ivec2(1, 0), 0).r;
	texels.z = texelFetch(lowermip, texelPos + ivec2(1, 1), 0).r;
	texels.w = texelFetch(lowermip, texelPos + ivec2(0, 1), 0).r;

	float minmaxz = minmax(texels.x, texels.y, texels.z, texels.w);
	vec3 extra;
	/* if we are reducing an odd-width texture then fetch the edge texels */
	if (((lowermipsize.x & 1) != 0) && (int(gl_FragCoord.x) == lowermipsize.x-3)) {
		/* if both edges are odd, fetch the top-left corner texel */
		if (((lowermipsize.y & 1) != 0) && (int(gl_FragCoord.y) == lowermipsize.y-3)) {
		  extra.z = texelFetch(lowermip, texelPos + ivec2(-1, -1), 0).r;
		  minmaxz = minmax(minmaxz, extra.z);
		}
		extra.x = texelFetch(lowermip, texelPos + ivec2(0, -1), 0).r;
		extra.y = texelFetch(lowermip, texelPos + ivec2(1, -1), 0).r;
		minmaxz = minmax(minmaxz, extra.x, extra.y);
	}
	/* if we are reducing an odd-height texture then fetch the edge texels */
	else if (((lowermipsize.y & 1) != 0) && (int(gl_FragCoord.y) == lowermipsize.y-3)) {
		extra.x = texelFetch(lowermip, texelPos + ivec2(0, -1), 0).r;
		extra.y = texelFetch(lowermip, texelPos + ivec2(1, -1), 0).r;
		minmaxz = minmax(minmaxz, extra.x, extra.y);
	}

	gl_FragDepth = minmaxz;
	gl_FragColor = vec4(1.0);
}
