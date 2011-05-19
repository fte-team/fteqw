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

#ifdef __linux__
#include <sys/stat.h>
#endif

static int tryrates[] = { 11025, 22051, 44100, 8000, 48000 };

static void OSS_SetUnderWater(soundcardinfo_t *sc, qboolean underwater)	//simply a stub. Any ideas how to actually implement this properly?
{
}

static unsigned int OSS_MMap_GetDMAPos(soundcardinfo_t *sc)
{
	struct count_info count;

	if (sc->audio_fd != -1)
	{
		if (ioctl(sc->audio_fd, SNDCTL_DSP_GETOPTR, &count)==-1)
		{
			perror("/dev/dsp");
			Con_Printf("Uh, sound dead.\n");
			close(sc->audio_fd);
			sc->audio_fd = -1;
			return 0;
		}
//		shm->samplepos = (count.bytes / (shm->samplebits / 8)) & (shm->samples-1);
//		fprintf(stderr, "%d    \r", count.ptr);
		sc->sn.samplepos = count.ptr / (sc->sn.samplebits / 8);
	}
	return sc->sn.samplepos;

}
static void OSS_MMap_Submit(soundcardinfo_t *sc, int start, int end)
{
}

static unsigned int OSS_Alsa_GetDMAPos(soundcardinfo_t *sc)
{
	struct audio_buf_info info;
	unsigned int bytes;
	if (ioctl (sc->audio_fd, SNDCTL_DSP_GETOSPACE, &info) != -1)
	{
		bytes = sc->snd_sent + info.bytes;
		sc->sn.samplepos = bytes / (sc->sn.samplebits / 8);
	}
	return sc->sn.samplepos;
}


static void OSS_Alsa_Submit(soundcardinfo_t *sc, int start, int end)
{
	unsigned int bytes, offset, ringsize;
	unsigned chunk;
	int result;

	/*we can't change the data that was already written*/
	bytes = end * sc->sn.numchannels * (sc->sn.samplebits/8);
	bytes -= sc->snd_sent;
	if (!bytes)
		return;

	ringsize = sc->sn.samples * (sc->sn.samplebits/8);

	chunk = bytes;
	offset = sc->snd_sent % ringsize;

	if (offset + chunk >= ringsize)
		chunk = ringsize - offset;
	result = write(sc->audio_fd, sc->sn.buffer + offset, chunk);
	if (result < chunk)
	{
		if (result >= 0)
			sc->snd_sent += result;
		printf("full?\n");
		return;
	}
	sc->snd_sent += chunk;

	chunk = bytes - chunk;
	if (chunk)
	{
		result = write(sc->audio_fd, sc->sn.buffer, chunk);
		if (result > 0)
			sc->snd_sent += result;
	}
}

static void OSS_Shutdown(soundcardinfo_t *sc)
{
	if (sc->sn.buffer)	//close it properly, so we can go and restart it later.
	{
		if (sc->Submit == OSS_Alsa_Submit)
			free(sc->sn.buffer); /*if using alsa-compat, just free the buffer*/
		else
			munmap(sc->sn.buffer, sc->sn.samples * (sc->sn.samplebits/8));
	}
	if (sc->audio_fd != -1)
		close(sc->audio_fd);
	*sc->name = '\0';
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
	qboolean alsadetected = false;

#ifdef __linux__
	struct stat sb;
	if (stat("/proc/asound", &sb) != -1)
		alsadetected = true;
#endif

	devname = Cvar_Get(va("snd_devicename%i", cardnum+1), cardnum?"":"/dev/dsp", 0, "Sound controls");
	snddev = devname->string;

	if (!*snddev)
		return 2;

	sc->inactive_sound = true;	//linux sound devices always play sound, even when we're not the active app...

// open the sound device, confirm capability to mmap, and get size of dma buffer

	Con_Printf("Initing OSS sound device %s\n", snddev);

	sc->audio_fd = open(snddev, O_WRONLY | O_NONBLOCK);	//try the primary device
	if (sc->audio_fd < 0)
	{
		perror(snddev);
		Con_Printf(CON_ERROR "OSS: Could not open %s\n", snddev);
		OSS_Shutdown(sc);
		return 0;
	}
	Q_strncpyz(sc->name, snddev, sizeof(sc->name));

//reset it
	rc = ioctl(sc->audio_fd, SNDCTL_DSP_RESET, 0);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf(CON_ERROR "OSS: Could not reset %s\n", snddev);
		OSS_Shutdown(sc);
		return 0;
	}

//check its general capabilities, we need trigger+mmap
	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETCAPS, &caps)==-1)
	{
		perror(snddev);
		Con_Printf(CON_ERROR "OSS: Sound driver too old\n");
		OSS_Shutdown(sc);
		return 0;
	}

//choose channels
#ifdef SNDCTL_DSP_CHANNELS /*I'm paranoid, okay?*/
	tmp = sc->sn.numchannels;
	rc = ioctl(sc->audio_fd, SNDCTL_DSP_CHANNELS, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf(CON_ERROR "OSS: Could not set %s to channels=%d\n", snddev, sc->sn.numchannels);
		OSS_Shutdown(sc);
		return 0;
	}
	sc->sn.numchannels = tmp;
#else
	tmp = 0;
	if (sc->sn.numchannels == 2)
		tmp = 1;
	rc = ioctl(sc->audio_fd, SNDCTL_DSP_STEREO, &tmp);
	if (rc < 0)
	{
		perror(snddev);
		Con_Printf(CON_ERROR "OSS: Could not set %s to stereo=%d\n", snddev, sc->sn.numchannels);
		OSS_Shutdown(sc);
		return 0;
	}
	if (tmp)
		sc->sn.numchannels = 2;
	else
		sc->sn.numchannels = 1;
#endif

//choose bits
	// ask the device what it supports
	ioctl(sc->audio_fd, SNDCTL_DSP_GETFMTS, &fmt);
	if (!(fmt & AFMT_S16_LE) && sc->sn.samplebits > 8)
		sc->sn.samplebits = 8;	// they asked for 16bit (the default) but their card does not support it
	if (!(fmt & AFMT_U8) && sc->sn.samplebits == 8)
	{	//their card doesn't support 8bit which we're trying to use.
		Con_Printf(CON_ERROR "OSS: No needed sample formats supported\n");
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
			Con_Printf(CON_ERROR "OSS: Could not support 16-bit data.  Try 8-bit.\n");
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
			Con_Printf(CON_ERROR "OSS: Could not support 8-bit data.\n");
			OSS_Shutdown(sc);
			return 0;
		}
	}
	else
	{
		perror(snddev);
		Con_Printf(CON_ERROR "OSS: %d-bit sound not supported.\n", sc->sn.samplebits);
		OSS_Shutdown(sc);
		return 0;
	}

//choose speed
	//use the default - menu set value.
	tmp = sc->sn.speed;
	if (ioctl(sc->audio_fd, SNDCTL_DSP_SPEED, &tmp) != 0)
	{	//humph, default didn't work. Go for random preset ones that should work.
		for (i=0 ; i<sizeof(tryrates)/4 ; i++)
		{
			tmp = tryrates[i];
			if (!ioctl(sc->audio_fd, SNDCTL_DSP_SPEED, &tmp)) break;
		}
		if (i == (sizeof(tryrates)/4))
		{
			perror(snddev);
			Con_Printf(CON_ERROR "OSS: Failed to obtain a suitable rate\n");
			OSS_Shutdown(sc);
			return 0;
		}
	}
	sc->sn.speed = tmp;

//figure out buffer size
	if (ioctl(sc->audio_fd, SNDCTL_DSP_GETOSPACE, &info)==-1)
	{
		perror("GETOSPACE");
		Con_Printf(CON_ERROR "OSS: Um, can't do GETOSPACE?\n");
		OSS_Shutdown(sc);
		return 0;
	}
	sc->sn.samples = info.fragstotal * info.fragsize;
	sc->sn.samples /= (sc->sn.samplebits/8);
	/*samples is the number of samples*channels */

// memory map the dma buffer
	sc->sn.buffer = MAP_FAILED;
	if (alsadetected)
	{
		Con_Printf("Alsa detected. Refusing to mmap.\n");
	}
	else if ((caps & DSP_CAP_TRIGGER) && (caps & DSP_CAP_MMAP))
	{
		sc->sn.buffer = (unsigned char *) mmap(NULL, sc->sn.samples*(sc->sn.samplebits/8), PROT_WRITE, MAP_FILE|MAP_SHARED, sc->audio_fd, 0);
		if (sc->sn.buffer == MAP_FAILED)
		{
			Con_Printf("%s: device reported mmap capability, but mmap failed.\n", snddev);
			if (alsadetected)
			{
				char *f, *n;
				f = com_argv[0];
				while((n = strchr(f, '/')))
					f = n + 1;
				Con_Printf("Your system is running alsa.\nTry: sudo echo \"%s 0 0 direct\" > /proc/asound/card0/pcm0p/oss\n", f);
			}
		}
	}
	if (sc->sn.buffer == MAP_FAILED)
	{
		sc->sn.buffer = NULL;

		sc->samplequeue = info.bytes / (sc->sn.samplebits/8);
		sc->sn.samples*=2;
		sc->sn.buffer = malloc(sc->sn.samples*(sc->sn.samplebits/8));
		sc->Submit		= OSS_Alsa_Submit;
		sc->GetDMAPos	= OSS_Alsa_GetDMAPos;
	}
	else
	{
		// toggle the trigger & start her up
		tmp = 0;
		rc  = ioctl(sc->audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
		if (rc < 0)
		{
			perror(snddev);
			Con_Printf(CON_ERROR "OSS: Could not toggle.\n");
			OSS_Shutdown(sc);
			return 0;
		}
		tmp = PCM_ENABLE_OUTPUT;
		rc = ioctl(sc->audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
		if (rc < 0)
		{
			perror(snddev);
			Con_Printf(CON_ERROR "OSS: Could not toggle.\n");
			OSS_Shutdown(sc);
			return 0;
		}
		sc->Submit		= OSS_MMap_Submit;
		sc->GetDMAPos	= OSS_MMap_GetDMAPos;
	}

	sc->sn.samplepos = 0;

	sc->Lock		= OSS_Lock;
	sc->Unlock		= OSS_Unlock;
	sc->SetWaterDistortion = OSS_SetUnderWater;
	sc->Shutdown	= OSS_Shutdown;

	return 1;
}

int (*pOSS_InitCard) (soundcardinfo_t *sc, int cardnum) = &OSS_InitCard;



#if 0//I'm unable to test due to alsa    def VOICECHAT
#include <stdint.h>
void *OSS_Capture_Init(int rate)
{
	int tmp;
	intptr_t fd;
	char *snddev = "/dev/dsp";
	fd = open(snddev, O_RDONLY | O_NONBLOCK);       //try the primary device
	if (fd == -1)
		return NULL;

#ifdef SNDCTL_DSP_CHANNELS
	tmp = 1;
	if (ioctl(fd, SNDCTL_DSP_CHANNELS, &tmp) != 0)
#else
	tmp = 0;
	if (ioctl(fd, SNDCTL_DSP_STEREO, &tmp) != 0)
#endif
	{
		Con_Printf("Couldn't set mono\n");
		perror(snddev);
	}

	tmp = AFMT_S16_LE;
	if (ioctl(fd, SNDCTL_DSP_SETFMT, &tmp) != 0)
	{
		Con_Printf("Couldn't set sample bits\n");
		perror(snddev);
	}

	tmp = rate;
	if (ioctl(fd, SNDCTL_DSP_SPEED, &tmp) != 0)
	{
		Con_Printf("Couldn't set capture rate\n");
		perror(snddev);
	}

	fd++;
	return (void*)fd;
}
void OSS_Capture_Start(void *ctx)
{
	/*oss will automagically restart it when we next read*/
}

void OSS_Capture_Stop(void *ctx)
{
	intptr_t fd = ((intptr_t)ctx)-1;

	ioctl(fd, SNDCTL_DSP_RESET, NULL);
}

void OSS_Capture_Shutdown(void *ctx)
{
	intptr_t fd = ((intptr_t)ctx)-1;

	close(fd);
}

unsigned int OSS_Capture_Update(void *ctx, unsigned char *buffer, unsigned int minbytes, unsigned int maxbytes)
{
	intptr_t fd = ((intptr_t)ctx)-1;
	ssize_t res;

	res = read(fd, buffer, maxbytes);
	if (res < 0)
		return 0;
	return res;
}

snd_capture_driver_t OSS_Capture =
{
	OSS_Capture_Init,
	OSS_Capture_Start,
	OSS_Capture_Update,
	OSS_Capture_Stop,
	OSS_Capture_Shutdown
};
#endif

