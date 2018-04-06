#include "../plugin.h"
#include "../engine.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

static cvar_t *ffmpeg_audiodecoder;

struct avaudioctx
{
	//raw file
	uint8_t *filedata;
	size_t fileofs;
	size_t filesize;

	//avformat stuff
	AVFormatContext *pFormatCtx;
	int audioStream;

	AVCodecContext *pACodecCtx;
	AVFrame *pAFrame;

	//decoding
	int64_t lasttime;

	//output audio
	//we throw away data if the format changes. which is awkward, but gah.
	int64_t samples_start;
	int samples_channels;
	int samples_speed;
	int samples_width;
	qbyte *samples_buffer;
	size_t samples_count;
	size_t samples_max;
};

static void S_AV_Purge(sfx_t *s)
{
	struct avaudioctx *ctx = (struct avaudioctx*)s->decoder.buf;

	s->loadstate = SLS_NOTLOADED;

	// Free the audio decoder
	if (ctx->pACodecCtx)
		avcodec_close(ctx->pACodecCtx);
	av_free(ctx->pAFrame);

	// Close the video file
	avformat_close_input(&ctx->pFormatCtx);

	//free the decoded buffer
	free(ctx->samples_buffer);

	//file storage will be cleared here too
	free(ctx);

	memset(&s->decoder, 0, sizeof(s->decoder));
}
static sfxcache_t *S_AV_Locate(sfx_t *sfx, sfxcache_t *buf, ssamplepos_t start, int length)
{	//warning: can be called on a different thread.
	struct avaudioctx *ctx = (struct avaudioctx*)sfx->decoder.buf;
	AVPacket		packet;
	int64_t			curtime;

	if (!buf)
		return NULL;

	curtime = start + length;

//	curtime = (mediatime * ctx->denum) / ctx->num;

	while (1)
	{
		if (ctx->lasttime > curtime)
			break;

		// We're ahead of the previous frame. try and read the next.
		if (av_read_frame(ctx->pFormatCtx, &packet) < 0)
			break;

		// Is this a packet from the video stream?
		if(packet.stream_index==ctx->audioStream)
		{
			int okay;
			int len;
			void *odata = packet.data;
			while (packet.size > 0)
			{
				okay = false;
				len = avcodec_decode_audio4(ctx->pACodecCtx, ctx->pAFrame, &okay, &packet);
				if (len < 0)
					break;
				packet.size -= len;
				packet.data += len;
				if (okay)
				{
					int width = 2;
					int channels = ctx->pACodecCtx->channels;
					unsigned int auddatasize = av_samples_get_buffer_size(NULL, ctx->pACodecCtx->channels, ctx->pAFrame->nb_samples, ctx->pACodecCtx->sample_fmt, 1);
					void *auddata = ctx->pAFrame->data[0];
					switch(ctx->pACodecCtx->sample_fmt)
					{	//we don't support planar audio. we just treat it as mono instead.
					default:
						auddatasize = 0;
						break;
					case AV_SAMPLE_FMT_U8P:
						auddatasize /= channels;
						channels = 1;
					case AV_SAMPLE_FMT_U8:
						width = 1;
						break;
					case AV_SAMPLE_FMT_S16P:
						auddatasize /= channels;
						channels = 1;
					case AV_SAMPLE_FMT_S16:
						width = 2;
						break;

					case AV_SAMPLE_FMT_FLTP:
						auddatasize /= channels;
						channels = 1;
					case AV_SAMPLE_FMT_FLT:
						//FIXME: support float audio internally.
						{
							float *in = (void*)auddata;
							signed short *out = (void*)auddata;
							int v;
							unsigned int i;
							for (i = 0; i < auddatasize/sizeof(*in); i++)
							{
								v = (short)(in[i]*32767);
								if (v < -32767)
									v = -32767;
								else if (v > 32767)
									v = 32767;
								out[i] = v;
							}
							auddatasize/=2;
							width = 2;
						}

					case AV_SAMPLE_FMT_DBLP:
						auddatasize /= channels;
						channels = 1;
					case AV_SAMPLE_FMT_DBL:
						{
							double *in = (double*)auddata;
							signed short *out = (void*)auddata;
							int v;
							unsigned int i;
							for (i = 0; i < auddatasize/sizeof(*in); i++)
							{
								v = (short)(in[i]*32767);
								if (v < -32767)
									v = -32767;
								else if (v > 32767)
									v = 32767;
								out[i] = v;
							}
							auddatasize/=4;
							width = 2;
						}
						break;
					}
					if (ctx->samples_channels != channels || ctx->samples_speed != ctx->pACodecCtx->sample_rate || ctx->samples_width != width)
					{	//something changed, update
						ctx->samples_channels = channels;
						ctx->samples_speed = ctx->pACodecCtx->sample_rate;
						ctx->samples_width = width;

						//and discard any decoded audio. this might loose some.
						ctx->samples_start += ctx->samples_count;
						ctx->samples_count = 0;
					}
					if (ctx->samples_max < (ctx->samples_count*ctx->samples_width*ctx->samples_channels)+auddatasize)
					{
						ctx->samples_max = (ctx->samples_count*ctx->samples_width*ctx->samples_channels)+auddatasize;
						ctx->samples_max *= 2;	//slop
						ctx->samples_buffer = realloc(ctx->samples_buffer, ctx->samples_max);
					}
					if (width == 1)
					{	//FTE uses signed 8bit audio. ffmpeg uses unsigned 8bit audio. *sigh*.
						char *out = (char*)(ctx->samples_buffer + ctx->samples_count*(ctx->samples_width*ctx->samples_channels));
						unsigned char *in = auddata;
						int i;
						for (i = 0; i < auddatasize; i++)
							out[i] = in[i]-128;
					}
					else
						memcpy(ctx->samples_buffer + ctx->samples_count*(ctx->samples_width*ctx->samples_channels), auddata, auddatasize);
					ctx->samples_count += auddatasize/(ctx->samples_width*ctx->samples_channels);
				}
			}
			packet.data = odata;
		}

		// Free the packet that was allocated by av_read_frame
		av_packet_unref(&packet);
	}

	buf->length = ctx->samples_count;
	buf->speed = ctx->samples_speed;
	buf->width = ctx->samples_width;
	buf->numchannels = ctx->samples_channels;
	buf->soundoffset = ctx->samples_start;
	buf->data = ctx->samples_buffer;

	//if we couldn't return any new data, then we're at an eof, return NULL to signal that.
	if (start == buf->soundoffset + buf->length && length > 0)
		return NULL;

	return buf;
}
static float S_AV_Query(struct sfx_s *sfx, struct sfxcache_s *buf, char *title, size_t titlesize)
{
	struct avaudioctx *ctx = (struct avaudioctx*)sfx->decoder.buf;
	if (!ctx)
		return -1;
	if (buf)
	{
		buf->data = NULL;
		buf->soundoffset = 0;
		buf->length = 0;
		buf->numchannels = ctx->samples_channels;
		buf->speed = ctx->samples_speed;
		buf->width = ctx->samples_width;
	}
	return ctx->pFormatCtx->duration / (float)AV_TIME_BASE;
}

static int AVIO_Mem_Read(void *opaque, uint8_t *buf, int buf_size)
{
	struct avaudioctx *ctx = opaque;
	if (ctx->fileofs > ctx->filesize)
		buf_size = 0;
	if (buf_size > ctx->filesize-ctx->fileofs)
		buf_size = ctx->filesize-ctx->fileofs;
	if (buf_size > 0)
	{
		memcpy(buf, ctx->filedata + ctx->fileofs, buf_size);
		ctx->fileofs += buf_size;
		return buf_size;
	}
	return 0;
}
static int64_t AVIO_Mem_Seek(void *opaque, int64_t offset, int whence)
{
	struct avaudioctx *ctx = opaque;
	whence &= ~AVSEEK_FORCE;
	switch(whence)
	{
	default:
		return -1;
	case SEEK_SET:
		ctx->fileofs = offset;
		break;
	case SEEK_CUR:
		ctx->fileofs += offset;
		break;
	case SEEK_END:
		ctx->fileofs = ctx->filesize + offset;
		break;
	case AVSEEK_SIZE:
		return ctx->filesize;
	}
	if (ctx->fileofs < 0)
		ctx->fileofs = 0;
	return ctx->fileofs;
}

/*const char *COM_GetFileExtension (const char *in)
{
	const char *dot;

	for (dot = in + strlen(in); dot >= in && *dot != '.'; dot--)
		;
	if (dot < in)
		return "";
	in = dot+1;
	return in;
}*/
static qboolean QDECL S_LoadAVSound (sfx_t *s, qbyte *data, size_t datalen, int sndspeed)
{
	struct avaudioctx *ctx;
	int i;
	AVCodec *pCodec;
	const int iBufSize = 4 * 1024;

	if (!ffmpeg_audiodecoder)
		return false;
	if (!ffmpeg_audiodecoder->value /* && *ffmpeg_audiodecoder.string */)
		return false;


	if (!data || !datalen)
		return false;

	if (datalen >= 4 && !strncmp(data, "RIFF", 4))
		return false;	//ignore it if it looks like a wav file. that means we don't need to figure out how to calculate loopstart.
//	if (strcasecmp(COM_GetFileExtension(s->name), "wav"))	//don't do .wav - I've no idea how to read the loopstart tag with ffmpeg.
//		return false;

	s->decoder.buf = ctx = malloc(sizeof(*ctx) + datalen);
	if (!ctx)
		return false;	//o.O
	memset(ctx, 0, sizeof(*ctx));

	// Create internal io buffer for FFmpeg
	ctx->filedata = data;	//defer that copy
	ctx->filesize = datalen;	//defer that copy
	ctx->pFormatCtx = avformat_alloc_context();
	ctx->pFormatCtx->pb = avio_alloc_context(av_malloc(iBufSize), iBufSize, 0, ctx, AVIO_Mem_Read, 0, AVIO_Mem_Seek);

	// Open file
	if(avformat_open_input(&ctx->pFormatCtx, s->name, NULL, NULL)==0)
	{
		// Retrieve stream information
		if(avformat_find_stream_info(ctx->pFormatCtx, NULL)>=0)
		{
			ctx->audioStream=-1;
			for(i=0; i<ctx->pFormatCtx->nb_streams; i++)
				if(ctx->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
				{
					ctx->audioStream=i;
					break;
				}
			if(ctx->audioStream!=-1)
			{
				ctx->pACodecCtx=ctx->pFormatCtx->streams[ctx->audioStream]->codec;
				pCodec=avcodec_find_decoder(ctx->pACodecCtx->codec_id);

				ctx->pAFrame=av_frame_alloc();
				if(pCodec!=NULL && ctx->pAFrame && avcodec_open2(ctx->pACodecCtx, pCodec, NULL) >= 0)
				{	//success
				}
				else
					ctx->audioStream = -1;
			}
		}

		if (ctx->audioStream != -1)
		{
			//sucky copy
			ctx->filedata = (uint8_t*)(ctx+1);
			memcpy(ctx->filedata, data, datalen);

			s->decoder.ended = S_AV_Purge;
			s->decoder.purge = S_AV_Purge;
			s->decoder.decodedata = S_AV_Locate;
			s->decoder.querydata = S_AV_Query;
			return true;
		}
	}
	S_AV_Purge(s);
	return false;
}
static qboolean AVAudio_Init(void)
{
	if (!pPlug_ExportNative("S_LoadSound", S_LoadAVSound))
	{
		ffmpeg_audiodecoder = pCvar_GetNVFDG("ffmpeg_audiodecoder_wip", "0", 0, "Enables the use of ffmpeg's decoder for pure audio files.", "ffmpeg");

		Con_Printf("avplug: Engine doesn't support audio decoder plugins\n");
		return false;
	}
	return true;
}


//generic module stuff. this has to go somewhere.
static void AVLogCallback(void *avcl, int level, const char *fmt, va_list vl)
{	//needs to be reenterant
#ifdef _DEBUG
	char		string[1024];
	Q_vsnprintf (string, sizeof(string), fmt, vl);
	pCon_Print(string);
#endif
}

//get the encoder/decoders to register themselves with the engine, then make sure avformat/avcodec have registered all they have to give.
qboolean AVEnc_Init(void);
qboolean AVDec_Init(void);
qintptr_t Plug_Init(qintptr_t *args)
{
	qboolean okay = false;

	okay |= AVAudio_Init();
	okay |= AVDec_Init();
	okay |= AVEnc_Init();
	if (okay)
	{
		av_register_all();
		avcodec_register_all();

		av_log_set_level(AV_LOG_WARNING);
		av_log_set_callback(AVLogCallback);
	}
	return okay;
}

