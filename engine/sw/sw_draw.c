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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#ifdef RGLQUAKE
#include "glquake.h"	//with sw refresh???
#endif
#include "d_local.h"	//trans stuff

#include "sw_draw.h"

extern unsigned int *d_8to32table;

extern cvar_t con_ocranaleds;
extern cvar_t scr_conalpha;
extern qboolean scr_con_forcedraw;

typedef struct {
	vrect_t	rect;
	int		width;
	int		height;
	qbyte	*ptexbytes;
	int		rowbytes;
} rectdesc_t;

static rectdesc_t	r_rectdesc;

qbyte		*draw_chars;				// 8*8 graphic characters
mpic_t		*draw_disc;
mpic_t		*draw_backtile;

void SWDraw_TransPic (int x, int y, mpic_t *pic);

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	cache_user_t	cache;
} swcachepic_t;

#define	MAX_CACHED_PICS		1024
swcachepic_t	swmenu_cachepics[MAX_CACHED_PICS];
int			swmenu_numcachepics;

qbyte sw_crosshaircolor;

// current rendering blend for sw
extern palremap_t *ib_remap;
int ib_index;
int ib_ri, ib_gi, ib_bi, ib_ai;
qboolean ib_colorblend, ib_alphablend;

/*
================
Draw_CachePic
================
*/
mpic_t	*SWDraw_SafeCachePic (char *extpath)
{
	swcachepic_t	*pic;
	int			i;
	mpic_t		*dat;
	char alternatename[MAX_QPATH];
	char path[MAX_QPATH];
	Q_strncpyz(path, extpath, sizeof(path));
	COM_StripExtension(path, path, sizeof(path));

	for (pic=swmenu_cachepics, i=0 ; i<swmenu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			break;

	if (i == swmenu_numcachepics)
	{
		if (swmenu_numcachepics == MAX_CACHED_PICS)
			Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
		swmenu_numcachepics++;
		strcpy (pic->name, path);
	}

	dat = Cache_Check (&pic->cache);

	if (dat)
		return dat;

	{
		qbyte *file, *image;
		int width;
		int height;
		snprintf(alternatename, MAX_QPATH-1,"pics/%s.pcx", path);
		file = COM_LoadMallocFile(alternatename);
		if (!file)
		{
			snprintf(alternatename, MAX_QPATH-1,"%s.pcx", path);
			file = COM_LoadMallocFile(alternatename);
		}
		if (file)
		{
			image = ReadPCXFile(file, com_filesize, &width, &height);
			BZ_Free(file);
			if (image)
			{
				dat = Cache_Alloc(&pic->cache, sizeof(mpic_t) + width*height, path);
				((mpic_t*)dat)->width = width;
				((mpic_t*)dat)->height = height;
				((mpic_t*)dat)->flags = 0;
				for (i = 0; i < width*height; i++)
				{
					if (image[i*4+3] < 64) // 25% threshhold
					{
						((mpic_t*)dat)->flags |= MPIC_ALPHA;
						dat->data[i] = 255;
					}
					else
						dat->data[i] = GetPaletteNoFB(image[i*4], image[i*4+1], image[i*4+2]);
				}

				BZ_Free(image);

				return dat;
			}
		}
	}
#ifdef AVAIL_JPEGLIB
	{
		qbyte *file, *image;
		int width;
		int height;
		snprintf(alternatename, MAX_QPATH-1,"%s.jpg", path);

		file = COM_LoadMallocFile(alternatename);
		if (file)
		{
			image = ReadJPEGFile(file, com_filesize, &width, &height);
			BZ_Free(file);
			if (image)
			{
				dat = Cache_Alloc(&pic->cache, sizeof(mpic_t) + width*height, path);
				((mpic_t*)dat)->width = width;
				((mpic_t*)dat)->height = height;
				((mpic_t*)dat)->flags = 0;
				for (i = 0; i < width*height; i++)
				{
					if (image[i*4+3] < 64) // 25% threshhold
					{
						((mpic_t*)dat)->flags |= MPIC_ALPHA;
						dat->data[i] = 255;
					}
					else
						dat->data[i] = GetPaletteNoFB(image[i*4], image[i*4+1], image[i*4+2]);
				}

				BZ_Free(image);

				return dat;
			}
		}
	}
#endif
	{
		qbyte *file, *image;
		int width;
		int height;
		snprintf(alternatename, MAX_QPATH-1,"%s.tga", path);

		file = COM_LoadMallocFile(alternatename);
		if (file)
		{
			image = ReadTargaFile (file, com_filesize, &width, &height, 0);
			BZ_Free(file);
			if (image)
			{
				dat = Cache_Alloc(&pic->cache, sizeof(mpic_t) + width*height, path);
				((mpic_t*)dat)->width = width;
				((mpic_t*)dat)->height = height;
				((mpic_t*)dat)->flags = 0;
				for (i = 0; i < width*height; i++)
				{
					if (image[i*4+3] < 64) // 25% threshhold
					{
						((mpic_t*)dat)->flags |= MPIC_ALPHA;
						dat->data[i] = 255;
					}
					else
						dat->data[i] = GetPaletteNoFB(image[i*4], image[i*4+1], image[i*4+2]);
				}

				BZ_Free(image);

				return dat;
			}
		}
	}

//
// load the pic from disk
//
	snprintf(alternatename, MAX_QPATH-1,"%s.lmp", path);
	COM_LoadCacheFile (alternatename, &pic->cache);
	
	dat = pic->cache.data;
	if (!dat)
	{
		char alternatename[MAX_QPATH];
		sprintf(alternatename, "gfx/%s.lmp", path);
		COM_LoadCacheFile(alternatename, &pic->cache);
		dat = pic->cache.data;
		if (!dat)
			return NULL;
//		Sys_Error ("Draw_CachePic: failed to load %s", path);
	}

	SwapPic ((qpic_t*)dat);

	((mpic_t*)dat)->width = ((qpic_t*)dat)->width;
	((mpic_t*)dat)->height = ((qpic_t*)dat)->height;
	((mpic_t*)dat)->flags = 0;

	return dat;
}

mpic_t	*SWDraw_CachePic (char *path)
{
	mpic_t	*pic;

	pic = SWDraw_SafeCachePic(path);
	if (!pic)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
		
	return pic;
}

mpic_t	*SWDraw_MallocPic (char *path)
{
	int			i;
	qpic_t		*dat;
	swcachepic_t	*pic;

	for (pic=swmenu_cachepics, i=0 ; i<swmenu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			break;

	if (i == swmenu_numcachepics)
	{
		if (swmenu_numcachepics == MAX_CACHED_PICS)
		{
			Con_Printf ("menu_numcachepics == MAX_CACHED_PICS\n");
			return NULL;
		}
		swmenu_numcachepics++;
		pic->cache.fake = false;
		pic->cache.data = NULL;
		strcpy (pic->name, path);
	}

	dat = Cache_Check (&pic->cache);

	if (dat)
		return (mpic_t	*)dat;



	{
		qbyte *file, *image;
		int width;
		int height;

		file = COM_LoadMallocFile(path);
		if (file)
		{
			image = ReadPCXFile(file, com_filesize, &width, &height);
			BZ_Free(file);
			if (image)
			{
				dat = BZ_Malloc(sizeof(qpic_t) + width*height);
				if (dat)
				{
					pic->cache.data = dat;
					pic->cache.fake = true;
					((mpic_t*)dat)->width = width;
					((mpic_t*)dat)->height = height;
					((mpic_t*)dat)->flags = 0;
					for (i = 0; i < width*height; i++)
						dat->data[i] = GetPaletteNoFB(image[i*4], image[i*4+1], image[i*4+2]);

					BZ_Free(image);

					return (mpic_t	*)dat;
				}
				BZ_Free(image);
			}
		}
	}

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadHunkFile (path);
	pic->cache.data = dat;
	pic->cache.fake = true;

	if (!dat)
	{
		return NULL;
//		Sys_Error ("Draw_CachePic: failed to load %s", path);
	}

	SwapPic (dat);

	((mpic_t*)dat)->width = dat->width;
	((mpic_t*)dat)->height = dat->height;
	((mpic_t*)dat)->flags = 0;

	return (mpic_t	*)dat;
}
mpic_t	*SWDraw_PicFromWad (char *name)
{
	char q2name[MAX_QPATH];
	qpic_t *qpic;
	mpic_t *mpic;
	
	sprintf(q2name, "pics/%s.pcx", name);
	mpic = SWDraw_MallocPic(q2name);
	if (mpic)
		return mpic;

	qpic = W_SafeGetLumpName (name);
	if (!qpic)
		return NULL;

	mpic = (mpic_t *)qpic;
	mpic->width = qpic->width;
	mpic->height = qpic->height;
	mpic->flags = memchr (&qpic->data, 255, mpic->width * mpic->height)?MPIC_ALPHA:0;
	return mpic;
}



/*
===============
Draw_Init
===============
*/
//FIXME: mallocs in place of wad references will not be freed
//we have a memory leak
void SWDraw_Init (void)
{
	int concrc = 0;
	draw_chars = W_SafeGetLumpName ("conchars");	//q1
	if (!draw_chars)
	{
		mpic_t *pic;	//try q2
		int i;
		int s;
		pic = (mpic_t *)SWDraw_MallocPic("pics/conchars.pcx");	//safe from host_hunkmarks...
		if (pic)
		{
			draw_chars = pic->data;

			s = pic->width*pic->height;
			for (i = 0; i < s; i++)	//convert 255s to 0, q1's transparent colour
				if (draw_chars[i] == 162 || draw_chars[i] == 255)
					draw_chars[i] = 0;
		}
	}
	else
		concrc = QCRC_Block(draw_chars, 128*128); // get CRC here because it hasn't been replaced

	if (!draw_chars)
	{	//now go for hexen2
		int i, x;
		char *tempchars = COM_LoadMallocFile("gfx/menu/conchars.lmp");
		char *in, *out;
		if (tempchars)
		{
			draw_chars = BZ_Malloc(256*256);
			
			out = draw_chars;
			for (i = 0; i < 8*8; i+=1)
			{
				if ((i/8)&1)
				{
					in = tempchars + ((i)/8)*16*8*8+(i&7)*32*8 - 256*4+128;
					for (x = 0; x < 16*8; x++)
						*out++ = *in++;
				}
				else
				{
					in = tempchars + (i/8)*16*8*8+(i&7)*32*8;
					for (x = 0; x < 16*8; x++)
						*out++ = *in++;
				}
			}
			for (i = 0; i < 8*8; i+=1)
			{
				if ((i/8)&1)
				{
					in = tempchars+128*128 + ((i)/8)*16*8*8+(i&7)*32*8 - 256*4+128;
					for (x = 0; x < 16*8; x++)
						*out++ = *in++;
				}
				else
				{
					in = tempchars+128*128 + (i/8)*16*8*8+(i&7)*32*8;
					for (x = 0; x < 16*8; x++)
						*out++ = *in++;
				}
			}
			Z_Free(tempchars);
		}
		else
		{	//nope, that failed too.
			//use our built in fallback
			{
				int width;
				int height;
				qbyte *image;
				qbyte *src, *dest;
				extern qbyte default_conchar[11356];

				image = ReadTargaFile(default_conchar, sizeof(default_conchar), &width, &height, false);
//				COM_WriteFile("test.dat", image, 256*256*4);

				draw_chars = BZ_Malloc(128*128);
			
				//downsample the 256 image to quake's 128 wide.
				for (i = 0; i < 128;i++)
				{
					src = image+i*8*16*16;
					dest = draw_chars + i*16*8;
					for (x = 0; x < 128; x++)
						dest[x] = src[(x*2)*4]?15:0;//GetPaletteIndex(image[i*4], image[i*4+1], image[i*4+2]);
				}

//				COM_WriteFile("test2.dat", draw_chars, 128*128);

				BZ_Free(image);
			}
		}
	}
	if (!draw_chars)
		Sys_Error("Failed to find suitable console characters\n");

	// add ocrana leds
	if (con_ocranaleds.value)
	{
		if (con_ocranaleds.value != 2 || concrc == 798) 
			AddOcranaLEDsIndexed (draw_chars, 128, 128);
	}

	// add conchars into sw menu cache
	swmenu_numcachepics = 0;

	// lame hack but whatever works
	strcpy(swmenu_cachepics[swmenu_numcachepics].name, "pics/conchars.pcx");
	swmenu_cachepics[swmenu_numcachepics].cache.fake = true;
	swmenu_cachepics[swmenu_numcachepics].cache.data = BZ_Malloc(sizeof(mpic_t) + 128*128);
	{
		mpic_t *dat = (mpic_t *)swmenu_cachepics[swmenu_numcachepics].cache.data;
		// reformat conchars for use in cache
		int j;

		for (j = 0; j < 128*128; j++)
			dat->data[j] = (draw_chars[j] == 255 || !draw_chars[j]) ? draw_chars[j] ^ 255 : draw_chars[j];

		dat->width = dat->height = 128;
		dat->flags = 1;
	}
	swmenu_numcachepics++;

	draw_disc = W_SafeGetLumpName ("disc");
	draw_backtile = W_SafeGetLumpName ("backtile");
	if (!draw_backtile)
		draw_backtile = (mpic_t	*)COM_LoadMallocFile("gfx/menu/backtile.lmp");

	if (draw_backtile)
	{
		{
			((mpic_t*)draw_backtile)->width = ((qpic_t*)draw_backtile)->width;
			((mpic_t*)draw_backtile)->height = ((qpic_t*)draw_backtile)->height;
			((mpic_t*)draw_backtile)->flags = 0;
		}
		r_rectdesc.width = draw_backtile->width;
		r_rectdesc.height = draw_backtile->height;
		r_rectdesc.ptexbytes = draw_backtile->data;
		r_rectdesc.rowbytes = draw_backtile->width;
	}

#ifdef PLUGINS
	Plug_DrawReloadImages();
#endif
}

void SWDraw_Shutdown(void)
{
	int i;
	swcachepic_t *pic;
	for (pic=swmenu_cachepics, i=0 ; i<swmenu_numcachepics ; pic++, i++)
	{
		if (pic->cache.fake)
		{
			if (pic->cache.data)
				BZ_Free(pic->cache.data);
			pic->cache.fake = false;
			pic->cache.data = NULL;
		}
	}
	swmenu_numcachepics=0;

	draw_disc = NULL;
}


/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void SWDraw_Character (int x, int y, unsigned int num)
{
	qbyte			*dest;
	qbyte			*source;
	int				drawline;	
	int				row, col;

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	if (y > vid.height - 8 || x < 0 || x > vid.width - 8)
		return;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	if (y < 0)
	{	// clipped
		drawline = 8 + y;
		source -= 128*y;
		y = 0;
	}
	else
		drawline = 8;


	if (r_pixbytes == 1)
	{
		dest = vid.conbuffer + y*vid.conrowbytes + x;
	
		while (drawline--)
		{
			if (source[0])
				dest[0] = source[0];
			if (source[1])
				dest[1] = source[1];
			if (source[2])
				dest[2] = source[2];
			if (source[3])
				dest[3] = source[3];
			if (source[4])
				dest[4] = source[4];
			if (source[5])
				dest[5] = source[5];
			if (source[6])
				dest[6] = source[6];
			if (source[7])
				dest[7] = source[7];
			source += 128;
			dest += vid.conrowbytes;
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short *dest16;
		dest16 = (unsigned short*)vid.conbuffer + y*vid.conrowbytes + x;
	
		while (drawline--)
		{
			if (source[0])
				dest16[0] = d_8to16table[source[0]];
			if (source[1])
				dest16[1] = d_8to16table[source[1]];
			if (source[2])
				dest16[2] = d_8to16table[source[2]];
			if (source[3])
				dest16[3] = d_8to16table[source[3]];
			if (source[4])
				dest16[4] = d_8to16table[source[4]];
			if (source[5])
				dest16[5] = d_8to16table[source[5]];
			if (source[6])
				dest16[6] = d_8to16table[source[6]];
			if (source[7])
				dest16[7] = d_8to16table[source[7]];
			source += 128;
			dest16 += vid.conrowbytes;
		}
	}
	else if (r_pixbytes == 4)
	{
		unsigned int *p32dest;		
		p32dest = (unsigned int *)vid.conbuffer + y*vid.conrowbytes + x;
	
		while (drawline--)
		{
			if (source[0])
				p32dest[0] = d_8to32table[source[0]];
			if (source[1])
				p32dest[1] = d_8to32table[source[1]];
			if (source[2])
				p32dest[2] = d_8to32table[source[2]];
			if (source[3])
				p32dest[3] = d_8to32table[source[3]];
			if (source[4])
				p32dest[4] = d_8to32table[source[4]];
			if (source[5])
				p32dest[5] = d_8to32table[source[5]];
			if (source[6])
				p32dest[6] = d_8to32table[source[6]];
			if (source[7])
				p32dest[7] = d_8to32table[source[7]];
			source += 128;
			p32dest += vid.conrowbytes;
		}
	}
}

/*
#define drawpal(r,g,b) pal555to8[(r|(g<<5)|(b<<10)) & consolecolours[colour].rgbmask]
#define draw(p) drawpal(host_basepal[p*3]>>3,host_basepal[p*3+1]>>3,host_basepal[p*3+2]>>3)
*/

#define draw(x) ib_remap->pal[x]
#define tdraw(x, y) Trans(x, ib_remap->pal[y])
void SWDraw_ColouredCharacter (int x, int y, unsigned int num)
{
	qbyte			*source;
	int				drawline;	
	int				row, col;
	extern cvar_t cl_noblink;
	unsigned int colour;
	qboolean alpha;
	
	if (y <= -8)
		return;			// totally off screen

	if (y > vid.height - 8 || x < 0 || x > vid.width - 8)
		return;

	colour = (num & CON_FGMASK) >> CON_FGSHIFT;
	alpha = !!(num & CON_HALFALPHA);

	if (num & CON_NONCLEARBG)
	{
		unsigned int bgcolour;

		bgcolour = (num & CON_BGMASK) >> CON_BGSHIFT;
		SWDraw_FillRGB(x, (y < 0) ? 0 : y, 8, (y < 0) ? 8 + y : 8,
			consolecolours[bgcolour].fr, 
			consolecolours[bgcolour].fg, 
			consolecolours[bgcolour].fb);
	}

	if (num & CON_BLINKTEXT)
	{
		if (!cl_noblink.value)
			if ((int)(cl.time*2) & 1)
				return;
	}

	if (colour == COLOR_WHITE && !alpha)
	{
		Draw_Character(x, y, num);
		return;
	}

	SWDraw_ImageColours(consolecolours[colour].fr, 
		consolecolours[colour].fg, 
		consolecolours[colour].fb,
		alpha ? 0.5 : 1); 

	num &= 255;
	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	if (y < 0)
	{	// clipped
		drawline = 8 + y;
		source -= 128*y;
		y = 0;
	}
	else
		drawline = 8;

	if (r_pixbytes == 1)
	{
		qbyte			*dest;
		dest = vid.conbuffer + y*vid.conrowbytes + x;
	
		if (alpha)
		{
			while (drawline--)
			{
				if (source[0])
					dest[0] = tdraw(dest[0], source[0]);
				if (source[1])
					dest[1] = tdraw(dest[1], source[1]);
				if (source[2])
					dest[2] = tdraw(dest[2], source[2]);
				if (source[3])
					dest[3] = tdraw(dest[3], source[3]);
				if (source[4])
					dest[4] = tdraw(dest[4], source[4]);
				if (source[5])
					dest[5] = tdraw(dest[5], source[5]);
				if (source[6])
					dest[6] = tdraw(dest[6], source[6]);
				if (source[7])
					dest[7] = tdraw(dest[7], source[7]);
				source += 128;
				dest += vid.conrowbytes;
			}
		}
		else
		{
			while (drawline--)
			{
				if (source[0])
					dest[0] = draw(source[0]);
				if (source[1])
					dest[1] = draw(source[1]);
				if (source[2])
					dest[2] = draw(source[2]);
				if (source[3])
					dest[3] = draw(source[3]);
				if (source[4])
					dest[4] = draw(source[4]);
				if (source[5])
					dest[5] = draw(source[5]);
				if (source[6])
					dest[6] = draw(source[6]);
				if (source[7])
					dest[7] = draw(source[7]);
				source += 128;
				dest += vid.conrowbytes;
			}
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short *dest16;
		unsigned char *pal = (unsigned char *)d_8to32table;
		int i;

		dest16 = (unsigned short *)vid.conbuffer + y*vid.conrowbytes + x;

		if (alpha)
		{
			while (drawline--)
			{
				for (i = 0; i < 8; i++)
				{
					if (source[i])
					{
						dest16[i] = (((((128+pal[source[i]*4+0]*ib_ri)>>12)<<10) + ((dest16[i]&0x7B00)>>1)) |
							((((128+pal[source[i]*4+1]*ib_gi)>>12)<<5) + ((dest16[i]&0x03D0)>>1)) |
							((128+pal[source[i]*4+2]*ib_bi)>>12)) + ((dest16[i]&0x001E)>>1);
					}
				}
				source += 128;
				dest16 += vid.conrowbytes;
			}
		}
		else
		{
			while (drawline--)
			{
				for (i = 0; i < 8; i++)
				{
					if (source[i])
						dest16[i] = (((128+pal[source[i]*4+0]*ib_ri)>>11)<<10)|
									(((128+pal[source[i]*4+1]*ib_gi)>>11)<<5)|
									((128+pal[source[i]*4+2]*ib_bi)>>11);
				}
				source += 128;
				dest16 += vid.conrowbytes;
			}
		}
	}
	else if (r_pixbytes == 4)
	{
		qbyte			*dest;
		int i;
		unsigned char *pal = (unsigned char *)d_8to32table;
		dest = vid.conbuffer + (y*vid.conrowbytes + x)*r_pixbytes;

		if (alpha)
		{
			while (drawline--)
			{
				for (i = 0; i < 8; i++)
				{
					if (source[i])
					{
						dest[0+i*4] = ((128+pal[source[i]*4+0]*ib_bi)>>9) + (dest[0+i*4]>>1);
						dest[1+i*4] = ((128+pal[source[i]*4+1]*ib_gi)>>9) + (dest[1+i*4]>>1);
						dest[2+i*4] = ((128+pal[source[i]*4+2]*ib_ri)>>9) + (dest[2+i*4]>>1);
					}				
				}
				source += 128;
				dest += vid.conrowbytes*r_pixbytes;
			}
		}
		else
		{
			while (drawline--)
			{
				for (i = 0; i < 8; i++)
				{
					if (source[i])
					{
						dest[0+i*4] = (128+pal[source[i]*4+0]*ib_bi)>>8;
						dest[1+i*4] = (128+pal[source[i]*4+1]*ib_gi)>>8;
						dest[2+i*4] = (128+pal[source[i]*4+2]*ib_ri)>>8;
					}				
				}
				source += 128;
				dest += vid.conrowbytes*r_pixbytes;
			}
		}
	}
}
#undef draw
#undef tdraw

/*
================
Draw_String
================
*/
void SWDraw_String (int x, int y, const qbyte *str)
{
	while (*str)
	{
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

/*
================
Draw_Alt_String
================
*/
void SWDraw_Alt_String (int x, int y, const qbyte *str)
{
	while (*str)
	{
		Draw_Character (x, y, (*str) | 0x80);
		str++;
		x += 8;
	}
}

void SWDraw_Pixel(int x, int y, qbyte color)
{
	qbyte			*dest;

	if (r_pixbytes == 1)
	{
		dest = vid.conbuffer + y*vid.conrowbytes + x;
		*dest = color;
	}
	else if (r_pixbytes == 4)
	{
		unsigned int	*p32dest;
	// FIXME: pre-expand to native format?
		p32dest = ((unsigned int *)vid.conbuffer + y*vid.conrowbytes + x);
		*p32dest = d_8to32table[color];
	}
}

void SWCrosshaircolor_Callback(struct cvar_s *var, char *oldvalue)
{
	sw_crosshaircolor = SCR_StringToPalIndex(var->string, 255);
}

#include "crosshairs.dat"
qbyte *COM_LoadFile (char *path, int usehunk);
void SWDraw_Crosshair(void)
{
	int x, y;
	extern cvar_t crosshair, cl_crossx, cl_crossy;
	extern vrect_t		scr_vrect;
	qbyte c, c2;
	int sc;

	c2 = c = sw_crosshaircolor;

	for (sc = 0; sc < cl.splitclients; sc++)
	{
		SCR_CrosshairPosition(sc, &x, &y);

	#define Pix(xp,yp,c) SWDraw_Pixel(x+xp, y+yp, c)

		switch((int)crosshair.value)
		{
		case 0:
			if (*crosshair.string>='a' && *crosshair.string<='z')
			{
				static qbyte *crosshairfile;	
				static int crosshairfilesize;
				static char cachedcrosshairfile[64];
				int fx, fy;
				qbyte *f;

				if (!strncmp(cachedcrosshairfile, crosshair.string, sizeof(cachedcrosshairfile)))
				{
					if (crosshairfile)
						Z_Free(crosshairfile);
					crosshairfile =  COM_LoadFile(va("%s.csh", crosshair.string), 0);
					crosshairfilesize = com_filesize;
					Q_strncpyz(cachedcrosshairfile, crosshair.string, sizeof(cachedcrosshairfile));
				}

				f = crosshairfile;
				if (!f)
					return;
				for (fy = 0; fy < 8; fy++)
				{
					for (fx = 0; fx < 8; )
					{
						if (f - crosshairfile > crosshairfilesize)
						{
							Con_Printf("Crosshair file has overrun");
							fy=10;
							break;
						}
						if (*f == 'x')
						{
							Pix(fx-3, fy-3, c);
							fx++;
						}
						else if (*f == 'X')
						{
							Pix(fx-3, fy-3, c2);
							fx++;
						}
						else if (*f == '0' || *f == 'o' || *f == 'O')
							fx++;

						f++;
					}				
				}
			}
			break;
		default:
		case 1:
			Draw_Character (
				scr_vrect.x + scr_vrect.width/2-4 + cl_crossx.value, 
				scr_vrect.y + scr_vrect.height/2-4 + cl_crossy.value, 
				'+');
			break;
	#include "crosshairs.dat"
		}
	}
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void SWDraw_DebugChar (qbyte num)
{
	qbyte			*dest;
	qbyte			*source;
	int				drawline;	
	extern qbyte		*draw_chars;
	int				row, col;

	if (!vid.direct)
		return;		// don't have direct FB access, so no debugchars...

	drawline = 8;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	dest = vid.direct + 312;

	while (drawline--)
	{
		dest[0] = source[0];
		dest[1] = source[1];
		dest[2] = source[2];
		dest[3] = source[3];
		dest[4] = source[4];
		dest[5] = source[5];
		dest[6] = source[6];
		dest[7] = source[7];
		source += 128;
		dest += 320;
	}
}

/*
=============
Draw_Pic
=============
*/
void SWDraw_Pic (int x, int y, mpic_t *pic)
{
	qbyte			*dest, *source;
	int				v, u;

	if (!pic)
		return;

	if (pic->flags & MPIC_ALPHA)
	{
		SWDraw_TransPic(x, y, pic);
		return;
	}

	if ((x < 0) ||
		(x + pic->width > vid.width) ||
		(y < 0) ||
		(y + pic->height > vid.height))
	{
		return;//Sys_Error ("Draw_Pic: bad coordinates");
	}

	source = pic->data;

	if (r_pixbytes == 1)
	{
		dest = vid.buffer + y * vid.rowbytes + x;

		for (v=0 ; v<pic->height ; v++)
		{
			Q_memcpy (dest, source, pic->width);
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
	else if (r_pixbytes == 4)
	{
		unsigned int *p32dest;
		p32dest = (unsigned int *)vid.buffer + y * vid.rowbytes + x;

		for (v=0 ; v<pic->height ; v++)
		{
			for (u=0 ; u<pic->width ; u++)
				p32dest[u] = d_8to32table[source[u]];
			p32dest += vid.rowbytes;
			source += pic->width;
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short *p16dest;

		p16dest = (unsigned short *)vid.buffer + y * vid.rowbytes + x;

		for (v=0 ; v<pic->height ; v++)
		{
			for (u=0 ; u<pic->width ; u++)
				p16dest[u] = d_8to16table[source[u]];
			p16dest += vid.rowbytes;
			source += pic->width;
		}
	}
}

/*
=============
Draw_SubPic
=============
*/
void SWDraw_TransSubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	qbyte			*dest, *source;
	int				v, u;

	if ((x < 0) ||
		(x + width > vid.width) ||
		(y < 0) ||
		(y + height > vid.height))
	{
		Sys_Error ("Draw_Pic: bad coordinates");
	}

	source = pic->data + srcy * pic->width + srcx;

	if (r_pixbytes == 1)
	{
		qbyte tbyte;
		dest = vid.buffer + y * vid.rowbytes + x;

		if (pic->width & 7)
		{	// general
			for (v=0 ; v<height ; v++)
			{
				for (u=0 ; u<width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;
	
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<height ; v++)
			{
				for (u=0 ; u<width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						dest[u+1] = tbyte;
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						dest[u+2] = tbyte;
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						dest[u+3] = tbyte;
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						dest[u+4] = tbyte;
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						dest[u+5] = tbyte;
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						dest[u+6] = tbyte;
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						dest[u+7] = tbyte;
				}
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short	*p16dest;
		p16dest = (unsigned short *)vid.buffer + y * vid.rowbytes + x;

		for (v=0 ; v<height ; v++)
		{
			for (u=0 ; u<(width) ; u++)
				p16dest[u] = d_8to16table[source[u]];
			p16dest += vid.rowbytes;
			source += pic->width;
		}
	}
	else if (r_pixbytes == 4)
	{
		unsigned int	*p32dest;
		p32dest = (unsigned int	*)vid.buffer + y * vid.rowbytes + x;

		for (v=0 ; v<height ; v++)
		{
			for (u=0 ; u<(width) ; u++)
				p32dest[u] = d_8to32table[source[u]];
			p32dest += vid.rowbytes;
			source += pic->width;
		}
	}
}
/*
=============
Draw_SubPic
=============
*/
void SWDraw_SubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	qbyte			*dest, *source;
	int				v, u;

	if (pic->flags & MPIC_ALPHA)
	{
		SWDraw_TransSubPic(x, y, pic, srcx, srcy, width, height);
		return;
	}

	if ((x < 0) ||
		(x + width > vid.width) ||
		(y < 0) ||
		(y + height > vid.height))
	{
		Sys_Error ("Draw_Pic: bad coordinates");
	}

	source = pic->data + srcy * pic->width + srcx;

	if (r_pixbytes == 1)
	{
		dest = vid.buffer + y * vid.rowbytes + x;

		for (v=0 ; v<height ; v++)
		{
			Q_memcpy (dest, source, width);
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short	*p16dest;
		p16dest = (unsigned short *)vid.buffer + y * vid.rowbytes + x;

		for (v=0 ; v<height ; v++)
		{
			for (u=0 ; u<(width) ; u++)
				p16dest[u] = d_8to16table[source[u]];
			p16dest += vid.rowbytes;
			source += pic->width;
		}
	}
	else if (r_pixbytes == 4)
	{
		unsigned int	*p32dest;
		p32dest = (unsigned int	*)vid.buffer + y * vid.rowbytes + x;

		for (v=0 ; v<height ; v++)
		{
			for (u=0 ; u<(width) ; u++)
				p32dest[u] = d_8to32table[source[u]];
			p32dest += vid.rowbytes;
			source += pic->width;
		}
	}
}


/*
=============
Draw_TransPic
=============
*/
void SWDraw_TransPic (int x, int y, mpic_t *pic)
{
	qbyte	*source, tbyte;
	int				v, u;

	if (!pic)
		return;
	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		return;
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	source = pic->data;

	if (r_pixbytes == 1)
	{
		qbyte	*dest;
		dest = vid.buffer + y * vid.rowbytes + x;

		if (pic->width & 7)
		{	// general
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;
	
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						dest[u+1] = tbyte;
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						dest[u+2] = tbyte;
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						dest[u+3] = tbyte;
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						dest[u+4] = tbyte;
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						dest[u+5] = tbyte;
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						dest[u+6] = tbyte;
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						dest[u+7] = tbyte;
				}
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short *dest;
		dest = (unsigned short *)vid.buffer + y * vid.rowbytes + x;

		if (pic->width & 7)
		{	// general
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;
	
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = d_8to16table[tbyte];
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						dest[u+1] = d_8to16table[tbyte];
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						dest[u+2] = d_8to16table[tbyte];
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						dest[u+3] = d_8to16table[tbyte];
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						dest[u+4] = d_8to16table[tbyte];
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						dest[u+5] = d_8to16table[tbyte];
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						dest[u+6] = d_8to16table[tbyte];
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						dest[u+7] = d_8to16table[tbyte];
				}
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
	}
	else if (r_pixbytes == 4)
	{
		unsigned int *p32dest = (unsigned int *)vid.buffer + y * vid.rowbytes + x;

		if (pic->width & 7)
		{	// general
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						p32dest[u] = d_8to32table[tbyte];
	
				p32dest += vid.rowbytes;
				source += pic->width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						p32dest[u] = d_8to32table[tbyte];
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						p32dest[u+1] = d_8to32table[tbyte];
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						p32dest[u+2] = d_8to32table[tbyte];
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						p32dest[u+3] = d_8to32table[tbyte];
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						p32dest[u+4] = d_8to32table[tbyte];
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						p32dest[u+5] = d_8to32table[tbyte];
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						p32dest[u+6] = d_8to32table[tbyte];
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						p32dest[u+7] = d_8to32table[tbyte];
				}
				p32dest += vid.rowbytes;
				source += pic->width;
			}
		}
	}
}


/*
=============
Draw_TransPicTranslate
=============
*/
void SWDraw_TransPicTranslate (int x, int y, int width, int height, qbyte *source, qbyte *translation)
{
	qbyte	tbyte;
	int				v, u;

	if (x < 0 || (unsigned)(x + width) > vid.width || y < 0 ||
		 (unsigned)(y + height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	if (r_pixbytes == 1)
	{
		qbyte	*dest;
		dest = vid.buffer + y * vid.rowbytes + x;

		if (width & 7)
		{	// general
			for (v=0 ; v<height ; v++)
			{
				for (u=0 ; u<width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = translation[tbyte];

				dest += vid.rowbytes;
				source += width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<height ; v++)
			{
				for (u=0 ; u<width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = translation[tbyte];
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						dest[u+1] = translation[tbyte];
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						dest[u+2] = translation[tbyte];
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						dest[u+3] = translation[tbyte];
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						dest[u+4] = translation[tbyte];
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						dest[u+5] = translation[tbyte];
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						dest[u+6] = translation[tbyte];
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						dest[u+7] = translation[tbyte];
				}
				dest += vid.rowbytes;
				source += width;
			}
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short	*dest;
		dest = (unsigned short*)vid.buffer + y * vid.rowbytes + x;

		if (width & 7)
		{	// general
			for (v=0 ; v<height ; v++)
			{
				for (u=0 ; u<width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = d_8to16table[translation[tbyte]];

				dest += vid.rowbytes;
				source += width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<height ; v++)
			{
				for (u=0 ; u<width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = d_8to16table[translation[tbyte]];
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						dest[u+1] = d_8to16table[translation[tbyte]];
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						dest[u+2] = d_8to16table[translation[tbyte]];
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						dest[u+3] = d_8to16table[translation[tbyte]];
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						dest[u+4] = d_8to16table[translation[tbyte]];
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						dest[u+5] = d_8to16table[translation[tbyte]];
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						dest[u+6] = d_8to16table[translation[tbyte]];
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						dest[u+7] = d_8to16table[translation[tbyte]];
				}
				dest += vid.rowbytes;
				source += width;
			}
		}
	}
	else if (r_pixbytes == 4)
	{
		unsigned int	*puidest;

		puidest = (unsigned int	*)(vid.buffer + ((y * vid.rowbytes + x) << 2));

		if (width & 7)
		{	// general
			for (v=0 ; v<height ; v++)
			{
				for (u=0 ; u<width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						puidest[u] = d_8to32table[translation[tbyte]];

				puidest += vid.rowbytes;
				source += width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<height ; v++)
			{
				for (u=0 ; u<width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						puidest[u] = d_8to32table[translation[tbyte]];
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						puidest[u+1] = d_8to32table[translation[tbyte]];
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						puidest[u+2] = d_8to32table[translation[tbyte]];
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						puidest[u+3] = d_8to32table[translation[tbyte]];
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						puidest[u+4] = d_8to32table[translation[tbyte]];
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						puidest[u+5] = d_8to32table[translation[tbyte]];
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						puidest[u+6] = d_8to32table[translation[tbyte]];
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						puidest[u+7] = d_8to32table[translation[tbyte]];
				}
				puidest += vid.rowbytes;
				source += width;
			}
		}
	}
	else
		Sys_Error("draw_transpictranslate: r_pixbytes\n");
}


void SWDraw_CharToConback (int num, qbyte *dest)
{
	int		row, col;
	qbyte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x])
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

void SWDraw_SubImageBlend32(
		int scx, int scy, int scwidth, int scheight,	//screen
		int six, int siy, int siwidth, int siheight,	//sub image
		int iwidth, int iheight, qbyte *in, int red, int green, int blue, int alpha)
{
	unsigned char *pal = (unsigned char *)d_8to32table;
	unsigned char *dest;
	qbyte *src;
	int v;
	int f, fstep;
	int x, y;
	int outstride = vid.rowbytes;

	dest = (unsigned char *)vid.buffer + 4*(scx + scy*outstride);

	fstep = (siwidth<<16)/scwidth;

	for (y=0 ; y<scheight ; y++, dest += outstride*4)
	{
		v = y*siheight/scheight;
		src = in + (siy+v)*iwidth + six;
		{
			f = 0;
			for (x=0; x<scwidth<<2; x+=4)	//sw 32 bit rendering is bgrx
			{
				if (src[(f>>16)] != 255)
				{
					dest[x + 0] = ((255-alpha)*dest[x + 0] + (blue*alpha*pal[(src[f>>16]<<2) + 0])/255)/255;
					dest[x + 1] = ((255-alpha)*dest[x + 1] + (green*alpha*pal[(src[f>>16]<<2) + 1])/255)/255;
					dest[x + 2] = ((255-alpha)*dest[x + 2] + (red*alpha*pal[(src[f>>16]<<2) + 2])/255)/255;
				}
				f += fstep;
			}
		}
	}
}

void SWDraw_SubImage32(
		int scx, int scy, int scwidth, int scheight,	//screen
		int six, int siy, int siwidth, int siheight,	//sub image
		int iwidth, int iheight, qbyte *in)
{
	unsigned int *gamma = d_8to32table;
	unsigned int *dest;
	qbyte *src;
	int v;
	int f, fstep;
	int x, y;
	int outstride = vid.rowbytes;

	if (ib_colorblend || ib_alphablend)
	{
		SWDraw_SubImageBlend32(scx, scy, scwidth, scheight,
			six, siy, siwidth, siheight,
			iwidth, iheight, in, ib_ri, ib_gi, ib_bi, ib_ai);
	}

	dest = (unsigned int *)vid.buffer + scx + scy*outstride;

	fstep = (siwidth<<16)/scwidth;

	for (y=0 ; y<scheight ; y++, dest += outstride)
	{
		v = y*siheight/scheight;
		src = in + (siy+v)*iwidth + six;
		{
			f = 0;
			for (x=0; x < (scwidth&~3); x+=4)	//cut down on loop stuff
			{
				if (src[(f>>16)] != 255)
				{
					dest[x] = gamma[src[(f>>16)]];
				}
				f += fstep;

				if (src[(f>>16)] != 255)
				{
					dest[1+x] = gamma[src[(f>>16)]];
				}
				f += fstep;

				if (src[(f>>16)] != 255)
				{
					dest[2+x] = gamma[src[(f>>16)]];
				}
				f += fstep;

				if (src[(f>>16)] != 255)
				{
					dest[3+x] = gamma[src[(f>>16)]];
				}
				f += fstep;
			}
			for (; x<scwidth; x+=1)	//sw 32 bit rendering is bgrx
			{
				if (src[(f>>16)] != 255)
				{
					dest[x] = gamma[src[(f>>16)]];
				}
				f += fstep;
			}
		}
	}
}

void SWDraw_SubImage16(
		int scx, int scy, int scwidth, int scheight,	//screen
		int six, int siy, int siwidth, int siheight,	//sub image
		int iwidth, int iheight, qbyte *in)
{
	unsigned short *dest;
	qbyte *src;
	int v;
	int f, fstep;
	int x, y;
	int outstride = vid.rowbytes;

	dest = (unsigned short *)vid.buffer + scx + scy*outstride;

	fstep = (siwidth<<16)/scwidth;

	for (y=0 ; y<scheight ; y++, dest += outstride)
	{
		v = y*siheight/scheight;
		src = in + (siy+v)*iwidth + six;
		{
			f = 0;
			for (x=0; x < (scwidth&~3); x+=4)	//cut down on loop stuff
			{
				if (src[(f>>16)] != 255)
				{
					dest[x] = d_8to16table[src[(f>>16)]];
				}
				f += fstep;

				if (src[(f>>16)] != 255)
				{
					dest[1+x] = d_8to16table[src[(f>>16)]];
				}
				f += fstep;

				if (src[(f>>16)] != 255)
				{
					dest[2+x] = d_8to16table[src[(f>>16)]];
				}
				f += fstep;

				if (src[(f>>16)] != 255)
				{
					dest[3+x] = d_8to16table[src[(f>>16)]];
				}
				f += fstep;
			}
			for (; x<scwidth; x+=1)	//sw 32 bit rendering is bgrx
			{
				if (src[(f>>16)] != 255)
				{
					dest[x] = d_8to16table[src[(f>>16)]];
				}
				f += fstep;
			}
		}
	}
}

void SWDraw_SubImage8(
		int scx, int scy, int scwidth, int scheight,	//screen
		int six, int siy, int siwidth, int siheight,	//sub image
		int iwidth, int iheight, qbyte *in)
{
	unsigned char *dest;
	qbyte *src;
	int v;
	int f, fstep;
	int x, y;
	int outstride = vid.rowbytes;

	dest = (unsigned char *)vid.buffer + scx + scy*outstride;

	fstep = (siwidth<<16)/scwidth;

	if (ib_colorblend) // use palette remap
	{
		qbyte *palremap = ib_remap->pal;

		if (ib_alphablend) // remap with alpha
		{
			for (y=0 ; y<scheight ; y++, dest += outstride)
			{
				v = y*siheight/scheight;
				src = in + (siy+v)*iwidth + six;
				{
					f = 0;
					for (x=0; x < scwidth; x++) 
					{
						if (src[(f>>16)] != 255)
							dest[x] = Trans(dest[x], palremap[src[(f>>16)]]);
						f += fstep;
					}
				}
			}
		}
		else // remap without alpha
		{
			for (y=0 ; y<scheight ; y++, dest += outstride)
			{
				v = y*siheight/scheight;
				src = in + (siy+v)*iwidth + six;
				{
					f = 0;
					for (x=0; x < scwidth; x++) 
					{
						if (src[(f>>16)] != 255)
							dest[x] = palremap[src[(f>>16)]];
						f += fstep;
					}
				}
			}
		}
	}
	else if (ib_alphablend)
	{
		for (y=0 ; y<scheight ; y++, dest += outstride)
		{
			v = y*siheight/scheight;
			src = in + (siy+v)*iwidth + six;
			{
				f = 0;
				for (x=0; x < scwidth; x++) 
				{
					if (src[(f>>16)] != 255)
						dest[x] = Trans(dest[x], src[(f>>16)]);
					f += fstep;
				}
			}
		}
	}
	else
	{
		for (y=0 ; y<scheight ; y++, dest += outstride)
		{
			v = y*siheight/scheight;
			src = in + (siy+v)*iwidth + six;
			{
				f = 0;
				for (x=0; x < (scwidth&~3); x+=4) // loop for every 4 pixels (to hopefully optimize)
				{
					if (src[(f>>16)] != 255)
					{
						dest[x] = src[(f>>16)];
					}
					f += fstep;

					if (src[(f>>16)] != 255)
					{
						dest[1+x] = src[(f>>16)];
					}
					f += fstep;

					if (src[(f>>16)] != 255)
					{
						dest[2+x] = src[(f>>16)];
					}
					f += fstep;

					if (src[(f>>16)] != 255)
					{
						dest[3+x] = src[(f>>16)];
					}
					f += fstep;
				}
				for (; x<scwidth; x+=1)	// draw rest of the pixels needed
				{
					if (src[(f>>16)] != 255)
					{
						dest[x] = src[(f>>16)];
					}
					f += fstep;
				}
			}
		}
	}
}

void SWDraw_ImageColours (float r, float g, float b, float a)	//like glcolour4f
{
	int ri, gi, bi, ai;

	if (r_pixbytes == 1)
		D_SetTransLevel(a, BM_BLEND); // 8bpp doesn't maintain blending correctly

	ri = 255*r;
	gi = 255*g;
	bi = 255*b;
	ai = 255*a;

	if (ri == ib_ri && gi == ib_gi && bi == ib_bi && ai == ib_ai)
	{
		// nothing changed
		return;
	}

	ib_colorblend = (ri == 255 && gi == 255 && bi == 255) ? false : true;
	ib_alphablend = (ai == 255) ? false : true;

	ib_ri = ri;
	ib_gi = gi;
	ib_bi = bi;
	ib_ai = ai;

	switch (r_pixbytes)
	{
	case 1:
		D_DereferenceRemap(ib_remap);
		ib_remap = D_GetPaletteRemap(ri, gi, bi, false, true, TOP_DEFAULT, BOTTOM_DEFAULT);
		ib_index = GetPaletteIndex(ri, gi, bi);
		return;
	case 2:
		ib_index = ((ri << 3) >> 10) | ((gi << 3) >> 5) | (bi << 3);
		return;
	case 4:
		ib_index = (ri << 16) | (gi << 8) | bi;
		return;
	}
}

void SWDraw_Image (float xp, float yp, float wp, float hp, float s1, float t1, float s2, float t2, mpic_t *pic)
{
	float xend, yend, xratio, yratio;

	if (!pic)
		return;

	// image scale 
	xratio = pic->width / wp;
	yratio = pic->height / hp;
	
	// redefine borders
	s2 = (s2-s1)*pic->width;
	t2 = (t2-t1)*pic->height;
	s1 = s1*pic->width;
	t1 = t1*pic->height;

	// clip left/top edge
	if (xp < 0)
	{
		s1 -= xp * xratio;
		s2 += xp * xratio;
		wp += xp;
		xp = 0;
	}

	if (yp < 0)
	{
		t1 -= yp * yratio;
		t2 += yp * yratio;
		hp += yp;
		yp = 0;
	}

	// clip right/bottom edge
	xend = xp+wp;
	yend = yp+hp;

	if (xend > vid.width)
	{
		xend -= vid.width;
		s2 -= xend * xratio;
		wp -= xend;
	}

	if (yend > vid.height)
	{
		yend -= vid.height;
		t2 -= yend * yratio;
		hp -= yend;
	}

	// bounds check...
	if (s2 < 1)
		s2 = 1;
	if (t2 < 1)
		t2 = 1;

	if (wp < 1 || hp < 1 || s1 >= pic->width || t1 >= pic->height)
		return;

// draw the pic
	if (r_pixbytes == 1)
	{
		SWDraw_SubImage8(xp, yp, wp, hp, s1, t1, s2, t2, pic->width, pic->height, pic->data);
	}
	else if (r_pixbytes == 2)
	{
		SWDraw_SubImage16(xp, yp, wp, hp, s1, t1, s2, t2, pic->width, pic->height, pic->data);
	}
	else
	{	
		SWDraw_SubImage32(xp, yp, wp, hp, s1, t1, s2, t2, pic->width, pic->height, pic->data);
	}
}


/*
================
Draw_ConsoleBackground

================
*/
void SWDraw_ConsoleBackground (int lines)
{
	int				x, y, v, w, h;
	qbyte			*src;
	qbyte *dest;
	int				f, fstep;
	mpic_t			*conback;
	char			ver[100];
//	static			char saveback[320*8];

	if ((!scr_con_forcedraw && !scr_conalpha.value) || !lines)
		return;

	conback = (mpic_t *)SWDraw_SafeCachePic ("gfx/conback.lmp");
	if (!conback)
		conback = (mpic_t *)SWDraw_SafeCachePic("pics/conback.pcx");
	if (!conback)
		conback = (mpic_t *)SWDraw_SafeCachePic ("gfx/menu/conback.lmp");
	if (!conback || conback->width < 320 || conback->height < 200)
	{
		swcachepic_t *cp;

		for (cp=swmenu_cachepics, v=0 ; v<swmenu_numcachepics ; cp++, v++)
			if (!strcmp ("gfx/conback", cp->name))
				break;

		conback = Cache_Alloc(&cp->cache, sizeof(mpic_t) + 320*200, cp->name);
		conback->width = 320;
		conback->height = 200;
		conback->flags = 0;
	}

	w = conback->width;
	h = conback->height;

	if (lines > vid.conheight)
		lines = vid.conheight;

// hack the version number directly into the pic

	//sprintf (ver, "start commands with a \\ character %4.2f", VERSION);

	sprintf (ver, "%i", build_number());
	dest = conback->data + w + w*186 - 11 - 8*strlen(ver);

//	memcpy(saveback, conback->data + w*186, w*8);
	for (x=0 ; x<strlen(ver) ; x++)
		SWDraw_CharToConback (ver[x], dest+(x<<3));
	
// draw the pic
	if (r_pixbytes == 1)
	{
		dest = vid.conbuffer;

		if (scr_conalpha.value < 1 && !scr_con_forcedraw)
		{
			D_SetTransLevel(scr_conalpha.value, BM_BLEND);

			for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
			{
				v = (vid.conheight - lines + y)*h/vid.conheight;
				src = conback->data + v*w;
				f = 0;
				fstep = w*0x10000/vid.conwidth;
				for (x=0 ; x<vid.conwidth ; x+=4)
				{
					dest[x] = Trans(dest[x], src[f>>16]);
					f += fstep;
					dest[x+1] = Trans(dest[x+1], src[f>>16]);
					f += fstep;
					dest[x+2] = Trans(dest[x+2], src[f>>16]);
					f += fstep;
					dest[x+3] = Trans(dest[x+3], src[f>>16]);
					f += fstep;
				}
			}
		}
		else
		{
			for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
			{

				v = (vid.conheight - lines + y)*h/vid.conheight;
				src = conback->data + v*w;
				if (vid.conwidth == w)
					memcpy (dest, src, vid.conwidth);
				else
				{
					f = 0;
					fstep = w*0x10000/vid.conwidth;
					for (x=0 ; x<vid.conwidth ; x+=4)
					{
						dest[x] = src[f>>16];
						f += fstep;
						dest[x+1] = src[f>>16];
						f += fstep;
						dest[x+2] = src[f>>16];
						f += fstep;
						dest[x+3] = src[f>>16];
						f += fstep;
					}
				}
			}
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short *dest16 = (unsigned short *)vid.conbuffer;
		unsigned short *pal = d_8to16table;

		for (y=0 ; y<lines ; y++, dest16 += vid.conrowbytes)
		{

			v = (vid.conheight - lines + y)*h/vid.conheight;
			src = conback->data + v*w;
//			if (vid.conwidth == w)
//				memcpy (dest16, src, vid.conwidth);
//			else
			{
				f = 0;
				fstep = w*0x10000/vid.conwidth;
				for (x=0 ; x<vid.conwidth ; x+=4)
				{
					dest16[x] = pal[src[f>>16]];
					f += fstep;
					dest16[x+1] = pal[src[f>>16]];
					f += fstep;
					dest16[x+2] = pal[src[f>>16]];
					f += fstep;
					dest16[x+3] = pal[src[f>>16]];
					f += fstep;
				}
			}
		}
	}
	else
	{
		extern cvar_t d_smooth;
		unsigned int *p24dest;
		unsigned char *pal = (qbyte *)d_8to32table;
		int alpha;
		if (scr_con_forcedraw)
			alpha = 255;
		else
			alpha = scr_conalpha.value*255;
		p24dest = (unsigned int *)vid.conbuffer;
		dest = (unsigned char *)vid.conbuffer;	

		if (d_smooth.value)	//smoothed
		{
			qbyte			*src2;
			int f1, f2;
			int vf, hf;
			for (y=0 ; y<lines ; y++, dest += (vid.conrowbytes<<2))
			{
				v = (vid.conheight - lines + y)*(h-1)/vid.conheight;
				src = conback->data + v*w;
				v = (vid.conheight - lines + y)*(h-1)/vid.conheight+1;
				src2 = conback->data + v*w;

				v = (vid.conheight - lines + y)*(h-1)/vid.conheight;
				vf = (((vid.conheight - lines + y)*(h-1.0)/vid.conheight) - v) * 255;

				f = 0;
				fstep = (w-1)*0x10000/vid.conwidth;
				for (x=0 ; x<vid.conwidth ; x+=1)
				{
					hf = (f - (f>>16)*65536) / 256;
					f1 = ((255-hf) * pal[(src [f>>16]<<2) + 0] + hf * pal[(src [(f>>16)+1]<<2) + 0])/255;
					f2 = ((255-hf) * pal[(src2[f>>16]<<2) + 0] + hf * pal[(src2[(f>>16)+1]<<2) + 0])/255;
					f1 = ((255-vf)*f1+vf*f2)/255;
					dest[(x<<2) + 0] = ((255-alpha)*dest[(x<<2) + 0] + alpha*f1)/255;


					f1 = ((255-hf) * pal[(src [f>>16]<<2) + 1] + hf * pal[(src [(f>>16)+1]<<2) + 1])/255;
					f2 = ((255-hf) * pal[(src2[f>>16]<<2) + 1] + hf * pal[(src2[(f>>16)+1]<<2) + 1])/255;
					f1 = ((255-vf)*f1+vf*f2)/255;
					dest[(x<<2) + 1] = ((255-alpha)*dest[(x<<2) + 1] + alpha*f1)/255;


					f1 = ((255-hf) * pal[(src [f>>16]<<2) + 2] + hf * pal[(src [(f>>16)+1]<<2) + 2])/255;
					f2 = ((255-hf) * pal[(src2[f>>16]<<2) + 2] + hf * pal[(src2[(f>>16)+1]<<2) + 2])/255;
					f1 = ((255-vf)*f1+vf*f2)/255;
					dest[(x<<2) + 2] = ((255-alpha)*dest[(x<<2) + 2] + alpha*f1)/255;


					f += fstep;
				}
			}

		}
		else
			
			if (alpha != 255)	//blend it on
		{
			for (y=0 ; y<lines ; y++, dest += (vid.conrowbytes<<2))
			{
				v = (vid.conheight - lines + y)*h/vid.conheight;
				src = conback->data + v*w;

				f = 0;
				fstep = w*0x10000/vid.conwidth;
				for (x=0 ; x<vid.conwidth ; x+=1)
				{
					dest[(x<<2) + 0] = ((255-alpha)*dest[(x<<2) + 0] + alpha*pal[(src[f>>16]<<2) + 0])/255;
					dest[(x<<2) + 1] = ((255-alpha)*dest[(x<<2) + 1] + alpha*pal[(src[f>>16]<<2) + 1])/255;
					dest[(x<<2) + 2] = ((255-alpha)*dest[(x<<2) + 2] + alpha*pal[(src[f>>16]<<2) + 2])/255;
					f += fstep;
				}
			}

		}
		else	//block colour (fast)
		{
			for (y=0 ; y<lines ; y++, p24dest += vid.conrowbytes)
			{
				v = (vid.conheight - lines + y)*h/vid.conheight;
				src = conback->data + v*w;

				f = 0;
				fstep = w*0x10000/vid.conwidth;
				for (x=0 ; x<vid.conwidth ; x+=4)
				{
					p24dest[x] = d_8to32table[src[f>>16]];
					f += fstep;
					p24dest[x+1] = d_8to32table[src[f>>16]];
					f += fstep;
					p24dest[x+2] = d_8to32table[src[f>>16]];
					f += fstep;
					p24dest[x+3] = d_8to32table[src[f>>16]];
					f += fstep;
				}
			}
		}
	}
	// put it back
//	memcpy(conback->data + 320*186, saveback, 320*8);
}

void SWDraw_EditorBackground (int lines)
{
	SWDraw_ConsoleBackground (lines);
}


/*
==============
R_DrawRect8
==============
*/
void R_DrawRect8 (vrect_t *prect, int rowbytes, qbyte *psrc,
	int transparent)
{
	qbyte	t;
	int		i, j, srcdelta, destdelta;
	qbyte	*pdest;

	pdest = vid.buffer + (prect->y * vid.rowbytes) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = vid.rowbytes - prect->width;

	if (transparent)
	{
		for (i=0 ; i<prect->height ; i++)
		{
			for (j=0 ; j<prect->width ; j++)
			{
				t = *psrc;
				if (t != TRANSPARENT_COLOR)
				{
					*pdest = t;
				}

				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}
	else
	{
		for (i=0 ; i<prect->height ; i++)
		{
			memcpy (pdest, psrc, prect->width);
			psrc += rowbytes;
			pdest += vid.rowbytes;
		}
	}
}


/*
==============
R_DrawRect16
==============
*/
void R_DrawRect16 (vrect_t *prect, int rowbytes, qbyte *psrc,
	int transparent)
{
	qbyte			t;
	int				i, j, srcdelta, destdelta;
	unsigned short	*pdest;

// FIXME: would it be better to pre-expand native-format versions?

	pdest = (unsigned short *)vid.buffer +
			(prect->y * (vid.rowbytes)) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = (vid.rowbytes) - prect->width;

	if (transparent)
	{
		for (i=0 ; i<prect->height ; i++)
		{
			for (j=0 ; j<prect->width ; j++)
			{
				t = *psrc;
				if (t != TRANSPARENT_COLOR)
				{
					*pdest = d_8to16table[t];
				}

				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}
	else
	{
		for (i=0 ; i<prect->height ; i++)
		{
			for (j=0 ; j<prect->width ; j++)
			{
				*pdest = d_8to16table[*psrc];
				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}
}

void R_DrawRect32 (vrect_t *prect, int rowbytes, qbyte *psrc,
	int transparent)
{
	qbyte			t;
	int				i, j, srcdelta, destdelta;
	unsigned int	*pdest;

// FIXME: would it be better to pre-expand native-format versions?

	pdest = (unsigned int *)vid.buffer +
			(prect->y * vid.rowbytes) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = vid.rowbytes  - prect->width;

	if (transparent)
	{
		for (i=0 ; i<prect->height ; i++)
		{
			for (j=0 ; j<prect->width ; j++)
			{
				t = *psrc;
				if (t != TRANSPARENT_COLOR)
				{
					*pdest = d_8to32table[t];
				}

				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}
	else
	{
		for (i=0 ; i<prect->height ; i++)
		{
			for (j=0 ; j<prect->width ; j++)
			{

				*pdest = d_8to32table[*psrc];
				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void SWDraw_TileClear (int x, int y, int w, int h)
{
	int				width, height, tileoffsetx, tileoffsety;
	qbyte			*psrc;
	vrect_t			vr;

	if (!r_rectdesc.height || !r_rectdesc.width)
		return;

	r_rectdesc.rect.x = x;
	r_rectdesc.rect.y = y;
	r_rectdesc.rect.width = w;
	r_rectdesc.rect.height = h;

	vr.y = r_rectdesc.rect.y;
	height = r_rectdesc.rect.height;

	tileoffsety = vr.y % r_rectdesc.height;

	while (height > 0)
	{
		vr.x = r_rectdesc.rect.x;
		width = r_rectdesc.rect.width;

		if (tileoffsety != 0)
			vr.height = r_rectdesc.height - tileoffsety;
		else
			vr.height = r_rectdesc.height;

		if (vr.height > height)
			vr.height = height;

		tileoffsetx = vr.x % r_rectdesc.width;

		while (width > 0)
		{
			if (tileoffsetx != 0)
				vr.width = r_rectdesc.width - tileoffsetx;
			else
				vr.width = r_rectdesc.width;

			if (vr.width > width)
				vr.width = width;

			psrc = r_rectdesc.ptexbytes +
					(tileoffsety * r_rectdesc.rowbytes) + tileoffsetx;

			if (r_pixbytes == 1)
			{
				R_DrawRect8 (&vr, r_rectdesc.rowbytes, psrc, 0);
			}
			else if (r_pixbytes == 4)
			{
				R_DrawRect32 (&vr, r_rectdesc.rowbytes, psrc, 0);
			}
			else
			{
				R_DrawRect16 (&vr, r_rectdesc.rowbytes, psrc, 0);
			}

			vr.x += vr.width;
			width -= vr.width;
			tileoffsetx = 0;	// only the left tile can be left-clipped
		}

		vr.y += vr.height;
		height -= vr.height;
		tileoffsety = 0;		// only the top tile can be top-clipped
	}
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void SWDraw_Fill32 (int x, int y, int w, int h, unsigned int c)
{
	int u, v;
	unsigned int	*p32dest;

	p32dest = (unsigned int*)vid.buffer + y*vid.rowbytes + x;
	for (v=0 ; v<h ; v++, p32dest += vid.rowbytes)
		for (u=0 ; u<w ; u++)
			p32dest[u] = c;
}

void SWDraw_Fill16 (int x, int y, int w, int h, unsigned short c)
{
	int u, v;
	unsigned short	*p16dest;

	p16dest = (unsigned short*)vid.buffer + y*vid.rowbytes + x;
	for (v=0 ; v<h ; v++, p16dest += vid.rowbytes)
		for (u=0 ; u<w ; u++)
			p16dest[u] = c;
}

void SWDraw_Fill8 (int x, int y, int w, int h, unsigned char c)
{
	int u, v;

	qbyte			*dest;
	dest = vid.buffer + y*vid.rowbytes + x;
	for (v=0 ; v<h ; v++, dest += vid.rowbytes)
		for (u=0 ; u<w ; u++)
			dest[u] = c;
}

void SWDraw_Fill (int x, int y, int w, int h, int c)
{
	if (x < 0 || x + w > vid.width ||
		y < 0 || y + h > vid.height) {
		Con_Printf("Bad Draw_Fill(%d, %d, %d, %d, %c)\n",
			x, y, w, h, c);
		return;
	}

	switch (r_pixbytes)
	{
	case 1:
		SWDraw_Fill8(x, y, w, h, (unsigned char)c);
		break;
	case 2:
		SWDraw_Fill16(x, y, w, h, d_8to16table[c]);
		break;
	case 4:
		SWDraw_Fill32(x, y, w, h, d_8to32table[c]);
		break;
	}
}

void SWDraw_FillRGB (int x, int y, int w, int h, float r, float g, float b)
{
	unsigned int c;


	if (x < 0 || x + w > vid.width ||
		y < 0 || y + h > vid.height) {
		Con_Printf("Bad Draw_FillRGB(%d, %d, %d, %d)\n",
			x, y, w, h);
		return;
	}


	switch (r_pixbytes)
	{
	case 1:
		c = GetPaletteIndex(r*255, g*255, b*255);
		SWDraw_Fill8(x, y, w, h, (unsigned char)c);
		break;
	case 2:
		c = ((int)(r*31) << 10) | ((int)(g*31) << 5) | (int)(b*31);
		SWDraw_Fill16(x, y, w, h, (unsigned short)c);
		break;
	case 4:
		c = ((int)(r*255)<<16) | ((int)(g*255)<<8) | (int)(b*255);
		SWDraw_Fill32(x, y, w, h, c);
		break;
	}
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void SWDraw_FadeScreen (void)
{
	int			x,y;

	if (r_pixbytes == 4)
	{
		qbyte		*pbuf;
		for (y=0 ; y<vid.height ; y++)
		{
			pbuf = (qbyte *)((unsigned int *)vid.buffer + vid.rowbytes*y);
			for (x=0 ; x<vid.width*4 ; x++)
			{
				pbuf[x] /= 1.7;
			}
		}

	}
	else if (r_pixbytes == 2)
	{
		unsigned short		*pbuf;
		for (y=0 ; y<vid.height ; y++)
		{
			int	t;

			pbuf = (unsigned short *)vid.buffer + vid.rowbytes*y;
			t = (y & 1) << 1;

			for (x=0 ; x<vid.width ; x++)
			{
				if ((x & 3) != t)
					pbuf[x] = 0;
			}
		}
	}
	else
	{
		qbyte *pbuf;
		qbyte *mtpal = D_GetMenuTintPal();

		if (!mtpal)
			return; 

		for (y=0 ; y<vid.height ; y++)
		{

			pbuf = (qbyte *)(vid.buffer + vid.rowbytes*y);

			for (x=0 ; x<vid.width ; x++)
			{
				pbuf[x] = mtpal[pbuf[x]];
			}
		}
	}

	S_ExtraUpdate ();
}


//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void SWDraw_BeginDisc (void)
{
	if (draw_disc)
		D_BeginDirectRect (vid.width - 24, 0, draw_disc->data, 24, 24);
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void SWDraw_EndDisc (void)
{
	if (draw_disc)
		D_EndDirectRect (vid.width - 24, 0, 24, 24);
}

