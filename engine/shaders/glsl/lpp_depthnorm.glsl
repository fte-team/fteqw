!!permu BUMP
!!permu SKELETAL

//light pre-pass rendering (defered lighting)
//this is the initial pass, that draws the surface normals and depth to the initial colour buffer

varying vec3 norm, tang, bitang;
#if defined(BUMP)
varying vec2 tc;
#endif
#ifdef VERTEX_SHADER
#include "sys/skeletal.h"
attribute vec2 v_texcoord;
void main()
{
#if defined(BUMP)
	gl_Position = skeletaltransform_nst(norm, tang, bitang);
	tc = v_texcoord;
#else
	gl_Position = skeletaltransform_n(norm);
#endif
}
#endif
#ifdef FRAGMENT_SHADER
#if defined(BUMP)
uniform sampler2D s_t0;
#endif
void main()
{
	vec3 onorm;
#if defined(BUMP)
	vec3 bm = 2.0*texture2D(s_t0, tc).xyz - 1.0;
	onorm = normalize(bm.x * tang + bm.y * bitang + bm.z * norm);
#else
	onorm = norm;
#endif
	gl_FragColor = vec4(onorm.xyz, gl_FragCoord.z / gl_FragCoord.w);
}
#endif
