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


// upper design bounds

#define	MAX_MAP_HULLSDQ1	4
#define	MAX_MAP_HULLSDH2	8
#define	MAX_MAP_HULLSM		16

#define	MAX_MAP_MODELS		256
#define	MAX_MAP_BRUSHES		0x8000
#define	MAX_MAP_ENTITIES	1024
#define	MAX_MAP_ENTSTRING	65536

#define	MAX_MAP_PLANES		65636*2
#define	MAX_MAP_NODES		65535		// q2/q3 uses more than q1. :(
#define	MAX_MAP_CLIPNODES	65535		//
#define	MAX_MAP_LEAFS		65535		// 
#define	MAX_MAP_VERTS		65535
#define	MAX_MAP_FACES		65535
#define	MAX_MAP_MARKSURFACES 65535
#define	MAX_MAP_TEXINFO		4096
#define	MAX_MAP_EDGES		256000
#define	MAX_MAP_SURFEDGES	512000
#define	MAX_MAP_MIPTEX		0x200000
#define	MAX_MAP_LIGHTING	0x100000
#define	MAX_MAP_VISIBILITY	0x200000

// key / value pair sizes

#define	MAX_KEY		32
#define	MAX_VALUE	1024


//=============================================================================

#define BSPVERSIONPREREL 28
#define BSPVERSION	29
//HalfLife support
#define BSPVERSIONHL	30

typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES	0
#define	LUMP_PLANES		1
#define	LUMP_TEXTURES	2
#define	LUMP_VERTEXES	3
#define	LUMP_VISIBILITY	4
#define	LUMP_NODES		5
#define	LUMP_TEXINFO	6
#define	LUMP_FACES		7
#define	LUMP_LIGHTING	8
#define	LUMP_CLIPNODES	9
#define	LUMP_LEAFS		10
#define	LUMP_MARKSURFACES 11
#define	LUMP_EDGES		12
#define	LUMP_SURFEDGES	13
#define	LUMP_MODELS		14

#define	HEADER_LUMPS	15

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLSDQ1];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} dq1model_t;

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLSDH2];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} dh2model_t;

typedef struct
{
	int			version;	
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	int			nummiptex;
	int			dataofs[4];		// [nummiptex]
} dmiptexlump_t;

#define	MIPLEVELS	4
typedef struct miptex_s
{
	char		name[16];
	unsigned	width, height;
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
} miptex_t;


typedef struct
{
	float	point[3];
} dvertex_t;


// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

typedef struct
{
	float	normal[3];
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;



#define	Q1CONTENTS_EMPTY	-1
#define	Q1CONTENTS_SOLID	-2
#define	Q1CONTENTS_WATER	-3
#define	Q1CONTENTS_SLIME	-4
#define	Q1CONTENTS_LAVA		-5
#define	Q1CONTENTS_SKY		-6

#define	FTECONTENTS_EMPTY	0
#define	FTECONTENTS_SOLID	1
#define	FTECONTENTS_WATER	2
#define	FTECONTENTS_SLIME	4
#define	FTECONTENTS_LAVA	8
#define	FTECONTENTS_SKY		16
#define FTECONTENTS_LADDER	32
#define FTECONTENTS_FLUID	(FTECONTENTS_WATER|FTECONTENTS_SLIME|FTECONTENTS_LAVA|FTECONTENTS_SKY)	//sky is a fluid for q1 code.

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
	int			planenum;
	short		children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for sphere culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} dnode_t;

typedef struct
{
	int			planenum;
	short		children[2];	// negative numbers are contents
} dclipnode_t;


typedef struct texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			miptex;
	int			flags;
} texinfo_t;
#define	TEX_SPECIAL		1		// sky or slime, no lightmap or 256 subdivision

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
	unsigned short	v[2];		// vertex numbers
} dedge_t;

#define	MAXLIGHTMAPS	4
typedef struct
{
	short		planenum;
	short		side;

	int			firstedge;		// we must support > 64k edges
	short		numedges;	
	short		texinfo;

// lighting info
	qbyte		styles[MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} dface_t;



#define	AMBIENT_WATER	0
#define	AMBIENT_SKY		1
#define	AMBIENT_SLIME	2
#define	AMBIENT_LAVA	3

#define	NUM_AMBIENTS			4		// automatic ambient sounds
#define NUM_MUSICS				1

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct
{
	int			contents;
	int			visofs;				// -1 = no visibility info

	short		mins[3];			// for frustum culling
	short		maxs[3];

	unsigned short		firstmarksurface;
	unsigned short		nummarksurfaces;

	qbyte		ambient_level[NUM_AMBIENTS];
} dleaf_t;

//============================================================================

#ifndef QUAKE_GAME

// the utilities get to be lazy and just use large static arrays

extern	int			nummodels;
extern	dmodel_t	dmodels[MAX_MAP_MODELS];

extern	int			visdatasize;
extern	qbyte		dvisdata[MAX_MAP_VISIBILITY];

extern	int			lightdatasize;
extern	qbyte		dlightdata[MAX_MAP_LIGHTING];

extern	int			texdatasize;
extern	qbyte		dtexdata[MAX_MAP_MIPTEX]; // (dmiptexlump_t)

extern	int			entdatasize;
extern	char		dentdata[MAX_MAP_ENTSTRING];

extern	int			numleafs;
extern	dleaf_t		dleafs[MAX_MAP_LEAFS];

extern	int			numplanes;
extern	dplane_t	dplanes[MAX_MAP_PLANES];

extern	int			numvertexes;
extern	dvertex_t	dvertexes[MAX_MAP_VERTS];

extern	int			numnodes;
extern	dnode_t		dnodes[MAX_MAP_NODES];

extern	int			numtexinfo;
extern	texinfo_t	texinfo[MAX_MAP_TEXINFO];

extern	int			numfaces;
extern	dface_t		dfaces[MAX_MAP_FACES];

extern	int			numclipnodes;
extern	dclipnode_t	dclipnodes[MAX_MAP_CLIPNODES];

extern	int			numedges;
extern	dedge_t		dedges[MAX_MAP_EDGES];

extern	int			nummarksurfaces;
extern	unsigned short	dmarksurfaces[MAX_MAP_MARKSURFACES];

extern	int			numsurfedges;
extern	int			dsurfedges[MAX_MAP_SURFEDGES];



void	LoadBSPFile (char *filename);
void	WriteBSPFile (char *filename);
void	PrintBSPFileSizes (void);

#endif
















#define	MIPLEVELS	4
typedef struct q2miptex_s
{
	char		name[32];
	unsigned	width, height;
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
	char		animname[32];			// next frame in animation chain
	int			flags;
	int			contents;
	int			value;
} q2miptex_t;



/*
==============================================================================

  .BSP file format

==============================================================================
*/

#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
		// little-endian "IBSP"

#define Q2BSPVERSION	38
#define Q3BSPVERSION	46


// upper design bounds
// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits
#define	MAX_Q2MAP_MODELS		1024
#define	MAX_Q2MAP_BRUSHES		MAX_MAP_BRUSHES
#define	MAX_Q2MAP_ENTITIES	2048

#define	MAX_Q2MAP_AREAS		256
#define	MAX_Q2MAP_AREAPORTALS	1024
#define	MAX_Q2MAP_PLANES		MAX_MAP_PLANES
#define	MAX_Q2MAP_BRUSHSIDES	0x40000
#define	MAX_Q2MAP_VERTS		MAX_MAP_VERTS
#define	MAX_Q2MAP_FACES		MAX_MAP_FACES
#define	MAX_Q2MAP_LEAFFACES	65536
#define	MAX_Q2MAP_LEAFBRUSHES 65536
#define	MAX_Q2MAP_PORTALS		65536
#define	MAX_Q2MAP_EDGES		128000
#define	MAX_Q2MAP_SURFEDGES	256000
#define	MAX_Q2MAP_LIGHTING	0x200000
#define	MAX_Q2MAP_VISIBILITY	MAX_MAP_VISIBILITY

// key / value pair sizes

#define	MAX_KEY		32
#define	MAX_VALUE	1024

//=============================================================================


#define	Q2LUMP_ENTITIES		0
#define	Q2LUMP_PLANES			1
#define	Q2LUMP_VERTEXES		2
#define	Q2LUMP_VISIBILITY		3
#define	Q2LUMP_NODES			4
#define	Q2LUMP_TEXINFO		5
#define	Q2LUMP_FACES			6
#define	Q2LUMP_LIGHTING		7
#define	Q2LUMP_LEAFS			8
#define	Q2LUMP_LEAFFACES		9
#define	Q2LUMP_LEAFBRUSHES	10
#define	Q2LUMP_EDGES			11
#define	Q2LUMP_SURFEDGES		12
#define	Q2LUMP_MODELS			13
#define	Q2LUMP_BRUSHES		14
#define	Q2LUMP_BRUSHSIDES		15
#define	Q2LUMP_POP			16
#define	Q2LUMP_AREAS			17
#define	Q2LUMP_AREAPORTALS	18
#define	Q2HEADER_LUMPS		19

enum Q3LUMP
{	
	Q3LUMP_ENTITIES		=0,
	Q3LUMP_SHADERS		=1,
	Q3LUMP_PLANES		=2,
	Q3LUMP_NODES		=3,
	Q3LUMP_LEAFS		=4,
	Q3LUMP_LEAFSURFACES	=5,
	Q3LUMP_LEAFBRUSHES	=6,
	Q3LUMP_MODELS		=7,
	Q3LUMP_BRUSHES		=8,
	Q3LUMP_BRUSHSIDES	=9,
	Q3LUMP_DRAWVERTS	=10,
	Q3LUMP_DRAWINDEXES	=11,
	Q3LUMP_FOGS			=12,
	Q3LUMP_SURFACES		=13,
	Q3LUMP_LIGHTMAPS	=14,
	Q3LUMP_LIGHTGRID	=15,
	Q3LUMP_VISIBILITY	=16,
	RBSPLUMP_LIGHTINDEXES=17,
	Q3LUMPS_TOTAL
};

typedef struct
{
	int ident;
	int			version;	
	lump_t		lumps[Q2HEADER_LUMPS];
} q2dheader_t;

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];		// for sounds or lights
	int			headnode;
	int			firstface, numfaces;	// submodels just draw faces
										// without walking the bsp tree
} q2dmodel_t;

typedef struct
{
	float mins[3];
	float maxs[3];
	int firstsurface;
	int num_surfaces;
	int firstbrush;
	int num_brushes;
} q3dmodel_t;



// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5




// contents flags are seperate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

// these definitions also need to be in q_shared.h!

// lower bits are stronger, and will eat weaker brushes completely
#define	Q2CONTENTS_SOLID		1		// an eye is never valid in a solid
#define	Q2CONTENTS_WINDOW		2		// translucent, but not watery
#define	Q2CONTENTS_AUX			4
#define	Q2CONTENTS_LAVA			8
#define	Q2CONTENTS_SLIME		16
#define	Q2CONTENTS_WATER		32
#define	Q2CONTENTS_MIST			64
#define	Q2LAST_VISIBLE_CONTENTS	64

// remaining contents are non-visible, and don't eat brushes

#define	Q2CONTENTS_AREAPORTAL		0x8000

#define	Q2CONTENTS_PLAYERCLIP		0x10000
#define	Q2CONTENTS_MONSTERCLIP	0x20000

// currents can be added to any other contents, and may be mixed
#define	Q2CONTENTS_CURRENT_0		0x40000
#define	Q2CONTENTS_CURRENT_90		0x80000
#define	Q2CONTENTS_CURRENT_180	0x100000
#define	Q2CONTENTS_CURRENT_270	0x200000
#define	Q2CONTENTS_CURRENT_UP		0x400000
#define	Q2CONTENTS_CURRENT_DOWN	0x800000

#define	Q2CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	Q2CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define	Q2CONTENTS_DEADMONSTER	0x4000000
#define	Q2CONTENTS_DETAIL			0x8000000	// brushes to be added after vis leafs
#define	Q2CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define Q3CONTENTS_TRANSLUCENT	0x20000000
#define	Q2CONTENTS_LADDER			0x20000000



#define	SURF_LIGHT		0x1		// value will hold the light strength

#define	SURF_SLICK		0x2		// effects game physics

#define	SURF_SKY		0x4		// don't draw, but add to skybox
#define	SURF_WARP		0x8		// turbulent water warp
#define	SURF_TRANS33	0x10
#define	SURF_TRANS66	0x20
#define	SURF_FLOWING	0x40	// scroll towards angle
#define	SURF_NODRAW		0x80	// don't bother referencing the texture

#define	SURF_NODRAW		0x80	// don't bother referencing the texture

#define Q3SURF_LADDER	0x8		//wee

// content masks
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(Q2CONTENTS_SOLID|Q2CONTENTS_WINDOW)
#define	MASK_PLAYERSOLID		(Q2CONTENTS_SOLID|Q2CONTENTS_PLAYERCLIP|Q2CONTENTS_WINDOW|Q2CONTENTS_MONSTER)
#define	MASK_DEADSOLID			(Q2CONTENTS_SOLID|Q2CONTENTS_PLAYERCLIP|Q2CONTENTS_WINDOW)
#define	MASK_MONSTERSOLID		(Q2CONTENTS_SOLID|Q2CONTENTS_MONSTERCLIP|Q2CONTENTS_WINDOW|Q2CONTENTS_MONSTER)
#define	MASK_WATER				(Q2CONTENTS_WATER|Q2CONTENTS_LAVA|Q2CONTENTS_SLIME)
#define	MASK_OPAQUE				(Q2CONTENTS_SOLID|Q2CONTENTS_SLIME|Q2CONTENTS_LAVA)
#define	MASK_SHOT				(Q2CONTENTS_SOLID|Q2CONTENTS_MONSTER|Q2CONTENTS_WINDOW|Q2CONTENTS_DEADMONSTER)
#define MASK_CURRENT			(Q2CONTENTS_CURRENT_0|Q2CONTENTS_CURRENT_90|Q2CONTENTS_CURRENT_180|Q2CONTENTS_CURRENT_270|Q2CONTENTS_CURRENT_UP|Q2CONTENTS_CURRENT_DOWN)



typedef struct
{
	int			planenum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for frustom culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} q2dnode_t;

typedef struct
{
	int plane;
	int children[2];
	int mins[3];
	int maxs[3];
} q3dnode_t;


typedef struct q2texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			flags;			// miptex flags + overrides
	int			value;			// light emission, etc
	char		texture[32];	// texture name (textures/ *.wal)
	int			nexttexinfo;	// for animations, -1 = end of chain
} q2texinfo_t;



typedef struct
{
	int				contents;			// OR of all brushes (not needed?)

	short			cluster;
	short			area;

	short			mins[3];			// for frustum culling
	short			maxs[3];

	unsigned short	firstleafface;
	unsigned short	numleaffaces;

	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;
} q2dleaf_t;

typedef struct
{
	int cluster;
	int area;
	int mins[3];
	int maxs[3];
	int firstleafsurface;
	int num_leafsurfaces;
	int firstleafbrush;
	int num_leafbrushes;
} q3dleaf_t;


typedef struct
{
	unsigned short	planenum;		// facing out of the leaf
	short	texinfo;
} q2dbrushside_t;

typedef struct
{
	int planenum;
	int texinfo;
} q3dbrushside_t;
typedef struct
{
	int planenum;
	int texinfo;
	int facenum;
} rbspbrushside_t;

typedef struct
{
	int			firstside;
	int			numsides;
	int			contents;
} q2dbrush_t;


typedef struct
{
	int firstside;
	int num_sides;
	int shadernum;
} q3dbrush_t;

#define	ANGLE_UP	-1
#define	ANGLE_DOWN	-2


// the visibility lump consists of a header with a count, then
// qbyte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define	DVIS_PVS	0
#define	DVIS_PHS	1
typedef struct
{
	int			numclusters;
	int			bitofs[8][2];	// bitofs[numclusters][2]
} q2dvis_t;

typedef struct
{
	int				numclusters;
	int				rowsize;
	unsigned char	data[1];
} q3dvis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct
{
	int		portalnum;
	int		otherarea;
} q2dareaportal_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
} q2darea_t;



















typedef struct
{
	char shadername[64];
	int surfflags;
	int contents;
} dq3shader_t;

typedef struct
{
	float n[3];
	float d;
} Q3PLANE_t;

struct Q3MODEL
{
	float mins[3];
	float maxs[3];
	int firstsurface;
	int num_surfaces;
	int firstbrush;
	int num_brushes;
};


typedef struct
{
	float point[3];
	float texcoords[2][2];
	float normal[3];
	unsigned char color[4];
} q3dvertex_t;

typedef struct
{
	float point[3];
	float texcoords[5][2];
	float normal[3];
	unsigned char color[4][4];
} rbspvertex_t;

struct Q3FOG
{
	char shadername[64] ; 	
	int brushnum;
	int visibleside;
};

enum q3surfacetype
{
	MST_BAD=0,
	MST_PLANAR=1,
	MST_PATCH=2,
	MST_TRIANGLE_SOUP=3,
	MST_FLARE=4	
};

typedef struct
{
	int shadernum;
	int fognum;
	int facetype;
	int firstvertex;
	int num_vertices;
	int firstindex;
	int num_indexes;
	int lightmapnum;
	int lightmap_x; 
	int lightmap_y;
	int lightmap_width;
	int lightmap_height;
	float lightmap_origin[3];
	float lightmap_vecs[2][3];
	float normal[3];
	int patchwidth;
	int patchheight;
} q3dface_t;

typedef struct
{
	int shadernum;
	int fognum;
	int facetype;
	int firstvertex;
	int num_vertices;
	int firstindex;
	int num_indexes;
	unsigned char lm_styles[4];
	unsigned char vt_styles[4];
	int lightmapnum[4];
	int lightmap_offs[2][4]; 
	int lightmap_width;
	int lightmap_height;
	float lightmap_origin[3];
	float lightmap_vecs[2][3];
	float normal[3];
	int patchwidth;
	int patchheight;
} rbspface_t;
