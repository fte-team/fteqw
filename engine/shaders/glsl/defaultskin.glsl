!!permu FULLBRIGHT
!!permu UPPERLOWER
!!permu FRAMEBLEND
!!permu SKELETAL
!!permu FOG
!!cvarf r_glsl_offsetmapping_scale
!!cvarf gl_specular

//standard shader used for models.
//must support skeletal and 2-way vertex blending or Bad Things Will Happen.
//the vertex shader is responsible for calculating lighting values.

#ifdef UPPERLOWER
#define UPPER
#define LOWER
#endif

varying vec2 tc;
varying vec3 light;
#if defined(SPECULAR) || defined(OFFSETMAPPING)
varying vec3 eyevector;
#endif




#ifdef VERTEX_SHADER
#include "sys/skeletal.h"
attribute vec2 v_texcoord;
uniform vec3 e_light_dir;
uniform vec3 e_light_mul;
uniform vec3 e_light_ambient;
#if defined(SPECULAR) || defined(OFFSETMAPPING)
uniform vec3 e_eyepos;
#endif
void main ()
{
#if defined(SPECULAR)||defined(OFFSETMAPPING)
	vec3 n, s, t, w;
	gl_Position = skeletaltransform_wnst(w,n,s,t);
	vec3 eyeminusvertex = e_eyepos - w.xyz;
	eyevector.x = -dot(eyeminusvertex, s.xyz);
	eyevector.y = dot(eyeminusvertex, t.xyz);
	eyevector.z = dot(eyeminusvertex, n.xyz);
#else
	vec3 n;
	gl_Position = skeletaltransform_n(n);
#endif

	light = e_light_ambient + (dot(n,e_light_dir)*e_light_mul);
	tc = v_texcoord;
}
#endif
#ifdef FRAGMENT_SHADER
#include "sys/fog.h"
uniform sampler2D s_t0;
#ifdef LOWER
uniform sampler2D s_t1;
uniform vec3 e_lowercolour;
#endif
#ifdef UPPER
uniform sampler2D s_t2;
uniform vec3 e_uppercolour;
#endif
#ifdef FULLBRIGHT
uniform sampler2D s_t3;
#endif

#if defined(SPECULAR)
uniform sampler2D s_t4;
uniform sampler2D s_t5;
uniform float cvar_gl_specular;
#endif

#ifdef OFFSETMAPPING
#include "sys/offsetmapping.h"
#endif



uniform vec4 e_colourident;
void main ()
{
	vec4 col, sp;

#ifdef OFFSETMAPPING
	vec2 tcoffsetmap = offsetmap(s_t1, tcbase, eyevector);
#define tc tcoffsetmap
#endif

	col = texture2D(s_t0, tc);
#ifdef UPPER
	vec4 uc = texture2D(s_t2, tc);
	col.rgb += uc.rgb*e_uppercolour*uc.a;
#endif
#ifdef LOWER
	vec4 lc = texture2D(s_t1, tc);
	col.rgb += lc.rgb*e_lowercolour*lc.a;
#endif
	col.rgb *= light;

#if defined(SPECULAR)
	vec3 bumps = normalize(vec3(texture2D(s_t4, tc)) - 0.5);
	vec4 specs = texture2D(s_t5, tc);

	vec3 halfdir = normalize(normalize(eyevector) + vec3(0.0, 0.0, 1.0));
	float spec = pow(max(dot(halfdir, bumps), 0.0), 32.0 * specs.a);
	col.rgb += cvar_gl_specular * spec * specs.rgb;
#endif

#ifdef FULLBRIGHT
	vec4 fb = texture2D(s_t3, tc);
	col.rgb = mix(col.rgb, fb.rgb, fb.a);
#endif

	gl_FragColor = fog4(col * e_colourident);
}
#endif
