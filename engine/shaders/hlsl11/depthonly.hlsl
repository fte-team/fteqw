//used for generating shadowmaps and stuff that draws nothing.

struct a2v
{
	float4 pos: POSITION;
	float3 normal: NORMAL;
	#ifdef MASK
		float4 tc: TEXCOORD0;
	#endif
};
struct v2f
{
	float4 pos: SV_POSITION;
	#ifdef MASK
		float2 tc: TEXCOORD0;
	#endif
};

#include <ftedefs.h>

#ifdef VERTEX_SHADER
	v2f main (a2v inp)
	{
		v2f outp;
		outp.pos = mul(m_model, inp.pos);
		outp.pos = mul(m_view, outp.pos);
		outp.pos = mul(m_projection, outp.pos);

		#ifdef MASK
			outp.tc = inp.tc.xy;
		#endif

		return outp;
	}
#endif
#ifdef FRAGMENT_SHADER
	#ifdef MASK
		Texture2D shaderTexture[1];
		SamplerState SampleType[1];
	#endif

	#if LEVEL < 1000
	//pre dx10 requires that we ALWAYS write to a target.
	float4 main (v2f inp) : SV_TARGET
	#else
	//but on 10, it'll write depth automatically and we don't care about colour.
	void main (v2f inp)	//dx10-level
	#endif
	{

	#ifdef MASK
		float alpha = shaderTexture[0].Sample(SampleType[0], inp.tc).a;
		#ifndef MASKOP
			#define MASKOP >=	//drawn if (alpha OP ref) is true.
		#endif
		//support for alpha masking
		if (!(alpha MASKOP MASK))
			discard;
	#endif

	#if LEVEL < 1000
		return float4(0, 0, 0, 1);
	#endif
	}
#endif
