//the bloom filter
//filter out any texels which are not to bloom

varying vec2 tc;

#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
void main ()
{
	tc = v_texcoord;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;
void main ()
{
	gl_FragColor = (texture2D(s_t0, tc) - vec4(0.5, 0.5, 0.5, 0.0)) * vec4(2.0,2.0,2.0,1.0);
}
#endif