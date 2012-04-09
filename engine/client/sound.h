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
// sound.h -- client sound i/o functions

#ifndef __SOUND__
#define __SOUND__

// !!! if this is changed, it much be changed in asm_i386.h too !!!
#define MAXSOUNDCHANNELS 8	//on a per device basis

// !!! if this is changed, it much be changed in asm_i386.h too !!!
struct sfx_s;
/*typedef struct
{
	int left;
	int right;
} portable_samplepair_t;
*/
typedef struct
{
	int s[MAXSOUNDCHANNELS];
} portable_samplegroup_t;

typedef struct {
	int decodedlen;
	struct sfxcache_s *(*decodedata) (struct sfx_s *sfx, struct sfxcache_s *buf, int start, int length);	//retrurn true when done.
	void (*abort) (struct sfx_s *sfx);	//it's not playing elsewhere. free entirly
	void *buf;
} sfxdecode_t;

typedef struct sfx_s
{
	char 	name[MAX_OSPATH];
#ifdef AVAIL_OPENAL
	unsigned int	openal_buffer;
#endif
	qboolean failedload:1; //no more super-spammy
	qboolean touched:1; //if the sound is still relevent
	sfxdecode_t		decoder;
} sfx_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct sfxcache_s
{
	unsigned int length;
	unsigned int loopstart;
	unsigned int speed;
	unsigned int width;
	unsigned int numchannels;
	unsigned int soundoffset;	//byte index into the sound
	qbyte	*data;		// variable sized
} sfxcache_t;

typedef struct
{
//	qboolean		gamealive;
//	qboolean		soundalive;
//	qboolean		splitbuffer;
	int				numchannels;			// this many samples per frame
	int				samples;				// mono samples in buffer (individual, non grouped)
//	int				submission_chunk;		// don't mix less than this #
	int				samplepos;				// in mono samples
	int				samplebits;
	int				speed;					// this many frames per second
	unsigned char	*buffer;				// pointer to mixed pcm buffer (not directly used by mixer)
} dma_t;

#define PITCHSHIFT 6	/*max audio file length = (1<<32>>PITCHSHIFT)/KHZ*/

typedef struct
{
	sfx_t	*sfx;			// sfx number
	int		vol[MAXSOUNDCHANNELS];		// 0-255 volume
//	int		delay[MAXSOUNDCHANNELS];
	int		start;			// start time in global paintsamples
	int 	pos;			// sample position in sfx, <0 means delay sound start (shifted up by 8)
	int     rate;			// 24.8 fixed point rate scaling
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;		//int audio_fd
	vec3_t	origin;			// origin of sound effect
	vec_t	dist_mult;		// distance multiplier (attenuation/clipK)
	int		master_vol;		// 0-255 master volume
} channel_t;

typedef struct
{
	int		rate;
	int		width;
	int		numchannels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

struct soundcardinfo_s;
typedef struct soundcardinfo_s soundcardinfo_t;

void S_Init (void);
void S_Startup (void);
void S_Shutdown (void);
void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation, float timeofs, float pitchadj);
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_StopSound (int entnum, int entchannel);
void S_StopAllSounds(qboolean clear);
void S_UpdateListener(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up);
void S_GetListenerInfo(float *origin, float *forward, float *right, float *up);
void S_Update (void);
void S_ExtraUpdate (void);
void S_MixerThread(soundcardinfo_t *sc);
void S_Purge(qboolean retaintouched);

qboolean S_HaveOutput(void);

void S_Music_Clear(sfx_t *onlyifsample);
void S_Music_Seek(float time);

sfx_t *S_PrecacheSound (char *sample);
void S_TouchSound (char *sample);
void S_UntouchAll(void);
void S_ClearPrecache (void);
void S_BeginPrecaching (void);
void S_EndPrecaching (void);

void S_PaintChannels(soundcardinfo_t *sc, int endtime);
void S_InitPaintChannels (soundcardinfo_t *sc);

void S_ShutdownCard (soundcardinfo_t *sc);

void S_DefaultSpeakerConfiguration(soundcardinfo_t *sc);
void S_ResetFailedLoad(void);

#ifdef PEXT2_VOICECHAT
void S_Voip_Parse(void);
#endif
#ifdef VOICECHAT
extern cvar_t cl_voip_showmeter;
void S_Voip_Transmit(unsigned char clc, sizebuf_t *buf);
void S_Voip_MapChange(void);
int S_Voip_Loudness(qboolean ignorevad);	//-1 for not capturing, otherwise between 0 and 100
qboolean S_Voip_Speaking(unsigned int plno);
void S_Voip_Ignore(unsigned int plno, qboolean ignore);
#else
#define S_Voip_Loudness() -1
#define S_Voip_Speaking(p) false
#define S_Voip_Ignore(p,s)
#endif

qboolean S_IsPlayingSomewhere(sfx_t *s);
qboolean ResampleSfx (sfx_t *sfx, int inrate, int inchannels, int inwidth, int insamps, int inloopstart, qbyte *data);

// picks a channel based on priorities, empty slots, number of channels
channel_t *SND_PickChannel(soundcardinfo_t *sc, int entnum, int entchannel);

// spatializes a channel
void SND_Spatialize(soundcardinfo_t *sc, channel_t *ch);

void SND_ResampleStream (void *in, int inrate, int inwidth, int inchannels, int insamps, void *out, int outrate, int outwidth, int outchannels, int resampstyle);

// restart entire sound subsystem (doesn't flush old sounds, so make sure that happens)
void S_DoRestart (void);

void S_Restart_f (void);

//plays streaming audio
void S_RawAudio(int sourceid, qbyte *data, int speed, int samples, int channels, int width);

void CLVC_Poll (void);

void SNDVC_MicInput(qbyte *buffer, int samples, int freq, int width);



#ifdef AVAIL_OPENAL
void OpenAL_LoadCache(sfx_t *s, sfxcache_t *sc);
void OpenAL_StartSound(int entnum, int entchannel, sfx_t * sfx, vec3_t origin, float fvol, float attenuation, float pitchscale);
void OpenAL_Update_Listener(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t velocity);
void OpenAL_CvarInit(void);
#endif


// ====================================================================
// User-setable variables
// ====================================================================

#define	MAX_CHANNELS			1024/*tracked sounds (including statics)*/
#define	MAX_DYNAMIC_CHANNELS	64	/*playing sounds (identical ones merge)*/


#define NUM_MUSICS				1

#define AMBIENT_FIRST 0
#define AMBIENT_STOP NUM_AMBIENTS
#define MUSIC_FIRST AMBIENT_STOP
#define MUSIC_STOP (MUSIC_FIRST + NUM_MUSICS)
#define DYNAMIC_FIRST MUSIC_STOP
#define DYNAMIC_STOP (DYNAMIC_FIRST + MAX_DYNAMIC_CHANNELS)

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

extern int				snd_speed;

extern vec3_t listener_origin;
extern vec3_t listener_forward;
extern vec3_t listener_right;
extern vec3_t listener_up;
extern vec_t sound_nominal_clip_dist;

extern	cvar_t loadas8bit;
extern	cvar_t bgmvolume;
extern	cvar_t volume;
extern	cvar_t snd_capture;

extern float voicevolumemod;

extern qboolean	snd_initialized;
extern cvar_t snd_usemultipledevices;

extern int		snd_blocked;

void S_LocalSound (char *s);
qboolean S_LoadSound (sfx_t *s);

typedef qboolean (*S_LoadSound_t) (sfx_t *s, qbyte *data, int datalen, int sndspeed);
qboolean S_RegisterSoundInputPlugin(S_LoadSound_t loadfnc);	//called to register additional sound input plugins

wavinfo_t GetWavinfo (char *name, qbyte *wav, int wavlength);

void S_AmbientOff (void);
void S_AmbientOn (void);


//inititalisation functions.
typedef int (*sounddriver) (soundcardinfo_t *sc, int cardnum);
extern sounddriver pOPENAL_InitCard;
extern sounddriver pDSOUND_InitCard;
extern sounddriver pALSA_InitCard;
extern sounddriver pOSS_InitCard;
extern sounddriver pSDL_InitCard;
extern sounddriver pWAV_InitCard;
extern sounddriver pAHI_InitCard;

struct soundcardinfo_s { //windows has one defined AFTER directsound
	char name[256];	//a description of the card.
	struct soundcardinfo_s *next;

//speaker orientations for spacialisation.
	float dist[MAXSOUNDCHANNELS];

	vec3_t speakerdir[MAXSOUNDCHANNELS];

//info on which sound effects are playing
	channel_t   channel[MAX_CHANNELS];
	int			total_chans;

//mixer
	volatile dma_t sn;	//why is this volatile?
	qboolean inactive_sound;	//continue mixing for this card even when the window isn't active.
	qboolean selfpainting;	//allow the sound code to call the right functions when it feels the need (not properly supported).

	int	paintedtime;	//used in the mixer as last-written pos (in frames)
	int	oldsamplepos;	//this is used to track buffer wraps
	int	buffers;	//used to keep track of how many buffer wraps for consistant sound
	int	samplequeue;	//this is the number of samples the device can enqueue. if set, DMAPos returns the write point (rather than hardware read point) (in samplepairs).

//callbacks
	void *(*Lock) (soundcardinfo_t *sc, unsigned int *startoffset);
	void (*Unlock) (soundcardinfo_t *sc, void *buffer);
	void (*Submit) (soundcardinfo_t *sc, int start, int end);
	void (*Shutdown) (soundcardinfo_t *sc);
	unsigned int (*GetDMAPos) (soundcardinfo_t *sc);
	void (*SetWaterDistortion) (soundcardinfo_t *sc, qboolean underwater);
	void (*Restore) (soundcardinfo_t *sc);
	void (*ChannelUpdate) (soundcardinfo_t *sc, channel_t *channel, unsigned int schanged);

//driver -specific
	void *thread;
	void *handle;
	int snd_sent;
	int snd_completed;
	int audio_fd;

// no clue how else to handle this yet!
#ifdef AVAIL_OPENAL
	int openal;
#endif
};

extern soundcardinfo_t *sndcardinfo;

typedef struct
{
	void *(*Init) (int samplerate);			/*create a new context*/
	void (*Start) (void *ctx);		/*begin grabbing new data, old data is potentially flushed*/
	unsigned int (*Update) (void *ctx, unsigned char *buffer, unsigned int minbytes, unsigned int maxbytes);	/*grab the data into a different buffer*/
	void (*Stop) (void *ctx);		/*stop grabbing new data, old data may remain*/
	void (*Shutdown) (void *ctx);	/*destroy everything*/
} snd_capture_driver_t;

#endif
