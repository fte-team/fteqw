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
// screen.h


extern	float		scr_con_current;
extern	float		scr_conlines;		// lines of console to display

extern	int			scr_fullupdate;	// set to 0 to force full redraw
extern	int			sb_lines;

extern	int			clearnotify;	// set to 0 whenever notify text is drawn
extern	qboolean	scr_disabled_for_loading;

extern	cvar_t		scr_fov;
extern	cvar_t		scr_viewsize;

extern cvar_t scr_viewsize;

// only the refresh window will be updated unless these variables are flagged 
extern	int			scr_copytop;
extern	int			scr_copyeverything;

qboolean	scr_skipupdate;

qboolean SCR_RSShot (void);
qboolean	block_drawing;

//void SCR_DrawConsole (qboolean noback);
//void SCR_SetUpToDrawConsole (void);

//void SCR_BeginLoadingPlaque (void);
//void SCR_EndLoadingPlaque (void);


//void SCR_Init (void);

//void SCR_UpdateScreen (void);

#if defined(RGLQUAKE)
void GLSCR_UpdateScreen (void);
#endif

void SCR_ImageName (char *mapname);

#if defined(SWQUAKE)
void SWSCR_UpdateScreen (void);
void SCR_UpdateWholeScreen (void);
#endif


//this stuff is internal to the screen systems.
void RSpeedShow(void);

void SCR_CrosshairPosition(int pnum, int *x, int *y);
void SCR_DrawLoading (void);
void SCR_CalcRefdef (void);
void SCR_TileClear (void);
void SCR_DrawNotifyString (void);
void SCR_CheckDrawCenterString (void);
void SCR_DrawRam (void);
void SCR_DrawNet (void);
void SCR_DrawTurtle (void);
void SCR_DrawPause (void);
void SCR_VRectForPlayer(vrect_t *vrect, int pnum);	//returns a region for the player's view

void CLSCR_Init(void);	//basically so I can register a few friendly cvars.

//TEI_SHOWLMP2 stuff
void SCR_ShowPics_Draw(void);
void SCR_ShowPic_Create(void);
void SCR_ShowPic_Hide(void);
void SCR_ShowPic_Move(void);
void SCR_ShowPic_Update(void);
void SCR_ShowPic_Clear(void);

//a header is better than none...
void Draw_TextBox (int x, int y, int width, int lines);
qboolean SCR_ScreenShot (char *filename);

void SCR_DrawTwoDimensional(int uimenu, qboolean nohud);

enum
{
	LS_NONE,
	LS_CONNECTION,
	LS_SERVER,
	LS_CLIENT,
};
int SCR_GetLoadingStage(void);
void SCR_SetLoadingStage(int stage);
