#include "quakedef.h"
#ifdef RGLQUAKE
#include "glquake.h"

//these are shared with gl_rsurf - move to header
void R_MirrorChain (msurface_t *s);
void GL_SelectTexture (GLenum target);
void R_RenderDynamicLightmaps (msurface_t *fa);
void R_BlendLightmaps (void);

extern int gldepthfunc;
extern int		*lightmap_textures;
extern int		lightmap_bytes;		// 1, 2, or 4

extern cvar_t		gl_detail;
extern cvar_t		r_fb_bmodels;
extern cvar_t		gl_part_flame;

extern cvar_t		gl_part_flame;
extern cvar_t		gl_maxshadowlights;
extern int		detailtexture;
//end header confict

extern lightmapinfo_t **lightmap;

extern model_t *currentmodel;
extern int		*deluxmap_textures;

extern int normalisationCubeMap;

int r_shadowframe;

int shadowsurfcount;
int shadowedgecount;
int shadowlightfaces;

int ppl_specular_fragmentprogram;

//#define glBegin glEnd


#define	Q2RF_WEAPONMODEL		4		// only draw through eyes

struct {
	short count;
	short count2;
	short next;
	short prev;
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

#define MAXARRAYVERTS	512
static surfvertexarray_t varray_v[MAXARRAYVERTS];
static unsigned int varray_i[MAXARRAYVERTS];
static unsigned int varray_i_forward[MAXARRAYVERTS];
static unsigned int varray_i_polytotri[MAXARRAYVERTS];	//012 023 034 045...
int varray_ic;
int varray_vc;

#define inline static

inline void PPL_EnableVertexArrays(void)
{
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->xyz);
}
inline void PPL_FlushArrays(void)
{
	if (varray_ic)
		glDrawElements(GL_TRIANGLES, varray_ic, GL_UNSIGNED_INT, varray_i);
	varray_ic = 0;
	varray_vc = 0;
}
static void PPL_GenerateArrays(msurface_t *surf)
{
	glpoly_t *p;
	int vi;
	int vc_s;
	float *v;

	for (p = surf->polys; p; p=p->next)
	{
		if (varray_ic + p->numverts*3>MAXARRAYVERTS)
		{
			PPL_FlushArrays();
		}

		vc_s = varray_vc;
		v = p->verts[0];

		varray_v[varray_vc].xyz[0] = v[0];
		varray_v[varray_vc].xyz[1] = v[1];
		varray_v[varray_vc].xyz[2] = v[2];
		varray_v[varray_vc].stw[0] = v[3];
		varray_v[varray_vc].stw[1] = v[4];
		varray_v[varray_vc].stl[0] = v[5];
		varray_v[varray_vc].stl[1] = v[6];
		varray_vc++;
		v += VERTEXSIZE;
		varray_v[varray_vc].xyz[0] = v[0];
		varray_v[varray_vc].xyz[1] = v[1];
		varray_v[varray_vc].xyz[2] = v[2];
		varray_v[varray_vc].stw[0] = v[3];
		varray_v[varray_vc].stw[1] = v[4];
		varray_v[varray_vc].stl[0] = v[5];
		varray_v[varray_vc].stl[1] = v[6];
		varray_vc++;
		v += VERTEXSIZE;
		for (vi=2 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
		{
			varray_i[varray_ic] = vc_s;
			varray_i[varray_ic+1] = varray_vc-1;
			varray_i[varray_ic+2] = varray_vc;
			varray_ic+=3;

			varray_v[varray_vc].xyz[0] = v[0];
			varray_v[varray_vc].xyz[1] = v[1];
			varray_v[varray_vc].xyz[2] = v[2];
			varray_v[varray_vc].stw[0] = v[3];
			varray_v[varray_vc].stw[1] = v[4];
			varray_v[varray_vc].stl[0] = v[5];
			varray_v[varray_vc].stl[1] = v[6];
			varray_vc++;
		}
	}
}
#ifdef SPECULAR
//same as above, but also generates cubemap texture coords for light reflection (based on blinn's formula)
static void PPL_GenerateArraysBlinnCubeMap(msurface_t *surf)
{
	glpoly_t *p;
	int vi;
	int vc_s;
	float *v;

	vec3_t eye, halfdir;

	for (p = surf->polys; p; p=p->next)
	{
		if (varray_ic + p->numverts*3>MAXARRAYVERTS)
		{
			PPL_FlushArrays();
		}

		vc_s = varray_vc;
		v = p->verts[0];

		varray_v[varray_vc].xyz[0] = v[0];
		varray_v[varray_vc].xyz[1] = v[1];
		varray_v[varray_vc].xyz[2] = v[2];
		varray_v[varray_vc].stw[0] = v[3];
		varray_v[varray_vc].stw[1] = v[4];
		varray_v[varray_vc].stl[0] = v[5];
		varray_v[varray_vc].stl[1] = v[6];
		VectorSubtract(cl.simorg[0], v, eye);
		VectorNormalize(eye);
		VectorAdd(eye, (v+7), halfdir);
//		VectorCopy(eye, halfdir);
		varray_v[varray_vc].ncm[0] = DotProduct(surf->texinfo->vecs[0], halfdir);
		varray_v[varray_vc].ncm[1] = DotProduct(surf->texinfo->vecs[1], halfdir);
		if (surf->flags & SURF_PLANEBACK)
			varray_v[varray_vc].ncm[2] = -DotProduct(surf->plane->normal, halfdir);
		else
			varray_v[varray_vc].ncm[2] = DotProduct(surf->plane->normal, halfdir);
		varray_vc++;
		v += VERTEXSIZE;
		varray_v[varray_vc].xyz[0] = v[0];
		varray_v[varray_vc].xyz[1] = v[1];
		varray_v[varray_vc].xyz[2] = v[2];
		varray_v[varray_vc].stw[0] = v[3];
		varray_v[varray_vc].stw[1] = v[4];
		varray_v[varray_vc].stl[0] = v[5];
		varray_v[varray_vc].stl[1] = v[6];
		VectorSubtract(r_refdef.vieworg, v, eye);
		VectorNormalize(eye);
		VectorAdd(eye, (v+7), halfdir);
		varray_v[varray_vc].ncm[0] = DotProduct(surf->texinfo->vecs[0], halfdir);
		varray_v[varray_vc].ncm[1] = DotProduct(surf->texinfo->vecs[1], halfdir);
		if (surf->flags & SURF_PLANEBACK)
			varray_v[varray_vc].ncm[2] = -DotProduct(surf->plane->normal, halfdir);
		else
			varray_v[varray_vc].ncm[2] = DotProduct(surf->plane->normal, halfdir);
		varray_vc++;
		v += VERTEXSIZE;
		for (vi=2 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
		{
			varray_i[varray_ic] = vc_s;
			varray_i[varray_ic+1] = varray_vc-1;
			varray_i[varray_ic+2] = varray_vc;
			varray_ic+=3;

			varray_v[varray_vc].xyz[0] = v[0];
			varray_v[varray_vc].xyz[1] = v[1];
			varray_v[varray_vc].xyz[2] = v[2];
			varray_v[varray_vc].stw[0] = v[3];
			varray_v[varray_vc].stw[1] = v[4];
			varray_v[varray_vc].stl[0] = v[5];
			varray_v[varray_vc].stl[1] = v[6];
			VectorSubtract(cl.simorg[0], v, eye);
			VectorNormalize(eye);
			VectorAdd(eye, (v+7), halfdir);
			varray_v[varray_vc].ncm[0] = DotProduct(surf->texinfo->vecs[0], halfdir);
			varray_v[varray_vc].ncm[1] = DotProduct(surf->texinfo->vecs[1], halfdir);
			if (surf->flags & SURF_PLANEBACK)
				varray_v[varray_vc].ncm[2] = -DotProduct(surf->plane->normal, halfdir);
			else
				varray_v[varray_vc].ncm[2] = DotProduct(surf->plane->normal, halfdir);
			varray_vc++;
		}
	}
}
#endif
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

	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	GL_Bind (tex->gl_texturenum);

	for (s=first; s ; s=s->texturechain)
	{
		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();

	glEnable(GL_BLEND);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_lightmap_format == GL_LUMINANCE || gl_lightmap_format == GL_RGB)
		glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f (0,0,0,1);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (gl_lightmap_format == GL_RGBA)
	{
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
	}

	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

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
}

static void PPL_BaseChain_NoBump_2TMU(msurface_t *s, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();

	if (tex->alphaed)
	{
		glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		glDisable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}


	GL_Bind (tex->gl_texturenum);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	qglActiveTextureARB(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	vi = -1;
	for (; s ; s=s->texturechain)
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

	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	qglActiveTextureARB(GL_TEXTURE0_ARB);

}

static void PPL_BaseChain_Bump_2TMU(msurface_t *first, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;
	msurface_t *s;
	PPL_EnableVertexArrays();

	if (tex->alphaed)
	{
		glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		glDisable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}

	//Bind normal map to texture unit 0
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumbumpmap);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	qglActiveTextureARB(GL_TEXTURE1_ARB);	//the deluxmap
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);


	vi = -1;
	for (s=first; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			GL_BindType(GL_TEXTURE_2D, deluxmap_textures[vi] );
			if (lightmap[vi]->deluxmodified)
			{
				lightmap[vi]->deluxmodified = false;
				theRect = &lightmap[vi]->deluxrectchange;
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmap[vi]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
		}

		PPL_GenerateArrays(s);
	}
	PPL_FlushArrays();

	qglActiveTextureARB(GL_TEXTURE0_ARB);
	GL_Bind (tex->gl_texturenum);

	qglActiveTextureARB(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

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

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	qglActiveTextureARB(GL_TEXTURE0_ARB);
}

static void PPL_BaseChain_Bump_4TMU(msurface_t *s, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();
	qglActiveTextureARB(GL_TEXTURE0_ARB);

	//Bind normal map to texture unit 0
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumbumpmap);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	qglActiveTextureARB(GL_TEXTURE1_ARB);	//the deluxmap
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	qglActiveTextureARB(GL_TEXTURE2_ARB);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	GL_Bind (tex->gl_texturenum);

	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	qglActiveTextureARB(GL_TEXTURE3_ARB);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

	qglClientActiveTextureARB(GL_TEXTURE3_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	vi = -1;
	for (; s ; s=s->texturechain)
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

	qglActiveTextureARB(GL_TEXTURE3_ARB);
	glDisable(GL_TEXTURE_2D);
	qglActiveTextureARB(GL_TEXTURE2_ARB);
	glDisable(GL_TEXTURE_2D);
	qglActiveTextureARB(GL_TEXTURE1_ARB);
	glDisable(GL_TEXTURE_2D);
	qglActiveTextureARB(GL_TEXTURE0_ARB);


	qglClientActiveTextureARB(GL_TEXTURE3_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);


	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

#ifdef SPECULAR
//Draw a texture chain with specular exponant 1.
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
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
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
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
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
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);

	if (qglGetError())
		Con_Printf("Error binding prev*prev\n");

	qglActiveTextureARB(GL_TEXTURE3_ARB);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumspec);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
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
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	qglActiveTextureARB(GL_TEXTURE1_ARB);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);
	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	qglActiveTextureARB(GL_TEXTURE2_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenum);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	qglActiveTextureARB(GL_TEXTURE3_ARB);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
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
	GLint errorPos, isNative;
	int i;
	const char *error;

	  //What should the minimum resource limits be?

      //RESOLVED: 10 attributes, 24 parameters, 4 texture indirections,
      //48 ALU instructions, 24 texture instructions, and 16 temporaries.

	//16 temps? hmm. that means we should be keeping the indirections instead of temp usage.
	//temps should be same speed, indirections could prevent texture loading for a bit.

	char *fp =
	//FP to do:
	//(diffuse*n.l + gloss*(n.h)^8)*lm
	//note excessive temp reuse...
	"!!ARBfp1.0\n"

	"OUTPUT      ocol  = result.color;\n"

	"PARAM       half  = { 0.5, 0.5, 0.5, 0.5 };\n"
	"PARAM       negone  = { -1,-1,-1,-1 };\n"

	"ATTRIB   tm_tc  = fragment.texcoord[0];\n"
	"ATTRIB   lm_tc  = fragment.texcoord[1];\n"
	"ATTRIB   cm_tc  = fragment.texcoord[2];\n"



	"TEMP diff, spec, nm, ld, cm, gm, lm, dm;\n"


	"TEX nm.rgb, tm_tc, texture[1], 2D;\n"		//t1 = n
	"TEX ld.rgb, lm_tc, texture[3], 2D;\n"		//t2 = l
	"TEX dm.rgb, tm_tc, texture[0], 2D;\n"		//t2 = d
	"TEX gm.rgb, tm_tc, texture[4], 2D;\n"		//t2 = gloss
	"TEX lm.rgb, lm_tc, texture[2], 2D;\n"	//specular = lm
	"TEX cm.rgb, cm_tc, texture[5], CUBE;\n"		//t2 = d

	//textures loaded - get diffuse
	"MAD nm, nm, 2, negone;\n"
	"MAD ld, ld, 2, negone;\n"
	"DP3 diff, nm, ld;\n"						//diff = n.l
	"MUL diff.rgb, diff, dm;\n"			//diff = diff*t2
	//diff now contains the entire diffuse part of the equation.

	//time for specular
	//t1 still = n
	"MAD cm, cm, 2, negone;\n"
	"DP3 spec, nm, cm;\n"				//spec = t1.t2

	"MUL spec, spec, spec;\n"
	"MUL spec, spec, spec;\n"
	"MUL spec, spec, spec;\n"

	"MUL spec, spec, gm;\n"
	//that's the specular part done.

	//we have diffuse and specular - wahoo
	//combine then halve.
	"ADD diff.rgb, diff, spec;\n"
	"MUL diff.rgb, diff, half;\n"


	//multiply by inverse lm and output the result.
	"SUB lm.rgb, 1, lm;\n"
	"MUL_SAT ocol.rgb, diff, lm;\n"
	//that's all folks.
	"END";

	ppl_specular_fragmentprogram = 0;

	for (i = 0; i < MAXARRAYVERTS; i++)
	{
		varray_i_forward[i] = i;
	}
	for (i = 0; i < MAXARRAYVERTS/3; i++)
	{
		varray_i_polytotri[i*3+0] = 0;
		varray_i_polytotri[i*3+1] = i+1;
		varray_i_polytotri[i*3+2] = i+2;
	}

	if (!gl_arb_fragment_program)
		return;

	glEnable(GL_FRAGMENT_PROGRAM_ARB);

	qglGenProgramsARB( 1, &ppl_specular_fragmentprogram ); 
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ppl_specular_fragmentprogram); 

	if (qglGetError())
		Con_Printf("GL Error binding fragment program\n");

	qglProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(fp), fp);
	if (qglGetError())
	{
		glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
		error = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
		Con_Printf("Fragment program error \'%s\'\n", error);
		ppl_specular_fragmentprogram = 0;
	}
	else
	{
		qglGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &isNative);
		if (!isNative)
			Con_Printf("Warning: Fragment program is emulated. You will likly experience poor performace.\n");
	}

	if (qglGetError())
		Con_Printf("GL Error loading fragment program\n");

	glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

static void PPL_BaseChain_Specular_FP(msurface_t *s, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();

	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ppl_specular_fragmentprogram); 

	if (qglGetError())
		Con_Printf("GL Error on shadow lighting\n");

	qglActiveTextureARB(GL_TEXTURE0_ARB);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenum);

	qglActiveTextureARB(GL_TEXTURE1_ARB);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumbumpmap);

	//qglActiveTextureARB(GL_TEXTURE2_ARB);
	//GL_BindType(GL_TEXTURE_2D, );	//lightmap

	//qglActiveTextureARB(GL_TEXTURE3_ARB);
	//GL_BindType(GL_TEXTURE_2D, );	//deluxmap

	if (qglGetError())
		Con_Printf("GL Error on shadow lighting\n");

	qglActiveTextureARB(GL_TEXTURE4_ARB);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumspec);

	qglActiveTextureARB(GL_TEXTURE5_ARB);
	GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);


	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->ncm);

	if (qglGetError())
		Con_Printf("GL Error on shadow lighting\n");

	vi = -1;
	for (; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			qglActiveTextureARB(GL_TEXTURE3_ARB);
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
			qglActiveTextureARB(GL_TEXTURE2_ARB);
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
		PPL_GenerateArraysBlinnCubeMap(s);
	}
	PPL_FlushArrays();

	if (qglGetError())
		Con_Printf("GL Error on shadow lighting\n");

	glDisable(GL_FRAGMENT_PROGRAM_ARB);


	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	qglActiveTextureARB(GL_TEXTURE0_ARB);

	if (qglGetError())
		Con_Printf("GL Error on shadow lighting\n");
}

#define GL_MODULATE_ADD_ATI                   0x8744
//we actually only use 7, so nur.
static void PPL_BaseChain_Specular_8TMU(msurface_t *first, texture_t *tex)
{	//uses blinn shading instead of phong. This way we don't have to generate lots of complex stuff.
	int vi;
	glRect_t    *theRect;
	msurface_t *s;

	float fourhalffloats[4] = {0.5,0.5,0.5,0.5};

	glColorMask(1,1,1,1);

	PPL_EnableVertexArrays();

/* lets do things in parallel.
normalmap -> rgb
rgb . halfvector -> alpha
alpha*alpha -> alpha			normalmap -> rgb
(alpha*alpha -> alpha)			rgb . luxmap -> rgb
alpha*gloss -> alpha			rgb * diffuse -> rgb
rgb + alpha -> rgb
rgb * lightmap -> rgb

  //note: crossbar could use third input texture removing the first tmu.
  //note: could combine3 combine the last two?
  //note: 5 tmus: not enough to work on a gf4.
*/
	glDisable(GL_BLEND);

//0 takes a normalmap
	qglActiveTextureARB(GL_TEXTURE0_ARB);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumbumpmap);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//1 takes a cubemap for specular half-vectors.
	qglActiveTextureARB(GL_TEXTURE1_ARB);
	GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->ncm);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGBA_ARB);	//writes alpha
//2 takes a normalmap
	qglActiveTextureARB(GL_TEXTURE2_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumbumpmap);
	qglClientActiveTextureARB(GL_TEXTURE2_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);	//square the alpha
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
//3 takes the deluxmap
	qglActiveTextureARB(GL_TEXTURE3_ARB);
	glEnable(GL_TEXTURE_2D);	//bind with the surface texturenum
	qglClientActiveTextureARB(GL_TEXTURE3_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);	//square the alpha again.
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
//4 multiplies with diffuse
	qglActiveTextureARB(GL_TEXTURE4_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenum);
	qglClientActiveTextureARB(GL_TEXTURE4_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);

	//nothing to the alpha (square yet again?)
//	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
//	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);

	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);	//square the alpha again.
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);

//5 halves rgb and alpha (so that adding will not clamp)
	qglActiveTextureARB(GL_TEXTURE5_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenum);	//need to bind something.
	qglClientActiveTextureARB(GL_TEXTURE5_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
/*	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, fourhalffloats);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_CONSTANT_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
*/
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_CONSTANT_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);

	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);

//6 adds rgb and alpha, using the glossmap...
	qglActiveTextureARB(GL_TEXTURE6_ARB);
	glEnable(GL_TEXTURE_2D);
	GL_BindType(GL_TEXTURE_2D, tex->gl_texturenumspec);
	qglClientActiveTextureARB(GL_TEXTURE6_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	//broken diffuse + specular
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,  GL_MODULATE_ADD_ATI);
	//perfect diffuse
/*	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,  GL_REPLACE);
*/
	//perfect specular
/*	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,  GL_MODULATE);
*/
//7 multiplies by lightmap
	qglActiveTextureARB(GL_TEXTURE7_ARB);
	glEnable(GL_TEXTURE_2D);	//bind with the surface texturenum
	qglClientActiveTextureARB(GL_TEXTURE7_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_ONE_MINUS_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);

	vi = -1;
	for (s = first; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			vi = s->lightmaptexturenum;

			qglActiveTextureARB(GL_TEXTURE3_ARB);
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
			qglActiveTextureARB(GL_TEXTURE7_ARB);
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
		PPL_GenerateArraysBlinnCubeMap(s);
	}
	PPL_FlushArrays();

	glColorMask(1,1,1,0);

	for (vi = 7; vi > 0; vi--)
	{
		qglActiveTextureARB(GL_TEXTURE0_ARB+vi);
		glDisable(GL_TEXTURE_2D);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB+vi);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	qglActiveTextureARB(GL_TEXTURE0_ARB);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
}
#endif

static void PPL_BaseTextureChain(msurface_t *first)
{
	extern int		*deluxmap_textures;
	extern cvar_t gl_bump, gl_specular;
	texture_t	*t;


	glEnable(GL_TEXTURE_2D);

	t = GLR_TextureAnimation (first->texinfo->texture);

	if (first->flags & SURF_DRAWTURB)
	{
		GL_DisableMultitexture();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Bind (t->gl_texturenum);
		for (; first ; first=first->texturechain)
			EmitWaterPolys (first, currententity->alpha);

		glDisable(GL_BLEND);
		glColor4f(1,1,1, 1);

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
		if (gl_bump.value && currentmodel->deluxdata && t->gl_texturenumbumpmap)
		{
			if (gl_mtexarbable>=4)
			{
				if (t->gl_texturenumspec && gl_specular.value)
				{
					if (ppl_specular_fragmentprogram)
						PPL_BaseChain_Specular_FP(first, t);
					else if (gl_mtexarbable>=8)
						PPL_BaseChain_Specular_8TMU(first, t);
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
			PPL_BaseChain_NoBump_2TMU(first, t);
		}
	}
}


static void PPL_FullBrightTextureChain(msurface_t *first)
{
	texture_t	*t;
	msurface_t	*s;

	t = GLR_TextureAnimation (first->texinfo->texture);

	if (detailtexture && gl_detail.value)
	{
		GL_Bind(detailtexture);
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

		PPL_EnableVertexArrays();
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
		for (s = first; s ; s=s->texturechain)
		{
			PPL_GenerateArrays(s);
		}
		PPL_FlushArrays();
	}

	if (t->gl_texturenumfb && r_fb_bmodels.value && cls.allow_luma)
	{
		GL_Bind(t->gl_texturenumfb);
		glBlendFunc(GL_ONE, GL_ONE);

		PPL_EnableVertexArrays();
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
		for (s = first; s ; s=s->texturechain)
		{
			PPL_GenerateArrays(s);
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

	glDisable(GL_BLEND);
	glColor4f(1,1,1, 1);
//	glDepthFunc(GL_LESS);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glShadeModel(GL_FLAT);

	currentmodel = model;
	currententity->alpha = 1;

	if (model == cl.worldmodel && skytexturenum>=0)
	{
		t = model->textures[skytexturenum];
		if (t)
		{
			s = t->texturechain;
			if (s)
			{
				t->texturechain = NULL;
				R_DrawSkyChain (s);
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
		{
			t->texturechain = NULL;
			continue;	// draw translucent water later
		}

		PPL_BaseTextureChain(s);
	}

	GL_DisableMultitexture();
}

void PPL_BaseBModelTextures(entity_t *e)
{
	extern msurface_t  *r_alpha_surfaces;
	int i, k;
	model_t *model;
	msurface_t *s;
	msurface_t *chain = NULL;

	glPushMatrix();
	R_RotateForEntity(e);
	currentmodel = model = e->model;
	s = model->surfaces+model->firstmodelsurface;

	if (currententity->alpha<1)
	{
		glEnable(GL_BLEND);
		glColor4f(1, 1, 1, currententity->alpha);
	}
	else
	{
		glDisable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (currentmodel->firstmodelsurface != 0 && r_dynamic.value)
	{
		for (k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			currentmodel->funcs.MarkLights (&cl_dlights[k], 1<<k,
				currentmodel->nodes + currentmodel->hulls[0].firstclipnode);
		}
	}

//update lightmaps.
	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
		R_RenderDynamicLightmaps (s);


	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (s->texinfo->flags & SURF_TRANS33 || s->texinfo->flags & SURF_TRANS66)
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

		s->texturechain = chain;
		chain = s;
	}

	if (chain)
		PPL_BaseTextureChain(chain);

	glPopMatrix();
	GL_DisableMultitexture();
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

		if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
			continue;

		if (!currententity->model)
			continue;


		if (cls.allow_anyparticles || currententity->visframe)	//allowed or static
		{
			if (currententity->model->particleeffect>=0)
			{
				if (currententity->model->particleengulphs)
				{
					if (gl_part_flame.value)
					{	//particle effect is addedin GLR_DrawEntitiesOnList. Is this so wrong?
						continue;
					}
				}
			}
		}

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawGAliasModel (currententity);
			break;

		case mod_brush:
			PPL_BaseBModelTextures (currententity);
			break;

		default:
			break;
		}
	}

	currentmodel = cl.worldmodel;
}

static void PPL_GenerateLightArrays(msurface_t *surf, vec3_t relativelightorigin, dlight_t *light)
{
	glpoly_t *p;
	int vi;
	int vc_s;
	float *v;

	vec3_t lightdir;
	float dist;

	for (p = surf->polys; p; p=p->next)
	{
		shadowlightfaces++;

		if (varray_ic + p->numverts*3>MAXARRAYVERTS)
		{
			PPL_FlushArrays();
		}

		vc_s = varray_vc;
		v = p->verts[0];

		varray_v[varray_vc].xyz[0] = v[0];
		varray_v[varray_vc].xyz[1] = v[1];
		varray_v[varray_vc].xyz[2] = v[2];
		varray_v[varray_vc].stw[0] = v[3];
		varray_v[varray_vc].stw[1] = v[4];
		lightdir[0] = relativelightorigin[0] - v[0];
		lightdir[1] = relativelightorigin[1] - v[1];
		lightdir[2] = relativelightorigin[2] - v[2];
		dist = 1-(sqrt(	(lightdir[0])*(lightdir[0]) +
						(lightdir[1])*(lightdir[1]) +
						(lightdir[2])*(lightdir[2])) / light->radius);
		VectorNormalize(lightdir);
		varray_v[varray_vc].stl[0] = light->color[0]*dist;
		varray_v[varray_vc].stl[1] = light->color[1]*dist;
		varray_v[varray_vc].stl[2] = light->color[2]*dist;
		varray_v[varray_vc].ncm[0] = DotProduct(lightdir, surf->texinfo->vecs[0]);
		varray_v[varray_vc].ncm[1] = -DotProduct(lightdir, surf->texinfo->vecs[1]);
		varray_v[varray_vc].ncm[2] = DotProduct(lightdir, surf->normal);


		varray_vc++;
		v += VERTEXSIZE;
		varray_v[varray_vc].xyz[0] = v[0];
		varray_v[varray_vc].xyz[1] = v[1];
		varray_v[varray_vc].xyz[2] = v[2];
		varray_v[varray_vc].stw[0] = v[3];
		varray_v[varray_vc].stw[1] = v[4];
		lightdir[0] = relativelightorigin[0] - v[0];
		lightdir[1] = relativelightorigin[1] - v[1];
		lightdir[2] = relativelightorigin[2] - v[2];
		dist = 1-(sqrt(	(lightdir[0])*(lightdir[0]) +
						(lightdir[1])*(lightdir[1]) +
						(lightdir[2])*(lightdir[2])) / light->radius);
		VectorNormalize(lightdir);
		varray_v[varray_vc].stl[0] = light->color[0]*dist;
		varray_v[varray_vc].stl[1] = light->color[1]*dist;
		varray_v[varray_vc].stl[2] = light->color[2]*dist;
		varray_v[varray_vc].ncm[0] = DotProduct(lightdir, surf->texinfo->vecs[0]);
		varray_v[varray_vc].ncm[1] = -DotProduct(lightdir, surf->texinfo->vecs[1]);
		varray_v[varray_vc].ncm[2] = DotProduct(lightdir, surf->normal);
		varray_vc++;
		v += VERTEXSIZE;
		for (vi=2 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
		{
			varray_i[varray_ic] = vc_s;
			varray_i[varray_ic+1] = varray_vc-1;
			varray_i[varray_ic+2] = varray_vc;
			varray_ic+=3;

			varray_v[varray_vc].xyz[0] = v[0];
			varray_v[varray_vc].xyz[1] = v[1];
			varray_v[varray_vc].xyz[2] = v[2];
			varray_v[varray_vc].stw[0] = v[3];
			varray_v[varray_vc].stw[1] = v[4];
			lightdir[0] = relativelightorigin[0] - v[0];
			lightdir[1] = relativelightorigin[1] - v[1];
			lightdir[2] = relativelightorigin[2] - v[2];
			dist = 1-(sqrt(	(lightdir[0])*(lightdir[0]) +
							(lightdir[1])*(lightdir[1]) +
							(lightdir[2])*(lightdir[2])) / light->radius);
			VectorNormalize(lightdir);
			varray_v[varray_vc].stl[0] = light->color[0]*dist;
			varray_v[varray_vc].stl[1] = light->color[1]*dist;
			varray_v[varray_vc].stl[2] = light->color[2]*dist;
			varray_v[varray_vc].ncm[0] = DotProduct(lightdir, surf->texinfo->vecs[0]);
			varray_v[varray_vc].ncm[1] = -DotProduct(lightdir, surf->texinfo->vecs[1]);
			varray_v[varray_vc].ncm[2] = DotProduct(lightdir, surf->normal);
			varray_vc++;
		}
	}
}

void PPL_LightTextures(model_t *model, vec3_t modelorigin, dlight_t *light)
{
	int		i;
	msurface_t	*s;
	texture_t	*t;
	extern cvar_t gl_bump;

	vec3_t relativelightorigin;

	PPL_EnableVertexArrays();

	VectorSubtract(light->origin, modelorigin, relativelightorigin);
	glShadeModel(GL_SMOOTH);
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;


		{
			extern int normalisationCubeMap;

			t = GLR_TextureAnimation (t);


			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);
			if (t->gl_texturenumbumpmap && gl_mtexarbable>2)
			{
				qglActiveTextureARB(GL_TEXTURE0_ARB);
				GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
				glEnable(GL_TEXTURE_2D);
				//Set up texture environment to do (tex0 dot tex1)*color
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//make texture normalmap available.
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

				qglClientActiveTextureARB(GL_TEXTURE0_ARB);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

				qglActiveTextureARB(GL_TEXTURE1_ARB);
				GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
				glEnable(GL_TEXTURE_CUBE_MAP_ARB);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//normalisation cubemap * normalmap
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

				qglClientActiveTextureARB(GL_TEXTURE1_ARB);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->ncm);

				qglActiveTextureARB(GL_TEXTURE2_ARB);
				GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
				glEnable(GL_TEXTURE_2D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//bumps * color		(the attenuation)
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB); //(doesn't actually use the bound texture)
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			}
			else
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glDisable(GL_TEXTURE_2D);
				qglActiveTextureARB(GL_TEXTURE1_ARB);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				glDisable(GL_TEXTURE_CUBE_MAP_ARB);
				qglActiveTextureARB(GL_TEXTURE0_ARB);

				qglClientActiveTextureARB(GL_TEXTURE0_ARB);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
			}

			for (; s; s=s->texturechain)
			{

				if (s->shadowframe != r_shadowframe)
					continue;

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
				PPL_GenerateLightArrays(s, relativelightorigin, light);
			}
			PPL_FlushArrays();
		}
	}

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_TEXTURE_2D);
	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglActiveTextureARB(GL_TEXTURE1_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	qglActiveTextureARB(GL_TEXTURE0_ARB);
	glDisable(GL_TEXTURE_2D);

}

void PPL_LightBModelTextures(entity_t *e, dlight_t *light)
{
	int i;
	model_t *model = e->model;

	msurface_t	*s;
	texture_t	*t;
	extern cvar_t gl_bump;

			vec3_t relativelightorigin;

	glPushMatrix();
	R_RotateForEntity(e);
	glColor4f(1, 1, 1, 1);


		VectorSubtract(light->origin, e->origin, relativelightorigin);
		glShadeModel(GL_SMOOTH);

		for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
		{
			t = GLR_TextureAnimation (s->texinfo->texture);

			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);
			if (t->gl_texturenumbumpmap && gl_mtexarbable>2)
			{
				qglActiveTextureARB(GL_TEXTURE0_ARB);
				GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
				glEnable(GL_TEXTURE_2D);
				//Set up texture environment to do (tex0 dot tex1)*color
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//make texture normalmap available.
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

				qglClientActiveTextureARB(GL_TEXTURE0_ARB);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

				qglActiveTextureARB(GL_TEXTURE1_ARB);
				GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
				glEnable(GL_TEXTURE_CUBE_MAP_ARB);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//normalisation cubemap * normalmap
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

				qglClientActiveTextureARB(GL_TEXTURE1_ARB);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->ncm);

				qglActiveTextureARB(GL_TEXTURE2_ARB);
				GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
				glEnable(GL_TEXTURE_2D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//bumps * color		(the attenuation)
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB); //(doesn't actually use the bound texture)
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			}
			else
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glDisable(GL_TEXTURE_2D);
				qglActiveTextureARB(GL_TEXTURE1_ARB);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				glDisable(GL_TEXTURE_CUBE_MAP_ARB);
				qglActiveTextureARB(GL_TEXTURE0_ARB);

				qglClientActiveTextureARB(GL_TEXTURE0_ARB);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
			}

//			for (; s; s=s->texturechain)
			{

				if (s->shadowframe != r_shadowframe)
					continue;

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
				PPL_GenerateLightArrays(s, relativelightorigin, light);
			}
			PPL_FlushArrays();
		}

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_TEXTURE_2D);
	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglActiveTextureARB(GL_TEXTURE1_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	qglActiveTextureARB(GL_TEXTURE0_ARB);
	glDisable(GL_TEXTURE_2D);

	glPopMatrix();
}

//draw the bumps on the models for each light.
void PPL_DrawEntLighting(dlight_t *light)
{
	int		i;

	PPL_LightTextures(cl.worldmodel, r_worldentity.origin, light);

	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
			continue;

		if (!currententity->model)
			continue;

		if (cls.allow_anyparticles || currententity->visframe)	//allowed or static
		{
			if (currententity->model->particleeffect>=0)
			{
				if (currententity->model->particleengulphs)
				{
					if (gl_part_flame.value)
					{
						continue;
					}
				}
			}
		}

		switch (currententity->model->type)
		{
		case mod_alias:
//			R_DrawGAliasModelLighting (currententity);
			break;

		case mod_brush:
			PPL_LightBModelTextures (currententity, light);
			break;

		default:
			break;
		}
	}
}

void PPL_FullBrights(model_t *model)
{
	int		tn;
	msurface_t	*s;
	texture_t	*t;

	glColor3f(1,1,1);

	glDepthMask(0);	//don't bother writing depth

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glShadeModel(GL_FLAT);

	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);

	for (tn=0 ; tn<model->numtextures ; tn++)
	{
		t = model->textures[tn];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;

		PPL_FullBrightTextureChain(s);

		t->texturechain=NULL;
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDepthMask(1);
}

void PPL_FullBrightBModelTextures(entity_t *e)
{
	int i;
	model_t *model;
	msurface_t *s;
	msurface_t *chain = NULL;

	glPushMatrix();
	R_RotateForEntity(e);
	currentmodel = model = e->model;
	s = model->surfaces+model->firstmodelsurface;

	glColor4f(1, 1, 1, 1);
	glDepthMask(0);	//don't bother writing depth

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_FLAT);

	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);

	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (chain && s->texinfo->texture != chain->texinfo->texture)	//last surface or not the same as the next
		{
			PPL_FullBrightTextureChain(chain);
			chain = NULL;
		}

		s->texturechain = chain;
		chain = s;
	}

	if (chain)
		PPL_FullBrightTextureChain(chain);

	glPopMatrix();
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDepthMask(1);
}

//draw the bumps on the models for each light.
void PPL_DrawEntFullBrights(void)
{
	int		i;

	PPL_FullBrights(cl.worldmodel);

	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
			continue;

		if (!currententity->model)
			continue;

		if (cls.allow_anyparticles || currententity->visframe)	//allowed or static
		{
			if (currententity->model->particleeffect>=0)
			{
				if (currententity->model->particleengulphs)
				{
					if (gl_part_flame.value)
					{
						continue;
					}
				}
			}
		}

		switch (currententity->model->type)
		{
		case mod_alias:
//			R_DrawGAliasModelLighting (currententity);
			break;

		case mod_brush:
			PPL_FullBrightBModelTextures (currententity);
			break;

		default:
			break;
		}
	}
}

















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

void PPL_RecursiveWorldNode_r (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	glpoly_t *p;
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
					if (DotProduct(surf->plane->normal, lightorg)-surf->plane->dist <= -lightradius)
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
#define PROJECTION_DISTANCE (float)(lightradius*2)//0x7fffffff

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

				for (p = surf->polys; p; p=p->next)
				{
					shadowsurfcount++;

					//front face
					glVertexPointer(3, GL_FLOAT, sizeof(GLfloat)*VERTEXSIZE, p->verts[0]);
					glDrawElements(GL_TRIANGLES, (p->numverts-2)*3, GL_UNSIGNED_INT, varray_i_polytotri);

					//back
					glBegin(GL_POLYGON);
					for (v = p->numverts-1; v >=0; v--)
					{
						v1 = p->verts[v];
						v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
						v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
						v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

						glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
					}
					glEnd();
					
				}
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
	glpoly_t *p;
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


				for (p = surf->polys; p; p=p->next)
				{
					//front face
					glVertexPointer(3, GL_FLOAT, sizeof(GLfloat)*VERTEXSIZE, p->verts);
					glDrawElements(GL_TRIANGLES, (p->numverts-2)*3, GL_UNSIGNED_INT, varray_i_polytotri);

//back
					glBegin(GL_POLYGON);
					for (v = p->numverts-1; v >=0; v--)
					{
						v1 = p->verts[v];
						v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
						v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
						v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

						glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
					}
					glEnd();
					
				}
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
	glpoly_t *p;
	int v;

	float *v2;
	vec3_t v4;

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



				for (p = surf->polys; p; p=p->next)
				{
					//front face
					glVertexPointer(3, GL_FLOAT, sizeof(GLfloat)*VERTEXSIZE, p->verts);
					glDrawElements(GL_TRIANGLES, (p->numverts-2)*3, GL_UNSIGNED_INT, varray_i_polytotri);
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
						glBegin( GL_QUAD_STRIP );
							glVertex3f( v1[0], v1[1], v1[2] );
							glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
							glVertex3f( v2[0], v2[1], v2[2] );
							glVertex3f( v2[0]+v4[0], v2[1]+v4[1], v2[2]+v4[2] );
						glEnd();
					}

//back
					glBegin(GL_POLYGON);
					for (v = p->numverts-1; v >=0; v--)
					{
						v1 = p->verts[v];
						v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
						v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
						v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

						glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
					}
					glEnd();
					
				}
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
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		{
			for ( ; c ; c--, surf++)
			{

			}
		}
	}

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

	glEnableClientState(GL_VERTEX_ARRAY);

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

	glVertexPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v[0].xyz);
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
			glDrawElements(GL_QUADS, varray_vc, GL_UNSIGNED_INT, varray_i_forward);
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
	glDrawElements(GL_QUADS, varray_vc, GL_UNSIGNED_INT, varray_i_forward);

	if (qglGetError())
		Con_Printf("GL Error on entities\n");

	varray_vc=0;

	firstedge=0;
}

void PPL_DrawBrushModel(dlight_t *dl, entity_t *e)
{
	glpoly_t *p;
	int v;
	float *v1, *v2;
	vec3_t v3, v4;

	int i;
	model_t *model;
	msurface_t *surf;

	RotateLightVector(e->angles, e->origin, dl->origin, lightorg);

	glPushMatrix();
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

		for (p = surf->polys; p; p=p->next)
		{
			//front face
			glBegin(GL_POLYGON);
			for (v = 0; v < p->numverts; v++)
				glVertex3fv(p->verts[v]);
			glEnd();

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
				glBegin( GL_QUAD_STRIP );
					glVertex3f( v1[0], v1[1], v1[2] );
					glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
					glVertex3f( v2[0], v2[1], v2[2] );
					glVertex3f( v2[0]+v4[0], v2[1]+v4[1], v2[2]+v4[2] );
				glEnd();
			}
			
//back
			
			glBegin(GL_POLYGON);
			for (v = p->numverts-1; v >=0; v--)
			{
				v1 = p->verts[v];
				v3[0] = ( v1[0]-lightorg[0] )*PROJECTION_DISTANCE;
				v3[1] = ( v1[1]-lightorg[1] )*PROJECTION_DISTANCE;
				v3[2] = ( v1[2]-lightorg[2] )*PROJECTION_DISTANCE;

				glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
			}
			glEnd();
			
		}
	}
	glPopMatrix();
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

		if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
			continue;

		if (!currententity->model)
			continue;

		if (dl->key == currententity->keynum)
			continue;

		if (currententity->flags & Q2RF_WEAPONMODEL)
			continue;	//weapon models don't cast shadows.

		if (cls.allow_anyparticles || currententity->visframe)	//allowed or static
		{
			if (currententity->model->particleeffect>=0)
			{
				if (currententity->model->particleengulphs)
				{
					if (gl_part_flame.value)
					{
						continue;
					}
				}
			}
		}

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawGAliasShadowVolume (currententity, dl->origin, dl->radius);
			break;

		case mod_brush:
			PPL_DrawBrushModel (dl, currententity);
			break;

		default:
			break;
		}
	}
}

void CL_NewDlight (int key, float x, float y, float z, float radius, float time,
				   int type);
//generates stencil shadows of the world geometry.
//redraws world geometry
void PPL_AddLight(dlight_t *dl)
{
	int i;
	int sdecrw;
	int sincrw;
	mnode_t *node;
	int leaf;
	qbyte *lvis;
	qbyte *vvis;

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

	if (R_CullBox(mins, maxs))
		return;

	if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
		i = cl.worldmodel->funcs.LeafForPoint(r_refdef.vieworg, cl.worldmodel);
	else
		i = r_viewleaf - cl.worldmodel->leafs;

	leaf = cl.worldmodel->funcs.LeafForPoint(dl->origin, cl.worldmodel);
	lvis = cl.worldmodel->funcs.LeafPVS(leaf, cl.worldmodel, lvisb);
	vvis = cl.worldmodel->funcs.LeafPVS(i, cl.worldmodel, vvisb);

//	if (!(lvis[i>>3] & (1<<(i&7))))	//light might not be visible, but it's effects probably should be.
//		return;
	if (!PPL_VisOverlaps(lvis, vvis))	//The two viewing areas do not intersect.
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
	glStencilFunc( GL_ALWAYS, 1, ~0 );

	glDisable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDisable(GL_TEXTURE_2D);
	glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	glDepthMask(0);

	if (gldepthfunc==GL_LEQUAL)
		glDepthFunc(GL_LESS);
	else
		glDepthFunc(GL_GREATER);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);

	sincrw = GL_INCR;
	sdecrw = GL_DECR;
	if (gl_ext_stencil_wrap)
	{	//minamlise damage...
		sincrw = GL_INCR_WRAP_EXT;
		sdecrw = GL_DECR_WRAP_EXT;
	}
//our stencil writes.

	PPL_EnableVertexArrays();

#ifdef _DEBUG
	if (r_shadows.value == 666)	//testing (visible shadow volumes)
	{
		if (qglGetError())
			Con_Printf("GL Error on entities\n");
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glColor3f(dl->color[0], dl->color[1], dl->color[2]);
		glDisable(GL_STENCIL_TEST);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		if (qglGetError())
			Con_Printf("GL Error on entities\n");
		PPL_RecursiveWorldNode(dl);
		if (qglGetError())
			Con_Printf("GL Error on entities\n");
		PPL_DrawShadowMeshes(dl);
		if (qglGetError())
			Con_Printf("GL Error on entities\n");
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	else
#endif
		
	if (qglStencilOpSeparateATI && r_shadows.value != 667)//GL_ATI_separate_stencil
	{
		glClearStencil(0);
		glClear(GL_STENCIL_BUFFER_BIT);
		glDisable(GL_CULL_FACE);

		qglStencilOpSeparateATI(GL_BACK, GL_KEEP, sincrw, GL_KEEP);
		qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, sdecrw, GL_KEEP);
		PPL_RecursiveWorldNode(dl);
		PPL_DrawShadowMeshes(dl);
		qglStencilOpSeparateATI(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_KEEP);

		glEnable(GL_CULL_FACE);

		glStencilFunc( GL_EQUAL, 0, ~0 );
	}
	else if (qglActiveStencilFaceEXT && r_shadows.value != 667)	//NVidias variation on a theme. (GFFX class)
	{
		glClearStencil(0);
		glClear(GL_STENCIL_BUFFER_BIT);
		glDisable(GL_CULL_FACE);

		glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

		glCullFace(GL_BACK);
		qglActiveStencilFaceEXT(GL_BACK);
		glStencilOp(GL_KEEP, sincrw, GL_KEEP);

		qglActiveStencilFaceEXT(GL_FRONT);
		glStencilOp(GL_KEEP, sdecrw, GL_KEEP);

		PPL_RecursiveWorldNode(dl);
		PPL_DrawShadowMeshes(dl);

		qglActiveStencilFaceEXT(GL_BACK);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		qglActiveStencilFaceEXT(GL_FRONT);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);

		glEnable(GL_CULL_FACE);

		glStencilFunc( GL_EQUAL, 0, ~0 );
	}
	else //your graphics card sucks and lacks efficient stencil shadow techniques.
	{	//centered around 0. Will only be increased then decreased less.
		glClearStencil(0);
		glClear(GL_STENCIL_BUFFER_BIT);

		glCullFace(GL_BACK);
		glStencilOp(GL_KEEP, sincrw, GL_KEEP);
		PPL_RecursiveWorldNode(dl);
		PPL_DrawShadowMeshes(dl);

		glCullFace(GL_FRONT);
		glStencilOp(GL_KEEP, sdecrw, GL_KEEP);
		PPL_RecursiveWorldNode(dl);
		PPL_DrawShadowMeshes(dl);

		glStencilFunc( GL_EQUAL, 0, ~0 );
	}
//end stencil writing.
	glEnable(GL_DEPTH_TEST);
	glDepthMask(0);
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	glCullFace(GL_FRONT);

	glColor3f(1,1,1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glColor4f(dl->color[0], dl->color[1], dl->color[2], 1);
	glDepthFunc(GL_EQUAL);

	lightorg[0] = dl->origin[0];
	lightorg[1] = dl->origin[1];
	lightorg[2] = dl->origin[2];

	PPL_DrawEntLighting(dl);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(1);
	glDepthFunc(gldepthfunc);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
}

void PPL_DrawWorld (void)
{
	dlight_t *l;
	int i;

	int maxshadowlights = gl_maxshadowlights.value;

	if (maxshadowlights < 1)
		maxshadowlights = 1;
	if (qglGetError())
		Con_Printf("GL Error before world\n");
//glColorMask(0,0,0,0);
	PPL_BaseTextures(cl.worldmodel);
	if (qglGetError())
		Con_Printf("GL Error during base textures\n");
//glColorMask(1,1,1,1);
	PPL_BaseEntTextures();
//	CL_NewDlightRGB(1, r_refdef.vieworg[0], r_refdef.vieworg[1]-16, r_refdef.vieworg[2]-24, 128, 1, 1, 1, 1);

	if (qglGetError())
		Con_Printf("GL Error on entities\n");

	if (r_shadows.value && glStencilFunc)
	{
		if (cl.worldmodel->fromgame == fg_quake || cl.worldmodel->fromgame == fg_halflife || cl.worldmodel->fromgame == fg_quake2 /*|| cl.worldmodel->fromgame == fg_quake3*/)
		{
			for (l = cl_dlights, i=0 ; i<MAX_DLIGHTS ; i++, l++)
			{
				if (l->die < cl.time || !l->radius || l->noppl)
					continue;
				if (l->color[0]<0)
					continue;	//quick check for darklight
				if (!maxshadowlights--)
					break;
				l->color[0]*=2.5;
				l->color[1]*=2.5;
				l->color[2]*=2.5;

				PPL_AddLight(l);
				l->color[0]/=2.5;
				l->color[1]/=2.5;
				l->color[2]/=2.5;
			}
			glEnable(GL_TEXTURE_2D);
		}

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}

	if (qglGetError())
		Con_Printf("GL Error on shadow lighting\n");

	PPL_DrawEntFullBrights();

	if (qglGetError())
		Con_Printf("GL Error on fullbrights/details\n");

//	Con_Printf("%i %i %i\n", shadowsurfcount, shadowedgecount, shadowlightfaces);
	shadowsurfcount	= 0;
	shadowedgecount = 0;
	shadowlightfaces = 0;
}
#endif
