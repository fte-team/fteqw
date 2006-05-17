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
// d_init.c: rasterization driver initialization

#include "quakedef.h"
#include "d_local.h"

#define NUM_MIPS	4

cvar_t	d_subdiv16 = SCVAR("d_subdiv16", "1");
cvar_t	d_mipcap = SCVAR("d_mipcap", "0");
cvar_t	d_mipscale = SCVAR("d_mipscale", "0.5");
extern cvar_t	d_smooth;

surfcache_t		*d_initial_rover;
qboolean		d_roverwrapped;
int				d_minmip;
float			d_scalemip[NUM_MIPS-1];

static float	basemip[NUM_MIPS-1] = {1.0, 0.5*0.8, 0.25*0.8};

void (*d_drawspans) (espan_t *pspan);

void D_DrawSpans32 (espan_t *pspan);
void D_DrawSpans32From8 (espan_t *pspan);
void D_DrawSpans32_Smooth (espan_t *pspan);

void D_Shutdown (void)
{
	D_ShutdownTrans();
}

/*
===============
D_Init
===============
*/
void D_Init (void)
{

	r_skydirect = 1;

	r_drawpolys = false;
	r_worldpolysbacktofront = false;
	r_recursiveaffinetriangles = true;

	if (!r_pixbytes)
		r_pixbytes = 1;
	r_aliasuvscale = 1.0;

#ifdef PEXT_TRANS
	D_InitTrans();
#endif
}


/*
===============
D_CopyRects
===============
*/
void D_CopyRects (vrect_t *prects, int transparent)
{

// this function is only required if the CPU doesn't have direct access to the
// back buffer, and there's some driver interface function that the driver
// doesn't support and requires Quake to do in software (such as drawing the
// console); Quake will then draw into wherever the driver points vid.buffer
// and will call this function before swapping buffers

	UNUSED(prects);
	UNUSED(transparent);
}


/*
===============
D_EnableBackBufferAccess
===============
*/
void D_EnableBackBufferAccess (void)
{

	VID_LockBuffer ();
}


/*
===============
D_TurnZOn
===============
*/
void D_TurnZOn (void)
{
// not needed for software version
}


/*
===============
D_DisableBackBufferAccess
===============
*/
void D_DisableBackBufferAccess (void)
{
	VID_UnlockBuffer ();
}


/*
===============
D_SetupFrame
===============
*/
void D_SetupFrame (void)
{
	extern qboolean r_usinglits;
	int		i;

	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = (void *)(qbyte *)vid.buffer;

	if (r_dowarp)
		screenwidth = WARP_WIDTH;
	else
		screenwidth = vid.rowbytes;

	d_roverwrapped = false;
	d_initial_rover = sc_rover;

	d_minmip = d_mipcap.value;
	if (d_minmip > 3)
		d_minmip = 3;
	else if (d_minmip < 0)
		d_minmip = 0;

	for (i=0 ; i<(NUM_MIPS-1) ; i++)
		d_scalemip[i] = basemip[i] * d_mipscale.value;

#if	id386
				if (r_pixbytes == 4)
					d_drawspans = d_smooth.value?D_DrawSpans32_Smooth:D_DrawSpans32;
				else if (r_pixbytes == 2)
					d_drawspans = D_DrawSpans16;
				else if (d_smooth.value)//override's subdiv16 (bigger impact)
					d_drawspans = D_DrawSpans8_Smooth;
				else if (d_subdiv16.value)
					d_drawspans = D_DrawSpans8SubDiv16;
				else
					d_drawspans = D_DrawSpans8;
#else
				if (r_pixbytes == 4)
				{
					if (r_usinglits)
						d_drawspans = d_smooth.value?D_DrawSpans32_Smooth:D_DrawSpans32;
					else
						d_drawspans = D_DrawSpans32From8;
				}
				else if (r_pixbytes == 2)
					d_drawspans = D_DrawSpans16;
				else
					d_drawspans = d_smooth.value?D_DrawSpans8_Smooth:D_DrawSpans8;
#endif				
}


/*
===============
D_UpdateRects
===============
*/
void D_UpdateRects (vrect_t *prect)
{

// the software driver draws these directly to the vid buffer

	UNUSED(prect);
}

