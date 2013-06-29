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
#include "winquake.h"

cvar_t	cl_nopred = SCVAR("cl_nopred","0");
extern cvar_t cl_lerp_players;
cvar_t	cl_pushlatency = SCVAR("pushlatency","-999");

extern float	pm_airaccelerate;

extern usercmd_t independantphysics[MAX_SPLITS];

#ifdef Q2CLIENT
#define	MAX_PARSE_ENTITIES	1024
extern entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

char *Get_Q2ConfigString(int i);

#ifdef Q2BSPS
void VARGS Q2_Pmove (q2pmove_t *pmove);
#define	Q2PMF_DUCKED			1
#define	Q2PMF_JUMP_HELD		2
#define	Q2PMF_ON_GROUND		4
#define	Q2PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define	Q2PMF_TIME_LAND		16	// pm_time is time before rejump
#define	Q2PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define Q2PMF_NO_PREDICTION	64	// temporarily disables prediction (used for grappling hook)
#endif

vec3_t cl_predicted_origins[UPDATE_BACKUP];


/*
===================
CL_CheckPredictionError
===================
*/
#ifdef Q2BSPS
void CLQ2_CheckPredictionError (void)
{
	int		frame;
	int		delta[3];
	int		i;
	int		len;

	if (cl_nopred.value || (cl.q2frame.playerstate.pmove.pm_flags & Q2PMF_NO_PREDICTION))
		return;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= (UPDATE_MASK);

	// compare what the server returned with what we had predicted it to be
	VectorSubtract (cl.q2frame.playerstate.pmove.origin, cl_predicted_origins[frame], delta);

	// save the prediction error for interpolation
	len = abs(delta[0]) + abs(delta[1]) + abs(delta[2]);
	if (len > 640)	// 80 world units
	{	// a teleport or something
		VectorClear (cl.prediction_error);
	}
	else
	{
//		if (/*cl_showmiss->value && */(delta[0] || delta[1] || delta[2]) )
//			Con_Printf ("prediction miss on %i: %i\n", cl.q2frame.serverframe,
//			delta[0] + delta[1] + delta[2]);

		VectorCopy (cl.q2frame.playerstate.pmove.origin, cl_predicted_origins[frame]);

		// save for error itnerpolation
		for (i=0 ; i<3 ; i++)
			cl.prediction_error[i] = delta[i]*0.125;
	}
}


/*
====================
CL_ClipMoveToEntities

====================
*/
void CLQ2_ClipMoveToEntities ( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr )
{
	int			i, x, zd, zu;
	trace_t		trace;
	float		*angles;
	entity_state_t	*ent;
	int			num;
	model_t		*cmodel;
	vec3_t		bmins, bmaxs;

	for (i=0 ; i<cl.q2frame.num_entities ; i++)
	{
		num = (cl.q2frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];

		if (!ent->solid)
			continue;

		if (ent->number == cl.playerview[0].playernum+1)
			continue;

		if (ent->solid == ES_SOLID_BSP)
		{	// special value for bmodel
			cmodel = cl.model_precache[ent->modelindex];
			if (!cmodel)
				continue;
			angles = ent->angles;
		}
		else
		{	// encoded bbox
			x = 8*(ent->solid & 31);
			zd = 8*((ent->solid>>5) & 31);
			zu = 8*((ent->solid>>10) & 63) - 32;

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			cmodel = CM_TempBoxModel (bmins, bmaxs);
			angles = vec3_origin;	// boxes don't rotate
		}

		if (tr->allsolid)
			return;

		trace = CM_TransformedBoxTrace (cmodel, start, end,
			mins, maxs, MASK_PLAYERSOLID,
			ent->origin, angles);

		if (trace.allsolid || trace.startsolid || trace.fraction < tr->fraction)
		{
			trace.ent = (struct edict_s *)ent;
			*tr = trace;
		}
	}
}


/*
================
CL_PMTrace
================
*/
q2trace_t	VARGS CLQ2_PMTrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	q2trace_t	q2t;
	trace_t		t;

	// check against world
	t = CM_BoxTrace (cl.worldmodel, start, end, mins, maxs, MASK_PLAYERSOLID);
	if (t.fraction < 1.0)
		t.ent = (struct edict_s *)1;

	// check all other solid models
	CLQ2_ClipMoveToEntities (start, mins, maxs, end, &t);

	q2t.allsolid = t.allsolid;
	q2t.contents = t.contents;
	VectorCopy(t.endpos, q2t.endpos);
	q2t.ent = t.ent;
	q2t.fraction = t.fraction;
	q2t.plane = t.plane;
	q2t.startsolid = t.startsolid;
	q2t.surface = t.surface;

	return q2t;
}

int		VARGS CLQ2_PMpointcontents (vec3_t point)
{
	int			i;
	entity_state_t	*ent;
	int			num;
	model_t		*cmodel;
	int			contents;

	contents = CM_PointContents (cl.worldmodel, point);

	for (i=0 ; i<cl.q2frame.num_entities ; i++)
	{
		num = (cl.q2frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];

		if (ent->solid != 31) // special value for bmodel
			continue;

		cmodel = cl.model_precache[ent->modelindex];
		if (!cmodel)
			continue;

		contents |= CM_TransformedPointContents (cl.worldmodel, point, cmodel->hulls[0].firstclipnode, ent->origin, ent->angles);
	}

	return contents;
}

#endif
/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CLQ2_PredictMovement (void)	//q2 doesn't support split clients.
{
#ifdef Q2BSPS
	int			ack, current;
	int			frame;
	int			oldframe;
	q2usercmd_t	*cmd;
	q2pmove_t		pm;
	int			step;
	int			oldz;
#endif
	int			i;
	int pnum = 0;

	if (cls.state != ca_active)
		return;

//	if (cl_paused->value)
//		return;

#ifdef Q2BSPS
	if (cl_nopred.value || (cl.q2frame.playerstate.pmove.pm_flags & Q2PMF_NO_PREDICTION))
#endif
	{	// just set angles
		for (i=0 ; i<3 ; i++)
		{
			cl.predicted_angles[i] = cl.playerview[pnum].viewangles[i] + SHORT2ANGLE(cl.q2frame.playerstate.pmove.delta_angles[i]);
		}
		return;
	}
#ifdef Q2BSPS
	ack = cls.netchan.incoming_acknowledged;
	current = cls.netchan.outgoing_sequence;

	// if we are too far out of date, just freeze
	if (current - ack >= UPDATE_MASK)
	{
//		if (cl_showmiss->value)
//			Con_Printf ("exceeded CMD_BACKUP\n");
		return;
	}

	// copy current state to pmove
	memset (&pm, 0, sizeof(pm));
	pm.trace = CLQ2_PMTrace;
	pm.pointcontents = CLQ2_PMpointcontents;

	pm_airaccelerate = atof(Get_Q2ConfigString(Q2CS_AIRACCEL));

	pm.s = cl.q2frame.playerstate.pmove;

//	SCR_DebugGraph (current - ack - 1, 0);

	frame = 0;

	// run frames
	while (++ack < current)
	{
		frame = ack & (UPDATE_MASK);
		cmd = (q2usercmd_t*)&cl.outframes[frame].cmd[0];

		pm.cmd = *cmd;
		Q2_Pmove (&pm);

		// save for debug checking
		VectorCopy (pm.s.origin, cl_predicted_origins[frame]);
	}

	if (independantphysics[0].msec)
	{
		cmd = (q2usercmd_t*)&independantphysics[0];

		pm.cmd = *cmd;
		Q2_Pmove (&pm);
	}

	oldframe = (ack-2) & (UPDATE_MASK);
	oldz = cl_predicted_origins[oldframe][2];
	step = pm.s.origin[2] - oldz;
	if (step > 63 && step < 160 && (pm.s.pm_flags & Q2PMF_ON_GROUND) )
	{
		cl.predicted_step = step * 0.125;
		cl.predicted_step_time = realtime - host_frametime * 0.5;
	}

	cl.playerview[0].onground = !!(pm.s.pm_flags & Q2PMF_ON_GROUND);


	// copy results out for rendering
	cl.predicted_origin[0] = pm.s.origin[0]*0.125;
	cl.predicted_origin[1] = pm.s.origin[1]*0.125;
	cl.predicted_origin[2] = pm.s.origin[2]*0.125;

	VectorCopy (pm.viewangles, cl.predicted_angles);
#endif
}

/*
=================
CL_NudgePosition

If pmove.origin is in a solid position,
try nudging slightly on all axis to
allow for the cut precision of the net coordinates
=================
*/
void CL_NudgePosition (void)
{
	vec3_t	base;
	int		x, y;

	if (cl.worldmodel->funcs.PointContents (cl.worldmodel, NULL, pmove.origin) == FTECONTENTS_EMPTY)
		return;

	VectorCopy (pmove.origin, base);
	for (x=-1 ; x<=1 ; x++)
	{
		for (y=-1 ; y<=1 ; y++)
		{
			pmove.origin[0] = base[0] + x * 1.0/8;
			pmove.origin[1] = base[1] + y * 1.0/8;
			if (cl.worldmodel->funcs.PointContents (cl.worldmodel, NULL, pmove.origin) == FTECONTENTS_EMPTY)
				return;
		}
	}
	Con_DPrintf ("CL_NudgePosition: stuck\n");
}

#endif

/*
==============
CL_PredictUsercmd
==============
*/
void CL_PredictUsercmd (int pnum, int entnum, player_state_t *from, player_state_t *to, usercmd_t *u)
{
	extern vec3_t player_mins;
	extern vec3_t player_maxs;
	// split up very long moves
	if (u->msec > 50)
	{
		player_state_t temp;
		usercmd_t split;

		split = *u;
		split.msec /= 2;

		CL_PredictUsercmd (pnum, entnum, from, &temp, &split);
		CL_PredictUsercmd (pnum, entnum, &temp, to, &split);
		return;
	}
	if (!cl.worldmodel || cl.worldmodel->needload)
		return;

	VectorCopy (from->origin, pmove.origin);
	VectorCopy (u->angles, pmove.angles);
	VectorCopy (from->velocity, pmove.velocity);
	VectorCopy (from->gravitydir, pmove.gravitydir);

	if (!(pmove.velocity[0] == 0) && !(pmove.velocity[0] != 0))
	{
		Con_Printf("nan velocity!\n");
		pmove.velocity[0] = 0;
		pmove.velocity[1] = 0;
		pmove.velocity[2] = 0;
	}

	pmove.jump_msec = (cls.z_ext & Z_EXT_PM_TYPE) ? 0 : from->jump_msec;
	pmove.jump_held = from->jump_held;
	pmove.waterjumptime = from->waterjumptime;
	pmove.pm_type = from->pm_type;

	pmove.cmd = *u;
	pmove.skipent = entnum;

	movevars.entgravity = cl.playerview[pnum].entgravity;
	movevars.maxspeed = cl.playerview[pnum].maxspeed;
	movevars.bunnyspeedcap = cl.bunnyspeedcap;
	pmove.onladder = false;

	VectorCopy(from->szmins, player_mins);
	VectorCopy(from->szmaxs, player_maxs);

	PM_PlayerMove (cl.gamespeed);

	to->waterjumptime = pmove.waterjumptime;
	to->jump_held = pmove.jump_held;
	to->jump_msec = pmove.jump_msec;
	pmove.jump_msec = 0;

	VectorCopy (pmove.origin, to->origin);
	VectorCopy (pmove.angles, to->viewangles);
	VectorCopy (pmove.velocity, to->velocity);
	VectorCopy (pmove.gravitydir, to->gravitydir);
	to->onground = pmove.onground;

	to->weaponframe = from->weaponframe;
	to->pm_type = from->pm_type;

	VectorCopy(player_mins, to->szmins);
	VectorCopy(player_maxs, to->szmaxs);
}


//Used when cl_nopred is 1 to determine whether we are on ground, otherwise stepup smoothing code produces ugly jump physics
void CL_CatagorizePosition (playerview_t *pv)
{
	if (cl.spectator)
	{
		pv->onground = false;	// in air
		return;
	}
	VectorClear (pmove.velocity);
	VectorCopy (pv->simorg, pmove.origin);
	pmove.numtouch = 0;
	PM_CategorizePosition ();
	pv->onground = pmove.onground;
}
//Smooth out stair step ups.
//Called before CL_EmitEntities so that the player's lightning model origin is updated properly
void CL_CalcCrouch (playerview_t *pv, float stepchange)
{
	qboolean teleported;
	vec3_t delta;
	float orgz = DotProduct(pv->simorg, pv->gravitydir);	//compensate for running on walls.

	VectorSubtract(pv->simorg, pv->oldorigin, delta);

	teleported = Length(delta)>48;
	VectorCopy (pv->simorg, pv->oldorigin);

	if (teleported)
	{
		// possibly teleported or respawned
		pv->oldz = orgz;
		pv->extracrouch = 0;
		pv->crouchspeed = 100;
		pv->crouch = 0;
		VectorCopy (pv->simorg, pv->oldorigin);
		return;
	}

	if (pv->onground && orgz - pv->oldz > 0)
	{
		if (orgz - pv->oldz > movevars.stepheight+2)
		{
			// if on steep stairs, increase speed
			if (pv->crouchspeed < 160)
			{
				pv->extracrouch = orgz - pv->oldz - host_frametime * 200 - 15;
				pv->extracrouch = min(pv->extracrouch, 5);
			}
			pv->crouchspeed = 160;
		}

		pv->oldz += host_frametime * pv->crouchspeed;
		if (pv->oldz > orgz)
			pv->oldz = orgz;

		if (orgz - pv->oldz > 15 + pv->extracrouch)
			pv->oldz = orgz - 15 - pv->extracrouch;
		pv->extracrouch -= host_frametime * 200;
		pv->extracrouch = max(pv->extracrouch, 0);

		pv->crouch = pv->oldz - orgz;
	}
	else
	{
		// in air or moving down
		pv->oldz = orgz;
		pv->crouch += host_frametime * 150;
		if (pv->crouch > 0)
			pv->crouch = 0;
		pv->crouchspeed = 100;
		pv->extracrouch = 0;
	}
}

float LerpAngles360(float to, float from, float frac)
{
	int delta;
	delta = (from-to);

	if (delta > 180)
		delta -= 360;
	if (delta < -180)
		delta += 360;

	return to + frac*delta;
}

//shamelessly ripped from zquake
extern cvar_t cl_nolerp;
static void CL_LerpMove (int pnum, float msgtime)
{
	static int		lastsequence = 0;
	static vec3_t	lerp_angles[3];
	static vec3_t	lerp_origin[3];
	static float	lerp_times[3];
	static qboolean	nolerp[2];
	static float	demo_latency = 0.01;
	float	frac;
	float	simtime;
	int		i;
	int		from, to;

	if (!CL_MayLerp() || cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
		return;

#ifdef NQPROT
	if (cls.demoplayback == DPB_NETQUAKE)
		return;
#endif

	if (cls.netchan.outgoing_sequence < lastsequence)
	{
		// reset
		lastsequence = -1;
		lerp_times[0] = -1;
		demo_latency = 0.01;
	}

	if (cls.netchan.outgoing_sequence > lastsequence)
	{
		lastsequence = cls.netchan.outgoing_sequence;
		// move along
		lerp_times[2] = lerp_times[1];
		lerp_times[1] = lerp_times[0];
		lerp_times[0] = msgtime;

		VectorCopy (lerp_origin[1], lerp_origin[2]);
		VectorCopy (lerp_origin[0], lerp_origin[1]);
		VectorCopy (cl.playerview[pnum].simorg, lerp_origin[0]);

		VectorCopy (lerp_angles[1], lerp_angles[2]);
		VectorCopy (lerp_angles[0], lerp_angles[1]);
		VectorCopy (cl.playerview[pnum].simangles, lerp_angles[0]);

		nolerp[1] = nolerp[0];
		nolerp[0] = false;
		for (i = 0; i < 3; i++)
			if (fabs(lerp_origin[0][i] - lerp_origin[1][i]) > 40)
				break;
		if (i < 3)
			nolerp[0] = true;	// a teleport or something
	}

	simtime = realtime - demo_latency;

	// adjust latency
	if (simtime > lerp_times[0])
	{
		// Com_DPrintf ("HIGH clamp\n");
		demo_latency = realtime - lerp_times[0];
	}
	else if (simtime < lerp_times[2])
	{
		// Com_DPrintf ("   low clamp\n");
		demo_latency = realtime - lerp_times[2];
	}
	else
	{
		// drift towards ideal latency
		float ideal_latency = (lerp_times[0] - lerp_times[2]) * 0.6;
		if (demo_latency > ideal_latency)
			demo_latency = max(demo_latency - host_frametime * 0.1, ideal_latency);
	}

	// decide where to lerp from
	if (simtime > lerp_times[1])
	{
		from = 1;
		to = 0;
	}
	else
	{
		from = 2;
		to = 1;
	}

	if (nolerp[to])
		return;

	frac = (simtime - lerp_times[from]) / (lerp_times[to] - lerp_times[from]);
	frac = bound (0, frac, 1);

	for (i=0 ; i<3 ; i++)
	{
		cl.playerview[pnum].simorg[i] = lerp_origin[from][i] +
				frac * (lerp_origin[to][i] - lerp_origin[from][i]);
		cl.playerview[pnum].simangles[i] = LerpAngles360(lerp_angles[from][i], lerp_angles[to][i], frac);
	}

//	LerpVector (lerp_origin[from], lerp_origin[to], frac, cl.simorg);
//	LerpAngles (lerp_angles[from], lerp_angles[to], frac, cl.simangles);
}

short LerpAngles16(short to, short from, float frac)
{
	int delta;
	delta = (from-to);

	if (delta > 32767)
		delta -= 65535;
	if (delta < -32767)
		delta += 65535;

	return to + frac*delta;
}

void CL_CalcClientTime(void)
{
	if (cls.protocol != CP_QUAKE3)
	{
		float oldst = realtime;

		if (cls.protocol == CP_QUAKEWORLD && cls.demoplayback == DPB_MVD && !(cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
		{
			extern float nextdemotime, olddemotime, demtime;
			float f;
			f = (demtime - olddemotime) / (nextdemotime - olddemotime);
			f = bound(0, f, 1);
			cl.servertime = cl.gametime*f + cl.oldgametime*(1-f);
		}
		else if (0)
		{
			float f;
			f = cl.gametime - cl.oldgametime;
			if (f > 0.1)
				f = 0.1;
			f = (realtime - cl.gametimemark) / (f);
			f = bound(0, f, 1);
			cl.servertime = cl.gametime*f + cl.oldgametime*(1-f);
		}
		else
		{
			float min, max;

			oldst = cl.servertime;

			max = cl.gametime;
			min = cl.oldgametime;
			if (max < min)
				max = min;

			if (max)
				cl.servertime += host_frametime;
			else
				cl.servertime = 0;

			if (cl.servertime > max)
			{
				if (cl.servertime > max)
				{
					cl.servertime = max;
//					Con_Printf("clamped to new time\n");
				}
				else
				{
					cl.servertime -= 0.02*(max - cl.servertime);
				}
			}
			if (cl.servertime < min)
			{
				if (cl.servertime < min-0.5)
				{
					cl.servertime = min-0.5;
//					Con_Printf("clamped to old time\n");
				}
				else if (cl.servertime < min-0.3)
				{
					cl.servertime += 0.02*(min - cl.servertime);
//					Con_Printf("running really slow\n");
				}
				else
				{
					cl.servertime += 0.01*(min - cl.servertime);
//					Con_Printf("running slow\n");
				}
			}
		}
		cl.time = cl.servertime;
		if (oldst == 0)
		{
			int i;
			for (i = 0; i < MAX_CLIENTS; i++)
			{
				cl.players[i].entertime += cl.servertime;
			}
		}
		return;
	}

	if (cls.protocol == CP_NETQUAKE || (cls.demoplayback && cls.demoplayback != DPB_MVD && cls.demoplayback != DPB_EZTV))
	{
		float want;
//		float off;

		want = cl.oldgametime + realtime - cl.gametimemark;
//		off = (want - cl.time);
		if (want>cl.time)	//don't decrease
			cl.time = want;

//		Con_Printf("Drifted to %f off by %f\n", cl.time, off);

//		Con_Printf("\n");
		if (cl.time > cl.gametime)
		{
			cl.time = cl.gametime;
//			Con_Printf("max TimeClamp\n");
		}
		if (cl.time < cl.oldgametime)
		{
			cl.time = cl.oldgametime;
//			Con_Printf("old TimeClamp\n");
		}

	}
	else
	{
		cl.time = realtime - cls.latency - cl_pushlatency.value*0.001;
		if (cl.time > realtime)
			cl.time = realtime;
	}
}

static void CL_DecodeStateSize(unsigned short solid, int modelindex, vec3_t mins, vec3_t maxs)
{
	if (solid == ES_SOLID_BSP)
	{
		if (modelindex < MAX_MODELS && cl.model_precache[modelindex] && !cl.model_precache[modelindex]->needload)
		{
			VectorCopy(cl.model_precache[modelindex]->mins, mins);
			VectorCopy(cl.model_precache[modelindex]->maxs, maxs);
		}
		else
		{
			VectorClear(mins);
			VectorClear(maxs);
		}
	}
	else if (solid)
	{
		mins[0] = -8*(solid&31);
		mins[1] = -8*(solid&31);
		mins[2] = -8*((solid>>5)&31);
		maxs[0] = 8*(solid&31);
		maxs[1] = 8*(solid&31);
		maxs[2] = 8*((solid>>10)&63) - 32;
	}
	else
	{
		VectorClear(mins);
		VectorClear(maxs);
	}
}

/*called on packet reception*/
#include "pr_common.h"
void CL_PlayerFrameUpdated(player_state_t *plstate, entity_state_t *state, int sequence)
{
	/*update the prediction info*/
	vec3_t a;
	int pmtype, i;
	switch(state->u.q1.pmovetype)
	{
	case MOVETYPE_NOCLIP:
		if (cls.z_ext & Z_EXT_PM_TYPE_NEW)
			pmtype = PM_SPECTATOR;
		else
			pmtype = PM_OLD_SPECTATOR;
		break;
	
	case MOVETYPE_FLY:
		pmtype = PM_FLY;
		break;
	case MOVETYPE_NONE:
		pmtype = PM_NONE;
		break;
	case MOVETYPE_BOUNCE:
	case MOVETYPE_TOSS:
		pmtype = PM_DEAD;
		break;
	case MOVETYPE_WALLWALK:
		pmtype = PM_WALLWALK;
		break;
	default:
		pmtype = PM_NORMAL;
		break;
	}

	plstate->pm_type = pmtype;
	VectorCopy(state->origin, plstate->origin);
	plstate->command.angles[0] = state->angles[0] * -3 *65536/360.0;
	plstate->command.angles[1] = state->angles[1] * 65536/360.0;
	plstate->command.angles[2] = state->angles[2] * 65536/360.0;
	VectorScale(state->u.q1.velocity, 1/8.0, plstate->velocity);
	plstate->messagenum = sequence;

	a[0] = ((-192-state->u.q1.gravitydir[0])/256.0f) * 360;
	a[1] = (state->u.q1.gravitydir[1]/256.0f) * 360;
	a[2] = 0;
	AngleVectors(a, plstate->gravitydir, NULL, NULL);

	cl.players[state->number-1].stats[STAT_WEAPONFRAME] = state->u.q1.weaponframe;
	cl.players[state->number-1].statsf[STAT_WEAPONFRAME] = state->u.q1.weaponframe;
	for (i = 0; i < cl.splitclients; i++)
	{
		if (cl.playerview[i].playernum == state->number-1)
		{
			cl.playerview[i].stats[STAT_WEAPONFRAME] = state->u.q1.weaponframe;
			cl.playerview[i].statsf[STAT_WEAPONFRAME] = state->u.q1.weaponframe;
		}
	}

	CL_DecodeStateSize(state->solid, state->modelindex, plstate->szmins, plstate->szmaxs);
}

/*called once every rendered frame*/
qboolean CL_PredictPlayer(lerpents_t *le, entity_state_t *state, int sequence)
{
	int msec, oldphysent;
	usercmd_t cmd;
	player_state_t start, exact;
	int pnum;

	if (state->number-1 > cl.allocated_client_slots || cl.intermission)
		return false;

	/*local players just interpolate for now. the prediction code will move it to the right place afterwards*/
	for (pnum = 0; pnum < cl.splitclients; pnum++)
	{
		if (state->number-1 == cl.playerview[pnum].playernum)
			return false;
	}

	memset(&cmd, 0, sizeof(cmd));
	memset(&start, 0, sizeof(start));

	CL_PlayerFrameUpdated(&start, state, sequence);

	msec = 500*(realtime - cls.latency + 0.02 - cl.inframes[sequence & UPDATE_MASK].receivedtime);
	cmd.msec = bound(0, msec, 255);
	cmd.forwardmove = state->u.q1.movement[0];
	cmd.sidemove = state->u.q1.movement[1];
	cmd.upmove = state->u.q1.movement[2];

	oldphysent = pmove.numphysent;
	CL_PredictUsercmd (0, state->number, &start, &exact, &cmd);	//uses player 0's maxspeed/grav...
	pmove.numphysent = oldphysent;

	/*need to update the entity's angles and origin so the linkentities function puts it in the correct predicted place*/
	le->angles[0] = state->angles[0];
	le->angles[1] = state->angles[1];
	le->angles[2] = state->angles[2];
	VectorCopy (exact.origin, le->origin);

	return true;
}

/*
==============
CL_PredictMove
==============
*/
void CL_PredictMovePNum (int seat)
{
	playerview_t *pv = &cl.playerview[seat];
	inframe_t	indstate;
	outframe_t	indcmd;
	int			i;
	float		f;
	inframe_t	*from, *to = NULL;
	outframe_t	*cmdfrom, *cmdto;
	int			oldphysent;
	vec3_t lrp, lrpv;
	double		simtime;
	extern cvar_t cl_netfps;
	
	//these are to make svc_viewentity work better
	float *vel;
	float *org;
	float stepheight = 0;
	float netfps = cl_netfps.value;
	if (!netfps)
	{
		//every video frame has its own input frame.
		simtime = realtime;
	}
	else
	{
		netfps = bound(6.7, netfps, cls.maxfps);
		if (netfps < 30)
		{
			//extrapolate if we've a low net rate. This should reduce apparent lag, but will be jerky if the net rate is not an (inverse) multiple of the monitor rate.
			//this is in addition to any monitor desync.
			simtime = realtime;
		}
		else
		{
			//interpolate. The input rate is completely smoothed out, at the cost of some latency.
			//You can still get juddering if the video rate doesn't match the monitor refresh rate (and isn't so high that it doesn't matter).
			//note that the code below will back-date input frames if the server acks too fast.
			netfps = bound(6.7, netfps, cls.maxfps);
			simtime = realtime - (1/netfps);
		}
	}

	pv->nolocalplayer = !!(cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS) || (cls.protocol != CP_QUAKEWORLD);

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
	{
		if (!cl.worldmodel || cl.worldmodel->needload)
			return;
		pv->crouch = 0;
		CLQ2_PredictMovement();
		return;
	}
#endif

	if (cl_pushlatency.value > 0)
		Cvar_Set (&cl_pushlatency, "0");

	if (cl.paused && !(cls.demoplayback!=DPB_MVD && cls.demoplayback!=DPB_EZTV) && (!cl.spectator || !pv->cam_auto))
		return;

	if (cl.intermission==1 && cls.protocol == CP_QUAKEWORLD)
	{
		pv->crouch = 0;
		return;
	}

#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE && !(cls.fteprotocolextensions2 & PEXT2_PREDINFO))
	{
		cl.ackedmovesequence = cl.movesequence;
	}
#endif

	if (!cl.validsequence)
	{
		return;
	}
	if (cl.movesequence - cl.ackedmovesequence >= UPDATE_BACKUP-1)
	{	//lagging like poo.
		if (!cl.intermission)	//keep the angles working though.
			VectorCopy (pv->viewangles, pv->simangles);
		return;
	}

	// this is the last frame received from the server
	from = &cl.inframes[cl.validsequence & UPDATE_MASK];
	cmdfrom = &cl.outframes[cl.ackedmovesequence & UPDATE_MASK];

	if (!cl.intermission)
	{
		VectorCopy (pv->viewangles, pv->simangles);
	}

	vel = from->playerstate[pv->playernum].velocity;
	org = from->playerstate[pv->playernum].origin;

#ifdef PEXT_SETVIEW
	//if the view is attached to an arbitary entity...
	if (pv->viewentity && (pv->viewentity != pv->playernum+1 || cl.ackedmovesequence == cl.movesequence))
	{
		if (pv->viewentity >= 0 && pv->viewentity <= cl.allocated_client_slots && from->playerstate[pv->viewentity-1].messagenum == cl.validsequence)
		{
		}
		else if (pv->viewentity < cl.maxlerpents)
		{
			pv->nolocalplayer = true;
//			Con_Printf("Using lerped pos\n");
			org = cl.lerpents[pv->viewentity].origin;
			vel = vec3_origin;
			goto fixedorg;
		}
	}
#endif
	if (!from->playerstate[pv->playernum].messagenum)
	{
		//no player states?? put the view on an ent
		if (pv->playernum < cl.maxlerpents)
		{
			pv->nolocalplayer = true;
//			Con_Printf("Using lerped pos\n");
			org = cl.lerpents[pv->playernum+1].origin;
			vel = vec3_origin;
			goto fixedorg;
		}
	}


	if (((cl_nopred.value && cls.demoplayback!=DPB_MVD && cls.demoplayback != DPB_EZTV)|| pv->fixangle || cl.paused))
	{
		if (cl_lerp_players.ival && !cls.demoplayback)
		{
			lerpents_t *le;
			if (pv->nolocalplayer) 
				le = &cl.lerpents[pv->cam_spec_track+1];
			else
				le = &cl.lerpplayers[pv->cam_spec_track];
			org = le->origin;
			vel = vec3_origin;
		}

fixedorg:
		VectorCopy (vel, pv->simvel);
		VectorCopy (org, pv->simorg);

//		to = &cl.inframes[cl.ackedinputsequence & UPDATE_MASK];



		CL_CatagorizePosition(pv);
		goto out;
	}

	// predict forward until cl.time <= to->senttime
	oldphysent = pmove.numphysent;
	CL_SetSolidPlayers();
	pmove.skipent = pv->playernum+1;

//	Con_Printf("%i<%i  %i\n", cl.ackedmovesequence, cl.movesequence, cl.validsequence);

	to = &cl.inframes[cl.validsequence & UPDATE_MASK];
	cmdto = &cl.outframes[cl.ackedmovesequence & UPDATE_MASK];

	if (pv->viewentity && pv->viewentity != pv->playernum+1 && CL_MayLerp())
	{
		float f;
		if (cl_lerp_players.ival && (cls.demoplayback==DPB_MVD || cls.demoplayback == DPB_EZTV))
		{
			lerpents_t *le = &cl.lerpplayers[pv->cam_spec_track];
			org = le->origin;
			vel = vec3_origin;
			VectorCopy(le->angles, pv->simangles);
			goto fixedorg;
		}

		to = &cl.inframes[cl.validsequence & UPDATE_MASK];
		from = &cl.inframes[cl.oldvalidsequence & UPDATE_MASK];

		//figure out the lerp factor
		if (cl.gametime == cl.servertime)
			f = 0;
		else
		{
			f = (cl.gametime-cl.servertime)/(cl.gametime-cl.oldgametime);
			//f = (cl.time-cl.lerpents[state->number].lerptime)/cl.lerpents[state->number].lerprate;
		}
		if (f<0)
			f=0;
		if (f>1)
			f=1;
//		f = 1-f;


		// calculate origin
		for (i=0 ; i<3 ; i++)
		{
			lrp[i] = to->playerstate[pv->cam_spec_track].origin[i] +
			f * (from->playerstate[pv->cam_spec_track].origin[i] - to->playerstate[pv->cam_spec_track].origin[i]);

			lrpv[i] = to->playerstate[pv->cam_spec_track].velocity[i] +
			f * (from->playerstate[pv->cam_spec_track].velocity[i] - to->playerstate[pv->cam_spec_track].velocity[i]);

			pv->simangles[i] = LerpAngles16(to->playerstate[pv->cam_spec_track].command.angles[i], from->playerstate[pv->cam_spec_track].command.angles[i], f)*360.0f/65535;
		}

		org = lrp;
		vel = lrpv;

		pv->pmovetype = PM_NONE;
		goto fixedorg;
	}
	else
	{
		if (cls.demoplayback==DPB_MVD || cls.demoplayback==DPB_EZTV)
		{
			from = &cl.inframes[(cl.ackedmovesequence-1) & UPDATE_MASK];
			to = &cl.inframes[(cl.ackedmovesequence) & UPDATE_MASK];
			cmdto = &cl.outframes[(cl.ackedmovesequence) & UPDATE_MASK];
			to->playerstate->pm_type = PM_SPECTATOR;
			simtime = cmdto->senttime;

			VectorCopy (pv->simvel, from->playerstate[pv->playernum].velocity);
			VectorCopy (pv->simorg, from->playerstate[pv->playernum].origin);

			CL_PredictUsercmd (seat, 0, &from->playerstate[pv->playernum], &to->playerstate[pv->playernum], &cmdto->cmd[seat]);
		}
		else
		{
			for (i=1 ; i<UPDATE_BACKUP-1 && cl.ackedmovesequence+i <
					cl.movesequence; i++)
			{
				to = &cl.inframes[(cl.validsequence+i) & UPDATE_MASK];
				cmdto = &cl.outframes[(cl.ackedmovesequence+i) & UPDATE_MASK];

				CL_PredictUsercmd (seat, pv->playernum+1, &from->playerstate[pv->playernum], &to->playerstate[pv->playernum], &cmdto->cmd[seat]);

				if (cmdto->senttime >= simtime)
					break;
				from = to;
				cmdfrom = cmdto;
			}
		}

		if (simtime > cmdto->senttime)
		{
			float msec;
			cmdfrom = cmdto;
			from = to;
			to = &indstate;
			cmdto = &indcmd;
			if (independantphysics[seat].msec && !cls.demoplayback)
				cmdto->cmd[seat] = independantphysics[seat];
			else
				cmdto->cmd[seat] = cmdfrom->cmd[seat];
			cmdto->senttime = simtime;
			msec = ((cmdto->senttime - cmdfrom->senttime) * 1000) + 0.5;
			cmdto->cmd[seat].msec = bound(0, msec, 250);

			CL_PredictUsercmd (seat, pv->playernum+1, &from->playerstate[pv->playernum]
				, &to->playerstate[pv->playernum], &cmdto->cmd[seat]);
		}
		pv->onground = pmove.onground;
		pv->pmovetype = to->playerstate[pv->playernum].pm_type;
		stepheight = to->playerstate[pv->playernum].origin[2] - from->playerstate[pv->playernum].origin[2];
	}

	//backdate it if our simulation time is in the past. this will happen on localhost, but not on 300-ping servers.
	for (i = 0; cmdfrom->senttime > simtime && i < 16; i++)
	{
		to = from;
		cmdto = cmdfrom;
		from = &cl.inframes[(cl.validsequence-i) & UPDATE_MASK];
		cmdfrom = &cl.outframes[(cl.ackedmovesequence-i) & UPDATE_MASK];
	}

	pmove.numphysent = oldphysent;

//	Con_Printf("%f %f %f\n", cmdfrom->senttime, simtime, cmdto->senttime);
	if (cmdto->senttime == cmdfrom->senttime)
	{
		VectorCopy (to->playerstate[pv->playernum].velocity, pv->simvel);
		VectorCopy (to->playerstate[pv->playernum].origin, pv->simorg);
	}
	else
	{
		int pnum = pv->playernum;
		// now interpolate some fraction of the final frame
		f = (simtime - cmdfrom->senttime) / (cmdto->senttime - cmdfrom->senttime);

		if (f < 0)
			f = 0;
		if (f > 1)
			f = 1;

		for (i=0 ; i<3 ; i++)
			if ( fabs(from->playerstate[pnum].origin[i] - to->playerstate[pnum].origin[i]) > 128)
			{	// teleported, so don't lerp
				VectorCopy (to->playerstate[pnum].velocity, pv->simvel);
				VectorCopy (to->playerstate[pnum].origin, pv->simorg);
				goto out;
			}

		for (i=0 ; i<3 ; i++)
		{
			pv->simorg[i] = (1-f)*from->playerstate[pnum].origin[i]   + f*to->playerstate[pnum].origin[i];
			pv->simvel[i] = (1-f)*from->playerstate[pnum].velocity[i] + f*to->playerstate[pnum].velocity[i];

/*			if (cl.spectator && Cam_TrackNum(vnum) >= 0)
				pv->simangles[i] = LerpAngles16(from->playerstate[pnum].command.angles[i], to->playerstate[pnum].command.angles[i], f) * (360.0/65535);
			else if (cls.demoplayback == DPB_QUAKEWORLD)
				pv->simangles[i] = LerpAngles16(cmdfrom->cmd[vnum].angles[i], cmdto->cmd[vnum].angles[i], f) * (360.0/65535);
*/		}
		CL_CatagorizePosition(pv);

	}

	if (pv->nolocalplayer && cl.maxlerpents > pv->playernum+1)
	{
		//keep the entity tracking the prediction position, so mirrors don't go all weird
		VectorCopy(to->playerstate[pv->playernum].origin, cl.lerpents[pv->playernum+1].origin);
		VectorScale(pv->simangles, 1, cl.lerpents[pv->playernum+1].angles);
		cl.lerpents[pv->playernum+1].angles[0] *= -0.333;
	}

	if (cls.demoplayback)
		CL_LerpMove (seat, cmdto->senttime);

out:
	CL_CalcCrouch (pv, stepheight);
	pv->waterlevel = pmove.waterlevel;
	VectorCopy(pmove.gravitydir, pv->gravitydir);
}

void CL_PredictMove (void)
{
	int i;

	//work out which packet entities are solid
	CL_SetSolidEntities ();

	// Set up prediction for other players
	CL_SetUpPlayerPrediction(false);

	// do client side motion prediction
	for (i = 0; i < cl.splitclients; i++)
		CL_PredictMovePNum(i);

	// Set up prediction for other players
	CL_SetUpPlayerPrediction(true);
}


/*
==============
CL_InitPrediction
==============
*/
void CL_InitPrediction (void)
{
	extern char cl_predictiongroup[];
	Cvar_Register (&cl_pushlatency, cl_predictiongroup);
	Cvar_Register (&cl_nopred,	cl_predictiongroup);
}
