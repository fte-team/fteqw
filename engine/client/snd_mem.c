/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_mem.c: sound caching

#include "quakedef.h"

#include "winquake.h"

int			cache_full_cycle;

qbyte *S_Alloc (int size);

/*
================
ResampleSfx
================
*/
void ResampleSfx (sfx_t *sfx, int inrate, int inwidth, qbyte *data)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample, fracstep;
	unsigned int samplefrac;
	sfxcache_t	*sc;
	
	sc = Cache_Check (&sfx->cache);
	if (!sc)
		return;

	stepscale = (float)inrate / snd_speed;	// this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = snd_speed;
	if (loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;

	if (sc->stereo)		
	{
		if (stepscale == 1 && inwidth == 1 && sc->width == 1)
		{
			outcount*=2;
	// fast special case
			for (i=0 ; i<outcount ; i++)
				((signed char *)sc->data)[i]
				= (int)( (unsigned char)(data[i]) - 128);
		}
		else if (stepscale == 1 && inwidth == 2 && sc->width == 2)
		{
			outcount*=2;
	// fast special case
			for (i=0 ; i<outcount ; i++)
				((short *)sc->data)[i]	= LittleShort ( ((short *)data)[i] );
		}
		else
		{
	// general case			
			samplefrac = 0;
			fracstep = stepscale*256;
			for (i=0 ; i<outcount ; i++)
			{
				srcsample = samplefrac >> 8;
				samplefrac += fracstep;

				if (inwidth == 2)
					sample = LittleShort ( ((short *)data)[(srcsample<<1)] );
				else
					sample = (int)( (unsigned char)(data[(srcsample<<1)]) - 128) << 8;
				if (sc->width == 2)
					((short *)sc->data)[i<<1] = sample;
				else
					((signed char *)sc->data)[i<<1] = sample >> 8;

//				srcsample = samplefrac >> 8;
//				samplefrac += fracstep;
				if (inwidth == 2)
					sample = LittleShort ( ((short *)data)[(srcsample<<1)+1] );
				else
					sample = (int)( (unsigned char)(data[(srcsample<<1)+1]) - 128) << 8;
				if (sc->width == 2)
					((short *)sc->data)[(i<<1)+1] = sample;
				else
					((signed char *)sc->data)[(i<<1)+1] = sample >> 8;
			}
		}
		return;
	}

// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
// fast special case
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i]
			= (int)( (unsigned char)(data[i]) - 128);
	}
	else if (stepscale == 1 && inwidth == 2 && sc->width == 2)
	{
// fast special case
		for (i=0 ; i<outcount ; i++)
			((short *)sc->data)[i]	= LittleShort ( ((short *)data)[i] );
	}
	else
	{
// general case
		samplefrac = 0;
		fracstep = stepscale*256;
		for (i=0 ; i<outcount ; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort ( ((short *)data)[srcsample] );
			else
				sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

//=============================================================================

typedef sfxcache_t *(*S_LoadSound_t) (sfx_t *s, qbyte *data, int datalen, int sndspeed);

sfxcache_t *S_LoadWavSound (sfx_t *s, qbyte *data, int datalen, int sndspeed)
{
	wavinfo_t	info;
	int		len;
	sfxcache_t	*sc;

	info = GetWavinfo (s->name, data, datalen);
	if (info.numchannels < 1 || info.numchannels > 2)
	{
		Con_Printf ("%s has an unsupported quantity of channels.\n",s->name);
		return NULL;
	}

	len = (int) ((double) info.samples * (double) snd_speed / (double) info.rate);
	len = len * info.width * info.numchannels;

	sc = Cache_Alloc ( &s->cache, len + sizeof(sfxcache_t), s->name);
	if (!sc)
	{
		return NULL;
	}
	
	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.numchannels-1;

	ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	return sc;
}

sfxcache_t *S_LoadOVSound (sfx_t *s, qbyte *data, int datalen, int sndspeed);

S_LoadSound_t AudioInputPlugins[8] =
{
	S_LoadWavSound

#ifdef AVAIL_OGGVORBIS
//	, S_LoadOVSound
#endif
};

qboolean S_RegisterSoundInputPlugin(S_LoadSound_t loadfnc)
{
	int i;
	for (i = 0; i < sizeof(AudioInputPlugins)/sizeof(AudioInputPlugins[0]); i++)
	{
		if (!AudioInputPlugins[i])
		{
			AudioInputPlugins[i] = loadfnc;
			return true;
		}
	}
	return false;
}

/*
==============
S_LoadSound
==============
*/

sfxcache_t *S_LoadSound (sfx_t *s)
{
	char stackbuf[65536];
    char	namebuffer[256];
	qbyte	*data;
	sfxcache_t	*sc;
	int i;

	char *name = s->name;

// see if still in memory
	sc = Cache_Check (&s->cache);
	if (sc)
		return sc;

	s->decoder = NULL;




	if (name[1] == ':' && name[2] == '\\')
	{
		FILE *f;
#ifndef _WIN32	//convert from windows to a suitable alternative.
		char unixname[128];
		sprintf(unixname, "/mnt/%c/%s", name[0]-'A'+'a', name+3);
		name = unixname;
		while (*name)
		{
			if (*name == '\\')
				*name = '/';
			name++;
		}			
		name = unixname;
#endif

		if ((f = fopen(name, "rb")))
		{
			com_filesize = COM_filelength(f);
			data = Hunk_TempAlloc (com_filesize);
			fread(data, 1, com_filesize, f);
			fclose(f);
		}
		else
		{
			Con_SafePrintf ("Couldn't load %s\n", namebuffer);
			return NULL;
		}
	}
	else
	{
	//Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);
	// load it in

		if (*name == '*')
		{
			Q_strcpy(namebuffer, "players/male/");	//q2
			Q_strcat(namebuffer, name+1);	//q2
		}
		else if (name[0] == '.' && name[1] == '.' && name[2] == '/')
			Q_strcpy(namebuffer, name+3);
		else
		{
			Q_strcpy(namebuffer, "sound/");
			Q_strcat(namebuffer, name);
		}

	//	Con_Printf ("loading %s\n",namebuffer);

		data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));
	}

	if (!data)
	{
		//FIXME: check to see if qued for download.
		Con_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	for (i = sizeof(AudioInputPlugins)/sizeof(AudioInputPlugins[0])-1; i >= 0; i--)
	{
		if (AudioInputPlugins[i])
		{
			sc = AudioInputPlugins[i](s, data, com_filesize, snd_speed);
			if (sc)
				return sc;
		}
	}
	return NULL;
}



/*
===============================================================================

WAV loading

===============================================================================
*/


qbyte	*data_p;
qbyte 	*iff_end;
qbyte 	*last_chunk;
qbyte 	*iff_data;
int 	iff_chunk_len;


short GetLittleShort(void)
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

int GetLittleLong(void)
{
	int val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	val = val + (*(data_p+2)<<16);
	val = val + (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

void FindNextChunk(char *name)
{
	while (1)
	{
		data_p=last_chunk;
		data_p += 4;
		if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}
//		if (iff_chunk_len > 1024*1024)
//			Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!Q_strncmp(data_p, name, 4))
			return;
	}
}

void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}


#if 0
void DumpChunks(void)
{
	char	str[5];
	
	str[4] = 0;
	data_p=iff_data;
	do
	{
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf ("0x%x : %s (%d)\n", (int)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}
#endif

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo (char *name, qbyte *wav, int wavlength)
{
	wavinfo_t	info;
	int     i;
	int     format;
	int		samples;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;
		
	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !Q_strncmp(data_p+8, "WAVE", 4)))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;
// DumpChunks ();

	FindChunk("fmt ");
	if (!data_p)
	{
		Con_Printf("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Con_Printf("Microsoft PCM format only\n");
		return info;
	}

	info.numchannels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();
//		Con_Printf("loopstart=%d\n", sfx->loopstart);

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if (!strncmp (data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
//				Con_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Con_Printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width /info.numchannels;

	if (info.samples)
	{
		if (samples < info.samples)
			Sys_Error ("Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;
	
	return info;
}
