#include "quakedef.h"

#ifdef TERRAIN
#include "glquake.h"
#include "shader.h"

#include "pr_common.h"

//heightmaps work thusly:
//there is one raw heightmap file
//the file is split to 4*4 sections.
//each section is textured independantly (remember banshees are capped at 256*256 pixels)
//it's built into 16 seperate display lists, these display lists are individually culled, but the drivers are expected to optimise them too.
//Tei claims 14x speedup with a single display list. hopefully we can achieve the same speed by culling per-section.
//we get 20->130
//perhaps we should build it with multitexture? (no - slower on ati)

int Surf_NewLightmaps(int count, int width, int height);

#define MAXSECTIONS 64	//this many sections max in each direction
#define SECTTEXSIZE 64	//this many texture samples per section
#define SECTHEIGHTSIZE 16 //this many height samples per section

//each section is this many sections higher in world space, to keep the middle centered at '0 0'
#define CHUNKBIAS	(MAXSECTIONS*MAXSECTIONS/2)
#define CHUNKLIMIT	(MAXSECTIONS*MAXSECTIONS)

#define LMCHUNKS 2

#define HMLMSTRIDE (LMCHUNKS*SECTTEXSIZE)

#define SECTION_MAGIC (*(int*)"HMMS")
#define SECTION_VER	0

enum
{
	//these flags can be found on disk
	TSF_HASWATER	= 1u<<0,

	//these flags should not be found on disk
	TSF_RELIGHT		= 1u<<29,	//height edited, needs relighting.
	TSF_DIRTY		= 1u<<30,	//its heightmap has changed, the mesh needs rebuilding
	TSF_EDITED		= 1u<<31	//says it needs to be written if saved

#define TSF_INTERNAL	(TSF_RELIGHT|TSF_DIRTY|TSF_EDITED)
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
} dsmesh_t;
typedef struct
{
	int magic;
	int ver;
	unsigned int flags;
	char texname[4][32];
	unsigned int texmap[SECTTEXSIZE][SECTTEXSIZE];
	float heights[SECTHEIGHTSIZE*SECTHEIGHTSIZE];
	unsigned short holes;
	float waterheight;
	float minh;
	float maxh;
	int numents;
	int entsofs;
	int reserved3;
	int reserved2;
	int reserved1;
} dsection_t;
typedef struct
{
	float heights[SECTHEIGHTSIZE*SECTHEIGHTSIZE];
	unsigned short holes;
	unsigned int flags;
	float waterheight;
	float minh, maxh;
	struct heightmap_s *hmmod;

#ifndef SERVERONLY
	char texname[4][32];
	int lightmap;
	int lmx, lmy;

	texnums_t textures;
	vbo_t vbo;
	mesh_t mesh;
	mesh_t *amesh;

	int numents;
	int maxents;
	entity_t *ents;
#endif
} hmsection_t;
typedef struct
{
	hmsection_t *section[MAXSECTIONS*MAXSECTIONS];
} hmcluster_t;
typedef struct heightmap_s {
	char path[MAX_QPATH];
	int firstsegx, firstsegy;
	int maxsegx, maxsegy; //tex/cull sections
	float sectionsize;	//each section is this big, in world coords
	hmcluster_t *cluster[MAXSECTIONS*MAXSECTIONS];
	shader_t *skyshader;
	shader_t *shader;
	mesh_t skymesh;
	mesh_t *askymesh;
	unsigned int exteriorcontents;

#ifndef SERVERONLY
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
} heightmap_t;


static void ted_dorelight(heightmap_t *hm);




static void Terr_LoadSectionTextures(hmsection_t *s)
{
#ifndef SERVERONLY
	extern texid_t missing_texture;
	//CL_CheckOrEnqueDownloadFile(s->texname[0], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[1], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[2], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[3], NULL, 0);
	s->textures.base			= *s->texname[0]?R_LoadHiResTexture(s->texname[0], NULL, 0):missing_texture;
	s->textures.upperoverlay	= *s->texname[1]?R_LoadHiResTexture(s->texname[1], NULL, 0):missing_texture;
	s->textures.loweroverlay	= *s->texname[2]?R_LoadHiResTexture(s->texname[2], NULL, 0):missing_texture;
	s->textures.fullbright		= *s->texname[3]?R_LoadHiResTexture(s->texname[3], NULL, 0):missing_texture;
	s->textures.bump			= *s->texname[0]?R_LoadHiResTexture(va("%s_norm", s->texname[0]), NULL, 0):r_nulltex;
	s->textures.specular		= *s->texname[0]?R_LoadHiResTexture(va("%s_spec", s->texname[0]), NULL, 0):r_nulltex;
#endif
}

#ifndef SERVERONLY
static void Terr_InitLightmap(hmsection_t *s)
{
	heightmap_t *hm = s->hmmod;
	struct lmsect_s *lms;

	if (!hm->unusedlmsects)
	{
		int lm;
		int i;
		lm = Surf_NewLightmaps(1, SECTTEXSIZE*LMCHUNKS, SECTTEXSIZE*LMCHUNKS);
		for (i = 0; i < LMCHUNKS*LMCHUNKS; i++)
		{
			lms = malloc(sizeof(*lms));
			lms->lm = lm;
			BE_UploadAllLightmaps();
			lms->x = (i & (LMCHUNKS-1))*SECTTEXSIZE;
			lms->y = (i / LMCHUNKS)*SECTTEXSIZE;
			lms->next = hm->unusedlmsects;
			hm->unusedlmsects = lms;
		}
	}

	lms = hm->unusedlmsects;
	hm->unusedlmsects = lms->next;
	
	s->lightmap = lms->lm;
	s->lmx = lms->x;
	s->lmy = lms->y;

	free(lms);
}
#endif

static char *Terr_DiskSectionName(heightmap_t *hm, int sx, int sy)
{
	sx -= CHUNKBIAS;
	sy -= CHUNKBIAS;
	//wrap cleanly
	sx &= CHUNKLIMIT-1;
	sy &= CHUNKLIMIT-1;
	return va("maps/%s/sect_%03x_%03x.hms", hm->path, sx, sy);
}
static hmsection_t *Terr_LoadSection(heightmap_t *hm, hmsection_t *s, int sx, int sy)
{
	dsection_t *ds = NULL;
	int i;
#ifndef SERVERONLY
	dsmesh_t *dm;
	unsigned char *lm;
#endif

	/*queue the file for download if we don't have it yet*/
	if (FS_LoadFile(Terr_DiskSectionName(hm, sx, sy), (void**)&ds) < 0
#ifndef CLIENTONLY
		&& !sv.state
#endif
		)
	{
#ifndef SERVERONLY
		CL_CheckOrEnqueDownloadFile(Terr_DiskSectionName(hm, sx, sy), NULL, 0);
#endif
		return NULL;
	}

	if (ds)
	{
		if (ds->magic != SECTION_MAGIC)
			return NULL;
		if (ds->ver != SECTION_VER)
			return NULL;
	}

	if (!s)
	{
		s = malloc(sizeof(*s));
		if (!s)
		{
			FS_FreeFile(ds);
			return NULL;
		}
		memset(s, 0, sizeof(*s));
		
#ifndef SERVERONLY
		s->lightmap = -1;
#endif
	}

	s->hmmod = hm;

#ifndef SERVERONLY
	s->flags |= TSF_DIRTY;

	if (s->lightmap < 0 && qrenderer != QR_NONE)
		Terr_InitLightmap(s);
#endif

	if (ds)
	{
		s->flags = ds->flags | TSF_DIRTY;
#ifndef SERVERONLY
		Q_strncpyz(s->texname[0], ds->texname[0], sizeof(s->texname[0]));
		Q_strncpyz(s->texname[1], ds->texname[1], sizeof(s->texname[1]));
		Q_strncpyz(s->texname[2], ds->texname[2], sizeof(s->texname[2]));
		Q_strncpyz(s->texname[3], ds->texname[3], sizeof(s->texname[3]));

		CL_CheckOrEnqueDownloadFile(s->texname[0], NULL, 0);
		CL_CheckOrEnqueDownloadFile(s->texname[1], NULL, 0);
		CL_CheckOrEnqueDownloadFile(s->texname[2], NULL, 0);
		CL_CheckOrEnqueDownloadFile(s->texname[3], NULL, 0);

		if (s->lightmap >= 0)
		{
			lm = lightmap[s->lightmap]->lightmaps;
			lm += (s->lmx * HMLMSTRIDE + s->lmy) * lightmap_bytes;
			for (i = 0; i < SECTTEXSIZE; i++)
			{
				memcpy(lm, ds->texmap + i, sizeof(ds->texmap[0]));
				lm += (HMLMSTRIDE)*lightmap_bytes;
			}
			lightmap[s->lightmap]->modified = true;
			lightmap[s->lightmap]->rectchange.l = 0;
			lightmap[s->lightmap]->rectchange.t = 0;
			lightmap[s->lightmap]->rectchange.w = HMLMSTRIDE;
			lightmap[s->lightmap]->rectchange.h = HMLMSTRIDE;
		}
#endif

		/*load the heights too*/
		for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
		{
			s->heights[i] = LittleFloat(ds->heights[i]);
		}
		s->minh = ds->minh;
		s->maxh = ds->maxh;
		s->waterheight = ds->waterheight;

#ifndef SERVERONLY
		s->numents = ds->numents;
		s->maxents = s->numents;
		s->ents = malloc(sizeof(*s->ents) * s->maxents);
		if (!s->ents)
			s->numents = s->maxents = 0;
		memset(s->ents, 0, sizeof(*s->ents) * s->maxents);
		for (i = 0, dm = (dsmesh_t*)((qbyte*)ds + ds->entsofs); i < s->numents; i++, dm = (dsmesh_t*)((qbyte*)dm + dm->size))
		{
			s->ents[i].model = Mod_ForName((char*)(dm + 1), false);
			if (!s->ents[i].model || s->ents[i].model->type == mod_dummy)
			{
				s->numents--;
				i--;
				continue;
			}
			s->ents[i].scale = dm->scale;
			VectorCopy(dm->axisorg[0], s->ents[i].axis[0]);
			VectorCopy(dm->axisorg[1], s->ents[i].axis[1]);
			VectorCopy(dm->axisorg[2], s->ents[i].axis[2]);
			VectorCopy(dm->axisorg[3], s->ents[i].origin);
			s->ents[i].origin[0] += (sx-CHUNKBIAS)*hm->sectionsize;
			s->ents[i].origin[1] += (sy-CHUNKBIAS)*hm->sectionsize;
			s->ents[i].shaderRGBAf[0] = 1;
			s->ents[i].shaderRGBAf[1] = 1;
			s->ents[i].shaderRGBAf[2] = 1;
			s->ents[i].shaderRGBAf[3] = 1;
		}
#endif
		FS_FreeFile(ds);
	}
	else
	{
		s->flags |= TSF_RELIGHT;

#if 0//def DEBUG
		void *f;
		if (lightmap_bytes == 4 && lightmap_bgra && FS_LoadFile(va("maps/%s/splatt.png", hm->path), &f) >= 0)
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
				lm += (s->lmx * HMLMSTRIDE + s->lmy) * lightmap_bytes;

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

		if (lightmap_bytes == 4 && lightmap_bgra && FS_LoadFile(va("maps/%s/heightmap.png", hm->path), &f) >= 0)
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

	Terr_LoadSectionTextures(s);

	return s;
}

static void Terr_SaveSection(heightmap_t *hm, hmsection_t *s, int sx, int sy)
{
#ifndef SERVERONLY
	dsection_t ds;
	dsmesh_t dm;
	unsigned char *lm;
	vfsfile_t *f;
	int nothing = 0;
	int i;
	//if its invalid or doesn't contain all the data...
	if (!s || s->lightmap < 0)
		return;

	memset(&ds, 0, sizeof(ds));
	memset(&dm, 0, sizeof(dm));

	ds.magic = SECTION_MAGIC;
	ds.ver = SECTION_VER;
	//mask off the flags which are only valid in memory
	ds.flags = s->flags & ~(TSF_INTERNAL);

	//kill the haswater flag if its entirely above any possible water anyway.
	if (s->waterheight < s->minh)
		ds.flags &= ~TSF_HASWATER;

	Q_strncpyz(ds.texname[0], s->texname[0], sizeof(ds.texname[0]));
	Q_strncpyz(ds.texname[1], s->texname[1], sizeof(ds.texname[1]));
	Q_strncpyz(ds.texname[2], s->texname[2], sizeof(ds.texname[2]));
	Q_strncpyz(ds.texname[3], s->texname[3], sizeof(ds.texname[3]));

	lm = lightmap[s->lightmap]->lightmaps;
	lm += (s->lmx * HMLMSTRIDE + s->lmy) * lightmap_bytes;
	for (i = 0; i < SECTTEXSIZE; i++)
	{
		memcpy(ds.texmap + i, lm, sizeof(ds.texmap[0]));
		lm += (HMLMSTRIDE)*lightmap_bytes;
	}

	for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
	{
		ds.heights[i] = LittleFloat(s->heights[i]);
	}
	ds.waterheight = s->waterheight;
	ds.holes = s->holes;
	ds.minh = s->minh;
	ds.maxh = s->maxh;
	ds.numents = s->numents;
	ds.entsofs = sizeof(ds);

	f = FS_OpenVFS(Terr_DiskSectionName(hm, sx, sy), "wb", FS_GAMEONLY);
	VFS_WRITE(f, &ds, sizeof(ds));
	for (i = 0; i < s->numents; i++)
	{
		int pad;
		dm.scale = s->ents[i].scale;
		VectorCopy(s->ents[i].axis[0], dm.axisorg[0]);
		VectorCopy(s->ents[i].axis[1], dm.axisorg[1]);
		VectorCopy(s->ents[i].axis[2], dm.axisorg[2]);
		VectorCopy(s->ents[i].origin, dm.axisorg[3]);
		dm.axisorg[3][0] += (CHUNKBIAS-sx)*hm->sectionsize;
		dm.axisorg[3][1] += (CHUNKBIAS-sy)*hm->sectionsize;
		dm.size = sizeof(dm) + strlen(s->ents[i].model->name) + 1;
		if (dm.size & 3)
			pad = 4 - (dm.size&3);
		else
			pad = 0;
		dm.size += pad;
		VFS_WRITE(f, &dm, sizeof(dm));
		VFS_WRITE(f, s->ents[i].model->name, strlen(s->ents[i].model->name)+1);
		if (pad)
			VFS_WRITE(f, &nothing, pad);
	}
	VFS_CLOSE(f);
#endif
}

/*convienience function*/
static hmsection_t *Terr_GetSection(heightmap_t *hm, int x, int y, qboolean doload)
{
	hmcluster_t *cluster;
	hmsection_t *section;
	int cx = x / MAXSECTIONS;
	int cy = y / MAXSECTIONS;
	int sx = x & (MAXSECTIONS-1);
	int sy = y & (MAXSECTIONS-1);
	cluster = hm->cluster[cx + cy*MAXSECTIONS];
	if (!cluster)
	{
		if (doload)
		{
			cluster = malloc(sizeof(*cluster));
			if (!cluster)
				return NULL;
			memset(cluster, 0, sizeof(*cluster));
			hm->cluster[cx + cy*MAXSECTIONS] = cluster;
		}
		else
			return NULL;
	}
	section = cluster->section[sx + sy*MAXSECTIONS];
	if (!section)
	{
		if (doload)
		{
			section = cluster->section[sx + sy*MAXSECTIONS] = Terr_LoadSection(hm, section, x, y);
		}
	}
	return section;
}

/*save all currently loaded sections*/
int HeightMap_Save(heightmap_t *hm)
{
	hmsection_t *s;
	int x, y;
	int sectionssaved = 0;
	for (x = hm->firstsegx; x < hm->maxsegx; x++)
	{
		for (y = hm->firstsegy; y < hm->maxsegy; y++)
		{
			s = Terr_GetSection(hm, x, y, false);
			if (!s)
				continue;
			if (s->flags & TSF_EDITED)
			{
				s->flags &= ~TSF_EDITED;
				Terr_SaveSection(hm, s, x, y);
				sectionssaved++;
			}
		}
	}

	return sectionssaved;
}

void Terr_DestroySection(heightmap_t *hm, hmsection_t *s)
{
#ifndef SERVERONLY
	if (hm && s->lightmap >= 0)
	{
		struct lmsect_s *lms;

		lms = malloc(sizeof(*lms));
		lms->lm = s->lightmap;
		lms->x = s->lmx;
		lms->y = s->lmy;
		lms->next = hm->unusedlmsects;
		hm->unusedlmsects = lms;
	}

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		qglDeleteBuffersARB(1, &s->vbo.coord.gl.vbo);
		qglDeleteBuffersARB(1, &s->vbo.indicies.gl.vbo);
	}
#endif

	free(s->mesh.xyz_array);
	free(s->mesh.indexes);
#endif

	free(s);
}

/*purge all sections
lightmaps only are purged whenever the client rudely kills lightmaps
we'll reload those when its next seen.
(lightmaps will already have been destroyed, so no poking them)
*/
void Terr_PurgeTerrainModel(model_t *mod, qboolean lightmapsonly)
{
	heightmap_t *hm = mod->terrain;
	hmcluster_t *c;
	hmsection_t *s;
	int cx, cy;
	int sx, sy;
	for (cy = 0; cy < MAXSECTIONS; cy++)
	for (cx = 0; cx < MAXSECTIONS; cx++)
	{
		c = hm->cluster[cx + cy*MAXSECTIONS];
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
#ifndef SERVERONLY
				s->lightmap = -1;
#endif
			}
			else
			{
				c->section[sx+sy*MAXSECTIONS] = NULL;

				Terr_DestroySection(NULL, s);
			}
		}
		if (!lightmapsonly)
		{
			hm->cluster[cx + cy*MAXSECTIONS] = NULL;
			free(c);
		}
	}
#ifndef SERVERONLY
	while (hm->unusedlmsects)
	{
		struct lmsect_s *lms;
		lms = hm->unusedlmsects;
		hm->unusedlmsects = lms->next;
		free(lms);
	}
#endif
}
#ifndef SERVERONLY
void Terr_DrawTerrainWater(float *mins, float *maxs, float waterz, float r, float g, float b, float a)
{
	scenetris_t *t;
	shader_t *s = R_RegisterCustom ("warp/terrain", Shader_DefaultBSPQ2, NULL);

	if (!TEXVALID(s->defaulttextures.base))
		s->defaulttextures.base = R_LoadHiResTexture("terwater", NULL, IF_NOALPHA);
	if (!TEXVALID(s->defaulttextures.bump))
		s->defaulttextures.bump = R_LoadBumpmapTexture("terwater_bump", NULL);
	if (!TEXVALID(s->defaulttextures.bump))
	{
		unsigned char dat[64*64] = {0};
		int i;
		for (i = 0; i < 64*64; i++)
			dat[i] = rand()&15;
		s->defaulttextures.bump = R_LoadTexture8BumpPal("terwater_bump", 64, 64, dat, 0);
	}


	if (cl_numstris && cl_stris[cl_numstris-1].shader == s)
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
		t->shader = s;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
		t->numvert = 0;
		t->numidx = 0;
	}

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
		VectorSet(cl_strisvertv[cl_numstrisvert], mins[0], mins[1], waterz);
		Vector4Set(cl_strisvertc[cl_numstrisvert], r, g, b, a);
		Vector2Set(cl_strisvertt[cl_numstrisvert], mins[0]/64, mins[1]/64);
		cl_numstrisvert++;

		VectorSet(cl_strisvertv[cl_numstrisvert], mins[0], maxs[1], waterz);
		Vector4Set(cl_strisvertc[cl_numstrisvert], r, g, b, a);
		Vector2Set(cl_strisvertt[cl_numstrisvert], mins[0]/64, maxs[1]/64);
		cl_numstrisvert++;

		VectorSet(cl_strisvertv[cl_numstrisvert], maxs[0], maxs[1], waterz);
		Vector4Set(cl_strisvertc[cl_numstrisvert], r, g, b, a);
		Vector2Set(cl_strisvertt[cl_numstrisvert], maxs[0]/64, maxs[1]/64);
		cl_numstrisvert++;

		VectorSet(cl_strisvertv[cl_numstrisvert], maxs[0], mins[1], waterz);
		Vector4Set(cl_strisvertc[cl_numstrisvert], r, g, b, a);
		Vector2Set(cl_strisvertt[cl_numstrisvert], maxs[0]/64, mins[1]/64);
		cl_numstrisvert++;
	}




	/*build the triangles*/
	cl_strisidx[cl_numstrisidx++] = t->numvert + 0;
	cl_strisidx[cl_numstrisidx++] = t->numvert + 1;
	cl_strisidx[cl_numstrisidx++] = t->numvert + 2;

	cl_strisidx[cl_numstrisidx++] = t->numvert + 0;
	cl_strisidx[cl_numstrisidx++] = t->numvert + 2;
	cl_strisidx[cl_numstrisidx++] = t->numvert + 3;

	cl_strisidx[cl_numstrisidx++] = t->numvert + 3;
	cl_strisidx[cl_numstrisidx++] = t->numvert + 2;
	cl_strisidx[cl_numstrisidx++] = t->numvert + 1;

	cl_strisidx[cl_numstrisidx++] = t->numvert + 3;
	cl_strisidx[cl_numstrisidx++] = t->numvert + 1;
	cl_strisidx[cl_numstrisidx++] = t->numvert + 0;


	t->numidx = cl_numstrisidx - t->firstidx;
	t->numvert += 4;
}

void Terr_RebuildMesh(hmsection_t *s, int x, int y)
{
	int vx, vy;
	int v;
	mesh_t *mesh = &s->mesh;
	heightmap_t *hm = s->hmmod;

	if (s->lightmap < 0)
	{
		Terr_InitLightmap(s);
	}

	s->minh = 9999999999999999.f;
	s->maxh = -9999999999999999.f;

	if (!mesh->xyz_array)
	{
		mesh->xyz_array = BZ_Malloc((sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)) * (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
		mesh->st_array = (void*) (mesh->xyz_array + (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
		mesh->lmst_array[0] = (void*) (mesh->st_array + (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
	}
	mesh->numvertexes = 0;
	/*64 quads across requires 65 verticies*/
	for (vx = 0; vx < SECTHEIGHTSIZE; vx++)
	{
		for (vy = 0; vy < SECTHEIGHTSIZE; vy++)
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
			mesh->lmst_array[0][v][0] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmy) / HMLMSTRIDE);
			mesh->lmst_array[0][v][1] += ((float)SECTTEXSIZE/(HMLMSTRIDE*2)) + ((float)(s->lmx) / HMLMSTRIDE);

			//TODO: include colour tints
		}
	}

	if (!mesh->indexes)
		mesh->indexes = BZ_Malloc(sizeof(index_t) * SECTHEIGHTSIZE*SECTHEIGHTSIZE*6);

	mesh->numindexes = 0;
	for (vx = 0; vx < SECTHEIGHTSIZE-1; vx++)
	{
		for (vy = 0; vy < SECTHEIGHTSIZE-1; vy++)
		{
			float d1,d2;
#if SECTHEIGHTSIZE >= 4
			int holebit;

			//skip generation of the mesh above holes
			holebit = (vy / (SECTHEIGHTSIZE/4))+(vx / (SECTHEIGHTSIZE/4))*4;
			holebit = 1u<<holebit;
			if (s->holes & holebit)
				continue;
#endif
			v = vx + vy*(SECTHEIGHTSIZE);

			d1 = fabs(mesh->xyz_array[v][2] - mesh->xyz_array[v+1+SECTHEIGHTSIZE][2]);
			d2 = fabs(mesh->xyz_array[v+1][2] - mesh->xyz_array[v+SECTHEIGHTSIZE][2]);
#if 1
			if (d1 < d2)
			{
				mesh->indexes[mesh->numindexes++] = v+0;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+1+SECTHEIGHTSIZE;
				mesh->indexes[mesh->numindexes++] = v+0;
				mesh->indexes[mesh->numindexes++] = v+1+SECTHEIGHTSIZE;
				mesh->indexes[mesh->numindexes++] = v+SECTHEIGHTSIZE;
			}
#endif
			else
			{
				mesh->indexes[mesh->numindexes++] = v+0;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+SECTHEIGHTSIZE;
				mesh->indexes[mesh->numindexes++] = v+1;
				mesh->indexes[mesh->numindexes++] = v+1+SECTHEIGHTSIZE;
				mesh->indexes[mesh->numindexes++] = v+SECTHEIGHTSIZE;
			}
		}
	}

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		if (!s->vbo.coord.gl.vbo)
			qglGenBuffersARB(1, &s->vbo.coord.gl.vbo);
		s->vbo.coord.gl.addr = 0;
		GL_SelectVBO(s->vbo.coord.gl.vbo);
		qglBufferDataARB(GL_ARRAY_BUFFER_ARB, (sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)) * (mesh->numvertexes), mesh->xyz_array, GL_STATIC_DRAW_ARB);
		GL_SelectVBO(0);
		s->vbo.texcoord.gl.addr = (void*)((char*)mesh->st_array - (char*)mesh->xyz_array);
		s->vbo.texcoord.gl.vbo = s->vbo.coord.gl.vbo;
		s->vbo.lmcoord[0].gl.addr = (void*)((char*)mesh->lmst_array[0] - (char*)mesh->xyz_array);
		s->vbo.lmcoord[0].gl.vbo = s->vbo.coord.gl.vbo;
//		Z_Free(mesh->xyz_array);
//		mesh->xyz_array = NULL;
//		mesh->st_array = NULL;
//		mesh->lmst_array = NULL;

		if (!s->vbo.indicies.gl.vbo)
			qglGenBuffersARB(1, &s->vbo.indicies.gl.vbo);
		s->vbo.indicies.gl.addr = 0;
		GL_SelectEBO(s->vbo.indicies.gl.vbo);
		qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(index_t) * mesh->numindexes, mesh->indexes, GL_STATIC_DRAW_ARB);
		GL_SelectEBO(0);
//		Z_Free(mesh->indexes);
//		mesh->indexes = NULL;
	}
#endif
}

void Terr_DrawTerrainModel (batch_t **batches, entity_t *e)
{
	//a 512*512 heightmap
	//will draw 2 tris per square, drawn twice for detail
	//so a million triangles per frame if the whole thing is visible.

	//with 130 to 180fps, display lists rule!
	int x, y, i;
	vec3_t mins, maxs;
	model_t *m = e->model;
	heightmap_t *hm = m->terrain;
	mesh_t *mesh;
	batch_t *b;
	hmsection_t *s;
	int bounds[4];
	
	if (hm->relight)
		ted_dorelight(hm);

	if (e->model == cl.worldmodel)
	{
		b = BE_GetTempBatch();
		if (b)
		{
			b->lightmap[0] = -1;
			b->lightmap[1] = -1;
			b->lightmap[2] = -1;
			b->lightmap[3] = -1;
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


	if (r_refdef.gfog_rgbd[3] || gl_maxdist.value>0)
	{
		float culldist;
		extern cvar_t r_fog_exp2;

		if (r_refdef.gfog_rgbd[3])
		{
			//figure out the eyespace distance required to reach that fog value
			culldist = log(0.5/255.0f);
			if (r_fog_exp2.ival)
				culldist = sqrt(culldist / (-r_refdef.gfog_rgbd[3] * r_refdef.gfog_rgbd[3]));
			else
				culldist = culldist / (-r_refdef.gfog_rgbd[3]);
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

	for (x = bounds[0]; x < bounds[1]; x++)
	{
		mins[0] = (x+0 - CHUNKBIAS)*hm->sectionsize;
		maxs[0] = (x+1 - CHUNKBIAS)*hm->sectionsize;
		for (y = bounds[2]; y < bounds[3]; y++)
		{
			mins[1] = (y+0 - CHUNKBIAS)*hm->sectionsize;
			maxs[1] = (y+1 - CHUNKBIAS)*hm->sectionsize;

			s = Terr_GetSection(hm, x, y, true);
			if (!s)
				continue;
			if (s->lightmap < 0)
				Terr_LoadSection(hm, s, x, y);

			if (s->flags & TSF_RELIGHT)
			{
				if (!hm->relight)
				{
					s->flags &= ~TSF_RELIGHT;
					hm->relight = s;
					hm->relightidx = 0;
					hm->relightmin[0] = mins[0];
					hm->relightmin[1] = mins[1];
				}
			}

			mesh = &s->mesh;
			if (s->flags & TSF_DIRTY)
			{
				s->flags &= ~TSF_DIRTY;

				Terr_RebuildMesh(s, x, y);
			}

			//chuck out any batches for models in this section
			for (i = 0; i < s->numents; i++)
			{
				if (s->ents[i].model && s->ents[i].model->type == mod_alias)
				{
					R_GAlias_GenerateBatches(&s->ents[i], batches);
				}
			}

			if (s->flags & TSF_HASWATER)
			{
				mins[2] = s->waterheight;
				maxs[2] = s->waterheight;
				if (!R_CullBox(mins, maxs))
				{
					Terr_DrawTerrainWater(mins, maxs, s->waterheight, 1, 1, 1, 1);
				}
			}

			mins[2] = s->minh;
			maxs[2] = s->maxh;

//			if (!BoundsIntersect(mins, maxs, r_refdef.vieworg, r_refdef.vieworg))
				if (R_CullBox(mins, maxs))
					continue;

			b = BE_GetTempBatch();
			if (!b)
				continue;
			b->ent = e;
			b->shader = hm->shader;
			b->flags = 0;
			b->mesh = &s->amesh;
			b->mesh[0] = mesh;
			b->meshes = 1;
			b->buildmeshes = NULL;
			b->skin = &s->textures;
			b->texture = NULL;
			b->vbo = &s->vbo;
			b->lightmap[0] = s->lightmap;
			b->lightmap[1] = -1;
			b->lightmap[2] = -1;
			b->lightmap[3] = -1;

			b->next = batches[b->shader->sort];
			batches[b->shader->sort] = b;
		}
	}
}
#endif

unsigned int Heightmap_PointContentsHM(heightmap_t *hm, float clipmipsz, vec3_t org)
{
	float x, y;
	float z, tz;
	int sx, sy;
	unsigned int holebit;
	hmsection_t *s;
	const float wbias = CHUNKBIAS * hm->sectionsize;

	sx = (org[0]+wbias)/hm->sectionsize;
	sy = (org[1]+wbias)/hm->sectionsize;
	if (sx < hm->firstsegx || sy < hm->firstsegy)
		return hm->exteriorcontents;
	if (sx >= hm->maxsegx || sy >= hm->maxsegy)
		return hm->exteriorcontents;
	s = Terr_GetSection(hm, sx, sy, true);
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

	holebit = sx*4/SECTHEIGHTSIZE + (sy*4/SECTHEIGHTSIZE)*4;
	if (s->holes & (1u<<holebit))
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
	if (s->flags & TSF_HASWATER)
		if (z < s->waterheight)
			return FTECONTENTS_WATER;
	return FTECONTENTS_EMPTY;
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
	s = Terr_GetSection(hm, sx, sy, true);
	if (!s)
		return;

	x = (org[0]+wbias - (sx*hm->sectionsize))*(SECTHEIGHTSIZE-1)/hm->sectionsize;
	y = (org[1]+wbias - (sy*hm->sectionsize))*(SECTHEIGHTSIZE-1)/hm->sectionsize;
/*
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x > hm->maxsegx-1)
		x = hm->maxsegx-1;
	if (y > hm->maxsegy-1)
		y = hm->maxsegy-1;
*/
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
	float frac;
	float htilesize;
	heightmap_t *hm;
	int contents;
} hmtrace_t;

//sx,sy are the tile coord
//note that tile SECTHEIGHTSIZE-1 does not exist, as the last sample overlaps the first sample of the next section
void Heightmap_Trace_Square(hmtrace_t *tr, int tx, int ty)
{
	vec3_t d[2];
	vec3_t p[4];
	vec4_t n[5];
	int t, i;

	qboolean startout, endout;
	float *enterplane;
	float enterfrac, exitfrac, nearfrac=0;
	float enterdist=0;
	float dist, d1, d2, f;
	int sx, sy;
	hmsection_t *s;

	if (tx < 0 || tx >= CHUNKLIMIT*(SECTHEIGHTSIZE-1))
		return;
	if (ty < 0 || ty >= CHUNKLIMIT*(SECTHEIGHTSIZE-1))
		return;
	s = Terr_GetSection(tr->hm, tx/(SECTHEIGHTSIZE-1), ty/(SECTHEIGHTSIZE-1), true);

	sx = tx;
	sy = ty;

	tx = tx % (SECTHEIGHTSIZE-1);
	ty = ty % (SECTHEIGHTSIZE-1);

	VectorSet(p[0], tr->htilesize*(sx+0), tr->htilesize*(sy+0), s->heights[(tx+0)+(ty+0)*SECTHEIGHTSIZE]);
	VectorSet(p[1], tr->htilesize*(sx+1), tr->htilesize*(sy+0), s->heights[(tx+1)+(ty+0)*SECTHEIGHTSIZE]);
	VectorSet(p[2], tr->htilesize*(sx+0), tr->htilesize*(sy+1), s->heights[(tx+0)+(ty+1)*SECTHEIGHTSIZE]);
	VectorSet(p[3], tr->htilesize*(sx+1), tr->htilesize*(sy+1), s->heights[(tx+1)+(ty+1)*SECTHEIGHTSIZE]);
//	DebugDrawQuadH(p[0][0], p[0][1], p[3][0], p[3][1], p[0][2], 0, 0, 0.2);

	for (t = 0; t < 2; t++)
	{
		/*generate the brush (in world space*/
		if (t == 0)
		{
//			continue;
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
			Vector4Set(n[4], 0, 0, -1, 0);
			n[4][3] = DotProduct(n[3], p[1]);
		}
		else
		{
//			continue;
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
			Vector4Set(n[4], 0, 0, -1, 0);
			n[4][3] = DotProduct(n[3], p[1]);
		}

		startout = false;
		endout = false;
		enterplane= NULL;
		enterfrac = -1;
		exitfrac = 10;
		for (i = 0; i < 4; i++)
		{
			/*calculate the distance based upon the shape of the object we're tracing for*/
			dist = n[i][3];


			d1 = DotProduct (tr->start, n[i]) - dist;
			d2 = DotProduct (tr->end, n[i]) - dist;

			if (d1 > 0)
				startout = true;
			if (d2 > 0)
				endout = true;

			//if we're fully outside any plane, then we cannot possibly enter the brush, skip to the next one
			if (d1 > 0 && d2 >= 0)
				goto nextbrush;

			//if we're fully inside the plane, then whatever is happening is not relevent for this plane
			if (d1 < 0 && d2 <= 0)
				continue;

			f = (d1) / (d1-d2);
			if (d1 > d2)
			{
				//entered the brush. favour the furthest fraction to avoid extended edges (yay for convex shapes)
				if (enterfrac < f)
				{
					enterfrac = f;
					nearfrac = (d1 - (0.03125)) / (d1-d2);
					enterplane = n[i];
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
nextbrush:
		;
	}
#if 0
	float normf = 0.70710678118654752440084436210485;
	float pd, sd, ed, bd;
	int tris, x, y;
	vec3_t closest;
	vec3_t point;


	pd = normf*(x+y);
	sd = normf*tr->start[0]+normf*tr->start[1];
	ed = normf*tr->end[0]+normf*tr->end[1];
	bd = normf*tr->maxs[0]+normf*tr->maxs[1];	//assume mins is this but negative
//see which of the two triangles in the square it travels over.

	tris = 0;
	if (sd<=pd || ed<=pd)
		tris |= 1;
	if (sd>=pd || ed>=pd)
		tris |= 2;

	point[0] = sx+1;
	point[1] = sy;
	point[2] = s->heights[sx+1+sy*tr->hm->terrainsize];

	if (tris & 1)
	{	//triangle with 0, 0
		vec3_t norm;
		float d1, d2, dc;

		x = tr->hm->heights[(sx+1)+(sy+0)*tr->hm->terrainsize] - tr->hm->heights[(sx+0)+(sy+0)*tr->hm->terrainsize];
		y = tr->hm->heights[(sx+0)+(sy+1)*tr->hm->terrainsize] - tr->hm->heights[(sx+0)+(sy+0)*tr->hm->terrainsize];

		norm[0] = (-x)/tr->hm->terrainscale;
		norm[1] = (-y)/tr->hm->terrainscale;
		norm[2] = 1.0f/(float)sqrt(norm[0]*norm[0] + norm[1]*norm[1] + 1);
		Closest(closest, norm, tr->mins, tr->maxs);
		dc = DotProduct(norm, closest) - DotProduct(norm, point);
		d1 = DotProduct(norm, tr->start) + dc;
		d2 = DotProduct(norm, tr->end) + dc;

		if (d1>=0 && d2<=0)
		{	//intersects
			tr->contents = FTECONTENTS_SOLID;

			d1 = (d1-d2)/(d1+d2);
			d2 = 1-d1;

			tr->impact[0] = tr->end[0]*d1+tr->start[0]*d2;
			tr->impact[1] = tr->end[1]*d1+tr->start[1]*d2;
			tr->impact[2] = tr->end[2]*d1+tr->start[2]*d2;
		}
	}
	if (tris & 2)
	{	//triangle with 1, 1
		vec3_t norm;
		float d1, d2, dc;
		norm[0] = (-x)/tr->hm->terrainscale;
		norm[1] = (-y)/tr->hm->terrainscale;
		norm[2] = 1.0f/(float)sqrt(norm[0]*norm[0] + norm[1]*norm[1] + 1);
		Closest(closest, norm, tr->mins, tr->maxs);
		dc = DotProduct(norm, closest) - DotProduct(norm, point);
		d1 = DotProduct(norm, tr->start) + dc;
		d2 = DotProduct(norm, tr->end) + dc;

		if (d1>=0 && d2<=0)
		{	//intersects
			tr->contents = FTECONTENTS_SOLID;

			d1 = (d1-d2)/(d1+d2);
			d2 = 1-d1;

			tr->impact[0] = tr->end[0]*d1+tr->start[0]*d2;
			tr->impact[1] = tr->end[1]*d1+tr->start[1]*d2;
			tr->impact[2] = tr->end[2]*d1+tr->start[2]*d2;
		}
	}
#endif
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
qboolean Heightmap_Trace(struct model_s *model, int hulloverride, int frame, vec3_t mataxis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, unsigned int against, struct trace_s *trace)
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
	hmtrace_t hmtrace;
	hmtrace.hm = model->terrain;
	hmtrace.htilesize = hmtrace.hm->sectionsize / (SECTHEIGHTSIZE-1);
	hmtrace.frac = 1;
	hmtrace.contents = 0;

	hmtrace.plane[0] = 0;
	hmtrace.plane[1] = 0;
	hmtrace.plane[2] = 1;
	hmtrace.plane[3] = 0;

	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;

	//to tile space
	hmtrace.start[0] = (start[0] + CHUNKBIAS*hmtrace.hm->sectionsize);
	hmtrace.start[1] = (start[1] + CHUNKBIAS*hmtrace.hm->sectionsize);
	hmtrace.start[2] = (start[2] + mins[2]);
	hmtrace.end[0] = (end[0] + CHUNKBIAS*hmtrace.hm->sectionsize);
	hmtrace.end[1] = (end[1] + CHUNKBIAS*hmtrace.hm->sectionsize);
	hmtrace.end[2] = (end[2] + mins[2]);

	dir[0] = (hmtrace.end[0] - hmtrace.start[0])/hmtrace.htilesize;
	dir[1] = (hmtrace.end[1] - hmtrace.start[1])/hmtrace.htilesize;
	pos[0] = hmtrace.start[0]/hmtrace.htilesize;
	pos[1] = hmtrace.start[1]/hmtrace.htilesize;

	emins[0] = (mins[0]-1)/hmtrace.htilesize;
	emins[1] = (mins[1]-1)/hmtrace.htilesize;
	emaxs[0] = (maxs[0]+1)/hmtrace.htilesize;
	emaxs[1] = (maxs[1]+1)/hmtrace.htilesize;

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
				frac[axis] = (npos[axis]*hmtrace.htilesize - hmtrace.start[axis])/(hmtrace.end[axis]-hmtrace.start[axis]);
			}
			else if (dir[axis] < 0)
			{
				npos[axis] = pos[axis];
				nudge[axis] = (float)(int)pos[axis] == pos[axis];
				npos[axis] = (int)npos[axis];
				frac[axis] = (npos[axis]*hmtrace.htilesize - hmtrace.start[axis])/(hmtrace.end[axis]-hmtrace.start[axis]);
				npos[axis] -= nudge[axis];
			}
			else
				frac[axis] = 1000000000000000;
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
		pos[!axis] = ((hmtrace.end[!axis] * frac[axis]) + (hmtrace.start[!axis] * (1-frac[axis])))/hmtrace.htilesize;
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

unsigned int Heightmap_FatPVS		(model_t *mod, vec3_t org, qbyte *pvsbuffer, unsigned int pvssize, qboolean add)
{
	return 0;
}

#ifndef CLIENTONLY
qboolean Heightmap_EdictInFatPVS	(model_t *mod, struct pvscache_s *edict, qbyte *pvsdata)
{
	return true;
}

void Heightmap_FindTouchedLeafs	(model_t *mod, pvscache_t *ent, float *mins, float *maxs)
{
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

qbyte *Heightmap_LeafnumPVS	(model_t *model, int num, qbyte *buffer, unsigned int buffersize)
{
	static qbyte heightmappvs = 255;
	return &heightmappvs;
}
int	Heightmap_LeafForPoint	(model_t *model, vec3_t point)
{
	return 0;
}

#ifndef SERVERONLY
static unsigned char *ted_getlightmap(hmsection_t *s, int idx)
{
	unsigned char *lm;
	int x = idx % SECTTEXSIZE, y = idx / SECTTEXSIZE;
	if (s->lightmap < 0)
	{
		Terr_LoadSection(s->hmmod, s, x, y);
		if (s->lightmap < 0)
			Terr_InitLightmap(s);
	}

	s->flags |= TSF_EDITED;

	lightmap[s->lightmap]->modified = true;
	lightmap[s->lightmap]->rectchange.l = 0;
	lightmap[s->lightmap]->rectchange.t = 0;
	lightmap[s->lightmap]->rectchange.w = HMLMSTRIDE;
	lightmap[s->lightmap]->rectchange.h = HMLMSTRIDE;
	lm = lightmap[s->lightmap]->lightmaps;
	lm += ((s->lmx+y) * HMLMSTRIDE + (s->lmy+x)) * lightmap_bytes;
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

	hm->relight = NULL;

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
		lm[3] = d*255;
	}
}
static void ted_sethole(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned int bit;
	unsigned int mask;
	mask = 1u<<idx;
	if (*(float*)ctx)
		bit = mask;
	else
		bit = 0;
	s->flags |= TSF_DIRTY|TSF_EDITED;
	s->holes = (s->holes & ~mask) | bit;
}
static void ted_heighttally(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	/*raise the terrain*/
	((float*)ctx)[0] += s->heights[idx]*w;
	((float*)ctx)[1] += w;
}
static void ted_heightsmooth(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	s->flags |= TSF_DIRTY|TSF_EDITED|TSF_RELIGHT;
	/*interpolate the terrain towards a certain value*/

	if (IS_NAN(s->heights[idx]))
		s->heights[idx] = *(float*)ctx;
	else
		s->heights[idx] = s->heights[idx]*(1-w) + w**(float*)ctx;
}
static void ted_heightraise(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength)
{
	s->flags |= TSF_DIRTY|TSF_EDITED|TSF_RELIGHT;
	/*raise the terrain*/
	s->heights[idx] += strength;
}
static void ted_heightset(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength)
{
	s->flags |= TSF_DIRTY|TSF_EDITED|TSF_RELIGHT;
	/*set the terrain to a specific value*/
	s->heights[idx] = *(float*)ctx;
}

static void ted_mixconcentrate(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
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

static void ted_mixnoise(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	vec4_t v;
	float sc;
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

static void ted_mixpaint(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	char *texname = ctx;
	int t;
	vec3_t newval;
	if (w > 1)
		w = 1;

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

/*
static void ted_mixlight(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
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
static void ted_mixset(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	if (w > 1)
		w = 1;
	lm[2] = lm[2]*(1-w) + (255*((float*)ctx)[0]*(w));
	lm[1] = lm[1]*(1-w) + (255*((float*)ctx)[1]*(w));
	lm[0] = lm[0]*(1-w) + (255*((float*)ctx)[2]*(w));
}

static void ted_mixtally(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
	((float*)ctx)[0] += lm[0]*w;
	((float*)ctx)[1] += lm[1]*w;
	((float*)ctx)[2] += lm[2]*w;
	((float*)ctx)[3] += w;
}


//calls 'func' for each tile upon the terrain. the 'tile' can be either height or texel
static void ted_itterate(heightmap_t *hm, float *pos, float radius, float strength, int steps, void(*func)(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength), void *ctx)
{
	int tx, ty;
	float wx, wy;
	float sc[2];
	int min[2], max[2];
	int sx,sy;
	hmsection_t *s;
	float w, xd, yd;

	min[0] = floor((pos[0] - radius)/(hm->sectionsize) - 1);
	min[1] = floor((pos[1] - radius)/(hm->sectionsize) - 1);
	max[0] = ceil((pos[0] + radius)/(hm->sectionsize) + 1);
	max[1] = ceil((pos[1] + radius)/(hm->sectionsize) + 1);

	min[0] = bound(hm->firstsegx, min[0], hm->maxsegx);
	min[1] = bound(hm->firstsegx, min[1], hm->maxsegy);
	max[0] = bound(hm->firstsegx, max[0], hm->maxsegx);
	max[1] = bound(hm->firstsegx, max[1], hm->maxsegy);

	sc[0] = hm->sectionsize/(steps-1);
	sc[1] = hm->sectionsize/(steps-1);

	for (sx = min[0]; sx < max[0]; sx++)
	{
		for (sy = min[1]; sy < max[1]; sy++)
		{
			s = Terr_GetSection(hm, sx, sy, true);
			if (!s)
				continue;

			for (tx = 0; tx < steps; tx++)
			{
				for (ty = 0; ty < steps; ty++)
				{
					/*both heights and textures have an overlapping/matching sample at the edge, there's no need for any half-pixels or anything here*/
					wx = (sx*(steps-1.0) + tx)*sc[0];
					wy = (sy*(steps-1.0) + ty)*sc[1];
					xd = wx - pos[0];
					yd = wy - pos[1];
					w = sqrt(radius*radius - (xd*xd+yd*yd));
					if (w > 0)
					{
						func(ctx, s, tx+ty*steps, wx, wy, w*strength/(radius));
					}
				}
			}
		}
	}
}

//Heightmap_NativeBoxContents
enum
{
	ter_reload,
	ter_save,
	ter_sethole,
	ter_height_set,
	ter_height_smooth,
	ter_height_spread,
	ter_raise,
	ter_lower,
	ter_tex_kill,
	ter_tex_get,
	ter_mixpaint,
	ter_mixconcentrate,
	ter_mixnoise,
	ter_mixblur,
	ter_water_set,
	ter_mesh_add,
	ter_mesh_kill
};
void QCBUILTIN PF_terrain_edit(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *vmw = prinst->parms->user;
	int action = G_FLOAT(OFS_PARM0);
	vec3_t pos;// G_VECTOR(OFS_PARM1);
	float radius = G_FLOAT(OFS_PARM2);
	float quant = G_FLOAT(OFS_PARM3);
//	G_FLOAT(OFS_RETURN) = Heightmap_Edit(w->worldmodel, action, pos, radius, quant);

	model_t *mod = vmw->worldmodel;
	heightmap_t *hm;
	vec4_t tally;

	G_FLOAT(OFS_RETURN) = 0;

	if (!mod || !mod->terrain)
		return;
	hm = mod->terrain;

	pos[0] = G_FLOAT(OFS_PARM1+0) + hm->sectionsize * CHUNKBIAS;
	pos[1] = G_FLOAT(OFS_PARM1+1) + hm->sectionsize * CHUNKBIAS;
	pos[2] = G_FLOAT(OFS_PARM1+2);

	switch(action)
	{
	case ter_reload:
		Terr_PurgeTerrainModel(mod, false);
		break;
	case ter_save:
		Con_Printf("%i sections saved\n", HeightMap_Save(hm));
		break;
	case ter_sethole:
		{
			int x, y;
			hmsection_t *s;
			x = pos[0]*4 / hm->sectionsize;
			y = pos[1]*4 / hm->sectionsize;
			x = bound(hm->firstsegx*4, x, hm->maxsegy*4-1);
			y = bound(hm->firstsegy*4, y, hm->maxsegy*4-1);
		
			s = Terr_GetSection(hm, x/4, y/4, true);
			if (!s)
				return;
			ted_sethole(&quant, s, (x&3) + (y&3)*4, x/4, y/4, 0);
		}
		break;
	case ter_height_set:
		ted_itterate(hm, pos, radius, 1, SECTHEIGHTSIZE, ted_heightset, &quant);
		break;
	case ter_height_smooth:
		tally[0] = 0;
		tally[1] = 0;
		ted_itterate(hm, pos, radius, 1, SECTHEIGHTSIZE, ted_heighttally, &tally);
		tally[0] /= tally[1];
		if (IS_NAN(tally[0]))
			tally[0] = 0;
		ted_itterate(hm, pos, radius, 1, SECTHEIGHTSIZE, ted_heightsmooth, &tally);
		break;
	case ter_height_spread:
		tally[0] = 0;
		tally[1] = 0;
		ted_itterate(hm, pos, radius/2, 1, SECTHEIGHTSIZE, ted_heighttally, &tally);
		tally[0] /= tally[1];
		if (IS_NAN(tally[0]))
			tally[0] = 0;
		ted_itterate(hm, pos, radius, 1, SECTHEIGHTSIZE, ted_heightsmooth, &tally);
		break;
	case ter_water_set:
		{
			int x, y;
			hmsection_t *s;
			x = pos[0] / hm->sectionsize;
			y = pos[1] / hm->sectionsize;
			x = bound(hm->firstsegx, x, hm->maxsegy-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);
		
			s = Terr_GetSection(hm, x, y, true);
			if (!s)
				return;
			s->flags |= TSF_HASWATER|TSF_EDITED;
			s->waterheight = quant;
		}
		break;
	case ter_lower:
		quant *= -1;
	case ter_raise:
		ted_itterate(hm, pos, radius, quant, SECTHEIGHTSIZE, ted_heightraise, &quant);
		break;
//	case ter_mixset:
//		ted_itterate(hm, pos, radius, 1, SECTTEXSIZE, ted_mixset, G_VECTOR(OFS_PARM4));
//		break;
	case ter_mixpaint:
		ted_itterate(hm, pos, radius, quant/10, SECTTEXSIZE, ted_mixpaint, PR_GetStringOfs(prinst, OFS_PARM4));
		break;
	case ter_mixconcentrate:
		ted_itterate(hm, pos, radius, 1, SECTTEXSIZE, ted_mixconcentrate, NULL);
		break;
	case ter_mixnoise:
		ted_itterate(hm, pos, radius, 1, SECTTEXSIZE, ted_mixnoise, NULL);
		break;
	case ter_mixblur:
		Vector4Set(tally, 0, 0, 0, 0);
		ted_itterate(hm, pos, radius, 1, SECTTEXSIZE, ted_mixtally, &tally);
		VectorScale(tally, 1/(tally[3]*255), tally);
		ted_itterate(hm, pos, radius, quant, SECTTEXSIZE, ted_mixset, &tally);
		break;
	case ter_tex_get:
		{
			int x, y;
			hmsection_t *s;
			x = pos[0] / hm->sectionsize;
			y = pos[1] / hm->sectionsize;
			x = bound(hm->firstsegx, x, hm->maxsegy-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);
		
			s = Terr_GetSection(hm, x, y, true);
			if (!s)
				return;
			x = bound(0, quant, 3);
			G_INT(OFS_RETURN) = PR_TempString(prinst, s->texname[x]);
		}
		break;
	case ter_tex_kill:
		{
			char *killtex = PR_GetStringOfs(prinst, OFS_PARM4);
			int x, y, t, to;
			hmsection_t *s;
			x = pos[0] / hm->sectionsize;
			y = pos[1] / hm->sectionsize;
			x = bound(hm->firstsegx, x, hm->maxsegy-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);
		
			s = Terr_GetSection(hm, x, y, true);
			if (!s)
				return;
			s->flags |= TSF_EDITED;
			for (t = 0; t < 4; t++)
			{
				if (!strcmp(s->texname[t], killtex))
				{
					unsigned char *lm = ted_getlightmap(s, 0);
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
									lm[to] += lm[t];
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
		break;
	case ter_mesh_add:
		{
			entity_t *e;
			float *epos;
			int x, y;
			hmsection_t *s;
			epos = ((wedict_t *)G_EDICT(prinst, OFS_PARM1))->v->origin;
			x = (epos[0] / hm->sectionsize) + CHUNKBIAS;
			y = (epos[1] / hm->sectionsize) + CHUNKBIAS;
			x = bound(hm->firstsegx, x, hm->maxsegy-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);
		
			s = Terr_GetSection(hm, x, y, true);
			if (!s)
				return;

			s->flags |= TSF_EDITED;

			if (s->maxents == s->numents)
			{
				s->maxents++;
				s->ents = realloc(s->ents, sizeof(*s->ents)*(s->maxents));
			}
			e = &s->ents[s->numents++];

			memset(e, 0, sizeof(*e));
			e->scale = ((wedict_t *)G_EDICT(prinst, OFS_PARM1))->xv->scale;
			e->shaderRGBAf[0] = 1;
			e->shaderRGBAf[1] = 1;
			e->shaderRGBAf[2] = 1;
			e->shaderRGBAf[3] = 1;
			VectorCopy(epos, e->origin);
			AngleVectorsFLU(((wedict_t *)G_EDICT(prinst, OFS_PARM1))->v->angles, e->axis[0], e->axis[1], e->axis[2]);
			e->model = vmw->Get_CModel(vmw, ((wedict_t *)G_EDICT(prinst, OFS_PARM1))->v->modelindex);
		}
		break;
	case ter_mesh_kill:
		{
//			int i;
//			entity_t *e;
			int x, y;
//			float r;
			hmsection_t *s;
			x = pos[0] / hm->sectionsize;
			y = pos[1] / hm->sectionsize;
			x = bound(hm->firstsegx, x, hm->maxsegy-1);
			y = bound(hm->firstsegy, y, hm->maxsegy-1);
		
			s = Terr_GetSection(hm, x, y, true);
			if (!s)
				return;

			s->numents = 0;
			s->flags |= TSF_EDITED;

			/*for (i = 0; i < s->numents; i++)
			{

			}*/
		}
		break;
	}
}
#else
void QCBUILTIN PF_terrain_edit(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = 0;
}
#endif

void Terr_ParseEntityLump(char *data, float *scale, int *minx, int *maxx, int *miny, int *maxy)
{
	char key[128];

	if (data)
	if ((data=COM_Parse(data)))	//read the map info.
	if (com_token[0] == '{')
	while (1)
	{
		if (!(data=COM_Parse(data)))
			break; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);	//_ vars are for comments/utility stuff that arn't visible to progs. Ignore them.
		else
			strcpy(key, com_token);
		if (!((data=COM_Parse(data))))
			break; // error		
		if (!strcmp("segmentsize", key))
			*scale = atof(com_token);
		else if (!strcmp("minxsegment", key))
			*minx = atoi(com_token) + CHUNKBIAS;
		else if (!strcmp("minysegment", key))
			*miny = atoi(com_token) + CHUNKBIAS;
		else if (!strcmp("maxxsegment", key))
			*maxx = atoi(com_token) + CHUNKBIAS;
		else if (!strcmp("maxysegment", key))
			*maxy = atoi(com_token) + CHUNKBIAS;
	}
}


qboolean Terr_LoadTerrainModel (model_t *mod, void *buffer)
{
	heightmap_t *hm;

	float skyrotate;
	vec3_t skyaxis;
	char shadername[MAX_QPATH];
	char skyname[MAX_QPATH];
	int numsegsx = 0, numsegsy = 0;
	int sectsize = 0;

	COM_FileBase(mod->name, shadername, sizeof(shadername));
	strcpy(shadername, "terrainshader");
	strcpy(skyname, "night");

	skyrotate = 0;
	skyaxis[0] = 0;
	skyaxis[1] = 0;
	skyaxis[2] = 0;

	buffer = COM_Parse(buffer);
	if (strcmp(com_token, "terrain"))
	{
		Con_Printf(CON_ERROR "%s wasn't terrain map\n", mod->name);	//shouldn't happen
		return false;
	}

	mod->type = mod_heightmap;

	hm = BZ_Malloc(sizeof(*hm));
	memset(hm, 0, sizeof(*hm));
	COM_FileBase(mod->name, hm->path, sizeof(hm->path));

	mod->entities = Hunk_AllocName(strlen(buffer)+1, mod->name);
	strcpy(mod->entities, buffer);

	hm->sectionsize = sectsize;
	hm->firstsegx = CHUNKBIAS - 1;
	hm->firstsegy = CHUNKBIAS - 1;
	hm->maxsegx = CHUNKBIAS + 1;
	hm->maxsegy = CHUNKBIAS + 1;
	hm->exteriorcontents = FTECONTENTS_SOLID;	//sky outside the map

	Terr_ParseEntityLump(mod->entities, &hm->sectionsize, &hm->firstsegx, &hm->maxsegx, &hm->firstsegy, &hm->maxsegy);

	if (hm->firstsegx < 0)
		hm->firstsegx = 0;
	if (hm->firstsegy < 0)
		hm->firstsegy = 0;
	if (hm->maxsegx > CHUNKLIMIT)
		hm->maxsegx = CHUNKLIMIT;
	if (hm->maxsegy > CHUNKLIMIT)
		hm->maxsegy = CHUNKLIMIT;

	mod->mins[0] = (hm->firstsegx - CHUNKBIAS) * hm->sectionsize;
	mod->mins[1] = (hm->firstsegy - CHUNKBIAS) * hm->sectionsize;
	mod->mins[2] = -999999999999999999999999.f;
	mod->maxs[0] = (hm->maxsegy - CHUNKBIAS) * hm->sectionsize;
	mod->maxs[1] = (hm->maxsegy - CHUNKBIAS) * hm->sectionsize;
	mod->maxs[2] = 999999999999999999999999.f;

#ifndef SERVERONLY
	if (qrenderer != QR_NONE)
	{
		hm->shader = R_RegisterShader(shadername,
					"{\n"
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
						"[\n"
						"program terraindebug\n"
						"]\n"
					"}\n"
				);
		hm->skyshader = R_RegisterCustom(va("skybox_%s", skyname), Shader_DefaultSkybox, NULL);
	}
#endif

	mod->funcs.NativeTrace			= Heightmap_Trace;
	mod->funcs.PointContents		= Heightmap_PointContents;

	mod->funcs.NativeContents		= Heightmap_NativeBoxContents;

	mod->funcs.LightPointValues		= Heightmap_LightPointValues;
	mod->funcs.StainNode			= Heightmap_StainNode;
	mod->funcs.MarkLights			= Heightmap_MarkLights;

	mod->funcs.LeafnumForPoint		= Heightmap_LeafForPoint;
	mod->funcs.LeafPVS				= Heightmap_LeafnumPVS;

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

void *Mod_LoadTerrainInfo(model_t *mod, char *loadname)
{
	heightmap_t *hm;
	float scale = 0;
	int bounds[4] = {0};
	if (!mod->entities)
		return NULL;

	Terr_ParseEntityLump(mod->entities, &scale, &bounds[0], &bounds[1], &bounds[2], &bounds[3]);

	bounds[0] += CHUNKBIAS;
	bounds[1] += CHUNKBIAS;
	bounds[2] += CHUNKBIAS;
	bounds[3] += CHUNKBIAS;

	if (bounds[0] < 0)
		bounds[0] = 0;
	if (bounds[2] < 0)
		bounds[2] = 0;
	if (bounds[1] > CHUNKLIMIT)
		bounds[1] = CHUNKLIMIT;
	if (bounds[3] > CHUNKLIMIT)
		bounds[3] = CHUNKLIMIT;

	if (!scale && (bounds[0] == bounds[1] || bounds[2] == bounds[3]))
		return NULL;

	if (scale < 1)
		scale = 1024;

	hm = Z_Malloc(sizeof(*hm));
	Q_strncpyz(hm->path, loadname, sizeof(hm->path));
	hm->sectionsize = scale;
	hm->firstsegx = bounds[0];
	hm->maxsegx = bounds[1];
	hm->firstsegy = bounds[2];
	hm->maxsegy = bounds[3];

	hm->exteriorcontents = FTECONTENTS_EMPTY;	//bsp geometry outside the heightmap


#ifndef SERVERONLY
	if (qrenderer != QR_NONE)
	{
		hm->skyshader = R_RegisterCustom(va("skybox_%s", loadname), Shader_DefaultSkybox, NULL);
		hm->shader = R_RegisterShader("terrainshader",
					"{\n"
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
						"[\n"
						"program terraindebug\n"
						"]\n"
					"}\n"
				);
	}
#endif
	return hm;
}
#endif
