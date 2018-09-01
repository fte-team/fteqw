!!cvard_srgb_b r_floorcolor
!!cvard_srgb_b r_wallcolor
!!permu FOG

//this is for the '286' preset walls, and just draws lightmaps coloured based upon surface normals.

#include "sys/fog.h"
varying vec4 col;
#ifdef VERTEX_SHADER
attribute vec3 v_normal;
attribute vec2 v_lmcoord;
varying vec2 lm;
uniform vec4 e_lmscale;
void main ()
{
#ifdef LM
	col = vec4(1.0);
#else
	col = vec4(e_lmscale.rgb * ((v_normal.z < 0.73)?r_wallcolor:r_floorcolor), e_lmscale.a);
#endif
	lm = v_lmcoord;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;
varying vec2 lm;
void main ()
{
	gl_FragColor = fog4(col * texture2D(s_t0, lm));
}
#endif
