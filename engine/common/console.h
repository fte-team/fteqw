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

#define MAXCONCOLOURS 16
typedef struct {
	float fr, fg, fb;
} consolecolours_t;

extern consolecolours_t consolecolours[MAXCONCOLOURS];

#define MAXQ3COLOURS 10
extern conchar_t q3codemasks[MAXQ3COLOURS];

#define CON_NONCLEARBG		0x00800000
#define CON_BLINKTEXT		0x00400000
#define CON_2NDCHARSETTEXT	0x00200000
#define CON_HALFALPHA		0x00100000
#define CON_HIGHCHARSMASK	0x00000080 // Quake's alternative mask

#define CON_FLAGSMASK		0xFFF00000
#define CON_HIDDEN			0x000F0000
#define CON_CHARMASK		0x0000FFFF

#define CON_FGMASK			0x0F000000
#define CON_BGMASK			0xF0000000
#define CON_FGSHIFT 24
#define CON_BGSHIFT 28

#define CON_Q3MASK			0x0F100000
#define CON_WHITEMASK		0x0F000000 // must be constant. things assume this

#define CON_DEFAULTCHAR		(CON_WHITEMASK | 32)

#define CON_LINKSTART		(CON_HIDDEN | '[')
#define CON_LINKEND			(CON_HIDDEN | ']')

// RGBI standard colors
#define COLOR_BLACK			0
#define COLOR_DARKBLUE		1
#define COLOR_DARKGREEN		2
#define COLOR_DARKCYAN		3
#define COLOR_DARKRED		4
#define COLOR_DARKMAGENTA	5
#define COLOR_BROWN			6
#define COLOR_GREY			7
#define COLOR_DARKGREY		8
#define COLOR_BLUE			9
#define COLOR_GREEN			10
#define COLOR_CYAN			11
#define COLOR_RED			12
#define COLOR_MAGENTA		13
#define COLOR_YELLOW		14
#define COLOR_WHITE			15

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"

#define CON_WARNING "^&E0"
#define CON_ERROR   "^&C0"
#define CON_NOTICE  "^&-1"

#define		CON_TEXTSIZE	16384

#define isextendedcode(x) ((x >= '0' && x <= '9') || (x >= 'A' && x <= 'F') || x == '-')
#define ishexcode(x) ((x >= '0' && x <= '9') || (x >= 'A' && x <= 'F') || (x >= 'a' && x <= 'f'))

typedef struct conline_s {
	struct conline_s *older;
	struct conline_s *newer;
	unsigned short length;
	unsigned short lines;
	float time;
} conline_t;

#define CONF_HIDDEN			1
#define CONF_NOTIFY			2
#define CONF_NOTIFY_BOTTOM	4 /*align the bottom*/
#define CONF_NOTIMES		8
typedef struct console_s
{
	char name[64];
	int linecount;
	unsigned int flags;
	int notif_x;
	int notif_y;
	int notif_w;
	int notif_l;
	float notif_t;
	conline_t *oldest;
	conline_t *current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		cr;
	conline_t *display;		// bottom of console displays this line
	int		vislines;		// pixel lines
	int		linesprinted;	// for notify times
	qboolean unseentext;
	int		commandcompletion;	//allows tab completion of quake console commands
	void	(*linebuffered) (struct console_s *con, char *line);	//if present, called on enter, causes the standard console input to appear.
	void	(*redirect) (struct console_s *con, int key);	//if present, called every character.
	void	*userdata;
	struct console_s *next;
} console_t;

extern	console_t	con_main;
extern	console_t	*con_current;			// point to either con_main or con_chat
extern	console_t	*con_chat;

//shared between console and keys.
//really the console input should be in console.c instead of keys.c I suppose.
#define		MAXCMDLINE	256
#define		CON_EDIT_LINES_MASK ((1<<6)-1)
extern	unsigned char	key_lines[CON_EDIT_LINES_MASK+1][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;
extern	int		history_line;

extern int scr_chatmode;

//extern int con_totallines;
extern qboolean con_initialized;
extern qbyte *con_chars;

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_ForceActiveNow(void);
void Con_Init (void);
void Con_Shutdown (void);
void Con_History_Load(void);
void Con_DrawConsole (int lines, qboolean noback);
char *Con_CopyConsole(qboolean nomarkup);
void Con_Print (char *txt);
void VARGS Con_Printf (const char *fmt, ...) LIKEPRINTF(1);
void VARGS Con_TPrintf (translation_t text, ...);
void VARGS Con_DPrintf (char *fmt, ...) LIKEPRINTF(1);
void VARGS Con_SafePrintf (char *fmt, ...) LIKEPRINTF(1);
void Con_Footerf(qboolean append, char *fmt, ...) LIKEPRINTF(2); 
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);

void Con_ExecuteLine(console_t *con, char *line);	//takes normal console commands


void Con_CycleConsole (void);
int Con_IsActive (console_t *con);
void Con_Destroy (console_t *con);
void Con_SetActive (console_t *con);
qboolean Con_NameForNum(int num, char *buffer, int buffersize);
console_t *Con_FindConsole(char *name);
console_t *Con_Create(char *name, unsigned int flags);
void Con_SetVisible (console_t *con);
void Con_PrintCon (console_t *con, char *txt);

void Con_NotifyBox (char *text);	// during startup for sound / cd warnings

#ifdef CRAZYDEBUGGING
#define TRACE(x) Sys_Printf x
#else
#define TRACE(x)
#endif
