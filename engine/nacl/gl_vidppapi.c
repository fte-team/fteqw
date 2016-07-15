#include "quakedef.h"
#include "glquake.h"

#include <ppapi/c/ppb_core.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/gles2/gl2ext_ppapi.h>
#include <ppapi/c/ppb_graphics_3d.h>
#include <ppapi/c/ppb_instance.h>
#include <ppapi/c/pp_errors.h>

extern PPB_Core *ppb_core;
extern PPB_GetInterface sys_gbi;
extern PPB_Graphics3D* graphics3d_interface;
extern PP_Instance pp_instance;
static PP_Resource glcontext;
extern PPB_Instance* instance_interface;
qboolean swappending;

extern cvar_t		vid_vsync;

void FrameEvent(void* user_data, int32_t result);
qboolean NAGL_SwapPending(void)
{
	return swappending;
}
void swap_callback(void* user_data, int32_t result)
{
	if (swappending)
	{
		swappending = false;

		FrameEvent(NULL, 0);
	}
}

void GLVID_SwapBuffers(void)
{
	qboolean vsync = vid_vsync.ival || !*vid_vsync.string;
	struct PP_CompletionCallback ccb = { swap_callback, NULL, vsync?PP_COMPLETIONCALLBACK_FLAG_NONE:PP_COMPLETIONCALLBACK_FLAG_OPTIONAL};
	glFlush();

	switch(graphics3d_interface->SwapBuffers(glcontext, ccb))
	{
	case PP_OK_COMPLETIONPENDING:
		swappending |= vsync;
		break;
	case PP_OK:
		break;
	case PP_ERROR_INPROGRESS:
		Con_DPrintf("chrome still can't handle vid_wait 0. forcing vsync\n");
		vid_vsync.ival = 1;
		break;
	default:
		Con_DPrintf("unknown error on SwapBuffers call\n");
		break;
	}
}

qboolean GLVID_ApplyGammaRamps (unsigned short *ramps)
{
	return false;
}

void *PPAPI_GetGLSymbol(char *symname)
{
	int i;
	static struct {char *name; void *ptr;} funcs[] =
	{
#define f(n) {#n , n},
		f(glActiveTexture)
		f(glAttachShader)
		f(glBindAttribLocation)
		f(glBindBuffer)
		f(glBindFramebuffer)
		f(glBindRenderbuffer)
		f(glBindTexture)
		f(glBlendColor)
		f(glBlendEquation)
		f(glBlendEquationSeparate)
		f(glBlendFunc)
		f(glBlendFuncSeparate)
		f(glBufferData)
		f(glBufferSubData)
		f(glCheckFramebufferStatus)
		f(glClear)
		f(glClearColor)
		f(glClearDepthf)
		f(glClearStencil)
		f(glColorMask)
		f(glCompileShader)
		f(glCompressedTexImage2D)
		f(glCompressedTexSubImage2D)
		f(glCopyTexImage2D)
		f(glCopyTexSubImage2D)
		f(glCreateProgram)
		f(glCreateShader)
		f(glCullFace)
		f(glDeleteBuffers)
		f(glDeleteFramebuffers)
		f(glDeleteProgram)
		f(glDeleteRenderbuffers)
		f(glDeleteShader)
		f(glDeleteTextures)
		f(glDepthFunc)
		f(glDepthMask)
		f(glDepthRangef)
		f(glDetachShader)
		f(glDisable)
		f(glDisableVertexAttribArray)
		f(glDrawArrays)
		f(glDrawElements)
		f(glEnable)
		f(glEnableVertexAttribArray)
		f(glFinish)
		f(glFlush)
		f(glFramebufferRenderbuffer)
		f(glFramebufferTexture2D)
		f(glFrontFace)
		f(glGenBuffers)
		f(glGenerateMipmap)
		f(glGenFramebuffers)
 		f(glGenRenderbuffers)
		f(glGenTextures)
		f(glGetActiveAttrib)
		f(glGetActiveUniform)
		f(glGetAttachedShaders)
		f(glGetAttribLocation)
		f(glGetBooleanv)
		f(glGetBufferParameteriv)
		f(glGetError)
		f(glGetFloatv)
		f(glGetFramebufferAttachmentParameteriv)
		f(glGetIntegerv)
		f(glGetProgramiv)
		f(glGetProgramInfoLog)
		f(glGetRenderbufferParameteriv)
		f(glGetShaderiv)
		f(glGetShaderInfoLog)
		f(glGetShaderPrecisionFormat)
		f(glGetShaderSource)
		f(glGetString)
		f(glGetTexParameterfv)
		f(glGetTexParameteriv)
		f(glGetUniformfv)
		f(glGetUniformiv)
		f(glGetUniformLocation)
		f(glGetVertexAttribfv)
		f(glGetVertexAttribiv)
		f(glGetVertexAttribPointerv)
		f(glHint)
		f(glIsBuffer)
		f(glIsEnabled)
		f(glIsFramebuffer)
		f(glIsProgram)
		f(glIsRenderbuffer)
		f(glIsShader)
		f(glIsTexture)
		f(glLineWidth)
		f(glLinkProgram)
		f(glPixelStorei)
		f(glPolygonOffset)
		f(glReadPixels)
		f(glReleaseShaderCompiler)
		f(glRenderbufferStorage)
		f(glSampleCoverage)
		f(glScissor)
		f(glShaderBinary)
		f(glShaderSource)
		f(glStencilFunc)
		f(glStencilFuncSeparate)
		f(glStencilMask)
		f(glStencilMaskSeparate)
		f(glStencilOp)
		f(glStencilOpSeparate)
		f(glTexImage2D)
		f(glTexParameterf)
		f(glTexParameterfv)
		f(glTexParameteri)
		f(glTexParameteriv)
		f(glTexSubImage2D)
		f(glUniform1f)
		f(glUniform1fv)
		f(glUniform1i)
		f(glUniform1iv)
		f(glUniform2f)
		f(glUniform2fv)
		f(glUniform2i)
		f(glUniform2iv)
		f(glUniform3f)
		f(glUniform3fv)
		f(glUniform3i)
		f(glUniform3iv)
		f(glUniform4f)
		f(glUniform4fv)
		f(glUniform4i)
		f(glUniform4iv)
		f(glUniformMatrix2fv)
		f(glUniformMatrix3fv)
		f(glUniformMatrix4fv)
		f(glUseProgram)
		f(glValidateProgram)
		f(glVertexAttrib1f)
		f(glVertexAttrib1fv)
		f(glVertexAttrib2f)
		f(glVertexAttrib2fv)
		f(glVertexAttrib3f)
		f(glVertexAttrib3fv)
		f(glVertexAttrib4f)
		f(glVertexAttrib4fv)
		f(glVertexAttribPointer)
		f(glViewport)
		{NULL}
	};
	for (i = 0; funcs[i].name; i++)
	{
		if (!strcmp(funcs[i].name, symname))
			return funcs[i].ptr;
	}
	return NULL;
}


void GL_Resized(int width, int height)
{
	extern cvar_t vid_conautoscale, vid_conwidth;

	vid.pixelwidth = width;
	vid.pixelheight = height;
	if (glcontext)
	{
		graphics3d_interface->ResizeBuffers(glcontext, width, height);
		Cvar_ForceCallback(&vid_conautoscale);
		Cvar_ForceCallback(&vid_conwidth);
	}
}

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	if (!vid.pixelwidth)
		vid.pixelwidth = info->width;
	if (!vid.pixelheight)
		vid.pixelheight = info->height;
	if (vid.pixelwidth < 320)
		vid.pixelwidth = 320;
	if (vid.pixelheight < 200)
		vid.pixelheight = 200;

	int32_t attribs[] = {PP_GRAPHICS3DATTRIB_WIDTH, vid.pixelwidth,
						PP_GRAPHICS3DATTRIB_HEIGHT, vid.pixelheight,
						PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
						PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
						PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR, PP_GRAPHICS3DATTRIB_BUFFER_DESTROYED,
						PP_GRAPHICS3DATTRIB_NONE};

	glcontext = graphics3d_interface->Create(pp_instance, 0, attribs);

	glSetCurrentContextPPAPI(glcontext);

	if (!instance_interface->BindGraphics(pp_instance, glcontext))
	{
		Con_Printf("failed to bind context\n");
		return false;
	}

	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	GLVID_SwapBuffers();

//	vid.pixelwidth = info->width;
//	vid.pixelheight = info->height;

	GL_Init(PPAPI_GetGLSymbol);

	return true;
}

void	GLVID_Shutdown (void)
{
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	GLVID_SwapBuffers();

	ppb_core->ReleaseResource(glcontext);
//	glTerminatePPAPI();
}
void GLVID_DeInit (void)
{
	GLVID_Shutdown();
}

void GLVID_Crashed(void);

void	GLVID_Update (vrect_t *rects);

int GLVID_SetMode (rendererstate_t *info, unsigned char *palette);

void GLVID_SetCaption(const char *caption)
{
}