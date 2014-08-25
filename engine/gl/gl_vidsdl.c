#include "quakedef.h"
#include "glquake.h"

#include <SDL.h>
#include <SDL_syswm.h>

#if SDL_MAJOR_VERSION >= 2
SDL_Window *sdlwindow;
static SDL_GLContext *sdlcontext;
#else
SDL_Surface *sdlsurf;
#endif

extern cvar_t		vid_vsync;
extern cvar_t vid_hardwaregamma;
extern cvar_t gl_lateswap;
extern cvar_t vid_gl_context_version;
extern cvar_t vid_gl_context_debug;
extern cvar_t vid_gl_context_forwardcompatible;
extern cvar_t vid_gl_context_es;
extern cvar_t vid_gl_context_compatibility;
extern int gammaworks;

#ifdef _WIN32	//half the rest of the code uses windows apis to focus windows. Should be fixed, but it's not too important.
HWND mainwindow;
#endif

extern qboolean vid_isfullscreen;

#if SDL_MAJOR_VERSION < 2
unsigned short intitialgammaramps[3][256];
#endif

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
	int flags = 0;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
#if !defined(FTE_TARGET_WEB) && SDL_MAJOR_VERSION < 2
	SDL_SetVideoMode(0, 0, 0, 0);	//to get around some SDL bugs
#endif

#if SDL_MAJOR_VERSION >= 2
	SDL_GL_LoadLibrary(NULL);
#endif

	if (info->bpp >= 32)
	{
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);	//technically we don't always need stencil support.
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	}
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (info->stereo)
		SDL_GL_SetAttribute(SDL_GL_STEREO, 1);

#if 0//SDL_MAJOR_VERSION >= 2
	//FIXME: this stuff isn't part of info.
	//this means it shouldn't be exposed to the menu or widely advertised.
	if (*vid_gl_context_version.string)
	{
		int major, minor;
		char *ver = vid_gl_context_version.string;
		major = strtoul(ver, &ver, 10);
		if (*ver == '.')
		{
			ver++;
			minor = strtoul(ver, &ver, 10);
		}
		else
			minor = 0;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
			(vid_gl_context_debug.ival?SDL_GL_CONTEXT_DEBUG_FLAG:0) |
			(vid_gl_context_forwardcompatible.ival?SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG:0) |
			0);

	if (vid_gl_context_es.ival)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	else if (vid_gl_context_compatibility.ival)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
	if (info->multisample)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, info->multisample);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	}

#if SDL_MAJOR_VERSION >= 2
	if (info->fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;
	flags |= SDL_WINDOW_OPENGL;
	flags |= SDL_WINDOW_RESIZABLE;
	flags |= SDL_WINDOW_INPUT_GRABBED;
	flags |= SDL_WINDOW_SHOWN;
	#if SDL_PATCHLEVEL >= 1
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	#endif
	sdlwindow = SDL_CreateWindow("My Magic Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, info->width, info->height, flags);
	if (!sdlwindow)
	{
		Con_Printf("Couldn't set video mode: %s\n", SDL_GetError());
		return false;
	}
	#if SDL_PATCHLEVEL >= 1
		SDL_GL_GetDrawableSize(sdlwindow, &vid.pixelwidth, &vid.pixelheight);	//get the proper physical size.
	#else
		SDL_GetWindowSize(sdlwindow, &vid.pixelwidth, &vid.pixelheight);
	#endif

	sdlcontext = SDL_GL_CreateContext(sdlwindow);
	if (!sdlcontext)
	{
		Con_Printf("Couldn't initialize GL context: %s\n", SDL_GetError());
		return false;
	}

	{
		SDL_Surface *iconsurf;
		#include "bymorphed.h"
		iconsurf = SDL_CreateRGBSurfaceFrom((void*)icon.pixel_data, icon.width, icon.height, 32, 4*icon.height, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);	//RGBA byte order on a little endian machine, at least...
		SDL_SetWindowIcon(sdlwindow, iconsurf);
		SDL_FreeSurface(iconsurf);
	}
#else
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
#endif
	ActiveApp = true;

	GL_Init(GLVID_getsdlglfunction);

	qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);

	mouseactive = false;
	if (vid_isfullscreen)
		IN_ActivateMouse();

#if SDL_MAJOR_VERSION < 2
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#else
	SDL_DisableScreenSaver();
#endif
	vid_vsync.modified = true;


	#ifdef _WIN32
	{	//win32 apis are very insistant upon having a window context for things that have nothing to do with windowing system stuff.
		#if SDL_MAJOR_VERSION >= 2
			SDL_SysWMinfo info;
			SDL_GetWindowWMInfo(sdlwindow, &info);
			if (info.subsystem == SDL_SYSWM_WINDOWS)
				mainwindow = info.info.win.window;
			else
				mainwindow = NULL;	//if we're using an x11 subsystem but running in windows then don't feck up... here, at least.
		#else
			SDL_SysWMinfo wmInfo;
			SDL_GetWMInfo(&wmInfo);
			mainwindow = wmInfo.window; //note that this is usually still null
		#endif
	}
	#endif

	return true;
}

void GLVID_DeInit (void)
{
	ActiveApp = false;

	IN_DeactivateMouse();


#if SDL_MAJOR_VERSION >= 2
	SDL_SetWindowGammaRamp(sdlwindow, NULL, NULL, NULL);
	SDL_GL_DeleteContext(sdlcontext);
	SDL_DestroyWindow(sdlwindow);
	sdlwindow = NULL;
#else
	SDL_SetGammaRamp (intitialgammaramps[0], intitialgammaramps[1], intitialgammaramps[2]);
#endif

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


void GLVID_SwapBuffers (void)
{
#if SDL_MAJOR_VERSION >= 2
	if (vid_vsync.modified)
	{
		if (*vid_vsync.string)
		{
			//if swap_tear isn't supported, try without.
			if (SDL_GL_SetSwapInterval(vid_vsync.ival) == -1 && vid_vsync.ival < 0)
				SDL_GL_SetSwapInterval(-vid_vsync.ival);
		}
		vid_vsync.modified = false;
	}

	SDL_GL_SwapWindow(sdlwindow);
#else
	SDL_GL_SwapBuffers();
#endif


	if (!vid_isfullscreen)
	{
		if (!_windowed_mouse.value)
		{
			IN_DeactivateMouse ();
		}
		else
		{
			if (!Key_MouseShouldBeFree() && ActiveApp)
				IN_ActivateMouse ();
			else
				IN_DeactivateMouse ();
		}
	}
}

qboolean GLVID_ApplyGammaRamps (unsigned short *ramps)
{
#if SDL_MAJOR_VERSION >= 2
	if (ramps)
	{
		if (vid_hardwaregamma.value)
		{
			if (gammaworks)
			{	//we have hardware gamma applied - if we're doing a BF, we don't want to reset to the default gamma (yuck)
				SDL_SetWindowGammaRamp (sdlwindow, &ramps[0], &ramps[256], &ramps[512]);
				return true;
			}
			gammaworks = !SDL_SetWindowGammaRamp (sdlwindow, &ramps[0], &ramps[256], &ramps[512]);
		}
		else
			gammaworks = false;

		return gammaworks;
	}
	else
	{
		SDL_SetWindowGammaRamp (sdlwindow, NULL, NULL, NULL);
		return true;
	}
#else
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
#endif
}

void GLVID_SetCaption(char *text)
{
#if SDL_MAJOR_VERSION >= 2
	SDL_SetWindowTitle(sdlwindow, text);
#else
	SDL_WM_SetCaption(text, NULL);
#endif
}



