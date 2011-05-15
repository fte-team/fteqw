#include "quakedef.h"

#ifdef GLQUAKE
#ifdef RTLIGHTS

#include "glquake.h"
#include "shader.h"

#define SHADOWMAP_SIZE 512

#define nearplane	(16)

static int shadow_fbo_id;

static void Sh_DrawEntLighting(dlight_t *light, vec3_t colour);


struct {
	int numlights;
	int shadowsurfcount;

	int numfrustumculled;
	int numpvsculled;
	int numscissorculled;
} bench;








typedef struct {
	unsigned int count;
	unsigned int max;
	mesh_t **s;
} shadowmeshsurfs_t;
typedef struct shadowmesh_s {
	unsigned int numindicies;
	unsigned int maxindicies;
	index_t *indicies;

	unsigned int numverts;
	unsigned int maxverts;
	vecV_t *verts;

	//we also have a list of all the surfaces that this light lights.
	unsigned int numsurftextures;
	shadowmeshsurfs_t *litsurfs;

	unsigned int leafbytes;
	unsigned char *litleaves;
} shadowmesh_t;

/*state of the current shadow mesh*/
#define inc 128
int sh_shadowframe;
static int sh_firstindex;
static int sh_vertnum;		//vertex number (set to 0 at SH_Begin)
static shadowmesh_t *sh_shmesh, sh_tempshmesh;

/* functions to add geometry to the shadow mesh */
static void SHM_BeginQuads (void)
{
	sh_firstindex = sh_shmesh->numverts;
}
static void SHM_End (void)
{
	int i;
	i = (sh_shmesh->numindicies+(sh_vertnum/4)*6+inc+5)&~(inc-1);	//and a bit of padding
	if (sh_shmesh->maxindicies != i)
	{
		sh_shmesh->maxindicies = i;
		sh_shmesh->indicies = BZ_Realloc(sh_shmesh->indicies, i * sizeof(*sh_shmesh->indicies));
	}
	//add the extra triangles
	for (i = 0; i < sh_vertnum; i+=4)
	{
		sh_shmesh->indicies[sh_shmesh->numindicies++] = sh_firstindex + i+0;
		sh_shmesh->indicies[sh_shmesh->numindicies++] = sh_firstindex + i+1;
		sh_shmesh->indicies[sh_shmesh->numindicies++] = sh_firstindex + i+2;

		sh_shmesh->indicies[sh_shmesh->numindicies++] = sh_firstindex + i+0;
		sh_shmesh->indicies[sh_shmesh->numindicies++] = sh_firstindex + i+2;
		sh_shmesh->indicies[sh_shmesh->numindicies++] = sh_firstindex + i+3;
	}
	sh_vertnum = 0;
}
static void SHM_Vertex3fv (const GLfloat *v)
{
	int i;

//add the verts as we go
	i = (sh_shmesh->numverts+inc+5)&~(inc-1);	//and a bit of padding
	if (sh_shmesh->maxverts < i)
	{
		sh_shmesh->maxverts = i;
		sh_shmesh->verts = BZ_Realloc(sh_shmesh->verts, i * sizeof(*sh_shmesh->verts));
	}

	sh_shmesh->verts[sh_shmesh->numverts][0] = v[0];
	sh_shmesh->verts[sh_shmesh->numverts][1] = v[1];
	sh_shmesh->verts[sh_shmesh->numverts][2] = v[2];

	sh_vertnum++;
	sh_shmesh->numverts++;


	if (sh_vertnum == 4)
	{
		SHM_End();
		sh_firstindex = sh_shmesh->numverts;
	}
}

void SHM_TriangleFan(int numverts, vecV_t *verts, vec3_t lightorg, float pd)
{
	int v, i, idxs;
	float *v1;
	vec3_t v3;
	vecV_t *outv;
	index_t *outi;

	/*make sure there's space*/
	v = (sh_shmesh->numverts+numverts*2 + inc)&~(inc-1);	//and a bit of padding
	if (sh_shmesh->maxverts < v)
	{
		sh_shmesh->maxverts = v;
		sh_shmesh->verts = BZ_Realloc(sh_shmesh->verts, v * sizeof(*sh_shmesh->verts));
	}
	outv = sh_shmesh->verts + sh_shmesh->numverts;

	for (v = 0; v < numverts; v++)
	{
		v1 = verts[v];
		VectorCopy(v1, outv[v]);

		v3[0] = ( v1[0]-lightorg[0] )*pd;
		v3[1] = ( v1[1]-lightorg[1] )*pd;
		v3[2] = ( v1[2]-lightorg[2] )*pd;

		outv[v+numverts][0] = v1[0]+v3[0];
		outv[v+numverts][1] = v1[1]+v3[1];
		outv[v+numverts][2] = v1[2]+v3[2];
	}

	idxs = (numverts-2)*3;
	/*now add the verts in a fan*/
	v = (sh_shmesh->numindicies+idxs*2+inc)&~(inc-1);	//and a bit of padding
	if (sh_shmesh->maxindicies < v)
	{
		sh_shmesh->maxindicies = v;
		sh_shmesh->indicies = BZ_Realloc(sh_shmesh->indicies, v * sizeof(*sh_shmesh->indicies));
	}
	outi = sh_shmesh->indicies + sh_shmesh->numindicies;

	for (v = 2, i = 0; v < numverts; v++, i+=3)
	{
		outi[i+0] = sh_shmesh->numverts;
		outi[i+1] = sh_shmesh->numverts+v-1;
		outi[i+2] = sh_shmesh->numverts+v;

		outi[i+0+idxs] = sh_shmesh->numverts+numverts+v;
		outi[i+1+idxs] = sh_shmesh->numverts+numverts+v-1;
		outi[i+2+idxs] = sh_shmesh->numverts+numverts;
	}

	/*we added this many*/
	sh_shmesh->numverts += numverts*2;
	sh_shmesh->numindicies += i*2;
}

static void SHM_Shadow_Cache_Surface(msurface_t *surf)
{
	int i;

	i = surf->texinfo->texture->wtexno;
	if (i < 0)
		return;

	if (sh_shmesh->litsurfs[i].count == sh_shmesh->litsurfs[i].max)
	{
		sh_shmesh->litsurfs[i].max += 64;
		sh_shmesh->litsurfs[i].s = BZ_Realloc(sh_shmesh->litsurfs[i].s, sizeof(void*)*(sh_shmesh->litsurfs[i].max));
	}
	sh_shmesh->litsurfs[i].s[sh_shmesh->litsurfs[i].count] = surf->mesh;
	sh_shmesh->litsurfs[i].count++;
}

static void SHM_Shadow_Cache_Leaf(mleaf_t *leaf)
{
	int i;

	i = leaf - cl.worldmodel->leafs;
	sh_shmesh->litleaves[i>>3] |= 1<<(i&7);
}

void SH_FreeShadowMesh(shadowmesh_t *sm)
{
	unsigned int i;
	for (i = 0; i < sm->numsurftextures; i++)
		Z_Free(sm->litsurfs[i].s);
	Z_Free(sm->litsurfs);
	Z_Free(sm->indicies);
	Z_Free(sm->verts);
	Z_Free(sm);
}

static void SHM_BeginShadowMesh(dlight_t *dl)
{
	unsigned int i;
	unsigned int lb;
	sh_vertnum = 0;

	lb = (cl.worldmodel->numleafs+7)/8;
	if (!dl->die || !dl->key)
	{
		sh_shmesh = dl->worldshadowmesh;
		if (!sh_shmesh || sh_shmesh->leafbytes != lb)
		{
			/*this shouldn't happen too often*/
			if (sh_shmesh)
			{
				SH_FreeShadowMesh(sh_shmesh);
			}

			/*Create a new shadowmesh for this light*/
			sh_shmesh = Z_Malloc(sizeof(*sh_shmesh) + lb);
			sh_shmesh->leafbytes = lb;
			sh_shmesh->litleaves = (unsigned char*)(sh_shmesh+1);

			dl->worldshadowmesh = sh_shmesh;
		}
		dl->rebuildcache = false;
	}
	else
	{
		sh_shmesh = &sh_tempshmesh;
		if (sh_shmesh->leafbytes != lb)
		{
			/*this happens on map changes*/
			sh_shmesh->leafbytes = lb;
			Z_Free(sh_shmesh->litleaves);
			sh_shmesh->litleaves = Z_Malloc(lb);
		}
	}
	sh_shmesh->maxverts = 0;
	sh_shmesh->numverts = 0;
	sh_shmesh->maxindicies = 0;
	sh_shmesh->numindicies = 0;

	if (sh_shmesh->numsurftextures != cl.worldmodel->numtextures)
	{
		if (sh_shmesh->litsurfs)
		{
			for (i = 0; i < sh_shmesh->numsurftextures; i++)
				Z_Free(sh_shmesh->litsurfs[i].s);
			Z_Free(sh_shmesh->litsurfs);
		}
		sh_shmesh->litsurfs = Z_Malloc(sizeof(shadowmeshsurfs_t)*cl.worldmodel->numtextures);
		sh_shmesh->numsurftextures=cl.worldmodel->numtextures;
	}
	else
	{
		for (i = 0; i < sh_shmesh->numsurftextures; i++)
			sh_shmesh->litsurfs[i].count = 0;
	}
}
static struct shadowmesh_s *SHM_FinishShadowMesh(dlight_t *dl)
{
	return sh_shmesh;
}


/*state of the world that is still to compile*/
static struct {
	short count;
	short count2;
	int next;
	int prev;
} edge[MAX_MAP_EDGES];
static int firstedge;

static void SHM_RecursiveWorldNodeQ1_r (dlight_t *dl, mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	int v;

	float		l, maxdist;
	int			j, s, t;
	vec3_t		impact;

	if (node->shadowframe != sh_shadowframe)
		return;

	if (node->contents == Q1CONTENTS_SOLID)
		return;		// solid


	//if light areabox is outside node, ignore node + children
	for (c = 0; c < 3; c++)
	{
		if (dl->origin[c] + dl->radius < node->minmaxs[c])
			return;
		if (dl->origin[c] - dl->radius > node->minmaxs[3+c])
			return;
	}

// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;
		SHM_Shadow_Cache_Leaf(pleaf);

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark++)->shadowframe = sh_shadowframe;
			} while (--c);
		}
		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = dl->origin[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = dl->origin[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = dl->origin[2] - plane->dist;
		break;
	default:
		dot = DotProduct (dl->origin, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	SHM_RecursiveWorldNodeQ1_r (dl, node->children[side]);

// draw stuff
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		{

			maxdist = dl->radius*dl->radius;

			for ( ; c ; c--, surf++)
			{
				if (surf->shadowframe != sh_shadowframe)
					continue;

//				if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
//					continue;		// wrong side

//				if (surf->flags & SURF_PLANEBACK)
//					continue;

				if (surf->flags & (SURF_DRAWALPHA | SURF_DRAWTILED))
				{	// no shadows
					continue;
				}

				//is the light on the right side?
				if (surf->flags & SURF_PLANEBACK)
				{//inverted normal.
					if (-DotProduct(surf->plane->normal, dl->origin)+surf->plane->dist >= dl->radius)
						continue;
				}
				else
				{
					if (DotProduct(surf->plane->normal, dl->origin)-surf->plane->dist >= dl->radius)
						continue;
				}

				//Yeah, you can blame LordHavoc for this alternate code here.
				for (j=0 ; j<3 ; j++)
					impact[j] = dl->origin[j] - surf->plane->normal[j]*dot;

				// clamp center of light to corner and check brightness
				l = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
				s = l+0.5;if (s < 0) s = 0;else if (s > surf->extents[0]) s = surf->extents[0];
				s = l - s;
				l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
				t = l+0.5;if (t < 0) t = 0;else if (t > surf->extents[1]) t = surf->extents[1];
				t = l - t;
				// compare to minimum light
				if ((s*s+t*t+dot*dot) < maxdist)
				{
					SHM_Shadow_Cache_Surface(surf);

					#define PROJECTION_DISTANCE (float)(dl->radius*2)//0x7fffffff

					//build a list of the edges that are to be drawn.
					for (v = 0; v < surf->numedges; v++)
					{
						int e, delta;
						e = cl.worldmodel->surfedges[surf->firstedge+v];
						//negative edge means backwards edge.
						if (e < 0)
						{
							e=-e;
							delta = -1;
						}
						else
						{
							delta = 1;
						}

						if (!edge[e].count)
						{
							if (firstedge)
								edge[firstedge].prev = e;
							edge[e].next = firstedge;
							edge[e].prev = 0;
							firstedge = e;
							edge[e].count = delta;
						}
						else
						{
							edge[e].count += delta;

							if (!edge[e].count)	//unlink
							{
								if (edge[e].next)
								{
									edge[edge[e].next].prev = edge[e].prev;
								}
								if (edge[e].prev)
									edge[edge[e].prev].next = edge[e].next;
								else
									firstedge = edge[e].next;
							}
						}
					}

					SHM_TriangleFan(surf->mesh->numvertexes, surf->mesh->xyz_array, dl->origin, PROJECTION_DISTANCE);
				}
			}
		}
	}

// recurse down the back side
	SHM_RecursiveWorldNodeQ1_r (dl, node->children[!side]);
}

static void SHM_RecursiveWorldNodeQ2_r (dlight_t *dl, mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	int v;

	float		l, maxdist;
	int			j, s, t;
	vec3_t		impact;

	if (node->shadowframe != sh_shadowframe)
		return;

	if (node->contents == Q2CONTENTS_SOLID)
		return;		// solid


	//if light areabox is outside node, ignore node + children
	for (c = 0; c < 3; c++)
	{
		if (dl->origin[c] + dl->radius < node->minmaxs[c])
			return;
		if (dl->origin[c] - dl->radius > node->minmaxs[3+c])
			return;
	}

// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;
		SHM_Shadow_Cache_Leaf(pleaf);

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark++)->shadowframe = sh_shadowframe;
			} while (--c);
		}
		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = dl->origin[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = dl->origin[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = dl->origin[2] - plane->dist;
		break;
	default:
		dot = DotProduct (dl->origin, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	SHM_RecursiveWorldNodeQ2_r (dl, node->children[side]);

// draw stuff
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		{

			maxdist = dl->radius*dl->radius;

			for ( ; c ; c--, surf++)
			{
				if (surf->shadowframe != sh_shadowframe)
					continue;

//				if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
//					continue;		// wrong side

//				if (surf->flags & SURF_PLANEBACK)
//					continue;

				if (surf->flags & (SURF_DRAWALPHA | SURF_DRAWTILED))
				{	// no shadows
					continue;
				}

				//is the light on the right side?
				if (surf->flags & SURF_PLANEBACK)
				{//inverted normal.
					if (-DotProduct(surf->plane->normal, dl->origin)+surf->plane->dist >= dl->radius)
						continue;
				}
				else
				{
					if (DotProduct(surf->plane->normal, dl->origin)-surf->plane->dist >= dl->radius)
						continue;
				}

				//Yeah, you can blame LordHavoc for this alternate code here.
				for (j=0 ; j<3 ; j++)
					impact[j] = dl->origin[j] - surf->plane->normal[j]*dot;

				// clamp center of light to corner and check brightness
				l = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
				s = l+0.5;if (s < 0) s = 0;else if (s > surf->extents[0]) s = surf->extents[0];
				s = l - s;
				l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
				t = l+0.5;if (t < 0) t = 0;else if (t > surf->extents[1]) t = surf->extents[1];
				t = l - t;
				// compare to minimum light
				if ((s*s+t*t+dot*dot) < maxdist)
				{
					SHM_Shadow_Cache_Surface(surf);



	#define PROJECTION_DISTANCE (float)(dl->radius*2)//0x7fffffff

					//build a list of the edges that are to be drawn.
					for (v = 0; v < surf->numedges; v++)
					{
						int e, delta;
						e = cl.worldmodel->surfedges[surf->firstedge+v];
						//negative edge means backwards edge.
						if (e < 0)
						{
							e=-e;
							delta = -1;
						}
						else
						{
							delta = 1;
						}

						if (!edge[e].count)
						{
							if (firstedge)
								edge[firstedge].prev = e;
							edge[e].next = firstedge;
							edge[e].prev = 0;
							firstedge = e;
							edge[e].count = delta;
						}
						else
						{
							edge[e].count += delta;

							if (!edge[e].count)	//unlink
							{
								if (edge[e].next)
								{
									edge[edge[e].next].prev = edge[e].prev;
								}
								if (edge[e].prev)
									edge[edge[e].prev].next = edge[e].next;
								else
									firstedge = edge[e].next;
							}
						}
					}

					SHM_TriangleFan(surf->mesh->numvertexes, surf->mesh->xyz_array, dl->origin, PROJECTION_DISTANCE);
				}
			}
		}
	}

// recurse down the back side
	SHM_RecursiveWorldNodeQ2_r (dl, node->children[!side]);
}

#ifdef Q2BSPS
static void SHM_MarkLeavesQ2(dlight_t *dl, unsigned char *lvis, unsigned char *vvis)
{
	mnode_t *node;
	int i;
	mleaf_t *leaf;
	int cluster;
	sh_shadowframe++;

	if (!dl->die)
	{
		//static
		//variation on mark leaves
		for (i=0,leaf=cl.worldmodel->leafs ; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
			cluster = leaf->cluster;
			if (cluster == -1)
				continue;
			if (lvis[cluster>>3] & (1<<(cluster&7)))// && vvis[cluster>>3] & (1<<(cluster&7)))
			{
				node = (mnode_t *)leaf;
				do
				{
					if (node->shadowframe == sh_shadowframe)
						break;
					node->shadowframe = sh_shadowframe;
					node = node->parent;
				} while (node);
			}
		}
	}
	else
	{
		//dynamic lights will be discarded after this frame anyway, so only include leafs that are visible
		//variation on mark leaves
		for (i=0,leaf=cl.worldmodel->leafs ; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
			cluster = leaf->cluster;
			if (cluster == -1)
				continue;
			if (lvis[cluster>>3] & vvis[cluster>>3] & (1<<(cluster&7)))
			{
				node = (mnode_t *)leaf;
				do
				{
					if (node->shadowframe == sh_shadowframe)
						break;
					node->shadowframe = sh_shadowframe;
					node = node->parent;
				} while (node);
			}
		}
	}
}
#endif

static void SHM_MarkLeavesQ1(dlight_t *dl, unsigned char *lvis, unsigned char *vvis)
{
	mnode_t *node;
	int i;
	sh_shadowframe++;

	if (!dl->die || !vvis)
	{
		//static
		//variation on mark leaves
		for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		{
			if (lvis[i>>3] & (1<<(i&7)))// && vvis[i>>3] & (1<<(i&7)))
			{
				node = (mnode_t *)&cl.worldmodel->leafs[i+1];
				do
				{
					if (node->shadowframe == sh_shadowframe)
						break;
					node->shadowframe = sh_shadowframe;
					node = node->parent;
				} while (node);
			}
		}
	}
	else
	{
		//dynamic lights will be discarded after this frame anyway, so only include leafs that are visible
		//variation on mark leaves
		for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		{
			if (lvis[i>>3] & vvis[i>>3] & (1<<(i&7)))
			{
				node = (mnode_t *)&cl.worldmodel->leafs[i+1];
				do
				{
					if (node->shadowframe == sh_shadowframe)
						break;
					node->shadowframe = sh_shadowframe;
					node = node->parent;
				} while (node);
			}
		}
	}
}

#ifdef Q3BSPS
void SHM_RecursiveWorldNodeQ3_r (dlight_t *dl, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	**msurf;
	msurface_t	*surf;
	mleaf_t		*leaf;
	int			i;

	if (node->contents != -1)
	{
		leaf = (mleaf_t *)node;
		SHM_Shadow_Cache_Leaf(leaf);

	// mark the polygons
		msurf = leaf->firstmarksurface;
		for (i=0 ; i<leaf->nummarksurfaces ; i++, msurf++)
		{
			surf = *msurf;

			//only check each surface once. it can appear in multiple leafs.
			if (surf->shadowframe == sh_shadowframe)
				continue;
			surf->shadowframe = sh_shadowframe;

			SHM_Shadow_Cache_Surface(surf);
		}
		return;
	}

	splitplane = node->plane;
	dist = DotProduct (dl->origin, splitplane->normal) - splitplane->dist;

	if (dist > dl->radius)
	{
		SHM_RecursiveWorldNodeQ3_r (dl, node->children[0]);
		return;
	}
	if (dist < -dl->radius)
	{
		SHM_RecursiveWorldNodeQ3_r (dl, node->children[1]);
		return;
	}
	SHM_RecursiveWorldNodeQ3_r (dl, node->children[0]);
	SHM_RecursiveWorldNodeQ3_r (dl, node->children[1]);
}
#endif

static void SHM_ComposeVolume_BruteForce(dlight_t *dl)
{
	shadowmeshsurfs_t *sms;
	unsigned int tno;
	unsigned int sno;
	unsigned int vno, vno2;
	unsigned int fvert, lvert;
	mesh_t *sm;
	for (tno = 0; tno < sh_shmesh->numsurftextures; tno++)
	{
		sms = &sh_shmesh->litsurfs[tno];
		if (!sms->count)
			continue;
		for (sno = 0; sno < sms->count; sno++)
		{
			sm = sms->s[sno];

			if (sm->istrifan)
			{
				//planer poly
//if ((rand()&63)!=63)
//continue;
				fvert = sh_shmesh->numverts;

				SHM_TriangleFan(sm->numvertexes, sm->xyz_array, dl->origin, PROJECTION_DISTANCE);

				vno = (sh_shmesh->numindicies+sm->numvertexes*6);	//and a bit of padding
				if (sh_shmesh->maxindicies < vno)
				{
					sh_shmesh->maxindicies = vno;
					sh_shmesh->indicies = BZ_Realloc(sh_shmesh->indicies, vno * sizeof(*sh_shmesh->indicies));
				}
				lvert = fvert + sm->numvertexes*2-1;
				for (vno = 0; vno < sm->numvertexes; vno++)
				{
					if (vno == sm->numvertexes-1)
						vno2 = 0;
					else
						vno2 = vno+1;
					sh_shmesh->indicies[sh_shmesh->numindicies++] = fvert+vno;
					sh_shmesh->indicies[sh_shmesh->numindicies++] = lvert-vno;
					sh_shmesh->indicies[sh_shmesh->numindicies++] = fvert+vno2;

					sh_shmesh->indicies[sh_shmesh->numindicies++] = lvert-vno;
					sh_shmesh->indicies[sh_shmesh->numindicies++] = lvert-vno2;
					sh_shmesh->indicies[sh_shmesh->numindicies++] = fvert+vno2;
				}
			}
			else
			{
				/*each triangle may or may not face the light*/
			}
		}
	}


	/*	unsigned int numindicies;
	unsigned int maxindicies;
	index_t *indicies;

	unsigned int numverts;
	unsigned int maxverts;
	vec3_t *verts;

	//we also have a list of all the surfaces that this light lights.
	unsigned int numsurftextures;
	shadowmeshsurfs_t *litsurfs;

	unsigned int leafbytes;
	unsigned char *litleaves;
	*/
}

static struct shadowmesh_s *SHM_BuildShadowVolumeMesh(dlight_t *dl, unsigned char *lvis, unsigned char *vvis)
{
	float *v1, *v2;
	vec3_t v3, v4;

	if (dl->worldshadowmesh && !dl->rebuildcache)
		return dl->worldshadowmesh;

	if (cl.worldmodel->fromgame == fg_quake || cl.worldmodel->fromgame == fg_halflife)
	{
		SHM_BeginShadowMesh(dl);
		SHM_MarkLeavesQ1(dl, lvis, vvis);
		SHM_RecursiveWorldNodeQ1_r(dl, cl.worldmodel->nodes);
	}
#ifdef Q3BSPS
	else if (cl.worldmodel->fromgame == fg_quake3)
	{
		SHM_BeginShadowMesh(dl);
		sh_shadowframe++;
		SHM_RecursiveWorldNodeQ3_r(dl, cl.worldmodel->nodes);
		SHM_ComposeVolume_BruteForce(dl);
		return SHM_FinishShadowMesh(dl);
//		SHM_RecursiveWorldNodeQ3_r(cl.worldmodel->nodes);

		//if generating shadow volumes too:
		// decompose the shadow-casting faces into triangles
		// find neighbours
		// emit front faces (clip back faces to the light's cube?)
		// emit edges where there were no neighbours
	}
#endif
#ifdef Q2BSPS
	else if (cl.worldmodel->fromgame == fg_quake2)
	{
		SHM_BeginShadowMesh(dl);
		SHM_MarkLeavesQ2(dl, lvis, vvis);
		SHM_RecursiveWorldNodeQ2_r(dl, cl.worldmodel->nodes);
	}
#endif
	else
		return NULL;

	SHM_BeginQuads();
	while(firstedge)
	{
		//border
		v1 = cl.worldmodel->vertexes[cl.worldmodel->edges[firstedge].v[0]].position;
		v2 = cl.worldmodel->vertexes[cl.worldmodel->edges[firstedge].v[1]].position;

		//get positions of v3 and v4 based on the light position
		v3[0] = v1[0] + ( v1[0]-dl->origin[0] )*PROJECTION_DISTANCE;
		v3[1] = v1[1] + ( v1[1]-dl->origin[1] )*PROJECTION_DISTANCE;
		v3[2] = v1[2] + ( v1[2]-dl->origin[2] )*PROJECTION_DISTANCE;

		v4[0] = v2[0] + ( v2[0]-dl->origin[0] )*PROJECTION_DISTANCE;
		v4[1] = v2[1] + ( v2[1]-dl->origin[1] )*PROJECTION_DISTANCE;
		v4[2] = v2[2] + ( v2[2]-dl->origin[2] )*PROJECTION_DISTANCE;

		if (edge[firstedge].count > 0)
		{
			SHM_Vertex3fv(v3);
			SHM_Vertex3fv(v4);
			SHM_Vertex3fv(v2);
			SHM_Vertex3fv(v1);
		}
		else
		{
			SHM_Vertex3fv(v1);
			SHM_Vertex3fv(v2);
			SHM_Vertex3fv(v4);
			SHM_Vertex3fv(v3);
		}
		edge[firstedge].count=0;

		firstedge = edge[firstedge].next;
	}
	SHM_End();

	firstedge=0;

	return SHM_FinishShadowMesh(dl);
}













static qboolean Sh_VisOverlaps(qbyte *v1, qbyte *v2)
{
	int i, m;
	m = (cl.worldmodel->numleafs-1)>>3;
	for (i=0 ; i<m ; i++)
	{
		if (v1[i] & v2[i])
			return true;
	}
	return false;
}

static qboolean Sh_LeafInView(qbyte *lightvis, qbyte *vvis)
{
	int i;
	int m = (cl.worldmodel->numleafs);
	mleaf_t *wl = cl.worldmodel->leafs;
	unsigned char lv;

	/*we can potentially walk off the end of the leafs, but lightvis shouldn't be set for those*/


	for (i = 0; i < m; i += 1<<3)
	{
		lv = lightvis[i>>3];// & vvis[i>>3];
		if (!lv)
			continue;
		if ((lv&0x01) && wl[i+0].visframe == r_visframecount) return true;
		if ((lv&0x02) && wl[i+1].visframe == r_visframecount) return true;
		if ((lv&0x04) && wl[i+2].visframe == r_visframecount) return true;
		if ((lv&0x08) && wl[i+3].visframe == r_visframecount) return true;
		if ((lv&0x10) && wl[i+4].visframe == r_visframecount) return true;
		if ((lv&0x20) && wl[i+5].visframe == r_visframecount) return true;
		if ((lv&0x40) && wl[i+6].visframe == r_visframecount) return true;
		if ((lv&0x80) && wl[i+7].visframe == r_visframecount) return true;
	}

	return false;
}

static void Sh_Scissor (int x, int y, int width, int height)
{
#if 0	//visible scissors
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho  (0, glwidth, glheight, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
//	GL_Set2D();

	glColor4f(1,1,1,1);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
	glDisable(GL_TEXTURE_2D);
	GL_TexEnv(GL_REPLACE);

	glBegin(GL_LINE_LOOP);
	glVertex2f(x, y);
	glVertex2f(x+glwidth, y);
	glVertex2f(x+glwidth, y+glheight);
	glVertex2f(x, y+glheight);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
#endif
	qglScissor(x, vid.pixelheight - (y + height),width,height);
}

#define BoxesOverlap(a,b,c,d) ((a)[0] <= (d)[0] && (b)[0] >= (c)[0] && (a)[1] <= (d)[1] && (b)[1] >= (c)[1] && (a)[2] <= (d)[2] && (b)[2] >= (c)[2])
static qboolean Sh_ScissorForBox(vec3_t mins, vec3_t maxs)
{
	int i, ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2, x, y, f;
	vec3_t smins, smaxs;
	vec4_t v, v2;
	int r_view_x = 0;
	int r_view_y = 0;
	int r_view_width = vid.pixelwidth;
	int r_view_height = vid.pixelheight;
	if (0)//!r_shadow_scissor.integer)
	{
		Sh_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
		return false;
	}
	// if view is inside the box, just say yes it's visible
	if (BoxesOverlap(r_refdef.vieworg, r_refdef.vieworg, mins, maxs))
	{
		Sh_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
		return false;
	}
	for (i = 0;i < 3;i++)
	{
		if (vpn[i] >= 0)
		{
			v[i] = mins[i];
			v2[i] = maxs[i];
		}
		else
		{
			v[i] = maxs[i];
			v2[i] = mins[i];
		}
	}
	f = DotProduct(vpn, r_refdef.vieworg) + 1;
	if (DotProduct(vpn, v2) <= f)
	{
		// entirely behind nearclip plane
		Sh_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
		return true;
	}
	if (DotProduct(vpn, v) >= f)
	{
		// entirely infront of nearclip plane
		x1 = y1 = x2 = y2 = 0;
		for (i = 0;i < 8;i++)
		{
			v[0] = (i & 1) ? mins[0] : maxs[0];
			v[1] = (i & 2) ? mins[1] : maxs[1];
			v[2] = (i & 4) ? mins[2] : maxs[2];
			v[3] = 1.0f;
			Matrix4_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y);
			v2[0]*=r_view_width;
			v2[1]*=r_view_height;
//			GL_TransformToScreen(v, v2);
			//Con_Printf("%.3f %.3f %.3f %.3f transformed to %.3f %.3f %.3f %.3f\n", v[0], v[1], v[2], v[3], v2[0], v2[1], v2[2], v2[3]);
			x = v2[0];
			y = v2[1];
			if (i)
			{
				if (x1 > x) x1 = x;
				if (x2 < x) x2 = x;
				if (y1 > y) y1 = y;
				if (y2 < y) y2 = y;
			}
			else
			{
				x1 = x2 = x;
				y1 = y2 = y;
			}
		}
	}
	else
	{
		// clipped by nearclip plane
		// this is nasty and crude...
		// create viewspace bbox
		i = 0;
		/*unrolled the first iteration to avoid warnings*/
		v[0] = ((i & 1) ? mins[0] : maxs[0]) - r_refdef.vieworg[0];
		v[1] = ((i & 2) ? mins[1] : maxs[1]) - r_refdef.vieworg[1];
		v[2] = ((i & 4) ? mins[2] : maxs[2]) - r_refdef.vieworg[2];
		v2[0] = DotProduct(v, vright);
		v2[1] = DotProduct(v, vup);
		v2[2] = DotProduct(v, vpn);
		smins[0] = smaxs[0] = v2[0];
		smins[1] = smaxs[1] = v2[1];
		smins[2] = smaxs[2] = v2[2];
		for (i = 1;i < 8;i++)
		{
			v[0] = ((i & 1) ? mins[0] : maxs[0]) - r_refdef.vieworg[0];
			v[1] = ((i & 2) ? mins[1] : maxs[1]) - r_refdef.vieworg[1];
			v[2] = ((i & 4) ? mins[2] : maxs[2]) - r_refdef.vieworg[2];
			v2[0] = DotProduct(v, vright);
			v2[1] = DotProduct(v, vup);
			v2[2] = DotProduct(v, vpn);
			if (smins[0] > v2[0]) smins[0] = v2[0];
			if (smaxs[0] < v2[0]) smaxs[0] = v2[0];
			if (smins[1] > v2[1]) smins[1] = v2[1];
			if (smaxs[1] < v2[1]) smaxs[1] = v2[1];
			if (smins[2] > v2[2]) smins[2] = v2[2];
			if (smaxs[2] < v2[2]) smaxs[2] = v2[2];
		}
		// now we have a bbox in viewspace
		// clip it to the view plane
		if (smins[2] < 1)
			smins[2] = 1;
		// return true if that culled the box
		if (smins[2] >= smaxs[2])
			return true;
		// ok some of it is infront of the view, transform each corner back to
		// worldspace and then to screenspace and make screen rect
		// initialize these variables just to avoid compiler warnings
		x1 = y1 = x2 = y2 = 0;
		for (i = 0;i < 8;i++)
		{
			v2[0] = (i & 1) ? smins[0] : smaxs[0];
			v2[1] = (i & 2) ? smins[1] : smaxs[1];
			v2[2] = (i & 4) ? smins[2] : smaxs[2];
			v[0] = v2[0] * vright[0] + v2[1] * vup[0] + v2[2] * vpn[0] + r_refdef.vieworg[0];
			v[1] = v2[0] * vright[1] + v2[1] * vup[1] + v2[2] * vpn[1] + r_refdef.vieworg[1];
			v[2] = v2[0] * vright[2] + v2[1] * vup[2] + v2[2] * vpn[2] + r_refdef.vieworg[2];
			v[3] = 1.0f;
			Matrix4_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y);
			v2[0]*=r_view_width;
			v2[1]*=r_view_height;
			//Con_Printf("%.3f %.3f %.3f %.3f transformed to %.3f %.3f %.3f %.3f\n", v[0], v[1], v[2], v[3], v2[0], v2[1], v2[2], v2[3]);
			x = v2[0];
			y = v2[1];
			if (i)
			{
				if (x1 > x) x1 = x;
				if (x2 < x) x2 = x;
				if (y1 > y) y1 = y;
				if (y2 < y) y2 = y;
			}
			else
			{
				x1 = x2 = x;
				y1 = y2 = y;
			}
		}
#if 1
		// this code doesn't handle boxes with any points behind view properly
		x1 = 1000;x2 = -1000;
		y1 = 1000;y2 = -1000;
		for (i = 0;i < 8;i++)
		{
			v[0] = (i & 1) ? mins[0] : maxs[0];
			v[1] = (i & 2) ? mins[1] : maxs[1];
			v[2] = (i & 4) ? mins[2] : maxs[2];
			v[3] = 1.0f;
			Matrix4_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y);
			v2[0]*=r_view_width;
			v2[1]*=r_view_height;
			//Con_Printf("%.3f %.3f %.3f %.3f transformed to %.3f %.3f %.3f %.3f\n", v[0], v[1], v[2], v[3], v2[0], v2[1], v2[2], v2[3]);
			if (v2[2] > 0)
			{
				x = v2[0];
				y = v2[1];

				if (x1 > x) x1 = x;
				if (x2 < x) x2 = x;
				if (y1 > y) y1 = y;
				if (y2 < y) y2 = y;
			}
		}
#endif
	}
	ix1 = x1 - 1.0f;
	iy1 = y1 - 1.0f;
	ix2 = x2 + 1.0f;
	iy2 = y2 + 1.0f;
	//Con_Printf("%f %f %f %f\n", x1, y1, x2, y2);
	if (ix1 < r_view_x) ix1 = r_view_x;
	if (iy1 < r_view_y) iy1 = r_view_y;
	if (ix2 > r_view_x + r_view_width) ix2 = r_view_x + r_view_width;
	if (iy2 > r_view_y + r_view_height) iy2 = r_view_y + r_view_height;
	if (ix2 <= ix1 || iy2 <= iy1)
		return true;
	// set up the scissor rectangle
	qglScissor(ix1, iy1, ix2 - ix1, iy2 - iy1);
	//qglEnable(GL_SCISSOR_TEST);
	return false;
}


void GL_BeginRenderBuffer_DepthOnly(texid_t depthtexture)
{
	if (gl_config.ext_framebuffer_objects)
	{
		if (!shadow_fbo_id)
		{
			qglGenRenderbuffersEXT(1, &shadow_fbo_id);
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo_id);
			qglDrawBuffer(GL_NONE);
			qglReadBuffer(GL_NONE);
		}
		else
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo_id);

		if (TEXVALID(depthtexture))
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depthtexture.num, 0);
	}
}
void GL_EndRenderBuffer_DepthOnly(texid_t depthtexture, int texsize)
{
	if (gl_config.ext_framebuffer_objects)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}
	else
	{
		GL_MTBind(0, GL_TEXTURE_2D, depthtexture);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);
	}
}

static void Sh_GenShadowFace(dlight_t *l, shadowmesh_t *smesh, int face, float proj[16])
{
	float mvm[16];
	vec3_t t1,t2;

	int smsize = SHADOWMAP_SIZE;
	int tno;
	mesh_t *m;
	texture_t *tex;

//	qglDepthRange(0, 1);

	if (l->fov)
		qglViewport (0, 0, smsize, smsize);
	else
		qglViewport (((face/2)*smsize)/3, ((face&1)*smsize)/2, smsize/3, smsize/2);

	switch(face)
	{
	case 0:
		//forward
		Matrix4_ModelViewMatrixFromAxis(mvm, l->axis[0], l->axis[1], l->axis[2], l->origin);
		break;
	case 1:
		//back
		VectorNegate(l->axis[0], t1);
		VectorNegate(l->axis[1], t2);
		Matrix4_ModelViewMatrixFromAxis(mvm, t1, t2, l->axis[2], l->origin);
		break;
	case 2:
		//left
		VectorNegate(l->axis[1], t1);
		VectorNegate(l->axis[0], t2);
		Matrix4_ModelViewMatrixFromAxis(mvm, l->axis[1], t2, l->axis[2], l->origin);
		break;
	case 3:
		//right
		VectorNegate(l->axis[1], t1);
		Matrix4_ModelViewMatrixFromAxis(mvm, t1, l->axis[0], l->axis[2], l->origin);
		break;
	case 4:
		//up
		VectorNegate(l->axis[0], t2);
		Matrix4_ModelViewMatrixFromAxis(mvm, l->axis[2], l->axis[1], t2, l->origin);
		break;
	case 5:
		//down
		VectorNegate(l->axis[2], t1);
		Matrix4_ModelViewMatrixFromAxis(mvm, t1, l->axis[1], l->axis[0], l->origin);
		break;
	}

	qglMatrixMode(GL_MODELVIEW);
	qglLoadMatrixf(mvm);

	R_SetFrustum(proj, mvm);

	if (smesh)
	for (tno = 0; tno < smesh->numsurftextures; tno++)
	{
		m = NULL;
		if (!smesh->litsurfs[tno].count)
			continue;
		tex = cl.worldmodel->textures[tno];
		BE_DrawMesh_List(tex->shader, smesh->litsurfs[tno].count, smesh->litsurfs[tno].s, &tex->vbo, &tex->shader->defaulttextures, 0);
	}

	BE_SelectMode(BEM_DEPTHONLY);
	BE_BaseEntTextures();

	if (0)
	{
		int i;
		static float depth[SHADOWMAP_SIZE*SHADOWMAP_SIZE];
		qglReadPixels(0, 0, smsize, smsize,
			GL_DEPTH_COMPONENT, GL_FLOAT, depth);
		for (i = SHADOWMAP_SIZE*SHADOWMAP_SIZE; i --> 0; )
		{
			if (depth[i] == 1)
				*((unsigned int*)depth+i) = 0;
			else
				*((unsigned int*)depth+i) = 0xff000000|((((unsigned char)(int)(depth[i]*128)))*0x10101);
		}
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			smsize, smsize, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, depth);

		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
}

void Sh_Shutdown(void)
{
	if (shadow_fbo_id)
	{
		qglDeleteRenderbuffersEXT(1, &shadow_fbo_id);
		shadow_fbo_id = 0;
	}
}

void Sh_GenShadowMap (dlight_t *l,  qbyte *lvis)
{
	int f;
	int smsize = SHADOWMAP_SIZE;
	float proj[16];

	shadowmesh_t *smesh;

	if (!TEXVALID(l->stexture))
	{
		l->stexture = GL_AllocNewTexture(smsize, smsize);

		GL_MTBind(0, GL_TEXTURE_2D, l->stexture);
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32_ARB, smsize, smsize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	//	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, smsize, smsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

	smesh = SHM_BuildShadowVolumeMesh(l, lvis, NULL);

	/*polygon offsets. urgh.*/
	qglEnable(GL_POLYGON_OFFSET_FILL);
	qglPolygonOffset(5, 25);
	BE_SetupForShadowMap();

	/*set framebuffer*/
	GL_BeginRenderBuffer_DepthOnly(l->stexture);
	qglClear (GL_DEPTH_BUFFER_BIT);

	if (l->fov)
	{
		Matrix4_Projection_Far(proj, l->fov, l->fov, nearplane, l->radius);
		qglMatrixMode(GL_PROJECTION);
		qglLoadMatrixf(proj);

		/*single face*/
		Sh_GenShadowFace(l, smesh, 0, proj);
	}
	else
	{
		Matrix4_Projection_Far(proj, 90, 90, nearplane, l->radius);
		qglMatrixMode(GL_PROJECTION);
		qglLoadMatrixf(proj);

		/*generate faces*/
		for (f = 0; f < 6; f++)
		{
			Sh_GenShadowFace(l, smesh, f, proj);
		}
	}
	/*end framebuffer*/
	GL_EndRenderBuffer_DepthOnly(l->stexture, smsize);

	qglDisable(GL_POLYGON_OFFSET_FILL);

	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf(r_refdef.m_projection);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadMatrixf(r_refdef.m_view);

	qglViewport(r_refdef.pxrect.x, vid.pixelheight - r_refdef.pxrect.y, r_refdef.pxrect.width, r_refdef.pxrect.height);

	R_SetFrustum(r_refdef.m_projection, r_refdef.m_view);
}

static float shadowprojectionbias[16] =
{
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.5f, 0.0f,
	0.5f, 0.5f, 0.4993f, 1.0f
};

static void Sh_DrawShadowMapLight(dlight_t *l, vec3_t colour, qbyte *vvis)
{
	float t[16];
	float bp[16];
	float proj[16], view[16];
	vec3_t biasorg;
	int ve;
	vec3_t mins, maxs;
	qbyte *lvis;
	qbyte	lvisb[MAX_MAP_LEAFS/8];

	if (R_CullSphere(l->origin, l->radius))
	{
		bench.numfrustumculled++;
		return;	//this should be the more common case
	}

	mins[0] = l->origin[0] - l->radius;
	mins[1] = l->origin[1] - l->radius;
	mins[2] = l->origin[2] - l->radius;

	maxs[0] = l->origin[0] + l->radius;
	maxs[1] = l->origin[1] + l->radius;
	maxs[2] = l->origin[2] + l->radius;

	if (Sh_ScissorForBox(mins, maxs))
	{
		bench.numscissorculled++;
		return;
	}

    if (l->worldshadowmesh)
    {
		lvis = l->worldshadowmesh->litleaves;
        //fixme: check head node first?
        if (!Sh_LeafInView(l->worldshadowmesh->litleaves, vvis))
        {
                bench.numpvsculled++;
                return;
        }
	}
	else
	{
		int leaf;
		leaf = cl.worldmodel->funcs.LeafnumForPoint(cl.worldmodel, l->origin);
		lvis = cl.worldmodel->funcs.LeafPVS(cl.worldmodel, leaf, lvisb, sizeof(lvisb));
		if (!Sh_VisOverlaps(lvis, vvis))	//The two viewing areas do not intersect.
		{
			bench.numpvsculled++;
			return;
		}
	}

	Sh_GenShadowMap(l, lvis);

	if (l->fov)
		Matrix4_Projection_Far(proj, l->fov, l->fov, nearplane, l->radius);
	else
		Matrix4_Projection_Far(proj, 90, 90, nearplane, l->radius);
	VectorMA(l->origin, 0, l->axis[0], biasorg);
	Matrix4_ModelViewMatrixFromAxis(view, l->axis[0], l->axis[1], l->axis[2], l->origin);

	//bp = shadowprojectionbias*proj*view;
	Matrix4_Multiply(shadowprojectionbias, proj, t);
	Matrix4_Multiply(t, view, bp);

	t[0] = bp[0];
	t[1] = bp[4];
	t[2] = bp[8];
	t[3] = bp[12];
	t[4] = bp[1];
	t[5] = bp[5];
	t[6] = bp[9];
	t[7] = bp[13];
	t[8] = bp[2];
	t[9] = bp[6];
	t[10] = bp[10];
	t[11] = bp[14];
	t[12] = bp[3];
	t[13] = bp[7];
	t[14] = bp[11];
	t[15] = bp[15];

	bench.numlights++;

	qglMatrixMode(GL_TEXTURE);
	GL_MTBind(7, GL_TEXTURE_2D, l->stexture);

//	qglEnable(GL_TEXTURE_2D);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
	qglTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE_ARB, GL_LUMINANCE);
	qglLoadMatrixf(bp);
	qglMatrixMode(GL_MODELVIEW);

	GL_SelectTexture(0);

	ve = 0;

	BE_SelectDLight(l, colour);
	BE_SelectMode(l->fov?BEM_SMAPLIGHTSPOT:BEM_SMAPLIGHT);
	Sh_DrawEntLighting(l, colour);

	GL_SelectTexture(7);
	qglDisable(GL_TEXTURE_2D);
	qglMatrixMode(GL_TEXTURE);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
}





// warning: ‘Sh_WorldLightingPass’ defined but not used
/*
static void Sh_WorldLightingPass(void)
{
	msurface_t *s;
	int i;
	int ve;

	ve = 0;
	for (i = 0; i < cl.worldmodel->numsurfaces; i++)
	{
		s = &cl.worldmodel->surfaces[i];
		if(s->visframe != r_framecount)
			continue;

		if (ve != s->texinfo->texture->vbo.vboe)
		{
			ve = s->texinfo->texture->vbo.vboe;

			GL_SelectVBO(s->texinfo->texture->vbo.vbocoord);
			GL_SelectEBO(s->texinfo->texture->vbo.vboe);
			qglVertexPointer(3, GL_FLOAT, sizeof(vecV_t), s->texinfo->texture->vbo.coord);
		}
		qglDrawRangeElements(GL_TRIANGLES, s->mesh->vbofirstvert, s->mesh->numvertexes, s->mesh->numindexes, GL_INDEX_TYPE, (index_t*)(s->mesh->vbofirstelement*sizeof(index_t)));
		RQuantAdd(RQUANT_LITFACES, s->mesh->numindexes);
	}
}
*/

/*
draws faces facing the light
Note: Backend mode must have been selected in advance, as must the light to light from
*/
static void Sh_DrawEntLighting(dlight_t *light, vec3_t colour)
{
	int tno;
	texture_t *tex;
	shadowmesh_t *sm;

	sm = light->worldshadowmesh;
	if (light->rebuildcache)
		sm = &sh_tempshmesh;
	if (sm)
	{
		for (tno = 0; tno < sm->numsurftextures; tno++)
		{
			if (!sm->litsurfs[tno].count)
				continue;
			tex = cl.worldmodel->textures[tno];
			if (tex->shader->flags & SHADER_NODLIGHT)
				continue;
			BE_DrawMesh_List(tex->shader, sm->litsurfs[tno].count, sm->litsurfs[tno].s, &tex->vbo, &tex->shader->defaulttextures, 0);
		}

		BE_BaseEntTextures();
	}
}


#define PROJECTION_DISTANCE (float)(dl->radius*2)//0x7fffffff
/*Fixme: this is brute forced*/
#pragma message("brush shadows are bruteforced")
static void Sh_DrawBrushModelShadow(dlight_t *dl, entity_t *e)
{
	int v;
	float *v1, *v2;
	vec3_t v3, v4;
	vec3_t lightorg;

	int i;
	model_t *model;
	msurface_t *surf;

	if (BE_LightCullModel(e->origin, e->model))
		return;

	RotateLightVector(e->axis, e->origin, dl->origin, lightorg);

	BE_SelectEntity(e);

	GL_SelectVBO(0);
	GL_SelectEBO(0);
	qglEnableClientState(GL_VERTEX_ARRAY);

	BE_PushOffsetShadow(true);

	model = e->model;
	surf = model->surfaces+model->firstmodelsurface;
	for (i = 0; i < model->nummodelsurfaces; i++, surf++)
	{
		if (surf->flags & SURF_PLANEBACK)
		{//inverted normal.
			if (DotProduct(surf->plane->normal, lightorg)-surf->plane->dist >= -0.1)
				continue;
		}
		else
		{
			if (DotProduct(surf->plane->normal, lightorg)-surf->plane->dist <= 0.1)
				continue;
		}

		if (surf->flags & (SURF_DRAWALPHA | SURF_DRAWTILED))
		{	// no shadows
			continue;
		}

		if (!surf->mesh)
			continue;

		//front face
		qglVertexPointer(3, GL_FLOAT, sizeof(vecV_t), surf->mesh->xyz_array);
		qglDrawArrays(GL_POLYGON, 0, surf->mesh->numvertexes);
//		qglDrawRangeElements(GL_TRIANGLES, 0, surf->mesh->numvertexes, surf->mesh->numindexes, GL_INDEX_TYPE, surf->mesh->indexes);
		RQuantAdd(RQUANT_SHADOWFACES, surf->mesh->numvertexes);

		for (v = 0; v < surf->mesh->numvertexes; v++)
		{
		//border
			v1 = surf->mesh->xyz_array[v];
			v2 = surf->mesh->xyz_array[( v+1 )%surf->mesh->numvertexes];

			//get positions of v3 and v4 based on the light position
			v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
			v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
			v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

			v4[0] = ( v2[0]-lightorg[0] )*PROJECTION_DISTANCE;
			v4[1] = ( v2[1]-lightorg[1] )*PROJECTION_DISTANCE;
			v4[2] = ( v2[2]-lightorg[2] )*PROJECTION_DISTANCE;

			//Now draw the quad from the two verts to the projected light
			//verts
			qglBegin( GL_QUAD_STRIP );
				qglVertex3fv(v1);
				qglVertex3f	(v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2]);
				qglVertex3fv(v2);
				qglVertex3f (v2[0]+v4[0], v2[1]+v4[1], v2[2]+v4[2]);
			qglEnd();
		}

//back
			//the same applies as earlier
		qglBegin(GL_POLYGON);
		for (v = surf->mesh->numvertexes-1; v >=0; v--)
		{
			v1 = surf->mesh->xyz_array[v];
			v3[0] = (v1[0]-lightorg[0])*PROJECTION_DISTANCE;
			v3[1] = (v1[1]-lightorg[1])*PROJECTION_DISTANCE;
			v3[2] = (v1[2]-lightorg[2])*PROJECTION_DISTANCE;

			qglVertex3f(v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2]);
		}
		qglEnd();
	}

	BE_PushOffsetShadow(false);
}


/*when this is called, the gl state has been set up to draw the stencil volumes using whatever extensions we have
if secondside is set, then the gpu sucks and we're drawing stuff the slow 2-pass way, and this is the second pass.
*/
static void Sh_DrawStencilLightShadows(dlight_t *dl, qbyte *lvis, qbyte *vvis, qboolean secondside)
{
	extern cvar_t gl_part_flame;
	int		i;
	struct shadowmesh_s *sm;
	entity_t *ent;

	BE_PushOffsetShadow(false);

	sm = SHM_BuildShadowVolumeMesh(dl, lvis, vvis);
	if (!sm)
		Sh_DrawBrushModelShadow(dl, &r_worldentity);
	else
	{
//qglEnable(GL_POLYGON_OFFSET_FILL);
//qglPolygonOffset(shaderstate.curpolyoffset.factor, shaderstate.curpolyoffset.unit);

		GL_SelectVBO(0);
		GL_SelectEBO(0);
		qglEnableClientState(GL_VERTEX_ARRAY);
		//draw cached world shadow mesh
		qglVertexPointer(3, GL_FLOAT, sizeof(vecV_t), sm->verts);
		qglDrawRangeElements(GL_TRIANGLES, 0, sm->numverts, sm->numindicies, GL_INDEX_TYPE, sm->indicies);
		RQuantAdd(RQUANT_SHADOWFACES, sm->numindicies);

//qglEnable(GL_POLYGON_OFFSET_FILL);
//qglPolygonOffset(shaderstate.curpolyoffset.factor, shaderstate.curpolyoffset.unit);
	}
	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		ent = &cl_visedicts[i];

		if (ent->flags & (RF_NOSHADOW|Q2RF_BEAM))
			continue;

		{
			if (ent->keynum == dl->key && ent->keynum)
				continue;
		}
		if (!ent->model)
			continue;

		if (cls.allow_anyparticles || ent->visframe)	//allowed or static
		{
			if (ent->model->engineflags & MDLF_ENGULPHS)
			{
				if (gl_part_flame.value)
					continue;
			}
		}

		switch (ent->model->type)
		{
		case mod_alias:
			R_DrawGAliasShadowVolume (ent, dl->origin, dl->radius);
			break;

		case mod_brush:
			Sh_DrawBrushModelShadow (dl, ent);
			break;

		default:
			break;
		}
	}
	BE_SelectEntity(&r_worldentity);
}

//draws a light using stencil shadows.
//redraws world geometry up to 3 times per light...
static qboolean Sh_DrawStencilLight(dlight_t *dl, vec3_t colour, qbyte *vvis)
{
	int sdecrw;
	int sincrw;
	int leaf;
	qbyte *lvis;

	qbyte	lvisb[MAX_MAP_LEAFS/8];

	vec3_t mins;
	vec3_t maxs;

	if (R_CullSphere(dl->origin, dl->radius))
	{
		bench.numfrustumculled++;
		return false;	//this should be the more common case
	}

	mins[0] = dl->origin[0] - dl->radius;
	mins[1] = dl->origin[1] - dl->radius;
	mins[2] = dl->origin[2] - dl->radius;

	maxs[0] = dl->origin[0] + dl->radius;
	maxs[1] = dl->origin[1] + dl->radius;
	maxs[2] = dl->origin[2] + dl->radius;

	if (!dl->rebuildcache)
	{
		//fixme: check head node first?
		if (!Sh_LeafInView(dl->worldshadowmesh->litleaves, vvis))
		{
			bench.numpvsculled++;
			return false;
		}
		lvis = NULL;
	}
	else
	{
		leaf = cl.worldmodel->funcs.LeafnumForPoint(cl.worldmodel, dl->origin);
		lvis = cl.worldmodel->funcs.LeafPVS(cl.worldmodel, leaf, lvisb, sizeof(lvisb));

		if (!Sh_VisOverlaps(lvis, vvis))	//The two viewing areas do not intersect.
		{
			bench.numpvsculled++;
			return false;
		}
	}

	//sets up the gl scissor (and culls to view)
	if (Sh_ScissorForBox(mins, maxs))
	{
		bench.numscissorculled++;
		return false;	//this doesn't cull often.
	}
	bench.numlights++;

	BE_SelectDLight(dl, colour);
	BE_SelectMode(BEM_STENCIL);

	//The backend doesn't maintain scissor state.
//	qglEnable(GL_SCISSOR_TEST);
	//The backend doesn't maintain stencil test state either - it needs to be active for more than just stencils, or disabled. its awkward.
	qglEnable(GL_STENCIL_TEST);

	//FIXME: is it practical to test to see if scissors allow not clearing the stencil buffer?

	/*we don't need all that much stencil buffer depth, and if we don't get enough or have dodgy volumes, wrap if we can*/
	sincrw = GL_INCR;
	sdecrw = GL_DECR;
	if (gl_config.ext_stencil_wrap)
	{	//minimise damage...
		sincrw = GL_INCR_WRAP_EXT;
		sdecrw = GL_DECR_WRAP_EXT;
	}
	//our stencil writes.

#ifdef _DEBUG
/*	if (r_shadows.value == 666)	//testing (visible shadow volumes)
	{
		checkerror();
		qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		qglColor3f(dl->color[0], dl->color[1], dl->color[2]);
		qglDisable(GL_STENCIL_TEST);
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(-1, -1);
	//	qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		Sh_DrawStencilLightShadows(dl, lvis, false);
		qglDisable(GL_POLYGON_OFFSET_FILL);
		checkerror();
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}*/
#endif

	if (qglStencilOpSeparateATI)
	{
		qglClearStencil(0);
		qglClear(GL_STENCIL_BUFFER_BIT);
		GL_CullFace(0);

		qglStencilFunc( GL_ALWAYS, 1, ~0 );

		qglStencilOpSeparateATI(GL_BACK, GL_KEEP, sincrw, GL_KEEP);
		qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, sdecrw, GL_KEEP);

		Sh_DrawStencilLightShadows(dl, lvis, vvis, false);
		qglStencilOpSeparateATI(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_KEEP);

		GL_CullFace(SHADER_CULL_FRONT);

		qglStencilFunc( GL_EQUAL, 0, ~0 );
	}
	else if (qglActiveStencilFaceEXT)
	{
		/*personally I prefer the ATI way (nvidia method)*/
		qglClearStencil(0);
		qglClear(GL_STENCIL_BUFFER_BIT);
		GL_CullFace(0);

		qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

		qglActiveStencilFaceEXT(GL_BACK);
		qglStencilOp(GL_KEEP, sincrw, GL_KEEP);
		qglStencilFunc( GL_ALWAYS, 1, ~0 );

		qglActiveStencilFaceEXT(GL_FRONT);
		qglStencilOp(GL_KEEP, sdecrw, GL_KEEP);
		qglStencilFunc( GL_ALWAYS, 1, ~0 );

		Sh_DrawStencilLightShadows(dl, lvis, vvis, false);

		qglActiveStencilFaceEXT(GL_BACK);
		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		qglStencilFunc( GL_ALWAYS, 0, ~0 );

		qglActiveStencilFaceEXT(GL_FRONT);
		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		qglStencilFunc( GL_EQUAL, 0, ~0 );

		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);

		GL_CullFace(SHADER_CULL_FRONT);
	}
	else //your graphics card sucks and lacks efficient stencil shadow techniques.
	{	//centered around 0. Will only be increased then decreased less.
		qglClearStencil(0);
		qglClear(GL_STENCIL_BUFFER_BIT);

		qglStencilFunc(GL_ALWAYS, 0, ~0);

		GL_CullFace(SHADER_CULL_BACK);
		qglStencilOp(GL_KEEP, sincrw, GL_KEEP);
		Sh_DrawStencilLightShadows(dl, lvis, vvis, false);

		GL_CullFace(SHADER_CULL_FRONT);
		qglStencilOp(GL_KEEP, sdecrw, GL_KEEP);
		Sh_DrawStencilLightShadows(dl, lvis, vvis, true);

		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		qglStencilFunc(GL_EQUAL, 0, ~0);
	}
	//end stencil writing.

#if 0	//draw the stencil stuff to the red channel
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	GL_Set2D();

	{
		qglColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
		qglStencilFunc(GL_GREATER, 1, ~0);
		R2D_ConsoleBackground(vid.height);

		qglColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
		qglStencilFunc(GL_LESS, 1, ~0);
		R2D_ConsoleBackground(vid.height);

		qglColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
		qglStencilFunc(GL_NEVER, 1, ~0);
		R2D_ConsoleBackground(vid.height);
	}

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();
#endif

	PPL_RevertToKnownState();

	BE_SelectMode(BEM_LIGHT);
	Sh_DrawEntLighting(dl, colour);

	qglDisable(GL_STENCIL_TEST);
	qglStencilFunc( GL_ALWAYS, 0, ~0 );

	return true;
}

static void Sh_DrawShadowlessLight(dlight_t *dl, vec3_t colour, qbyte *vvis)
{
	vec3_t mins, maxs;

	if (R_CullSphere(dl->origin, dl->radius))
	{
		bench.numfrustumculled++;
		return;	//this should be the more common case
	}

	if (!dl->rebuildcache)
	{
		//fixme: check head node first?
		if (!Sh_LeafInView(dl->worldshadowmesh->litleaves, vvis))
		{
			bench.numpvsculled++;
			return;
		}
	}
	else
	{
		int leaf;
		qbyte *lvis;
		qbyte	lvisb[MAX_MAP_LEAFS/8];

		leaf = cl.worldmodel->funcs.LeafnumForPoint(cl.worldmodel, dl->origin);
		lvis = cl.worldmodel->funcs.LeafPVS(cl.worldmodel, leaf, lvisb, sizeof(lvisb));

		if (!dl->die)
			SHM_BuildShadowVolumeMesh(dl, lvis, vvis);

		if (!Sh_VisOverlaps(lvis, vvis))	//The two viewing areas do not intersect.
		{
			bench.numpvsculled++;
			return;
		}
	}

	mins[0] = dl->origin[0] - dl->radius;
	mins[1] = dl->origin[1] - dl->radius;
	mins[2] = dl->origin[2] - dl->radius;

	maxs[0] = dl->origin[0] + dl->radius;
	maxs[1] = dl->origin[1] + dl->radius;
	maxs[2] = dl->origin[2] + dl->radius;


//sets up the gl scissor (and culls to view)
	if (Sh_ScissorForBox(mins, maxs))
	{
		bench.numscissorculled++;
		return;	//was culled.
	}

	bench.numlights++;

	BE_SelectDLight(dl, colour);
	BE_SelectMode(BEM_LIGHT);
	Sh_DrawEntLighting(dl, colour);
}

void Sh_DrawLights(qbyte *vis)
{
	vec3_t colour;
	dlight_t *dl;
	int i;
	unsigned int ignoreflags;
	extern cvar_t r_shadow_realtime_world, r_shadow_realtime_dlight;
	extern cvar_t r_shadow_realtime_world_shadows, r_shadow_realtime_dlight_shadows;

	if (!r_shadow_realtime_world.ival && !r_shadow_realtime_dlight.ival)
		return;

	if (!gl_config.arb_shader_objects)
	{
		Con_Printf("Missing GL extensions: switching off realtime lighting.\n");
		r_shadow_realtime_world.ival = 0;
		r_shadow_realtime_dlight.ival = 0;
		return;
	}

	ignoreflags = (r_shadow_realtime_world.value?LFLAG_REALTIMEMODE:LFLAG_NORMALMODE);

	for (dl = cl_dlights+rtlights_first, i=rtlights_first; i<rtlights_max; i++, dl++)
	{
		if (!dl->radius)
			continue;	//dead

		if (!(dl->flags & ignoreflags))
			continue;

		if (dl->die)
		{
			colour[0] = dl->color[0]*10;
			colour[1] = dl->color[1]*10;
			colour[2] = dl->color[2]*10;
		}
		else
		{
			colour[0] = dl->color[0];
			colour[1] = dl->color[1];
			colour[2] = dl->color[2];
		}
		if (dl->style)
		{
			if (cl_lightstyle[dl->style-1].colour & 1)
				colour[0] *= d_lightstylevalue[dl->style-1]/255.0f;
			else
				colour[0] = 0;
			if (cl_lightstyle[dl->style-1].colour & 2)
				colour[1] *= d_lightstylevalue[dl->style-1]/255.0f;
			else
				colour[1] = 0;
			if (cl_lightstyle[dl->style-1].colour & 4)
				colour[2] *= d_lightstylevalue[dl->style-1]/255.0f;
			else
				colour[2] = 0;
		}

		if (colour[0] < 0.001 && colour[1] < 0.001 && colour[2] < 0.001)
			continue;	//just switch these off.

		if (((!dl->die)?!r_shadow_realtime_world_shadows.ival:!r_shadow_realtime_dlight_shadows.ival) || dl->flags & LFLAG_NOSHADOWS)
		{
			Sh_DrawShadowlessLight(dl, colour, vis);
		}
		else if (dl->flags & LFLAG_SHADOWMAP)
		{
			Sh_DrawShadowMapLight(dl, colour, vis);
		}
		else
		{
			Sh_DrawStencilLight(dl, colour, vis);
		}
	}

	qglDisable(GL_SCISSOR_TEST);
	BE_SelectMode(BEM_STANDARD);

//	if (developer.value)
//	Con_Printf("%i lights drawn, %i frustum culled, %i pvs culled, %i scissor culled\n", bench.numlights, bench.numfrustumculled, bench.numpvsculled, bench.numscissorculled);
//	memset(&bench, 0, sizeof(bench));
}
#endif
#endif
