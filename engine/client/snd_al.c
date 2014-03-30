#include "quakedef.h"

/*
This is based on Jogi's OpenAL support.
Much of it is stripped, to try and get it clean/compliant.

Missing features:
FIXME: listener velocity calculations (currently ugly).
FIXME: does not track entity velocities, so no dopler (awkward, quake doesn't move playing sounds at all).
FIXME: no eax / efx (underwater reverb etc).
FIXME: a capture device would be useful (voice chat).
*/

#ifdef AVAIL_OPENAL

#if defined(_WIN32)
 #define AL_APIENTRY __cdecl
#else
 #define AL_APIENTRY
#endif
#define AL_API


typedef int ALint;
typedef unsigned int ALuint;
typedef float ALfloat;
typedef int ALenum;
typedef char ALchar;
typedef char ALboolean;
typedef int ALsizei;
typedef void ALvoid;

static dllhandle_t *openallib;
static qboolean openallib_tried;
static AL_API ALenum (AL_APIENTRY *palGetError)( void );
static AL_API void (AL_APIENTRY *palSourcef)( ALuint sid, ALenum param, ALfloat value ); 
static AL_API void (AL_APIENTRY *palSourcei)( ALuint sid, ALenum param, ALint value ); 

static AL_API void (AL_APIENTRY *palSourcePlayv)( ALsizei ns, const ALuint *sids );
static AL_API void (AL_APIENTRY *palSourceStopv)( ALsizei ns, const ALuint *sids );
static AL_API void (AL_APIENTRY *palSourcePlay)( ALuint sid );
static AL_API void (AL_APIENTRY *palSourceStop)( ALuint sid );

static AL_API void (AL_APIENTRY *palDopplerFactor)( ALfloat value );

static AL_API void (AL_APIENTRY *palGenBuffers)( ALsizei n, ALuint* buffers );
static AL_API ALboolean (AL_APIENTRY *palIsBuffer)( ALuint bid );
static AL_API void (AL_APIENTRY *palBufferData)( ALuint bid, ALenum format, const ALvoid* data, ALsizei size, ALsizei freq );
static AL_API void (AL_APIENTRY *palDeleteBuffers)( ALsizei n, const ALuint* buffers );

static AL_API void (AL_APIENTRY *palListenerfv)( ALenum param, const ALfloat* values ); 
static AL_API void (AL_APIENTRY *palSourcefv)( ALuint sid, ALenum param, const ALfloat* values ); 
static AL_API const ALchar* (AL_APIENTRY *palGetString)( ALenum param );
static AL_API void (AL_APIENTRY *palGenSources)( ALsizei n, ALuint* sources ); 
static AL_API void (AL_APIENTRY *palListenerf)( ALenum param, ALfloat value );
static AL_API void (AL_APIENTRY *palDeleteBuffers)( ALsizei n, const ALuint* buffers );
static AL_API void (AL_APIENTRY *palDeleteSources)( ALsizei n, const ALuint* sources );
static AL_API void (AL_APIENTRY *palSpeedOfSound)( ALfloat value );
static AL_API void (AL_APIENTRY *palDistanceModel)( ALenum distanceModel );

//needed for streaming
static AL_API void (AL_APIENTRY *palGetSourcei)(ALuint source, ALenum pname, ALint *value);
static AL_API void (AL_APIENTRY *palSourceQueueBuffers)(ALuint source, ALsizei n, ALuint* buffers);
static AL_API void (AL_APIENTRY *palSourceUnqueueBuffers)(ALuint source, ALsizei n, ALuint* buffers);

#define AL_NONE                                   0
#define AL_FALSE                                  0
#define AL_TRUE                                   1
#define AL_SOURCE_RELATIVE                        0x202
#define AL_PITCH                                  0x1003
#define AL_POSITION                               0x1004
#define AL_VELOCITY                               0x1006
#define AL_LOOPING                                0x1007
#define AL_BUFFER                                 0x1009
#define AL_GAIN                                   0x100A
#define AL_ORIENTATION                            0x100F
#define	AL_SOURCE_STATE							  0x1010
#define	AL_PLAYING								  0x1012	
#define	AL_BUFFERS_PROCESSED					  0x1016
#define AL_REFERENCE_DISTANCE                     0x1020
#define AL_ROLLOFF_FACTOR                         0x1021
#define AL_MAX_DISTANCE                           0x1023
#define	AL_SOURCE_TYPE							  0x1027
#define	AL_STREAMING							  0x1029
#define AL_FORMAT_MONO8                           0x1100
#define AL_FORMAT_MONO16                          0x1101
#define AL_FORMAT_STEREO8                         0x1102
#define AL_FORMAT_STEREO16                        0x1103
#define AL_INVALID_NAME                           0xA001
#define AL_INVALID_ENUM                           0xA002
#define AL_INVALID_VALUE                          0xA003
#define AL_INVALID_OPERATION                      0xA004
#define AL_OUT_OF_MEMORY                          0xA005
#define AL_VENDOR                                 0xB001
#define AL_VERSION                                0xB002
#define AL_RENDERER                               0xB003
#define AL_EXTENSIONS                             0xB004
#define AL_DISTANCE_MODEL                         0xD000
#define AL_INVERSE_DISTANCE                       0xD001
#define AL_INVERSE_DISTANCE_CLAMPED               0xD002
#define AL_LINEAR_DISTANCE                        0xD003
#define AL_LINEAR_DISTANCE_CLAMPED                0xD004
#define AL_EXPONENT_DISTANCE                      0xD005
#define AL_EXPONENT_DISTANCE_CLAMPED              0xD006




#if defined(_WIN32)
 #define ALC_APIENTRY __cdecl
#else
 #define ALC_APIENTRY
#endif
#define ALC_API

typedef char ALCboolean;
typedef char ALCchar;
typedef int ALCint;
typedef unsigned int ALCuint;
typedef int ALCenum;
typedef size_t ALCsizei;
typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;
typedef void ALCvoid;

static ALC_API ALCdevice *     (ALC_APIENTRY *palcOpenDevice)( const ALCchar *devicename );
static ALC_API ALCboolean      (ALC_APIENTRY *palcCloseDevice)( ALCdevice *device );

static ALC_API ALCcontext *    (ALC_APIENTRY *palcCreateContext)( ALCdevice *device, const ALCint* attrlist );
static ALC_API void            (ALC_APIENTRY *palcDestroyContext)( ALCcontext *context );
static ALC_API ALCboolean      (ALC_APIENTRY *palcMakeContextCurrent)( ALCcontext *context );
static ALC_API void            (ALC_APIENTRY *palcProcessContext)( ALCcontext *context );

static ALC_API const ALCchar * (ALC_APIENTRY *palcGetString)( ALCdevice *device, ALCenum param );
static ALC_API ALCboolean      (ALC_APIENTRY *palcIsExtensionPresent)( ALCdevice *device, const ALCchar *extname );

#define ALC_DEFAULT_DEVICE_SPECIFIER             0x1004
#define ALC_DEVICE_SPECIFIER                     0x1005
#define ALC_EXTENSIONS                           0x1006
#define ALC_ALL_DEVICES_SPECIFIER				 0x1013	//ALC_ENUMERATE_ALL_EXT

//#include "AL/alut.h"
//#include "AL/al.h"
//#include "AL/alext.h"

//efx
#define AL_AUXILIARY_SEND_FILTER                 0x20006
#define AL_FILTER_NULL                           0x0000
#define AL_EFFECTSLOT_EFFECT                     0x0001
#define AL_EFFECT_TYPE                           0x8001
#define AL_EFFECT_REVERB                         0x0001
#define AL_EFFECT_EAXREVERB                      0x8000

#define AL_REVERB_DENSITY                        0x0001
#define AL_REVERB_DIFFUSION                      0x0002
#define AL_REVERB_GAIN                           0x0003
#define AL_REVERB_GAINHF                         0x0004
#define AL_REVERB_DECAY_TIME                     0x0005
#define AL_REVERB_DECAY_HFRATIO                  0x0006
#define AL_REVERB_REFLECTIONS_GAIN               0x0007
#define AL_REVERB_REFLECTIONS_DELAY              0x0008
#define AL_REVERB_LATE_REVERB_GAIN               0x0009
#define AL_REVERB_LATE_REVERB_DELAY              0x000A
#define AL_REVERB_AIR_ABSORPTION_GAINHF          0x000B
#define AL_REVERB_ROOM_ROLLOFF_FACTOR            0x000C
#define AL_REVERB_DECAY_HFLIMIT                  0x000D

/* EAX Reverb effect parameters */
#define AL_EAXREVERB_DENSITY                     0x0001
#define AL_EAXREVERB_DIFFUSION                   0x0002
#define AL_EAXREVERB_GAIN                        0x0003
#define AL_EAXREVERB_GAINHF                      0x0004
#define AL_EAXREVERB_GAINLF                      0x0005
#define AL_EAXREVERB_DECAY_TIME                  0x0006
#define AL_EAXREVERB_DECAY_HFRATIO               0x0007
#define AL_EAXREVERB_DECAY_LFRATIO               0x0008
#define AL_EAXREVERB_REFLECTIONS_GAIN            0x0009
#define AL_EAXREVERB_REFLECTIONS_DELAY           0x000A
#define AL_EAXREVERB_REFLECTIONS_PAN             0x000B
#define AL_EAXREVERB_LATE_REVERB_GAIN            0x000C
#define AL_EAXREVERB_LATE_REVERB_DELAY           0x000D
#define AL_EAXREVERB_LATE_REVERB_PAN             0x000E
#define AL_EAXREVERB_ECHO_TIME                   0x000F
#define AL_EAXREVERB_ECHO_DEPTH                  0x0010
#define AL_EAXREVERB_MODULATION_TIME             0x0011
#define AL_EAXREVERB_MODULATION_DEPTH            0x0012
#define AL_EAXREVERB_AIR_ABSORPTION_GAINHF       0x0013
#define AL_EAXREVERB_HFREFERENCE                 0x0014
#define AL_EAXREVERB_LFREFERENCE                 0x0015
#define AL_EAXREVERB_ROOM_ROLLOFF_FACTOR         0x0016
#define AL_EAXREVERB_DECAY_HFLIMIT               0x0017
static AL_API void*(AL_APIENTRY *palGetProcAddress)(const ALchar *fname);
static AL_API void (AL_APIENTRY *palSource3i)(ALuint source, ALenum param, ALint value1, ALint value2, ALint value3);

static AL_API void (AL_APIENTRY *palAuxiliaryEffectSloti)(ALuint effectslot, ALenum param, ALint iValue);
static AL_API ALvoid (AL_APIENTRY *palGenAuxiliaryEffectSlots)(ALsizei n, ALuint *effectslots);
static AL_API ALvoid (AL_APIENTRY *palDeleteAuxiliaryEffectSlots)(ALsizei n, const ALuint *effectslots);
static AL_API ALvoid (AL_APIENTRY *palDeleteEffects)(ALsizei n, const ALuint *effects);

static AL_API ALvoid (AL_APIENTRY *palGenEffects)(ALsizei n, ALuint *effects);
static AL_API ALvoid (AL_APIENTRY *palEffecti)(ALuint effect, ALenum param, ALint iValue);
static AL_API ALvoid (AL_APIENTRY *palEffectiv)(ALuint effect, ALenum param, const ALint *piValues);
static AL_API ALvoid (AL_APIENTRY *palEffectf)(ALuint effect, ALenum param, ALfloat flValue);
static AL_API ALvoid (AL_APIENTRY *palEffectfv)(ALuint effect, ALenum param, const ALfloat *pflValues);

//capture-specific stuff
static ALC_API void           (ALC_APIENTRY *palcGetIntegerv)( ALCdevice *device, ALCenum param, ALCsizei size, ALCint *data );
static ALC_API ALCdevice *    (ALC_APIENTRY *palcCaptureOpenDevice)( const ALCchar *devicename, ALCuint frequency, ALCenum format, ALCsizei buffersize );
static ALC_API ALCboolean     (ALC_APIENTRY *palcCaptureCloseDevice)( ALCdevice *device );
static ALC_API void           (ALC_APIENTRY *palcCaptureStart)( ALCdevice *device );
static ALC_API void           (ALC_APIENTRY *palcCaptureStop)( ALCdevice *device );
static ALC_API void           (ALC_APIENTRY *palcCaptureSamples)( ALCdevice *device, ALCvoid *buffer, ALCsizei samples );
#define ALC_CAPTURE_DEVICE_SPECIFIER             0x310
#define ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER     0x311
#define ALC_CAPTURE_SAMPLES                      0x312




#define SOUNDVARS "OpenAL variables"


extern sfx_t *known_sfx;
extern int loaded_sfx;
extern int num_sfx;


static void OnChangeALMaxDistance (cvar_t *var, char *value);
static void OnChangeALSpeedOfSound (cvar_t *var, char *value);
static void OnChangeALDopplerFactor (cvar_t *var, char *value);
static void OnChangeALDistanceModel (cvar_t *var, char *value);
/*
static void S_Init_f(void);
static void S_Info(void);

static void S_Shutdown_f(void);
*/
static cvar_t s_al_debug = CVAR("s_al_debug", "0");
static cvar_t s_al_max_distance = CVARFC("s_al_max_distance", "1000",0,OnChangeALMaxDistance);
static cvar_t s_al_speedofsound = CVARFC("s_al_speedofsound", "343.3",0,OnChangeALSpeedOfSound);
static cvar_t s_al_dopplerfactor = CVARFC("s_al_dopplerfactor", "3.0",0,OnChangeALDopplerFactor);
static cvar_t s_al_distancemodel = CVARFC("s_al_distancemodel", "2",0,OnChangeALDistanceModel);
static cvar_t s_al_rolloff_factor = CVAR("s_al_rolloff_factor", "1");
static cvar_t s_al_reference_distance = CVAR("s_al_reference_distance", "120");
static cvar_t s_al_velocityscale = CVAR("s_al_velocityscale", "1");
static cvar_t s_al_static_listener = CVAR("s_al_static_listener", "0");	//cheat

typedef struct
{
	#define NUM_SOURCES MAX_CHANNELS
	ALuint source[NUM_SOURCES];

	ALCdevice *OpenAL_Device;
	ALCcontext *OpenAL_Context;

	ALfloat ListenPos[3];// = { 0.0, 0.0, 0.0 };

	// Velocity of the listener.
	ALfloat ListenVel[3];// = { 0.0, 0.0, 0.0 };

	// Orientation of the listener. (first 3 elements are "at", second 3 are "up")
	ALfloat ListenOri[6];// = { 0.0, 0.0, -1.0, 0.0, 1.0, 0.0 };

	int cureffect;
	ALuint effectslot;			//the global reverb slot
	ALuint effecttype[2];	//effect used when underwater
} oalinfo_t;
static void PrintALError(char *string)
{
	ALenum err;
	char *text = NULL;
	if (!s_al_debug.value)
		return;
	err = palGetError();
	switch(err)
	{
	case 0:
		return;
	case AL_INVALID_NAME:
		text = "invalid name";
		break;
	case AL_INVALID_ENUM:
		text = "invalid enum";
		break;
	case AL_INVALID_VALUE:
		text = "invalid value";
		break;
	case AL_INVALID_OPERATION:
		text = "invalid operation";
		break;
	case AL_OUT_OF_MEMORY:
		text = "out of memory";
		break;
	default:
		text = "unknown";
		break;
	}
	Con_Printf("OpenAL - %s: %x: %s\n",string,err,text);
}

void OpenAL_LoadCache(unsigned int *bufptr, sfxcache_t *sc, float volume)
{
	unsigned int fmt;
	unsigned int size;
	switch(sc->width)
	{
	case 1:
		if (sc->numchannels == 2)
		{
			fmt = AL_FORMAT_STEREO8;
			size = sc->length*2;
		}
		else
		{
			fmt = AL_FORMAT_MONO8;
			size = sc->length*1;
		}
		break;
	case 2:
		if (sc->numchannels == 2)
		{
			fmt = AL_FORMAT_STEREO16;
			size = sc->length*4;
		}
		else
		{
			fmt = AL_FORMAT_MONO16;
			size = sc->length*2;
		}
		break;
	default:
		return;
	}
	PrintALError("pre Buffer Data");
	palGenBuffers(1, bufptr);
	/*openal is inconsistant and supports only 8bit unsigned or 16bit signed*/
	if (volume != 1)
	{
		if (sc->width == 1)
		{
			unsigned char *tmp = malloc(size);
			char *src = sc->data;
			int i;
			for (i = 0; i < size; i++)
			{
				tmp[i] = src[i]*volume+128;	//signed->unsigned
			}
			palBufferData(*bufptr, fmt, tmp, size, sc->speed);
			free(tmp);
		}
		else
		{
			short *tmp = malloc(size);
			short *src = (short*)sc->data;
			int i;
			for (i = 0; i < (size>>1); i++)
			{
				tmp[i] = bound(-32767, src[i]*volume, 32767);	//signed.
			}
			palBufferData(*bufptr, fmt, tmp, size, sc->speed);
			free(tmp);
		}
	}
	else
	{
		if (sc->width == 1)
		{
			unsigned char *tmp = malloc(size);
			char *src = sc->data;
			int i;
			for (i = 0; i < size; i++)
			{
				tmp[i] = src[i]+128;
			}
			palBufferData(*bufptr, fmt, tmp, size, sc->speed);
			free(tmp);
		}
		else
			palBufferData(*bufptr, fmt, sc->data, size, sc->speed);
	}

	//FIXME: we need to handle oal-oom error codes

	PrintALError("Buffer Data");
}

void OpenAL_CvarInit(void)
{
	Cvar_Register(&s_al_debug, SOUNDVARS);
	Cvar_Register(&s_al_max_distance, SOUNDVARS);
	Cvar_Register(&s_al_dopplerfactor, SOUNDVARS);
	Cvar_Register(&s_al_distancemodel, SOUNDVARS);
	Cvar_Register(&s_al_reference_distance, SOUNDVARS);
	Cvar_Register(&s_al_rolloff_factor, SOUNDVARS);
	Cvar_Register(&s_al_velocityscale, SOUNDVARS);
	Cvar_Register(&s_al_static_listener, SOUNDVARS);
	Cvar_Register(&s_al_speedofsound, SOUNDVARS);
}

extern float voicevolumemod;
static void OpenAL_ListenerUpdate(soundcardinfo_t *sc, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t velocity)
{
	oalinfo_t *oali = sc->handle;

	VectorScale(velocity, s_al_velocityscale.value, oali->ListenVel);
	VectorCopy(origin, oali->ListenPos);

	oali->ListenOri[0] = forward[0];
	oali->ListenOri[1] = forward[1];
	oali->ListenOri[2] = forward[2];
	oali->ListenOri[3] = up[0];
	oali->ListenOri[4] = up[1];
	oali->ListenOri[5] = up[2];


	if (!s_al_static_listener.value)
	{
		palListenerf(AL_GAIN, 1);
		palListenerfv(AL_POSITION, oali->ListenPos);
		palListenerfv(AL_VELOCITY, oali->ListenVel);
		palListenerfv(AL_ORIENTATION, oali->ListenOri);
	}
}

//schanged says the sample has changed, otherwise its merely moved around a little, maybe changed in volume, but nothing that will restart it.
static void OpenAL_ChannelUpdate(soundcardinfo_t *sc, channel_t *chan, unsigned int schanged)
{
	oalinfo_t *oali = sc->handle;
	ALuint src;
	sfx_t *sfx = chan->sfx;
	float pitch, cvolume;
	int chnum = chan - sc->channel;
	ALuint buf;

	if (chnum >= NUM_SOURCES)
		return;

	src = oali->source[chnum];
	if (!src)
	{
		//not currently playing. be prepared to create one
		if (!sfx || chan->master_vol == 0)
			return;
		palGenSources(1, &src);
		//unable to start a new sound source, give up.
		if (!src)
		{
			//FIXME: find one which has already stopped and steal it.
			Con_Printf("Out of OpenAL sources!\n");
			return;
		}
		oali->source[chnum] = src;
		schanged = true;	//should normally be true anyway, but hey
	}

	PrintALError("pre start sound");

	if (schanged && src)
		palSourceStop(src);

	//reclaim any queued buffers
	palGetSourcei(src, AL_SOURCE_TYPE, &buf);
	if (buf == AL_STREAMING)
	{
		for(;;)
		{
			palGetSourcei(src, AL_BUFFERS_PROCESSED, &buf);
			if (!buf)
				break;
			palSourceUnqueueBuffers(src, 1, &buf);
			palDeleteBuffers(1, &buf);
		}
	}

	/*just wanted to stop it?*/
	if (!sfx || chan->master_vol == 0)
	{
		if (src)
		{
			palDeleteSources(1, &src);
			oali->source[chnum] = 0;
		}
		return;
	}

	cvolume = chan->master_vol/255.0f;
	if (!(chan->flags & CF_ABSVOLUME))
		cvolume *= volume.value*voicevolumemod;

	if (schanged || sfx->decoder.decodedata)
	{
		if (!sfx->openal_buffer)
		{
			if (!S_LoadSound(sfx))
				return;	//can't load it
			if (sfx->decoder.decodedata)
			{
				int offset;
				sfxcache_t sbuf, *sc = sfx->decoder.decodedata(sfx, &sbuf, chan->pos>>PITCHSHIFT, 65536);
				memcpy(&sbuf, sc, sizeof(sbuf));

				//hack up the sound to offset it correctly
				offset = (chan->pos>>PITCHSHIFT) - sbuf.soundoffset;
				sbuf.data += offset * sc->width*sc->numchannels;
				sbuf.length -= offset;
				sbuf.soundoffset = 0;

				//build a buffer with it and queue it up.
				//buffer will be purged later on when its unqueued
				OpenAL_LoadCache(&buf, &sbuf, max(1,cvolume));
				palSourceQueueBuffers(src, 1, &buf);

				//yay
				chan->pos += sbuf.length<<PITCHSHIFT;

				palGetSourcei(src, AL_SOURCE_STATE, &buf);
				if (buf != AL_PLAYING)
					schanged = true;
			}
			else
			{
				OpenAL_LoadCache(&sfx->openal_buffer, sfx->decoder.buf, 1);
				palSourcei(src, AL_BUFFER, sfx->openal_buffer);
			}
		}
		else
			palSourcei(src, AL_BUFFER, sfx->openal_buffer);
	}

	palSourcef(src, AL_GAIN, min(cvolume, 1));	//openal only supports a max volume of 1. anything above is an error and will be clamped.
	if (chan->entnum == -1 || chan->entnum == cl.playerview[0].viewentity)
		palSourcefv(src, AL_POSITION, vec3_origin);
	else
		palSourcefv(src, AL_POSITION, chan->origin);
	palSourcefv(src, AL_VELOCITY, vec3_origin);

	if (schanged)
	{
		pitch = (float)chan->rate/(1<<PITCHSHIFT);
		palSourcef(src, AL_PITCH, pitch);

		if (chan->entnum == -2)	//don't do the underwater thing on static sounds. it sounds like arse with all those sources.
			palSource3i(src, AL_AUXILIARY_SEND_FILTER, 0, 0, AL_FILTER_NULL);
		else
			palSource3i(src, AL_AUXILIARY_SEND_FILTER, oali->effectslot, 0, AL_FILTER_NULL);

		palSourcei(src, AL_LOOPING, chan->looping?AL_TRUE:AL_FALSE);
		if (chan->entnum == -1 || chan->entnum == cl.playerview[0].viewentity)
		{
			palSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
//			palSourcef(src, AL_ROLLOFF_FACTOR, 0.0f);
		}
		else
		{
			palSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
//			palSourcef(src, AL_ROLLOFF_FACTOR, s_al_rolloff_factor.value*chan->dist_mult);
		}
		switch(s_al_distancemodel.ival)
		{
		default:
			palSourcef(src, AL_ROLLOFF_FACTOR, s_al_reference_distance.value);
			palSourcef(src, AL_REFERENCE_DISTANCE, 1);
			palSourcef(src, AL_MAX_DISTANCE, 1/chan->dist_mult);	//clamp to the maximum distance you'd normally be allowed to hear... this is probably going to be annoying.
			break;
		case 2:	//linear, mimic quake.
			palSourcef(src, AL_ROLLOFF_FACTOR, 1);
			palSourcef(src, AL_REFERENCE_DISTANCE, 0);
			palSourcef(src, AL_MAX_DISTANCE, 1/chan->dist_mult);
			break;
		}

	/*and start it up again*/
		palSourcePlay(src);
	}

	PrintALError("post start sound");
}

/*
static void S_Info (void)
{
	if (OpenAL_Device == NULL)
		return;

	Con_Printf("OpenAL Version        : %s\n",palGetString(AL_VERSION));
	Con_Printf("OpenAL Vendor         : %s\n",palGetString(AL_VENDOR));
	Con_Printf("OpenAL Renderer       : %s\n",palGetString(AL_RENDERER));
	if(palcIsExtensionPresent(NULL, (const ALCchar*)"ALC_ENUMERATION_EXT")==AL_TRUE)
	{
		Con_Printf("OpenAL Device         : %s\n",palcGetString(OpenAL_Device,ALC_DEVICE_SPECIFIER));
	}
	Con_Printf("OpenAL Default Device : %s\n",palcGetString(OpenAL_Device,ALC_DEFAULT_DEVICE_SPECIFIER));
	Con_Printf("OpenAL AL Extension   : %s\n",palGetString(AL_EXTENSIONS));
	Con_Printf("OpenAL ALC Extension  : %s\n",palcGetString(NULL,ALC_EXTENSIONS));
}
*/

static qboolean OpenAL_InitLibrary(void)
{
	static dllfunction_t openalfuncs[] =
	{
		{(void*)&palGetError, "alGetError"},
		{(void*)&palSourcef, "alSourcef"},
		{(void*)&palSourcei, "alSourcei"},
		{(void*)&palSourcePlayv, "alSourcePlayv"},
		{(void*)&palSourceStopv, "alSourceStopv"},
		{(void*)&palSourcePlay, "alSourcePlay"},
		{(void*)&palSourceStop, "alSourceStop"},
		{(void*)&palDopplerFactor, "alDopplerFactor"},
		{(void*)&palGenBuffers, "alGenBuffers"},
		{(void*)&palIsBuffer, "alIsBuffer"},
		{(void*)&palBufferData, "alBufferData"},
		{(void*)&palDeleteBuffers, "alDeleteBuffers"},
		{(void*)&palListenerfv, "alListenerfv"},
		{(void*)&palSourcefv, "alSourcefv"},
		{(void*)&palGetString, "alGetString"},
		{(void*)&palGenSources, "alGenSources"},
		{(void*)&palListenerf, "alListenerf"},
		{(void*)&palDeleteSources, "alDeleteSources"},
		{(void*)&palSpeedOfSound, "alSpeedOfSound"},
		{(void*)&palDistanceModel, "alDistanceModel"},

		{(void*)&palGetProcAddress, "alGetProcAddress"},
		{(void*)&palGetSourcei, "alGetSourcei"},
		{(void*)&palSourceQueueBuffers, "alSourceQueueBuffers"},
		{(void*)&palSourceUnqueueBuffers, "alSourceUnqueueBuffers"},

		{(void*)&palcOpenDevice, "alcOpenDevice"},
		{(void*)&palcCloseDevice, "alcCloseDevice"},
		{(void*)&palcCreateContext, "alcCreateContext"},
		{(void*)&palcDestroyContext, "alcDestroyContext"},
		{(void*)&palcMakeContextCurrent, "alcMakeContextCurrent"},
		{(void*)&palcProcessContext, "alcProcessContext"},
		{(void*)&palcGetString, "alcGetString"},
		{(void*)&palcIsExtensionPresent, "alcIsExtensionPresent"},
		{NULL}
	};

	if (!openallib_tried)
	{
		openallib_tried = true;
		openallib = Sys_LoadLibrary("OpenAL32", openalfuncs);
	}
	return !!openallib;
}

static qboolean OpenAL_Init(soundcardinfo_t *sc, const char *devname)
{
	oalinfo_t *oali = Z_Malloc(sizeof(oalinfo_t));
	sc->handle = oali;

	if (!OpenAL_InitLibrary())
	{
		Con_Printf("OpenAL is not installed\n");
		return false;
	}

	if (oali->OpenAL_Context)
	{
		Con_Printf("OpenAL: only able to load one device at a time\n");
		return false;
	}

	oali->OpenAL_Device = palcOpenDevice(devname);
	if (oali->OpenAL_Device == NULL)
	{
		PrintALError("Could not init a sound device\n");
		return false;
	}

	oali->OpenAL_Context = palcCreateContext(oali->OpenAL_Device, NULL);
	if (!oali->OpenAL_Context)
		return false;

	palcMakeContextCurrent(oali->OpenAL_Context);
//	palcProcessContext(oali->OpenAL_Context);

	//S_Info();

	//fixme...
	memset(oali->source, 0, sizeof(oali->source));
	PrintALError("alGensources for normal sources");


	palListenerfv(AL_POSITION, oali->ListenPos);
	palListenerfv(AL_VELOCITY, oali->ListenVel);
	palListenerfv(AL_ORIENTATION, oali->ListenOri);

	return true;
}

static void OnChangeALMaxDistance (cvar_t *var, char *oldvalue)
{
}
static void OnChangeALSpeedOfSound (cvar_t *var, char *value)
{
	if (palSpeedOfSound)
		palSpeedOfSound(var->value);
}
static void OnChangeALDopplerFactor (cvar_t *var, char *oldvalue)
{
	if (palDopplerFactor)
		palDopplerFactor(var->value);
}
static void OnChangeALDistanceModel (cvar_t *var, char *value)
{
	if (!palDistanceModel)
		return;

	switch (var->ival)
	{
		case 0:
			palDistanceModel(AL_INVERSE_DISTANCE);
			break;
		case 1:
			palDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
			break;
		case 2:
			palDistanceModel(AL_LINEAR_DISTANCE);
			break;
		case 3:
			palDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
			break;
		case 4:
			palDistanceModel(AL_EXPONENT_DISTANCE);
			break;
		case 5:
			palDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
			break;
		case 6:
			palDistanceModel(AL_NONE);
			break;
		default:
			Cvar_ForceSet(var, "0");
	}	
}

/*stub should not be called*/
static void *OpenAL_LockBuffer (soundcardinfo_t *sc, unsigned int *sampidx)
{
	//Con_Printf("OpenAL: LockBuffer\n");
	return NULL;
}

/*stub should not be called*/
static void OpenAL_UnlockBuffer (soundcardinfo_t *sc, void *buffer)
{
	//Con_Printf("OpenAL: UnlockBuffer\n");
}

static void OpenAL_SetUnderWater (soundcardinfo_t *sc, qboolean underwater)
{
	oalinfo_t *oali = sc->handle;

	if (!oali->effectslot)
		return;
	//don't spam it
	if (oali->cureffect == underwater)
		return;
	oali->cureffect = underwater;
	PrintALError("preunderwater");
	palAuxiliaryEffectSloti(oali->effectslot, AL_EFFECTSLOT_EFFECT, oali->effecttype[oali->cureffect]);
	PrintALError("postunderwater");
	//Con_Printf("OpenAL: SetUnderWater %i\n", underwater);
}

/*stub should not be called*/
static void OpenAL_Submit (soundcardinfo_t *sc, int start, int end)
{
	//Con_Printf("OpenAL: Submit\n");
}

/*stub should not be called*/
static unsigned int OpenAL_GetDMAPos (soundcardinfo_t *sc)
{
	//Con_Printf("OpenAL: GetDMAPos\n");
	return 0;
}

static void OpenAL_Shutdown (soundcardinfo_t *sc)
{
	oalinfo_t *oali = sc->handle;
	int i;

	palDeleteSources(NUM_SOURCES, oali->source);

	/*make sure the buffers are cleared from the sound effects*/
	for (i=0;i<num_sfx;i++)
	{
		if (known_sfx[i].openal_buffer)
		{
			palDeleteBuffers(1,&known_sfx[i].openal_buffer);
			known_sfx[i].openal_buffer = 0;
		}
	}

	palDeleteAuxiliaryEffectSlots(1, &oali->effectslot);
	palDeleteEffects(1, &oali->effecttype[1]);

	palcDestroyContext(oali->OpenAL_Context);
	palcCloseDevice(oali->OpenAL_Device);
	Z_Free(oali);
}

typedef struct {
    float flDensity;
    float flDiffusion;
    float flGain;
    float flGainHF;
    float flGainLF;
    float flDecayTime;
    float flDecayHFRatio;
    float flDecayLFRatio;
    float flReflectionsGain;
    float flReflectionsDelay;
    float flReflectionsPan[3];
    float flLateReverbGain;
    float flLateReverbDelay;
    float flLateReverbPan[3];
    float flEchoTime;
    float flEchoDepth;
    float flModulationTime;
    float flModulationDepth;
    float flAirAbsorptionGainHF;
    float flHFReference;
    float flLFReference;
    float flRoomRolloffFactor;
    int   iDecayHFLimit;
} EFXEAXREVERBPROPERTIES, *LPEFXEAXREVERBPROPERTIES;
#define EFX_REVERB_PRESET_PSYCHOTIC \
    { 0.0625f, 0.5000f, 0.3162f, 0.8404f, 1.0000f, 7.5600f, 0.9100f, 1.0000f, 0.4864f, 0.0200f, { 0.0000f, 0.0000f, 0.0000f }, 2.4378f, 0.0300f, { 0.0000f, 0.0000f, 0.0000f }, 0.2500f, 0.0000f, 4.0000f, 1.0000f, 0.9943f, 5000.0000f, 250.0000f, 0.0000f, 0x0 }
#define EFX_REVERB_PRESET_UNDERWATER \
    { 0.3645f, 1.0000f, 0.3162f, 0.0100f, 1.0000f, 1.4900f, 0.1000f, 1.0000f, 0.5963f, 0.0070f, { 0.0000f, 0.0000f, 0.0000f }, 7.0795f, 0.0110f, { 0.0000f, 0.0000f, 0.0000f }, 0.2500f, 0.0000f, 1.1800f, 0.3480f, 0.9943f, 5000.0000f, 250.0000f, 0.0000f, 0x1 }
ALuint OpenAL_LoadEffect(const EFXEAXREVERBPROPERTIES *reverb)
{
	ALuint effect;
	palGenEffects(1, &effect);
	if(1)//alGetEnumValue("AL_EFFECT_EAXREVERB") != 0)
	{
		/* EAX Reverb is available. Set the EAX effect type then load the
		 * reverb properties. */
		palEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

		palEffectf(effect, AL_EAXREVERB_DENSITY, reverb->flDensity);
		palEffectf(effect, AL_EAXREVERB_DIFFUSION, reverb->flDiffusion);
		palEffectf(effect, AL_EAXREVERB_GAIN, reverb->flGain);
		palEffectf(effect, AL_EAXREVERB_GAINHF, reverb->flGainHF);
		palEffectf(effect, AL_EAXREVERB_GAINLF, reverb->flGainLF);
		palEffectf(effect, AL_EAXREVERB_DECAY_TIME, reverb->flDecayTime);
		palEffectf(effect, AL_EAXREVERB_DECAY_HFRATIO, reverb->flDecayHFRatio);
		palEffectf(effect, AL_EAXREVERB_DECAY_LFRATIO, reverb->flDecayLFRatio);
		palEffectf(effect, AL_EAXREVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain);
		palEffectf(effect, AL_EAXREVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay);
		palEffectfv(effect, AL_EAXREVERB_REFLECTIONS_PAN, reverb->flReflectionsPan);
		palEffectf(effect, AL_EAXREVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain);
		palEffectf(effect, AL_EAXREVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay);
		palEffectfv(effect, AL_EAXREVERB_LATE_REVERB_PAN, reverb->flLateReverbPan);
		palEffectf(effect, AL_EAXREVERB_ECHO_TIME, reverb->flEchoTime);
		palEffectf(effect, AL_EAXREVERB_ECHO_DEPTH, reverb->flEchoDepth);
		palEffectf(effect, AL_EAXREVERB_MODULATION_TIME, reverb->flModulationTime);
		palEffectf(effect, AL_EAXREVERB_MODULATION_DEPTH, reverb->flModulationDepth);
		palEffectf(effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF);
		palEffectf(effect, AL_EAXREVERB_HFREFERENCE, reverb->flHFReference);
		palEffectf(effect, AL_EAXREVERB_LFREFERENCE, reverb->flLFReference);
		palEffectf(effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor);
		palEffecti(effect, AL_EAXREVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit);
	}
	else
	{
		/* No EAX Reverb. Set the standard reverb effect type then load the
		 * available reverb properties. */
		palEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);

		palEffectf(effect, AL_REVERB_DENSITY, reverb->flDensity);
		palEffectf(effect, AL_REVERB_DIFFUSION, reverb->flDiffusion);
		palEffectf(effect, AL_REVERB_GAIN, reverb->flGain);
		palEffectf(effect, AL_REVERB_GAINHF, reverb->flGainHF);
		palEffectf(effect, AL_REVERB_DECAY_TIME, reverb->flDecayTime);
		palEffectf(effect, AL_REVERB_DECAY_HFRATIO, reverb->flDecayHFRatio);
		palEffectf(effect, AL_REVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain);
		palEffectf(effect, AL_REVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay);
		palEffectf(effect, AL_REVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain);
		palEffectf(effect, AL_REVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay);
		palEffectf(effect, AL_REVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF);
		palEffectf(effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor);
		palEffecti(effect, AL_REVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit);
	}
	return effect;
}

static qboolean QDECL OpenAL_InitCard(soundcardinfo_t *sc, const char *devname)
{
	oalinfo_t *oali;
	//no default support, because we're buggy as fuck
	if (!devname)
		return false;

	if (!devname || !*devname)
		devname = palcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

	Con_Printf("Initiating OpenAL: %s.\n", devname);

	if (OpenAL_Init(sc, devname) == false)
	{
		Con_Printf(CON_ERROR "OpenAL init failed\n");
		return false;
	}
	oali = sc->handle;

	Con_DPrintf("OpenAL AL Extension   : %s\n",palGetString(AL_EXTENSIONS));
	Con_DPrintf("OpenAL ALC Extension  : %s\n",palcGetString(oali->OpenAL_Device,ALC_EXTENSIONS));

	sc->Lock = OpenAL_LockBuffer;
	sc->Unlock = OpenAL_UnlockBuffer;
	sc->SetWaterDistortion = OpenAL_SetUnderWater;
	sc->Submit = OpenAL_Submit;
	sc->Shutdown = OpenAL_Shutdown;
	sc->GetDMAPos = OpenAL_GetDMAPos;
	sc->ChannelUpdate = OpenAL_ChannelUpdate;
	sc->ListenerUpdate = OpenAL_ListenerUpdate;

	Q_snprintfz(sc->name, sizeof(sc->name), "%s", devname);

	sc->inactive_sound = true;
	sc->selfpainting = true;

	Cvar_ForceCallback(&s_al_distancemodel);
	Cvar_ForceCallback(&s_al_speedofsound);
	Cvar_ForceCallback(&s_al_dopplerfactor);
	Cvar_ForceCallback(&s_al_max_distance);

	PrintALError("preeffects");
	palSource3i = palGetProcAddress("alSource3i");
	palAuxiliaryEffectSloti = palGetProcAddress("alAuxiliaryEffectSloti");
	palGenAuxiliaryEffectSlots = palGetProcAddress("alGenAuxiliaryEffectSlots");
	palDeleteAuxiliaryEffectSlots = palGetProcAddress("alDeleteAuxiliaryEffectSlots");
	palDeleteEffects = palGetProcAddress("alDeleteEffects");
	palGenEffects = palGetProcAddress("alGenEffects");
	palEffecti = palGetProcAddress("alEffecti");
	palEffectiv = palGetProcAddress("alEffectiv");
	palEffectf = palGetProcAddress("alEffectf");
	palEffectfv = palGetProcAddress("alEffectfv");

	palGenAuxiliaryEffectSlots(1, &oali->effectslot);

	oali->cureffect = 0;
	oali->effecttype[0] = 0;
	{
		EFXEAXREVERBPROPERTIES uw = EFX_REVERB_PRESET_UNDERWATER;
		oali->effecttype[1] = OpenAL_LoadEffect(&uw);
	}
	PrintALError("posteffects");
	return true;
}

#define SDRVNAME "OpenAL"
static qboolean QDECL OpenAL_Enumerate(void (QDECL *callback)(const char *driver, const char *devicecode, const char *readabledevice))
{
	const char *devnames;
	if (!OpenAL_InitLibrary())
		return true; //enumerate nothing if al is disabled

	devnames = palcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
	if (!devnames)
		devnames = palcGetString(NULL, ALC_DEVICE_SPECIFIER);
	while(*devnames)
	{
		callback(SDRVNAME, devnames, va("OAL:%s", devnames));
		devnames += strlen(devnames)+1;
	}
	return true;
}

sounddriver_t OPENAL_Output =
{
	SDRVNAME,
	OpenAL_InitCard,
	OpenAL_Enumerate
};


#if defined(VOICECHAT)

qboolean OpenAL_InitCapture(void)
{
	if (!OpenAL_InitLibrary())
		return false;

	//is there really much point checking for the name when the functions should exist or not regardless?
	//if its not really supported, I would trust the open+enumerate operations to reliably fail. the functions are exported as actual symbols after all, not some hidden driver feature.
	//it doesn't really matter if the default driver supports it, so long as one does, I guess.
	//if (!palcIsExtensionPresent(NULL, "ALC_EXT_capture"))
	//	return false;

	if(!palcCaptureOpenDevice)
	{
		palcGetIntegerv = Sys_GetAddressForName(openallib, "alcGetIntegerv");

		palcCaptureOpenDevice = Sys_GetAddressForName(openallib, "alcCaptureOpenDevice");
		palcCaptureStart = Sys_GetAddressForName(openallib, "alcCaptureStart");
		palcCaptureSamples = Sys_GetAddressForName(openallib, "alcCaptureSamples");
		palcCaptureStop = Sys_GetAddressForName(openallib, "alcCaptureStop");
		palcCaptureCloseDevice = Sys_GetAddressForName(openallib, "alcCaptureCloseDevice");
	}

	return palcGetIntegerv&&palcCaptureOpenDevice&&palcCaptureStart&&palcCaptureSamples&&palcCaptureStop&&palcCaptureCloseDevice;
}
qboolean QDECL OPENAL_Capture_Enumerate (void (QDECL *callback) (const char *drivername, const char *devicecode, const char *readablename))
{
	const char *devnames;
	if (!OpenAL_InitCapture())
		return true; //enumerate nothing if al is disabled

	devnames = palcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
	while(*devnames)
	{
		callback(SDRVNAME, devnames, va("OAL:%s", devnames));
		devnames += strlen(devnames)+1;
	}
	return true;
}
//fte's capture api specifies mono 16.
void *QDECL OPENAL_Capture_Init (int samplerate, const char *device)
{
	if (!device)	//no default devices please, too buggy for that.
		return NULL;

	if (!OpenAL_InitCapture())
		return NULL; //enumerate nothing if al is disabled

	if (!device || !*device)
		device = palcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);

	return palcCaptureOpenDevice(device, samplerate, AL_FORMAT_MONO16, 0.5*samplerate);
}
void QDECL OPENAL_Capture_Start (void *ctx)
{
	ALCdevice *device = ctx;
	palcCaptureStart(device);
}
unsigned int QDECL OPENAL_Capture_Update (void *ctx, unsigned char *buffer, unsigned int minbytes, unsigned int maxbytes)
{
#define samplesize sizeof(short)
	ALCdevice *device = ctx;
	int avail = 0;
	palcGetIntegerv(device, ALC_CAPTURE_SAMPLES, sizeof(ALint), &avail);
	if (avail*samplesize < minbytes)
		return 0;	//don't bother grabbing it if its below the threshold.
	palcCaptureSamples(device, (ALCvoid *)buffer, avail);
	return avail * samplesize;
}
void QDECL OPENAL_Capture_Stop (void *ctx)
{
	ALCdevice *device = ctx;
	palcCaptureStop(device);
}
void QDECL OPENAL_Capture_Shutdown (void *ctx)
{
	ALCdevice *device = ctx;
	palcCaptureCloseDevice(device);
}

snd_capture_driver_t OPENAL_Capture =
{
	1,
	SDRVNAME,
	OPENAL_Capture_Enumerate,
	OPENAL_Capture_Init,
	OPENAL_Capture_Start,
	OPENAL_Capture_Update,
	OPENAL_Capture_Stop,
	OPENAL_Capture_Shutdown
};
#endif

#endif
