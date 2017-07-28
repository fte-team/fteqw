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
qboolean GLSCR_UpdateScreen (void);
#endif

void SCR_ImageName (const char *mapname);

//this stuff is internal to the screen systems.
void RSpeedShow(void);

void SCR_CrosshairPosition(playerview_t *pview, float *x, float *y);
void SCR_DrawLoading (qboolean opaque);
void SCR_TileClear (int skipbottom);
void SCR_DrawNotifyString (void);
void SCR_CheckDrawCenterString (void);
void SCR_DrawNet (void);
void SCR_DrawTurtle (void);
void SCR_DrawPause (void);
qboolean SCR_HardwareCursorIsActive(void);

void CLSCR_Init(void);	//basically so I can register a few friendly cvars.

//TEI_SHOWLMP2 stuff
void SCR_ShowPics_Draw(void);
void SCR_ShowPic_Create(void);
void SCR_ShowPic_Hide(void);
void SCR_ShowPic_Move(void);
void SCR_ShowPic_Update(void);
void SCR_ShowPic_ClearAll(qboolean persistflag);
char *SCR_ShowPics_ClickCommand(int cx, int cy);
void SCR_ShowPic_Script_f(void);
void SCR_ShowPic_Remove_f(void);

//a header is better than none...
void Draw_TextBox (int x, int y, int width, int lines);
enum fs_relative;


typedef enum uploadfmt
{
		TF_INVALID,
		TF_RGBA32,              /*rgba byte order*/
		TF_BGRA32,              /*bgra byte order*/
		TF_RGBX32,              /*rgb byte order, with extra wasted byte after blue*/
		TF_BGRX32,              /*rgb byte order, with extra wasted byte after blue*/
		TF_RGB24,               /*rgb byte order, no alpha channel nor pad, and regular top down*/
		TF_BGR24,               /*bgr byte order, no alpha channel nor pad, and regular top down*/
		TF_BGR24_FLIP,  /*bgr byte order, no alpha channel nor pad, and bottom up*/
		TF_LUM8,		/*8bit greyscale image*/
		TF_MIP4_LUM8,	/*8bit 4-mip greyscale image*/
		TF_MIP4_SOLID8,	/*8bit 4-mip image in default palette*/
		TF_MIP4_8PAL24,	/*8bit 4-mip image with included palette*/
		TF_MIP4_8PAL24_T255,/*8bit 4-mip image with included palette where index 255 is alpha 0*/
		TF_SOLID8,      /*8bit quake-palette image*/
		TF_TRANS8,      /*8bit quake-palette image, index 255=transparent*/
		TF_TRANS8_FULLBRIGHT,   /*fullbright 8 - fullbright texels have alpha 255, everything else 0*/
		TF_HEIGHT8,     /*image data is greyscale, convert to a normalmap and load that, uploaded alpha contains the original heights*/
		TF_HEIGHT8PAL, /*source data is palette values rather than actual heights, generate a fallback heightmap*/
		TF_H2_T7G1, /*8bit data, odd indexes give greyscale transparence*/
		TF_H2_TRANS8_0, /*8bit data, 0 is transparent, not 255*/
		TF_H2_T4A4,     /*8bit data, weird packing*/

		/*this block requires a palette*/
		TF_PALETTES,
		TF_8PAL24,
		TF_8PAL32,

		/*for render targets*/
		TF_DEPTH16,
		TF_DEPTH24,
		TF_DEPTH32,
		TF_RGBA16F,
		TF_RGBA32F,

		/*for weird systems where the gl driver needs to do the decode (read: webgl)*/
		TF_SYSTEMDECODE
} uploadfmt_t;

qboolean SCR_ScreenShot (char *filename, enum fs_relative fsroot, void **buffer, int numbuffers, int bytestride, int width, int height, enum uploadfmt fmt);

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
int Font_RegisterTrackerImage(const char *image);	//returns a unicode char value that can be used to embed the char within a line of text.
qboolean Font_TrackerValid(unsigned int imid);
struct font_s *Font_LoadFont(float height, const char *fontfilename);
void Font_Free(struct font_s *f);
void Font_BeginString(struct font_s *font, float vx, float vy, int *px, int *py);
void Font_BeginScaledString(struct font_s *font, float vx, float vy, float szx, float szy, float *px, float *py); /*avoid using*/
void Font_Transform(float vx, float vy, int *px, int *py);
int Font_CharHeight(void);
float Font_CharScaleHeight(void);
int Font_CharWidth(unsigned int charflags, unsigned int codepoint);
float Font_CharScaleWidth(unsigned int charflags, unsigned int codepoint);
int Font_CharEndCoord(struct font_s *font, int x, unsigned int charflags, unsigned int codepoint);
int Font_DrawChar(int px, int py, unsigned int charflags, unsigned int codepoint);
float Font_DrawScaleChar(float px, float py, unsigned int charflags, unsigned int codepoint); /*avoid using*/
void Font_EndString(struct font_s *font);
void Font_InvalidateColour(vec4_t newcolour);
/*these three functions deal with formatted blocks of text (including tabs and new lines)*/
fte_inline conchar_t *Font_Decode(conchar_t *start, unsigned int *codeflags, unsigned int *codepoint)
{
	if (*start & CON_LONGCHAR)
		if (!(*start & CON_RICHFORECOLOUR))
		{
			*codeflags = start[1];
			*codepoint = ((start[0] & CON_CHARMASK)<<16) | (start[1] & CON_CHARMASK);
			return start+2;
		}

	*codeflags = start[0] & CON_FLAGSMASK;
	*codepoint = start[0] & CON_CHARMASK;
	return start+1;
}
conchar_t *Font_DecodeReverse(conchar_t *start, conchar_t *stop, unsigned int *codeflags, unsigned int *codepoint);
int Font_LineBreaks(conchar_t *start, conchar_t *end, int maxpixelwidth, int maxlines, conchar_t **starts, conchar_t **ends);
int Font_LineWidth(conchar_t *start, conchar_t *end);
float Font_LineScaleWidth(conchar_t *start, conchar_t *end);
void Font_LineDraw(int x, int y, conchar_t *start, conchar_t *end);
conchar_t *Font_CharAt(int x, conchar_t *start, conchar_t *end);
extern struct font_s *font_default;
extern struct font_s *font_console;
extern struct font_s *font_tiny;
void PR_ReleaseFonts(unsigned int purgeowner);	//for menu/csqc
void PR_ReloadFonts(qboolean reload);
/*end fonts*/

void R_NetgraphInit(void);
void R_NetGraph (void);
void R_FrameTimeGraph (int frametime);
