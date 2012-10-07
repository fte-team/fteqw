!!permu DELUXE
!!permu FULLBRIGHT
!!permu FOG
!!permu LIGHTSTYLED
!!permu BUMP
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
uniform sampler2D s_t0;	//diffuse
uniform sampler2D s_t1;	//lightmap0
#if defined(OFFSETMAPPING) || defined(DELUXE)
uniform sampler2D s_t2;	//normal
#endif
uniform sampler2D s_t3;	//deluxe0
#ifdef FULLBRIGHT
uniform sampler2D s_t4;	//fullbright
#endif
#ifdef LIGHTSTYLED
uniform sampler2D s_t5;	//lightmap1
uniform sampler2D s_t6;	//lightmap2
uniform sampler2D s_t7;	//lightmap3
uniform sampler2D s_t8;	//deluxe1
uniform sampler2D s_t9;	//deluxe2
uniform sampler2D s_t10;	//deluxe3
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
//adjust texture coords for offsetmapping
#ifdef OFFSETMAPPING
	vec2 tcoffsetmap = offsetmap(s_t2, tc, eyevector);
#define tc tcoffsetmap
#endif

//yay, regular texture!
	gl_FragColor = texture2D(s_t0, tc);

//modulate that by the lightmap(s) including deluxemap(s)
#ifdef LIGHTSTYLED
	vec4 lightmaps;
	#ifdef DELUXE
		vec3 norm = texture2D(s_t2, tc).rgb;
		lightmaps  = texture2D(s_t1, lm ) * e_lmscale[0] * dot(norm, texture2D(s_t3, lm ));
		lightmaps += texture2D(s_t5, lm2) * e_lmscale[1] * dot(norm, texture2D(s_t8, lm2));
		lightmaps += texture2D(s_t6, lm3) * e_lmscale[2] * dot(norm, texture2D(s_t9, lm3));
		lightmaps += texture2D(s_t7, lm4) * e_lmscale[3] * dot(norm, texture2D(s_t10,lm4));
	#else
		lightmaps  = texture2D(s_t1, lm ) * e_lmscale[0];
		lightmaps += texture2D(s_t5, lm2) * e_lmscale[1];
		lightmaps += texture2D(s_t6, lm3) * e_lmscale[2];
		lightmaps += texture2D(s_t7, lm4) * e_lmscale[3];
	#endif
	gl_FragColor.rgb *= lightmaps.rgb;
#else
	#ifdef DELUXE
//gl_FragColor.rgb = dot(normalize(texture2D(s_t2, tc).rgb - 0.5), normalize(texture2D(s_t3, lm).rgb - 0.5));
//gl_FragColor.rgb = texture2D(s_t3, lm).rgb;
		gl_FragColor.rgb *= (texture2D(s_t1, lm) * e_lmscale).rgb * dot(normalize(texture2D(s_t2, tc).rgb-0.5), 2.0*(texture2D(s_t3, lm).rgb-0.5));
	#else
		gl_FragColor.rgb *= (texture2D(s_t1, lm) * e_lmscale).rgb;
	#endif
#endif

//add on the fullbright
#ifdef FULLBRIGHT
	gl_FragColor.rgb += texture2D(s_t4, tc).rgb;
#endif

//entity modifiers
	gl_FragColor = gl_FragColor * e_colourident;

//and finally hide it all if we're fogged.
#ifdef FOG
	gl_FragColor = fog4(gl_FragColor);
#endif
}
#endif
