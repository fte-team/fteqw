#include "quakedef.h"
#include "winquake.h"

#include <SDL.h>

extern cvar_t snd_khz;

#define	SOUND_BUFFER_SIZE			0x0400

int snd_inited;

soundcardinfo_t *sndcardinfo;

int snd_firsttime = 0;

int aimedforguid;

//lamocodec.
static char buffer[SOUND_BUFFER_SIZE];
int sndpos;

void SNDDMA_Submit(soundcardinfo_t *sc)
{	//We already wrote it into the 'dma' buffer (heh, the closest we can get to it at least)
	//so we now wait for sdl to request it.
	//yes, this can result in slow sound.
}
void SNDDMA_Shutdown(soundcardinfo_t *sc)
{
	if (snd_inited)
	{
		snd_inited = false;
		SDL_CloseAudio();
	}
}
int SNDDMA_GetDMAPos(soundcardinfo_t *sc)
{
	sc->sn.samplepos = (sndpos / (sc->sn.samplebits/8)) % sc->sn.samples;
	return sc->sn.samplepos;
}

void SNDDMA_Paint(void *userdata, qbyte *stream, int len)
{
	if (len > SOUND_BUFFER_SIZE)
		len = SOUND_BUFFER_SIZE;	//whoa nellie!
	if (len > SOUND_BUFFER_SIZE - sndpos)
	{	//buffer will wrap, fill in the rest
		memcpy(stream, buffer + sndpos, SOUND_BUFFER_SIZE - sndpos);
		len -= SOUND_BUFFER_SIZE - sndpos;
		sndpos = 0;
	}	//and finish from the start
	memcpy(stream, buffer + sndpos, len);
	sndpos += len;
}


void S_UpdateCapture(void)	//any ideas how to get microphone input?
{
}

int SNDDMA_Init(soundcardinfo_t *sc)
{
	SDL_AudioSpec desired, obtained;

	if (snd_inited)
	{	//our init code actually calls this function multiple times, in the case that the user has multiple sound cards
//		Con_Printf("Sound was already inited\n");
		return 2;	//erm. SDL won't allow multiple sound cards anyway.
	}
	
Con_Printf("SDL AUDIO INITING\n");
	if(SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		Con_Print("Couldn't initialize SDL audio subsystem\n");
		return false;
	}

	memset(&desired, 0, sizeof(desired));

	if (!sc->sn.speed)
	{
		if (snd_khz.value >= 45)
			sc->sn.speed = 48000;
		else if (snd_khz.value >= 30)	//set by a slider
			sc->sn.speed = 44100;
		else if (snd_khz.value >= 20)
			sc->sn.speed = 22050;
		else
			sc->sn.speed = 11025;
	}

	desired.freq = sc->sn.speed;
	desired.channels = 2;
	desired.samples = SOUND_BUFFER_SIZE;
	desired.format = AUDIO_S16;
	desired.callback = SNDDMA_Paint;
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
	sc->sn.buffer = buffer;
	Con_Printf("Got sound %i-%i\n", obtained.freq, obtained.format);
	snd_inited = true;
	SDL_PauseAudio(0);
	return true;
}

void SNDDMA_SetUnderWater(qboolean underwater)
{
}

