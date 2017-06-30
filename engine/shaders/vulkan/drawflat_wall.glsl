!!permu FOG
!!cvar3f r_floorcolor
!!cvar3f r_wallcolor
!!cvarb r_fog_exp2=true
!!samps 1
#include "sys/defs.h"

//this is for the '286' preset walls, and just draws lightmaps coloured based upon surface normals.

#include "sys/fog.h"
varying vec4 col;
#ifdef VERTEX_SHADER
//attribute vec3 v_normal;
//attribute vec2 v_lmcoord;
varying vec2 lm;
//uniform vec3 cvar_r_wallcolor;
//uniform vec3 cvar_r_floorcolor;
//uniform vec4 e_lmscale;
void main ()
{
	col = vec4(e_lmscale.rgb/255.0 * ((v_normal.z < 0.73)?cvar_r_wallcolor:cvar_r_floorcolor), e_lmscale.a);
	lm = v_lmcoord;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
//uniform sampler2D s_t0;
varying vec2 lm;
void main ()
{
	gl_FragColor = fog4(col * texture2D(s_t0, lm));
}
#endif
