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
// world.c -- world query functions

#include "quakedef.h"
#include "pr_common.h"

#if defined(CSQC_DAT) || !defined(CLIENTONLY)
/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/

extern cvar_t sv_compatiblehulls;

typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	int			type;
	int			hitcontentsmask;
	wedict_t		*passedict;
#ifdef Q2SERVER
	q2edict_t	*q2passedict;
#endif
	int			hullnum;
	qboolean	capsule;
} moveclip_t;

/*
===============================================================================

HULL BOXES

===============================================================================
*/


static	hull_t		box_hull;
static	mclipnode_t	box_clipnodes[6];
static	mplane_t	box_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
static void World_InitBoxHull (void)
{
	int		i;
	int		side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i=0 ; i<6 ; i++)
	{
		box_clipnodes[i].planenum = i;
		
		side = i&1;
		
		box_clipnodes[i].children[side] = Q1CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side^1] = i + 1;
		else
			box_clipnodes[i].children[side^1] = Q1CONTENTS_SOLID;
		
		box_planes[i].type = i>>1;
		box_planes[i].normal[i>>1] = 1;
	}
	
}


/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t	*World_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

model_t mod_capsule;
qboolean World_BoxTrace(struct model_s *model, int hulloverride, int frame, vec3_t axis[3], vec3_t p1, vec3_t p2, vec3_t mins, vec3_t maxs, unsigned int against, struct trace_s *trace)
{
	hull_t *hull = &box_hull;

	//bbox vs bbox
	//capsule vs bbox (NYI)

	memset (trace, 0, sizeof(trace_t));
	trace->fraction = 1;
	trace->allsolid = true;

	VectorCopy (p2, trace->endpos);
	return Q1BSP_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, p1, p2, trace);
}
qboolean World_CapsuleTrace(struct model_s *model, int hulloverride, int frame, vec3_t axis[3], vec3_t p1, vec3_t p2, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int against, struct trace_s *trace)
{
	//bbox vs capsule (NYI)
	//capsule vs capsule (NYI)

	memset (trace, 0, sizeof(trace_t));
	trace->fraction = 1;
	trace->allsolid = false;
	VectorCopy(p2, trace->endpos);

	if (capsule)
	{	//capsule vs capsule.
		//no orientation support on either (ignore axis)
		float sr = ((model->maxs[0]-model->mins[0]) + (model->maxs[1]-model->mins[1]))/4.0;
		float sh = (model->maxs[2]-model->mins[2]) - sr*2;
		float mr = ((maxs[0]-mins[0]) + (maxs[1]-mins[1]))/4.0;
		float mh = (maxs[2]-mins[2]) - mr*2;
		vec3_t sup = {0, 0, 1};
		vec3_t dir, sright;
		vec4_t nearestplane;
		float d1, d2;
		vec3_t nearestpoint;
		float neardist;

		//expand the static capsule's height+radius by the mover's height+radius, so that its point+capsule instead
		sr += mr;
		sh += mh;

		VectorSubtract(p1, p2, dir);
		d2=VectorNormalize(dir);
		CrossProduct(sup, dir, sright);
		VectorNormalize(sright);
		CrossProduct(sup, sright, nearestplane);
		VectorNormalize(nearestplane);
		nearestplane[3] = DotProduct(vec3_origin, nearestplane);	//capsule is at 0 0 0
		d1 = DotProduct(nearestplane, p1) - nearestplane[3];
		d2 = DotProduct(nearestplane, p2) - nearestplane[3];
		d2 = -d1 /(d1+d2);
		VectorInterpolate(p1, d2, p2, nearestpoint);

		neardist = VectorLength(nearestpoint);
		if (neardist < sr)
		{
			float x, y, oz, nz;
			//sqrt(h*h-(x*x+y*y))=z
			//change the hypotenuse from the messed up value to the actual radius
			//and update z to match the changed h
			x = DotProduct(sup, nearestpoint) - 0;
			y = DotProduct(sright, nearestpoint) - 0;
			oz = DotProduct(nearestplane, nearestpoint) - nearestplane[3];
			nz = sqrt(sr*sr - (x*x+y*y));

			VectorMA(nearestpoint, nz-oz, dir, trace->endpos);
			trace->fraction = 0;
		}
	}
	return false;
}
model_t *World_CapsuleForBox(vec3_t mins, vec3_t maxs)
{
	VectorCopy(mins, mod_capsule.mins);
	VectorCopy(maxs, mod_capsule.maxs);
	mod_capsule.funcs.NativeTrace = World_CapsuleTrace;
	return &mod_capsule;
}
/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

/*
===============
SV_CreateAreaNode

===============
*/
static areanode_t *World_CreateAreaNode (world_t *w, int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &w->areanodes[w->numareanodes];
	w->numareanodes++;

	ClearLink (&anode->edicts);
	
	if (depth == w->areanodedepth)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;
	
	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);	
	VectorCopy (mins, mins2);	
	VectorCopy (maxs, maxs1);	
	VectorCopy (maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	
	anode->children[0] = World_CreateAreaNode (w, depth+1, mins2, maxs2);
	anode->children[1] = World_CreateAreaNode (w, depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void World_ClearWorld (world_t *w)
{
	int maxdepth;
	vec3_t mins, maxs;
	if (w->worldmodel)
	{
		VectorCopy(w->worldmodel->mins, mins);
		VectorCopy(w->worldmodel->maxs, maxs);
	}
	else
	{
		VectorSet(mins, -4096, -4096, -4096);
		VectorSet(maxs, 4096, 4096, 4096);
	}

	World_InitBoxHull ();
	
	memset (&w->portallist, 0, sizeof(w->portallist));
	ClearLink (&w->portallist.edicts);
	w->portallist.axis = -1;

	maxdepth = 4;

	if (!w->areanodes || w->areanodedepth != maxdepth)
	{
		Z_Free(w->areanodes);
		w->areanodedepth = maxdepth;
		w->areanodes = Z_Malloc(sizeof(*w->areanodes) * pow(2, w->areanodedepth+1));
	}
	else
		memset (w->areanodes, 0, sizeof(*w->areanodes)*w->numareanodes);
	w->numareanodes = 0;
	World_CreateAreaNode (w, 0, mins, maxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void World_UnlinkEdict (wedict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}


/*
====================
SV_TouchLinks
====================
*/
#define MAX_NODELINKS	256	//all this means is that any more than this will not touch.
static wedict_t *nodelinks[MAX_NODELINKS];
void World_TouchLinks (world_t *w, wedict_t *ent, areanode_t *node)
{
	link_t		*l, *next;
	wedict_t		*touch;

	int linkcount = 0, ln;

	//work out who they are first.
	for (l = node->edicts.next ; l != &node->edicts ; l = next)
	{
		if (linkcount == MAX_NODELINKS)
			break;
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch == ent)
			continue;

		if (!touch->v->touch || touch->v->solid != SOLID_TRIGGER)
			continue;

		if (ent->v->absmin[0] > touch->v->absmax[0]
		|| ent->v->absmin[1] > touch->v->absmax[1]
		|| ent->v->absmin[2] > touch->v->absmax[2]
		|| ent->v->absmax[0] < touch->v->absmin[0]
		|| ent->v->absmax[1] < touch->v->absmin[1]
		|| ent->v->absmax[2] < touch->v->absmin[2] )
			continue;

		if (!((int)ent->xv->dimension_solid & (int)touch->xv->dimension_hit))
			continue;

		nodelinks[linkcount++] = touch;
	}

	for (ln = 0; ln < linkcount; ln++)
	{
		touch = nodelinks[ln];

		//make sure nothing moved it away
		if (ED_ISFREE(touch))
			continue;
		if (!touch->v->touch || touch->v->solid != SOLID_TRIGGER)
			continue;

		if (ent->v->absmin[0] > touch->v->absmax[0]
		|| ent->v->absmin[1] > touch->v->absmax[1]
		|| ent->v->absmin[2] > touch->v->absmax[2]
		|| ent->v->absmax[0] < touch->v->absmin[0]
		|| ent->v->absmax[1] < touch->v->absmin[1]
		|| ent->v->absmax[2] < touch->v->absmin[2] )
			continue;

		if (!((int)ent->xv->dimension_solid & (int)touch->xv->dimension_hit))	//didn't change did it?...
			continue;

		w->Event_Touch(w, touch, ent);

		if (ED_ISFREE(ent))
			break;
	}


// recurse down both sides
	if (node->axis == -1 || ED_ISFREE(ent))
		return;
	
	if (ent->v->absmax[node->axis] > node->dist)
		World_TouchLinks (w, ent, node->children[0]);
	if (ent->v->absmin[node->axis] < node->dist)
		World_TouchLinks (w, ent, node->children[1]);
}

#ifdef Q2BSPS
void Q2BSP_FindTouchedLeafs(model_t *model, struct pvscache_s *ent, float *mins, float *maxs)
{
#define MAX_TOTAL_ENT_LEAFS		128
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int num_leafs;
	int			topnode;
	int i, j;
	int			area;
	int nullarea = (model->fromgame == fg_quake2)?0:-1;

	//ent->num_leafs == q2's ent->num_clusters
	ent->num_leafs = 0;
	ent->areanum = nullarea;
	ent->areanum2 = nullarea;

	if (!mins || !maxs)
		return;

	//get all leafs, including solids
	num_leafs = CM_BoxLeafnums (model, mins, maxs,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// set areas
	for (i=0 ; i<num_leafs ; i++)
	{
		clusters[i] = CM_LeafCluster (model, leafs[i]);
		area = CM_LeafArea (model, leafs[i]);
		if (area != nullarea)
		{	// doors may legally straggle two areas,
			// but nothing should ever need more than that
			if (ent->areanum != nullarea && ent->areanum != area)
				ent->areanum2 = area;
			else
				ent->areanum = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS)
	{	// assume we missed some leafs, and mark by headnode
		ent->num_leafs = -1;
		ent->headnode = topnode;
	}
	else
	{
		ent->num_leafs = 0;
		for (i=0 ; i<num_leafs ; i++)
		{
			if (clusters[i] == -1)
				continue;		// not a visible leaf
			for (j=0 ; j<i ; j++)
				if (clusters[j] == clusters[i])
					break;
			if (j == i)
			{
				if (ent->num_leafs == MAX_ENT_LEAFS)
				{	// assume we missed some leafs, and mark by headnode
					ent->num_leafs = -1;
					ent->headnode = topnode;
					break;
				}

				ent->leafnums[ent->num_leafs++] = clusters[i];
			}
		}
	}
}
#endif

/*
===============
SV_LinkEdict

===============
*/
void QDECL World_LinkEdict (world_t *w, wedict_t *ent, qboolean touch_triggers)
{
	areanode_t	*node;
	
	if (ent->area.prev)
		World_UnlinkEdict (ent);	// unlink from old position
	
	if (ent == w->edicts)
		return;		// don't add the world

	if (ED_ISFREE(ent))
		return;

// set the abs box
	if (ent->v->solid == SOLID_BSP && 
	(ent->v->angles[0] || ent->v->angles[1] || ent->v->angles[2]) )
	{	// expand for rotation

#if 1
		int i;
		float v;
		float max;
		//q2 method
		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v =fabs( ent->v->mins[i]);
			if (v > max)
				max = v;
			v =fabs( ent->v->maxs[i]);
			if (v > max)
				max = v;
		}
		for (i=0 ; i<3 ; i++)
		{
			ent->v->absmin[i] = ent->v->origin[i] - max;
			ent->v->absmax[i] = ent->v->origin[i] + max;
		}
#else

		int			i;

		vec3_t f, r, u;
		vec3_t mn, mx;

		//we need to link to the correct leaves

		AngleVectors(ent->v->angles, f,r,u);

		mn[0] = DotProduct(ent->v->mins, f);
		mn[1] = -DotProduct(ent->v->mins, r);
		mn[2] = DotProduct(ent->v->mins, u);

		mx[0] = DotProduct(ent->v->maxs, f);
		mx[1] = -DotProduct(ent->v->maxs, r);
		mx[2] = DotProduct(ent->v->maxs, u);
		for (i = 0; i < 3; i++)
		{
			if (mn[i] < mx[i])
			{
				ent->v->absmin[i] = ent->v->origin[i]+mn[i]-0.1;
				ent->v->absmax[i] = ent->v->origin[i]+mx[i]+0.1;
			}
			else
			{	//box went inside out
				ent->v->absmin[i] = ent->v->origin[i]+mx[i]-0.1;
				ent->v->absmax[i] = ent->v->origin[i]+mn[i]+0.1;
			}
		}
#endif
	}
	else
	{
		VectorAdd (ent->v->origin, ent->v->mins, ent->v->absmin);	
		VectorAdd (ent->v->origin, ent->v->maxs, ent->v->absmax);
	}

	{
		unsigned int mdl = ent->v->modelindex;
		if (mdl < MAX_PRECACHE_MODELS && sv.models[mdl] && sv.models[mdl]->type == mod_brush)
			ent->solidsize = ES_SOLID_BSP;
		else
			ent->solidsize = COM_EncodeSize(ent->v->mins, ent->v->maxs);
	}

//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int)ent->v->flags & FL_ITEM)
	{
		ent->v->absmin[0] -= 15;
		ent->v->absmin[1] -= 15;
		ent->v->absmin[2] -= 1;
		ent->v->absmax[0] += 15;
		ent->v->absmax[1] += 15;
		ent->v->absmax[2] += 1;
	}
	else
	{	// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v->absmin[0] -= 1;
		ent->v->absmin[1] -= 1;
		ent->v->absmin[2] -= 1;
		ent->v->absmax[0] += 1;
		ent->v->absmax[1] += 1;
		ent->v->absmax[2] += 1;
	}
	
// link to PVS leafs
	if (w->worldmodel && w->worldmodel->loadstate == MLS_LOADED && w->worldmodel->funcs.FindTouchedLeafs)
	{
		w->worldmodel->funcs.FindTouchedLeafs(w->worldmodel, &ent->pvsinfo, ent->v->absmin, ent->v->absmax);
	}

//	if (ent->v->solid == SOLID_NOT && !sv_gameplayfix_nolinknonsolid.ival)
//		return;

// find the first node that the ent's box crosses
	if (ent->v->solid == SOLID_PORTAL)
		node = &w->portallist;
	else
	{
		node = w->areanodes;
		while (1)
		{
			if (node->axis == -1)
				break;
			if (ent->v->absmin[node->axis] > node->dist)
				node = node->children[0];
			else if (ent->v->absmax[node->axis] < node->dist)
				node = node->children[1];
			else
				break;		// crosses the node
		}
	}
	
// link it in	

	InsertLinkBefore (&ent->area, &node->edicts);
	
// if touch_triggers, touch all entities at this node and decend for more
	if (touch_triggers && ent->v->solid != SOLID_NOT)
		World_TouchLinks (w, ent, w->areanodes);
}


#ifdef Q2SERVER
void VARGS WorldQ2_UnlinkEdict(world_t *w, q2edict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}

void VARGS WorldQ2_LinkEdict(world_t *w, q2edict_t *ent)
{
	areanode_t	*node;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int			num_leafs;
	int			i, j;
	int			area;
	int			topnode;

	if (ent->area.prev)
		WorldQ2_UnlinkEdict (w, ent);	// unlink from old position
		
	if (ent == ge->edicts)
		return;		// don't add the world

	if (!ent->inuse)
		return;

	// set the size
	VectorSubtract (ent->maxs, ent->mins, ent->size);
	
	// encode the size into the entity_state for client prediction
	if (ent->solid == Q2SOLID_BBOX && !(ent->svflags & SVF_DEADMONSTER))
	{	// assume that x/y are equal and symetric
		ent->s.solid = COM_EncodeSize(ent->mins, ent->maxs);
		/*
		i = ent->maxs[0]/8;
		if (i<1)
			i = 1;
		if (i>31)
			i = 31;

		// z is not symetric
		j = (-ent->mins[2])/8;
		if (j<1)
			j = 1;
		if (j>31)
			j = 31;

		// and z maxs can be negative...
		k = (ent->maxs[2]+32)/8;
		if (k<1)
			k = 1;
		if (k>63)
			k = 63;

		//fixme: 32bit?
		ent->s.solid = (k<<10) | (j<<5) | i;*/
	}
	else if (ent->solid == Q2SOLID_BSP)
	{
		ent->s.solid = ES_SOLID_BSP;		// a solid_bbox will never create this value
	}
	else
		ent->s.solid = 0;

	// set the abs box
	if (ent->solid == Q2SOLID_BSP && 
	(ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2]) )
	{	// expand for rotation
		float		max, v;
		int			i;

		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v =fabs( ent->mins[i]);
			if (v > max)
				max = v;
			v =fabs( ent->maxs[i]);
			if (v > max)
				max = v;
		}
		for (i=0 ; i<3 ; i++)
		{
			ent->absmin[i] = ent->s.origin[i] - max;
			ent->absmax[i] = ent->s.origin[i] + max;
		}
	}
	else
	{	// normal
		VectorAdd (ent->s.origin, ent->mins, ent->absmin);	
		VectorAdd (ent->s.origin, ent->maxs, ent->absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->absmin[0] -= 1;
	ent->absmin[1] -= 1;
	ent->absmin[2] -= 1;
	ent->absmax[0] += 1;
	ent->absmax[1] += 1;
	ent->absmax[2] += 1;

// link to PVS leafs
	ent->num_clusters = 0;
	ent->areanum = 0;
	ent->areanum2 = 0;

	//get all leafs, including solids
	num_leafs = CM_BoxLeafnums (w->worldmodel, ent->absmin, ent->absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// set areas
	for (i=0 ; i<num_leafs ; i++)
	{
		clusters[i] = CM_LeafCluster (w->worldmodel, leafs[i]);
		area = CM_LeafArea (w->worldmodel, leafs[i]);
		if (area)
		{	// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if (ent->areanum && ent->areanum != area)
				ent->areanum2 = area;
			else
				ent->areanum = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS)
	{	// assume we missed some leafs, and mark by headnode
		ent->num_clusters = -1;
		ent->headnode = topnode;
	}
	else
	{
		ent->num_clusters = 0;
		for (i=0 ; i<num_leafs ; i++)
		{
			if (clusters[i] == -1)
				continue;		// not a visible leaf
			for (j=0 ; j<i ; j++)
				if (clusters[j] == clusters[i])
					break;
			if (j == i)
			{
				if (ent->num_clusters == MAX_ENT_CLUSTERS)
				{	// assume we missed some leafs, and mark by headnode
					ent->num_clusters = -1;
					ent->headnode = topnode;
					break;
				}

				ent->clusternums[ent->num_clusters++] = clusters[i];
			}
		}
	}

	// if first time, make sure old_origin is valid
	if (!ent->linkcount)
	{
		VectorCopy (ent->s.origin, ent->s.old_origin);
	}
	ent->linkcount++;

	if (ent->solid == Q2SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = w->areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}

	// link it in	
	InsertLinkBefore (&ent->area, &node->edicts);
}

void WorldQ2_Q1BSP_LinkEdict(world_t *w, q2edict_t *ent)
{
	areanode_t	*node;
	int			i, j, k;

	if (ent->area.prev)
		WorldQ2_UnlinkEdict (w, ent);	// unlink from old position
		
	if (ent == ge->edicts)
		return;		// don't add the world

	if (!ent->inuse)
		return;

	// set the size
	VectorSubtract (ent->maxs, ent->mins, ent->size);
	
	// encode the size into the entity_state for client prediction
	if (ent->solid == Q2SOLID_BBOX && !(ent->svflags & SVF_DEADMONSTER))
	{	// assume that x/y are equal and symetric
		i = ent->maxs[0]/8;
		if (i<1)
			i = 1;
		if (i>31)
			i = 31;

		// z is not symetric
		j = (-ent->mins[2])/8;
		if (j<1)
			j = 1;
		if (j>31)
			j = 31;

		// and z maxs can be negative...
		k = (ent->maxs[2]+32)/8;
		if (k<1)
			k = 1;
		if (k>63)
			k = 63;

		ent->s.solid = (k<<10) | (j<<5) | i;
	}
	else if (ent->solid == Q2SOLID_BSP)
	{
		ent->s.solid = 31;		// a solid_bbox will never create this value
	}
	else
		ent->s.solid = 0;

	// set the abs box
	if (ent->solid == Q2SOLID_BSP && 
	(ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2]) )
	{	// expand for rotation
		float		max, v;
		int			i;

		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v =fabs( ent->mins[i]);
			if (v > max)
				max = v;
			v =fabs( ent->maxs[i]);
			if (v > max)
				max = v;
		}
		for (i=0 ; i<3 ; i++)
		{
			ent->absmin[i] = ent->s.origin[i] - max;
			ent->absmax[i] = ent->s.origin[i] + max;
		}
	}
	else
	{	// normal
		VectorAdd (ent->s.origin, ent->mins, ent->absmin);	
		VectorAdd (ent->s.origin, ent->maxs, ent->absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->absmin[0] -= 1;
	ent->absmin[1] -= 1;
	ent->absmin[2] -= 1;
	ent->absmax[0] += 1;
	ent->absmax[1] += 1;
	ent->absmax[2] += 1;

// link to PVS leafs
	ent->num_clusters = 0;
	ent->areanum = 0;
	ent->areanum2 = 0;


	ent->areanum = 1;
/*
	//get all leafs, including solids
	num_leafs = CM_BoxLeafnums (ent->absmin, ent->absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// set areas
	for (i=0 ; i<num_leafs ; i++)
	{
		clusters[i] = CM_LeafCluster (leafs[i]);
		area = CM_LeafArea (leafs[i]);
		if (area)
		{	// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if (ent->areanum && ent->areanum != area)
			{
				ent->areanum2 = area;
			}
			else
				ent->areanum = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS)
	{	// assume we missed some leafs, and mark by headnode
		ent->num_clusters = -1;
		ent->headnode = topnode;
	}
	else
	{
		ent->num_clusters = 0;
		for (i=0 ; i<num_leafs ; i++)
		{
			if (clusters[i] == -1)
				continue;		// not a visible leaf
			for (j=0 ; j<i ; j++)
				if (clusters[j] == clusters[i])
					break;
			if (j == i)
			{
				if (ent->num_clusters == MAX_ENT_CLUSTERS)
				{	// assume we missed some leafs, and mark by headnode
					ent->num_clusters = -1;
					ent->headnode = topnode;
					break;
				}

				ent->clusternums[ent->num_clusters++] = clusters[i];
			}
		}
	}
	*/

	// if first time, make sure old_origin is valid
	if (!ent->linkcount)
	{
		VectorCopy (ent->s.origin, ent->s.old_origin);
	}
	ent->linkcount++;

	if (ent->solid == Q2SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = w->areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}

	// link it in
	InsertLinkBefore (&ent->area, &node->edicts);
}
#endif

/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_PointContents

==================
*/
int World_PointContents (world_t *w, vec3_t p)
{
	return w->worldmodel->funcs.PointContents(w->worldmodel, NULL, p);
}

//===========================================================================

/*
============
SV_TestEntityPosition

A small wrapper around SV_BoxInSolidEntity that never clips against the
supplied entity.
============
*/
wedict_t	*World_TestEntityPosition (world_t *w, wedict_t *ent)
{
	trace_t	trace;

	trace = World_Move (w, ent->v->origin, ent->v->mins, ent->v->maxs, ent->v->origin, ((ent->v->solid == SOLID_NOT || ent->v->solid == SOLID_TRIGGER)?MOVE_NOMONSTERS:0), ent);
	
	if (trace.startsolid || trace.allsolid)
		return trace.ent?trace.ent:w->edicts;
		
	return NULL;
}

qboolean Q1BSP_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace);

//wrapper function. Rotates the start and end positions around the angles if needed.
//qboolean TransformedHullCheck (hull_t *hull, vec3_t start, vec3_t end, trace_t *trace, vec3_t angles)
qboolean World_TransformedTrace (struct model_s *model, int hulloverride, int frame, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, qboolean capsule, struct trace_s *trace, vec3_t origin, vec3_t angles, unsigned int hitcontentsmask)
{
	vec3_t		start_l, end_l;
	vec3_t		axis[3];
	qboolean	result;

	memset (trace, 0, sizeof(trace_t));
	trace->fraction = 1;
	trace->allsolid = true;
	trace->startsolid = false;
	trace->inopen = true;	//probably wrong...
	VectorCopy (end, trace->endpos);

	if (IS_NAN(end[0]) || IS_NAN(end[1]) || IS_NAN(end[2]))
	{
		Con_DPrintf("Nan in traceline\n");
		return false;
	}

	// don't rotate non bsp ents. Too small to bother.
	if (model && model->loadstate == MLS_LOADED)
	{
		VectorSubtract (start, origin, start_l);
		VectorSubtract (end, origin, end_l);

		if (angles[0] || angles[1] || angles[2])
		{
			AngleVectors (angles, axis[0], axis[1], axis[2]);
			VectorNegate(axis[1], axis[1]);
			result = model->funcs.NativeTrace (model, hulloverride, frame, axis, start_l, end_l, mins, maxs, capsule, hitcontentsmask, trace);
		}
		else
		{
			result = model->funcs.NativeTrace (model, hulloverride, frame, NULL, start_l, end_l, mins, maxs, capsule, hitcontentsmask, trace);
		}

		VectorAdd (trace->endpos, origin, trace->endpos);
	}
	else if (hitcontentsmask & FTECONTENTS_BODY)
	{
		hull_t *hull = &box_hull;

		VectorSubtract (start, origin, start_l);
		VectorSubtract (end, origin, end_l);
		VectorCopy (end_l, trace->endpos);
		result = Q1BSP_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l, trace);
		VectorAdd (trace->endpos, origin, trace->endpos);
	}
	else
		result = false;

	return result;
}

/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
static trace_t World_ClipMoveToEntity (world_t *w, wedict_t *ent, vec3_t eorg, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int hullnum, qboolean hitmodel, qboolean capsule, unsigned int hitcontentsmask)	//hullnum overrides min/max for q1 style bsps
{
	trace_t		trace;
	model_t		*model;
	int mdlidx = ent->v->modelindex;

// get the clipping hull
	if ((ent->v->solid == SOLID_BSP || ent->v->solid == SOLID_PORTAL) && mdlidx)
	{
		model = w->Get_CModel(w, mdlidx);
		if (!model || (model->type != mod_brush && model->type != mod_heightmap))
		{
//			Host_Error("SOLID_BSP with non bsp model (classname: %s)", PR_GetString(w->progs, ent->v->classname));
			model = NULL;
		}
	}
	else
		model = NULL;

	if (!model || model->loadstate != MLS_LOADED)
	{
		vec3_t boxmins, boxmaxs;
		model = NULL;
		VectorSubtract (ent->v->mins, maxs, boxmins);
		VectorSubtract (ent->v->maxs, mins, boxmaxs);
//		if (ent->xv->geomtype == GEOMTYPE_CAPSULE && !hitmodel)
//			model = World_CapsuleForBox(boxmins, boxmaxs);
//		else
			World_HullForBox(boxmins, boxmaxs);
	}

// trace a line through the apropriate clipping hull
	if (ent->v->solid == SOLID_PORTAL)
	{
		//solid_portal cares only about origins and as such has no mins/max
		World_TransformedTrace(model, 0, ent->v->frame, start, end, vec3_origin, vec3_origin, capsule, &trace, eorg, ent->v->angles, hitcontentsmask);
		if (trace.startsolid)	//portals should not block traces. this prevents infinite looping
			trace.startsolid = false;
		hitmodel = false;
	}
	else if (ent->v->solid != SOLID_BSP)
	{
		ent->v->angles[0]*=-1;	//carmack made bsp models rotate wrongly.
		World_TransformedTrace(model, hullnum, ent->v->frame, start, end, mins, maxs, capsule, &trace, eorg, ent->v->angles, hitcontentsmask);
		ent->v->angles[0]*=-1;
	}
	else
	{
		World_TransformedTrace(model, hullnum, ent->v->frame, start, end, mins, maxs, capsule, &trace, eorg, ent->v->angles, hitcontentsmask);
	}

// if using hitmodel, we know it hit the bounding box, so try a proper trace now.
	if (hitmodel && (trace.fraction != 1 || trace.startsolid) && !model)
	{
		//okay, we hit the bbox
		model = w->Get_CModel(w, mdlidx);

		if (model && model->funcs.NativeTrace && model->loadstate == MLS_LOADED)
		{
			//do the second trace, using the actual mesh.
			World_TransformedTrace(model, hullnum, ent->v->frame, start, end, mins, maxs, capsule, &trace, eorg, ent->v->angles, hitcontentsmask);
		}
	}

// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid)
		trace.ent = ent;

	return trace;
}
#ifdef Q2SERVER
static trace_t WorldQ2_ClipMoveToEntity (world_t *w, q2edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, unsigned int hitcontentsmask)
{
	trace_t		trace;
	model_t		*model = NULL;

// get the clipping hull
	if (ent->s.solid == Q2SOLID_BSP)
		model = w->Get_CModel(w, ent->s.modelindex);

	if (!model || model->type != mod_brush || model->loadstate != MLS_LOADED)
	{
		vec3_t boxmins, boxmaxs;
		VectorSubtract (ent->mins, maxs, boxmins);
		VectorSubtract (ent->maxs, mins, boxmaxs);
		World_HullForBox(boxmins, boxmaxs);
		model = NULL;
	}

// trace a line through the apropriate clipping hull
	World_TransformedTrace(model, 0, 0, start, end, mins, maxs, false, &trace, ent->s.origin, ent->s.angles, hitcontentsmask);

// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid  )
		trace.ent = (edict_t *)ent;

	return trace;
}
#endif
#ifdef Q2BSPS
float	*area_mins, *area_maxs;
wedict_t	**area_list;
#ifdef Q2SERVER
q2edict_t	**area_q2list;
#endif
int		area_count, area_maxcount;
int		area_type;
#define AREA_SOLID 1
#define AREA_TRIGGER 2
static void World_AreaEdicts_r (areanode_t *node)
{
	link_t		*l, *next, *start;
	wedict_t		*check;

	// touch linked edicts
	start = &node->edicts;

	for (l=start->next  ; l != start ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->v->solid == SOLID_NOT)
			continue;		// deactivated

		/*q2 still has solid/trigger lists, emulate that here*/
		if ((check->v->solid == SOLID_TRIGGER) != (area_type == AREA_TRIGGER))
			continue;

		if (check->v->absmin[0] > area_maxs[0]
		|| check->v->absmin[1] > area_maxs[1]
		|| check->v->absmin[2] > area_maxs[2]
		|| check->v->absmax[0] < area_mins[0]
		|| check->v->absmax[1] < area_mins[1]
		|| check->v->absmax[2] < area_mins[2])
			continue;		// not touching

		if (area_count == area_maxcount)
		{
			Con_Printf ("SV_AreaEdicts: MAXCOUNT\n");
			return;
		}

		area_list[area_count] = check;
		area_count++;
	}
	
	if (node->axis == -1)
		return;		// terminal node

	// recurse down both sides
	if ( area_maxs[node->axis] > node->dist )
		World_AreaEdicts_r ( node->children[0] );
	if ( area_mins[node->axis] < node->dist )
		World_AreaEdicts_r ( node->children[1] );
}

/*
================
SV_AreaEdicts
================
*/
int World_AreaEdicts (world_t *w, vec3_t mins, vec3_t maxs, wedict_t **list,
	int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	World_AreaEdicts_r (w->areanodes);

	return area_count;
}

#ifdef Q2SERVER
static void WorldQ2_AreaEdicts_r (areanode_t *node)
{
	link_t		*l, *next, *start;
	q2edict_t		*check;

	// touch linked edicts
	start = &node->edicts;

	for (l=start->next  ; l != start ; l = next)
	{
		if (!l)
		{
			int i;
			World_ClearWorld(&sv.world);
			check = ge->edicts;
			for (i = 0; i < ge->num_edicts; i++, check = (q2edict_t	*)((char *)check + ge->edict_size))
				memset(&check->area, 0, sizeof(check->area));
			Con_Printf ("SV_AreaEdicts: Bad links\n");
			return;
		}
		next = l->next;
		check = Q2EDICT_FROM_AREA(l);

		if (check->solid == Q2SOLID_NOT)
			continue;		// deactivated

		/*q2 still has solid/trigger lists, emulate that here*/
		if ((check->solid == Q2SOLID_TRIGGER) != (area_type == AREA_TRIGGER))
			continue;

		if (check->absmin[0] > area_maxs[0]
		|| check->absmin[1] > area_maxs[1]
		|| check->absmin[2] > area_maxs[2]
		|| check->absmax[0] < area_mins[0]
		|| check->absmax[1] < area_mins[1]
		|| check->absmax[2] < area_mins[2])
			continue;		// not touching

		if (area_count == area_maxcount)
		{
			Con_Printf ("SV_AreaEdicts: MAXCOUNT\n");
			return;
		}

		area_q2list[area_count] = check;
		area_count++;
	}
	
	if (node->axis == -1)
		return;		// terminal node

	// recurse down both sides
	if ( area_maxs[node->axis] > node->dist )
		WorldQ2_AreaEdicts_r ( node->children[0] );
	if ( area_mins[node->axis] < node->dist )
		WorldQ2_AreaEdicts_r ( node->children[1] );
}

int VARGS WorldQ2_AreaEdicts (world_t *w, vec3_t mins, vec3_t maxs, q2edict_t **list,
	int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_q2list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	WorldQ2_AreaEdicts_r (w->areanodes);

	return area_count;
}
#endif

/*
================
SV_HeadnodeForEntity

Returns a headnode that can be used for testing or clipping an
object of mins/maxs size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/

#ifdef Q2SERVER
static model_t *WorldQ2_ModelForEntity (world_t *w, q2edict_t *ent)
{
	model_t	*model;

// decide which clipping hull to use, based on the size
	if (ent->solid == Q2SOLID_BSP)
	{	// explicit hulls in the BSP model
		model = w->Get_CModel(w, ent->s.modelindex);

		if (!model)
			SV_Error ("Q2SOLID_BSP with a non bsp model");

		if (model->loadstate == MLS_LOADED)
			return model;
	}

	// create a temp hull from bounding box sizes

	return CM_TempBoxModel (ent->mins, ent->maxs);
}
#endif

#ifdef Q2SERVER
void WorldQ2_ClipMoveToEntities (world_t *w, moveclip_t *clip )
{
	int			i, num;
	q2edict_t		*touchlist[MAX_Q2EDICTS], *touch;
	trace_t		trace;
	model_t		*model;
	float		*angles;

	num = WorldQ2_AreaEdicts (w, clip->boxmins, clip->boxmaxs, touchlist
		, MAX_Q2EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i=0 ; i<num ; i++)
	{
		touch = touchlist[i];
		if (touch->solid == Q2SOLID_NOT)
			continue;
		if (touch == clip->q2passedict)
			continue;
		if (clip->trace.allsolid)
			return;
		if (clip->q2passedict)
		{
		 	if (touch->owner == clip->q2passedict)
				continue;	// don't clip against own missiles
			if (clip->q2passedict->owner == touch)
				continue;	// don't clip against owner
		}

		if (touch->svflags & SVF_DEADMONSTER)
		if ( !(clip->hitcontentsmask & Q2CONTENTS_DEADMONSTER))
				continue;

		// might intersect, so do an exact clip
		model = WorldQ2_ModelForEntity (w, touch);
		angles = touch->s.angles;
		if (touch->solid != Q2SOLID_BSP)
			angles = vec3_origin;	// boxes don't rotate

		if (touch->svflags & SVF_MONSTER)
			World_TransformedTrace (model, 0, 0, clip->start, clip->end, clip->mins2, clip->maxs2, false, &trace, touch->s.origin, angles, clip->hitcontentsmask);
		else
			World_TransformedTrace (model, 0, 0, clip->start, clip->end, clip->mins, clip->maxs, false, &trace, touch->s.origin, angles, clip->hitcontentsmask);

		if (trace.allsolid || trace.startsolid ||
		trace.fraction < clip->trace.fraction)
		{
			trace.ent = (edict_t *)touch;
		 	if (clip->trace.startsolid)
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
				clip->trace = trace;
		}
		else if (trace.startsolid)
			clip->trace.startsolid = true;
	}
#undef ped
}
#endif
#endif
//===========================================================================

//a portal is flush with a world surface behind it.
//this causes problems. namely that we can't pass through the portal plane if the bsp behind it prevents out origin from getting through.
//so if the trace was clipped and ended infront of the portal, continue the trace to the edges of the portal cutout instead.
void World_PortalCSG(wedict_t *portal, float *trmin, float *trmax, vec3_t start, vec3_t end, trace_t *trace)
{
	vec4_t planes[6];	//far, near, right, left, up, down
	int plane;
	vec3_t worldpos;
	float bestfrac;
	int hitplane;
	float portalradius = portal->v->impulse;
	//only run this code if we impacted on the portal's parent.
	if (trace->fraction == 1 && !trace->startsolid)
		return;
	if (!portalradius)
		return;
	
	if (trace->startsolid)
		VectorCopy(start, worldpos);	//make sure we use a sane valid position.
	else
		VectorCopy(trace->endpos, worldpos);

	//determine the csg area. normals should be facing in
	AngleVectors(portal->v->angles, planes[1], planes[3], planes[5]);
	VectorNegate(planes[1], planes[0]);
	VectorNegate(planes[3], planes[2]);
	VectorNegate(planes[5], planes[4]);

	portalradius/=2;
	planes[0][3] = DotProduct(portal->v->origin, planes[0]) - (4.0/32);
	planes[1][3] = DotProduct(portal->v->origin, planes[1]) - (4.0/32);	//an epsilon beyond the portal
	planes[2][3] = DotProduct(portal->v->origin, planes[2]) - portalradius;
	planes[3][3] = DotProduct(portal->v->origin, planes[3]) - portalradius;
	planes[4][3] = DotProduct(portal->v->origin, planes[4]) - portalradius;
	planes[5][3] = DotProduct(portal->v->origin, planes[5]) - portalradius;

	//if we're actually inside the csg region
	for (plane = 0; plane < 6; plane++)
	{
		vec3_t nearest;
		float d = DotProduct(worldpos, planes[plane]);
		int k;
		for (k = 0; k < 3; k++)
			nearest[k] = (planes[plane][k]>=0)?trmax[k]:trmin[k];
		if (!plane)	//front plane gets further away with side
			planes[plane][3] -= DotProduct(nearest, planes[plane]);
		else if (plane>1)	//side planes get nearer with size
			planes[plane][3] += 24;//DotProduct(nearest, planes[plane]);
		if (d - planes[plane][3] >= 0)
			continue;	//endpos is inside
		else
			return;		//end is already outside
	}
	//yup, we're inside, the trace shouldn't end where it actually did
	bestfrac = 1;
	hitplane = -1;
	for (plane = 0; plane < 6; plane++)
	{
		float ds = DotProduct(start, planes[plane]) - planes[plane][3];
		float de = DotProduct(end, planes[plane]) - planes[plane][3];
		float frac;
		if (ds >= 0 && de < 0)
		{
			frac = (ds) / (ds - de);
			if (frac < bestfrac)
			{
				if (frac < 0)
					frac = 0;
				bestfrac = frac;
				hitplane = plane;
			}
		}
	}
	trace->startsolid = trace->allsolid = false;
	//if we cross the front of the portal, don't shorten the trace, that will artificially clip us
	if (hitplane == 0 && trace->fraction > bestfrac)
		return;
	//okay, elongate to clip to the portal hole properly.
	trace->fraction = bestfrac;
	VectorInterpolate(start, bestfrac, end, trace->endpos);

	if (hitplane >= 0)
	{
		VectorCopy(planes[hitplane], trace->plane.normal);
		trace->plane.dist = planes[hitplane][3];
		if (hitplane == 1)
			trace->ent = portal;
	}
}

/*
====================
SV_ClipToEverything

like SV_ClipToLinks, but doesn't use the links part. This can be used for checking triggers, solid entities, not-solid entities.
Sounds pointless, I know.
====================
*/
static void World_ClipToEverything (world_t *w, moveclip_t *clip)
{
	int e;
	trace_t		trace;
	wedict_t		*touch;
	for (e=1 ; e<w->num_edicts ; e++)
	{
		touch = (wedict_t*)EDICT_NUM(w->progs, e);

		if (ED_ISFREE(touch))
			continue;
		if (touch->v->solid == SOLID_NOT && !((int)touch->v->flags & FL_FINDABLE_NONSOLID))
			continue;
		if (touch->v->solid == SOLID_TRIGGER && !((int)touch->v->flags & FL_FINDABLE_NONSOLID))
			continue;

		if (touch == clip->passedict)
			continue;

		if (clip->type & MOVE_NOMONSTERS && touch->v->solid != SOLID_BSP)
			continue;

		if (clip->passedict)
		{
			if (w->usesolidcorpse)
			{
				// don't clip corpse against character
				if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
					continue;
				// don't clip character against corpse
				if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
					continue;
			}
			if (!((int)clip->passedict->xv->dimension_hit & (int)touch->xv->dimension_solid))
				continue;
		}

		if (clip->boxmins[0] > touch->v->absmax[0]
				|| clip->boxmins[1] > touch->v->absmax[1]
				|| clip->boxmins[2] > touch->v->absmax[2]
				|| clip->boxmaxs[0] < touch->v->absmin[0]
				|| clip->boxmaxs[1] < touch->v->absmin[1]
				|| clip->boxmaxs[2] < touch->v->absmin[2] )
			continue;

		if (clip->passedict && clip->passedict->v->size[0] && !touch->v->size[0])
			continue;	// points never interact

	// might intersect, so do an exact clip
//		if (clip->trace.allsolid)
//			return;
		if (clip->passedict)
		{
		 	if ((wedict_t*)PROG_TO_EDICT(w->progs, touch->v->owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if ((wedict_t*)PROG_TO_EDICT(w->progs, clip->passedict->v->owner) == touch)
				continue;	// don't clip against owner
		}

		if (touch->v->solid == SOLID_PORTAL)
		{
			//make sure we don't hit the world if we're inside the portal
			World_PortalCSG(touch, clip->mins, clip->maxs, clip->start, clip->end, &clip->trace);
		}

		if ((int)touch->v->flags & FL_MONSTER)
			trace = World_ClipMoveToEntity (w, touch, touch->v->origin, clip->start, clip->mins2, clip->maxs2, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL, clip->capsule, clip->hitcontentsmask);
		else
			trace = World_ClipMoveToEntity (w, touch, touch->v->origin, clip->start, clip->mins, clip->maxs, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL, clip->capsule, clip->hitcontentsmask);
		if (trace.allsolid || trace.startsolid ||
				trace.fraction < clip->trace.fraction)
		{
			if (clip->type & MOVE_ENTCHAIN)
			{
				touch->v->chain = EDICT_TO_PROG(w->progs, clip->trace.ent?clip->trace.ent:w->edicts);
				clip->trace.ent = touch;
			}
			else
			{
				trace.ent = touch;
				clip->trace = trace;
			}
		}
	}
}

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
static void World_ClipToLinks (world_t *w, areanode_t *node, moveclip_t *clip)
{
	link_t		*l, *next;
	wedict_t		*touch;
	trace_t		trace;

// touch linked edicts
	for (l = node->edicts.next ; l != &node->edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch->v->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;

		/*if its a trigger, we only clip against it if the flags are aligned*/
		if (touch->v->solid == SOLID_TRIGGER || touch->v->solid == SOLID_LADDER)
		{
			if (!(clip->type & MOVE_TRIGGERS))
				continue;
			if (!((int)touch->v->flags & FL_FINDABLE_NONSOLID))
				continue;
		}

		if (clip->type & MOVE_LAGGED)
		{
			//can't touch lagged ents - we do an explicit test for them later.
			if (touch->entnum-1 < w->maxlagents)
				if (w->lagents[touch->entnum-1].present)
					continue;
		}

		if ((clip->type & MOVE_NOMONSTERS) && (touch->v->solid != SOLID_BSP && touch->v->solid != SOLID_PORTAL))
			continue;

		if (clip->passedict)
		{
			if (w->usesolidcorpse)
			{
				// don't clip corpse against character
				if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
					continue;
				// don't clip character against corpse
				if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
					continue;
			}
			if (!((int)clip->passedict->xv->dimension_hit & (int)touch->xv->dimension_solid))
				continue;
		}

		if (clip->boxmins[0] > touch->v->absmax[0]
		|| clip->boxmins[1] > touch->v->absmax[1]
		|| clip->boxmins[2] > touch->v->absmax[2]
		|| clip->boxmaxs[0] < touch->v->absmin[0]
		|| clip->boxmaxs[1] < touch->v->absmin[1]
		|| clip->boxmaxs[2] < touch->v->absmin[2] )
			continue;

		if (clip->passedict && clip->passedict->v->size[0] && !touch->v->size[0])
			continue;	// points never interact

	// might intersect, so do an exact clip
//		if (clip->trace.allsolid)
//			return;
		if (clip->passedict)
		{
		 	if ((wedict_t*)PROG_TO_EDICT(w->progs, touch->v->owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if ((wedict_t*)PROG_TO_EDICT(w->progs, clip->passedict->v->owner) == touch)
				continue;	// don't clip against owner
		}

		if (touch->v->solid == SOLID_PORTAL)
		{
			//make sure we don't hit the world if we're inside the portal
			World_PortalCSG(touch, clip->mins, clip->maxs, clip->start, clip->end, &clip->trace);
		}

		if ((int)touch->v->flags & FL_MONSTER)
			trace = World_ClipMoveToEntity (w, touch, touch->v->origin, clip->start, clip->mins2, clip->maxs2, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL, clip->capsule, clip->hitcontentsmask);
		else
			trace = World_ClipMoveToEntity (w, touch, touch->v->origin, clip->start, clip->mins, clip->maxs, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL, clip->capsule, clip->hitcontentsmask);

		if (trace.fraction < clip->trace.fraction)
		{
			//trace traveled less, but don't forget if we started in a solid.
			trace.startsolid |= clip->trace.startsolid;
			trace.allsolid |= clip->trace.allsolid;

			if (clip->type & MOVE_ENTCHAIN)
			{
				touch->v->chain = EDICT_TO_PROG(w->progs, clip->trace.ent?clip->trace.ent:w->edicts);
				clip->trace.ent = touch;
			}
			else
			{
				if (clip->trace.startsolid && !trace.startsolid)
					trace.ent = clip->trace.ent;	//something else hit earlier, that one gets the trace entity, but not the fraction. yeah, combining traces like this was always going to be weird.
				else
					trace.ent = touch;
				clip->trace = trace;
			}
		}
		else if (trace.startsolid || trace.allsolid)
		{
			//even if the trace traveled less, we still care if it was in a solid.
			clip->trace.startsolid |= trace.startsolid;
			clip->trace.allsolid |= trace.allsolid;
			if (!clip->trace.ent)
				clip->trace.ent = touch;
		}
	}
	
// recurse down both sides
	if (node->axis == -1)
		return;

	if ( clip->boxmaxs[node->axis] > node->dist )
		World_ClipToLinks (w, node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		World_ClipToLinks (w, node->children[1], clip );
}
#ifdef Q2SERVER
static void WorldQ2_ClipToLinks (world_t *w, areanode_t *node, moveclip_t *clip)
{
	link_t		*l, *next;
	q2edict_t		*touch;
	trace_t		trace;

// touch linked edicts
	for (l = node->edicts.next ; l != &node->edicts ; l = next)
	{
		next = l->next;
		touch = Q2EDICT_FROM_AREA(l);
		if (touch->s.solid == Q2SOLID_NOT)
			continue;
		if (touch == clip->q2passedict)
			continue;
		if (touch->s.solid == Q2SOLID_TRIGGER)
			SV_Error ("Trigger in clipping list");

		if (clip->type & MOVE_NOMONSTERS && touch->s.solid != Q2SOLID_BSP)
			continue;

		if (clip->boxmins[0] > touch->absmax[0]
		|| clip->boxmins[1] > touch->absmax[1]
		|| clip->boxmins[2] > touch->absmax[2]
		|| clip->boxmaxs[0] < touch->absmin[0]
		|| clip->boxmaxs[1] < touch->absmin[1]
		|| clip->boxmaxs[2] < touch->absmin[2] )
			continue;

		if (clip->q2passedict && clip->q2passedict->size[0] && !touch->size[0])
			continue;	// points never interact

	// might intersect, so do an exact clip
		if (clip->trace.allsolid)
			return;
		if (clip->passedict)
		{
		 	if (touch->owner == clip->q2passedict)
				continue;	// don't clip against own missiles
			if (clip->q2passedict->owner == touch)
				continue;	// don't clip against owner
		}

		trace = WorldQ2_ClipMoveToEntity (w, touch, clip->start, clip->mins, clip->maxs, clip->end, clip->hitcontentsmask);

		if (trace.allsolid || trace.startsolid ||
		trace.fraction < clip->trace.fraction)
		{
			trace.ent = (edict_t *)touch;
			clip->trace = trace;
		}
	}
	
// recurse down both sides
	if (node->axis == -1)
		return;

	if ( clip->boxmaxs[node->axis] > node->dist )
		World_ClipToLinks (w, node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		World_ClipToLinks (w, node->children[1], clip );
}
#endif
/*
==================
SV_MoveBounds
==================
*/
static void World_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
#if 0
// debug to test against everything
boxmins[0] = boxmins[1] = boxmins[2] = -9999;
boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
	int		i;
	
	for (i=0 ; i<3 ; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
#endif
}

/*
==================
SV_Move
==================
*/
trace_t World_Move (world_t *w, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, wedict_t *passedict)
{
	moveclip_t	clip;
	int			i;
	int hullnum;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	if (passedict && passedict->xv->hull && !(type & MOVE_IGNOREHULL))
		hullnum = passedict->xv->hull;
#ifdef CLIENTONLY
	else
		hullnum = 0;
#else
	else if (sv_compatiblehulls.value)
		hullnum = 0;
	else
	{
		int diff;
		int best;
		hullnum = 0;
		best = 8192;
		//x/y pos/neg are assumed to be the same magnitute.
		//z pos/height are assumed to be different from all the others.
		for (i = 0; i < MAX_MAP_HULLSM; i++)
		{
			if (!w->worldmodel->hulls[i].available)
				continue;
#define sq(x) ((x)*(x))
			diff = sq(w->worldmodel->hulls[i].clip_maxs[2] - maxs[2]) +
				sq(w->worldmodel->hulls[i].clip_mins[2] - mins[2]) +
				sq(w->worldmodel->hulls[i].clip_maxs[1] - maxs[1]) +
				sq(w->worldmodel->hulls[i].clip_mins[0] - mins[0]);
			if (diff < best)
			{
				best = diff;
				hullnum=i;
			}
		}
		hullnum++;
	}
#endif

	if (passedict->xv->hitcontentsmaski)
		clip.hitcontentsmask = passedict->xv->hitcontentsmaski;
/*#ifndef NOLEGACY
	else if (passedict->xv->hitcontentsmask)
		clip.hitcontentsmask = passedict->xv->hitcontentsmask;
#endif*/
	else if (type & MOVE_NOMONSTERS)
		clip.hitcontentsmask = MASK_WORLDSOLID; /*solid only to world*/
	else if (maxs[0] - mins[0] > 0)
		clip.hitcontentsmask = MASK_BOXSOLID;	/*impacts playerclip*/
	else
		clip.hitcontentsmask = MASK_POINTSOLID;		/*ignores playerclip but hits everything else*/
	clip.capsule = (passedict->xv->geomtype == GEOMTYPE_CAPSULE);

	if (type & MOVE_OTHERONLY)
	{
		wedict_t *other = WEDICT_NUM(w->progs, *w->g.other);
		return World_ClipMoveToEntity (w, other, other->v->origin, start, mins, maxs, end, hullnum, type & MOVE_HITMODEL, clip.capsule, clip.hitcontentsmask);
	}

// clip to world
	clip.trace = World_ClipMoveToEntity (w, w->edicts, w->edicts->v->origin, start, mins, maxs, end, hullnum, false, clip.capsule, clip.hitcontentsmask);

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = passedict;
	clip.hullnum = 0;//hullnum; //BUG: hexen2's SV_ClipMoveToEntity's move_ent argument is set inconsistantly. This has the effect that the SOLID_BSP's .hull field is used instead of the SOLID_BBOX entity. We can't fix this because hexen2 depends upon it - this is the 'tibet5' bug.
#ifdef Q2SERVER
	clip.q2passedict = NULL;
#endif

	if (type & MOVE_MISSILE)
	{
		for (i=0 ; i<3 ; i++)
		{
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	}
	else
	{
		VectorCopy (mins, clip.mins2);
		VectorCopy (maxs, clip.maxs2);
	}
	
// create the bounding box of the entire move
	World_MoveBounds (start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

// clip to entities
	if (clip.type & MOVE_EVERYTHING)
		World_ClipToEverything (w, &clip);
	else
	{
		if (clip.type & MOVE_LAGGED)
		{
			clip.type &= ~MOVE_LAGGED;
#ifndef CLIENTONLY
			if (w == &sv.world)
			{
				if (passedict->entnum && passedict->entnum <= sv.allocated_client_slots)
				{
					clip.type |= MOVE_LAGGED;
					w->lagents = svs.clients[passedict->entnum-1].laggedents;
					w->maxlagents = svs.clients[passedict->entnum-1].laggedents_count;
					w->lagentsfrac = svs.clients[passedict->entnum-1].laggedents_frac;
				}
				else if (passedict->v->owner)
				{
					if (passedict->v->owner && passedict->v->owner <= sv.allocated_client_slots)
					{
						clip.type |= MOVE_LAGGED;
						w->lagents = svs.clients[passedict->v->owner-1].laggedents;
						w->maxlagents = svs.clients[passedict->v->owner-1].laggedents_count;
						w->lagentsfrac = svs.clients[passedict->v->owner-1].laggedents_frac;
					}
				}
			}
#endif
		}
		if (clip.type & MOVE_LAGGED)
		{
			trace_t trace;
			wedict_t *touch;
			vec3_t lp;

			World_ClipToLinks (w, w->areanodes, &clip );

			for (i = 0; i < w->maxlagents; i++)
			{
				if (!w->lagents[i].present)
					continue;
				if (clip.trace.allsolid)
					break;

				touch = (wedict_t*)EDICT_NUM(w->progs, i+1);
				if (touch->v->solid == SOLID_NOT)
					continue;
				if (touch == clip.passedict)
					continue;
				if (touch->v->solid == SOLID_TRIGGER || touch->v->solid == SOLID_LADDER)
				{
					if (!(clip.type & MOVE_TRIGGERS))
						continue;
					if (!((int)touch->v->flags & FL_FINDABLE_NONSOLID))
						continue;
				}

				if (clip.type & MOVE_NOMONSTERS && touch->v->solid != SOLID_BSP)
					continue;

				if (clip.passedict)
				{
					if (w->usesolidcorpse)
					{
						// don't clip corpse against character
						if (clip.passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
							continue;
						// don't clip character against corpse
						if (clip.passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
							continue;
					}
					if (!((int)clip.passedict->xv->dimension_hit & (int)touch->xv->dimension_solid))
						continue;
				}

				VectorInterpolate(touch->v->origin, w->lagentsfrac, w->lagents[i].laggedpos, lp);

				if (clip.boxmins[0] > lp[0]+touch->v->maxs[0]
						|| clip.boxmins[1] > lp[1]+touch->v->maxs[1]
						|| clip.boxmins[2] > lp[2]+touch->v->maxs[2]
						|| clip.boxmaxs[0] < lp[0]+touch->v->mins[0]
						|| clip.boxmaxs[1] < lp[1]+touch->v->mins[1]
						|| clip.boxmaxs[2] < lp[2]+touch->v->mins[2] )
					continue;

				if (clip.passedict && clip.passedict->v->size[0] && !touch->v->size[0])
					continue;	// points never interact

				if (clip.passedict)
				{
	 				if ((wedict_t*)PROG_TO_EDICT(w->progs, touch->v->owner) == clip.passedict)
						continue;	// don't clip against own missiles
					if ((wedict_t*)PROG_TO_EDICT(w->progs, clip.passedict->v->owner) == touch)
						continue;	// don't clip against owner
				}

				trace = World_ClipMoveToEntity (w, touch, lp, clip.start, clip.mins, clip.maxs, clip.end, clip.hullnum, clip.type & MOVE_HITMODEL, clip.capsule, clip.hitcontentsmask);

				if (trace.allsolid || trace.startsolid || trace.fraction < clip.trace.fraction)
				{
					if (clip.type & MOVE_ENTCHAIN)
					{
						touch->v->chain = EDICT_TO_PROG(w->progs, clip.trace.ent?clip.trace.ent:w->edicts);
						clip.trace.ent = touch;
					}
					else
					{
						trace.ent = touch;
						clip.trace = trace;
					}
				}
			}
		}
		else
			World_ClipToLinks (w, w->areanodes, &clip );
		World_ClipToLinks(w, &w->portallist, &clip);
	}

//	if (clip.trace.startsolid)
//		clip.trace.fraction = 0;

	if (!clip.trace.ent)
		return clip.trace;

	return clip.trace;
}
#ifdef Q2SERVER
trace_t WorldQ2_Move (world_t *w, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int hitcontentsmask, q2edict_t *passedict)
{
	moveclip_t	clip;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

// clip to world
	w->worldmodel->funcs.NativeTrace(w->worldmodel, 0, 0, NULL, start, end, mins, maxs, false, hitcontentsmask, &clip.trace);
	clip.trace.ent = ge->edicts;

	if (clip.trace.fraction == 0)
		return clip.trace;

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = MOVE_NORMAL;
	clip.hitcontentsmask = hitcontentsmask;
	clip.passedict = NULL;
	clip.q2passedict = passedict;

	VectorCopy (mins, clip.mins2);
	VectorCopy (maxs, clip.maxs2);
	
// create the bounding box of the entire move
//FIXME: should we use clip.trace.endpos here?	
	World_MoveBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

// clip to entities
#ifdef Q2BSPS
	if (w->worldmodel->fromgame == fg_quake2 || w->worldmodel->fromgame == fg_quake3)
		WorldQ2_ClipMoveToEntities(w, &clip);
	else
#endif
		WorldQ2_ClipToLinks (w, w->areanodes, &clip );

	return clip.trace;
}
#endif

static void (QDECL *world_current_physics_engine)(world_t*world);
qboolean QDECL World_RegisterPhysicsEngine(const char *enginename, void(QDECL*startupfunc)(world_t*world))
{
	if (world_current_physics_engine)
		return false;	//no thanks, we already have one.
	world_current_physics_engine = startupfunc;
	return true;
}
void World_RBE_Shutdown(world_t *world)
{
#ifdef USERBE
	unsigned int u;
	wedict_t *ed;
	if (!world->rbe)
		return;

	if (world->progs)
	{
		for (u = 0; u < world->num_edicts; u++)
		{
			ed = WEDICT_NUM(world->progs, u);
			world->rbe->RemoveJointFromEntity(world, ed);
			world->rbe->RemoveFromEntity(world, ed);
		}
	}
	world->rbe->End(world);
#endif
}
void QDECL World_UnregisterPhysicsEngine(const char *enginename)
{
#ifdef RAGDOLL
	rag_uninstanciateall();
#endif

#if defined(CSQC_DAT) && !defined(SERVERONLY)
	{
		extern world_t csqc_world;
		World_RBE_Shutdown(&csqc_world);
	}
#endif
#if !defined(CLIENTONLY)
	World_RBE_Shutdown(&sv.world);
#endif

	world_current_physics_engine = NULL;
}
void World_RBE_Start(world_t *world)
{
	if (world_current_physics_engine)
	{
		if (world->worldmodel)
			world_current_physics_engine(world);
	}
}

void World_Destroy(world_t *world)
{
	World_RBE_Shutdown(world);

	Z_Free(world->areanodes);
	world->areanodes = NULL;
	world->areanodedepth = 0;

	memset(world, 0, sizeof(*world));
}

#ifdef USERBE
static qboolean GenerateCollisionMesh_BSP(world_t *world, model_t *mod, wedict_t *ed, vec3_t geomcenter)
{
	unsigned int sno;
	msurface_t *surf;
	mesh_t *mesh;
	unsigned int numverts;
	unsigned int numindexes,i;
	int *ptr_elements;
	float *ptr_verts;

	numverts = 0;
	numindexes = 0;
	for (sno = 0; sno < mod->nummodelsurfaces; sno++)
	{
		surf = &mod->surfaces[sno+mod->firstmodelsurface];
		if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
			continue;

		if (surf->mesh)
		{
			mesh = surf->mesh;
			numverts += mesh->numvertexes;
			numindexes += mesh->numindexes;
		}
		else
		{
			numverts += surf->numedges;
			numindexes += (surf->numedges-2) * 3;
		}
	}
	if (!numindexes)
	{
		Con_DPrintf("entity %i (classname %s) has no geometry\n", NUM_FOR_EDICT(world->progs, (edict_t*)ed), PR_GetString(world->progs, ed->v->classname));
		return false;
	}
	ptr_elements = (int*)BZ_Malloc(numindexes*sizeof(*ptr_elements));
	ptr_verts = (float*)BZ_Malloc(numverts*sizeof(vec3_t));

	numverts = 0;
	numindexes = 0;
	for (sno = 0; sno < mod->nummodelsurfaces; sno++)
	{
		surf = &mod->surfaces[sno+mod->firstmodelsurface];
		if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
			continue;

		if (surf->mesh)
		{
			mesh = surf->mesh;
			for (i = 0; i < mesh->numvertexes; i++)
				VectorSubtract(mesh->xyz_array[i], geomcenter, (ptr_verts + 3*(numverts+i)));
			for (i = 0; i < mesh->numindexes; i+=3)
			{
				//flip the triangles as we go
				ptr_elements[numindexes+i+0] = numverts+mesh->indexes[i+2];
				ptr_elements[numindexes+i+1] = numverts+mesh->indexes[i+1];
				ptr_elements[numindexes+i+2] = numverts+mesh->indexes[i+0];
			}
			numverts += mesh->numvertexes;
			numindexes += i;
		}
		else
		{
			float *vec;
			medge_t *edge;
			int lindex;
			for (i = 0; i < surf->numedges; i++)
			{
				lindex = mod->surfedges[surf->firstedge + i];

				if (lindex > 0)
				{
					edge = &mod->edges[lindex];
					vec = mod->vertexes[edge->v[0]].position;
				}
				else
				{
					edge = &mod->edges[-lindex];
					vec = mod->vertexes[edge->v[1]].position;
				}
			
				VectorSubtract(vec, geomcenter, (ptr_verts + 3*(numverts+i)));
			}
			for (i = 2; i < surf->numedges; i++)
			{
				//quake is backwards, not ode
				ptr_elements[numindexes++] = numverts+i;
				ptr_elements[numindexes++] = numverts+i-1;
				ptr_elements[numindexes++] = numverts;
			}
			numverts += surf->numedges;
		}
	}

	ed->ode.ode_element3i = ptr_elements;
	ed->ode.ode_vertex3f = ptr_verts;
	ed->ode.ode_numvertices = numverts;
	ed->ode.ode_numtriangles = numindexes/3;
	return true;
}

#include "com_mesh.h"
static qboolean GenerateCollisionMesh_Alias(world_t *world, model_t *mod, wedict_t *ed, vec3_t geomcenter)
{
	mesh_t mesh;
	unsigned int numverts;
	unsigned int numindexes,i;
	galiasinfo_t *inf;
	unsigned int surfnum = 0;
	entity_t re;
	int *ptr_elements;
	float *ptr_verts;

	numverts = 0;
	numindexes = 0;

	//fill in the parts of the entity_t that Alias_GAliasBuildMesh needs.
	world->Get_FrameState(world, ed, &re.framestate);
	re.fatness = ed->xv->fatness;
	re.model = mod;

	inf = (galiasinfo_t*)Mod_Extradata (mod);
	while(inf)
	{
		numverts += inf->numverts;
		numindexes += inf->numindexes;
		inf = inf->nextsurf;
	}

	if (!numindexes)
	{
		Con_DPrintf("entity %i (classname %s) has no geometry\n", NUM_FOR_EDICT(world->progs, (edict_t*)ed), PR_GetString(world->progs, ed->v->classname));
		return false;
	}
	ptr_elements = (int*)BZ_Malloc(numindexes*sizeof(*ptr_elements));
	ptr_verts = (float*)BZ_Malloc(numverts*sizeof(vec3_t));

	numverts = 0;
	numindexes = 0;

	inf = (galiasinfo_t*)Mod_Extradata (mod);
	while(inf)
	{
		Alias_GAliasBuildMesh(&mesh, NULL, inf, surfnum++, &re, false);
		for (i = 0; i < mesh.numvertexes; i++)
			VectorSubtract(mesh.xyz_array[i], geomcenter, (ptr_verts + 3*(numverts+i)));
		for (i = 0; i < mesh.numindexes; i+=3)
		{
			//flip the triangles as we go
			ptr_elements[numindexes+i+0] = numverts+mesh.indexes[i+2];
			ptr_elements[numindexes+i+1] = numverts+mesh.indexes[i+1];
			ptr_elements[numindexes+i+2] = numverts+mesh.indexes[i+0];
		}
		numverts += inf->numverts;
		numindexes += inf->numindexes;
		inf = inf->nextsurf;
	}

	Alias_FlushCache();	//it got built using an entity on the stack, make sure other stuff doesn't get hurt.

	ed->ode.ode_element3i = ptr_elements;
	ed->ode.ode_vertex3f = ptr_verts;
	ed->ode.ode_numvertices = numverts;
	ed->ode.ode_numtriangles = numindexes/3;
	return true;
}

//Bullet has a fit if we have any degenerate triangles, so make sure we can determine some surface normal
static void CollisionMesh_CleanupMesh(wedict_t *ed)
{
	float *v1, *v2, *v3;
	vec3_t d1, d2, cr;
	int in, out;
	for (in = 0, out = 0; in < ed->ode.ode_numtriangles*3; in+=3)
	{
		v1 = &ed->ode.ode_vertex3f[ed->ode.ode_element3i[in+0]*3];
		v2 = &ed->ode.ode_vertex3f[ed->ode.ode_element3i[in+1]*3];
		v3 = &ed->ode.ode_vertex3f[ed->ode.ode_element3i[in+2]*3];
		VectorSubtract(v3, v1, d1);
		VectorSubtract(v2, v1, d2);
		CrossProduct(d1, d2, cr);
		if (DotProduct(cr,cr) == 0)
			continue;
		ed->ode.ode_element3i[out+0] = ed->ode.ode_element3i[in+0];
		ed->ode.ode_element3i[out+1] = ed->ode.ode_element3i[in+1];
		ed->ode.ode_element3i[out+2] = ed->ode.ode_element3i[in+2];
		out+=3;
	}
	ed->ode.ode_numtriangles = out/3;
}

qboolean QDECL World_GenerateCollisionMesh(world_t *world, model_t *mod, wedict_t *ed, vec3_t geomcenter)
{
	qboolean result;
	switch(mod->type)
	{
	case mod_brush:
		result = GenerateCollisionMesh_BSP(world, mod, ed, geomcenter);
		break;
	case mod_alias:
		result = GenerateCollisionMesh_Alias(world, mod, ed, geomcenter);
		break;
	case mod_heightmap:
	case mod_halflife:
	case mod_sprite:
	case mod_dummy:
	default:
		return false;	//panic!
	}

	if (result)
	{
		CollisionMesh_CleanupMesh(ed);
		if (ed->ode.ode_numtriangles > 0)
			return true;
	}
	return false;
}
void QDECL World_ReleaseCollisionMesh(wedict_t *ed)
{
	BZ_Free(ed->ode.ode_element3i);
	ed->ode.ode_element3i = NULL;
	BZ_Free(ed->ode.ode_vertex3f);
	ed->ode.ode_vertex3f = NULL;
	ed->ode.ode_numvertices = 0;
	ed->ode.ode_numtriangles = 0;
}
#endif
#endif
