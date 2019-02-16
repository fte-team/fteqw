!!ver 100 450
!!permu FOG
!!cvarf r_wateralpha
!!samps diffuse lightmap

#include "sys/defs.h"

//this is the shader that's responsible for drawing default q1 turbulant water surfaces
//this is expected to be moderately fast.

#include "sys/fog.h"
varying vec2 tc;
#ifdef LIT
varying vec2 lm0;
#endif
#ifdef VERTEX_SHADER
void main ()
{
	tc = v_texcoord.st;
	#ifdef FLOW
	tc.s += e_time * -0.5;
	#endif
	#ifdef LIT
	lm0 = v_lmcoord;
	#endif
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
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
	vec3 ts = vec3(texture2D(s_diffuse, ntc));

#ifdef LIT
	ts *= (texture2D(s_lightmap, lm0) * e_lmscale).rgb;
#endif

	gl_FragColor = fog4blend(vec4(ts, USEALPHA) * e_colourident);
}
#endif
