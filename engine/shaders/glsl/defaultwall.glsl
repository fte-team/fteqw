!!ver 110 // 130
!!permu DELUXE
!!permu FULLBRIGHT
!!permu FOG
!!permu LIGHTSTYLED
!!permu BUMP
!!permu SPECULAR
!!permu REFLECTCUBEMASK
!!cvarf r_glsl_offsetmapping_scale
!!cvarf gl_specular

#include "sys/defs.h"

#if GL_VERSION >= 130
#define texture2D texture
#define textureCube texture
#define gl_FragColor gl_FragData[0]
#endif

//this is what normally draws all of your walls, even with rtlights disabled
//note that the '286' preset uses drawflat_walls instead.

#include "sys/fog.h"
#if defined(OFFSETMAPPING) || defined(SPECULAR) || defined(REFLECTCUBEMASK)
varying vec3 eyevector;
#endif

#ifdef REFLECTCUBEMASK
varying mat3 invsurface;
#endif

varying vec2 tc;
#ifdef VERTEXLIT
	varying vec4 vc;
#else
	#ifdef LIGHTSTYLED
		//we could use an offset, but that would still need to be per-surface which would break batches
		//fixme: merge attributes?
		varying vec2 lm0, lm1, lm2, lm3;
	#else
		varying vec2 lm0;
	#endif
#endif

#ifdef VERTEX_SHADER
void main ()
{
#if defined(OFFSETMAPPING) || defined(SPECULAR) || defined(REFLECTCUBEMASK)
	vec3 eyeminusvertex = e_eyepos - v_position.xyz;
	eyevector.x = dot(eyeminusvertex, v_svector.xyz);
	eyevector.y = dot(eyeminusvertex, v_tvector.xyz);
	eyevector.z = dot(eyeminusvertex, v_normal.xyz);
#endif
#ifdef REFLECTCUBEMASK
	invsurface[0] = v_svector;
	invsurface[1] = v_tvector;
	invsurface[2] = v_normal;
#endif
	tc = v_texcoord;
#ifdef VERTEXLIT
	#ifdef LIGHTSTYLED
	//FIXME, only one colour.
	vc = v_colour * e_lmscale[0];
	#else
	vc = v_colour * e_lmscale;
	#endif
#else
	lm0 = v_lmcoord;
#ifdef LIGHTSTYLED
	lm1 = v_lmcoord2;
	lm2 = v_lmcoord3;
	lm3 = v_lmcoord4;
#endif
#endif
	gl_Position = ftetransform();
}
#endif


#ifdef FRAGMENT_SHADER

//samplers
#define s_colourmap	s_t0
uniform sampler2D	s_colourmap;

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
#if GL_VERSION >= 130
	vec2 lmsize = vec2(textureSize(s_lightmap0, 0));
#else
	#define lmsize vec2(128.0,2048.0)
#endif
#define texelstolightmap (16.0)
	vec2 lmcoord0 = floor(lm0 * lmsize*texelstolightmap)/(lmsize*texelstolightmap);
#define lm0 lmcoord0
#endif


//yay, regular texture!
	gl_FragColor = texture2D(s_diffuse, tc);

#if defined(BUMP) && (defined(DELUXE) || defined(SPECULAR) || defined(REFLECTCUBEMASK))
	vec3 norm = normalize(texture2D(s_normalmap, tc).rgb - 0.5);
#elif defined(SPECULAR) || defined(DELUXE) || defined(REFLECTCUBEMASK)
	vec3 norm = vec3(0, 0, 1);	//specular lighting expects this to exist.
#endif

//modulate that by the lightmap(s) including deluxemap(s)
#ifdef VERTEXLIT
	#ifdef LIGHTSTYLED
	vec3 lightmaps = vc.rgb;
	#else
	vec3 lightmaps = vc.rgb;
	#endif
#else
	#ifdef LIGHTSTYLED
		vec3 lightmaps;
		#ifdef DELUXE
			lightmaps  = texture2D(s_lightmap0, lm0).rgb * e_lmscale[0].rgb * dot(norm, 2.0*texture2D(s_deluxmap0, lm0).rgb-0.5);
			lightmaps += texture2D(s_lightmap1, lm1).rgb * e_lmscale[1].rgb * dot(norm, 2.0*texture2D(s_deluxmap1, lm1).rgb-0.5);
			lightmaps += texture2D(s_lightmap2, lm2).rgb * e_lmscale[2].rgb * dot(norm, 2.0*texture2D(s_deluxmap2, lm2).rgb-0.5);
			lightmaps += texture2D(s_lightmap3, lm3).rgb * e_lmscale[3].rgb * dot(norm, 2.0*texture2D(s_deluxmap3, lm3).rgb-0.5);
		#else
			lightmaps  = texture2D(s_lightmap0, lm0).rgb * e_lmscale[0].rgb;
			lightmaps += texture2D(s_lightmap1, lm1).rgb * e_lmscale[1].rgb;
			lightmaps += texture2D(s_lightmap2, lm2).rgb * e_lmscale[2].rgb;
			lightmaps += texture2D(s_lightmap3, lm3).rgb * e_lmscale[3].rgb;
		#endif
	#else
		vec3 lightmaps = (texture2D(s_lightmap, lm0) * e_lmscale).rgb;
		//modulate by the  bumpmap dot light
		#ifdef DELUXE
			vec3 delux = 2.0*(texture2D(s_deluxmap, lm0).rgb-0.5);
			lightmaps *= 1.0 / max(0.25, delux.z);	//counter the darkening from deluxmaps
			lightmaps *= dot(norm, delux);
		#endif
	#endif
#endif

//add in specular, if applicable.
#ifdef SPECULAR
	vec4 specs = texture2D(s_specular, tc);
	#if defined(DELUXE) && !defined(VERTEXLIT)
//not lightstyled...
		vec3 halfdir = normalize(normalize(eyevector) + 2.0*(texture2D(s_deluxmap0, lm0).rgb-0.5));	//this norm should be the deluxemap info instead
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

#ifdef REFLECTCUBEMASK
	vec3 rtc = reflect(-eyevector, norm);
	rtc = rtc.x*invsurface[0] + rtc.y*invsurface[1] + rtc.z*invsurface[2];
	rtc = (m_model * vec4(rtc.xyz,0.0)).xyz;
	gl_FragColor.rgb += texture2D(s_reflectmask, tc).rgb * textureCube(s_reflectcube, rtc).rgb;
#endif

#ifdef EIGHTBIT //FIXME: with this extra flag, half the permutations are redundant.
	lightmaps *= 0.5;	//counter the fact that the colourmap contains overbright values and logically ranges from 0 to 2 intead of to 1.
	float pal = texture2D(s_paletted, tc).r;	//the palette index. hopefully not interpolated.
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

