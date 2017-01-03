uniform sampler2D depthtex;
uniform ivec2 screenres;

#if __VERSION__ == 120
#define fragColor gl_FragColor
#define fragDepth gl_FragDepth
#else
out vec4 fragColor;
out float fragDepth;
#endif

void main() {
	fragDepth = texture2D(depthtex, vec2(gl_FragCoord.xy*2.0) / vec2(screenres)).r;
	fragColor = vec4(fragDepth, 0., 0.,1.0);
}