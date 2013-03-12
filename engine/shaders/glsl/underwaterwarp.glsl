!!cvarf r_waterwarp

//this is a post processing shader that is drawn fullscreen whenever the view is underwater.
//its generally expected to warp the view a little.

#ifdef VERTEX_SHADER
attribute vec2 v_texcoord;
varying vec2 v_stc;
varying vec2 v_warp;
varying vec2 v_edge;
uniform float e_time;
void main ()
{
	gl_Position = ftetransform();
	v_stc = (1.0+(gl_Position.xy / gl_Position.w))/2.0;
	v_warp.s = e_time * 0.25 + v_texcoord.s;
	v_warp.t = e_time * 0.25 + v_texcoord.t;
	v_edge = v_texcoord.xy;
}
#endif
#ifdef FRAGMENT_SHADER
varying vec2 v_stc;
varying vec2 v_warp;
varying vec2 v_edge;
uniform sampler2D s_t0;/*$currentrender*/
uniform sampler2D s_t1;/*warp image*/
uniform sampler2D s_t2;/*edge image*/
uniform vec4 e_rendertexturescale;
uniform float cvar_r_waterwarp;
void main ()
{
	float amptemp;
	vec3 edge;
	edge = texture2D( s_t2, v_edge ).rgb;
	amptemp = (0.010 / 0.625) * cvar_r_waterwarp * edge.x;
	vec3 offset;
	offset = texture2D( s_t1, v_warp ).rgb;
	offset.x = (offset.x - 0.5) * 2.0;
	offset.y = (offset.y - 0.5) * 2.0;
	vec2 temp;
	temp.x = v_stc.x + offset.x * amptemp;
	temp.y = v_stc.y + offset.y * amptemp;
	gl_FragColor = texture2D( s_t0, temp*e_rendertexturescale.st );
}
#endif
