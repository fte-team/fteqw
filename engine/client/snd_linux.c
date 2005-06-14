#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/soundcard.h>
#include <stdio.h>
#include "quakedef.h"

static int tryrates[] = { 11025, 22051, 44100, 8000 };

static void OSS_SetUnderWater(qboolean underwater)	//simply a stub. Any ideas how to actually implement this properly?
{
}

static int OSS_GetDMAPos(soundcardinfo_t *sc)
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

static void OSS_Shutdown(soundcardinfo_t *sc)
{
	if (sc->sn.buffer)	//close it properly, so we can go and restart it later.
		munmap(sc->sn.buffer, sc->sn.samples * (sc->sn.samplebits/8));
	if (sc->audio_fd)
		close(sc->audio_fd);
	*sc->name = '\0';
}

static void OSS_Submit(soundcardinfo_t *sc)
{
}

static void *OSS_Lock(soundcardinfo_t *sc)
{
	return sc->sn.buffer;
}

static void OSS_Unlock(soundcardinfo_t *sc, void *buffer)
{
}

static int OSS_InitCard(soundcardinfo_t *sc, int cardnum)
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

	printf("Initing sound device %s\n", snddev);

	sc->audio_fd = open(snddev, O_RDWR | O_NONBLOCK);	//try the primary device
	if (sc->audio_fd < 0)
	{
		perror(snddev);
		Con_Printf("Could not open %s\n", snddev);

		devname = Cvar_Get("snd_devicename2", "", 0, "Sound controls");
		snddev = devname->string;
		if (*snddev)	//try a secondary if they named one
		{
			printf("Initing sound device %s\n", snddev);
			sc->audio_fd = open(snddev, O_RDWR | O_NONBLOCK);

			if (sc->audio_fd < 0)
				Con_Printf("Could not open %s\n", snddev);
		}

		if (sc->audio_fd < 0)
		{
			Con_Printf("Running without sound\n");
			OSS_Shutdown(sc);
			return 0;
		}
	}
	Q_strncpyz(sc->name, snddev, sizeof(sc->name));

	rc = ioctl(sc->audio_fd, SNDCTL_DSP_RESET, 0);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf("Could not reset %s\n", snddev);
		OSS_Shutdown(sc);
		return 0;
	}

	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETCAPS, &caps)==-1)
	{
		perror(snddev);
		Con_Printf("Sound driver too old\n");
		OSS_Shutdown(sc);
		return 0;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP))
	{
		Con_Printf("Sorry but your soundcard can't do this\n");
		OSS_Shutdown(sc);
		return 0;
	}

	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETOSPACE, &info)==-1)
	{
		perror("GETOSPACE");
		Con_Printf("Um, can't do GETOSPACE?\n");
		OSS_Shutdown(sc);
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
	if (s)
		sc->sn.speed = atoi(s);
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
		OSS_Shutdown(sc);
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
		OSS_Shutdown(sc);
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
		OSS_Shutdown(sc);
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
			OSS_Shutdown(sc);
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
			OSS_Shutdown(sc);
			return 0;
		}
	}
	else
	{
		perror(snddev);
		Con_Printf("%d-bit sound not supported.", sc->sn.samplebits);
		OSS_Shutdown(sc);
		return 0;
	}

// toggle the trigger & start her up

	tmp = 0;
	rc  = ioctl(sc->audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf("Could not toggle.\n");
		OSS_Shutdown(sc);
		return 0;
	}
	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl(sc->audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf("Could not toggle.\n");
		OSS_Shutdown(sc);
		return 0;
	}

	sc->sn.samplepos = 0;

	sc->Lock		= OSS_Lock;
	sc->Unlock		= OSS_Unlock;
	sc->SetWaterDistortion = OSS_SetUnderWater;
	sc->Submit		= OSS_Submit;
	sc->Shutdown	= OSS_Shutdown;
	sc->GetDMAPos	= OSS_GetDMAPos;

	return 1;
}

int (*pOSS_InitCard) (soundcardinfo_t *sc, int cardnum) = &OSS_InitCard;



void S_UpdateCapture(void)
{
}
