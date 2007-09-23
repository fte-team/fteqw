/*
Copyright (C) 2006-2007 Mark Olsen

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

#include <exec/exec.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

#include "quakedef.h"
#include "d_local.h"

qbyte vid_curpal[768];
static unsigned char pal[1024];

struct Window *window;
struct Screen *screen;

static void *pointermem;

static void *displaybuffer;

void ResetFrameBuffers(void)
{
	int vid_surfcachesize;
	void *vid_surfcache;
	int buffersize;

	if (d_pzbuffer)
	{
		D_FlushCaches();
		free(d_pzbuffer);
		d_pzbuffer = NULL;
	}
	buffersize = vid.width * vid.height * sizeof(*d_pzbuffer);
	vid_surfcachesize = D_SurfaceCacheForRes (vid.width, vid.height, 0);
	buffersize += vid_surfcachesize;

	d_pzbuffer = malloc(buffersize);
	vid_surfcache = (qbyte *) d_pzbuffer + vid.width * vid.height * sizeof(*d_pzbuffer);

	D_InitCaches(vid_surfcache, vid_surfcachesize);
}

qboolean SWVID_Init (rendererstate_t *info, unsigned char *palette)
{
	printf("SWVID_Init\n");

	printf("Trying to open a %dx%dx%d screen\n", info->width, info->height, info->bpp);

	vid.conwidth = vid.width = info->width;
	vid.conheight = vid.height = info->height;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	vid.direct = 0; /* We never have direct access to the currently displayed buffer */

	vid.numpages = 1;

	vid.colormap = host_colormap;

	r_pixbytes = info->bpp/8;

	redshift = 16;
	greenshift = 8;
	blueshift = 0;

	redbits = 8;
	greenbits = 8;
	bluebits = 8;


	displaybuffer = AllocVec(info->width*info->height*info->bpp/8, MEMF_ANY);
	if (displaybuffer)
	{
		memset(displaybuffer, -1, info->width*info->height*info->bpp/8);
		vid.conbuffer = vid.buffer = displaybuffer;
		vid.conrowbytes = vid.rowbytes = info->width*info->bpp/8;

		if (info->fullscreen)
		{
			screen = OpenScreenTags(0,
				SA_Width, vid.width,
				SA_Height, vid.height,
				SA_Depth, info->bpp,
				SA_Quiet, TRUE,
				TAG_DONE);
		}

		window = OpenWindowTags(0,
			WA_InnerWidth, info->width,
			WA_InnerHeight, info->height,
			WA_Title, "FTEQuake",
			WA_DragBar, screen?FALSE:TRUE,
			WA_DepthGadget, screen?FALSE:TRUE,
			WA_Borderless, screen?TRUE:FALSE,
			WA_RMBTrap, TRUE,
			screen?WA_PubScreen:TAG_IGNORE, (ULONG)screen,
			WA_Activate, TRUE,
			WA_ReportMouse, TRUE,
			TAG_DONE);

		if (window)
		{
			pointermem = AllocVec(256, MEMF_ANY|MEMF_CLEAR);
			if (pointermem)
			{
				SetPointer(window, pointermem, 16, 16, 0, 0);

				ResetFrameBuffers();

				return true;
			}
		}

		if (screen)
		{
			CloseScreen(screen);
			screen = 0;
		}

		FreeVec(displaybuffer);

		displaybuffer = 0;
	}

	return false;
}

void SWVID_Shutdown (void)
{
	if (window)
	{
		CloseWindow(window);
		window = 0;
	}

	if (screen)
	{
		CloseScreen(screen);
		screen = 0;
	}

	if (pointermem)
	{
		FreeVec(pointermem);
		pointermem = 0;
	}

	if (displaybuffer)
	{
		FreeVec(displaybuffer);
		displaybuffer = 0;
	}

	printf("SWVID_Shutdown\n");
}

void SWVID_ShiftPalette(unsigned char *p)
{
	SWVID_SetPalette(p);
}

void SWVID_SetPalette(unsigned char *palette)
{
	int i;

	ULONG spal[1+(256*3)+1];

	if (screen)
	{
		spal[0] = 256<<16;

		for(i=0;i<256;i++)
		{
			spal[1+(i*3)] = ((unsigned int)palette[i*3])<<24;
			spal[2+(i*3)] = ((unsigned int)palette[i*3+1])<<24;
			spal[3+(i*3)] = ((unsigned int)palette[i*3+2])<<24;
		}

		spal[1+(3*256)] = 0;

		LoadRGB32(&screen->ViewPort, spal);
	}

	memcpy(vid_curpal, palette, sizeof(vid_curpal));

	for(i=0;i<256;i++)
	{
		pal[i*4] = 0;
		pal[i*4+1] = palette[i*3+0];
		pal[i*4+2] = palette[i*3+1];
		pal[i*4+3] = palette[i*3+2];
	}
}

void SWVID_Update(vrect_t *rects)
{
#if 1
	while(rects)
	{
		if (r_pixbytes == 1)
		{
			if (screen)
				WritePixelArray(displaybuffer, rects->x, rects->y, vid.rowbytes, window->RPort, rects->x, rects->y, rects->width, rects->height, RECTFMT_LUT8);
			else
				WriteLUTPixelArray(displaybuffer, rects->x, rects->y, vid.rowbytes, window->RPort, pal, window->BorderLeft+rects->x, window->BorderTop+rects->y, rects->width, rects->height, CTABFMT_XRGB8);
		}
		else
#endif
			WritePixelArray(displaybuffer, 0, 0, vid.rowbytes*4, window->RPort, window->BorderLeft, window->BorderTop, vid.width, vid.height, RECTFMT_ARGB);

		rects = rects->pnext;
	}
}

void SWVID_LockBuffer (void)
{
}

void SWVID_UnlockBuffer (void)
{
}

void SWVID_ForceLockState (int lk)
{
}

int SWVID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void SWD_BeginDirectRect (int x, int y, qbyte *pbitmap, int width, int height)
{
}

void SWD_EndDirectRect (int x, int y, int width, int height)
{
}

void Sys_SendKeyEvents(void)
{
}

void SWVID_SetCaption(char *caption)
{
	SetWindowTitles(window, caption, (void *)-1);
}

