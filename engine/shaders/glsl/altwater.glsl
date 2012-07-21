//modifier: REFLECT (s_t2 is a reflection instead of diffusemap)
//modifier: STRENGTH (0.1 = fairly gentle, 0.2 = big waves)
//modifier: FRESNEL (5=water)
//modifier: TXSCALE (0.2 - wave strength)
//modifier: RIPPLEMAP (s_t3 contains a ripplemap
//modifier: TINT    (some colour value)

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
#ifdef RIPPLEMAP
uniform sampler2D s_t3; 	//ripplemap
#endif

uniform float e_time;
void main (void)
{
	vec2 stc, ntc;
	vec3 n, refr, refl, fres;
	float f;
	stc = (1.0 + (tf.xy / tf.w)) * 0.5;

	//apply q1-style warp, just for kicks
	ntc.s = tc.s + sin(tc.t+e_time)*0.125;
	ntc.t = tc.t + sin(tc.s+e_time)*0.125;

	//generate the two wave patterns from the normalmap
	n = (texture2D(s_t1, TXSCALE*tc + vec2(e_time*0.1, 0.0)).xyz);
	n += (texture2D(s_t1, TXSCALE*tc - vec2(0, e_time*0.097)).xyz);
	n -= 1.0 - 4.0/256.0;

#ifdef RIPPLEMAP
	n += texture2D(s_t3, stc)*3;
#endif

	//the fresnel term decides how transparent the water should be
	f = pow(1.0-abs(dot(normalize(n), normalize(eye))), float(FRESNEL));

	refr = texture2D(s_t0, stc + n.st*STRENGTH).rgb * TINT;
#ifdef REFLECT
	refl = texture2D(s_t2, stc - n.st*STRENGTH).rgb;
#else
	refl = texture2D(s_t2, ntc).xyz;
#endif
//	refl += 0.1*pow(dot(n, vec3(0.0,0.0,1.0)), 64.0);

	fres = refr * (1.0-f) + refl*f;

//	fres = texture2D(s_t2, stc).xyz;

	gl_FragColor = vec4(fres, 1.0);
}
#endif
