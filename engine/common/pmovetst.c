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
#include "quakedef.h"

qboolean PM_TransformedHullCheck (model_t *model, vec3_t start, vec3_t end, trace_t *trace, vec3_t origin, vec3_t angles);
int Q1BSP_HullPointContents(hull_t *hull, vec3_t p);
static	hull_t		box_hull;
static	dclipnode_t	box_clipnodes[6];
static	mplane_t	box_planes[6];

extern	vec3_t player_mins;
extern	vec3_t player_maxs;

/*
===================
PM_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void PM_InitBoxHull (void)
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
PM_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t	*PM_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}


int PM_TransformedModelPointContents (model_t *mod, vec3_t p, vec3_t origin, vec3_t angles)
{
	vec3_t p_l, forward, up, right, temp;
	VectorSubtract (p, origin, p_l);

	// rotate start and end into the models frame of reference
	if (angles[0] || angles[1] || angles[2])
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	return mod->funcs.PointContents(mod, p_l);
}


/*
==================
PM_PointContents

==================
*/
int PM_PointContents (vec3_t p)
{
	int			num;

	int pc;
	physent_t *pe;
	model_t *pm;

	pm = pmove.physents[0].model;
	pc = pm->funcs.PointContents(pm, p);
	//we need this for e2m2 - waterjumping on to plats wouldn't work otherwise.
	for (num = 1; num < pmove.numphysent; num++)
	{
		pe = &pmove.physents[num];
		pm = pe->model;
		if (pm)
		{
			if (pe->forcecontentsmask)
			{
				if (PM_TransformedModelPointContents(pm, p, pe->origin, pe->angles))
					pc |= pe->forcecontentsmask;
			}
			else
			{
				if (pe->nonsolid)
					continue;
				pc |= PM_TransformedModelPointContents(pm, p, pe->origin, pe->angles);
			}
		}
		else if (pe->forcecontentsmask)
		{
			if (p[0] >= pe->mins[0] && p[0] <= pe->maxs[0] && 
				p[1] >= pe->mins[1] && p[1] <= pe->maxs[1] &&
				p[2] >= pe->mins[2] && p[2] <= pe->maxs[2])
				pc |= pe->forcecontentsmask;
		}
	}

	return pc;
}

int PM_ExtraBoxContents (vec3_t p)
{
	int			num;

	int pc = 0;
	physent_t *pe;
	model_t *pm;
	trace_t tr;

	for (num = 1; num < pmove.numphysent; num++)
	{
		pe = &pmove.physents[num];
		if (!pe->nonsolid)
			continue;
		pm = pe->model;
		if (pm)
		{
			if (pe->forcecontentsmask)
			{
				PM_TransformedHullCheck(pm, p, p, &tr, pe->origin, pe->angles);
				if (tr.startsolid)
					pc |= pe->forcecontentsmask;
			}
		}
		else if (pe->forcecontentsmask)
		{
			if (p[0]+player_maxs[0] >= pe->mins[0] && p[0]+player_mins[0] <= pe->maxs[0] && 
				p[1]+player_maxs[1] >= pe->mins[1] && p[1]+player_mins[1] <= pe->maxs[1] &&
				p[2]+player_maxs[2] >= pe->mins[2] && p[2]+player_mins[2] <= pe->maxs[2])
				pc |= pe->forcecontentsmask;
		}
	}

	return pc;
}

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

vec3_t trace_extents;


qboolean PM_TransformedHullCheck (model_t *model, vec3_t start, vec3_t end, trace_t *trace, vec3_t origin, vec3_t angles)
{
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, right, up;
	vec3_t		temp;
	qboolean	rotated;
	qboolean	result;

	// subtract origin offset
	VectorSubtract (start, origin, start_l);
	VectorSubtract (end, origin, end_l);

	// rotate start and end into the models frame of reference
	if (model && 
	(angles[0] || angles[1] || angles[2]) )
		rotated = true;
	else
		rotated = false;

	if (rotated)
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (start_l, temp);
		start_l[0] = DotProduct (temp, forward);
		start_l[1] = -DotProduct (temp, right);
		start_l[2] = DotProduct (temp, up);

		VectorCopy (end_l, temp);
		end_l[0] = DotProduct (temp, forward);
		end_l[1] = -DotProduct (temp, right);
		end_l[2] = DotProduct (temp, up);
	}
	// sweep the box through the model

	if (model && model->funcs.Trace)
		result = model->funcs.Trace(model, 0, 0, start_l, end_l, player_mins, player_maxs, trace);
	else
	{
		result = Q1BSP_RecursiveHullCheck (&box_hull, box_hull.firstclipnode, 0, 1, start_l, end_l, trace);
	}

	if (rotated)
	{
		// FIXME: figure out how to do this with existing angles
//		VectorNegate (angles, a);

		if (trace->fraction != 1.0)
		{
			a[0] = -angles[0];
			a[1] = -angles[1];
			a[2] = -angles[2];
			AngleVectors (a, forward, right, up);

			VectorCopy (trace->plane.normal, temp);
			trace->plane.normal[0] = DotProduct (temp, forward);
			trace->plane.normal[1] = -DotProduct (temp, right);
			trace->plane.normal[2] = DotProduct (temp, up);
		}
		trace->endpos[0] = start[0] + trace->fraction * (end[0] - start[0]);
		trace->endpos[1] = start[1] + trace->fraction * (end[1] - start[1]);
		trace->endpos[2] = start[2] + trace->fraction * (end[2] - start[2]);
	}
	else
	{
		trace->endpos[0] += origin[0];
		trace->endpos[1] += origin[1];
		trace->endpos[2] += origin[2];
	}

	return result;
}

/*
================
PM_TestPlayerPosition

Returns false if the given player position is not valid (in solid)
================
*/
qboolean PM_TestPlayerPosition (vec3_t pos)
{
	int			i;
	physent_t	*pe;
	vec3_t		mins, maxs;
	hull_t		*hull;
	trace_t		trace;

	for (i=0 ; i< pmove.numphysent ; i++)
	{
		pe = &pmove.physents[i];

		if (pe->nonsolid)
			continue;

	// get the clipping hull
		if (pe->model)
		{
/*
#ifdef Q2BSPS
			if (pe->model->fromgame == fg_quake2 || pe->model->fromgame == fg_quake3)
			{
				trace_t trace = CM_TransformedBoxTrace(pe->model, pos, pos, player_mins, player_maxs, MASK_PLAYERSOLID, pe->origin, pe->angles);
				if (trace.fraction == 0)
					return false;
				continue;
			}
#endif*/

			PM_TransformedHullCheck (pe->model, pos, pos, &trace, pe->origin, pe->angles);
			if (trace.allsolid)
				return false;	//solid
		}
		else
		{
			VectorSubtract (pe->mins, player_maxs, mins);
			VectorSubtract (pe->maxs, player_mins, maxs);
			hull = PM_HullForBox (mins, maxs);
			VectorSubtract(pos, pe->origin, mins);

			if (Q1BSP_HullPointContents(hull, mins) & FTECONTENTS_SOLID)
				return false;

//			if (Q1BSP_Trace(&box_hull, 0, 0, pe->origin, pe->origin, pe->mins, pe->maxs, pos, pe->origin, pe->angles) & FTECONTENTS_SOLID)
//				return false;
		}
	}

	return true;
}

/*
================
PM_PlayerTrace
================
*/
trace_t PM_PlayerTrace (vec3_t start, vec3_t end)
{
	trace_t		trace, total;
	int			i;
	physent_t	*pe;

// fill in a default trace
	memset (&total, 0, sizeof(trace_t));
	total.fraction = 1;
	total.entnum = -1;
	VectorCopy (end, total.endpos);

	for (i=0 ; i< pmove.numphysent ; i++)
	{
		pe = &pmove.physents[i];

		if (pe->nonsolid)
			continue;

		if (!pe->model)
		{
			vec3_t mins, maxs;
			VectorSubtract (pe->mins, player_maxs, mins);
			VectorSubtract (pe->maxs, player_mins, maxs);
			PM_HullForBox (mins, maxs);
		}

	// trace a line through the apropriate clipping hull
		PM_TransformedHullCheck (pe->model, start, end, &trace, pe->origin, pe->angles);

		if (trace.allsolid)
			trace.startsolid = true;
		if (trace.startsolid)
		{
//			if (!pmove.physents[i].model)	//caught inside annother model
//				continue;	//don't count this.
			trace.fraction = 0;
		}

	// did we clip the move?
		if (trace.fraction < total.fraction)
		{
			// fix trace up by the offset
			total = trace;
			total.entnum = i;
		}

	}

	return total;
}

//for use outside the pmove code. lame, but works.
trace_t PM_TraceLine (vec3_t start, vec3_t end)
{
	VectorClear(player_mins);
	VectorClear(player_maxs);
	pmove.hullnum = 0;
	return PM_PlayerTrace(start, end);
}
