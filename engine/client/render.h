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

//=============================================================================

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;

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

typedef struct entity_s
{
	int						keynum;			// for matching entities in different frames
	vec3_t					origin;
	vec3_t					angles;
	vec3_t					axis[3];

	vec4_t					shaderRGBAf;
	float					shaderTime;

	vec3_t					oldorigin;
	vec3_t					oldangles;
	
	struct model_s			*model;			// NULL = no model
	int						frame;
	int						skinnum;		// for Alias models

	struct player_info_s	*scoreboard;	// identify player

	float					frame1time;
	float					frame2time;

	struct efrag_s			*efrag;			// linked list of efrags (FIXME)
	int						visframe;		// last frame this entity was
											// found in an active leaf
											// only used for static objects
											
	int						dlightframe;	// dynamic lighting
	int						dlightbits;
	
// FIXME: could turn these into a union
	int						trivial_accept;
	struct mnode_s			*topnode;		// for bmodels, first world node
											//  that splits bmodel, or NULL if
											//  not split

	float	bonecontrols[4];

	int flags;

	refEntityType_t rtype;
	float rotation;

#ifdef Q3SHADERS
	struct shader_s *forcedshader;
#endif

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
#ifdef SWQUAKE
	struct palremap_s		*palremap;
#endif
	float lerpfrac;
	int oldframe;
} entity_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh
									// FIXME: not need vrect next field here?
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	float		vrectrightedge;			// rightmost right edge we care about,
										//  for use in edge list
	float		fvrectx, fvrecty;		// for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int			vrect_x_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj;
										// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible 
										// 2.0 = 90 degrees
	float		xOrigin;			// should probably always be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	float		fov_x, fov_y;
	
	int			ambientlight;

	int			flags;

	int			currentplayernum;

	float		time;

	qboolean	useperspective;
} refdef_t;


//
// refresh
//
extern	int		reinit_surfcache;


extern	refdef_t	r_refdef;
extern vec3_t	r_origin, vpn, vright, vup;

extern	struct texture_s	*r_notexture_mip;

extern	entity_t	r_worldentity;

#if defined(RGLQUAKE)
void GLR_Init (void);
void GLR_ReInit (void);
void GLR_InitTextures (void);
void GLR_InitEfrags (void);
void GLR_RenderView (void);		// must set r_refdef first
								// called whenever r_refdef or vid change
void GLR_InitSky (struct texture_s *mt);	// called at level load
void GLR_SetSky (char *name, float rotate, vec3_t axis);
qboolean GLR_CheckSky(void);

void GLR_AddEfrags (entity_t *ent);
void GLR_RemoveEfrags (entity_t *ent);

void GLR_PreNewMap(void);
void GLR_NewMap (void);

void GLR_PushDlights (void);
void GLR_DrawWaterSurfaces (void);

void GLR_AddStain(vec3_t org, float red, float green, float blue, float radius);
void GLR_LessenStains(void);

void MediaGL_ShowFrame8bit(qbyte *framedata, int inwidth, int inheight, qbyte *palette);
void MediaGL_ShowFrameRGBA_32(qbyte *framedata, int inwidth, int inheight);	//top down
void MediaGL_ShowFrameBGR_24_Flip(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...

void GLR_SetSky (char *name, float rotate, vec3_t axis);
qboolean GLR_CheckSky(void);
void GLR_AddStain(vec3_t org, float red, float green, float blue, float radius);
void GLR_LessenStains(void);

void GLVID_DeInit (void);
void GLR_DeInit (void);
void GLSCR_DeInit (void);

int GLR_LightPoint (vec3_t p);
#endif





#if defined(SWQUAKE)
void SWR_Init (void);
void SWR_InitTextures (void);
void SWR_InitEfrags (void);
void SWR_RenderView (void);		// must set r_refdef first
void SWR_ViewChanged (vrect_t *pvrect, int lineadj, float aspect);
								// called whenever r_refdef or vid change
void SWR_InitSky (struct texture_s *mt);	// called at level load
void SWR_SetSky (char *name, float rotate, vec3_t axis);
qboolean SWR_CheckSky(void);

void SWR_AddEfrags (entity_t *ent);
void SWR_RemoveEfrags (entity_t *ent);

void SWR_NewMap (void);

void SWR_PushDlights (void);

void SWR_AddStain(vec3_t org, float red, float green, float blue, float radius);
void SWR_LessenStains(void);

void MediaSW_ShowFrame8bit(qbyte *framedata, int inwidth, int inheight, qbyte *palette);
void MediaSW_ShowFrameRGBA_32(qbyte *framedata, int inwidth, int inheight);	//top down
void MediaSW_ShowFrameBGR_24_Flip(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...

void SWR_SetSky (char *name, float rotate, vec3_t axis);
qboolean SWR_CheckSky(void);
void SWR_AddStain(vec3_t org, float red, float green, float blue, float radius);
void SWR_LessenStains(void);

void SWVID_Shutdown (void);
void SWR_DeInit (void);
void SWSCR_DeInit (void);

int SWR_LightPoint (vec3_t p);
#endif

void R_AddEfrags (entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

//
// surface cache related
//
extern	int		reinit_surfcache;	// if 1, surface cache is currently empty and
extern qboolean	r_cache_thrash;	// set if thrashing the surface cache

int	D_SurfaceCacheForRes (int width, int height, int bpp);
void D_FlushCaches (void);
void D_DeleteSurfaceCache (void);
void D_InitCaches (void *buffer, int size);
void R_SetVrect (vrect_t *pvrect, vrect_t *pvrectin, int lineadj);

struct palremap_s *D_IdentityRemap(void);

//normalmaps
//bumpmaps
//32bits
//8bits
//8bitpal24
//8bitpal32

#define TF_NOMIPMAP		0x0000
#define TF_NOTBUMPMAP	0x0000
#define TF_NOALPHA		0x0000

#define TF_MIPMAP		0x0001
#define TF_BUMPMAP		0x0002	//or normalmap, depending on 8/24 bitness
#define TF_ALPHA		0x0004	//preserve alpha channel (8biit, use index 255 for transparency)

#define TF_FULLBRIGHT	0x0008	//dark pixels have alpha forced to 0
#define TF_24BIT		0x0010
#define TF_32BIT		0x0020	//use the standard quake palette

#define TF_MANDATORY	(TF_NOMIPMAP|TF_NOTBUMPMAP|TF_NOALPHA)

#if 0
/*
int R_LoadTexture(char *name, int width, int height, void *data, void *palette, int flags)
{
	if (palette)
	{
		if (flags & TF_BUMPMAP)
			return 0;	//huh?

		if (flags & TF_FULLBRIGHT)
			return 0;	//huh?

		if (flags & TF_32BIT)
			return R_LoadTexture8Pal32(name, width, height, data, palette, flags&TF_MIPMAP, flags&TF_ALPHA);

		return 0;
	}

	if (flags & TF_FULLBRIGHT)
	{
		if (flags & TF_BUMPMAP)
			return 0;	//huh?

		if (flags & TF_24BIT)
			return 0;
		if (flags & TF_32BIT)
			return 0;

		//8bit fullbrights
		return R_LoadTextureFB(name, width, height, data, flags&TF_MIPMAP, flags&TF_ALPHA);
	}

	if (flags & TF_BUMPMAP)
	{
		if (flags & TF_24BIT)
			return 0;
		if (flags & TF_32BIT)
			return R_LoadTexture32(name, width, height, data, flags&TF_MIPMAP, flags&TF_ALPHA);	//Warning: this is not correct
		return R_LoadTexture8Bump(skinname,width,height,data,usemips,alpha) R_LoadTexture(name, width, height, data, NULL, TF_BUMPMAP | ((usemips)?TF_MIPMAP:TF_NOMIPMAP) | ((alpha)?TF_ALPHA:TF_NOALPHA))
	}

	if (flags & TF_32BIT)
		return R_LoadTexture32(name, width, height, data, flags&TF_MIPMAP, flags&TF_ALPHA);
	if (data)
		return R_LoadTexture8(name, width, height, data, flags&TF_MIPMAP, flags&TF_ALPHA);

	return R_FindTexture(name);
}

	#define R_LoadTexture8Pal32(skinname,width,height,data,palette,usemips,alpha) R_LoadTexture(name, width, height, data, palette, TF_32BIT | ((usemips)?TF_MIPMAP:TF_NOMIPMAP) | ((alpha)?TF_ALPHA:TF_NOALPHA))
	#define R_LoadTexture8(skinname,width,height,data,usemips,alpha) R_LoadTexture(name, width, height, data, NULL, ((usemips)?TF_MIPMAP:TF_NOMIPMAP) | ((alpha)?TF_ALPHA:TF_NOALPHA))
	#define R_LoadTexture32(skinname,width,height,data,usemips,alpha) R_LoadTexture(name, width, height, data, NULL, TF_32BIT | ((usemips)?TF_MIPMAP:TF_NOMIPMAP) | ((alpha)?TF_ALPHA:TF_NOALPHA))
	#define R_LoadTextureFB(skinname,width,height,data,usemips,alpha) R_LoadTexture(name, width, height, data, NULL, TF_FULLBRIGHT | ((usemips)?TF_MIPMAP:TF_NOMIPMAP) | ((alpha)?TF_ALPHA:TF_NOALPHA))
	#define R_LoadTexture8Bump(skinname,width,height,data,usemips,alpha) R_LoadTexture(name, width, height, data, NULL, TF_BUMPMAP | ((usemips)?TF_MIPMAP:TF_NOMIPMAP) | ((alpha)?TF_ALPHA:TF_NOALPHA))

	#define R_FindTexture(name) R_LoadTexture(name, 0, 0, NULL, NULL, 0)
	#define R_LoadCompressed(name)  ((qrenderer == QR_OPENGL)?GL_LoadCompressed(name):0)
*/
#elif defined(RGLQUAKE) && defined(D3DQUAKE)
	#define R_LoadTexture8Pal32(skinname,width,height,data,palette,usemips,alpha) ((qrenderer == QR_DIRECT3D)?D3D_LoadTexture8Pal32(skinname, width, height, data, palette, usemips, alpha):GL_LoadTexture8Pal32(skinname, width, height, data, palette, usemips, alpha))
	#define R_LoadTexture8Pal24(skinname,width,height,data,palette,usemips,alpha) ((qrenderer == QR_DIRECT3D)?D3D_LoadTexture8Pal24(skinname, width, height, data, palette, usemips, alpha):GL_LoadTexture8Pal24(skinname, width, height, data, palette, usemips, alpha))
	#define R_LoadTexture8(skinname,width,height,data,usemips,alpha) ((qrenderer == QR_DIRECT3D)?D3D_LoadTexture(skinname, width, height, data, usemips, alpha):GL_LoadTexture(skinname, width, height, data, usemips, alpha))
	#define R_LoadTexture32(skinname,width,height,data,usemips,alpha) ((qrenderer == QR_DIRECT3D)?D3D_LoadTexture32(skinname, width, height, data, usemips, alpha):GL_LoadTexture32(skinname, width, height, data, usemips, alpha))
	#define R_LoadTextureFB(skinname,width,height,data,usemips,alpha) ((qrenderer == QR_DIRECT3D)?D3D_LoadTextureFB(skinname, width, height, data, usemips, alpha):GL_LoadTextureFB(skinname, width, height, data, usemips, alpha))
	#define R_LoadTexture8Bump(skinname,width,height,data,usemips,alpha) ((qrenderer == QR_DIRECT3D)?/*D3D_LoadTexture8Bump(skinname, width, height, data, usemips, alpha)*/NULL:GL_LoadTexture8Bump(skinname, width, height, data, usemips, alpha))

	#define R_FindTexture(name)  ((qrenderer == QR_DIRECT3D)?D3D_FindTexture(name):GL_FindTexture(name))
	#define R_LoadCompressed(name)  ((qrenderer == QR_DIRECT3D)?D3D_LoadCompressed(name):GL_LoadCompressed(name))
#elif defined(D3DQUAKE)
	#define R_LoadTexture8Pal32	D3D_LoadTexture8Pal32
	#define R_LoadTexture8Pal24	D3D_LoadTexture8Pal24
	#define R_LoadTexture8		D3D_LoadTexture
	#define R_LoadTexture32		D3D_LoadTexture32
	#define R_LoadTextureFB		D3D_LoadTextureFB
	#define R_LoadTexture8Bump	D3D_LoadTexture8Bump

	#define R_FindTexture		D3D_FindTexture
	#define R_LoadCompressed	D3D_LoadCompressed
#elif defined(RGLQUAKE)
	#define R_LoadTexture8Pal32	GL_LoadTexture8Pal32
	#define R_LoadTexture8Pal24	GL_LoadTexture8Pal24
	#define R_LoadTexture8		GL_LoadTexture
	#define R_LoadTexture32		GL_LoadTexture32
	#define R_LoadTextureFB		GL_LoadTextureFB
	#define R_LoadTexture8Bump	GL_LoadTexture8Bump

	#define R_FindTexture		GL_FindTexture
	#define R_LoadCompressed	GL_LoadCompressed
#endif



void GL_ParallelPerspective(double xmin, double xmax, double ymax, double ymin, double znear, double zfar);
void GL_InfinatePerspective(double fovx, double fovy, double zNear);

#if defined(RGLQUAKE) || defined(D3DQUAKE)

void	GLMod_Init (void);
qboolean Mod_GetTag(struct model_s *model, int tagnum, int frame, int frame2, float f2ness, float f1time, float f2time, float *result);
int Mod_TagNumForName(struct model_s *model, char *name);
int Mod_SkinNumForName(struct model_s *model, char *name);

void	GLMod_ClearAll (void);
struct model_s *GLMod_ForName (char *name, qboolean crash);
struct model_s *GLMod_FindName (char *name);
void	*GLMod_Extradata (struct model_s *mod);	// handles caching
void	GLMod_TouchModel (char *name);

struct mleaf_s *GLMod_PointInLeaf (struct model_s *model, float *p);

void GLMod_Think (void);
void GLMod_NowLoadExternal(void);
void GLR_WipeStains(void);
void GLR_LoadSkys (void);
#endif

#if defined(SWQUAKE)

void	SWMod_Init (void);
void	SWMod_ClearAll (void);
struct model_s *SWMod_ForName (char *name, qboolean crash);
struct model_s *SWMod_FindName (char *name);
void	*SWMod_Extradata (struct model_s *mod);	// handles caching
void	SWMod_TouchModel (char *name);

struct mleaf_s *SWMod_PointInLeaf (struct model_s *model, float *p);

void SWMod_Think (void);
void SWMod_NowLoadExternal(void);
#endif

extern struct model_s		*currentmodel;

qboolean Media_ShowFilm(void);
void Media_CaptureDemoEnd(void);
void Media_RecordFrame (void);
qboolean Media_PausedDemo (void);
double Media_TweekCaptureFrameTime(double time);

void MYgluPerspective(double fovx, double fovy, double zNear, double zFar);

void R_MarkLeaves (void);
void R_SetFrustum (void);
void R_SetRenderer(int wanted);
void RQ_Init(void);

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

void WritePCXfile (char *filename, qbyte *data, int width, int height, int rowbytes, qbyte *palette, qboolean upload); //data is 8bit.
qbyte *ReadPCXFile(qbyte *buf, int length, int *width, int *height);
qbyte *ReadTargaFile(qbyte *buf, int length, int *width, int *height, int asgrey);
qbyte *ReadJPEGFile(qbyte *infile, int length, int *width, int *height);
qbyte *ReadPNGFile(qbyte *buf, int length, int *width, int *height, char *name);
qbyte *ReadPCXPalette(qbyte *buf, int len, qbyte *out);

void BoostGamma(qbyte *rgba, int width, int height);
void SaturateR8G8B8(qbyte *data, int size, float sat);
void AddOcranaLEDsIndexed (qbyte *image, int h, int w);

void CL_NewDlightRGB (int key, float x, float y, float z, float radius, float time,
				   float r, float g, float b);

void Renderer_Init(void);
void R_RestartRenderer_f (void);//this goes here so we can save some stack when first initing the sw renderer.

//used to live in glquake.h
qbyte GetPaletteIndex(int red, int green, int blue);
qbyte GetPaletteNoFB(int red, int green, int blue);
extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_drawviewmodelinvis;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_netgraph;

#ifdef R_XFLIP
extern cvar_t	r_xflip;
#endif

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_poly;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_keeptjunctions;
extern	cvar_t	gl_reporttjunctions;
extern	cvar_t	r_flashblend;
extern	cvar_t	r_lightstylesmooth;
extern	cvar_t	r_lightstylespeed;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_load24bit;
extern	cvar_t	gl_finish;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;

extern cvar_t   d_palconvwrite;
extern cvar_t	d_palremapsize;

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
int rspeeds[RSPEED_MAX];

enum {
	RQUANT_MSECS,	//old r_speeds
	RQUANT_EPOLYS,
	RQUANT_WPOLYS,
	RQUANT_SHADOWFACES,
	RQUANT_SHADOWEDGES,
	RQUANT_LITFACES,

	RQUANT_MAX
};
int rquant[RQUANT_MAX];

#define RQuantAdd(type,quant) rquant[type] += quant;

#define RSpeedLocals() int rsp
#define RSpeedMark() int rsp = r_speeds.value?Sys_DoubleTime()*1000000:0
#define RSpeedRemark() rsp = r_speeds.value?Sys_DoubleTime()*1000000:0

//extern void (_stdcall *qglFinish) (void);
//#define RSpeedEnd(spt) do {qglFinish(); rspeeds[spt] += r_speeds.value?Sys_DoubleTime()*1000000 - rsp:0;}while (0)
#define RSpeedEnd(spt) rspeeds[spt] += r_speeds.value?Sys_DoubleTime()*1000000 - rsp:0
