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
#ifdef RGLQUAKE
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



void RSpeedShow(void)
{
	int i;
	static int samplerspeeds[RSPEED_MAX];
	static int samplerquant[RQUANT_MAX];
	char *RSpNames[RSPEED_MAX];
	char *RQntNames[RQUANT_MAX];
	char *s;
	static int framecount;

	if (!r_speeds.value)
		return;

	memset(RSpNames, 0, sizeof(RSpNames));
	RSpNames[RSPEED_TOTALREFRESH] = "Total refresh";
	RSpNames[RSPEED_CLIENT] = "Protocol and entity setup";
	RSpNames[RSPEED_WORLDNODE] = "World walking";
	RSpNames[RSPEED_WORLD] = "World rendering";
	RSpNames[RSPEED_DYNAMIC] = "Lightmap updates";
	RSpNames[RSPEED_PARTICLES] = "Particle physics and sorting";
	RSpNames[RSPEED_PARTICLESDRAW] = "Particle drawing";
	RSpNames[RSPEED_2D] = "2d elements";
	RSpNames[RSPEED_SERVER] = "Server";

	RSpNames[RSPEED_PALETTEFLASHES] = "Palette flashes";
	RSpNames[RSPEED_STENCILSHADOWS] = "Stencil Shadows";

	RSpNames[RSPEED_FULLBRIGHTS] = "World fullbrights";

	RSpNames[RSPEED_FINISH] = "Waiting for card to catch up";

	RQntNames[RQUANT_MSECS] = "Microseconds";
	RQntNames[RQUANT_EPOLYS] = "Entity Polys";
	RQntNames[RQUANT_WPOLYS] = "World Polys";
	RQntNames[RQUANT_SHADOWFACES] = "Shadow Faces";
	RQntNames[RQUANT_SHADOWEDGES] = "Shadow edges";
	RQntNames[RQUANT_LITFACES] = "Lit faces";

	for (i = 0; i < RSPEED_MAX; i++)
	{
		s = va("%i %-40s", samplerspeeds[i], RSpNames[i]);
		Draw_String(vid.width-strlen(s)*8, i*8, s);
	}
	for (i = 0; i < RQUANT_MAX; i++)
	{
		s = va("%i %-40s", samplerquant[i], RQntNames[i]);
		Draw_String(vid.width-strlen(s)*8, (i+RSPEED_MAX)*8, s);
	}

	if (framecount++>=100)
	{
		for (i = 0; i < RSPEED_MAX; i++)
		{
			samplerspeeds[i] = rspeeds[i];
			rspeeds[i] = 0;
		}
		for (i = 0; i < RQUANT_MAX; i++)
		{
			samplerquant[i] = rquant[i];
			rquant[i] = 0;
		}
		framecount=0;
	}
}

void SCR_DrawTwoDimensional(int uimenu, qboolean nohud)
{
	RSpeedMark();
	//
	// draw any areas not covered by the refresh
	//
	if (!nohud)
		SCR_TileClear ();

	if (r_netgraph.value)
		GLR_NetGraph ();

	if (scr_drawdialog)
	{
#ifdef PLUGINS
		if (!nohud)
			Plug_SBar ();
#endif
		SCR_ShowPics_Draw();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
		scr_copyeverything = true;
	}
	else if (scr_drawloading)
	{
		SCR_DrawLoading ();
#ifdef PLUGINS
		Plug_SBar ();
#endif
		SCR_ShowPics_Draw();
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
	else if (cl.intermission == 3 && key_dest == key_game)
	{
	}
	else
	{
		if (!nohud)
		{
			Draw_Crosshair();

			SCR_DrawRam ();
			SCR_DrawNet ();
			SCR_DrawFPS ();
			SCR_DrawUPS ();
			SCR_DrawTurtle ();
			SCR_DrawPause ();
#ifdef PLUGINS
			Plug_SBar ();
#endif
			SCR_ShowPics_Draw();
		}
		else
			SCR_DrawFPS ();
		SCR_CheckDrawCenterString ();
	glTexEnvi ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
#ifdef TEXTEDITOR
		if (editoractive)
			Editor_Draw();
#endif
		M_Draw (uimenu);
		SCR_DrawConsole (false);
	}

	RSpeedEnd(RSPEED_2D);
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
	extern cvar_t gl_2dscale;
	static float old2dscale=1;
	int uimenu;
#ifdef TEXTEDITOR
	extern qboolean editormodal, editoractive;
#endif
	qboolean nohud;
	RSpeedMark();

	if (block_drawing)
	{
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}

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

//pretect against too small resolutions (possibly minimising task switches).
		if (vid.width<320)
		{
			vid.width=320;
			vid.conwidth=320;
		}
		if (vid.height<200)
		{
			vid.height=200;
			vid.conheight=200;
		}

		vid.recalc_refdef = true;
		Con_CheckResize();

#ifdef PLUGINS
		Plug_ResChanged();
#endif
		GL_Set2D();
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
		GL_DoSwap();
		RSpeedEnd(RSPEED_TOTALREFRESH);
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
		GL_DoSwap();
		RSpeedEnd(RSPEED_TOTALREFRESH);
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

	nohud = false;
#ifdef VM_CG
	if (CG_Refresh())
		nohud = true;
	else
#endif
		if (cl.worldmodel && uimenu != 1)
		V_RenderView ();
	else
		GL_DoSwap();

	GL_Set2D ();

	GLR_BrightenScreen();

	SCR_DrawTwoDimensional(uimenu, nohud);

	GLV_UpdatePalette ();
#if defined(_WIN32) && defined(RGLQUAKE)
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
	qbyte *ret = BZ_Malloc(prepadbytes + glwidth*glheight*3);

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, ret + prepadbytes); 

	*truewidth = glwidth;
	*trueheight = glheight;

	if (gammaworks)
	{
		c = prepadbytes+glwidth*glheight*3;
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
