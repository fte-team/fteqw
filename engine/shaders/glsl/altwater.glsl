!!cvarf r_glsl_turbscale
//modifier: REFLECT (s_t2 is a reflection instead of diffusemap)
//modifier: STRENGTH (0.1 = fairly gentle, 0.2 = big waves)
//modifier: FRESNEL (5=water)
//modifier: TXSCALE (0.2 - wave strength)
//modifier: RIPPLEMAP (s_t3 contains a ripplemap
//modifier: TINT    (some colour value)

uniform float cvar_r_glsl_turbscale;

#ifndef FRESNEL
#define FRESNEL 5.0
#endif
#ifndef STRENGTH
#define STRENGTH 0.1
#endif
#ifndef TXSCALE
#define TXSCALE 0.2
#endif
#ifndef TINT
#define TINT vec3(0.7, 0.8, 0.7)
#endif
#ifndef FOGTINT
#define FOGTINT vec3(0.2, 0.3, 0.2)
#endif

varying vec2 tc;
varying vec4 tf;
varying vec3 norm;
varying vec3 eye;
#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
attribute vec3 v_normal;
uniform vec3 e_eyepos;
void main (void)
{
	tc = v_texcoord.st;
	tf = ftetransform();
	norm = v_normal;
	eye = e_eyepos - v_position.xyz;
	gl_Position = tf;
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;	//refract
uniform sampler2D s_t1;	//normalmap
uniform sampler2D s_t2;	//diffuse/reflection
#ifdef DEPTH
uniform sampler2D s_t3; 	//refraction depth
#ifdef RIPPLEMAP
uniform sampler2D s_t4; 	//ripplemap
#endif
#else
#ifdef RIPPLEMAP
uniform sampler2D s_t3; 	//ripplemap
#endif
#endif

uniform float e_time;
void main (void)
{
	vec2 stc, ntc;
	vec3 n, refr, refl;
	float fres;
	float depth;
	stc = (1.0 + (tf.xy / tf.w)) * 0.5;
	//hack the texture coords slightly so that there are no obvious gaps
	stc.t -= 1.5*norm.z/1080.0;

	//apply q1-style warp, just for kicks
	ntc.s = tc.s + sin(tc.t+e_time)*0.125;
	ntc.t = tc.t + sin(tc.s+e_time)*0.125;

	//generate the two wave patterns from the normalmap
	n = (texture2D(s_t1, TXSCALE*tc + vec2(e_time*0.1, 0.0)).xyz);
	n += (texture2D(s_t1, TXSCALE*tc - vec2(0, e_time*0.097)).xyz);
	n -= 1.0 - 4.0/256.0;

#ifdef RIPPLEMAP
	n += texture2D(s_t4, stc).rgb*3.0;
#endif

	//the fresnel term decides how transparent the water should be
	fres = pow(1.0-abs(dot(normalize(n), normalize(eye))), float(FRESNEL));

#ifdef DEPTH
	float far = #include "cvar/gl_maxdist";
	float near = #include "cvar/gl_mindist";
	//get depth value at the surface
	float sdepth = gl_FragCoord.z;
	sdepth = (2.0*near) / (far + near - sdepth * (far - near));
	sdepth = mix(near, far, sdepth);

	//get depth value at the ground beyond the surface.
	float gdepth = texture2D(s_t3, stc).x;
	gdepth = (2.0*near) / (far + near - gdepth * (far - near));
	if (gdepth >= 0.5)
	{
		gdepth = sdepth;
		depth = 0.0;
	}
	else
	{
		gdepth = mix(near, far, gdepth);
		depth = gdepth - sdepth;
	}

	//reduce the normals in shallow water (near walls, reduces the pain of linear sampling)
	if (depth < 100)
		n *= depth/100.0;
#else
	depth = 1;
#endif 


	//refraction image (and water fog, if possible)
	refr = texture2D(s_t0, stc + n.st*STRENGTH*cvar_r_glsl_turbscale).rgb * TINT;
#ifdef DEPTH
	refr = mix(refr, FOGTINT, min(depth/4096, 1));
#endif

	//reflection/diffuse
#ifdef REFLECT
	refl = texture2D(s_t2, stc - n.st*STRENGTH*cvar_r_glsl_turbscale).rgb;
#else
	refl = texture2D(s_t2, ntc).xyz;
#endif
	//FIXME: add specular

	//interplate by fresnel
	refr = mix(refr, refl, fres);

	//done
	gl_FragColor = vec4(refr, 1.0);
}
#endif
