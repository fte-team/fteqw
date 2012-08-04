!!permu FOG
#include "sys/fog.h"
varying vec2 tc;
varying vec2 lm;
varying vec4 vc;

#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
attribute vec2 v_lmcoord;
attribute vec4 v_colour;
void main (void)
{
	tc = v_texcoord.st;
	lm = v_lmcoord.st;
	vc = v_colour;
	gl_Position = ftetransform();
}
#endif




#ifdef FRAGMENT_SHADER
//four texture passes
uniform sampler2D s_t0;
uniform sampler2D s_t1;
uniform sampler2D s_t2;
uniform sampler2D s_t3;

//mix values
uniform sampler2D s_t4;


void main (void)
{
	vec4 m = texture2D(s_t4, lm);

	gl_FragColor = fog4(vc*vec4(m.aaa,1.0)*(
		texture2D(s_t0, tc)*m.r
		+ texture2D(s_t1, tc)*m.g
		+ texture2D(s_t2, tc)*m.b
		+ texture2D(s_t3, tc)*(1.0 - (m.r + m.g + m.b))
		));
}
#endif