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
#ifdef GLQUAKE
#include "glquake.h"
#include "shader.h"
#include "gl_draw.h"

#include <time.h>

void GLSCR_UpdateScreen (void);


extern qboolean	scr_drawdialog;

extern cvar_t vid_triplebuffer;
extern cvar_t          scr_fov;

extern qboolean        scr_initialized;
extern float oldsbar;
extern qboolean        scr_drawloading;

extern int scr_chatmode;
extern cvar_t scr_chatmodecvar;
extern cvar_t vid_conautoscale;
extern qboolean		scr_con_forcedraw;

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
	extern cvar_t vid_conheight;
	int uimenu;
#ifdef TEXTEDITOR
	extern qboolean editormodal;
#endif
	qboolean nohud;
	qboolean noworld;
	RSpeedMark();

	vid.numpages = 2 + vid_triplebuffer.value;

	if (scr_disabled_for_loading)
	{
		extern float scr_disabled_time;
		if (Sys_DoubleTime() - scr_disabled_time > 60 || key_dest != key_game)
		{
			scr_disabled_for_loading = false;
		}
		else
		{		
			GL_BeginRendering ();
			scr_drawloading = true;
			SCR_DrawLoading ();
			scr_drawloading = false;
			GL_EndRendering ();	
			GL_DoSwap();
			RSpeedEnd(RSPEED_TOTALREFRESH);
			return;
		}
	}

	if (!scr_initialized || !con_initialized)
	{
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;                         // not initialized yet
	}


	Shader_DoReload();

	GL_BeginRendering ();
#ifdef VM_UI
	uimenu = UI_MenuState();
#else
	uimenu = 0;
#endif

#ifdef TEXTEDITOR
	if (editormodal)
	{
		Editor_Draw();
		V_UpdatePalette (false);
#if defined(_WIN32) && defined(GLQUAKE)
		Media_RecordFrame();
#endif
		R2D_BrightenScreen();

		if (key_dest == key_console)
			Con_DrawConsole(vid.height/2, false);
		GL_EndRendering ();	
		GL_DoSwap();
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}
#endif
	if (Media_ShowFilm())
	{
		M_Draw(0);
		V_UpdatePalette (false);
#if defined(_WIN32) && defined(GLQUAKE)
		Media_RecordFrame();
#endif
		R2D_BrightenScreen();
		GL_EndRendering ();	
		GL_DoSwap();
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}
	
	//
	// determine size of refresh window
	//
	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	noworld = false;
	nohud = false;

#ifdef VM_CG
	if (CG_Refresh())
		nohud = true;
	else
#endif
#ifdef CSQC_DAT
		if (cls.state == ca_active && CSQC_DrawView())
		nohud = true;
	else
#endif
		if (uimenu != 1)
		{
			if (r_worldentity.model && cls.state == ca_active)
 				V_RenderView ();
			else
			{
				noworld = true;
			}
		}
	else
		GL_DoSwap();

	GL_Set2D (false);

	if (!noworld)
	{
		R2D_PolyBlend ();
		R2D_BrightenScreen();
	}

	scr_con_forcedraw = false;
	if (noworld)
	{
		extern char levelshotname[];

		if ((key_dest == key_console || key_dest == key_game) && SCR_GetLoadingStage() == LS_NONE)
			scr_con_current = scr_conlines = vid.height;

		//draw the levelshot or the conback fullscreen
		if (*levelshotname)
			R2D_ScalePic(0, 0, vid.width, vid.height, R2D_SafeCachePic (levelshotname));
		else if (scr_con_current != vid.height)
			R2D_ConsoleBackground(0, vid.height, true);
		else
			scr_con_forcedraw = true;

		nohud = true;
	}
	else if (!nohud)
		SCR_TileClear ();

	SCR_DrawTwoDimensional(uimenu, nohud);

	V_UpdatePalette (false);
#if defined(_WIN32) && defined(GLQUAKE)
	Media_RecordFrame();
#endif

	RSpeedEnd(RSPEED_TOTALREFRESH);
	RSpeedShow();

	GL_EndRendering ();
}


char *GLVID_GetRGBInfo(int prepadbytes, int *truewidth, int *trueheight)
{	//returns a BZ_Malloced array
	extern qboolean gammaworks;
	int i, c;
	qbyte *ret;

	if (gl_config.gles)
	{
		qbyte *p;

		// gles only guarantees GL_RGBA/GL_UNSIGNED_BYTE so downconvert and resize
		ret = BZ_Malloc(prepadbytes + vid.pixelwidth*vid.pixelheight*4);
		qglReadPixels (0, 0, vid.pixelwidth, vid.pixelheight, GL_RGBA, GL_UNSIGNED_BYTE, ret + prepadbytes); 

		c = vid.pixelwidth*vid.pixelheight;
		p = ret + prepadbytes;
		for (i = 1; i < c; i++)
		{
			p[i*3+0]=p[i*4+0];
			p[i*3+1]=p[i*4+1];
			p[i*3+2]=p[i*4+2];
		}
		ret = BZ_Realloc(ret, prepadbytes + vid.pixelwidth*vid.pixelheight*3);
	}
	else
	{
		ret = BZ_Malloc(prepadbytes + vid.pixelwidth*vid.pixelheight*3);
		qglReadPixels (0, 0, vid.pixelwidth, vid.pixelheight, GL_RGB, GL_UNSIGNED_BYTE, ret + prepadbytes); 
	}

	*truewidth = vid.pixelwidth;
	*trueheight = vid.pixelheight;

	if (gammaworks)
	{
		c = prepadbytes+vid.pixelwidth*vid.pixelheight*3;
		for (i=prepadbytes ; i<c ; i+=3)
		{
			extern qbyte		gammatable[256];
			ret[i+0] = gammatable[ret[i+0]];
			ret[i+1] = gammatable[ret[i+1]];
			ret[i+2] = gammatable[ret[i+2]];
		}
	}
	
	return ret;
}
#endif
