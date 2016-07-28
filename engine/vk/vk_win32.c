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
// vk_win32.c -- win32-specific window handling for vulkan.
//I should probably merge this with gl_vidnt.c somehow... might get messy though

#include "quakedef.h"
#ifdef VKQUAKE
#include "glquake.h"
#include "vkrenderer.h"
#include "winquake.h"
#include "resource.h"
#include "shader.h"
#include <commctrl.h>

void STT_Event(void);

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

#define WINDOW_CLASS_NAME_W L"FTEVkQuake"
#define WINDOW_CLASS_NAME_A "FTEVkQuake"

extern cvar_t vid_width;
extern cvar_t vid_height;
extern cvar_t vid_wndalpha;
extern qboolean gammaworks;

typedef enum {MS_WINDOWED, MS_FULLDIB, MS_FULLWINDOW, MS_UNINIT} modestate_t;

//qboolean VID_SetWindowedMode (int modenum);
//qboolean VID_SetFullDIBMode (int modenum);
static qboolean VID_SetWindowedMode (rendererstate_t *info);	//-1 on bpp or hz for default.
static qboolean VID_SetFullDIBMode (rendererstate_t *info);	//-1 on bpp or hz for default.

qboolean		scr_skipupdate;

#ifdef MULTITHREAD
#define WTHREAD	//While the user is resizing a window, the entire thread that owns said window becomes frozen. in order to cope with window resizing, its easiest to just create a separate thread to be microsoft's plaything. our main game thread can then just keep rendering. hopefully that won't bug out on the present.
#endif
#ifdef WTHREAD
static HANDLE	windowthread;
#endif

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
extern qboolean	mouseactive;  // from in_win.c
static HICON	hIcon;
extern qboolean vid_isfullscreen;

unsigned short originalgammaramps[3][256];

qboolean vid_initializing;

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

//unsigned short	d_8to16rgbtable[256];
//unsigned	d_8to24rgbtable[256];
//unsigned short	d_8to16bgrtable[256];
//unsigned	d_8to24bgrtable[256];

static modestate_t	modestate = MS_UNINIT;

extern float gammapending;


static LONG WINAPI VKMainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static qboolean VKAppActivate(BOOL fActive, BOOL minimize);
static void ClearAllStates (void);
static void VID_UpdateWindowStatus (HWND hWnd);

//====================================
// Note that 0 is MODE_WINDOWED
extern cvar_t	vid_mode;
// Note that 3 is MODE_FULLSCREEN_DEFAULT
extern cvar_t		vid_vsync;
extern cvar_t		_windowed_mouse;
extern cvar_t		vid_hardwaregamma;
extern cvar_t		vid_desktopgamma;
extern cvar_t		vid_preservegamma;

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

static dllhandle_t *hInstVulkan = NULL;

/*doesn't consider parent offsets*/
static RECT centerrect(unsigned int parentleft, unsigned int parenttop, unsigned int parentwidth, unsigned int parentheight, unsigned int cwidth, unsigned int cheight)
{
	RECT r;
	if (modestate!=MS_WINDOWED)
	{
		if (!vid_width.ival)
			cwidth = parentwidth;
		if (!vid_height.ival)
			cheight = parentwidth;
	}

	if (parentwidth < cwidth)
	{
		r.left = parentleft;
		r.right = r.left+parentwidth;
	}
	else
	{
		r.left = parentleft + (parentwidth - cwidth) / 2;
		r.right = r.left + cwidth;
	}

	if (parentheight < cheight)
	{
		r.top = parenttop;
		r.bottom = r.top + parentheight;
	}
	else
	{
		r.top = parenttop + (parentheight - cheight) / 2;
		r.bottom = r.top + cheight;
	}

	return r;
}

static qboolean VID_SetWindowedMode (rendererstate_t *info)
{
	int i;
	HDC				hdc;
	int				wwidth, wheight, pleft, ptop, pwidth, pheight;
	RECT			rect;

	modestate = MS_WINDOWED;

	hdc = GetDC(NULL);
	if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
	{
		ReleaseDC(NULL, hdc);
		Con_Printf("Can't run GL in non-RGB mode\n");
		return false;
	}
	ReleaseDC(NULL, hdc);

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = info->width;
	WindowRect.bottom = info->height;


#ifndef FTE_SDL
	if (sys_parentwindow)
	{
		SetWindowLong(sys_parentwindow, GWL_STYLE, GetWindowLong(sys_parentwindow, GWL_STYLE)|WS_OVERLAPPED);
		WindowStyle = WS_CHILDWINDOW|WS_OVERLAPPED;
		ExWindowStyle = 0;

		pleft = sys_parentleft;
		ptop = sys_parenttop;
		pwidth = sys_parentwidth;
		pheight = sys_parentheight;

		WindowRect.right = sys_parentwidth;
		WindowRect.bottom = sys_parentheight;
	}
	else
#endif
	{
		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
					  WS_MINIMIZEBOX;
		ExWindowStyle = 0;

		WindowStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;

		pleft = 0;
		ptop = 0;
		pwidth = GetSystemMetrics(SM_CXSCREEN);
		pheight = GetSystemMetrics(SM_CYSCREEN);

		/*Assume dual monitors, and chop the width to try to put it on only one screen*/
		if (pwidth >= pheight*2)
			pwidth /= 2;
	}

	DIBWidth = WindowRect.right - WindowRect.left;
	DIBHeight = WindowRect.bottom - WindowRect.top;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	wwidth = rect.right - rect.left;
	wheight = rect.bottom - rect.top;

	WindowRect = centerrect(pleft, ptop, pwidth, pheight, wwidth, wheight);

	// Create the DIB window
	if (WinNT)
	{
		mainwindow = CreateWindowExW (
			 ExWindowStyle,
			 WINDOW_CLASS_NAME_W,
			 _L(FULLENGINENAME),
			 WindowStyle,
			 WindowRect.left, WindowRect.top,
			 WindowRect.right - WindowRect.left,
			 WindowRect.bottom - WindowRect.top,
			 sys_parentwindow,
			 NULL,
			 global_hInstance,
			 NULL);
	}
	else
	{
		mainwindow = CreateWindowExA (
			 ExWindowStyle,
			 WINDOW_CLASS_NAME_A,
			 FULLENGINENAME,
			 WindowStyle,
			 WindowRect.left, WindowRect.top,
			 WindowRect.right - WindowRect.left,
			 WindowRect.bottom - WindowRect.top,
			 sys_parentwindow,
			 NULL,
			 global_hInstance,
			 NULL);
	}

	if (!mainwindow)
	{
		Con_Printf ("Couldn't create DIB window");
		return false;
	}

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	if (!sys_parentwindow)
	{
#ifdef WS_EX_LAYERED
		int av;
		av = 255*vid_wndalpha.value;
		if (av < 70)
			av = 70;
		if (av < 255)
		{
			HMODULE hm = GetModuleHandleA("user32.dll");
			lpfnSetLayeredWindowAttributes pSetLayeredWindowAttributes;
			pSetLayeredWindowAttributes = (void*)GetProcAddress(hm, "SetLayeredWindowAttributes");

			if (pSetLayeredWindowAttributes)
			{
				// Set WS_EX_LAYERED on this window
				SetWindowLong(mainwindow, GWL_EXSTYLE, GetWindowLong(mainwindow, GWL_EXSTYLE) | WS_EX_LAYERED);

				// Make this window 70% alpha
				pSetLayeredWindowAttributes(mainwindow, 0, (BYTE)av, LWA_ALPHA);
			}
		}
#endif
	}

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	SetFocus(mainwindow);

//	ShowWindow (mainwindow, SW_SHOWDEFAULT);
//	UpdateWindow (mainwindow);

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(mainwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(mainwindow, hdc);

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

	vid_isfullscreen=false;

	CL_UpdateWindowTitle();

	return true;
}

static void VKVID_SetWindowCaption(const char *text)
{
	wchar_t wide[2048];
	widen(wide, sizeof(wide), text);
	SetWindowTextW(mainwindow, wide);
}


static qboolean VID_SetFullDIBMode (rendererstate_t *info)
{
	int i;
	HDC				hdc;
	int				wwidth, wheight;
	RECT			rect;

	if (info->fullscreen != 2)
	{	//make windows change res.

		modestate = MS_FULLDIB;

		gdevmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
		if (info->bpp)
			gdevmode.dmFields |= DM_BITSPERPEL;
		if (info->rate)
			gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
		gdevmode.dmBitsPerPel = info->bpp;
		if (info->bpp && (gdevmode.dmBitsPerPel < 15))
		{
			Con_Printf("Forcing at least 15bpp\n");
			gdevmode.dmBitsPerPel = 16;
		}
		gdevmode.dmDisplayFrequency = info->rate;
		gdevmode.dmPelsWidth = info->width;
		gdevmode.dmPelsHeight = info->height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			Con_SafePrintf((gdevmode.dmFields&DM_DISPLAYFREQUENCY)?"Windows rejected mode %i*%i*%ibpp@%ihz\n":"Windows rejected mode %i*%i*%ibpp\n", (int)gdevmode.dmPelsWidth, (int)gdevmode.dmPelsHeight, (int)gdevmode.dmBitsPerPel, (int)gdevmode.dmDisplayFrequency);
			return false;
		}
	}
	else
	{
		modestate = MS_FULLWINDOW;
		
	}

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
	if(WinNT)
	{
		mainwindow = CreateWindowExW (
			ExWindowStyle,
			WINDOW_CLASS_NAME_W,
			_L(FULLENGINENAME),
			WindowStyle,
			rect.left, rect.top,
			wwidth,
			wheight,
			NULL,
			NULL,
			global_hInstance,
			NULL);
	}
	else
	{
		mainwindow = CreateWindowExA (
			ExWindowStyle,
			WINDOW_CLASS_NAME_A,
			FULLENGINENAME,
			WindowStyle,
			rect.left, rect.top,
			wwidth,
			wheight,
			NULL,
			NULL,
			global_hInstance,
			NULL);
	}

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window");

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	if (modestate == MS_FULLWINDOW)
		ShowWindow (mainwindow, SW_SHOWMAXIMIZED);
	else
		ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(mainwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(mainwindow, hdc);


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

	return true;
}

static void Win_Touch_Init(HWND wnd);
static qboolean CreateMainWindow(rendererstate_t *info)
{
	qboolean		stat;
	if (WinNT)
	{
		WNDCLASSW		wc;
		/* Register the frame class */
		wc.style         = CS_OWNDC;
		wc.lpfnWndProc   = (WNDPROC)VKMainWndProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = global_hInstance;
		wc.hIcon         = hIcon;
		wc.hCursor       = hArrowCursor;
		wc.hbrBackground = NULL;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = WINDOW_CLASS_NAME_W;
		if (!RegisterClassW (&wc))	//this isn't really fatal, we'll let the CreateWindow fail instead.
			Con_Printf("RegisterClass failed\n");
	}
	else
	{
		WNDCLASSA		wc;
		/* Register the frame class */
		wc.style         = CS_OWNDC;
		wc.lpfnWndProc   = (WNDPROC)VKMainWndProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = global_hInstance;
		wc.hIcon         = hIcon;
		wc.hCursor       = hArrowCursor;
		wc.hbrBackground = NULL;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = WINDOW_CLASS_NAME_A;
		if (!RegisterClassA (&wc))	//this isn't really fatal, we'll let the CreateWindow fail instead.
			Con_Printf("RegisterClass failed\n");
	}

	if (!info->fullscreen)
	{
		stat = VID_SetWindowedMode(info);
	}
	else
	{
		stat = VID_SetFullDIBMode(info);
	}
	VID_UpdateWindowStatus(mainwindow);

	Win_Touch_Init(mainwindow);

	INS_UpdateGrabs(info->fullscreen, vid.activeapp);

	return stat;
}

#ifdef WTHREAD
static rendererstate_t *rs;
static int VKVID_WindowThread(void *cond)
{
	extern qboolean mouseshowtoggle;
	int cursor = 1;
	MSG		msg;
	HWND wnd;
	CreateMainWindow(rs);
	wnd = mainwindow;
	Sys_ConditionSignal(cond);

	while (GetMessageW(&msg, NULL, 0, 0))
	{
//		TranslateMessageW (&msg);
		DispatchMessageW (&msg);

		//ShowCursor is thread-local.
		if (cursor != mouseshowtoggle)
		{
			cursor = mouseshowtoggle;
			ShowCursor(cursor);
		}
	}
	DestroyWindow(wnd);
	return 0;
}
#endif

static qboolean VK_CreateSurface(void)
{
	VkResult err;
	VkWin32SurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
	createInfo.flags = 0;
	createInfo.hinstance = GetModuleHandle(NULL);
	createInfo.hwnd = mainwindow;

	err = vkCreateWin32SurfaceKHR(vk.instance, &createInfo, NULL, &vk.surface);
	switch(err)
	{
	default:
		Con_Printf("Unknown vulkan device creation error: %x\n", err);
		return false;
	case VK_SUCCESS:
		break;
	}
	return true;
}

static qboolean VID_AttachVulkan (rendererstate_t *info)
{	//make sure we can get a valid renderer.
#ifdef VK_NO_PROTOTYPES
	hInstVulkan = NULL;
	if (!hInstVulkan)
		hInstVulkan = *info->subrenderer?LoadLibrary(info->subrenderer):NULL;
	if (!hInstVulkan)
		hInstVulkan = LoadLibrary("vulkan-1.dll");
	if (!hInstVulkan)
	{
		Con_Printf("Unable to load vulkan-1.dll\nNo Vulkan drivers are installed\n");
		return false;
	}
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) GetProcAddress(hInstVulkan, "vkGetInstanceProcAddr");
#endif

	return VK_Init(info, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_CreateSurface);
}


static void VID_UnSetMode (void);
static int VKVID_SetMode (rendererstate_t *info, unsigned char *palette)
{
	int				temp;
	qboolean		stat;
#ifdef WTHREAD
	void			*cond;
#endif
#ifndef NPFTE
    MSG				msg;
#endif
//	HDC				hdc;

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

#ifdef WTHREAD
	cond = Sys_CreateConditional();
	Sys_LockConditional(cond);
	rs = info;
	windowthread = Sys_CreateThread("windowthread", VKVID_WindowThread, cond, 0, 0);
	if (!Sys_ConditionWait(cond))
		Con_SafePrintf ("Looks like the window thread isn't starting up\n");
	Sys_UnlockConditional(cond);
	Sys_DestroyConditional(cond);

	stat = !!mainwindow;
#else
	stat = CreateMainWindow(info);
#endif

	if (stat)
	{
		stat = VID_AttachVulkan(info);
		if (!stat)
			return false;
	}

	if (!stat)
		return false;

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

#ifndef NPFTE
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

	if (vid_desktopgamma.value)
	{
		HDC hDC = GetDC(GetDesktopWindow());
		gammaworks = GetDeviceGammaRamp(hDC, originalgammaramps);
		ReleaseDC(GetDesktopWindow(), hDC);
	}
	else
	{
		HDC hDC = GetDC(mainwindow);
		gammaworks = GetDeviceGammaRamp(hDC, originalgammaramps);
		ReleaseDC(mainwindow, hDC);
	}

	return true;
}

static void VID_UnSetMode (void)
{
	if (mainwindow && vid_initialized)
	{
		VKAppActivate(false, false);

		vid_canalttab = false;
		VK_Shutdown();

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);
	}

	if (mainwindow)
	{
	//	ShowWindow(mainwindow, SW_HIDE);
	//	SetWindowLongPtr(mainwindow, GWL_WNDPROC, DefWindowProc);
	//	PostMessage(mainwindow, WM_CLOSE, 0, 0);
#ifdef WTHREAD
		if (windowthread)
		{
			SendMessage(mainwindow, WM_USER+4, 0, 0);
			Sys_WaitOnThread(windowthread);
			windowthread = NULL;
		}
		else
#endif
			DestroyWindow(mainwindow);
		mainwindow = NULL;
	}

	if (hInstVulkan)
		FreeLibrary(hInstVulkan);
	hInstVulkan = NULL;
}


/*
================
VID_UpdateWindowStatus
================
*/
static void VID_UpdateWindowStatus (HWND hWnd)
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
	if (vid.pixelwidth != window_width || vid.pixelheight != window_height)
		vk.neednewswapchain = true;

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	INS_UpdateClipCursor ();
}

//====================================

static void QDECL VID_Wait_Override_Callback(struct cvar_s *var, char *oldvalue)
{
	qboolean want = !!atoi(oldvalue);
	if (vk.vsync != want)
	{
		vk.vsync = want;
		vk.neednewswapchain = true;
	}
}

static void VKVID_Recenter_f(void)
{
	// 4 unused variables
	//int nw = vid_width.value;
	//int nh = vid_height.value;
	//int nx = 0;
	//int ny = 0;

#ifdef _MSC_VER
#define strtoull _strtoui64
#endif

	if (Cmd_Argc() > 1)
		sys_parentleft = atoi(Cmd_Argv(1));
	if (Cmd_Argc() > 2)
		sys_parenttop = atoi(Cmd_Argv(2));
	if (Cmd_Argc() > 3)
		sys_parentwidth = atoi(Cmd_Argv(3));
	if (Cmd_Argc() > 4)
		sys_parentheight = atoi(Cmd_Argv(4));
	if (Cmd_Argc() > 5)
	{
		HWND newparent = (HWND)(DWORD_PTR)strtoull(Cmd_Argv(5), NULL, 16);
		if (newparent != sys_parentwindow && mainwindow && modestate==MS_WINDOWED)
			SetParent(mainwindow, sys_parentwindow);
		sys_parentwindow = newparent;
	}

	if (sys_parentwindow && modestate==MS_WINDOWED)
	{
		WindowRect = centerrect(sys_parentleft, sys_parenttop, sys_parentwidth, sys_parentheight, sys_parentwidth, sys_parentheight);
		MoveWindow(mainwindow, WindowRect.left, WindowRect.top, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, FALSE);

		VID_UpdateWindowStatus (mainwindow);
	}
}

static void QDECL VID_WndAlpha_Override_Callback(struct cvar_s *var, char *oldvalue)
{
	//this code tells windows to use the alpha channel of the screen, but does really nasty things with the mouse such that its unplayable.
	//its not useful.
/*	if (modestate==MS_WINDOWED)
	{
		struct qDWM_BLURBEHIND 
		{
			  DWORD dwFlags;
			  BOOL  fEnable;
			  HRGN  hRgnBlur;
			  BOOL  fTransitionOnMaximized;
		} bb = {1, true, NULL, true};
		HRESULT (WINAPI *pDwmEnableBlurBehindWindow)(HWND hWnd,const struct qDWM_BLURBEHIND *pBlurBehind);
		dllfunction_t dwm[] =
		{
			{(void*)&pDwmEnableBlurBehindWindow, "DwmEnableBlurBehindWindow"},
			{NULL,NULL}
		};
		if (Sys_LoadLibrary("dwmapi.dll", dwm))
			pDwmEnableBlurBehindWindow(mainwindow, &bb);
	}
*/

#ifdef WS_EX_LAYERED
	//enable whole-window fixed transparency. should work in win2k+
	//note that this can destroy framerates, and they won't reset when the setting is reverted to 1.
	//be prepared to do a vid_restart.
	if (modestate==MS_WINDOWED)
	{
		int av;
		HMODULE hm = GetModuleHandleA("user32.dll");
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
			{
				SetWindowLong(mainwindow, GWL_EXSTYLE, GetWindowLong(mainwindow, GWL_EXSTYLE) & ~WS_EX_LAYERED);
				pSetLayeredWindowAttributes(mainwindow, 0, (BYTE)255, LWA_ALPHA);
			}
		}
	}
#endif
}

static void VKVID_SwapBuffers (void)
{
// handle the mouse state when windowed if that's changed

	INS_UpdateGrabs(modestate != MS_WINDOWED, vid.activeapp);
}

static void OblitterateOldGamma(void)
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

static qboolean VKVID_ApplyGammaRamps (unsigned int gammarampsize, unsigned short *ramps)
{
	if (ramps)
	{
		if (!gammaworks || gammarampsize != 256)
			return false;

		if (vid_hardwaregamma.value == 1 && modestate == MS_WINDOWED)
			return false;	//don't do hardware gamma in windowed mode

		if (vid.activeapp && vid_hardwaregamma.value)	//this is needed because ATI drivers don't work properly (or when task-switched out).
		{
			if (gammaworks)
			{	//we have hardware gamma applied - if we're doing a BF, we don't want to reset to the default gamma (yuck)
				if (vid_desktopgamma.value)
				{
					HDC hDC = GetDC(GetDesktopWindow());
					SetDeviceGammaRamp (hDC, ramps);
					ReleaseDC(GetDesktopWindow(), hDC);
				}
				else
				{
					HDC hDC = GetDC(mainwindow);
					SetDeviceGammaRamp (hDC, ramps);
					ReleaseDC(mainwindow, hDC);
				}
			}
			return true;
		}
		return false;
	}
	else
	{
		if (vid_initialized)
		{
			//revert to default
			OblitterateOldGamma();

			if (vid_desktopgamma.value)
			{
				HDC hDC = GetDC(GetDesktopWindow());
				SetDeviceGammaRamp (hDC, originalgammaramps);
				ReleaseDC(GetDesktopWindow(), hDC);
			}
			else
			{
				HDC hDC = GetDC(mainwindow);
				SetDeviceGammaRamp (hDC, originalgammaramps);
				ReleaseDC(mainwindow, hDC);
			}
		}
		return true;
	}
}

void VKVID_Crashed(void)
{
	if (vid_initialized && gammaworks)
	{
		OblitterateOldGamma();

		if (vid_desktopgamma.value)
		{
			HDC hDC = GetDC(GetDesktopWindow());
			SetDeviceGammaRamp (hDC, originalgammaramps);
			ReleaseDC(GetDesktopWindow(), hDC);
		}
		else
		{
			HDC hDC = GetDC(mainwindow);
			SetDeviceGammaRamp (hDC, originalgammaramps);
			ReleaseDC(mainwindow, hDC);
		}
	}
}

static void	VKVID_Shutdown (void)
{
	if (vid_initialized)
	{
		OblitterateOldGamma();

		if (vid_desktopgamma.value)
		{
			HDC hDC = GetDC(GetDesktopWindow());
			SetDeviceGammaRamp(hDC, originalgammaramps);
			ReleaseDC(GetDesktopWindow(), hDC);
		}
		else
		{
			HDC hDC = GetDC(mainwindow);
			SetDeviceGammaRamp(hDC, originalgammaramps);
			ReleaseDC(mainwindow, hDC);
		}
	}

	gammaworks = false;

	VID_UnSetMode();
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
static void ClearAllStates (void)
{
	int		i;

// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (0, i, 0, false);
	}

	Key_ClearStates ();
	INS_ClearStates ();
}

static qboolean VKAppActivate(BOOL fActive, BOOL minimize)
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

	if (vid.activeapp == fActive && Minimized == minimize)
		return false;	//so windows doesn't crash us over and over again.

	vid.activeapp = fActive;// && (foregroundwindow==mainwindow);
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!vid.activeapp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (vid.activeapp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	INS_UpdateGrabs(modestate != MS_WINDOWED, vid.activeapp);

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			if (vid_canalttab && vid_wassuspended)
			{
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);

								// Fix for alt-tab bug in NVidia drivers
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);
			}
		}
		else if (modestate == MS_FULLWINDOW)
		{
			ShowWindow (mainwindow, SW_SHOWMAXIMIZED);
			UpdateWindow (mainwindow);
		}

		gammapending = 0.5;				//delayed gamma force
		Cvar_ForceCallback(&v_gamma);	//so the delay isn't so blatent when you have decent graphics drivers that don't break things.
	}

	if (!fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			if (vid_canalttab)
			{
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}

		Cvar_ForceCallback(&v_gamma);	//wham bam thanks.
	}

	return true;
}

#ifndef TWF_WANTPALM
typedef struct _TOUCHINPUT {
  LONG      x;
  LONG      y;
  HANDLE    hSource;
  DWORD     dwID;
  DWORD     dwFlags;
  DWORD     dwMask;
  DWORD     dwTime;
  ULONG_PTR dwExtraInfo;
  DWORD     cxContact;
  DWORD     cyContact;
} TOUCHINPUT, *PTOUCHINPUT;
DECLARE_HANDLE(HTOUCHINPUT);

#define WM_TOUCH					0x0240 
#define TOUCHINPUTMASKF_CONTACTAREA	0x0004
#define TOUCHEVENTF_DOWN			0x0002
#define TOUCHEVENTF_UP				0x0004
#define TWF_WANTPALM				0x00000002
#endif

static BOOL (WINAPI *pRegisterTouchWindow)(HWND hWnd, ULONG ulFlags);
static BOOL (WINAPI *pGetTouchInputInfo)(HTOUCHINPUT hTouchInput, UINT cInputs, PTOUCHINPUT pInputs, int cbSize);
static BOOL (WINAPI *pCloseTouchInputHandle)(HTOUCHINPUT hTouchInput);
static void Win_Touch_Init(HWND wnd)
{
	HMODULE lib;
	lib = LoadLibraryA("user32.dll");
	pRegisterTouchWindow = (void*)GetProcAddress(lib, "RegisterTouchWindow");
	pGetTouchInputInfo = (void*)GetProcAddress(lib, "GetTouchInputInfo");
	pCloseTouchInputHandle = (void*)GetProcAddress(lib, "CloseTouchInputHandle");

	if (pRegisterTouchWindow && pGetTouchInputInfo && pCloseTouchInputHandle)
		pRegisterTouchWindow(wnd, TWF_WANTPALM);
}
static void Win_Touch_Event(int points, HTOUCHINPUT ti)
{
	float sz;
	int i;
	TOUCHINPUT *inputs = malloc(points * sizeof(*inputs)), *input;
	if (inputs)
	{
		if (pGetTouchInputInfo(ti, points, inputs, sizeof(*inputs)))
		{
			for (i = 0, input = inputs; i < points; i++, input++)
			{
				int id = input->dwID+1;	//googling implies the id is generally a low 0-based index. I can't test this. the +1 ensures that mouselook is not broken by someone trying to use a touchscreen at the same time.
				if (input->dwMask & TOUCHINPUTMASKF_CONTACTAREA)
					sz = sqrt((input->cxContact*input->cxContact + input->cyContact*input->cyContact) / 10000.0);
				else
					sz = 0;

				//the web seems to imply that the ids should be low values, <16 or so. hurrah.

				//movement *then* buttons. this should ensure that the cursor is positioned correctly.
				IN_MouseMove(id, true, input->x/100.0f, input->y/100.0f, 0, sz);

				if (input->dwFlags & TOUCHEVENTF_DOWN)
					IN_KeyEvent(id, true, K_MOUSE1, 0);
				if (input->dwFlags & TOUCHEVENTF_UP)
					IN_KeyEvent(id, false, K_MOUSE1, 0);
			}
		}
		free(inputs);
	}

	pCloseTouchInputHandle(ti);
}

#ifdef WTHREAD
static void MainThreadWndProc(void *ctx, void *data, size_t msg, size_t ex)
{
	switch(msg)
	{
	case WM_COPYDATA:
		Host_RunFile(data, ex, NULL);
		Z_Free(data);
		break;
	case WM_CLOSE:
		Cbuf_AddText("\nquit\n", RESTRICT_LOCAL);
		break;
	case WM_SIZE:
	case WM_MOVE:
		VID_UpdateWindowStatus(mainwindow);
		break;
	case WM_KILLFOCUS:
		VKAppActivate(FALSE, Minimized);
		if (modestate == MS_FULLDIB)
			ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
		ClearAllStates ();
		break;
	case WM_SETFOCUS:
		if (!VKAppActivate(TRUE, Minimized))
			break;
		ClearAllStates ();
		break;
	}
}
#endif

/* main window procedure
due to moving the main window over to a different thread, we gain access to input timestamps (as well as video refreshes when dragging etc)
however, we have to tread carefully. the main/render thread will be running the whole time, and may trigger messages that we need to respond to _now_.
this means that the main and window thread cannot be allowed to contest any mutexes where anything but memory is touched before its unlocked.
(or in other words, we can't have the main thread near-perma-lock any mutexes that can be locked-to-sync here)
*/
static LONG WINAPI VKMainWndProc (
	HWND	hWnd,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam)
{
	LONG	lRet = 1;
//	int		fActive, fMinimized;
	int 	temp;
	extern unsigned int uiWheelMessage;

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

	switch (uMsg)
	{
		case WM_COPYDATA:
			{
				COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lParam;
#ifdef WTHREAD
				COM_AddWork(WG_MAIN, MainThreadWndProc, NULL, memcpy(Z_Malloc(cds->cbData), cds->lpData, cds->cbData), uMsg, cds->cbData);
#else
				Host_RunFile(cds->lpData, cds->cbData, NULL);
#endif
				lRet = 1;
			}
			break;
		case WM_KILLFOCUS:
#ifdef WTHREAD
			COM_AddWork(WG_MAIN, MainThreadWndProc, NULL, NULL, uMsg, 0);
#else
			VKAppActivate(FALSE, Minimized);//FIXME: thread
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			ClearAllStates ();	//FIXME: thread
#endif
			break;
		case WM_SETFOCUS:
#ifdef WTHREAD
			COM_AddWork(WG_MAIN, MainThreadWndProc, NULL, NULL, uMsg, 0);
#else
			if (!VKAppActivate(TRUE, Minimized))//FIXME: thread
				break;
			ClearAllStates ();	//FIXME: thread
#endif
			break;

		case WM_TOUCH:
			Win_Touch_Event(LOWORD(wParam), (HTOUCHINPUT)lParam);
			return 0;	//return 0 if we handled it.

		case WM_CREATE:
			break;

		case WM_MOVE:
#ifdef WTHREAD
			COM_AddWork(WG_MAIN, MainThreadWndProc, NULL, NULL, uMsg, 0);
#else
			VID_UpdateWindowStatus (hWnd);
#endif
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (!vid_initializing)
				INS_TranslateKeyEvent(wParam, lParam, true, 0, false);
			break;

//		case WM_UNICHAR:
		case WM_DEADCHAR:
		case WM_SYSDEADCHAR:
		case WM_CHAR:
		case WM_SYSCHAR:
//			if (!vid_initializing)
//				INS_TranslateKeyEvent(wParam, lParam, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (!vid_initializing)
				INS_TranslateKeyEvent(wParam, lParam, false, 0, false);
			break;

		case WM_MOUSEACTIVATE:
			lRet = MA_ACTIVATEANDEAT;
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
				if (sys_parentwindow && modestate == MS_WINDOWED)
					SetFocus(hWnd);
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
				INS_MouseEvent (temp);	//FIXME: thread (halflife)

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL:
			if (!vid_initializing)
			{
				if ((short) HIWORD(wParam&0xffffffff) > 0)
				{
					IN_KeyEvent(0, true, K_MWHEELUP, 0);
					IN_KeyEvent(0, false, K_MWHEELUP, 0);
				}
				else
				{
					IN_KeyEvent(0, true, K_MWHEELDOWN, 0);
					IN_KeyEvent(0, false, K_MWHEELDOWN, 0);
				}
			}
			break;

		case WM_INPUT:
			// raw input handling
			if (!vid_initializing)
			{
				INS_RawInput_Read((HANDLE)lParam);
				lRet = 0;
			}
			break;

		case WM_USER+4:
			PostQuitMessage(0);
			break;
		case WM_USER:
#ifndef NOMEDIA
			STT_Event();
#endif
			break;

		case WM_GETMINMAXINFO:
			{
				RECT windowrect;
				RECT clientrect;
				MINMAXINFO *mmi = (MINMAXINFO *) lParam;

				GetWindowRect (hWnd, &windowrect);
				GetClientRect (hWnd, &clientrect);

				mmi->ptMinTrackSize.x = 320 + ((windowrect.right - windowrect.left) - (clientrect.right - clientrect.left));
				mmi->ptMinTrackSize.y = 200 + ((windowrect.bottom - windowrect.top) - (clientrect.bottom - clientrect.top));
			}
			return 0;
		case WM_SIZE:
			vid.isminimized  = (wParam==SIZE_MINIMIZED);
			if (!vid_initializing)
			{
#ifdef WTHREAD
				COM_AddWork(WG_MAIN, MainThreadWndProc, NULL, NULL, uMsg, 0);
#else
				VID_UpdateWindowStatus(hWnd);
#endif
			}
			break;

		case WM_CLOSE:
			if (!vid_initializing)
			{
				if (wantquit)
				{
					//urr, this would be the second time that they've told us to quit.
					//assume the main thread has deadlocked
					if (MessageBoxW (hWnd, L"Terminate process?", L"Confirm Exit",
							MB_YESNO | MB_SETFOREGROUND | MB_ICONEXCLAMATION | MB_DEFBUTTON2) == IDYES)
					{
						//abrupt process termination is never nice, but sometimes drivers suck.
						//or qc code runs away, or ...
						exit(1);
					}
				}

				else if (MessageBoxW (hWnd, L"Are you sure you want to quit?", L"Confirm Exit",
							MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION|MB_DEFBUTTON2) == IDYES)
				{
#ifdef WTHREAD
					COM_AddWork(WG_MAIN, MainThreadWndProc, NULL, NULL, uMsg, 0);
#else
					Cbuf_AddText("\nquit\n", RESTRICT_LOCAL);
#endif
					wantquit = true;
				}
			}
			break;

		case WM_ERASEBKGND:
			lRet = TRUE;
			break;
/*
		case WM_ACTIVATE:
//			fActive = LOWORD(wParam);
//			fMinimized = (BOOL) HIWORD(wParam);
//			if (!GLAppActivate(!(fActive == WA_INACTIVE), fMinimized))
				break;//so, urm, tell me microsoft, what changed?
			if (modestate == MS_FULLDIB)
				ShowWindow(hWnd, SW_SHOWNORMAL);

#ifdef WTHREAD
#else
		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();	//FIXME: thread

			Cvar_ForceCallback(&vid_conautoscale);	//FIXME: thread
#endif
			break;
*/
		case WM_DESTROY:
			break;
		case WM_SETCURSOR:
			//only use a custom cursor if the cursor is inside the client area
			switch(lParam&0xffff)
			{
			case 0:
				break;
			case HTCLIENT:
				if (hCustomCursor)	//custom cursor enabled
					SetCursor(hCustomCursor);
				else				//fallback on an arrow cursor, just so we have something visible at startup or so
					SetCursor(hArrowCursor);
				lRet = TRUE;
				break;
			default:
				lRet = DefWindowProcW (hWnd, uMsg, wParam, lParam);
				break;
			}
			break;

#ifndef WTHREAD
		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);	//FIXME: thread
			break;
#endif

		default:
			/* pass all unhandled messages to DefWindowProc */
			if (WinNT)
				lRet = DefWindowProcW (hWnd, uMsg, wParam, lParam);
			else
				lRet = DefWindowProcA (hWnd, uMsg, wParam, lParam);
			break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}

void VKVID_DeInit (void)
{
	VKVID_Shutdown();
	vid.activeapp = false;

	Cvar_Unhook(&vid_vsync);
	Cvar_Unhook(&vid_wndalpha);
	Cmd_RemoveCommand("vid_recenter");

	if (WinNT)
		UnregisterClassW(WINDOW_CLASS_NAME_W, global_hInstance);
	else
		UnregisterClassA(WINDOW_CLASS_NAME_A, global_hInstance);
}

/*
===================
VID_Init
===================
*/
qboolean VKVID_Init (rendererstate_t *info, unsigned char *palette)
{
	extern int isPlugin;
//	qbyte	*ptmp;
	DEVMODE	devmode;

	memset(&devmode, 0, sizeof(devmode));

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));
	hArrowCursor = LoadCursor (NULL,IDC_ARROW);

	vid_initialized = false;
	vid_initializing = true;

	if (!VKVID_SetMode (info, palette))
	{
		VID_UnSetMode();
		return false;
	}

	vid_canalttab = true;

	Cvar_Hook(&vid_vsync, VID_Wait_Override_Callback);
	Cvar_Hook(&vid_wndalpha, VID_WndAlpha_Override_Callback);

	Cmd_AddCommand("vid_recenter", VKVID_Recenter_f);

	if (isPlugin >= 2)
	{
		fprintf(stdout, "refocuswindow %"PRIxPTR"\n", (quintptr_t)mainwindow);
		fflush(stdout);
	}

	vid_initialized = true;
	vid_initializing = false;

	return true;
}

rendererinfo_t vkrendererinfo =
{
	"Vulkan",
	{
		"vk",
		"Vulkan"
	},
	QR_VULKAN,

	VK_Draw_Init,
	VK_Draw_Shutdown,

	VK_UpdateFiltering,
	VK_LoadTextureMips,
	VK_DestroyTexture,

	VK_R_Init,
	VK_R_DeInit,
	VK_R_RenderView,

	VKVID_Init,
	VKVID_DeInit,
	VKVID_SwapBuffers,
	VKVID_ApplyGammaRamps,
	WIN_CreateCursor,
	WIN_SetCursor,
	WIN_DestroyCursor,
	VKVID_SetWindowCaption,
	VKVID_GetRGBInfo,

	VK_SCR_UpdateScreen,

	VKBE_SelectMode,
	VKBE_DrawMesh_List,
	VKBE_DrawMesh_Single,
	VKBE_SubmitBatch,
	VKBE_GetTempBatch,
	VKBE_DrawWorld,
	VKBE_Init,
	VKBE_GenBrushModelVBO,
	VKBE_ClearVBO,
	VKBE_UploadAllLightmaps,
	VKBE_SelectEntity,
	VKBE_SelectDLight,
	VKBE_Scissor,
	VKBE_LightCullModel,

	VKBE_VBO_Begin,
	VKBE_VBO_Data,
	VKBE_VBO_Finish,
	VKBE_VBO_Destroy,

	VKBE_RenderToTextureUpdate2d,

	"no more"
};
#endif
