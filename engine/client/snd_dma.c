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
static sfx_t *S_FindName (char *name);

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

#define	MAX_SFX		512
sfx_t		*known_sfx;		// hunk allocated [MAX_SFX]
int			num_sfx;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

//int 		desired_speed = 44100;
int 		desired_bits = 16;

int sound_started=0;

cvar_t bgmvolume				= CVARAFD(	"musicvolume", "0", "bgmvolume", CVAR_ARCHIVE,
											"Volume level for background music.");
cvar_t volume					= CVARFD(	"volume", "0.7", CVAR_ARCHIVE,
											"Main volume level for all engine sound.");

cvar_t nosound					= CVARFD(	"nosound", "0", CVAR_ARCHIVE,
											"Disable all sound from the engine.");
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
cvar_t snd_khz					= CVARAFD(	"s_khz", "44",
											"snd_khz", CVAR_ARCHIVE, "Sound speed, in kilohertz. Common values are 11, 22, 44, 48.");
cvar_t	snd_inactive			= CVARAFD(	"s_inactive", "0",
											"snd_inactive", 0,
											"Play sound while application is inactive (ex. tabbed out). Needs a snd_restart if changed."
											);	//set if you want sound even when tabbed out.
cvar_t _snd_mixahead			= CVARAFD(	"s_mixahead", "0.08",
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

cvar_t snd_usemultipledevices	= CVARAFD(	"s_multipledevices", "0",
											"snd_multipledevices", 0, "If enabled, all output sound devices in your computer will be initialised for playback, not just the default device.");
cvar_t snd_driver		= CVARAF(	"s_driver", "",
											"snd_driver", 0);

#ifdef VOICECHAT
static void S_Voip_Play_Callback(cvar_t *var, char *oldval);
cvar_t cl_voip_send = CVARD("cl_voip_send", "0", "Sends voice-over-ip data to the server whenever it is set");
cvar_t cl_voip_vad_threshhold = CVARD("cl_voip_vad_threshhold", "15", "This is the threshhold for voice-activation-detection when sending voip data");
cvar_t cl_voip_vad_delay = CVARD("cl_voip_vad_delay", "0.3", "Keeps sending voice data for this many seconds after voice activation would normally stop");
cvar_t cl_voip_capturingvol = CVARAFD("cl_voip_capturingvol", "0.5", NULL, CVAR_ARCHIVE, "Volume multiplier applied while capturing, to avoid your audio from being heard by others");
cvar_t cl_voip_showmeter = CVARAFD("cl_voip_showmeter", "1", NULL, CVAR_ARCHIVE, "Shows your speech volume above the hud. 0=hide, 1=show when transmitting, 2=ignore voice-activation disable");

cvar_t cl_voip_play = CVARAFDC("cl_voip_play", "1", NULL, CVAR_ARCHIVE, "Enables voip playback.", S_Voip_Play_Callback);
cvar_t cl_voip_micamp = CVARAFDC("cl_voip_micamp", "2", NULL, CVAR_ARCHIVE, "Amplifies your microphone when using voip.", 0);
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
		Con_Printf("%5d stereo\n", sc->sn.numchannels - 1);
		Con_Printf("%5d samples\n", sc->sn.samples);
		Con_Printf("%5d samplepos\n", sc->sn.samplepos);
		Con_Printf("%5d samplebits\n", sc->sn.samplebits);
		Con_Printf("%5d speed\n", sc->sn.speed);
		Con_Printf("%5d total_channels\n", sc->total_chans);
	}
}

#ifdef VOICECHAT
#include <speex.h>
#include <speex_preprocess.h>
static struct
{
	qboolean inited;
	qboolean loaded;
	dllhandle_t *speexlib;
	dllhandle_t *speexdsplib;

	SpeexBits encbits;
	void *encoder;
	SpeexPreprocessState *preproc;
	unsigned int framesize;
	unsigned int samplerate;

	SpeexBits decbits[MAX_CLIENTS];
	void *decoder[MAX_CLIENTS];
	unsigned char decseq[MAX_CLIENTS];	/*sender's sequence, to detect+cover minor packetloss*/
	unsigned char decgen[MAX_CLIENTS];	/*last generation. if it changes, we flush speex to reset packet loss*/
	float decamp[MAX_CLIENTS];	/*amplify them by this*/
	float lastspoke[MAX_CLIENTS];	/*time when they're no longer considered talking. if future, they're talking*/

	unsigned char capturebuf[32768]; /*pending data*/
	unsigned int capturepos;/*amount of pending data*/
	unsigned int encsequence;/*the outgoing sequence count*/
	unsigned int generation;/*incremented whenever capture is restarted*/
	qboolean wantsend;	/*set if we're capturing data to send*/
	float voiplevel;	/*your own voice level*/
	unsigned int dumps;	/*trigger a new generation thing after a bit*/
	unsigned int keeps;	/*for vad_delay*/

	snd_capture_driver_t *driver;/*capture driver's functions*/
	void *driverctx;	/*capture driver context*/
} s_speex;

static const SpeexMode *(VARGS *qspeex_lib_get_mode)(int mode);
static void (VARGS *qspeex_bits_init)(SpeexBits *bits);
static void (VARGS *qspeex_bits_reset)(SpeexBits *bits);
static int (VARGS *qspeex_bits_write)(SpeexBits *bits, char *bytes, int max_len);

static SpeexPreprocessState *(VARGS *qspeex_preprocess_state_init)(int frame_size, int sampling_rate);
static int (VARGS *qspeex_preprocess_ctl)(SpeexPreprocessState *st, int request, void *ptr);
static int (VARGS *qspeex_preprocess_run)(SpeexPreprocessState *st, spx_int16_t *x);

static void * (VARGS *qspeex_encoder_init)(const SpeexMode *mode);
static int (VARGS *qspeex_encoder_ctl)(void *state, int request, void *ptr);
static int (VARGS *qspeex_encode_int)(void *state, spx_int16_t *in, SpeexBits *bits);

static void *(VARGS *qspeex_decoder_init)(const SpeexMode *mode);
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
	{(void*)&qspeex_decode_int, "speex_decode_int"},
	{(void*)&qspeex_bits_read_from, "speex_bits_read_from"},

	{NULL}
};
static dllfunction_t qspeexdspfuncs[] =
{
	{(void*)&qspeex_preprocess_state_init, "speex_preprocess_state_init"},
	{(void*)&qspeex_preprocess_ctl, "speex_preprocess_ctl"},
	{(void*)&qspeex_preprocess_run, "speex_preprocess_run"},

	{NULL}
};

snd_capture_driver_t DSOUND_Capture;
snd_capture_driver_t OSS_Capture;

static qboolean S_Speex_Init(void)
{
	int i;
	const SpeexMode *mode;
	if (s_speex.inited)
		return s_speex.loaded;
	s_speex.inited = true;

	s_speex.speexlib = Sys_LoadLibrary("libspeex", qspeexfuncs);
	if (!s_speex.speexlib)
	{
		Con_Printf("libspeex not found. Voice chat is not available.\n");
		return false;
	}

	s_speex.speexdsplib = Sys_LoadLibrary("libspeexdsp", qspeexdspfuncs);
	if (!s_speex.speexdsplib)
	{
		Con_Printf("libspeexdsp not found. Voice chat is not available.\n");
		return false;
	}

	mode = qspeex_lib_get_mode(SPEEX_MODEID_NB);


	qspeex_bits_init(&s_speex.encbits);
	qspeex_bits_reset(&s_speex.encbits);

	s_speex.encoder = qspeex_encoder_init(mode);

	qspeex_encoder_ctl(s_speex.encoder, SPEEX_GET_FRAME_SIZE, &s_speex.framesize);
	qspeex_encoder_ctl(s_speex.encoder, SPEEX_GET_SAMPLING_RATE, &s_speex.samplerate);
	s_speex.samplerate = 11025;
	qspeex_encoder_ctl(s_speex.encoder, SPEEX_SET_SAMPLING_RATE, &s_speex.samplerate);

	s_speex.preproc = qspeex_preprocess_state_init(s_speex.framesize, s_speex.samplerate);

	i = 1;
	qspeex_preprocess_ctl(s_speex.preproc, SPEEX_PREPROCESS_SET_DENOISE, &i);

	i = 1;
	qspeex_preprocess_ctl(s_speex.preproc, SPEEX_PREPROCESS_SET_AGC, &i);

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		qspeex_bits_init(&s_speex.decbits[i]);
		qspeex_bits_reset(&s_speex.decbits[i]);
		s_speex.decoder[i] = qspeex_decoder_init(mode);
		s_speex.decamp[i] = 1;
	}
	s_speex.loaded = true;
	return s_speex.loaded;
}

void S_Voip_Parse(void)
{
	unsigned int sender;
	unsigned int bytes;
	unsigned char data[1024], *start;
	short decodebuf[1024];
	unsigned int decodesamps, len, newseq, drops;
	unsigned char seq, gen;
	float amp = 1;
	unsigned int i;

	sender = MSG_ReadByte();
	gen = MSG_ReadByte();
	seq = MSG_ReadByte();
	bytes = MSG_ReadShort();
	if (bytes > sizeof(data) || !cl_voip_play.ival || !S_Speex_Init() || (gen & 0xf0))
	{
		MSG_ReadSkip(bytes);
		return;
	}
	MSG_ReadData(data, bytes);

	sender &= MAX_CLIENTS-1;

	amp = s_speex.decamp[sender];

	decodesamps = 0;
	newseq = 0;
	drops = 0;
	start = data;

	s_speex.lastspoke[sender] = realtime + 0.5;
	if (s_speex.decgen[sender] != gen)
	{
		qspeex_bits_reset(&s_speex.decbits[sender]);
		s_speex.decgen[sender] = gen;
		s_speex.decseq[sender] = seq;
	}

	while (bytes > 0)
	{
		if (decodesamps + s_speex.framesize > sizeof(decodebuf)/sizeof(decodebuf[0]))
		{
			S_RawAudio(sender, (qbyte*)decodebuf, s_speex.samplerate, decodesamps, 1, 2);
			decodesamps = 0;
		}

		if (s_speex.decseq[sender] != seq)
		{
			qspeex_decode_int(s_speex.decoder[sender], NULL, decodebuf + decodesamps);
			s_speex.decseq[sender]++;
			drops++;
		}
		else
		{
			bytes--;
			len = *start++;
			qspeex_bits_read_from(&s_speex.decbits[sender], start, len);
			bytes -= len;
			start += len;
			qspeex_decode_int(s_speex.decoder[sender], &s_speex.decbits[sender], decodebuf + decodesamps);
			newseq++;
		}
		if (amp != 1)
		{
			for (i = decodesamps; i < decodesamps+s_speex.framesize; i++)
				decodebuf[i] *= amp;
		}
		decodesamps += s_speex.framesize;
	}
	s_speex.decseq[sender] += newseq;

	if (drops)
		Con_DPrintf("%i dropped audio frames\n", drops);

	if (decodesamps > 0)
		S_RawAudio(sender, (qbyte*)decodebuf, s_speex.samplerate, decodesamps, 1, 2);
}

void S_Voip_Transmit(unsigned char clc, sizebuf_t *buf)
{
	unsigned char outbuf[1024];
	unsigned int outpos;//in bytes
	unsigned int encpos;//in bytes
	short *start;
	unsigned char initseq;//in frames
	unsigned int i;
	unsigned int samps;
	float level, f;
	float micamp = cl_voip_micamp.value;
	qboolean voipsendenable = true;

	/*if you're sending sound, you should be prepared to accept others yelling at you to shut up*/
	if (!cl_voip_play.ival)
		voipsendenable = false;
	if (!(cls.fteprotocolextensions2 & PEXT2_VOICECHAT))
		voipsendenable = false;

	if (!voipsendenable)
	{
		if (s_speex.driver)
		{
			if (s_speex.wantsend)
				s_speex.driver->Stop(s_speex.driverctx);
			s_speex.driver->Shutdown(s_speex.driverctx);
			s_speex.driverctx = NULL;
			s_speex.driver = NULL;
		}
		return;
	}

	voipsendenable = cl_voip_send.ival>0;

	if (!s_speex.driver)
	{
		s_speex.voiplevel = -1;
		/*only init the first time capturing is requested*/
		if (!voipsendenable)
			return;

		/*Add new drivers in order of priority*/
		if (!s_speex.driver || !s_speex.driver->Init)
			s_speex.driver = &DSOUND_Capture;
		if (!s_speex.driver || !s_speex.driver->Init)
			s_speex.driver = &OSS_Capture;

		/*no way to capture audio, give up*/
		if (!s_speex.driver || !s_speex.driver->Init)
			return;

		/*see if we can init speex...*/
		if (!S_Speex_Init())
			return;

		s_speex.driverctx = s_speex.driver->Init(s_speex.samplerate);
	}

	/*couldn't init a driver?*/
	if (!s_speex.driverctx)
	{
		return;
	}

	if (!voipsendenable && s_speex.wantsend)
	{
		s_speex.wantsend = false;
		s_speex.capturepos += s_speex.driver->Update(s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, 1, sizeof(s_speex.capturebuf) - s_speex.capturepos);
		s_speex.driver->Stop(s_speex.driverctx);
		/*note: we still grab audio to flush everything that was captured while it was active*/
	}
	else if (voipsendenable && !s_speex.wantsend)
	{
		s_speex.wantsend = true;
		if (!s_speex.capturepos)
		{	/*if we were actually still sending, it was probably only off for a single frame, in which case don't reset it*/
			s_speex.dumps = 0;
			s_speex.generation++;
			s_speex.encsequence = 0;
			qspeex_bits_reset(&s_speex.encbits);
		}
		else
		{
			s_speex.capturepos += s_speex.driver->Update(s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, 1, sizeof(s_speex.capturebuf) - s_speex.capturepos);
		}
		s_speex.driver->Start(s_speex.driverctx);

		voicevolumemod = cl_voip_capturingvol.value;
	}

	s_speex.capturepos += s_speex.driver->Update(s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, s_speex.framesize*2, sizeof(s_speex.capturebuf) - s_speex.capturepos);

	if (!s_speex.wantsend && s_speex.capturepos < s_speex.framesize*2)
	{
		s_speex.voiplevel = -1;
		s_speex.capturepos = 0;
		voicevolumemod = 1;
		return;
	}

	initseq = s_speex.encsequence;
	level = 0;
	samps=0;
	for (encpos = 0, outpos = 0; s_speex.capturepos-encpos >= s_speex.framesize*2 && sizeof(outbuf)-outpos > 64; s_speex.encsequence++)
	{
		start = (short*)(s_speex.capturebuf + encpos);

		qspeex_preprocess_run(s_speex.preproc, start);

		for (i = 0; i < s_speex.framesize; i++)
		{
			f = start[i] * micamp;
			start[i] = f;
			f = fabs(start[i]);
			level += f*f;
		}
		samps+=s_speex.framesize;

		qspeex_bits_reset(&s_speex.encbits);
		qspeex_encode_int(s_speex.encoder, start, &s_speex.encbits);
		outbuf[outpos] = qspeex_bits_write(&s_speex.encbits, outbuf+outpos+1, sizeof(outbuf) - (outpos+1));
		outpos += 1+outbuf[outpos];
		encpos += s_speex.framesize*2;
	}
	if (samps)
	{
		float nl;
		nl = (3000*level) / (32767.0f*32767*samps);
		s_speex.voiplevel = (s_speex.voiplevel*7 + nl)/8;
		if (s_speex.voiplevel < cl_voip_vad_threshhold.ival && !(cl_voip_send.ival & 2))
		{
			/*try and dump it, it was too quiet, and they're not pressing +voip*/
			if (s_speex.keeps > samps)
			{
				/*but not instantly*/
				s_speex.keeps -= samps;
			}
			else
			{
				outpos = 0;
				s_speex.dumps += samps;
				s_speex.keeps = 0;
			}
		}
		else
			s_speex.keeps = s_speex.samplerate * cl_voip_vad_delay.value;
		if (outpos)
		{
			if (s_speex.dumps > s_speex.samplerate/4)
				s_speex.generation++;
			s_speex.dumps = 0;
		}
	}

	if (outpos && buf->maxsize - buf->cursize >= outpos+4)
	{
		MSG_WriteByte(buf, clc);
		MSG_WriteByte(buf, (s_speex.generation & 0x0f)); /*gonna leave that nibble clear here... in this version, the client will ignore packets with those bits set. can use them for codec or something*/
		MSG_WriteByte(buf, initseq);
		MSG_WriteShort(buf, outpos);
		SZ_Write(buf, outbuf, outpos);
	}

	/*remove sent data*/
	memmove(s_speex.capturebuf, s_speex.capturebuf + encpos, s_speex.capturepos-encpos);
	s_speex.capturepos -= encpos;
}
void S_Voip_Ignore(unsigned int slot, qboolean ignore)
{
	CL_SendClientCommand(true, "vignore %i %i", slot, ignore);
}
static void S_Voip_Enable_f(void)
{
	Cvar_SetValue(&cl_voip_send, cl_voip_send.ival | 2);
}
static void S_Voip_Disable_f(void)
{
	Cvar_SetValue(&cl_voip_send, cl_voip_send.ival & ~2);
}
static void S_Voip_f(void)
{
	int i;
	if (!strcmp(Cmd_Argv(1), "maxgain"))
	{
		i = atoi(Cmd_Argv(2));
		qspeex_preprocess_ctl(s_speex.preproc, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &i);
	}
}
static void S_Voip_Play_Callback(cvar_t *var, char *oldval)
{
	if (cls.fteprotocolextensions2 & PEXT2_VOICECHAT)
	{
		if (var->ival)
			CL_SendClientCommand(true, "unmuteall");
		else
			CL_SendClientCommand(true, "muteall");
	}
}
void S_Voip_MapChange(void)
{
	Cvar_ForceCallback(&cl_voip_play);
}
int S_Voip_Loudness(qboolean ignorevad)
{
	if (s_speex.voiplevel > 100)
		return 100;
	if (!s_speex.driverctx || (!ignorevad && s_speex.dumps))
		return -1;
	return s_speex.voiplevel;
}
qboolean S_Voip_Speaking(unsigned int plno)
{
	if (plno >= MAX_CLIENTS)
		return false;
	return s_speex.lastspoke[plno] > realtime;
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


sounddriver pOPENAL_InitCard;
sounddriver pDSOUND_InitCard;
sounddriver pALSA_InitCard;
sounddriver pOSS_InitCard;
sounddriver pMacOS_InitCard;
sounddriver pSDL_InitCard;
sounddriver pWAV_InitCard;
sounddriver pDroid_InitCard;
sounddriver pAHI_InitCard;
sounddriver pPPAPI_InitCard;

typedef struct {
	char *name;
	sounddriver *ptr;
} sdriver_t;
sdriver_t drivers[] = {
//in order of preference
	{"OpenAL", &pOPENAL_InitCard},	//yay, get someone else to sort out sound support, woot

	{"DSound", &pDSOUND_InitCard},	//prefered on windows
	{"MacOS", &pMacOS_InitCard},	//prefered on mac
	{"Droid", &pDroid_InitCard},		//prefered on android (java thread)
	{"AHI", &pAHI_InitCard},		//prefered on morphos
	{"PPAPI", &pPPAPI_InitCard},	//google's native client

	{"SDL", &pSDL_InitCard},		//prefered on linux
	{"ALSA", &pALSA_InitCard},		//pure shite
	{"OSS", &pOSS_InitCard},		//good, but not likely to work any more

	{"WaveOut", &pWAV_InitCard},	//doesn't work properly in vista, etc.
	{NULL, NULL}
};

static int SNDDMA_Init(soundcardinfo_t *sc, int *cardnum, int *drivernum)
{
	sdriver_t *sd;
	int st = 0;

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

	if (*snd_driver.string)
	{
		if (*drivernum)
			return 2;
		for (sd = drivers; sd->name; sd++)
			if (!Q_strcasecmp(sd->name, snd_driver.string))
				break;
	}
	else
		sd = &drivers[*drivernum];
	if (!sd->ptr)
		return 2;	//no more cards.
	if (!*sd->ptr)	//driver not loaded
	{
		Con_DPrintf("Sound driver %s is not loaded\n", sd->name);
		st = 2;
	}
	else
	{
		Con_DPrintf("Trying to load a %s sound device\n", sd->name);
		st = (**sd->ptr)(sc, *cardnum);
	}

	if (st == 1)	//worked
	{
		*cardnum += 1;	//use the next card next time
		return st;
	}
	else if (st == 0)	//failed, try the next card with this driver.
	{
		*cardnum += 1;
		return SNDDMA_Init(sc, cardnum, drivernum);
	}
	else	//card number wasn't valid, try the first card of the next driver
	{
		*cardnum = 0;
		*drivernum += 1;
		return SNDDMA_Init(sc, cardnum, drivernum);
	}
}

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


/*
================
S_Startup
================
*/

void S_ClearRaw(void);
void S_Startup (void)
{
	int cardnum, drivernum;
	int warningmessage=0;
	int		rc;
	soundcardinfo_t *sc;

	if (!snd_initialized)
		return;

	if (sound_started)
		S_Shutdown();

	snd_blocked = 0;
	snd_speed = 0;

	for(cardnum = 0, drivernum = 0;;)
	{
		sc = Z_Malloc(sizeof(soundcardinfo_t));
		rc = SNDDMA_Init(sc, &cardnum, &drivernum);

		if (!rc)	//error stop
		{
			Con_Printf("S_Startup: SNDDMA_Init failed.\n");
			Z_Free(sc);
			break;
		}
		if (rc == 2)	//silently stop (no more cards)
		{
			Z_Free(sc);
			break;
		}

		S_DefaultSpeakerConfiguration(sc);

		if (sndcardinfo)
		{	//if the sample speeds of multiple soundcards do not match, it'll fail.
			if (snd_speed != sc->sn.speed)
			{
				if (!warningmessage)
				{
					Con_Printf("S_Startup: Ignoring soundcard %s due to mismatched sample speeds.\nTry running Quake with -singlesound to use just the primary soundcard\n", sc->name);
					S_ShutdownCard(sc);
					warningmessage = true;
				}

				Z_Free(sc);
				continue;
			}
		}
		else
			snd_speed = sc->sn.speed;

		sc->next = sndcardinfo;
		sndcardinfo = sc;

		if (!snd_usemultipledevices.ival)
			break;
	}

	sound_started = !!sndcardinfo;

	S_ClearRaw();

	CL_InitTEntSounds();
}

void S_SetUnderWater(qboolean underwater)
{
	soundcardinfo_t *sc;

	for (sc = sndcardinfo; sc; sc=sc->next)
		sc->SetWaterDistortion(sc, underwater);
}

//why isn't this part of S_Restart_f anymore?
//so that the video code can call it directly without flushing the models it's just loaded.
void S_DoRestart (void)
{
	int i;

	S_StopAllSounds (true);
	S_Shutdown();

	if (nosound.ival)
		return;

	S_Startup();

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");

	S_StopAllSounds (true);


	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.sound_name[i][0])
			break;
		cl.sound_precache[i] = S_FindName (cl.sound_name[i]);
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

		S_Shutdown();
		sound_started = 0;
	}
	else if (!Q_strcasecmp(command, "multi") || !Q_strcasecmp(command, "multiple"))
	{
		if (!Q_strcasecmp(Cmd_Argv (2), "off"))
		{
			if (snd_usemultipledevices.ival)
			{
				Cvar_SetValue(&snd_usemultipledevices, 0);
				S_Restart_f();
			}
		}
		else if (!snd_usemultipledevices.ival)
		{
			Cvar_SetValue(&snd_usemultipledevices, 1);
			S_Restart_f();
		}
		return;
	}
	if (!Q_strcasecmp(command, "single"))
	{
		Cvar_SetValue(&snd_usemultipledevices, 0);
		S_Restart_f();
		return;
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

#ifdef VOICECHAT
	Cvar_Register(&cl_voip_send,		"Voice Chat");
	Cvar_Register(&cl_voip_vad_threshhold,	"Voice Chat");
	Cvar_Register(&cl_voip_vad_delay,	"Voice Chat");
	Cvar_Register(&cl_voip_capturingvol,	"Voice Chat");
	Cvar_Register(&cl_voip_showmeter,	"Voice Chat");
	Cvar_Register(&cl_voip_play,		"Voice Chat");
	Cvar_Register(&cl_voip_micamp,		"Voice Chat");
	Cmd_AddCommand("+voip", S_Voip_Enable_f);
	Cmd_AddCommand("-voip", S_Voip_Disable_f);
	Cmd_AddCommand("voip", S_Voip_f);
#endif

	Cvar_Register(&snd_inactive,		"Sound controls");

	Cvar_Register(&snd_playersoundvolume,		"Sound controls");
	Cvar_Register(&snd_usemultipledevices,		"Sound controls");
	Cvar_Register(&snd_driver,		"Sound controls");

	Cvar_Register(&snd_linearresample, "Sound controls");
	Cvar_Register(&snd_linearresample_stream, "Sound controls");

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

	if (COM_CheckParm ("-nomultipledevices") || COM_CheckParm ("-singlesound"))
		Cvar_SetValue(&snd_usemultipledevices, 0);

	if (COM_CheckParm ("-multisound"))
		Cvar_SetValue(&snd_usemultipledevices, 1);


	if (host_parms.memsize < 0x800000)
	{
		Cvar_Set (&loadas8bit, "1");
		Con_Printf ("loading all sounds as 8bit\n");
	}

	snd_initialized = true;

	known_sfx = Hunk_AllocName (MAX_SFX*sizeof(sfx_t), "sfx_t");
	num_sfx = 0;

// create a piece of DMA memory

	if (sndcardinfo)
		Con_SafePrintf ("Sound sampling rate: %i\n", sndcardinfo->sn.speed);

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");

	S_StopAllSounds (true);
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_ShutdownCard(soundcardinfo_t *sc)
{
	soundcardinfo_t *prev;
#if defined(_WIN32) && defined(AVAIL_DSOUND)
	extern int aimedforguid;
	aimedforguid = 0;
#endif

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
void S_Shutdown(void)
{
	soundcardinfo_t *sc, *next;
#if defined(_WIN32) && defined(AVAIL_DSOUND)
	extern int aimedforguid;
	aimedforguid = 0;
#endif

	for (sc = sndcardinfo; sc; sc=next)
	{
		next = sc->next;
		sc->Shutdown(sc);
		Z_Free(sc);
		sndcardinfo = next;
	}

	sound_started = 0;
	S_Purge(false);

	num_sfx = 0;
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
static sfx_t *S_FindName (char *name)
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

	sfx = &known_sfx[i];
	strcpy (sfx->name, name);
	known_sfx[i].touched = true;

	num_sfx++;

	return sfx;
}

void S_Purge(qboolean retaintouched)
{
	sfx_t	*sfx;
	int i;

	S_LockMixer();
	for (i=0 ; i < num_sfx ; i++)
	{
		sfx = &known_sfx[i];

		/*don't purge the file if its still relevent*/
		if (retaintouched && sfx->touched)
			continue;

		/*nothing to do if there's no data within*/
		if (!sfx->decoder.buf)
			continue;

		/*stop the decoder first*/
		if (sfx->decoder.abort)
			sfx->decoder.abort(sfx);

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
		known_sfx[i].failedload = false;
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

	S_FindName (name);
}

/*
==================
S_PrecacheSound

==================
*/
sfx_t *S_PrecacheSound (char *name)
{
	sfx_t	*sfx;

	if (nosound.ival)
		return NULL;

	sfx = S_FindName (name);

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
		if (sc->channel[ch_idx].entnum == cl.playernum[0]+1 && entnum != cl.playernum[0]+1 && sc->channel[ch_idx].sfx)
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
	sfx_t *snd;
	int i;

// anything coming from the view entity will always be full volume
	if (ch->entnum == -1 || ch->entnum == cl.playernum[0]+1)
	{
		for (i = 0; i < sc->sn.numchannels; i++)
		{
			ch->vol[i] = ch->master_vol * (ruleset_allow_localvolume.value ? snd_playersoundvolume.value : 1);
		}
		return;
	}

// calculate stereo seperation and distance attenuation
	snd = ch->sfx;
	VectorSubtract(ch->origin, listener_origin, world_vec);

	dist = VectorNormalize(world_vec) * ch->dist_mult;

	listener_vec[1] = DotProduct(listener_right, world_vec);
	listener_vec[0] = DotProduct(listener_forward, world_vec);
	listener_vec[2] = DotProduct(listener_up, world_vec);

	if (snd_leftisright.ival)
		listener_vec[1] = -listener_vec[1];

	for (i = 0; i < sc->sn.numchannels; i++)
	{
		scale = (1 + DotProduct(listener_vec, sc->speakerdir[i])) / 2;
		if (scale > 1)
			scale = 1;
		scale = (1.0 - dist) * scale * sc->dist[i];
		ch->vol[i] = (int) (ch->master_vol * scale);
		if (ch->vol[i] < 0)
			ch->vol[i] = 0;
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
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(sc, target_chan);

	if (!target_chan->vol[0] && !target_chan->vol[1] && !target_chan->vol[2] && !target_chan->vol[3] && !target_chan->vol[4] && !target_chan->vol[5])
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

	S_LockMixer();
	for (sc = sndcardinfo; sc; sc = sc->next)
		S_StartSoundCard(sc, entnum, entchannel, sfx, origin, fvol, attenuation, -(int)(timeofs * sc->sn.speed), pitchadj);
	S_UnlockMixer();
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
				if (s->decoder.abort)
				if (!S_IsPlayingSomewhere(s))	//if we aint playing it elsewhere, free it compleatly.
				{
					s->decoder.abort(s);
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

			if (s)
			if (s->decoder.abort)
			if (!S_IsPlayingSomewhere(s))	//if we aint playing it elsewhere, free it compleatly.
			{
				s->decoder.abort(s);
//				if (s->cache.data)
//					Cache_Free(&s->cache);
			}
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
		chan = &sc->channel[i];
		if (!chan->sfx)
		{
			char *nexttrack = Media_NextTrack(i-MUSIC_FIRST);
			sfx_t *newmusic;

			if (nexttrack && *nexttrack)
			{
				newmusic = S_PrecacheSound(nexttrack);

				if (newmusic && !newmusic->failedload)
				{
					chan->sfx = newmusic;
					chan->rate = 1<<PITCHSHIFT;
					chan->pos = 0;
					chan->vol[0] = chan->vol[1] = chan->vol[2] = chan->vol[3] = chan->vol[4] = chan->vol[5] = chan->master_vol = 100;
				}
			}
		}
		if (chan->sfx)
			chan->vol[0] = chan->vol[1] = chan->vol[2] = chan->vol[3] = chan->vol[4] = chan->vol[5] = chan->master_vol = (255/volume.value)*bgmvolume.value;
	}


// calc ambient sound levels
	if (!cl.worldmodel || cl.worldmodel->type != mod_brush || cl.worldmodel->fromgame != fg_quake)
		return;

	l = Q1BSP_LeafForPoint(cl.worldmodel, listener_origin);
	if (!l || !ambient_level.value)
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
		chan->vol[0] = chan->vol[1] = chan->vol[2] = chan->vol[3] = chan->vol[4] = chan->vol[5] = chan->master_vol;

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
	if (sc->openal == 1)
	{
		OpenAL_Update_Listener(listener_origin, listener_forward, listener_right, listener_up, listener_velocity);
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

int GetSoundtime(soundcardinfo_t *sc)
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
		sc->buffers++;					// buffer wrapped

		if (sc->paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			sc->buffers = 0;
			sc->paintedtime = fullsamples;
			S_StopAllSounds (true);
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

#ifdef _WIN32
	IN_Accumulate ();
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
	soundtime = GetSoundtime(sc);

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
		S_StartSound(cl.playernum[0]+1, -1, sfx, vec3_origin, 1.0, 0.0, 0, 0);
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
		S_StartSound(cl.playernum[0]+1, -1, sfx, vec3_origin, vol, 0.0, 0, 0);
		i+=2;
	}
}

void S_SoundList_f(void)
{
	/*
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		if (!sfx->decoder)
		{
			Con_Printf("S(      )        : %s\n", sfx->name);
			continue;
		}
		sc = Cache_Check (&sfx->cache);
		if (!sc)
			continue;
		size = sc->length*sc->width*(sc->numchannels);
		total += size;
		if (sc->loopstart >= 0)
			Con_Printf ("L");
		else
			Con_Printf (" ");
		Con_Printf("(%2db%2ic) %6i : %s\n",sc->width*8, sc->numchannels, size, sfx->name);
	}
	Con_Printf ("Total resident: %i\n", total);
	*/
}


void S_LocalSound (char *sound)
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
void S_RawAudio(int sourceid, qbyte *data, int speed, int samples, int channels, int width)
{
	soundcardinfo_t *si;
	int i;
	int prepadl;
	int oldlength;
	int spare;
	int outsamples;
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
//		Con_Printf("Restarting raw stream\n");
	}

	speedfactor	= (double)speed/snd_speed;
	outsamples = samples/speedfactor;

	oldlength = s->length;

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

		if (spare > snd_speed*2) // more than 2 seconds of sound
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
				break;
			}
		if (i == si->total_chans)	//this one wasn't playing.
		{
			channel_t *c = SND_PickChannel(si, -1, 0);
			c->entnum = -1;
			c->entchannel = 0;
			c->dist_mult = 0;
			c->looping = false;
			c->master_vol = 255;
			c->pos = 0;
			c->rate = 1<<PITCHSHIFT;
			c->sfx = &s->sfx;
			c->start = 0;
			SND_Spatialize(si, c);
		}
	}
	S_UnlockMixer();
}
