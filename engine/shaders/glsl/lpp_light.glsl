//this shader is a light shader. ideally drawn with a quad covering the entire region
//the output is contribution from this light (which will be additively blended)
//you can blame Electro for much of the maths in here.
//fixme: no fog

//s_t0 is the normals and depth
//output should be amount of light hitting the surface.

varying vec4 tf;
#ifdef VERTEX_SHADER
void main()
{
	tf = ftetransform();
	gl_Position = tf;
}
#endif
#ifdef FRAGMENT_SHADER
uniform sampler2D s_t0;	//norm.xyz, depth
uniform vec3 l_lightposition;
uniform mat4 m_invviewprojection;
uniform vec3 l_lightcolour;
uniform float l_lightradius;
uniform mat4 l_cubematrix;





#ifdef PCF
#define USE_ARB_SHADOW
#ifndef USE_ARB_SHADOW
//fall back on regular samplers if we must
#define sampler2DShadow sampler2D
#endif
uniform sampler2DShadow s_shadowmap;

uniform vec4 l_shadowmapproj; //light projection matrix info
uniform vec2 l_shadowmapscale;	//xy are the texture scale, z is 1, w is the scale.
vec3 ShadowmapCoord(vec4 cubeproj)
{
#ifdef SPOT
	//bias it. don't bother figuring out which side or anything, its not needed
	//l_projmatrix contains the light's projection matrix so no other magic needed
	return ((cubeproj.xyz-vec3(0.0,0.0,0.015))/cubeproj.w + vec3(1.0, 1.0, 1.0)) * vec3(0.5, 0.5, 0.5);
//#elif defined(CUBESHADOW)
//	vec3 shadowcoord = vshadowcoord.xyz / vshadowcoord.w;
//	#define dosamp(x,y) shadowCube(s_shadowmap, shadowcoord + vec2(x,y)*texscale.xy).r
#else
	//figure out which axis to use
	//texture is arranged thusly:
	//forward left  up
	//back    right down
	vec3 dir = abs(cubeproj.xyz);
	//assume z is the major axis (ie: forward from the light)
	vec3 t = cubeproj.xyz;
	float ma = dir.z;
	vec3 axis = vec3(0.5/3.0, 0.5/2.0, 0.5);
	if (dir.x > ma)
	{
		ma = dir.x;
		t = cubeproj.zyx;
		axis.x = 0.5;
	}
	if (dir.y > ma)
	{
		ma = dir.y;
		t = cubeproj.xzy;
		axis.x = 2.5/3.0;
	}
	//if the axis is negative, flip it.
	if (t.z > 0.0)
	{
		axis.y = 1.5/2.0;
		t.z = -t.z;
	}

	//we also need to pass the result through the light's projection matrix too
	//the 'matrix' we need only contains 5 actual values. and one of them is a -1. So we might as well just use a vec4.
	//note: the projection matrix also includes scalers to pinch the image inwards to avoid sampling over borders, as well as to cope with non-square source image
	//the resulting z is prescaled to result in a value between -0.5 and 0.5.
	//also make sure we're in the right quadrant type thing
	return axis + ((l_shadowmapproj.xyz*t.xyz + vec3(0.0, 0.0, l_shadowmapproj.w)) / -t.z);
#endif
}

float ShadowmapFilter(vec4 vtexprojcoord)
{
	vec3 shadowcoord = ShadowmapCoord(vtexprojcoord);

	#if 0//def GL_ARB_texture_gather
		vec2 ipart, fpart;
		#define dosamp(x,y) textureGatherOffset(s_shadowmap, ipart.xy, vec2(x,y)))
		vec4 tl = step(shadowcoord.z, dosamp(-1.0, -1.0));
		vec4 bl = step(shadowcoord.z, dosamp(-1.0, 1.0));
		vec4 tr = step(shadowcoord.z, dosamp(1.0, -1.0));
		vec4 br = step(shadowcoord.z, dosamp(1.0, 1.0));
		//we now have 4*4 results, woo
		//we can just average them for 1/16th precision, but that's still limited graduations
		//the middle four pixels are 'full strength', but we interpolate the sides to effectively give 3*3
		vec4 col =     vec4(tl.ba, tr.ba) + vec4(bl.rg, br.rg) + //middle two rows are full strength
				mix(vec4(tl.rg, tr.rg), vec4(bl.ba, br.ba), fpart.y); //top+bottom rows
		return dot(mix(col.rgb, col.agb, fpart.x), vec3(1.0/9.0));	//blend r+a, gb are mixed because its pretty much free and gives a nicer dot instruction instead of lots of adds.

	#else
#ifdef USE_ARB_SHADOW
		//with arb_shadow, we can benefit from hardware acclerated pcf, for smoother shadows
		#define dosamp(x,y) shadow2D(s_shadowmap, shadowcoord.xyz + (vec3(x,y,0.0)*l_shadowmapscale.xyx)).r
#else
		//this will probably be a bit blocky.
		#define dosamp(x,y) float(texture2D(s_shadowmap, shadowcoord.xy + (vec2(x,y)*l_shadowmapscale.xy)).r >= shadowcoord.z)
#endif
		float s = 0.0;
		#if r_glsl_pcf >= 1 && r_glsl_pcf < 5
			s += dosamp(0.0, 0.0);
			return s;
		#elif r_glsl_pcf >= 5 && r_glsl_pcf < 9
			s += dosamp(-1.0, 0.0);
			s += dosamp(0.0, -1.0);
			s += dosamp(0.0, 0.0);
			s += dosamp(0.0, 1.0);
			s += dosamp(1.0, 0.0);
			return s/5.0;
		#else
			s += dosamp(-1.0, -1.0);
			s += dosamp(-1.0, 0.0);
			s += dosamp(-1.0, 1.0);
			s += dosamp(0.0, -1.0);
			s += dosamp(0.0, 0.0);
			s += dosamp(0.0, 1.0);
			s += dosamp(1.0, -1.0);
			s += dosamp(1.0, 0.0);
			s += dosamp(1.0, 1.0);
			return s/9.0;
		#endif
	#endif
}
#else
float ShadowmapFilter(vec4 vtexprojcoord)
{
	return 1.0;
}
#endif





vec3 calcLightWorldPos(vec2 screenPos, float depth)
{
	vec4 pos = m_invviewprojection * vec4(screenPos.xy, (depth*2.0)-1.0, 1.0);
	return pos.xyz / pos.w;
}
void main ()
{
	vec3 lightColour		= l_lightcolour.rgb;
	float lightIntensity		= 1.0;
	float lightAttenuation	= l_lightradius;	// fixme: just use the light radius for now, use better near/far att math separately once working
	float radiusFar			= l_lightradius;
	float radiusNear		= l_lightradius*0.5;

	vec2 fc;
	fc = tf.xy / tf.w;
	vec4 data = texture2D(s_t0, (1.0 + fc) / 2.0);
	float depth = data.a;
	vec3 norm = data.xyz;

	/* calc where the wall that generated this sample came from */
	vec3 worldPos	= calcLightWorldPos(fc, depth);

	/*we need to know the cube projection (for both cubemaps+shadows)*/
	vec4 cubeaxis = l_cubematrix*vec4(worldPos.xyz, 1.0);

	/*calc diffuse lighting term*/
	vec3 lightDir = l_lightposition - worldPos;
	float zdiff = 1.0 - clamp(length(lightDir) / lightAttenuation, 0.0, 1.0);
	float atten = (radiusFar * zdiff) / (radiusFar - radiusNear);
	atten = pow(atten, 2.0);
	lightDir = normalize(lightDir);
	float nDotL = dot(norm, lightDir);
	float lightDiffuse = max(0.0, nDotL) * atten;

	//fixme: apply fog
	//fixme: output a specular term
	//fixme: cubemap filters

	gl_FragColor = vec4(lightDiffuse * (lightColour * lightIntensity) * ShadowmapFilter(cubeaxis), 1.0);
}
#endif
