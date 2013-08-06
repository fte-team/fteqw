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
#ifdef GL_STATIC
	//this reduces dependancies in the webgl build (removing warnings about emulation being poo)
	return NULL;
#else
	return SDL_GL_GetProcAddress(functionname);
#endif
}

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	int flags;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
#ifndef FTE_TARGET_WEB
	SDL_SetVideoMode( 0, 0, 0, 0 );	//to get around some SDL bugs
#endif

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

	ActiveApp = true;

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

qboolean GLVID_ApplyGammaRamps (unsigned short *ramps)
{
	if (ramps)
	{
		if (vid_hardwaregamma.value)
		{
			if (gammaworks)
			{	//we have hardware gamma applied - if we're doing a BF, we don't want to reset to the default gamma (yuck)
				SDL_SetGammaRamp (&ramps[0], &ramps[256], &ramps[512]);
				return true;
			}
			gammaworks = !SDL_SetGammaRamp (&ramps[0], &ramps[256], &ramps[512]);
		}
		else
			gammaworks = false;

		return gammaworks;
	}
	else
	{
		SDL_SetGammaRamp (intitialgammaramps[0], intitialgammaramps[1], intitialgammaramps[2]);
		return true;
	}
}

void GLVID_SetCaption(char *text)
{
	SDL_WM_SetCaption( text, NULL );
}



