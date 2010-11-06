/*
Copyright (C) 1996-1997 Id Software, Inc.

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

#ifndef GLQUAKE_H
#define GLQUAKE_H

// disable data conversion warnings
#ifdef MSVCDISABLEWARNINGS
#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs);
qboolean BoundsIntersect (vec3_t mins1, vec3_t maxs1, vec3_t mins2, vec3_t maxs2);
void ClearBounds (vec3_t mins, vec3_t maxs);

#ifdef GLQUAKE
	#ifdef __MACOSX__
		//apple, you suck.
		#include <AGL/agl.h>
	#else
		#include <GL/gl.h>
	#endif
//#include <GL/glu.h>
#include "glsupp.h"

void GL_InitFogTexture(void);

void GL_BeginRendering (void);
void GL_EndRendering (void);

void GLR_NetGraph (void);
void GLR_FrameTimeGraph (int frametime);
void GL_FlushSkinCache(void);
void GL_GAliasFlushSkinCache(void);

// Function prototypes for the Texture Object Extension routines
typedef GLboolean (APIENTRY *ARETEXRESFUNCPTR)(GLsizei, const GLuint *,
                    const GLboolean *);
typedef void (APIENTRY *BINDTEXFUNCPTR)(GLenum, GLuint);
typedef void (APIENTRY *DELTEXFUNCPTR)(GLsizei, const GLuint *);
typedef void (APIENTRY *GENTEXFUNCPTR)(GLsizei, GLuint *);
typedef GLboolean (APIENTRY *ISTEXFUNCPTR)(GLuint);
typedef void (APIENTRY *PRIORTEXFUNCPTR)(GLsizei, const GLuint *,
                    const GLclampf *);
typedef void (APIENTRY *TEXSUBIMAGEPTR)(int, int, int, int, int, int, int, int, void *);
typedef void (APIENTRY *FTEPFNGLCOMPRESSEDTEXIMAGE2DARBPROC)	(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data);
typedef void (APIENTRY *FTEPFNGLGETCOMPRESSEDTEXIMAGEARBPROC)	(GLenum target, GLint lod, const GLvoid* img);
typedef void (APIENTRY *FTEPFNGLPNTRIANGLESIATIPROC)(GLenum pname, GLint param);
typedef void (APIENTRY *FTEPFNGLPNTRIANGLESFATIPROC)(GLenum pname, GLfloat param);
typedef void (APIENTRY *FTEPFNGLACTIVESTENCILFACEEXTPROC) (GLenum face);

typedef GLhandleARB	(APIENTRYP FTEPFNGLCREATEPROGRAMOBJECTARBPROC)	(void);
typedef void		(APIENTRYP FTEPFNGLDELETEOBJECTARBPROC)		(GLhandleARB obj);
typedef void		(APIENTRYP FTEPFNGLUSEPROGRAMOBJECTARBPROC)	(GLhandleARB programObj);
typedef GLhandleARB	(APIENTRYP FTEPFNGLCREATESHADEROBJECTARBPROC)	(GLenum shaderType);
typedef void		(APIENTRYP FTEPFNGLSHADERSOURCEARBPROC)		(GLhandleARB shaderObj, GLsizei count, const GLcharARB* *string, const GLint *length);
typedef void		(APIENTRYP FTEPFNGLCOMPILESHADERARBPROC)		(GLhandleARB shaderObj);
typedef void        (APIENTRYP FTEPFNGLGETOBJECTPARAMETERIVARBPROC) (GLhandleARB obj, GLenum pname, GLint *params);
typedef void		(APIENTRYP FTEPFNGLATTACHOBJECTARBPROC)		(GLhandleARB containerObj, GLhandleARB obj);
typedef void		(APIENTRYP FTEPFNGLGETINFOLOGARBPROC)			(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
typedef void		(APIENTRYP FTEPFNGLLINKPROGRAMARBPROC)			(GLhandleARB programObj);
typedef GLint		(APIENTRYP FTEPFNGLGETUNIFORMLOCATIONARBPROC)	(GLhandleARB programObj, const GLcharARB *name);
typedef void		(APIENTRYP FTEPFNGLUNIFORM4FARBPROC)			(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void		(APIENTRYP FTEPFNGLUNIFORMMATRIX4FVARBPROC)		(GLint location, GLsizei count, GLboolean transpose, GLfloat *value);
typedef void		(APIENTRYP FTEPFNGLUNIFORM4FVARBPROC)			(GLint location, GLsizei count, GLfloat *value);
typedef void		(APIENTRYP FTEPFNGLUNIFORM3FARBPROC)			(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void		(APIENTRYP FTEPFNGLUNIFORM3FVARBPROC)			(GLint location, GLsizei count, GLfloat *value);
typedef void		(APIENTRYP FTEPFNGLUNIFORM1IARBPROC)			(GLint location, GLint v0);
typedef void		(APIENTRYP FTEPFNGLUNIFORM1FARBPROC)			(GLint location, GLfloat v0);

typedef void (APIENTRY * FTEPFNGLLOCKARRAYSEXTPROC) (GLint first, GLsizei count);
typedef void (APIENTRY * FTEPFNGLUNLOCKARRAYSEXTPROC) (void);

extern	BINDTEXFUNCPTR bindTexFunc;
extern	DELTEXFUNCPTR delTexFunc;
extern	TEXSUBIMAGEPTR TexSubImage2DFunc;
extern void (APIENTRY *qglStencilOpSeparateATI) (GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
extern FTEPFNGLCOMPRESSEDTEXIMAGE2DARBPROC qglCompressedTexImage2DARB;
extern FTEPFNGLGETCOMPRESSEDTEXIMAGEARBPROC qglGetCompressedTexImageARB;
extern	FTEPFNGLPNTRIANGLESIATIPROC qglPNTrianglesiATI;
extern	FTEPFNGLPNTRIANGLESFATIPROC qglPNTrianglesfATI;

qboolean GL_CheckExtension(char *extname);

typedef struct {
	qboolean tex_env_combine;
	qboolean nv_tex_env_combine4;
	qboolean env_add;

	qboolean arb_texture_non_power_of_two;
	qboolean sgis_generate_mipmap;

	qboolean arb_texture_env_combine;
	qboolean arb_texture_env_dot3;
	qboolean arb_texture_cube_map;

	qboolean arb_texture_compression;
//	qboolean arb_fragment_program;
	qboolean arb_shader_objects;
	qboolean ext_framebuffer_objects;
	qboolean ext_stencil_wrap;
	int ext_texture_filter_anisotropic;
	int maxtmus;	//max texture units
} gl_config_t;

extern gl_config_t gl_config;

extern	float	gldepthmin, gldepthmax;

/*
void GL_Upload32 (char *name, unsigned *data, int width, int height, unsigned int flags);	//name was added for texture compression output
void GL_Upload32_BGRA (char *name, unsigned *data, int width, int height, unsigned int flags);	//name was added for texture compression output
void GL_Upload8 (char *name, qbyte *data, int width, int height, unsigned int flags, unsigned int alphatype);
void GL_Upload24BGR_Flip (char *name, qbyte *data, int width, int height, unsigned int flags);
void GL_Upload24BGR (char *name, qbyte *data, int width, int height, unsigned int flags);
#ifdef GL_EXT_paletted_texture
void GL_Upload8_EXT (qbyte *data, int width, int height,  qboolean mipmap, qboolean alpha);
#endif
*/
texid_t GL_LoadTexture (char *identifier, int width, int height, qbyte *data, unsigned int flags, unsigned int transtype);
texid_t GL_LoadTexture8Bump (char *identifier, int width, int height, unsigned char *data, unsigned int flags, float bumpscale);
texid_t GL_LoadTexture8Pal24 (char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags);
texid_t GL_LoadTexture8Pal32 (char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags);
texid_t GL_LoadTexture32 (char *identifier, int width, int height, void *data, unsigned int flags);
texid_t GL_LoadCompressed(char *name);
texid_t GL_FindTexture (char *identifier);

texid_t GL_LoadTextureFB (char *identifier, int width, int height, qbyte *data, unsigned int flags);
void GL_Upload8Pal24 (qbyte *data, qbyte *pal, int width, int height, unsigned int flags);
/*
typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

FTE_DEPRECATED extern glvert_t glv;
*/
#endif

// r_local.h -- private refresh defs

//#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
//#define	MAX_LBM_HEIGHT		480

//#define TILE_SIZE		128		// size of textures generated by R_GenTiledSurf

//#define SKYSHIFT		7
//#define	SKYSIZE			(1 << SKYSHIFT)
//#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01


void R_TimeRefresh_f (void);

#include "particles.h"

//====================================================

extern	entity_t	r_worldentity;
extern	vec3_t		r_entorigin;
extern	entity_t	*currententity;
extern	int			r_visframecount;	// ??? what difs?
extern	int			r_framecount;
extern	mplane_t	frustum[4];

extern float r_wateralphaval;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;	//q2
extern	texture_t	*r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	texid_t	netgraphtexture;	// netgraph texture

extern	int			mirrortexturenum;	// quake texturenum, not gltexturenum
extern	qboolean	mirror;
extern	mplane_t	*mirror_plane;

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;

FTE_DEPRECATED void PPL_RevertToKnownState(void);

qboolean R_CullBox (vec3_t mins, vec3_t maxs);
qboolean R_CullEntityBox(entity_t *e, vec3_t modmins, vec3_t modmaxs);
qboolean R_CullSphere (vec3_t origin, float radius);

#ifdef GLQUAKE
void R_TranslatePlayerSkin (int playernum);
void GL_Bind (texid_t texnum);
void GL_MBind(int tmunum, texid_t texnum);
void GL_CullFace(unsigned int sflags);
void GL_TexEnv(GLenum mode);
void GL_BindType (int type, texid_t texnum);
void GL_FlushBackEnd (void);

// Multitexture
#define    GL_TEXTURE0_SGIS				0x835E
#define    GL_TEXTURE1_SGIS				0x835F

typedef void (APIENTRY *lpMTexFUNC) (GLenum en, GLfloat f1, GLfloat f2);
typedef void (APIENTRY *lpSelTexFUNC) (GLenum en);
extern lpMTexFUNC qglMTexCoord2fSGIS;
extern lpSelTexFUNC qglSelectTextureSGIS;

extern	int gl_canstencil;
extern FTEPFNGLACTIVESTENCILFACEEXTPROC qglActiveStencilFaceEXT;

extern lpMTexFUNC qglMTexCoord2fSGIS;
extern lpSelTexFUNC qglSelectTextureSGIS;


extern int gl_mtexarbable;	//max texture units
extern qboolean gl_mtexable;

extern int mtexid0;
extern int mtexid1;

extern qboolean gl_mtexable;

void GL_SelectTexture (int tmunum);
void GL_SetShaderState2D(qboolean is2d);
void GL_ForceDepthWritable(void);

void R_DrawRailCore(entity_t *e);
void R_DrawLightning(entity_t *e);
void R_DrawBeam( entity_t *e );

#endif

//
// vid_gl*.c
//
#ifdef GLQUAKE
void GL_DoSwap (void);
#endif

//
// gl_backend.c
//
#ifdef GLQUAKE
void FTE_DEPRECATED R_BackendInit(void);
void FTE_DEPRECATED R_IBrokeTheArrays(void);
#endif



//
// gl_warp.c
//
void R_DrawSkyChain (batch_t *batch); /*called from the backend, and calls back into it*/
texnums_t R_InitSky (texture_t *mt, qbyte *src); /*generate q1 sky texnums*/

//
// gl_draw.c
//
#ifdef GLQUAKE
texid_t GL_LoadPicTexture (qpic_t *pic);
void GL_Set2D (void);
#endif

//
// gl_rmain.c
//
qboolean R_ShouldDraw(entity_t *e);
#ifdef GLQUAKE
void R_RotateForEntity (const entity_t *e, const model_t *mod);

void GL_InitSceneProcessingShaders (void);
void GL_SetupSceneProcessingTextures (void);
#endif

//
// gl_alias.c
//
#ifdef GLQUAKE
void R_DrawGAliasShadowVolume(entity_t *e, vec3_t lightpos, float radius);

//misc model formats
void R_DrawHLModel(entity_t	*curent);

//typedef float m3by3_t[3][3];
//int GetTag(model_t *mod, char *tagname, int frame, float **org, m3by3_t **ang);
#endif

//
// gl_rlight.c
//
#ifdef GLQUAKE
void GLR_MarkLights (dlight_t *light, int bit, mnode_t *node);
void GLR_MarkQ2Lights (dlight_t *light, int bit, mnode_t *node);
void GLR_RenderDlights (void);
void R_InitFlashblends (void);
int GLR_LightPoint (vec3_t p);
#endif
void GLQ3_LightGrid(model_t *mod, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir);

//
// gl_heightmap.c
//
#ifdef GLQUAKE
void GL_DrawHeightmapModel (entity_t *e);
qboolean GL_LoadHeightmapModel (model_t *mod, void *buffer);
#endif

//doom
#ifdef MAP_DOOM
void GLR_DoomWorld();
#endif

//gl_bloom.c
#ifdef GLQUAKE
void R_BloomRegister(void);
void R_BloomBlend(void);
void R_InitBloomTextures(void);
#endif

//
// gl_rsurf.c
//
#ifdef GLQUAKE
void GL_LoadShaders(void);
#endif


//
// gl_ngraph.c
//
void R_NetGraph (void);


#if defined(GLQUAKE)

extern void (APIENTRY *qglAccum) (GLenum op, GLfloat value);
extern void (APIENTRY *qglAlphaFunc) (GLenum func, GLclampf ref);
extern GLboolean (APIENTRY *qglAreTexturesResident) (GLsizei n, const GLuint *textures, GLboolean *residences);
extern void (APIENTRY *qglArrayElement) (GLint i);
extern void (APIENTRY *qglBegin) (GLenum mode);
extern void (APIENTRY *qglBindTexture) (GLenum target, GLuint texture);
extern void (APIENTRY *qglBitmap) (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
extern void (APIENTRY *qglBlendFunc) (GLenum sfactor, GLenum dfactor);
extern void (APIENTRY *qglCallList) (GLuint list);
extern void (APIENTRY *qglCallLists) (GLsizei n, GLenum type, const GLvoid *lists);
extern void (APIENTRY *qglClear) (GLbitfield mask);
extern void (APIENTRY *qglClearAccum) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void (APIENTRY *qglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
extern void (APIENTRY *qglClearDepth) (GLclampd depth);
extern void (APIENTRY *qglClearIndex) (GLfloat c);
extern void (APIENTRY *qglClearStencil) (GLint s);
extern void (APIENTRY *qglClipPlane) (GLenum plane, const GLdouble *equation);
extern void (APIENTRY *qglColor3b) (GLbyte red, GLbyte green, GLbyte blue);
extern void (APIENTRY *qglColor3bv) (const GLbyte *v);
extern void (APIENTRY *qglColor3d) (GLdouble red, GLdouble green, GLdouble blue);
extern void (APIENTRY *qglColor3dv) (const GLdouble *v);
extern void (APIENTRY *qglColor3f) (GLfloat red, GLfloat green, GLfloat blue);
extern void (APIENTRY *qglColor3fv) (const GLfloat *v);
extern void (APIENTRY *qglColor3i) (GLint red, GLint green, GLint blue);
extern void (APIENTRY *qglColor3iv) (const GLint *v);
extern void (APIENTRY *qglColor3s) (GLshort red, GLshort green, GLshort blue);
extern void (APIENTRY *qglColor3sv) (const GLshort *v);
extern void (APIENTRY *qglColor3ub) (GLubyte red, GLubyte green, GLubyte blue);
extern void (APIENTRY *qglColor3ubv) (const GLubyte *v);
extern void (APIENTRY *qglColor3ui) (GLuint red, GLuint green, GLuint blue);
extern void (APIENTRY *qglColor3uiv) (const GLuint *v);
extern void (APIENTRY *qglColor3us) (GLushort red, GLushort green, GLushort blue);
extern void (APIENTRY *qglColor3usv) (const GLushort *v);
extern void (APIENTRY *qglColor4b) (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
extern void (APIENTRY *qglColor4bv) (const GLbyte *v);
extern void (APIENTRY *qglColor4d) (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
extern void (APIENTRY *qglColor4dv) (const GLdouble *v);
extern void (APIENTRY *qglColor4f) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void (APIENTRY *qglColor4fv) (const GLfloat *v);
extern void (APIENTRY *qglColor4i) (GLint red, GLint green, GLint blue, GLint alpha);
extern void (APIENTRY *qglColor4iv) (const GLint *v);
extern void (APIENTRY *qglColor4s) (GLshort red, GLshort green, GLshort blue, GLshort alpha);
extern void (APIENTRY *qglColor4sv) (const GLshort *v);
extern void (APIENTRY *qglColor4ub) (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
extern void (APIENTRY *qglColor4ubv) (const GLubyte *v);
extern void (APIENTRY *qglColor4ui) (GLuint red, GLuint green, GLuint blue, GLuint alpha);
extern void (APIENTRY *qglColor4uiv) (const GLuint *v);
extern void (APIENTRY *qglColor4us) (GLushort red, GLushort green, GLushort blue, GLushort alpha);
extern void (APIENTRY *qglColor4usv) (const GLushort *v);
extern void (APIENTRY *qglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
extern void (APIENTRY *qglColorMaterial) (GLenum face, GLenum mode);
extern void (APIENTRY *qglColorPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern void (APIENTRY *qglCopyPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
extern void (APIENTRY *qglCopyTexImage1D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
extern void (APIENTRY *qglCopyTexImage2D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
extern void (APIENTRY *qglCopyTexSubImage1D) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
extern void (APIENTRY *qglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
extern void (APIENTRY *qglCullFace) (GLenum mode);
extern void (APIENTRY *qglDeleteLists) (GLuint list, GLsizei range);
extern void (APIENTRY *qglDeleteTextures) (GLsizei n, const GLuint *textures);
extern void (APIENTRY *qglDepthFunc) (GLenum func);
extern void (APIENTRY *qglDepthMask) (GLboolean flag);
extern void (APIENTRY *qglDepthRange) (GLclampd zNear, GLclampd zFar);
extern void (APIENTRY *qglDisable) (GLenum cap);
extern void (APIENTRY *qglDisableClientState) (GLenum array);
extern void (APIENTRY *qglDrawArrays) (GLenum mode, GLint first, GLsizei count);
extern void (APIENTRY *qglDrawBuffer) (GLenum mode);
extern void (APIENTRY *qglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
extern void (APIENTRY *qglDrawPixels) (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
extern void (APIENTRY *qglEdgeFlag) (GLboolean flag);
extern void (APIENTRY *qglEdgeFlagPointer) (GLsizei stride, const GLvoid *pointer);
extern void (APIENTRY *qglEdgeFlagv) (const GLboolean *flag);
extern void (APIENTRY *qglEnable) (GLenum cap);
extern void (APIENTRY *qglEnableClientState) (GLenum array);
extern void (APIENTRY *qglEnd) (void);
extern void (APIENTRY *qglEndList) (void);
extern void (APIENTRY *qglEvalCoord1d) (GLdouble u);
extern void (APIENTRY *qglEvalCoord1dv) (const GLdouble *u);
extern void (APIENTRY *qglEvalCoord1f) (GLfloat u);
extern void (APIENTRY *qglEvalCoord1fv) (const GLfloat *u);
extern void (APIENTRY *qglEvalCoord2d) (GLdouble u, GLdouble v);
extern void (APIENTRY *qglEvalCoord2dv) (const GLdouble *u);
extern void (APIENTRY *qglEvalCoord2f) (GLfloat u, GLfloat v);
extern void (APIENTRY *qglEvalCoord2fv) (const GLfloat *u);
extern void (APIENTRY *qglEvalMesh1) (GLenum mode, GLint i1, GLint i2);
extern void (APIENTRY *qglEvalMesh2) (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
extern void (APIENTRY *qglEvalPoint1) (GLint i);
extern void (APIENTRY *qglEvalPoint2) (GLint i, GLint j);
extern void (APIENTRY *qglFeedbackBuffer) (GLsizei size, GLenum type, GLfloat *buffer);
extern void (APIENTRY *qglFinish) (void);
extern void (APIENTRY *qglFlush) (void);
extern void (APIENTRY *qglFogf) (GLenum pname, GLfloat param);
extern void (APIENTRY *qglFogfv) (GLenum pname, const GLfloat *params);
extern void (APIENTRY *qglFogi) (GLenum pname, GLint param);
extern void (APIENTRY *qglFogiv) (GLenum pname, const GLint *params);
extern void (APIENTRY *qglFrontFace) (GLenum mode);
extern void (APIENTRY *qglFrustum) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
extern GLuint (APIENTRY *qglGenLists) (GLsizei range);
extern void (APIENTRY *qglGenTextures) (GLsizei n, GLuint *textures);
extern void (APIENTRY *qglGetBooleanv) (GLenum pname, GLboolean *params);
extern void (APIENTRY *qglGetClipPlane) (GLenum plane, GLdouble *equation);
extern void (APIENTRY *qglGetDoublev) (GLenum pname, GLdouble *params);
extern GLenum (APIENTRY *qglGetError) (void);
extern void (APIENTRY *qglGetFloatv) (GLenum pname, GLfloat *params);
extern void (APIENTRY *qglGetIntegerv) (GLenum pname, GLint *params);
extern void (APIENTRY *qglGetLightfv) (GLenum light, GLenum pname, GLfloat *params);
extern void (APIENTRY *qglGetLightiv) (GLenum light, GLenum pname, GLint *params);
extern void (APIENTRY *qglGetMapdv) (GLenum target, GLenum query, GLdouble *v);
extern void (APIENTRY *qglGetMapfv) (GLenum target, GLenum query, GLfloat *v);
extern void (APIENTRY *qglGetMapiv) (GLenum target, GLenum query, GLint *v);
extern void (APIENTRY *qglGetMaterialfv) (GLenum face, GLenum pname, GLfloat *params);
extern void (APIENTRY *qglGetMaterialiv) (GLenum face, GLenum pname, GLint *params);
extern void (APIENTRY *qglGetPixelMapfv) (GLenum map, GLfloat *values);
extern void (APIENTRY *qglGetPixelMapuiv) (GLenum map, GLuint *values);
extern void (APIENTRY *qglGetPixelMapusv) (GLenum map, GLushort *values);
extern void (APIENTRY *qglGetPointerv) (GLenum pname, GLvoid* *params);
extern void (APIENTRY *qglGetPolygonStipple) (GLubyte *mask);
extern const GLubyte * (APIENTRY *qglGetString) (GLenum name);
extern void (APIENTRY *qglGetTexEnvfv) (GLenum target, GLenum pname, GLfloat *params);
extern void (APIENTRY *qglGetTexEnviv) (GLenum target, GLenum pname, GLint *params);
extern void (APIENTRY *qglGetTexGendv) (GLenum coord, GLenum pname, GLdouble *params);
extern void (APIENTRY *qglGetTexGenfv) (GLenum coord, GLenum pname, GLfloat *params);
extern void (APIENTRY *qglGetTexGeniv) (GLenum coord, GLenum pname, GLint *params);
extern void (APIENTRY *qglGetTexImage) (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
extern void (APIENTRY *qglGetTexLevelParameterfv) (GLenum target, GLint level, GLenum pname, GLfloat *params);
extern void (APIENTRY *qglGetTexLevelParameteriv) (GLenum target, GLint level, GLenum pname, GLint *params);
extern void (APIENTRY *qglGetTexParameterfv) (GLenum target, GLenum pname, GLfloat *params);
extern void (APIENTRY *qglGetTexParameteriv) (GLenum target, GLenum pname, GLint *params);
extern void (APIENTRY *qglHint) (GLenum target, GLenum mode);
extern void (APIENTRY *qglIndexMask) (GLuint mask);
extern void (APIENTRY *qglIndexPointer) (GLenum type, GLsizei stride, const GLvoid *pointer);
extern void (APIENTRY *qglIndexd) (GLdouble c);
extern void (APIENTRY *qglIndexdv) (const GLdouble *c);
extern void (APIENTRY *qglIndexf) (GLfloat c);
extern void (APIENTRY *qglIndexfv) (const GLfloat *c);
extern void (APIENTRY *qglIndexi) (GLint c);
extern void (APIENTRY *qglIndexiv) (const GLint *c);
extern void (APIENTRY *qglIndexs) (GLshort c);
extern void (APIENTRY *qglIndexsv) (const GLshort *c);
extern void (APIENTRY *qglIndexub) (GLubyte c);
extern void (APIENTRY *qglIndexubv) (const GLubyte *c);
extern void (APIENTRY *qglInitNames) (void);
extern void (APIENTRY *qglInterleavedArrays) (GLenum format, GLsizei stride, const GLvoid *pointer);
extern GLboolean (APIENTRY *qglIsEnabled) (GLenum cap);
extern GLboolean (APIENTRY *qglIsList) (GLuint list);
extern GLboolean (APIENTRY *qglIsTexture) (GLuint texture);
extern void (APIENTRY *qglLightModelf) (GLenum pname, GLfloat param);
extern void (APIENTRY *qglLightModelfv) (GLenum pname, const GLfloat *params);
extern void (APIENTRY *qglLightModeli) (GLenum pname, GLint param);
extern void (APIENTRY *qglLightModeliv) (GLenum pname, const GLint *params);
extern void (APIENTRY *qglLightf) (GLenum light, GLenum pname, GLfloat param);
extern void (APIENTRY *qglLightfv) (GLenum light, GLenum pname, const GLfloat *params);
extern void (APIENTRY *qglLighti) (GLenum light, GLenum pname, GLint param);
extern void (APIENTRY *qglLightiv) (GLenum light, GLenum pname, const GLint *params);
extern void (APIENTRY *qglLineStipple) (GLint factor, GLushort pattern);
extern void (APIENTRY *qglLineWidth) (GLfloat width);
extern void (APIENTRY *qglListBase) (GLuint base);
extern void (APIENTRY *qglLoadIdentity) (void);
extern void (APIENTRY *qglLoadMatrixd) (const GLdouble *m);
extern void (APIENTRY *qglLoadMatrixf) (const GLfloat *m);
extern void (APIENTRY *qglLoadName) (GLuint name);
extern void (APIENTRY *qglLogicOp) (GLenum opcode);
extern void (APIENTRY *qglMap1d) (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
extern void (APIENTRY *qglMap1f) (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
extern void (APIENTRY *qglMap2d) (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
extern void (APIENTRY *qglMap2f) (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
extern void (APIENTRY *qglMapGrid1d) (GLint un, GLdouble u1, GLdouble u2);
extern void (APIENTRY *qglMapGrid1f) (GLint un, GLfloat u1, GLfloat u2);
extern void (APIENTRY *qglMapGrid2d) (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
extern void (APIENTRY *qglMapGrid2f) (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
extern void (APIENTRY *qglMaterialf) (GLenum face, GLenum pname, GLfloat param);
extern void (APIENTRY *qglMaterialfv) (GLenum face, GLenum pname, const GLfloat *params);
extern void (APIENTRY *qglMateriali) (GLenum face, GLenum pname, GLint param);
extern void (APIENTRY *qglMaterialiv) (GLenum face, GLenum pname, const GLint *params);
extern void (APIENTRY *qglMatrixMode) (GLenum mode);
extern void (APIENTRY *qglMultMatrixd) (const GLdouble *m);
extern void (APIENTRY *qglMultMatrixf) (const GLfloat *m);
extern void (APIENTRY *qglNewList) (GLuint list, GLenum mode);
extern void (APIENTRY *qglNormal3b) (GLbyte nx, GLbyte ny, GLbyte nz);
extern void (APIENTRY *qglNormal3bv) (const GLbyte *v);
extern void (APIENTRY *qglNormal3d) (GLdouble nx, GLdouble ny, GLdouble nz);
extern void (APIENTRY *qglNormal3dv) (const GLdouble *v);
extern void (APIENTRY *qglNormal3f) (GLfloat nx, GLfloat ny, GLfloat nz);
extern void (APIENTRY *qglNormal3fv) (const GLfloat *v);
extern void (APIENTRY *qglNormal3i) (GLint nx, GLint ny, GLint nz);
extern void (APIENTRY *qglNormal3iv) (const GLint *v);
extern void (APIENTRY *qglNormal3s) (GLshort nx, GLshort ny, GLshort nz);
extern void (APIENTRY *qglNormal3sv) (const GLshort *v);
extern void (APIENTRY *qglNormalPointer) (GLenum type, GLsizei stride, const GLvoid *pointer);
extern void (APIENTRY *qglOrtho) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
extern void (APIENTRY *qglPassThrough) (GLfloat token);
extern void (APIENTRY *qglPixelMapfv) (GLenum map, GLsizei mapsize, const GLfloat *values);
extern void (APIENTRY *qglPixelMapuiv) (GLenum map, GLsizei mapsize, const GLuint *values);
extern void (APIENTRY *qglPixelMapusv) (GLenum map, GLsizei mapsize, const GLushort *values);
extern void (APIENTRY *qglPixelStoref) (GLenum pname, GLfloat param);
extern void (APIENTRY *qglPixelStorei) (GLenum pname, GLint param);
extern void (APIENTRY *qglPixelTransferf) (GLenum pname, GLfloat param);
extern void (APIENTRY *qglPixelTransferi) (GLenum pname, GLint param);
extern void (APIENTRY *qglPixelZoom) (GLfloat xfactor, GLfloat yfactor);
extern void (APIENTRY *qglPointSize) (GLfloat size);
extern void (APIENTRY *qglPolygonMode) (GLenum face, GLenum mode);
extern void (APIENTRY *qglPolygonOffset) (GLfloat factor, GLfloat units);
extern void (APIENTRY *qglPolygonStipple) (const GLubyte *mask);
extern void (APIENTRY *qglPopAttrib) (void);
extern void (APIENTRY *qglPopClientAttrib) (void);
extern void (APIENTRY *qglPopMatrix) (void);
extern void (APIENTRY *qglPopName) (void);
extern void (APIENTRY *qglPrioritizeTextures) (GLsizei n, const GLuint *textures, const GLclampf *priorities);
extern void (APIENTRY *qglPushAttrib) (GLbitfield mask);
extern void (APIENTRY *qglPushClientAttrib) (GLbitfield mask);
extern void (APIENTRY *qglPushMatrix) (void);
extern void (APIENTRY *qglPushName) (GLuint name);
extern void (APIENTRY *qglRasterPos2d) (GLdouble x, GLdouble y);
extern void (APIENTRY *qglRasterPos2dv) (const GLdouble *v);
extern void (APIENTRY *qglRasterPos2f) (GLfloat x, GLfloat y);
extern void (APIENTRY *qglRasterPos2fv) (const GLfloat *v);
extern void (APIENTRY *qglRasterPos2i) (GLint x, GLint y);
extern void (APIENTRY *qglRasterPos2iv) (const GLint *v);
extern void (APIENTRY *qglRasterPos2s) (GLshort x, GLshort y);
extern void (APIENTRY *qglRasterPos2sv) (const GLshort *v);
extern void (APIENTRY *qglRasterPos3d) (GLdouble x, GLdouble y, GLdouble z);
extern void (APIENTRY *qglRasterPos3dv) (const GLdouble *v);
extern void (APIENTRY *qglRasterPos3f) (GLfloat x, GLfloat y, GLfloat z);
extern void (APIENTRY *qglRasterPos3fv) (const GLfloat *v);
extern void (APIENTRY *qglRasterPos3i) (GLint x, GLint y, GLint z);
extern void (APIENTRY *qglRasterPos3iv) (const GLint *v);
extern void (APIENTRY *qglRasterPos3s) (GLshort x, GLshort y, GLshort z);
extern void (APIENTRY *qglRasterPos3sv) (const GLshort *v);
extern void (APIENTRY *qglRasterPos4d) (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
extern void (APIENTRY *qglRasterPos4dv) (const GLdouble *v);
extern void (APIENTRY *qglRasterPos4f) (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
extern void (APIENTRY *qglRasterPos4fv) (const GLfloat *v);
extern void (APIENTRY *qglRasterPos4i) (GLint x, GLint y, GLint z, GLint w);
extern void (APIENTRY *qglRasterPos4iv) (const GLint *v);
extern void (APIENTRY *qglRasterPos4s) (GLshort x, GLshort y, GLshort z, GLshort w);
extern void (APIENTRY *qglRasterPos4sv) (const GLshort *v);
extern void (APIENTRY *qglReadBuffer) (GLenum mode);
extern void (APIENTRY *qglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
extern void (APIENTRY *qglRectd) (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
extern void (APIENTRY *qglRectdv) (const GLdouble *v1, const GLdouble *v2);
extern void (APIENTRY *qglRectf) (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
extern void (APIENTRY *qglRectfv) (const GLfloat *v1, const GLfloat *v2);
extern void (APIENTRY *qglRecti) (GLint x1, GLint y1, GLint x2, GLint y2);
extern void (APIENTRY *qglRectiv) (const GLint *v1, const GLint *v2);
extern void (APIENTRY *qglRects) (GLshort x1, GLshort y1, GLshort x2, GLshort y2);
extern void (APIENTRY *qglRectsv) (const GLshort *v1, const GLshort *v2);
extern GLint (APIENTRY *qglRenderMode) (GLenum mode);
extern void (APIENTRY *qglRotated) (GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
extern void (APIENTRY *qglRotatef) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
extern void (APIENTRY *qglScaled) (GLdouble x, GLdouble y, GLdouble z);
extern void (APIENTRY *qglScalef) (GLfloat x, GLfloat y, GLfloat z);
extern void (APIENTRY *qglScissor) (GLint x, GLint y, GLsizei width, GLsizei height);
extern void (APIENTRY *qglSelectBuffer) (GLsizei size, GLuint *buffer);
extern void (APIENTRY *qglShadeModel) (GLenum mode);
extern void (APIENTRY *qglStencilFunc) (GLenum func, GLint ref, GLuint mask);
extern void (APIENTRY *qglStencilMask) (GLuint mask);
extern void (APIENTRY *qglStencilOp) (GLenum fail, GLenum zfail, GLenum zpass);
extern void (APIENTRY *qglTexCoord1d) (GLdouble s);
extern void (APIENTRY *qglTexCoord1dv) (const GLdouble *v);
extern void (APIENTRY *qglTexCoord1f) (GLfloat s);
extern void (APIENTRY *qglTexCoord1fv) (const GLfloat *v);
extern void (APIENTRY *qglTexCoord1i) (GLint s);
extern void (APIENTRY *qglTexCoord1iv) (const GLint *v);
extern void (APIENTRY *qglTexCoord1s) (GLshort s);
extern void (APIENTRY *qglTexCoord1sv) (const GLshort *v);
extern void (APIENTRY *qglTexCoord2d) (GLdouble s, GLdouble t);
extern void (APIENTRY *qglTexCoord2dv) (const GLdouble *v);
extern void (APIENTRY *qglTexCoord2f) (GLfloat s, GLfloat t);
extern void (APIENTRY *qglTexCoord2fv) (const GLfloat *v);
extern void (APIENTRY *qglTexCoord2i) (GLint s, GLint t);
extern void (APIENTRY *qglTexCoord2iv) (const GLint *v);
extern void (APIENTRY *qglTexCoord2s) (GLshort s, GLshort t);
extern void (APIENTRY *qglTexCoord2sv) (const GLshort *v);
extern void (APIENTRY *qglTexCoord3d) (GLdouble s, GLdouble t, GLdouble r);
extern void (APIENTRY *qglTexCoord3dv) (const GLdouble *v);
extern void (APIENTRY *qglTexCoord3f) (GLfloat s, GLfloat t, GLfloat r);
extern void (APIENTRY *qglTexCoord3fv) (const GLfloat *v);
extern void (APIENTRY *qglTexCoord3i) (GLint s, GLint t, GLint r);
extern void (APIENTRY *qglTexCoord3iv) (const GLint *v);
extern void (APIENTRY *qglTexCoord3s) (GLshort s, GLshort t, GLshort r);
extern void (APIENTRY *qglTexCoord3sv) (const GLshort *v);
extern void (APIENTRY *qglTexCoord4d) (GLdouble s, GLdouble t, GLdouble r, GLdouble q);
extern void (APIENTRY *qglTexCoord4dv) (const GLdouble *v);
extern void (APIENTRY *qglTexCoord4f) (GLfloat s, GLfloat t, GLfloat r, GLfloat q);
extern void (APIENTRY *qglTexCoord4fv) (const GLfloat *v);
extern void (APIENTRY *qglTexCoord4i) (GLint s, GLint t, GLint r, GLint q);
extern void (APIENTRY *qglTexCoord4iv) (const GLint *v);
extern void (APIENTRY *qglTexCoord4s) (GLshort s, GLshort t, GLshort r, GLshort q);
extern void (APIENTRY *qglTexCoord4sv) (const GLshort *v);
extern void (APIENTRY *qglTexCoordPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern void (APIENTRY *qglTexEnvf) (GLenum target, GLenum pname, GLfloat param);
extern void (APIENTRY *qglTexEnvfv) (GLenum target, GLenum pname, const GLfloat *params);
extern void (APIENTRY *qglTexEnvi) (GLenum target, GLenum pname, GLint param);
extern void (APIENTRY *qglTexEnviv) (GLenum target, GLenum pname, const GLint *params);
extern void (APIENTRY *qglTexGend) (GLenum coord, GLenum pname, GLdouble param);
extern void (APIENTRY *qglTexGendv) (GLenum coord, GLenum pname, const GLdouble *params);
extern void (APIENTRY *qglTexGenf) (GLenum coord, GLenum pname, GLfloat param);
extern void (APIENTRY *qglTexGenfv) (GLenum coord, GLenum pname, const GLfloat *params);
extern void (APIENTRY *qglTexGeni) (GLenum coord, GLenum pname, GLint param);
extern void (APIENTRY *qglTexGeniv) (GLenum coord, GLenum pname, const GLint *params);
extern void (APIENTRY *qglTexImage1D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void (APIENTRY *qglTexImage2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void (APIENTRY *qglTexParameterf) (GLenum target, GLenum pname, GLfloat param);
extern void (APIENTRY *qglTexParameterfv) (GLenum target, GLenum pname, const GLfloat *params);
extern void (APIENTRY *qglTexParameteri) (GLenum target, GLenum pname, GLint param);
extern void (APIENTRY *qglTexParameteriv) (GLenum target, GLenum pname, const GLint *params);
extern void (APIENTRY *qglTexSubImage1D) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
extern void (APIENTRY *qglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
extern void (APIENTRY *qglTranslated) (GLdouble x, GLdouble y, GLdouble z);
extern void (APIENTRY *qglTranslatef) (GLfloat x, GLfloat y, GLfloat z);
extern void (APIENTRY *qglVertex2d) (GLdouble x, GLdouble y);
extern void (APIENTRY *qglVertex2dv) (const GLdouble *v);
extern void (APIENTRY *qglVertex2f) (GLfloat x, GLfloat y);
extern void (APIENTRY *qglVertex2fv) (const GLfloat *v);
extern void (APIENTRY *qglVertex2i) (GLint x, GLint y);
extern void (APIENTRY *qglVertex2iv) (const GLint *v);
extern void (APIENTRY *qglVertex2s) (GLshort x, GLshort y);
extern void (APIENTRY *qglVertex2sv) (const GLshort *v);
extern void (APIENTRY *qglVertex3d) (GLdouble x, GLdouble y, GLdouble z);
extern void (APIENTRY *qglVertex3dv) (const GLdouble *v);
extern void (APIENTRY *qglVertex3f) (GLfloat x, GLfloat y, GLfloat z);
extern void (APIENTRY *qglVertex3fv) (const GLfloat *v);
extern void (APIENTRY *qglVertex3i) (GLint x, GLint y, GLint z);
extern void (APIENTRY *qglVertex3iv) (const GLint *v);
extern void (APIENTRY *qglVertex3s) (GLshort x, GLshort y, GLshort z);
extern void (APIENTRY *qglVertex3sv) (const GLshort *v);
extern void (APIENTRY *qglVertex4d) (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
extern void (APIENTRY *qglVertex4dv) (const GLdouble *v);
extern void (APIENTRY *qglVertex4f) (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
extern void (APIENTRY *qglVertex4fv) (const GLfloat *v);
extern void (APIENTRY *qglVertex4i) (GLint x, GLint y, GLint z, GLint w);
extern void (APIENTRY *qglVertex4iv) (const GLint *v);
extern void (APIENTRY *qglVertex4s) (GLshort x, GLshort y, GLshort z, GLshort w);
extern void (APIENTRY *qglVertex4sv) (const GLshort *v);
extern void (APIENTRY *qglVertexPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern void (APIENTRY *qglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);

#ifdef _WIN32
extern BOOL  (WINAPI *qwglCopyContext)(HGLRC, HGLRC, UINT);
extern HGLRC (WINAPI *qwglCreateContext)(HDC);
extern HGLRC (WINAPI *qwglCreateLayerContext)(HDC, int);
extern BOOL  (WINAPI *qwglDeleteContext)(HGLRC);
extern HGLRC (WINAPI *qwglGetCurrentContext)(VOID);
extern HDC   (WINAPI *qwglGetCurrentDC)(VOID);
extern PROC  (WINAPI *qwglGetProcAddress)(LPCSTR);
extern BOOL  (WINAPI *qwglMakeCurrent)(HDC, HGLRC);
extern BOOL  (WINAPI *qSwapBuffers)(HDC);
#endif

extern void (APIENTRY *qglDrawRangeElements) (GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *);

extern void (APIENTRY *qglGenBuffersARB)(GLsizei n, GLuint* ids);
extern void (APIENTRY *qglDeleteBuffersARB)(GLsizei n, GLuint* ids);
extern void (APIENTRY *qglBindBufferARB)(GLenum target, GLuint id);
extern void (APIENTRY *qglBufferDataARB)(GLenum target, GLsizei size, const void* data, GLenum usage);
extern void (APIENTRY *qglBufferSubDataARB)(GLenum target, GLint offset, GLsizei size, void* data);
extern void *(APIENTRY *qglMapBufferARB)(GLenum target, GLenum access);
extern GLboolean (APIENTRY *qglUnmapBufferARB)(GLenum target);

extern const GLubyte * (APIENTRY * qglGetStringi) (GLenum name, GLuint index);

extern void (APIENTRY *qglGenFramebuffersEXT)(GLsizei n, GLuint* ids);
extern void (APIENTRY *qglDeleteFramebuffersEXT)(GLsizei n, const GLuint* ids);
extern void (APIENTRY *qglBindFramebufferEXT)(GLenum target, GLuint id);
extern void (APIENTRY *qglGenRenderbuffersEXT)(GLsizei n, GLuint* ids);
extern void (APIENTRY *qglDeleteRenderbuffersEXT)(GLsizei n, const GLuint* ids);
extern void (APIENTRY *qglBindRenderbufferEXT)(GLenum target, GLuint id);
extern void (APIENTRY *qglRenderbufferStorageEXT)(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height);
extern void (APIENTRY *qglFramebufferTexture2DEXT)(GLenum target, GLenum attachmentPoint, GLenum textureTarget, GLuint textureId, GLint  level);

/*
extern qboolean gl_arb_fragment_program;
extern PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB;
extern PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB;
extern PFNGLBINDPROGRAMARBPROC qglBindProgramARB;
extern PFNGLGENPROGRAMSARBPROC qglGenProgramsARB;
*/

//glslang - arb_shader_objects
extern FTEPFNGLCREATEPROGRAMOBJECTARBPROC	qglCreateProgramObjectARB;
extern FTEPFNGLDELETEOBJECTARBPROC			qglDeleteObjectARB;
extern FTEPFNGLUSEPROGRAMOBJECTARBPROC		qglUseProgramObjectARB;
extern FTEPFNGLCREATESHADEROBJECTARBPROC	qglCreateShaderObjectARB;
extern FTEPFNGLSHADERSOURCEARBPROC			qglShaderSourceARB;
extern FTEPFNGLCOMPILESHADERARBPROC		qglCompileShaderARB;
extern FTEPFNGLGETOBJECTPARAMETERIVARBPROC	qglGetObjectParameterivARB;
extern FTEPFNGLATTACHOBJECTARBPROC			qglAttachObjectARB;
extern FTEPFNGLGETINFOLOGARBPROC			qglGetInfoLogARB;
extern FTEPFNGLLINKPROGRAMARBPROC			qglLinkProgramARB;
extern FTEPFNGLGETUNIFORMLOCATIONARBPROC	qglGetUniformLocationARB;
extern FTEPFNGLUNIFORMMATRIX4FVARBPROC		qglUniformMatrix4fvARB;
extern FTEPFNGLUNIFORM4FARBPROC			qglUniform4fARB;
extern FTEPFNGLUNIFORM4FVARBPROC			qglUniform4fvARB;
extern FTEPFNGLUNIFORM3FARBPROC			qglUniform3fARB;
extern FTEPFNGLUNIFORM3FVARBPROC			qglUniform3fvARB;
extern FTEPFNGLUNIFORM1IARBPROC			qglUniform1iARB;
extern FTEPFNGLUNIFORM1FARBPROC			qglUniform1fARB;

//glslang helper api
GLhandleARB GLSlang_CreateProgram (char *precompilerconstants, char *vert, char *frag);
GLint GLSlang_GetUniformLocation (int prog, char *name);
#define GLSlang_UseProgram(prog) qglUseProgramObjectARB(prog);
#define GLSlang_SetUniform1i(uni, parm0) qglUniform1iARB(uni, parm0);
#define GLSlang_SetUniform1f(uni, parm0) qglUniform1fARB(uni, parm0);
#define GLSlang_DeleteObject(object) qglDeleteObjectARB(object);


extern FTEPFNGLLOCKARRAYSEXTPROC qglLockArraysEXT;
extern FTEPFNGLUNLOCKARRAYSEXTPROC qglUnlockArraysEXT;

void GL_Init(void *(*getglfunction) (char *name));

#endif

qbyte GetPaletteIndex(int red, int green, int blue);
int Mod_ReadFlagsFromMD1(char *name, int md3version);

/*
//opengl 3 deprecation

. Application-generated object names - the names of all object types, such as
buffer, query, and texture objects, must be generated using the corresponding
Gen* commands. Trying to bind an object name not returned by a Gen*
command will result in an INVALID OPERATION error. This behavior is
already the case for framebuffer, renderbuffer, and vertex array objects. Object
types which have default objects (objects named zero) , such as vertex
array, framebuffer, and texture objects, may also bind the default object, even
though it is not returned by Gen*.

. OpenGL Shading Language versions 1.10 and 1.20. These versions of the
shading language depend on many API features that have also been deprecated.

. Pixel transfer modes and operations - all pixel transfer modes, including
pixel maps, shift and bias, color table lookup, color matrix, and convolution
commands and state, and all associated state and commands defining
that state.

. Legacy OpenGL 1.0 pixel formats - the values 1, 2, 3, and 4 are no longer
accepted as internal formats by TexImage* or any other command taking
an internal format argument. The initial internal format of a texel array is
RGBA instead of 1.

. Texture borders - the border value to TexImage* must always be zero, or
an INVALID VALUE error is generated (section 3.8.1); all language in section
3.8 referring to nonzero border widths during texture image specification
and texture sampling; and all associated state.

GL_COLOR_INDEX

glBegin
glEnd
glEdgeFlag*; 
glColor*,
glFogCoord*
glIndex*
glNormal3*
glSecondaryColor3*
glTexCoord*
glVertex*

glColorPointer
glEdgeFlagPointer
glFogCoordPointer
glIndexPointer
glNormalPointer
glSecondary-
glColorPointer, 
glTexCoordPointer
glVertexPointer
glEnableClientState
glDisableClientState,

glInterleavedArrays
glClientActiveTexture
glFrustum,
glLoadIdentity
glLoadMatrix
glLoadTransposeMatrix
glMatrixMode,
glMultMatrix
glMultTransposeMatrix
glOrtho
glPopMatrix
glPushMatrix,
glRotate
glScale
glTranslate
GL_RESCALE_NORMAL
GL_NORMALIZE
glTexGen*
GL_TEXTURE_GEN_*,
Material*
glLight*
glLightModel*
glColorMaterial
glShadeModel
GL_LIGHTING
GL_VERTEX_PROGRAM_TWO_SIDE
GL_LIGHTi,
GL_COLOR_MATERIAL
glClipPlane
GL_CLAMP_VERTEX_COLOR
GL_CLAMP_FRAGMENT_COLOR
glRect*

glRasterPos*
glWindowPos*

GL_POINT_SMOOTH
GL_POINT_SPRITE

glLineStipple
GL_LINE_STIPPLE
GL_POLYGON
GL_QUADS
GL_QUAD_STRIP
glPolygonMode
glPolygonStipple
GL_POLYGON_STIPPLE
glDrawPixels
glPixelZoom
glBitmap
GL_BITMAP

GL_TEXTURE_COMPONENTS
GL_ALPHA
GL_LUMINANCE
GL_LUMINANCE_ALPHA
GL_INTENSITY

GL_DEPTH_TEXTURE_MODE

GL_CLAMP

GL_GENERATE_MIPMAP

glAreTexturesResident
glPrioritizeTextures,
GL_TEXTURE_PRIORITY
GL_TEXTURE_ENV
GL_TEXTURE_FILTER_CONTROL
GL_TEXTURE_LOD_BIAS

GL_TEXTURE_1D
GL_TEXTURE_2D,
GL_TEXTURE_3D
GL_TEXTURE_1D_ARRAY
GL_TEXTURE_2D_ARRAY
GL_TEXTURE_CUBE_MAP
GL_COLOR_SUM
GL_FOG
glFog
GL_MAX_TEXTURE_UNITS
GL_MAX_TEXTURE_COORDS
glAlphaFunc
GL_ALPHA_TEST

glClearAccum
GL_ACCUM_BUFFER_BIT

glCopyPixels

GL_AUX0
GL_RED_BITS
GL_GREEN_BITS
GL_BLUE_BITS
GL_ALPHA_BITS
GL_DEPTH_BITS
STENCIL BITS
glMap*
glEvalCoord*
glMapGrid*
glEvalMesh*
glEvalPoint*

glRenderMode
glInitNames
glPopName
glPushName
glLoadName
glSelectBuffer
glFeedbackBuffer
glPassThrough
glNewList
glEndList
glCallList
glCallLists
glListBase
glGenLists,
glIsList
glDeleteLists


GL_PERSPECTIVE_CORRECTION_HINT
GL_POINT_SMOOTH_HINT,
GL_FOG_HINT
GL_GENERATE_MIPMAP_HINT

glPushAttrib
glPushClientAttrib
glPopAttrib
glPopClientAttrib,
GL_MAX_ATTRIB_STACK_DEPTH,
GL_MAX_CLIENT_ATTRIB_STACK_DEPTH
GL_ATTRIB_STACK_DEPTH
GL_CLIENT_ATTRIB_STACK_DEPTH
GL_ALL_ATTRIB_BITS
GL_CLIENT_ALL_ATTRIB_BITS.
GL_EXTENSIONS
*/

#endif
