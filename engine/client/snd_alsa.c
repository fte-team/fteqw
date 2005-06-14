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


static unsigned int ALSA_GetDMAPos (soundcardinfo_t *sc)
{
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes = sc->sn.samples / sc->sn.numchannels;

	snd_pcm_avail_update (sc->handle);
	snd_pcm_mmap_begin (sc->handle, &areas, &offset, &nframes);
	offset *= sc->sn.numchannels;
	nframes *= sc->sn.numchannels;
	sc->sn.samplepos = offset;
	sc->sn.buffer = areas->addr;
	return sc->sn.samplepos;
}

static void ALSA_Shutdown (soundcardinfo_t *sc)
{
	snd_pcm_close (sc->handle);
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

	snd_pcm_avail_update (sc->handle);
	snd_pcm_mmap_begin (sc->handle, &areas, &offset, &nframes);

	state = snd_pcm_state (sc->handle);

	switch (state) {
		case SND_PCM_STATE_PREPARED:
			snd_pcm_mmap_commit (sc->handle, offset, nframes);
			snd_pcm_start (sc->handle);
			break;
		case SND_PCM_STATE_RUNNING:
			snd_pcm_mmap_commit (sc->handle, offset, nframes);
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

void S_UpdateCapture(void)
{
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

	snd_pcm_hw_params_alloca (&hw);
	snd_pcm_sw_params_alloca (&sw);

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

	err = snd_pcm_open (&pcm, pcmname, SND_PCM_STREAM_PLAYBACK,
						  SND_PCM_NONBLOCK);
	if (0 > err) {
		Con_Printf ("Error: audio open error: %s\n", snd_strerror (err));
		return 0;
	}
	Con_Printf ("ALSA: Using PCM %s.\n", pcmname);

	err = snd_pcm_hw_params_any (pcm, hw);
	if (0 > err) {
		Con_Printf ("ALSA: error setting hw_params_any. %s\n",
					snd_strerror (err));
		goto error;
	}

	err = snd_pcm_hw_params_set_access (pcm, hw,  SND_PCM_ACCESS_MMAP_INTERLEAVED);
	if (0 > err) {
		Con_Printf ("ALSA: Failure to set noninterleaved PCM access. %s\n"
					"Note: Interleaved is not supported\n",
					snd_strerror (err));
		goto error;
	}

	switch (bps) {
		case -1:
			err = snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16);
			if (0 <= err) {
				bps = 16;
			} else if (0 <= (err = snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_U8))) {
				bps = 8;
			} else {
				Con_Printf ("ALSA: no useable formats. %s\n",
							snd_strerror (err));
				goto error;
			}
			break;
		case 8:
		case 16:
			err = snd_pcm_hw_params_set_format (pcm, hw, bps == 8 ?
												  SND_PCM_FORMAT_U8 :
												  SND_PCM_FORMAT_S16);
			if (0 > err) {
				Con_Printf ("ALSA: no usable formats. %s\n",
							snd_strerror (err));
				goto error;
			}
			break;
		default:
			Con_Printf ("ALSA: desired format not supported\n");
			goto error;
	}

	switch (stereo) {
		case -1:
			err = snd_pcm_hw_params_set_channels (pcm, hw, 2);
			if (0 <= err) {
				stereo = 1;
			} else if (0 <= (err = snd_pcm_hw_params_set_channels (pcm, hw, 1))) {
				stereo = 0;
			} else {
				Con_Printf ("ALSA: no usable channels. %s\n",
							snd_strerror (err));
				goto error;
			}
			break;
		case 0:
		case 1:
			err = snd_pcm_hw_params_set_channels (pcm, hw, stereo ? 2 : 1);
			if (0 > err) {
				Con_Printf ("ALSA: no usable channels. %s\n",
							snd_strerror (err));
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
			err = snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
			if (0 <= err) {
				frag_size = 32 * bps;
			} else {
				rate = 22050;
				err = snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
				if (0 <= err) {
					frag_size = 16 * bps;
				} else {
					rate = 11025;
					err = snd_pcm_hw_params_set_rate_near (pcm, hw, &rate,
															 0);
					if (0 <= err) {
						frag_size = 8 * bps;
					} else {
						Con_Printf ("ALSA: no usable rates. %s\n",
									snd_strerror (err));
						goto error;
					}
				}
			}
			break;
		case 11025:
		case 22050:
		case 44100:
			err = snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
			if (0 > err) {
				Con_Printf ("ALSA: desired rate %i not supported. %s\n", rate,
							snd_strerror (err));
				goto error;
			}
			frag_size = 8 * bps * rate / 11025;
			break;
		default:
			Con_Printf ("ALSA: desired rate %i not supported.\n", rate);
			goto error;
	}

	err = snd_pcm_hw_params_set_period_size_near (pcm, hw, &frag_size, 0);
	if (0 > err) {
		Con_Printf ("ALSA: unable to set period size near %i. %s\n",
					(int) frag_size, snd_strerror (err));
		goto error;
	}
	err = snd_pcm_hw_params (pcm, hw);
	if (0 > err) {
		Con_Printf ("ALSA: unable to install hw params: %s\n",
					snd_strerror (err));
		goto error;
	}
	err = snd_pcm_sw_params_current (pcm, sw);
	if (0 > err) {
		Con_Printf ("ALSA: unable to determine current sw params. %s\n",
					snd_strerror (err));
		goto error;
	}
	err = snd_pcm_sw_params_set_start_threshold (pcm, sw, ~0U);
	if (0 > err) {
		Con_Printf ("ALSA: unable to set playback threshold. %s\n",
					snd_strerror (err));
		goto error;
	}
	err = snd_pcm_sw_params_set_stop_threshold (pcm, sw, ~0U);
	if (0 > err) {
		Con_Printf ("ALSA: unable to set playback stop threshold. %s\n",
					snd_strerror (err));
		goto error;
	}
	err = snd_pcm_sw_params (pcm, sw);
	if (0 > err) {
		Con_Printf ("ALSA: unable to install sw params. %s\n",
					snd_strerror (err));
		goto error;
	}

	sc->sn.numchannels = stereo + 1;
	sc->sn.samplepos = 0;
	sc->sn.samplebits = bps;

	err = snd_pcm_hw_params_get_buffer_size (hw, &buffer_size);
	if (0 > err) {
		Con_Printf ("ALSA: unable to get buffer size. %s\n",
					snd_strerror (err));
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
	snd_pcm_close (pcm);
	return false;
}

int (*pALSA_InitCard) (soundcardinfo_t *sc, int cardnum) = &ALSA_InitCard;


