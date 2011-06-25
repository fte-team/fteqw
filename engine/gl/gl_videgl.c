#include "gl_videgl.h"

EGLContext eglctx = EGL_NO_CONTEXT;
EGLDisplay egldpy = EGL_NO_DISPLAY;
EGLSurface eglsurf = EGL_NO_SURFACE;

static dllhandle_t egllibrary;
static dllhandle_t eslibrary;

static EGLint (*qeglGetError)(void);

static EGLDisplay (*qeglGetDisplay)(EGLNativeDisplayType display_id);
static EGLBoolean (*qeglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
static EGLBoolean (*qeglTerminate)(EGLDisplay dpy);

static EGLBoolean (*qeglGetConfigs)(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
static EGLBoolean (*qeglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);

static EGLSurface (*qeglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
static EGLBoolean (*qeglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
static EGLBoolean (*qeglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);

static EGLBoolean (*qeglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
static EGLBoolean (*qeglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
static EGLContext (*qeglCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
static EGLBoolean (*qeglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
static void *(*qeglGetProcAddress) (char *name);

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

	if (qeglGetProcAddress)
		proc = qeglGetProcAddress(f);
	if (!proc)
		proc = Sys_GetAddressForName(eslibrary, f);
	if (!proc)
		proc = Sys_GetAddressForName(egllibrary, f);

	return proc;
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
	/* apps seem to load glesv2 first for dependency issues */
	eslibrary = Sys_LoadLibrary("libGLESv2", NULL);
	if (!eslibrary)
		return false;
	
	egllibrary = Sys_LoadLibrary("libEGL", qeglfuncs);
	if (!egllibrary)
	{
		/* TODO: some implementations combine EGL/GLESv2 into single library... */
		Sys_CloseLibrary(eslibrary);
		return false;
	}

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

void EGL_BeginRendering (void)
{
}


void EGL_EndRendering (void)
{
	qeglSwapBuffers(egldpy, eglsurf);
	/* TODO: check result? */
}

qboolean EGL_Init (rendererstate_t *info, unsigned char *palette, EGLNativeWindowType window)
{
	EGLint numconfig;
	EGLConfig cfg;
	EGLint attrib[] =
	{
		EGL_BUFFER_SIZE, info->bpp,
		EGL_SAMPLES, info->multisample,
		EGL_STENCIL_SIZE, 8,
		EGL_ALPHA_MASK_SIZE, 8,
		EGL_DEPTH_SIZE, 16,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	EGLint contextattr[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE, EGL_NONE
	};

	if (!EGL_LoadLibrary(""))
	{
		Con_Printf(CON_ERROR "EGL: unable to load library!\n");
		return false;
	}

	egldpy = qeglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (egldpy == EGL_NO_DISPLAY)
	{
		Con_Printf(CON_ERROR "EGL: can't get display!\n");
		return false;
	}

	if (!qeglInitialize(egldpy, NULL, NULL))
	{
		Con_Printf(CON_ERROR "EGL: can't initialize display!");
		return false;
	}

/*
	if (!qeglGetConfigs(egldpy, NULL, 0, &numconfigs) || !numconfigs)
	{
		Con_Printf(CON_ERROR "EGL: can't get configs!");
		return false;
	}
*/

	if (!qeglChooseConfig(egldpy, attrib, &cfg, 1, &numconfig))
	{
		Con_Printf(CON_ERROR "EGL: can't choose config!");
		return false;
	}

	eglsurf = qeglCreateWindowSurface(egldpy, cfg, window, NULL);
	if (eglsurf == EGL_NO_SURFACE)
	{
		Con_Printf(CON_ERROR "EGL: no surface!");
		return false;
	}

	eglctx = qeglCreateContext(egldpy, cfg, EGL_NO_SURFACE, contextattr);
	if (eglctx == EGL_NO_CONTEXT)
	{
		Con_Printf(CON_ERROR "EGL: no context!");
		return false;
	}

	if (!qeglMakeCurrent(egldpy, eglsurf, eglsurf, eglctx))
	{
		Con_Printf(CON_ERROR "EGL: can't make current!");
		return false;
	}

	return true;
}
