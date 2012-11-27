//regular sky shader for scrolling q1 skies
//the sky surfaces are thrown through this as-is.

struct a2v
{
	float4 pos: POSITION;
	float2 tc: TEXCOORD0;
};
struct v2f
{
	float4 pos: SV_POSITION;
	float2 tc: TEXCOORD0;
	float3 mpos: TEXCOORD1;
};

#include <ftedefs.h>

#ifdef VERTEX_SHADER
	v2f main (a2v inp)
	{
		v2f outp;
		outp.pos = mul(m_model, inp.pos);
		outp.mpos = outp.pos.xyz;
		outp.pos = mul(m_view, outp.pos);
		outp.pos = mul(m_projection, outp.pos);
		outp.tc = inp.tc;
		return outp;
	}
#endif

#ifdef FRAGMENT_SHADER
	Texture2D shaderTexture[2];
	SamplerState SampleType;

	float4 main (v2f inp) : SV_TARGET
	{
		float2 tccoord;
		float3 dir = inp.mpos - v_eyepos;
		dir.z *= 3.0;
		dir.xy /= 0.5*length(dir);
		tccoord = (dir.xy + e_time*0.03125);
		float4 solid = shaderTexture[0].Sample(SampleType, tccoord);
		tccoord = (dir.xy + e_time*0.0625);
		float4 clouds = shaderTexture[1].Sample(SampleType, tccoord);
		return (solid*(1.0-clouds.a)) + (clouds.a*clouds);
	}
#endif