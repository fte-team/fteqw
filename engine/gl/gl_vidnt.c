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
#include "glquake.h"
#include "winquake.h"
#include "resource.h"
#include <commctrl.h>


#ifndef CDS_FULLSCREEN
#define CDS_FULLSCREEN 4
#endif


#ifndef WM_XBUTTONDOWN
   #define WM_XBUTTONDOWN      0x020B
   #define WM_XBUTTONUP      0x020C
#endif
#ifndef MK_XBUTTON1
   #define MK_XBUTTON1         0x0020
   #define MK_XBUTTON2         0x0040
#endif 





#define WINDOW_CLASS_NAME "WinQuake"

#define MAX_MODE_LIST	128
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;

#ifdef AVAIL_DX7
void D3DInitialize(void);
void d3dSetMode(int fullscreen, int width, int height, int bpp, int zbpp);
#endif
BOOL bSetupPixelFormat(HDC hDC);

//qboolean VID_SetWindowedMode (int modenum);
//qboolean VID_SetFullDIBMode (int modenum);
qboolean VID_SetWindowedMode (rendererstate_t *info);	//-1 on bpp or hz for default.
qboolean VID_SetFullDIBMode (rendererstate_t *info);	//-1 on bpp or hz for default.

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean		DDActive;
qboolean		scr_skipupdate;

int		gl_canstencil;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean	leavecurrentmode= true;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static int		windowed_mouse;
extern qboolean	mouseactive;  // from in_win.c
static HICON	hIcon;
extern qboolean vid_isfullscreen;

unsigned short origionalgammaramps[3][256];

#ifdef SWQUAKE
extern
#endif
		qboolean vid_initializing;

extern cvar_t gl_2dscale;

qboolean VID_AttachGL (rendererstate_t *info);

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow, dibwindow;

unsigned char	vid_curpal[256*3];

float vid_gamma = 1.0;

HGLRC	baseRC;
HDC		maindc;

glvert_t glv;


HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

//unsigned short	d_8to16rgbtable[256];
//unsigned	d_8to24rgbtable[256];
//unsigned short	d_8to16bgrtable[256];
//unsigned	d_8to24bgrtable[256];

float		gldepthmin, gldepthmax;

modestate_t	modestate = MS_UNINIT;


LONG WINAPI GLMainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void GLAppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init(void *(*getglfunction) (char *name));

PROC glArrayElementEXT;
PROC glColorPointerEXT;
PROC glTexCoordPointerEXT;
PROC glVertexPointerEXT;

typedef void (APIENTRY *lp3DFXFUNC) (int, int, int, int, int, const void*);
lp3DFXFUNC glColorTableEXT;
qboolean is8bit = false;
qboolean isPermedia = false;

//====================================
// Note that 0 is MODE_WINDOWED
extern cvar_t	vid_mode;
extern cvar_t		_vid_default_mode;
// Note that 3 is MODE_FULLSCREEN_DEFAULT
extern cvar_t		_vid_default_mode_win;
extern cvar_t		vid_wait;
extern cvar_t		vid_nopageflip;
extern cvar_t		_vid_wait_override;
extern cvar_t		vid_stretch_by_2;
extern cvar_t		_windowed_mouse;
extern cvar_t		vid_hardwaregamma;

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
		return proc;
	}
	return proc;
}
void *getwglfunc(char *name)
{
	FARPROC proc;
	proc = GetProcAddress(hInstGL, name);
	if (!proc)
	{
		if (!hInstwgl)
			hInstwgl = LoadLibrary("opengl32.dll");
		proc = GetProcAddress(hInstwgl, name);
		if (!proc)
			Sys_Error("GL function %s was not found in %s\nPossibly you do not have a full enough gl implementation", name, opengldllname);
	}
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


qboolean GLInitialise (char *renderer)
{
	if (hInstGL)
		FreeModule(hInstGL);
	if (hInstwgl)
		FreeModule(hInstwgl);
	hInstwgl=NULL;

	strcpy(opengldllname, renderer);

	hInstGL = LoadLibrary(opengldllname);
	
	if (!hInstGL)
	{
		Con_SafePrintf ("Couldn't load %s\n", opengldllname);
		return false;
	}

	// windows dependant
	qwglCreateContext		= (void *)getwglfunc("wglCreateContext");
	qwglDeleteContext		= (void *)getwglfunc("wglDeleteContext");
	qwglGetCurrentContext	= (void *)getwglfunc("wglGetCurrentContext");
	qwglGetCurrentDC		= (void *)getwglfunc("wglGetCurrentDC");
	qwglGetProcAddress		= (void *)getwglfunc("wglGetProcAddress");
	qwglMakeCurrent			= (void *)getwglfunc("wglMakeCurrent");
	qSwapBuffers			= SwapBuffers;

	return true;
}

// direct draw software compatability stuff

void GLVID_HandlePause (qboolean pause)
{
}

void GLVID_ForceLockState (int lk)
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

	DIBWidth = info->width;
	DIBHeight = info->height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
				  WS_MINIMIZEBOX;
	ExWindowStyle = 0;

	WindowStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	wwidth = rect.right - rect.left;
	wheight = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 WINDOW_CLASS_NAME,
		 "FTE QuakeWorld",
		 WindowStyle,
		 rect.left, rect.top,
		 wwidth,
		 wheight,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
	{
		Con_Printf ("Couldn't create DIB window");
		return false;
	}

	// Center and show the DIB window
	CenterWindow(dibwindow, WindowRect.right - WindowRect.left,
				 WindowRect.bottom - WindowRect.top, false);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);


	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = Q_atoi(com_argv[i+1]);
	else
	{
		vid.conwidth = 640;
		gl_2dscale.modified = true;
	}

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = Q_atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	if (vid.conheight > info->height)
		vid.conheight = info->height;
	if (vid.conwidth > info->width)
		vid.conwidth = info->width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.numpages = 2;

	mainwindow = dibwindow;
	vid_isfullscreen=false;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
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
		 "FTE QuakeWorld",
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
		vid.conwidth = Q_atoi(com_argv[i+1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = Q_atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	if (vid.conheight > info->height)
		vid.conheight = info->height;
	if (vid.conwidth > info->width)
		vid.conwidth = info->width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

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


int GLVID_SetMode (rendererstate_t *info, unsigned char *palette)
{
	int				temp;
	qboolean		stat;
    MSG				msg;
//	HDC				hdc;

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	// Set either the fullscreen or windowed mode
	if (!info->fullscreen)
	{
		if (_windowed_mouse.value && (key_dest == key_game || key_dest == key_menu))
		{
			stat = VID_SetWindowedMode(info);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(info);
		}
	}
	else
	{
		stat = VID_SetFullDIBMode(info);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
	{
		return false;
	}

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	VID_SetPalette (palette);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	GLVID_SetPalette (palette);

	vid.recalc_refdef = 1;

	maindc = GetDC(mainwindow);
	GetDeviceGammaRamp(maindc, origionalgammaramps);

	if (!VID_AttachGL(info))
		return false;

	return true;
}

void VID_UnSetMode (void)
{
	HGLRC hRC;
   	HDC	  hDC = NULL;

	if (mainwindow && vid_initialized)
	{
		GLAppActivate(false, false);

		vid_canalttab = false;
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

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);
	}

	if (mainwindow)
	{
		dibwindow=NULL;
//		SendMessage(mainwindow, WM_CLOSE, 0, 0);
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
void VID_UpdateWindowStatus (void)
{

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();

	gl_2dscale.modified = true;
}


//====================================

qboolean VID_AttachGL (rendererstate_t *info)
{	//make sure we can get a valid renderer.
	do
	{
#ifdef AVAIL_DX7
		if (!Q_strcasecmp(info->glrenderer, "D3D"))
		{
			extern cvar_t gl_ztrick;
			int zbpp = info->bpp > 16 ? 24 : 16;
			gl_canstencil = false;
			D3DInitialize();	//replacement of GLInitialise, to get the function pointers set up.
			if (COM_CheckParm("-zbpp"))
			{
				zbpp = Q_atoi(com_argv[COM_CheckParm("-zbpp")+1]);
			}
			d3dSetMode(info->fullscreen, info->width, info->height, info->bpp, zbpp);	//d3d cheats to get it's dimensions and stuff... One that we can currently live with though.

			gl_ztrickdisabled |= 2;	//ztrick does funny things.
			Cvar_Set(&gl_ztrick, "0");

			maindc = GetDC(mainwindow);

			Con_SafePrintf(S_COLOR_GREEN"OpenGL to Direct3D wrapper enabled\n");	//green to make it show.
			break;
		}
#endif
		gl_ztrickdisabled &= ~2;
		if (GLInitialise(info->glrenderer))
		{
			maindc = GetDC(mainwindow);
			bSetupPixelFormat(maindc);
			break;
		}

		if (!stricmp(info->glrenderer, "opengl32.dll") || !stricmp(info->glrenderer, "opengl32"))	//go for windows system dir if we failed with the default. Should help to avoid the 3dfx problem.
		{
			char systemgl[MAX_OSPATH+1];
			GetSystemDirectory(systemgl, sizeof(systemgl)-1);
			strncat(systemgl, "\\", sizeof(systemgl)-1);
			strncat(systemgl, info->glrenderer, sizeof(systemgl)-1);
			if (GLInitialise(systemgl))
			{
				maindc = GetDC(mainwindow);
				bSetupPixelFormat(maindc);
				break;
			}
		}

		return false;
	} while(1);
	

    baseRC = qwglCreateContext( maindc );
	if (!baseRC)
	{
		Con_SafePrintf(S_COLOR_RED"Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.\n");	//green to make it show.
		return false;
	}
    if (!qwglMakeCurrent( maindc, baseRC ))
	{
		Con_SafePrintf(S_COLOR_RED"wglMakeCurrent failed\n");	//green to make it show.
		return false;
	}

	GL_Init(getglfunc);
	qwglSwapIntervalEXT		= getglfunc("wglSwapIntervalEXT");
	if (qwglSwapIntervalEXT && _vid_wait_override.value>=0)
		qwglSwapIntervalEXT(_vid_wait_override.value);
	_vid_wait_override.modified = false;
	qSwapBuffers(maindc);

	return true;
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;

//    if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport (*x, *y, *width, *height);
}

void GL_EndRendering (void)
{
	if (!scr_skipupdate || block_drawing)
		qSwapBuffers(maindc);

	if (_vid_wait_override.modified && qwglSwapIntervalEXT && _vid_wait_override.value>=0)
	{
		qwglSwapIntervalEXT(_vid_wait_override.value);
		_vid_wait_override.modified = false;
	}

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if (!_windowed_mouse.value) {
			if (windowed_mouse)	{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !mouseactive && ActiveApp) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else if (mouseactive && key_dest != key_game) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
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

extern gammaworks;

void	GLVID_ShiftPalette (unsigned char *palette)
{
	extern	unsigned short ramps[3][256];
	
//	VID_SetPalette (palette);

	if (ActiveApp && vid_hardwaregamma.value)	//this is needed because ATI drivers don't work properly (or when task-switched out).
		gammaworks = !!SetDeviceGammaRamp (maindc, ramps);
	else
		gammaworks = false;
	if (!gammaworks)
	{
		SetDeviceGammaRamp(maindc, origionalgammaramps);
	}
}


void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}


void	GLVID_Shutdown (void)
{
	SetDeviceGammaRamp(maindc, origionalgammaramps);

	VID_UnSetMode();
}


//==========================================================================


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
	32,				// 32-bit z-buffer	
	8,				// stencil buffer
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
    };
    int pixelformat;

	if ((pixelformat = ChoosePixelFormat(hDC, &pfd)))
		if (SetPixelFormat(hDC, pixelformat, &pfd))
		{
			gl_canstencil = pfd.cStencilBits;
			return TRUE;
		}

	pfd.cStencilBits = 0;
	gl_canstencil = false;

    if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
    {
        MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

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
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

void GLAppActivate(BOOL fActive, BOOL minimize)
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

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);

								// Fix for alt-tab bug in NVidia drivers
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);
			}
			gl_2dscale.modified = true;
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value && (key_dest == key_game || key_dest == key_menu))
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}

		v_gamma.modified = true;	//so that we can start doing palette flashes and things
	}

	if (!fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab) { 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}

		v_gamma.modified = true;	//wham bam thanks.

		SetDeviceGammaRamp(maindc, origionalgammaramps);
	}
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
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (!vid_initializing)
				Key_Event (MapKey(lParam), true);
			break;
			
		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (!vid_initializing)
				Key_Event (MapKey(lParam), false);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
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
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;

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
					Key_Event(K_MWHEELUP, true);
					Key_Event(K_MWHEELUP, false);
				}
				else
				{
					Key_Event(K_MWHEELDOWN, true);
					Key_Event(K_MWHEELDOWN, false);
				}
			}
			break;

    	case WM_SIZE:
			if (!vid_initializing)
			{
				WindowRect.right = ((short*)&lParam)[0] - WindowRect.left;
				WindowRect.bottom = ((short*)&lParam)[1] - WindowRect.top;

				gl_2dscale.modified = true;
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
			GLAppActivate(!(fActive == WA_INACTIVE), fMinimized);
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


qboolean GLVID_Is8bit() {
	return is8bit;
}

#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB

void VID_Init8bitPalette() 
{
	// Check for 8bit Extensions and initialize them.
	int i;
	char thePalette[256*3];
	char *oldPalette, *newPalette;

	glColorTableEXT = (void *)qwglGetProcAddress("glColorTableEXT");
    if (!glColorTableEXT || strstr(gl_extensions, "GL_EXT_shared_texture_palette") ||
		COM_CheckParm("-no8bit"))
		return;

	Con_SafePrintf("8-bit GL extensions enabled.\n");
    glEnable( GL_SHARED_TEXTURE_PALETTE_EXT );
	oldPalette = (char *) d_8to24rgbtable; //d_8to24table3dfx;
	newPalette = thePalette;
	for (i=0;i<256;i++) {
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		oldPalette++;
	}
	glColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE,
		(void *) thePalette);
	is8bit = TRUE;
}

static void Check_Gamma (unsigned char *pal, float usegammaval)
{
//	float	f, inf;
//	unsigned char	palette[768];
//	int		i;
/*
	if (usegammaval)
		vid_gamma = usegammaval;
	else if ((i = COM_CheckParm("-gamma")) == 0) {
		if ((gl_renderer && strstr(gl_renderer, "Voodoo")) ||
			(gl_vendor && strstr(gl_vendor, "3Dfx")))
			vid_gamma = 1;
		else
			vid_gamma = 0.7; // default to 0.7 on non-3dfx hardware
	} else
		vid_gamma = Q_atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
	*/
}


void GLVID_DeInit (void)
{
	GLVID_Shutdown();

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

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON2));

	vid_initialized = false;
	vid_initializing = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;

	if (hwnd_dialog)
		DestroyWindow (hwnd_dialog);

	Check_Gamma(palette, 0);
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

	vid_initialized = true;
	vid_initializing = false;

	return true;
}
