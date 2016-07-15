#include "quakedef.h"

#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_audio.h>
#include <ppapi/c/ppb_audio_config.h>
extern PPB_Core *ppb_core;
extern PPB_Audio *audio_interface;
extern PPB_AudioConfig *audioconfig_interface;
extern PP_Instance pp_instance;

static PPB_Audio_Callback audio_callback;

extern int S_GetMixerTime(soundcardinfo_t *sc);

static void PPAPI_audio_callback(void *sample_buffer, uint32_t len, void *user_data)
{
	soundcardinfo_t *sc = user_data;
	unsigned int framesz;
	if (sc)
	{
		int curtime = S_GetMixerTime(sc);
		framesz = sc->sn.numchannels * sc->sn.samplebits/8;

		//might as well dump it directly...
		sc->sn.buffer = sample_buffer;
		sc->sn.samples = len / (sc->sn.samplebits/8);
		S_PaintChannels (sc, curtime + (len / framesz));
		sc->sn.samples = 0;
		sc->sn.buffer = NULL;

		sc->snd_sent += len;
	}
}

static void PPAPI_Shutdown(soundcardinfo_t *sc)
{
	audio_interface->StopPlayback((PP_Resource)sc->handle);
	ppb_core->ReleaseResource((PP_Resource)sc->handle);
}

static unsigned int PPAPI_GetDMAPos(soundcardinfo_t *sc)
{
	sc->sn.samplepos = sc->snd_sent / (sc->sn.samplebits/8);
	return sc->sn.samplepos;
}

static void PPAPI_UnlockBuffer(soundcardinfo_t *sc, void *buffer)
{
}

static void *PPAPI_LockBuffer(soundcardinfo_t *sc, unsigned int *sampidx)
{
	*sampidx = 0;
    return sc->sn.buffer;
}

static void PPAPI_SetUnderWater(soundcardinfo_t *sc, qboolean uw)
{
}

static void PPAPI_Submit(soundcardinfo_t *sc, int start, int end)
{
}

int PPAPI_InitCard (soundcardinfo_t *sc, int cardnum)
{
	PP_Resource config;
	int framecount;

	/*I'm not aware of any limits on the number of 'devices' we can create, but virtual devices on the same physical device are utterly pointless, so don't load more than one*/
	if (cardnum != 0)
		return 2;

	/*the docs say only two sample rates are allowed*/
	if (sc->sn.speed <= 44100)
		sc->sn.speed = 44100;
	else
		sc->sn.speed = 48000;
	/*we can't choose these two settings*/
	sc->sn.samplebits = 16;
	sc->sn.numchannels = 2;

#ifdef PPB_AUDIO_CONFIG_INTERFACE_1_1
	framecount = audioconfig_interface->RecommendSampleFrameCount(pp_instance, sc->sn.speed, 2048);
#else
	framecount = audioconfig_interface->RecommendSampleFrameCount(sc->sn.speed, 2048);
#endif

	/*the callback paints directly into the caller's buffer, so we don't need a separate 'dma' buffer*/
	sc->selfpainting = true;
	sc->sn.samples = 0;	/*framecount*/
	sc->sn.buffer = NULL;

	sc->snd_sent = 0;
	sc->sn.samplepos = 0;

	sc->Submit		= PPAPI_Submit;
	sc->GetDMAPos	= PPAPI_GetDMAPos;
	sc->Lock		= PPAPI_LockBuffer;
	sc->Unlock		= PPAPI_UnlockBuffer;
	sc->Shutdown	= PPAPI_Shutdown;


	config = audioconfig_interface->CreateStereo16Bit(pp_instance, sc->sn.speed, framecount);
	if (config)
	{
		sc->handle = (void*)audio_interface->Create(pp_instance, config, PPAPI_audio_callback, sc);
		ppb_core->ReleaseResource(config);
		if (sc->handle)
		{
			if (audio_interface->StartPlayback((PP_Resource)sc->handle))
				return 1;
		}
	}
	return 0;
}

int (*pPPAPI_InitCard) (soundcardinfo_t *sc, int cardnum) = &PPAPI_InitCard;