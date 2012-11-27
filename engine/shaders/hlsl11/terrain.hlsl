struct a2v
{
	float4 pos: POSITION;
	float4 tc: TEXCOORD0;
	float4 vcol: COLOR0;
};
struct v2f
{
	float4 pos: SV_POSITION;
	float2 tc: TEXCOORD0;
	float2 lmtc: TEXCOORD1;
	float4 vcol: COLOR0;
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
		outp.vcol = inp.vcol;
		return outp;
	}
#endif

#ifdef FRAGMENT_SHADER
	Texture2D shaderTexture[5];
	SamplerState SampleType;

	float4 main (v2f inp) : SV_TARGET
	{
return float4(1,1,1,1);

//		float4 m = shaderTexture[4].Sample(SampleType, inp.tc) ;

//		return inp.vcol*float4(m.aaa,1.0)*(
//			  shaderTexture[0].Sample(SampleType, inp.tc)*m.r
//			+ shaderTexture[1].Sample(SampleType, inp.tc)*m.g
//			+ shaderTexture[2].Sample(SampleType, inp.tc)*m.b
//			+ shaderTexture[3].Sample(SampleType, inp.tc)*1.0 - (m.r + m.g + m.b))
//			;

	}
#endif