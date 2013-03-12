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

		if (ent->number == cl.playernum[0]+1)
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
		cmd = (q2usercmd_t*)&cl.frames[frame].cmd[0];

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

	cl.onground[0] = !!(pm.s.pm_flags & Q2PMF_ON_GROUND);


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

	movevars.entgravity = cl.entgravity[pnum];
	movevars.maxspeed = cl.maxspeed[pnum];
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
void CL_CatagorizePosition (int pnum)
{
	if (cl.spectator)
	{
		cl.onground[pnum] = false;	// in air
		return;
	}
	VectorClear (pmove.velocity);
	VectorCopy (cl.playerview[pnum].simorg, pmove.origin);
	pmove.numtouch = 0;
	PM_CategorizePosition ();
	cl.onground[pnum] = pmove.onground;
}
//Smooth out stair step ups.
//Called before CL_EmitEntities so that the player's lightning model origin is updated properly
void CL_CalcCrouch (int pnum, float stepchange)
{
	qboolean teleported;
	static vec3_t oldorigin[MAX_SPLITS];
	static float oldz[MAX_SPLITS] = {0}, extracrouch[MAX_SPLITS] = {0}, crouchspeed[MAX_SPLITS] = {100,100};
	vec3_t delta;

	VectorSubtract(cl.playerview[pnum].simorg, oldorigin[pnum], delta);

	teleported = Length(delta)>48;
	VectorCopy (cl.playerview[pnum].simorg, oldorigin[pnum]);

	if (teleported)
	{
		// possibly teleported or respawned
		oldz[pnum] = cl.playerview[pnum].simorg[2];
		extracrouch[pnum] = 0;
		crouchspeed[pnum] = 100;
		cl.crouch[pnum] = 0;
		VectorCopy (cl.playerview[pnum].simorg, oldorigin[pnum]);
		return;
	}

	if (cl.onground[pnum] && cl.playerview[pnum].simorg[2] - oldz[pnum] > 0)
	{
		if (cl.playerview[pnum].simorg[2] - oldz[pnum] > movevars.stepheight+2)
		{
			// if on steep stairs, increase speed
			if (crouchspeed[pnum] < 160)
			{
				extracrouch[pnum] = cl.playerview[pnum].simorg[2] - oldz[pnum] - host_frametime * 200 - 15;
				extracrouch[pnum] = min(extracrouch[pnum], 5);
			}
			crouchspeed[pnum] = 160;
		}

		oldz[pnum] += host_frametime * crouchspeed[pnum];
		if (oldz[pnum] > cl.playerview[pnum].simorg[2])
			oldz[pnum] = cl.playerview[pnum].simorg[2];

		if (cl.playerview[pnum].simorg[2] - oldz[pnum] > 15 + extracrouch[pnum])
			oldz[pnum] = cl.playerview[pnum].simorg[2] - 15 - extracrouch[pnum];
		extracrouch[pnum] -= host_frametime * 200;
		extracrouch[pnum] = max(extracrouch[pnum], 0);

		cl.crouch[pnum] = oldz[pnum] - cl.playerview[pnum].simorg[2];
	}
	else
	{
		// in air or moving down
		oldz[pnum] = cl.playerview[pnum].simorg[2];
		cl.crouch[pnum] += host_frametime * 150;
		if (cl.crouch[pnum] > 0)
			cl.crouch[pnum] = 0;
		crouchspeed[pnum] = 100;
		extracrouch[pnum] = 0;
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

			if (max)
				cl.servertime += host_frametime;
			else
				cl.servertime = 0;

			if (cl.servertime > max)
			{
				if (cl.servertime > cl.gametime)
				{
					cl.servertime = cl.gametime;
//					Con_Printf("clamped to new time\n");
				}
				else
				{
					cl.servertime -= 0.02*(cl.gametime - cl.servertime);
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
		if (cl.playernum[i] == state->number-1)
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
		if (state->number-1 == cl.playernum[pnum])
			return false;
	}

	memset(&cmd, 0, sizeof(cmd));
	memset(&start, 0, sizeof(start));

	CL_PlayerFrameUpdated(&start, state, sequence);

	msec = 500*(realtime - cls.latency + 0.02 - cl.frames[sequence & UPDATE_MASK].receivedtime);
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
void CL_PredictMovePNum (int pnum)
{
	frame_t ind;
	int			i;
	float		f;
	frame_t		*from, *to = NULL;
	int			oldphysent;
	vec3_t lrp, lrpv;

	//these are to make svc_viewentity work better
	float *vel;
	float *org;
	float stepheight = 0;

	cl.nolocalplayer[pnum] = !!(cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS);

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
	{
		if (!cl.worldmodel || cl.worldmodel->needload)
			return;
		cl.crouch[pnum] = 0;
		CLQ2_PredictMovement();
		return;
	}
#endif

	if (cl_pushlatency.value > 0)
		Cvar_Set (&cl_pushlatency, "0");

	if (cl.paused && !(cls.demoplayback!=DPB_MVD && cls.demoplayback!=DPB_EZTV) && (!cl.spectator || !autocam[pnum]))
		return;

	if (cl.intermission==1 && cls.protocol == CP_QUAKEWORLD)
	{
		cl.crouch[pnum] = 0;
		return;
	}

#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE)
	{
		cl.ackedinputsequence = cls.netchan.outgoing_sequence;
	}
#endif

	if (!cl.ackedinputsequence)
	{
		return;
	}
	if (cls.netchan.outgoing_sequence - cl.ackedinputsequence >= UPDATE_BACKUP-1)
	{	//lagging like poo.
		if (!cl.intermission)	//keep the angles working though.
			VectorCopy (cl.playerview[pnum].viewangles, cl.playerview[pnum].simangles);
		return;
	}

	// this is the last frame received from the server
	from = &cl.frames[cl.ackedinputsequence & UPDATE_MASK];

	if (!cl.intermission)
	{
		VectorCopy (cl.playerview[pnum].viewangles, cl.playerview[pnum].simangles);
	}

	vel = from->playerstate[cl.playernum[pnum]].velocity;
	org = from->playerstate[cl.playernum[pnum]].origin;

#ifdef PEXT_SETVIEW
	if (cl.viewentity[pnum])
	{
		if (cl.viewentity[pnum] < cl.maxlerpents)
		{
			cl.nolocalplayer[pnum] = true;
//			Con_Printf("Using lerped pos\n");
			org = cl.lerpents[cl.viewentity[pnum]].origin;
			vel = vec3_origin;
			goto fixedorg;
		}
	}
#endif
	if (!from->playerstate[cl.playernum[pnum]].messagenum)
	{
		//no player states?? put the view on an ent
		if (cl.playernum[pnum] < cl.maxlerpents)
		{
			cl.nolocalplayer[pnum] = true;
//			Con_Printf("Using lerped pos\n");
			org = cl.lerpents[cl.playernum[pnum]+1].origin;
			vel = vec3_origin;
			goto fixedorg;
		}
	}


	if (((cl_nopred.value && cls.demoplayback!=DPB_MVD && cls.demoplayback != DPB_EZTV)|| cl.playerview[pnum].fixangle || cl.paused))
	{
		if (cl_lerp_players.ival && !cls.demoplayback)
		{
			lerpents_t *le;
			if (cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS) 
				le = &cl.lerpents[spec_track[pnum]+1];
			else
				le = &cl.lerpplayers[spec_track[pnum]];
			org = le->origin;
			vel = vec3_origin;
		}

fixedorg:
		VectorCopy (vel, cl.playerview[pnum].simvel);
		VectorCopy (org, cl.playerview[pnum].simorg);

		to = &cl.frames[cl.ackedinputsequence & UPDATE_MASK];



		CL_CatagorizePosition(pnum);
		goto out;
	}

	// predict forward until cl.time <= to->senttime
	oldphysent = pmove.numphysent;
	CL_SetSolidPlayers();
	pmove.skipent = cl.playernum[pnum]+1;

	to = &cl.frames[cl.ackedinputsequence & UPDATE_MASK];

	if (Cam_TrackNum(pnum)>=0 && CL_MayLerp())
	{
		float f;
		if (cl_lerp_players.ival && (cls.demoplayback==DPB_MVD || cls.demoplayback == DPB_EZTV))
		{
			lerpents_t *le = &cl.lerpplayers[spec_track[pnum]];
			org = le->origin;
			vel = vec3_origin;
			VectorCopy(le->angles, cl.playerview[pnum].simangles);
			goto fixedorg;
		}

		to = &cl.frames[cl.validsequence & UPDATE_MASK];
		from = &cl.frames[cl.oldvalidsequence & UPDATE_MASK];

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
			lrp[i] = to->playerstate[spec_track[pnum]].origin[i] +
			f * (from->playerstate[spec_track[pnum]].origin[i] - to->playerstate[spec_track[pnum]].origin[i]);

			lrpv[i] = to->playerstate[spec_track[pnum]].velocity[i] +
			f * (from->playerstate[spec_track[pnum]].velocity[i] - to->playerstate[spec_track[pnum]].velocity[i]);

			cl.playerview[pnum].simangles[i] = LerpAngles16(to->playerstate[spec_track[pnum]].command.angles[i], from->playerstate[spec_track[pnum]].command.angles[i], f)*360.0f/65535;
		}

		org = lrp;
		vel = lrpv;

		cl.pmovetype[pnum] = PM_NONE;
		goto fixedorg;
	}
	else
	{
		if (cls.demoplayback==DPB_MVD || cls.demoplayback==DPB_EZTV)
		{
			to = &cl.frames[(cls.netchan.outgoing_sequence-1) & UPDATE_MASK];
			to->playerstate->pm_type = PM_SPECTATOR;

			VectorCopy (cl.playerview[pnum].simvel, from->playerstate[cl.playernum[pnum]].velocity);
			VectorCopy (cl.playerview[pnum].simorg, from->playerstate[cl.playernum[pnum]].origin);

			CL_PredictUsercmd (pnum, cl.playernum[pnum]+1, &from->playerstate[cl.playernum[pnum]], &to->playerstate[cl.playernum[pnum]], &to->cmd[pnum]);
		}
		else
		{
			for (i=1 ; i<UPDATE_BACKUP-1 && cl.ackedinputsequence+i <
					cls.netchan.outgoing_sequence; i++)
			{
				to = &cl.frames[(cl.ackedinputsequence+i) & UPDATE_MASK];
				CL_PredictUsercmd (pnum, cl.playernum[pnum]+1, &from->playerstate[cl.playernum[pnum]]
					, &to->playerstate[cl.playernum[pnum]], &to->cmd[pnum]);

				if (to->senttime >= realtime)
					break;
				from = to;
			}
		}

		if (independantphysics[pnum].msec)
		{
			from = to;
			to = &ind;
			to->cmd[pnum] = independantphysics[pnum];
			to->senttime = realtime;
				CL_PredictUsercmd (pnum, cl.playernum[pnum]+1, &from->playerstate[cl.playernum[pnum]]
				, &to->playerstate[cl.playernum[pnum]], &to->cmd[pnum]);
		}
		cl.onground[pnum] = pmove.onground;
		cl.pmovetype[pnum] = to->playerstate[cl.playernum[pnum]].pm_type;
		stepheight = to->playerstate[cl.playernum[pnum]].origin[2] - from->playerstate[cl.playernum[pnum]].origin[2];

		if (cl.nolocalplayer[pnum] && cl.maxlerpents > cl.playernum[pnum]+1)
		{
			//keep the entity tracking the prediction position, so mirrors don't go all weird
			VectorCopy(to->playerstate[cl.playernum[pnum]].origin, cl.lerpents[cl.playernum[pnum]+1].origin);
			VectorScale(to->cmd[pnum].angles, 360.0f / 0xffff, cl.lerpents[cl.playernum[pnum]+1].angles);
			cl.lerpents[cl.playernum[pnum]+1].angles[0] *= -0.333;
		}
	}

	pmove.numphysent = oldphysent;

	if (1)//!independantphysics.msec)
	{
		VectorCopy (to->playerstate[cl.playernum[pnum]].velocity, cl.playerview[pnum].simvel);
		VectorCopy (to->playerstate[cl.playernum[pnum]].origin, cl.playerview[pnum].simorg);
	}
	else
	{
		// now interpolate some fraction of the final frame
		if (to->senttime == from->senttime)
			f = 0;
		else
		{
			f = (realtime - from->senttime) / (to->senttime - from->senttime);

			if (f < 0)
				f = 0;
			if (f > 1)
				f = 1;
		}

		for (i=0 ; i<3 ; i++)
			if ( fabs(org[i] - to->playerstate[cl.playernum[pnum]].origin[i]) > 128)
			{	// teleported, so don't lerp
				VectorCopy (to->playerstate[cl.playernum[pnum]].velocity, cl.playerview[pnum].simvel);
				VectorCopy (to->playerstate[cl.playernum[pnum]].origin, cl.playerview[pnum].simorg);
				goto out;
			}

		for (i=0 ; i<3 ; i++)
		{
			cl.playerview[pnum].simorg[i] = org[i]
				+ f*(to->playerstate[cl.playernum[pnum]].origin[i] - org[i]);
			cl.playerview[pnum].simvel[i] = vel[i]
				+ f*(to->playerstate[cl.playernum[pnum]].velocity[i] - vel[i]);
		}
		CL_CatagorizePosition(pnum);

	}

	if (cls.demoplayback)
		CL_LerpMove (pnum, to->senttime);

out:
	CL_CalcCrouch (pnum, stepheight);
	cl.waterlevel[pnum] = pmove.waterlevel;
	VectorCopy(pmove.gravitydir, cl.playerview[pnum].gravitydir);
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
