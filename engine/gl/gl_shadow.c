#include "quakedef.h"

/*
room for improvement:
There is no screen-space culling of lit surfaces.
model meshes are interpolated multiple times per frame
*/

//#define DBG_COLOURNOTDEPTH


#if defined(RTLIGHTS) && !defined(SERVERONLY)

#ifdef VKQUAKE
#include "../vk/vkrenderer.h"
#endif
#include "glquake.h"
#include "shader.h"

#ifdef D3D9QUAKE
#include "shader.h"
#if !defined(HMONITOR_DECLARED) && (WINVER < 0x0500)
    #define HMONITOR_DECLARED
    DECLARE_HANDLE(HMONITOR);
#endif
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 pD3DDev9;
void D3D9BE_Cull(unsigned int sflags);
void D3D9BE_RenderShadowBuffer(unsigned int numverts, IDirect3DVertexBuffer9 *vbuf, unsigned int numindicies, IDirect3DIndexBuffer9 *ibuf);
#endif
#ifdef D3D11QUAKE
void D3D11BE_GenerateShadowBuffer(void **vbuf, vecV_t *verts, int numverts, void **ibuf, index_t *indicies, int numindicies);
void D3D11BE_RenderShadowBuffer(unsigned int numverts, void *vbuf, unsigned int numindicies, void *ibuf);
void D3D11_DestroyShadowBuffer(void *vbuf, void *ibuf);
void D3D11BE_DoneShadows(void);
#endif
#ifdef VKQUAKE
#endif
void GLBE_RenderShadowBuffer(unsigned int numverts, int vbo, vecV_t *verts, unsigned numindicies, int ibo, index_t *indicies);

static void SHM_Shutdown(void);

#define SHADOWMAP_SIZE 512

#define PROJECTION_DISTANCE (float)(dl->radius*2)//0x7fffffff

#ifdef GLQUAKE
static texid_t shadowmap[2];
static int shadow_fbo_id;
static int shadow_fbo_depth_num;
#endif
texid_t crepuscular_texture_id;
fbostate_t crepuscular_fbo;
shader_t *crepuscular_shader;

cvar_t r_shadow_shadowmapping_nearclip = CVAR("r_shadow_shadowmapping_nearclip", "1");
cvar_t r_shadow_shadowmapping_bias = CVAR("r_shadow_shadowmapping_bias", "0.03");
cvar_t r_shadow_scissor = CVARD("r_shadow_scissor", "1", "constrains stencil shadows to the onscreen box that contains the maxmium extents of the light. This avoids unnecessary work.");

cvar_t r_shadow_realtime_world				= CVARFD ("r_shadow_realtime_world", "0", CVAR_ARCHIVE, "Enables the use of static/world realtime lights.");
cvar_t r_shadow_realtime_world_shadows		= CVARF ("r_shadow_realtime_world_shadows", "1", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_world_lightmaps	= CVARFD ("r_shadow_realtime_world_lightmaps", "0", 0, "Specifies how much of the map's normal lightmap to retain when using world realtime lights. 0 completely replaces lighting.");
cvar_t r_shadow_realtime_dlight				= CVARFD ("r_shadow_realtime_dlight", "1", CVAR_ARCHIVE, "Enables the use of dynamic realtime lights, allowing explosions to use bumpmaps etc properly.");
cvar_t r_shadow_realtime_dlight_shadows		= CVARFD ("r_shadow_realtime_dlight_shadows", "1", CVAR_ARCHIVE, "Allows dynamic realtime lights to cast shadows as they move.");
cvar_t r_shadow_realtime_dlight_ambient		= CVAR ("r_shadow_realtime_dlight_ambient", "0");
cvar_t r_shadow_realtime_dlight_diffuse		= CVAR ("r_shadow_realtime_dlight_diffuse", "1");
cvar_t r_shadow_realtime_dlight_specular	= CVAR ("r_shadow_realtime_dlight_specular", "4");	//excessive, but noticable. its called stylized, okay? shiesh, some people
cvar_t r_editlights_import_radius			= CVAR ("r_editlights_import_radius", "1");
cvar_t r_editlights_import_ambient			= CVAR ("r_editlights_import_ambient", "0");
cvar_t r_editlights_import_diffuse			= CVAR ("r_editlights_import_diffuse", "1");
cvar_t r_editlights_import_specular			= CVAR ("r_editlights_import_specular", "1");	//excessive, but noticable. its called stylized, okay? shiesh, some people
cvar_t r_shadow_playershadows				= CVARD ("r_shadow_playershadows", "1", "Controls the presence of shadows on the local player.");
cvar_t r_shadow_shadowmapping				= CVARD ("r_shadow_shadowmapping", "1", "Enables soft shadows instead of stencil shadows.");
cvar_t r_shadow_shadowmapping_precision		= CVARD ("r_shadow_shadowmapping_precision", "1", "Scales the shadowmap detail level up or down.");
extern cvar_t r_shadow_shadowmapping_nearclip;
extern cvar_t r_shadow_shadowmapping_bias;
cvar_t r_sun_dir							= CVARD ("r_sun_dir", "0.2 0.5 0.8", "Specifies the direction that crepusular rays appear along");
cvar_t r_sun_colour							= CVARFD ("r_sun_colour", "0 0 0", CVAR_ARCHIVE, "Specifies the colour of sunlight that appears in the form of crepuscular rays.");

static void Sh_DrawEntLighting(dlight_t *light, vec3_t colour);

static qbyte	lvisb[(MAX_MAP_LEAFS+7)>>3];

/*
called on framebuffer resize.
flushes textures so they can be regenerated at the real size
*/
void Sh_Reset(void)
{
#ifdef GLQUAKE
	if (shadow_fbo_id)
	{
		qglDeleteFramebuffersEXT(1, &shadow_fbo_id);
		shadow_fbo_id = 0;
		shadow_fbo_depth_num = 0;
	}
	if (shadowmap[0])
	{
		Image_DestroyTexture(shadowmap[0]);
		shadowmap[0] = r_nulltex;
	}
	if (shadowmap[1])
	{
		Image_DestroyTexture(shadowmap[1]);
		shadowmap[1] = r_nulltex;
	}
	if (crepuscular_texture_id)
	{
		Image_DestroyTexture(crepuscular_texture_id);
		crepuscular_texture_id = r_nulltex;
	}
	GLBE_FBO_Destroy(&crepuscular_fbo);
#endif
}
void Sh_Shutdown(void)
{
	Sh_Reset();
	SHM_Shutdown();
}



typedef struct {
	unsigned int count;
	unsigned int max;
	texture_t *tex;
	vbo_t *vbo;
	mesh_t **s;
} shadowmeshbatch_t;
typedef struct shadowmesh_s
{
	enum
	{
		SMT_STENCILVOLUME,	//build edges mesh (and surface list)
		SMT_SHADOWMAP,		//build front faces mesh (and surface list)
		SMT_SHADOWLESS		//build surface list only
	} type;
	unsigned int numindicies;
	unsigned int maxindicies;
	index_t *indicies;

	unsigned int numverts;
	unsigned int maxverts;
	vecV_t *verts;

	//we also have a list of all the surfaces that this light lights.
	unsigned int numbatches;
	shadowmeshbatch_t *batches;

	unsigned int leafbytes;
	unsigned char *litleaves;

#ifdef VKQUAKE
	struct vk_shadowbuffer *vkbuffer;
#endif
#ifdef GLQUAKE
	GLuint vebo[2];
#endif
#ifdef D3D9QUAKE
	IDirect3DVertexBuffer9	*d3d9_vbuffer;
	IDirect3DIndexBuffer9	*d3d9_ibuffer;
#endif
#ifdef D3D11QUAKE
	void	*d3d11_vbuffer;
	void	*d3d11_ibuffer;
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

static void SHM_MeshFrontOnly(int numverts, vecV_t *verts, int numidx, index_t *idx)
{
	int first = sh_shmesh->numverts;
	int v, i;
	vecV_t *outv;
	index_t *outi;

	/*make sure there's space*/
	v = (sh_shmesh->numverts+numverts + inc)&~(inc-1);	//and a bit of padding
	if (sh_shmesh->maxverts < v)
	{
		v += 1024;
		sh_shmesh->maxverts = v;
		sh_shmesh->verts = BZ_Realloc(sh_shmesh->verts, v * sizeof(*sh_shmesh->verts));
	}

	outv = sh_shmesh->verts + sh_shmesh->numverts;
	for (v = 0; v < numverts; v++)
	{
		VectorCopy(verts[v], outv[v]);
	}

	v = (sh_shmesh->numindicies+numidx + inc)&~(inc-1);	//and a bit of padding
	if (sh_shmesh->maxindicies < v)
	{
		v += 1024;
		sh_shmesh->maxindicies = v;
		sh_shmesh->indicies = BZ_Realloc(sh_shmesh->indicies, v * sizeof(*sh_shmesh->indicies));
	}
	outi = sh_shmesh->indicies + sh_shmesh->numindicies;
	for (i = 0; i < numidx; i++)
	{
		outi[i] = first + idx[i];
	}

	sh_shmesh->numverts += numverts;
	sh_shmesh->numindicies += numidx;
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
		v += 1024;
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
		v += 1024;
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

	i = surf->sbatch->shadowbatch;
	if (i < 0)
		return;

	if (sh_shmesh->batches[i].count == sh_shmesh->batches[i].max)
	{
		sh_shmesh->batches[i].max += 64;
		sh_shmesh->batches[i].s = BZ_Realloc(sh_shmesh->batches[i].s, sizeof(void*)*(sh_shmesh->batches[i].max));
	}
	sh_shmesh->batches[i].s[sh_shmesh->batches[i].count] = surf->mesh;
	sh_shmesh->batches[i].count++;
}

static void SHM_Shadow_Cache_Leaf(mleaf_t *leaf)
{
	int i;

	i = (leaf - cl.worldmodel->leafs)-1;
	sh_shmesh->litleaves[i>>3] |= 1<<(i&7);
}

static void SH_FreeShadowMesh_(shadowmesh_t *sm)
{
	unsigned int i;
	for (i = 0; i < sm->numbatches; i++)
		Z_Free(sm->batches[i].s);
	sm->numbatches = 0;
	Z_Free(sm->batches);
	sm->batches = NULL;
	Z_Free(sm->indicies);
	sm->indicies = NULL;
	Z_Free(sm->verts);
	sm->verts = NULL;
	sm->numindicies = 0;
	sm->numverts = 0;

	switch (qrenderer)
	{
	case QR_NONE:
	case QR_SOFTWARE:
	default:
		break;

#ifdef GLQUAKE
	case QR_OPENGL:
		if (qglDeleteBuffersARB)
			qglDeleteBuffersARB(2, sm->vebo);
		sm->vebo[0] = 0;
		sm->vebo[1] = 0;
		break;
#endif
#ifdef VKQUAKE
	case QR_VULKAN:
		VKBE_DestroyShadowBuffer(sm->vkbuffer);
		sm->vkbuffer = NULL;
		break;
#endif
#ifdef D3D9QUAKE
	case QR_DIRECT3D9:
		if (sm->d3d9_ibuffer)
			IDirect3DIndexBuffer9_Release(sm->d3d9_ibuffer);
		sm->d3d9_ibuffer = NULL;
		if (sm->d3d9_vbuffer)
			IDirect3DVertexBuffer9_Release(sm->d3d9_vbuffer);
		sm->d3d9_vbuffer = NULL;
		break;
#endif
#ifdef D3D11QUAKE
	case QR_DIRECT3D11:
		D3D11_DestroyShadowBuffer(sm->d3d11_vbuffer, sm->d3d11_ibuffer);
		sm->d3d11_vbuffer = NULL;
		sm->d3d11_ibuffer = NULL;
		break;
#endif
	}
}
void SH_FreeShadowMesh(shadowmesh_t *sm)
{
	SH_FreeShadowMesh_(sm);
	Z_Free(sm);
}

static void SH_CalcShadowBatches(model_t *mod)
{
	int s;
	batch_t *b;
	batch_t *l = NULL;
	int sb;

	l = NULL;
	for (s = 0; s < SHADER_SORT_COUNT; s++)
	{
		for (b = mod->batches[s]; b; b = b->next)
		{
			if (!l || l->vbo != b->vbo || l->texture != b->texture)
			{
				b->shadowbatch = mod->numshadowbatches++;
				l = b;
			}
			else
				b->shadowbatch = l->shadowbatch;
		}
	}

	if (!mod->numshadowbatches)
		mod->shadowbatches = NULL;
	else
	{
		l = NULL;
		sb = 0;
		mod->shadowbatches = BZ_Malloc(sizeof(*mod->shadowbatches)*mod->numshadowbatches);
		for (s = 0; s < SHADER_SORT_COUNT; s++)
		{
			for (b = mod->batches[s]; b; b = b->next)
			{
				if (!l || l->vbo != b->vbo || l->texture != b->texture)
				{
					mod->shadowbatches[sb].tex = b->texture;
					mod->shadowbatches[sb].vbo = b->vbo;
					sb++;
					l = b;
				}
			}
		}
	}
}

static void SHM_BeginShadowMesh(dlight_t *dl, int type)
{
	unsigned int i;
	unsigned int lb;
	sh_vertnum = 0;

	lb = (cl.worldmodel->numclusters+7)/8;
	if (!dl->die || !dl->key)
	{
		sh_shmesh = dl->worldshadowmesh;
		if (!sh_shmesh || sh_shmesh->leafbytes != lb)
		{
			/*this shouldn't happen too often*/
			if (sh_shmesh)
			{	//FIXME: if the light is the same light, reuse the memory allocations where possible...
				SH_FreeShadowMesh(sh_shmesh);
			}

			/*Create a new shadowmesh for this light*/
			sh_shmesh = Z_Malloc(sizeof(*sh_shmesh) + lb);
			sh_shmesh->leafbytes = lb;
			sh_shmesh->litleaves = (unsigned char*)(sh_shmesh+1);

			dl->worldshadowmesh = sh_shmesh;
		}
		memset(sh_shmesh->litleaves, 0, sh_shmesh->leafbytes);
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
	sh_shmesh->type = type;

	if (!cl.worldmodel->numshadowbatches)
	{
		SH_CalcShadowBatches(cl.worldmodel);
	}

	if (sh_shmesh->numbatches != cl.worldmodel->numshadowbatches)
	{
		if (sh_shmesh->batches)
		{
			for (i = 0; i < sh_shmesh->numbatches; i++)
				Z_Free(sh_shmesh->batches[i].s);
			Z_Free(sh_shmesh->batches);
		}
		sh_shmesh->batches = Z_Malloc(sizeof(shadowmeshbatch_t)*cl.worldmodel->numshadowbatches);
		sh_shmesh->numbatches=cl.worldmodel->numshadowbatches;
	}

	for (i = 0; i < sh_shmesh->numbatches; i++)
	{
		sh_shmesh->batches[i].count = 0;
	}
}
static struct shadowmesh_s *SHM_FinishShadowMesh(dlight_t *dl)
{
	if (sh_shmesh != &sh_tempshmesh || 1)
	{
		switch (qrenderer)
		{
		case QR_NONE:
		case QR_SOFTWARE:
		default:
			break;

#ifdef GLQUAKE
		case QR_OPENGL:
			if (!qglGenBuffersARB)
				return sh_shmesh;
			if (!sh_shmesh->vebo[0])
				qglGenBuffersARB(2, sh_shmesh->vebo);

			GL_DeselectVAO();
			GL_SelectVBO(sh_shmesh->vebo[0]);
			qglBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(*sh_shmesh->verts) * sh_shmesh->numverts, sh_shmesh->verts, GL_STATIC_DRAW_ARB);

			GL_SelectEBO(sh_shmesh->vebo[1]);
			qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(*sh_shmesh->indicies) * sh_shmesh->numindicies, sh_shmesh->indicies, GL_STATIC_DRAW_ARB);
			break;
#endif
#ifdef VKQUAKE
		case QR_VULKAN:
			VKBE_DestroyShadowBuffer(sh_shmesh->vkbuffer);
			sh_shmesh->vkbuffer = VKBE_GenerateShadowBuffer(sh_shmesh->verts, sh_shmesh->numverts, sh_shmesh->indicies, sh_shmesh->numindicies, sh_shmesh == &sh_tempshmesh);
			break;
#endif
#ifdef D3D9QUAKE
		case QR_DIRECT3D9:
			if (sh_shmesh->numindicies && sh_shmesh->numverts)
			{
				void *map;
				IDirect3DDevice9_CreateIndexBuffer(pD3DDev9, sizeof(index_t) * sh_shmesh->numindicies, 0, D3DFMT_QINDEX, D3DPOOL_MANAGED, &sh_shmesh->d3d9_ibuffer, NULL);
				IDirect3DIndexBuffer9_Lock(sh_shmesh->d3d9_ibuffer, 0, sizeof(index_t) * sh_shmesh->numindicies, &map, D3DLOCK_DISCARD);
				memcpy(map, sh_shmesh->indicies, sizeof(index_t) * sh_shmesh->numindicies);
				IDirect3DIndexBuffer9_Unlock(sh_shmesh->d3d9_ibuffer);

				IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, sizeof(vecV_t) * sh_shmesh->numverts, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &sh_shmesh->d3d9_vbuffer, NULL);
				IDirect3DVertexBuffer9_Lock(sh_shmesh->d3d9_vbuffer, 0, sizeof(vecV_t) * sh_shmesh->numverts, &map, D3DLOCK_DISCARD);
				memcpy(map, sh_shmesh->verts, sizeof(vecV_t) * sh_shmesh->numverts);
				IDirect3DVertexBuffer9_Unlock(sh_shmesh->d3d9_vbuffer);

			}
			break;
#endif
#ifdef D3D11QUAKE	
		case QR_DIRECT3D11:
			D3D11BE_GenerateShadowBuffer(&sh_shmesh->d3d11_vbuffer, sh_shmesh->verts, sh_shmesh->numverts, &sh_shmesh->d3d11_ibuffer, sh_shmesh->indicies, sh_shmesh->numindicies);
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
} *edge;
static int firstedge;
static int maxedge;

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
				s = (l - s)*surf->texinfo->vecscale[0];
				l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
				t = l+0.5;if (t < 0) t = 0;else if (t > surf->extents[1]) t = surf->extents[1];
				t = (l - t)*surf->texinfo->vecscale[1];
				// compare to minimum light
				if ((s*s+t*t+dot*dot) < maxdist)
				{
					SHM_Shadow_Cache_Surface(surf);
					if (sh_shmesh->type == SMT_SHADOWMAP)
					{
						SHM_MeshFrontOnly(surf->mesh->numvertexes, surf->mesh->xyz_array, surf->mesh->numindexes, surf->mesh->indexes);
						continue;
					}
					if (sh_shmesh->type != SMT_STENCILVOLUME)
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

#ifdef Q2BSPS
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

		if (pleaf->cluster >= 0)
			sh_shmesh->litleaves[pleaf->cluster>>3] |= 1<<(pleaf->cluster&7);

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
				s = l;if (s < 0) s = 0;else if (s > surf->extents[0]) s = surf->extents[0];
				s = (l - s)*surf->texinfo->vecscale[0];
				l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
				t = l;if (t < 0) t = 0;else if (t > surf->extents[1]) t = surf->extents[1];
				t = (l - t)*surf->texinfo->vecscale[1];
				// compare to minimum light
				if ((s*s+t*t+dot*dot) < maxdist)
				{
					SHM_Shadow_Cache_Surface(surf);
					if (sh_shmesh->type == SMT_SHADOWMAP)
					{
						SHM_MeshFrontOnly(surf->mesh->numvertexes, surf->mesh->xyz_array, surf->mesh->numindexes, surf->mesh->indexes);
						continue;
					}
					if (sh_shmesh->type != SMT_STENCILVOLUME)
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
			if (lvis[cluster>>3] & /*vvis[cluster>>3] &*/ (1<<(cluster&7)))
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

	if (!lvis)
		return;

	//variation on mark leaves
	for (i=0 ; i<cl.worldmodel->numclusters ; i++)
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
		if (leaf->cluster >= 0)
			sh_shmesh->litleaves[leaf->cluster>>3] |= 1<<(leaf->cluster&7);

	// mark the polygons
		msurf = leaf->firstmarksurface;
		for (i=0 ; i<leaf->nummarksurfaces ; i++, msurf++)
		{
			surf = *msurf;

			//only check each surface once. it can appear in multiple leafs.
			if (surf->shadowframe == sh_shadowframe)
				continue;
			surf->shadowframe = sh_shadowframe;

			//FIXME: radius check
			SHM_Shadow_Cache_Surface(surf);
			if (sh_shmesh->type == SMT_SHADOWMAP)
				SHM_MeshFrontOnly(surf->mesh->numvertexes, surf->mesh->xyz_array, surf->mesh->numindexes, surf->mesh->indexes);
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

static void SHM_Shutdown(void)
{
	SH_FreeShadowMesh_(&sh_tempshmesh);
	BZ_Free(sh_tempshmesh.litleaves);
	sh_tempshmesh.litleaves = NULL;
	sh_tempshmesh.leafbytes = 0;
	free(cv.tris);
	free(cv.edges);
	free(cv.points);
	memset(&cv, 0, sizeof(cv));
}

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
	shadowmeshbatch_t *sms;
	unsigned int tno;
	unsigned int sno;
	int i, e;
	mesh_t *sm;
	vec3_t ext;
	float sc;
	cv.numedges = 0;
	cv.numpoints = 0;
	cv.numtris = 0;

	for (tno = 0; tno < sh_shmesh->numbatches; tno++)
	{
		sms = &sh_shmesh->batches[tno];
		if (!sms->count)
			continue;
		if ((cl.worldmodel->shadowbatches[tno].tex->shader->flags & (SHADER_BLEND|SHADER_NODRAW)))
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
		
		sc = dl->radius * VectorNormalize(ext);

		/*back face*/
		sh_shmesh->verts[(i * 2) + 1][0] = cv.points[i][0] + ext[0] * sc;
		sh_shmesh->verts[(i * 2) + 1][1] = cv.points[i][1] + ext[1] * sc;
		sh_shmesh->verts[(i * 2) + 1][2] = cv.points[i][2] + ext[2] * sc;
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

static struct shadowmesh_s *SHM_BuildShadowMesh(dlight_t *dl, unsigned char *lvis, unsigned char *vvis, int type)
{
	float *v1, *v2;
	vec3_t v3, v4;

	if (dl->worldshadowmesh && !dl->rebuildcache && dl->worldshadowmesh->type == type)
		return dl->worldshadowmesh;

	if (!lvis)
	{
		int clus;
		clus = cl.worldmodel->funcs.ClusterForPoint(cl.worldmodel, dl->origin);
		lvis = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, clus, lvisb, sizeof(lvisb));
	}

	firstedge=0;
	if (maxedge < cl.worldmodel->numedges)
	{
		maxedge = cl.worldmodel->numedges;
		Z_Free(edge);
		edge = Z_Malloc(sizeof(*edge) * maxedge);
	}

	if (cl.worldmodel->type == mod_brush)
	{
		switch(cl.worldmodel->fromgame)
		{
		case fg_quake:
		case fg_halflife:
			/*if (!dl->die)
			{
				SHM_BeginShadowMesh(dl, true);
				SHM_MarkLeavesQ1(dl, lvis);
				SHM_RecursiveWorldNodeQ1_r(dl, cl.worldmodel->nodes);
				if (!surfonly)
					SHM_ComposeVolume_BruteForce(dl);
			}
			else*/
			{
				SHM_BeginShadowMesh(dl, type);

				SHM_MarkLeavesQ1(dl, lvis);
				SHM_RecursiveWorldNodeQ1_r(dl, cl.worldmodel->nodes);
			}
			break;
#ifdef Q2BSPS
		case fg_quake2:
			SHM_BeginShadowMesh(dl, type);
			SHM_MarkLeavesQ2(dl, lvis, vvis);
			SHM_RecursiveWorldNodeQ2_r(dl, cl.worldmodel->nodes);
			break;
#endif
#ifdef Q3BSPS
		case fg_quake3:
			/*q3 doesn't have edge info*/
			SHM_BeginShadowMesh(dl, type);

			sh_shadowframe++;
			SHM_RecursiveWorldNodeQ3_r(dl, cl.worldmodel->nodes);
			if (type == SMT_STENCILVOLUME)
				SHM_ComposeVolume_BruteForce(dl);
			break;
#endif
		default:
			SHM_BeginShadowMesh(dl, type);
			sh_shadowframe++;

			{
				int cluster = cl.worldmodel->funcs.ClusterForPoint(cl.worldmodel, dl->origin);
				sh_shmesh->litleaves[cluster>>3] |= 1<<(cluster&7);
			}
			break;
		}
	}
	else
	{
		SHM_BeginShadowMesh(dl, type);
		sh_shadowframe++;
	}

	/*generate edge polys for map types that need it (q1/q2)*/
	switch (type)
	{
	case SMT_STENCILVOLUME:
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
		break;
	}

	return SHM_FinishShadowMesh(dl);
}













static qboolean Sh_VisOverlaps(qbyte *v1, qbyte *v2)
{
	int i, m;
	if (!v2 || !v1)
		return true;
	m = (cl.worldmodel->numclusters+7)>>3;

	for (i=(m&~3) ; i<m ; i++)
	{
		if (v1[i] & v2[i])
			return true;
	}
	m>>=2;
	for (i=0 ; i<m ; i++)
	{
		if (((unsigned int*)v1)[i] & ((unsigned int*)v2)[i])
			return true;
	}
	return false;
}

#if 1
#define Sh_LeafInView Sh_VisOverlaps
#else
static qboolean Sh_LeafInView(qbyte *lightvis, qbyte *vvis)
{
	int i;
	int m = (cl.worldmodel->numvisleafs);
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
#endif


/*
static void Sh_Scissor (srect_t *r)
{
	//float xs = vid.pixelwidth / (float)vid.width, ys = vid.pixelheight / (float)vid.height;
	switch(qrenderer)
	{
	case QR_NONE:
	case QR_SOFTWARE:
	case QR_DIRECT3D11:
	default:
		break;

	case QR_OPENGL:
#ifdef GLQUAKE
		qglScissor(
			floor(r_refdef.pxrect.x + r->x*r_refdef.pxrect.width),
			floor((r_refdef.pxrect.y + r->y*r_refdef.pxrect.height) - r_refdef.pxrect.height),
			ceil(r->width * r_refdef.pxrect.width),
			ceil(r->height * r_refdef.pxrect.height));
		qglEnable(GL_SCISSOR_TEST);

		if (qglDepthBoundsEXT)
		{
			qglDepthBoundsEXT(r->dmin, r->dmax);
			qglEnable(GL_DEPTH_BOUNDS_TEST_EXT);
		}
#endif
		break;
	case QR_DIRECT3D9:
#ifdef D3D9QUAKE
		{
			RECT rect;
			rect.left = r->x;
			rect.right = r->x + r->width;
			rect.top = r->y;
			rect.bottom = r->y + r->height;
			IDirect3DDevice9_SetScissorRect(pD3DDev9, &rect);
		}
#endif
		break;
	}
}
static void Sh_ScissorOff (void)
{
	switch(qrenderer)
	{
	default:
		break;
	case QR_OPENGL:
#ifdef GLQUAKE
		qglDisable(GL_SCISSOR_TEST);
		if (qglDepthBoundsEXT)
			qglDisable(GL_DEPTH_BOUNDS_TEST_EXT);
#endif
		break;
	case QR_DIRECT3D9:
#ifdef D3D9QUAKE
#endif
		break;
	}
}
*/
#if 0
static qboolean Sh_ScissorForSphere(vec3_t center, float radius, vrect_t *rect)
{
	/*return false to say that its fully offscreen*/

	float v[4], tempv[4];
	int i;
	vrect_t r;

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
	static const int edge[12][2] =
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

	r->x = 0;
	r->y = 0;
	r->width = 1;
	r->height = 1;
	r->dmin = 0;
	r->dmax = 1;
	if (!r_shadow_scissor.ival)
	{
		r->x = 0;
		r->y = 0;
		r->width  = 1;
		r->height = 1;
		return false;
	}
	/*if view is inside the box, then skip this maths*/
//	if (BoxesOverlap(r_refdef.vieworg, r_refdef.vieworg, mins, maxs))
//	{
//		return false;
//	}

	ncpdist = DotProduct(r_refdef.vieworg, vpn) + r_refdef.mindist;

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
	x1 = (1+x1) / 2;
	x2 = (1+x2) / 2;
	y1 = (1+y1) / 2;
	y2 = (1+y2) / 2;
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
	if (x1 > 1)
		x1 = 1;
	if (y1 > 1)
		y1 = 1;
	if (x2 > 1)
		x2 = 1;
	if (y2 > 1)
		y2 = 1;
	r->x = x1;
	r->y = y1;
	r->width  = x2 - r->x;
	r->height = y2 - r->y;
	if (r->width == 0 || r->height == 0)
		return true;	//meh

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
#ifdef DBG_COLOURNOTDEPTH
void GL_BeginRenderBuffer_DepthOnly(texid_t depthtexture)
{
	if (gl_config.ext_framebuffer_objects)
	{
		if (!shadow_fbo_id)
		{
			int drb;
			qglGenFramebuffersEXT(1, &shadow_fbo_id);
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo_id);

			//create an unnamed depth buffer
//			qglGenRenderbuffersEXT(1, &drb);
//			qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, drb);
//			qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24_ARB, SHADOWMAP_SIZE*3, SHADOWMAP_SIZE*2);
//			qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, drb);

			if (qglDrawBuffer)
				qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			if (qglReadBuffer)
				qglReadBuffer(GL_NONE);
		}
		else
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo_id);

		if (TEXVALID(depthtexture))
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, depthtexture.num, 0);
	}
}
#else
void GL_BeginRenderBuffer_DepthOnly(texid_t depthtexture)
{
	if (gl_config.ext_framebuffer_objects)
	{
		if (!shadow_fbo_id)
		{
			qglGenFramebuffersEXT(1, &shadow_fbo_id);
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo_id);
			if (qglDrawBuffers)
				qglDrawBuffers(0, NULL);
			else if (qglDrawBuffer)
				qglDrawBuffer(GL_NONE);
			if (qglReadBuffer)
				qglReadBuffer(GL_NONE);
		}
		else
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo_id);

		if (shadow_fbo_depth_num != depthtexture->num)
		{
			shadow_fbo_depth_num = depthtexture->num;
			if (TEXVALID(depthtexture))
				qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depthtexture->num, 0);
		}
	}
}
#endif
void GL_EndRenderBuffer_DepthOnly(int restorefbo, texid_t depthtexture, int texsize)
{
	if (gl_config.ext_framebuffer_objects)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, restorefbo);
	}
	else
	{
		GL_MTBind(0, GL_TEXTURE_2D, depthtexture);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);
	}
}
#endif

void D3D11BE_BeginShadowmapFace(void);

//determine the 5 bounding points of a shadowmap light projection side
//needs to match Sh_GenShadowFace
static void Sh_LightFrustumPlanes(dlight_t *l, vec3_t axis[3], vec4_t *planes, int face)
{
	vec3_t tmp;
	int axis0, axis1, axis2;
	int dir;
	int i;
	//+x,+y,+z,-x,-y,-z
	axis0 = (face+0)%3;	//our major axis
	axis1 = (face+1)%3;
	axis2 = (face+2)%3;
	dir = (face >= 3)?-1:1;

	//center point is always the same
	VectorCopy(l->origin, planes[4]);
	VectorScale(axis[axis0], dir, planes[4]);
	VectorNormalize(planes[4]);
	planes[4][3] = r_shadow_shadowmapping_nearclip.value + DotProduct(planes[4], l->origin);

	for (i = 0; i < 4; i++)
	{
		VectorScale(axis[axis0], dir, tmp);
		VectorMA(tmp,		((i&1)?1:-1), axis[axis1], tmp);
		VectorMA(tmp,		((i&2)?1:-1), axis[axis2], planes[i]);
		VectorNormalize(planes[i]);
		planes[i][3] = DotProduct(planes[i], l->origin);
	}
}

//culling for the face happens in the caller.
//these faces should thus match Sh_LightFrustumPlanes
static void Sh_GenShadowFace(dlight_t *l, vec3_t axis[3], shadowmesh_t *smesh, int face, int smsize, float proj[16])
{
	vec3_t t1,t2,t3;
	texture_t *tex;
	int tno;

/*	if (face >= 3)
		face -= 3;
	else
		face += 3;
*/
	switch(face)
	{
	case 0:
		//down
		VectorCopy(axis[0], t1);
		VectorCopy(axis[1], t2);
		VectorCopy(axis[2], t3);
		Matrix4x4_CM_LightMatrixFromAxis(r_refdef.m_view, t1, t2, t3, l->origin);
		r_refdef.flipcull = 0;
		break;
	case 1:
		//back
		VectorCopy(axis[2], t1);
		VectorCopy(axis[1], t2);
		VectorCopy(axis[0], t3);
		Matrix4x4_CM_LightMatrixFromAxis(r_refdef.m_view, t1, t2, t3, l->origin);
		r_refdef.flipcull = SHADER_CULL_FLIP;
		break;
	case 2:
		//right
		VectorCopy(axis[0], t1);
		VectorCopy(axis[2], t2);
		VectorCopy(axis[1], t3);
		Matrix4x4_CM_LightMatrixFromAxis(r_refdef.m_view, t1, t2, t3, l->origin);
		r_refdef.flipcull = SHADER_CULL_FLIP;
		break;
	case 3:
		//up
		VectorCopy(axis[0], t1);
		VectorCopy(axis[1], t2);
		VectorCopy(axis[2], t3);
		VectorNegate(t3, t3);
		Matrix4x4_CM_LightMatrixFromAxis(r_refdef.m_view, t1, t2, t3, l->origin);
		r_refdef.flipcull = SHADER_CULL_FLIP;
		break;
	case 4:
		//forward
		VectorCopy(axis[2], t1);
		VectorCopy(axis[1], t2);
		VectorCopy(axis[0], t3);
		VectorNegate(t3, t3);
		Matrix4x4_CM_LightMatrixFromAxis(r_refdef.m_view, t1, t2, t3, l->origin);
		r_refdef.flipcull = 0;
		break;
	case 5:
		//left
		VectorCopy(axis[0], t1);
		VectorCopy(axis[2], t2);
		VectorCopy(axis[1], t3);
		VectorNegate(t3, t3);
		Matrix4x4_CM_LightMatrixFromAxis(r_refdef.m_view, t1, t2, t3, l->origin);
		r_refdef.flipcull = 0;
		break;
	}

	if (l->fov)
	{
		r_refdef.pxrect.x = (SHADOWMAP_SIZE-smsize)/2;
		r_refdef.pxrect.width = smsize;
		r_refdef.pxrect.height = smsize;
		r_refdef.pxrect.y = (SHADOWMAP_SIZE-smsize)/2;
		r_refdef.pxrect.maxheight = SHADOWMAP_SIZE;
	}
	else
	{
		r_refdef.pxrect.x = (face%3 * SHADOWMAP_SIZE) + (SHADOWMAP_SIZE-smsize)/2;
		r_refdef.pxrect.width = smsize;
		r_refdef.pxrect.height = smsize;
		r_refdef.pxrect.y = (((face<3)*SHADOWMAP_SIZE) + (SHADOWMAP_SIZE-smsize)/2);
		r_refdef.pxrect.maxheight = SHADOWMAP_SIZE*2;
	}

	R_SetFrustum(proj, r_refdef.m_view);

#ifdef DBG_COLOURNOTDEPTH
	BE_SelectMode(BEM_STANDARD);
#else
	BE_SelectMode(BEM_DEPTHONLY);
#endif

	BE_SelectEntity(&r_worldentity);


	switch(qrenderer)
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		GL_ViewportUpdate();
		GL_CullFace(SHADER_CULL_FRONT);
		GLBE_RenderShadowBuffer(smesh->numverts, smesh->vebo[0], smesh->verts, smesh->numindicies, smesh->vebo[1], smesh->indicies);
		break;
#endif
#ifdef VKQUAKE
	case QR_VULKAN:
		//FIXME: generate a single commandbuffer (requires full separation of viewprojection matrix)
		VKBE_BeginShadowmapFace();
		VKBE_RenderShadowBuffer(smesh->vkbuffer);
		break;
#endif
#ifdef D3D11QUAKE
	case QR_DIRECT3D11:
		//opengl render targets are upside down - our code kinda assumes gl
		r_refdef.pxrect.y = r_refdef.pxrect.maxheight -(r_refdef.pxrect.y+r_refdef.pxrect.height);
		D3D11BE_BeginShadowmapFace();
		D3D11BE_RenderShadowBuffer(smesh->numverts, smesh->d3d11_vbuffer, smesh->numindicies, smesh->d3d11_ibuffer);
		break;
#endif
	default:
		//FIXME: should be able to merge batches between textures+lightmaps.
		for (tno = 0; tno < smesh->numbatches; tno++)
		{
			if (!smesh->batches[tno].count)
				continue;
			tex = cl.worldmodel->shadowbatches[tno].tex;
			if (tex->shader->flags & (SHADER_NOSHADOWS|SHADER_NODRAW))	//FIXME: shadows not lights
				continue;
			BE_DrawMesh_List(tex->shader, smesh->batches[tno].count, smesh->batches[tno].s, cl.worldmodel->shadowbatches[tno].vbo, NULL, 0);
		}
		break;
	}

	//fixme: this walks through the entity lists up to 6 times per frame per entity.
	switch(qrenderer)
	{
	default:
		break;
#ifdef GLQUAKE
	case QR_OPENGL:
		GLBE_BaseEntTextures();
		break;
#endif
#ifdef D3D9QUAKE
	case QR_DIRECT3D9:
		D3D9BE_BaseEntTextures();
		break;
#endif
#ifdef D3D11QUAKE
	case QR_DIRECT3D11:
		D3D11BE_BaseEntTextures();
		break;
#endif
#ifdef VKQUAKE
	case QR_VULKAN:
		VKBE_BaseEntTextures();
		break;
#endif
	}

/*
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
*/
}

qboolean D3D11_BeginShadowMap(int id, int w, int h);
void D3D11_EndShadowMap(void);

void D3D11BE_SetupForShadowMap(dlight_t *dl, qboolean isspot, int texwidth, int texheight, float shadowscale);

qboolean Sh_GenShadowMap (dlight_t *l, vec3_t axis[3], qbyte *lvis, int smsize)
{
#ifdef GLQUAKE
	int restorefbo = 0;
#endif
	int f;
	float oproj[16], oview[16];
	pxrect_t oprect;
	shadowmesh_t *smesh;
	int isspot = (l->fov != 0);
	int sidevisible;
	int oldflip = r_refdef.flipcull;
	int oldexternalview = r_refdef.externalview;

	if (!R_CullSphere(l->origin, 0))
		sidevisible = l->fov?1:0x3f;	//assume all are visible if the central point is onscreen
	else
	{
		sidevisible = 0;
		//FIXME: if the fov is < 90, we need to clip by the near lightplane first
		for (f = 0; f < (l->fov?1:6); f++)
		{
			vec4_t planes[5];
			float dist;
			int fp,lp;
			Sh_LightFrustumPlanes(l, axis, planes, f);
			for (fp = 0; fp < r_refdef.frustum_numplanes; fp++)
			{
				vec3_t nearest;
				//make a guess based upon the frustum plane
				VectorMA(l->origin, l->radius, r_refdef.frustum[fp].normal, nearest);
				//clip that point to the various planes

				for(lp = 0; lp < 5; lp++)
				{
					dist = DotProduct(nearest, planes[lp]) - planes[lp][3];
					if (dist < 0)
						VectorMA(nearest, dist, planes[lp], nearest);
				}

//				P_RunParticleEffect(nearest, vec3_origin, 15, 1);
				//give up if the best point for any frustum plane is offscreen
				dist = DotProduct(r_refdef.frustum[fp].normal, nearest) - r_refdef.frustum[fp].dist;
				if (dist <= 0)
					break;		
			}
			if (fp == r_refdef.frustum_numplanes)
				sidevisible |= 1u<<f;
		}
	}

	//if nothing is visible, then there's no point generating any shadowmaps at all...
	if (!sidevisible)
		return false;

	memcpy(oproj, r_refdef.m_projection, sizeof(oproj));
	memcpy(oview, r_refdef.m_view, sizeof(oview));
	oprect = r_refdef.pxrect;
	smesh = SHM_BuildShadowMesh(l, lvis, NULL, SMT_SHADOWMAP);

	Matrix4x4_CM_Projection_Far(r_refdef.m_projection, l->fov?l->fov:90, l->fov?l->fov:90, r_shadow_shadowmapping_nearclip.value, l->radius);

	switch(qrenderer)
	{
	default:
		return false;
#ifdef GLQUAKE
	case QR_OPENGL:
		if (!TEXVALID(shadowmap[isspot]))
		{
			if (isspot)
			{
				shadowmap[isspot] = Image_CreateTexture("***shadowmap2dspot***", NULL, 0);
				qglGenTextures(1, &shadowmap[isspot]->num);
				GL_MTBind(0, GL_TEXTURE_2D, shadowmap[isspot]);
	#ifdef DBG_COLOURNOTDEPTH
				qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SHADOWMAP_SIZE, SHADOWMAP_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	#else
				qglTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16_ARB, SHADOWMAP_SIZE, SHADOWMAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	#endif
			}
			else
			{
				shadowmap[isspot] = Image_CreateTexture("***shadowmap2dcube***", NULL, 0);
				qglGenTextures(1, &shadowmap[isspot]->num);
				GL_MTBind(0, GL_TEXTURE_2D, shadowmap[isspot]);
	#ifdef DBG_COLOURNOTDEPTH
				qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SHADOWMAP_SIZE*3, SHADOWMAP_SIZE*2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	#else
				if (gl_config.gles)
					qglTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOWMAP_SIZE*3, SHADOWMAP_SIZE*2, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
				else
					qglTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16_ARB, SHADOWMAP_SIZE*3, SHADOWMAP_SIZE*2, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
	#endif
			}
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	#if 0//def DBG_COLOURNOTDEPTH
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	#else
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	#endif
			//in case we're using shadow samplers
			if (gl_config.arb_shadow)
			{
				qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
				qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
				qglTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE_ARB, GL_LUMINANCE);
			}
		}

		/*set framebuffer*/
		GL_BeginRenderBuffer_DepthOnly(shadowmap[isspot]);
		restorefbo = GLBE_SetupForShadowMap(shadowmap[isspot], isspot?smsize:smsize*3, isspot?smsize:smsize*2, (smsize-4) / (float)SHADOWMAP_SIZE);

		BE_Scissor(NULL);
		qglViewport(0, 0, smsize*3, smsize*2);
		qglClear (GL_DEPTH_BUFFER_BIT);
#ifdef DBG_COLOURNOTDEPTH
		qglClearColor(0,1,0,1);
		qglClear (GL_COLOR_BUFFER_BIT);
#endif

		if (!gl_config.nofixedfunc)
		{
			qglMatrixMode(GL_PROJECTION);
			qglLoadMatrixf(r_refdef.m_projection);
			qglMatrixMode(GL_MODELVIEW);
		}
		break;
#endif
#ifdef D3D11QUAKE
	case QR_DIRECT3D11:
		if (!D3D11_BeginShadowMap(isspot, (isspot?SHADOWMAP_SIZE:(SHADOWMAP_SIZE*3)), (isspot?SHADOWMAP_SIZE:(SHADOWMAP_SIZE*2))))
			return false;

//		BE_Scissor(&rect);
		break;
#endif

#ifdef VKQUAKE
	case QR_VULKAN:
		if (!VKBE_BeginShadowmap(isspot, (isspot?SHADOWMAP_SIZE:(SHADOWMAP_SIZE*3)), (isspot?SHADOWMAP_SIZE:(SHADOWMAP_SIZE*2))))
			return false;
		break;
#endif
	}

	r_refdef.externalview = true;	//never any viewmodels

	/*generate faces*/
	for (f = 0; f < 6; f++)
	{
		if (sidevisible & (1u<<f))
		{
			RQuantAdd(RQUANT_SHADOWSIDES, 1);
			Sh_GenShadowFace(l, axis, smesh, f, smsize, r_refdef.m_projection);
		}
	}

	memcpy(r_refdef.m_view, oview, sizeof(r_refdef.m_view));
	memcpy(r_refdef.m_projection, oproj, sizeof(r_refdef.m_projection));

	r_refdef.pxrect = oprect;

	r_refdef.flipcull = oldflip;
	r_refdef.externalview = oldexternalview;
	R_SetFrustum(r_refdef.m_projection, r_refdef.m_view);

	switch(qrenderer)
	{
	default:
		break;
#ifdef GLQUAKE
	case QR_OPENGL:
		/*end framebuffer*/
		GL_EndRenderBuffer_DepthOnly(restorefbo, shadowmap[isspot], smsize);
		if (!gl_config.nofixedfunc)
		{
			qglMatrixMode(GL_PROJECTION);
			qglLoadMatrixf(r_refdef.m_projection);
			qglMatrixMode(GL_MODELVIEW);
			qglLoadMatrixf(r_refdef.m_view);
		}
		GL_ViewportUpdate();
		break;
#endif
#ifdef D3D11QUAKE
	case QR_DIRECT3D11:
		D3D11_EndShadowMap();
		D3D11BE_DoneShadows();
		break;
#endif
#ifdef VKQUAKE
	case QR_VULKAN:
		VKBE_DoneShadows();
		break;
#endif
	}

	return true;
}

static void Sh_DrawShadowMapLight(dlight_t *l, vec3_t colour, vec3_t axis[3], qbyte *vvis)
{
	vec3_t mins, maxs;
	qbyte *lvis;
	srect_t rect;
	int smsize;
	qboolean isspot;

	if (R_CullSphere(l->origin, l->radius))
	{
		RQuantAdd(RQUANT_RTLIGHT_CULL_FRUSTUM, 1);
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
		RQuantAdd(RQUANT_RTLIGHT_CULL_SCISSOR, 1);
		return;
	}

	if (vvis)
	{
		if (!l->rebuildcache && l->worldshadowmesh)
		{
			lvis = l->worldshadowmesh->litleaves;
			//fixme: check head node first?
			if (!Sh_LeafInView(l->worldshadowmesh->litleaves, vvis))
			{
				RQuantAdd(RQUANT_RTLIGHT_CULL_PVS, 1);
				return;
			}
		}
		else
		{
			int clus;
			clus = cl.worldmodel->funcs.ClusterForPoint(cl.worldmodel, l->origin);
			lvis = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, clus, lvisb, sizeof(lvisb));
			//FIXME: surely we can use the phs for this?

			if (!Sh_VisOverlaps(lvis, vvis))	//The two viewing areas do not intersect.
			{
				RQuantAdd(RQUANT_RTLIGHT_CULL_PVS, 1);
				return;
			}
		}
	}
	else
		lvis = NULL;


	isspot = l->fov != 0;
	if (isspot)
		smsize = SHADOWMAP_SIZE;
	else
	{
		//Stolen from DP. Actually, LH pasted it to me in IRC.
		vec3_t nearestpoint;
		vec3_t d;
		float distance, lodlinear;
		nearestpoint[0] = bound(l->origin[0]-l->radius, r_origin[0], l->origin[0]+l->radius);
		nearestpoint[1] = bound(l->origin[1]-l->radius, r_origin[1], l->origin[1]+l->radius);
		nearestpoint[2] = bound(l->origin[2]-l->radius, r_origin[2], l->origin[2]+l->radius);
		VectorSubtract(nearestpoint, r_origin, d);
		distance = VectorLength(d);
		lodlinear = (l->radius * r_shadow_shadowmapping_precision.value) / sqrt(max(1.0f, distance / l->radius));
		smsize = bound(16, lodlinear, SHADOWMAP_SIZE);
	}

#ifdef D3D11QUAKE
	if (qrenderer == QR_DIRECT3D11)
		D3D11BE_SetupForShadowMap(l, isspot, isspot?smsize:smsize*3, isspot?smsize:smsize*2, (smsize-4) / (float)SHADOWMAP_SIZE);
#endif
#ifdef VKQUAKE
	if (qrenderer == QR_VULKAN)
		VKBE_SetupForShadowMap(l, isspot, isspot?smsize:smsize*3, isspot?smsize:smsize*2, (smsize-4) / (float)SHADOWMAP_SIZE);
#endif

	if (!BE_SelectDLight(l, colour, axis, isspot?LSHADER_SPOT:LSHADER_SMAP))
		return;
	if (!Sh_GenShadowMap(l, axis, lvis, smsize))
		return;

	RQuantAdd(RQUANT_RTLIGHT_DRAWN, 1);

	//may as well use scissors
	BE_Scissor(&rect);

	BE_SelectEntity(&r_worldentity);

	BE_SelectMode(BEM_LIGHT);
	Sh_DrawEntLighting(l, colour);

}


/*
draws faces facing the light
Note: Backend mode must have been selected in advance, as must the light to light from
*/
static void Sh_DrawEntLighting(dlight_t *light, vec3_t colour)
{
	int tno;
	texture_t *tex;
	shader_t *shader;
	shadowmesh_t *sm;

	sm = light->worldshadowmesh;
	if (light->rebuildcache)
		sm = &sh_tempshmesh;
	if (sm)
	{
		for (tno = 0; tno < sm->numbatches; tno++)
		{
			if (!sm->batches[tno].count)
				continue;
			tex = cl.worldmodel->shadowbatches[tno].tex;
			if (cl.worldmodel->fromgame == fg_quake2)
				shader = R_TextureAnimation_Q2(tex)->shader;
			else
				shader = R_TextureAnimation(false, tex)->shader;
			if (shader->flags & (SHADER_NODLIGHT|SHADER_NODRAW|SHADER_SKY))
				continue;
			//FIXME: it should be worth building a dedicated ebo, for static ones
			BE_DrawMesh_List(shader, sm->batches[tno].count, sm->batches[tno].s, cl.worldmodel->shadowbatches[tno].vbo, NULL, 0);
			RQuantAdd(RQUANT_LITFACES, sm->batches[tno].count);
		}

		switch(qrenderer)
		{
		default:
			break;
#ifdef GLQUAKE
		case QR_OPENGL:
			GLBE_BaseEntTextures();
			break;
#endif
#ifdef VKQUAKE
		case QR_VULKAN:
			VKBE_BaseEntTextures();
			break;
#endif
#ifdef D3D9QUAKE
		case QR_DIRECT3D9:
			D3D9BE_BaseEntTextures();
			break;
#endif
#ifdef D3D11QUAKE
		case QR_DIRECT3D11:
			D3D11BE_BaseEntTextures();
			break;
#endif
		}
	}
}

#ifdef GLQUAKE
/*Fixme: this is brute forced*/
#ifdef warningmsg
#pragma warningmsg("brush shadows are bruteforced")
#endif
static void Sh_DrawBrushModelShadow(dlight_t *dl, entity_t *e)
{
	int v;
	float *v1, *v2;
	vec3_t v3, v4;
	vec3_t lightorg;

	int i;
	model_t *model;
	msurface_t *surf;

	if (qrenderer != QR_OPENGL)
		return;

	if (BE_LightCullModel(e->origin, e->model))
		return;

	RotateLightVector((void *)e->axis, e->origin, dl->origin, lightorg);

	BE_SelectEntity(e);

	GL_DeselectVAO();
	GL_SelectVBO(0);
	GL_SelectEBO(0);
	qglEnableClientState(GL_VERTEX_ARRAY);

	GLBE_PolyOffsetStencilShadow(true);

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
		RQuantAdd(RQUANT_SHADOWINDICIES, surf->mesh->numvertexes);

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

	GLBE_PolyOffsetStencilShadow(false);
}
#endif

#if defined(GLQUAKE) || defined(D3D9QUAKE)
/*when this is called, the gl state has been set up to draw the stencil volumes using whatever extensions we have
if secondside is set, then the gpu sucks and we're drawing stuff the slow 2-pass way, and this is the second pass.
*/
static void Sh_DrawStencilLightShadows(dlight_t *dl, qbyte *lvis, qbyte *vvis, qboolean secondside)
{
	struct shadowmesh_s *sm;
#ifdef GLQUAKE
	extern cvar_t gl_part_flame;
	int		i;
	entity_t *ent;
	model_t *emodel;
#endif

	sm = SHM_BuildShadowMesh(dl, lvis, vvis, SMT_STENCILVOLUME);
	if (!sm)
	{
#ifdef GLQUAKE
		Sh_DrawBrushModelShadow(dl, &r_worldentity);
#endif
	}
	else
	{
		switch (qrenderer)
		{
		case QR_NONE:
		case QR_SOFTWARE:
		default:
			break;

#ifdef D3D11QUAKE
//		case QR_DIRECT3D11:
//			D3D11BE_RenderShadowBuffer(sm->numverts, sm->d3d11_vbuffer, sm->numindicies, sm->d3d11_ibuffer);
//			break;
#endif
#ifdef D3D9QUAKE
		case QR_DIRECT3D9:
			D3D9BE_RenderShadowBuffer(sm->numverts, sm->d3d9_vbuffer, sm->numindicies, sm->d3d9_ibuffer);
			break;
#endif
#ifdef GLQUAKE
		case QR_OPENGL:
			GLBE_RenderShadowBuffer(sm->numverts, sm->vebo[0], sm->verts, sm->numindicies, sm->vebo[1], sm->indicies);
			break;
#endif
#ifdef VKQUAKE
//		case QR_VULKAN:
//			VKBE_RenderShadowBuffer(sm->numverts, sm->vebo[0], sm->verts, sm->numindicies, sm->vebo[1], sm->indicies);
//			break;
#endif
		}
	}
	if (!r_drawentities.value)
		return;

#ifdef GLQUAKE
	if (qrenderer != QR_OPENGL)
		return;	//FIXME: still uses glBegin specifics.
	if (gl_config_nofixedfunc)
		return;	/*too lazy to use shaders*/
	if (gl_config_gles)
		return;	//FIXME: uses glBegin

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		ent = &cl_visedicts[i];

		if (ent->rtype != RT_MODEL)
			continue;

		if (ent->flags & (RF_NOSHADOW|Q2RF_BEAM))
			continue;

		if (ent->keynum == dl->key && ent->keynum)
			continue;

		emodel = ent->model;
		if (!emodel)
			continue;

		if (cls.allow_anyparticles)	//allowed or static
		{
			if (emodel->engineflags & MDLF_EMITREPLACE)
			{
				if (gl_part_flame.value)
					continue;
			}
		}

		if (emodel->loadstate == MLS_NOTLOADED)
		{
			if (!Mod_LoadModel(emodel, MLV_WARN))
				continue;
		}
		if (emodel->loadstate != MLS_LOADED)
			continue;

		switch (emodel->type)
		{
		case mod_alias:
			R_DrawGAliasShadowVolume (ent, dl->origin, dl->radius);
			break;

		case mod_brush:
			Sh_DrawBrushModelShadow (dl, ent);
			break;

		case mod_sprite:	//never any shadows on sprites, it doesn't really make sense.
			break;

		default:
			break;
		}
	}
	BE_SelectEntity(&r_worldentity);
#endif
}

//draws a light using stencil shadows.
//redraws world geometry up to 3 times per light...
static qboolean Sh_DrawStencilLight(dlight_t *dl, vec3_t colour, vec3_t axis[3], qbyte *vvis)
{
	int sref;
	int clus;
	qbyte *lvis;
	srect_t rect;

	vec3_t mins;
	vec3_t maxs;

	if (R_CullSphere(dl->origin, dl->radius))
	{
		RQuantAdd(RQUANT_RTLIGHT_CULL_FRUSTUM, 1);
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
			RQuantAdd(RQUANT_RTLIGHT_CULL_PVS, 1);
			return false;
		}
		lvis = NULL;
	}
	else
	{
		clus = cl.worldmodel->funcs.ClusterForPoint(cl.worldmodel, dl->origin);
		lvis = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, clus, lvisb, sizeof(lvisb));

		if (!Sh_VisOverlaps(lvis, vvis))	//The two viewing areas do not intersect.
		{
			RQuantAdd(RQUANT_RTLIGHT_CULL_PVS, 1);
			return false;
		}
	}

	//sets up the gl scissor (and culls to view)
	if (Sh_ScissorForBox(mins, maxs, &rect))
	{
		RQuantAdd(RQUANT_RTLIGHT_CULL_SCISSOR, 1);
		return false;	//this doesn't cull often.
	}
	RQuantAdd(RQUANT_RTLIGHT_DRAWN, 1);

	BE_SelectDLight(dl, colour, axis, LSHADER_STANDARD);
	BE_SelectMode(BEM_STENCIL);

	//The backend doesn't maintain scissor state.
	//The backend doesn't maintain stencil test state either - it needs to be active for more than just stencils, or disabled. its awkward.
	BE_Scissor(&rect);


	switch(qrenderer)
	{
	default:
		(void)sref;
		break;
#ifdef GLQUAKE
	case QR_OPENGL:
		{
			int sfrontfail;
			int sbackfail;
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
				sfrontfail = GL_DECR_WRAP_EXT;
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
			if (gl_config.arb_depth_clamp && r_refdef.maxdist != 0)
				qglEnable(GL_DEPTH_CLAMP_ARB);

		#if 0 //def _DEBUG
		//	if (r_shadows.value == 666)	//testing (visible shadow volumes)
			{
				qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				qglColor4f(dl->color[0], dl->color[1], dl->color[2], 1);
				qglDisable(GL_STENCIL_TEST);
//				qglEnable(GL_POLYGON_OFFSET_FILL);
//				qglPolygonOffset(-1, -1);
			//	qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				Sh_DrawStencilLightShadows(dl, lvis, vvis, false);
//				qglDisable(GL_POLYGON_OFFSET_FILL);
//				qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		#endif

			if (qglStencilOpSeparateATI)
			{
				//ATI/GLES/ARB method
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
				//Nvidia-specific method.
				sref/=2;
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

			BE_SelectMode(BEM_LIGHT);
			Sh_DrawEntLighting(dl, colour);
			qglDisable(GL_STENCIL_TEST);
			qglStencilFunc( GL_ALWAYS, 0, ~0 );
		}
		break;
#endif
#ifdef D3D9QUAKE
	case QR_DIRECT3D9:
		sref = (1<<8)-1;
		sref/=2;

		/*clear the stencil buffer*/
		IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_STENCIL, D3DCOLOR_XRGB(0, 0, 0), 1.0f, sref);

		/*set up 2-sided stenciling*/
		D3D9BE_Cull(0);
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

		//disable stencil writing
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_TWOSIDEDSTENCILMODE, false);
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
#else
#define Sh_DrawStencilLight Sh_DrawShadowlessLight
#endif

static void Sh_DrawShadowlessLight(dlight_t *dl, vec3_t colour, vec3_t axis[3], qbyte *vvis)
{
	vec3_t mins, maxs;
	srect_t rect;

	if (R_CullSphere(dl->origin, dl->radius))
	{
		RQuantAdd(RQUANT_RTLIGHT_CULL_FRUSTUM, 1);
		return;	//this should be the more common case
	}

	if (!dl->rebuildcache)
	{
		//fixme: check head node first?
		if (!Sh_LeafInView(dl->worldshadowmesh->litleaves, vvis))
		{
			RQuantAdd(RQUANT_RTLIGHT_CULL_PVS, 1);
			return;
		}
	}
	else
	{
		int clus;
		qbyte *lvis;

		clus = cl.worldmodel->funcs.ClusterForPoint(cl.worldmodel, dl->origin);
		lvis = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, clus, lvisb, sizeof(lvisb));

		SHM_BuildShadowMesh(dl, lvis, vvis, SMT_SHADOWLESS);

		if (!Sh_VisOverlaps(lvis, vvis))	//The two viewing areas do not intersect.
		{
			RQuantAdd(RQUANT_RTLIGHT_CULL_PVS, 1);
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
		RQuantAdd(RQUANT_RTLIGHT_CULL_SCISSOR, 1);
		return;	//was culled.
	}

	//should we actually scissor here? there's not really much point I suppose.
	BE_Scissor(NULL);

	RQuantAdd(RQUANT_RTLIGHT_DRAWN, 1);

	BE_SelectDLight(dl, colour, axis, dl->fov?LSHADER_SPOT:LSHADER_STANDARD);
	BE_SelectMode(BEM_LIGHT);
	Sh_DrawEntLighting(dl, colour);
}

void Sh_DrawCrepuscularLight(dlight_t *dl, float *colours)
{
#ifdef GLQUAKE
	int oldfbo;
	static mesh_t mesh;
	image_t *oldsrccol;
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

	//fixme: we should add an extra few pixels each side to the fbo, to avoid too much weirdness at screen edges.

	if (!crepuscular_texture_id)
	{
		/*FIXME: requires npot*/
		crepuscular_shader = R_RegisterShader("crepuscular_screen", SUF_NONE,
			"{\n"
				"program crepuscular_rays\n"
				"{\n"
					"map $sourcecolour\n"
					"blend add\n"
				"}\n"
			"}\n"
			);

		crepuscular_texture_id = Image_CreateTexture("***crepusculartexture***", NULL, IF_LINEAR|IF_NOMIPMAP|IF_CLAMP|IF_NOGAMMA);
		Image_Upload(crepuscular_texture_id, TF_RGBA32, NULL, NULL, vid.pixelwidth, vid.pixelheight, IF_LINEAR|IF_NOMIPMAP|IF_CLAMP|IF_NOGAMMA);
	}

	BE_Scissor(NULL);

	oldfbo = GLBE_FBO_Update(&crepuscular_fbo, FBO_RB_DEPTH, &crepuscular_texture_id, 1, r_nulltex, vid.pixelwidth, vid.pixelheight, 0);

	GL_ForceDepthWritable();
//	qglClearColor(0, 0, 0, 1);
	qglClear(GL_DEPTH_BUFFER_BIT);

	BE_SelectMode(BEM_CREPUSCULAR);
	BE_SelectDLight(dl, colours, dl->axis, LSHADER_STANDARD);
	GLBE_SubmitMeshes(cl.worldmodel->batches, SHADER_SORT_PORTAL, SHADER_SORT_BLEND);

	GLBE_FBO_Pop(oldfbo);

	oldsrccol = NULL;//shaderstate.tex_sourcecol;
	GLBE_FBO_Sources(crepuscular_texture_id, NULL);
//	crepuscular_shader->defaulttextures.base = crepuscular_texture_id;
	//shaderstate.tex_sourcecol = oldsrccol;

	BE_SelectMode(BEM_STANDARD);

	BE_DrawMesh_Single(crepuscular_shader, &mesh, NULL, 0);

	GLBE_FBO_Sources(oldsrccol, NULL);
#endif
}

void Sh_PurgeShadowMeshes(void)
{
	dlight_t *dl;
	int i;
	for (dl = cl_dlights, i=0; i<cl_maxdlights; i++, dl++)
	{
		if (dl->worldshadowmesh)
		{
			SH_FreeShadowMesh(dl->worldshadowmesh);
			dl->worldshadowmesh = NULL;
			dl->rebuildcache = true;
		}
	}
	Z_Free(edge);
	edge = NULL;
	maxedge = 0;
}

void R_StaticEntityToRTLight(int i);
void Sh_PreGenerateLights(void)
{
	unsigned int ignoreflags;
	dlight_t *dl;
	int shadowtype;
	int leaf;
	qbyte *lvis;
	int i;

	r_shadow_realtime_world_lightmaps.value = atof(r_shadow_realtime_world_lightmaps.string);
	if ((r_shadow_realtime_dlight.ival || r_shadow_realtime_world.ival) && rtlights_max == RTL_FIRST)
	{
		qboolean okay = false;
		if (!okay)
			okay |= R_LoadRTLights();
		if (!okay)
			okay |= R_ImportRTLights(cl.worldmodel->entities);
		if (!okay && r_shadow_realtime_world.ival && r_shadow_realtime_world_lightmaps.value != 1)
		{
			r_shadow_realtime_world_lightmaps.value = 1;
			Con_Printf(CON_WARNING "No lights detected in map.\n");
		}

		for (i = 0; i < cl.num_statics; i++)
		{
			R_StaticEntityToRTLight(i);
		}
	}

	ignoreflags = (r_shadow_realtime_world.value?LFLAG_REALTIMEMODE:LFLAG_NORMALMODE);

	for (dl = cl_dlights+rtlights_first, i=rtlights_first; i<rtlights_max; i++, dl++)
	{
		dl->rebuildcache = true;

		if (dl->radius)
		{
			if (dl->flags & ignoreflags)
			{
				if (dl->flags & LFLAG_CREPUSCULAR)
					continue;

				if (((!dl->die)?!r_shadow_realtime_world_shadows.ival:!r_shadow_realtime_dlight_shadows.ival) || (dl->flags & LFLAG_NOSHADOWS))
					shadowtype = SMT_SHADOWLESS;
				else if (dl->flags & LFLAG_SHADOWMAP || r_shadow_shadowmapping.ival)
					shadowtype = SMT_SHADOWMAP;
				else
					shadowtype = SMT_STENCILVOLUME;

				leaf = cl.worldmodel->funcs.ClusterForPoint(cl.worldmodel, dl->origin);
				lvis = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, leaf, lvisb, sizeof(lvisb));

				SHM_BuildShadowMesh(dl, lvis, NULL, shadowtype);
				continue;
			}
		}

		if (dl->worldshadowmesh)
		{
			SH_FreeShadowMesh(dl->worldshadowmesh);
			dl->worldshadowmesh = NULL;
			dl->rebuildcache = true;
		}
	}
}

void Com_ParseVector(char *str, vec3_t out)
{
	str = COM_Parse(str);
	out[0] = atof(com_token);
	str = COM_Parse(str);
	out[1] = atof(com_token);
	str = COM_Parse(str);
	out[2] = atof(com_token);
}

void Sh_CheckSettings(void)
{
	qboolean canstencil = false, cansmap = false, canshadowless = false;
	r_shadow_shadowmapping.ival = r_shadow_shadowmapping.value;
	r_shadow_realtime_world.ival = r_shadow_realtime_world.value;
	r_shadow_realtime_dlight.ival = r_shadow_realtime_dlight.value;
	r_shadow_realtime_world_shadows.ival = r_shadow_realtime_world_shadows.value;
	r_shadow_realtime_dlight_shadows.ival = r_shadow_realtime_dlight_shadows.value;

	switch(qrenderer)
	{
#ifdef VKQUAKE
	case QR_VULKAN:
		canshadowless = true;
		cansmap = true;
		break;
#endif
#ifdef GLQUAKE
	case QR_OPENGL:
		canshadowless = gl_config.arb_shader_objects || !gl_config_nofixedfunc; //falls back to crappy texture env
		if (gl_config.arb_shader_objects && gl_config.ext_framebuffer_objects && gl_config.arb_depth_texture)// && gl_config.arb_shadow)
			cansmap = true;
		else if (r_shadow_realtime_world_shadows.ival || r_shadow_realtime_dlight_shadows.ival)
		{
			if (!gl_config.arb_shader_objects)
				Con_Printf("No arb_shader_objects\n");
			if (!gl_config.ext_framebuffer_objects)
				Con_Printf("No ext_framebuffer_objects\n");
			if (!gl_config.arb_depth_texture)
				Con_Printf("No arb_depth_texture\n");
		}
		if (gl_stencilbits)
			canstencil = true;
		break;
#endif
#ifdef D3D9QUAKE
	case QR_DIRECT3D9:
		canshadowless = true;
		//the code still has a lot of ifdefs, so will crash if you try it in a merged build.
		//its not really usable in d3d-only builds either, so no great loss.
//		canstencil = true;
		break;
#endif
#ifdef D3D11QUAKE
	case QR_DIRECT3D11:
		canshadowless = true;	//all feature levels
/* shadows are buggy right now. tbh they've always been buggy... rendering seems fine, its just the shadowmaps that are bad
		if (D3D11_BeginShadowMap(0, SHADOWMAP_SIZE*3, SHADOWMAP_SIZE*2))
		{
			D3D11_EndShadowMap();
			cansmap = true;		//tends to not work properly until feature level 10 for one error or another.
		}
*/
		break;
#endif
	default:
		break;
	}

	if (!canstencil && !cansmap && !canshadowless)
	{
		//can't even do lighting
		if (r_shadow_realtime_world.ival || r_shadow_realtime_dlight.ival)
			Con_Printf("Missing driver extensions: realtime lighting is not possible.\n");
		r_shadow_realtime_world.ival = 0;
		r_shadow_realtime_dlight.ival = 0;
	}
	else if (!canstencil && !cansmap)
	{
		//no shadow methods available at all.
		if ((r_shadow_realtime_world.ival&&r_shadow_realtime_world_shadows.ival)||(r_shadow_realtime_dlight.ival&&r_shadow_realtime_dlight_shadows.ival))
			Con_Printf("Missing driver extensions: realtime shadows are not possible.\n");
		r_shadow_realtime_world_shadows.ival = 0;
		r_shadow_realtime_dlight_shadows.ival = 0;
	}
	else if (!canstencil || !cansmap)
	{
		//only one shadow method
		if (!!r_shadow_shadowmapping.ival != cansmap)
		{
			if (r_shadow_shadowmapping.ival && ((r_shadow_realtime_world.ival&&r_shadow_realtime_world_shadows.ival)||(r_shadow_realtime_dlight.ival&&r_shadow_realtime_dlight_shadows.ival)))
				Con_Printf("Missing driver extensions: forcing shadowmapping %s.\n", cansmap?"on":"off");
			r_shadow_shadowmapping.ival = cansmap;
		}
	}
	else
	{
		//both shadow methods available.
	}
}

void Sh_CalcPointLight(vec3_t point, vec3_t light)
{
	vec3_t colour;
	dlight_t *dl;
	vec3_t disp;
	float dist;
	float frac;
	int i;
	unsigned int ignoreflags;

	vec3_t norm, impact;
	ignoreflags = (r_shadow_realtime_world.value?LFLAG_REALTIMEMODE:LFLAG_NORMALMODE);

	VectorClear(light);
	if (ignoreflags)
	for (dl = cl_dlights+rtlights_first, i=rtlights_first; i<rtlights_max; i++, dl++)
	{
		if (!(dl->flags & ignoreflags))
			continue;

		if (dl->key == cl.playerview[0].viewentity)	//ignore the light if its emitting from the player. generally the player can't *SEE* that light so it still counts.
			continue;								//disable this check if this function gets used for anything other than iris adaptation

		colour[0] = dl->color[0];
		colour[1] = dl->color[1];
		colour[2] = dl->color[2];
		if (dl->style)
		{
			colour[0] *= cl_lightstyle[dl->style-1].colours[0] * d_lightstylevalue[dl->style-1]/255.0f;
			colour[1] *= cl_lightstyle[dl->style-1].colours[1] * d_lightstylevalue[dl->style-1]/255.0f;
			colour[2] *= cl_lightstyle[dl->style-1].colours[2] * d_lightstylevalue[dl->style-1]/255.0f;
		}
		else
		{
			colour[0] *= r_lightstylescale.value;
			colour[1] *= r_lightstylescale.value;
			colour[2] *= r_lightstylescale.value;
		}

		if (colour[0] < 0.001 && colour[1] < 0.001 && colour[2] < 0.001)
			continue;	//just switch these off.

		VectorSubtract(dl->origin, point, disp);
		dist = VectorLength(disp);
		frac = dist / dl->radius;
		if (frac >= 1)
			continue;
		//FIXME: this should be affected by the direction.
		if (CL_TraceLine(point, dl->origin, impact, norm, NULL)>=1)
			VectorMA(light, 1-frac, colour, light);
	}
}

int drawdlightnum;
void Sh_DrawLights(qbyte *vis)
{
	vec3_t rotated[3];
	vec3_t *axis;
	vec3_t colour;
	dlight_t *dl;
	int i;
	unsigned int ignoreflags;

	if (r_shadow_realtime_world.modified ||
		r_shadow_realtime_world_shadows.modified ||
		r_shadow_realtime_dlight.modified ||
		r_shadow_realtime_dlight_shadows.modified ||
		r_shadow_shadowmapping.modified)
	{
		r_shadow_realtime_world.modified =
		r_shadow_realtime_world_shadows.modified =
		r_shadow_realtime_dlight.modified =
		r_shadow_realtime_dlight_shadows.modified =
		r_shadow_shadowmapping.modified =
				false;
		Sh_CheckSettings();
		//make sure the lighting is reloaded
		Sh_PreGenerateLights();
	}

	if (!r_shadow_realtime_world.ival && !r_shadow_realtime_dlight.ival)
	{
		return;
	}

	ignoreflags = (r_shadow_realtime_world.value?LFLAG_REALTIMEMODE:LFLAG_NORMALMODE);

//	if (r_refdef.recurse)
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
			colour[0] *= cl_lightstyle[dl->style-1].colours[0] * d_lightstylevalue[dl->style-1]/255.0f;
			colour[1] *= cl_lightstyle[dl->style-1].colours[1] * d_lightstylevalue[dl->style-1]/255.0f;
			colour[2] *= cl_lightstyle[dl->style-1].colours[2] * d_lightstylevalue[dl->style-1]/255.0f;
		}
		else
		{
			colour[0] *= r_lightstylescale.value;
			colour[1] *= r_lightstylescale.value;
			colour[2] *= r_lightstylescale.value;
		}
		colour[0] *= r_refdef.hdr_value;
		colour[1] *= r_refdef.hdr_value;
		colour[2] *= r_refdef.hdr_value;

		if (colour[0] < 0.001 && colour[1] < 0.001 && colour[2] < 0.001)
			continue;	//just switch these off.

		if (!dl->lightcolourscales[0] && !dl->lightcolourscales[1] && !dl->lightcolourscales[2])
			continue;	//these lights are just coronas.

		if (dl->rotation[0] || dl->rotation[1] || dl->rotation[2])
		{	//auto-rotating (static) rtlights
			vec3_t rot;
			vec3_t rotationaxis[3];
			VectorScale(dl->rotation, cl.time, rot);
			AngleVectorsFLU(rot, rotationaxis[0], rotationaxis[1], rotationaxis[2]);
			Matrix3_Multiply(dl->axis, rotationaxis, rotated);
			axis = rotated;
		}
		else
			axis = dl->axis;

		drawdlightnum++;
		if (dl->flags & LFLAG_CREPUSCULAR)
			Sh_DrawCrepuscularLight(dl, colour);
		else if (((i >= RTL_FIRST)?!r_shadow_realtime_world_shadows.ival:!r_shadow_realtime_dlight_shadows.ival) || dl->flags & LFLAG_NOSHADOWS)
		{
			Sh_DrawShadowlessLight(dl, colour, axis, vis);
		}
		else if ((dl->flags & LFLAG_SHADOWMAP) || r_shadow_shadowmapping.ival)
		{
			Sh_DrawShadowMapLight(dl, colour, axis, vis);
		}
		else
		{
			Sh_DrawStencilLight(dl, colour, axis, vis);
		}
	}

#ifdef GLQUAKE
	if (gl_config.arb_shader_objects)
	{
		dlight_t sun = {0};
		vec3_t sundir;
		float dot;
		Com_ParseVector(r_sun_dir.string, sundir);
		Com_ParseVector(r_sun_colour.string, colour);

		//fade it out if we're looking at an angle parallel to it (to avoid nasty visible graduations or backwards rays!)
		dot = DotProduct(vpn, sundir);
		dot = 1-dot;
		dot *= dot;
		dot = 1-dot;
		VectorScale(colour, dot, colour);

		if (colour[0] > 0.001 || colour[1] > 0.001 || colour[2] > 0.001)
		{
			//only do this if we can see some sky surfaces. pointless otherwise
			batch_t *b;
			for (b = cl.worldmodel->batches[SHADER_SORT_SKY]; b; b = b->next)
			{
				if (b->meshes)
					break;
			}
			if (b)
			{
				VectorNormalize(sundir);
				VectorMA(r_origin, 1000, sundir, sun.origin);
				Sh_DrawCrepuscularLight(&sun, colour);
			}
		}
	}
#endif

	BE_Scissor(NULL);

	BE_SelectMode(BEM_STANDARD);

//	if (developer.value)
//	Con_Printf("%i lights drawn, %i frustum culled, %i pvs culled, %i scissor culled\n", bench.numlights, bench.numfrustumculled, bench.numpvsculled, bench.numscissorculled);
//	memset(&bench, 0, sizeof(bench));

	drawdlightnum = -1;
}
#endif

//stencil shadows generally require that the farclip distance is really really far away
//so this little function is used to check if its needed or not.
qboolean Sh_StencilShadowsActive(void)
{
#if defined(RTLIGHTS) && !defined(SERVERONLY)
	//if shadowmapping is forced on all lights then we don't need special depth stuff
//	if (r_shadow_shadowmapping.ival)
//		return false;
	if (isDedicated)
		return false;
	return	(r_shadow_realtime_dlight.ival && r_shadow_realtime_dlight_shadows.ival) ||
			(r_shadow_realtime_world.ival && r_shadow_realtime_world_shadows.ival);
#else
	return false;
#endif
}

void Sh_RegisterCvars(void)
{
#if defined(RTLIGHTS) && !defined(SERVERONLY)
#define REALTIMELIGHTING "Realtime Lighting"
	Cvar_Register (&r_shadow_scissor,					REALTIMELIGHTING);
	Cvar_Register (&r_shadow_realtime_world,			REALTIMELIGHTING);
	Cvar_Register (&r_shadow_realtime_world_shadows,	REALTIMELIGHTING);
	Cvar_Register (&r_shadow_realtime_dlight,			REALTIMELIGHTING);
	Cvar_Register (&r_shadow_realtime_dlight_ambient,	REALTIMELIGHTING);
	Cvar_Register (&r_shadow_realtime_dlight_diffuse,	REALTIMELIGHTING);
	Cvar_Register (&r_shadow_realtime_dlight_specular,	REALTIMELIGHTING);
	Cvar_Register (&r_shadow_realtime_dlight_shadows,	REALTIMELIGHTING);
	Cvar_Register (&r_shadow_realtime_world_lightmaps,	REALTIMELIGHTING);
	Cvar_Register (&r_shadow_playershadows,				REALTIMELIGHTING);
	Cvar_Register (&r_shadow_shadowmapping,				REALTIMELIGHTING);
	Cvar_Register (&r_shadow_shadowmapping_precision,	REALTIMELIGHTING);
	Cvar_Register (&r_shadow_shadowmapping_nearclip,	REALTIMELIGHTING);
	Cvar_Register (&r_shadow_shadowmapping_bias,		REALTIMELIGHTING);
	Cvar_Register (&r_sun_dir,							REALTIMELIGHTING);
	Cvar_Register (&r_sun_colour,						REALTIMELIGHTING);
#endif
}