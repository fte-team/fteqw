!!samps screen=0

/*  Emulates visual artifacts from video cards used by old mods and games, by eukara */

#include "sys/defs.h"
varying vec2 texcoord;

#ifdef VERTEX_SHADER
void main()
{
	texcoord = v_texcoord.xy;
	texcoord.y = 1.0 - texcoord.y;
	gl_Position = ftetransform();
}
#endif

#ifdef FRAGMENT_SHADER
uniform vec2 e_sourcesize;

vec3 p_dither(vec3 col)
{
	float neighbour1 = mod(gl_FragCoord.x,  1.0) * 0.05;
	float neighbour2 = mod(-gl_FragCoord.x, 2.0) * 0.05;
	float neighbour3 = mod(gl_FragCoord.y,  1.0) * 0.05;
	float neighbour4 = mod(-gl_FragCoord.y, 2.0) * 0.05;
	float apprx = 0.945 - neighbour1 + neighbour2 + neighbour3 + neighbour4;

	col.r = pow(col.r, 1.0 / apprx);
	col.g = pow(col.g, 1.0 / apprx);
	col.b = pow(col.b, 1.0 / apprx);
	return col.rgb;
}
vec3 p_gamma(vec3 col)
{
	float gammaed = 0.11;
	float linegamma = gammaed;
	float coord = 1.0 - texcoord.y
;

 	float lines = mod(gl_FragCoord.y, 2.0);

	if (lines < 1.0) {
		linegamma = 0.0;
	}

	float gamma = 1.3 - gammaed + linegamma;

	col.r = pow(col.r, 1.0 / gamma);
	col.g = pow(col.g, 1.0 / gamma);
	col.b = pow(col.b, 1.0 / gamma);
	return col;
}

void main(void)
{
	vec3 col = texture2D(s_screen, floor(texcoord.xy * e_sourcesize)/e_sourcesize).rgb;

	/* Slow and messy, combine into one call */
	col = p_gamma(col);
	col = p_gamma(col);
	col = mix(col, p_gamma(col), 0.25);

	/* Dither comes last */
	col = p_dither(col);
	
	/* 24 to 16 */
	col.rgb = floor(col.rgb * vec3(64,128,64))/vec3(64,128,64);

	gl_FragColor = vec4(col, 1.0);
}
#endif
