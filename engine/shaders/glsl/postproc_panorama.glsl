!!cvarf ffov

//panoramic view rendering, for promo map shots or whatever.

#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
varying vec2 texcoord;
void main()
{
	texcoord = v_texcoord.xy;
	gl_Position = ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform samplerCube s_t0;
varying vec2 texcoord;
uniform float cvar_ffov;
void main()
{
	vec3 tc;	
	float ang;	
	ang = texcoord.x*radians(cvar_ffov);	
	tc.x = sin(ang);	
	tc.y = -texcoord.y;	
	tc.z = cos(ang);	
	gl_FragColor = textureCube(s_t0, tc);
}
#endif
