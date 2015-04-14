/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_dma.c -- main control for any streaming sound output devices

#include "quakedef.h"

static void S_Play(void);
static void S_PlayVol(void);
static void S_SoundList_f(void);
static void S_Update_(soundcardinfo_t *sc);
void S_StopAllSounds(qboolean clear);
static void S_StopAllSounds_f (void);

static void S_UpdateCard(soundcardinfo_t *sc);
static void S_ClearBuffer (soundcardinfo_t *sc);
sfx_t *S_FindName (const char *name, qboolean create);

// =======================================================================
// Internal sound data & structures
// =======================================================================

soundcardinfo_t *sndcardinfo;	//the master card.

int				snd_blocked = 0;
static qboolean	snd_ambient = 1;
qboolean		snd_initialized = false;
int				snd_speed;

vec3_t		listener_origin;
vec3_t		listener_forward = {1, 0, 0};
vec3_t		listener_right = {0, 1, 0};
vec3_t		listener_up = {0, 0, 1};
vec3_t		listener_velocity;
vec_t		sound_nominal_clip_dist=1000.0;

#define	MAX_SFX		8192
sfx_t		*known_sfx;		// hunk allocated [MAX_SFX]
int			num_sfx;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

//int 		desired_speed = 44100;
int 		desired_bits = 16;

int sound_started=0;

cvar_t bgmvolume				= CVARAFD(	"musicvolume", "0.3", "bgmvolume", CVAR_ARCHIVE,
											"Volume level for background music.");
cvar_t volume					= CVARFD(	"volume", "0.7", CVAR_ARCHIVE,
											"Main volume level for all engine sound.");

cvar_t nosound					= CVARFD(	"nosound", "0", CVAR_ARCHIVE,
											"Disable all sound from the engine. Cannot be overriden by configs or anything if set via the -nosound commandline argument.");
cvar_t precache					= CVARAF(	"s_precache", "1",
											"precache", 0);
cvar_t loadas8bit				= CVARAFD(	"s_loadas8bit", "0",
											"loadas8bit", CVAR_ARCHIVE,
											"Downsample sounds on load as lower quality 8-bit sound.");
cvar_t ambient_level			= CVARAF(	"s_ambientlevel", "0.3",
											"ambient_level", 0);
cvar_t ambient_fade				= CVARAF(	"s_ambientfade", "100",
											"ambient_fade", 0);
cvar_t snd_noextraupdate		= CVARAF(	"s_noextraupdate", "0",
											"snd_noextraupdate", 0);
cvar_t snd_show					= CVARAF(	"s_show", "0",
											"snd_show", 0);
cvar_t snd_khz					= CVARAFD(	"s_khz", "48",
											"snd_khz", CVAR_ARCHIVE, "Sound speed, in kilohertz. Common values are 11, 22, 44, 48. Values above 1000 are explicitly in hertz.");
cvar_t	snd_inactive			= CVARAFD(	"s_inactive", "1",
											"snd_inactive", 0,
											"Play sound while application is inactive (ex. tabbed out). Needs a snd_restart if changed."
											);	//set if you want sound even when tabbed out.
cvar_t _snd_mixahead			= CVARAFD(	"s_mixahead", "0.1",
											"_snd_mixahead", CVAR_ARCHIVE, "Specifies how many seconds to prebuffer audio. Lower values give less latency, but might result in crackling. Different hardware/drivers have different tolerances.");
cvar_t snd_leftisright			= CVARAF(	"s_swapstereo", "0",
											"snd_leftisright", CVAR_ARCHIVE);
cvar_t snd_eax					= CVARAF(	"s_eax", "0",
											"snd_eax", 0);
cvar_t snd_speakers				= CVARAFD(	"s_numspeakers", "2",
											"snd_numspeakers", 0, "Number of hardware audio channels to use. "DISTRIBUTION" supports up to 6.");
cvar_t snd_buffersize			= CVARAF(	"s_buffersize", "0",
											"snd_buffersize", 0);
cvar_t snd_samplebits			= CVARAF(	"s_bits", "16",
											"snd_samplebits", CVAR_ARCHIVE);
cvar_t snd_playersoundvolume	= CVARAFD(	"s_localvolume", "1",
											"snd_localvolume", 0,
											"Sound level for sounds local or originating from the player such as firing and pain sounds.");	//sugested by crunch

cvar_t snd_playbackrate			= CVARFD(	"snd_playbackrate", "1", CVAR_CHEAT, "Debugging cvar that changes the playback rate of all new sounds.");

cvar_t snd_linearresample		= CVARAF(	"s_linearresample", "1",
											"snd_linearresample", 0);
cvar_t snd_linearresample_stream = CVARAF(	"s_linearresample_stream", "0",
											"snd_linearresample_stream", 0);

cvar_t snd_mixerthread			= CVARAD(	"s_mixerthread", "1",
											"snd_mixerthread", "When enabled sound mixing will be run on a separate thread. Currently supported only by directsound. Other drivers may unconditionally thread audio. Set to 0 only if you have issues.");
cvar_t snd_device				= CVARAF(	"s_device", "",
											"snd_device", CVAR_ARCHIVE);
cvar_t snd_device_opts			= CVARFD(	"_s_device_opts", "", CVAR_NOSET, "The possible audio output devices, in \"value\" \"description\" pairs, for gamecode to read.");

#ifdef VOICECHAT
static void S_Voip_Play_Callback(cvar_t *var, char *oldval);
cvar_t snd_voip_capturedevice	= CVARF("cl_voip_capturedevice", "", CVAR_ARCHIVE);
cvar_t snd_voip_capturedevice_opts	= CVARFD("_cl_voip_capturedevice_opts", "", CVAR_NOSET, "The possible audio capture devices, in \"value\" \"description\" pairs, for gamecode to read.");
int voipbutton;	//+voip, no longer part of cl_voip_send to avoid it getting saved
cvar_t snd_voip_send			= CVARFD("cl_voip_send", "0", CVAR_ARCHIVE, "Sends voice-over-ip data to the server whenever it is set.\n0: only send voice if +voip is pressed.\n1: voice activation.\n2: constantly send.\n+4: Do not send to game, only to rtp sessions.");
cvar_t snd_voip_test			= CVARD("cl_voip_test", "0", "If 1, enables you to hear your own voice directly, bypassing the server and thus without networking latency, but is fine for checking audio levels. Note that sv_voip_echo can be set if you want to include latency and packetloss considerations, but setting that cvar requires server admin access and is thus much harder to use.");
cvar_t snd_voip_vad_threshhold	= CVARD("cl_voip_vad_threshhold", "15", "This is the threshhold for voice-activation-detection when sending voip data");
cvar_t snd_voip_vad_delay		= CVARD("cl_voip_vad_delay", "0.3", "Keeps sending voice data for this many seconds after voice activation would normally stop");
cvar_t snd_voip_capturingvol	= CVARAFD("cl_voip_capturingvol", "0.5", NULL, CVAR_ARCHIVE, "Volume multiplier applied while capturing, to avoid your audio from being heard by others. Does not affect game volume when other speak (minimum of cl_voip_capturingvol and cl_voip_ducking is used).");
cvar_t snd_voip_showmeter		= CVARAFD("cl_voip_showmeter", "1", NULL, CVAR_ARCHIVE, "Shows your speech volume above the standard hud. 0=hide, 1=show when transmitting, 2=ignore voice-activation disable");

cvar_t snd_voip_play			= CVARAFDC("cl_voip_play", "1", NULL, CVAR_ARCHIVE, "Enables voip playback. Value is a volume scaler.", S_Voip_Play_Callback);
cvar_t snd_voip_ducking			= CVARAFD("cl_voip_ducking", "0.5", NULL, CVAR_ARCHIVE, "Scales game audio by this much when someone is talking to you. Does not affect your speaker volume when you speak (minimum of cl_voip_capturingvol and cl_voip_ducking is used).");
cvar_t snd_voip_micamp			= CVARAFDC("cl_voip_micamp", "2", NULL, CVAR_ARCHIVE, "Amplifies your microphone when using voip.", 0);
cvar_t snd_voip_codec			= CVARAFDC("cl_voip_codec", "0", NULL, CVAR_ARCHIVE, "0: speex(@11khz). 1: raw. 2: opus. 3: speex(@8khz). 4: speex(@16). 5:speex(@32).", 0);
cvar_t snd_voip_noisefilter		= CVARAFDC("cl_voip_noisefilter", "1", NULL, CVAR_ARCHIVE, "Enable the use of the noise cancelation filter.", 0);
cvar_t snd_voip_autogain		= CVARAFDC("cl_voip_autogain", "0", NULL, CVAR_ARCHIVE, "Attempts to normalize your voice levels to a standard level. Useful for lazy people, but interferes with voice activation levels.", 0);
#endif

extern vfsfile_t *rawwritefile;
#ifdef MULTITHREAD
void *mixermutex;
void S_LockMixer(void)
{
	Sys_LockMutex(mixermutex);
}
void S_UnlockMixer(void)
{
	Sys_UnlockMutex(mixermutex);
}
#else
void S_LockMixer(void)
{
}
void S_UnlockMixer(void)
{
}
#endif

void S_AmbientOff (void)
{
	snd_ambient = false;
}


void S_AmbientOn (void)
{
	snd_ambient = true;
}

qboolean S_HaveOutput(void)
{
	return sound_started && sndcardinfo;
}


void S_SoundInfo_f(void)
{
	int i, j;
	int active, known;
	soundcardinfo_t *sc;
	if (!sound_started)
	{
		Con_Printf ("sound system not started\n");
		return;
	}

	if (!sndcardinfo)
	{
		Con_Printf ("No sound cards\n");
		return;
	}
	for (sc = sndcardinfo; sc; sc = sc->next)
	{
		Con_Printf("Audio Device: %s\n", sc->name);
		Con_Printf(" %d channels, %gkhz, %d bit audio%s\n", sc->sn.numchannels, sc->sn.speed/1000.0, sc->sn.samplebits, sc->selfpainting?", threaded":"");
		Con_Printf(" %d samples in buffer\n", sc->sn.samples);
		for (i = 0, active = 0, known = 0; i < sc->total_chans; i++)
		{
			if (sc->channel[i].sfx)
			{
				known++;
				for (j = 0; j < MAXSOUNDCHANNELS; j++)
				{
					if (sc->channel[i].vol[j])
					{
						active++;
						break;
					}
				}
				if (j<MAXSOUNDCHANNELS)
					Con_Printf(" %s (%i %i, %g %g %g, active)\n", sc->channel[i].sfx->name, sc->channel[i].entnum, sc->channel[i].entchannel, sc->channel[i].origin[0], sc->channel[i].origin[1], sc->channel[i].origin[2]);
				else
					Con_DPrintf(" %s (%i %i, %g %g %g, inactive)\n", sc->channel[i].sfx->name, sc->channel[i].entnum, sc->channel[i].entchannel, sc->channel[i].origin[0], sc->channel[i].origin[1], sc->channel[i].origin[2]);
			}
		}
		Con_Printf(" %d/%d/%d/%d active/known/highest/max\n", active, known, sc->total_chans, MAX_CHANNELS);
		for (i = 0; i < sc->sn.numchannels; i++)
		{
			Con_Printf(" chan %i: fwd:%-4g rt:%-4g up:%-4g dist:%-4g\n", i, sc->speakerdir[i][0], sc->speakerdir[i][1], sc->speakerdir[i][2], sc->dist[i]);
		}
	}
}

#ifdef VOICECHAT
#include <speex.h>
#include <speex_preprocess.h>

enum
{
	VOIP_SPEEX_OLD	= 0,	//original supported codec (with needless padding and at the wrong rate to keep quake implementations easy)
	VOIP_RAW		= 1,	//support is not recommended.
	VOIP_OPUS		= 2,	//supposed to be better than speex.
	VOIP_SPEEX_NARROW = 3,	//narrowband speex. packed data.
	VOIP_SPEEX_WIDE = 4,	//wideband speex. packed data.
	VOIP_SPEEX_ULTRAWIDE = 5,//wideband speex. packed data.

	VOIP_INVALID = 16	//not currently generating audio.
};
static struct
{
	struct
	{
		qboolean inited;
		qboolean loaded;
		dllhandle_t *speexlib;

		SpeexBits encbits;
		SpeexBits decbits[MAX_CLIENTS];

		const SpeexMode *modenb;
		const SpeexMode *modewb;
		const SpeexMode *modeuwb;
	} speex;

	struct
	{
		qboolean inited;
		qboolean loaded;
		dllhandle_t *speexdsplib;

		SpeexPreprocessState *preproc;	//filter out noise
		int curframesize;
		int cursamplerate;
	} speexdsp;

	struct
	{
		qboolean inited;
		qboolean loaded;
		dllhandle_t *opuslib;
	} opus;

	unsigned char enccodec;
	void *encoder;
	unsigned int encframesize;
	unsigned int encsamplerate;

	void *decoder[MAX_CLIENTS];
	unsigned char deccodec[MAX_CLIENTS];
	unsigned char decseq[MAX_CLIENTS];	/*sender's sequence, to detect+cover minor packetloss*/
	unsigned char decgen[MAX_CLIENTS];	/*last generation. if it changes, we flush speex to reset packet loss*/
	unsigned int decsamplerate[MAX_CLIENTS];
	unsigned int decframesize[MAX_CLIENTS];
	float lastspoke[MAX_CLIENTS];	/*time when they're no longer considered talking. if future, they're talking*/
	float lastspoke_any;

	unsigned char capturebuf[32768]; /*pending data*/
	unsigned int capturepos;/*amount of pending data*/
	unsigned int encsequence;/*the outgoing sequence count*/
	unsigned int enctimestamp;/*for rtp streaming*/
	unsigned int generation;/*incremented whenever capture is restarted*/
	qboolean wantsend;	/*set if we're capturing data to send*/
	float voiplevel;	/*your own voice level*/
	unsigned int dumps;	/*trigger a new generation thing after a bit*/
	unsigned int keeps;	/*for vad_delay*/

	snd_capture_driver_t *cdriver;/*capture driver's functions*/
	void *cdriverctx;	/*capture driver context*/
} s_voip;

#define OPUS_APPLICATION_VOIP				2048
#define OPUS_RESET_STATE					4028
#ifdef OPUS_STATIC
#include "opus.h"
#define qopus_encoder_create	opus_encoder_create
#define qopus_encoder_destroy	opus_encoder_destroy
#define qopus_encoder_ctl		opus_encoder_ctl
#define qopus_encode			opus_encode
#define qopus_decoder_create	opus_decoder_create
#define qopus_decoder_destroy	opus_decoder_destroy
#define qopus_decoder_ctl		opus_decoder_ctl
#define qopus_decode			opus_decode
#else
#define opus_int32 int
#define opus_int16 short
#define OpusEncoder void
#define OpusDecoder void
static OpusEncoder *(VARGS *qopus_encoder_create)(opus_int32 Fs, int channels, int application, int *error);
static void (VARGS *qopus_encoder_destroy)(OpusEncoder *st);
static int (VARGS *qopus_encoder_ctl)(OpusEncoder *st, int request, ...);
static opus_int32 (VARGS *qopus_encode)(OpusEncoder *st, const opus_int16 *pcm, int frame_size, unsigned char *data, opus_int32 max_data_bytes);
static OpusDecoder *(VARGS *qopus_decoder_create)(opus_int32 Fs, int channels, int *error);
static void (VARGS *qopus_decoder_destroy)(OpusDecoder *st);
static int (VARGS *qopus_decoder_ctl)(OpusDecoder *st, int request, ...);
static int (VARGS *qopus_decode)(OpusDecoder *st, const unsigned char *data, opus_int32 len, opus_int16 *pcm, int frame_size, int decode_fec);
static dllfunction_t qopusfuncs[] =
{
	{(void*)&qopus_encoder_create,	"opus_encoder_create"},
	{(void*)&qopus_encoder_destroy,	"opus_encoder_destroy"},
	{(void*)&qopus_encoder_ctl,		"opus_encoder_ctl"},
	{(void*)&qopus_encode,			"opus_encode"},

	{(void*)&qopus_decoder_create,	"opus_decoder_create"},
	{(void*)&qopus_decoder_destroy,	"opus_decoder_destroy"},
	{(void*)&qopus_decoder_ctl,		"opus_decoder_ctl"},
	{(void*)&qopus_decode,			"opus_decode"},

	{NULL}
};
#endif


#ifdef SPEEX_STATIC
#define qspeex_lib_get_mode speex_lib_get_mode
#define qspeex_bits_init speex_bits_init
#define qspeex_bits_reset speex_bits_reset
#define qspeex_bits_write speex_bits_write

#define qspeex_preprocess_state_init speex_preprocess_state_init
#define qspeex_preprocess_state_destroy speex_preprocess_state_destroy
#define qspeex_preprocess_ctl speex_preprocess_ctl
#define qspeex_preprocess_run speex_preprocess_run

#define qspeex_encoder_init speex_encoder_init
#define qspeex_encoder_destroy speex_encoder_destroy
#define qspeex_encoder_ctl speex_encoder_ctl
#define qspeex_encode_int speex_encode_int

#define qspeex_decoder_init speex_decoder_init
#define qspeex_decoder_destroy speex_decoder_destroy
#define qspeex_decode_int speex_decode_int
#define qspeex_bits_read_from speex_bits_read_from
#else
static const SpeexMode *(VARGS *qspeex_lib_get_mode)(int mode);
static void (VARGS *qspeex_bits_init)(SpeexBits *bits);
static void (VARGS *qspeex_bits_reset)(SpeexBits *bits);
static int (VARGS *qspeex_bits_write)(SpeexBits *bits, char *bytes, int max_len);

static SpeexPreprocessState *(VARGS *qspeex_preprocess_state_init)(int frame_size, int sampling_rate);
static void (VARGS *qspeex_preprocess_state_destroy)(SpeexPreprocessState *st);
static int (VARGS *qspeex_preprocess_ctl)(SpeexPreprocessState *st, int request, void *ptr);
static int (VARGS *qspeex_preprocess_run)(SpeexPreprocessState *st, spx_int16_t *x);

static void * (VARGS *qspeex_encoder_init)(const SpeexMode *mode);
static int (VARGS *qspeex_encoder_ctl)(void *state, int request, void *ptr);
static int (VARGS *qspeex_encode_int)(void *state, spx_int16_t *in, SpeexBits *bits);

static void *(VARGS *qspeex_decoder_init)(const SpeexMode *mode);
static void (VARGS *qspeex_decoder_destroy)(void *state);
static int (VARGS *qspeex_decode_int)(void *state, SpeexBits *bits, spx_int16_t *out);
static void (VARGS *qspeex_bits_read_from)(SpeexBits *bits, char *bytes, int len);

static dllfunction_t qspeexfuncs[] =
{
	{(void*)&qspeex_lib_get_mode, "speex_lib_get_mode"},
	{(void*)&qspeex_bits_init, "speex_bits_init"},
	{(void*)&qspeex_bits_reset, "speex_bits_reset"},
	{(void*)&qspeex_bits_write, "speex_bits_write"},

	{(void*)&qspeex_encoder_init, "speex_encoder_init"},
	{(void*)&qspeex_encoder_ctl, "speex_encoder_ctl"},
	{(void*)&qspeex_encode_int, "speex_encode_int"},

	{(void*)&qspeex_decoder_init, "speex_decoder_init"},
	{(void*)&qspeex_decoder_destroy, "speex_decoder_destroy"},
	{(void*)&qspeex_decode_int, "speex_decode_int"},
	{(void*)&qspeex_bits_read_from, "speex_bits_read_from"},

	{NULL}
};
static dllfunction_t qspeexdspfuncs[] =
{
	{(void*)&qspeex_preprocess_state_init, "speex_preprocess_state_init"},
	{(void*)&qspeex_preprocess_state_destroy, "speex_preprocess_state_destroy"},
	{(void*)&qspeex_preprocess_ctl, "speex_preprocess_ctl"},
	{(void*)&qspeex_preprocess_run, "speex_preprocess_run"},

	{NULL}
};
#endif

#ifdef AVAIL_OPENAL
extern snd_capture_driver_t OPENAL_Capture;
#endif
snd_capture_driver_t DSOUND_Capture;
snd_capture_driver_t OSS_Capture;

snd_capture_driver_t *capturedrivers[] =
{
	&DSOUND_Capture,
	&OSS_Capture,
#ifdef AVAIL_OPENAL
	&OPENAL_Capture,
#endif
	NULL
};

static qboolean S_SpeexDSP_Init(void)
{
#ifndef SPEEX_STATIC
	if (s_voip.speexdsp.inited)
		return s_voip.speexdsp.loaded;
	s_voip.speexdsp.inited = true;

	
	s_voip.speexdsp.speexdsplib = Sys_LoadLibrary("libspeexdsp", qspeexdspfuncs);
	if (!s_voip.speexdsp.speexdsplib)
	{
		Con_Printf("libspeexdsp not found. Your mic may be noisy.\n");
		return false;
	}
#endif

	s_voip.speexdsp.loaded = true;
	return s_voip.speexdsp.loaded;
}

static qboolean S_Speex_Init(void)
{
#ifndef SPEEX_STATIC
	if (s_voip.speex.inited)
		return s_voip.speex.loaded;
	s_voip.speex.inited = true;

	s_voip.speex.speexlib = Sys_LoadLibrary("libspeex", qspeexfuncs);
	if (!s_voip.speex.speexlib)
	{
		Con_Printf("libspeex not found. Voice chat is not available.\n");
		return false;
	}
#endif

	s_voip.speex.modenb = qspeex_lib_get_mode(SPEEX_MODEID_NB);
	s_voip.speex.modewb = qspeex_lib_get_mode(SPEEX_MODEID_WB);
	s_voip.speex.modeuwb = qspeex_lib_get_mode(SPEEX_MODEID_UWB);

	s_voip.speex.loaded = true;
	return s_voip.speex.loaded;
}

static qboolean S_Opus_Init(void)
{
#ifndef OPUS_STATIC
#ifdef _WIN32
	char *modulename = "libopus-0" ARCH_DL_POSTFIX;
#else
	char *modulename = "libopus"ARCH_DL_POSTFIX".0";
#endif

	if (s_voip.opus.inited)
		return s_voip.opus.loaded;
	s_voip.opus.inited = true;

	s_voip.opus.opuslib = Sys_LoadLibrary(modulename, qopusfuncs);
	if (!s_voip.opus.opuslib)
	{
		Con_Printf("%s not found. Voice chat is not available.\n", modulename);
		return false;
	}
#endif

	Con_Printf("OPUS support is experimental and should not be used\n");	//need to remove the packet length prefix.

	s_voip.opus.loaded = true;
	return s_voip.opus.loaded;
}

void S_Voip_Decode(unsigned int sender, unsigned int codec, unsigned int gen, unsigned char seq, unsigned int bytes, unsigned char *data)
{
	unsigned char *start;
	short decodebuf[8192];
	unsigned int decodesamps, len, drops;
	int r;

	if (sender >= MAX_CLIENTS)
		return;

	decodesamps = 0;
	drops = 0;
	start = data;

	s_voip.lastspoke[sender] = realtime + 0.5;
	if (s_voip.lastspoke[sender] > s_voip.lastspoke_any)
		s_voip.lastspoke_any = s_voip.lastspoke[sender];

	//if they re-started speaking, flush any old state to avoid things getting weirdly delayed and reset the codec properly.
	if (s_voip.decgen[sender] != gen || s_voip.deccodec[sender] != codec)
	{
		S_RawAudio(sender, NULL, s_voip.decsamplerate[sender], 0, 1, 2, 0);

		if (s_voip.deccodec[sender] != codec)
		{
			//make sure old state is closed properly.
			switch(s_voip.deccodec[sender])
			{
			case VOIP_SPEEX_OLD:
			case VOIP_SPEEX_NARROW:
			case VOIP_SPEEX_WIDE:
			case VOIP_SPEEX_ULTRAWIDE:
				qspeex_decoder_destroy(s_voip.decoder[sender]);
				break;
			case VOIP_RAW:
				break;
			case VOIP_OPUS:
				qopus_decoder_destroy(s_voip.decoder[sender]);
				break;
			}
			s_voip.decoder[sender] = NULL;
			s_voip.deccodec[sender] = VOIP_INVALID;
		}

		switch(codec)
		{
		default:	//codec not supported.
			return;
		case VOIP_RAW:
			s_voip.decsamplerate[sender] = 11025;
			break;
		case VOIP_SPEEX_OLD:
		case VOIP_SPEEX_NARROW:
		case VOIP_SPEEX_WIDE:
		case VOIP_SPEEX_ULTRAWIDE:
			{
				const SpeexMode *smode;
				if (!S_Speex_Init())
					return;	//speex not usable.
				if (codec == VOIP_SPEEX_NARROW)
				{
					s_voip.decsamplerate[sender] = 8000;
					s_voip.decframesize[sender] = 160;
					smode = s_voip.speex.modenb;
				}
				else if (codec == VOIP_SPEEX_WIDE)
				{
					s_voip.decsamplerate[sender] = 16000;
					s_voip.decframesize[sender] = 320;
					smode = s_voip.speex.modewb;
				}
				else if (codec == VOIP_SPEEX_ULTRAWIDE)
				{
					s_voip.decsamplerate[sender] = 32000;
					s_voip.decframesize[sender] = 640;
					smode = s_voip.speex.modeuwb;
				}
				else
				{
					s_voip.decsamplerate[sender] = 11025;
					s_voip.decframesize[sender] = 160;
					smode = s_voip.speex.modenb;
				}
				if (!s_voip.decoder[sender])
				{
					qspeex_bits_init(&s_voip.speex.decbits[sender]);
					qspeex_bits_reset(&s_voip.speex.decbits[sender]);
					s_voip.decoder[sender] = qspeex_decoder_init(smode);
					if (!s_voip.decoder[sender])
						return;
				}
				else
					qspeex_bits_reset(&s_voip.speex.decbits[sender]);
			}
			break;
		case VOIP_OPUS:
			if (!S_Opus_Init())
				return;

			//the lazy way to reset the codec!
			if (!s_voip.decoder[sender])
			{
				//opus outputs to 8, 12, 16, 24, or 48khz. pick whichever has least excess samples and resample to fit it.
				if (snd_speed <= 8000)
					s_voip.decsamplerate[sender] = 8000;
				else if (snd_speed <= 12000)
					s_voip.decsamplerate[sender] = 12000;
				else if (snd_speed <= 16000)
					s_voip.decsamplerate[sender] = 16000;
				else if (snd_speed <= 24000)
					s_voip.decsamplerate[sender] = 24000;
				else
					s_voip.decsamplerate[sender] = 48000;
				s_voip.decoder[sender] = qopus_decoder_create(s_voip.decsamplerate[sender], 1/*FIXME: support stereo where possible*/, NULL);
				if (!s_voip.decoder[sender])
					return;

				s_voip.decframesize[sender] = (sizeof(decodebuf) / sizeof(decodebuf[0])) / 2;	//this is the maximum size in a single frame.
			}
			else
				qopus_decoder_ctl(s_voip.decoder[sender], OPUS_RESET_STATE);
			break;
		}
		s_voip.deccodec[sender] = codec;
		s_voip.decgen[sender] = gen;
		s_voip.decseq[sender] = seq;
	}


	//if there's packetloss, tell the decoder about the missing parts.
	//no infinite loops please.
	if ((unsigned)(seq - s_voip.decseq[sender]) > 10)
		s_voip.decseq[sender] = seq - 10;
	while(s_voip.decseq[sender] != seq)
	{
		if (decodesamps + s_voip.decframesize[sender] > sizeof(decodebuf)/sizeof(decodebuf[0]))
		{
			S_RawAudio(sender, (qbyte*)decodebuf, s_voip.decsamplerate[sender], decodesamps, 1, 2, snd_voip_play.value);
			decodesamps = 0;
		}
		switch(codec)
		{
		case VOIP_SPEEX_OLD:
		case VOIP_SPEEX_NARROW:
		case VOIP_SPEEX_WIDE:
		case VOIP_SPEEX_ULTRAWIDE:
			qspeex_decode_int(s_voip.decoder[sender], NULL, decodebuf + decodesamps);
			decodesamps += s_voip.decframesize[sender];
			break;
		case VOIP_OPUS:
			r = qopus_decode(s_voip.decoder[sender], NULL, 0, decodebuf + decodesamps, min(s_voip.decframesize[sender], sizeof(decodebuf)/sizeof(decodebuf[0]) - decodesamps), false);
			if (r > 0)
				decodesamps += r;
			break;
		}
		s_voip.decseq[sender]++;
	}

	while (bytes > 0)
	{
		if (decodesamps + s_voip.decframesize[sender] >= sizeof(decodebuf)/sizeof(decodebuf[0]))
		{
			S_RawAudio(sender, (qbyte*)decodebuf, s_voip.decsamplerate[sender], decodesamps, 1, 2, snd_voip_play.value);
			decodesamps = 0;
		}
		switch(codec)
		{
		default:
			bytes = 0;
			break;
		case VOIP_SPEEX_OLD:
		case VOIP_SPEEX_NARROW:
		case VOIP_SPEEX_WIDE:
		case VOIP_SPEEX_ULTRAWIDE:
			if (codec == VOIP_SPEEX_OLD)
			{	//older versions support only this, and require this extra bit.
				bytes--;
				len = *start++;
				if (bytes < len)
					break;
			}
			else
				len = bytes;
			qspeex_bits_read_from(&s_voip.speex.decbits[sender], start, len);
			bytes -= len;
			start += len;
			while (qspeex_decode_int(s_voip.decoder[sender], &s_voip.speex.decbits[sender], decodebuf + decodesamps) == 0)
			{
				decodesamps += s_voip.decframesize[sender];
				s_voip.decseq[sender]++;
				seq++;
				if (decodesamps + s_voip.decframesize[sender] >= sizeof(decodebuf)/sizeof(decodebuf[0]))
				{
					S_RawAudio(sender, (qbyte*)decodebuf, s_voip.decsamplerate[sender], decodesamps, 1, 2, snd_voip_play.value);
					decodesamps = 0;
				}
			}
			break;
		case VOIP_RAW:
			len = min(bytes, sizeof(decodebuf)-(sizeof(decodebuf[0])*decodesamps));
			memcpy(decodebuf+decodesamps, start, len);
			decodesamps += len / sizeof(decodebuf[0]);
			s_voip.decseq[sender]++;
			bytes -= len;
			start += len;
			break;
		case VOIP_OPUS:
			len = bytes;
			if (decodesamps > 0)
			{
				S_RawAudio(sender, (qbyte*)decodebuf, s_voip.decsamplerate[sender], decodesamps, 1, 2, snd_voip_play.value);
				decodesamps = 0;
			}
			r = qopus_decode(s_voip.decoder[sender], start, len, decodebuf + decodesamps, sizeof(decodebuf)/sizeof(decodebuf[0]) - decodesamps, false);
//			Con_Printf("Decoded %i frames from %i bytes\n", r, len);
			if (r > 0)
			{
				decodesamps += r;
				s_voip.decseq[sender] += 1;//r / s_voip.decframesize[sender];
				seq += 1;//r / s_voip.decframesize[sender];
			}
			else if (r < 0)
				Con_Printf("Opus decoding error %i\n", r);

			bytes -= len;
			start += len;
			break;
		}
	}

	if (drops)
		Con_DPrintf("%i dropped audio frames\n", drops);

	if (decodesamps > 0)
		S_RawAudio(sender, (qbyte*)decodebuf, s_voip.decsamplerate[sender], decodesamps, 1, 2, snd_voip_play.value);
}

#ifdef SUPPORT_ICE
qboolean S_Voip_RTP_CodecOkay(char *codec)
{
	if (!strcmp(codec, "speex@8000") || !strcmp(codec, "speex@11025") || !strcmp(codec, "speex@16000") || !strcmp(codec, "speex@32000"))
	{
		if (S_Speex_Init())
			return true;
	}
	else if (!strcmp(codec, "opus"))
	{
		if (S_Opus_Init())
			return true;
	}
	return false;
}
void S_Voip_RTP_Parse(unsigned short sequence, char *codec, unsigned char *data, unsigned int datalen)
{
	if (!strcmp(codec, "speex@8000"))
		S_Voip_Decode(MAX_CLIENTS-1, VOIP_SPEEX_NARROW, 0, sequence, datalen, data);
	if (!strcmp(codec, "speex@11025"))
		S_Voip_Decode(MAX_CLIENTS-1, VOIP_SPEEX_OLD, 0, sequence, datalen, data);	//very much non-standard rtp
	if (!strcmp(codec, "speex@16000"))
		S_Voip_Decode(MAX_CLIENTS-1, VOIP_SPEEX_WIDE, 0, sequence&0xff, datalen, data);
	if (!strcmp(codec, "speex@32000"))
		S_Voip_Decode(MAX_CLIENTS-1, VOIP_SPEEX_ULTRAWIDE, 0, sequence, datalen, data);
	if (!strcmp(codec, "opus"))
		S_Voip_Decode(MAX_CLIENTS-1, VOIP_OPUS, 0, sequence, datalen, data);
}
qboolean NET_RTP_Transmit(unsigned int sequence, unsigned int timestamp, const char *codec, char *cdata, int clength);
qboolean NET_RTP_Active(void);
#else
#define NET_RTP_Active() false
#endif

void S_Voip_Parse(void)
{
	unsigned int sender;
	unsigned int bytes;
	unsigned char data[1024];
	unsigned char seq, gen;
	unsigned char codec;

	sender = MSG_ReadByte();
	gen = MSG_ReadByte();
	codec = gen>>4;
	gen &= 0x0f;
	seq = MSG_ReadByte();
	bytes = MSG_ReadShort();
	if (bytes > sizeof(data) || snd_voip_play.value <= 0)
	{
		MSG_ReadSkip(bytes);
		return;
	}
	MSG_ReadData(data, bytes);

	sender %= MAX_CLIENTS;

	//if testing, don't get confused if the server is echoing voice too!
	if (snd_voip_test.ival)
		if (sender == cl.playerview[0].playernum)
			return;

	S_Voip_Decode(sender, codec, gen, seq, bytes, data);
}
static float S_Voip_Preprocess(short *start, unsigned int samples, float micamp)
{
	int i;
	float level = 0, f;
	int framesize = s_voip.encframesize;
	while(samples >= framesize)
	{
		if (s_voip.speexdsp.preproc)
			qspeex_preprocess_run(s_voip.speexdsp.preproc, start);
		for (i = 0; i < framesize; i++)
		{
			f = start[i] * micamp;
			start[i] = bound(-32768, f, 32767);	//clamp it carefully, so it doesn't go to crap when given far too high a mic amp
			level += f*f;
		}

		start += framesize;
		samples -= framesize;
	}
	return level;
}
static void S_Voip_TryInitCaptureContext(char *driver, char *device, int rate)
{
	int i;

	s_voip.cdriver = NULL;

	/*Add new drivers in order of priority*/
	for (i = 0; capturedrivers[i]; i++)
	{
		if (capturedrivers[i]->Init && (!driver || !strcmp(capturedrivers[i]->drivername, driver)))
		{
			s_voip.cdriver = capturedrivers[i];

			s_voip.cdriverctx = s_voip.cdriver->Init(s_voip.encsamplerate, device);
			if (s_voip.cdriverctx)
			{
				//success!
				return;
			}
		}
	}

	if (!s_voip.cdriver)
	{
		if (!driver)
			Con_Printf("No microphone drivers supported\n");
		else
			Con_Printf("Microphone driver \"%s\" is not valid\n", driver);
	}
	else
		Con_Printf("No microphone detected\n");
	s_voip.cdriver = NULL;
}

static void S_Voip_InitCaptureContext(int rate)
{
	char *s;

	s_voip.cdriver = NULL;
	s_voip.cdriverctx = NULL;

	for (s = snd_voip_capturedevice.string; ; )
	{
		char *sep;
		s = COM_Parse(s);
		if (!*com_token)
			break;

		sep = strchr(com_token, ':');
		if (sep)
			*sep++ = 0;
		S_Voip_TryInitCaptureContext(com_token, sep, rate);
	}
	if (!s_voip.cdriver)
		S_Voip_TryInitCaptureContext(NULL, NULL, rate);
}

void S_Voip_Transmit(unsigned char clc, sizebuf_t *buf)
{
	unsigned char outbuf[8192];
	unsigned int outpos;//in bytes
	unsigned int encpos;//in bytes
	short *start;
	unsigned int initseq;//in frames
	unsigned int inittimestamp;//in samples
	unsigned int samps;
	float level;
	int len;
	float micamp = snd_voip_micamp.value;
	qboolean voipsendenable = true;
	int voipcodec = snd_voip_codec.ival;
	qboolean rtpstream = NET_RTP_Active();

	if (buf)
	{
		/*if you're sending sound, you should be prepared to accept others yelling at you to shut up*/
		if (snd_voip_play.value <= 0)
			voipsendenable = false;
		/*don't send sound if its not supported. that'll break stuff*/
		if (!(cls.fteprotocolextensions2 & PEXT2_VOICECHAT))
			voipsendenable = false;
	}
	else
	{
		/*we're not sending it to a server. the above considerations don't matter*/
		voipsendenable = snd_voip_test.ival;
	}
	/*don't send sound if mic volume won't send anything anyway*/
	if (micamp <= 0)
		voipsendenable = false;

	if (rtpstream)
	{
		voipsendenable = true;
		//if rtp streaming is enabled, hack the codec to something better supported
		if (voipcodec == VOIP_SPEEX_OLD)
			voipcodec = VOIP_SPEEX_WIDE;
	}


	voicevolumemod = s_voip.lastspoke_any > realtime?snd_voip_ducking.value:1;

	if (!voipsendenable || (voipcodec != s_voip.enccodec && s_voip.cdriver))
	{
		if (s_voip.cdriver)
		{
			if (s_voip.cdriverctx)
			{
				if (s_voip.wantsend)
				{
					s_voip.cdriver->Stop(s_voip.cdriverctx);
					s_voip.wantsend = false;
				}
				s_voip.cdriver->Shutdown(s_voip.cdriverctx);
				s_voip.cdriverctx = NULL;
			}
			s_voip.cdriver = NULL;
		}
		switch(s_voip.enccodec)
		{
		case VOIP_SPEEX_OLD:
		case VOIP_SPEEX_NARROW:
		case VOIP_SPEEX_WIDE:
		case VOIP_SPEEX_ULTRAWIDE:
			break;
		case VOIP_OPUS:
			qopus_encoder_destroy(s_voip.encoder);
			break;
		}
		s_voip.encoder = NULL;
		s_voip.enccodec = VOIP_INVALID;

		if (!voipsendenable)
			return;
	}

	voipsendenable = voipbutton || (snd_voip_send.ival>0);

	if (!s_voip.cdriver)
	{
		s_voip.voiplevel = -1;
		/*only init the first time capturing is requested*/
		if (!voipsendenable)
			return;

		/*see if we can init our encoding codec...*/
		switch(voipcodec)
		{
		case VOIP_SPEEX_OLD:
		case VOIP_SPEEX_NARROW:
		case VOIP_SPEEX_WIDE:
		case VOIP_SPEEX_ULTRAWIDE:
			{
				const SpeexMode *smode;
				if (!S_Speex_Init())
				{
					Con_Printf("Unable to use speex codec - not installed\n");
					return;
				}

				if (voipcodec == VOIP_SPEEX_ULTRAWIDE)
					smode = s_voip.speex.modeuwb;
				else if (voipcodec == VOIP_SPEEX_WIDE)
					smode = s_voip.speex.modewb;
				else
					smode = s_voip.speex.modenb;
				qspeex_bits_init(&s_voip.speex.encbits);
				qspeex_bits_reset(&s_voip.speex.encbits);
				s_voip.encoder = qspeex_encoder_init(smode);
				if (!s_voip.encoder)
					return;
				qspeex_encoder_ctl(s_voip.encoder, SPEEX_GET_FRAME_SIZE, &s_voip.encframesize);
				qspeex_encoder_ctl(s_voip.encoder, SPEEX_GET_SAMPLING_RATE, &s_voip.encsamplerate);
				if (voipcodec == VOIP_SPEEX_NARROW)
					s_voip.encsamplerate = 8000;
				else if (voipcodec == VOIP_SPEEX_WIDE)
					s_voip.encsamplerate = 16000;
				else if (voipcodec == VOIP_SPEEX_ULTRAWIDE)
					s_voip.encsamplerate = 32000;
				else
					s_voip.encsamplerate = 11025;
				qspeex_encoder_ctl(s_voip.encoder, SPEEX_SET_SAMPLING_RATE, &s_voip.encsamplerate);
			}
			break;
		case VOIP_RAW:
			s_voip.encsamplerate = 11025;
			s_voip.encframesize = 256;
			break;
		case VOIP_OPUS:
			if (!S_Opus_Init())
			{
				Con_Printf("Unable to use opus codec - not installed\n");
				return;
			}

			//use whatever is convienient.
			s_voip.encsamplerate = 48000;
			s_voip.encframesize = s_voip.encsamplerate / 400;	//2.5ms frames, at a minimum.
			s_voip.encframesize *= 4;	//go for 10ms
			s_voip.encoder = qopus_encoder_create(s_voip.encsamplerate, 1, OPUS_APPLICATION_VOIP, NULL);
			if (!s_voip.encoder)
				return;

//			opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate_bps));
//			opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
//			opus_encoder_ctl(enc, OPUS_SET_VBR(use_vbr));
//			opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(cvbr));
//			opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
//			opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(use_inbandfec));
//			opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(forcechannels));
//			opus_encoder_ctl(enc, OPUS_SET_DTX(use_dtx));
//			opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(packet_loss_perc));

//			opus_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&skip));
//			opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(16));


			break;
		default:
			Con_Printf("Unable to use that codec - not implemented\n");
			//can't start up other coedcs, cos we're too lame.
			return;
		}
		s_voip.enccodec = voipcodec;

		S_Voip_InitCaptureContext(s_voip.encsamplerate);	//sets cdriver+cdriverctx
	}

	/*couldn't init a driver?*/
	if (!s_voip.cdriverctx || !s_voip.cdriver)
	{
		return;
	}

	if (!voipsendenable && s_voip.wantsend)
	{
		s_voip.wantsend = false;
		s_voip.capturepos += s_voip.cdriver->Update(s_voip.cdriverctx, (unsigned char*)s_voip.capturebuf + s_voip.capturepos, 1, sizeof(s_voip.capturebuf) - s_voip.capturepos);
		s_voip.cdriver->Stop(s_voip.cdriverctx);
		/*note: we still grab audio to flush everything that was captured while it was active*/
	}
	else if (voipsendenable && !s_voip.wantsend)
	{
		s_voip.wantsend = true;
		if (!s_voip.capturepos)
		{	/*if we were actually still sending, it was probably only off for a single frame, in which case don't reset it*/
			s_voip.dumps = 0;
			s_voip.generation++;
			s_voip.encsequence = 0;

			//reset codecs so they start with a clean slate when new audio blocks are generated.
			switch(s_voip.enccodec)
			{
			case VOIP_SPEEX_OLD:
			case VOIP_SPEEX_NARROW:
			case VOIP_SPEEX_WIDE:
			case VOIP_SPEEX_ULTRAWIDE:
				qspeex_bits_reset(&s_voip.speex.encbits);
				break;
			case VOIP_RAW:
				break;
			case VOIP_OPUS:
				qopus_encoder_ctl(s_voip.encoder, OPUS_RESET_STATE);
				break;
			}
		}
		else
		{
			s_voip.capturepos += s_voip.cdriver->Update(s_voip.cdriverctx, (unsigned char*)s_voip.capturebuf + s_voip.capturepos, 1, sizeof(s_voip.capturebuf) - s_voip.capturepos);
		}
		s_voip.cdriver->Start(s_voip.cdriverctx);
	}

	if (s_voip.wantsend)
		voicevolumemod = min(voicevolumemod, snd_voip_capturingvol.value);

	s_voip.capturepos += s_voip.cdriver->Update(s_voip.cdriverctx, (unsigned char*)s_voip.capturebuf + s_voip.capturepos, s_voip.encframesize*2, sizeof(s_voip.capturebuf) - s_voip.capturepos);

	if (!s_voip.wantsend && s_voip.capturepos < s_voip.encframesize*2)
	{
		s_voip.voiplevel = -1;
		s_voip.capturepos = 0;
		return;
	}

	initseq = s_voip.encsequence;
	inittimestamp = s_voip.enctimestamp;
	level = 0;
	samps=0;
	//*2 for 16bit audio input.
	for (encpos = 0, outpos = 0; encpos+s_voip.encframesize*2 <= s_voip.capturepos && outpos+256 < sizeof(outbuf); )
	{
		start = (short*)(s_voip.capturebuf + encpos);

		if (snd_voip_noisefilter.ival || snd_voip_autogain.ival)
		{
			if (!s_voip.speexdsp.preproc || snd_voip_noisefilter.modified || snd_voip_noisefilter.modified || s_voip.speexdsp.curframesize != s_voip.encframesize || s_voip.speexdsp.cursamplerate != s_voip.encsamplerate)
			{
				if (s_voip.speexdsp.preproc)
					qspeex_preprocess_state_destroy(s_voip.speexdsp.preproc);
				s_voip.speexdsp.preproc = NULL;
				if (S_SpeexDSP_Init())
				{
					int i;
					s_voip.speexdsp.preproc = qspeex_preprocess_state_init(s_voip.encframesize, s_voip.encsamplerate);
					i = snd_voip_noisefilter.ival;
					qspeex_preprocess_ctl(s_voip.speexdsp.preproc, SPEEX_PREPROCESS_SET_DENOISE, &i);
					i = snd_voip_autogain.ival;
					qspeex_preprocess_ctl(s_voip.speexdsp.preproc, SPEEX_PREPROCESS_SET_AGC, &i);

					s_voip.speexdsp.curframesize = s_voip.encframesize;
					s_voip.speexdsp.cursamplerate = s_voip.encsamplerate;
				}
			}
		}
		else if (s_voip.speexdsp.preproc)
		{
			qspeex_preprocess_state_destroy(s_voip.speexdsp.preproc);
			s_voip.speexdsp.preproc = NULL;
		}

		switch(s_voip.enccodec)
		{
		case VOIP_SPEEX_OLD:
			level += S_Voip_Preprocess(start, s_voip.encframesize, micamp);
			qspeex_bits_reset(&s_voip.speex.encbits);
			qspeex_encode_int(s_voip.encoder, start, &s_voip.speex.encbits);
			len = qspeex_bits_write(&s_voip.speex.encbits, outbuf+(outpos+1), sizeof(outbuf) - (outpos+1));
			if (len < 0 || len > 255)
				len = 0;
			outbuf[outpos] = len;
			outpos += 1+len;
			s_voip.encsequence++;
			samps+=s_voip.encframesize;
			encpos += s_voip.encframesize*2;
			break;
		case VOIP_SPEEX_NARROW:
		case VOIP_SPEEX_WIDE:
		case VOIP_SPEEX_ULTRAWIDE:
			qspeex_bits_reset(&s_voip.speex.encbits);
			for (; encpos+s_voip.encframesize*2 <= s_voip.capturepos; )
			{
				start = (short*)(s_voip.capturebuf + encpos);
				level += S_Voip_Preprocess(start, s_voip.encframesize, micamp);
				qspeex_encode_int(s_voip.encoder, start, &s_voip.speex.encbits);
				s_voip.encsequence++;
				samps+=s_voip.encframesize;
				encpos += s_voip.encframesize*2;

				if (rtpstream)	//FIXME: why?
					break;
			}
			len = qspeex_bits_write(&s_voip.speex.encbits, outbuf+outpos, sizeof(outbuf) - outpos);
			outpos += len;
			break;
		case VOIP_RAW:
			len = s_voip.capturepos-encpos;	//amount of data to be eaten in this frame
			len = min(len, sizeof(outbuf)-outpos);
			len &= ~((s_voip.encframesize*2)-1);
			level += S_Voip_Preprocess(start, len/2, micamp);
			memcpy(outbuf+outpos, start, len);	//'encode'
			outpos += len;			//bytes written to output
			encpos += len;			//number of bytes consumed

			s_voip.encsequence++;	//increment number of packets, for packetloss detection.
			samps+=len / 2;	//number of samplepairs eaten in this packet. for stats.
			break;
		case VOIP_OPUS:
			{
				//opus rtp only supports/allows a single chunk in each packet.
				int frames;
				//densely pack the frames.
				start = (short*)(s_voip.capturebuf + encpos);
				frames = (s_voip.capturepos-encpos)/2;
				frames = s_voip.encframesize;
				if (frames >= 2880)
					frames = 2880;
				else if (frames >= 1920)
					frames = 1920;
				else if (frames >= 960)
					frames = 960;
				else if (frames >= 480)
					frames = 480;
				else if (frames >= 240)
					frames = 240;
				else if (frames >= 120)
					frames = 120;
				else
				{
					Con_Printf("invalid Opus frame size\n");
					frames = 0;
				}
//				Con_Printf("Encoding %i frames", frames);
				level += S_Voip_Preprocess(start, frames, micamp);
				len = qopus_encode(s_voip.encoder, start, frames, outbuf+outpos, sizeof(outbuf) - outpos);
//				Con_Printf(" (%i bytes)\n", len);
				if (len >= 0)
				{
					s_voip.encsequence += frames / s_voip.encframesize;
					outpos += len;
					samps+=frames;
					encpos += frames*2;
				}
				else
				{
					Con_Printf("Opus encoding error: %i\n", len);
					encpos = s_voip.capturepos;
				}
			}
			break;
		default:
			outbuf[outpos] = 0;
			break;
		}

		if (rtpstream || s_voip.enccodec == VOIP_OPUS)
			break;
	}
	if (samps)
	{
		float nl;
		s_voip.enctimestamp += samps;
		nl = (3000*level) / (32767.0f*32767*samps);
		s_voip.voiplevel = (s_voip.voiplevel*7 + nl)/8;
		if (s_voip.voiplevel < snd_voip_vad_threshhold.ival && !voipbutton && !(snd_voip_send.ival & 6))
		{
			/*try and dump it, it was too quiet, and they're not pressing +voip*/
			if (s_voip.keeps > samps)
			{
				/*but not instantly*/
				s_voip.keeps -= samps;
			}
			else
			{
				outpos = 0;
				s_voip.dumps += samps;
				s_voip.keeps = 0;
			}
		}
		else
			s_voip.keeps = s_voip.encsamplerate * snd_voip_vad_delay.value;
		if (outpos)
		{
			if (s_voip.dumps > s_voip.encsamplerate/4)
				s_voip.generation++;
			s_voip.dumps = 0;
		}
	}

	if (outpos && (!buf || buf->maxsize - buf->cursize >= outpos+4))
	{
		if (buf && (snd_voip_send.ival != 4))
		{
			MSG_WriteByte(buf, clc);
			MSG_WriteByte(buf, (s_voip.enccodec<<4) | (s_voip.generation & 0x0f)); /*gonna leave that nibble clear here... in this version, the client will ignore packets with those bits set. can use them for codec or something*/
			MSG_WriteByte(buf, initseq);
			MSG_WriteShort(buf, outpos);
			SZ_Write(buf, outbuf, outpos);
		}

#ifdef SUPPORT_ICE
		if (rtpstream)
		{
			switch(s_voip.enccodec)
			{
			case VOIP_SPEEX_NARROW:
			case VOIP_SPEEX_WIDE:
			case VOIP_SPEEX_ULTRAWIDE:
			case VOIP_SPEEX_OLD:
				NET_RTP_Transmit(initseq, inittimestamp, va("speex@%i", s_voip.encsamplerate), outbuf, outpos);
				break;
			case VOIP_OPUS:
				NET_RTP_Transmit(initseq, inittimestamp, "opus", outbuf, outpos);
				break;
			}
		}
#endif

		if (snd_voip_test.ival)
			S_Voip_Decode(cl.playerview[0].playernum, s_voip.enccodec, s_voip.generation & 0x0f, initseq, outpos, outbuf);

		//update our own lastspoke, so queries shows that we're speaking when we're speaking in a generic way, even if we can't hear ourselves.
		//but don't update general lastspoke, so ducking applies only when others speak. use capturingvol for yourself. they're more explicit that way.
		s_voip.lastspoke[cl.playerview[0].playernum] = realtime + 0.5;
	}

	/*remove sent data*/
	if (encpos)
	{
		memmove(s_voip.capturebuf, s_voip.capturebuf + encpos, s_voip.capturepos-encpos);
		s_voip.capturepos -= encpos;
	}
}
void S_Voip_Ignore(unsigned int slot, qboolean ignore)
{
	CL_SendClientCommand(true, "vignore %i %i", slot, ignore);
}
static void S_Voip_Enable_f(void)
{
	voipbutton = true;
}
static void S_Voip_Disable_f(void)
{
	voipbutton = false;
}
static void S_Voip_f(void)
{
	int i;
	if (!strcmp(Cmd_Argv(1), "maxgain"))
	{
		i = atoi(Cmd_Argv(2));
		if (s_voip.speexdsp.preproc)
			qspeex_preprocess_ctl(s_voip.speexdsp.preproc, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &i);
	}
}
static void S_Voip_Play_Callback(cvar_t *var, char *oldval)
{
	if (cls.fteprotocolextensions2 & PEXT2_VOICECHAT)
	{
		if (var->value > 0)
			CL_SendClientCommand(true, "unmuteall");
		else
			CL_SendClientCommand(true, "muteall");
	}
}
void S_Voip_MapChange(void)
{
	Cvar_ForceCallback(&snd_voip_play);
}
int S_Voip_Loudness(qboolean ignorevad)
{
	if (s_voip.voiplevel > 100)
		return 100;
	if (!s_voip.cdriverctx || (!ignorevad && s_voip.dumps))
		return -1;
	return s_voip.voiplevel;
}
qboolean S_Voip_Speaking(unsigned int plno)
{
	if (plno >= MAX_CLIENTS)
		return false;
	return s_voip.lastspoke[plno] > realtime;
}

void QDECL S_Voip_EnumeratedCaptureDevice(const char *driver, const char *devicecode, const char *readabledevice)
{
	const char *fullintname;
	char opts[8192];
	char nbuf[1024];
	char dbuf[1024];
	
	if (devicecode && (	strchr(devicecode, ' ') ||
						strchr(devicecode, '\"')))
		fullintname = va("\"%s:%s\"", driver, devicecode);	//it'll all get escaped anyway. but yeah, needs to be a single token or our multi-device stuff won't work properly. yes, this is a bit of a hack.
	else if (devicecode)
		fullintname = va("%s:%s", driver, devicecode);
	else
		fullintname = driver;

	Q_snprintfz(opts, sizeof(opts), "%s%s%s %s", snd_voip_capturedevice_opts.string, *snd_voip_capturedevice_opts.string?" ":"", COM_QuotedString(fullintname, nbuf, sizeof(nbuf), false), COM_QuotedString(readabledevice, dbuf, sizeof(dbuf), false));
	Cvar_ForceSet(&snd_voip_capturedevice_opts, opts);
}
void S_Voip_Init(void)
{
	int i;
	for (i = 0; i < MAX_CLIENTS; i++)
		s_voip.deccodec[i] = VOIP_INVALID;
	s_voip.enccodec = VOIP_INVALID;

	Cvar_Register(&snd_voip_capturedevice,		"Voice Chat");
	Cvar_Register(&snd_voip_capturedevice_opts,		"Voice Chat");
	Cvar_Register(&snd_voip_send,		"Voice Chat");
	Cvar_Register(&snd_voip_vad_threshhold,	"Voice Chat");
	Cvar_Register(&snd_voip_vad_delay,	"Voice Chat");
	Cvar_Register(&snd_voip_capturingvol,	"Voice Chat");
	Cvar_Register(&snd_voip_showmeter,	"Voice Chat");
	Cvar_Register(&snd_voip_play,		"Voice Chat");
	Cvar_Register(&snd_voip_test,		"Voice Chat");
	Cvar_Register(&snd_voip_ducking,		"Voice Chat");
	Cvar_Register(&snd_voip_micamp,		"Voice Chat");
	Cvar_Register(&snd_voip_codec,		"Voice Chat");
	Cvar_Register(&snd_voip_noisefilter,		"Voice Chat");
	Cvar_Register(&snd_voip_autogain,		"Voice Chat");
	Cmd_AddCommand("+voip", S_Voip_Enable_f);
	Cmd_AddCommand("-voip", S_Voip_Disable_f);
	Cmd_AddCommand("voip", S_Voip_f);


	Cvar_ForceSet(&snd_voip_capturedevice_opts, "");
	S_Voip_EnumeratedCaptureDevice("", NULL, "Default");
	for (i = 0; capturedrivers[i]; i++)
	{
		if (!capturedrivers[i]->Init)
			continue;
		if (!capturedrivers[i]->Enumerate || !capturedrivers[i]->Enumerate(S_Voip_EnumeratedCaptureDevice))
			S_Voip_EnumeratedCaptureDevice(capturedrivers[i]->drivername, NULL, va("Default %s", capturedrivers[i]->drivername));
	}
}
#else
void S_Voip_Parse(void)
{
	unsigned int bytes;

	MSG_ReadByte();
	MSG_ReadByte();
	MSG_ReadByte();
	bytes = MSG_ReadShort();
	MSG_ReadSkip(bytes);
}
#endif



void S_DefaultSpeakerConfiguration(soundcardinfo_t *sc)
{
	sc->dist[0] = 1;
	sc->dist[1] = 1;
	sc->dist[2] = 1;
	sc->dist[3] = 1;
	sc->dist[4] = 1;
	sc->dist[5] = 1;

	switch (sc->sn.numchannels)
	{
	case 1:
		VectorSet(sc->speakerdir[0], 0, 0, 0);
		break;
	case 2:
	case 3:
		VectorSet(sc->speakerdir[0], 0, -1, 0);
		VectorSet(sc->speakerdir[1], 0, 1, 0);
		VectorSet(sc->speakerdir[2], 0, 0, 0);
		break;
	case 4: // quad
	case 5:
		VectorSet(sc->speakerdir[0], 0.7, -0.7, 0);
		VectorSet(sc->speakerdir[1], 0.7, 0.7, 0);
		VectorSet(sc->speakerdir[2], -0.7, -0.7, 0);
		VectorSet(sc->speakerdir[3], -0.7, 0.7, 0);
		VectorSet(sc->speakerdir[4], 0, 0, 0);
		break;
	case 6: // 5.1
	case 7:
		VectorSet(sc->speakerdir[0], 0.7, -0.7, 0);
		VectorSet(sc->speakerdir[1], 0.7, 0.7, 0);
		VectorSet(sc->speakerdir[2], 1, 0, 0);
		VectorSet(sc->speakerdir[3], 0, 0, 0);
		VectorSet(sc->speakerdir[4], -0.7, -0.7, 0);
		VectorSet(sc->speakerdir[5], -0.7, 0.7, 0);
		VectorSet(sc->speakerdir[6], 0, 0, 0);
		break;
	case 8: // 7.1
	default:
		VectorSet(sc->speakerdir[0], 0.7, -0.7, 0);
		VectorSet(sc->speakerdir[1], 0.7, 0.7, 0);
		VectorSet(sc->speakerdir[2], 1, 0, 0);
		VectorSet(sc->speakerdir[3], 0, 0, 0);
		VectorSet(sc->speakerdir[4], -0.7, -0.7, 0);
		VectorSet(sc->speakerdir[5], -0.7, 0.7, 0);
		VectorSet(sc->speakerdir[6], 0, -1, 0);
		VectorSet(sc->speakerdir[7], 0, 1, 0);
		break;
	}
}


sounddriver_t DSOUND_Output;
#ifdef AVAIL_OPENAL
extern sounddriver_t OPENAL_Output;
#endif

sounddriver pALSA_InitCard;
sounddriver pSNDIO_InitCard;
sounddriver pOSS_InitCard;
sounddriver pMacOS_InitCard;
sounddriver pSDL_InitCard;
sounddriver pWAV_InitCard;
sounddriver pDroid_InitCard;
sounddriver pAHI_InitCard;
#ifdef NACL
extern sounddriver pPPAPI_InitCard;
#endif

//in order of preference
sounddriver_t *outputdrivers[] =
{
#ifdef AVAIL_OPENAL
	&OPENAL_Output,
#endif
	&DSOUND_Output,
	NULL
};
typedef struct {
	char *name;
	sounddriver *ptr;
} sdriver_t;
sdriver_t olddrivers[] = {
//in order of preference
	{"MacOS", &pMacOS_InitCard},	//prefered on mac
	{"Droid", &pDroid_InitCard},	//prefered on android (java thread)
	{"AHI", &pAHI_InitCard},		//prefered on morphos
#ifdef NACL
	{"PPAPI", &pPPAPI_InitCard},	//google's native client
#endif
	{"SNDIO", &pSNDIO_InitCard},	//prefered on OpenBSD

	{"SDL", &pSDL_InitCard},		//prefered on linux
	{"ALSA", &pALSA_InitCard},		//pure shite
	{"OSS", &pOSS_InitCard},		//good, but not likely to work any more

	{"WaveOut", &pWAV_InitCard},	//doesn't work properly in vista, etc.
	{NULL, NULL}
};

static soundcardinfo_t *SNDDMA_Init(char *driver, char *device)
{
	soundcardinfo_t *sc = Z_Malloc(sizeof(soundcardinfo_t));
	sdriver_t *od;
	sounddriver_t *sd;
	int i;
	int st;

	memset(sc, 0, sizeof(*sc));

	// set requested rate
	if (snd_khz.ival >= 1000)
		sc->sn.speed = snd_khz.ival;
	else if (snd_khz.ival <= 0)
		sc->sn.speed = 22050;
/*	else if (snd_khz.ival >= 195)
		sc->sn.speed = 200000;
	else if (snd_khz.ival >= 180)
		sc->sn.speed = 192000;
	else if (snd_khz.ival >= 90)
		sc->sn.speed = 96000; */
	else if (snd_khz.ival >= 45)
		sc->sn.speed = 48000;
	else if (snd_khz.ival >= 30)
		sc->sn.speed = 44100;
	else if (snd_khz.ival >= 20)
		sc->sn.speed = 22050;
	else if (snd_khz.ival >= 10)
		sc->sn.speed = 11025;
	else
		sc->sn.speed = 8000;

	// set requested speaker count
	if (snd_speakers.ival > MAXSOUNDCHANNELS)
		sc->sn.numchannels = MAXSOUNDCHANNELS;
	else if (snd_speakers.ival > 1)
		sc->sn.numchannels = (int)snd_speakers.ival;
	else
		sc->sn.numchannels = 1;

	// set requested sample bits
	if (snd_samplebits.ival >= 16)
		sc->sn.samplebits = 16;
	else
		sc->sn.samplebits = 8;

	// set requested buffer size
	if (snd_buffersize.ival > 0)
		sc->sn.samples = snd_buffersize.ival * sc->sn.numchannels;
	else
		sc->sn.samples = 0;

	for (i = 0; outputdrivers[i]; i++)
	{
		sd = outputdrivers[i];
		if (sd && (!driver || !Q_strcasecmp(sd->name, driver)))
		{
			//skip drivers which are not present.
			if (!sd->InitCard)
				continue;

			st = (**sd->InitCard)(sc, device);
			if (st)
			{
				S_DefaultSpeakerConfiguration(sc);
				if (sndcardinfo)
				{	//if the sample speeds of multiple soundcards do not match, it'll fail.
					if (snd_speed != sc->sn.speed)
					{
						Con_TPrintf("S_Startup: Ignoring soundcard %s due to mismatched sample speeds.\n", sc->name);
						S_ShutdownCard(sc);
						continue;
					}
				}
				else
					snd_speed = sc->sn.speed;

				sc->next = sndcardinfo;
				sndcardinfo = sc;
				return sc;
			}
		}
	}

	for (i = 0; olddrivers[i].name; i++)
	{
		od = &olddrivers[i];
		if (!driver || !Q_strcasecmp(od->name, driver))
		{
			//skip drivers which are not present.
			if (!*od->ptr)
				continue;

			st = (**od->ptr)(sc, device?atoi(device):0);
			if (st == 1)
			{
				S_DefaultSpeakerConfiguration(sc);
				if (sndcardinfo)
				{	//if the sample speeds of multiple soundcards do not match, it'll fail.
					if (snd_speed != sc->sn.speed)
					{
						Con_TPrintf("S_Startup: Ignoring soundcard %s due to mismatched sample speeds.\nTry running Quake with -singlesound to use just the primary soundcard\n", sc->name);
						S_ShutdownCard(sc);
						continue;
					}
				}
				else
					snd_speed = sc->sn.speed;

				sc->next = sndcardinfo;
				sndcardinfo = sc;
				return sc;
			}
		}
	}

	Z_Free(sc);
	if (!driver)
		Con_TPrintf("Could not start audio device \"%s\"\n", device?device:"default");
	else
		Con_TPrintf("Could not start \"%s\" device \"%s\"\n", driver, device?device:"default");
	return NULL;
}

void QDECL S_EnumeratedOutDevice(const char *driver, const char *devicecode, const char *readabledevice)
{
	const char *fullintname;
	char opts[8192];
	char nbuf[1024];
	char dbuf[1024];
	
	if (devicecode && (	strchr(devicecode, ' ') ||
						strchr(devicecode, '\"')))
		fullintname = va("\"%s:%s\"", driver, devicecode);	//it'll all get escaped anyway. but yeah, needs to be a single token or our multi-device stuff won't work properly. yes, this is a bit of a hack.
	else if (devicecode)
		fullintname = va("%s:%s", driver, devicecode);
	else
		fullintname = driver;

	Q_snprintfz(opts, sizeof(opts), "%s%s%s %s", snd_device_opts.string, *snd_device_opts.string?" ":"", COM_QuotedString(fullintname, nbuf, sizeof(nbuf), false), COM_QuotedString(readabledevice, dbuf, sizeof(dbuf), false));
	Cvar_ForceSet(&snd_device_opts, opts);
}
void S_EnumerateDevices(void)
{
	int i;
	sounddriver_t *sd;
	Cvar_ForceSet(&snd_device_opts, "");
	S_EnumeratedOutDevice("", NULL, "Default");

	for (i = 0; outputdrivers[i]; i++)
	{
		sd = outputdrivers[i];
		if (sd && sd->name)
		{
			if (!sd->Enumerate || !sd->Enumerate(S_EnumeratedOutDevice))
				S_EnumeratedOutDevice(sd->name, "", va("Default %s", sd->name));
		}
	}
}

/*
================
S_Startup
================
*/

void S_ClearRaw(void);
void S_Startup (void)
{
	char *s;

	if (!snd_initialized)
		return;

	if (sound_started)
		S_Shutdown(false);

	snd_blocked = 0;
	snd_speed = 0;

	for (s = snd_device.string; ; )
	{
		char *sep;
		s = COM_Parse(s);
		if (!*com_token)
			break;

		sep = strchr(com_token, ':');
		if (sep)
			*sep++ = 0;
		SNDDMA_Init(com_token, sep);
	}
	if (!sndcardinfo)
		SNDDMA_Init(NULL, NULL);

	sound_started = true;

	S_ClearRaw();

	if (!known_sfx)
		known_sfx = Z_Malloc(MAX_SFX*sizeof(sfx_t));
	num_sfx = 0;

	CL_InitTEntSounds();

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");
}

void S_SetUnderWater(qboolean underwater)
{
	soundcardinfo_t *sc;

	for (sc = sndcardinfo; sc; sc=sc->next)
		if (sc->SetWaterDistortion)
			sc->SetWaterDistortion(sc, underwater);
}

//why isn't this part of S_Restart_f anymore?
//so that the video code can call it directly without flushing the models it's just loaded.
void S_DoRestart (void)
{
	int i;

	S_StopAllSounds (true);
	S_Shutdown(false);

	if (nosound.ival)
		return;

	S_Startup();

	S_StopAllSounds (true);


	for (i=1 ; i<MAX_PRECACHE_SOUNDS ; i++)
	{
		if (!cl.sound_name[i][0])
			break;
		cl.sound_precache[i] = S_FindName (cl.sound_name[i], true);
	}
}

void S_Restart_f (void)
{
	S_DoRestart();
}

void S_Control_f (void)
{
	int i;
	char *command;

	command = Cmd_Argv (1);

	if (!Q_strcasecmp(command, "off"))
	{
		Cache_Flush();//forget the old sounds.

		S_StopAllSounds (true);

		S_Shutdown(false);
		sound_started = 0;
	}

	if (!Q_strcasecmp(command, "rate") || !Q_strcasecmp(command, "speed"))
	{
		Cvar_SetValue(&snd_khz, atof(Cmd_Argv (2))/1000);
		S_Restart_f();
		return;
	}

	//individual device control
	if (!Q_strncasecmp(command, "card", 4))
	{
		int card;
		soundcardinfo_t *sc;
		card = atoi(command+4);

		for (i = 0, sc = sndcardinfo; i < card && sc; i++,sc=sc->next)
			;

		if (!sc)
		{
			Con_Printf("Sound card %i is invalid (try resetting first)\n", card);
			return;
		}
		if (Cmd_Argc() < 3)
		{
			Con_Printf("Scard %i is %s\n", card, sc->name);
			return;
		}
		command = Cmd_Argv (2);
		if (!Q_strcasecmp(command, "mono"))
		{
			for (i = 0; i < MAXSOUNDCHANNELS; i++)
			{
				VectorSet(sc->speakerdir[i], 0, 0, 0);
				sc->dist[i] = 1;
			}
		}
		else if (!Q_strcasecmp(command, "standard") || !Q_strcasecmp(command, "stereo"))
		{
			for (i = 0; i < MAXSOUNDCHANNELS; i++)
			{
				VectorSet(sc->speakerdir[i], 0, (i&1)?1:-1, 0);
				sc->dist[i] = 1;
			}
		}
		else if (!Q_strcasecmp(command, "swap"))
		{
			for (i = 0; i < MAXSOUNDCHANNELS; i++)
			{
				sc->speakerdir[i][1] *= -1;
			}
		}
		else if (!Q_strcasecmp(command, "front"))
		{
			for (i = 0; i < MAXSOUNDCHANNELS; i++)
			{
				VectorSet(sc->speakerdir[i], 0.7, (i&1)?-0.7:0.7, 0);
				sc->dist[i] = 1;
			}
		}
		else if (!Q_strcasecmp(command, "back"))
		{
			for (i = 0; i < MAXSOUNDCHANNELS; i++)
			{
				VectorSet(sc->speakerdir[i], -0.7, (i&1)?-0.7:0.7, 0);
				sc->dist[i] = 1;
			}
		}
		return;
	}
	else
		Con_Printf("valid commands are: off, single, multi, cardX mono, cardX stereo, cardX front, cardX back\n");
}

/*
================
S_Init
================
*/
void S_Init (void)
{
	int p;

	Con_DPrintf("\nSound Initialization\n");

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("play2", S_Play);
	Cmd_AddCommand("playvol", S_PlayVol);
	Cmd_AddCommand("stopsound", S_StopAllSounds_f);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	Cmd_AddCommand("snd_restart", S_Restart_f);

	Cmd_AddCommand("soundcontrol", S_Control_f);

	Cvar_Register(&nosound,				"Sound controls");
	Cvar_Register(&volume,				"Sound controls");
	Cvar_Register(&precache,			"Sound controls");
	Cvar_Register(&loadas8bit,			"Sound controls");
	Cvar_Register(&bgmvolume,			"Sound controls");
	Cvar_Register(&ambient_level,		"Sound controls");
	Cvar_Register(&ambient_fade,		"Sound controls");
	Cvar_Register(&snd_noextraupdate,	"Sound controls");
	Cvar_Register(&snd_show,			"Sound controls");
	Cvar_Register(&_snd_mixahead,		"Sound controls");
	Cvar_Register(&snd_khz,				"Sound controls");
	Cvar_Register(&snd_leftisright,		"Sound controls");
	Cvar_Register(&snd_eax,				"Sound controls");
	Cvar_Register(&snd_speakers,		"Sound controls");
	Cvar_Register(&snd_buffersize,		"Sound controls");
	Cvar_Register(&snd_samplebits,		"Sound controls");
	Cvar_Register(&snd_playbackrate,	"Sound controls");

	Cvar_Register(&snd_inactive,		"Sound controls");

#ifdef MULTITHREAD
	Cvar_Register(&snd_mixerthread,				"Sound controls");
#endif
	Cvar_Register(&snd_playersoundvolume,		"Sound controls");
	Cvar_Register(&snd_device,		"Sound controls");
	Cvar_Register(&snd_device_opts,		"Sound controls");

	Cvar_Register(&snd_linearresample, "Sound controls");
	Cvar_Register(&snd_linearresample_stream, "Sound controls");

#ifdef VOICECHAT
	S_Voip_Init();
#endif
	S_EnumerateDevices();

#ifdef MULTITHREAD
	mixermutex = Sys_CreateMutex();
#endif

#ifdef AVAIL_OPENAL
	OpenAL_CvarInit();
#endif

	if (COM_CheckParm("-nosound"))
	{
		Cvar_ForceSet(&nosound, "1");
		nosound.flags |= CVAR_NOSET;
		return;
	}

	p = COM_CheckParm ("-soundspeed");
	if (!p)
		p = COM_CheckParm ("-sspeed");
	if (!p)
		p = COM_CheckParm ("-sndspeed");
	if (p)
	{
		if (p < com_argc-1)
			Cvar_SetValue(&snd_khz, atof(com_argv[p+1]));
		else
			Sys_Error ("S_Init: you must specify a speed in KB after -soundspeed");
	}

	snd_initialized = true;

	known_sfx = Z_Malloc(MAX_SFX*sizeof(sfx_t));
	num_sfx = 0;
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_ShutdownCard(soundcardinfo_t *sc)
{
	soundcardinfo_t *prev;

	if (sndcardinfo == sc)
		sndcardinfo = sc->next;
	else
	{
		for (prev = sndcardinfo; prev->next; prev = prev->next)
		{
			if (prev->next == sc)
				prev->next = sc->next;
		}
	}

	sc->Shutdown(sc);
	Z_Free(sc);
}
void S_Shutdown(qboolean final)
{
	soundcardinfo_t *sc, *next;

	for (sc = sndcardinfo; sc; sc=next)
	{
		next = sc->next;
		sc->Shutdown(sc);
		Z_Free(sc);
		sndcardinfo = next;
	}

	sound_started = 0;
	S_Purge(false);

	Z_Free(known_sfx);
	known_sfx = NULL;
	num_sfx = 0;

#ifdef MULTITHREAD
	if (final && mixermutex)
	{
		Sys_DestroyMutex(mixermutex);
		mixermutex = NULL;
	}
#endif
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

also touches it
==================
*/
sfx_t *S_FindName (const char *name, qboolean create)
{
	int		i;
	sfx_t	*sfx;

	if (!name)
		Sys_Error ("S_FindName: NULL\n");

	if (Q_strlen(name) >= MAX_OSPATH)
		Sys_Error ("Sound name too long: %s", name);

// see if already loaded
	for (i=0 ; i < num_sfx ; i++)
		if (!Q_strcmp(known_sfx[i].name, name))
		{
			known_sfx[i].touched = true;
			return &known_sfx[i];
		}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	if (create)
	{
		sfx = &known_sfx[i];
		strcpy (sfx->name, name);
		known_sfx[i].touched = true;

		num_sfx++;
	}
	else
		sfx = NULL;

	return sfx;
}

void S_Purge(qboolean retaintouched)
{
	sfx_t	*sfx;
	int i;

	//make sure ambients are kept. silly ambients.
	if (retaintouched)
	{
		ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
		ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");
	}

	if (!num_sfx)
		return;

	S_LockMixer();
	for (i=0 ; i < num_sfx ; i++)
	{
		sfx = &known_sfx[i];
		/*don't hurt sounds if they're being processed by a worker thread*/
		if (sfx->loadstate == SLS_LOADING)
			continue;

		/*don't purge the file if its still relevent*/
		if (retaintouched && sfx->touched)
			continue;

		sfx->loadstate = SLS_NOTLOADED;
		/*nothing to do if there's no data within*/
		if (!sfx->decoder.buf)
			continue;
		/*stop the decoder first*/
		if (sfx->decoder.purge)
			sfx->decoder.purge(sfx);
		else if (sfx->decoder.ended)
			sfx->decoder.ended(sfx);

		/*if there's any data associated still, kill it. if present, it should be a single sfxcache_t (with data in same alloc)*/
		if (sfx->decoder.buf)
			BZ_Free(sfx->decoder.buf);
		memset(&sfx->decoder, 0, sizeof(sfx->decoder));
	}
	S_UnlockMixer();
}

void S_ResetFailedLoad(void)
{
	int i;
	for (i=0 ; i < num_sfx ; i++)
	{
		if (known_sfx[i].loadstate == SLS_FAILED)
			known_sfx[i].loadstate = SLS_NOTLOADED;
	}
}

void S_UntouchAll(void)
{
	int i;
	for (i=0 ; i < num_sfx ; i++)
		known_sfx[i].touched = false;
}

/*
==================
S_TouchSound

==================
*/
void S_TouchSound (char *name)
{
	if (!sound_started)
		return;

	S_FindName (name, true);
}

/*
==================
S_PrecacheSound

==================
*/
sfx_t *S_PrecacheSound (const char *name)
{
	sfx_t	*sfx;

	if (nosound.ival || !known_sfx || !*name)
		return NULL;

	sfx = S_FindName (name, true);

// cache it in
	if (precache.ival && sndcardinfo)
		S_LoadSound (sfx);

	return sfx;
}


//=============================================================================

/*
=================
SND_PickChannel
=================
*/
channel_t *SND_PickChannel(soundcardinfo_t *sc, int entnum, int entchannel)
{
    int ch_idx;
    int oldestpos;
    int oldest;

// Check for replacement sound, or find the best one to replace
    oldest = -1;
    oldestpos = -1;
    for (ch_idx=DYNAMIC_FIRST; ch_idx < DYNAMIC_STOP ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& sc->channel[ch_idx].entnum == entnum
		&& (sc->channel[ch_idx].entchannel == entchannel || entchannel == -1))
		{	// always override sound from same entity
			oldest = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (sc->channel[ch_idx].entnum == cl.playerview[0].playernum+1 && entnum != cl.playerview[0].playernum+1 && sc->channel[ch_idx].sfx)
			continue;

		if (!sc->channel[ch_idx].sfx)
		{
			oldestpos = 0x7fffffff;
			oldest = ch_idx;
		}
		else if (sc->channel[ch_idx].pos > oldestpos)
		{
			oldestpos = sc->channel[ch_idx].pos;
			oldest = ch_idx;
		}
	}

	if (oldest == -1)
		return NULL;

	if (sc->channel[oldest].sfx)
		sc->channel[oldest].sfx = NULL;

	if (sc->total_chans <= oldest)
		sc->total_chans = oldest+1;
    return &sc->channel[oldest];
}

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize(soundcardinfo_t *sc, channel_t *ch)
{
	vec3_t listener_vec;
    vec_t dist;
    vec_t scale;
    vec3_t world_vec;
	int i, v;

// anything coming from the view entity will always be full volume
	if (ch->flags & CF_ABSVOLUME)
	{
		v = ch->master_vol;
		for (i = 0; i < sc->sn.numchannels; i++)
		{
			ch->vol[i] = v;
		}
		return;
	}
	if (ch->entnum == -1 || ch->entnum == cl.playerview[0].playernum+1)
	{
		v = ch->master_vol * (ruleset_allow_localvolume.value ? snd_playersoundvolume.value : 1) * volume.value * voicevolumemod;
		v = bound(0, v, 255);
		for (i = 0; i < sc->sn.numchannels; i++)
		{
			ch->vol[i] = v;
		}
		return;
	}

// calculate stereo seperation and distance attenuation
	VectorSubtract(ch->origin, listener_origin, world_vec);

	dist = VectorNormalize(world_vec) * ch->dist_mult;

	//rotate the world_vec into listener space, so that the audio direction stored in the speakerdir array can be used directly.
	listener_vec[0] = DotProduct(listener_forward, world_vec);
	listener_vec[1] = DotProduct(listener_right, world_vec);
	listener_vec[2] = DotProduct(listener_up, world_vec);

	if (snd_leftisright.ival)
		listener_vec[1] = -listener_vec[1];

	for (i = 0; i < sc->sn.numchannels; i++)
	{
		scale = 1 + DotProduct(listener_vec, sc->speakerdir[i]);
		scale = (1.0 - dist) * scale * sc->dist[i];
		v = ch->master_vol * scale * volume.value * voicevolumemod;
		ch->vol[i] = bound(0, v, 255);
	}
}

// =======================================================================
// Start a sound effect
// =======================================================================

static void S_StartSoundCard(soundcardinfo_t *sc, int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation, int startpos, float pitchadj)
{
	channel_t *target_chan, *check;
	int		vol;
	int		ch_idx;
	int		skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (nosound.ival)
		return;

	if (pitchadj <= 0)
		pitchadj = 100;

	pitchadj *= snd_playbackrate.value * (cls.state?cl.gamespeed:1);

	vol = fvol*255;

// pick a channel to play on
	target_chan = SND_PickChannel(sc, entnum, entchannel);
	if (!target_chan)
		return;

// spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	if (!origin)
	{
		VectorCopy(listener_origin, target_chan->origin);
	}
	else
	{
		VectorCopy(origin, target_chan->origin);
	}
	target_chan->flags = 0;
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(sc, target_chan);

	if (!target_chan->vol[0] && !target_chan->vol[1] && !target_chan->vol[2] && !target_chan->vol[3] && !target_chan->vol[4] && !target_chan->vol[5] && sc->ChannelUpdate)
		return;		// not audible at all

// new channel
	if (!S_LoadSound (sfx))
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->rate = ((1<<PITCHSHIFT) * pitchadj) / 100;	/*pitchadj is a percentage*/
	if (target_chan->rate < 1)	/*make sure the rate won't crash us*/
		target_chan->rate = 1;
	target_chan->pos = startpos*target_chan->rate;
	target_chan->looping = false;

// if an identical sound has also been started this frame, offset the pos
// a bit to keep it from just making the first one louder
	check = &sc->channel[DYNAMIC_FIRST];
	for (ch_idx=DYNAMIC_FIRST; ch_idx < DYNAMIC_STOP; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = rand () % (int)(0.1*sc->sn.speed);
			target_chan->pos -= skip*target_chan->rate;
			break;
		}
	}

	if (sc->ChannelUpdate)
		sc->ChannelUpdate(sc, target_chan, true);
}

void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation, float timeofs, float pitchadj)
{
	soundcardinfo_t *sc;

	if (!sfx || !*sfx->name)	//no named sounds would need specific starting.
		return;

	if (cls.demoseeking)
		return;

	S_LockMixer();
	for (sc = sndcardinfo; sc; sc = sc->next)
		S_StartSoundCard(sc, entnum, entchannel, sfx, origin, fvol, attenuation, -(int)(timeofs * sc->sn.speed), pitchadj);
	S_UnlockMixer();
}

float S_GetSoundTime(int entnum, int entchannel)
{
	int i;
	float result = -1;	//if we didn't find one
	soundcardinfo_t *sc;
	S_LockMixer();
	for (sc = sndcardinfo; sc && result == -1; sc = sc->next)
	{
		for (i = 0; i < sc->total_chans; i++)
		{
			if (sc->channel[i].entnum == entnum && sc->channel[i].entchannel == entchannel && sc->channel[i].sfx)
			{
				result = (sc->channel[i].pos>>PITCHSHIFT) / (float)snd_speed;	//the time into the sound, ignoring play rate.
				break;
			}
		}
		//we found one on this sound device card, ignore others.
		if (result != -1)
			break;
	}
	S_UnlockMixer();
	return result;
}

qboolean S_IsPlayingSomewhere(sfx_t *s)
{
	soundcardinfo_t *si;
	int i;
	for (si = sndcardinfo; si; si=si->next)
	{
		for (i = 0; i < si->total_chans; i++)
		if (si->channel[i].sfx == s)
			return true;
	}
	return false;
}

static void S_StopSoundCard(soundcardinfo_t *sc, int entnum, int entchannel)
{
	int i;

	for (i=0 ; i<sc->total_chans ; i++)
	{
		if (sc->channel[i].entnum == entnum
			&& (!entchannel || sc->channel[i].entchannel == entchannel))
		{
			sc->channel[i].sfx = NULL;
			if (sc->ChannelUpdate)
				sc->ChannelUpdate(sc, &sc->channel[i], true);
			if (entchannel)
				break;
		}
	}
}

void S_StopSound(int entnum, int entchannel)
{
	soundcardinfo_t *sc;
	S_LockMixer();
	for (sc = sndcardinfo; sc; sc = sc->next)
		S_StopSoundCard(sc, entnum, entchannel);
	S_UnlockMixer();
}

void S_StopAllSounds(qboolean clear)
{
	int		i;
	sfx_t *s;

	soundcardinfo_t *sc;

	if (!sound_started)
		return;
	S_LockMixer();

	for (sc = sndcardinfo; sc; sc = sc->next)
	{
		for (i=0 ; i<sc->total_chans ; i++)
			if (sc->channel[i].sfx)
			{
				s = sc->channel[i].sfx;
				sc->channel[i].sfx = NULL;
				if (s->loadstate == SLS_LOADING)
					COM_WorkerPartialSync(s, &s->loadstate, SLS_LOADING);
				if (s->decoder.ended)
				if (!S_IsPlayingSomewhere(s))	//if we aint playing it elsewhere, free it compleatly.
				{
					s->decoder.ended(s);
				}
				if (sc->ChannelUpdate)
					sc->ChannelUpdate(sc, &sc->channel[i], true);
			}

		sc->total_chans = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS + NUM_MUSICS;	// no statics

		Q_memset(sc->channel, 0, MAX_CHANNELS * sizeof(channel_t));

		if (clear)
			S_ClearBuffer (sc);
	}

	S_UnlockMixer();
}

static void S_StopAllSounds_f (void)
{
	S_StopAllSounds (true);
}

static void S_ClearBuffer (soundcardinfo_t *sc)
{
	void *buffer;
	unsigned int dummy;

	int		clear;

	if (!sound_started || !sc->sn.buffer)
		return;

	if (sc->sn.samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	dummy = 0;
	buffer = sc->Lock(sc, &dummy);
	if (buffer)
	{
		Q_memset(buffer, clear, sc->sn.samples * sc->sn.samplebits/8);
		sc->Unlock(sc, buffer);
	}
}

/*
=================
S_StaticSound
=================
*/
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t	*ss;
	soundcardinfo_t *scard;

	if (!sfx)
		return;

	S_LockMixer();

	for (scard = sndcardinfo; scard; scard = scard->next)
	{
		if (scard->total_chans == MAX_CHANNELS)
		{
			Con_Printf ("total_channels == MAX_CHANNELS\n");
			continue;
		}

		if (!S_LoadSound (sfx))
			break;

		ss = &scard->channel[scard->total_chans];
		scard->total_chans++;

		ss->entnum = -2;
		ss->sfx = sfx;
		ss->rate = 1<<PITCHSHIFT;
		VectorCopy (origin, ss->origin);
		ss->master_vol = vol;
		ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
		ss->pos = 0;
		ss->looping = true;

		SND_Spatialize (scard, ss);

		if (scard->ChannelUpdate)
			scard->ChannelUpdate(scard, ss, true);
	}

	S_UnlockMixer();
}


//=============================================================================

void S_Music_Clear(sfx_t *onlyifsample)
{
	//stops the current BGM music
	//calling this will trigger Media_NextTrack later
	sfx_t *s;
	soundcardinfo_t *sc;
	int i;
	for (i = MUSIC_FIRST; i < MUSIC_STOP; i++)
	{
		for (sc = sndcardinfo; sc; sc=sc->next)
		{
			s = sc->channel[i].sfx;
			if (!s)
				continue;
			if (onlyifsample && s != onlyifsample)
				continue;

			sc->channel[i].pos = 0;
			sc->channel[i].sfx = NULL;

			if (s && s->decoder.ended && !S_IsPlayingSomewhere(s))	//if we aint playing it elsewhere, free it compleatly.
				s->decoder.ended(s);
		}
	}
}
void S_Music_Seek(float time)
{
	soundcardinfo_t *sc;
	int i;
	for (i = MUSIC_FIRST; i < MUSIC_STOP; i++)
	{
		for (sc = sndcardinfo; sc; sc=sc->next)
		{
			sc->channel[i].pos += sc->sn.speed*time * sc->channel[i].rate;

			if (sc->channel[i].pos < 0)
			{	//clamp to the start of the track
				sc->channel[i].pos=0;
			}
			//if we seek over the end, ignore it. The sound playing code will spot that.
		}
	}
}

/*
===================
S_UpdateAmbientSounds
===================
*/
char *Media_NextTrack(int musicchannelnum);
mleaf_t *Q1BSP_LeafForPoint (model_t *model, vec3_t p);
void S_UpdateAmbientSounds (soundcardinfo_t *sc)
{
	mleaf_t		*l;
	float		vol, oldvol;
	int			ambient_channel;
	channel_t	*chan;
	int i;

	if (!snd_ambient)
		return;

	for (i = MUSIC_FIRST; i < MUSIC_STOP; i++)
	{
		qboolean changed = false;
		chan = &sc->channel[i];
		if (!chan->sfx)
		{
			char *nexttrack = Media_NextTrack(i-MUSIC_FIRST);
			sfx_t *newmusic;

			if (nexttrack && *nexttrack)
			{
				newmusic = S_PrecacheSound(nexttrack);

				if (newmusic && newmusic->loadstate != SLS_FAILED)
				{
					chan->sfx = newmusic;
					chan->rate = 1<<PITCHSHIFT;
					chan->pos = 0;
					changed = true;
				}
			}
		}
		if (chan->sfx)
		{
			chan->flags = CF_ABSVOLUME;	//bypasses volume cvar completely.
			chan->master_vol = bound(0, 255*bgmvolume.value*voicevolumemod, 255);
			chan->vol[0] = chan->vol[1] = chan->vol[2] = chan->vol[3] = chan->vol[4] = chan->vol[5] = chan->master_vol;
			if (sc->ChannelUpdate)
				sc->ChannelUpdate(sc, chan, changed);
		}
	}


// calc ambient sound levels
	if (!cl.worldmodel || cl.worldmodel->type != mod_brush || cl.worldmodel->fromgame != fg_quake || cl.worldmodel->loadstate != MLS_LOADED)
		return;

	l = Q1BSP_LeafForPoint(cl.worldmodel, listener_origin);
	if (!l || ambient_level.value <= 0)
	{
		for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
		{
			chan = &sc->channel[AMBIENT_FIRST+ambient_channel];
			chan->sfx = NULL;
			if (sc->ChannelUpdate)
				sc->ChannelUpdate(sc, chan, true);
		}
		return;
	}

	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		static float level[NUM_AMBIENTS];
		chan = &sc->channel[AMBIENT_FIRST+ambient_channel];
		chan->sfx = ambient_sfx[AMBIENT_FIRST+ambient_channel];
		chan->entnum = -1;
		chan->looping = true;
		chan->rate = 1<<PITCHSHIFT;

		VectorCopy(listener_origin, chan->origin);

		vol = ambient_level.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		oldvol = level[ambient_channel];

	// don't adjust volume too fast
		if (level[ambient_channel] < vol)
		{
			level[ambient_channel] += host_frametime * ambient_fade.value;
			if (level[ambient_channel] > vol)
				level[ambient_channel] = vol;
		}
		else if (chan->master_vol > vol)
		{
			level[ambient_channel] -= host_frametime * ambient_fade.value;
			if (level[ambient_channel] < vol)
				level[ambient_channel] = vol;
		}

		chan->master_vol = level[ambient_channel];
		chan->vol[0] = chan->vol[1] = chan->vol[2] = chan->vol[3] = chan->vol[4] = chan->vol[5] = bound(0, chan->master_vol * (volume.value*voicevolumemod), 255);

		if (sc->ChannelUpdate)
			sc->ChannelUpdate(sc, chan, (oldvol == 0) ^ (level[ambient_channel] == 0));
	}
}

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_UpdateListener(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);
}

void S_GetListenerInfo(float *origin, float *forward, float *right, float *up)
{
	VectorCopy(listener_origin, origin);
	VectorCopy(listener_forward, forward);
	VectorCopy(listener_right, right);
	VectorCopy(listener_up, up);
}

static void S_UpdateCard(soundcardinfo_t *sc)
{
	int			i, j;
	int			total;
	channel_t	*ch;
	channel_t	*combine;

	if (!sound_started)
		return;
	if ((snd_blocked > 0))
	{
		if (!sc->inactive_sound)
			return;
	}

#ifdef AVAIL_OPENAL
	if (sc->ListenerUpdate)
	{
		sc->ListenerUpdate(sc, listener_origin, listener_forward, listener_right, listener_up, listener_velocity);
	}
#endif

// update general area ambient sound sources
	S_UpdateAmbientSounds (sc);

	combine = NULL;

// update spatialization for static and dynamic sounds
	ch = sc->channel+DYNAMIC_FIRST;
	for (i=DYNAMIC_FIRST ; i<sc->total_chans; i++, ch++)
	{
		if (!ch->sfx)
			continue;

		SND_Spatialize(sc, ch);         // respatialize channel
		if (!ch->vol[0] && !ch->vol[1] && !ch->vol[2] && !ch->vol[3] && !ch->vol[4] && !ch->vol[5])
			continue;

	// try to combine static sounds with a previous channel of the same
	// sound effect so we don't mix five torches every frame

		if (i >= DYNAMIC_STOP)
		{
		// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->vol[0] += ch->vol[0];
				combine->vol[1] += ch->vol[1];
				combine->vol[2] += ch->vol[2];
				combine->vol[3] += ch->vol[3];
				combine->vol[4] += ch->vol[4];
				combine->vol[5] += ch->vol[5];
				ch->vol[0] = ch->vol[1] = ch->vol[2] = ch->vol[3] = ch->vol[4] = ch->vol[5] = 0;
				continue;
			}
		// search for one
			combine = sc->channel+DYNAMIC_FIRST;
			for (j=DYNAMIC_FIRST ; j<i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;

			if (j == sc->total_chans)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->vol[0] += ch->vol[0];
					combine->vol[1] += ch->vol[1];
					combine->vol[2] += ch->vol[2];
					combine->vol[3] += ch->vol[3];
					combine->vol[4] += ch->vol[4];
					combine->vol[5] += ch->vol[5];
					ch->vol[0] = ch->vol[1] = ch->vol[2] = ch->vol[3] = ch->vol[4] = ch->vol[5] = 0;
				}
				continue;
			}
		}
	}

//
// debugging output
//
	if (snd_show.ival)
	{
		total = 0;
		ch = sc->channel;
		for (i=0 ; i<sc->total_chans; i++, ch++)
			if (ch->sfx && (ch->vol[0] || ch->vol[1]) )
			{
//					Con_Printf ("%i, %i %i %i %i %i %i %s\n", i, ch->vol[0], ch->vol[1], ch->vol[2], ch->vol[3], ch->vol[4], ch->vol[5], ch->sfx->name);
				total++;
			}

		Con_Printf ("----(%i)----\n", total);
	}

// mix some sound

	if (sc->selfpainting)
		return;

	if (snd_blocked > 0)
	{
		if (!sc->inactive_sound)
			return;
	}

	S_Update_(sc);
}

int S_GetMixerTime(soundcardinfo_t *sc)
{
	int		samplepos;
	int		fullsamples;

	fullsamples = sc->sn.samples / sc->sn.numchannels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
	samplepos = sc->GetDMAPos(sc);

	samplepos -= sc->samplequeue;

	if (samplepos < 0)
	{
		samplepos = 0;
	}
	if (samplepos < sc->oldsamplepos)
	{
		int bias;
		sc->buffers++;					// buffer wrapped

		if (sc->paintedtime > 0x40000000)
		{
			//when things get too large, we push everything back to prevent overflows
			bias = sc->paintedtime;
			bias -= bias % fullsamples;
			sc->paintedtime -= bias;
			sc->buffers -= bias / fullsamples;
		}
	}
	sc->oldsamplepos = samplepos;

	return sc->buffers*fullsamples + samplepos/sc->sn.numchannels;
}

void S_Update (void)
{
	soundcardinfo_t *sc;

	S_LockMixer();
	for (sc = sndcardinfo; sc; sc = sc->next)
		S_UpdateCard(sc);
	S_UnlockMixer();
}

void S_ExtraUpdate (void)
{
	soundcardinfo_t *sc;

	if (!sound_started)
		return;

#if defined(_WIN32) && !defined(WINRT)
	INS_Accumulate ();
#endif

	if (snd_noextraupdate.ival)
		return;		// don't pollute timings

	for (sc = sndcardinfo; sc; sc = sc->next)
	{
		if (sc->selfpainting)
			continue;

		if (snd_blocked > 0)
		{
			if (!sc->inactive_sound)
				continue;
		}

		S_LockMixer();
		S_Update_(sc);
		S_UnlockMixer();
	}
}



static void S_Update_(soundcardinfo_t *sc)
{
	int soundtime; /*in pairs*/
	unsigned        endtime;
	int				samps;

// Updates DMA time
	soundtime = S_GetMixerTime(sc);

	if (sc->samplequeue)
	{
		/*device uses a write-once queue*/
		endtime = soundtime + sc->samplequeue/sc->sn.numchannels;
		soundtime = sc->paintedtime;
		samps = sc->samplequeue / sc->sn.numchannels;
	}
	else
	{
		/*device uses memory-mapped output*/
		// check to make sure that we haven't overshot
		if (sc->paintedtime < soundtime)
		{
			//Con_Printf ("S_Update_ : overflow\n");
			sc->paintedtime = soundtime;
		}

		// mix ahead of current position
		endtime = soundtime + (int)(_snd_mixahead.value * sc->sn.speed);	
		samps = sc->sn.samples / sc->sn.numchannels;
	}
	if (endtime - soundtime > samps)
	{
		endtime = soundtime + samps;
	}

	/*DirectSound may have killed us to give priority to another app, ask to restore it*/
	if (sc->Restore)
		sc->Restore(sc);

	S_PaintChannels (sc, endtime);

	sc->Submit(sc, soundtime, endtime);
}

/*
called periodically by dedicated mixer threads.
do any blocking calls AFTER this returns. note that this means you can't use the Submit/unlock method to submit blocking audio.
*/
void S_MixerThread(soundcardinfo_t *sc)
{
	S_LockMixer();
	S_Update_(sc);
	S_UnlockMixer();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play(void)
{
	int 	i;
	char name[256];
	sfx_t	*sfx;

	i = 1;
	while (i<Cmd_Argc())
	{
		if (!Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strncpyz(name, Cmd_Argv(i), sizeof(name)-4);
			Q_strcat(name, ".wav");
		}
		else
			Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
		sfx = S_PrecacheSound(name);
		S_StartSound(cl.playerview[0].playernum+1, -1, sfx, vec3_origin, 1.0, 0.0, 0, 0);
		i++;
	}
}

void S_PlayVol(void)
{
	int i;
	float vol;
	char name[256];
	sfx_t	*sfx;

	i = 1;
	while (i<Cmd_Argc())
	{
		if (!Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strncpy(name, Cmd_Argv(i), sizeof(name)-4);
			Q_strcat(name, ".wav");
		}
		else
			Q_strncpy(name, Cmd_Argv(i), sizeof(name));
		sfx = S_PrecacheSound(name);
		vol = Q_atof(Cmd_Argv(i+1));
		S_StartSound(cl.playerview[0].playernum+1, -1, sfx, vec3_origin, vol, 0.0, 0, 0);
		i+=2;
	}
}

void S_SoundList_f(void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	sfxcache_t	scachebuf;
	int		size, total;
	int duration;

	S_LockMixer();


	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		if (sfx->decoder.decodedata)
		{
			sc = sfx->decoder.decodedata(sfx, &scachebuf, 0, 0x0fffffff);
			if (!sc)
			{
				Con_Printf("S(      )            : %s\n", sfx->name);
				continue;
			}
		}
		else
			sc = sfx->decoder.buf;
		if (!sc)
		{
			Con_Printf("?(      )            : %s\n", sfx->name);
			continue;
		}
		size = (sc->soundoffset+sc->length)*sc->width*(sc->numchannels);
		duration = (sc->soundoffset+sc->length) / sc->speed;
		total += size;
		if (sc->loopstart >= 0)
			Con_Printf ("L");
		else
			Con_Printf (" ");
		Con_Printf("(%2db%2ic) %6i %2is : %s\n",sc->width*8, sc->numchannels, size, duration, sfx->name);
	}
	Con_Printf ("Total resident: %i\n", total);

	S_UnlockMixer();
}


void S_LocalSound (const char *sound)
{
	sfx_t	*sfx;

	if (nosound.ival)
		return;
	if (!sound_started)
		return;

	sfx = S_PrecacheSound (sound);
	if (!sfx)
	{
		Con_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound (-1, -1, sfx, vec3_origin, 1, 1, 0, 0);
}











typedef struct {
	sfxdecode_t decoder;

	qboolean inuse;
	int id;
	sfx_t sfx;

	int numchannels;
	int width;
	int length;
	void *data;
} streaming_t;
#define MAX_RAW_SOURCES (MAX_CLIENTS+1)
streaming_t s_streamers[MAX_RAW_SOURCES];

void S_ClearRaw(void)
{
	memset(s_streamers, 0, sizeof(s_streamers));
}

//returns an sfxcache_t stating where the data is
sfxcache_t *S_Raw_Locate(sfx_t *sfx, sfxcache_t *buf, int start, int length)
{
	streaming_t *s = sfx->decoder.buf;
	if (buf)
	{
		buf->data = s->data;
		buf->length = s->length;
		buf->loopstart = -1;
		buf->numchannels = s->numchannels;
		buf->soundoffset = 0;
		buf->speed = snd_speed;
		buf->width = s->width;
	}
	return buf;
}

//streaming audio.	//this is useful when there is one source, and the sound is to be played with no attenuation
void S_RawAudio(int sourceid, qbyte *data, int speed, int samples, int channels, int width, float volume)
{
	soundcardinfo_t *si;
	int i;
	int prepadl;	//this is the amount of data that was previously available, and will be removed from the buffer.
	int spare;		//the amount of existing data that is still left to be played
	int outsamples;	//the amount of data we're going to add (at the output rate)
	double speedfactor;
	qbyte *newcache;
	streaming_t *s, *free=NULL;
	for (s = s_streamers, i = 0; i < MAX_RAW_SOURCES; i++, s++)
	{
		if (!s->inuse)
		{
			if (!free)
				free = s;
			continue;
		}
		if (s->id == sourceid)
			break;
	}
	if (!data)
	{
		if (i == MAX_RAW_SOURCES)
			return;	//wierd, it wasn't even playing.
		s->inuse = false;

		S_LockMixer();
		for (si = sndcardinfo; si; si=si->next)
		for (i = 0; i < si->total_chans; i++)
			if (si->channel[i].sfx == &s->sfx)
			{
				si->channel[i].sfx = NULL;
				break;
			}
		BZ_Free(s->data);
		S_UnlockMixer();
		return;
	}
	if (i == MAX_RAW_SOURCES || !s->inuse)	//whoops.
	{
		if (i == MAX_RAW_SOURCES)
		{
			if (!free)
			{
				Con_Printf("No free audio streams\n");
				return;
			}
			s = free;
		}
		s->sfx.decoder.buf = s;
		s->sfx.decoder.decodedata = S_Raw_Locate;
		s->numchannels = channels;
		s->width = width;
		s->data = NULL;
		s->length = 0;

		s->id = sourceid;
		s->inuse = true;
		strcpy(s->sfx.name, "raw stream");
//		Con_Printf("Added new raw stream\n");
	}
	S_LockMixer();

	if (s->width != width || s->numchannels != channels)
	{
		s->width = width;
		s->numchannels = channels;
		s->length = 0;
		Con_Printf("Restarting raw stream\n");
	}

	speedfactor	= (double)speed/snd_speed;
	outsamples = samples/speedfactor;

	prepadl = 0x7fffffff;
	for (si = sndcardinfo; si; si=si->next)	//make sure all cards are playing, and that we still get a prepad if just one is.
	{
		for (i = 0; i < si->total_chans; i++)
			if (si->channel[i].sfx == &s->sfx)
			{
				if (prepadl > (si->channel[i].pos>>PITCHSHIFT))
					prepadl = (si->channel[i].pos>>PITCHSHIFT);
				break;
			}
	}

	if (prepadl == 0x7fffffff)
	{
		if (snd_show.ival)
			Con_Printf("Wasn't playing\n");
		prepadl = 0;
		spare = 0;
		if (spare > snd_speed)
		{
			Con_DPrintf("Sacrificed raw sound stream\n");
			spare = 0;	//too far out. sacrifice it all
		}
	}
	else
	{
		if (prepadl < 0)
			prepadl = 0;
		spare = s->length - prepadl;
		if (spare < 0)	//remaining samples since last time
			spare = 0;

		if (spare > snd_speed*2) // more than 2 seconds of sound. don't buffer more than 2 seconds. 1: its probably buggy if we need to. 2: takes too much memory, and we use malloc+copies.
		{
			Con_DPrintf("Sacrificed raw sound stream\n");
			spare = 0;	//too far out. sacrifice it all
		}
	}

	newcache = BZ_Malloc((spare+outsamples) * (s->numchannels) * s->width);
	memcpy(newcache, (qbyte*)s->data + prepadl * (s->numchannels) * s->width, spare * (s->numchannels) * s->width);

	BZ_Free(s->data);
	s->data = newcache;

	s->length = spare + outsamples;

	{
		extern cvar_t snd_linearresample_stream;
		short *outpos = (short *)((char*)s->data + spare * (s->numchannels) * s->width);
		SND_ResampleStream(data,
			speed,
			width,
			channels,
			samples,
			outpos,
			snd_speed,
			s->width,
			s->numchannels,
			snd_linearresample_stream.ival);
	}

	for (si = sndcardinfo; si; si=si->next)
	{
		for (i = 0; i < si->total_chans; i++)
			if (si->channel[i].sfx == &s->sfx)
			{
				si->channel[i].pos -= prepadl*si->channel[i].rate;

				if (si->channel[i].pos < 0)
					si->channel[i].pos = 0;
				si->channel[i].master_vol = 255 * volume;
				if (si->ChannelUpdate)
					si->ChannelUpdate(si, &si->channel[i], false);
				break;
			}
		if (i == si->total_chans)	//this one wasn't playing.
		{
			channel_t *c = SND_PickChannel(si, -1, 0);
			if (c)
			{
				c->flags = CF_ABSVOLUME;
				c->entnum = -1;
				c->entchannel = 0;
				c->dist_mult = 0;
				c->looping = false;
				c->master_vol = 255 * volume;
				c->pos = 0;
				c->rate = 1<<PITCHSHIFT;
				c->sfx = &s->sfx;
				SND_Spatialize(si, c);

				if (si->ChannelUpdate)
					si->ChannelUpdate(si, c, true);
			}
		}
	}
	S_UnlockMixer();
}
