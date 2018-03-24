#include "quakedef.h"
#if defined(GLQUAKE) && defined(USE_EGL)
#include "gl_videgl.h"

extern cvar_t vid_vsync;

EGLContext eglctx = EGL_NO_CONTEXT;
EGLDisplay egldpy = EGL_NO_DISPLAY;
EGLSurface eglsurf = EGL_NO_SURFACE;

static dllhandle_t *egllibrary;
static dllhandle_t *eslibrary;

static EGLint		(EGLAPIENTRY *qeglGetError)(void);

static EGLDisplay	(EGLAPIENTRY *qeglGetPlatformDisplay)(EGLenum platform, void *native_display, const EGLint *attrib_list);
static EGLDisplay	(EGLAPIENTRY *qeglGetDisplay)(EGLNativeDisplayType display_id);
static EGLBoolean	(EGLAPIENTRY *qeglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
static EGLBoolean	(EGLAPIENTRY *qeglTerminate)(EGLDisplay dpy);

static EGLBoolean	(EGLAPIENTRY *qeglGetConfigs)(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
static EGLBoolean	(EGLAPIENTRY *qeglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);

static EGLSurface	(EGLAPIENTRY *qeglCreatePlatformWindowSurface)(EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list);
static EGLSurface	(EGLAPIENTRY *qeglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
static EGLBoolean	(EGLAPIENTRY *qeglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
static EGLBoolean	(EGLAPIENTRY *qeglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);

static EGLBoolean	(EGLAPIENTRY *qeglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
static EGLBoolean	(EGLAPIENTRY *qeglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
static EGLContext	(EGLAPIENTRY *qeglCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
static EGLBoolean	(EGLAPIENTRY *qeglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
static void *		(EGLAPIENTRY *qeglGetProcAddress) (char *name);

static EGLBoolean 	(EGLAPIENTRY *qeglSwapInterval) (EGLDisplay display, EGLint interval);

static dllfunction_t qeglfuncs[] =
{
	{(void*)&qeglGetError, "eglGetError"},
	
	{(void*)&qeglGetDisplay, "eglGetDisplay"},
	{(void*)&qeglInitialize, "eglInitialize"},
	{(void*)&qeglTerminate, "eglTerminate"},

	{(void*)&qeglGetConfigs, "eglGetConfigs"},
	{(void*)&qeglChooseConfig, "eglChooseConfig"},

	{(void*)&qeglCreateWindowSurface, "eglCreateWindowSurface"},
	{(void*)&qeglDestroySurface, "eglDestroySurface"},
	{(void*)&qeglQuerySurface, "eglQuerySurface"},

	{(void*)&qeglSwapBuffers, "eglSwapBuffers"},
	{(void*)&qeglMakeCurrent, "eglMakeCurrent"},
	{(void*)&qeglCreateContext, "eglCreateContext"},
	{(void*)&qeglDestroyContext, "eglDestroyContext"},

	{(void*)&qeglGetProcAddress, "eglGetProcAddress"},

	//EGL 1.1
	{(void*)&qeglSwapInterval,	"eglSwapInterval"},

	{NULL}
};


void *EGL_Proc(char *f)
{
	void *proc = NULL;

	/*
	char fname[512];
	{
		sprintf(fname, "wrap_%s", f);
		f = fname;
	}
	*/

	if (!proc)
		proc = Sys_GetAddressForName(eslibrary, f);
	if (!proc)
		proc = Sys_GetAddressForName(egllibrary, f);

	//eglGetProcAddress functions must work regardless of context...
	//FIXME: this means lots of false-positives.
	//as well as lots of thunks, which will result in slowdowns.
	if (qeglGetProcAddress)
		proc = qeglGetProcAddress(f);

	return proc;
}

const char *EGL_GetErrorString(int error)
{
	switch(error)
	{
	case EGL_BAD_ACCESS:		return "BAD_ACCESS";
	case EGL_BAD_ALLOC:		return "BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:		return "BAD_ATTRIBUTE";
	case EGL_BAD_CONFIG:		return "BAD_CONFIG";
	case EGL_BAD_CONTEXT:		return "BAD_CONEXT";
	case EGL_BAD_CURRENT_SURFACE:	return "BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:		return "BAD_DISPLAY";
	case EGL_BAD_MATCH:		return "BAD_MATCH";
	case EGL_BAD_NATIVE_PIXMAP:	return "BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:	return "BAD_NATIVE_WINDOW";
	case EGL_BAD_PARAMETER:		return "BAD_PARAMETER";
	case EGL_BAD_SURFACE:		return "BAD_SURFACE";
	default:			return va("EGL:%#x", error);
	}
}

void EGL_UnloadLibrary(void)
{
	if (egllibrary)
		Sys_CloseLibrary(egllibrary);
	if (egllibrary == eslibrary)
		eslibrary = NULL;
	if (eslibrary)
		Sys_CloseLibrary(eslibrary);
	eslibrary = egllibrary = NULL;
}

qboolean EGL_LoadLibrary(char *driver)
{
	/*	linux seem to load glesv2 first for dependency issues.
		(most things are expected to statically link to their libs)
		strictly speaking, EGL says that functions should work regardless of context.
		(which of course makes portability a nightmare, especially on windows where static linking is basically impossible)
		(android's EGL bugs out if you use eglGetProcAddress for core functions too)
	*/
	Sys_Printf("Attempting to dlopen libGLESv2... ");
	eslibrary = Sys_LoadLibrary("libGLESv2", NULL);
	if (!eslibrary)
	{
		Sys_Printf("failed\n");
//		return false;
	}
	else
		Sys_Printf("success\n");
#ifndef _WIN32
	if (!eslibrary)
	{
		eslibrary = dlopen("libGL.so.1.2", RTLD_NOW|RTLD_GLOBAL);
		if (eslibrary) Sys_Printf("Loaded libGL.so.1.2\n");
	}
	if (!eslibrary)
	{
		eslibrary = dlopen("libGL.so.1", RTLD_NOW|RTLD_GLOBAL);
		if (eslibrary) Sys_Printf("Loaded libGL.so.1\n");
	}
	if (!eslibrary)
	{
		eslibrary = dlopen("libGL", RTLD_NOW|RTLD_GLOBAL);
		if (eslibrary) Sys_Printf("Loaded libGL\n");
	}
#endif
	if (!eslibrary)
		Sys_Printf("unable to load some libGL\n");
	
	Sys_Printf("Attempting to dlopen libEGL... ");
	egllibrary = Sys_LoadLibrary("libEGL", qeglfuncs);
	if (!egllibrary)
	{
		Sys_Printf("failed\n");
		Con_Printf("libEGL library not loadable\n");
		/* TODO: some implementations combine EGL/GLESv2 into single library... */
		Sys_CloseLibrary(eslibrary);
		return false;
	}
	Sys_Printf("success\n");

	//these are from egl1.5
	qeglGetPlatformDisplay		= EGL_Proc("eglGetPlatformDisplay");
	qeglCreatePlatformWindowSurface	= EGL_Proc("eglCreatePlatformWindowSurface");
	//and in case they arn't defined...
	if (!qeglGetPlatformDisplay)		qeglGetPlatformDisplay		= EGL_Proc("eglGetPlatformDisplayEXT");
	if (!qeglCreatePlatformWindowSurface)	qeglCreatePlatformWindowSurface	= EGL_Proc("eglCreatePlatformWindowSurfaceEXT");

	return true;
}

void EGL_Shutdown(void)
{
	if (eglctx == EGL_NO_CONTEXT)
		return;

	qeglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	qeglDestroyContext(egldpy, eglctx);

	if (eglsurf != EGL_NO_SURFACE)
		qeglDestroySurface(egldpy, eglsurf);

	qeglTerminate(egldpy);

	eglctx = EGL_NO_CONTEXT;
	egldpy = EGL_NO_DISPLAY;
	eglsurf = EGL_NO_SURFACE;
}

static void EGL_UpdateSwapInterval(void)
{
	int interval;
	vid_vsync.modified = false;
	if (*vid_vsync.string)
		interval = vid_vsync.ival;
	else
		interval = 1;	//default is to always vsync, according to EGL docs, so lets just do that.

	if (qeglSwapInterval)
		qeglSwapInterval(egldpy, interval);
}

void EGL_SwapBuffers (void)
{
	if (vid_vsync.modified)
		EGL_UpdateSwapInterval();

	TRACE(("EGL_SwapBuffers\n"));
	TRACE(("swapping buffers\n"));
	qeglSwapBuffers(egldpy, eglsurf);
	/* TODO: check result? */
	TRACE(("EGL_SwapBuffers done\n"));
}

qboolean EGL_Init (rendererstate_t *info, unsigned char *palette, int eglplat, void *nwindow, void *ndpy, EGLNativeWindowType windowid, EGLNativeDisplayType dpyid)
{
	EGLint numconfig;
	EGLConfig cfg;
	EGLint major, minor;
	EGLint attrib[] =
	{
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
//		EGL_BUFFER_SIZE, info->bpp,
//		EGL_SAMPLES, info->multisample,
//		EGL_STENCIL_SIZE, 8,
		EGL_ALPHA_MASK_SIZE, 0,
		EGL_DEPTH_SIZE, 16,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	EGLint wndattrib[] =
	{
//		EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_LINEAR_KHR,

		EGL_NONE
	};
	EGLint contextattr[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,	//requires EGL 1.3
		EGL_NONE, EGL_NONE
	};

/*	if (!EGL_LoadLibrary(""))
	{
		Con_Printf(CON_ERROR "EGL: unable to load library!\n");
		return false;
	}
*/

	if (qeglGetPlatformDisplay && eglplat)
		egldpy = qeglGetPlatformDisplay(eglplat, ndpy, NULL/*attribs*/);
	else
		egldpy = qeglGetDisplay(dpyid);
	if (egldpy == EGL_NO_DISPLAY)
	{
		Con_Printf(CON_WARNING "EGL: creating default display\n");
		egldpy = qeglGetDisplay(EGL_DEFAULT_DISPLAY);
	}
	if (egldpy == EGL_NO_DISPLAY)
	{
		Con_Printf(CON_ERROR "EGL: can't get display!\n");
		return false;
	}

	//NOTE: mesa's egl really loves to crash on this call, and I define crash as 'anything that fails to return to caller', which fucks everything up.
	if (!qeglInitialize(egldpy, &major, &minor))
	{
		Con_Printf(CON_ERROR "EGL: can't initialize display!\n");
		return false;
	}

/*
	if (!qeglGetConfigs(egldpy, NULL, 0, &numconfigs) || !numconfigs)
	{
		Con_Printf(CON_ERROR "EGL: can't get configs!\n");
		return false;
	}
*/

	if (!qeglChooseConfig(egldpy, attrib, &cfg, 1, &numconfig))
	{
		Con_Printf(CON_ERROR "EGL: can't choose config!\n");
		return false;
	}
	
	if (!numconfig)
	{
		Con_Printf(CON_ERROR "EGL: no configs!\n");
		return false;
	}

	if (qeglCreatePlatformWindowSurface)
		eglsurf = qeglCreatePlatformWindowSurface(egldpy, cfg, nwindow, info->srgb?wndattrib:NULL);
	else
		eglsurf = qeglCreateWindowSurface(egldpy, cfg, windowid, info->srgb?wndattrib:NULL);
	if (eglsurf == EGL_NO_SURFACE)
	{
		int err = qeglGetError();
		if (eglplat == EGL_PLATFORM_WAYLAND_KHR && err == EGL_BAD_CONTEXT)	//slightly more friendly error that slags off nvidia for their refusal to implement existing standards, as is apparently appropriate.
			Con_Printf(CON_ERROR "EGL: eglCreateWindowSurface failed: Bad Display. This often happens with nvidia... Try different drivers...\n");
		else
			Con_Printf(CON_ERROR "EGL: eglCreateWindowSurface failed: %s\n", EGL_GetErrorString(err));
		return false;
	}
	
	eglctx = qeglCreateContext(egldpy, cfg, EGL_NO_SURFACE, contextattr);
	if (eglctx == EGL_NO_CONTEXT)
	{
		Con_Printf(CON_ERROR "EGL: no context!\n");
		return false;
	}

	if (!qeglMakeCurrent(egldpy, eglsurf, eglsurf, eglctx))
	{
		Con_Printf(CON_ERROR "EGL: can't make current!\n");
		return false;
	}
	
	if (eglplat == EGL_PLATFORM_WAYLAND_KHR)
	{	//if we don't do this, only the first frame will get displayed, and we'll lock up
		qeglSwapInterval = NULL;
		Con_DPrintf(CON_WARNING"Wayland: Disabling vsync controls to work around wayland bug\n");
	}

	EGL_UpdateSwapInterval();

	return true;
}
#endif

