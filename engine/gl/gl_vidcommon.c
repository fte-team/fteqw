#include "quakedef.h"
#include "glquake.h"

//standard 1.1 opengl calls
void (APIENTRY *qglAlphaFunc) (GLenum func, GLclampf ref);
void (APIENTRY *qglBegin) (GLenum mode);
void (APIENTRY *qglBlendFunc) (GLenum sfactor, GLenum dfactor);
void (APIENTRY *qglClear) (GLbitfield mask);
void (APIENTRY *qglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void (APIENTRY *qglClearDepth) (GLclampd depth);
void (APIENTRY *qglClearStencil) (GLint s);
void (APIENTRY *qglColor3f) (GLfloat red, GLfloat green, GLfloat blue);
void (APIENTRY *qglColor3ub) (GLubyte red, GLubyte green, GLubyte blue);
void (APIENTRY *qglColor4f) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void (APIENTRY *qglColor4fv) (const GLfloat *v);
void (APIENTRY *qglColor4ub) (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void (APIENTRY *qglColor4ubv) (const GLubyte *v);
void (APIENTRY *qglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void (APIENTRY *qglCullFace) (GLenum mode);
void (APIENTRY *qglDepthFunc) (GLenum func);
void (APIENTRY *qglDepthMask) (GLboolean flag);
void (APIENTRY *qglDepthRange) (GLclampd zNear, GLclampd zFar);
void (APIENTRY *qglDisable) (GLenum cap);
void (APIENTRY *qglDrawBuffer) (GLenum mode);
void (APIENTRY *qglDrawPixels) (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void (APIENTRY *qglEnable) (GLenum cap);
void (APIENTRY *qglEnd) (void);
void (APIENTRY *qglFinish) (void);
void (APIENTRY *qglFlush) (void);
void (APIENTRY *qglFrustum) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
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
void (APIENTRY *qglOrtho) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void (APIENTRY *qglPolygonMode) (GLenum face, GLenum mode);
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
void (APIENTRY *qglTexImage2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (APIENTRY *qglTexParameteri) (GLenum target, GLenum pname, GLint param);
void (APIENTRY *qglTexParameterf) (GLenum target, GLenum pname, GLfloat param);
void (APIENTRY *qglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void (APIENTRY *qglTranslatef) (GLfloat x, GLfloat y, GLfloat z);
void (APIENTRY *qglVertex2f) (GLfloat x, GLfloat y);
void (APIENTRY *qglVertex3f) (GLfloat x, GLfloat y, GLfloat z);
void (APIENTRY *qglVertex3fv) (const GLfloat *v);
void (APIENTRY *qglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);
void (APIENTRY *qglGetTexLevelParameteriv) (GLenum target, GLint level, GLenum pname, GLint *params);

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

PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB;
PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB;
PFNGLBINDPROGRAMARBPROC qglBindProgramARB;
PFNGLGENPROGRAMSARBPROC qglGenProgramsARB;

PFNGLLOCKARRAYSEXTPROC qglLockArraysEXT;
PFNGLUNLOCKARRAYSEXTPROC qglUnlockArraysEXT;



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
PFNGLPNTRIANGLESIATIPROC qglPNTrianglesiATI;
PFNGLPNTRIANGLESFATIPROC qglPNTrianglesfATI;

//stencil shadowing
void (APIENTRY *qglStencilOpSeparateATI) (GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
PFNGLACTIVESTENCILFACEEXTPROC qglActiveStencilFaceEXT;

//quick hack that made quake work on both 1 and 1.1 gl implementations.
BINDTEXFUNCPTR bindTexFunc;


int gl_mtexarbable=0;	//max texture units
qboolean gl_mtexable = false;
int gl_bumpmappingpossible;


qboolean gammaworks;	//if the gl drivers can set proper gamma.


gl_config_t gl_config;

//int		texture_mode = GL_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_LINEAR;
int		texture_mode = GL_LINEAR;
//int		texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int		texture_mode = GL_LINEAR_MIPMAP_LINEAR;

int		texture_extension_number = 1;

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

	//none of them bumpmapping possabilities.
	gl_bumpmappingpossible = false;

	//no GL_ATI_separate_stencil
	qglStencilOpSeparateATI = NULL;

	//no GL_EXT_stencil_two_side
	qglActiveStencilFaceEXT = NULL;

	//no truform. sorry.
	qglPNTrianglesfATI = NULL;
	qglPNTrianglesiATI = NULL;

	//fragment programs
	qglProgramStringARB = NULL;
	qglGetProgramivARB = NULL;
	qglBindProgramARB = NULL;
	qglGenProgramsARB = NULL;

	gl_config.arb_texture_non_power_of_two = false;
	gl_config.sgis_generate_mipmap = false;

	if (strstr(gl_extensions, "GL_ARB_texture_non_power_of_two"))
		gl_config.arb_texture_non_power_of_two = true;
	if (strstr(gl_extensions, "GL_SGIS_generate_mipmap"))
		gl_config.sgis_generate_mipmap = true;

	if (strstr(gl_extensions, "GL_ARB_multitexture") && !COM_CheckParm("-noamtex"))
	{	//ARB multitexture is the popular choice.
		Con_SafePrintf("ARB Multitexture extensions found. Use -noamtex to disable.\n");
		qglActiveTextureARB = (void *) getglext("glActiveTextureARB");
		qglClientActiveTextureARB = (void *) getglext("glClientActiveTextureARB");
		qglMultiTexCoord2fARB = (void *) getglext("glMultiTexCoord2fARB");
		qglMultiTexCoord3fARB = (void *) getglext("glMultiTexCoord3fARB");		

		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_mtexarbable);
		gl_mtexable = true;

		qglMTexCoord2fSGIS = qglMultiTexCoord2fARB;
		qglSelectTextureSGIS = qglActiveTextureARB;

		mtexid0 = GL_TEXTURE0_ARB;
		mtexid1 = GL_TEXTURE1_ARB;

		if (!qglActiveTextureARB || !qglClientActiveTextureARB || !qglMultiTexCoord2fARB || !qglMultiTexCoord3fARB)
		{
			qglActiveTextureARB = NULL;
			qglMultiTexCoord2fARB = NULL;
			qglMultiTexCoord3fARB = NULL;
			qglMTexCoord2fSGIS = NULL;
			qglSelectTextureSGIS = NULL;
			gl_mtexable=false;
			gl_mtexarbable = false;
		}

	}
	else if (strstr(gl_extensions, "GL_SGIS_multitexture") && !COM_CheckParm("-nomtex"))
	{	//SGIS multitexture, limited in many ways but basic functionality is identical to ARB
		Con_SafePrintf("Multitexture extensions found.\n");
		qglMTexCoord2fSGIS = (void *) getglext("glMTexCoord2fSGIS");
		qglSelectTextureSGIS = (void *) getglext("glSelectTextureSGIS");
		gl_mtexable = true;

		mtexid0 = GL_TEXTURE0_SGIS;
		mtexid1 = GL_TEXTURE1_SGIS;
	}

	if (strstr(gl_extensions, "GL_EXT_stencil_wrap"))
		gl_config.ext_stencil_wrap = true;

	if (strstr(gl_extensions, "GL_ATI_separate_stencil"))
		qglStencilOpSeparateATI = (void *) getglext("glStencilOpSeparateATI");
	if (strstr(gl_extensions, "GL_EXT_stencil_two_side"))
		qglActiveStencilFaceEXT = (void *) getglext("glActiveStencilFaceEXT");

	if (strstr(gl_extensions, "GL_ARB_texture_compression"))
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

	if (strstr(gl_extensions, "GL_ATI_pn_triangles"))
	{
		qglPNTrianglesfATI = (void *)getglext("glPNTrianglesfATI");
		qglPNTrianglesiATI = (void *)getglext("glPNTrianglesiATI");
	}

	if (strstr(gl_extensions, "GL_EXT_texture_object"))
	{
		bindTexFunc			= (void *)getglext("glBindTextureEXT");
		if (!bindTexFunc)	//grrr
			bindTexFunc			= (void *)getglext("glBindTexture");
	}

	gl_config.tex_env_combine = !!strstr(gl_extensions, "GL_EXT_texture_env_combine");
	gl_config.env_add = !!strstr(gl_extensions, "GL_EXT_texture_env_add");
	gl_config.nv_tex_env_combine4 = !!strstr(gl_extensions, "GL_NV_texture_env_combine4");

	gl_config.arb_texture_env_combine = !!strstr(gl_extensions, "GL_ARB_texture_env_combine");
	gl_config.arb_texture_env_dot3 = !!strstr(gl_extensions, "GL_ARB_texture_env_dot3");
	gl_config.arb_texture_cube_map = !!strstr(gl_extensions, "GL_ARB_texture_cube_map");

	if (gl_mtexarbable && gl_config.arb_texture_cube_map && gl_config.arb_texture_env_combine && gl_config.arb_texture_env_dot3 && !COM_CheckParm("-nobump") && gl_bump.value)
		gl_bumpmappingpossible = true;

	
	if (!!strstr(gl_extensions, "GL_ARB_fragment_program"))
	{
		gl_config.arb_fragment_program = true;
		qglProgramStringARB = (void *)getglext("glProgramStringARB");
		qglGetProgramivARB = (void *)getglext("glGetProgramivARB");
		qglBindProgramARB = (void *)getglext("glBindProgramARB");
		qglGenProgramsARB = (void *)getglext("glGenProgramsARB");
	}
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
	qglColor3f			= (void *)getglcore("glColor3f");
	qglColor3ub			= (void *)getglcore("glColor3ub");
	qglColor4f			= (void *)getglcore("glColor4f");
	qglColor4fv			= (void *)getglcore("glColor4fv");
	qglColor4ub			= (void *)getglcore("glColor4ub");
	qglColor4ubv		= (void *)getglcore("glColor4ubv");
	qglColorMask		= (void *)getglcore("glColorMask");
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
	qglTexImage2D		= (void *)getglcore("glTexImage2D");
	qglTexParameteri	= (void *)getglcore("glTexParameteri");
	qglTexParameterf	= (void *)getglcore("glTexParameterf");
	qglTexSubImage2D	= (void *)getglcore("glTexSubImage2D");
	qglTranslatef		= (void *)getglcore("glTranslatef");
	qglVertex2f			= (void *)getglcore("glVertex2f");
	qglVertex3f			= (void *)getglcore("glVertex3f");
	qglVertex3fv		= (void *)getglcore("glVertex3fv");
	qglViewport			= (void *)getglcore("glViewport");

	qglGetError			= (void *)getglcore("glGetError");

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

	//fixme: definatly make non-core
	qglStencilOp		= (void *)getglcore("glStencilOp");
	qglStencilFunc		= (void *)getglcore("glStencilFunc");
	qglPushAttrib		= (void *)getglcore("glPushAttrib");
	qglPopAttrib		= (void *)getglcore("glPopAttrib");
	qglScissor			= (void *)getglcore("glScissor");




	gl_vendor = glGetString (GL_VENDOR);
	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString (GL_VERSION);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	Con_DPrintf ("GL_EXTENSIONS: %s\n", gl_extensions);

	GL_CheckExtensions (getglfunction);

	glClearColor (0,0,0,0);	//clear to black so that it looks a little nicer on start.
	glClear(GL_COLOR_BUFFER_BIT);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

unsigned int	d_8to24rgbtable[256];
