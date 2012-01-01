#include "quakedef.h"

#if defined(GLQUAKE) || defined(D3DQUAKE)
#ifdef RTLIGHTS

#include "glquake.h"
#include "shader.h"

#ifdef D3DQUAKE
#include "shader.h"
#if !defined(HMONITOR_DECLARED) && (WINVER < 0x0500)
    #define HMONITOR_DECLARED
    DECLARE_HANDLE(HMONITOR);
#endif
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 pD3DDev9;
void D3DBE_Cull(unsigned int sflags);
void D3DBE_RenderShadowBuffer(unsigned int numverts, IDirect3DVertexBuffer9 *vbuf, unsigned int numindicies, IDirect3DIndexBuffer9 *ibuf);
#endif
void GLBE_RenderShadowBuffer(unsigned int numverts, int vbo, vecV_t *verts, unsigned numindicies, int ibo, index_t *indicies);

#define SHADOWMAP_SIZE 512

#define PROJECTION_DISTANCE (float)(dl->radius*2)//0x7fffffff

#define nearplane	(16)

static int shadow_fbo_id;
static int crepuscular_fbo_id;
texid_t crepuscular_texture_id;
shader_t *crepuscular_shader;

static void Sh_DrawEntLighting(dlight_t *light, vec3_t colour);


struct {
	int numlights;
	int shadowsurfcount;

	int numfrustumculled;
	int numpvsculled;
	int numscissorculled;
} bench;



void Sh_Shutdown(void)
{
#ifdef GLQUAKE
	if (shadow_fbo_id)
	{
		qglDeleteRenderbuffersEXT(1, &shadow_fbo_id);
		shadow_fbo_id = 0;
	}
	if (crepuscular_fbo_id)
	{
		qglDeleteRenderbuffersEXT(1, &crepuscular_fbo_id);
		crepuscular_fbo_id = 0;
	}
#endif
}




typedef struct {
	unsigned int count;
	unsigned int max;
	mesh_t **s;
} shadowmeshsurfs_t;
typedef struct shadowmesh_s {
	qboolean surfonly;
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

#ifdef GLQUAKE
	GLuint vebo[2];
#endif
#ifdef D3DQUAKE
	IDirect3DVertexBuffer9	*d3d_vbuffer;
	IDirect3DIndexBuffer9	*d3d_ibuffer;
#endif
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
static void SHM_Vertex3fv (const float *v)
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

static void SHM_TriangleFan(int numverts, vecV_t *verts, vec3_t lightorg, float pd)
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

static void SH_FreeShadowMesh(shadowmesh_t *sm)
{
	unsigned int i;
	for (i = 0; i < sm->numsurftextures; i++)
		Z_Free(sm->litsurfs[i].s);
	Z_Free(sm->litsurfs);
	Z_Free(sm->indicies);
	Z_Free(sm->verts);

	switch (qrenderer)
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		qglDeleteBuffersARB(2, sm->vebo);
		sm->vebo[0] = 0;
		sm->vebo[1] = 0;
		break;
#endif
#ifdef D3DQUAKE
	case QR_DIRECT3D:
		if (sm->d3d_ibuffer)
			IDirect3DIndexBuffer9_Release(sm->d3d_ibuffer);
		sm->d3d_ibuffer = NULL;
		if (sm->d3d_vbuffer)
			IDirect3DVertexBuffer9_Release(sm->d3d_vbuffer);
		sm->d3d_vbuffer = NULL;
		break;
#endif
	}

	Z_Free(sm);
}

static void SHM_BeginShadowMesh(dlight_t *dl, qboolean surfonly)
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
	sh_shmesh->surfonly = surfonly;

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
	if (sh_shmesh != &sh_tempshmesh)
	{
		switch (qrenderer)
		{
#ifdef GLQUAKE
		case QR_OPENGL:
			qglGenBuffersARB(2, sh_shmesh->vebo);

			GL_SelectVBO(sh_shmesh->vebo[0]);
			qglBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(*sh_shmesh->verts) * sh_shmesh->numverts, sh_shmesh->verts, GL_STATIC_DRAW_ARB);

			GL_SelectEBO(sh_shmesh->vebo[1]);
			qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(*sh_shmesh->indicies) * sh_shmesh->numindicies, sh_shmesh->indicies, GL_STATIC_DRAW_ARB);
			break;
#endif
#ifdef D3DQUAKE
		case QR_DIRECT3D:
			if (sh_shmesh->numindicies && sh_shmesh->numverts)
			{
				void *map;
				IDirect3DDevice9_CreateIndexBuffer(pD3DDev9, sizeof(index_t) * sh_shmesh->numindicies, 0, D3DFMT_QINDEX, D3DPOOL_MANAGED, &sh_shmesh->d3d_ibuffer, NULL);
				IDirect3DIndexBuffer9_Lock(sh_shmesh->d3d_ibuffer, 0, sizeof(index_t) * sh_shmesh->numindicies, &map, D3DLOCK_DISCARD);
				memcpy(map, sh_shmesh->indicies, sizeof(index_t) * sh_shmesh->numindicies);
				IDirect3DIndexBuffer9_Unlock(sh_shmesh->d3d_ibuffer);

				IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, sizeof(vecV_t) * sh_shmesh->numverts, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &sh_shmesh->d3d_vbuffer, NULL);
				IDirect3DVertexBuffer9_Lock(sh_shmesh->d3d_vbuffer, 0, sizeof(vecV_t) * sh_shmesh->numverts, &map, D3DLOCK_DISCARD);
				memcpy(map, sh_shmesh->verts, sizeof(vecV_t) * sh_shmesh->numverts);
				IDirect3DVertexBuffer9_Unlock(sh_shmesh->d3d_vbuffer);

			}
			break;
#endif
		}

		Z_Free(sh_shmesh->verts);
		sh_shmesh->verts = NULL;

		Z_Free(sh_shmesh->indicies);
		sh_shmesh->indicies = NULL;
	}
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
					if (sh_shmesh->surfonly)
						continue;

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
					if (sh_shmesh->surfonly)
						continue;

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

static void SHM_MarkLeavesQ1(dlight_t *dl, unsigned char *lvis)
{
	mnode_t *node;
	int i;
	sh_shadowframe++;

	//variation on mark leaves
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (lvis[i>>3] & (1<<(i&7)))
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

#ifdef Q3BSPS
static void SHM_RecursiveWorldNodeQ3_r (dlight_t *dl, mnode_t *node)
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

static struct {
	unsigned int numtris;
	unsigned int maxtris;
	struct {
		signed int edge[3];
	} *tris; /*negative for reverse edge*/

	unsigned int numedges;
	unsigned int maxedges;
	struct {
		unsigned int vert[2];
	} *edges;

	unsigned int numpoints;
	unsigned int maxpoints;
	vec3_t *points;

	unsigned int maxedgeuses;
	int *edgeuses;	/*negative for back sides, so 0 means unused or used equally on both sides*/
} cv;

#define VERT_POS_EPSILON (1.0f/32)
static int SHM_ComposeVolume_FindVert(float *vert)
{
	int i;
	for (i = 0; i < cv.numpoints; i++)
	{
#if 1
		if (cv.points[i][0] == vert[0] &&
			cv.points[i][1] == vert[1] &&
			cv.points[i][2] == vert[2])
#else
		vec3_t d;
		d[0] = cv.points[i][0]-vert[0];
		d[1] = cv.points[i][1]-vert[1];
		d[2] = cv.points[i][2]-vert[2];
		if (d[0]*d[0] < VERT_POS_EPSILON &&
			d[1]*d[1] < VERT_POS_EPSILON &&
			d[2]*d[2] < VERT_POS_EPSILON)
#endif
			return i;
	}
	VectorCopy(vert, cv.points[i]);
	cv.numpoints++;
	return i;
}
static int SHM_ComposeVolume_FindEdge(int v1, int v2)
{
	int i;
	for (i = 0; i < cv.numedges; i++)
	{
		if (cv.edges[i].vert[0] == v1 && cv.edges[i].vert[1] == v2)
			return i;
		if (cv.edges[i].vert[0] == v2 && cv.edges[i].vert[1] == v1)
			return -(i+1);
	}
	cv.edges[i].vert[0] = v1;
	cv.edges[i].vert[1] = v2;
	cv.numedges++;
	return i;
}

/*each triangle is coplanar, and all face the light, and its a triangle fan. this is a special case that provides a slight speedup*/
static void SHM_ComposeVolume_Fan(vecV_t *points, int numpoints)
{
	int newmax;
	int lastedge;
	int i;

	#define MAX_ARRAY_VERTS 65535
	static index_t pointidx[MAX_ARRAY_VERTS];

	/*make sure there's space*/
	newmax = (cv.numpoints+numpoints + inc)&~(inc-1);
	if (cv.maxpoints < newmax)
	{
		cv.maxpoints = newmax;
		cv.points = BZ_Realloc(cv.points, newmax * sizeof(*cv.points));
	}
	newmax = (cv.numedges+(numpoints-2)*3 + inc)&~(inc-1);
	if (cv.maxedges < newmax)
	{
		cv.maxedges = newmax;
		cv.edges = BZ_Realloc(cv.edges, newmax * sizeof(*cv.edges));
	}
	newmax = (cv.numtris+(numpoints-2) + inc)&~(inc-1);
	if (cv.maxtris < newmax)
	{
		cv.maxtris = newmax;
		cv.tris = BZ_Realloc(cv.tris, newmax * sizeof(*cv.tris));
	}

	for (i = 0; i < numpoints; i++)
	{
		pointidx[i] = SHM_ComposeVolume_FindVert(points[i]);
	}
	lastedge = SHM_ComposeVolume_FindEdge(pointidx[0], pointidx[1]);
	for (i = 2; i < numpoints; i++)
	{
		cv.tris[cv.numtris].edge[0] = lastedge;
		cv.tris[cv.numtris].edge[1] = SHM_ComposeVolume_FindEdge(pointidx[i-1], pointidx[i]);
		lastedge = SHM_ComposeVolume_FindEdge(pointidx[i], pointidx[0]);
		cv.tris[cv.numtris].edge[2] = lastedge;
		lastedge = -(lastedge+1);
		cv.numtris++;
	}
}
static void SHM_ComposeVolume_Soup(vecV_t *points, int numpoints, index_t *idx, int numidx)
{
	int newmax;
	int i;

	#define MAX_ARRAY_VERTS 65535
	static index_t pointidx[MAX_ARRAY_VERTS];

	/*make sure there's space*/
	newmax = (cv.numpoints+numpoints + inc)&~(inc-1);
	if (cv.maxpoints < newmax)
	{
		cv.maxpoints = newmax;
		cv.points = BZ_Realloc(cv.points, newmax * sizeof(*cv.points));
	}
	newmax = (cv.numedges+numidx + inc)&~(inc-1);
	if (cv.maxedges < newmax)
	{
		cv.maxedges = newmax;
		cv.edges = BZ_Realloc(cv.edges, newmax * sizeof(*cv.edges));
	}
	newmax = (cv.numtris+numidx/3 + inc)&~(inc-1);
	if (cv.maxtris < newmax)
	{
		cv.maxtris = newmax;
		cv.tris = BZ_Realloc(cv.tris, newmax * sizeof(*cv.tris));
	}

	for (i = 0; i < numpoints; i++)
	{
		pointidx[i] = SHM_ComposeVolume_FindVert(points[i]);
	}

	for (i = 0; i < numidx; i+=3, idx+=3)
	{
		cv.tris[cv.numtris].edge[0] = SHM_ComposeVolume_FindEdge(pointidx[idx[0]], pointidx[idx[1]]);
		cv.tris[cv.numtris].edge[1] = SHM_ComposeVolume_FindEdge(pointidx[idx[1]], pointidx[idx[2]]);
		cv.tris[cv.numtris].edge[2] = SHM_ComposeVolume_FindEdge(pointidx[idx[2]], pointidx[idx[0]]);
		cv.numtris++;
	}
}
/*call this function after generating litsurfs meshes*/
static void SHM_ComposeVolume_BruteForce(dlight_t *dl)
{
	shadowmeshsurfs_t *sms;
	unsigned int tno;
	unsigned int sno;
	int i, e;
	mesh_t *sm;
	vec3_t ext;
	cv.numedges = 0;
	cv.numpoints = 0;
	cv.numtris = 0;

	for (tno = 0; tno < sh_shmesh->numsurftextures; tno++)
	{
		sms = &sh_shmesh->litsurfs[tno];
		if (!sms->count)
			continue;
		if ((cl.worldmodel->textures[tno]->shader->flags & (SHADER_BLEND|SHADER_NODRAW)))
			continue;

		for (sno = 0; sno < sms->count; sno++)
		{
			sm = sms->s[sno];

			if (sm->istrifan)
				SHM_ComposeVolume_Fan(sm->xyz_array, sm->numvertexes);
			else
				SHM_ComposeVolume_Soup(sm->xyz_array, sm->numvertexes, sm->indexes, sm->numindexes);
		}
	}

	/*FIXME: clip away overlapping triangles*/

	if (cv.maxedgeuses < cv.numedges)
	{
		BZ_Free(cv.edgeuses);
		cv.maxedgeuses = cv.numedges;
		cv.edgeuses = Z_Malloc(cv.maxedgeuses * sizeof(*cv.edgeuses));
	}
	else
		memset(cv.edgeuses, 0, cv.numedges * sizeof(*cv.edgeuses));
	
	i = (sh_shmesh->numverts+cv.numpoints*6+inc+5)&~(inc-1);	//and a bit of padding
	if (sh_shmesh->maxverts < i)
	{
		sh_shmesh->maxverts = i;
		sh_shmesh->verts = BZ_Realloc(sh_shmesh->verts, i * sizeof(*sh_shmesh->verts));
	}

	for (i = 0; i < cv.numpoints; i++)
	{
		/*front face*/
		sh_shmesh->verts[(i * 2) + 0][0] = cv.points[i][0];
		sh_shmesh->verts[(i * 2) + 0][1] = cv.points[i][1];
		sh_shmesh->verts[(i * 2) + 0][2] = cv.points[i][2];

		/*shadow direction*/
		ext[0] = cv.points[i][0]-dl->origin[0];
		ext[1] = cv.points[i][1]-dl->origin[1];
		ext[2] = cv.points[i][2]-dl->origin[2];
		VectorNormalize(ext);

		/*back face*/
		sh_shmesh->verts[(i * 2) + 1][0] = cv.points[i][0] + ext[0] * dl->radius;
		sh_shmesh->verts[(i * 2) + 1][1] = cv.points[i][1] + ext[1] * dl->radius;
		sh_shmesh->verts[(i * 2) + 1][2] = cv.points[i][2] + ext[2] * dl->radius;
	}
	sh_shmesh->numverts = i*2;

	i = (sh_shmesh->numindicies+cv.numtris*6+cv.numedges*6+inc+5)&~(inc-1);	//and a bit of padding
	if (sh_shmesh->maxindicies < i)
	{
		sh_shmesh->maxindicies = i;
		sh_shmesh->indicies = BZ_Realloc(sh_shmesh->indicies, i * sizeof(*sh_shmesh->indicies));
	}

	for (tno = 0; tno < cv.numtris; tno++)
	{
		for (i = 0; i < 3; i++)
		{
			e = cv.tris[tno].edge[i];
			if (e < 0)
			{
				e = -(e+1);
				cv.edgeuses[e]--;
				e = cv.edges[e].vert[1];
			}
			else
			{
				cv.edgeuses[e]++;
				e = cv.edges[e].vert[0];
			}

			sh_shmesh->indicies[sh_shmesh->numindicies+i] = e*2;
			sh_shmesh->indicies[sh_shmesh->numindicies+5-i] = e*2 + 1;
		}
		sh_shmesh->numindicies += 6;
	}

	for (i = 0; i < cv.numedges; i++)
	{
		if (cv.edgeuses[i] > 0)
		{
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[1]*2 + 0;
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[0]*2 + 0;
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[0]*2 + 1;

			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[0]*2 + 1;
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[1]*2 + 1;
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[1]*2 + 0;
		}
		else if (cv.edgeuses[i] < 0)
		{
			//generally should not happen...
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[1]*2 + 0;
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[0]*2 + 1;
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[0]*2 + 0;

			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[0]*2 + 1;
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[1]*2 + 0;
			sh_shmesh->indicies[sh_shmesh->numindicies++] = cv.edges[i].vert[1]*2 + 1;
		}
	}
}

static struct shadowmesh_s *SHM_BuildShadowMesh(dlight_t *dl, unsigned char *lvis, unsigned char *vvis, qboolean surfonly)
{
	float *v1, *v2;
	vec3_t v3, v4;

	if (dl->worldshadowmesh && !dl->rebuildcache)
		return dl->worldshadowmesh;

	firstedge=0;

	switch(cl.worldmodel->fromgame)
	{
	case fg_quake:
	case fg_halflife:
		if (!dl->die)
		{
			SHM_BeginShadowMesh(dl, true);
			SHM_MarkLeavesQ1(dl, lvis);
			SHM_RecursiveWorldNodeQ1_r(dl, cl.worldmodel->nodes);
			if (!surfonly)
				SHM_ComposeVolume_BruteForce(dl);
		}
		else
		{
			SHM_BeginShadowMesh(dl, surfonly);
			SHM_MarkLeavesQ1(dl, lvis);
			SHM_RecursiveWorldNodeQ1_r(dl, cl.worldmodel->nodes);
		}
		break;
#ifdef Q2BSPS
	case fg_quake2:
		SHM_BeginShadowMesh(dl, surfonly);
		SHM_MarkLeavesQ2(dl, lvis, vvis);
		SHM_RecursiveWorldNodeQ2_r(dl, cl.worldmodel->nodes);
		break;
#endif
#ifdef Q3BSPS
	case fg_quake3:
		/*q3 doesn't have edge info*/
		SHM_BeginShadowMesh(dl, true);
		sh_shadowframe++;
		SHM_RecursiveWorldNodeQ3_r(dl, cl.worldmodel->nodes);
		if (!surfonly)
			SHM_ComposeVolume_BruteForce(dl);
		break;
#endif
	default:
		return NULL;
	}

	/*generate edge polys for map types that need it (q1/q2)*/
	if (!surfonly)
	{
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
	}

	return SHM_FinishShadowMesh(dl);
}













static qboolean Sh_VisOverlaps(qbyte *v1, qbyte *v2)
{
	int i, m;
	if (!v2)
		return false;
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

typedef struct
{
	int x;
	int y;
	int width;
	int height;
	double dmin;
	double dmax;
} srect_t;
static void Sh_Scissor (srect_t r)
{
#if 0	//visible scissors
	extern cvar_t temp1;
	if (temp1.ival)
	{
		qglMatrixMode(GL_PROJECTION);
		qglPushMatrix();
		qglLoadIdentity();
		qglOrtho  (0, vid.pixelwidth, vid.pixelheight, 0, -99999, 99999);
		qglMatrixMode(GL_MODELVIEW);
		qglPushMatrix();
		qglLoadIdentity();
	//	GL_Set2D();

		qglColor4f(1,1,1,1);
		qglDisable(GL_DEPTH_TEST);
		qglDisable(GL_SCISSOR_TEST);
		qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
		qglDisable(GL_TEXTURE_2D);

		qglBegin(GL_LINE_LOOP);
		qglVertex2f(r.x, vid.pixelheight - (r.y + r.height));
		qglVertex2f(r.x+r.width, vid.pixelheight - (r.y + r.height));
		qglVertex2f(r.x+r.width, vid.pixelheight - (r.y));
		qglVertex2f(r.x, vid.pixelheight - (r.y));
		qglEnd();

		qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

		qglMatrixMode(GL_PROJECTION);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
		qglPopMatrix();
	}
#endif

	switch(qrenderer)
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		qglScissor(r.x, r.y, r.width, r.height);

		if (qglDepthBoundsEXT)
		{
			qglDepthBoundsEXT(r.dmin, r.dmax);
			qglEnable(GL_DEPTH_BOUNDS_TEST_EXT);
		}
		break;
#endif
#ifdef D3DQUAKE
	case QR_DIRECT3D:
		{
			RECT rect;
			rect.left = r.x;
			rect.right = r.x + r.width;
			rect.top = r.y;
			rect.bottom = r.y + r.height;
			IDirect3DDevice9_SetScissorRect(pD3DDev9, &rect);
		}
		break;
#endif
	}
}

#if 0
static qboolean Sh_ScissorForSphere(vec3_t center, float radius, vrect_t *rect)
{
	/*return false to say that its fully offscreen*/

	float v[4], tempv[4];
	extern cvar_t temp1;
	int i;
	vrect_t r;
	
	radius *= temp1.value;

	rect->x = 0;
	rect->y = 0;
	rect->width = vid.pixelwidth;
	rect->height = vid.pixelheight;


/*
	for (i = 0; i < 4; i++)
	{
		v[3] = 1;
		VectorMA(center, radius, frustum[i].normal, v);

		tempv[0] = r_refdef.m_view[0]*v[0] + r_refdef.m_view[4]*v[1] + r_refdef.m_view[8]*v[2] + r_refdef.m_view[12]*v[3];
		tempv[1] = r_refdef.m_view[1]*v[0] + r_refdef.m_view[5]*v[1] + r_refdef.m_view[9]*v[2] + r_refdef.m_view[13]*v[3];
		tempv[2] = r_refdef.m_view[2]*v[0] + r_refdef.m_view[6]*v[1] + r_refdef.m_view[10]*v[2] + r_refdef.m_view[14]*v[3];
		tempv[3] = r_refdef.m_view[3]*v[0] + r_refdef.m_view[7]*v[1] + r_refdef.m_view[11]*v[2] + r_refdef.m_view[15]*v[3];

		product[0] = r_refdef.m_projection[0]*tempv[0] + r_refdef.m_projection[4]*tempv[1] + r_refdef.m_projection[8]*tempv[2] + r_refdef.m_projection[12]*tempv[3];
		product[1] = r_refdef.m_projection[1]*tempv[0] + r_refdef.m_projection[5]*tempv[1] + r_refdef.m_projection[9]*tempv[2] + r_refdef.m_projection[13]*tempv[3];
		product[2] = r_refdef.m_projection[2]*tempv[0] + r_refdef.m_projection[6]*tempv[1] + r_refdef.m_projection[10]*tempv[2] + r_refdef.m_projection[14]*tempv[3];
		product[3] = r_refdef.m_projection[3]*tempv[0] + r_refdef.m_projection[7]*tempv[1] + r_refdef.m_projection[11]*tempv[2] + r_refdef.m_projection[15]*tempv[3];

		v[0] /= v[3];
		v[1] /= v[3];
		v[2] /= v[3];

		out[0] = (1+v[0])/2;
		out[1] = (1+v[1])/2;
		out[2] = (1+v[2])/2;

		r.x 
	}
*/
	return false;
}
#endif

#define BoxesOverlap(a,b,c,d) ((a)[0] <= (d)[0] && (b)[0] >= (c)[0] && (a)[1] <= (d)[1] && (b)[1] >= (c)[1] && (a)[2] <= (d)[2] && (b)[2] >= (c)[2])
static qboolean Sh_ScissorForBox(vec3_t mins, vec3_t maxs, srect_t *r)
{
	static const edge[12][2] =
	{
		{0, 1}, {0, 2}, {1, 3}, {2, 3},
		{4, 5}, {4, 6}, {5, 7}, {6, 7},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	};
	//the box is a simple cube.
	//clip each vert to the near clip plane
	//insert a replacement vertex for edges that cross the nearclip plane where it crosses
	//calc the scissor rect from projecting the verts that survived, plus the clipped edge ones.
	float ncpdist;
	float dist[8];
	int sign[8];
	vec4_t vert[20];
	vec3_t p[8];
	int numverts = 0, i, v1, v2;
	vec4_t v,tv;
	float frac;
	float x,x1,x2,y,y1,y2;
	double z, z1, z2;
	extern cvar_t gl_mindist;

	r->x = 0;
	r->y = 0;
	r->width = vid.pixelwidth;
	r->height = vid.pixelheight;
	r->dmin = 0;
	r->dmax = 1;
	if (1)//!r_shadow_scissor.integer)
	{
		return false;
	}
	/*if view is inside the box, then skip this maths*/
//	if (BoxesOverlap(r_refdef.vieworg, r_refdef.vieworg, mins, maxs))
//	{
//		return false;
//	}

	ncpdist = DotProduct(r_refdef.vieworg, vpn) + gl_mindist.value;

	for (i = 0; i < 8; i++)
	{
		p[i][0] = (i & 1) ? mins[0] : maxs[0];
		p[i][1] = (i & 2) ? mins[1] : maxs[1];
		p[i][2] = (i & 4) ? mins[2] : maxs[2];
		dist[i] = ncpdist - DotProduct(p[i], vpn);
		sign[i] = (dist[i] > 0);
		if (!sign[i])
		{
			VectorCopy(p[i], vert[numverts]);
			numverts++;
		}
	}

	/*fully clipped by near plane*/
	if (!numverts)
		return true;

	if (numverts != 8)
	{
		/*crosses near clip plane somewhere*/
		for (i = 0; i < 12; i++)
		{
			v1 = edge[i][0];
			v2 = edge[i][1];
			if (sign[v1] != sign[v2])
			{
				frac = dist[v1] / (dist[v1] - dist[v2]);
				VectorInterpolate(p[v1], frac, p[v2], vert[numverts]);
				numverts++;
			}
		}
	}
	x1 = y1 = z1 = 1;
	x2 = y2 = z2 = -1;
	/*transform each vert to get the screen pos*/
	for (i = 0; i < numverts; i++)
	{
		vert[i][3] = 1;
		Matrix4x4_CM_Transform4(r_refdef.m_view, vert[i], tv); 
		Matrix4x4_CM_Transform4(r_refdef.m_projection, tv, v);

		x = v[0] / v[3];
		y = v[1] / v[3];
		z = (double)v[2] / v[3];
		if (x < x1) x1 = x;
		if (x > x2) x2 = x;
		if (y < y1) y1 = y;
		if (y > y2) y2 = y;
		if (z < z1) z1 = z;
		if (z > z2) z2 = z;
	}
	x1 = ((1+x1) * r_refdef.vrect.width * vid.pixelwidth) / (vid.width * 2);
	x2 = ((1+x2) * r_refdef.vrect.width * vid.pixelwidth) / (vid.width * 2);
	y1 = ((1+y1) * r_refdef.vrect.height * vid.pixelheight) / (vid.height * 2);
	y2 = ((1+y2) * r_refdef.vrect.height * vid.pixelheight) / (vid.height * 2);
	z1 = (1+z1) / 2;
	z2 = (1+z2) / 2;

	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x2 < 0)
		x2 = 0;
	if (y2 < 0)
		y2 = 0;
	if (x1 > r_refdef.vrect.width)
		x1 = r_refdef.vrect.width;
	if (y1 > r_refdef.vrect.height * vid.pixelheight / vid.height)
		y1 = r_refdef.vrect.height * vid.pixelheight / vid.height;
	if (x2 > r_refdef.vrect.width)
		x2 = r_refdef.vrect.width;
	if (y2 > r_refdef.vrect.height * vid.pixelheight / vid.height)
		y2 = r_refdef.vrect.height * vid.pixelheight / vid.height;
	r->x = floor(x1);
	r->y = floor(y1);
	r->width  = ceil(x2) - r->x;
	r->height = ceil(y2) - r->y;

	r->x += r_refdef.vrect.x;
	r->y += r_refdef.vrect.y;
	r->dmin = z1;
	r->dmax = z2;
	return false;
}

#if 0
static qboolean Sh_ScissorForBox(vec3_t mins, vec3_t maxs, vrect_t *r)
{
	int i, ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2, x, y, f;
	vec3_t smins, smaxs;
	vec4_t v, v2;

	r->x = 0;
	r->y = 0;
	r->width = vid.pixelwidth;
	r->height = vid.pixelheight;
	if (0)//!r_shadow_scissor.integer)
	{
		return false;
	}
	// if view is inside the box, just say yes it's fully visible
	if (BoxesOverlap(r_refdef.vieworg, r_refdef.vieworg, mins, maxs))
	{
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
	f = DotProduct(vpn, r_refdef.vieworg);
	if (DotProduct(vpn, v2) <= f)
	{
		// entirely behind nearclip plane, entirely obscured
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
			Matrix4x4_CM_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y);
			v2[0]*=vid.pixelwidth;
			v2[1]*=vid.pixelheight;
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
			Matrix4x4_CM_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y);
			v2[0]*=vid.pixelwidth;
			v2[1]*=vid.pixelheight;
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
			Matrix4x4_CM_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y);
			v2[0]*=vid.pixelwidth;
			v2[1]*=vid.pixelheight;
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
	if (ix1 < r->x) ix1 = r->x;
	if (iy1 < r->y) iy1 = r->y;
	if (ix2 > r->x + r->width) ix2 = r->x + r->width;
	if (iy2 > r->y + r->height) iy2 = r->y + r->height;
	if (ix2 <= ix1 || iy2 <= iy1)
		return true;
	// set up the scissor rectangle
	
	r->x = ix1;
	r->y = iy1;
	r->width = ix2 - ix1;
	r->height = iy2 - iy1;
	return false;
}
#endif

#ifdef GLQUAKE
void GL_BeginRenderBuffer_DepthOnly(texid_t depthtexture)
{
	if (gl_config.ext_framebuffer_objects)
	{
		if (!shadow_fbo_id)
		{
			qglGenFramebuffersEXT(1, &shadow_fbo_id);
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
	qboolean oxv;
	float mvm[16], sav[16];
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
		Matrix4x4_CM_ModelViewMatrixFromAxis(mvm, l->axis[0], l->axis[1], l->axis[2], l->origin);
		break;
	case 1:
		//back
		VectorNegate(l->axis[0], t1);
		VectorNegate(l->axis[1], t2);
		Matrix4x4_CM_ModelViewMatrixFromAxis(mvm, t1, t2, l->axis[2], l->origin);
		break;
	case 2:
		//left
		VectorNegate(l->axis[1], t1);
		VectorNegate(l->axis[0], t2);
		Matrix4x4_CM_ModelViewMatrixFromAxis(mvm, l->axis[1], t2, l->axis[2], l->origin);
		break;
	case 3:
		//right
		VectorNegate(l->axis[1], t1);
		Matrix4x4_CM_ModelViewMatrixFromAxis(mvm, t1, l->axis[0], l->axis[2], l->origin);
		break;
	case 4:
		//up
		VectorNegate(l->axis[0], t2);
		Matrix4x4_CM_ModelViewMatrixFromAxis(mvm, l->axis[2], l->axis[1], t2, l->origin);
		break;
	case 5:
		//down
		VectorNegate(l->axis[2], t1);
		Matrix4x4_CM_ModelViewMatrixFromAxis(mvm, t1, l->axis[1], l->axis[0], l->origin);
		break;
	}

	qglMatrixMode(GL_MODELVIEW);
	memcpy(sav, r_refdef.m_view, sizeof(r_refdef.m_view));
	memcpy(r_refdef.m_view, mvm, sizeof(r_refdef.m_view));
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
	/*shadow meshes are always drawn as an external view*/
	oxv = r_refdef.externalview;
	r_refdef.externalview = true;
	BE_BaseEntTextures();
	r_refdef.externalview = oxv;

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

	memcpy(r_refdef.m_view, sav, sizeof(r_refdef.m_view));
}

void Sh_GenShadowMap (dlight_t *l,  qbyte *lvis)
{
	int f;
	int smsize = SHADOWMAP_SIZE;
	float proj[16];

	shadowmesh_t *smesh;

	if (!TEXVALID(l->stexture))
	{
		l->stexture = GL_AllocNewTexture("***shadowmap***", smsize, smsize);

		GL_MTBind(0, GL_TEXTURE_2D, l->stexture);
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32_ARB, smsize, smsize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	//	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, smsize, smsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

	smesh = SHM_BuildShadowMesh(l, lvis, NULL, true);

	/*polygon offsets. urgh.*/
	qglEnable(GL_POLYGON_OFFSET_FILL);
	qglPolygonOffset(5, 25);
	BE_SetupForShadowMap();

	/*set framebuffer*/
	GL_BeginRenderBuffer_DepthOnly(l->stexture);
	qglClear (GL_DEPTH_BUFFER_BIT);

	if (l->fov)
	{
		Matrix4x4_CM_Projection_Far(proj, l->fov, l->fov, nearplane, l->radius);
		qglMatrixMode(GL_PROJECTION);
		qglLoadMatrixf(proj);

		/*single face*/
		Sh_GenShadowFace(l, smesh, 0, proj);
	}
	else
	{
		Matrix4x4_CM_Projection_Far(proj, 90, 90, nearplane, l->radius);
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
	srect_t rect;

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

	if (Sh_ScissorForBox(mins, maxs, &rect))
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
		Matrix4x4_CM_Projection_Far(proj, l->fov, l->fov, nearplane, l->radius);
	else
		Matrix4x4_CM_Projection_Far(proj, 90, 90, nearplane, l->radius);
	VectorMA(l->origin, 0, l->axis[0], biasorg);
	Matrix4x4_CM_ModelViewMatrixFromAxis(view, l->axis[0], l->axis[1], l->axis[2], l->origin);

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

	Sh_Scissor(rect);
	qglEnable(GL_STENCIL_TEST);

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

	GLBE_SelectDLight(l, colour);
	BE_SelectMode(l->fov?BEM_SMAPLIGHTSPOT:BEM_SMAPLIGHT);
	Sh_DrawEntLighting(l, colour);

	GL_SelectTexture(7);
	qglDisable(GL_TEXTURE_2D);
	qglMatrixMode(GL_TEXTURE);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
}
#endif




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

/*Fixme: this is brute forced*/
#ifdef warningmsg
#pragma warningmsg("brush shadows are bruteforced")
#endif
static void Sh_DrawBrushModelShadow(dlight_t *dl, entity_t *e)
{
#ifdef GLQUAKE
	int v;
	float *v1, *v2;
	vec3_t v3, v4;
	vec3_t lightorg;

	int i;
	model_t *model;
	msurface_t *surf;

	if (BE_LightCullModel(e->origin, e->model))
		return;

	RotateLightVector((void *)e->axis, e->origin, dl->origin, lightorg);

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
			v3[0] = ( v1[0]-lightorg[0] );
			v3[1] = ( v1[1]-lightorg[1] );
			v3[2] = ( v1[2]-lightorg[2] );
			VectorNormalizeFast(v3);
			VectorScale(v3, PROJECTION_DISTANCE, v3);

			v4[0] = ( v2[0]-lightorg[0] );
			v4[1] = ( v2[1]-lightorg[1] );
			v4[2] = ( v2[2]-lightorg[2] );
			VectorNormalizeFast(v4);
			VectorScale(v4, PROJECTION_DISTANCE, v4);

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
			v3[0] = (v1[0]-lightorg[0]);
			v3[1] = (v1[1]-lightorg[1]);
			v3[2] = (v1[2]-lightorg[2]);
			VectorNormalizeFast(v3);
			VectorScale(v3, PROJECTION_DISTANCE, v3);

			qglVertex3f(v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2]);
		}
		qglEnd();
	}

	BE_PushOffsetShadow(false);
#endif
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

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
		BE_PushOffsetShadow(false);
#endif

	sm = SHM_BuildShadowMesh(dl, lvis, vvis, false);
	if (!sm)
		Sh_DrawBrushModelShadow(dl, &r_worldentity);
	else
	{
		switch (qrenderer)
		{
#ifdef D3DQUAKE
		case QR_DIRECT3D:
			D3DBE_RenderShadowBuffer(sm->numverts, sm->d3d_vbuffer, sm->numindicies, sm->d3d_ibuffer);
			break;
#endif
#ifdef GLQUAKE
		case QR_OPENGL:
			GLBE_RenderShadowBuffer(sm->numverts, sm->vebo[0], sm->verts, sm->numindicies, sm->vebo[1], sm->indicies);
			break;
#endif
		}
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
	int sref;
	int leaf;
	qbyte *lvis;
	srect_t rect;

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
	if (Sh_ScissorForBox(mins, maxs, &rect))
	{
		bench.numscissorculled++;
		return false;	//this doesn't cull often.
	}
	bench.numlights++;

	BE_SelectDLight(dl, colour);
	BE_SelectMode(BEM_STENCIL);

	//The backend doesn't maintain scissor state.
	//The backend doesn't maintain stencil test state either - it needs to be active for more than just stencils, or disabled. its awkward.
	Sh_Scissor(rect);


	switch(qrenderer)
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		{
			int sfrontfail;
			int sbackfail;
			qglEnable(GL_SCISSOR_TEST);
			qglEnable(GL_STENCIL_TEST);

			//FIXME: is it practical to test to see if scissors allow not clearing the stencil buffer?

			/*we don't need all that much stencil buffer depth, and if we don't get enough or have dodgy volumes, wrap if we can*/
		#ifdef I_LIVE_IN_A_FREE_COUNTRY
			sref = 0;
			sbackfail = GL_INCR;
			sfrontfail = GL_DECR;
			if (gl_config.ext_stencil_wrap)
			{	//minimise damage...
				sbackfail = GL_INCR_WRAP_EXT;
				sdecrw = GL_DECR_WRAP_EXT;
			}
		#else
			sref = (1<<gl_stencilbits)-1; /*this is halved for two-sided stencil support, just in case there's no wrap support*/
			sbackfail = GL_DECR;
			sfrontfail = GL_INCR;
			if (gl_config.ext_stencil_wrap)
			{	//minimise damage...
				sbackfail = GL_DECR_WRAP_EXT;
				sfrontfail = GL_INCR_WRAP_EXT;
			}
		#endif
			//our stencil writes.
			if (gl_config.arb_depth_clamp)
				qglEnable(GL_DEPTH_CLAMP_ARB);

		#if 0 //def _DEBUG
		//	if (r_shadows.value == 666)	//testing (visible shadow volumes)
			{
				checkglerror();
				qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				qglColor3f(dl->color[0], dl->color[1], dl->color[2]);
				qglDisable(GL_STENCIL_TEST);
				qglEnable(GL_POLYGON_OFFSET_FILL);
				qglPolygonOffset(-1, -1);
			//	qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				Sh_DrawStencilLightShadows(dl, lvis, vvis, false);
				qglDisable(GL_POLYGON_OFFSET_FILL);
				checkglerror();
				qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		#endif

			if (qglStencilOpSeparateATI)
			{
				sref/=2;
				qglClearStencil(sref);
				qglClear(GL_STENCIL_BUFFER_BIT);
				GL_CullFace(0);

				qglStencilFunc(GL_ALWAYS, 0, ~0);

				qglStencilOpSeparateATI(GL_BACK, GL_KEEP, sbackfail, GL_KEEP);
				qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, sfrontfail, GL_KEEP);

				Sh_DrawStencilLightShadows(dl, lvis, vvis, false);
				qglStencilOpSeparateATI(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_KEEP);

				GL_CullFace(SHADER_CULL_FRONT);

				qglStencilFunc(GL_EQUAL, sref, ~0);
			}
			else if (qglActiveStencilFaceEXT)
			{
				sref/=2;
				/*personally I prefer the ATI way (nvidia method)*/
				qglClearStencil(sref);
				qglClear(GL_STENCIL_BUFFER_BIT);
				GL_CullFace(0);

				qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

				qglActiveStencilFaceEXT(GL_BACK);	
				qglStencilOp(GL_KEEP, sbackfail, GL_KEEP);
				qglStencilFunc(GL_ALWAYS, 0, ~0 );

				qglActiveStencilFaceEXT(GL_FRONT);
				qglStencilOp(GL_KEEP, sfrontfail, GL_KEEP);
				qglStencilFunc(GL_ALWAYS, 0, ~0 );

				Sh_DrawStencilLightShadows(dl, lvis, vvis, false);

				qglActiveStencilFaceEXT(GL_BACK);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				qglStencilFunc(GL_ALWAYS, 0, ~0 );

				qglActiveStencilFaceEXT(GL_FRONT);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				qglStencilFunc(GL_EQUAL, sref, ~0 );

				qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
			}
			else //your graphics card sucks and lacks efficient stencil shadow techniques.
			{	//centered around 0. Will only be increased then decreased less.
				qglClearStencil(sref);
				qglClear(GL_STENCIL_BUFFER_BIT);

				qglStencilFunc(GL_ALWAYS, 0, ~0);

				GL_CullFace(SHADER_CULL_BACK);
				qglStencilOp(GL_KEEP, sbackfail, GL_KEEP);
				Sh_DrawStencilLightShadows(dl, lvis, vvis, false);

				GL_CullFace(SHADER_CULL_FRONT);
				qglStencilOp(GL_KEEP, sfrontfail, GL_KEEP);
				Sh_DrawStencilLightShadows(dl, lvis, vvis, true);

				qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				qglStencilFunc(GL_EQUAL, sref, ~0);
			}
			if (gl_config.arb_depth_clamp)
				qglDisable(GL_DEPTH_CLAMP_ARB);
			//end stencil writing.

			/*stencil writing probably changed the vertex pointer, and our backend caches it*/
			PPL_RevertToKnownState();

		#if 0	//draw the stencil stuff to the red channel
			qglMatrixMode(GL_PROJECTION);
			qglPushMatrix();
			qglMatrixMode(GL_MODELVIEW);
			qglPushMatrix();
			GL_Set2D();

			{
				qglColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
				qglStencilFunc(GL_GREATER, sref, ~0);
				R2D_ConsoleBackground(vid.height);

				qglColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
				qglStencilFunc(GL_LESS, sref, ~0);
				R2D_ConsoleBackground(vid.height);

				qglColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
				qglStencilFunc(GL_EQUAL, sref, ~0);
				R2D_ConsoleBackground(vid.height);
			}

			qglMatrixMode(GL_PROJECTION);
			qglPopMatrix();
			qglMatrixMode(GL_MODELVIEW);
			qglPopMatrix();
		#endif


			BE_SelectMode(BEM_LIGHT);
			Sh_DrawEntLighting(dl, colour);
			qglDisable(GL_STENCIL_TEST);
			qglStencilFunc( GL_ALWAYS, 0, ~0 );
		}
		break;
#endif
#ifdef D3DQUAKE
	case QR_DIRECT3D:
		sref = (1<<8)-1;
		sref/=2;

		/*clear the stencil buffer*/
		IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_STENCIL, D3DCOLOR_XRGB(0, 0, 0), 1.0f, sref);

		/*set up 2-sided stenciling*/
		D3DBE_Cull(0);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILENABLE, true);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_TWOSIDEDSTENCILMODE, true);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILZFAIL, D3DSTENCILOP_DECR);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_INCR);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);

		/*draw the shadows*/
		Sh_DrawStencilLightShadows(dl, lvis, vvis, false);

		//disable stencil writing, switch culling back to normal
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_TWOSIDEDSTENCILMODE, false);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_CW);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILFUNC, D3DCMP_EQUAL);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILREF, sref);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILMASK, ~0);

		/*draw the light*/
		BE_SelectMode(BEM_LIGHT);
		Sh_DrawEntLighting(dl, colour);

		/*okay, no more stencil stuff*/
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILENABLE, false);
		break;
#endif
	}

	return true;
}

static void Sh_DrawShadowlessLight(dlight_t *dl, vec3_t colour, qbyte *vvis)
{
	vec3_t mins, maxs;
	srect_t rect;

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

		SHM_BuildShadowMesh(dl, lvis, vvis, true);

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


//sets up the gl scissor (actually just culls to view)
	if (Sh_ScissorForBox(mins, maxs, &rect))
	{
		bench.numscissorculled++;
		return;	//was culled.
	}

	switch(qrenderer)
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		//so state doesn't linger
		qglDisable(GL_SCISSOR_TEST);
		if (qglDepthBoundsEXT)
			qglDisable(GL_DEPTH_BOUNDS_TEST_EXT);
		break;
#endif
	}

	bench.numlights++;

	BE_SelectDLight(dl, colour);
	BE_SelectMode(BEM_LIGHT);
	Sh_DrawEntLighting(dl, colour);
}

void GLBE_SubmitMeshes (qboolean drawworld, batch_t **blist, int start, int stop);
void Sh_DrawCrepuscularLight(dlight_t *dl, float *colours, batch_t **batches)
{
#ifdef GLQUAKE
	static mesh_t mesh;
	static vecV_t xyz[4] =
	{
		{-1,-1,-1},
		{-1,1,-1},
		{1,1,-1},
		{1,-1,-1}
	};
	static vec2_t tc[4] =
	{
		{0,0},
		{0,1},
		{1,1},
		{1,0}
	};
	static index_t idx[6] =
	{
		0,1,2,
		0,2,3
	};
	if (qrenderer != QR_OPENGL)
		return;

	mesh.numindexes = 6;
	mesh.numvertexes = 4;
	mesh.xyz_array = xyz;
	mesh.st_array = tc;
	mesh.indexes = idx;

	/*
	a crepuscular light (seriously, that's the correct spelling) is one that gives 'god rays', rather than regular light.
	our implementation doesn't cast shadows. this allows it to actually be outside the map, and to shine through cloud layers in the sky.
	we could cast shadows if the light was actually inside, I suppose.
	Anyway, its done using an FBO, where everything but the sky is black (stuff that occludes the sky is black too).
	which is then blitted onto the screen in 2d-space.
	*/

	/*requires an FBO, as stated above*/
	if (!gl_config.ext_framebuffer_objects)
		return;
//checkglerror();
	if (!crepuscular_fbo_id)
	{
		qglGenFramebuffersEXT(1, &crepuscular_fbo_id);
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, crepuscular_fbo_id);
		qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		qglReadBuffer(GL_NONE);

		crepuscular_texture_id = GL_AllocNewTexture("***crepusculartexture***", vid.pixelwidth, vid.pixelheight);

		/*FIXME: requires npot*/
		crepuscular_shader = R_RegisterShader("crepuscular_screen",
			"{\n"
				"program crepuscular_rays\n"
				"{\n"
					"map \"***crepusculartexture***\"\n"
					"blend add\n"
				"}\n"
			"}\n"
			);
		GL_MTBind(0, GL_TEXTURE_2D, crepuscular_texture_id);
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid.pixelwidth, vid.pixelheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, crepuscular_texture_id.num, 0);
	}
	else
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, crepuscular_fbo_id);
//checkglerror();
	BE_SelectMode(BEM_CREPUSCULAR);
	BE_SelectDLight(dl, colours);
//checkglerror();
	GLBE_SubmitMeshes(true, batches, SHADER_SORT_PORTAL, SHADER_SORT_BLEND);
//checkglerror();
	//fixme: check regular post-proc
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	checkglerror();
	BE_SelectMode(BEM_STANDARD);

	GLBE_DrawMesh_Single(crepuscular_shader, &mesh, NULL, &crepuscular_shader->defaulttextures, 0);
	checkglerror();
#endif
}

void Sh_PreGenerateLights(void)
{
	unsigned int ignoreflags;
	dlight_t *dl;
	qboolean shadow;
	int leaf;
	qbyte *lvis;
	qbyte	lvisb[MAX_MAP_LEAFS/8];
	int i;

	ignoreflags = (r_shadow_realtime_world.value?LFLAG_REALTIMEMODE:LFLAG_NORMALMODE);

	for (dl = cl_dlights+rtlights_first, i=rtlights_first; i<rtlights_max; i++, dl++)
	{
		if (!dl->radius)
			continue;	//dead

		if (!(dl->flags & ignoreflags))
			continue;

		if (dl->flags & LFLAG_CREPUSCULAR)
			continue;
		else if (((!dl->die)?!r_shadow_realtime_world_shadows.ival:!r_shadow_realtime_dlight_shadows.ival) || dl->flags & LFLAG_NOSHADOWS)
			shadow = false;
		else if (dl->flags & LFLAG_SHADOWMAP)
			shadow = false;
		else
			shadow = true;

		leaf = cl.worldmodel->funcs.LeafnumForPoint(cl.worldmodel, dl->origin);
		lvis = cl.worldmodel->funcs.LeafPVS(cl.worldmodel, leaf, lvisb, sizeof(lvisb));

		SHM_BuildShadowMesh(dl, lvis, NULL, !shadow);
	}
}

void Sh_DrawLights(qbyte *vis, batch_t **mbatches)
{
	vec3_t colour;
	dlight_t *dl;
	int i;
	unsigned int ignoreflags;
	extern cvar_t r_shadow_realtime_world, r_shadow_realtime_dlight;
	extern cvar_t r_shadow_realtime_world_shadows, r_shadow_realtime_dlight_shadows;

	if (!r_shadow_realtime_world.ival && !r_shadow_realtime_dlight.ival)
	{
		return;
	}

	switch(qrenderer)
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		/*no stencil?*/
		if (gl_config.nofixedfunc)
		{
			Con_Printf("FTE does not support stencil shadows without a fixed-function pipeline\n");
			r_shadow_realtime_world.ival = 0;
			r_shadow_realtime_dlight.ival = 0;
			return;
		}

		if (!gl_config.arb_shader_objects)
		{
			Con_Printf("Missing GL extensions: switching off realtime lighting.\n");
			r_shadow_realtime_world.ival = 0;
			r_shadow_realtime_dlight.ival = 0;
			return;
		}
		break;
#endif
#ifdef D3DQUAKE
	case QR_DIRECT3D:
		#ifdef GLQUAKE
		//the code still has a lot of ifdefs, so will crash if you try it in a merged build.
		//its not really usable in d3d-only builds either, so no great loss.
		return;
		#endif
		break;
#endif
	default:
		return;
	}

	ignoreflags = (r_shadow_realtime_world.value?LFLAG_REALTIMEMODE:LFLAG_NORMALMODE);

	for (dl = cl_dlights+rtlights_first, i=rtlights_first; i<rtlights_max; i++, dl++)
	{
		if (!dl->radius)
			continue;	//dead

		if (!(dl->flags & ignoreflags))
			continue;

		colour[0] = dl->color[0];
		colour[1] = dl->color[1];
		colour[2] = dl->color[2];
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

		if (dl->flags & LFLAG_CREPUSCULAR)
			Sh_DrawCrepuscularLight(dl, colour, mbatches);
		else if (((!dl->die)?!r_shadow_realtime_world_shadows.ival:!r_shadow_realtime_dlight_shadows.ival) || dl->flags & LFLAG_NOSHADOWS)
		{
			Sh_DrawShadowlessLight(dl, colour, vis);
		}
		else if (dl->flags & LFLAG_SHADOWMAP)
		{
#ifdef GLQUAKE
			Sh_DrawShadowMapLight(dl, colour, vis);
#endif
		}
		else
		{
			Sh_DrawStencilLight(dl, colour, vis);
		}
	}

	switch(qrenderer)
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		qglDisable(GL_SCISSOR_TEST);
		if (qglDepthBoundsEXT)
			qglDisable(GL_DEPTH_BOUNDS_TEST_EXT);
		break;
#endif
	}

	BE_SelectMode(BEM_STANDARD);

//	if (developer.value)
//	Con_Printf("%i lights drawn, %i frustum culled, %i pvs culled, %i scissor culled\n", bench.numlights, bench.numfrustumculled, bench.numpvsculled, bench.numscissorculled);
//	memset(&bench, 0, sizeof(bench));
}
#endif
#endif
