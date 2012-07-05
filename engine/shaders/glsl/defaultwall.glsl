!!permu OFFSETMAPPING
!!permu FULLBRIGHT
!!permu FOG
!!permu LIGHTSTYLED
!!cvarf r_glsl_offsetmapping_scale

//this is what normally draws all of your walls, even with rtlights disabled
//note that the '286' preset uses drawflat_walls instead.

#include "sys/fog.h"
#if defined(OFFSETMAPPING)
varying vec3 eyevector;
#endif

varying vec2 tc;
#ifdef LIGHTSTYLED
//we could use an offset, but that would still need to be per-surface which would break batches
//fixme: merge attributes?
varying vec2 lm, lm2, lm3, lm4;
#else
varying vec2 lm;
#endif

#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
attribute vec2 v_lmcoord;
#ifdef LIGHTSTYLED
attribute vec2 v_lmcoord2;
attribute vec2 v_lmcoord3;
attribute vec2 v_lmcoord4;
#endif
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
	eyevector.x = -dot(eyeminusvertex, v_svector.xyz);
	eyevector.y = dot(eyeminusvertex, v_tvector.xyz);
	eyevector.z = dot(eyeminusvertex, v_normal.xyz);
#endif
	tc = v_texcoord;
	lm = v_lmcoord;
#ifdef LIGHTSTYLED
	lm2 = v_lmcoord2;
	lm3 = v_lmcoord3;
	lm4 = v_lmcoord4;
#endif
	gl_Position = ftetransform();
}
#endif


#ifdef FRAGMENT_SHADER
//samplers
uniform sampler2D s_t0;
uniform sampler2D s_t1;
#ifdef OFFSETMAPPING
uniform sampler2D s_t2;
#endif
#ifdef FULLBRIGHT
uniform sampler2D s_t4;
#endif
#ifdef LIGHTSTYLED
uniform sampler2D s_t5;
uniform sampler2D s_t6;
uniform sampler2D s_t7;
#endif

#ifdef LIGHTSTYLED
uniform vec4 e_lmscale[4];
#else
uniform vec4 e_lmscale;
#endif
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
	gl_FragColor = texture2D(s_t0, tc);
#ifdef LIGHTSTYLED
	vec4 lightmaps;
	lightmaps  = texture2D(s_t1, lm ) * e_lmscale[0];
	lightmaps += texture2D(s_t5, lm2) * e_lmscale[1];
	lightmaps += texture2D(s_t6, lm3) * e_lmscale[2];
	lightmaps += texture2D(s_t7, lm4) * e_lmscale[3];
	gl_FragColor.rgb *= lightmaps.rgb;
#else
	gl_FragColor.rgb *= texture2D(s_t1, lm) * e_lmscale;
#endif

#ifdef FULLBRIGHT
	gl_FragColor.rgb += texture2D(s_t4, tc).rgb;
#endif
	gl_FragColor = gl_FragColor * e_colourident;
#ifdef FOG
	gl_FragColor = fog4(gl_FragColor);
#endif
}
#endif
