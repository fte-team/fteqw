/*
Copyright (C) 2006-2007 Mark Olsen

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

#include "quakedef.h"
#include "glquake.h"

extern int sys_glesversion;
extern float sys_dpi_x;
extern float sys_dpi_y;

static dllhandle_t *sys_gl_module = NULL;

void *GLES_GetSymbol(char *symname)
{
	void *ret;

	ret = Sys_GetAddressForName(sys_gl_module, symname);

	if (!ret)
		Sys_Warn("GLES_GetSymbol: couldn't find %s\n", symname);
	return ret;
}

#if 1
void GLVID_SwapBuffers(void)
{
}
void GLVID_DeInit(void)
{
	if (sys_gl_module)
		Sys_CloseLibrary(sys_gl_module);
	sys_gl_module = NULL;
}
qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	/*
	Android 2.2 does not provide EGL headers, thus the context was already created within java code.
	This means that we cannot change width/height/bpp/etc.
	But we still initialize everything as if we did.

	Pros: Works on android 2.2, which reportedly is 50% of all androids right now.
	Cons: vid_restart cannot change width/height/bpp/rate/etc.
	Mneh: you probably couldn't change width/height/rate anyway.
	Cons: any gl objects which were not destroyed properly will leak.
	Mneh: we should have cleaned them up properly in the first place.
	Cons: GL_EndRendering call will not swap buffers. Buffers will be swapped on return to java.
	*/

	if (!sys_glesversion)
	{
		Sys_Printf("GLES version not specified yet\n");
		return false;
	}

	if (sys_glesversion >= 2)
		Sys_Printf("Loading GLES2 driver\n");
	else
		Sys_Printf("Loading GLES1 driver\n");
	sys_gl_module = Sys_LoadLibrary((sys_glesversion>=2)?"libGLESv2.so":"libGLESv1_CM.so", NULL);
	if (!sys_gl_module)
	{
		GLVID_DeInit();
		return false;
	}

	vid.dpi_x = sys_dpi_x;
	vid.dpi_y = sys_dpi_y;

	vid.activeapp = true;

	return GL_Init(info, GLES_GetSymbol);
}
#else

#include <EGL/egl.h>

extern void *sys_window;
static EGLDisplay sys_display = EGL_NO_DISPLAY;
static EGLSurface sys_surface = EGL_NO_SURFACE;
static EGLContext sys_context = EGL_NO_CONTEXT;

void GLVID_DeInit(void)
{
	if (sys_display != EGL_NO_DISPLAY)
	{
		eglMakeCurrent(sys_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (sys_context != EGL_NO_CONTEXT)
			eglDestroyContext(sys_display, sys_context);
		if (sys_surface != EGL_NO_SURFACE)
			eglDestroySurface(sys_display, sys_surface);
		eglTerminate(sys_display);
		sys_context = EGL_NO_CONTEXT;
		sys_surface = EGL_NO_SURFACE;
		sys_display = EGL_NO_DISPLAY;
	}
	if (sys_gl_module != NULL)
	{
		Sys_CloseLibrary(sys_gl_module);
		sys_gl_module = NULL;
	}
}

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	vid.pixelwidth = info->width;
	vid.pixelheight = info->height;
	vid.numpages = 3;

	const EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, (sys_glesversion>=2)?EGL_OPENGL_ES2_BIT:EGL_OPENGL_ES_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, (info->bpp==16)?5:8,
		EGL_GREEN_SIZE, (info->bpp==16)?6:8,
		EGL_RED_SIZE, (info->bpp==16)?5:8,
		EGL_DEPTH_SIZE, 16,
//		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLint ctxattribs[] = {EGL_CONTEXT_CLIENT_VERSION, sys_glesversion, EGL_NONE};
	EGLint w, h, dummy, format;
	EGLint numConfigs;
	EGLConfig config;

	sys_gl_module = Sys_LoadLibrary((sys_glesversion>=2)?"libGLESv2.so":"libGLESv1_CM.so", NULL);
	if (!sys_gl_module)
	{
		GLVID_DeInit();
		return false;
	}

	sys_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (sys_display == EGL_NO_DISPLAY)
	{
		GLVID_DeInit();
		return false;
	}
	if (!eglInitialize(sys_display, NULL, NULL))
	{
		GLVID_DeInit();
		return false;
	}
	eglChooseConfig(sys_display, attribs, &config, 1, &numConfigs);

	eglGetConfigAttrib(sys_display, config, EGL_NATIVE_VISUAL_ID, &format);

	ANativeWindow_setBuffersGeometry(sys_window, 0, 0, format);

	sys_surface = eglCreateWindowSurface(sys_display, config, sys_window, NULL);
	sys_context = eglCreateContext(sys_display, config, NULL, ctxattribs);


	if (eglMakeCurrent(sys_display, sys_surface, sys_surface, sys_context) == EGL_FALSE)
		return false;

	eglQuerySurface(sys_display, sys_surface, EGL_WIDTH, &w);
	eglQuerySurface(sys_display, sys_surface, EGL_HEIGHT, &h);
	vid.pixelwidth = w;
	vid.pixelheight = h;

	return GL_Init(info, GLES_GetSymbol);
}

void GLVID_SwapBuffers(void)
{
	eglSwapBuffers(sys_display, sys_surface);
}
#endif

qboolean GLVID_ApplyGammaRamps (unsigned int gammarampsize, unsigned short *ramps)
{
	return false;
}

void GLVID_SetCaption(const const char *caption)
{
}

