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
// r_alias.c: routines for setting up to draw alias models

//changes include stvertexes now being seperatly number from the triangles.
//this allows q2 models to be supported.
//lerping is also available.

//future aims include better skin management.

//the asm code cannot handle alias models anymore.

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"	// FIXME: shouldn't be needed (is needed for patch
						// right now, but that should move)

#define	Q2RF_DEPTHHACK			16		// for view weapon Z crunching

#define LIGHT_MIN	5		// lowest light value we'll allow, to avoid the
							//  need for inner-loop light clamping

mtriangle_t		*ptriangles;
affinetridesc_t	r_affinetridesc;

void *acolormap;	// FIXME: should go away
qbyte *apalremap;

dtrivertx_t		*r_apoldverts;
dtrivertx_t		*r_apnewverts;
vec3_t r_afrntlerp;
vec3_t r_abacklerp;
vec3_t r_amovelerp;

// TODO: these probably will go away with optimized rasterization
mmdl_t				*pmdl;
vec3_t				r_plightvec;
int					r_ambientlight;
float				r_shadelight;
aliashdr_t			*paliashdr;
finalvert_t			*pfinalverts;
auxvert_t			*pauxverts;
static float		ziscale;
static model_t		*pmodel;

extern int cl_playerindex;

static maliasskindesc_t	*pskindesc;

int				r_amodels_drawn;
int				a_skinwidth;
int				r_anumverts;

float	aliastransform[3][4];

typedef struct {
	int	index0;
	int	index1;
} aedge_t;

static aedge_t	aedges[12] = {
{0, 1}, {1, 2}, {2, 3}, {3, 0},
{4, 5}, {5, 6}, {6, 7}, {7, 4},
{0, 5}, {1, 4}, {2, 7}, {3, 6}
};

#define NUMVERTEXNORMALS	162

extern float	r_avertexnormals[NUMVERTEXNORMALS][3];

void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv);//,
	//mstvert_t *pstverts);
void R_AliasSetUpTransform (int trivial_accept);
void R_AliasTransformVector (vec3_t in, vec3_t out);
void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av,
	dtrivertx_t *pnewverts, dtrivertx_t *poldverts);//, mstvert_t *pstverts);
void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av);


/*
================
R_AliasCheckBBox
================
*/
qboolean R_AliasCheckBBox (void)
{
	int					i, flags, nframe, oframe, numv;
	aliashdr_t			*pahdr;
	float				zi, basepts[8][3], v0, v1, frac;
	finalvert_t			*pv0, *pv1, viewpts[16];
	auxvert_t			*pa0, *pa1, viewaux[16];
	maliasframedesc_t	*pnewframedesc, *poldframedesc;
	qboolean			zclipped, zfullyclipped;
	unsigned			anyclip, allclip;
	int					minz;
	float a, b;
	vec3_t min, max;

	
// expand, rotate, and translate points into worldspace

	currententity->trivial_accept = 0;
	pmodel = currententity->model;
	pahdr = SWMod_Extradata (pmodel);
	pmdl = (mmdl_t *)((qbyte *)pahdr + pahdr->model);

	R_AliasSetUpTransform (0);

// construct the base bounding box for this frame
	nframe = currententity->frame;
// TODO: don't repeat this check when drawing?
	if ((nframe >= pmdl->numframes) || (nframe < 0))
	{
		Con_DPrintf ("No such frame %d %s\n", nframe,
				pmodel->name);
		nframe = 0;
	}

// construct the base bounding box for this frame
	oframe = currententity->oldframe;
// TODO: don't repeat this check when drawing?
	if ((oframe >= pmdl->numframes) || (oframe < 0))
	{
		Con_DPrintf ("No such frame %d %s\n", oframe,
				pmodel->name);
		oframe = 0;
	}

	pnewframedesc = &pahdr->frames[nframe];
	poldframedesc = &pahdr->frames[oframe];

	for (i = 0; i < 3; i++)	//choose the most outward of the two.
	{
		a = poldframedesc->scale_origin[i] + poldframedesc->bboxmin.v[i]*poldframedesc->scale[i];
		b = pnewframedesc->scale_origin[i] + pnewframedesc->bboxmin.v[i]*pnewframedesc->scale[i];
		min[i] = a>b?b:a;

		a = poldframedesc->scale_origin[i] + poldframedesc->bboxmax.v[i]*poldframedesc->scale[i];
		b = pnewframedesc->scale_origin[i] + pnewframedesc->bboxmax.v[i]*pnewframedesc->scale[i];
		max[i] = a>b?a:b;
	}

	// x worldspace coordinates
	basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] = min[0];
	basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] = max[0];

	// y worldspace coordinates
	basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] = min[1];
	basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] = max[1];

	// z worldspace coordinates
	basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] = min[2];
	basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] = max[2];				

	zclipped = false;
	zfullyclipped = true;

	minz = 9999;
	for (i=0; i<8 ; i++)
	{
		R_AliasTransformVector  (&basepts[i][0], &viewaux[i].fv[0]);

		if (viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE)
		{
		// we must clip points that are closer than the near clip plane
			viewpts[i].flags = ALIAS_Z_CLIP;
			zclipped = true;
		}
		else
		{
			if (viewaux[i].fv[2] < minz)
				minz = viewaux[i].fv[2];
			viewpts[i].flags = 0;
			zfullyclipped = false;
		}
	}

	
	if (zfullyclipped)
	{
		return false;	// everything was near-z-clipped
	}

	numv = 8;

	if (zclipped)
	{
	// organize points by edges, use edges to get new points (possible trivial
	// reject)
		for (i=0 ; i<12 ; i++)
		{
		// edge endpoints
			pv0 = &viewpts[aedges[i].index0];
			pv1 = &viewpts[aedges[i].index1];
			pa0 = &viewaux[aedges[i].index0];
			pa1 = &viewaux[aedges[i].index1];

		// if one end is clipped and the other isn't, make a new point
			if (pv0->flags ^ pv1->flags)
			{
				frac = (ALIAS_Z_CLIP_PLANE - pa0->fv[2]) /
					   (pa1->fv[2] - pa0->fv[2]);
				viewaux[numv].fv[0] = pa0->fv[0] +
						(pa1->fv[0] - pa0->fv[0]) * frac;
				viewaux[numv].fv[1] = pa0->fv[1] +
						(pa1->fv[1] - pa0->fv[1]) * frac;
				viewaux[numv].fv[2] = ALIAS_Z_CLIP_PLANE;
				viewpts[numv].flags = 0;
				numv++;
			}
		}
	}

// project the vertices that remain after clipping
	anyclip = 0;
	allclip = ALIAS_XY_CLIP_MASK;

// TODO: probably should do this loop in ASM, especially if we use floats
	for (i=0 ; i<numv ; i++)
	{
	// we don't need to bother with vertices that were z-clipped
		if (viewpts[i].flags & ALIAS_Z_CLIP)
			continue;

		zi = 1.0 / viewaux[i].fv[2];

	// FIXME: do with chop mode in ASM, or convert to float
		v0 = (viewaux[i].fv[0] * xscale * zi) + xcenter;
		v1 = (viewaux[i].fv[1] * yscale * zi) + ycenter;

		flags = 0;

		if (v0 < r_refdef.fvrectx)
			flags |= ALIAS_LEFT_CLIP;
		if (v1 < r_refdef.fvrecty)
			flags |= ALIAS_TOP_CLIP;
		if (v0 > r_refdef.fvrectright)
			flags |= ALIAS_RIGHT_CLIP;
		if (v1 > r_refdef.fvrectbottom)
			flags |= ALIAS_BOTTOM_CLIP;

		anyclip |= flags;
		allclip &= flags;
	}

	if (allclip)
		return false;	// trivial reject off one side

	currententity->trivial_accept = !anyclip & !zclipped;

	if (currententity->trivial_accept)
	{
		if (minz > (r_aliastransition + (pmdl->size * r_resfudge)))
		{
			currententity->trivial_accept |= 2;
		}
	}

	return true;
}

/*
================
R_AliasTransformVector
================
*/
void R_AliasTransformVector (vec3_t in, vec3_t out)
{
	out[0] = DotProduct(in, aliastransform[0]) + aliastransform[0][3];
	out[1] = DotProduct(in, aliastransform[1]) + aliastransform[1][3];
	out[2] = DotProduct(in, aliastransform[2]) + aliastransform[2][3];
}


/*
================
R_AliasPreparePoints

General clipped case
================
*/
mstvert_t	*stc;
mtriangle_t	*tn;
void R_AliasPreparePoints (void)
{
	void (*drawfnc) (void);
	int			i;
	mstvert_t	*pstverts;
	finalvert_t	*fv;
	auxvert_t	*av;
	mtriangle_t	*ptri;
	finalvert_t	*pfv[3];

	r_anumverts = pmdl->numverts;
 	fv = pfinalverts;
	av = pauxverts;

	if (r_pixbytes == 4)
		drawfnc = D_PolysetDraw32;
	else if (r_pixbytes == 2)
		drawfnc = D_PolysetDraw16;
	else
	{
#if	id386
		drawfnc = D_PolysetDrawAsm;
#else
		drawfnc = D_PolysetDrawC;
#endif
	}

	for (i=0 ; i<r_anumverts ; i++, fv++, av++, r_apnewverts++, r_apoldverts++)
	{
		R_AliasTransformFinalVert (fv, av, r_apnewverts, r_apoldverts);
		if (av->fv[2] < ALIAS_Z_CLIP_PLANE)
			fv->flags |= ALIAS_Z_CLIP;
		else
		{
			 R_AliasProjectFinalVert (fv, av);

			if (fv->v[0] < r_refdef.aliasvrect.x)
				fv->flags |= ALIAS_LEFT_CLIP;
			if (fv->v[1] < r_refdef.aliasvrect.y)
				fv->flags |= ALIAS_TOP_CLIP;
			if (fv->v[0] > r_refdef.aliasvrectright)
				fv->flags |= ALIAS_RIGHT_CLIP;
			if (fv->v[1] > r_refdef.aliasvrectbottom)
				fv->flags |= ALIAS_BOTTOM_CLIP;	
		}
	}

	stc = pstverts = (mstvert_t *)((qbyte *)paliashdr + paliashdr->stverts);
//
// clip and draw all triangles
//
	r_affinetridesc.numtriangles = 1;

	ptri = (mtriangle_t *)((qbyte *)paliashdr + paliashdr->triangles);
	for (i=0 ; i<pmdl->numtris ; i++, ptri++)
	{
		pfv[0] = &pfinalverts[ptri->xyz_index[0]];
		pfv[1] = &pfinalverts[ptri->xyz_index[1]];
		pfv[2] = &pfinalverts[ptri->xyz_index[2]];

		if ( pfv[0]->flags & pfv[1]->flags & pfv[2]->flags & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP) )
			continue;		// completely clipped

		if ( ! ( (pfv[0]->flags | pfv[1]->flags | pfv[2]->flags) &
			(ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP) ) )
		{	// totally unclipped
			r_affinetridesc.pfinalverts = pfinalverts;
			r_affinetridesc.ptriangles = ptri;

			drawfnc ();
		}
		else		
		{	// partially clipped
			R_AliasClipTriangle (ptri, drawfnc);
		}
	}
}


/*
================
R_AliasSetUpTransform
================
*/
void R_AliasSetUpTransform (int trivial_accept)
{
	int				i;
	float			rotationmatrix[3][4], t2matrix[3][4];
	static float	viewmatrix[3][4];

	for (i=0 ; i<3 ; i++)
	{
		rotationmatrix[i][0] = currententity->axis[0][i]*currententity->scale;
		rotationmatrix[i][1] = currententity->axis[1][i]*currententity->scale;
		rotationmatrix[i][2] = currententity->axis[2][i]*currententity->scale;
	}

	rotationmatrix[0][3] = -modelorg[0];
	rotationmatrix[1][3] = -modelorg[1];
	rotationmatrix[2][3] = -modelorg[2];


// TODO: should be global, set when vright, etc., set
	VectorCopy (vright, viewmatrix[0]);
	VectorCopy (vup, viewmatrix[1]);
	VectorInverse (viewmatrix[1]);
	VectorCopy (vpn, viewmatrix[2]);

//	viewmatrix[0][3] = 0;
//	viewmatrix[1][3] = 0;
//	viewmatrix[2][3] = 0;

	if (currententity->flags & Q2RF_WEAPONMODEL)
	{	//rotate viewmodel to view first
		float	vmmatrix[3][4];
		for (i=0 ; i<3 ; i++)
		{
			t2matrix[i][0] = cl.viewent[r_refdef.currentplayernum].axis[0][i];
			t2matrix[i][1] = cl.viewent[r_refdef.currentplayernum].axis[1][i];
			t2matrix[i][2] = cl.viewent[r_refdef.currentplayernum].axis[2][i];
		}

		t2matrix[0][3] = cl.viewent[r_refdef.currentplayernum].origin[0];
		t2matrix[1][3] = cl.viewent[r_refdef.currentplayernum].origin[1];
		t2matrix[2][3] = cl.viewent[r_refdef.currentplayernum].origin[2];

		R_ConcatTransforms (rotationmatrix, t2matrix, vmmatrix);
		R_ConcatTransforms (viewmatrix, vmmatrix, aliastransform);
	}
	else
		R_ConcatTransforms (viewmatrix, rotationmatrix, aliastransform);


// do the scaling up of x and y to screen coordinates as part of the transform
// for the unclipped case (it would mess up clipping in the clipped case).
// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
// correspondingly so the projected x and y come out right
// FIXME: make this work for clipped case too?
	if (trivial_accept)
	{
		for (i=0 ; i<4 ; i++)
		{
			aliastransform[0][i] *= aliasxscale *
					(1.0 / ((float)0x8000 * 0x10000));
			aliastransform[1][i] *= aliasyscale *
					(1.0 / ((float)0x8000 * 0x10000));
			aliastransform[2][i] *= 1.0 / ((float)0x8000 * 0x10000);

		}
	}
}


/*
================
R_AliasTransformFinalVert
================
*/
void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av,
	dtrivertx_t *pnewverts, dtrivertx_t *poldverts)//, mstvert_t *pstverts)
{
	int		temp;
	float	lightcos, *plightnormal;

	vec3_t	lerp_org;

	lerp_org[0] = r_amovelerp[0] + pnewverts->v[0]*r_afrntlerp[0] + poldverts->v[0]*r_abacklerp[0];
	lerp_org[1] = r_amovelerp[1] + pnewverts->v[1]*r_afrntlerp[1] + poldverts->v[1]*r_abacklerp[1];
	lerp_org[2] = r_amovelerp[2] + pnewverts->v[2]*r_afrntlerp[2] + poldverts->v[2]*r_abacklerp[2];

	av->fv[0] = DotProduct(lerp_org, aliastransform[0]) +
			aliastransform[0][3];
	av->fv[1] = DotProduct(lerp_org, aliastransform[1]) +
			aliastransform[1][3];
	av->fv[2] = DotProduct(lerp_org, aliastransform[2]) +
			aliastransform[2][3];

	fv->v[2] = 0;
	fv->v[3] = 0;

	fv->flags = 0;

// lighting
	plightnormal = r_avertexnormals[pnewverts->lightnormalindex];
	lightcos = DotProduct (plightnormal, r_plightvec);
	temp = r_ambientlight;

	if (lightcos < 0)
	{
		temp += (int)(r_shadelight * lightcos);

	// clamp; because we limited the minimum ambient and shading light, we
	// don't have to clamp low light, just bright
		if (temp < 0)
			temp = 0;
	}

	fv->v[4] = temp;
}


//#if !id386 //since stvert_t was changed.

/*
================
R_AliasTransformAndProjectFinalVerts
================
*/
void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv)//, stvert_t *pstverts)
{
	int			i, temp;
	float		lightcos, *plightnormal, zi;
	dtrivertx_t	*pnewverts, *poldverts;

	vec3_t lerp_org;

	pnewverts = r_apnewverts;
	poldverts = r_apoldverts;

	for (i=0 ; i<r_anumverts ; i++, fv++, pnewverts++, poldverts++)
	{
		lerp_org[0] = r_amovelerp[0] + pnewverts->v[0]*r_afrntlerp[0] + poldverts->v[0]*r_abacklerp[0];
		lerp_org[1] = r_amovelerp[1] + pnewverts->v[1]*r_afrntlerp[1] + poldverts->v[1]*r_abacklerp[1];
		lerp_org[2] = r_amovelerp[2] + pnewverts->v[2]*r_afrntlerp[2] + poldverts->v[2]*r_abacklerp[2];

	// transform and project
		zi = 1.0 / (DotProduct(lerp_org, aliastransform[2]) +
				aliastransform[2][3]);

	// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
	// scaled up by 1/2**31, and the scaling cancels out for x and y in the
	// projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct(lerp_org, aliastransform[0]) +
				aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct(lerp_org, aliastransform[1]) +
				aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = 0;//pstverts->s;
		fv->v[3] = 0;//pstverts->t;
		fv->flags = 0;

	// lighting
		plightnormal = r_avertexnormals[pnewverts->lightnormalindex];	//don't bother lerping light.
		lightcos = DotProduct (plightnormal, r_plightvec);
		temp = r_ambientlight;

		if (lightcos < 0)
		{
			temp += (int)(r_shadelight * lightcos);

		// clamp; because we limited the minimum ambient and shading light, we
		// don't have to clamp low light, just bright
			if (temp < 0)
				temp = 0;
		}

		fv->v[4] = temp;
	}
}

//#endif


/*
================
R_AliasProjectFinalVert
================
*/
void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av)
{
	float	zi;

// project points
	zi = 1.0 / av->fv[2];

	fv->v[5] = zi * ziscale;

	fv->v[0] = (av->fv[0] * aliasxscale * zi) + aliasxcenter;
	fv->v[1] = (av->fv[1] * aliasyscale * zi) + aliasycenter;
}


/*
================
R_AliasPrepareUnclippedPoints
================
*/
void R_AliasPrepareUnclippedPoints (void)
{
	r_anumverts = pmdl->numverts;

	R_AliasTransformAndProjectFinalVerts (pfinalverts);
/*
	if (r_affinetridesc.drawtype)
	{
		if (r_pixbytes == 4)
			D_PolysetDrawFinalVerts32Trans (pfinalverts, r_anumverts);
		else if (r_pixbytes == 2)
			D_PolysetDrawFinalVerts16C (pfinalverts, r_anumverts);
#if 0//id386
		else if (t_state & TT_ONE)
			D_PolysetDrawFinalVertsAsm (pfinalverts, r_anumverts);
#endif
		else
			D_PolysetDrawFinalVertsC (pfinalverts, r_anumverts);
	}
*/
	r_affinetridesc.pfinalverts = pfinalverts;
	r_affinetridesc.ptriangles = (mtriangle_t *)
			((qbyte *)paliashdr + paliashdr->triangles);
	r_affinetridesc.numtriangles = pmdl->numtris;

	if (r_pixbytes == 4)
		D_PolysetDraw32 ();
#if 0//id386
	else if (t_state & TT_ONE)
		D_PolysetDrawAsm ();
#endif
	else
		D_PolysetDrawC ();
}

/*
===============
R_AliasSetupSkin
===============
*/
void R_AliasSetupSkin (void)
{
	int					skinnum;
	int					i, numskins;
	maliasskingroup_t	*paliasskingroup;
	float				*pskinintervals, fullskininterval;
	float				skintargettime, skintime;

	skinnum = currententity->skinnum;
	if ((skinnum >= pmdl->numskins) || (skinnum < 0))
	{
		Con_DPrintf ("R_AliasSetupSkin: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	pskindesc = ((maliasskindesc_t *)
			((qbyte *)paliashdr + paliashdr->skindesc)) + skinnum;
	a_skinwidth = pmdl->skinwidth;

	if (pskindesc->type == ALIAS_SKIN_GROUP)
	{
		paliasskingroup = (maliasskingroup_t *)((qbyte *)paliashdr +
				pskindesc->skin);
		pskinintervals = (float *)
				((qbyte *)paliashdr + paliasskingroup->intervals);
		numskins = paliasskingroup->numskins;
		fullskininterval = pskinintervals[numskins-1];
	
		skintime = cl.time;// + currententity->syncbase;
	
	// when loading in Mod_LoadAliasSkinGroup, we guaranteed all interval
	// values are positive, so we don't have to worry about division by 0
		skintargettime = skintime -
				((int)(skintime / fullskininterval)) * fullskininterval;
	
		for (i=0 ; i<(numskins-1) ; i++)
		{
			if (pskinintervals[i] > skintargettime)
				break;
		}
	
		pskindesc = &paliasskingroup->skindescs[i];
	}

	r_affinetridesc.pskindesc = pskindesc;
	r_affinetridesc.pskin = (void *)((qbyte *)paliashdr + pskindesc->skin);
	r_affinetridesc.skinwidth = a_skinwidth;
	r_affinetridesc.skinheight = pmdl->skinheight;

	if (currententity->model != cl.model_precache[cl_playerindex])
		return;
//alternate player skins.
	if (currententity->scoreboard && r_pixbytes == 1)
	{
		qbyte	*base;
		skin_t	*skin;

		if (!currententity->scoreboard->skin)
			Skin_Find (currententity->scoreboard);
		base = Skin_Cache8 (currententity->scoreboard->skin);
		skin = currententity->scoreboard->skin;
		if (base && skin->cachedbpp == r_pixbytes*8)
		{
			r_affinetridesc.pskin = base;
			r_affinetridesc.skinwidth = skin->width;
			r_affinetridesc.skinheight = skin->height;
		}
	}
	else if (currententity->scoreboard && r_pixbytes == 4)
	{
		qbyte	*base;
		skin_t	*skin;

		if (!currententity->scoreboard->skin)
			Skin_Find (currententity->scoreboard);
		base = Skin_Cache32 (currententity->scoreboard->skin);
		skin = currententity->scoreboard->skin;
		if (base && skin->cachedbpp == r_pixbytes*8)
		{
			r_affinetridesc.pskin = base;
			r_affinetridesc.skinwidth = skin->width;
			r_affinetridesc.skinheight = skin->height;
		}
	}
}

/*
================
R_AliasSetupLighting
================
*/
void R_AliasSetupLighting (alight_t *plighting)
{

	if (r_pixbytes == 4)
	{	//fixes inverse lighting in sw 32.
		//we fix it here so the lighting code doesn't have to have lots of extra minuses, as they are multiplied out
		plighting->ambientlight=(128-plighting->ambientlight);
		plighting->shadelight=(128-plighting->shadelight);
	}

// guarantee that no vertex will ever be lit below LIGHT_MIN, so we don't have
// to clamp off the bottom
	r_ambientlight = plighting->ambientlight;

	r_shadelight = plighting->shadelight;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_ambientlight = (255 - r_ambientlight) << VID_CBITS;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	if (r_shadelight < 0)
		r_shadelight = 0;

	r_shadelight *= VID_GRADES;

// rotate the lighting vector into the model's frame of reference
	r_plightvec[0] = DotProduct (plighting->plightvec, currententity->axis[0]);
	r_plightvec[1] = DotProduct (plighting->plightvec, currententity->axis[1]);
	r_plightvec[2] = DotProduct (plighting->plightvec, currententity->axis[2]);
}

/*
=================
R_AliasSetupFrame

set r_apverts
=================
*/
void R_AliasSetupFrame (void)
{
	int				frame, oframe;
	int				i, numframes;
	maliasgroup_t	*paliasgroup;
	float			*pintervals, fullinterval, targettime, time;

//	float *min1, *min2;
//	vec3_t max1, max2;
	float fl, bl;

	frame = currententity->frame;
	if ((frame >= pmdl->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}
	oframe = currententity->oldframe;
	if ((oframe >= pmdl->numframes) || (oframe < 0))
	{
//		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", oframe);	//pointless
		oframe = 0;
	}

	bl = currententity->lerpfrac;
	fl = 1.0 - bl;

	for (i = 0; i < 3; i++)
	{
		r_abacklerp[i] = paliashdr->frames[oframe].scale[i]*bl;
		r_afrntlerp[i] = paliashdr->frames[frame].scale[i]*fl;
		r_amovelerp[i] = paliashdr->frames[frame].scale_origin[i]*fl + paliashdr->frames[oframe].scale_origin[i]*bl;
	}

	if (paliashdr->frames[frame].type == ALIAS_SINGLE)
	{
		r_apnewverts = (dtrivertx_t *)
				((qbyte *)paliashdr + paliashdr->frames[frame].frame);
	}
	else
	{
		paliasgroup = (maliasgroup_t *)
					((qbyte *)paliashdr + paliashdr->frames[frame].frame);
		pintervals = (float *)((qbyte *)paliashdr + paliasgroup->intervals);
		numframes = paliasgroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = currententity->frame1time;

	//
	// when loading in Mod_LoadAliasGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
	//
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		r_apnewverts = (dtrivertx_t *)
					((qbyte *)paliashdr + paliasgroup->frames[i].frame);
	}

	if (paliashdr->frames[oframe].type == ALIAS_SINGLE)	//things could go haywire here...
	{
		r_apoldverts = (dtrivertx_t *)
				((qbyte *)paliashdr + paliashdr->frames[oframe].frame);
	}
	else
	{
		paliasgroup = (maliasgroup_t *)
					((qbyte *)paliashdr + paliashdr->frames[oframe].frame);
		pintervals = (float *)((qbyte *)paliashdr + paliasgroup->intervals);
		numframes = paliasgroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = currententity->frame1time;

	//
	// when loading in Mod_LoadAliasGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
	//
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		r_apoldverts = (dtrivertx_t *)
					((qbyte *)paliashdr + paliasgroup->frames[i].frame);
	}
}


/*
================
R_AliasDrawModel
================
*/
void R_AliasDrawModel (alight_t *plighting)
{
	finalvert_t		finalverts[MAXALIASVERTS +
						((CACHE_SIZE - 1) / sizeof(finalvert_t)) + 1];
	auxvert_t		auxverts[MAXALIASVERTS];

	extern qbyte transfactor;
	extern qbyte transbackfac;

	if (r_pixbytes == 1)
	{
		if (currententity->shaderRGBAf[3] < TRANS_LOWER_CAP)
			return;

		if (currententity->shaderRGBAf[3] > TRANS_UPPER_CAP)
		{
			transbackfac = 0;
		}
		else
		{
			D_SetTransLevel(currententity->shaderRGBAf[3], BM_BLEND);
			transbackfac = 1;
		}
	}
	else
	{
		transfactor = currententity->shaderRGBAf[3]*255;
		transbackfac = 255 - transfactor;
	}

	r_amodels_drawn++;

// cache align
	pfinalverts = (finalvert_t *)
			(((long)&finalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	pauxverts = &auxverts[0];

	paliashdr = (aliashdr_t *)SWMod_Extradata (currententity->model);
	pmdl = (mmdl_t *)((qbyte *)paliashdr + paliashdr->model);

	R_AliasSetupSkin ();
	R_AliasSetUpTransform (currententity->trivial_accept);
	R_AliasSetupLighting (plighting);
	R_AliasSetupFrame ();

	if (!currententity->palremap)
		currententity->palremap = D_IdentityRemap();
//		Sys_Error ("R_AliasDrawModel: !currententity->colormap");

	r_affinetridesc.drawtype = (currententity->trivial_accept == 3) &&
			r_recursiveaffinetriangles;

	r_affinetridesc.pstverts = (mstvert_t *)((qbyte *)paliashdr + paliashdr->stverts);

	apalremap = currententity->palremap->pal;
	acolormap = vid.colormap;
	if (r_pixbytes == 2)
		acolormap = vid.colormap16;

	if (r_affinetridesc.drawtype)
	{
		D_PolysetUpdateTables ();		// FIXME: precalc...
	}
	else
	{
#if id386
		D_Aff8Patch (acolormap, apalremap);
#endif
	}

	if (currententity == &cl.viewent[r_refdef.currentplayernum] || currententity->flags & Q2RF_DEPTHHACK)
		ziscale = (float)0x8000 * (float)0x10000 * 3.0;
	else
		ziscale = (float)0x8000 * (float)0x10000;

	if (currententity->trivial_accept)
		R_AliasPrepareUnclippedPoints ();
	else
		R_AliasPreparePoints ();
}

