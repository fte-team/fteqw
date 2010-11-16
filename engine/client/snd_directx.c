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
#include "quakedef.h"
#include "winquake.h"

#include <dsound.h>

#define FORCE_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
// SDL fix, seems SDL builds complain about multiple definitions of those 2
#ifndef _SDL
	FORCE_DEFINE_GUID(IID_IDirectSound, 0x279AFA83, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
	FORCE_DEFINE_GUID(IID_IKsPropertySet, 0x31efac30, 0x515c, 0x11d0, 0xa9, 0xaa, 0x00, 0xaa, 0x00, 0x61, 0xbe, 0x93);
#endif

#define SND_ERROR 0
#define SND_LOADED 1
#define SND_NOMORE 2	//like error, but doesn't try the next card.

#ifdef AVAIL_DSOUND

#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)
#define iDirectSoundEnumerate(a,b,c)	pDirectSoundEnumerate(a,b)

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);
#if defined(VOICECHAT) && !defined(__MINGW32__)
HRESULT (WINAPI *pDirectSoundCaptureCreate)(GUID FAR *lpGUID, LPDIRECTSOUNDCAPTURE FAR *lplpDS, IUnknown FAR *pUnkOuter);
#endif
HRESULT (WINAPI *pDirectSoundEnumerate)(LPDSENUMCALLBACKA lpCallback, LPVOID lpContext );

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

typedef struct {
	LPDIRECTSOUND pDS;
	LPDIRECTSOUNDBUFFER pDSBuf;
	LPDIRECTSOUNDBUFFER pDSPBuf;

	DWORD gSndBufSize;
	DWORD		mmstarttime;

#ifdef _IKsPropertySet_
	LPKSPROPERTYSET	EaxKsPropertiesSet;
#endif
} dshandle_t;

HINSTANCE hInstDS;

static void DSOUND_Restore(soundcardinfo_t *sc)
{
	DWORD	dwStatus;
	dshandle_t *dh = sc->handle;
	if (dh->pDSBuf->lpVtbl->GetStatus (dh->pDSBuf, &dwStatus) != DD_OK)
		Con_Printf ("Couldn't get sound buffer status\n");

	if (dwStatus & DSBSTATUS_BUFFERLOST)
		dh->pDSBuf->lpVtbl->Restore (dh->pDSBuf);

	if (!(dwStatus & DSBSTATUS_PLAYING))
		dh->pDSBuf->lpVtbl->Play(dh->pDSBuf, 0, 0, DSBPLAY_LOOPING);
}

DWORD	dwSize;
static void *DSOUND_Lock(soundcardinfo_t *sc)
{
	void *ret;
	int reps;
	DWORD	dwSize2=0;
	DWORD	*pbuf2;
	HRESULT	hresult;

	dshandle_t *dh = sc->handle;
	dwSize=0;

	reps = 0;

	while ((hresult = dh->pDSBuf->lpVtbl->Lock(dh->pDSBuf, 0, dh->gSndBufSize, (void**)&ret, &dwSize,
								   (void**)&pbuf2, &dwSize2, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_Printf ("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
			return NULL;
		}

		if (++reps > 10000)
		{
			Con_Printf ("S_TransferStereo16: DS: couldn't restore buffer\n");
			return NULL;
		}

		DSOUND_Restore(sc);
	}

	return ret;
}

//called when the mixer is done with it.
static void DSOUND_Unlock(soundcardinfo_t *sc, void *buffer)
{
	dshandle_t *dh = sc->handle;
	dh->pDSBuf->lpVtbl->Unlock(dh->pDSBuf, buffer, dwSize, NULL, 0);
}

/*
==================
FreeSound
==================
*/
//per device
static void DSOUND_Shutdown (soundcardinfo_t *sc)
{
	dshandle_t *dh = sc->handle;
	if (!dh)
		return;
	sc->handle = NULL;
#ifdef _IKsPropertySet_
	if (dh->EaxKsPropertiesSet)
	{
		IKsPropertySet_Release(dh->EaxKsPropertiesSet);
	}
#endif
	if (dh->pDSBuf)
	{
		dh->pDSBuf->lpVtbl->Stop(dh->pDSBuf);
		dh->pDSBuf->lpVtbl->Release(dh->pDSBuf);
	}

// only release primary buffer if it's not also the mixing buffer we just released
	if (dh->pDSPBuf && (dh->pDSBuf != dh->pDSPBuf))
	{
		dh->pDSPBuf->lpVtbl->Release(dh->pDSPBuf);
	}

	if (dh->pDS)
	{
		dh->pDS->lpVtbl->SetCooperativeLevel (dh->pDS, mainwindow, DSSCL_NORMAL);
		dh->pDS->lpVtbl->Release(dh->pDS);
	}

	dh->pDS = NULL;
	dh->pDSBuf = NULL;
	dh->pDSPBuf = NULL;
#ifdef _IKsPropertySet_
	dh->EaxKsPropertiesSet = NULL;
#endif

	Z_Free(dh);
}


const char *dsndcard;
GUID FAR *dsndguid;
int dsnd_guids;
int aimedforguid;
static BOOL (CALLBACK  DSEnumCallback)(GUID FAR *guid, LPCSTR str1, LPCSTR str2, LPVOID parm)
{
	if (guid == NULL)
		return TRUE;

	if (aimedforguid == dsnd_guids)
	{
		dsndcard = str1;
		dsndguid = guid;
	}
	dsnd_guids++;
	return TRUE;
}


/*
	Direct Sound.
	These following defs should be moved to winquake.h somewhere.

	We tell DS to use a different wave format. We do this to gain extra channels. >2
	We still use the old stuff too, when we can for compatability.

	EAX 2 is also supported.
	This is a global state. Once applied, it's applied for other programs too.
	We have to do a few special things to try to ensure support in all it's different versions.
*/

/* new formatTag:*/
#ifndef WAVE_FORMAT_EXTENSIBLE
# define WAVE_FORMAT_EXTENSIBLE (0xfffe)
#endif

/* Speaker Positions:*/
# define SPEAKER_FRONT_LEFT              0x1
# define SPEAKER_FRONT_RIGHT             0x2
# define SPEAKER_FRONT_CENTER            0x4
# define SPEAKER_LOW_FREQUENCY           0x8
# define SPEAKER_BACK_LEFT               0x10
# define SPEAKER_BACK_RIGHT              0x20
# define SPEAKER_FRONT_LEFT_OF_CENTER    0x40
# define SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
# define SPEAKER_BACK_CENTER             0x100
# define SPEAKER_SIDE_LEFT               0x200
# define SPEAKER_SIDE_RIGHT              0x400
# define SPEAKER_TOP_CENTER              0x800
# define SPEAKER_TOP_FRONT_LEFT          0x1000
# define SPEAKER_TOP_FRONT_CENTER        0x2000
# define SPEAKER_TOP_FRONT_RIGHT         0x4000
# define SPEAKER_TOP_BACK_LEFT           0x8000
# define SPEAKER_TOP_BACK_CENTER         0x10000
# define SPEAKER_TOP_BACK_RIGHT          0x20000

/* Bit mask locations reserved for future use*/
# define SPEAKER_RESERVED                0x7FFC0000

/* Used to specify that any possible permutation of speaker configurations*/
# define SPEAKER_ALL                     0x80000000

/* DirectSound Speaker Config*/
# define KSAUDIO_SPEAKER_MONO            (SPEAKER_FRONT_CENTER)
# define KSAUDIO_SPEAKER_STEREO          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
# define KSAUDIO_SPEAKER_QUAD            (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT)
# define KSAUDIO_SPEAKER_SURROUND        (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_FRONT_CENTER | SPEAKER_BACK_CENTER)
# define KSAUDIO_SPEAKER_5POINT1         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | \
                                         SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT)
# define KSAUDIO_SPEAKER_7POINT1         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | \
                                         SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | \
                                         SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | \
                                         SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER)

typedef struct {
	WAVEFORMATEX    Format;
	union {
		WORD wValidBitsPerSample;       /* bits of precision  */
		WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
		WORD wReserved;                 /* If neither applies, set to */
										/* zero. */
	} Samples;
	DWORD           dwChannelMask;      /* which channels are */
										/* present in stream  */
	GUID            SubFormat;
} QWAVEFORMATEX;

const static GUID  KSDATAFORMAT_SUBTYPE_PCM = {0x00000001,0x0000,0x0010,
						{0x80,
						0x00,
						0x00,
						0xaa,
						0x00,
						0x38,
						0x9b,
						0x71}};

#ifdef _IKsPropertySet_
const static GUID  CLSID_EAXDIRECTSOUND = {0x4ff53b81, 0x1ce0, 0x11d3,
{0xaa, 0xb8, 0x0, 0xa0, 0xc9, 0x59, 0x49, 0xd5}};
const static GUID  DSPROPSETID_EAX20_LISTENERPROPERTIES = {0x306a6a8, 0xb224, 0x11d2,
{0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};

typedef struct _EAXLISTENERPROPERTIES
{
    long lRoom;                    // room effect level at low frequencies
    long lRoomHF;                  // room effect high-frequency level re. low frequency level
    float flRoomRolloffFactor;     // like DS3D flRolloffFactor but for room effect
    float flDecayTime;             // reverberation decay time at low frequencies
    float flDecayHFRatio;          // high-frequency to low-frequency decay time ratio
    long lReflections;             // early reflections level relative to room effect
    float flReflectionsDelay;      // initial reflection delay time
    long lReverb;                  // late reverberation level relative to room effect
    float flReverbDelay;           // late reverberation delay time relative to initial reflection
    unsigned long dwEnvironment;   // sets all listener properties
    float flEnvironmentSize;       // environment size in meters
    float flEnvironmentDiffusion;  // environment diffusion
    float flAirAbsorptionHF;       // change in level per meter at 5 kHz
    unsigned long dwFlags;         // modifies the behavior of properties
} EAXLISTENERPROPERTIES, *LPEAXLISTENERPROPERTIES;
enum
{
    EAX_ENVIRONMENT_GENERIC,
    EAX_ENVIRONMENT_PADDEDCELL,
    EAX_ENVIRONMENT_ROOM,
    EAX_ENVIRONMENT_BATHROOM,
    EAX_ENVIRONMENT_LIVINGROOM,
    EAX_ENVIRONMENT_STONEROOM,
    EAX_ENVIRONMENT_AUDITORIUM,
    EAX_ENVIRONMENT_CONCERTHALL,
    EAX_ENVIRONMENT_CAVE,
    EAX_ENVIRONMENT_ARENA,
    EAX_ENVIRONMENT_HANGAR,
    EAX_ENVIRONMENT_CARPETEDHALLWAY,
    EAX_ENVIRONMENT_HALLWAY,
    EAX_ENVIRONMENT_STONECORRIDOR,
    EAX_ENVIRONMENT_ALLEY,
    EAX_ENVIRONMENT_FOREST,
    EAX_ENVIRONMENT_CITY,
    EAX_ENVIRONMENT_MOUNTAINS,
    EAX_ENVIRONMENT_QUARRY,
    EAX_ENVIRONMENT_PLAIN,
    EAX_ENVIRONMENT_PARKINGLOT,
    EAX_ENVIRONMENT_SEWERPIPE,
    EAX_ENVIRONMENT_UNDERWATER,
    EAX_ENVIRONMENT_DRUGGED,
    EAX_ENVIRONMENT_DIZZY,
    EAX_ENVIRONMENT_PSYCHOTIC,

    EAX_ENVIRONMENT_COUNT
};
typedef enum
{
    DSPROPERTY_EAXLISTENER_NONE,
    DSPROPERTY_EAXLISTENER_ALLPARAMETERS,
    DSPROPERTY_EAXLISTENER_ROOM,
    DSPROPERTY_EAXLISTENER_ROOMHF,
    DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR,
    DSPROPERTY_EAXLISTENER_DECAYTIME,
    DSPROPERTY_EAXLISTENER_DECAYHFRATIO,
    DSPROPERTY_EAXLISTENER_REFLECTIONS,
    DSPROPERTY_EAXLISTENER_REFLECTIONSDELAY,
    DSPROPERTY_EAXLISTENER_REVERB,
    DSPROPERTY_EAXLISTENER_REVERBDELAY,
    DSPROPERTY_EAXLISTENER_ENVIRONMENT,
    DSPROPERTY_EAXLISTENER_ENVIRONMENTSIZE,
    DSPROPERTY_EAXLISTENER_ENVIRONMENTDIFFUSION,
    DSPROPERTY_EAXLISTENER_AIRABSORPTIONHF,
    DSPROPERTY_EAXLISTENER_FLAGS
} DSPROPERTY_EAX_LISTENERPROPERTY;

const static GUID DSPROPSETID_EAX20_BUFFERPROPERTIES ={
    0x306a6a7,
    0xb224,
    0x11d2,
    {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};

const static GUID CLSID_EAXDirectSound ={
		0x4ff53b81,
		0x1ce0,
		0x11d3,
		{0xaa, 0xb8, 0x0, 0xa0, 0xc9, 0x59, 0x49, 0xd5}};

typedef struct _EAXBUFFERPROPERTIES
{
    long lDirect;                // direct path level
    long lDirectHF;              // direct path level at high frequencies
    long lRoom;                  // room effect level
    long lRoomHF;                // room effect level at high frequencies
    float flRoomRolloffFactor;   // like DS3D flRolloffFactor but for room effect
    long lObstruction;           // main obstruction control (attenuation at high frequencies)
    float flObstructionLFRatio;  // obstruction low-frequency level re. main control
    long lOcclusion;             // main occlusion control (attenuation at high frequencies)
    float flOcclusionLFRatio;    // occlusion low-frequency level re. main control
    float flOcclusionRoomRatio;  // occlusion room effect level re. main control
    long lOutsideVolumeHF;       // outside sound cone level at high frequencies
    float flAirAbsorptionFactor; // multiplies DSPROPERTY_EAXLISTENER_AIRABSORPTIONHF
    unsigned long dwFlags;       // modifies the behavior of properties
} EAXBUFFERPROPERTIES, *LPEAXBUFFERPROPERTIES;

typedef enum
{
    DSPROPERTY_EAXBUFFER_NONE,
    DSPROPERTY_EAXBUFFER_ALLPARAMETERS,
    DSPROPERTY_EAXBUFFER_DIRECT,
    DSPROPERTY_EAXBUFFER_DIRECTHF,
    DSPROPERTY_EAXBUFFER_ROOM,
    DSPROPERTY_EAXBUFFER_ROOMHF,
    DSPROPERTY_EAXBUFFER_ROOMROLLOFFFACTOR,
    DSPROPERTY_EAXBUFFER_OBSTRUCTION,
    DSPROPERTY_EAXBUFFER_OBSTRUCTIONLFRATIO,
    DSPROPERTY_EAXBUFFER_OCCLUSION,
    DSPROPERTY_EAXBUFFER_OCCLUSIONLFRATIO,
    DSPROPERTY_EAXBUFFER_OCCLUSIONROOMRATIO,
    DSPROPERTY_EAXBUFFER_OUTSIDEVOLUMEHF,
    DSPROPERTY_EAXBUFFER_AIRABSORPTIONFACTOR,
    DSPROPERTY_EAXBUFFER_FLAGS
} DSPROPERTY_EAX_BUFFERPROPERTY;
#endif

static void DSOUND_SetUnderWater(soundcardinfo_t *sc, qboolean underwater)
{
#ifdef _IKsPropertySet_
	dshandle_t *dh = sc->handle;

	//attempt at eax support.
	//EAX is a global thing. Get it going in a game and your media player will be doing it too.

	if (dh->EaxKsPropertiesSet)	//only on ds cards.
	{
		EAXLISTENERPROPERTIES ListenerProperties =  {0};

/*		DWORD p;
		IKsPropertySet_Get(dh->EaxKsPropertiesSet, &DSPROPSETID_EAX20_LISTENERPROPERTIES,
			DSPROPERTY_EAXLISTENER_ALLPARAMETERS, 0, 0, &ListenerProperties,
			sizeof(ListenerProperties), &p);
*/
		if (underwater)
		{
#if 1 //phycotic.
			ListenerProperties.flEnvironmentSize = 2.8;
			ListenerProperties.flEnvironmentDiffusion = 0.240;
			ListenerProperties.lRoom = -374;
			ListenerProperties.lRoomHF = -150;
			ListenerProperties.flRoomRolloffFactor = 0;
			ListenerProperties.flAirAbsorptionHF = -5;
			ListenerProperties.lReflections = -10000;
			ListenerProperties.flReflectionsDelay  = 0.053;
			ListenerProperties.lReverb = 625;
			ListenerProperties.flReverbDelay = 0.08;
			ListenerProperties.flDecayTime = 5.096;
			ListenerProperties.flDecayHFRatio = 0.910;
			ListenerProperties.dwFlags = 0x3f;
			ListenerProperties.dwEnvironment = EAX_ENVIRONMENT_PSYCHOTIC;
#else
			ListenerProperties.flEnvironmentSize = 5.8;
			ListenerProperties.flEnvironmentDiffusion = 0;
			ListenerProperties.lRoom = -374;
			ListenerProperties.lRoomHF = -2860;
			ListenerProperties.flRoomRolloffFactor = 0;
			ListenerProperties.flAirAbsorptionHF = -5;
			ListenerProperties.lReflections = -889;
			ListenerProperties.flReflectionsDelay  = 0.024;
			ListenerProperties.lReverb = 797;
			ListenerProperties.flReverbDelay = 0.035;
			ListenerProperties.flDecayTime = 5.568;
			ListenerProperties.flDecayHFRatio = 0.100;
			ListenerProperties.dwFlags = 0x3f;
			ListenerProperties.dwEnvironment = EAX_ENVIRONMENT_UNDERWATER;
#endif
		}
		else
		{
			ListenerProperties.flEnvironmentSize = 1;
			ListenerProperties.flEnvironmentDiffusion = 0;
			ListenerProperties.lRoom = 0;
			ListenerProperties.lRoomHF = 0;
			ListenerProperties.flRoomRolloffFactor = 0;
			ListenerProperties.flAirAbsorptionHF = 0;
			ListenerProperties.lReflections = 1000;
			ListenerProperties.flReflectionsDelay  = 0;
			ListenerProperties.lReverb = 813;
			ListenerProperties.flReverbDelay = 0.00;
			ListenerProperties.flDecayTime = 0.1;
			ListenerProperties.flDecayHFRatio = 0.1;
			ListenerProperties.dwFlags = 0x3f;
			ListenerProperties.dwEnvironment = EAX_ENVIRONMENT_GENERIC;
		}

//		env = EAX_ENVIRONMENT_UNDERWATER;

		if (FAILED(IKsPropertySet_Set(dh->EaxKsPropertiesSet, &DSPROPSETID_EAX20_LISTENERPROPERTIES,
					DSPROPERTY_EAXLISTENER_ALLPARAMETERS, 0, 0, &ListenerProperties,
					sizeof(ListenerProperties))))
			Con_SafePrintf ("EAX set failed\n");
	}
#endif
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
static int DSOUND_GetDMAPos(soundcardinfo_t *sc)
{
	DWORD	mmtime;
	int		s;
	DWORD	dwWrite;

	dshandle_t *dh = sc->handle;

	dh->pDSBuf->lpVtbl->GetCurrentPosition(dh->pDSBuf, &mmtime, &dwWrite);
	s = mmtime - dh->mmstarttime;


	s >>= (sc->sn.samplebits/8) - 1;

	s %= (sc->sn.samples);

	return s;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
static void DSOUND_Submit(soundcardinfo_t *sc)
{
}




/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
int DSOUND_InitCard (soundcardinfo_t *sc, int cardnum)
{
	extern cvar_t snd_eax, snd_inactive;
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DWORD			dwSize, dwWrite;
	DSCAPS			dscaps;
	QWAVEFORMATEX	format, pformat;
	HRESULT			hresult;
	int				reps;
	qboolean		primary_format_set;
	dshandle_t *dh;
	char *buffer;

	if (COM_CheckParm("-wavonly"))
		return SND_NOMORE;

	memset (&format, 0, sizeof(format));

	if (sc->sn.numchannels >= 6)	//5.1 surround
	{
		format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		format.Format.cbSize = 22;
		memcpy(&format.SubFormat, &KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID));

		format.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
		sc->sn.numchannels = 6;
	}
	else if (sc->sn.numchannels >= 4)	//4 speaker quad
	{
		format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		format.Format.cbSize = 22;
		memcpy(&format.SubFormat, &KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID));

		format.dwChannelMask = KSAUDIO_SPEAKER_QUAD;
		sc->sn.numchannels = 4;
	}
	else if (sc->sn.numchannels >= 2)	//stereo
	{
		format.Format.wFormatTag = WAVE_FORMAT_PCM;
		format.Format.cbSize = 0;
		sc->sn.numchannels = 2;
	}
	else //mono time
	{
		format.Format.wFormatTag = WAVE_FORMAT_PCM;
		format.Format.cbSize = 0;
		sc->sn.numchannels = 1;
	}

	format.Format.nChannels = sc->sn.numchannels;
    format.Format.wBitsPerSample = sc->sn.samplebits;
    format.Format.nSamplesPerSec = sc->sn.speed;
    format.Format.nBlockAlign = format.Format.nChannels
		*format.Format.wBitsPerSample / 8;
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec
		*format.Format.nBlockAlign;

	if (!hInstDS)
	{
		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == NULL)
		{
			Con_SafePrintf ("Couldn't load dsound.dll\n");
			return SND_ERROR;
		}

		pDirectSoundCreate = (void *)GetProcAddress(hInstDS,"DirectSoundCreate");

		if (!pDirectSoundCreate)
		{
			Con_SafePrintf ("Couldn't get DS proc addr\n");
			return SND_ERROR;
		}

		pDirectSoundEnumerate = (void *)GetProcAddress(hInstDS,"DirectSoundEnumerateA");
	}

	dsnd_guids=0;
	dsndguid=NULL;
	dsndcard="DirectSound";
	if (pDirectSoundEnumerate)
		pDirectSoundEnumerate(&DSEnumCallback, NULL);
	if (!snd_usemultipledevices.ival)	//if only one device, ALWAYS use the default.
		dsndguid=NULL;

	aimedforguid++;

	if (!dsndguid)	//no more...
		if (aimedforguid != 1)	//not the first device.
			return SND_NOMORE;

	sc->handle = Z_Malloc(sizeof(dshandle_t));
	dh = sc->handle;
 //EAX attempt
#ifndef MINIMAL
#ifdef _IKsPropertySet_
	dh->pDS = NULL;
	if (snd_eax.ival)
	{
		CoInitialize(NULL);
		if (FAILED(CoCreateInstance( &CLSID_EAXDirectSound, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectSound, (void **)&dh->pDS )))
			dh->pDS=NULL;
		else
			IDirectSound_Initialize(dh->pDS, dsndguid);
	}

	if (!dh->pDS)
#endif
#endif
	{
		while ((hresult = iDirectSoundCreate(dsndguid, &dh->pDS, NULL)) != DS_OK)
		{
			if (hresult != DSERR_ALLOCATED)
			{
				Con_SafePrintf (": create failed\n");
				return SND_ERROR;
			}

//			if (MessageBox (NULL,
//							"The sound hardware is in use by another app.\n\n"
//							"Select Retry to try to start sound again or Cancel to run Quake with no sound.",
//							"Sound not available",
//							MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
//			{
				Con_SafePrintf (": failure\n"
								"  hardware already in use\n"
								"  Close the other app then use snd_restart\n");
				return SND_ERROR;
//			}
		}
	}
	Q_strncpyz(sc->name, dsndcard, sizeof(sc->name));

	dscaps.dwSize = sizeof(dscaps);

	if (DS_OK != dh->pDS->lpVtbl->GetCaps (dh->pDS, &dscaps))
	{
		Con_SafePrintf ("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_SafePrintf ("No DirectSound driver installed\n");
		DSOUND_Shutdown (sc);
		return SND_ERROR;
	}

	if (DS_OK != dh->pDS->lpVtbl->SetCooperativeLevel (dh->pDS, mainwindow, DSSCL_EXCLUSIVE))
	{
		Con_SafePrintf ("Set coop level failed\n");
		DSOUND_Shutdown (sc);
		return SND_ERROR;
	}


// get access to the primary buffer, if possible, so we can set the
// sound hardware format
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER|DSBCAPS_CTRLVOLUME;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

#ifdef DSBCAPS_GLOBALFOCUS
	if (snd_inactive.ival)
	{
		dsbuf.dwFlags |= DSBCAPS_GLOBALFOCUS;
		sc->inactive_sound = true;
	}
#endif

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

	if (!COM_CheckParm ("-snoforceformat"))
	{
		if (DS_OK == dh->pDS->lpVtbl->CreateSoundBuffer(dh->pDS, &dsbuf, &dh->pDSPBuf, NULL))
		{
			pformat = format;

			if (DS_OK != dh->pDSPBuf->lpVtbl->SetFormat (dh->pDSPBuf, (WAVEFORMATEX *)&pformat))
			{
//				if (snd_firsttime)
//					Con_SafePrintf ("Set primary sound buffer format: no\n");
			}
			else
//			{
//				if (snd_firsttime)
//					Con_SafePrintf ("Set primary sound buffer format: yes\n");

				primary_format_set = true;
//			}
		}
	}

	if (!primary_format_set || !COM_CheckParm ("-primarysound"))
	{
	// create the secondary buffer we'll actually work with
		memset (&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCSOFTWARE;	//dmw 29 may, 2003 removed locsoftware
#ifdef DSBCAPS_GLOBALFOCUS
		if (snd_inactive.ival)
		{
			dsbuf.dwFlags |= DSBCAPS_GLOBALFOCUS;
			sc->inactive_sound = true;
		}
#endif
		dsbuf.dwBufferBytes = sc->sn.samples / format.Format.nChannels;
		if (!dsbuf.dwBufferBytes)
		{
			dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
			// the fast rates will need a much bigger buffer
			if (format.Format.nSamplesPerSec > 48000)
				dsbuf.dwBufferBytes *= 4;
		}
		dsbuf.lpwfxFormat = (WAVEFORMATEX *)&format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		if (DS_OK != dh->pDS->lpVtbl->CreateSoundBuffer(dh->pDS, &dsbuf, &dh->pDSBuf, NULL))
		{
			Con_SafePrintf ("DS:CreateSoundBuffer Failed");
			DSOUND_Shutdown (sc);
			return SND_ERROR;
		}

		sc->sn.numchannels = format.Format.nChannels;
		sc->sn.samplebits = format.Format.wBitsPerSample;
		sc->sn.speed = format.Format.nSamplesPerSec;

		if (DS_OK != dh->pDSBuf->lpVtbl->GetCaps (dh->pDSBuf, &dsbcaps))
		{
			Con_SafePrintf ("DS:GetCaps failed\n");
			DSOUND_Shutdown (sc);
			return SND_ERROR;
		}

//		if (snd_firsttime)
//			Con_SafePrintf ("Using secondary sound buffer\n");
	}
	else
	{
		if (DS_OK != dh->pDS->lpVtbl->SetCooperativeLevel (dh->pDS, mainwindow, DSSCL_WRITEPRIMARY))
		{
			Con_SafePrintf ("Set coop level failed\n");
			DSOUND_Shutdown (sc);
			return SND_ERROR;
		}

		if (DS_OK != dh->pDSPBuf->lpVtbl->GetCaps (dh->pDSPBuf, &dsbcaps))
		{
			Con_Printf ("DS:GetCaps failed\n");
			DSOUND_Shutdown (sc);
			return SND_ERROR;
		}

		dh->pDSBuf = dh->pDSPBuf;
//		Con_SafePrintf ("Using primary sound buffer\n");
	}

	dh->gSndBufSize = dsbcaps.dwBufferBytes;

#if 1
	// Make sure mixer is active
	dh->pDSBuf->lpVtbl->Play(dh->pDSBuf, 0, 0, DSBPLAY_LOOPING);

/*	if (snd_firsttime)
		Con_SafePrintf("   %d channel(s)\n"
		               "   %d bits/sample\n"
					   "   %d bytes/sec\n",
					   shm->channels, shm->samplebits, shm->speed);*/


// initialize the buffer
	reps = 0;

	while ((hresult = dh->pDSBuf->lpVtbl->Lock(dh->pDSBuf, 0, dh->gSndBufSize, (void**)&buffer, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_SafePrintf ("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			DSOUND_Shutdown (sc);
			return SND_ERROR;
		}

		if (++reps > 10000)
		{
			Con_SafePrintf ("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			DSOUND_Shutdown (sc);
			return SND_ERROR;
		}
	}

	memset(buffer, 0, dwSize);
//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

//	Sleep(500);

	dh->pDSBuf->lpVtbl->Unlock(dh->pDSBuf, buffer, dwSize, NULL, 0);


	dh->pDSBuf->lpVtbl->Stop(dh->pDSBuf);
#endif
	dh->pDSBuf->lpVtbl->GetCurrentPosition(dh->pDSBuf, &dh->mmstarttime, &dwWrite);
	dh->pDSBuf->lpVtbl->Play(dh->pDSBuf, 0, 0, DSBPLAY_LOOPING);

	sc->sn.samples = dh->gSndBufSize/(sc->sn.samplebits/8);
	sc->sn.samplepos = 0;
	sc->sn.buffer = NULL;


	sc->Lock		= DSOUND_Lock;
	sc->Unlock		= DSOUND_Unlock;
	sc->SetWaterDistortion	= DSOUND_SetUnderWater;
	sc->Submit		= DSOUND_Submit;
	sc->Shutdown	= DSOUND_Shutdown;
	sc->GetDMAPos	= DSOUND_GetDMAPos;
	sc->Restore		= DSOUND_Restore;

#ifdef _IKsPropertySet_
	//attempt at eax support
	if (snd_eax.ival)
	{
		int r;
		DWORD support;

		if (SUCCEEDED(IDirectSoundBuffer_QueryInterface(dh->pDSBuf, &IID_IKsPropertySet, (void*)&dh->EaxKsPropertiesSet)))
		{
			r = IKsPropertySet_QuerySupport(dh->EaxKsPropertiesSet, &DSPROPSETID_EAX20_LISTENERPROPERTIES, DSPROPERTY_EAXLISTENER_ALLPARAMETERS, &support);
			if(!SUCCEEDED(r) || (support&(KSPROPERTY_SUPPORT_GET|KSPROPERTY_SUPPORT_SET))
					!= (KSPROPERTY_SUPPORT_GET|KSPROPERTY_SUPPORT_SET))
			{
				IKsPropertySet_Release(dh->EaxKsPropertiesSet);
				dh->EaxKsPropertiesSet = NULL;
				Con_SafePrintf ("EAX 2 not supported\n");
				return SND_LOADED;//otherwise successful. It can be used for normal sound anyway.
			}

			//worked. EAX is supported.
		}
		else
		{
			Con_SafePrintf ("Couldn't get extended properties\n");
			dh->EaxKsPropertiesSet = NULL;
		}
	}
#endif
	return SND_LOADED;
}
int (*pDSOUND_InitCard) (soundcardinfo_t *sc, int cardnum) = &DSOUND_InitCard;

#endif












#if defined(VOICECHAT) && defined(AVAIL_DSOUND) && !defined(__MINGW32__)



LPDIRECTSOUNDCAPTURE DSCapture;
LPDIRECTSOUNDCAPTUREBUFFER DSCaptureBuffer;
long lastreadpos;
long bufferbytes = 1024*1024;

long inputwidth = 2;

static WAVEFORMATEX  wfxFormat;

qboolean SNDDMA_InitCapture (void)
{
	DWORD capturePos;
	DSCBUFFERDESC bufdesc;

	wfxFormat.wFormatTag = WAVE_FORMAT_PCM;
    wfxFormat.nChannels = 1;
    wfxFormat.nSamplesPerSec = 11025;
	wfxFormat.wBitsPerSample = 8*inputwidth;
    wfxFormat.nBlockAlign = wfxFormat.nChannels * (wfxFormat.wBitsPerSample / 8);
	wfxFormat.nAvgBytesPerSec = wfxFormat.nSamplesPerSec * wfxFormat.nBlockAlign;
    wfxFormat.cbSize = 0;

	bufdesc.dwSize = sizeof(bufdesc);
	bufdesc.dwBufferBytes = bufferbytes;
	bufdesc.dwFlags = 0;
	bufdesc.dwReserved = 0;
	bufdesc.lpwfxFormat = &wfxFormat;

	if (DSCaptureBuffer)
	{
		IDirectSoundCaptureBuffer_Stop(DSCaptureBuffer);
		IDirectSoundCaptureBuffer_Release(DSCaptureBuffer);
		DSCaptureBuffer=NULL;
	}
	if (DSCapture)
	{
		IDirectSoundCapture_Release(DSCapture);
		DSCapture=NULL;
	}


	if (!hInstDS)
	{
		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == NULL)
		{
			Con_SafePrintf ("Couldn't load dsound.dll\n");
			return false;
		}

	}
	if (!pDirectSoundCaptureCreate)
	{
		pDirectSoundCaptureCreate = (void *)GetProcAddress(hInstDS,"DirectSoundCaptureCreate");

		if (!pDirectSoundCaptureCreate)
		{
			Con_SafePrintf ("Couldn't get DS proc addr\n");
			return false;
		}

//		pDirectSoundCaptureEnumerate = (void *)GetProcAddress(hInstDS,"DirectSoundCaptureEnumerateA");
	}
	pDirectSoundCaptureCreate(NULL, &DSCapture, NULL);

	if (FAILED(IDirectSoundCapture_CreateCaptureBuffer(DSCapture, &bufdesc, &DSCaptureBuffer, NULL)))
	{
		Con_SafePrintf ("Couldn't create a capture buffer\n");
		IDirectSoundCapture_Release(DSCapture);
		DSCapture=NULL;
		return false;
	}

	IDirectSoundCaptureBuffer_Start(DSCaptureBuffer, DSBPLAY_LOOPING);

	lastreadpos = 0;
	IDirectSoundCaptureBuffer_GetCurrentPosition(DSCaptureBuffer, &capturePos, &lastreadpos);
	return true;
}

/*minsamples is a hint*/
unsigned int DSOUND_UpdateCapture(qboolean enable, unsigned char *buffer, unsigned int minbytes, unsigned int maxbytes)
{
	HRESULT hr;
	LPBYTE lpbuf1 = NULL;
	LPBYTE lpbuf2 = NULL;
	DWORD dwsize1 = 0;
	DWORD dwsize2 = 0;

	DWORD capturePos;
	DWORD readPos;
	long  filled;

	if (!enable)
	{
		if (DSCaptureBuffer)
		{
			IDirectSoundCaptureBuffer_Stop(DSCaptureBuffer);
			IDirectSoundCaptureBuffer_Release(DSCaptureBuffer);
			DSCaptureBuffer=NULL;
		}
		if (DSCapture)
		{
			IDirectSoundCapture_Release(DSCapture);
			DSCapture=NULL;
		}
		return 0;
	}
	else if (!DSCaptureBuffer)
	{
		SNDDMA_InitCapture();
		return 0;
	}

// Query to see how much data is in buffer.
	hr = IDirectSoundCaptureBuffer_GetCurrentPosition( DSCaptureBuffer, &capturePos, &readPos );
	if( hr != DS_OK )
	{
		return 0;
	}
	filled = readPos - lastreadpos;
	if( filled < 0 ) filled += bufferbytes; // unwrap offset

	if (filled > maxbytes)	//figure out how much we need to empty it by, and if that's enough to be worthwhile.
		filled = maxbytes;
	else if (filled < minbytes)
		return 0;

//	filled /= inputwidth;
//	filled *= inputwidth;

	// Lock free space in the DS
	hr = IDirectSoundCaptureBuffer_Lock(DSCaptureBuffer, lastreadpos, filled, (void **) &lpbuf1, &dwsize1, (void **) &lpbuf2, &dwsize2, 0);
	if (hr == DS_OK)
	{
		// Copy from DS to the buffer
		memcpy(buffer, lpbuf1, dwsize1);
		if(lpbuf2 != NULL)
		{
			memcpy(buffer+dwsize1, lpbuf2, dwsize2);
		}
		// Update our buffer offset and unlock sound buffer
 		lastreadpos = (lastreadpos + dwsize1 + dwsize2) % bufferbytes;
		IDirectSoundCaptureBuffer_Unlock(DSCaptureBuffer, lpbuf1, dwsize1, lpbuf2, dwsize2);
	}
	else
	{
		return 0;
	}
	return filled;
}
unsigned int (*pDSOUND_UpdateCapture) (qboolean enable, unsigned char *buffer, unsigned int minbytes, unsigned int maxbytes) = &DSOUND_UpdateCapture;
#endif
