!!permu DELUXE
!!permu FULLBRIGHT
!!permu FOG
!!permu LIGHTSTYLED
!!permu BUMP
!!permu SPECULAR
!!cvarf r_glsl_offsetmapping_scale
!!cvarf gl_specular

//this is what normally draws all of your walls, even with rtlights disabled
//note that the '286' preset uses drawflat_walls instead.

#include "sys/fog.h"
#if defined(OFFSETMAPPING) || defined(SPECULAR)
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
#if defined(OFFSETMAPPING) || defined(SPECULAR)
uniform vec3 e_eyepos;
attribute vec3 v_normal;
attribute vec3 v_svector;
attribute vec3 v_tvector;
#endif
void main ()
{
#if defined(OFFSETMAPPING) || defined(SPECULAR)
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
#if defined(BUMP) && (defined(OFFSETMAPPING) || defined(DELUXE) || defined(SPECULAR))
uniform sampler2D s_t2;	//normal.rgb+height.a
#endif
#ifdef DELUXE
uniform sampler2D s_t3;	//deluxe0
#endif
#ifdef FULLBRIGHT
uniform sampler2D s_t4;	//fullbright
#endif
#ifdef SPECULAR
uniform sampler2D s_t5;	//specular
#endif
#ifdef LIGHTSTYLED
uniform sampler2D s_t6;	//lightmap1
uniform sampler2D s_t7;	//lightmap2
uniform sampler2D s_t8;	//lightmap3
uniform sampler2D s_t9;	//deluxe1
uniform sampler2D s_t10;	//deluxe2
uniform sampler2D s_t11;	//deluxe3
#endif

#ifdef LIGHTSTYLED
uniform vec4 e_lmscale[4];
#else
uniform vec4 e_lmscale;
#endif
uniform vec4 e_colourident;
#ifdef SPECULAR
uniform float cvar_gl_specular;
#endif
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

#if defined(BUMP) && (defined(DELUXE) || defined(SPECULAR))
	vec3 norm = normalize(texture2D(s_t2, tc).rgb - 0.5);
#elif defined(SPECULAR) || defined(DELUXE)
	vec3 norm = vec3(0, 0, 1);	//specular lighting expects this to exist.
#endif

//modulate that by the lightmap(s) including deluxemap(s)
#ifdef LIGHTSTYLED
	vec3 lightmaps;
	#ifdef DELUXE
		lightmaps  = texture2D(s_t1, lm ).rgb * e_lmscale[0].rgb * dot(norm, texture2D(s_t3, lm ).rgb);
		lightmaps += texture2D(s_t6, lm2).rgb * e_lmscale[1].rgb * dot(norm, texture2D(s_t9, lm2).rgb);
		lightmaps += texture2D(s_t7, lm3).rgb * e_lmscale[2].rgb * dot(norm, texture2D(s_t10, lm3).rgb);
		lightmaps += texture2D(s_t8, lm4).rgb * e_lmscale[3].rgb * dot(norm, texture2D(s_t11,lm4).rgb);
	#else
		lightmaps  = texture2D(s_t1, lm ).rgb * e_lmscale[0].rgb;
		lightmaps += texture2D(s_t6, lm2).rgb * e_lmscale[1].rgb;
		lightmaps += texture2D(s_t7, lm3).rgb * e_lmscale[2].rgb;
		lightmaps += texture2D(s_t8, lm4).rgb * e_lmscale[3].rgb;
	#endif
#else
	vec3 lightmaps = (texture2D(s_t1, lm) * e_lmscale).rgb;
	//modulate by the  bumpmap dot light
	#ifdef DELUXE
		lightmaps *= dot(norm, 2.0*(texture2D(s_t3, lm).rgb-0.5));
	#endif
#endif

#ifdef SPECULAR
	vec4 specs = texture2D(s_t5, tc);
	#ifdef DELUXE
//not lightstyled...
		vec3 halfdir = normalize(normalize(eyevector) + 2.0*(texture2D(s_t3, lm).rgb-0.5));	//this norm should be the deluxemap info instead
	#else
		vec3 halfdir = normalize(normalize(eyevector) + vec3(0.0, 0.0, 1.0));	//this norm should be the deluxemap info instead
	#endif
	float spec = pow(max(dot(halfdir, norm), 0.0), 32.0 * specs.a);
	spec *= cvar_gl_specular;
//NOTE: rtlights tend to have a *4 scaler here to over-emphasise the effect because it looks cool.
//As not all maps will have deluxemapping, and the double-cos from the light util makes everything far too dark anyway,
//we default to something that is not garish when the light value is directly infront of every single pixel.
//we can justify this difference due to the rtlight editor etc showing the *4.
	gl_FragColor.rgb += spec * specs.rgb;
#endif

	//now we have our diffuse+specular terms, modulate by lightmap values.
	gl_FragColor.rgb *= lightmaps.rgb;

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
