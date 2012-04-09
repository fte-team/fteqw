#include "quakedef.h"

#ifdef TERRAIN
#ifdef GLQUAKE
#include "glquake.h"
#endif
#include "shader.h"

#include "pr_common.h"

int Surf_LM_AllocBlock (int w, int h, int *x, int *y, shader_t *shader);

//heightmaps work thusly:
//there is one raw heightmap file
//the file is split to 4*4 sections.
//each section is textured independantly (remember banshees are capped at 256*256 pixels)
//it's built into 16 seperate display lists, these display lists are individually culled, but the drivers are expected to optimise them too.
//Tei claims 14x speedup with a single display list. hopefully we can achieve the same speed by culling per-section.
//we get 20->130
//perhaps we should build it with multitexture? (no - slower on ati)

#define MAXSECTIONS 64	//this many sections max in each direction
#define SECTTEXSIZE 64	//this many texture samples per section
#define SECTHEIGHTSIZE 16 //this many height samples per section

typedef struct
{
	char texname[4][32];
	unsigned int texmap[SECTTEXSIZE][SECTTEXSIZE];
	float heights[SECTHEIGHTSIZE*SECTHEIGHTSIZE];
	unsigned short holes;
} dsection_t;
typedef struct
{
	float heights[SECTHEIGHTSIZE*SECTHEIGHTSIZE];
	unsigned short holes;
#ifndef SERVERONLY
	char texname[4][32];
	int lightmap;
	int lmx, lmy;

	texnums_t textures;
	vbo_t vbo;
	unsigned short minh, maxh;
	mesh_t mesh;
	mesh_t *amesh;
	qboolean modified:1;
#endif
} hmsection_t;
typedef struct {
	char path[MAX_QPATH];
	int numsegsx, numsegsy; //tex/cull sections
	float sectionsize;	//each section is this big, in world coords
	hmsection_t *section[MAXSECTIONS*MAXSECTIONS];
	shader_t *skyshader;
	shader_t *shader;
	mesh_t skymesh;
	mesh_t *askymesh;
} heightmap_t;

static void GL_LoadSectionTextures(hmsection_t *s)
{
#ifndef SERVERONLY
	//CL_CheckOrEnqueDownloadFile(s->texname[0], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[1], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[2], NULL, 0);
	//CL_CheckOrEnqueDownloadFile(s->texname[3], NULL, 0);
	s->textures.base			= R_LoadHiResTexture(s->texname[0], NULL, 0);
	s->textures.upperoverlay	= R_LoadHiResTexture(s->texname[1], NULL, 0);
	s->textures.loweroverlay	= R_LoadHiResTexture(s->texname[2], NULL, 0);
	s->textures.fullbright		= R_LoadHiResTexture(s->texname[3], NULL, 0);
	s->textures.bump			= R_LoadHiResTexture(va("%s_norm", s->texname[0]), NULL, 0);
	s->textures.specular		= R_LoadHiResTexture(va("%s_spec", s->texname[0]), NULL, 0);
#endif
}

static char *GL_DiskSectionName(heightmap_t *hm, int sx, int sy)
{
	return va("maps/%s/sect_%02i_%02i.hms", hm->path, sx, sy);
}
static hmsection_t *GL_LoadSection(heightmap_t *hm, int sx, int sy)
{
	hmsection_t *s;
	dsection_t *ds;
	int i;
#ifndef SERVERONLY
	unsigned char *lm;
#endif

	s = malloc(sizeof(*s));
	if (!s)
		return NULL;
	memset(s, 0, sizeof(*s));

#ifndef SERVERONLY
	s->lightmap = -1;

	Q_strncpyz(s->texname[0], va("maps/%s/grass", hm->path), sizeof(s->texname[0]));
	Q_strncpyz(s->texname[1], va("maps/%s/rock", hm->path), sizeof(s->texname[1]));
	Q_strncpyz(s->texname[2], va("maps/%s/road", hm->path), sizeof(s->texname[2]));
	Q_strncpyz(s->texname[3], va("maps/%s/ground", hm->path), sizeof(s->texname[3]));
	s->modified = true;

	if (s->lightmap < 0)
	{
		s->lightmap = Surf_LM_AllocBlock(SECTTEXSIZE, SECTTEXSIZE, &s->lmx, &s->lmy, hm->shader);
		BE_UploadAllLightmaps();
	}
#endif

	if (FS_LoadFile(GL_DiskSectionName(hm, sx, sy), &ds) >= 0)
	{
#ifndef SERVERONLY
		Q_strncpyz(s->texname[0], ds->texname[0], sizeof(s->texname[0]));
		Q_strncpyz(s->texname[1], ds->texname[1], sizeof(s->texname[1]));
		Q_strncpyz(s->texname[2], ds->texname[2], sizeof(s->texname[2]));
		Q_strncpyz(s->texname[3], ds->texname[3], sizeof(s->texname[3]));

		lm = lightmap[s->lightmap]->lightmaps;
		lm += (s->lmx * LMBLOCK_WIDTH + s->lmy) * lightmap_bytes;
		for (i = 0; i < SECTTEXSIZE; i++)
		{
			memcpy(lm, ds->texmap + i, sizeof(ds->texmap[0]));
			lm += (LMBLOCK_WIDTH)*lightmap_bytes;
		}
		lightmap[s->lightmap]->modified = true;
		lightmap[s->lightmap]->rectchange.l = 0;
		lightmap[s->lightmap]->rectchange.t = 0;
		lightmap[s->lightmap]->rectchange.w = LMBLOCK_WIDTH;
		lightmap[s->lightmap]->rectchange.h = LMBLOCK_HEIGHT;
#endif

		/*load the heights too*/
		for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
		{
			s->heights[i] = LittleFloat(ds->heights[i]);
		}

		FS_FreeFile(ds);
	}
	else
	{
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
				lm += (s->lmx * LMBLOCK_WIDTH + s->lmy) * lightmap_bytes;

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
					lm += (LMBLOCK_WIDTH - SECTTEXSIZE)*lightmap_bytes;
				}
				BZ_Free(splatter);

				lightmap[s->lightmap]->modified = true;
				lightmap[s->lightmap]->rectchange.l = 0;
				lightmap[s->lightmap]->rectchange.t = 0;
				lightmap[s->lightmap]->rectchange.w = LMBLOCK_WIDTH;
				lightmap[s->lightmap]->rectchange.h = LMBLOCK_HEIGHT;
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

	GL_LoadSectionTextures(s);

	hm->section[sx+sy*MAXSECTIONS] = s;

	return s;
}

static void GL_SaveSection(heightmap_t *hm, int sx, int sy)
{
#ifndef SERVERONLY
	hmsection_t *s = hm->section[sx+sy*MAXSECTIONS];
	dsection_t ds;
	unsigned char *lm;
	int i;
	if (!s || s->lightmap < 0)
		return;

	Q_strncpyz(ds.texname[0], s->texname[0], sizeof(ds.texname[0]));
	Q_strncpyz(ds.texname[1], s->texname[1], sizeof(ds.texname[1]));
	Q_strncpyz(ds.texname[2], s->texname[2], sizeof(ds.texname[2]));
	Q_strncpyz(ds.texname[3], s->texname[3], sizeof(ds.texname[3]));

	lm = lightmap[s->lightmap]->lightmaps;
	lm += (s->lmx * LMBLOCK_WIDTH + s->lmy) * lightmap_bytes;
	for (i = 0; i < SECTTEXSIZE; i++)
	{
		memcpy(ds.texmap + i, lm, sizeof(ds.texmap[0]));
		lm += (LMBLOCK_WIDTH)*lightmap_bytes;
	}

	for (i = 0; i < SECTHEIGHTSIZE*SECTHEIGHTSIZE; i++)
	{
		ds.heights[i] = LittleFloat(s->heights[i]);
	}

	FS_WriteFile(GL_DiskSectionName(hm, sx, sy), &ds, sizeof(ds), FS_GAMEONLY);
#endif
}

/*save all currently loaded sections*/
void HeightMap_Save(heightmap_t *hm)
{
	hmsection_t *s;
	int x, y;
	for (x = 0; x < hm->numsegsx; x++)
	{
		for (y = 0; y < hm->numsegsy; y++)
		{
			s = hm->section[x+y*MAXSECTIONS];
			GL_SaveSection(hm, x, y);
		}
	}
}

/*purge all sections*/
void HeightMap_Purge(model_t *mod)
{
	heightmap_t *hm = mod->terrain;
	hmsection_t *s;
	int x, y;
	for (x = 0; x < hm->numsegsx; x++)
	{
		for (y = 0; y < hm->numsegsy; y++)
		{
			s = hm->section[x+y*MAXSECTIONS];
			hm->section[x+y*MAXSECTIONS] = NULL;
			free(s);
		}
	}
}
#ifndef SERVERONLY
void GL_DrawHeightmapModel (batch_t **batches, entity_t *e)
{
	//a 512*512 heightmap
	//will draw 2 tris per square, drawn twice for detail
	//so a million triangles per frame if the whole thing is visible.

	//with 130 to 180fps, display lists rule!
	int x, y, vx, vy, v;
	vec3_t mins, maxs;
	model_t *m = e->model;
	heightmap_t *hm = m->terrain;
	mesh_t *mesh;
	batch_t *b;
	hmsection_t *s;

	if (e->model == cl.worldmodel)
	{
		b = BE_GetTempBatch();
		if (b)
		{
			b->lightmap = -1;
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

	for (x = 0; x < hm->numsegsx; x++)
	{
		mins[0] = (x+0)*hm->sectionsize;
		maxs[0] = (x+1)*hm->sectionsize;
		for (y = 0; y < hm->numsegsy; y++)
		{
			mins[1] = (y+0)*hm->sectionsize;
			maxs[1] = (y+1)*hm->sectionsize;

			s = hm->section[x+y*MAXSECTIONS];
			if (!s)
			{
				s = GL_LoadSection(hm, x, y);
				if (!s)
					continue;
			}
			mesh = &s->mesh;
			if (s->modified)
			{
//				minx = x*SECTHEIGHTSIZE;
//				miny = y*SECTHEIGHTSIZE;

				s->modified = false;

				if (s->lightmap < 0)
				{
					s->lightmap = Surf_LM_AllocBlock(SECTTEXSIZE, SECTTEXSIZE, &s->lmx, &s->lmy, hm->shader);
					BE_UploadAllLightmaps();
				}

				s->minh = 999999999999999;
				s->maxh = -999999999999999;

				if (!mesh->xyz_array)
				{
					mesh->xyz_array = BZ_Malloc((sizeof(vecV_t)+sizeof(vec2_t)+sizeof(vec2_t)) * (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
					mesh->st_array = (void*) (mesh->xyz_array + (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
					mesh->lmst_array = (void*) (mesh->st_array + (SECTHEIGHTSIZE)*(SECTHEIGHTSIZE));
				}
				mesh->numvertexes = 0;
				/*64 quads across requires 65 verticies*/
				for (vx = 0; vx < SECTHEIGHTSIZE; vx++)
				{
					for (vy = 0; vy < SECTHEIGHTSIZE; vy++)
					{
						v = mesh->numvertexes++;
						mesh->xyz_array[v][0] = (x + vx/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
						mesh->xyz_array[v][1] = (y + vy/(SECTHEIGHTSIZE-1.0f)) * hm->sectionsize;
						mesh->xyz_array[v][2] = s->heights[vx + vy*SECTHEIGHTSIZE];

						if (s->maxh < mesh->xyz_array[v][2])
							s->maxh = mesh->xyz_array[v][2];
						if (s->minh > mesh->xyz_array[v][2])
							s->minh = mesh->xyz_array[v][2];

						mesh->st_array[v][0] = mesh->xyz_array[v][0] / 64;
						mesh->st_array[v][1] = mesh->xyz_array[v][1] / 64;

						//calc the position in the range -0.5 to 0.5
						mesh->lmst_array[v][0] = (((float)vx / (SECTHEIGHTSIZE-1))-0.5);
						mesh->lmst_array[v][1] = (((float)vy / (SECTHEIGHTSIZE-1))-0.5);
						//scale down to a half-texel
						mesh->lmst_array[v][0] *= (SECTTEXSIZE-1.0f)/LMBLOCK_WIDTH;
						mesh->lmst_array[v][1] *= (SECTTEXSIZE-1.0f)/LMBLOCK_HEIGHT;
						//bias it
						mesh->lmst_array[v][0] += ((float)SECTTEXSIZE/(LMBLOCK_WIDTH*2)) + ((float)(s->lmy) / LMBLOCK_WIDTH);
						mesh->lmst_array[v][1] += ((float)SECTTEXSIZE/(LMBLOCK_HEIGHT*2)) + ((float)(s->lmx) / LMBLOCK_HEIGHT);

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
						//TODO: holes
						if (s->holes & (vx / (SECTHEIGHTSIZE/4)) << (vy / (SECTHEIGHTSIZE/4)) )
							continue;
						v = vx + vy*(SECTHEIGHTSIZE);
						mesh->indexes[mesh->numindexes++] = v+0;
						mesh->indexes[mesh->numindexes++] = v+1;
						mesh->indexes[mesh->numindexes++] = v+SECTHEIGHTSIZE;
						mesh->indexes[mesh->numindexes++] = v+1;
						mesh->indexes[mesh->numindexes++] = v+1+SECTHEIGHTSIZE;
						mesh->indexes[mesh->numindexes++] = v+SECTHEIGHTSIZE;
					}
				}

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
					s->vbo.lmcoord.gl.addr = (void*)((char*)mesh->lmst_array - (char*)mesh->xyz_array);
					s->vbo.lmcoord.gl.vbo = s->vbo.coord.gl.vbo;
//					Z_Free(mesh->xyz_array);
//					mesh->xyz_array = NULL;
//					mesh->st_array = NULL;
//					mesh->lmst_array = NULL;

					if (!s->vbo.indicies.gl.vbo)
						qglGenBuffersARB(1, &s->vbo.indicies.gl.vbo);
					s->vbo.indicies.gl.addr = 0;
					GL_SelectEBO(s->vbo.indicies.gl.vbo);
					qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(index_t) * mesh->numindexes, mesh->indexes, GL_STATIC_DRAW_ARB);
					GL_SelectEBO(0);
//					Z_Free(mesh->indexes);
//					mesh->indexes = NULL;
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
			b->lightmap = s->lightmap;

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
	int sectidx;
	hmsection_t *s;

	sx = org[0]/hm->sectionsize;
	sy = org[1]/hm->sectionsize;
	if (sx < 0 || sy < 0)
		return FTECONTENTS_SOLID;
	if (sx >= hm->numsegsx || sy >= hm->numsegsy)
		return FTECONTENTS_SOLID;
	sectidx = sx + sy*MAXSECTIONS;
	s = hm->section[sectidx];
	if (!s)
	{
		s = GL_LoadSection(hm, sx, sy);
		if (!s)
			return FTECONTENTS_SOLID;
	}

	x = (org[0] - (sx*hm->sectionsize))*(SECTHEIGHTSIZE-1)/hm->sectionsize;
	y = (org[1] - (sy*hm->sectionsize))*(SECTHEIGHTSIZE-1)/hm->sectionsize;
	z = (org[2]+clipmipsz);

	sx = x; x-=sx;
	sy = y; y-=sy;

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
	norm[0] = 0;
	norm[1] = 0;
	norm[2] = 1;
/*
	float x, y;
	float z;
	int sx, sy;
	vec3_t d1, d2;

	x = org[0]/(SECTHEIGHTSIZE * hm->sectionsize);
	y = org[1]/(SECTHEIGHTSIZE * hm->sectionsize);
	z = org[2];

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x > hm->tilesx-1)
		x = hm->tilesx-1;
	if (y > hm->tilesy-1)
		y = hm->tilesy-1;

	sx = x; x-=sx;
	sy = y; y-=sy;

	if (x+y>1)	//the 1, 1 triangle
	{
		//0, 1
		//1, 1
		//1, 0
		d1[0] = (SECTHEIGHTSIZE * hm->sectionsize);
		d1[1] = 0;
		d1[2] = (hm->heights[(sx+1)+(sy+1)*hm->tilesx] - hm->heights[(sx+0)+(sy+1)*hm->tilesx]);
		d2[0] = 0;
		d2[1] = (SECTHEIGHTSIZE * hm->sectionsize);
		d2[2] = (hm->heights[(sx+1)+(sy+1)*hm->tilesx] - hm->heights[(sx+1)+(sy+0)*hm->tilesx]);
	}
	else
	{	//the 0,0 triangle
		//0, 1
		//1, 0
		//0, 0
		d1[0] = (SECTHEIGHTSIZE * hm->sectionsize);
		d1[1] = 0;
		d1[2] = (hm->heights[(sx+0)+(sy+1)*hm->tilesx] - hm->heights[(sx+0)+(sy+0)*hm->tilesx]);
		d2[0] = 0;
		d2[1] = (SECTHEIGHTSIZE * hm->sectionsize);
		d2[2] = (hm->heights[(sx+1)+(sy+0)*hm->tilesx] - hm->heights[(sx+0)+(sy+0)*hm->tilesx]);
	}

	VectorNormalize(d1);
	VectorNormalize(d2);
	CrossProduct(d1, d2, norm);
	VectorNormalize(norm);
*/
}

#if 0
typedef struct {
	vec3_t start;
	vec3_t end;
	vec3_t impact;
	vec4_t plane;
	float frac;
	heightmap_t *hm;
	int contents;
} hmtrace_t;
#define Closestf(res,n,min,max) res = ((n>0)?min:max)
#define Closest(res,n,min,max) Closestf(res[0],n[0],min[0],max[0]);Closestf(res[1],n[1],min[1],max[1]);Closestf(res[2],n[2],min[2],max[2])
void Heightmap_Trace_Square(hmtrace_t *tr, int sx, int sy)
{
	vec3_t d[2];
	vec3_t p[3];
	vec4_t n[5];
	int t, i;

	qboolean startout, endout;
	float *enterplane;
	float enterfrac, exitfrac;
	float enterdist=0;
	float dist, d1, d2, f;

	if (sx < 0 || sx > tr->hm->tilesx)
		return;
	if (sy < 0 || sy > tr->hm->tilesy)
		return;

	for (t = 0; t < 2; t++)
	{
		/*generate the brush*/
		if (t == 0)
		{
			VectorSet(p[0], tr->hm->terrainscale*(sx+0), tr->hm->terrainscale*(sy+0), tr->hm->heights[(sx+0)+(sy+0)*tr->hm->tilesx]);
			VectorSet(p[1], tr->hm->terrainscale*(sx+1), tr->hm->terrainscale*(sy+0), tr->hm->heights[(sx+1)+(sy+0)*tr->hm->tilesx]);
			VectorSet(p[2], tr->hm->terrainscale*(sx+0), tr->hm->terrainscale*(sy+1), tr->hm->heights[(sx+0)+(sy+1)*tr->hm->tilesx]);
			VectorSubtract(p[1], p[0], d[0]);
			VectorSubtract(p[2], p[0], d[1]);
			//left-most
			Vector4Set(n[0], -1, 0, 0, tr->hm->terrainscale*(sx+0));
			//top-most
			Vector4Set(n[1], 0, -1, 0, tr->hm->terrainscale*(sy+0));
			//bottom-right
			VectorSet(n[2], 0.70710678118654752440084436210485, 0.70710678118654752440084436210485, 0);
			n[2][3] = -DotProduct(n[2], p[1]);
			//top
			CrossProduct(d[0], d[1], n[3]);
			VectorNormalize(n[3]);
			n[3][3] = -DotProduct(n[3], p[1]);
			//down
			Vector4Set(n[4], 0, 0, 1, 0);
		}
		else
		{
			VectorSet(p[0], tr->hm->terrainscale*(sx+1), tr->hm->terrainscale*(sy+1), tr->hm->heights[(sx+1)+(sy+1)*tr->hm->tilesx]);
			VectorSet(p[1], tr->hm->terrainscale*(sx+1), tr->hm->terrainscale*(sy+0), tr->hm->heights[(sx+1)+(sy+0)*tr->hm->tilesx]);
			VectorSet(p[2], tr->hm->terrainscale*(sx+0), tr->hm->terrainscale*(sy+1), tr->hm->heights[(sx+0)+(sy+1)*tr->hm->tilesx]);
			VectorSubtract(p[1], p[0], d[0]);
			VectorSubtract(p[2], p[0], d[1]);
			//right-most
			Vector4Set(n[0], 1, 0, 0, tr->hm->terrainscale*(sx+1));
			//bottom-most
			Vector4Set(n[1], 0, 1, 0, tr->hm->terrainscale*(sy+1));
			//bottom-right
			VectorSet(n[2], -0.70710678118654752440084436210485, -0.70710678118654752440084436210485, 0);
			n[2][3] = -DotProduct(n[2], p[1]);
			//top
			CrossProduct(d[0], d[1], n[3]);
			VectorNormalize(n[3]);
			n[3][3] = -DotProduct(n[3], p[1]);
			//down
			Vector4Set(n[4], 0, 0, 1, 0);
		}




		startout = false;
		endout = false;
		enterplane= NULL;
		enterfrac = -1;
		exitfrac = 10;
		for (i = 0; i < 5; i++)
		{
			/*calculate the distance based upon the shape of the object we're tracing for*/
			dist = n[i][3];


			d1 = DotProduct (tr->start, n[i]) - dist;
			d2 = DotProduct (tr->end, n[i]) - dist;

			if (d1 >= 0)
				startout = true;
			if (d2 > 0)
				endout = true;

			//if we're fully outside any plane, then we cannot possibly enter the brush, skip to the next one
			if (d1 > 0 && d2 >= 0)
				goto nextbrush;

			//if we're fully inside the plane, then whatever is happening is not relevent for this plane
			if (d1 < 0 && d2 <= 0)
				continue;

			f = d1 / (d1-d2);
			if (d1 > d2)
			{
				//entered the brush. favour the furthest fraction to avoid extended edges (yay for convex shapes)
				if (enterfrac < f)
				{
					enterfrac = f;
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
				tr->frac = enterfrac;
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
	point[2] = tr->hm->heights[sx+1+sy*tr->hm->terrainsize];

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
void Heightmap_RecurseTrace(hmtrace_t *tr, float p1[2], float p2[2])
{
	float newv[2];
	float frac;
	int mid;
	int axis;
	//FIXME: expand the trace somehow
	if ((int)p1[0] == (int)p2[0] && (int)p1[1] == (int)p2[1])
	{	//end
		Heightmap_Trace_Square(tr, p1[0], p2[1]);
		return;
	}
	/*decide the plane axis*/
	axis = abs(p2[1] - p1[1]) > abs(p2[0] - p1[0]);
	/*figure out the index to split the trace at*/
	mid = (p1[axis] + p2[axis])*0.5;
	if (!mid)/*make sure we make progress*/
		mid = (p2[axis] > p1[axis])?1:-1;

	//it crosses somewhere, it must do.
	if (p2[axis] > p1[axis])
		frac = ((p1[axis] - mid) - DIST_EPSILON)/(p1[axis]-p2[axis]);
	else
		frac = ((p1[axis] - mid) + DIST_EPSILON)/(p1[axis]-p2[axis]);

	newv[axis] = mid;
	newv[!axis] = (p2[!axis] * frac) + (p1[!axis] * (1-frac));
	if ((int)p1[axis] != (int)newv[axis])
		Heightmap_RecurseTrace(tr, p1, newv);
	if (!tr->contents)
	{
		Heightmap_RecurseTrace(tr, newv, p2);
	}
}

/*
Heightmap_TraceRecurse
Traces an arbitary box through a heightmap. (interface with outside)

Why is recursion good?
1: it is consistant with bsp models. :)
2: it allows us to use any size model we want
3: we don't have to work out the height of the terrain every X units, but can be more precise.

Obviously, we don't care all that much about 1
*/
qboolean Heightmap_Trace(struct model_s *model, int hulloverride, int frame, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, unsigned int against, struct trace_s *trace)
{
	float p1[2], p2[2];
	hmtrace_t hmtrace;
	hmtrace.hm = model->terrain;

	hmtrace.start[0] = start[0]/hmtrace.hm->terrainscale;
	hmtrace.start[1] = start[1]/hmtrace.hm->terrainscale;
	hmtrace.start[2] = (start[2] + mins[2]);
	hmtrace.end[0] = end[0]/hmtrace.hm->terrainscale;
	hmtrace.end[1] = end[1]/hmtrace.hm->terrainscale;
	hmtrace.end[2] = (end[2] + mins[2]);

	p1[0] = (start[0])/hmtrace.hm->terrainscale;
	p1[1] = (start[1])/hmtrace.hm->terrainscale;
	p2[0] = (end[0])/hmtrace.hm->terrainscale;
	p2[1] = (end[1])/hmtrace.hm->terrainscale;

	Heightmap_RecurseTrace(&hmtrace, p1, p2);

	trace->plane.dist = hmtrace.plane[3];
	trace->plane.normal[0] = hmtrace.plane[0];
	trace->plane.normal[1] = hmtrace.plane[1];
	trace->plane.normal[2] = hmtrace.plane[2];

	if (hmtrace.frac == -1)
	{
		trace->fraction = 0;
		trace->startsolid = true;
		trace->allsolid = true;
	}
	else
	{
		trace->fraction = hmtrace.frac;
		VectorInterpolate(start, hmtrace.frac, end, trace->endpos);
	}
	return trace->fraction < 1;
}
#else
/*
Heightmap_Trace
Traces a line through a heightmap, sampling the terrain at various different positions.
This is inprecise, only supports points (or vertical lines), and can often travel though sticky out bits of terrain.
*/
qboolean Heightmap_Trace(model_t *model, int forcehullnum, int frame, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, unsigned int contentmask, trace_t *trace)
{
	vec3_t org;
	vec3_t dir;
	float distleft;
	float dist;
	heightmap_t *hm = model->terrain;
	memset(trace, 0, sizeof(trace_t));

	if (Heightmap_PointContentsHM(hm, mins[2], start) == FTECONTENTS_SOLID)
	{
		trace->fraction = 0;
		trace->startsolid = true;
		trace->allsolid = true;
		VectorCopy(start, trace->endpos);
		return true;
	}
	VectorCopy(start, org);
	VectorSubtract(end, start, dir);
	dist = VectorNormalize(dir);
	if (dist < 10 && dist)
	{	//if less than 10 units, do at least 10 steps
		VectorScale(dir, 1/10.0f, dir);
		dist = 10;
	}
	distleft = dist;

	while(distleft>0)
	{
		VectorAdd(org, dir, org);
		if (Heightmap_PointContentsHM(hm, mins[2], org) == FTECONTENTS_SOLID)
		{	//go back to the previous safe spot
			VectorSubtract(org, dir, org);
			break;
		}
		distleft--;
	}

	trace->contents = Heightmap_PointContentsHM(hm, mins[2], end);

	if (distleft <= 0 && trace->contents != FTECONTENTS_SOLID)
	{	//all the way
		trace->fraction = 1;
		VectorCopy(end, trace->endpos);
	}
	else
	{	//we didn't get all the way there. :(
		VectorSubtract(org, start, dir);
		trace->fraction = Length(dir)/dist;
		if (trace->fraction > 1)
			trace->fraction = 1;
		VectorCopy(org, trace->endpos);
	}

	trace->plane.normal[0] = 0;
	trace->plane.normal[1] = 0;
	trace->plane.normal[2] = 1;
	Heightmap_Normal(model->terrain, trace->endpos, trace->plane.normal);

	return trace->fraction != 1;
}

#endif
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
		s->lightmap = Surf_LM_AllocBlock(SECTTEXSIZE, SECTTEXSIZE, &s->lmx, &s->lmy, NULL);
		BE_UploadAllLightmaps();
	}

	lightmap[s->lightmap]->modified = true;
	lightmap[s->lightmap]->rectchange.l = 0;
	lightmap[s->lightmap]->rectchange.t = 0;
	lightmap[s->lightmap]->rectchange.w = LMBLOCK_WIDTH;
	lightmap[s->lightmap]->rectchange.h = LMBLOCK_HEIGHT;
	lm = lightmap[s->lightmap]->lightmaps;
	lm += ((s->lmx+y) * LMBLOCK_WIDTH + (s->lmy+x)) * lightmap_bytes;
	return lm;
}
static void ted_heighttally(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	/*raise the terrain*/
	((float*)ctx)[0] += s->heights[idx]*w;
	((float*)ctx)[1] += w;
}
static void ted_heightsmooth(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	s->modified = true;
	/*interpolate the terrain towards a certain value*/
	s->heights[idx] = s->heights[idx]*(1-w) + w**(float*)ctx;
}
static void ted_heightraise(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength)
{
	s->modified = true;
	/*raise the terrain*/
	s->heights[idx] += strength;
}
static void ted_heightset(void *ctx, hmsection_t *s, int idx, float wx, float wy, float strength)
{
	s->modified = true;
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

static void ted_mixset(void *ctx, hmsection_t *s, int idx, float wx, float wy, float w)
{
	unsigned char *lm = ted_getlightmap(s, idx);
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

	min[0] = bound(0, min[0], hm->numsegsx);
	min[1] = bound(0, min[1], hm->numsegsy);
	max[0] = bound(0, max[0], hm->numsegsx);
	max[1] = bound(0, max[1], hm->numsegsy);

	sc[0] = hm->sectionsize/(steps-1);
	sc[1] = hm->sectionsize/(steps-1);

	for (sx = min[0]; sx < max[0]; sx++)
	{
		for (sy = min[1]; sy < max[1]; sy++)
		{
			s = hm->section[(int)(sx) + (int)(sy)*MAXSECTIONS];
			if (!s)
				s = GL_LoadSection(hm, sx, sy);
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
	ter_height_set,
	ter_height_smooth,
	ter_raise,
	ter_lower,
	ter_tex_set,
	ter_tex_get,
	ter_mixset,
	ter_mixconcentrate,
	ter_mixnoise,
	ter_mixblur,
};
void QCBUILTIN PF_terrain_edit(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *vmw = prinst->parms->user;
	int action = G_FLOAT(OFS_PARM0);
	float *pos = G_VECTOR(OFS_PARM1);
	float radius = G_FLOAT(OFS_PARM2);
	float quant = G_FLOAT(OFS_PARM3);
//	G_FLOAT(OFS_RETURN) = Heightmap_Edit(w->worldmodel, action, pos, radius, quant);

	model_t *mod = vmw->worldmodel;
	heightmap_t *hm;
	vec4_t tally;

	G_FLOAT(OFS_RETURN) = 0;

	if (!mod || mod->type != mod_heightmap)
		return;
	hm = mod->terrain;

	switch(action)
	{
	case ter_reload:
		HeightMap_Purge(mod);
		break;
	case ter_save:
		HeightMap_Save(hm);
		break;
	case ter_height_set:
		ted_itterate(hm, pos, radius, 1, SECTHEIGHTSIZE, ted_heightset, &quant);
		break;
	case ter_height_smooth:
		tally[0] = 0;
		tally[1] = 0;
		ted_itterate(hm, pos, radius, 1, SECTHEIGHTSIZE, ted_heighttally, &tally);
		tally[0] /= tally[1];
		ted_itterate(hm, pos, radius, 1, SECTHEIGHTSIZE, ted_heightsmooth, &tally);
		break;
	case ter_lower:
		quant *= -1;
	case ter_raise:
		ted_itterate(hm, pos, radius, quant, SECTHEIGHTSIZE, ted_heightraise, &quant);
		break;
	case ter_mixset:
		ted_itterate(hm, pos, radius, 1, SECTTEXSIZE, ted_mixset, G_VECTOR(OFS_PARM4));
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
		VectorScale(tally, 1/tally[3], tally);
		ted_itterate(hm, pos, radius, quant, SECTTEXSIZE, ted_mixset, &tally);
		break;
	case ter_tex_set:
		ted_itterate(hm, pos, radius, 1, SECTTEXSIZE, ted_mixset, NULL);
/*		radius *= (float)hm->numsegsx / hm->tilesx;
		for (x = 0; x < hm->numsegsx; x++)
		{
			for (y = 0; y < hm->numsegsy; y++)
			{
				xd = (sc[0] - x) * (float)hm->numsegsx / hm->tilesx;
				yd = (sc[1] - y) * (float)hm->numsegsy / hm->tilesy;
				w = sqrt(radius*radius - (xd*xd+yd*yd));
				if (w > 0)
				{
					s = hm->section[(int)(x) + (int)(y)*MAXSECTIONS];
					if (!s)
						s = GL_LoadSection(hm, x, y);
					if (s)
					{
						if (quant < 0 || quant >= 4)
							quant = 0;
						Q_strncpyz(s->texname[(int)quant], PR_GetStringOfs(prinst, OFS_PARM4), sizeof(s->texname[0]));
						s->modified = true;

						GL_LoadSectionTextures(s);
					}
				}
			}
		}
*/
		break;
	case ter_tex_get:
/*
		x = sc[0]*hm->numsegsx / hm->tilesx;
		y = sc[1]*hm->numsegsy / hm->tilesy;
		x = bound(0, x, hm->numsegsx-1);
		y = bound(0, y, hm->numsegsy-1);
	
		G_INT(OFS_RETURN) = 0;
		s = hm->section[(int)(x) + (int)(y)*MAXSECTIONS];
		if (!s)
			s = GL_LoadSection(hm, x, y);
		if (s)
		{
			x = bound(0, quant, 3);
			G_INT(OFS_RETURN) = PR_TempString(prinst, s->texname[x]);
		}
*/
		break;
	}
}
#else
void QCBUILTIN PF_terrain_edit(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	G_FLOAT(OFS_RETURN) = 0;
}
#endif

qboolean GL_LoadHeightmapModel (model_t *mod, void *buffer)
{
	heightmap_t *hm;

	float skyrotate;
	vec3_t skyaxis;
	char shadername[MAX_QPATH];
	char entfile[MAX_QPATH];
	char skyname[MAX_QPATH];
	int numsegsx = 0, numsegsy = 0;
	int sectsize = 0;

	COM_FileBase(mod->name, shadername, sizeof(shadername));
	Q_snprintfz(entfile, sizeof(entfile), "maps/%s/entities.ent", shadername);
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

	for(;;)
	{
		buffer = COM_Parse(buffer);
		if (!buffer)
			break;

		if (!strcmp(com_token, "shadername"))
		{
			buffer = COM_Parse(buffer);
			Q_strncpyz(shadername, com_token, sizeof(shadername));
		}
		else if (!strcmp(com_token, "segmentsize"))	//size of each segment in quake units
		{
			buffer = COM_Parse(buffer);
			sectsize = atof(com_token);
		}
		else if (!strcmp(com_token, "entfile"))
		{
			buffer = COM_Parse(buffer);
			Q_strncpyz(entfile, com_token, sizeof(entfile));
		}
		else if (!strcmp(com_token, "skybox"))
		{
			buffer = COM_Parse(buffer);
			Q_strncpyz(skyname, com_token, sizeof(skyname));
		}
		else if (!strcmp(com_token, "skyrotate"))
		{
			buffer = COM_Parse(buffer);
			skyaxis[0] = atof(com_token);
			buffer = COM_Parse(buffer);
			skyaxis[1] = atof(com_token);
			buffer = COM_Parse(buffer);
			skyaxis[2] = atof(com_token);
			skyrotate = VectorNormalize(skyaxis);
		}
		else if (!strcmp(com_token, "texturesegments"))
		{
			buffer = COM_Parse(buffer);
			numsegsx = numsegsy = atoi(com_token);
		}
		else if (!strcmp(com_token, "texturesegmentsx"))
		{
			buffer = COM_Parse(buffer);
			numsegsx = atoi(com_token);
		}
		else if (!strcmp(com_token, "texturesegmentsy"))
		{
			buffer = COM_Parse(buffer);
			numsegsy = atoi(com_token);
		}
		else
		{
			Con_Printf(CON_ERROR "%s, unrecognised token \"%s\" in terrain map\n", mod->name, com_token);
			return false;
		}
	}

	if (!sectsize)
		sectsize = 1024;

	if (!numsegsx)
		numsegsx = 16;
	if (!numsegsy)
		numsegsy = 16;

	if (numsegsx > MAXSECTIONS || numsegsy > MAXSECTIONS)
	{
		Con_Printf(CON_ERROR "%s, heightmap uses too many sections max is %i\n", mod->name, MAXSECTIONS);
		return false;
	}
	mod->type = mod_heightmap;

	hm = BZ_Malloc(sizeof(*hm));
	memset(hm, 0, sizeof(*hm));
	COM_FileBase(mod->name, hm->path, sizeof(hm->path));

	mod->entities = COM_LoadHunkFile(entfile);
	if (!mod->entities)
	{
		BZ_Free(hm);
		Con_Printf(CON_ERROR "unable to read %s\n", entfile);
		return false;
	}

	hm->sectionsize = sectsize;
	hm->numsegsx = numsegsx;
	hm->numsegsy = numsegsy;

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
#endif
