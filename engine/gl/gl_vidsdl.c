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
extern qboolean gammaworks;

#ifdef _WIN32	//half the rest of the code uses windows apis to focus windows. Should be fixed, but it's not too important.
HWND mainwindow;
#endif

extern qboolean vid_isfullscreen;

#if SDL_MAJOR_VERSION < 2
unsigned short intitialgammaramps[3][256];
#endif

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

#if SDL_MAJOR_VERSION >= 2
void *GLVID_CreateCursor			(const char *filename, float hotx, float hoty, float scale)
{
	int width;
	int height;
	SDL_Cursor *curs;
	SDL_Surface *surf;
	qbyte *rgbadata_start;
	qboolean hasalpha;
	void *filedata;
	int filelen;
	if (!filename || !*filename)
		return NULL;
	filelen = FS_LoadFile(filename, &filedata);
	if (!filedata)
		return NULL;

	rgbadata_start = Read32BitImageFile(filedata, filelen, &width, &height, &hasalpha, "cursor");
	FS_FreeFile(filedata);
	if (!rgbadata_start)
		return NULL;

	if (scale != 1)
	{
		int nw,nh;
		qbyte *nd;
		nw = width * scale;
		nh = height * scale;
		if (nw <= 0 || nh <= 0 || nw > 128 || nh > 128)	//don't go crazy.
			return NULL;
		nd = BZ_Malloc(nw*nh*4);
		Image_ResampleTexture((unsigned int*)rgbadata_start, width, height, (unsigned int*)nd, nw, nh);
		width = nw;
		height = nh;
		BZ_Free(rgbadata_start);
		rgbadata_start = nd;
	}

	surf = SDL_CreateRGBSurfaceFrom(rgbadata_start, width, height, 32, width*4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	curs = SDL_CreateColorCursor(surf, hotx, hoty);
	SDL_FreeSurface(surf);
	BZ_Free(rgbadata_start);
	return curs;
}
qboolean GLVID_SetCursor			(void *cursor)
{
	SDL_SetCursor(cursor);
	return !!cursor;
}
void GLVID_DestroyCursor			(void *cursor)
{
	SDL_FreeCursor(cursor);
}
#endif


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
	sdlwindow = SDL_CreateWindow(FULLENGINENAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, info->width, info->height, flags);
	if (!sdlwindow)
	{
		Con_Printf("Couldn't set video mode: %s\n", SDL_GetError());
		return false;
	}
	CL_UpdateWindowTitle();
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
	vid.activeapp = true;

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

#if SDL_MAJOR_VERSION >= 2
	rf->VID_CreateCursor = GLVID_CreateCursor;
	rf->VID_DestroyCursor = GLVID_DestroyCursor;
	rf->VID_SetCursor = GLVID_SetCursor;
#endif

	return true;
}

void GLVID_DeInit (void)
{
	vid.activeapp = false;

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
			if (!Key_MouseShouldBeFree() && vid.activeapp)
				IN_ActivateMouse ();
			else
				IN_DeactivateMouse ();
		}
	}
}

qboolean GLVID_ApplyGammaRamps (unsigned int gammarampsize, unsigned short *ramps)
{
#if SDL_MAJOR_VERSION >= 2
	if (ramps && gammarampsize == 256)
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
	if (ramps && gammarampsize == 256)
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

void GLVID_SetCaption(const char *text)
{
#if SDL_MAJOR_VERSION >= 2
	SDL_SetWindowTitle(sdlwindow, text);
#else
	SDL_WM_SetCaption(text, NULL);
#endif
}



