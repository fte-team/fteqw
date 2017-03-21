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

#define CON_LONGCHAR_MASK	(CON_LONGCHAR|CON_RICHFORECOLOUR)

#define CON_NONCLEARBG		0x00800000	//disabled if CON_RICHFORECOLOUR
#define CON_HALFALPHA		0x00400000	//disabled if CON_RICHFORECOLOUR
#define CON_LINKSPECIAL		0x00200000	//disabled if CON_RICHFORECOLOUR
#define CON_LONGCHAR		0x00100000	//flags (other than hidden) are found in the following conchar. disabled if CON_RICHFORECOLOUR
#define CON_HIDDEN			0x00080000
#define CON_BLINKTEXT		0x00040000
#define CON_2NDCHARSETTEXT	0x00020000
#define CON_RICHFORECOLOUR	0x00010000	//if set, the upper 3 nibbles are r4g4b4. background is clear, halfalpha is ignored.
//#define CON_HIGHCHARSMASK	0x00000080 // Quake's alternative mask

#define CON_FLAGSMASK		0xFFFF0000
#define CON_CHARMASK		0x0000FFFF

#define CON_FGMASK			0x0F000000
#define CON_BGMASK			0xF0000000
#define CON_FGSHIFT 24		//second highest nibble
#define CON_BGSHIFT 28		//high nibble
#define CON_RICHFOREMASK	0xFFF00000
#define CON_RICHBSHIFT 20	//
#define CON_RICHGSHIFT 24
#define CON_RICHRSHIFT 28	//high nibble

#define CON_WHITEMASK		0x0F000000 // must be constant. things assume this

#define CON_DEFAULTCHAR		(CON_WHITEMASK | 32)

#define CON_LINKSTART		(CON_LINKSPECIAL | CON_HIDDEN | '[')
#define CON_LINKEND			(CON_LINKSPECIAL | CON_HIDDEN | ']')

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

#define CON_DEFAULT "^&--"
#define CON_WARNING "^&E0"
#define CON_ERROR   "^&C0"
#define CON_NOTICE  "^&-1"

#define isextendedcode(x) ((x >= '0' && x <= '9') || (x >= 'A' && x <= 'F') || x == '-')
#define ishexcode(x) ((x >= '0' && x <= '9') || (x >= 'A' && x <= 'F') || (x >= 'a' && x <= 'f'))

#define CONL_CENTERED	(1u<<0)
#define CONL_NONOTIFY	(1u<<1)
#define CONL_BREAKPOINT	(1u<<2)	//red
#define CONL_EXECUTION	(1u<<3)	//yellow
typedef struct conline_s {
	struct conline_s *older;
	struct conline_s *newer;
	unsigned short length;
	unsigned short maxlength;
	unsigned char numlines;	//updated so we scroll properly
	unsigned char flags;	
	unsigned short id;
	float time;
} conline_t;

//majority of these are mututally exclusive. the bits allow multiple.
#define CB_NONE			0
#define CB_SCROLL		1
#define CB_COPY			2
#define CB_CLOSE		3
#define CB_MOVE			4
#define CB_ACTIONBAR	5
#define CB_SELECT		6
#define CB_SIZELEFT		(1u<<29)
#define CB_SIZERIGHT	(1u<<30)
#define CB_SIZEBOTTOM	(1u<<31)
#define CONF_HIDDEN			1	/*do not show in the console list (unless active)*/
#define CONF_NOTIFY			2	/*text printed to console also appears as notify lines*/
#define CONF_NOTIFY_BOTTOM	4	/*align the bottom*/
#define CONF_NOTIFY_RIGHT	8
//#define CONF_NOTIMES		16
#define CONF_KEYFOCUSED		32
#define CONF_ISWINDOW		64
#define CONF_NOWRAP			128
#define CONF_KEEPSELECTION	256
typedef struct console_s
{
	int id;
	int nextlineid;	//the current line being written to. so we can rewrite links etc.
	char name[128];
	char title[128];
	char prompt[128];
	char backimage[MAX_QPATH];
	shader_t *backshader;
	float wnd_x;
	float wnd_y;
	float wnd_w;
	float wnd_h;
	int linecount;
	unsigned int flags;
	float notif_x;
	float notif_y;
	float notif_w;
	int notif_l;
	float notif_fade;		// will be transparent for this long when fading
	float notif_t;
	int maxlines;
	conline_t *oldest;
	conline_t *current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		cr;
	conline_t *display;		// bottom of console displays this line
	int		displayoffset;	// horizontal offset
	int		vislines;		// pixel lines
	int		linesprinted;	// for notify times
	qboolean unseentext;
	unsigned parseflags;
	conchar_t defaultcharbits;
	int		commandcompletion;	//allows tab completion of quake console commands

	int				(*linebuffered) (struct console_s *con, char *line);	//if present, called on enter, causes the standard console input to appear. return 2 to not save the line in history.
	qboolean		(*redirect) (struct console_s *con, unsigned int unicode, int key);	//if present, called every character.
	qboolean		(*mouseover)(struct console_s *con, char **out_tiptext, shader_t **out_shader);
	qboolean		(*close) (struct console_s *con, qboolean force);
	void			*userdata;		//user context
	conline_t		*userline;	//editor cursor line
	unsigned int	useroffset;	//editor cursor offset

	conline_t		*highlightline;	//used for highlights (this line gets flagged)
	conline_t	*completionline;	//temp text at the bottom of the console
	conline_t	*footerline;	//temp text at the bottom of the console
	conline_t	*selstartline, *selendline;
	unsigned int	selstartoffset, selendoffset;
	int mousedown[2];	//x,y position that the current buttons were clicked.
	unsigned int buttonsdown;
	int mousecursor[2];	//x,y

	struct console_s *next;
} console_t;

extern	console_t	con_main;
extern	console_t	*con_curwindow;		// refers to a windowed console
extern	console_t	*con_current;		// point to either con_main or con_chat
extern	console_t	*con_mouseover;
extern	console_t	*con_chat;

//shared between console and keys.
//really the console input should be in console.c instead of keys.c I suppose.
#define		MAXCMDLINE	8192
#define		CON_EDIT_LINES_MASK ((1<<8)-1)
extern	unsigned char	*key_lines[CON_EDIT_LINES_MASK+1];
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
void Con_History_Save(void);
void Con_History_Load(void);
struct font_s;
void Con_DrawOneConsole(console_t *con, qboolean focused, struct font_s *font, float fx, float fy, float fsx, float fsy, float lineagelimit);
void Con_DrawConsole (int lines, qboolean noback);
char *Con_CopyConsole(console_t *con, qboolean nomarkup, qboolean onlyiflink);
void Con_Print (const char *txt);
void Con_CenterPrint(const char *txt);
void Con_PrintFlags(const char *text, unsigned int setflags, unsigned int clearflags);
void VARGS Con_Printf (const char *fmt, ...) LIKEPRINTF(1);
void VARGS Con_TPrintf (translation_t text, ...);
void VARGS Con_DPrintf (const char *fmt, ...) LIKEPRINTF(1);	//developer>=1, for stuff that's probably actually slightly useful
void VARGS Con_DLPrintf (int level, const char *fmt, ...) LIKEPRINTF(2);	//developer>=2, for spammy stuff
void VARGS Con_SafePrintf (const char *fmt, ...) LIKEPRINTF(1);
void Con_Footerf(console_t *con, qboolean append, const char *fmt, ...) LIKEPRINTF(3); 
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);//note: allows csqc to intercept the toggleconsole
void Con_ToggleConsole_Force(void);

int Con_ExecuteLine(console_t *con, char *line);	//takes normal console commands
int Con_Navigate(console_t *con, char *line);		//special webbrowser hacks


void Con_CycleConsole (void);
int Con_IsActive (console_t *con);
void Con_Destroy (console_t *con);
void Con_ClearCon(console_t *con);
void Con_SetActive (console_t *con);
qboolean Con_NameForNum(int num, char *buffer, int buffersize);
console_t *Con_FindConsole(const char *name);
console_t *Con_Create(const char *name, unsigned int flags);
void Con_PrintCon (console_t *con, const char *txt, unsigned int parseflags);
qboolean Con_InsertConChars (console_t *con, conline_t *line, int offset, conchar_t *c, int len);
conline_t *Con_ResizeLineBuffer(console_t *con, conline_t *old, unsigned int length);
void Con_FlushBackgrounds(void);

void Con_NotifyBox (char *text);	// during startup for sound / cd warnings

#ifdef CRAZYDEBUGGING
#define TRACE(x) Sys_Printf x
#else
#define TRACE(x)
#endif

