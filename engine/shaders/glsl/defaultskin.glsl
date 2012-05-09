!!permu FULLBRIGHT
!!permu UPPERLOWER
!!permu FRAMEBLEND
!!permu SKELETAL
!!permu FOG

//standard shader used for models.
//must support skeletal and 2-way vertex blending or Bad Things Will Happen.
//the vertex shader is responsible for calculating lighting values.

varying vec2 tc;
varying vec3 light;
#ifdef VERTEX_SHADER
#include "sys/skeletal.h"
attribute vec2 v_texcoord;
uniform vec3 e_light_dir;
uniform vec3 e_light_mul;
uniform vec3 e_light_ambient;
void main ()
{
	vec3 n;
	gl_Position = skeletaltransform_n(n);
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
uniform vec4 e_colourident;
void main ()
{
	vec4 col, sp;
	col = texture2D(s_t0, tc);
#ifdef UPPER
	vec4 uc = texture2D(s_t2, tc);
	col.rgb = mix(col.rgb, uc.rgb*e_uppercolour, uc.a);
#endif
#ifdef LOWER
	vec4 lc = texture2D(s_t1, tc);
	col.rgb = mix(col.rgb, lc.rgb*e_lowercolour, lc.a);
#endif
	col.rgb *= light;
#ifdef FULLBRIGHT
	vec4 fb = texture2D(s_t3, tc);
	col.rgb = mix(col.rgb, fb.rgb, fb.a);
#endif
	gl_FragColor = fog4(col * e_colourident);
}
#endif
