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
// d_surf.c: rasterization driver surface heap manager

#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"

float           surfscale;
qboolean        r_cache_thrash;         // set if surface cache is thrashing

int r_flushcache;

int                                     sc_size;
surfcache_t                     *sc_rover, *sc_base;

#define GUARDSIZE       4


int     D_SurfaceCacheForRes (int width, int height, int bpp)
{
	extern cvar_t sw_surfcachesize;
	int             size, pix;

	if (COM_CheckParm ("-surfcachesize"))
	{
		size = Q_atoi(com_argv[COM_CheckParm("-surfcachesize")+1]) * 1024;
		return size;
	}

	if (sw_surfcachesize.value >= 512*1024) // force minimum of 512k
	{
		return (int)sw_surfcachesize.value;
	}
	
	size = 4096*1024;//SURFCACHE_SIZE_AT_320X200;

	pix = width*height;
	if (pix > 64000)
		size += (pix-64000)*4;

	size*=8;
	if (bpp)
		return size*bpp;
	return size;
}

void D_CheckCacheGuard (void)
{
	qbyte    *s;
	int             i;

	s = (qbyte *)sc_base + sc_size;
	for (i=0 ; i<GUARDSIZE ; i++)
		if (s[i] != (qbyte)i)
			Sys_Error ("D_CheckCacheGuard: failed");
}

void D_ClearCacheGuard (void)
{
	qbyte    *s;
	int             i;
	
	s = (qbyte *)sc_base + sc_size;
	for (i=0 ; i<GUARDSIZE ; i++)
		s[i] = (qbyte)i;
}


/*
================
D_InitCaches

================
*/
void D_InitCaches (void *buffer, int size)
{
//	Con_Printf(S_NOTICE "Using %i KB for SW surface cache\n", size / 1024);

	sc_size = size - GUARDSIZE;
	sc_base = (surfcache_t *)buffer;
	sc_rover = sc_base;
	
	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;
	
	D_ClearCacheGuard ();
}


/*
==================
D_FlushCaches
==================
*/
void D_FlushCaches (void)
{
	surfcache_t     *c;
	
	if (!sc_base)
		return;

	for (c = sc_base ; c ; c = c->next)
	{
		if (c->owner)
			*c->owner = NULL;
	}
	
	sc_rover = sc_base;
	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;
}

/*
=================
D_SCAlloc
=================
*/
surfcache_t     *D_SCAlloc (int width, int bpp, int size)
{
	surfcache_t             *newsc;
	qboolean                wrapped_this_time;

//	if ((width < 0) || (width > 256))
//		Sys_Error ("D_SCAlloc: bad cache width %d\n", width);

//	if ((size <= 0) || (size > 0x10000*bpp))
//		Sys_Error ("D_SCAlloc: bad cache size %d\n", size);	

#ifdef __alpha__
	size = (int)((long)&((surfcache_t *)0)->data[size]);
#else
	size = (int)&((surfcache_t *)0)->data[size];
#endif
	size = (size + 3) & ~3;
	if (size > sc_size)
		Sys_Error ("D_SCAlloc: %i > cache size",size);

// if there is not size bytes after the rover, reset to the start
	wrapped_this_time = false;

	if ( !sc_rover || (qbyte *)sc_rover - (qbyte *)sc_base > sc_size - size)
	{
		if (sc_rover)
		{
			wrapped_this_time = true;
		}
		sc_rover = sc_base;
	}
		
// colect and free surfcache_t blocks until the rover block is large enough
	newsc = sc_rover;
	if (sc_rover->owner)
		*sc_rover->owner = NULL;
	
	while (newsc->size < size)
	{
	// free another
		sc_rover = sc_rover->next;
		if (!sc_rover)
			Sys_Error ("D_SCAlloc: hit the end of memory");
		if (sc_rover->owner)
			*sc_rover->owner = NULL;
			
		newsc->size += sc_rover->size;
		newsc->next = sc_rover->next;
	}

// create a fragment out of any leftovers
	if (newsc->size - size > 256)
	{
		sc_rover = (surfcache_t *)( (qbyte *)newsc + size);
		sc_rover->size = newsc->size - size;
		sc_rover->next = newsc->next;
		sc_rover->width = 0;
		sc_rover->owner = NULL;
		newsc->next = sc_rover;
		newsc->size = size;
	}
	else
		sc_rover = newsc->next;
	
	newsc->width = width;
// DEBUG
	if (width > 0)
		newsc->height = (size - sizeof(*newsc) + sizeof(newsc->data)) / (width*bpp);

	newsc->bytesperpix = bpp;

	newsc->owner = NULL;              // should be set properly after return

	if (d_roverwrapped)
	{
		if (wrapped_this_time || (sc_rover >= d_initial_rover))
			r_cache_thrash = true;
	}
	else if (wrapped_this_time)
	{       
		d_roverwrapped = true;
	}	

D_CheckCacheGuard ();   // DEBUG
	return newsc;
}


/*
=================
D_SCDump
=================
*/
void D_SCDump (void)
{
	surfcache_t             *test;

	for (test = sc_base ; test ; test = test->next)
	{
		if (test == sc_rover)
			Sys_Printf ("ROVER:\n");
		Sys_Printf ("%p : %i bytes     %i width\n",test, test->size, test->width);
	}
}

//=============================================================================

/*
================
D_CacheSurface
================
*/
surfcache_t *D_CacheSurface (msurface_t *surface, int miplevel)
{
	surfcache_t     *cache;
	int bpp;

//
// if the surface is animating or flashing, flush the cache
//
	r_drawsurf.texture = SWR_TextureAnimation (surface->texinfo->texture);
	r_drawsurf.lightadj[0] = d_lightstylevalue[surface->styles[0]];
	r_drawsurf.lightadj[1] = d_lightstylevalue[surface->styles[1]];
	r_drawsurf.lightadj[2] = d_lightstylevalue[surface->styles[2]];
	r_drawsurf.lightadj[3] = d_lightstylevalue[surface->styles[3]];
	
//
// see if the cache holds apropriate data
//
	cache = surface->cachespots[miplevel];

	if (cache && !cache->dlight && surface->dlightframe != r_framecount
			&& cache->texture == r_drawsurf.texture && cache->fcache == r_flushcache	//extra part added to flush caches over a group of frames on palette change...
			&& cache->lightadj[0] == r_drawsurf.lightadj[0]
			&& cache->lightadj[1] == r_drawsurf.lightadj[1]
			&& cache->lightadj[2] == r_drawsurf.lightadj[2]
			&& cache->lightadj[3] == r_drawsurf.lightadj[3] )
		return cache;

//
// determine shape of surface
//
	surfscale = 1.0 / (1<<miplevel);

	r_drawsurf.surfmip = miplevel;
	r_drawsurf.surfwidth = surface->extents[0] >> miplevel;
	r_drawsurf.rowbytes = r_drawsurf.surfwidth;
	r_drawsurf.surfheight = surface->extents[1] >> miplevel;
	
	bpp = r_pixbytes;
//
// allocate memory if needed
//
	if (!cache)     // if a texture just animated, don't reallocate it
	{
		cache = D_SCAlloc (r_drawsurf.surfwidth, bpp, 
						   r_drawsurf.surfwidth * r_drawsurf.surfheight * bpp);
		surface->cachespots[miplevel] = cache;
		cache->owner = &surface->cachespots[miplevel];
		cache->mipscale = surfscale;
	}	
	
	if (surface->dlightframe == r_framecount)
		cache->dlight = 1;
	else
		cache->dlight = 0;

	cache->fcache = r_flushcache;

	r_drawsurf.surfdat = (pixel_t *)cache->data;
	
	cache->texture = r_drawsurf.texture;
	cache->lightadj[0] = r_drawsurf.lightadj[0];
	cache->lightadj[1] = r_drawsurf.lightadj[1];
	cache->lightadj[2] = r_drawsurf.lightadj[2];
	cache->lightadj[3] = r_drawsurf.lightadj[3];

//
// draw and light the surface texture
//
	r_drawsurf.surf = surface;

	c_surf++;
	if (cache->bytesperpix==4 && r_usinglits)
		R_DrawSurface32 ();
	else
		R_DrawSurface ();

	return surface->cachespots[miplevel];
}


