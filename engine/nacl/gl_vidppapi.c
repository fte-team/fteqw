#include "quakedef.h"
#include "glquake.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/gles2/gl2ext_ppapi.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_instance.h"

extern PPB_GetInterface sys_gbi;
extern PPB_Graphics3D* graphics3d_interface;
extern PP_Instance pp_instance;
static PP_Resource glcontext;
extern PPB_Instance* instance_interface;
int delayedswap = false;

void swap_callback(void* user_data, int32_t result)
{
//  printf("swap result: %d\n", result);
}
void GL_BeginRendering (void)
{
}
void GL_EndRendering (void)
{
	delayedswap = true;
	glFlush();
}

void GL_DoSwap(void)
{
	if (delayedswap)
	{
		struct PP_CompletionCallback ccb = { swap_callback, NULL, PP_COMPLETIONCALLBACK_FLAG_OPTIONAL};
		graphics3d_interface->SwapBuffers(glcontext, ccb);
		delayedswap = false;
	}
}

void	GLVID_SetPalette (unsigned char *palette)
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

void	GLVID_ShiftPalette (unsigned char *palette)
{
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

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
	int32_t attribs[] = {PP_GRAPHICS3DATTRIB_WIDTH, info->width,
						PP_GRAPHICS3DATTRIB_HEIGHT, info->height,
						PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
						PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
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
	GL_EndRendering();
	GL_DoSwap();

	vid.pixelwidth = info->width;
	vid.pixelheight = info->height;

    GLVID_SetPalette (palette);
	GL_Init(PPAPI_GetGLSymbol);
	vid.recalc_refdef = 1;

	return true;
}

void	GLVID_Shutdown (void)
{
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	GL_EndRendering();
	GL_DoSwap();

	glTerminatePPAPI();
}
void GLVID_DeInit (void)
{
	GLVID_Shutdown();
}

void GLVID_Crashed(void);

void	GLVID_Update (vrect_t *rects);

int GLVID_SetMode (rendererstate_t *info, unsigned char *palette);

void GLVID_SetCaption(char *caption)
{
}