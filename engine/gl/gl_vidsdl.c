#include "quakedef.h"

#include <SDL.h>
#include <SDL_syswm.h>

#ifdef GLQUAKE
	#include "glquake.h"
	#define OPENGL_SDL
#endif

#if SDL_MAJOR_VERSION >= 2
	#if SDL_VERSION_ATLEAST(2,0,6)
		#ifdef VKQUAKE
			#include <SDL_vulkan.h>
			#include "../vk/vkrenderer.h"
			#define VULKAN_SDL
		#endif
	#endif
	SDL_Window *sdlwindow;
	#ifdef OPENGL_SDL
		static SDL_GLContext *sdlcontext;
	#endif
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

#ifdef OPENGL_SDL
static void *GLVID_getsdlglfunction(char *functionname)
{
#ifdef GL_STATIC
	//this reduces dependancies in the webgl build (removing warnings about emulation being poo)
	return NULL;
#else
	return SDL_GL_GetProcAddress(functionname);
#endif
}
#endif

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


static qboolean SDLVID_Init (rendererstate_t *info, unsigned char *palette, r_qrenderer_t qrenderer)
{
	int flags = 0;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
#if !defined(FTE_TARGET_WEB) && SDL_MAJOR_VERSION < 2
	SDL_SetVideoMode(0, 0, 0, 0);	//to get around some SDL bugs
#endif

#ifdef OPENGL_SDL
	if (qrenderer == QR_OPENGL)
	{
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

		if (info->srgb)
			SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

#if SDL_MAJOR_VERSION >= 2
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
	}
#endif

#if SDL_MAJOR_VERSION >= 2
	switch(qrenderer)
	{
	default:
		break;
#ifdef OPENGL_SDL
	case QR_OPENGL:
		flags |= SDL_WINDOW_OPENGL;
		break;
#endif
#ifdef VULKAN_SDL
	case QR_VULKAN:
		flags |= SDL_WINDOW_VULKAN;
		break;
#endif
	}
	if (info->fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;
	flags |= SDL_WINDOW_RESIZABLE;
	flags |= SDL_WINDOW_INPUT_GRABBED;
	flags |= SDL_WINDOW_SHOWN;
	#if SDL_PATCHLEVEL >= 1
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	#endif
	sdlwindow = SDL_CreateWindow(FULLENGINENAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, info->width, info->height, flags);
	if (!sdlwindow)
	{
		Con_Printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		return false;
	}
	CL_UpdateWindowTitle();

	switch(qrenderer)
	{
#ifdef OPENGL_SDL
#if SDL_PATCHLEVEL >= 1
	case QR_OPENGL:
		SDL_GL_GetDrawableSize(sdlwindow, &vid.pixelwidth, &vid.pixelheight);	//get the proper physical size.
		break;
#endif
#endif
#ifdef VULKAN_SDL
	case QR_VULKAN:
		SDL_Vulkan_GetDrawableSize(sdlwindow, &vid.pixelwidth, &vid.pixelheight);
		break;
#endif
	default:
		SDL_GetWindowSize(sdlwindow, &vid.pixelwidth, &vid.pixelheight);
		break;
	}

#ifdef OPENGL_SDL
	if (qrenderer == QR_OPENGL)
	{
		int srgb;
		sdlcontext = SDL_GL_CreateContext(sdlwindow);
		if (!sdlcontext)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
			sdlcontext = SDL_GL_CreateContext(sdlwindow);
			if (!sdlcontext)
			{
				Con_Printf("Couldn't initialize GL context: %s\n", SDL_GetError());
				return false;
			}
		}

		srgb = 0;
		SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &srgb);
		if (srgb)
			vid.flags |= VID_SRGB_CAPABLE;
	}
#endif

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

#ifdef OPENGL_SDL
qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	if (SDLVID_Init(info, palette, QR_OPENGL))
	{
		return GL_Init(info, GLVID_getsdlglfunction);
	}
	return false;
}
#endif

void GLVID_DeInit (void)
{
	vid.activeapp = false;

	IN_DeactivateMouse();


#if SDL_MAJOR_VERSION >= 2
	SDL_SetWindowGammaRamp(sdlwindow, NULL, NULL, NULL);
	switch(qrenderer)
	{
#ifdef OPENGL_SDL
	case QR_OPENGL:
		SDL_GL_DeleteContext(sdlcontext);
		break;
#endif
#ifdef VULKAN_SDL
	case QR_VULKAN:
		
		break;
#endif
	default:
		break;
	}
	SDL_DestroyWindow(sdlwindow);
	sdlwindow = NULL;
#else
	SDL_SetGammaRamp (intitialgammaramps[0], intitialgammaramps[1], intitialgammaramps[2]);
#endif

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
#ifdef OPENGL_SDL
	GL_ForgetPointers();
#endif
}


void GLVID_SwapBuffers (void)
{
	switch(qrenderer)
	{
#ifdef OPENGL_SDL
	case QR_OPENGL:
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
		break;
#endif
	default:
		break;
	}


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
		switch(vid_hardwaregamma.ival)
		{
		case 0:	//never use hardware/glsl gamma
		case 2:	//ALWAYS use glsl gamma
			return false;
		default:
		case 1:	//no hardware gamma when windowed
			if (!vid_isfullscreen)
				return false;
			break;
		case 3:	//ALWAYS try to use hardware gamma, even when it fails...
			break;
		}

		gammaworks |= !SDL_SetWindowGammaRamp (sdlwindow, &ramps[0], &ramps[256], &ramps[512]);
		return gammaworks;
	}
	else if (gammaworks)
	{
		SDL_SetWindowGammaRamp (sdlwindow, NULL, NULL, NULL);
		return true;
	}
#else
	if (ramps && gammarampsize == 256)
	{
		switch(vid_hardwaregamma.ival)
		{
		case 0:	//never use hardware/glsl gamma
		case 2:	//ALWAYS use glsl gamma
			return false;
		default:
		case 1:	//no hardware gamma when windowed
			if (!vid_isfullscreen)
				return false;
			break;
		case 3:	//ALWAYS try to use hardware gamma, even when it fails...
			break;
		}

		gammaworks |= !SDL_SetGammaRamp (&ramps[0], &ramps[256], &ramps[512]);
		return gammaworks;
	}
	else
	{
		SDL_SetGammaRamp (intitialgammaramps[0], intitialgammaramps[1], intitialgammaramps[2]);
		return true;
	}
#endif
	return false;
}

void GLVID_SetCaption(const char *text)
{
#if SDL_MAJOR_VERSION >= 2
	SDL_SetWindowTitle(sdlwindow, text);
#else
	SDL_WM_SetCaption(text, NULL);
#endif
}



#ifdef VULKAN_SDL
static qboolean VKSDL_CreateSurface(void)
{
	return SDL_Vulkan_CreateSurface(sdlwindow, vk.instance, &vk.surface);
}
static qboolean VKVID_Init (rendererstate_t *info, unsigned char *palette)
{
	unsigned extcount;
	const char **extnames;
	if (!SDLVID_Init(info, palette, QR_VULKAN))
		return false;
	if (!SDL_Vulkan_GetInstanceExtensions(sdlwindow, &extcount, NULL))
		return false;
	extnames = alloca(sizeof(*extnames)*(extcount+1));
	if (!SDL_Vulkan_GetInstanceExtensions(sdlwindow, &extcount, extnames))
		return false;

	vkGetInstanceProcAddr = SDL_Vulkan_GetVkGetInstanceProcAddr();
	if (!VK_Init(info, extnames, VKSDL_CreateSurface, NULL))
	{
		SDL_ShowSimpleMessageBox(0, "FTEQuake", extnames[1], sdlwindow);
		return false;
	}
	return true;
}
rendererinfo_t vkrendererinfo =
{
	"Vulkan-SDL",
	{
		"vk",
		"Vulkan"
	},
	QR_VULKAN,

	VK_Draw_Init,
	VK_Draw_Shutdown,

	VK_UpdateFiltering,
	VK_LoadTextureMips,
	VK_DestroyTexture,

	VK_R_Init,
	VK_R_DeInit,
	VK_R_RenderView,

	VKVID_Init,
	GLVID_DeInit,
	GLVID_SwapBuffers,
	GLVID_ApplyGammaRamps,
	NULL,
	NULL,
	NULL,
	GLVID_SetCaption,
	VKVID_GetRGBInfo,

	VK_SCR_UpdateScreen,

	VKBE_SelectMode,
	VKBE_DrawMesh_List,
	VKBE_DrawMesh_Single,
	VKBE_SubmitBatch,
	VKBE_GetTempBatch,
	VKBE_DrawWorld,
	VKBE_Init,
	VKBE_GenBrushModelVBO,
	VKBE_ClearVBO,
	VKBE_UploadAllLightmaps,
	VKBE_SelectEntity,
	VKBE_SelectDLight,
	VKBE_Scissor,
	VKBE_LightCullModel,

	VKBE_VBO_Begin,
	VKBE_VBO_Data,
	VKBE_VBO_Finish,
	VKBE_VBO_Destroy,

	VKBE_RenderToTextureUpdate2d,

	"no more"
};
#else
rendererinfo_t vkrendererinfo;
#endif
