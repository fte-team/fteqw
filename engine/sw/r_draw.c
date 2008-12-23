/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// r_draw.c

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"	// FIXME: shouldn't need to include this

#define MAXLEFTCLIPEDGES		100

// !!! if these are changed, they must be changed in asm_draw.h too !!!
#define FULLY_CLIPPED_CACHED	0x80000000
#define FRAMECOUNT_MASK			0x7FFFFFFF

unsigned int	cacheoffset;

int			c_faceclip;					// number of faces clipped

zpointdesc_t	r_zpointdesc;

polydesc_t		r_polydesc;



clipplane_t	*entity_clipplanes;
clipplane_t	view_clipplanes[4];
clipplane_t	world_clipplanes[16];

medge_t			*r_pedge;

qboolean		r_leftclipped, r_rightclipped;
static qboolean	makeleftedge, makerightedge;
qboolean		r_nearzionly;

int		sintable[SINTABLESIZE];
int		intsintable[SINTABLESIZE];

mvertex_t	r_leftenter, r_leftexit;
mvertex_t	r_rightenter, r_rightexit;

typedef struct
{
	float	u,v;
	int		ceilv;
} evert_t;

int				r_emitted;
float			r_nearzi;
float			r_u1, r_v1, r_lzi1;
int				r_ceilv1;

qboolean	r_lastvertvalid;

msurface_t *r_alpha_surfaces;






int				r_skyframe;

msurface_t		*r_skyfaces;
mplane_t		r_skyplanes[6];
mtexinfo_t		r_skytexinfo[6];
mvertex_t		*r_skyverts;
medge_t			*r_skyedges;
int				*r_skysurfedges;

// I just copied this code from q2...
int skybox_planes[12] = {2,-128, 0,-128, 2,128, 1,128, 0,128, 1,-128};

int box_surfedges[24] = { 1,2,3,4,  -1,5,6,7,  8,9,-6,10,  -2,-7,-9,11,
  12,-3,-11,-8,  -12,-10,-5,-4};
int box_edges[24] = { 1,2, 2,3, 3,4, 4,1, 1,5, 5,6, 6,2, 7,8, 8,6, 5,7, 8,3, 7,4};

int	box_faces[6] = {0,0,2,2,2,0};

vec3_t	box_vecs[6][2] = {
	{	{0,-1,0}, {-1,0,0} },
	{ {0,1,0}, {0,0,-1} },
	{	{0,-1,0}, {1,0,0} },
	{ {1,0,0}, {0,0,-1} },
	{ {0,-1,0}, {0,0,-1} },
	{ {-1,0,0}, {0,0,-1} }
};

float	box_verts[8][3] = {
	{-1,-1,-1},
	{-1,1,-1},
	{1,1,-1},
	{1,-1,-1},
	{-1,-1,1},
	{-1,1,1},
	{1,-1,1},
	{1,1,1}
};

// down, west, up, north, east, south
// {"rt", "bk", "lf", "ft", "up", "dn"};
static char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
int	r_skysideimage[6] = {5, 2, 4, 1, 0, 3};
extern	mtexinfo_t		r_skytexinfo[6];

char skyname[64];

/*
================
R_LoadSkyBox
================
*/
void SWR_LoadSkyBox (void)
{
#ifdef Q2BSPS
	void *Mod_LoadWall(char *name);
	int i;
	char	pathname[MAX_QPATH];
	for (i=0 ; i<6 ; i++)
	{
		snprintf (pathname, MAX_QPATH-1, "env/%s%s.tga", skyname, suf[r_skysideimage[i]]);
		r_skytexinfo[i].texture = Mod_LoadWall (pathname);	//preferable
		if (!r_skytexinfo[i].texture)
		{
			snprintf (pathname, MAX_QPATH-1, "env/%s%s.pcx", skyname, suf[r_skysideimage[i]]);
			r_skytexinfo[i].texture = Mod_LoadWall (pathname);	//q2 fall back
		}
		if (!r_skytexinfo[i].texture)
		{
			// break out and erase skyname so renderer won't render it
			skyname[0] = '\0';
			return;
		}
	}
#endif
}

void SWR_SetSky (char *name, float rotate, vec3_t axis)
{
	int		i;

	Q_strncpyz (skyname, name, sizeof(skyname));
//	skyrotate = rotate;
//	VectorCopy (axis, skyaxis);

	for (i=0 ; i<6 ; i++)
	{
		r_skytexinfo[i].texture = NULL;
	}

	SWR_LoadSkyBox();
}

qboolean SWR_CheckSky (void)
{
	int		i;
	char	pathname[MAX_QPATH];

	if (!*skyname)
		return true;
	for (i=0 ; i<6 ; i++)
	{
		sprintf (pathname, "env/%s%s.pcx", skyname, suf[r_skysideimage[i]]);
		if (COM_FCheckExists(pathname))
		{
			continue;// it exists, don't bother going for a tga version
		}

		sprintf (pathname, "env/%s%s.tga", skyname, suf[r_skysideimage[i]]);
		if (!COM_FCheckExists(pathname))
			return false;

//		if (!CL_CheckOrDownloadFile(pathname, NULL, -1))
//			return false;
	}
	return true;
}

/*
================
R_InitSkyBox

================
*/
void R_InitSkyBox (void)
{
	int		i;
	model_t *wm;

	wm = cl.worldmodel;
	
	if (wm->numsurfaces+6 > MAX_MAP_FACES
		|| wm->numvertexes+8 > MAX_MAP_VERTS
		|| wm->numedges+12 > MAX_MAP_EDGES)
		Host_Error ("InitSkyBox: map overflow");

	r_skyfaces = wm->surfaces + wm->numsurfaces;
//	wm->numsurfaces += 6;
	r_skyverts = wm->vertexes + wm->numvertexes;
//	wm->numvertexes += 8;
	r_skyedges = wm->edges + wm->numedges;
//	wm->numedges += 12;
	r_skysurfedges = wm->surfedges + wm->numsurfedges;
//	wm->numsurfedges += 24;

	memset (r_skyfaces, 0, 6*sizeof(*r_skyfaces));

	for (i=0 ; i<6 ; i++)
	{
		r_skyplanes[i].normal[skybox_planes[i*2]] = 1;
		r_skyplanes[i].dist = skybox_planes[i*2+1];

		VectorCopy (box_vecs[i][0], r_skytexinfo[i].vecs[0]);
		VectorCopy (box_vecs[i][1], r_skytexinfo[i].vecs[1]);

		r_skyfaces[i].plane = &r_skyplanes[i];
		r_skyfaces[i].numedges = 4;
		r_skyfaces[i].flags = box_faces[i] | SURF_DRAWSKYBOX;
		r_skyfaces[i].firstedge = wm->numsurfedges+i*4;
		r_skyfaces[i].texinfo = &r_skytexinfo[i];
		r_skyfaces[i].texturemins[0] = -128;
		r_skyfaces[i].texturemins[1] = -128;
		r_skyfaces[i].extents[0] = 256;
		r_skyfaces[i].extents[1] = 256;
	}

	for (i=0 ; i<24 ; i++)
		if (box_surfedges[i] > 0)
			r_skysurfedges[i] = wm->numedges - 1 + box_surfedges[i];
		else
			r_skysurfedges[i] = -(wm->numedges - 1 + -box_surfedges[i]);

	for(i=0 ; i<12 ; i++)
	{
		r_skyedges[i].v[0] = wm->numvertexes-1+box_edges[i*2+0];
		r_skyedges[i].v[1] = wm->numvertexes-1+box_edges[i*2+1];
		r_skyedges[i].cachededgeoffset = 0;
	}

	Hunk_Check();
}

void SWR_Skyboxname_Callback(struct cvar_s *var, char *oldvalue)
{
	Q_strncpyz (skyname, var->string, sizeof(skyname));
	SWR_LoadSkyBox();
}

/*
================
R_EmitSkyBox
================
*/
qboolean R_EmitSkyBox (void)
{
	int		i, j;
	int		oldkey;

	if (insubmodel)
		return false;		// submodels should never have skies
	if (r_skyframe == r_framecount)
		return true;		// already set this frame

	if (!*skyname)	//none set
		return false;

	r_skyframe = r_framecount;

	// set the eight fake vertexes
	for (i=0 ; i<8 ; i++)
		for (j=0 ; j<3 ; j++)
			r_skyverts[i].position[j] = r_origin[j] + box_verts[i][j]*128;

	// set the six fake planes
	for (i=0 ; i<6 ; i++)
		if (skybox_planes[i*2+1] > 0)
			r_skyplanes[i].dist = r_origin[skybox_planes[i*2]]+128;
		else
			r_skyplanes[i].dist = r_origin[skybox_planes[i*2]]-128;

	// fix texture offseets
	for (i=0 ; i<6 ; i++)
	{
		r_skytexinfo[i].vecs[0][3] = -DotProduct (r_origin, r_skytexinfo[i].vecs[0]);
		r_skytexinfo[i].vecs[1][3] = -DotProduct (r_origin, r_skytexinfo[i].vecs[1]);
	}

	// emit the six faces
	oldkey = r_currentkey;
	r_currentkey = 0x7ffffff0;
 	for (i=0 ; i<6 ; i++)
	{
		R_RenderFace (r_skyfaces + i, 15);
	}
	r_currentkey = oldkey;		// bsp sorting order

	return true;
}




#if	!id386

/*
================
R_EmitEdge
================
*/
void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1)
{
	edge_t	*edge, *pcheck;
	int		u_check;
	float	u, u_step;
	vec3_t	local, transformed;
	float	*world;
	int		v, v2, ceilv0;
	float	scale, lzi0, u0, v0;
	int		side;

	if (r_lastvertvalid)
	{
		u0 = r_u1;
		v0 = r_v1;
		lzi0 = r_lzi1;
		ceilv0 = r_ceilv1;
	}
	else
	{
		world = &pv0->position[0];
	
	// transform and project
		VectorSubtract (world, modelorg, local);
		TransformVector (local, transformed);
	
		if (transformed[2] < NEAR_CLIP)
			transformed[2] = NEAR_CLIP;
	
		lzi0 = 1.0 / transformed[2];
	
	// FIXME: build x/yscale into transform?
		scale = xscale * lzi0;
		u0 = (xcenter + scale*transformed[0]);
		if (u0 < r_refdef.fvrectx_adj)
			u0 = r_refdef.fvrectx_adj;
		if (u0 > r_refdef.fvrectright_adj)
			u0 = r_refdef.fvrectright_adj;
	
		scale = yscale * lzi0;
		v0 = (ycenter - scale*transformed[1]);
		if (v0 < r_refdef.fvrecty_adj)
			v0 = r_refdef.fvrecty_adj;
		if (v0 > r_refdef.fvrectbottom_adj)
			v0 = r_refdef.fvrectbottom_adj;
	
		ceilv0 = (int) ceil(v0);
	}

	world = &pv1->position[0];

// transform and project
	VectorSubtract (world, modelorg, local);
	TransformVector (local, transformed);

	if (transformed[2] < NEAR_CLIP)
		transformed[2] = NEAR_CLIP;

	r_lzi1 = 1.0 / transformed[2];

	scale = xscale * r_lzi1;
	r_u1 = (xcenter + scale*transformed[0]);
	if (r_u1 < r_refdef.fvrectx_adj)
		r_u1 = r_refdef.fvrectx_adj;
	if (r_u1 > r_refdef.fvrectright_adj)
		r_u1 = r_refdef.fvrectright_adj;

	scale = yscale * r_lzi1;
	r_v1 = (ycenter - scale*transformed[1]);
	if (r_v1 < r_refdef.fvrecty_adj)
		r_v1 = r_refdef.fvrecty_adj;
	if (r_v1 > r_refdef.fvrectbottom_adj)
		r_v1 = r_refdef.fvrectbottom_adj;

	if (r_lzi1 > lzi0)
		lzi0 = r_lzi1;

	if (lzi0 > r_nearzi)	// for mipmap finding
		r_nearzi = lzi0;

// for right edges, all we want is the effect on 1/z
	if (r_nearzionly)
		return;

	r_emitted = 1;

	r_ceilv1 = (int) ceil(r_v1);


// create the edge
	if (ceilv0 == r_ceilv1)
	{
	// we cache unclipped horizontal edges as fully clipped
		if (cacheoffset != 0x7FFFFFFF)
		{
			cacheoffset = FULLY_CLIPPED_CACHED |
					(r_framecount & FRAMECOUNT_MASK);
		}

		return;		// horizontal edge
	}

	side = ceilv0 > r_ceilv1;

	edge = edge_p++;

	edge->owner = r_pedge;

	edge->nearzi = lzi0;

	if (side == 0)
	{
	// trailing edge (go from p1 to p2)
		v = ceilv0;
		v2 = r_ceilv1 - 1;

		edge->surfs[0] = surface_p - surfaces;
		edge->surfs[1] = 0;

		u_step = ((r_u1 - u0) / (r_v1 - v0));
		u = u0 + ((float)v - v0) * u_step;
	}
	else
	{
	// leading edge (go from p2 to p1)
		v2 = ceilv0 - 1;
		v = r_ceilv1;

		edge->surfs[0] = 0;
		edge->surfs[1] = surface_p - surfaces;

		u_step = ((u0 - r_u1) / (v0 - r_v1));
		u = r_u1 + ((float)v - r_v1) * u_step;
	}

	edge->u_step = u_step*0x100000;
	edge->u = u*0x100000 + 0xFFFFF;

// we need to do this to avoid stepping off the edges if a very nearly
// horizontal edge is less than epsilon above a scan, and numeric error causes
// it to incorrectly extend to the scan, and the extension of the line goes off
// the edge of the screen
// FIXME: is this actually needed?
	if (edge->u < r_refdef.vrect_x_adj_shift20)
		edge->u = r_refdef.vrect_x_adj_shift20;
	if (edge->u > r_refdef.vrectright_adj_shift20)
		edge->u = r_refdef.vrectright_adj_shift20;

//
// sort the edge in normally
//
	u_check = edge->u;
	if (edge->surfs[0])
		u_check++;	// sort trailers after leaders

	if (!newedges[v] || newedges[v]->u >= u_check)
	{
		edge->next = newedges[v];
		newedges[v] = edge;
	}
	else
	{
		pcheck = newedges[v];
		while (pcheck->next && pcheck->next->u < u_check)
			pcheck = pcheck->next;
		edge->next = pcheck->next;
		pcheck->next = edge;
	}

	edge->nextremove = removeedges[v2];
	removeedges[v2] = edge;
}


/*
================
R_ClipEdge
================
*/
void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip)
{
	float		d0, d1, f;
	mvertex_t	clipvert;

	if (clip)
	{
		do
		{
			d0 = DotProduct (pv0->position, clip->normal) - clip->dist;
			d1 = DotProduct (pv1->position, clip->normal) - clip->dist;

			if (d0 >= 0)
			{
			// point 0 is unclipped
				if (d1 >= 0)
				{
				// both points are unclipped
					continue;
				}

			// only point 1 is clipped

			// we don't cache clipped edges
				cacheoffset = 0x7FFFFFFF;

				f = d0 / (d0 - d1);
				clipvert.position[0] = pv0->position[0] +
						f * (pv1->position[0] - pv0->position[0]);
				clipvert.position[1] = pv0->position[1] +
						f * (pv1->position[1] - pv0->position[1]);
				clipvert.position[2] = pv0->position[2] +
						f * (pv1->position[2] - pv0->position[2]);

				if (clip->leftedge)
				{
					r_leftclipped = true;
					r_leftexit = clipvert;
				}
				else if (clip->rightedge)
				{
					r_rightclipped = true;
					r_rightexit = clipvert;
				}

				R_ClipEdge (pv0, &clipvert, clip->next);
				return;
			}
			else
			{
			// point 0 is clipped
				if (d1 < 0)
				{
				// both points are clipped
				// we do cache fully clipped edges
					if (!r_leftclipped)
						cacheoffset = FULLY_CLIPPED_CACHED |
								(r_framecount & FRAMECOUNT_MASK);
					return;
				}

			// only point 0 is clipped
				r_lastvertvalid = false;

			// we don't cache partially clipped edges
				cacheoffset = 0x7FFFFFFF;

				f = d0 / (d0 - d1);
				clipvert.position[0] = pv0->position[0] +
						f * (pv1->position[0] - pv0->position[0]);
				clipvert.position[1] = pv0->position[1] +
						f * (pv1->position[1] - pv0->position[1]);
				clipvert.position[2] = pv0->position[2] +
						f * (pv1->position[2] - pv0->position[2]);

				if (clip->leftedge)
				{
					r_leftclipped = true;
					r_leftenter = clipvert;
				}
				else if (clip->rightedge)
				{
					r_rightclipped = true;
					r_rightenter = clipvert;
				}

				R_ClipEdge (&clipvert, pv1, clip->next);
				return;
			}
		} while ((clip = clip->next) != NULL);
	}

// add the edge
	R_EmitEdge (pv0, pv1);
}

#endif	// !id386


/*
================
R_EmitCachedEdge
================
*/
void R_EmitCachedEdge (void)
{
	edge_t		*pedge_t;

	pedge_t = (edge_t *)((unsigned long)r_edges + r_pedge->cachededgeoffset);

	if (!pedge_t->surfs[0])
		pedge_t->surfs[0] = surface_p - surfaces;
	else
		pedge_t->surfs[1] = surface_p - surfaces;

	if (pedge_t->nearzi > r_nearzi)	// for mipmap finding
		r_nearzi = pedge_t->nearzi;

	r_emitted = 1;
}


/*
================
R_RenderFace
================
*/
void R_RenderFace (msurface_t *fa, int clipflags)
{
	extern float r_wateralphaval;
	int			i, lindex;
	unsigned	mask;
	mplane_t	*pplane;
	float		distinv;
	vec3_t		p_normal;
	medge_t		*pedges, tedge;
	clipplane_t	*pclip;

	if (fa->texinfo->texture && (fa->texinfo->flags & (SURF_ALPHATEST|SURF_TRANS33|SURF_TRANS66)))
	{
		if (fa->nextalphasurface)
			return;

		fa->nextalphasurface = r_alpha_surfaces;
		r_alpha_surfaces = fa;
		return;
	}

	if (r_wateralphaval != 1.0 && fa->flags & SURF_DRAWTURB)
	{
		if (fa->nextalphasurface)
			return;

		fa->nextalphasurface = r_alpha_surfaces;
		r_alpha_surfaces = fa;
		return;
	}

	if ( fa->texinfo->flags & SURF_SKY)
	{
		if (R_EmitSkyBox ())
			return;
	}

// skip out if no more surfs
	if ((surface_p) >= surf_max)
	{
		r_outofsurfaces++;
		return;
	}

// ditto if not enough edges left, or switch to auxedges if possible
	if ((edge_p + fa->numedges + 4) >= edge_max)
	{
		r_outofedges += fa->numedges;
		return;
	}

	c_faceclip++;

// set up clip planes
	pclip = NULL;

	for (i=3, mask = 0x08 ; i>=0 ; i--, mask >>= 1)
	{
		if (clipflags & mask)
		{
			view_clipplanes[i].next = pclip;
			pclip = &view_clipplanes[i];
		}
	}


// push the edges through
	r_emitted = 0;
	r_nearzi = 0;
	r_nearzionly = false;
	makeleftedge = makerightedge = false;
	pedges = currententity->model->edges;
	r_lastvertvalid = false;

	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = currententity->model->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];

		// if the edge is cached, we can just reuse the edge
			if (!insubmodel)
			{
				if (r_pedge->cachededgeoffset & FULLY_CLIPPED_CACHED)
				{
					if ((r_pedge->cachededgeoffset & FRAMECOUNT_MASK) ==
						r_framecount)
					{
						r_lastvertvalid = false;
						continue;
					}
				}
				else
				{
					if ((((unsigned long)edge_p - (unsigned long)r_edges) >
						 r_pedge->cachededgeoffset) &&
						(((edge_t *)((unsigned long)r_edges +
						 r_pedge->cachededgeoffset))->owner == r_pedge))
					{
						R_EmitCachedEdge ();
						r_lastvertvalid = false;
						continue;
					}
				}
			}

		// assume it's cacheable
			cacheoffset = (qbyte *)edge_p - (qbyte *)r_edges;
			r_leftclipped = r_rightclipped = false;
			R_ClipEdge (&r_pcurrentvertbase[r_pedge->v[0]],
						&r_pcurrentvertbase[r_pedge->v[1]],
						pclip);
			r_pedge->cachededgeoffset = cacheoffset;

			if (r_leftclipped)
				makeleftedge = true;
			if (r_rightclipped)
				makerightedge = true;
			r_lastvertvalid = true;
		}
		else
		{
			lindex = -lindex;
			r_pedge = &pedges[lindex];
		// if the edge is cached, we can just reuse the edge
			if (!insubmodel)
			{
				if (r_pedge->cachededgeoffset & FULLY_CLIPPED_CACHED)
				{
					if ((r_pedge->cachededgeoffset & FRAMECOUNT_MASK) ==
						r_framecount)
					{
						r_lastvertvalid = false;
						continue;
					}
				}
				else
				{
				// it's cached if the cached edge is valid and is owned
				// by this medge_t
					if ((((unsigned long)edge_p - (unsigned long)r_edges) >
						 r_pedge->cachededgeoffset) &&
						(((edge_t *)((unsigned long)r_edges +
						 r_pedge->cachededgeoffset))->owner == r_pedge))
					{
						R_EmitCachedEdge ();
						r_lastvertvalid = false;
						continue;
					}
				}
			}

		// assume it's cacheable
			cacheoffset = (qbyte *)edge_p - (qbyte *)r_edges;
			r_leftclipped = r_rightclipped = false;
			R_ClipEdge (&r_pcurrentvertbase[r_pedge->v[1]],
						&r_pcurrentvertbase[r_pedge->v[0]],
						pclip);
			r_pedge->cachededgeoffset = cacheoffset;

			if (r_leftclipped)
				makeleftedge = true;
			if (r_rightclipped)
				makerightedge = true;
			r_lastvertvalid = true;
		}
	}

// if there was a clip off the left edge, add that edge too
// FIXME: faster to do in screen space?
// FIXME: share clipped edges?
	if (makeleftedge)
	{
		r_pedge = &tedge;
		r_lastvertvalid = false;
		R_ClipEdge (&r_leftexit, &r_leftenter, pclip->next);
	}

// if there was a clip off the right edge, get the right r_nearzi
	if (makerightedge)
	{
		r_pedge = &tedge;
		r_lastvertvalid = false;
		r_nearzionly = true;
		R_ClipEdge (&r_rightexit, &r_rightenter, view_clipplanes[1].next);
	}

// if no edges made it out, return without posting the surface
	if (!r_emitted)
		return;

	r_polycount++;

	surface_p->data = (void *)fa;
	surface_p->nearzi = r_nearzi;
	surface_p->flags = fa->flags;
	surface_p->insubmodel = insubmodel;
	surface_p->spanstate = 0;
	surface_p->entity = currententity;
	surface_p->key = r_currentkey++;
	surface_p->spans = NULL;

	pplane = fa->plane;
// FIXME: cache this?
	TransformVector (pplane->normal, p_normal);
// FIXME: cache this?
	distinv = 1.0 / (pplane->dist - DotProduct (modelorg, pplane->normal));

	surface_p->d_zistepu = p_normal[0] * xscaleinv * distinv;
	surface_p->d_zistepv = -p_normal[1] * yscaleinv * distinv;
	surface_p->d_ziorigin = p_normal[2] * distinv -
			xcenter * surface_p->d_zistepu -
			ycenter * surface_p->d_zistepv;

//JDC	VectorCopy (r_worldmodelorg, surface_p->modelorg);
	surface_p++;
}


/*
================
R_RenderBmodelFace
================
*/
void R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf)
{
	int			i;
	unsigned	mask;
	mplane_t	*pplane;
	float		distinv;
	vec3_t		p_normal;
	medge_t		tedge;
	clipplane_t	*pclip;

// skip out if no more surfs
	if (surface_p >= surf_max)
	{
		r_outofsurfaces++;
		return;
	}

	if (psurf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66|SURF_ALPHATEST))
	{
		if (psurf->nextalphasurface)
			return;

		psurf->nextalphasurface = r_alpha_surfaces;
		r_alpha_surfaces = psurf;
		return;
	}

// ditto if not enough edges left, or switch to auxedges if possible
	if ((edge_p + psurf->numedges + 4) >= edge_max)
	{
		r_outofedges += psurf->numedges;
		return;
	}

	c_faceclip++;

// this is a dummy to give the caching mechanism someplace to write to
	r_pedge = &tedge;

// set up clip planes
	pclip = NULL;

	for (i=3, mask = 0x08 ; i>=0 ; i--, mask >>= 1)
	{
		if (r_clipflags & mask)
		{
			view_clipplanes[i].next = pclip;
			pclip = &view_clipplanes[i];
		}
	}

// push the edges through
	r_emitted = 0;
	r_nearzi = 0;
	r_nearzionly = false;
	makeleftedge = makerightedge = false;
// FIXME: keep clipped bmodel edges in clockwise order so last vertex caching
// can be used?
	r_lastvertvalid = false;

	for ( ; pedges ; pedges = pedges->pnext)
	{
		r_leftclipped = r_rightclipped = false;
		R_ClipEdge (pedges->v[0], pedges->v[1], pclip);

		if (r_leftclipped)
			makeleftedge = true;
		if (r_rightclipped)
			makerightedge = true;
	}

// if there was a clip off the left edge, add that edge too
// FIXME: faster to do in screen space?
// FIXME: share clipped edges?
	if (makeleftedge)
	{
		r_pedge = &tedge;
		R_ClipEdge (&r_leftexit, &r_leftenter, pclip->next);
	}

// if there was a clip off the right edge, get the right r_nearzi
	if (makerightedge)
	{
		r_pedge = &tedge;
		r_nearzionly = true;
		R_ClipEdge (&r_rightexit, &r_rightenter, view_clipplanes[1].next);
	}

// if no edges made it out, return without posting the surface
	if (!r_emitted)
		return;

	r_polycount++;

	surface_p->data = (void *)psurf;
	surface_p->nearzi = r_nearzi;
	surface_p->flags = psurf->flags;
	surface_p->insubmodel = true;
	surface_p->spanstate = 0;
	surface_p->entity = currententity;
	surface_p->key = r_currentbkey;
	surface_p->spans = NULL;

	pplane = psurf->plane;
// FIXME: cache this?
	TransformVector (pplane->normal, p_normal);
// FIXME: cache this?
	distinv = 1.0 / (pplane->dist - DotProduct (modelorg, pplane->normal));

	surface_p->d_zistepu = p_normal[0] * xscaleinv * distinv;
	surface_p->d_zistepv = -p_normal[1] * yscaleinv * distinv;
	surface_p->d_ziorigin = p_normal[2] * distinv -
			xcenter * surface_p->d_zistepu -
			ycenter * surface_p->d_zistepv;

//JDC	VectorCopy (r_worldmodelorg, surface_p->modelorg);
	surface_p++;
}


/*
================
R_RenderPoly
================
*/
void R_RenderPoly (msurface_t *fa, int clipflags)
{
	int			i, lindex, lnumverts, s_axis, t_axis;
	float		dist, lastdist, lzi, scale, u, v, frac;
	unsigned	mask;
	vec3_t		local, transformed;
	clipplane_t	*pclip;
	medge_t		*pedges;
	mplane_t	*pplane;
	mvertex_t	verts[2][100];	//FIXME: do real number
	polyvert_t	pverts[100];	//FIXME: do real number, safely
	int			vertpage, newverts, newpage, lastvert;
	qboolean	visible;

// FIXME: clean this up and make it faster
// FIXME: guard against running out of vertices

	s_axis = t_axis = 0;	// keep compiler happy

// set up clip planes
	pclip = NULL;

	for (i=3, mask = 0x08 ; i>=0 ; i--, mask >>= 1)
	{
		if (clipflags & mask)
		{
			view_clipplanes[i].next = pclip;
			pclip = &view_clipplanes[i];
		}
	}

// reconstruct the polygon
// FIXME: these should be precalculated and loaded off disk
	pedges = currententity->model->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currententity->model->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			verts[0][i] = r_pcurrentvertbase[r_pedge->v[0]];
		}
		else
		{
			r_pedge = &pedges[-lindex];
			verts[0][i] = r_pcurrentvertbase[r_pedge->v[1]];
		}
	}

// clip the polygon, done if not visible
	while (pclip)
	{
		lastvert = lnumverts - 1;
		lastdist = DotProduct (verts[vertpage][lastvert].position,
							   pclip->normal) - pclip->dist;

		visible = false;
		newverts = 0;
		newpage = vertpage ^ 1;

		for (i=0 ; i<lnumverts ; i++)
		{
			dist = DotProduct (verts[vertpage][i].position, pclip->normal) -
					pclip->dist;

			if ((lastdist > 0) != (dist > 0))
			{
				frac = dist / (dist - lastdist);
				verts[newpage][newverts].position[0] =
						verts[vertpage][i].position[0] +
						((verts[vertpage][lastvert].position[0] -
						  verts[vertpage][i].position[0]) * frac);
				verts[newpage][newverts].position[1] =
						verts[vertpage][i].position[1] +
						((verts[vertpage][lastvert].position[1] -
						  verts[vertpage][i].position[1]) * frac);
				verts[newpage][newverts].position[2] =
						verts[vertpage][i].position[2] +
						((verts[vertpage][lastvert].position[2] -
						  verts[vertpage][i].position[2]) * frac);
				newverts++;
			}

			if (dist >= 0)
			{
				verts[newpage][newverts] = verts[vertpage][i];
				newverts++;
				visible = true;
			}

			lastvert = i;
			lastdist = dist;
		}

		if (!visible || (newverts < 3))
			return;

		lnumverts = newverts;
		vertpage ^= 1;
		pclip = pclip->next;
	}

// transform and project, remembering the z values at the vertices and
// r_nearzi, and extract the s and t coordinates at the vertices
	pplane = fa->plane;
	switch (pplane->type)
	{
	case PLANE_X:
	case PLANE_ANYX:
		s_axis = 1;
		t_axis = 2;
		break;
	case PLANE_Y:
	case PLANE_ANYY:
		s_axis = 0;
		t_axis = 2;
		break;
	case PLANE_Z:
	case PLANE_ANYZ:
		s_axis = 0;
		t_axis = 1;
		break;
	}

	r_nearzi = 0;

	for (i=0 ; i<lnumverts ; i++)
	{
	// transform and project
		VectorSubtract (verts[vertpage][i].position, modelorg, local);
		TransformVector (local, transformed);

		if (transformed[2] < NEAR_CLIP)
			transformed[2] = NEAR_CLIP;

		lzi = 1.0 / transformed[2];

		if (lzi > r_nearzi)	// for mipmap finding
			r_nearzi = lzi;

	// FIXME: build x/yscale into transform?
		scale = xscale * lzi;
		u = (xcenter + scale*transformed[0]);
		if (u < r_refdef.fvrectx_adj)
			u = r_refdef.fvrectx_adj;
		if (u > r_refdef.fvrectright_adj)
			u = r_refdef.fvrectright_adj;

		scale = yscale * lzi;
		v = (ycenter - scale*transformed[1]);
		if (v < r_refdef.fvrecty_adj)
			v = r_refdef.fvrecty_adj;
		if (v > r_refdef.fvrectbottom_adj)
			v = r_refdef.fvrectbottom_adj;

		pverts[i].u = u;
		pverts[i].v = v;
		pverts[i].zi = lzi;
		pverts[i].s = verts[vertpage][i].position[s_axis];
		pverts[i].t = verts[vertpage][i].position[t_axis];
	}

// build the polygon descriptor, including fa, r_nearzi, and u, v, s, t, and z
// for each vertex
	r_polydesc.numverts = lnumverts;
	r_polydesc.nearzi = r_nearzi;
	r_polydesc.pcurrentface = fa;
	r_polydesc.pverts = pverts;

// draw the polygon
	D_DrawPoly ();
}


/*
================
R_ZDrawSubmodelPolys
================
*/
void R_ZDrawSubmodelPolys (model_t *pmodel)
{
	int			i, numsurfaces;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;

	psurf = &pmodel->surfaces[pmodel->firstmodelsurface];
	numsurfaces = pmodel->nummodelsurfaces;

	for (i=0 ; i<numsurfaces ; i++, psurf++)
	{
	// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

	// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
		// FIXME: use bounding-box-based frustum clipping info?
			R_RenderPoly (psurf, 15);
		}
	}
}
























//this code from Quake2

typedef struct
{
	int                     nump;
	emitpoint_t     *pverts;
	qbyte            *pixels;                        // image
	int                     pixel_width;            // image width
	int         pixel_height;       // image height
	vec3_t          vup, vright, vpn;       // in worldspace, for plane eq
	float       dist;
	float       s_offset, t_offset;
	float       viewer_position[3];
	void       (*drawspanlet)( void );
	int         stipple_parity;
	int alpha;
} q2polydesc_t;

q2polydesc_t r_q2polydesc;
static espan_t	*s_polygon_spans;
static int		clip_current;
static int		s_minindex, s_maxindex;
vec5_t	r_clip_verts[2][MAXWORKINGVERTS+2];



model_t *currentmodel;

#define AFFINE_SPANLET_SIZE      16
#define AFFINE_SPANLET_SIZE_BITS 4

typedef struct
{
	qbyte     *pbase, *pdest;
	short	 *pz;
	fixed16_t s, t;
	fixed16_t sstep, tstep;
	int       izi, izistep, izistep_times_2;
	int       spancount;
	unsigned  u, v;
} spanletvars_t;

spanletvars_t s_spanletvars;

void R_DrawSpanletConstant( void )
{
	do
	{
		if (*s_spanletvars.pz <= (s_spanletvars.izi >> 16))
		{
			*s_spanletvars.pdest = 15;//r_polyblendcolor;
		}

		s_spanletvars.izi += s_spanletvars.izistep;
		s_spanletvars.pdest++;
		s_spanletvars.pz++;
	} while (--s_spanletvars.spancount > 0);
}

void R_16DrawSpanlet33Stipple( void )
{
	unsigned btemp;
	unsigned short    *pdest = (unsigned short    *)s_spanletvars.pdest;
	short   *pz    = s_spanletvars.pz;
	int      izi   = s_spanletvars.izi;
	
	if ( r_q2polydesc.stipple_parity ^ ( s_spanletvars.v & 1 ) )
	{
		s_spanletvars.pdest += s_spanletvars.spancount;
		s_spanletvars.pz    += s_spanletvars.spancount;

		if ( s_spanletvars.spancount == AFFINE_SPANLET_SIZE )
			s_spanletvars.izi += s_spanletvars.izistep << AFFINE_SPANLET_SIZE_BITS;
		else
			s_spanletvars.izi += s_spanletvars.izistep * s_spanletvars.izistep;
		
		if ( r_q2polydesc.stipple_parity ^ ( s_spanletvars.u & 1 ) )
		{
			izi += s_spanletvars.izistep;
			s_spanletvars.s   += s_spanletvars.sstep;
			s_spanletvars.t   += s_spanletvars.tstep;

			pdest++;
			pz++;
			s_spanletvars.spancount--;
		}

		s_spanletvars.sstep *= 2;
		s_spanletvars.tstep *= 2;

		while ( s_spanletvars.spancount > 0 )
		{
			unsigned s = s_spanletvars.s >> 16;
			unsigned t = s_spanletvars.t >> 16;

			btemp = *( s_spanletvars.pbase + ( s ) + ( t * cachewidth ) );
			
			if ( btemp != 255 )
			{
				if ( *pz <= ( izi >> 16 ) )
					*pdest = btemp;
			}
			
			izi               += s_spanletvars.izistep_times_2;
			s_spanletvars.s   += s_spanletvars.sstep;
			s_spanletvars.t   += s_spanletvars.tstep;
			
			pdest += 2;
			pz    += 2;
			
			s_spanletvars.spancount -= 2;
		}
	}
}

void R_8DrawSpanletAlphaTest( void )	//8 bit rendering only
{
	unsigned btemp;

	do
	{
		unsigned ts, tt;

		ts = s_spanletvars.s >> 16;
		tt = s_spanletvars.t >> 16;

		btemp = *(s_spanletvars.pbase + (ts) + (tt) * cachewidth);

		if ( btemp != 255 )
		{
			if (*s_spanletvars.pz <= (s_spanletvars.izi >> 16))
			{
				*s_spanletvars.pdest = btemp;
			}
		}

		s_spanletvars.izi += s_spanletvars.izistep;
		s_spanletvars.pdest++;
		s_spanletvars.pz++;
		s_spanletvars.s += s_spanletvars.sstep;
		s_spanletvars.t += s_spanletvars.tstep;
	} while (--s_spanletvars.spancount > 0);
}

void R_8DrawSpanletAlphaBlend( void )	//8 bit rendering only
{
	unsigned btemp;

	D_SetTransLevel(r_q2polydesc.alpha/255.0, BM_BLEND);

	do
	{
		unsigned ts, tt;

		ts = s_spanletvars.s >> 16;
		tt = s_spanletvars.t >> 16;

		btemp = *(s_spanletvars.pbase + (ts) + (tt) * cachewidth);

		if ( btemp != 255 )
		{
			if (*s_spanletvars.pz <= (s_spanletvars.izi >> 16))
			{
				*s_spanletvars.pdest = Trans(*s_spanletvars.pdest, btemp);
			}
		}

		s_spanletvars.izi += s_spanletvars.izistep;
		s_spanletvars.pdest++;
		s_spanletvars.pz++;
		s_spanletvars.s += s_spanletvars.sstep;
		s_spanletvars.t += s_spanletvars.tstep;
	} while (--s_spanletvars.spancount > 0);
}

void R_8DrawSpanletTurbulentAlphaBlend( void )
{
	extern int				*r_turb_turb;
	unsigned btemp;

	D_SetTransLevel(r_q2polydesc.alpha/255.0, BM_BLEND);

	do
	{
		unsigned ts, tt;

		ts = ((s_spanletvars.s + r_turb_turb[(s_spanletvars.t>>16)&(CYCLE-1)])>>16)&63;
		tt = ((s_spanletvars.t + r_turb_turb[(s_spanletvars.s>>16)&(CYCLE-1)])>>16)&63;

		btemp = *(s_spanletvars.pbase + (ts) + (tt) * cachewidth);

		if ( btemp != 255 )
		{
			if (*s_spanletvars.pz <= (s_spanletvars.izi >> 16))
			{
				*s_spanletvars.pdest = Trans(*s_spanletvars.pdest, btemp);
			}
		}

		s_spanletvars.izi += s_spanletvars.izistep;
		s_spanletvars.pdest++;
		s_spanletvars.pz++;
		s_spanletvars.s += s_spanletvars.sstep;
		s_spanletvars.t += s_spanletvars.tstep;
	} while (--s_spanletvars.spancount > 0);
}


void R_16DrawSpanletAlphaTest( void )	//16 bit rendering only
{
	unsigned btemp;

	do
	{
		unsigned ts, tt;

		ts = s_spanletvars.s >> 16;
		tt = s_spanletvars.t >> 16;

		btemp = *(s_spanletvars.pbase + (ts) + (tt) * cachewidth);

		if ( btemp != 255 )
		{
			if (*s_spanletvars.pz <= (s_spanletvars.izi >> 16))
			{
				*s_spanletvars.pdest = btemp;
			}
		}

		s_spanletvars.izi += s_spanletvars.izistep;
		s_spanletvars.pdest++;
		s_spanletvars.pz++;
		s_spanletvars.s += s_spanletvars.sstep;
		s_spanletvars.t += s_spanletvars.tstep;
	} while (--s_spanletvars.spancount > 0);
}

void R_32DrawSpanletAlphaTest( void )
{
	unsigned btemp;

	do
	{
		unsigned ts, tt;

		ts = s_spanletvars.s >> 16;
		tt = s_spanletvars.t >> 16;

		btemp = *((int *)s_spanletvars.pbase + (ts) + (tt) * cachewidth);

		if ( btemp &0xff000000 )
		{
			if (*s_spanletvars.pz <= (s_spanletvars.izi >> 16))
			{
				*(int*)s_spanletvars.pdest = btemp;
			}
		}

		s_spanletvars.izi += s_spanletvars.izistep;
		s_spanletvars.pdest+=4;
		s_spanletvars.pz++;
		s_spanletvars.s += s_spanletvars.sstep;
		s_spanletvars.t += s_spanletvars.tstep;
	} while (--s_spanletvars.spancount > 0);
}
void R_32DrawSpanletBlended( void )
{
	unsigned *btemp;

	do
	{
		unsigned ts, tt;

		ts = s_spanletvars.s >> 16;
		tt = s_spanletvars.t >> 16;

		btemp = ((unsigned int *)s_spanletvars.pbase + (ts) + (tt) * cachewidth);

		if ( ((qbyte *)btemp)[3] )
		{
			if (*s_spanletvars.pz <= (s_spanletvars.izi >> 16))
			{
				((qbyte *)s_spanletvars.pdest)[0] = (((qbyte *)s_spanletvars.pdest)[0]*(255-r_q2polydesc.alpha) + ((qbyte *)btemp)[2]*r_q2polydesc.alpha)/255;
				((qbyte *)s_spanletvars.pdest)[1] = (((qbyte *)s_spanletvars.pdest)[1]*(255-r_q2polydesc.alpha) + ((qbyte *)btemp)[1]*r_q2polydesc.alpha)/255;
				((qbyte *)s_spanletvars.pdest)[2] = (((qbyte *)s_spanletvars.pdest)[2]*(255-r_q2polydesc.alpha) + ((qbyte *)btemp)[0]*r_q2polydesc.alpha)/255;
			}
		}

		s_spanletvars.izi += s_spanletvars.izistep;
		s_spanletvars.pdest+=4;
		s_spanletvars.pz++;
		s_spanletvars.s += s_spanletvars.sstep;
		s_spanletvars.t += s_spanletvars.tstep;
	} while (--s_spanletvars.spancount > 0);
}

void R_32DrawSpanletTurbulentBlended( void )
{
	extern int				*r_turb_turb;
	unsigned *btemp;
	int	     sturb, tturb;
	do
	{
		sturb = ((s_spanletvars.s + r_turb_turb[(s_spanletvars.t>>16)&(CYCLE-1)])>>16)&63;
		tturb = ((s_spanletvars.t + r_turb_turb[(s_spanletvars.s>>16)&(CYCLE-1)])>>16)&63;

		btemp = ( (int *)s_spanletvars.pbase + ( sturb ) + ( tturb << 6 ) );

		if ( *s_spanletvars.pz <= ( s_spanletvars.izi >> 16 ) )
		{
			((qbyte *)s_spanletvars.pdest)[0] = (((qbyte *)s_spanletvars.pdest)[0]*(255-r_q2polydesc.alpha) + ((qbyte *)btemp)[2]*r_q2polydesc.alpha)/255;
			((qbyte *)s_spanletvars.pdest)[1] = (((qbyte *)s_spanletvars.pdest)[1]*(255-r_q2polydesc.alpha) + ((qbyte *)btemp)[1]*r_q2polydesc.alpha)/255;
			((qbyte *)s_spanletvars.pdest)[2] = (((qbyte *)s_spanletvars.pdest)[2]*(255-r_q2polydesc.alpha) + ((qbyte *)btemp)[0]*r_q2polydesc.alpha)/255;
		}

		s_spanletvars.izi += s_spanletvars.izistep;
		s_spanletvars.pdest+=4;
		s_spanletvars.pz++;
		s_spanletvars.s += s_spanletvars.sstep;
		s_spanletvars.t += s_spanletvars.tstep;

	} while ( --s_spanletvars.spancount > 0 );
}

/*
** R_DrawSpanlet66Stipple
*/
void R_16DrawSpanlet66Stipple( void )
{
	unsigned btemp;
	unsigned short    *pdest = (unsigned short    *)s_spanletvars.pdest;
	short   *pz    = s_spanletvars.pz;
	int      izi   = s_spanletvars.izi;

	s_spanletvars.pdest += s_spanletvars.spancount<<1;
	s_spanletvars.pz    += s_spanletvars.spancount;

	if ( s_spanletvars.spancount == AFFINE_SPANLET_SIZE )
		s_spanletvars.izi += s_spanletvars.izistep << AFFINE_SPANLET_SIZE_BITS;
	else
		s_spanletvars.izi += s_spanletvars.izistep * s_spanletvars.izistep;

	if ( r_q2polydesc.stipple_parity ^ ( s_spanletvars.v & 1 ) )
	{
		if ( r_q2polydesc.stipple_parity ^ ( s_spanletvars.u & 1 ) )
		{
			izi += s_spanletvars.izistep;
			s_spanletvars.s += s_spanletvars.sstep;
			s_spanletvars.t += s_spanletvars.tstep;

			pdest++;
			pz++;
			s_spanletvars.spancount--;
		}

		s_spanletvars.sstep *= 2;
		s_spanletvars.tstep *= 2;

		while ( s_spanletvars.spancount > 0 )
		{
			unsigned s = s_spanletvars.s >> 16;
			unsigned t = s_spanletvars.t >> 16;
			
			btemp = vid.colormap16[*( s_spanletvars.pbase + ( s ) + ( t * cachewidth ) )];

			if ( btemp != 255 )
			{
				if ( *pz <= ( izi >> 16 ) )
					*pdest = btemp;
			}
			
			izi             += s_spanletvars.izistep_times_2;
			s_spanletvars.s += s_spanletvars.sstep;
			s_spanletvars.t += s_spanletvars.tstep;
			
			pdest += 2;
			pz    += 2;
			
			s_spanletvars.spancount -= 2;
		}
	}
	else
	{
		while ( s_spanletvars.spancount > 0 )
		{
			unsigned s = s_spanletvars.s >> 16;
			unsigned t = s_spanletvars.t >> 16;
			
			btemp = s+t;//vid.colormap16[*( s_spanletvars.pbase + ( s ) + ( t * cachewidth ) )];
			
			if ( btemp != 255 )
			{
				if ( *pz <= ( izi >> 16 ) )
					*pdest = btemp;
			}
			
			izi             += s_spanletvars.izistep;
			s_spanletvars.s += s_spanletvars.sstep;
			s_spanletvars.t += s_spanletvars.tstep;
			
			pdest++;
			pz++;
			
			s_spanletvars.spancount--;
		}
	}
}

/*
** R_PolygonDrawSpans
*/
// PGM - iswater was qboolean. changed to allow passing more flags
void R_PolygonDrawSpans(espan_t *pspan, int iswater )
{
	extern int				*r_turb_turb;
	int			count;
	fixed16_t	snext, tnext;
	float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float		sdivzspanletstepu, tdivzspanletstepu, zispanletstepu;

	s_spanletvars.pbase = cacheblock;

//PGM
//	if ( iswater & SURF_WARP)
		r_turb_turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));
//	else if (iswater & SURF_FLOWING)
//		r_turb_turb = blanktable;
//PGM

	sdivzspanletstepu = d_sdivzstepu * AFFINE_SPANLET_SIZE;
	tdivzspanletstepu = d_tdivzstepu * AFFINE_SPANLET_SIZE;
	zispanletstepu = d_zistepu * AFFINE_SPANLET_SIZE;

// we count on FP exceptions being turned off to avoid range problems
	s_spanletvars.izistep = (int)(d_zistepu * 0x8000 * 0x10000);
	s_spanletvars.izistep_times_2 = s_spanletvars.izistep * 2;

	s_spanletvars.pz = 0;

	do
	{
		s_spanletvars.pdest   = (qbyte *)d_viewbuffer + r_pixbytes*( d_scantable[pspan->v] /*r_screenwidth * pspan->v*/ + pspan->u);
		s_spanletvars.pz      = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;
		s_spanletvars.u       = pspan->u;
		s_spanletvars.v       = pspan->v;

		count = pspan->count;

		if (count <= 0)
			goto NextSpan;

	// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;

		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
	// we count on FP exceptions being turned off to avoid range problems
		s_spanletvars.izi = (int)(zi * 0x8000 * 0x10000);

		s_spanletvars.s = (int)(sdivz * z) + sadjust;
		s_spanletvars.t = (int)(tdivz * z) + tadjust;

		if ( !iswater )
		{
			if (s_spanletvars.s > bbextents)
				s_spanletvars.s = bbextents;
			else if (s_spanletvars.s < 0)
				s_spanletvars.s = 0;

			if (s_spanletvars.t > bbextentt)
				s_spanletvars.t = bbextentt;
			else if (s_spanletvars.t < 0)
				s_spanletvars.t = 0;
		}

		do
		{
		// calculate s and t at the far end of the span
			if (count >= AFFINE_SPANLET_SIZE )
				s_spanletvars.spancount = AFFINE_SPANLET_SIZE;
			else
				s_spanletvars.spancount = count;

			count -= s_spanletvars.spancount;

			if (count)
			{
			// calculate s/z, t/z, zi->fixed s and t at far end of span,
			// calculate s and t steps across span by shifting
				sdivz += sdivzspanletstepu;
				tdivz += tdivzspanletstepu;
				zi += zispanletstepu;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				tnext = (int)(tdivz * z) + tadjust;

				if ( !iswater )
				{
					if (snext > bbextents)
						snext = bbextents;
					else if (snext < AFFINE_SPANLET_SIZE)
						snext = AFFINE_SPANLET_SIZE;	// prevent round-off error on <0 steps from
									//  from causing overstepping & running off the
									//  edge of the texture

					if (tnext > bbextentt)
						tnext = bbextentt;
					else if (tnext < AFFINE_SPANLET_SIZE)
						tnext = AFFINE_SPANLET_SIZE;	// guard against round-off error on <0 steps
				}

				s_spanletvars.sstep = (snext - s_spanletvars.s) >> AFFINE_SPANLET_SIZE_BITS;
				s_spanletvars.tstep = (tnext - s_spanletvars.t) >> AFFINE_SPANLET_SIZE_BITS;
			}
			else
			{
			// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
			// can't step off polygon), clamp, calculate s and t steps across
			// span by division, biasing steps low so we don't run off the
			// texture
				spancountminus1 = (float)(s_spanletvars.spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				tnext = (int)(tdivz * z) + tadjust;

				if ( !iswater )
				{
					if (snext > bbextents)
						snext = bbextents;
					else if (snext < AFFINE_SPANLET_SIZE)
						snext = AFFINE_SPANLET_SIZE;	// prevent round-off error on <0 steps from
									//  from causing overstepping & running off the
									//  edge of the texture

					if (tnext > bbextentt)
						tnext = bbextentt;
					else if (tnext < AFFINE_SPANLET_SIZE)
						tnext = AFFINE_SPANLET_SIZE;	// guard against round-off error on <0 steps
				}

				if (s_spanletvars.spancount > 1)
				{
					s_spanletvars.sstep = (snext - s_spanletvars.s) / (s_spanletvars.spancount - 1);
					s_spanletvars.tstep = (tnext - s_spanletvars.t) / (s_spanletvars.spancount - 1);
				}
			}

			if ( iswater )
			{
				s_spanletvars.s = s_spanletvars.s & ((CYCLE<<16)-1);
				s_spanletvars.t = s_spanletvars.t & ((CYCLE<<16)-1);
			}

			r_q2polydesc.drawspanlet();

			s_spanletvars.s = snext;
			s_spanletvars.t = tnext;

		} while (count > 0);

NextSpan:
		pspan++;

	} while (pspan->count != DS_SPAN_LIST_END);
}

/*
**
** R_PolygonScanLeftEdge
**
** Goes through the polygon and scans the left edge, filling in 
** screen coordinate data for the spans
*/
void R_PolygonScanLeftEdge (void)
{
	int			i, v, itop, ibottom, lmaxindex;
	emitpoint_t	*pvert, *pnext;
	espan_t		*pspan;
	float		du, dv, vtop, vbottom, slope;
	fixed16_t	u, u_step;

	pspan = s_polygon_spans;
	i = s_minindex;
	if (i == 0)
		i = r_q2polydesc.nump;

	lmaxindex = s_maxindex;
	if (lmaxindex == 0)
		lmaxindex = r_q2polydesc.nump;

	vtop = ceil (r_q2polydesc.pverts[i].v);

	do
	{
		pvert = &r_q2polydesc.pverts[i];
		pnext = pvert - 1;

		vbottom = ceil (pnext->v);

		if (vtop < vbottom)
		{
			du = pnext->u - pvert->u;
			dv = pnext->v - pvert->v;

			slope = du / dv;
			u_step = (int)(slope * 0x10000);
		// adjust u to ceil the integer portion
			u = (int)((pvert->u + (slope * (vtop - pvert->v))) * 0x10000) +
					(0x10000 - 1);
			itop = (int)vtop;
			ibottom = (int)vbottom;

			for (v=itop ; v<ibottom ; v++)
			{
				pspan->u = u >> 16;
				pspan->v = v;
				u += u_step;
				pspan++;
			}
		}

		vtop = vbottom;

		i--;
		if (i == 0)
			i = r_q2polydesc.nump;

	} while (i != lmaxindex);
}

/*
** R_PolygonScanRightEdge
**
** Goes through the polygon and scans the right edge, filling in
** count values.
*/
void R_PolygonScanRightEdge (void)
{
	int			i, v, itop, ibottom;
	emitpoint_t	*pvert, *pnext;
	espan_t		*pspan;
	float		du, dv, vtop, vbottom, slope, uvert, unext, vvert, vnext;
	fixed16_t	u, u_step;

	pspan = s_polygon_spans;
	i = s_minindex;

	vvert = r_q2polydesc.pverts[i].v;
	if (vvert < r_refdef.fvrecty_adj)
		vvert = r_refdef.fvrecty_adj;
	if (vvert > r_refdef.fvrectbottom_adj)
		vvert = r_refdef.fvrectbottom_adj;

	vtop = ceil (vvert);

	do
	{
		pvert = &r_q2polydesc.pverts[i];
		pnext = pvert + 1;

		vnext = pnext->v;
		if (vnext < r_refdef.fvrecty_adj)
			vnext = r_refdef.fvrecty_adj;
		if (vnext > r_refdef.fvrectbottom_adj)
			vnext = r_refdef.fvrectbottom_adj;

		vbottom = ceil (vnext);

		if (vtop < vbottom)
		{
			uvert = pvert->u;
			if (uvert < r_refdef.fvrectx_adj)
				uvert = r_refdef.fvrectx_adj;
			if (uvert > r_refdef.fvrectright_adj)
				uvert = r_refdef.fvrectright_adj;

			unext = pnext->u;
			if (unext < r_refdef.fvrectx_adj)
				unext = r_refdef.fvrectx_adj;
			if (unext > r_refdef.fvrectright_adj)
				unext = r_refdef.fvrectright_adj;

			du = unext - uvert;
			dv = vnext - vvert;
			slope = du / dv;
			u_step = (int)(slope * 0x10000);
		// adjust u to ceil the integer portion
			u = (int)((uvert + (slope * (vtop - vvert))) * 0x10000) +
					(0x10000 - 1);
			itop = (int)vtop;
			ibottom = (int)vbottom;

			for (v=itop ; v<ibottom ; v++)
			{
				pspan->count = (u >> 16) - pspan->u;
				u += u_step;
				pspan++;
			}
		}

		vtop = vbottom;
		vvert = vnext;

		i++;
		if (i == r_q2polydesc.nump)
			i = 0;

	} while (i != s_maxindex);

	pspan->count = DS_SPAN_LIST_END;	// mark the end of the span list 
}

/*
** R_ClipPolyFace
**
** Clips the winding at clip_verts[clip_current] and changes clip_current
** Throws out the back side
*/
int R_ClipPolyFace (int nump, clipplane_t *pclipplane)
{
	int		i, outcount;
	float	dists[MAXWORKINGVERTS+3];
	float	frac, clipdist, *pclipnormal;
	float	*in, *instep, *outstep, *vert2;

	clipdist = pclipplane->dist;
	pclipnormal = pclipplane->normal;
	
// calc dists
	if (clip_current)
	{
		in = r_clip_verts[1][0];
		outstep = r_clip_verts[0][0];
		clip_current = 0;
	}
	else
	{
		in = r_clip_verts[0][0];
		outstep = r_clip_verts[1][0];
		clip_current = 1;
	}
	
	instep = in;
	for (i=0 ; i<nump ; i++, instep += sizeof (vec5_t) / sizeof (float))
	{
		dists[i] = DotProduct (instep, pclipnormal) - clipdist;
	}
	
// handle wraparound case
	dists[nump] = dists[0];
	memcpy (instep, in, sizeof (vec5_t));


// clip the winding
	instep = in;
	outcount = 0;

	for (i=0 ; i<nump ; i++, instep += sizeof (vec5_t) / sizeof (float))
	{
		if (dists[i] >= 0)
		{
			memcpy (outstep, instep, sizeof (vec5_t));
			outstep += sizeof (vec5_t) / sizeof (float);
			outcount++;
		}

		if (dists[i] == 0 || dists[i+1] == 0)
			continue;

		if ( (dists[i] > 0) == (dists[i+1] > 0) )
			continue;
			
	// split it into a new vertex
		frac = dists[i] / (dists[i] - dists[i+1]);
			
		vert2 = instep + sizeof (vec5_t) / sizeof (float);
		
		outstep[0] = instep[0] + frac*(vert2[0] - instep[0]);
		outstep[1] = instep[1] + frac*(vert2[1] - instep[1]);
		outstep[2] = instep[2] + frac*(vert2[2] - instep[2]);
		outstep[3] = instep[3] + frac*(vert2[3] - instep[3]);
		outstep[4] = instep[4] + frac*(vert2[4] - instep[4]);

		outstep += sizeof (vec5_t) / sizeof (float);
		outcount++;
	}	
	
	return outcount;
}

void R_PolygonCalculateGradients (void)
{
	vec3_t		p_normal, p_saxis, p_taxis;
	float		distinv;

	TransformVector (r_q2polydesc.vpn, p_normal);
	TransformVector (r_q2polydesc.vright, p_saxis);
	TransformVector (r_q2polydesc.vup, p_taxis);

	distinv = 1.0 / (-(DotProduct (r_q2polydesc.viewer_position, r_q2polydesc.vpn)) + r_q2polydesc.dist );

	d_sdivzstepu  =  p_saxis[0] * xscaleinv;
	d_sdivzstepv  = -p_saxis[1] * yscaleinv;
	d_sdivzorigin =  p_saxis[2] - xcenter * d_sdivzstepu - ycenter * d_sdivzstepv;

	d_tdivzstepu  =  p_taxis[0] * xscaleinv;
	d_tdivzstepv  = -p_taxis[1] * yscaleinv;
	d_tdivzorigin =  p_taxis[2] - xcenter * d_tdivzstepu - ycenter * d_tdivzstepv;

	d_zistepu =   p_normal[0] * xscaleinv * distinv;
	d_zistepv =  -p_normal[1] * yscaleinv * distinv;
	d_ziorigin =  p_normal[2] * distinv - xcenter * d_zistepu - ycenter * d_zistepv;

	sadjust = (fixed16_t) ( ( DotProduct( r_q2polydesc.viewer_position, r_q2polydesc.vright) + r_q2polydesc.s_offset ) * 0x10000 );
	tadjust = (fixed16_t) ( ( DotProduct( r_q2polydesc.viewer_position, r_q2polydesc.vup   ) + r_q2polydesc.t_offset ) * 0x10000 );

// -1 (-epsilon) so we never wander off the edge of the texture
	bbextents = (r_q2polydesc.pixel_width << 16) - 1;
	bbextentt = (r_q2polydesc.pixel_height << 16) - 1;
}

static void R_DrawPoly( int iswater )
{
	int			i, nump;
	float		ymin, ymax;
	emitpoint_t	*pverts;
	espan_t	spans[MAXHEIGHT+1];

	s_polygon_spans = spans;

// find the top and bottom vertices, and make sure there's at least one scan to
// draw
	ymin = 999999.9;
	ymax = -999999.9;
	pverts = r_q2polydesc.pverts;

	for (i=0 ; i<r_q2polydesc.nump ; i++)
	{
		if (pverts->v < ymin)
		{
			ymin = pverts->v;
			s_minindex = i;
		}

		if (pverts->v > ymax)
		{
			ymax = pverts->v;
			s_maxindex = i;
		}

		pverts++;
	}

	ymin = ceil (ymin);
	ymax = ceil (ymax);

	if (ymin >= ymax)
		return;		// doesn't cross any scans at all

	cachewidth = r_q2polydesc.pixel_width;
	cacheblock = r_q2polydesc.pixels;

// copy the first vertex to the last vertex, so we don't have to deal with
// wrapping
	nump = r_q2polydesc.nump;
	pverts = r_q2polydesc.pverts;
	pverts[nump] = pverts[0];

	R_PolygonCalculateGradients ();
	R_PolygonScanLeftEdge ();
	R_PolygonScanRightEdge ();

	R_PolygonDrawSpans( s_polygon_spans, iswater );
}


void R_ClipAndDrawPoly ( float alpha, int isturbulent, qboolean textured )
{
	emitpoint_t	outverts[MAXWORKINGVERTS+3], *pout;
	float		*pv;
	int			i, nump;
	float		scale;
	vec3_t		transformed, local;

	if (r_pixbytes == 4)
	{
		if (alpha == 1 && !isturbulent)
			r_q2polydesc.drawspanlet = R_32DrawSpanletAlphaTest;
		else if (alpha <= 0)
			return;
		else
		{
			r_q2polydesc.alpha = alpha*255;
			if (isturbulent)
				r_q2polydesc.drawspanlet = R_32DrawSpanletTurbulentBlended;
			else
				r_q2polydesc.drawspanlet = R_32DrawSpanletBlended;
		}
	}
	else if (r_pixbytes == 2)
	{
		if (alpha < 0.2)
			return;
		else if (alpha < 0.5)
			r_q2polydesc.drawspanlet = R_16DrawSpanlet33Stipple;
		else if (alpha < 0.9)
			r_q2polydesc.drawspanlet = R_16DrawSpanlet66Stipple;
		else
			r_q2polydesc.drawspanlet = R_16DrawSpanletAlphaTest;
	}
	else
	{
		if (alpha >= TRANS_UPPER_CAP)
			r_q2polydesc.drawspanlet = R_8DrawSpanletAlphaTest;
		else if (alpha <= TRANS_LOWER_CAP)
			return;
		else if (isturbulent)
		{
			r_q2polydesc.alpha = alpha*255;
			r_q2polydesc.drawspanlet = R_8DrawSpanletTurbulentAlphaBlend;
		}
		else
		{
			r_q2polydesc.alpha = alpha*255;		
			r_q2polydesc.drawspanlet = R_8DrawSpanletAlphaBlend;
		}
	}

	// clip to the frustum in worldspace
	nump = r_q2polydesc.nump;
	clip_current = 0;

	for (i=0 ; i<4 ; i++)
	{
		nump = R_ClipPolyFace (nump, &view_clipplanes[i]);
		if (nump < 3)
			return;
		if (nump > MAXWORKINGVERTS)
			Host_Error("R_ClipAndDrawPoly: too many points: %d", nump );
	}

// transform vertices into viewspace and project
	pv = &r_clip_verts[clip_current][0][0];

	for (i=0 ; i<nump ; i++)
	{
		VectorSubtract (pv, r_origin, local);
		TransformVector (local, transformed);

		if (transformed[2] < NEAR_CLIP)
			transformed[2] = NEAR_CLIP;

		pout = &outverts[i];
		pout->zi = 1.0 / transformed[2];

		pout->s = pv[3];
		pout->t = pv[4];
		
		scale = xscale * pout->zi;
		pout->u = (xcenter+0.5 + scale * transformed[0]);

		scale = yscale * pout->zi;
		pout->v = (ycenter - scale * transformed[1]);

		pv += sizeof (vec5_t) / sizeof (pv);
	}

// draw it
	r_q2polydesc.nump = nump;
	r_q2polydesc.pverts = outverts;

	R_DrawPoly( isturbulent );
}


void R_BuildPolygonFromSurface(msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	vec5_t     *pverts;
	float       tmins[2] = { 0, 0 };

	r_q2polydesc.nump = 0;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	pverts = r_clip_verts[0];

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		VectorCopy (vec, pverts[i] );
	}

	VectorCopy( fa->texinfo->vecs[0], r_q2polydesc.vright );
	VectorCopy( fa->texinfo->vecs[1], r_q2polydesc.vup );
	VectorCopy( fa->plane->normal, r_q2polydesc.vpn );
	VectorCopy( r_origin, r_q2polydesc.viewer_position );

	if ( fa->flags & SURF_PLANEBACK )
	{
		VectorNegate( r_q2polydesc.vpn, r_q2polydesc.vpn );
	}

// PGM 09/16/98
	if ( fa->texinfo->flags & (SURF_WARP|SURF_FLOWING) || (fa->flags & SURF_DRAWTURB) )
	{
		r_q2polydesc.pixels       = (qbyte *)fa->texinfo->texture + fa->texinfo->texture->offsets[0];
		r_q2polydesc.pixel_width  = fa->texinfo->texture->width;
		r_q2polydesc.pixel_height = fa->texinfo->texture->height;
	}
// PGM 09/16/98
	else
	{
		surfcache_t *scache;

		scache = D_CacheSurface( fa, 0 );

		r_q2polydesc.pixels       = scache->data;
		r_q2polydesc.pixel_width  = scache->width;
		r_q2polydesc.pixel_height = scache->height;

		tmins[0] = fa->texturemins[0];
		tmins[1] = fa->texturemins[1];
	}

	r_q2polydesc.dist = DotProduct( r_q2polydesc.vpn, pverts[0] );

	r_q2polydesc.s_offset = fa->texinfo->vecs[0][3] - tmins[0];
	r_q2polydesc.t_offset = fa->texinfo->vecs[1][3] - tmins[1];

	// scrolling texture addition
	if (fa->texinfo->flags & SURF_FLOWING)
	{
		r_q2polydesc.s_offset += -128 * ( (cl.time*0.25) - (int)(cl.time*0.25) );
	}

	r_q2polydesc.nump = lnumverts;
}



void SWR_DrawAlphaSurfaces( void )
{
	msurface_t *s = r_alpha_surfaces, *os;

	currentmodel = r_worldentity.model;

	modelorg[0] = -r_origin[0];
	modelorg[1] = -r_origin[1];
	modelorg[2] = -r_origin[2];

	while ( s )
	{
		R_BuildPolygonFromSurface( s );

		if (s->flags & SURF_DRAWTURB)
		{
			extern float r_wateralphaval;
			R_ClipAndDrawPoly( r_wateralphaval, true, true );
		}
		else if (s->texinfo->flags & SURF_TRANS66)
		{
			R_ClipAndDrawPoly( 0.66f, (s->texinfo->flags & (SURF_WARP|SURF_FLOWING)), true );
		}
		else if (s->texinfo->flags & SURF_TRANS33)
		{
			R_ClipAndDrawPoly( 0.33f, (s->texinfo->flags & (SURF_WARP|SURF_FLOWING)), true );
		}
		else
			R_ClipAndDrawPoly( 1.f, (s->texinfo->flags & (SURF_WARP|SURF_FLOWING)), true );

		os = s;
		s = s->nextalphasurface;
		os->nextalphasurface=NULL;
	}
	
	r_alpha_surfaces = NULL;
}
