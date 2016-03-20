uniform vec3 sh0;
uniform vec3 sh1;
uniform vec3 sh2;
uniform vec3 sh3;
uniform vec3 sh4;
uniform vec3 sh5;
uniform vec3 sh6;
uniform vec3 sh7;
uniform vec3 sh8;


varying vec3 varposition;
varying vec3 varnormal;

float linearrgb_to_srgb(float c)
{
	if(c < 0.0031308)
		return (c < 0.0) ? 0.0: c * 12.92;
	else
		return 1.055 * pow(c, 1.0/2.4) - 0.055;
}

void linearrgb_to_srgb(vec4 col_from, out vec4 col_to)
{
	col_to.r = linearrgb_to_srgb(col_from.r);
	col_to.g = linearrgb_to_srgb(col_from.g);
	col_to.b = linearrgb_to_srgb(col_from.b);
	col_to.a = col_from.a;
}

void main()
{
	vec4 v = (gl_ProjectionMatrix[3][3] == 0.0) ? vec4(varposition, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
	vec4 co_homogenous = (gl_ProjectionMatrixInverse * v);

	vec4 co = vec4(co_homogenous.xyz / co_homogenous.w, 0.0);
	vec3 worldvec = normalize( (gl_ModelViewMatrixInverse * co).xyz );

	/* Second order Spherical Harmonics */
	/* http://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/ */
	vec3 sh = vec3(0.0);

	sh += 0.282095 * sh0;

	sh += -0.488603 * worldvec.z * sh1;
	sh += 0.488603 * worldvec.y * sh2;
	sh += -0.488603 * worldvec.x * sh3;

	sh += 1.092548 * worldvec.x * worldvec.z * sh4;
	sh += -1.092548 * worldvec.z * worldvec.y * sh5;
	sh += 0.315392 * (3.0 * worldvec.y * worldvec.y - 1.0) * sh6;
	sh += -1.092548 * worldvec.x * worldvec.y * sh7;
	sh += 0.546274 * (worldvec.x * worldvec.x - worldvec.z * worldvec.z) * sh8;

	vec4 shcolor = vec4(sh, 1.0);
	linearrgb_to_srgb(shcolor, shcolor);
	gl_FragColor = shcolor;
}