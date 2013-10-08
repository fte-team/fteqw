#include "quakedef.h"

#include <SDL.h>

SDL_Surface *sdlsurf;

qboolean ActiveApp;
qboolean mouseactive;
extern qboolean mouseusedforgui;
extern qboolean vid_isfullscreen;

#ifdef FTE_TARGET_WEB	//theoretically generic, but the IME is probably going to be more annoying on systems where its actually implemented properly.

#if SDL_MAJOR_VERSION > 1 || (SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION >= 3)
#define HAVE_SDL_TEXTINPUT
#endif
#endif

void IN_ActivateMouse(void)
{
	if (mouseactive)
		return;

	mouseactive = true;
	SDL_ShowCursor(0);
	SDL_WM_GrabInput(SDL_GRAB_ON);
}

void IN_DeactivateMouse(void)
{
	if (!mouseactive)
		return;

	mouseactive = false;
	SDL_ShowCursor(1);
	SDL_WM_GrabInput(SDL_GRAB_OFF);
}

#define tenoh	0,0,0,0,0, 0,0,0,0,0
#define fiftyoh tenoh, tenoh, tenoh, tenoh, tenoh
#define hundredoh fiftyoh, fiftyoh
static unsigned int tbl_sdltoquake[] =
{
	0,0,0,0,		//SDLK_UNKNOWN		= 0,
	0,0,0,0,		//SDLK_FIRST		= 0,
	K_BACKSPACE,	//SDLK_BACKSPACE	= 8,
	K_TAB,			//SDLK_TAB			= 9,
	0,0,
	0,				//SDLK_CLEAR		= 12,
	K_ENTER,		//SDLK_RETURN		= 13,
    0,0,0,0,0,
	K_PAUSE,		//SDLK_PAUSE		= 19,
	0,0,0,0,0,0,0,
	K_ESCAPE,		//SDLK_ESCAPE		= 27,
	0,0,0,0,
	K_SPACE,		//SDLK_SPACE		= 32,
	'!',			//SDLK_EXCLAIM		= 33,
	'"',			//SDLK_QUOTEDBL		= 34,
	'#',			//SDLK_HASH			= 35,
	'$',			//SDLK_DOLLAR		= 36,
	0,
	'&',			//SDLK_AMPERSAND	= 38,
	'\'',			//SDLK_QUOTE		= 39,
	'(',			//SDLK_LEFTPAREN	= 40,
	')',			//SDLK_RIGHTPAREN	= 41,
	'*',			//SDLK_ASTERISK		= 42,
	'+',			//SDLK_PLUS			= 43,
	',',			//SDLK_COMMA		= 44,
	'-',			//SDLK_MINUS		= 45,
	'.',			//SDLK_PERIOD		= 46,
	'/',			//SDLK_SLASH		= 47,
	'0',			//SDLK_0			= 48,
	'1',			//SDLK_1			= 49,
	'2',			//SDLK_2			= 50,
	'3',			//SDLK_3			= 51,
	'4',			//SDLK_4			= 52,
	'5',			//SDLK_5			= 53,
	'6',			//SDLK_6			= 54,
	'7',			//SDLK_7			= 55,
	'8',			//SDLK_8			= 56,
	'9',			//SDLK_9			= 57,
	':',			//SDLK_COLON		= 58,
	';',			//SDLK_SEMICOLON	= 59,
	'<',			//SDLK_LESS			= 60,
	'=',			//SDLK_EQUALS		= 61,
	'>',			//SDLK_GREATER		= 62,
	'?',			//SDLK_QUESTION		= 63,
	'@',			//SDLK_AT			= 64,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	'[',		//SDLK_LEFTBRACKET	= 91,
	'\\',		//SDLK_BACKSLASH	= 92,
	']',		//SDLK_RIGHTBRACKET	= 93,
	'^',		//SDLK_CARET		= 94,
	'_',		//SDLK_UNDERSCORE	= 95,
	'`',		//SDLK_BACKQUOTE	= 96,
	'a',		//SDLK_a			= 97,
	'b',		//SDLK_b			= 98,
	'c',		//SDLK_c			= 99,
	'd',		//SDLK_d			= 100,
	'e',		//SDLK_e			= 101,
	'f',		//SDLK_f			= 102,
	'g',		//SDLK_g			= 103,
	'h',		//SDLK_h			= 104,
	'i',		//SDLK_i			= 105,
	'j',		//SDLK_j			= 106,
	'k',		//SDLK_k			= 107,
	'l',		//SDLK_l			= 108,
	'm',		//SDLK_m			= 109,
	'n',		//SDLK_n			= 110,
	'o',		//SDLK_o			= 111,
	'p',		//SDLK_p			= 112,
	'q',		//SDLK_q			= 113,
	'r',		//SDLK_r			= 114,
	's',		//SDLK_s			= 115,
	't',		//SDLK_t			= 116,
	'u',		//SDLK_u			= 117,
	'v',		//SDLK_v			= 118,
	'w',		//SDLK_w			= 119,
	'x',		//SDLK_x			= 120,
	'y',		//SDLK_y			= 121,
	'z',		//SDLK_z			= 122,
	0,0,0,0,
	K_DEL, 		//SDLK_DELETE		= 127,
	hundredoh /*227*/, tenoh, tenoh, 0,0,0,0,0,0,0,0,
	K_KP_INS,		//SDLK_KP0		= 256,
	K_KP_END,		//SDLK_KP1		= 257,
	K_KP_DOWNARROW,		//SDLK_KP2		= 258,
	K_KP_PGDN,		//SDLK_KP3		= 259,
	K_KP_LEFTARROW,		//SDLK_KP4		= 260,
	K_KP_5,		//SDLK_KP5		= 261,
	K_KP_RIGHTARROW,		//SDLK_KP6		= 262,
	K_KP_HOME,		//SDLK_KP7		= 263,
	K_KP_UPARROW,		//SDLK_KP8		= 264,
	K_KP_PGUP,		//SDLK_KP9		= 265,
	K_KP_DEL,//SDLK_KP_PERIOD	= 266,
	K_KP_SLASH,//SDLK_KP_DIVIDE	= 267,
	K_KP_STAR,//SDLK_KP_MULTIPLY= 268,
	K_KP_MINUS,	//SDLK_KP_MINUS		= 269,
	K_KP_PLUS,	//SDLK_KP_PLUS		= 270,
	K_KP_ENTER,	//SDLK_KP_ENTER		= 271,
	K_KP_EQUALS,//SDLK_KP_EQUALS	= 272,
	K_UPARROW,	//SDLK_UP		= 273,
	K_DOWNARROW,//SDLK_DOWN		= 274,
	K_RIGHTARROW,//SDLK_RIGHT	= 275,
	K_LEFTARROW,//SDLK_LEFT		= 276,
	K_INS,		//SDLK_INSERT	= 277,
	K_HOME,		//SDLK_HOME		= 278,
	K_END,		//SDLK_END		= 279,
	K_PGUP, 	//SDLK_PAGEUP	= 280,
	K_PGDN,		//SDLK_PAGEDOWN	= 281,
	K_F1,		//SDLK_F1		= 282,
	K_F2,		//SDLK_F2		= 283,
	K_F3,		//SDLK_F3		= 284,
	K_F4,		//SDLK_F4		= 285,
	K_F5,		//SDLK_F5		= 286,
	K_F6,		//SDLK_F6		= 287,
	K_F7,		//SDLK_F7		= 288,
	K_F8,		//SDLK_F8		= 289,
	K_F9,		//SDLK_F9		= 290,
	K_F10,		//SDLK_F10		= 291,
	K_F11,		//SDLK_F11		= 292,
	K_F12,		//SDLK_F12		= 293,
	0,			//SDLK_F13		= 294,
	0,			//SDLK_F14		= 295,
	0,			//SDLK_F15		= 296,
	0,0,0,
	0,//K_NUMLOCK,	//SDLK_NUMLOCK	= 300,
	K_CAPSLOCK,	//SDLK_CAPSLOCK	= 301,
	0,//K_SCROLLOCK,//SDLK_SCROLLOCK= 302,
	K_SHIFT,	//SDLK_RSHIFT	= 303,
	K_SHIFT,	//SDLK_LSHIFT	= 304,
	K_CTRL,		//SDLK_RCTRL	= 305,
	K_CTRL,		//SDLK_LCTRL	= 306,
	K_RALT,		//SDLK_RALT		= 307,
	K_LALT,		//SDLK_LALT		= 308,
	0,			//SDLK_RMETA	= 309,
	0,			//SDLK_LMETA	= 310,
	0,			//SDLK_LSUPER	= 311,		/* Left "Windows" key */
	0,			//SDLK_RSUPER	= 312,		/* Right "Windows" key */
	0,			//SDLK_MODE		= 313,		/* "Alt Gr" key */
	0,			//SDLK_COMPOSE	= 314,		/* Multi-key compose key */
	0,			//SDLK_HELP		= 315,
	0,			//SDLK_PRINT	= 316,
	0,			//SDLK_SYSREQ	= 317,
	K_PAUSE,	//SDLK_BREAK	= 318,
	0,			//SDLK_MENU		= 319,
	0,			//SDLK_POWER	= 320,		/* Power Macintosh power key */
	'e',		//SDLK_EURO		= 321,		/* Some european keyboards */
	0			//SDLK_UNDO		= 322,		/* Atari keyboard has Undo */
};

static unsigned int tbl_sdltoquakemouse[] =
{
	K_MOUSE1,
	K_MOUSE3,
	K_MOUSE2,
	K_MWHEELUP,
	K_MWHEELDOWN,
	K_MOUSE4,
	K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,
	K_MOUSE9,
	K_MOUSE10
};

void Sys_SendKeyEvents(void)
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_ACTIVEEVENT:
			if (event.active.state & SDL_APPINPUTFOCUS)
			{	//follow keyboard status
				ActiveApp = !!event.active.gain;
				break;
			}
			break;

		case SDL_VIDEORESIZE:
#ifndef SERVERONLY
			vid.pixelwidth = event.resize.w;
			vid.pixelheight = event.resize.h;
			{
			extern cvar_t vid_conautoscale, vid_conwidth;	//make sure the screen is updated properly.
			Cvar_ForceCallback(&vid_conautoscale);
			Cvar_ForceCallback(&vid_conwidth);
			}
#endif
			break;

		case SDL_KEYUP:
		case SDL_KEYDOWN:
			{
				int u = event.key.keysym.unicode;
				int s = event.key.keysym.sym;
				int qs;
				if (s < sizeof(tbl_sdltoquake) / sizeof(tbl_sdltoquake[0]))
					qs = tbl_sdltoquake[s];
				else 
					qs = 0;

#ifdef FTE_TARGET_WEB
				if (s == 1249)
					qs = K_SHIFT;
#endif
#ifdef HAVE_SDL_TEXTINPUT
				IN_KeyEvent(0, event.key.state, qs, 0);
#else
				IN_KeyEvent(0, event.key.state, qs, u);
#endif
			}
			break;
#ifdef HAVE_SDL_TEXTINPUT
		case SDL_TEXTINPUT:
			{
				int i;
				unsigned int uc;
				int err;
				char *text = event.text.text;
				while(*text)
				{
					uc = utf8_decode(&err, text, &text);
					if (uc && !err)
					{
						IN_KeyEvent(0, true, 0, uc);
						IN_KeyEvent(0, false, 0, uc);
					}
				}
			}
			break;
#endif

		case SDL_MOUSEMOTION:
			if (!mouseactive)
				IN_MouseMove(0, true, event.motion.x, event.motion.y, 0, 0);
			else
				IN_MouseMove(0, false, event.motion.xrel, event.motion.yrel, 0, 0);
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			//Hmm. SDL allows for 255 buttons...
			if (event.button.button > sizeof(tbl_sdltoquakemouse)/sizeof(tbl_sdltoquakemouse[0]))
				event.button.button = sizeof(tbl_sdltoquakemouse)/sizeof(tbl_sdltoquakemouse[0]);
			IN_KeyEvent(0, event.button.state, tbl_sdltoquakemouse[event.button.button-1], 0);
			break;

		case SDL_QUIT:
			Cbuf_AddText("quit", RESTRICT_LOCAL);
			break;
		}
	}
}






void INS_Shutdown (void)
{
	IN_DeactivateMouse();
}

void INS_ReInit (void)
{
	IN_ActivateMouse();

#ifdef HAVE_SDL_TEXTINPUT
	SDL_StartTextInput();
#else
	SDL_EnableUNICODE(SDL_ENABLE);
#endif
}

//stubs, all the work is done in Sys_SendKeyEvents
void INS_Move(float *movements, int pnum)
{
}
void INS_Init (void)
{
}
void INS_Accumulate(void)	//input polling
{
}
void INS_Commands (void)	//used to Cbuf_AddText joystick button events in windows.
{
}



