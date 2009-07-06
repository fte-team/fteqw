#include "quakedef.h"
#ifdef D3DQUAKE
#include "winquake.h"
#include "d3dquake.h"
#include "glquake.h"

#pragma comment(lib, "d3dx.lib")
#pragma comment(lib, "ddraw.lib")
#pragma comment(lib, "dxguid.lib")


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


int gl_bumpmappingpossible;

static LPD3DXCONTEXT pD3DX;
static LPDIRECTDRAW7 pDD;
static LPDIRECT3D7 pD3D;
LPDIRECT3DDEVICE7 pD3DDev;
static LPDIRECTDRAWSURFACE7 pPrimary;
static LPDIRECTDRAWGAMMACONTROL pGammaControl;


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

void BuildGammaTable (float g, float c);
void	D3D7_VID_GenPaletteTables (unsigned char *palette)
{
	qbyte	*pal;
	unsigned r,g,b;
	unsigned v;
	unsigned short i;
	unsigned	*table;
	extern qbyte gammatable[256];

	if (palette)
	{
		BuildGammaTable(1, 1);

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

		if (LittleLong(1) != 1)
			for (i=0 ; i<256 ; i++)
				d_8to24rgbtable[i] = LittleLong(d_8to24rgbtable[i]);
	}
}

#if !defined(SWQUAKE) && !defined(GLQUAKE)
void D_FlushCaches (void)
{
}
#endif




void D3DMod_Think (void)
{
}
void D3DMod_NowLoadExternal(void)
{
}
void	D3DMod_TouchModel (char *name)
{
}
void	*D3DMod_Extradata (struct model_s *mod)	// handles caching
{
	return NULL;
}
struct model_s *D3DMod_FindName (char *name)
{
	return NULL;
}
struct model_s *D3DMod_ForName (char *name, qboolean crash)
{
	return NULL;
}
void	D3DMod_ClearAll (void)
{
}
void	D3DMod_Init (void)
{
}


typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;
static modestate_t modestate;

qboolean D3DAppActivate(BOOL fActive, BOOL minimize)
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
		Cvar_ForceCallback(&v_gamma);
	}

	if (!fActive)
	{
		Cvar_ForceCallback(&v_gamma);	//wham bam thanks.
	}

	return true;
}





static LRESULT WINAPI D3D7_WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
				IN_TranslateKeyEvent(wParam, lParam, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (!vid_initializing)
				IN_TranslateKeyEvent(wParam, lParam, false);
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
			if (!D3DAppActivate(!(fActive == WA_INACTIVE), fMinimized))
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



qboolean D3D7_VID_Init(rendererstate_t *info, unsigned char *palette)
{
	DWORD width = info->width;
	DWORD height = info->height;
	DWORD bpp = info->bpp;
	DWORD zbpp = 16;
	DWORD flags = 0;
	MSG msg;

	extern cvar_t vid_conwidth;
	extern cvar_t vid_conheight;

	DDGAMMARAMP gammaramp;
	char errs[1024];
	int i;
	HRESULT hr;

	char *CLASSNAME = "FTED3D7QUAKE";
	WNDCLASS wc = {
		0,
		&D3D7_WindowProc,
		0,
		0,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		CLASSNAME
	};

//	wc.style = 0;
	wc.lpfnWndProc = D3D7_WindowProc;
//	wc.cbClsExtra;
//	wc.cbWndExtra;
//	wc.hInstance;
//	wc.hIcon;
//	wc.hCursor;
//	wc.hbrBackground;
//	wc.lpszMenuName;
	wc.lpszClassName = CLASSNAME;

	vid_initializing = true;

	if( FAILED(hr = D3DXInitialize()) )
	{
		D3DXGetErrorString(hr, sizeof(errs), errs);
		Con_Printf("D3D initialisation failed: error %X: %s\n", hr, errs);
		return false;
	}

	RegisterClass(&wc);

	flags |= info->fullscreen ? D3DX_CONTEXT_FULLSCREEN : 0;

	if (flags & D3DX_CONTEXT_FULLSCREEN)
		mainwindow = CreateWindow(CLASSNAME, "Direct3D", 0, 0, 0, width, height, NULL, NULL, NULL, NULL);
	else
		mainwindow = CreateWindow(CLASSNAME, "Direct3D", WS_OVERLAPPEDWINDOW, 0, 0, width, height, NULL, NULL, NULL, NULL);
/*
	width = vid_conwidth.value;
	height = vid_conheight.value;
*/
	// Try as specified.
	hr = D3DXCreateContextEx(D3DX_DEFAULT, flags,
		mainwindow, NULL, bpp, 0,
		zbpp, 0, 1, width, height, D3DX_DEFAULT, &pD3DX);
	if( FAILED(hr) )
	{
		D3DXGetErrorString(hr, sizeof(errs), errs);
		printf("D3D initialisation failed: error %X: %s\n", hr, errs);

		// default z-buffer
		hr = D3DXCreateContextEx(D3DX_DEFAULT, flags,
			mainwindow, NULL, bpp, 0,
			D3DX_DEFAULT, 0, 1, width, height, D3DX_DEFAULT, &pD3DX);
		if( FAILED(hr) )
		{
			// default depth and z-buffer
			hr = D3DXCreateContextEx(D3DX_DEFAULT, flags,
				mainwindow, NULL, D3DX_DEFAULT, 0,
				D3DX_DEFAULT, 0, 1, width, height, D3DX_DEFAULT, &pD3DX);
			if( FAILED(hr) )
			{
				// default everything
				hr = D3DXCreateContextEx(D3DX_DEFAULT, flags,
					mainwindow, NULL, D3DX_DEFAULT, 0,
					D3DX_DEFAULT, 0, 1, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, &pD3DX);
				if( FAILED(hr) )
				{
					D3DXGetErrorString(hr, sizeof(errs), errs);
					Con_Printf("D3D fallbacks failed: error %X: %s\n", hr, errs);

					DestroyWindow(mainwindow);
					mainwindow = NULL;
					return false;
				}
			}
		}
	}
	//the void* casts are because microsoft screwed up in the header files I have
	pDD = pD3DX->lpVtbl->GetDD((void*)pD3DX);
	pD3D = pD3DX->lpVtbl->GetD3D((void*)pD3DX);
	pD3DDev = pD3DX->lpVtbl->GetD3DDevice((void*)pD3DX);
	pPrimary = pD3DX->lpVtbl->GetPrimary((void*)pD3DX);

	while (PeekMessage(&msg, NULL,  0, 0, PM_REMOVE)) 
	{ 
		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	}

	ShowWindow(mainwindow, SW_NORMAL);
	pD3DX->lpVtbl->SetClearColor((void*)pD3DX, 0xffffffff);
	pD3DX->lpVtbl->Clear((void*)pD3DX, D3DCLEAR_TARGET);
	pD3DX->lpVtbl->UpdateFrame((void*)pD3DX, 0);



	pD3DX->lpVtbl->GetBufferSize((void*)pD3DX, &width, &height);
	vid.width = width;
	vid.height = height;
	vid.recalc_refdef = true;

	pDD->lpVtbl->QueryInterface ((void*)pDD, &IID_IDirectDrawGammaControl, (void**)&pGammaControl);
	if (pGammaControl)
	{
		for (i = 0; i < 256; i++)
			gammaramp.red[i] = i*2;
		pGammaControl->lpVtbl->SetGammaRamp(pGammaControl, 0, &gammaramp);
	}
	else
		Con_Printf("Couldn't get gamma controls\n");

	vid_initializing = false;





	
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER );
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHAREF, 0.666*256 );

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE );


	//pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_DITHERENABLE, FALSE);
	//pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_SPECULARENABLE, FALSE);
	//pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_LIGHTING, FALSE);

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZWRITEENABLE, TRUE);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZVISIBLE, TRUE);

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);


	GetWindowRect(mainwindow, &window_rect);


	D3D7_VID_GenPaletteTables(palette);



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



qboolean	(D3D7_R_CheckSky)			(void)
{
	return false;
}
void	(D3D7_R_SetSky)					(char *name, float rotate, vec3_t axis)
{
}

void	(D3D7_R_NewMap)					(void)
{
	extern int skytexturenum;
	int i;
	r_worldentity.model = cl.worldmodel;
	GLR_AnimateLight();
	D3D7_BuildLightmaps();

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

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
void	(D3D7_R_PreNewMap)				(void)
{
	r_viewleaf = NULL;
	r_oldviewleaf = NULL;
	r_viewleaf2 = NULL;
	r_oldviewleaf2 = NULL;
}
int		(D3D7_R_LightPoint)				(vec3_t point)
{
	return 0;
}

void	(D3D7_R_PushDlights)			(void)
{
}
void	(D3D7_R_AddStain)				(vec3_t org, float red, float green, float blue, float radius)
{
}
void	(D3D7_R_LessenStains)			(void)
{
}

void	 (D3D7_VID_DeInit)				(void)
{
	if (pPrimary)
	{
		pPrimary->lpVtbl->Release(pPrimary);
		pPrimary = NULL;
	}
	if (pD3DDev)
	{
		pD3DDev->lpVtbl->Release(pD3DDev);
		pD3DDev = NULL;
	}
	if (pD3D)
	{
		pD3D->lpVtbl->Release(pD3D);
		pD3D = NULL;
	}
	if (pDD)
	{
		pDD->lpVtbl->Release(pDD);
		pDD = NULL;
	}
	if (pD3DX)
	{
		pD3DX->lpVtbl->Release((void*)pD3DX);
		pD3DX = NULL;
	}
	if (mainwindow)
	{
		DestroyWindow(mainwindow);
		mainwindow = NULL;
	}
}
void	(D3D7_VID_LockBuffer)			(void)
{
}
void	(D3D7_VID_UnlockBuffer)			(void)
{
}
void	(D3D7_D_BeginDirectRect)		(int x, int y, qbyte *pbitmap, int width, int height)
{
}
void	(D3D7_D_EndDirectRect)			(int x, int y, int width, int height)
{
}
void	(D3D7_VID_ForceLockState)		(int lk)
{
}
int		(D3D7_VID_ForceUnlockedAndReturnState) (void)
{
	return 0;
}

void	(D3D7_VID_SetPalette)			(unsigned char *palette)
{
	D3D7_VID_GenPaletteTables(palette);
}
void	(D3D7_VID_ShiftPalette)			(unsigned char *palette)
{
	D3D7_VID_GenPaletteTables(palette);
}
char	*(D3D7_VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight)
{
	return NULL;
}
void	(D3D7_VID_SetWindowCaption)		(char *msg)
{
	SetWindowText(mainwindow, msg);
}

void d3d7_ortho(float *m)
{
	D3DXMatrixOrthoOffCenter((D3DXMATRIX*)m, 0, vid.width, vid.height, 0, -100, 100);
}

void D3D7_Set2D (void)
{
	int r;
	float m[16];
	D3DVIEWPORT7 vport;
//	pD3DDev->lpVtbl->EndScene(pD3DDev);

	D3DXMatrixOrthoOffCenter((D3DXMATRIX*)m, 0, vid.width, vid.height, 0, -100, 100);
	r = pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_PROJECTION, (D3DMATRIX*)m);

	D3DXMatrixIdentity((D3DXMATRIX*)m);
	r = pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX*)m);

	D3DXMatrixIdentity((D3DXMATRIX*)m);
	pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_VIEW, (D3DMATRIX*)m);

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_CULLMODE, D3DCULL_CCW );

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZENABLE, D3DZB_FALSE);

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_MAGFILTER,  D3DTFG_LINEAR);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_MAGFILTER,  D3DTFG_LINEAR);

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_MIPFILTER,  D3DTFP_LINEAR);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_MIPFILTER,  D3DTFP_LINEAR);

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_MINFILTER,  D3DTFN_LINEAR);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_MINFILTER,  D3DTFN_LINEAR);

	vport.dwX = 0;
	vport.dwY = 0;
	pD3DX->lpVtbl->GetBufferSize((void*)pD3DX, &vport.dwWidth, &vport.dwHeight);
	vport.dvMinZ = 0;
	vport.dvMaxZ = 1;
	pD3DDev->lpVtbl->SetViewport(pD3DDev, &vport);

//	pD3DDev->lpVtbl->BeginScene(pD3DDev);
}

void D3D7_GetBufferSize(int *width, int *height)
{
	pD3DX->lpVtbl->GetBufferSize((void*)pD3DX, width, height);
}


void	(D3D7_SCR_UpdateScreen)			(void)
{
	extern cvar_t vid_conheight;
	int uimenu;
#ifdef TEXTEDITOR
	extern qboolean editormodal;
#endif
	qboolean nohud;
	RSpeedMark();

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
			pD3DDev->lpVtbl->BeginScene(pD3DDev);
			scr_drawloading = true;
			SCR_DrawLoading ();
			scr_drawloading = false;
			pD3DDev->lpVtbl->EndScene(pD3DDev);
			pD3DX->lpVtbl->UpdateFrame((void*)pD3DX, 0);
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
		{
			DWORD w, h;
			pD3DX->lpVtbl->GetBufferSize((void*)pD3DX, &w, &h);
			if (vid.conwidth <= 0)
				vid.conwidth = w;
			if (vid.conheight <= 0)
				vid.conheight = h;
		}
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

	pD3DDev->lpVtbl->BeginScene(pD3DDev);
	D3D7_Set2D ();
/*
#ifdef TEXTEDITOR
	if (editormodal)
	{
		Editor_Draw();
		GLV_UpdatePalette (false, host_frametime);
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
//		GLV_UpdatePalette (false, host_frametime);
#if defined(_WIN32)
		Media_RecordFrame();
#endif
//		GLR_BrightenScreen();
		pD3DDev->lpVtbl->EndScene(pD3DDev);
		pD3DX->lpVtbl->UpdateFrame((void*)pD3DX, 0);

		pD3DX->lpVtbl->SetClearColor((void*)pD3DX, rand());
		pD3DX->lpVtbl->Clear((void*)pD3DX, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER);

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


	D3D7_Set2D ();

//	GLR_BrightenScreen();

	if (!nohud)
		SCR_TileClear ();

	SCR_DrawTwoDimensional(uimenu, nohud);

//	GLV_UpdatePalette (false, host_frametime);
#if defined(_WIN32) && defined(RGLQUAKE)
	Media_RecordFrame();
#endif

	RSpeedEnd(RSPEED_TOTALREFRESH);
	RSpeedShow();



	pD3DDev->lpVtbl->EndScene(pD3DDev);
	pD3DX->lpVtbl->UpdateFrame((void*)pD3DX, 0);


	pD3DX->lpVtbl->SetClearColor((void*)pD3DX, rand());
	pD3DX->lpVtbl->Clear((void*)pD3DX, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER);


	window_center_x = (window_rect.left + window_rect.right)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;



	IN_UpdateGrabs(modestate != MS_WINDOWED, ActiveApp);
}





mpic_t	*(D3D7_Draw_SafePicFromWad)			(char *name);
mpic_t	*(D3D7_Draw_CachePic)			(char *path);
mpic_t	*(D3D7_Draw_SafeCachePic)		(char *path);
void	(D3D7_Draw_Init)				(void);
void	(D3D7_Draw_ReInit)				(void);
void	(D3D7_Draw_Character)			(int x, int y, unsigned int num);
void	(D3D7_Draw_ColouredCharacter)	(int x, int y, unsigned int num);
void	(D3D7_Draw_String)				(int x, int y, const qbyte *str);
void	(D3D7_Draw_Alt_String)			(int x, int y, const qbyte *str);
void	(D3D7_Draw_Crosshair)			(void);
void	(D3D7_Draw_DebugChar)			(qbyte num);
void	(D3D7_Draw_Pic)					(int x, int y, mpic_t *pic);
void	(D3D7_Draw_ScalePic)			(int x, int y, int width, int height, mpic_t *pic);
void	(D3D7_Draw_SubPic)				(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void	(D3D7_Draw_TransPic)			(int x, int y, mpic_t *pic);
void	(D3D7_Draw_TransPicTranslate)	(int x, int y, int w, int h, qbyte *pic, qbyte *translation);
void	(D3D7_Draw_ConsoleBackground)	(int lines);
void	(D3D7_Draw_EditorBackground)	(int lines);
void	(D3D7_Draw_TileClear)			(int x, int y, int w, int h);
void	(D3D7_Draw_Fill)				(int x, int y, int w, int h, int c);
void	(D3D7_Draw_FillRGB)				(int x, int y, int w, int h, float r, float g, float b);
void	(D3D7_Draw_FadeScreen)			(void);
void	(D3D7_Draw_BeginDisc)			(void);
void	(D3D7_Draw_EndDisc)				(void);

void	(D3D7_Draw_Image)				(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);	//gl-style scaled/coloured/subpic
void	(D3D7_Draw_ImageColours)		(float r, float g, float b, float a);

void	(D3D7_R_Init)					(void);
void	(D3D7_R_DeInit)					(void);
void	(D3D7_R_ReInit)					(void);
void	(D3D7_R_RenderView)				(void);		// must set r_refdef first

qboolean	(D3D7_R_CheckSky)			(void);
void	(D3D7_R_SetSky)					(char *name, float rotate, vec3_t axis);

void	(D3D7_R_NewMap)					(void);
void	(D3D7_R_PreNewMap)				(void);
int		(D3D7_R_LightPoint)				(vec3_t point);

void	(D3D7_R_PushDlights)			(void);
void	(D3D7_R_AddStain)				(vec3_t org, float red, float green, float blue, float radius);
void	(D3D7_R_LessenStains)			(void);

void (D3D7_Media_ShowFrameBGR_24_Flip)	(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...
void (D3D7_Media_ShowFrameRGBA_32)		(qbyte *framedata, int inwidth, int inheight);	//top down
void (D3D7_Media_ShowFrame8bit)			(qbyte *framedata, int inwidth, int inheight, qbyte *palette);	//paletted topdown (framedata is 8bit indexes into palette)

void	(D3D7_Mod_Init)					(void);
void	(D3D7_Mod_ClearAll)				(void);
struct model_s *(D3D7_Mod_ForName)		(char *name, qboolean crash);
struct model_s *(D3D7_Mod_FindName)		(char *name);
void	*(D3D7_Mod_Extradata)			(struct model_s *mod);	// handles caching
void	(D3D7_Mod_TouchModel)			(char *name);

void	(D3D7_Mod_NowLoadExternal)		(void);
void	(D3D7_Mod_Think)				(void);
int (D3D7_Mod_TagNumForName)			(struct model_s *model, char *name);
int (D3D7_Mod_SkinForName)				(struct model_s *model, char *name);


qboolean (D3D7_VID_Init)				(rendererstate_t *info, unsigned char *palette);
void	 (D3D7_VID_DeInit)				(void);
void	(D3D7_VID_LockBuffer)			(void);
void	(D3D7_VID_UnlockBuffer)			(void);
void	(D3D7_D_BeginDirectRect)		(int x, int y, qbyte *pbitmap, int width, int height);
void	(D3D7_D_EndDirectRect)			(int x, int y, int width, int height);
void	(D3D7_VID_ForceLockState)		(int lk);
int		(D3D7_VID_ForceUnlockedAndReturnState) (void);
void	(D3D7_VID_SetPalette)			(unsigned char *palette);
void	(D3D7_VID_ShiftPalette)			(unsigned char *palette);
char	*(D3D7_VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight);
void	(D3D7_VID_SetWindowCaption)		(char *msg);

void	(D3D7_SCR_UpdateScreen)			(void);







rendererinfo_t d3d7rendererinfo =
{
	"Direct3D7 Native",
	{
		"D3D7"
	},
	QR_DIRECT3D,

	D3D7_Draw_SafePicFromWad,
	D3D7_Draw_CachePic,
	D3D7_Draw_SafeCachePic,
	D3D7_Draw_Init,
	D3D7_Draw_ReInit,
	D3D7_Draw_Character,
	D3D7_Draw_ColouredCharacter,
	NULL,
	D3D7_Draw_String,
	D3D7_Draw_Alt_String,
	D3D7_Draw_Crosshair,
	D3D7_Draw_DebugChar,
	D3D7_Draw_Pic,
	D3D7_Draw_ScalePic,
	D3D7_Draw_SubPic,
	D3D7_Draw_TransPic,
	D3D7_Draw_TransPicTranslate,
	D3D7_Draw_ConsoleBackground,
	D3D7_Draw_EditorBackground,
	D3D7_Draw_TileClear,
	D3D7_Draw_Fill,
	D3D7_Draw_FillRGB,
	D3D7_Draw_FadeScreen,
	D3D7_Draw_BeginDisc,
	D3D7_Draw_EndDisc,

	D3D7_Draw_Image,
	D3D7_Draw_ImageColours,

	D3D7_R_Init,
	D3D7_R_DeInit,
	D3D7_R_ReInit,
	D3D7_R_RenderView,

	D3D7_R_CheckSky,
	D3D7_R_SetSky,

	D3D7_R_NewMap,
	D3D7_R_PreNewMap,
	D3D7_R_LightPoint,

	D3D7_R_PushDlights,
	D3D7_R_AddStain,
	D3D7_R_LessenStains,

	D3D7_Media_ShowFrameBGR_24_Flip,
	D3D7_Media_ShowFrameRGBA_32,
	D3D7_Media_ShowFrame8bit,

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
	Mod_SkinNumForName,
	Mod_FrameNumForName,
	Mod_FrameDuration,


	D3D7_VID_Init,
	D3D7_VID_DeInit,
	D3D7_VID_LockBuffer,
	D3D7_VID_UnlockBuffer,
	D3D7_D_BeginDirectRect,
	D3D7_D_EndDirectRect,
	D3D7_VID_ForceLockState,
	D3D7_VID_ForceUnlockedAndReturnState,
	D3D7_VID_SetPalette,
	D3D7_VID_ShiftPalette,
	D3D7_VID_GetRGBInfo,
	D3D7_VID_SetWindowCaption,

	D3D7_SCR_UpdateScreen

};
#endif
