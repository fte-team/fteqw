!!cvarf r_wateralpha
!!permu FOG

//this is the shader that's responsible for drawing default q1 turbulant water surfaces
//this is expected to be moderately fast.

#include "sys/fog.h"
varying vec2 tc;
#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
void main ()
{
	tc = v_texcoord.st;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;
uniform float e_time;
#ifndef ALPHA
uniform float cvar_r_wateralpha;
#define USEALPHA cvar_r_wateralpha
#else
#define USEALPHA float(ALPHA)
#endif
void main ()
{
	vec2 ntc;
	ntc.s = tc.s + sin(tc.t+e_time)*0.125;
	ntc.t = tc.t + sin(tc.s+e_time)*0.125;
	vec3 ts = vec3(texture2D(s_t0, ntc));
	gl_FragColor = fog4(vec4(ts, USEALPHA));
}
#endif
