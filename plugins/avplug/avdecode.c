#include "../plugin.h"
#include "../engine.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

//between av 52.31 and 54.35, lots of constants etc got renamed to gain an extra AV_ prefix.
/*
#define AV_PIX_FMT_BGRA PIX_FMT_BGRA
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#define AV_PIX_FMT_BGRA PIX_FMT_BGRA
#define AV_SAMPLE_FMT_U8 SAMPLE_FMT_U8
#define AV_SAMPLE_FMT_S16 SAMPLE_FMT_S16
#define AV_SAMPLE_FMT_FLT SAMPLE_FMT_FLT
#define AVIOContext ByteIOContext
#define avio_alloc_context av_alloc_put_byte
*/

#define PASSFLOAT(f) *(int*)&(f)

#define ARGNAMES ,sourceid, data, speed, samples, channels, width, PASSFLOAT(volume)
BUILTIN(void, S_RawAudio, (int sourceid, void *data, int speed, int samples, int channels, int width, float volume));
#undef ARGNAMES

/*should probably try threading this*/
/*timing is based upon the start time. this means overflow issues with rtsp etc*/

struct decctx
{
	unsigned int width, height;

	qhandle_t file;
	int64_t fileofs;
	int64_t filelen;
	AVFormatContext *pFormatCtx;

	int audioStream;
	AVCodecContext  *pACodecCtx;
	AVFrame         *pAFrame;

	int videoStream;
	AVCodecContext  *pVCodecCtx;
	AVFrame         *pVFrame;
	int64_t num, denum;

	AVPicture        pFrameRGB;
	struct SwsContext		*pScaleCtx;

	unsigned int	starttime;
	unsigned int	lastframe;
};

static qboolean AVDec_SetSize (void *vctx, int width, int height)
{
	struct decctx	*ctx = (struct decctx*)vctx;
	AVPicture		newscaled;

	//colourspace conversions will be fastest if we
//	if (width > ctx->pCodecCtx->width)
		width = ctx->pVCodecCtx->width;
//	if (height > ctx->pCodecCtx->height)
		height = ctx->pVCodecCtx->height;

	//is this a no-op?
	if (width == ctx->width && height == ctx->height && ctx->pScaleCtx)
		return true;

	if (avpicture_alloc(&newscaled, AV_PIX_FMT_BGRA, width, height) >= 0)
	{
		//update the scale context as required
		//clear the old stuff out
		avpicture_free(&ctx->pFrameRGB);

		ctx->width = width;
		ctx->height = height;
		ctx->pFrameRGB = newscaled;
		return qtrue;
	}
	return qfalse;	//unsupported
}

static int AVIO_Read(void *opaque, uint8_t *buf, int buf_size)
{
	struct decctx *ctx = opaque;
	int ammount;
	ammount = pFS_Read(ctx->file, buf, buf_size);
	if (ammount > 0)
		ctx->fileofs += ammount;
	return ammount;
}
static int64_t AVIO_Seek(void *opaque, int64_t offset, int whence)
{
	struct decctx *ctx = opaque;
	int64_t ret = ctx->fileofs;
	whence &= ~AVSEEK_FORCE;
	switch(whence)
	{
	case SEEK_SET:
	default:
		ctx->fileofs = offset;
		break;
	case SEEK_CUR:
		ctx->fileofs += offset;
		break;
	case SEEK_END:
		ctx->fileofs = ctx->filelen + offset;
		break;
	case AVSEEK_SIZE:
		return ctx->filelen;
	}
	pFS_Seek(ctx->file, ctx->fileofs & 0xffffffff, ctx->fileofs>>32);
	return ctx->fileofs;
}

static void AVDec_Destroy(void *vctx)
{
	struct decctx *ctx = (struct decctx*)vctx;

	// Free the video stuff
	avpicture_free(&ctx->pFrameRGB);
	if (ctx->pVCodecCtx)
		avcodec_close(ctx->pVCodecCtx);
	av_free(ctx->pVFrame);

	// Free the audio decoder
	if (ctx->pACodecCtx)
		avcodec_close(ctx->pACodecCtx);
	av_free(ctx->pAFrame);

	// Close the video file
	avformat_close_input(&ctx->pFormatCtx);

	if (ctx->file >= 0)
		pFS_Close(ctx->file);

	free(ctx);
}

static void *AVDec_Create(char *medianame)
{
	struct decctx *ctx;

	unsigned int             i;
	AVCodec         *pCodec;
	qboolean useioctx = false;

	/*only respond to av: media prefixes*/
	if (!strncmp(medianame, "av:", 3))
	{
		medianame = medianame + 3;
		useioctx = true;
	}
	else if (!strncmp(medianame, "avs:", 4))
	{
		medianame = medianame + 4;
		//let avformat do its own avio context stuff
	}
	else
		return NULL;

	ctx = malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));

	//so we always decode the first frame instantly.

	ctx->starttime = pSys_Milliseconds();

	ctx->file = -1;
	if (useioctx)
	{
		// Create internal Buffer for FFmpeg:
		const int iBufSize = 32 * 1024;
		char *pBuffer = malloc(iBufSize);
		AVIOContext *ioctx;

		ctx->filelen = pFS_Open(medianame, &ctx->file, 1);
		if (ctx->filelen < 0)
		{
			Con_Printf("Unable to open %s\n", medianame);
			free(ctx);
			free(pBuffer);
			return NULL;
		}

		ioctx = avio_alloc_context(pBuffer, iBufSize, 0, ctx, AVIO_Read, 0, AVIO_Seek);
		ctx->pFormatCtx = avformat_alloc_context();

		ctx->pFormatCtx->pb = ioctx;
	}
	/*
small how-to note for if I ever try to add support for voice-and-video rtp decoding.
this stuff is presumably needed to handle ICE+stun+ports etc.
I prolly need to hack around with adding rtcp too. :s

rtsp: Add support for depacketizing RTP data via custom IO

To use this, set sdpflags=custom_io to the sdp demuxer. During
the avformat_open_input call, the SDP is read from the AVFormatContext
AVIOContext (ctx->pb) - after the avformat_open_input call,
during the av_read_frame() calls, the same ctx->pb is used for reading
packets (and sending back RTCP RR packets).

Normally, one would use this with a read-only AVIOContext for the
SDP during the avformat_open_input call, then close that one and
replace it with a read-write one for the packets after the
avformat_open_input call has returned.

This allows using the RTP depacketizers as "pure" demuxers, without
having them tied to the libavformat network IO.
	*/


	// Open video file
	if(avformat_open_input(&ctx->pFormatCtx, medianame, NULL, NULL)==0)
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

				ctx->pAFrame=avcodec_alloc_frame();
				if(pCodec!=NULL && ctx->pAFrame && avcodec_open2(ctx->pACodecCtx, pCodec, NULL) >= 0)
				{

				}
				else
					ctx->audioStream = -1;
			}

			ctx->videoStream=-1;
			for(i=0; i<ctx->pFormatCtx->nb_streams; i++)
				if(ctx->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
				{
					ctx->videoStream=i;
					break;
				}
			if(ctx->videoStream!=-1)
			{
				// Get a pointer to the codec context for the video stream
				ctx->pVCodecCtx=ctx->pFormatCtx->streams[ctx->videoStream]->codec;
				ctx->num = ctx->pFormatCtx->streams[ctx->videoStream]->time_base.num;
				ctx->denum = ctx->pFormatCtx->streams[ctx->videoStream]->time_base.den;

				// Find the decoder for the video stream
				pCodec=avcodec_find_decoder(ctx->pVCodecCtx->codec_id);

				// Open codec
				if(pCodec!=NULL && avcodec_open2(ctx->pVCodecCtx, pCodec, NULL) >= 0)
				{
					// Allocate video frame
					ctx->pVFrame=avcodec_alloc_frame();
					if(ctx->pVFrame!=NULL)
					{
						if (AVDec_SetSize(ctx, ctx->pVCodecCtx->width, ctx->pVCodecCtx->height))
						{
							return ctx;
						}
					}
				}
			}
		}
	}
	AVDec_Destroy(ctx);
	return NULL;
}

static void *AVDec_DisplayFrame(void *vctx, qboolean nosound, uploadfmt_t *fmt, int *width, int *height)
{
	struct decctx *ctx = (struct decctx*)vctx;
	AVPacket        packet;
	int             frameFinished;
	qboolean		repainted = false;
	int64_t curtime, lasttime;

	curtime = ((pSys_Milliseconds() - ctx->starttime) * ctx->denum);
	curtime /= (ctx->num * 1000);
	
	*fmt = TF_BGRA32;
	while (1)
	{
		lasttime = av_frame_get_best_effort_timestamp(ctx->pVFrame);

		if (lasttime > curtime)
			break;

		// We're ahead of the previous frame. try and read the next.
		if (av_read_frame(ctx->pFormatCtx, &packet) < 0)
		{
			*fmt = TF_INVALID;
			break;
		}

		// Is this a packet from the video stream?
		if(packet.stream_index==ctx->videoStream)
		{
			// Decode video frame
			avcodec_decode_video2(ctx->pVCodecCtx, ctx->pVFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if(frameFinished)
			{
				ctx->pScaleCtx = sws_getCachedContext(ctx->pScaleCtx, ctx->pVCodecCtx->width, ctx->pVCodecCtx->height, ctx->pVCodecCtx->pix_fmt, ctx->width, ctx->height, AV_PIX_FMT_BGRA, SWS_POINT, 0, 0, 0);

				// Convert the image from its native format to RGB
				sws_scale(ctx->pScaleCtx, (void*)ctx->pVFrame->data, ctx->pVFrame->linesize, 0, ctx->pVCodecCtx->height, ctx->pFrameRGB.data, ctx->pFrameRGB.linesize);

				repainted = true;
			}
		}
		else if(packet.stream_index==ctx->audioStream && !nosound)
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
					unsigned int auddatasize = av_samples_get_buffer_size(NULL, ctx->pACodecCtx->channels, ctx->pAFrame->nb_samples, ctx->pACodecCtx->sample_fmt, 1);
					void *auddata = ctx->pAFrame->data[0];
					switch(ctx->pACodecCtx->sample_fmt)
					{
					default:
						auddatasize = 0;
						break;
					case AV_SAMPLE_FMT_U8:
						width = 1;
						break;
					case AV_SAMPLE_FMT_S16:
						width = 2;
						break;
					case AV_SAMPLE_FMT_FLT:
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
						break;
					}
					pS_RawAudio(-1, auddata, ctx->pACodecCtx->sample_rate, auddatasize/(ctx->pACodecCtx->channels*width), ctx->pACodecCtx->channels, width, 1);
				}
			}
			packet.data = odata;
		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

	*width = ctx->width;
	*height = ctx->height;
	if (!repainted)
		return NULL;
	return ctx->pFrameRGB.data[0];
}
static void AVDec_GetSize (void *vctx, int *width, int *height)
{
	struct decctx *ctx = (struct decctx*)vctx;
	*width = ctx->width;
	*height = ctx->height;
}

/*static void AVDec_CursorMove (void *vctx, float posx, float posy)
{
	//its a video, dumbass
}
static void AVDec_Key (void *vctx, int code, int unicode, int isup)
{
	//its a video, dumbass
}
static void AVDec_ChangeStream(void *vctx, char *newstream)
{
}
*/
static void AVDec_Rewind(void *vctx)
{
	struct decctx *ctx = (struct decctx*)vctx;
	if (ctx->videoStream >= 0)
		av_seek_frame(ctx->pFormatCtx, ctx->videoStream, 0, AVSEEK_FLAG_BACKWARD);
	if (ctx->audioStream >= 0)
		av_seek_frame(ctx->pFormatCtx, ctx->audioStream, 0, AVSEEK_FLAG_BACKWARD);

	ctx->starttime = pSys_Milliseconds();
}

/*
//avcodec has no way to shut down properly.
static qintptr_t AVDec_Shutdown(qintptr_t *args)
{
	return 0;
}
*/

static media_decoder_funcs_t decoderfuncs =
{
	"avplug",
	AVDec_Create,
	AVDec_DisplayFrame,
	NULL,//doneframe
	AVDec_Destroy,
	AVDec_Rewind,

	NULL,//AVDec_CursorMove,
	NULL,//AVDec_Key,
	NULL,//AVDec_SetSize,
	AVDec_GetSize,
	NULL,//AVDec_ChangeStream
};

static qboolean AVDec_Init(void)
{
	if (!pPlug_ExportNative("Media_VideoDecoder", &decoderfuncs))
	{
		Con_Printf("avplug: Engine doesn't support media decoder plugins\n");
		return false;
	}

	CHECKBUILTIN(S_RawAudio);
	CHECKBUILTIN(FS_Seek);

	return true;
}

//get the encoder/decoders to register themselves with the engine, then make sure avformat/avcodec have registered all they have to give.
qboolean AVEnc_Init(void);
qintptr_t Plug_Init(qintptr_t *args)
{
	qboolean okay = false;

	okay |= AVDec_Init();
	okay |= AVEnc_Init();
	if (okay)
	{
		av_register_all();
		avcodec_register_all();
	}
	return okay;
}

