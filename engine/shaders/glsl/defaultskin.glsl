!!ver 100 130
!!permu FULLBRIGHT
!!permu UPPERLOWER
!!permu FRAMEBLEND
!!permu SKELETAL
!!permu FOG
!!permu BUMP
!!cvarf r_glsl_offsetmapping_scale
!!cvarf gl_specular
!!cvardf gl_affinemodels=0

#include "sys/defs.h"

//standard shader used for models.
//must support skeletal and 2-way vertex blending or Bad Things Will Happen.
//the vertex shader is responsible for calculating lighting values.

#if gl_affinemodels==1 && __VERSION__ >= 130
noperspective
#endif
	varying vec2 tc;
varying vec3 light;
#if defined(SPECULAR) || defined(OFFSETMAPPING)
varying vec3 eyevector;
#endif




#ifdef VERTEX_SHADER
#include "sys/skeletal.h"
void main ()
{
#if defined(SPECULAR)||defined(OFFSETMAPPING)
	vec3 n, s, t, w;
	gl_Position = skeletaltransform_wnst(w,n,s,t);
	vec3 eyeminusvertex = e_eyepos - w.xyz;
	eyevector.x = dot(eyeminusvertex, s.xyz);
	eyevector.y = dot(eyeminusvertex, t.xyz);
	eyevector.z = dot(eyeminusvertex, n.xyz);
#else
	vec3 n;
	gl_Position = skeletaltransform_n(n);
#endif

	float d = dot(n,e_light_dir);
	if (d < 0.0)		//vertex shader. this might get ugly, but I don't really want to make it per vertex.
		d = 0.0;	//this avoids the dark side going below the ambient level.
	light = e_light_ambient + (dot(n,e_light_dir)*e_light_mul);
	tc = v_texcoord;
}
#endif
#ifdef FRAGMENT_SHADER

#include "sys/fog.h"

#if defined(SPECULAR)
uniform float cvar_gl_specular;
#endif

#ifdef OFFSETMAPPING
#include "sys/offsetmapping.h"
#endif

#ifdef EIGHTBIT
#define s_colourmap s_t0
uniform sampler2D s_colourmap;
#endif

#if __VERSION__ >= 130
#define gl_FragColor thecolour
out vec4 thecolour;
#endif


void main ()
{
	vec4 col, sp;

#ifdef OFFSETMAPPING
	vec2 tcoffsetmap = offsetmap(s_normalmap, tc, eyevector);
#define tc tcoffsetmap
#endif

#ifdef EIGHTBIT
	vec3 lightlev = light;
	//FIXME: with this extra flag, half the permutations are redundant.
	lightlev *= 0.5;	//counter the fact that the colourmap contains overbright values and logically ranges from 0 to 2 intead of to 1.
	float pal = texture2D(s_paletted, tc).r;	//the palette index. hopefully not interpolated.
	lightlev -= 1.0 / 128.0;	//software rendering appears to round down, so make sure we favour the lower values instead of rounding to the nearest
	col.r = texture2D(s_colourmap, vec2(pal, 1.0-lightlev.r)).r;	//do 3 lookups. this is to cope with lit files, would be a waste to not support those.
	col.g = texture2D(s_colourmap, vec2(pal, 1.0-lightlev.g)).g;	//its not very softwarey, but re-palettizing is ugly.
	col.b = texture2D(s_colourmap, vec2(pal, 1.0-lightlev.b)).b;	//without lits, it should be identical.
	col.a = (pal<1.0)?1.0:0.0;
#else
	col = texture2D(s_diffuse, tc);
	#ifdef UPPER
		vec4 uc = texture2D(s_upper, tc);
		col.rgb += uc.rgb*e_uppercolour*uc.a;
	#endif
	#ifdef LOWER
		vec4 lc = texture2D(s_lower, tc);
		col.rgb += lc.rgb*e_lowercolour*lc.a;
	#endif

	#if defined(BUMP) && defined(SPECULAR)
		vec3 bumps = normalize(vec3(texture2D(s_normalmap, tc)) - 0.5);
		vec4 specs = texture2D(s_specular, tc);

		vec3 halfdir = normalize(normalize(eyevector) + vec3(0.0, 0.0, 1.0));
		float spec = pow(max(dot(halfdir, bumps), 0.0), 32.0 * specs.a);
		col.rgb += cvar_gl_specular * spec * specs.rgb;
	#endif

	col.rgb *= light;

	#ifdef FULLBRIGHT
		vec4 fb = texture2D(s_fullbright, tc);
//		col.rgb = mix(col.rgb, fb.rgb, fb.a);
		col.rgb += fb.rgb * fb.a;
	#endif
#endif

	gl_FragColor = fog4(col * e_colourident);
}
#endif

