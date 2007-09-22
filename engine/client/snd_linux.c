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

static void OSS_SetUnderWater(soundcardinfo_t *sc, qboolean underwater)	//simply a stub. Any ideas how to actually implement this properly?
{
}

static unsigned int OSS_GetDMAPos(soundcardinfo_t *sc)
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
	struct audio_buf_info info;
	int caps;
	char *snddev = NULL;
	cvar_t *devname;

	devname = Cvar_Get(va("snd_devicename%i", cardnum+1), cardnum?"":"/dev/dsp", 0, "Sound controls");
	snddev = devname->string;

	if (!*snddev)
		return 2;

	sc->inactive_sound = true;	//linux sound devices always play sound, even when we're not the active app...

// open the sound device, confirm capability to mmap, and get size of dma buffer

	Con_Printf("Initing OSS sound device %s\n", snddev);

	sc->audio_fd = open(snddev, O_RDWR | O_NONBLOCK);	//try the primary device
	if (sc->audio_fd < 0)
	{
		perror(snddev);
		Con_Printf(SP_ERROR "OSS: Could not open %s\n", snddev);
		OSS_Shutdown(sc);
		return 0;
	}
	Q_strncpyz(sc->name, snddev, sizeof(sc->name));

	rc = ioctl(sc->audio_fd, SNDCTL_DSP_RESET, 0);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf(SP_ERROR "OSS: Could not reset %s\n", snddev);
		OSS_Shutdown(sc);
		return 0;
	}

	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETCAPS, &caps)==-1)
	{
		perror(snddev);
		Con_Printf(SP_ERROR "OSS: Sound driver too old\n");
		OSS_Shutdown(sc);
		return 0;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP))
	{
		Con_Printf(SP_ERROR "OSS: Sorry but your soundcard can't do this\n");
		OSS_Shutdown(sc);
		return 0;
	}

	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETOSPACE, &info)==-1)
	{
		perror("GETOSPACE");
		Con_Printf(SP_ERROR "OSS: Um, can't do GETOSPACE?\n");
		OSS_Shutdown(sc);
		return 0;
	}

// set sample bits & speed

	ioctl(sc->audio_fd, SNDCTL_DSP_GETFMTS, &fmt);
	if (!(fmt & AFMT_S16_LE) && sc->sn.samplebits > 8)
		sc->sn.samplebits = 8;
	else if (!(fmt & AFMT_U8))
	{
		Con_Printf(SP_ERROR "OSS: No needed sample formats supported\n");
		OSS_Shutdown(sc);
		return 0;
	}

	//use the default - menu set value.
	if (ioctl(sc->audio_fd, SNDCTL_DSP_SPEED, &sc->sn.speed))	
	{	//humph, default didn't work. Go for random preset ones that should work.
		for (i=0 ; i<sizeof(tryrates)/4 ; i++)
			if (!ioctl(sc->audio_fd, SNDCTL_DSP_SPEED, &tryrates[i])) break;
		if (i == (sizeof(tryrates)/4))
		{
			perror(snddev);
			Con_Printf(SP_ERROR "OSS: Failed to obtain a suitable rate\n");
			OSS_Shutdown(sc);
			return 0;
		}
		sc->sn.speed = tryrates[i];
	}

	if (sc->sn.samples > (info.fragstotal * info.fragsize * 4))
	{
		Con_Printf(SP_NOTICE "OSS: Enabling bigfoot's mmap hack! Hope you know what you're doing!\n");
		sc->sn.samples = info.fragstotal * info.fragsize * 4;
	}
	sc->sn.samples = info.fragstotal * info.fragsize;

// memory map the dma buffer

	sc->sn.buffer = (unsigned char *) mmap(NULL, sc->sn.samples, PROT_WRITE, MAP_FILE|MAP_SHARED, sc->audio_fd, 0);
	if (!sc->sn.buffer)
	{
		perror(snddev);
		Con_Printf(SP_ERROR "OSS: Could not mmap %s\n", snddev);
		OSS_Shutdown(sc);
		return 0;
	}

	sc->sn.samples /= (sc->sn.samplebits/8);

	tmp = 0;
	if (sc->sn.numchannels == 2)
		tmp = 1;
	rc = ioctl(sc->audio_fd, SNDCTL_DSP_STEREO, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf(SP_ERROR "OSS: Could not set %s to stereo=%d\n", snddev, sc->sn.numchannels);
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
		Con_Printf(SP_ERROR "OSS: Could not set %s speed to %d\n", snddev, sc->sn.speed);
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
			Con_Printf(SP_ERROR "OSS: Could not support 16-bit data.  Try 8-bit.\n");
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
			Con_Printf(SP_ERROR "OSS: Could not support 8-bit data.\n");
			OSS_Shutdown(sc);
			return 0;
		}
	}
	else
	{
		perror(snddev);
		Con_Printf(SP_ERROR "OSS: %d-bit sound not supported.\n", sc->sn.samplebits);
		OSS_Shutdown(sc);
		return 0;
	}

// toggle the trigger & start her up

	tmp = 0;
	rc  = ioctl(sc->audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf(SP_ERROR "OSS: Could not toggle.\n");
		OSS_Shutdown(sc);
		return 0;
	}
	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl(sc->audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf(SP_ERROR "OSS: Could not toggle.\n");
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

