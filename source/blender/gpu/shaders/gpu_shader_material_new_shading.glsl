/*********** NEW SHADER NODES ***************/

/* Blending */
void node_bsdf_opaque(vec4 color, vec4 ambient_light, vec4 direct_light, out vec4 result)
{
	result = vec4( (ambient_light.rgb + direct_light.rgb) * color.rgb, 1.0);
}

void node_bsdf_transparent(vec4 color, vec4 background, out vec4 result)
{
	result = vec4( background.rgb * color.rgb, color.a);
}

/* Others Bsdfs */

void node_subsurface_scattering(vec4 color, float scale, vec3 radius, float sharpen, float texture_blur, vec3 N, out vec4 result)
{
	node_bsdf_diffuse_lights(color, 0.0, N, vec3(0.0), vec4(0.2), result);
}

void node_bsdf_hair(vec4 color, float offset, float roughnessu, float roughnessv, vec3 tangent, out vec4 result)
{
	result = color;
}

void node_ambient_occlusion(vec4 color, out vec4 result)
{
	result = color;
}

void node_holdout(out vec4 result)
{
	result = vec4(0.0);
}

/* emission */

void node_emission(vec4 color, float strength, vec3 N, out vec4 result)
{
	result = color*strength;
}

/* blackbody */

void node_blackbody(float T, out vec4 col)
{
	float u = ( 0.860117757 + 1.54118254e-4 * T + 1.28641212e-7 * T*T ) / ( 1.0 + 8.42420235e-4 * T + 7.08145163e-7 * T*T );
	float v = ( 0.317398726 + 4.22806245e-5 * T + 4.20481691e-8 * T*T ) / ( 1.0 - 2.89741816e-5 * T + 1.61456053e-7 * T*T );

	float x = 3*u / ( 2*u - 8*v + 4 );
	float y = 2*v / ( 2*u - 8*v + 4 );
	float z = 1 - x - y;

	float Y = 1;
	float X = Y/y * x;
	float Z = Y/y * z;

	mat3 XYZtoRGB = mat3(
		3.2404542, -0.9692660,  0.0556434,
		-1.5371385, 1.8760108, -0.2040259,
		-0.4985314,  0.0415560, 1.0572252
	);

	col = vec4(( XYZtoRGB * vec3( X, Y, Z ) ), 1.0);
}

vec3 xyz_to_rgb(float x, float y, float z)
{
	return vec3( 3.240479 * x + -1.537150 * y + -0.498535 * z,
	             -0.969256 * x +  1.875991 * y +  0.041556 * z,
	              0.055648 * x + -0.204043 * y +  1.057311 * z);
}

// CIE colour matching functions xBar, yBar, and zBar for
//	 wavelengths from 380 through 780 nanometers, every 5
//	 nanometers.  For a wavelength lambda in this range:
//		  cie_colour_match[(lambda - 380) / 5][0] = xBar
//		  cie_colour_match[(lambda - 380) / 5][1] = yBar
//		  cie_colour_match[(lambda - 380) / 5][2] = zBar
uniform vec3 node_wavelength_LUT[81] = vec3[81](
	vec3(0.0014,0.0000,0.0065), vec3(0.0022,0.0001,0.0105), vec3(0.0042,0.0001,0.0201),
	vec3(0.0076,0.0002,0.0362), vec3(0.0143,0.0004,0.0679), vec3(0.0232,0.0006,0.1102),
	vec3(0.0435,0.0012,0.2074), vec3(0.0776,0.0022,0.3713), vec3(0.1344,0.0040,0.6456),
	vec3(0.2148,0.0073,1.0391), vec3(0.2839,0.0116,1.3856), vec3(0.3285,0.0168,1.6230),
	vec3(0.3483,0.0230,1.7471), vec3(0.3481,0.0298,1.7826), vec3(0.3362,0.0380,1.7721),
	vec3(0.3187,0.0480,1.7441), vec3(0.2908,0.0600,1.6692), vec3(0.2511,0.0739,1.5281),
	vec3(0.1954,0.0910,1.2876), vec3(0.1421,0.1126,1.0419), vec3(0.0956,0.1390,0.8130),
	vec3(0.0580,0.1693,0.6162), vec3(0.0320,0.2080,0.4652), vec3(0.0147,0.2586,0.3533),
	vec3(0.0049,0.3230,0.2720), vec3(0.0024,0.4073,0.2123), vec3(0.0093,0.5030,0.1582),
	vec3(0.0291,0.6082,0.1117), vec3(0.0633,0.7100,0.0782), vec3(0.1096,0.7932,0.0573),
	vec3(0.1655,0.8620,0.0422), vec3(0.2257,0.9149,0.0298), vec3(0.2904,0.9540,0.0203),
	vec3(0.3597,0.9803,0.0134), vec3(0.4334,0.9950,0.0087), vec3(0.5121,1.0000,0.0057),
	vec3(0.5945,0.9950,0.0039), vec3(0.6784,0.9786,0.0027), vec3(0.7621,0.9520,0.0021),
	vec3(0.8425,0.9154,0.0018), vec3(0.9163,0.8700,0.0017), vec3(0.9786,0.8163,0.0014),
	vec3(1.0263,0.7570,0.0011), vec3(1.0567,0.6949,0.0010), vec3(1.0622,0.6310,0.0008),
	vec3(1.0456,0.5668,0.0006), vec3(1.0026,0.5030,0.0003), vec3(0.9384,0.4412,0.0002),
	vec3(0.8544,0.3810,0.0002), vec3(0.7514,0.3210,0.0001), vec3(0.6424,0.2650,0.0000),
	vec3(0.5419,0.2170,0.0000), vec3(0.4479,0.1750,0.0000), vec3(0.3608,0.1382,0.0000),
	vec3(0.2835,0.1070,0.0000), vec3(0.2187,0.0816,0.0000), vec3(0.1649,0.0610,0.0000),
	vec3(0.1212,0.0446,0.0000), vec3(0.0874,0.0320,0.0000), vec3(0.0636,0.0232,0.0000),
	vec3(0.0468,0.0170,0.0000), vec3(0.0329,0.0119,0.0000), vec3(0.0227,0.0082,0.0000),
	vec3(0.0158,0.0057,0.0000), vec3(0.0114,0.0041,0.0000), vec3(0.0081,0.0029,0.0000),
	vec3(0.0058,0.0021,0.0000), vec3(0.0041,0.0015,0.0000), vec3(0.0029,0.0010,0.0000),
	vec3(0.0020,0.0007,0.0000), vec3(0.0014,0.0005,0.0000), vec3(0.0010,0.0004,0.0000),
	vec3(0.0007,0.0002,0.0000), vec3(0.0005,0.0002,0.0000), vec3(0.0003,0.0001,0.0000),
	vec3(0.0002,0.0001,0.0000), vec3(0.0002,0.0001,0.0000), vec3(0.0001,0.0000,0.0000),
	vec3(0.0001,0.0000,0.0000), vec3(0.0001,0.0000,0.0000), vec3(0.0000,0.0000,0.0000)
);

void node_wavelength(float w, out vec4 col)
{
	float ii = (w-380.0) * (1.0/5.0);  // scaled 0..80
	int i = int(ii);
	vec3 color;

	if(i < 0 || i >= 80) {
		color = vec3(0.0, 0.0, 0.0);
	}
	else {
		ii -= i;
		color = mix(node_wavelength_LUT[i], node_wavelength_LUT[i+1], ii);
	}

	color = xyz_to_rgb(color.x, color.y, color.z);
	color *= 1.0/2.52;	// Empirical scale from lg to make all comps <= 1

	/* Clamp to zero if values are smaller */
	col = vec4(max(color, vec3(0.0, 0.0, 0.0)), 1.0);

	// srgb_to_linearrgb(col, col);
}

/* background */

void background_transform_to_world(vec3 viewvec, out vec3 worldvec)
{
	vec4 v = (gl_ProjectionMatrix[3][3] == 0.0) ? vec4(viewvec, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
	vec4 co_homogenous = (gl_ProjectionMatrixInverse * v);

	vec4 co = vec4(co_homogenous.xyz / co_homogenous.w, 0.0);
	worldvec = (gl_ModelViewMatrixInverse * co).xyz;
}

void node_background(vec4 color, float strength, vec3 N, out vec4 result)
{
	result = color * strength;
}

/* closures */

void node_mix_shader(float fac, vec4 shader1, vec4 shader2, out vec4 shader)
{
	shader = mix(shader1, shader2, saturate(fac));
}

void node_add_shader(vec4 shader1, vec4 shader2, out vec4 shader)
{
	shader = shader1 + shader2;
}

/* fresnel */

void node_fresnel(float ior, vec3 N, vec3 I, out float result)
{
	/* handle perspective/orthographic */
	vec3 I_view = (gl_ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);

	float eta = max(ior, 0.00001);
	result = fresnel_dielectric(I_view, N, (gl_FrontFacing) ? eta : 1.0/eta);
}

/* layer_weight */

void node_layer_weight(float blend, vec3 N, vec3 I, out float fresnel, out float facing)
{
	/* fresnel */
	float eta = max(1.0 - blend, 0.00001);
	vec3 I_view = (gl_ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);

	fresnel = fresnel_dielectric(I_view, N, (gl_FrontFacing)? 1.0/eta : eta );

	/* facing */
	facing = abs(dot(I_view, N));
	if (blend != 0.5) {
		blend = clamp(blend, 0.0, 0.99999);
		blend = (blend < 0.5) ? 2.0 * blend : 0.5 / (1.0 - blend);
		facing = pow(facing, blend);
	}
	facing = 1.0 - facing;
}

/* gamma */

void node_gamma(vec4 col, float gamma, out vec4 outcol)
{
	outcol = col;

	if (col.r > 0.0)
		outcol.r = compatible_pow(col.r, gamma);
	if (col.g > 0.0)
		outcol.g = compatible_pow(col.g, gamma);
	if (col.b > 0.0)
		outcol.b = compatible_pow(col.b, gamma);
}

/* geometry */

void node_attribute(vec3 attr, out vec4 outcol, out vec3 outvec, out float outf)
{
	outcol = vec4(attr, 1.0);
	outvec = attr;
	outf = (attr.x + attr.y + attr.z) / 3.0;
}

void node_uvmap(vec3 attr_uv, out vec3 outvec)
{
	outvec = attr_uv;
}

void tangent_orco_x(vec3 orco_in, out vec3 orco_out)
{
	orco_out = vec3(0.0, orco_in.z * -0.5, orco_in.y * 0.5);
}

void tangent_orco_y(vec3 orco_in, out vec3 orco_out)
{
	orco_out = vec3(orco_in.z * -0.5, 0.0, orco_in.x * 0.5);
}

void tangent_orco_z(vec3 orco_in, out vec3 orco_out)
{
	orco_out = vec3(orco_in.y * -0.5, orco_in.x * 0.5, 0.0);
}

void node_tangent(vec3 N, vec3 orco, mat4 objmat, mat4 invviewmat, out vec3 T)
{
	N = (invviewmat*vec4(N, 0.0)).xyz;
	T = (objmat*vec4(orco, 0.0)).xyz;
	T = cross(N, normalize(cross(T, N)));
}

void node_tangentmap(vec4 attr_tangent, mat4 toworld, out vec3 tangent)
{
	tangent = (toworld * vec4(attr_tangent.xyz, 0.0)).xyz;
}

void default_tangent(vec3 N, vec3 orco, mat4 objmat, mat4 viewmat, mat4 invviewmat, out vec3 T)
{
	orco = vec3(orco.y * -0.5, orco.x * 0.5, 0.0);
	node_tangent(N, orco, objmat, invviewmat, T);
	T = (viewmat * vec4(T, 0.0)).xyz;
}

void node_geometry(vec3 I, vec3 N, vec3 attr_orco, mat4 toworld, mat4 fromobj,
	out vec3 position, out vec3 normal, out vec3 tangent,
	out vec3 true_normal, out vec3 incoming, out vec3 parametric,
	out float backfacing, out float pointiness)
{
	position = (toworld * vec4(I, 1.0)).xyz;
	normal = (toworld * vec4(N, 0.0)).xyz;
	attr_orco = vec3(attr_orco.y * -0.5, attr_orco.x * 0.5, 0.0);
	node_tangent(N, attr_orco, fromobj, toworld, tangent);
	true_normal = normal;

	/* handle perspective/orthographic */
	vec3 I_view = (gl_ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);
	incoming = -(toworld * vec4(I_view, 0.0)).xyz;

	parametric = vec3(0.0);
	backfacing = (gl_FrontFacing) ? 0.0 : 1.0;
	pointiness = 0.5;
}

void node_geometry_lamp(vec3 N, vec4 P, vec3 I, mat4 toworld,
	out vec3 position, out vec3 normal, out vec3 tangent,
	out vec3 true_normal, out vec3 incoming, out vec3 parametric,
	out float backfacing, out float pointiness)
{
	position = (toworld*P).xyz;
	normal = normalize(toworld*vec4(N, 0.0)).xyz;
	tangent = vec3(0.0);
	true_normal = normal;
	incoming = normalize(toworld*vec4(I, 0.0)).xyz;

	parametric = vec3(0.0);
	backfacing = 0.0;
	pointiness = 0.0;
}

void node_tex_coord(
        vec3 I, vec3 N, mat4 viewinvmat, mat4 obinvmat, vec4 camerafac,
        vec3 attr_orco, vec3 attr_uv,
        out vec3 generated, out vec3 normal, out vec3 uv, out vec3 object,
        out vec3 camera, out vec3 window, out vec3 reflection)
{
	generated = attr_orco * 0.5 + vec3(0.5);
	normal = normalize((obinvmat * (viewinvmat * vec4(N, 0.0))).xyz);
	uv = attr_uv;
	object = (obinvmat * (viewinvmat * vec4(I, 1.0))).xyz;
	camera = vec3(I.xy, -I.z);
	vec4 projvec = gl_ProjectionMatrix * vec4(I, 1.0);
	window = vec3(mtex_2d_mapping(projvec.xyz / projvec.w).xy * camerafac.xy + camerafac.zw, 0.0);

	vec3 shade_I;
	shade_view(I, shade_I);
	vec3 view_reflection = reflect(shade_I, normalize(N));
	reflection = (viewinvmat * vec4(view_reflection, 0.0)).xyz;
}

void node_tex_coord_background(
        vec3 I, vec3 N, mat4 viewinvmat, mat4 obinvmat, vec4 camerafac,
        vec3 attr_orco, vec3 attr_uv,
        out vec3 generated, out vec3 normal, out vec3 uv, out vec3 object,
        out vec3 camera, out vec3 window, out vec3 reflection)
{
	vec4 v = (gl_ProjectionMatrix[3][3] == 0.0) ? vec4(I, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
	vec4 co_homogenous = (gl_ProjectionMatrixInverse * v);

	vec4 co = vec4(co_homogenous.xyz / co_homogenous.w, 0.0);

	co = normalize(co);
	vec3 coords = (gl_ModelViewMatrixInverse * co).xyz;

	generated = coords;
	normal = -coords;
	uv = vec3(attr_uv.xy, 0.0);
	object = coords;

	camera = vec3(co.xy, -co.z);
	window = (gl_ProjectionMatrix[3][3] == 0.0) ?
             vec3(mtex_2d_mapping(I).xy * camerafac.xy + camerafac.zw, 0.0) :
             vec3(vec2(0.5) * camerafac.xy + camerafac.zw, 0.0);

	reflection = -coords;
}

/* textures */

float calc_gradient(vec3 p, int gradient_type)
{
	float x, y, z;
	x = p.x;
	y = p.y;
	z = p.z;
	if (gradient_type == 0) {  /* linear */
		return x;
	}
	else if (gradient_type == 1) {  /* quadratic */
		float r = max(x, 0.0);
		return r * r;
	}
	else if (gradient_type == 2) {  /* easing */
		float r = min(max(x, 0.0), 1.0);
		float t = r * r;
		return (3.0 * t - 2.0 * t * r);
	}
	else if (gradient_type == 3) {  /* diagonal */
		return (x + y) * 0.5;
	}
	else if (gradient_type == 4) {  /* radial */
		return atan(y, x) / (M_PI * 2) + 0.5;
	}
	else {
		float r = max(1.0 - sqrt(x * x + y * y + z * z), 0.0);
		if (gradient_type == 5) {  /* quadratic sphere */
			return r * r;
		}
		else if (gradient_type == 6) {  /* sphere */
			return r;
		}
	}
	return 0.0;
}

void node_tex_gradient(vec3 co, float gradient_type, out vec4 color, out float fac)
{
	float f = calc_gradient(co, int(gradient_type));
	f = clamp(f, 0.0, 1.0);

	color = vec4(f, f, f, 1.0);
	fac = f;
}

void node_tex_checker(vec3 co, vec4 color1, vec4 color2, float scale, out vec4 color, out float fac)
{
	vec3 p = co * scale;

	/* Prevent precision issues on unit coordinates. */
	p.x = (p.x + 0.000001) * 0.999999;
	p.y = (p.y + 0.000001) * 0.999999;
	p.z = (p.z + 0.000001) * 0.999999;

	int xi = int(abs(floor(p.x)));
	int yi = int(abs(floor(p.y)));
	int zi = int(abs(floor(p.z)));

	bool check = ((mod(xi, 2) == mod(yi, 2)) == bool(mod(zi, 2)));

	color = check ? color1 : color2;
	fac = check ? 1.0 : 0.0;
}

#ifdef BIT_OPERATIONS
vec2 calc_brick_texture(vec3 p, float mortar_size, float bias,
                        float brick_width, float row_height,
                        float offset_amount, int offset_frequency,
                        float squash_amount, int squash_frequency)
{
	int bricknum, rownum;
	float offset = 0.0;
	float x, y;

	rownum = floor_to_int(p.y / row_height);

	if (offset_frequency != 0 && squash_frequency != 0) {
		brick_width *= (rownum % squash_frequency != 0) ? 1.0 : squash_amount; /* squash */
		offset = (rownum % offset_frequency != 0) ? 0.0 : (brick_width * offset_amount); /* offset */
	}

	bricknum = floor_to_int((p.x + offset) / brick_width);

	x = (p.x + offset) - brick_width * bricknum;
	y = p.y - row_height * rownum;

	return vec2(clamp((integer_noise((rownum << 16) + (bricknum & 0xFFFF)) + bias), 0.0, 1.0),
	            (x < mortar_size || y < mortar_size ||
	             x > (brick_width - mortar_size) ||
	             y > (row_height - mortar_size)) ? 1.0 : 0.0);
}
#endif

void node_tex_brick(vec3 co,
                    vec4 color1, vec4 color2,
                    vec4 mortar, float scale,
                    float mortar_size, float bias,
                    float brick_width, float row_height,
                    float offset_amount, float offset_frequency,
                    float squash_amount, float squash_frequency,
                    out vec4 color, out float fac)
{
#ifdef BIT_OPERATIONS
	vec2 f2 = calc_brick_texture(co * scale,
	                             mortar_size, bias,
	                             brick_width, row_height,
	                             offset_amount, int(offset_frequency),
	                             squash_amount, int(squash_frequency));
	float tint = f2.x;
	float f = f2.y;
	if (f != 1.0) {
		float facm = 1.0 - tint;
		color1 = facm * color1 + tint * color2;
	}
	color = (f == 1.0) ? mortar : color1;
	fac = f;
#else
	color = vec4(1.0);
	fac = 1.0;
#endif
}

void node_tex_clouds(vec3 co, float size, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
}

void node_tex_environment_equirectangular(vec3 co, sampler2D ima, out vec4 color)
{
	vec3 nco = normalize(co);
	float u = -atan(nco.y, nco.x) / (2.0 * M_PI) + 0.5;
	float v = atan(nco.z, hypot(nco.x, nco.y)) / M_PI + 0.5;

	color = texture2D(ima, vec2(u, v));
}

void node_tex_environment_mirror_ball(vec3 co, sampler2D ima, out vec4 color)
{
	vec3 nco = normalize(co);

	nco.y -= 1.0;

	float div = 2.0 * sqrt(max(-0.5 * nco.y, 0.0));
	if(div > 0.0)
		nco /= div;

	float u = 0.5 * (nco.x + 1.0);
	float v = 0.5 * (nco.z + 1.0);

	color = texture2D(ima, vec2(u, v));
}

void node_tex_environment_empty(vec3 co, out vec4 color)
{
	color = vec4(1.0, 0.0, 1.0, 1.0);
}

void node_tex_image(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
	color = texture2D(ima, co.xy);
	alpha = color.a;
}

void node_tex_image_closest(vec3 co, sampler2D ima, vec2 res, out vec4 color, out float alpha)
{
	color = texelFetch(ima, ivec2(fract(co.xy) * res), 0);
	alpha = color.a;
}

void node_tex_image_box(vec3 texco,
                        vec3 nob,
                        sampler2D ima,
                        float blend,
                        out vec4 color,
                        out float alpha)
{
	/* project from direction vector to barycentric coordinates in triangles */
	nob = vec3(abs(nob.x), abs(nob.y), abs(nob.z));
	nob /= (nob.x + nob.y + nob.z);

	/* basic idea is to think of this as a triangle, each corner representing
	 * one of the 3 faces of the cube. in the corners we have single textures,
	 * in between we blend between two textures, and in the middle we a blend
	 * between three textures.
	 *
	 * the Nxyz values are the barycentric coordinates in an equilateral
	 * triangle, which in case of blending, in the middle has a smaller
	 * equilateral triangle where 3 textures blend. this divides things into
	 * 7 zones, with an if () test for each zone */

	vec3 weight = vec3(0.0, 0.0, 0.0);
	float limit = 0.5 * (1.0 + blend);

	/* first test for corners with single texture */
	if (nob.x > limit * (nob.x + nob.y) && nob.x > limit * (nob.x + nob.z)) {
		weight.x = 1.0;
	}
	else if (nob.y > limit * (nob.x + nob.y) && nob.y > limit * (nob.y + nob.z)) {
		weight.y = 1.0;
	}
	else if (nob.z > limit * (nob.x + nob.z) && nob.z > limit * (nob.y + nob.z)) {
		weight.z = 1.0;
	}
	else if (blend > 0.0) {
		/* in case of blending, test for mixes between two textures */
		if (nob.z < (1.0 - limit) * (nob.y + nob.x)) {
			weight.x = nob.x / (nob.x + nob.y);
			weight.x = clamp((weight.x - 0.5 * (1.0 - blend)) / blend, 0.0, 1.0);
			weight.y = 1.0 - weight.x;
		}
		else if (nob.x < (1.0 - limit) * (nob.y + nob.z)) {
			weight.y = nob.y / (nob.y + nob.z);
			weight.y = clamp((weight.y - 0.5 * (1.0 - blend)) / blend, 0.0, 1.0);
			weight.z = 1.0 - weight.y;
		}
		else if (nob.y < (1.0 - limit) * (nob.x + nob.z)) {
			weight.x = nob.x / (nob.x + nob.z);
			weight.x = clamp((weight.x - 0.5 * (1.0 - blend)) / blend, 0.0, 1.0);
			weight.z = 1.0 - weight.x;
		}
		else {
			/* last case, we have a mix between three */
			weight.x = ((2.0 - limit) * nob.x + (limit - 1.0)) / (2.0 * limit - 1.0);
			weight.y = ((2.0 - limit) * nob.y + (limit - 1.0)) / (2.0 * limit - 1.0);
			weight.z = ((2.0 - limit) * nob.z + (limit - 1.0)) / (2.0 * limit - 1.0);
		}
	}
	else {
		/* Desperate mode, no valid choice anyway, fallback to one side.*/
		weight.x = 1.0;
	}

	color = vec4(0);
	if (weight.x > 0.0) {
		color += weight.x * texture2D(ima, texco.yz);
	}
	if (weight.y > 0.0) {
		color += weight.y * texture2D(ima, texco.xz);
	}
	if (weight.z > 0.0) {
		color += weight.z * texture2D(ima, texco.yx);
	}

	alpha = color.a;
}

void node_tex_image_empty(vec3 co, out vec4 color, out float alpha)
{
	color = vec4(0.0);
	alpha = 0.0;
}

void node_tex_magic(vec3 co, float scale, float distortion, float depth, out vec4 color, out float fac)
{
	vec3 p = co * scale;
	float x = sin((p.x + p.y + p.z) * 5.0);
	float y = cos((-p.x + p.y - p.z) * 5.0);
	float z = -cos((-p.x - p.y + p.z) * 5.0);

	if (depth > 0) {
		x *= distortion;
		y *= distortion;
		z *= distortion;
		y = -cos(x - y + z);
		y *= distortion;
		if (depth > 1) {
			x = cos(x - y - z);
			x *= distortion;
			if (depth > 2) {
				z = sin(-x - y - z);
				z *= distortion;
				if (depth > 3) {
					x = -cos(-x + y - z);
					x *= distortion;
					if (depth > 4) {
						y = -sin(-x + y + z);
						y *= distortion;
						if (depth > 5) {
							y = -cos(-x + y + z);
							y *= distortion;
							if (depth > 6) {
								x = cos(x + y + z);
								x *= distortion;
								if (depth > 7) {
									z = sin(x + y - z);
									z *= distortion;
									if (depth > 8) {
										x = -cos(-x - y + z);
										x *= distortion;
										if (depth > 9) {
											y = -sin(x - y + z);
											y *= distortion;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if (distortion != 0.0) {
		distortion *= 2.0;
		x /= distortion;
		y /= distortion;
		z /= distortion;
	}

	color = vec4(0.5 - x, 0.5 - y, 0.5 - z, 1.0);
	fac = (color.x + color.y + color.z) / 3.0;
}

#ifdef BIT_OPERATIONS
float noise_fade(float t)
{
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float noise_scale3(float result)
{
	return 0.9820 * result;
}

float noise_nerp(float t, float a, float b)
{
	return (1.0 - t) * a + t * b;
}

float noise_grad(uint hash, float x, float y, float z)
{
	uint h = hash & 15u;
	float u = h < 8u ? x : y;
	float vt = ((h == 12u) || (h == 14u)) ? x : z;
	float v = h < 4u ? y : vt;
	return (((h & 1u) != 0u) ? -u : u) + (((h & 2u) != 0u) ? -v : v);
}

float noise_perlin(float x, float y, float z)
{
	int X; float fx = floorfrac(x, X);
	int Y; float fy = floorfrac(y, Y);
	int Z; float fz = floorfrac(z, Z);

	float u = noise_fade(fx);
	float v = noise_fade(fy);
	float w = noise_fade(fz);

	float result;

	result = noise_nerp(w, noise_nerp(v, noise_nerp(u, noise_grad(hash(X, Y, Z), fx, fy, fz),
	                                                noise_grad(hash(X + 1, Y, Z), fx - 1.0, fy, fz)),
	                                  noise_nerp(u, noise_grad(hash(X, Y + 1, Z), fx, fy - 1.0, fz),
	                                             noise_grad(hash(X + 1, Y + 1, Z), fx - 1.0, fy - 1.0, fz))),
	                    noise_nerp(v, noise_nerp(u, noise_grad(hash(X, Y, Z + 1), fx, fy, fz - 1.0),
	                                             noise_grad(hash(X + 1, Y, Z + 1), fx - 1.0, fy, fz - 1.0)),
	                               noise_nerp(u, noise_grad(hash(X, Y + 1, Z + 1), fx, fy - 1.0, fz - 1.0),
	                                          noise_grad(hash(X + 1, Y + 1, Z + 1), fx - 1.0, fy - 1.0, fz - 1.0))));
	return noise_scale3(result);
}

float noise(vec3 p)
{
	return 0.5 * noise_perlin(p.x, p.y, p.z) + 0.5;
}

float snoise(vec3 p)
{
	return noise_perlin(p.x, p.y, p.z);
}

float noise_turbulence(vec3 p, float octaves, int hard)
{
	float fscale = 1.0;
	float amp = 1.0;
	float sum = 0.0;
	int i, n;
	octaves = clamp(octaves, 0.0, 16.0);
	n = int(octaves);
	for (i = 0; i <= n; i++) {
		float t = noise(fscale * p);
		if (hard != 0) {
			t = abs(2.0 * t - 1.0);
		}
		sum += t * amp;
		amp *= 0.5;
		fscale *= 2.0;
	}
	float rmd = octaves - floor(octaves);
	if  (rmd != 0.0) {
		float t = noise(fscale * p);
		if (hard != 0) {
			t = abs(2.0 * t - 1.0);
		}
		float sum2 = sum + t * amp;
		sum *= (float(1 << n) / float((1 << (n + 1)) - 1));
		sum2 *= (float(1 << (n + 1)) / float((1 << (n + 2)) - 1));
		return (1.0 - rmd) * sum + rmd * sum2;
	}
	else {
		sum *= (float(1 << n) / float((1 << (n + 1)) - 1));
		return sum;
	}
}
#endif  // BIT_OPERATIONS

void node_tex_noise(vec3 co, float scale, float detail, float distortion, out vec4 color, out float fac)
{
#ifdef BIT_OPERATIONS
	vec3 p = co * scale;
	int hard = 0;
	if (distortion != 0.0) {
		vec3 r, offset = vec3(13.5, 13.5, 13.5);
		r.x = noise(p + offset) * distortion;
		r.y = noise(p) * distortion;
		r.z = noise(p - offset) * distortion;
		p += r;
	}

	fac = noise_turbulence(p, detail, hard);
	color = vec4(fac,
	             noise_turbulence(vec3(p.y, p.x, p.z), detail, hard),
	             noise_turbulence(vec3(p.y, p.z, p.x), detail, hard),
	             1);
#else  // BIT_OPERATIONS
	color = vec4(1.0);
	fac = 1.0;
#endif  // BIT_OPERATIONS
}


#ifdef BIT_OPERATIONS

/* Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

float noise_musgrave_fBm(vec3 p, float H, float lacunarity, float octaves)
{
	float rmd;
	float value = 0.0;
	float pwr = 1.0;
	float pwHL = pow(lacunarity, -H);
	int i;

	for (i = 0; i < int(octaves); i++) {
		value += snoise(p) * pwr;
		pwr *= pwHL;
		p *= lacunarity;
	}

	rmd = octaves - floor(octaves);
	if (rmd != 0.0)
		value += rmd * snoise(p) * pwr;

	return value;
}

/* Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

float noise_musgrave_multi_fractal(vec3 p, float H, float lacunarity, float octaves)
{
	float rmd;
	float value = 1.0;
	float pwr = 1.0;
	float pwHL = pow(lacunarity, -H);
	int i;

	for (i = 0; i < int(octaves); i++) {
		value *= (pwr * snoise(p) + 1.0);
		pwr *= pwHL;
		p *= lacunarity;
	}

	rmd = octaves - floor(octaves);
	if (rmd != 0.0)
		value *= (rmd * pwr * snoise(p) + 1.0); /* correct? */

	return value;
}

/* Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_hetero_terrain(vec3 p, float H, float lacunarity, float octaves, float offset)
{
	float value, increment, rmd;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	/* first unscaled octave of function; later octaves are scaled */
	value = offset + snoise(p);
	p *= lacunarity;

	for (i = 1; i < int(octaves); i++) {
		increment = (snoise(p) + offset) * pwr * value;
		value += increment;
		pwr *= pwHL;
		p *= lacunarity;
	}

	rmd = octaves - floor(octaves);
	if (rmd != 0.0) {
		increment = (snoise(p) + offset) * pwr * value;
		value += rmd * increment;
	}

	return value;
}

/* Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_hybrid_multi_fractal(vec3 p, float H, float lacunarity, float octaves, float offset, float gain)
{
	float result, signal, weight, rmd;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	result = snoise(p) + offset;
	weight = gain * result;
	p *= lacunarity;

	for (i = 1; (weight > 0.001f) && (i < int(octaves)); i++) {
		if (weight > 1.0)
			weight = 1.0;

		signal = (snoise(p) + offset) * pwr;
		pwr *= pwHL;
		result += weight * signal;
		weight *= gain * signal;
		p *= lacunarity;
	}

	rmd = octaves - floor(octaves);
	if (rmd != 0.0)
		result += rmd * ((snoise(p) + offset) * pwr);

	return result;
}

/* Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_ridged_multi_fractal(vec3 p, float H, float lacunarity, float octaves, float offset, float gain)
{
	float result, signal, weight;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	signal = offset - abs(snoise(p));
	signal *= signal;
	result = signal;
	weight = 1.0;

	for (i = 1; i < int(octaves); i++) {
		p *= lacunarity;
		weight = clamp(signal * gain, 0.0, 1.0);
		signal = offset - abs(snoise(p));
		signal *= signal;
		signal *= weight;
		result += signal * pwr;
		pwr *= pwHL;
	}

	return result;
}

float svm_musgrave(int type,
                   float dimension,
                   float lacunarity,
                   float octaves,
                   float offset,
                   float intensity,
                   float gain,
                   vec3 p)
{
	if (type == 0 /*NODE_MUSGRAVE_MULTIFRACTAL*/)
		return intensity * noise_musgrave_multi_fractal(p, dimension, lacunarity, octaves);
	else if (type == 1 /*NODE_MUSGRAVE_FBM*/)
		return intensity * noise_musgrave_fBm(p, dimension, lacunarity, octaves);
	else if (type == 2 /*NODE_MUSGRAVE_HYBRID_MULTIFRACTAL*/)
		return intensity * noise_musgrave_hybrid_multi_fractal(p, dimension, lacunarity, octaves, offset, gain);
	else if (type == 3 /*NODE_MUSGRAVE_RIDGED_MULTIFRACTAL*/)
		return intensity * noise_musgrave_ridged_multi_fractal(p, dimension, lacunarity, octaves, offset, gain);
	else if (type == 4 /*NODE_MUSGRAVE_HETERO_TERRAIN*/)
		return intensity * noise_musgrave_hetero_terrain(p, dimension, lacunarity, octaves, offset);
	return 0.0;
}
#endif  // #ifdef BIT_OPERATIONS

void node_tex_musgrave(vec3 co,
                       float scale,
                       float detail,
                       float dimension,
                       float lacunarity,
                       float offset,
                       float gain,
                       float type,
                       out vec4 color,
                       out float fac)
{
#ifdef BIT_OPERATIONS
	fac = svm_musgrave(int(type),
	                   dimension,
	                   lacunarity,
	                   detail,
	                   offset,
	                   1.0,
	                   gain,
	                   co * scale);
#else
	fac = 1.0;
#endif

	color = vec4(fac, fac, fac, 1.0);
}

void node_tex_sky(vec3 co, out vec4 color)
{
	color = vec4(1.0);
}

void node_tex_voronoi(vec3 co, float scale, float coloring, out vec4 color, out float fac)
{
#ifdef BIT_OPERATIONS
	vec3 p = co * scale;
	int xx, yy, zz, xi, yi, zi;
	float da[4];
	vec3 pa[4];

	xi = floor_to_int(p[0]);
	yi = floor_to_int(p[1]);
	zi = floor_to_int(p[2]);

	da[0] = 1e+10;
	da[1] = 1e+10;
	da[2] = 1e+10;
	da[3] = 1e+10;

	for (xx = xi - 1; xx <= xi + 1; xx++) {
		for (yy = yi - 1; yy <= yi + 1; yy++) {
			for (zz = zi - 1; zz <= zi + 1; zz++) {
				vec3 ip = vec3(xx, yy, zz);
				vec3 vp = cellnoise_color(ip);
				vec3 pd = p - (vp + ip);
				float d = dot(pd, pd);
				vp += vec3(xx, yy, zz);
				if (d < da[0]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = da[0];
					da[0] = d;
					pa[3] = pa[2];
					pa[2] = pa[1];
					pa[1] = pa[0];
					pa[0] = vp;
				}
				else if (d < da[1]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = d;

					pa[3] = pa[2];
					pa[2] = pa[1];
					pa[1] = vp;
				}
				else if (d < da[2]) {
					da[3] = da[2];
					da[2] = d;

					pa[3] = pa[2];
					pa[2] = vp;
				}
				else if (d < da[3]) {
					da[3] = d;
					pa[3] = vp;
				}
			}
		}
	}

	if (coloring == 0.0) {
		fac = abs(da[0]);
		color = vec4(fac, fac, fac, 1);
	}
	else {
		color = vec4(cellnoise_color(pa[0]), 1);
		fac = (color.x + color.y + color.z) * (1.0 / 3.0);
	}
#else  // BIT_OPERATIONS
	color = vec4(1.0);
	fac = 1.0;
#endif  // BIT_OPERATIONS
}

#ifdef BIT_OPERATIONS
float calc_wave(vec3 p, float distortion, float detail, float detail_scale, int wave_type, int wave_profile)
{
	float n;

	if (wave_type == 0) /* type bands */
		n = (p.x + p.y + p.z) * 10.0;
	else /* type rings */
		n = length(p) * 20.0;

	if (distortion != 0.0)
		n += distortion * noise_turbulence(p * detail_scale, detail, 0);

	if (wave_profile == 0) { /* profile sin */
		return 0.5 + 0.5 * sin(n);
	}
	else { /* profile saw */
		n /= 2.0 * M_PI;
		n -= int(n);
		return (n < 0.0) ? n + 1.0 : n;
	}
}
#endif  // BIT_OPERATIONS

void node_tex_wave(
        vec3 co, float scale, float distortion, float detail, float detail_scale, float wave_type, float wave_profile,
        out vec4 color, out float fac)
{
#ifdef BIT_OPERATIONS
	float f;
	f = calc_wave(co * scale, distortion, detail, detail_scale, int(wave_type), int(wave_profile));

	color = vec4(f, f, f, 1.0);
	fac = f;
#else  // BIT_OPERATIONS
	color = vec4(1.0);
	fac = 1;
#endif  // BIT_OPERATIONS
}

/* light path */

void node_light_path(
	out float is_camera_ray,
	out float is_shadow_ray,
	out float is_diffuse_ray,
	out float is_glossy_ray,
	out float is_singular_ray,
	out float is_reflection_ray,
	out float is_transmission_ray,
	out float ray_length,
	out float ray_depth,
	out float transparent_depth,
	out float transmission_depth)
{
	is_camera_ray = 1.0;
	is_shadow_ray = 0.0;
	is_diffuse_ray = 0.0;
	is_glossy_ray = 0.0;
	is_singular_ray = 0.0;
	is_reflection_ray = 0.0;
	is_transmission_ray = 0.0;
	ray_length = 1.0;
	ray_depth = 1.0;
	transparent_depth = 1.0;
	transmission_depth = 1.0;
}

void node_light_falloff(float strength, float tsmooth, vec4 lamppos, vec3 pos, out float quadratic, out float linear, out float constant)
{
	float ray_length = length(lamppos.xyz - pos);

	if (tsmooth > 0.0) {
		float squared = ray_length * ray_length;
		strength *= squared / (tsmooth + squared);
	}

	quadratic = strength;
	linear = (strength * ray_length);
	constant = (strength * ray_length * ray_length);
}

void node_object_info(mat4 objmat, out vec3 location, out float object_index, out float material_index, out float random)
{
	location = objmat[3].xyz;
	object_index = 0.0;
	material_index = 0.0;
	random = 0.0;
}

void node_normal_map(vec4 tangent, vec3 normal, vec3 texnormal, out vec3 outnormal)
{
	vec3 B = tangent.w * cross(normal, tangent.xyz);

	outnormal = texnormal.x * tangent.xyz + texnormal.y * B + texnormal.z * normal;
	outnormal = normalize(outnormal);
}

void node_bump(float strength, float dist, float height, vec3 N, vec3 surf_pos, float invert, out vec3 result)
{
	if (invert != 0.0) {
		dist *= -1.0;
	}
	vec3 dPdx = dFdx(surf_pos);
	vec3 dPdy = dFdy(surf_pos);

	/* Get surface tangents from normal. */
	vec3 Rx = cross(dPdy, N);
	vec3 Ry = cross(N, dPdx);

	/* Compute surface gradient and determinant. */
	float det = dot(dPdx, Rx);
	float absdet = abs(det);

	float dHdx = dFdx(height);
	float dHdy = dFdy(height);
	vec3 surfgrad = dHdx * Rx + dHdy * Ry;

	strength = max(strength, 0.0);

	result = normalize(absdet * N - dist * sign(det) * surfgrad);
	result = normalize(strength * result + (1.0 - strength) * N);
}

/* output */

void node_output_material(vec4 surface, vec4 volume, float displacement, out vec4 result)
{
	result = surface;
}

void node_output_world(vec4 surface, vec4 volume, out vec4 result)
{
	result = surface;
}

void node_output_lamp(vec4 surface, out vec4 result)
{
	result = surface;
}