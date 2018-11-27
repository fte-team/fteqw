!!permu FOG
!!samps 2
#include "sys/fog.h"

//regular sky shader for scrolling q1 skies
//the sky surfaces are thrown through this as-is.

#ifdef VERTEX_SHADER
varying vec3 pos;
void main ()
{
	pos = v_position.xyz;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform float e_time;
uniform vec3 e_eyepos;
varying vec3 pos;
void main ()
{
	vec2 tccoord;
	vec3 dir = pos - e_eyepos;
	dir.z *= 3.0;
	dir.xy /= 0.5*length(dir);
	tccoord = (dir.xy + e_time*0.03125);
	vec3 solid = vec3(texture2D(s_t0, tccoord));
	tccoord = (dir.xy + e_time*0.0625);
	vec4 clouds = texture2D(s_t1, tccoord);
	gl_FragColor = vec4(fog3((solid.rgb*(1.0-clouds.a)) + (clouds.a*clouds.rgb)), 1.0);
}
#endif
