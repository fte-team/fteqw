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

//#define glBegin glEnd


#define	Q2RF_WEAPONMODEL		4		// only draw through eyes

#define EDGEOPTIMISE
#ifdef EDGEOPTIMISE
struct {
	short count;
	short count2;
	short next;
	short prev;
} edge[MAX_MAP_EDGES];
int firstedge;
#endif

vec3_t lightorg = {0, 0, 0};
float lightradius;

static void PPL_BaseTextureChain(msurface_t *first)
{
	extern int		*deluxmap_textures;
	extern cvar_t gl_bump;
	texture_t	*t;

	msurface_t *s = first;

	int vi;
	glRect_t    *theRect;
	glpoly_t *p;
	float *v;

	glEnable(GL_TEXTURE_2D);

	t = GLR_TextureAnimation (s->texinfo->texture);

	if (s->flags & SURF_DRAWTURB)
	{
		GL_DisableMultitexture();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Bind (t->gl_texturenum);
		for (; s ; s=s->texturechain)
			EmitWaterPolys (s);

		glDisable(GL_BLEND);
		glColor4f(1,1,1, 1);

		t->texturechain = NULL;	//no lighting effects. (good job these don't animate eh?)
		return;
	}

	if (s->lightmaptexturenum < 0)	//no lightmap
	{
		GL_DisableMultitexture();
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		GL_Bind (t->gl_texturenum);


		for (; s ; s=s->texturechain)
		for (p = s->polys; p; p=p->next)
		{
			glBegin(GL_POLYGON);
			v = p->verts[0];
			for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[3], v[4]);
				glVertex3fv (v);
			}
			glEnd ();
		}
	}
	else if (!gl_mtexable)
	{	//multitexture isn't supported.
		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		GL_Bind (t->gl_texturenum);
		for (s = first; s ; s=s->texturechain)
		{
			for (p = s->polys; p; p=p->next)
			{
				glBegin(GL_POLYGON);
				v = p->verts[0];
				for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
				{
					glTexCoord2f (v[3], v[4]);
					glVertex3fv (v);
				}
				glEnd ();
			}
		}

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

		for (s = first; s ; s=s->texturechain)
		{
			vi = s->lightmaptexturenum;
			// Binds lightmap to texenv 1
			GL_Bind (lightmap_textures[vi]);
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

			for (p = s->polys; p; p=p->next)
			{
				glBegin(GL_POLYGON);
				v = p->verts[0];
				for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
				{
					glTexCoord2f (v[5], v[6]);
					glVertex3fv (v);
				}
				glEnd ();
			}
		}
	}
	else
	{
		if (gl_bump.value && currentmodel->deluxdata && t->gl_texturenumbumpmap)
		{
			qglActiveTextureARB(GL_TEXTURE0_ARB);

			//Bind normal map to texture unit 0
			GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
			glEnable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

			qglActiveTextureARB(GL_TEXTURE1_ARB);	//the deluxmap
			glEnable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);
//we now have normalmap.deluxmap on the screen.
			if (gl_mtexarbable>=4)	//go the whole hog. bumpmapping in one pass.
			{
				//continue going to give (normalmap.deluxemap)*texture*lightmap.
				qglActiveTextureARB(GL_TEXTURE2_ARB);
				glEnable(GL_TEXTURE_2D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind (t->gl_texturenum);

				qglActiveTextureARB(GL_TEXTURE3_ARB);
				glEnable(GL_TEXTURE_2D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

				vi = -1;
				for (; s ; s=s->texturechain)
				{
//					if (vi != s->lightmaptexturenum)
					{
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

					for (p = s->polys; p; p=p->next)
					{
						glBegin(GL_POLYGON);
						v = p->verts[0];
						for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
						{									
							qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
							qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[5], v[6]);
							qglMultiTexCoord2fARB(GL_TEXTURE2_ARB, v[3], v[4]);
							qglMultiTexCoord2fARB(GL_TEXTURE3_ARB, v[5], v[6]);
							glVertex3fv (v);
						}
						glEnd ();
					}
				}

				qglActiveTextureARB(GL_TEXTURE3_ARB);
				glDisable(GL_TEXTURE_2D);
				qglActiveTextureARB(GL_TEXTURE2_ARB);
				glDisable(GL_TEXTURE_2D);
				qglActiveTextureARB(GL_TEXTURE1_ARB);
				glDisable(GL_TEXTURE_2D);
				qglActiveTextureARB(GL_TEXTURE0_ARB);	//the deluxmap

				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				return;
			}

			glDisable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			for (; s ; s=s->texturechain)
			{
				vi = s->lightmaptexturenum;
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

				for (p = s->polys; p; p=p->next)
				{
					glBegin(GL_POLYGON);
					v = p->verts[0];
					for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
					{									
						qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
						qglMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[5], v[6]);
						glVertex3fv (v);
					}
					glEnd ();
				}
			}
			glDisable(GL_TEXTURE_2D);
			qglActiveTextureARB(GL_TEXTURE0_ARB);

			glBlendFunc(GL_DST_COLOR, GL_ZERO);	//tell the texture + lightmap to do current*tex*light (where current is normalmap.deluxemap)
			glEnable(GL_BLEND);

			s = first;

			GL_SelectTexture(mtexid0);
			GL_Bind(t->gl_texturenum);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_EnableMultitexture();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
		}
		else
		{
			// Binds world to texture env 0
			GL_SelectTexture(mtexid0);
			GL_Bind (t->gl_texturenum);
			if (t->alphaed)
			{
				glEnable(GL_BLEND);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
			else
			{
				glDisable(GL_BLEND);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
			GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
		}

		for (; s; s=s->texturechain)
		{
			vi = s->lightmaptexturenum;
			// Binds lightmap to texenv 1
			GL_Bind (lightmap_textures[vi]);
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
			for (p = s->polys; p; p=p->next)
			{
				glBegin(GL_POLYGON);
				v = p->verts[0];
				for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
				{
					qglMTexCoord2fSGIS (mtexid0, v[3], v[4]);
					qglMTexCoord2fSGIS (mtexid1, v[5], v[6]);
					glVertex3fv (v);
				}
				glEnd ();
			}
		}
	}
}

static void PPL_FullBrightTextureChain(msurface_t *first)
{
	glpoly_t	*p;
	float		*v;
	texture_t	*t;
	msurface_t	*s;
	int i;

	t = GLR_TextureAnimation (first->texinfo->texture);

	if (detailtexture && gl_detail.value)
	{
		GL_Bind(detailtexture);
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

		for (s = first; s ; s=s->texturechain)
		{
			for (p = s->polys; p; p=p->next)
			{
				glBegin(GL_POLYGON);
				v = p->verts[0];
				for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
				{
					glTexCoord2f (v[5] * 18, v[6] * 18);
					glVertex3fv (v);
				}
				glEnd();
			}
		}
	}

	if (t->gl_texturenumfb && r_fb_bmodels.value && cls.allow_luma)
	{
		GL_Bind(t->gl_texturenumfb);
		glBlendFunc(GL_DST_COLOR, GL_ONE);

		for (s = first; s ; s=s->texturechain)
		{
			for (p = s->polys; p; p=p->next)
			{
				glBegin(GL_POLYGON);
				v = p->verts[0];
				for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
				{
					glTexCoord2f (v[3], v[4]);
					glVertex3fv (v);
				}
				glEnd();
			}
		}
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
	int i, k;
	model_t *model;
	msurface_t *s;
	msurface_t *chain = NULL;

	glPushMatrix();
	R_RotateForEntity(e);
	currentmodel = model = e->model;
	s = model->surfaces+model->firstmodelsurface;

	glDisable(GL_BLEND);
	glColor4f(1, 1, 1, 1);

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
		if (chain && s->texinfo->texture != chain->texinfo->texture)	//last surface or not the same as the next
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

void PPL_LightTextures(model_t *model, vec3_t modelorigin, dlight_t *light)
{
	int		i;
	msurface_t	*s;
	texture_t	*t;
	extern cvar_t gl_bump;

	int vi;
	glpoly_t *p;
	float *v;
	float dist;

	if (gl_bump.value)
	{
		vec3_t relativelightorigin;

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
				vec3_t lightdir;

				t = GLR_TextureAnimation (t);


				if (t->gl_texturenumbumpmap)
				{
					qglActiveTextureARB(GL_TEXTURE0_ARB);
					GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
					glEnable(GL_TEXTURE_2D);
					//Set up texture environment to do (tex0 dot tex1)*color
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//make texture normalmap available.
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

					qglActiveTextureARB(GL_TEXTURE1_ARB);
					GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
					glEnable(GL_TEXTURE_CUBE_MAP_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//normalisation cubemap * normalmap
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

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
					glDisable(GL_TEXTURE_CUBE_MAP_ARB);
					qglActiveTextureARB(GL_TEXTURE0_ARB);
				}

				for (; s; s=s->texturechain)
				{

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
					for (p = s->polys; p; p=p->next)
					{
						glBegin(GL_POLYGON);
						v = p->verts[0];
						for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
						{
							lightdir[0] = relativelightorigin[0] - v[0];
							lightdir[1] = relativelightorigin[1] - v[1];
							lightdir[2] = relativelightorigin[2] - v[2];

							dist = 1-(sqrt(	(lightdir[0])*(lightdir[0]) +
											(lightdir[1])*(lightdir[1]) +
											(lightdir[2])*(lightdir[2])) / light->radius);

							VectorNormalize(lightdir);

							glColor3f(light->color[0]*dist, light->color[1]*dist, light->color[2]*dist);
							qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
							qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, DotProduct(lightdir, s->texinfo->vecs[0]), -DotProduct(lightdir, s->texinfo->vecs[1]), DotProduct(lightdir, s->normal));
							glVertex3fv (v);
						}
						glEnd ();
					}
				}
			}
		}

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_2D);
		qglActiveTextureARB(GL_TEXTURE1_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		qglActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else
	{
		vec3_t relativelightorigin;
		vec3_t lightdir;

		glDisable(GL_TEXTURE_2D);

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
//				t = GLR_TextureAnimation (t);


//				GL_Bind (t->gl_texturenum);
				for (; s; s=s->texturechain)
				{

				/*	if (fabs(s->center[0] - lightorg[0]) > lightradius+s->radius ||
						fabs(s->center[1] - lightorg[1]) > lightradius+s->radius ||
						fabs(s->center[2] - lightorg[2]) > lightradius+s->radius)
						continue;*/


					if (s->flags & SURF_PLANEBACK)
					{//inverted normal.
						if (DotProduct(s->plane->normal, lightorg)-s->plane->dist <= -lightradius)
							continue;
					}
					else
					{
						if (DotProduct(s->plane->normal, lightorg)-s->plane->dist >= lightradius)
							continue;
					}

					for (p = s->polys; p; p=p->next)
					{
						glBegin(GL_POLYGON);
						v = p->verts[0];
						for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
						{
							lightdir[0] = relativelightorigin[0] - v[0];
							lightdir[1] = relativelightorigin[1] - v[1];
							lightdir[2] = relativelightorigin[2] - v[2];

							dist = 1-(sqrt(	(lightdir[0])*(lightdir[0]) +
											(lightdir[1])*(lightdir[1]) +
											(lightdir[2])*(lightdir[2])) / light->radius);

							VectorNormalize(lightdir);

							glColor3f(light->color[0]*dist, light->color[1]*dist, light->color[2]*dist);
//							glTexCoord2f (v[3], v[4]);
							glVertex3fv (v);
						}
						glEnd ();
					}
				}
			}
		}
	}
}
void PPL_LightBModelTextures(entity_t *e, dlight_t *light)
{
	glpoly_t *p;

	int i;
	model_t *model = e->model;

	msurface_t	*s;
	texture_t	*t;
	extern cvar_t gl_bump;

	int vi;
	float *v;
	float dist;

	glPushMatrix();
	R_RotateForEntity(e);
	glColor4f(1, 1, 1, 1);

	if (gl_bump.value)
	{
		vec3_t relativelightorigin;

		VectorSubtract(light->origin, e->origin, relativelightorigin);
		glShadeModel(GL_SMOOTH);

		for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
		{
			t = GLR_TextureAnimation (s->texinfo->texture);

			{
				extern int normalisationCubeMap;
				vec3_t lightdir;

				if (t->gl_texturenumbumpmap)
				{
					qglActiveTextureARB(GL_TEXTURE0_ARB);
					GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
					glEnable(GL_TEXTURE_2D);
					//Set up texture environment to do (tex0 dot tex1)*color
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//make texture normalmap available.
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);

					qglActiveTextureARB(GL_TEXTURE1_ARB);
					GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
					glEnable(GL_TEXTURE_CUBE_MAP_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//normalisation cubemap * normalmap
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);

					qglActiveTextureARB(GL_TEXTURE2_ARB);
					GL_BindType(GL_TEXTURE_2D, t->gl_texturenumbumpmap);
					glEnable(GL_TEXTURE_2D);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);	//bumps * color
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
					glDisable(GL_TEXTURE_CUBE_MAP_ARB);
					qglActiveTextureARB(GL_TEXTURE0_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				}

				{
/*
					if (fabs(s->center[0] - lightorg[0]) > lightradius+s->radius ||
						fabs(s->center[1] - lightorg[1]) > lightradius+s->radius ||
						fabs(s->center[2] - lightorg[2]) > lightradius+s->radius)
						continue;
*/

					if (s->flags & SURF_PLANEBACK)
					{//inverted normal.
						if (-DotProduct(s->plane->normal, relativelightorigin)-s->plane->dist >= lightradius)
							continue;
					}
					else
					{
						if (DotProduct(s->plane->normal, relativelightorigin)-s->plane->dist >= lightradius)
							continue;
					}
					for (p = s->polys; p; p=p->next)
					{
						glBegin(GL_POLYGON);
						v = p->verts[0];
						for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
						{
							lightdir[0] = relativelightorigin[0] - v[0];
							lightdir[1] = relativelightorigin[1] - v[1];
							lightdir[2] = relativelightorigin[2] - v[2];

							dist = 1-(sqrt(	(lightdir[0])*(lightdir[0]) +
											(lightdir[1])*(lightdir[1]) +
											(lightdir[2])*(lightdir[2])) / light->radius);

							VectorNormalize(lightdir);

							glColor3f(light->color[0]*dist, light->color[1]*dist, light->color[2]*dist);
							qglMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
							qglMultiTexCoord3fARB(GL_TEXTURE1_ARB, DotProduct(lightdir, s->texinfo->vecs[0]), -DotProduct(lightdir, s->texinfo->vecs[1]), DotProduct(lightdir, s->normal));
							glVertex3fv (v);
						}
						glEnd ();
					}
				}
			}
		}

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_2D);
		qglActiveTextureARB(GL_TEXTURE1_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		qglActiveTextureARB(GL_TEXTURE0_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		vec3_t relativelightorigin;
		vec3_t lightdir;

		glDisable(GL_TEXTURE_2D);

		VectorSubtract(light->origin, e->origin, relativelightorigin);

		glShadeModel(GL_SMOOTH);
		for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
		{
			{
				{

				/*	if (fabs(s->center[0] - lightorg[0]) > lightradius+s->radius ||
						fabs(s->center[1] - lightorg[1]) > lightradius+s->radius ||
						fabs(s->center[2] - lightorg[2]) > lightradius+s->radius)
						continue;*/


					if (s->flags & SURF_PLANEBACK)
					{//inverted normal.
						if (DotProduct(s->plane->normal, lightorg)-s->plane->dist <= -lightradius)
							continue;
					}
					else
					{
						if (DotProduct(s->plane->normal, lightorg)-s->plane->dist >= lightradius)
							continue;
					}

					for (p = s->polys; p; p=p->next)
					{
						glBegin(GL_POLYGON);
						v = p->verts[0];
						for (vi=0 ; vi<p->numverts ; vi++, v+= VERTEXSIZE)
						{
							lightdir[0] = relativelightorigin[0] - v[0];
							lightdir[1] = relativelightorigin[1] - v[1];
							lightdir[2] = relativelightorigin[2] - v[2];

							dist = 1-(sqrt(	(lightdir[0])*(lightdir[0]) +
											(lightdir[1])*(lightdir[1]) +
											(lightdir[2])*(lightdir[2])) / light->radius);

							VectorNormalize(lightdir);

							glColor3f(light->color[0]*dist, light->color[1]*dist, light->color[2]*dist);
//							glTexCoord2f (v[3], v[4]);
							glVertex3fv (v);
						}
						glEnd ();
					}
				}
			}
		}
	}




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

int r_shadowframe;

void PPL_RecursiveWorldNode_r (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	glpoly_t *p;
	int v;
#ifndef EDGEOPTIMISE
	float *v2;
	vec3_t v4;
#endif
	float *v1;
	vec3_t v3;

	if (node->shadowframe != r_shadowframe)
		return;

	if (node->contents == Q1CONTENTS_SOLID)
		return;		// solid

//	if (R_CullBox (node->minmaxs, node->minmaxs+3))
//		return;
	
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
				if (fabs(surf->center[0] - lightorg[0]) > lightradius+surf->radius ||
					fabs(surf->center[1] - lightorg[1]) > lightradius+surf->radius ||
					fabs(surf->center[2] - lightorg[2]) > lightradius+surf->radius)
					continue;
				
#define PROJECTION_DISTANCE (float)0x7fffffff
#ifdef EDGEOPTIMISE
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
#endif
				for (p = surf->polys; p; p=p->next)
				{
					//front face
					glBegin(GL_POLYGON);
					for (v = 0; v < p->numverts; v++)
						glVertex3fv(p->verts[v]);
					glEnd();
					
#ifndef EDGEOPTIMISE
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
						glBegin( GL_QUADS );
							glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
							glVertex3f( v2[0]+v4[0], v2[1]+v4[1], v2[2]+v4[2] );
							glVertex3f( v2[0], v2[1], v2[2] );
							glVertex3f( v1[0], v1[1], v1[2] );
						glEnd();
					}
#endif
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
#ifndef EDGEOPTIMISE
	float *v2;
	vec3_t v4;
#endif
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
#define PROJECTION_DISTANCE (float)0x7fffffff
				if (surf->flags & (SURF_DRAWALPHA | SURF_DRAWTILED))
				{	// no shadows
					continue;
				}

#ifdef EDGEOPTIMISE
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
#endif

				for (p = surf->polys; p; p=p->next)
				{
					//front face
					glBegin(GL_POLYGON);
					for (v = 0; v < p->numverts; v++)
						glVertex3fv(p->verts[v]);
					glEnd();
#ifndef EDGEOPTIMISE
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
#endif
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
//#ifndef EDGEOPTIMISE
	float *v2;
	vec3_t v4;
//#endif
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
#define PROJECTION_DISTANCE (float)0x7fffffff
				/*if (surf->flags & (SURF_DRAWALPHA | SURF_DRAWTILED))
				{	// no shadows
					continue;
				}*/

/*#ifdef EDGEOPTIMISE
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
#endif*/

				for (p = surf->polys; p; p=p->next)
				{
					//front face
					glBegin(GL_POLYGON);
					for (v = 0; v < p->numverts; v++)
						glVertex3fv(p->verts[v]);
					glEnd();
//#ifndef EDGEOPTIMISE
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
//#endif
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

	if (cl.worldmodel->fromgame == fg_quake3)
		PPL_RecursiveWorldNodeQ3_r(cl.worldmodel->nodes);
	else if (cl.worldmodel->fromgame == fg_quake2)
		PPL_RecursiveWorldNodeQ2_r(cl.worldmodel->nodes);
	else
		PPL_RecursiveWorldNode_r(cl.worldmodel->nodes);

#ifdef EDGEOPTIMISE
	glBegin( GL_QUADS );
	while(firstedge)
//	for (firstedge = 0; firstedge < cl.worldmodel->numedges; firstedge++)
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

		//Now draw the quad from the two verts to the projected light
		//verts
		while (edge[firstedge].count > 0)
		{
			glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
			glVertex3f( v2[0]+v4[0], v2[1]+v4[1], v2[2]+v4[2] );
			glVertex3f( v2[0], v2[1], v2[2] );
			glVertex3f( v1[0], v1[1], v1[2] );
			edge[firstedge].count--;
		}
		while (edge[firstedge].count < 0)
		{
			glVertex3f( v1[0], v1[1], v1[2] );
			glVertex3f( v2[0], v2[1], v2[2] );
			glVertex3f( v2[0]+v4[0], v2[1]+v4[1], v2[2]+v4[2] );
			glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
			edge[firstedge].count++;
		}

		firstedge = edge[firstedge].next;
	}
	glEnd();
	for (firstedge = 0; firstedge < cl.worldmodel->numedges; firstedge++)
		edge[firstedge].count = 0;
	firstedge=0;
#endif
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
#define PROJECTION_DISTANCE (float)0x7fffffff
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

	if (cl.worldmodel->fromgame == fg_quake3)
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

#ifdef _DEBUG
	if (r_shadows.value == 666)	//testing (visible shadow volumes)
	{
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glColor3f(dl->color[0], dl->color[1], dl->color[2]);
		glDisable(GL_STENCIL_TEST);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		PPL_RecursiveWorldNode(dl);
		PPL_DrawShadowMeshes(dl);
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

	PPL_BaseTextures(cl.worldmodel);
	PPL_BaseEntTextures();
//	CL_NewDlightRGB(1, r_refdef.vieworg[0], r_refdef.vieworg[1]-16, r_refdef.vieworg[2]-24, 128, 1, 1, 1, 1);
	if (r_shadows.value && glStencilFunc)
	{
		if (cl.worldmodel->fromgame == fg_quake || cl.worldmodel->fromgame == fg_halflife || cl.worldmodel->fromgame == fg_quake2 /*|| cl.worldmodel->fromgame == fg_quake3*/)
		{
			for (l = cl_dlights, i=0 ; i<MAX_DLIGHTS ; i++, l++)
			{
				if (l->die < cl.time || !l->radius || l->noppl)
					continue;
				if (l->color[0] < 0 || l->color[1] < 0 || l->color[2] < 0)
					continue;
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
	}
	PPL_DrawEntFullBrights();
}
#endif
