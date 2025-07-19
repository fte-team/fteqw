#include "quakedef.h"

#ifdef FTE_SDL3
	#include <SDL3/SDL.h>
#else
	#include <SDL.h>
	#include <SDL_syswm.h>
#endif

#ifdef GLQUAKE
	#include "glquake.h"
	#define OPENGL_SDL
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
	#if SDL_VERSION_ATLEAST(2,0,6)
		#ifdef VKQUAKE
			#if SDL_VERSION_ATLEAST(3,0,0)
				#include <SDL3/SDL_vulkan.h>
			#else
				#include <SDL_vulkan.h>
			#endif
			#include "../vk/vkrenderer.h"
			#define VULKAN_SDL
		#endif
	#endif
	SDL_Window *sdlwindow;
	#ifdef OPENGL_SDL
		#if SDL_VERSION_ATLEAST(3,0,0)
			static SDL_GLContext sdlcontext;
		#else
			static SDL_GLContext *sdlcontext;
		#endif
	#endif
#else
	SDL_Surface *sdlsurf;
#endif
void INS_SetOSK(int osk);

#include "vr.h"

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

#if !SDL_VERSION_ATLEAST(2,0,0)
unsigned short intitialgammaramps[3][256];
#endif

extern qboolean mouseactive;
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

#if SDL_VERSION_ATLEAST(2,0,0)
#if SDL_VERSION_ATLEAST(3,0,0)
static SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    return SDL_CreateSurfaceFrom(width, height,
                                 SDL_GetPixelFormatForMasks(depth, Rmask, Gmask, Bmask, Amask),
                                 pixels, pitch);
}
#undef SDL_FreeSurface
#define SDL_FreeSurface SDL_DestroySurface
#undef SDL_FreeCursor
#define SDL_FreeCursor SDL_DestroyCursor
#endif

void *GLVID_CreateCursor			(const qbyte *imagedata, int width, int height, uploadfmt_t format, float hotx, float hoty, float scale)
{
	SDL_Cursor *curs;
	SDL_Surface *surf;
	Uint32 r,g,b,a;
	if (!imagedata)
		return NULL;
	switch(format)
	{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	case PTI_LLLX8:
	case PTI_RGBX8:
		r = 0xff000000;
		g = 0x00ff0000;
		b = 0x0000ff00;
		a = 0x00000000;
		break;
	case PTI_LLLA8:
	case PTI_RGBA8:
		r = 0xff000000;
		g = 0x00ff0000;
		b = 0x0000ff00;
		a = 0x000000ff;
		break;
	case PTI_BGRX8:
		b = 0xff000000;
		g = 0x00ff0000;
		r = 0x0000ff00;
		a = 0x00000000;
		break;
	case PTI_BGRA8:
		b = 0xff000000;
		g = 0x00ff0000;
		r = 0x0000ff00;
		a = 0x000000ff;
		break;
#else
	case PTI_LLLX8:
	case PTI_RGBX8:
		r = 0x000000ff;
		g = 0x0000ff00;
		b = 0x00ff0000;
		a = 0x00000000;
		break;
	case PTI_LLLA8:
	case PTI_RGBA8:
		r = 0x000000ff;
		g = 0x0000ff00;
		b = 0x00ff0000;
		a = 0xff000000;
		break;
	case PTI_BGRX8:
		b = 0x000000ff;
		g = 0x0000ff00;
		r = 0x00ff0000;
		a = 0x00000000;
		break;
	case PTI_BGRA8:
		b = 0x000000ff;
		g = 0x0000ff00;
		r = 0x00ff0000;
		a = 0xff000000;
		break;
#endif
	case PTI_A2BGR10:
		r = 0x000003ff;
		g = 0x000ffc00;
		b = 0x3ff00000;
		a = 0xc0000000;
		break;

	default:
		return NULL;
	}

	if (scale != 1)
	{
		int nw,nh;
		qbyte *nd;
		nw = width * scale;
		nh = height * scale;
		if (nw <= 0 || nh <= 0 || nw > 128 || nh > 128)	//don't go crazy.
			return NULL;

		nd = Image_ResampleTexture(format, imagedata, width, height, NULL, nw, nh);

		surf = SDL_CreateRGBSurfaceFrom(nd, nw, nh, 32, nw*4, r, g, b, a);
		curs = SDL_CreateColorCursor(surf, hotx, hoty);
		SDL_FreeSurface(surf);

		BZ_Free(nd);
	}
	else
	{
		surf = SDL_CreateRGBSurfaceFrom((void*)imagedata, width, height, 32, width*4, r, g, b, a);
		curs = SDL_CreateColorCursor(surf, hotx, hoty);
		SDL_FreeSurface(surf);
	}
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


static void GLVID_SetIcon				(void)
{
	SDL_Surface *iconsurf = NULL;
	size_t filesize = 0;
	qbyte *filedata = NULL;
	qbyte *imagedata = NULL;

	#ifdef IMAGEFMT_PNG
	if (!filedata)
		filedata = FS_LoadMallocFile("icon.png", &filesize);
	#endif
	if (!filedata)
		filedata = FS_LoadMallocFile("icon.tga", &filesize);
	#ifdef IMAGEFMT_JPG
	if (!filedata)
		filedata = FS_LoadMallocFile("icon.jpg", &filesize);
	#endif
	#ifdef IMAGEFMT_BMP
	if (!filedata)
		filedata = FS_LoadMallocFile("icon.ico", &filesize);
	#endif
	#ifdef HAVE_LEGACY
	if (!filedata)
		filedata = FS_LoadMallocFile("darkplaces-icon.tga", &filesize);
	#endif

	if (filedata)
	{
		int imagewidth, imageheight;
		uploadfmt_t format = PTI_INVALID;
		imagedata = ReadRawImageFile(filedata, filesize, &imagewidth, &imageheight, &format, true, "icon.*");
		Z_Free(filedata);

		if (imagedata)
		{
			/* hopefully SDL can resize as appropriate
			qbyte *resized = Image_ResampleTexture(format, imagedata, imagewidth, imageheight, NULL, 64, 64);
			if (resized)
			{
				Z_Free(imagedata);
				imagedata = resized;
				imagewidth = 64;
				imageheight = 64;
			}*/
			switch(format)
			{
			case PTI_LLLA8:		//fallthrough
			case PTI_RGBA8:		iconsurf = SDL_CreateRGBSurfaceFrom(imagedata, imagewidth, imageheight, 32, 4*imagewidth, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000); break;
			case PTI_LLLX8:		//fallthrough
			case PTI_RGBX8:		iconsurf = SDL_CreateRGBSurfaceFrom(imagedata, imagewidth, imageheight, 32, 4*imagewidth, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000); break;
			case PTI_BGRA8:		iconsurf = SDL_CreateRGBSurfaceFrom(imagedata, imagewidth, imageheight, 32, 4*imagewidth, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000); break;
			case PTI_BGRX8:		iconsurf = SDL_CreateRGBSurfaceFrom(imagedata, imagewidth, imageheight, 32, 4*imagewidth, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000); break;
			case PTI_A2BGR10:	iconsurf = SDL_CreateRGBSurfaceFrom(imagedata, imagewidth, imageheight, 32, 4*imagewidth, 0x3FF00000, 0x000FFC00, 0x000003FF, 0xC0000000); break;
			default:	//others shouldn't happen.
				break;
			}
		}
	}

	if (!iconsurf)
	{
		#include "fte_eukara64.h"
//		#include "bymorphed.h"
		iconsurf = SDL_CreateRGBSurfaceFrom((void*)icon.pixel_data, icon.width, icon.height, 32, 4*icon.width, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);	//RGBA byte order on a little endian machine, at least...
	}
	SDL_SetWindowIcon(sdlwindow, iconsurf);
	SDL_FreeSurface(iconsurf);
	Z_Free(imagedata);
}

#if SDL_VERSION_ATLEAST(3,0,0)
//converts an output device name/number to an index.
//So we can use eg VGA-0 on linux and get the proper device.
static SDL_DisplayID SDLVID_GetVideoDevice(const char *devicename)
{
	char *end;
	int numdisps = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&numdisps);
	SDL_DisplayID ret = 0;	//invalid
	int idx = strtol(devicename, &end, 0);
	if (*end)
	{	//okay, so its not purely a number. scan by name
		for (idx = 0; idx < numdisps; idx++)
		{
			const char *dname = SDL_GetDisplayName(displays[idx]);
			if (dname && !Q_strcasecmp(dname, devicename))
			{
				ret = displays[idx];
				break;
			}
		}
	}
	else
	{
		if (idx < 0 || idx >= numdisps)
			ret = 0;	//invalid.
		else
			ret = displays[idx];
	}
	if (!ret)
		ret = SDL_GetPrimaryDisplay();	//FIXME: this sucks. should get the device that currently holds the cursor or w/e, so we don't piss off the user by showing it on device instead of the one with their task bar/shortcut/etc. sdl seems to be catering to wayland and doesn't seem to have any GetCursorPos (other than post-window-creation events which makes it useless).
	SDL_free(displays);
	return ret;
}
static qboolean SDLVID_GetVideoMode(rendererstate_t *info, SDL_DisplayID *display, SDL_DisplayMode *mode)
{
	*display = SDLVID_GetVideoDevice(info->devicename);
	if (info->fullscreen != 1)
		return false;

	if (SDL_GetClosestFullscreenDisplayMode(*display, info->width, info->height, info->rate, true, mode))
	{
		info->width = mode->w;
		info->height = mode->h;
		return true;	//yay
	}
	return false;	//fail
}
static void	SDLVID_EnumerateVideoModes (const char *driver, const char *output, void (*cb) (int w, int h))
{
	int nummodes, m;
	SDL_DisplayID display = SDLVID_GetVideoDevice(output);
	SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display, &nummodes);
	for (m = 0; m < nummodes; m++)
		cb(modes[m]->w, modes[m]->h);
	SDL_free(modes);
}
#else
//converts an output device name/number to an index.
//So we can use eg VGA-0 on linux and get the proper device.
static int SDLVID_GetVideoDevice(const char *devicename)
{
	char *end;
	int display = strtol(devicename, &end, 0);
	if (*end)
	{	//okay, so its not purely a number. scan by name
		display = SDL_GetNumVideoDisplays();
		if (display)
		{
			while (display --> 0)
			{
				const char *dname = SDL_GetDisplayName(display);
				if (dname && !Q_strcasecmp(dname, devicename))
					break;
			}
		}
	}
	else
	{
		if (display < 0 || display >= SDL_GetNumVideoDisplays())
			display = 0;
	}
	return display;
}
static qboolean SDLVID_GetVideoMode(rendererstate_t *info, int *display, SDL_DisplayMode *mode)
{
	SDL_DisplayMode targ;
	*display = SDLVID_GetVideoDevice(info->devicename);
	if (info->fullscreen != 1)
		return false;
	targ.w = info->width;
	targ.h = info->height;
	if (info->bpp == 30)
		targ.format = SDL_PIXELFORMAT_ARGB2101010;
	else
		targ.format = SDL_PIXELFORMAT_UNKNOWN;
	targ.driverdata = NULL;
	targ.refresh_rate = info->rate;

	if (SDL_GetClosestDisplayMode(*display, &targ, mode))
	{
		info->width = targ.w;
		info->height = targ.h;
		return true;	//yay
	}
	return false;	//fail
}
static void	SDLVID_EnumerateVideoModes (const char *driver, const char *output, void (*cb) (int w, int h))
{
	SDL_DisplayMode modeinfo;
	int modes, m;
	int display = SDLVID_GetVideoDevice(output);
	modes = SDL_GetNumDisplayModes(display);
	for (m = 0; m < modes; m++)
	{
		if (0==SDL_GetDisplayMode(display, m, &modeinfo))
			cb(modeinfo.w, modeinfo.h);
	}
}
#endif
#endif


static qboolean SDLVID_Init (rendererstate_t *info, unsigned char *palette, r_qrenderer_t qrenderer)
{
	int flags = 0;
#if SDL_VERSION_ATLEAST(2,0,0)
	int display = -1;
	SDL_DisplayMode modeinfo, *usemode;

	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");		//we understand touch events. we do NOT want to get confused with mouse motion constantly warping.
#ifdef SDL_HINT_IME_INTERNAL_EDITING
	SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "1");	//our code doesn't handle displaying non-committed text. ask to not be expected to show it, where possible.
#endif
#ifdef SDL_HINT_IME_SUPPORT_EXTENDED_TEXT
//	SDL_SetHint(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, "1");//says we don't have a limit. enable once we actually support this stuff.
#endif
#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");				//not much point having an IME if you can't see it...
#endif
#endif

#if SDL_VERSION_ATLEAST(3,0,0)
	SDL_InitSubSystem(SDL_INIT_VIDEO);
#else
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
#endif
#if !defined(FTE_TARGET_WEB) && !SDL_VERSION_ATLEAST(2,0,0)
	SDL_SetVideoMode(0, 0, 0, 0);	//to get around some SDL bugs
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
	switch(qrenderer)
	{
	default:
		return false;
#ifdef OPENGL_SDL
	case QR_OPENGL:
		SDL_GL_LoadLibrary(NULL);

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
		if (info->multisample)
		{
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, info->multisample);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		}

		flags |= SDL_WINDOW_OPENGL;
		break;
#endif
#ifdef VULKAN_SDL
	case QR_VULKAN:		
		flags |= SDL_WINDOW_VULKAN;
		break;
#endif
	}
	flags |= SDL_WINDOW_RESIZABLE;
#if SDL_VERSION_ATLEAST(3,0,0)
//	flags |= SDL_WINDOW_MOUSE_GRABBED;
	flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
	flags |= SDL_WINDOW_HIDDEN;
#else
	flags |= SDL_WINDOW_INPUT_GRABBED;
	#if SDL_VERSION_ATLEAST(2,0,1)
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	#endif
#endif

	usemode = NULL;
	if (SDLVID_GetVideoMode(info, &display, &modeinfo))
		usemode = &modeinfo;
	rf->VID_EnumerateVideoModes = SDLVID_EnumerateVideoModes;

#if SDL_VERSION_ATLEAST(3,0,0)
	sdlwindow = SDL_CreateWindow(FULLENGINENAME, info->width, info->height, flags);
#else
	sdlwindow = SDL_CreateWindow(FULLENGINENAME, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display), info->width, info->height, flags);
#endif
	if (!sdlwindow)
	{
		Con_Printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_SetWindowMinimumSize(sdlwindow, 320, 200);

	if (usemode)
	{
#if SDL_VERSION_ATLEAST(3,0,0)
		SDL_SetWindowFullscreenMode(sdlwindow, usemode);
#else
		SDL_SetWindowDisplayMode(sdlwindow, usemode);
#endif
		SDL_SetWindowFullscreen(sdlwindow, SDL_WINDOW_FULLSCREEN);
	}
	else if (info->fullscreen)
	{
#if SDL_VERSION_ATLEAST(3,0,0)
		SDL_SetWindowFullscreen(sdlwindow, SDL_WINDOW_FULLSCREEN);	//SDL decides itself now.
#else
		SDL_SetWindowFullscreen(sdlwindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
	}
	SDL_ShowWindow(sdlwindow);

#if defined(__linux__) && !SDL_VERSION_ATLEAST(3,0,0)
	if (usemode)
	{	//try to work around an nvidia bug (panning being forced active, instead of disabled).
		//if the user pans then they might get stuck with x11 at the wrong resolution, so try to back out if it would fail.
		int w, h;
		SDL_GetWindowSize(sdlwindow, &w, &h);
		Sys_SendKeyEvents();
		SDL_GetWindowSize(sdlwindow, &vid.pixelwidth, &vid.pixelheight);
		if (w != vid.pixelwidth || h != vid.pixelheight)
		{
			Con_Printf(CON_ERROR "Video mode change didn't stick (Nvidia bug?). Resorting to fullscreen-desktop mode.\n");
			SDL_SetWindowFullscreen(sdlwindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
			SDL_ShowWindow(sdlwindow);
		}
	}
#endif

	CL_UpdateWindowTitle();
	GLVID_SetIcon();

#if SDL_VERSION_ATLEAST(2,26,0)
	SDL_GetWindowSizeInPixels(sdlwindow, &vid.pixelwidth, &vid.pixelheight);
#else
	switch(qrenderer)
	{
#ifdef OPENGL_SDL
#if SDL_VERSION_ATLEAST(2,0,1)
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
#endif

#if SDL_VERSION_ATLEAST(3,0,0)
	vid.dpi_x = vid.dpi_y = SDL_GetWindowDisplayScale(sdlwindow)*96;
//#elif SDL_VERSION_ATLEAST(2,0,4)
//	SDL_GetDisplayDPI(display, NULL, &vid.dpi_x, &vid.dpi_y);	//unusable rubbish.
#else
	{	//SDL_GetDisplayDPI is unusable (depends on buggy drivers/hardware), so try and guess based on initial settings. this may cause issues with monitors using different scales though.
		int ww,wh;
		SDL_GetWindowSize(sdlwindow, &ww, &wh);
		vid.dpi_x = (96 * vid.pixelwidth)/ww;
		vid.dpi_y = (96 * vid.pixelheight)/wh;
	}
#endif


#ifdef OPENGL_SDL
	if (qrenderer == QR_OPENGL)
	{
		int srgb;
#if	SDL_VERSION_ATLEAST(3,0,0)
		vrsetup_t setup = {sizeof(setup)};
		const char *driver = SDL_GetCurrentVideoDriver();
		if (SDL_strcmp(driver, "x11") == 0)
			setup.vrplatform = VR_X11_GLX;
		else if (SDL_strcmp(driver, "wayland") == 0)
			setup.vrplatform = VR_EGL;
		else if (SDL_strcmp(driver, "windows") == 0)
			setup.vrplatform = VR_WIN_WGL;
		else
			setup.vrplatform = VR_HEADLESS;	//unsupported.
#else
		vrsetup_t setup = {sizeof(setup)};
		SDL_SysWMinfo wminfo;
		SDL_VERSION(&wminfo.version);
		SDL_GetWindowWMInfo(sdlwindow, &wminfo);
		switch(wminfo.subsystem)
		{
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
		case SDL_SYSWM_WINDOWS:
			setup.vrplatform = VR_WIN_WGL;
			break;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
		case SDL_SYSWM_X11:
			setup.vrplatform = VR_X11_GLX;
			break;
#endif
#if defined(SDL_VIDEO_DRIVER_WAYLAND) && SDL_VERSION_ATLEAST(3,0,0)
		case SDL_SYSWM_WAYLAND:
			setup.vrplatform = VR_EGL;
			break;
#endif
		default:
			setup.vrplatform = VR_HEADLESS; //unsupported platform...
			break;
		}
#endif
		if (info->vr && !info->vr->Prepare(&setup))
			info->vr = NULL;

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

		//now fill it in properly, now that we have a context.
#if	SDL_VERSION_ATLEAST(3,0,0)
		switch(setup.vrplatform)
		{
		case VR_X11_GLX:
			setup.x11_glx.display = SDL_GetPointerProperty(SDL_GetWindowProperties(sdlwindow), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
			//setup.x11_glx.screen = SDL_GetNumberProperty(SDL_GetWindowProperties(sdlwindow), SDL_PROP_WINDOW_X11_SCREEN_NUMBER, 0);
			setup.x11_glx.drawable = SDL_GetNumberProperty(SDL_GetWindowProperties(sdlwindow), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
			setup.x11_glx.visualid = 0;//???
			setup.x11_glx.glxfbconfig = 0;//???
			setup.x11_glx.glxcontext = SDL_GL_GetCurrentContext();
			break;
		case VR_EGL:	//wayland
			setup.egl.getprocaddr = (void *(*)(const char *)) SDL_EGL_GetProcAddress;
			setup.egl.egldisplay = SDL_EGL_GetCurrentDisplay();
			setup.egl.eglconfig = SDL_EGL_GetCurrentConfig();
			setup.egl.eglcontext = SDL_GL_GetCurrentContext();
			break;
		case VR_WIN_WGL:
			setup.wgl.hdc = SDL_GetPointerProperty(SDL_GetWindowProperties(sdlwindow), SDL_PROP_WINDOW_WIN32_HDC_POINTER, NULL);
			setup.wgl.hglrc = SDL_GL_GetCurrentContext();
			break;
		default:
			break;
		}
#else
		switch(wminfo.subsystem)
		{
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
		case SDL_SYSWM_WINDOWS:
			setup.wgl.hdc = wminfo.info.win.hdc;
			setup.wgl.hglrc = SDL_GL_GetCurrentContext();
			break;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
		case SDL_SYSWM_X11:
			setup.x11_glx.display = wminfo.info.x11.display;
			setup.x11_glx.drawable = wminfo.info.x11.window;
			setup.x11_glx.visualid = 0;//???
			setup.x11_glx.glxfbconfig = 0;//???
			setup.x11_glx.glxcontext = SDL_GL_GetCurrentContext();
			break;
#endif
#if defined(SDL_VIDEO_DRIVER_WAYLAND) && SDL_VERSION_ATLEAST(3,0,0)
		case SDL_SYSWM_WAYLAND:
			setup.vrplatform = VR_EGL;
			setup.egl.getprocaddr = SDL_EGL_GetProcAddress;
			setup.egl.egldisplay = SDL_EGL_GetCurrentEGLDisplay();
			setup.egl.eglconfig = SDL_EGL_GetCurrentEGLConfig();
			setup.egl.eglcontext = SDL_GL_GetCurrentContext();
			break;

#endif
		default:
			setup.vrplatform = VR_HEADLESS; //unsupported platform...
			break;
		}
#endif

		if (info->vr && !info->vr->Init(&setup, info))
		{
			info->vr->Shutdown();
			vid.vr = NULL;
		}
		else
			vid.vr = info->vr;
	}
#endif

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
//	if (vid_isfullscreen)
//		IN_ActivateMouse();

#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_DisableScreenSaver();
#else
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif
	vid_vsync.modified = true;

#ifdef _WIN32
	{	//win32 apis are very insistant upon having a window context for things that have nothing to do with windowing system stuff.
	#if	SDL_VERSION_ATLEAST(3,0,0)
		mainwindow = SDL_GetPointerProperty(SDL_GetWindowProperties(sdlwindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	#elif SDL_VERSION_ATLEAST(2,0,0)
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

#if SDL_VERSION_ATLEAST(2,0,0)
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
	INS_SetOSK(false);

#if SDL_VERSION_ATLEAST(2,0,0)
#if !SDL_VERSION_ATLEAST(3,0,0)
	SDL_SetWindowGammaRamp(sdlwindow, NULL, NULL, NULL);
#endif

	switch(qrenderer)
	{
#ifdef OPENGL_SDL
	case QR_OPENGL:
#if SDL_VERSION_ATLEAST(3,0,0)
		SDL_GL_DestroyContext(sdlcontext);
#else
		SDL_GL_DeleteContext(sdlcontext);
#endif
		break;
#endif
#ifdef VULKAN_SDL
	case QR_VULKAN:
		VK_Shutdown();
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
#if SDL_VERSION_ATLEAST(2,0,0)
		if (vid_vsync.modified)
		{
			if (*vid_vsync.string)
			{
				//if swap_tear fails, try without.
#if SDL_VERSION_ATLEAST(3,0,0)
				if (!SDL_GL_SetSwapInterval(vid_vsync.ival) && vid_vsync.ival < 0)
#else
				if (SDL_GL_SetSwapInterval(vid_vsync.ival) == -1 && vid_vsync.ival < 0)
#endif
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
		if (!in_windowed_mouse.value)
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
#if SDL_VERSION_ATLEAST(3,0,0)
	return false;
#elif SDL_VERSION_ATLEAST(2,0,0)
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
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_SetWindowTitle(sdlwindow, text);
#else
	SDL_WM_SetCaption(text, NULL);
#endif
}



#ifdef VULKAN_SDL
static qboolean VKSDL_CreateSurface(void)
{
#if SDL_VERSION_ATLEAST(3,0,0)
	return SDL_Vulkan_CreateSurface(sdlwindow, vk.instance, vkallocationcb, &vk.surface);
#else
	return SDL_Vulkan_CreateSurface(sdlwindow, vk.instance, &vk.surface);
#endif
}
static qboolean VKVID_Init (rendererstate_t *info, unsigned char *palette)
{
	unsigned extcount;
#if SDL_VERSION_ATLEAST(3,0,0)
	char const* const*extnames;
#else
	const char **extnames;
#endif
	if (!SDLVID_Init(info, palette, QR_VULKAN))
		return false;
#if SDL_VERSION_ATLEAST(3,0,0)
	extnames = SDL_Vulkan_GetInstanceExtensions(&extcount);
#else
	if (!SDL_Vulkan_GetInstanceExtensions(sdlwindow, &extcount, NULL))
		return false;
	extnames = alloca(sizeof(*extnames)*extcount);
	if (!SDL_Vulkan_GetInstanceExtensions(sdlwindow, &extcount, extnames))
		return false;
#endif

	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
	if (!VK_Init(info, extnames, extcount, VKSDL_CreateSurface, NULL))
		return false;
	return true;
}
static qboolean VKVID_EnumerateDevices(void *usercontext, void(*callback)(void *context, const char *devicename, const char *outputname, const char *desc))
{
    qboolean ret = false;
#ifdef VK_NO_PROTOTYPES
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
#endif
	//FIXME: enumerate monitor names too, not just gpus.
    ret = VK_EnumerateDevices(usercontext, callback, "Vulkan-SDL-", vkGetInstanceProcAddr);
    return ret;
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

	"no more",

	NULL,	//GetPriority
	SDLVID_EnumerateVideoModes,
	VKVID_EnumerateDevices,
	NULL,	//MayRefresh
};
#else
rendererinfo_t vkrendererinfo;
#endif
