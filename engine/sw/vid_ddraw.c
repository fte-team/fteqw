#include "quakedef.h"
#include "winquake.h"

qboolean vid_palettized;
#ifdef AVAIL_DDRAW

HMODULE hinstDDRAW;
LPDIRECTDRAW2 lpDirectDraw;
LPDIRECTDRAWSURFACE lpddsFrontBuffer, lpddsBackBuffer, lpddsOffScreenBuffer;
LPDIRECTDRAWPALETTE	lpddpPalette;
extern qbyte		vid_curpal[];

qboolean modex;
qboolean vid_initialized;


extern int redbits, redshift;
extern int greenbits, greenshift;
extern int bluebits, blueshift;


//end

static const char *DDrawError (int code);

/*
** DDRAW_SetPalette
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
void DDRAW_SetPalette( const unsigned char *pal )
{
	static PALETTEENTRY palentries[256];	//does this help any drivers - being static?
	int i;

	if (!lpddpPalette)
		return;

	for ( i = 0; i < 256; i++, pal += 3 )
	{
		palentries[i].peRed   = pal[0];
		palentries[i].peGreen = pal[1];
		palentries[i].peBlue  = pal[2];
		palentries[i].peFlags = PC_RESERVED | PC_NOCOLLAPSE;
	}
	if ( lpddpPalette->lpVtbl->SetEntries( lpddpPalette,
		                                        0,
												0,
												256,
												palentries ) != DD_OK )
	{
		Con_Printf( "DDRAW_SetPalette() - SetEntries failed\n" );
	}
}



void DDRAW_Shutdown(void)
{
	if ( lpddsOffScreenBuffer )
	{
		Con_SafePrintf( "...releasing offscreen buffer\n");
		lpddsOffScreenBuffer->lpVtbl->Unlock( lpddsOffScreenBuffer, vid.buffer );
		lpddsOffScreenBuffer->lpVtbl->Release( lpddsOffScreenBuffer );
		lpddsOffScreenBuffer = NULL;
	}
	if ( lpddsBackBuffer )
	{
		Con_SafePrintf( "...releasing back buffer\n");
		lpddsBackBuffer->lpVtbl->Release( lpddsBackBuffer );
		lpddsBackBuffer = NULL;
	}
	if ( lpddsFrontBuffer )
	{
		Con_SafePrintf( "...releasing front buffer\n");
		lpddsFrontBuffer->lpVtbl->Release( lpddsFrontBuffer );
		lpddsFrontBuffer = NULL;
	}
	if ( lpddpPalette)
	{
		Con_SafePrintf( "...releasing palette\n");
		lpddpPalette->lpVtbl->Release ( lpddpPalette );
		lpddpPalette = NULL;
	}
	if ( lpDirectDraw )
	{
		Con_SafePrintf( "...restoring display mode\n");
		lpDirectDraw->lpVtbl->RestoreDisplayMode( lpDirectDraw );
		Con_SafePrintf( "...restoring normal coop mode\n");
	    lpDirectDraw->lpVtbl->SetCooperativeLevel( lpDirectDraw, mainwindow, DDSCL_NORMAL );
		Con_SafePrintf( "...releasing lpDirectDraw\n");
		lpDirectDraw->lpVtbl->Release( lpDirectDraw );
		lpDirectDraw = NULL;
	}
	if ( hinstDDRAW )
	{
		Con_SafePrintf( "...freeing library\n");
		FreeLibrary( hinstDDRAW );
		hinstDDRAW = NULL;
	}

}


unsigned short LowBitPos(DWORD dword)
{
	int i;
	for (i = 0; ; i++)
	{
		if (dword & (1<<i))
			return i;
	}
    return 32;
}

unsigned short HighBitPos(DWORD dword)
{
	int i;
	for (i = LowBitPos(dword); ; i++)
	{
		if (!(dword & (1<<i)))
			return i;
	}
    return 32;
}


qboolean DDRAW_Init(rendererstate_t *info, unsigned char **ppbuffer, int *ppitch )
{
	int i;
	PALETTEENTRY palentries[256];
	DDSURFACEDESC ddsd;
	DDSCAPS ddscaps;
	LPDIRECTDRAW lpDirectDraw1;

	HRESULT ddrval;
	HRESULT (WINAPI *QDirectDrawCreate)( GUID FAR *lpGUID, LPVOID  *lplpDD, IUnknown FAR * pUnkOuter );

	if (!hinstDDRAW)
	{
		hinstDDRAW = LoadLibrary( "ddraw.dll" );
		if (!hinstDDRAW)
		{
			Con_Printf( "Failed to load ddraw.dll");
			goto fail;
		}
	}

	if ( ( QDirectDrawCreate = ( HRESULT (WINAPI *)( GUID FAR *, LPVOID *, IUnknown FAR * ) ) GetProcAddress( hinstDDRAW, "DirectDrawCreate" ) ) == NULL )
	{
		Con_Printf( "*** DirectDrawCreate == NULL ***\n" );
		goto fail;
	}

	if ( ( ddrval = QDirectDrawCreate( NULL, &lpDirectDraw1, NULL ) ) != DD_OK )
	{
		Con_Printf( "failed - %s\n", DDrawError( ddrval ) );
		goto fail;
	}

	Con_SafePrintf( "...Using ddraw2: ");
	ddrval=lpDirectDraw1->lpVtbl->QueryInterface(lpDirectDraw1, &IID_IDirectDraw2,(void**)&lpDirectDraw);
	lpDirectDraw1->lpVtbl->Release(lpDirectDraw1);
	if (ddrval != DD_OK)
	{
		Con_Printf( "failed - %s\n", DDrawError( ddrval ) );
		goto fail;
	}
	Con_SafePrintf( "ok\n" );

#if 1
	/*
	** see if linear modes exist first
	*/
	modex = false;

	Con_SafePrintf( "...setting exclusive mode: ");
	if ( ( ddrval = lpDirectDraw->lpVtbl->SetCooperativeLevel( lpDirectDraw, 
																		 mainwindow,
																		DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT) ) != DD_OK )
	{
		Con_SafePrintf( "failed - %s\n",DDrawError (ddrval) );
		goto fail;
	}
	Con_SafePrintf( "ok\n" );

	/*
	** try changing the display mode normally
	*/
	Con_SafePrintf( "...finding display mode\n" );
	Con_SafePrintf( "...setting linear mode: " );

	if ( ( ddrval = lpDirectDraw->lpVtbl->SetDisplayMode( lpDirectDraw, vid.width, vid.height, r_pixbytes*8, info->rate, 0 ) ) == DD_OK )
	{
		Con_SafePrintf( "ok\n" );
	}
	/*
	** if no linear mode found, go for modex if we're trying 320x240
	*/
	else if ( ( vid.width==320 && vid.height==240 ) && info->allow_modex )
	{
		Con_SafePrintf( "failed\n" );
		Con_SafePrintf( "...attempting ModeX 320x240: ");

		/*
		** reset to normal cooperative level
		*/
		lpDirectDraw->lpVtbl->SetCooperativeLevel( lpDirectDraw,
															 mainwindow,
															 DDSCL_NORMAL );

		/*															 
		** set exclusive mode
		*/
		if ( ( ddrval = lpDirectDraw->lpVtbl->SetCooperativeLevel( lpDirectDraw, 
																			 mainwindow,
																			 DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_NOWINDOWCHANGES | DDSCL_ALLOWMODEX | DDSCL_ALLOWREBOOT) ) != DD_OK )
		{
			Con_SafePrintf( "failed SCL - %s\n",DDrawError (ddrval) );
			goto fail;
		}

		/*
		** change our display mode
		*/
		if ( ( ddrval = lpDirectDraw->lpVtbl->SetDisplayMode( lpDirectDraw, vid.width, vid.height, r_pixbytes*8, info->rate, 0 ) ) != DD_OK )
		{
			Con_SafePrintf( "failed SDM - %s\n", DDrawError( ddrval ) );
			goto fail;
		}
		Con_SafePrintf( "ok\n" );

		modex = true;
	}
	else
	{
		Con_SafePrintf( "failed\n" );
		goto fail;
	}

	/*
	** create our front buffer
	*/
	memset( &ddsd, 0, sizeof( ddsd ) );
	ddsd.dwSize = sizeof( ddsd );
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 1;
	ddsd.dwRefreshRate = info->rate;
	if (ddsd.dwRefreshRate)
		ddsd.dwFlags |= DDSD_REFRESHRATE;

	Con_Printf("Rate: %i\n", ddsd.dwRefreshRate);

	Con_SafePrintf( "...creating front buffer: ");
	if ( ( ddrval = lpDirectDraw->lpVtbl->CreateSurface( lpDirectDraw, &ddsd, &lpddsFrontBuffer, NULL ) ) != DD_OK )
	{
		Con_SafePrintf( "failed - %s\n", DDrawError( ddrval ) );
		goto fail;
	}
	Con_SafePrintf( "ok\n" );

	Con_Printf("Rate: %i\n", ddsd.dwRefreshRate);

#else
	Con_SafePrintf( "...setting normal mode: ");
	if ( ( ddrval = lpDirectDraw->lpVtbl->SetCooperativeLevel( lpDirectDraw, 
																		 mainwindow,
																		DDSCL_NORMAL ) ) != DD_OK )
	{
		Con_SafePrintf( "failed - %s\n",DDrawError (ddrval) );
		goto fail;
	}
	Con_SafePrintf( "ok\n" );


	i = r_pixbytes;
	if (vid_use32bit.value)
		r_pixbytes = 4;
	else
		r_pixbytes = 1;

	if (r_pixbytes != i && cls.state)
	{
		Con_Printf("Cannot change bpp when connected\n");
		r_pixbytes = i;
	}

	/*
	** try changing the display mode
	*/
/*	if ( ( ddrval = lpDirectDraw->lpVtbl->SetDisplayMode( lpDirectDraw, vid.width, vid.height, r_pixbytes*8 ) ) != DD_OK )
	{
		Con_SafePrintf( "failed - %s\n",DDrawError (ddrval) );
		goto fail;
	}
	Con_SafePrintf( "ok\n" );
*/

	/*
	** create our front buffer
	*/
	memset( &ddsd, 0, sizeof( ddsd ) );
	ddsd.dwSize = sizeof( ddsd );
	ddsd.dwFlags = /*DDSD_CAPS |*/ DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 1;
	ddsd.dwRefreshRate = info->rate;
	if (ddsd.dwRefreshRate)
		ddsd.dwFlags |= DDSD_REFRESHRATE;
	Con_Printf("Rate: %i\n", ddsd.dwRefreshRate);

	Con_SafePrintf( "...creating front buffer: ");
	if ( ( ddrval = lpDirectDraw->lpVtbl->CreateSurface( lpDirectDraw, &ddsd, &lpddsFrontBuffer, NULL ) ) != DD_OK )
	{
		Con_SafePrintf( "failed - %s\n", DDrawError( ddrval ) );
		goto fail;
	}
	Con_SafePrintf( "ok\n" );

	Con_Printf("Rate: %i\n", ddsd.dwRefreshRate);
#endif

	/*
	** see if we're a ModeX mode
	*/
	lpddsFrontBuffer->lpVtbl->GetCaps( lpddsFrontBuffer, &ddscaps );
	if ( ddscaps.dwCaps & DDSCAPS_MODEX )
		Con_SafePrintf( "...using ModeX\n" );

	/*
	** create our back buffer
	*/
	ddsd.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;

	Con_SafePrintf( "...creating back buffer: " );
	if ( ( ddrval = lpddsFrontBuffer->lpVtbl->GetAttachedSurface( lpddsFrontBuffer, &ddsd.ddsCaps, &lpddsBackBuffer ) ) != DD_OK )
	{
		Con_SafePrintf( "failed - %s\n", DDrawError( ddrval ) );
		goto fail;
	}
	Con_SafePrintf( "ok\n" );

	/*
	** create our rendering buffer
	*/
	memset( &ddsd, 0, sizeof( ddsd ) );
	ddsd.dwSize = sizeof( ddsd );
	ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	ddsd.dwHeight = vid.height;
	ddsd.dwWidth = vid.width;
	ddsd.dwRefreshRate = info->rate;
	if (ddsd.dwRefreshRate)
		ddsd.dwFlags |= DDSD_REFRESHRATE;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;

	Con_SafePrintf( "...creating offscreen buffer: " );
	if ( ( ddrval = lpDirectDraw->lpVtbl->CreateSurface( lpDirectDraw, &ddsd, &lpddsOffScreenBuffer, NULL ) ) != DD_OK )
	{
		Con_SafePrintf( "failed - %s\n", DDrawError( ddrval ) );
		goto fail;
	}
	Con_SafePrintf( "ok\n" );

	if (r_pixbytes == 1)
	{
		/*
		** create our DIRECTDRAWPALETTE
		*/
		Con_SafePrintf( "...creating palette: " );
		if ( ( ddrval = lpDirectDraw->lpVtbl->CreatePalette( lpDirectDraw,
															DDPCAPS_8BIT | DDPCAPS_ALLOW256,
															palentries,
															&lpddpPalette,
															NULL ) ) != DD_OK )
		{
			Con_SafePrintf( "failed - %s\n", DDrawError( ddrval ) );
			goto fail;
		}
		Con_SafePrintf( "ok\n" );

		Con_SafePrintf( "...setting palette: " );
		if ( ( ddrval = lpddsFrontBuffer->lpVtbl->SetPalette( lpddsFrontBuffer,
															 lpddpPalette ) ) != DD_OK )
		{
			Con_SafePrintf( "failed - %s\n", DDrawError( ddrval ) );
			goto fail;
		}
		Con_SafePrintf( "ok\n" );

		DDRAW_SetPalette( ( const unsigned char * ) vid_curpal );

		vid_palettized = true;
	}
	else
		vid_palettized = false;

	/*
	** lock the back buffer
	*/
	memset( &ddsd, 0, sizeof( ddsd ) );
	ddsd.dwSize = sizeof( ddsd );
	
Con_SafePrintf( "...locking backbuffer: " );
	if ( ( ddrval = lpddsOffScreenBuffer->lpVtbl->Lock( lpddsOffScreenBuffer, NULL, &ddsd, DDLOCK_WAIT, NULL ) ) != DD_OK )
	{
		Con_SafePrintf( "failed - %s\n", DDrawError( ddrval ) );
		goto fail;
	}
Con_SafePrintf( "ok\n" );

	if (r_pixbytes == 2)
	{
		DDPIXELFORMAT format;
		format.dwSize = sizeof(format);
		Con_SafePrintf( "...getting pixel format: " );
		if (lpddsFrontBuffer->lpVtbl->GetPixelFormat(lpddsFrontBuffer, &format) == DD_OK)
		{
			int hi, lo;
			lo = LowBitPos(format.dwRBitMask);
			hi = HighBitPos(format.dwRBitMask);
			Con_SafePrintf("Hi=%i, Low=%i\n", hi, lo);
			redshift = lo;
			redbits = hi-lo;

			lo = LowBitPos(format.dwGBitMask);
			hi = HighBitPos(format.dwGBitMask);
			Con_SafePrintf("Hi=%i, Low=%i\n", hi, lo);
			greenshift = lo;
			greenbits = hi-lo;

			lo = LowBitPos(format.dwBBitMask);
			hi = HighBitPos(format.dwBBitMask);
			Con_SafePrintf("Hi=%i, Low=%i\n", hi, lo);
			blueshift = lo;
			bluebits = hi-lo;
		}
		else
		{
			Con_SafePrintf( "failed\n" );
			goto fail;
		}
		Con_SafePrintf( "ok\n" );
	}

	*ppbuffer = ddsd.lpSurface;
	*ppitch   = ddsd.lPitch/r_pixbytes;

	for ( i = 0; i < vid.height; i++ )
	{
		memset( *ppbuffer + i * *ppitch, 0, *ppitch );
	}	

	return true;

fail:
	Con_SafePrintf( "*** DDraw init failure ***\n" );
	DDRAW_Shutdown();
	return false;
}











static const char *DDrawError (int code)
{
    switch(code) {
        case DD_OK:
            return "DD_OK";
        case DDERR_ALREADYINITIALIZED:
            return "DDERR_ALREADYINITIALIZED";
        case DDERR_BLTFASTCANTCLIP:
            return "DDERR_BLTFASTCANTCLIP";
        case DDERR_CANNOTATTACHSURFACE:
            return "DDER_CANNOTATTACHSURFACE";
        case DDERR_CANNOTDETACHSURFACE:
            return "DDERR_CANNOTDETACHSURFACE";
        case DDERR_CANTCREATEDC:
            return "DDERR_CANTCREATEDC";
        case DDERR_CANTDUPLICATE:
            return "DDER_CANTDUPLICATE";
        case DDERR_CLIPPERISUSINGHWND:
            return "DDER_CLIPPERUSINGHWND";
        case DDERR_COLORKEYNOTSET:
            return "DDERR_COLORKEYNOTSET";
        case DDERR_CURRENTLYNOTAVAIL:
            return "DDERR_CURRENTLYNOTAVAIL";
        case DDERR_DIRECTDRAWALREADYCREATED:
            return "DDERR_DIRECTDRAWALREADYCREATED";
        case DDERR_EXCEPTION:
            return "DDERR_EXCEPTION";
        case DDERR_EXCLUSIVEMODEALREADYSET:
            return "DDERR_EXCLUSIVEMODEALREADYSET";
        case DDERR_GENERIC:
            return "DDERR_GENERIC";
        case DDERR_HEIGHTALIGN:
            return "DDERR_HEIGHTALIGN";
        case DDERR_HWNDALREADYSET:
            return "DDERR_HWNDALREADYSET";
        case DDERR_HWNDSUBCLASSED:
            return "DDERR_HWNDSUBCLASSED";
        case DDERR_IMPLICITLYCREATED:
            return "DDERR_IMPLICITLYCREATED";
        case DDERR_INCOMPATIBLEPRIMARY:
            return "DDERR_INCOMPATIBLEPRIMARY";
        case DDERR_INVALIDCAPS:
            return "DDERR_INVALIDCAPS";
        case DDERR_INVALIDCLIPLIST:
            return "DDERR_INVALIDCLIPLIST";
        case DDERR_INVALIDDIRECTDRAWGUID:
            return "DDERR_INVALIDDIRECTDRAWGUID";
        case DDERR_INVALIDMODE:
            return "DDERR_INVALIDMODE";
        case DDERR_INVALIDOBJECT:
            return "DDERR_INVALIDOBJECT";
        case DDERR_INVALIDPARAMS:
            return "DDERR_INVALIDPARAMS";
        case DDERR_INVALIDPIXELFORMAT:
            return "DDERR_INVALIDPIXELFORMAT";
        case DDERR_INVALIDPOSITION:
            return "DDERR_INVALIDPOSITION";
        case DDERR_INVALIDRECT:
            return "DDERR_INVALIDRECT";
        case DDERR_LOCKEDSURFACES:
            return "DDERR_LOCKEDSURFACES";
        case DDERR_NO3D:
            return "DDERR_NO3D";
        case DDERR_NOALPHAHW:
            return "DDERR_NOALPHAHW";
        case DDERR_NOBLTHW:
            return "DDERR_NOBLTHW";
        case DDERR_NOCLIPLIST:
            return "DDERR_NOCLIPLIST";
        case DDERR_NOCLIPPERATTACHED:
            return "DDERR_NOCLIPPERATTACHED";
        case DDERR_NOCOLORCONVHW:
            return "DDERR_NOCOLORCONVHW";
        case DDERR_NOCOLORKEY:
            return "DDERR_NOCOLORKEY";
        case DDERR_NOCOLORKEYHW:
            return "DDERR_NOCOLORKEYHW";
        case DDERR_NOCOOPERATIVELEVELSET:
            return "DDERR_NOCOOPERATIVELEVELSET";
        case DDERR_NODC:
            return "DDERR_NODC";
        case DDERR_NODDROPSHW:
            return "DDERR_NODDROPSHW";
        case DDERR_NODIRECTDRAWHW:
            return "DDERR_NODIRECTDRAWHW";
        case DDERR_NOEMULATION:
            return "DDERR_NOEMULATION";
        case DDERR_NOEXCLUSIVEMODE:
            return "DDERR_NOEXCLUSIVEMODE";
        case DDERR_NOFLIPHW:
            return "DDERR_NOFLIPHW";
        case DDERR_NOGDI:
            return "DDERR_NOGDI";
        case DDERR_NOHWND:
            return "DDERR_NOHWND";
        case DDERR_NOMIRRORHW:
            return "DDERR_NOMIRRORHW";
        case DDERR_NOOVERLAYDEST:
            return "DDERR_NOOVERLAYDEST";
        case DDERR_NOOVERLAYHW:
            return "DDERR_NOOVERLAYHW";
        case DDERR_NOPALETTEATTACHED:
            return "DDERR_NOPALETTEATTACHED";
        case DDERR_NOPALETTEHW:
            return "DDERR_NOPALETTEHW";
        case DDERR_NORASTEROPHW:
            return "Operation could not be carried out because there is no appropriate raster op hardware present or available.\0";
        case DDERR_NOROTATIONHW:
            return "Operation could not be carried out because there is no rotation hardware present or available.\0";
        case DDERR_NOSTRETCHHW:
            return "Operation could not be carried out because there is no hardware support for stretching.\0";
        case DDERR_NOT4BITCOLOR:
            return "DirectDrawSurface is not in 4 bit color palette and the requested operation requires 4 bit color palette.\0";
        case DDERR_NOT4BITCOLORINDEX:
            return "DirectDrawSurface is not in 4 bit color index palette and the requested operation requires 4 bit color index palette.\0";
        case DDERR_NOT8BITCOLOR:
            return "DDERR_NOT8BITCOLOR";
        case DDERR_NOTAOVERLAYSURFACE:
            return "Returned when an overlay member is called for a non-overlay surface.\0";
        case DDERR_NOTEXTUREHW:
            return "Operation could not be carried out because there is no texture mapping hardware present or available.\0";
        case DDERR_NOTFLIPPABLE:
            return "DDERR_NOTFLIPPABLE";
        case DDERR_NOTFOUND:
            return "DDERR_NOTFOUND";
        case DDERR_NOTLOCKED:
            return "DDERR_NOTLOCKED";
        case DDERR_NOTPALETTIZED:
            return "DDERR_NOTPALETTIZED";
        case DDERR_NOVSYNCHW:
            return "DDERR_NOVSYNCHW";
        case DDERR_NOZBUFFERHW:
            return "Operation could not be carried out because there is no hardware support for zbuffer blitting.\0";
        case DDERR_NOZOVERLAYHW:
            return "Overlay surfaces could not be z layered based on their BltOrder because the hardware does not support z layering of overlays.\0";
        case DDERR_OUTOFCAPS:
            return "The hardware needed for the requested operation has already been allocated.\0";
        case DDERR_OUTOFMEMORY:
            return "DDERR_OUTOFMEMORY";
        case DDERR_OUTOFVIDEOMEMORY:
            return "DDERR_OUTOFVIDEOMEMORY";
        case DDERR_OVERLAYCANTCLIP:
            return "The hardware does not support clipped overlays.\0";
        case DDERR_OVERLAYCOLORKEYONLYONEACTIVE:
            return "Can only have ony color key active at one time for overlays.\0";
        case DDERR_OVERLAYNOTVISIBLE:
            return "Returned when GetOverlayPosition is called on a hidden overlay.\0";
        case DDERR_PALETTEBUSY:
            return "DDERR_PALETTEBUSY";
        case DDERR_PRIMARYSURFACEALREADYEXISTS:
            return "DDERR_PRIMARYSURFACEALREADYEXISTS";
        case DDERR_REGIONTOOSMALL:
            return "Region passed to Clipper::GetClipList is too small.\0";
        case DDERR_SURFACEALREADYATTACHED:
            return "DDERR_SURFACEALREADYATTACHED";
        case DDERR_SURFACEALREADYDEPENDENT:
            return "DDERR_SURFACEALREADYDEPENDENT";
        case DDERR_SURFACEBUSY:
            return "DDERR_SURFACEBUSY";
        case DDERR_SURFACEISOBSCURED:
            return "Access to surface refused because the surface is obscured.\0";
        case DDERR_SURFACELOST:
            return "DDERR_SURFACELOST";
        case DDERR_SURFACENOTATTACHED:
            return "DDERR_SURFACENOTATTACHED";
        case DDERR_TOOBIGHEIGHT:
            return "Height requested by DirectDraw is too large.\0";
        case DDERR_TOOBIGSIZE:
            return "Size requested by DirectDraw is too large, but the individual height and width are OK.\0";
        case DDERR_TOOBIGWIDTH:
            return "Width requested by DirectDraw is too large.\0";
        case DDERR_UNSUPPORTED:
            return "DDERR_UNSUPPORTED";
        case DDERR_UNSUPPORTEDFORMAT:
            return "FOURCC format requested is unsupported by DirectDraw.\0";
        case DDERR_UNSUPPORTEDMASK:
            return "Bitmask in the pixel format requested is unsupported by DirectDraw.\0";
        case DDERR_VERTICALBLANKINPROGRESS:
            return "Vertical blank is in progress.\0";
        case DDERR_WASSTILLDRAWING:
            return "DDERR_WASSTILLDRAWING";
        case DDERR_WRONGMODE:
            return "This surface can not be restored because it was created in a different mode.\0";
        case DDERR_XALIGN:
            return "Rectangle provided was not horizontally aligned on required boundary.\0";
        default:
            return "UNKNOWN\0";
	}
}



void DDRAW_SwapBuffers (void)
{
	RECT r;
	HRESULT rval;
	DDSURFACEDESC ddsd;

	r.left = 0;
	r.top = 0;
	r.right = vid.width;
	r.bottom = vid.height;

	lpddsOffScreenBuffer->lpVtbl->Unlock( lpddsOffScreenBuffer, vid.buffer );

	if ( modex )
	{
		if ( ( rval = lpddsBackBuffer->lpVtbl->BltFast( lpddsBackBuffer,
																0, 0,
																lpddsOffScreenBuffer, 
																&r, 
																DDBLTFAST_WAIT ) ) == DDERR_SURFACELOST )
		{
			lpddsBackBuffer->lpVtbl->Restore( lpddsBackBuffer );
			lpddsBackBuffer->lpVtbl->BltFast( lpddsBackBuffer,
														0, 0,
														lpddsOffScreenBuffer, 
														&r, 
														DDBLTFAST_WAIT );

			Con_DPrintf("surface lost\n");
		}

		if ( ( rval = lpddsFrontBuffer->lpVtbl->Flip( lpddsFrontBuffer,
														 NULL, DDFLIP_WAIT ) ) == DDERR_SURFACELOST )
		{
			lpddsFrontBuffer->lpVtbl->Restore( lpddsFrontBuffer );
			lpddsFrontBuffer->lpVtbl->Flip( lpddsFrontBuffer, NULL, DDFLIP_WAIT );
		}
	}
	else
	{
		if ( ( rval = lpddsBackBuffer->lpVtbl->BltFast( lpddsFrontBuffer,
																0, 0,
																lpddsOffScreenBuffer, 
																&r, 
																DDBLTFAST_WAIT ) ) == DDERR_SURFACELOST )
		{
			lpddsBackBuffer->lpVtbl->Restore( lpddsFrontBuffer );
			lpddsBackBuffer->lpVtbl->BltFast( lpddsFrontBuffer,
														0, 0,
														lpddsOffScreenBuffer, 
														&r, 
														DDBLTFAST_WAIT );

			Con_DPrintf("surface lost\n");
		}
	}

	memset( &ddsd, 0, sizeof( ddsd ) );
	ddsd.dwSize = sizeof( ddsd );

	if (lpddsOffScreenBuffer->lpVtbl->Lock( lpddsOffScreenBuffer, NULL, &ddsd, DDLOCK_WAIT, NULL ))
		Sys_Error("Failed to lock ddraw");
	vid.conbuffer = vid.buffer = ddsd.lpSurface;
	vid.conrowbytes = vid.rowbytes = ddsd.lPitch/r_pixbytes;	
}
#else
qboolean DDRAW_Init(rendererstate_t *info, unsigned char **ppbuffer, int *ppitch )
{
	return false;
}
void DDRAW_SwapBuffers (void)
{
}
void DDRAW_Shutdown(void)
{
}
void DDRAW_SetPalette( const unsigned char *pal )
{
}
#endif

