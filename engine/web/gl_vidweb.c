#include "quakedef.h"
#include "glquake.h"
#include "web/ftejslib.h"
vfsfile_t *FSWEB_OpenTempHandle(int f);

extern cvar_t gl_lateswap;
extern qboolean gammaworks;

extern qboolean vid_isfullscreen;

qboolean mouseactive;
extern qboolean mouseusedforgui;

static int gamepaddeviceids[] = {DEVID_UNSET,DEVID_UNSET,DEVID_UNSET,DEVID_UNSET,DEVID_UNSET,DEVID_UNSET,DEVID_UNSET,DEVID_UNSET};
static int keyboardid[] = {0};
static int mouseid[] = {0};

static void *GLVID_getsdlglfunction(char *functionname)
{
	return NULL;
}

static void IN_GamePadButtonEvent(unsigned int joydevid, int button, int ispressed, int isstandardmapping)
{
	int standardmapping[] =
	{	//the order of these keys is different from that of xinput
		//however, the quake button codes should be the same. I really ought to define some K_ aliases for them.
		K_GP_A,
		K_GP_B,
		K_GP_X,
		K_GP_Y,
		K_GP_LEFT_SHOULDER,
		K_GP_RIGHT_SHOULDER,
		K_GP_LEFT_TRIGGER,
		K_GP_RIGHT_TRIGGER,
		K_GP_BACK,
		K_GP_START,
		K_GP_LEFT_THUMB,
		K_GP_RIGHT_THUMB,
		K_GP_DPAD_UP,
		K_GP_DPAD_DOWN,
		K_GP_DPAD_LEFT,
		K_GP_DPAD_RIGHT,
		K_GP_GUIDE,
		//K_GP_UNKNOWN
	};

	if (joydevid < countof(gamepaddeviceids))
	{
		if (joydevid == gamepaddeviceids[joydevid])
		{
			if (!ispressed)
				return;	//don't send axis events until its enabled.
			gamepaddeviceids[joydevid] = joydevid;
		}
		joydevid = gamepaddeviceids[joydevid];
	}

	if (isstandardmapping)
	{
		if (button < countof(standardmapping))
			IN_KeyEvent(joydevid, ispressed, standardmapping[button], 0);
	}
	else
	{
		if (button < 32+4)
			IN_KeyEvent(joydevid, ispressed, K_JOY1+button, 0);
	}
}

static void IN_GamePadAxisEvent(unsigned int joydevid, int axis, float value, int isstandardmapping)
{
	if (joydevid < countof(gamepaddeviceids))
	{
		joydevid = gamepaddeviceids[joydevid];
		if (joydevid == DEVID_UNSET)
			return;	//don't send axis events until its enabled.
	}
	if (isstandardmapping)
	{
		int axismap[] = {GPAXIS_LT_RIGHT,GPAXIS_LT_DOWN,GPAXIS_RT_RIGHT,GPAXIS_RT_DOWN};
		if (axis < countof(axismap))
			IN_JoystickAxisEvent(joydevid, axismap[axis], value);
	}
	else
		IN_JoystickAxisEvent(joydevid, axis, value);
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
	unsigned char tab[256] =
	{
		/*  0*/ 0,0,0,0,0,0,0,0,                K_BACKSPACE,K_TAB,0,0,0,K_ENTER,0,0,
		/* 16*/ K_SHIFT,K_CTRL,K_ALT,K_PAUSE,K_CAPSLOCK,0,0,0,0,0,0,K_ESCAPE,0,0,0,0,
		/* 32*/ ' ',K_PGUP,K_PGDN,K_END,K_HOME,K_LEFTARROW,K_UPARROW,K_RIGHTARROW,              K_DOWNARROW,0,0,0,K_PRINTSCREEN,K_INS,K_DEL,0,
		/* 48*/ '0','1','2','3','4','5','6','7',                '8','9',0,';',0,'=',0,0,

		/* 64*/ 0,'a','b','c','d','e','f','g',          'h','i','j','k','l','m','n','o',
		/* 80*/ 'p','q','r','s','t','u','v','w',                'x','y','z',K_LWIN,K_RWIN,K_APP,0,0,
		/* 96*/ K_KP_INS,K_KP_END,K_KP_DOWNARROW,K_KP_PGDN,K_KP_LEFTARROW,K_KP_5,K_KP_RIGHTARROW,K_KP_HOME,             K_KP_UPARROW,K_KP_PGDN,K_KP_STAR,K_KP_PLUS,0,K_KP_MINUS,K_KP_DEL,K_KP_SLASH,
		/*112*/ K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,K_F11,K_F12,0,0,0,0,
		/*128*/ 0,0,0,0,0,0,0,0,                0,0,0,0,0,0,0,0,
		/*144*/ K_KP_NUMLOCK,K_SCRLCK,0,0,0,0,0,0,              0,0,0,0,0,0,0,0,
		/*160*/ 0,0,0,'#',0,0,0,0,                0,0,0,0,0,'-',0,0,
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

//	Con_DPrintf("You just pressed dom key %u, which is quake key %u\n", code, tab[code]);
	return tab[code];
}
static unsigned int domkeytoshift(unsigned int code)
{
	unsigned char tab[256] =
	{
		/*  0*/ 0,0,0,0,0,0,0,0,                K_BACKSPACE,K_TAB,0,0,0,K_ENTER,0,0,
		/* 16*/ K_SHIFT,K_CTRL,K_ALT,K_PAUSE,K_CAPSLOCK,0,0,0,0,0,0,K_ESCAPE,0,0,0,0,
		/* 32*/ ' ',K_PGUP,K_PGDN,K_END,K_HOME,K_LEFTARROW,K_UPARROW,K_RIGHTARROW,              K_DOWNARROW,0,0,0,K_PRINTSCREEN,K_INS,K_DEL,0,
		/* 48*/ ')','!','\"',0/*£*/,'$','%','^','&',                '*','(',0,':',0,'+',0,0,

		/* 64*/ 0,'A','B','C','D','E','F','G',          'H','I','J','K','L','M','N','O',
		/* 80*/ 'P','Q','R','S','T','U','V','W',                'X','Y','Z',K_LWIN,K_RWIN,K_APP,0,0,
		/* 96*/ K_KP_INS,K_KP_END,K_KP_DOWNARROW,K_KP_PGDN,K_KP_LEFTARROW,K_KP_5,K_KP_RIGHTARROW,K_KP_HOME,             K_KP_UPARROW,K_KP_PGDN,K_KP_STAR,K_KP_PLUS,0,K_KP_MINUS,K_KP_DEL,K_KP_SLASH,
		/*112*/ K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,K_F11,K_F12,0,0,0,0,
		/*128*/ 0,0,0,0,0,0,0,0,                0,0,0,0,0,0,0,0,
		/*144*/ K_KP_NUMLOCK,K_SCRLCK,0,0,0,0,0,0,              0,0,0,0,0,0,0,0,
		/*160*/ 0,0,0,'~',0,0,0,0,                0,0,0,0,0,'_',0,0,
		/*176*/ 0,0,0,0,0,0,0,0,                0,0,':','+','<','_','>','?',
		/*192*/ '`',0,0,0,0,0,0,0,             0,0,0,0,0,0,0,0,
		/*208*/ 0,0,0,0,0,0,0,0,                0,0,0,'{','|','}','@','`',
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

//	Con_DPrintf("You just pressed dom key %u, which is quake key %u\n", code, tab[code]);
	return tab[code];
}
static int DOM_KeyEvent(unsigned int devid, int down, int scan, int uni)
{
	extern int		shift_down;
//	Con_Printf("Key %s %i %i:%c\n", down?"down":"up", scan, uni, uni?(char)uni:' ');
	if (shift_down)
	{
		uni = domkeytoshift(scan);
		scan = domkeytoquake(scan);
		uni = (uni >= 32 && uni <= 127)?uni:0;
	}
	else
	{
		scan = domkeytoquake(scan);
		uni = (scan >= 32 && scan <= 127)?scan:0;
	}
	IN_KeyEvent(keyboardid[devid], down, scan, uni);
	//Chars which don't map to some printable ascii value get preventDefaulted.
	//This is to stop fucking annoying fucking things like backspace randomly destroying the page and thus game.
	//And it has to be conditional, or we don't get any unicode chars at all.
	//The behaviour browsers seem to give is retardedly unhelpful, and just results in hacks to detect keys that appear to map to ascii...
	//Preventing the browser from leaving the page etc should NOT mean I can no longer get ascii/unicode values, only that the browser stops trying to do something random due to the event.
	//If you are the person that decreed that this is the holy way, then please castrate yourself now.
//	if (scan == K_BACKSPACE || scan == K_LCTRL || scan == K_LALT || scan == K_LSHIFT || scan == K_RCTRL || scan == K_RALT || scan == K_RSHIFT)
		return true;
//	return false;
}
static void DOM_ButtonEvent(unsigned int devid, int down, int button)
{
	if (down == 2)
	{
		//fixme: the event is a float. we ignore that.
		while(button < 0)
		{
			IN_KeyEvent(mouseid[devid], true, K_MWHEELUP, 0);
			button += 1;
		}
		while(button > 0)
		{
			IN_KeyEvent(mouseid[devid], true, K_MWHEELDOWN, 0);
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

		IN_KeyEvent(mouseid[devid], down, K_MOUSE1+button, 0);
	}
}
void DOM_MouseMove(unsigned int devid, int abs, float x, float y, float z, float size)
{
	IN_MouseMove(mouseid[devid], abs, x, y, z, size);
}

void DOM_LoadFile(char *loc, char *mime, int handle)
{
	vfsfile_t *file = NULL;
	if (handle != -1)
		file = FSWEB_OpenTempHandle(handle);
	else
	{
		char str[1024];
		if (!strcmp(mime, "joinurl") || !strcmp(mime, "observeurl")  || !strcmp(mime, "connecturl"))
		{
			extern cvar_t spectator;
			if (!strcmp(mime, "joinurl"))
				Cvar_Set(&spectator, "0");
			if (!strcmp(mime, "observeurl"))
				Cvar_Set(&spectator, "1");
			Cbuf_AddText(va("connect %s\n", COM_QuotedString(loc, str, sizeof(str), false)), RESTRICT_INSECURE);
			return;
		}
		if (!strcmp(mime, "demourl"))
		{
			Cbuf_AddText(va("qtvplay %s\n", COM_QuotedString(loc, str, sizeof(str), false)), RESTRICT_INSECURE);
			return;
		}
	}
	//try and open it. generally downloading it from the server.
	if (!Host_RunFile(loc, strlen(loc), file))
	{
		if (file)
			VFS_CLOSE(file);
	}
}
int VID_ShouldSwitchToFullscreen(void)
{	//if false, mouse grabs won't work and we'll be forced to touchscreen mode.
	//we can only go fullscreen when the user clicks something.
	//this means that the user will get pissed off at the fullscreen state changing when they first click on the menus after it loading up.
	//this is confounded by escape bringing up the menu. <ESC>GRR IT CHANGED MODE!<options>WTF IT CHANGED AGAIN FUCKING PIECE OF SHIT!.
	//annoying, but that's web browsers for you. the best thing we can do is to not regrab until they next click while actually back in the game.
	extern cvar_t vid_fullscreen;
	return !!vid_fullscreen.value && (!Key_Dest_Has(kdm_console | kdm_cwindows | kdm_emenu) || !Key_MouseShouldBeFree());
}
qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	vid_isfullscreen = true;

	if (!emscriptenfte_setupcanvas(
		info->width,
		info->height,
		VID_Resized,
		DOM_MouseMove,
		DOM_ButtonEvent,
		DOM_KeyEvent,
		DOM_LoadFile,
		IN_GamePadButtonEvent,
		IN_GamePadAxisEvent,
		VID_ShouldSwitchToFullscreen
		))
	{
		Con_Printf("Couldn't set up canvas\n");
		return false;
	}

	vid.activeapp = true;

	GL_Init(GLVID_getsdlglfunction);

	qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);

	VID_Resized(vid.pixelwidth, vid.pixelheight);

	mouseactive = false;

	return true;
}

void GLVID_DeInit (void)
{
	vid.activeapp = false;

	emscriptenfte_setupcanvas(-1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}


void GLVID_SwapBuffers (void)
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
			if ((key_dest == key_game||mouseusedforgui) && vid.activeapp)
				IN_ActivateMouse ();
			else if (!(key_dest == key_game || mouseusedforgui) || !vid.activeapp)
				IN_DeactivateMouse ();
		}
	}
*/
}

qboolean GLVID_ApplyGammaRamps (unsigned int gammarampsize, unsigned short *ramps)
{
	gammaworks = false;
	return gammaworks;
}

void GLVID_SetCaption(const char *text)
{
	emscriptenfte_settitle(text);
}

void Sys_SendKeyEvents(void)
{
	/*most callbacks happen outside our code, we don't need to poll for events - except for joysticks*/
	qboolean shouldbefree = Key_MouseShouldBeFree();
	emscriptenfte_updatepointerlock(_windowed_mouse.ival && !shouldbefree, shouldbefree);
	emscriptenfte_polljoyevents();
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
void INS_EnumerateDevices(void *ctx, void(*callback)(void *ctx, const char *type, const char *devicename, unsigned int *qdevid))
{
	size_t i;
	char foobar[64];
	for (i = 0; i < countof(gamepaddeviceids); i++)
	{
		Q_snprintfz(foobar, sizeof(foobar), "gp%i", i);
		callback(ctx, "gamepad", foobar, &gamepaddeviceids[i]);
	}
	for (i = 0; i < countof(mouseid); i++)
	{
		Q_snprintfz(foobar, sizeof(foobar), "m%i", i);
		callback(ctx, "mouse", foobar, &mouseid[i]);
	}
	for (i = 0; i < countof(keyboardid); i++)
	{
		Q_snprintfz(foobar, sizeof(foobar), "kb%i", i);
		callback(ctx, "keyboard", foobar, &keyboardid[i]);
	}
}

