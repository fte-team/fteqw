
/*
** This handles DIB section management under Windows.
*/
#include "quakedef.h"
#include "winquake.h"
#include "r_local.h"

void DIB_Shutdown( void );

#ifndef _WIN32
#  error You should not be trying to compile this file on this platform
#endif

HDC mainhDC, hdcDIBSection;
HBITMAP hDIBSection;
HPALETTE hPal, hpalOld;
qbyte *pDIBBase;
extern qboolean vid_palettized;
extern cvar_t vid_use32bit;

static qboolean s_systemcolors_saved;

extern int redbits, redshift;
extern int greenbits, greenshift;
extern int bluebits, blueshift;

static HGDIOBJ previously_selected_GDI_obj;

static int s_syspalindices[] = 
{
  COLOR_ACTIVEBORDER,
  COLOR_ACTIVECAPTION,
  COLOR_APPWORKSPACE,
  COLOR_BACKGROUND,
  COLOR_BTNFACE,
  COLOR_BTNSHADOW,
  COLOR_BTNTEXT,
  COLOR_CAPTIONTEXT,
  COLOR_GRAYTEXT,
  COLOR_HIGHLIGHT,
  COLOR_HIGHLIGHTTEXT,
  COLOR_INACTIVEBORDER,

  COLOR_INACTIVECAPTION,
  COLOR_MENU,
  COLOR_MENUTEXT,
  COLOR_SCROLLBAR,
  COLOR_WINDOW,
  COLOR_WINDOWFRAME,
  COLOR_WINDOWTEXT
};

#define NUM_SYS_COLORS ( sizeof( s_syspalindices ) / sizeof( int ) )

static COLORREF s_oldsyscolors[NUM_SYS_COLORS];

typedef struct dibinfo
{
	BITMAPINFOHEADER	header;
	RGBQUAD				acolors[256];
} dibinfo_t;

typedef struct
{
	WORD palVersion;
	WORD palNumEntries;
	PALETTEENTRY palEntries[256];
} identitypalette_t;

static identitypalette_t s_ipal;

static void DIB_SaveSystemColors( void );
static void DIB_RestoreSystemColors( void );

/*
** DIB_Init
**
** Builds our DIB section
*/
qboolean DIB_Init( unsigned char **ppbuffer, int *ppitch )
{
	dibinfo_t   dibheader;
	BITMAPINFO *pbmiDIB = ( BITMAPINFO * ) &dibheader;
	int i;

	memset( &dibheader, 0, sizeof( dibheader ) );

	/*
	** grab a DC
	*/
	if ( !mainhDC )
	{
		if ( ( mainhDC = GetDC( mainwindow ) ) == NULL )
			return false;
	}

	/*
	** figure out if we're running in an 8-bit display mode
	*/
 	if ( GetDeviceCaps( mainhDC, RASTERCAPS ) & RC_PALETTE )
	{
		vid_palettized = true;

		// save system colors
		if ( !s_systemcolors_saved )
		{
			DIB_SaveSystemColors();
			s_systemcolors_saved = true;
		}
	}
	else
	{
		vid_palettized = false;
	}

	/*
	** fill in the BITMAPINFO struct
	*/
	pbmiDIB->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
	pbmiDIB->bmiHeader.biWidth         = vid.width;
	pbmiDIB->bmiHeader.biHeight        = vid.height;
	pbmiDIB->bmiHeader.biPlanes        = 1;
	pbmiDIB->bmiHeader.biBitCount      = r_pixbytes*8;
	pbmiDIB->bmiHeader.biCompression   = BI_RGB;
	pbmiDIB->bmiHeader.biSizeImage     = 0;
	pbmiDIB->bmiHeader.biXPelsPerMeter = 0;
	pbmiDIB->bmiHeader.biYPelsPerMeter = 0;
	pbmiDIB->bmiHeader.biClrUsed       = 0;
	pbmiDIB->bmiHeader.biClrImportant  = 0;

	/*
	** fill in the palette
	*/
	for ( i = 0; i < 256; i++ )
	{
		dibheader.acolors[i].rgbRed   = ( d_8to24rgbtable[i] >> 0 )  & 0xff;
		dibheader.acolors[i].rgbGreen = ( d_8to24rgbtable[i] >> 8 )  & 0xff;
		dibheader.acolors[i].rgbBlue  = ( d_8to24rgbtable[i] >> 16 ) & 0xff;
	}

	/*
	** create the DIB section
	*/
	hDIBSection = CreateDIBSection( mainhDC,
		                                     pbmiDIB,
											 DIB_RGB_COLORS,
											 (void**)&pDIBBase,
											 NULL,
											 0 );

	if ( hDIBSection == NULL )
	{
		Con_Printf( "DIB_Init() - CreateDIBSection failed\n" );
		goto fail;
	}

	if ( pbmiDIB->bmiHeader.biHeight > 0 )
    {
		// bottom up
		*ppbuffer	= pDIBBase + ( vid.height - 1 ) * vid.width * r_pixbytes;
		*ppitch		= -(int)vid.width;
    }
    else
    {
		// top down
		*ppbuffer	= pDIBBase;
		*ppitch		= vid.width;
    }

	/*
	** clear the DIB memory buffer
	*/
	memset( pDIBBase, 0xff, vid.width * vid.height * r_pixbytes);

	if ( ( hdcDIBSection = CreateCompatibleDC( mainhDC ) ) == NULL )
	{
		Con_Printf( "DIB_Init() - CreateCompatibleDC failed\n" );
		goto fail;
	}
	if ( ( previously_selected_GDI_obj = SelectObject( hdcDIBSection, hDIBSection ) ) == NULL )
	{
		Con_Printf( "DIB_Init() - SelectObject failed\n" );
		goto fail;
	}

	redbits		= 5; redshift=10;
	greenbits	= 5; greenshift=5;
	bluebits	= 5; blueshift=0;

	return true;

fail:
	DIB_Shutdown();
	return false;
	
}

void DIB_Resized(void)
{
}

/*
** DIB_SetPalette
**
** Sets the color table in our DIB section, and also sets the system palette
** into an identity mode if we're running in an 8-bit palettized display mode.
**
** The palette is expected to be 1024 bytes, in the format:
**
** R = offset 0
** G = offset 1
** B = offset 2
** A = offset 3
*/
void DIB_SetPalette( const unsigned char *_pal )
{
	const unsigned char *pal = _pal;
  	LOGPALETTE		*pLogPal = ( LOGPALETTE * ) &s_ipal;
	RGBQUAD			colors[256];
	int				i;
	int				ret;
	HDC				hDC = mainhDC;

	/*
	** set the DIB color table
	*/
	if (r_pixbytes == 1 && hdcDIBSection )
	{
		for ( i = 0; i < 256; i++, pal += 3 )
		{
			colors[i].rgbRed   = pal[0];
			colors[i].rgbGreen = pal[1];
			colors[i].rgbBlue  = pal[2];
			colors[i].rgbReserved = 0;
		}

		/*
		colors[0].rgbRed = 0;
		colors[0].rgbGreen = 0;
		colors[0].rgbBlue = 0;

		colors[255].rgbRed = 0xff;
		colors[255].rgbGreen = 0xff;
		colors[255].rgbBlue = 0xff;
		*/

		if ( SetDIBColorTable( hdcDIBSection, 0, 256, colors ) == 0 )
		{
			Con_Printf( "DIB_SetPalette() - SetDIBColorTable failed\n" );
		}
	}

	/*
	** for 8-bit color desktop modes we set up the palette for maximum
	** speed by going into an identity palette mode.
	*/
	if ( vid_palettized )
	{
		int i;
		HPALETTE hpalOld;

		if ( SetSystemPaletteUse( hDC, SYSPAL_NOSTATIC ) == SYSPAL_ERROR )
		{
			Sys_Error( "DIB_SetPalette() - SetSystemPaletteUse() failed\n" );
		}

		/*
		** destroy our old palette
		*/
		if ( hPal )
		{
			DeleteObject( hPal );
			hPal = 0;
		}

		/*
		** take up all physical palette entries to flush out anything that's currently
		** in the palette
		*/
		pLogPal->palVersion		= 0x300;
		pLogPal->palNumEntries	= 256;

		for ( i = 0, pal = _pal; i < 256; i++, pal += 3 )
		{
			pLogPal->palPalEntry[i].peRed	= pal[0];
			pLogPal->palPalEntry[i].peGreen	= pal[1];
			pLogPal->palPalEntry[i].peBlue	= pal[2];
			pLogPal->palPalEntry[i].peFlags	= PC_RESERVED | PC_NOCOLLAPSE;
		}
		/* if they're using 8-bpp desktop with 8-bpp renderer keep black/white
		constant so windows is partially usable? */
		pLogPal->palPalEntry[0].peRed		= 0;
		pLogPal->palPalEntry[0].peGreen		= 0;
		pLogPal->palPalEntry[0].peBlue		= 0;
		pLogPal->palPalEntry[0].peFlags		= 0;
		pLogPal->palPalEntry[255].peRed		= 0xff;
		pLogPal->palPalEntry[255].peGreen	= 0xff;
		pLogPal->palPalEntry[255].peBlue	= 0xff;
		pLogPal->palPalEntry[255].peFlags	= 0;

		if ( ( hPal = CreatePalette( pLogPal ) ) == NULL )
		{
			Sys_Error( "DIB_SetPalette() - CreatePalette failed(%x)\n", GetLastError() );
		}

		if ( ( hpalOld = SelectPalette( hDC, hPal, FALSE ) ) == NULL )
		{
			Sys_Error( "DIB_SetPalette() - SelectPalette failed(%x)\n",GetLastError() );
		}

		/*
		if ( hpalOld == NULL )
			hpalOld = hpalOld;
		*/

		if ( ( ret = RealizePalette( hDC ) ) != pLogPal->palNumEntries ) 
		{
			Sys_Error( "DIB_SetPalette() - RealizePalette set %d entries\n", ret );
		}
	}
}

/*
** DIB_Shutdown
*/
void DIB_Shutdown( void )
{
	if ( vid_palettized && s_systemcolors_saved )
		DIB_RestoreSystemColors();

	if ( hPal )
	{
		DeleteObject( hPal );
		hPal = NULL;
	}

	if ( hpalOld )
	{
		SelectPalette( mainhDC, hpalOld, FALSE );
		RealizePalette( mainhDC );
		hpalOld = NULL;
	}

	if ( hdcDIBSection )
	{
		SelectObject( hdcDIBSection, previously_selected_GDI_obj );
		DeleteDC( hdcDIBSection );
		hdcDIBSection = NULL;
	}

	if ( hDIBSection )
	{
		DeleteObject( hDIBSection );
		hDIBSection = NULL;
		pDIBBase = NULL;
	}

	if ( mainhDC )
	{
		ReleaseDC( mainwindow, mainhDC );
		mainhDC = 0;
	}
}


/*
** DIB_Save/RestoreSystemColors
*/
static void DIB_RestoreSystemColors( void )
{
    SetSystemPaletteUse( mainhDC, SYSPAL_STATIC );
    SetSysColors( NUM_SYS_COLORS, s_syspalindices, s_oldsyscolors );
}

static void DIB_SaveSystemColors( void )
{
	int i;

	for ( i = 0; i < NUM_SYS_COLORS; i++ )
		s_oldsyscolors[i] = GetSysColor( s_syspalindices[i] );
}



void DIB_SwapBuffers(void)
{
	extern float usingstretch;
	if ( vid_palettized )
	{
//		holdpal = SelectPalette(hdcScreen, hpalDIB, FALSE);
//		RealizePalette(hdcScreen);
	}

	if (usingstretch == 1)
		BitBlt( mainhDC,
				0, 0,
				vid.width,
				vid.height,
				hdcDIBSection,
				0, 0,
				SRCCOPY );
	else
		StretchBlt( mainhDC,	//Why is StretchBlt not optimised for a scale of 2? Surly that would be a frequently used quantity?
			0, 0,
			vid.width*usingstretch,
			vid.height*usingstretch,
			hdcDIBSection,
			0, 0,
			vid.width, vid.height,
			SRCCOPY );

	if ( vid_palettized )
	{
//		SelectPalette(hdcScreen, holdpal, FALSE);
	}
}

