/*
 
 Copyright (C) 2001-2002       A Nourai
 Copyright (C) 2006            Jacek Piszczek (Mac OSX port)
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 
 See the included (GNU.txt) GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"
#include "glquake.h"

#include <dlfcn.h>

//#include "vid_macos.h"

#define WARP_WIDTH		320
#define WARP_HEIGHT		200

// note: cocoa code is separated in vid_cocoa.m because of compilation pbs

cvar_t in_xflip = SCVAR("in_xflip", "0");

static int real_width, real_height;

static void *agllibrary;
static void *opengllibrary;
void *AGL_GetProcAddress(char *functionname)
{
	void *func;
	if (agllibrary)
	{
		func = dlsym(agllibrary, functionname);
		if (func)
			return func;
	}
	if (opengllibrary)
	{
		func = dlsym(opengllibrary, functionname);
		if (func)
			return func;
	}
	return NULL;
}

qboolean GLVID_Init(rendererstate_t *info, unsigned char *palette)
{
	int argnum;
	int i;

	agllibrary = dlopen("/System/Library/Frameworks/AGL.framework/AGL", RTLD_LAZY);
	if (!agllibrary)
	{
		Con_Printf("Couldn't load AGL framework\n");
		return false;
	}
	opengllibrary = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
	//don't care if opengl failed.

	vid.width = info->width;
	vid.height = info->height;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

    // initialise the NSApplication and the screen
	initCocoa(info);

    // initCocoa stores current screen size in vid
	real_width = vid.width;
	real_height = vid.height;


    // calculate the conwidth AFTER the screen has been opened
	if (vid.width <= 640)
	{
		vid.conwidth = vid.width;
		vid.conheight = vid.height;
	}
	else
	{
		vid.conwidth = vid.width/2;
		vid.conheight = vid.height/2;
	}
    
	if ((i = COM_CheckParm("-conwidth")) && i + 1 < com_argc)
	{
		vid.conwidth = Q_atoi(com_argv[i + 1]);
        
		// pick a conheight that matches with correct aspect
		vid.conheight = vid.conwidth * 3 / 4;
	}
    
	vid.conwidth &= 0xfff8; // make it a multiple of eight
    
	if ((i = COM_CheckParm("-conheight")) && i + 1 < com_argc)
		vid.conheight = Q_atoi(com_argv[i + 1]);
    
	if (vid.conwidth < 320)
		vid.conwidth = 320;
    
	if (vid.conheight < 200)
		vid.conheight = 200;
    
	vid.rowbytes = vid.width;
	vid.direct = 0; /* Isn't used anywhere, but whatever. */
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	if (vid.conheight > vid.height)
		vid.conheight = vid.height;
	if (vid.conwidth > vid.width)
		vid.conwidth = vid.width;

	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	GL_Init(AGL_GetProcAddress);

	GLVID_SetPalette(palette);

	vid.recalc_refdef = 1;

	return true;
}

void GLVID_DeInit(void)
{
	killCocoa();
}

void GL_DoSwap(void)
{
}

void GLVID_ForceLockState(int i)
{
}

int GLVID_ForceUnlockedAndReturnState(void)
{
	return 0;
}

void GLVID_SetPalette (unsigned char *palette)
{
	qbyte *pal;
	unsigned int r,g,b;
	int i;
	unsigned *table1;
	unsigned *table2;
	extern qbyte gammatable[256];

	Con_Printf("Converting 8to24\n");

	pal = palette;
	table1 = d_8to24rgbtable;
	table2 = d_8to24bgrtable;

	if (vid_hardwaregamma.value)
	{
		for (i=0 ; i<256 ; i++)
		{
			r = pal[0];
			g = pal[1];
			b = pal[2];
			pal += 3;

			*table1++ = LittleLong((255<<24) + (r<<0) + (g<<8) + (b<<16));
			*table2++ = LittleLong((255<<24) + (r<<16) + (g<<8) + (b<<0));
		}
	}
	else
	{
		for (i=0 ; i<256 ; i++)
		{
			r = gammatable[pal[0]];
			g = gammatable[pal[1]];
			b = gammatable[pal[2]];
			pal += 3;

			*table1++ = LittleLong((255<<24) + (r<<0) + (g<<8) + (b<<16));
			*table2++ = LittleLong((255<<24) + (r<<16) + (g<<8) + (b<<0));
		}
	}
	d_8to24bgrtable[255] &= LittleLong(0xffffff);	// 255 is transparent
	d_8to24rgbtable[255] &= LittleLong(0xffffff);	// 255 is transparent
	Con_Printf("Converted\n");
}

void Sys_SendKeyEvents(void)
{
}

void GLVID_LockBuffer(void)
{
}

void GLVID_UnlockBuffer(void)
{
}

qboolean GLVID_IsLocked(void)
{
	return 0;
}

void GLD_BeginDirectRect(int x, int y, qbyte *pbitmap, int width, int height)
{
}

void GLD_EndDirectRect(int x, int y, int width, int height)
{
}

void GLVID_SetCaption(char *text)
{
}

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = real_width;
	*height = real_height;
}

void GL_EndRendering(void)
{
	flushCocoa();
}

void GLVID_SetDeviceGammaRamp(unsigned short *ramps)
{
    cocoaGamma(ramps,ramps+256,ramps+512);
}

void GLVID_ShiftPalette(unsigned char *p)
{
	extern	unsigned short ramps[3][256];
	if (vid_hardwaregamma.value)
		GLVID_SetDeviceGammaRamp(ramps);
}


