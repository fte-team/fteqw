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
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/vt.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#include <dlfcn.h>

#include "quakedef.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "glquake.h"

#include <GL/glx.h>
#ifdef USE_EGL
#include "gl_videgl.h"
#endif

#include <X11/keysym.h>
#include <X11/cursorfont.h>

#ifdef USE_DGA
#include <X11/extensions/Xxf86dga.h>
#endif

#ifdef __linux__
	#define WITH_VMODE	//undefine this if the following include fails.
#endif
#ifdef WITH_VMODE
#include <X11/extensions/xf86vmode.h>
#endif


static Display *vid_dpy = NULL;
static Cursor vid_nullcursor;
static Window vid_window;
static Window vid_decoywindow;	//for legacy mode, this is a boring window that we can reparent into as needed
static Window vid_root;
static GLXContext ctx = NULL;
static int scrnum;
static enum
{
	PSL_NONE,
#ifdef USE_EGL
	PSL_EGL,
#endif
	PSL_GLX
} currentpsl;

extern cvar_t vid_conautoscale;

static float old_windowed_mouse = 0;

#ifdef USE_DGA
static int dgamouse = 0;
#endif

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask)

#define X_MASK (KEY_MASK | MOUSE_MASK | ResizeRequest | StructureNotifyMask | FocusChangeMask | VisibilityChangeMask)


#define FULLSCREEN_VMODE	1	//using xf86 vidmode
#define FULLSCREEN_VMODEACTIVE	2	//xf86 vidmode currently forced
#define FULLSCREEN_LEGACY	4	//override redirect used
#define FULLSCREEN_WM		8	//fullscreen hint used
#define FULLSCREEN_ACTIVE	16	//currently fullscreen
static int fullscreenflags;
static int fullscreenwidth;
static int fullscreenheight;

#ifdef WITH_VMODE
static int vidmode_ext = 0;
static XF86VidModeModeInfo **vidmodes;
static int num_vidmodes;
static int vidmode_usemode = -1;	//so that it can be reset if they switch away.

static unsigned short originalramps[3][256];
static qboolean originalapplied;	//states that the origionalramps arrays are valid, and contain stuff that we should revert to on close
#endif

void X_GoFullscreen(void);
void X_GoWindowed(void);
/*when alt-tabbing or whatever, the window manager creates a window, then destroys it again, resulting in weird focus events that trigger mode switches and grabs. using a timer reduces the weirdness and allows alt-tab to work properly. or at least better than it was working. that's the theory anyway*/
static unsigned int modeswitchtime;
static int modeswitchpending;

extern cvar_t	_windowed_mouse;

/*-----------------------------------------------------------------------*/

//qboolean is8bit = false;
//qboolean isPermedia = false;
qboolean ActiveApp = false;
static qboolean gracefulexit;

#define SYS_CLIPBOARD_SIZE 512
char clipboard_buffer[SYS_CLIPBOARD_SIZE];


/*-----------------------------------------------------------------------*/

static void *gllibrary;

XVisualInfo* (*qglXChooseVisual) (Display *dpy, int screen, int *attribList);
void (*qglXSwapBuffers) (Display *dpy, GLXDrawable drawable);
Bool (*qglXMakeCurrent) (Display *dpy, GLXDrawable drawable, GLXContext ctx);
GLXContext (*qglXCreateContext) (Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct);
void (*qglXDestroyContext) (Display *dpy, GLXContext ctx);
void *(*qglXGetProcAddress) (char *name);

void GLX_CloseLibrary(void)
{
	dlclose(gllibrary);
	gllibrary = NULL;
}

qboolean GLX_InitLibrary(char *driver)
{
	if (driver && *driver)
		gllibrary = dlopen(driver, RTLD_LAZY);
	else
		gllibrary = NULL;
	if (!gllibrary)	//I hate this.
		gllibrary = dlopen("libGL.so.1", RTLD_LAZY);
	if (!gllibrary)
		gllibrary = dlopen("libGL.so", RTLD_LAZY);
	if (!gllibrary)
		return false;

	qglXChooseVisual = dlsym(gllibrary, "glXChooseVisual");
	qglXSwapBuffers = dlsym(gllibrary, "glXSwapBuffers");
	qglXMakeCurrent = dlsym(gllibrary, "glXMakeCurrent");
	qglXCreateContext = dlsym(gllibrary, "glXCreateContext");
	qglXDestroyContext = dlsym(gllibrary, "glXDestroyContext");
	qglXGetProcAddress = dlsym(gllibrary, "glXGetProcAddress");
	if (!qglXGetProcAddress)
		qglXGetProcAddress = dlsym(gllibrary, "glXGetProcAddressARB");

	if (!qglXSwapBuffers && !qglXDestroyContext && !qglXCreateContext && !qglXMakeCurrent && !qglXChooseVisual)
		return false;

	return true;
}

void *GLX_GetSymbol(char *name)
{
	void *symb;

	if (qglXGetProcAddress)
		symb = qglXGetProcAddress(name);
	else
		symb = NULL;

	if (!symb)
		symb = dlsym(gllibrary, name);
	return symb;
}

static int XLateKey(XKeyEvent *ev, unsigned int *unicode)
{

	int key;
	char buf[64];
	KeySym keysym, shifted;

	key = 0;

	keysym = XLookupKeysym(ev, 0);
	XLookupString(ev, buf, sizeof buf, &shifted, 0);
	if (unicode)
		*unicode = buf[0];

	switch(keysym)
	{
		case XK_KP_Page_Up:
		case XK_Page_Up:	 key = K_PGUP; break;

		case XK_KP_Page_Down:
		case XK_Page_Down:	 key = K_PGDN; break;

		case XK_KP_Home:
		case XK_Home:	 key = K_HOME; break;

		case XK_KP_End:
		case XK_End:	 key = K_END; break;

		case XK_KP_Left:
		case XK_Left:	 key = K_LEFTARROW; break;

		case XK_KP_Right:
		case XK_Right:	key = K_RIGHTARROW;		break;

		case XK_KP_Down:
		case XK_Down:	 key = K_DOWNARROW; break;

		case XK_KP_Up:
		case XK_Up:		 key = K_UPARROW;	 break;

		case XK_Escape: key = K_ESCAPE;		break;

		case XK_KP_Enter:
		case XK_Return: key = K_ENTER;		 break;

		case XK_Tab:		key = K_TAB;			 break;

		case XK_F1:		 key = K_F1;				break;

		case XK_F2:		 key = K_F2;				break;

		case XK_F3:		 key = K_F3;				break;

		case XK_F4:		 key = K_F4;				break;

		case XK_F5:		 key = K_F5;				break;

		case XK_F6:		 key = K_F6;				break;

		case XK_F7:		 key = K_F7;				break;

		case XK_F8:		 key = K_F8;				break;

		case XK_F9:		 key = K_F9;				break;

		case XK_F10:		key = K_F10;			 break;

		case XK_F11:		key = K_F11;			 break;

		case XK_F12:		key = K_F12;			 break;

		case XK_BackSpace: key = K_BACKSPACE; break;

		case XK_KP_Delete:
		case XK_Delete: key = K_DEL; break;

		case XK_Pause:	key = K_PAUSE;		 break;

		case XK_Shift_L:
		case XK_Shift_R:	key = K_SHIFT;		break;

		case XK_Execute:
		case XK_Control_L:
		case XK_Control_R:	key = K_CTRL;		 break;

		case XK_Alt_L:
		case XK_Meta_L:
		case XK_Alt_R:
		case XK_Meta_R: key = K_ALT;			break;

		case XK_KP_Begin: key = '5';	break;

		case XK_KP_Insert:
		case XK_Insert:key = K_INS; break;

		case XK_KP_Multiply: key = '*'; break;
		case XK_KP_Add:  key = '+'; break;
		case XK_KP_Subtract: key = '-'; break;
		case XK_KP_Divide: key = '/'; break;

#if 0
		case 0x021: key = '1';break;/* [!] */
		case 0x040: key = '2';break;/* [@] */
		case 0x023: key = '3';break;/* [#] */
		case 0x024: key = '4';break;/* [$] */
		case 0x025: key = '5';break;/* [%] */
		case 0x05e: key = '6';break;/* [^] */
		case 0x026: key = '7';break;/* [&] */
		case 0x02a: key = '8';break;/* [*] */
		case 0x028: key = '9';;break;/* [(] */
		case 0x029: key = '0';break;/* [)] */
		case 0x05f: key = '-';break;/* [_] */
		case 0x02b: key = '=';break;/* [+] */
		case 0x07c: key = '\'';break;/* [|] */
		case 0x07d: key = '[';break;/* [}] */
		case 0x07b: key = ']';break;/* [{] */
		case 0x022: key = '\'';break;/* ["] */
		case 0x03a: key = ';';break;/* [:] */
		case 0x03f: key = '/';break;/* [?] */
		case 0x03e: key = '.';break;/* [>] */
		case 0x03c: key = ',';break;/* [<] */
#endif

		default:
			key = *(unsigned char*)buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';
			break;
	}

	return key;
}

static void install_grabs(void)
{
	XGrabPointer(vid_dpy, vid_window,
				 True,
				 0,
				 GrabModeAsync, GrabModeAsync,
				 vid_window,
				 None,
				 CurrentTime);

#ifdef USE_DGA
	// TODO: make this into a cvar, like "in_dgamouse", instead of parameters
	// TODO: inform the user when DGA is enabled
	if (!COM_CheckParm("-nodga") && !COM_CheckParm("-nomdga"))
	{
		XF86DGADirectVideo(vid_dpy, DefaultScreen(vid_dpy), XF86DGADirectMouse);
		dgamouse = 1;
	}
	else
#endif
	{
		XWarpPointer(vid_dpy, None, vid_window,
					 0, 0, 0, 0,
					 vid.width / 2, vid.height / 2);
	}

//	XSync(vid_dpy, True);
}

static void uninstall_grabs(void)
{
#ifdef USE_DGA
	if (dgamouse)
	{
		XF86DGADirectVideo(vid_dpy, DefaultScreen(vid_dpy), 0);
		dgamouse = 0;
	}
#endif

	if (vid_dpy)
		XUngrabPointer(vid_dpy, CurrentTime);

//	XSync(vid_dpy, True);
}

static void ClearAllStates (void)
{
	int		i;

// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (0, i, 0, false);
	}

	Key_ClearStates ();
//	IN_ClearStates ();
}

static void GetEvent(void)
{
	XEvent event, rep;
	int b;
	unsigned int uc;
	qboolean x11violations = true;
	Window mw;

	XNextEvent(vid_dpy, &event);

	switch (event.type) {
	case ResizeRequest:
		vid.pixelwidth = event.xresizerequest.width;
		vid.pixelheight = event.xresizerequest.height;
                Cvar_ForceCallback(&vid_conautoscale);
//		if (fullscreenflags & FULLSCREEN_ACTIVE)
//			XMoveWindow(vid_dpy, vid_window, 0, 0);
		break;
	case ConfigureNotify:
		if (event.xconfigurerequest.window == vid_window)
		{
			vid.pixelwidth = event.xconfigurerequest.width;
			vid.pixelheight = event.xconfigurerequest.height;
	                Cvar_ForceCallback(&vid_conautoscale);
		}
		else if (event.xconfigurerequest.window == vid_decoywindow)
		{
			if (!(fullscreenflags & FULLSCREEN_ACTIVE))
				XResizeWindow(vid_dpy, vid_window, event.xconfigurerequest.width, event.xconfigurerequest.height);
		}
//		if (fullscreenflags & FULLSCREEN_ACTIVE)
//			XMoveWindow(vid_dpy, vid_window, 0, 0);
		break;
	case KeyPress:
		b = XLateKey(&event.xkey, &uc);
		Key_Event(0, b, uc, true);
		break;
	case KeyRelease:
		b = XLateKey(&event.xkey, NULL);
		Key_Event(0, b, 0, false);
		break;

	case MotionNotify:
#ifdef USE_DGA
		if (dgamouse && old_windowed_mouse)
		{
			IN_MouseMove(0, false, event.xmotion.x_root, event.xmotion.y_root, 0, 0);
		}
		else
#endif
		{
			if (old_windowed_mouse)
			{
				int cx = vid.pixelwidth/2, cy=vid.pixelheight/2;
				IN_MouseMove(0, false, event.xmotion.x - cx, event.xmotion.y - cy, 0, 0);

				/* move the mouse to the window center again (disabling warp first so we don't see it*/
				XSelectInput(vid_dpy, vid_window, X_MASK & ~PointerMotionMask);
				XWarpPointer(vid_dpy, None, vid_window, 0, 0, 0, 0,
					cx, cy);
				XSelectInput(vid_dpy, vid_window, X_MASK);
			}
			else
			{
				IN_MouseMove(0, true, event.xmotion.x, event.xmotion.y, 0, 0);
			}
		}
		break;

	case ButtonPress:
		b=-1;
		if (event.xbutton.button == 1)
			b = K_MOUSE1;
		else if (event.xbutton.button == 2)
			b = K_MOUSE3;
		else if (event.xbutton.button == 3)
			b = K_MOUSE2;
		//note, the x11 protocol does not support a mousewheel
		//we only support it because we follow convention. the actual protocol specifies 4+5 as regular buttons
		else if (event.xbutton.button == 4)
			b = x11violations?K_MWHEELUP:K_MOUSE4;
		else if (event.xbutton.button == 5)
			b = x11violations?K_MWHEELDOWN:K_MOUSE5;
		//note, the x11 protocol does not support more than 5 mouse buttons
		//which is a bit of a shame, but hey.
		else if (event.xbutton.button == 6)
			b = x11violations?K_MOUSE4:-1;
		else if (event.xbutton.button == 7)
			b = x11violations?K_MOUSE5:-1;
		else if (event.xbutton.button == 8)
			b = x11violations?K_MOUSE6:-1;
		else if (event.xbutton.button == 9)
			b = x11violations?K_MOUSE7:-1;
		else if (event.xbutton.button == 10)
			b = x11violations?K_MOUSE8:-1;
		else if (event.xbutton.button == 11)
			b = x11violations?K_MOUSE9:-1;
		else if (event.xbutton.button == 12)
			b = x11violations?K_MOUSE10:-1;

		if (b>=0)
			IN_KeyEvent(0, true, b, 0);

/*
		if (fullscreenflags & FULLSCREEN_LEGACY)
		if (fullscreenflags & FULLSCREEN_VMODE)
		if (!ActiveApp)
		{	//KDE doesn't seem to like us, in that you can't alt-tab back or click to activate.
			//This allows us to steal input focus back from the window manager
			XSetInputFocus(vid_dpy, vid_window, RevertToParent, CurrentTime);
		}
*/
		break;

	case ButtonRelease:
		b=-1;
		if (event.xbutton.button == 1)
			b = K_MOUSE1;
		else if (event.xbutton.button == 2)
			b = K_MOUSE3;
		else if (event.xbutton.button == 3)
			b = K_MOUSE2;
		//note, the x11 protocol does not support a mousewheel
		//we only support it because we follow convention. the actual protocol specifies 4+5 as regular buttons
		else if (event.xbutton.button == 4)
			b = x11violations?K_MWHEELUP:K_MOUSE4;
		else if (event.xbutton.button == 5)
			b = x11violations?K_MWHEELDOWN:K_MOUSE5;
		//note, the x11 protocol does not support more than 5 mouse buttons
		//which is a bit of a shame, but hey.
		else if (event.xbutton.button == 6)
			b = x11violations?K_MOUSE4:-1;
		else if (event.xbutton.button == 7)
			b = x11violations?K_MOUSE5:-1;
		else if (event.xbutton.button == 8)
			b = x11violations?K_MOUSE6:-1;
		else if (event.xbutton.button == 9)
			b = x11violations?K_MOUSE7:-1;
		else if (event.xbutton.button == 10)
			b = x11violations?K_MOUSE8:-1;
		else if (event.xbutton.button == 11)
			b = x11violations?K_MOUSE9:-1;
		else if (event.xbutton.button == 12)
			b = x11violations?K_MOUSE10:-1;

		if (b>=0)
			IN_KeyEvent(0, false, b, 0);
		break;

	case FocusIn:
		//activeapp is if the game window is focused
		ActiveApp = true;

		//but change modes to track the desktop window
//		if (!(fullscreenflags & FULLSCREEN_ACTIVE) || event.xfocus.window != vid_decoywindow)
		{
			modeswitchpending = 1;
			modeswitchtime = Sys_Milliseconds() + 1500;	/*fairly slow, to make sure*/
		}

		//we we're focusing onto the game window and we're currently fullscreen, hide the other one so alt-tab won't select that instead of a real alternate app.
//		if ((fullscreenflags & FULLSCREEN_ACTIVE) && (fullscreenflags & FULLSCREEN_LEGACY) && event.xfocus.window == vid_window)
//			XUnmapWindow(vid_dpy, vid_decoywindow);
		break;
	case FocusOut:
		//if we're already active, the decoy window shouldn't be focused anyway.
		if ((fullscreenflags & FULLSCREEN_ACTIVE) && event.xfocus.window == vid_decoywindow)
		{
			break;
		}

#ifdef WITH_VMODE
		if (originalapplied)
			XF86VidModeSetGammaRamp(vid_dpy, scrnum, 256, originalramps[0], originalramps[1], originalramps[2]);

#endif
		mw = vid_window;
		if ((fullscreenflags & FULLSCREEN_LEGACY) && (fullscreenflags & FULLSCREEN_ACTIVE))
			mw = vid_decoywindow;

                if (event.xfocus.window == mw)
		{
			ActiveApp = false;
			if (old_windowed_mouse)
			{
				Con_DPrintf("uninstall grabs\n");
				uninstall_grabs();
				XUndefineCursor(vid_dpy, vid_window);
				old_windowed_mouse = false;
			}
			ClearAllStates();
		}
		modeswitchpending = -1;
		modeswitchtime = Sys_Milliseconds() + 100;	/*fairly fast, so we don't unapply stuff when switching to other progs with delays*/
		break;
	case ClientMessage:
		{
			char *name = XGetAtomName(vid_dpy, event.xclient.message_type);
			if (!strcmp(name, "WM_PROTOCOLS") && event.xclient.format == 32)
			{
				char *protname = XGetAtomName(vid_dpy, event.xclient.data.l[0]);
				if (!strcmp(protname, "WM_DELETE_WINDOW"))
				{
					Cmd_ExecuteString("menu_quit prompt", RESTRICT_LOCAL);
					XSetInputFocus(vid_dpy, vid_window, RevertToParent, CurrentTime);
				}
				else
					Con_Printf("Got message %s\n", protname);
				XFree(protname);
			}
			else
				Con_Printf("Got message %s\n", name);
			XFree(name);
		}
		break;

#if 1
	case SelectionRequest:	//needed for copy-to-clipboard
		{
			Atom xa_string = XInternAtom(vid_dpy, "UTF8_STRING", false);
			memset(&rep, 0, sizeof(rep));
			if (event.xselectionrequest.property == None)
				event.xselectionrequest.property = XInternAtom(vid_dpy, "foobar2000", false);
			if (event.xselectionrequest.property != None && event.xselectionrequest.target == xa_string)
			{
				XChangeProperty(vid_dpy, event.xselectionrequest.requestor, event.xselectionrequest.property, event.xselectionrequest.target, 8, PropModeReplace, (void*)clipboard_buffer, strlen(clipboard_buffer));
				rep.xselection.property = event.xselectionrequest.property;
			}
			else
			{
				rep.xselection.property = None;
			}
			rep.xselection.type = SelectionNotify;
			rep.xselection.serial = 0;
			rep.xselection.send_event = true;
			rep.xselection.display = rep.xselection.display;
			rep.xselection.requestor = event.xselectionrequest.requestor;
			rep.xselection.selection = event.xselectionrequest.selection;
			rep.xselection.target = event.xselectionrequest.target;
			rep.xselection.time = event.xselectionrequest.time;
			XSendEvent(vid_dpy, event.xselectionrequest.requestor, 0, 0, &rep);
		}
		break;
#endif

	default:
//		Con_Printf("%x\n", event.type);
		break;
	}
}


void GLVID_Shutdown(void)
{
	printf("GLVID_Shutdown\n");
	if (!vid_dpy)
		return;

	XUngrabKeyboard(vid_dpy, CurrentTime);
	if (old_windowed_mouse)
		uninstall_grabs();

#ifdef WITH_VMODE
	if (originalapplied)
		XF86VidModeSetGammaRamp(vid_dpy, scrnum, 256, originalramps[0], originalramps[1], originalramps[2]);
#endif


	switch(currentpsl)
	{
#ifdef USE_EGL
	case PSL_EGL:
		EGL_Shutdown();
		break;
#endif
	case PSL_GLX:
		if (ctx)
		{
			qglXDestroyContext(vid_dpy, ctx);
			ctx = NULL;
		}
		break;
	case PSL_NONE:
		break;
	}

	if (vid_window)
		XDestroyWindow(vid_dpy, vid_window);
	if (vid_nullcursor)
		XFreeCursor(vid_dpy, vid_nullcursor);
#ifdef WITH_VMODE
	if (vid_dpy)
	{
		if (fullscreenflags & FULLSCREEN_VMODEACTIVE)
			XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[0]);
		fullscreenflags &= ~FULLSCREEN_VMODEACTIVE;

		if (vidmodes)
			XFree(vidmodes);
		vidmodes = NULL;
		num_vidmodes = 0;
	}
#endif
	XCloseDisplay(vid_dpy);
	vid_dpy = NULL;
	vid_window = (Window)NULL;
	currentpsl = PSL_NONE;
}

void GLVID_DeInit(void)	//FIXME:....
{
	GLVID_Shutdown();
}


void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	Sys_Quit();
	exit(0);
}
void signal_handler_graceful(int sig)
{
	gracefulexit = true;
//	signal(sig, signal_handler);
}

void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler_graceful);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
#ifndef __CYGWIN__
	signal(SIGIOT, signal_handler);
#endif
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

static Cursor CreateNullCursor(Display *display, Window root)
{
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
          &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
}

void	GLVID_ShiftPalette (unsigned char *palette)
{
#ifdef WITH_VMODE
	extern qboolean gammaworks;
	extern cvar_t vid_hardwaregamma;
	extern	unsigned short ramps[3][256];

//	VID_SetPalette (palette);

	if (originalapplied && ActiveApp && vid_hardwaregamma.value)	//this is needed because ATI drivers don't work properly (or when task-switched out).
	{
		if (gammaworks)
		{	//we have hardware gamma applied - if we're doing a BF, we don't want to reset to the default gamma (yuck)
			XF86VidModeSetGammaRamp (vid_dpy, scrnum, 256, ramps[0], ramps[1], ramps[2]);
			return;
		}
		gammaworks = !!XF86VidModeSetGammaRamp (vid_dpy, scrnum, 256, ramps[0], ramps[1], ramps[2]);
	}
	else
		gammaworks = false;
#endif
}

void	GLVID_SetPalette (unsigned char *palette)
{
	qbyte	*pal;
	unsigned r,g,b;
	unsigned short i;
	unsigned	*table;
	extern qbyte gammatable[256];

//
// 8 8 8 encoding
//
	Con_DPrintf("Converting 8to24\n");

	pal = palette;
	table = d_8to24rgbtable;
	for (i=0 ; i<256 ; i++)
	{
		r = gammatable[pal[0]];
		g = gammatable[pal[1]];
		b = gammatable[pal[2]];
		pal += 3;

		*table++ = BigLong((r<<24)|(g<<16)|(b<<8)|255);
	}

	d_8to24rgbtable[255] &= BigLong(0xffffff00); // 255 is transparent
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (void)
{
	switch(currentpsl)
	{
#ifdef USE_EGL
	case PSL_EGL:
		EGL_BeginRendering();
		break;
#endif
	case PSL_GLX:
	case PSL_NONE:
		break;
	}
}


void GL_EndRendering (void)
{
	switch(currentpsl)
	{
#ifdef USE_EGL
	case PSL_EGL:
		EGL_EndRendering();
		break;
#endif
	case PSL_GLX:
		//we don't need to flush, XSawpBuffers does it for us.
		//chances are, it's version is more suitable anyway. At least there's the chance that it might be.
		qglXSwapBuffers(vid_dpy, vid_window);
		break;
	case PSL_NONE:
		break;
	}
}

#include "bymorphed.h"
void X_StoreIcon(Window wnd)
{
	int i;
	unsigned long data[64*64+2];
	unsigned int *indata = (unsigned int*)icon.pixel_data;
	Atom propname = XInternAtom(vid_dpy, "_NET_WM_ICON", false);
	Atom proptype = XInternAtom(vid_dpy, "CARDINAL", false);

	data[0] = icon.width;
	data[1] = icon.height;
	for (i = 0; i < data[0]*data[1]; i++)
		data[i+2] = indata[i];

	XChangeProperty(vid_dpy, wnd, propname, proptype, 32, PropModeReplace, (void*)data, data[0]*data[1]+2);
}

void X_GoFullscreen(void)
{
	XEvent xev;
	
	//for NETWM window managers
	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = vid_window;
	xev.xclient.message_type = XInternAtom(vid_dpy, "_NET_WM_STATE", False);
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;	//add
	xev.xclient.data.l[1] = XInternAtom(vid_dpy, "_NET_WM_STATE_FULLSCREEN", False);
	xev.xclient.data.l[2] = 0;
	XSync(vid_dpy, False);
	XSendEvent(vid_dpy, DefaultRootWindow(vid_dpy), False, SubstructureNotifyMask, &xev);
	XSync(vid_dpy, False);

	//for any other window managers, and broken NETWM
	XMoveResizeWindow(vid_dpy, vid_window, 0, 0, fullscreenwidth, fullscreenheight);
	XSync(vid_dpy, False);
}
void X_GoWindowed(void)
{
	XEvent xev;
	XFlush(vid_dpy);
	XSync(vid_dpy, False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = vid_window;
	xev.xclient.message_type = XInternAtom(vid_dpy, "_NET_WM_STATE", False);
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 0;	//remove
	xev.xclient.data.l[1] = XInternAtom(vid_dpy, "_NET_WM_STATE_FULLSCREEN", False);
	xev.xclient.data.l[2] = 0;
	XSendEvent(vid_dpy, DefaultRootWindow(vid_dpy), False, SubstructureNotifyMask, &xev);
	XSync(vid_dpy, False);

	//XMoveResizeWindow(vid_dpy, vid_window, 0, 0, 640, 480);
}
qboolean X_CheckWMFullscreenAvailable(void)
{
	//root window must have _NET_SUPPORTING_WM_CHECK which is a Window created by the WM
	//the WM's window must have _NET_WM_NAME set, which is the name of the window manager
	//if we can find those, then the window manager has not crashed.
	//if we can then find _NET_WM_STATE_FULLSCREEN in the _NET_SUPPORTED atom list on the root, then we can get fullscreen mode from the WM
	//and we'll have no alt-tab issues whatsoever.

	Atom xa_net_supporting_wm_check = XInternAtom(vid_dpy, "_NET_SUPPORTING_WM_CHECK", False);
	Atom xa_net_wm_name = XInternAtom(vid_dpy, "_NET_WM_NAME", False);
	Atom xa_net_supported = XInternAtom(vid_dpy, "_NET_SUPPORTED", False);
	Atom xa_net_wm_state_fullscreen = XInternAtom(vid_dpy, "_NET_WM_STATE_FULLSCREEN", False);
	Window wmwindow;
	unsigned char *prop;
	unsigned long bytes_after, nitems;
	Atom type;
	int format;
	qboolean success = false;
	unsigned char *wmname;
	int i;
	

	if (XGetWindowProperty(vid_dpy, vid_root, xa_net_supporting_wm_check, 0, 16384, False, AnyPropertyType, &type, &format, &nitems, &bytes_after, &prop) != Success || prop == NULL)
	{
		Con_Printf("Window manager not identified\n");
		return success;
	}
	wmwindow = *(Window *)prop;
	XFree(prop);
	
	if (XGetWindowProperty(vid_dpy, wmwindow, xa_net_wm_name, 0, 16384, False, AnyPropertyType, &type, &format, &nitems, &bytes_after, &wmname) != Success || wmname == NULL)
	{
		Con_Printf("Window manager crashed or something\n");
		return success;
	}
	else
	{
		if (XGetWindowProperty(vid_dpy, vid_root, xa_net_supported, 0, 16384, False, AnyPropertyType, &type, &format, &nitems, &bytes_after, &prop) != Success || prop == NULL)
		{
			Con_Printf("Window manager \"%s\" support nothing\n", wmname);
		}
		else
		{
			for (i = 0; i < nitems; i++)
			{
//				Con_Printf("supported: %s\n", XGetAtomName(vid_dpy, ((Atom*)prop)[i]));
				if (((Atom*)prop)[i] == xa_net_wm_state_fullscreen)
				{
					success = true;
					break;
				}
			}
			if (!success)
				Con_Printf("Window manager \"%s\" does not appear to support fullscreen\n", wmname);
			else
				Con_Printf("Window manager \"%s\" supports fullscreen\n", wmname);
			XFree(prop);
		}
		XFree(wmname);
	}
	return success;
}

Window X_CreateWindow(qboolean override, XVisualInfo *visinfo, unsigned int width, unsigned int height)
{
	Window wnd;
	XSetWindowAttributes attr;
	XSizeHints szhints;
	unsigned int mask;
	Atom prots[1];

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(vid_dpy, vid_root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;
	attr.backing_store = NotUseful;
	attr.save_under = False;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWBackingStore |CWSaveUnder;

	// override redirect prevents the windowmanager from finding out about us, and thus will not apply borders to our window.
	if (override)
	{
		mask |= CWOverrideRedirect;
		attr.override_redirect = True;
	}

	memset(&szhints, 0, sizeof(szhints));
	szhints.flags = PMinSize;
	szhints.min_width = 320;
	szhints.min_height = 200;
	szhints.x = 0;
	szhints.y = 0;
	szhints.width = width;
	szhints.height = height;

	wnd = XCreateWindow(vid_dpy, vid_root, 0, 0, width, height,
						0, visinfo->depth, InputOutput,
						visinfo->visual, mask, &attr);
	/*ask the window manager to stop triggering bugs in Xlib*/
	prots[0] = XInternAtom(vid_dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(vid_dpy, wnd, prots, sizeof(prots)/sizeof(prots[0]));
	XSetWMNormalHints(vid_dpy, wnd, &szhints);
	/*set caption*/
	XStoreName(vid_dpy, wnd, "FTE QuakeWorld");
	XSetIconName(vid_dpy, wnd, "FTEQW");
	X_StoreIcon(wnd);
	/*make it visible*/
	XMapWindow(vid_dpy, wnd);

	return wnd;
}

qboolean X11VID_Init (rendererstate_t *info, unsigned char *palette, int psl)
{
	int width = info->width;	//can override these if vmode isn't available
	int height = info->height;
	int i;
	int attrib[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		GLX_STENCIL_SIZE, 8,
		None
	};
#ifdef USE_EGL
	XVisualInfo vinfodef;
#endif
	XVisualInfo *visinfo;
	qboolean fullscreen = false;

#ifdef WITH_VMODE
	int MajorVersion, MinorVersion;

	if (info->fullscreen)
		fullscreen = true;
#endif


	S_Startup();

	currentpsl = psl;

	switch(currentpsl)
	{
#ifdef USE_EGL
	case PSL_EGL:
		if (!EGL_LoadLibrary(info->glrenderer))
		{
			Con_Printf("couldn't load EGL library\n");
			return false;
		}
		break;
#endif
	case PSL_GLX:
		if (!GLX_InitLibrary(info->glrenderer))
		{
			Con_Printf("Couldn't intialise GLX\nEither your drivers are not installed or you need to specify the library name with the gl_driver cvar\n");
			return false;
		}
		break;
	case PSL_NONE:
		return false;
	}

	if (!vid_dpy)
		vid_dpy = XOpenDisplay(NULL);
	if (!vid_dpy)
	{
		Con_Printf(CON_ERROR "Error: couldn't open the X display\n");
		return false;
	}

	scrnum = DefaultScreen(vid_dpy);
	vid_root = RootWindow(vid_dpy, scrnum);

#ifdef WITH_VMODE	//find out if it's supported on this pc.
	MajorVersion = MinorVersion = 0;
	if (COM_CheckParm("-novmode") || !XF86VidModeQueryVersion(vid_dpy, &MajorVersion, &MinorVersion))
	{
		vidmode_ext = 0;
	}
	else
	{
		Con_Printf("Using XF86-VidModeExtension Ver. %d.%d\n", MajorVersion, MinorVersion);
		vidmode_ext = MajorVersion;
	}
#endif

	fullscreenflags = 0;

#ifdef WITH_VMODE
	vidmode_usemode = -1;
	if (vidmode_ext)
	{
		int best_fit, best_dist, dist, x, y;

		XF86VidModeGetAllModeLines(vid_dpy, scrnum, &num_vidmodes, &vidmodes);
		// Are we going fullscreen?  If so, let's change video mode
		if (fullscreen)
		{
			best_dist = 9999999;
			best_fit = -1;

			for (i = 0; i < num_vidmodes; i++)
			{
				if (width > vidmodes[i]->hdisplay ||
					height > vidmodes[i]->vdisplay)
					continue;

				x = width - vidmodes[i]->hdisplay;
				y = height - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (dist < best_dist)
				{
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1)
			{
				// change to the mode
				if (XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[vidmode_usemode=best_fit]))
				{
					width = vidmodes[best_fit]->hdisplay;
					height = vidmodes[best_fit]->vdisplay;
					// Move the viewport to top left
					XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);
					XSync(vid_dpy, False);

					fullscreenflags |= FULLSCREEN_VMODE | FULLSCREEN_VMODEACTIVE;
				}
				else
					Con_Printf("Failed to apply mode %i*%i\n", vidmodes[best_fit]->hdisplay, vidmodes[best_fit]->vdisplay);
			}
		}
	}
#endif

	if (fullscreen)
	{
		if (!(fullscreenflags & FULLSCREEN_VMODE))
		{
			//if we can't actually change the mode, our fullscreen is the size of the root window
			XWindowAttributes xwa;
			XGetWindowAttributes(vid_dpy, DefaultRootWindow(vid_dpy), &xwa);
			width = xwa.width;
			height = xwa.height;
		}

		if (X_CheckWMFullscreenAvailable())
			fullscreenflags |= FULLSCREEN_WM;
		else
                        fullscreenflags |= FULLSCREEN_LEGACY;
	}

	switch(currentpsl)
	{
#ifdef USE_EGL
	case PSL_EGL:
		visinfo = &vinfodef;
		if (!XMatchVisualInfo(vid_dpy, scrnum, info->bpp, TrueColor, visinfo))
	//	if (!XMatchVisualInfo(vid_dpy, scrnum, DefaultDepth(vid_dpy, scrnum), TrueColor, &visinfo))
		{
			Sys_Error("Couldn't choose visual for EGL\n");
		}
		break;
#endif
	case PSL_GLX:
		visinfo = qglXChooseVisual(vid_dpy, scrnum, attrib);
		if (!visinfo)
		{
			Sys_Error("qkHack: Error couldn't get an RGB, Double-buffered, Depth visual\n");
		}
		break;
	case PSL_NONE:
		break;	//erm
	}

	ActiveApp = false;
	if (fullscreenflags & FULLSCREEN_LEGACY)
	{
		vid_decoywindow = X_CreateWindow(false, visinfo, 640, 480);
		vid_window = X_CreateWindow(true, visinfo, width, height);
	}
	else
		vid_window = X_CreateWindow(false, visinfo, width, height);

	CL_UpdateWindowTitle();
	/*make it visible*/

#ifdef WITH_VMODE
	if (fullscreen & FULLSCREEN_VMODE)
	{
		XRaiseWindow(vid_dpy, vid_window);
		XWarpPointer(vid_dpy, None, vid_window, 0, 0, 0, 0, 0, 0);
		XFlush(vid_dpy);
		// Move the viewport to top left
		XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);
	}
#endif

	vid_nullcursor = CreateNullCursor(vid_dpy, vid_window);

	XFlush(vid_dpy);

#ifdef WITH_VMODE
	if (vidmode_ext >= 2)
	{
		int rampsize = 256;
		XF86VidModeGetGammaRampSize(vid_dpy, scrnum, &rampsize);
		if (rampsize != 256)
		{
			originalapplied = false;
			Con_Printf("Gamma ramps are not of 256 components (but %i).\n", rampsize);
		}
		else
			originalapplied = XF86VidModeGetGammaRamp(vid_dpy, scrnum, 256, originalramps[0], originalramps[1], originalramps[2]);
	}
	else
		originalapplied = false;
#endif

	switch(currentpsl)
	{
	case PSL_GLX:
		ctx = qglXCreateContext(vid_dpy, visinfo, NULL, True);
		if (!ctx)
		{
			Con_Printf("Failed to create GLX context.\n");
			GLVID_Shutdown();
			return false;
		}

		if (!qglXMakeCurrent(vid_dpy, vid_window, ctx))
		{
			Con_Printf("glXMakeCurrent failed\n");
			GLVID_Shutdown();
			return false;
		}

		GL_Init(&GLX_GetSymbol);
		break;
#ifdef USE_EGL
	case PSL_EGL:
		EGL_Init(info, palette, vid_window, vid_dpy);
		GL_Init(&EGL_Proc);
		break;
#endif
	case PSL_NONE:
		break;
	}

	GLVID_SetPalette(palette);
	GLVID_ShiftPalette(palette);

	qglGetIntegerv(GL_STENCIL_BITS, &gl_stencilbits);

	InitSig(); // trap evil signals

	//probably going to be resized in the event handler
	vid.pixelwidth = fullscreenwidth = width;
	vid.pixelheight = fullscreenheight = height;

	vid.numpages = 2;

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);
	if (fullscreenflags & FULLSCREEN_WM)
		X_GoFullscreen();
	if (fullscreenflags & FULLSCREEN_LEGACY)
		XMoveResizeWindow(vid_dpy, vid_window, 0, 0, fullscreenwidth, fullscreenheight);
	if (fullscreenflags)
		fullscreenflags |= FULLSCREEN_ACTIVE;

	vid.recalc_refdef = 1;				// force a surface cache flush

	if (Cvar_Get("vidx_grabkeyboard", "0", 0, "Additional video options")->value)
		XGrabKeyboard(vid_dpy, vid_window,
				  False,
				  GrabModeAsync, GrabModeAsync,
				  CurrentTime);
	else
		XSetInputFocus(vid_dpy, vid_window, RevertToParent, CurrentTime);
	XRaiseWindow(vid_dpy, vid_window);
	if (fullscreenflags & FULLSCREEN_LEGACY)
		XMoveResizeWindow(vid_dpy, vid_window, 0, 0, fullscreenwidth, fullscreenheight);

	return true;
}
qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	return X11VID_Init(info, palette, PSL_GLX);
}
#ifdef USE_EGL
qboolean EGLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	return X11VID_Init(info, palette, PSL_EGL);
}
#endif

void Sys_SendKeyEvents(void)
{
	if (gracefulexit)
	{
		Cbuf_AddText("\nquit\n", RESTRICT_LOCAL);
		gracefulexit = false;
	}
	if (vid_dpy && vid_window)
	{
		qboolean wantwindowed;

		while (XPending(vid_dpy))
			GetEvent();

		if (modeswitchpending && modeswitchtime < Sys_Milliseconds())
		{
			if (old_windowed_mouse)
			{
				Con_DPrintf("uninstall grabs\n");
				uninstall_grabs();
				XUndefineCursor(vid_dpy, vid_window);
				old_windowed_mouse = false;
			}
			if (modeswitchpending > 0)
			{
				//entering fullscreen mode
#ifdef WITH_VMODE
				if (fullscreenflags & FULLSCREEN_VMODE)
				{
					if (!(fullscreenflags & FULLSCREEN_VMODEACTIVE))
					{
						// change to the mode
						XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[vidmode_usemode]);
						fullscreenflags |= FULLSCREEN_VMODEACTIVE;
						// Move the viewport to top left
					}
					XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);
				}
#endif
				Cvar_ForceCallback(&v_gamma);

				/*release the mouse now, because we're paranoid about clip regions*/
				if (fullscreenflags & FULLSCREEN_WM)
					X_GoFullscreen();
				if (fullscreenflags & FULLSCREEN_LEGACY)
				{
					XMoveWindow(vid_dpy, vid_window, 0, 0);
					XReparentWindow(vid_dpy, vid_window, vid_root, 0, 0);
					//XUnmapWindow(vid_dpy, vid_decoywindow);
					//make sure we have it
					XSetInputFocus(vid_dpy, vid_window, RevertToParent, CurrentTime);
					XRaiseWindow(vid_dpy, vid_window);
					XMoveResizeWindow(vid_dpy, vid_window, 0, 0, fullscreenwidth, fullscreenheight);
				}
				if (fullscreenflags)
					fullscreenflags |= FULLSCREEN_ACTIVE;
			}
			if (modeswitchpending < 0)
			{
				//leave fullscreen mode
		 		if (!COM_CheckParm("-stayactive"))
 				{	//a parameter that leaves the program fullscreen if you taskswitch.
 					//sounds pointless, works great with two moniters. :D
					if (fullscreenflags & FULLSCREEN_VMODE)
					{
	 					if (originalapplied)
							XF86VidModeSetGammaRamp(vid_dpy, scrnum, 256, originalramps[0], originalramps[1], originalramps[2]);
						if (fullscreenflags & FULLSCREEN_VMODEACTIVE)
						{
							XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[0]);
							fullscreenflags &= ~FULLSCREEN_VMODEACTIVE;
						}
					}
				}
				if (fullscreenflags & FULLSCREEN_WM)
					X_GoWindowed();
				if (fullscreenflags & FULLSCREEN_LEGACY)
				{
					XMapWindow(vid_dpy, vid_decoywindow);
					XReparentWindow(vid_dpy, vid_window, vid_decoywindow, 0, 0);
					XResizeWindow(vid_dpy, vid_decoywindow, 640, 480);
				}
				fullscreenflags &= ~FULLSCREEN_ACTIVE;
			}
	                modeswitchpending = 0;
		}

		if (modeswitchpending)
			return;

		wantwindowed = !!_windowed_mouse.value;
		if (!ActiveApp)
			wantwindowed = false;
		if (Key_MouseShouldBeFree() && !fullscreenflags)
			wantwindowed = false;

		if (old_windowed_mouse != wantwindowed)
		{
			old_windowed_mouse = wantwindowed;

			if (!wantwindowed)
			{
				Con_DPrintf("uninstall grabs\n");
				/* ungrab the pointer */
				uninstall_grabs();
				XUndefineCursor(vid_dpy, vid_window);
			}
			else
			{
				Con_DPrintf("install grabs\n");
				/* grab the pointer */
				if (fullscreenflags & FULLSCREEN_ACTIVE)
					install_grabs();
				else
					uninstall_grabs();	//don't grab the mouse when fullscreen. its a: not needed. b: breaks gnome's alt-tab.
				/*hide the cursor*/
				XDefineCursor(vid_dpy, vid_window, vid_nullcursor);
			}
		}
	}
}

void Force_CenterView_f (void)
{
	cl.playerview[0].viewangles[PITCH] = 0;
}


//these are done from the x11 event handler. we don't support evdev.
void INS_Move(float *movements, int pnum)
{
}
void INS_Commands(void)
{
}
void INS_Init(void)
{
}
void INS_ReInit(void)
{
}
void INS_Shutdown(void)
{
}

void GL_DoSwap(void) {}

void GLVID_SetCaption(char *text)
{
	XStoreName(vid_dpy, vid_window, text);
}

#ifdef USE_EGL
#include "shader.h"
#include "gl_draw.h"
rendererinfo_t eglrendererinfo =
{
	"EGL",
	{
		"egl"
	},
	QR_OPENGL,

	GLDraw_Init,
	GLDraw_DeInit,

	GL_LoadTextureFmt,
	GL_LoadTexture8Pal24,
	GL_LoadTexture8Pal32,
	GL_LoadCompressed,
	GL_FindTexture,
	GL_AllocNewTexture,
	GL_UploadFmt,
	GL_DestroyTexture,

	GLR_Init,
	GLR_DeInit,
	GLR_RenderView,

	GLR_NewMap,
	GLR_PreNewMap,

	Surf_AddStain,
	Surf_LessenStains,

	RMod_Init,
	RMod_Shutdown,
	RMod_ClearAll,
	RMod_ForName,
	RMod_FindName,
	RMod_Extradata,
	RMod_TouchModel,

	RMod_NowLoadExternal,
	RMod_Think,

	Mod_GetTag,
	Mod_TagNumForName,
	Mod_SkinNumForName,
	Mod_FrameNumForName,
	Mod_FrameDuration,

	EGLVID_Init,
	GLVID_DeInit,
	GLVID_SetPalette,
	GLVID_ShiftPalette,
	GLVID_GetRGBInfo,

	GLVID_SetCaption,       //setcaption


	GLSCR_UpdateScreen,

	GLBE_SelectMode,
	GLBE_DrawMesh_List,
	GLBE_DrawMesh_Single,
	GLBE_SubmitBatch,
	GLBE_GetTempBatch,
	GLBE_DrawWorld,
	GLBE_Init,
	GLBE_GenBrushModelVBO,
	GLBE_ClearVBO,
	GLBE_UploadAllLightmaps,
	GLBE_SelectEntity,
	GLBE_SelectDLight,
	GLBE_LightCullModel,

	""
};
#endif

#if 1
char *Sys_GetClipboard(void)
{
	Atom xa_clipboard = XInternAtom(vid_dpy, "PRIMARY", false);
	Atom xa_string = XInternAtom(vid_dpy, "UTF8_STRING", false);
	Window clipboardowner = XGetSelectionOwner(vid_dpy, xa_clipboard);
	if (clipboardowner != None && clipboardowner != vid_window)
	{
		int fmt;
		Atom type;
		unsigned long nitems, bytesleft;
		unsigned char *data;
		XConvertSelection(vid_dpy, xa_clipboard, xa_string, None, vid_window, CurrentTime);
		XFlush(vid_dpy);
		XGetWindowProperty(vid_dpy, vid_window, xa_string, 0, 0, False, AnyPropertyType, &type, &fmt, &nitems, &bytesleft, &data);
		
		return data;
	}
        return clipboard_buffer;
}

void Sys_CloseClipboard(char *bf)
{
	if (bf == clipboard_buffer)
		return;

	XFree(bf);
}

void Sys_SaveClipboard(char *text)
{
	Atom xa_clipboard = XInternAtom(vid_dpy, "PRIMARY", false);
        Q_strncpyz(clipboard_buffer, text, SYS_CLIPBOARD_SIZE);
	XSetSelectionOwner(vid_dpy, xa_clipboard, vid_window, CurrentTime);
}
#endif

