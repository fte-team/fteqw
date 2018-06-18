!!permu FOG
!!cvarf r_wateralpha
!!cvarb r_fog_exp2=true
!!argf alpha=0
!!samps diffuse

#include "sys/defs.h"

//this is the shader that's responsible for drawing default q1 turbulant water surfaces
//this is expected to be moderately fast.

#include "sys/fog.h"
layout(location=0) varying vec2 tc;

#ifdef VERTEX_SHADER
void main ()
{
	tc = v_texcoord.st;
	#ifdef FLOW
	tc.s += e_time * -0.5;
	#endif
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
void main ()
{
	vec2 ntc;
	ntc.s = tc.s + sin(tc.t+e_time)*0.125;
	ntc.t = tc.t + sin(tc.s+e_time)*0.125;
	vec3 ts = vec3(texture2D(s_diffuse, ntc));
	if (arg_alpha != 0.0)
		gl_FragColor = fog4(vec4(ts, arg_alpha));
	else
		gl_FragColor = fog4(vec4(ts, cvar_r_wateralpha));
}
#endif
