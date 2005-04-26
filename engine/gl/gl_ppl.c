#include "quakedef.h"
#ifdef RGLQUAKE
#include "glquake.h"
#include "shader.h"

//these are shared with gl_rsurf - move to header
void R_MirrorChain (msurface_t *s);
void GL_SelectTexture (GLenum target);
void R_RenderDynamicLightmaps (msurface_t *fa);
void R_BlendLightmaps (void);

extern qboolean r_inmirror;
extern int gldepthfunc;
extern int		*lightmap_textures;
extern int		lightmap_bytes;		// 1, 2, or 4

extern cvar_t		gl_detail;
extern cvar_t		r_fb_bmodels;
extern cvar_t		gl_part_flame;

extern cvar_t		gl_part_flame;
extern cvar_t		gl_maxshadowlights;
extern cvar_t		r_shadow_realtime_world;
extern int		detailtexture;
extern cvar_t gl_bump;
extern cvar_t gl_specular;
//end header confict

extern cvar_t gl_schematics;
extern cvar_t r_drawflat;
extern cvar_t r_wallcolour;
extern cvar_t r_floorcolour;

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

//#define glBegin glEnd


typedef struct shadowmesh_s {
	int numindicies;
	int *indicies;
	vec3_t *verts;
} shadowmesh_t;


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

#define MAXARRAYVERTS	2048
static surfvertexarray_t varray_v[MAXARRAYVERTS];
static unsigned int varray_i[MAXARRAYVERTS];
static unsigned int varray_i_forward[MAXARRAYVERTS];
static unsigned int varray_i_polytotri[MAXARRAYVERTS];	//012 023 034 045...
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
	qglDisableClientState( GL_COLOR_ARRAY );
}
inline void PPL_FlushArrays(void)
{
	if (varray_ic)
		qglDrawElements(GL_TRIANGLES, varray_ic, GL_UNSIGNED_INT, varray_i);
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
	GL_Bind (tex->gl_texturenum);

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

static void PPL_BaseChain_NoBump_2TMU(msurface_t *s, texture_t *tex)
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

		qglVertexPointer(3, GL_FLOAT, sizeof(vec4_t), s->mesh->xyz_array);

		qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_UNSIGNED_INT, s->mesh->indexes);
	}

	qglDisable(GL_TEXTURE_2D);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_SelectTexture(GL_TEXTURE0_ARB);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

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
	GL_MBind(GL_TEXTURE0_ARB, tex->gl_texturenumbumpmap);
	qglEnable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_ARB);
	qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);

	GL_MBind(GL_TEXTURE1_ARB, tex->gl_texturenumbumpmap);
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

	GL_MBind(GL_TEXTURE0_ARB, tex->gl_texturenum);

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

	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	GL_SelectTexture(GL_TEXTURE0_ARB);
}

static void PPL_BaseChain_Bump_4TMU(msurface_t *s, texture_t *tex)
{
	int vi;
	glRect_t    *theRect;

	PPL_EnableVertexArrays();

	//Bind normal map to texture unit 0
	GL_MBind(GL_TEXTURE0_ARB, tex->gl_texturenumbumpmap);
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
	GL_MBind(GL_TEXTURE2_ARB, tex->gl_texturenum);
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
		"	spec = pow(dv, 8.0) * specs;\n"
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

	GL_MBind(GL_TEXTURE0_ARB, tex->gl_texturenum);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	GL_MBind(GL_TEXTURE1_ARB, tex->gl_texturenumbumpmap);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

//	GL_MBind(GL_TEXTURE2_ARB, lightmap_textures[vi] );

//	GL_MBind(GL_TEXTURE3_ARB, deluxmap_textures[vi] );

	GL_MBind(GL_TEXTURE4_ARB, tex->gl_texturenumspec);

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
		
		qglVertexPointer(3, GL_FLOAT, sizeof(GL_FLOAT)*4, s->mesh->xyz_array);
		qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_UNSIGNED_INT, s->mesh->indexes);
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

//single textured.
static void PPL_BaseChain_Flat(msurface_t *first)
{
	static vec_t wallcolour[4] = {0,0,0,1};
	static vec_t floorcolour[4] = {0,0,0,1};
	msurface_t *s;
	int iswall = -1;
	int vi=-10;
	glRect_t    *theRect;
	
	if (r_wallcolour.modified)
	{
		char *s;
		r_wallcolour.modified = false;

		s = COM_Parse(r_wallcolour.string);
		wallcolour[0] = atof(com_token);
		s = COM_Parse(s);
		wallcolour[1] = atof(com_token);
		s = COM_Parse(s);
		wallcolour[2] = atof(com_token);
	}
	if (r_floorcolour.modified)
	{
		char *s;
		r_floorcolour.modified = false;

		s = COM_Parse(r_floorcolour.string);
		floorcolour[0] = atof(com_token);
		s = COM_Parse(s);
		floorcolour[1] = atof(com_token);
		s = COM_Parse(s);
		floorcolour[2] = atof(com_token);
	}

	PPL_EnableVertexArrays();
	GL_TexEnv(GL_MODULATE);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);

	for (s = first; s ; s=s->texturechain)
	{
		if (vi != s->lightmaptexturenum)
		{
			PPL_FlushArrays();
			if (vi < 0)
				qglEnable(GL_TEXTURE_2D);
			else if (s->lightmaptexturenum < 0)
				qglDisable(GL_TEXTURE_2D);
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
		}

		if ((s->plane->normal[2]*s->plane->normal[2]) <= 0.5*0.5)
		{
			if (iswall != 0)
			{
				PPL_FlushArrays();
				iswall=0;
				qglColor4fv(wallcolour);
			}
		}
		else if (iswall != 1)
		{
			PPL_FlushArrays();
			iswall=1;
			qglColor4fv(floorcolour);
		}
		PPL_GenerateArrays(s);
	}

	PPL_FlushArrays();
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglColor3f(1,1,1);
}

static void PPL_BaseChain_NPR_Sketch(msurface_t *first)
{
	msurface_t *s;
	int vi=-10;
	int i;
	glRect_t    *theRect;

	static int textures[10];

	GL_SelectTexture(GL_TEXTURE0_ARB);
	if (r_drawflat.modified)	//reload textures
	{
		r_drawflat.modified = false;
		for (i = 0; i < sizeof(textures)/sizeof(textures[0]); i++)
		{
			textures[i] = Mod_LoadHiResTexture(va("sketch%i", i+1), "sketch", true, false, false);
			if (!textures[i])
			{
				int data[128*128];
				FILE *file;
				unsigned char *f;
				int p;

				file = fopen(va("textures/tex%i_3_128_128.raw", i+1), "rb");
			
				if (file)
				{
					f = Hunk_TempAlloc(128*128*3);
					if (fread(f, 128*3, 128, file) == 128)
					{
						for (p = 0; p < 128*128; p++)
							data[p] = LittleLong(f[p*3] + (f[p*3+1]<<8) + (f[p*3+2]<<16) + (255<<24));
						textures[i] = GL_LoadTexture32 (va("textures/tex%i_3_128_128.raw", i+1), 128, 128, data, true, false);
					}
					fclose(file);
				}
			}
		}
	}

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

			GL_MBind(GL_TEXTURE0_ARB, textures[rand()%10]);

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
	texture_t	*t;
	if (r_drawflat.value)
	{
		if (r_drawflat.value == 2)
		{
			if (gl_mtexarbable >= 2)	//shiesh!.
			{
				PPL_BaseChain_NPR_Sketch(first);
				return;
			}
		}
		else
		{
			PPL_BaseChain_Flat(first);	//who cares about texture? :/
			return;
		}
	}
#ifdef Q3SHADERS
	if (first->texinfo->texture->shader)
	{
		meshbuffer_t mb;
		msurface_t *s;
		int vi=-1;
		int redraw = false;

		glRect_t    *theRect;
		if (first->texinfo->texture->shader->flags & SHADER_FLARE )
			return;

		if (!varrayactive)
			R_IBrokeTheArrays();

		mb.entity = &r_worldentity;
		mb.shader = first->texinfo->texture->shader;
		mb.mesh = NULL;
		mb.fog = NULL;
		mb.infokey = -2;
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
				if (s->flags&SURF_DRAWALPHA || !(mb.shader->sort & SHADER_SORT_OPAQUE))
				{
					extern msurface_t  *r_alpha_surfaces;
					s->nextalphasurface = r_alpha_surfaces;
					r_alpha_surfaces = s;
					s->ownerent = &r_worldentity;
					continue;
				}

				if (s->mesh)
				{
					redraw = mb.fog != s->fog || mb.infokey != vi|| mb.shader->flags&SHADER_DEFORMV_BULGE;

					if (redraw)// || numIndexes + s->mesh->numindexes > MAX_ARRAY_INDEXES)
					{
						if (mb.mesh)
							R_RenderMeshBuffer ( &mb, false );
						redraw = false;
					}

					mb.infokey = vi;
					mb.mesh = s->mesh;
					mb.fog = s->fog;
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

	t = GLR_TextureAnimation (first->texinfo->texture);

	if (first->flags & SURF_DRAWTURB)
	{
		GL_DisableMultitexture();
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Bind (t->gl_texturenum);
		for (; first ; first=first->texturechain)
			EmitWaterPolys (first, currententity->alpha);

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
		if (gl_bump.value && currentmodel->deluxdata && t->gl_texturenumbumpmap)
		{
			if (gl_mtexarbable>=4)
			{
				if (t->gl_texturenumspec && gl_specular.value)
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
		qglBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

		PPL_EnableVertexArrays();
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
		for (s = first; s ; s=s->texturechain)
		{
			PPL_GenerateArrays(s);
		}
		PPL_FlushArrays();
	}

	if (t->gl_texturenumfb && r_fb_bmodels.value && cls.allow_luma)
	{
		GL_Bind(t->gl_texturenumfb);
		qglBlendFunc(GL_ONE, GL_ONE);

		PPL_EnableVertexArrays();
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglTexCoordPointer(2, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stw);
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

	GL_DoSwap();

	qglDisable(GL_BLEND);
	qglColor4f(1,1,1, 1);
//	qglDepthFunc(GL_LESS);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglShadeModel(GL_FLAT);

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
	if (!r_inmirror && mirrortexturenum>=0 && model == cl.worldmodel && r_mirroralpha.value != 1.0)
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
			continue;	// draw translucent water later

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

	qglPushMatrix();
	R_RotateForEntity(e);
	currentmodel = model = e->model;
	s = model->surfaces+model->firstmodelsurface;

	GL_TexEnv(GL_MODULATE);

	if (currententity->alpha<1)
	{
		qglEnable(GL_BLEND);
		qglColor4f(1, 1, 1, currententity->alpha);
	}
	else
	{
		qglDisable(GL_BLEND);
		qglColor4f(1, 1, 1, 1);
	}

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (currentmodel->firstmodelsurface != 0 && r_dynamic.value)
	{
		for (k=0 ; k<MAX_SWLIGHTS ; k++)
		{
			if (!cl_dlights[k].radius)
				continue;
			if (cl_dlights[k].nodynamic)
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

	qglPopMatrix();
	GL_DisableMultitexture();
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	if (!varrayactive)
		R_IBrokeTheArrays();
}

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

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
	VectorScale( perpvec, e->frame / 2, perpvec );

	for ( i = 0; i < 6; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglDepthMask( GL_FALSE );
	qglDisable(GL_ALPHA_TEST);

	r = ( d_8to24rgbtable[e->skinnum & 0xFF] ) & 0xFF;
	g = ( d_8to24rgbtable[e->skinnum & 0xFF] >> 8 ) & 0xFF;
	b = ( d_8to24rgbtable[e->skinnum & 0xFF] >> 16 ) & 0xFF;

	r *= 1/255.0F;
	g *= 1/255.0F;
	b *= 1/255.0F;

	qglColor4f( r, g, b, e->alpha );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		qglVertex3fv( start_points[i] );
		qglVertex3fv( end_points[i] );
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	qglEnd();

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglDepthMask( GL_TRUE );
}

void PPL_BaseEntTextures(void)
{
	extern qboolean r_inmirror;
	extern model_t *currentmodel;
	int		i,j;

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
			j = currententity->keynum;
			while(j)
			{
				
				if (j == (cl.viewentity[r_refdef.currentplayernum]?cl.viewentity[r_refdef.currentplayernum]:(cl.playernum[r_refdef.currentplayernum]+1)))
					break;

				j = cl.lerpents[j].tagent;
			}
			if (j)
				continue;

			if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
				continue;
			if (!Cam_DrawPlayer(0, currententity->keynum-1))
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
			if (!varrayactive)
				R_IBrokeTheArrays();
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

#ifdef PPL
static void PPL_GenerateLightArrays(msurface_t *surf, vec3_t relativelightorigin, dlight_t *light, vec3_t colour)
{
	int vi;
	float *v, *stw;
	surfvertexarray_t *out;

	vec3_t lightdir;
	float dist;

	shadowlightfaces++;

//	if (varray_vc + surf->mesh->numvertexes*3>MAXARRAYVERTS)
	{
		PPL_FlushArrays();
	}

	v = surf->mesh->xyz_array[0];
	stw = surf->mesh->st_array[0];

	out = &varray_v[varray_vc];

	for (vi=0 ; vi<surf->mesh->numvertexes ; vi++, v+=4, stw+=2, out++)
	{
		out->xyz[0] = v[0];
		out->xyz[1] = v[1];
		out->xyz[2] = v[2];
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
		out->ncm[2] = DotProduct(lightdir, surf->normal);
	}
	for (vi=0 ; vi<surf->mesh->numindexes ; vi++)
	{
		varray_i[varray_ic++] = varray_vc+vi;
	}

	varray_vc += surf->mesh->numvertexes;
}

//flags
enum{
PERMUTATION_GENERIC = 0,
PERMUTATION_BUMPMAP = 1,
PERMUTATION_SPECULAR = 2,
PERMUTATION_BUMP_SPEC = 3,

PERMUTATIONS
};
int ppl_light_shader[PERMUTATIONS];
int ppl_light_shader_eyeposition[PERMUTATIONS];
int ppl_light_shader_lightposition[PERMUTATIONS];
int ppl_light_shader_lightcolour[PERMUTATIONS];
int ppl_light_shader_lightradius[PERMUTATIONS];

void PPL_CreateLightTexturesProgram(void)
{
	int i;

	char *permutation[PERMUTATIONS] = {
		"",
		"#define BUMP\n",
		"#define SPECULAR\n",
		"#define SPECULAR\n#define BUMP\n"
	};
	char *vert = 
		"varying vec2 tcbase;\n"
		"uniform vec3 texr, texu, texf;\n"
		"varying vec3 LightVector;\n"
		"uniform vec3 LightPosition;\n"

		"#ifdef SPECULAR\n"
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

		"#ifdef SPECULAR\n"
		"	vec3 eyeminusvertex = EyePosition - gl_Vertex.xyz;\n"
		"	EyeVector.x = dot(eyeminusvertex, gl_MultiTexCoord1.xyz);\n"
		"	EyeVector.y = dot(eyeminusvertex, gl_MultiTexCoord2.xyz);\n"
		"	EyeVector.z = dot(eyeminusvertex, gl_MultiTexCoord3.xyz);\n"
		"#endif\n"
		"}\n"
		;

	char *frag =
		"uniform sampler2D baset;\n"
		"#if defined(BUMP) || defined(SPECULAR)\n"
		"uniform sampler2D bumpt;\n"
		"#endif\n"
		"#ifdef SPECULAR\n"
		"uniform sampler2D speculart;\n"
		"#endif\n"

		"varying vec2 tcbase;\n"
		"varying vec3 LightVector;\n"

		"uniform float lightradius;\n"
		"uniform vec3 LightColour;\n"

		"#ifdef SPECULAR\n"
		"varying vec3 EyeVector;\n"
		"#endif\n"


		"void main (void)\n"
		"{\n"
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

			GLSlang_UseProgram(0);
		}
	}
};

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

		t = GLR_TextureAnimation (t);


		p = 0;
		if (t->gl_texturenumbumpmap)
			p |= PERMUTATION_BUMPMAP;
		if (gl_specular.value && t->gl_texturenumspec)
			p |= PERMUTATION_SPECULAR;

		if (p != lp)
		{
			lp = p;
			GLSlang_UseProgram(ppl_light_shader[p]);
			qglUniform3fvARB(ppl_light_shader_eyeposition[p], 1, relativeeyeorigin);
			qglUniform3fvARB(ppl_light_shader_lightposition[p], 1, relativelightorigin);
			qglUniform3fvARB(ppl_light_shader_lightcolour[p], 1, colour);
			qglUniform1fARB(ppl_light_shader_lightradius[p], light->radius);
		}


		GL_MBind(GL_TEXTURE0_ARB, t->gl_texturenum);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		GL_MBind(GL_TEXTURE1_ARB, t->gl_texturenumbumpmap);
		GL_MBind(GL_TEXTURE2_ARB, t->gl_texturenumspec);
		GL_SelectTexture(GL_TEXTURE0_ARB);

		for (; s; s=s->texturechain)
		{
//			if (s->shadowframe != r_shadowframe)
//				continue;

/*
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
*/

			qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, s->texinfo->vecs[0][0], s->texinfo->vecs[0][1], s->texinfo->vecs[0][2]);
			qglMultiTexCoord3fARB(GL_TEXTURE2_ARB, -s->texinfo->vecs[1][0], -s->texinfo->vecs[1][1], -s->texinfo->vecs[1][2]);

			if (s->flags & SURF_PLANEBACK)
				qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, -s->plane->normal[0], -s->plane->normal[1], -s->plane->normal[2]);
			else
				qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, s->plane->normal[0], s->plane->normal[1], s->plane->normal[2]);

			qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->st_array);
			qglVertexPointer(3, GL_FLOAT, sizeof(float)*4, s->mesh->xyz_array);
			qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_UNSIGNED_INT, s->mesh->indexes);

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

	if (ppl_light_shader)
	{
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

			t = GLR_TextureAnimation (t);


			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);
			if (t->gl_texturenumbumpmap && gl_mtexarbable>3)
			{
				GL_MBind(GL_TEXTURE0_ARB, t->gl_texturenumbumpmap);
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

				GL_MBind(GL_TEXTURE2_ARB, t->gl_texturenumbumpmap);	//a dummy
				qglEnable(GL_TEXTURE_2D);
				GL_TexEnv(GL_COMBINE_ARB);	//bumps * color		(the attenuation)
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB); //(doesn't actually use the bound texture)
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);

				GL_MBind(GL_TEXTURE3_ARB, t->gl_texturenum);
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
			}
			PPL_FlushArrays();
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
	GL_TexEnv(GL_MODULATE);
	qglBlendFunc(GL_ONE, GL_ONE);

	if (qglGetError())
		Con_Printf("GL Error early in lighttextures\n");

	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (tnum != s->texinfo->texture)
		{
			tnum = s->texinfo->texture;

			t = GLR_TextureAnimation (tnum);

			p = 0;
			if (t->gl_texturenumbumpmap)
				p |= PERMUTATION_BUMPMAP;
			if (gl_specular.value && t->gl_texturenumspec)
				p |= PERMUTATION_SPECULAR;
			if (p != lp)
			{
				lp = p;
				GLSlang_UseProgram(ppl_light_shader[p]);
				qglUniform3fvARB(ppl_light_shader_eyeposition[p], 1, relativeeyeorigin);
				qglUniform3fvARB(ppl_light_shader_lightposition[p], 1, relativelightorigin);
				qglUniform3fvARB(ppl_light_shader_lightcolour[p], 1, colour);
				qglUniform1fARB(ppl_light_shader_lightradius[p], light->radius);
			}

			GL_MBind(GL_TEXTURE0_ARB, t->gl_texturenum);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
			GL_MBind(GL_TEXTURE1_ARB, t->gl_texturenumbumpmap);
			GL_MBind(GL_TEXTURE2_ARB, t->gl_texturenumspec);
			GL_SelectTexture(GL_TEXTURE0_ARB);
		}

		qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, -s->texinfo->vecs[0][0], -s->texinfo->vecs[0][1], -s->texinfo->vecs[0][2]);
		qglMultiTexCoord3fARB(GL_TEXTURE2_ARB, s->texinfo->vecs[1][0], s->texinfo->vecs[1][1], s->texinfo->vecs[1][2]);

		if (s->flags & SURF_PLANEBACK)
			qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, -s->plane->normal[0], -s->plane->normal[1], -s->plane->normal[2]);
		else
			qglMultiTexCoord3fARB(GL_TEXTURE3_ARB, s->plane->normal[0], s->plane->normal[1], s->plane->normal[2]);

		qglTexCoordPointer(2, GL_FLOAT, 0, s->mesh->st_array);
		qglVertexPointer(3, GL_FLOAT, sizeof(float)*4, s->mesh->xyz_array);
		qglDrawElements(GL_TRIANGLES, s->mesh->numindexes, GL_UNSIGNED_INT, s->mesh->indexes);


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

	if (ppl_light_shader)
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
			t = GLR_TextureAnimation (s->texinfo->texture);

			qglEnableClientState(GL_COLOR_ARRAY);
			qglColorPointer(3, GL_FLOAT, sizeof(surfvertexarray_t), varray_v->stl);
			if (t->gl_texturenumbumpmap && gl_mtexarbable>3)
			{
				GL_MBind(GL_TEXTURE0_ARB, t->gl_texturenumbumpmap);
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

				GL_MBind(GL_TEXTURE2_ARB, t->gl_texturenumbumpmap);
				qglEnable(GL_TEXTURE_2D);
				GL_TexEnv(GL_COMBINE_ARB);	//bumps * color		(the attenuation)
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB); //(doesn't actually use the bound texture)
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);

				GL_MBind(GL_TEXTURE3_ARB, t->gl_texturenum);
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
	int		i, j;

	PPL_LightTextures(cl.worldmodel, r_worldentity.origin, light, colour);

	if (!r_drawentities.value)
		return;

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
			j = currententity->keynum;
			while(j)
			{
				
				if (j == (cl.viewentity[r_refdef.currentplayernum]?cl.viewentity[r_refdef.currentplayernum]:(cl.playernum[r_refdef.currentplayernum]+1)))
					break;

				j = cl.lerpents[j].tagent;
			}
			if (j)
				continue;

			if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
				continue;
			if (!Cam_DrawPlayer(0, currententity->keynum-1))
				continue;
		}

		if (currententity->flags & Q2RF_BEAM)
		{
			continue;
		}
		if (!currententity->model)
			continue;

		switch (currententity->model->type)
		{
		case mod_alias:
			if (!varrayactive)
				R_IBrokeTheArrays();
			R_DrawGAliasModelLighting (currententity, light->origin, colour, light->radius);
			break;

		case mod_brush:
			PPL_LightBModelTextures (currententity, light, colour);
			break;

		default:
			break;
		}
	}
}
#endif

void PPL_FullBrights(model_t *model)
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

		PPL_FullBrightTextureChain(s);

		t->texturechain=NULL;
	}

	GL_TexEnv(GL_REPLACE);
	qglDepthMask(1);
}

void PPL_FullBrightBModelTextures(entity_t *e)
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
			PPL_FullBrightTextureChain(chain);
			chain = NULL;
		}

		s->texturechain = chain;
		chain = s;
	}

	if (chain)
		PPL_FullBrightTextureChain(chain);

	qglPopMatrix();
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	qglDepthMask(1);
}

//draw the bumps on the models for each light.
void PPL_DrawEntFullBrights(void)
{
	int		i;

//	if (gl_detail.value || (r_fb_bmodels.value && cls.allow_luma))
		PPL_FullBrights(cl.worldmodel);

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
					VectorMA(pos, 8, s->normal, v);
					qglTexCoord2f (fcol, frow);
					qglVertex3fv(v);
					VectorMA(pos, -8, dir, pos);
					VectorMA(pos, 8, s->normal, v);
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
			VectorMA(v1, 4, s->normal, pos);

			VectorMA(pos, 4, dir, v);
			VectorMA(v, -4, s->normal, v);
			qglVertex3fv(v);
			qglVertex3fv(pos);

			VectorMA(v, 8, s->normal, v);
			qglVertex3fv(v);
			qglVertex3fv(pos);

			//the line
			qglVertex3fv(pos);
			VectorMA(pos, len/2 - sl*4, dir, pos);
			qglVertex3fv(pos);

			//right hand side.
			VectorMA(v2, 4, s->normal, pos);

			VectorMA(pos, -4, dir, v);
			VectorMA(v, -4, s->normal, v);
			qglVertex3fv(v);
			qglVertex3fv(pos);

			VectorMA(v, 8, s->normal, v);
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

				qglVertexPointer(3, GL_FLOAT, sizeof(float)*4, surf->mesh->xyz_array);
				qglDrawElements(GL_TRIANGLES, surf->mesh->numindexes, GL_UNSIGNED_INT, surf->mesh->indexes);

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
				qglVertexPointer(3, GL_FLOAT, sizeof(GLfloat)*4, surf->mesh->xyz_array);
				qglDrawElements(GL_TRIANGLES, surf->mesh->numindexes, GL_UNSIGNED_INT, surf->mesh->indexes);

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
					qglDrawElements(GL_TRIANGLES, (p->numverts-2)*3, GL_UNSIGNED_INT, varray_i_polytotri);
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

	if (dl->worldshadowmesh)
	{
		qglEnableClientState(GL_VERTEX_ARRAY);
		qglVertexPointer(3, GL_FLOAT, sizeof(vec3_t), dl->worldshadowmesh->verts);
		qglDrawElements(GL_TRIANGLES, dl->worldshadowmesh->numindicies, GL_UNSIGNED_INT, dl->worldshadowmesh->indicies);
		return;
	}

	lightradius = dl->radius;

	lightorg[0] = dl->origin[0]+0.5;
	lightorg[1] = dl->origin[1]+0.5;
	lightorg[2] = dl->origin[2]+0.5;

	modelorg[0] = lightorg[0];
	modelorg[1] = lightorg[1];
	modelorg[2] = lightorg[2];

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
}

void PPL_DrawBrushModelShadow(dlight_t *dl, entity_t *e)
{
	int v;
	float *v1, *v2;
	vec3_t v3, v4;

	int i;
	model_t *model;
	msurface_t *surf;

	RotateLightVector(e->angles, e->origin, dl->origin, lightorg);

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
		qglVertexPointer(3, GL_FLOAT, 4*sizeof(float), surf->mesh->xyz_array);
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
			PPL_DrawBrushModelShadow (dl, currententity);
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
			ML_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, (float)vid.width/vid.height, r_refdef.fov_y);
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
			ML_Project(v, v2, r_refdef.viewangles, r_refdef.vieworg, vid.width/vid.height, r_refdef.fov_y);
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

void CL_NewDlight (int key, float x, float y, float z, float radius, float time,
				   int type);
//generates stencil shadows of the world geometry.
//redraws world geometry
void PPL_AddLight(dlight_t *dl)
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
		return;	//just switch these off.

	if (PPL_ScissorForBox(mins, maxs))
		return;	//was culled.

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

	PPL_EnableVertexArrays();

	qglDisable(GL_TEXTURE_2D);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//	if (1)
//		goto noshadows;

	qglEnable(GL_SCISSOR_TEST);


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
		PPL_UpdateNodeShadowFrames(lvisb);
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

		PPL_UpdateNodeShadowFrames(lvisb);
		PPL_RecursiveWorldNode(dl);
		PPL_DrawShadowMeshes(dl);

		qglActiveStencilFaceEXT(GL_BACK);
		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		qglActiveStencilFaceEXT(GL_FRONT);
		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);

		qglEnable(GL_CULL_FACE);

		qglActiveStencilFaceEXT(GL_BACK);
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
	qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	qglCullFace(GL_FRONT);

//noshadows:
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
}

#endif

void GL_CheckTMUIs0(void);

void PPL_DrawWorld (void)
{
	RSpeedLocals();
	dlight_t *l;
#if 0
	dlight_t *lc, *furthestprev;
	float furthest;
#endif
	int i;

	vec3_t mins, maxs;

	int maxshadowlights = gl_maxshadowlights.value;

	if (!lightmap)
	{
		R_PreNewMap();
		R_NewMap();
		return;	// :/
	}

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
	RSpeedRemark();
	if (r_shadows.value && qglStencilFunc && gl_canstencil)
	{
		if (cl.worldmodel->fromgame == fg_quake || cl.worldmodel->fromgame == fg_halflife || cl.worldmodel->fromgame == fg_quake2 /*|| cl.worldmodel->fromgame == fg_quake3*/)
		{
//			lc = NULL;
			for (l = cl_dlights, i=0 ; i<MAX_DLIGHTS ; i++, l++)
			{
				if (!l->radius || l->noppl)
					continue;
				if (l->color[0]<0)
					continue;	//quick check for darklight

				if (l->isstatic)
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
//				if (R_CullSphere(l->origin, l->radius*1.1))
//					continue;

#if 1
				if (!maxshadowlights--)
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
				if(!l->isstatic)
				{
					l->color[0]*=10;
					l->color[1]*=10;
					l->color[2]*=10;
				}
				TRACE(("dbg: calling PPL_AddLight\n"));
				PPL_AddLight(l);
				if(!l->isstatic)
				{
					l->color[0]/=10;
					l->color[1]/=10;
					l->color[2]/=10;
				}
			}
			qglEnable(GL_TEXTURE_2D);
		
		}

		qglDisableClientState(GL_COLOR_ARRAY);
	}
	RSpeedEnd(RSPEED_STENCILSHADOWS);
#endif

//	if (qglGetError())
//		Con_Printf("GL Error on shadow lighting\n");

	RSpeedRemark();

	if (gl_schematics.value)
		PPL_Schematics();

	TRACE(("dbg: calling PPL_DrawEntFullBrights\n"));
	PPL_DrawEntFullBrights();

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
#endif

void PPL_CreateShaderObjects(void)
{
#ifdef PPL
	PPL_CreateLightTexturesProgram();
#endif
	PPL_LoadSpecularFragmentProgram();
}