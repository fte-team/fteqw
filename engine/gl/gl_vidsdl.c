#include "quakedef.h"
#include "glquake.h"

#include <SDL.h>

SDL_Surface *sdlsurf;

extern cvar_t vid_hardwaregamma;
extern cvar_t gl_lateswap;
extern int gammaworks;

int glwidth;
int glheight;
float vid_gamma = 1.0;

#ifdef _WIN32	//half the rest of the code uses windows apis to focus windows. Should be fixed, but it's not too important.
HWND mainwindow;
#endif

extern qboolean vid_isfullscreen;

cvar_t	in_xflip = {"in_xflip", "0"};

unsigned short intitialgammaramps[3][256];

qboolean ActiveApp;
qboolean mouseactive;
extern qboolean mouseusedforgui;
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

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	int flags;

	Con_Printf("SDL GLVID_Init\n");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
	SDL_SetVideoMode( 0, 0, 0, 0 );	//to get around some SDL bugs

	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
Con_Printf("Getting gamma\n");
	SDL_GetGammaRamp(intitialgammaramps[0], intitialgammaramps[1], intitialgammaramps[2]);

	if (info->fullscreen)
	{
		flags = SDL_FULLSCREEN;
		vid_isfullscreen = true;
	}
	else
	{
		flags = SDL_RESIZABLE;
		vid_isfullscreen = false;
	}
	sdlsurf = SDL_SetVideoMode(glwidth=info->width, glheight=info->height, info->bpp, flags | SDL_OPENGL);
	if (!sdlsurf)
	{
		Con_Printf(stderr, "Couldn't set GL mode: %s\n", SDL_GetError());
		return false;
	}

	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &gl_canstencil);

	ActiveApp = true;

	GLVID_SetPalette (palette);
	GL_Init(SDL_GL_GetProcAddress);

	glViewport (0, 0, glwidth, glheight);

	mouseactive = false;
	if (vid_isfullscreen)
		IN_ActivateMouse();

	return true;
}

void GLVID_DeInit (void)
{
	ActiveApp = false;

	IN_DeactivateMouse();
	Con_Printf("Restoring gamma\n");
	SDL_SetGammaRamp (intitialgammaramps[0], intitialgammaramps[1], intitialgammaramps[2]);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = glwidth;//WindowRect.right - WindowRect.left;
	*height = glheight;//WindowRect.bottom - WindowRect.top;

//    if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport (*x, *y, *width, *height);
}

qboolean screenflush;
void GL_DoSwap (void)
{
	if (!screenflush)
		return;
	screenflush = 0;

	if (!scr_skipupdate || block_drawing)
		 SDL_GL_SwapBuffers( );


	if (!vid_isfullscreen)
	{
		if (!_windowed_mouse.value)
		{
			if (mouseactive)
			{
				IN_DeactivateMouse ();
			}
		}
		else
		{
			if ((key_dest == key_game||mouseusedforgui) && ActiveApp)
				IN_ActivateMouse ();
			else if (!(key_dest == key_game || mouseusedforgui) || !ActiveApp)
				IN_DeactivateMouse ();
		}
	}
}

void GL_EndRendering (void)
{
	screenflush = true;
	if (!gl_lateswap.value)
		GL_DoSwap();
}





void GLVID_HandlePause (qboolean pause)
{
}

void GLVID_LockBuffer (void)
{
}

void GLVID_UnlockBuffer (void)
{
}

int GLVID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void GLD_BeginDirectRect (int x, int y, qbyte *pbitmap, int width, int height)
{
}

void GLD_EndDirectRect (int x, int y, int width, int height)
{
}
void GLVID_ForceLockState (int lk)
{
}

void	GLVID_SetPalette (unsigned char *palette)
{
	qbyte	*pal;
	unsigned r,g,b;
	unsigned v;
	unsigned short i;
	unsigned	*table;
	extern qbyte gammatable[256];

	//
	// 8 8 8 encoding
	//
	if (vid_hardwaregamma.value)
	{
	//	don't built in the gamma table

		pal = palette;
		table = d_8to24rgbtable;
		for (i=0 ; i<256 ; i++)
		{
			r = pal[0];
			g = pal[1];
			b = pal[2];
			pal += 3;
			
	//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
	//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
			v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
			*table++ = v;
		}
		d_8to24rgbtable[255] &= 0xffffff;	// 255 is transparent
	}
	else
	{
//computer has no hardware gamma (poor suckers) increase table accordingly

		pal = palette;
		table = d_8to24rgbtable;
		for (i=0 ; i<256 ; i++)
		{
			r = gammatable[pal[0]];
			g = gammatable[pal[1]];
			b = gammatable[pal[2]];
			pal += 3;
			
	//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
	//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
			v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
			*table++ = v;
		}
		d_8to24rgbtable[255] &= 0xffffff;	// 255 is transparent
	}
}
void	GLVID_ShiftPalette (unsigned char *palette)
{
	extern	unsigned short ramps[3][256];
	
	if (vid_hardwaregamma.value)	//this is needed because ATI drivers don't work properly (or when task-switched out).
	{
		if (gammaworks)
		{	//we have hardware gamma applied - if we're doing a BF, we don't want to reset to the default gamma (yuck)
			SDL_SetGammaRamp (ramps[0], ramps[1], ramps[2]);
			return;
		}
		gammaworks = !SDL_SetGammaRamp (ramps[0], ramps[1], ramps[2]);
	}
	else

		gammaworks = false;
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
	K_ALT,		//SDLK_RALT		= 307,
	K_ALT,		//SDLK_LALT		= 308,
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

int mouse_x, mouse_y;

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
			glwidth = event.resize.w;
			glheight = event.resize.h;
			break;

		case SDL_KEYUP:
		case SDL_KEYDOWN:
			Key_Event(tbl_sdltoquake[event.key.keysym.sym], event.key.state);
			break;

		case SDL_MOUSEMOTION:
			mouse_x += event.motion.xrel;
			mouse_y += event.motion.yrel;
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			//Hmm. SDL allows for 255 buttons...
			if (event.button.button > 6)
				event.button.button = 6;
			Key_Event(K_MOUSE1+event.button.button-1, event.button.state);
			break;

		case SDL_QUIT:
			Cbuf_AddText("quit", RESTRICT_LOCAL);
			break;
		}
	}
}






void IN_Shutdown (void)
{
}
void IN_Init (void)
{
	Cvar_Register (&in_xflip, "input controls");
}
void IN_Move (usercmd_t *cmd, int pnum)	//add mouse movement to cmd
{
	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;


	if (!cl.paused && mouseactive)
	{
// add mouse X/Y movement to cmd
		if ( (in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1) ))
			cmd->sidemove += m_side.value * mouse_x;
		else
			cl.viewangles[pnum][YAW] -= m_yaw.value * mouse_x;

		if (in_mlook.state[pnum] & 1)
			V_StopPitchDrift (pnum);
		
		if ( (in_mlook.state[pnum] & 1) && !(in_strafe.state[pnum] & 1))
		{
			cl.viewangles[pnum][PITCH] += m_pitch.value * mouse_y;

			CL_ClampPitch(pnum);
		}
		else
		{
			if ((in_strafe.state[pnum] & 1) && noclip_anglehack)
				cmd->upmove -= m_forward.value * mouse_y;
			else
				cmd->forwardmove -= m_forward.value * mouse_y;
		}
	}

	mouse_x = 0;
	mouse_y = 0;
}
void IN_Accumulate(void)	//input polling
{
}
void IN_Commands (void)	//used to Cbuf_AddText joystick button events in windows.
{
}



