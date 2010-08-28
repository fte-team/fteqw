#include "quakedef.h"
#ifdef GLQUAKE
#include "glquake.h"
#include "gl_draw.h"

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

/*
PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB;
PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB;
PFNGLBINDPROGRAMARBPROC qglBindProgramARB;
PFNGLGENPROGRAMSARBPROC qglGenProgramsARB;
*/
FTEPFNGLLOCKARRAYSEXTPROC qglLockArraysEXT;
FTEPFNGLUNLOCKARRAYSEXTPROC qglUnlockArraysEXT;

//glslang - arb_shader_objects
FTEPFNGLCREATEPROGRAMOBJECTARBPROC  qglCreateProgramObjectARB;
FTEPFNGLDELETEOBJECTARBPROC         qglDeleteObjectARB;
FTEPFNGLUSEPROGRAMOBJECTARBPROC     qglUseProgramObjectARB;
FTEPFNGLCREATESHADEROBJECTARBPROC   qglCreateShaderObjectARB;
FTEPFNGLSHADERSOURCEARBPROC         qglShaderSourceARB;
FTEPFNGLCOMPILESHADERARBPROC        qglCompileShaderARB;
FTEPFNGLGETOBJECTPARAMETERIVARBPROC qglGetObjectParameterivARB;
FTEPFNGLATTACHOBJECTARBPROC         qglAttachObjectARB;
FTEPFNGLGETINFOLOGARBPROC           qglGetInfoLogARB;
FTEPFNGLLINKPROGRAMARBPROC          qglLinkProgramARB;
FTEPFNGLGETUNIFORMLOCATIONARBPROC   qglGetUniformLocationARB;
FTEPFNGLUNIFORMMATRIX4FVARBPROC		qglUniformMatrix4fvARB;
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
int mtexid1;

//ati_truform
FTEPFNGLPNTRIANGLESIATIPROC qglPNTrianglesiATI;
FTEPFNGLPNTRIANGLESFATIPROC qglPNTrianglesfATI;

//stencil shadowing
void (APIENTRY *qglStencilOpSeparateATI) (GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
FTEPFNGLACTIVESTENCILFACEEXTPROC qglActiveStencilFaceEXT;

//quick hack that made quake work on both 1 and 1.1 gl implementations.
BINDTEXFUNCPTR bindTexFunc;

#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif
#if defined(DEBUG)

#define GLchar char
typedef void (APIENTRY *GLDEBUGPROCAMD)(GLuint id,
					GLenum category,
					GLenum severity,
					GLsizei lengt,
					const GLchar* message,
					GLvoid* userParam);
void (*qglDebugMessageEnableAMD)(GLenum category,
					GLenum severity,
					GLsizei count,
					const GLuint* ids,
					GLboolean enabled);
void (*qglDebugMessageInsertAMD)(enum category,
					enum severity,
					GLuint id,
					GLsizei length, 
					const char* buf);
void (*qglDebugMessageCallbackAMD)(GLDEBUGPROCAMD callback,
					void* userParam);
GLuint (*qglGetDebugMessageLogAMD)(GLuint count,
					GLsizei bufsize,
					GLenum* categories,
					GLuint* severities,
					GLuint* ids,
					GLsizei* lengths, 
					char* message);

#define GL_DEBUG_CATEGORY_API_ERROR_AMD                    0x9149
#define GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD                0x914A
#define GL_DEBUG_CATEGORY_DEPRECATION_AMD                  0x914B
#define GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD           0x914C
#define GL_DEBUG_CATEGORY_PERFORMANCE_AMD                  0x914D
#define GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD              0x914E
#define GL_DEBUG_CATEGORY_APPLICATION_AMD                  0x914F
#define GL_DEBUG_CATEGORY_OTHER_AMD                        0x9150


void (APIENTRY myGLDEBUGPROCAMD)(GLuint id,
					GLenum category,
					GLenum severity,
					GLsizei length,
					const GLchar* message,
					GLvoid* userParam)
{
#ifndef _WIN32
#define OutputDebugString(s) puts(s)
#endif
	switch(category)
	{
	case GL_DEBUG_CATEGORY_API_ERROR_AMD:
		OutputDebugString("glerr: ");
		break;
	case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD:
		OutputDebugString("glwsys: ");
		break;
	case GL_DEBUG_CATEGORY_DEPRECATION_AMD:
		OutputDebugString("gldepr: ");
		break;
	case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD:
		OutputDebugString("glundef: ");
		break;
	case GL_DEBUG_CATEGORY_PERFORMANCE_AMD:
		OutputDebugString("glperf: ");
		break;
	case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD:
		OutputDebugString("glshad: ");
		break;
	case GL_DEBUG_CATEGORY_APPLICATION_AMD:
		OutputDebugString("glappm: ");
		break;
	default:
	case GL_DEBUG_CATEGORY_OTHER_AMD:
		OutputDebugString("glothr: ");
		break;
	}
	OutputDebugString(message);
	OutputDebugString("\n");
}
#endif


int gl_mtexarbable=0;	//max texture units
qboolean gl_mtexable = false;
int gl_bumpmappingpossible;


qboolean gammaworks;	//if the gl drivers can set proper gamma.


gl_config_t gl_config;
int		gl_canstencil;

float		gldepthmin, gldepthmax;
const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
static const char *gl_extensions;

unsigned int gl_major_version;
unsigned int gl_minor_version;
static unsigned int gl_num_extensions;


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

texid_t GL_AllocNewTexture(void)
{
	texid_t r;
	qglGenTextures(1, &r.num);
	return r;
}

void GL_DestroyTexture(texid_t tex)
{
	qglDeleteTextures(1, &tex.num);
}

void APIENTRY GL_DrawRangeElementsEmul(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
	qglDrawElements(mode, count, type, indices);
}
void APIENTRY GL_BindBufferARBStub(GLenum target, GLuint id)
{
}

#define getglcore getglfunction
#define getglext(name) getglfunction(name)
void GL_CheckExtensions (void *(*getglfunction) (char *name))
{
	extern cvar_t gl_bump;

	memset(&gl_config, 0, sizeof(gl_config));

	//multitexture
	gl_mtexable = false;
	gl_mtexarbable = 0;
	qglActiveTextureARB = NULL;
	qglMultiTexCoord2fARB = NULL;
	qglMultiTexCoord3fARB = NULL;
	qglMTexCoord2fSGIS = NULL;
	qglSelectTextureSGIS = NULL;
	mtexid0 = 0;
	mtexid1 = 0;

	//none of them bumpmapping possibilities.
	gl_bumpmappingpossible = false;

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

		Con_SafePrintf("Anisotropic filter extension found (%dx max).\n",gl_config.ext_texture_filter_anisotropic);
	}

	if (GL_CheckExtension("GL_ARB_texture_non_power_of_two"))
		gl_config.arb_texture_non_power_of_two = true;
//	if (GL_CheckExtension("GL_SGIS_generate_mipmap"))	//a suprising number of implementations have this broken.
//		gl_config.sgis_generate_mipmap = true;

	if (GL_CheckExtension("GL_ARB_multitexture") && !COM_CheckParm("-noamtex"))
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
		mtexid1 = GL_TEXTURE1_ARB;

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
			Con_SafePrintf("ARB Multitexture extensions found. Use -noamtex to disable.\n");
		}

	}
	else if (GL_CheckExtension("GL_SGIS_multitexture") && !COM_CheckParm("-nomtex"))
	{	//SGIS multitexture, limited in many ways but basic functionality is identical to ARB
		Con_SafePrintf("Multitexture extensions found.\n");
		qglMTexCoord2fSGIS = (void *) getglext("glMTexCoord2fSGIS");
		qglSelectTextureSGIS = (void *) getglext("glSelectTextureSGIS");
		gl_mtexable = true;

		mtexid0 = GL_TEXTURE0_SGIS;
		mtexid1 = GL_TEXTURE1_SGIS;
	}

	if (GL_CheckExtension("GL_EXT_stencil_wrap"))
		gl_config.ext_stencil_wrap = true;

	if (GL_CheckExtension("GL_ATI_separate_stencil"))
		qglStencilOpSeparateATI = (void *) getglext("glStencilOpSeparateATI");
	if (GL_CheckExtension("GL_EXT_stencil_two_side"))
		qglActiveStencilFaceEXT = (void *) getglext("glActiveStencilFaceEXT");

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

	if (gl_mtexarbable && gl_config.arb_texture_cube_map && gl_config.arb_texture_env_combine && gl_config.arb_texture_env_dot3 && !COM_CheckParm("-nobump") && gl_bump.value)
		gl_bumpmappingpossible = true;

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

/*
	if (GL_CheckExtension("GL_ARB_fragment_program"))
	{
		gl_config.arb_fragment_program = true;
		qglProgramStringARB = (void *)getglext("glProgramStringARB");
		qglGetProgramivARB = (void *)getglext("glGetProgramivARB");
		qglBindProgramARB = (void *)getglext("glBindProgramARB");
		qglGenProgramsARB = (void *)getglext("glGenProgramsARB");
	}
*/

	// glslang
	//the gf2 to gf4 cards emulate vertex_shader and thus supports shader_objects.
	//but our code kinda requires both for clean workings.
	if (GL_CheckExtension("GL_ARB_fragment_shader")
		&& GL_CheckExtension("GL_ARB_vertex_shader")
		&& GL_CheckExtension("GL_ARB_shader_objects"))
	{
		gl_config.arb_shader_objects = true;
		qglCreateProgramObjectARB	= (void *)getglext("glCreateProgramObjectARB");
		qglDeleteObjectARB			= (void *)getglext("glDeleteObjectARB");
		qglUseProgramObjectARB		= (void *)getglext("glUseProgramObjectARB");
		qglCreateShaderObjectARB	= (void *)getglext("glCreateShaderObjectARB");
		qglShaderSourceARB			= (void *)getglext("glShaderSourceARB");
		qglCompileShaderARB			= (void *)getglext("glCompileShaderARB");
		qglGetObjectParameterivARB	= (void *)getglext("glGetObjectParameterivARB");
		qglAttachObjectARB			= (void *)getglext("glAttachObjectARB");
		qglGetInfoLogARB			= (void *)getglext("glGetInfoLogARB");
		qglLinkProgramARB			= (void *)getglext("glLinkProgramARB");
		qglGetUniformLocationARB	= (void *)getglext("glGetUniformLocationARB");
		qglUniformMatrix4fvARB		= (void *)getglext("glUniformMatrix4fvARB");
		qglUniform4fARB				= (void *)getglext("glUniform4fARB");
		qglUniform4fvARB			= (void *)getglext("glUniform4fvARB");
		qglUniform3fARB				= (void *)getglext("glUniform3fARB");
		qglUniform3fvARB			= (void *)getglext("glUniform3fvARB");
		qglUniform1iARB				= (void *)getglext("glUniform1iARB");
		qglUniform1fARB				= (void *)getglext("glUniform1fARB");
	}

	if (GL_CheckExtension("GL_EXT_framebuffer_object"))
	{
		gl_config.ext_framebuffer_objects = true;
		qglGenFramebuffersEXT		= (void *)getglext("glGenFramebuffersEXT");
		qglDeleteFramebuffersEXT	= (void *)getglext("glDeleteFramebuffersEXT");
		qglBindFramebufferEXT		= (void *)getglext("glBindFramebufferEXT");
		qglGenRenderbuffersEXT		= (void *)getglext("glGenRenderbuffersEXT");
		qglDeleteRenderbuffersEXT	= (void *)getglext("glDeleteRenderbuffersEXT");
		qglBindRenderbufferEXT		= (void *)getglext("glBindRenderbufferEXT");
		qglRenderbufferStorageEXT	= (void *)getglext("glRenderbufferStorageEXT");
		qglFramebufferTexture2DEXT	= (void *)getglext("glFramebufferTexture2DEXT");
	}

#ifdef DEBUG
	if (GL_CheckExtension("GL_AMD_debug_output"))
	{
		qglDebugMessageEnableAMD	= (void *)getglext("glDebugMessageEnableAMD");
		qglDebugMessageInsertAMD	= (void *)getglext("glDebugMessageInsertAMD");
		qglDebugMessageCallbackAMD	= (void *)getglext("glDebugMessageCallbackAMD");
		qglGetDebugMessageLogAMD	= (void *)getglext("glGetDebugMessageLogAMD");
	}
	else
	{
		qglDebugMessageEnableAMD = NULL;
		qglDebugMessageInsertAMD = NULL;
		qglDebugMessageCallbackAMD = NULL;
		qglGetDebugMessageLogAMD = NULL;
	}
#endif
}

// glslang helper api function definitions
// type should be GL_FRAGMENT_SHADER_ARB or GL_VERTEX_SHADER_ARB
GLhandleARB GLSlang_CreateShader (char *precompilerconstants, char *shadersource, GLenum shadertype)
{
	GLhandleARB shader;
	GLint       compiled;
	char        str[1024];
	int loglen;
	char *prstrings[4];

	prstrings[0] = "#define ENGINE_"DISTRIBUTION"\n";
	switch (shadertype)
	{
	case GL_FRAGMENT_SHADER_ARB:
		prstrings[1] = "#define FRAGMENT_SHADER\n";
		break;
	case GL_VERTEX_SHADER_ARB:
		prstrings[1] = "#define VERTEX_SHADER\n";
		break;
	default:
		prstrings[1] = "#define UNKNOWN_SHADER\n";
		break;
	}
	prstrings[2] = precompilerconstants;
	prstrings[3] = shadersource;

	shader = qglCreateShaderObjectARB(shadertype);

	qglShaderSourceARB(shader, 4, (const GLcharARB**)prstrings, NULL);
	qglCompileShaderARB(shader);

	qglGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
	if(!compiled)
	{
		Con_DPrintf("Shader source:\n%s%s%s\n", prstrings[0], prstrings[1], prstrings[2], prstrings[3]);
		qglGetInfoLogARB(shader, sizeof(str), NULL, str);
		qglDeleteObjectARB(shader);
		switch (shadertype)
		{
		case GL_FRAGMENT_SHADER_ARB:
			Con_Printf("Fragment shader compilation error:\n----------\n%s\n----------\n", str);
			break;
		case GL_VERTEX_SHADER_ARB:
			Con_Printf("Vertex shader compilation error:\n----------\n%s\n----------\n", str);
			break;
		default:
			Con_Printf("Shader_CreateShader: This shouldn't happen ever\n");
			break;
		}
		return 0;
	}

	if (developer.ival)
	{
		qglGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &loglen);
		if (loglen)
		{
			qglGetInfoLogARB(shader, sizeof(str), NULL, str);
			if (strstr(str, "WARNING"))
			{
				Con_Printf("Shader source:\n%s%s%s\n", prstrings[0], prstrings[1], prstrings[2], prstrings[3]);
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

	qglLinkProgramARB(program);

	//flag the source objects for deletion, they'll only be deleted when they're no longer attached to anything
	qglDeleteObjectARB(vert);
	qglDeleteObjectARB(frag);

	qglGetObjectParameterivARB(program, GL_OBJECT_LINK_STATUS_ARB, &linked);

	if(!linked)
	{
		qglGetInfoLogARB(program, sizeof(str), NULL, str);
		Con_Printf("Program link error: %s\n", str);
		return (GLhandleARB)0;
	}

	return program;
}

#if HASHPROGRAMS
struct compiledshaders_s
{
	int uses;
	char *consts;
	char *vert;
	char *frag;
	GLhandleARB handle;
	bucket_t buck;
};

bucket_t *compiledshadersbuckets[64];
static hashtable_t compiledshaderstable;
#endif

GLhandleARB GLSlang_CreateProgram(char *precompilerconstants, char *vert, char *frag)
{
	GLhandleARB handle;
	GLhandleARB vs;
	GLhandleARB fs;
#if HASHPROGRAMS
	unsigned int hashkey;
	struct compiledshaders_s *cs;
#endif

	if (!gl_config.arb_shader_objects)
		return 0;

	if (!precompilerconstants)
		precompilerconstants = "";

#if HASHPROGRAMS
	hashkey = Hash_Key(precompilerconstants, ~0) ^ Hash_Key(frag, ~0);

	cs = Hash_GetKey(&compiledshaderstable, hashkey);
	while(cs)
	{
		if (!strcmp(cs->consts, precompilerconstants))
		if (!strcmp(cs->vert, vert))
		if (!strcmp(cs->frag, frag))
		{
			cs->uses++;
			return cs->handle;
		}
		cs = Hash_GetNextKey(&compiledshaderstable, hashkey, cs);
	}
#endif

	vs = GLSlang_CreateShader(precompilerconstants, vert, GL_VERTEX_SHADER_ARB);
	fs = GLSlang_CreateShader(precompilerconstants, frag, GL_FRAGMENT_SHADER_ARB);
	if (!vs || !fs)
		handle = 0;
	else
		handle = GLSlang_CreateProgramObject(vs, fs);
	//delete ignores 0s.
	qglDeleteObjectARB(vs);
	qglDeleteObjectARB(fs);

#if HASHPROGRAMS
	cs = Z_Malloc(sizeof(*cs) + strlen(precompilerconstants)+1+strlen(vert)+1+strlen(frag)+1);
	cs->consts = (char*)(cs + 1);
	cs->vert = cs->consts + strlen(precompilerconstants)+1;
	cs->frag = cs->vert + strlen(vert)+1;
	cs->handle = handle;
	cs->uses = 1;
	strcpy(cs->consts, precompilerconstants);
	strcpy(cs->vert, vert);
	strcpy(cs->frag, frag);
	Hash_AddKey(&compiledshaderstable, hashkey, cs, &cs->buck);
#endif

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

	qglBindBufferARB		= (void *)getglext("qglBindBufferARB");
	if (!qglBindBufferARB)
		qglBindBufferARB	= (void *)getglext("qglBindBuffer");
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
		gl_major_version = 1;
		gl_minor_version = 1;
	}
	qglGetIntegerv(GL_NUM_EXTENSIONS, &gl_num_extensions);
	if (!qglGetError() && gl_num_extensions)
	{
		int i;
		if (developer.value)
		{
			Con_Printf ("GL_EXTENSIONS:");
			for (i = 0; i < gl_num_extensions; i++)
				Con_Printf (" %s", qglGetStringi(GL_EXTENSIONS, i));
			Con_Printf ("\n");
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

	GL_CheckExtensions (getglfunction);

	qglClearColor (0,0,0,0);	//clear to black so that it looks a little nicer on start.
	qglClear(GL_COLOR_BUFFER_BIT);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglShadeModel (GL_FLAT);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

#ifdef DEBUG
	if (qglDebugMessageEnableAMD)
		qglDebugMessageEnableAMD(0, 0, 0, NULL, true);
	if (qglDebugMessageCallbackAMD)
		qglDebugMessageCallbackAMD(myGLDEBUGPROCAMD, NULL);
#endif

#if HASHPROGRAMS
	Hash_InitTable(&compiledshaderstable, sizeof(compiledshadersbuckets)/Hash_BytesForBuckets(1), compiledshadersbuckets);
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


	R2D_SafePicFromWad,
	R2D_SafeCachePic,
	GLDraw_Init,
	GLDraw_ReInit,
	GLDraw_Crosshair,
	R2D_ScalePic,
	R2D_SubPic,
	GLDraw_TransPicTranslate,
	R2D_ConsoleBackground,
	R2D_EditorBackground,
	R2D_TileClear,
	GLDraw_Fill,
	GLDraw_FillRGB,
	GLDraw_FadeScreen,
	GLDraw_BeginDisc,
	GLDraw_EndDisc,

	R2D_Image,
	R2D_ImageColours,

	GLR_Init,
	GLR_DeInit,
	GLR_RenderView,


	GLR_NewMap,
	GLR_PreNewMap,
	GLR_LightPoint,
	GLR_PushDlights,


	Surf_AddStain,
	Surf_LessenStains,

	RMod_Init,
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

	""
};

#endif
