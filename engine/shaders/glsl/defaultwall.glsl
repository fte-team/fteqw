!!permu OFFSETMAPPING
!!permu FULLBRIGHT
!!permu FOG
!!cvarf r_glsl_offsetmapping_scale

//this is what normally draws all of your walls, even with rtlights disabled
//note that the '286' preset uses drawflat_walls instead.

#include "sys/fog.h"
#if defined(OFFSETMAPPING)
varying vec3 eyevector;
#endif
#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
attribute vec2 v_lmcoord;
varying vec2 tc, lm;
#if defined(OFFSETMAPPING)
uniform vec3 e_eyepos;
attribute vec3 v_normal;
attribute vec3 v_svector;
attribute vec3 v_tvector;
#endif
void main ()
{
#if defined(OFFSETMAPPING)
	vec3 eyeminusvertex = e_eyepos - v_position.xyz;
	eyevector.x = dot(eyeminusvertex, v_svector.xyz);
	eyevector.y = -dot(eyeminusvertex, v_tvector.xyz);
	eyevector.z = dot(eyeminusvertex, v_normal.xyz);
#endif
	tc = v_texcoord;
	lm = v_lmcoord;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;
uniform sampler2D s_t1;
#ifdef OFFSETMAPPING
uniform sampler2D s_t2;
#endif
#ifdef FULLBRIGHT
uniform sampler2D s_t4;
#endif
varying vec2 tc, lm;
uniform vec4 e_lmscale;
uniform vec4 e_colourident;
#ifdef OFFSETMAPPING
#include "sys/offsetmapping.h"
#endif
void main ()
{
#ifdef OFFSETMAPPING
	vec2 tcoffsetmap = offsetmap(s_t2, tc, eyevector);
#define tc tcoffsetmap
#endif
	gl_FragColor = texture2D(s_t0, tc) * texture2D(s_t1, lm) * e_lmscale;
#ifdef FULLBRIGHT
	gl_FragColor.rgb += texture2D(s_t4, tc).rgb;
#endif
	gl_FragColor = gl_FragColor * e_colourident;
#ifdef FOG
	gl_FragColor = fog4(gl_FragColor);
#endif
}
#endif
