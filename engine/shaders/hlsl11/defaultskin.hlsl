struct a2v
{
	float4 pos: POSITION;
	float4 tc: TEXCOORD0;
	float3 normal: NORMAL;
};
struct v2f
{
	float4 pos: SV_POSITION;
	float2 tc: TEXCOORD0;
	float3 light: TEXCOORD1;
};

#include <ftedefs.h>

#ifdef VERTEX_SHADER
//attribute vec2 v_texcoord;
//uniform vec3 e_light_dir;
//uniform vec3 e_light_mul;
//uniform vec3 e_light_ambient;
	v2f main (a2v inp)
	{
		v2f outp;
		outp.pos = mul(m_model, inp.pos);
		outp.pos = mul(m_view, outp.pos);
		outp.pos = mul(m_projection, outp.pos);
		outp.light = e_light_ambient + (dot(inp.normal,e_light_dir)*e_light_mul);
		outp.tc = inp.tc.xy;
		return outp;
	}
#endif
#ifdef FRAGMENT_SHADER
	Texture2D shaderTexture[4];	//diffuse, lower, upper, fullbright
	SamplerState SampleType;

//uniform vec4 e_colourident;
	float4 main (v2f inp) : SV_TARGET
	{
		float4 col;
		col = shaderTexture[0].Sample(SampleType, inp.tc);

		#ifdef MASK
			#ifndef MASKOP
				#define MASKOP >=	//drawn if (alpha OP ref) is true.
			#endif
			//support for alpha masking
			if (!(col.a MASKOP MASK))
				discard;
		#endif

#ifdef UPPER
		float4 uc = shaderTexture[2].Sample(SampleType, inp.tc);
		col.rgb = mix(col.rgb, uc.rgb*e_uppercolour, uc.a);
#endif
#ifdef LOWER
		float4 lc = shaderTexture[1].Sample(SampleType, inp.tc);
		col.rgb = mix(col.rgb, lc.rgb*e_lowercolour, lc.a);
#endif
		col.rgb *= inp.light;
#ifdef FULLBRIGHT
		float4 fb = shaderTexture[3].Sample(SampleType, inp.tc);
		col.rgb = mix(col.rgb, fb.rgb, fb.a);
#endif
		return col;
//		return fog4(col * e_colourident);
	}
#endif
