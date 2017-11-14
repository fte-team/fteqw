!!ver 100-450
//this shader is applies gamma/contrast/brightness to the source image, and dumps it out.

varying vec2 tc;
varying vec4 vc;

#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
attribute vec4 v_colour;
void main ()
{
	tc = v_texcoord;
	vc = v_colour;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;
void main ()
{
	gl_FragColor = pow(texture2D(s_t0, tc) * vc.g, vec4(vc.r)) + vc.b;
}
#endif
