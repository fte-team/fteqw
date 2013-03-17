#include "quakedef.h"
#include "glquake.h"

#include <SDL.h>

SDL_Surface *sdlsurf;

extern cvar_t vid_hardwaregamma;
extern cvar_t gl_lateswap;
extern int gammaworks;

#ifdef _WIN32	//half the rest of the code uses windows apis to focus windows. Should be fixed, but it's not too important.
HWND mainwindow;
#endif

extern qboolean vid_isfullscreen;

unsigned short intitialgammaramps[3][256];

qboolean ActiveApp;
qboolean mouseactive;
extern qboolean mouseusedforgui;


static void *GLVID_getsdlglfunction(char *functionname)
{
	return SDL_GL_GetProcAddress(functionname);
}

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	int flags;

	Con_Printf("SDL GLVID_Init\n");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
	SDL_SetVideoMode( 0, 0, 0, 0 );	//to get around some SDL bugs

	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );

	if (info->multisample)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, info->multisample);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	}

Con_Printf("Getting gamma\n");
	SDL_GetGammaRamp(intitialgammaramps[0], intitialgammaramps[1], intitialgammaramps[2]);

	if (info->fullscreen)
	{
		flags = SDL_FULLSCREEN;
		vid_isfullscreen = true;
	}
	else
	{
		flags = SDL_RESIZABLE;
		vid_isfullscreen = false;
	}
	sdlsurf = SDL_SetVideoMode(vid.pixelwidth=info->width, vid.pixelheight=info->height, info->bpp, flags | SDL_OPENGL);
	if (!sdlsurf)
	{
		Con_Printf("Couldn't set GL mode: %s\n", SDL_GetError());
		return false;
	}

	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &gl_stencilbits);

	ActiveApp = true;

	GLVID_SetPalette (palette);
	GL_Init(GLVID_getsdlglfunction);

	qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);

	mouseactive = false;
	if (vid_isfullscreen)
		IN_ActivateMouse();

	return true;
}

void GLVID_DeInit (void)
{
	ActiveApp = false;

	IN_DeactivateMouse();
	Con_Printf("Restoring gamma\n");
	SDL_SetGammaRamp (intitialgammaramps[0], intitialgammaramps[1], intitialgammaramps[2]);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


void GL_BeginRendering (void)
{
//    if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	qglViewport (*x, *y, *width, *height);
}

qboolean screenflush;
void GL_DoSwap (void)
{
	if (!screenflush)
		return;
	screenflush = 0;

	SDL_GL_SwapBuffers( );


	if (!vid_isfullscreen)
	{
		if (!_windowed_mouse.value)
		{
			if (mouseactive)
			{
				IN_DeactivateMouse ();
			}
		}
		else
		{
			if ((key_dest == key_game||mouseusedforgui) && ActiveApp)
				IN_ActivateMouse ();
			else if (!(key_dest == key_game || mouseusedforgui) || !ActiveApp)
				IN_DeactivateMouse ();
		}
	}
}

void GL_EndRendering (void)
{
	screenflush = true;
	if (!gl_lateswap.value)
		GL_DoSwap();
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
void	GLVID_ShiftPalette (unsigned char *palette)
{
	extern	unsigned short ramps[3][256];

	if (vid_hardwaregamma.value)	//this is needed because ATI drivers don't work properly (or when task-switched out).
	{
		if (gammaworks)
		{	//we have hardware gamma applied - if we're doing a BF, we don't want to reset to the default gamma (yuck)
			SDL_SetGammaRamp (ramps[0], ramps[1], ramps[2]);
			return;
		}
		gammaworks = !SDL_SetGammaRamp (ramps[0], ramps[1], ramps[2]);
	}
	else

		gammaworks = false;
}

void GLVID_SetCaption(char *text)
{
	SDL_WM_SetCaption( text, NULL );
}



