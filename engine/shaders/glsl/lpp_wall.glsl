//the final defered lighting pass.
//the lighting values were written to some render target, which is fed into this shader, and now we draw all the wall textures with it.

varying vec2 tc, lm;
varying vec4 tf;
#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
attribute vec2 v_lmcoord;
void main ()
{
	tc = v_texcoord;
	lm = v_lmcoord;
	gl_Position = tf = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;
uniform sampler2D s_t1;
uniform sampler2D s_t2;
uniform vec4 e_lmscale;
void main ()
{
	vec2 nst;
	nst = tf.xy / tf.w;
	nst = (1.0 + nst) / 2.0;
	vec4 l = texture2D(s_t0, nst)*5.0;
	vec4 c = texture2D(s_t1, tc);
	vec3 lmsamp = texture2D(s_t2, lm).rgb*e_lmscale.rgb;
	vec3 diff = l.rgb;
	vec3 chrom = diff / (0.001 + dot(diff, vec3(0.3, 0.59, 0.11)));
	vec3 spec = chrom * l.a;
	gl_FragColor = vec4((diff + lmsamp) * c.xyz, 1.0);
}
#endif
