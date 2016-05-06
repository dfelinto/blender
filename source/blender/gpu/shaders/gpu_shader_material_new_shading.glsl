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
	worldvec = normalize( (gl_ModelViewMatrixInverse * co).xyz );
}

void background_sampling_default(vec3 viewvec, mat4 viewinvmat, out vec3 worldvec)
{
	viewvec = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(viewvec): vec3(0.0, 0.0, -1.0);
	worldvec = normalize( (viewinvmat * vec4(viewvec,0.0)).xyz );
}

void node_background(vec4 color, float strength, vec3 N, out vec4 result)
{
	result = vec4(color.rgb * strength, 1.0);
}

/* closures */

void node_mix_shader(float fac, vec4 shader1, vec4 shader2, out vec4 shader)
{
	shader = mix(shader1, shader2, clamp(fac, 0.0, 1.0));
}

void node_add_shader(vec4 shader1, vec4 shader2, out vec4 shader)
{
	shader = shader1 + shader2;
}

/* fresnel */

void node_fresnel(float ior, vec3 N, vec3 I, out float result)
{
	/* handle perspective/orthographic */
	vec3 I_view = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(I): vec3(0.0, 0.0, -1.0);

	float eta = max(ior, 0.00001);
	result = fresnel_dielectric(I_view, N, (gl_FrontFacing)? eta: 1.0/eta);
}

/* layer_weight */

void node_layer_weight(float blend, vec3 N, vec3 I, out float fresnel, out float facing)
{
	/* fresnel */
	float eta = max(1.0 - blend, 0.00001);
	vec3 I_view = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(I): vec3(0.0, 0.0, -1.0);

	fresnel = fresnel_dielectric(I_view, N, (gl_FrontFacing)? 1.0/eta : eta );

	/* facing */
	facing = abs(dot(I_view, N));
	if(blend != 0.5) {
		blend = clamp(blend, 0.0, 0.99999);
		blend = (blend < 0.5)? 2.0*blend: 0.5/(1.0 - blend);
		facing = pow(facing, blend);
	}
	facing = 1.0 - facing;
}

/* gamma */

void node_gamma(vec4 col, float gamma, out vec4 outcol)
{
	outcol = col;

	if(col.r > 0.0)
		outcol.r = compatible_pow(col.r, gamma);
	if(col.g > 0.0)
		outcol.g = compatible_pow(col.g, gamma);
	if(col.b > 0.0)
		outcol.b = compatible_pow(col.b, gamma);
}

/* geometry */

void node_attribute(vec3 attr_uv, out vec4 outcol, out vec3 outvec, out float outf)
{
	outcol = vec4(attr_uv, 1.0);
	outvec = attr_uv;
	outf = (attr_uv.x + attr_uv.y + attr_uv.z)/3.0;
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
	position = (toworld*vec4(I, 1.0)).xyz;
	normal = (toworld*vec4(N, 0.0)).xyz;
	attr_orco = vec3(attr_orco.y * -0.5, attr_orco.x * 0.5, 0.0);
	node_tangent(N, attr_orco, fromobj, toworld, tangent);
	true_normal = normal;

	/* handle perspective/orthographic */
	vec3 I_view = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(I): vec3(0.0, 0.0, -1.0);
	incoming = -(toworld*vec4(I_view, 0.0)).xyz;

	parametric = vec3(0.0);
	backfacing = (gl_FrontFacing)? 0.0: 1.0;
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

void node_tex_coord(vec3 I, vec3 N, mat4 viewinvmat, mat4 obinvmat, vec4 camerafac,
	vec3 attr_orco, vec3 attr_uv,
	out vec3 generated, out vec3 normal, out vec3 uv, out vec3 object,
	out vec3 camera, out vec3 window, out vec3 reflection)
{
	generated = attr_orco * 0.5 + vec3(0.5);
	normal = normalize((obinvmat*(viewinvmat*vec4(N, 0.0))).xyz);
	uv = attr_uv;
	object = (obinvmat*(viewinvmat*vec4(I, 1.0))).xyz;
	camera = vec3(I.xy, -I.z);
	vec4 projvec = gl_ProjectionMatrix * vec4(I, 1.0);
	window = vec3(mtex_2d_mapping(projvec.xyz/projvec.w).xy * camerafac.xy + camerafac.zw, 0.0);

	vec3 shade_I;
	shade_view(I, shade_I);
	vec3 view_reflection = reflect(shade_I, normalize(N));
	reflection = (viewinvmat*vec4(view_reflection, 0.0)).xyz;
}

void node_tex_coord_background(vec3 I, vec3 N, mat4 viewinvmat, mat4 obinvmat, vec4 camerafac,
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

void node_tex_gradient(vec3 co, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
}

void node_tex_checker(vec3 co, vec4 color1, vec4 color2, float scale, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
}

void node_tex_brick(vec3 co, vec4 color1, vec4 color2, vec4 mortar, float scale, float mortar_size, float bias, float brick_width, float row_height, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
}

void node_tex_clouds(vec3 co, float size, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
}

void node_tex_environment_equirectangular(vec3 co, sampler2D ima, out vec4 color)
{
	vec3 nco = normalize(co);
	float u = -atan(nco.y, nco.x)/(M_2PI) + 0.5;
	float v = atan(nco.z, hypot(nco.x, nco.y))/M_PI + 0.5;

	color = texture2D(ima, vec2(u, v));
}

void node_tex_environment_mirror_ball(vec3 co, sampler2D ima, out vec4 color)
{
	vec3 nco = normalize(co);

	nco.y -= 1.0;

	float div = 2.0*sqrt(max(-0.5*nco.y, 0.0));
	if(div > 0.0)
		nco /= div;

	float u = 0.5*(nco.x + 1.0);
	float v = 0.5*(nco.z + 1.0);

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

void node_tex_image_empty(vec3 co, out vec4 color, out float alpha)
{
	color = vec4(0.0);
	alpha = 0.0;
}

void node_tex_magic(vec3 p, float scale, float distortion, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
}

void node_tex_musgrave(vec3 co, float scale, float detail, float dimension, float lacunarity, float offset, float gain, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
}

#define TEX_NOISE_PERLIN 0
#define TEX_NOISE_VORONOI_F1 1
#define TEX_NOISE_VORONOI_F2 2
#define TEX_NOISE_VORONOI_F3 3
#define TEX_NOISE_VORONOI_F4 4
#define TEX_NOISE_VORONOI_F2_F1 5
#define TEX_NOISE_VORONOI_CRACKLE 6
#define TEX_NOISE_CELL_NOISE 7

int texture_quick_floor(float x)
{
	return int(x) - ((x < 0) ? 1 : 0);
}

float texture_fade(float t)
{
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float texture_scale3(float result)
{
	return 0.9820 * result;
}

float texture_nerp(float t, float a, float b)
{
	return (1.0 - t) * a + t * b;
}

float texture_grad(int hash, float x, float y, float z)
{
	// use vectors pointing to the edges of the cube
	int h = hash & 15;
	float u = h<8 ? x : y;
	float v = h<4 ? y : h == 12 || h == 14 ? x : z;
	return (bool(h&1) ? -u : u) + (bool(h&2) ? -v : v);
}

uint texture_rot(uint x, uint k)
{
	return (x<<k) | (x>>(uint(32)-k));
}

uint texture_hash(int kx, int ky, int kz)
{
	uint deadbeef = uint(0xdeadbeef);
	uint len = uint(3);

	uint a = deadbeef + uint(len << 2u) + uint(13);
	uint b = a;
	uint c = a;

	c += uint(kz);
	b += uint(ky);
	a += uint(kx);

	c ^= b;
	c -= texture_rot(b,uint(14));
	a ^= c;
	a -= texture_rot(c,uint(11));
	b ^= a;
	b -= texture_rot(a,uint(25));
	c ^= b;
	c -= texture_rot(b,uint(16));
	a ^= c;
	a -= texture_rot(c,uint(4));
	b ^= a;
	b -= texture_rot(a,uint(14));
	c ^= b;
	c -= texture_rot(b,uint(24));

	return c;
}

float texture_perlin (float x, float y, float z) {

	int X = texture_quick_floor(x);
	float fx = x-float(X);
	int Y = texture_quick_floor(y);
	float fy = y-float(Y);
	int Z = texture_quick_floor(z);
	float fz = z-float(Z);

	float u = texture_fade(fx);
	float v = texture_fade(fy);
	float w = texture_fade(fz);

    float result = texture_nerp (w, texture_nerp (v, texture_nerp (u, texture_grad (int(texture_hash (X  , Y  , Z  )), fx     ,fy     , fz     ),
                                                                      texture_grad (int(texture_hash (X+1, Y  , Z  )), fx-1.0, fy     , fz     )),
                                                     texture_nerp (u, texture_grad (int(texture_hash (X  , Y+1, Z  )), fx     ,fy-1.0 , fz     ),
                                                                      texture_grad (int(texture_hash (X+1, Y+1, Z  )), fx-1.0, fy-1.0 , fz     ))),
                                    texture_nerp (v, texture_nerp (u, texture_grad (int(texture_hash (X  , Y  , Z+1)), fx     ,fy     , fz-1.0 ),
                                                                      texture_grad (int(texture_hash (X+1, Y  , Z+1)), fx-1.0, fy     , fz-1.0 )),
                                                     texture_nerp (u, texture_grad (int(texture_hash (X  , Y+1, Z+1)), fx     ,fy-1.0 , fz-1.0 ),
                                                                      texture_grad (int(texture_hash (X+1, Y+1, Z+1)), fx-1.0, fy-1.0 , fz-1.0 ))));
	return texture_scale3(result);
}

// perlin noise in range -1..1
float texture_snoise(vec3 p)
{
	return texture_perlin(p.x, p.y, p.z);
}

// perlin noise in range 0..1
float texture_noise(vec3 p)
{
	float r = texture_perlin(p.x, p.y, p.z);
	return 0.5*r + 0.5;
}

float texture_safe_noise(vec3 p, int type)
{
	float f = 0.0;

	/* Perlin noise in range -1..1 */
	if (type == 0)
		f = texture_snoise(p);

	/* Perlin noise in range 0..1 */
	else
		f = texture_noise(p);

	/* can happen for big coordinates, things even out to 0.5 then anyway */
	if (f > 1.18118254e+36 || f < -1.18118254e+36)
		return 0.5;

	return f;
}

float texture_noise_basis(vec3 p, int basis)
{
	if (basis == TEX_NOISE_PERLIN)
		return texture_safe_noise(p, 1);
	/* not supported in cycles? */
	/*
	if (basis == TEX_NOISE_VORONOI_F1)
		return voronoi_F1S(p);
	if (basis == TEX_NOISE_VORONOI_F2)
		return voronoi_F2S(p);
	if (basis == TEX_NOISE_VORONOI_F3)
		return voronoi_F3S(p);
	if (basis == TEX_NOISE_VORONOI_F4)
		return voronoi_F4S(p);
	if (basis == TEX_NOISE_VORONOI_F2_F1)
		return voronoi_F1F2S(p);
	if (basis == TEX_NOISE_VORONOI_CRACKLE)
		return voronoi_CrS(p);
	if (basis == TEX_NOISE_CELL_NOISE)
		return texture_cellnoise(p);
	*/
	return 0.0;
}

float texture_noise_turbulence(vec3 p, int basis, float details, int hard)
{

	float fscale = 1.0;
	float amp = 1.0;
	float sum = 0.0;
	int i, n;

	float octaves = clamp(details, 0.0, 16.0);
	n = int(octaves);

	for (i = 0; i <= n; i++) {
		float t = texture_noise_basis(fscale * p, basis);

		if (hard==1)
			t = abs(2.0 * t - 1.0);

		sum += t * amp;
		amp *= 0.5;
		fscale *= 2.0;
	}

	float rmd = octaves - floor(octaves);

	if (rmd != 0.0) {
		float t = texture_noise_basis(fscale * p, basis);

		if (bool(hard))
			t = abs(2.0 * t - 1.0);

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

void node_tex_noise(vec3 co, float scale, float detail, float distortion, out vec4 color, out float fac)
{
	int basis = TEX_NOISE_PERLIN;
	int hard = 0;

	co *= scale;

	if(distortion != 0.0) {
		vec3 r, offset = vec3(13.5);

		r.x = texture_noise_basis(co + offset, basis) * distortion;
		r.y = texture_noise_basis(co, basis) * distortion;
		r.z = texture_noise_basis(co - offset, basis) * distortion;

		co += r;
	}

	fac = texture_noise_turbulence(co, basis, detail, hard);
	color = vec4(fac,
		texture_noise_turbulence(vec3(co.y, co.x, co.z), basis, detail, hard),
		texture_noise_turbulence(vec3(co.y, co.z, co.x), basis, detail, hard),
		1.0);
}

void node_tex_sky(vec3 co, out vec4 color)
{
	color = vec4(1.0);
}

void node_tex_voronoi(vec3 co, float scale, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
}

void node_tex_wave(vec3 co, float scale, float distortion, float detail, float detail_scale, out vec4 color, out float fac)
{
	color = vec4(1.0);
	fac = 1.0;
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

void node_normal_map_tangent(float strength, vec4 color, vec3 N, vec4 T, mat4 viewmat, mat4 obmat, mat4 viewinvmat, mat4 obinvmat, out vec3 result)
{
	color = ( color - vec4(0.5))*vec4(2.0);
	N = normalize((obinvmat*(viewinvmat*vec4(N, 0.0))).xyz);
	T = vec4( normalize((obinvmat*(viewinvmat*vec4(T.xyz, 0.0))).xyz), T.w);
	//vec3 No = color.xyz * vec3(1.0, -1.0, 1.0);
	vec3 B = T.w * cross(N, T.xyz);
	vec3 No = from_tangent_to_world(color.xyz, N, T.xyz, B);
	result = normalize(N + (No - N) * max(strength, 0.0));
	result = normalize((obmat*vec4(result,0.0)).xyz);
}

void node_normal_map_object(float strength, vec4 color, vec3 N, vec4 T, mat4 viewmat, mat4 obmat, mat4 viewinvmat, mat4 obinvmat, out vec3 result)
{
	color = ( color - vec4(0.5))*vec4(2.0);
	vec3 No = color.xyz * vec3(1.0, -1.0, 1.0);
	N = normalize((obinvmat*(viewinvmat*vec4(N, 0.0))).xyz);
	result = normalize(N + (No - N) * max(strength, 0.0));
	result = normalize((obmat*vec4(result,0.0)).xyz);
}

void node_normal_map_world(float strength, vec4 color, vec3 N, vec4 T, mat4 viewmat, mat4 obmat, mat4 viewinvmat, mat4 obinvmat, out vec3 result)
{
	color = ( color - vec4(0.5))*vec4(2.0);
	vec3 No = color.xyz * vec3(1.0, -1.0, 1.0);
	N = normalize((viewinvmat*vec4(N, 0.0)).xyz);
	result = normalize(N + (No - N) * max(strength, 0.0));
}

void node_normal_map_blend_object(float strength, vec4 color, vec3 N, vec4 T, mat4 viewmat, mat4 obmat, mat4 viewinvmat, mat4 obinvmat, out vec3 result)
{
	color = ( color - vec4(0.5))*vec4(2.0);
	vec3 No = color.xzy * vec3(1.0, -1.0, 1.0);
	N = normalize((obinvmat*(viewinvmat*vec4(N, 0.0))).xyz);
	result = normalize(N + (No - N) * max(strength, 0.0));
	result = normalize((obmat*vec4(result,0.0)).xyz);
}

void node_normal_map_blend_world(float strength, vec4 color, vec3 N, vec4 T, mat4 viewmat, mat4 obmat, mat4 viewinvmat, mat4 obinvmat, out vec3 result)
{
	color = ( color - vec4(0.5))*vec4(2.0);
	vec3 No = color.xzy * vec3(1.0, -1.0, 1.0);
	N = normalize((viewinvmat*vec4(N, 0.0)).xyz);
	result = normalize(N + (No - N) * max(strength, 0.0));
}

void node_bump(float strength, float dist, float height, vec3 N, out vec3 result)
{
	result = N;
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