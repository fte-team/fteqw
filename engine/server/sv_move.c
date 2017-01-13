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

hull_t *Q1BSP_ChooseHull(model_t *model, int hullnum, vec3_t mins, vec3_t maxs, vec3_t offset);

//this function is axial. major axis determines ground. if it switches slightly, a new axis may become the ground...
qboolean World_CheckBottom (world_t *world, wedict_t *ent, vec3_t up)
{
	vec3_t	mins, maxs, start, stop;
	trace_t	trace;
	int		x, y;
	float	mid;

	int a0,a1,a2;	//logical x, y, z
	int sign;

	mins[0] = fabs(up[0]);
	mins[1] = fabs(up[1]);
	mins[2] = fabs(up[2]);
	if (mins[2] > mins[0] && mins[2] > mins[1])
	{
		a0 = 0;
		a1 = 1;
		a2 = 2;
	}
	else
	{
		a2 = mins[1] > mins[0];
		a0 = 1 - a2;
		a1 = 2;
	}
	sign = (up[a2]>0)?1:-1;
	
	VectorAdd (ent->v->origin, ent->v->mins, mins);
#ifdef Q1BSPS
	if (world->worldmodel->fromgame == fg_quake || world->worldmodel->fromgame == fg_halflife)
	{
		//quake's hulls are weird. sizes are defined as from mins to mins+hullsize. the actual maxs is ignored other than for its size.
		hull_t *hull;
		hull = Q1BSP_ChooseHull(world->worldmodel, ent->xv->hull, ent->v->mins, ent->v->maxs, start);
		VectorAdd (mins, start, mins);
		VectorSubtract (mins, hull->clip_mins, maxs);
		VectorAdd (maxs, hull->clip_maxs, maxs);
	}
	else
#endif
		VectorAdd (ent->v->origin, ent->v->maxs, maxs);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
	start[a2] = (sign<0)?maxs[a2]:mins[a2] - sign;
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[a0] = x ? maxs[a0] : mins[a0];
			start[a1] = y ? maxs[a1] : mins[a1];
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
	start[a2] = (sign<0)?maxs[a2]:mins[a2];
	
// the midpoint must be within 16 of the bottom
	start[a0] = stop[a0] = (mins[a0] + maxs[a0])*0.5;
	start[a1] = stop[a1] = (mins[a1] + maxs[a1])*0.5;
	stop[a2] = start[a2] - 2*movevars.stepheight*sign;
	trace = World_Move (world, start, vec3_origin, vec3_origin, stop, true|MOVE_IGNOREHULL, ent);

	if (trace.fraction == 1.0)
		return false;

	mid = trace.endpos[2];

	mid = (mid-start[a2]-(movevars.stepheight*sign)) / (stop[a2]-start[a2]);
	
// the corners must be within 16 of the midpoint	
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[a0] = stop[a0] = x ? maxs[a0] : mins[a0];
			start[a1] = stop[a1] = y ? maxs[a1] : mins[a1];
			
			trace = World_Move (world, start, vec3_origin, vec3_origin, stop, true|MOVE_IGNOREHULL, ent);
	
			if (trace.fraction == 1.0 || trace.fraction > mid)//mid - trace.endpos[2] > movevars.stepheight)
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
qboolean World_movestep (world_t *world, wedict_t *ent, vec3_t move, vec3_t axis[3], qboolean relink, qboolean noenemy, void (*set_move_trace)(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals, trace_t *trace), struct globalvars_s *set_trace_globs)
{
	float		dz;
	vec3_t		oldorg, neworg, end;
	trace_t		trace;
	int			i;
	wedict_t	*enemy = world->edicts;
	int eflags = ent->v->flags;
	vec3_t		eaxis[3];

	if (!axis)
	{
		//fixme?
		World_GetEntGravityAxis(ent, eaxis);
		axis = eaxis;
	}

#ifndef CLIENTONLY
	if (progstype != PROG_H2 || world != &sv.world)
#endif
		eflags &= ~FLH2_NOZ|FLH2_HUNTFACE;

// try the move	
	VectorCopy (ent->v->origin, oldorg);
	VectorAdd (ent->v->origin, move, neworg);

// flying monsters don't step up
	if ((eflags & (FL_SWIM | FL_FLY)) && !(eflags & (FLH2_NOZ|FLH2_HUNTFACE)))
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
					VectorSubtract(ent->v->origin, ((wedict_t*)PROG_TO_EDICT(world->progs, ent->v->enemy))->v->origin, end);
					dz = DotProduct(end, axis[2]);
					if (eflags & FLH2_HUNTFACE) /*get the ent's origin_z to match its victims face*/
						dz += ((wedict_t*)PROG_TO_EDICT(world->progs, ent->v->enemy))->v->view_ofs[2];
					if (dz > 40)
						VectorMA(neworg, -8, axis[2], neworg);
					if (dz < 30)
						VectorMA(neworg, 8, axis[2], neworg);
				}
			}
			trace = World_Move (world, ent->v->origin, ent->v->mins, ent->v->maxs, neworg, false, ent);
			if (set_move_trace)
				set_move_trace(world->progs, set_trace_globs, &trace);
	
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
	VectorMA(neworg, movevars.stepheight, axis[2], neworg);
	VectorMA(neworg, movevars.stepheight*-2, axis[2], end);

	trace = World_Move (world, neworg, ent->v->mins, ent->v->maxs, end, false, ent);
	if (set_move_trace)
		set_move_trace(world->progs, set_trace_globs, &trace);

	if (trace.allsolid)
		return false;

	if (trace.startsolid)
	{
		//move up by an extra step, if needed
		VectorMA(neworg, -movevars.stepheight, axis[2], neworg);
		trace = World_Move (world, neworg, ent->v->mins, ent->v->maxs, end, false, ent);
		if (set_move_trace)
			set_move_trace(world->progs, set_trace_globs, &trace);
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
	
	if (!World_CheckBottom (world, ent, axis[2]))
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

qboolean World_GetEntGravityAxis(wedict_t *ent, vec3_t axis[3])
{
	if (ent->xv->gravitydir[0] || ent->xv->gravitydir[1] || ent->xv->gravitydir[2])
	{
		void PerpendicularVector( vec3_t dst, const vec3_t src );
		VectorNegate(ent->xv->gravitydir, axis[2]);
		VectorNormalize(axis[2]);
		PerpendicularVector(axis[0], axis[2]);
		VectorNormalize(axis[0]);
		CrossProduct(axis[2], axis[0], axis[1]);
		VectorNormalize(axis[1]);
		return true;
	}
	else
	{
		VectorSet(axis[0], 1, 0, 0);
		VectorSet(axis[1], 0, 1, 0);
		VectorSet(axis[2], 0, 0, 1);
		return false;
	}
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
float World_changeyaw (wedict_t *ent)
{
	float		ideal, current, move, speed;
	vec3_t surf[3];
	if (World_GetEntGravityAxis(ent, surf))
	{
		//complex matrix stuff
		float mat[16];
		float surfm[16], invsurfm[16];
		float viewm[16];
		vec3_t view[4];
		vec3_t vang;

		/*calc current view matrix relative to the surface*/
		ent->v->angles[PITCH] *= -1;
		AngleVectors(ent->v->angles, view[0], view[1], view[2]);
		VectorNegate(view[1], view[1]);

		World_GetEntGravityAxis(ent, surf);

		Matrix4x4_RM_FromVectors(surfm, surf[0], surf[1], surf[2], vec3_origin);
		Matrix3x4_InvertTo4x4_Simple(surfm, invsurfm);

		/*calc current view matrix relative to the surface*/
		Matrix4x4_RM_FromVectors(viewm, view[0], view[1], view[2], vec3_origin);
		Matrix4_Multiply(viewm, invsurfm, mat);
		/*convert that back to angles*/
		Matrix3x4_RM_ToVectors(mat, view[0], view[1], view[2], view[3]);
		VectorAngles(view[0], view[2], vang);

		/*edit it*/

		ideal = ent->v->ideal_yaw;
		speed = ent->v->yaw_speed;
		move = ideal - anglemod(vang[YAW]);
		if (move > 180)
			move -= 360;
		else if (move < -180)
			move += 360;
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
		vang[YAW] = anglemod(vang[YAW] + move);

		/*clamp pitch, kill roll. monsters don't pitch/roll.*/
		vang[PITCH] = 0;
		vang[ROLL] = 0;

		move = ideal - vang[YAW];

		/*turn those angles back to a matrix*/
		AngleVectors(vang, view[0], view[1], view[2]);
		VectorNegate(view[1], view[1]);
		Matrix4x4_RM_FromVectors(mat, view[0], view[1], view[2], vec3_origin);
		/*rotate back into world space*/
		Matrix4_Multiply(mat, surfm, viewm);
		/*and figure out the final result*/
		Matrix3x4_RM_ToVectors(viewm, view[0], view[1], view[2], view[3]);
		VectorAngles(view[0], view[2], ent->v->angles);

		//make sure everything is sane
		ent->v->angles[PITCH] = anglemod(ent->v->angles[PITCH]);
		ent->v->angles[YAW] = anglemod(ent->v->angles[YAW]);
		ent->v->angles[ROLL] = anglemod(ent->v->angles[ROLL]);
		return move;
	}


	//FIXME: gravitydir. reorient the angles to change the yaw with respect to the current ground surface.

	current = anglemod( ent->v->angles[1] );
	ideal = ent->v->ideal_yaw;
	speed = ent->v->yaw_speed;

	if (current == ideal)
		return 0;
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

	return ideal - ent->v->angles[1];
}

/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.

======================
*/
qboolean World_StepDirection (world_t *world, wedict_t *ent, float yaw, float dist, vec3_t axis[3])
{
	vec3_t		move, oldorigin;
	float		delta, s;
	
	ent->v->ideal_yaw = yaw;

	delta = World_changeyaw(ent);

	yaw = yaw*M_PI*2 / 360;

	s = cos(yaw)*dist;
	VectorScale(axis[0], s, move);
	s = sin(yaw)*dist;
	VectorMA(move, s, axis[1], move);

	//FIXME: Hexen2: ent flags & FL_SET_TRACE

	VectorCopy (ent->v->origin, oldorigin);
	if (World_movestep (world, ent, move, axis, false, false, NULL, NULL))
	{
		delta = anglemod(delta);
		if (delta > 45 && delta < 315)
		{	// not turned far enough, so don't take the step
			//FIXME: surely this is noticably inefficient?
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

void World_NewChaseDir (world_t *world, wedict_t *actor, wedict_t *enemy, float dist, vec3_t axis[3])
{
	float		deltax,deltay;
	float			d[3];
	float		tdir, olddir, turnaround;

	olddir = anglemod( (int)(actor->v->ideal_yaw/45)*45 );
	turnaround = anglemod(olddir - 180);

	VectorSubtract(enemy->v->origin, actor->v->origin, d);
	deltax = DotProduct(d, axis[0]);
	deltay = DotProduct(d, axis[1]);
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
			
		if (tdir != turnaround && World_StepDirection(world, actor, tdir, dist, axis))
			return;
	}

// try other directions
	if ( ((rand()&3) & 1) ||  fabs(deltay)>fabs(deltax))
	{
		tdir=d[1];
		d[1]=d[2];
		d[2]=tdir;
	}

	if (d[1]!=DI_NODIR && d[1]!=turnaround 
	&& World_StepDirection(world, actor, d[1], dist, axis))
			return;

	if (d[2]!=DI_NODIR && d[2]!=turnaround
	&& World_StepDirection(world, actor, d[2], dist, axis))
			return;

/* there is no direct path to the player, so pick another direction */

	if (olddir!=DI_NODIR && World_StepDirection(world, actor, olddir, dist, axis))
			return;

	if (rand()&1) 	/*randomly determine direction of search*/
	{
		for (tdir=0 ; tdir<=315 ; tdir += 45)
			if (tdir!=turnaround && World_StepDirection(world, actor, tdir, dist, axis) )
					return;
	}
	else
	{
		for (tdir=315 ; tdir >=0 ; tdir -= 45)
			if (tdir!=turnaround && World_StepDirection(world, actor, tdir, dist, axis) )
					return;
	}

	if (turnaround != DI_NODIR && World_StepDirection(world, actor, turnaround, dist, axis) )
			return;

	actor->v->ideal_yaw = olddir;		// can't move

// if a bridge was pulled out from underneath a monster, it may not have
// a valid standing position at all

	if (!World_CheckBottom (world, actor, axis[2]))
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
	vec3_t axis[3];

	ent = (wedict_t*)PROG_TO_EDICT(world->progs, *world->g.self);	
	goal = (wedict_t*)PROG_TO_EDICT(world->progs, ent->v->goalentity);

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		return false;
	}

// if the next step hits the enemy, return immediately
	if ( PROG_TO_EDICT(world->progs, ent->v->enemy) != (edict_t*)world->edicts && World_CloseEnough (ent, goal, dist) )
		return true;


	World_GetEntGravityAxis(ent, axis);

// bump around...
	if ( (rand()&3)==1 ||
	!World_StepDirection (world, ent, ent->v->ideal_yaw, dist, axis))
	{
		World_NewChaseDir (world, ent, goal, dist, axis);
	}
	return true;
}

#endif
