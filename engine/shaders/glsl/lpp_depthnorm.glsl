!!permu BUMP
!!permu SKELETAL
!!cvarf r_glsl_offsetmapping_scale

//light pre-pass rendering (defered lighting)
//this is the initial pass, that draws the surface normals and depth to the initial colour buffer

#include "sys/defs.h"

#if defined(OFFSETMAPPING)
varying vec3 eyevector;
#endif

varying vec3 norm, tang, bitang;
#if defined(BUMP)
varying vec2 tc;
#endif
#ifdef VERTEX_SHADER
#include "sys/skeletal.h"

void main()
{
#if defined(BUMP)
	gl_Position = skeletaltransform_nst(norm, tang, bitang);
	tc = v_texcoord;
#else
	gl_Position = skeletaltransform_n(norm);
#endif

#if defined(OFFSETMAPPING)
	vec3 eyeminusvertex = e_eyepos - v_position.xyz;
	eyevector.x = dot(eyeminusvertex, v_svector.xyz);
	eyevector.y = dot(eyeminusvertex, v_tvector.xyz);
	eyevector.z = dot(eyeminusvertex, v_normal.xyz);
#endif
}
#endif
#ifdef FRAGMENT_SHADER
#ifdef OFFSETMAPPING
#include "sys/offsetmapping.h"
#endif
void main()
{
//adjust texture coords for offsetmapping
#ifdef OFFSETMAPPING
	vec2 tcoffsetmap = offsetmap(s_normalmap, tc, eyevector);
#define tc tcoffsetmap
#endif

	vec3 onorm;
#if defined(BUMP)
	vec3 bm = 2.0*texture2D(s_normalmap, tc).xyz - 1.0;
	onorm = normalize(bm.x * tang + bm.y * bitang + bm.z * norm);
#else
	onorm = norm;
#endif
	gl_FragColor = vec4(onorm.xyz, gl_FragCoord.z);
}
#endif
