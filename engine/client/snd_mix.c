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

#ifdef HAVE_MIXER

#define	PAINTBUFFER_SIZE	2048

portable_samplegroup_t paintbuffer[PAINTBUFFER_SIZE];	//FIXME: we really ought to be using SSE and floats or something.

int 	*snd_p, snd_vol;
short	*snd_out;

void S_TransferPaintBuffer(soundcardinfo_t *sc, int endtime)
{
	unsigned int 	out_idx;
	unsigned int 	count;
	unsigned int 	outlimit;
	int 			*p;
	int				val;
//	int				snd_vol;
	short			*pbuf;
	int				i, numc;

	p = (int *) paintbuffer;
	count = (endtime - sc->paintedtime) * sc->sn.numchannels;
	outlimit = sc->sn.samples;
	out_idx = (sc->paintedtime * sc->sn.numchannels) % outlimit;
//	snd_vol = (volume.value*voicevolumemod)*256;
	numc = sc->sn.numchannels;

	pbuf = sc->Lock(sc, &out_idx);
	if (!pbuf)
		return;

	switch(sc->sn.sampleformat)
	{
	case QSF_INVALID:	//erk...
	case QSF_EXTERNALMIXER:	//shouldn't reach this.
		break;
	case QSF_U8:
		{
			unsigned char *out = (unsigned char *) pbuf;
			while (count)
			{
				for (i = 0; i < numc; i++)
				{
					val = *p++;// * snd_vol) >> 8;
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
		break;
	case QSF_S8:
		{
			char *out = (char *) pbuf;
			while (count)
			{
				for (i = 0; i < numc; i++)
				{
					val = *p++;// * snd_vol) >> 8;
					if (val > 0x7fff)
						val = 0x7fff;
					else if (val < (short)0x8000)
						val = (short)0x8000;
					out[out_idx] = (val>>8);
					out_idx = (out_idx + 1) % outlimit;
				}
				p += MAXSOUNDCHANNELS - numc;
				count -= numc;
			}
		}
		break;
	case QSF_S16:
		{
			short *out = (short *) pbuf;
			while (count)
			{
				for (i = 0; i < numc; i++)
				{
					val = *p++;// * snd_vol) >> 8;
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
		break;
	case QSF_F32:
		{
			float *out = (float *) pbuf;
			while (count)
			{
				for (i = 0; i < numc; i++)
				{
					out[out_idx] = *p++ * (1.0 / 32768);
					out_idx = (out_idx + 1) % outlimit;
				}
				p += MAXSOUNDCHANNELS - numc;
				count -= numc;
			}
		}
		break;
	}

	sc->Unlock(sc, pbuf);
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

static void SND_PaintChannel8_O2I1	(channel_t *ch, sfxcache_t *sc, int starttime, int count);
static void SND_PaintChannel16_O2I1	(channel_t *ch, sfxcache_t *sc, int starttime, int count);
static void SND_PaintChannel8_O4I1	(channel_t *ch, sfxcache_t *sc, int count);
static void SND_PaintChannel16_O4I1	(channel_t *ch, sfxcache_t *sc, int count);
static void SND_PaintChannel8_O6I1	(channel_t *ch, sfxcache_t *sc, int count);
static void SND_PaintChannel16_O6I1	(channel_t *ch, sfxcache_t *sc, int count);
static void SND_PaintChannel8_O8I1	(channel_t *ch, sfxcache_t *sc, int count);
static void SND_PaintChannel16_O8I1	(channel_t *ch, sfxcache_t *sc, int count);
static void SND_PaintChannel8_O2I2	(channel_t *ch, sfxcache_t *sc, int starttime, int count);
static void SND_PaintChannel16_O2I2	(channel_t *ch, sfxcache_t *sc, int starttime, int count);

//NOTE: MAY NOT CALL SYS_ERROR
void S_PaintChannels(soundcardinfo_t *sc, int endtime)
{
	int 	i;
	int 	end;
	channel_t *ch;
	sfxcache_t	scachebuf;
	sfxcache_t	*scache;
	sfx_t *s;
	int		ltime, count;
	int avail;
	unsigned int maxlen = ruleset_allow_overlongsounds.ival?0xffffffffu>>PITCHSHIFT:snd_speed*20;

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
			s = ch->sfx;
			if (!s || s->loadstate == SLS_LOADING)
				continue;
			if (!ch->vol[0] && !ch->vol[1] && !ch->vol[2] && !ch->vol[3] && !ch->vol[4] && !ch->vol[5])
			{
				//does it still make a sound if it cannot be heard?...
				//technically no...
				//this code is hacky.
				if (s->decoder.querydata)
					s->decoder.querydata(s, scache=&scachebuf, NULL, 0);
				else if (s->decoder.decodedata)
					scache = s->decoder.decodedata(s, &scachebuf, ch->pos>>PITCHSHIFT, 0);	/*1 for luck - balances audio termination below*/
				else
					scache = s->decoder.buf;
				ch->pos += (end-sc->paintedtime)*ch->rate;
				if (!scache || (ch->pos>>PITCHSHIFT) > scache->soundoffset+scache->length)
				{
					ch->pos = 0;
					if (s->loopstart != -1)
						ch->pos = s->loopstart<<PITCHSHIFT;
					else if (!(ch->flags & CF_FORCELOOP))
					{
						ch->sfx = NULL;
						if (s->decoder.ended)
						{
							if (!S_IsPlayingSomewhere(s))
								s->decoder.ended(s);
						}
					}
				}
				continue;
			}

			ltime = sc->paintedtime;
			while (ltime < end)
			{
				ssamplepos_t spos = ch->pos>>PITCHSHIFT;
				if (s->decoder.decodedata)
					scache = s->decoder.decodedata(s, &scachebuf, spos, 1 + (((end - ltime) * ch->rate)>>PITCHSHIFT));	/*1 for luck - balances audio termination below*/
				else if (s->decoder.buf)
				{
					scache = s->decoder.buf;
					if (spos >= scache->length)
						scache = NULL;	//EOF
				}
				else
					scache = NULL;

				if (!scache)
				{	//hit eof, loop it or stop it
					if (s->loopstart != -1)	/*some wavs contain a loop offset directly in the sound file, such samples loop even if a non-looping builtin was used*/
					{
						ch->pos &= ~((~0u)<<PITCHSHIFT);	/*clear out all but the subsample offset*/
						ch->pos += s->loopstart<<PITCHSHIFT;	//ignore the offset if its off the end of the file
					}
					else if (ch->flags & CF_FORCELOOP)	/*(static)channels which are explicitly looping always loop from the start*/
					{
						/*restart it*/
						ch->pos = 0;
					}
					else
					{	// channel just stopped
						ch->sfx = NULL;
						if (s->decoder.ended)
						{
							if (!S_IsPlayingSomewhere(s))
								s->decoder.ended(s);
						}
						break;
					}

					//retry at that new offset (continue here could give an infinite loop)
					spos = ch->pos>>PITCHSHIFT;
					if (s->decoder.decodedata)
						scache = s->decoder.decodedata(s, &scachebuf, spos, 1 + (((end - ltime) * ch->rate)>>PITCHSHIFT));	/*1 for luck - balances audio termination below*/
					else if (s->decoder.buf)
					{
						scache = s->decoder.buf;
						if (spos >= scache->length)
							scache = NULL;	//EOF
					}
					else
						scache = NULL;

					if (!scache)
						break;
				}

				if (spos < scache->soundoffset || spos > scache->soundoffset+scache->length)
					avail = 0;	//urm, we would be trying to read outside of the buffer. let mixing slip when there's no data available yet.
				else
				{
					// find how many samples till the sample ends (clamp max length)
					avail = scache->length;
					if (avail > maxlen)
						avail = snd_speed*10;
					avail = (((int)(scache->soundoffset + avail)<<PITCHSHIFT) - ch->pos + (ch->rate-1)) / ch->rate;
				}
				// mix the smaller of how much is available or the time left
				count = min(avail, end - ltime);

				if (count > 0)
				{
					if (ch->pos < 0)	//sounds with a pos of 0 are delay-start sounds
					{
						//don't progress past 0, so it actually starts properly at the right time with no clicks or anything
						if (count > (-ch->pos+255)>>PITCHSHIFT)
							count = ((-ch->pos+255)>>PITCHSHIFT);
						ltime += count;
						ch->pos += count*ch->rate;
						continue;
					}

					if (scache->width == 1)
					{
						if (scache->numchannels==2)
							SND_PaintChannel8_O2I2(ch, scache, ltime-sc->paintedtime, count);
						else if (sc->sn.numchannels <= 2)
							SND_PaintChannel8_O2I1(ch, scache, ltime-sc->paintedtime, count);
						else if (sc->sn.numchannels <= 4)
							SND_PaintChannel8_O4I1(ch, scache, count);
						else if (sc->sn.numchannels <= 6)
							SND_PaintChannel8_O6I1(ch, scache, count);
						else
							SND_PaintChannel8_O8I1(ch, scache, count);
					}
					else if (scache->width == 2)
					{
						if (scache->numchannels==2)
							SND_PaintChannel16_O2I2(ch, scache, ltime-sc->paintedtime, count);
						else if (sc->sn.numchannels <= 2)
							SND_PaintChannel16_O2I1(ch, scache, ltime-sc->paintedtime, count);							
						else if (sc->sn.numchannels <= 4)
							SND_PaintChannel16_O4I1(ch, scache, count);
						else if (sc->sn.numchannels <= 6)
							SND_PaintChannel16_O6I1(ch, scache, count);
						else
							SND_PaintChannel16_O8I1(ch, scache, count);
					}
					ltime += count;
					ch->pos += ch->rate * count;
				}
				else
					break;
			}
		}

	// transfer out according to DMA format
		S_TransferPaintBuffer(sc, end);
		sc->paintedtime = end;
	}
}

static void SND_PaintChannel8_O2I1 (channel_t *ch, sfxcache_t *sc, int starttime, int count)
{
	int 	data;
	signed char *sfx;
	int		i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

	if (ch->rate != (1<<PITCHSHIFT))
	{
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[pos>>PITCHSHIFT];
			pos += ch->rate;
			paintbuffer[starttime+i].s[0] += ch->vol[0] * data;
			paintbuffer[starttime+i].s[1] += ch->vol[1] * data;
		}
	}
	else
	{
		sfx = (signed char *)sc->data + (pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			data = sfx[i];
			paintbuffer[starttime+i].s[0] += ch->vol[0] * data;
			paintbuffer[starttime+i].s[1] += ch->vol[1] * data;
		}
	}
}

static void SND_PaintChannel8_O2I2 (channel_t *ch, sfxcache_t *sc, int starttime, int count)
{
//	int 	data;
	signed char *sfx;
	int		i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

	if (ch->rate != (1<<PITCHSHIFT))
	{
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[starttime+i].s[0] += ch->vol[0] * sfx[(pos>>(PITCHSHIFT-1))&~1];
			paintbuffer[starttime+i].s[1] += ch->vol[1] * sfx[(pos>>(PITCHSHIFT-1))|1];
			pos += ch->rate;
		}
	}
	else
	{
		sfx = (signed char *)sc->data + (pos>>PITCHSHIFT)*2;
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[starttime+i].s[0] += ch->vol[0] * sfx[(i<<1)];
			paintbuffer[starttime+i].s[1] += ch->vol[1] * sfx[(i<<1)+1];
		}
	}
}

static void SND_PaintChannel8_O4I1 (channel_t *ch, sfxcache_t *sc, int count)
{
	signed char *sfx;
	int		i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed char data;
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[pos>>PITCHSHIFT];
			pos += ch->rate;
			paintbuffer[i].s[0] += ch->vol[0] * data;
			paintbuffer[i].s[1] += ch->vol[1] * data;
			paintbuffer[i].s[2] += ch->vol[2] * data;
			paintbuffer[i].s[3] += ch->vol[3] * data;
		}
	}
	else
	{
		sfx = (signed char *)sc->data + (pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += ch->vol[0] * sfx[i];
			paintbuffer[i].s[1] += ch->vol[1] * sfx[i];
			paintbuffer[i].s[2] += ch->vol[2] * sfx[i];
			paintbuffer[i].s[3] += ch->vol[3] * sfx[i];
		}
	}
}

static void SND_PaintChannel8_O6I1 (channel_t *ch, sfxcache_t *sc, int count)
{
	signed char *sfx;
	int		i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed char data;
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[pos>>PITCHSHIFT];
			pos += ch->rate;
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
		sfx = (signed char *)sc->data + (pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += ch->vol[0] * sfx[i];
			paintbuffer[i].s[1] += ch->vol[1] * sfx[i];
			paintbuffer[i].s[2] += ch->vol[2] * sfx[i];
			paintbuffer[i].s[3] += ch->vol[3] * sfx[i];
			paintbuffer[i].s[4] += ch->vol[4] * sfx[i];
			paintbuffer[i].s[5] += ch->vol[5] * sfx[i];
		}
	}
}

static void SND_PaintChannel8_O8I1 (channel_t *ch, sfxcache_t *sc, int count)
{
	signed char *sfx;
	int		i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed char data;
		sfx = (signed char *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			data = sfx[pos>>PITCHSHIFT];
			pos += ch->rate;
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
		sfx = (signed char *)sc->data + (pos>>PITCHSHIFT);
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
	}
}


static void SND_PaintChannel16_O2I1 (channel_t *ch, sfxcache_t *sc, int starttime, int count)
{
	int data;
	int left, right;
	int leftvol, rightvol;
	signed short *sfx;
	int	i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

	leftvol = ch->vol[0];
	rightvol = ch->vol[1];

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed short data;
		sfx = (signed short *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			float frac = pos&((1<<PITCHSHIFT)-1);
			data = sfx[pos>>PITCHSHIFT] * (1-frac) + sfx[(pos>>PITCHSHIFT)+1] * frac;
			pos += ch->rate;
			paintbuffer[starttime+i].s[0] += (leftvol * data)>>8;
			paintbuffer[starttime+i].s[1] += (rightvol * data)>>8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + (pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			data = sfx[i];
			left = (data * leftvol) >> 8;
			right = (data * rightvol) >> 8;
			paintbuffer[starttime+i].s[0] += left;
			paintbuffer[starttime+i].s[1] += right;
		}
	}
}

static void SND_PaintChannel16_O2I2 (channel_t *ch, sfxcache_t *sc, int starttime, int count)
{
	int leftvol, rightvol;
	signed short *sfx;
	int	i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

	leftvol = ch->vol[0];
	rightvol = ch->vol[1];

	if (ch->rate != (1<<PITCHSHIFT))
	{
		signed short l, r;
		sfx = (signed short *)sc->data;
		for (i=0 ; i<count ; i++)
		{
			l = sfx[(pos>>(PITCHSHIFT-1))&~1];
			r = sfx[(pos>>(PITCHSHIFT-1))|1];
			pos += ch->rate;
			paintbuffer[starttime+i].s[0] += (ch->vol[0] * l)>>8;
			paintbuffer[starttime+i].s[1] += (ch->vol[1] * r)>>8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + (pos>>PITCHSHIFT)*2;
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[starttime+i].s[0] += (*sfx++ * leftvol) >> 8;
			paintbuffer[starttime+i].s[1] += (*sfx++ * rightvol) >> 8;
		}
	}
}

static void SND_PaintChannel16_O4I1 (channel_t *ch, sfxcache_t *sc, int count)
{
	int vol[4];
	signed short *sfx;
	int	i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

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
			data = sfx[pos>>PITCHSHIFT];
			pos += ch->rate;
			paintbuffer[i].s[0] += (vol[0] * data)>>8;
			paintbuffer[i].s[1] += (vol[1] * data)>>8;
			paintbuffer[i].s[2] += (vol[2] * data)>>8;
			paintbuffer[i].s[3] += (vol[3] * data)>>8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + pos;
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += (sfx[i] * vol[0]) >> 8;
			paintbuffer[i].s[1] += (sfx[i] * vol[1]) >> 8;
			paintbuffer[i].s[2] += (sfx[i] * vol[2]) >> 8;
			paintbuffer[i].s[3] += (sfx[i] * vol[3]) >> 8;
		}
	}
}

static void SND_PaintChannel16_O6I1 (channel_t *ch, sfxcache_t *sc, int count)
{
	int vol[6];
	signed short *sfx;
	int	i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

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
			data = sfx[pos>>PITCHSHIFT];
			pos += ch->rate;
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
		sfx = (signed short *)sc->data + (pos>>PITCHSHIFT);
		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].s[0] += (sfx[i] * vol[0]) >> 8;
			paintbuffer[i].s[1] += (sfx[i] * vol[1]) >> 8;
			paintbuffer[i].s[2] += (sfx[i] * vol[2]) >> 8;
			paintbuffer[i].s[3] += (sfx[i] * vol[3]) >> 8;
			paintbuffer[i].s[4] += (sfx[i] * vol[4]) >> 8;
			paintbuffer[i].s[5] += (sfx[i] * vol[5]) >> 8;
		}
	}
}

static void SND_PaintChannel16_O8I1 (channel_t *ch, sfxcache_t *sc, int count)
{
	int vol[8];
	signed short *sfx;
	int	i;
	unsigned int pos = ch->pos-(sc->soundoffset<<PITCHSHIFT);

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
			data = sfx[pos>>PITCHSHIFT];
			pos += ch->rate;
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
		sfx = (signed short *)sc->data + (pos>>PITCHSHIFT);
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
	}
}
#endif
