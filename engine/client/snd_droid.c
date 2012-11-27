/*
this file is basically a copy of the SDL one
java code has a function or two which just periodically calls us to ask us to dump out audio for it
*/
#include "quakedef.h"
#include <jni.h>
#include <pthread.h>

static soundcardinfo_t *sys_sc = NULL;
extern int sys_soundflags;

//called by the java code when it wants to know what sort of AudioTrack format to use.
JNIEXPORT jint JNICALL Java_com_fteqw_FTEDroidEngine_audioinfo(JNIEnv *env, jclass this, jint arg)
{
	soundcardinfo_t *sc = sys_sc;
	if (!sc)
		return 0;

	switch(arg)
	{
	case 1:
		return sc->sn.numchannels;
	case 2:
		return sc->sn.samplebits;
	default:
		return sc->sn.speed;
	}
}

//transfer the 'dma' buffer into the buffer it requests, called from a dedicated sound thread created by the java code.
JNIEXPORT jint JNICALL Java_com_fteqw_FTEDroidEngine_paintaudio(JNIEnv *env, jclass this, jbyteArray stream, jint len)
{
	int offset = 0;
	soundcardinfo_t *sc = sys_sc;
	int framesz;

	if (sc)
	{
		int buffersize = sc->sn.samples*sc->sn.samplebits/8;

		int curtime = GetSoundtime(sc);
		framesz = sc->sn.numchannels * sc->sn.samplebits/8;

		S_PaintChannels (sc, curtime + (len / framesz));

		if (len > buffersize)
		{
			len = buffersize;       //whoa nellie!
		}

		if (len + sc->snd_sent%buffersize > buffersize)
		{	//buffer will wrap, fill in the rest
			(*env)->SetByteArrayRegion(env, stream, offset, buffersize - (sc->snd_sent%buffersize), (char*)sc->sn.buffer + (sc->snd_sent%buffersize));
			offset += buffersize - (sc->snd_sent%buffersize);
			sc->snd_sent += buffersize - (sc->snd_sent%buffersize);
			len -= buffersize - (sc->snd_sent%buffersize);
			if (len < 0) /*this must be impossible, surely?*/
				len = 0;
		}
		//and finish from the start
		(*env)->SetByteArrayRegion(env, stream, offset, len, (char*)sc->sn.buffer + (sc->snd_sent%buffersize));
		offset += len;
		sc->snd_sent += len;
	}
	else
		offset = len;	/*so the playback thread ends up blocked properly*/
	return offset;
}


static void Droid_Shutdown(soundcardinfo_t *sc)
{
	//fixme: what if we're currently inside Java_com_fteqw_FTEDroidEngine_paintaudio?
	sys_sc = NULL;
	free(sc->sn.buffer);
	sys_soundflags = 0;
}

//return the number of samples that have already been submitted to the device.
static unsigned int Droid_GetDMAPos(soundcardinfo_t *sc)
{
	sc->sn.samplepos = sc->snd_sent / (sc->sn.samplebits/8);
	return sc->sn.samplepos;
}

static void Droid_UnlockBuffer(soundcardinfo_t *sc, void *buffer)
{
}

static void *Droid_LockBuffer(soundcardinfo_t *sc, unsigned int *sampidx)
{
	return sc->sn.buffer;
}

static void Droid_SetUnderWater(soundcardinfo_t *sc, qboolean uw)
{
}

static void Droid_Submit(soundcardinfo_t *sc, int start, int end)
{
}

//on android, 16bit audio is 'guarenteed'.
//8bit is not guarenteed.
//there's no reference to sample rates.	I assume 44.1khz will always work, though we want to avoid that cpu+mem load if we can
//nor any guarentee about channels supported. I assume mono will always work.
static int Droid_InitCard (soundcardinfo_t *sc, int cardnum)
{
	if (sys_sc)
		return 2;

	sc->selfpainting = true;
//	sc->sn.speed = 11025;
//	sc->sn.samplebits = 16;
//	sc->sn.numchannels = 1;

	/*internal buffer should have 1 sec audio*/
	sc->sn.samples = sc->sn.speed*sc->sn.numchannels;

	sc->Lock = Droid_LockBuffer;
	sc->Unlock = Droid_UnlockBuffer;
	sc->SetWaterDistortion = Droid_SetUnderWater;
	sc->Submit = Droid_Submit;
	sc->Shutdown = Droid_Shutdown;
	sc->GetDMAPos = Droid_GetDMAPos;

	sc->sn.buffer = malloc(sc->sn.samples*sc->sn.samplebits/8);
	
	sys_sc = sc;

	sys_soundflags = 3;

	return 1;
}
int (*pDroid_InitCard) (soundcardinfo_t *sc, int cardnum) = &Droid_InitCard;

