#include "quakedef.h"

soundcardinfo_t *sndcardinfo;

int snd_firsttime = 0;

int aimedforguid;

void SNDDMA_Submit(soundcardinfo_t *sc)
{
}
void SNDDMA_Shutdown(soundcardinfo_t *sc)
{
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
	Con_Printf("SDL has no sound code\n");
	return 0;
}

void SNDDMA_SetUnderWater(qboolean underwater)
{
}
