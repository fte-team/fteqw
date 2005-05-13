#include "quakedef.h"

/*

============================================================================

Physics functions (common)
*/

#if	!id386

/*
==================
SV_HullPointContents

==================
*/
static int Q1_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	float		d;
	dclipnode_t	*node;
	mplane_t	*plane;

	while (num >= 0)
	{
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("SV_HullPointContents: bad node number");
	
		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}
	
	return num;
}
#else
int VARGS Q1_HullPointContents (hull_t *hull, int num, vec3_t p);
#endif	// !id386



#define	DIST_EPSILON	(0.03125)
qboolean Q1BSP_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace)
{
	dclipnode_t	*node;
	mplane_t	*plane;
	float		t1, t2;
	float		frac;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

// check for empty
	if (num < 0)
	{
		if (num != Q1CONTENTS_SOLID)
		{
			trace->allsolid = false;
			if (num == Q1CONTENTS_EMPTY)
				trace->inopen = true;
			else
				trace->inwater = true;
		}
		else
			trace->startsolid = true;
		return true;		// empty
	}

	if (num < hull->firstclipnode || num > hull->lastclipnode)
		Sys_Error ("Q1BSP_RecursiveHullCheck: bad node number");

//
// find the point distances
//
	node = hull->clipnodes + num;
	plane = hull->planes + node->planenum;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
	}
	
#if 1
	if (t1 >= 0 && t2 >= 0)
		return Q1BSP_RecursiveHullCheck (hull, node->children[0], p1f, p2f, p1, p2, trace);
	if (t1 < 0 && t2 < 0)
		return Q1BSP_RecursiveHullCheck (hull, node->children[1], p1f, p2f, p1, p2, trace);
#else
	if ( (t1 >= DIST_EPSILON && t2 >= DIST_EPSILON) || (t2 > t1 && t1 >= 0) )
		return Q1BSP_RecursiveHullCheck (hull, node->children[0], p1f, p2f, p1, p2, trace);
	if ( (t1 <= -DIST_EPSILON && t2 <= -DIST_EPSILON) || (t2 < t1 && t1 <= 0) )
		return Q1BSP_RecursiveHullCheck (hull, node->children[1], p1f, p2f, p1, p2, trace);
#endif

// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < 0)
		frac = (t1 + DIST_EPSILON)/(t1-t2);
	else
		frac = (t1 - DIST_EPSILON)/(t1-t2);
	if (frac < 0)
		frac = 0;
	if (frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f)*frac;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	side = (t1 < 0);

// move up to the node
	if (!Q1BSP_RecursiveHullCheck (hull, node->children[side], p1f, midf, p1, mid, trace) )
		return false;

#ifdef PARANOID
	if (Q1BSP_RecursiveHullCheck (sv_hullmodel, mid, node->children[side])
	== Q1CONTENTS_SOLID)
	{
		Con_Printf ("mid PointInHullSolid\n");
		return false;
	}
#endif
	
	if (Q1_HullPointContents (hull, node->children[side^1], mid)
	!= Q1CONTENTS_SOLID)
// go past the node
		return Q1BSP_RecursiveHullCheck (hull, node->children[side^1], midf, p2f, mid, p2, trace);
	
	if (trace->allsolid)
		return false;		// never got out of the solid area
		
//==================
// the other side of the node is solid, this is the impact point
//==================
	if (!side)
	{
		VectorCopy (plane->normal, trace->plane.normal);
		trace->plane.dist = plane->dist;
	}
	else
	{
		VectorSubtract (vec3_origin, plane->normal, trace->plane.normal);
		trace->plane.dist = -plane->dist;
	}

	while (Q1_HullPointContents (hull, hull->firstclipnode, mid)
	== Q1CONTENTS_SOLID)
	{ // shouldn't really happen, but does occasionally
		frac -= 0.1;
		if (frac < 0)
		{
			trace->fraction = midf;
			VectorCopy (mid, trace->endpos);
			Con_DPrintf ("backup past 0\n");
			return false;
		}
		midf = p1f + (p2f - p1f)*frac;
		for (i=0 ; i<3 ; i++)
			mid[i] = p1[i] + frac*(p2[i] - p1[i]);
	}

	trace->fraction = midf;
	VectorCopy (mid, trace->endpos);

	return false;
}

int Q1BSP_HullPointContents(hull_t *hull, vec3_t p)
{
	switch(Q1_HullPointContents(hull, hull->firstclipnode, p))
	{
	case Q1CONTENTS_EMPTY:
		return FTECONTENTS_EMPTY;
	case Q1CONTENTS_SOLID:
		return FTECONTENTS_SOLID;
	case Q1CONTENTS_WATER:
		return FTECONTENTS_WATER;
	case Q1CONTENTS_SLIME:
		return FTECONTENTS_SLIME;
	case Q1CONTENTS_LAVA:
		return FTECONTENTS_LAVA;
	case Q1CONTENTS_SKY:
		return FTECONTENTS_SKY;
	default:
		Sys_Error("Q1_PointContents: Unknown contents type");
		return FTECONTENTS_SOLID;
	}
}

void Q1BSP_SetHullFuncs(hull_t *hull)
{
	hull->funcs.RecursiveHullCheck = Q1BSP_RecursiveHullCheck;
	hull->funcs.HullPointContents = Q1BSP_HullPointContents;
}

/*
Physics functions (common)

============================================================================

Rendering functions (Client only)
*/
#ifndef SERVERONLY

extern int	r_dlightframecount;

//goes through the nodes marking the surfaces near the dynamic light as lit.
void Q1BSP_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	
	if (node->contents < 0)
		return;	

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;
	
	if (dist > light->radius)
	{
		Q1BSP_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		Q1BSP_MarkLights (light, bit, node->children[1]);
		return;
	}
		
// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	Q1BSP_MarkLights (light, bit, node->children[0]);
	Q1BSP_MarkLights (light, bit, node->children[1]);
}

#define MAXFRAGMENTTRIS 256
vec3_t decalfragmentverts[MAXFRAGMENTTRIS*3];

typedef struct {
	vec3_t center;

	vec3_t normal;
	vec3_t tangent1;
	vec3_t tangent2;

	vec3_t planenorm[6];
	float planedist[6];

	vec_t radius;
	int numtris;

} fragmentdecal_t;

#define FloatInterpolate(a, bness, b, c) (c) = (a)*(1-bness) + (b)*bness
#define VectorInterpolate(a, bness, b, c) FloatInterpolate((a)[0], bness, (b)[0], (c)[0]),FloatInterpolate((a)[1], bness, (b)[1], (c)[1]),FloatInterpolate((a)[2], bness, (b)[2], (c)[2])

//#define SHOWCLIPS
//#define FRAGMENTASTRIANGLES	//works, but produces more fragments.

#ifdef FRAGMENTASTRIANGLES

//if the triangle is clipped away, go recursive if there are tris left.
void Fragment_ClipTriToPlane(int trinum, float *plane, float planedist, fragmentdecal_t *dec)
{
	float *point[3];
	float dotv[3];

	vec3_t impact1, impact2;
	float t;

	int i, i2, i3;
	int clippedverts = 0;

	for (i = 0; i < 3; i++)
	{
		point[i] = decalfragmentverts[trinum*3+i];
		dotv[i] = DotProduct(point[i], plane)-planedist;
		clippedverts += dotv[i] < 0;
	}

	//if they're all clipped away, scrap the tri
	switch (clippedverts)
	{
	case 0:
		return;	//plane does not clip the triangle.

	case 1:	//split into 3, disregard the clipped vert
		for (i = 0; i < 3; i++)
		{
			if (dotv[i] < 0)
			{	//This is the vertex that's getting clipped.

				if (dotv[i] > -DIST_EPSILON)
					return;	//it's only over the line by a tiny ammount.

				i2 = (i+1)%3;
				i3 = (i+2)%3;

				if (dotv[i2] < DIST_EPSILON)
					return;
				if (dotv[i3] < DIST_EPSILON)
					return;

				//work out where the two lines impact the plane
				t = (dotv[i]) / (dotv[i]-dotv[i2]);
				VectorInterpolate(point[i], t, point[i2], impact1);

				t = (dotv[i]) / (dotv[i]-dotv[i3]);
				VectorInterpolate(point[i], t, point[i3], impact2);

#ifdef SHOWCLIPS
				if (dec->numtris != MAXFRAGMENTTRIS)
				{
					VectorCopy(impact2,					decalfragmentverts[dec->numtris*3+0]);
					VectorCopy(decalfragmentverts[trinum*3+i],	decalfragmentverts[dec->numtris*3+1]);
					VectorCopy(impact1,					decalfragmentverts[dec->numtris*3+2]);
					dec->numtris++;
				}
#endif


				//shrink the tri, putting the impact into the killed vertex.
				VectorCopy(impact2, point[i]);


				if (dec->numtris == MAXFRAGMENTTRIS)
					return;	//:(

				//build the second tri
				VectorCopy(impact1,					decalfragmentverts[dec->numtris*3+0]);
				VectorCopy(decalfragmentverts[trinum*3+i2],	decalfragmentverts[dec->numtris*3+1]);
				VectorCopy(impact2,					decalfragmentverts[dec->numtris*3+2]);
				dec->numtris++;

				return;
			}
		}
		Sys_Error("Fragment_ClipTriToPlane: Clipped vertex not founc\n");
		return;	//can't handle it
	case 2:	//split into 3, disregarding both the clipped.
		for (i = 0; i < 3; i++)
		{
			if (!(dotv[i] < 0))
			{	//This is the vertex that's staying.

				if (dotv[i] < DIST_EPSILON)
					break;	//only just inside

				i2 = (i+1)%3;
				i3 = (i+2)%3;

				//work out where the two lines impact the plane
				t = (dotv[i]) / (dotv[i]-dotv[i2]);
				VectorInterpolate(point[i], t, point[i2], impact1);

				t = (dotv[i]) / (dotv[i]-dotv[i3]);
				VectorInterpolate(point[i], t, point[i3], impact2);

				//shrink the tri, putting the impact into the killed vertex.

#ifdef SHOWCLIPS
				if (dec->numtris != MAXFRAGMENTTRIS)
				{
					VectorCopy(impact1,					decalfragmentverts[dec->numtris*3+0]);
					VectorCopy(point[i2],	decalfragmentverts[dec->numtris*3+1]);
					VectorCopy(point[i3],					decalfragmentverts[dec->numtris*3+2]);
					dec->numtris++;
				}
				if (dec->numtris != MAXFRAGMENTTRIS)
				{
					VectorCopy(impact1,					decalfragmentverts[dec->numtris*3+0]);
					VectorCopy(point[i3],	decalfragmentverts[dec->numtris*3+1]);
					VectorCopy(impact2,					decalfragmentverts[dec->numtris*3+2]);
					dec->numtris++;
				}
#endif

				VectorCopy(impact1, point[i2]);
				VectorCopy(impact2, point[i3]);
				return;
			}
		}
	case 3://scrap it
		//fill the verts with the verts of the last and go recursive (due to the nature of Fragment_ClipTriangle, which doesn't actually know if we clip them away)
#ifndef SHOWCLIPS
		dec->numtris--;
		VectorCopy(decalfragmentverts[dec->numtris*3+0], decalfragmentverts[trinum*3+0]);
		VectorCopy(decalfragmentverts[dec->numtris*3+1], decalfragmentverts[trinum*3+1]);
		VectorCopy(decalfragmentverts[dec->numtris*3+2], decalfragmentverts[trinum*3+2]);
		if (trinum < dec->numtris)
			Fragment_ClipTriToPlane(trinum, plane, planedist, dec);
#endif
		return;
	}
}

void Fragment_ClipTriangle(fragmentdecal_t *dec, float *a, float *b, float *c)
{
	//emit the triangle, and clip it's fragments.
	int start, i;

	int p;

	if (dec->numtris == MAXFRAGMENTTRIS)
		return;	//:(

	start = dec->numtris;

	VectorCopy(a, decalfragmentverts[dec->numtris*3+0]);
	VectorCopy(b, decalfragmentverts[dec->numtris*3+1]);
	VectorCopy(c, decalfragmentverts[dec->numtris*3+2]);
	dec->numtris++;

	//clip all the fragments to all of the planes.
	//This will produce a quad if the source triangle was big enough.

	for (p = 0; p < 6; p++)
	{
		for (i = start; i < dec->numtris; i++)
			Fragment_ClipTriToPlane(i, dec->planenorm[p], dec->plantdist[p], dec);
	}
}

#else

#define MAXFRAGMENTVERTS 128
int Fragment_ClipPolyToPlane(float *inverts, float *outverts, int incount, float *plane, float planedist)
{
	float dotv[MAXFRAGMENTVERTS];
	int i, i2, i3;
	int outcount = 0;
	int clippedcount = 0;
	vec3_t impact;

	float *lastvalid;	//the reason these arn't just an index is because it'd need to be a special case for the first vert.
	float lastvaliddot;

	for (i = 0; i < incount; i++)
	{
		dotv[i] = DotProduct((inverts+i*3), plane) - planedist;
		if (dotv[i]<-DIST_EPSILON)
			clippedcount++;
		else
		{
			lastvalid = inverts+i*3;
			lastvaliddot = dotv[i];
		}
	}

	if (clippedcount == incount)
		return 0;	//all were clipped
	if (clippedcount == 0)
	{
		memcpy(outverts, inverts, sizeof(float)*3*incount);
		return incount;
	}

	for (i = 0; i < incount; )
	{
		if (dotv[i] < -DIST_EPSILON)	//clipped
		{
			//work out where the line impacts the plane
			lastvaliddot = (dotv[i]) / (dotv[i]-lastvaliddot);
			VectorInterpolate((inverts+i*3), lastvaliddot, lastvalid, impact);

			if (outcount+1 >= MAXFRAGMENTVERTS)	//bum
				break;

			outverts[outcount*3 + 0] = impact[0];
			outverts[outcount*3 + 1] = impact[1];
			outverts[outcount*3 + 2] = impact[2];
			outcount++;

			i3 = (i+1);
			while (dotv[i3%incount] < -DIST_EPSILON)	//clipped
				i3++;
			i = (i3-1)%incount;
			i2=i3%incount;

			lastvaliddot = (dotv[i]) / (dotv[i]-dotv[i2]);
			VectorInterpolate((inverts+i*3), lastvaliddot, (inverts+i2*3), impact);

			outverts[outcount*3 + 0] = impact[0];
			outverts[outcount*3 + 1] = impact[1];
			outverts[outcount*3 + 2] = impact[2];
			outcount++;
			lastvalid = outverts+outcount*3;
			lastvaliddot = 0;		// :)

			i = i3;
		}
		else
		{	//this vertex wasn't clipped. Just copy to the output.

			if (outcount == MAXFRAGMENTVERTS)	//bum
				break;

			outverts[outcount*3 + 0] = inverts[i*3 + 0];
			outverts[outcount*3 + 1] = inverts[i*3 + 1];
			outverts[outcount*3 + 2] = inverts[i*3 + 2];
			lastvalid = inverts+i*3;
			lastvaliddot = dotv[i];

			outcount++;
			i++;
		}
	}

	return outcount;
}

void Fragment_ClipTriangle(fragmentdecal_t *dec, float *a, float *b, float *c)
{
	//emit the triangle, and clip it's fragments.
	int start, i;
	int p;
	float verts[MAXFRAGMENTVERTS*3];
	float verts2[MAXFRAGMENTVERTS*3];
	int numverts;


	if (dec->numtris == MAXFRAGMENTTRIS)
		return;	//don't bother

	VectorCopy(a, (verts+0*3));
	VectorCopy(b, (verts+1*3));
	VectorCopy(c, (verts+2*3));
	numverts = 3;

	//clip the triangle to the 6 planes.
	for (p = 0; p < 6; p+=2)
	{
		numverts = Fragment_ClipPolyToPlane(verts, verts2, numverts, dec->planenorm[p], dec->planedist[p]);
		if (numverts < 3)	//totally clipped.
			return;

		numverts = Fragment_ClipPolyToPlane(verts2, verts, numverts, dec->planenorm[p+1], dec->planedist[p+1]);
		if (numverts < 3)	//totally clipped.
			return;
	}

	//decompose the resultant polygon into triangles.

	while(numverts>2)
	{
		if (dec->numtris == MAXFRAGMENTTRIS)
			return;

		numverts--;

		VectorCopy((verts+3*0),				decalfragmentverts[dec->numtris*3+0]);
		VectorCopy((verts+3*(numverts-1)),	decalfragmentverts[dec->numtris*3+1]);
		VectorCopy((verts+3*numverts),		decalfragmentverts[dec->numtris*3+2]);
		dec->numtris++;
	}
}

#endif

//this could be inlined, but I'm lazy.
void Q1BSP_FragmentToMesh (fragmentdecal_t *dec, mesh_t *mesh)
{
	int i;

	float *a, *b, *c;

	for (i = 0; i < mesh->numindexes; i+=3)
	{
		if (dec->numtris == MAXFRAGMENTTRIS)
			break;
		a = mesh->xyz_array[mesh->indexes[i+0]];
		b = mesh->xyz_array[mesh->indexes[i+1]];
		c = mesh->xyz_array[mesh->indexes[i+2]];

		Fragment_ClipTriangle(dec, a, b, c);
	}
}

#include "glquake.h"
void Q1BSP_ClipDecalToNodes (fragmentdecal_t *dec, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	
	if (node->contents < 0)
		return;	

	splitplane = node->plane;
	dist = DotProduct (dec->center, splitplane->normal) - splitplane->dist;

	if (dist > dec->radius)
	{
		Q1BSP_ClipDecalToNodes (dec, node->children[0]);
		return;
	}
	if (dist < -dec->radius)
	{
		Q1BSP_ClipDecalToNodes (dec, node->children[1]);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		Q1BSP_FragmentToMesh(dec, surf->mesh);
	}

	Q1BSP_ClipDecalToNodes (dec, node->children[0]);
	Q1BSP_ClipDecalToNodes (dec, node->children[1]);
}

int Q1BSP_ClipDecal(vec3_t center, vec3_t normal, vec3_t tangent, float size, float **out)
{	//quad marks a full, independant quad
	int p;
	fragmentdecal_t dec;

	VectorCopy(center, dec.center);
	VectorCopy(normal, dec.normal);
	VectorCopy(tangent, dec.tangent1);
	CrossProduct(tangent, normal, dec.tangent2);
	dec.radius = size/2;
	dec.numtris = 0;

	VectorCopy(dec.tangent1,	dec.planenorm[0]);
	VectorNegate(dec.tangent1,	dec.planenorm[1]);
	VectorCopy(dec.tangent2,	dec.planenorm[2]);
	VectorNegate(dec.tangent2,	dec.planenorm[3]);
	VectorCopy(dec.normal,		dec.planenorm[4]);
	VectorNegate(dec.normal,	dec.planenorm[5]);
	for (p = 0; p < 6; p++)
		dec.planedist[p] = -(dec.radius - DotProduct(dec.center, dec.planenorm[p]));

	Q1BSP_ClipDecalToNodes(&dec, cl.worldmodel->nodes);

	*out = (float *)decalfragmentverts;
	return dec.numtris;
}

//This is spike's testing function, and is only usable by gl. :)
void Q1BSP_TestClipDecal(void)
{
	/*
	int i;
	int numtris;
	vec3_t fwd;
	vec3_t start;
	vec3_t center, normal, tangent;
	float *verts;

	if (cls.state != ca_active)
		return;

	VectorCopy(cl.simorg[0], start);
	start[2]+=22;
	VectorMA(start, 10000, vpn, fwd);

	TraceLineN(start, fwd, center, normal);

	CrossProduct(fwd, normal, tangent);
	VectorNormalize(tangent);

	numtris = Q1BSP_ClipDecal(center, normal, tangent, 128, &verts);
	qglDisable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
	qglDisable(GL_DEPTH_TEST);

	qglColor3f(1, 0, 0);
	qglShadeModel(GL_SMOOTH);
	qglBegin(GL_TRIANGLES);
	for (i = 0; i < numtris; i++)
	{
		qglVertex3fv(verts+i*9+0);
		qglVertex3fv(verts+i*9+3);
		qglVertex3fv(verts+i*9+6);
	}
	qglEnd();

	qglColor3f(1, 1, 1);
	qglBegin(GL_LINES);
	for (i = 0; i < numtris; i++)
	{
		qglVertex3fv(verts+i*9+0);
		qglVertex3fv(verts+i*9+3);
		qglVertex3fv(verts+i*9+3);
		qglVertex3fv(verts+i*9+6);
		qglVertex3fv(verts+i*9+6);
		qglVertex3fv(verts+i*9+0);
	}

	qglVertex3fv(center);
	VectorMA(center, 10, normal, fwd);
	qglVertex3fv(fwd);

	qglColor3f(0, 1, 0);
	qglVertex3fv(center);
	VectorMA(center, 10, tangent, fwd);
	qglVertex3fv(fwd);

	qglColor3f(0, 0, 1);
	qglVertex3fv(center);
	CrossProduct(tangent, normal, fwd);
	VectorMA(center, 10, fwd, fwd);
	qglVertex3fv(fwd);

	qglColor3f(1, 1, 1);

	qglEnd();
	qglEnable(GL_TEXTURE_2D);
	qglEnable(GL_DEPTH_TEST);
	*/
}

#endif
/*
Rendering functions (Client only)

==============================================================================

Server only functions
*/
#ifndef CLIENTONLY

extern int		fatbytes;
extern qbyte	fatpvs[(MAX_MAP_LEAFS+1)/4];

//does the recursive work of Q1BSP_FatPVS
void SV_Q1BSP_AddToFatPVS (vec3_t org, mnode_t *node)
{
	int		i;
	qbyte	*pvs;
	mplane_t	*plane;
	float	d;

	while (1)
	{
	// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0)
		{
			if (node->contents != Q1CONTENTS_SOLID)
			{
				pvs = Mod_Q1LeafPVS ( (mleaf_t *)node, sv.worldmodel, NULL);
				for (i=0 ; i<fatbytes ; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}
	
		plane = node->plane;
		d = DotProduct (org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else
		{	// go down both
			SV_Q1BSP_AddToFatPVS (org, node->children[0]);
			node = node->children[1];
		}
	}
}

/*
=============
Q1BSP_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
void Q1BSP_FatPVS (vec3_t org, qboolean add)
{
	fatbytes = (sv.worldmodel->numleafs+31)>>3;
	if (!add)
		Q_memset (fatpvs, 0, fatbytes);
	SV_Q1BSP_AddToFatPVS (org, sv.worldmodel->nodes);
}

qboolean Q1BSP_EdictInFatPVS(edict_t *ent)
{
	int i;

	if (ent->num_leafs == MAX_ENT_LEAFS+1)
		return true;	//it's in too many leafs for us to cope with. Just trivially accept it.

	for (i=0 ; i < ent->num_leafs ; i++)
		if (fatpvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
			return true;	//we might be able to see this one.
		
	return false;	//none of this ents leafs were visible, so neither is the ent.
}

/*
===============
SV_FindTouchedLeafs

Links the edict to the right leafs so we can get it's potential visability.
===============
*/
void Q1BSP_RFindTouchedLeafs (edict_t *ent, mnode_t *node)
{
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	int			leafnum;

	if (node->contents == Q1CONTENTS_SOLID)
		return;
	
// add an efrag if the node is a leaf

	if ( node->contents < 0)
	{
		if (ent->num_leafs >= MAX_ENT_LEAFS)
		{
			ent->num_leafs = MAX_ENT_LEAFS+1;	//too many. mark it as such so we can trivially accept huge mega-big brush models.
			return;
		}

		leaf = (mleaf_t *)node;
		leafnum = leaf - sv.worldmodel->leafs - 1;

		ent->leafnums[ent->num_leafs] = leafnum;
		ent->num_leafs++;			
		return;
	}
	
// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(ent->v->absmin, ent->v->absmax, splitplane);
	
// recurse down the contacted sides
	if (sides & 1)
		Q1BSP_RFindTouchedLeafs (ent, node->children[0]);
		
	if (sides & 2)
		Q1BSP_RFindTouchedLeafs (ent, node->children[1]);
}
void Q1BSP_FindTouchedLeafs(edict_t *ent)
{
	ent->num_leafs = 0;
	if (ent->v->modelindex)
		Q1BSP_RFindTouchedLeafs (ent, sv.worldmodel->nodes);
}

#endif
/*
Server only functions

==============================================================================

*/

