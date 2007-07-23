#include "quakedef.h"
#include "d_local.h"
#include "SDL.h"

extern qboolean vid_isfullscreen;

extern qboolean ActiveApp;
#ifdef _WIN32
#include <windows.h>
HWND mainwindow;
#endif

qboolean Minimized;

extern SDL_Surface *sdlsurf;

qbyte vid_curpal[768];

cvar_t in_xflip = {"in_xflip", "0"};

void ResetFrameBuffers(void)
{
	int vid_surfcachesize;
	void *vid_surfcache;
	int buffersize;

	if (d_pzbuffer)
	{
		D_FlushCaches();
		free(d_pzbuffer);
		d_pzbuffer = NULL;
	}
	buffersize = vid.width * vid.height * sizeof(*d_pzbuffer);
	vid_surfcachesize = D_SurfaceCacheForRes (vid.width, vid.height, 0);
	buffersize += vid_surfcachesize;

	d_pzbuffer = malloc(buffersize);
	vid_surfcache = (qbyte *) d_pzbuffer + vid.width * vid.height * sizeof(*d_pzbuffer);

	D_InitCaches(vid_surfcache, vid_surfcachesize);
}

qboolean SWVID_Init (rendererstate_t *info, unsigned char *palette)
{
	int flags;
	Con_Printf("SDL SWVID_Init\n");

	info->bpp = 8;	//I don't know thier card details I'm afraid.

	SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);

	if (info->fullscreen)
	{
		flags = SDL_FULLSCREEN;
		vid_isfullscreen = true;
	}
	else
	{
		flags = 0; // :( SDL_RESIZABLE;
		vid_isfullscreen = false;
	}

flags |= SDL_SWSURFACE;

	sdlsurf = SDL_SetVideoMode(info->width, info->height, info->bpp, flags|SDL_DOUBLEBUF);
	if (!sdlsurf)
		return false;	//bummer.

	vid.width = vid.conwidth = sdlsurf->w;
	vid.height = vid.conheight = sdlsurf->h;
	vid.colormap = host_colormap;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	ActiveApp = true;

	SDL_SetClipRect(sdlsurf, NULL);


	vid.numpages = (sdlsurf->flags & SDL_DOUBLEBUF)?2:1;
	vid.aspect = ((float)vid.height/vid.width);

	r_pixbytes = info->bpp/8;

	SWVID_LockBuffer();	//make sure our buffer and pitch are set up right.
	SWVID_UnlockBuffer();

	ResetFrameBuffers();
	return true;
}

void SWVID_SetCaption(char *caption)
{
}

void SWVID_SetPalette(unsigned char *palette)
{
	int i;
	SDL_Color colours[256];
	memcpy(vid_curpal, palette, sizeof(vid_curpal));
	for (i = 0; i < 256; i++)
	{
		colours[i].r = palette[i*3+0];
		colours[i].g = palette[i*3+1];
		colours[i].b = palette[i*3+2];
	}
	SDL_SetColors(sdlsurf, colours, 0, 256);
}
void SWVID_ShiftPalette(unsigned char *palette)
{
	SWVID_SetPalette(palette);
}

void SWVID_Shutdown(void)
{
	ActiveApp = false;

	IN_Shutdown();
	Con_Printf("Restoring gamma\n");
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void SWVID_LockBuffer (void)
{
	if (SDL_LockSurface(sdlsurf)<0)
		if (SDL_LockSurface(sdlsurf)<0)
			Sys_Error("Couldn't lock surface\n");

	vid.buffer = vid.conbuffer = sdlsurf->pixels;
	vid.rowbytes = vid.conrowbytes = sdlsurf->pitch/r_pixbytes;
}

void SWVID_ForceLockState (int lk)	//I detest these functions. FIXME: Remove
{
	SWVID_LockBuffer();
}

void SWVID_UnlockBuffer (void)
{
	SDL_UnlockSurface(sdlsurf);
	vid.buffer = NULL;
}

int SWVID_ForceUnlockedAndReturnState(void) //FIXME: Remove
{
	SWVID_UnlockBuffer();
	return 0;
}

void SWVID_Update(vrect_t *rects)
{
	SDL_Flip(sdlsurf);

	IN_UpdateGrabs();
}

void SWVID_HandlePause (qboolean pause)
{
}

void SWD_BeginDirectRect (int x, int y, qbyte *pbitmap, int width, int height)
{
}
void SWD_EndDirectRect (int x, int y, int width, int height)
{
}

