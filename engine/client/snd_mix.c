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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "quakedef.h"

#define	PAINTBUFFER_SIZE	2048

float voicevolumemod = 1;
portable_samplegroup_t paintbuffer[PAINTBUFFER_SIZE];

int 	*snd_p, snd_vol;
short	*snd_out;

void S_TransferPaintBuffer(soundcardinfo_t *sc, int endtime)
{
	unsigned int 	startidx, out_idx;
	unsigned int 	count;
	unsigned int 	outlimit;
	int 			*p;
	int				val;
	int				snd_vol;
	short			*pbuf;
	int				i, numc;

	p = (int *) paintbuffer;
	count = (endtime - sc->paintedtime) * sc->sn.numchannels;
	outlimit = sc->sn.samples;
	startidx = out_idx = (sc->paintedtime * sc->sn.numchannels) % outlimit;
	snd_vol = (volume.value*voicevolumemod)*256;
	numc = sc->sn.numchannels;

	pbuf = sc->Lock(sc);
	if (!pbuf)
		return;

	if (sc->sn.samplebits == 16)
	{
		short *out = (short *) pbuf;
		while (count)
		{
			for (i = 0; i < numc; i++)
			{
				val = (*p * snd_vol) >> 8;
				p++;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (short)0x8000)
					val = (short)0x8000;
				out[out_idx] = val;
				out_idx = (out_idx + 1) % outlimit;
			}
			p += MAXSOUNDCHANNELS - numc;
			count -= numc;
		}
	}
	else if (sc->sn.samplebits == 8)
	{
		unsigned char *out = (unsigned char *) pbuf;
		while (count)
		{
			for (i = 0; i < numc; i++)
			{
				val = (*p * snd_vol) >> 8;
				p++;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (short)0x8000)
					val = (short)0x8000;
				out[out_idx] = (val>>8) + 128;
				out_idx = (out_idx + 1) % outlimit;
			}
			p += MAXSOUNDCHANNELS - numc;
			count -= numc;
		}
	}

	sc->Unlock(sc, pbuf);
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int endtime);
void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int endtime);
void SND_PaintChannelFrom8_4Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom16_4Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom8_6Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom16_6Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom8_8Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom16_8Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom8Stereo (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom16Stereo (channel_t *ch, sfxcache_t *sc, int count);

void S_PaintChannels(soundcardinfo_t *sc, int endtime)
{
	int 	i, j;
	int 	end;
	channel_t *ch;
	sfxcache_t	*scache;
	sfx_t *s;
	int		ltime, count;

//	sc->rawstart += sc->paintedtime - sc->oldpaintedtime;
//	sc->oldpaintedtime = sc->paintedtime;
	while (sc->paintedtime < endtime)
	{
	// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - sc->paintedtime > PAINTBUFFER_SIZE)
			end = sc->paintedtime + PAINTBUFFER_SIZE;

	// clear the paint buffer
		Q_memset(paintbuffer, 0, (end - sc->paintedtime) * sizeof(portable_samplegroup_t));

	// paint in the channels.
		ch = sc->channel;
		for (i=0; i<sc->total_chans ; i++, ch++)
		{
			if (!ch->sfx)
				continue;
			if (!ch->vol[0] && !ch->vol[1] && !ch->vol[2] && !ch->vol[3] && !ch->vol[4] && !ch->vol[5])
				continue;

			scache = S_LoadSound (ch->sfx);
			if (!scache)
				continue;

			if ((ch->pos>>PITCHSHIFT) > scache->length)	//cache was flushed and gamedir changed.
			{
				ch->pos = scache->length*ch->rate;
				ch->end = sc->paintedtime;
			}


			ltime = sc->paintedtime;

			if (ch->sfx->decoder)
			{
				int len_diff;
				soundcardinfo_t *sndc;
#define qmax(x, y) (x>y)?(x):(y)
				len_diff = scache->length;
//				start = ch->end - scache->length;
//				samples = end - start;

#ifdef _MSC_VER
#pragma message("pitch fix needed")
#endif
				ch->sfx->decoder->decodemore(ch->sfx,
					end - (ch->end - scache->length) + 1);
//						ch->pos + end-ltime+1);

				scache = S_LoadSound (ch->sfx);
				if (!scache)
					continue;
				len_diff = scache->length - len_diff;

				for (sndc = sndcardinfo; sndc; sndc=sndc->next)
				{
					for (j = 0; j < sndc->total_chans; j++)
						if (sndc->channel[j].sfx == ch->sfx)	//extend all of these.
							ch->end += len_diff*ch->rate;
				}
			}

			while (ltime < end)
			{	// paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;


				if (count > 0)
				{
					if (ch->pos < 0)	//delay the sound a little
					{
						if (count > -ch->pos)
							count = -ch->pos;
						ltime += count;
						ch->pos += count*ch->rate;
						continue;
					}

					if (scache->width == 1)
					{
						if (scache->numchannels==2)
							SND_PaintChannelFrom8Stereo(ch, scache, count);
						else if (sc->sn.numchannels == 8)
							SND_PaintChannelFrom8_8Speaker(ch, scache, count);
						else if (sc->sn.numchannels == 6)
							SND_PaintChannelFrom8_6Speaker(ch, scache, count);
						else if (sc->sn.numchannels == 4)
							SND_PaintChannelFrom8_4Speaker(ch, scache, count);
						else
							SND_PaintChannelFrom8(ch, scache, count);
					}
					else
					{
						if (scache->numchannels==2)
							SND_PaintChannelFrom16Stereo(ch, scache, count);
						else if (sc->sn.numchannels == 8)
							SND_PaintChannelFrom16_8Speaker(ch, scache, count);
						else if (sc->sn.numchannels == 6)
							SND_PaintChannelFrom16_6Speaker(ch, scache, count);
						else if (sc->sn.numchannels == 4)
							SND_PaintChannelFrom16_4Speaker(ch, scache, count);
						else
							SND_PaintChannelFrom16(ch, scache, count);
					}
					ltime += count;
				}

			// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (scache->loopstart >= 0)
					{
						if (scache->length == scache->loopstart)
							break;
						ch->pos = scache->loopstart*ch->rate;
						ch->end = ltime + ((scache->length - scache->loopstart)<<PITCHSHIFT)/ch->rate;
						if (!scache->length)
						{
							scache->loopstart=-1;
							break;
						}
					}
					else if (ch->looping && scache->length)
					{
						ch->pos = 0;
						ch->end = ltime + ((scache->length)<<PITCHSHIFT)/ch->rate;
					}
					else
					{	// channel just stopped
						s = ch->sfx;
						ch->sfx = NULL;
						if (s->decoder)
						{
							if (!S_IsPlayingSomewhere(s))
								s->decoder->abort(s);
						}
						break;
					}
				}
			}
		}

	// transfer out according to DMA format
		S_TransferPaintBuffer(sc, end);
		sc->paintedtime = end;
	}
}

void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count)
{
	int 	data;
	signed char *sfx;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;

	if (ch->rate != (1<<PITCHSHIFT))
	{
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[ch->pos>>PITCHSHIFT];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += ch->vol[0] * data;
			paintbuffer[i].s[1] += ch->vol[1] * data;
		}
	}
	else
	{
		sfx = (signed char *)sc->data + (ch->pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			data = sfx[i];
			paintbuffer[i].s[0] += ch->vol[0] * data;
			paintbuffer[i].s[1] += ch->vol[1] * data;
		}
		ch->pos += count<<PITCHSHIFT;
	}
}

void SND_PaintChannelFrom8Stereo (channel_t *ch, sfxcache_t *sc, int count)
{
//	int 	data;
	signed char *sfx;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;

	if (ch->rate != (1<<PITCHSHIFT))
	{
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += ch->vol[0] * sfx[(ch->pos>>(PITCHSHIFT-1))&~1];
			paintbuffer[i].s[1] += ch->vol[1] * sfx[(ch->pos>>(PITCHSHIFT-1))|1];
			ch->pos += ch->rate;
		}
	}
	else
	{
		sfx = (signed char *)sc->data + (ch->pos>>PITCHSHIFT)*2;
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += ch->vol[0] * sfx[(i<<1)];
			paintbuffer[i].s[1] += ch->vol[1] * sfx[(i<<1)+1];
		}
		ch->pos += count<<PITCHSHIFT;
	}
}

void SND_PaintChannelFrom8_4Speaker (channel_t *ch, sfxcache_t *sc, int count)
{
	signed char *sfx;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;
	if (ch->vol[2] > 255)
		ch->vol[2] = 255;
	if (ch->vol[3] > 255)
		ch->vol[3] = 255;

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed char data;
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[ch->pos>>PITCHSHIFT];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += ch->vol[0] * data;
			paintbuffer[i].s[1] += ch->vol[1] * data;
			paintbuffer[i].s[2] += ch->vol[2] * data;
			paintbuffer[i].s[3] += ch->vol[3] * data;
		}
	}
	else
	{
		sfx = (signed char *)sc->data + (ch->pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += ch->vol[0] * sfx[i];
			paintbuffer[i].s[1] += ch->vol[1] * sfx[i];
			paintbuffer[i].s[2] += ch->vol[2] * sfx[i];
			paintbuffer[i].s[3] += ch->vol[3] * sfx[i];
		}
		ch->pos += count<<PITCHSHIFT;
	}
}

void SND_PaintChannelFrom8_6Speaker (channel_t *ch, sfxcache_t *sc, int count)
{
	signed char *sfx;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;
	if (ch->vol[2] > 255)
		ch->vol[2] = 255;
	if (ch->vol[3] > 255)
		ch->vol[3] = 255;
	if (ch->vol[4] > 255)
		ch->vol[4] = 255;
	if (ch->vol[5] > 255)
		ch->vol[5] = 255;

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed char data;
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[ch->pos>>PITCHSHIFT];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += ch->vol[0] * data;
			paintbuffer[i].s[1] += ch->vol[1] * data;
			paintbuffer[i].s[2] += ch->vol[2] * data;
			paintbuffer[i].s[3] += ch->vol[3] * data;
			paintbuffer[i].s[4] += ch->vol[4] * data;
			paintbuffer[i].s[5] += ch->vol[5] * data;
		}
	}
	else
	{
		sfx = (signed char *)sc->data + (ch->pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += ch->vol[0] * sfx[i];
			paintbuffer[i].s[1] += ch->vol[1] * sfx[i];
			paintbuffer[i].s[2] += ch->vol[2] * sfx[i];
			paintbuffer[i].s[3] += ch->vol[3] * sfx[i];
			paintbuffer[i].s[4] += ch->vol[4] * sfx[i];
			paintbuffer[i].s[5] += ch->vol[5] * sfx[i];
		}
		ch->pos += count<<PITCHSHIFT;
	}
}

void SND_PaintChannelFrom8_8Speaker (channel_t *ch, sfxcache_t *sc, int count)
{
	signed char *sfx;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;
	if (ch->vol[2] > 255)
		ch->vol[2] = 255;
	if (ch->vol[3] > 255)
		ch->vol[3] = 255;
	if (ch->vol[4] > 255)
		ch->vol[4] = 255;
	if (ch->vol[5] > 255)
		ch->vol[5] = 255;
	if (ch->vol[6] > 255)
		ch->vol[6] = 255;
	if (ch->vol[7] > 255)
		ch->vol[7] = 255;

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed char data;
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[ch->pos>>PITCHSHIFT];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += ch->vol[0] * data;
			paintbuffer[i].s[1] += ch->vol[1] * data;
			paintbuffer[i].s[2] += ch->vol[2] * data;
			paintbuffer[i].s[3] += ch->vol[3] * data;
			paintbuffer[i].s[4] += ch->vol[4] * data;
			paintbuffer[i].s[5] += ch->vol[5] * data;
			paintbuffer[i].s[6] += ch->vol[6] * data;
			paintbuffer[i].s[7] += ch->vol[7] * data;
		}
	}
	else
	{
		sfx = (signed char *)sc->data + (ch->pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += ch->vol[0] * sfx[i];
			paintbuffer[i].s[1] += ch->vol[1] * sfx[i];
			paintbuffer[i].s[2] += ch->vol[2] * sfx[i];
			paintbuffer[i].s[3] += ch->vol[3] * sfx[i];
			paintbuffer[i].s[4] += ch->vol[4] * sfx[i];
			paintbuffer[i].s[5] += ch->vol[5] * sfx[i];
			paintbuffer[i].s[6] += ch->vol[6] * sfx[i];
			paintbuffer[i].s[7] += ch->vol[7] * sfx[i];
		}
		ch->pos += count<<PITCHSHIFT;
	}
}


void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count)
{
	int data;
	int left, right;
	int leftvol, rightvol;
	signed short *sfx;
	int	i;

	leftvol = ch->vol[0];
	rightvol = ch->vol[1];

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed short data;
		sfx = (signed short *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[ch->pos>>PITCHSHIFT];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += (leftvol * data)>>8;
			paintbuffer[i].s[1] += (rightvol * data)>>8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + (ch->pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			data = sfx[i];
			left = (data * leftvol) >> 8;
			right = (data * rightvol) >> 8;
			paintbuffer[i].s[0] += left;
			paintbuffer[i].s[1] += right;
		}
		ch->pos += count<<PITCHSHIFT;
	}
}

void SND_PaintChannelFrom16Stereo (channel_t *ch, sfxcache_t *sc, int count)
{
	int leftvol, rightvol;
	signed short *sfx;
	int	i;

	leftvol = ch->vol[0];
	rightvol = ch->vol[1];

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed short l, r;
		sfx = (signed short *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			l = sfx[(ch->pos>>(PITCHSHIFT-1))&~1];
			r = sfx[(ch->pos>>(PITCHSHIFT-1))|1];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += (ch->vol[0] * l)>>8;
			paintbuffer[i].s[1] += (ch->vol[1] * r)>>8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + (ch->pos>>PITCHSHIFT)*2;
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += (*sfx++ * leftvol) >> 8;
			paintbuffer[i].s[1] += (*sfx++ * rightvol) >> 8;
		}
		ch->pos += count<<PITCHSHIFT;
	}
}

void SND_PaintChannelFrom16_4Speaker (channel_t *ch, sfxcache_t *sc, int count)
{
	int vol[4];
	signed short *sfx;
	int	i;

	vol[0] = ch->vol[0];
	vol[1] = ch->vol[1];
	vol[2] = ch->vol[2];
	vol[3] = ch->vol[3];

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed short data;
		sfx = (signed short *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[ch->pos>>PITCHSHIFT];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += (vol[0] * data)>>8;
			paintbuffer[i].s[1] += (vol[1] * data)>>8;
			paintbuffer[i].s[2] += (vol[2] * data)>>8;
			paintbuffer[i].s[3] += (vol[3] * data)>>8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + ch->pos;
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += (sfx[i] * vol[0]) >> 8;
			paintbuffer[i].s[1] += (sfx[i] * vol[1]) >> 8;
			paintbuffer[i].s[2] += (sfx[i] * vol[2]) >> 8;
			paintbuffer[i].s[3] += (sfx[i] * vol[3]) >> 8;
		}
		ch->pos += count<<PITCHSHIFT;
	}
}

void SND_PaintChannelFrom16_6Speaker (channel_t *ch, sfxcache_t *sc, int count)
{
	int vol[6];
	signed short *sfx;
	int	i;

	vol[0] = ch->vol[0];
	vol[1] = ch->vol[1];
	vol[2] = ch->vol[2];
	vol[3] = ch->vol[3];
	vol[4] = ch->vol[4];
	vol[5] = ch->vol[5];

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed short data;
		sfx = (signed short *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[ch->pos>>PITCHSHIFT];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += (vol[0] * data)>>8;
			paintbuffer[i].s[1] += (vol[1] * data)>>8;
			paintbuffer[i].s[2] += (vol[2] * data)>>8;
			paintbuffer[i].s[3] += (vol[3] * data)>>8;
			paintbuffer[i].s[4] += (vol[4] * data)>>8;
			paintbuffer[i].s[5] += (vol[5] * data)>>8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + (ch->pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += (sfx[i] * vol[0]) >> 8;
			paintbuffer[i].s[1] += (sfx[i] * vol[1]) >> 8;
			paintbuffer[i].s[2] += (sfx[i] * vol[2]) >> 8;
			paintbuffer[i].s[3] += (sfx[i] * vol[3]) >> 8;
			paintbuffer[i].s[4] += (sfx[i] * vol[4]) >> 8;
			paintbuffer[i].s[5] += (sfx[i] * vol[5]) >> 8;
		}
		ch->pos += count<<PITCHSHIFT;
	}
}

void SND_PaintChannelFrom16_8Speaker (channel_t *ch, sfxcache_t *sc, int count)
{
	int vol[8];
	signed short *sfx;
	int	i;

	vol[0] = ch->vol[0];
	vol[1] = ch->vol[1];
	vol[2] = ch->vol[2];
	vol[3] = ch->vol[3];
	vol[4] = ch->vol[4];
	vol[5] = ch->vol[5];
	vol[6] = ch->vol[6];
	vol[7] = ch->vol[7];

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed short data;
		sfx = (signed short *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[ch->pos>>PITCHSHIFT];
			ch->pos += ch->rate;
			paintbuffer[i].s[0] += (vol[0] * data)>>8;
			paintbuffer[i].s[1] += (vol[1] * data)>>8;
			paintbuffer[i].s[2] += (vol[2] * data)>>8;
			paintbuffer[i].s[3] += (vol[3] * data)>>8;
			paintbuffer[i].s[4] += (vol[4] * data)>>8;
			paintbuffer[i].s[5] += (vol[5] * data)>>8;
			paintbuffer[i].s[6] += (vol[6] * data)>>8;
			paintbuffer[i].s[7] += (vol[7] * data)>>8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + (ch->pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += (sfx[i] * vol[0]) >> 8;
			paintbuffer[i].s[1] += (sfx[i] * vol[1]) >> 8;
			paintbuffer[i].s[2] += (sfx[i] * vol[2]) >> 8;
			paintbuffer[i].s[3] += (sfx[i] * vol[3]) >> 8;
			paintbuffer[i].s[4] += (sfx[i] * vol[4]) >> 8;
			paintbuffer[i].s[5] += (sfx[i] * vol[5]) >> 8;
			paintbuffer[i].s[6] += (sfx[i] * vol[6]) >> 8;
			paintbuffer[i].s[7] += (sfx[i] * vol[7]) >> 8;
		}
		ch->pos += count<<PITCHSHIFT;
	}
}
