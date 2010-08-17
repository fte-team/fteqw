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

extern cvar_t gl_triplebuffer;
extern cvar_t          scr_fov;

extern qboolean        scr_initialized;
extern float oldsbar;
extern qboolean        scr_drawloading;

extern int scr_chatmode;
extern cvar_t scr_chatmodecvar;
extern cvar_t vid_conautoscale;
extern qboolean		scr_con_forcedraw;

// console size manipulation callbacks
void GLVID_Console_Resize(void)
{
	extern cvar_t gl_font;
	extern cvar_t vid_conwidth, vid_conheight;
	int cwidth, cheight;
	float xratio;
	float yratio=0;
	cwidth = vid_conwidth.value;
	cheight = vid_conheight.value;

	xratio = vid_conautoscale.value;
	if (xratio > 0)
	{
		char *s = strchr(vid_conautoscale.string, ' ');
		if (s)
			yratio = atof(s + 1);
		
		if (yratio <= 0)
			yratio = xratio;

		xratio = 1 / xratio;
		yratio = 1 / yratio;

		//autoscale overrides conwidth/height (without actually changing them)
		cwidth = vid.pixelwidth;
		cheight = vid.pixelheight;
	}
	else
	{
		xratio = 1;
		yratio = 1;
	}


	if (!cwidth)
		cwidth = vid.pixelwidth;
	if (!cheight)
		cheight = vid.pixelheight;

	cwidth*=xratio;
	cheight*=yratio;

	if (cwidth < 320)
		cwidth = 320;
	if (cheight < 200)
		cheight = 200;

	vid.width = cwidth;
	vid.height = cheight;

	vid.recalc_refdef = true;

	if (font_tiny)
		Font_Free(font_tiny);
	font_tiny = NULL;
	if (font_conchar)
		Font_Free(font_conchar);
	font_conchar = NULL;

	Cvar_ForceCallback(&gl_font);

#ifdef PLUGINS
	Plug_ResChanged();
#endif
}

void GL_Font_Callback(struct cvar_s *var, char *oldvalue)
{
	if (font_conchar)
		Font_Free(font_conchar);
	font_conchar = Font_LoadFont(8*vid.pixelheight/vid.height, var->string);
	if (!font_conchar && *var->string)
		font_conchar = Font_LoadFont(8*vid.pixelheight/vid.height, "");
}

void GLVID_Conheight_Callback(struct cvar_s *var, char *oldvalue)
{
	if (var->value > 1536)	//anything higher is unreadable.
	{
		Cvar_ForceSet(var, "1536");
		return;
	}
	if (var->value < 200 && var->value)	//lower would be wrong
	{
		Cvar_ForceSet(var, "200");
		return;
	}

	GLVID_Console_Resize();
}

void GLVID_Conwidth_Callback(struct cvar_s *var, char *oldvalue)
{
	//let let the user be too crazy
	if (var->value > 2048)	//anything higher is unreadable.
	{
		Cvar_ForceSet(var, "2048");
		return;
	}
	if (var->value < 320 && var->value)	//lower would be wrong
	{
		Cvar_ForceSet(var, "320");
		return;
	}

	GLVID_Console_Resize();
}

void GLVID_Conautoscale_Callback(struct cvar_s *var, char *oldvalue)
{
	GLVID_Console_Resize();
}

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

	vid.numpages = 2 + gl_triplebuffer.value;

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
		GLV_UpdatePalette (false, host_frametime);
#if defined(_WIN32) && defined(GLQUAKE)
		Media_RecordFrame();
#endif
		R2D_BrightenScreen();

		if (key_dest == key_console)
			Con_DrawConsole(vid_conheight.value/2, false);
		GL_EndRendering ();	
		GL_DoSwap();
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}
#endif
	if (Media_ShowFilm())
	{
		M_Draw(0);
		GLV_UpdatePalette (false, host_frametime);
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

	GL_Set2D ();

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
			scr_con_current = vid.height;

		//draw the levelshot or the conback fullscreen
		if (*levelshotname)
		{
			if(Draw_ScalePic)
				Draw_ScalePic(0, 0, vid.width, vid.height, Draw_SafeCachePic (levelshotname));
			else if (scr_con_current != vid.height)
				Draw_ConsoleBackground(0, vid.height, true);
		}
		else if (scr_con_current != vid.height)
			Draw_ConsoleBackground(0, vid.height, true);
		else
			scr_con_forcedraw = true;

		nohud = true;
	}
	else if (!nohud)
		SCR_TileClear ();

	SCR_DrawTwoDimensional(uimenu, nohud);

	GLV_UpdatePalette (false, host_frametime);
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
	qbyte *ret = BZ_Malloc(prepadbytes + vid.pixelwidth*vid.pixelheight*3);

	qglReadPixels (0, 0, vid.pixelwidth, vid.pixelheight, GL_RGB, GL_UNSIGNED_BYTE, ret + prepadbytes); 

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
