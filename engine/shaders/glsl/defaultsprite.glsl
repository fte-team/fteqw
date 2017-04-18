!!permu FOG
//used by both particles and sprites.
//note the fog blending mode is all that differs from defaultadditivesprite

#include "sys/fog.h"
#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
attribute vec4 v_colour;
varying vec2 tc;
varying vec4 vc;
void main ()
{
	tc = v_texcoord;
	vc = v_colour;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;
varying vec2 tc;
varying vec4 vc;
uniform vec4 e_colourident;
uniform vec4 e_lmscale;
void main ()
{
	vec4 col = texture2D(s_t0, tc);
#ifdef MASK
	if (col.a < float(MASK))
		discard;
#endif
	gl_FragColor = fog4blend(col * vc * e_colourident * e_lmscale);
}
#endif
