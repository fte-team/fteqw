#include "../plugin.h"
#include "../engine.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
//#include <libavutil/channel_layout.h>

/*
Most of the logic in here came from here:
http://svn.gnumonks.org/tags/21c3-video/upstream/ffmpeg-0.4.9-pre1/output_example.c
*/

struct encctx
{
	AVFormatContext *fc;
	qboolean doneheaders;

	AVStream *video_st;
	struct SwsContext *scale_ctx;
	AVFrame *picture;
	uint8_t *video_outbuf;
	int video_outbuf_size;

	AVStream *audio_st;
	AVFrame *audio;
};

static void AVEnc_End (void *ctx);

static AVFrame *alloc_frame(enum PixelFormat pix_fmt, int width, int height)
{
   AVFrame *picture;
   uint8_t *picture_buf;
   int size;

   picture = avcodec_alloc_frame();
   if(!picture)
      return NULL;
   size = avpicture_get_size(pix_fmt, width, height);
   picture_buf = (uint8_t*)(av_malloc(size));
   if (!picture_buf)
   {
      av_free(picture);
      return NULL;
   }
   avpicture_fill((AVPicture *) picture, picture_buf, pix_fmt, width, height);
   picture->width = width;
   picture->height = height;
   return picture;
}
AVStream *add_video_stream(struct encctx *ctx, AVCodec *codec, int fps, int width, int height)
{
	AVCodecContext *c;
	AVStream *st;
	char prof[128];
	int bitrate = (int)pCvar_GetFloat("avplug_videobitrate");
	int forcewidth = (int)pCvar_GetFloat("avplug_videoforcewidth");
	int forceheight = (int)pCvar_GetFloat("avplug_videoforceheight");

	st = avformat_new_stream(ctx->fc, codec);
	if (!st)
	{
		fprintf(stderr, "Could not alloc stream\n");
		exit(1);
	}

	c = st->codec;
	c->codec_id = codec->id;
	c->codec_type = codec->type;

	/* put sample parameters */
	c->bit_rate = bitrate;
	/* resolution must be a multiple of two */
	c->width = forcewidth?forcewidth:width;
	c->height = forceheight?forceheight:height;
	/* frames per second */
	c->time_base.num = 1;
	c->time_base.den = fps;
	c->gop_size = 12; /* emit one intra frame every twelve frames at most */
	c->pix_fmt       = PIX_FMT_YUV420P;
	if (c->codec_id == CODEC_ID_MPEG2VIDEO)
	{
		/* just for testing, we also add B frames */
		c->max_b_frames = 2;
	}
	if (c->codec_id == CODEC_ID_MPEG1VIDEO)
	{
		/* needed to avoid using macroblocks in which some coeffs overflow 
		   this doesnt happen with normal video, it just happens here as the 
		   motion of the chroma plane doesnt match the luma plane */
//		c->mb_decision=2;
	}
	// some formats want stream headers to be seperate
	if (ctx->fc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	*prof = 0;
	pCvar_GetString("avplug_format", prof, sizeof(prof));
//	av_opt_set(c->priv_data, "profile", prof, AV_OPT_SEARCH_CHILDREN);

	return st;
}
void close_video(struct encctx *ctx)
{
	if (!ctx->video_st)
		return;

	avcodec_close(ctx->video_st->codec);
	if (ctx->picture)
	{
		av_free(ctx->picture->data[0]);
		av_free(ctx->picture);
	}
	av_free(ctx->video_outbuf);
}
static void AVEnc_Video (void *vctx, void *data, int frame, int width, int height)
{
	struct encctx *ctx = vctx;
	//weird maths to flip it.
	const uint8_t *srcslices[2] = {(uint8_t*)data + (height-1)*width*3, NULL};
	int srcstride[2] = {-width*3, 0};
	int success;
	AVPacket pkt;

	if (!ctx->video_st)
		return;

	//convert RGB to whatever the codec needs (ie: yuv...).
	ctx->scale_ctx = sws_getCachedContext(ctx->scale_ctx, width, height, AV_PIX_FMT_RGB24, ctx->picture->width, ctx->picture->height, ctx->video_st->codec->pix_fmt, SWS_POINT, 0, 0, 0);
	sws_scale(ctx->scale_ctx, srcslices, srcstride, 0, height, ctx->picture->data, ctx->picture->linesize);

	av_init_packet(&pkt);
	ctx->picture->pts = av_rescale_q(frame, ctx->video_st->codec->time_base, ctx->video_st->time_base);
	success = 0;
	pkt.data = ctx->video_outbuf;
	pkt.size = ctx->video_outbuf_size;
	if (avcodec_encode_video2(ctx->video_st->codec, &pkt, ctx->picture, &success) == 0 && success)
	{
		pkt.pts = ctx->video_st->codec->coded_frame->pts;
		if(ctx->video_st->codec->coded_frame->key_frame)
			pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = ctx->video_st->index;
		pkt.data = ctx->video_outbuf;
//		pkt.size = psize;

		av_write_frame(ctx->fc, &pkt);
	}
}

AVStream *add_audio_stream(struct encctx *ctx, AVCodec *codec, int samplerate, int bits, int channels)
{
	AVCodecContext *c;
	AVStream *st;
	int bitrate = (int)pCvar_GetFloat("avplug_audiobitrate");

	st = avformat_new_stream(ctx->fc, codec);
	if (!st)
	{
		fprintf(stderr, "Could not alloc stream\n");
		exit(1);
	}

	c = st->codec;
	c->codec_id = codec->id;
	c->codec_type = codec->type;

	/* put sample parameters */
	c->bit_rate = bitrate;
	/* frames per second */
	c->sample_fmt = ((bits==16)?AV_SAMPLE_FMT_S16:AV_SAMPLE_FMT_U8);
	c->sample_rate = samplerate;
	c->channels = channels;
	switch(channels)
	{
	case 1:
		c->channel_layout = AV_CH_FRONT_CENTER;
		break;
	case 2:
		c->channel_layout = AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT;
		break;
	default:
		break;
	}

	// some formats want stream headers to be seperate
	if (ctx->fc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return st;
}
void close_audio(struct encctx *ctx)
{
	if (!ctx->audio_st)
		return;

	avcodec_close(ctx->audio_st->codec);
}
static void AVEnc_Audio (void *vctx, void *data, int bytes)
{
	struct encctx *ctx = vctx;
	int success;
	AVPacket pkt;

	ctx->audio->nb_samples = ctx->audio_st->codec->frame_size;
	if (avcodec_fill_audio_frame(ctx->audio, ctx->audio_st->codec->channels, ctx->audio_st->codec->sample_fmt, data, bytes, 0) < 0)
		return;

	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	success = 0;
	if (avcodec_encode_audio2(ctx->audio_st->codec, &pkt, ctx->audio, &success) == 0 && success)
	{
		pkt.pts = ctx->audio_st->codec->coded_frame->pts;
		if(ctx->audio_st->codec->coded_frame->key_frame)
			pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = ctx->audio_st->index;
//		pkt.data = ctx->video_outbuf;
//		pkt.size = psize;

		av_write_frame(ctx->fc, &pkt);
	}
}

static void *AVEnc_Begin (char *streamname, int videorate, int width, int height, int *sndkhz, int *sndchannels, int *sndbits)
{
	struct encctx *ctx;
	AVOutputFormat *fmt = NULL;
	AVCodec *videocodec = NULL;
	AVCodec *audiocodec = NULL;
	char formatname[64];
	formatname[0] = 0;
	pCvar_GetString("avplug_format", formatname, sizeof(formatname));

	if (*formatname)
	{
		fmt = av_guess_format(formatname, NULL, NULL);
		if (!fmt)
		{
			Con_Printf("Unknown format specified.\n");
			return NULL;
		}
	}
	if (!fmt)
		fmt = av_guess_format(NULL, streamname, NULL);
	if (!fmt)
	{
		Con_DPrintf("Could not deduce output format from file extension: using MPEG.\n");
		fmt = av_guess_format("mpeg", NULL, NULL);
	}
	if (!fmt)
	{
		Con_Printf("Format not known\n");
		return NULL;
	}

	if (videorate)
	{
		char codecname[64];
		codecname[0] = 0;
		pCvar_GetString("avplug_videocodec", codecname, sizeof(codecname));

		if (strcmp(codecname, "none"))
		{
			if (codecname[0])
			{
				videocodec = avcodec_find_encoder_by_name(codecname);
				if (!videocodec)
				{
					Con_Printf("Unsupported avplug_codec \"%s\"\n", codecname);
					return NULL;
				}
			}
			if (!videocodec && fmt->video_codec != AV_CODEC_ID_NONE)
				videocodec = avcodec_find_encoder(fmt->video_codec);
		}
	}
	if (*sndkhz)
	{
		char codecname[64];
		codecname[0] = 0;
		pCvar_GetString("avplug_audiocodec", codecname, sizeof(codecname));

		if (strcmp(codecname, "none"))
		{
			if (codecname[0])
			{
				audiocodec = avcodec_find_encoder_by_name(codecname);
				if (!audiocodec)
				{
					Con_Printf("avplug: Unsupported avplug_codec \"%s\"\n", codecname);
					return NULL;
				}
			}
			if (!audiocodec && fmt->audio_codec != AV_CODEC_ID_NONE)
				audiocodec = avcodec_find_encoder(fmt->audio_codec);
		}
	}

	Con_DPrintf("avplug: Using format \"%s\"\n", fmt->name);
	if (videocodec)
		Con_DPrintf("avplug: Using Video Codec \"%s\"\n", videocodec->name);
	else
		Con_DPrintf("avplug: Not encoding video\n");
	if (audiocodec)
		Con_DPrintf("avplug: Using Audio Codec \"%s\"\n", audiocodec->name);
	else
		Con_DPrintf("avplug: Not encoding audio\n");
	
	if (!videocodec && !audiocodec)
	{
		Con_DPrintf("avplug: Nothing to encode!\n");
		return NULL;
	}

	if (!audiocodec)
		*sndkhz = 0;

	ctx = malloc(sizeof(*ctx));
	if (!ctx)
		return NULL;
	memset(ctx, 0, sizeof(*ctx));

	ctx->fc = avformat_alloc_context();
	ctx->fc->oformat = fmt;
	Q_snprintf(ctx->fc->filename, sizeof(ctx->fc->filename), "%s", streamname);


	//pick default codecs
	ctx->video_st = NULL;
	if (videocodec)
	{
		ctx->video_st = add_video_stream(ctx, videocodec, videorate, width, height);

		if (ctx->video_st)
		{
			AVCodecContext *c = ctx->video_st->codec;
			if (avcodec_open2(c, videocodec, NULL) < 0)
			{
				Con_Printf("avplug: Could not init codec instance \"%s\". Maybe try a different framerate/resolution/bitrate\n", videocodec->name);
				AVEnc_End(ctx);
				return NULL;
			}

			ctx->picture = alloc_frame(c->pix_fmt, c->width, c->height);
		
			ctx->video_outbuf_size = 200000;
			ctx->video_outbuf = av_malloc(ctx->video_outbuf_size);
			if (!ctx->video_outbuf)
				ctx->video_outbuf_size = 0;
		}
	}
	if (audiocodec)
	{
		ctx->audio_st = add_audio_stream(ctx, audiocodec, *sndkhz, *sndbits, *sndchannels);
		if (ctx->audio_st)
		{
			AVCodecContext *c = ctx->audio_st->codec;
			if (avcodec_open2(c, audiocodec, NULL) < 0)
			{
				Con_Printf("avplug: Could not init codec instance \"%s\".\n", audiocodec->name);
				AVEnc_End(ctx);
				return NULL;
			}

			ctx->audio = avcodec_alloc_frame();
		}
	}

	av_dump_format(ctx->fc, 0, streamname, 1);

	if (!(fmt->flags & AVFMT_NOFILE))
	{
		if (avio_open(&ctx->fc->pb, streamname, AVIO_FLAG_WRITE) < 0)
		{
			Con_Printf("Could not open '%s'\n", streamname);
			AVEnc_End(ctx);
			return NULL;
		}
	}

	//nearly complete, can make the file dirty now.
	avformat_write_header(ctx->fc, NULL);
	ctx->doneheaders = true;
	return ctx;
}
static void AVEnc_End (void *vctx)
{
	struct encctx *ctx = vctx;
	unsigned int i;

	close_video(ctx);

	//don't write trailers if this is an error case and we never even wrote the headers.
	if (ctx->doneheaders)
		av_write_trailer(ctx->fc);

	for(i = 0; i < ctx->fc->nb_streams; i++)
		av_freep(&ctx->fc->streams[i]);
//	if (!(fmt->flags & AVFMT_NOFILE))
		avio_close(ctx->fc->pb);
	av_free(ctx->fc);
	free(ctx);
}
static media_encoder_funcs_t encoderfuncs =
{
	"avplug",
	AVEnc_Begin,
	AVEnc_Video,
	AVEnc_Audio,
	AVEnc_End
};

/*
qintptr_t AVEnc_ExecuteCommand(qintptr_t *args)
{
	char cmd[256];
	Cmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp(cmd, "avcapture"))
	{
menuclear
menualias menucallback

menubox 0 0 320 8
menutext 0 0 "GO GO GO!!!" 		"radio21"
menutext 0 8 "Fall back" 		"radio22"
menutext 0 8 "Stick together" 		"radio23"
menutext 0 16 "Get in position"		"radio24"
menutext 0 24 "Storm the front"	 	"radio25"
menutext 0 24 "Report in"	 	"radio26"
menutext 0 24 "Cancel"	
		return true;
	}
	return false;
}
*/

qboolean AVEnc_Init(void)
{
	pCvar_Register("avplug_format",				"mp4",			0, "avplug");

	pCvar_Register("avplug_videocodec",			"mpeg4",		0, "avplug");
	pCvar_Register("avplug_videocodecprofile",	"",				0, "avplug");
	pCvar_Register("avplug_videobitrate",		"4000000",		0, "avplug");
	pCvar_Register("avplug_videoforcewidth",	"",				0, "avplug");
	pCvar_Register("avplug_videoforceheight",	"",				0, "avplug");
	pCvar_Register("avplug_audiocodec",			"libmp3lame",	0, "avplug");
	pCvar_Register("avplug_audiobitrate",		"64000",		0, "avplug");

	if (!pPlug_ExportNative("Media_VideoEncoder", &encoderfuncs))
	{
		Con_Printf("avplug: Engine doesn't support media encoder plugins\n");
		return false;
	}

//	if (Plug_Export("ExecuteCommand", AVEnc_ExecuteCommand))
//		Cmd_AddCommand("avcapture");

	return true;
}

