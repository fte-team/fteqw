//this shader is a light shader. ideally drawn with a quad covering the entire region
//the output is contribution from this light (which will be additively blended)
//you can blame Electro for much of the maths in here.
//fixme: no fog

varying vec4 tf;
#ifdef VERTEX_SHADER
void main()
{
	tf = ftetransform();
	gl_Position = tf;
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;
uniform vec3 l_lightposition;
uniform mat4 m_invviewprojection;
uniform vec3 l_lightcolour;
uniform float l_lightradius;
vec3 calcLightWorldPos(vec2 screenPos, float depth)
{
	vec4 pos;
	pos.x	= screenPos.x;
	pos.y	= screenPos.y;
	pos.z	= depth;
	pos.w	= 1.0;
	pos	= m_invviewprojection * pos;
	return pos.xyz / pos.w;
}
void main ()
{
	vec3 lightColour		= l_lightcolour.rgb;
	float lightIntensity	= 1.0;
	float lightAttenuation	= l_lightradius;	// fixme: just use the light radius for now, use better near/far att math separately once working
	float radiusFar		= l_lightradius;
	float radiusNear		= l_lightradius*0.5;

	vec2 fc;
	fc = tf.xy / tf.w;
	vec4 data = texture2D(s_t0, (1.0 + fc) / 2.0);
	float depth = data.a;
	vec3 norm = data.xyz;

	/* calc where the wall that generated this sample came from */
	vec3 worldPos	= calcLightWorldPos(fc, depth);

	/*calc diffuse lighting term*/
	vec3 lightDir = l_lightposition - worldPos;
	float zdiff = 1.0 - clamp(length(lightDir) / lightAttenuation, 0.0, 1.0);
	float atten = (radiusFar * zdiff) / (radiusFar - radiusNear);
	atten = pow(atten, 2.0);
	lightDir = normalize(lightDir);
	float nDotL = dot(norm, lightDir) * atten;
	float lightDiffuse = max(0.0, nDotL);

	gl_FragColor = vec4(lightDiffuse * (lightColour * lightIntensity), 1.0);
}
#endif
