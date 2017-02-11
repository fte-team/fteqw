!!ver 100 150
!!permu TESS
!!permu BUMP
!!permu FRAMEBLEND
!!permu SKELETAL
!!permu UPPERLOWER
!!permu FOG
!!permu REFLECTCUBEMASK
!!cvarf r_glsl_offsetmapping_scale
!!cvardf r_glsl_pcf
!!cvardf r_tessellation=0

#include "sys/defs.h"

#ifndef USE_ARB_SHADOW
//fall back on regular samplers if we must
#define sampler2DShadow sampler2D
#endif

//this is the main shader responsible for realtime dlights.

//texture units:
//s0=diffuse, s1=normal, s2=specular, s3=shadowmap
//custom modifiers:
//PCF(shadowmap)
//CUBEPROJ(projected cubemap)
//SPOT(projected circle
//CUBESHADOW

#ifndef r_glsl_pcf
#error r_glsl_pcf wasnt defined
#endif
#if r_glsl_pcf < 1
	#undef r_glsl_pcf
	#define r_glsl_pcf 9
#endif

#if 0 && defined(GL_ARB_texture_gather) && defined(PCF) 
#extension GL_ARB_texture_gather : enable
#endif

#ifdef UPPERLOWER
#define UPPER
#define LOWER
#endif

//if there's no vertex normals known, disable some stuff.
//FIXME: this results in dupe permutations.
#ifdef NOBUMP
#undef SPECULAR
#undef BUMP
#undef OFFSETMAPPING
#endif

#if !defined(TESS_CONTROL_SHADER)
	varying vec2 tcbase;
	varying vec3 lightvector;
	#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
		varying vec3 eyevector;
	#endif
	#ifdef REFLECTCUBEMASK
		varying mat3 invsurface;
	#endif
	#if defined(PCF) || defined(CUBE) || defined(SPOT)
		varying vec4 vtexprojcoord;
	#endif
#endif


#ifdef VERTEX_SHADER
#ifdef TESS
varying vec3 vertex, normal;
#endif
#include "sys/skeletal.h"
void main ()
{
	vec3 n, s, t, w;
	gl_Position = skeletaltransform_wnst(w,n,s,t);
	tcbase = v_texcoord;	//pass the texture coords straight through
	vec3 lightminusvertex = l_lightposition - w.xyz;
#ifdef NOBUMP
	//the only important thing is distance
	lightvector = lightminusvertex;
#else
	//the light direction relative to the surface normal, for bumpmapping.
	lightvector.x = dot(lightminusvertex, s.xyz);
	lightvector.y = dot(lightminusvertex, t.xyz);
	lightvector.z = dot(lightminusvertex, n.xyz);
#endif
#if defined(SPECULAR)||defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
	vec3 eyeminusvertex = e_eyepos - w.xyz;
	eyevector.x = dot(eyeminusvertex, s.xyz);
	eyevector.y = dot(eyeminusvertex, t.xyz);
	eyevector.z = dot(eyeminusvertex, n.xyz);
#endif
#ifdef REFLECTCUBEMASK
	invsurface[0] = v_svector;
	invsurface[1] = v_tvector;
	invsurface[2] = v_normal;
#endif
#if defined(PCF) || defined(SPOT) || defined(CUBE)
	//for texture projections/shadowmapping on dlights
	vtexprojcoord = (l_cubematrix*vec4(w.xyz, 1.0));
#endif

#ifdef TESS
	vertex = w;
	normal = n;
#endif
}
#endif






#if defined(TESS_CONTROL_SHADER)
layout(vertices = 3) out;

in vec3 vertex[];
out vec3 t_vertex[];
in vec3 normal[];
out vec3 t_normal[];
in vec2 tcbase[];
out vec2 t_tcbase[];
in vec3 lightvector[];
out vec3 t_lightvector[];
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
in vec3 eyevector[];
out vec3 t_eyevector[];
#endif
void main()
{
	//the control shader needs to pass stuff through
#define id gl_InvocationID
	t_vertex[id] = vertex[id];
	t_normal[id] = normal[id];
	t_tcbase[id] = tcbase[id];
	t_lightvector[id] = lightvector[id];
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
	t_eyevector[id] = eyevector[id];
#endif

	gl_TessLevelOuter[0] = float(r_tessellation)+1.0;
	gl_TessLevelOuter[1] = float(r_tessellation)+1.0;
	gl_TessLevelOuter[2] = float(r_tessellation)+1.0;
	gl_TessLevelInner[0] = float(r_tessellation)+1.0;
}
#endif









#if defined(TESS_EVALUATION_SHADER)
layout(triangles) in;

in vec3 t_vertex[];
in vec3 t_normal[];
in vec2 t_tcbase[];
in vec3 t_lightvector[];
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
in vec3 t_eyevector[];
#endif

#define LERP(a) (gl_TessCoord.x*a[0] + gl_TessCoord.y*a[1] + gl_TessCoord.z*a[2])
void main()
{
#define factor 1.0
	tcbase = LERP(t_tcbase);
	vec3 w = LERP(t_vertex);

	vec3 t0 = w - dot(w-t_vertex[0],t_normal[0])*t_normal[0];
	vec3 t1 = w - dot(w-t_vertex[1],t_normal[1])*t_normal[1];
	vec3 t2 = w - dot(w-t_vertex[2],t_normal[2])*t_normal[2];
	w = w*(1.0-factor) + factor*(gl_TessCoord.x*t0+gl_TessCoord.y*t1+gl_TessCoord.z*t2);

#if defined(PCF) || defined(SPOT) || defined(CUBE)
	//for texture projections/shadowmapping on dlights
	vtexprojcoord = (l_cubematrix*vec4(w.xyz, 1.0));
#endif

	//FIXME: we should be recalcing these here, instead of just lerping them
	lightvector = LERP(t_lightvector);
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
	eyevector = LERP(t_eyevector);
#endif

	gl_Position = m_modelviewprojection * vec4(w,1.0);
}
#endif











#ifdef FRAGMENT_SHADER

#include "sys/fog.h"

#ifdef PCF
vec3 ShadowmapCoord(void)
{
#ifdef SPOT
	//bias it. don't bother figuring out which side or anything, its not needed
	//l_projmatrix contains the light's projection matrix so no other magic needed
	return ((vtexprojcoord.xyz-vec3(0.0,0.0,0.015))/vtexprojcoord.w + vec3(1.0, 1.0, 1.0)) * vec3(0.5, 0.5, 0.5);
//#elif defined(CUBESHADOW)
//	vec3 shadowcoord = vshadowcoord.xyz / vshadowcoord.w;
//	#define dosamp(x,y) shadowCube(s_shadowmap, shadowcoord + vec2(x,y)*texscale.xy).r
#else
	//figure out which axis to use
	//texture is arranged thusly:
	//forward left  up
	//back    right down
	vec3 dir = abs(vtexprojcoord.xyz);
	//assume z is the major axis (ie: forward from the light)
	vec3 t = vtexprojcoord.xyz;
	float ma = dir.z;
	vec3 axis = vec3(0.5/3.0, 0.5/2.0, 0.5);
	if (dir.x > ma)
	{
		ma = dir.x;
		t = vtexprojcoord.zyx;
		axis.x = 0.5;
	}
	if (dir.y > ma)
	{
		ma = dir.y;
		t = vtexprojcoord.xzy;
		axis.x = 2.5/3.0;
	}
	//if the axis is negative, flip it.
	if (t.z > 0.0)
	{
		axis.y = 1.5/2.0;
		t.z = -t.z;
	}

	//we also need to pass the result through the light's projection matrix too
	//the 'matrix' we need only contains 5 actual values. and one of them is a -1. So we might as well just use a vec4.
	//note: the projection matrix also includes scalers to pinch the image inwards to avoid sampling over borders, as well as to cope with non-square source image
	//the resulting z is prescaled to result in a value between -0.5 and 0.5.
	//also make sure we're in the right quadrant type thing
	return axis + ((l_shadowmapproj.xyz*t.xyz + vec3(0.0, 0.0, l_shadowmapproj.w)) / -t.z);
#endif
}

float ShadowmapFilter(void)
{
	vec3 shadowcoord = ShadowmapCoord();

	#if 0//def GL_ARB_texture_gather
		vec2 ipart, fpart;
		#define dosamp(x,y) textureGatherOffset(s_shadowmap, ipart.xy, vec2(x,y)))
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
#ifdef USE_ARB_SHADOW
		//with arb_shadow, we can benefit from hardware acclerated pcf, for smoother shadows
		#define dosamp(x,y) shadow2D(s_shadowmap, shadowcoord.xyz + (vec3(x,y,0.0)*l_shadowmapscale.xyx))
#else
		//this will probably be a bit blocky.
		#define dosamp(x,y) float(texture2D(s_shadowmap, shadowcoord.xy + (vec2(x,y)*l_shadowmapscale.xy)).r >= shadowcoord.z)
#endif
		float s = 0.0;
		#if r_glsl_pcf >= 1 && r_glsl_pcf < 5
			s += dosamp(0.0, 0.0);
			return s;
		#elif r_glsl_pcf >= 5 && r_glsl_pcf < 9
			s += dosamp(-1.0, 0.0);
			s += dosamp(0.0, -1.0);
			s += dosamp(0.0, 0.0);
			s += dosamp(0.0, 1.0);
			s += dosamp(1.0, 0.0);
			return s/5.0;
		#else
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
	vec2 tcoffsetmap = offsetmap(s_normalmap, tcbase, eyevector);
#define tcbase tcoffsetmap
#endif
#if defined(FLAT)
	vec3 bases = vec3(1.0);
#else
	vec3 bases = vec3(texture2D(s_diffuse, tcbase));
#endif
#ifdef UPPER
	vec4 uc = texture2D(s_upper, tcbase);
	bases.rgb += uc.rgb*e_uppercolour*uc.a;
#endif
#ifdef LOWER
	vec4 lc = texture2D(s_lower, tcbase);
	bases.rgb += lc.rgb*e_lowercolour*lc.a;
#endif
#if defined(BUMP) || defined(SPECULAR) || defined(REFLECTCUBEMASK)
	vec3 bumps = normalize(vec3(texture2D(s_normalmap, tcbase)) - 0.5);
#elif defined(REFLECTCUBEMASK)
	vec3 bumps = vec3(0.0,0.0,1.0);
#endif
#ifdef SPECULAR
	vec4 specs = texture2D(s_specular, tcbase);
#endif

	float colorscale = max(1.0 - (dot(lightvector, lightvector)/(l_lightradius*l_lightradius)), 0.0);
	vec3 diff;
#ifdef NOBUMP
	//surface can only support ambient lighting, even for lights that try to avoid it.
	diff = bases * (l_lightcolourscale.x+l_lightcolourscale.y);
#else
	vec3 nl = normalize(lightvector);
	#ifdef BUMP
		diff = bases * (l_lightcolourscale.x + l_lightcolourscale.y * max(dot(bumps, nl), 0.0));
	#else
		//we still do bumpmapping even without bumps to ensure colours are always sane. light.exe does it too.
		diff = bases * (l_lightcolourscale.x + l_lightcolourscale.y * max(dot(vec3(0.0, 0.0, 1.0), nl), 0.0));
	#endif
#endif


#ifdef SPECULAR
	vec3 halfdir = normalize(normalize(eyevector) + nl);
	float spec = pow(max(dot(halfdir, bumps), 0.0), 32.0 * specs.a);
	diff += l_lightcolourscale.z * spec * specs.rgb;
#endif

#ifdef REFLECTCUBEMASK
	vec3 rtc = reflect(-eyevector, bumps);
	rtc = rtc.x*invsurface[0] + rtc.y*invsurface[1] + rtc.z*invsurface[2];
	rtc = (m_model * vec4(rtc.xyz,0.0)).xyz;
	diff += texture2D(s_reflectmask, tcbase).rgb * textureCube(s_reflectcube, rtc).rgb;
#endif

#ifdef CUBE
	/*filter the colour by the cubemap projection*/
	diff *= textureCube(s_projectionmap, vtexprojcoord.xyz).rgb;
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
//	diff = ShadowmapCoord();
#endif

#if defined(PROJECTION)
	/*2d projection, not used*/
//	diff *= texture2d(s_projectionmap, shadowcoord);
#endif

	gl_FragColor.rgb = fog3additive(diff*colorscale*l_lightcolour);

}
#endif

