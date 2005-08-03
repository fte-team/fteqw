/*
	snd_alsa.c

	Support for the ALSA 1.0.1 sound driver

	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
//actually stolen from darkplaces.
//I guess noone can be arsed to write it themselves. :/

#include <alsa/asoundlib.h>

#include "quakedef.h"
#include <dlfcn.h>

static void *alsasharedobject;

int (*psnd_pcm_open)				(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode);
int (*psnd_pcm_close)				(snd_pcm_t *pcm);
const char *(*psnd_strerror)			(int errnum);
int (*psnd_pcm_hw_params_any)			(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int (*psnd_pcm_hw_params_set_access)		(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t _access);
int (*psnd_pcm_hw_params_set_format)		(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val);
int (*psnd_pcm_hw_params_set_channels)		(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
int (*psnd_pcm_hw_params_set_rate_near)		(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
int (*psnd_pcm_hw_params_set_period_size_near)	(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir);
int (*psnd_pcm_hw_params)			(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int (*psnd_pcm_sw_params_current)		(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int (*psnd_pcm_sw_params_set_start_threshold)	(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
int (*psnd_pcm_sw_params_set_stop_threshold)	(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
int (*psnd_pcm_sw_params)			(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int (*psnd_pcm_hw_params_get_buffer_size)	(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val);
snd_pcm_sframes_t (*psnd_pcm_avail_update)	(snd_pcm_t *pcm);
int (*psnd_pcm_mmap_begin)			(snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas, snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames);
snd_pcm_sframes_t (*psnd_pcm_mmap_commit)	(snd_pcm_t *pcm, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames);
snd_pcm_state_t (*psnd_pcm_state)		(snd_pcm_t *pcm);
int (*psnd_pcm_start)				(snd_pcm_t *pcm);

size_t (*psnd_pcm_hw_params_sizeof)		(void);
size_t (*psnd_pcm_sw_params_sizeof)		(void);



static unsigned int ALSA_GetDMAPos (soundcardinfo_t *sc)
{
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes = sc->sn.samples / sc->sn.numchannels;

	psnd_pcm_avail_update (sc->handle);
	psnd_pcm_mmap_begin (sc->handle, &areas, &offset, &nframes);
	offset *= sc->sn.numchannels;
	nframes *= sc->sn.numchannels;
	sc->sn.samplepos = offset;
	sc->sn.buffer = areas->addr;
	return sc->sn.samplepos;
}

static void ALSA_Shutdown (soundcardinfo_t *sc)
{
	psnd_pcm_close (sc->handle);
}

static void ALSA_Submit (soundcardinfo_t *sc)
{
	extern int soundtime;
	int			state;
	int			count = sc->paintedtime - soundtime;
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t nframes;
	snd_pcm_uframes_t offset;

	nframes = count / sc->sn.numchannels;

	psnd_pcm_avail_update (sc->handle);
	psnd_pcm_mmap_begin (sc->handle, &areas, &offset, &nframes);

	state = psnd_pcm_state (sc->handle);

	switch (state) {
		case SND_PCM_STATE_PREPARED:
			psnd_pcm_mmap_commit (sc->handle, offset, nframes);
			psnd_pcm_start (sc->handle);
			break;
		case SND_PCM_STATE_RUNNING:
			psnd_pcm_mmap_commit (sc->handle, offset, nframes);
			break;
		default:
			break;
	}
}

static void *ALSA_LockBuffer(soundcardinfo_t *sc)
{
	return sc->sn.buffer;
}

static void ALSA_UnlockBuffer(soundcardinfo_t *sc, void *buffer)
{
}

static void ALSA_SetUnderWater(soundcardinfo_t *sc, qboolean underwater)
{
}

static qboolean Alsa_InitAlsa(void)
{
	static qboolean tried;
	static qboolean alsaworks;
	if (tried)
		return alsaworks;
	tried = true;

	alsasharedobject = dlopen("libasound.so", RTLD_LAZY|RTLD_LOCAL);
	if (!alsasharedobject)
		return false;


	psnd_pcm_open				= dlsym(alsasharedobject, "snd_pcm_open");
	psnd_pcm_close				= dlsym(alsasharedobject, "snd_pcm_close");
	psnd_strerror				= dlsym(alsasharedobject, "snd_strerror");
	psnd_pcm_hw_params_any			= dlsym(alsasharedobject, "snd_pcm_hw_params_any");
	psnd_pcm_hw_params_set_access		= dlsym(alsasharedobject, "snd_pcm_hw_params_set_access");
	psnd_pcm_hw_params_set_format		= dlsym(alsasharedobject, "snd_pcm_hw_params_set_format");
	psnd_pcm_hw_params_set_channels		= dlsym(alsasharedobject, "snd_pcm_hw_params_set_channels");
	psnd_pcm_hw_params_set_rate_near	= dlsym(alsasharedobject, "snd_pcm_hw_params_set_rate_near");
	psnd_pcm_hw_params_set_period_size_near	= dlsym(alsasharedobject, "snd_pcm_hw_params_set_period_size_near");
	psnd_pcm_hw_params			= dlsym(alsasharedobject, "snd_pcm_hw_params");
	psnd_pcm_sw_params_current		= dlsym(alsasharedobject, "snd_pcm_sw_params_current");
	psnd_pcm_sw_params_set_start_threshold	= dlsym(alsasharedobject, "snd_pcm_sw_params_set_start_threshold");
	psnd_pcm_sw_params_set_stop_threshold	= dlsym(alsasharedobject, "snd_pcm_sw_params_set_stop_threshold");
	psnd_pcm_sw_params			= dlsym(alsasharedobject, "snd_pcm_sw_params");
	psnd_pcm_hw_params_get_buffer_size	= dlsym(alsasharedobject, "snd_pcm_hw_params_get_buffer_size");
	psnd_pcm_avail_update			= dlsym(alsasharedobject, "snd_pcm_avail_update");
	psnd_pcm_mmap_begin			= dlsym(alsasharedobject, "snd_pcm_mmap_begin");
	psnd_pcm_state				= dlsym(alsasharedobject, "snd_pcm_state");
	psnd_pcm_mmap_commit			= dlsym(alsasharedobject, "snd_pcm_mmap_commit");
	psnd_pcm_start				= dlsym(alsasharedobject, "snd_pcm_start");
	psnd_pcm_hw_params_sizeof		= dlsym(alsasharedobject, "snd_pcm_hw_params_sizeof");
	psnd_pcm_sw_params_sizeof		= dlsym(alsasharedobject, "snd_pcm_sw_params_sizeof");

	alsaworks = psnd_pcm_open
		&& psnd_pcm_close
		&& psnd_strerror
		&& psnd_pcm_hw_params_any
		&& psnd_pcm_hw_params_set_access
		&& psnd_pcm_hw_params_set_format
		&& psnd_pcm_hw_params_set_channels
		&& psnd_pcm_hw_params_set_rate_near
		&& psnd_pcm_hw_params_set_period_size_near
		&& psnd_pcm_hw_params
		&& psnd_pcm_sw_params_current
		&& psnd_pcm_sw_params_set_start_threshold
		&& psnd_pcm_sw_params_set_stop_threshold
		&& psnd_pcm_sw_params
		&& psnd_pcm_hw_params_get_buffer_size
		&& psnd_pcm_avail_update
		&& psnd_pcm_mmap_begin
		&& psnd_pcm_state
		&& psnd_pcm_mmap_commit
		&& psnd_pcm_start
		&& psnd_pcm_hw_params_sizeof
		&& psnd_pcm_sw_params_sizeof;

	return alsaworks;
}

static int ALSA_InitCard (soundcardinfo_t *sc, int cardnum)
{
	snd_pcm_t   *pcm;
	snd_pcm_uframes_t buffer_size;

	soundcardinfo_t *ec;	//existing card
	char *pcmname;
	cvar_t *devname;

	int					 err, i;
	int					 bps = -1, stereo = -1;
	unsigned int		 rate = 0;
	snd_pcm_hw_params_t	*hw;
	snd_pcm_sw_params_t	*sw;
	snd_pcm_uframes_t	 frag_size;

	if (!Alsa_InitAlsa())
		return 2;

	hw = alloca(psnd_pcm_hw_params_sizeof());
	sw = alloca(psnd_pcm_sw_params_sizeof());

	devname = Cvar_Get(va("snd_alsadevice%i", cardnum+1), cardnum==0?"default":"", 0, "Sound controls");
	pcmname = devname->string;

	if (!*pcmname)
		return 2;

	for (ec = sndcardinfo; ec; ec = ec->next)
		if (!strcmp(ec->name, pcmname))
			break;
	if (ec)
		return 2;	//no more

	sc->inactive_sound = true;	//linux sound devices always play sound, even when we're not the active app...

// COMMANDLINEOPTION: Linux ALSA Sound: -sndbits <number> sets sound precision to 8 or 16 bit (email me if you want others added)
	if ((i=COM_CheckParm("-sndbits")) != 0)
	{
		bps = atoi(com_argv[i+1]);
		if (bps != 16 && bps != 8)
		{
			Con_Printf("Error: invalid sample bits: %d\n", bps);
			return false;
		}
	}

// COMMANDLINEOPTION: Linux ALSA Sound: -sndspeed <hz> chooses 44100 hz, 22100 hz, or 11025 hz sound output rate
	if ((i=COM_CheckParm("-sndspeed")) != 0)
	{
		rate = atoi(com_argv[i+1]);
		if (rate!=44100 && rate!=22050 && rate!=11025)
		{
			Con_Printf("Error: invalid sample rate: %d\n", rate);
			return false;
		}
	}

// COMMANDLINEOPTION: Linux ALSA Sound: -sndmono sets sound output to mono
	if ((i=COM_CheckParm("-sndmono")) != 0)
		stereo=0;
// COMMANDLINEOPTION: Linux ALSA Sound: -sndstereo sets sound output to stereo
	if ((i=COM_CheckParm("-sndstereo")) != 0)
		stereo=1;

	err = psnd_pcm_open (&pcm, pcmname, SND_PCM_STREAM_PLAYBACK,
						  SND_PCM_NONBLOCK);
	if (0 > err) {
		Con_Printf ("Error: audio open error: %s\n", psnd_strerror (err));
		return 0;
	}
	Con_Printf ("ALSA: Using PCM %s.\n", pcmname);

	err = psnd_pcm_hw_params_any (pcm, hw);
	if (0 > err) {
		Con_Printf ("ALSA: error setting hw_params_any. %s\n",
					psnd_strerror (err));
		goto error;
	}

	err = psnd_pcm_hw_params_set_access (pcm, hw,  SND_PCM_ACCESS_MMAP_INTERLEAVED);
	if (0 > err) {
		Con_Printf ("ALSA: Failure to set noninterleaved PCM access. %s\n"
					"Note: Interleaved is not supported\n",
					psnd_strerror (err));
		goto error;
	}

	switch (bps) {
		case -1:
			err = psnd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16);
			if (0 <= err) {
				bps = 16;
			} else if (0 <= (err = psnd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_U8))) {
				bps = 8;
			} else {
				Con_Printf ("ALSA: no useable formats. %s\n",
							psnd_strerror (err));
				goto error;
			}
			break;
		case 8:
		case 16:
			err = psnd_pcm_hw_params_set_format (pcm, hw, bps == 8 ?
												  SND_PCM_FORMAT_U8 :
												  SND_PCM_FORMAT_S16);
			if (0 > err) {
				Con_Printf ("ALSA: no usable formats. %s\n",
							psnd_strerror (err));
				goto error;
			}
			break;
		default:
			Con_Printf ("ALSA: desired format not supported\n");
			goto error;
	}

	switch (stereo) {
		case -1:
			err = psnd_pcm_hw_params_set_channels (pcm, hw, 2);
			if (0 <= err) {
				stereo = 1;
			} else if (0 <= (err = psnd_pcm_hw_params_set_channels (pcm, hw, 1))) {
				stereo = 0;
			} else {
				Con_Printf ("ALSA: no usable channels. %s\n",
							psnd_strerror (err));
				goto error;
			}
			break;
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			err = psnd_pcm_hw_params_set_channels (pcm, hw, stereo+1);
			if (0 > err) {
				Con_Printf ("ALSA: no usable channels. %s\n",
							psnd_strerror (err));
				goto error;
			}
			break;
		default:
			Con_Printf ("ALSA: desired channels not supported\n");
			goto error;
	}

	switch (rate) {
		case 0:
			rate = 44100;
			err = psnd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
			if (0 <= err) {
				frag_size = 32 * bps;
			} else {
				rate = 22050;
				err = psnd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
				if (0 <= err) {
					frag_size = 16 * bps;
				} else {
					rate = 11025;
					err = psnd_pcm_hw_params_set_rate_near (pcm, hw, &rate,
															 0);
					if (0 <= err) {
						frag_size = 8 * bps;
					} else {
						Con_Printf ("ALSA: no usable rates. %s\n",
									psnd_strerror (err));
						goto error;
					}
				}
			}
			break;
		case 11025:
		case 22050:
		case 44100:
			err = psnd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
			if (0 > err) {
				Con_Printf ("ALSA: desired rate %i not supported. %s\n", rate,
							psnd_strerror (err));
				goto error;
			}
			frag_size = 8 * bps * rate / 11025;
			break;
		default:
			Con_Printf ("ALSA: desired rate %i not supported.\n", rate);
			goto error;
	}

	err = psnd_pcm_hw_params_set_period_size_near (pcm, hw, &frag_size, 0);
	if (0 > err) {
		Con_Printf ("ALSA: unable to set period size near %i. %s\n",
					(int) frag_size, psnd_strerror (err));
		goto error;
	}
	err = psnd_pcm_hw_params (pcm, hw);
	if (0 > err) {
		Con_Printf ("ALSA: unable to install hw params: %s\n",
					psnd_strerror (err));
		goto error;
	}
	err = psnd_pcm_sw_params_current (pcm, sw);
	if (0 > err) {
		Con_Printf ("ALSA: unable to determine current sw params. %s\n",
					psnd_strerror (err));
		goto error;
	}
	err = psnd_pcm_sw_params_set_start_threshold (pcm, sw, ~0U);
	if (0 > err) {
		Con_Printf ("ALSA: unable to set playback threshold. %s\n",
					psnd_strerror (err));
		goto error;
	}
	err = psnd_pcm_sw_params_set_stop_threshold (pcm, sw, ~0U);
	if (0 > err) {
		Con_Printf ("ALSA: unable to set playback stop threshold. %s\n",
					psnd_strerror (err));
		goto error;
	}
	err = psnd_pcm_sw_params (pcm, sw);
	if (0 > err) {
		Con_Printf ("ALSA: unable to install sw params. %s\n",
					psnd_strerror (err));
		goto error;
	}

	sc->sn.numchannels = stereo + 1;
	sc->sn.samplepos = 0;
	sc->sn.samplebits = bps;

	err = psnd_pcm_hw_params_get_buffer_size (hw, &buffer_size);
	if (0 > err) {
		Con_Printf ("ALSA: unable to get buffer size. %s\n",
					psnd_strerror (err));
		goto error;
	}

	sc->Lock		= ALSA_LockBuffer;
	sc->Unlock		= ALSA_UnlockBuffer;
	sc->SetWaterDistortion = ALSA_SetUnderWater;
	sc->Submit		= ALSA_Submit;
	sc->Shutdown	= ALSA_Shutdown;
	sc->GetDMAPos	= ALSA_GetDMAPos;

	sc->sn.samples = buffer_size * sc->sn.numchannels;		// mono samples in buffer
	sc->sn.speed = rate;
	sc->handle = pcm;
	ALSA_GetDMAPos (sc);		// sets shm->buffer

	return true;

error:
	psnd_pcm_close (pcm);
	return false;
}

int (*pALSA_InitCard) (soundcardinfo_t *sc, int cardnum) = &ALSA_InitCard;


