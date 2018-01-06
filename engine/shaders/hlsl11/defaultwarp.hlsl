!!samps diffuse
//!!cvarf r_wateralpha

struct a2v
{
	float4 pos: POSITION;
	float2 tc: TEXCOORD0;
};
struct v2f
{
	float4 pos: SV_POSITION;
	float2 tc: TEXCOORD0;
};

#include <ftedefs.h>

#ifdef VERTEX_SHADER
	v2f main (a2v inp)
	{
		v2f outp;
		outp.pos = mul(m_model, inp.pos);
		outp.pos = mul(m_view, outp.pos);
		outp.pos = mul(m_projection, outp.pos);
		outp.tc = inp.tc;
		return outp;
	}
#endif

#ifdef FRAGMENT_SHADER
//	float cvar_r_wateralpha;
//	float e_time;
	SamplerState s_diffuse;
	Texture2D t_diffuse;
	float4 main (v2f inp) : SV_TARGET
	{
		float2 ntc;
		float4 r;
		ntc.x = inp.tc.x + sin(inp.tc.y+e_time)*0.125;
		ntc.y = inp.tc.y + sin(inp.tc.x+e_time)*0.125;
		r = t_diffuse.Sample(s_diffuse, ntc);
#ifdef ALPHA
		r.a = float(ALPHA);
#else
//		r.a *= r_wateralpha;
#endif
		return r;
	}
#endif