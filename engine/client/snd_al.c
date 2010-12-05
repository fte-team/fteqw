#include "quakedef.h"

/*
This is based on Jogi's OpenAL support.
Much of it is stripped, to try and get it clean/compliant.

Missing features:
FIXME: does not kill old sounds on replaced sound channels (quake compliance).
FIXME: no static/ambient sounds (quake compliance).
FIXME: does not support streaming audio from voice/videos (voice/q2/q3 compliance).
If the above ar implemented, it can be the default device.
FIXME: listener velocity calculations (currently ugly).
FIXME: does not track entity velocities, so no dopler (awkward, quake move playing sounds at all).
FIXME: no eax (underwater).
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
#define AL_REFERENCE_DISTANCE                     0x1020
#define AL_ROLLOFF_FACTOR                         0x1021
#define AL_MAX_DISTANCE                           0x1023
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
typedef int ALCenum;
typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;

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

//#include "AL/alut.h"
//#include "AL/al.h"
//#include "AL/alext.h"




#define SOUNDVARS "OpenAL variables"


extern sfx_t *known_sfx;
extern int loaded_sfx;
extern int num_sfx;

static qboolean openal_vars_initialized = false;


static void OnChangeALMaxDistance (cvar_t *var, char *value);
static void OnChangeALSpeedOfSound (cvar_t *var, char *value);
static void OnChangeALDopplerFactor (cvar_t *var, char *value);
static void OnChangeALDistanceModel (cvar_t *var, char *value);
/*
static void S_Init_f(void);
static void S_Info(void);

static void S_Shutdown_f(void);
*/
static cvar_t s_al_enable = CVAR("s_al_enable", "0");
static cvar_t s_al_debug = CVAR("s_al_debug", "0");
static cvar_t s_al_max_distance = CVARFC("s_al_max_distance", "1000",0,OnChangeALMaxDistance);
static cvar_t s_al_speedofsound = CVARFC("s_al_speedofsound", "343.3",0,OnChangeALSpeedOfSound);
static cvar_t s_al_dopplerfactor = CVARFC("s_al_dopplerfactor", "3.0",0,OnChangeALDopplerFactor);
static cvar_t s_al_distancemodel = CVARFC("s_al_distancemodel", "1",0,OnChangeALDistanceModel);
static cvar_t s_al_rolloff_factor = CVAR("s_al_rolloff_factor", "1");
static cvar_t s_al_reference_distance = CVAR("s_al_reference_distance", "120");static cvar_t s_al_velocityscale = CVAR("s_al_velocityscale", "1");
static cvar_t s_al_static_listener = CVAR("s_al_static_listener", "0");	//cheat

#define NUM_SOURCES MAX_CHANNELS
static ALuint source[NUM_SOURCES];

static ALCdevice *OpenAL_Device;
static ALCcontext *OpenAL_Context;

static ALfloat ListenPos[] = { 0.0, 0.0, 0.0 };

// Velocity of the listener.
static ALfloat ListenVel[] = { 0.0, 0.0, 0.0 };

// Orientation of the listener. (first 3 elements are "at", second 3 are "up")
static ALfloat ListenOri[] = { 0.0, 0.0, -1.0, 0.0, 1.0, 0.0 };

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

void OpenAL_LoadCache(sfx_t *s, sfxcache_t *sc)
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
	palGenBuffers(1, &s->openal_buffer);
	/*openal is inconsistant and supports only 8bit unsigned or 16bit signed*/
	if (sc->width == 1)
	{
		unsigned char *tmp = malloc(size);
		char *src = sc->data;
		int i;
		for (i = 0; i < size; i++)
		{
			tmp[i] = src[i]+128;
		}
		palBufferData(s->openal_buffer, fmt, tmp, size, sc->speed);
		free(tmp);
	}
	else
		palBufferData(s->openal_buffer, fmt, sc->data, size, sc->speed);

	//FIXME: we need to handle oal-oom error codes

	PrintALError("Buffer Data");
}

void OpenAL_CvarInit(void)
{
	Cvar_Register(&s_al_enable, SOUNDVARS);
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
void OpenAL_Update_Listener(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t velocity)
{
	VectorScale(velocity, s_al_velocityscale.value, ListenVel);
	VectorCopy(origin, ListenPos);

	ListenOri[0] = forward[0];
	ListenOri[1] = forward[1];
	ListenOri[2] = forward[2];
	ListenOri[3] = up[0];
	ListenOri[4] = up[1];
	ListenOri[5] = up[2];


	if (!s_al_static_listener.value)
	{
		palListenerf(AL_GAIN, volume.value*voicevolumemod);
		palListenerfv(AL_POSITION, ListenPos);
		palListenerfv(AL_VELOCITY, ListenVel);
		palListenerfv(AL_ORIENTATION, ListenOri);
	}
}

/*
static void OpenAL_StopAllSounds(qboolean clear)
{
	Con_Printf("-------------------------- %i ---------------\n",clear);
	palSourceStopv(NUM_SOURCES,source);
	palSourceStopv(NUM_SOURCES,static_source);
	num_sfx=0;
	num_static_source=0;
	if (clear)
	{
	}
}
*/
/*
void OpenAL_StartSound(int entnum, int entchannel, sfx_t * sfx, vec3_t origin, float fvol, float attenuation, float pitch)
{
	vec3_t	tmp;
	extern cvar_t temp1;
	if (!temp1.value)
		temp1.value = 1;

	if (!sfx->openal_buffer)
	{
		sfxcache_t *sc = Cache_Check(&sfx->cache);
		if (!sc)
			return;
		OpenAL_LoadCache(sfx, sc);
	}

	if (!origin)
		VectorClear(tmp);
	else
	{
		tmp[0] = origin[0];
		tmp[1] = origin[1];
		tmp[2] = origin[2];
	}

	PrintALError("pre start sound");

	palSourceStop(source[num_sfx]);
	palSourcei(source[num_sfx], AL_BUFFER, sfx->openal_buffer);
	palSourcef(source[num_sfx], AL_PITCH, pitch*temp1.value);
	palSourcef(source[num_sfx], AL_GAIN, fvol);
//	palSourcef(source[num_sfx], AL_MAX_DISTANCE, s_al_max_distance.value);
//	palSourcef(source[num_sfx], AL_ROLLOFF_FACTOR, s_al_rolloff_factor.value);
	palSourcefv(source[num_sfx], AL_POSITION, tmp);
	palSourcefv(source[num_sfx], AL_VELOCITY, vec3_origin);
	palSourcef(source[num_sfx], AL_REFERENCE_DISTANCE, s_al_reference_distance.value);

	if (entnum == -1 || entnum == cl.playernum[0]+1)
	{
		palSourcei(source[num_sfx], AL_SOURCE_RELATIVE, AL_TRUE);
		palSourcef(source[num_sfx], AL_ROLLOFF_FACTOR, 0.0f);
	}
	else
	{
		palSourcei(source[num_sfx], AL_SOURCE_RELATIVE, AL_FALSE);
		palSourcef(source[num_sfx], AL_ROLLOFF_FACTOR, s_al_rolloff_factor.value);
	}

	palSourcePlay(source[num_sfx]);
	num_sfx++;
	if (num_sfx >= NUM_SOURCES)
		num_sfx =0;

	PrintALError("post start sound");
}*/

static void OpenAL_ChannelUpdate(soundcardinfo_t *sc, channel_t *chan, unsigned int schanged)
{
	ALuint src;
	sfx_t *sfx = chan->sfx;
	float pitch;

	src = source[chan - sc->channel];
	if (!src)
	{
		if (!sfx || chan->master_vol == 0)
			return;
		palGenSources(1, &src);
		source[chan - sc->channel] = src;
		schanged = true;
	}

	PrintALError("pre start sound");

	if (schanged && src)
		palSourceStop(src);

	/*just wanted to stop it?*/
	if (!sfx || chan->master_vol == 0)
	{
		if (src)
		{
			palDeleteBuffers(1, &src);
			source[chan - sc->channel] = 0;
		}
		return;
	}

	if (schanged)
	{
		if (!sfx->openal_buffer)
		{
			sfxcache_t *sc = S_LoadSound(sfx);
			if (!sc)	/*ack! can't start it if its not loaded!*/
				return;
			OpenAL_LoadCache(sfx, sc);
		}

		palSourcei(src, AL_BUFFER, sfx->openal_buffer);
	}
	palSourcef(src, AL_GAIN, chan->master_vol/255.0f);
//	palSourcef(src, AL_MAX_DISTANCE, s_al_max_distance.value);
//	palSourcef(src, AL_ROLLOFF_FACTOR, s_al_rolloff_factor.value);
	palSourcefv(src, AL_POSITION, chan->origin);
	palSourcefv(src, AL_VELOCITY, vec3_origin);

	if (schanged)
	{
		pitch = (float)chan->rate/(1<<PITCHSHIFT);
		palSourcef(src, AL_PITCH, pitch);

		palSourcef(src, AL_REFERENCE_DISTANCE, s_al_reference_distance.value);
		palSourcei(src, AL_LOOPING, chan->looping?AL_TRUE:AL_FALSE);
		if (chan->entnum == -1 || chan->entnum == cl.playernum[0]+1)
		{
			palSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
			palSourcef(src, AL_ROLLOFF_FACTOR, 0.0f);
		}
		else
		{
			palSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
			palSourcef(src, AL_ROLLOFF_FACTOR, s_al_rolloff_factor.value*chan->dist_mult);
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

static qboolean OpenAL_Init(void)
{
	dllfunction_t openalfuncs[] =
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
	if (!Sys_LoadLibrary("OpenAL32", openalfuncs))
	{
		Con_Printf("OpenAL is not installed\n");
		return false;
	}

	OpenAL_Device = palcOpenDevice(NULL);
	if (OpenAL_Device == NULL)
	{
		PrintALError("Could not init a sound device\n");
		return false;
	}

	OpenAL_Context = palcCreateContext(OpenAL_Device, NULL);
	palcMakeContextCurrent(OpenAL_Context);
//	palcProcessContext(OpenAL_Context);

	//S_Info();


	palGenSources(NUM_SOURCES, source);
	PrintALError("alGensources for normal sources");


	palListenerfv(AL_POSITION, ListenPos);
	palListenerfv(AL_VELOCITY, ListenVel);
	palListenerfv(AL_ORIENTATION, ListenOri);

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
static void *OpenAL_LockBuffer (soundcardinfo_t *sc)
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
	//Con_Printf("OpenAL: SetUnderWater %i\n", underwater);
}

/*stub should not be called*/
static void OpenAL_Submit (soundcardinfo_t *sc)
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
	int i;

	palDeleteSources(NUM_SOURCES, source);

	/*make sure the buffers are cleared from the sound effects*/
	for (i=0;i<num_sfx;i++)
	{
		if (known_sfx[i].openal_buffer)
		{
			palDeleteBuffers(1,&known_sfx[i].openal_buffer);
			known_sfx[i].openal_buffer = 0;
		}
	}

	palcDestroyContext(OpenAL_Context);
	OpenAL_Context = NULL;
	palcCloseDevice(OpenAL_Device);
	OpenAL_Device = NULL;
}


static int OpenAL_InitCard(soundcardinfo_t *sc, int cardnum)
{
	if (cardnum != 0)
		return 2;

	if (!s_al_enable.ival)
		return 2;

	Con_Printf("Initiating OpenAL sound device.\n");

	if (OpenAL_Init() == false)
	{
		Con_Printf(CON_ERROR "OpenAL init failed\n");
		return false;
	}

	sc->Lock = OpenAL_LockBuffer;
	sc->Unlock = OpenAL_UnlockBuffer;
	sc->SetWaterDistortion = OpenAL_SetUnderWater;
	sc->Submit = OpenAL_Submit;
	sc->Shutdown = OpenAL_Shutdown;
	sc->GetDMAPos = OpenAL_GetDMAPos;
	sc->ChannelUpdate = OpenAL_ChannelUpdate;

	snprintf(sc->name, sizeof(sc->name), "OpenAL device");

	sc->openal = 1;
	sc->inactive_sound = true;
	sc->selfpainting = true;

	Cvar_ForceCallback(&s_al_distancemodel);
	Cvar_ForceCallback(&s_al_speedofsound);
	Cvar_ForceCallback(&s_al_dopplerfactor);
	Cvar_ForceCallback(&s_al_max_distance);
	return true;
}

int (*pOPENAL_InitCard) (soundcardinfo_t *sc, int cardnum) = &OpenAL_InitCard;

#endif
