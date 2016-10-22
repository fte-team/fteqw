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

#ifndef __MODEL__
#define __MODEL__

#include "modelgen.h"
#include "spritegn.h"

struct hull_s;
struct trace_s;
struct wedict_s;
struct model_s;
struct world_s;
struct dlight_s;
typedef struct builddata_s builddata_t;

typedef enum {
	SHADER_SORT_NONE,
	SHADER_SORT_RIPPLE,
	SHADER_SORT_PRELIGHT,
	SHADER_SORT_PORTAL,
	SHADER_SORT_SKY,
	SHADER_SORT_OPAQUE,
	//fixme: occlusion tests
	SHADER_SORT_DECAL,
	SHADER_SORT_SEETHROUGH,
	SHADER_SORT_BANNER,
	SHADER_SORT_UNDERWATER,
	SHADER_SORT_BLEND,
	SHADER_SORT_ADDITIVE,
	SHADER_SORT_NEAREST,


	SHADER_SORT_COUNT
} shadersort_t;

#define MAX_BONES 256
#ifdef FTE_TARGET_WEB
#define MAX_GPU_BONES 32	//ATI drivers bug out and start to crash if you put this at 128. FIXME: make dynamic.
#else
#define MAX_GPU_BONES 64	//ATI drivers bug out and start to crash if you put this at 128. FIXME: make dynamic.
#endif
struct doll_s;
void rag_uninstanciateall(void);
void rag_flushdolls(qboolean force);
void rag_freedoll(struct doll_s *doll);
struct doll_s *rag_createdollfromstring(struct model_s *mod, const char *fname, int numbones, const char *file);
struct world_s;
void rag_doallanimations(struct world_s *world);
void rag_removedeltaent(lerpents_t *le);
void rag_updatedeltaent(entity_t *ent, lerpents_t *le);
void rag_lerpdeltaent(lerpents_t *le, unsigned int bonecount, short *newstate, float frac, short *oldstate);

typedef struct mesh_s
{
	int				numvertexes;
	int				numindexes;

	/*position within its vbo*/
	unsigned int	vbofirstvert;
	unsigned int	vbofirstelement;

	/*
	FIXME: move most of this stuff out into a vbo struct
	*/

	float			xyz_blendw[2];

	/*arrays used for rendering*/
	vecV_t			*xyz_array;
	vecV_t			*xyz2_array;
	vec3_t			*normals_array;	/*required for lighting*/
	vec3_t			*snormals_array;/*required for rtlighting*/
	vec3_t			*tnormals_array;/*required for rtlighting*/
	vec2_t			*st_array;		/*texture coords*/
	vec2_t			*lmst_array[MAXRLIGHTMAPS];	/*second texturecoord set (merely dubbed lightmap, one for each potential lightstyle)*/
	avec4_t			*colors4f_array[MAXRLIGHTMAPS];/*floating point colours array*/
	byte_vec4_t		*colors4b_array;/*byte colours array*/

    index_t			*indexes;

	//required for shadow volumes
	int				*trneighbors;
	vec3_t			*trnormals;

	qboolean		istrifan;	/*if its a fan/poly/single quad  (permits optimisations)*/
	const float		*bones;
	int				numbones;
	byte_vec4_t		*bonenums;
	vec4_t			*boneweights;
} mesh_t;
extern mesh_t nullmesh;

/*
batches are generated for each shader/ent as required.
once a batch is known to the backend for that frame, its shader, vbo, ent, lightmap, textures may not be changed until the frame has finished rendering. This is to potentially permit caching.
*/
typedef struct batch_s
{
	mesh_t **mesh; /*list must be long enough for all surfaces that will form part of this batch times two, for mirrors/portals*/
	struct batch_s *next;
	unsigned int meshes;
	unsigned int firstmesh;

	shader_t *shader;
	struct vbo_s *vbo;
	entity_t *ent;	/*used for shader properties*/
	struct mfog_s *fog;

	short lightmap[MAXRLIGHTMAPS];	/*used for shader lightmap textures*/
	unsigned char lmlightstyle[MAXRLIGHTMAPS];
	unsigned char vtlightstyle[MAXRLIGHTMAPS];

	unsigned int maxmeshes;	/*not used by backend*/
	unsigned int flags;	/*backend flags (force transparency etc)*/
	struct texture_s *texture; /*is this used by the backend?*/
	struct texnums_s *skin;

	void (*buildmeshes)(struct batch_s *b);
#if R_MAX_RECURSE > 2
	unsigned int recursefirst[R_MAX_RECURSE-2];	//fixme: should thih, firstmesh, and meshes be made ushorts?
#endif
	/*caller-use, not interpreted by backend*/
	union
	{
		struct
		{
			unsigned int shadowbatch; //a unique index to accelerate shadowmesh generation (dlights, yay!)
			unsigned int ebobatch;	//
		};
		struct 
		{
			unsigned int surf_first;
			unsigned int surf_count;
		};
		vec4_t plane;	/*used only at load (for portal surfaces, so multiple planes are not part of the same batch)*/
	};
} batch_t;
/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

// entity effects

#define	EF_BRIGHTFIELD			(1<<0)
#define	EF_MUZZLEFLASH 			(1<<1)
#define	EF_BRIGHTLIGHT 			(1<<2)
#define	EF_DIMLIGHT 			(1<<3)
#define	QWEF_FLAG1	 			(1<<4)	//only applies to qw player entities
#define	NQEF_NODRAW				(1<<4)	//so packet entities are free to get this instead
#define	QWEF_FLAG2	 			(1<<5)	//only applies to qw player entities
#define	NQEF_ADDITIVE			(1<<5)	//so packet entities are free to get this instead
#define	EF_BLUE					(1<<6)
#define	EF_RED					(1<<7)
#define	H2EF_NODRAW				(1<<7)	//this is going to get complicated... emulated server side.
#define	DPEF_NOGUNBOB			(1<<8)	//viewmodel attachment does not bob
#define	EF_FULLBRIGHT			(1<<9)	//abslight=1
#define	DPEF_FLAME				(1<<10)	//'onfire'
#define	DPEF_STARDUST			(1<<11)	//'showering sparks'
#define	EF_NOSHADOW		 		(1<<12)	//doesn't cast a shadow
#define	EF_NODEPTHTEST			(1<<13)	//shows through walls.
#define		DPEF_SELECTABLE_		(1<<14)	//highlights when prydoncursored
#define		DPEF_DOUBLESIDED_		(1<<15)	//disables culling
#define		DPEF_NOSELFSHADOW_		(1<<16)	//doesn't cast shadows on any noselfshadow entities.
#define		DPEF_DYNAMICMODELLIGHT_	(1<<17)	//forces dynamic lights... I have no idea what this is actually needed for.
#define	EF_GREEN				(1<<18)
#define	EF_UNUSED19				(1<<19)
#define	EF_RESTARTANIM_BIT		(1<<20)	//restarts the anim when toggled between states
#define	EF_TELEPORT_BIT			(1<<21)	//disable lerping when toggled between states
#define DPEF_LOWPRECISION		(1<<22) //part of the protocol/server, not the client itself.
#define EF_NOMODELFLAGS			(1<<23)
#define EF_MF_ROCKET			(1<<24)
#define EF_MF_GRENADE			(1<<25)
#define EF_MF_GIB				(1<<26)
#define EF_MF_ROTATE			(1<<27)
#define EF_MF_TRACER			(1<<28)
#define EF_MF_ZOMGIB			(1u<<29)
#define EF_MF_TRACER2			(1u<<30)
#define EF_MF_TRACER3			(1u<<31)

#define EF_HASPARTICLETRAIL		(0xff800000 | EF_BRIGHTFIELD|DPEF_FLAME|DPEF_STARDUST)

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/

struct mnode_s;

typedef struct {
	//model is being purged from memory.
	void (*PurgeModel) (struct model_s *mod);

	unsigned int (*PointContents)	(struct model_s *model, vec3_t axis[3], vec3_t p);
	unsigned int (*BoxContents)		(struct model_s *model, int hulloverride, int frame, vec3_t axis[3], vec3_t p, vec3_t mins, vec3_t maxs);

	//deals with whatever is native for the bsp (gamecode is expected to distinguish this).
	qboolean (*NativeTrace)		(struct model_s *model, int hulloverride, int frame, vec3_t axis[3], vec3_t p1, vec3_t p2, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int against, struct trace_s *trace);
	unsigned int (*NativeContents)(struct model_s *model, int hulloverride, int frame, vec3_t axis[3], vec3_t p, vec3_t mins, vec3_t maxs);

	unsigned int (*FatPVS)		(struct model_s *model, vec3_t org, qbyte *pvsbuffer, unsigned int buffersize, qboolean merge);
	qboolean (*EdictInFatPVS)	(struct model_s *model, struct pvscache_s *edict, qbyte *pvsbuffer);
	void (*FindTouchedLeafs)	(struct model_s *model, struct pvscache_s *ent, vec3_t cullmins, vec3_t cullmaxs);	//edict system as opposed to q2 game dll system.

	void (*LightPointValues)	(struct model_s *model, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir);
	void (*StainNode)			(struct mnode_s *node, float *parms);
	void (*MarkLights)			(struct dlight_s *light, int bit, struct mnode_s *node);

	int	(*ClusterForPoint)		(struct model_s *model, vec3_t point);	//pvs index (leaf-1 for q1bsp). may be negative (ie: no pvs).
	qbyte *(*ClusterPVS)		(struct model_s *model, int cluster, qbyte *buffer, unsigned int buffersize);
} modelfuncs_t;




//
// in memory representation
//
// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vec3_t		position;
} mvertex_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

typedef struct vbo_s
{
	unsigned int numvisible;
	struct msurface_s **vislist;

	unsigned int indexcount;
	unsigned int vertcount;
	unsigned int meshcount;
	struct msurface_s **meshlist;

	vboarray_t		indicies;
	void *vertdata; /*internal use*/

	int vao;
	unsigned int vaodynamic;	/*mask of the attributes that are dynamic*/
	unsigned int vaoenabled;	/*mask of the attributes *currently* enabled. renderer may change this */
	vboarray_t coord;
	vboarray_t coord2;
	vboarray_t texcoord;
	vboarray_t lmcoord[MAXRLIGHTMAPS];

	vboarray_t normals;
	vboarray_t svector;
	vboarray_t tvector;

	qboolean colours_bytes;
	vboarray_t colours[MAXRLIGHTMAPS];

	vboarray_t bonenums;

	vboarray_t boneweights;

	void *vbomem;
	void *ebomem;

	unsigned int vbobones;
	const float *bones;
	unsigned  int numbones;

	struct vbo_s *next;
} vbo_t;
void GL_SelectVBO(int vbo);
void GL_SelectEBO(int vbo);
void GL_DeselectVAO(void);

typedef struct texture_s
{
	char		name[64];
	unsigned	width, height;

	struct shader_s	*shader;
	char		*partname;				//parsed from the worldspawn entity

	int			anim_total;				// total tenths in sequence ( 0 = no)
	int			anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s *anim_next;		// in the animation sequence
	struct texture_s *alternate_anims;	// bmodels in frmae 1 use these

	qbyte		*mips[4];	//the different mipmap levels.
	qbyte		*palette;	//host_basepal or halflife per-texture palette
} texture_t;
/*
typedef struct
{
	float coord[3];
	float texcoord[2];
	float lmcoord[2];

	float normals[3];
	float svector[3];
	float tvector[3];
} vbovertex_t;
*/
#define SURF_DRAWSKYBOX		0x00001
#define	SURF_PLANEBACK		0x00002
#define	SURF_DRAWSKY		0x00004
#define SURF_DRAWSPRITE		0x00008
#define SURF_DRAWTURB		0x00010
#define SURF_DRAWTILED		0x00020
#define SURF_DRAWBACKGROUND	0x00040
#define SURF_UNDERWATER		0x00080
#define SURF_DONTWARP		0x00100
//#define SURF_BULLETEN		0x00200
#define SURF_NOFLAT			0x08000
#define SURF_DRAWALPHA		0x10000

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	unsigned int	v[2];
} medge_t;

typedef struct mtexinfo_s
{
	float		vecs[2][4];
	float		vecscale[2];
	texture_t	*texture;
	int			flags;

	//it's a q2 thing.
	int			numframes;
	struct mtexinfo_s	*next;
} mtexinfo_t;

#define SPECULAR
#ifdef SPECULAR
#define	VERTEXSIZE	10
#else
#define	VERTEXSIZE	7
#endif

typedef struct mfog_s
{
	char			shadername[MAX_QPATH];
	struct shader_s		*shader;

	mplane_t		*visibleplane;

	int				numplanes;
	mplane_t		**planes;
} mfog_t;

#define LMSHIFT_DEFAULT 4
typedef struct msurface_s
{
	mplane_t	*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	unsigned short	numedges;	// are backwards edges

	unsigned short		lmshift;	//texels>>lmshift = lightmap samples.
	int			texturemins[2];
	short		extents[2];

	unsigned short	light_s[MAXRLIGHTMAPS], light_t[MAXRLIGHTMAPS];	// gl lightmap coordinates

	mfog_t		*fog;
	mesh_t		*mesh;

	batch_t		*sbatch;
	mtexinfo_t	*texinfo;
	int			visframe;		// should be drawn when node is crossed
	int			shadowframe;
	int			clipcount;
	
// legacy lighting info
	int			dlightframe;
	int			dlightbits;

//static lighting
	int			lightmaptexturenums[MAXRLIGHTMAPS];	//rbsp+fbsp formats have multiple lightmaps
	qbyte		styles[MAXQ1LIGHTMAPS];
	qbyte		vlstyles[MAXRLIGHTMAPS];
	int			cached_light[MAXQ1LIGHTMAPS];	// values currently used in lightmap
	int			cached_colour[MAXQ1LIGHTMAPS];
	qboolean	cached_dlight;				// true if dynamic light in cache
#ifndef NOSTAINS
	qboolean stained;
#endif
	qbyte		*samples;		// [numstyles*surfsize]
} msurface_t;

typedef struct mbrush_s
{
	struct mbrush_s *next;
	unsigned int contents;
	int numplanes;
	struct mbrushplane_s
	{
		vec3_t normal;
		float dist;
	} planes[1];
} mbrush_t;

typedef struct mnode_s
{
// common with leaf
	int			contents;		// 0, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	int			shadowframe;
	
	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;
	struct mbrush_s	*brushes;

// node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];
#if defined(Q2BSPS) || defined(MAP_PROC)
	int childnum[2];
#endif

	unsigned int		firstsurface;
	unsigned int		numsurfaces;
} mnode_t;



typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current
	int			shadowframe;

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;
	struct mbrush_s	*brushes;

// leaf specific
	qbyte		*compressed_vis;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
	qbyte		ambient_sound_level[NUM_AMBIENTS];

#if defined(Q2BSPS) || defined(Q3BSPS)
	int			cluster;
//	struct mleaf_s *vischain;
#endif
#ifdef Q2BSPS
	//it's a q2 thing
	int			area;
	unsigned int	firstleafbrush;
	unsigned int	numleafbrushes;
	unsigned int	firstleafcmesh;
	unsigned int	numleafcmeshes;
	unsigned int	firstleafpatch;
	unsigned int	numleafpatches;
#endif
} mleaf_t;


typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLSM];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
	qboolean	hullavailable[MAX_MAP_HULLSM];
} mmodel_t;


// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct hull_s
{
	mclipnode_t	*clipnodes;
	mplane_t	*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
	int			available;
} hull_t;

void Q1BSP_CheckHullNodes(hull_t *hull);
void Q1BSP_SetModelFuncs(struct model_s *mod);
void Q1BSP_LoadBrushes(struct model_s *model);
void Q1BSP_Init(void);
void *Q1BSPX_FindLump(char *lumpname, int *lumpsize);
void Q1BSPX_Setup(struct model_s *mod, char *filebase, unsigned int filelen, lump_t *lumps, int numlumps);

typedef struct fragmentdecal_s fragmentdecal_t;
void Fragment_ClipPoly(fragmentdecal_t *dec, int numverts, float *inverts, shader_t *surfshader);
size_t Fragment_ClipPlaneToBrush(vecV_t *points, size_t maxpoints, void *planes, size_t planestride, size_t numplanes, vec4_t face);
void Mod_ClipDecal(struct model_s *mod, vec3_t center, vec3_t normal, vec3_t tangent1, vec3_t tangent2, float size, unsigned int surfflagmask, unsigned int surflagmatch, void (*callback)(void *ctx, vec3_t *fte_restrict points, size_t numpoints, shader_t *shader), void *ctx);

void Q1BSP_MarkLights (dlight_t *light, int bit, mnode_t *node);
void GLQ1BSP_LightPointValues(struct model_s *model, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir);
qboolean Q1BSP_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, struct trace_s *trace);
qbyte *Q1BSP_LeafPVS (struct model_s *model, mleaf_t *leaf, qbyte *buffer, unsigned int buffersize);

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s
{
	float	up, down, left, right;
	shader_t *shader;
	image_t *image;
} mspriteframe_t;

mspriteframe_t *R_GetSpriteFrame (entity_t *currententity);


typedef struct
{
	int				numframes;
	float			*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t	type;
	mspriteframe_t		*frameptr;
} mspriteframedesc_t;

typedef struct
{
	int					type;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	float				beamlength;		// remove?
	mspriteframedesc_t	frames[1];
} msprite_t;


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/
#if 0
typedef struct {
	int		s;
	int		t;
} mstvert_t;

typedef struct
{
	int					firstpose;
	int					numposes;
	float				interval;
	dtrivertx_t			bboxmin;
	dtrivertx_t			bboxmax;
	
	vec3_t		scale;
	vec3_t		scale_origin;

	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct
{
	dtrivertx_t			bboxmin;
	dtrivertx_t			bboxmax;
	int					frame;
} maliasgroupframedesc_t;

typedef struct
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s {
	int					xyz_index[3];
	int					st_index[3];
	
	int	pad[2];
} mtriangle_t;


#define	MAX_SKINS	32
typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;
	int					numposes;
	int					poseverts;
	int					posedata;	// numposes*poseverts trivert_t

	int					baseposedata; //original verts for triangles to reference
	int					triangles; //we need tri data for shadow volumes

	int					commands;	// gl command list with embedded s/t
	int					gl_texturenum[MAX_SKINS][4];
	int					texels[MAX_SKINS];
	maliasframedesc_t	frames[1];	// variable sized
} aliashdr_t;

#define	MAXALIASVERTS	2048
#define ALIAS_Z_CLIP_PLANE	5
#define	MAXALIASFRAMES	256
#define	MAXALIASTRIS	2048
extern	aliashdr_t	*pheader;
extern	mstvert_t	stverts[MAXALIASVERTS*2];
extern	mtriangle_t	triangles[MAXALIASTRIS];
extern	dtrivertx_t	*poseverts[MAXALIASFRAMES];

#endif


/*
========================================================================

.MD2 triangle model file format

========================================================================
*/

// LordHavoc: grabbed this from the Q2 utility source,
// renamed a things to avoid conflicts

#define MD2IDALIASHEADER		(('2'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD2ALIAS_VERSION	8
#define	MD2MAX_SKINNAME		64	//part of the format

/*
#define	MD2MAX_TRIANGLES	4096
#define MD2MAX_FRAMES		512
#define MD2MAX_VERTS		2048
#define MD2MAX_SKINS		32
// sanity checking size
#define MD2MAX_SIZE	(1024*4200)
*/
typedef struct
{
	short	s;
	short	t;
} md2stvert_t;

typedef struct 
{
	short	index_xyz[3];
	short	index_st[3];
} md2triangle_t;

typedef struct
{
	qbyte	v[3];			// scaled qbyte to fit in frame mins/maxs
	qbyte	lightnormalindex;
} md2trivertx_t;

/*
#define MD2TRIVERTX_V0   0
#define MD2TRIVERTX_V1   1
#define MD2TRIVERTX_V2   2
#define MD2TRIVERTX_LNI  3
#define MD2TRIVERTX_SIZE 4
*/

typedef struct
{
	float		scale[3];	// multiply qbyte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing
	md2trivertx_t	verts[1];	// variable sized
} md2frame_t;


// the glcmd format:
// a positive integer starts a tristrip command, followed by that many
// vertex structures.
// a negative integer starts a trifan command, followed by -x vertexes
// a zero indicates the end of the command list.
// a vertex consists of a floating point s, a floating point t,
// and an integer vertex index.


typedef struct
{
	int			ident;
	int			version;

	int			skinwidth;
	int			skinheight;
	int			framesize;		// qbyte size of each frame

	int			num_skins;
	int			num_xyz;
	int			num_st;			// greater than num_xyz for seams
	int			num_tris;
	int			num_glcmds;		// dwords in strip/fan command list
	int			num_frames;

	int			ofs_skins;		// each skin is a MAX_SKINNAME string
	int			ofs_st;			// qbyte offset from start for stverts
	int			ofs_tris;		// offset for dtriangles
	int			ofs_frames;		// offset for first frame
	int			ofs_glcmds;	
	int			ofs_end;		// end of file
} md2_t;

//#define ALIASTYPE_MDL 1
//#define ALIASTYPE_MD2 2





//===================================================================


typedef struct
{
	qbyte			ambient[3];
	qbyte			diffuse[3];
	qbyte			direction[2];
} dq3gridlight_t;

typedef struct
{
	unsigned char	ambient[4][3];
	unsigned char	diffuse[4][3];
	unsigned char	styles[4];
	unsigned char	direction[2];
} rbspgridlight_t;

//q3 based
typedef struct {
	int gridBounds[4];	//3 = 0*1
	vec3_t gridMins;
	vec3_t gridSize;
	int numlightgridelems;
//rbsp specific
	rbspgridlight_t *rbspelements;
	unsigned short *rbspindexes;
//non-rbsp specific
	dq3gridlight_t *lightgrid;

	//the reason rbsp is seperate from the non-rbsp is because it allows better memory compression.
	//I chose not to expand at loadtime because q3 would suffer from greater cache misses.
} q3lightgridinfo_t;


//
// Whole model
//

typedef enum {mod_brush, mod_sprite, mod_alias, mod_dummy, mod_halflife, mod_heightmap} modtype_t;
typedef enum {fg_quake, fg_quake2, fg_quake3, fg_halflife, fg_new, fg_doom, fg_doom3} fromgame_t;	//useful when we have very similar model types. (eg quake/halflife bsps)

#define	MF_ROCKET				(1u<<0)			// leave a trail
#define	MF_GRENADE				(1u<<1)			// leave a trail
#define	MF_GIB					(1u<<2)			// leave a trail
#define	MF_ROTATE				(1u<<3)			// rotate (bonus items)
#define	MF_TRACER				(1u<<4)			// green split trail
#define	MF_ZOMGIB				(1u<<5)			// small blood trail
#define	MF_TRACER2				(1u<<6)			// orange split trail + rotate
#define	MF_TRACER3				(1u<<7)			// purple trail

//hexen2 support.
#define  MFH2_FIREBALL			(1u<<8)			// Yellow transparent trail in all directions
#define  MFH2_ICE				(1u<<9)			// Blue-white transparent trail, with gravity
#define  MFH2_MIP_MAP			(1u<<10)		// This model has mip-maps
#define  MFH2_SPIT				(1u<<11)		// Black transparent trail with negative light
#define  MFH2_TRANSPARENT		(1u<<12)		// Transparent sprite
#define  MFH2_SPELL				(1u<<13)		// Vertical spray of particles
#define  MFH2_HOLEY				(1u<<14)		// Solid model with color 0
#define  MFH2_SPECIAL_TRANS		(1u<<15)		// Translucency through the particle table
#define  MFH2_FACE_VIEW			(1u<<16)		// Poly Model always faces you
#define  MFH2_VORP_MISSILE		(1u<<17)		// leave a trail at top and bottom of model
#define  MFH2_SET_STAFF			(1u<<18)		// slowly move up and left/right
#define  MFH2_MAGICMISSILE		(1u<<19)		// a trickle of blue/white particles with gravity
#define  MFH2_BONESHARD			(1u<<20)		// a trickle of brown particles with gravity
#define  MFH2_SCARAB			(1u<<21)		// white transparent particles with little gravity
#define  MFH2_ACIDBALL			(1u<<22)		// Green drippy acid shit
#define  MFH2_BLOODSHOT			(1u<<23)		// Blood rain shot trail
#define  MFH2_ROCKET			(1u<<31)		// spider blood (remapped from MF_ROCKET, to avoid dlight issues)

typedef union {
	struct {
		int numlinedefs;
		int numsidedefs;
		int numsectors;
	} doom;
} specificmodeltype_t;

typedef struct
{
	int walkno;
	int area[2];
	vec3_t plane;
	float dist;
	vec3_t min;
	vec3_t max;
	int numpoints;
	vec4_t *points;
} portal_t;

enum
{
	MLS_NOTLOADED,
	MLS_LOADING,
	MLS_LOADED,
	MLS_FAILED
};
typedef struct model_s
{
	char		name[MAX_QPATH];
	int			datasequence;
	int			loadstate;//MLS_
	qboolean	tainted;
	qboolean	pushdepth;		// bsp submodels have this flag set so you don't get z fighting on co-planar surfaces.

	struct model_s *submodelof;

	modtype_t	type;
	fromgame_t	fromgame;

	int			numframes;
	synctype_t	synctype;
	
	int			flags;
	int			engineflags;
	int			particleeffect;
	int			particletrail;
	int			traildefaultindex;
	struct skytris_s		*skytris;	//for surface emittance
	float					skytime;	//for surface emittance
	struct skytriblock_s	*skytrimem;

//
// volume occupied by the model graphics
//		
	vec3_t		mins, maxs;
	float		radius;
	float		clampscale;

//
// solid volume for clipping 
//
	qboolean	clipbox;
	vec3_t		clipmins, clipmaxs;

//
// brush model
//
	int			firstmodelsurface, nummodelsurfaces;

	int			numsubmodels;
	mmodel_t	*submodels;

	int			numplanes;
	mplane_t	*planes;

	int			numclusters;
	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;
	vec3_t		*normals;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	mnode_t		*nodes;
	void		*cnodes;
	mnode_t		*rootnode;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			numclipnodes;
	mclipnode_t	*clipnodes;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		hulls[MAX_MAP_HULLSM];

	int			numtextures;
	texture_t	**textures;

	qbyte		*pvs, *phs;			// fully expanded and decompressed
	qbyte		*visdata;
	void	*vis;
	qbyte		*lightdata;
	qbyte		*deluxdata;
	unsigned	lightdatasize;
	q3lightgridinfo_t *lightgrid;
	mfog_t		*fogs;
	int			numfogs;
	char		*entities;
	int			entitiescrc;

	struct doll_s		*dollinfo;
	shader_t	*simpleskin[4];

	struct {
		texture_t *tex;
		vbo_t *vbo;
	} *shadowbatches;
	int numshadowbatches;
	vbo_t *vbos;
	void *terrain;
	batch_t *batches[SHADER_SORT_COUNT];
	unsigned int numbatches;
	struct
	{
		int first;				//once built...
		int count;				//num lightmaps
		int	merge;				//merge this many source lightmaps together. woo.
		int width;				//x size of lightmaps
		int height;				//y size of lightmaps
		int surfstyles;			//numbers of style per surface.
		qboolean deluxemapping;	//lightmaps are interleaved with deluxemap data (lightmap indicies should only be even values)
	} lightmaps;

	unsigned	checksum;
	unsigned	checksum2;

	portal_t *portal;
	unsigned int numportals;

	modelfuncs_t	funcs;
//
// additional model data
//
	void *meshinfo;	//data allocated within the memgroup allocations, will be nulled out when the model is flushed
	zonegroup_t memgroup;
} model_t;

#define MDLF_EMITREPLACE     0x0001 // particle effect engulphs model (don't draw)
#define MDLF_EMITFORWARDS    0x0002
#define MDLF_NODEFAULTTRAIL  0x0004
#define MDLF_RGBLIGHTING     0x0008
#define MDLF_PLAYER          0x0010 // players have specific lighting values
#define MDLF_FLAME           0x0020 // can be excluded with r_drawflame, fullbright render hack
#define MDLF_DOCRC           0x0040 // model needs CRC built
#define MDLF_NEEDOVERBRIGHT  0x0080 // only overbright these models with gl_overbright_all set
#define MDLF_BOLT            0x0100 // doesn't produce shadows
#define	MDLF_NOTREPLACEMENTS 0x0200 // can be considered a cheat, disable texture replacements
#define MDLF_EZQUAKEFBCHEAT  0x0400 // this is a blatent cheat, one that can disadvantage us fairly significantly if we don't support it.
#define MDLF_HASBRUSHES		 0x0800 // q1bsp has brush info for more precise traceboxes
#define MDLF_RECALCULATERAIN 0x1000 // particles changed, recalculate any sky polys

//============================================================================
#endif	// __MODEL__


float RadiusFromBounds (vec3_t mins, vec3_t maxs);


//
// gl_heightmap.c
//
#ifdef TERRAIN
void Terr_Init(void);
struct terrainfuncs_s;
struct terrainfuncs_s *QDECL Terr_GetTerrainFuncs(void);
void Terr_DrawTerrainModel (batch_t **batch, entity_t *e);
void Terr_FreeModel(model_t *mod);
void Terr_FinishTerrain(model_t *model);
void Terr_PurgeTerrainModel(model_t *hm, qboolean lightmapsonly, qboolean lightmapreusable);
void *Mod_LoadTerrainInfo(model_t *mod, char *loadname, qboolean force);	//call this after loading a bsp
qboolean Terrain_LocateSection(char *name, flocation_t *loc);	//used on servers to generate sections for download.
qboolean Heightmap_Trace(model_t *model, int forcehullnum, int frame, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int contentmask, struct trace_s *trace);
unsigned int Heightmap_PointContents(model_t *model, vec3_t axis[3], vec3_t org);
struct fragmentdecal_s;
void Terrain_ClipDecal(struct fragmentdecal_s *dec, float *center, float radius, model_t *model);
qboolean Terr_DownloadedSection(char *fname);

void CL_Parse_BrushEdit(void);
qboolean SV_Parse_BrushEdit(void);
qboolean SV_Prespawn_Brushes(sizebuf_t *msg, unsigned int *modelindex, unsigned int *lastid);
#endif





qboolean Heightmap_Edit(model_t *mod, int action, float *pos, float radius, float quant);


#ifdef Q2BSPS

void CM_InitBoxHull (void);

#ifdef __cplusplus
//#pragma warningmsg ("                  c++ stinks")
#else

void CM_Init(void);

qboolean	CM_SetAreaPortalState (struct model_s *mod, int portalnum, qboolean open);
qboolean	CM_HeadnodeVisible (struct model_s *mod, int nodenum, qbyte *visbits);
qboolean	VARGS CM_AreasConnected (struct model_s *mod, unsigned int area1, unsigned int area2);
int		CM_NumClusters (struct model_s *mod);
int		CM_ClusterSize (struct model_s *mod);
int		CM_LeafContents (struct model_s *mod, int leafnum);
int		CM_LeafCluster (struct model_s *mod, int leafnum);
int		CM_LeafArea (struct model_s *mod, int leafnum);
int		CM_WriteAreaBits (struct model_s *mod, qbyte *buffer, int area, qboolean merge);
int		CM_PointLeafnum (struct model_s *mod, vec3_t p);
qbyte	*CM_ClusterPVS (struct model_s *mod, int cluster, qbyte *buffer, unsigned int buffersize);
qbyte	*CM_ClusterPHS (struct model_s *mod, int cluster);
int		CM_BoxLeafnums (struct model_s *mod, vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode);
int		CM_PointContents (struct model_s *mod, vec3_t p);
int		CM_TransformedPointContents (struct model_s *mod, vec3_t p, int headnode, vec3_t origin, vec3_t angles);
int		CM_HeadnodeForBox (struct model_s *mod, vec3_t mins, vec3_t maxs);
//struct trace_s	CM_TransformedBoxTrace (struct model_s *mod, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int brushmask, vec3_t origin, vec3_t angles);
struct model_s *CM_TempBoxModel(vec3_t mins, vec3_t maxs);

void	CMQ2_SetAreaPortalState (model_t *mod, unsigned int portalnum, qboolean open);
void	CMQ3_SetAreaPortalState (model_t *mod, unsigned int area1, unsigned int area2, qboolean open);
#endif



#endif	//Q2BSPS
