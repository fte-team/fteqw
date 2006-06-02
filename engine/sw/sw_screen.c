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
// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "r_local.h"



extern qboolean	scr_drawdialog;

extern cvar_t gl_triplebuffer;
extern cvar_t          scr_fov;

extern qboolean        scr_initialized;
extern float oldsbar;
extern qboolean        scr_drawloading;

extern int scr_chatmode;
extern cvar_t scr_chatmodecvar;

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
extern void D_SetTransLevel(float level, blendmode_t blend);
extern qbyte Trans(qbyte p, qbyte p2);

void SWSCR_UpdateScreen (void)
{
	qboolean nohud;
	int uimenu;
	vrect_t		vrect;

	if (scr_skipupdate || block_drawing)
		return;

	if (scr_disabled_for_loading)
	{
		if (key_dest != key_console)
			return;
	}

#ifdef _WIN32
	{	// don't suck up any cpu if minimized
		extern qboolean Minimized;
		if (Minimized)
			return;
	}
#endif

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet

#ifdef VM_UI
	uimenu = UI_MenuState();
#else
	uimenu = 0;
#endif

#ifdef TEXTEDITOR
	if (editormodal)
	{
		Editor_Draw();
		SWV_UpdatePalette (false);

		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.pnext = 0;
	
		SWVID_Update (&vrect);
		return;
	}
#endif
	if (Media_ShowFilm())
	{
		SWV_UpdatePalette (false);

		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.pnext = 0;
	
		SWVID_Update (&vrect);
		return;
	}

	if (vid.recalc_refdef)
	{
		// something changed, so reorder the screen
		SCR_CalcRefdef ();
	}

//
// do 3D refresh drawing, and then update the screen
//
	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly

	SCR_TileClear();
	SCR_SetUpToDrawConsole ();
	SCR_EraseCenterString ();
	
	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in
									//  for linear writes all the time

	nohud = false;
#ifdef TEXTEDIT
	if (!editormodal)	//don't render view.
#endif
	{
#ifdef CSQC_DAT
		if (CSQC_DrawView())
			nohud = true;
		else
#endif

		if (cl.worldmodel)
		{
			VID_LockBuffer ();
			V_RenderView ();
			VID_UnlockBuffer ();
		}
	}

	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly

	SCR_DrawTwoDimensional(uimenu, nohud);

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in
									//  for linear writes all the time

	SWV_UpdatePalette (false);

//
// update one of three areas
//
	if (scr_copyeverything)
	{
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.pnext = 0;
	
		SWVID_Update (&vrect);
	}
	else if (scr_copytop)
	{
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height - sb_lines;
		vrect.pnext = 0;
	
		SWVID_Update (&vrect);
	}	
	else
	{
		vrect.x = scr_vrect.x;
		vrect.y = scr_vrect.y;
		vrect.width = scr_vrect.width;
		vrect.height = scr_vrect.height;
		vrect.pnext = 0;
	
		SWVID_Update (&vrect);
	}	
}

/*
==================
SCR_UpdateWholeScreen
==================
*/
void SCR_UpdateWholeScreen (void)
{
	scr_fullupdate = 0;
	SCR_UpdateScreen ();
}







char *SWVID_GetRGBInfo(int prepadbytes, int *truewidth, int *trueheight)
{	//returns a BZ_Malloced array
	qbyte *ret = BZ_Malloc(prepadbytes + vid.width*vid.height*3);

	qbyte *dest, *src;
	int y, x;

	extern unsigned char	vid_curpal[256*3];

	if (r_pixbytes == 4)
	{	//32 bit to 24
		dest = ret+prepadbytes + vid.width*3*(vid.height-1);

		for (y=0 ; y<vid.height ; y++, dest -= vid.width*3)
		{
			src = vid.buffer + y*vid.rowbytes*4;
			for (x=0 ; x<vid.width*3 ; x+=3, src+=4)
			{
				dest[x]		= src[2];
				dest[x+1]	= src[1];
				dest[x+2]	= src[0];
			}
		}
	}
	else
	{	//8bit to 24 using palette lookups
		dest = ret+prepadbytes + vid.width*3*(vid.height-1);

		for (y=0 ; y<vid.height ; y++, dest -= vid.width*3)
		{
			src = vid.buffer + y*vid.rowbytes;
			for (x=0 ; x<vid.width*3 ; x+=3, src++)
			{
				dest[x]		= vid_curpal[*src*3];
				dest[x+1]	= vid_curpal[*src*3+1];
				dest[x+2]	= vid_curpal[*src*3+2];
			}
		}
	}


	*truewidth = vid.width;
	*trueheight = vid.height;
	
	return ret;
}
