#include "quakedef.h"

#ifdef TERRAIN
#include "glquake.h"
#include "shader.h"

#include "pr_common.h"

//#define STRICTEDGES	//strict (ugly) grid
#define TERRAINTHICKNESS 16
#define TERRAINACTIVESECTIONS 3000

/*
a note on networking:
By default terrain is NOT networked. This means content is loaded without networking delays.
If you wish to edit the terrain collaboratively, you can enable the mod_terrain_networked cvar.
When set, changes on the server will notify clients that a section has changed, and the client will reload it as needed.
Changes on the client WILL NOT notify the server, and will get clobbered if the change is also made on the server.
This means for editing purposes, you MUST funnel it via ssqc with your own permission checks.
It also means for explosions and local stuff, the server will merely restate changes from impacts if you do them early. BUT DO NOT CALL THE EDIT FUNCTION IF THE SERVER HAS ALREADY APPLIED THE CHANGE.
*/
cvar_t mod_terrain_networked = CVARD("mod_terrain_networked", "0", "Terrain edits are networked. Clients will download sections on demand, and servers will notify clients of changes.");
cvar_t mod_terrain_defaulttexture = CVARD("mod_terrain_defaulttexture", "", "Newly created terrain tiles will use this texture. This should generally be updated by the terrain editor.");
cvar_t mod_terrain_savever = CVARD("mod_terrain_savever", "", "Which terrain section version to write if terrain was edited.");

/*
terminology:
tile:
	a single grid tile of 2*2 height samples.
	iterrated for collisions but otherwise unused.
section:
	16*16 tiles, with a single texture spread over them.
	samples have an overlap with the neighbouring section (so 17*17 height samples). texture samples do not quite match height frequency (63*63 vs 16*16).
	smallest unit for culling.
block:
	16*16 sections. forms a single disk file. used only to avoid 16777216 files in a single directory, instead getting 65536 files for a single fully populated map... much smaller...
	each block file is about 4mb each. larger can be detrimental to automatic downloads.
cluster:
	64*64 sections
	internal concept to avoid a single pointer array of 16 million entries per terrain.
*/

int Surf_NewLightmaps(int count, int width, int height, qboolean deluxe);

#define MAXCLUSTERS 64
#define MAXSECTIONS 64	//this many sections within each cluster in each direction
#define SECTHEIGHTSIZE 17 //this many height samples per section
#define SECTTEXSIZE 64	//this many texture samples per section
#define SECTIONSPERBLOCK 16

//each section is this many sections higher in world space, to keep the middle centered at '0 0'
#define CHUNKBIAS	(MAXCLUSTERS*MAXSECTIONS/2)
#define CHUNKLIMIT	(MAXCLUSTERS*MAXSECTIONS)

#define LMCHUNKS 8//(LMBLOCK_WIDTH/SECTTEXSIZE)

#define HMLMSTRIDE (LMCHUNKS*SECTTEXSIZE)

#define SECTION_MAGIC (*(int*)"HMMS")
#define SECTION_VER_DEFAULT	1
/*simple version history:
ver=0
	SECTHEIGHTSIZE=16
ver=1
	SECTHEIGHTSIZE=17 (oops, makes holes more usable)
	(holes in this format are no longer supported)
ver=2
	uses deltas instead of absolute values
	variable length image names
*/

#define TGS_NOLOAD			0
#define TGS_LAZYLOAD		1	//see if its available, if not, queue it. don't create too much work at once. peace man
#define TGS_TRYLOAD			2	//try and get it, but don't stress if its not available yet
#define TGS_WAITLOAD		4	//load it, wait for it if needed.
#define TGS_ANYSTATE		8	//returns the section regardless of its current state, even if its loading.
#define TGS_NODOWNLOAD		16	//don't queue it for download
#define TGS_NORENDER		32	//don't upload any textures or whatever
#define TGS_DEFAULTONFAIL	64	//if it failed to load, generate a default anyway

enum
{
	//these flags can be found on disk
	TSF_HASWATER_V0	= 1u<<0,	//no longer flagged.
	TSF_HASCOLOURS	= 1u<<1,
	TSF_HASHEIGHTS	= 1u<<2,
	TSF_HASSHADOW	= 1u<<3,

	//these flags are found only on disk
	TSF_COMPRESSED	= 1u<<31,

	//these flags should not be found on disk
	TSF_NOTIFY		= 1u<<28,	//modified on server, waiting for clients to be told about the change.
	TSF_RELIGHT		= 1u<<29,	//height edited, needs relighting.
	TSF_DIRTY		= 1u<<30,	//its heightmap has changed, the mesh needs rebuilding
	TSF_EDITED		= 1u<<31	//says it needs to be written if saved

#define TSF_INTERNAL	(TSF_RELIGHT|TSF_DIRTY|TSF_EDITED|TSF_NOTIFY)
};
enum
{
	TMF_SCALE	= 1u<<0,
	//what else do we want? alpha? colormod perhaps?
};

typedef struct
{
	int size;
	vec3_t axisorg[4];
	float scale;
	int reserved3;
	int reserved2;
	int reserved1;
	//char modelname[1+];
} dsmesh_v1_t;

typedef struct
{
	unsigned int flags;
	char texname[4][32];
	unsigned int texmap[SECTTEXSIZE][SECTTEXSIZE];
	float heights[SECTHEIGHTSIZE*SECTHEIGHTSIZE];
	unsigned short holes;
	unsigned short reserved0;
	float waterheight;
	float minh;
	float maxh;
	int ents_num;
	int reserved1;
	int reserved4;
	int reserved3;
	int reserved2;
} dsection_v1_t;

//file header for a single section
typedef struct
{
	int magic;
	int ver;
} dsection_t;

//file header for a block of sections.
//(because 16777216 files in a single directory is a bad plan. windows really doesn't like it.)
typedef struct
{
	//a block is a X*Y group of sections
	//if offset==0, the section isn't present.
	//the data length of the section preceeds the actual data.
	int magic;
	int ver;
	unsigned int offset[SECTIONSPERBLOCK*SECTIONSPERBLOCK];
} dblock_t;

typedef struct hmpolyset_s
{
	struct hmpolyset_s *next;
	shader_t *shader;
	mesh_t mesh;
	mesh_t *amesh;
	vbo_t vbo;
} hmpolyset_t;
struct hmwater_s
{
	struct hmwater_s *next;
	unsigned int contentmask;
	qboolean simple;	//no holes, one height
	float minheight;
	float maxheight;
	char shadername[MAX_QPATH];
	shader_t *shader;
	qbyte holes[8];
	float heights[9*9];
};
enum
{
	TSLS_NOTLOADED,
	TSLS_LOADING1,	//section is queued to the worker (and may be loaded as part of another section)
	TSLS_LOADING2,	//waiting for main thread to finish, worker will ignore
	TSLS_LOADED,
	TSLS_FAILED
};
typedef struct
{
	link_t recycle;
	int sx, sy;

	int loadstate;

	float heights[SECTHEIGHTSIZE*SECTHEIGHTSIZE];
	unsigned char holes[8];
	unsigned int flags;
	float maxh_cull;	//includes water+mesh heights
	float minh, maxh;
	struct heightmap_s *hmmod;

	struct hmwater_s *water;

	size_t traceseq;

#ifndef SERVERONLY
	pvscache_t pvscache;
	vec4_t colours[SECTHEIGHTSIZE*SECTHEIGHTSIZE];	//FIXME: make bytes
	char texname[4][MAX_QPATH];
	int lightmap;
	int lmx, lmy;

	texnums_t textures;
	vbo_t vbo;
	mesh_t mesh;
	mesh_t *amesh;

	hmpolyset_t *polys;
#endif
	int numents;
	int maxents;
	struct hmentity_s **ents;
} hmsection_t;
typedef struct
{
	hmsection_t *section[MAXSECTIONS*MAXSECTIONS];
} hmcluster_t;
typedef struct brushtex_s
{
	char shadername[MAX_QPATH];
	shader_t *shader;
	vbo_t vbo;
	mesh_t mesh;
	mesh_t *pmesh;
	qboolean rebuild;
//	struct
//	{
//		unsigned int brush;
//		unsigned short face;
//	} *faces;
//	int numfaces;
	struct brushtex_s *next;
} brushtex_t;
typedef struct
{
	unsigned int contents;
	size_t	numplanes;
	vec4_t	*planes;
	struct brushface_s
	{
		brushtex_t *tex;
		vec4_t sdir;
		vec4_t tdir;
	} *faces;
} brushes_t;
typedef struct heightmap_s
{
	char path[MAX_QPATH];
	char skyname[MAX_QPATH];
	char groundshadername[MAX_QPATH];
	char defaultwatershader[MAX_QPATH];	//typically the name of the ocean or whatever.
	float defaultwaterheight;
	float defaultgroundheight;
	char defaultgroundtexture[MAX_QPATH];
	int firstsegx, firstsegy;
	int maxsegx, maxsegy; //tex/cull sections
	float sectionsize;	//each section is this big, in world coords
	hmcluster_t *cluster[MAXCLUSTERS*MAXCLUSTERS];
	shader_t *skyshader;
	shader_t *shader;
	mesh_t skymesh;
	mesh_t *askymesh;
	unsigned int exteriorcontents;
	unsigned int loadingsections;	//number of sections currently being loaded. avoid loading extras while non-zero.
	size_t traceseq;
	size_t drawnframe;
	enum
	{
		DGT_SOLID,	//invalid/new areas should be completely solid until painted.
		DGT_HOLES,	//invalid/new sections should be non-solid+invisible
		DGT_FLAT	//invalid/new sections should be filled with ground by default
	} defaultgroundtype;
	enum
	{
		HMM_TERRAIN,
		HMM_BLOCKS
	} mode;
	int tilecount[2];
	int tilepixcount[2];

	int activesections;
	link_t recycle;		//section list in lru order
//	link_t collected;	//memory that may be reused, to avoid excess reallocs.

	struct hmentity_s
	{
		size_t drawnframe;	//don't add it to the scene multiple times.
		size_t traceseq;	//don't trace through this entity multiple times if its in different sections.
		int refs;		//entity is free/reusable when its no longer referenced by any sections
		entity_t ent;

		struct hmentity_s *next;	//used for freeing/allocating an entity
	} *entities;
	void *entitylock;	//lock this if you're going to read/write entities of any kind.

#ifndef SERVERONLY
	unsigned int numusedlmsects;	//to track leaks and stats
	unsigned int numunusedlmsects;
	struct lmsect_s
	{
		struct lmsect_s *next;
		int lm, x, y;
	} *unusedlmsects;
#endif

#ifndef SERVERONLY
	//I'm putting this here because we might have some quite expensive lighting routines going on
	//and that'll make editing the terrain jerky as fook, so relighting it a few texels at a time will help maintain a framerate while editing
	hmsection_t *relight;
	unsigned int relightidx;
	vec2_t relightmin;
#endif





	brushtex_t *brushtextures;
	brushes_t *wbrushes;
	int numbrushes;
} heightmap_t;

#ifndef SERVERONLY
static void ted_dorelight(heightmap_t *hm);
#endif
static qboolean Terr_Collect(heightmap_t *hm);
static hmsection_t *Terr_GetSection(heightmap_t *hm, int x, int y, unsigned int flags);
static void Terr_LoadSectionWorker(void *ctx, void *data, size_t a, size_t b);
static void Terr_WorkerLoadedSectionLightmap(void *ctx, void *data, size_t a, size_t b);
static void Terr_WorkerLoadedSection(void *ctx, void *data, size_t a, size_t b);
static void Terr_WorkerFailedSection(void *ctx, void *data, size_t a, size_t b);

void Terr_Brush_Draw(heightmap_t *hm, batch_t **batches, entity_t *e);

#ifndef SERVERONLY
static texid_t Terr_LoadTexture(char *name)
{
	extern texid_t missing_texture;
	texid_t id;
	if (*name)
	{
		id = R_LoadHiResTexture(name, NULL, 0);
		if (!TEXVALID(id))
		{
			id = missing_texture;
			Con_Printf("Unable to load texture %s\n", name);
		}
	}
	else
		id = missing_texture;
	return id;
}
#endif

static void Terr_LoadSectionTextures(hmsection_t *s)
{
#ifndef SERVERONLY
	extern texid_t missing_texture;
	struct hmwater_s *w;
	if (isDedicated)
		return;
	//CL_CheckOrEnqueDownloadFile(s->texname[0], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[1], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[2], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[3], NULL, 0);
	switch(s->hmmod->mode)
	{
	case HMM_BLOCKS:
		s->textures.base			= Terr_LoadTexture(va("maps/%s/atlas.tga", s->hmmod->path));
		s->textures.fullbright		= Terr_LoadTexture(va("maps/%s/atlas_luma.tga", s->hmmod->path));
		s->textures.bump			= Terr_LoadTexture(va("maps/%s/atlas_norm.tga", s->hmmod->path));
		s->textures.specular		= Terr_LoadTexture(va("maps/%s/atlas_spec.tga", s->hmmod->path));
		s->textures.upperoverlay	= missing_texture;
		s->textures.loweroverlay	= missing_texture;
		break;
	case HMM_TERRAIN:
		s->textures.base			= Terr_LoadTexture(s->texname[0]);
		s->textures.upperoverlay	= Terr_LoadTexture(s->texname[1]);
		s->textures.loweroverlay	= Terr_LoadTexture(s->texname[2]);
		s->textures.fullbright		= Terr_LoadTexture(s->texname[3]);
		s->textures.bump			= *s->texname[0]?R_LoadHiResTexture(va("%s_norm", s->texname[0]), NULL, 0):r_nulltex;
		s->textures.specular		= *s->texname[0]?R_LoadHiResTexture(va("%s_spec", s->texname[0]), NULL, 0):r_nulltex;
		break;
	}
	for (w = s->water; w; w = w->next)
	{
		w->shader = R_RegisterCustom (w->shadername, SUF_NONE, Shader_DefaultWaterShader, NULL);
		R_BuildDefaultTexnums(NULL, w->shader);	//this might get expensive. hideously so.
	}
#endif
}

#ifndef SERVERONLY
static qboolean Terr_InitLightmap(hmsection_t *s, qboolean initialise)
{
	heightmap_t *hm = s->hmmod;

	if (s->lightmap < 0)
	{
		struct lmsect_s *lms;
		if (!hm->unusedlmsects)
		{
			int lm;
			int i;
			lm = Surf_NewLightmaps(1, SECTTEXSIZE*LMCHUNKS, SECTTEXSIZE*LMCHUNKS, false);
			for (i = 0; i < LMCHUNKS*LMCHUNKS; i++)
			{
				lms = BZ_Malloc(sizeof(*lms));
				lms->lm = lm;
				lms->x = (i & (LMCHUNKS-1))*SECTTEXSIZE;
				lms->y = (i / LMCHUNKS)*SECTTEXSIZE;
				lms->next = hm->unusedlmsects;
				hm->unusedlmsects = lms;
				hm->numunusedlmsects++;
			}
		}

		lms = hm->unusedlmsects;
		hm->unusedlmsects = lms->next;
		
		s->lightmap = lms->lm;
		s->lmx = lms->x;
		s->lmy = lms->y;

		hm->numunusedlmsects--;
		hm->numusedlmsects++;

		Z_Free(lms);
	}

	if (initialise && s->lightmap >= 0)
	{
		int x, y;
		unsigned char *lm;
		lm = lightmap[s->lightmap]->lightmaps;
		lm += (s->lmy * HMLMSTRIDE + s->lmx) * lightmap_bytes;
		for (y = 0; y < SECTTEXSIZE; y++)
		{
			for (x = 0; x < SECTTEXSIZE; x++)
			{
				lm[x*4+0] = 0;
				lm[x*4+1] = 0;
				lm[x*4+2] = 0;
				lm[x*4+3] = 255;
			}
			lm += (HMLMSTRIDE)*lightmap_bytes;
		}
	}

	if (s->lightmap >= 0)
	{
		lightmap[s->lightmap]->modified = true;
		lightmap[s->lightmap]->rectchange.l = 0;
		lightmap[s->lightmap]->rectchange.t = 0;
		lightmap[s->lightmap]->rectchange.w = HMLMSTRIDE;
		lightmap[s->lightmap]->rectchange.h = HMLMSTRIDE;
	}

	return s->lightmap>=0;
}
#endif

static char *genextendedhex(int n, char *buf)
{
	char *ret;
	static char nibble[16] = "0123456789abcdef";
	unsigned int m;
	int i;
	for (i = 7; i >= 1; i--)	//>=1 ensures at least two nibbles appear.
	{
		m = 0xfffffff8<<(i*4);
		if ((n & m) != m && (n & m) != 0)
			break;
	}
	ret = buf;
	for(i++; i >= 0; i--)
		*buf++ = nibble[(n>>i*4) & 0xf];
	*buf++ = 0;
	return ret;
}
static char *Terr_DiskBlockName(heightmap_t *hm, int sx, int sy, char *out, size_t outsize)
{
	char xpart[9];
	char ypart[9];
	//using a naming scheme centered around 0 means we can gracefully expand the map away from 0,0
	sx -= CHUNKBIAS;
	sy -= CHUNKBIAS;
	//wrap cleanly
	sx &= CHUNKLIMIT-1;
	sy &= CHUNKLIMIT-1;
	sx /= SECTIONSPERBLOCK;
	sy /= SECTIONSPERBLOCK;
	if (sx >= CHUNKBIAS/SECTIONSPERBLOCK)
		sx |= 0xffffff00;
	if (sy >= CHUNKBIAS/SECTIONSPERBLOCK)
		sy |= 0xffffff00;
	Q_snprintfz(out, outsize, "maps/%s/block_%s_%s.hms", hm->path, genextendedhex(sx, xpart), genextendedhex(sy, ypart));
	return out;
}
static char *Terr_DiskSectionName(heightmap_t *hm, int sx, int sy, char *out, size_t outsize)
{
	sx -= CHUNKBIAS;
	sy -= CHUNKBIAS;
	//wrap cleanly
	sx &= CHUNKLIMIT-1;
	sy &= CHUNKLIMIT-1;
	Q_snprintfz(out, outsize, "maps/%s/sect_%03x_%03x.hms", hm->path, sx, sy);
	return out;
}
#ifndef SERVERONLY
static char *Terr_TempDiskSectionName(heightmap_t *hm, int sx, int sy)
{
	sx -= CHUNKBIAS;
	sy -= CHUNKBIAS;
	//wrap cleanly
	sx &= CHUNKLIMIT-1;
	sy &= CHUNKLIMIT-1;
	return va("temp/%s/sect_%03x_%03x.hms", hm->path, sx, sy);
}
#endif

static int dehex_e(int i, qboolean *error)
{
	if      (i >= '0' && i <= '9')
		return (i-'0');
	else if (i >= 'A' && i <= 'F')
		return (i-'A'+10);
	else if (i >= 'a' && i <= 'f')
		return (i-'a'+10);
	else
		*error = true;
	return 0;
}
static qboolean Terr_IsSectionFName(heightmap_t *hm, char *fname, int *sx, int *sy)
{
	int l;
	qboolean error = false;
	*sx = 0xdeafbeef;	//something clearly invalid
	*sy = 0xdeafbeef;

	//not this model...
	if (!hm)
		return false;

	//expect the first 5 chars to be maps/ or temp/
	fname += 5;

	l = strlen(hm->path);
	if (strncmp(fname, hm->path, l) || fname[l] != '/')
		return false;
	fname += l+1;

	//fname now has a fixed length.
	if (strlen(fname) != 16)
		return false;
	if (strncmp(fname, "sect_", 5) || fname[8] != '_' || (strcmp(fname+12, ".hms") && strcmp(fname+12, ".tmp")))
		return false;

	*sx = 0;
	*sx += dehex_e(fname[5], &error)<<8;
	*sx += dehex_e(fname[6], &error)<<4;
	*sx += dehex_e(fname[7], &error)<<0;

	*sy = 0;
	*sy += dehex_e(fname[9], &error)<<8;
	*sy += dehex_e(fname[10], &error)<<4;
	*sy += dehex_e(fname[11], &error)<<0;

	*sx += CHUNKBIAS;
	*sy += CHUNKBIAS;

	if ((unsigned)*sx >= CHUNKLIMIT)
		*sx -= CHUNKLIMIT;
	if ((unsigned)*sy >= CHUNKLIMIT)
		*sy -= CHUNKLIMIT;

	//make sure its a valid section index.
	if ((unsigned)*sx >= CHUNKLIMIT)
		return false;
	if ((unsigned)*sy >= CHUNKLIMIT)
		return false;
	return true;
}

static hmsection_t *Terr_GenerateSection(heightmap_t *hm, int sx, int sy, qboolean scheduleload)
{
	hmsection_t *s;
	hmcluster_t *cluster;
	int clusternum = (sx/MAXSECTIONS) + (sy/MAXSECTIONS)*MAXCLUSTERS;

#ifdef LOADERTHREAD
	Sys_LockMutex(com_resourcemutex);
#endif
	cluster = hm->cluster[clusternum];
	if (!cluster)
		cluster = hm->cluster[clusternum] = Z_Malloc(sizeof(*cluster));
	s = cluster->section[(sx%MAXSECTIONS) + (sy%MAXSECTIONS)*MAXSECTIONS];
	if (!s)
	{
		s = Z_Malloc(sizeof(*s));
		if (!s)
		{
#ifdef LOADERTHREAD
			Sys_UnlockMutex(com_resourcemutex);
#endif
			return NULL;
		}
#ifndef SERVERONLY
		s->lightmap = -1;
#endif
		
#ifndef SERVERONLY
		s->numents = 0;
#endif

		s->sx = sx;
		s->sy = sy;
		cluster->section[(sx%MAXSECTIONS) + (sy%MAXSECTIONS)*MAXSECTIONS] = s;
		hm->activesections++;
		s->hmmod = hm;

		s->flags = TSF_DIRTY;

		hm->loadingsections+=1;

		s->loadstate = TSLS_LOADING1;

		if (scheduleload)
			COM_AddWork(1, Terr_LoadSectionWorker, s, hm, sx, sy);
	}
#ifdef LOADERTHREAD
	Sys_UnlockMutex(com_resourcemutex);
#endif
	return s;
}

//generates some water
static void *Terr_GenerateWater(hmsection_t *s, float maxheight)
{
	int i;
	struct hmwater_s *w;
	w = Z_Malloc(sizeof(*s->water));
	w->next = s->water;
	s->water = w;
	Q_strncpyz(w->shadername, s->hmmod->defaultwatershader, sizeof(w->shadername));
	w->simple = true;
	w->contentmask = FTECONTENTS_WATER;
	memset(w->holes, 0, sizeof(w->holes));
	for (i = 0; i < 9*9; i++)
		w->heights[i] = maxheight;
	w->maxheight = w->minheight = maxheight;
	if (s->maxh_cull < w->maxheight)
		s->maxh_cull = w->maxheight;
	return w;
}

//embeds a mesh
static void Terr_AddMesh(heightmap_t *hm, int loadflags, model_t *mod, vec3_t epos, vec3_t axis[3], float scale)
{
	struct hmentity_s *e, *f = NULL;
	hmsection_t *s;
	int min[2], max[2], coord[2];
	int i;

	if (!mod)
		return;

	if (!scale)
		scale = 1;

	if (axis[0][0] != 1 || axis[0][1] != 0 || axis[0][2] != 0 ||
		axis[1][0] != 0 || axis[1][1] != 1 || axis[1][2] != 0 ||
		axis[2][0] != 0 || axis[2][1] != 0 || axis[2][2] != 1)
	{
		min[0] = floor((epos[0]-mod->radius*scale) / hm->sectionsize) + CHUNKBIAS;
		min[1] = floor((epos[1]-mod->radius*scale) / hm->sectionsize) + CHUNKBIAS;
		min[0] = bound(hm->firstsegx, min[0], hm->maxsegx-1);
		min[1] = bound(hm->firstsegy, min[1], hm->maxsegy-1);
		
		max[0] = floor((epos[0]+mod->radius*scale) / hm->sectionsize) + CHUNKBIAS;
		max[1] = floor((epos[1]+mod->radius*scale) / hm->sectionsize) + CHUNKBIAS;
		max[0] = bound(hm->firstsegx, max[0], hm->maxsegx-1);
		max[1] = bound(hm->firstsegy, max[1], hm->maxsegy-1);
	}
	else
	{
		min[0] = floor((epos[0]+mod->mins[0]*scale) / hm->sectionsize) + CHUNKBIAS;
		min[1] = floor((epos[1]+mod->mins[1]*scale) / hm->sectionsize) + CHUNKBIAS;
		min[0] = bound(hm->firstsegx, min[0], hm->maxsegx-1);
		min[1] = bound(hm->firstsegy, min[1], hm->maxsegy-1);
		
		max[0] = floor((epos[0]+mod->maxs[0]*scale) / hm->sectionsize) + CHUNKBIAS;
		max[1] = floor((epos[1]+mod->maxs[1]*scale) / hm->sectionsize) + CHUNKBIAS;
		max[0] = bound(hm->firstsegx, max[0], hm->maxsegx-1);
		max[1] = bound(hm->firstsegy, max[1], hm->maxsegy-1);
	}

	Sys_LockMutex(hm->entitylock);
	//try to find the ent if it already exists (don't do dupes)
	for (e = hm->entities; e; e = e->next)
	{
		if (!e->refs)
			f = e;
		else
		{
			if (e->ent.origin[0] != epos[0] || e->ent.origin[1] != epos[1] || e->ent.origin[2] != epos[2])
				continue;
			if (e->ent.model != mod || e->ent.scale != scale)
				continue;
			if (memcmp(axis, e->ent.axis, sizeof(e->ent.axis)))
				continue;
			break;	//looks like a match.
		}
	}
	//allocate it if needed
	if (!e)
	{
		if (f)
			e = f;	//can reuse a released one
		else
		{	//allocate one
			e = Z_Malloc(sizeof(*e));
			e->next = hm->entities;
			hm->entities = e;
		}

		e->ent.drawflags = SCALE_ORIGIN_ORIGIN;
		e->ent.scale = scale;
		e->ent.playerindex = -1;
		e->ent.shaderRGBAf[0] = 1;
		e->ent.shaderRGBAf[1] = 1;
		e->ent.shaderRGBAf[2] = 1;
		e->ent.shaderRGBAf[3] = 1;
		VectorCopy(epos, e->ent.origin);
		memcpy(e->ent.axis, axis, sizeof(e->ent.axis));
		e->ent.model = mod;
	}

	for (coord[0] = min[0]; coord[0] <= max[0]; coord[0]++)
	{
		for (coord[1] = min[1]; coord[1] <= max[1]; coord[1]++)
		{
			s = Terr_GetSection(hm, coord[0], coord[1], loadflags|TGS_ANYSTATE);
			if (!s)
				continue;

			//don't add pointless dupes
			for (i = 0; i < s->numents; i++)
			{
				if (s->ents[i] == e)
					break;
			}
			if (i < s->numents)
				continue;

			//FIXME: while technically correct, this causes issues with the v1 format.
			s->flags |= TSF_EDITED;

			//FIXME: race condition - main thread might be walking the entity list.
			//FIXME: even worse: the editor might be running through this routine adding/removing entities at the same time as the loader.
			if (s->maxents == s->numents)
			{
				s->maxents++;
				s->ents = realloc(s->ents, sizeof(*s->ents)*(s->maxents));
			}
			s->ents[s->numents++] = e;
			e->refs++;
		}
	}
	Sys_UnlockMutex(hm->entitylock);
}

static void *Terr_ReadV1(heightmap_t *hm, hmsection_t *s, void *ptr, int len)
{
#ifndef SERVERONLY
	dsmesh_v1_t *dm;
	float *colours;
	qbyte *lmstart;
#endif
	dsection_v1_t *ds = ptr;
	int i;

	unsigned int flags = LittleLong(ds->flags);
	s->flags |= flags & ~(TSF_INTERNAL|TSF_HASWATER_V0);
	for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
	{
		s->heights[i] = LittleFloat(ds->heights[i]);
	}
	s->minh = ds->minh;
	s->maxh = ds->maxh;
	if (flags & TSF_HASWATER_V0)
		Terr_GenerateWater(s, ds->waterheight);

	memset(s->holes, 0, sizeof(s->holes));
	for (i = 0; i < 8*8; i++)
	{
		int x = (i & 7);
		int y = (i>>3);
		int b = (1u<<(x>>1)) << ((y>>1)<<2);
		if (ds->holes & b)
			s->holes[y] |= 1u<<x;
	}

	ptr = ds+1;

#ifndef SERVERONLY
	/*deal with textures*/
	Q_strncpyz(s->texname[0], ds->texname[0], sizeof(s->texname[0]));
	Q_strncpyz(s->texname[1], ds->texname[1], sizeof(s->texname[1]));
	Q_strncpyz(s->texname[2], ds->texname[2], sizeof(s->texname[2]));
	Q_strncpyz(s->texname[3], ds->texname[3], sizeof(s->texname[3]));

	/*load in the mixture/lighting*/
	lmstart = BZ_Malloc(SECTTEXSIZE*SECTTEXSIZE*4);
	memcpy(lmstart, ds->texmap, SECTTEXSIZE*SECTTEXSIZE*4);
	COM_AddWork(0, Terr_WorkerLoadedSectionLightmap, hm, lmstart, s->sx, s->sy);

	s->mesh.colors4f_array[0] = s->colours;
	if (flags & TSF_HASCOLOURS)
	{
		for (i = 0, colours = (float*)ptr; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++, colours+=4)
		{
			s->colours[i][0] = LittleFloat(colours[0]);
			s->colours[i][1] = LittleFloat(colours[1]);
			s->colours[i][2] = LittleFloat(colours[2]);
			s->colours[i][3] = LittleFloat(colours[3]);
		}
		ptr = colours;
	}
	else
	{
		for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
		{
			s->colours[i][0] = 1;
			s->colours[i][1] = 1;
			s->colours[i][2] = 1;
			s->colours[i][3] = 1;
		}
	}

	/*load any static ents*/
	for (i = 0, dm = (dsmesh_v1_t*)ptr; i < ds->ents_num; i++, dm = (dsmesh_v1_t*)((qbyte*)dm + dm->size))
	{
		vec3_t org;
		org[0] = dm->axisorg[3][0] + (s->sx-CHUNKBIAS)*hm->sectionsize;
		org[1] = dm->axisorg[3][1] + (s->sy-CHUNKBIAS)*hm->sectionsize;
		org[2] = dm->axisorg[3][2];
		Terr_AddMesh(hm, TGS_NOLOAD, Mod_ForName((char*)(dm + 1), MLV_WARN), org, dm->axisorg, dm->scale); 
	}
#endif
	return ptr;
}




struct terrstream_s
{
	qbyte *buffer;
	int maxsize;
	int pos;
};
//I really hope these get inlined properly.
static int Terr_Read_SInt(struct terrstream_s *strm)
{
	int val;
	strm->pos = (strm->pos + sizeof(val)-1) & ~(sizeof(val)-1);
	val = *(int*)(strm->buffer+strm->pos);
	strm->pos += sizeof(val);
	return LittleLong(val);
}
static qbyte Terr_Read_Byte(struct terrstream_s *strm)
{
	qbyte val;
	val = *(qbyte*)(strm->buffer+strm->pos);
	strm->pos += sizeof(val);
	return val;
}
static float Terr_Read_Float(struct terrstream_s *strm)
{
	float val;
	strm->pos = (strm->pos + sizeof(val)-1) & ~(sizeof(val)-1);
	val = *(float*)(strm->buffer+strm->pos);
	strm->pos += sizeof(val);
	return LittleFloat(val);
}
static char *Terr_Read_String(struct terrstream_s *strm, char *val, int maxlen)
{
	int len = strlen(strm->buffer + strm->pos);
	maxlen = min(len, maxlen-1);	//truncate
	memcpy(val, strm->buffer + strm->pos, maxlen);
	val[maxlen] = 0;
	strm->pos += len+1;
	return val;
}
#ifndef SERVERONLY
static void Terr_Write_SInt(struct terrstream_s *strm, int val)
{
	val = LittleLong(val);
	strm->pos = (strm->pos + sizeof(val)-1) & ~(sizeof(val)-1);
	*(int*)(strm->buffer+strm->pos) = val;
	strm->pos += sizeof(val);
}
static void Terr_Write_Byte(struct terrstream_s *strm, qbyte val)
{
	*(qbyte*)(strm->buffer+strm->pos) = val;
	strm->pos += sizeof(val);
}
static void Terr_Write_Float(struct terrstream_s *strm, float val)
{
	val = LittleFloat(val);
	strm->pos = (strm->pos + sizeof(val)-1) & ~(sizeof(val)-1);
	*(float*)(strm->buffer+strm->pos) = val;
	strm->pos += sizeof(val);
}
static void Terr_Write_String(struct terrstream_s *strm, char *val)
{
	int len = strlen(val)+1;
	memcpy(strm->buffer + strm->pos, val, len);
	strm->pos += len;
}
static void Terr_TrimWater(hmsection_t *s)
{
	int i;
	struct hmwater_s *w, **link;

	for (link = &s->water; (w = *link); )
	{
		//one has a height above the terrain?
		for (i = 0; i < 9*9; i++)
			if (w->heights[i] > s->minh)
				break;
		if (i == 9*9)
		{
			*link = w->next;
			Z_Free(w);
			continue;
		}
		else
			link = &(*link)->next;
	}
}
static void Terr_SaveV2(heightmap_t *hm, hmsection_t *s, vfsfile_t *f, int sx, int sy)
{
	qbyte buffer[65536], last, delta, *lm;
	struct terrstream_s strm = {buffer, sizeof(buffer), 0};
	unsigned int flags = s->flags;
	int i, j, x, y;
	struct hmwater_s *w;

	flags &= ~(TSF_INTERNAL);
	flags &= ~(TSF_HASCOLOURS|TSF_HASHEIGHTS|TSF_HASSHADOW);

	for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
	{
		if (s->colours[i][0] != 1 || s->colours[i][1] != 1 || s->colours[i][2] != 1 || s->colours[i][3] != 1)
		{
			flags |= TSF_HASCOLOURS;
			break;
		}
	}
	for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
	{
		if (s->heights[i] != s->heights[0])
		{
			flags |= TSF_HASHEIGHTS;
			break;
		}
	}

	lm = lightmap[s->lightmap]->lightmaps;
	lm += (s->lmy * HMLMSTRIDE + s->lmx) * lightmap_bytes;
	for (y = 0; y < SECTTEXSIZE; y++)
	{
		for (x = 0; x < SECTTEXSIZE; x++)
		{
			if (lm[x*4+3] != 255)
			{
				flags |= TSF_HASSHADOW;
				y = SECTTEXSIZE;
				break;
			}
		}
		lm += (HMLMSTRIDE)*lightmap_bytes;
	}

	//write the flags so the loader knows what to load
	Terr_Write_SInt(&strm, flags);

	//if heights are compressed, only the first is present.
	if (!(flags & TSF_HASHEIGHTS))
		Terr_Write_Float(&strm, s->heights[0]);
	else
	{
		for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
			Terr_Write_Float(&strm, s->heights[i]);
	}

	for (i = 0; i < sizeof(s->holes); i++)
		Terr_Write_Byte(&strm, s->holes[i]);

	Terr_TrimWater(s);
	for (j = 0, w = s->water; w; j++)
		w = w->next;
	Terr_Write_SInt(&strm, j);
	for (i = 0, w = s->water; i < j; i++, w = w->next)
	{
		char *shadername = w->shader->name;
		int fl = 0;

		if (strcmp(shadername, hm->defaultwatershader))
			fl |= 1;
		for (x = 0; x < 8; x++)
			if (w->holes[x])
				break;
		fl |= ((x==8)?0:2);
		for (x = 0; x < 9*9; x++)
			if (w->heights[x] != w->heights[0])
				break;
		fl |= ((x==9*9)?0:4);

		
		Terr_Write_SInt(&strm, fl);
		Terr_Write_SInt(&strm, w->contentmask);
		if (fl & 1)
			Terr_Write_String(&strm, shadername);
		if (fl & 2)
		{
			for (x = 0; x < 8; x++)
				Terr_Write_Byte(&strm, w->holes[x]);
		}
		if (fl & 4)
		{
			for (x = 0; x < 9*9; x++)
				Terr_Write_Float(&strm, w->heights[x]);
		}
		else
			Terr_Write_Float(&strm, w->heights[0]);
	}

	if (flags & TSF_HASCOLOURS)
	{
		//FIXME: bytes? channels?
		for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
		{
			Terr_Write_Float(&strm, s->colours[i][0]);
			Terr_Write_Float(&strm, s->colours[i][1]);
			Terr_Write_Float(&strm, s->colours[i][2]);
			Terr_Write_Float(&strm, s->colours[i][3]);
		}
	}

	for (j = 0; j < 4; j++)
		Terr_Write_String(&strm, s->texname[j]);
	for (j = 0; j < 4; j++)
	{
		if (j == 3)
		{
			//only write the channel if it has actual data
			if (!(flags & TSF_HASSHADOW))
				continue;
		}
		else
		{
			//only write the data if there's actually a texture.
			//its not meant to be possible to delete a texture without deleting its data too.
			//
			if (!*s->texname[2-j])
				continue;
		}

		//write the channel
		last = 0;
		lm = lightmap[s->lightmap]->lightmaps;
		lm += (s->lmy * HMLMSTRIDE + s->lmx) * lightmap_bytes;
		for (y = 0; y < SECTTEXSIZE; y++)
		{
			for (x = 0; x < SECTTEXSIZE; x++)
			{
				delta = lm[x*4+j] - last;
				last = lm[x*4+j];
				Terr_Write_Byte(&strm, delta);
			}
			lm += (HMLMSTRIDE)*lightmap_bytes;
		}
	}

	Sys_LockMutex(hm->entitylock);
	Terr_Write_SInt(&strm, s->numents);
	for (i = 0; i < s->numents; i++)
	{
		unsigned int mf;

		//make sure we don't overflow. we should always be aligned at this point.
		if (strm.pos > strm.maxsize/2)
		{
			VFS_WRITE(f, strm.buffer, strm.pos);
			strm.pos = 0;
		}

		mf = 0;
		if (s->ents[i]->ent.scale != 1)
			mf |= TMF_SCALE;
		Terr_Write_SInt(&strm, mf);
		if (s->ents[i]->ent.model)
			Terr_Write_String(&strm, s->ents[i]->ent.model->name);
		else
			Terr_Write_String(&strm, "*invalid");
		Terr_Write_Float(&strm, s->ents[i]->ent.origin[0]+(CHUNKBIAS-sx)*hm->sectionsize);
		Terr_Write_Float(&strm, s->ents[i]->ent.origin[1]+(CHUNKBIAS-sy)*hm->sectionsize);
		Terr_Write_Float(&strm, s->ents[i]->ent.origin[2]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[0][0]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[0][1]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[0][2]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[1][0]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[1][1]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[1][2]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[2][0]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[2][1]);
		Terr_Write_Float(&strm, s->ents[i]->ent.axis[2][2]);
		if (mf & TMF_SCALE)
			Terr_Write_Float(&strm, s->ents[i]->ent.scale);
	}
	Sys_UnlockMutex(hm->entitylock);

	//reset it in case the buffer is getting a little full
	strm.pos = (strm.pos + sizeof(int)-1) & ~(sizeof(int)-1);
	VFS_WRITE(f, strm.buffer, strm.pos);
	strm.pos = 0;
}
static void Terr_WorkerLoadedSectionLightmap(void *ctx, void *data, size_t a, size_t b)
{
	heightmap_t *hm = ctx;
	hmsection_t *s = Terr_GetSection(hm, a, b, TGS_NOLOAD|TGS_ANYSTATE);
	qbyte *inlm = data;
	qbyte *outlm;
	int y;

	if (s)
	if (lightmap_bytes == 4 && Terr_InitLightmap(s, false))
	{
		outlm = lightmap[s->lightmap]->lightmaps;
		outlm += (s->lmy * HMLMSTRIDE + s->lmx) * lightmap_bytes;
		for (y = 0; y < SECTTEXSIZE; y++)
		{
			memcpy(outlm, inlm, SECTTEXSIZE*4);
			inlm += SECTTEXSIZE*4;
			outlm += (HMLMSTRIDE)*lightmap_bytes;
		}
	}

	BZ_Free(data);
}
#endif
static void *Terr_ReadV2(heightmap_t *hm, hmsection_t *s, void *ptr, int len)
{
#ifndef SERVERONLY
	char modelname[MAX_QPATH];
	qbyte last;
	int y;
	qboolean present;
	qbyte *lmstart = NULL, *lm, delta;
#endif
	struct terrstream_s strm = {ptr, len, 0};
	float f;
	int i, j, x;
	unsigned int flags = Terr_Read_SInt(&strm);

	s->flags |= flags & ~TSF_INTERNAL;
	if (flags & TSF_HASHEIGHTS)
	{
		s->minh = s->maxh = s->heights[0] = Terr_Read_Float(&strm);
		for (i = 1; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
		{
			f = Terr_Read_Float(&strm);
			if (s->minh > f)
				s->minh = f;
			if (s->maxh < f)
				s->maxh = f;
			s->heights[i] = f;
		}
	}
	else
	{
		s->minh = s->maxh = f = Terr_Read_Float(&strm);
		for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
			s->heights[i] = f;
	}
	
	for (i = 0; i < sizeof(s->holes); i++)
		s->holes[i] = Terr_Read_Byte(&strm);

	j = Terr_Read_SInt(&strm);
	for (i = 0; i < j; i++)
	{
		struct hmwater_s *w = Z_Malloc(sizeof(*w));
		int fl = Terr_Read_SInt(&strm);
		w->next = s->water;
		s->water = w;
		w->simple = true;
		w->contentmask = Terr_Read_SInt(&strm);
		if (fl & 1)
			Terr_Read_String(&strm, w->shadername, sizeof(w->shadername));
		else
			Q_strncpyz(w->shadername, hm->defaultwatershader, sizeof(w->shadername));
		if (fl & 2)
		{
			for (x = 0; x < 8; x++)
				w->holes[i] = Terr_Read_Byte(&strm);
			w->simple = false;
		}
		if (fl & 4)
		{
			for (x = 0; x < 9*9; x++)
			{
				w->heights[x] = Terr_Read_Float(&strm);
			}
			w->simple = false;
		}
		else
		{	//all heights the same can be used as a way to compress the data
			w->minheight = w->maxheight = Terr_Read_Float(&strm);
			for (x = 0; x < 9*9; x++)
				w->heights[x] = w->minheight = w->maxheight;
		}
	}

	//dedicated server can stop reading here.

#ifndef SERVERONLY
	if (flags & TSF_HASCOLOURS)
	{
		for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
		{
			s->colours[i][0] = Terr_Read_Float(&strm);
			s->colours[i][1] = Terr_Read_Float(&strm);
			s->colours[i][2] = Terr_Read_Float(&strm);
			s->colours[i][3] = Terr_Read_Float(&strm);
		}
	}
	else
	{
		for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
		{
			s->colours[i][0] = 1;
			s->colours[i][1] = 1;
			s->colours[i][2] = 1;
			s->colours[i][3] = 1;
		}
	}

	for (j = 0; j < 4; j++)
		Terr_Read_String(&strm, s->texname[j], sizeof(s->texname[j]));
	for (j = 0; j < 4; j++)
	{
		if (j == 3)
			present = !!(flags & TSF_HASSHADOW);
		else
			present = !!(*s->texname[2-j]);

		//should be able to skip this if no shadows or textures
		if (!lmstart)
			lmstart = BZ_Malloc(SECTTEXSIZE*SECTTEXSIZE*4);

		if (present)
		{
			//read the channel
			last = 0;
			lm = lmstart;
			for (y = 0; y < SECTTEXSIZE; y++)
			{
				for (x = 0; x < SECTTEXSIZE; x++)
				{
					delta = Terr_Read_Byte(&strm);
					last = (last+delta)&0xff;
					lm[x*4+j] = last;
				}
				lm += x*4;
			}
		}
		else
		{
			last = ((j==3)?255:0);
			lm = lmstart;
			for (y = 0; y < SECTTEXSIZE; y++)
			{
				for (x = 0; x < SECTTEXSIZE; x++)
					lm[x*4+j] = last;
				lm += x*4;
			}
		}
	}

	if (lmstart)
		COM_AddWork(0, Terr_WorkerLoadedSectionLightmap, hm, lmstart, s->sx, s->sy);

	/*load any static ents*/
	j = Terr_Read_SInt(&strm);
	for (i = 0; i < j; i++)
	{
		vec3_t axis[3];
		vec3_t org;
		unsigned int mf;
		model_t *mod;
		float scale;
		mf = Terr_Read_SInt(&strm);

		mod = Mod_FindName(Terr_Read_String(&strm, modelname, sizeof(modelname)));
		org[0] = Terr_Read_Float(&strm);
		org[1] = Terr_Read_Float(&strm);
		org[2] = Terr_Read_Float(&strm);
		axis[0][0] = Terr_Read_Float(&strm);
		axis[0][1] = Terr_Read_Float(&strm);
		axis[0][2] = Terr_Read_Float(&strm);
		axis[1][0] = Terr_Read_Float(&strm);
		axis[1][1] = Terr_Read_Float(&strm);
		axis[1][2] = Terr_Read_Float(&strm);
		axis[2][0] = Terr_Read_Float(&strm);
		axis[2][1] = Terr_Read_Float(&strm);
		axis[2][2] = Terr_Read_Float(&strm);
		scale = (mf&TMF_SCALE)?Terr_Read_Float(&strm):1;

		org[0] += (s->sx-CHUNKBIAS)*hm->sectionsize;
		org[1] += (s->sy-CHUNKBIAS)*hm->sectionsize;

		Terr_AddMesh(hm, TGS_NOLOAD, mod, org, axis, scale);
	}
#endif
	return ptr;
}

//#include "gl_adt.inc"

static void Terr_ClearSection(hmsection_t *s)
{
	struct hmwater_s *w;
	int i;
	Sys_LockMutex(s->hmmod->entitylock);
	for (i = 0; i < s->numents; i++)
		s->ents[i]->refs-=1;
	s->numents = 0;
	Sys_UnlockMutex(s->hmmod->entitylock);

	while(s->water)
	{
		w = s->water;
		s->water = w->next;
		Z_Free(w);
	}
}

static void Terr_GenerateDefault(heightmap_t *hm, hmsection_t *s)
{
	int i;

	memset(s->holes, 0, sizeof(s->holes));

#ifndef SERVERONLY
	Q_strncpyz(s->texname[0], "", sizeof(s->texname[0]));
	Q_strncpyz(s->texname[1], "", sizeof(s->texname[1]));
	Q_strncpyz(s->texname[2], "", sizeof(s->texname[2]));
	Q_strncpyz(s->texname[3], hm->defaultgroundtexture, sizeof(s->texname[3]));

	if (s->lightmap >= 0)
	{
		int j;
		qbyte *lm;

		lm = lightmap[s->lightmap]->lightmaps;
		lm += (s->lmy * HMLMSTRIDE + s->lmx) * lightmap_bytes;
		for (i = 0; i < SECTTEXSIZE; i++)
		{
			for (j = 0; j < SECTTEXSIZE; j++)
			{
				lm[j*4+0] = 0;
				lm[j*4+0] = 0;
				lm[j*4+0] = 0;
				lm[j*4+3] = 255;
			}
			lm += (HMLMSTRIDE)*lightmap_bytes;
		}
		lightmap[s->lightmap]->modified = true;
		lightmap[s->lightmap]->rectchange.l = 0;
		lightmap[s->lightmap]->rectchange.t = 0;
		lightmap[s->lightmap]->rectchange.w = HMLMSTRIDE;
		lightmap[s->lightmap]->rectchange.h = HMLMSTRIDE;
	}
	for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
	{
		s->colours[i][0] = 1;
		s->colours[i][1] = 1;
		s->colours[i][2] = 1;
		s->colours[i][3] = 1;
	}
	s->mesh.colors4f_array[0] = s->colours;
#endif

	for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
		s->heights[i] = hm->defaultgroundheight;

	if (hm->defaultwaterheight > hm->defaultgroundheight)
		Terr_GenerateWater(s, hm->defaultwaterheight);

#if 0//def DEBUG
	void *f;
	if (lightmap_bytes == 4 && lightmap_bgra && FS_LoadFile(va("maps/%s/splatt.png", hm->path), &f) != (qofs_t)-1)
	{
		//temp
		int vx, vy;
		int x, y;
		extern qbyte *Read32BitImageFile(qbyte *buf, int len, int *width, int *height, qboolean *hasalpha, char *fname);
		int sw, sh;
		qboolean hasalpha;
		unsigned char *splatter = Read32BitImageFile(f, com_filesize, &sw, &sh, &hasalpha, "splattermap");
		if (splatter)
		{
			lm = lightmap[s->lightmap]->lightmaps;
			lm += (s->lmy * HMLMSTRIDE + s->lmx) * lightmap_bytes;

			for (vx = 0; vx < SECTTEXSIZE; vx++)
			{
				x = sw * (((float)sy) + ((float)vx / (SECTTEXSIZE-1))) / hm->numsegsx;
				if (x > sw-1)
					x = sw-1;
				for (vy = 0; vy < SECTTEXSIZE; vy++)
				{
					y = sh * (((float)sx) + ((float)vy / (SECTTEXSIZE-1))) / hm->numsegsy;
					if (y > sh-1)
						y = sh-1;

					lm[2] = splatter[(y + x*sh)*4+0];
					lm[1] = splatter[(y + x*sh)*4+1];
					lm[0] = splatter[(y + x*sh)*4+2];
					lm[3] = splatter[(y + x*sh)*4+3];
					lm += 4;
				}
				lm += (HMLMSTRIDE - SECTTEXSIZE)*lightmap_bytes;
			}
			BZ_Free(splatter);

			lightmap[s->lightmap]->modified = true;
			lightmap[s->lightmap]->rectchange.l = 0;
			lightmap[s->lightmap]->rectchange.t = 0;
			lightmap[s->lightmap]->rectchange.w = HMLMSTRIDE;
			lightmap[s->lightmap]->rectchange.h = HMLMSTRIDE;
		}
		FS_FreeFile(f);
	}

	if (lightmap_bytes == 4 && lightmap_bgra && !qofs_Error(FS_LoadFile(va("maps/%s/heightmap.png", hm->path), &f)))
	{
		//temp
		int vx, vy;
		int x, y;
		extern qbyte *Read32BitImageFile(qbyte *buf, int len, int *width, int *height, qboolean *hasalpha, char *fname);
		int sw, sh;
		float *h;
		qboolean hasalpha;
		unsigned char *hmimage = Read32BitImageFile(f, com_filesize, &sw, &sh, &hasalpha, "heightmap");
		if (hmimage)
		{
			h = s->heights;

			for (vx = 0; vx < SECTHEIGHTSIZE; vx++)
			{
				x = sw * (((float)sy) + ((float)vx / (SECTHEIGHTSIZE-1))) / hm->numsegsx;
				if (x > sw-1)
					x = sw-1;
				for (vy = 0; vy < SECTHEIGHTSIZE; vy++)
				{
					y = sh * (((float)sx) + ((float)vy / (SECTHEIGHTSIZE-1))) / hm->numsegsy;
					if (y > sh-1)
						y = sh-1;

					*h = 0;
					*h += hmimage[(y + x*sh)*4+0];
					*h += hmimage[(y + x*sh)*4+1]<<8;
					*h += hmimage[(y + x*sh)*4+2]<<16;
					*h *= 4.0f/(1<<16);
					h++;
				}
			}
			BZ_Free(hmimage);
		}
		FS_FreeFile(f);
	}
#endif
}

static void Terr_WorkerLoadedSection(void *ctx, void *data, size_t a, size_t b)
{
	hmsection_t *s = ctx;
	Terr_LoadSectionTextures(s);
	COM_AssertMainThread("foo");
	InsertLinkBefore(&s->recycle, &s->hmmod->recycle);
	s->hmmod->loadingsections-=1;
	s->flags &= ~TSF_EDITED;
	s->loadstate = TSLS_LOADED;
}
static void Terr_WorkerFailedSection(void *ctx, void *data, size_t a, size_t b)
{
	hmsection_t *s = ctx;
	Terr_WorkerLoadedSection(ctx, data, a, b);
	s->flags &= ~TSF_EDITED;
	s->loadstate = TSLS_FAILED;
}

static hmsection_t *Terr_ReadSection(heightmap_t *hm, hmsection_t *s, int ver, void *filebase, unsigned int filelen)
{
	qboolean failed = false;
	void *ptr = filebase;

	if (ptr && ver == 1)
		Terr_ReadV1(hm, s, ptr, filelen);
	else if (ptr && ver == 2)
		Terr_ReadV2(hm, s, ptr, filelen);
	else
	{
//		s->flags |= TSF_RELIGHT;
		Terr_GenerateDefault(hm, s);

		failed = true;
	}

	s->flags &= ~TSF_EDITED;	//its just been loaded (and was probably edited by the loader), make sure it doesn't get saved or whatever

	s->loadstate = TSLS_LOADING2;
	if (failed)
		COM_AddWork(0, Terr_WorkerFailedSection, s, NULL, s->sx, s->sy);
	else
		COM_AddWork(0, Terr_WorkerLoadedSection, s, NULL, s->sx, s->sy);

	return s;
}

#ifndef SERVERONLY
qboolean Terr_DownloadedSection(char *fname)
{
/*
	qofs_t len;
	dsection_t *fileptr;
	int x, y;
	heightmap_t *hm;
	int ver = 0;

	if (!cl.worldmodel)
		return false;

	hm = cl.worldmodel->terrain;

	if (Terr_IsSectionFName(hm, fname, &x, &y))
	{
		fileptr = NULL;
		len = FS_LoadFile(fname, (void**)&fileptr);

		if (!qofs_Error(len) && len >= sizeof(*fileptr) && fileptr->magic == SECTION_MAGIC)
			Terr_ReadSection(hm, ver, x, y, fileptr+1, len - sizeof(*fileptr));
		else
			Terr_ReadSection(hm, ver, x, y, NULL, 0);

		if (fileptr)
			FS_FreeFile(fileptr);
		return true;
	}
*/
	return false;
}
#endif

static void Terr_LoadSection(heightmap_t *hm, hmsection_t *s, int sx, int sy, unsigned int flags)
{
#ifndef SERVERONLY
	//when using networked terrain, the client will never load a section from disk, but will only load it from the server
	//one section at a time.
	if (mod_terrain_networked.ival && !sv.state)
	{
		char fname[MAX_QPATH];
		if (flags & TGS_NODOWNLOAD)
			return;
		//try to download it now...
		if (!cl.downloadlist)
			CL_CheckOrEnqueDownloadFile(Terr_DiskSectionName(hm, sx, sy, fname, sizeof(fname)), Terr_TempDiskSectionName(hm, sx, sy), DLLF_OVERWRITE|DLLF_TEMPORARY);
		return;
	}
#endif

	if (!s)
	{
		Terr_GenerateSection(hm, sx, sy, true);
	}
}
static void Terr_LoadSectionWorker(void *ctx, void *data, size_t a, size_t b)
{
	heightmap_t *hm = data;
	hmsection_t *s = ctx;
	int sx = a;
	int sy = b;
	int flags = 0;
	void *diskimage;
	qofs_t len;
	char fname[MAX_QPATH];

	//already processed, or not otherwise valid
	if (s->loadstate != TSLS_LOADING1)
		return;

#if SECTIONSPERBLOCK > 1
	len = FS_LoadFile(Terr_DiskBlockName(hm, sx, sy, fname, sizeof(fname)), (void**)&diskimage);
	if (!qofs_Error(len))
	{
		int offset;
		int x, y;
		int ver;
		dblock_t *block = diskimage;
		if (block->magic != SECTION_MAGIC || !(block->ver & 0x80000000))
		{
			//give it a dummy so we don't constantly hit the disk
			Terr_ReadSection(hm, s, 0, NULL, 0);
		}
		else
		{
			sx&=~(SECTIONSPERBLOCK-1);
			sy&=~(SECTIONSPERBLOCK-1);

			ver = block->ver & ~0x80000000;
			for (y = 0; y < SECTIONSPERBLOCK; y++)
				for (x = 0; x < SECTIONSPERBLOCK; x++)
				{
					//noload avoids recursion.
					s = Terr_GenerateSection(hm, sx+x, sy+y, false);
					if (s->loadstate == TSLS_LOADING1)
					{
						offset = block->offset[x + y*SECTIONSPERBLOCK];
						if (!offset)
							Terr_ReadSection(hm, s, ver, NULL, 0);	//no data in the file for this section
						else
							Terr_ReadSection(hm, s, ver, (char*)diskimage + offset, len - offset);
					}
				}
		}
		FS_FreeFile(diskimage);
		return;
	}
#endif

	//legacy one-section-per-file format.
	len = FS_LoadFile(Terr_DiskSectionName(hm, sx, sy, fname, sizeof(fname)), (void**)&diskimage);
	if (!qofs_Error(len))
	{
		dsection_t *h = diskimage;
		if (len >= sizeof(*h) && h->magic == SECTION_MAGIC)
		{
			Terr_ReadSection(hm, s, h->ver, h+1, len-sizeof(*h));
			FS_FreeFile(diskimage);
			return;
		}
		if (diskimage)
			FS_FreeFile(diskimage);
	}

#ifdef ADT
	if (Terr_ImportADT(hm, sx, sy, flags))
		return;
#endif

	//generate a dummy one
	Terr_ReadSection(hm, s, 0, NULL, 0);
}

#ifndef SERVERONLY
static void Terr_SaveV1(heightmap_t *hm, hmsection_t *s, vfsfile_t *f, int sx, int sy)
{
	int i;
	dsmesh_v1_t dm;
	qbyte *lm;
	dsection_v1_t ds;
	vec4_t dcolours[SECTHEIGHTSIZE*SECTHEIGHTSIZE];
	int nothing = 0;
	struct hmwater_s *w = s->water;

	memset(&ds, 0, sizeof(ds));
	memset(&dm, 0, sizeof(dm));

	//mask off the flags which are only valid in memory
	ds.flags = s->flags & ~(TSF_INTERNAL|TSF_HASWATER_V0);

	//kill the haswater flag if its entirely above any possible water anyway.
	if (w)
		ds.flags |= TSF_HASWATER_V0;
	ds.flags &= ~TSF_HASCOLOURS;	//recalculated

	Q_strncpyz(ds.texname[0], s->texname[0], sizeof(ds.texname[0]));
	Q_strncpyz(ds.texname[1], s->texname[1], sizeof(ds.texname[1]));
	Q_strncpyz(ds.texname[2], s->texname[2], sizeof(ds.texname[2]));
	Q_strncpyz(ds.texname[3], s->texname[3], sizeof(ds.texname[3]));

	for (i = 0; i < 8*8; i++)
	{
		int x = (i & 7);
		int y = (i>>3);
		int b = (1u<<(x>>1)) << ((y>>1)<<2);
		if (s->holes[y] & (1u<<x))
			ds.holes |= b;
	}

	//make sure the user can see the holes they just saved.
	memset(s->holes, 0, sizeof(s->holes));
	for (i = 0; i < 8*8; i++)
	{
		int x = (i & 7);
		int y = (i>>3);
		int b = (1u<<(x>>1)) << ((y>>1)<<2);
		if (ds.holes & b)
			s->holes[y] |= 1u<<x;
	}
	s->flags |= TSF_DIRTY;

	lm = lightmap[s->lightmap]->lightmaps;
	lm += (s->lmy * HMLMSTRIDE + s->lmx) * lightmap_bytes;
	for (i = 0; i < SECTTEXSIZE; i++)
	{
		memcpy(ds.texmap + i, lm, sizeof(ds.texmap[0]));
		lm += (HMLMSTRIDE)*lightmap_bytes;
	}

	for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
	{
		ds.heights[i] = LittleFloat(s->heights[i]);

		if (s->colours[i][0] != 1 || s->colours[i][1] != 1 || s->colours[i][2] != 1 || s->colours[i][3] != 1)
		{
			ds.flags |= TSF_HASCOLOURS;
			dcolours[i][0] = LittleFloat(s->colours[i][0]);
			dcolours[i][1] = LittleFloat(s->colours[i][1]);
			dcolours[i][2] = LittleFloat(s->colours[i][2]);
			dcolours[i][3] = LittleFloat(s->colours[i][3]);
		}
		else
		{
			dcolours[i][0] = dcolours[i][1] = dcolours[i][2] = dcolours[i][3] = LittleFloat(1);
		}
	}
	ds.waterheight = w?w->heights[4*8+4]:s->minh;
	ds.minh = s->minh;
	ds.maxh = s->maxh;
	Sys_LockMutex(hm->entitylock);
	ds.ents_num = s->numents;

	VFS_WRITE(f, &ds, sizeof(ds));
	if (ds.flags & TSF_HASCOLOURS)
		VFS_WRITE(f, dcolours, sizeof(dcolours));
	for (i = 0; i < s->numents; i++)
	{
		int pad;
		dm.scale = s->ents[i]->ent.scale;
		VectorCopy(s->ents[i]->ent.axis[0], dm.axisorg[0]);
		VectorCopy(s->ents[i]->ent.axis[1], dm.axisorg[1]);
		VectorCopy(s->ents[i]->ent.axis[2], dm.axisorg[2]);
		VectorCopy(s->ents[i]->ent.origin, dm.axisorg[3]);
		dm.axisorg[3][0] += (CHUNKBIAS-sx)*hm->sectionsize;
		dm.axisorg[3][1] += (CHUNKBIAS-sy)*hm->sectionsize;
		dm.size = sizeof(dm) + strlen(s->ents[i]->ent.model->name) + 1;
		if (dm.size & 3)
			pad = 4 - (dm.size&3);
		else
			pad = 0;
		dm.size += pad;
		VFS_WRITE(f, &dm, sizeof(dm));
		VFS_WRITE(f, s->ents[i]->ent.model->name, strlen(s->ents[i]->ent.model->name)+1);
		if (pad)
			VFS_WRITE(f, &nothing, pad);
	}
	Sys_UnlockMutex(hm->entitylock);
}

static void Terr_Save(heightmap_t *hm, hmsection_t *s, vfsfile_t *f, int sx, int sy, int ver)
{
	if (ver == 1)
		Terr_SaveV1(hm, s, f, sx, sy);
	else if (ver == 2)
		Terr_SaveV2(hm, s, f, sx, sy);
}
#endif

//doesn't clear edited/dirty flags or anything
static qboolean Terr_SaveSection(heightmap_t *hm, hmsection_t *s, int sx, int sy, qboolean blocksave)
{
#ifdef SERVERONLY
	return true;
#else
	vfsfile_t *f;
	char fname[MAX_QPATH];
	int x, y;
	int writever = mod_terrain_savever.ival;
	if (!writever)
		writever = SECTION_VER_DEFAULT;
	//if its invalid or doesn't contain all the data...
	if (!s || s->lightmap < 0)
		return true;

#if SECTIONSPERBLOCK > 1
	if (blocksave)
	{
		dblock_t dbh;
		sx = sx & ~(SECTIONSPERBLOCK-1);
		sy = sy & ~(SECTIONSPERBLOCK-1);

		//make sure its loaded before we replace the file
		for (y = 0; y < SECTIONSPERBLOCK; y++)
		{
			for (x = 0; x < SECTIONSPERBLOCK; x++)
			{
				s = Terr_GetSection(hm, sx+x, sy+y, TGS_WAITLOAD|TGS_NODOWNLOAD);
				if (s)
					s->flags |= TSF_EDITED;	//stop them from getting reused for something else.
			}
		}

		//make sure all lightmap info was loaded.
		COM_WorkerFullSync();

		Terr_DiskBlockName(hm, sx, sy, fname, sizeof(fname));
		FS_CreatePath(fname, FS_GAMEONLY);
		f = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
		if (!f)
		{
			Con_Printf("Failed to open %s\n", fname);
			return false;
		}

		memset(&dbh, 0, sizeof(dbh));
		dbh.magic = LittleLong(SECTION_MAGIC);
		dbh.ver = LittleLong(writever | 0x80000000);
		VFS_WRITE(f, &dbh, sizeof(dbh));
		for (y = 0; y < SECTIONSPERBLOCK; y++)
		{
			for (x = 0; x < SECTIONSPERBLOCK; x++)
			{
				s = Terr_GetSection(hm, sx+x, sy+y, TGS_WAITLOAD|TGS_NODOWNLOAD);
				if (s && s->loadstate == TSLS_LOADED)
				{
					dbh.offset[y*SECTIONSPERBLOCK + x] = VFS_TELL(f);
					Terr_Save(hm, s, f, sx+x, sy+y, writever);
					s->flags &= ~TSF_EDITED;
				}
				else
					dbh.offset[y*SECTIONSPERBLOCK + x] = 0;
			}
		}

		VFS_SEEK(f, 0);
		VFS_WRITE(f, &dbh, sizeof(dbh));
		VFS_CLOSE(f);
	}
	else
#endif
	{
		dsection_t dsh;
		Terr_DiskSectionName(hm, sx, sy, fname, sizeof(fname));

//		if (s && (s->flags & (TSF_EDITED|TSF_FAILEDLOAD)) != TSF_FAILEDLOAD)
//			return FS_Remove(fname, FS_GAMEONLY);	//delete the file if the section got reverted to default, and wasn't later modified.

		//make sure all lightmap info was loaded.
		COM_WorkerFullSync();

		FS_CreatePath(fname, FS_GAMEONLY);
		f = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
		if (!f)
		{
			Con_Printf("Failed to open %s\n", fname);
			return false;
		}

		memset(&dsh, 0, sizeof(dsh));
		dsh.magic = SECTION_MAGIC;
		dsh.ver = writever;
		VFS_WRITE(f, &dsh, sizeof(dsh));
		Terr_Save(hm, s, f, sx, sy, writever);
		VFS_CLOSE(f);
	}
	return true;
#endif
}

/*convienience function*/
static hmsection_t *Terr_GetSection(heightmap_t *hm, int x, int y, unsigned int flags)
{
	hmcluster_t *cluster;
	hmsection_t *section;
	int cx = x / MAXSECTIONS;
	int cy = y / MAXSECTIONS;
	int sx = x & (MAXSECTIONS-1);
	int sy = y & (MAXSECTIONS-1);
	cluster = hm->cluster[cx + cy*MAXCLUSTERS];
	if (!cluster)
		section = NULL;
	else
		section = cluster->section[sx + sy*MAXSECTIONS];
	if (!section)
	{
		if (flags & (TGS_LAZYLOAD|TGS_TRYLOAD|TGS_WAITLOAD))
		{
			if ((flags & TGS_LAZYLOAD) && hm->loadingsections)
				return NULL;
			section = Terr_GenerateSection(hm, x, y, true);
		}
	}
#ifndef SERVERONLY
	//when using networked terrain, the client will never load a section from disk, but only loading it from the server
	//this means we need to send a new request to download the section if it was flagged as modified.
	if (!(flags & TGS_NODOWNLOAD))
	if (section && (section->flags & TSF_NOTIFY) && mod_terrain_networked.ival && !sv.state)
	{
		//try to download it now...
		if (!cl.downloadlist)
		{
			char fname[MAX_QPATH];
			CL_CheckOrEnqueDownloadFile(Terr_DiskSectionName(hm, x, y, fname, sizeof(fname)), Terr_TempDiskSectionName(hm, x, y), DLLF_OVERWRITE|DLLF_TEMPORARY);

			section->flags &= ~TSF_NOTIFY;
		}
	}
#endif

	if (section)
	{
		//wait for it to load if we're meant to be doing that.
		if (section->loadstate == TSLS_LOADING1 && (flags & TGS_WAITLOAD))
		{
			//process the load
			COM_WorkerPartialSync(section, &section->loadstate, TSLS_LOADING1);
		}
		if (section->loadstate == TSLS_LOADING2 && (flags & TGS_WAITLOAD))
			COM_MainThreadFlush();	//make sure any associated lightmaps also got read+handled

		//if it failed, generate a default (for editing)
		if (section->loadstate == TSLS_FAILED && (flags & TGS_DEFAULTONFAIL))
		{
			section->flags = (section->flags & ~TSF_EDITED);
			Terr_ClearSection(section);
			Terr_GenerateDefault(hm, section);
		}


		if ((section->loadstate != TSLS_LOADED) && !(flags & TGS_ANYSTATE))
			section = NULL;
	}

	return section;
}

/*save all currently loaded sections*/
int Heightmap_Save(heightmap_t *hm)
{
	hmsection_t *s;
	int x, y;
	int sectionssaved = 0;
	for (x = hm->firstsegx; x < hm->maxsegx; x++)
	{
		for (y = hm->firstsegy; y < hm->maxsegy; y++)
		{
			s = Terr_GetSection(hm, x, y, TGS_NOLOAD);
			if (!s)
				continue;
			if (s->flags & TSF_EDITED)
			{
/*				//make sure all the parts are loaded before trying to write them, so we don't try reading partial files, which would be bad, mmkay?
				for (sy = y&~(SECTIONSPERBLOCK-1); sy < y+SECTIONSPERBLOCK && sy < hm->maxsegy; sy++)
				{
					for (sx = x&~(SECTIONSPERBLOCK-1); sx < x+SECTIONSPERBLOCK && sx < hm->maxsegx; sx++)
					{
						os = Terr_GetSection(hm, sx, sy, TGS_WAITLOAD|TGS_NODOWNLOAD|TGS_NORENDER);
						if (os)
							os->flags |= TSF_EDITED;
					}
				}
*/

				if (Terr_SaveSection(hm, s, x, y, true))
				{
					s->flags &= ~TSF_EDITED;
					sectionssaved++;
				}
			}
		}
	}

	return sectionssaved;
}

#ifndef CLIENTONLY
//on servers, we can get requests to download current map sections. if so, give them it.
qboolean Terrain_LocateSection(char *name, flocation_t *loc)
{
	heightmap_t *hm;
	hmsection_t *s;
	int x, y;
	char fname[MAX_QPATH];

	//reject if its not in maps
	if (strncmp(name, "maps/", 5))
		return false;

	if (!sv.world.worldmodel)
		return false;
	hm = sv.world.worldmodel->terrain;
	if (!Terr_IsSectionFName(hm, name, &x, &y))
		return false;

	//verify that its valid
	if (strcmp(name, Terr_DiskSectionName(hm, x, y, fname, sizeof(fname))))
		return false;

	s = Terr_GetSection(hm, x, y, TGS_NOLOAD);
	if (!s || !(s->flags & TSF_EDITED))
		return false;	//its not been edited, might as well just use the regular file

	if (!Terr_SaveSection(hm, s, x, y, false))
		return false;

	return FS_FLocateFile(name, FSLFRT_IFFOUND, loc);
}
#endif

void Terr_DestroySection(heightmap_t *hm, hmsection_t *s, qboolean lightmapreusable)
{
	if (s && s->loadstate == TSLS_LOADING1)
		COM_WorkerPartialSync(s, &s->loadstate, TSLS_LOADING1);
	if (s && s->loadstate == TSLS_LOADING2)
		COM_MainThreadFlush();	//make sure any associated lightmaps also got read+handled

	RemoveLink(&s->recycle);

	Terr_ClearSection(s);

#ifndef SERVERONLY
	if (s->lightmap >= 0)
	{
		struct lmsect_s *lms;

		if (lightmapreusable)
		{
			lms = BZ_Malloc(sizeof(*lms));
			lms->lm = s->lightmap;
			lms->x = s->lmx;
			lms->y = s->lmy;
			lms->next = hm->unusedlmsects;
			hm->unusedlmsects = lms;
			hm->numunusedlmsects++;
		}
		hm->numusedlmsects--;
	}

	if (hm->relight == s)
		hm->relight = NULL;

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL && qglDeleteBuffersARB)
	{
		qglDeleteBuffersARB(1, &s->vbo.coord.gl.vbo);
		qglDeleteBuffersARB(1, &s->vbo.indicies.gl.vbo);
	}
#endif

	Z_Free(s->ents);
	Z_Free(s->mesh.xyz_array);
	Z_Free(s->mesh.indexes);
#endif

	Z_Free(s);

	hm->activesections--;
}

#ifndef SERVERONLY
//dedicated servers do not support editing. no lightmap info causes problems.

//when a terrain section has the notify flag set on the server, the server needs to go through and set out notifications to replicate it to the various clients
//so the clients know to re-download the section.
static void Terr_DoEditNotify(heightmap_t *hm)
{
	int i;
	char *cmd;
	hmsection_t *s;
	link_t *ln = &hm->recycle;

	if (!sv.state)
		return;

	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (svs.clients[i].state >= cs_connected && svs.clients[i].netchan.remote_address.type != NA_LOOPBACK)
		{
			if (svs.clients[i].backbuf.cursize)
				return;
		}
	}

	for (ln = &hm->recycle; ln->next != &hm->recycle; ln = &s->recycle)
	{
		s = (hmsection_t*)ln->next;
		if (s->flags & TSF_NOTIFY)
		{
			s->flags &= ~TSF_NOTIFY;
			cmd = va("mod_terrain_reload %s %i %i\n", hm->path, s->sx - CHUNKBIAS, s->sy - CHUNKBIAS);
			for (i = 0; i < sv.allocated_client_slots; i++)
			{
				if (svs.clients[i].state >= cs_connected && svs.clients[i].netchan.remote_address.type != NA_LOOPBACK)
				{
					SV_StuffcmdToClient(&svs.clients[i], cmd);
				}
			}
			return;
		}
	}
}

//garbage collect the oldest section, to make space for another
static qboolean Terr_Collect(heightmap_t *hm)
{
	hmcluster_t *c;
	hmsection_t *s;
	int cx, cy;
	int sx, sy;

	link_t *ln = &hm->recycle;
	for (ln = &hm->recycle; ln->next != &hm->recycle; )
	{
		s = (hmsection_t*)ln->next;
		if ((s->flags & TSF_EDITED) || s->loadstate == TSLS_LOADING1 || s->loadstate == TSLS_LOADING2)
			ln = &s->recycle;
		else
		{
			cx = s->sx/MAXSECTIONS;
			cy = s->sy/MAXSECTIONS;
			c = hm->cluster[cx + cy*MAXCLUSTERS];
			sx = s->sx & (MAXSECTIONS-1);
			sy = s->sy & (MAXSECTIONS-1);
			if (c->section[sx+sy*MAXSECTIONS] != s)
				Sys_Error("invalid section collection");
			c->section[sx+sy*MAXSECTIONS] = NULL;

#if 0
			if (hm->relight == s)
				hm->relight = NULL;
			RemoveLink(&s->recycle);
			InsertLinkAfter(&s->recycle, &hm->collected);
			hm->activesections--;
#else
			Terr_DestroySection(hm, s, true);
#endif
			return true;
		}
	}
	return false;
}
#endif

/*purge all sections, but not root
lightmaps only are purged whenever the client rudely kills lightmaps
we'll reload those when its next seen.
(lightmaps will already have been destroyed, so no poking them)
*/
void Terr_PurgeTerrainModel(model_t *mod, qboolean lightmapsonly, qboolean lightmapreusable)
{
	heightmap_t *hm = mod->terrain;
	hmcluster_t *c;
	hmsection_t *s;
	int cx, cy;
	int sx, sy;

//	Con_Printf("PrePurge: %i lm chunks used, %i unused\n", hm->numusedlmsects, hm->numunusedlmsects);

	for (cy = 0; cy < MAXCLUSTERS; cy++)
	for (cx = 0; cx < MAXCLUSTERS; cx++)
	{
		int numremaining = 0;
		c = hm->cluster[cx + cy*MAXCLUSTERS];
		if (!c)
			continue;

		for (sy = 0; sy < MAXSECTIONS; sy++)
		for (sx = 0; sx < MAXSECTIONS; sx++)
		{
			s = c->section[sx + sy*MAXSECTIONS];
			if (!s)
			{
			}
			else if (lightmapsonly)
			{
				numremaining++;
#ifndef SERVERONLY
				s->lightmap = -1;
#endif
			}
			else
			{
				c->section[sx+sy*MAXSECTIONS] = NULL;

				Terr_DestroySection(hm, s, lightmapreusable);
			}
		}
		if (!numremaining)
		{
			hm->cluster[cx + cy*MAXSECTIONS] = NULL;
			BZ_Free(c);
		}
	}
#ifndef SERVERONLY
	if (!lightmapreusable)
	{
		while (hm->unusedlmsects)
		{
			struct lmsect_s *lms;
			lms = hm->unusedlmsects;
			hm->unusedlmsects = lms->next;
			BZ_Free(lms);

			hm->numunusedlmsects--;
		}
	}
#endif

//	Con_Printf("PostPurge: %i lm chunks used, %i unused\n", hm->numusedlmsects, hm->numunusedlmsects);
}
void Terr_FreeModel(model_t *mod)
{
	heightmap_t *hm = mod->terrain;
	if (hm)
	{
		Terr_PurgeTerrainModel(mod, false, false);
		Z_Free(hm);
		mod->terrain = NULL;
	}
}
#ifndef SERVERONLY
void Terr_DrawTerrainWater(heightmap_t *hm, float *mins, float *maxs, struct hmwater_s *w)
{
	scenetris_t *t;
	int flags = BEF_NOSHADOWS;
	int firstv;
	int y, x;
	
	//need to filter by height too, or reflections won't work properly.
	if (cl_numstris && cl_stris[cl_numstris-1].shader == w->shader && cl_stris[cl_numstris-1].flags == flags && cl_strisvertv[cl_stris[cl_numstris-1].firstvert][2] == w->maxheight)
	{
		t = &cl_stris[cl_numstris-1];
	}
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris+=8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = w->shader;
		t->flags = flags;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
		t->numvert = 0;
		t->numidx = 0;
	}

	if (!w->simple)
	{
		float step = (maxs[0] - mins[0]) / 8;
		if (cl_numstrisidx+9*9*6 > cl_maxstrisidx)
		{
			cl_maxstrisidx=cl_numstrisidx+12 + 9*9*6*4;
			cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
		}
		if (cl_numstrisvert+9*9 > cl_maxstrisvert)
		{
			cl_maxstrisvert+=9*9+64;
			cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
			cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(*cl_strisvertt)*cl_maxstrisvert);
			cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(*cl_strisvertc)*cl_maxstrisvert);
		}

		firstv = t->numvert;
		for (y = 0; y < 9; y++)
		{
			for (x = 0; x < 9; x++)
			{
				cl_strisvertv[cl_numstrisvert][0] = mins[0] + step*x;
				cl_strisvertv[cl_numstrisvert][1] = mins[1] + step*y;
				cl_strisvertv[cl_numstrisvert][2] = w->heights[x + y*9];
				cl_strisvertt[cl_numstrisvert][0] = cl_strisvertv[cl_numstrisvert][0]/64;
				cl_strisvertt[cl_numstrisvert][1] = cl_strisvertv[cl_numstrisvert][1]/64;
				Vector4Set(cl_strisvertc[cl_numstrisvert], 1,1,1,1)
				cl_numstrisvert++;
			}
		}
		for (y = 0; y < 8; y++)
		{
			for (x = 0; x < 8; x++)
			{
				if (w->holes[y] & (1u<<x))
					continue;
				cl_strisidx[cl_numstrisidx++] = firstv+(x+0)+(y+0)*9;
				cl_strisidx[cl_numstrisidx++] = firstv+(x+0)+(y+1)*9;
				cl_strisidx[cl_numstrisidx++] = firstv+(x+1)+(y+0)*9;

				cl_strisidx[cl_numstrisidx++] = firstv+(x+1)+(y+0)*9;
				cl_strisidx[cl_numstrisidx++] = firstv+(x+0)+(y+1)*9;
				cl_strisidx[cl_numstrisidx++] = firstv+(x+1)+(y+1)*9;
			}
		}
		t->numidx = cl_numstrisidx - t->firstidx;
		t->numvert = cl_numstrisvert - t->firstvert;
	}
	else
	{
		if (cl_numstrisidx+12 > cl_maxstrisidx)
		{
			cl_maxstrisidx=cl_numstrisidx+12 + 64;
			cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
		}
		if (cl_numstrisvert+4 > cl_maxstrisvert)
		{
			cl_maxstrisvert+=64;
			cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
			cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(*cl_strisvertt)*cl_maxstrisvert);
			cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(*cl_strisvertc)*cl_maxstrisvert);
		}

		{
			VectorSet(cl_strisvertv[cl_numstrisvert], mins[0], mins[1], w->maxheight);
			Vector4Set(cl_strisvertc[cl_numstrisvert], 1,1,1,1)
			Vector2Set(cl_strisvertt[cl_numstrisvert], mins[0]/64, mins[1]/64);
			cl_numstrisvert++;

			VectorSet(cl_strisvertv[cl_numstrisvert], mins[0], maxs[1], w->maxheight);
			Vector4Set(cl_strisvertc[cl_numstrisvert], 1,1,1,1)
			Vector2Set(cl_strisvertt[cl_numstrisvert], mins[0]/64, maxs[1]/64);
			cl_numstrisvert++;

			VectorSet(cl_strisvertv[cl_numstrisvert], maxs[0], maxs[1], w->maxheight);
			Vector4Set(cl_strisvertc[cl_numstrisvert], 1,1,1,1)
			Vector2Set(cl_strisvertt[cl_numstrisvert], maxs[0]/64, maxs[1]/64);
			cl_numstrisvert++;

			VectorSet(cl_strisvertv[cl_numstrisvert], maxs[0], mins[1], w->maxheight);
			Vector4Set(cl_strisvertc[cl_numstrisvert], 1,1,1,1)
			Vector2Set(cl_strisvertt[cl_numstrisvert], maxs[0]/64, mins[1]/64);
			cl_numstrisvert++;
		}


		firstv = t->numvert;

		/*build the triangles*/
		cl_strisidx[cl_numstrisidx++] = firstv + 0;
		cl_strisidx[cl_numstrisidx++] = firstv + 1;
		cl_strisidx[cl_numstrisidx++] = firstv + 2;

		cl_strisidx[cl_numstrisidx++] = firstv + 0;
		cl_strisidx[cl_numstrisidx++] = firstv + 2;
		cl_strisidx[cl_numstrisidx++] = firstv + 3;

		cl_strisidx[cl_numstrisidx++] = firstv + 3;
		cl_strisidx[cl_numstrisidx++] = firstv + 2;
		cl_strisidx[cl_numstrisidx++] = firstv + 1;

		cl_strisidx[cl_numstrisidx++] = firstv + 3;
		cl_strisidx[cl_numstrisidx++] = firstv + 1;
		cl_strisidx[cl_numstrisidx++] = firstv + 0;


		t->numidx = cl_numstrisidx - t->firstidx;
		t->numvert = cl_numstrisvert - t->firstvert;
	}
}

static void Terr_RebuildMesh(model_t *model, hmsection_t *s, int x, int y)
{
	int vx, vy;
	int v;
	mesh_t *mesh = &s->mesh;
	heightmap_t *hm = s->hmmod;
	
	Terr_InitLightmap(s, false);

	s->minh = 9999999999999999.f;
	s->maxh = -9999999999999999.f;

	switch(hm->mode)
	{
	case HMM_BLOCKS:
		//tiles, like dungeon keeper
		if (mesh->xyz_array)
			BZ_Free(mesh->xyz_array);
		{
			mesh->xyz_array = BZ_Malloc((sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)) * (SECTHEIGHTSIZE-1)*(SECTHEIGHTSIZE-1)*4*3);
			mesh->st_array = (void*) (mesh->xyz_array + (SECTHEIGHTSIZE-1)*(SECTHEIGHTSIZE-1)*4*3);
			mesh->lmst_array[0] = (void*) (mesh->st_array + (SECTHEIGHTSIZE-1)*(SECTHEIGHTSIZE-1)*4*3);
		}
		mesh->numvertexes = 0;

		if (mesh->indexes)
			BZ_Free(mesh->indexes);
		mesh->indexes = BZ_Malloc(sizeof(index_t) * SECTHEIGHTSIZE*SECTHEIGHTSIZE*6*3);
		mesh->numindexes = 0;
		mesh->colors4f_array[0] = NULL;

		for (vy = 0; vy < SECTHEIGHTSIZE-1; vy++)
		{
			for (vx = 0; vx < SECTHEIGHTSIZE-1; vx++)
			{
				float st[2], inst[2];
#if SECTHEIGHTSIZE == 17
				int holebit;
				int holerow;

				//skip generation of the mesh above holes
				holerow = ((vy<<3)/(SECTHEIGHTSIZE-1));
				holebit = 1u<<((vx<<3)/(SECTHEIGHTSIZE-1));
				if (s->holes[holerow] & holebit)
					continue;
#endif

				//top face
				v = mesh->numvertexes;
				mesh->numvertexes += 4;
				mesh->xyz_array[v+0][0] = (x-CHUNKBIAS + (vx+0)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+0][1] = (y-CHUNKBIAS + (vy+0)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+0][2] = s->heights[vx + vy*SECTHEIGHTSIZE];

				mesh->xyz_array[v+1][0] = (x-CHUNKBIAS + (vx+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+1][1] = (y-CHUNKBIAS + (vy+0)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+1][2] = s->heights[vx + vy*SECTHEIGHTSIZE];

				mesh->xyz_array[v+2][0] = (x-CHUNKBIAS + (vx+0)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+2][1] = (y-CHUNKBIAS + (vy+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+2][2] = s->heights[vx + vy*SECTHEIGHTSIZE];

				mesh->xyz_array[v+3][0] = (x-CHUNKBIAS + (vx+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+3][1] = (y-CHUNKBIAS + (vy+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+3][2] = s->heights[vx + vy*SECTHEIGHTSIZE];

				if (s->maxh < mesh->xyz_array[v][2])
					s->maxh = mesh->xyz_array[v][2];
				if (s->minh > mesh->xyz_array[v][2])
					s->minh = mesh->xyz_array[v][2];

				st[0] = 1.0f/hm->tilecount[0] * vx;
				st[1] = 1.0f/hm->tilecount[1] * vy;
				inst[0] = 0.5f/(hm->tilecount[0]*hm->tilepixcount[0]);
				inst[1] = 0.5f/(hm->tilecount[1]*hm->tilepixcount[1]);
				mesh->st_array[v+0][0] = st[0]+inst[0];
				mesh->st_array[v+0][1] = st[1]+inst[1];
				mesh->st_array[v+1][0] = st[0]-inst[0]+1.0f/hm->tilecount[0];
				mesh->st_array[v+1][1] = st[1]+inst[1];
				mesh->st_array[v+2][0] = st[0]+inst[0];
				mesh->st_array[v+2][1] = st[1]-inst[1]+1.0f/hm->tilecount[1];
				mesh->st_array[v+3][0] = st[0]-inst[0]+1.0f/hm->tilecount[0];
				mesh->st_array[v+3][1] = st[1]-inst[1]+1.0f/hm->tilecount[1];

				//calc the position in the range -0.5 to 0.5
				mesh->lmst_array[0][v][0] = (((float)vx / (SECTHEIGHTSIZE-1))-0.5);
				mesh->lmst_array[0][v][1] = (((float)vy / (SECTHEIGHTSIZE-1))-0.5);
				//scale down to a half-texel
				mesh->lmst_array[0][v][0] *= (SECTTEXSIZE-1.0f)/HMLMSTRIDE;
				mesh->lmst_array[0][v][1] *= (SECTTEXSIZE-1.0f)/HMLMSTRIDE;
				//bias it
				mesh->lmst_array[0][v][0] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmx) / HMLMSTRIDE);
				mesh->lmst_array[0][v][1] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmy) / HMLMSTRIDE);

				mesh->indexes[mesh->numindexes++] = v+0;
				mesh->indexes[mesh->numindexes++] = v+2;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+2;
				mesh->indexes[mesh->numindexes++] = v+1+2;


				//x boundary
				v = mesh->numvertexes;
				mesh->numvertexes += 4;
				mesh->xyz_array[v+0][0] = (x-CHUNKBIAS + (vx+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+0][1] = (y-CHUNKBIAS + (vy+0)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+0][2] = s->heights[vx+0 + vy*SECTHEIGHTSIZE];

				mesh->xyz_array[v+1][0] = (x-CHUNKBIAS + (vx+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+1][1] = (y-CHUNKBIAS + (vy+0)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+1][2] = s->heights[(vx+1) + vy*SECTHEIGHTSIZE];

				mesh->xyz_array[v+2][0] = (x-CHUNKBIAS + (vx+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+2][1] = (y-CHUNKBIAS + (vy+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+2][2] = s->heights[(vx+0) + vy*SECTHEIGHTSIZE];

				mesh->xyz_array[v+3][0] = (x-CHUNKBIAS + (vx+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+3][1] = (y-CHUNKBIAS + (vy+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+3][2] = s->heights[(vx+1) + vy*SECTHEIGHTSIZE];

				if (s->maxh < mesh->xyz_array[v][2])
					s->maxh = mesh->xyz_array[v][2];
				if (s->minh > mesh->xyz_array[v][2])
					s->minh = mesh->xyz_array[v][2];

				st[0] = 1.0f/hm->tilecount[0] * vx;
				st[1] = 1.0f/hm->tilecount[1] * vy;
				inst[0] = 0.5f/(hm->tilecount[0]*hm->tilepixcount[0]);
				inst[1] = 0.5f/(hm->tilecount[1]*hm->tilepixcount[1]);
				mesh->st_array[v+0][0] = st[0]+inst[0];
				mesh->st_array[v+0][1] = st[1]+inst[1];
				mesh->st_array[v+1][0] = st[0]+inst[0];
				mesh->st_array[v+1][1] = st[1]-inst[1]+1.0f/hm->tilecount[1];
				mesh->st_array[v+2][0] = st[0]-inst[0]+1.0f/hm->tilecount[0];
				mesh->st_array[v+2][1] = st[1]+inst[1];
				mesh->st_array[v+3][0] = st[0]-inst[0]+1.0f/hm->tilecount[0];
				mesh->st_array[v+3][1] = st[1]-inst[1]+1.0f/hm->tilecount[1];

				//calc the position in the range -0.5 to 0.5
				mesh->lmst_array[0][v][0] = (((float)vx / (SECTHEIGHTSIZE-1))-0.5);
				mesh->lmst_array[0][v][1] = (((float)vy / (SECTHEIGHTSIZE-1))-0.5);
				//scale down to a half-texel
				mesh->lmst_array[0][v][0] *= (SECTTEXSIZE-1.0f)/HMLMSTRIDE;
				mesh->lmst_array[0][v][1] *= (SECTTEXSIZE-1.0f)/HMLMSTRIDE;
				//bias it
				mesh->lmst_array[0][v][0] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmx) / HMLMSTRIDE);
				mesh->lmst_array[0][v][1] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmy) / HMLMSTRIDE);


				mesh->indexes[mesh->numindexes++] = v+0;
				mesh->indexes[mesh->numindexes++] = v+2;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+2;
				mesh->indexes[mesh->numindexes++] = v+1+2;

				//y boundary
				v = mesh->numvertexes;
				mesh->numvertexes += 4;
				mesh->xyz_array[v+0][0] = (x-CHUNKBIAS + (vx+0)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+0][1] = (y-CHUNKBIAS + (vy+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+0][2] = s->heights[vx + (vy+0)*SECTHEIGHTSIZE];

				mesh->xyz_array[v+1][0] = (x-CHUNKBIAS + (vx+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+1][1] = (y-CHUNKBIAS + (vy+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+1][2] = s->heights[vx + (vy+0)*SECTHEIGHTSIZE];

				mesh->xyz_array[v+2][0] = (x-CHUNKBIAS + (vx+0)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+2][1] = (y-CHUNKBIAS + (vy+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+2][2] = s->heights[vx + (vy+1)*SECTHEIGHTSIZE];

				mesh->xyz_array[v+3][0] = (x-CHUNKBIAS + (vx+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+3][1] = (y-CHUNKBIAS + (vy+1)/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v+3][2] = s->heights[vx + (vy+1)*SECTHEIGHTSIZE];

				if (s->maxh < mesh->xyz_array[v][2])
					s->maxh = mesh->xyz_array[v][2];
				if (s->minh > mesh->xyz_array[v][2])
					s->minh = mesh->xyz_array[v][2];

				st[0] = 1.0f/hm->tilecount[0] * vx;
				st[1] = 1.0f/hm->tilecount[1] * vy;
				inst[0] = 0.5f/(hm->tilecount[0]*hm->tilepixcount[0]);
				inst[1] = 0.5f/(hm->tilecount[1]*hm->tilepixcount[1]);
				mesh->st_array[v+0][0] = st[0]+inst[0];
				mesh->st_array[v+0][1] = st[1]+inst[1];
				mesh->st_array[v+1][0] = st[0]-inst[0]+1.0f/hm->tilecount[0];
				mesh->st_array[v+1][1] = st[1]+inst[1];
				mesh->st_array[v+2][0] = st[0]+inst[0];
				mesh->st_array[v+2][1] = st[1]-inst[1]+1.0f/hm->tilecount[1];
				mesh->st_array[v+3][0] = st[0]-inst[0]+1.0f/hm->tilecount[0];
				mesh->st_array[v+3][1] = st[1]-inst[1]+1.0f/hm->tilecount[1];

				//calc the position in the range -0.5 to 0.5
				mesh->lmst_array[0][v][0] = (((float)vx / (SECTHEIGHTSIZE-1))-0.5);
				mesh->lmst_array[0][v][1] = (((float)vy / (SECTHEIGHTSIZE-1))-0.5);
				//scale down to a half-texel
				mesh->lmst_array[0][v][0] *= (SECTTEXSIZE-1.0f)/HMLMSTRIDE;
				mesh->lmst_array[0][v][1] *= (SECTTEXSIZE-1.0f)/HMLMSTRIDE;
				//bias it
				mesh->lmst_array[0][v][0] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmx) / HMLMSTRIDE);
				mesh->lmst_array[0][v][1] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmy) / HMLMSTRIDE);

				mesh->indexes[mesh->numindexes++] = v+0;
				mesh->indexes[mesh->numindexes++] = v+2;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+2;
				mesh->indexes[mesh->numindexes++] = v+1+2;
			}
		}
		break;
	case HMM_TERRAIN:
		//smooth terrain
		if (!mesh->xyz_array)
		{
			mesh->xyz_array = BZ_Malloc((sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)) * (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
			mesh->st_array = (void*) (mesh->xyz_array + (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
			mesh->lmst_array[0] = (void*) (mesh->st_array + (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
		}
		mesh->colors4f_array[0] = s->colours;
		mesh->numvertexes = 0;
		/*64 quads across requires 65 verticies*/
		for (vy = 0; vy < SECTHEIGHTSIZE; vy++)
		{
			for (vx = 0; vx < SECTHEIGHTSIZE; vx++)
			{
				v = mesh->numvertexes++;
				mesh->xyz_array[v][0] = (x-CHUNKBIAS + vx/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v][1] = (y-CHUNKBIAS + vy/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
				mesh->xyz_array[v][2] = s->heights[vx + vy*SECTHEIGHTSIZE];

				if (s->maxh < mesh->xyz_array[v][2])
					s->maxh = mesh->xyz_array[v][2];
				if (s->minh > mesh->xyz_array[v][2])
					s->minh = mesh->xyz_array[v][2];

				mesh->st_array[v][0] = mesh->xyz_array[v][0] / 128;
				mesh->st_array[v][1] = mesh->xyz_array[v][1] / 128;

				//calc the position in the range -0.5 to 0.5
				mesh->lmst_array[0][v][0] = (((float)vx / (SECTHEIGHTSIZE-1))-0.5);
				mesh->lmst_array[0][v][1] = (((float)vy / (SECTHEIGHTSIZE-1))-0.5);
				//scale down to a half-texel
				mesh->lmst_array[0][v][0] *= (SECTTEXSIZE-1.0f)/HMLMSTRIDE;
				mesh->lmst_array[0][v][1] *= (SECTTEXSIZE-1.0f)/HMLMSTRIDE;
				//bias it
				mesh->lmst_array[0][v][0] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmx) / HMLMSTRIDE);
				mesh->lmst_array[0][v][1] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmy) / HMLMSTRIDE);
			}
		}

		if (!mesh->indexes)
			mesh->indexes = BZ_Malloc(sizeof(index_t) * SECTHEIGHTSIZE*SECTHEIGHTSIZE*6);

		mesh->numindexes = 0;
		for (vy = 0; vy < SECTHEIGHTSIZE-1; vy++)
		{
			for (vx = 0; vx < SECTHEIGHTSIZE-1; vx++)
			{
	#ifndef STRICTEDGES
				float d1,d2;
	#endif

	#if SECTHEIGHTSIZE == 17
				int holerow;
				int holebit;

				//skip generation of the mesh above holes
				holerow = ((vy<<3)/(SECTHEIGHTSIZE-1));
				holebit = 1u<<((vx<<3)/(SECTHEIGHTSIZE-1));
				if (s->holes[holerow] & holebit)
					continue;
	#endif
				v = vx + vy*(SECTHEIGHTSIZE);

	#ifndef STRICTEDGES
				d1 = fabs(mesh->xyz_array[v][2] - mesh->xyz_array[v+1+SECTHEIGHTSIZE][2]);
				d2 = fabs(mesh->xyz_array[v+1][2] - mesh->xyz_array[v+SECTHEIGHTSIZE][2]);
				if (d1 < d2)
				{
					mesh->indexes[mesh->numindexes++] = v+0;
					mesh->indexes[mesh->numindexes++] = v+1+SECTHEIGHTSIZE;
					mesh->indexes[mesh->numindexes++] = v+1;
					mesh->indexes[mesh->numindexes++] = v+0;
					mesh->indexes[mesh->numindexes++] = v+SECTHEIGHTSIZE;
					mesh->indexes[mesh->numindexes++] = v+1+SECTHEIGHTSIZE;
				}
				else
	#endif
				{
					mesh->indexes[mesh->numindexes++] = v+0;
					mesh->indexes[mesh->numindexes++] = v+SECTHEIGHTSIZE;
					mesh->indexes[mesh->numindexes++] = v+1;
					mesh->indexes[mesh->numindexes++] = v+1;
					mesh->indexes[mesh->numindexes++] = v+SECTHEIGHTSIZE;
					mesh->indexes[mesh->numindexes++] = v+1+SECTHEIGHTSIZE;
				}
			}
		}
		break;
	}

	//pure holes
	if (!mesh->numindexes)
	{
		memset(&s->pvscache, 0, sizeof(s->pvscache));
		return;
	}

	if (s->maxh_cull < s->maxh)
		s->maxh_cull = s->maxh;
	{
		vec3_t mins, maxs;
		mins[0] = (x-CHUNKBIAS) * hm->sectionsize;
		mins[1] = (y-CHUNKBIAS) * hm->sectionsize;
		mins[2] = s->minh;
		maxs[0] = (x+1-CHUNKBIAS) * hm->sectionsize;
		maxs[1] = (y+1-CHUNKBIAS) * hm->sectionsize;
		maxs[2] = s->maxh_cull;
		model->funcs.FindTouchedLeafs(model, &s->pvscache, mins, maxs);
	}

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL && qglGenBuffersARB)
	{
		if (!s->vbo.coord.gl.vbo)
		{
			qglGenBuffersARB(1, &s->vbo.coord.gl.vbo);
			GL_SelectVBO(s->vbo.coord.gl.vbo);
		}
		else
			GL_SelectVBO(s->vbo.coord.gl.vbo);

		qglBufferDataARB(GL_ARRAY_BUFFER_ARB, (sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)+sizeof(vec4_t)) * (mesh->numvertexes), NULL, GL_STATIC_DRAW_ARB);

		qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, (sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)) * mesh->numvertexes, mesh->xyz_array);
		if (mesh->colors4f_array[0])
			qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, (sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)) * mesh->numvertexes, sizeof(vec4_t)*mesh->numvertexes,  mesh->colors4f_array[0]);
		GL_SelectVBO(0);
		s->vbo.coord.gl.addr = 0;
		s->vbo.texcoord.gl.addr = (void*)((char*)mesh->st_array - (char*)mesh->xyz_array);
		s->vbo.texcoord.gl.vbo = s->vbo.coord.gl.vbo;
		s->vbo.lmcoord[0].gl.addr = (void*)((char*)mesh->lmst_array[0] - (char*)mesh->xyz_array);
		s->vbo.lmcoord[0].gl.vbo = s->vbo.coord.gl.vbo;
		s->vbo.colours[0].gl.addr = (void*)((sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)) * mesh->numvertexes);
		s->vbo.colours[0].gl.vbo = s->vbo.coord.gl.vbo;

		if (!s->vbo.indicies.gl.vbo)
			qglGenBuffersARB(1, &s->vbo.indicies.gl.vbo);
		s->vbo.indicies.gl.addr = 0;
		GL_SelectEBO(s->vbo.indicies.gl.vbo);
		qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(index_t) * mesh->numindexes, mesh->indexes, GL_STATIC_DRAW_ARB);
		GL_SelectEBO(0);

#if 1
		Z_Free(mesh->xyz_array);
		mesh->xyz_array = NULL;
		mesh->st_array = NULL;
		mesh->lmst_array[0] = NULL;

		Z_Free(mesh->indexes);
		mesh->indexes = NULL;
#endif
	}
#endif
#ifdef D3D11QUAKE
	if (qrenderer == QR_DIRECT3D11)
	{
		void D3D11BE_GenBatchVBOs(vbo_t **vbochain, batch_t *firstbatch, batch_t *stopbatch);
		batch_t batch = {0};
		mesh_t *meshes = &s->mesh;
		vbo_t *vbo = NULL;
		batch.maxmeshes = 1;
		batch.mesh = &meshes;

		//BE_ClearVBO(&s->vbo);
		D3D11BE_GenBatchVBOs(&vbo, &batch, NULL);
		s->vbo = *vbo;
	}
#endif
}

struct tdibctx
{
	heightmap_t *hm;
	int vx;
	int vy;
	entity_t *ent;
	batch_t **batches;
	qbyte *pvs;
	model_t *wmodel;
};
void Terr_DrawInBounds(struct tdibctx *ctx, int x, int y, int w, int h)
{
	vec3_t mins, maxs;
	hmsection_t *s;
	struct hmwater_s *wa;
	int i, j;
	batch_t *b;
	heightmap_t *hm = ctx->hm;

	mins[0] = (x+0 - CHUNKBIAS)*hm->sectionsize;
	maxs[0] = (x+w - CHUNKBIAS)*hm->sectionsize;

	mins[1] = (y+0 - CHUNKBIAS)*hm->sectionsize;
	maxs[1] = (y+h - CHUNKBIAS)*hm->sectionsize;

	mins[2] = r_origin[2]-999999;
	maxs[2] = r_origin[2]+999999;
	if (R_CullBox(mins, maxs))
		return;

	if (w == 1 && h == 1)
	{
		s = Terr_GetSection(hm, x, y, TGS_LAZYLOAD);
		if (!s)
			return;

		/*move to head*/
		RemoveLink(&s->recycle);
		InsertLinkBefore(&s->recycle, &hm->recycle);

		if (s->lightmap < 0)
			Terr_LoadSection(hm, s, x, y, TGS_NODOWNLOAD);

		if (s->flags & TSF_RELIGHT)
		{
			if (!hm->relight)
			{
				hm->relight = s;
				hm->relightidx = 0;
				hm->relightmin[0] = mins[0];
				hm->relightmin[1] = mins[1];
			}
		}

		if (s->flags & TSF_DIRTY)
		{
			s->flags &= ~TSF_DIRTY;

			Terr_RebuildMesh(ctx->wmodel, s, x, y);
		}

		if (ctx->pvs && !ctx->wmodel->funcs.EdictInFatPVS(ctx->wmodel, &s->pvscache, ctx->pvs))
			return;	//this section isn't in any visible bsp leafs

		if (s->numents)
		{
			Sys_LockMutex(hm->entitylock);
			//chuck out any batches for models in this section
			for (i = 0; i < s->numents; i++)
			{
				vec3_t dist;
				float a, dmin, dmax;
				model_t *model;
				//skip the entity if its already been added to some batch this frame.
				if (s->ents[i]->drawnframe == hm->drawnframe)
					continue;
				s->ents[i]->drawnframe = hm->drawnframe;

				model = s->ents[i]->ent.model;
				if (!model)
					continue;

				if (model->loadstate == MLS_NOTLOADED)
				{
	//				if (hm->beinglazy)
	//					continue;
	//				hm->beinglazy = true;
					Mod_LoadModel(model, MLV_WARN);
				}
				if (model->loadstate != MLS_LOADED)
					continue;

				VectorSubtract(s->ents[i]->ent.origin, r_origin, dist);
				a = VectorLength(dist);
				dmin = 1024 + model->radius*160;
				dmax = dmin + 1024;
				a = (a - dmin) / (dmax - dmin);
				a = 1-a;
				if (a < 0)
					continue;
				if (a >= 1)
				{
					a = 1;
					s->ents[i]->ent.flags &= ~RF_TRANSLUCENT;
				}
				else
					s->ents[i]->ent.flags |= RF_TRANSLUCENT;
				s->ents[i]->ent.shaderRGBAf[3] = a;
				switch(model->type)
				{
				case mod_alias:
					R_GAlias_GenerateBatches(&s->ents[i]->ent, ctx->batches);
					break;
				case mod_brush:
					Surf_GenBrushBatches(ctx->batches, &s->ents[i]->ent);
					break;
				default:	//FIXME: no sprites! oh noes!
					break;
				}
			}
			Sys_UnlockMutex(hm->entitylock);
		}

		for (wa = s->water; wa; wa = wa->next)
		{
			mins[2] = wa->minheight;
			maxs[2] = wa->maxheight;
			if (!R_CullBox(mins, maxs))
			{
				Terr_DrawTerrainWater(hm, mins, maxs, wa);
			}
		}

		mins[2] = s->minh;
		maxs[2] = s->maxh;

//		if (!BoundsIntersect(mins, maxs, r_refdef.vieworg, r_refdef.vieworg))
			if (R_CullBox(mins, maxs))
				return;

		b = BE_GetTempBatch();
		if (!b)
			return;
		b->ent = ctx->ent;
		b->shader = hm->shader;
		b->flags = 0;
		b->mesh = &s->amesh;
		b->mesh[0] = &s->mesh;
		b->meshes = 1;
		b->buildmeshes = NULL;
		b->skin = &s->textures;
		b->texture = NULL;
		b->vbo = &s->vbo;
		b->lightmap[0] = s->lightmap;
		for (j = 1; j < MAXRLIGHTMAPS; j++)
			b->lightmap[j] = -1;

		b->next = ctx->batches[b->shader->sort];
		ctx->batches[b->shader->sort] = b;
	}
	else if (w && h)
	{
		//divide and conquer, radiating outwards from the view.
		if (w > h)
		{
			i = x + w;
			w = x + w/2;
			if (ctx->vx >= w)
			{
				Terr_DrawInBounds(ctx, w, y, i-w, h);
				Terr_DrawInBounds(ctx, x, y, w-x, h);
			}
			else
			{
				Terr_DrawInBounds(ctx, x, y, w-x, h);
				Terr_DrawInBounds(ctx, w, y, i-w, h);
			}
		}
		else
		{
			i = y + h;
			h = y + h/2;
			if (ctx->vy >= h)
			{
				Terr_DrawInBounds(ctx, x, h, w, i-h);
				Terr_DrawInBounds(ctx, x, y, w, h-y);
			}
			else
			{
				Terr_DrawInBounds(ctx, x, y, w, h-y);
				Terr_DrawInBounds(ctx, x, h, w, i-h);
			}
		}
	}
}

void Terr_DrawTerrainModel (batch_t **batches, entity_t *e)
{
	extern qbyte *frustumvis;
	model_t *m = e->model;
	heightmap_t *hm = m->terrain;
	batch_t *b;
	int bounds[4], j;
	struct tdibctx tdibctx;

	if (!r_refdef.recurse)
	{
		Terr_DoEditNotify(hm);
//		while (hm->activesections > 0)
//			if (!Terr_Collect(hm))
//				break;
		while (hm->activesections > TERRAINACTIVESECTIONS)
		{
			if (!Terr_Collect(hm))
				break;
			break;
		}
	}
	
//	hm->beinglazy = false;
	if (hm->relight)
		ted_dorelight(hm);

	if (e->model == cl.worldmodel && hm->skyshader)
	{
		b = BE_GetTempBatch();
		if (b)
		{
			for (j = 0; j < MAXRLIGHTMAPS; j++)
				b->lightmap[j] = -1;
			b->ent = e;
			b->shader = hm->skyshader;
			b->flags = 0;
			b->mesh = &hm->askymesh;
			b->mesh[0] = &hm->skymesh;
			b->meshes = 1;
			b->buildmeshes = NULL;
			b->skin = &b->shader->defaulttextures;
			b->texture = NULL;
	//		vbo = b->vbo = hm->vbo[x+y*MAXSECTIONS];
			b->vbo = NULL;

			b->next = batches[b->shader->sort];
			batches[b->shader->sort] = b;
		}
	}

	Terr_Brush_Draw(hm, batches, e);

	if (r_refdef.globalfog.density || gl_maxdist.value>0)
	{
		float culldist;
		extern cvar_t r_fog_exp2;

		if (r_refdef.globalfog.density)
		{
			//figure out the eyespace distance required to reach that fog value
			culldist = log(0.5/255.0f);
			if (r_fog_exp2.ival)
				culldist = sqrt(culldist / (-r_refdef.globalfog.density * r_refdef.globalfog.density));
			else
				culldist = culldist / (-r_refdef.globalfog.density);
			//anything drawn beyond this point is fully obscured by fog
			culldist += 4096;
		}
		else
			culldist = 999999999999999.f;

		if (culldist > gl_maxdist.value && gl_maxdist.value>0)
			culldist = gl_maxdist.value;

		bounds[0] = bound(hm->firstsegx, (r_refdef.vieworg[0] + (CHUNKBIAS + 0)*hm->sectionsize - culldist) / hm->sectionsize,  hm->maxsegx);
		bounds[1] = bound(hm->firstsegx, (r_refdef.vieworg[0] + (CHUNKBIAS + 1)*hm->sectionsize + culldist) / hm->sectionsize,  hm->maxsegx);
		bounds[2] = bound(hm->firstsegy, (r_refdef.vieworg[1] + (CHUNKBIAS + 0)*hm->sectionsize - culldist) / hm->sectionsize,  hm->maxsegy);
		bounds[3] = bound(hm->firstsegy, (r_refdef.vieworg[1] + (CHUNKBIAS + 1)*hm->sectionsize + culldist) / hm->sectionsize,  hm->maxsegy);
	}
	else
	{
		bounds[0] = hm->firstsegx;
		bounds[1] = hm->maxsegx;
		bounds[2] = hm->firstsegy;
		bounds[3] = hm->maxsegy;
	}

	hm->drawnframe+=1;
	tdibctx.hm = hm;
	tdibctx.batches = batches;
	tdibctx.ent = e;
	tdibctx.vx = (r_refdef.vieworg[0] + CHUNKBIAS*hm->sectionsize) / hm->sectionsize;
	tdibctx.vy = (r_refdef.vieworg[1] + CHUNKBIAS*hm->sectionsize) / hm->sectionsize;
	tdibctx.wmodel = e->model;
	tdibctx.pvs = (e->model == cl.worldmodel)?frustumvis:NULL;
	Terr_DrawInBounds(&tdibctx, bounds[0], bounds[2], bounds[1]-bounds[0], bounds[3]-bounds[2]);
}

typedef struct fragmentdecal_s fragmentdecal_t;

void Fragment_ClipPoly(fragmentdecal_t *dec, int numverts, float *inverts);
void Terrain_ClipDecal(fragmentdecal_t *dec, float *center, float radius, model_t *model)
{
	int min[2], max[2], mint[2], maxt[2];
	int x, y, tx, ty;
	vecV_t vert[6];
	hmsection_t *s;
	heightmap_t *hm = model->terrain; 
	min[0] = floor((center[0] - radius)/(hm->sectionsize)) + CHUNKBIAS;
	min[1] = floor((center[1] - radius)/(hm->sectionsize)) + CHUNKBIAS;
	max[0] = ceil((center[0] + radius)/(hm->sectionsize)) + CHUNKBIAS;
	max[1] = ceil((center[1] + radius)/(hm->sectionsize)) + CHUNKBIAS;

	min[0] = bound(hm->firstsegx, min[0], hm->maxsegx);
	min[1] = bound(hm->firstsegy, min[1], hm->maxsegy);
	max[0] = bound(hm->firstsegx, max[0], hm->maxsegx);
	max[1] = bound(hm->firstsegy, max[1], hm->maxsegy);

	for (y = min[1]; y < max[1]; y++)
	{
		for (x = min[0]; x < max[0]; x++)
		{
			s = Terr_GetSection(hm, x, y, TGS_WAITLOAD);
			if (!s)
				continue;

			mint[0] = floor((center[0] - radius)*(SECTHEIGHTSIZE-1)/(hm->sectionsize) + (CHUNKBIAS - x)*(SECTHEIGHTSIZE-1));
			mint[1] = floor((center[1] - radius)*(SECTHEIGHTSIZE-1)/(hm->sectionsize) + (CHUNKBIAS - y)*(SECTHEIGHTSIZE-1));
			maxt[0] =  ceil((center[0] + radius)*(SECTHEIGHTSIZE-1)/(hm->sectionsize) + (CHUNKBIAS - x)*(SECTHEIGHTSIZE-1));
			maxt[1] =  ceil((center[1] + radius)*(SECTHEIGHTSIZE-1)/(hm->sectionsize) + (CHUNKBIAS - y)*(SECTHEIGHTSIZE-1));

			mint[0] = bound(0, mint[0], (SECTHEIGHTSIZE-1));
			mint[1] = bound(0, mint[1], (SECTHEIGHTSIZE-1));
			maxt[0] = bound(0, maxt[0], (SECTHEIGHTSIZE-1));
			maxt[1] = bound(0, maxt[1], (SECTHEIGHTSIZE-1));

			for (ty = mint[1]; ty < maxt[1]; ty++)
			{
				for (tx = mint[0]; tx < maxt[0]; tx++)
				{
#ifndef STRICTEDGES
					float d1, d2;
					d1 = fabs(s->heights[(tx+0) + (ty+0)*SECTHEIGHTSIZE] - s->heights[(tx+1) + (ty+1)*SECTHEIGHTSIZE]);
					d2 = fabs(s->heights[(tx+1) + (ty+0)*SECTHEIGHTSIZE] - s->heights[(tx+0) + (ty+1)*SECTHEIGHTSIZE]);
					if (d1 < d2)
					{
						vert[0][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[0][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;
						vert[1][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[1][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;
						vert[2][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[2][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;

						vert[3][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[3][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;
						vert[4][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[4][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;
						vert[5][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[5][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;

						vert[0][2] = s->heights[(tx+0) + (ty+0)*SECTHEIGHTSIZE];
						vert[1][2] = s->heights[(tx+1) + (ty+1)*SECTHEIGHTSIZE];
						vert[2][2] = s->heights[(tx+1) + (ty+0)*SECTHEIGHTSIZE];
						vert[3][2] = s->heights[(tx+0) + (ty+0)*SECTHEIGHTSIZE];
						vert[4][2] = s->heights[(tx+0) + (ty+1)*SECTHEIGHTSIZE];
						vert[5][2] = s->heights[(tx+1) + (ty+1)*SECTHEIGHTSIZE];
					}
					else
#endif
					{
						vert[0][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[0][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;
						vert[1][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[1][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;
						vert[2][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[2][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;

						vert[3][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[3][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;
						vert[4][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+0)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[4][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;
						vert[5][0] = (x-CHUNKBIAS)*hm->sectionsize + (tx+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;vert[5][1] = (y-CHUNKBIAS)*hm->sectionsize + (ty+1)/(float)(SECTHEIGHTSIZE-1)*hm->sectionsize;

						vert[0][2] = s->heights[(tx+0) + (ty+0)*SECTHEIGHTSIZE];
						vert[1][2] = s->heights[(tx+0) + (ty+1)*SECTHEIGHTSIZE];
						vert[2][2] = s->heights[(tx+1) + (ty+0)*SECTHEIGHTSIZE];
						vert[3][2] = s->heights[(tx+1) + (ty+0)*SECTHEIGHTSIZE];
						vert[4][2] = s->heights[(tx+0) + (ty+1)*SECTHEIGHTSIZE];
						vert[5][2] = s->heights[(tx+1) + (ty+1)*SECTHEIGHTSIZE];
					}

					Fragment_ClipPoly(dec, 3, &vert[0][0]);
					Fragment_ClipPoly(dec, 3, &vert[3][0]);
				}
			}
		}
	}
}

#endif

unsigned int Heightmap_PointContentsHM(heightmap_t *hm, float clipmipsz, vec3_t org)
{
	float x, y;
	float z, tz;
	int sx, sy;
	unsigned int holerow;
	unsigned int holebit;
	hmsection_t *s;
	struct hmwater_s *w;
	unsigned int contents;
	const float wbias = CHUNKBIAS * hm->sectionsize;

	sx = (org[0]+wbias)/hm->sectionsize;
	sy = (org[1]+wbias)/hm->sectionsize;
	if (sx < hm->firstsegx || sy < hm->firstsegy)
		return hm->exteriorcontents;
	if (sx >= hm->maxsegx || sy >= hm->maxsegy)
		return hm->exteriorcontents;
	s = Terr_GetSection(hm, sx, sy, TGS_TRYLOAD);
	if (!s)
	{
		return FTECONTENTS_SOLID;
	}

	x = (org[0]+wbias - (sx*hm->sectionsize))*(SECTHEIGHTSIZE-1)/hm->sectionsize;
	y = (org[1]+wbias - (sy*hm->sectionsize))*(SECTHEIGHTSIZE-1)/hm->sectionsize;
	z = (org[2]+clipmipsz);

	if (z < s->minh-16)
		return hm->exteriorcontents;

	sx = x; x-=sx;
	sy = y; y-=sy;

	holerow = ((sy<<3)/(SECTHEIGHTSIZE-1));
	holebit = 1u<<((sx<<3)/(SECTHEIGHTSIZE-1));
	if (s->holes[holerow] & (1u<<holebit))
		return FTECONTENTS_EMPTY;

	//made of two triangles:
	if (x+y>1)	//the 1, 1 triangle
	{
		float v1, v2, v3;
		v3 = 1-y;
		v2 = x+y-1;
		v1 = 1-x;
		//0, 1
		//1, 1
		//1, 0
		tz = (s->heights[(sx+0)+(sy+1)*SECTHEIGHTSIZE]*v1 +
			  s->heights[(sx+1)+(sy+1)*SECTHEIGHTSIZE]*v2 +
			  s->heights[(sx+1)+(sy+0)*SECTHEIGHTSIZE]*v3);
	}
	else
	{
		float v1, v2, v3;
		v1 = y;
		v2 = x;
		v3 = 1-y-x;

		//0, 1
		//1, 0
		//0, 0
		tz = (s->heights[(sx+0)+(sy+1)*SECTHEIGHTSIZE]*v1 +
			  s->heights[(sx+1)+(sy+0)*SECTHEIGHTSIZE]*v2 +
			  s->heights[(sx+0)+(sy+0)*SECTHEIGHTSIZE]*v3);
	}
	if (z <= tz)
		return FTECONTENTS_SOLID;	//contained within

	contents = FTECONTENTS_EMPTY;
	for (w = s->water; w; w = w->next)
	{
		if (w->holes[holerow] & (1u<<holebit))
			continue;
		if (z < w->maxheight)	//FIXME
			contents |= w->contentmask;
	}
	return contents;
}

unsigned int Heightmap_PointContents(model_t *model, vec3_t axis[3], vec3_t org)
{
	heightmap_t *hm = model->terrain;
	return Heightmap_PointContentsHM(hm, 0, org);
}
unsigned int Heightmap_NativeBoxContents(model_t *model, int hulloverride, int frame, vec3_t axis[3], vec3_t org, vec3_t mins, vec3_t maxs)
{
	heightmap_t *hm = model->terrain;
	return Heightmap_PointContentsHM(hm, mins[2], org);
}

void Heightmap_Normal(heightmap_t *hm, vec3_t org, vec3_t norm)
{
#if 0
	norm[0] = 0;
	norm[1] = 0;
	norm[2] = 1;
#else
	float x, y;
	int sx, sy;
	vec3_t d1, d2;
	const float wbias = CHUNKBIAS * hm->sectionsize;
	hmsection_t *s;

	norm[0] = 0;
	norm[1] = 0;
	norm[2] = 1;

	sx = (org[0]+wbias)/hm->sectionsize;
	sy = (org[1]+wbias)/hm->sectionsize;
	if (sx < hm->firstsegx || sy < hm->firstsegy)
		return;
	if (sx >= hm->maxsegx || sy >= hm->maxsegy)
		return;
	s = Terr_GetSection(hm, sx, sy, TGS_TRYLOAD);
	if (!s)
		return;

	x = (org[0]+wbias - (sx*hm->sectionsize))*(SECTHEIGHTSIZE-1)/hm->sectionsize;
	y = (org[1]+wbias - (sy*hm->sectionsize))*(SECTHEIGHTSIZE-1)/hm->sectionsize;

	sx = x; x-=sx;
	sy = y; y-=sy;

	if (x+y>1)	//the 1, 1 triangle
	{
		//0, 1
		//1, 1
		//1, 0
		d1[0] = (hm->sectionsize / SECTHEIGHTSIZE);
		d1[1] = 0;
		d1[2] = (s->heights[(sx+1)+(sy+1)*SECTHEIGHTSIZE] - s->heights[(sx+0)+(sy+1)*SECTHEIGHTSIZE]);
		d2[0] = 0;
		d2[1] = (hm->sectionsize / SECTHEIGHTSIZE);
		d2[2] = (s->heights[(sx+1)+(sy+1)*SECTHEIGHTSIZE] - s->heights[(sx+1)+(sy+0)*SECTHEIGHTSIZE]);
	}
	else
	{	//the 0,0 triangle
		//0, 1
		//1, 0
		//0, 0
		d1[0] = (hm->sectionsize / SECTHEIGHTSIZE);
		d1[1] = 0;
		d1[2] = (s->heights[(sx+1)+(sy+0)*SECTHEIGHTSIZE] - s->heights[(sx+0)+(sy+0)*SECTHEIGHTSIZE]);
		d2[0] = 0;
		d2[1] = (hm->sectionsize / SECTHEIGHTSIZE);
		d2[2] = (s->heights[(sx+0)+(sy+1)*SECTHEIGHTSIZE] - s->heights[(sx+0)+(sy+0)*SECTHEIGHTSIZE]);
	}

	VectorNormalize(d1);
	VectorNormalize(d2);
	CrossProduct(d1, d2, norm);
	VectorNormalize(norm);
#endif
}

typedef struct {
	vec3_t start;
	vec3_t end;
	vec3_t impact;
	vec4_t plane;
	vec3_t mins;
	vec3_t maxs;
	qboolean capsule;
	float frac;
	float htilesize;
	heightmap_t *hm;
	int contents;
	int hitcontentsmask;
	trace_t *result;
} hmtrace_t;

static void Heightmap_Trace_Brush(hmtrace_t *tr, vec4_t *planes, int numplanes)
{
	qboolean startout;
	float *enterplane;
	double enterfrac, exitfrac, nearfrac=0;
	double enterdist=0;
	double dist, d1, d2, f;
	int i;

	startout = false;
	enterplane= NULL;
	enterfrac = -1;
	exitfrac = 10;
	for (i = 0; i < numplanes; i++)
	{
		/*calculate the distance based upon the shape of the object we're tracing for*/
		dist = planes[i][3];


		d1 = DotProduct (tr->start, planes[i]) - dist;
		d2 = DotProduct (tr->end, planes[i]) - dist;

		//if we're fully outside any plane, then we cannot possibly enter the brush, skip to the next one
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 > 0)
			startout = true;

		//if we're fully inside the plane, then whatever is happening is not relevent for this plane
		if (d1 <= 0 && d2 <= 0)
			continue;

		f = (d1) / (d1-d2);
		if (d1 > d2)
		{
			//entered the brush. favour the furthest fraction to avoid extended edges (yay for convex shapes)
			if (enterfrac < f)
			{
				enterfrac = f;
				nearfrac = (d1 - (0.03125)) / (d1-d2);
				enterplane = planes[i];
				enterdist = dist;
			}
		}
		else
		{
			//left the brush, favour the nearest plane (smallest frac)
			if (exitfrac > f)
			{
				exitfrac = f;
			}
		}
	}

	if (!startout)
	{
		tr->frac = -1;
		return;
	}
	if (enterfrac != -1 && enterfrac < exitfrac)
	{
		//impact!
		if (enterfrac < tr->frac)
		{
			if (nearfrac < 0)
				nearfrac = 0;
			tr->frac = nearfrac;//enterfrac;
			tr->plane[3] = enterdist;
			VectorCopy(enterplane, tr->plane);
		}
	}
}

//sx,sy are the tile coord
//note that tile SECTHEIGHTSIZE-1 does not exist, as the last sample overlaps the first sample of the next section
static void Heightmap_Trace_Square(hmtrace_t *tr, int tx, int ty)
{
	vec3_t d[2];
	vec3_t p[4];
	vec4_t n[5];
	int t;
	int i;

#ifndef STRICTEDGES
	float d1, d2;
#endif
	int sx, sy;
	hmsection_t *s;
	unsigned int holerow;
	unsigned int holebit;

	sx = tx/(SECTHEIGHTSIZE-1);
	sy = ty/(SECTHEIGHTSIZE-1);
	if (sx < tr->hm->firstsegx || sx >= tr->hm->maxsegx)
		return;//s = NULL;
	else if (sy < tr->hm->firstsegy || sy >= tr->hm->maxsegy)
		return;//s = NULL;
	else
		s = Terr_GetSection(tr->hm, sx, sy, TGS_TRYLOAD|TGS_WAITLOAD);

	if (!s)
	{
		//you're not allowed to walk into sections that have not loaded.
		//might as well check the entire section instead of just one tile
		Vector4Set(n[0],  1, 0, 0, (tx/(SECTHEIGHTSIZE-1) + 1 - CHUNKBIAS)*tr->hm->sectionsize);
		Vector4Set(n[1], -1, 0, 0, -(tx/(SECTHEIGHTSIZE-1) + 0 - CHUNKBIAS)*tr->hm->sectionsize);
		Vector4Set(n[2], 0,  1, 0, (ty/(SECTHEIGHTSIZE-1) + 1 - CHUNKBIAS)*tr->hm->sectionsize);
		Vector4Set(n[3], 0, -1, 0, -(ty/(SECTHEIGHTSIZE-1) + 0 - CHUNKBIAS)*tr->hm->sectionsize);
		Heightmap_Trace_Brush(tr, n, 4);
		return;
	}

	if (s->traceseq != tr->hm->traceseq && s->numents)
	{
		s->traceseq = tr->hm->traceseq;
		Sys_LockMutex(tr->hm->entitylock);
		for (i = 0; i < s->numents; i++)
		{
			vec3_t start_l, end_l;
			trace_t etr;
			model_t *model;
			int frame;
			if (s->ents[i]->traceseq == tr->hm->traceseq)
				continue;
			s->ents[i]->traceseq = tr->hm->traceseq;
			model = s->ents[i]->ent.model;
			frame = s->ents[i]->ent.framestate.g[FS_REG].frame[0];
			//FIXME: IGNORE the entity if it isn't loaded yet? surely that's bad?
			if (!model || model->loadstate != MLS_LOADED || !model->funcs.NativeTrace)
				continue;
			//figure out where on the submodel the trace is.
			VectorSubtract (tr->start, s->ents[i]->ent.origin, start_l);
			VectorSubtract (tr->end, s->ents[i]->ent.origin, end_l);
			start_l[2] -= tr->mins[2];
			end_l[2] -= tr->mins[2];
			VectorScale(start_l, s->ents[i]->ent.scale, start_l);
			VectorScale(end_l, s->ents[i]->ent.scale, end_l);

			//skip if the local trace points are outside the model's bounds
/*			for (j = 0; j < 3; j++)
			{
				if (start_l[j]+tr->mins[j] > model->maxs[j] && end_l[j]+tr->mins[j] > model->maxs[j])
					continue;
				if (start_l[j]+tr->maxs[j] < model->mins[j] && end_l[j]+tr->maxs[j] < model->mins[j])
					continue;
			}
*/
			//do the trace
			memset(&etr, 0, sizeof(etr));
			etr.fraction = 1;
			model->funcs.NativeTrace (model, 0, frame, s->ents[i]->ent.axis, start_l, end_l, tr->mins, tr->maxs, tr->capsule, tr->hitcontentsmask, &etr);

			tr->result->startsolid |= etr.startsolid;
			tr->result->allsolid |= etr.allsolid;
			if (etr.fraction < tr->frac)
			{
				tr->contents = etr.contents;
				tr->frac = etr.fraction;
				tr->plane[3] = etr.plane.dist;
				tr->plane[0] = etr.plane.normal[0];
				tr->plane[1] = etr.plane.normal[1];
				tr->plane[2] = etr.plane.normal[2];
			}
		}
		Sys_UnlockMutex(tr->hm->entitylock);
	}

	sx = tx - CHUNKBIAS*(SECTHEIGHTSIZE-1);
	sy = ty - CHUNKBIAS*(SECTHEIGHTSIZE-1);

	tx = tx % (SECTHEIGHTSIZE-1);
	ty = ty % (SECTHEIGHTSIZE-1);

	holerow = ((ty<<3)/(SECTHEIGHTSIZE-1));
	holebit = 1u<<((tx<<3)/(SECTHEIGHTSIZE-1));
	if (s->holes[holerow] & holebit)
		return;	//no collision with holes

	switch(tr->hm->mode)
	{
	case HMM_BLOCKS:
		//left-most
		Vector4Set(n[0], -1, 0, 0, -tr->htilesize*(sx+0));
		//bottom-most
		Vector4Set(n[1], 0, 1, 0, tr->htilesize*(sy+1));
		//right-most
		Vector4Set(n[2], 1, 0, 0, tr->htilesize*(sx+1));
		//top-most
		Vector4Set(n[3], 0, -1, 0, -tr->htilesize*(sy+0));
		//top
		Vector4Set(n[4], 0, 0, 1, s->heights[(tx+0)+(ty+0)*SECTHEIGHTSIZE]);

		Heightmap_Trace_Brush(tr, n, 5);
		return;
	case HMM_TERRAIN:
		VectorSet(p[0], tr->htilesize*(sx+0), tr->htilesize*(sy+0), s->heights[(tx+0)+(ty+0)*SECTHEIGHTSIZE]);
		VectorSet(p[1], tr->htilesize*(sx+1), tr->htilesize*(sy+0), s->heights[(tx+1)+(ty+0)*SECTHEIGHTSIZE]);
		VectorSet(p[2], tr->htilesize*(sx+0), tr->htilesize*(sy+1), s->heights[(tx+0)+(ty+1)*SECTHEIGHTSIZE]);
		VectorSet(p[3], tr->htilesize*(sx+1), tr->htilesize*(sy+1), s->heights[(tx+1)+(ty+1)*SECTHEIGHTSIZE]);

#ifndef STRICTEDGES
		d1 = fabs(p[0][2] - p[3][2]);
		d2 = fabs(p[1][2] - p[2][2]);
		if (d1 < d2)
		{
			for (t = 0; t < 2; t++)
			{
				/*generate the brush (in world space*/
				if (t == 0)
				{
					VectorSubtract(p[3], p[2], d[0]);
					VectorSubtract(p[2], p[0], d[1]);
					//left-most
					Vector4Set(n[0], -1, 0, 0, -tr->htilesize*(sx+0));
					//bottom-most
					Vector4Set(n[1], 0, 1, 0, tr->htilesize*(sy+1));
					//top-right
					VectorSet(n[2], 0.70710678118654752440084436210485, -0.70710678118654752440084436210485, 0);
					n[2][3] = DotProduct(n[2], p[0]);
					//top
					VectorNormalize(d[0]);
					VectorNormalize(d[1]);
					CrossProduct(d[0], d[1], n[3]);
					VectorNormalize(n[3]);
					n[3][3] = DotProduct(n[3], p[0]);
					//down
					VectorNegate(n[3], n[4]);
					n[4][3] = DotProduct(n[4], p[0]) - n[4][2]*TERRAINTHICKNESS;
				}
				else
				{
					VectorSubtract(p[1], p[0], d[0]);
					VectorSubtract(p[3], p[1], d[1]);

					//right-most
					Vector4Set(n[0], 1, 0, 0, tr->htilesize*(sx+1));
					//top-most
					Vector4Set(n[1], 0, -1, 0, -tr->htilesize*(sy+0));
					//bottom-left
					VectorSet(n[2], -0.70710678118654752440084436210485, 0.70710678118654752440084436210485, 0);
					n[2][3] = DotProduct(n[2], p[0]);
					//top
					VectorNormalize(d[0]);
					VectorNormalize(d[1]);
					CrossProduct(d[0], d[1], n[3]);
					VectorNormalize(n[3]);
					n[3][3] = DotProduct(n[3], p[0]);
					//down
					VectorNegate(n[3], n[4]);
					n[4][3] = DotProduct(n[4], p[0]) - n[4][2]*TERRAINTHICKNESS;
				}
				Heightmap_Trace_Brush(tr, n, 5);
			}
		}
		else
#endif
		{
			for (t = 0; t < 2; t++)
			{
				/*generate the brush (in world space*/
				if (t == 0)
				{
					VectorSubtract(p[1], p[0], d[0]);
					VectorSubtract(p[2], p[0], d[1]);
					//left-most
					Vector4Set(n[0], -1, 0, 0, -tr->htilesize*(sx+0));
					//top-most
					Vector4Set(n[1], 0, -1, 0, -tr->htilesize*(sy+0));
					//bottom-right
					VectorSet(n[2], 0.70710678118654752440084436210485, 0.70710678118654752440084436210485, 0);
					n[2][3] = DotProduct(n[2], p[1]);
					//top
					VectorNormalize(d[0]);
					VectorNormalize(d[1]);
					CrossProduct(d[0], d[1], n[3]);
					VectorNormalize(n[3]);
					n[3][3] = DotProduct(n[3], p[1]);
					//down
					VectorNegate(n[3], n[4]);
					n[4][3] = DotProduct(n[4], p[1]) - n[4][2]*TERRAINTHICKNESS;
				}
				else
				{
					VectorSubtract(p[3], p[2], d[0]);
					VectorSubtract(p[3], p[1], d[1]);

					//right-most
					Vector4Set(n[0], 1, 0, 0, tr->htilesize*(sx+1));
					//bottom-most
					Vector4Set(n[1], 0, 1, 0, tr->htilesize*(sy+1));
					//top-left
					VectorSet(n[2], -0.70710678118654752440084436210485, -0.70710678118654752440084436210485, 0);
					n[2][3] = DotProduct(n[2], p[1]);
					//top
					VectorNormalize(d[0]);
					VectorNormalize(d[1]);
					CrossProduct(d[0], d[1], n[3]);
					VectorNormalize(n[3]);
					n[3][3] = DotProduct(n[3], p[1]);
					//down
					VectorNegate(n[3], n[4]);
					n[4][3] = DotProduct(n[4], p[1]) - n[4][2]*TERRAINTHICKNESS;
				}
				Heightmap_Trace_Brush(tr, n, 5);
			}
		}
		break;
	}
}

#define DIST_EPSILON 0
/*
Heightmap_TraceRecurse
Traces an arbitary box through a heightmap. (interface with outside)

Why is recursion good?
1: it is consistant with bsp models. :)
2: it allows us to use any size model we want
3: we don't have to work out the height of the terrain every X units, but can be more precise.

Obviously, we don't care all that much about 1
*/
qboolean Heightmap_Trace(struct model_s *model, int hulloverride, int frame, vec3_t mataxis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int against, struct trace_s *trace)
{
	vec2_t pos, npos;
	qboolean nudge[2];
	vec2_t dir;
	vec2_t frac;
	vec2_t emins;
	vec2_t emaxs;
	int x, y;
	int axis;
	int breaklimit = 1000;
	float wbias;
	hmtrace_t hmtrace;
	hmtrace.hm = model->terrain;
	hmtrace.hm->traceseq++;
	hmtrace.htilesize = hmtrace.hm->sectionsize / (SECTHEIGHTSIZE-1);
	hmtrace.frac = 1;
	hmtrace.contents = 0;

	hmtrace.plane[0] = 0;
	hmtrace.plane[1] = 0;
	hmtrace.plane[2] = 0;
	hmtrace.plane[3] = 0;
	hmtrace.capsule = capsule;

	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	hmtrace.result = trace;

	//to tile space
	hmtrace.start[0] = (start[0]);
	hmtrace.start[1] = (start[1]);
	hmtrace.start[2] = (start[2] + mins[2]);
	hmtrace.end[0] = (end[0]);
	hmtrace.end[1] = (end[1]);
	hmtrace.end[2] = (end[2] + mins[2]);

	dir[0] = (hmtrace.end[0] - hmtrace.start[0])/hmtrace.htilesize;
	dir[1] = (hmtrace.end[1] - hmtrace.start[1])/hmtrace.htilesize;
	pos[0] = (hmtrace.start[0]+CHUNKBIAS*hmtrace.hm->sectionsize)/hmtrace.htilesize;
	pos[1] = (hmtrace.start[1]+CHUNKBIAS*hmtrace.hm->sectionsize)/hmtrace.htilesize;
	wbias = CHUNKBIAS*hmtrace.hm->sectionsize;

	emins[0] = (mins[0]-1.5)/hmtrace.htilesize;
	emins[1] = (mins[1]-1.5)/hmtrace.htilesize;
	emaxs[0] = (maxs[0]+1.5)/hmtrace.htilesize;
	emaxs[1] = (maxs[1]+1.5)/hmtrace.htilesize;

	VectorCopy(mins, hmtrace.mins);
	VectorCopy(maxs, hmtrace.maxs);

	/*fixme:
	set pos to the leading corner instead
	on boundary changes, scan across multiple blocks
	*/

	//make sure the start tile is valid
	for (y = pos[1] + emins[1]; y <= pos[1] + emaxs[1]; y++)
		for (x = pos[0] + emins[0]; x <= pos[0] + emaxs[0]; x++)
			Heightmap_Trace_Square(&hmtrace, x, y);
	for(;;)
	{
		if (breaklimit--< 0)
			break;
		for (axis = 0; axis < 2; axis++)
		{
			if (dir[axis] > 0)
			{
				nudge[axis] = false;
				npos[axis] = pos[axis] + 1-(pos[axis]-(int)pos[axis]);
				frac[axis] = (npos[axis]*hmtrace.htilesize-wbias - hmtrace.start[axis])/(hmtrace.end[axis]-hmtrace.start[axis]);
			}
			else if (dir[axis] < 0)
			{
				npos[axis] = pos[axis];
				nudge[axis] = (float)(int)pos[axis] == pos[axis];
				npos[axis] = (int)npos[axis];
				frac[axis] = (npos[axis]*hmtrace.htilesize-wbias - hmtrace.start[axis])/(hmtrace.end[axis]-hmtrace.start[axis]);
				npos[axis] -= nudge[axis];
			}
			else
				frac[axis] = 1000000000000000.0;
		}

		//which side are we going down?
		if (frac[0] < frac[1])
			axis = 0;
		else
			axis = 1;

		if (frac[axis] >= 1)
			break;

		//touch the neighbour(s)
		if (dir[axis] > 0)
		{
			pos[axis] = (int)pos[axis] + 1;
			pos[axis] = npos[axis];
			Heightmap_Trace_Square(&hmtrace, pos[0], pos[1]);
		}
		else
		{
			pos[axis] = npos[axis];
			Heightmap_Trace_Square(&hmtrace, pos[0], pos[1]);
		}
		//and make sure our position on the other axis is correct, for the next time around the loop
		if (frac[axis] > hmtrace.frac)
			break;
		pos[!axis] = ((hmtrace.end[!axis] * frac[axis]) + (hmtrace.start[!axis] * (1-frac[axis])) + CHUNKBIAS*hmtrace.hm->sectionsize)/hmtrace.htilesize;
	}

	{
		brushes_t *brushes = hmtrace.hm->wbrushes;
		int count = hmtrace.hm->numbrushes;
		while(count-->0)
		{
			if (brushes->contents & against)
				Heightmap_Trace_Brush(&hmtrace, brushes->planes, brushes->numplanes);
			brushes++;
		}
	}

	trace->plane.dist = hmtrace.plane[3];
	trace->plane.normal[0] = hmtrace.plane[0];
	trace->plane.normal[1] = hmtrace.plane[1];
	trace->plane.normal[2] = hmtrace.plane[2];

	if (hmtrace.frac == -1)
	{
		trace->fraction = 0;
		trace->startsolid = true;
		trace->allsolid = true;
		VectorCopy(start, trace->endpos);
	}
	else
	{
		if (hmtrace.frac < 0)
			hmtrace.frac = 0;
		trace->fraction = hmtrace.frac;
		VectorInterpolate(start, hmtrace.frac, end, trace->endpos);
	}
	return trace->fraction < 1;
}

typedef struct
{
	int id;
	int x, y;
} hmpvs_t;
unsigned int Heightmap_FatPVS		(model_t *mod, vec3_t org, qbyte *pvsbuffer, unsigned int pvssize, qboolean add)
{
	//embed the org onto the pvs
	hmpvs_t *hmpvs = (hmpvs_t*)pvsbuffer;
	hmpvs->id = 0xdeadbeef;
	hmpvs->x = org[0];
	hmpvs->y = org[1];
	return sizeof(*hmpvs);
}

#ifndef CLIENTONLY
qboolean Heightmap_EdictInFatPVS	(model_t *mod, struct pvscache_s *edict, qbyte *pvsdata)
{
	int x,y;
	hmpvs_t *hmpvs = (hmpvs_t*)pvsdata;
	//check distance
	x = edict->areanum - hmpvs->x;
	y = edict->areanum2 - hmpvs->y;

	return (x*x+y*y) < 4096*4096;
}

void Heightmap_FindTouchedLeafs	(model_t *mod, pvscache_t *ent, float *mins, float *maxs)
{
	ent->areanum = (mins[0] + maxs[0]) * 0.5;
	ent->areanum2 = (mins[1] + maxs[1]) * 0.5;
}
#endif

void Heightmap_LightPointValues	(model_t *mod, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	float time = realtime;

	res_diffuse[0] = 128;
	res_diffuse[1] = 128;
	res_diffuse[2] = 128;
	res_ambient[0] = 64;
	res_ambient[1] = 64;
	res_ambient[2] = 64;
	res_dir[0] = sin(time);
	res_dir[1] = cos(time);
	res_dir[2] = sin(time);
	VectorNormalize(res_dir);
}
void Heightmap_StainNode			(mnode_t *node, float *parms)
{
}
void Heightmap_MarkLights			(dlight_t *light, int bit, mnode_t *node)
{
}

qbyte *Heightmap_ClusterPVS	(model_t *model, int num, qbyte *buffer, unsigned int buffersize)
{
	return NULL;
//	static qbyte heightmappvs = 255;
//	return &heightmappvs;
}
int	Heightmap_ClusterForPoint	(model_t *model, vec3_t point)
{
	return -1;
}

#ifndef SERVERONLY
static unsigned char *ted_getlightmap(hmsection_t *s, int idx)
{
	unsigned char *lm;
	int x = idx % SECTTEXSIZE, y = idx / SECTTEXSIZE;
	if (s->lightmap < 0)
	{
		Terr_LoadSection(s->hmmod, s, x, y, true);
		Terr_InitLightmap(s, true);
	}

	s->flags |= TSF_EDITED;

	lightmap[s->lightmap]->modified = true;
	lightmap[s->lightmap]->rectchange.l = 0;
	lightmap[s->lightmap]->rectchange.t = 0;
	lightmap[s->lightmap]->rectchange.w = HMLMSTRIDE;
	lightmap[s->lightmap]->rectchange.h = HMLMSTRIDE;
	lm = lightmap[s->lightmap]->lightmaps;
	lm += ((s->lmy+y) * HMLMSTRIDE + (s->lmx+x)) * lightmap_bytes;
	return lm;
}
static void ted_dorelight(heightmap_t *hm)
{
	unsigned char *lm = ted_getlightmap(hm->relight, 0);
	int x, y;
#define EXPAND 2
	vec3_t surfnorms[(SECTTEXSIZE+EXPAND*2)*(SECTTEXSIZE+EXPAND*2)];
//	float scaletab[EXPAND*2*EXPAND*2];
	vec3_t ldir = {0.4, 0.7, 2};
	hmsection_t *s = hm->relight;
	s->flags &= ~TSF_RELIGHT;
	hm->relight = NULL;

	if (s->lightmap < 0)
		return;

	for (y = -EXPAND; y < SECTTEXSIZE+EXPAND; y++)
	for (x = -EXPAND; x < SECTTEXSIZE+EXPAND; x++)
	{
		vec3_t pos;
		pos[0] = hm->relightmin[0] + (x*hm->sectionsize/(SECTTEXSIZE-1));
		pos[1] = hm->relightmin[1] + (y*hm->sectionsize/(SECTTEXSIZE-1));
		pos[2] = 0;
		Heightmap_Normal(s->hmmod, pos, surfnorms[x+EXPAND + (y+EXPAND)*(SECTTEXSIZE+EXPAND*2)]);
	}

	VectorNormalize(ldir);

	for (y = 0; y < SECTTEXSIZE; y++, lm += (HMLMSTRIDE-SECTTEXSIZE)*4)
	for (x = 0; x < SECTTEXSIZE; x++, lm += 4)
	{
		vec3_t norm;
		float d;
		int sx,sy;
		VectorClear(norm);
		for (sy = -EXPAND; sy <= EXPAND; sy++)
		for (sx = -EXPAND; sx <= EXPAND; sx++)
		{
			d = sqrt((EXPAND*2+1)*(EXPAND*2+1) - sx*sx+sy*sy);
			VectorMA(norm, d, surfnorms[x+sx+EXPAND + (y+sy+EXPAND)*(SECTTEXSIZE+EXPAND*2)], norm);
		}

		VectorNormalize(norm);
		d = DotProduct(ldir, norm);
		if (d < 0)
			d = 0;
//		lm[0] = norm[0]*127 + 128;
//		lm[1] = norm[1]*127 + 128;
//		lm[2] = norm[2]*127 + 128;
		lm[3] = 127 + d*128;
	}

	lightmap[s->lightmap]->modified = true;
	lightmap[s->lightmap]->rectchange.l = 0;
	lightmap[s->lightmap]->rectchange.t = 0;
	lightmap[s->lightmap]->rectchange.w = HMLMSTRIDE;
	lightmap[s->lightmap]->rectchange.h = HMLMSTRIDE;
}
static void ted_sethole(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned int row = idx/9;
	unsigned int col = idx%9;
	unsigned int bit;
	unsigned int mask;
	if (row == 8 || col == 8)
		return;	//meh, our painting function is written with an overlap of 1
	if (w <= 0)
		return;
	mask = 1u<<(col);
	if (*(float*)ctx > 0)
		bit = mask;
	else
		bit = 0;
	s->flags |= TSF_NOTIFY|TSF_DIRTY|TSF_EDITED;
	s->holes[row] = (s->holes[row] & ~mask) | bit;
}
static void ted_heighttally(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	/*raise the terrain*/
	((float*)ctx)[0] += s->heights[idx]*w;
	((float*)ctx)[1] += w;
}
static void ted_heightsmooth(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	s->flags |= TSF_NOTIFY|TSF_DIRTY|TSF_EDITED|TSF_RELIGHT;
	/*interpolate the terrain towards a certain value*/

	if (IS_NAN(s->heights[idx]))
		s->heights[idx] = *(float*)ctx;
	else
		s->heights[idx] = s->heights[idx]*(1-w) + w**(float*)ctx;
}
static void ted_heightraise(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength)
{
	s->flags |= TSF_NOTIFY|TSF_DIRTY|TSF_EDITED|TSF_RELIGHT;
	/*raise the terrain*/
	s->heights[idx] += strength;
}
static void ted_heightset(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength)
{
	s->flags |= TSF_NOTIFY|TSF_DIRTY|TSF_EDITED|TSF_RELIGHT;
	/*set the terrain to a specific value*/
	s->heights[idx] = *(float*)ctx;
}

static void ted_waterset(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength)
{
	struct hmwater_s *w = s->water;
	if (!w)
		w = Terr_GenerateWater(s, *(float*)ctx);
	s->flags |= TSF_NOTIFY|TSF_DIRTY|TSF_EDITED;

	//FIXME: water doesn't render properly. don't let people make dodgy water regions because they can't see it.
	//this is temp code.
	//for (idx = 0; idx < 9*9; idx++)
		//w->heights[idx] = *(float*)ctx;
	//end fixme

	w->heights[idx] = *(float*)ctx;
	if (w->minheight > w->heights[idx])
		w->minheight = w->heights[idx];
	if (w->maxheight < w->heights[idx])
		w->maxheight = w->heights[idx];

	//FIXME: what about holes?
}

static void ted_texconcentrate(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	s->flags |= TSF_NOTIFY|TSF_EDITED;

	/*concentrate the lightmap values to a single channel*/
	if (lm[0] > lm[1] && lm[0] > lm[2] && lm[0] > (255-(lm[0]+lm[1]+lm[2])))
	{
		lm[0] = lm[0]*(1-w) + 255*(w);
		lm[1] = lm[1]*(1-w) + 0*(w);
		lm[2] = lm[2]*(1-w) + 0*(w);
	}
	else if (lm[1] > lm[2] && lm[1] > (255-(lm[0]+lm[1]+lm[2])))
	{
		lm[0] = lm[0]*(1-w) + 0*(w);
		lm[1] = lm[1]*(1-w) + 255*(w);
		lm[2] = lm[2]*(1-w) + 0*(w);
	}
	else if (lm[2] > (255-(lm[0]+lm[1]+lm[2])))
	{
		lm[0] = lm[0]*(1-w) + 0*(w);
		lm[1] = lm[1]*(1-w) + 0*(w);
		lm[2] = lm[2]*(1-w) + 255*(w);
	}
	else
	{
		lm[0] = lm[0]*(1-w) + 0*(w);
		lm[1] = lm[1]*(1-w) + 0*(w);
		lm[2] = lm[2]*(1-w) + 0*(w);
	}
}

static void ted_texnoise(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	vec4_t v;
	float sc;

	s->flags |= TSF_NOTIFY|TSF_EDITED;

	/*randomize the lightmap somewhat (you'll probably want to concentrate it a bit after)*/
	v[0] = (rand()&255);
	v[1] = (rand()&255);
	v[2] = (rand()&255);
	v[3] = (rand()&255);
	sc = v[0] + v[1] + v[2] + v[3];
	Vector4Scale(v, 255/sc, v);

	lm[0] = lm[0]*(1-w) + (v[0]*(w));
	lm[1] = lm[1]*(1-w) + (v[1]*(w));
	lm[2] = lm[2]*(1-w) + (v[2]*(w));
}

static void ted_texpaint(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	const char *texname = ctx;
	int t;
	vec3_t newval;
	if (w > 1)
		w = 1;

	s->flags |= TSF_NOTIFY|TSF_EDITED;

	for (t = 0; t < 4; t++)
	{
		if (!strncmp(s->texname[t], texname, sizeof(s->texname[t])-1))
		{
			newval[0] = (t == 0);
			newval[1] = (t == 1);
			newval[2] = (t == 2);
			lm[2] = lm[2]*(1-w) + (255*newval[0]*(w));
			lm[1] = lm[1]*(1-w) + (255*newval[1]*(w));
			lm[0] = lm[0]*(1-w) + (255*newval[2]*(w));
			return;
		}
	}

	/*special handling to make a section accept the first texture painted on it as a base texture. no more chessboard*/
	if (!*s->texname[0] && !*s->texname[1] && !*s->texname[2] && !*s->texname[3])
	{
		Q_strncpyz(s->texname[3], texname, sizeof(s->texname[3]));
		Terr_LoadSectionTextures(s);

		for (idx = 0; idx < SECTTEXSIZE*SECTTEXSIZE; idx++)
		{
			lm = ted_getlightmap(s, idx);
			lm[2] = 0;
			lm[1] = 0;
			lm[0] = 0;
		}
		return;
	}
	for (t = 0; t < 4; t++)
	{
		if (!*s->texname[t])
		{
			Q_strncpyz(s->texname[t], texname, sizeof(s->texname[t]));

			newval[0] = (t == 0);
			newval[1] = (t == 1);
			newval[2] = (t == 2);
			lm[2] = lm[2]*(1-w) + (255*newval[0]*(w));
			lm[1] = lm[1]*(1-w) + (255*newval[1]*(w));
			lm[0] = lm[0]*(1-w) + (255*newval[2]*(w));

			Terr_LoadSectionTextures(s);
			return;
		}
	}
}

static void ted_texreplace(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	if (w > 0)
		ted_texpaint(ctx, s, idx, wx, wy, 1);
}

/*
static void ted_texlight(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	vec3_t pos, pos2;
	vec3_t norm, tnorm;
	vec3_t ldir = {0.4, 0.7, 2};
	float d;
	int x,y;
	trace_t tr;
	VectorClear(norm);
	for (y = -4; y < 4; y++)
	for (x = -4; x < 4; x++)
	{
		pos[0] = wx - (CHUNKBIAS + x/64.0) * s->hmmod->sectionsize;
		pos[1] = wy - (CHUNKBIAS + y/64.0) * s->hmmod->sectionsize;
#if 0
		pos[2] = 10000;
		pos2[0] = wx - (CHUNKBIAS + x/64.0) * s->hmmod->sectionsize;
		pos2[1] = wy - (CHUNKBIAS + y/64.0) * s->hmmod->sectionsize;
		pos2[2] = -10000;
		Heightmap_Trace(cl.worldmodel, 0, 0, NULL, pos, pos2, vec3_origin, vec3_origin, FTECONTENTS_SOLID, &tr);
		VectorCopy(tr.plane.normal, tnorm);
#else
		Heightmap_Normal(s->hmmod, pos, tnorm);
#endif
		d = sqrt(32 - x*x+y*y);
		VectorMA(norm, d, tnorm, norm);
	}

	VectorNormalize(ldir);
	VectorNormalize(norm);
	d = DotProduct(ldir, norm);
	if (d < 0)
		d = 0;
	lm[3] = d*255;
}
*/
static void ted_texset(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	if (w > 1)
		w = 1;
	s->flags |= TSF_NOTIFY|TSF_EDITED;

	lm[2] = lm[2]*(1-w) + (255*((float*)ctx)[0]*(w));
	lm[1] = lm[1]*(1-w) + (255*((float*)ctx)[1]*(w));
	lm[0] = lm[0]*(1-w) + (255*((float*)ctx)[2]*(w));
}

static void ted_textally(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	((float*)ctx)[0] += lm[0]*w;
	((float*)ctx)[1] += lm[1]*w;
	((float*)ctx)[2] += lm[2]*w;
	((float*)ctx)[3] += w;
}

static void ted_tint(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	float *col = s->colours[idx];
	float *newval = ctx;
	if (w > 1)
		w = 1;
	s->flags |= TSF_NOTIFY|TSF_DIRTY|TSF_EDITED|TSF_HASCOLOURS;	/*dirty because of the vbo*/
	col[0] = col[0]*(1-w) + (newval[0]*(w));
	col[1] = col[1]*(1-w) + (newval[1]*(w));
	col[2] = col[2]*(1-w) + (newval[2]*(w));
	col[3] = col[3]*(1-w) + (newval[3]*(w));
}

enum
{
	tid_linear,
	tid_exponential,
	tid_square_linear,
	tid_square_exponential,
	tid_flat
};
//calls 'func' for each tile upon the terrain. the 'tile' can be either height or texel
static void ted_itterate(heightmap_t *hm, int distribution, float *pos, float radius, float strength, int steps, void(*func)(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength), void *ctx)
{
	int tx, ty;
	float wx, wy;
	float sc[2];
	int min[2], max[2];
	int sx,sy;
	hmsection_t *s;
	float w, xd, yd;

	if (radius < 0)
	{
		radius *= -1;
		distribution |= 2;
	}

	min[0] = floor((pos[0] - radius)/(hm->sectionsize) - 1.5);
	min[1] = floor((pos[1] - radius)/(hm->sectionsize) - 1.5);
	max[0] = ceil((pos[0] + radius)/(hm->sectionsize) + 1.5);
	max[1] = ceil((pos[1] + radius)/(hm->sectionsize) + 1.5);

	min[0] = bound(hm->firstsegx, min[0], hm->maxsegx);
	min[1] = bound(hm->firstsegy, min[1], hm->maxsegy);
	max[0] = bound(hm->firstsegx, max[0], hm->maxsegx);
	max[1] = bound(hm->firstsegy, max[1], hm->maxsegy);

	sc[0] = hm->sectionsize/(steps-1);
	sc[1] = hm->sectionsize/(steps-1);

	for (sy = min[1]; sy < max[1]; sy++)
	{
		for (sx = min[0]; sx < max[0]; sx++)
		{
			s = Terr_GetSection(hm, sx, sy, TGS_WAITLOAD|TGS_DEFAULTONFAIL);
			if (!s)
				continue;

			for (ty = 0; ty < steps; ty++)
			{
				wy = (sy*(steps-1.0) + ty)*sc[1];
				yd = wy - pos[1];// - sc[1]/4;
				for (tx = 0; tx < steps; tx++)
				{
					/*both heights and textures have an overlapping/matching sample at the edge, there's no need for any half-pixels or anything here*/
					wx = (sx*(steps-1.0) + tx)*sc[0];
					xd = wx - pos[0];// - sc[0]/4;

					switch(distribution)
					{
					case tid_exponential:
						w = radius*radius - (xd*xd+yd*yd);
						if (w > 0)
							func(ctx, s, tx+ty*steps, wx, wy, sqrt(w)*strength/(radius));
						break;
					case tid_linear:
						w = radius - sqrt(xd*xd+yd*yd);
						if (w > 0)
							func(ctx, s, tx+ty*steps, wx, wy, w*strength/(radius));
						break;
					case tid_square_exponential:
						w = max(fabs(xd), fabs(yd));
						w = radius*radius - w*w;
						if (w > 0)
							func(ctx, s, tx+ty*steps, wx, wy, sqrt(w)*strength/(radius));
						break;
					case tid_square_linear:
						w = max(fabs(xd), fabs(yd));
						w = radius - w;
						if (w > 0)
							func(ctx, s, tx+ty*steps, wx, wy, w*strength/(radius));
						break;
					case tid_flat:
						w = max(fabs(xd), fabs(yd));
						w = radius - w;
						if (w > 0)
							func(ctx, s, tx+ty*steps, wx, wy, strength);
						break;
					}
				}
			}
		}
	}
}

void ted_texkill(hmsection_t *s, const char *killtex)
{
	int x, y, t, to;
	if (!s)
		return;
	for (t = 0; t < 4; t++)
	{
		if (!strcmp(s->texname[t], killtex))
		{
			unsigned char *lm = ted_getlightmap(s, 0);
			s->flags |= TSF_EDITED;
			s->texname[t][0] = 0;
			for (to = 0; to < 4; to++)
				if (*s->texname[to])
					break;
			if (to == 4)
				to = 0;

			if (to == 0 || to == 2)
				to = 2 - to;
			if (t == 0 || t == 2)
				t = 2 - t;

			for (y = 0; y < SECTTEXSIZE; y++)
			{
				for (x = 0; x < SECTTEXSIZE; x++, lm+=4)
				{
					if (t == 3)
					{
						//to won't be 3
						lm[to] = lm[to] + (255 - (lm[0] + lm[1] + lm[2]));
					}
					else
					{
						if (to != 3)
							lm[to] = (lm[to]+lm[t])&0xff;
						lm[t] = 0;
					}
				}
				lm += SECTTEXSIZE*4*(LMCHUNKS-1);
			}
			if (t == 0 || t == 2)
				t = 2 - t;
			Terr_LoadSectionTextures(s);
		}
	}
}

void QCBUILTIN PF_terrain_edit(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *vmw = prinst->parms->user;
	int action = G_FLOAT(OFS_PARM0);
	vec3_t pos;// G_VECTOR(OFS_PARM1);
	float radius = G_FLOAT(OFS_PARM2);
	float quant = G_FLOAT(OFS_PARM3);
//	G_FLOAT(OFS_RETURN) = Heightmap_Edit(w->worldmodel, action, pos, radius, quant);
	model_t *mod = vmw->Get_CModel(vmw, ((wedict_t*)PROG_TO_EDICT(prinst, *vmw->g.self))->v->modelindex);
	heightmap_t *hm;
	vec4_t tally;

	G_FLOAT(OFS_RETURN) = 0;

	if (!mod || !mod->terrain)
	{
		if (mod)
		{
			char basename[MAX_QPATH];
			COM_FileBase(mod->name, basename, sizeof(basename));
			mod->terrain = Mod_LoadTerrainInfo(mod, basename, true);
			G_FLOAT(OFS_RETURN) = !!mod->terrain;
		}
		return;
	}
	hm = mod->terrain;

	pos[0] = G_FLOAT(OFS_PARM1+0) + hm->sectionsize * CHUNKBIAS;
	pos[1] = G_FLOAT(OFS_PARM1+1) + hm->sectionsize * CHUNKBIAS;
	pos[2] = G_FLOAT(OFS_PARM1+2);

	switch(action)
	{
	case ter_reload:
		G_FLOAT(OFS_RETURN) = 1;
		Terr_PurgeTerrainModel(mod, false, true);
		break;
	case ter_save:
		quant = Heightmap_Save(hm);
		Con_DPrintf("ter_save: %g sections saved\n", quant);
		G_FLOAT(OFS_RETURN) = quant;
		break;
	case ter_sethole:
	/*	{
			int x, y;
			hmsection_t *s;
			x = pos[0]*4 / hm->sectionsize;
			y = pos[1]*4 / hm->sectionsize;
			x = bound(hm->firstsegx*4, x, hm->maxsegx*4-1);
			y = bound(hm->firstsegy*4, y, hm->maxsegy*4-1);
		
			s = Terr_GetSection(hm, x/4, y/4, TGS_FORCELOAD);
			if (!s)
				return;
			ted_sethole(&quant, s, (x&3) + (y&3)*4, x/4, y/4, 0);
		}
	*/	
		pos[0] -= 0.5 * hm->sectionsize / 8;
		pos[1] -= 0.5 * hm->sectionsize / 8;
		ted_itterate(hm, tid_linear, pos, radius, 1, 9, ted_sethole, &quant);
		break;
	case ter_height_set:
		ted_itterate(hm, tid_linear, pos, radius, 1, SECTHEIGHTSIZE, ted_heightset, &quant);
		break;
	case ter_height_flatten:
		tally[0] = 0;
		tally[1] = 0;
		ted_itterate(hm, tid_exponential, pos, radius, 1, SECTHEIGHTSIZE, ted_heighttally, &tally);
		tally[0] /= tally[1];
		if (IS_NAN(tally[0]))
			tally[0] = 0;
		ted_itterate(hm, tid_exponential, pos, radius, quant, SECTHEIGHTSIZE, ted_heightsmooth, &tally);
		break;
	case ter_height_smooth:
		tally[0] = 0;
		tally[1] = 0;
		ted_itterate(hm, tid_linear, pos, radius, 1, SECTHEIGHTSIZE, ted_heighttally, &tally);
		tally[0] /= tally[1];
		if (IS_NAN(tally[0]))
			tally[0] = 0;
		ted_itterate(hm, tid_linear, pos, radius, quant, SECTHEIGHTSIZE, ted_heightsmooth, &tally);
		break;
	case ter_height_spread:
		tally[0] = 0;
		tally[1] = 0;
		ted_itterate(hm, tid_exponential, pos, radius/2, 1, SECTHEIGHTSIZE, ted_heighttally, &tally);
		tally[0] /= tally[1];
		if (IS_NAN(tally[0]))
			tally[0] = 0;
		ted_itterate(hm, tid_exponential, pos, radius, 1, SECTHEIGHTSIZE, ted_heightsmooth, &tally);
		break;
	case ter_water_set:
		ted_itterate(hm, tid_linear, pos, radius, 1, 9, ted_waterset, &quant);
		break;
	case ter_lower:
		quant *= -1;
	case ter_raise:
		ted_itterate(hm, tid_exponential, pos, radius, quant, SECTHEIGHTSIZE, ted_heightraise, &quant);
		break;
	case ter_tint:
		ted_itterate(hm, tid_exponential, pos, radius, quant, SECTHEIGHTSIZE, ted_tint, G_VECTOR(OFS_PARM4));	//and parm5 too
		break;
//	case ter_mixset:
//		ted_itterate(hm, tid_exponential, pos, radius, 1, SECTTEXSIZE, ted_mixset, G_VECTOR(OFS_PARM4));
//		break;
	case ter_tex_blend:
		ted_itterate(hm, tid_exponential, pos, radius, quant/10, SECTTEXSIZE, ted_texpaint, (void*)PR_GetStringOfs(prinst, OFS_PARM4));
		break;
	case ter_tex_replace:
		ted_itterate(hm, tid_exponential, pos, radius, 1, SECTTEXSIZE, ted_texreplace, (void*)PR_GetStringOfs(prinst, OFS_PARM3));
		break;
	case ter_tex_concentrate:
		ted_itterate(hm, tid_exponential, pos, radius, 1, SECTTEXSIZE, ted_texconcentrate, NULL);
		break;
	case ter_tex_noise:
		ted_itterate(hm, tid_exponential, pos, radius, 1, SECTTEXSIZE, ted_texnoise, NULL);
		break;
	case ter_tex_blur:
		Vector4Set(tally, 0, 0, 0, 0);
		ted_itterate(hm, tid_exponential, pos, radius, 1, SECTTEXSIZE, ted_textally, &tally);
		VectorScale(tally, 1/(tally[3]*255), tally);
		ted_itterate(hm, tid_exponential, pos, radius, quant, SECTTEXSIZE, ted_texset, &tally);
		break;
	case ter_tex_get:
		{
			int x, y;
			hmsection_t *s;
			x = pos[0] / hm->sectionsize;
			y = pos[1] / hm->sectionsize;
			x = bound(hm->firstsegx, x, hm->maxsegx-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);
		
			s = Terr_GetSection(hm, x, y, TGS_WAITLOAD|TGS_DEFAULTONFAIL);
			if (!s)
				return;
			x = bound(0, quant, 3);
			G_INT(OFS_RETURN) = PR_TempString(prinst, s->texname[x]);
		}
		break;
	case ter_tex_kill:
		{
			int x, y;
			x = pos[0] / hm->sectionsize;
			y = pos[1] / hm->sectionsize;
			x = bound(hm->firstsegx, x, hm->maxsegx-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);

			ted_texkill(Terr_GetSection(hm, x, y, TGS_WAITLOAD|TGS_DEFAULTONFAIL), PR_GetStringOfs(prinst, OFS_PARM4));
		}
		break;
	case ter_reset:
		{
			int x, y;
			hmsection_t *s;
			x = pos[0] / hm->sectionsize;
			y = pos[1] / hm->sectionsize;
			x = bound(hm->firstsegx, x, hm->maxsegx-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);
			s = Terr_GetSection(hm, x, y, TGS_WAITLOAD|TGS_DEFAULTONFAIL);
			if (s)
			{
				s->flags = (s->flags & ~TSF_EDITED);
				Terr_ClearSection(s);
				Terr_GenerateDefault(hm, s);
			}
		}
		break;
	case ter_mesh_add:
		{
			vec3_t axis[3];
			wedict_t *ed = G_WEDICT(prinst, OFS_PARM1);
			//FIXME: modeltype pitch inversion
			AngleVectorsFLU(ed->v->angles, axis[0], axis[1], axis[2]);
			Terr_AddMesh(hm, TGS_WAITLOAD|TGS_DEFAULTONFAIL, vmw->Get_CModel(vmw, ed->v->modelindex), ed->v->origin, axis, ed->xv->scale);
		}
		break;
	case ter_mesh_kill:
		{
			int i;
//			entity_t *e;
			int x, y;
//			float r;
			hmsection_t *s;
			x = pos[0] / hm->sectionsize;
			y = pos[1] / hm->sectionsize;
			x = bound(hm->firstsegx, x, hm->maxsegx-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);
		
			s = Terr_GetSection(hm, x, y, TGS_WAITLOAD|TGS_DEFAULTONFAIL);
			if (!s)
				return;

			Sys_LockMutex(hm->entitylock);
			//FIXME: this doesn't work properly.
			if (s->numents)
			{
				for (i = 0; i < s->numents; i++)
					s->ents[i]->refs -= 1;
				s->flags |= TSF_EDITED;
				s->numents = 0;
			}
			Sys_UnlockMutex(hm->entitylock);
		}
		break;
	}
}
#else
void QCBUILTIN PF_terrain_edit(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = 0;
}
#endif

void Terr_ParseEntityLump(char *data, heightmap_t *heightmap)
{
	char key[128];
	char value[2048];

	heightmap->sectionsize = 1024;
	heightmap->mode = HMM_TERRAIN;

	heightmap->defaultgroundheight = 0;
	heightmap->defaultwaterheight = 0;
	Q_snprintfz(heightmap->defaultwatershader, sizeof(heightmap->defaultwatershader), "water/%s", heightmap->path);
	Q_strncpyz(heightmap->defaultgroundtexture, "", sizeof(heightmap->defaultgroundtexture));

	if (data)
	if ((data=COM_ParseOut(data, key, sizeof(key))))	//read the map info.
	if (key[0] == '{')
	while (1)
	{
		if (!(data=COM_ParseOut(data, key, sizeof(key))))
			break; // error
		if (key[0] == '}')
			break; // end of worldspawn
		if (key[0] == '_')
			memmove(key, key+1, strlen(key)); //_ vars are for comments/utility stuff that arn't visible to progs and for compat. We want to support these stealth things.
		if (!((data=COM_ParseOut(data, value, sizeof(value)))))
			break; // error		
		if (!strcmp("segmentsize", key))
			heightmap->sectionsize = atof(value);
		else if (!strcmp("minxsegment", key))
			heightmap->firstsegx = atoi(value);
		else if (!strcmp("minysegment", key))
			heightmap->firstsegy = atoi(value);
		else if (!strcmp("maxxsegment", key))
			heightmap->maxsegx = atoi(value);
		else if (!strcmp("maxysegment", key))
			heightmap->maxsegy = atoi(value);
		else if (!strcmp("defaultwaterheight", key))
			heightmap->defaultwaterheight = atof(value);
		else if (!strcmp("defaultgroundheight", key))
			heightmap->defaultgroundheight = atof(value);
		else if (!strcmp("defaultgroundtexture", key))
			Q_strncpyz(heightmap->defaultgroundtexture, value, sizeof(heightmap->defaultgroundtexture));
		else if (!strcmp("defaultwatertexture", key))
			Q_strncpyz(heightmap->defaultwatershader, value, sizeof(heightmap->defaultwatershader));
		else if (!strcmp("tiles", key))
		{
			char *d;
			heightmap->mode = HMM_BLOCKS;
			d = value;
			d = COM_ParseOut(d, key, sizeof(key));
			heightmap->tilepixcount[0] = atoi(key);
			d = COM_ParseOut(d, key, sizeof(key));
			heightmap->tilepixcount[1] = atoi(key);
			d = COM_ParseOut(d, key, sizeof(key));
			heightmap->tilecount[0] = atoi(key);
			d = COM_ParseOut(d, key, sizeof(key));
			heightmap->tilecount[1] = atoi(key);
		}
	}

	/*bias and bound it*/
	heightmap->firstsegx += CHUNKBIAS;
	heightmap->firstsegy += CHUNKBIAS;
	heightmap->maxsegx += CHUNKBIAS;
	heightmap->maxsegy += CHUNKBIAS;
	if (heightmap->firstsegx < 0)
		heightmap->firstsegx = 0;
	if (heightmap->firstsegy < 0)
		heightmap->firstsegy = 0;
	if (heightmap->maxsegx > CHUNKLIMIT)
		heightmap->maxsegx = CHUNKLIMIT;
	if (heightmap->maxsegy > CHUNKLIMIT)
		heightmap->maxsegy = CHUNKLIMIT;
}

void Terr_FinishTerrain(model_t *mod)
{
	heightmap_t *hm = mod->terrain;
#ifndef SERVERONLY
	if (qrenderer != QR_NONE)
	{
		if (*hm->skyname)
			hm->skyshader = R_RegisterCustom(va("skybox_%s", hm->skyname), SUF_NONE, Shader_DefaultSkybox, NULL);
		else
			hm->skyshader = NULL;

		switch (hm->mode)
		{
		case HMM_BLOCKS:
			hm->shader = R_RegisterShader("terraintileshader", SUF_NONE,
					"{\n"
						"{\n"
							"map $diffuse\n"	
						"}\n"
					"}\n"
				);
			break;
		case HMM_TERRAIN:
			hm->shader = R_RegisterShader(hm->groundshadername, SUF_LIGHTMAP,
					"{\n"
						"bemode rtlight\n"
							"{\n"
								"{\n"
									"map $diffuse\n"
									"blendfunc add\n"
								"}\n"
								"{\n"
									"map $upperoverlay\n"
								"}\n"
								"{\n"
									"map $loweroverlay\n"
								"}\n"
								"{\n"
									"map $fullbright\n"
								"}\n"
								"{\n"
									"map $lightmap\n"
								"}\n"
								"{\n"
									"map $shadowmap\n"
								"}\n"
								"{\n"
									"map $lightcubemap\n"
								"}\n"
								//woo, one glsl to rule them all
								"program terrain#RTLIGHT\n"
							"}\n"
						"bemode depthdark\n"
							"{\n"
								"program depthonly\n"
								"{\n"
									"depthwrite\n"
								"}\n"
							"}\n"
						"bemode depthonly\n"
							"{\n"
								"program depthonly\n"
								"{\n"
									"depthwrite\n"
									"colormask\n"
								"}\n"
							"}\n"

						"{\n"
							"map $diffuse\n"
						"}\n"
						"{\n"
							"map $upperoverlay\n"
						"}\n"
						"{\n"
							"map $loweroverlay\n"
						"}\n"
						"{\n"
							"map $fullbright\n"
						"}\n"
						"{\n"
							"map $lightmap\n"
						"}\n"
						"program terrain\n"
						"if r_terraindebug\n"
						"program terraindebug\n"
						"endif\n"
					"}\n"
				);
			break;
		}
	}
#endif
}

int Fragment_ClipPolyToPlane(float *inverts, float *outverts, int incount, float *plane, float planedist);
size_t Terr_GenerateBrushFace(vecV_t *points, size_t maxpoints, vec4_t *planes, size_t numplanes, vec4_t face)
{
	int p;
	vec4_t verts[128];
	vec4_t verts2[128];
	vec4_t *cverts;
	int flip;
//	vec3_t d1, d2, n;
	size_t numverts;

	//generate some huge quad/poly aligned with the plane
	vec3_t tmp = {0.1,0.04,0.96};
	vec3_t right, forward;
	
//	if (face[2] != 1)
//		return 0;

	CrossProduct(face, tmp, right);
	VectorNormalize(right);
	CrossProduct(face, right, forward);
	VectorNormalize(forward);

	VectorScale(face, face[3],			verts[0]);
	VectorMA(verts[0], 8192, right,		verts[0]);
	VectorMA(verts[0], 8192, forward,	verts[0]);

	VectorScale(face, face[3],			verts[1]);
	VectorMA(verts[1], 8192, right,		verts[1]);
	VectorMA(verts[1], -8192, forward,	verts[1]);

	VectorScale(face, face[3],			verts[2]);
	VectorMA(verts[2], -8192, right,	verts[2]);
	VectorMA(verts[2], -8192, forward,	verts[2]);

	VectorScale(face, face[3],			verts[3]);
	VectorMA(verts[3], -8192, right,	verts[3]);
	VectorMA(verts[3], 8192, forward,	verts[3]);

	numverts = 4;


	//clip the quad to the various other planes
	flip = 0;
	for (p = 0; p < numplanes; p++)
	{
		if (planes[p] != face)
		{
			vec3_t norm;
			flip^=1;
			VectorNegate(planes[p], norm);
			if (flip)
				numverts = Fragment_ClipPolyToPlane((float*)verts, (float*)verts2, numverts, norm, -planes[p][3]);
			else
				numverts = Fragment_ClipPolyToPlane((float*)verts2, (float*)verts, numverts, norm, -planes[p][3]);
	
			if (numverts < 3)	//totally clipped.
				return 0;
		}
	}

	if (numverts > maxpoints)
		return 0;

	if (flip)
		cverts = verts2;
	else
		cverts = verts;
	for (p = 0; p < numverts; p++)
	{
		VectorCopy(cverts[p], points[p]);
	}

	return numverts;
}

#ifndef SERVERONLY
void Terr_Brush_Draw(heightmap_t *hm, batch_t **batches, entity_t *e)
{
#ifndef _DEBUG
	return;
#else
	batch_t *b;
	size_t i, j;
	vbobctx_t ctx;

	brushtex_t *bt;
	brushes_t *br;

	static vecV_t coord[65536];
	static vec2_t texcoord[65536];
	static vec3_t normal[65536];
	static vec3_t svector[65536];
	static vec3_t tvector[65536];
	static index_t index[65535];
	size_t numverts = 0;
	size_t numindicies = 0;
	int w, h;
	float scale[2];

	for (bt = hm->brushtextures; bt; bt = bt->next)
	{
		if (!bt->shader)
		{
			if (!strcmp(bt->shadername, "clip"))
				bt->shader = R_RegisterShader(bt->shadername, SUF_LIGHTMAP, "{\nsurfaceparm nodraw\n}");
			else
				bt->shader = R_RegisterCustom (bt->shadername, SUF_LIGHTMAP, Shader_DefaultBSPQ1, NULL);
//				bt->shader = R_RegisterShader_Lightmap(bt->shadername);
			R_BuildDefaultTexnums(NULL, bt->shader);
		}

		if (bt->rebuild)
		{
			//FIXME: don't block.
			if (R_GetShaderSizes(bt->shader, &w, &h, false) < 0)
				continue;
			bt->rebuild = false;

			if (w<1) w = 64;
			if (h<1) h = 64;
			scale[0] = 1.0/w;	//I hate needing this.
			scale[1] = 1.0/h;

			BE_VBO_Destroy(&bt->vbo.coord);
			BE_VBO_Destroy(&bt->vbo.indicies);

			for (i = 0, br = hm->wbrushes; i < hm->numbrushes; i++, br++)
			{
				for (j = 0; j < br->numplanes; j++)
				{
					if (br->faces[j].tex == bt)
					{
						size_t add, k, o;
						//this face needs to be built
						add = Terr_GenerateBrushFace(&coord[numverts], 64, br->planes, br->numplanes, br->planes[j]);
						for (k = 0, o = numverts; k < add; k++, o++)
						{
	//						VectorCopy(points[k], coord[o]);
							VectorCopy(br->planes[j], normal[o]);
							VectorCopy(br->faces[j].sdir, svector[o]);
							VectorCopy(br->faces[j].tdir, tvector[o]);

							//compute the texcoord plane
							texcoord[o][0] = (DotProduct(svector[o], coord[o]) + br->faces[j].sdir[3]) * scale[0];
							texcoord[o][1] = (DotProduct(tvector[o], coord[o]) + br->faces[j].tdir[3]) * scale[1];
						}
						for (k = 2; k < add; k++)
						{	//triangle fans
							index[numindicies++] = numverts + 0;
							index[numindicies++] = numverts + k-1;
							index[numindicies++] = numverts + k-0;
						}
						numverts += add;
					}
				}
			}

			BE_VBO_Begin(&ctx, (sizeof(coord[0])+sizeof(texcoord[0])+sizeof(normal[0])+sizeof(svector[0])+sizeof(tvector[0])) * numverts);
			BE_VBO_Data(&ctx, coord, sizeof(coord[0])*numverts, &bt->vbo.coord);
			BE_VBO_Data(&ctx, texcoord, sizeof(texcoord[0])*numverts, &bt->vbo.texcoord);
			BE_VBO_Data(&ctx, normal, sizeof(normal[0])*numverts, &bt->vbo.normals);
			BE_VBO_Data(&ctx, svector, sizeof(svector[0])*numverts, &bt->vbo.svector);
			BE_VBO_Data(&ctx, tvector, sizeof(tvector[0])*numverts, &bt->vbo.tvector);
			BE_VBO_Finish(&ctx, index, sizeof(index[0])*numindicies, &bt->vbo.indicies);
			bt->pmesh = &bt->mesh;
			bt->mesh.numindexes = numindicies;
			bt->mesh.numvertexes = numverts;
		}
		if (!bt->mesh.numindexes)
			continue;	//o.O

		b = BE_GetTempBatch();
		if (b)
		{
			for (j = 0; j < MAXRLIGHTMAPS; j++)
				b->lightmap[j] = -1;
			b->ent = e;
			b->shader = bt->shader;
			b->flags = 0;
			b->mesh = &bt->pmesh;
			b->meshes = 1;
			b->buildmeshes = NULL;
			b->skin = &b->shader->defaulttextures;
			b->texture = NULL;
			b->vbo = &bt->vbo;

			b->next = batches[b->shader->sort];
			batches[b->shader->sort] = b;
		}
	}
#endif
}
#endif

brushtex_t *Terr_Brush_FindTexture(heightmap_t *hm, char *texname)
{
	brushtex_t *bt;
	if (!hm)
		return NULL;

	for (bt = hm->brushtextures; bt; bt = bt->next)
	{
		if (!strcmp(bt->shadername, texname))
			return bt;
	}
	bt = Z_Malloc(sizeof(*bt));
	bt->next = hm->brushtextures;
	hm->brushtextures = bt;
	Q_strncpyz(bt->shadername, texname, sizeof(bt->shadername));

	return bt;
}

void Terr_Brush_Insert(heightmap_t *hm, brushes_t *brush)
{
	int i;
	brushes_t *out;
	if (!hm)
		return;

	hm->wbrushes = BZ_Realloc(hm->wbrushes, sizeof(*hm->wbrushes) * (hm->numbrushes+1));
	out = &hm->wbrushes[hm->numbrushes];
	out->contents = brush->contents;
	out->numplanes = brush->numplanes;
	out->planes = BZ_Malloc((sizeof(*out->planes)+sizeof(*out->faces)) * out->numplanes);
	out->faces = (void*)(out->planes+out->numplanes);
	for (i = 0; i < out->numplanes; i++)
	{
		Vector4Copy(brush->planes[i], out->planes[i]);
		out->faces[i].tex = brush->faces[i].tex;
		Vector4Copy(brush->faces[i].sdir, out->faces[i].sdir);
		Vector4Copy(brush->faces[i].tdir, out->faces[i].tdir);

		//make sure this stuff is rebuilt properly.
		out->faces[i].tex->rebuild = true;
	}
	hm->numbrushes+=1;
}

void Terr_ReformEntitiesLump(model_t *mod, heightmap_t *hm, char *entities)
{
	char token[8192];
	int nest = 0;
	int buflen = strlen(entities);
	char *out, *start;
	int i;
	int submodelnum = 0;
	qboolean foundsubmodel = false;
	qboolean inbrush = false;
	int numplanes = 0;
	vec4_t planes[64];
	struct brushface_s faces[64];
	int brushcontents = FTECONTENTS_SOLID;
	heightmap_t *subhm = NULL;
	model_t *submod = NULL;

	/*FIXME: we need to re-form the entities lump to insert model fields as appropriate*/
	mod->entities = out = ZG_Malloc(&mod->memgroup, buflen+1);

	while(entities)
	{
		start = entities;
		entities = COM_ParseOut(entities, token, sizeof(token));
		if (token[0] == '}' && token[1] == 0)
		{
			nest--;
			if (inbrush)
			{
				brushes_t brush;
				//finish the brush
				brush.contents = brushcontents;
				brush.numplanes = numplanes;
				brush.planes = planes;
				brush.faces = faces;
				if (numplanes)
					Terr_Brush_Insert(subhm, &brush);
				numplanes = 0;
				inbrush = false;
				continue;
			}
		}
		else if (token[0] == '{' && token[1] == 0)
		{
			nest++;
			if (nest == 1)
				foundsubmodel = false;
			if (nest == 2)
			{
				if (!foundsubmodel)
				{
					foundsubmodel = true;
					if (submodelnum)
					{
						Q_snprintfz(token, sizeof(token), "*%i:%s", submodelnum, mod->name);
						*out++ = 'm';
						*out++ = 'o';
						*out++ = 'd';
						*out++ = 'e';
						*out++ = 'l';
						*out++ = ' ';
						*out++ = '\"';
						for (i = 0; token[i]; i++)
							*out++ = token[i];
						*out++ = '\"';
						*out++ = ' ';
						
						submod = Mod_FindName (token);
						if (submod->loadstate == MLS_NOTLOADED)
						{
							submod->type = mod_heightmap;
							submod->entities = "";
							subhm = submod->terrain = Mod_LoadTerrainInfo(submod, submod->name, true);

							subhm->exteriorcontents = FTECONTENTS_EMPTY;

							ClearBounds(submod->mins, submod->maxs);

							submod->funcs.NativeTrace			= Heightmap_Trace;
							submod->funcs.PointContents			= Heightmap_PointContents;
							submod->funcs.NativeContents		= Heightmap_NativeBoxContents;
							submod->funcs.LightPointValues		= Heightmap_LightPointValues;
							submod->funcs.StainNode				= Heightmap_StainNode;
							submod->funcs.MarkLights			= Heightmap_MarkLights;
							submod->funcs.ClusterForPoint		= Heightmap_ClusterForPoint;
							submod->funcs.ClusterPVS			= Heightmap_ClusterPVS;
#ifndef CLIENTONLY
							submod->funcs.FindTouchedLeafs		= Heightmap_FindTouchedLeafs;
							submod->funcs.EdictInFatPVS			= Heightmap_EdictInFatPVS;
							submod->funcs.FatPVS				= Heightmap_FatPVS;
#endif
							submod->loadstate = MLS_LOADED;
						}
						else
							subhm = NULL;
					}
					else
					{
						submod = mod;
						subhm = hm;
					}
					submodelnum++;
				}
				inbrush = true;
				continue;
			}
		}
		else if (inbrush)
		{
			//parse a plane
			//( -0 -0 16 ) ( -0 -0 32 ) ( 64 -0 16 ) texname 0 -32 rotation sscale tscale
			//( -0 -0 16 ) ( -0 -0 32 ) ( 64 -0 16 ) texname [x y z d] [x y z d] rotation sscale tscale
			brushtex_t *bt;
			vec3_t d1,d2;
			vec3_t points[3];
			vec4_t texplane[2];
			float scale[2], rot;
			int p, a;
			memset(points, 0, sizeof(points));
			for (p = 0; p < 3; p++)
			{
				if (token[0] != '(' || token[1] != 0)
					break;
				entities = COM_ParseOut(entities, token, sizeof(token));
				points[p][0] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				points[p][1] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				points[p][2] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				if (token[0] != ')' || token[1] != 0)
					break;
				entities = COM_ParseOut(entities, token, sizeof(token));

				for (a = 0; a < 3; a++)
				{
					if (submod->mins[a] > points[p][a])
						submod->mins[a] = points[p][a];
					if (submod->maxs[a] < points[p][a])
						submod->maxs[a] = points[p][a];
				}
			}

			bt = Terr_Brush_FindTexture(subhm, token);
			if (*token == '*')
			{
				if (!strncmp(token, "*lava", 5))
					brushcontents = FTECONTENTS_LAVA;
				else if (!strncmp(token, "*slime", 5))
					brushcontents = FTECONTENTS_SLIME;
				else
					brushcontents = FTECONTENTS_WATER;
			}
			else if (!strcmp(token, "clip"))
				brushcontents = FTECONTENTS_PLAYERCLIP;
			else
				brushcontents = FTECONTENTS_SOLID;

			//FIXME: halflife format has the entire [x y z dist] plane specified.
			entities = COM_ParseOut(entities, token, sizeof(token));
			if (*token == '[')
			{
				texplane[0][0] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				texplane[0][1] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				texplane[0][2] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				texplane[0][3] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				//]
				entities = COM_ParseOut(entities, token, sizeof(token));
				//[
				entities = COM_ParseOut(entities, token, sizeof(token));
				texplane[1][0] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				texplane[1][1] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				texplane[1][2] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				texplane[1][3] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				//]
			}
			else
			{
				texplane[0][3] = atof(token);
				entities = COM_ParseOut(entities, token, sizeof(token));
				texplane[1][3] = atof(token);
			}

			entities = COM_ParseOut(entities, token, sizeof(token));
			rot = atof(token);
			entities = COM_ParseOut(entities, token, sizeof(token));
			scale[0] = atof(token);
			entities = COM_ParseOut(entities, token, sizeof(token));
			scale[1] = atof(token);

			VectorSubtract(points[0], points[1], d1);
			VectorSubtract(points[2], points[1], d2);
			CrossProduct(d1, d2, planes[numplanes]);
			VectorNormalize(planes[numplanes]);
			planes[numplanes][3] = DotProduct(points[1], planes[numplanes]);
			faces[numplanes].tex = bt;

			//quake's .maps use the normal to decide which texture directions to use in some lame axially-aligned way.
			{
				float a=fabs(planes[numplanes][0]),b=fabs(planes[numplanes][1]),c=fabs(planes[numplanes][2]);
				VectorClear(texplane[0]);
				VectorClear(texplane[1]);
				if (a>b&&a>c)
					texplane[0][1] = 1;
				else
					texplane[0][0] = 1;
				if (c>a&&c>b)
					texplane[1][1] = -1;
				else
					texplane[1][2] = -1;
			}

			if (rot)
			{
				int mas, mat;
				float s,t;
				float a = rot*(M_PI/180);
				float cosa = cos(a), sina=sin(a);
				for (mas=0; mas<2&&!texplane[0][mas]; mas++);
				for (mat=0; mat<2&&!texplane[1][mat]; mat++);
				for (i = 0; i < 2; i++)
				{
					s = cosa*texplane[i][mas] - sina*texplane[i][mat];
					t = sina*texplane[i][mas] + cosa*texplane[i][mat];
					texplane[i][mas] = s;
					texplane[i][mat] = t;
				}
			}

			if (!scale[0]) scale[0] = 1;
			if (!scale[1]) scale[1] = 1;
			VectorScale(texplane[0], 1.0/scale[0], faces[numplanes].sdir);
			VectorScale(texplane[1], 1.0/scale[1], faces[numplanes].tdir);
			faces[numplanes].sdir[3] = texplane[0][3];
			faces[numplanes].tdir[3] = texplane[1][3];

			numplanes++;
			continue;
		}
		while(start < entities)
			*out++ = *start++;
	}
	*out++ = 0;
}

qboolean QDECL Terr_LoadTerrainModel (model_t *mod, void *buffer, size_t bufsize)
{
	int exterior = FTECONTENTS_SOLID;
	heightmap_t *hm;

	char token[MAX_QPATH];
	int sectsize = 0;
	char *src;

	src = COM_ParseOut(buffer, token, sizeof(token));
	if (!strcmp(token, "terrain"))
		buffer = src;
	else if (!strcmp(token, "{"))
		exterior = FTECONTENTS_EMPTY;
	else
	{
		Con_Printf(CON_ERROR "%s wasn't terrain map\n", mod->name);	//shouldn't happen
		return false;
	}

	mod->type = mod_heightmap;

	hm = Z_Malloc(sizeof(*hm));
	ClearLink(&hm->recycle);
//	ClearLink(&hm->collected);
	COM_FileBase(mod->name, hm->path, sizeof(hm->path));

	Terr_ReformEntitiesLump(mod, hm, buffer);

	strcpy(hm->groundshadername, "terrainshader");
	strcpy(hm->skyname, "sky1");

	hm->entitylock = Sys_CreateMutex();
	hm->sectionsize = sectsize;
	if (exterior)
	{
		hm->firstsegx = -1;
		hm->firstsegy = -1;
		hm->maxsegx = +1;
		hm->maxsegy = +1;
	}
	else
	{
		hm->firstsegx = 0;
		hm->firstsegy = 0;
		hm->maxsegx = 0;
		hm->maxsegy = 0;
	}
	hm->exteriorcontents = exterior;	//sky outside the map

	Terr_ParseEntityLump(mod->entities, hm);

	mod->mins[0] = (hm->firstsegx - CHUNKBIAS) * hm->sectionsize;
	mod->mins[1] = (hm->firstsegy - CHUNKBIAS) * hm->sectionsize;
	mod->mins[2] = -999999999999999999999999.f;
	mod->maxs[0] = (hm->maxsegx - CHUNKBIAS) * hm->sectionsize;
	mod->maxs[1] = (hm->maxsegy - CHUNKBIAS) * hm->sectionsize;
	mod->maxs[2] = 999999999999999999999999.f;

	mod->funcs.NativeTrace			= Heightmap_Trace;
	mod->funcs.PointContents		= Heightmap_PointContents;

	mod->funcs.NativeContents		= Heightmap_NativeBoxContents;

	mod->funcs.LightPointValues		= Heightmap_LightPointValues;
	mod->funcs.StainNode			= Heightmap_StainNode;
	mod->funcs.MarkLights			= Heightmap_MarkLights;

	mod->funcs.ClusterForPoint		= Heightmap_ClusterForPoint;
	mod->funcs.ClusterPVS			= Heightmap_ClusterPVS;

#ifndef CLIENTONLY
	mod->funcs.FindTouchedLeafs		= Heightmap_FindTouchedLeafs;
	mod->funcs.EdictInFatPVS		= Heightmap_EdictInFatPVS;
	mod->funcs.FatPVS				= Heightmap_FatPVS;
#endif
/*	mod->hulls[0].funcs.HullPointContents = Heightmap_PointContents;
	mod->hulls[1].funcs.HullPointContents = Heightmap_PointContents;
	mod->hulls[2].funcs.HullPointContents = Heightmap_PointContents;
	mod->hulls[3].funcs.HullPointContents = Heightmap_PointContents;
*/

	mod->terrain = hm;

	return true;
}

void *Mod_LoadTerrainInfo(model_t *mod, char *loadname, qboolean force)
{
	heightmap_t *hm;
	heightmap_t potential;
	if (!mod->entities)
		return NULL;

	memset(&potential, 0, sizeof(potential));
	Terr_ParseEntityLump(mod->entities, &potential);

	if (potential.firstsegx >= potential.maxsegx || potential.firstsegy >= potential.maxsegy)
	{
		//figure out the size such that it encompases the entire bsp.
		potential.firstsegx = floor(mod->mins[0] / potential.sectionsize) + CHUNKBIAS;
		potential.firstsegy = floor(mod->mins[1] / potential.sectionsize) + CHUNKBIAS;
		potential.maxsegx = ceil(mod->maxs[0] / potential.sectionsize) + CHUNKBIAS;
		potential.maxsegy = ceil(mod->maxs[1] / potential.sectionsize) + CHUNKBIAS;
		if (*loadname=='*')
		{
			potential.firstsegx = bound(0, potential.firstsegx, CHUNKLIMIT);
			potential.firstsegy = bound(0, potential.firstsegy, CHUNKLIMIT);
			potential.maxsegx = bound(potential.firstsegx, potential.maxsegx, CHUNKLIMIT);
			potential.maxsegy = bound(potential.firstsegx, potential.maxsegy, CHUNKLIMIT);
		}
		else
		{//bound it, such that 0 0 will always be loaded.
			potential.firstsegx = bound(0, potential.firstsegx, CHUNKBIAS);
			potential.firstsegy = bound(0, potential.firstsegy, CHUNKBIAS);
			potential.maxsegx = bound(CHUNKBIAS+1, potential.maxsegx, CHUNKLIMIT);
			potential.maxsegy = bound(CHUNKBIAS+1, potential.maxsegy, CHUNKLIMIT);
		}

		if (!force)
		{
			char sect[MAX_QPATH];
			Q_snprintfz(sect, sizeof(sect), "maps/%s/sect_%03x_%03x.hms", loadname, potential.firstsegx + (potential.maxsegx-potential.firstsegx)/2, potential.firstsegy + (potential.maxsegy-potential.firstsegy)/2);
			if (!COM_FCheckExists(sect))
			{
				Q_snprintfz(sect, sizeof(sect), "maps/%s/block_00_00.hms", loadname);
				if (!COM_FCheckExists(sect))
					return NULL;
			}
		}
	}

	hm = Z_Malloc(sizeof(*hm));
	*hm = potential;
	hm->entitylock = Sys_CreateMutex();
	ClearLink(&hm->recycle);
	Q_strncpyz(hm->path, loadname, sizeof(hm->path));
	Q_strncpyz(hm->groundshadername, "terrainshader", sizeof(hm->groundshadername));

	hm->exteriorcontents = FTECONTENTS_EMPTY;	//bsp geometry outside the heightmap

	return hm;
}

void Mod_Terrain_Create_f(void)
{
	char *mname;
	char *mdata;
	mname = va("maps/%s.hmp", Cmd_Argv(1));
	mdata = va(
		"terrain\n"
		"{\n"
			"classname \"worldspawn\"\n"
			"message \"%s\"\n"
			"_sky sky1\n"
			"_fog 0.02\n"
			"_segmentsize 1024\n"
			"_minxsegment -2048\n"
			"_minysegment -2048\n"
			"_maxxsegment 2048\n"
			"_maxysegment 2048\n"
			"//_defaultgroundtexture city4_2\n"
			"//_defaultwatertexture *water2\n"
			"//_defaultgroundheight -1024\n"
			"//_defaultwaterheight 0\n"	//hurrah, sea level.
//			"_tiles 64 64 8 8\n"
		"}\n"
		"{\n"
			"classname info_player_start\n"
			"origin \"0 0 1024\"\n"
		"}\n"
		, Cmd_Argv(2));
	COM_WriteFile(mname, mdata, strlen(mdata));

	//FIXME: create 4 sections around the origin
}
//reads in the terrain a tile at a time, and writes it out again.
//the new version will match our current format version.
//this is mostly so I can strip out old format revisions...
#ifndef SERVERONLY
void Mod_Terrain_Convert_f(void)
{
	model_t *mod;
	heightmap_t *hm;
	if (Cmd_FromGamecode())
		return;

	if (Cmd_Argc() >= 2)
		mod = Mod_FindName(va("maps/%s.hmp", Cmd_Argv(1)));
	else if (cls.state)
		mod = cl.worldmodel;
	else
		mod = NULL;
	if (!mod || mod->type == mod_dummy)
		return;
	hm = mod->terrain;
	if (!hm)
		return;

	{
		char *texkill = Cmd_Argv(2);
		hmsection_t *s;
		int x, sx;
		int y, sy;

		while(Terr_Collect(hm))	//collect as many as we can now, so when we collect later, the one that's collected is fresh.
			;
		for (y = hm->firstsegy; y < hm->maxsegy; y+=SECTIONSPERBLOCK)
		{
			Sys_Printf("%g%% complete\n", 100 * (y-hm->firstsegy)/(float)(hm->maxsegy-hm->firstsegy));
			for (x = hm->firstsegx; x < hm->maxsegx; x+=SECTIONSPERBLOCK)
			{
				for (sy = y; sy < y+SECTIONSPERBLOCK && sy < hm->maxsegy; sy++)
				{
					for (sx = x; sx < x+SECTIONSPERBLOCK && sx < hm->maxsegx; sx++)
					{
						s = Terr_GetSection(hm, sx, sy, TGS_WAITLOAD|TGS_NODOWNLOAD|TGS_NORENDER);
						if (s)
						{
							if (*texkill)
								ted_texkill(s, texkill);
							s->flags |= TSF_EDITED;
						}
					}
				}
				for (sy = y; sy < y+SECTIONSPERBLOCK && sy < hm->maxsegy; sy++)
				{
					for (sx = x; sx < x+SECTIONSPERBLOCK && sx < hm->maxsegx; sx++)
					{
						s = Terr_GetSection(hm, sx, sy, TGS_WAITLOAD|TGS_NODOWNLOAD|TGS_NORENDER);
						if (s)
						{
							if (s->flags & TSF_EDITED)
							{
								if (Terr_SaveSection(hm, s, sx, sy, true))
								{
									s->flags &= ~TSF_EDITED;
								}
							}
						}
					}
				}
				while(Terr_Collect(hm))
					;
			}
		}
		Sys_Printf("%g%% complete\n", 100.0f);
	}
}
#endif
void Mod_Terrain_Reload_f(void)
{
	model_t *mod;
	heightmap_t *hm;
	if (Cmd_Argc() >= 2)
		mod = Mod_FindName(va("maps/%s.hmp", Cmd_Argv(1)));
#ifndef SERVERONLY
	else if (cls.state)
		mod = cl.worldmodel;
#endif
	else
		mod = NULL;
	if (!mod || mod->type == mod_dummy)
		return;
	hm = mod->terrain;
	if (!hm)
		return;

	if (Cmd_Argc() >= 4)
	{
		hmsection_t *s;
		int sx = atoi(Cmd_Argv(2)) + CHUNKBIAS;
		int sy = atoi(Cmd_Argv(3)) + CHUNKBIAS;
		if (hm)
		{
			s = Terr_GetSection(hm, sx, sy, TGS_NOLOAD);
			if (s)
			{
				s->flags |= TSF_NOTIFY;
			}
		}
	}
	else
		Terr_PurgeTerrainModel(mod, false, true);
}

void Terr_Init(void)
{
	Cvar_Register(&mod_terrain_networked, "Terrain");
	Cvar_Register(&mod_terrain_defaulttexture, "Terrain");
	Cvar_Register(&mod_terrain_savever, "Terrain");
	Cmd_AddCommand("mod_terrain_create", Mod_Terrain_Create_f);
	Cmd_AddCommand("mod_terrain_reload", Mod_Terrain_Reload_f);
#ifndef SERVERONLY
	Cmd_AddCommandD("mod_terrain_convert", Mod_Terrain_Convert_f, "mod_terrain_convert [mapname] [texkill]\nConvert a terrain to the current format. If texkill is specified, only tiles with the named texture will be converted, and tiles with that texture will be stripped. This is a slow operation.");
#endif

	Mod_RegisterModelFormatText(NULL, "FTE Heightmap Map (hmp)", "terrain", Terr_LoadTerrainModel);
	Mod_RegisterModelFormatText(NULL, "Quake Map Format (map)", "{", Terr_LoadTerrainModel);
}
#endif
