
float exp_blender(float f)
{
	return pow(2.71828182846, f);
}

float compatible_pow(float x, float y)
{
	if(y == 0.0) /* x^0 -> 1, including 0^0 */
		return 1.0;

	/* glsl pow doesn't accept negative x */
	if(x < 0.0) {
		if(mod(-y, 2.0) == 0.0)
			return pow(-x, y);
		else
			return -pow(-x, y);
	}
	else if(x == 0.0)
		return 0.0;

	return pow(x, y);
}

void rgb_to_hsv(vec4 rgb, out vec4 outcol)
{
	float cmax, cmin, h, s, v, cdelta;
	vec3 c;

	cmax = max(rgb[0], max(rgb[1], rgb[2]));
	cmin = min(rgb[0], min(rgb[1], rgb[2]));
	cdelta = cmax-cmin;

	v = cmax;
	if (cmax!=0.0)
		s = cdelta/cmax;
	else {
		s = 0.0;
		h = 0.0;
	}

	if (s == 0.0) {
		h = 0.0;
	}
	else {
		c = (vec3(cmax, cmax, cmax) - rgb.xyz)/cdelta;

		if (rgb.x==cmax) h = c[2] - c[1];
		else if (rgb.y==cmax) h = 2.0 + c[0] -  c[2];
		else h = 4.0 + c[1] - c[0];

		h /= 6.0;

		if (h<0.0)
			h += 1.0;
	}

	outcol = vec4(h, s, v, rgb.w);
}

void hsv_to_rgb(vec4 hsv, out vec4 outcol)
{
	float i, f, p, q, t, h, s, v;
	vec3 rgb;

	h = hsv[0];
	s = hsv[1];
	v = hsv[2];

	if(s==0.0) {
		rgb = vec3(v, v, v);
	}
	else {
		if(h==1.0)
			h = 0.0;
		
		h *= 6.0;
		i = floor(h);
		f = h - i;
		rgb = vec3(f, f, f);
		p = v*(1.0-s);
		q = v*(1.0-(s*f));
		t = v*(1.0-(s*(1.0-f)));
		
		if (i == 0.0) rgb = vec3(v, t, p);
		else if (i == 1.0) rgb = vec3(q, v, p);
		else if (i == 2.0) rgb = vec3(p, v, t);
		else if (i == 3.0) rgb = vec3(p, q, v);
		else if (i == 4.0) rgb = vec3(t, p, v);
		else rgb = vec3(v, p, q);
	}

	outcol = vec4(rgb, hsv.w);
}

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

#define M_PI 3.14159265358979323846
#define M_1_PI 0.31830988618379069

/*********** SHADER NODES ***************/

void vcol_attribute(vec4 attvcol, out vec4 vcol)
{
	vcol = vec4(attvcol.x/255.0, attvcol.y/255.0, attvcol.z/255.0, 1.0);
}

void uv_attribute(vec2 attuv, out vec3 uv)
{
	uv = vec3(attuv*2.0 - vec2(1.0, 1.0), 0.0);
}

void geom(vec3 co, vec3 nor, mat4 viewinvmat, vec3 attorco, vec2 attuv, vec4 attvcol, out vec3 global, out vec3 local, out vec3 view, out vec3 orco, out vec3 uv, out vec3 normal, out vec4 vcol, out float vcol_alpha, out float frontback)
{
	local = co;
	view = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(local): vec3(0.0, 0.0, -1.0);
	global = (viewinvmat*vec4(local, 1.0)).xyz;
	orco = attorco;
	uv_attribute(attuv, uv);
	normal = -normalize(nor);	/* blender render normal is negated */
	vcol_attribute(attvcol, vcol);
	srgb_to_linearrgb(vcol, vcol);
	vcol_alpha = attvcol.a;
	frontback = (gl_FrontFacing)? 1.0: 0.0;
}

void particle_info(vec4 sprops, vec3 loc, vec3 vel, vec3 avel, out float index, out float age, out float life_time, out vec3 location, out float size, out vec3 velocity, out vec3 angular_velocity)
{
    index = sprops.x;
    age = sprops.y;
    life_time = sprops.z;
    size = sprops.w;

    location = loc;
    velocity = vel;
    angular_velocity = avel;
}

void mapping(vec3 vec, mat4 mat, vec3 minvec, vec3 maxvec, float domin, float domax, out vec3 outvec)
{
	outvec = (mat * vec4(vec, 1.0)).xyz;
	if(domin == 1.0)
		outvec = max(outvec, minvec);
	if(domax == 1.0)
		outvec = min(outvec, maxvec);
}

void camera(vec3 co, out vec3 outview, out float outdepth, out float outdist)
{
	outdepth = abs(co.z);
	outdist = length(co);
	outview = normalize(co*vec3(1.0,1.0,-1.0));
}

void lamp(vec4 col, float energy, vec3 lv, float dist, vec3 shadow, float visifac, out vec4 outcol, out vec3 outlv, out float outdist, out vec4 outshadow, out float outvisifac)
{
	outcol = col * energy;
	outlv = lv;
	outdist = dist;
	outshadow = vec4(shadow, 1.0);
	outvisifac = visifac;
}

void math_add(float val1, float val2, out float outval)
{
	outval = val1 + val2;
}

void math_subtract(float val1, float val2, out float outval)
{
	outval = val1 - val2;
}

void math_multiply(float val1, float val2, out float outval)
{
	outval = val1 * val2;
}

void math_divide(float val1, float val2, out float outval)
{
	if (val2 == 0.0)
		outval = 0.0;
	else
		outval = val1 / val2;
}

void math_sine(float val, out float outval)
{
	outval = sin(val);
}

void math_cosine(float val, out float outval)
{
	outval = cos(val);
}

void math_tangent(float val, out float outval)
{
	outval = tan(val);
}

void math_asin(float val, out float outval)
{
	if (val <= 1.0 && val >= -1.0)
		outval = asin(val);
	else
		outval = 0.0;
}

void math_acos(float val, out float outval)
{
	if (val <= 1.0 && val >= -1.0)
		outval = acos(val);
	else
		outval = 0.0;
}

void math_atan(float val, out float outval)
{
	outval = atan(val);
}

void math_pow(float val1, float val2, out float outval)
{
	if (val1 >= 0.0) {
		outval = compatible_pow(val1, val2);
	}
	else {
		float val2_mod_1 = mod(abs(val2), 1.0);

		if (val2_mod_1 > 0.999 || val2_mod_1 < 0.001)
			outval = compatible_pow(val1, floor(val2 + 0.5));
		else
			outval = 0.0;
	}
}

void math_log(float val1, float val2, out float outval)
{
	if(val1 > 0.0  && val2 > 0.0)
		outval= log2(val1) / log2(val2);
	else
		outval= 0.0;
}

void math_max(float val1, float val2, out float outval)
{
	outval = max(val1, val2);
}

void math_min(float val1, float val2, out float outval)
{
	outval = min(val1, val2);
}

void math_round(float val, out float outval)
{
	outval= floor(val + 0.5);
}

void math_less_than(float val1, float val2, out float outval)
{
	if(val1 < val2)
		outval = 1.0;
	else
		outval = 0.0;
}

void math_greater_than(float val1, float val2, out float outval)
{
	if(val1 > val2)
		outval = 1.0;
	else
		outval = 0.0;
}

void math_modulo(float val1, float val2, out float outval)
{
	if (val2 == 0.0)
		outval = 0.0;
	else
		outval = mod(val1, val2);

	/* change sign to match C convention, mod in GLSL will take absolute for negative numbers,
	 * see https://www.opengl.org/sdk/docs/man/html/mod.xhtml */
	outval = (val1 > 0.0) ? outval : -outval;
}

void math_abs(float val1, out float outval)
{
    outval = abs(val1);
}

void squeeze(float val, float width, float center, out float outval)
{
	outval = 1.0/(1.0 + pow(2.71828183, -((val-center)*width)));
}

void vec_math_add(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = v1 + v2;
	outval = (abs(outvec[0]) + abs(outvec[1]) + abs(outvec[2]))/3.0;
}

void vec_math_sub(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = v1 - v2;
	outval = (abs(outvec[0]) + abs(outvec[1]) + abs(outvec[2]))/3.0;
}

void vec_math_average(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = v1 + v2;
	outval = length(outvec);
	outvec = normalize(outvec);
}

void vec_math_dot(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = vec3(0, 0, 0);
	outval = dot(v1, v2);
}

void vec_math_cross(vec3 v1, vec3 v2, out vec3 outvec, out float outval)
{
	outvec = cross(v1, v2);
	outval = length(outvec);
	outvec /= outval;
}

void vec_math_normalize(vec3 v, out vec3 outvec, out float outval)
{
	outval = length(v);
	outvec = normalize(v);
}

void vec_math_negate(vec3 v, out vec3 outv)
{
	outv = -v;
}

void normal(vec3 dir, vec3 nor, out vec3 outnor, out float outdot)
{
	outnor = nor;
	outdot = -dot(dir, nor);
}

void normal_new_shading(vec3 dir, vec3 nor, out vec3 outnor, out float outdot)
{
	outnor = normalize(nor);
	outdot = dot(normalize(dir), nor);
}

void curves_vec(float fac, vec3 vec, sampler2D curvemap, out vec3 outvec)
{
	outvec.x = texture2D(curvemap, vec2((vec.x + 1.0)*0.5, 0.0)).x;
	outvec.y = texture2D(curvemap, vec2((vec.y + 1.0)*0.5, 0.0)).y;
	outvec.z = texture2D(curvemap, vec2((vec.z + 1.0)*0.5, 0.0)).z;

	if (fac != 1.0)
		outvec = (outvec*fac) + (vec*(1.0-fac));

}

void curves_rgb(float fac, vec4 col, sampler2D curvemap, out vec4 outcol)
{
	outcol.r = texture2D(curvemap, vec2(texture2D(curvemap, vec2(col.r, 0.0)).a, 0.0)).r;
	outcol.g = texture2D(curvemap, vec2(texture2D(curvemap, vec2(col.g, 0.0)).a, 0.0)).g;
	outcol.b = texture2D(curvemap, vec2(texture2D(curvemap, vec2(col.b, 0.0)).a, 0.0)).b;

	if (fac != 1.0)
		outcol = (outcol*fac) + (col*(1.0-fac));

	outcol.a = col.a;
}

void set_value(float val, out float outval)
{
	outval = val;
}

void set_rgb(vec3 col, out vec3 outcol)
{
	outcol = col;
}

void set_rgba(vec4 col, out vec4 outcol)
{
	outcol = col;
}

void set_value_zero(out float outval)
{
	outval = 0.0;
}

void set_value_one(out float outval)
{
	outval = 1.0;
}

void set_rgb_zero(out vec3 outval)
{
	outval = vec3(0.0);
}

void set_rgb_one(out vec3 outval)
{
	outval = vec3(1.0);
}

void set_rgba_zero(out vec4 outval)
{
	outval = vec4(0.0);
}

void set_rgba_one(out vec4 outval)
{
	outval = vec4(1.0);
}

void brightness_contrast(vec4 col, float brightness, float contrast, out vec4 outcol)
{
	float a = 1.0 + contrast;
	float b = brightness - contrast*0.5;

	outcol.r = max(a*col.r + b, 0.0);
	outcol.g = max(a*col.g + b, 0.0);
	outcol.b = max(a*col.b + b, 0.0);
	outcol.a = col.a;
}

void mix_blend(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col2, fac);
	outcol.a = col1.a;
}

void mix_add(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 + col2, fac);
	outcol.a = col1.a;
}

void mix_mult(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 * col2, fac);
	outcol.a = col1.a;
}

void mix_screen(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = vec4(1.0) - (vec4(facm) + fac*(vec4(1.0) - col2))*(vec4(1.0) - col1);
	outcol.a = col1.a;
}

void mix_overlay(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	if(outcol.r < 0.5)
		outcol.r *= facm + 2.0*fac*col2.r;
	else
		outcol.r = 1.0 - (facm + 2.0*fac*(1.0 - col2.r))*(1.0 - outcol.r);

	if(outcol.g < 0.5)
		outcol.g *= facm + 2.0*fac*col2.g;
	else
		outcol.g = 1.0 - (facm + 2.0*fac*(1.0 - col2.g))*(1.0 - outcol.g);

	if(outcol.b < 0.5)
		outcol.b *= facm + 2.0*fac*col2.b;
	else
		outcol.b = 1.0 - (facm + 2.0*fac*(1.0 - col2.b))*(1.0 - outcol.b);
}

void mix_sub(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, col1 - col2, fac);
	outcol.a = col1.a;
}

void mix_div(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	if(col2.r != 0.0) outcol.r = facm*outcol.r + fac*outcol.r/col2.r;
	if(col2.g != 0.0) outcol.g = facm*outcol.g + fac*outcol.g/col2.g;
	if(col2.b != 0.0) outcol.b = facm*outcol.b + fac*outcol.b/col2.b;
}

void mix_diff(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = mix(col1, abs(col1 - col2), fac);
	outcol.a = col1.a;
}

void mix_dark(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol.rgb = min(col1.rgb, col2.rgb*fac);
	outcol.a = col1.a;
}

void mix_light(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol.rgb = max(col1.rgb, col2.rgb*fac);
	outcol.a = col1.a;
}

void mix_dodge(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	outcol = col1;

	if(outcol.r != 0.0) {
		float tmp = 1.0 - fac*col2.r;
		if(tmp <= 0.0)
			outcol.r = 1.0;
		else if((tmp = outcol.r/tmp) > 1.0)
			outcol.r = 1.0;
		else
			outcol.r = tmp;
	}
	if(outcol.g != 0.0) {
		float tmp = 1.0 - fac*col2.g;
		if(tmp <= 0.0)
			outcol.g = 1.0;
		else if((tmp = outcol.g/tmp) > 1.0)
			outcol.g = 1.0;
		else
			outcol.g = tmp;
	}
	if(outcol.b != 0.0) {
		float tmp = 1.0 - fac*col2.b;
		if(tmp <= 0.0)
			outcol.b = 1.0;
		else if((tmp = outcol.b/tmp) > 1.0)
			outcol.b = 1.0;
		else
			outcol.b = tmp;
	}
}

void mix_burn(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float tmp, facm = 1.0 - fac;

	outcol = col1;

	tmp = facm + fac*col2.r;
	if(tmp <= 0.0)
		outcol.r = 0.0;
	else if((tmp = (1.0 - (1.0 - outcol.r)/tmp)) < 0.0)
		outcol.r = 0.0;
	else if(tmp > 1.0)
		outcol.r = 1.0;
	else
		outcol.r = tmp;

	tmp = facm + fac*col2.g;
	if(tmp <= 0.0)
		outcol.g = 0.0;
	else if((tmp = (1.0 - (1.0 - outcol.g)/tmp)) < 0.0)
		outcol.g = 0.0;
	else if(tmp > 1.0)
		outcol.g = 1.0;
	else
		outcol.g = tmp;

	tmp = facm + fac*col2.b;
	if(tmp <= 0.0)
		outcol.b = 0.0;
	else if((tmp = (1.0 - (1.0 - outcol.b)/tmp)) < 0.0)
		outcol.b = 0.0;
	else if(tmp > 1.0)
		outcol.b = 1.0;
	else
		outcol.b = tmp;
}

void mix_hue(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	vec4 hsv, hsv2, tmp;
	rgb_to_hsv(col2, hsv2);

	if(hsv2.y != 0.0) {
		rgb_to_hsv(outcol, hsv);
		hsv.x = hsv2.x;
		hsv_to_rgb(hsv, tmp); 

		outcol = mix(outcol, tmp, fac);
		outcol.a = col1.a;
	}
}

void mix_sat(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	vec4 hsv, hsv2;
	rgb_to_hsv(outcol, hsv);

	if(hsv.y != 0.0) {
		rgb_to_hsv(col2, hsv2);

		hsv.y = facm*hsv.y + fac*hsv2.y;
		hsv_to_rgb(hsv, outcol);
	}
}

void mix_val(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	vec4 hsv, hsv2;
	rgb_to_hsv(col1, hsv);
	rgb_to_hsv(col2, hsv2);

	hsv.z = facm*hsv.z + fac*hsv2.z;
	hsv_to_rgb(hsv, outcol);
}

void mix_color(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	outcol = col1;

	vec4 hsv, hsv2, tmp;
	rgb_to_hsv(col2, hsv2);

	if(hsv2.y != 0.0) {
		rgb_to_hsv(outcol, hsv);
		hsv.x = hsv2.x;
		hsv.y = hsv2.y;
		hsv_to_rgb(hsv, tmp); 

		outcol = mix(outcol, tmp, fac);
		outcol.a = col1.a;
	}
}

void mix_soft(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);
	float facm = 1.0 - fac;

	vec4 one= vec4(1.0);
	vec4 scr= one - (one - col2)*(one - col1);
	outcol = facm*col1 + fac*((one - col1)*col2*col1 + col1*scr);
}

void mix_linear(float fac, vec4 col1, vec4 col2, out vec4 outcol)
{
	fac = clamp(fac, 0.0, 1.0);

	outcol = col1 + fac*(2.0*(col2 - vec4(0.5)));
}

void valtorgb(float fac, sampler2D colormap, out vec4 outcol, out float outalpha)
{
	outcol = texture2D(colormap, vec2(fac, 0.0));
	outalpha = outcol.a;
}

void rgbtobw(vec4 color, out float outval)  
{
	outval = color.r*0.35 + color.g*0.45 + color.b*0.2; /* keep these factors in sync with texture.h:RGBTOBW */
}

void invert(float fac, vec4 col, out vec4 outcol)
{
	outcol.xyz = mix(col.xyz, vec3(1.0, 1.0, 1.0) - col.xyz, fac);
	outcol.w = col.w;
}

void clamp_vec3(vec3 vec, vec3 min, vec3 max, out vec3 out_vec)
{
	out_vec = clamp(vec, min, max);
}

void clamp_val(float value, float min, float max, out float out_value)
{
	out_value = clamp(value, min, max);
}

void hue_sat(float hue, float sat, float value, float fac, vec4 col, out vec4 outcol)
{
	vec4 hsv;

	rgb_to_hsv(col, hsv);

	hsv[0] += (hue - 0.5);
	if(hsv[0]>1.0) hsv[0]-=1.0; else if(hsv[0]<0.0) hsv[0]+= 1.0;
	hsv[1] *= sat;
	if(hsv[1]>1.0) hsv[1]= 1.0; else if(hsv[1]<0.0) hsv[1]= 0.0;
	hsv[2] *= value;
	if(hsv[2]>1.0) hsv[2]= 1.0; else if(hsv[2]<0.0) hsv[2]= 0.0;

	hsv_to_rgb(hsv, outcol);

	outcol = mix(col, outcol, fac);
}

void separate_rgb(vec4 col, out float r, out float g, out float b)
{
	r = col.r;
	g = col.g;
	b = col.b;
}

void combine_rgb(float r, float g, float b, out vec4 col)
{
	col = vec4(r, g, b, 1.0);
}

void separate_xyz(vec3 vec, out float x, out float y, out float z)
{
	x = vec.r;
	y = vec.g;
	z = vec.b;
}

void combine_xyz(float x, float y, float z, out vec3 vec)
{
	vec = vec3(x, y, z);
}

void separate_hsv(vec4 col, out float h, out float s, out float v)
{
	vec4 hsv;

	rgb_to_hsv(col, hsv);
	h = hsv[0];
	s = hsv[1];
	v = hsv[2];
}

void combine_hsv(float h, float s, float v, out vec4 col)
{
	hsv_to_rgb(vec4(h, s, v, 1.0), col);
}

void output_node(vec4 rgb, float alpha, out vec4 outrgb)
{
	outrgb = vec4(rgb.rgb, alpha);
}

/*********** TEXTURES ***************/

void texture_flip_blend(vec3 vec, out vec3 outvec)
{
	outvec = vec.yxz;
}

void texture_blend_lin(vec3 vec, out float outval)
{
	outval = (1.0+vec.x)/2.0;
}

void texture_blend_quad(vec3 vec, out float outval)
{
	outval = max((1.0+vec.x)/2.0, 0.0);
	outval *= outval;
}

void texture_wood_sin(vec3 vec, out float value, out vec4 color, out vec3 normal)
{
	float a = sqrt(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z)*20.0;
	float wi = 0.5 + 0.5*sin(a);

	value = wi;
	color = vec4(wi, wi, wi, 1.0);
	normal = vec3(0.0, 0.0, 0.0);
}

void texture_image(vec3 vec, sampler2D ima, out float value, out vec4 color, out vec3 normal)
{
	color = texture2D(ima, (vec.xy + vec2(1.0, 1.0))*0.5);
	value = color.a;

	normal.x = 2.0*(color.r - 0.5);
	normal.y = 2.0*(0.5 - color.g);
	normal.z = 2.0*(color.b - 0.5);
}

/************* MTEX *****************/

void texco_orco(vec3 attorco, out vec3 orco)
{
	orco = attorco;
}

void texco_uv(vec2 attuv, out vec3 uv)
{
	/* disabled for now, works together with leaving out mtex_2d_mapping
	   uv = vec3(attuv*2.0 - vec2(1.0, 1.0), 0.0); */
	uv = vec3(attuv, 0.0);
}

void texco_norm(vec3 normal, out vec3 outnormal)
{
	/* corresponds to shi->orn, which is negated so cancels
	   out blender normal negation */
	outnormal = normalize(normal);
}

void texco_tangent(vec4 tangent, out vec3 outtangent)
{
	outtangent = normalize(tangent.xyz);
}

void texco_global(mat4 viewinvmat, vec3 co, out vec3 global)
{
	global = (viewinvmat*vec4(co, 1.0)).xyz;
}

void texco_object(mat4 viewinvmat, mat4 obinvmat, vec3 co, out vec3 object)
{
	object = (obinvmat*(viewinvmat*vec4(co, 1.0))).xyz;
}

void texco_refl(vec3 vn, vec3 view, out vec3 ref)
{
	ref = view - 2.0*dot(vn, view)*vn;
}

void shade_norm(vec3 normal, out vec3 outnormal)
{
	/* blender render normal is negated */
	outnormal = -normalize(normal);
}

void mtex_rgb_blend(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	incol = fact*texcol + facm*outcol;
}

void mtex_rgb_mul(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	incol = (facm + fact*texcol)*outcol;
}

void mtex_rgb_screen(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	incol = vec3(1.0) - (vec3(facm) + fact*(vec3(1.0) - texcol))*(vec3(1.0) - outcol);
}

void mtex_rgb_overlay(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	if(outcol.r < 0.5)
		incol.r = outcol.r*(facm + 2.0*fact*texcol.r);
	else
		incol.r = 1.0 - (facm + 2.0*fact*(1.0 - texcol.r))*(1.0 - outcol.r);

	if(outcol.g < 0.5)
		incol.g = outcol.g*(facm + 2.0*fact*texcol.g);
	else
		incol.g = 1.0 - (facm + 2.0*fact*(1.0 - texcol.g))*(1.0 - outcol.g);

	if(outcol.b < 0.5)
		incol.b = outcol.b*(facm + 2.0*fact*texcol.b);
	else
		incol.b = 1.0 - (facm + 2.0*fact*(1.0 - texcol.b))*(1.0 - outcol.b);
}

void mtex_rgb_sub(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	incol = -fact*facg*texcol + outcol;
}

void mtex_rgb_add(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	incol = fact*facg*texcol + outcol;
}

void mtex_rgb_div(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	if(texcol.r != 0.0) incol.r = facm*outcol.r + fact*outcol.r/texcol.r;
	if(texcol.g != 0.0) incol.g = facm*outcol.g + fact*outcol.g/texcol.g;
	if(texcol.b != 0.0) incol.b = facm*outcol.b + fact*outcol.b/texcol.b;
}

void mtex_rgb_diff(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	incol = facm*outcol + fact*abs(texcol - outcol);
}

void mtex_rgb_dark(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm, col;

	fact *= facg;
	facm = 1.0-fact;

	incol.r = min(outcol.r, texcol.r) * fact + outcol.r * facm;
	incol.g = min(outcol.g, texcol.g) * fact + outcol.g * facm;
	incol.b = min(outcol.b, texcol.b) * fact + outcol.b * facm;
}

void mtex_rgb_light(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm, col;

	fact *= facg;

	col = fact*texcol.r;
	if(col > outcol.r) incol.r = col; else incol.r = outcol.r;
	col = fact*texcol.g;
	if(col > outcol.g) incol.g = col; else incol.g = outcol.g;
	col = fact*texcol.b;
	if(col > outcol.b) incol.b = col; else incol.b = outcol.b;
}

void mtex_rgb_hue(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	vec4 col;

	mix_hue(fact*facg, vec4(outcol, 1.0), vec4(texcol, 1.0), col);
	incol.rgb = col.rgb;
}

void mtex_rgb_sat(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	vec4 col;

	mix_sat(fact*facg, vec4(outcol, 1.0), vec4(texcol, 1.0), col);
	incol.rgb = col.rgb;
}

void mtex_rgb_val(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	vec4 col;

	mix_val(fact*facg, vec4(outcol, 1.0), vec4(texcol, 1.0), col);
	incol.rgb = col.rgb;
}

void mtex_rgb_color(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	vec4 col;

	mix_color(fact*facg, vec4(outcol, 1.0), vec4(texcol, 1.0), col);
	incol.rgb = col.rgb;
}

void mtex_rgb_soft(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	float facm;

	fact *= facg;
	facm = 1.0-fact;

	vec3 one = vec3(1.0);
	vec3 scr = one - (one - texcol)*(one - outcol);
	incol = facm*outcol + fact*((one - texcol)*outcol*texcol + outcol*scr);
}

void mtex_rgb_linear(vec3 outcol, vec3 texcol, float fact, float facg, out vec3 incol)
{
	fact *= facg;

	if(texcol.r > 0.5)
		incol.r = outcol.r + fact*(2.0*(texcol.r - 0.5));
	else
		incol.r = outcol.r + fact*(2.0*(texcol.r) - 1.0);

	if(texcol.g > 0.5)
		incol.g = outcol.g + fact*(2.0*(texcol.g - 0.5));
	else
		incol.g = outcol.g + fact*(2.0*(texcol.g) - 1.0);

	if(texcol.b > 0.5)
		incol.b = outcol.b + fact*(2.0*(texcol.b - 0.5));
	else
		incol.b = outcol.b + fact*(2.0*(texcol.b) - 1.0);
}

void mtex_value_vars(inout float fact, float facg, out float facm)
{
	fact *= abs(facg);
	facm = 1.0-fact;

	if(facg < 0.0) {
		float tmp = fact;
		fact = facm;
		facm = tmp;
	}
}

void mtex_value_blend(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	incol = fact*texcol + facm*outcol;
}

void mtex_value_mul(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	facm = 1.0 - facg;
	incol = (facm + fact*texcol)*outcol;
}

void mtex_value_screen(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	facm = 1.0 - facg;
	incol = 1.0 - (facm + fact*(1.0 - texcol))*(1.0 - outcol);
}

void mtex_value_sub(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	fact = -fact;
	incol = fact*texcol + outcol;
}

void mtex_value_add(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	fact = fact;
	incol = fact*texcol + outcol;
}

void mtex_value_div(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	if(texcol != 0.0)
		incol = facm*outcol + fact*outcol/texcol;
	else
		incol = 0.0;
}

void mtex_value_diff(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	incol = facm*outcol + fact*abs(texcol - outcol);
}

void mtex_value_dark(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	incol = facm*outcol + fact*min(outcol, texcol);
}

void mtex_value_light(float outcol, float texcol, float fact, float facg, out float incol)
{
	float facm;
	mtex_value_vars(fact, facg, facm);

	float col = fact*texcol;
	if(col > outcol) incol = col; else incol = outcol;
}

void mtex_value_clamp_positive(float fac, out float outfac)
{
	outfac = max(fac, 0.0);
}

void mtex_value_clamp(float fac, out float outfac)
{
	outfac = clamp(fac, 0.0, 1.0);
}

void mtex_har_divide(float har, out float outhar)
{
	outhar = har/128.0;
}

void mtex_har_multiply_clamp(float har, out float outhar)
{
	har *= 128.0;

	if(har < 1.0) outhar = 1.0;
	else if(har > 511.0) outhar = 511.0;
	else outhar = har;
}

void mtex_alpha_from_col(vec4 col, out float alpha)
{
	alpha = col.a;
}

void mtex_alpha_to_col(vec4 col, float alpha, out vec4 outcol)
{
	outcol = vec4(col.rgb, alpha);
}

void mtex_rgbtoint(vec4 rgb, out float intensity)
{
	intensity = dot(vec3(0.35, 0.45, 0.2), rgb.rgb);
}

void mtex_value_invert(float invalue, out float outvalue)
{
	outvalue = 1.0 - invalue;
}

void mtex_rgb_invert(vec4 inrgb, out vec4 outrgb)
{
	outrgb = vec4(vec3(1.0) - inrgb.rgb, inrgb.a);
}

void mtex_value_stencil(float stencil, float intensity, out float outstencil, out float outintensity)
{
	float fact = intensity;
	outintensity = intensity*stencil;
	outstencil = stencil*fact;
}

void mtex_rgb_stencil(float stencil, vec4 rgb, out float outstencil, out vec4 outrgb)
{
	float fact = rgb.a;
	outrgb = vec4(rgb.rgb, rgb.a*stencil);
	outstencil = stencil*fact;
}

void mtex_mapping_ofs(vec3 texco, vec3 ofs, out vec3 outtexco)
{
	outtexco = texco + ofs;
}

void mtex_mapping_size(vec3 texco, vec3 size, out vec3 outtexco)
{
	outtexco = size*texco;
}

void mtex_2d_mapping(vec3 vec, out vec3 outvec)
{
	outvec = vec3(vec.xy*0.5 + vec2(0.5), vec.z);
}

vec3 mtex_2d_mapping(vec3 vec)
{
	return vec3(vec.xy*0.5 + vec2(0.5), vec.z);
}

void mtex_image(vec3 texco, sampler2D ima, out float value, out vec4 color)
{
	color = texture2D(ima, texco.xy);
	value = 1.0;
}

void mtex_normal(vec3 texco, sampler2D ima, out vec3 normal)
{
	// The invert of the red channel is to make
	// the normal map compliant with the outside world.
	// It needs to be done because in Blender
	// the normal used points inward.
	// Should this ever change this negate must be removed.
	vec4 color = texture2D(ima, texco.xy);
	normal = 2.0*(vec3(-color.r, color.g, color.b) - vec3(-0.5, 0.5, 0.5));
}

void mtex_bump_normals_init( vec3 vN, out vec3 vNorg, out vec3 vNacc, out float fPrevMagnitude )
{
	vNorg = vN;
	vNacc = vN;
	fPrevMagnitude = 1.0;
}

/** helper method to extract the upper left 3x3 matrix from a 4x4 matrix */
mat3 to_mat3(mat4 m4)
{
	mat3 m3;
	m3[0] = m4[0].xyz;
	m3[1] = m4[1].xyz;
	m3[2] = m4[2].xyz;
	return m3;
}

void mtex_bump_init_objspace( vec3 surf_pos, vec3 surf_norm,
							  mat4 mView, mat4 mViewInv, mat4 mObj, mat4 mObjInv, 
							  float fPrevMagnitude_in, vec3 vNacc_in,
							  out float fPrevMagnitude_out, out vec3 vNacc_out, 
							  out vec3 vR1, out vec3 vR2, out float fDet ) 
{
	mat3 obj2view = to_mat3(gl_ModelViewMatrix);
	mat3 view2obj = to_mat3(gl_ModelViewMatrixInverse);
	
	vec3 vSigmaS = view2obj * dFdx( surf_pos );
	vec3 vSigmaT = view2obj * dFdy( surf_pos );
	vec3 vN = normalize( surf_norm * obj2view );

	vR1 = cross( vSigmaT, vN );
	vR2 = cross( vN, vSigmaS ) ;
	fDet = dot ( vSigmaS, vR1 );
	
	/* pretransform vNacc (in mtex_bump_apply) using the inverse transposed */
	vR1 = vR1 * view2obj;
	vR2 = vR2 * view2obj;
	vN = vN * view2obj;
	
	float fMagnitude = abs(fDet) * length(vN);
	vNacc_out = vNacc_in * (fMagnitude / fPrevMagnitude_in);
	fPrevMagnitude_out = fMagnitude;
}

void mtex_bump_init_texturespace( vec3 surf_pos, vec3 surf_norm, 
								  float fPrevMagnitude_in, vec3 vNacc_in,
								  out float fPrevMagnitude_out, out vec3 vNacc_out, 
								  out vec3 vR1, out vec3 vR2, out float fDet ) 
{
	vec3 vSigmaS = dFdx( surf_pos );
	vec3 vSigmaT = dFdy( surf_pos );
	vec3 vN = surf_norm; /* normalized interpolated vertex normal */
	
	vR1 = normalize( cross( vSigmaT, vN ) );
	vR2 = normalize( cross( vN, vSigmaS ) );
	fDet = sign( dot(vSigmaS, vR1) );
	
	float fMagnitude = abs(fDet);
	vNacc_out = vNacc_in * (fMagnitude / fPrevMagnitude_in);
	fPrevMagnitude_out = fMagnitude;
}

void mtex_bump_init_viewspace( vec3 surf_pos, vec3 surf_norm, 
							   float fPrevMagnitude_in, vec3 vNacc_in,
							   out float fPrevMagnitude_out, out vec3 vNacc_out, 
							   out vec3 vR1, out vec3 vR2, out float fDet ) 
{
	vec3 vSigmaS = dFdx( surf_pos );
	vec3 vSigmaT = dFdy( surf_pos );
	vec3 vN = surf_norm; /* normalized interpolated vertex normal */
	
	vR1 = cross( vSigmaT, vN );
	vR2 = cross( vN, vSigmaS ) ;
	fDet = dot ( vSigmaS, vR1 );
	
	float fMagnitude = abs(fDet);
	vNacc_out = vNacc_in * (fMagnitude / fPrevMagnitude_in);
	fPrevMagnitude_out = fMagnitude;
}

void mtex_bump_tap3( vec3 texco, sampler2D ima, float hScale, 
                     out float dBs, out float dBt )
{
	vec2 STll = texco.xy;
	vec2 STlr = texco.xy + dFdx(texco.xy) ;
	vec2 STul = texco.xy + dFdy(texco.xy) ;
	
	float Hll,Hlr,Hul;
	rgbtobw( texture2D(ima, STll), Hll );
	rgbtobw( texture2D(ima, STlr), Hlr );
	rgbtobw( texture2D(ima, STul), Hul );
	
	dBs = hScale * (Hlr - Hll);
	dBt = hScale * (Hul - Hll);
}

#ifdef BUMP_BICUBIC

void mtex_bump_bicubic( vec3 texco, sampler2D ima, float hScale, 
                     out float dBs, out float dBt ) 
{
	float Hl;
	float Hr;
	float Hd;
	float Hu;
	
	vec2 TexDx = dFdx(texco.xy);
	vec2 TexDy = dFdy(texco.xy);
 
	vec2 STl = texco.xy - 0.5 * TexDx ;
	vec2 STr = texco.xy + 0.5 * TexDx ;
	vec2 STd = texco.xy - 0.5 * TexDy ;
	vec2 STu = texco.xy + 0.5 * TexDy ;
	
	rgbtobw(texture2D(ima, STl), Hl);
	rgbtobw(texture2D(ima, STr), Hr);
	rgbtobw(texture2D(ima, STd), Hd);
	rgbtobw(texture2D(ima, STu), Hu);
	
	vec2 dHdxy = vec2(Hr - Hl, Hu - Hd);
	float fBlend = clamp(1.0-textureQueryLOD(ima, texco.xy).x, 0.0, 1.0);
	if(fBlend!=0.0)
	{
		// the derivative of the bicubic sampling of level 0
		ivec2 vDim;
		vDim = textureSize(ima, 0);

		// taking the fract part of the texture coordinate is a hardcoded wrap mode.
		// this is acceptable as textures use wrap mode exclusively in 3D view elsewhere in blender. 
		// this is done so that we can still get a valid texel with uvs outside the 0,1 range
		// by texelFetch below, as coordinates are clamped when using this function.
		vec2 fTexLoc = vDim*fract(texco.xy) - vec2(0.5, 0.5);
		ivec2 iTexLoc = ivec2(floor(fTexLoc));
		vec2 t = clamp(fTexLoc - iTexLoc, 0.0, 1.0);		// sat just to be pedantic

/*******************************************************************************************
 * This block will replace the one below when one channel textures are properly supported. *
 *******************************************************************************************
		vec4 vSamplesUL = textureGather(ima, (iTexLoc+ivec2(-1,-1) + vec2(0.5,0.5))/vDim );
		vec4 vSamplesUR = textureGather(ima, (iTexLoc+ivec2(1,-1) + vec2(0.5,0.5))/vDim );
		vec4 vSamplesLL = textureGather(ima, (iTexLoc+ivec2(-1,1) + vec2(0.5,0.5))/vDim );
		vec4 vSamplesLR = textureGather(ima, (iTexLoc+ivec2(1,1) + vec2(0.5,0.5))/vDim );

		mat4 H = mat4(vSamplesUL.w, vSamplesUL.x, vSamplesLL.w, vSamplesLL.x,
					vSamplesUL.z, vSamplesUL.y, vSamplesLL.z, vSamplesLL.y,
					vSamplesUR.w, vSamplesUR.x, vSamplesLR.w, vSamplesLR.x,
					vSamplesUR.z, vSamplesUR.y, vSamplesLR.z, vSamplesLR.y);
*/	
		ivec2 iTexLocMod = iTexLoc + ivec2(-1, -1);

		mat4 H;
		
		for(int i = 0; i < 4; i++) {
			for(int j = 0; j < 4; j++) {
				ivec2 iTexTmp = iTexLocMod + ivec2(i,j);
				
				// wrap texture coordinates manually for texelFetch to work on uvs oitside the 0,1 range.
				// this is guaranteed to work since we take the fractional part of the uv above.
				iTexTmp.x = (iTexTmp.x < 0)? iTexTmp.x + vDim.x : ((iTexTmp.x >= vDim.x)? iTexTmp.x - vDim.x : iTexTmp.x);
				iTexTmp.y = (iTexTmp.y < 0)? iTexTmp.y + vDim.y : ((iTexTmp.y >= vDim.y)? iTexTmp.y - vDim.y : iTexTmp.y);

				rgbtobw(texelFetch(ima, iTexTmp, 0), H[i][j]);
			}
		}
		
		float x = t.x, y = t.y;
		float x2 = x * x, x3 = x2 * x, y2 = y * y, y3 = y2 * y;

		vec4 X = vec4(-0.5*(x3+x)+x2,		1.5*x3-2.5*x2+1,	-1.5*x3+2*x2+0.5*x,		0.5*(x3-x2));
		vec4 Y = vec4(-0.5*(y3+y)+y2,		1.5*y3-2.5*y2+1,	-1.5*y3+2*y2+0.5*y,		0.5*(y3-y2));
		vec4 dX = vec4(-1.5*x2+2*x-0.5,		4.5*x2-5*x,			-4.5*x2+4*x+0.5,		1.5*x2-x);
		vec4 dY = vec4(-1.5*y2+2*y-0.5,		4.5*y2-5*y,			-4.5*y2+4*y+0.5,		1.5*y2-y);
	
		// complete derivative in normalized coordinates (mul by vDim)
		vec2 dHdST = vDim * vec2(dot(Y, H * dX), dot(dY, H * X));

		// transform derivative to screen-space
		vec2 dHdxy_bicubic = vec2( dHdST.x * TexDx.x + dHdST.y * TexDx.y,
								   dHdST.x * TexDy.x + dHdST.y * TexDy.y );

		// blend between the two
		dHdxy = dHdxy*(1-fBlend) + dHdxy_bicubic*fBlend;
	}

	dBs = hScale * dHdxy.x;
	dBt = hScale * dHdxy.y;
}

#endif

void mtex_bump_tap5( vec3 texco, sampler2D ima, float hScale, 
                     out float dBs, out float dBt ) 
{
	vec2 TexDx = dFdx(texco.xy);
	vec2 TexDy = dFdy(texco.xy);

	vec2 STc = texco.xy;
	vec2 STl = texco.xy - 0.5 * TexDx ;
	vec2 STr = texco.xy + 0.5 * TexDx ;
	vec2 STd = texco.xy - 0.5 * TexDy ;
	vec2 STu = texco.xy + 0.5 * TexDy ;
	
	float Hc,Hl,Hr,Hd,Hu;
	rgbtobw( texture2D(ima, STc), Hc );
	rgbtobw( texture2D(ima, STl), Hl );
	rgbtobw( texture2D(ima, STr), Hr );
	rgbtobw( texture2D(ima, STd), Hd );
	rgbtobw( texture2D(ima, STu), Hu );
	
	dBs = hScale * (Hr - Hl);
	dBt = hScale * (Hu - Hd);
}

void mtex_bump_deriv( vec3 texco, sampler2D ima, float ima_x, float ima_y, float hScale, 
                     out float dBs, out float dBt ) 
{
	float s = 1.0;		// negate this if flipped texture coordinate
	vec2 TexDx = dFdx(texco.xy);
	vec2 TexDy = dFdy(texco.xy);
	
	// this variant using a derivative map is described here
	// http://mmikkelsen3d.blogspot.com/2011/07/derivative-maps.html
	vec2 dim = vec2(ima_x, ima_y);
	vec2 dBduv = hScale*dim*(2.0*texture2D(ima, texco.xy).xy-1.0);
	
	dBs = dBduv.x*TexDx.x + s*dBduv.y*TexDx.y;
	dBt = dBduv.x*TexDy.x + s*dBduv.y*TexDy.y;
}

void mtex_bump_apply( float fDet, float dBs, float dBt, vec3 vR1, vec3 vR2, vec3 vNacc_in,
					  out vec3 vNacc_out, out vec3 perturbed_norm ) 
{
	vec3 vSurfGrad = sign(fDet) * ( dBs * vR1 + dBt * vR2 );
	
	vNacc_out = vNacc_in - vSurfGrad;
	perturbed_norm = normalize( vNacc_out );
}

void mtex_bump_apply_texspace( float fDet, float dBs, float dBt, vec3 vR1, vec3 vR2,
                               sampler2D ima, vec3 texco, float ima_x, float ima_y, vec3 vNacc_in,
							   out vec3 vNacc_out, out vec3 perturbed_norm ) 
{
	vec2 TexDx = dFdx(texco.xy);
	vec2 TexDy = dFdy(texco.xy);

	vec3 vSurfGrad = sign(fDet) * ( 
	            dBs / length( vec2(ima_x*TexDx.x, ima_y*TexDx.y) ) * vR1 + 
	            dBt / length( vec2(ima_x*TexDy.x, ima_y*TexDy.y) ) * vR2 );
				
	vNacc_out = vNacc_in - vSurfGrad;
	perturbed_norm = normalize( vNacc_out );
}

void mtex_negate_texnormal(vec3 normal, out vec3 outnormal)
{
	outnormal = vec3(-normal.x, -normal.y, normal.z);
}

void mtex_nspace_tangent(vec4 tangent, vec3 normal, vec3 texnormal, out vec3 outnormal)
{
	vec3 B = tangent.w * cross(normal, tangent.xyz);

	outnormal = texnormal.x*tangent.xyz + texnormal.y*B + texnormal.z*normal;
	outnormal = normalize(outnormal);
}

void mtex_nspace_world(mat4 viewmat, vec3 texnormal, out vec3 outnormal)
{
	outnormal = normalize((viewmat*vec4(texnormal, 0.0)).xyz);
}

void mtex_nspace_object(vec3 texnormal, out vec3 outnormal)
{
	outnormal = normalize(gl_NormalMatrix * texnormal);
}

void mtex_blend_normal(float norfac, vec3 normal, vec3 newnormal, out vec3 outnormal)
{
	outnormal = (1.0 - norfac)*normal + norfac*newnormal;
	outnormal = normalize(outnormal);
}

/******* MATERIAL *********/

void lamp_visibility_sun_hemi(vec3 lampvec, out vec3 lv, out float dist, out float visifac)
{
	lv = lampvec;
	dist = 1.0;
	visifac = 1.0;
}

void lamp_visibility_other(vec3 co, vec3 lampco, out vec3 lv, out float dist, out float visifac)
{
	lv = co - lampco;
	dist = length(lv);
	lv = normalize(lv);
	visifac = 1.0;
}

void lamp_falloff_realistic(float lampdist, float dist, out float visifac)
{
	// visifac = 1.0/(dist*dist + 1e-8);
	visifac = 1.0;
}

void lamp_falloff_invlinear(float lampdist, float dist, out float visifac)
{
	visifac = lampdist/(lampdist + dist);
}

void lamp_falloff_invsquare(float lampdist, float dist, out float visifac)
{
	visifac = lampdist/(lampdist + dist*dist);
}

void lamp_falloff_sliders(float lampdist, float ld1, float ld2, float dist, out float visifac)
{
	float lampdistkw = lampdist*lampdist;

	visifac = lampdist/(lampdist + ld1*dist);
	visifac *= lampdistkw/(lampdistkw + ld2*dist*dist);
}

void lamp_falloff_curve(float lampdist, sampler2D curvemap, float dist, out float visifac)
{
	visifac = texture2D(curvemap, vec2(dist/lampdist, 0.0)).x;
}

void lamp_visibility_sphere(float lampdist, float dist, float visifac, out float outvisifac)
{
	float t= lampdist - dist;

	outvisifac= visifac*max(t, 0.0)/lampdist;
}

void lamp_visibility_spot_square(vec3 lampvec, mat4 lampimat, vec2 scale, vec3 lv, out float inpr)
{
	if(dot(lv, lampvec) > 0.0) {
		vec3 lvrot = (lampimat*vec4(lv, 0.0)).xyz;
		/* without clever non-uniform scale, we could do: */
		// float x = max(abs(lvrot.x / lvrot.z), abs(lvrot.y / lvrot.z));
		float x = max(abs((lvrot.x / scale.x) / lvrot.z), abs((lvrot.y / scale.y) / lvrot.z));

		inpr = 1.0/sqrt(1.0 + x*x);
	}
	else
		inpr = 0.0;
}

void lamp_visibility_spot_circle(vec3 lampvec, mat4 lampimat, vec2 scale, vec3 lv, out float inpr)
{
	/* without clever non-uniform scale, we could do: */
	// inpr = dot(lv, lampvec);
	if (dot(lv, lampvec) > 0.0) {
		vec3 lvrot = (lampimat * vec4(lv, 0.0)).xyz;
		float x = abs(lvrot.x / lvrot.z);
		float y = abs(lvrot.y / lvrot.z);

		float ellipse = abs((x * x) / (scale.x * scale.x) + (y * y) / (scale.y * scale.y));

		inpr = 1.0 / sqrt(1.0 + ellipse);
	}
	else
		inpr = 0.0;
}

void lamp_visibility_spot(float spotsi, float spotbl, float inpr, float visifac, out float outvisifac)
{
	float t = spotsi;

	if(inpr <= t) {
		outvisifac = 0.0;
	}
	else {
		t = inpr - t;

		/* soft area */
		if(spotbl != 0.0)
			inpr *= smoothstep(0.0, 1.0, t/spotbl);

		outvisifac = visifac*inpr;
	}
}

void lamp_visibility_clamp(float visifac, out float outvisifac)
{
	outvisifac = (visifac < 0.001)? 0.0: visifac;
}

void shade_view(vec3 co, out vec3 view)
{
	/* handle perspective/orthographic */
	view = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(co): vec3(0.0, 0.0, -1.0);
}

void shade_tangent_v(vec3 lv, vec3 tang, out vec3 vn)
{
	vec3 c = cross(lv, tang);
	vec3 vnor = cross(c, tang);

	vn = -normalize(vnor);
}

void shade_inp(vec3 vn, vec3 lv, out float inp)
{
	inp = dot(vn, lv);
}

void shade_is_no_diffuse(out float is)
{
	is = 0.0;
}

void shade_is_hemi(float inp, out float is)
{
	is = 0.5*inp + 0.5;
}

float rectangleSolidAngle(vec3 worldPos ,vec3 p0 ,vec3 p1 ,vec3 p2 ,vec3 p3 )
{
	vec3 v0 = p0 - worldPos ;
	vec3 v1 = p1 - worldPos ;
	vec3 v2 = p2 - worldPos ;
	vec3 v3 = p3 - worldPos ;

	vec3 n0 = normalize ( cross (v0 , v1 ));
	vec3 n1 = normalize ( cross (v1 , v2 ));
	vec3 n2 = normalize ( cross (v2 , v3 ));
	vec3 n3 = normalize ( cross (v3 , v0 ));

	float g0 = acos ( dot (-n0 , n1 ));
	float g1 = acos ( dot (-n1 , n2 ));
	float g2 = acos ( dot (-n2 , n3 ));
	float g3 = acos ( dot (-n3 , n0 ));

	return g0 + g1 + g2 + g3 - 2.0 * M_PI;
}

float area_lamp_energy(mat4 lampmat, vec3 co, vec3 n, vec3 lampco, vec2 areasize)
{
	n = -n;

	vec3 right = normalize(vec3(lampmat*vec4(1.0,0.0,0.0,0.0))); //lamp right axis
	vec3 up = normalize(vec3(lampmat*vec4(0.0,1.0,0.0,0.0))); //lamp up axis
	vec3 lampv = normalize(vec3(lampmat*vec4(0.0,0.0,-1.0,0.0)));; //lamp projection axis

	float illuminance = 0.0;

	if ( dot ( co - lampco , lampv ) > 0.0 )
	{
		float width = areasize.x / 2.0;
		float height = areasize.y / 2.0;

		vec3 p0 = lampco + right * -width + up *  height;
		vec3 p1 = lampco + right * -width + up * -height;
		vec3 p2 = lampco + right *  width + up * -height;
		vec3 p3 = lampco + right *  width + up *  height;

		float solidAngle = rectangleSolidAngle ( co , p0 , p1 , p2 , p3 );

		illuminance = solidAngle * 0.2 * (
		max(0.0, dot( normalize ( p0 - co ) , n ) )+
		max(0.0, dot( normalize ( p1 - co ) , n ) )+
		max(0.0, dot( normalize ( p2 - co ) , n ) )+
		max(0.0, dot( normalize ( p3 - co ) , n ) )+
		max(0.0, dot( normalize ( lampco - co ) , n )));
	}

	float normalization = 1 / (areasize.x * areasize.y);

	return illuminance * normalization * M_PI;
}

void shade_inp_area(vec3 co, vec3 lampco, vec3 lampvec, vec3 vn, mat4 lampmat, float areasize, float areasizey, out float inp)
{
	vec2 asize = vec2(areasize,areasizey);
	inp = area_lamp_energy(lampmat, co, vn, lampco, asize);
}

void shade_diffuse_oren_nayer(float nl, vec3 n, vec3 l, vec3 v, float rough, out float is)
{
	vec3 h = normalize(v + l);
	float nh = max(dot(n, h), 0.0);
	float nv = max(dot(n, v), 0.0);
	float realnl = dot(n, l);

	if(realnl < 0.0) {
		is = 0.0;
	}
	else if(nl < 0.0) {
		is = 0.0;
	}
	else {
		float vh = max(dot(v, h), 0.0);
		float Lit_A = acos(realnl);
		float View_A = acos(nv);

		vec3 Lit_B = normalize(l - realnl*n);
		vec3 View_B = normalize(v - nv*n);

		float t = max(dot(Lit_B, View_B), 0.0);

		float a, b;

		if(Lit_A > View_A) {
			a = Lit_A;
			b = View_A;
		}
		else {
			a = View_A;
			b = Lit_A;
		}

		float A = 1.0 - (0.5*((rough*rough)/((rough*rough) + 0.33)));
		float B = 0.45*((rough*rough)/((rough*rough) + 0.09));

		b *= 0.95;
		is = nl*(A + (B * t * sin(a) * tan(b)));
	}
}

void shade_diffuse_toon(vec3 n, vec3 l, vec3 v, float size, float tsmooth, out float is)
{
	float rslt = dot(n, l);
	float ang = acos(rslt);

	if(ang < size) is = 1.0;
	else if(ang > (size + tsmooth) || tsmooth == 0.0) is = 0.0;
	else is = 1.0 - ((ang - size)/tsmooth);
}

void shade_diffuse_minnaert(float nl, vec3 n, vec3 v, float darkness, out float is)
{
	if(nl <= 0.0) {
		is = 0.0;
	}
	else {
		float nv = max(dot(n, v), 0.0);

		if(darkness <= 1.0)
			is = nl*pow(max(nv*nl, 0.1), darkness - 1.0);
		else
			is = nl*pow(1.0001 - nv, darkness - 1.0);
	}
}

float fresnel_fac(vec3 view, vec3 vn, float grad, float fac)
{
	float t1, t2;
	float ffac;

	if(fac==0.0) {
		ffac = 1.0;
	}
	else {
		t1= dot(view, vn);
		if(t1>0.0)  t2= 1.0+t1;
		else t2= 1.0-t1;

		t2= grad + (1.0-grad)*pow(t2, fac);

		if(t2<0.0) ffac = 0.0;
		else if(t2>1.0) ffac = 1.0;
		else ffac = t2;
	}

	return ffac;
}

void shade_diffuse_fresnel(vec3 vn, vec3 lv, vec3 view, float fac_i, float fac, out float is)
{
	is = fresnel_fac(lv, vn, fac_i, fac);
}

void shade_cubic(float is, out float outis)
{
	if(is>0.0 && is<1.0)
		outis= smoothstep(0.0, 1.0, is);
	else
		outis= is;
}

void shade_visifac(float i, float visifac, float refl, out float outi)
{
	/*if(i > 0.0)*/
		outi = max(i*visifac*refl, 0.0);
	/*else
		outi = i;*/
}

void shade_tangent_v_spec(vec3 tang, out vec3 vn)
{
	vn = tang;
}

void shade_add_to_diffuse(float i, vec3 lampcol, vec3 col, out vec3 outcol)
{
	if(i > 0.0)
		outcol = i*lampcol*col;
	else
		outcol = vec3(0.0, 0.0, 0.0);
}

void shade_hemi_spec(vec3 vn, vec3 lv, vec3 view, float spec, float hard, float visifac, out float t)
{
	lv += view;
	lv = normalize(lv);

	t = dot(vn, lv);
	t = 0.5*t + 0.5;

	t = visifac*spec*pow(t, hard);
}

void shade_phong_spec(vec3 n, vec3 l, vec3 v, float hard, out float specfac)
{
	vec3 h = normalize(l + v);
	float rslt = max(dot(h, n), 0.0);

	specfac = pow(rslt, hard);
}

void shade_cooktorr_spec(vec3 n, vec3 l, vec3 v, float hard, out float specfac)
{
	vec3 h = normalize(v + l);
	float nh = dot(n, h);

	if(nh < 0.0) {
		specfac = 0.0;
	}
	else {
		float nv = max(dot(n, v), 0.0);
		float i = pow(nh, hard);

		i = i/(0.1+nv);
		specfac = i;
	}
}

void shade_blinn_spec(vec3 n, vec3 l, vec3 v, float refrac, float spec_power, out float specfac)
{
	if(refrac < 1.0) {
		specfac = 0.0;
	}
	else if(spec_power == 0.0) {
		specfac = 0.0;
	}
	else {
		if(spec_power<100.0)
			spec_power= sqrt(1.0/spec_power);
		else
			spec_power= 10.0/spec_power;

		vec3 h = normalize(v + l);
		float nh = dot(n, h);
		if(nh < 0.0) {
			specfac = 0.0;
		}
		else {
			float nv = max(dot(n, v), 0.01);
			float nl = dot(n, l);
			if(nl <= 0.01) {
				specfac = 0.0;
			}
			else {
				float vh = max(dot(v, h), 0.01);

				float a = 1.0;
				float b = (2.0*nh*nv)/vh;
				float c = (2.0*nh*nl)/vh;

				float g = 0.0;

				if(a < b && a < c) g = a;
				else if(b < a && b < c) g = b;
				else if(c < a && c < b) g = c;

				float p = sqrt(((refrac * refrac)+(vh*vh)-1.0));
				float f = (((p-vh)*(p-vh))/((p+vh)*(p+vh)))*(1.0+((((vh*(p+vh))-1.0)*((vh*(p+vh))-1.0))/(((vh*(p-vh))+1.0)*((vh*(p-vh))+1.0))));
				float ang = acos(nh);

				specfac = max(f*g*exp_blender((-(ang*ang)/(2.0*spec_power*spec_power))), 0.0);
			}
		}
	}
}

void shade_wardiso_spec(vec3 n, vec3 l, vec3 v, float rms, out float specfac)
{
	vec3 h = normalize(l + v);
	float nh = max(dot(n, h), 0.001);
	float nv = max(dot(n, v), 0.001);
	float nl = max(dot(n, l), 0.001);
	float angle = tan(acos(nh));
	float alpha = max(rms, 0.001);

	specfac= nl * (1.0/(4.0*M_PI*alpha*alpha))*(exp_blender(-(angle*angle)/(alpha*alpha))/(sqrt(nv*nl)));
}

void shade_toon_spec(vec3 n, vec3 l, vec3 v, float size, float tsmooth, out float specfac)
{
	vec3 h = normalize(l + v);
	float rslt = dot(h, n);
	float ang = acos(rslt);

	if(ang < size) rslt = 1.0;
	else if(ang >= (size + tsmooth) || tsmooth == 0.0) rslt = 0.0;
	else rslt = 1.0 - ((ang - size)/tsmooth);

	specfac = rslt;
}

void shade_spec_area_inp(float specfac, float inp, out float outspecfac)
{
	outspecfac = specfac*inp;
}

void shade_spec_t(float shadfac, float spec, float visifac, float specfac, out float t)
{
	t = shadfac*spec*visifac*specfac;
}

void shade_add_spec(float t, vec3 lampcol, vec3 speccol, out vec3 outcol)
{
	outcol = t*lampcol*speccol;
}

void alpha_spec_correction(vec3 spec, float spectra, float alpha, out float outalpha)
{
	if (spectra > 0.0) {
		float t = clamp(max(max(spec.r, spec.g), spec.b) * spectra, 0.0, 1.0);
		outalpha = (1.0 - t) * alpha + t;
	}
	else {
		outalpha = alpha;
	}
}

void shade_add(vec4 col1, vec4 col2, out vec4 outcol)
{
	outcol = col1 + col2;
}

void shade_madd(vec4 col, vec4 col1, vec4 col2, out vec4 outcol)
{
	outcol = col + col1*col2;
}

void shade_add_clamped(vec4 col1, vec4 col2, out vec4 outcol)
{
	outcol = col1 + max(col2, vec4(0.0, 0.0, 0.0, 0.0));
}

void shade_madd_clamped(vec4 col, vec4 col1, vec4 col2, out vec4 outcol)
{
	outcol = col + max(col1*col2, vec4(0.0, 0.0, 0.0, 0.0));
}

void shade_maddf(vec4 col, float f, vec4 col1, out vec4 outcol)
{
	outcol = col + f*col1;
}

void shade_mul(vec4 col1, vec4 col2, out vec4 outcol)
{
	outcol = col1*col2;
}

void shade_mul_value(float fac, vec4 col, out vec4 outcol)
{
	outcol = col*fac;
}

void shade_mul_value_v3(float fac, vec3 col, out vec3 outcol)
{
	outcol = col*fac;
}

void shade_obcolor(vec4 col, vec4 obcol, out vec4 outcol)
{
	outcol = vec4(col.rgb*obcol.rgb, col.a);
}

void ramp_rgbtobw(vec3 color, out float outval)
{
	outval = color.r*0.3 + color.g*0.58 + color.b*0.12;
}

void shade_only_shadow(float i, float shadfac, float energy, vec3 shadcol, out vec3 outshadrgb)
{
	outshadrgb = i*energy*(1.0 - shadfac)*(vec3(1.0)-shadcol);
}

void shade_only_shadow_diffuse(vec3 shadrgb, vec3 rgb, vec4 diff, out vec4 outdiff)
{
	outdiff = diff - vec4(rgb*shadrgb, 0.0);
}

void shade_only_shadow_specular(vec3 shadrgb, vec3 specrgb, vec4 spec, out vec4 outspec)
{
	outspec = spec - vec4(specrgb*shadrgb, 0.0);
}

void shade_clamp_positive(vec4 col, out vec4 outcol)
{
	outcol = max(col, vec4(0.0));
}

void test_shadowbuf(vec3 rco, sampler2DShadow shadowmap, mat4 shadowpersmat, float shadowbias, float inp, out float result)
{
	if(inp <= 0.0) {
		result = 0.0;
	}
	else {
		vec4 co = shadowpersmat*vec4(rco, 1.0);

		//float bias = (1.5 - inp*inp)*shadowbias;
		co.z -= shadowbias*co.w;
		
		if (co.w > 0.0 && co.x > 0.0 && co.x/co.w < 1.0 && co.y > 0.0 && co.y/co.w < 1.0)
			result = shadow2DProj(shadowmap, co).x;
		else
			result = 1.0;
	}
}

void test_shadowbuf_vsm(vec3 rco, sampler2D shadowmap, mat4 shadowpersmat, float shadowbias, float bleedbias, float inp, out float result)
{
	if(inp <= 0.0) {
		result = 0.0;
	}
	else {
		vec4 co = shadowpersmat*vec4(rco, 1.0);
		if (co.w > 0.0 && co.x > 0.0 && co.x/co.w < 1.0 && co.y > 0.0 && co.y/co.w < 1.0) {
			vec2 moments = texture2DProj(shadowmap, co).rg;
			float dist = co.z/co.w;
			float p = 0.0;
			
			if(dist <= moments.x)
				p = 1.0;

			float variance = moments.y - (moments.x*moments.x);
			variance = max(variance, shadowbias/10.0);

			float d = moments.x - dist;
			float p_max = variance / (variance + d*d);

			// Now reduce light-bleeding by removing the [0, x] tail and linearly rescaling (x, 1]
			p_max = clamp((p_max-bleedbias)/(1.0-bleedbias), 0.0, 1.0);

			result = max(p, p_max);
		}
		else {
			result = 1.0;
		}
	}
}

void shadows_only(vec3 rco, sampler2DShadow shadowmap, mat4 shadowpersmat, float shadowbias, vec3 shadowcolor, float inp, out vec3 result)
{
	result = vec3(1.0);

	if(inp > 0.0) {
		float shadfac;

		test_shadowbuf(rco, shadowmap, shadowpersmat, shadowbias, inp, shadfac);
		result -= (1.0 - shadfac) * (vec3(1.0) - shadowcolor);
	}
}

void shadows_only_vsm(vec3 rco, sampler2D shadowmap, mat4 shadowpersmat, float shadowbias, float bleedbias, vec3 shadowcolor, float inp, out vec3 result)
{
	result = vec3(1.0);

	if(inp > 0.0) {
		float shadfac;

		test_shadowbuf_vsm(rco, shadowmap, shadowpersmat, shadowbias, bleedbias, inp, shadfac);
		result -= (1.0 - shadfac) * (vec3(1.0) - shadowcolor);
	}
}

void shade_light_texture(vec3 rco, sampler2D cookie, mat4 shadowpersmat, out vec4 result)
{

	vec4 co = shadowpersmat*vec4(rco, 1.0);

	result = texture2DProj(cookie, co);
}

void shade_exposure_correct(vec3 col, float linfac, float logfac, out vec3 outcol)
{
	outcol = linfac*(1.0 - exp(col*logfac));
}

void shade_mist_factor(vec3 co, float enable, float miststa, float mistdist, float misttype, float misi, out float outfac)
{
	if(enable == 1.0) {
		float fac, zcor;

		zcor = (gl_ProjectionMatrix[3][3] == 0.0)? length(co): -co[2];
		
		fac = clamp((zcor - miststa) / mistdist, 0.0, 1.0);
		if(misttype == 0.0) fac *= fac;
		else if(misttype == 1.0);
		else fac = sqrt(fac);

		outfac = 1.0 - (1.0 - fac) * (1.0 - misi);
	}
	else {
		outfac = 0.0;
	}
}

void shade_world_mix(vec3 hor, vec4 col, out vec4 outcol)
{
	float fac = clamp(col.a, 0.0, 1.0);
	outcol = vec4(mix(hor, col.rgb, fac), col.a);
}

void shade_alpha_opaque(vec4 col, out vec4 outcol)
{
	outcol = vec4(col.rgb, 1.0);
}

void shade_alpha_obcolor(vec4 col, vec4 obcol, out vec4 outcol)
{
	outcol = vec4(col.rgb, col.a*obcol.a);
}

/*********** NEW SHADER UTILITIES **************/

float fresnel_dielectric(vec3 Incoming, vec3 Normal, float eta)
{
	/* compute fresnel reflectance without explicitly computing
	 * the refracted direction */
	float c = abs(dot(Incoming, Normal));
	float g = eta * eta - 1.0 + c * c;
	float result;

	if(g > 0.0) {
		g = sqrt(g);
		float A =(g - c)/(g + c);
		float B =(c *(g + c)- 1.0)/(c *(g - c)+ 1.0);
		result = 0.5 * A * A *(1.0 + B * B);
	}
	else {
		result = 1.0;  /* TIR (no refracted component) */
	}

	return result;
}

float hypot(float x, float y)
{
	return sqrt(x*x + y*y);
}

float inverseDistance(vec3 V)
{
	return max( 1 / length(V), 1e-8);
}

void viewN_to_shadeN(vec3 N, out vec3 shadeN)
{
	shadeN = normalize(-N); 
}

void basic_srgb_to_linear(vec4 srgb, out vec4 linear)
{
	linear.r = pow(srgb.r, 0.4545454);
	linear.g = pow(srgb.g, 0.4545454);
	linear.b = pow(srgb.b, 0.4545454);
	linear.a = srgb.a;
}

void spec_bsdf_ggx(vec3 N, vec3 H, vec3 L, vec3 V, float roughness, out float bsdf)
{
	/* GGX Spec */
	float a  = max(1e-4, roughness); /* Artifacts appear with roughness below this threshold */
	float a2 = max(1e-8, a*a);

	float NH = clamp(dot(N, H), 1e-8, 1);
	float NL = clamp(dot(N, L), 1e-8, 1);
	float NV = clamp(dot(N, V), 1e-8, 1);

	/* G_Smith_GGX */
	float G_V = 2 / (1 + sqrt(1 + a2 * (1 - NV*NV) / (NV*NV) ) ); 
	float G_L = 2 / (1 + sqrt(1 + a2 * (1 - NL*NL) / (NL*NL) ) ); 
	float G = G_V * G_L;

	/* D_GGX */
	float tmp = (NH * a2 - NH) * NH + 1.0 ;
	float D = a2 / (tmp * tmp * M_PI) ;

	bsdf = (D * G * 4.0) / NV;
}

void bsdf_ashikhmin_velvet(vec3 N, vec3 H, vec3 L, vec3 V, float sigma, out float bsdf)
{
	/* TODO optimize for display */
	sigma = max(sigma, 1e-2);
	float m_1_sig2 = 1 / (sigma * sigma);

	N = normalize(N);
	L = normalize(L);
	V = normalize(V);

	float NL = dot(N, L);

	bsdf = 0.0;

	if(NL > 0) {
		float NV = dot(N, V);
		float NH = dot(N, H);
		float VH = abs(dot(V, H));

		if(abs(NV) > 1e-5 && abs(NH) < 1.0f-1e-5 && VH > 1e-5) {
			float NHdivVH = NH / VH;
			NHdivVH = max(NHdivVH, 1e-5);

			float fac1 = 2 * abs(NHdivVH * NV);
			float fac2 = 2 * abs(NHdivVH * NL);

			float sinNH2 = 1 - NH * NH;
			float sinNH4 = sinNH2 * sinNH2;
			float cotan2 = (NH * NH) / sinNH2;

			float D = exp(-cotan2 * m_1_sig2) * m_1_sig2 * M_1_PI / sinNH4;
			float G = min(1.0, min(fac1, fac2)); // TODO: derive G from D analytically

			bsdf = 0.25 * (D * G) / NV;
		}

	}
}

void diffuse_bsdf_oren_nayar(float NL, vec3 N, vec3 L, vec3 V, float roughness, out float bsdf)
{
	NL = max(0.0, NL);
	float LV = max(0.0, dot(L, V) );
	float NV = max(1e-8, dot(N, V) );

	float sigma = roughness;
	float div = 1.0 / (M_PI + ((3.0 * M_PI - 4.0) / 6.0) * sigma);

	float A = 1.0 * div;
	float B = sigma * div;

	float s = LV - NL * NV;
	float t = mix(1.0, max(NL, NV), step(0.0, s));
	float oren_nayer_diff = NL * (A + B * s / t);

	float lambert_diff = NL * M_1_PI;

	bsdf = mix(lambert_diff, oren_nayer_diff, roughness);
}


vec3 linePlaneIntersect(vec3 LineOrigin, vec3 LineVec, vec3 PlaneOrigin, vec3 PlaneNormal)
{
	return LineOrigin + LineVec * ( dot(PlaneNormal, PlaneOrigin - LineOrigin) / dot(PlaneNormal, LineVec) );
}

void mostRepresentativePointSphereOrTube(float light_radius, float light_length, vec3 lampY, float light_distance, vec3 R, inout vec3 L, inout float roughness, inout float energy_conservation)
{
	L = light_distance * L;

	/* Tube Light */
	if(light_length>0){
		roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */
		
		// Use Tube Light specular instead of a plane.
		// Energy conservation
		// asin(x) is angle to sphere, atan(x) is angle to disk, saturate(x) is free and in the middle
		//float LineAngle = clamp( light_length / light_distance, 0.0, 1.0);

		//energy_conservation *= roughness / clamp(roughness + 0.5 * LineAngle, 0.0, 1.0);

		// Closest point on line segment to ray
		vec3 L01 = lampY * light_length;
		vec3 L0 = L - 0.5 * L01;
		vec3 L1 = L + 0.5 * L01;

		// Shortest distance
		float a = light_length * light_length;
		float b = dot( R, L01 );
		float t = clamp( dot( L0, b*R - L01 ) / (a - b*b), 0.0, 1.0);
		L = L0 + t * L01;
	}

	/* Sphere Light */
	if(light_radius>0){
		roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

		/* energy preservation */
		float SphereAngle = clamp( light_radius / light_distance, 0.0, 1.0);
		energy_conservation *= pow( roughness / clamp(roughness + 0.5 * SphereAngle, 0.0, 1.0), 2.0);

		/* sphere light */
		vec3 ClosestPointOnRay =  dot(L, R) * R;
		vec3 CenterToRay = ClosestPointOnRay - L;
		vec3 ClosestPointOnSphere = L + CenterToRay * clamp( light_radius * inverseDistance(CenterToRay), 0.0, 1.0);
		L = ClosestPointOnSphere;
	}

	L = normalize(L);
}

/*********** NEW SHADER NODES ***************/

#define NUM_LIGHTS 3

/* bsdfs */
void node_bsdf_diffuse(vec4 color, float roughness, vec3 N, vec3 V, vec4 ambient_light, vec4 direct_light, out vec4 result)
{
	shade_view(V, V); V = -V;

	/* oren_nayar approximation for ambient */
	float NV = clamp(dot(N, V), 0.0, 0.999);
	float fac = 1.0 - pow(1.0 - NV, 1.3);
	float BRDF = mix(1.0, 0.78, fac*roughness);

	result = vec4( (ambient_light.rgb * BRDF + direct_light.rgb) * color.rgb, 1.0);
}

void node_bsdf_glossy(vec4 color, float roughness, vec3 N, vec3 V, vec4 ambient_light, vec4 direct_light, out vec4 result)
{
	result = vec4( (ambient_light.rgb + direct_light.rgb) * color.rgb, 1.0);
}

void node_bsdf_refraction(vec4 color, float roughness, float ior, vec3 N, vec3 V, vec4 ambient_light, vec4 direct_light, out vec4 result)
{
	shade_view(V, V); V = -V;

	ior = (gl_FrontFacing) ? ior : 1.0/ior;
	float c = abs(dot(V, N));
	float g = ior * ior - 1.0 + c * c;

	if(g > 0.0) 
		result = vec4(color.rgb * (direct_light.rgb + ambient_light.rgb), 1.0);
	else
		result = vec4(0.0, 0.0, 0.0, 1.0);
}

void node_bsdf_glass(vec4 color, float roughness, float ior, vec3 N, vec3 V, vec4 reflected_light, vec4 refracted_light, out vec4 result)
{
	shade_view(V, V); V = -V;
	N = normalize(N);

	float fresnel = fresnel_dielectric(V, N, (gl_FrontFacing)? ior: 1.0/ior);

	result = vec4( mix(refracted_light, reflected_light, fresnel).rgb * color.rgb, 1.0);
}

void node_bsdf_translucent(vec4 color, float roughness, vec3 N, vec3 V, vec4 ambient_light, vec4 direct_light, out vec4 result)
{
	result = vec4( (ambient_light.rgb + direct_light.rgb) * color.rgb, 1.0);
}

void node_bsdf_transparent(vec4 color, vec4 background, out vec4 result)
{
	result = vec4(background.rgb * color.rgb, 1.0);
}

void node_bsdf_velvet(vec4 color, float sigma, vec3 N, vec3 V, vec4 ambient_light, vec4 direct_light, out vec4 result)
{
	result = vec4( (ambient_light.rgb + direct_light.rgb) * color.rgb, 1.0);
}

/* bsdfs with opengl Lights */
void node_bsdf_diffuse_lights(vec4 color, float roughness, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	shade_view(V, V); V = -V;

	/* ambient light */
	vec3 accumulator = ambient_light.rgb;
	float bsdf = 0.0;
	
	/* oren_nayar approximation for ambient */
	float NV = clamp(dot(N, V), 0.0, 0.999);
	float fac = 1.0 - pow(1.0 - NV, 1.3);
	accumulator *= mix(1.0, 0.78, fac*roughness);

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_diffuse = gl_LightSource[i].diffuse.rgb;

		diffuse_bsdf_oren_nayar(dot(N,L), N, L, V, roughness, bsdf);
		accumulator += bsdf * light_diffuse;
	}

	result = vec4(accumulator*color.rgb, 1.0);
}

void node_bsdf_glossy_lights(vec4 color, float roughness, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	shade_view(V, V); V = -V;
	N = normalize(N);

	vec3 accumulator = ambient_light.rgb;

	if (roughness < 1e-4) {
		//Instead of a tiny bright spot
		result = vec4(accumulator * color.rgb, 1.0);
		return;
	}

	float specfac = 0.0;

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		vec3 H = normalize(L + V);

		spec_bsdf_ggx(N, H, L, V, roughness, specfac);
		accumulator += light_specular * specfac;
	}

	result = vec4(M_1_PI * M_1_PI * accumulator * color.rgb, 1.0); /* M_1_P to reduce Brightness */
}

void node_bsdf_refraction_lights(vec4 color, float roughness, float ior, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	node_bsdf_glossy_lights(color, roughness, N, V, ambient_light, result);
}

void node_bsdf_glass_lights(vec4 color, float roughness, float ior, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	node_bsdf_glossy_lights(color, roughness, N, V, ambient_light, result);
}

void node_bsdf_translucent_lights(vec4 color, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	N = -N;
	node_bsdf_diffuse_lights(color, 0.0, N, V, ambient_light, result);
}

void node_bsdf_velvet_lights(vec4 color, float sigma, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	shade_view(V, V); V = -V;

	/* ambient light */
	vec3 accumulator = ambient_light.rgb;
	float bsdf = 0.0;

	/* directional lights */
	for(int i = 0; i < NUM_LIGHTS; i++) {
		vec3 L = gl_LightSource[i].position.xyz;
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		vec3 H = normalize(L + V);

		bsdf_ashikhmin_velvet(N, H, L, V, sigma, bsdf);
		accumulator += bsdf * light_specular;
	}

	result = vec4(accumulator * color.rgb, 1.0);
}

/* bsdfs with physical Lights */

/* DIFFUSE */

void node_bsdf_diffuse_sphere_light(vec3 N, vec3 L, vec3 V, float light_distance, float light_radius, float roughness, out float diff)
{
	light_radius = max(light_radius, 0.0001);
	float sqrDist =  light_distance * light_distance;
	float cosTheta = clamp(dot(N, L), -0.999, 0.999) ; 
	float sqrLightRadius = light_radius * light_radius ;
	float h = min(light_radius / light_distance , 0.9999) ;
	float h2 = h*h;

	float illuminance = 0.0 ;
	if ( cosTheta * cosTheta > h2 )
	{
		illuminance = M_PI * h2 * clamp(cosTheta, 0.0, 1.0) ;
	}
	else
	{
		float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
		float x = sqrt(1.0 / h2 - 1.0);
		float y = -x * ( cosTheta / sinTheta );
		float sinThetaSqrtY = sinTheta * sqrt(1.0 - y * y);
		illuminance = (cosTheta * acos(y) - x * sinThetaSqrtY ) * h2 + atan(sinThetaSqrtY / x);
	}

	/* Energy conservation + cycle matching */
	diff = max(illuminance, 0.0) * 2.5 * M_1_PI / sqrLightRadius;
}

void node_bsdf_diffuse_area_light(vec3 N, vec3 L, vec3 V, float light_distance, vec3 lampco, vec2 scale, mat4 lampmat, float areasizex, float areasizey, float roughness, out float diff)
{
	N = -N;

	vec3 lampX = normalize( (lampmat * vec4(1.0,0.0,0.0,0.0) ).xyz ); //lamp right axis
	vec3 lampY = normalize( (lampmat * vec4(0.0,1.0,0.0,0.0) ).xyz ); //lamp up axis
	vec3 lampZ = normalize( (lampmat * vec4(0.0,0.0,1.0,0.0) ).xyz ); //lamp projection axis

	diff = 0.0;

	areasizex *= scale.x;
	areasizey *= scale.y;

	if ( dot(-L, lampZ) > 0.0 )
	{
		float width = areasizex / 2.0;
		float height = areasizey / 2.0;

		vec3 p0 = lampco + lampX * -width + lampY *  height;
		vec3 p1 = lampco + lampX * -width + lampY * -height;
		vec3 p2 = lampco + lampX *  width + lampY * -height;
		vec3 p3 = lampco + lampX *  width + lampY *  height;

		float solidAngle = rectangleSolidAngle(V, p0, p1, p2, p3);

		diff = solidAngle * 0.2 * (
			max(0.0, dot( normalize(p0 - V), N) ) +
			max(0.0, dot( normalize(p1 - V), N) ) +
			max(0.0, dot( normalize(p2 - V), N) ) +
			max(0.0, dot( normalize(p3 - V), N) ) +
			max(0.0, dot( -L, N) )
		);
	}

	/* Energy conservation + cycle matching */
	diff *= 8.0 / (areasizex * areasizey);
}

void node_bsdf_diffuse_sun_light(vec3 N, vec3 L, vec3 V, float light_distance, float light_radius, float roughness, out float diff)
{
	light_radius = max(light_radius, 0.0001);
	float sqrDist =  light_distance * light_distance;
	float cosTheta = clamp(dot(N, L), -0.999, 0.999) ; 
	float sqrLightRadius = light_radius * light_radius ;
	float h = min(light_radius / light_distance , 0.9999) ;
	float h2 = h*h;

	float illuminance = 0.0 ;
	if ( cosTheta * cosTheta > h2 )
	{
		illuminance = M_PI * h2 * clamp(cosTheta, 0.0, 1.0) ;
	}
	else
	{
		float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
		float x = sqrt(1.0 / h2 - 1.0);
		float y = -x * ( cosTheta / sinTheta );
		float sinThetaSqrtY = sinTheta * sqrt(1.0 - y * y);
		illuminance = (cosTheta * acos(y) - x * sinThetaSqrtY ) * h2 + atan(sinThetaSqrtY / x);
	}

	/* Energy conservation + cycle matching */
	diff = max(illuminance, 0.0) * 2.5 / sqrLightRadius;
}

/* GLOSSY SHARP */

void node_bsdf_glossy_sharp_sphere_light(vec3 N, vec3 L, vec3 V, float light_distance, float light_radius, float roughness, out float specfac)
{
	L = light_distance * L;
	N = normalize(N);
	shade_view(V, V);

	vec3 R = -reflect(V, N);

	/* Does not behave well when light get very close to the surface or penetrate it */
	vec3 C = max(0.0, dot(L, R)) * R - L;
	specfac = ( length(C) < light_radius ) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	specfac *= 2.5 / (light_radius * light_radius);
}

void node_bsdf_glossy_sharp_area_light(vec3 N, vec3 L, vec3 V, float light_distance, vec2 scale, mat4 viewinvmat, mat4 lampinvmat, mat4 lampmat, float areasizex, float areasizey, float roughness, out float specfac)
{
	L = light_distance * L;
	N = normalize(N);
	shade_view(V, V);

	vec3 lampX = normalize( (lampmat * vec4(1.0,0.0,0.0,0.0) ).xyz ); //lamp right axis
	vec3 lampY = normalize( (lampmat * vec4(0.0,1.0,0.0,0.0) ).xyz ); //lamp up axis
	vec3 lampZ = normalize( (lampmat * vec4(0.0,0.0,1.0,0.0) ).xyz ); //lamp projection axis

	areasizex *= scale.x;
	areasizey *= scale.y;
	float width = areasizex / 2.0;
	float height = areasizey / 2.0;

	/* Find the intersection point E between the reflection vector and the light plane */
	vec3 R = reflect(V, N);
	vec3 E = linePlaneIntersect(vec3(0.0), R, L, lampZ);
	vec3 pointOnLightPlane = E - L;
	float A = dot(lampX, pointOnLightPlane);
	float B = dot(lampY, pointOnLightPlane);

	specfac = (A < width && B < height) ? 1.0 : 0.0;
	specfac *= (A > -width && B > -height) ? 1.0 : 0.0;

	/* Masking */
	specfac *= (dot(-L, lampZ) > 0.0) ? 1.0 : 0.0;
	specfac *= (dot(R, lampZ) > 0.0) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	specfac *= 25.0 / (areasizex * areasizey);
}

void node_bsdf_glossy_sharp_sun_light(vec3 N, vec3 L, vec3 V, float light_distance, float light_radius, float roughness, out float specfac)
{
	//TODO Find a better approximation
	specfac = 0.0;
}

/* GLOSSY GGX */

void node_bsdf_glossy_ggx_sphere_light(vec3 N, vec3 L, vec3 V, float light_distance, float light_radius, float roughness, out float specfac)
{
	if (roughness < 1e-4 && light_radius == 0) {
		//Instead of a tiny bright spot
		specfac = 0.0;
		return;
	}

	N = normalize(N);
	shade_view(V, V);

	vec3 R = reflect(V, N);
	float energy_conservation = 1.0;

	mostRepresentativePointSphereOrTube(light_radius, 0.0, vec3(0.0), light_distance, R, L, roughness, energy_conservation);

	vec3 H = normalize(L + V);
	spec_bsdf_ggx(N, H, L, V, roughness, specfac);

	specfac *= energy_conservation * 1.6 * M_1_PI / (light_distance * light_distance);
}

void node_bsdf_glossy_ggx_area_light(vec3 N, vec3 L, vec3 V, float light_distance, vec2 scale, mat4 viewinvmat, mat4 lampinvmat, mat4 lampmat, float areasizex, float areasizey, float roughness, out float specfac)
{
	if (areasizex == 0 || areasizey == 0) {
		//Instead of a tiny bright spot
		specfac = 0.0;
		return;
	}

	areasizex *= scale.x;
	areasizey *= scale.y;

	//Used later for Masking
	vec3 lampZ = normalize( (lampmat * vec4(0.0,0.0,1.0,0.0) ).xyz ); //lamp projection axis
	float masking = max(dot( normalize(-L), lampZ), 0.0);
	//End of masking

	N = normalize(N);
	shade_view(V, V);

	vec3 R = reflect(V,N);
	//R = normalize(mix(N, R, 1 - roughness)); //Dominant Direction 

	float energy_conservation = 1.0;

	float max_size = max(areasizex, areasizey);
	float min_size = min(areasizex, areasizey);
	vec3 lampVec = (areasizex > areasizey) ? normalize( (lampmat * vec4(1.0,0.0,0.0,0.0) ).xyz ) : normalize( (lampmat * vec4(0.0,1.0,0.0,0.0) ).xyz ); //lamp up axis


	mostRepresentativePointSphereOrTube(min_size/2, max_size-min_size, lampVec, light_distance, R, L, roughness, energy_conservation);

	vec3 H = normalize(L + V);

	spec_bsdf_ggx(N, H, L, V, roughness, specfac);

	//energy_conservation
	float LineAngle = clamp( (max_size-min_size) / light_distance, 0.0, 1.0);
	float energy_conservation_line = energy_conservation * ( roughness / clamp(roughness + 0.5 * LineAngle, 0.0, 1.1));
	//Empirical modification for low roughness matching
	float energy_conservation_mod = energy_conservation * (1 + roughness) / ( max_size/min_size );
	energy_conservation = mix(energy_conservation_mod, energy_conservation_line, min(roughness/0.3, 0.9*(1.1-roughness)/0.1)); //Empiric

	specfac *= energy_conservation / (light_distance * light_distance);
	specfac *= 2.9; //Cycles matching //Empiric
	specfac *= masking;
}

void node_bsdf_glossy_ggx_sun_light(vec3 N, vec3 L, vec3 V, float light_distance, float light_radius, float roughness, out float specfac)
{
	//TODO Find a better approximation
	if (roughness < 1e-4 && light_radius == 0.0) {
		//Instead of a tiny bright spot
		specfac = 0.0;
		return;
	}

	N = normalize(N);
	shade_view(V, V);

	vec3 R = reflect(V,N);
	float energy_conservation = 1.0;


	/* GGX Spec */
	if(light_radius > 0.0){
		//roughness = max(3e-3, roughness); /* Artifacts appear with roughness below this threshold */

		/* energy preservation */
		// float SphereAngle = clamp( light_radius / light_distance, 0.0, 1.0);
		// energy_conservation = 2*a / (2*a + SphereAngle); //( x / (x + 0.5 * y))^2;
		// energy_conservation *= energy_conservation;

		float radius = sin(light_radius); //Disk radius
		float distance = cos(light_radius); // Distance to disk

		/* disk light */
		float LR = dot(L, R);
		vec3 S = normalize(R - LR * L);
		vec3 L = LR < distance ? normalize(distance * L + S * radius) : R;
	}

	//L = normalize(L);
	vec3 H = normalize(L + V);
	spec_bsdf_ggx(N, H, L, V, roughness, specfac);
	specfac *= energy_conservation;
}

/* REFRACT SHARP */

void node_bsdf_refract_sharp_sphere_light(vec3 N, vec3 L, vec3 V, float light_distance, float ior, float light_radius, float roughness, out float specfac)
{
	L = light_distance * L;
	N = normalize(N);
	shade_view(V, V);

	vec3 R = refract(-V, N, (gl_FrontFacing) ? 1.0/ior : ior);

	/* Does not behave well when light get very close to the surface or penetrate it */
	vec3 C = max(0.0, dot(L, R)) * R - L;
	specfac = ( length(C) < light_radius ) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	specfac *= 2.5 / (light_radius * light_radius);
}

void node_bsdf_refract_sharp_area_light(vec3 N, vec3 L, vec3 V, float light_distance, float ior, vec2 scale, mat4 viewinvmat, mat4 lampinvmat, mat4 lampmat, float areasizex, float areasizey, float roughness, out float specfac)
{
	L = light_distance * L;
	N = normalize(N);
	shade_view(V, V);

	vec3 lampX = normalize( (lampmat * vec4(1.0,0.0,0.0,0.0) ).xyz ); //lamp right axis
	vec3 lampY = normalize( (lampmat * vec4(0.0,1.0,0.0,0.0) ).xyz ); //lamp up axis
	vec3 lampZ = normalize( (lampmat * vec4(0.0,0.0,1.0,0.0) ).xyz ); //lamp projection axis

	areasizex *= scale.x;
	areasizey *= scale.y;
	float width = areasizex / 2.0;
	float height = areasizey / 2.0;

	/* Find the intersection point E between the reflection vector and the light plane */
	vec3 R = refract(-V, N, (gl_FrontFacing) ? 1.0/ior : ior);
	vec3 E = linePlaneIntersect(vec3(0.0), R, L, lampZ);
	vec3 pointOnLightPlane = E - L;
	float A = dot(lampX, pointOnLightPlane);
	float B = dot(lampY, pointOnLightPlane);

	specfac = (A < width && B < height) ? 1.0 : 0.0;
	specfac *= (A > -width && B > -height) ? 1.0 : 0.0;

	/* Masking */
	specfac *= (dot(-L, lampZ) > 0.0) ? 1.0 : 0.0;
	specfac *= (dot(-R, lampZ) > 0.0) ? 1.0 : 0.0;

	/* Energy conservation + cycle matching */
	specfac *= 25.0 / (areasizex * areasizey);
}

void node_bsdf_refract_sharp_sun_light(vec3 N, vec3 L, vec3 V, float light_distance, float ior, float light_radius, float roughness, out float specfac)
{
	//TODO Find a better approximation
	specfac = 0.0;
}

/* VELVET */

void node_bsdf_velvet_sphere_light(vec3 N, vec3 L, vec3 V, float light_distance, float sigma, float light_radius, out float specfac)
{
	L = L;
	N = normalize(N);
	shade_view(V, V);
	vec3 H = normalize(L + V);

	bsdf_ashikhmin_velvet(N, H, L, V, sigma, specfac);

	/* Energy conservation + cycle matching */
	specfac *= 8.0 / (light_distance * light_distance);
}

void node_bsdf_velvet_area_light(vec3 N, vec3 L, vec3 V, float light_distance, float sigma, vec2 scale, mat4 viewinvmat, mat4 lampinvmat, mat4 lampmat, float areasizex, float areasizey, out float specfac)
{
	L = L;
	N = normalize(N);
	shade_view(V, V);
	vec3 H = normalize(L + V);

	areasizex *= scale.x;
	areasizey *= scale.y;

	bsdf_ashikhmin_velvet(N, H, L, V, sigma, specfac);

	/* Energy conservation + cycle matching */
	specfac *= 25.0 / (areasizex * areasizey);
	specfac *= 1.0 / (light_distance * light_distance);
}

void node_bsdf_velvet_sun_light(vec3 N, vec3 L, vec3 V, float light_distance, float sigma, float light_radius, out float specfac)
{
	N = normalize(N);
	shade_view(V, V);
	vec3 H = normalize(L + V);

	bsdf_ashikhmin_velvet(N, H, L, V, sigma, specfac);
}


/* Others Bsdfs */
void default_diffuse(vec4 color, vec3 normal, out vec4 result)
{
	node_bsdf_diffuse_lights(color, 0.0, normal, vec3(0.0), vec4(0.2), result);
}

void node_bsdf_anisotropic(vec4 color, float roughness, float anisotropy, float rotation, vec3 N, vec3 T, out vec4 result)
{
	default_diffuse(color, N, result);
}

void node_bsdf_toon(vec4 color, float size, float tsmooth, vec3 N, out vec4 result)
{
	default_diffuse(color, N, result);
}

void node_subsurface_scattering(vec4 color, float scale, vec3 radius, float sharpen, float texture_blur, vec3 N, out vec4 result)
{
	default_diffuse(color, N, result);
}

void node_bsdf_hair(vec4 color, float offset, float roughnessu, float roughnessv, out vec4 result)
{
	result = color;
}

void node_ambient_occlusion(vec4 color, out vec4 result)
{
	result = color;
}

/* emission */

void node_emission(vec4 color, float strength, vec3 N, out vec4 result)
{
	result = color*strength;
}

/* blackbody */

void node_blackbody(float T, out vec4 col)
{
	float u = ( 0.860117757f + 1.54118254e-4f * T + 1.28641212e-7f * T*T ) / ( 1.0f + 8.42420235e-4f * T + 7.08145163e-7f * T*T );
	float v = ( 0.317398726f + 4.22806245e-5f * T + 4.20481691e-8f * T*T ) / ( 1.0f - 2.89741816e-5f * T + 1.61456053e-7f * T*T );

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
uniform vec3 node_wavelength_LUT[81] = {
	vec3(0.0014f,0.0000f,0.0065f), vec3(0.0022f,0.0001f,0.0105f), vec3(0.0042f,0.0001f,0.0201f),
	vec3(0.0076f,0.0002f,0.0362f), vec3(0.0143f,0.0004f,0.0679f), vec3(0.0232f,0.0006f,0.1102f),
	vec3(0.0435f,0.0012f,0.2074f), vec3(0.0776f,0.0022f,0.3713f), vec3(0.1344f,0.0040f,0.6456f),
	vec3(0.2148f,0.0073f,1.0391f), vec3(0.2839f,0.0116f,1.3856f), vec3(0.3285f,0.0168f,1.6230f),
	vec3(0.3483f,0.0230f,1.7471f), vec3(0.3481f,0.0298f,1.7826f), vec3(0.3362f,0.0380f,1.7721f),
	vec3(0.3187f,0.0480f,1.7441f), vec3(0.2908f,0.0600f,1.6692f), vec3(0.2511f,0.0739f,1.5281f),
	vec3(0.1954f,0.0910f,1.2876f), vec3(0.1421f,0.1126f,1.0419f), vec3(0.0956f,0.1390f,0.8130f),
	vec3(0.0580f,0.1693f,0.6162f), vec3(0.0320f,0.2080f,0.4652f), vec3(0.0147f,0.2586f,0.3533f),
	vec3(0.0049f,0.3230f,0.2720f), vec3(0.0024f,0.4073f,0.2123f), vec3(0.0093f,0.5030f,0.1582f),
	vec3(0.0291f,0.6082f,0.1117f), vec3(0.0633f,0.7100f,0.0782f), vec3(0.1096f,0.7932f,0.0573f),
	vec3(0.1655f,0.8620f,0.0422f), vec3(0.2257f,0.9149f,0.0298f), vec3(0.2904f,0.9540f,0.0203f),
	vec3(0.3597f,0.9803f,0.0134f), vec3(0.4334f,0.9950f,0.0087f), vec3(0.5121f,1.0000f,0.0057f),
	vec3(0.5945f,0.9950f,0.0039f), vec3(0.6784f,0.9786f,0.0027f), vec3(0.7621f,0.9520f,0.0021f),
	vec3(0.8425f,0.9154f,0.0018f), vec3(0.9163f,0.8700f,0.0017f), vec3(0.9786f,0.8163f,0.0014f),
	vec3(1.0263f,0.7570f,0.0011f), vec3(1.0567f,0.6949f,0.0010f), vec3(1.0622f,0.6310f,0.0008f),
	vec3(1.0456f,0.5668f,0.0006f), vec3(1.0026f,0.5030f,0.0003f), vec3(0.9384f,0.4412f,0.0002f),
	vec3(0.8544f,0.3810f,0.0002f), vec3(0.7514f,0.3210f,0.0001f), vec3(0.6424f,0.2650f,0.0000f),
	vec3(0.5419f,0.2170f,0.0000f), vec3(0.4479f,0.1750f,0.0000f), vec3(0.3608f,0.1382f,0.0000f),
	vec3(0.2835f,0.1070f,0.0000f), vec3(0.2187f,0.0816f,0.0000f), vec3(0.1649f,0.0610f,0.0000f),
	vec3(0.1212f,0.0446f,0.0000f), vec3(0.0874f,0.0320f,0.0000f), vec3(0.0636f,0.0232f,0.0000f),
	vec3(0.0468f,0.0170f,0.0000f), vec3(0.0329f,0.0119f,0.0000f), vec3(0.0227f,0.0082f,0.0000f),
	vec3(0.0158f,0.0057f,0.0000f), vec3(0.0114f,0.0041f,0.0000f), vec3(0.0081f,0.0029f,0.0000f),
	vec3(0.0058f,0.0021f,0.0000f), vec3(0.0041f,0.0015f,0.0000f), vec3(0.0029f,0.0010f,0.0000f),
	vec3(0.0020f,0.0007f,0.0000f), vec3(0.0014f,0.0005f,0.0000f), vec3(0.0010f,0.0004f,0.0000f),
	vec3(0.0007f,0.0002f,0.0000f), vec3(0.0005f,0.0002f,0.0000f), vec3(0.0003f,0.0001f,0.0000f),
	vec3(0.0002f,0.0001f,0.0000f), vec3(0.0002f,0.0001f,0.0000f), vec3(0.0001f,0.0000f,0.0000f),
	vec3(0.0001f,0.0000f,0.0000f), vec3(0.0001f,0.0000f,0.0000f), vec3(0.0000f,0.0000f,0.0000f)
};

void node_wavelength(float w, out vec4 col)
{
	float ii = (w-380.0f) * (1.0f/5.0f);  // scaled 0..80
	int i = int(ii);
	vec3 color;
	
	if(i < 0 || i >= 80) {
		color = vec3(0.0f, 0.0f, 0.0f);
	}
	else {
		ii -= i;
		color = mix(node_wavelength_LUT[i], node_wavelength_LUT[i+1], ii);
	}
	
	color = xyz_to_rgb(color.x, color.y, color.z);
	color *= 1.0f/2.52f;	// Empirical scale from lg to make all comps <= 1
	
	/* Clamp to zero if values are smaller */
	col = vec4(max(color, vec3(0.0f, 0.0f, 0.0f)), 1.0);

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
	shader = mix(shader1, shader2, fac);
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

void node_geometry(vec3 I, vec3 N, mat4 toworld,
	out vec3 position, out vec3 normal, out vec3 tangent,
	out vec3 true_normal, out vec3 incoming, out vec3 parametric,
	out float backfacing, out float pointiness)
{
	position = (toworld*vec4(I, 1.0)).xyz;
	normal = (toworld*vec4(N, 0.0)).xyz;
	tangent = vec3(0.0);
	true_normal = normal;

	/* handle perspective/orthographic */
	vec3 I_view = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(I): vec3(0.0, 0.0, -1.0);
	incoming = -(toworld*vec4(I_view, 0.0)).xyz;

	parametric = vec3(0.0);
	backfacing = (gl_FrontFacing)? 0.0: 1.0;
	pointiness = 0.5;
}

void node_geometry_lamp(vec3 I, vec3 N, mat4 toworld,
	out vec3 position, out vec3 normal, out vec3 tangent,
	out vec3 true_normal, out vec3 incoming, out vec3 parametric,
	out float backfacing, out float pointiness)
{
	position = (toworld*vec4(I-N, 1.0)).xyz;
	normal = normalize((toworld*vec4(N, 0.0)).xyz);
	tangent = vec3(0.0);
	true_normal = normal;
	incoming = normal;

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

void node_tex_coord_background_sampling_normal(vec3 I, vec3 N, mat4 viewmat, mat4 obinvmat, vec4 camerafac,
	vec3 attr_orco, vec3 attr_uv,
	out vec3 generated, out vec3 normal, out vec3 uv, out vec3 object,
	out vec3 camera, out vec3 window, out vec3 reflection)
{
	//N = (viewinvmat * vec4(N, 0.0) ).xyz;

	generated = N;
	normal = -N;
	uv = vec3(attr_uv.xy, 0.0);
	object = N;

	camera = (viewmat * vec4(N, 0.0) ).xyz * vec3(1.0, 1.0, -1.0);
	window = (gl_ProjectionMatrix[3][3] == 0.0) ? 
	              vec3(mtex_2d_mapping(N).xy * camerafac.xy + camerafac.zw, 0.0) : 
	              vec3(vec2(0.5) * camerafac.xy + camerafac.zw, 0.0);

	reflection = -N;
}


void node_tex_coord_background_sampling(vec3 I, vec3 N, mat4 viewinvmat, mat4 obinvmat, vec4 camerafac,
	vec3 attr_orco, vec3 attr_uv,
	out vec3 generated, out vec3 normal, out vec3 uv, out vec3 object,
	out vec3 camera, out vec3 window, out vec3 reflection)
{
	I = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(I): vec3(0.0, 0.0, -1.0);
	vec3 wI = (viewinvmat * vec4(I,0.0)).xyz;

	generated = wI;
	normal = -wI;
	uv = vec3(attr_uv.xy, 0.0);
	object = wI;

	camera = I * vec3(1.0, 1.0, -1.0);
	window = (gl_ProjectionMatrix[3][3] == 0.0) ? 
	              vec3(mtex_2d_mapping(I).xy * camerafac.xy + camerafac.zw, 0.0) : 
	              vec3(vec2(0.5) * camerafac.xy + camerafac.zw, 0.0);

	reflection = -wI;
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

void node_tex_environment_equirectangular(vec3 co, sampler2D ima, float Lod, out vec4 color)
{
	vec3 nco = normalize(co);
	float u = -atan(nco.y, nco.x)/(2.0*M_PI) + 0.5;
	float v = atan(nco.z, hypot(nco.x, nco.y))/M_PI + 0.5;

	color = texture2DLod(ima, vec2(u, v), Lod);
}

void node_tex_environment_mirror_ball(vec3 co, sampler2D ima, float Lod, out vec4 color)
{
	vec3 nco = normalize(co);

	nco.y -= 1.0;

	float div = 2.0*sqrt(max(-0.5*nco.y, 0.0));
	if(div > 0.0)
		nco /= div;

	float u = 0.5*(nco.x + 1.0);
	float v = 0.5*(nco.z + 1.0);

	color = texture2DLod(ima, vec2(u, v), Lod);
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

	uint a = deadbeef + uint(len << 2) + uint(13);
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
	if (isinf(f))
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
	out float transparent_depth)
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
}

void node_light_falloff(float strength, float tsmooth, vec3 ray, out float quadratic, out float linear, out float constant)
{
	float ray_length = length(ray);

	if (tsmooth > 0.0) {
		float squared = ray_length * ray_length;
		strength *= squared / (tsmooth + squared);
	}

	quadratic = strength;
	linear = (strength * ray_length);
	constant = (strength * ray_length * ray_length);
}

void node_object_info(out vec3 location, out float object_index, out float material_index, out float random)
{
	location = vec3(0.0);
	object_index = 0.0;
	material_index = 0.0;
	random = 0.0;
}

void node_vector_transform(vec3 Vector, mat4 mat, out vec3 result)
{
	result = ( mat * vec4(Vector,0.0) ).xyz;
}

void node_normal_map_tangent(float strength, vec4 color, vec3 N, vec4 T, mat4 viewmat, mat4 obmat, mat4 viewinvmat, mat4 obinvmat, out vec3 result)
{
	color = ( color - vec4(0.5))*vec4(2.0);
	N = normalize((obinvmat*(viewinvmat*vec4(N, 0.0))).xyz);
	T = vec4( normalize((obinvmat*(viewinvmat*vec4(T.xyz, 0.0))).xyz), T.w);
	vec3 B = T.w * cross(N, T.xyz);
	vec3 No = color.x*T.xyz + color.y*B + color.z*N;
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


/* environment shader */

void env_sampling_reflect_sharp(vec3 I, vec3 N, mat4 viewinvmat, out vec3 result)
{
	result = reflect(normalize(I), normalize(N));
}

void env_sampling_refract_sharp(vec3 I, vec3 N, float eta, out vec3 result)
{
	result = refract(normalize(I), normalize(N), (gl_FrontFacing) ? 1.0/eta : eta);
}

/* Hammersley points set */
uniform vec4 hammersley32[32] 	= vec4[32](vec4(0,0,1,0),vec4(0.03125,0.5,-1,1.224646798818428e-16),vec4(0.0625,0.25,0,1),vec4(0.09375,0.75,-1.8369701961596905e-16,-1),vec4(0.125,0.125,0.7071067811865475,0.7071067811865475),vec4(0.15625,0.625,-0.7071067811865476,-0.7071067811865474),vec4(0.1875,0.375,-0.7071067811865474,0.7071067811865476),vec4(0.21875,0.875,0.7071067811865474,-0.7071067811865476),vec4(0.25,0.0625,0.9238795325112867,0.3826834323650898),vec4(0.28125,0.5625,-0.9238795325112867,-0.38268343236508967),vec4(0.3125,0.3125,-0.3826834323650897,0.9238795325112867),vec4(0.34375,0.8125,0.38268343236509006,-0.9238795325112866),vec4(0.375,0.1875,0.3826834323650898,0.9238795325112867),vec4(0.40625,0.6875,-0.38268343236509034,-0.9238795325112865),vec4(0.4375,0.4375,-0.9238795325112867,0.3826834323650898),vec4(0.46875,0.9375,0.9238795325112865,-0.38268343236509034),vec4(0.5,0.03125,0.9807852804032304,0.19509032201612825),vec4(0.53125,0.53125,-0.9807852804032304,-0.19509032201612836),vec4(0.5625,0.28125,-0.1950903220161282,0.9807852804032304),vec4(0.59375,0.78125,0.19509032201612828,-0.9807852804032304),vec4(0.625,0.15625,0.5555702330196022,0.8314696123025452),vec4(0.65625,0.65625,-0.555570233019602,-0.8314696123025453),vec4(0.6875,0.40625,-0.8314696123025453,0.555570233019602),vec4(0.71875,0.90625,0.8314696123025452,-0.5555702330196022),vec4(0.75,0.09375,0.8314696123025452,0.5555702330196022),vec4(0.78125,0.59375,-0.8314696123025455,-0.5555702330196018),vec4(0.8125,0.34375,-0.5555702330196018,0.8314696123025455),vec4(0.84375,0.84375,0.5555702330196017,-0.8314696123025455),vec4(0.875,0.21875,0.19509032201612825,0.9807852804032304),vec4(0.90625,0.71875,-0.19509032201612864,-0.9807852804032304),vec4(0.9375,0.46875,-0.9807852804032303,0.19509032201612844),vec4(0.96875,0.96875,0.9807852804032304,-0.19509032201612864));

/* needed for uint type and bitwise operation */
#extension GL_EXT_gpu_shader4: enable

/* Tiny Encryption Algorithm (used for noise generation) */
uvec2 TEA(uvec2 v)
{
	/* set up */
	uint v0 = uint(v.x);
	uint v1 = uint(v.y);
	uint sum = 0u;

    /* cache key */
    uint k[4] = uint[4](0xA341316Cu , 0xC8013EA4u , 0xAD90777Du , 0x7E95761Eu );   

    /* basic cycle start */
    for (int i = 0; i < 3; i++) {                       
        sum += 0x9e3779b9u;  /* a key schedule constant */
        v0 += ((v1 << 4u) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5u) + k[1]);
        v1 += ((v0 << 4u) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5u) + k[3]);
    }

    return uvec2(v0, v1);
}

/* From http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html */
uint radicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return bits;
}

vec4 hammersley2d(uint i, uint N, uvec2 random) {
	float E1 = fract( float(i) / float(N) + float( random.x & uint(0xffff) ) / float(1<<16) );
	float E2 = float( radicalInverse_VdC(i) ^ uint(random.y) ) * 2.3283064365386963e-10;
	float phi = 2.0f * M_PI * E2;
	return vec4( E1, E2, cos(phi), sin(phi) );
}

vec3 importance_sample_GGX(vec4 Xi, float a2)
{
	float Phi = 2.0 * M_PI * Xi.y;
	float CosTheta = sqrt( (1.0 - Xi.x) / ( 1.0 + (a2 - 1.0) * Xi.x ) );
	float SinTheta = sqrt( 1.0 - CosTheta * CosTheta );

	return vec3(SinTheta*Xi.z, SinTheta*Xi.w, CosTheta);
}

vec3 tangentSpace( vec3 vector, vec3 N, vec3 T, vec3 B)
{
	return T * vector.x + B * vector.y + N * vector.z;
}

void env_sampling_reflect_glossy(vec3 I, vec3 N, float roughness, float precalcLodFactor, float maxLod, mat4 viewinvmat, sampler2D ima, out vec3 result)
{
	/* handle perspective/orthographic */
	//I = (gl_ProjectionMatrix[3][3] == 0.0)? normalize(I): vec3(0.0, 0.0, -1.0);
	//I = normalize( (viewinvmat*vec4(I,0.0)).xyz );
	I = normalize(I);

	/* Noise for the importance sample Envmap */
	uvec2 random = uvec2(gl_FragCoord.x,gl_FragCoord.y);
	random = TEA(random);
	random.x ^= uint( 21389u );
	random.y ^= uint( 49233u );

	/* Generate tangent space */
	//N = normalize( (viewinvmat * vec4(N, 0.0)).xyz );
	N = normalize(N);
	vec3 UpVector = abs(N.z) < 0.99999 ? vec3(0.0,0.0,1.0) : vec3(1.0,0.0,0.0);
	vec3 T = normalize( cross(UpVector,N) );
	vec3 B = cross(N,T);

	//roughness = max( 0.0132, roughness * roughness ); /* Error threshold */
	float a  = max(1e-8, roughness);
	float a2 = max(1e-8, a*a);

	float NV = max(1e-8, abs(dot(I, N)));

	vec3 V;
	V.x = sqrt( 1.0 - NV * NV );
	V.y = 0.0;
	V.z = NV;

	float Vis_SmithV = NV + sqrt( NV * (NV - NV * a2) + a2 );

	/* Integrating Envmap */
	vec4 color;
	vec3 FilteredColor = vec3(0.0);
	float Weight = 0.0;
	float IntegralA = 0.0;
	float IntegralB = 0.0;

	const uint NumSamples = 32u;
	for ( uint i = 0u; i < NumSamples; i++ ) {
		vec4 Xi = hammersley2d( i, NumSamples, random );
		vec3 H = importance_sample_GGX(Xi, a2);
		vec3 tH = tangentSpace( H, N, T, B );
		vec3 L = reflect(I, tH);

		/* Sampling Environment */
		float NL = max(1e-8, dot( N, L ));
		float NH = max(1e-8, dot( N, tH ));

		float d = (NH * NH) * (a2 - 1.0) + 1.0;
		float pdf = (NH * a2) / (M_PI * d*d);

		float distortion = sqrt(1.0-L.z*L.z); /* Distortion for Equirectangular Env */
		float Lod = precalcLodFactor - 0.5*log2( pdf * distortion);

		if (NL > 0 && pdf > 0)
			node_tex_environment_equirectangular(L, ima, Lod, color);
		else
			continue;

		srgb_to_linearrgb(color, color);
		FilteredColor += color.rgb * NL;
		Weight += NL;

		/* Step 2 Integrating BRDF */
		L = -reflect(V, H);
		NL = max(1e-8, L.z);
		NH = max(1e-8, H.z);
		float VH = max(1e-8, dot( V, H ));

	    float Vis_SmithL = NL + sqrt( NL * (NL - NL * a2) + a2 );
		float Vis = 1.0 / ( Vis_SmithV * Vis_SmithL );

		/* Incident light = nl
		 * pdf = D * nh / (4 * vh)
		 * nl * Vis / pdf */
		float nl_Vis_PDF = NL * Vis * (4.0 * VH / NH);

		float Fc = pow( 1.0 - VH, 5.0 );
		IntegralA += (1.0 - Fc) * nl_Vis_PDF;
		IntegralB += Fc * nl_Vis_PDF;
	}

	IntegralA /= NumSamples;
	IntegralB /= NumSamples;
	FilteredColor /= max( Weight, 0.001 );

	result = FilteredColor*(IntegralA+IntegralB);
}

/* http://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/ */
void irradianceFromSH(vec3 n, vec3 sh0, vec3 sh1, vec3 sh2, vec3 sh3, vec3 sh4, vec3 sh5, vec3 sh6, vec3 sh7, vec3 sh8, out vec4 irradiance)
{
	vec3 sh = vec3(0.0);

	sh += 0.282095 * sh0;

	sh += -0.488603 * n.z * sh1;
	sh += 0.488603 * n.y * sh2;
	sh += -0.488603 * n.x * sh3;

	sh += 1.092548 * n.x * n.z * sh4;
	sh += -1.092548 * n.z * n.y * sh5;
	sh += 0.315392 * (3.0 * n.y * n.y - 1.0) * sh6;
	sh += -1.092548 * n.x * n.y * sh7;
	sh += 0.546274 * (n.x * n.x - n.z * n.z) * sh8;

	irradiance = vec4( sh, 1.0 );
}


/* ********************** matcap style render ******************** */

void material_preview_matcap(vec4 color, sampler2D ima, vec4 N, vec4 mask, out vec4 result)
{
	vec3 normal;
	vec2 tex;
	
#ifndef USE_OPENSUBDIV
	/* remap to 0.0 - 1.0 range. This is done because OpenGL 2.0 clamps colors 
	 * between shader stages and we want the full range of the normal */
	normal = vec3(2.0, 2.0, 2.0) * vec3(N.x, N.y, N.z) - vec3(1.0, 1.0, 1.0);
	if (normal.z < 0.0) {
		normal.z = 0.0;
	}
	normal = normalize(normal);
#else
	normal = inpt.v.normal;
	mask = vec4(1.0, 1.0, 1.0, 1.0);
#endif

	tex.x = 0.5 + 0.49 * normal.x;
	tex.y = 0.5 + 0.49 * normal.y;
	result = texture2D(ima, tex) * mask;
}
