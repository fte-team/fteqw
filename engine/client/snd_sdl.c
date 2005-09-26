#include "quakedef.h"
#include "winquake.h"

#include <SDL.h>

#define	SOUND_BUFFER_SIZE			0x0400

static void SSDL_Shutdown(soundcardinfo_t *sc)
{
Con_Printf("Shutdown SDL sound\n");
	SDL_CloseAudio();
Con_Printf("buffer\n");
	if (sc->sn.buffer)
		free(sc->sn.buffer);
	sc->sn.buffer = NULL;
Con_Printf("down\n");
}
static unsigned int SSDL_GetDMAPos(soundcardinfo_t *sc)
{
	sc->sn.samplepos = (sc->snd_sent / (sc->sn.samplebits/8)) % sc->sn.samples;
	return sc->sn.samplepos;
}

//this function is called from inside SDL.
//transfer the 'dma' buffer into the buffer it requests.
static void SSDL_Paint(void *userdata, qbyte *stream, int len)
{
	soundcardinfo_t *sc = userdata;

	if (len > SOUND_BUFFER_SIZE)
		len = SOUND_BUFFER_SIZE;	//whoa nellie!
	if (len > SOUND_BUFFER_SIZE - sc->snd_sent)
	{	//buffer will wrap, fill in the rest
		memcpy(stream, sc->sn.buffer + sc->snd_sent, SOUND_BUFFER_SIZE - sc->snd_sent);
		len -= SOUND_BUFFER_SIZE - sc->snd_sent;
		sc->snd_sent = 0;
	}	//and finish from the start
	memcpy(stream, sc->sn.buffer + sc->snd_sent, len);
	sc->snd_sent += len;
}

static void *SSDL_LockBuffer(soundcardinfo_t *sc)
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


static void SSDL_Submit(soundcardinfo_t *sc)
{
	//SDL will call SSDL_Paint to paint when it's time, and the sound buffer is always there...
}


void S_UpdateCapture(void)	//any ideas how to get microphone input?
{
}

static int SDL_InitCard(soundcardinfo_t *sc, int cardnum)
{
	SDL_AudioSpec desired, obtained;

	if (cardnum)
	{	//our init code actually calls this function multiple times, in the case that the user has multiple sound cards
		return 2;	//erm. SDL won't allow multiple sound cards anyway.
	}

Con_Printf("SDL AUDIO INITING\n");
	if(SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE))
	{
		Con_Printf("Couldn't initialize SDL audio subsystem\n");
		return false;
	}

	memset(&desired, 0, sizeof(desired));

	desired.freq = sc->sn.speed;
	desired.channels = 2;
	desired.samples = SOUND_BUFFER_SIZE;
	desired.format = AUDIO_S16;
	desired.callback = SSDL_Paint;
	desired.userdata = sc;


	if ( SDL_OpenAudio(&desired, &obtained) < 0 )
	{
		Con_Printf("SDL: SNDDMA_Init: couldn't open sound device (%s).\n", SDL_GetError());
		return false;
	}
	sc->sn.numchannels = obtained.channels;
	sc->sn.speed = desired.freq;
	sc->sn.samplebits = 16;
	sc->sn.samples = SOUND_BUFFER_SIZE;
	sc->sn.buffer = malloc(SOUND_BUFFER_SIZE*sc->sn.samplebits/8);
	Con_Printf("Got sound %i-%i\n", obtained.freq, obtained.format);
	SDL_PauseAudio(0);

	sc->Lock		= SSDL_LockBuffer;
	sc->Unlock		= SSDL_UnlockBuffer;
	sc->SetWaterDistortion	= SSDL_SetUnderWater;
	sc->Submit		= SSDL_Submit;
	sc->Shutdown		= SSDL_Shutdown;
	sc->GetDMAPos		= SSDL_GetDMAPos;

	return true;
}

sounddriver pSDL_InitCard = &SDL_InitCard;
