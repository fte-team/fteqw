#include "quakedef.h"
#include "winquake.h"

#include <SDL.h>

extern cvar_t snd_khz;

#define	SOUND_BUFFER_SIZE			0x0400

int snd_inited;

soundcardinfo_t *sndcardinfo;

int snd_firsttime = 0;

int aimedforguid;

void SNDDMA_Submit(soundcardinfo_t *sc)
{
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
	return 0;
}


void S_UpdateCapture(void)	//any ideas how to get microphone input?
{
}

int SNDDMA_Init(soundcardinfo_t *sc)
{
	SDL_AudioSpec desired, obtained;
	
	MessageBox(NULL, "hello", "fnar", 0);

	if(SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		Con_Print("Couldn't initialize SDL audio subsystem\n");
		MessageBox(NULL, "hjkl", "fnar", 0);
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
	desired.callback = paint;
	
	if ( SDL_OpenAudio(&desired, &obtained) < 0 )
	{
		Con_Printf("SDL: SNDDMA_Init: couldn't open sound device (%s).\n", SDL_GetError());
		MessageBox(NULL, "hello", "fghjfghjfgfnar", 0);
		return false;
	}
	snd_inited = true;
	SDL_PauseAudio(0);
	MessageBox(NULL, "he;'lk'khjllo", "fnghkfghar", 0);
	return true;
}

void SNDDMA_SetUnderWater(qboolean underwater)
{
}
