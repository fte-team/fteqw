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

#ifdef GL_ES_VERSION_2_0
qboolean gles2 = true;
#else
qboolean gles2 = false;
#endif

static dllhandle_t *sys_gl_module = NULL;

void *GLES_GetSymbol(char *symname)
{
	void *ret;

	ret = Sys_GetAddressForName(sys_gl_module, symname);

	if (!ret)
		Sys_Warn("GLES_GetSymbol: couldn't find %s\n", symname);
	return ret;
}

void GLVID_SetPalette (unsigned char *palette)
{
	qbyte *pal;
	unsigned int r,g,b;
	int i;
	unsigned *table1;
	extern qbyte gammatable[256];

	pal = palette;
	table1 = d_8to24rgbtable;
	for (i=0 ; i<256 ; i++)
	{
		r = gammatable[pal[0]];
		g = gammatable[pal[1]];
		b = gammatable[pal[2]];
		pal += 3;
		
		*table1++ = LittleLong((255<<24) + (r<<0) + (g<<8) + (b<<16));
	}
	d_8to24rgbtable[255] &= LittleLong(0xffffff);	// 255 is transparent
}

#if 1
void GL_BeginRendering(void)
{
}
void GL_EndRendering (void)
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

	if (gles2)
		Sys_Printf("Loading GLES2 driver\n");
	else
		Sys_Printf("Loading GLES1 driver\n");
	sys_gl_module = Sys_LoadLibrary(gles2?"libGLESv2.so":"libGLESv1_CM.so", NULL);
	if (!sys_gl_module)
	{
		GLVID_DeInit();
		return false;
	}

	GLVID_SetPalette (palette);
	GL_Init(GLES_GetSymbol);
	vid.recalc_refdef = 1;
	return true;
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
	gl_canstencil = 0;

	const EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, gles2?EGL_OPENGL_ES2_BIT:EGL_OPENGL_ES_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, (info->bpp==16)?5:8,
		EGL_GREEN_SIZE, (info->bpp==16)?6:8,
		EGL_RED_SIZE, (info->bpp==16)?5:8,
		EGL_DEPTH_SIZE, 16,
//		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLint ctxattribs[] = {EGL_CONTEXT_CLIENT_VERSION, gles2?2:1, EGL_NONE};
	EGLint w, h, dummy, format;
	EGLint numConfigs;
	EGLConfig config;

	sys_gl_module = Sys_LoadLibrary(gles2?"libGLESv2.so":"libGLESv1_CM.so", NULL);
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

        GLVID_SetPalette (palette);
	GL_Init(GLES_GetSymbol);
	vid.recalc_refdef = 1;
	return true;
}

void GL_BeginRendering(void)
{
}

void GL_EndRendering (void)
{
	eglSwapBuffers(sys_display, sys_surface);
}
#endif

void GL_DoSwap(void)
{
}

void GLVID_ShiftPalette (unsigned char *palette)
{
//	if (gammaworks)
	{
	}
}

void Sys_SendKeyEvents(void)
{
}

void GLVID_SetCaption(char *caption)
{
}

extern qboolean mouse_active;

cvar_t m_filter = CVARF("m_filter", "1", CVAR_ARCHIVE);

extern cvar_t _windowed_mouse;

float mouse_x, mouse_y;
float old_mouse_x, old_mouse_y;

void IN_Shutdown(void)
{
}

void IN_ReInit()
{
	Cvar_Register (&m_filter, "input controls");
}

void IN_Init(void)
{
	IN_ReInit();
}

void IN_Commands(void)
{
}

void IN_Move (float *movements, int pnum)
{
	extern int mousecursor_x, mousecursor_y;
	extern int mousemove_x, mousemove_y;

	if (pnum != 0)
		return;	//we're lazy today.

	if (m_filter.value)
	{
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	if(in_xflip.value) mouse_x *= -1;

   	if (Key_MouseShouldBeFree())
	{
		mousemove_x += mouse_x;
		mousemove_y += mouse_y;

		if (mousecursor_y<0)
			mousecursor_y=0;
		if (mousecursor_x<0)
			mousecursor_x=0;

		if (mousecursor_x >= vid.width)
			mousecursor_x = vid.width - 1;

		if (mousecursor_y >= vid.height)
			mousecursor_y = vid.height - 1;

		mouse_x = mouse_y = 0;
#ifdef VM_UI
		UI_MousePosition(mousecursor_x, mousecursor_y);
#endif
	}


	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;
   
	if ( (in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1) ))
	{
		if (movements)
			movements[1] += m_side.value * mouse_x;
	}
	else
	{
		cl.viewangles[pnum][YAW] -= m_yaw.value * mouse_x;
	}
	if (in_mlook.state[pnum] & 1)
		V_StopPitchDrift (pnum);
   
	if ( (in_mlook.state[pnum] & 1) && !(in_strafe.state[pnum] & 1)) {
		cl.viewangles[pnum][PITCH] += m_pitch.value * mouse_y;
		CL_ClampPitch(pnum);
	}
	else
	{
		if (movements)
		{
			if ((in_strafe.state[pnum] & 1) && noclip_anglehack)
				movements[2] -= m_forward.value * mouse_y;
			else
				movements[0] -= m_forward.value * mouse_y;
		}
	}
	mouse_x = mouse_y = 0.0;
}

