#include "quakedef.h"
#ifdef GLQUAKE
#include "glquake.h"
#include "gl_draw.h"
#include "shader.h"

//standard 1.1 opengl calls
void (APIENTRY *qglAlphaFunc) (GLenum func, GLclampf ref);
void (APIENTRY *qglBegin) (GLenum mode);
void (APIENTRY *qglBlendFunc) (GLenum sfactor, GLenum dfactor);
void (APIENTRY *qglCallList) (GLuint list);
void (APIENTRY *qglClear) (GLbitfield mask);
void (APIENTRY *qglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void (APIENTRY *qglClearDepth) (GLclampd depth);
void (APIENTRY *qglClearStencil) (GLint s);
void (APIENTRY *qglClipPlane) (GLenum plane, const GLdouble *equation);
void (APIENTRY *qglColor3f) (GLfloat red, GLfloat green, GLfloat blue);
void (APIENTRY *qglColor3ub) (GLubyte red, GLubyte green, GLubyte blue);
void (APIENTRY *qglColor4f) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void (APIENTRY *qglColor4fv) (const GLfloat *v);
void (APIENTRY *qglColor4ub) (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void (APIENTRY *qglColor4ubv) (const GLubyte *v);
void (APIENTRY *qglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void (APIENTRY *qglCopyTexImage2D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void (APIENTRY *qglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void (APIENTRY *qglCullFace) (GLenum mode);
void (APIENTRY *qglDepthFunc) (GLenum func);
void (APIENTRY *qglDepthMask) (GLboolean flag);
void (APIENTRY *qglDepthRange) (GLclampd zNear, GLclampd zFar);
void (APIENTRY *qglDisable) (GLenum cap);
void (APIENTRY *qglDrawBuffer) (GLenum mode);
void (APIENTRY *qglDrawPixels) (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void (APIENTRY *qglEnable) (GLenum cap);
void (APIENTRY *qglEnd) (void);
void (APIENTRY *qglEndList) (void);
void (APIENTRY *qglFinish) (void);
void (APIENTRY *qglFlush) (void);
void (APIENTRY *qglFrustum) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLuint (APIENTRY *qglGenLists) (GLsizei range);
void (APIENTRY *qglGenTextures) (GLsizei n, GLuint *textures);
GLenum (APIENTRY *qglGetError) (void);
void (APIENTRY *qglGetFloatv) (GLenum pname, GLfloat *params);
void (APIENTRY *qglGetIntegerv) (GLenum pname, GLint *params);
const GLubyte * (APIENTRY *qglGetString) (GLenum name);
void (APIENTRY *qglHint) (GLenum target, GLenum mode);
void (APIENTRY *qglLoadIdentity) (void);
void (APIENTRY *qglLoadMatrixf) (const GLfloat *m);
void (APIENTRY *qglNormal3f) (GLfloat nx, GLfloat ny, GLfloat nz);
void (APIENTRY *qglNormal3fv) (const GLfloat *v);
void (APIENTRY *qglMatrixMode) (GLenum mode);
void (APIENTRY *qglMultMatrixf) (const GLfloat *m);
void (APIENTRY *qglNewList) (GLuint list, GLenum mode);
void (APIENTRY *qglOrtho) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void (APIENTRY *qglPolygonMode) (GLenum face, GLenum mode);
void (APIENTRY *qglPolygonOffset) (GLfloat factor, GLfloat units);
void (APIENTRY *qglPopMatrix) (void);
void (APIENTRY *qglPushMatrix) (void);
void (APIENTRY *qglReadBuffer) (GLenum mode);
void (APIENTRY *qglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
void (APIENTRY *qglRotatef) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void (APIENTRY *qglScalef) (GLfloat x, GLfloat y, GLfloat z);
void (APIENTRY *qglShadeModel) (GLenum mode);
void (APIENTRY *qglTexCoord1f) (GLfloat s);
void (APIENTRY *qglTexCoord2f) (GLfloat s, GLfloat t);
void (APIENTRY *qglTexCoord2fv) (const GLfloat *v);
void (APIENTRY *qglTexEnvf) (GLenum target, GLenum pname, GLfloat param);
void (APIENTRY *qglTexEnvfv) (GLenum target, GLenum pname, const GLfloat *param);
void (APIENTRY *qglTexEnvi) (GLenum target, GLenum pname, GLint param);
void (APIENTRY *qglTexGeni) (GLenum coord, GLenum pname, GLint param);
void (APIENTRY *qglTexGenfv) (GLenum coord, GLenum pname, const GLfloat *param);
void (APIENTRY *qglTexImage2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (APIENTRY *qglTexImage3D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (APIENTRY *qglTexParameteri) (GLenum target, GLenum pname, GLint param);
void (APIENTRY *qglTexParameterf) (GLenum target, GLenum pname, GLfloat param);
void (APIENTRY *qglTexParameteriv) (GLenum target, GLenum pname, const GLint *params);
void (APIENTRY *qglTexParameterfv) (GLenum target, GLenum pname, const GLfloat *params);
void (APIENTRY *qglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void (APIENTRY *qglTranslatef) (GLfloat x, GLfloat y, GLfloat z);
void (APIENTRY *qglVertex2f) (GLfloat x, GLfloat y);
void (APIENTRY *qglVertex3f) (GLfloat x, GLfloat y, GLfloat z);
void (APIENTRY *qglVertex3fv) (const GLfloat *v);
void (APIENTRY *qglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);
void (APIENTRY *qglGetTexLevelParameteriv) (GLenum target, GLint level, GLenum pname, GLint *params);

void (APIENTRY *qglDrawRangeElements) (GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *);
void (APIENTRY *qglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void (APIENTRY *qglArrayElement) (GLint i);
void (APIENTRY *qglVertexPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void (APIENTRY *qglNormalPointer) (GLenum type, GLsizei stride, const GLvoid *pointer);
void (APIENTRY *qglTexCoordPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void (APIENTRY *qglColorPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void (APIENTRY *qglDrawArrays) (GLenum mode, GLint first, GLsizei count);
void (APIENTRY *qglDisableClientState) (GLenum array);
void (APIENTRY *qglEnableClientState) (GLenum array);

void (APIENTRY *qglScissor) (GLint x, GLint y, GLsizei width, GLsizei height);
void (APIENTRY *qglStencilOp) (GLenum fail, GLenum zfail, GLenum zpass);
void (APIENTRY *qglStencilFunc) (GLenum func, GLint ref, GLuint mask);
void (APIENTRY *qglPushAttrib) (GLbitfield mask);
void (APIENTRY *qglPopAttrib) (void);

void (APIENTRY *qglFogf) (GLenum pname, GLfloat param);
void (APIENTRY *qglFogi) (GLenum pname, GLint param);
void (APIENTRY *qglFogfv) (GLenum pname, const GLfloat *params);

void (APIENTRY *qglDeleteTextures) (GLsizei n, const GLuint *textures);

void (APIENTRY *qglGenBuffersARB)(GLsizei n, GLuint* ids);
void (APIENTRY *qglDeleteBuffersARB)(GLsizei n, GLuint* ids);
void (APIENTRY *qglBindBufferARB)(GLenum target, GLuint id);
void (APIENTRY *qglBufferDataARB)(GLenum target, GLsizei size, const void* data, GLenum usage);
void (APIENTRY *qglBufferSubDataARB)(GLenum target, GLint offset, GLsizei size, void* data);
void *(APIENTRY *qglMapBufferARB)(GLenum target, GLenum access);
GLboolean (APIENTRY *qglUnmapBufferARB)(GLenum target);

const GLubyte * (APIENTRY * qglGetStringi) (GLenum name, GLuint index);

void (APIENTRY *qglGenFramebuffersEXT)(GLsizei n, GLuint* ids);
void (APIENTRY *qglDeleteFramebuffersEXT)(GLsizei n, const GLuint* ids);
void (APIENTRY *qglBindFramebufferEXT)(GLenum target, GLuint id);
void (APIENTRY *qglGenRenderbuffersEXT)(GLsizei n, GLuint* ids);
void (APIENTRY *qglDeleteRenderbuffersEXT)(GLsizei n, const GLuint* ids);
void (APIENTRY *qglBindRenderbufferEXT)(GLenum target, GLuint id);
void (APIENTRY *qglRenderbufferStorageEXT)(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height);
void (APIENTRY *qglFramebufferTexture2DEXT)(GLenum target, GLenum attachmentPoint, GLenum textureTarget, GLuint textureId, GLint  level);
void (APIENTRY *qglFramebufferRenderbufferEXT)(GLenum target, GLenum attachmentPoint, GLenum textureTarget, GLuint textureId);
GLenum (APIENTRY *qglCheckFramebufferStatusEXT)(GLenum target);

/*
PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB;
PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB;
PFNGLBINDPROGRAMARBPROC qglBindProgramARB;
PFNGLGENPROGRAMSARBPROC qglGenProgramsARB;
*/
FTEPFNGLLOCKARRAYSEXTPROC qglLockArraysEXT;
FTEPFNGLUNLOCKARRAYSEXTPROC qglUnlockArraysEXT;

/*glslang - arb_shader_objects
gl core uses different names/distinctions from the extension
*/
FTEPFNGLCREATEPROGRAMOBJECTARBPROC  qglCreateProgramObjectARB;
FTEPFNGLDELETEOBJECTARBPROC         qglDeleteProgramObject_;
FTEPFNGLDELETEOBJECTARBPROC         qglDeleteShaderObject_;
FTEPFNGLUSEPROGRAMOBJECTARBPROC     qglUseProgramObjectARB;
FTEPFNGLCREATESHADEROBJECTARBPROC   qglCreateShaderObjectARB;
FTEPFNGLSHADERSOURCEARBPROC         qglShaderSourceARB;
FTEPFNGLCOMPILESHADERARBPROC        qglCompileShaderARB;
FTEPFNGLGETOBJECTPARAMETERIVARBPROC qglGetShaderParameteriv_;
FTEPFNGLGETOBJECTPARAMETERIVARBPROC qglGetProgramParameteriv_;
FTEPFNGLATTACHOBJECTARBPROC         qglAttachObjectARB;
FTEPFNGLGETINFOLOGARBPROC           qglGetShaderInfoLog_;
FTEPFNGLGETINFOLOGARBPROC           qglGetProgramInfoLog_;
FTEPFNGLLINKPROGRAMARBPROC          qglLinkProgramARB;
FTEPFNGLBINDATTRIBLOCATIONARBPROC   qglBindAttribLocationARB;
FTEPFNGLGETATTRIBLOCATIONARBPROC	qglGetAttribLocationARB;
FTEPFNGLVERTEXATTRIBPOINTER			qglVertexAttribPointer;
FTEPFNGLGETVERTEXATTRIBIV			qglGetVertexAttribiv;
FTEPFNGLENABLEVERTEXATTRIBARRAY		qglEnableVertexAttribArray;
FTEPFNGLDISABLEVERTEXATTRIBARRAY	qglDisableVertexAttribArray;
FTEPFNGLGETUNIFORMLOCATIONARBPROC   qglGetUniformLocationARB;
FTEPFNGLUNIFORMMATRIXPROC			qglUniformMatrix4fvARB;
FTEPFNGLUNIFORMMATRIXPROC			qglUniformMatrix3x4fv;
FTEPFNGLUNIFORMMATRIXPROC			qglUniformMatrix4x3fv;
FTEPFNGLUNIFORM4FARBPROC            qglUniform4fARB;
FTEPFNGLUNIFORM4FVARBPROC           qglUniform4fvARB;
FTEPFNGLUNIFORM3FARBPROC            qglUniform3fARB;
FTEPFNGLUNIFORM3FVARBPROC           qglUniform3fvARB;
FTEPFNGLUNIFORM1IARBPROC            qglUniform1iARB;
FTEPFNGLUNIFORM1FARBPROC            qglUniform1fARB;

//extensions
//arb multitexture
qlpSelTexFUNC qglActiveTextureARB;
qlpSelTexFUNC qglClientActiveTextureARB;
qlpMTex3FUNC	qglMultiTexCoord3fARB;
qlpMTex2FUNC	qglMultiTexCoord2fARB;

//generic multitexture
lpMTexFUNC qglMTexCoord2fSGIS;
lpSelTexFUNC qglSelectTextureSGIS;
int mtexid0;

//ati_truform
FTEPFNGLPNTRIANGLESIATIPROC qglPNTrianglesiATI;
FTEPFNGLPNTRIANGLESFATIPROC qglPNTrianglesfATI;

//stencil shadowing
void (APIENTRY *qglStencilOpSeparateATI) (GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
FTEPFNGLACTIVESTENCILFACEEXTPROC qglActiveStencilFaceEXT;

//quick hack that made quake work on both 1 and 1.1 gl implementations.
BINDTEXFUNCPTR bindTexFunc;

#define GLchar char
#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif
#if defined(DEBUG)
typedef void (APIENTRY *GLDEBUGPROCARB)(GLenum source,
					GLenum type,
					GLuint id,
					GLenum severity,
					GLsizei length,
					const GLchar* message,
					GLvoid* userParam);
void (APIENTRY *qglDebugMessageControlARB)(enum source,
					enum type,
					enum severity,
					GLsizei count,
					const GLuint* ids,
					GLboolean enabled);
void (APIENTRY *qglDebugMessageInsertARB)(enum source,
					enum type,
					GLuint id,
					enum severity,
					GLsizei length, 
					const char* buf);
void (APIENTRY *qglDebugMessageCallbackARB)(GLDEBUGPROCARB callback,
					void* userParam);
GLuint (APIENTRY *qglGetDebugMessageLogARB)(GLuint count,
					GLsizei bufsize,
					GLenum*	sources,
					GLenum* types,
					GLuint* ids,
					GLuint* severities,
					GLsizei* lengths,
					char* messageLog);

#define GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB               0x8242
#define GL_MAX_DEBUG_MESSAGE_LENGTH_ARB               0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES_ARB              0x9144
#define GL_DEBUG_LOGGED_MESSAGES_ARB                  0x9145
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_ARB       0x8243
#define GL_DEBUG_CALLBACK_FUNCTION_ARB                0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM_ARB              0x8245
#define GL_DEBUG_SOURCE_API_ARB                       0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB             0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER_ARB           0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY_ARB               0x8249
#define GL_DEBUG_SOURCE_APPLICATION_ARB               0x824A
#define GL_DEBUG_SOURCE_OTHER_ARB                     0x824B
#define GL_DEBUG_TYPE_ERROR_ARB                       0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB         0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB          0x824E
#define GL_DEBUG_TYPE_PORTABILITY_ARB                 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE_ARB                 0x8250
#define GL_DEBUG_TYPE_OTHER_ARB                       0x8251
#define GL_DEBUG_SEVERITY_HIGH_ARB                    0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_ARB                  0x9147
#define GL_DEBUG_SEVERITY_LOW_ARB                     0x9148 


void (APIENTRY myGLDEBUGPROCAMD)(GLenum source,
					GLenum type,
					GLuint id,
					GLenum severity,
					GLsizei length,
					const GLchar* message,
					GLvoid* userParam)
{
#ifndef _WIN32
#define OutputDebugString(s) puts(s)
#endif
	switch(type)
	{
	case GL_DEBUG_TYPE_ERROR_ARB:
		OutputDebugString("Error: ");
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
		OutputDebugString("Depricated: ");
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
		OutputDebugString("Undefined: ");
		break;
	case GL_DEBUG_TYPE_PORTABILITY_ARB:
		OutputDebugString("Portability: ");
		break;
	case GL_DEBUG_TYPE_PERFORMANCE_ARB:
		OutputDebugString("Performance: ");
		break;
	default:
	case GL_DEBUG_TYPE_OTHER_ARB:
		OutputDebugString("Other: ");
		break;
	}
	OutputDebugString(message);
	OutputDebugString("\n");
}
#endif


int gl_mtexarbable=0;	//max texture units
qboolean gl_mtexable = false;


qboolean gammaworks;	//if the gl drivers can set proper gamma.


gl_config_t gl_config;
int		gl_stencilbits;

float		gldepthmin, gldepthmax;
const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
static const char *gl_extensions;

static unsigned int gl_num_extensions;

extern cvar_t gl_workaround_ati_shadersource;


qboolean GL_CheckExtension(char *extname)
{
	int i;
	cvar_t *v = Cvar_Get(va("gl_ext_%s", extname), "1", 0, "GL Extensions");
	if (v && !v->ival)
		return false;

	if (gl_num_extensions && qglGetStringi)
	{
		for (i = 0; i < gl_num_extensions; i++)
			if (!strcmp(qglGetStringi(GL_EXTENSIONS, i), extname))
				return true;
	}

	if (!gl_extensions)
		return false;

	//note that this is not actually correct...
	return !!strstr(gl_extensions, extname);
}

void APIENTRY GL_DrawRangeElementsEmul(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
	qglDrawElements(mode, count, type, indices);
}
void APIENTRY GL_BindBufferARBStub(GLenum target, GLuint id)
{
}

void APIENTRY GL_ClientStateStub(GLenum array)
{
}

#define getglcore getglfunction
#define getglext(name) getglfunction(name)
void GL_CheckExtensions (void *(*getglfunction) (char *name), float ver)
{
	memset(&gl_config, 0, sizeof(gl_config));

	gl_config.glversion = ver;

	if (!strncmp(gl_version, "OpenGL ES", 9))
		gl_config.gles = true;
	else
		gl_config.gles = false;

	if (gl_config.gles)
		gl_config.nofixedfunc = gl_config.glversion >= 2;
	else
	{
		/*in gl3.0 things are depricated but not removed*/
		/*in gl3.1 depricated things are removed unless compatibility is present*/
		/*in gl3.2 there's a profile flag we can query*/
		if (gl_config.glversion >= 3.2)
		{
			GLint profile = 0;
#define GL_CONTEXT_PROFILE_MASK					0x9126
#define GL_CONTEXT_CORE_PROFILE_BIT				0x00000001
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT	0x00000002
			qglGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);

			if (!profile)
			{
				Con_DPrintf("Driver reports invalid profile, assuming compatibility support\n");
				gl_config.nofixedfunc = false;
			}
			else
				gl_config.nofixedfunc = !(profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
		}
		else if (gl_config.glversion == 3.1)
			gl_config.nofixedfunc = !GL_CheckExtension("GL_ARB_compatibility");
		else
			gl_config.nofixedfunc = false;
	}

	//multitexture
	gl_mtexable = false;
	gl_mtexarbable = 0;
	qglActiveTextureARB = NULL;
	qglMultiTexCoord2fARB = NULL;
	qglMultiTexCoord3fARB = NULL;
	qglMTexCoord2fSGIS = NULL;
	qglSelectTextureSGIS = NULL;
	mtexid0 = 0;

	//no GL_ATI_separate_stencil
	qglStencilOpSeparateATI = NULL;

	//no GL_EXT_stencil_two_side
	qglActiveStencilFaceEXT = NULL;

	//no truform. sorry.
	qglPNTrianglesfATI = NULL;
	qglPNTrianglesiATI = NULL;

	//fragment programs
/*	gl_config.arb_fragment_program = false;
	qglProgramStringARB = NULL;
	qglGetProgramivARB = NULL;
	qglBindProgramARB = NULL;
	qglGenProgramsARB = NULL;
*/

	qglGenFramebuffersEXT		= NULL;
	qglDeleteFramebuffersEXT	= NULL;
	qglBindFramebufferEXT		= NULL;
	qglGenRenderbuffersEXT		= NULL;
	qglDeleteRenderbuffersEXT	= NULL;
	qglBindRenderbufferEXT		= NULL;
	qglRenderbufferStorageEXT	= NULL;
	qglFramebufferTexture2DEXT	= NULL;

	gl_config.arb_texture_non_power_of_two = false;
	gl_config.sgis_generate_mipmap = false;

	gl_config.tex_env_combine = false;
	gl_config.env_add = false;
	gl_config.nv_tex_env_combine4 = false;

	gl_config.arb_texture_env_combine = false;
	gl_config.arb_texture_env_dot3 = false;
	gl_config.arb_texture_cube_map = false;

	gl_config.arb_shader_objects = false;
	gl_config.ext_framebuffer_objects = false;

	gl_config.ext_texture_filter_anisotropic = 0;

	if (GL_CheckExtension("GL_EXT_texture_filter_anisotropic"))
	{
		qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.ext_texture_filter_anisotropic);

		Con_DPrintf("Anisotropic filter extension found (%dx max).\n",gl_config.ext_texture_filter_anisotropic);
	}

	if (GL_CheckExtension("GL_ARB_texture_non_power_of_two"))
		gl_config.arb_texture_non_power_of_two = true;
//	if (GL_CheckExtension("GL_SGIS_generate_mipmap"))	//a suprising number of implementations have this broken.
//		gl_config.sgis_generate_mipmap = true;

	if (gl_config.gles)
	{
		qglActiveTextureARB = (void *) getglext("glActiveTexture");
		qglClientActiveTextureARB = (void *) getglext("glClientActiveTexture");
		qglSelectTextureSGIS = qglActiveTextureARB;
		mtexid0 = GL_TEXTURE0_ARB;
	}
	else if (GL_CheckExtension("GL_ARB_multitexture") && !COM_CheckParm("-noamtex"))
	{	//ARB multitexture is the popular choice.
		qglActiveTextureARB = (void *) getglext("glActiveTextureARB");
		qglClientActiveTextureARB = (void *) getglext("glClientActiveTextureARB");
		qglMultiTexCoord2fARB = (void *) getglext("glMultiTexCoord2fARB");
		qglMultiTexCoord3fARB = (void *) getglext("glMultiTexCoord3fARB");

		qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_mtexarbable);
		gl_mtexable = true;

		qglMTexCoord2fSGIS = qglMultiTexCoord2fARB;
		qglSelectTextureSGIS = qglActiveTextureARB;

		mtexid0 = GL_TEXTURE0_ARB;

		if (!qglActiveTextureARB || !qglClientActiveTextureARB || !qglMultiTexCoord2fARB)
		{
			qglActiveTextureARB = NULL;
			qglClientActiveTextureARB = NULL;
			qglMultiTexCoord2fARB = NULL;
			qglMTexCoord2fSGIS = NULL;
			qglSelectTextureSGIS = NULL;
			gl_mtexable=false;
			gl_mtexarbable = false;
		}
		else
		{
			Con_DPrintf("ARB Multitexture extensions found. Use -noamtex to disable.\n");
		}

	}
	else if (GL_CheckExtension("GL_SGIS_multitexture") && !COM_CheckParm("-nomtex"))
	{	//SGIS multitexture, limited in many ways but basic functionality is identical to ARB
		Con_SafePrintf("Multitexture extensions found.\n");
		qglMTexCoord2fSGIS = (void *) getglext("glMTexCoord2fSGIS");
		qglSelectTextureSGIS = (void *) getglext("glSelectTextureSGIS");
		gl_mtexable = true;

		mtexid0 = GL_TEXTURE0_SGIS;
	}

	if (GL_CheckExtension("GL_EXT_stencil_wrap"))
		gl_config.ext_stencil_wrap = true;

	if (GL_CheckExtension("GL_ATI_separate_stencil"))
		qglStencilOpSeparateATI = (void *) getglext("glStencilOpSeparateATI");
	if (GL_CheckExtension("GL_EXT_stencil_two_side"))
		qglActiveStencilFaceEXT = (void *) getglext("glActiveStencilFaceEXT");

	/*not enabled - its only useful for shadow volumes, but (on nvidia) it affects the depth values even when not clamped which results in shadow z-fighting. best rely upon infinite projection matricies instead*/
//	if (GL_CheckExtension("GL_ARB_depth_clamp") || GL_CheckExtension("GL_NV_depth_clamp"))
//		gl_config.arb_depth_clamp = true;

	if (GL_CheckExtension("GL_ARB_texture_compression"))
	{
		qglCompressedTexImage2DARB = (void *)getglext("glCompressedTexImage2DARB");
		qglGetCompressedTexImageARB = (void *)getglext("glGetCompressedTexImageARB");

		if (!qglCompressedTexImage2DARB || !qglGetCompressedTexImageARB)
		{
			qglCompressedTexImage2DARB = NULL;
			qglGetCompressedTexImageARB = NULL;
		}
		else
			gl_config.arb_texture_compression = true;
	}

	if (GL_CheckExtension("GL_EXT_depth_bounds_test"))
		qglDepthBoundsEXT = (void *)getglext("glDepthBoundsEXT");
	else if (GL_CheckExtension("GL_NV_depth_bounds_test"))
		qglDepthBoundsEXT = (void *)getglext("glDepthBoundsNV");
	else
		qglDepthBoundsEXT = NULL;

	if (GL_CheckExtension("GL_ATI_pn_triangles"))
	{
		qglPNTrianglesfATI = (void *)getglext("glPNTrianglesfATI");
		qglPNTrianglesiATI = (void *)getglext("glPNTrianglesiATI");
	}

	if (GL_CheckExtension("GL_EXT_texture_object"))
	{
		bindTexFunc			= (void *)getglext("glBindTextureEXT");
		if (!bindTexFunc)	//grrr
			bindTexFunc			= (void *)getglext("glBindTexture");
	}

	if (GL_CheckExtension("GL_EXT_compiled_vertex_array"))
	{
		qglLockArraysEXT = (void *)getglext("glLockArraysEXT");
		qglUnlockArraysEXT = (void *)getglext("glUnlockArraysEXT");
	}

	/*various combiner features*/
	gl_config.tex_env_combine = GL_CheckExtension("GL_EXT_texture_env_combine");
	gl_config.env_add = GL_CheckExtension("GL_EXT_texture_env_add");
	gl_config.nv_tex_env_combine4 = GL_CheckExtension("GL_NV_texture_env_combine4");
	gl_config.arb_texture_env_combine = GL_CheckExtension("GL_ARB_texture_env_combine");
	gl_config.arb_texture_env_dot3 = GL_CheckExtension("GL_ARB_texture_env_dot3");

	gl_config.arb_texture_cube_map = GL_CheckExtension("GL_ARB_texture_cube_map");

	/*vbos*/
	if (GL_CheckExtension("GL_ARB_vertex_buffer_object"))
	{
		qglGenBuffersARB = (void *)getglext("glGenBuffersARB");
		qglDeleteBuffersARB = (void *)getglext("glDeleteBuffersARB");
		qglBindBufferARB = (void *)getglext("glBindBufferARB");
		qglBufferDataARB = (void *)getglext("glBufferDataARB");
		qglBufferSubDataARB = (void *)getglext("glBufferSubDataARB");
		qglMapBufferARB = (void *)getglext("glMapBufferARB");
		qglUnmapBufferARB = (void *)getglext("glUnmapBufferARB");
	}

	// glslang
	//the gf2 to gf4 cards emulate vertex_shader and thus supports shader_objects.
	//but our code kinda requires both for clean workings.
	if (strstr(gl_renderer, " Mesa ") && Cvar_Get("gl_blacklist_mesa_glsl", "1", CVAR_RENDERERLATCH, "gl blacklists")->ival && (gl_config.glversion < 3 || gl_config.gles))
	{
//(9:12:33 PM) bigfoot: Spike, can you please blacklist your menu shader on Mesa? My machine just hard locked up again because I forgot that pressing escape in FTE is verboten
//(11:51:42 PM) bigfoot: OpenGL vendor string: Tungsten Graphics, Inc
//(11:51:50 PM) bigfoot: OpenGL version string: 2.1 Mesa 7.7.1

//blacklist all glsl, it can't handle #define macros properly either.
//if the menu shader is hardlocking, I don't know what else will do it too.
		Con_Printf(CON_NOTICE "Mesa detected, ignoring any GLSL support. Use '+set gl_blacklist_mesa_glsl 0' on the commandline to reenable it.\n");
	}
	else if (gl_config.glversion >= 2)// && (gl_config.gles || 0))
	{
		/*core names are different from extension names (more functions too)*/
		gl_config.arb_shader_objects = true;
		qglCreateProgramObjectARB	= (void *)getglext( "glCreateProgram");
		qglDeleteProgramObject_		= (void *)getglext( "glDeleteProgram");
		qglDeleteShaderObject_		= (void *)getglext( "glDeleteShader");
		qglUseProgramObjectARB		= (void *)getglext( "glUseProgram");
		qglCreateShaderObjectARB	= (void *)getglext( "glCreateShader");
		qglGetProgramParameteriv_	= (void *)getglext( "glGetProgramiv");
		qglGetShaderParameteriv_	= (void *)getglext( "glGetShaderiv");
		qglAttachObjectARB			= (void *)getglext( "glAttachShader");
		qglGetProgramInfoLog_		= (void *)getglext( "glGetProgramInfoLog");
		qglGetShaderInfoLog_		= (void *)getglext( "glGetShaderInfoLog");
		qglShaderSourceARB			= (void *)getglext("glShaderSource");
		qglCompileShaderARB			= (void *)getglext("glCompileShader");
		qglLinkProgramARB			= (void *)getglext("glLinkProgram");
		qglBindAttribLocationARB	= (void *)getglext("glBindAttribLocation");
		qglGetAttribLocationARB		= (void *)getglext("glGetAttribLocation");
		qglVertexAttribPointer		= (void *)getglext("glVertexAttribPointer");
		qglGetVertexAttribiv		= (void *)getglext("glGetVertexAttribiv");
		qglEnableVertexAttribArray	= (void *)getglext("glEnableVertexAttribArray");
		qglDisableVertexAttribArray	= (void *)getglext("glDisableVertexAttribArray");
		qglGetUniformLocationARB	= (void *)getglext("glGetUniformLocation");
		qglUniformMatrix4fvARB		= (void *)getglext("glUniformMatrix4fv");
		qglUniformMatrix3x4fv		= (void *)getglext("glUniformMatrix3x4fv");
		qglUniformMatrix4x3fv		= (void *)getglext("glUniformMatrix4x3fv");
		qglUniform4fARB				= (void *)getglext("glUniform4f");
		qglUniform4fvARB			= (void *)getglext("glUniform4fv");
		qglUniform3fARB				= (void *)getglext("glUniform3f");
		qglUniform3fvARB			= (void *)getglext("glUniform3fv");
		qglUniform1iARB				= (void *)getglext("glUniform1i");
		qglUniform1fARB				= (void *)getglext("glUniform1f");
		Con_DPrintf("GLSL available\n");
	}
	else if (GL_CheckExtension("GL_ARB_fragment_shader")
		&& GL_CheckExtension("GL_ARB_vertex_shader")
		&& GL_CheckExtension("GL_ARB_shader_objects"))
	{
		gl_config.arb_shader_objects = true;
		qglCreateProgramObjectARB	= (void *)getglext("glCreateProgramObjectARB");
		qglDeleteProgramObject_		= (void *)getglext("glDeleteObjectARB");
		qglDeleteShaderObject_		= (void *)getglext("glDeleteObjectARB");
		qglUseProgramObjectARB		= (void *)getglext("glUseProgramObjectARB");
		qglCreateShaderObjectARB	= (void *)getglext("glCreateShaderObjectARB");
		qglShaderSourceARB			= (void *)getglext("glShaderSourceARB");
		qglCompileShaderARB			= (void *)getglext("glCompileShaderARB");
		qglGetProgramParameteriv_	= (void *)getglext("glGetObjectParameterivARB");
		qglGetShaderParameteriv_	= (void *)getglext("glGetObjectParameterivARB");
		qglAttachObjectARB			= (void *)getglext("glAttachObjectARB");
		qglGetProgramInfoLog_		= (void *)getglext("glGetInfoLogARB");
		qglGetShaderInfoLog_		= (void *)getglext("glGetInfoLogARB");
		qglLinkProgramARB			= (void *)getglext("glLinkProgramARB");
		qglBindAttribLocationARB	= (void *)getglext("glBindAttribLocationARB");
		qglGetAttribLocationARB		= (void *)getglext("glGetAttribLocationARB");
		qglVertexAttribPointer		= (void *)getglext("glVertexAttribPointerARB");
		qglGetVertexAttribiv		= (void *)getglext("glGetVertexAttribivARB");
		qglEnableVertexAttribArray	= (void *)getglext("glEnableVertexAttribArrayARB");
		qglDisableVertexAttribArray	= (void *)getglext("glDisableVertexAttribArrayARB");
		qglGetUniformLocationARB	= (void *)getglext("glGetUniformLocationARB");
		qglUniformMatrix4fvARB		= (void *)getglext("glUniformMatrix4fvARB");
		qglUniformMatrix3x4fv		= (void *)getglext("glUniformMatrix3x4fvARB");
		qglUniformMatrix4x3fv		= (void *)getglext("glUniformMatrix4x3fvARB");
		qglUniform4fARB				= (void *)getglext("glUniform4fARB");
		qglUniform4fvARB			= (void *)getglext("glUniform4fvARB");
		qglUniform3fARB				= (void *)getglext("glUniform3fARB");
		qglUniform3fvARB			= (void *)getglext("glUniform3fvARB");
		qglUniform1iARB				= (void *)getglext("glUniform1iARB");
		qglUniform1fARB				= (void *)getglext("glUniform1fARB");

		Con_DPrintf("GLSL available\n");
	}

	if (GL_CheckExtension("GL_EXT_framebuffer_object"))
	{
		gl_config.ext_framebuffer_objects = true;
		qglGenFramebuffersEXT			= (void *)getglext("glGenFramebuffersEXT");
		qglDeleteFramebuffersEXT		= (void *)getglext("glDeleteFramebuffersEXT");
		qglBindFramebufferEXT			= (void *)getglext("glBindFramebufferEXT");
		qglGenRenderbuffersEXT			= (void *)getglext("glGenRenderbuffersEXT");
		qglDeleteRenderbuffersEXT		= (void *)getglext("glDeleteRenderbuffersEXT");
		qglBindRenderbufferEXT			= (void *)getglext("glBindRenderbufferEXT");
		qglRenderbufferStorageEXT		= (void *)getglext("glRenderbufferStorageEXT");
		qglFramebufferTexture2DEXT		= (void *)getglext("glFramebufferTexture2DEXT");
		qglFramebufferRenderbufferEXT	= (void *)getglext("glFramebufferRenderbufferEXT");
		qglCheckFramebufferStatusEXT	= (void *)getglext("glCheckFramebufferStatusEXT");
	}

#ifdef DEBUG
	if (GL_CheckExtension("GL_ARB_debug_output"))
	{
		qglDebugMessageControlARB	= (void *)getglext("glDebugMessageControlARB");
		qglDebugMessageInsertARB	= (void *)getglext("glDebugMessageInsertARB");
		qglDebugMessageCallbackARB	= (void *)getglext("glDebugMessageCallbackARB");
		qglGetDebugMessageLogARB	= (void *)getglext("glGetDebugMessageLogARB");
	}
	else
	{
		qglDebugMessageControlARB = NULL;
		qglDebugMessageInsertARB = NULL;
		qglDebugMessageCallbackARB = NULL;
		qglGetDebugMessageLogARB = NULL;
	}
#endif
}

static const char *glsl_hdrs[] =
{
	"sys/skeletal.h",
			"attribute vec3 v_normal;\n"
			"attribute vec3 v_svector;\n"
			"attribute vec3 v_tvector;\n"
			"#ifdef SKELETAL\n"
				"attribute vec4 v_bone;"
				"attribute vec4 v_weight;"
				"uniform mat3x4 m_bones["STRINGIFY(MAX_BONES)"];\n"
				
				"vec4 skeletaltransform()"
				"{"
					"mat3x4 wmat;\n"
					"wmat = m_bones[int(v_bone.x)] * v_weight.x;\n"
					"wmat += m_bones[int(v_bone.y)] * v_weight.y;\n"
					"wmat += m_bones[int(v_bone.z)] * v_weight.z;\n"
					"wmat += m_bones[int(v_bone.w)] * v_weight.w;\n"
					"return m_modelviewprojection * vec4(vec4(v_position.xyz, 1.0) * wmat, 1.0);"
				"}\n"
				"vec4 skeletaltransform_nst(out vec3 n, out vec3 t, out vec3 b)"
				"{"
					"mat3x4 wmat;\n"
					"wmat = m_bones[int(v_bone.x)] * v_weight.x;"
					"wmat += m_bones[int(v_bone.y)] * v_weight.y;"
					"wmat += m_bones[int(v_bone.z)] * v_weight.z;"
					"wmat += m_bones[int(v_bone.w)] * v_weight.w;"
					"n = vec4(v_normal.xyz, 1.0) * wmat;"
					"t = vec4(v_svector.xyz, 1.0) * wmat;"
					"b = vec4(v_tvector.xyz, 1.0) * wmat;"
					"return m_modelviewprojection * vec4(vec4(v_position.xyz, 1.0) * wmat, 1.0);"
				"}\n"
				"vec4 skeletaltransform_wnst(out vec3 w, out vec3 n, out vec3 t, out vec3 b)"
				"{"
					"mat3x4 wmat;\n"
					"wmat = m_bones[int(v_bone.x)] * v_weight.x;"
					"wmat += m_bones[int(v_bone.y)] * v_weight.y;"
					"wmat += m_bones[int(v_bone.z)] * v_weight.z;"
					"wmat += m_bones[int(v_bone.w)] * v_weight.w;"
					"n = vec4(v_normal.xyz, 1.0) * wmat;"
					"t = vec4(v_svector.xyz, 1.0) * wmat;"
					"b = vec4(v_tvector.xyz, 1.0) * wmat;"
					"w = vec4(v_position.xyz, 1.0) * wmat;"
					"return m_modelviewprojection * vec4(w, 1.0);"
				"}\n"
				"vec4 skeletaltransform_n(out vec3 n)"
				"{"
					"mat3x4 wmat;\n"
					"wmat = m_bones[int(v_bone.x)] * v_weight.x;"
					"wmat += m_bones[int(v_bone.y)] * v_weight.y;"
					"wmat += m_bones[int(v_bone.z)] * v_weight.z;"
					"wmat += m_bones[int(v_bone.w)] * v_weight.w;"
					"n = vec4(v_normal.xyz, 1.0) * wmat;"
					"return m_modelviewprojection * vec4(vec4(v_position.xyz, 1.0) * wmat, 1.0);"
				"}\n"
			"#else\n"
				"#define skeletaltransform() ftetransform()\n"
				"vec4 skeletaltransform_wnst(out vec3 w, out vec3 n, out vec3 t, out vec3 b)"
				"{"
					"n = v_normal;"
					"t = v_svector;"
					"b = v_tvector;"
					"w = v_position.xyz;"
					"return ftetransform();"
				"}\n"
				"vec4 skeletaltransform_nst(out vec3 n, out vec3 t, out vec3 b)"
				"{"
					"n = v_normal;"
					"t = v_svector;"
					"b = v_tvector;"
					"return ftetransform();"
				"}\n"
				"vec4 skeletaltransform_n(out vec3 n)"
				"{"
					"n = v_normal;"
					"return ftetransform();"
				"}\n"
			"#endif\n"
		,
	"sys/fog.h",
			"#ifdef FRAGMENT_SHADER\n"
				"#ifdef FOG\n"
					"uniform vec4 w_fog;\n"
					"vec3 fog3(in vec3 regularcolour)"
					"{"
						"float z = gl_FragCoord.z / gl_FragCoord.w;\n"
						"float fac = exp2(-("
											"w_fog.w * w_fog.w * "
											"z * z * "
											"1.442695));\n"
						"fac = clamp(fac, 0.0, 1.0);\n"
						"return mix(w_fog.rgb, regularcolour, fac);\n"
					"}\n"
					"vec3 fog3additive(in vec3 regularcolour)"
					"{"
						"float z = gl_FragCoord.z / gl_FragCoord.w;\n"
						"float fac = exp2(-("
											"w_fog.w * w_fog.w * "
											"z * z * "
											"1.442695));\n"
						"fac = clamp(fac, 0.0, 1.0);\n"
						"return regularcolour * fac;\n"
					"}\n"
					"vec4 fog4(in vec4 regularcolour)"
					"{"
						"return vec4(fog3(regularcolour.rgb), 1.0) * regularcolour.a;\n"
					"}\n"
					"vec4 fog4additive(in vec4 regularcolour)"
					"{"
						"float z = gl_FragCoord.z / gl_FragCoord.w;\n"
						"float fac = exp2(-("
											"w_fog.w * w_fog.w * "
											"z * z * "
											"1.442695));\n"
						"fac = clamp(fac, 0.0, 1.0);\n"
						"return regularcolour * vec4(fac, fac, fac, 1.0);\n"
					"}\n"
					"vec4 fog4blend(in vec4 regularcolour)"
					"{"
						"float z = gl_FragCoord.z / gl_FragCoord.w;\n"
						"float fac = exp2(-("
											"w_fog.w * w_fog.w * "
											"z * z * "
											"1.442695));\n"
						"fac = clamp(fac, 0.0, 1.0);\n"
						"return regularcolour * vec4(1.0, 1.0, 1.0, fac);\n"
					"}\n"
				"#else\n"
					/*don't use macros for this - mesa bugs out*/
					"vec3 fog3(in vec3 regularcolour) { return regularcolour; }\n"
					"vec3 fog3additive(in vec3 regularcolour) { return regularcolour; }\n"
					"vec4 fog4(in vec4 regularcolour) { return regularcolour; }\n"
					"vec4 fog4additive(in vec4 regularcolour) { return regularcolour; }\n"
					"vec4 fog4blend(in vec4 regularcolour) { return regularcolour; }\n"
				"#endif\n"
			"#endif\n"
		,
	NULL
};

qboolean GLSlang_GenerateIncludes(int maxstrings, int *strings, const GLchar *prstrings[], GLint length[], const char *shadersource)
{
	int i;
	char *incline, *inc;
	char incname[256];
	while((incline=strstr(shadersource, "#include")))
	{
		if (*strings == maxstrings)
			return false;

		/*emit up to the include*/
		prstrings[*strings] = shadersource;
		length[*strings] = incline - shadersource;
		*strings += 1;

		incline += 8;
		incline = COM_ParseOut (incline, incname, sizeof(incname));

		for (i = 0; glsl_hdrs[i]; i += 2)
		{
			if (!strcmp(incname, glsl_hdrs[i]))
			{
				if (!GLSlang_GenerateIncludes(maxstrings, strings, prstrings, length, glsl_hdrs[i+1]))
					return false;
				break;
			}
		}
		if (!glsl_hdrs[i])
		{
			if (FS_LoadFile(incname, &inc) >= 0)
			{
				if (!GLSlang_GenerateIncludes(maxstrings, strings, prstrings, length, inc))
				{
					FS_FreeFile(inc);
					return false;
				}
				FS_FreeFile(inc);
			}
		}

		/*move the pointer past the include*/
		shadersource = incline;
	}
	if (*shadersource)
	{
		if (*strings == maxstrings)
			return false;

		/*dump the remaining shader string*/
		prstrings[*strings] = shadersource;
		length[*strings] = strlen(prstrings[*strings]);
		*strings += 1;
	}
	return true;
}

// glslang helper api function definitions
// type should be GL_FRAGMENT_SHADER_ARB or GL_VERTEX_SHADER_ARB
GLhandleARB GLSlang_CreateShader (char *name, int ver, char **precompilerconstants, const char *shadersource, GLenum shadertype)
{
	GLhandleARB shader;
	GLint       compiled;
	char        str[1024];
	int loglen, i;
	const GLchar *prstrings[64+16];
	GLint length[sizeof(prstrings)/sizeof(prstrings[0])];
	int strings = 0;

	if (ver)
	{
		prstrings[strings] = va("#version %u\n", ver);
		length[strings] = strlen(prstrings[strings]);
		strings++;
	}

	while(*precompilerconstants)
	{
		prstrings[strings] = *precompilerconstants++;
		length[strings] = strlen(prstrings[strings]);
		strings++;
	}

	prstrings[strings] = "#define ENGINE_"DISTRIBUTION"\n";
	length[strings] = strlen(prstrings[strings]);
	strings++;

	switch (shadertype)
	{
	case GL_FRAGMENT_SHADER_ARB:
		prstrings[strings] = "#define FRAGMENT_SHADER\n";
		length[strings] = strlen(prstrings[strings]);
		strings++;
		if (gl_config.gles)
		{
			prstrings[strings] =	"precision mediump float;\n";
			length[strings] = strlen(prstrings[strings]);
			strings++;
		}
		break;
	case GL_VERTEX_SHADER_ARB:
		prstrings[strings] = "#define VERTEX_SHADER\n";
		length[strings] = strlen(prstrings[strings]);
		strings++;
		if (gl_config.gles)
		{
			prstrings[strings] =
					"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
					"precision highp float;\n"
					"#else\n"
					"precision mediump float;\n"
					"#endif\n"
				;
			length[strings] = strlen(prstrings[strings]);
			strings++;
		}
		if (gl_config.nofixedfunc)
		{
			prstrings[strings] =
					"attribute vec3 v_position;\n"
					"#define ftetransform() (m_modelviewprojection * vec4(v_position, 1.0))\n"
					"uniform mat4 m_modelviewprojection;\n"
				;
			length[strings] = strlen(prstrings[strings]);
			strings++;
		}
		else
		{
			prstrings[strings] =
					"#define ftetransform ftransform\n"
					"#define v_position gl_Vertex\n"
					"uniform mat4 m_modelviewprojection;\n"
				;
			length[strings] = strlen(prstrings[strings]);
			strings++;
		}

		break;
	default:
		prstrings[strings] = "#define UNKNOWN_SHADER\n";
		length[strings] = strlen(prstrings[strings]);
		strings++;
		break;
	}

	GLSlang_GenerateIncludes(sizeof(prstrings)/sizeof(prstrings[0]), &strings, prstrings, length, shadersource);

	shader = qglCreateShaderObjectARB(shadertype);

	if (gl_workaround_ati_shadersource.ival)
	{
		/*ATI Driver Bug: ATI drivers ignore the 'length' array.
		this code does what the drivers fail to do.
		this patch makes the submission more mainstream
		if ati can feck it up so much on a system with no real system memory issues, I wouldn't be surprised if embedded systems also mess it up.
		*/
		char *combined;
		int totallen = 1;
		for (i = 0; i < strings; i++)
			totallen += length[i];
		combined = malloc(totallen);
		totallen = 0;
		combined[totallen] = 0;
		for (i = 0; i < strings; i++)
		{
			memcpy(combined + totallen, prstrings[i], length[i]);
			totallen += length[i];
			combined[totallen] = 0;
		}
		qglShaderSourceARB(shader, 1, &combined, NULL);
		free(combined);
	}
	else
		qglShaderSourceARB(shader, strings, prstrings, length);
	qglCompileShaderARB(shader);

	qglGetShaderParameteriv_(shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
	if(!compiled)
	{
		qglGetShaderInfoLog_(shader, sizeof(str), NULL, str);
		qglDeleteShaderObject_(shader);
		switch (shadertype)
		{
		case GL_FRAGMENT_SHADER_ARB:
			Con_Printf("Fragment shader (%s) compilation error:\n----------\n%s----------\n", name, str);
			break;
		case GL_VERTEX_SHADER_ARB:
			Con_Printf("Vertex shader (%s) compilation error:\n----------\n%s----------\n", name, str);
			break;
		default:
			Con_Printf("Shader_CreateShader: This shouldn't happen ever\n");
			break;
		}
		Con_DPrintf("Shader \"%s\" source:\n", name);
		for (i = 0; i < strings; i++)
		{
			int j;
			if (length[i] < 0)
				Con_DPrintf("%s", prstrings[i]);
			else
			{
				for (j = 0; j < length[i]; j++)
					Con_DPrintf("%c", prstrings[i][j]);
			}
		}
		Con_DPrintf("%s\n", str);
		return 0;
	}

	if (developer.ival)
	{
		qglGetShaderParameteriv_(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &loglen);
		if (loglen)
		{
			qglGetShaderInfoLog_(shader, sizeof(str), NULL, str);
			if (strstr(str, "WARNING"))
			{
				Con_Printf("Shader source:\n");
				for (i = 0; i < strings; i++)
					Con_Printf("%s", prstrings[i]);
				Con_Printf("%s\n", str);
			}
		}
	}

	return shader;
}

GLhandleARB GLSlang_CreateProgramObject (GLhandleARB vert, GLhandleARB frag)
{
	GLhandleARB program;
	GLint       linked;
	char        str[1024];

	program = qglCreateProgramObjectARB();
	qglAttachObjectARB(program, vert);
	qglAttachObjectARB(program, frag);

	qglBindAttribLocationARB(program, gl_config.nofixedfunc?0:7, "v_position");
	qglBindAttribLocationARB(program, 1, "v_colour");
	qglBindAttribLocationARB(program, 2, "v_texcoord");
	qglBindAttribLocationARB(program, 3, "v_lmcoord");
	qglBindAttribLocationARB(program, 4, "v_normal");
	qglBindAttribLocationARB(program, 5, "v_snormal");
	qglBindAttribLocationARB(program, 6, "v_tnormal");
	qglBindAttribLocationARB(program, 8, "v_bone");
	qglBindAttribLocationARB(program, 9, "v_weight");

	qglLinkProgramARB(program);

	qglGetProgramParameteriv_(program, GL_OBJECT_LINK_STATUS_ARB, &linked);

	if(!linked)
	{
		qglGetProgramInfoLog_(program, sizeof(str), NULL, str);
		Con_Printf("Program link error: %s\n", str);

		qglDeleteProgramObject_(program);

		return (GLhandleARB)0;
	}
	return program;
}

GLhandleARB GLSlang_CreateProgram(char *name, int ver, char **precompilerconstants, char *vert, char *frag)
{
	GLhandleARB handle;
	GLhandleARB vs;
	GLhandleARB fs;
	char *nullconstants = NULL;

	if (!gl_config.arb_shader_objects)
		return 0;

	if (!precompilerconstants)
		precompilerconstants = &nullconstants;

	vs = GLSlang_CreateShader(name, ver, precompilerconstants, vert, GL_VERTEX_SHADER_ARB);
	fs = GLSlang_CreateShader(name, ver, precompilerconstants, frag, GL_FRAGMENT_SHADER_ARB);

	if (!vs || !fs)
		handle = 0;
	else
		handle = GLSlang_CreateProgramObject(vs, fs);
	//delete ignores 0s.
	qglDeleteShaderObject_(vs);
	qglDeleteShaderObject_(fs);

	return handle;
}

GLint GLSlang_GetUniformLocation (int prog, char *name)
{
	int i = qglGetUniformLocationARB(prog, name);
	if (i == -1)
	{
		Con_Printf("Failed to get location of uniform '%s'\n", name);
	}
	return i;
}

//the vid routines have initialised a window, and now they are giving us a reference to some of of GetProcAddress to get pointers to the funcs.
void GL_Init(void *(*getglfunction) (char *name))
{
	unsigned int gl_major_version;
	unsigned int gl_minor_version;

	qglAlphaFunc		= (void *)getglcore("glAlphaFunc");
	qglBegin			= (void *)getglcore("glBegin");
	qglBlendFunc		= (void *)getglcore("glBlendFunc");
	bindTexFunc			= (void *)getglcore("glBindTexture");	//for compleateness
	qglClear			= (void *)getglcore("glClear");
	qglClearColor		= (void *)getglcore("glClearColor");
	qglClearDepth		= (void *)getglcore("glClearDepth");
	qglClearStencil		= (void *)getglcore("glClearStencil");
	qglClipPlane 		= (void *)getglcore("glClipPlane");
	qglColor3f			= (void *)getglcore("glColor3f");
	qglColor3ub			= (void *)getglcore("glColor3ub");
	qglColor4f			= (void *)getglcore("glColor4f");
	qglColor4fv			= (void *)getglcore("glColor4fv");
	qglColor4ub			= (void *)getglcore("glColor4ub");
	qglColor4ubv		= (void *)getglcore("glColor4ubv");
	qglColorMask		= (void *)getglcore("glColorMask");
	qglCopyTexImage2D	= (void *)getglcore("glCopyTexImage2D");
	qglCopyTexSubImage2D= (void *)getglcore("glCopyTexSubImage2D");
	qglCullFace			= (void *)getglcore("glCullFace");
	qglDepthFunc		= (void *)getglcore("glDepthFunc");
	qglDepthMask		= (void *)getglcore("glDepthMask");
	qglDepthRange		= (void *)getglcore("glDepthRange");
	qglDisable			= (void *)getglcore("glDisable");
	qglDrawBuffer		= (void *)getglcore("glDrawBuffer");
	qglDrawPixels		= (void *)getglcore("glDrawPixels");
	qglEnable			= (void *)getglcore("glEnable");
	qglEnd				= (void *)getglcore("glEnd");
	qglFinish			= (void *)getglcore("glFinish");
	qglFlush			= (void *)getglcore("glFlush");
	qglFrustum			= (void *)getglcore("glFrustum");
	qglGenTextures		= (void *)getglcore("glGenTextures");
	qglGetFloatv		= (void *)getglcore("glGetFloatv");
	qglGetIntegerv		= (void *)getglcore("glGetIntegerv");
	qglGetString		= (void *)getglcore("glGetString");
	qglGetTexLevelParameteriv	= (void *)getglcore("glGetTexLevelParameteriv");
	qglHint				= (void *)getglcore("glHint");
	qglLoadIdentity		= (void *)getglcore("glLoadIdentity");
	qglLoadMatrixf		= (void *)getglcore("glLoadMatrixf");
	qglNormal3f			= (void *)getglcore("glNormal3f");
	qglNormal3fv		= (void *)getglcore("glNormal3fv");
	qglMatrixMode		= (void *)getglcore("glMatrixMode");
	qglMultMatrixf		= (void *)getglcore("glMultMatrixf");
	qglOrtho			= (void *)getglcore("glOrtho");
	qglPolygonMode		= (void *)getglcore("glPolygonMode");
	qglPopMatrix		= (void *)getglcore("glPopMatrix");
	qglPushMatrix		= (void *)getglcore("glPushMatrix");
	qglReadBuffer		= (void *)getglcore("glReadBuffer");
	qglReadPixels		= (void *)getglcore("glReadPixels");
	qglRotatef			= (void *)getglcore("glRotatef");
	qglScalef			= (void *)getglcore("glScalef");
	qglShadeModel		= (void *)getglcore("glShadeModel");
	qglTexCoord1f		= (void *)getglcore("glTexCoord1f");
	qglTexCoord2f		= (void *)getglcore("glTexCoord2f");
	qglTexCoord2fv		= (void *)getglcore("glTexCoord2fv");
	qglTexEnvf			= (void *)getglcore("glTexEnvf");
	qglTexEnvfv			= (void *)getglcore("glTexEnvfv");
	qglTexEnvi			= (void *)getglcore("glTexEnvi");
	qglTexGeni			= (void *)getglcore("glTexGeni");
	qglTexGenfv			= (void *)getglcore("glTexGenfv");
	qglTexImage2D		= (void *)getglcore("glTexImage2D");
	qglTexImage3D		= (void *)getglext("glTexImage3D");
	qglTexParameteri	= (void *)getglcore("glTexParameteri");
	qglTexParameterf	= (void *)getglcore("glTexParameterf");
	qglTexParameteriv	= (void *)getglcore("glTexParameteriv");
	qglTexParameterfv	= (void *)getglcore("glTexParameterfv");
	qglTexSubImage2D	= (void *)getglcore("glTexSubImage2D");
	qglTranslatef		= (void *)getglcore("glTranslatef");
	qglVertex2f			= (void *)getglcore("glVertex2f");
	qglVertex3f			= (void *)getglcore("glVertex3f");
	qglVertex3fv		= (void *)getglcore("glVertex3fv");
	qglViewport			= (void *)getglcore("glViewport");

	qglGetError			= (void *)getglcore("glGetError");
	qglDeleteTextures	= (void *)getglcore("glDeleteTextures");

	//various vertex array stuff.
	qglDrawElements			= (void *)getglcore("glDrawElements");
	qglArrayElement			= (void *)getglcore("glArrayElement");
	qglVertexPointer		= (void *)getglcore("glVertexPointer");
	qglNormalPointer		= (void *)getglcore("glNormalPointer");
	qglTexCoordPointer		= (void *)getglcore("glTexCoordPointer");
	qglColorPointer			= (void *)getglcore("glColorPointer");
	qglDrawArrays			= (void *)getglcore("glDrawArrays");
	qglEnableClientState	= (void *)getglcore("glEnableClientState");
	qglDisableClientState	= (void *)getglcore("glDisableClientState");

	qglDrawRangeElements	= (void *)getglext("glDrawRangeElements");
	if (qglDrawRangeElements == 0)
		qglDrawRangeElements = GL_DrawRangeElementsEmul;

	//fixme: definatly make non-core
	qglStencilOp		= (void *)getglcore("glStencilOp");
	qglStencilFunc		= (void *)getglcore("glStencilFunc");
	qglPushAttrib		= (void *)getglcore("glPushAttrib");
	qglPopAttrib		= (void *)getglcore("glPopAttrib");
	qglScissor			= (void *)getglcore("glScissor");

	//does this need to be non-core as well?
	qglFogi				= (void *)getglcore("glFogi");
	qglFogf				= (void *)getglcore("glFogf");
	qglFogfv			= (void *)getglcore("glFogfv");

	qglPolygonOffset	= (void *)getglext("glPolygonOffset");

	qglGetStringi		= (void *)getglext("glGetStringi");

	//used by heightmaps
	qglGenLists		= (void*)getglcore("glGenLists");
	qglNewList		= (void*)getglcore("glNewList");
	qglEndList		= (void*)getglcore("glEndList");
	qglCallList		= (void*)getglcore("glCallList");

	qglBindBufferARB		= (void *)getglext("glBindBufferARB");
	if (!qglBindBufferARB)
		qglBindBufferARB	= (void *)getglext("glBindBuffer");
	if (!qglBindBufferARB)
		qglBindBufferARB	= GL_BindBufferARBStub;

	gl_vendor = qglGetString (GL_VENDOR);
	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = qglGetString (GL_RENDERER);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = qglGetString (GL_VERSION);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);

	if (qglGetError())
		Con_Printf("glGetError %s:%i\n", __FILE__, __LINE__);
	qglGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);
	qglGetIntegerv(GL_MINOR_VERSION, &gl_minor_version);
	if (qglGetError())
	{
		/*GL_MAJOR_VERSION not supported? try and parse (es-aware)*/
		const char *s;
		for (s = gl_version; *s && (*s < '0' || *s > '9'); s++)
			;
		gl_major_version = atoi(s);
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s == '.')
			s++;
		gl_minor_version = atoi(s);
	}
	/*gl3 adds glGetStringi instead, as core, with the old form require GL_ARB_compatibility*/
	if (gl_major_version >= 3 && qglGetStringi) /*warning: wine fails to export qglGetStringi*/
	{
		int i;
		qglGetIntegerv(GL_NUM_EXTENSIONS, &gl_num_extensions);
		if (developer.value)
		{
			Con_Printf ("GL_EXTENSIONS:\n");
			for (i = 0; i < gl_num_extensions; i++)
			{
				Con_Printf (" %s", qglGetStringi(GL_EXTENSIONS, i));
				Con_Printf("\n");
			}
			Con_Printf ("end of list\n");
		}
		else
			Con_Printf ("GL_EXTENSIONS: %i extensions\n", gl_num_extensions);
		gl_extensions = NULL;
	}
	else
	{
		gl_num_extensions = 0;
		gl_extensions = qglGetString (GL_EXTENSIONS);
		Con_DPrintf ("GL_EXTENSIONS: %s\n", gl_extensions);

		if (!gl_extensions)
			Sys_Error("no extensions\n");
	}
	GL_CheckExtensions (getglfunction, gl_major_version + (gl_minor_version/10.f));

	if (gl_config.gles && gl_config.glversion >= 2)
	{
		/*no matricies in gles, so don't try!*/
		qglLoadMatrixf = NULL;
		qglPolygonMode = NULL;
		qglShadeModel = NULL;
		qglDepthRange = NULL;

		qglEnableClientState = GL_ClientStateStub;
		qglDisableClientState = GL_ClientStateStub;

		qglDrawRangeElements = GL_DrawRangeElementsEmul;
	}
	else if (gl_config.nofixedfunc)
	{
		qglLoadMatrixf = NULL;
		qglPolygonMode = NULL;
		qglShadeModel = NULL;
		qglDepthRange = NULL;

		qglEnableClientState = GL_ClientStateStub;
		qglDisableClientState = GL_ClientStateStub;
	}

	qglClearColor (0,0,0,0);	//clear to black so that it looks a little nicer on start.
	qglClear(GL_COLOR_BUFFER_BIT);

	if (qglPolygonMode)
		qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	if (qglShadeModel)
		qglShadeModel (GL_FLAT);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

#ifdef DEBUG
	if (qglDebugMessageControlARB)
		qglDebugMessageControlARB(0, 0, 0, 0, NULL, true);
	if (qglDebugMessageCallbackARB)
		qglDebugMessageCallbackARB(myGLDEBUGPROCAMD, NULL);
	qglGetError();	/*suck up the invalid operation error for non-debug contexts*/
#endif
}

unsigned int	d_8to24rgbtable[256];





rendererinfo_t openglrendererinfo = {
	"OpenGL",
	{
		"gl",
		"opengl",
		"hardware",
	},
	QR_OPENGL,


	GLDraw_Init,
	GLDraw_DeInit,

	GL_LoadTextureFmt,
	GL_LoadTexture8Pal24,
	GL_LoadTexture8Pal32,
	GL_LoadCompressed,
	GL_FindTexture,
	GL_AllocNewTexture,
	GL_UploadFmt,
	GL_DestroyTexture,

	GLR_Init,
	GLR_DeInit,
	GLR_RenderView,


	GLR_NewMap,
	GLR_PreNewMap,
	GLR_LightPoint,


	Surf_AddStain,
	Surf_LessenStains,

	RMod_Init,
	RMod_Shutdown,
	RMod_ClearAll,
	RMod_ForName,
	RMod_FindName,
	RMod_Extradata,
	RMod_TouchModel,

	RMod_NowLoadExternal,
	RMod_Think,

	Mod_GetTag,
	Mod_TagNumForName,
	Mod_SkinNumForName,
	Mod_FrameNumForName,
	Mod_FrameDuration,

	GLVID_Init,
	GLVID_DeInit,
	GLVID_SetPalette,
	GLVID_ShiftPalette,
	GLVID_GetRGBInfo,

	GLVID_SetCaption,	//setcaption


	GLSCR_UpdateScreen,

	GLBE_SelectMode,
	GLBE_DrawMesh_List,
	GLBE_DrawMesh_Single,
	GLBE_SubmitBatch,
	GLBE_GetTempBatch,
	GLBE_DrawWorld,
	GLBE_Init,
	GLBE_GenBrushModelVBO,
	GLBE_ClearVBO,
	GLBE_UploadAllLightmaps,
	GLBE_SelectEntity,
	GLBE_SelectDLight,
	GLBE_LightCullModel,

	""
};

#endif

