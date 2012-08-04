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
//CUBEPROJ(projected cubemap)
//SPOT(projected circle
//CUBESHADOW

#if 0 && defined(GL_ARB_texture_gather) && defined(PCF) 
#extension GL_ARB_texture_gather : enable
#endif


varying vec2 tcbase;
varying vec3 lightvector;
#if defined(SPECULAR) || defined(OFFSETMAPPING)
varying vec3 eyevector;
#endif
#if defined(PCF) || defined(CUBEPROJ)
varying vec4 vtexprojcoord;
uniform mat4 l_cubematrix;
#ifndef SPOT
uniform mat4 l_projmatrix;
#endif
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
#if defined(PCF) || defined(SPOT) || defined(PROJECTION)
	//for texture projections/shadowmapping on dlights
	vtexprojcoord = (l_cubematrix*vec4(w.xyz, 1.0));
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
#ifdef CUBEPROJ
uniform samplerCube s_t3;
#endif
#ifdef PCF
#ifdef CUBESHADOW
uniform samplerCubeShadow s_t4;
#else
#if 0//def GL_ARB_texture_gather
uniform sampler2D s_t4;
#else
uniform sampler2DShadow s_t4;
#endif
#endif
#endif


uniform float l_lightradius;
uniform vec3 l_lightcolour;
uniform vec3 l_lightcolourscale;



#ifdef PCF
//#define shadow2DProj(t,c) (vec2(1.0,1.0))
//#define shadow2DProj(t,c) texture2DProj(t,c).rg

float ShadowmapFilter(void)
{
#ifdef SPOT
	const vec3 texscale = vec3(1.0/512.0, 1.0/512.0, 1.0);
#else
	const vec3 texscale = vec3(1.0/(512.0*3.0), 1.0/(512.0*2.0), 1.0);
#endif

	//dehomogonize input
	vec3 shadowcoord = (vtexprojcoord.xyz / vtexprojcoord.w);

#ifdef CUBESHADOW
//	vec3 shadowcoord = vshadowcoord.xyz / vshadowcoord.w;
//	#define dosamp(x,y) shadowCube(s_t4, shadowcoord + vec2(x,y)*texscale.xy).r
#else

#ifdef SPOT
	//bias it. don't bother figuring out which side or anything, its not needed
	//l_projmatrix contains the light's projection matrix so no other magic needed
	shadowcoord.xyz = (shadowcoord.xyz + vec3(1.0, 1.0, 1.0)) * vec3(0.5, 0.5, 0.5);
#else
	//figure out which axis to use
	//texture is arranged thusly:
	//forward left  up
	//back    right down
	vec3 dir = abs(shadowcoord);
	//assume z is the major axis (ie: forward from the light)
	vec3 t = shadowcoord;
	float ma = dir.z;
	vec4 axis = vec4(1.0, 1.0, 1.0, 0.0);
	if (dir.x > ma)
	{
		ma = dir.x;
		t = shadowcoord.zyx;
		axis.x = 3.0;
	}
	if (dir.y > ma)
	{
		ma = dir.y;
		t = shadowcoord.xzy;
		axis.x = 5.0;
	}
	if (t.z > 0.0)
	{
		axis.y = 3.0;
		t.z = -t.z;
	}


	//we also need to pass the result through the light's projection matrix too
	vec4 nsc =l_projmatrix*vec4(t, 1.0);
	shadowcoord = (nsc.xyz / nsc.w);

	//now bias and relocate it
	shadowcoord = (shadowcoord + axis.xyz) * vec3(0.5/3.0, 0.5/2.0, 0.5);
#endif

	#if 0//def GL_ARB_texture_gather
		vec2 ipart, fpart;
		#define dosamp(x,y) textureGatherOffset(s_t4, ipart.xy, vec2(x,y)))
		vec4 tl = step(shadowcoord.z, dosamp(-1.0, -1.0));
		vec4 bl = step(shadowcoord.z, dosamp(-1.0, 1.0));
		vec4 tr = step(shadowcoord.z, dosamp(1.0, -1.0));
		vec4 br = step(shadowcoord.z, dosamp(1.0, 1.0));
		//we now have 4*4 results, woo
		//we can just average them for 1/16th precision, but that's still limited graduations
		//the middle four pixels are 'full strength', but we interpolate the sides to effectively give 3*3
		vec4 col =     vec4(tl.ba, tr.ba) + vec4(bl.rg, br.rg) + //middle two rows are full strength
				mix(vec4(tl.rg, tr.rg), vec4(bl.ba, br.ba), fpart.y); //top+bottom rows
		return dot(mix(col.rgb, col.agb, fpart.x), vec3(1.0/9.0));	//blend r+a, gb are mixed because its pretty much free and gives a nicer dot instruction instead of lots of adds.

	#else
		#define dosamp(x,y) shadow2D(s_t4, shadowcoord.xyz + (vec3(x,y,0.0)*texscale.xyz)).r
		float s = 0.0;
		s += dosamp(-1.0, -1.0);
		s += dosamp(-1.0, 0.0);
		s += dosamp(-1.0, 1.0);
		s += dosamp(0.0, -1.0);
		s += dosamp(0.0, 0.0);
		s += dosamp(0.0, 1.0);
		s += dosamp(1.0, -1.0);
		s += dosamp(1.0, 0.0);
		s += dosamp(1.0, 1.0);
		return s/9.0;
	#endif
#endif
}
#endif


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
	//we still do bumpmapping even without bumps to ensure colours are always sane. light.exe does it too.
	diff = bases * (l_lightcolourscale.x + l_lightcolourscale.y * max(dot(vec3(0.0, 0.0, 1.0), nl), 0.0));
#endif


#ifdef SPECULAR
	vec3 halfdir = normalize(normalize(eyevector) + nl);
	float spec = pow(max(dot(halfdir, bumps), 0.0), 32.0 * specs.a);
	diff += l_lightcolourscale.z * spec * specs.rgb;
#endif



#ifdef CUBEPROJ
	/*filter the colour by the cubemap projection*/
	diff *= textureCube(s_t3, vtexprojcoord.xyz).rgb;
#endif

#if defined(SPOT)
	/*filter the colour by the spotlight. discard anything behind the light so we don't get a mirror image*/
	if (vtexprojcoord.w < 0.0) discard;
	vec2 spot = ((vtexprojcoord.st)/vtexprojcoord.w);colorscale*=1.0-(dot(spot,spot));
#endif

#ifdef PCF
	/*filter the light by the shadowmap. logically a boolean, but we allow fractions for softer shadows*/
//diff.rgb = (vtexprojcoord.xyz/vtexprojcoord.w) * 0.5 + 0.5;
	colorscale *= ShadowmapFilter();
//	gl_FragColor.rgb = vec3(ShadowmapFilter());

#endif

#if defined(PROJECTION)
	/*2d projection, not used*/
//	diff *= texture2d(s_t3, shadowcoord);
#endif

	gl_FragColor.rgb = fog3additive(diff*colorscale*l_lightcolour);
}
#endif
