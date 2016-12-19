#include "../plugin.h"
#include "../engine.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
//#include <libavutil/channel_layout.h>

/*
Most of the logic in here came from here:
http://svn.gnumonks.org/tags/21c3-video/upstream/ffmpeg-0.4.9-pre1/output_example.c
*/

struct encctx
{
	char abspath[MAX_OSPATH];
	AVFormatContext *fc;
	qboolean doneheaders;

	AVStream *video_st;
	struct SwsContext *scale_ctx;
	AVFrame *picture;
	uint8_t *video_outbuf;
	int video_outbuf_size;

	AVStream *audio_st;
	AVFrame *audio;
	uint8_t *audio_outbuf;
	uint32_t audio_outcount;
	int64_t audio_pts;
};

#define VARIABLE_AUDIO_FRAME_MIN_SIZE 512	//audio frames smaller than a certain size are just wasteful
#define VARIABLE_AUDIO_FRAME_MAX_SIZE 1024

static void AVEnc_End (void *ctx);

static AVFrame *alloc_frame(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	uint8_t *picture_buf;
	int size;

	picture = av_frame_alloc();
	if(!picture)
		return NULL;
	size = av_image_get_buffer_size(pix_fmt, width, height, 1);
	picture_buf = (uint8_t*)(av_malloc(size));
	if (!picture_buf)
	{
		av_free(picture);
		return NULL;
	}
	av_image_fill_arrays(picture->data, picture->linesize, picture_buf, pix_fmt, width, height, 1/*fixme: align*/);
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
		return NULL;

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
	//c->gop_size = 12; /* emit one intra frame every twelve frames at most */
	c->pix_fmt       = AV_PIX_FMT_YUV420P;
	if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
	{
		/* just for testing, we also add B frames */
		c->max_b_frames = 2;
	}
	if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
	{
		/* needed to avoid using macroblocks in which some coeffs overflow 
		   this doesnt happen with normal video, it just happens here as the 
		   motion of the chroma plane doesnt match the luma plane */
//		c->mb_decision=2;
	}
	// some formats want stream headers to be seperate
	if (ctx->fc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	pCvar_GetString("avplug_videopreset", prof, sizeof(prof));
	if (*prof)
		av_opt_set(c->priv_data, "preset", prof, AV_OPT_SEARCH_CHILDREN);
	pCvar_GetString("avplug_video_crf", prof, sizeof(prof));
	if (*prof)
		av_opt_set(c->priv_data, "crf", prof, AV_OPT_SEARCH_CHILDREN);


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
static void AVEnc_Video (void *vctx, void *data, int frame, int width, int height, enum uploadfmt qpfmt)
{
	struct encctx *ctx = vctx;
	const uint8_t *srcslices[2];
	int srcstride[2];
	int success;
	AVPacket pkt;
	int avpfmt;
	int inbpp;
	int err;

	if (!ctx->video_st)
		return;

	switch(qpfmt)
	{
	case TF_BGRA32: avpfmt = AV_PIX_FMT_BGRA; inbpp = 4; break;
	case TF_RGBA32: avpfmt = AV_PIX_FMT_RGBA; inbpp = 4; break;
	case TF_BGR24: avpfmt = AV_PIX_FMT_BGR24; inbpp = 3; break;
	case TF_RGB24: avpfmt = AV_PIX_FMT_RGB24; inbpp = 3; break;
	default:
		return;
	}

	//weird maths to flip it.
	srcslices[0] = (uint8_t*)data + (height-1)*width*inbpp;
	srcstride[0] = -width*inbpp;
	srcslices[1] = NULL;
	srcstride[1] = 0;

	//convert RGB to whatever the codec needs (ie: yuv...).
	//also rescales, but only if the user resizes the video while recording. which is a stupid thing to do.
	ctx->scale_ctx = sws_getCachedContext(ctx->scale_ctx, width, height, avpfmt, ctx->picture->width, ctx->picture->height, ctx->video_st->codec->pix_fmt, SWS_POINT, 0, 0, 0);
	sws_scale(ctx->scale_ctx, srcslices, srcstride, 0, height, ctx->picture->data, ctx->picture->linesize);

	av_init_packet(&pkt);
	ctx->picture->pts = frame;
	success = 0;
	pkt.data = ctx->video_outbuf;
	pkt.size = ctx->video_outbuf_size;
	ctx->picture->format = ctx->video_st->codec->pix_fmt;
	err = avcodec_encode_video2(ctx->video_st->codec, &pkt, ctx->picture, &success);
	if (err)
	{
		char buf[512];
		Con_Printf("avcodec_encode_video2: error: %s\n", av_make_error_string(buf, sizeof(buf), err));
	}
	else if (err == 0 && success)
	{
//		pkt.pts = ctx->video_st->codec->coded_frame->pts;
//		if(ctx->video_st->codec->coded_frame->key_frame)
//			pkt.flags |= AV_PKT_FLAG_KEY;
		av_packet_rescale_ts(&pkt, ctx->video_st->codec->time_base, ctx->video_st->time_base);
		pkt.stream_index = ctx->video_st->index;
		err = av_interleaved_write_frame(ctx->fc, &pkt);
	
		if (err)
		{
			char buf[512];
			Con_Printf("av_interleaved_write_frame: error: %s\n", av_make_error_string(buf, sizeof(buf), err));
		}
	}
}

AVStream *add_audio_stream(struct encctx *ctx, AVCodec *codec, int samplerate, int *bits, int channels)
{
	AVCodecContext *c;
	AVStream *st;
	int bitrate = (int)pCvar_GetFloat("avplug_audiobitrate");

	st = avformat_new_stream(ctx->fc, codec);
	if (!st)
		return NULL;

	st->id = ctx->fc->nb_streams-1;
	c = st->codec;
	c->codec_id = codec->id;
	c->codec_type = codec->type;

	/* put sample parameters */
	c->bit_rate = bitrate;
	/* frames per second */
	c->time_base.num = 1;
	c->time_base.den = samplerate;
	c->sample_rate = samplerate;
	c->channels = channels;
#if 0
	switch(channels)
	{
	case 1:
		c->channel_layout = AV_CH_LAYOUT_MONO;
		break;
	case 2:
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		break;
	default:
		break;
	}
#else
	c->channel_layout = av_get_default_channel_layout(c->channels);
#endif

	c->channel_layout = av_get_default_channel_layout(c->channels);
	c->sample_fmt = codec->sample_fmts[0];

//	if (c->sample_fmt == AV_SAMPLE_FMT_FLTP || c->sample_fmt == AV_SAMPLE_FMT_FLT)
//		*bits = 32;	//get the engine to mix 32bit audio instead of whatever its currently set to.
//	else if (c->sample_fmt == AV_SAMPLE_FMT_U8P || c->sample_fmt == AV_SAMPLE_FMT_U8)
//		*bits = 8;	//get the engine to mix 32bit audio instead of whatever its currently set to.
//	else if (c->sample_fmt == AV_SAMPLE_FMT_S16P || c->sample_fmt == AV_SAMPLE_FMT_S16)
//		*bits = 16;
//	else
		*bits = 32;

	c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	// some formats want stream headers to be seperate
	if (ctx->fc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;

//	avcodec_parameters_from_context(st->codecpar, c);

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
	int err;

	if (!ctx->audio_st)
		return;

	while (bytes)
	{
		int i, p, chans = ctx->audio_st->codec->channels;
		int blocksize = sizeof(float)*chans;
		int count = bytes / blocksize;
		int planesize = ctx->audio_st->codec->frame_size;
		float *in;
		int offset;

		if (!planesize)	//variable-sized frames. yay
		{
			planesize = count;
			if (count > VARIABLE_AUDIO_FRAME_MAX_SIZE - ctx->audio_outcount)
				count = VARIABLE_AUDIO_FRAME_MAX_SIZE - ctx->audio_outcount;
		}
		else if (count > ctx->audio_st->codec->frame_size - ctx->audio_outcount)
			count = ctx->audio_st->codec->frame_size - ctx->audio_outcount;

		in = (float*)data;
		offset = ctx->audio_outcount;
		ctx->audio_outcount += count;
		data = (qbyte*)data + count * blocksize;
		bytes -= count * blocksize;

		//input is always float audio, because I'm lazy.
		//output is whatever the codec needs (may be packed or planar, gah).
		//the engine's mixer will do all rate scaling for us, as well as channel selection
		switch(ctx->audio_st->codec->sample_fmt)
		{
		case AV_SAMPLE_FMT_DBL:
			offset *= chans;
			count *= chans;
			planesize *= chans;
			chans = 1;
		case AV_SAMPLE_FMT_DBLP:
			for (p = 0; p < chans; p++)
			{
				double *f = (double*)ctx->audio_outbuf + p*planesize + offset;
				for (i = 0; i < count*chans; i+=chans)
					*f++ = in[i];
				in++;
			}
			break;
		case AV_SAMPLE_FMT_FLT:
			offset *= chans;
			count *= chans;
			planesize *= chans;
			chans = 1;
		case AV_SAMPLE_FMT_FLTP:
			for (p = 0; p < chans; p++)
			{
				float *f = (float *)ctx->audio_outbuf + p*planesize + offset;
				for (i = 0; i < count*chans; i+=chans)
					*f++ = in[i];
				in++;
			}
			break;
		case AV_SAMPLE_FMT_S32:
			offset *= chans;
			count *= chans;
			planesize *= chans;
			chans = 1;
		case AV_SAMPLE_FMT_S32P:
			for (p = 0; p < chans; p++)
			{
				int32_t *f = (int32_t *)ctx->audio_outbuf + p*planesize + offset;
				for (i = 0; i < count*chans; i+=chans)
					*f++ = bound(0x80000000, (in[i] * 0x7fffffff), 0x7fffffff);
				in++;
			}
			break;
		case AV_SAMPLE_FMT_S16:
			offset *= chans;
			count *= chans;
			planesize *= chans;
			chans = 1;
		case AV_SAMPLE_FMT_S16P:
			for (p = 0; p < chans; p++)
			{
				int16_t *f = (int16_t *)ctx->audio_outbuf + p*planesize + offset;
				for (i = 0; i < count*chans; i+=chans)
					*f++ = bound(-32768, (int)(in[i] * 32767), 32767);

				//sin((ctx->audio_pts+ctx->audio_outcount-count+i/chans)*0.1) * 32767;//
				in++;
			}
			break;
		case AV_SAMPLE_FMT_U8:
			offset *= chans;
			count *= chans;
			planesize *= chans;
			chans = 1;
		case AV_SAMPLE_FMT_U8P:
			for (p = 0; p < chans; p++)
			{
				uint8_t *f = (uint8_t*)ctx->audio_outbuf + p*planesize + offset;
				for (i = 0; i < count*chans; i+=chans)
					*f++ = bound(0, 128+(int)(in[i] * 127), 255);
				in++;
			}
			break;
		default:
			return;
		}

		if (ctx->audio_st->codec->frame_size)
		{
			if (ctx->audio_outcount < ctx->audio_st->codec->frame_size)
				break;	//not enough data yet.
		}
		else
		{
			if (ctx->audio_outcount < VARIABLE_AUDIO_FRAME_MIN_SIZE)
				break;	//not enough data yet.
		}

		ctx->audio->nb_samples = ctx->audio_outcount;
		avcodec_fill_audio_frame(ctx->audio, ctx->audio_st->codec->channels, ctx->audio_st->codec->sample_fmt, ctx->audio_outbuf, av_get_bytes_per_sample(ctx->audio_st->codec->sample_fmt)*ctx->audio_outcount*ctx->audio_st->codec->channels, 1);

		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		success = 0;
		ctx->audio->pts = ctx->audio_pts;
		ctx->audio_pts += ctx->audio_outcount;
		ctx->audio_outcount = 0;
		err = avcodec_encode_audio2(ctx->audio_st->codec, &pkt, ctx->audio, &success);

		if (err)
		{
			char buf[512];
			Con_Printf("avcodec_encode_audio2: error: %s\n", av_make_error_string(buf, sizeof(buf), err));
		}
		else if (success)
		{
	//		pkt.pts = ctx->audio_st->codec->coded_frame->pts;
	//		if(ctx->audio_st->codec->coded_frame->key_frame)
	//			pkt.flags |= AV_PKT_FLAG_KEY;

			av_packet_rescale_ts(&pkt, ctx->audio_st->codec->time_base, ctx->audio_st->time_base);
			pkt.stream_index = ctx->audio_st->index;
			err = av_interleaved_write_frame(ctx->fc, &pkt);
			if (err)
			{
				char buf[512];
				Con_Printf("av_interleaved_write_frame: error: %s\n", av_make_error_string(buf, sizeof(buf), err));
			}
		}
	}
}

static void *AVEnc_Begin (char *streamname, int videorate, int width, int height, int *sndkhz, int *sndchannels, int *sndbits)
{
	struct encctx *ctx;
	AVOutputFormat *fmt = NULL;
	AVCodec *videocodec = NULL;
	AVCodec *audiocodec = NULL;
	char formatname[64];
	int err;
	formatname[0] = 0;
	pCvar_GetString("avplug_format_force", formatname, sizeof(formatname));

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
					Con_Printf("Unsupported avplug_videocodec \"%s\"\n", codecname);
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
					Con_Printf("avplug: Unsupported avplug_audiocodec \"%s\"\n", codecname);
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
		ctx->video_st = add_video_stream(ctx, videocodec, videorate, width, height);
	if (audiocodec)
		ctx->audio_st = add_audio_stream(ctx, audiocodec, *sndkhz, sndbits, *sndchannels);

	if (ctx->video_st)
	{
		AVCodecContext *c = ctx->video_st->codec;
		err = avcodec_open2(c, videocodec, NULL);
		if (err < 0)
		{
			char buf[512];
			Con_Printf("avplug: Could not init codec instance \"%s\" - %s\nMaybe try a different framerate/resolution/bitrate\n", videocodec->name, av_make_error_string(buf, sizeof(buf), err));
			AVEnc_End(ctx);
			return NULL;
		}

		ctx->picture = alloc_frame(c->pix_fmt, c->width, c->height);

		ctx->video_outbuf_size = 200000;
		ctx->video_outbuf = av_malloc(ctx->video_outbuf_size);
		if (!ctx->video_outbuf)
			ctx->video_outbuf_size = 0;
	}
	if (ctx->audio_st)
	{
		int sz;
		AVCodecContext *c = ctx->audio_st->codec;
		err = avcodec_open2(c, audiocodec, NULL);
		if (err < 0)
		{
			char buf[512];
			Con_Printf("avplug: Could not init codec instance \"%s\" - %s\n", audiocodec->name, av_make_error_string(buf, sizeof(buf), err));
			AVEnc_End(ctx);
			return NULL;
		}

		ctx->audio = av_frame_alloc();
		sz = ctx->audio_st->codec->frame_size;
		if (!sz)
			sz = VARIABLE_AUDIO_FRAME_MAX_SIZE;
		sz *= av_get_bytes_per_sample(ctx->audio_st->codec->sample_fmt) * ctx->audio_st->codec->channels;
		ctx->audio_outbuf = av_malloc(sz);
	}

	av_dump_format(ctx->fc, 0, streamname, 1);

	if (!(fmt->flags & AVFMT_NOFILE))
	{
		//okay, this is annoying, but I'm too lazy to figure out the issue I was having with avio stuff.
		if (!pFS_NativePath(streamname, FS_GAMEONLY, ctx->abspath, sizeof(ctx->abspath)) || avio_open(&ctx->fc->pb, ctx->abspath, AVIO_FLAG_WRITE) < 0)
		{
			Con_Printf("Could not open '%s'\n", streamname);
			AVEnc_End(ctx);
			return NULL;
		}
	}

	//nearly complete, can make the file dirty now.
	err = avformat_write_header(ctx->fc, NULL);
	if (err < 0)
	{
		char buf[512];
		Con_Printf("avformat_write_header: failed %s\n", av_make_error_string(buf, sizeof(buf), err));
		AVEnc_End(ctx);
		return NULL;
	}
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
	{
		av_write_trailer(ctx->fc);
		if (*ctx->abspath)
			Con_Printf("Finished writing %s\n", ctx->abspath);
	}

	for(i = 0; i < ctx->fc->nb_streams; i++)
		av_freep(&ctx->fc->streams[i]);
//	if (!(fmt->flags & AVFMT_NOFILE))
		avio_close(ctx->fc->pb);
	av_free(ctx->audio_outbuf);
	av_free(ctx->fc);
	free(ctx);
}
static media_encoder_funcs_t encoderfuncs =
{
	sizeof(media_encoder_funcs_t),
	"avplug",
	"Use ffmpeg's various codecs. Various settings are configured with the avplug_* cvars.",
	".mp4",
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
	CHECKBUILTIN(FS_NativePath);
	if (!BUILTINISVALID(FS_NativePath))
	{
		Con_Printf("avplug: Engine too old\n");
		return false;
	}
	if (!pPlug_ExportNative("Media_VideoEncoder", &encoderfuncs))
	{
		Con_Printf("avplug: Engine doesn't support media encoder plugins\n");
		return false;
	}

	pCvar_Register("avplug_format_force",		"",				0, "avplug");

	pCvar_Register("avplug_videocodec",			"",				0, "avplug");
	pCvar_Register("avplug_videobitrate",		"4000000",		0, "avplug");
	pCvar_Register("avplug_videoforcewidth",	"",				0, "avplug");
	pCvar_Register("avplug_videoforceheight",	"",				0, "avplug");
	pCvar_Register("avplug_videopreset",		"veryfast",		0, "avplug");
	pCvar_Register("avplug_audiocodec",			"",				0, "avplug");
	pCvar_Register("avplug_audiobitrate",		"64000",		0, "avplug");

//	if (Plug_Export("ExecuteCommand", AVEnc_ExecuteCommand))
//		Cmd_AddCommand("avcapture");

	return true;
}

