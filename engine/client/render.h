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

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

struct msurface_s;

//=============================================================================

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;


typedef struct entity_s
{
	int						keynum;			// for matching entities in different frames
	vec3_t					origin;
	vec3_t					angles;	
	vec3_t					axis[3];	

	vec3_t					oldorigin;
	vec3_t					oldangles;
	
	struct model_s			*model;			// NULL = no model
	int						frame;
	qbyte					*colormap;
	int						skinnum;		// for Alias models

	struct player_info_s	*scoreboard;	// identify player

	float					syncbase;

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

	int flags;

#ifdef PEXT_SCALE
	float scale;
#endif
#ifdef PEXT_TRANS
	float alpha;
#endif
#ifdef PEXT_FATNESS
	float fatness;
#endif
#ifdef PEXT_HEXEN2
	int drawflags;
	int abslight;
#endif
	float lerpfrac;
	float lerptime;
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
	float		xOrigin;			// should probably allways be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	float		fov_x, fov_y;
	
	int			ambientlight;

	int			flags;

	int			currentplayernum;
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

void GLR_NewMap (void);

void GLR_PushDlights (void);
void GLR_DrawWaterSurfaces (void);

void GLR_AddStain(vec3_t org, float red, float green, float blue, float radius);
void GLR_LessenStains(void);

void MediaGL_ShowFrame8bit(qbyte *framedata, int inwidth, int inheight, qbyte *palette);
void MediaGL_ShowFrameRGBA_32(qbyte *framedata, int inwidth, int inheight);	//top down
void MediaGL_ShowFrameBGR_24_Flip(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...
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
#endif
void R_ParseParticleEffect (void);
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);

typedef struct {
	float lastdist;
	struct beamseg_s *lastbeam; // last beam point
} trailstate_t;
int R_RocketTrail (vec3_t start, vec3_t end, int type, trailstate_t *oldpoint);
int R_RunParticleEffectType(vec3_t org, vec3_t dir, float count, int type);
void R_RunParticleEffect2 (vec3_t org, vec3_t dmin, vec3_t dmax, int color, int effect, int count);
void R_RunParticleEffect3 (vec3_t org, vec3_t box, int color, int effect, int count);
void R_RunParticleEffect4 (vec3_t org, float radius, int color, int effect, int count);
int AllocateParticleType(char *name);

void R_DefaultTrail (struct model_s *model);
void R_TorchEffect (vec3_t pos, int type);

void R_EntityParticles (float *org, qbyte colour, float *radius);
void R_BlobExplosion (vec3_t org);
qboolean R_EMPExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);

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





#if defined(RGLQUAKE)

void	GLMod_Init (void);
void	GLMod_ClearAll (void);
struct model_s *GLMod_ForName (char *name, qboolean crash);
struct model_s *GLMod_FindName (char *name);
void	*GLMod_Extradata (struct model_s *mod);	// handles caching
void	GLMod_TouchModel (char *name);

struct mleaf_s *GLMod_PointInLeaf (float *p, struct model_s *model);
qbyte	*GLMod_LeafPVS (struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);
qbyte	*GLMod_LeafnumPVS (int leafnum, struct model_s *model, qbyte *buffer);

void GLMod_Think (void);
void GLMod_NowLoadExternal(void);
void GLR_WipeStains(void);
void R_LoadSkys (void);
#endif

#if defined(SWQUAKE)

void	SWMod_Init (void);
void	SWMod_ClearAll (void);
struct model_s *SWMod_ForName (char *name, qboolean crash);
struct model_s *SWMod_FindName (char *name);
void	*SWMod_Extradata (struct model_s *mod);	// handles caching
void	SWMod_TouchModel (char *name);

struct mleaf_s *SWMod_PointInLeaf (float *p, struct model_s *model);
qbyte	*SWMod_LeafPVS (struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);

void SWMod_Think (void);
void SWMod_NowLoadExternal(void);
#endif

qboolean Media_ShowFilm(void);
void Media_CaptureDemoEnd(void);
void Media_RecordAudioFrame (short *sample_buffer, int samples);
void Media_RecordFrame (void);

void R_Part_SkyTri(float *v1, float *v2, float *v3, struct msurface_s *surf);
void R_ClearParticles (void);
void R_Part_NewServer(void);
int ParticleTypeForName(char *name);
void R_SetRenderer(r_qrenderer_t wanted);
void R_DrawParticles (void);
void R_InitParticles (void);
void RQ_Init(void);
void R_BlasterParticles (vec3_t org, vec3_t dir);
void CLQ2_RailTrail (vec3_t start, vec3_t end);
void CLQ2_BubbleTrail (vec3_t start, vec3_t end);
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
void CLQ2_BlasterTrail(vec3_t oldorg, vec3_t neworg);
void CLQ2_BlasterTrail2(vec3_t oldorg, vec3_t neworg);
void CLQ2_RocketTrail(vec3_t oldorg, vec3_t neworg, struct q2centity_s *ent);

void WritePCXfile (char *filename, qbyte *data, int width, int height, int rowbytes, qbyte *palette, qboolean upload); //data is 8bit.
qbyte *ReadPCXFile(qbyte *buf, int length, int *width, int *height);
qbyte *ReadTargaFile(qbyte *buf, int length, int *width, int *height, int asgrey);
qbyte *ReadJPEGFile(qbyte *infile, int length, int *width, int *height);
qbyte *ReadPNGFile(qbyte *buf, int length, int *width, int *height);
qbyte *ReadPCXPalette(qbyte *buf, int len, qbyte *out);

void BoostGamma(qbyte *rgba, int width, int height);

void CL_NewDlightRGB (int key, float x, float y, float z, float radius, float time,
				   float r, float g, float b);

void Renderer_Init(void);
void R_RestartRenderer_f (void);//this goes here so we can save some stack when first initing the sw renderer.

//used to live in glquake.h
qbyte GetPalette(int red, int green, int blue);
extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
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
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_load24bit;
extern	cvar_t	gl_finish;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;

extern	cvar_t	r_palconvbits;
extern cvar_t   r_palconvwrite;
