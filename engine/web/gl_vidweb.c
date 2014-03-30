#include "quakedef.h"
#include "glquake.h"
#include "web/ftejslib.h"

extern cvar_t vid_hardwaregamma;
extern cvar_t gl_lateswap;
extern int gammaworks;

extern qboolean vid_isfullscreen;

qboolean ActiveApp;
qboolean mouseactive;
extern qboolean mouseusedforgui;


static void *GLVID_getsdlglfunction(char *functionname)
{
	return NULL;
}

static void VID_Resized(int width, int height)
{
	extern cvar_t vid_conautoscale, vid_conwidth;
	vid.pixelwidth = width;
	vid.pixelheight = height;
//Con_Printf("Resized: %i %i\n", vid.pixelwidth, vid.pixelheight);

	Cvar_ForceCallback(&vid_conautoscale);
	Cvar_ForceCallback(&vid_conwidth);
}
static unsigned int domkeytoquake(unsigned int code)
{
	unsigned int tab[256] =
	{
		/*  0*/ 0,0,0,0,0,0,0,0,                K_BACKSPACE,K_TAB,0,0,0,K_ENTER,0,0,
		/* 16*/ K_SHIFT,K_CTRL,K_ALT,K_PAUSE,K_CAPSLOCK,0,0,0,0,0,0,K_ESCAPE,0,0,0,0,
		/* 32*/ ' ',K_PGUP,K_PGDN,K_END,K_HOME,K_LEFTARROW,K_UPARROW,K_RIGHTARROW,              K_DOWNARROW,0,0,0,K_PRINTSCREEN,K_INS,K_DEL,0,
		/* 48*/ '0','1','2','3','4','5','6','7',                '8','9',0,0,0,0,0,0,

		/* 64*/ 0,'a','b','c','d','e','f','g',          'h','i','j','k','l','m','n','o',
		/* 80*/ 'p','q','r','s','t','u','v','w',                'x','y','z',K_LWIN,K_RWIN,K_APP,0,0,
		/* 96*/ K_KP_INS,K_KP_END,K_KP_DOWNARROW,K_KP_PGDN,K_KP_LEFTARROW,K_KP_5,K_KP_RIGHTARROW,K_KP_HOME,             K_KP_UPARROW,K_KP_PGDN,K_KP_STAR,K_KP_PLUS,0,K_KP_MINUS,K_KP_DEL,K_KP_SLASH,
		/*112*/ K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,K_F11,K_F12,0,0,0,0,
		/*128*/ 0,0,0,0,0,0,0,0,                0,0,0,0,0,0,0,0,
		/*144*/ K_KP_NUMLOCK,K_SCRLCK,0,0,0,0,0,0,              0,0,0,0,0,0,0,0,
		/*160*/ 0,0,0,'#',0,0,0,0,                0,0,0,0,0,0,0,0,
		/*176*/ 0,0,0,0,0,0,0,0,                0,0,';','=',',','-','.','/',
		/*192*/ '`',0,0,0,0,0,0,0,             0,0,0,0,0,0,0,0,
		/*208*/ 0,0,0,0,0,0,0,0,                0,0,0,'[','\\',']','\'','`',
		/*224*/ 0,0,0,0,0,0,0,0,                0,0,0,0,0,0,0,0,
		/*240*/ 0,0,0,0,0,0,0,0,                0,0,0,0,0,0,0,0,
	};
	if (!code)
		return 0;
	if (code >= sizeof(tab)/sizeof(tab[0]))
	{
		Con_DPrintf("You just pressed key %u, but I don't know what its meant to be\n", code);
		return 0;
	}
	if (!tab[code])
		Con_DPrintf("You just pressed key %u, but I don't know what its meant to be\n", code);

	Con_DPrintf("You just pressed dom key %u, which is quake key %u\n", code, tab[code]);
	return tab[code];
}
static int DOM_KeyEvent(int devid, int down, int scan, int uni)
{
	IN_KeyEvent(0, down, domkeytoquake(scan), uni);
	//Chars which don't map to some printable ascii value get preventDefaulted.
	//This is to stop fucking annoying fucking things like backspace randomly destroying the page and thus game.
	//And it has to be conditional, or we don't get any unicode chars at all.
	//The behaviour browsers seem to give is retardedly unhelpful, and just results in hacks to detect keys that appear to map to ascii...
	//Preventing the browser from leaving the page etc should NOT mean I can no longer get ascii/unicode values, only that the browser stops trying to do something random due to the event.
	//If you are the person that decreed that this is the holy way, then please castrate yourself now.
	if (scan < ' ' || scan >= 127)
		return true;
	return false;
}
static void DOM_ButtonEvent(int devid, int down, int button)
{
	if (down == 2)
	{
		//fixme: the event is a float. we ignore that.
		while(button < 0)
		{
			IN_KeyEvent(0, true, K_MWHEELUP, 0);
			button += 1;
		}
		while(button > 0)
		{
			IN_KeyEvent(0, true, K_MWHEELDOWN, 0);
			button -= 1;
		}
	}
	else
	{
		//swap buttons 2 and 3, so rmb is still +forward by default and not +mlook.
		if (button == 2)
			button = 1;
		else if (button == 1)
			button = 2;

		IN_KeyEvent(0, down, K_MOUSE1+button, 0);
	}
}

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	int flags;

	vid_isfullscreen = true;

	if (!emscriptenfte_setupcanvas(
		info->width,
		info->height,
		VID_Resized,
		IN_MouseMove,
		DOM_ButtonEvent,
		DOM_KeyEvent
		))
	{
		Con_Printf("Couldn't set up canvas\n");
		return false;
	}

	ActiveApp = true;

	GL_Init(GLVID_getsdlglfunction);

	qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);

	mouseactive = false;

	return true;
}

void GLVID_DeInit (void)
{
	ActiveApp = false;

	emscriptenfte_setupcanvas(-1, -1, NULL, NULL, NULL, NULL);
}


void VIDGL_SwapBuffers (void)
{
	//webgl doesn't support swapbuffers.
	//you can't use it for loading screens.
	//such things must result in waiting until the following frame.
	//although there IS a swapped-buffers event, which we should probably use in preference to requestanimationframe or whatever the call is.

/*
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
*/
}

qboolean GLVID_ApplyGammaRamps (unsigned short *ramps)
{
	gammaworks = false;
	return gammaworks;
}

void GLVID_SetCaption(char *text)
{
//	SDL_WM_SetCaption( text, NULL );
}

void Sys_SendKeyEvents(void)
{
	/*callbacks happen outside our code, we don't need to poll for events*/
}
/*various stuff for joysticks, which we don't support in this port*/
void INS_Shutdown (void)
{
}
void INS_ReInit (void)
{
}
void INS_Move(float *movements, int pnum)
{
}
void INS_Init (void)
{
}
void INS_Accumulate(void)
{
}
void INS_Commands (void)
{
}

