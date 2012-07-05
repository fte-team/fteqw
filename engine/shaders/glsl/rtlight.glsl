!!permu BUMP
!!permu SPECULAR
!!permu OFFSETMAPPING
!!permu SKELETAL
!!permu FOG
!!cvarf r_glsl_offsetmapping_scale

//this is the main shader responsible for realtime dlights.

//texture units:
//s0=diffuse, s1=normal, s2=specular, s3=shadowmap
//custom modifiers:
//PCF(shadowmap)
//CUBE(projected cubemap)


varying vec2 tcbase;
varying vec3 lightvector;
#if defined(SPECULAR) || defined(OFFSETMAPPING)
varying vec3 eyevector;
#endif
#if defined(PCF) || defined(CUBE)
varying vec4 vshadowcoord;
uniform mat4 l_projmatrix;
#endif
#ifdef VERTEX_SHADER
#include "sys/skeletal.h"
uniform vec3 l_lightposition;
attribute vec2 v_texcoord;
#if defined(SPECULAR) || defined(OFFSETMAPPING)
uniform vec3 e_eyepos;
#endif
void main ()
{
	vec3 n, s, t, w;
	gl_Position = skeletaltransform_wnst(w,n,s,t);
	tcbase = v_texcoord;	//pass the texture coords straight through
	vec3 lightminusvertex = l_lightposition - w.xyz;
	lightvector.x = -dot(lightminusvertex, s.xyz);
	lightvector.y = dot(lightminusvertex, t.xyz);
	lightvector.z = dot(lightminusvertex, n.xyz);
#if defined(SPECULAR)||defined(OFFSETMAPPING)
	vec3 eyeminusvertex = e_eyepos - w.xyz;
	eyevector.x = -dot(eyeminusvertex, s.xyz);
	eyevector.y = dot(eyeminusvertex, t.xyz);
	eyevector.z = dot(eyeminusvertex, n.xyz);
#endif
#if defined(PCF) || defined(SPOT) || defined(PROJECTION) || defined(CUBE)
	vshadowcoord = l_projmatrix*vec4(w.xyz, 1.0);
#endif
}
#endif
#ifdef FRAGMENT_SHADER
#include "sys/fog.h"
uniform sampler2D s_t0;
#if defined(BUMP) || defined(SPECULAR) || defined(OFFSETMAPPING)
uniform sampler2D s_t1;
#endif
#ifdef SPECULAR
uniform sampler2D s_t2;
#endif
#ifdef CUBE
uniform samplerCube s_t3;
#endif
#ifdef PCF
#ifdef CUBE
uniform samplerCubeShadow s_t7;
#else
uniform sampler2DShadow s_t7;
#endif
#endif
uniform float l_lightradius;
uniform vec3 l_lightcolour;
uniform vec3 l_lightcolourscale;
#ifdef OFFSETMAPPING
#include "sys/offsetmapping.h"
#endif
void main ()
{
//read raw texture samples (offsetmapping munges the tex coords first)
#ifdef OFFSETMAPPING
	vec2 tcoffsetmap = offsetmap(s_t1, tcbase, eyevector);
#define tcbase tcoffsetmap
#endif
	vec3 bases = vec3(texture2D(s_t0, tcbase));
#if defined(BUMP) || defined(SPECULAR)
	vec3 bumps = normalize(vec3(texture2D(s_t1, tcbase)) - 0.5);
#endif
#ifdef SPECULAR
	vec4 specs = texture2D(s_t2, tcbase);
#endif

	vec3 nl = normalize(lightvector);
	float colorscale = max(1.0 - (dot(lightvector, lightvector)/(l_lightradius*l_lightradius)), 0.0);
	vec3 diff;
#ifdef BUMP
	diff = bases * (l_lightcolourscale.x + l_lightcolourscale.y * max(dot(bumps, nl), 0.0));
#else
	diff = bases * (l_lightcolourscale.x + l_lightcolourscale.y * max(dot(vec3(0.0, 0.0, 1.0), nl), 0.0));
#endif


#ifdef SPECULAR
	vec3 halfdir = normalize(normalize(eyevector) + nl);
	float spec = pow(max(dot(halfdir, bumps), 0.0), 32.0 * specs.a);
	diff += l_lightcolourscale.z * spec * specs.rgb;
#endif



#ifdef CUBE
	diff *= textureCube(s_t3, vshadowcoord.xyz).rgb;
#endif
#ifdef PCF
	#if defined(SPOT)
		const float texx = 512.0;
		const float texy = 512.0;
		vec4 shadowcoord = vshadowcoord;
	#else
		const float texx = 512.0;
		const float texy = 512.0;
		vec4 shadowcoord;
		shadowcoord.zw = vshadowcoord.zw;
		shadowcoord.xy = vshadowcoord.xy;
	#endif
	#ifdef CUBE
		const float xPixelOffset = 1.0/texx;	const float yPixelOffset = 1.0/texy;	float s = 0.0;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadowCubeProj(s_t7, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		colorscale *= s/9.0;
	#else
		const float xPixelOffset = 1.0/texx;	const float yPixelOffset = 1.0/texy;	float s = 0.0;
		s += shadow2DProj(s_t7, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadow2DProj(s_t7, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadow2DProj(s_t7, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadow2DProj(s_t7, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadow2DProj(s_t7, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadow2DProj(s_t7, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadow2DProj(s_t7, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadow2DProj(s_t7, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		s += shadow2DProj(s_t7, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;
		colorscale *= s/9.0;
	#endif
#endif
#if defined(SPOT)
	if (shadowcoord.w < 0.0) discard;
	vec2 spot = ((shadowcoord.st)/shadowcoord.w - 0.5)*2.0;colorscale*=1.0-(dot(spot,spot));
#endif
#if defined(PROJECTION)
	l_lightcolour *= texture2d(s_t3, shadowcoord);
#endif
	gl_FragColor.rgb = fog3additive(diff*colorscale*l_lightcolour);
}
#endif
