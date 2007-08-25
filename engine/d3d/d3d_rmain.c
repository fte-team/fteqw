#include "quakedef.h"
#ifdef D3DQUAKE
#include "d3dquake.h"

extern mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
extern int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;
extern qbyte			areabits[MAX_Q2MAP_AREAS/8];


int r_visframecount;
entity_t r_worldentity;
refdef_t	r_refdef;
vec3_t	r_origin, vpn, vright, vup;
extern float	r_projection_matrix[16];
extern float	r_view_matrix[16];


//mplane_t	frustum[4];

vec3_t modelorg;

entity_t *currententity;
extern cvar_t gl_mindist;


void	(D3D_R_DeInit)					(void)
{
}
void	(D3D_R_ReInit)					(void)
{
}
void	(D3D_R_Init)					(void)
{
	D3D_R_ReInit();
}

//most of this is a direct copy from gl
void	(D3D_SetupFrame)				(void)
{
	mleaf_t	*leaf;
	vec3_t	temp;

	GLR_AnimateLight();
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	VectorCopy (r_refdef.vieworg, r_origin);
	r_framecount++;

	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;
	r_viewleaf = GLMod_PointInLeaf (cl.worldmodel, r_origin);

	if (!r_viewleaf)
	{
	}
	else if (r_viewleaf->contents == Q1CONTENTS_EMPTY)
	{	//look down a bit			
		VectorCopy (r_origin, temp);
		temp[2] -= 16;
		leaf = GLMod_PointInLeaf (cl.worldmodel, temp);
		if (leaf->contents <= Q1CONTENTS_WATER && leaf->contents >= Q1CONTENTS_LAVA)
			r_viewleaf2 = leaf;
		else
			r_viewleaf2 = NULL;
	}
	else if (r_viewleaf->contents <= Q1CONTENTS_WATER && r_viewleaf->contents >= Q1CONTENTS_LAVA)
	{	//in water, look up a bit.
	
		VectorCopy (r_origin, temp);
		temp[2] += 16;
		leaf = GLMod_PointInLeaf (cl.worldmodel, temp);
		if (leaf->contents == Q1CONTENTS_EMPTY)
			r_viewleaf2 = leaf;
		else
			r_viewleaf2 = NULL;
	}
	else
		r_viewleaf2 = NULL;
	
	if (r_viewleaf)
		V_SetContentsColor (r_viewleaf->contents);
}

void D3D_SetupViewPort(void)
{
	float	screenaspect;
	int glwidth = vid.width, glheight=vid.height;
	int		x, x2, y2, y, w, h;

	float fov_x, fov_y;

	D3DVIEWPORT7 vport;

	D3D_GetBufferSize(&glwidth, &glheight);

	//
	// set up viewpoint
	//
	x = r_refdef.vrect.x * glwidth/(int)vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/(int)vid.width;
	y = (/*vid.height-*/r_refdef.vrect.y) * glheight/(int)vid.height;
	y2 = ((int)/*vid.height - */(r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/(int)vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y < 0)
		y--;
	if (y2 < glheight)
		y2++;

	w = x2 - x;
	h = y2 - y;

/*	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}
*/

	vport.dwX = x;
	vport.dwY = y;
	vport.dwWidth = w;
	vport.dwHeight = h;
	vport.dvMinZ = 0;
	vport.dvMaxZ = 1;
	pD3DDev->lpVtbl->SetViewport(pD3DDev, &vport);

	fov_x = r_refdef.fov_x;//+sin(cl.time)*5;
	fov_y = r_refdef.fov_y;//-sin(cl.time+1)*5;

	if (r_waterwarp.value<0 && r_viewleaf->contents <= Q1CONTENTS_WATER)
	{
		fov_x *= 1 + (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
		fov_y *= 1 + (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
	}

	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
//	if (r_refdef.useperspective)
	{
/*		if ((!r_shadows.value || !gl_canstencil) && gl_maxdist.value>256)//gl_nv_range_clamp)
		{
	//		yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
	//		yfov = (2.0 * tan (scr_fov.value/360*M_PI)) / screenaspect;
	//		yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*(scr_fov.value*2)/M_PI;
	//		MYgluPerspective (yfov,  screenaspect,  4,  4096);
			MYgluPerspective (fov_x, fov_y,  gl_mindist.value,  gl_maxdist.value);
		}
		else*/
		{
			GL_InfinatePerspective(fov_x, fov_y, gl_mindist.value);
		}
	}
/*	else
	{
		if (gl_maxdist.value>=1)
			GL_ParallelPerspective(-fov_x/2, fov_x/2, fov_y/2, -fov_y/2, -gl_maxdist.value, gl_maxdist.value);
		else
			GL_ParallelPerspective(0, r_refdef.vrect.width, 0, r_refdef.vrect.height, -9999, 9999);
	}*/

	Matrix4_ModelViewMatrixFromAxis(r_view_matrix, vpn, vright, vup, r_refdef.vieworg);


	pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_PROJECTION, (D3DMATRIX*)r_projection_matrix);
	pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_VIEW, (D3DMATRIX*)r_view_matrix);


	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZENABLE, D3DZB_TRUE);
}






qbyte *Q1BSP_LeafPVS (model_t *model, mleaf_t *leaf, qbyte *buffer);



//fixme: direct copy from gl (apart from lightmaps)
static void D3D7_RecursiveWorldNode (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;
	int shift;

start:

	if (node->contents == Q1CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox (node->minmaxs, node->minmaxs+3))
		return;

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
				(*mark++)->visframe = r_framecount;
			} while (--c);
		}

	// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);
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
	D3D7_RecursiveWorldNode (node->children[side]);

// draw stuff
  	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		shift = 0;//GLR_LightmapShift(cl.worldmodel);

//		if (dot < 0 -BACKFACE_EPSILON)
//			side = SURF_PLANEBACK;
//		else if (dot > BACKFACE_EPSILON)
//			side = 0;
		{
			for ( ; c ; c--, surf++)
			{
				if (surf->visframe != r_framecount)
					continue;

//				if (((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
//					continue;		// wrong side

				D3DR_RenderDynamicLightmaps (surf, shift);
				// if sorting by texture, just store it out
/*				if (surf->flags & SURF_DRAWALPHA)
				{	// add to the translucent chain
					surf->nextalphasurface = r_alpha_surfaces;
					r_alpha_surfaces = surf;
					surf->ownerent = &r_worldentity;
				}
				else
*/				{
					surf->texturechain = surf->texinfo->texture->texturechain;
					surf->texinfo->texture->texturechain = surf;
				}
			}
		}
	}

// recurse down the back side
//	GLR_RecursiveWorldNode (node->children[!side]);
	node = node->children[!side];
	goto start;
}

struct {
	float x, y, z;
//	unsigned colour;
	float wms, wmt;
	float lms, lmt;
} worldvert[64];
void D3D_DrawTextureChains(void)
{
	texture_t *t;
	msurface_t *s;
	vec3_t *xyz;
	vec2_t *wm;
	vec2_t *lm;
	mesh_t *m;
	int i;
	int v;
	int lmnum;
	extern	int	skytexturenum;		// index in cl.loadmodel, not gl texture object

	if (skytexturenum>=0)
	{
		t = currentmodel->textures[skytexturenum];
		if (t)
		{
			s = t->texturechain;
			if (s)
			{
				t->texturechain = NULL;
				D3D7_DrawSkyChain (s);
			}
		}
	}

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, FALSE );
	for (i = 0; i < currentmodel->numtextures; i++)
	{
		t = currentmodel->textures[i];
		if (!t)
			continue;	//happens on e1m2
		s = t->texturechain;
		if (!s)
			continue;
		t->texturechain = NULL;

pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, (LPDIRECTDRAWSURFACE7)t->gl_texturenum);
pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_TEXCOORDINDEX, 0);

pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAOP,  D3DTOP_SELECTARG1 );
pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );

		while(s)
		{
			m = s->mesh;
			if (m)
			{

				lmnum = s->lightmaptexturenum;
				if (lmnum >= 0)
				{
					if (lightmap[lmnum]->modified)
					{
						DDSURFACEDESC2 desc;
						
						desc.dwSize = sizeof(desc);
						lightmap_d3dtextures[lmnum]->lpVtbl->Lock(lightmap_d3dtextures[lmnum], NULL, &desc, DDLOCK_NOSYSLOCK|DDLOCK_WAIT|DDLOCK_WRITEONLY|DDLOCK_DISCARDCONTENTS, NULL);
						memcpy(desc.lpSurface, lightmap[lmnum]->lightmaps, LMBLOCK_WIDTH*LMBLOCK_HEIGHT*4);
					/*	{
							int i;
							unsigned char *c;
							unsigned char v;
							c = desc.lpSurface;
							for (i = 0; i < LMBLOCK_WIDTH*LMBLOCK_HEIGHT; i++)
							{
								v = rand();
								*c++ = v;
								*c++ = v;
								*c++ = v;
								c++;
							}
						}*/
						lightmap_d3dtextures[lmnum]->lpVtbl->Unlock(lightmap_d3dtextures[lmnum], NULL);

						lightmap[lmnum]->modified = false;
					}


					pD3DDev->lpVtbl->SetTexture(pD3DDev, 1, (LPDIRECTDRAWSURFACE7)lightmap_d3dtextures[lmnum]);
					pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
					pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLORARG2, D3DTA_CURRENT);
					pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLOROP, D3DTOP_MODULATE);
					pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_TEXCOORDINDEX, 1);

					pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_ALPHAOP,  D3DTOP_SELECTARG1 );
					pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				}
				else
				{
					pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
				}

//pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);

				xyz = m->xyz_array;
				wm = m->st_array;
				lm = m->lmst_array;

	for (v = 0; v < m->numvertexes; v++, xyz++, wm++, lm++)
	{
		worldvert[v].x = (*xyz)[0];
		worldvert[v].y = (*xyz)[1];
		worldvert[v].z = (*xyz)[2];
//		worldvert[v].colour = 0;
		worldvert[v].wms = (*wm)[0];
		worldvert[v].wmt = (*wm)[1];
		worldvert[v].lms = (*lm)[0];
		worldvert[v].lmt = (*lm)[1];
	}

				pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_TEX2, worldvert, m->numvertexes, m->indexes, m->numindexes, 0);
			}
			s = s->texturechain;
		}
	}
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE );
}

void D3D_BaseBModelTextures(entity_t *e)
{
	texture_t *t;
	msurface_t *s;
	vec3_t *xyz;
	vec2_t *wm;
	vec2_t *lm;
	mesh_t *m;
	int i;
	int v;
	float matrix[16];
	int lmnum = -1;
	currentmodel = e->model;


	for (s = currentmodel->surfaces+currentmodel->firstmodelsurface, i = 0; i < currentmodel->nummodelsurfaces; i++, s++)
	{
		t = R_TextureAnimation(s->texinfo->texture);

		{
			m = s->mesh;
			if (m)
				D3DR_RenderDynamicLightmaps (s, 0);
		}
	}



	Matrix4_ModelMatrixFromAxis(matrix, e->axis[0], e->axis[1], e->axis[2], e->origin);
	pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX*)matrix);

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, FALSE );
	for (s = currentmodel->surfaces+currentmodel->firstmodelsurface, i = 0; i < currentmodel->nummodelsurfaces; i++, s++)
	{
		t = R_TextureAnimation(s->texinfo->texture);

		pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, (LPDIRECTDRAWSURFACE7)t->gl_texturenum);
		pD3DDev->lpVtbl->SetTexture(pD3DDev, 1, (LPDIRECTDRAWSURFACE7)lightmap_d3dtextures[s->lightmaptexturenum]);
		{
			m = s->mesh;
			if (m)
			{

				if (lmnum != s->lightmaptexturenum)
				{
					lmnum = s->lightmaptexturenum;
					if (lmnum >= 0)
					{
						if (lightmap[lmnum]->modified)
						{
							DDSURFACEDESC2 desc;
							
							desc.dwSize = sizeof(desc);
							lightmap_d3dtextures[lmnum]->lpVtbl->Lock(lightmap_d3dtextures[lmnum], NULL, &desc, DDLOCK_NOSYSLOCK|DDLOCK_WAIT|DDLOCK_WRITEONLY|DDLOCK_DISCARDCONTENTS, NULL);
							memcpy(desc.lpSurface, lightmap[lmnum]->lightmaps, LMBLOCK_WIDTH*LMBLOCK_HEIGHT*4);
						/*	{
								int i;
								unsigned char *c;
								unsigned char v;
								c = desc.lpSurface;
								for (i = 0; i < LMBLOCK_WIDTH*LMBLOCK_HEIGHT; i++)
								{
									v = rand();
									*c++ = v;
									*c++ = v;
									*c++ = v;
									c++;
								}
							}*/
							lightmap_d3dtextures[lmnum]->lpVtbl->Unlock(lightmap_d3dtextures[lmnum], NULL);

							lightmap[lmnum]->modified = false;
						}


						pD3DDev->lpVtbl->SetTexture(pD3DDev, 1, (LPDIRECTDRAWSURFACE7)lightmap_d3dtextures[lmnum]);
						pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
						pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLORARG2, D3DTA_CURRENT);
						pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLOROP, D3DTOP_MODULATE);
						pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_TEXCOORDINDEX, 1);

						pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_ALPHAOP,  D3DTOP_SELECTARG1 );
						pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					}
					else
					{
						pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
					}
				}



				xyz = m->xyz_array;
				wm = m->st_array;
				lm = m->lmst_array;

	for (v = 0; v < m->numvertexes; v++, xyz++, wm++, lm++)
	{
		worldvert[v].x = (*xyz)[0];
		worldvert[v].y = (*xyz)[1];
		worldvert[v].z = (*xyz)[2];
//		worldvert[v].colour = 0;
		worldvert[v].wms = (*wm)[0];
		worldvert[v].wmt = (*wm)[1];
		worldvert[v].lms = (*lm)[0];
		worldvert[v].lmt = (*lm)[1];
	}

				pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_TEX2, worldvert, m->numvertexes, m->indexes, m->numindexes, 0);
			}
		}
	}
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE );

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);


	pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX*)matrix);
}



typedef struct {
	float pos[3];
	int colour;
	float tc[2];
} d3dvert_t;
/*
================
R_GetSpriteFrame
================
*/
/*
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_DrawSprite: no such frame %d (%s)\n", frame, currententity->model->name);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if (psprite->frames[frame].type == SPR_ANGLED)
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[(int)((r_refdef.viewangles[1]-currententity->angles[1])/360*8 + 0.5-4)&7];
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = currententity->frame1time;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}
*/
static void D3D_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	vec3_t		forward, right, up;
	msprite_t		*psprite;

	d3dvert_t d3dvert[4];
	index_t vertindexes[6] = {
		0, 1, 2,
		0, 2, 3
	};

	if (!e->model)
		return;

	if (e->flags & RF_NODEPTHTEST)
		pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS);

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = e->model->cache.data;
//	frame = 0x05b94140;

	switch(psprite->type)
	{
	case SPR_ORIENTED:
		// bullet marks on walls
		AngleVectors (e->angles, forward, right, up);
		break;

	case SPR_FACING_UPRIGHT:
		up[0] = 0;up[1] = 0;up[2]=1;
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalize (right);
		break;
	case SPR_VP_PARALLEL_UPRIGHT:
		up[0] = 0;up[1] = 0;up[2]=1;
		VectorCopy (vright, right);
		break;

	default:
	case SPR_VP_PARALLEL:
		//normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
		break;
	}
	up[0]*=e->scale;
	up[1]*=e->scale;
	up[2]*=e->scale;
	right[0]*=e->scale;
	right[1]*=e->scale;
	right[2]*=e->scale;

	pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, (LPDIRECTDRAWSURFACE7)frame->gl_texturenum);

/*	{
		extern int gldepthfunc;
		qglDepthFunc(gldepthfunc);
		qglDepthMask(0);
		if (gldepthmin == 0.5) 
			qglCullFace ( GL_BACK );
		else
			qglCullFace ( GL_FRONT );

		GL_TexEnv(GL_MODULATE);

		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDisable (GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
	}*/

/*	if (e->flags & Q2RF_ADDATIVE)
	{
		qglEnable(GL_BLEND);
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else if (e->shaderRGBAf[3]<1 || gl_blendsprites.value)
	{
		qglEnable(GL_BLEND);
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
		qglEnable (GL_ALPHA_TEST);
*/

	d3dvert[0].colour = 0xffffffff;
	d3dvert[0].tc[0] = 0;
	d3dvert[0].tc[1] = 1;
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, d3dvert[0].pos);

	d3dvert[1].colour = 0xffffffff;
	d3dvert[1].tc[0] = 0;
	d3dvert[1].tc[0] = 0;
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, d3dvert[1].pos);

	d3dvert[2].colour = 0xffffffff;
	d3dvert[2].tc[0] = 1;
	d3dvert[2].tc[1] = 0;
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, d3dvert[2].pos);

	d3dvert[3].colour = 0xffffffff;
	d3dvert[3].tc[0] = 1;
	d3dvert[3].tc[1] = 1;
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, d3dvert[3].pos);
	

	pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLEFAN, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, d3dvert, 4, vertindexes, 6, 0);


	if (e->flags & RF_NODEPTHTEST)
		pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);

//	if (e->flags & Q2RF_ADDATIVE)	//back to regular blending for us!
//		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//==================================================================================

void D3DR_DrawSprite(void *e, void *parm)
{
	currententity = e;

	D3D_DrawSpriteModel (currententity);

//	P_FlushRenderer();
}



qboolean D3D_ShouldDraw(void)
{
	{
		if (currententity->flags & Q2RF_EXTERNALMODEL)
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


void D3DR_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (!D3D_ShouldDraw())
			continue;


		switch (currententity->rtype)
		{
		case RT_SPRITE:
			RQ_AddDistReorder(D3DR_DrawSprite, currententity, NULL, currententity->origin);
			continue;
#ifdef Q3SHADERS
		case RT_BEAM:
		case RT_RAIL_RINGS:
		case RT_LIGHTNING:
			R_DrawLightning(currententity);
			continue;
		case RT_RAIL_CORE:
			R_DrawRailCore(currententity);
			continue;
#endif
		case RT_MODEL:	//regular model
			break;
		case RT_PORTALSURFACE:
			continue;	//this doesn't do anything anyway, does it?
		default:
		case RT_POLY:	//these are a little painful, we need to do them some time... just not yet.
			continue;
		}
		if (currententity->flags & Q2RF_BEAM)
		{
//			R_DrawBeam(currententity);
			continue;
		}
		if (!currententity->model)
			continue;


		if (cl.lerpents && (cls.allow_anyparticles || currententity->visframe))	//allowed or static
		{
/*			if (gl_part_flame.value)
			{
				if (currententity->model->engineflags & MDLF_ENGULPHS)
					continue;
			}*/
		}

		switch (currententity->model->type)
		{

		case mod_alias:
//			if (r_refdef.flags & Q2RDF_NOWORLDMODEL || !cl.worldmodel || cl.worldmodel->type != mod_brush || cl.worldmodel->fromgame == fg_doom)
				D3D_DrawAliasModel ();
			break;
		
#ifdef HALFLIFEMODELS
		case mod_halflife:
			R_DrawHLModel (currententity);
			break;
#endif

		case mod_brush:
//			if (!cl.worldmodel || cl.worldmodel->type != mod_brush || cl.worldmodel->fromgame == fg_doom)
				D3D_BaseBModelTextures (currententity);
			break;

		case mod_sprite:
			RQ_AddDistReorder(D3DR_DrawSprite, currententity, NULL, currententity->origin);
			break;
/*
#ifdef TERRAIN
		case mod_heightmap:
			D3D_DrawHeightmapModel(currententity);
			break;
#endif
*/
		default:
			break;
		}
	}

	{
		float m_identity[16] = {
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		};
		pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX*)m_identity);
	}
}

void D3D_DrawWorld(void)
{
	RSpeedLocals();
	entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;
	currentmodel = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
#ifdef TERRAIN
// FIXME: Dunno what needs to be fixed here?
//	if (currentmodel->type == mod_heightmap)
//		D3D_DrawHeightmapModel(currententity);
//	else
#endif
	{
//		qglColor3f (1,1,1);
	//#ifdef QUAKE2
//		R_ClearSkyBox ();
	//#endif

		RSpeedRemark();
/*
#ifdef Q2BSPS
		if (ent.model->fromgame == fg_quake2 || ent.model->fromgame == fg_quake3)
		{
			int leafnum;
			int clientarea;
#ifdef QUAKE2
			if (cls.protocol == CP_QUAKE2)	//we can get server sent info
				memcpy(areabits, cl.q2frame.areabits, sizeof(areabits));
			else
#endif
			{	//generate the info each frame.
				leafnum = CM_PointLeafnum (cl.worldmodel, r_refdef.vieworg);
				clientarea = CM_LeafArea (cl.worldmodel, leafnum);
				CM_WriteAreaBits(cl.worldmodel, areabits, clientarea);
			}
#ifdef Q3BSPS
			if (ent.model->fromgame == fg_quake3)
			{
				D3D7_LeafWorldNode ();
			}
			else
#endif
				D3D7_RecursiveQ2WorldNode (cl.worldmodel->nodes);
		}
		else
#endif
*/
			D3D7_RecursiveWorldNode (cl.worldmodel->nodes);

		RSpeedEnd(RSPEED_WORLDNODE);
		TRACE(("dbg: calling PPL_DrawWorld\n"));
//		if (r_shadows.value >= 2 && gl_canstencil && gl_mtexable)
			D3D_DrawTextureChains();
//		else
//			DrawTextureChains (cl.worldmodel, 1, r_refdef.vieworg);


//qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

//		GLR_LessenStains();
	}
}

void D3D_R_RenderScene(void)
{
	if (!cl.worldmodel || (!cl.worldmodel->nodes && cl.worldmodel->type != mod_heightmap))
		r_refdef.flags |= Q2RDF_NOWORLDMODEL;

	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
	{
		R_MarkLeaves ();	// done here so we know if we're in water
		D3D_DrawWorld ();		// adds static entities to the list
	}

	D3DR_DrawEntitiesOnList ();

	P_DrawParticles();
}

void	(D3D_R_RenderView)				(void)
{
	D3D_SetupFrame();
	D3D_SetupViewPort();
	R_SetFrustum();
	D3D_R_RenderScene();
}
#endif
