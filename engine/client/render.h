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

// refresh.h -- public interface to refresh functions

// default soldier colors
#define TOP_DEFAULT		1
#define BOTTOM_DEFAULT	6

#define	TOP_RANGE		(TOP_DEFAULT<<4)
#define	BOTTOM_RANGE	(BOTTOM_DEFAULT<<4)

extern int		r_framecount;

struct msurface_s;
struct batch_s;
struct model_s;
struct texnums_s;
struct texture_s;

static const texid_t r_nulltex = {{0}};


#if 1 || defined(MINIMAL) || defined(D3DQUAKE) || defined(ANDROID)
	#define sizeof_index_t 2
#endif
#if sizeof_index_t == 2
	#define GL_INDEX_TYPE GL_UNSIGNED_SHORT
	#define D3DFMT_QINDEX D3DFMT_INDEX16
	typedef unsigned short index_t;
	#define MAX_INDICIES 0xffff
#else
	#define GL_INDEX_TYPE GL_UNSIGNED_INT
	#define D3DFMT_QINDEX D3DFMT_INDEX32
	typedef unsigned int index_t;
	#define MAX_INDICIES 0xffffffff
#endif

//=============================================================================

//the eye doesn't see different colours in the same proportion.
//must add to slightly less than 1
#define NTSC_RED 0.299
#define NTSC_GREEN 0.587
#define NTSC_BLUE 0.114
#define NTSC_SUM (NTSC_RED + NTSC_GREEN + NTSC_BLUE)

typedef enum {
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_BEAM,
	RT_RAIL_CORE,
	RT_RAIL_RINGS,
	RT_LIGHTNING,
	RT_PORTALSURFACE,		// doesn't draw anything, just info for portals

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

struct dlight_s;
typedef struct entity_s
{
	int						keynum;			// for matching entities in different frames
	vec3_t					origin;
	vec3_t					angles;
	vec3_t					axis[3];

	vec4_t					shaderRGBAf; /*colormod+alpha, available for shaders to mix*/
	float					shaderTime;  /*timestamp, for syncing shader times to spawns*/
	vec3_t					glowmod;     /*meant to be a multiplier for the fullbrights*/

	int						light_known; /*bsp lighting has been caled*/
	vec3_t                  light_avg;   /*midpoint level*/
	vec3_t                  light_range; /*avg + this = max, avg - this = min*/
	vec3_t                  light_dir;

	vec3_t					oldorigin;
	vec3_t					oldangles;
	
	struct model_s			*model;			// NULL = no model
	int						skinnum;		// for Alias models

	int						playerindex;	//for qw skins
	int						topcolour;		//colourmapping
	int						bottomcolour;	//colourmapping
	int						h2playerclass;	//hexen2's quirky colourmapping

//	struct efrag_s			*efrag;			// linked list of efrags (FIXME)
//	int						visframe;		// last frame this entity was
											// found in an active leaf
											// only used for static objects
											
//	int						dlightframe;	// dynamic lighting
//	int						dlightbits;
	
// FIXME: could turn these into a union
//	int						trivial_accept;
//	struct mnode_s			*topnode;		// for bmodels, first world node
											//  that splits bmodel, or NULL if
											//  not split

	framestate_t			framestate;

	int flags;

	refEntityType_t rtype;
	float rotation;

	struct shader_s *forcedshader;

#ifdef PEXT_SCALE
	float scale;
#endif
#ifdef PEXT_FATNESS
	float fatness;
#endif
#ifdef PEXT_HEXEN2
	int drawflags;
	int abslight;
#endif
} entity_t;

#define RDFD_FOV 1
typedef struct
{
	vrect_t		grect;				// game rectangle. fullscreen except for csqc/splitscreen. 
	vrect_t		vrect;				// subwindow in grect for 3d view

	vec3_t		pvsorigin;			/*render the view using this point for pvs (useful for mirror views)*/
	vec3_t		vieworg;			/*logical view center*/
	vec3_t		viewangles;
	vec3_t		viewaxis[3];		/*forward, left, up (NOT RIGHT)*/

	float		fov_x, fov_y, afov;

	qboolean	drawsbar;
	int			flags;	//(Q2)RDF_ flags
	int			dirty;

	playerview_t *playerview;
//	int			currentplayernum;

	float		time;
//	float		waterheight;	//updated by the renderer. stuff sitting at this height generate ripple effects

	float		m_projection[16];
	float		m_view[16];

	vec4_t		gfog_rgbd;

	vrect_t		pxrect;		/*vrect, but in pixels rather than virtual coords*/
	qboolean	externalview; /*draw external models and not viewmodels*/
	qboolean	recurse;	/*in a mirror/portal/half way through drawing something else*/
	qboolean	forcevis;	/*if true, vis comes from the forcedvis field instead of recalculated*/
	qboolean	flipcull;	/*reflected/flipped view, requires inverted culling*/
	qboolean	useperspective; /*not orthographic*/

	int			postprocshader; /*if set, renders to texture then invokes this shader*/
	int			postproccube; /*postproc shader wants a cubemap, this is the mask of sides required*/

	qbyte		*forcedvis;
} refdef_t;

extern	refdef_t	r_refdef;
extern vec3_t	r_origin, vpn, vright, vup;

extern	struct texture_s	*r_notexture_mip;

extern	entity_t	r_worldentity;

void BE_GenModelBatches(struct batch_s **batches);

//gl_alias.c
void GL_GAliasFlushSkinCache(void);
void R_GAlias_DrawBatch(struct batch_s *batch);
void R_GAlias_GenerateBatches(entity_t *e, struct batch_s **batches);
void R_LightArraysByte_BGR(const entity_t *entity, vecV_t *coords, byte_vec4_t *colours, int vertcount, vec3_t *normals);
void R_LightArrays(const entity_t *entity, vecV_t *coords, vec4_t *colours, int vertcount, vec3_t *normals, float scale);

void R_DrawSkyChain (struct batch_s *batch); /*called from the backend, and calls back into it*/
void R_InitSky (struct texnums_s *ret, struct texture_s *mt, qbyte *src); /*generate q1 sky texnums*/

//r_surf.c
void Surf_DrawWorld(void);
void Surf_GenBrushBatches(struct batch_s **batches, entity_t *ent);
void Surf_StainSurf(struct msurface_s *surf, float *parms);
void Surf_AddStain(vec3_t org, float red, float green, float blue, float radius);
void Surf_LessenStains(void);
void Surf_WipeStains(void);
void Surf_DeInit(void);
void Surf_Clear(struct model_s *mod);
void Surf_BuildLightmaps(void);				//enables Surf_BuildModelLightmaps, calls it for each bsp.
void Surf_ClearLightmaps(void);				//stops Surf_BuildModelLightmaps from working.
void Surf_BuildModelLightmaps (struct model_s *m);	//rebuild lightmaps for a single bsp. beware of submodels.
void Surf_RenderDynamicLightmaps (struct msurface_s *fa);
void Surf_RenderAmbientLightmaps (struct msurface_s *fa, int ambient);
int Surf_LightmapShift (struct model_s *model);
#ifndef LMBLOCK_WIDTH
#define	LMBLOCK_WIDTH		128
#define	LMBLOCK_HEIGHT		128
typedef struct glRect_s {
	unsigned char l,t,w,h;
} glRect_t;
typedef unsigned char stmap;
struct mesh_s;
typedef struct {
	texid_t lightmap_texture;
	qboolean	modified;
	qboolean	external;
	qboolean	hasdeluxe;
	int			width;
	int			height;
	glRect_t	rectchange;
	qbyte		*lightmaps;//[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
	stmap		*stainmaps;//[3*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];	//rgb no a. added to lightmap for added (hopefully) speed.
} lightmapinfo_t;
extern lightmapinfo_t **lightmap;
extern int numlightmaps;
//extern texid_t		*lightmap_textures;
//extern texid_t		*deluxmap_textures;
extern int			lightmap_bytes;		// 1, 3, or 4
extern qboolean		lightmap_bgra;		/*true=bgra, false=rgba*/
#endif
void Surf_RebuildLightmap_Callback (struct cvar_s *var, char *oldvalue);


void R_SetSky(char *skyname);		/*override all sky shaders*/

#if defined(GLQUAKE)
void GLR_Init (void);
void GLR_ReInit (void);
void GLR_InitTextures (void);
void GLR_InitEfrags (void);
void GLR_RenderView (void);		// must set r_refdef first
								// called whenever r_refdef or vid change
void GLR_DrawPortal(struct batch_s *batch, struct batch_s **blist, int portaltype);

void GLR_PreNewMap(void);
void GLR_NewMap (void);

void GLR_PushDlights (void);
void GLR_DrawWaterSurfaces (void);

void GLVID_DeInit (void);
void GLR_DeInit (void);
void GLSCR_DeInit (void);
void GLVID_Console_Resize(void);
#endif
int R_LightPoint (vec3_t p);
void R_RenderDlights (void);

enum imageflags
{
	/*warning: many of these flags only apply the first time it is requested*/
	IF_CLAMP = 1<<0,
	IF_NEAREST = 1<<1,
	IF_UIPIC = 1<<10,	//subject to texturemode2d
	IF_LINEAR = 1<<11,
	IF_NOPICMIP = 1<<2,
	IF_NOMIPMAP = 1<<3,
	IF_NOALPHA = 1<<4,
	IF_NOGAMMA = 1<<5,
	IF_3DMAP = 1<<6,	/*waning - don't test directly*/
	IF_CUBEMAP = 1<<7,	/*waning - don't test directly*/
	IF_CUBEMAPEXTRA = 1<<8,
	IF_TEXTYPE = (1<<6) | (1<<7) | (1<<8), /*0=2d, 1=3d, 2-7=cubeface*/
	IF_TEXTYPESHIFT = 6, /*0=2d, 1=3d, 2-7=cubeface*/
	IF_MIPCAP = 1<<9,
	IF_REPLACE = 1<<30,
	IF_SUBDIRONLY = 1<<31
};

#define R_LoadTexture8(id,w,h,d,f,t)		R_LoadTexture(id,w,h,t?TF_TRANS8:TF_SOLID8,d,f)
#define R_LoadTexture32(id,w,h,d,f)			R_LoadTexture(id,w,h,TF_RGBA32,d,f)
#define R_LoadTextureFB(id,w,h,d,f)			R_LoadTexture(id,w,h,TF_TRANS8_FULLBRIGHT,d,f)
#define R_LoadTexture8BumpPal(id,w,h,d,f)	R_LoadTexture(id,w,h,TF_HEIGHT8PAL,d,f)
#define R_LoadTexture8Bump(id,w,h,d,f)		R_LoadTexture(id,w,h,TF_HEIGHT8,d,f)

/*it seems a little excessive to have to include glquake (and windows headers), just to load some textures/shaders for the backend*/
#ifdef GLQUAKE
texid_tf GL_AllocNewTexture(char *name, int w, int h, unsigned int flags);
void GL_UploadFmt(texid_t tex, char *name, enum uploadfmt fmt, void *data, void *palette, int width, int height, unsigned int flags);
texid_tf GL_LoadTextureFmt (char *identifier, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags);
void GL_DestroyTexture(texid_t tex);
#endif
#ifdef D3DQUAKE
texid_t D3D9_LoadTexture (char *identifier, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags);
texid_t D3D9_LoadTexture8Pal24 (char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags);
texid_t D3D9_LoadTexture8Pal32 (char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags);
texid_t D3D9_LoadCompressed (char *name);
texid_t D3D9_FindTexture (char *identifier, unsigned int flags);
texid_t D3D9_AllocNewTexture(char *ident, int width, int height, unsigned int flags);
void    D3D9_Upload (texid_t tex, char *name, enum uploadfmt fmt, void *data, void *palette, int width, int height, unsigned int flags);
void    D3D9_DestroyTexture (texid_t tex);
void D3D9_Image_Shutdown(void);

texid_t D3D11_LoadTexture (char *identifier, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags);
texid_t D3D11_LoadTexture8Pal24 (char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags);
texid_t D3D11_LoadTexture8Pal32 (char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags);
texid_t D3D11_LoadCompressed (char *name);
texid_t D3D11_FindTexture (char *identifier, unsigned int flags);
texid_t D3D11_AllocNewTexture(char *ident, int width, int height, unsigned int flags);
void    D3D11_Upload (texid_t tex, char *name, enum uploadfmt fmt, void *data, void *palette, int width, int height, unsigned int flags);
void    D3D11_DestroyTexture (texid_t tex);
void D3D11_Image_Shutdown(void);
#endif

extern int image_width, image_height;
texid_tf R_LoadReplacementTexture(char *name, char *subpath, unsigned int flags);
texid_tf R_LoadHiResTexture(char *name, char *subpath, unsigned int flags);
texid_tf R_LoadBumpmapTexture(char *name, char *subpath);

extern	texid_t	particletexture;
extern	texid_t particlecqtexture;
extern	texid_t explosiontexture;
extern	texid_t balltexture;
extern	texid_t beamtexture;
extern	texid_t ptritexture;

void	Mod_Init (void);
void Mod_Shutdown (void);
int Mod_TagNumForName(struct model_s *model, char *name);
int Mod_SkinNumForName(struct model_s *model, char *name);
int Mod_FrameNumForName(struct model_s *model, char *name);
float Mod_GetFrameDuration(struct model_s *model, int frameno);

void Mod_ResortShaders(void);
void	Mod_ClearAll (void);
struct model_s *Mod_ForName (char *name, qboolean crash);
struct model_s *Mod_FindName (char *name);
void	*Mod_Extradata (struct model_s *mod);	// handles caching
void	Mod_TouchModel (char *name);
void Mod_RebuildLightmaps (void);

struct mleaf_s *Mod_PointInLeaf (struct model_s *model, float *p);

void Mod_Think (void);
void Mod_NowLoadExternal(void);
void GLR_LoadSkys (void);
void R_BloomRegister(void);

#ifdef RUNTIMELIGHTING
void LightFace (int surfnum);
void LightLoadEntities(char *entstring);
#endif


extern struct model_s		*currentmodel;

qboolean Media_ShowFilm(void);
void Media_CaptureDemoEnd(void);
void Media_RecordFrame (void);
qboolean Media_PausedDemo (void);
int Media_Capturing (void);
double Media_TweekCaptureFrameTime(double oldtime, double time);

void MYgluPerspective(double fovx, double fovy, double zNear, double zFar);

void	R_PushDlights				(void);
qbyte *R_MarkLeaves_Q1 (void);
qbyte *R_CalcVis_Q1 (void);
qbyte *R_MarkLeaves_Q2 (void);
qbyte *R_MarkLeaves_Q3 (void);
void R_SetFrustum (float projmat[16], float viewmat[16]);
void R_SetRenderer(rendererinfo_t *ri);
void R_AnimateLight (void);
struct texture_s *R_TextureAnimation (int frame, struct texture_s *base);
void RQ_Init(void);
void RQ_Shutdown(void);

void CLQ2_EntityEvent(entity_state_t *es);
void CLQ2_TeleporterParticles(entity_state_t *es);
void CLQ2_IonripperTrail(vec3_t oldorg, vec3_t neworg);
void CLQ2_TrackerTrail(vec3_t oldorg, vec3_t neworg, int flags);
void CLQ2_Tracker_Shell(vec3_t org);
void CLQ2_TagTrail(vec3_t oldorg, vec3_t neworg, int flags);
void CLQ2_FlagTrail(vec3_t oldorg, vec3_t neworg, int flags);
void CLQ2_TrapParticles(entity_t *ent);
void CLQ2_BfgParticles(entity_t *ent);
struct q2centity_s;
void CLQ2_FlyEffect(struct q2centity_s *ent, vec3_t org);
void CLQ2_DiminishingTrail(vec3_t oldorg, vec3_t neworg, struct q2centity_s *ent, unsigned int effects);
void CLQ2_BlasterTrail2(vec3_t oldorg, vec3_t neworg);

void WritePCXfile (const char *filename, qbyte *data, int width, int height, int rowbytes, qbyte *palette, qboolean upload); //data is 8bit.
qbyte *ReadPCXFile(qbyte *buf, int length, int *width, int *height);
qbyte *ReadTargaFile(qbyte *buf, int length, int *width, int *height, qboolean *hasalpha, int asgrey);
qbyte *ReadJPEGFile(qbyte *infile, int length, int *width, int *height);
qbyte *ReadPNGFile(qbyte *buf, int length, int *width, int *height, const char *name);
qbyte *ReadPCXPalette(qbyte *buf, int len, qbyte *out);

void BoostGamma(qbyte *rgba, int width, int height);
void SaturateR8G8B8(qbyte *data, int size, float sat);
void AddOcranaLEDsIndexed (qbyte *image, int h, int w);

void Renderer_Init(void);
void Renderer_Start(void);
qboolean Renderer_Started(void);
void R_ShutdownRenderer(void);
void R_RestartRenderer_f (void);//this goes here so we can save some stack when first initing the sw renderer.

//used to live in glquake.h
qbyte GetPaletteIndex(int red, int green, int blue);
extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_drawviewmodelinvis;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_glsl_offsetmapping;
extern	cvar_t	r_shadow_realtime_dlight, r_shadow_realtime_dlight_shadows;
extern	cvar_t	r_shadow_realtime_dlight_ambient;
extern	cvar_t	r_shadow_realtime_dlight_diffuse;
extern	cvar_t	r_shadow_realtime_dlight_specular;
extern	cvar_t	r_shadow_realtime_world, r_shadow_realtime_world_shadows;
extern	cvar_t	r_shadow_shadowmapping;
extern	cvar_t	r_editlights_import_radius;
extern	cvar_t	r_editlights_import_ambient;
extern	cvar_t	r_editlights_import_diffuse;
extern	cvar_t	r_editlights_import_specular;
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_waterstyle;
extern	cvar_t	r_slimestyle;
extern	cvar_t	r_lavastyle;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_netgraph;
extern	cvar_t	r_deluxemapping;

#ifdef R_XFLIP
extern cvar_t	r_xflip;
#endif

extern cvar_t r_lightprepass;
extern cvar_t gl_maxdist;
extern	cvar_t	r_clear;
extern	cvar_t	gl_poly;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_nohwblend;
extern	cvar_t	gl_reporttjunctions;
extern	cvar_t	r_coronas, r_flashblend, r_flashblendscale;
extern	cvar_t	r_lightstylesmooth;
extern	cvar_t	r_lightstylesmooth_limit;
extern	cvar_t	r_lightstylespeed;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_load24bit;
extern	cvar_t	gl_finish;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;

extern  cvar_t	r_lightmap_saturation;

enum {
	RSPEED_TOTALREFRESH,
	RSPEED_LINKENTITIES,
	RSPEED_PROTOCOL,
	RSPEED_WORLDNODE,
	RSPEED_WORLD,
	RSPEED_DRAWENTITIES,
	RSPEED_STENCILSHADOWS,
	RSPEED_FULLBRIGHTS,
	RSPEED_DYNAMIC,
	RSPEED_PARTICLES,
	RSPEED_PARTICLESDRAW,
	RSPEED_PALETTEFLASHES,
	RSPEED_2D,
	RSPEED_SERVER,
	RSPEED_FINISH,

	RSPEED_MAX
};
extern int rspeeds[RSPEED_MAX];

enum {
	RQUANT_MSECS,	//old r_speeds
	RQUANT_EPOLYS,
	RQUANT_WPOLYS,
	RQUANT_DRAWS,
	RQUANT_ENTBATCHES,
	RQUANT_WORLDBATCHES,
	RQUANT_2DBATCHES,
	RQUANT_SHADOWFACES,
	RQUANT_SHADOWEDGES,
	RQUANT_LITFACES,

	RQUANT_MAX
};
extern int rquant[RQUANT_MAX];

#define RQuantAdd(type,quant) rquant[type] += quant

#if defined(NDEBUG) || !defined(_WIN32)
#define RSpeedLocals()
#define RSpeedMark()
#define RSpeedRemark()
#define RSpeedEnd(spt)
#else
#define RSpeedLocals() int rsp
#define RSpeedMark() int rsp = (r_speeds.ival>1)?Sys_DoubleTime()*1000000:0
#define RSpeedRemark() rsp = (r_speeds.ival>1)?Sys_DoubleTime()*1000000:0

#if defined(_WIN32) && defined(GLQUAKE)
extern void (_stdcall *qglFinish) (void);
#define RSpeedEnd(spt) do {if(r_speeds.ival > 1){if(r_speeds.ival > 2 && qglFinish)qglFinish(); rspeeds[spt] += (int)(Sys_DoubleTime()*1000000) - rsp;}}while (0)
#else
#define RSpeedEnd(spt) rspeeds[spt] += (r_speeds.ival>1)?Sys_DoubleTime()*1000000 - rsp:0
#endif
#endif
