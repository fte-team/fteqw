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
// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#ifdef GLQUAKE
#include "glquake.h"
#include "winquake.h"
#include "resource.h"
#include <commctrl.h>

#ifndef SetWindowLongPtr	//yes its a define, for unicode support
#define SetWindowLongPtr SetWindowLong
#endif

#ifndef CDS_FULLSCREEN
	#define CDS_FULLSCREEN 4
#endif

#ifndef WM_XBUTTONDOWN
   #define WM_XBUTTONDOWN      0x020B
   #define WM_XBUTTONUP      0x020C
#endif
#ifndef MK_XBUTTON1
   #define MK_XBUTTON1         0x0020
#endif
#ifndef MK_XBUTTON2
   #define MK_XBUTTON2         0x0040
#endif
// copied from DarkPlaces in an attempt to grab more buttons
#ifndef MK_XBUTTON3
   #define MK_XBUTTON3         0x0080
#endif
#ifndef MK_XBUTTON4
   #define MK_XBUTTON4         0x0100
#endif
#ifndef MK_XBUTTON5
   #define MK_XBUTTON5         0x0200
#endif
#ifndef MK_XBUTTON6
   #define MK_XBUTTON6         0x0400
#endif
#ifndef MK_XBUTTON7
   #define MK_XBUTTON7         0x0800
#endif

#ifndef WM_INPUT
	#define WM_INPUT 255
#endif

#ifndef WS_EX_LAYERED
	#define WS_EX_LAYERED 0x00080000
#endif
#ifndef LWA_ALPHA
	#define LWA_ALPHA 0x00000002
#endif
typedef BOOL (WINAPI *lpfnSetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags); 

extern cvar_t vid_conwidth, vid_conautoscale;


#define WINDOW_CLASS_NAME "FTEGLQuake"

#define MAX_MODE_LIST	128
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

extern cvar_t vid_width;
extern cvar_t vid_height;
extern cvar_t vid_wndalpha;

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;

#ifdef USE_D3D
void D3DInitialize(void);
void d3dSetMode(int fullscreen, int width, int height, int bpp, int zbpp);
#endif
BOOL bSetupPixelFormat(HDC hDC);

//qboolean VID_SetWindowedMode (int modenum);
//qboolean VID_SetFullDIBMode (int modenum);
qboolean VID_SetWindowedMode (rendererstate_t *info);	//-1 on bpp or hz for default.
qboolean VID_SetFullDIBMode (rendererstate_t *info);	//-1 on bpp or hz for default.

qboolean		scr_skipupdate;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean	leavecurrentmode= true;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
extern qboolean	mouseactive;  // from in_win.c
static HICON	hIcon;
extern qboolean vid_isfullscreen;

unsigned short originalgammaramps[3][256];

qboolean vid_initializing;

qboolean VID_AttachGL (rendererstate_t *info);

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow, dibwindow;

unsigned char	vid_curpal[256*3];

HGLRC	baseRC;
HDC		maindc;


HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

//unsigned short	d_8to16rgbtable[256];
//unsigned	d_8to24rgbtable[256];
//unsigned short	d_8to16bgrtable[256];
//unsigned	d_8to24bgrtable[256];

modestate_t	modestate = MS_UNINIT;


LONG WINAPI GLMainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
qboolean GLAppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (HWND hWnd);
void GL_Init(void *(*getglfunction) (char *name));

typedef void (APIENTRY *lp3DFXFUNC) (int, int, int, int, int, const void*);
lp3DFXFUNC qglColorTableEXT;
qboolean is8bit = false;
qboolean isPermedia = false;

//====================================
// Note that 0 is MODE_WINDOWED
extern cvar_t	vid_mode;
// Note that 3 is MODE_FULLSCREEN_DEFAULT
extern cvar_t		_vid_wait_override;
extern cvar_t		_windowed_mouse;
extern cvar_t		vid_hardwaregamma;
extern cvar_t		vid_desktopgamma;
extern cvar_t		gl_lateswap;
extern cvar_t		vid_preservegamma;

extern cvar_t		vid_gl_context_version;
extern cvar_t		vid_gl_context_debug;
extern cvar_t		vid_gl_context_forwardcompatible;
extern cvar_t		vid_gl_context_compatibility;

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

HMODULE hInstGL = NULL;
HMODULE hInstwgl = NULL;
char opengldllname[MAX_OSPATH];

//just GetProcAddress with a safty net.
void *getglfunc(char *name)
{
	FARPROC proc;
	proc = qwglGetProcAddress?qwglGetProcAddress(name):NULL;
	if (!proc)
	{
		proc = GetProcAddress(hInstGL, name);
		TRACE(("dbg: getglfunc: gpa %s: success %i\n", name, !!proc));
		return proc;
	}
	TRACE(("dbg: getglfunc: glgpa %s: success %i\n", name, !!proc));
	return proc;
}
void *getwglfunc(char *name)
{
	FARPROC proc;
	TRACE(("dbg: getwglfunc: %s: getting\n", name));

	proc = GetProcAddress(hInstGL, name);
	if (!proc)
	{
		if (!hInstwgl)
		{
			TRACE(("dbg: getwglfunc: explicitly loading opengl32.dll\n", name));
			hInstwgl = LoadLibrary("opengl32.dll");
		}
		TRACE(("dbg: getwglfunc: %s: wglgetting\n", name));
		proc = GetProcAddress(hInstwgl, name);
		TRACE(("dbg: getwglfunc: gpa %s: success %i\n", name, !!proc));
		if (!proc)
			Sys_Error("GL function %s was not found in %s\nPossibly you do not have a full enough gl implementation", name, opengldllname);
	}
	TRACE(("dbg: getwglfunc: glgpa %s: success %i\n", name, !!proc));
	return proc;
}

HGLRC (WINAPI *qwglCreateContext)(HDC);
BOOL  (WINAPI *qwglDeleteContext)(HGLRC);
HGLRC (WINAPI *qwglGetCurrentContext)(VOID);
HDC   (WINAPI *qwglGetCurrentDC)(VOID);
PROC  (WINAPI *qwglGetProcAddress)(LPCSTR);
BOOL  (WINAPI *qwglMakeCurrent)(HDC, HGLRC);
BOOL  (WINAPI *qSwapBuffers)(HDC);

BOOL (WINAPI *qwglSwapIntervalEXT) (int);

BOOL (APIENTRY *qGetDeviceGammaRamp)(HDC hDC, GLvoid *ramp);
BOOL (APIENTRY *qSetDeviceGammaRamp)(HDC hDC, GLvoid *ramp);

BOOL (APIENTRY *qwglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);

HGLRC (APIENTRY *qwglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);
#define WGL_CONTEXT_MAJOR_VERSION_ARB		0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB		0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB			0x2093
#define WGL_CONTEXT_FLAGS_ARB				0x2094
#define		WGL_CONTEXT_DEBUG_BIT_ARB				0x0001
#define		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB	0x0002
#define WGL_CONTEXT_PROFILE_MASK_ARB		0x9126
#define		WGL_CONTEXT_CORE_PROFILE_BIT_ARB	0x00000001
#define		WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define ERROR_INVALID_VERSION_ARB			0x2095
#define	ERROR_INVALID_PROFILE_ARB		0x2096



qboolean GLInitialise (char *renderer)
{
	if (hInstGL)
		FreeModule(hInstGL);
	if (hInstwgl)
		FreeModule(hInstwgl);
	hInstwgl=NULL;

	strcpy(opengldllname, renderer);

	if (*renderer)
	{
		Con_DPrintf ("Loading renderer dll \"%s\"", renderer);
		hInstGL = LoadLibrary(opengldllname);

		if (hInstGL)
			Con_DPrintf (" Success\n");
		else
			Con_DPrintf (" Failed\n");
	}
	else
		hInstGL = NULL;
	
	if (!hInstGL)
	{
		unsigned int emode;
		strcpy(opengldllname, "opengl32");
		Con_DPrintf ("Loading renderer dll \"%s\"", opengldllname);
		emode = SetErrorMode(SEM_FAILCRITICALERRORS); /*no annoying errors if they use glide*/
		hInstGL = LoadLibrary(opengldllname);
		SetErrorMode(emode);

		if (hInstGL)
			Con_DPrintf (" Success\n");
		else
			Con_DPrintf (" Failed\n");
	}
	if (!hInstGL)
	{
		if (*renderer)
			Con_Printf ("Couldn't load %s or %s\n", renderer, opengldllname);
		else
			Con_Printf ("Couldn't load %s\n", opengldllname);
		return false;
	}

	Con_DPrintf ("Loaded renderer dll %s\n", opengldllname);

	// windows dependant
	qwglCreateContext		= (void *)getwglfunc("wglCreateContext");
	qwglDeleteContext		= (void *)getwglfunc("wglDeleteContext");
	qwglGetCurrentContext	= (void *)getwglfunc("wglGetCurrentContext");
	qwglGetCurrentDC		= (void *)getwglfunc("wglGetCurrentDC");
	qwglGetProcAddress		= (void *)getwglfunc("wglGetProcAddress");
	qwglMakeCurrent			= (void *)getwglfunc("wglMakeCurrent");
	qSwapBuffers			= SwapBuffers;

	qGetDeviceGammaRamp			= (void *)getglfunc("wglGetDeviceGammaRamp3DFX");
	qSetDeviceGammaRamp			= (void *)getglfunc("wglSetDeviceGammaRamp3DFX");

	TRACE(("dbg: GLInitialise: got wgl funcs\n"));

	return true;
}

void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
//    RECT    rect;
    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY*2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

qboolean VID_SetWindowedMode (rendererstate_t *info)
//qboolean VID_SetWindowedMode (int modenum)
{
	int i;
	HDC				hdc;
	int				lastmodestate, wwidth, wheight;
	RECT			rect;

	hdc = GetDC(NULL);
	if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
	{
		ReleaseDC(NULL, hdc);
		Con_Printf("Can't run GL in non-RGB mode\n");
		return false;
	}
	ReleaseDC(NULL, hdc);

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = info->width;
	WindowRect.bottom = info->height;


	if (sys_parentwindow)
	{
		SetWindowLong(sys_parentwindow, GWL_STYLE, GetWindowLong(sys_parentwindow, GWL_STYLE)|WS_OVERLAPPED);
		WindowStyle = WS_CHILDWINDOW|WS_OVERLAPPED;
		ExWindowStyle = 0;

		if (info->width > sys_parentwidth)
			WindowRect.right = sys_parentwidth;
		else if (info->width < sys_parentwidth)
			WindowRect.left = (sys_parentwidth - info->width)/2;

		if (info->height > sys_parentheight)
			WindowRect.bottom = sys_parentheight;
		else if (info->height < sys_parentheight)
			WindowRect.top = (sys_parentheight - info->height)/2;


		WindowRect.right += WindowRect.left;
		WindowRect.bottom += WindowRect.top;
	}
	else
	{
		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
					  WS_MINIMIZEBOX;
		ExWindowStyle = 0;

		WindowStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	}

	DIBWidth = WindowRect.right - WindowRect.left;
	DIBHeight = WindowRect.bottom - WindowRect.top;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	wwidth = rect.right - rect.left;
	wheight = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 WINDOW_CLASS_NAME,
		 FULLENGINENAME,
		 WindowStyle,
		 rect.left, rect.top,
		 wwidth,
		 wheight,
		 sys_parentwindow,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
	{
		Con_Printf ("Couldn't create DIB window");
		return false;
	}

	if (!sys_parentwindow)
	{
#ifdef WS_EX_LAYERED
		int av;
		av = 255*vid_wndalpha.value;
		if (av < 70)
			av = 70;
		if (av < 255)
		{
			HMODULE hm = GetModuleHandle("user32.dll");
			lpfnSetLayeredWindowAttributes pSetLayeredWindowAttributes;
			pSetLayeredWindowAttributes = (void*)GetProcAddress(hm, "SetLayeredWindowAttributes");

			if (pSetLayeredWindowAttributes)
			{
				// Set WS_EX_LAYERED on this window 
				SetWindowLong(dibwindow, GWL_EXSTYLE, GetWindowLong(dibwindow, GWL_EXSTYLE) | WS_EX_LAYERED);

				// Make this window 70% alpha
				pSetLayeredWindowAttributes(dibwindow, 0, (BYTE)av, LWA_ALPHA);
			}
		}
#endif

		// Center and show the DIB window
		CenterWindow(dibwindow, WindowRect.right - WindowRect.left,
					 WindowRect.bottom - WindowRect.top, false);
	}
	else
		SetFocus(dibwindow);

//	ShowWindow (dibwindow, SW_SHOWDEFAULT);
//	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);


	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.width = Q_atoi(com_argv[i+1]);
	else
	{
		vid.width = 640;
	}

	vid.width &= 0xfff8; // make it a multiple of eight

	if (vid.width < 320)
		vid.width = 320;

	// pick a conheight that matches with correct aspect
	vid.height = vid.width*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.height = Q_atoi(com_argv[i+1]);
	if (vid.height < 200)
		vid.height = 200;

	if (vid.height > info->height)
		vid.height = info->height;
	if (vid.width > info->width)
		vid.width = info->width;

	vid.numpages = 2;

	mainwindow = dibwindow;
	vid_isfullscreen=false;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}

void GLVID_SetCaption(char *text)
{
	SetWindowText(mainwindow, text);
}


qboolean VID_SetFullDIBMode (rendererstate_t *info)
{
	int i;
	HDC				hdc;
	int				lastmodestate, wwidth, wheight;
	RECT			rect;

	if (leavecurrentmode && Q_strcasecmp(info->glrenderer, "D3D"))	//don't do this with d3d - d3d should set it's own video mode.
	{	//make windows change res.
		gdevmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
		if (info->bpp)
			gdevmode.dmFields |= DM_BITSPERPEL;
		if (info->rate)
			gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
		gdevmode.dmBitsPerPel = info->bpp;
		if (info->bpp && (gdevmode.dmBitsPerPel < 15))
		{
			Con_Printf("Forcing at least 16bpp\n");
			gdevmode.dmBitsPerPel = 16;
		}
		gdevmode.dmDisplayFrequency = info->rate;
		gdevmode.dmPelsWidth = info->width;
		gdevmode.dmPelsHeight = info->height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			Con_SafePrintf((gdevmode.dmFields&DM_DISPLAYFREQUENCY)?"Windows rejected mode %i*%i*%i*%i\n":"Windows rejected mode %i*%i*%i\n", gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, gdevmode.dmBitsPerPel, gdevmode.dmDisplayFrequency);
			return false;
		}
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = info->width;
	WindowRect.bottom = info->height;

	DIBWidth = info->width;
	DIBHeight = info->height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	wwidth = rect.right - rect.left;
	wheight = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 WINDOW_CLASS_NAME,
		 FULLENGINENAME,
		 WindowStyle,
		 rect.left, rect.top,
		 wwidth,
		 wheight,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);


	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.width = Q_atoi(com_argv[i+1]);
	else
		vid.width = 640;

	vid.width &= 0xfff8; // make it a multiple of eight

	if (vid.width < 320)
		vid.width = 320;

	// pick a conheight that matches with correct aspect
	vid.height = vid.width*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.height = Q_atoi(com_argv[i+1]);
	if (vid.height < 200)
		vid.height = 200;

	if (vid.height > info->height)
		vid.height = info->height;
	if (vid.width > info->width)
		vid.width = info->width;

	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;
	vid_isfullscreen=true;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}

extern int gammaworks;
static void ReleaseGL(void);
static qboolean CreateMainWindow(rendererstate_t *info)
{
	qboolean		stat;
	if (!info->fullscreen)
	{
		if (_windowed_mouse.value && (key_dest == key_game || key_dest == key_menu))
		{
			TRACE(("dbg: GLVID_SetMode: VID_SetWindowedMode\n"));
			stat = VID_SetWindowedMode(info);
		}
		else
		{
			TRACE(("dbg: GLVID_SetMode: VID_SetWindowedMode 2\n"));
			stat = VID_SetWindowedMode(info);
		}
	}
	else
	{
		TRACE(("dbg: GLVID_SetMode: VID_SetFullDIBMode\n"));
		stat = VID_SetFullDIBMode(info);
	}

	IN_UpdateGrabs(info->fullscreen, ActiveApp);

	return stat;
}
BOOL CheckForcePixelFormat(rendererstate_t *info);
void VID_UnSetMode (void);
int GLVID_SetMode (rendererstate_t *info, unsigned char *palette)
{
	int				temp;
	qboolean		stat;
#ifndef NPQTV
    MSG				msg;
#endif
//	HDC				hdc;

	TRACE(("dbg: GLVID_SetMode\n"));

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	// Set either the fullscreen or windowed mode
	qwglChoosePixelFormatARB = NULL;
	qwglCreateContextAttribsARB = NULL;
	stat = CreateMainWindow(info);
	if (stat)
	{
		stat = VID_AttachGL(info);
		if (stat)
		{
			TRACE(("dbg: GLVID_SetMode: attaching gl okay\n"));
			if (CheckForcePixelFormat(info))
			{
				HMODULE oldgl = hInstGL;
				hInstGL = NULL;	//don't close the gl library, just in case
				VID_UnSetMode();
				hInstGL = oldgl;

				if (CreateMainWindow(info) && VID_AttachGL(info))
				{
					//we have our multisample window
				}
				else
				{
					//multisample failed
					//try the origional way
					if (!CreateMainWindow(info) || !VID_AttachGL(info))
					{
						Con_Printf("Failed to undo antialising. Giving up.\n");
						return false;	//eek
					}
				}
			}
		}
		else
		{
			TRACE(("dbg: GLVID_SetMode: attaching gl failed\n"));
			return false;
		}
	}

	if (!stat)
	{
		TRACE(("dbg: GLVID_SetMode: VID_Set... failed\n"));
		return false;
	}

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus (mainwindow);

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	VID_SetPalette (palette);

#ifndef NPQTV
	/*I don't like this, but if we */
	while (PeekMessage (&msg, mainwindow, 0, 0, PM_REMOVE))
	{
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}
	Sleep (100);
#endif

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	GLVID_SetPalette (palette);

	vid.recalc_refdef = 1;

	maindc = GetDC(mainwindow);
	if (vid_desktopgamma.value)
	{
		HDC hDC = GetDC(GetDesktopWindow());
		gammaworks = qGetDeviceGammaRamp(hDC, originalgammaramps);
		ReleaseDC(GetDesktopWindow(), hDC);
	}
	else
		gammaworks = qGetDeviceGammaRamp(maindc, originalgammaramps);

	return true;
}

static void ReleaseGL(void)
{
	HGLRC hRC;
   	HDC	  hDC = NULL;

	if (qwglGetCurrentContext)
	{
		hRC = qwglGetCurrentContext();
		hDC = qwglGetCurrentDC();

    	qwglMakeCurrent(NULL, NULL);

    	if (hRC)
    		qwglDeleteContext(hRC);
	}
	qwglGetCurrentContext=NULL;

	if (hDC && dibwindow)
		ReleaseDC(dibwindow, hDC);
}

void VID_UnSetMode (void)
{
	if (mainwindow && vid_initialized)
	{
		GLAppActivate(false, false);

		vid_canalttab = false;
		ReleaseGL();

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);
	}

	if (mainwindow)
	{
		dibwindow=NULL;
	//	ShowWindow(mainwindow, SW_HIDE);
	//	SetWindowLongPtr(mainwindow, GWL_WNDPROC, DefWindowProc);
	//	PostMessage(mainwindow, WM_CLOSE, 0, 0);
		DestroyWindow(mainwindow);
		mainwindow = NULL;
	}
	if (hInstGL)
	{
		FreeLibrary(hInstGL);
		hInstGL = NULL;
	}
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (HWND hWnd)
{
	POINT p;
	RECT nr;
	GetClientRect(hWnd, &nr);

	//if its bad then we're probably minimised
	if (nr.right <= nr.left)
		return;
	if (nr.bottom <= nr.top)
		return;

	WindowRect = nr;
	p.x = 0;
	p.y = 0;
	ClientToScreen(hWnd, &p);
	window_x = p.x;
	window_y = p.y;
	window_width = WindowRect.right - WindowRect.left;
	window_height = WindowRect.bottom - WindowRect.top;
	vid.pixelwidth = window_width;
	vid.pixelheight = window_height;

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


//====================================

qboolean VID_AttachGL (rendererstate_t *info)
{	//make sure we can get a valid renderer.
	do
	{
#ifdef USE_D3D
		if (!Q_strcasecmp(info->glrenderer, "D3D"))
		{
			int zbpp = info->bpp > 16 ? 24 : 16;
			gl_canstencil = false;
			TRACE(("dbg: VID_AttachGL: D3DInitialize\n"));
			D3DInitialize();	//replacement of GLInitialise, to get the function pointers set up.
			if (COM_CheckParm("-zbpp"))
			{
				zbpp = Q_atoi(com_argv[COM_CheckParm("-zbpp")+1]);
			}
			TRACE(("dbg: VID_AttachGL: d3dSetMode\n"));
			d3dSetMode(info->fullscreen, info->width, info->height, info->bpp, zbpp);	//d3d cheats to get it's dimensions and stuff... One that we can currently live with though.

			maindc = GetDC(mainwindow);

			Con_Printf(CON_NOTICE "OpenGL to Direct3D wrapper enabled\n");	//green to make it show.
			break;
		}
#endif
		TRACE(("dbg: VID_AttachGL: GLInitialise\n"));
		if (GLInitialise(info->glrenderer))
		{
			maindc = GetDC(mainwindow);
			TRACE(("dbg: VID_AttachGL: bSetupPixelFormat\n"));
			if (bSetupPixelFormat(maindc))
				break;
		}

		if (!*info->glrenderer || !stricmp(info->glrenderer, "opengl32.dll") || !stricmp(info->glrenderer, "opengl32"))	//go for windows system dir if we failed with the default. Should help to avoid the 3dfx problem.
		{
			char systemgl[MAX_OSPATH+1];
			GetSystemDirectory(systemgl, sizeof(systemgl)-1);
			strncat(systemgl, "\\", sizeof(systemgl)-1);
			if (*info->glrenderer)
				strncat(systemgl, info->glrenderer, sizeof(systemgl)-1);
			else
				strncat(systemgl, "opengl32.dll", sizeof(systemgl)-1);
			TRACE(("dbg: VID_AttachGL: GLInitialise (system dir specific)\n"));
			if (GLInitialise(systemgl))
			{
				maindc = GetDC(mainwindow);
				TRACE(("dbg: VID_AttachGL: bSetupPixelFormat\n"));
				if (bSetupPixelFormat(maindc))
					break;
			}
		}

		TRACE(("dbg: VID_AttachGL: failed to find a valid dll\n"));
		return false;
	} while(1);
	
	TRACE(("dbg: VID_AttachGL: qwglCreateContext\n"));

    baseRC = qwglCreateContext( maindc );
	if (!baseRC)
	{
		Con_SafePrintf(CON_ERROR "Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.\n");	//green to make it show.
		return false;
	}
	TRACE(("dbg: VID_AttachGL: qwglMakeCurrent\n"));
    if (!qwglMakeCurrent( maindc, baseRC ))
	{
		Con_SafePrintf(CON_ERROR "wglMakeCurrent failed\n");	//green to make it show.
		return false;
	}

	qwglCreateContextAttribsARB = getglfunc("wglCreateContextAttribsARB");
#ifdef _DEBUG
	//attempt to promote that to opengl3.
	if (qwglCreateContextAttribsARB)
	{
		HGLRC opengl3;
		int attribs[7];
		char *mv;
		int i = 0;

		mv = vid_gl_context_version.string;
		while (*mv)
		{
			if (*mv++ == '.')
				break;
		}

		if (*vid_gl_context_version.string)
		{
			attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
			attribs[i++] = (int)vid_gl_context_version.value;
		}
		if (*mv)
		{
			attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
			attribs[i++] = atoi(mv);
		}

		//flags
		attribs[i+1] = 0;
		if (vid_gl_context_debug.value)
			attribs[i+1] |= WGL_CONTEXT_DEBUG_BIT_ARB;
		if (vid_gl_context_forwardcompatible.value)
			attribs[i+1] |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

		if (attribs[i+1])
		{
			attribs[i] = WGL_CONTEXT_FLAGS_ARB;
			i += 2;
		}

		attribs[i+1] = 0;
		if (vid_gl_context_compatibility.value)
			attribs[i+1] |= WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
		else
			attribs[i+1] |= WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
		attribs[i] = WGL_CONTEXT_PROFILE_MASK_ARB;
		i+=2;

		attribs[i] = 0;

		if (!i)
		{
			//just use the default (ie: max = opengl 2.1 or so)
		}
		else if ((opengl3 = qwglCreateContextAttribsARB(maindc, NULL, attribs)))
		{
			qwglMakeCurrent(NULL, NULL);
			qwglDeleteContext(baseRC);

			baseRC = opengl3;
			if (!qwglMakeCurrent( maindc, baseRC ))
			{
				Con_SafePrintf(CON_ERROR "wglMakeCurrent failed\n");	//green to make it show.
				return false;
			}
		}
		else
		{
			DWORD error = GetLastError();
			if (error == ERROR_INVALID_VERSION_ARB)
				Con_Printf("Unsupported OpenGL context version (%s).\n", vid_gl_context_version.string);
			else
				Con_Printf("Unknown error creating an OpenGL (%s) Context.\n", vid_gl_context_version.string);
		}
	}
#endif

	TRACE(("dbg: VID_AttachGL: GL_Init\n"));
	GL_Init(getglfunc);

	qwglChoosePixelFormatARB	= getglfunc("wglChoosePixelFormatARB");

	qwglSwapIntervalEXT		= getglfunc("wglSwapIntervalEXT");
	if (qwglSwapIntervalEXT && *_vid_wait_override.string)
	{
		TRACE(("dbg: VID_AttachGL: qwglSwapIntervalEXT\n"));
		qwglSwapIntervalEXT(_vid_wait_override.value);
	}
	TRACE(("dbg: VID_AttachGL: qSwapBuffers\n"));
	qglClearColor(0, 0, 0, 0);
	qglClear(GL_COLOR_BUFFER_BIT);
	qSwapBuffers(maindc);

	if (!qGetDeviceGammaRamp) qGetDeviceGammaRamp = (void*)GetDeviceGammaRamp;
	if (!qSetDeviceGammaRamp) qSetDeviceGammaRamp = (void*)SetDeviceGammaRamp;

	return true;
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (void)
{
	vid.pixelwidth = WindowRect.right - WindowRect.left;
	vid.pixelheight = WindowRect.bottom - WindowRect.top;

//    if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport (*x, *y, *width, *height);
}

void VID_Wait_Override_Callback(struct cvar_s *var, char *oldvalue)
{
	if (qwglSwapIntervalEXT && *_vid_wait_override.string)
		qwglSwapIntervalEXT(_vid_wait_override.value);
}

void VID_Size_Override_Callback(struct cvar_s *var, char *oldvalue)
{
	int nw = vid_width.value;
	int nh = vid_height.value;
	int nx = 0;
	int ny = 0;

	if (sys_parentwindow && modestate==MS_WINDOWED)
	{
		if (nw > sys_parentwidth)
			nw = sys_parentwidth;
		else
			nx = (sys_parentwidth - nw)/2;
		if (nh > sys_parentheight)
			nh = sys_parentheight;
		else
			ny = (sys_parentheight - nh)/2;

		MoveWindow(mainwindow, nx, ny, nw, nh, FALSE);
	}
}

void VID_WndAlpha_Override_Callback(struct cvar_s *var, char *oldvalue)
{
#ifdef WS_EX_LAYERED
	if (modestate==MS_WINDOWED)
	{
		int av;
		HMODULE hm = GetModuleHandle("user32.dll");
		lpfnSetLayeredWindowAttributes pSetLayeredWindowAttributes;
		pSetLayeredWindowAttributes = (void*)GetProcAddress(hm, "SetLayeredWindowAttributes");

		av = 255 * var->value;
		if (av < 70)
			av = 70;
		if (av > 255)
			av = 255;

		if (pSetLayeredWindowAttributes)
		{
			// Set WS_EX_LAYERED on this window 

			if (av < 255)
			{
				SetWindowLong(mainwindow, GWL_EXSTYLE, GetWindowLong(mainwindow, GWL_EXSTYLE) | WS_EX_LAYERED);

				// Make this window 70% alpha
				pSetLayeredWindowAttributes(mainwindow, 0, (BYTE)av, LWA_ALPHA);
			}
			else
				SetWindowLong(mainwindow, GWL_EXSTYLE, GetWindowLong(mainwindow, GWL_EXSTYLE) & ~WS_EX_LAYERED);
		}
	}
#endif
}

qboolean screenflush;
void GL_DoSwap (void)
{
	if (!screenflush)
		return;
	screenflush = 0;

	if (!scr_skipupdate)
		qSwapBuffers(maindc);

// handle the mouse state when windowed if that's changed

	IN_UpdateGrabs(modestate != MS_WINDOWED, ActiveApp);
}

void GL_EndRendering (void)
{
	screenflush = true;
	if (!gl_lateswap.value)
		GL_DoSwap();
}

void OblitterateOldGamma(void)
{
	int i;
	if (vid_preservegamma.value)
		return;

	for (i = 0; i < 256; i++)
	{
		originalgammaramps[0][i] = (i<<8) + i;
		originalgammaramps[1][i] = (i<<8) + i;
		originalgammaramps[2][i] = (i<<8) + i;
	}
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

	if (ActiveApp && vid_hardwaregamma.value)	//this is needed because ATI drivers don't work properly (or when task-switched out).
	{
		if (gammaworks)
		{	//we have hardware gamma applied - if we're doing a BF, we don't want to reset to the default gamma (yuck)
			if (vid_desktopgamma.value)
			{
				HDC hDC = GetDC(GetDesktopWindow());
				qSetDeviceGammaRamp (hDC, ramps);
				ReleaseDC(GetDesktopWindow(), hDC);
			}
			else
			{
				qSetDeviceGammaRamp (maindc, ramps);
			}
			return;
		}
	}
}

void GLVID_Crashed(void)
{
	if (qSetDeviceGammaRamp && gammaworks)
	{
		OblitterateOldGamma();
		qSetDeviceGammaRamp(maindc, originalgammaramps);
	}
}

void	GLVID_Shutdown (void)
{
	if (qSetDeviceGammaRamp)
	{
		OblitterateOldGamma();

		if (vid_desktopgamma.value)
		{
			HDC hDC = GetDC(GetDesktopWindow());
			qSetDeviceGammaRamp(hDC, originalgammaramps);
			ReleaseDC(GetDesktopWindow(), hDC);
		}
		else
		{
			qSetDeviceGammaRamp(maindc, originalgammaramps);
		}
	}
	qSetDeviceGammaRamp = NULL;
	qGetDeviceGammaRamp = NULL;

	gammaworks = false;

	VID_UnSetMode();
}


//==========================================================================

#define 	WGL_DRAW_TO_WINDOW_ARB   0x2001
#define 	WGL_SUPPORT_OPENGL_ARB   0x2010
#define 	WGL_ACCELERATION_ARB   0x2003
#define 	WGL_FULL_ACCELERATION_ARB   0x2027
#define 	WGL_COLOR_BITS_ARB   0x2014
#define 	WGL_ALPHA_BITS_ARB   0x201B
#define 	WGL_DEPTH_BITS_ARB   0x2022
#define 	WGL_STENCIL_BITS_ARB   0x2023
#define 	WGL_DOUBLE_BUFFER_ARB   0x2011
qboolean shouldforcepixelformat;
int forcepixelformat;

BOOL CheckForcePixelFormat(rendererstate_t *info)
{
	if (qwglChoosePixelFormatARB && info->multisample)
	{
		HDC hDC;
		int valid;
		float fAttributes[] = {0,0};
		UINT numFormats;
		int pixelformat;
		int iAttributes[] = { WGL_DRAW_TO_WINDOW_ARB,GL_TRUE,
				WGL_SUPPORT_OPENGL_ARB,GL_TRUE,
				WGL_ACCELERATION_ARB,WGL_FULL_ACCELERATION_ARB,
				WGL_COLOR_BITS_ARB,info->bpp,
				WGL_ALPHA_BITS_ARB,8,
				WGL_DEPTH_BITS_ARB,16,
				WGL_STENCIL_BITS_ARB,8,
				WGL_DOUBLE_BUFFER_ARB,GL_TRUE,
				WGL_SAMPLE_BUFFERS_ARB,GL_TRUE,
				WGL_SAMPLES_ARB, info->multisample,						// Check For 4x Multisampling
				0,0};

		TRACE(("dbg: bSetupPixelFormat: attempting wglChoosePixelFormatARB (multisample 4)\n"));
		hDC = GetDC(mainwindow);

		valid = qwglChoosePixelFormatARB(hDC,iAttributes,fAttributes,1,&pixelformat,&numFormats);
		while ((!valid || numFormats < 1) && iAttributes[19] > 1)
		{	//failed, switch wgl_samples to 2
			iAttributes[19] /= 2;
			TRACE(("dbg: bSetupPixelFormat: attempting wglChoosePixelFormatARB (smaller multisample)\n"));
			valid = qwglChoosePixelFormatARB(hDC,iAttributes,fAttributes,1,&pixelformat,&numFormats);
		}

		ReleaseDC(mainwindow, hDC);
		if (valid && numFormats > 0)
		{
			shouldforcepixelformat = true;
			forcepixelformat = pixelformat;
			return true;
		}
	}
	return false;
}

BYTE IntensityFromShifted(unsigned int index, unsigned int shift, unsigned int bits)
{
	unsigned int val;

	val = (index >> shift) & ((1 << bits) - 1);

	switch (bits)
	{
	case 1:
		val = val ? 0xFF : 0;
		break;
	case 2:
		val |= val << 2;
		val |= val << 4;
		break;
	case 3:
		val = val << (8 - bits);
		val |= val >> 3;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		val = val << (8 - bits);
		val |= val >> bits;
		break;
	case 8:
		break;
	default:
		return 0;
	}

	return val;
}

void FixPaletteInDescriptor(HDC hDC, PIXELFORMATDESCRIPTOR *pfd)
{
	LOGPALETTE *ppal;
	HPALETTE hpal;
	int idx, clrs;

	if (pfd->dwFlags & PFD_NEED_PALETTE)
	{
		clrs = 1 << pfd->cColorBits;

		ppal = Z_Malloc(sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * clrs);

		ppal->palVersion = 0x300;
		ppal->palNumEntries = clrs;

		for (idx = 0; idx < clrs; idx++)
		{
			ppal->palPalEntry[idx].peRed = IntensityFromShifted(idx, pfd->cRedShift, pfd->cRedBits);
			ppal->palPalEntry[idx].peGreen = IntensityFromShifted(idx, pfd->cGreenShift, pfd->cGreenBits);
			ppal->palPalEntry[idx].peBlue = IntensityFromShifted(idx, pfd->cBlueShift, pfd->cBlueBits);
			ppal->palPalEntry[idx].peFlags = 0;
		}

		hpal = CreatePalette(ppal);
		SelectPalette(hDC, hpal, FALSE);
		RealizePalette(hDC);
		Z_Free(ppal);
	}
}

BOOL bSetupPixelFormat(HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,				// version number
	PFD_DRAW_TO_WINDOW 		// support window
	|  PFD_SUPPORT_OPENGL 	// support OpenGL
	|  PFD_DOUBLEBUFFER ,	// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	24,				// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,				// no alpha buffer
	0,				// shift bit ignored
	0,				// no accumulation buffer
	0, 0, 0, 0, 			// accum bits ignored
#ifndef RTLIGHTS
	32,				// 32-bit z-buffer
	0,				// 0 stencil, don't need it unless we're using rtlights
#else
	24,				// 24-bit z-buffer
	8,				// stencil buffer
#endif
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
    };
    int pixelformat;

	TRACE(("dbg: bSetupPixelFormat: ChoosePixelFormat\n"));

	if (shouldforcepixelformat && qwglChoosePixelFormatARB)	//the extra && is paranoia
	{
		shouldforcepixelformat = false;
		pixelformat = forcepixelformat;
	}
	else
	{
		if ((pixelformat = ChoosePixelFormat(hDC, &pfd)))
		{
			TRACE(("dbg: ChoosePixelFormat 1: worked\n"));

			if (SetPixelFormat(hDC, pixelformat, &pfd))
			{
				TRACE(("dbg: bSetupPixelFormat: we can use the stencil buffer. woot\n"));
				DescribePixelFormat(hDC, pixelformat, sizeof(pfd), &pfd); 
				FixPaletteInDescriptor(hDC, &pfd);
				gl_canstencil = pfd.cStencilBits;
				return TRUE;
			}
		}
		TRACE(("dbg: ChoosePixelFormat 1: no stencil buffer for us\n"));

		pfd.cStencilBits = 0;
		gl_canstencil = false;

		if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
		{
			Con_Printf("bSetupPixelFormat: ChoosePixelFormat failed (%i)\n", GetLastError());
			return FALSE;
		}
	}

    if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        Con_Printf("bSetupPixelFormat: SetPixelFormat failed (%i)\n", GetLastError());
        return FALSE;
    }

	FixPaletteInDescriptor(hDC, &pfd);
    return TRUE;
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, 0, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

qboolean GLAppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	if (ActiveApp == fActive && Minimized == minimize)
		return false;	//so windows doesn't crash us over and over again.

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	IN_UpdateGrabs(modestate != MS_WINDOWED, ActiveApp);

	if (fActive)
	{
		if (modestate != MS_WINDOWED)
		{
			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);

								// Fix for alt-tab bug in NVidia drivers
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);
			}
		}

		Cvar_ForceCallback(&v_gamma);
	}

	if (!fActive)
	{
		if (modestate != MS_WINDOWED)
		{
			if (vid_canalttab) { 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}

		Cvar_ForceCallback(&v_gamma);	//wham bam thanks.

		if (qSetDeviceGammaRamp)
		{
			OblitterateOldGamma();

			if (vid_desktopgamma.value)
			{
				HDC hDC = GetDC(GetDesktopWindow());
				qSetDeviceGammaRamp (hDC, originalgammaramps);
				ReleaseDC(GetDesktopWindow(), hDC);
			}
			else
			{
				qSetDeviceGammaRamp(maindc, originalgammaramps);
			}
		}
	}

	return true;
}

/* main window procedure */
LONG WINAPI GLMainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    LONG    lRet = 1;
	int		fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

    switch (uMsg)
    {
		case WM_KILLFOCUS:
			GLAppActivate(FALSE, Minimized);
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			ClearAllStates ();
			break;
		case WM_SETFOCUS:
			if (!GLAppActivate(TRUE, Minimized))
				break;
			ClearAllStates ();
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			VID_UpdateWindowStatus (hWnd);
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (!vid_initializing)
			{
				IN_TranslateKeyEvent(wParam, lParam, true);
			}
			break;

//		case WM_UNICHAR:
		case WM_DEADCHAR:
		case WM_SYSDEADCHAR:
		case WM_CHAR:
		case WM_SYSCHAR:
//			if (!vid_initializing)
//				IN_TranslateKeyEvent(wParam, lParam, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (!vid_initializing)
			{
				IN_TranslateKeyEvent(wParam, lParam, false);
			}
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			temp = 0;

			if (wParam & MK_LBUTTON)
			{
				temp |= 1;
#ifdef NPQTV
				SetFocus(hWnd);
#endif
			}

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;

			if (wParam & MK_XBUTTON3)
				temp |= 32;

			if (wParam & MK_XBUTTON4)
				temp |= 64;

			if (wParam & MK_XBUTTON5)
				temp |= 128;

			if (wParam & MK_XBUTTON6)
				temp |= 256;

			if (wParam & MK_XBUTTON7)
				temp |= 512;

			if (!vid_initializing)
				IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if (!vid_initializing)
			{
				if ((short) HIWORD(wParam) > 0)
				{
					Key_Event(K_MWHEELUP, 0, true);
					Key_Event(K_MWHEELUP, 0, false);
				}
				else
				{
					Key_Event(K_MWHEELDOWN, 0, true);
					Key_Event(K_MWHEELDOWN, 0, false);
				}
			}
			break;

		case WM_INPUT:
			// raw input handling
			IN_RawInput_MouseRead((HANDLE)lParam);
			break;

    	case WM_SIZE:
			if (!vid_initializing)
			{
				Cvar_ForceCallback(&vid_conautoscale);
				Cvar_ForceCallback(&vid_conwidth);
				VID_UpdateWindowStatus (hWnd);
			}
            break;

   	    case WM_CLOSE:
			if (!vid_initializing)
				if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
							MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
				{
					Sys_Quit ();
				}

	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			if (!GLAppActivate(!(fActive == WA_INACTIVE), fMinimized))
				break;//so, urm, tell me microsoft, what changed?
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWNORMAL);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

   	    case WM_DESTROY:
        {
			if (dibwindow)
				DestroyWindow (dibwindow);
        }
        break;

		case MM_MCINOTIFY:
            lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

			
		case WM_MWHOOK:
			if (!vid_initializing)
				MW_Hook_Message (lParam);
			break;
		
    	default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
    }

    /* return 1 if handled message, 0 if not */
    return lRet;
}


qboolean GLVID_Is8bit(void) {
	return is8bit;
}


void VID_Init8bitPalette(void) 
{
#ifdef GL_USE8BITTEX
#ifdef GL_EXT_paletted_texture
#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB

	// Check for 8bit Extensions and initialize them.
	int i;
	char thePalette[256*3];
	char *oldPalette, *newPalette;

	qglColorTableEXT = (void *)qwglGetProcAddress("glColorTableEXT");
	if (!qglColorTableEXT || !GL_CheckExtension("GL_EXT_shared_texture_palette") || COM_CheckParm("-no8bit"))
		return;

	Con_SafePrintf("8-bit GL extensions enabled.\n");
	qglEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
	oldPalette = (char *) d_8to24rgbtable; //d_8to24table3dfx;
	newPalette = thePalette;
	for (i=0;i<256;i++)
	{
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		oldPalette++;
	}
	qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE,
		(void *) thePalette);
	is8bit = TRUE;

#endif
#endif
}

void GLVID_DeInit (void)
{
	GLVID_Shutdown();
	ActiveApp = false;

	Cvar_Unhook(&_vid_wait_override);
	Cvar_Unhook(&vid_wndalpha);

	UnregisterClass(WINDOW_CLASS_NAME, global_hInstance);
}
/*
===================
VID_Init
===================
*/
qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
//	qbyte	*ptmp;
	DEVMODE	devmode;
	WNDCLASS wc;

	memset(&devmode, 0, sizeof(devmode));

	/* Register the frame class */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)GLMainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = global_hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass (&wc))	//this isn't really fatal, we'll let the CreateWindow fail instead.
		MessageBox(NULL, "RegisterClass failed", "GAH", 0);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));

	vid_initialized = false;
	vid_initializing = true;

	vid.colormap = host_colormap;

	if (hwnd_dialog)
		DestroyWindow (hwnd_dialog);

	VID_SetPalette (palette);

	if (!GLVID_SetMode (info, palette))
	{
		VID_UnSetMode();
		return false;
	}

	// Check for 3DFX Extensions and initialize them.
	VID_Init8bitPalette();

	vid_canalttab = true;

	S_Restart_f();

	Cvar_Hook(&_vid_wait_override, VID_Wait_Override_Callback);
	Cvar_Hook(&vid_width, VID_Size_Override_Callback);
	Cvar_Hook(&vid_height, VID_Size_Override_Callback);
	Cvar_Hook(&vid_wndalpha, VID_WndAlpha_Override_Callback);

	vid_initialized = true;
	vid_initializing = false;

	return true;
}
#endif
