//add them together
//optionally apply tonemapping

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
uniform sampler2D s_t1;
uniform sampler2D s_t2;
uniform sampler2D s_t3;
void main ()
{
	gl_FragColor = 
		texture2D(s_t0, tc) +
		texture2D(s_t1, tc) +
		texture2D(s_t2, tc) +
		texture2D(s_t3, tc) ;
}
#endif
