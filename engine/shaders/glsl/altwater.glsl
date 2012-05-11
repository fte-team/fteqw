//modifier: REFLECT
//modifier: FRESNEL (5=water)

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
uniform sampler2D s_t2;	//diffuse
#ifdef REFLECT
uniform sampler2D s_t3; 	//reflect
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
	n = (texture2D(s_t1, 0.2*tc + vec2(e_time*0.1, 0)).xyz);
	n += (texture2D(s_t1, 0.2*tc - vec2(0, e_time*0.097)).xyz);
	n -= 1.0 - 4.0/256.0;

	n = normalize(n);

#if 1//def REFRACT
	refr = texture2D(s_t0, stc + n.st*0.2).rgb;
#else
	refr = texture2D(s_t2, ntc).xyz;
#endif
#ifdef REFLECT
	refl = texture2D(s_t3, stc - n.st*0.2).rgb;
#else
	refl = texture2D(s_t2, ntc).xyz;
#endif

	//the fresnel term decides how transparent the water should be
	f = pow(1.0-abs(dot(n, normalize(eye))), float(FRESNEL));
	fres = refr * (1.0-f) + refl*f;

	gl_FragColor = vec4(fres, 1.0);
}
#endif
