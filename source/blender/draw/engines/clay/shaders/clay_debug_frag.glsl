uniform sampler2D depthtex;
uniform ivec2 screenres;

#if __VERSION__ == 120
#define fragColor gl_FragColor
#else
out vec4 fragColor;
#endif

void main() {
	//fragColor = vec4(texture2D(depthtex, vec2(gl_FragCoord.xy) / vec2(screenres)).rgb, 1.0);
	fragColor = vec4(texture2D(depthtex, vec2(gl_FragCoord.xy) / vec2(screenres)).rrr, 1.0);
	//fragColor = vec4(.5,.5,.0, 1.0);
}