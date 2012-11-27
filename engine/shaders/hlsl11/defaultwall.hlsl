struct a2v
{
	float4 pos: POSITION;
	float4 tc: TEXCOORD0;
};
struct v2f
{
	float4 pos: SV_POSITION;
	float2 tc: TEXCOORD0;
	float2 lmtc: TEXCOORD1;
};

#include <ftedefs.h>

#ifdef VERTEX_SHADER
	v2f main (a2v inp)
	{
		v2f outp;
		outp.pos = mul(m_model, inp.pos);
		outp.pos = mul(m_view, outp.pos);
		outp.pos = mul(m_projection, outp.pos);
		outp.tc = inp.tc.xy;
		outp.lmtc = inp.tc.zw;
		return outp;
	}
#endif

#ifdef FRAGMENT_SHADER
	Texture2D shaderTexture[2];
	SamplerState SampleType[2];

	float4 main (v2f inp) : SV_TARGET
	{
		return shaderTexture[0].Sample(SampleType[0], inp.tc) * shaderTexture[1].Sample(SampleType[1], inp.lmtc).bgra;
	}
#endif