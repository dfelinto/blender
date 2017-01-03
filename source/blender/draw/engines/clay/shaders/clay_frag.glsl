uniform vec4 color;
uniform ivec2 screenres;

#if __VERSION__ == 120
varying vec3 normal;
#define fragColor gl_FragColor
#else
in vec3 normal;
out vec4 fragColor;
#endif


void main() {
	vec3 n = normalize(normal);
	vec3 v = normalize(vec3(1.,1.,.5));
	float vn = dot(v,n) * .5 + .5;
	vec3 col = vn * vec3(1.,.3,.1);

	fragColor = vec4(1.0, 0.0, 1.0,1.0);
	//fragColor = vec4(gl_FragCoord.xy / vec2(screenres), 0.0, 1.0);
}