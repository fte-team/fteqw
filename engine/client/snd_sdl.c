#include "quakedef.h"
#include "winquake.h"

#include <SDL.h>

#define SELFPAINT

//SDL calls a callback each time it needs to repaint the 'hardware' buffers
//This results in extra latency.
//SDL runs does this multithreaded.
//So we tell it a fairly pathetically sized buffer and try and get it to copy often
//hopefully this lowers sound latency, and has no suddenly starting sounds and stuff.
//It still has greater latency than direct access, of course.

//FIXME: One thing I saw in quakeforge was that quakeforge basically leaves the audio locked except for a really short period of time.
//An interesting idea, which ensures the driver can only paint in a small time-frame. this would possibly allow lower latency painting.

static void SSDL_Shutdown(soundcardinfo_t *sc)
{
	Con_DPrintf("Shutdown SDL sound\n");
	SDL_CloseAudio();
#ifndef SELFPAINT
	if (sc->sn.buffer)
		free(sc->sn.buffer);
#endif
	sc->sn.buffer = NULL;
}
static unsigned int SSDL_GetDMAPos(soundcardinfo_t *sc)
{
	sc->sn.samplepos = sc->snd_sent / (sc->sn.samplebits/8);
	return sc->sn.samplepos;
}

//this function is called from inside SDL.
//transfer the 'dma' buffer into the buffer it requests.
static void VARGS SSDL_Paint(void *userdata, qbyte *stream, int len)
{
	soundcardinfo_t *sc = userdata;
	int buffersize = sc->sn.samples*(sc->sn.samplebits/8);

#ifdef SELFPAINT
	sc->sn.buffer = stream;
	sc->sn.samples = len / (sc->sn.samplebits/8);
	sc->samplequeue = sc->sn.samples;
	S_MixerThread(sc);
	sc->snd_sent += len;
#else
	if (len > buffersize)
	{
		len = buffersize;	//whoa nellie!
	}

	if (len + sc->snd_sent%buffersize > buffersize)
	{	//buffer will wrap, fill in the rest
		memcpy(stream, (char*)sc->sn.buffer + (sc->snd_sent%buffersize), buffersize - (sc->snd_sent%buffersize));
		stream += buffersize - sc->snd_sent%buffersize;
		sc->snd_sent += buffersize - sc->snd_sent%buffersize;
		len -= buffersize - (sc->snd_sent%buffersize);
		if (len < 0)
			return;
	}	//and finish from the start
	memcpy(stream, (char*)sc->sn.buffer + (sc->snd_sent%buffersize), len);
	sc->snd_sent += len;
#endif
}

static void *SSDL_LockBuffer(soundcardinfo_t *sc, unsigned int *sampidx)
{
	SDL_LockAudio();

	return sc->sn.buffer;
}

static void SSDL_UnlockBuffer(soundcardinfo_t *sc, void *buffer)
{
	SDL_UnlockAudio();
}

static void SSDL_SetUnderWater(soundcardinfo_t *sc, qboolean uw)
{
}


static void SSDL_Submit(soundcardinfo_t *sc, int start, int end)
{
	//SDL will call SSDL_Paint to paint when it's time, and the sound buffer is always there...
}

static int SDL_InitCard(soundcardinfo_t *sc, int cardnum)
{
	SDL_AudioSpec desired, obtained;

	if (cardnum)
	{	//our init code actually calls this function multiple times, in the case that the user has multiple sound cards
		return 2;	//erm. SDL won't allow multiple sound cards anyway.
	}

	Con_Printf("Initing SDL audio.\n");

	if(SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE))
	{
		Con_Printf("Couldn't initialize SDL audio subsystem\n");
		return false;
	}

	memset(&desired, 0, sizeof(desired));

	desired.freq = sc->sn.speed;
	desired.channels = sc->sn.numchannels;	//fixme!
	desired.samples = 0x0200;	//'Good values seem to range between 512 and 8192 inclusive, depending on the application and CPU speed.'
	desired.format = AUDIO_S16SYS;
	desired.callback = (void*)SSDL_Paint;
	desired.userdata = sc;
	memcpy(&obtained, &desired, sizeof(obtained));

#ifdef FTE_TARGET_WEB
	if ( SDL_OpenAudio(&desired, NULL) < 0 )
	{
		Con_Printf("SDL: SNDDMA_Init: couldn't open sound device (%s).\n", SDL_GetError());
		return false;
	}
	obtained = desired;
#else
	if ( SDL_OpenAudio(&desired, &obtained) < 0 )
	{
		Con_Printf("SDL: SNDDMA_Init: couldn't open sound device (%s).\n", SDL_GetError());
		return false;
	}
#endif
	sc->sn.numchannels = obtained.channels;
	sc->sn.speed = obtained.freq;
	sc->sn.samplebits = obtained.format&0xff;
	sc->sn.samples = 32768;//*sc->sn.numchannels;	//doesn't really matter, so long as it's higher than obtained.samples

#ifdef SELFPAINT
	sc->selfpainting = true;
#endif

	Con_DPrintf("channels: %i\n", sc->sn.numchannels);
	Con_DPrintf("Speed: %i\n", sc->sn.speed);
	Con_DPrintf("Samplebits: %i\n", sc->sn.samplebits);
	Con_DPrintf("SDLSamples: %i (low for latency)\n", obtained.samples);
	Con_DPrintf("FakeSamples: %i\n", sc->sn.samples);

#ifndef SELFPAINT
	sc->sn.buffer = malloc(sc->sn.samples*sc->sn.samplebits/8);
#endif
	Con_DPrintf("Got sound %i-%i\n", obtained.freq, obtained.format);

	sc->Lock		= SSDL_LockBuffer;
	sc->Unlock		= SSDL_UnlockBuffer;
	sc->SetWaterDistortion	= SSDL_SetUnderWater;
	sc->Submit		= SSDL_Submit;
	sc->Shutdown		= SSDL_Shutdown;
	sc->GetDMAPos		= SSDL_GetDMAPos;

	SDL_PauseAudio(0);

	return true;
}

sounddriver pSDL_InitCard = &SDL_InitCard;

