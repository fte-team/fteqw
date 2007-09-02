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

#include "qwsvdef.h"

#ifndef CLIENTONLY
/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/

extern cvar_t sv_compatablehulls;

typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	int			type;
	edict_t		*passedict;
#ifdef Q2SERVER
	q2edict_t	*q2passedict;
#endif
	int hullnum;
} moveclip_t;

/*
===============================================================================

HULL BOXES

===============================================================================
*/


static	hull_t		box_hull;
static	dclipnode_t	box_clipnodes[6];
static	mplane_t	box_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void SV_InitBoxHull (void)
{
	int		i;
	int		side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	Q1BSP_SetHullFuncs(&box_hull);

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
hull_t	*SV_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/


areanode_t	sv_areanodes[AREA_NODES];
int			sv_numareanodes;

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);
	
	if (depth == AREA_DEPTH)
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
	
	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	SV_InitBoxHull ();
	
	memset (sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode (0, sv.worldmodel->mins, sv.worldmodel->maxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict (edict_t *ent)
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
edict_t *nodelinks[MAX_NODELINKS];
void SV_TouchLinks ( edict_t *ent, areanode_t *node )
{	//Spike: rewritten this function to cope with killtargets used on a few maps.
	link_t		*l, *next;
	edict_t		*touch;
	int			old_self, old_other;

	int linkcount = 0, ln;

	//work out who they are first.
	for (l = node->trigger_edicts.next ; l != &node->trigger_edicts ; l = next)
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

	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;
	for (ln = 0; ln < linkcount; ln++)
	{
		touch = nodelinks[ln];

		//make sure nothing moved it away
		if (touch->isfree)
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

		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, touch);
		pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, ent);
		pr_global_struct->time = sv.time;
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_Touch();
		else
#endif
			PR_ExecuteProgram (svprogfuncs, touch->v->touch);

		if (ent->isfree)
			break;
	}
	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;


// recurse down both sides
	if (node->axis == -1 || ent->isfree)
		return;
	
	if ( ent->v->absmax[node->axis] > node->dist )
		SV_TouchLinks ( ent, node->children[0] );
	if ( ent->v->absmin[node->axis] < node->dist )
		SV_TouchLinks ( ent, node->children[1] );
}

#ifdef Q2BSPS
void Q2BSP_FindTouchedLeafs(model_t *model, edict_t *ent)
{
#define MAX_TOTAL_ENT_LEAFS		128
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int num_leafs;
	int			topnode;
	int i, j;
	int			area;

	//ent->num_leafs == q2's ent->num_clusters
	ent->num_leafs = 0;
	ent->areanum = 0;
	ent->areanum2 = 0;

	//get all leafs, including solids
	num_leafs = CM_BoxLeafnums (model, ent->v->absmin, ent->v->absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// set areas
	for (i=0 ; i<num_leafs ; i++)
	{
		clusters[i] = CM_LeafCluster (model, leafs[i]);
		area = CM_LeafArea (model, leafs[i]);
		if (area)
		{	// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if (ent->areanum && ent->areanum != area)
			{
				if (ent->areanum2 && ent->areanum2 != area && sv.state == ss_loading)
					Con_DPrintf ("Object touching 3 areas at %f %f %f\n",
					ent->v->absmin[0], ent->v->absmin[1], ent->v->absmin[2]);
				ent->areanum2 = area;
			}
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
void SV_LinkEdict (edict_t *ent, qboolean touch_triggers)
{
	areanode_t	*node;
	
	if (ent->area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position

	ent->solidtype = ent->v->solid;
		
	if (ent == sv.edicts)
		return;		// don't add the world

	if (ent->isfree)
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

//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int)ent->v->flags & FL_ITEM)
	{
		ent->v->absmin[0] -= 15;
		ent->v->absmin[1] -= 15;
		ent->v->absmax[0] += 15;
		ent->v->absmax[1] += 15;
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
	sv.worldmodel->funcs.FindTouchedLeafs_Q1(sv.worldmodel, ent);
/*
#ifdef Q2BSPS
	if (sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3)
		Q2BSP_FindTouchedLeafs(ent);
	else
#endif
		if (sv.worldmodel->fromgame == fg_doom)
	{
	}
	else
		Q1BSP_FindTouchedLeafs(ent);
*/

	if (ent->v->solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
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
	
// link it in	

	if (ent->v->solid == SOLID_TRIGGER || ent->v->solid == SOLID_LADDER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);
	
// if touch_triggers, touch all entities at this node and decend for more
	if (touch_triggers)
		SV_TouchLinks ( ent, sv_areanodes );
}


#ifdef Q2SERVER
void VARGS SVQ2_UnlinkEdict(q2edict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}

void VARGS SVQ2_LinkEdict(q2edict_t *ent)
{
	areanode_t	*node;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int			num_leafs;
	int			i, j, k;
	int			area;
	int			topnode;

	if (ent->area.prev)
		SVQ2_UnlinkEdict (ent);	// unlink from old position
		
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

	//get all leafs, including solids
	num_leafs = CM_BoxLeafnums (sv.worldmodel, ent->absmin, ent->absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// set areas
	for (i=0 ; i<num_leafs ; i++)
	{
		clusters[i] = CM_LeafCluster (sv.worldmodel, leafs[i]);
		area = CM_LeafArea (sv.worldmodel, leafs[i]);
		if (area)
		{	// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if (ent->areanum && ent->areanum != area)
			{
				if (ent->areanum2 && ent->areanum2 != area && sv.state == ss_loading)
					Con_DPrintf ("Object touching 3 areas at %f %f %f\n",
					ent->absmin[0], ent->absmin[1], ent->absmin[2]);
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

	// if first time, make sure old_origin is valid
	if (!ent->linkcount)
	{
		VectorCopy (ent->s.origin, ent->s.old_origin);
	}
	ent->linkcount++;

	if (ent->solid == Q2SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
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
	if (ent->solid == Q2SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);

}

void SVQ2_Q1BSP_LinkEdict(q2edict_t *ent)
{
	areanode_t	*node;
	int			i, j, k;

	if (ent->area.prev)
		SVQ2_UnlinkEdict (ent);	// unlink from old position
		
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
				if (ent->areanum2 && ent->areanum2 != area && sv.state == ss_loading)
					Con_DPrintf ("Object touching 3 areas at %f %f %f\n",
					ent->absmin[0], ent->absmin[1], ent->absmin[2]);
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
	node = sv_areanodes;
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
	if (ent->solid == Q2SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);
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
int SV_PointContents (vec3_t p)
{
	return sv.worldmodel->funcs.PointContents(sv.worldmodel, p);
}

//===========================================================================

/*
============
SV_TestEntityPosition

A small wrapper around SV_BoxInSolidEntity that never clips against the
supplied entity.
============
*/
edict_t	*SV_TestEntityPosition (edict_t *ent)
{
	trace_t	trace;

	trace = SV_Move (ent->v->origin, ent->v->mins, ent->v->maxs, ent->v->origin, 0, ent);
	
	if (trace.startsolid)
		return sv.edicts;
		
	return NULL;
}

qboolean Q1BSP_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace);

//wrapper function. Rotates the start and end positions around the angles if needed.
//qboolean TransformedHullCheck (hull_t *hull, vec3_t start, vec3_t end, trace_t *trace, vec3_t angles)
qboolean TransformedTrace (struct model_s *model, int hulloverride, int frame, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct trace_s *trace, vec3_t origin, vec3_t angles)
{
	qboolean rotated;
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, right, up;
	vec3_t		temp;
	qboolean	result;

	memset (trace, 0, sizeof(trace_t));
	trace->fraction = 1;
	trace->allsolid = false;
	trace->startsolid = false;
	trace->inopen = true;	//probably wrong...
	VectorCopy (end, trace->endpos);

	// don't rotate non bsp ents. Too small to bother.
	if (model)
	{
		rotated = (angles[0] || angles[1] || angles[2]);
		if (rotated)
		{
			AngleVectors (angles, forward, right, up);

			VectorSubtract (start, origin, temp);
			start_l[0] = DotProduct (temp, forward);
			start_l[1] = -DotProduct (temp, right);
			start_l[2] = DotProduct (temp, up);

			VectorSubtract (end, origin, temp);
			end_l[0] = DotProduct (temp, forward);
			end_l[1] = -DotProduct (temp, right);
			end_l[2] = DotProduct (temp, up);
		}
		else
		{
			VectorSubtract (start, origin, start_l);
			VectorSubtract (end, origin, end_l);
		}
		result = model->funcs.Trace (model, hulloverride, frame, start_l, end_l, mins, maxs, trace);
		if (rotated)
		{
			// FIXME: figure out how to do this with existing angles

			if (trace->fraction != 1)
			{
				VectorNegate (angles, a);
				AngleVectors (a, forward, right, up);

				VectorCopy (trace->plane.normal, temp);
				trace->plane.normal[0] = DotProduct (temp, forward);
				trace->plane.normal[1] = -DotProduct (temp, right);
				trace->plane.normal[2] = DotProduct (temp, up);


				trace->endpos[0] = start[0] + trace->fraction * (end[0] - start[0]);
				trace->endpos[1] = start[1] + trace->fraction * (end[1] - start[1]);
				trace->endpos[2] = start[2] + trace->fraction * (end[2] - start[2]);
			}
			else
			{
				VectorCopy (end, trace->endpos);
			}
		}
		else
			VectorAdd (trace->endpos, origin, trace->endpos);
	}
	else
	{
		hull_t *hull = &box_hull;

		memset (trace, 0, sizeof(trace_t));
		trace->fraction = 1;
		trace->allsolid = true;

		VectorSubtract (start, origin, start_l);
		VectorSubtract (end, origin, end_l);
		VectorCopy (end_l, trace->endpos);
		result = Q1BSP_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l, trace);
		VectorAdd (trace->endpos, origin, trace->endpos);
	}

	return result;
}

qboolean TransformedNativeTrace (struct model_s *model, int hulloverride, int frame, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, unsigned int against, struct trace_s *trace, vec3_t origin, vec3_t angles)
{
	qboolean rotated;
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, right, up;
	vec3_t		temp;
	qboolean	result;

	memset (trace, 0, sizeof(trace_t));
	trace->fraction = 1;
	trace->allsolid = false;
	trace->startsolid = false;
	trace->inopen = true;	//probably wrong...
	VectorCopy (end, trace->endpos);

	// don't rotate non bsp ents. Too small to bother.
	if (model)
	{
		rotated = (angles[0] || angles[1] || angles[2]);
		if (rotated)
		{
			AngleVectors (angles, forward, right, up);

			VectorSubtract (start, origin, temp);
			start_l[0] = DotProduct (temp, forward);
			start_l[1] = -DotProduct (temp, right);
			start_l[2] = DotProduct (temp, up);

			VectorSubtract (end, origin, temp);
			end_l[0] = DotProduct (temp, forward);
			end_l[1] = -DotProduct (temp, right);
			end_l[2] = DotProduct (temp, up);
		}
		else
		{
			VectorSubtract (start, origin, start_l);
			VectorSubtract (end, origin, end_l);
		}
		result = model->funcs.NativeTrace (model, hulloverride, frame, start_l, end_l, mins, maxs, against, trace);
		if (rotated)
		{
			// FIXME: figure out how to do this with existing angles
	//		VectorNegate (angles, a);
			a[0] = -angles[0];
			a[1] = -angles[1];
			a[2] = -angles[2];
			AngleVectors (a, forward, right, up);

			VectorCopy (trace->plane.normal, temp);
			trace->plane.normal[0] = DotProduct (temp, forward);
			trace->plane.normal[1] = -DotProduct (temp, right);
			trace->plane.normal[2] = DotProduct (temp, up);

			trace->endpos[0] = start[0] + trace->fraction * (end[0] - start[0]);
			trace->endpos[1] = start[1] + trace->fraction * (end[1] - start[1]);
			trace->endpos[2] = start[2] + trace->fraction * (end[2] - start[2]);
		}
		VectorAdd (trace->endpos, origin, trace->endpos);
	}
	else
	{
		hull_t *hull = &box_hull;

		memset (trace, 0, sizeof(trace_t));
		trace->fraction = 1;
		trace->allsolid = true;

		VectorSubtract (start, origin, start_l);
		VectorSubtract (end, origin, end_l);
		VectorCopy (end_l, trace->endpos);
		result = Q1BSP_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l, trace);
		VectorAdd (trace->endpos, origin, trace->endpos);
	}

	return result;
}

/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int hullnum, qboolean hitmodel)	//hullnum overrides min/max for q1 style bsps
{
	trace_t		trace;
	model_t		*model;

/*
#ifdef Q2BSPS
	if (ent->v->solid == SOLID_BSP)
		if (sv.models[(int)ent->v->modelindex] && (sv.models[(int)ent->v->modelindex]->fromgame == fg_quake2 || sv.models[(int)ent->v->modelindex]->fromgame == fg_quake3))
		{
			trace = CM_TransformedBoxTrace (start, end, mins, maxs, sv.models[(int)ent->v->modelindex]->hulls[0].firstclipnode, MASK_PLAYERSOLID, ent->v->origin, ent->v->angles);
			if (trace.fraction < 1 || trace.startsolid)
				trace.ent = ent;
			return trace;
		}
#endif
*/

// get the clipping hull
	if (ent->v->solid == SOLID_BSP)
	{
		model = sv.models[(int)ent->v->modelindex];
		if (!model || (model->type != mod_brush && model->type != mod_heightmap))
			SV_Error("SOLID_BSP with non bsp model (classname: %s)", svprogfuncs->stringtable + ent->v->classname);
	}
	else
	{
		vec3_t boxmins, boxmaxs;
		VectorSubtract (ent->v->mins, maxs, boxmins);
		VectorSubtract (ent->v->maxs, mins, boxmaxs);
		SV_HullForBox(boxmins, boxmaxs);
		model = NULL;
	}

// trace a line through the apropriate clipping hull
	if (ent->v->solid != SOLID_BSP)
	{
		ent->v->angles[0]*=-1;	//carmack made bsp models rotate wrongly.
		TransformedTrace(model, hullnum, ent->v->frame, start, end, mins, maxs, &trace, ent->v->origin, ent->v->angles);
		ent->v->angles[0]*=-1;
	}
	else
	{
		TransformedTrace(model, hullnum, ent->v->frame, start, end, mins, maxs, &trace, ent->v->origin, ent->v->angles);
	}

// fix trace up by the offset
	if (trace.fraction != 1)
	{
		if (!model && hitmodel && ent->v->solid != SOLID_BSP && ent->v->modelindex > 0)
		{
			//okay, we hit the bbox

			model_t *model;
			if (ent->v->modelindex < 1 || ent->v->modelindex >= MAX_MODELS)
				SV_Error("SV_ClipMoveToEntity: modelindex out of range\n");
			model = sv.models[ (int)ent->v->modelindex ];
			if (!model)
			{	//if the model isn't loaded, load it.
				//this saves on memory requirements with mods that don't ever use this.
				model = sv.models[(int)ent->v->modelindex] = Mod_ForName(sv.strings.model_precache[(int)ent->v->modelindex], false);
			}

			if (model && model->funcs.Trace)
			{
				//do the second trace
				TransformedTrace(model, hullnum, ent->v->frame, start, end, mins, maxs, &trace, ent->v->origin, ent->v->angles);
			}
		}

		if (trace.startsolid)
		{
			if (ent != sv.edicts)
				Con_Printf("Trace started solid\n");
		}
	}

// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid  )
		trace.ent = ent;

	return trace;
}
#ifdef Q2SERVER
trace_t SVQ2_ClipMoveToEntity (q2edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t		trace;
	model_t		*model;

// get the clipping hull
	if (ent->s.solid == Q2SOLID_BSP)
	{
		model = sv.models[(int)ent->s.modelindex];
		if (!model || model->type != mod_brush)
			SV_Error("SOLID_BSP with non bsp model");
	}
	else
	{
		vec3_t boxmins, boxmaxs;
		VectorSubtract (ent->mins, maxs, boxmins);
		VectorSubtract (ent->maxs, mins, boxmaxs);
		SV_HullForBox(boxmins, boxmaxs);
		model = NULL;
	}

// trace a line through the apropriate clipping hull
	TransformedTrace(model, 0, 0, start, end, mins, maxs, &trace, ent->s.origin, ent->s.angles);

// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid  )
		trace.ent = (edict_t *)ent;

	return trace;
}
#endif
#ifdef Q2BSPS
float	*area_mins, *area_maxs;
edict_t	**area_list;
#ifdef Q2SERVER
q2edict_t	**area_q2list;
#endif
int		area_count, area_maxcount;
int		area_type;
#define AREA_SOLID 1
void SV_AreaEdicts_r (areanode_t *node)
{
	link_t		*l, *next, *start;
	edict_t		*check;
	int			count;

	count = 0;

	// touch linked edicts
	if (area_type == AREA_SOLID)
		start = &node->solid_edicts;
	else
		start = &node->trigger_edicts;

	for (l=start->next  ; l != start ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->v->solid == SOLID_NOT)
			continue;		// deactivated
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
		SV_AreaEdicts_r ( node->children[0] );
	if ( area_mins[node->axis] < node->dist )
		SV_AreaEdicts_r ( node->children[1] );
}

/*
================
SV_AreaEdicts
================
*/
int SV_AreaEdicts (vec3_t mins, vec3_t maxs, edict_t **list,
	int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	SV_AreaEdicts_r (sv_areanodes);

	return area_count;
}

#ifdef Q2SERVER
void SVQ2_AreaEdicts_r (areanode_t *node)
{
	link_t		*l, *next, *start;
	q2edict_t		*check;
	int			count;

	count = 0;

	// touch linked edicts
	if (area_type == AREA_SOLID)
		start = &node->solid_edicts;
	else
		start = &node->trigger_edicts;

	for (l=start->next  ; l != start ; l = next)
	{
		if (!l)
		{
			int i;
			SV_ClearWorld();
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
		SVQ2_AreaEdicts_r ( node->children[0] );
	if ( area_mins[node->axis] < node->dist )
		SVQ2_AreaEdicts_r ( node->children[1] );
}

int VARGS SVQ2_AreaEdicts (vec3_t mins, vec3_t maxs, q2edict_t **list,
	int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_q2list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	SVQ2_AreaEdicts_r (sv_areanodes);

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
static model_t *SVQ2_ModelForEntity (q2edict_t *ent)
{
	model_t	*model;

// decide which clipping hull to use, based on the size
	if (ent->solid == Q2SOLID_BSP)
	{	// explicit hulls in the BSP model
		model = sv.models[ (int)ent->s.modelindex ];

		if (!model)
			SV_Error ("Q2SOLID_BSP with a non bsp model");

		return model;
	}

	// create a temp hull from bounding box sizes

	return CM_TempBoxModel (ent->mins, ent->maxs);
}
#endif
/*
void SV_ClipMoveToEntities ( moveclip_t *clip )
{
	int			i, num;
	edict_t		*touchlist[MAX_EDICTS], *touch;
	trace_t		trace;
	int			headnode;
	float		*angles;

	int passed = clip->passedict?EDICT_TO_PROG(svprogfuncs, clip->passedict):0;

	num = SV_AreaEdicts (clip->boxmins, clip->boxmaxs, touchlist
		, MAX_EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i=0 ; i<num ; i++)
	{
		touch = touchlist[i];
		if (touch->v->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (clip->trace.allsolid)
			return;
		if (clip->passedict)
		{
		 	if (touch->v->owner == passed)
				continue;	// don't clip against own missiles
			if (clip->passedict->v->owner == EDICT_TO_PROG(svprogfuncs, touch))
				continue;	// don't clip against owner
		}

		if (clip->type & MOVE_NOMONSTERS && touch->v->solid != SOLID_BSP)
			continue;

		// don't clip corpse against character
		if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
			continue;
		// don't clip character against corpse
		if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
			continue;

		if (!((int)clip->passedict->xv->dimension_hit & (int)touch->xv->dimension_solid))
			continue;

//		if ( !(clip->contentmask & CONTENTS_DEADMONSTER)
//		&& (touch->svflags & SVF_DEADMONSTER) )
//				continue;

		// might intersect, so do an exact clip
		headnode = SV_HeadnodeForEntity (touch);
		angles = touch->v->angles;
		if (touch->v->solid != SOLID_BSP)
			angles = vec3_origin;	// boxes don't rotate

		if ((int)touch->v->flags & FL_MONSTER)
			trace = CM_TransformedBoxTrace (clip->start, clip->end,
				clip->mins2, clip->maxs2, headnode, MASK_PLAYERSOLID,
				touch->v->origin, angles);
		else
			trace = CM_TransformedBoxTrace (clip->start, clip->end,
				clip->mins, clip->maxs, headnode,  MASK_PLAYERSOLID,
				touch->v->origin, angles);

		if (trace.allsolid || trace.startsolid ||
				trace.fraction < clip->trace.fraction)
		{
			if (clip->type & MOVE_HITMODEL && touch->v->solid != SOLID_BSP)
			{
				model_t *model;
				if (touch->v->modelindex < 1 || touch->v->modelindex >= MAX_MODELS)
					SV_Error("SV_ClipMoveToEntity: modelindex out of range\n");
				model = sv.models[ (int)touch->v->modelindex ];
				if (!model)
				{	//if the model isn't loaded, load it.
					//this saves on memory requirements with mods that don't ever use this.
					model = sv.models[(int)touch->v->modelindex] = Mod_ForName(sv.model_precache[(int)touch->v->modelindex], false);
				}

				if (model && model->hulls[0].available)
				{
					hull_t *hull = &model->hulls[0];
					//do the second trace
					memset (&trace, 0, sizeof(trace_t));
					trace.fraction = 1;
					trace.allsolid = true;
					TransformedHullCheck(hull, clip->start, clip->end, &trace, touch->v->angles);

					if (trace.fraction < clip->trace.fraction)
					{
						trace.ent = touch;
						clip->trace = trace;
					}
				}
			}
			else
			{
				trace.ent = touch;
				clip->trace = trace;
			}
		}
	}
}
*/
#ifdef Q2SERVER
void SVQ2_ClipMoveToEntities ( moveclip_t *clip, int contentsmask )
{
	int			i, num;
	q2edict_t		*touchlist[MAX_EDICTS], *touch;
	trace_t		trace;
	model_t		*model;
	float		*angles;

	num = SVQ2_AreaEdicts (clip->boxmins, clip->boxmaxs, touchlist
		, MAX_EDICTS, AREA_SOLID);

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
		if ( !(contentsmask & Q2CONTENTS_DEADMONSTER))
				continue;

		// might intersect, so do an exact clip
		model = SVQ2_ModelForEntity (touch);
		angles = touch->s.angles;
		if (touch->solid != Q2SOLID_BSP)
			angles = vec3_origin;	// boxes don't rotate

		if (touch->svflags & SVF_MONSTER)
			trace = CM_TransformedBoxTrace (model, clip->start, clip->end,
				clip->mins2, clip->maxs2, contentsmask,
				touch->s.origin, angles);
		else
			trace = CM_TransformedBoxTrace (model, clip->start, clip->end,
				clip->mins, clip->maxs, contentsmask,
				touch->s.origin, angles);

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


/*
====================
SV_ClipToEverything

like SV_ClipToLinks, but doesn't use the links part. This can be used for checking triggers, solid entities, not-solid entities.
Sounds pointless, I know.
====================
*/
void SV_ClipToEverything (moveclip_t *clip)
{
	int e;
	trace_t		trace;
	edict_t		*touch;
	for (e=1 ; e<sv.num_edicts ; e++)
	{
		touch = EDICT_NUM(svprogfuncs, e);

		if (touch->isfree)
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
			// don't clip corpse against character
			if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
				continue;
			// don't clip character against corpse
			if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
				continue;

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
		if (clip->trace.allsolid)
			return;
		if (clip->passedict)
		{
		 	if (PROG_TO_EDICT(svprogfuncs, touch->v->owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(svprogfuncs, clip->passedict->v->owner) == touch)
				continue;	// don't clip against owner
		}

		if ((int)touch->v->flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2, clip->maxs2, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL);
		else
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins, clip->maxs, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL);
		if (trace.allsolid || trace.startsolid ||
				trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
			clip->trace = trace;
		}
	}
}

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToLinks ( areanode_t *node, moveclip_t *clip )
{
	link_t		*l, *next;
	edict_t		*touch;
	trace_t		trace;

	if (clip->type & MOVE_TRIGGERS)
	{
		for (l = node->trigger_edicts.next ; l != &node->trigger_edicts ; l = next)
		{
			next = l->next;
			touch = EDICT_FROM_AREA(l);
			if (!((int)touch->v->flags & FL_FINDABLE_NONSOLID))
				continue;
			if (touch->v->solid != SOLID_TRIGGER)
				continue;
			if (touch == clip->passedict)
				continue;

			if (clip->type & MOVE_NOMONSTERS && touch->v->solid != SOLID_BSP)
				continue;

			if (clip->passedict)
			{
	/* These can never happen, touch is a SOLID_TRIGGER
				// don't clip corpse against character
				if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
					continue;
				// don't clip character against corpse
				if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
					continue;
	*/
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
			if (clip->trace.allsolid)
				return;
			if (clip->passedict)
			{
		 		if (PROG_TO_EDICT(svprogfuncs, touch->v->owner) == clip->passedict)
					continue;	// don't clip against own missiles
				if (PROG_TO_EDICT(svprogfuncs, clip->passedict->v->owner) == touch)
					continue;	// don't clip against owner
			}

			if ((int)touch->v->flags & FL_MONSTER)
				trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2, clip->maxs2, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL);
			else
				trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins, clip->maxs, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL);
			if (trace.allsolid || trace.startsolid ||
					trace.fraction < clip->trace.fraction)
			{
				trace.ent = touch;
				clip->trace = trace;
			}
		}
	}

// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch->v->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (touch->v->solid == SOLID_TRIGGER || touch->v->solid == SOLID_LADDER)
			SV_Error ("Trigger in clipping list");

		if (clip->type & MOVE_NOMONSTERS && touch->v->solid != SOLID_BSP)
			continue;

		if (clip->passedict)
		{
			// don't clip corpse against character
			if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
				continue;
			// don't clip character against corpse
			if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
				continue;

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
		if (clip->trace.allsolid)
			return;
		if (clip->passedict)
		{
		 	if (PROG_TO_EDICT(svprogfuncs, touch->v->owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(svprogfuncs, clip->passedict->v->owner) == touch)
				continue;	// don't clip against owner
		}

		if ((int)touch->v->flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2, clip->maxs2, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL);
		else
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins, clip->maxs, clip->end, clip->hullnum, clip->type & MOVE_HITMODEL);
		if (trace.allsolid || trace.startsolid ||
		trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
			clip->trace = trace;
		}
	}
	
// recurse down both sides
	if (node->axis == -1)
		return;

	if ( clip->boxmaxs[node->axis] > node->dist )
		SV_ClipToLinks ( node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		SV_ClipToLinks ( node->children[1], clip );
}
#ifdef Q2SERVER
void SVQ2_ClipToLinks ( areanode_t *node, moveclip_t *clip )
{
	link_t		*l, *next;
	q2edict_t		*touch;
	trace_t		trace;

// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
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

//		if ((int)touch->s.flags & FL_MONSTER)
//			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2, clip->maxs2, clip->end);
//		else
			trace = SVQ2_ClipMoveToEntity (touch, clip->start, clip->mins, clip->maxs, clip->end);

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
		SV_ClipToLinks ( node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		SV_ClipToLinks ( node->children[1], clip );
}
#endif
/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
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
trace_t SV_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict)
{
	moveclip_t	clip;
	int			i;
	int hullnum;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	if (passedict && passedict->xv->hull)
		hullnum = passedict->xv->hull;
	else if (sv_compatablehulls.value)
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
			if (!sv.worldmodel->hulls[i].available)
				continue;
#define sq(x) ((x)*(x))
			diff = sq(sv.worldmodel->hulls[i].clip_maxs[2] - maxs[2]) +
				sq(sv.worldmodel->hulls[i].clip_mins[2] - mins[2]) +
				sq(sv.worldmodel->hulls[i].clip_maxs[1] - maxs[1]) +
				sq(sv.worldmodel->hulls[i].clip_mins[0] - mins[0]);
			if (diff < best)
			{
				best = diff;
				hullnum=i;
			}
		}
		hullnum++;
	}

// clip to world
	clip.trace = SV_ClipMoveToEntity ( sv.edicts, start, mins, maxs, end, hullnum, false);

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = passedict;
	clip.hullnum = hullnum;
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
	SV_MoveBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

// clip to entities
	if (clip.type & MOVE_EVERYTHING)
		SV_ClipToEverything (&clip);
	else
		SV_ClipToLinks ( sv_areanodes, &clip );

	if (clip.trace.startsolid)
		clip.trace.fraction = 0;

	if (!clip.trace.ent)
		return clip.trace;

	return clip.trace;
}
#ifdef Q2SERVER
trace_t SVQ2_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, q2edict_t *passedict)
{
	moveclip_t	clip;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

// clip to world
	clip.trace = CM_BoxTrace(sv.worldmodel, start, end, mins, maxs, type);//SVQ2_ClipMoveToEntity ( ge->edicts, start, mins, maxs, end );
	clip.trace.ent = (edict_t *)ge->edicts;

	if (clip.trace.fraction == 0)
		return clip.trace;

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = NULL;
	clip.q2passedict = passedict;

	VectorCopy (mins, clip.mins2);
	VectorCopy (maxs, clip.maxs2);
	
// create the bounding box of the entire move
	SV_MoveBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

// clip to entities
#ifdef Q2BSPS
	if (sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3)
		SVQ2_ClipMoveToEntities(&clip, type);
	else
#endif
		SVQ2_ClipToLinks ( sv_areanodes, &clip );

	return clip.trace;
}
#endif

#endif
