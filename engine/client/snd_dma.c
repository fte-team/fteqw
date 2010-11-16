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

void S_Play(void);
void S_PlayVol(void);
void S_SoundList(void);
void S_Update_(soundcardinfo_t *sc);
void S_StopAllSounds(qboolean clear);
void S_StopAllSoundsC(void);

void S_UpdateCard(soundcardinfo_t *sc);

// =======================================================================
// Internal sound data & structures
// =======================================================================

soundcardinfo_t *sndcardinfo;	//the master card.

int				snd_blocked = 0;
static qboolean	snd_ambient = 1;
qboolean		snd_initialized = false;
int				snd_speed;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
vec_t		sound_nominal_clip_dist=1000.0;

int			soundtime;		// sample PAIRS


#define	MAX_SFX		512
sfx_t		*known_sfx;		// hunk allocated [MAX_SFX]
int			num_sfx;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

//int 		desired_speed = 44100;
int 		desired_bits = 16;

int sound_started=0;

cvar_t bgmvolume				= CVARF(	"musicvolume", "0", CVAR_ARCHIVE);
cvar_t volume					= CVARF(	"volume", "0.7", CVAR_ARCHIVE);

cvar_t nosound					= CVAR(		"nosound", "0");
cvar_t precache					= CVARAF(	"s_precache", "1",
											"precache", 0);
cvar_t loadas8bit				= CVARAF(	"s_loadas8bit", "0",
											"loadas8bit", 0);
cvar_t bgmbuffer				= CVAR(		"bgmbuffer", "4096");
cvar_t ambient_level			= CVARAF(	"s_ambientlevel", "0.3",
											"ambient_level", 0);
cvar_t ambient_fade				= CVARAF(	"s_ambientfade", "100",
											"ambient_fade", 0);
cvar_t snd_noextraupdate		= CVARAF(	"s_noextraupdate", "0",
											"snd_noextraupdate", 0);
cvar_t snd_show					= CVARAF(	"s_show", "0",
											"snd_show", 0);
cvar_t snd_khz					= CVARAF(	"s_khz", "11",
											"snd_khz", CVAR_ARCHIVE);
cvar_t	snd_inactive			= CVARAF(	"s_inactive", "0",
											"snd_inactive", 0);	//set if you want sound even when tabbed out.
cvar_t _snd_mixahead			= CVARAF(	"s_mixahead", "0.2",
											"_snd_mixahead", CVAR_ARCHIVE);
cvar_t snd_leftisright			= CVARAF(	"s_swapstereo", "0",
											"snd_leftisright", CVAR_ARCHIVE);
cvar_t snd_eax					= CVARAF(	"s_eax", "0",
											"snd_eax", 0);
cvar_t snd_speakers				= CVARAF(	"s_numspeakers", "2",
											"snd_numspeakers", 0);
cvar_t snd_buffersize			= CVARAF(	"s_buffersize", "0",
											"snd_buffersize", 0);
cvar_t snd_samplebits			= CVARAF(	"s_bits", "16",
											"snd_samplebits", CVAR_ARCHIVE);
cvar_t snd_playersoundvolume	= CVARAF(	"s_localvolume", "1",
											"snd_localvolume", 0);	//sugested by crunch

cvar_t snd_linearresample		= CVARAF(	"s_linearresample", "1",
											"snd_linearresample", 0);
cvar_t snd_linearresample_stream = CVARAF(	"s_linearresample_stream", "0",
											"snd_linearresample_stream", 0);

cvar_t snd_usemultipledevices	= CVARAF(	"s_multipledevices", "0",
											"snd_multipledevices", 0);

#ifdef VOICECHAT
static void S_Voip_Play_Callback(cvar_t *var, char *oldval);
cvar_t cl_voip_send = CVAR("cl_voip_send", "0");
cvar_t cl_voip_play = CVARC("cl_voip_play", "1", S_Voip_Play_Callback);
cvar_t cl_voip_micamp = CVAR("cl_voip_micamp", "2");
#endif

extern vfsfile_t *rawwritefile;


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
#include "speex/speex.h"
#include "speex/speex_preprocess.h"
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
	unsigned char decseq[MAX_CLIENTS];
	float decamp[MAX_CLIENTS];
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
		Con_Printf("libspeex not found. Voice chat not available.\n");
		return false;
	}

	s_speex.speexdsplib = Sys_LoadLibrary("libspeexdsp", qspeexdspfuncs);
	if (!s_speex.speexdsplib)
	{
		Con_Printf("libspeexdsp not found. Voice chat not available.\n");
		return false;
	}

	mode = qspeex_lib_get_mode(SPEEX_MODEID_NB);


	qspeex_bits_init(&s_speex.encbits);
	qspeex_bits_reset(&s_speex.encbits);

	s_speex.encoder = qspeex_encoder_init(mode);

	qspeex_encoder_ctl(s_speex.encoder, SPEEX_GET_FRAME_SIZE, &s_speex.framesize);
	qspeex_encoder_ctl(s_speex.encoder, SPEEX_GET_SAMPLING_RATE, &s_speex.samplerate);

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

void S_ParseVoiceChat(void)
{
	unsigned int sender = MSG_ReadByte();
	int bytes;
	unsigned char data[1024], *start;
	short decodebuf[1024];
	unsigned int decodesamps, len, newseq, drops;
	unsigned char seq;
	float amp = 1;
	unsigned int i;
	seq = MSG_ReadByte();
	bytes = MSG_ReadShort();
	if (bytes > sizeof(data) || !cl_voip_play.ival)
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
	while (bytes > 0)
	{
		if (decodesamps + s_speex.framesize > sizeof(decodebuf)/sizeof(decodebuf[0]))
		{
			S_RawAudio(sender, (qbyte*)decodebuf, 11025, decodesamps, 1, 2);
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
		S_RawAudio(sender, (qbyte*)decodebuf, 11025, decodesamps, 1, 2);
}

unsigned int (*pDSOUND_UpdateCapture) (qboolean enable, unsigned char *buffer, unsigned int minbytes, unsigned int maxbytes);
void S_TransmitVoiceChat(unsigned char clc, sizebuf_t *buf)
{
	static unsigned char capturebuf[32768];
	static unsigned int capturepos;//in bytes
	static unsigned int encsequence;//in frames
	unsigned char outbuf[1024];
	unsigned int outpos;//in bytes
	unsigned int encpos;//in bytes
	unsigned short *start;
	unsigned char initseq;//in frames
	unsigned int i;
	float micamp = cl_voip_micamp.value;

	//add new drivers in order or desirability.
	if (pDSOUND_UpdateCapture)
	{
		capturepos += pDSOUND_UpdateCapture(1, (unsigned char*)capturebuf + capturepos, 64, sizeof(capturebuf) - capturepos);
	}
	else
	{
		return;
	}

	if (!cl_voip_send.ival)
	{
		capturepos = 0;
		return;
	}

	if (!S_Speex_Init())
		return;

	initseq = encsequence;
	for (encpos = 0, outpos = 0; capturepos-encpos >= s_speex.framesize*2 && sizeof(outbuf)-outpos > 64; )
	{
		start = (short*)(capturebuf + encpos);

		qspeex_preprocess_run(s_speex.preproc, start);

		if (micamp != 1)
		{
			for (i = 0; i < s_speex.framesize; i++)
			{
				start[i] *= micamp;
			}
		}

		qspeex_bits_reset(&s_speex.encbits);
		qspeex_encode_int(s_speex.encoder, start, &s_speex.encbits);
		outbuf[outpos] = qspeex_bits_write(&s_speex.encbits, outbuf+outpos+1, sizeof(outbuf) - (outpos+1));
		outpos += 1+outbuf[outpos];
		encpos += s_speex.framesize*2;
		encsequence++;
	}

	if (outpos && buf->maxsize - buf->cursize >= outpos+4)
	{
		MSG_WriteByte(buf, clc);
		MSG_WriteByte(buf, initseq);
		MSG_WriteShort(buf, outpos);
		SZ_Write(buf, outbuf, outpos);
	}

	/*remove sent data*/
	memmove(capturebuf, capturebuf + encpos, capturepos-encpos);
	capturepos -= encpos;
}

static void S_Voip_Enable_f(void)
{
	Cvar_Set(&cl_voip_send, "1");
}
static void S_Voip_Disable_f(void)
{
	Cvar_Set(&cl_voip_send, "0");
}
static void S_Voip_f(void)
{
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
#endif


sounddriver pOPENAL_InitCard;
sounddriver pDSOUND_InitCard;
sounddriver pALSA_InitCard;
sounddriver pOSS_InitCard;
sounddriver pMacOS_InitCard;
sounddriver pSDL_InitCard;
sounddriver pWAV_InitCard;
sounddriver pAHI_InitCard;

typedef struct {
	char *name;
	sounddriver *ptr;
} sdriver_t;
sdriver_t drivers[] = {
//in order of preference
	{"OpenAL", &pOPENAL_InitCard},	//yay, get someone else to sort out sound support, woot
	{"DSound", &pDSOUND_InitCard},	//prefered on windows
	{"MacOS", &pMacOS_InitCard},	//prefered on mac
	{"AHI", &pAHI_InitCard},		//prefered on morphos

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
	if (!snd_khz.ival)
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

	if (sc->sn.numchannels < 3)
	{
		VectorSet(sc->speakerdir[0], 0, 1, 0);
		VectorSet(sc->speakerdir[1], 0, -1, 0);
	}
	else if (sc->sn.numchannels < 5)
	{
		VectorSet(sc->speakerdir[0], 0.7, -0.7, 0);
		VectorSet(sc->speakerdir[1], 0.7, 0.7, 0);
		VectorSet(sc->speakerdir[2], -0.7, -0.7, 0);
		VectorSet(sc->speakerdir[3], -0.7, 0.7, 0);
	}
	else
	{
		VectorSet(sc->speakerdir[0], 0.7, -0.7, 0);
		VectorSet(sc->speakerdir[1], 0.7, 0.7, 0);
		VectorSet(sc->speakerdir[2], 0, 0, 0);
		VectorSet(sc->speakerdir[3], 0, 0, 0);
		VectorSet(sc->speakerdir[4], -0.7, -0.7, 0);
		VectorSet(sc->speakerdir[5], -0.7, 0.7, 0);
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
}

void SNDDMA_SetUnderWater(qboolean underwater)
{
	soundcardinfo_t *sc;

	for (sc = sndcardinfo; sc; sc=sc->next)
		sc->SetWaterDistortion(sc, underwater);
}

//why isn't this part of S_Restart_f anymore?
//so that the video code can call it directly without flushing the models it's just loaded.
void S_DoRestart (void)
{
	if (nosound.ival)
		return;

	S_StopAllSounds (true);

	S_Shutdown();
	sound_started = 0;
	S_Startup();

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");

	S_StopAllSounds (true);
}

void S_Restart_f (void)
{
	Cache_Flush();//forget the old sounds.

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
				VectorSet(sc->speakerdir[i], 0, (i&1)?-1:1, 0);
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
	Cmd_AddCommand("stopsound", S_StopAllSoundsC);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	Cmd_AddCommand("snd_restart", S_Restart_f);

	Cmd_AddCommand("soundcontrol", S_Control_f);

	Cvar_Register(&nosound,				"Sound controls");
	Cvar_Register(&volume,				"Sound controls");
	Cvar_Register(&precache,			"Sound controls");
	Cvar_Register(&loadas8bit,			"Sound controls");
	Cvar_Register(&bgmvolume,			"Sound controls");
	Cvar_Register(&bgmbuffer,			"Sound controls");
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

#ifdef VOICECHAT
	Cvar_Register(&cl_voip_send,		"Voice Chat");
	Cvar_Register(&cl_voip_play,		"Voice Chat");
	Cvar_Register(&cl_voip_micamp,		"Voice Chat");
	Cmd_AddCommand("+voip", S_Voip_Enable_f);
	Cmd_AddCommand("-voip", S_Voip_Disable_f);
#endif

	Cvar_Register(&snd_inactive,		"Sound controls");

	Cvar_Register(&snd_playersoundvolume,		"Sound controls");
	Cvar_Register(&snd_usemultipledevices,		"Sound controls");

	Cvar_Register(&snd_linearresample, "Sound controls");
	Cvar_Register(&snd_linearresample_stream, "Sound controls");

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
	if (p)
	{
		if (p < com_argc-1)
			Cvar_SetValue(&snd_khz, atof(com_argv[p+1])/1000);
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
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName (char *name)
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
			return &known_sfx[i];
		}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strcpy (sfx->name, name);

	num_sfx++;

	return sfx;
}

void S_ResetFailedLoad(void)
{
	int i;
	for (i=0 ; i < num_sfx ; i++)
		known_sfx[i].failedload = false;
}


/*
==================
S_TouchSound

==================
*/
void S_TouchSound (char *name)
{
	sfx_t	*sfx;

	if (!sound_started)
		return;

	sfx = S_FindName (name);
	Cache_Check (&sfx->cache);
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
    int first_to_die;
    int life_left;

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = 0x7fffffff;
    for (ch_idx=DYNAMIC_FIRST; ch_idx < DYNAMIC_STOP ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& sc->channel[ch_idx].entnum == entnum
		&& (sc->channel[ch_idx].entchannel == entchannel || entchannel == -1) && sc->channel[ch_idx].end > snd_speed/10)
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (sc->channel[ch_idx].entnum == cl.playernum[0]+1 && entnum != cl.playernum[0]+1 && sc->channel[ch_idx].sfx)
			continue;

		if (sc->channel[ch_idx].end < life_left)
		{
			life_left = sc->channel[ch_idx].end;
			first_to_die = ch_idx;
		}
   }

	if (first_to_die == -1)
		return NULL;

	if (sc->channel[first_to_die].sfx)
		sc->channel[first_to_die].sfx = NULL;

	if (sc->total_chans <= first_to_die)
		sc->total_chans = first_to_die+1;
    return &sc->channel[first_to_die];
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

void S_StartSoundCard(soundcardinfo_t *sc, int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation, int startpos, int pitchadj)
{
	channel_t *target_chan, *check;
	sfxcache_t	*scache;
	int		vol;
	int		ch_idx;
	int		skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (nosound.ival)
		return;

#ifdef AVAIL_OPENAL
	if (sc->openal)
		OpenAL_StartSound(entnum, entchannel, sfx, origin, fvol, attenuation);
#endif

	vol = fvol*255;
	if (!pitchadj)
		pitchadj = 100;

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
	scache = S_LoadSound (sfx);
	if (!scache)
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	if (scache->length > snd_speed*20 && !ruleset_allow_overlongsounds.ival)
	{
		Con_DPrintf("Shortening over-long sound effect\n");
		startpos = scache->length - snd_speed*10;
	}
	target_chan->sfx = sfx;
	target_chan->rate = ((1<<PITCHSHIFT) * pitchadj) / 100;
	target_chan->pos = startpos*target_chan->rate;
	target_chan->end = sc->paintedtime + ((scache->length - startpos)<<PITCHSHIFT)/target_chan->rate;
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
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip*target_chan->rate;
			target_chan->end -= skip;
			break;
		}
	}
}

void S_StartSoundDelayed(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation, float timeofs)
{
	soundcardinfo_t *sc;

	if (!sfx || !*sfx->name)	//no named sounds would need specific starting.
		return;

	for (sc = sndcardinfo; sc; sc = sc->next)
		S_StartSoundCard(sc, entnum, entchannel, sfx, origin, fvol, attenuation, -(int)(timeofs * sc->sn.speed), 0);
}

void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation, int pitchadj)
{
	soundcardinfo_t *sc;

	if (!sfx || !*sfx->name)	//no named sounds would need specific starting.
		return;

	for (sc = sndcardinfo; sc; sc = sc->next)
		S_StartSoundCard(sc, entnum, entchannel, sfx, origin, fvol, attenuation, 0, pitchadj);
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

void S_StopSoundCard(soundcardinfo_t *sc, int entnum, int entchannel)
{
	int i;

	for (i=0 ; i<sc->total_chans ; i++)
	{
		if (sc->channel[i].entnum == entnum
			&& (!entchannel || sc->channel[i].entchannel == entchannel))
		{
			sc->channel[i].end = 0;
			sc->channel[i].sfx = NULL;
			if (entchannel)
				return;
		}
	}
}

void S_StopSound(int entnum, int entchannel)
{
	soundcardinfo_t *sc;
	for (sc = sndcardinfo; sc; sc = sc->next)
		S_StopSoundCard(sc, entnum, entchannel);
}

void S_StopAllSounds(qboolean clear)
{
	int		i;
	sfx_t *s;

	soundcardinfo_t *sc;

	for (sc = sndcardinfo; sc; sc = sc->next)
	{
		if (!sound_started)
			return;


		for (i=0 ; i<sc->total_chans ; i++)
			if (sc->channel[i].sfx)
			{
				s = sc->channel[i].sfx;
				sc->channel[i].sfx = NULL;
				if (s->decoder)
				if (!S_IsPlayingSomewhere(s))	//if we aint playing it elsewhere, free it compleatly.
				{
					s->decoder->abort(s);
				}
			}

		sc->total_chans = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS + NUM_MUSICS;	// no statics

		Q_memset(sc->channel, 0, MAX_CHANNELS * sizeof(channel_t));

		if (clear)
			S_ClearBuffer (sc);
	}
}

void S_StopAllSoundsC (void)
{
	S_StopAllSounds (true);
}

void S_ClearBuffer (soundcardinfo_t *sc)
{
	void *buffer;

	int		clear;

	if (!sound_started || !sc->sn.buffer)
		return;

	if (sc->sn.samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	buffer = sc->Lock(sc);
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
	sfxcache_t		*scache;
	soundcardinfo_t *scard;

	if (!sfx)
		return;

	for (scard = sndcardinfo; scard; scard = scard->next)
	{
		if (scard->total_chans == MAX_CHANNELS)
		{
			Con_Printf ("total_channels == MAX_CHANNELS\n");
			continue;
		}

		ss = &scard->channel[scard->total_chans];
		scard->total_chans++;

		scache = S_LoadSound (sfx);
		if (!scache)
			return;

		if (scache->loopstart == -1)
		{
			Con_Printf ("Ambient sound %s not looped\n", sfx->name);
			scache->loopstart = 0;
		}

		ss->sfx = sfx;
		ss->rate = 1<<PITCHSHIFT;
		VectorCopy (origin, ss->origin);
		ss->master_vol = vol;
		ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
		ss->end = scard->paintedtime + (scache->length<<PITCHSHIFT)/ss->rate;
		ss->looping = true;

		SND_Spatialize (scard, ss);
	}
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

			sc->channel[i].end = 0;
			sc->channel[i].sfx = NULL;

			if (s)
			if (s->decoder)
			if (!S_IsPlayingSomewhere(s))	//if we aint playing it elsewhere, free it compleatly.
			{
				s->decoder->abort(s);
				if (s->cache.data)
					Cache_Free(&s->cache);
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
			sc->channel[i].end += sc->sn.speed*time;

			if (sc->channel[i].pos < 0)
			{	//clamp to the start of the track
				sc->channel[i].end -= sc->channel[i].pos;
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
	float		vol;
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
			sfxcache_t *scache;
			sfx_t *newmusic;

			if (nexttrack && *nexttrack)
			{
				newmusic = S_PrecacheSound(nexttrack);


				scache = S_LoadSound (newmusic);
				if (scache)
				{
					chan->sfx = newmusic;
					chan->rate = 1<<PITCHSHIFT;
					chan->pos = 0;
					chan->end = sc->paintedtime + ((scache->length+1000)<<PITCHSHIFT)/chan->rate;
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
			sc->channel[AMBIENT_FIRST+ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		chan = &sc->channel[AMBIENT_FIRST+ambient_channel];
		chan->sfx = ambient_sfx[AMBIENT_FIRST+ambient_channel];
		chan->rate = 1<<PITCHSHIFT;

		VectorCopy(listener_origin, chan->origin);

		vol = ambient_level.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

	// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += host_frametime * ambient_fade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= host_frametime * ambient_fade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->vol[0] = chan->vol[1] = chan->vol[2] = chan->vol[3] = chan->vol[4] = chan->vol[5] = chan->master_vol;
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

void S_UpdateCard(soundcardinfo_t *sc)
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
		OpenAL_Update_Listener(listener_origin, listener_forward, listener_right, listener_up);
		return;
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
	S_Update_(sc);
}

void GetSoundtime(soundcardinfo_t *sc)
{
	int		samplepos;
	int		fullsamples;

	fullsamples = sc->sn.samples / sc->sn.numchannels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
	samplepos = sc->GetDMAPos(sc);

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

	soundtime = sc->buffers*fullsamples + samplepos/sc->sn.numchannels;
}

void S_Update (void)
{
	soundcardinfo_t *sc;

	for (sc = sndcardinfo; sc; sc = sc->next)
		S_UpdateCard(sc);
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
		S_Update_(sc);
}



void S_Update_(soundcardinfo_t *sc)
{
	unsigned        endtime;
	int				samps;

	if (sc->selfpainting)
		return;

	if ((snd_blocked > 0))
	{
		if (!sc->inactive_sound)
			return;
	}

// Updates DMA time
	GetSoundtime(sc);

// check to make sure that we haven't overshot
	if (sc->paintedtime < soundtime)
	{
		//Con_Printf ("S_Update_ : overflow\n");
		sc->paintedtime = soundtime;
	}

// mix ahead of current position
	endtime = soundtime + (int)(_snd_mixahead.value * sc->sn.speed);
//	samps = shm->samples >> (shm->numchannels-1);
	samps = sc->sn.samples / sc->sn.numchannels;
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	if (sc->Restore)
		sc->Restore(sc);

	S_PaintChannels (sc, endtime);

	sc->Submit(sc);
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play(void)
{
//	static int hash=345;
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
		S_StartSound(cl.playernum[0]+1, -1, sfx, vec3_origin, 1.0, 0.0, 0);
//		hash++;
		i++;
	}
}

void S_PlayVol(void)
{
//	static int hash=543;
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
		S_StartSound(cl.playernum[0]+1, -1, sfx, vec3_origin, vol, 0.0, 0);
//		hash;
		i+=2;
	}
}

void S_SoundList(void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		sc = Cache_Check (&sfx->cache);
		if (!sc)
			continue;
		size = sc->length*sc->width*(sc->numchannels);
		total += size;
		if (sc->loopstart >= 0)
			Con_Printf ("L");
		else
			Con_Printf (" ");
		Con_Printf("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
	}
	Con_Printf ("Total resident: %i\n", total);
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
	S_StartSound (-1, -1, sfx, vec3_origin, 1, 1, 0);
}


void S_ClearPrecache (void)
{
}


void S_BeginPrecaching (void)
{
}


void S_EndPrecaching (void)
{
}













typedef struct {
	qboolean inuse;
	int id;
	sfx_t sfx;
	sfxcache_t *sfxcache;
} streaming_t;
#define MAX_RAW_SOURCES (MAX_CLIENTS+1)
streaming_t s_streamers[MAX_RAW_SOURCES];

/*
qboolean S_IsPlayingSomewhere(sfx_t *s)
{
	soundcardinfo_t *si;
	int i;
	for (si = sndcardinfo; si; si=si->next)
	{
		for (i = 0; i < scard->total_chans; i++)
		if (si->channel[i].sfx == s)
			return true;
	}
	return false;
}*/
#undef free

void S_ClearRaw(void)
{
	memset(s_streamers, 0, sizeof(s_streamers));
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
	sfxcache_t *newcache;
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
		s->sfxcache->loopstart = -1;	//stop mixing it
		s->inuse = false;

		for (si = sndcardinfo; si; si=si->next)
		for (i = 0; i < si->total_chans; i++)
			if (si->channel[i].sfx == &s->sfx)
			{
				si->channel[i].sfx = NULL;
				break;
			}
		BZ_Free(s->sfxcache);
		return;
	}
	if (i == MAX_RAW_SOURCES)	//whoops.
	{
		if (!free)
		{
			Con_Printf("No free audio streams\n");
			return;
		}
		s = free;
		free->id = sourceid;
		free->inuse = true;
		free->sfx.cache.fake = true;
		strcpy(free->sfx.name, "");
		free->sfxcache = BZ_Malloc(sizeof(sfxcache_t));
		free->sfx.cache.data = free->sfxcache;
		free->sfxcache->speed = snd_speed;
		free->sfxcache->numchannels = channels;
		free->sfxcache->width = width;
		free->sfxcache->loopstart = -1;
		free->sfxcache->length = 0;
//		Con_Printf("Added new raw stream\n");
	}
	if (s->sfxcache->width != width || s->sfxcache->numchannels != channels || s->sfxcache->speed != snd_speed)
	{
		s->sfxcache->width = width;
		s->sfxcache->numchannels = channels;
		s->sfxcache->speed = snd_speed;
		s->sfxcache->length = 0;
//		Con_Printf("Restarting raw stream\n");
	}

	speedfactor	= (double)speed/snd_speed;
	outsamples = samples/speedfactor;

	oldlength = s->sfxcache->length;

	prepadl = 0x7fffffff;
	for (si = sndcardinfo; si; si=si->next)	//make sure all cards are playing, and that we still get a prepad if just one is.
	{
		for (i = 0; i < si->total_chans; i++)
			if (si->channel[i].sfx == &s->sfx)
			{
				if (prepadl > (si->channel[i].pos>>8))
					prepadl = (si->channel[i].pos>>8);
				break;
			}
	}

	if (prepadl == 0x7fffffff)
	{
		if (snd_show.ival)
			Con_Printf("Wasn't playing\n");
		prepadl = 0;
		spare = s->sfxcache->length;
		if (spare > snd_speed)
		{
			Con_DPrintf("Sacrificed raw sound stream\n");
			spare = 0;	//too far out. sacrifice it all
		}
	}
	else
	{
		spare = s->sfxcache->length - prepadl;
		if (spare < 0)	//remaining samples since last time
			spare = 0;

		if (s->sfxcache->length > snd_speed*2) // more than 2 seconds of sound
		{
			Con_DPrintf("Sacrificed raw sound stream\n");
			spare = 0;	//too far out. sacrifice it all
		}
	}

	newcache = BZ_Malloc(sizeof(sfxcache_t) + (spare+outsamples) * (s->sfxcache->numchannels) * s->sfxcache->width);
	memcpy(newcache, s->sfxcache, sizeof(sfxcache_t));
	memcpy(newcache->data, s->sfxcache->data + prepadl * (s->sfxcache->numchannels) * s->sfxcache->width, spare * (s->sfxcache->numchannels) * s->sfxcache->width);

	BZ_Free(s->sfxcache);
	s->sfxcache = s->sfx.cache.data = newcache;

	newcache->length = spare + outsamples;

	{
		extern cvar_t snd_linearresample_stream;
		short *outpos = (short *)(newcache->data + spare * (s->sfxcache->numchannels) * s->sfxcache->width);
		SND_ResampleStream(data, 
			speed, 
			width, 
			channels,
			samples,
			outpos, 
			snd_speed, 
			s->sfxcache->width, 
			s->sfxcache->numchannels, 
			snd_linearresample_stream.ival);
	}

	s->sfxcache->loopstart = s->sfxcache->length;

	for (si = sndcardinfo; si; si=si->next)
	{
		for (i = 0; i < si->total_chans; i++)
			if (si->channel[i].sfx == &s->sfx)
			{
				si->channel[i].pos -= prepadl*si->channel[i].rate;
//				si->channel[i].end -= prepadl;
				si->channel[i].end += outsamples;

				if (si->channel[i].end < si->paintedtime)
				{
					si->channel[i].pos = 0;
					si->channel[i].end = si->paintedtime + s->sfxcache->length;
				}
				break;
			}
		if (i == si->total_chans)	//this one wasn't playing.
		{
			S_StartSoundCard(si, -1, 0, &s->sfx, r_origin, 1, 32767, 500, 0);
//			Con_Printf("Restarted\n");
		}
	}

//	Con_Printf("Stripped %i, added %i (length %i)\n", prepadl, samples, s->sfxcache->length);
}
