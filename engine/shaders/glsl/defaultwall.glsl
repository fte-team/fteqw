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
varying vec2 lm0, lm1, lm2, lm3;
#else
varying vec2 lm0;
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
	eyevector.x = dot(eyeminusvertex, v_svector.xyz);
	eyevector.y = dot(eyeminusvertex, v_tvector.xyz);
	eyevector.z = dot(eyeminusvertex, v_normal.xyz);
#endif
	tc = v_texcoord;
	lm0 = v_lmcoord;
#ifdef LIGHTSTYLED
	lm1 = v_lmcoord2;
	lm2 = v_lmcoord3;
	lm3 = v_lmcoord4;
#endif
	gl_Position = ftetransform();
}
#endif


#ifdef FRAGMENT_SHADER

//samplers
#define s_diffuse	s_t0
#define s_lightmap0	s_t1
#define s_normalmap	s_t2
#define s_delux0	s_t3
#define s_fullbright	s_t4
#define s_specular	s_t5
#define s_lightmap1	s_t6
#define s_lightmap2	s_t7
#define s_lightmap3	s_t8
#define s_delux1	s_t9
#define s_delux2	s_t10
#define s_delux3	s_t11
#define s_paletted	s_diffuse
#define s_colourmap	s_fullbright

uniform sampler2D s_diffuse;
uniform sampler2D s_lightmap0;
#if defined(BUMP) && (defined(OFFSETMAPPING) || defined(DELUXE) || defined(SPECULAR))
uniform sampler2D s_normalmap;
#endif
#ifdef DELUXE
uniform sampler2D s_delux0;
#endif
#if defined(FULLBRIGHT) || defined(EIGHTBIT)
uniform sampler2D s_fullbright;
#endif
#ifdef SPECULAR
uniform sampler2D s_specular;
#endif
#ifdef LIGHTSTYLED
uniform sampler2D s_lightmap1;
uniform sampler2D s_lightmap2;
uniform sampler2D s_lightmap3;
uniform sampler2D s_delux1;
uniform sampler2D s_delux2;
uniform sampler2D s_delux3;
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
	vec2 tcoffsetmap = offsetmap(s_normalmap, tc, eyevector);
#define tc tcoffsetmap
#endif

#if defined(EIGHTBIT) && !defined(LIGHTSTYLED)
	//optional: round the lightmap coords to ensure all pixels within a texel have different lighting values either. it just looks wrong otherwise.
	//don't bother if its lightstyled, such cases will have unpredictable correlations anyway.
	//FIXME: this rounding is likely not correct with respect to software rendering. oh well.
	vec2 lmcoord0 = floor(lm0 * 256.0*8.0)/(256.0*8.0);
#define lm0 lmcoord0
#endif


//yay, regular texture!
	gl_FragColor = texture2D(s_diffuse, tc);

#if defined(BUMP) && (defined(DELUXE) || defined(SPECULAR))
	vec3 norm = normalize(texture2D(s_normalmap, tc).rgb - 0.5);
#elif defined(SPECULAR) || defined(DELUXE)
	vec3 norm = vec3(0, 0, 1);	//specular lighting expects this to exist.
#endif

//modulate that by the lightmap(s) including deluxemap(s)
#ifdef LIGHTSTYLED
	vec3 lightmaps;
	#ifdef DELUXE
		lightmaps  = texture2D(s_lightmap0, lm0).rgb * e_lmscale[0].rgb * dot(norm, 2.0*texture2D(s_delux0, lm0).rgb-0.5);
		lightmaps += texture2D(s_lightmap1, lm1).rgb * e_lmscale[1].rgb * dot(norm, 2.0*texture2D(s_delux1, lm1).rgb-0.5);
		lightmaps += texture2D(s_lightmap2, lm2).rgb * e_lmscale[2].rgb * dot(norm, 2.0*texture2D(s_delux2, lm2).rgb-0.5);
		lightmaps += texture2D(s_lightmap3, lm3).rgb * e_lmscale[3].rgb * dot(norm, 2.0*texture2D(s_delux3, lm3).rgb-0.5);
	#else
		lightmaps  = texture2D(s_lightmap0, lm0).rgb * e_lmscale[0].rgb;
		lightmaps += texture2D(s_lightmap1, lm1).rgb * e_lmscale[1].rgb;
		lightmaps += texture2D(s_lightmap2, lm2).rgb * e_lmscale[2].rgb;
		lightmaps += texture2D(s_lightmap3, lm3).rgb * e_lmscale[3].rgb;
	#endif
#else
	vec3 lightmaps = (texture2D(s_lightmap0, lm0) * e_lmscale).rgb;
	//modulate by the  bumpmap dot light
	#ifdef DELUXE
		lightmaps *= dot(norm, 2.0*(texture2D(s_delux0, lm0).rgb-0.5));
	#endif
#endif

//add in specular, if applicable.
#ifdef SPECULAR
	vec4 specs = texture2D(s_specular, tc);
	#ifdef DELUXE
//not lightstyled...
		vec3 halfdir = normalize(normalize(eyevector) + 2.0*(texture2D(s_delux0, lm0).rgb-0.5));	//this norm should be the deluxemap info instead
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

#ifdef EIGHTBIT //FIXME: with this extra flag, half the permutations are redundant.
	lightmaps *= 0.5;	//counter the fact that the colourmap contains overbright values and logically ranges from 0 to 2 intead of to 1.
	float pal = texture2D(s_diffuse, tc).r;	//the palette index. hopefully not interpolated.
	lightmaps -= 1.0 / 128.0;	//software rendering appears to round down, so make sure we favour the lower values instead of rounding to the nearest
	gl_FragColor.r = texture2D(s_colourmap, vec2(pal, 1.0-lightmaps.r)).r;	//do 3 lookups. this is to cope with lit files, would be a waste to not support those.
	gl_FragColor.g = texture2D(s_colourmap, vec2(pal, 1.0-lightmaps.g)).g;	//its not very softwarey, but re-palettizing is ugly.
	gl_FragColor.b = texture2D(s_colourmap, vec2(pal, 1.0-lightmaps.b)).b;	//without lits, it should be identical.
#else
	//now we have our diffuse+specular terms, modulate by lightmap values.
	gl_FragColor.rgb *= lightmaps.rgb;

//add on the fullbright
#ifdef FULLBRIGHT
	gl_FragColor.rgb += texture2D(s_fullbright, tc).rgb;
#endif
#endif


//entity modifiers
	gl_FragColor = gl_FragColor * e_colourident;

//and finally hide it all if we're fogged.
#ifdef FOG
	gl_FragColor = fog4(gl_FragColor);
#endif
}
#endif
