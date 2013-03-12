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


#define WARP_WIDTH              320
#define WARP_HEIGHT             200

static Display *vid_dpy = NULL;
static Cursor vid_nullcursor;
static Window vid_window;
static GLXContext ctx = NULL;
int scrnum;

static qboolean vidglx_fullscreen;

static float old_windowed_mouse = 0;

#ifdef USE_DGA
static int dgamouse = 0;
#endif

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask)

#define X_MASK (KEY_MASK | MOUSE_MASK | ResizeRequest | StructureNotifyMask | FocusChangeMask | VisibilityChangeMask)


#ifdef WITH_VMODE
static int vidmode_ext = 0;
static XF86VidModeModeInfo **vidmodes;
static int num_vidmodes;
static qboolean vidmode_active = false;
static int vidmode_usemode = -1;	//so that it can be reset if they switch away.

unsigned short originalramps[3][256];
qboolean originalapplied;	//states that the origionalramps arrays are valid, and contain stuff that we should revert to on close
#endif

extern cvar_t	_windowed_mouse;

/*-----------------------------------------------------------------------*/

float		gldepthmin, gldepthmax;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean is8bit = false;
qboolean isPermedia = false;
qboolean mouseactive = false;
qboolean ActiveApp = false;


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

void ClearAllStates (void)
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
	XEvent event;
	int b;
	unsigned int uc;
	qboolean wantwindowed;
	qboolean x11violations = true;

	if (!vid_dpy)
		return;

	XNextEvent(vid_dpy, &event);

	switch (event.type) {
	case ResizeRequest:
		vid.pixelwidth = event.xresizerequest.width;
		vid.pixelheight = event.xresizerequest.height;
		break;
	case ConfigureNotify:
		vid.pixelwidth = event.xconfigurerequest.width;
		vid.pixelheight = event.xconfigurerequest.height;
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
#ifdef WITH_VMODE
		if (vidmode_ext && vidmode_usemode>=0)
		if (!ActiveApp)
		{	//KDE doesn't seem to like us, in that you can't alt-tab back or click to activate.
			//This allows us to steal input focus back from the window manager
			XSetInputFocus(vid_dpy, vid_window, RevertToParent, CurrentTime);
		}
#endif
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
		ActiveApp = true;
#ifdef WITH_VMODE
		if (vidmode_ext && vidmode_usemode>=0)
		{
			if (!vidmode_active)
			{
				// change to the mode
				XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[vidmode_usemode]);
				vidmode_active = true;
				// Move the viewport to top left
			}
			XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);
		}
#endif
		Cvar_ForceCallback(&v_gamma);
		break;
	case FocusOut:
#ifdef WITH_VMODE
		if (originalapplied)
			XF86VidModeSetGammaRamp(vid_dpy, scrnum, 256, originalramps[0], originalramps[1], originalramps[2]);

 		if (!COM_CheckParm("-stayactive"))
 		{	//a parameter that leaves the program fullscreen if you taskswitch.
 			//sounds pointless, works great with two moniters. :D
 			if (originalapplied)
				XF86VidModeSetGammaRamp(vid_dpy, scrnum, 256, originalramps[0], originalramps[1], originalramps[2]);
			if (vidmode_active)
			{
				XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[0]);
				vidmode_active = false;
			}
		}
#endif

		ActiveApp = false;
		ClearAllStates();

		break;
	case ClientMessage:
		{
			char *name = XGetAtomName(vid_dpy, event.xclient.message_type);
			if (!strcmp(name, "WM_PROTOCOLS") && event.xclient.format == 32)
			{
				char *protname = XGetAtomName(vid_dpy, event.xclient.data.l[0]);
				if (!strcmp(protname, "WM_DELETE_WINDOW"))
					Cmd_ExecuteString("menu_quit", RESTRICT_LOCAL);
				else
					Con_Printf("Got message %s\n", protname);
				XFree(protname);
			}
			else
				Con_Printf("Got message %s\n", name);
			XFree(name);
		}
		break;
	default:
//		Con_Printf("%x\n", event.type);
		break;
	}

	wantwindowed = !!_windowed_mouse.value;
	if (!ActiveApp)
		wantwindowed = false;
	if (Key_MouseShouldBeFree() && !vidglx_fullscreen)
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
			install_grabs();
			/*hide the cursor*/
			XDefineCursor(vid_dpy, vid_window, vid_nullcursor);
		}
	}
}


void GLVID_Shutdown(void)
{
#ifdef USE_EGL
	EGL_Shutdown();
#else
	printf("GLVID_Shutdown\n");
	if (!vid_dpy)
		return;

	XUngrabKeyboard(vid_dpy, CurrentTime);
	if (old_windowed_mouse)
		uninstall_grabs();

	if (ctx)
	{
		qglXDestroyContext(vid_dpy, ctx);
		ctx = NULL;
	}

#ifdef WITH_VMODE
	if (originalapplied)
		XF86VidModeSetGammaRamp(vid_dpy, scrnum, 256, originalramps[0], originalramps[1], originalramps[2]);
#endif

	if (vid_window)
		XDestroyWindow(vid_dpy, vid_window);
	if (vid_nullcursor)
		XFreeCursor(vid_dpy, vid_nullcursor);
#ifdef WITH_VMODE
	if (vid_dpy) {
		if (vidmode_active)
			XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[0]);
		vidmode_active = false;

		if (vidmodes)
			XFree(vidmodes);
		vidmodes = NULL;
		num_vidmodes = 0;
	}
#endif
	XCloseDisplay(vid_dpy);
	vid_dpy = NULL;
	vid_window = (Window)NULL;
#endif
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

void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
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
	Con_Printf("Converting 8to24\n");

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
#ifdef USE_EGL
	EGL_BeginRendering();
#endif
}


void GL_EndRendering (void)
{
#ifdef USE_EGL
	EGL_EndRendering();
#else
//return;
//we don't need the flush, XSawpBuffers does it for us.
//chances are, it's version is more suitable anyway. At least there's the chance that it might be.
	qglXSwapBuffers(vid_dpy, vid_window);
#endif
}

qboolean GLVID_Is8bit(void)
{
	return is8bit;
}

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
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
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
#ifdef USE_EGL
	XVisualInfo vinfodef;
#endif
	XVisualInfo *visinfo;
	qboolean fullscreen = false;
	Atom prots[1];

#ifdef WITH_VMODE
	int MajorVersion, MinorVersion;

	if (info->fullscreen)
		fullscreen = true;
#endif


	S_Startup();

#ifdef USE_EGL
	if (!EGL_LoadLibrary(info->glrenderer))
	{
		Con_Printf("couldn't load EGL library\n");
		return false;
	}
#else
	if (!GLX_InitLibrary(info->glrenderer))
	{
		Con_Printf("Couldn't intialise GLX\nEither your drivers are not installed or you need to specify the library name with the gl_driver cvar\n");
		return false;
	}
#endif

// interpret command-line params

// set vid parameters
	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.width = Q_atoi(com_argv[i+1]);
	else
		vid.width = 640;

	vid.width &= ~7; // make it a multiple of eight

	if (vid.width < 320)
		vid.width = 320;

	// pick a conheight that matches with correct aspect
	vid.height = vid.width*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.height = Q_atoi(com_argv[i+1]);
	if (vid.height < 200)
		vid.height = 200;
	if (!vid_dpy)
		vid_dpy = XOpenDisplay(NULL);
	if (!vid_dpy)
	{
		Con_Printf(CON_ERROR "Error: couldn't open the X display\n");
		return false;
	}

	scrnum = DefaultScreen(vid_dpy);
	root = RootWindow(vid_dpy, scrnum);

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
				if (info->width > vidmodes[i]->hdisplay ||
					info->height > vidmodes[i]->vdisplay)
					continue;

				x = info->width - vidmodes[i]->hdisplay;
				y = info->height - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (dist < best_dist)
				{
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1 && (!best_dist || COM_CheckParm("-fullscreen")))
			{
				// change to the mode
				XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[vidmode_usemode=best_fit]);
				vidmode_active = true;
				// Move the viewport to top left
				XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);

			}
			else
				fullscreen = 0;
		}
	}
#endif

	vidglx_fullscreen = fullscreen;

#ifdef USE_EGL
	visinfo = &vinfodef;
	if (!XMatchVisualInfo(vid_dpy, scrnum, info->bpp, TrueColor, visinfo))
//	if (!XMatchVisualInfo(vid_dpy, scrnum, DefaultDepth(vid_dpy, scrnum), TrueColor, &visinfo))
	{
		Sys_Error("Couldn't choose visual for EGL\n");
	}
#else
	visinfo = qglXChooseVisual(vid_dpy, scrnum, attrib);
	if (!visinfo)
	{
		Sys_Error("qkHack: Error couldn't get an RGB, Double-buffered, Depth visual\n");
	}
#endif

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(vid_dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

#ifdef WITH_VMODE	//get rid of borders
	// fullscreen
	if (vidmode_active) {
		mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore |
			CWEventMask | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	}
#endif

	vid_window = XCreateWindow(vid_dpy, root, 0, 0, info->width, info->height,
						0, visinfo->depth, InputOutput,
						visinfo->visual, mask, &attr);

	ActiveApp = false;
	/*ask the window manager to stop triggering bugs in Xlib*/
	prots[0] = XInternAtom(vid_dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(vid_dpy, vid_window, prots, sizeof(prots)/sizeof(prots[0]));
	/*set caption*/
	XStoreName(vid_dpy, vid_window, FULLENGINENAME);
	/*make it visibl*/
	XMapWindow(vid_dpy, vid_window);
	/*put it somewhere*/
	XMoveWindow(vid_dpy, vid_window, 0, 0);

	//XFree(visinfo);

#ifdef WITH_VMODE
	if (vidmode_active) {
		XRaiseWindow(vid_dpy, vid_window);
		XWarpPointer(vid_dpy, None, vid_window, 0, 0, 0, 0, 0, 0);
		XFlush(vid_dpy);
		// Move the viewport to top left
		XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);
	}
#endif

	vid_nullcursor = CreateNullCursor(vid_dpy, vid_window);

	XFlush(vid_dpy);

#ifndef USE_EGL
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

#else
	EGL_Init(info, palette, vid_window);
	GL_Init(&EGL_Proc);
#endif

	GLVID_SetPalette(palette);
	GLVID_ShiftPalette(palette);

	qglGetIntegerv(GL_STENCIL_BITS, &gl_stencilbits);

	InitSig(); // trap evil signals

	vid.pixelwidth = info->width;
	vid.pixelheight = info->height;

	if (vid.height > info->height)
		vid.height = info->height;
	if (vid.width > info->width)
		vid.width = info->width;

	vid.numpages = 2;

	Con_SafePrintf ("Video mode %dx%d initialized.\n", info->width, info->height);

	vid.recalc_refdef = 1;				// force a surface cache flush

	if (Cvar_Get("vidx_grabkeyboard", "0", 0, "Additional video options")->value)
		XGrabKeyboard(vid_dpy, vid_window,
				  False,
				  GrabModeAsync, GrabModeAsync,
				  CurrentTime);
	else
		XSetInputFocus(vid_dpy, vid_window, RevertToParent, CurrentTime);
	XRaiseWindow(vid_dpy, vid_window);

	return true;
}

void Sys_SendKeyEvents(void)
{
	if (vid_dpy && vid_window) {
		while (XPending(vid_dpy))
			GetEvent();
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

