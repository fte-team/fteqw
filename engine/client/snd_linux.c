#ifdef __CYGWIN__
#include "quakedef.h"
int SNDDMA_Init(soundcardinfo_t *sc)
{
	Con_Printf("Cygwin targets do not have sound. Sorry.\n");
	return 0;
}
void S_Init(void)
{
	Con_Printf("Cygwin targets do not have sound. Sorry.\n");
}
void S_Startup (void){}
void S_Restart_f (void){}
void SNDDMA_SetUnderWater(qboolean underwater) {}
void S_RawAudio(int sourceid, qbyte *data, int speed, int samples, int channels, int width){}
void S_Shutdown (void){}
void S_ShutdownCur (void){}
void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation){}
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation){}
void S_StopSound (int entnum, int entchannel){}
void S_StopAllSounds(qboolean clear){}
void S_ClearBuffer (soundcardinfo_t *sc){}
void S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up){}
void S_ExtraUpdate (void){}
sfx_t *S_PrecacheSound (char *sample)
{
	return NULL;
}
void S_TouchSound (char *sample){}
void S_LocalSound (char *s){}
void S_ClearPrecache (void){}
void S_BeginPrecaching (void){}
void S_EndPrecaching (void){}

cvar_t bgmvolume;
cvar_t volume;
cvar_t precache;

int snd_speed;
#else

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <linux/soundcard.h>
#include <stdio.h>
#include "quakedef.h"

qboolean	snd_firsttime = true;
static int tryrates[] = { 11025, 22051, 44100, 8000 };

soundcardinfo_t *sndcardinfo;

void SNDDMA_SetUnderWater(qboolean underwater)	//simply a stub. Any ideas how to actually implement this properly?
{
}

void S_UpdateCapture(void)	//any ideas how to get microphone input?
{
}

int SNDDMA_Init(soundcardinfo_t *sc)
{	//FIXME: implement snd_multipledevices somehow.
	int rc;
	int fmt;
	int tmp;
	int i;
	char *s;
	struct audio_buf_info info;
	int caps;
	char *snddev = NULL;
	cvar_t *devname;

	soundcardinfo_t *ec;

	devname = Cvar_Get("snd_devicename", "/dev/dsp", 0, "Sound controls");
	snddev = devname->string;

	for (ec = sndcardinfo; ec; ec = ec->next)
		if (!strcmp(ec->name, snddev))
			return 2;

	sc->inactive_sound = true;	//linux sound devices always play sound, even when we're not the active app...

// open the sound device, confirm capability to mmap, and get size of dma buffer

	Q_strncpyz(sc->name, snddev, sizeof(sc->name));
	printf("Initing sound device %s\n", snddev);

	sc->audio_fd = open(snddev, O_RDWR);
	if (sc->audio_fd < 0)
	{
		perror(snddev);
		Con_Printf("Could not open %s\n", snddev);
		SNDDMA_Shutdown(sc);
		return 0;
	}

	rc = ioctl(sc->audio_fd, SNDCTL_DSP_RESET, 0);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf("Could not reset %s\n", snddev);
		SNDDMA_Shutdown(sc);
		return 0;
	}

	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETCAPS, &caps)==-1)
	{
		perror(snddev);
		Con_Printf("Sound driver too old\n");
		SNDDMA_Shutdown(sc);
		return 0;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP))
	{
		Con_Printf("Sorry but your soundcard can't do this\n");
		SNDDMA_Shutdown(sc);
		return 0;
	}

	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETOSPACE, &info)==-1)
	{   
		perror("GETOSPACE");
		Con_Printf("Um, can't do GETOSPACE?\n");
		SNDDMA_Shutdown(sc);
		return 0;
	}
   
	sc->sn.splitbuffer = 0;

// set sample bits & speed

	s = getenv("QUAKE_SOUND_SAMPLEBITS");
	if (s) sc->sn.samplebits = atoi(s);
	else if ((i = COM_CheckParm("-sndbits")) != 0)
		sc->sn.samplebits = atoi(com_argv[i+1]);
	if (sc->sn.samplebits != 16 && sc->sn.samplebits != 8)
	{
		ioctl(sc->audio_fd, SNDCTL_DSP_GETFMTS, &fmt);
		if (fmt & AFMT_S16_LE)
			sc->sn.samplebits = 16;
		else if (fmt & AFMT_U8)
			sc->sn.samplebits = 8;
	}

	s = getenv("QUAKE_SOUND_SPEED");
	if (s) sc->sn.speed = atoi(s);
	else if ((i = COM_CheckParm("-sndspeed")) != 0)
		sc->sn.speed = atoi(com_argv[i+1]);
	else
	{
		if (!sc->sn.speed || ioctl(sc->audio_fd, SNDCTL_DSP_SPEED, &sc->sn.speed))	//use the default - menu set value.
		{	//humph, default didn't work. Go for random preset ones that should work.
			for (i=0 ; i<sizeof(tryrates)/4 ; i++)
				if (!ioctl(sc->audio_fd, SNDCTL_DSP_SPEED, &tryrates[i])) break;
			sc->sn.speed = tryrates[i];
		}
	}

	s = getenv("QUAKE_SOUND_CHANNELS");
	if (s) sc->sn.numchannels = atoi(s);
	else if ((i = COM_CheckParm("-sndmono")) != 0)
		sc->sn.numchannels = 1;
	else if ((i = COM_CheckParm("-sndstereo")) != 0)
		sc->sn.numchannels = 2;
	else sc->sn.numchannels = 2;

	sc->sn.samples = info.fragstotal * info.fragsize / (sc->sn.samplebits/8);
	sc->sn.submission_chunk = 1;

// memory map the dma buffer

	sc->sn.buffer = (unsigned char *) mmap(NULL, info.fragstotal
		* info.fragsize, PROT_WRITE, MAP_FILE|MAP_SHARED, sc->audio_fd, 0);
	if (!sc->sn.buffer)
	{
		perror(snddev);
		Con_Printf("Could not mmap %s\n", snddev);
		SNDDMA_Shutdown(sc);
		return 0;
	}

	tmp = 0;
	if (sc->sn.numchannels == 2)
		tmp = 1;
	rc = ioctl(sc->audio_fd, SNDCTL_DSP_STEREO, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf("Could not set %s to stereo=%d", snddev, sc->sn.numchannels);
		SNDDMA_Shutdown(sc);
		return 0;
	}
	if (tmp)
		sc->sn.numchannels = 2;
	else
		sc->sn.numchannels = 1;

	rc = ioctl(sc->audio_fd, SNDCTL_DSP_SPEED, &sc->sn.speed);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf("Could not set %s speed to %d", snddev, sc->sn.speed);
		SNDDMA_Shutdown(sc);
		return 0;
	}

	if (sc->sn.samplebits == 16)
    	{
		rc = AFMT_S16_LE;
		rc = ioctl(sc->audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0)
		{
			perror(snddev);
			Con_Printf("Could not support 16-bit data.  Try 8-bit.\n");
			SNDDMA_Shutdown(sc);
			return 0;
		}
	}
	else if (sc->sn.samplebits == 8)
	{
		rc = AFMT_U8;
		rc = ioctl(sc->audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0)
		{
			perror(snddev);
			Con_Printf("Could not support 8-bit data.\n");
			SNDDMA_Shutdown(sc);
			return 0;
		}
	}
	else
	{
		perror(snddev);
		Con_Printf("%d-bit sound not supported.", sc->sn.samplebits);
		SNDDMA_Shutdown(sc);
		return 0;
	}

// toggle the trigger & start her up

	tmp = 0;
	rc  = ioctl(sc->audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf("Could not toggle.\n");
		SNDDMA_Shutdown(sc);
		return 0;
	}
	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl(sc->audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf("Could not toggle.\n");
		SNDDMA_Shutdown(sc);
		return 0;
	}

	sc->sn.samplepos = 0;

	return 1;
}

int SNDDMA_GetDMAPos(soundcardinfo_t *sc)
{
	struct count_info count;

	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETOPTR, &count)==-1)
	{
		perror("/dev/dsp");
		Con_Printf("Uh, sound dead.\n");
		close(sc->audio_fd);
		return 0;
	}
//	shm->samplepos = (count.bytes / (shm->samplebits / 8)) & (shm->samples-1);
//	fprintf(stderr, "%d    \r", count.ptr);
	sc->sn.samplepos = count.ptr / (sc->sn.samplebits / 8);

	return sc->sn.samplepos;

}

void SNDDMA_Shutdown(soundcardinfo_t *sc)
{
	if (sc->sn.buffer)	//close it properly, so we can go and restart it later.
		munmap(sc->sn.buffer, sc->sn.samples * (sc->sn.samplebits/8));
	if (sc->audio_fd)
		close(sc->audio_fd);
	*sc->name = '\0';
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(soundcardinfo_t *sc)
{
}

#endif
