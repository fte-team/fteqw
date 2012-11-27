#include "../plugin.h"
#include "../engine.h"

#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>
#include <windows.h>

/*should probably try threading this*/
/*timing is based upon the start time. this means overflow issues with rtsp etc*/

struct decctx
{
	unsigned int width, height;

	AVFormatContext *pFormatCtx;
	int videoStream;
	AVCodecContext  *pCodecCtx;
	AVFrame         *pFrame;
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
		width = ctx->pCodecCtx->width;
//	if (height > ctx->pCodecCtx->height)
		height = ctx->pCodecCtx->height;

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

static void *AVDec_Create(char *medianame)
{
	struct decctx *ctx;

	unsigned int             i;
	AVCodec         *pCodec;

	/*only respond to av: media prefixes*/
	if (!strncmp(medianame, "av:", 3))
		medianame = medianame + 3;
	else
		return NULL;

	ctx = malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));

	//so we always decode the first frame instantly.

	ctx->starttime = timeGetTime();

	// Open video file
	if(avformat_open_input(&ctx->pFormatCtx, medianame, NULL, NULL)==0)
	{
		// Retrieve stream information
		if(av_find_stream_info(ctx->pFormatCtx)>=0)
		{
			// Find the first video stream
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
				ctx->pCodecCtx=ctx->pFormatCtx->streams[ctx->videoStream]->codec;
				ctx->num = ctx->pFormatCtx->streams[ctx->videoStream]->time_base.num;
				ctx->denum = ctx->pFormatCtx->streams[ctx->videoStream]->time_base.den;

				// Find the decoder for the video stream
				pCodec=avcodec_find_decoder(ctx->pCodecCtx->codec_id);

				// Open codec
				if(pCodec!=NULL && avcodec_open(ctx->pCodecCtx, pCodec) >= 0)
				{
					// Allocate video frame
					ctx->pFrame=avcodec_alloc_frame();
					if(ctx->pFrame!=NULL)
					{
						if (AVDec_SetSize(ctx, ctx->pCodecCtx->width, ctx->pCodecCtx->height))
						{
							return ctx;
						}
						av_free(ctx->pFrame);
					}
					avcodec_close(ctx->pCodecCtx);
				}
			}
		}
		av_close_input_file(ctx->pFormatCtx);
	}
	free(ctx);
	return NULL;
}

static void *AVDec_DisplayFrame(void *vctx, qboolean nosound, enum uploadfmt_e *fmt, int *width, int *height)
{
	struct decctx *ctx = (struct decctx*)vctx;
	AVPacket        packet;
	int             frameFinished;
	qboolean		repainted = false;
	int64_t curtime, lasttime;

	curtime = ((timeGetTime() - ctx->starttime) * ctx->denum);
	curtime /= (ctx->num * 1000);
	
	*fmt = TF_BGRA32;
	while (1)
	{
		lasttime = av_frame_get_best_effort_timestamp(ctx->pFrame);

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
			avcodec_decode_video2(ctx->pCodecCtx, ctx->pFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if(frameFinished)
			{
				ctx->pScaleCtx = sws_getCachedContext(ctx->pScaleCtx, ctx->pCodecCtx->width, ctx->pCodecCtx->height, ctx->pCodecCtx->pix_fmt, ctx->width, ctx->height, AV_PIX_FMT_BGRA, SWS_POINT, 0, 0, 0);

				// Convert the image from its native format to RGB
				sws_scale(ctx->pScaleCtx, ctx->pFrame->data, ctx->pFrame->linesize, 0, ctx->pCodecCtx->height, ctx->pFrameRGB.data, ctx->pFrameRGB.linesize);

				repainted = true;
			}
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
static void AVDec_Destroy(void *vctx)
{
	struct decctx *ctx = (struct decctx*)vctx;

    // Free the RGB image
    avpicture_free(&ctx->pFrameRGB);

    // Free the YUV frame
    av_free(ctx->pFrame);

    // Close the codec
    avcodec_close(ctx->pCodecCtx);

    // Close the video file
    av_close_input_file(ctx->pFormatCtx);

	free(ctx);
}
static void AVDec_GetSize (void *vctx, int *width, int *height)
{
	struct decctx *ctx = (struct decctx*)vctx;
	*width = ctx->width;
	*height = ctx->height;
}

static void AVDec_CursorMove (void *vctx, float posx, float posy)
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
static void AVDec_Rewind(void *ctx)
{
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
	if (!Plug_ExportNative("Media_VideoDecoder", &decoderfuncs))
	{
		Con_Printf("avplug: Engine doesn't support media decoder plugins\n");
		return false;
	}

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
		av_register_all();
	return okay;
}

