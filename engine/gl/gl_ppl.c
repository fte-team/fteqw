//FIXME: Light visibility is decided from weather the light's pvs overlaps the view pvs.
//This doesn't take light radius into account. This means that lights around corners that will never be visible are drawn in full per-pixel goodness.
//This is bad. lights*3, 33% framerate for no worthwhile effect.

#include "quakedef.h"

#ifndef NEWBACKEND

#ifdef RGLQUAKE
#include "glquake.h"
#include "shader.h"
#include "renderque.h"

#define qglGetError() 0

//these are shared with gl_rsurf - move to header
void R_MirrorChain (msurface_t *s);
void GL_SelectTexture (GLenum target);
void R_RenderDynamicLightmaps (msurface_t *fa, int shift);
void R_BlendLightmaps (void);


void PPL_BeginShadowMesh(dlight_t *dl);
void PPL_FinishShadowMesh(dlight_t *dl);
void PPL_FlushShadowMesh(dlight_t *dl);
void PPL_Shadow_Cache_Surface(msurface_t *surf);	//only caches for lighting
void PPL_Shadow_Cache_Leaf(mleaf_t *leaf);

extern qboolean r_inmirror;
extern int gldepthfunc;
extern int		*lightmap_textures;
extern int		lightmap_bytes;		// 1, 2, or 4

extern cvar_t		gl_detail;
extern cvar_t		gl_detailscale;
extern cvar_t		gl_overbright, gl_overbright_all;
extern cvar_t		r_fb_bmodels;
extern cvar_t		gl_part_flame;

extern cvar_t		gl_maxshadowlights;
extern cvar_t		r_shadow_realtime_world;
extern cvar_t		r_shadow_realtime_world_lightmaps;
extern cvar_t		r_shadow_glsl_offsetmapping;
extern cvar_t		r_shadow_glsl_offsetmapping_scale;
extern cvar_t		r_shadow_glsl_offsetmapping_bias;
extern int		detailtexture;
extern cvar_t gl_bump;
extern cvar_t gl_specular;
extern cvar_t gl_mylumassuck;
//end header confict

extern cvar_t gl_schematics;
extern cvar_t r_drawflat;
extern cvar_t r_wallcolour;
extern cvar_t r_floorcolour;

float r_lightmapintensity;	//1 or r_shadow_realtime_world_lightmaps 

int overbright;

extern lightmapinfo_t **lightmap;

extern model_t *currentmodel;
extern int		*deluxmap_textures;

extern int normalisationCubeMap;

int r_shadowframe;

int shadowsurfcount;
int shadowedgecount;
int shadowlightfaces;
int shadowemittedeges;

int ppl_specular_shader;
int ppl_specular_shader_vieworg;
int ppl_specular_shader_texr;
int ppl_specular_shader_texu;
int ppl_specular_shader_texf;

int GLR_LightmapShift (model_t *model);

//#define glBegin glEnd

qboolean PPL_ShouldDraw(void)
{
	if (r_inmirror)
	{
		if (currententity->flags & Q2RF_WEAPONMODEL)
			return false;
	}
	else
	{
		if (currententity->flags & Q2RF_WEAPONMODEL && r_secondaryview >= 2)
			return false;
		if (currententity->flags & Q2RF_EXTERNALMODEL && r_secondaryview != 3)
			return false;
//		if (currententity->keynum == (cl.viewentity[r_refdef.currentplayernum]?cl.viewentity[r_refdef.currentplayernum]:(cl.playernum[r_refdef.currentplayernum]+1)))
//			return false;
//		if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
//			continue;
		if (!Cam_DrawPlayer(r_refdef.currentplayernum, currententity->keynum-1))
			return false;
	}
	return true;
}

typedef struct {
	int count;
	msurface_t **s;
} shadowmeshsurfs_t;
typedef struct shadowmesh_s {
	int numindicies;
	int numverts;
	int *indicies;
	vec3_t *verts;

	//we also have a list of all the surfaces that this light lights.
	int numsurftextures;
	shadowmeshsurfs_t *litsurfs;

	unsigned char *litleaves;
} shadowmesh_t;


#define	Q2RF_WEAPONMODEL		4		// only draw through eyes

struct {
	short count;
	short count2;
	int next;
	int prev;
} edge[MAX_MAP_EDGES];
int firstedge;

vec3_t lightorg = {0, 0, 0};
float lightradius;



typedef struct {
	float xyz[3];	//xyz world coordinates
	float stw[2];	//base texture/normalmap/specular map st coords
	float stl[3];	//lightmap/deluxmap st coords (or attenuated distance*colour)
	float ncm[3];	//normalisation cube map (reflected light dir)
} surfvertexarray_t;

#define MAXARRAYVERTS	2048
static surfvertexarray_t varray_v[MAXARRAYVERTS];
static index_t varray_i[MAXARRAYVERTS];
//static unsigned int varray_i_forward[MAXARRAYVERTS];
//static unsigned int varray_i_polytotri[MAXARRAYVERTS];	//012 023 034 045...
int varray_ic;
int varray_vc;

#define inline static

extern qboolean varrayactive;	//used by the backend

inline void PPL_EnableVertexArrays(void)
{
	varrayactive = false;

	qglDisableClientState(GL_COLOR_ARRAY);
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglVertexPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->xyz);
}
inline void PPL_FlushArrays(void)
{
	if (varray_ic)
		qglDrawElements(GL_TRIANGLES, varray_ic, GL_INDEX_TYPE, varray_i);
	varray_ic = 0;
	varray_vc = 0;
}
static void PPL_GenerateArrays(msurface_t *surf)
{
	int vi;

	if (!surf->mesh)
		return;
	if (surf->mesh->numindexes > MAXARRAYVERTS)
		return;
	if (surf->mesh->numvertexes > MAXARRAYVERTS)
		return;
	if (!surf->mesh->st_array)
		return;
	if (!surf->mesh->lmst_array)
		return;
	if (varray_ic)	//FIXME: go muuuch faster please
		PPL_FlushArrays();
	for (vi = 0; vi < surf->mesh->numindexes; vi++)
		varray_i[vi] = surf->mesh->indexes[vi];
	for (vi = 0; vi < surf->mesh->numvertexes; vi++)
	{
		VectorCopy(surf->mesh->xyz_array[vi], varray_v[vi].xyz);
		varray_v[vi].stw[0] = surf->mesh->st_array[vi][0];
		varray_v[vi].stw[1] = surf->mesh->st_array[vi][1];
		varray_v[vi].stl[0] = surf->mesh->lmst_array[vi][0];
		varray_v[vi].stl[1] = surf->mesh->lmst_array[vi][1];
	}

	varray_vc = surf->mesh->numvertexes;
	varray_ic = surf->mesh->numindexes;
}
static void PPL_GenerateDetailArrays(msurface_t *surf)
{
	int vi;

	if (!surf->mesh)
		return;
	if (surf->mesh->numindexes > MAXARRAYVERTS)
		return;
	if (surf->mesh->numvertexes > MAXARRAYVERTS)
		return;
	if (!surf->mesh->st_array)
		return;
	if (!surf->mesh->lmst_array)
		return;
	if (varray_ic)	//FIXME: go muuuch faster please
		PPL_FlushArrays();
	for (vi = 0; vi < surf->mesh->numindexes; vi++)
		varray_i[vi] = surf->mesh->indexes[vi];
	for (vi = 0; vi < surf->mesh->numvertexes; vi++)
	{
		VectorCopy(surf->mesh->xyz_array[vi], varray_v[vi].xyz);
		varray_v[vi].stw[0] = surf->mesh->st_array[vi][0]*gl_detailscale.value;
		varray_v[vi].stw[1] = surf->mesh->st_array[vi][1]*gl_detailscale.value;
	}

	varray_vc = surf->mesh->numvertexes;
	varray_ic = surf->mesh->numindexes;
}

/*
static void PPL_BaseChain_NoLightmap(msurface_t *first, texture_t *tex)
{
	Sys_Error("1 TMU is disabled for now (surface has no lightmap)\n");
}
*/

static void PPL_BaseChain_NoBump_1TMU(msurface_t *first, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;
	msurface_t *s;

	PPL_EnableVertexArrays();

	qglDisable(GL_BLEND);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	GL_TexEnv(GL_REPLACE);
	GL_Bind (tex->tn.base);

	for (s=first; s ; s=s->texturechain)
	{
		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();

	qglEnable(GL_BLEND);
	GL_TexEnv(GL_MODULATE);

	if (gl_lightmap_format == GL_LUMINANCE || gl_lightmap_format == GL_RGB)
		qglBlendFunc (GL_ZERO, GL_SRC_COLOR);
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		qglColor4f (0,0,0,1);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (gl_lightmap_format == GL_RGBA)
		qglBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);

	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	vi = -1;
	for (s=first; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			GL_BindType(GL_TEXTURE_2D, lightmap_textures[vi] );
			if (lightmap[vi]->modified)
			{
				lightmap[vi]->modified = false;
				theRect = &lightmap[vi]->rectchange;
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
		}

		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/*static void PPL_BaseChain_NoBump_2TMU(msurface_t *s, texture_t *tex)
{	//doesn't merge surfaces, but tells gl to do each vertex arrayed surface individually, which means no vertex copying.
	int vi;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();

	if (tex->alphaed)
	{
		qglEnable(GL_BLEND);
		GL_TexEnv(GL_MODULATE);
	}
	else
	{
		qglDisable(GL_BLEND);
		GL_TexEnv(GL_REPLACE);
	}


	GL_MBind(GL_TEXTURE0_ARB, tex->gl_texturenum);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	GL_TexEnv(GL_MODULATE);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	vi = -1;
	for (; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			if (vi<0)
				qglEnable(GL_TEXTURE_2D);
			vi = s->lightmaptexturenum;

			if (vi>=0)
			{
				GL_Bind(lightmap_textures[vi] );
				if (lightmap[vi]->modified)
				{
					lightmap[vi]->modified = false;
					theRect = &lightmap[vi]->rectchange;
					qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
						LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
					theRect->l = LMBLOCK_WIDTH;
					theRect->t = LMBLOCK_HEIGHT;
					theRect->h = 0;
					theRect->w = 0;
				}
			}
			else
				qglDisable(GL_TEXTURE_2D);
		}

		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->st_array);
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->lmst_array);

		qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);

		qglDrawRangeElements(GL_TRIANGLES, 0, s->mesh->numvertexes, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
		//qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
	}

	qglDisable(GL_TEXTURE_2D);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
}*/

#if 0
static void PPL_BaseChain_NoBump_2TMU_Overbright(msurface_t *s, texture_t *tex)
{	//doesn't merge surfaces, but tells gl to do each vertex arrayed surface individually, which means no vertex copying.
	int vi;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();

	if (tex->alphaed || currententity->shaderRGBAf[3]<1)
	{
		if (*tex->name == '{')
		{
			qglEnable(GL_ALPHA_TEST);
			qglDisable(GL_BLEND);
			GL_TexEnv(GL_REPLACE);
		}
		else
		{
			qglEnable(GL_BLEND);
			GL_TexEnv(GL_MODULATE);
		}
	}
	else
	{
		qglDisable(GL_BLEND);
		GL_TexEnv(GL_REPLACE);
	}


	GL_MBind(GL_TEXTURE0_ARB, tex->tn.base);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_TexEnv(GL_MODULATE);


/*	if (currententity->shaderRGBAf[3]<1)
	{
		s->lightmaptexturenum = -1;
		qglBlendFunc(GL_SRC_COLOR, GL_ONE);
	}
*/
	if (overbright != 1)
	{
		GL_TexEnv(GL_COMBINE_ARB);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
		qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, overbright);	//this is the key
	}

	vi = -1;
	for (; s ; s=s->texturechain)
	{
		if (!s->mesh)	//urm.
			continue;
		if (s->mesh->numvertexes <= 1)
			continue;
		if (vi != s->lightmaptexturenum)
		{
			if (vi<0)
				qglEnable(GL_TEXTURE_2D);
			vi = s->lightmaptexturenum;

			if (vi>=0)
			{
				GL_Bind(lightmap_textures[vi] );
				if (lightmap[vi]->modified)
				{
					lightmap[vi]->modified = false;
					theRect = &lightmap[vi]->rectchange;
					qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
						LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
					theRect->l = LMBLOCK_WIDTH;
					theRect->t = LMBLOCK_HEIGHT;
					theRect->h = 0;
					theRect->w = 0;
				}
			}
			else
				qglDisable(GL_TEXTURE_2D);
		}

		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->st_array);
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->lmst_array);

		qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);

		qglDrawRangeElements(GL_TRIANGLES, 0, s->mesh->numvertexes, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
	}

	if (overbright != 1)
	{
		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);	//just in case
		GL_TexEnv(GL_MODULATE);
	}

	qglDisable(GL_TEXTURE_2D);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (tex->alphaed)
		qglDisable(GL_ALPHA_TEST);
}
#endif

static void PPL_BaseChain_NoBump_2TMU_Overbright(msurface_t *s, texture_t *tex, vbo_t *vbo, int fullbright)
{	//doesn't merge surfaces, but tells gl to do each vertex arrayed surface individually, which means no vertex copying.
#if 0
extern cvar_t temp1;
if (temp1.value)
{
	int vi;
	glRect_t    *theRect;
	int first = 0, last = 0;

	qglDisableClientState(GL_COLOR_ARRAY);
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, vbo->vboe);

	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vbocoord);
	qglVertexPointer(3, GL_FLOAT, 0, vbo->coord);

	
	GL_MBind(GL_TEXTURE0_ARB, tex->tn.base);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vbotexcoord);
	qglTexCoordPointer(2, GL_FLOAT, 0, vbo->texcoord);

	if (tex->alphaed || currententity->shaderRGBAf[3]<1)
	{
		if (*tex->name == '{')
		{
			qglEnable(GL_ALPHA_TEST);
			qglDisable(GL_BLEND);
			GL_TexEnv(GL_REPLACE);
		}
		else
		{
			qglEnable(GL_BLEND);
			GL_TexEnv(GL_MODULATE);
		}
	}
	else
	{
		qglDisable(GL_BLEND);
		GL_TexEnv(GL_REPLACE);
	}

	if (fullbright)
	{
		GL_MBind(GL_TEXTURE2_ARB, tex->tn.fullbright);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		//qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vbotexcoord);
		qglTexCoordPointer(2, GL_FLOAT, 0, vbo->texcoord);
		GL_TexEnv(GL_ADD);
		qglEnable(GL_TEXTURE_2D);
	}

	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vbolmcoord);
	qglTexCoordPointer(2, GL_FLOAT, 0, vbo->lmcoord);

	GL_TexEnv(GL_MODULATE);



/*	if (currententity->shaderRGBAf[3]<1)
	{
		s->lightmaptexturenum = -1;
		qglBlendFunc(GL_SRC_COLOR, GL_ONE);
	}
*/
	if (overbright != 1)
	{
		GL_TexEnv(GL_COMBINE_ARB);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
		qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, overbright);	//this is the key
	}

//	numidxs = 0;
	vi = -1;
	for (; s ; s=s->texturechain)
	{
		if (!s->mesh)	//urm.
			continue;
		if (s->mesh->numvertexes <= 1)
			continue;
		if (vi != s->lightmaptexturenum)
		{
			if (last != first)
				qglDrawElements(GL_TRIANGLES, last - first, GL_INDEX_TYPE, (index_t*)(first*sizeof(index_t)));
			last = first;

			if (vi<0)
				qglEnable(GL_TEXTURE_2D);
			vi = s->lightmaptexturenum;

			if (vi>=0)
			{
				GL_Bind(lightmap_textures[vi] );
				if (lightmap[vi]->modified)
				{
					lightmap[vi]->modified = false;
					theRect = &lightmap[vi]->rectchange;
					qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
						LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
					theRect->l = LMBLOCK_WIDTH;
					theRect->t = LMBLOCK_HEIGHT;
					theRect->h = 0;
					theRect->w = 0;
				}
			}
			else
				qglDisable(GL_TEXTURE_2D);
		}
		qglDrawRangeElements(GL_TRIANGLES, s->mesh->vbofirstvert, s->mesh->vbofirstvert+s->mesh->numvertexes-1, s->mesh->numindexes, GL_INDEX_TYPE, vbo->indicies + s->mesh->vbofirstelement);
		if (s->mesh->vbofirstelement != last)
		{
			if (last != first)
				qglDrawElements(GL_TRIANGLES, last - first, GL_INDEX_TYPE, (index_t*)(first*sizeof(index_t)));
			first = s->mesh->vbofirstelement;
			last = first;
		}
		last += s->mesh->numindexes;
	}
	if (last != first)
		qglDrawElements(GL_TRIANGLES, last - first, GL_INDEX_TYPE, (index_t*)(first*sizeof(index_t)));

	//rebinding vbos is meant to be cheap, thankfully
	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	if (overbright != 1)
	{
		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);	//just in case
		GL_TexEnv(GL_MODULATE);
	}

	//tmu 1 should be selected here
	qglDisable(GL_TEXTURE_2D);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (fullbright)
	{
		GL_SelectTexture(GL_TEXTURE2_ARB);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
		qglDisable(GL_TEXTURE_2D);
	}

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);


	if (tex->alphaed)
		qglDisable(GL_ALPHA_TEST);
}
else
#endif
{
		int vi;
	glRect_t    *theRect;
//	int first = 0, last = 0;

	qglDisableClientState(GL_COLOR_ARRAY);
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, vbo->vboe);

	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vbocoord);
	qglVertexPointer(3, GL_FLOAT, 0, vbo->coord);

	
	GL_MBind(GL_TEXTURE0_ARB, tex->tn.base);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vbotexcoord);
	qglTexCoordPointer(2, GL_FLOAT, 0, vbo->texcoord);

	if (tex->alphaed || currententity->shaderRGBAf[3]<1)
	{
		if (*tex->name == '{')
		{
			qglEnable(GL_ALPHA_TEST);
			qglDisable(GL_BLEND);
			GL_TexEnv(GL_REPLACE);
		}
		else
		{
			qglEnable(GL_BLEND);
			GL_TexEnv(GL_MODULATE);
		}
	}
	else
	{
		qglDisable(GL_BLEND);
		GL_TexEnv(GL_REPLACE);
	}

	if (fullbright)
	{
		GL_MBind(GL_TEXTURE2_ARB, tex->tn.fullbright);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		//qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vbotexcoord);
		qglTexCoordPointer(2, GL_FLOAT, 0, vbo->texcoord);
		GL_TexEnv(GL_ADD);
		qglEnable(GL_TEXTURE_2D);
	}

	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vbolmcoord);
	qglTexCoordPointer(2, GL_FLOAT, 0, vbo->lmcoord);

	GL_TexEnv(GL_MODULATE);



/*	if (currententity->shaderRGBAf[3]<1)
	{
		s->lightmaptexturenum = -1;
		qglBlendFunc(GL_SRC_COLOR, GL_ONE);
	}
*/
	if (overbright != 1)
	{
		GL_TexEnv(GL_COMBINE_ARB);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
		qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, overbright);	//this is the key
	}

//	numidxs = 0;
	vi = -1;
	for (; s ; s=s->texturechain)
	{
		if (!s->mesh)	//urm.
			continue;
		if (s->mesh->numvertexes <= 1)
			continue;
		if (vi != s->lightmaptexturenum)
		{
//			if (last != first)
//				qglDrawElements(GL_TRIANGLES, last - first, GL_INDEX_TYPE, (index_t*)(first*sizeof(index_t)));
//			last = first;

			if (vi<0)
				qglEnable(GL_TEXTURE_2D);
			vi = s->lightmaptexturenum;

			if (vi>=0)
			{
				GL_Bind(lightmap_textures[vi] );
				if (lightmap[vi]->modified)
				{
					lightmap[vi]->modified = false;
					theRect = &lightmap[vi]->rectchange;
					qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
						LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
					theRect->l = LMBLOCK_WIDTH;
					theRect->t = LMBLOCK_HEIGHT;
					theRect->h = 0;
					theRect->w = 0;
				}
			}
			else
				qglDisable(GL_TEXTURE_2D);
		}
		qglDrawRangeElements(GL_TRIANGLES, s->mesh->vbofirstvert, s->mesh->vbofirstvert+s->mesh->numvertexes-1, s->mesh->numindexes, GL_INDEX_TYPE, vbo->indicies + s->mesh->vbofirstelement);
//		if (s->mesh->vbofirstelement != last)
//		{
//			if (last != first)
//				qglDrawElements(GL_TRIANGLES, last - first, GL_INDEX_TYPE, (index_t*)(first*sizeof(index_t)));
//			first = s->mesh->vbofirstelement;
//			last = first;
//		}
//		last += s->mesh->numindexes;
	}
//	if (last != first)
//		qglDrawElements(GL_TRIANGLES, last - first, GL_INDEX_TYPE, (index_t*)(first*sizeof(index_t)));

	//rebinding vbos is meant to be cheap, thankfully
	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	if (overbright != 1)
	{
		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);	//just in case
		GL_TexEnv(GL_MODULATE);
	}

	//tmu 1 should be selected here
	qglDisable(GL_TEXTURE_2D);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (fullbright)
	{
		GL_SelectTexture(GL_TEXTURE2_ARB);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
		qglDisable(GL_TEXTURE_2D);
	}

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);


	if (tex->alphaed)
		qglDisable(GL_ALPHA_TEST);
}
}

/*
static void PPL_BaseChain_NoBump_2TMU_TEST(msurface_t *s, texture_t *tex)
{	//this was just me testing efficiency between arrays/glbegin.
	int vi, i;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();

	if (tex->alphaed)
	{
		qglEnable(GL_BLEND);
		GL_TexEnv(GL_MODULATE);
	}
	else
	{
		qglDisable(GL_BLEND);
		GL_TexEnv(GL_REPLACE);
	}

	qglCullFace(GL_BACK);


	GL_MBind(GL_TEXTURE0_ARB, tex->gl_texturenum);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	GL_TexEnv(GL_MODULATE);

	vi = -1;
	for (; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			if (vi<0)
				qglEnable(GL_TEXTURE_2D);
			vi = s->lightmaptexturenum;

			if (vi>=0)
			{
				GL_Bind(lightmap_textures[vi] );
				if (lightmap[vi]->modified)
				{
					lightmap[vi]->modified = false;
					theRect = &lightmap[vi]->rectchange;
					qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
						LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
					theRect->l = LMBLOCK_WIDTH;
					theRect->t = LMBLOCK_HEIGHT;
					theRect->h = 0;
					theRect->w = 0;
				}
			}
			else
				qglDisable(GL_TEXTURE_2D);
		}

		qglBegin(GL_POLYGON);
		switch(s->mesh->numvertexes)
		{
		default:
			for (i = s->mesh->numvertexes-1; i >= 6; i--)
			{
				qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, s->mesh->st_array[i][0], s->mesh->st_array[i][1]);
				qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, s->mesh->lmst_array[i][0], s->mesh->lmst_array[i][1]);
				qglVertex3fv(s->mesh->xyz_array[i]);
			}
		case 6:
			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, s->mesh->st_array[5][0], s->mesh->st_array[5][1]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, s->mesh->lmst_array[5][0], s->mesh->lmst_array[5][1]);
			qglVertex3fv(s->mesh->xyz_array[5]);
		case 5:
			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, s->mesh->st_array[4][0], s->mesh->st_array[4][1]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, s->mesh->lmst_array[4][0], s->mesh->lmst_array[4][1]);
			qglVertex3fv(s->mesh->xyz_array[4]);
		case 4:
			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, s->mesh->st_array[3][0], s->mesh->st_array[3][1]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, s->mesh->lmst_array[3][0], s->mesh->lmst_array[3][1]);
			qglVertex3fv(s->mesh->xyz_array[3]);
		case 3:
			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, s->mesh->st_array[2][0], s->mesh->st_array[2][1]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, s->mesh->lmst_array[2][0], s->mesh->lmst_array[2][1]);
			qglVertex3fv(s->mesh->xyz_array[2]);
		case 2:
			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, s->mesh->st_array[1][0], s->mesh->st_array[1][1]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, s->mesh->lmst_array[1][0], s->mesh->lmst_array[1][1]);
			qglVertex3fv(s->mesh->xyz_array[1]);
		case 1:
			qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, s->mesh->st_array[0][0], s->mesh->st_array[0][1]);
			qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, s->mesh->lmst_array[0][0], s->mesh->lmst_array[0][1]);
			qglVertex3fv(s->mesh->xyz_array[0]);
		case 0:
			break;
		}
		qglEnd();
	}

	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE0_ARB);
}
*/

static void PPL_BaseChain_Bump_2TMU(msurface_t *first, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;
	msurface_t *s;
	PPL_EnableVertexArrays();

	if (tex->alphaed)
	{
		qglEnable(GL_BLEND);
		GL_TexEnv(GL_MODULATE);
	}
	else
	{
		qglDisable(GL_BLEND);
		GL_TexEnv(GL_REPLACE);
	}

	//Bind normal map to texture unit 0
	GL_MBind(GL_TEXTURE0_ARB, tex->tn.bump);
	qglEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_ARB);
	qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_ARB);
	qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);


	vi = -1;
	for (s=first; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			GL_Bind(deluxmap_textures[vi] );
			if (lightmap[vi]->deluxmodified)
			{
				lightmap[vi]->deluxmodified = false;
				theRect = &lightmap[vi]->deluxrectchange;
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
					lightmap[vi]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
		}

		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();

	qglEnable(GL_BLEND);
	qglBlendFunc(GL_DST_COLOR, GL_ZERO);

	GL_MBind(GL_TEXTURE0_ARB, tex->tn.base);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_MODULATE);

	vi = -1;
	for (s=first; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			GL_Bind(lightmap_textures[vi] );
			if (lightmap[vi]->modified)
			{
				lightmap[vi]->modified = false;
				theRect = &lightmap[vi]->rectchange;
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
		}

		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisable(GL_BLEND);

	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);
	GL_SelectTexture(GL_TEXTURE0_ARB);
}

static void PPL_BaseChain_Bump_4TMU(msurface_t *s, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();

	//Bind normal map to texture unit 0
	GL_MBind(GL_TEXTURE0_ARB, tex->tn.bump);
	qglEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_REPLACE);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	//1 gets the deluxmap
	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_ARB);
	qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	//2 gets the diffusemap
	GL_MBind(GL_TEXTURE2_ARB, tex->tn.base);
	qglEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_MODULATE);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	//3 gets the lightmap
	GL_SelectTexture(GL_TEXTURE3_ARB);
	qglEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_MODULATE);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	vi = -1;
	for (; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			GL_MBind(GL_TEXTURE1_ARB, deluxmap_textures[vi] );
			if (lightmap[vi]->deluxmodified)
			{
				lightmap[vi]->deluxmodified = false;
				theRect = &lightmap[vi]->deluxrectchange;
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
					lightmap[vi]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			GL_MBind(GL_TEXTURE3_ARB, lightmap_textures[vi] );
			if (lightmap[vi]->modified)
			{
				lightmap[vi]->modified = false;
				theRect = &lightmap[vi]->rectchange;
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
		}

		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();

	GL_SelectTexture(GL_TEXTURE3_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE2_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	GL_TexEnv(GL_MODULATE);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);


	GL_TexEnv(GL_MODULATE);
}

#ifdef SPECULAR
//Draw a texture chain with specular exponant 1.
//erm...
//this uses the wrong stuff to work on gf4tis.
/*
static void PPL_BaseChain_Specular_4TMU(msurface_t *first, texture_t *tex)
{
//if I ever do write this function, It'll take a couple of passes.
	int vi;
	glRect_t    *theRect;
	msurface_t *s;

	glColorMask(1,1,1,0);

	PPL_EnableVertexArrays();

	if (qglGetError())
		Con_Printf("Error before PPL_BaseChain_Specular\n");

	//first 4 texture units: (N.((L+V)/2))^2
glDisable(GL_BLEND);
	qglActiveTextureARB(GL_TEXTURE0_ARB);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumbumpmap);
	glEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_REPLACE);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	if (qglGetError())
		Con_Printf("Error binding dot3 tmu1\n");

	qglActiveTextureARB(GL_TEXTURE1_ARB);
	glDisable(GL_TEXTURE_2D);
	GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	if (qglGetError())
		Con_Printf("Error binding dot3 cubemap\n");
	GL_TexEnv(GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);
	if (qglGetError())
		Con_Printf("Error binding dot3 combine\n");
	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->ncm);

	if (qglGetError())
		Con_Printf("Error binding dot3 tmu2\n");

	//prev*prev (the exponential)
	qglActiveTextureARB(GL_TEXTURE2_ARB);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumbumpmap);	//need to bind something.
	glEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);

	if (qglGetError())
		Con_Printf("Error binding prev*prev\n");

	qglActiveTextureARB(GL_TEXTURE3_ARB);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumspec);
	glEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	qglClientActiveTextureARB(GL_TEXTURE3_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	if (qglGetError())
		Con_Printf("Error binding specular in PPL_BaseChain_Specular\n");

	for (s = first; s ; s=s->texturechain)
	{
		PPL_GenerateArraysBlinnCubeMap(s);
	}
	PPL_FlushArrays();

glEnable(GL_BLEND);
glBlendFunc(GL_DST_COLOR, GL_ZERO);
	// Add normal dot delux times diffusemap then multiple the entire lot by the lightmap.
	qglActiveTextureARB(GL_TEXTURE0_ARB);
	GL_TexEnv(GL_REPLACE);

	qglActiveTextureARB(GL_TEXTURE1_ARB);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);
	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	qglActiveTextureARB(GL_TEXTURE2_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenum);
	GL_TexEnv(GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	qglActiveTextureARB(GL_TEXTURE3_ARB);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	qglClientActiveTextureARB(GL_TEXTURE3_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	vi = -1;
	for (s = first; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			qglActiveTextureARB(GL_TEXTURE1_ARB);
			GL_BindType(GL_TEXTURE_2D, deluxmap_textures[vi] );
			if (lightmap[vi]->deluxmodified)
			{
				lightmap[vi]->deluxmodified = false;
				theRect = &lightmap[vi]->deluxrectchange;
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
					lightmap[vi]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			qglActiveTextureARB(GL_TEXTURE3_ARB);
			GL_BindType(GL_TEXTURE_2D, lightmap_textures[vi] );
			if (lightmap[vi]->modified)
			{
				lightmap[vi]->modified = false;
				theRect = &lightmap[vi]->rectchange;
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
		}
		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();

	glColorMask(1,1,1,0);

	if (qglGetError())
		Con_Printf("Error drawing in PPL_BaseChain_Specular\n");

	
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisable(GL_TEXTURE_2D);

	qglActiveTextureARB(GL_TEXTURE0_ARB);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
}
*/

void PPL_LoadSpecularFragmentProgram(void)
{
//#define SMOOOOOTH	//define this to calculate everything per-pixel as opposed to interpolating the halfdir
	char *vert = 
		"varying vec2 tcbase;\n"
		"varying vec2 tclm;\n"
		"uniform vec3 vieworg;\n"
#ifdef SMOOOOOTH
		"varying vec3 fragpos;\n"
		"varying vec3 norm;\n"
#else
		"uniform vec3 texr, texu, texf;\n"
		"varying vec3 halfnorm;\n"
#endif
		"void main (void)\n"
		"{\n"
		"	gl_Position = ftransform();\n"

		"	tcbase = gl_MultiTexCoord0.xy;\n"	//pass the texture coords straight through
		"	tclm = gl_MultiTexCoord1.xy;\n"

#ifdef SMOOOOOTH
		" fragpos = vec3(gl_Vertex.xyz);\n"
		"norm = gl_Normal;\n"
#else
		"	vec3 eye = normalize(vieworg - vec3(gl_Vertex.xyz));\n"
		"	vec3 halfdir = (eye + texf) / 2.0;\n"
		"	halfnorm.x = dot(texr, halfdir);\n"	//put halfnorm into object space
		"	halfnorm.y = dot(texu, halfdir);\n"
		"	halfnorm.z = dot(texf, halfdir);\n"
#endif
		"}\n"
		;

/*
			VectorSubtract(r_refdef.vieworg, v, eye);
		VectorNormalize(eye);
		VectorAdd(eye, (v+7), halfdir);	//v+7 is the light dir (or plane normal)
		varray_v[varray_vc].ncm[0] = DotProduct(surf->texinfo->vecs[0], halfdir);
		varray_v[varray_vc].ncm[1] = DotProduct(surf->texinfo->vecs[1], halfdir);
		if (surf->flags & SURF_PLANEBACK)
			varray_v[varray_vc].ncm[2] = -DotProduct(surf->plane->normal, halfdir);
		else
			varray_v[varray_vc].ncm[2] = DotProduct(surf->plane->normal, halfdir);
*/
	char *frag =
		"uniform sampler2D baset;\n"
		"uniform sampler2D bumpt;\n"
		"uniform sampler2D lightmapt;\n"
		"uniform sampler2D deluxt;\n"
		"uniform sampler2D speculart;\n"
		"varying vec2 tcbase;\n"
		"varying vec2 tclm;\n"

#ifdef SMOOOOOTH
		"uniform vec3 vieworg;\n"
		"varying vec3 fragpos;\n"
		"uniform vec3 texr, texu, texf;\n"
#else
		"varying vec3 halfnorm;\n"
#endif
		"void main (void)\n"
		"{\n"
		"	vec3 bases = vec3(texture2D(baset, tcbase));\n"
		"	vec3 bumps = vec3(texture2D(bumpt, tcbase)) * 2.0 - 1.0;\n"
		"	vec3 deluxs = vec3(texture2D(deluxt, tclm)) * 2.0 - 1.0;\n"
		"	vec3 lms = vec3(texture2D(lightmapt, tclm));\n"
		"	vec3 specs = vec3(texture2D(speculart, tcbase));\n"
		"	vec3 diff, spec;\n"

#ifdef SMOOOOOTH
		"	vec3 eye = normalize(vieworg - fragpos);\n"
		"	vec3 halfdir = (eye + texf) / 2.0;\n"
		"	vec3 halfnorm;\n"
		"	halfnorm.x = dot(texr, halfdir);\n"	//put halfnorm into object space
		"	halfnorm.y = dot(texu, halfdir);\n"
		"	halfnorm.z = dot(texf, halfdir);\n"
#endif

		"	diff = bases * dot(bumps, deluxs);\n"
		"	float dv = dot(normalize(halfnorm), bumps);\n"
		"	spec = pow(dv, 16.0) * specs;\n"
		"	gl_FragColor = vec4((diff+spec)*lms, 1.0);\n"
		"}\n"
		;

	ppl_specular_shader = GLSlang_CreateProgram(NULL, vert, frag);

	if (ppl_specular_shader)
	{
		GLSlang_UseProgram(ppl_specular_shader);

		qglUniform1iARB(qglGetUniformLocationARB(ppl_specular_shader, "baset"), 0);
		qglUniform1iARB(qglGetUniformLocationARB(ppl_specular_shader, "bumpt"), 1);
		qglUniform1iARB(qglGetUniformLocationARB(ppl_specular_shader, "lightmapt"), 2);
		qglUniform1iARB(qglGetUniformLocationARB(ppl_specular_shader, "deluxt"), 3);
		qglUniform1iARB(qglGetUniformLocationARB(ppl_specular_shader, "speculart"), 4);

		ppl_specular_shader_vieworg = qglGetUniformLocationARB(ppl_specular_shader, "vieworg");
		ppl_specular_shader_texr = qglGetUniformLocationARB(ppl_specular_shader, "texr");
		ppl_specular_shader_texu = qglGetUniformLocationARB(ppl_specular_shader, "texu");
		ppl_specular_shader_texf = qglGetUniformLocationARB(ppl_specular_shader, "texf");

		GLSlang_UseProgram(0);
	}
}

static void PPL_BaseChain_Specular_FP(msurface_t *s, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();

	GLSlang_UseProgram(ppl_specular_shader);

	if (qglGetError())
		Con_Printf("GL Error on shadow lighting\n");

	GL_MBind(GL_TEXTURE0_ARB, tex->tn.base);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_MBind(GL_TEXTURE1_ARB, tex->tn.bump);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

//	GL_MBind(GL_TEXTURE2_ARB, lightmap_textures[vi] );

//	GL_MBind(GL_TEXTURE3_ARB, deluxmap_textures[vi] );

	GL_MBind(GL_TEXTURE4_ARB, tex->tn.specular);

	qglUniform3fvARB(ppl_specular_shader_vieworg, 1, r_refdef.vieworg);

	if (qglGetError())
		Con_Printf("GL Error early during PPL_BaseChain_Specular_FP\n");

	vi = -1;
	for (; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			vi = s->lightmaptexturenum;

			GL_MBind(GL_TEXTURE3_ARB, deluxmap_textures[vi] );
			if (lightmap[vi]->deluxmodified)
			{
				lightmap[vi]->deluxmodified = false;
				theRect = &lightmap[vi]->deluxrectchange;
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
					lightmap[vi]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			GL_MBind(GL_TEXTURE2_ARB, lightmap_textures[vi] );
			if (lightmap[vi]->modified)
			{
				lightmap[vi]->modified = false;
				theRect = &lightmap[vi]->rectchange;
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
		}

		qglUniform3fvARB(ppl_specular_shader_texr, 1, s->texinfo->vecs[0]);
		qglUniform3fvARB(ppl_specular_shader_texu, 1, s->texinfo->vecs[1]);
		if (s->flags & SURF_PLANEBACK)
			qglUniform3fARB(ppl_specular_shader_texf, -s->plane->normal[0], -s->plane->normal[1], -s->plane->normal[2]);
		else
			qglUniform3fvARB(ppl_specular_shader_texf, 1, s->plane->normal);

		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->st_array);
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->lmst_array);
		
		qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);
		qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
	}

	GLSlang_UseProgram(0);

	GL_SelectTexture(GL_TEXTURE2_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);


	if (qglGetError())
		Con_Printf("GL Error on specular lighting\n");
}

#endif

static vec_t wallcolour[4] = {0,0,0,1};
static vec_t floorcolour[4] = {0,0,0,1};
static int walltexture = 0;
static int floortexture = 0;
static qboolean simpletextures = false;

//single textured.
void GLR_Wallcolour_Callback(struct cvar_s *var, char *oldvalue)
{
	SCR_StringToRGB(var->string, wallcolour, 255);
}

void GLR_Floorcolour_Callback(struct cvar_s *var, char *oldvalue)
{
	SCR_StringToRGB(var->string, floorcolour, 255);
}

void GLR_Walltexture_Callback(struct cvar_s *var, char *oldvalue)
{
	if (!var->string[0])
	{
		walltexture = 0;
		if (!floortexture)
			simpletextures = false;
		return;
	}

	walltexture = Mod_LoadHiResTexture(var->string, NULL, true, false, true);
	if (walltexture)
		simpletextures = true;
}

void GLR_Floortexture_Callback(struct cvar_s *var, char *oldvalue)
{
	if (!var->string[0])
	{
		floortexture = 0;
		if (!walltexture)
			simpletextures = false;
		return;
	}

	floortexture = Mod_LoadHiResTexture(var->string, NULL, true, false, true);
	if (floortexture)
		simpletextures = true;
}

static void PPL_BaseChain_Flat(msurface_t *first)
{
	msurface_t *s;
	int iswall = -1;
	int vi=-10;
	glRect_t    *theRect;
	
	if (!r_lightmapintensity)
	{	//these are bad. :(

		PPL_EnableVertexArrays();
		qglColor4f(0,0,0,1);
		qglDisable(GL_TEXTURE_2D);	//texturing? who wants texturing?!?!
		for (s = first; s ; s=s->texturechain)
			PPL_GenerateArrays(s);
		PPL_FlushArrays();
		qglEnable(GL_TEXTURE_2D);
		return;
	}

	PPL_EnableVertexArrays();
	GL_TexEnv(GL_MODULATE);

	for (s = first; s ; s=s->texturechain)
	{
		if (s->mesh->numvertexes < 3) continue;
		if (vi != s->lightmaptexturenum)
		{
			if (vi < 0)
			{
				qglEnable(GL_TEXTURE_2D);
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
			vi = s->lightmaptexturenum;
			if (s->lightmaptexturenum < 0)
			{
				qglDisable(GL_TEXTURE_2D);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}

			if (vi>=0)
			{
				GL_Bind(lightmap_textures[vi] );
				if (lightmap[vi]->modified)
				{
					lightmap[vi]->modified = false;
					theRect = &lightmap[vi]->rectchange;
					qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
						LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
					theRect->l = LMBLOCK_WIDTH;
					theRect->t = LMBLOCK_HEIGHT;
					theRect->h = 0;
					theRect->w = 0;
				}
			}
		}

		if (s->plane->normal[2] <= 0.5)
		{
			if (iswall != 0)
			{
				iswall=0;
				qglColor4fv(wallcolour);
			}
		}
		else if (iswall != 1)
		{
			iswall=1;
			qglColor4fv(floorcolour);
		}

		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->lmst_array);
		qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);
		qglDrawRangeElements(GL_TRIANGLES, 0, s->mesh->numvertexes, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
	}

	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglColor3f(1,1,1);
}

static int nprtextures[10];

void GLR_Drawflat_Callback(struct cvar_s *var, char *oldvalue)
{
	int i;

	if (var->value != 2)
		return;

	for (i = 0; i < sizeof(nprtextures)/sizeof(nprtextures[0]); i++)
	{
		nprtextures[i] = Mod_LoadHiResTexture(va("sketch%i", i+1), "sketch", true, false, false);
		if (!nprtextures[i])
		{
			int data[128*128];
			FILE *file;
			unsigned char *f;
			int p;

			file = fopen(va("nprtextures/tex%i_3_128_128.raw", i+1), "rb");
		
			if (file)
			{
				f = Hunk_TempAlloc(128*128*3);
				if (fread(f, 128*3, 128, file) == 128)
				{
					for (p = 0; p < 128*128; p++)
						data[p] = LittleLong(f[p*3] + (f[p*3+1]<<8) + (f[p*3+2]<<16) + (255<<24));
					nprtextures[i] = GL_LoadTexture32 (va("textures/tex%i_3_128_128.raw", i+1), 128, 128, data, true, false);
				}
				fclose(file);
			}
		}
	}
}

static void PPL_BaseChain_SimpleTexture(msurface_t *first)
{
	msurface_t *s;
	int vi=-10;
	int iswall;
	int oldwall=-1;
	glRect_t    *theRect;

	GL_SelectTexture(GL_TEXTURE0_ARB);
	PPL_EnableVertexArrays();

//draw the surface properly
	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	GL_TexEnv(GL_MODULATE);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	GL_TexEnv(GL_MODULATE);
	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	for (s = first; s ; s=s->texturechain)
	{
		if (s->plane->normal[2] <= 0.5)
			iswall = 1;
		else
			iswall = 0;

		if (vi != s->lightmaptexturenum || iswall != oldwall)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;
			oldwall = iswall;

			if (iswall)
			{
				GL_MBind(GL_TEXTURE0_ARB, walltexture);			
				qglColor4fv(wallcolour);
			}
			else
			{
				GL_MBind(GL_TEXTURE0_ARB, floortexture);
				qglColor4fv(floorcolour);
			}

			if (vi < 0)
				GL_MBind(GL_TEXTURE1_ARB, 0 );
			else
			{
				GL_MBind(GL_TEXTURE1_ARB, lightmap_textures[vi] );
				if (lightmap[vi]->modified)
				{
					lightmap[vi]->modified = false;
					theRect = &lightmap[vi]->rectchange;
					qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
						LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
					theRect->l = LMBLOCK_WIDTH;
					theRect->t = LMBLOCK_HEIGHT;
					theRect->h = 0;
					theRect->w = 0;
				}
			}
		}
		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisableClientState(GL_VERTEX_ARRAY);
	qglDisable(GL_TEXTURE_2D);
	qglColor3f(1,1,1);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisableClientState(GL_VERTEX_ARRAY);
	qglEnable(GL_TEXTURE_2D);
	qglColor3f(1,1,1);
}

static void PPL_BaseChain_NPR_Sketch(msurface_t *first)
{
	msurface_t *s;
	int vi=-10;
	int i;
	glRect_t    *theRect;

	GL_SelectTexture(GL_TEXTURE0_ARB);
	PPL_EnableVertexArrays();

//draw the surface properly
	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	GL_TexEnv(GL_MODULATE);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	GL_TexEnv(GL_MODULATE);
	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	qglColor3f(1,1,1);
	for (s = first; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			GL_MBind(GL_TEXTURE0_ARB, nprtextures[rand()%10]);

			if (vi < 0)
				GL_MBind(GL_TEXTURE1_ARB, 0 );
			else
			{
				GL_MBind(GL_TEXTURE1_ARB, lightmap_textures[vi] );
				if (lightmap[vi]->modified)
				{
					lightmap[vi]->modified = false;
					theRect = &lightmap[vi]->rectchange;
					qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
						LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
						lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
					theRect->l = LMBLOCK_WIDTH;
					theRect->t = LMBLOCK_HEIGHT;
					theRect->h = 0;
					theRect->w = 0;
				}
			}
		}
		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisable(GL_TEXTURE_2D);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	qglDisableClientState(GL_VERTEX_ARRAY);
	qglDisableClientState(GL_COLOR_ARRAY);

	//draw some extra lines around the edge for added coolness.
	qglColor3f(0,0,0);

	for (vi = 0; vi < 2; vi++)
	{
		for (s = first; s ; s=s->texturechain)
		{
			if (!s->mesh)
				continue;

			for (i=0; i<s->mesh->numindexes; i+=3)
			{
				qglBegin(GL_LINE_LOOP);
				qglVertex3f(s->mesh->xyz_array[s->mesh->indexes[i+0]][0]+5*(rand()/(float)RAND_MAX-0.5),
							s->mesh->xyz_array[s->mesh->indexes[i+0]][1]+5*(rand()/(float)RAND_MAX-0.5),
							s->mesh->xyz_array[s->mesh->indexes[i+0]][2]+5*(rand()/(float)RAND_MAX-0.5));
				qglVertex3f(s->mesh->xyz_array[s->mesh->indexes[i+1]][0]+5*(rand()/(float)RAND_MAX-0.5),
							s->mesh->xyz_array[s->mesh->indexes[i+1]][1]+5*(rand()/(float)RAND_MAX-0.5),
							s->mesh->xyz_array[s->mesh->indexes[i+1]][2]+5*(rand()/(float)RAND_MAX-0.5));
				qglVertex3f(s->mesh->xyz_array[s->mesh->indexes[i+2]][0]+5*(rand()/(float)RAND_MAX-0.5),
							s->mesh->xyz_array[s->mesh->indexes[i+2]][1]+5*(rand()/(float)RAND_MAX-0.5),
							s->mesh->xyz_array[s->mesh->indexes[i+2]][2]+5*(rand()/(float)RAND_MAX-0.5));
				qglEnd();
			}
		}
	}

	qglEnable(GL_TEXTURE_2D);
}

static void PPL_BaseTextureChain(msurface_t *first)
{
	texture_t	*ot = first->texinfo->texture;
	texture_t	*t;
#ifdef Q3SHADERS
	shader_t *shader;
#endif

	if (r_drawflat.value||!r_lightmapintensity)
	{
		if (r_drawflat.value == 2)
		{
			if (gl_mtexarbable >= 2)	//shiesh!.
			{
				PPL_BaseChain_NPR_Sketch(first);
				return;
			}
		}
		else if (!(first->flags & SURF_NOFLAT))
		{
			if (gl_mtexarbable >= 2 && simpletextures)
				PPL_BaseChain_SimpleTexture(first);
			else
				PPL_BaseChain_Flat(first);	//who cares about texture? :/
			return;
		}
	}
	
	t = R_TextureAnimation (ot);

#ifdef Q3SHADERS
	shader = t->shader;
	if (shader)// && shader->style != SSTYLE_LIGHTMAPPED)
	{
		meshbuffer_t mb;
		msurface_t *s;
		int vi=-1;
		int redraw = false;
		int dlb;

		glRect_t    *theRect;
		if (shader->flags & SHADER_FLARE )
		{
			dlight_t *dl;
			while(first)
			{	//a quick hack to convert to a dlight
				dl = CL_AllocDlight(0);
				VectorCopy(first->mesh->xyz_array[0], dl->origin);
				dl->color[0] = 0.2;
				dl->color[1] = 0.2;
				dl->color[2] = 0.2;
				dl->radius = 50;

				//flashblend only
				dl->flags = LFLAG_ALLOW_FLASH;

				first = first->texturechain;
			}
			return;
		}

		if (!varrayactive)
			R_IBrokeTheArrays();

		mb.entity = &r_worldentity;
		mb.shader = shader;
		mb.mesh = NULL;
		mb.fog = NULL;
		mb.infokey = -2;
		if (first->dlightframe == r_framecount)
			mb.dlightbits = first->dlightbits;
		else
			mb.dlightbits = 0;

		GL_DisableMultitexture();

		qglShadeModel(GL_SMOOTH);

		{
			for (s = first; s ; s=s->texturechain)
			{
				if (vi != s->lightmaptexturenum)
				{
					vi = s->lightmaptexturenum;
					if (vi >= 0)
					{
						if (gl_bump.value)
						if (lightmap[vi]->deluxmodified)
						{
							GL_BindType(GL_TEXTURE_2D, deluxmap_textures[vi] );
							lightmap[vi]->deluxmodified = false;
							theRect = &lightmap[vi]->deluxrectchange;
							qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
								LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
								lightmap[vi]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
							theRect->l = LMBLOCK_WIDTH;
							theRect->t = LMBLOCK_HEIGHT;
							theRect->h = 0;
							theRect->w = 0;
						}
						if (lightmap[vi]->modified)
						{
							GL_BindType(GL_TEXTURE_2D, lightmap_textures[vi] );
							lightmap[vi]->modified = false;
							theRect = &lightmap[vi]->rectchange;
							qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
								LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
								lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
							theRect->l = LMBLOCK_WIDTH;
							theRect->t = LMBLOCK_HEIGHT;
							theRect->h = 0;
							theRect->w = 0;
						}
					}
				}

				if (s->mesh)
				{
					if (s->dlightframe == r_framecount)
						dlb = s->dlightbits;
					else
						dlb = 0;
					redraw = mb.dlightbits != dlb || mb.fog != s->fog || mb.infokey != vi||R_MeshWillExceed(s->mesh);

					if (redraw)
					{
						if (mb.mesh)
							R_RenderMeshBuffer ( &mb, false );
						redraw = false;
					}

					mb.infokey = vi;
					mb.mesh = s->mesh;
					mb.fog = s->fog;
					mb.dlightbits = dlb;
					R_PushMesh(s->mesh, mb.shader->features);
				}
			}
		}

		if (mb.mesh)
			R_RenderMeshBuffer ( &mb, false );

		return;
	}
#endif
	qglEnable(GL_TEXTURE_2D);





	if (first->flags & SURF_DRAWTURB)
	{
		GL_DisableMultitexture();
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Bind (t->tn.base);
		EmitWaterPolyChain (first, currententity->shaderRGBAf[3]);

		qglDisable(GL_BLEND);
		qglColor4f(1,1,1, 1);

		t->texturechain = NULL;	//no lighting effects. (good job these don't animate eh?)
		return;
	}
/*	else if (s->lightmaptexturenum < 0)	//no lightmap
	{
		PPL_BaseChain_NoLightmap(first, t);
	}*/
	else if (gl_mtexarbable < 2)
	{	//multitexture isn't supported.
		PPL_BaseChain_NoBump_1TMU(first, t);
	}
	else
	{
		if (gl_bump.value && currentmodel->deluxdata && t->tn.bump)
		{
			if (gl_mtexarbable>=4)
			{
				if (t->tn.specular && gl_specular.value)
				{
					if (ppl_specular_shader)
						PPL_BaseChain_Specular_FP(first, t);
//					else if (gl_mtexarbable>=8)
//						PPL_BaseChain_Specular_8TMU(first, t);
					else
						PPL_BaseChain_Bump_4TMU(first, t);	//can't do specular.
				}
				else
					PPL_BaseChain_Bump_4TMU(first, t);
			}
			else
				PPL_BaseChain_Bump_2TMU(first, t);
		}
		else
		{
//			PPL_BaseChain_NoBump_2TMU_TEST(first, t);
//			PPL_BaseChain_NoBump_2TMU(first, t);

			if (gl_mtexarbable>=3)
			{
				PPL_BaseChain_NoBump_2TMU_Overbright(first, t, &ot->vbo, r_fb_bmodels.value && cls.allow_luma);
				return;
			}
			PPL_BaseChain_NoBump_2TMU_Overbright(first, t, &ot->vbo, false);
		}
	}

	/*if we couldn't draw fullbrights above due to tmu limitations, draw it now*/
	if (t->tn.fullbright && r_fb_bmodels.value && cls.allow_luma)
	{
		msurface_t *s;

		GL_Bind(t->tn.fullbright);
		qglEnable(GL_BLEND);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
		if (gl_mylumassuck.value)
			qglEnable(GL_ALPHA_TEST);

		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ot->vbo.vboe);

		qglEnableClientState(GL_VERTEX_ARRAY);
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, ot->vbo.vbocoord);
		qglVertexPointer(3, GL_FLOAT, 0, ot->vbo.coord);

		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, ot->vbo.vbotexcoord);
		qglTexCoordPointer(2, GL_FLOAT, 0, ot->vbo.texcoord);

		for (s = first; s ; s=s->texturechain)
		{
			qglDrawRangeElements(GL_TRIANGLES, s->mesh->vbofirstvert, s->mesh->vbofirstvert+s->mesh->numvertexes-1, s->mesh->numindexes, GL_INDEX_TYPE, ot->vbo.indicies + s->mesh->vbofirstelement);
		}

		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		qglDisable(GL_BLEND);

		if (gl_mylumassuck.value)
			qglDisable(GL_ALPHA_TEST);
	}
}


static void PPL_DetailTextureChain(msurface_t *first)
{
	texture_t	*ot = first->texinfo->texture;
	texture_t	*t;
	msurface_t	*s;

	t = R_TextureAnimation (ot);

	if (detailtexture && gl_detail.value)
	{
		GL_Bind(detailtexture);
		qglBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

		PPL_EnableVertexArrays();
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
		for (s = first; s ; s=s->texturechain)
		{
			PPL_GenerateDetailArrays(s);
		}
		PPL_FlushArrays();
	}
}

//requires multitexture
void PPL_BaseTextures(model_t *model)
{
	int		i;
	msurface_t	*s;
	texture_t	*t;

	currententity = &r_worldentity;

	GL_DoSwap();

	currententity->shaderRGBAf[0] = 1;
	currententity->shaderRGBAf[1] = 1;
	currententity->shaderRGBAf[2] = 1;
	currententity->shaderRGBAf[3] = 1;

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglColor4fv(currententity->shaderRGBAf);
//	qglDepthFunc(GL_LESS);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglShadeModel(GL_FLAT);

	overbright = 1;
	if (gl_overbright_all.value || (model->engineflags & MDLF_NEEDOVERBRIGHT))
	{
		if (gl_overbright.value>=2)
			overbright = 4;
		else if (gl_overbright.value)
			overbright = 2;
	}

	currentmodel = model;

	if (model == cl.worldmodel && skytexturenum>=0)
	{
		t = model->textures[skytexturenum];
		if (t)
		{
			s = t->texturechain;
			if (s)
			{
				t->texturechain = NULL;
				GL_DrawSkyChain (s);
			}
		}
	}
	if (mirrortexturenum>=0 && model == cl.worldmodel && r_mirroralpha.value != 1.0)
	{
		t = model->textures[mirrortexturenum];
		if (t)
		{
			s = t->texturechain;
			if (s)
			{
				t->texturechain = NULL;
				if (!r_inmirror)
					R_MirrorChain (s);
			}
		}
	}

	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;

		if ((s->flags & SURF_DRAWTURB) && r_wateralphaval != 1.0)
			continue;	// draw translucent water later

		PPL_BaseTextureChain(s);
	}

	GL_DisableMultitexture();
}

void PPL_BaseBModelTextures(entity_t *e)
{
	extern cvar_t r_drawflat_nonworldmodel;
	extern msurface_t  *r_alpha_surfaces;
	int i, k;
	int shift;
	int sflags;
	model_t *model;
	msurface_t *s;
	msurface_t *chain = NULL;

	qglPushMatrix();
	R_RotateForEntity(e);
	currentmodel = model = e->model;
	s = model->surfaces+model->firstmodelsurface;

	shift = GLR_LightmapShift(currentmodel);

	if (currententity->drawflags & DRF_TRANSLUCENT)
		currententity->shaderRGBAf[3]=0.5;
	if ((currententity->drawflags & MLS_ABSLIGHT) == MLS_ABSLIGHT)
	{
		currententity->shaderRGBAf[0] =
		currententity->shaderRGBAf[1] =
		currententity->shaderRGBAf[2] = currententity->abslight/255.0f;
	}

	if (currententity->shaderRGBAf[3]<1)
	{
		GL_TexEnv(GL_MODULATE);
		qglEnable(GL_BLEND);
	}
	else
	{
		GL_TexEnv(GL_REPLACE);
		qglDisable(GL_BLEND);
	}

	qglColor4fv(currententity->shaderRGBAf);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (model->fromgame != fg_quake3)
	{
		if (currentmodel->nummodelsurfaces != 0 && r_dynamic.value)
		{
			for (k=0 ; k<dlights_software ; k++)
			{
				if (!cl_dlights[k].radius)
					continue;
				if (!(cl_dlights[k].flags & LFLAG_ALLOW_LMHACK))
					continue;

				currentmodel->funcs.MarkLights (&cl_dlights[k], 1<<k,
					currentmodel->nodes + currentmodel->hulls[0].firstclipnode);
			}
		}

//update lightmaps.
		for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
			R_RenderDynamicLightmaps (s, shift);
	}

	if (!r_drawflat_nonworldmodel.value && (!cl.worldmodel->submodels || model->submodels != cl.worldmodel->submodels))
		sflags = SURF_NOFLAT;
	else
		sflags = 0;

	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (s->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
		{
			s->ownerent = currententity;
			s->nextalphasurface = r_alpha_surfaces;
			r_alpha_surfaces = s;
			continue;
		}
		else if (chain && s->texinfo->texture != chain->texinfo->texture)	//last surface or not the same as the next
		{
			PPL_BaseTextureChain(chain);
			chain = NULL;
		}

		s->flags |= sflags;
		s->texturechain = chain;
		chain = s;
	}

	if (chain)
		PPL_BaseTextureChain(chain);

	qglPopMatrix();
	GL_DisableMultitexture();
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	if (!varrayactive)
		R_IBrokeTheArrays();
}

#ifdef Q3SHADERS
void R_DrawLightning(entity_t *e)
{
	vec3_t v;
	vec3_t dir, cr;
	float scale = e->scale;
	float length;

	vec3_t points[4];
	vec2_t texcoords[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
	index_t indexarray[6] = {0, 1, 2, 0, 2, 3};

	mesh_t mesh;
	meshbuffer_t mb;

	if (!e->forcedshader)
		return;

	if (!scale)
		scale = 10;


	VectorSubtract(e->origin, e->oldorigin, dir);
	length = Length(dir);

	//this seems to be about right.
	texcoords[2][0] = length/128;
	texcoords[3][0] = length/128;

	VectorSubtract(r_refdef.vieworg, e->origin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->origin, -scale/2, cr, points[0]);
	VectorMA(e->origin, scale/2, cr, points[1]);

	VectorSubtract(r_refdef.vieworg, e->oldorigin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->oldorigin, scale/2, cr, points[2]);
	VectorMA(e->oldorigin, -scale/2, cr, points[3]);

	mesh.xyz_array = points;
	mesh.indexes = indexarray;
	mesh.numindexes = sizeof(indexarray)/sizeof(indexarray[0]);
	mesh.colors_array = NULL;
	mesh.lmst_array = NULL;
	mesh.normals_array = NULL;
	mesh.numvertexes = 4;
	mesh.st_array = texcoords;

	mb.entity = e;
	mb.mesh = &mesh;
	mb.shader = e->forcedshader;
	mb.infokey = 0;
	mb.fog = NULL;
	mb.infokey = currententity->keynum;
	mb.dlightbits = 0;


	R_IBrokeTheArrays();

	R_PushMesh(&mesh, mb.shader->features | MF_NONBATCHED);

	R_RenderMeshBuffer ( &mb, false );
}
void R_DrawRailCore(entity_t *e)
{
	vec3_t v;
	vec3_t dir, cr;
	float scale = e->scale;
	float length;

	mesh_t mesh;
	meshbuffer_t mb;
	vec3_t points[4];
	vec2_t texcoords[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
	index_t indexarray[6] = {0, 1, 2, 0, 2, 3};
	int colors[4];
	qbyte colorsb[4];

	if (!e->forcedshader)
		return;

	if (!scale)
		scale = 10;


	VectorSubtract(e->origin, e->oldorigin, dir);
	length = Length(dir);

	//this seems to be about right.
	texcoords[2][0] = length/128;
	texcoords[3][0] = length/128;

	VectorSubtract(r_refdef.vieworg, e->origin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->origin, -scale/2, cr, points[0]);
	VectorMA(e->origin, scale/2, cr, points[1]);

	VectorSubtract(r_refdef.vieworg, e->oldorigin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->oldorigin, scale/2, cr, points[2]);
	VectorMA(e->oldorigin, -scale/2, cr, points[3]);

	colorsb[0] = e->shaderRGBAf[0]*255;
	colorsb[1] = e->shaderRGBAf[1]*255;
	colorsb[2] = e->shaderRGBAf[2]*255;
	colorsb[3] = e->shaderRGBAf[3]*255;
	colors[0] = colors[1] = colors[2] = colors[3] = *(int*)colorsb;

	mesh.xyz_array = points;
	mesh.indexes = indexarray;
	mesh.numindexes = sizeof(indexarray)/sizeof(indexarray[0]);
	mesh.colors_array = (byte_vec4_t*)colors;
	mesh.lmst_array = NULL;
	mesh.normals_array = NULL;
	mesh.numvertexes = 4;
	mesh.st_array = texcoords;

	mb.entity = e;
	mb.mesh = &mesh;
	mb.shader = e->forcedshader;
	mb.infokey = 0;
	mb.fog = NULL;
	mb.infokey = currententity->keynum;
	mb.dlightbits = 0;


	R_IBrokeTheArrays();

	R_PushMesh(&mesh, mb.shader->features | MF_NONBATCHED | MF_COLORS);

	R_RenderMeshBuffer ( &mb, false );
}
#endif

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	points[NUM_BEAM_SEGS*2];
	vec3_t oldorigin, origin;
	float scale;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );

	scale = e->scale;
	if (!scale)
		scale = e->framestate.g[FS_REG].frame[0];
	if (!scale)
		scale = 6;
	VectorScale( perpvec, scale / 2, perpvec );

	for ( i = 0; i < 6; i++ )
	{
		RotatePointAroundVector( points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( points[i], origin, points[i] );
		VectorAdd( points[i], direction, points[i+NUM_BEAM_SEGS] );
	}

#ifdef Q3SHADERS
	if (e->forcedshader)
	{
		index_t indexarray[NUM_BEAM_SEGS*6];
		vec2_t texcoords[NUM_BEAM_SEGS*2];
		mesh_t mesh;
		meshbuffer_t mb;

		mesh.xyz_array = points;
		mesh.indexes = indexarray;
		mesh.numindexes = sizeof(indexarray)/sizeof(indexarray[0]);
		mesh.colors_array = NULL;
		mesh.lmst_array = NULL;
		mesh.normals_array = NULL;
		mesh.numvertexes = NUM_BEAM_SEGS*2;
		mesh.st_array = texcoords;

		mb.entity = e;
		mb.mesh = &mesh;
		mb.shader = e->forcedshader;
		mb.infokey = 0;
		mb.fog = NULL;
		mb.infokey = currententity->keynum;
		mb.dlightbits = 0;

		for (i = 0; i < NUM_BEAM_SEGS; i++)
		{
			indexarray[i*6+0] = i+0;
			indexarray[i*6+1] = (i+1)%NUM_BEAM_SEGS;
			indexarray[i*6+2] = indexarray[i*6+1]+NUM_BEAM_SEGS;

			indexarray[i*6+3] = indexarray[i*6+0];
			indexarray[i*6+4] = indexarray[i*6+2];
			indexarray[i*6+5] = i+0+NUM_BEAM_SEGS;

			texcoords[i][1] = (float)i/NUM_BEAM_SEGS+0.35;
			texcoords[i][0] = 0;
			texcoords[i+NUM_BEAM_SEGS][1] = (float)i/NUM_BEAM_SEGS+0.35;
			texcoords[i+NUM_BEAM_SEGS][0] = 1;
		}

		R_IBrokeTheArrays();

		R_PushMesh(&mesh, mb.shader->features | MF_NONBATCHED);

		R_RenderMeshBuffer ( &mb, false );
	}
	else
#endif
	{
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_BLEND );
		qglDepthMask( GL_FALSE );
		qglDisable(GL_ALPHA_TEST);

		r = ( d_8to24rgbtable[e->skinnum & 0xFF] ) & 0xFF;
		g = ( d_8to24rgbtable[e->skinnum & 0xFF] >> 8 ) & 0xFF;
		b = ( d_8to24rgbtable[e->skinnum & 0xFF] >> 16 ) & 0xFF;

		r *= e->shaderRGBAf[0]/255.0F;
		g *= e->shaderRGBAf[1]/255.0F;
		b *= e->shaderRGBAf[2]/255.0F;

		qglColor4f( r, g, b, e->shaderRGBAf[3] );

		qglBegin( GL_TRIANGLE_STRIP );
		for ( i = 0; i < NUM_BEAM_SEGS; i++ )
		{
			qglVertex3fv( points[i] );
			qglVertex3fv( points[i+NUM_BEAM_SEGS] );
			qglVertex3fv( points[((i+1)%NUM_BEAM_SEGS)] );
			qglVertex3fv( points[((i+1)%NUM_BEAM_SEGS)+NUM_BEAM_SEGS] );
		}
		qglEnd();

		qglEnable( GL_TEXTURE_2D );
		qglDisable( GL_BLEND );
		qglDepthMask( GL_TRUE );
	}
}

void PPL_DelayBaseBModelTextures(int count, void **e, void *parm)
{
	while(count--)
	{
		currententity = *e++;

		qglDepthFunc ( gldepthfunc );
		qglEnable(GL_DEPTH_TEST);
		qglDepthMask(1);
		qglEnable(GL_POLYGON_OFFSET_FILL);
		PPL_BaseBModelTextures (currententity);
		qglDisable(GL_POLYGON_OFFSET_FILL);
	}
}

void PPL_BaseEntTextures(void)
{
	extern model_t *currentmodel;
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (!PPL_ShouldDraw())
			continue;

		if (currententity->rtype)
			continue;
		if (currententity->flags & Q2RF_BEAM)
		{
			R_DrawBeam(currententity);
			continue;
		}
		if (!currententity->model)
			continue;

		if (cls.allow_anyparticles || currententity->visframe)	//allowed or static
		{
			if (currententity->model->engineflags & MDLF_ENGULPHS)
			{
				if (gl_part_flame.value)
					continue;
			}
		}

		if (currententity->model->engineflags & MDLF_NOTREPLACEMENTS)
		{
			if (currententity->model->fromgame != fg_quake || currententity->model->type != mod_alias)
				if (!ruleset_allow_sensative_texture_replacements.value)
					continue;
		}


		switch (currententity->model->type)
		{
			//FIXME: We want to depth sort with particles, but we also want depth. :(
			//Until then, we have broken model lighting.
		case mod_alias:
			R_DrawGAliasModel (currententity);
//			if (currententity->flags & Q2RF_WEAPONMODEL)
//				RQ_AddDistReorder(PPL_DrawEnt, currententity, NULL, r_refdef.vieworg);
//			else
//				RQ_AddDistReorder(PPL_DrawEnt, currententity, NULL, currententity->origin);
			break;

		case mod_brush:
			if (currententity->shaderRGBAf[3] < 1)
				RQ_AddDistReorder(PPL_DelayBaseBModelTextures, currententity, NULL, currententity->origin);
			else
			{
				qglDepthFunc ( gldepthfunc );
				qglEnable(GL_DEPTH_TEST);
				qglDepthMask(1);
				qglEnable(GL_POLYGON_OFFSET_FILL);
				PPL_BaseBModelTextures (currententity);
				qglDisable(GL_POLYGON_OFFSET_FILL);
			}
			break;

		default:
			break;
		}
	}

	currentmodel = cl.worldmodel;
}

#ifdef PPL
static void PPL_GenerateLightArrays(msurface_t *surf, vec3_t relativelightorigin, dlight_t *light, vec3_t colour)
{
	int vi;
	float *v, *stw;
	surfvertexarray_t *out;

	vec3_t lightdir;
	float dist;

	shadowlightfaces++;

	v = surf->mesh->xyz_array[0];
	stw = surf->mesh->st_array[0];

	out = &varray_v[0];

	for (vi=0 ; vi<surf->mesh->numvertexes ; vi++, v+=3, stw+=2, out++)
	{
		out->stw[0] = stw[0];
		out->stw[1] = stw[1];
		lightdir[0] = relativelightorigin[0] - v[0];
		lightdir[1] = relativelightorigin[1] - v[1];
		lightdir[2] = relativelightorigin[2] - v[2];
		dist = 1-(sqrt(	(lightdir[0])*(lightdir[0]) +
						(lightdir[1])*(lightdir[1]) +
						(lightdir[2])*(lightdir[2])) / light->radius);
		VectorNormalize(lightdir);
		out->stl[0] = colour[0]*dist;
		out->stl[1] = colour[1]*dist;
		out->stl[2] = colour[2]*dist;
		out->ncm[0] = DotProduct(lightdir, surf->texinfo->vecs[0]);
		out->ncm[1] = -DotProduct(lightdir, surf->texinfo->vecs[1]);
		if (surf->flags & SURF_PLANEBACK)
			out->ncm[2] = -DotProduct(lightdir, surf->plane->normal);
		else
			out->ncm[2] = DotProduct(lightdir, surf->plane->normal);
	}
}

//flags
enum{
PERMUTATION_GENERIC = 0,
PERMUTATION_BUMPMAP = 1,
PERMUTATION_SPECULAR = 2,
PERMUTATION_BUMP_SPEC = 3,
PERMUTATION_OFFSET = 4,
PERMUTATION_OFFSET_BUMP = 5,
PERMUTATION_OFFSET_SPEC = 6,
PERMUTATION_OFFSET_BUMP_SPEC = 7,

PERMUTATIONS
};
int ppl_light_shader[PERMUTATIONS];
int ppl_light_shader_eyeposition[PERMUTATIONS];
int ppl_light_shader_lightposition[PERMUTATIONS];
int ppl_light_shader_lightcolour[PERMUTATIONS];
int ppl_light_shader_lightradius[PERMUTATIONS];
int ppl_light_shader_offset_scale[PERMUTATIONS];
int ppl_light_shader_offset_bias[PERMUTATIONS];

void PPL_CreateLightTexturesProgram(void)
{
	int i;

	char *permutation[PERMUTATIONS] = {
		"",
		"#define BUMP\n",
		"#define SPECULAR\n",
		"#define SPECULAR\n#define BUMP\n",
		"#define USEOFFSETMAPPING\n",
		"#define USEOFFSETMAPPING\n#define BUMP\n",
		"#define USEOFFSETMAPPING\n#define SPECULAR\n",
		"#define USEOFFSETMAPPING\n#define SPECULAR\n#define BUMP\n"
	};
	char *vert = 
		"varying vec2 tcbase;\n"
		"uniform vec3 texr, texu, texf;\n"
		"varying vec3 LightVector;\n"
		"uniform vec3 LightPosition;\n"

		"#if defined(SPECULAR) || defined(USEOFFSETMAPPING)\n"
		"uniform vec3 EyePosition;\n"
		"varying vec3 EyeVector;\n"
		"#endif\n"

		"void main (void)\n"
		"{\n"
		"	gl_Position = ftransform();\n"

		"	tcbase = gl_MultiTexCoord0.xy;\n"	//pass the texture coords straight through

		"	vec3 lightminusvertex = LightPosition - gl_Vertex.xyz;\n"
		"	LightVector.x = dot(lightminusvertex, gl_MultiTexCoord1.xyz);\n"
		"	LightVector.y = dot(lightminusvertex, gl_MultiTexCoord2.xyz);\n"
		"	LightVector.z = dot(lightminusvertex, gl_MultiTexCoord3.xyz);\n"

		"#if defined(SPECULAR)||defined(USEOFFSETMAPPING)\n"
		"	vec3 eyeminusvertex = EyePosition - gl_Vertex.xyz;\n"
		"	EyeVector.x = dot(eyeminusvertex, gl_MultiTexCoord1.xyz);\n"
		"	EyeVector.y = dot(eyeminusvertex, gl_MultiTexCoord2.xyz);\n"
		"	EyeVector.z = dot(eyeminusvertex, gl_MultiTexCoord3.xyz);\n"
		"#endif\n"
		"}\n"
		;

	char *frag =
		"uniform sampler2D baset;\n"
		"#if defined(BUMP) || defined(SPECULAR) || defined(USEOFFSETMAPPING)\n"
		"uniform sampler2D bumpt;\n"
		"#endif\n"
		"#ifdef SPECULAR\n"
		"uniform sampler2D speculart;\n"
		"#endif\n"

		"varying vec2 tcbase;\n"
		"varying vec3 LightVector;\n"

		"uniform float lightradius;\n"
		"uniform vec3 LightColour;\n"

		"#if defined(SPECULAR) || defined(USEOFFSETMAPPING)\n"
		"varying vec3 EyeVector;\n"
		"#endif\n"

"#ifdef USEOFFSETMAPPING\n"
"uniform float OffsetMapping_Scale;\n"
"uniform float OffsetMapping_Bias;\n"
"#endif\n"


		"void main (void)\n"
		"{\n"
"#ifdef USEOFFSETMAPPING\n"
"	// this is 3 sample because of ATI Radeon 9500-9800/X300 limits\n"
"	vec2 OffsetVector = normalize(EyeVector).xy * vec2(-0.333, 0.333);\n"
"	vec2 TexCoordOffset = tcbase + OffsetVector * (OffsetMapping_Bias + OffsetMapping_Scale * texture2D(bumpt, tcbase).w);\n"
"	TexCoordOffset += OffsetVector * (OffsetMapping_Bias + OffsetMapping_Scale * texture2D(bumpt, TexCoordOffset).w);\n"
"	TexCoordOffset += OffsetVector * (OffsetMapping_Bias + OffsetMapping_Scale * texture2D(bumpt, TexCoordOffset).w);\n"
"#define tcbase TexCoordOffset\n"
"#endif\n"


		"#ifdef BUMP\n"
		"	vec3 bases = vec3(texture2D(baset, tcbase));\n"
		"#else\n"
		"	vec3 diff = vec3(texture2D(baset, tcbase));\n"
		"#endif\n"
		"#if defined(BUMP) || defined(SPECULAR)\n"
		"	vec3 bumps = vec3(texture2D(bumpt, tcbase)) * 2.0 - 1.0;\n"
		"#endif\n"
		"#ifdef SPECULAR\n"
		"	vec3 specs = vec3(texture2D(speculart, tcbase));\n"
		"#endif\n"

		"	vec3 nl = normalize(LightVector);\n"
		"	float colorscale = max(1.0 - dot(LightVector, LightVector)/(lightradius*lightradius), 0.0);\n"

		"#ifdef BUMP\n"
		"	vec3 diff;\n"
		"	diff = bases * max(dot(bumps, nl), 0.0);\n"
		"#endif\n"
		"#ifdef SPECULAR\n"
		"	vec3 halfdir = (normalize(EyeVector) + normalize(LightVector))/2.0;\n"
		"	float dv = dot(halfdir, bumps);\n"
		"	diff += pow(dv, 8.0) * specs;\n"
		"#endif\n"
		"	gl_FragColor.rgb = diff*colorscale*LightColour;\n"
		"}\n"
		;

	for (i = 0; i < PERMUTATIONS; i++)
	{
		ppl_light_shader[i] = GLSlang_CreateProgram(permutation[i], vert, frag);

		if (ppl_light_shader[i])
		{
			GLSlang_UseProgram(ppl_light_shader[i]);

			qglUniform1iARB(qglGetUniformLocationARB(ppl_light_shader[i], "baset"), 0);
			qglUniform1iARB(qglGetUniformLocationARB(ppl_light_shader[i], "bumpt"), 1);
			qglUniform1iARB(qglGetUniformLocationARB(ppl_light_shader[i], "speculart"), 2);

			ppl_light_shader_eyeposition[i] = qglGetUniformLocationARB(ppl_light_shader[i], "EyePosition");
			ppl_light_shader_lightposition [i]= qglGetUniformLocationARB(ppl_light_shader[i], "LightPosition");
			ppl_light_shader_lightcolour[i] = qglGetUniformLocationARB(ppl_light_shader[i], "LightColour");
			ppl_light_shader_lightradius[i] = qglGetUniformLocationARB(ppl_light_shader[i], "lightradius");
			ppl_light_shader_offset_scale[i] = qglGetUniformLocationARB(ppl_light_shader[i], "OffsetMapping_Scale");
			ppl_light_shader_offset_bias[i] = qglGetUniformLocationARB(ppl_light_shader[i], "OffsetMapping_Bias");

			GLSlang_UseProgram(0);
		}
	}
};


void PPL_LightTexturesFP_Cached(model_t *model, vec3_t modelorigin, dlight_t *light, vec3_t colour)
{
	int i, j;
	texture_t	*t;
	msurface_t	*s;
	int p, lp=-1;
	extern cvar_t gl_specular;
	shadowmesh_t *shm = light->worldshadowmesh;
	

	vec3_t relativelightorigin;
	vec3_t relativeeyeorigin;

	if (qglGetError())
		Con_Printf("GL Error before lighttextures\n");

	VectorSubtract(light->origin, modelorigin, relativelightorigin);
	VectorSubtract(r_refdef.vieworg, modelorigin, relativeeyeorigin);

	qglEnable(GL_BLEND);
	GL_TexEnv(GL_MODULATE);
	qglBlendFunc(GL_ONE, GL_ONE);
	qglDisableClientState(GL_COLOR_ARRAY);
	if (qglGetError())
		Con_Printf("GL Error early in lighttextures\n");

	for (j=0 ; j<shm->numsurftextures ; j++)
	{
		if (!shm->litsurfs[j].count)
			continue;

		s = shm->litsurfs[j].s[0];
		t = s->texinfo->texture;
		t = R_TextureAnimation (t);

		for (i=0 ; i<shm->litsurfs[j].count ; i++)
		{
			s = shm->litsurfs[j].s[i];

			if (s->visframe != r_framecount)
				continue;

			if (s->flags & SURF_PLANEBACK)
			{//inverted normal.
				if (DotProduct(s->plane->normal, relativelightorigin)-s->plane->dist > lightradius)
					continue;
			}
			else
			{
				if (-DotProduct(s->plane->normal, relativelightorigin)+s->plane->dist > lightradius)
					continue;
			}

	//		if ((s->flags & SURF_DRAWTURB) && r_wateralphaval != 1.0)
	//			continue;	// draw translucent water later



			p = 0;
			if (t->tn.bump && ppl_light_shader[p|PERMUTATION_BUMPMAP])
				p |= PERMUTATION_BUMPMAP;
			if (gl_specular.value && t->tn.specular && ppl_light_shader[p|PERMUTATION_SPECULAR])
				p |= PERMUTATION_SPECULAR;
			if (r_shadow_glsl_offsetmapping.value && t->tn.bump && ppl_light_shader[p|PERMUTATION_OFFSET])
				p |= PERMUTATION_OFFSET;

			if (p != lp)
			{
				lp = p;
				GLSlang_UseProgram(ppl_light_shader[p]);
				if (ppl_light_shader_eyeposition[p] != -1)
					qglUniform3fvARB(ppl_light_shader_eyeposition[p], 1, relativeeyeorigin);
				qglUniform3fvARB(ppl_light_shader_lightposition[p], 1, relativelightorigin);
				qglUniform3fvARB(ppl_light_shader_lightcolour[p], 1, colour);
				qglUniform1fARB(ppl_light_shader_lightradius[p], light->radius);

				if (ppl_light_shader_offset_scale[p]!=-1)
					qglUniform1fARB(ppl_light_shader_offset_scale[p], r_shadow_glsl_offsetmapping_scale.value);
				if (ppl_light_shader_offset_bias[p]!=-1)
					qglUniform1fARB(ppl_light_shader_offset_bias[p], r_shadow_glsl_offsetmapping_bias.value);
			}


			if (p & PERMUTATION_BUMPMAP)
				GL_MBind(GL_TEXTURE1_ARB, t->tn.bump);
			if (p & PERMUTATION_SPECULAR)
				GL_MBind(GL_TEXTURE2_ARB, t->tn.specular);

			GL_MBind(GL_TEXTURE0_ARB, t->tn.base);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);



			qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, s->texinfo->vecs[0][0], s->texinfo->vecs[0][1], s->texinfo->vecs[0][2]);
			qglMultiTexCoord3fARB(GL_TEXTURE2_ARB, -s->texinfo->vecs[1][0], -s->texinfo->vecs[1][1], -s->texinfo->vecs[1][2]);

			if (s->flags & SURF_PLANEBACK)
				qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, -s->plane->normal[0], -s->plane->normal[1], -s->plane->normal[2]);
			else
				qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, s->plane->normal[0], s->plane->normal[1], s->plane->normal[2]);

			qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->st_array);

			qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);
			qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
		}
	}
	GLSlang_UseProgram(0);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (qglGetError())
		Con_Printf("GL Error during lighttextures\n");
}

void PPL_LightTexturesFP(model_t *model, vec3_t modelorigin, dlight_t *light, vec3_t colour)
{
	int i;
	texture_t	*t;
	msurface_t	*s;
	int p, lp=-1;
	extern cvar_t gl_specular;

	vec3_t relativelightorigin;
	vec3_t relativeeyeorigin;

	if (qglGetError())
		Con_Printf("GL Error before lighttextures\n");

	VectorSubtract(light->origin, modelorigin, relativelightorigin);
	VectorSubtract(r_refdef.vieworg, modelorigin, relativeeyeorigin);

	qglEnable(GL_BLEND);
	GL_TexEnv(GL_MODULATE);
	qglBlendFunc(GL_ONE, GL_ONE);
	qglDisableClientState(GL_COLOR_ARRAY);
	if (qglGetError())
		Con_Printf("GL Error early in lighttextures\n");

	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;

//		if ((s->flags & SURF_DRAWTURB) && r_wateralphaval != 1.0)
//			continue;	// draw translucent water later

		t = R_TextureAnimation (t);


		p = 0;
		if (t->tn.bump && ppl_light_shader[p|PERMUTATION_BUMPMAP])
			p |= PERMUTATION_BUMPMAP;
		if (gl_specular.value && t->tn.specular && ppl_light_shader[p|PERMUTATION_SPECULAR])
			p |= PERMUTATION_SPECULAR;
		if (r_shadow_glsl_offsetmapping.value && t->tn.bump && ppl_light_shader[p|PERMUTATION_OFFSET])
			p |= PERMUTATION_OFFSET;

		if (p != lp)
		{
			lp = p;
			GLSlang_UseProgram(ppl_light_shader[p]);
			if (ppl_light_shader_eyeposition[p] != -1)
				qglUniform3fvARB(ppl_light_shader_eyeposition[p], 1, relativeeyeorigin);
			qglUniform3fvARB(ppl_light_shader_lightposition[p], 1, relativelightorigin);
			qglUniform3fvARB(ppl_light_shader_lightcolour[p], 1, colour);
			qglUniform1fARB(ppl_light_shader_lightradius[p], light->radius);

			if (ppl_light_shader_offset_scale[p]!=-1)
				qglUniform1fARB(ppl_light_shader_offset_scale[p], r_shadow_glsl_offsetmapping_scale.value);
			if (ppl_light_shader_offset_bias[p]!=-1)
				qglUniform1fARB(ppl_light_shader_offset_bias[p], r_shadow_glsl_offsetmapping_bias.value);
		}


		if (p & PERMUTATION_BUMPMAP)
			GL_MBind(GL_TEXTURE1_ARB, t->tn.bump);
		if (p & PERMUTATION_SPECULAR)
			GL_MBind(GL_TEXTURE2_ARB, t->tn.specular);

		GL_MBind(GL_TEXTURE0_ARB, t->tn.base);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

		for (; s; s=s->texturechain)
		{
			if (s->shadowframe != r_shadowframe)
				continue;

			if (s->flags & SURF_PLANEBACK)
			{//inverted normal.
				if (DotProduct(s->plane->normal, relativelightorigin)-s->plane->dist > lightradius)
					continue;
			}
			else
			{
				if (-DotProduct(s->plane->normal, relativelightorigin)+s->plane->dist > lightradius)
					continue;
			}

			qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, s->texinfo->vecs[0][0], s->texinfo->vecs[0][1], s->texinfo->vecs[0][2]);
			qglMultiTexCoord3fARB(GL_TEXTURE2_ARB, -s->texinfo->vecs[1][0], -s->texinfo->vecs[1][1], -s->texinfo->vecs[1][2]);

			if (s->flags & SURF_PLANEBACK)
				qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, -s->plane->normal[0], -s->plane->normal[1], -s->plane->normal[2]);
			else
				qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, s->plane->normal[0], s->plane->normal[1], s->plane->normal[2]);

			qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->st_array);

			qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);
			qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
		}

	}
	GLSlang_UseProgram(0);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (qglGetError())
		Con_Printf("GL Error during lighttextures\n");
}

void PPL_LightTextures(model_t *model, vec3_t modelorigin, dlight_t *light, vec3_t colour)
{
	int		i;
	msurface_t	*s;
	texture_t	*t;

	vec3_t relativelightorigin;

	if (ppl_light_shader[0])
	{
		if (model == cl.worldmodel && light->worldshadowmesh)
			PPL_LightTexturesFP_Cached(model, modelorigin, light, colour);
		else
			PPL_LightTexturesFP(model, modelorigin, light, colour);
		return;
	}

	PPL_EnableVertexArrays();

	VectorSubtract(light->origin, modelorigin, relativelightorigin);
	qglShadeModel(GL_SMOOTH);
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;

		if ((s->flags & SURF_DRAWTURB) && r_wateralphaval != 1.0)
			continue;	// draw translucent water later


		{
			extern int normalisationCubeMap;

			t = R_TextureAnimation (t);


			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);
			if (t->tn.bump && gl_mtexarbable>3)
			{
				GL_MBind(GL_TEXTURE0_ARB, t->tn.bump);
				qglEnable(GL_TEXTURE_2D);
				//Set up texture environment to do (tex0 dot tex1)*color
				GL_TexEnv(GL_REPLACE);	//make texture normalmap available.

				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

				GL_SelectTexture(GL_TEXTURE1_ARB);
				GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
				qglEnable(GL_TEXTURE_CUBE_MAP_ARB);
				GL_TexEnv(GL_COMBINE_ARB);	//normalisation cubemap . normalmap
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->ncm);

				GL_MBind(GL_TEXTURE2_ARB, t->tn.bump);	//a dummy
				qglEnable(GL_TEXTURE_2D);
				GL_TexEnv(GL_COMBINE_ARB);	//bumps * color		(the attenuation)
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB); //(doesn't actually use the bound texture)
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);

				GL_MBind(GL_TEXTURE3_ARB, t->tn.base);
				qglEnable(GL_TEXTURE_2D);
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
			}
			else
			{
				if (gl_mtexarbable>3)
				{
					GL_TexEnv(GL_MODULATE);
					qglDisable(GL_TEXTURE_2D);
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

					GL_SelectTexture(GL_TEXTURE2_ARB);
					qglDisable(GL_TEXTURE_2D);
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}
				GL_SelectTexture(GL_TEXTURE1_ARB);
				GL_TexEnv(GL_MODULATE);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);

				GL_SelectTexture(GL_TEXTURE0_ARB);
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
				qglDisable(GL_TEXTURE_2D);
			}

			for (; s; s=s->texturechain)
			{

				if (s->shadowframe != r_shadowframe)
					continue;

				if (s->flags & SURF_PLANEBACK)
				{//inverted normal.
					if (DotProduct(s->plane->normal, relativelightorigin)-s->plane->dist > lightradius)
						continue;
				}
				else
				{
					if (-DotProduct(s->plane->normal, relativelightorigin)+s->plane->dist > lightradius)
						continue;
				}

				PPL_GenerateLightArrays(s, relativelightorigin, light, colour);

				qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);
				qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
				varray_ic = 0;
				varray_vc = 0;

			}
		}
	}

	if (gl_mtexarbable>2)
	{
		GL_TexEnv(GL_MODULATE);
		qglDisable(GL_TEXTURE_2D);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

		GL_SelectTexture(GL_TEXTURE2_ARB);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	GL_TexEnv(GL_MODULATE);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	GL_TexEnv(GL_MODULATE);
	qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);

}

void PPL_LightBModelTexturesFP(entity_t *e, dlight_t *light, vec3_t colour)
{
	int i;
	texture_t	*t;
	msurface_t	*s;
	model_t *model = e->model;
	texture_t *tnum = NULL;
	int p, lp = -1;

	vec3_t relativelightorigin;
	vec3_t relativeeyeorigin;



	if (qglGetError())
		Con_Printf("GL Error before lighttextures\n");
//Fixme: rotate
	VectorSubtract(light->origin, e->origin, relativelightorigin);
	VectorSubtract(r_refdef.vieworg, e->origin, relativeeyeorigin);

	qglEnable(GL_BLEND);
	qglBlendFunc(GL_ONE, GL_ONE);

	if (qglGetError())
		Con_Printf("GL Error early in lighttextures\n");

	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (tnum != s->texinfo->texture)
		{
			tnum = s->texinfo->texture;

			t = R_TextureAnimation (tnum);

			p = 0;
			if (t->tn.bump && ppl_light_shader[p|PERMUTATION_BUMPMAP])
				p |= PERMUTATION_BUMPMAP;
			if (gl_specular.value && t->tn.specular && ppl_light_shader[p|PERMUTATION_SPECULAR])
				p |= PERMUTATION_SPECULAR;
			if (p != lp)
			{
				lp = p;
				GLSlang_UseProgram(ppl_light_shader[p]);
				if (ppl_light_shader_eyeposition[p] != -1)
					qglUniform3fvARB(ppl_light_shader_eyeposition[p], 1, relativeeyeorigin);
				qglUniform3fvARB(ppl_light_shader_lightposition[p], 1, relativelightorigin);
				qglUniform3fvARB(ppl_light_shader_lightcolour[p], 1, colour);
				qglUniform1fARB(ppl_light_shader_lightradius[p], light->radius);
			}

			GL_MBind(GL_TEXTURE0_ARB, t->tn.base);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
			GL_MBind(GL_TEXTURE1_ARB, t->tn.bump);
			GL_MBind(GL_TEXTURE2_ARB, t->tn.specular);
			GL_SelectTexture(GL_TEXTURE0_ARB);
		}

		qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, -s->texinfo->vecs[0][0], -s->texinfo->vecs[0][1], -s->texinfo->vecs[0][2]);
		qglMultiTexCoord3fARB(GL_TEXTURE2_ARB, s->texinfo->vecs[1][0], s->texinfo->vecs[1][1], s->texinfo->vecs[1][2]);

		if (s->flags & SURF_PLANEBACK)
			qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, -s->plane->normal[0], -s->plane->normal[1], -s->plane->normal[2]);
		else
			qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, s->plane->normal[0], s->plane->normal[1], s->plane->normal[2]);

		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->st_array);
		qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);
		qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);


	}
	GLSlang_UseProgram(0);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (qglGetError())
		Con_Printf("GL Error during lighttextures\n");
}

void PPL_LightBModelTextures(entity_t *e, dlight_t *light, vec3_t colour)
{
	int i;
	model_t *model = e->model;

	msurface_t	*s;
	texture_t	*t;

	vec3_t relativelightorigin;

	qglPushMatrix();
	R_RotateForEntity(e);

	if (ppl_light_shader[0])
	{
		PPL_LightBModelTexturesFP(e, light, colour);
		qglPopMatrix();
		return;
	}

	qglColor4f(1, 1, 1, 1);

	PPL_EnableVertexArrays();


		VectorSubtract(light->origin, e->origin, relativelightorigin);
		qglShadeModel(GL_SMOOTH);

		for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
		{
			t = R_TextureAnimation (s->texinfo->texture);

			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);
			if (t->tn.bump && gl_mtexarbable>3)
			{
				GL_MBind(GL_TEXTURE0_ARB, t->tn.bump);
				qglEnable(GL_TEXTURE_2D);
				//Set up texture environment to do (tex0 dot tex1)*color
				GL_TexEnv(GL_REPLACE);	//make texture normalmap available.

				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

				GL_SelectTexture(GL_TEXTURE1_ARB);
				GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
				qglEnable(GL_TEXTURE_CUBE_MAP_ARB);
				GL_TexEnv(GL_COMBINE_ARB);	//normalisation cubemap * normalmap
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->ncm);

				GL_MBind(GL_TEXTURE2_ARB, t->tn.bump);
				qglEnable(GL_TEXTURE_2D);
				GL_TexEnv(GL_COMBINE_ARB);	//bumps * color		(the attenuation)
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB); //(doesn't actually use the bound texture)
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);

				GL_MBind(GL_TEXTURE3_ARB, t->tn.base);
				qglEnable(GL_TEXTURE_2D);
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
			}
			else
			{
				if (gl_mtexarbable>3)
				{
					GL_TexEnv(GL_MODULATE);
					qglDisable(GL_TEXTURE_2D);
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

					GL_SelectTexture(GL_TEXTURE2_ARB);
					qglDisable(GL_TEXTURE_2D);
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}

				GL_TexEnv(GL_MODULATE);
				qglDisable(GL_TEXTURE_2D);
				GL_SelectTexture(GL_TEXTURE1_ARB);
				GL_TexEnv(GL_MODULATE);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
				GL_SelectTexture(GL_TEXTURE0_ARB);


				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
			}

//			for (; s; s=s->texturechain)
			{

//				if (s->shadowframe != r_shadowframe)
//					continue;

			/*	if (fabs(s->center[0] - lightorg[0]) > lightradius+s->radius ||
					fabs(s->center[1] - lightorg[1]) > lightradius+s->radius ||
					fabs(s->center[2] - lightorg[2]) > lightradius+s->radius)
					continue;*/


				if (s->flags & SURF_PLANEBACK)
				{//inverted normal.
					if (-DotProduct(s->plane->normal, relativelightorigin)+s->plane->dist > lightradius)
						continue;
				}
				else
				{
					if (DotProduct(s->plane->normal, relativelightorigin)-s->plane->dist > lightradius)
						continue;
				}
				PPL_GenerateLightArrays(s, relativelightorigin, light, colour);

				qglVertexPointer(3, GL_FLOAT, 0, s->mesh->xyz_array);
				qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_INDEX_TYPE, s->mesh->indexes);
				varray_ic = 0;
				varray_vc = 0;
			}
			PPL_FlushArrays();
		}

	if (gl_mtexarbable>2)
	{
		GL_TexEnv(GL_MODULATE);
		qglDisable(GL_TEXTURE_2D);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

		GL_SelectTexture(GL_TEXTURE2_ARB);
		qglDisable(GL_TEXTURE_2D);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	GL_TexEnv(GL_MODULATE);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(GL_TEXTURE1_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	GL_TexEnv(GL_MODULATE);
	qglDisable(GL_TEXTURE_CUBE_MAP_ARB);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);

	qglPopMatrix();
}

//draw the bumps on the models for each light.
void PPL_DrawEntLighting(dlight_t *light, vec3_t colour)
{
	int i;

	PPL_LightTextures(cl.worldmodel, r_worldentity.origin, light, colour);

	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (!PPL_ShouldDraw())
			continue;
		if (r_inmirror)
		{
			if (currententity->flags & Q2RF_WEAPONMODEL)
				continue;
		}
		else
		{
			if (currententity->keynum == (cl.viewentity[r_refdef.currentplayernum]?cl.viewentity[r_refdef.currentplayernum]:(cl.playernum[r_refdef.currentplayernum]+1)))
				continue;
//			if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
//				continue;
			if (!Cam_DrawPlayer(0, currententity->keynum-1))
				continue;
		}

		if (currententity->flags & Q2RF_BEAM)
		{
			continue;
		}
		if (!currententity->model)
			continue;

		if (currententity->model->engineflags & MDLF_NOTREPLACEMENTS)
		{
			if (currententity->model->fromgame != fg_quake || currententity->model->type != mod_alias)
				if (!ruleset_allow_sensative_texture_replacements.value)
					continue;
		}

		switch (currententity->model->type)
		{
		case mod_alias:
			if (!varrayactive)
				R_IBrokeTheArrays();
			R_DrawGAliasModelLighting (currententity, light->origin, colour, light->radius);
			break;

		case mod_brush:
			qglEnable(GL_POLYGON_OFFSET_FILL);
			PPL_LightBModelTextures (currententity, light, colour);
			qglDisable(GL_POLYGON_OFFSET_FILL);
			break;

		default:
			break;
		}
	}
}
#endif

void PPL_Details(model_t *model)
{
	int		tn;
	msurface_t	*s;
	texture_t	*t;

	qglColor3f(1,1,1);

	qglDepthMask(0);	//don't bother writing depth

	GL_TexEnv(GL_MODULATE);

	qglShadeModel(GL_FLAT);

	qglEnable(GL_BLEND);
	qglEnable(GL_TEXTURE_2D);

	for (tn=0 ; tn<model->numtextures ; tn++)
	{
		t = model->textures[tn];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;

		if ((s->flags & SURF_DRAWTURB) && r_wateralphaval != 1.0)
			continue;	// draw translucent water later

		PPL_DetailTextureChain(s);
	}

	GL_TexEnv(GL_REPLACE);
	qglDepthMask(1);
}

void PPL_DetailsBModelTextures(entity_t *e)
{
	int i;
	model_t *model;
	msurface_t *s;
	msurface_t *chain = NULL;

	qglPushMatrix();
	R_RotateForEntity(e);
	currentmodel = model = e->model;
	s = model->surfaces+model->firstmodelsurface;

	qglColor4f(1, 1, 1, 1);
	qglDepthMask(0);	//don't bother writing depth

	GL_TexEnv(GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglShadeModel(GL_FLAT);

	qglEnable(GL_BLEND);
	qglEnable(GL_TEXTURE_2D);

	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (chain && s->texinfo->texture != chain->texinfo->texture)	//last surface or not the same as the next
		{
			PPL_DetailTextureChain(chain);
			chain = NULL;
		}

		s->texturechain = chain;
		chain = s;
	}

	if (chain)
		PPL_DetailTextureChain(chain);

	qglPopMatrix();
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	qglDepthMask(1);
}

//draw the detail textures over the top
void PPL_DrawEntDetails(void)
{
	int		i;

	currententity = &r_worldentity;
	if (!gl_detail.value)
		return;

	PPL_Details(cl.worldmodel);
	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
			continue;

		if (!Cam_DrawPlayer(0, currententity->keynum-1))
			continue;

		if (currententity->flags & Q2RF_BEAM)
			continue;

		if (!currententity->model)
			continue;

		if (cls.allow_anyparticles || currententity->visframe)	//allowed or static
		{
			if (currententity->model->engineflags & MDLF_ENGULPHS)
			{
				if (gl_part_flame.value)
					continue;
			}
		}

		switch (currententity->model->type)
		{
		case mod_alias:
//			R_DrawGAliasModelLighting (currententity);
			break;

		case mod_brush:
			qglEnable(GL_POLYGON_OFFSET_FILL);
			PPL_DetailsBModelTextures (currententity);
			qglDisable(GL_POLYGON_OFFSET_FILL);
			break;

		default:
			break;
		}
	}
}


void PPL_SchematicsTextureChain(msurface_t *first)
{
	extern int char_texture;
	msurface_t *s;
	float *v1, *v2;
	float len;
	unsigned char str[64];
	int sl, c;
	vec3_t dir;
	vec3_t pos, v;
	const float size = 0.0625;
	float frow, fcol;
	int e, en;

	if (!cl.worldmodel->surfedges)
		return;

	qglEnable(GL_ALPHA_TEST);

	if (qglPolygonOffset)
		qglPolygonOffset(-1, 0);

	frow = rand()/(float)RAND_MAX;
	frow=frow/2+0.5;
	qglColor3f(frow, frow, 0);

	//draw the distances
	if (gl_schematics.value != 2)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);

		qglEnable(GL_TEXTURE_2D);
		GL_Bind(char_texture);

		qglBegin(GL_QUADS);
		for (s = first; s ; s=s->texturechain)
		{
			for (e = s->numedges; e >= 0; e--)
			{
				en = cl.worldmodel->surfedges[e+s->firstedge];
				if (en<0)	//backwards
				{
					en = -en;
					v2 = cl.worldmodel->vertexes[cl.worldmodel->edges[en].v[0]].position;
					v1 = cl.worldmodel->vertexes[cl.worldmodel->edges[en].v[1]].position;
				}
				else
				{
					v1 = cl.worldmodel->vertexes[cl.worldmodel->edges[en].v[0]].position;
					v2 = cl.worldmodel->vertexes[cl.worldmodel->edges[en].v[1]].position;
				}



				VectorSubtract(v1, v2, dir);
				len = Length(dir);
				VectorNormalize(dir);
				sprintf(str, "%i", (len<1)?1:(int)len);
				sl = strlen(str);
				VectorMA(v2, len/2 + sl*4, dir, pos);

				for (c = 0; c < sl; c++)
				{
					frow = (str[c]>>4)*size;
					fcol = (str[c]&15)*size;

					qglTexCoord2f (fcol, frow + size);
					qglVertex3fv(pos);
					if (s->flags & SURF_PLANEBACK)
						VectorMA(pos, -8, s->plane->normal, v);
					else
						VectorMA(pos, 8, s->plane->normal, v);
					qglTexCoord2f (fcol, frow);
					qglVertex3fv(v);
					VectorMA(pos, -8, dir, pos);
					if (s->flags & SURF_PLANEBACK)
						VectorMA(pos, -8, s->plane->normal, v);
					else
						VectorMA(pos, 8, s->plane->normal, v);
					qglTexCoord2f (fcol + size, frow);
					qglVertex3fv(v);
					qglTexCoord2f (fcol + size, frow + size);
					qglVertex3fv(pos);
				}
			}
		}
		qglEnd();

		qglDisable(GL_POLYGON_OFFSET_FILL);
	}

	qglEnable(GL_POLYGON_OFFSET_LINE);

	qglDisable(GL_TEXTURE_2D);

	qglBegin(GL_LINES);
	for (s = first; s ; s=s->texturechain)
	{
		for (e = s->numedges; e >= 0; e--)
		{
			en = cl.worldmodel->surfedges[e+s->firstedge];
			if (en<0)	//backwards
				en = -en;
			v1 = cl.worldmodel->vertexes[cl.worldmodel->edges[en].v[0]].position;
			v2 = cl.worldmodel->vertexes[cl.worldmodel->edges[en].v[1]].position;


			VectorSubtract(v2, v1, dir);
			len = Length(dir);
			VectorNormalize(dir);

			if (gl_schematics.value != 2)
			{
				sprintf(str, "%i", (len<1)?1:(int)len);
				sl = strlen(str);
			}
			else
				sl = 0;

			//left side. (find arrowhead part)
			if (s->flags & SURF_PLANEBACK)
				VectorMA(v1, -4, s->plane->normal, pos);
			else
				VectorMA(v1, 4, s->plane->normal, pos);

			VectorMA(pos, 4, dir, v);
			if (s->flags & SURF_PLANEBACK)
				VectorMA(v, 4, s->plane->normal, v);
			else
				VectorMA(v, -4, s->plane->normal, v);
			qglVertex3fv(v);
			qglVertex3fv(pos);

			if (s->flags & SURF_PLANEBACK)
				VectorMA(v, -8, s->plane->normal, v);
			else
				VectorMA(v, 8, s->plane->normal, v);
			qglVertex3fv(v);
			qglVertex3fv(pos);

			//the line
			qglVertex3fv(pos);
			VectorMA(pos, len/2 - sl*4, dir, pos);
			qglVertex3fv(pos);

			//right hand side.
			if (s->flags & SURF_PLANEBACK)
				VectorMA(v2, -4, s->plane->normal, pos);
			else
				VectorMA(v2, 4, s->plane->normal, pos);

			VectorMA(pos, -4, dir, v);
			if (s->flags & SURF_PLANEBACK)
				VectorMA(v, 4, s->plane->normal, v);
			else
				VectorMA(v, -4, s->plane->normal, v);
			qglVertex3fv(v);
			qglVertex3fv(pos);

			if (s->flags & SURF_PLANEBACK)
				VectorMA(v, -8, s->plane->normal, v);
			else
				VectorMA(v, 8, s->plane->normal, v);
			qglVertex3fv(v);
			qglVertex3fv(pos);

			//the line
			qglVertex3fv(pos);
			VectorMA(pos, -(len/2 - sl*4), dir, pos);
			qglVertex3fv(pos);

		}
	}
	qglEnd();
	qglDisable(GL_POLYGON_OFFSET_LINE);

	qglEnable(GL_TEXTURE_2D);
}

// :)
void PPL_Schematics(void)
{
	int		tn;
	msurface_t	*s;
	texture_t	*t;
	model_t *model;

	qglColor3f(1,1,1);

	qglDepthMask(0);	//don't bother writing depth

	GL_TexEnv(GL_MODULATE);

	qglShadeModel(GL_FLAT);

	qglEnable(GL_BLEND);
	qglDisable(GL_TEXTURE_2D);

	model = cl.worldmodel;
	for (tn=0 ; tn<model->numtextures ; tn++)
	{
		t = model->textures[tn];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;

		PPL_SchematicsTextureChain(s);

		t->texturechain=NULL;
	}

	GL_TexEnv(GL_REPLACE);
	qglDepthMask(1);
}









#ifdef PPL





qboolean PPL_VisOverlaps(qbyte *v1, qbyte *v2)
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
qboolean PPL_LeafInView(qbyte *lightvis)
{
	int i;
	int m = (cl.worldmodel->numleafs+7)/8;
	mleaf_t *wl = cl.worldmodel->leafs;
	unsigned char lv;
	for (i = 0; i < m; i++)
	{
		lv = lightvis[i];
		if (lv&1   && wl[(i<<3)+0].visframe == r_visframecount) return true;
		if (lv&2   && wl[(i<<3)+1].visframe == r_visframecount) return true;
		if (lv&4   && wl[(i<<3)+2].visframe == r_visframecount) return true;
		if (lv&8   && wl[(i<<3)+3].visframe == r_visframecount) return true;
		if (lv&16  && wl[(i<<3)+4].visframe == r_visframecount) return true;
		if (lv&32  && wl[(i<<3)+5].visframe == r_visframecount) return true;
		if (lv&64  && wl[(i<<3)+6].visframe == r_visframecount) return true;
		if (lv&128 && wl[(i<<3)+7].visframe == r_visframecount) return true;
	}

	return false;
}

void PPL_RecursiveWorldNode_r (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	int v;

	float *v1;
	vec3_t v3;

	if (node->shadowframe != r_shadowframe)
		return;

	if (node->contents == Q1CONTENTS_SOLID)
		return;		// solid


	//if light areabox is outside node, ignore node + children
	for (c = 0; c < 3; c++)
	{
		if (lightorg[c] + lightradius < node->minmaxs[c])
			return;
		if (lightorg[c] - lightradius > node->minmaxs[3+c])
			return;
	}
	
// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;
		PPL_Shadow_Cache_Leaf(pleaf);

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark++)->shadowframe = r_shadowframe;
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
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	PPL_RecursiveWorldNode_r (node->children[side]);

// draw stuff
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		{
			for ( ; c ; c--, surf++)
			{
				if (surf->shadowframe != r_shadowframe)
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
					if (-DotProduct(surf->plane->normal, lightorg)+surf->plane->dist >= lightradius)
						continue;
				}
				else
				{
					if (DotProduct(surf->plane->normal, lightorg)-surf->plane->dist >= lightradius)
						continue;
				}

/*				if (fabs(surf->center[0] - lightorg[0]) > lightradius+surf->radius ||
					fabs(surf->center[1] - lightorg[1]) > lightradius+surf->radius ||
					fabs(surf->center[2] - lightorg[2]) > lightradius+surf->radius)
					continue;
*/				

				PPL_Shadow_Cache_Surface(surf);



#define PROJECTION_DISTANCE (float)(lightradius*2)//0x7fffffff

				//build a list of the edges that are to be drawn.
				for (v = 0; v < surf->numedges; v++)
				{
					int e, delta;
					shadowemittedeges++;
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

				shadowsurfcount++;

				qglVertexPointer(3, GL_FLOAT, 0, surf->mesh->xyz_array);
				qglDrawElements(GL_TRIANGLES, surf->mesh->numindexes, GL_INDEX_TYPE, surf->mesh->indexes);

				//fixme:this only works becuse q1bsps don't have combined meshes yet...
					//back (depth precision doesn't matter)
					qglBegin(GL_POLYGON);
					for (v = surf->mesh->numvertexes-1; v >=0; v--)
					{
						v1 = surf->mesh->xyz_array[v];
						v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
						v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
						v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

						qglVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
					}
					qglEnd();
			}
		}
	}

// recurse down the back side
	PPL_RecursiveWorldNode_r (node->children[!side]);
}

//2 changes, but otherwise the same
void PPL_RecursiveWorldNodeQ2_r (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	int v;

	float *v1;
	vec3_t v3;

	if (node->contents == Q2CONTENTS_SOLID)
		return;		// solid

	if (node->shadowframe != r_shadowframe)
		return;
//	if (R_CullBox (node->minmaxs, node->minmaxs+3))
//		return;
	
// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark++)->shadowframe = r_shadowframe;
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
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	PPL_RecursiveWorldNodeQ2_r (node->children[side]);

// draw stuff
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		{
			for ( ; c ; c--, surf++)
			{
				if (surf->shadowframe != r_shadowframe)
					continue;
/*
				if (surf->lightframe == r_shadowframe)	//done this one!
					continue;
				surf->lightframe = r_shadowframe;
*/
//				if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
//					continue;		// wrong side

//				if (surf->flags & SURF_PLANEBACK)
//					continue;

				if (surf->flags & SURF_PLANEBACK)
				{//inverted normal.
					if (DotProduct(surf->plane->normal, lightorg)-surf->plane->dist >= 0)
						continue;
				}
				else
				{
					if (DotProduct(surf->plane->normal, lightorg)-surf->plane->dist <= 0)
						continue;
				}
//#define PROJECTION_DISTANCE (float)0x7fffffff
				if (surf->flags & (SURF_DRAWALPHA | SURF_DRAWTILED))
				{	// no shadows
					continue;
				}

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

				//front face
				qglVertexPointer(3, GL_FLOAT, 0, surf->mesh->xyz_array);
				qglDrawElements(GL_TRIANGLES, surf->mesh->numindexes, GL_INDEX_TYPE, surf->mesh->indexes);

				//fixme:this only works becuse q1bsps don't have combined meshes yet...
				//back (depth precision doesn't matter)
				qglBegin(GL_POLYGON);
				for (v = surf->mesh->numvertexes-1; v >=0; v--)
				{
					v1 = surf->mesh->xyz_array[v];
					v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
					v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
					v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

					qglVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
				}
				qglEnd();

			}
		}
	}

// recurse down the back side
	PPL_RecursiveWorldNodeQ2_r (node->children[!side]);
}

void PPL_RecursiveWorldNodeQ3_r (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
//	glpoly_t *p;
//	int v;

//	float *v2;
//	vec3_t v4;

//	vec3_t v3;

	if (node->contents == Q2CONTENTS_SOLID)
		return;		// solid

	if (node->shadowframe != r_shadowframe)
		return;
//	if (R_CullBox (node->minmaxs, node->minmaxs+3))
//		return;
	
// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				surf = *mark;
				(*mark++)->shadowframe = r_shadowframe;

/*				if (surf->shadowframe != r_shadowframe)
					continue;
*/
//				if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
//					continue;		// wrong side

//				if (surf->flags & SURF_PLANEBACK)
//					continue;

				if (surf->flags & SURF_PLANEBACK)
				{//inverted normal.
					if (DotProduct(surf->plane->normal, lightorg)-surf->plane->dist <= -lightradius)
						continue;
				}
				else
				{
					if (DotProduct(surf->plane->normal, lightorg)-surf->plane->dist >= lightradius)
						continue;
				}
//#define PROJECTION_DISTANCE (float)0x7fffffff
				/*if (surf->flags & (SURF_DRAWALPHA | SURF_DRAWTILED))
				{	// no shadows
					continue;
				}*/


				Sys_Error("PPL_RecursiveWorldNodeQ3_r needs work");
				/*

				for (p = surf->polys; p; p=p->next)
				{
					//front face
					qglVertexPointer(3, GL_FLOAT, sizeof(GLfloat)*VERTEXSIZE, p->verts);
					qglDrawElements(GL_TRIANGLES, (p->numverts-2)*3, GL_INDEX_TYPE, varray_i_polytotri);
//fixme...
					for (v = 0; v < p->numverts; v++)
					{
					//border
						v1 = p->verts[v];
						v2 = p->verts[( v+1 )%p->numverts];

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
							qglVertex3f( v1[0], v1[1], v1[2] );
							qglVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
							qglVertex3f( v2[0], v2[1], v2[2] );
							qglVertex3f( v2[0]+v4[0], v2[1]+v4[1], v2[2]+v4[2] );
						qglEnd();
					}

//back
					qglBegin(GL_POLYGON);
					for (v = p->numverts-1; v >=0; v--)
					{
						v1 = p->verts[v];
						v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
						v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
						v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

						qglVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
					}
					qglEnd();
					
				}
				*/
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
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	PPL_RecursiveWorldNodeQ3_r (node->children[side]);

// draw stuff
/*  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		{
			for ( ; c ; c--, surf++)
			{

			}
		}
	}
*/
// recurse down the back side
	PPL_RecursiveWorldNodeQ3_r (node->children[!side]);
}

void PPL_RecursiveWorldNode (dlight_t *dl)
{
	float *v1, *v2;
	vec3_t v3, v4;

	lightradius = dl->radius;

	lightorg[0] = dl->origin[0]+0.5;
	lightorg[1] = dl->origin[1]+0.5;
	lightorg[2] = dl->origin[2]+0.5;

	modelorg[0] = lightorg[0];
	modelorg[1] = lightorg[1];
	modelorg[2] = lightorg[2];

	if (dl->worldshadowmesh)
	{
		qglEnableClientState(GL_VERTEX_ARRAY);
		qglVertexPointer(3, GL_FLOAT, 0, dl->worldshadowmesh->verts);
		qglDrawRangeElements(GL_TRIANGLES, 0, dl->worldshadowmesh->numverts, dl->worldshadowmesh->numindicies, GL_INDEX_TYPE, dl->worldshadowmesh->indicies);
		return;
	}

	PPL_BeginShadowMesh(dl);


	qglEnableClientState(GL_VERTEX_ARRAY);

	if (qglGetError())
		Con_Printf("GL Error on entities\n");
	if (cl.worldmodel->fromgame == fg_quake3)
		PPL_RecursiveWorldNodeQ3_r(cl.worldmodel->nodes);
	else if (cl.worldmodel->fromgame == fg_quake2)
		PPL_RecursiveWorldNodeQ2_r(cl.worldmodel->nodes);
	else
		PPL_RecursiveWorldNode_r(cl.worldmodel->nodes);
	if (qglGetError())
		Con_Printf("GL Error on entities\n");

	qglVertexPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v[0].xyz);
	if (qglGetError())
		Con_Printf("GL Error on entities\n");
	while(firstedge)
	{
		//border
		v1 = cl.worldmodel->vertexes[cl.worldmodel->edges[firstedge].v[0]].position;
		v2 = cl.worldmodel->vertexes[cl.worldmodel->edges[firstedge].v[1]].position;

		//get positions of v3 and v4 based on the light position
		v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
		v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
		v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

		v4[0] = ( v2[0]-lightorg[0] )*PROJECTION_DISTANCE;
		v4[1] = ( v2[1]-lightorg[1] )*PROJECTION_DISTANCE;
		v4[2] = ( v2[2]-lightorg[2] )*PROJECTION_DISTANCE;

		if (varray_vc + 4>MAXARRAYVERTS)
		{
			qglDrawArrays(GL_QUADS, 0, varray_vc);
			if (qglGetError())
				Con_Printf("GL Error on entities\n");
			varray_vc=0;
		}

		if (edge[firstedge].count > 0)
		{
			varray_v[varray_vc].xyz[0] = v1[0]+v3[0];
			varray_v[varray_vc].xyz[1] = v1[1]+v3[1];
			varray_v[varray_vc].xyz[2] = v1[2]+v3[2];
			varray_vc++;
			varray_v[varray_vc].xyz[0] = v2[0]+v4[0];
			varray_v[varray_vc].xyz[1] = v2[1]+v4[1];
			varray_v[varray_vc].xyz[2] = v2[2]+v4[2];
			varray_vc++;
			varray_v[varray_vc].xyz[0] = v2[0];
			varray_v[varray_vc].xyz[1] = v2[1];
			varray_v[varray_vc].xyz[2] = v2[2];
			varray_vc++;
			varray_v[varray_vc].xyz[0] = v1[0];
			varray_v[varray_vc].xyz[1] = v1[1];
			varray_v[varray_vc].xyz[2] = v1[2];
			varray_vc++;
		}
		else
		{
			varray_v[varray_vc].xyz[0] = v1[0];
			varray_v[varray_vc].xyz[1] = v1[1];
			varray_v[varray_vc].xyz[2] = v1[2];
			varray_vc++;
			varray_v[varray_vc].xyz[0] = v2[0];
			varray_v[varray_vc].xyz[1] = v2[1];
			varray_v[varray_vc].xyz[2] = v2[2];
			varray_vc++;
			varray_v[varray_vc].xyz[0] = v2[0]+v4[0];
			varray_v[varray_vc].xyz[1] = v2[1]+v4[1];
			varray_v[varray_vc].xyz[2] = v2[2]+v4[2];
			varray_vc++;
			varray_v[varray_vc].xyz[0] = v1[0]+v3[0];
			varray_v[varray_vc].xyz[1] = v1[1]+v3[1];
			varray_v[varray_vc].xyz[2] = v1[2]+v3[2];
			varray_vc++;
		}
		edge[firstedge].count=0;

		firstedge = edge[firstedge].next;

		shadowedgecount++;
	}
	qglDrawArrays(GL_QUADS, 0, varray_vc);

	if (qglGetError())
		Con_Printf("GL Error on entities\n");

	varray_vc=0;

	firstedge=0;

	PPL_FinishShadowMesh(dl);
}

void PPL_DrawBrushModelShadow(dlight_t *dl, entity_t *e)
{
	int v;
	float *v1, *v2;
	vec3_t v3, v4;

	int i;
	model_t *model;
	msurface_t *surf;

	RotateLightVector(e->axis, e->origin, dl->origin, lightorg);

	qglPushMatrix();
	R_RotateForEntity(e);
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
//#define PROJECTION_DISTANCE (float)0x7fffffff
		if (surf->flags & (SURF_DRAWALPHA | SURF_DRAWTILED))
		{	// no shadows
			continue;
		}

			//front face
		qglVertexPointer(3, GL_FLOAT, 0, surf->mesh->xyz_array);
		qglDrawArrays(GL_POLYGON, 0, surf->mesh->numvertexes);

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
	qglPopMatrix();
}

void PPL_DrawShadowMeshes(dlight_t *dl)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (r_inmirror)
		{
			if (currententity->flags & Q2RF_WEAPONMODEL)
				continue;
		}
		else
		{
			if (currententity->keynum == dl->key)
				continue;
		}

		if (currententity->flags & Q2RF_BEAM)
		{
			R_DrawBeam(currententity);
			continue;
		}
		if (!currententity->model)
			continue;

		if (cls.allow_anyparticles || currententity->visframe)	//allowed or static
		{
			if (currententity->model->engineflags & MDLF_ENGULPHS)
			{
				if (gl_part_flame.value)
					continue;
			}
		}

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawGAliasShadowVolume (currententity, dl->origin, dl->radius);
			break;

		case mod_brush:
			qglEnable(GL_POLYGON_OFFSET_FILL);
			PPL_DrawBrushModelShadow (dl, currententity);
			qglDisable(GL_POLYGON_OFFSET_FILL);
			break;

		default:
			break;
		}
	}
}

void PPL_UpdateNodeShadowFrames(qbyte	*lvis)
{
	int i;
	mnode_t *node;

	if (!lvis)	//using a cached light, we don't need shadowframes
		return;



#ifdef Q3BSPS
	if (cl.worldmodel->fromgame == fg_quake3)
	{
		mleaf_t	*leaf;
		r_shadowframe++;
		for (i=0, leaf=cl.worldmodel->leafs; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
			node = (mnode_t *)leaf;
			while (node)
			{
				if (node->shadowframe == r_shadowframe)
					break;
				node->shadowframe = r_shadowframe;
				node = node->parent;
			}
		}
	}
	else
#endif
#ifdef Q2BSPS
		 if (cl.worldmodel->fromgame == fg_quake2)
	{
		mleaf_t	*leaf;
		int cluster;
		r_shadowframe++;

		for (i=0, leaf=cl.worldmodel->leafs; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
			cluster = leaf->cluster;
			if (cluster == -1)
				continue;
			if (lvis[cluster>>3] & (1<<(cluster&7)))
			{
				node = (mnode_t *)leaf;
				do
				{
					if (node->shadowframe == r_shadowframe)
						break;
					node->shadowframe = r_shadowframe;
					node = node->parent;
				} while (node);
			}
		}
	}
	else
#endif
	{
		if (r_novis.value != 2)
		{
			r_shadowframe++;

			//variation on mark leaves
			for (i=0 ; i<cl.worldmodel->numleafs ; i++)
			{
				if (lvis[i>>3] & (1<<(i&7)))// && vvis[i>>3] & (1<<(i&7)))
				{
					node = (mnode_t *)&cl.worldmodel->leafs[i+1];
					do
					{
						if (node->shadowframe == r_shadowframe)
							break;
						node->shadowframe = r_shadowframe;
						node = node->parent;
					} while (node);
				}
			}
		}
	}
}

#if 1 //DP's stolen code
static void GL_Scissor (int x, int y, int width, int height)
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
	qglScissor(x, glheight - (y + height),width,height);
}

#define BoxesOverlap(a,b,c,d) ((a)[0] <= (d)[0] && (b)[0] >= (c)[0] && (a)[1] <= (d)[1] && (b)[1] >= (c)[1] && (a)[2] <= (d)[2] && (b)[2] >= (c)[2])
qboolean PPL_ScissorForBox(vec3_t mins, vec3_t maxs)
{
	int i, ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2, x, y, f;
	vec3_t smins, smaxs;
	vec4_t v, v2;
	int r_view_x = 0;
	int r_view_y = 0;
	int r_view_width = glwidth;
	int r_view_height = glheight;
	if (0)//!r_shadow_scissor.integer)
	{
		GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
		return false;
	}
	// if view is inside the box, just say yes it's visible
	if (BoxesOverlap(r_refdef.vieworg, r_refdef.vieworg, mins, maxs))
	{
		GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
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
		GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
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
		for (i = 0;i < 8;i++)
		{
			v[0] = ((i & 1) ? mins[0] : maxs[0]) - r_refdef.vieworg[0];
			v[1] = ((i & 2) ? mins[1] : maxs[1]) - r_refdef.vieworg[1];
			v[2] = ((i & 4) ? mins[2] : maxs[2]) - r_refdef.vieworg[2];
			v2[0] = DotProduct(v, vright);
			v2[1] = DotProduct(v, vup);
			v2[2] = DotProduct(v, vpn);
			if (i)
			{
				if (smins[0] > v2[0]) smins[0] = v2[0];
				if (smaxs[0] < v2[0]) smaxs[0] = v2[0];
				if (smins[1] > v2[1]) smins[1] = v2[1];
				if (smaxs[1] < v2[1]) smaxs[1] = v2[1];
				if (smins[2] > v2[2]) smins[2] = v2[2];
				if (smaxs[2] < v2[2]) smaxs[2] = v2[2];
			}
			else
			{
				smins[0] = smaxs[0] = v2[0];
				smins[1] = smaxs[1] = v2[1];
				smins[2] = smaxs[2] = v2[2];
			}
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
			Matrix4_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, vid.width/vid.height, r_refdef.fov_y);
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
#if 0
		// this code doesn't handle boxes with any points behind view properly
		x1 = 1000;x2 = -1000;
		y1 = 1000;y2 = -1000;
		for (i = 0;i < 8;i++)
		{
			v[0] = (i & 1) ? mins[0] : maxs[0];
			v[1] = (i & 2) ? mins[1] : maxs[1];
			v[2] = (i & 4) ? mins[2] : maxs[2];
			v[3] = 1.0f;
			GL_TransformToScreen(v, v2);
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
#endif

//generates stencil shadows of the world geometry.
//redraws world geometry
qboolean PPL_AddLight(dlight_t *dl)
{
	int i;
	int sdecrw;
	int sincrw;
	int leaf;
	qbyte *lvis;
	qbyte *vvis;
	vec3_t colour;

	qbyte	lvisb[MAX_MAP_LEAFS/8];
	qbyte	vvisb[MAX_MAP_LEAFS/8];

	vec3_t mins;
	vec3_t maxs;

	mins[0] = dl->origin[0] - dl->radius;
	mins[1] = dl->origin[1] - dl->radius;
	mins[2] = dl->origin[2] - dl->radius;

	maxs[0] = dl->origin[0] + dl->radius;
	maxs[1] = dl->origin[1] + dl->radius;
	maxs[2] = dl->origin[2] + dl->radius;

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

	if (colour[0] < 0.1 && colour[1] < 0.1 && colour[2] < 0.1)
		return false;	//just switch these off.

	if (PPL_ScissorForBox(mins, maxs))
		return false;	//was culled.

	if (dl->worldshadowmesh)
	{
		if (!PPL_LeafInView(dl->worldshadowmesh->litleaves))
			return false;
/*
		if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
			i = cl.worldmodel->funcs.LeafForPoint(r_refdef.vieworg, cl.worldmodel);
		else
			i = r_viewleaf - cl.worldmodel->leafs;
		vvis = cl.worldmodel->funcs.LeafPVS(i, cl.worldmodel, vvisb);

	//	if (!(lvis[i>>3] & (1<<(i&7))))	//light might not be visible, but it's effects probably should be.
	//		return;
		if (!PPL_VisOverlaps(dl->worldshadowmesh->litleaves, vvis))	//The two viewing areas do not intersect.
			return;
*/
		lvis = NULL;
	}
	else
	{
		if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
			i = cl.worldmodel->funcs.LeafnumForPoint(cl.worldmodel, r_refdef.vieworg);
		else
			i = r_viewleaf - cl.worldmodel->leafs;

		leaf = cl.worldmodel->funcs.LeafnumForPoint(cl.worldmodel, dl->origin);
		lvis = cl.worldmodel->funcs.LeafPVS(cl.worldmodel, leaf, lvisb, sizeof(lvisb));
		vvis = cl.worldmodel->funcs.LeafPVS(cl.worldmodel, i, vvisb, sizeof(vvisb));

	//	if (!(lvis[i>>3] & (1<<(i&7))))	//light might not be visible, but it's effects probably should be.
	//		return;
		if (!PPL_VisOverlaps(lvis, vvis))	//The two viewing areas do not intersect.
			return false;
	}

	PPL_EnableVertexArrays();

	qglDisable(GL_TEXTURE_2D);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	qglEnable(GL_SCISSOR_TEST);
	if (!((int)r_shadows.value & 4))
	{
		qglDisable(GL_BLEND);
		qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		qglDepthMask(0);

		if (gldepthfunc==GL_LEQUAL)
			qglDepthFunc(GL_LESS);
		else
			qglDepthFunc(GL_GREATER);
		qglEnable(GL_DEPTH_TEST);
		qglEnable(GL_STENCIL_TEST);

		sincrw = GL_INCR;
		sdecrw = GL_DECR;
		if (gl_config.ext_stencil_wrap)
		{	//minamlise damage...
			sincrw = GL_INCR_WRAP_EXT;
			sdecrw = GL_DECR_WRAP_EXT;
		}
	//our stencil writes.

	#ifdef _DEBUG
		if (r_shadows.value == 666)	//testing (visible shadow volumes)
		{
			PPL_UpdateNodeShadowFrames(lvis);

			if (qglGetError())
				Con_Printf("GL Error on entities\n");
			qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
			qglColor3f(dl->color[0], dl->color[1], dl->color[2]);
			qglDisable(GL_STENCIL_TEST);
			qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			if (qglGetError())
				Con_Printf("GL Error on entities\n");
			PPL_RecursiveWorldNode(dl);
			if (qglGetError())
				Con_Printf("GL Error on entities\n");
			PPL_DrawShadowMeshes(dl);
			if (qglGetError())
				Con_Printf("GL Error on entities\n");
			qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		else
	#endif
			
		if (qglStencilOpSeparateATI && !((int)r_shadows.value & 2))//GL_ATI_separate_stencil
		{
			qglClearStencil(0);
			qglClear(GL_STENCIL_BUFFER_BIT);
			qglDisable(GL_CULL_FACE);

			qglStencilFunc( GL_ALWAYS, 1, ~0 );

			qglStencilOpSeparateATI(GL_BACK, GL_KEEP, sincrw, GL_KEEP);
			qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, sdecrw, GL_KEEP);

			PPL_UpdateNodeShadowFrames(lvis);
			PPL_RecursiveWorldNode(dl);
			PPL_DrawShadowMeshes(dl);
			qglStencilOpSeparateATI(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_KEEP);

			qglEnable(GL_CULL_FACE);

			qglStencilFunc( GL_EQUAL, 0, ~0 );
		}
		else if (qglActiveStencilFaceEXT && !((int)r_shadows.value & 2))	//NVidias variation on a theme. (GFFX class)
		{
			qglClearStencil(0);
			qglClear(GL_STENCIL_BUFFER_BIT);
			qglDisable(GL_CULL_FACE);

			qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

			qglActiveStencilFaceEXT(GL_BACK);
			qglStencilOp(GL_KEEP, sincrw, GL_KEEP);
			qglStencilFunc( GL_ALWAYS, 1, ~0 );

			qglActiveStencilFaceEXT(GL_FRONT);
			qglStencilOp(GL_KEEP, sdecrw, GL_KEEP);
			qglStencilFunc( GL_ALWAYS, 1, ~0 );

			PPL_UpdateNodeShadowFrames(lvis);
			PPL_RecursiveWorldNode(dl);
			PPL_DrawShadowMeshes(dl);

			qglActiveStencilFaceEXT(GL_BACK);
			qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

			qglActiveStencilFaceEXT(GL_FRONT);
			qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

			qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);

			qglEnable(GL_CULL_FACE);

			qglActiveStencilFaceEXT(GL_BACK);
			qglStencilFunc( GL_ALWAYS, 0, ~0 );
			qglActiveStencilFaceEXT(GL_FRONT);
			qglStencilFunc( GL_EQUAL, 0, ~0 );
		}
		else //your graphics card sucks and lacks efficient stencil shadow techniques.
		{	//centered around 0. Will only be increased then decreased less.
			qglClearStencil(0);
			qglClear(GL_STENCIL_BUFFER_BIT);

			qglEnable(GL_CULL_FACE);

			qglStencilFunc( GL_ALWAYS, 0, ~0 );

			shadowsurfcount = 0;
			qglCullFace(GL_BACK);
			qglStencilOp(GL_KEEP, sincrw, GL_KEEP);
			PPL_UpdateNodeShadowFrames(lvis);
			PPL_RecursiveWorldNode(dl);
			PPL_DrawShadowMeshes(dl);

			shadowsurfcount=0;
			qglCullFace(GL_FRONT);
			qglStencilOp(GL_KEEP, sdecrw, GL_KEEP);
			PPL_UpdateNodeShadowFrames(lvis);
			PPL_RecursiveWorldNode(dl);
			PPL_DrawShadowMeshes(dl);

			qglStencilFunc( GL_EQUAL, 0, ~0 );
		}
	//end stencil writing.

		qglEnable(GL_DEPTH_TEST);
		qglDepthMask(0);
		qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		qglCullFace(GL_FRONT);

#if 0	//draw the stencil stuff to the red channel
/*
		{
#pragma comment(lib, "opengl32.lib")
			static char buffer[1024*1024*8];
			glReadPixels(0, 0, vid.width, vid.height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, buffer);
			glDrawPixels(vid.width, vid.height, GL_GREEN, GL_UNSIGNED_BYTE, buffer);
		}
*/

		qglMatrixMode(GL_PROJECTION);
		qglPushMatrix();
		qglMatrixMode(GL_MODELVIEW);
		qglPushMatrix();
		GL_Set2D();

		{
			extern cvar_t vid_conheight;

			qglColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
			qglStencilFunc(GL_GREATER, 1, ~0);
			Draw_ConsoleBackground(vid_conheight.value);

			qglColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
			qglStencilFunc(GL_LESS, 1, ~0);
			Draw_ConsoleBackground(vid_conheight.value);

			qglColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
			qglStencilFunc(GL_NEVER, 1, ~0);
			Draw_ConsoleBackground(vid_conheight.value);
		}

		qglMatrixMode(GL_PROJECTION);
		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
		qglPopMatrix();
#endif

		qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	}
	qglColor3f(1,1,1);

	qglEnable(GL_BLEND);
	qglBlendFunc(GL_ONE, GL_ONE);
	qglColor4f(dl->color[0], dl->color[1], dl->color[2], 1);
	qglDepthFunc(GL_EQUAL);

	lightorg[0] = dl->origin[0]+0.5;
	lightorg[1] = dl->origin[1]+0.5;
	lightorg[2] = dl->origin[2]+0.5;

	PPL_DrawEntLighting(dl, colour);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask(1);
	qglDepthFunc(gldepthfunc);
	qglEnable(GL_DEPTH_TEST);

	qglDisable(GL_STENCIL_TEST);
	qglStencilFunc( GL_ALWAYS, 0, ~0 );

	qglDisable(GL_SCISSOR_TEST);
	qglDisable(GL_BLEND);
	GL_TexEnv(GL_REPLACE);

	return true;
}

#endif

void GL_CheckTMUIs0(void);

void PPL_DrawWorld (void)
{
	RSpeedLocals();
#if 0
	dlight_t *lc, *furthestprev;
	float furthest;
#endif
#ifdef PPL
	dlight_t *l;
	int i;
	int numlights;

	vec3_t mins, maxs;
#endif

	int maxshadowlights = gl_maxshadowlights.value;

	if (!r_shadow_realtime_world.value)
		r_lightmapintensity = 1;
	else
		r_lightmapintensity = r_shadow_realtime_world_lightmaps.value;

/*
	if (!lightmap)
	{
		R_PreNewMap();
		R_NewMap();
		return;	// :/
	}
	*/

	if (maxshadowlights < 1)
		maxshadowlights = 1;
//	if (qglGetError())
//		Con_Printf("GL Error before world\n");
//glColorMask(0,0,0,0);

	RSpeedRemark();
	TRACE(("dbg: calling PPL_BaseTextures\n"));
	PPL_BaseTextures(cl.worldmodel);
	RSpeedEnd(RSPEED_WORLD);

//	if (qglGetError())
//		Con_Printf("GL Error during base textures\n");
//glColorMask(1,1,1,1);

	RSpeedRemark();
	TRACE(("dbg: calling PPL_BaseEntTextures\n"));
	PPL_BaseEntTextures();
	RSpeedEnd(RSPEED_DRAWENTITIES);

//	CL_NewDlightRGB(1, r_refdef.vieworg[0], r_refdef.vieworg[1]-16, r_refdef.vieworg[2]-24, 128, 1, 1, 1, 1);

//	if (qglGetError())
//		Con_Printf("GL Error on entities\n");

#ifdef PPL
	numlights = 0;
	RSpeedRemark();
	if (r_shadows.value && qglStencilFunc && gl_canstencil)
	{
		if (cl.worldmodel->fromgame == fg_quake || cl.worldmodel->fromgame == fg_halflife || cl.worldmodel->fromgame == fg_quake2 /*|| cl.worldmodel->fromgame == fg_quake3*/)
		{
//			lc = NULL;
			for (l = cl_dlights, i=0 ; i<dlights_running ; i++, l++)
			{
				if (!l->radius || !(l->flags & LFLAG_ALLOW_PPL))
					continue;
				if (l->color[0]<0)
					continue;	//quick check for darklight

				if (l->flags & LFLAG_REALTIMEMODE)
				{
					if (!r_shadow_realtime_world.value)
						continue;
				}

				mins[0] = l->origin[0] - l->radius;
				mins[1] = l->origin[1] - l->radius;
				mins[2] = l->origin[2] - l->radius;
				maxs[0] = l->origin[0] + l->radius;
				maxs[1] = l->origin[1] + l->radius;
				maxs[2] = l->origin[2] + l->radius;
				if (R_CullBox(mins, maxs))
					continue;
				if (R_CullSphere(l->origin, l->radius))
					continue;

#if 1
				if (maxshadowlights-- <= 0)
					continue;
#else
				VectorSubtract(l->origin, r_refdef.vieworg, mins)
				l->dist = Length(mins);
				VectorNormalize(mins);
				l->dist*=1-sqrt(DotProduct(vpn, mins)*DotProduct(vpn, mins));

				l->next = lc;
				lc = l;
				maxshadowlights--;
			}
			while (maxshadowlights<0)//ooer... we exceeded our quota... strip the furthest ones out.
			{
				furthest = lc->dist;
				furthestprev=NULL;
				for (l = lc; l->next; l = l->next)
				{
					if (l->next->dist > furthest)
					{
						furthest = l->next->dist;
						furthestprev = l;
					}
				}
				if (furthestprev)
					furthestprev->next = furthestprev->next->next;
				else
					lc = lc->next;

				maxshadowlights++;
			}

			for (l = lc; l; l = l->next)	//we now have our quotaed list
			{
#endif
				TRACE(("dbg: calling PPL_AddLight\n"));
				if (PPL_AddLight(l))
					numlights++;
			}
			qglEnable(GL_TEXTURE_2D);
		
		}

		qglDisableClientState(GL_COLOR_ARRAY);
	}
	RSpeedEnd(RSPEED_STENCILSHADOWS);
#endif
//	Con_Printf("%i lights\n", numlights);

//	if (qglGetError())
//		Con_Printf("GL Error on shadow lighting\n");

	RSpeedRemark();

	if (gl_schematics.value)
		PPL_Schematics();

	TRACE(("dbg: calling PPL_DrawEntFullBrights\n"));
	PPL_DrawEntDetails();

	RSpeedEnd(RSPEED_FULLBRIGHTS);

//	if (qglGetError())
//		Con_Printf("GL Error on fullbrights/details\n");

//	Con_Printf("%i %i(%i) %i\n", shadowsurfcount, shadowedgecount, shadowemittedeges, shadowlightfaces);

	RQuantAdd(RQUANT_SHADOWFACES, shadowsurfcount);
	RQuantAdd(RQUANT_SHADOWEDGES, shadowedgecount);
	RQuantAdd(RQUANT_LITFACES, shadowlightfaces);

	shadowsurfcount	= 0;
	shadowedgecount = 0;
	shadowlightfaces = 0;
	shadowemittedeges = 0;

	GL_CheckTMUIs0();

	R_IBrokeTheArrays();
}

void PPL_CreateShaderObjects(void)
{
#ifdef PPL
	PPL_CreateLightTexturesProgram();
#endif
	PPL_LoadSpecularFragmentProgram();
}

void PPL_FlushShadowMesh(dlight_t *dl)
{
	int tn;
	shadowmesh_t *sm;
	sm = dl->worldshadowmesh;
	if (sm)
	{
		dl->worldshadowmesh = NULL;
		for (tn = 0; tn < sm->numsurftextures; tn++)
			if (sm->litsurfs[tn].count)
				BZ_Free(sm->litsurfs);
		BZ_Free(sm->indicies);
		BZ_Free(sm->verts);
		BZ_Free(sm);
	}
}

//okay, so this is a bit of a hack...
qboolean buildingmesh;
void (APIENTRY *realBegin) (GLenum);
void (APIENTRY *realEnd) (void);
void (APIENTRY *realVertex3f) (GLfloat x, GLfloat y, GLfloat z);
void (APIENTRY *realVertex3fv) (const GLfloat *v);
void (APIENTRY *realVertexPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void (APIENTRY *realDrawArrays) (GLenum mode, GLint first, GLsizei count);
void (APIENTRY *realDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);

#define inc 128
int sh_type;
int sh_index[64*64];
int sh_vertnum;		//vertex number (set to 0 at SH_Begin)
int sh_maxverts;
int sh_numverts;	//total emitted
int sh_maxindicies;
int sh_numindicies;
const float *sh_vertexpointer;
int sh_vpstride;
shadowmesh_t *sh_shmesh;
void APIENTRY SH_Begin (GLenum e)
{
	sh_type = e;
}
void APIENTRY SH_End (void)
{
	int i;
	int v1, v2;
	switch(sh_type)
	{
	case GL_POLYGON:
		i = (sh_numindicies+(sh_vertnum-2)*3+inc+5)&~(inc-1);	//and a bit of padding
		if (sh_maxindicies != i)
		{
			sh_maxindicies = i;
			sh_shmesh->indicies = BZ_Realloc(sh_shmesh->indicies, i * sizeof(*sh_shmesh->indicies));
		}
		//decompose the poly into a triangle fan.
		v1 = sh_index[0];
		v2 = sh_index[1];
		for (i = 2; i < sh_vertnum; i++)
		{
			sh_shmesh->indicies[sh_numindicies++] = v1;
			sh_shmesh->indicies[sh_numindicies++] = v2;
			sh_shmesh->indicies[sh_numindicies++] = v2 = sh_index[i];
		}
		sh_vertnum = 0;
		break;
	case GL_TRIANGLES:
		i = (sh_numindicies+(sh_vertnum)+inc+5)&~(inc-1);	//and a bit of padding
		if (sh_maxindicies != i)
		{
			sh_maxindicies = i;
			sh_shmesh->indicies = BZ_Realloc(sh_shmesh->indicies, i * sizeof(*sh_shmesh->indicies));
		}
		//add the extra triangles
		for (i = 0; i < sh_vertnum; i+=3)
		{
			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+0];
			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+1];
			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+2];
		}
		sh_vertnum = 0;
		break;
	case GL_QUADS:
		i = (sh_numindicies+(sh_vertnum/4)*6+inc+5)&~(inc-1);	//and a bit of padding
		if (sh_maxindicies != i)
		{
			sh_maxindicies = i;
			sh_shmesh->indicies = BZ_Realloc(sh_shmesh->indicies, i * sizeof(*sh_shmesh->indicies)); 
		}
		//add the extra triangles
		for (i = 0; i < sh_vertnum; i+=4)
		{
			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+0];
			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+1];
			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+2];

			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+0];
			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+2];
			sh_shmesh->indicies[sh_numindicies++] = sh_index[i+3];
		}
		sh_vertnum = 0;
		break;
	default:
		if (sh_vertnum)
			Sys_Error("SH_End: verticies were left");
	}
}
void APIENTRY SH_Vertex3f (GLfloat x, GLfloat y, GLfloat z)
{
	int i;
	if (sh_vertnum > sizeof(sh_index)/sizeof(sh_index[0]))
		Sys_Error("SH_End: too many verticies");

//add the verts as we go
	i = (sh_numverts+inc+5)&~(inc-1);	//and a bit of padding
	if (sh_maxverts != i)
	{
		sh_maxverts = i;
		sh_shmesh->verts = BZ_Realloc(sh_shmesh->verts, i * sizeof(*sh_shmesh->verts));
	}

	sh_shmesh->verts[sh_numverts][0] = x;
	sh_shmesh->verts[sh_numverts][1] = y;
	sh_shmesh->verts[sh_numverts][2] = z;

	sh_index[sh_vertnum] = sh_numverts;
	sh_vertnum++;
	sh_numverts++;

	switch(sh_type)
	{
	case GL_POLYGON:
		break;
	case GL_TRIANGLES:
		if (sh_vertnum == 3)
			SH_End();
		break;
	case GL_QUADS:
		if (sh_vertnum == 4)
			SH_End();
		break;
	default:
		Sys_Error("SH_Vertex3f: bad type");
	}
}
void APIENTRY SH_Vertex3fv (const GLfloat *v)
{
	SH_Vertex3f(v[0], v[1], v[2]);
}
void APIENTRY SH_VertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	sh_vertexpointer = pointer;
	sh_vpstride = stride/4;
	if (!sh_vpstride)
		sh_vpstride = 3;
}
void APIENTRY SH_DrawArrays (GLenum mode, GLint first, GLsizei count)
{
	int i;
	SH_Begin(mode);
	count+=first;
	for (i = first; i < count; i++)
		SH_Vertex3fv(sh_vertexpointer + i*sh_vpstride);
	SH_End();
}
void APIENTRY SH_DrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	int i;
	SH_Begin(mode);
	for (i = 0; i < count; i++)
		SH_Vertex3fv(sh_vertexpointer + (((int*)indices)[i])*sh_vpstride);
	SH_End();
}

void PPL_Shadow_Cache_Surface(msurface_t *surf)
{
	int i;
	if (!buildingmesh)
		return;

	for (i = 0; i < cl.worldmodel->numtextures; i++)
		if (surf->texinfo->texture == cl.worldmodel->textures[i])
			break;

	sh_shmesh->litsurfs[i].s = BZ_Realloc(sh_shmesh->litsurfs[i].s, sizeof(void*)*(sh_shmesh->litsurfs[i].count+1));
	sh_shmesh->litsurfs[i].s[sh_shmesh->litsurfs[i].count] = surf;
	sh_shmesh->litsurfs[i].count++;
}

void PPL_Shadow_Cache_Leaf(mleaf_t *leaf)
{
	int i;
	if (!buildingmesh)
		return;

	i = leaf - cl.worldmodel->leafs;
	sh_shmesh->litleaves[i>>3] = 1<<(i&7);
}

void PPL_BeginShadowMesh(dlight_t *dl)
{
	PPL_FlushShadowMesh(dl);

	if (buildingmesh)
		return;
	if (dl->die)
		return;

	sh_maxverts = 0;
	sh_numverts = 0;
	sh_vertnum = 0;
	sh_maxindicies = 0;
	sh_numindicies = 0;

	buildingmesh = true;
	realBegin			= qglBegin;
	realEnd				= qglEnd;
	realVertex3f		= qglVertex3f;
	realVertex3fv		= qglVertex3fv;
	realVertexPointer	= qglVertexPointer;
	realDrawArrays		= qglDrawArrays;
	realDrawElements	= qglDrawElements;

	qglBegin			= SH_Begin;
	qglEnd				= SH_End;
	qglVertex3f			= SH_Vertex3f;
	qglVertex3fv		= SH_Vertex3fv;
	qglVertexPointer	= SH_VertexPointer;
	qglDrawArrays		= SH_DrawArrays;
	qglDrawElements		= SH_DrawElements;

	sh_shmesh = Z_Malloc(sizeof(*sh_shmesh) + (cl.worldmodel->numleafs+7)/8);
	sh_shmesh->litsurfs = Z_Malloc(sizeof(shadowmeshsurfs_t)*cl.worldmodel->numtextures);
	sh_shmesh->numsurftextures=cl.worldmodel->numtextures;

	sh_shmesh->litleaves = (unsigned char*)(sh_shmesh+1);
}
void PPL_FinishShadowMesh(dlight_t *dl)
{
	if (!buildingmesh)
		return;

	qglBegin			= realBegin;
	qglEnd				= realEnd;
	qglVertex3f			= realVertex3f;
	qglVertex3fv		= realVertex3fv;
	qglVertexPointer	= realVertexPointer;
	qglDrawArrays		= realDrawArrays;
	qglDrawElements		= realDrawElements;
	buildingmesh		= false;

	dl->worldshadowmesh = sh_shmesh;
	sh_shmesh->numindicies = sh_numindicies;
	sh_shmesh->numverts = sh_numverts;

	sh_shmesh = NULL;
}

#endif	//ifdef GLQUAKE
#endif

