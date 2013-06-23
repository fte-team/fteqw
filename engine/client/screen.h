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

typedef struct playerview_s playerview_t;

extern	float		scr_con_current;
extern	float		scr_conlines;		// lines of console to display

extern	int			sb_lines;

extern	int			clearnotify;	// set to 0 whenever notify text is drawn
extern	qboolean	scr_disabled_for_loading;

extern	cvar_t		scr_fov;
extern	cvar_t		scr_viewsize;

qboolean SCR_RSShot (void);

//void SCR_DrawConsole (qboolean noback);
//void SCR_SetUpToDrawConsole (void);

//void SCR_BeginLoadingPlaque (void);
//void SCR_EndLoadingPlaque (void);


//void SCR_Init (void);

//void SCR_UpdateScreen (void);

#if defined(GLQUAKE)
void GLSCR_UpdateScreen (void);
#endif

void SCR_ImageName (char *mapname);

//this stuff is internal to the screen systems.
void RSpeedShow(void);

void SCR_CrosshairPosition(playerview_t *pview, int *x, int *y);
void SCR_DrawLoading (void);
void SCR_CalcRefdef (void);
void SCR_TileClear (void);
void SCR_DrawNotifyString (void);
void SCR_CheckDrawCenterString (void);
void SCR_DrawNet (void);
void SCR_DrawTurtle (void);
void SCR_DrawPause (void);
void SCR_VRectForPlayer(vrect_t *vrect, int pnum);	//returns a region for the player's view
void SCR_DrawCursor(int prydoncursornum);

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
qboolean SCR_ScreenShot (char *filename, void *rgb_buffer, int width, int height);

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
void SCR_SetLoadingFile(char *str);

/*fonts*/
void Font_Init(void);
void Font_Shutdown(void);
struct font_s *Font_LoadFont(int height, char *fontfilename);
void Font_Free(struct font_s *f);
void Font_BeginString(struct font_s *font, int vx, int vy, int *px, int *py);
void Font_BeginScaledString(struct font_s *font, float vx, float vy, float szx, float szy, float *px, float *py); /*avoid using*/
void Font_Transform(int vx, int vy, int *px, int *py);
int Font_CharHeight(void);
float Font_CharScaleHeight(void);
int Font_CharWidth(unsigned int charcode);
float Font_CharScaleWidth(unsigned int charcode);
int Font_CharEndCoord(struct font_s *font, int x, unsigned int charcode);
int Font_DrawChar(int px, int py, unsigned int charcode);
float Font_DrawScaleChar(float px, float py, unsigned int charcode); /*avoid using*/
void Font_EndString(struct font_s *font);
void Font_ForceColour(float r, float g, float b, float a);	//This colour will be applied while the char mask remains WHITE. If you print char by char, make sure to include the mask.
void Font_InvalidateColour(void);
/*these three functions deal with formatted blocks of text (including tabs and new lines)*/
int Font_LineBreaks(conchar_t *start, conchar_t *end, int maxpixelwidth, int maxlines, conchar_t **starts, conchar_t **ends);
int Font_LineWidth(conchar_t *start, conchar_t *end);
float Font_LineScaleWidth(conchar_t *start, conchar_t *end);
void Font_LineDraw(int x, int y, conchar_t *start, conchar_t *end);
extern struct font_s *font_conchar;
extern struct font_s *font_tiny;
void PR_ResetFonts(qboolean purge);	//for menu/csqc
/*end fonts*/

void R_NetgraphInit(void);
void R_NetGraph (void);
void R_FrameTimeGraph (int frametime);
