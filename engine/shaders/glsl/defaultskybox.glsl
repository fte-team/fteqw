!!permu FOG
!!samps reflectcube
!!cvardf r_skyfog=0.5
#include "sys/defs.h"
#include "sys/fog.h"

//simple shader for simple skyboxes.

varying vec3 pos;
#ifdef VERTEX_SHADER
void main ()
{
	pos = v_position.xyz - e_eyepos;
	pos.y = -pos.y;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
void main ()
{
	vec4 skybox = textureCube(s_reflectcube, pos);
	gl_FragColor = vec4(mix(skybox.rgb, fog3(skybox.rgb), float(r_skyfog)), 1.0);
}
#endif
