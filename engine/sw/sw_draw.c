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

extern unsigned int *d_8to32table;

typedef struct {
	vrect_t	rect;
	int		width;
	int		height;
	qbyte	*ptexbytes;
	int		rowbytes;
} rectdesc_t;

static rectdesc_t	r_rectdesc;

qbyte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

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

/*
================
Draw_CachePic
================
*/
qpic_t	*SWDraw_SafeCachePic (char *extpath)
{
	swcachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	char alternatename[MAX_QPATH];
	char path[MAX_QPATH];
	Q_strncpyz(path, extpath, sizeof(path));
	COM_StripExtension(path, path);
	
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
		_snprintf(alternatename, MAX_QPATH-1,"%s.pcx", path);

		file = COM_LoadMallocFile(alternatename);
		if (file)
		{
			image = ReadPCXFile(file, com_filesize, &width, &height);
			BZ_Free(file);
			if (image)
			{
				dat = Cache_Alloc(&pic->cache, sizeof(qpic_t) + width*height, path);
				dat->width = width;
				dat->height = height;
				for (i = 0; i < width*height; i++)
				{
					if (image[i*4+3] < 64) // 25% threshhold
						dat->data[i] = 255;
					else
						dat->data[i] = GetPalette(image[i*4], image[i*4+1], image[i*4+2]);
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
		_snprintf(alternatename, MAX_QPATH-1,"%s.jpg", path);

		file = COM_LoadMallocFile(alternatename);
		if (file)
		{
			image = ReadJPEGFile(file, com_filesize, &width, &height);
			BZ_Free(file);
			if (image)
			{
				dat = Cache_Alloc(&pic->cache, sizeof(qpic_t) + width*height, path);
				dat->width = width;
				dat->height = height;
				for (i = 0; i < width*height; i++)
				{
					if (image[i*4+3] < 64) // 25% threshhold
						dat->data[i] = 255;
					else
						dat->data[i] = GetPalette(image[i*4], image[i*4+1], image[i*4+2]);
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
		_snprintf(alternatename, MAX_QPATH-1,"%s.tga", path);

		file = COM_LoadMallocFile(alternatename);
		if (file)
		{
			image = ReadTargaFile (file, com_filesize, &width, &height, 0);
			BZ_Free(file);
			if (image)
			{
				dat = Cache_Alloc(&pic->cache, sizeof(qpic_t) + width*height, path);
				dat->width = width;
				dat->height = height;
				for (i = 0; i < width*height; i++)
				{
					if (image[i*4+3] < 64) // 25% threshhold
						dat->data[i] = 255;
					else
						dat->data[i] = GetPalette(image[i*4], image[i*4+1], image[i*4+2]);
				}

				BZ_Free(image);

				return dat;
			}
		}
	}

//
// load the pic from disk
//
	_snprintf(alternatename, MAX_QPATH-1,"%s.lmp", path);
	COM_LoadCacheFile (alternatename, &pic->cache);
	
	dat = (qpic_t *)pic->cache.data;
	if (!dat)
	{
		char alternatename[MAX_QPATH];
		sprintf(alternatename, "gfx/%s.lmp", path);
		dat = (qpic_t *)COM_LoadTempFile (alternatename);
		if (!dat)
			return NULL;
//		Sys_Error ("Draw_CachePic: failed to load %s", path);
	}

	SwapPic (dat);

	return dat;
}
qpic_t	*SWDraw_CachePic (char *path)
{
	qpic_t	*pic;
	pic = SWDraw_SafeCachePic(path);
	if (!pic)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
		
	return pic;
}

qpic_t *SWDraw_ConcharsMalloc (char *name)
{
	// stupid hack for conchars...
	qpic_t *dat;
	swcachepic_t *pic;
	int i, j;

	for (pic=swmenu_cachepics, i=0 ; i<swmenu_numcachepics ; pic++, i++)
		if (!strcmp (name, pic->name))
			break;	

	if (i == swmenu_numcachepics)
	{
		if (swmenu_numcachepics == MAX_CACHED_PICS)
			Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
		swmenu_numcachepics++;
		pic->cache.fake = true;
		pic->cache.data = BZ_Malloc(sizeof(qpic_t) + 128*128);
		dat = pic->cache.data;
		// change 0 to 255 through conchars
		for (j = 0; j < 128*128; j++)
			dat->data[j] = (draw_chars[j] == 255 || !draw_chars[j]) ? draw_chars[j] ^ 255 : draw_chars[j];
//		memcpy (dat->data, draw_chars, 128*128);
		dat->width = dat->height = 128;
		strcpy (pic->name, name);
	}

	return pic->cache.data;
}

qpic_t	*SWDraw_MallocPic (char *path)
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
			Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
		swmenu_numcachepics++;
		pic->cache.fake = false;
		pic->cache.data = NULL;
		strcpy (pic->name, path);
	}

	dat = Cache_Check (&pic->cache);

	if (dat)
		return dat;



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
					dat->width = width;
					dat->height = height;
					for (i = 0; i < width*height; i++)
						dat->data[i] = GetPalette(image[i*4], image[i*4+1], image[i*4+2]);

					BZ_Free(image);

					return dat;
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

	return dat;
}
qpic_t	*SWDraw_PicFromWad (char *name)
{
	char q2name[MAX_QPATH];
	qpic_t *qpic;

	if (!strcmp(name, "conchars")) // conchars hack
		return SWDraw_ConcharsMalloc("conchars");
	
	sprintf(q2name, "pics/%s.pcx", name);
	qpic = SWDraw_MallocPic(q2name);
	if (qpic)
		return qpic;

	return W_SafeGetLumpName (name);
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
	draw_chars = W_SafeGetLumpName ("conchars");	//q1
	if (!draw_chars)
	{
		qpic_t *pic;	//try q2
		int i;
		int s;
		pic = SWDraw_MallocPic("pics/conchars.pcx");	//safe from host_hunkmarks...
		if (pic)
		{
			draw_chars = pic->data;

			s = pic->width*pic->height;
			for (i = 0; i < s; i++)	//convert 255s to 0, q1's transparent colour
				if (draw_chars[i] == 162 || draw_chars[i] == 255)
					draw_chars[i] = 0;
		}
	}
	if (!draw_chars)
	{	//now go for hexen2
		int i, x;
		char *tempchars = COM_LoadMallocFile("gfx/menu/conchars.lmp");
		char *in, *out;
		if (!tempchars)
			Sys_Error("No charset found\n");

		draw_chars = BZ_Malloc(8*8*256*8);
		
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
	if (!draw_chars)
		Sys_Error("Failed to find suitable console charactures\n");
	draw_disc = W_SafeGetLumpName ("disc");
	draw_backtile = W_SafeGetLumpName ("backtile");
	if (!draw_backtile)
		draw_backtile = (qpic_t	*)COM_LoadMallocFile("gfx/menu/backtile.lmp");

	if (draw_backtile)
	{
		r_rectdesc.width = draw_backtile->width;
		r_rectdesc.height = draw_backtile->height;
		r_rectdesc.ptexbytes = draw_backtile->data;
		r_rectdesc.rowbytes = draw_backtile->width;
	}
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

#define FindPallete(r,g,b) palxxxto8[((r&palmask[0])>>palshift[0]) | ((g&palmask[1])<<palshift[1]) | ((b&palmask[2])<<palshift[2])]
#define colourmask(p,r,g,b) FindPallete(host_basepal[p*3]*r, host_basepal[p*3+1]*g, host_basepal[p*3+2]*b)
#define draw(p) colourmask(p, (int)consolecolours[colour].r, (int)consolecolours[colour].g, (int)consolecolours[colour].b)
void SWDraw_ColouredCharacter (int x, int y, unsigned int num)
{
	qbyte			*source;
	int				drawline;	
	int				row, col;

int colour;
	
	if (y <= -8)
		return;			// totally off screen

	if (y > vid.height - 8 || x < 0 || x > vid.width - 8)
		return;

	colour = (num&CON_COLOURMASK)/256;

	if (num & CON_BLINKTEXT)
	{
		if ((int)(cl.time*2) & 1)
			return;
	}

	if (colour == 0)	//0 is white anyway (speedup)
	{
		Draw_Character(x, y, num);
		return;
	}

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
	else if (r_pixbytes == 2)
	{
		unsigned short *dest16;
		unsigned char *pal = (unsigned char *)d_8to32table;
		int i;
		int rm, gm, bm;

		dest16 = (unsigned short *)vid.conbuffer + y*vid.conrowbytes + x;

		rm = consolecolours[colour].r*32;
		gm = consolecolours[colour].g*32;
		bm = consolecolours[colour].b*32;
	
		while (drawline--)
		{
			for (i = 0; i < 8; i++)
			{
				if (source[i])
					dest16[i] = ((pal[source[i]*4+0]*bm/256)<<10)+
								((pal[source[i]*4+1]*gm/256)<<5)+
								pal[source[i]*4+2]*rm/256;
			}
				/*
			if (source[0])
				dest16[0] = pal[draw(source[0])];
			if (source[1])
				dest16[1] = pal[draw(source[1])];
			if (source[2])
				dest16[2] = pal[draw(source[2])];
			if (source[3])
				dest16[3] = pal[draw(source[3])];
			if (source[4])
				dest16[4] = pal[draw(source[4])];
			if (source[5])
				dest16[5] = pal[draw(source[5])];
			if (source[6])
				dest16[6] = pal[draw(source[6])];
			if (source[7])
				dest16[7] = pal[draw(source[7])];
				*/
			source += 128;
			dest16 += vid.conrowbytes;
		}
	}
	else if (r_pixbytes == 4)
	{
		qbyte			*dest;
		int i;
		unsigned char *pal = (unsigned char *)d_8to32table;
		dest = vid.conbuffer + (y*vid.conrowbytes + x)*r_pixbytes;
	
		while (drawline--)
		{
			for (i = 0; i < 8; i++)
			{
				if (source[i])
				{
					dest[0+i*4] = pal[source[i]*4+0]*consolecolours[colour].b;
					dest[1+i*4] = pal[source[i]*4+1]*consolecolours[colour].g;
					dest[2+i*4] = pal[source[i]*4+2]*consolecolours[colour].r;
				}				
			}
			source += 128;
			dest += vid.conrowbytes*r_pixbytes;
		}
	}
}
#undef draw

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

#include "crosshairs.dat"
qbyte *COM_LoadFile (char *path, int usehunk);
void SWDraw_Crosshair(void)
{
	int x, y;
	extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor;
	extern vrect_t		scr_vrect;
	qbyte c = (qbyte)crosshaircolor.value;
	qbyte c2 = (qbyte)crosshaircolor.value;

	int sc;
	
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
void SWDraw_Pic (int x, int y, qpic_t *pic)
{
	qbyte			*dest, *source;
	int				v, u;

	if (!pic)
		return;

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
void SWDraw_SubPic(int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height)
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
void SWDraw_TransPic (int x, int y, qpic_t *pic)
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
void SWDraw_TransPicTranslate (int x, int y, qpic_t *pic, qbyte *translation)
{
	qbyte	*source, tbyte;
	int				v, u;

	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
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
						dest[u] = translation[tbyte];

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
				source += pic->width;
			}
		}
	}
	else if (r_pixbytes == 4)
	{
		unsigned int	*puidest;

		puidest = (unsigned int	*)(vid.buffer + ((y * vid.rowbytes + x) << 2));

		if (pic->width & 7)
		{	// general
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						puidest[u] = d_8to32table[translation[tbyte]];

				puidest += vid.rowbytes;
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
				source += pic->width;
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

//blend in colour and alpha (still 8 bit source though)
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
			for (; x<scwidth; x+=1)	//sw 32 bit rendering is bgrx
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

static qboolean SWDraw_Image_Blend;
static int SWDraw_Image_Red=1, SWDraw_Image_Green=1, SWDraw_Image_Blue=1, SWDraw_Image_Alpha=1;
void SWDraw_ImageColours (float r, float g, float b, float a)	//like glcolour4f
{
	SWDraw_Image_Red=r*255;
	SWDraw_Image_Green=g*255;
	SWDraw_Image_Blue=b*255;
	SWDraw_Image_Alpha=a*255;

	SWDraw_Image_Blend = r<1 || b<1 || g<1 || a<1;
}

void SWDraw_Image (float xp, float yp, float wp, float hp, float s1, float t1, float s2, float t2, qpic_t *pic)
{
	float xend, yend, xratio, yratio;

	if (!pic)
		return;

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
		if (SWDraw_Image_Blend)	//blend it on
		{
			SWDraw_SubImageBlend32(xp, yp, wp, hp, s1, t1, s2, t2,
							pic->width, pic->height, pic->data,
							SWDraw_Image_Red, SWDraw_Image_Green, SWDraw_Image_Blue, SWDraw_Image_Alpha);

		}
		else	//block colour (fast)
		{
			SWDraw_SubImage32(xp, yp, wp, hp, s1, t1, s2, t2, pic->width, pic->height, pic->data);
		}
	}
}


/*
================
Draw_ConsoleBackground

================
*/
void SWDraw_ConsoleBackground (int lines)
{
	int				x, y, v;
	qbyte			*src;
	qbyte *dest;
	int				f, fstep;
	qpic_t			*conback;
	char			ver[100];
	static			char saveback[320*8];

	conback = SWDraw_SafeCachePic ("gfx/conback.lmp");
	if (!conback)
		conback = SWDraw_SafeCachePic("pics/conback.pcx");
	if (!conback)
		conback = SWDraw_SafeCachePic ("gfx/menu/conback.lmp");
	if (!conback)
		Sys_Error("gfx/conback.lmp not found\n");

	if (lines > vid.conheight)
		lines = vid.conheight;

// hack the version number directly into the pic

	//sprintf (ver, "start commands with a \\ character %4.2f", VERSION);

	sprintf (ver, "%4.2f", VERSION);
	dest = conback->data + 320 + 320*186 - 11 - 8*strlen(ver);

	memcpy(saveback, conback->data + 320*186, 320*8);
	for (x=0 ; x<strlen(ver) ; x++)
		SWDraw_CharToConback (ver[x], dest+(x<<3));
	
// draw the pic
	if (r_pixbytes == 1)
	{
		dest = vid.conbuffer;

		for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
		{

			v = (vid.conheight - lines + y)*200/vid.conheight;
			src = conback->data + v*320;
			if (vid.conwidth == 320)
				memcpy (dest, src, vid.conwidth);
			else
			{
				f = 0;
				fstep = 320*0x10000/vid.conwidth;
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
	else if (r_pixbytes == 2)
	{
		unsigned short *dest16 = (unsigned short *)vid.conbuffer;
		unsigned short *pal = d_8to16table;

		for (y=0 ; y<lines ; y++, dest16 += vid.conrowbytes)
		{

			v = (vid.conheight - lines + y)*200/vid.conheight;
			src = conback->data + v*320;
//			if (vid.conwidth == 320)
//				memcpy (dest16, src, vid.conwidth);
//			else
			{
				f = 0;
				fstep = 320*0x10000/vid.conwidth;
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
		int alpha = ((float)(lines)/((vid.height * 3) >> 2))*255;
		if (alpha > 255) alpha = 255;
		p24dest = (unsigned int *)vid.conbuffer;
		dest = (unsigned char *)vid.conbuffer;	

		if (d_smooth.value)	//smoothed
		{
			qbyte			*src2;
			int f1, f2;
			int vf, hf;
			for (y=0 ; y<lines ; y++, dest += (vid.conrowbytes<<2))
			{
				v = (vid.conheight - lines + y)*199/vid.conheight;
				src = conback->data + v*320;
				v = (vid.conheight - lines + y)*199/vid.conheight+1;
				src2 = conback->data + v*320;

				v = (vid.conheight - lines + y)*199/vid.conheight;
				vf = (((vid.conheight - lines + y)*199.0/vid.conheight) - v) * 255;

				f = 0;
				fstep = 319*0x10000/vid.conwidth;
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
				v = (vid.conheight - lines + y)*200/vid.conheight;
				src = conback->data + v*320;

				f = 0;
				fstep = 320*0x10000/vid.conwidth;
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
				v = (vid.conheight - lines + y)*200/vid.conheight;
				src = conback->data + v*320;

				f = 0;
				fstep = 320*0x10000/vid.conwidth;
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
	memcpy(conback->data + 320*186, saveback, 320*8);
}

#ifdef TEXTEDITOR
void SWDraw_EditorBackground (int lines)
{
	SWDraw_ConsoleBackground (lines);
}
#endif


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
void SWDraw_Fill (int x, int y, int w, int h, int c)
{
	int				u, v;

	if (x < 0 || x + w > vid.width ||
		y < 0 || y + h > vid.height) {
		Con_Printf("Bad Draw_Fill(%d, %d, %d, %d, %c)\n",
			x, y, w, h, c);
		return;
	}

	if (r_pixbytes == 1)
	{
		qbyte			*dest;
		dest = vid.buffer + y*vid.rowbytes + x;
		for (v=0 ; v<h ; v++, dest += vid.rowbytes)
			for (u=0 ; u<w ; u++)
				dest[u] = c;
	}
	else if (r_pixbytes == 4)
	{
		unsigned int	*p32dest;

		p32dest = (unsigned int*)vid.buffer + y*vid.rowbytes + x;
		for (v=0 ; v<h ; v++, p32dest += vid.rowbytes)
			for (u=0 ; u<w ; u++)
				p32dest[u] = d_8to32table[c];
	}
	else if (r_pixbytes == 2)
	{
		unsigned short	*p16dest;

		p16dest = (unsigned short*)vid.buffer + y*vid.rowbytes + x;
		for (v=0 ; v<h ; v++, p16dest += vid.rowbytes)
			for (u=0 ; u<w ; u++)
				p16dest[u] = d_8to16table[c];
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

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();

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
		qbyte		*pbuf;
		for (y=0 ; y<vid.height ; y++)
		{
			int	t;

			pbuf = (qbyte *)(vid.buffer + vid.rowbytes*y);
			t = (y & 1) << 1;

			for (x=0 ; x<vid.width ; x++)
			{
				if ((x & 3) != t)
					pbuf[x] = 0;
			}
		}
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}






void SWDraw_Box(int x1, int y1, int x2, int y2, int paletteindex, float alpha)
{
	int x;
	int y;
	int w;
	int h;

	qbyte			*dest;
	unsigned int	*puidest;
	unsigned		uc;
	int				u, v;

	Set_TransLevelF(alpha);
	if (t_state & TT_ZERO)
		return;

	if (x1 < x2)
	{
		x = x1;
		w = x2-x1;
	}
	else
	{
		x = x2;
		w = x1-x2;
	}

	if (y1 < y2)
	{
		y = y1;
		h = y2-y1;
	}
	else
	{
		y = y2;
		h = y1-y2;
	}


	if (x < 0 || x + w > vid.width ||
		y < 0 || y + h > vid.height) {
		Con_Printf("Bad SWDraw_Box(%d, %d, %d, %d, %i)\n",
			x, y, w, h, paletteindex);
		return;
	}

	if (r_pixbytes == 1)
	{
		if (t_state & TT_ONE)
		{
			dest = vid.buffer + y*vid.rowbytes + x;
			for (v=0 ; v<h ; v++, dest += vid.rowbytes)
				for (u=0 ; u<w ; u++)
					dest[u] = paletteindex;
		}
		else
		{
			dest = vid.buffer + y*vid.rowbytes + x;
			for (v=0 ; v<h ; v++, dest += vid.rowbytes)
				for (u=0 ; u<w ; u++)
					dest[u] = Trans(dest[u], paletteindex);
		}
	}
	else if (r_pixbytes == 4)
	{
		uc = d_8to32table[paletteindex];

		puidest = (unsigned int *)vid.buffer + y * (vid.rowbytes) + x;
		for (v=0 ; v<h ; v++, puidest += vid.rowbytes)
			for (u=0 ; u<w ; u++)
				puidest[u] = uc;
	}
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

