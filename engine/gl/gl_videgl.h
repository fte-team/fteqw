#ifndef __GL_VIDEGL_H__
#define __GL_VIDEGL_H__

#include "quakedef.h"
#define NativeWindowType EGLNativeWindowType
#include <EGL/egl.h>
#ifndef _WIN32
	#include <dlfcn.h>
#endif

void *EGL_Proc(char *f);
void EGL_UnloadLibrary(void);
qboolean EGL_LoadLibrary(char *driver);
void EGL_Shutdown(void);
void EGL_SwapBuffers (void);
qboolean EGL_Init (rendererstate_t *info, unsigned char *palette, EGLNativeWindowType window, EGLNativeDisplayType dpy);

#endif
