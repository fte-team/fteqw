#include "quakedef.h"
#ifdef D3DQUAKE
#include "winquake.h"
#include "d3d9quake.h"
#include "glquake.h"

//#include    "d3d9.h"


//#pragma comment(lib, "../libs/dxsdk9/lib/d3d9.lib")


#ifndef WM_XBUTTONDOWN
   #define WM_XBUTTONDOWN      0x020B
   #define WM_XBUTTONUP      0x020C
#endif
#ifndef MK_XBUTTON1
   #define MK_XBUTTON1         0x0020
   #define MK_XBUTTON2         0x0040
// copied from DarkPlaces in an attempt to grab more buttons
   #define MK_XBUTTON3         0x0080
   #define MK_XBUTTON4         0x0100
   #define MK_XBUTTON5         0x0200
   #define MK_XBUTTON6         0x0400
   #define MK_XBUTTON7         0x0800
#endif 

#ifndef WM_INPUT
	#define WM_INPUT 255
#endif


int gl_bumpmappingpossible;

void D3D9_GetBufferSize(int *width, int *height);
static LPDIRECT3D9 pD3D;
LPDIRECT3DDEVICE9 pD3DDev9;
//static LPDIRECTDRAWGAMMACONTROL pGammaControl;


static qboolean vid_initializing;

extern qboolean        scr_initialized;                // ready to draw
extern qboolean        scr_drawloading;


cvar_t vid_hardwaregamma;


HWND mainwindow;

struct texture_s	*r_notexture_mip;

int		r_framecount;

mleaf_t		*r_viewleaf;

#define	MAX_MOD_KNOWN	1024
int mod_numknown;
model_t	mod_known[MAX_MOD_KNOWN];
model_t *loadmodel;
model_t *currentmodel;
char	loadname[32];
qbyte *mod_base;

qboolean			DDActive;

model_t *lightmodel;
int relitsurface;

int		window_center_x, window_center_y;
RECT		window_rect;
int window_x, window_y;

qboolean	r_cache_thrash;	// set if thrashing the surface cache

mpic_t		*draw_disc;	// also used on sbar

int d3d9width, d3d9height;
#if 0
#if !defined(SWQUAKE) && !defined(RGLQUAKE)
qbyte GetPaletteIndex(int red, int green, int blue)
{
	//slow, horrible method.
	{
		int i, best=15;
		int bestdif=256*256*256, curdif;
		extern qbyte *host_basepal;
		qbyte *pa;

	#define _abs(x) ((x)*(x))

		pa = host_basepal;
		for (i = 0; i < 256; i++, pa+=3)
		{
			curdif = _abs(red - pa[0]) + _abs(green - pa[1]) + _abs(blue - pa[2]);
			if (curdif < bestdif)
			{
				if (curdif<1)
					return i;
				bestdif = curdif;
				best = i;
			}
		}
		return best;
	}
}
#endif
#endif

void BuildGammaTable (float g, float c);
void	D3D9_VID_GenPaletteTables (unsigned char *palette)
{
	extern unsigned short		ramps[3][256];
	qbyte	*pal;
	unsigned r,g,b;
	unsigned v;
	unsigned short i;
	unsigned	*table;
	extern qbyte gammatable[256];

	if (palette)
	{
		extern cvar_t v_contrast;
		BuildGammaTable(v_gamma.value, v_contrast.value);

		//
		// 8 8 8 encoding
		//
		if (1)//vid_hardwaregamma.value)
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

		if (LittleLong(1) != 1)
		{
			for (i=0 ; i<256 ; i++)
				d_8to24rgbtable[i] = LittleLong(d_8to24rgbtable[i]);
		}
	}

	if (pD3DDev9)
		IDirect3DDevice9_SetGammaRamp(pD3DDev9, 0, D3DSGR_NO_CALIBRATION, ramps);
}
#if 0
#if !defined(SWQUAKE) && !defined(GLQUAKE)
void D_FlushCaches (void)
{
}
#endif
#endif

/*

void D3D9_Mod_Think (void)
{
}
void D3D9_Mod_NowLoadExternal(void)
{
}
void	D3D9_Mod_TouchModel (char *name)
{
}
void	*D3D9_Mod_Extradata (struct model_s *mod)	// handles caching
{
	return NULL;
}
struct model_s *D3D9_Mod_FindName (char *name)
{
	return NULL;
}
struct model_s *D3D9_Mod_ForName (char *name, qboolean crash)
{
	return NULL;
}
void	D3D9_Mod_ClearAll (void)
{
}
void	D3D9_Mod_Init (void)
{
}*/


typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;
static modestate_t modestate;

qboolean D3D9AppActivate(BOOL fActive, BOOL minimize)
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

	if (fActive)
	{
/*		if (modestate != MS_WINDOWED)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
//			if (vid_canalttab && vid_wassuspended)
			{
//				vid_wassuspended = false;
//				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);

								// Fix for alt-tab bug in NVidia drivers
//				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value && (key_dest == key_game || key_dest == key_menu))
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
*/
		Cvar_ForceCallback(&v_gamma);
	}

	if (!fActive)
	{
/*		if (modestate != MS_WINDOWED)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
//			if (vid_canalttab)
//			{ 
//				ChangeDisplaySettings (NULL, 0);
//				vid_wassuspended = true;
//			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
*/
		Cvar_ForceCallback(&v_gamma);	//wham bam thanks.
/*
		if (qSetDeviceGammaRamp)
		{
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
		*/
	}

	return true;
}





static LRESULT WINAPI D3D9_WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
			GetWindowRect(mainwindow, &window_rect);
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
//			VID_UpdateWindowStatus ();
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

		case WM_INPUT:
			// raw input handling
			IN_RawInput_MouseRead((HANDLE)lParam);
			break;

    	case WM_SIZE:
			if (!vid_initializing)
			{
				GetWindowRect(mainwindow, &window_rect);
				// force width/height to be updated
//				glwidth = window_rect.right - window_rect.left;
//				glheight = window_rect.bottom - window_rect.top;
//				Cvar_ForceCallback(&vid_conautoscale);
//				Cvar_ForceCallback(&vid_conwidth);
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
			if (!D3D9AppActivate(!(fActive == WA_INACTIVE), fMinimized))
				break;//so, urm, tell me microsoft, what changed?
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWNORMAL);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
//			ClearAllStates ();

			break;

   	    case WM_DESTROY:
        {
//			if (dibwindow)
//				DestroyWindow (dibwindow);
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

static D3DPRESENT_PARAMETERS d3dpp;
void resetD3D9(void)
{
	IDirect3DDevice9_Reset(pD3DDev9, &d3dpp);


	IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 255, 0), 1.0f, 0);
	IDirect3DDevice9_BeginScene(pD3DDev9);
	IDirect3DDevice9_EndScene(pD3DDev9);
	IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);



	
	
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAREF, 0.666*256 );

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHATESTENABLE, TRUE );


	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_DITHERENABLE, FALSE);
	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_SPECULARENABLE, FALSE);
	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_LIGHTING, FALSE);

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZWRITEENABLE, TRUE);

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

}

#if (WINVER < 0x500) && !defined(__GNUC__)
typedef struct tagMONITORINFO
{
    DWORD   cbSize;
    RECT    rcMonitor;
    RECT    rcWork;
    DWORD   dwFlags;
} MONITORINFO, *LPMONITORINFO;
#endif
/*
void testD3D(HWND hWnd)
{
	
	D3DPRESENT_PARAMETERS d3dpp[2];

	int i;
	int numadaptors;
	int err;
	D3DADAPTER_IDENTIFIER9 inf;

	static HMODULE d3d9dll;
	LPDIRECT3D9 (WINAPI *pDirect3DCreate9) (int version);

	if (!d3d9dll)
		d3d9dll = LoadLibrary("d3d9.dll");
	if (!d3d9dll)
	{
		Con_Printf("Direct3d 9 does not appear to be installed\n");
		return;
	}
	pDirect3DCreate9 = (void*)GetProcAddress(d3d9dll, "Direct3DCreate9");
	if (!pDirect3DCreate9)
	{
		Con_Printf("Direct3d 9 does not appear to be installed properly\n");
		return;
	}

	pD3D = pDirect3DCreate9(D3D_SDK_VERSION);    // create the Direct3D interface
	if (!pD3D)
		return;

	for (i = 0; i < 2; i++)
	{
		memset(&d3dpp[0], 0, sizeof(d3dpp));    // clear out the struct for use
		d3dpp[0].SwapEffect = D3DSWAPEFFECT_DISCARD;    // discard old frames
		d3dpp[0].hDeviceWindow = hWnd;    // set the window to be used by Direct3D
		d3dpp[0].BackBufferWidth = d3d9width = info->width;
		d3dpp[0].BackBufferHeight = d3d9height = info->height;
		d3dpp[0].MultiSampleType = info->multisample;
		d3dpp[0].BackBufferCount = 3;
		d3dpp[0].FullScreen_RefreshRateInHz = info->fullscreen?info->rate:0;	//don't pass a rate if not fullscreen, d3d doesn't like it.
		d3dpp[0].Windowed = !info->fullscreen;

		d3dpp[0].EnableAutoDepthStencil = true;
		d3dpp[0].AutoDepthStencilFormat = D3DFMT_D16;
		d3dpp[0].BackBufferFormat = info->fullscreen?D3DFMT_X8R8G8B8:D3DFMT_UNKNOWN;
	}

		IDirect3D9_CreateDevice(pD3D, 
				i,
				D3DDEVTYPE_HAL,
				hWnd,
				D3DCREATE_SOFTWARE_VERTEXPROCESSING,
				&d3dpp,
				&pD3DDev9);

}
*/
void initD3D9(HWND hWnd, rendererstate_t *info)
{
	int i;
	int numadaptors;
	int err;
	D3DADAPTER_IDENTIFIER9 inf;
	extern cvar_t _vid_wait_override;

	static HMODULE d3d9dll;
	LPDIRECT3D9 (WINAPI *pDirect3DCreate9) (int version);

	if (!d3d9dll)
		d3d9dll = LoadLibrary("d3d9.dll");
	if (!d3d9dll)
	{
		Con_Printf("Direct3d 9 does not appear to be installed\n");
		return;
	}
	pDirect3DCreate9 = (void*)GetProcAddress(d3d9dll, "Direct3DCreate9");
	if (!pDirect3DCreate9)
	{
		Con_Printf("Direct3d 9 does not appear to be installed properly\n");
		return;
	}

	pD3D = pDirect3DCreate9(D3D_SDK_VERSION);    // create the Direct3D interface
	if (!pD3D)
		return;

	numadaptors = IDirect3D9_GetAdapterCount(pD3D);
	for (i = 0; i < numadaptors; i++)
	{	//try each adaptor in turn until we get one that actually works
		memset(&d3dpp, 0, sizeof(d3dpp));    // clear out the struct for use
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;    // discard old frames
		d3dpp.hDeviceWindow = hWnd;    // set the window to be used by Direct3D
		d3dpp.BackBufferWidth = d3d9width = info->width;
		d3dpp.BackBufferHeight = d3d9height = info->height;
		d3dpp.MultiSampleType = info->multisample;
		d3dpp.BackBufferCount = 3;
		d3dpp.FullScreen_RefreshRateInHz = info->fullscreen?info->rate:0;	//don't pass a rate if not fullscreen, d3d doesn't like it.
		d3dpp.Windowed = !info->fullscreen;

		d3dpp.EnableAutoDepthStencil = true;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
		d3dpp.BackBufferFormat = info->fullscreen?D3DFMT_X8R8G8B8:D3DFMT_UNKNOWN;

		if (!*_vid_wait_override.string)
			d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
		else
		{
			if (_vid_wait_override.value == 1)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
			else if (_vid_wait_override.value == 2)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
			else if (_vid_wait_override.value == 3)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_THREE;
			else if (_vid_wait_override.value == 4)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_FOUR;
			else
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}

		memset(&inf, 0, sizeof(inf));
		err = IDirect3D9_GetAdapterIdentifier(pD3D, i, 0, &inf);

		// create a device class using this information and information from the d3dpp stuct
		IDirect3D9_CreateDevice(pD3D, 
				i,
				D3DDEVTYPE_HAL,
				hWnd,
				D3DCREATE_SOFTWARE_VERTEXPROCESSING,
				&d3dpp,
				&pD3DDev9);

		if (pD3DDev9)
		{
			HMONITOR hm;
			MONITORINFO mi;
			char *s;
			for (s = inf.Description + strlen(inf.Description)-1; s >= inf.Description && *s <= ' '; s--)
				*s = 0;
			Con_Printf("D3D9: Using device %s\n", inf.Description);

			if (d3dpp.Windowed)	//fullscreen we get positioned automagically.
			{					//windowed, we get positioned at 0,0... which is often going to be on the wrong screen
								//the user can figure it out from here
				static HANDLE huser32;
				BOOL (WINAPI *pGetMonitorInfoA)(HMONITOR hMonitor, LPMONITORINFO lpmi);
				if (!huser32)
					huser32 = LoadLibrary("user32.dll");
				if (!huser32)
					return;
				pGetMonitorInfoA = (void*)GetProcAddress(huser32, "GetMonitorInfoA");
				if (!pGetMonitorInfoA)
					return;

				hm = IDirect3D9_GetAdapterMonitor(pD3D, i);
				memset(&mi, 0, sizeof(mi));
				mi.cbSize = sizeof(mi);
				pGetMonitorInfoA(hm, &mi);
				MoveWindow(d3dpp.hDeviceWindow, mi.rcWork.left, mi.rcWork.top, d3dpp.BackBufferWidth, d3dpp.BackBufferHeight, false);
			}
			return;	//successful
		}
	}


	Con_Printf("IDirect3D9_CreateDevice failed\n");


	return;
}

qboolean D3D9_VID_Init(rendererstate_t *info, unsigned char *palette)
{
	DWORD width = info->width;
	DWORD height = info->height;
	DWORD bpp = info->bpp;
	DWORD zbpp = 16;
	DWORD flags = 0;
	MSG msg;

	extern cvar_t vid_conwidth;
	extern cvar_t vid_conheight;

	//DDGAMMARAMP gammaramp;
	//int i;

	char *CLASSNAME = "FTED3D9QUAKE";
	WNDCLASS wc = {
		0,
		&D3D9_WindowProc,
		0,
		0,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		CLASSNAME
	};

	vid_initializing = true;

	RegisterClass(&wc);

	if (info->fullscreen)
		mainwindow = CreateWindow(CLASSNAME, "Direct3D", 0, 0, 0, width, height, NULL, NULL, NULL, NULL);
	else
		mainwindow = CreateWindow(CLASSNAME, "Direct3D", WS_OVERLAPPEDWINDOW, 0, 0, width, height, NULL, NULL, NULL, NULL);
/*
	width = vid_conwidth.value;
	height = vid_conheight.value;
*/
	// Try as specified.

	initD3D9(mainwindow, info);
	if (!pD3DDev9)
		return false;



	while (PeekMessage(&msg, NULL,  0, 0, PM_REMOVE)) 
	{ 
		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	}

	ShowWindow(mainwindow, SW_NORMAL);
	//IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, 0xffffffff, 1, 0);
	//IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);

	IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	IDirect3DDevice9_BeginScene(pD3DDev9);
	IDirect3DDevice9_EndScene(pD3DDev9);
	IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);



//	pD3DX->lpVtbl->GetBufferSize((void*)pD3DX, &width, &height);
	vid.width = width;
	vid.height = height;
	vid.recalc_refdef = true;

//	pDD->lpVtbl->QueryInterface ((void*)pDD, &IID_IDirectDrawGammaControl, (void**)&pGammaControl);
/*	if (pGammaControl)
	{
		for (i = 0; i < 256; i++)
			gammaramp.red[i] = i*2;
		pGammaControl->lpVtbl->SetGammaRamp(pGammaControl, 0, &gammaramp);
	}
	else*/
		Con_Printf("Couldn't get gamma controls\n");

	vid_initializing = false;



resetD3D9();/*

	
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAREF, 0.666*256 );

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHATESTENABLE, TRUE );


	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_DITHERENABLE, FALSE);
	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_SPECULARENABLE, FALSE);
	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_LIGHTING, FALSE);

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZWRITEENABLE, TRUE);

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

*/
	GetWindowRect(mainwindow, &window_rect);


	D3D9_VID_GenPaletteTables(palette);

	{
		extern qboolean	mouseactive;
		mouseactive = false;
	}

	{
		extern cvar_t v_contrast;
		void GLV_Gamma_Callback(struct cvar_s *var, char *oldvalue);
		Cvar_Hook(&v_gamma, GLV_Gamma_Callback);
		Cvar_Hook(&v_contrast, GLV_Gamma_Callback);

		Cvar_ForceCallback(&v_gamma);
	}

	{
		extern cvar_t vid_conwidth, vid_conheight;
		vid.conwidth = vid_conwidth.value;
		vid.conheight = vid_conheight.value;
		if (vid.width != vid.conwidth || vid.height != vid.conheight)
			vid.recalc_refdef = true;
		vid.width = vid.conwidth;
		vid.height = vid.conheight;
	}

	return true;
}



qboolean	(D3D9_R_CheckSky)			(void)
{
	return false;
}
void	(D3D9_R_SetSky)					(char *name, float rotate, vec3_t axis)
{
}

void	(D3D9_R_NewMap)					(void)
{
	extern int skytexturenum;
	int i;
	r_worldentity.model = cl.worldmodel;
	GLR_AnimateLight();
	D3D9_BuildLightmaps();

	P_ClearParticles();

	skytexturenum = -1;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
		if (!Q_strncmp(cl.worldmodel->textures[i]->name,"sky",3) )
			skytexturenum = i;
//		if (!Q_strncmp(cl.worldmodel->textures[i]->name,"window02_1",10) )
//			mirrortexturenum = i;
 		cl.worldmodel->textures[i]->texturechain = NULL;
	}
}

extern mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
void	(D3D9_R_PreNewMap)				(void)
{
	r_viewleaf = NULL;
	r_oldviewleaf = NULL;
	r_viewleaf2 = NULL;
	r_oldviewleaf2 = NULL;
}
int		(D3D9_R_LightPoint)				(vec3_t point)
{
	return 0;
}

void	(D3D9_R_PushDlights)			(void)
{
}
void	(D3D9_R_AddStain)				(vec3_t org, float red, float green, float blue, float radius)
{
}
void	(D3D9_R_LessenStains)			(void)
{
}

void	(D3D9_Mod_Init)					(void)
{
}
void	(D3D9_Mod_ClearAll)				(void)
{
}
struct model_s *(D3D9_Mod_ForName)		(char *name, qboolean crash)
{
	return NULL;
}
struct model_s *(D3D9_Mod_FindName)		(char *name)
{
	return NULL;
}
void	*(D3D9_Mod_Extradata)			(struct model_s *mod)
{
	return NULL;
}	// handles caching
void	(D3D9_Mod_TouchModel)			(char *name)
{
}

void	(D3D9_Mod_NowLoadExternal)		(void)
{
}
void	(D3D9_Mod_Think)				(void)
{
}

int (D3D9_Mod_SkinForName)				(struct model_s *model, char *name)
{
	return 0;
}

void	 (D3D9_VID_DeInit)				(void)
{
	if (pD3DDev9)
	{
		IDirect3DDevice9_Release(pD3DDev9);
		pD3DDev9 = NULL;
	}
	if (pD3D)
	{
		IDirect3D9_Release(pD3D);
		pD3D = NULL;
	}
	if (mainwindow)
	{
		DestroyWindow(mainwindow);
		mainwindow = NULL;
	}
}
void	(D3D9_VID_LockBuffer)			(void)
{
}
void	(D3D9_VID_UnlockBuffer)			(void)
{
}
void	(D3D9_D_BeginDirectRect)		(int x, int y, qbyte *pbitmap, int width, int height)
{
}
void	(D3D9_D_EndDirectRect)			(int x, int y, int width, int height)
{
}
void	(D3D9_VID_ForceLockState)		(int lk)
{
}
int		(D3D9_VID_ForceUnlockedAndReturnState) (void)
{
	return 0;
}

void	(D3D9_VID_SetPalette)			(unsigned char *palette)
{
	D3D9_VID_GenPaletteTables(palette);
}
void	(D3D9_VID_ShiftPalette)			(unsigned char *palette)
{
	D3D9_VID_GenPaletteTables(palette);
}
char	*(D3D9_VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight)
{
	return NULL;
}
void	(D3D9_VID_SetWindowCaption)		(char *msg)
{
	SetWindowText(mainwindow, msg);
}

#if 0
#undef IDirect3DDevice9_SetRenderState
void IDirect3DDevice9_SetRenderState(void *pD3DDev9, int D3DRS, int param)
{
}
#endif

void D3D9_Set2D (void)
{
	float m[16];
	D3DVIEWPORT9 vport;
//	IDirect3DDevice9_EndScene(pD3DDev9);

	Matrix4_Orthographic(m, 0, vid.width, vid.height, 0, -100, 100);
	d3d7_ortho(m);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_PROJECTION, (D3DMATRIX*)m);

	Matrix4_Identity(m);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_WORLD, (D3DMATRIX*)m);

	Matrix4_Identity(m);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_VIEW, (D3DMATRIX*)m);

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_CCW );

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZFUNC, D3DCMP_ALWAYS);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZENABLE, D3DZB_FALSE);

	IDirect3DDevice9_SetSamplerState(pD3DDev9, 0, D3DSAMP_MAGFILTER,  D3DTEXF_LINEAR );
	IDirect3DDevice9_SetSamplerState(pD3DDev9, 1, D3DSAMP_MAGFILTER,  D3DTEXF_LINEAR );

	IDirect3DDevice9_SetSamplerState(pD3DDev9, 0, D3DSAMP_MIPFILTER,  D3DTEXF_NONE );
	IDirect3DDevice9_SetSamplerState(pD3DDev9, 1, D3DSAMP_MIPFILTER,  D3DTEXF_NONE );

	IDirect3DDevice9_SetSamplerState(pD3DDev9, 0, D3DSAMP_MINFILTER,  D3DTEXF_LINEAR );
	IDirect3DDevice9_SetSamplerState(pD3DDev9, 1, D3DSAMP_MINFILTER,  D3DTEXF_LINEAR );

	vport.X = 0;
	vport.Y = 0;
	D3D9_GetBufferSize(&vport.Width, &vport.Height);
	vport.MinZ = 0;
	vport.MaxZ = 1;
	IDirect3DDevice9_SetViewport(pD3DDev9, &vport);

//	IDirect3DDevice9_BeginScene(pD3DDev9);
}

void D3D9_GetBufferSize(int *width, int *height)
{
	*width = d3d9width;//vid.width;
	*height = d3d9height;//vid.height;
//	IDirect3DDevice9_GetBufferSize((void*)pD3DX, width, height);
}

int d3d9error(int i)
{
	if (FAILED(i))// != D3D_OK)
		Con_Printf("D3D error: %x  %i\n", i);
	return i;
}

typedef struct {
	float x, y, z;
	unsigned int colour;
} d3d9bsvert_t;
void D3D9_BrightenScreen (void)
{
	d3d9bsvert_t d3d9bsvert[4];
index_t d3d9quadindexes[6] = {
	0, 1, 2,
	0, 2, 3
};

extern cvar_t gl_contrast;
	float f;
	unsigned int colour;

	RSpeedMark();

	if (gl_contrast.value <= 1.0)
		return;

	f = gl_contrast.value;
	f = min (f, 3);

	IDirect3DDevice9_SetTexture (pD3DDev9, 0, NULL);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_DESTBLEND, D3DBLEND_ONE);

	while (f > 1)
	{
		if (f >= 2)
			colour = 0xffffffff;
		else
		{
			colour = (f-1)*255;
			colour = (colour * 0x010101) | 0xff000000;
		}


		d3d9bsvert[0].x = 0;
		d3d9bsvert[0].y = 0;
		d3d9bsvert[0].z = 0;
		d3d9bsvert[0].colour = colour;

		d3d9bsvert[1].x = vid.width;
		d3d9bsvert[1].y = 0;
		d3d9bsvert[1].z = 0;
		d3d9bsvert[1].colour = colour;

		d3d9bsvert[2].x = vid.width;
		d3d9bsvert[2].y = vid.height;
		d3d9bsvert[2].z = 0;
		d3d9bsvert[2].colour = colour;

		d3d9bsvert[3].x = 0;
		d3d9bsvert[3].y = vid.height;
		d3d9bsvert[3].z = 0;
		d3d9bsvert[3].colour = colour;

		IDirect3DDevice9_SetFVF(pD3DDev9, D3DFVF_XYZ|D3DFVF_DIFFUSE);
		IDirect3DDevice9_DrawIndexedPrimitiveUP(pD3DDev9, D3DPT_TRIANGLELIST, 0, 4, 2, d3d9quadindexes, D3DFMT_QINDEX, d3d9bsvert, sizeof(d3d9bsvert[0]));


		f *= 0.5;
	}
	
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHABLENDENABLE, FALSE);

	RSpeedEnd(RSPEED_PALETTEFLASHES);
}

void	(D3D9_SCR_UpdateScreen)			(void)
{
	extern int keydown[];
	extern cvar_t vid_conheight;
	int uimenu;
#ifdef TEXTEDITOR
	extern qboolean editormodal;
#endif
	qboolean nohud;
	RSpeedMark();

	switch (IDirect3DDevice9_TestCooperativeLevel(pD3DDev9))
	{
	case D3DERR_DEVICELOST:
		//the user has task switched away from us or something
		return;
	case D3DERR_DEVICENOTRESET:
		resetD3D9();
		if (FAILED(IDirect3DDevice9_TestCooperativeLevel(pD3DDev9)))
			Sys_Error("D3D9 Device lost. Additionally restoration failed.\n");
	//	D3DSucks();
	//	scr_disabled_for_loading = false;

		VID_ShiftPalette (NULL);
		break;
	default:
		break;
	}


	if (keydown['k'])
	{
		d3d9error(IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(rand()&255, rand()&255, rand()&255), 1.0f, 0));
		d3d9error(IDirect3DDevice9_BeginScene(pD3DDev9));
		d3d9error(IDirect3DDevice9_EndScene(pD3DDev9));
		d3d9error(IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL));

		VID_ShiftPalette (NULL);
	}


	if (block_drawing)
	{
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}

	vid.numpages = 2;// + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading)
	{
		extern float scr_disabled_time;
		if (Sys_DoubleTime() - scr_disabled_time > 60 || key_dest != key_game)
		{
			scr_disabled_for_loading = false;
		}
		else
		{		
			IDirect3DDevice9_BeginScene(pD3DDev9);
			scr_drawloading = true;
			SCR_DrawLoading ();
			scr_drawloading = false;
			IDirect3DDevice9_EndScene(pD3DDev9);
			IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);
			RSpeedEnd(RSPEED_TOTALREFRESH);
			return;
		}
	}

	if (!scr_initialized || !con_initialized)
	{
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;                         // not initialized yet
	}

	{
		extern cvar_t vid_conwidth, vid_conheight;
		vid.conwidth = vid_conwidth.value;
		vid.conheight = vid_conheight.value;
		if (vid.width != vid.conwidth || vid.height != vid.conheight)
			vid.recalc_refdef = true;
		vid.width = vid.conwidth;
		vid.height = vid.conheight;
	}


#ifdef VM_UI
	uimenu = UI_MenuState();
#else
	uimenu = 0;
#endif

	d3d9error(IDirect3DDevice9_BeginScene(pD3DDev9));
	D3D9_Set2D ();
/*
#ifdef TEXTEDITOR
	if (editormodal)
	{
		Editor_Draw();
		GLV_UpdatePalette (false);
#if defined(_WIN32) && defined(RGLQUAKE)
		Media_RecordFrame();
#endif
		GLR_BrightenScreen();

		if (key_dest == key_console)
			Con_DrawConsole(vid_conheight.value/2, false);
		GL_EndRendering ();	
		GL_DoSwap();
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}
#endif
*/
	if (Media_ShowFilm())
	{
		M_Draw(0);
//		GLV_UpdatePalette (false);
#if defined(_WIN32)
		Media_RecordFrame();
#endif
//		GLR_BrightenScreen();
		IDirect3DDevice9_EndScene(pD3DDev9);
		IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);

		d3d9error(IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(rand()&255, rand()&255, rand()&255), 1, 0));

//		pD3DDev->lpVtbl->BeginScene(pD3DDev);

		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}

	//
	// determine size of refresh window
	//
	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	nohud = false;

#ifdef VM_CG
	if (CG_Refresh())
		nohud = true;
	else
#endif
#ifdef CSQC_DAT
		if (cls.state == ca_active && CSQC_DrawView())
		nohud = true;
	else
#endif
		if (r_worldentity.model && uimenu != 1)
		{
		V_RenderView ();
//		Q1BSP_TestClipDecal();
		}


	D3D9_Set2D ();

	D3D9_BrightenScreen();

	if (!nohud)
		SCR_TileClear ();

	SCR_DrawTwoDimensional(uimenu, nohud);

	GLV_UpdatePalette (false);
#if defined(_WIN32) && defined(RGLQUAKE)
	Media_RecordFrame();
#endif

	RSpeedEnd(RSPEED_TOTALREFRESH);
	RSpeedShow();



	d3d9error(IDirect3DDevice9_EndScene(pD3DDev9));
	d3d9error(IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL));



	d3d9error(IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, rand()&255), 1, 0));
	d3d9error(IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(rand()&255, rand()&255, rand()&255), 1, 0));

	window_center_x = (window_rect.left + window_rect.right)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;



	if (modestate == MS_WINDOWED)
	{
		extern int mouseusedforgui;
		extern qboolean	mouseactive;
		qboolean wantactive;

		wantactive = _windowed_mouse.value && (key_dest == key_game||mouseusedforgui) && ActiveApp;
		if (wantactive != mouseactive)
		{
			if (!mouseactive)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}

	VID_ShiftPalette (NULL);
}





mpic_t	*(D3D9_Draw_SafePicFromWad)			(char *name);
mpic_t	*(D3D9_Draw_CachePic)			(char *path);
mpic_t	*(D3D9_Draw_SafeCachePic)		(char *path);
void	(D3D9_Draw_Init)				(void);
void	(D3D9_Draw_ReInit)				(void);
void	(D3D9_Draw_Character)			(int x, int y, unsigned int num);
void	(D3D9_Draw_ColouredCharacter)	(int x, int y, unsigned int num);
void	(D3D9_Draw_String)				(int x, int y, const qbyte *str);
void	(D3D9_Draw_Alt_String)			(int x, int y, const qbyte *str);
void	(D3D9_Draw_Crosshair)			(void);
void	(D3D9_Draw_DebugChar)			(qbyte num);
void	(D3D9_Draw_Pic)					(int x, int y, mpic_t *pic);
void	(D3D9_Draw_ScalePic)			(int x, int y, int width, int height, mpic_t *pic);
void	(D3D9_Draw_SubPic)				(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void	(D3D9_Draw_TransPic)			(int x, int y, mpic_t *pic);
void	(D3D9_Draw_TransPicTranslate)	(int x, int y, int w, int h, qbyte *pic, qbyte *translation);
void	(D3D9_Draw_ConsoleBackground)	(int lines);
void	(D3D9_Draw_EditorBackground)	(int lines);
void	(D3D9_Draw_TileClear)			(int x, int y, int w, int h);
void	(D3D9_Draw_Fill)				(int x, int y, int w, int h, int c);
void	(D3D9_Draw_FillRGB)				(int x, int y, int w, int h, float r, float g, float b);
void	(D3D9_Draw_FadeScreen)			(void);
void	(D3D9_Draw_BeginDisc)			(void);
void	(D3D9_Draw_EndDisc)				(void);

void	(D3D9_Draw_Image)				(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);	//gl-style scaled/coloured/subpic
void	(D3D9_Draw_ImageColours)		(float r, float g, float b, float a);

void	(D3D9_R_Init)					(void);
void	(D3D9_R_DeInit)					(void);
void	(D3D9_R_ReInit)					(void);
void	(D3D9_R_RenderView)				(void);		// must set r_refdef first

qboolean	(D3D9_R_CheckSky)			(void);
void	(D3D9_R_SetSky)					(char *name, float rotate, vec3_t axis);

void	(D3D9_R_NewMap)					(void);
void	(D3D9_R_PreNewMap)				(void);
int		(D3D9_R_LightPoint)				(vec3_t point);

void	(D3D9_R_PushDlights)			(void);
void	(D3D9_R_AddStain)				(vec3_t org, float red, float green, float blue, float radius);
void	(D3D9_R_LessenStains)			(void);

void (D3D9_Media_ShowFrameBGR_24_Flip)	(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...
void (D3D9_Media_ShowFrameRGBA_32)		(qbyte *framedata, int inwidth, int inheight);	//top down
void (D3D9_Media_ShowFrame8bit)			(qbyte *framedata, int inwidth, int inheight, qbyte *palette);	//paletted topdown (framedata is 8bit indexes into palette)

void	(D3D9_Mod_Init)					(void);
void	(D3D9_Mod_ClearAll)				(void);
struct model_s *(D3D9_Mod_ForName)		(char *name, qboolean crash);
struct model_s *(D3D9_Mod_FindName)		(char *name);
void	*(D3D9_Mod_Extradata)			(struct model_s *mod);	// handles caching
void	(D3D9_Mod_TouchModel)			(char *name);

void	(D3D9_Mod_NowLoadExternal)		(void);
void	(D3D9_Mod_Think)				(void);
qboolean(D3D9_Mod_GetTag)				(struct model_s *model, int tagnum, int frame1, int frame2, float f2ness, float f1time, float f2time, float *result);
int (D3D9_Mod_TagNumForName)			(struct model_s *model, char *name);
int (D3D9_Mod_SkinForName)				(struct model_s *model, char *name);


qboolean (D3D9_VID_Init)				(rendererstate_t *info, unsigned char *palette);
void	 (D3D9_VID_DeInit)				(void);
void	(D3D9_VID_LockBuffer)			(void);
void	(D3D9_VID_UnlockBuffer)			(void);
void	(D3D9_D_BeginDirectRect)		(int x, int y, qbyte *pbitmap, int width, int height);
void	(D3D9_D_EndDirectRect)			(int x, int y, int width, int height);
void	(D3D9_VID_ForceLockState)		(int lk);
int		(D3D9_VID_ForceUnlockedAndReturnState) (void);
void	(D3D9_VID_SetPalette)			(unsigned char *palette);
void	(D3D9_VID_ShiftPalette)			(unsigned char *palette);
char	*(D3D9_VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight);
void	(D3D9_VID_SetWindowCaption)		(char *msg);

void	(D3D9_SCR_UpdateScreen)			(void);







rendererinfo_t d3d9rendererinfo =
{
	"Direct3D9 Native",
	{
		"D3D9",
		"Direct3d",
		"DirectX",
		"DX"
	},
	QR_DIRECT3D,

	D3D9_Draw_SafePicFromWad,
	D3D9_Draw_CachePic,
	D3D9_Draw_SafeCachePic,
	D3D9_Draw_Init,
	D3D9_Draw_ReInit,
	D3D9_Draw_Character,
	D3D9_Draw_ColouredCharacter,
	D3D9_Draw_String,
	D3D9_Draw_Alt_String,
	D3D9_Draw_Crosshair,
	D3D9_Draw_DebugChar,
	D3D9_Draw_Pic,
	D3D9_Draw_ScalePic,
	D3D9_Draw_SubPic,
	D3D9_Draw_TransPic,
	D3D9_Draw_TransPicTranslate,
	D3D9_Draw_ConsoleBackground,
	D3D9_Draw_EditorBackground,
	D3D9_Draw_TileClear,
	D3D9_Draw_Fill,
	D3D9_Draw_FillRGB,
	D3D9_Draw_FadeScreen,
	D3D9_Draw_BeginDisc,
	D3D9_Draw_EndDisc,

	D3D9_Draw_Image,
	D3D9_Draw_ImageColours,

	D3D9_R_Init,
	D3D9_R_DeInit,
	D3D9_R_ReInit,
	D3D9_R_RenderView,

	D3D9_R_CheckSky,
	D3D9_R_SetSky,

	D3D9_R_NewMap,
	D3D9_R_PreNewMap,
	D3D9_R_LightPoint,

	D3D9_R_PushDlights,
	D3D9_R_AddStain,
	D3D9_R_LessenStains,

	D3D9_Media_ShowFrameBGR_24_Flip,
	D3D9_Media_ShowFrameRGBA_32,
	D3D9_Media_ShowFrame8bit,

	GLMod_Init,
	GLMod_ClearAll,
	GLMod_ForName,
	GLMod_FindName,
	GLMod_Extradata,
	GLMod_TouchModel,

	GLMod_NowLoadExternal,
	GLMod_Think,
	Mod_GetTag,
	Mod_TagNumForName,
	D3D9_Mod_SkinForName,


	D3D9_VID_Init,
	D3D9_VID_DeInit,
	D3D9_VID_LockBuffer,
	D3D9_VID_UnlockBuffer,
	D3D9_D_BeginDirectRect,
	D3D9_D_EndDirectRect,
	D3D9_VID_ForceLockState,
	D3D9_VID_ForceUnlockedAndReturnState,
	D3D9_VID_SetPalette,
	D3D9_VID_ShiftPalette,
	D3D9_VID_GetRGBInfo,
	D3D9_VID_SetWindowCaption,

	D3D9_SCR_UpdateScreen

};
#endif
