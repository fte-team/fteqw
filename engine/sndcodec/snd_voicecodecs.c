#include "bothdefs.h"

#ifdef VOICECHAT

#include "quakedef.h"
#include "voicechat.h"



#include "g72x.h"

int VC_g72x_encoder (short *in, unsigned char *out, int samples, int (*g72x_encoder)(int,int,struct g72x_state *state), int bits)
{
	struct g72x_state state;
	unsigned int	out_buffer = 0;
	int		out_bits = 0;
	unsigned char		out_byte;
	int code;
	int written=0;

	g72x_init_state(&state);

	while (samples)
	{
		code = g72x_encoder(*in, AUDIO_ENCODING_LINEAR, &state);

		out_buffer |= (code << out_bits);
		out_bits += bits;
		if (out_bits >= 8)
		{
			out_byte = out_buffer & 0xff;
			out_bits -= 8;
			out_buffer >>= 8;
			*out = out_byte;
			out++;
			written++;
		}
		in++;

		samples--;
	}
	code=0;
	while (out_bits > 0)
	{
		out_buffer |= (code << out_bits);
		out_bits += bits;
		if (out_bits >= 8)
		{
			out_byte = out_buffer & 0xff;
			out_bits -= 8;
			out_buffer >>= 8;
			*out = out_byte;
			out++;
			written++;
		}
	}

	return written;
}

int VC_g723_24_encoder (short *in, unsigned char *out, int samples)
{
	return VC_g72x_encoder(in, out, samples, g723_24_encoder, 3);
}

int VC_g721_encoder (short *in, unsigned char *out, int samples)
{
	return VC_g72x_encoder(in, out, samples, g721_encoder, 4);
}

int VC_g723_40_encoder (short *in, unsigned char *out, int samples)
{
	return VC_g72x_encoder(in, out, samples, g723_40_encoder, 5);
}

int VC_g72x_decoder (unsigned char *in, short *out, int samples, int (*g72x_decoder)(int,int,struct g72x_state *state), int bits)
{
	struct g72x_state state;
	unsigned int	in_buffer = 0;
	int		in_bits = 0;
	unsigned char		in_byte;
	int code;
	int read=0;

	g72x_init_state(&state);

	while (samples)
	{
		if (in_bits < bits)
		{
			in_byte = *in++;
			read++;
			in_buffer |= (in_byte << in_bits);
			in_bits += 8;
		}
		code = in_buffer & ((1 << bits) - 1);
		in_buffer >>= bits;
		in_bits -= bits;

		*out = g72x_decoder(code, AUDIO_ENCODING_LINEAR, &state);
		out++;

		samples--;
	}

	return read;
}

int VC_g723_24_decoder (unsigned char *in, short *out, int samples)
{
	return VC_g72x_decoder(in, out, samples, g723_24_decoder, 3);
}

int VC_g721_decoder (unsigned char *in, short *out, int samples)
{
	return VC_g72x_decoder(in, out, samples, g721_decoder, 4);
}

int VC_g723_40_decoder (unsigned char *in, short *out, int samples)
{
	return VC_g72x_decoder(in, out, samples, g723_40_decoder, 5);
}















int VS_Raw_enc (short *in, unsigned char *out, int samples)
{
	memcpy(out, in, samples*2);
	return samples*2;
}

int VS_Raw_dec (unsigned char *in, short *out, int samples)
{
	memcpy(out, in, samples*2);
	return samples*2;
}

























audiocodec_t audiocodecs[] = {
	{"Raw 11025sps 16bit", VS_Raw_enc, VS_Raw_dec},
#ifdef _G72X_H
	{"G.723.24",	VC_g723_24_encoder,	VC_g723_24_decoder},
	{"G.721 32",	VC_g721_encoder,	VC_g721_decoder},
	{"G.723.40",	VC_g723_40_encoder,	VC_g723_40_decoder},
#else
	{"Non-implemented codec (G.723.24)"},
	{"Non-implemented codec (G.721 32)"},
	{"Non-implemented codec (G.723.40)"},
#endif
	{0}
};

const int audionumcodecs = sizeof(audiocodecs)/sizeof(audiocodec_t);
#endif
