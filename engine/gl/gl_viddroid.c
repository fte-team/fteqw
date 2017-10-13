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

extern float sys_dpi_x;
extern float sys_dpi_y;
extern cvar_t vid_vsync;

static dllhandle_t *sys_gl_module = NULL;
static rendererinfo_t gles1rendererinfo;

static void *GLES_GetSymbol(char *symname)
{
	void *ret;

	ret = Sys_GetAddressForName(sys_gl_module, symname);

	if (!ret)
		Sys_Warn("GLES_GetSymbol: couldn't find %s\n", symname);
	return ret;
}


#include <EGL/egl.h>
#include <android/native_window.h>
#include <jni.h>
#include <android/native_window_jni.h>

/*android is a real fucking pain*/


static EGLDisplay sys_display;
static EGLSurface sys_surface;
static EGLContext sys_context;
static jobject sys_jsurface;
extern JNIEnv *sys_jenv;

extern qboolean r_forceheadless;
JNIEXPORT void JNICALL Java_com_fteqw_FTEDroidEngine_setwindow(JNIEnv *env, jobject obj, 
				jobject surface)
{
	if (sys_jsurface)
		(*env)->DeleteGlobalRef(sys_jenv, sys_jsurface);
	sys_jenv = env;
	sys_jsurface = surface?(*env)->NewGlobalRef(sys_jenv, surface):NULL;

	r_forceheadless = (sys_jsurface == NULL);

	if (qrenderer)	//if the window changed then we need to restart everything to match it, BEFORE we return from this function... :(
		R_RestartRenderer_f();
}

void GLVID_DeInit(void)
{
	if (sys_display)
	{
		eglMakeCurrent(sys_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (sys_context)
			eglDestroyContext(sys_display, sys_context);

		if (sys_surface != EGL_NO_SURFACE)
			eglDestroySurface(sys_display, sys_surface);

		eglTerminate(sys_display);
	}
	sys_display = EGL_NO_DISPLAY;
	sys_context = EGL_NO_CONTEXT;
	sys_surface = EGL_NO_SURFACE;

Sys_Printf("GLVID_DeInited\n");
}

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
Sys_Printf("GLVID_Initing...\n");
	if (!sys_jsurface)
	{
		Sys_Printf("GLVID_Init failed: no window known yet\n");
		return false;	//not at this time...
	}

//	vid.pixelwidth = ANativeWindow_getWidth(sys_window);
//	vid.pixelheight = ANativeWindow_getHeight(sys_window);

	vid.numpages = 3;

	EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, (info->bpp==16)?5:8,
		EGL_GREEN_SIZE, (info->bpp==16)?6:8,
		EGL_RED_SIZE, (info->bpp==16)?5:8,
		EGL_DEPTH_SIZE, 16,
//		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLint w, h, format;
	EGLint numConfigs;
	EGLConfig config;
	int glesversion;

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
#ifdef EGL_VERSION_1_5
	attribs[1] = EGL_OPENGL_ES3_BIT;
	if (info->renderer==&gles1rendererinfo || !eglChooseConfig(sys_display, attribs, &config, 1, &numConfigs))
#endif
	{
		attribs[1] = EGL_OPENGL_ES2_BIT;
		if (info->renderer==&gles1rendererinfo || !eglChooseConfig(sys_display, attribs, &config, 1, &numConfigs))
		{
			//gles2 was added in egl1.3
			attribs[1] = EGL_OPENGL_ES_BIT;
			if (!eglChooseConfig(sys_display, attribs, &config, 1, &numConfigs))
			{
				//EGL_RENDERABLE_TYPE added in egl1.2
				if (!eglChooseConfig(sys_display, attribs+2, &config, 1, &numConfigs))
				{
					GLVID_DeInit();
					return false;
				}
			}
		}
	}


	eglGetConfigAttrib(sys_display, config, EGL_RENDERABLE_TYPE, &format);
	if (info->renderer==&gles1rendererinfo && (format & EGL_OPENGL_ES_BIT))
		glesversion = 1;
#ifdef EGL_VERSION_1_5
	else if (format & EGL_OPENGL_ES3_BIT)
		glesversion = 3;
#endif
	else if (format & EGL_OPENGL_ES2_BIT)
		glesversion = 2;
	else
		glesversion = 1;

	Sys_Printf("Creating gles %i context\n", glesversion);

	sys_gl_module = Sys_LoadLibrary((glesversion>=2)?"libGLESv2.so":"libGLESv1_CM.so", NULL);
	if (!sys_gl_module)
	{
		GLVID_DeInit();
		return false;
	}

	eglGetConfigAttrib(sys_display, config, EGL_NATIVE_VISUAL_ID, &format);
	ANativeWindow *anwindow = ANativeWindow_fromSurface(sys_jenv, sys_jsurface);
	ANativeWindow_setBuffersGeometry(anwindow, 0, 0, format);

	sys_surface = eglCreateWindowSurface(sys_display, config, anwindow, NULL);
	ANativeWindow_release(anwindow);
	if (!sys_surface)
		return false;
	EGLint ctxattribs[] = {EGL_CONTEXT_CLIENT_VERSION, glesversion, EGL_NONE};
	sys_context = eglCreateContext(sys_display, config, NULL, glesversion>1?ctxattribs:NULL);


	if (eglMakeCurrent(sys_display, sys_surface, sys_surface, sys_context) == EGL_FALSE)
		return false;

	eglQuerySurface(sys_display, sys_surface, EGL_WIDTH, &w);
	eglQuerySurface(sys_display, sys_surface, EGL_HEIGHT, &h);
	vid.pixelwidth = w;
	vid.pixelheight = h;

	if (!GL_Init(info, GLES_GetSymbol))
		return false;
	Sys_Printf("GLVID_Inited...\n");
	vid_vsync.modified = true;
	return true;
}

void GLVID_SwapBuffers(void)
{
	if (vid_vsync.modified)
	{
		int interval;
		vid_vsync.modified = false;
		if (*vid_vsync.string)
			interval = vid_vsync.ival;
		else
			interval = 1;	//default is to always vsync, according to EGL docs, so lets just do that.
		eglSwapInterval(sys_display, interval);
	}

	eglSwapBuffers(sys_display, sys_surface);

	EGLint w, h;
	eglQuerySurface(sys_display, sys_surface, EGL_WIDTH, &w);
	eglQuerySurface(sys_display, sys_surface, EGL_HEIGHT, &h);
	if (w != vid.pixelwidth || h != vid.pixelheight)
	{
		vid.pixelwidth = w;
		vid.pixelheight = h;
		extern cvar_t vid_conautoscale;
		Cvar_ForceCallback(&vid_conautoscale);
	}
}

qboolean GLVID_ApplyGammaRamps (unsigned int gammarampsize, unsigned short *ramps)
{
	return false;
}

void GLVID_SetCaption(const const char *caption)
{
	// :(
}

void VID_Register(void)
{
	//many android devices have drivers for both gles1 AND gles2.
	//we default to gles2 because its more capable, but some people might want to try using gles1. so register a renderer for that.
	//the init code explicitly checks for our gles1rendererinfo and tries to create a gles1 context instead.
	extern rendererinfo_t openglrendererinfo;
	gles1rendererinfo = openglrendererinfo;
	gles1rendererinfo.description = "OpenGL ES 1";
	memset(&gles1rendererinfo.name, 0, sizeof(gles1rendererinfo.name));	//make sure there's no 'gl' etc names.
	gles1rendererinfo.name[0] = "gles1";
	R_RegisterRenderer(&gles1rendererinfo);
}


#ifdef VKQUAKE
#include "../vk/vkrenderer.h"
static qboolean VKVID_CreateSurface(void)
{
	VkResult err;
	VkAndroidSurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
	createInfo.flags = 0;
	createInfo.window = ANativeWindow_fromSurface(sys_jenv, sys_jsurface);
	err = vkCreateAndroidSurfaceKHR(vk.instance, &createInfo, NULL, &vk.surface);
	ANativeWindow_release(createInfo.window);
	switch(err)
	{
	default:
		Con_Printf("Unknown vulkan device creation error: %x\n", err);
		return false;
	case VK_SUCCESS:
		break;
	}
	return true;
}

static qboolean VKVID_Init (rendererstate_t *info, unsigned char *palette)
{
	//this is simpler than most platforms, as the window itself is handled by java code, and we can't create/destroy it here
	//(android surfaces can be resized/resampled separately from their window, and are always 'fullscreen' anyway, so this isn't actually an issue for once)
	const char *extnames[] = {VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, NULL};
	if (!sys_jsurface)
	{
		Sys_Printf("VKVID_Init failed: no window known yet\n");
		return false;
	}
#ifdef VK_NO_PROTOTYPES
	dllhandle_t *hInstVulkan = NULL;
	if (!hInstVulkan)
		hInstVulkan = *info->subrenderer?Sys_LoadLibrary(info->subrenderer, NULL):NULL;
	if (!hInstVulkan)
		hInstVulkan = Sys_LoadLibrary("libvulkan.so", NULL);
	if (!hInstVulkan)
	{
		Con_Printf("Unable to load libvulkan.so\nNo Vulkan drivers are installed\n");
		return false;
	}
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) Sys_GetAddressForName(hInstVulkan, "vkGetInstanceProcAddr");
#endif

	return VK_Init(info, extnames, VKVID_CreateSurface, NULL);
}
void VKVID_DeInit(void)
{
	VK_Shutdown();
	//that's all folks.
}
static void VKVID_SwapBuffers(void)
{
	// :(
}

rendererinfo_t vkrendererinfo =
{
	"Vulkan",
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
	VKVID_DeInit,
	VKVID_SwapBuffers,
	GLVID_ApplyGammaRamps,
	NULL,//_CreateCursor,
	NULL,//_SetCursor,
	NULL,//_DestroyCursor,
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
#endif
