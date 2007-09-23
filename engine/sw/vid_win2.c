
#include "quakedef.h"
#include "winquake.h"
#include "r_local.h"
#include "d_local.h"


HWND		mainwindow;
int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

#ifdef MGL
qboolean usingmgl=true;
#endif

qboolean VID_AllocBuffers (int width, int height, int bpp);

qboolean	DDActive;

qbyte		vid_curpal[256*3];

//cvar_t	_windowed_mouse = {"_windowed_mouse","1", true};
float usingstretch;


qboolean vid_initializing;
extern qboolean vid_isfullscreen;

static qbyte		*vid_surfcache;
static int		vid_surfcachesize;

void SWimp_AppActivate( qboolean active );
void SWimp_Shutdown( void );

qboolean DIB_Init( unsigned char **ppbuffer, int *ppitch );
void DIB_SwapBuffers(void);
void DIB_Shutdown( void );
void DIB_SetPalette( const unsigned char *_pal );

qboolean DDRAW_Init(rendererstate_t *info, unsigned char **ppbuffer, int *ppitch );
void DDRAW_SwapBuffers (void);
void DDRAW_Shutdown(void);
void DDRAW_SetPalette( const unsigned char *pal );

extern int r_flushcache;

// extra button defines
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

void R_GammaCorrectAndSetPalette(const unsigned char *pal)
{
	int i;
	int v;
	int r,g,b, bgr;
	int *table, *bgrt;
	unsigned short *pal16 = d_8to16table;
	qbyte *cp = vid_curpal;

	if (r_pixbytes == 2)	//16 bit needs a 16 bit colormap.
	{
		extern qbyte gammatable[256];
		int j, i;
		int r, g, b;
		float f;
		unsigned short *data;
		r_flushcache++;
		if (!vid.colormap16)
			vid.colormap16 = BZ_Malloc(sizeof(short)*256*VID_GRADES+sizeof(int));
		data = vid.colormap16;
		for (j = 0; j < VID_GRADES; j++)
		{
			f = (1 - ((float)j/VID_GRADES));
			f = (float)gammatable[(int)(f*255)]/255;
			f *= 2;
			for (i = 0; i < 256; i++)
			{
				r = pal[i*3+0]*f;
				g = pal[i*3+1]*f;
				b = pal[i*3+2]*f;
				if (r > 255)
					r = 255;
				if (g > 255)
					g = 255;
				if (b > 255)
					b = 255;
				r >>= 8 - redbits;
				g >>= 8 - greenbits;
				b >>= 8 - bluebits;
				data[i] = (r<<redshift) + (g<<greenshift) + (b<<blueshift);
			}
			data+=256;
		}
	}


	table = d_8to24rgbtable;
	bgrt = d_8to24bgrtable;
	for (i=0 ; i<256 ; i++)
	{
		r = cp[0] = pal[0];
		g = cp[1] = pal[1];
		b = cp[2] = pal[2];
		cp += 3;
		pal += 3;
		
//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;

		bgr = (255<<24) + (r<<16) + (g<<8) + (b<<0);
		*bgrt++ = bgr;
		*pal16++ = (((r*(1<<redbits))/256)<<redshift) + (((g*(1<<greenbits))/256)<<greenshift) + (((b*(1<<bluebits))/256)<<blueshift);
	}
	d_8to24rgbtable[255] &= 0xffffff;	// 255 is transparent
	d_8to24bgrtable[255] &= 0xffffff;	// 255 is transparent
}

void SWAppActivate(BOOL fActive, BOOL minimize)
{
	qboolean newa;
	Minimized = minimize;

	// we don't want to act like we're active if we're minimized
	if (fActive && !Minimized)
		newa = true;
	else
		newa = false;

	if (ActiveApp == newa)	//don't pause and resume these too often.
		return;
	ActiveApp = newa;

	Key_ClearStates();

	// minimize/restore mouse-capture on demand
	if (!ActiveApp)
	{		
		CDAudio_Pause ();
		S_BlockSound ();

/*		if ( win_noalttab->value )
		{
			WIN_EnableAltTab();
		}
*/	}
	else
	{		
		CDAudio_Resume ();
		S_UnblockSound ();
/*		if ( win_noalttab->value )
		{
			WIN_DisableAltTab();
		}
*/	}
}

void VID2_UpdateWindowStatus (void)
{

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* main window procedure */
LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
	LONG			lRet = 0;
	int				fActive, fMinimized, temp;
	HDC				hdc;
	PAINTSTRUCT		ps;
	extern unsigned int uiWheelMessage;
//	static int		recursiveflag;

	if ( uMsg == uiWheelMessage ) {
		uMsg = WM_MOUSEWHEEL;
		wParam <<= 16;
	}

	switch (uMsg)
	{
		case WM_CREATE:
			break;

		case WM_SYSCOMMAND:

		// Check for maximize being hit
			switch (wParam & ~0x0F)
			{
				case SC_MAXIMIZE:
					Cbuf_AddText("vid_fullscreen 1;vid_restart\n", RESTRICT_LOCAL);
				// if minimized, bring up as a window before going fullscreen,
				// so MGL will have the right state to restore
/*					if (Minimized)
					{
						force_mode_set = true;
						VID_SetMode (vid_modenum, vid_curpal);
						force_mode_set = false;
					}

					VID_SetMode ((int)vid_fullscreen_mode.value, vid_curpal);
*/					break;

                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
					if (vid_isfullscreen)
					{
					// don't call DefWindowProc() because we don't want to start
					// the screen saver fullscreen
						break;
					}

				// fall through windowed and allow the screen saver to start

				default:
//				if (!vid_initializing)
//				{
//					S_BlockSound ();					
//				}

				lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);

//				if (!vid_initializing)
//				{
//					S_UnblockSound ();
//				}
			}
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID2_UpdateWindowStatus ();

//			if ((modestate == MS_WINDOWED) && !in_mode_set && !Minimized)
//				VID_RememberWindowPos ();

			break;

		case WM_SIZE:
			Minimized = false;
			
			if (!(wParam & SIZE_RESTORED))
			{
				if (wParam & SIZE_MINIMIZED)
					Minimized = true;
			}

			if (!Minimized && !vid_initializing && !vid_isfullscreen)
			{
				int nt, nl;
				int nw, nh;
				qboolean move = false;
				RECT r;
				GetClientRect (hWnd, &r);
				nw = (int)((r.right - r.left)/usingstretch)&~3;
				nh = (int)((r.bottom - r.top)/usingstretch)&~3;

				window_width = nw;
				window_height = nh;
				VID2_UpdateWindowStatus();

				if (nw < 320)
				{
					move = true;
					nw = 320;
				}
				if (nh < 200)
				{
					move = true;
					nh = 200;
				}
				if (nh > MAXHEIGHT)
				{
					move = true;
					nh = MAXHEIGHT;
				}
				if ((r.right - r.left) & 3)
					move = true;
				if ((r.bottom - r.top) & 3)
					move = true;

				GetWindowRect (hWnd, &r);
				nl = r.left;
				nt = r.top;
				r.left =0;
				r.top = 0;
				r.right = nw*usingstretch;
				r.bottom = nh*usingstretch;
				AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
				vid.recalc_refdef = true;
				if (move)
					MoveWindow(hWnd, nl, nt, r.right - r.left, r.bottom - r.top, true);
				else
				{
					if (vid.width != nw || vid.height != nh)
					{
						M_RemoveAllMenus();	//can cause probs
						DIB_Shutdown();
						vid.conwidth = vid.width = nw;//vid_stretch.value;
						vid.conheight = vid.height = nh;///vid_stretch.value;
						
						DIB_Init( &vid.buffer, &vid.rowbytes );
						vid.conbuffer = vid.buffer;
						vid.conrowbytes = vid.rowbytes;	

						if (VID_AllocBuffers(vid.width, vid.height, r_pixbytes))
							D_InitCaches (vid_surfcache, vid_surfcachesize);

						SCR_UpdateWholeScreen();
					}
					else
						SCR_UpdateWholeScreen();
				}

			}
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			SWAppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
//			ClearAllStates ();

			break;

		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);

//			if (!in_mode_set && host_initialized)
//				SCR_UpdateWholeScreen ();

			EndPaint(hWnd, &ps);
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
			if (!vid_initializing)
			{
				temp = 0;

				if (wParam & MK_LBUTTON)
					temp |= 1;

				if (wParam & MK_RBUTTON)
					temp |= 2;

				if (wParam & MK_MBUTTON)
					temp |= 4;

				// extra buttons
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

				IN_MouseEvent (temp);
			}
			break;
		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if ((short) HIWORD(wParam) > 0) {
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			} else {
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;
		case WM_INPUT:
			// raw input handling
			IN_RawInput_MouseRead((HANDLE)lParam);
			break;
		// KJB: Added these new palette functions
/*		case WM_PALETTECHANGED:
			if ((HWND)wParam == hWnd)
				break;
			// Fall through to WM_QUERYNEWPALETTE 
		case WM_QUERYNEWPALETTE:
			hdc = GetDC(NULL);

			if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
				vid_palettized = true;
			else
				vid_palettized = false;

			ReleaseDC(NULL,hdc);

			scr_fullupdate = 0;

			if (vid_initialized && !in_mode_set && windc && MGL_activatePalette(windc,false) && !Minimized)
			{
				VID_SetPalette (vid_curpal);
				InvalidateRect (mainwindow, NULL, false);

			// specifically required if WM_QUERYNEWPALETTE realizes a new palette
				lRet = TRUE;
			}
			break;
*/
/*		case WM_DISPLAYCHANGE:
			if (!in_mode_set && (modestate == MS_WINDOWED) && !vid_fulldib_on_focus_mode)
			{
				force_mode_set = true;
				VID_SetMode (vid_modenum, vid_curpal);
				force_mode_set = false;
			}
			break;
*/
   	    case WM_CLOSE:
		// this causes Close in the right-click task bar menu not to work, but right
		// now bad things happen if Close is handled in that case (garbage and a
		// crash on Win95)
			if (!vid_initializing)
			{
				if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
							MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
				{
					Sys_Quit ();
				}
			}
			break;

		case MM_MCINOTIFY:
            lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;
			
		case WM_MWHOOK:
			MW_Hook_Message (lParam);
			break;

		default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
	        break;
    }

    /* return 0 if handled message, 1 if not */
    return lRet;
}



/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME FULLENGINENAME
#define	WINDOW_TITLE_NAME FULLENGINENAME


void VID_CreateWindow( int width, int height, qboolean fullscreen)
{
	WNDCLASS		wc;
	RECT			r;
	int				x, y, w, h;
	int				exstyle;
	int stylebits;


	if ( fullscreen )
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP;
	}
	else
	{
		exstyle = 0;
		stylebits = WS_OVERLAPPEDWINDOW;
	}

	/* Register the frame class */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = global_hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_GRAYTEXT;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	r.left = 0;
	r.top = 0;
	r.right  = width*usingstretch;
	r.bottom = height*usingstretch;

	AdjustWindowRectEx (&r, stylebits, FALSE, exstyle);

	window_rect = r;

	w = r.right - r.left;
	h = r.bottom - r.top;
	x = 0;//vid_xpos.value;
	y = 0;//vid_ypos.value;

	mainwindow = CreateWindowEx (
		exstyle,
		 WINDOW_CLASS_NAME,
		 WINDOW_TITLE_NAME,
		 stylebits,
		 x, y, w, h,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!mainwindow)
		Sys_Error ( "Couldn't create window");
	
	ShowWindow( mainwindow, SW_SHOWNORMAL );
	UpdateWindow( mainwindow );
	SetForegroundWindow( mainwindow );
	SetFocus( mainwindow );

	window_x = x;
	window_y = y;
	window_width = w;
	window_height = h;

	VID2_UpdateWindowStatus();

	// let the sound and input subsystems know about the new window
//	ri.Vid_NewWindow (width, height);
}

/*
** SWimp_Init
**
** This routine is responsible for initializing the implementation
** specific stuff in a software rendering subsystem.
*/
int SWimp_Init( void *hInstance, void *wndProc )
{
//	sww_state.hInstance = ( HINSTANCE ) hInstance;
//	sww_state.wndproc = wndProc;

	return true;
}

/*
** SWimp_InitGraphics
**
** This initializes the software refresh's implementation specific
** graphics subsystem.  In the case of Windows it creates DIB or
** DDRAW surfaces.
**
** The necessary width and height parameters are grabbed from
** vid.width and vid.height.
*/
static qboolean SWimp_InitGraphics( rendererstate_t *info, qboolean fullscreen )
{
	// free resources in use
	SWimp_Shutdown ();

	usingstretch = info->stretch;
	if (fullscreen || usingstretch < 0.25)
		usingstretch = 1;

	// create a new window
	VID_CreateWindow (vid.width, vid.height, fullscreen);

	// initialize the appropriate subsystem
	if ( !fullscreen )
	{
		if (COM_CheckParm("-nodib"))
		{
			vid.buffer = 0;
			vid.rowbytes = 0;
			return false;
		}

		if ( !DIB_Init( &vid.buffer, &vid.rowbytes ) )
		{
			vid.buffer = 0;
			vid.rowbytes = 0;

			return false;
		}
	}
	else
	{
		if (COM_CheckParm("-noddraw"))
		{
			vid.buffer = 0;
			vid.rowbytes = 0;
			return false;
		}

		if ( !DDRAW_Init( info, &vid.buffer, &vid.rowbytes ) )
		{
			vid.buffer = 0;
			vid.rowbytes = 0;

			return false;
		}
	}

	vid.conbuffer = vid.buffer;
	vid.conrowbytes = vid.rowbytes;

	return true;
}

/*
** SWimp_EndFrame
**
** This does an implementation specific copy from the backbuffer to the
** front buffer.  In the Win32 case it uses BitBlt or BltFast depending
** on whether we're using DIB sections/GDI or DDRAW.
*/
void SWimp_EndFrame (void)
{
	if ( !vid_isfullscreen )
	{
		DIB_SwapBuffers ();
	}
	else
	{
		DDRAW_SwapBuffers();
	}

	vid.conbuffer = vid.buffer;
	vid.conrowbytes = vid.rowbytes;
}

qboolean VID_AllocBuffers (int width, int height, int bpp)
{
	int		tsize, tbuffersize;
	if (!bpp)
		bpp = 1;

	tbuffersize = width * height * bpp * sizeof (*d_pzbuffer);

	tsize = D_SurfaceCacheForRes (width, height, bpp);

	tbuffersize += tsize;

// see if there's enough memory, allowing for the normal mode 0x13 pixel,
// z, and surface buffers
/*	if (host_parms.memsize - (Hunk_LowMark() + VID_highhunkmark) < tbuffersize &&
	 (host_parms.memsize - tbuffersize + bpp*(SURFCACHE_SIZE_AT_320X200 +
		 0x10000 * 3)) < MINIMUM_MEMORY)
	{
		Con_SafePrintf ("Not enough memory for video mode\n");
		return false;		// not enough memory for mode
	}
*/
	vid_surfcachesize = tsize;

	if (d_pzbuffer)
	{
		D_FlushCaches ();
//		Hunk_FreeToHighMark (VID_highhunkmark);
		BZ_Free(d_pzbuffer);
		d_pzbuffer = NULL;
	}

//	VID_highhunkmark = Hunk_HighMark ();

//	d_pzbuffer = Hunk_HighAllocName (tbuffersize, "video");
	d_pzbuffer = BZ_Malloc(tbuffersize);

	if (!d_pzbuffer)
		return false;

	vid_surfcache = (qbyte *)d_pzbuffer +
			width * height * bpp * sizeof (*d_pzbuffer);
	
	return true;
}

/*
** SWimp_SetMode
*/
typedef enum {err_ok, err_invalid_mode, err_invalid_fullscreen, err_unknown} err_t;
err_t SWimp_SetMode( rendererstate_t *info )
{
	const char *win_fs[] = { "W", "FS" };
	err_t retval = err_ok;

	qboolean fullscreen = info->fullscreen;

	Con_SafePrintf ( "setting mode %i*%i:", info->width, info->height );

	vid.conwidth = info->width;
	vid.conheight = info->height;
	vid.width = info->width;
	vid.height = info->height;

	Con_SafePrintf( " %d %d %s\n", info->width, info->height, win_fs[info->fullscreen!=0] );	

	vid_initializing = true;
	if ( fullscreen )
	{
		if ( !SWimp_InitGraphics( info, true ) )
		{
			if ( SWimp_InitGraphics( info, false ) )
			{
				// mode is legal but not as fullscreen
				fullscreen = false;
				retval = err_ok;
			}
			else
			{
				// failed to set a valid mode in windowed mode
				vid_initializing = true;
				return err_unknown;
			}
		}
	}
	else
	{
		// failure to set a valid mode in windowed mode
		if ( !SWimp_InitGraphics( info, false ) )
		{
			vid_initializing = true;
			return err_unknown;
		}
	}

	VID_AllocBuffers(info->width, info->height, r_pixbytes);
	D_InitCaches (vid_surfcache, vid_surfcachesize);

	vid.conbuffer = vid.buffer;

	vid_isfullscreen = fullscreen;
#if 0
	if ( retval != rserr_unknown )
	{
		if ( retval == rserr_invalid_fullscreen ||
			 ( retval == rserr_ok && !fullscreen ) )
		{
			SetWindowLong( sww_state.hWnd, GWL_STYLE, WINDOW_STYLE );
		}
	}
#endif
	R_GammaCorrectAndSetPalette( ( const unsigned char * ) vid_curpal );
	vid_initializing = false;

	vid.recalc_refdef = 1;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.aspect = ((float)vid.height / (float)vid.width) *
				(320.0 / 240.0);

	vid.numpages = 1;

	window_center_x = vid.width/2;
	window_center_y = vid.height/2;	

	Cache_Flush();

	return retval;
}

/*
** SWimp_SetPalette
**
** System specific palette setting routine.  A NULL palette means
** to use the existing palette.  The palette is expected to be in
** a padded 4-qbyte xRGB format.
*/
void SWimp_SetPalette( const unsigned char *palette )
{
	// MGL - what the fuck was kendall doing here?!
	// clear screen to black and change palette
	//	for (i=0 ; i<vid.height ; i++)
	//		memset (vid.buffer + i*vid.rowbytes, 0, vid.width);

	if ( !palette )
		palette = ( const unsigned char * ) vid_curpal;

	R_GammaCorrectAndSetPalette(palette);

	if ( !vid_isfullscreen )
	{
		DIB_SetPalette( ( const unsigned char * ) palette );
	}
	else
	{
		DDRAW_SetPalette( ( const unsigned char * ) palette );
	}
}

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.  Destroys
** DIBs or DDRAW surfaces as appropriate.
*/
void SWimp_Shutdown( void )
{
	Con_SafePrintf( "Shutting down SW imp\n" );
	DIB_Shutdown();
	DDRAW_Shutdown();

	if ( mainwindow )
	{
		Con_SafePrintf( "...destroying window\n" );
		ShowWindow( mainwindow, SW_SHOWNORMAL );	// prevents leaving empty slots in the taskbar
		DestroyWindow (mainwindow);
		mainwindow = NULL;
		UnregisterClass (WINDOW_CLASS_NAME, global_hInstance);
	}
}

//===============================================================================
qboolean SWVID_Init (rendererstate_t *info, unsigned char *palette)
{
#ifdef MGL
	if (usingmgl)
	{
		MGL_Init(palette);
		return true;
	}
#endif

	if (info->height > MAXHEIGHT)
		return false;

	usingstretch = info->stretch;
	if (usingstretch <= 0.25)
		usingstretch = 1;

	if (info->bpp >= 32)
		r_pixbytes = 4;
	else if (info->bpp >= 16)
		r_pixbytes = 2;
	else
		r_pixbytes = 1;

	vid.colormap = host_colormap;	

	SWimp_Init(global_hInstance, MainWndProc);

	if (hwnd_dialog)
		DestroyWindow (hwnd_dialog);

	if (SWimp_SetMode(info))
	{
		return false;
	}
	SWimp_SetPalette(palette);

	S_Restart_f ();

	return true;
}

void	SWVID_Shutdown (void)
{
#ifdef MGL
	if (usingmgl)
	{
		MGL_Shutdown();
		return;
	}
#endif
	IN_DeactivateMouse();
	IN_ShowMouse();
	SWimp_Shutdown();
}

void	SWVID_Update (vrect_t *rects)	//end frame...
{
	extern qboolean mouseactive;
	qboolean mouse;
#ifdef MGL
	if (usingmgl)
	{
		MGL_Update(rects);
		return;
	}
#endif
	SWimp_EndFrame();

	// handle the mouse state when windowed if that's changed
	if (!vid_isfullscreen)
	{
		mouse = false;
		if (_windowed_mouse.value)
			if (key_dest == key_game)// || key_dest == key_menu)
				mouse = true;
	}
	else
	{
		if (key_dest == key_menu)
			mouse = false;
		else
			mouse = true;
	}
	if (!ActiveApp)
		mouse = false;	//nope can't have it.
	if (mouse != mouseactive)
	{
		if (mouse)
		{
			IN_ActivateMouse();
			IN_HideMouse();
			IN_UpdateClipCursor();
		}
		else
		{
			IN_DeactivateMouse();
			IN_ShowMouse();
		}
	}
}

void SWVID_SetCaption(char *text)
{
	SetWindowText(mainwindow, text);
}

void SWVID_LockBuffer (void)	//ignored
{
#ifdef MGL
	if (usingmgl)
	{
		MGL_LockBuffer();
		return;
	}
#endif
}
void SWVID_UnlockBuffer (void)	//ignored
{
#ifdef MGL
	if (usingmgl)
	{
		MGL_UnlockBuffer();
		return;
	}
#endif
}

void SWD_BeginDirectRect (int x, int y, qbyte *pbitmap, int width, int height)
{
#ifdef MGL
	if (usingmgl)
	{
		MGL_BeginDirectRect(x, y, pbitmap, width, height);
		return;
	}
#endif
}
void SWD_EndDirectRect (int x, int y, int width, int height)
{
#ifdef MGL
	if (usingmgl)
	{
		MGL_EndDirectRect(x, y, width, height);
		return;
	}
#endif
}

void SWVID_ForceLockState (int lk)
{
#ifdef MGL
	if (usingmgl)
	{
		MGL_ForceLockState(lk);
		return;
	}
#endif
}
int SWVID_ForceUnlockedAndReturnState (void)
{
#ifdef MGL
	if (usingmgl)
	{
		return MGL_ForceUnlockedAndReturnState();
	}
#endif
	return 0;
}

void	SWVID_SetPalette (unsigned char *palette)
{
#ifdef MGL
	if (usingmgl)
	{
		VMGL_SetPalette(palette);
		return;
	}
#endif
	SWimp_SetPalette(palette);	
}

void	SWVID_ShiftPalette (unsigned char *palette)
{
#ifdef MGL
	if (usingmgl)
	{
		MGL_ShiftPalette(palette);
		return;
	}
#endif
	SWimp_SetPalette(palette);
}





