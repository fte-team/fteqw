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
#include "glquake.h"

#include <time.h>

void GLSCR_UpdateScreen (void);


extern qboolean	scr_drawdialog;

extern cvar_t gl_triplebuffer;
extern cvar_t          scr_fov;

extern qboolean        scr_initialized;
extern float oldsbar;
extern qboolean        scr_drawloading;

extern float   oldfov;


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

void GLSCR_UpdateScreen (void)
{
	extern cvar_t gl_2dscale;
	static float old2dscale=1;
	int uimenu;
#ifdef TEXTEDITOR
	extern qboolean editormodal, editoractive;
#endif
	if (block_drawing)
		return;

	if (gl_2dscale.modified)
	{
		gl_2dscale.modified=false;
		if (gl_2dscale.value < 0)	//lower would be wrong
			Cvar_Set(&gl_2dscale, "0");
		if (gl_2dscale.value > 2)	//anything higher is unreadable.
			Cvar_Set(&gl_2dscale, "2");

		old2dscale = gl_2dscale.value;
		vid.width = vid.conwidth = (glwidth - 320) * gl_2dscale.value + 320;
		vid.height = vid.conheight = (glheight - 240) * gl_2dscale.value + 240;

		vid.recalc_refdef = true;
		Con_CheckResize();
	}

	vid.numpages = 2 + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading)
	{
/*		if (Sys_DoubleTime() - scr_disabled_time > 60 || key_dest != key_game)
		{
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		}
		else
*/		{		
			GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
			SCR_DrawLoading ();
			GL_EndRendering ();	
			return;
		}
	}

	if (!scr_initialized || !con_initialized)
		return;                         // not initialized yet

	uimenu = UI_MenuState();


	if (oldsbar != cl_sbar.value) {
		oldsbar = cl_sbar.value;
		vid.recalc_refdef = true;
	}

	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

#ifdef TEXTEDITOR
	if (editormodal)
	{
		Editor_Draw();
		GLV_UpdatePalette ();
#if defined(_WIN32) && defined(RGLQUAKE)
		Media_RecordFrame();
#endif
		GLR_BrightenScreen();
		GL_EndRendering ();	
		return;
	}
#endif
	if (Media_ShowFilm())
	{
		M_Draw(0);
		GLV_UpdatePalette ();
#if defined(_WIN32) && defined(RGLQUAKE)
		Media_RecordFrame();
#endif
		GLR_BrightenScreen();
		GL_EndRendering ();	
		return;
	}
	
	//
	// determine size of refresh window
	//
	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (scr_chatmode != scr_chatmodecvar.value)
		vid.recalc_refdef = true;

	if (vid.recalc_refdef || scr_viewsize.modified)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();
	if (cl.worldmodel && uimenu != 1)
		V_RenderView ();

	GL_Set2D ();

	//
	// draw any areas not covered by the refresh
	//
	SCR_TileClear ();

	if (r_netgraph.value)
		GLR_NetGraph ();

	if (scr_drawdialog)
	{
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
		scr_copyeverything = true;
	}
	else if (scr_drawloading)
	{
		SCR_DrawLoading ();
		Sbar_Draw ();
	}
	else if (cl.intermission == 1 && key_dest == key_game)
	{
		Sbar_IntermissionOverlay ();
		M_Draw (uimenu);
	}
	else if (cl.intermission == 2 && key_dest == key_game)
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else
	{		
		Draw_Crosshair();

		SCR_DrawRam ();
		SCR_DrawNet ();
		SCR_DrawFPS ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		Sbar_Draw ();
	glTexEnvi ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
#ifdef TEXTEDITOR
		if (editoractive)
			Editor_Draw();
#endif
		M_Draw (uimenu);
		SCR_DrawConsole (false);
	}

	GLR_BrightenScreen();

	GLV_UpdatePalette ();
#if defined(_WIN32) && defined(RGLQUAKE)
	Media_RecordFrame();
#endif
	GL_EndRendering ();	
}


char *GLVID_GetRGBInfo(int prepadbytes, int *truewidth, int *trueheight)
{	//returns a BZ_Malloced array
	qbyte *ret = BZ_Malloc(prepadbytes + glwidth*glheight*3);

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, ret + prepadbytes); 

	*truewidth = glwidth;
	*trueheight = glheight;
	
	return ret;
}
