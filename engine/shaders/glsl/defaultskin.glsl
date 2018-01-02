!!ver 100 150
!!permu TESS
!!permu FULLBRIGHT
!!permu UPPERLOWER
!!permu FRAMEBLEND
!!permu SKELETAL
!!permu FOG
!!permu BUMP
!!permu REFLECTCUBEMASK
!!cvarf r_glsl_offsetmapping_scale
!!cvarf gl_specular
!!cvardf gl_affinemodels=0
!!cvardf r_tessellation_level=5
!!samps diffuse normalmap specular fullbright upper lower paletted

#include "sys/defs.h"

//standard shader used for models.
//must support skeletal and 2-way vertex blending or Bad Things Will Happen.
//the vertex shader is responsible for calculating lighting values.

#if gl_affinemodels==1 && __VERSION__ >= 130
#define affine noperspective
#else
#define affine
#endif








#ifdef VERTEX_SHADER
#include "sys/skeletal.h"

affine varying vec2 tc;
varying vec3 light;
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
varying vec3 eyevector;
#endif
#ifdef REFLECTCUBEMASK
	varying mat3 invsurface;
#endif
#ifdef TESS
varying vec3 vertex;
varying vec3 normal;
#endif

void main ()
{
#if defined(SPECULAR)||defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
	vec3 n, s, t, w;
	gl_Position = skeletaltransform_wnst(w,n,s,t);
	vec3 eyeminusvertex = e_eyepos - w.xyz;
	eyevector.x = dot(eyeminusvertex, s.xyz);
	eyevector.y = dot(eyeminusvertex, t.xyz);
	eyevector.z = dot(eyeminusvertex, n.xyz);
#else
	vec3 n, s, t, w;
	gl_Position = skeletaltransform_wnst(w,n,s,t);
#endif
#ifdef REFLECTCUBEMASK
	invsurface[0] = s;
	invsurface[1] = t;
	invsurface[2] = n;
#endif

	tc = v_texcoord;

	float d = dot(n,e_light_dir);
	if (d < 0.0)		//vertex shader. this might get ugly, but I don't really want to make it per vertex.
		d = 0.0;	//this avoids the dark side going below the ambient level.
	light = e_light_ambient + (d*e_light_mul);

//FIXME: Software rendering imitation should possibly push out normals by half a pixel or something to approximate software's over-estimation of distant model sizes (small models are drawn using JUST their verticies using the nearest pixel, which results in larger meshes)

#ifdef TESS
	normal = n;
	vertex = w;
#endif
}
#endif










#if defined(TESS_CONTROL_SHADER)
layout(vertices = 3) out;

in vec3 vertex[];
out vec3 t_vertex[];
in vec3 normal[];
out vec3 t_normal[];
affine in vec2 tc[];
affine out vec2 t_tc[];
in vec3 light[];
out vec3 t_light[];
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
in vec3 eyevector[];
out vec3 t_eyevector[];
#endif
#ifdef REFLECTCUBEMASK
in mat3 invsurface[];
out mat3 t_invsurface[];
#endif
void main()
{
	//the control shader needs to pass stuff through
#define id gl_InvocationID
	t_vertex[id] = vertex[id];
	t_normal[id] = normal[id];
	t_tc[id] = tc[id];
	t_light[id] = light[id];
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
	t_eyevector[id] = eyevector[id];
#endif
#ifdef REFLECTCUBEMASK
	t_invsurface[id][0] = invsurface[id][0];
	t_invsurface[id][1] = invsurface[id][1];
	t_invsurface[id][2] = invsurface[id][2];
#endif

	gl_TessLevelOuter[0] = float(r_tessellation_level);
	gl_TessLevelOuter[1] = float(r_tessellation_level);
	gl_TessLevelOuter[2] = float(r_tessellation_level);
	gl_TessLevelInner[0] = float(r_tessellation_level);
}
#endif









#if defined(TESS_EVALUATION_SHADER)
layout(triangles) in;

in vec3 t_vertex[];
in vec3 t_normal[];
affine in vec2 t_tc[];
affine out vec2 tc;
in vec3 t_light[];
out vec3 light;
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
in vec3 t_eyevector[];
out vec3 eyevector;
#endif
#ifdef REFLECTCUBEMASK
in mat3 t_invsurface[];
out mat3 invsurface;
#endif

#define LERP(a) (gl_TessCoord.x*a[0] + gl_TessCoord.y*a[1] + gl_TessCoord.z*a[2])
void main()
{
#define factor 1.0
	tc = LERP(t_tc);
	vec3 w = LERP(t_vertex);

	vec3 t0 = w - dot(w-t_vertex[0],t_normal[0])*t_normal[0];
	vec3 t1 = w - dot(w-t_vertex[1],t_normal[1])*t_normal[1];
	vec3 t2 = w - dot(w-t_vertex[2],t_normal[2])*t_normal[2];
	w = w*(1.0-factor) + factor*(gl_TessCoord.x*t0+gl_TessCoord.y*t1+gl_TessCoord.z*t2);

	//FIXME: we should be recalcing these here, instead of just lerping them
	light = LERP(t_light);
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
	eyevector = LERP(t_eyevector);
#endif
#ifdef REFLECTCUBEMASK
	invsurface[0] = LERP(t_invsurface[0]);
	invsurface[1] = LERP(t_invsurface[1]);
	invsurface[2] = LERP(t_invsurface[2]);
#endif

	gl_Position = m_modelviewprojection * vec4(w,1.0);
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

affine varying vec2 tc;
varying vec3 light;
#if defined(SPECULAR) || defined(OFFSETMAPPING) || defined(REFLECTCUBEMASK)
varying vec3 eyevector;
#endif
#ifdef REFLECTCUBEMASK
	varying mat3 invsurface;
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
//	lightlev -= 1.0 / 128.0;	//software rendering appears to round down, so make sure we favour the lower values instead of rounding to the nearest
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
		float spec = pow(max(dot(halfdir, bumps), 0.0), FTE_SPECULAR_EXPONENT * specs.a);
		col.rgb += FTE_SPECULAR_MULTIPLIER * spec * specs.rgb;
	#elif defined(REFLECTCUBEMASK)
		vec3 bumps = vec3(0, 0, 1);
	#endif

	#ifdef REFLECTCUBEMASK
		vec3 rtc = reflect(-eyevector, bumps);
		rtc = rtc.x*invsurface[0] + rtc.y*invsurface[1] + rtc.z*invsurface[2];
		rtc = (m_model * vec4(rtc.xyz,0.0)).xyz;
		col.rgb += texture2D(s_reflectmask, tc).rgb * textureCube(s_reflectcube, rtc).rgb;
	#endif

	col.rgb *= light;

	#ifdef FULLBRIGHT
		vec4 fb = texture2D(s_fullbright, tc);
//		col.rgb = mix(col.rgb, fb.rgb, fb.a);
		col.rgb += fb.rgb * fb.a * e_glowmod.rgb;
	#endif
#endif

	gl_FragColor = fog4(col * e_colourident);
}
#endif

