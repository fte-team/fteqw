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
// sv_phys.c

#include "qwsvdef.h"

extern nqglobalvars_t realpr_nqglobal_struct;

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects 

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

cvar_t	sv_maxvelocity = {"sv_maxvelocity","2000"}; 

cvar_t	sv_gravity			 = { "sv_gravity", "800"};    
cvar_t	sv_stopspeed		 = { "sv_stopspeed", "100"};    
cvar_t	sv_maxspeed			 = { "sv_maxspeed", "320"};    
cvar_t	sv_spectatormaxspeed = { "sv_spectatormaxspeed", "500"};
cvar_t	sv_accelerate		 = { "sv_accelerate", "10"};     
cvar_t	sv_airaccelerate	 = { "sv_airaccelerate", "0.7"};    
cvar_t	sv_wateraccelerate	 = { "sv_wateraccelerate", "10"};     
cvar_t	sv_friction			 = { "sv_friction", "4"};      
cvar_t	sv_waterfriction	 = { "sv_waterfriction", "4"};     

cvar_t	pm_ktjump			 = {"pm_ktjump", "", NULL, CVAR_SERVERINFO};
cvar_t	pm_bunnyspeedcap	 = {"pm_bunnyspeedcap", "", NULL, CVAR_SERVERINFO};
cvar_t	pm_slidefix			 = {"pm_slidefix", "", NULL, CVAR_SERVERINFO};
cvar_t	pm_airstep			 = {"pm_airstep", "", NULL, CVAR_SERVERINFO}; 

extern cvar_t sv_nomsec;


#define	MOVE_EPSILON	0.01

void SV_Physics_Toss (edict_t *ent);

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts (void)
{
	int			e;
	edict_t		*check;

// see if any solid entities are inside the final position
	for (e=1 ; e<sv.num_edicts ; e++)
	{
		check = EDICT_NUM(svprogfuncs, e);
		if (check->isfree)
			continue;
		if (check->v.movetype == MOVETYPE_PUSH
		|| check->v.movetype == MOVETYPE_NONE
		|| check->v.movetype == MOVETYPE_FOLLOW
		|| check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		if (SV_TestEntityPosition (check))
			Con_Printf ("entity in invalid position\n");
	}
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity (edict_t *ent)
{
	int		i;

//
// bound velocity
//
	for (i=0 ; i<3 ; i++)
	{
		if (IS_NAN(ent->v.velocity[i]))
		{
			Con_Printf ("Got a NaN velocity on %s\n", PR_GetString(svprogfuncs, ent->v.classname));
			ent->v.velocity[i] = 0;
		}
		if (IS_NAN(ent->v.origin[i]))
		{
			Con_Printf ("Got a NaN origin on %s\n", PR_GetString(svprogfuncs, ent->v.classname));
			ent->v.origin[i] = 0;
		}
	}

	if (Length(ent->v.velocity) > sv_maxvelocity.value)
	{
		Con_DPrintf("Slowing %s\n", PR_GetString(svprogfuncs, ent->v.classname));
		VectorScale (ent->v.velocity, sv_maxvelocity.value/Length(ent->v.velocity), ent->v.velocity);
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
qboolean SV_RunThink (edict_t *ent)
{
	float	thinktime;

	do
	{
		thinktime = ent->v.nextthink;
		if (thinktime <= 0)
			return true;
		if (thinktime > sv.time + host_frametime)
			return true;
		
		if (thinktime < sv.time)
			thinktime = sv.time;	// don't let things stay in the past.
									// it is possible to start that way
									// by a trigger with a local time.
		ent->v.nextthink = 0;

		pr_global_struct->time = thinktime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
		pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, sv.edicts);
		PR_ExecuteProgram (svprogfuncs, ent->v.think);

		if (ent->isfree)
			return false;

		if (ent->v.nextthink <= thinktime)	//hmm... infinate loop was possible here.. Quite a few non-QW mods do this.
			return true;
	} while (1);

	return true;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (edict_t *e1, edict_t *e2)
{
	int		old_self, old_other;
	
	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;
	
	pr_global_struct->time = sv.time;
	if (e1->v.touch && e1->v.solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, e1);
		pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, e2);
		PR_ExecuteProgram (svprogfuncs, e1->v.touch);
	}
	
	if (e2->v.touch && e2->v.solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, e2);
		pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, e1);
		PR_ExecuteProgram (svprogfuncs, e2->v.touch);
	}

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
}


/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define	STOP_EPSILON	0.1
//courtesy of darkplaces, it's just more efficient.
void ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	int i;
	float backoff;

	backoff = -DotProduct (in, normal) * overbounce;
	VectorMA(in, backoff, normal, out);

	for (i = 0;i < 3;i++)
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
}



/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
============
*/
#define	MAX_CLIP_PLANES	5
int SV_FlyMove (edict_t *ent, float time, trace_t *steptrace)
{
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	int			i, j;
	trace_t		trace;
	vec3_t		end;
	float		time_left;
	int			blocked;
	
	numbumps = 4;
	
	blocked = 0;
	VectorCopy (ent->v.velocity, original_velocity);
	VectorCopy (ent->v.velocity, primal_velocity);
	numplanes = 0;
	
	time_left = time;

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		for (i=0 ; i<3 ; i++)
			end[i] = ent->v.origin[i] + time_left * ent->v.velocity[i];

		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

		if (trace.allsolid)
		{	// entity is trapped in another solid
			VectorCopy (vec3_origin, ent->v.velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{	// actually covered some distance
			VectorCopy (trace.endpos, ent->v.origin);
			VectorCopy (ent->v.velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		if (!trace.ent)
			SV_Error ("SV_FlyMove: !trace.ent");

		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1;		// floor
			if (trace.ent->v.solid == SOLID_BSP)
			{
				ent->v.flags =	(int)ent->v.flags | FL_ONGROUND;
				ent->v.groundentity = EDICT_TO_PROG(svprogfuncs, trace.ent);
			}
		}
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step
			if (steptrace)
				*steptrace = trace;	// save for player extrafriction
		}

//
// run the impact function
//
		SV_Impact (ent, trace.ent);
		if (ent->isfree)
			break;		// removed by the impact function

		
		time_left -= time_left * trace.fraction;
		
	// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			VectorCopy (vec3_origin, ent->v.velocity);
			return 3;
		}

		if (0)
		{
			ClipVelocity(ent->v.velocity, trace.plane.normal, ent->v.velocity, 1);
			break;
		}
		else
		{
			VectorCopy (trace.plane.normal, planes[numplanes]);
			numplanes++;

	//
	// modify original_velocity so it parallels all of the clip planes
	//
			for (i=0 ; i<numplanes ; i++)
			{
				ClipVelocity (original_velocity, planes[i], new_velocity, 1);
				for (j=0 ; j<numplanes ; j++)
					if (j != i)
					{
						if (DotProduct (new_velocity, planes[j]) < 0)
							break;	// not ok
					}
				if (j == numplanes)
					break;
			}
			
			if (i != numplanes)
			{	// go along this plane
				VectorCopy (new_velocity, ent->v.velocity);
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
//					Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
					VectorCopy (vec3_origin, ent->v.velocity);
					return 7;
				}
				CrossProduct (planes[0], planes[1], dir);
				VectorNormalize(dir);	//fixes slow falling in corners
				d = DotProduct (dir, ent->v.velocity);
				VectorScale (dir, d, ent->v.velocity);
			}
		}

//
// if original velocity is against the original velocity, stop dead
// to avoid tiny occilations in sloping corners
//
		if (DotProduct (ent->v.velocity, primal_velocity) <= 0)
		{
			VectorCopy (vec3_origin, ent->v.velocity);
			return blocked;
		}
	}

	return blocked;
}


/*
============
SV_AddGravity

============
*/
void SV_AddGravity (edict_t *ent, float scale)
{
	ent->v.velocity[2] -= scale * movevars.gravity * host_frametime;
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity (edict_t *ent, vec3_t push)
{
	trace_t	trace;
	vec3_t	end;
		
	VectorAdd (ent->v.origin, push, end);

	if (ent->v.movetype == MOVETYPE_FLYMISSILE)
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_MISSILE, ent);
	else if (ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT)
	// only clip against bmodels
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
	else
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);
	
//	if (trace.ent)
//		VectorMA(trace.endpos, sv_impactpush.value, trace.plane.normal, ent->v.origin);
//	else
		VectorCopy (trace.endpos, ent->v.origin);
	SV_LinkEdict (ent, true);

	if (trace.ent)
		SV_Impact (ent, trace.ent);		

	return trace;
}					




typedef struct
{
	edict_t	*ent;
	vec3_t	origin;
	vec3_t	angles;
//	float	deltayaw;
} pushed_t;
pushed_t	pushed[MAX_EDICTS], *pushed_p;

/*
============
SV_Push

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
============
*/
qboolean SV_PushAngles (edict_t *pusher, vec3_t move, vec3_t amove)
{
	int			i, e;
	edict_t		*check, *block;
	vec3_t		mins, maxs;
	float oldsolid;
	pushed_t	*p;
	vec3_t		org, org2, move2, forward, right, up;

	pushed_p = pushed;

	// find the bounding box
	for (i=0 ; i<3 ; i++)
	{
		mins[i] = pusher->v.absmin[i] + move[i];
		maxs[i] = pusher->v.absmax[i] + move[i];
	}

// we need this for pushing things later
	VectorSubtract (vec3_origin, amove, org);
	AngleVectors (org, forward, right, up);

// save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy (pusher->v.origin, pushed_p->origin);
	VectorCopy (pusher->v.angles, pushed_p->angles);
//	if (pusher->client)
//		pushed_p->deltayaw = pusher->client->ps.pmove.delta_angles[YAW];
	pushed_p++;

// move the pusher to it's final position
	VectorAdd (pusher->v.origin, move, pusher->v.origin);
	VectorAdd (pusher->v.angles, amove, pusher->v.angles);
	SV_LinkEdict (pusher, false);

// see if any solid entities are inside the final position
	for (e = 1; e < sv.num_edicts; e++)
	{
		check = EDICT_NUM(svprogfuncs, e);
		if (check->isfree)
			continue;
		
		if (check->v.movetype == MOVETYPE_PUSH
		|| check->v.movetype == MOVETYPE_NONE
		|| check->v.movetype == MOVETYPE_NOCLIP)
			continue;

#if 1
		oldsolid = pusher->v.solid;
		pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition (check);
		pusher->v.solid = oldsolid;
		if (block)
			continue;
#else
		if (!check->area.prev)
			continue;		// not linked in anywhere
#endif

	// if the entity is standing on the pusher, it will definitely be moved
		if ( ! ( ((int)check->v.flags & FL_ONGROUND)
			&& PROG_TO_EDICT(svprogfuncs, check->v.groundentity) == pusher) )
		{
			// see if the ent needs to be tested
			if ( check->v.absmin[0] >= maxs[0]
			|| check->v.absmin[1] >= maxs[1]
			|| check->v.absmin[2] >= maxs[2]
			|| check->v.absmax[0] <= mins[0]
			|| check->v.absmax[1] <= mins[1]
			|| check->v.absmax[2] <= mins[2] )
				continue;
			

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}

		if ((pusher->v.movetype == MOVETYPE_PUSH) || (PROG_TO_EDICT(svprogfuncs, check->v.groundentity) == pusher))
		{
			// move this entity
			pushed_p->ent = check;
			VectorCopy (check->v.origin, pushed_p->origin);
			VectorCopy (check->v.angles, pushed_p->angles);
			pushed_p++;

			// try moving the contacted entity 
			VectorAdd (check->v.origin, move, check->v.origin);
//			if (check->client)
//			{	// FIXME: doesn't rotate monsters?
//				check->client->ps.pmove.delta_angles[YAW] += amove[YAW];
//			}
			VectorAdd (check->v.angles, amove, check->v.angles);

			// figure movement due to the pusher's amove
			VectorSubtract (check->v.origin, pusher->v.origin, org);
			org2[0] = DotProduct (org, forward);
			org2[1] = DotProduct (org, right);
			org2[2] = DotProduct (org, up);
			VectorSubtract (org2, org, move2);
			VectorAdd (check->v.origin, move2, check->v.origin);

			// may have pushed them off an edge
			if (PROG_TO_EDICT(svprogfuncs, check->v.groundentity) != pusher)
				check->v.groundentity = 0;

			block = SV_TestEntityPosition (check);
			if (!block)
			{	// pushed ok
				SV_LinkEdict (check, false);
				// impact?
				continue;
			}



			// if it is ok to leave in the old position, do it
			// this is only relevent for riding entities, not pushed
			// FIXME: this doesn't acount for rotation
			VectorSubtract (check->v.origin, move, check->v.origin);
			block = SV_TestEntityPosition (check);
			if (!block)
			{
				pushed_p--;
				continue;
			}
		}

		// if it is sitting on top. Do not block.
		if (check->v.mins[0] == check->v.maxs[0])
		{
			SV_LinkEdict (check, false);
			continue;
		}
		
//		Con_Printf("Pusher hit %s\n", PR_GetString(svprogfuncs, check->v.classname));
		if (pusher->v.blocked)
		{
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, pusher);
			pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, check);
			PR_ExecuteProgram (svprogfuncs, pusher->v.blocked);
		}

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for (p=pushed_p-1 ; p>=pushed ; p--)
		{
			VectorCopy (p->origin, p->ent->v.origin);
			VectorCopy (p->angles, p->ent->v.angles);
//			if (p->ent->client)
//			{
//				p->ent->client->ps.pmove.delta_angles[YAW] = p->deltayaw;
//			}
			SV_LinkEdict (p->ent, false);
		}
		return false;
	}

//FIXME: is there a better way to handle this?
	// see if anything we moved has touched a trigger
	for (p=pushed_p-1 ; p>=pushed ; p--)
		SV_TouchLinks ( p->ent, sv_areanodes );

	return true;
}

/*
============
SV_Push

============
*/
qboolean SV_Push (edict_t *pusher, vec3_t move, vec3_t amove)
{
	int			i, e;
	edict_t		*check, *block;
	vec3_t		mins, maxs;
	vec3_t		pushorig;
	int			num_moved;
	edict_t		*moved_edict[MAX_EDICTS];
	vec3_t		moved_from[MAX_EDICTS];
	float oldsolid;

	if (amove[0] || amove[1] || amove[2])
	{
		return SV_PushAngles(pusher, move, amove);
	}

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = pusher->v.absmin[i] + move[i];
		maxs[i] = pusher->v.absmax[i] + move[i];
	}

	VectorCopy (pusher->v.origin, pushorig);
	
// move the pusher to it's final position

	VectorAdd (pusher->v.origin, move, pusher->v.origin);
	SV_LinkEdict (pusher, false);

// see if any solid entities are inside the final position
	num_moved = 0;
	for (e=1 ; e<sv.num_edicts ; e++)
	{
		check = EDICT_NUM(svprogfuncs, e);
		if (check->isfree)
			continue;
		if (check->v.movetype == MOVETYPE_PUSH
		|| check->v.movetype == MOVETYPE_NONE
		|| check->v.movetype == MOVETYPE_FOLLOW
		|| check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		oldsolid = pusher->v.solid;
		pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition (check);
		pusher->v.solid = oldsolid;
		if (block)
			continue;

	// if the entity is standing on the pusher, it will definately be moved
		if ( ! ( ((int)check->v.flags & FL_ONGROUND)
		&& 
			PROG_TO_EDICT(svprogfuncs, check->v.groundentity) == pusher) )
		{
			if ( check->v.absmin[0] >= maxs[0]
			|| check->v.absmin[1] >= maxs[1]
			|| check->v.absmin[2] >= maxs[2]
			|| check->v.absmax[0] <= mins[0]
			|| check->v.absmax[1] <= mins[1]
			|| check->v.absmax[2] <= mins[2] )
				continue;

		// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}

		VectorCopy (check->v.origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity 
		VectorAdd (check->v.origin, move, check->v.origin);
		block = SV_TestEntityPosition (check);
		if (!block)
		{	// pushed ok
			SV_LinkEdict (check, false);
			continue;
		}

		// if it is ok to leave in the old position, do it
		VectorSubtract (check->v.origin, move, check->v.origin);
		block = SV_TestEntityPosition (check);
		if (!block)
		{
			num_moved--;
			continue;
		}

	// if it is still inside the pusher, block
		if (check->v.mins[0] == check->v.maxs[0])
		{
			SV_LinkEdict (check, false);
			continue;
		}
		if (check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER)
		{	// corpse
			check->v.mins[0] = check->v.mins[1] = 0;
			VectorCopy (check->v.mins, check->v.maxs);
			SV_LinkEdict (check, false);
			continue;
		}
		
		VectorCopy (pushorig, pusher->v.origin);
		SV_LinkEdict (pusher, false);

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (pusher->v.blocked)
		{
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, pusher);
			pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, check);
			PR_ExecuteProgram (svprogfuncs, pusher->v.blocked);
		}
		
	// move back any entities we already moved
		for (i=0 ; i<num_moved ; i++)
		{
			VectorCopy (moved_from[i], moved_edict[i]->v.origin);
			SV_LinkEdict (moved_edict[i], false);
		}
		return false;
	}

	return true;
}


/*
============
SV_PushMove

============
*/
void SV_PushMove (edict_t *pusher, float movetime)
{
	int			i;
	vec3_t		move;
	vec3_t		amove;

	if (!pusher->v.velocity[0] && !pusher->v.velocity[1] && !pusher->v.velocity[2]
		&& !pusher->v.avelocity[0] && !pusher->v.avelocity[1] && !pusher->v.avelocity[2])
	{
		pusher->v.ltime += movetime;
		return;
	}

	for (i=0 ; i<3 ; i++)
	{
		move[i] = pusher->v.velocity[i] * movetime;
		amove[i] = pusher->v.avelocity[i] * movetime;
	}

	if (SV_Push (pusher, move, amove))
		pusher->v.ltime += movetime;
}


/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher (edict_t *ent)
{
	float	thinktime;
	float	oldltime;
	float	movetime;
vec3_t oldorg, move;
vec3_t oldang, amove;
float	l;

	oldltime = ent->v.ltime;
	
	thinktime = ent->v.nextthink;
	if (thinktime < ent->v.ltime + host_frametime)
	{
		movetime = thinktime - ent->v.ltime;
		if (movetime < 0)
			movetime = 0;
	}
	else
		movetime = host_frametime;

	if (movetime)
	{
		SV_PushMove (ent, movetime);	// advances ent->v.ltime if not blocked
	}
		
	if (thinktime > oldltime && thinktime <= ent->v.ltime)
	{
VectorCopy (ent->v.origin, oldorg);
VectorCopy (ent->v.angles, oldang);
		ent->v.nextthink = 0;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
		pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, sv.edicts);
		PR_ExecuteProgram (svprogfuncs, ent->v.think);
		if (ent->isfree)
			return;
VectorSubtract (ent->v.origin, oldorg, move);
VectorSubtract (ent->v.angles, oldang, amove);

l = Length(move)+Length(amove);
if (l > 1.0/64)
{
//	Con_Printf ("**** snap: %f\n", Length (l));
	VectorCopy (oldorg, ent->v.origin);
	SV_Push (ent, move, amove);
}

	}

}


/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void SV_Physics_None (edict_t *ent)
{
// regular thinking
	SV_RunThink (ent);
}


/*
=============
SV_Physics_Follow

Entities that are "stuck" to another entity
=============
*/
void SV_Physics_Follow (edict_t *ent)
{
	vec3_t vf, vr, vu, angles, v;
	edict_t *e;

	// regular thinking
	if (!SV_RunThink (ent))
		return;

	// LordHavoc: implemented rotation on MOVETYPE_FOLLOW objects
	e = PROG_TO_EDICT(svprogfuncs, ent->v.aiment);
	if (e->v.angles[0] == ent->v.punchangle[0] && e->v.angles[1] == ent->v.punchangle[1] && e->v.angles[2] == ent->v.punchangle[2])
	{
		// quick case for no rotation
		VectorAdd(e->v.origin, ent->v.view_ofs, ent->v.origin);
	}
	else
	{
		angles[0] = -ent->v.punchangle[0];
		angles[1] =  ent->v.punchangle[1];
		angles[2] =  ent->v.punchangle[2];
		AngleVectors (angles, vf, vr, vu);
		v[0] = ent->v.view_ofs[0] * vf[0] + ent->v.view_ofs[1] * vr[0] + ent->v.view_ofs[2] * vu[0];
		v[1] = ent->v.view_ofs[0] * vf[1] + ent->v.view_ofs[1] * vr[1] + ent->v.view_ofs[2] * vu[1];
		v[2] = ent->v.view_ofs[0] * vf[2] + ent->v.view_ofs[1] * vr[2] + ent->v.view_ofs[2] * vu[2];
		angles[0] = -e->v.angles[0];
		angles[1] =  e->v.angles[1];
		angles[2] =  e->v.angles[2];
		AngleVectors (angles, vf, vr, vu);
		ent->v.origin[0] = v[0] * vf[0] + v[1] * vf[1] + v[2] * vf[2] + e->v.origin[0];
		ent->v.origin[1] = v[0] * vr[0] + v[1] * vr[1] + v[2] * vr[2] + e->v.origin[1];
		ent->v.origin[2] = v[0] * vu[0] + v[1] * vu[1] + v[2] * vu[2] + e->v.origin[2];
	}
	VectorAdd (e->v.angles, ent->v.v_angle, ent->v.angles);
	SV_LinkEdict (ent, true);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip (edict_t *ent)
{
// regular thinking
	if (!SV_RunThink (ent))
		return;
	
	VectorMA (ent->v.angles, host_frametime, ent->v.avelocity, ent->v.angles);
	VectorMA (ent->v.origin, host_frametime, ent->v.velocity, ent->v.origin);

	SV_LinkEdict (ent, false);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition (edict_t *ent)
{
	int		cont;

	cont = SV_PointContents (ent->v.origin);

	//needs to be q1 progs compatable
	if (cont & FTECONTENTS_LAVA)
		cont = Q1CONTENTS_LAVA;
	else if (cont & FTECONTENTS_SLIME)
		cont = Q1CONTENTS_SLIME;
	else if (cont & FTECONTENTS_WATER)
		cont = Q1CONTENTS_WATER;
	else
		cont = Q1CONTENTS_EMPTY;

	if (!ent->v.watertype)
	{	// just spawned here
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		return;
	}
	
	if (cont <= Q1CONTENTS_WATER)
	{
		if (ent->v.watertype == Q1CONTENTS_EMPTY)
		{	// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}		
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
	}
	else
	{
		if (ent->v.watertype != Q1CONTENTS_EMPTY)
		{	// just crossed into open
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}		
		ent->v.watertype = Q1CONTENTS_EMPTY;
		ent->v.waterlevel = cont;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss (edict_t *ent)
{
	trace_t	trace;
	vec3_t	move;
	float	backoff;

	SV_CheckVelocity (ent);

// regular thinking
	if (!SV_RunThink (ent))
		return;

	if (ent->v.velocity[2] > 0)
		ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;

// if onground, return without moving
	if ( ((int)ent->v.flags & FL_ONGROUND) )
		return;

// add gravity
	if (ent->v.movetype != MOVETYPE_FLY
		&& ent->v.movetype != MOVETYPE_FLYMISSILE
		&& ent->v.movetype != MOVETYPE_BOUNCEMISSILE
		&& ent->v.movetype != MOVETYPE_SWIM)
		SV_AddGravity (ent, 1.0);

// move angles
	VectorMA (ent->v.angles, host_frametime, ent->v.avelocity, ent->v.angles);

// move origin
	VectorScale (ent->v.velocity, host_frametime, move);
	trace = SV_PushEntity (ent, move);
	if (trace.fraction == 1)
		return;
	if (ent->isfree)
		return;

	VectorCopy(trace.endpos, move);
	
	if (ent->v.movetype == MOVETYPE_BOUNCE)
		backoff = 1.5;
	else if (ent->v.movetype == MOVETYPE_BOUNCEMISSILE)
		backoff = 2;
	else
		backoff = 1;

	ClipVelocity (ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff);


// stop if on ground
	if ((trace.plane.normal[2] > 0.7) && (ent->v.movetype != MOVETYPE_BOUNCEMISSILE))
	{		
		if (ent->v.velocity[2] < 60 || ent->v.movetype != MOVETYPE_BOUNCE )
		{
			ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(svprogfuncs, trace.ent);
			VectorCopy (vec3_origin, ent->v.velocity);
			VectorCopy (vec3_origin, ent->v.avelocity);
		}
	}
	
// check for in water
	SV_CheckWaterTransition (ent);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
FIXME: is this true?
=============
*/
void SV_Physics_Step (edict_t *ent)
{
	qboolean	hitsound;

// frefall if not onground
	if ( ! ((int)ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM) ) )
	{
		if (ent->v.velocity[2] < movevars.gravity*-0.1)
			hitsound = true;
		else
			hitsound = false;

		SV_AddGravity (ent, 1.0);
		SV_CheckVelocity (ent);
		SV_FlyMove (ent, host_frametime, NULL);
		SV_LinkEdict (ent, true);

		if ( (int)ent->v.flags & FL_ONGROUND )	// just hit ground
		{
			if (hitsound)
			{
				if (progstype == PROG_H2)
					SV_StartSound (ent, 0, "fx/thngland.wav", 255, 1);
				else
					SV_StartSound (ent, 0, "demon/dland2.wav", 255, 1);
			}
		}
	}

// regular thinking
	SV_RunThink (ent);
	
	SV_CheckWaterTransition (ent);
}

//============================================================================

void SV_ProgStartFrame (void)
{

// let the progs know that a new frame has started
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.edicts);
	pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, sv.edicts);
	pr_global_struct->time = sv.time;
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->StartFrame);
}













/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void SV_CheckStuck (edict_t *ent)
{
	int		i, j;
	int		z;
	vec3_t	org;
//return;
	if (!SV_TestEntityPosition(ent))
	{
		VectorCopy (ent->v.origin, ent->v.oldorigin);
		return;
	}

	VectorCopy (ent->v.origin, org);
	VectorCopy (ent->v.oldorigin, ent->v.origin);
	if (!SV_TestEntityPosition(ent))
	{
		Con_DPrintf ("Unstuck.\n");
		SV_LinkEdict (ent, true);
		return;
	}
	
	for (z=0 ; z < pm_stepheight ; z++)
		for (i=-1 ; i <= 1 ; i++)
			for (j=-1 ; j <= 1 ; j++)
			{
				ent->v.origin[0] = org[0] + i;
				ent->v.origin[1] = org[1] + j;
				ent->v.origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					Con_DPrintf ("Unstuck.\n");
					SV_LinkEdict (ent, true);
					return;
				}
			}
			
	VectorCopy (org, ent->v.origin);
	Con_DPrintf ("player is stuck.\n");
}

/*
=============
SV_CheckWater
=============
*/
qboolean SV_CheckWater (edict_t *ent)
{
	vec3_t	point;
	int		cont;

	point[0] = ent->v.origin[0];
	point[1] = ent->v.origin[1];
	point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;	

	ent->v.waterlevel = 0;
	ent->v.watertype = Q1CONTENTS_EMPTY;
	cont = SV_PointContents (point);
	if (cont & FTECONTENTS_FLUID)
	{
		if (cont & FTECONTENTS_LAVA)
			ent->v.watertype = Q1CONTENTS_LAVA;
		else if (cont & FTECONTENTS_SLIME)
			ent->v.watertype = Q1CONTENTS_SLIME;
		else if (cont & FTECONTENTS_WATER)
			ent->v.watertype = Q1CONTENTS_WATER;
		else 
			ent->v.watertype = Q1CONTENTS_SKY;
		ent->v.waterlevel = 1;
		point[2] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2])*0.5;
		cont = SV_PointContents (point);
		if (cont & FTECONTENTS_FLUID)
		{
			ent->v.waterlevel = 2;
			point[2] = ent->v.origin[2] + ent->v.view_ofs[2];
			cont = SV_PointContents (point);
			if (cont & FTECONTENTS_FLUID)
				ent->v.waterlevel = 3;
		}
	}
	
	return ent->v.waterlevel > 1;
}


/*
============
SV_WallFriction

============
*/
void SV_WallFriction (edict_t *ent, trace_t *trace)
{
	vec3_t		forward, right, up;
	float		d, i;
	vec3_t		into, side;
	
	AngleVectors (ent->v.v_angle, forward, right, up);
	d = DotProduct (trace->plane.normal, forward);
	
	d += 0.5;
	if (d >= 0 || IS_NAN(d))
		return;
		
// cut the tangential velocity
	i = DotProduct (trace->plane.normal, ent->v.velocity);
	VectorScale (trace->plane.normal, i, into);
	VectorSubtract (ent->v.velocity, into, side);
	
	ent->v.velocity[0] = side[0] * (1 + d);
	ent->v.velocity[1] = side[1] * (1 + d);
}

/*
=====================
SV_TryUnstick

Player has come to a dead stop, possibly due to the problem with limited
float precision at some angle joins in the BSP hull.

Try fixing by pushing one pixel in each direction.

This is a hack, but in the interest of good gameplay...
======================
*/
int SV_TryUnstick (edict_t *ent, vec3_t oldvel)
{
	int		i;
	vec3_t	oldorg;
	vec3_t	dir;
	int		clip;
	trace_t	steptrace;
	
	VectorCopy (ent->v.origin, oldorg);
	VectorCopy (vec3_origin, dir);

	for (i=0 ; i<8 ; i++)
	{
// try pushing a little in an axial direction
		switch (i)
		{
			case 0:	dir[0] = 2; dir[1] = 0; break;
			case 1:	dir[0] = 0; dir[1] = 2; break;
			case 2:	dir[0] = -2; dir[1] = 0; break;
			case 3:	dir[0] = 0; dir[1] = -2; break;
			case 4:	dir[0] = 2; dir[1] = 2; break;
			case 5:	dir[0] = -2; dir[1] = 2; break;
			case 6:	dir[0] = 2; dir[1] = -2; break;
			case 7:	dir[0] = -2; dir[1] = -2; break;
		}
		
		SV_PushEntity (ent, dir);

// retry the original move
		ent->v.velocity[0] = oldvel[0];
		ent->v. velocity[1] = oldvel[1];
		ent->v. velocity[2] = 0;
		clip = SV_FlyMove (ent, 0.1, &steptrace);

		if ( fabs(oldorg[1] - ent->v.origin[1]) > 4
		|| fabs(oldorg[0] - ent->v.origin[0]) > 4 )
		{
//Con_DPrintf ("unstuck!\n");
			return clip;
		}
			
// go back to the original pos and try again
		VectorCopy (oldorg, ent->v.origin);
	}
	
	VectorCopy (vec3_origin, ent->v.velocity);
	return 7;		// still not moving
}

/*
=====================
SV_WalkMove

Only used by players
======================
*/
#define	SMSTEPSIZE	4
void SV_WalkMove (edict_t *ent)
{
	vec3_t		upmove, downmove;
	vec3_t		oldorg, oldvel;
	vec3_t		nosteporg, nostepvel;
	int			clip;
	int			oldonground;
	trace_t		steptrace, downtrace;
	
//
// do a regular slide move unless it looks like you ran into a step
//
	oldonground = (int)ent->v.flags & FL_ONGROUND;
	ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;
	
	VectorCopy (ent->v.origin, oldorg);
	VectorCopy (ent->v.velocity, oldvel);
	
	clip = SV_FlyMove (ent, host_frametime, &steptrace);

	if ( !(clip & 2) )
		return;		// move didn't block on a step

	if (!oldonground && ent->v.waterlevel == 0)
		return;		// don't stair up while jumping
	
	if (ent->v.movetype != MOVETYPE_WALK)
		return;		// gibbed by a trigger
	
//	if (sv_nostep.value)
//		return;
	
	if ( (int)ent->v.flags & FL_WATERJUMP )
		return;

	VectorCopy (ent->v.origin, nosteporg);
	VectorCopy (ent->v.velocity, nostepvel);

//
// try moving up and forward to go up a step
//
	VectorCopy (oldorg, ent->v.origin);	// back to start pos

	VectorCopy (vec3_origin, upmove);
	VectorCopy (vec3_origin, downmove);
	upmove[2] = pm_stepheight;
	downmove[2] = -pm_stepheight + oldvel[2]*host_frametime;

// move up
	SV_PushEntity (ent, upmove);	// FIXME: don't link?

// move forward
	ent->v.velocity[0] = oldvel[0];
	ent->v.velocity[1] = oldvel[1];
	ent->v.velocity[2] = 0;
	clip = SV_FlyMove (ent, host_frametime, &steptrace);

// check for stuckness, possibly due to the limited precision of floats
// in the clipping hulls
	if (clip)
	{
		if ( fabs(oldorg[1] - ent->v.origin[1]) < 0.03125
		&& fabs(oldorg[0] - ent->v.origin[0]) < 0.03125 )
		{	// stepping up didn't make any progress
			clip = SV_TryUnstick (ent, oldvel);

//			Con_Printf("Try unstick fwd\n");
		}
	}
	
// extra friction based on view angle
	if ( clip & 2 )
	{
		vec3_t lastpos, lastvel, lastdown;

//		Con_Printf("couldn't do it\n");

		//retry with a smaller step (allows entering smaller areas with a step of 4)
		VectorCopy (downmove, lastdown);
		VectorCopy (ent->v.origin, lastpos);
		VectorCopy (ent->v.velocity, lastvel);

	//
	// try moving up and forward to go up a step
	//
		VectorCopy (oldorg, ent->v.origin);	// back to start pos

		VectorCopy (vec3_origin, upmove);
		VectorCopy (vec3_origin, downmove);
		upmove[2] = SMSTEPSIZE;
		downmove[2] = -SMSTEPSIZE + oldvel[2]*host_frametime;

	// move up
		SV_PushEntity (ent, upmove);	// FIXME: don't link?

	// move forward
		ent->v.velocity[0] = oldvel[0];
		ent->v.velocity[1] = oldvel[1];
		ent->v.velocity[2] = 0;
		clip = SV_FlyMove (ent, host_frametime, &steptrace);

	// check for stuckness, possibly due to the limited precision of floats
	// in the clipping hulls
		if (clip)
		{
			if ( fabs(oldorg[1] - ent->v.origin[1]) < 0.03125
			&& fabs(oldorg[0] - ent->v.origin[0]) < 0.03125 )
			{	// stepping up didn't make any progress
				clip = SV_TryUnstick (ent, oldvel);

//				Con_Printf("Try unstick up\n");
			}
		}

		if ( fabs(oldorg[1] - ent->v.origin[1])+fabs(oldorg[0] - ent->v.origin[0]) < fabs(oldorg[1] - lastpos[1])+fabs(oldorg[1] - lastpos[1]))
		{	// stepping up didn't make any progress
				//go back
				VectorCopy (lastdown, downmove);
				VectorCopy (lastpos, ent->v.origin);
				VectorCopy (lastvel, ent->v.velocity);

				SV_WallFriction (ent, &steptrace);

//				Con_Printf("wall friction\n");
			}

		else if (clip & 2)
		{
			SV_WallFriction (ent, &steptrace);
//			Con_Printf("wall friction 2\n");
		}
	}

// move down
	downtrace = SV_PushEntity (ent, downmove);	// FIXME: don't link?

	if (downtrace.plane.normal[2] > 0.7)
	{
		if (ent->v.solid == SOLID_BSP)
		{
			ent->v.flags =	(int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(svprogfuncs, downtrace.ent);
		}
	}
	else
	{
// if the push down didn't end up on good ground, use the move without
// the step up.  This happens near wall / slope combinations, and can
// cause the player to hop up higher on a slope too steep to climb	
		VectorCopy (nosteporg, ent->v.origin);
		VectorCopy (nostepvel, ent->v.velocity);

//		Con_Printf("down not good\n");
	}
}



/*
================
SV_Physics_Client

Player character actions


From normal Quake in an attempt to fix physics in QuakeRally
================
*/
#define FL_JUMPRELEASED 4096
void SV_Physics_Client (edict_t	*ent, int num)
{
	qboolean readyforjump;
//	float oldvel;

	if ( svs.clients[num-1].state < cs_spawned )
		return;		// unconnected slot

	readyforjump = false;
	if (progstype == PROG_QW)
		if (ent->v.button2)
			if ((int)ent->v.flags & FL_JUMPRELEASED)
				readyforjump = true;
//
// call standard client pre-think
//	
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPreThink);
	
	if (readyforjump)	//qw progs can't jump for themselves...
	{
		if (!ent->v.button2 && !((int)ent->v.flags & FL_JUMPRELEASED))
			ent->v.velocity[2] += 270;
	}
//
// do a move
//
	SV_CheckVelocity (ent);	

//
// decide which move function to call
//
	switch ((int)ent->v.movetype)
	{
	case MOVETYPE_NONE:
		if (!SV_RunThink (ent))
			return;
		break;

	case MOVETYPE_WALK:
//		oldvel = ent->v.velocity[0];
		if (!SV_RunThink (ent))
			return;
		if (!SV_CheckWater (ent) && ! ((int)ent->v.flags & FL_WATERJUMP) )
			SV_AddGravity (ent, ent->v.gravity);

//		if (fabs(oldvel - ent->v.velocity[0])> 100)
//			Con_Printf("grav: %f -> %f\n", oldvel, ent->v.velocity[0]);

		SV_CheckStuck (ent);

//		if (fabs(oldvel - ent->v.velocity[0])> 100)
//			Con_Printf("stuck: %f -> %f\n", oldvel, ent->v.velocity[0]);

		SV_WalkMove (ent);

//		if (fabs(oldvel - ent->v.velocity[0])> 100)
//			Con_Printf("walk: %f -> %f\n", oldvel, ent->v.velocity[0]);

		break;

	case MOVETYPE_FOLLOW:
		SV_Physics_Follow (ent);
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		SV_Physics_Toss (ent);
		break;

	case MOVETYPE_FLY:
	case MOVETYPE_SWIM:
		if (!SV_RunThink (ent))
			return;
		SV_FlyMove (ent, host_frametime, NULL);
		break;
		
	case MOVETYPE_NOCLIP:
		if (!SV_RunThink (ent))
			return;
		VectorMA (ent->v.origin, host_frametime, ent->v.velocity, ent->v.origin);
		VectorMA (ent->v.angles, host_frametime, ent->v.avelocity, ent->v.angles);
		break;
		
	default:
		Sys_Error ("SV_Physics_client: bad movetype %i", (int)ent->v.movetype);
	}

//
// call standard player post-think
//		
	SV_LinkEdict (ent, true);

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPostThink);
}


/*
================
SV_RunEntity

================
*/
void SV_RunEntity (edict_t *ent)
{
	int		c,originMoved;
	edict_t	*ent2;
	vec3_t	oldOrigin,oldAngle;

	if (ent->v.lastruntime == (float)realtime)
		return;
	ent->v.lastruntime = (float)realtime;

	ent2 = PROG_TO_EDICT(svprogfuncs, ent->v.movechain);
	if (ent2 != sv.edicts)
	{
		VectorCopy(ent->v.origin,oldOrigin);
		VectorCopy(ent->v.angles,oldAngle);
	}

	switch ( (int)ent->v.movetype)
	{
	case MOVETYPE_PUSH:
		SV_Physics_Pusher (ent);
		break;
	case MOVETYPE_NONE:
		SV_Physics_None (ent);
		break;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip (ent);
		break;
	case MOVETYPE_STEP:
	case MOVETYPE_PUSHPULL:
		SV_Physics_Step (ent);
		break;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow (ent);
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_BOUNCEMISSILE:
	case MOVETYPE_FLY:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_SWIM:
		SV_Physics_Toss (ent);
		break;
	case MOVETYPE_WALK:
		if (!SV_RunThink (ent))
			return;
		if (!SV_CheckWater (ent) && ! ((int)ent->v.flags & FL_WATERJUMP) )
			SV_AddGravity (ent, ent->v.gravity);
		SV_CheckStuck (ent);

		SV_WalkMove (ent);

		break;
	default:
		SV_Error ("SV_Physics: bad movetype %i on %s", (int)ent->v.movetype, svprogfuncs->stringtable + ent->v.classname);
	}

	if (ent2 != sv.edicts)
	{
		originMoved = !VectorCompare(ent->v.origin,oldOrigin);
		if (originMoved || !VectorCompare(ent->v.angles,oldAngle))
		{
			VectorSubtract(ent->v.origin,oldOrigin,oldOrigin);
			VectorSubtract(ent->v.angles,oldAngle,oldAngle);

			for(c=0;c<10;c++)
			{   // chain a max of 10 objects
				if (ent2->isfree) break;

				VectorAdd(oldOrigin,ent2->v.origin,ent2->v.origin);
				if ((int)ent2->v.flags & FL_MOVECHAIN_ANGLE)
				{
					VectorAdd(oldAngle,ent2->v.angles,ent2->v.angles);
				}

				if (originMoved && ent2->v.chainmoved)
				{	// callback function
					pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent2);
					pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, ent);
					PR_ExecuteProgram(svprogfuncs, ent2->v.chainmoved);
				}

				ent2 = PROG_TO_EDICT(svprogfuncs, ent2->v.movechain);
				if (ent2 == sv.edicts) break;

			}
		}
	}
}

/*
================
SV_RunNewmis

================
*/
void SV_RunNewmis (void)
{
	edict_t	*ent;

	if (!realpr_nqglobal_struct.newmis)	//newmis variable is not exported.
		return;

	if (sv_nomsec.value >= 2)
		return;

	if (!pr_global_struct->newmis)
		return;
	ent = PROG_TO_EDICT(svprogfuncs, pr_global_struct->newmis);
	host_frametime = 0.05;
	pr_global_struct->newmis = 0;
	
	SV_RunEntity (ent);	
	
	host_frametime = pr_global_struct->frametime;
}

trace_t SV_Trace_Toss (edict_t *tossent, edict_t *ignore)
{
	int i;
	float gravity;
	vec3_t move, end;
	trace_t trace;

	vec3_t origin, velocity;

	// this has to fetch the field from the original edict, since our copy is truncated
	gravity = tossent->v.gravity;
	if (!gravity)
		gravity = 1.0;
	gravity *= sv_gravity.value * 0.05;

	VectorCopy (tossent->v.origin, origin);
	VectorCopy (tossent->v.velocity, velocity);

	SV_CheckVelocity (tossent);

	for (i = 0;i < 200;i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		velocity[2] -= gravity;
		VectorScale (velocity, 0.05, move);
		VectorAdd (origin, move, end);
		trace = SV_Move (origin, tossent->v.mins, tossent->v.maxs, end, MOVE_NORMAL, tossent);
		VectorCopy (trace.endpos, origin);

		if (trace.fraction < 1 && trace.ent && trace.ent != ignore)
			break;

		if (Length(velocity) > sv_maxvelocity.value)
		{
//			Con_DPrintf("Slowing %s\n", PR_GetString(svprogfuncs, tossent->v.classname));
			VectorScale (velocity, sv_maxvelocity.value/Length(velocity), velocity);
		}
	}

	trace.fraction = 0; // not relevant
	return trace;
}

/*
================
SV_Physics

================
*/
qboolean SV_Physics (void)
{
	int		i;
	edict_t	*ent;
	static double	old_time;

#ifdef Q2SERVER
	if (!svprogfuncs)	//make tics multiples of sv_maxtic (defaults to 0.1)
	{
		host_frametime = realtime - old_time;
		if (host_frametime < sv_maxtic.value && realtime)
		{
			sv.time+=host_frametime;
			return true;	//don't bother with the whole server thing for a bit longer
		}
		if (host_frametime > sv_maxtic.value)
			host_frametime = sv_maxtic.value;
		old_time = realtime;


		sv.framenum++;
		sv.time = sv.framenum*100;
		ge->RunFrame();
		return false;
	}
#endif

// don't bother running a frame if sys_ticrate seconds haven't passed
	host_frametime = realtime - old_time;
	if (host_frametime < sv_mintic.value)
		return false;
	if (host_frametime > sv_maxtic.value)
		host_frametime = sv_maxtic.value;
	old_time = realtime;

	pr_global_struct->frametime = host_frametime;

	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		host_client = &svs.clients[i];
		if (host_client->state == cs_spawned)
		if (sv_nomsec.value || SV_PlayerPhysicsQC
#ifndef NQPROT
				)
#else
				|| (host_client->nqprot))
#endif
			SV_ClientThink();
	}

	SV_ProgStartFrame ();

	PR_RunThreads();

//
// treat each object in turn
// even the world gets a chance to think
//
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);
		if (ent->isfree)
			continue;

		if (ent->solidtype != ent->v.solid)
		{
//			Con_Printf("Entity \"%s\" improperly changed solid type\n", svprogfuncs->stringtable+ent->v.classname);
			SV_LinkEdict (ent, true);	// a change of solidity should always relink the edict. someone messed up.
		}
		else if (pr_global_struct->force_retouch)
			SV_LinkEdict (ent, true);	// force retouch even for stationary

		if (i > 0 && i <= sv.allocated_client_slots)
		{
			if (sv_nomsec.value || SV_PlayerPhysicsQC
#ifndef NQPROT
				)
#else
				|| (svs.clients[i-1].nqprot))
#endif
			{
				SV_Physics_Client(ent, i);
				SV_RunNewmis ();
			}
			else
				SV_LinkEdict(ent, true);
			continue;		// clients are run directly from packets
		}

		SV_RunEntity (ent);
		SV_RunNewmis ();
	}
	
	if (pr_global_struct->force_retouch)
		pr_global_struct->force_retouch-=1;	

	if (EndFrameQC)
	{
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.edicts);
		pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, sv.edicts);
		pr_global_struct->time = sv.time;
		PR_ExecuteProgram (svprogfuncs, EndFrameQC);
	}

	NPP_Flush();	//flush it just in case there was an error and we stopped preparsing. This is only really needed while debugging.
	return false;
}

void SV_SetMoveVars(void)
{
	movevars.gravity			= sv_gravity.value; 
	movevars.stopspeed		    = sv_stopspeed.value;		 
	movevars.maxspeed			= sv_maxspeed.value;			 
	movevars.spectatormaxspeed  = sv_spectatormaxspeed.value; 
	movevars.accelerate		    = sv_accelerate.value;		 
	movevars.airaccelerate	    = sv_airaccelerate.value;	 
	movevars.wateraccelerate	= sv_wateraccelerate.value;	   
	movevars.friction			= sv_friction.value;			 
	movevars.waterfriction	    = sv_waterfriction.value;	 
	movevars.entgravity			= 1.0;
}
