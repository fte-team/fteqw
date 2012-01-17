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
// sv_move.c -- monster movement

#include "quakedef.h"
#include "pr_common.h"

#if defined(CSQC_DAT) || !defined(CLIENTONLY)

/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
int c_yes, c_no;

qboolean World_CheckBottom (world_t *world, wedict_t *ent)
{
	int savedhull;
	vec3_t	mins, maxs, start, stop;
	trace_t	trace;
	int		x, y;
	float	mid, bottom;
	
	VectorAdd (ent->v->origin, ent->v->mins, mins);
	VectorAdd (ent->v->origin, ent->v->maxs, maxs);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (!(World_PointContents (world, start) & FTECONTENTS_SOLID))
				goto realcheck;
		}

	c_yes++;
	return true;		// we got out easy

realcheck:
	c_no++;
//
// check it for real...
//
	start[2] = mins[2];
	
// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*movevars.stepheight;
	savedhull = ent->xv->hull;
	ent->xv->hull = 0;
	trace = World_Move (world, start, vec3_origin, vec3_origin, stop, true, ent);
	ent->xv->hull = savedhull;

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];
	
// the corners must be within 16 of the midpoint	
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];
			
			savedhull = ent->xv->hull;
			ent->xv->hull = 0;
			trace = World_Move (world, start, vec3_origin, vec3_origin, stop, true, ent);
			ent->xv->hull = savedhull;
			
			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > movevars.stepheight)
				return false;
		}

	c_yes++;
	return true;
}

/*
=============
SV_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned, and
pr_global_struct->trace_normal is set to the normal of the blocking wall
=============
*/
qboolean World_movestep (world_t *world, wedict_t *ent, vec3_t move, qboolean relink, qboolean noenemy, void (*set_move_trace)(trace_t *trace, struct globalvars_s *pr_globals), struct globalvars_s *set_trace_globs)
{
	float		dz;
	vec3_t		oldorg, neworg, end;
	trace_t		trace;
	int			i;
	wedict_t	*enemy = world->edicts;
	int eflags = ent->v->flags;

	if (progstype != PROG_H2)
		eflags &= ~FLH2_NOZ|FLH2_HUNTFACE;

// try the move	
	VectorCopy (ent->v->origin, oldorg);
	VectorAdd (ent->v->origin, move, neworg);

// flying monsters don't step up
	if ( eflags & (FL_SWIM | FL_FLY) && !(eflags & (FLH2_NOZ|FLH2_HUNTFACE)))
	{
	// try one move with vertical motion, then one without
		for (i=0 ; i<2 ; i++)
		{
			VectorAdd (ent->v->origin, move, neworg);
			if (!noenemy)
			{
				enemy = (wedict_t*)PROG_TO_EDICT(world->progs, ent->v->enemy);
				if (i == 0 && enemy->entnum)
				{
					dz = ent->v->origin[2] - ((wedict_t*)PROG_TO_EDICT(world->progs, ent->v->enemy))->v->origin[2];
					if (eflags & FLH2_HUNTFACE) /*get the ent's origin_z to match its victims face*/
						dz += ((wedict_t*)PROG_TO_EDICT(world->progs, ent->v->enemy))->v->view_ofs[2];
					if (dz > 40)
						neworg[2] -= 8;
					if (dz < 30)
						neworg[2] += 8;
				}
			}
			trace = World_Move (world, ent->v->origin, ent->v->mins, ent->v->maxs, neworg, false, ent);
			if (set_move_trace)
				set_move_trace(&trace, set_trace_globs);
	
			if (trace.fraction == 1)
			{
				if ( (eflags & FL_SWIM) && !(World_PointContents(world, trace.endpos) & FTECONTENTS_FLUID))
					continue;	// swim monster left water
	
				VectorCopy (trace.endpos, ent->v->origin);
				if (relink)
					World_LinkEdict (world, ent, true);
				return true;
			}
			
			if (noenemy || !enemy->entnum)
				break;
		}
		
		return false;
	}

// push down from a step height above the wished position
	neworg[2] += movevars.stepheight;
	VectorCopy (neworg, end);
	end[2] -= movevars.stepheight*2;

	trace = World_Move (world, neworg, ent->v->mins, ent->v->maxs, end, false, ent);
	if (set_move_trace)
		set_move_trace(&trace, set_trace_globs);

	if (trace.allsolid)
		return false;

	if (trace.startsolid)
	{
		neworg[2] -= movevars.stepheight;
		trace = World_Move (world, neworg, ent->v->mins, ent->v->maxs, end, false, ent);
		if (set_move_trace)
			set_move_trace(&trace, set_trace_globs);
		if (trace.allsolid || trace.startsolid)
			return false;
	}
	if (trace.fraction == 1)
	{
	// if monster had the ground pulled out, go ahead and fall
		if ( (int)ent->v->flags & FL_PARTIALGROUND )
		{
			VectorAdd (ent->v->origin, move, ent->v->origin);
			if (relink)
				World_LinkEdict (world, ent, true);
			ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;
//	Con_Printf ("fall down\n"); 
			return true;
		}
	
		return false;		// walked off an edge
	}

// check point traces down for dangling corners
	VectorCopy (trace.endpos, ent->v->origin);
	
	if (!World_CheckBottom (world, ent))
	{
		if ( (int)ent->v->flags & FL_PARTIALGROUND )
		{	// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if (relink)
				World_LinkEdict (world, ent, true);
			return true;
		}
		VectorCopy (oldorg, ent->v->origin);
		return false;
	}

	if ( (int)ent->v->flags & FL_PARTIALGROUND )
	{
//		Con_Printf ("back on ground\n"); 
		ent->v->flags = (int)ent->v->flags & ~FL_PARTIALGROUND;
	}
	ent->v->groundentity = EDICT_TO_PROG(world->progs, trace.ent);

// the move is ok
	if (relink)
		World_LinkEdict (world, ent, true);
	return true;
}


//============================================================================

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void World_changeyaw (wedict_t *ent)
{
	float		ideal, current, move, speed;

	current = anglemod( ent->v->angles[1] );
	ideal = ent->v->ideal_yaw;
	speed = ent->v->yaw_speed;

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v->angles[1] = anglemod (current + move);
}

/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.

======================
*/
qboolean World_StepDirection (world_t *world, wedict_t *ent, float yaw, float dist)
{
	vec3_t		move, oldorigin;
	float		delta;
	
	ent->v->ideal_yaw = yaw;

	World_changeyaw(ent);

	yaw = yaw*M_PI*2 / 360;
	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

	//FIXME: Hexen2: ent flags & FL_SET_TRACE

	VectorCopy (ent->v->origin, oldorigin);
	if (World_movestep (world, ent, move, false, false, NULL, NULL))
	{
		delta = ent->v->angles[YAW] - ent->v->ideal_yaw;
		if (delta > 45 && delta < 315)
		{		// not turned far enough, so don't take the step
			VectorCopy (oldorigin, ent->v->origin);
		}
		World_LinkEdict (world, ent, true);
		return true;
	}
	World_LinkEdict (world, ent, true);
		
	return false;
}

/*
======================
SV_FixCheckBottom

======================
*/
void World_FixCheckBottom (wedict_t *ent)
{
//	Con_Printf ("SV_FixCheckBottom\n");
	
	ent->v->flags = (int)ent->v->flags | FL_PARTIALGROUND;
}



/*
================
SV_NewChaseDir

================
*/
#define	DI_NODIR	-1

void World_NewChaseDir (world_t *world, wedict_t *actor, wedict_t *enemy, float dist)
{
	float		deltax,deltay;
	float			d[3];
	float		tdir, olddir, turnaround;

	olddir = anglemod( (int)(actor->v->ideal_yaw/45)*45 );
	turnaround = anglemod(olddir - 180);

	deltax = enemy->v->origin[0] - actor->v->origin[0];
	deltay = enemy->v->origin[1] - actor->v->origin[1];
	if (deltax>10)
		d[1]= 0;
	else if (deltax<-10)
		d[1]= 180;
	else
		d[1]= DI_NODIR;
	if (deltay<-10)
		d[2]= 270;
	else if (deltay>10)
		d[2]= 90;
	else
		d[2]= DI_NODIR;

// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		if (d[1] == 0)
			tdir = d[2] == 90 ? 45 : 315;
		else
			tdir = d[2] == 90 ? 135 : 215;
			
		if (tdir != turnaround && World_StepDirection(world, actor, tdir, dist))
			return;
	}

// try other directions
	if ( ((rand()&3) & 1) ||  abs(deltay)>abs(deltax))
	{
		tdir=d[1];
		d[1]=d[2];
		d[2]=tdir;
	}

	if (d[1]!=DI_NODIR && d[1]!=turnaround 
	&& World_StepDirection(world, actor, d[1], dist))
			return;

	if (d[2]!=DI_NODIR && d[2]!=turnaround
	&& World_StepDirection(world, actor, d[2], dist))
			return;

/* there is no direct path to the player, so pick another direction */

	if (olddir!=DI_NODIR && World_StepDirection(world, actor, olddir, dist))
			return;

	if (rand()&1) 	/*randomly determine direction of search*/
	{
		for (tdir=0 ; tdir<=315 ; tdir += 45)
			if (tdir!=turnaround && World_StepDirection(world, actor, tdir, dist) )
					return;
	}
	else
	{
		for (tdir=315 ; tdir >=0 ; tdir -= 45)
			if (tdir!=turnaround && World_StepDirection(world, actor, tdir, dist) )
					return;
	}

	if (turnaround != DI_NODIR && World_StepDirection(world, actor, turnaround, dist) )
			return;

	actor->v->ideal_yaw = olddir;		// can't move

// if a bridge was pulled out from underneath a monster, it may not have
// a valid standing position at all

	if (!World_CheckBottom (world, actor))
		World_FixCheckBottom (actor);

}

/*
======================
SV_CloseEnough

======================
*/
qboolean World_CloseEnough (wedict_t *ent, wedict_t *goal, float dist)
{
	int		i;
	
	for (i=0 ; i<3 ; i++)
	{
		if (goal->v->absmin[i] > ent->v->absmax[i] + dist)
			return false;
		if (goal->v->absmax[i] < ent->v->absmin[i] - dist)
			return false;
	}
	return true;
}

/*
======================
SV_MoveToGoal

======================
*/
qboolean World_MoveToGoal (world_t *world, wedict_t *ent, float dist)
{
	wedict_t	*goal;

	ent = (wedict_t*)PROG_TO_EDICT(world->progs, *world->g.self);	
	goal = (wedict_t*)PROG_TO_EDICT(world->progs, ent->v->goalentity);

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		return false;
	}

// if the next step hits the enemy, return immediately
	if ( PROG_TO_EDICT(world->progs, ent->v->enemy) != (edict_t*)world->edicts && World_CloseEnough (ent, goal, dist) )
		return true;

// bump around...
	if ( (rand()&3)==1 ||
	!World_StepDirection (world, ent, ent->v->ideal_yaw, dist))
	{
		World_NewChaseDir (world, ent, goal, dist);
	}
	return true;
}

#endif
