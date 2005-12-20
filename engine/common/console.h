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
//
// console
//

#define MAXCONCOLOURS 8
typedef struct {
	float r, g, b;
	int rgbmask;
} consolecolours_t;
extern consolecolours_t consolecolours[MAXCONCOLOURS];

#define CON_STANDARDMASK	0x0080
#define CON_COLOURMASK		0x0700
#define CON_SPAREMASK3		0x0800	//something cool?
#define CON_SPAREMASK2		0x1000	//underline?
#define CON_SPAREMASK1		0x2000	//italics?
#define CON_BLINKTEXT		0x4000
#define CON_2NDCHARSETTEXT	0x8000

#define COLOR_BLACK		0
#define COLOR_RED		1
#define COLOR_GREEN		2
#define COLOR_YELLOW	3
#define COLOR_BLUE		4
#define COLOR_MAGENTA	5
#define COLOR_CYAN		6
#define COLOR_WHITE		7

#define M_COLOR_WHITE	((COLOR_WHITE)<<8)

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_MAGENTA	"^5"
#define S_COLOR_CYAN	"^6"
#define S_COLOR_WHITE	"^7"

#define		CON_TEXTSIZE	16384
typedef struct console_s
{
	char name[64];
	unsigned short	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line
	int		linewidth;
	int		totallines;
	int		vislines;
	qboolean unseentext;
	int		commandcompletion;	//allows tab completion of quake console commands
	void	(*linebuffered) (struct console_s *con, char *line);	//if present, called on enter, causes the standard console input to appear.
	void	(*redirect) (struct console_s *con, int key);	//if present, called every characture.
	void	*userdata;
	struct console_s *next;
} console_t;

extern	console_t	con_main;
extern	console_t	*con_current;			// point to either con_main or con_chat

extern	int			con_ormask;

extern int scr_chatmode;

//extern int con_totallines;
extern qboolean con_initialized;
extern qbyte *con_chars;
extern	int	con_notifylines;		// scan lines to clear for notify lines

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_DrawConsole (int lines, qboolean noback);
void Con_Print (char *txt);
void VARGS Con_Printf (const char *fmt, ...);
void VARGS Con_TPrintf (translation_t text, ...);
void VARGS Con_DPrintf (char *fmt, ...);
void VARGS Con_SafePrintf (char *fmt, ...);
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);

void Con_ExecuteLine(console_t *con, char *line);	//takes normal console commands


void Con_CycleConsole (void);
qboolean Con_IsActive (console_t *con);
void Con_Destroy (console_t *con);
console_t *Con_FindConsole(char *name);
console_t *Con_Create(char *name);
void Con_SetVisible (console_t *con);
void Con_PrintCon (console_t *con, char *txt);

void Con_NotifyBox (char *text);	// during startup for sound / cd warnings

#ifdef CRAZYDEBUGGING
#define TRACE(x) Con_Printf x
#else
#define TRACE(x)
#endif
