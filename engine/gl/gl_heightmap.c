#include "quakedef.h"

#if defined(TERRAIN) && !defined(SERVERONLY) //fixme
#ifdef GLQUAKE
#include "glquake.h"
#endif
#include "shader.h"

//heightmaps work thusly:
//there is one raw heightmap file
//the file is split to 4*4 sections.
//each section is textured independantly (remember banshees are capped at 512 pixels)
//there's a detailtexture blended over the top to fake the detail.
//it's built into 16 seperate display lists, these display lists are individually culled, but the drivers are expected to optimise them too.
//Tei claims 14x speedup with a single display list. hopefully we can achieve the same speed by culling per-texture.
//we get 20->130
//perhaps we should build it with multitexture? (no - slower on ati)

#define SECTIONS 8

typedef struct {
	char path[MAX_QPATH];
	char hmapname[MAX_QPATH];
	unsigned short *heights;
	int terrainsize;
	float terrainscale;
	float heightscale;
	int numsegs;
	texid_t detailtexture;
	texid_t textures[SECTIONS*SECTIONS];
	int displaylist[SECTIONS*SECTIONS];	//display lists are famous for being stupidly fast with heightmaps.
	unsigned short mins[SECTIONS*SECTIONS], maxs[SECTIONS*SECTIONS];

	shader_t *skyshader;
	shader_t *shader;
	mesh_t mesh[SECTIONS*SECTIONS];
	mesh_t *amesh[SECTIONS*SECTIONS];
	mesh_t skymesh;
	mesh_t *askymesh;

	qboolean modified[SECTIONS*SECTIONS];
} heightmap_t;

#define DISPLISTS
//#define MULTITEXTURE	//ATI suck. I don't know about anyone else (this goes at 1/5th the speed).

void GL_DrawHeightmapModel (batch_t **batches, entity_t *e)
{
	//a 512*512 heightmap
	//will draw 2 tris per square, drawn twice for detail
	//so a million triangles per frame if the whole thing is visible.

	//with 130 to 180fps, display lists rule!
	int x, y, vx, vy, v;
	float subsize;
	int minx, miny;
	vec3_t mins, maxs;
	model_t *m = e->model;
	heightmap_t *hm = m->terrain;
	mesh_t *mesh;
	batch_t *b;

	if (e->model == cl.worldmodel)
	{
		b = BE_GetTempBatch();
		if (b)
		{
			b->ent = e;
			b->shader = hm->skyshader;
			b->flags = 0;
			b->mesh = &hm->askymesh;
			b->mesh[0] = &hm->skymesh;
			b->meshes = 1;
			b->buildmeshes = NULL;
			b->skin = &b->shader->defaulttextures;
			b->texture = NULL;
	//		vbo = b->vbo = hm->vbo[x+y*SECTIONS];
			b->vbo = NULL;

			b->next = batches[b->shader->sort];
			batches[b->shader->sort] = b;
		}
	}

	subsize = hm->terrainsize/SECTIONS;
	for (x = 0; x < hm->numsegs; x++)
	{
		mins[0] = (x+0)*subsize*hm->terrainscale;
		maxs[0] = (x+1)*subsize*hm->terrainscale;
		for (y = 0; y < hm->numsegs; y++)
		{
			mins[1] = (y+0)*subsize*hm->terrainscale;
			maxs[1] = (y+1)*subsize*hm->terrainscale;

			mesh = &hm->mesh[x+y*SECTIONS];
			if (hm->modified[x+y*SECTIONS])
			{
				minx = x*subsize;
				miny = y*subsize;

				hm->modified[x+y*SECTIONS] = false;

				hm->mins[x+y*SECTIONS] = 65536 * hm->heightscale;
				hm->maxs[x+y*SECTIONS] = 0;

				if (!mesh->xyz_array)
					mesh->xyz_array = BZ_Malloc(sizeof(vecV_t) * (subsize+1)*(subsize+1));
				if (!mesh->st_array)
					mesh->st_array = BZ_Malloc(sizeof(vec2_t) * (subsize+1)*(subsize+1));
				if (!mesh->lmst_array)
					mesh->lmst_array = BZ_Malloc(sizeof(vec2_t) * (subsize+1)*(subsize+1));
				mesh->numvertexes = 0;
				/*64 quads across requires 65 verticies*/
				for (vx = 0; vx <= subsize; vx++)
				{
					for (vy = 0; vy <= subsize; vy++)
					{
						v = mesh->numvertexes++;
						mesh->xyz_array[v][0] = (vx+minx)*hm->terrainscale;
						mesh->xyz_array[v][1] = (vy+miny)*hm->terrainscale;
						mesh->xyz_array[v][2] = hm->heights[(vx+minx) + (vy+miny)*hm->terrainsize]*hm->heightscale;

						if (hm->maxs[x+y*SECTIONS] < mesh->xyz_array[v][2])
							hm->maxs[x+y*SECTIONS] = mesh->xyz_array[v][2];
						if (hm->mins[x+y*SECTIONS] > mesh->xyz_array[v][2])
							hm->mins[x+y*SECTIONS] = mesh->xyz_array[v][2];

						mesh->st_array[v][0] = mesh->xyz_array[v][0] / 64;
						mesh->st_array[v][1] = mesh->xyz_array[v][1] / 64;

						mesh->lmst_array[v][0] = mesh->xyz_array[v][0] / 64;
						mesh->lmst_array[v][1] = mesh->xyz_array[v][1] / 64;
					}
				}

				if (!mesh->indexes)
					mesh->indexes = BZ_Malloc(sizeof(index_t) * subsize*subsize*6);

				mesh->numindexes = 0;
				for (vx = 0; vx < subsize; vx++)
				{
					for (vy = 0; vy < subsize; vy++)
					{
						v = vx + vy*(subsize+1);
						mesh->indexes[mesh->numindexes++] = v+0;
						mesh->indexes[mesh->numindexes++] = v+1;
						mesh->indexes[mesh->numindexes++] = v+subsize+1;
						mesh->indexes[mesh->numindexes++] = v+1;
						mesh->indexes[mesh->numindexes++] = v+1+subsize+1;
						mesh->indexes[mesh->numindexes++] = v+subsize+1;
					}
				}


				//GL_BuildVBO();
			}

			mins[2] = hm->mins[x+y*SECTIONS];
			maxs[2] = hm->maxs[x+y*SECTIONS];

			if (!BoundsIntersect(mins, maxs, r_refdef.vieworg, r_refdef.vieworg))
				if (R_CullBox(mins, maxs))
					continue;

			b = BE_GetTempBatch();
			if (!b)
				continue;
			b->ent = e;
			b->shader = hm->shader;
			b->flags = 0;
			b->mesh = &hm->amesh[x+y*SECTIONS];
			b->mesh[0] = mesh;
			b->meshes = 1;
			b->buildmeshes = NULL;
			b->skin = &b->shader->defaulttextures;
			b->texture = NULL;
	//		vbo = b->vbo = hm->vbo[x+y*SECTIONS];
			b->vbo = NULL;

			b->next = batches[b->shader->sort];
			batches[b->shader->sort] = b;
		}
	}
}

unsigned int Heightmap_PointContentsHM(heightmap_t *hm, float clipmipsz, vec3_t org)
{
	float x, y;
	float z, tz;
	int sx, sy;

	x = org[0]/hm->terrainscale;
	y = org[1]/hm->terrainscale;
	z = (org[2]+clipmipsz)/hm->heightscale;

	if (z < 0)
		return FTECONTENTS_SOLID;
	if (z > 65535)
	{
		if (z > 65535+64 || clipmipsz)	//top 64 units are sky
			return FTECONTENTS_SOLID;
		else
			return FTECONTENTS_SKY;
	}
	if (x < 0)
	{
		if (x <= -1 || clipmipsz)
			return FTECONTENTS_SOLID;
		else
			return FTECONTENTS_SKY;
	}
	if (y < 0)
	{
		if (x <= -1 || clipmipsz)
			return FTECONTENTS_SOLID;
		else
			return FTECONTENTS_SKY;
	}
	if (x >= hm->terrainsize-1)
	{
		if (x >= hm->terrainsize || clipmipsz)
			return FTECONTENTS_SOLID;
		else
			return FTECONTENTS_SKY;
	}
	if (y >= hm->terrainsize-1)
	{
		if (y >= hm->terrainsize || clipmipsz)
			return FTECONTENTS_SOLID;
		else
			return FTECONTENTS_SKY;
	}

	sx = x; x-=sx;
	sy = y; y-=sy;

	//made of two triangles:
#if 1
	if (x+y>1)	//the 1, 1 triangle
	{
		float v1, v2, v3;
		v3 = 1-y;
		v2 = x+y-1;
		v1 = 1-x;
		//0, 1
		//1, 1
		//1, 0
		tz = (hm->heights[(sx+0)+(sy+1)*hm->terrainsize]*v1 +
			  hm->heights[(sx+1)+(sy+1)*hm->terrainsize]*v2 +
			  hm->heights[(sx+1)+(sy+0)*hm->terrainsize]*v3);
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
		tz = (hm->heights[(sx+0)+(sy+1)*hm->terrainsize]*v1 +
			  hm->heights[(sx+1)+(sy+0)*hm->terrainsize]*v2 +
			  hm->heights[(sx+0)+(sy+0)*hm->terrainsize]*v3);
	}
#else
	{
		float t, b;
	//square?
	//:(

		t = (hm->heights[sx+sy*hm->terrainsize]*(1-x) + hm->heights[sx+1+sy*hm->terrainsize]*x);
		b = (hm->heights[sx+(sy+1)*hm->terrainsize]*(1-x) + hm->heights[sx+1+(sy+1)*hm->terrainsize]*x);
		tz = t*(1-y) + b*y;
	}
#endif
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
	float x, y;
	float z;
	int sx, sy;
	vec3_t d1, d2;

	x = org[0]/hm->terrainscale;
	y = org[1]/hm->terrainscale;
	z = org[2];

	sx = x; x-=sx;
	sy = y; y-=sy;

	if (x+y>1)	//the 1, 1 triangle
	{
		//0, 1
		//1, 1
		//1, 0
		d1[0] = hm->terrainscale;
		d1[1] = 0;
		d1[2] = hm->heights[(sx+1)+(sy+1)*hm->terrainsize] - hm->heights[(sx+0)+(sy+1)*hm->terrainsize];
		d2[0] = 0;
		d2[1] = hm->terrainscale;
		d2[2] = hm->heights[(sx+1)+(sy+1)*hm->terrainsize] - hm->heights[(sx+1)+(sy+0)*hm->terrainsize];
	}
	else
	{
		//0, 1
		//1, 0
		//0, 0
		d1[0] = hm->terrainscale;
		d1[1] = 0;
		d1[2] = hm->heights[(sx+0)+(sy+1)*hm->terrainsize] - hm->heights[(sx+0)+(sy+0)*hm->terrainsize];
		d2[0] = 0;
		d2[1] = hm->terrainscale;
		d2[2] = hm->heights[(sx+1)+(sy+0)*hm->terrainsize] - hm->heights[(sx+0)+(sy+0)*hm->terrainsize];
	}

	CrossProduct(d1, d2, norm);
	VectorNormalize(norm);
}

#if 0
typedef struct {
	vec3_t start;
	vec3_t end;
	vec3_t mins;
	vec3_t maxs;
	vec3_t impact;
	heightmap_t *hm;
	int contents;
} hmtrace_t;
#define Closestf(res,n,min,max) res = ((n>0)?min:max)
#define Closest(res,n,min,max) Closestf(res[0],n[0],min[0],max[0]);Closestf(res[1],n[1],min[1],max[1]);Closestf(res[2],n[2],min[2],max[2])
void Heightmap_Trace_Square(hmtrace_t *tr, int sx, int sy)
{
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

	tris = sd<=pd || ed<=pd;
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

		norm[0] = (-x*tr->hm->heightscale)/tr->hm->terrainscale;
		norm[1] = (-y*tr->hm->heightscale)/tr->hm->terrainscale;
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
		norm[0] = (-x*tr->hm->heightscale)/tr->hm->terrainscale;
		norm[1] = (-y*tr->hm->heightscale)/tr->hm->terrainscale;
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
}
void Heightmap_Trace_Y(hmtrace_t *tr, int x, int min, int max)
{
	int mid;
	if (min == max)
	{	//end
		Heightmap_Trace_Square(tr, x, min);
		return;
	}
	mid = ((max-min)>>1)+min;
	if (tr->start[1] < min+tr->mins[1] && tr->end[1] < min+tr->mins[1])
	{	//both on one size.
		Heightmap_Trace_Y(tr, x, min, mid);
		return;
	}
	if (tr->start[1] > max+tr->maxs[1] && tr->end[1] > max+tr->maxs[1])
	{	//both on one size.
		Heightmap_Trace_Y(tr, x, mid, max);
		return;
	}

	//crosses this line.
	if (tr->start[1] > tr->end[1])
	{
		Heightmap_Trace_Y(tr, x, min, mid);
		if (!tr->contents)
			Heightmap_Trace_Y(tr, x, mid, max);
	}
	else
	{
		Heightmap_Trace_Y(tr, x, mid, max);
		if (!tr->contents)
			Heightmap_Trace_Y(tr, x, min, mid);
	}
}
void Heightmap_Trace_X(hmtrace_t *tr, int min, int max)
{
	int mid;
	if (min == max)
	{	//end
		//FIXME: we don't have to check ALL squares like this, we could use a much smaller range.
		Heightmap_Trace_Y(tr, min, 0, tr->hm->terrainsize);
		return;
	}
	mid = ((max-min)>>1)+min;
	if (tr->start[0] < min+tr->mins[0] && tr->end[0] < min+tr->mins[0])
	{	//both on one size.
		Heightmap_Trace_X(tr, min, mid);
		return;
	}
	if (tr->start[0] > max+tr->maxs[0] && tr->end[0] > max+tr->maxs[0])
	{	//both on one size.
		Heightmap_Trace_X(tr, mid, max);
		return;
	}

	//crosses this line.
	if (tr->start[0] > tr->end[0])
	{
		Heightmap_Trace_X(tr, min, mid);
		if (!tr->contents)
			Heightmap_Trace_X(tr, mid, max);
	}
	else
	{
		Heightmap_Trace_X(tr, mid, max);
		if (!tr->contents)
			Heightmap_Trace_X(tr, min, mid);
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
qboolean Heightmap_Trace(model_t *model, int forcehullnum, int frame, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, trace_t *trace)
{
	hmtrace_t hmtrace;
	hmtrace.hm = model->terrain;

	hmtrace.start[0] = start[0]/hmtrace.hm->terrainscale;
	hmtrace.start[1] = start[1]/hmtrace.hm->terrainscale;
	hmtrace.start[2] = (start[2])/hmtrace.hm->heightscale;
	hmtrace.end[0] = end[0]/hmtrace.hm->terrainscale;
	hmtrace.end[1] = end[1]/hmtrace.hm->terrainscale;
	hmtrace.end[2] = (end[2])/hmtrace.hm->heightscale;
	hmtrace.mins[0] = mins[0]/hmtrace.hm->terrainscale;
	hmtrace.mins[1] = mins[1]/hmtrace.hm->terrainscale;
	hmtrace.mins[2] = (mins[2])/hmtrace.hm->heightscale;
	hmtrace.maxs[0] = maxs[0]/hmtrace.hm->terrainscale;
	hmtrace.maxs[1] = maxs[1]/hmtrace.hm->terrainscale;
	hmtrace.maxs[2] = (maxs[2])/hmtrace.hm->heightscale;

	//FIXME: we don't have to check ALL squares like this, we could use a much smaller range.
	Heightmap_Trace_X(&hmtrace, 0, hmtrace.hm->terrainsize);
}
#else
/*
Heightmap_Trace
Traces a line through a heightmap, sampling the terrain at various different positions.
This is inprecise, only supports points (or vertical lines), and can often travel though sticky out bits of terrain.
*/
qboolean Heightmap_Trace(model_t *model, int forcehullnum, int frame, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, trace_t *trace)
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
qboolean Heightmap_NativeTrace(struct model_s *model, int hulloverride, int frame, vec3_t axis[3], vec3_t p1, vec3_t p2, vec3_t mins, vec3_t maxs, unsigned int against, struct trace_s *trace)
{
	return Heightmap_Trace(model, hulloverride, frame, axis, p1, p2, mins, maxs, trace);
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

//Heightmap_NativeBoxContents

enum
{
	ter_reload,
	ter_save,
	ter_set,
	ter_smooth,
	ter_raise,
	ter_lower
};
qboolean Heightmap_Edit(model_t *mod, int action, float *pos, float radius, float quant)
{
	unsigned short *tmp;
	heightmap_t *hm = mod->terrain;
	int size;
	int x, y, min[2], max[2];
	float xd, yd, w;
	vec2_t sc;
	if (mod->type != mod_heightmap)
		return false;

	size = hm->terrainsize;

	if (radius < 0.05)
		radius = 0.05;

	sc[0] = pos[0] / hm->terrainscale;
	sc[1] = pos[1] / hm->terrainscale;
	radius = radius / hm->terrainscale;
	quant /= hm->heightscale;

	min[0] = sc[0] - radius;
	min[1] = sc[1] - radius;
	max[0] = sc[0] + radius;
	max[1] = sc[1] + radius;

	if (min[0] < 0)
		min[0] = 0;
	if (min[1] < 0)
		min[1] = 0;

	if (max[0] > size)
		max[0] = size;
	if (max[1] < size)
		max[1] = size;

	switch(action)
	{
	case ter_reload:
		tmp = (unsigned short*)COM_LoadTempFile(hm->hmapname);
		if (tmp)
			x = com_filesize/2;
		else
			x = 0;
		if (x > size*size)
			x = size*size;
		while (x-- > 0)
		{
			hm->heights[x] = LittleShort(tmp[x]);
		}
		break;
	case ter_save:
		tmp = Hunk_TempAlloc(size*size*2);
		for (x = 0; x < size*size; x++)
		{
			tmp[x] = LittleShort(hm->heights[x]);
		}
		COM_WriteFile(hm->hmapname, tmp, size*size*2);
		break;
	case ter_set:
		for (x = min[0]; x < max[0]; x++)
		{
			for (y = min[1]; y < max[1]; y++)
			{
				xd = sc[0] - x;
				yd = sc[1] - y;
				if (sqrt(xd*xd+yd*yd) < radius)
				{
					hm->heights[x + y*size] = quant;
					hm->modified[(int)(x/(hm->terrainscale)) + (int)(y/(hm->terrainscale))*SECTIONS] = true;
				}
			}
		}
		break;
	case ter_smooth:
	case ter_raise:
		for (x = min[0]; x < max[0]; x++)
		{
			for (y = min[1]; y < max[1]; y++)
			{
				xd = sc[0] - x;
				yd = sc[1] - y;
				w = sqrt(radius*radius - (xd*xd+yd*yd));
				if (w > 0)
				{
					w *= quant/radius;
					hm->heights[x + y*size] += w;
					hm->modified[(int)(x/(hm->terrainscale)) + (int)(y/(hm->terrainscale))*SECTIONS] = true;
				}
			}
		}
		break;
	case ter_lower:
		for (x = min[0]; x < max[0]; x++)
		{
			for (y = min[1]; y < max[1]; y++)
			{
				xd = sc[0] - x;
				yd = sc[1] - y;
				w = sqrt(radius*radius - (xd*xd+yd*yd));
				if (w > 0)
				{
					w *= quant/radius;
					/*don't drop below 0*/
					if (hm->heights[x + y*size]-w < 0)
						hm->heights[x + y*size] = 0;
					else
						hm->heights[x + y*size] -= w;
					hm->modified[(int)(x/(hm->terrainscale)) + (int)(y/(hm->terrainscale))*SECTIONS] = true;
				}
			}
		}
		break;
	}

	return true;
}

qboolean GL_LoadHeightmapModel (model_t *mod, void *buffer)
{
	heightmap_t *hm;
	unsigned short *heightmap;
	int size;
	int x, y;

	float skyrotate;
	vec3_t skyaxis;
	char heightmapname[MAX_QPATH];
	char detailtexname[MAX_QPATH];
	char basetexname[MAX_QPATH];
	char exttexname[MAX_QPATH];
	char entfile[MAX_QPATH];
	char skyname[MAX_QPATH];
	float worldsize = 64;
	float heightsize = 1/16;
	int numsegs = 1;

	*heightmapname = '\0';
	*detailtexname = '\0';
	*basetexname = '\0';
	*exttexname = '\0';
	*entfile = '\0';
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

	if (qrenderer != QR_OPENGL)
		return false;

	for(;;)
	{
		buffer = COM_Parse(buffer);
		if (!buffer)
			break;

		if (!strcmp(com_token, "heightmap"))
		{
			buffer = COM_Parse(buffer);
			Q_strncpyz(heightmapname, com_token, sizeof(heightmapname));
		}
		else if (!strcmp(com_token, "detail"))
		{
			buffer = COM_Parse(buffer);
			Q_strncpyz(detailtexname, com_token, sizeof(detailtexname));
		}
		else if (!strcmp(com_token, "texturegridbase"))
		{
			buffer = COM_Parse(buffer);
			Q_strncpyz(basetexname, com_token, sizeof(basetexname));
		}
		else if (!strcmp(com_token, "texturegridext"))
		{
			buffer = COM_Parse(buffer);
			Q_strncpyz(exttexname, com_token, sizeof(exttexname));
		}
		else if (!strcmp(com_token, "gridsize"))
		{
			buffer = COM_Parse(buffer);
			worldsize = atof(com_token);
		}
		else if (!strcmp(com_token, "heightsize"))
		{
			buffer = COM_Parse(buffer);
			heightsize = atof(com_token);
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
			numsegs = atoi(com_token);
		}
		else
		{
			Con_Printf(CON_ERROR "%s, unrecognised token in terrain map\n", mod->name);
			return false;
		}
	}

	if (numsegs > SECTIONS)
	{
		Con_Printf(CON_ERROR "%s, heightmap uses too many sections max is %i\n", mod->name, SECTIONS);
		return false;
	}


	mod->type = mod_heightmap;

	heightmap = (unsigned short*)COM_LoadTempFile(heightmapname);
	if (!heightmap)
	{
		size = 1024;
		heightmap = Hunk_TempAlloc(size*size*2);

		for (x = 0; x < size; x++)
		for (y = 0; y < size; y++)
		{
			heightmap[x+y*size] = 0;
		}
	}
	else
	{
		size = sqrt(com_filesize/2);
		if (size % numsegs)
		{
			Con_Printf(CON_ERROR "%s, heightmap is not a multiple of %i\n", mod->name, numsegs);
			return false;
		}
	}

	hm = Hunk_Alloc(sizeof(*hm) + size*size*2);
	memset(hm, 0, sizeof(*hm));
	Q_strncpyz(hm->hmapname, heightmapname, sizeof(hm->hmapname));
	hm->heights = (unsigned short*)(hm+1);
	for (x = 0; x < size*size; x++)
	{
		hm->heights[x] = LittleShort(heightmap[x]);
	}
	hm->terrainsize = size;
	hm->terrainscale = worldsize;
	hm->heightscale = heightsize;
	hm->numsegs = numsegs;

	hm->shader = R_RegisterShader(basetexname,
				"{\n"
					"{\n"
						"map maps/test/ground.jpg\n"
					"}\n"
				"}\n"
			);

	hm->skyshader = R_RegisterCustom(va("skybox_%s", skyname), Shader_DefaultSkybox, NULL);


	mod->entities = COM_LoadHunkFile(entfile);

	if (qrenderer != QR_NONE)
	{
		hm->detailtexture = R_LoadHiResTexture(detailtexname, "", IF_NOGAMMA);

		for (x = 0; x < numsegs; x++)
		{
			int y;

			for (y = 0; y < numsegs; y++)
			{
				hm->textures[x+y*SECTIONS] = R_LoadHiResTexture(va("%s%02ix%02i%s", basetexname, x, y, exttexname), "", IF_CLAMP|IF_NOGAMMA);
				hm->modified[x+y*SECTIONS] = true;
			}
		}
	}

	mod->funcs.Trace				= Heightmap_Trace;
	mod->funcs.PointContents		= Heightmap_PointContents;

	mod->funcs.NativeContents		= Heightmap_NativeBoxContents;
	mod->funcs.NativeTrace			= Heightmap_NativeTrace;

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
