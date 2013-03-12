#ifndef __GL_VIDEGL_H__
#define __GL_VIDEGL_H__

#include "quakedef.h"
#define NativeWindowType EGLNativeWindowType
#include <EGL/egl.h>
#include <dlfcn.h>

void *EGL_Proc(char *f);
void EGL_UnloadLibrary(void);
qboolean EGL_LoadLibrary(char *driver);
void EGL_Shutdown(void);
void EGL_BeginRendering (void);
void EGL_EndRendering (void);
qboolean EGL_Init (rendererstate_t *info, unsigned char *palette, EGLNativeWindowType window, EGLNativeDisplayType dpy);

#endif
