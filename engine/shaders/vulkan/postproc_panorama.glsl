!!cvarf ffov
!!samps reflectcube
#include "sys/defs.h"

//panoramic view rendering, for promo map shots or whatever.

#ifdef VERTEX_SHADER
varying vec2 texcoord;
void main()
{
	texcoord = v_texcoord.xy;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
varying vec2 texcoord;
void main()
{
	vec3 tc;	
	float ang;	
	ang = texcoord.x*radians(cvar_ffov);	
	tc.x = sin(ang);	
	tc.y = -texcoord.y;	
	tc.z = cos(ang);	
	gl_FragColor = textureCube(s_reflectcube, tc);
}
#endif
