	struct a2v
	{
		float4 pos: POSITION;
	};
	struct v2f
	{
#ifndef FRAGMENT_SHADER
		float4 pos: POSITION;
#endif
		float3 texc: TEXCOORD0;
	};

#ifdef VERTEX_SHADER
	float3 e_eyepos;
	float4x4  m_modelviewprojection;
	v2f main (a2v inp)
	{
		v2f outp;
		outp.pos = mul(m_modelviewprojection, inp.pos);
		outp.texc= inp.pos - e_eyepos;
		outp.texc.y = -outp.texc;
		return outp;
	}
#endif

#ifdef FRAGMENT_SHADER
	sampler s_reflectcube;
	float4 main (v2f inp) : COLOR0
	{
		return texCUBE(s_reflectcube, inp.texc);
	}
#endif