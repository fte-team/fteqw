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

#ifndef __CYGWIN__

#include "quakedef.h"

#ifndef NOSOUNDASM
#define NOSOUNDASM	//since channels per sound card went to 6 (portable_samplegroup_t was changed)
#endif

#ifdef _WIN32
#include "winquake.h"
#else
#define DWORD	unsigned long
#endif

#define	PAINTBUFFER_SIZE	2048
portable_samplegroup_t paintbuffer[PAINTBUFFER_SIZE];
int		snd_scaletable[32][256];
int 	*snd_p, snd_vol;
short	*snd_out;

void Snd_WriteLinearBlastStereo16 (soundcardinfo_t *sc);

#if	defined(NOSOUNDASM) || !id386
void Snd_WriteLinearBlastStereo16 (soundcardinfo_t *sc)
{
	int		i, i2;
	int		val;

	for (i=0, i2=0; i<sc->snd_linear_count ; i+=2, i2+=6)
	{
		val = (snd_p[i2]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i2+1]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+1] = (short)0x8000;
		else
			snd_out[i+1] = val;
	}
}
#endif

void S_TransferStereo16 (soundcardinfo_t *sc, int endtime)
{
	int		lpos;
	int		lpaintedtime;
	DWORD	*pbuf;
#if defined(_WIN32) && !defined(NODIRECTX)
	int		reps;
	DWORD	dwSize=0,dwSize2=0;
	DWORD	*pbuf2;
	HRESULT	hresult;
#endif
	
	snd_vol = volume.value*256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = sc->paintedtime;

#if defined(_WIN32) && !defined(NODIRECTX)
	if (sc->pDSBuf)
	{
		reps = 0;

		while ((hresult = sc->pDSBuf->lpVtbl->Lock(sc->pDSBuf, 0, sc->gSndBufSize, (void**)&pbuf, &dwSize, 
									   (void**)&pbuf2, &dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf ("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_ShutdownCard (sc);
				SNDDMA_Init (sc);
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf ("S_TransferStereo16: DS: couldn't restore buffer\n");
				S_ShutdownCard (sc);
				SNDDMA_Init (sc);
				return;
			}
		}
	}
	else
#endif
	{
		pbuf = (DWORD *)sc->sn.buffer;
	}

	while (lpaintedtime < endtime)
	{
	// handle recirculating buffer issues
		lpos = lpaintedtime % ((sc->sn.samples>>1));

		snd_out = (short *) pbuf + (lpos<<1);

		sc->snd_linear_count = (sc->sn.samples>>1) - lpos;
		if (lpaintedtime + sc->snd_linear_count > endtime)
			sc->snd_linear_count = endtime - lpaintedtime;

		sc->snd_linear_count <<= 1;

	// write a linear blast of samples
		Snd_WriteLinearBlastStereo16 (sc);

		if (sc == sndcardinfo)	//only do this for one sound card.
			Media_RecordAudioFrame(snd_out, sc->snd_linear_count);

		snd_p += sc->snd_linear_count;
		lpaintedtime += (sc->snd_linear_count>>1);		
	}

#if defined(_WIN32) && !defined(NODIRECTX)
	if (sc->pDSBuf)
		sc->pDSBuf->lpVtbl->Unlock(sc->pDSBuf, pbuf, dwSize, NULL, 0);
#endif
}

void Snd_WriteLinearBlastStereo16_4Speaker (soundcardinfo_t *sc)
{
	int		i, i2;
	int		val;

	for (i=0, i2=0; i<sc->snd_linear_count ; i+=4, i2+=6)
	{
		val = (snd_p[i2]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i2+1]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+1] = (short)0x8000;
		else
			snd_out[i+1] = val;

		val = (snd_p[i2+2]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+2] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+2] = (short)0x8000;
		else
			snd_out[i+2] = val;

		val = (snd_p[i2+3]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+3] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+3] = (short)0x8000;
		else
			snd_out[i+3] = val;

//		snd_out[i+0] = rand();
//		snd_out[i+1] = rand();
//		snd_out[i+2] = rand();
//		snd_out[i+3] = rand();
	}
}

void S_Transfer4Speaker16 (soundcardinfo_t *sc, int endtime)
{
	int		lpos;
	int		lpaintedtime;
	DWORD	*pbuf;
#if defined(_WIN32) && !defined(NODIRECTX)
	int		reps;
	DWORD	dwSize=0,dwSize2=0;
	DWORD	*pbuf2;
	HRESULT	hresult;
#endif
	
	snd_vol = volume.value*256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = sc->paintedtime;

#if defined(_WIN32) && !defined(NODIRECTX)
	if (sc->pDSBuf)
	{
		reps = 0;

		while ((hresult = sc->pDSBuf->lpVtbl->Lock(sc->pDSBuf, 0, sc->gSndBufSize, (void**)&pbuf, &dwSize, 
									  (void**)&pbuf2, &dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf ("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_ShutdownCard (sc);
				SNDDMA_Init (sc);
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf ("S_TransferStereo16: DS: couldn't restore buffer\n");
				S_ShutdownCard (sc);
				SNDDMA_Init (sc);
				return;
			}
		}
	}
	else
#endif
	{
		pbuf = (DWORD *)sc->sn.buffer;
	}

	while (lpaintedtime < endtime)
	{
	// handle recirculating buffer issues
		lpos = lpaintedtime % ((sc->sn.samples>>2));

		snd_out = (short *) pbuf + (lpos<<2);

		sc->snd_linear_count = (sc->sn.samples>>2) - lpos;
		if (lpaintedtime + sc->snd_linear_count > endtime)
			sc->snd_linear_count = endtime - lpaintedtime;

		sc->snd_linear_count <<= 2;

	// write a linear blast of samples
		Snd_WriteLinearBlastStereo16_4Speaker (sc);

		if (sc == sndcardinfo)	//only do this for one sound card.
			Media_RecordAudioFrame(snd_out, sc->snd_linear_count);

		snd_p += sc->snd_linear_count;
		lpaintedtime += (sc->snd_linear_count>>2);		
	}

#if defined(_WIN32) && !defined(NODIRECTX)
	if (sc->pDSBuf)
		sc->pDSBuf->lpVtbl->Unlock(sc->pDSBuf, pbuf, dwSize, NULL, 0);
#endif
}

void Snd_WriteLinearBlast6Speaker16 (soundcardinfo_t *sc)
{
	int		i;
	int		val;

	for (i=0 ; i<sc->snd_linear_count ; i+=6)
	{
		val = (snd_p[i]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i+1]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+1] = (short)0x8000;
		else
			snd_out[i+1] = val;

		val = (snd_p[i+2]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+2] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+2] = (short)0x8000;
		else
			snd_out[i+2] = val;

		val = (snd_p[i+3]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+3] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+3] = (short)0x8000;
		else
			snd_out[i+3] = val;

		val = (snd_p[i+4]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+4] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+4] = (short)0x8000;
		else
			snd_out[i+4] = val;

		val = (snd_p[i+5]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+5] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+5] = (short)0x8000;
		else
			snd_out[i+5] = val;

#if 0
		snd_out[i+0] = rand();
		snd_out[i+1] = rand();
		snd_out[i+2] = rand();
		snd_out[i+3] = rand();
		snd_out[i+4] = rand();
		snd_out[i+5] = rand();
#elif 0
		snd_out[i+0]=snd_out[i+1]=snd_out[i+2]=snd_out[i+3]=snd_out[i+4]=snd_out[i+5] = rand();
#endif
	}
}
void S_Transfer6Speaker16 (soundcardinfo_t *sc, int endtime)
{
	int		lpos;
	int		lpaintedtime;
	DWORD	*pbuf;
#if defined(_WIN32) && !defined(NODIRECTX)
	int		reps;
	DWORD	dwSize=0,dwSize2=0;
	DWORD	*pbuf2;
	HRESULT	hresult;
#endif
	
	snd_vol = volume.value*256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = sc->paintedtime;

#if defined(_WIN32) && !defined(NODIRECTX)
	if (sc->pDSBuf)
	{
		reps = 0;

		while ((hresult = sc->pDSBuf->lpVtbl->Lock(sc->pDSBuf, 0, sc->gSndBufSize, (void**)&pbuf, &dwSize, 
									   (void**)&pbuf2, &dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf ("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_ShutdownCard (sc);
				SNDDMA_Init (sc);
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf ("S_TransferStereo16: DS: couldn't restore buffer\n");
				S_ShutdownCard (sc);
				SNDDMA_Init (sc);
				return;
			}
		}
	}
	else
#endif
	{
		pbuf = (DWORD *)sc->sn.buffer;
	}

	while (lpaintedtime < endtime)
	{
	// handle recirculating buffer issues
		lpos = (lpaintedtime % ((sc->sn.samples/6)));

		snd_out = (short *) pbuf + (lpos*6);

		sc->snd_linear_count = (sc->sn.samples/6) - lpos;
		if (lpaintedtime + sc->snd_linear_count > endtime)
			sc->snd_linear_count = endtime - lpaintedtime;

		sc->snd_linear_count *= 6;

	// write a linear blast of samples
		Snd_WriteLinearBlast6Speaker16 (sc);

		if (sc == sndcardinfo)	//only do this for one sound card.
			Media_RecordAudioFrame(snd_out, sc->snd_linear_count);

		snd_p += sc->snd_linear_count;
		lpaintedtime += (sc->snd_linear_count/6);	
	}

#if defined(_WIN32) && !defined(NODIRECTX)
	if (sc->pDSBuf)
		sc->pDSBuf->lpVtbl->Unlock(sc->pDSBuf, pbuf, dwSize, NULL, 0);
#endif
}

void S_TransferPaintBuffer(soundcardinfo_t *sc, int endtime)
{
	int 	out_idx;
	int 	count;
	int 	out_mask;
	int 	*p;
	int 	step;
	int		val;
	int		snd_vol;
	DWORD	*pbuf;
#if defined(_WIN32) && !defined(NODIRECTX)
	int		reps;
	DWORD	dwSize=0,dwSize2=0;
	DWORD	*pbuf2;
	HRESULT	hresult;
#endif

	if (sc->sn.samplebits == 16 && sc->sn.numchannels == 2)
	{
		S_TransferStereo16 (sc, endtime);
		return;
	}

	if (sc->sn.samplebits == 16 && sc->sn.numchannels == 6)
	{
		S_Transfer6Speaker16 (sc, endtime);
		return;
	}

	if (sc->sn.samplebits == 16 && sc->sn.numchannels == 4)
	{
		S_Transfer4Speaker16 (sc, endtime);
		return;
	}
	p = (int *) paintbuffer;
	count = (endtime - sc->paintedtime) * sc->sn.numchannels;
	out_mask = sc->sn.samples - 1; 
	out_idx = sc->paintedtime * sc->sn.numchannels & out_mask;
	if (sc->sn.numchannels>2)
		step = 1;
	else
		step = 3 - sc->sn.numchannels;
	snd_vol = volume.value*256;

	

#if defined(_WIN32) && !defined(NODIRECTX)
	if (sc->pDSBuf)
	{
		reps = 0;

		while ((hresult = sc->pDSBuf->lpVtbl->Lock(sc->pDSBuf, 0, sc->gSndBufSize, (void**)&pbuf, &dwSize, 
									   (void**)&pbuf2,&dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf ("S_TransferPaintBuffer: DS::Lock Sound Buffer Failed\n");
				S_ShutdownCard (sc);
				SNDDMA_Init (sc);
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf ("S_TransferPaintBuffer: DS: couldn't restore buffer\n");
				S_ShutdownCard (sc);
				SNDDMA_Init (sc);
				return;
			}
		}
	}
	else
#endif
	{
		pbuf = (DWORD *)sc->sn.buffer;
	}

	if (sc->sn.samplebits == 16)
	{
		short *out = (short *) pbuf;
		while (count--)
		{
			val = (*p * snd_vol) >> 8;
			p+= step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
	else if (sc->sn.samplebits == 8)
	{
		unsigned char *out = (unsigned char *) pbuf;
		while (count--)
		{
			val = (*p * snd_vol) >> 8;
			p+= step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = (val>>8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}

#if defined(_WIN32) && !defined(NODIRECTX)
	if (sc->pDSBuf) {
		DWORD dwNewpos, dwWrite;
		int il = sc->paintedtime;
		int ir = endtime - sc->paintedtime;
		
		ir += il;

		sc->pDSBuf->lpVtbl->Unlock(sc->pDSBuf, pbuf, dwSize, NULL, 0);

		sc->pDSBuf->lpVtbl->GetCurrentPosition(sc->pDSBuf, &dwNewpos, &dwWrite);

//		if ((dwNewpos >= il) && (dwNewpos <= ir))
//			Con_Printf("%d-%d p %d c\n", il, ir, dwNewpos);
	}
#endif
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int endtime);
void SND_PaintChannelFrom8Duel (channel_t *ch, sfxcache_t *sc, int endtime);
void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int endtime);
void SND_PaintChannelFrom8_4Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom16_4Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom8_6Speaker (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom16_6Speaker (channel_t *ch, sfxcache_t *sc, int count);
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

			if (ch->pos > scache->length)	//cache was flushed and gamedir changed.
			{
				ch->pos = scache->length;
				ch->end = scache->length;
			}


			ltime = sc->paintedtime;

			if (ch->sfx->decoder)
			{
				soundcardinfo_t *sndc;
#define qmax(x, y) (x>y)?(x):(y)
				ch->sfx->decoder->decodemore(ch->sfx, 
						ch->pos + end-ltime+1000);
						//ch->pos + qmax(end-ltime+1000, 1000));	//try to exceed by a little.
				scache = S_LoadSound (ch->sfx);
				if (!scache)
					continue;

				for (sndc = sndcardinfo; sndc; sndc=sndc->next)
				{
					for (j = 0; j < sndc->total_chans; j++)
						if (sndc->channel[j].sfx == ch->sfx)	//extend all of these.
							ch->end = ltime + (scache->length - ch->pos);
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
						ch->pos += count;
						continue;
					}

/*dmw having fun
					int uw;	
					int oldpos = ch->pos;
					int vol = ch->vol[0];
					int rvol = ch->vol[1];
					int end = ch->end;
					ch->vol[1]=0;
					ch->vol[0]/=2;
					for (uw = 0; uw < 5; uw++)
					{
*/
						if (scache->width == 1)
						{						
							if (scache->stereo)
								SND_PaintChannelFrom8Stereo(ch, scache, count);
							else if (sc->sn.numchannels == 6)
								SND_PaintChannelFrom8_6Speaker(ch, scache, count);
							else if (sc->sn.numchannels == 4)
								SND_PaintChannelFrom8_4Speaker(ch, scache, count);
							else	
								SND_PaintChannelFrom8(ch, scache, count);
						}
						else
						{
							if (scache->stereo)
								SND_PaintChannelFrom16Stereo(ch, scache, count);
							else if (sc->sn.numchannels == 6)
								SND_PaintChannelFrom16_6Speaker(ch, scache, count);
							else if (sc->sn.numchannels == 4)
								SND_PaintChannelFrom16_4Speaker(ch, scache, count);
							else
								SND_PaintChannelFrom16(ch, scache, count);
						}
/* lots of fun
//						ch->vol[0]*=-1;
						ch->vol[1]=0;
						ch->vol[0]/=1.3;
						ch->pos=oldpos-cursndcard->sn.speed*uw*0.06;
						if (ch->pos >= sc->length || ch->pos < 0)
							break;

					}
					ch->vol[0] = vol;
					ch->vol[1] = rvol;
					ch->pos = oldpos+count;
	*/
					ltime += count;
				}

			// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (scache->loopstart >= 0)
					{
						ch->pos = scache->loopstart;
						ch->end = ltime + scache->length - ch->pos;
						if (!scache->length)
						{
							scache->loopstart=-1;
							break;
						}
					}
					else if (ch->looping && scache->length)
					{
						ch->pos = 0;
						ch->end = ltime + scache->length - ch->pos;
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

void SND_InitScaletable (void)
{
	int		i, j;
	
	for (i=0 ; i<32 ; i++)
		for (j=0 ; j<256 ; j++)
			snd_scaletable[i][j] = ((signed char)j) * i * 8;
}


#if	defined(NOSOUNDASM) || !id386

void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count)
{
	int 	data;
	int		*lscale, *rscale;
	unsigned char *sfx;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;
	
		
	lscale = snd_scaletable[ch->vol[0] >> 3];
	rscale = snd_scaletable[ch->vol[1] >> 3];
	sfx = (signed char *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		paintbuffer[i].s[0] += lscale[data];
		paintbuffer[i].s[1] += rscale[data];
	}
		
	ch->pos += count;
}
void SND_PaintChannelFrom8Duel (channel_t *ch, sfxcache_t *sc, int count)
{
	int		*lscale, *rscale;
	unsigned char *sfx1, *sfx2;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;
	
		
	lscale = snd_scaletable[ch->vol[0] >> 3];
	rscale = snd_scaletable[ch->vol[1] >> 3];
	i = ch->pos - ch->delay[0];
	if (i < 0) i = 0;
	sfx1 = (signed char *)sc->data + i;
	i = ch->pos - ch->delay[1];
	if (i < 0) i = 0;
	sfx2 = (signed char *)sc->data + i;

	for (i=0 ; i<count ; i++)
	{
		paintbuffer[i].s[0] += lscale[sfx1[i]];
		paintbuffer[i].s[1] += rscale[sfx2[i]];
	}

	ch->pos += count;
}

#endif	// !id386

void SND_PaintChannelFrom8Stereo (channel_t *ch, sfxcache_t *sc, int count)
{
//	int 	data;
	int		*lscale, *rscale;
	unsigned char *sfx;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;
	
		
	lscale = snd_scaletable[ch->vol[0] >> 3];
	rscale = snd_scaletable[ch->vol[1] >> 3];
	sfx = (signed char *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{		
		paintbuffer[i].s[0] += lscale[sfx[(i<<1)]];		
		paintbuffer[i].s[1] += rscale[sfx[(i<<1)+1]];
	}
		
	ch->pos += count;
}

void SND_PaintChannelFrom8_4Speaker (channel_t *ch, sfxcache_t *sc, int count)
{
	int		*scale0, *scale1, *scale2, *scale3;
	unsigned char *sfx;
	int		i;

	if (ch->vol[0] > 255)
		ch->vol[0] = 255;
	if (ch->vol[1] > 255)
		ch->vol[1] = 255;
	if (ch->vol[2] > 255)
		ch->vol[2] = 255;
	if (ch->vol[3] > 255)
		ch->vol[3] = 255;

	scale0 = snd_scaletable[ch->vol[0] >> 3];
	scale1 = snd_scaletable[ch->vol[1] >> 3];
	scale2 = snd_scaletable[ch->vol[2] >> 3];
	scale3 = snd_scaletable[ch->vol[3] >> 3];
	sfx = (signed char *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		paintbuffer[i].s[0] += scale0[sfx[i]];
		paintbuffer[i].s[1] += scale1[sfx[i]];
		paintbuffer[i].s[2] += scale2[sfx[i]];
		paintbuffer[i].s[3] += scale3[sfx[i]];
	}

	ch->pos += count;
}

void SND_PaintChannelFrom8_6Speaker (channel_t *ch, sfxcache_t *sc, int count)
{
	int		*scale0, *scale1, *scale2, *scale3, *scale4, *scale5;
	unsigned char *sfx;
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

	scale0 = snd_scaletable[ch->vol[0] >> 3];
	scale1 = snd_scaletable[ch->vol[1] >> 3];
	scale2 = snd_scaletable[ch->vol[2] >> 3];
	scale3 = snd_scaletable[ch->vol[3] >> 3];
	scale4 = snd_scaletable[ch->vol[4] >> 3];
	scale5 = snd_scaletable[ch->vol[5] >> 3];
	sfx = (signed char *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{		
		paintbuffer[i].s[0] += scale0[sfx[i]];		
		paintbuffer[i].s[1] += scale1[sfx[i]];
		paintbuffer[i].s[2] += scale2[sfx[i]];		
		paintbuffer[i].s[3] += scale3[sfx[i]];
		paintbuffer[i].s[4] += scale4[sfx[i]];		
		paintbuffer[i].s[5] += scale5[sfx[i]];
	}
		
	ch->pos += count;
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
	sfx = (signed short *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		paintbuffer[i].s[0] += left;
		paintbuffer[i].s[1] += right;
	}
	
	ch->pos += count;
}

void SND_PaintChannelFrom16Stereo (channel_t *ch, sfxcache_t *sc, int count)
{
	int leftvol, rightvol;
	signed short *sfx;
	int	i;

	leftvol = ch->vol[0];
	rightvol = ch->vol[1];
	sfx = (signed short *)sc->data + ch->pos*2;

	for (i=0 ; i<count ; i++)
	{
		paintbuffer[i].s[0] += (*sfx++ * leftvol) >> 8;
		paintbuffer[i].s[1] += (*sfx++ * rightvol) >> 8;
	}
	
	ch->pos += count;
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
	sfx = (signed short *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		paintbuffer[i].s[0] += (sfx[i] * vol[0]) >> 8;
		paintbuffer[i].s[1] += (sfx[i] * vol[1]) >> 8;
		paintbuffer[i].s[2] += (sfx[i] * vol[2]) >> 8;
		paintbuffer[i].s[3] += (sfx[i] * vol[3]) >> 8;
		paintbuffer[i].s[4] += (sfx[i] * vol[4]) >> 8;
		paintbuffer[i].s[5] += (sfx[i] * vol[5]) >> 8;
	}
	
	ch->pos += count;
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
	sfx = (signed short *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		paintbuffer[i].s[0] += (sfx[i] * vol[0]) >> 8;
		paintbuffer[i].s[1] += (sfx[i] * vol[1]) >> 8;
		paintbuffer[i].s[2] += (sfx[i] * vol[2]) >> 8;
		paintbuffer[i].s[3] += (sfx[i] * vol[3]) >> 8;
	}
	
	ch->pos += count;
}

#endif
