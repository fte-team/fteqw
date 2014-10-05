/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/* ZOID
 *
 * Player camera tracking in Spectator mode
 *
 * This takes over player controls for spectator automatic camera.
 * Player moves as a spectator, but the camera tracks and enemy player
 */

#include "quakedef.h"
#include "winquake.h"

#define	PM_SPECTATORMAXSPEED	500
#define	PM_STOPSPEED	100
#define	PM_MAXSPEED			320
#define BUTTON_JUMP 2
#define BUTTON_ATTACK 1
#define MAX_ANGLE_TURN 10

char cl_spectatorgroup[] = "Spectator Tracking";

// track high fragger
cvar_t cl_hightrack = SCVAR("cl_hightrack", "0");

cvar_t cl_chasecam = SCVAR("cl_chasecam", "1");
cvar_t cl_selfcam = SCVAR("cl_selfcam", "1");

//cvar_t cl_camera_maxpitch = {"cl_camera_maxpitch", "10" };
//cvar_t cl_camera_maxyaw = {"cl_camera_maxyaw", "30" };


int selfcam=1;

static float vlen(vec3_t v)
{
	return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

// returns true if weapon model should be drawn in camera mode
qboolean Cam_DrawViewModel(playerview_t *pv)
{
	if (cl.spectator)
	{
		if (pv->cam_auto && pv->cam_locked && cl_chasecam.ival)
			return true;
		return false;
	}
	else
	{
		if (selfcam == 1 && !r_refdef.externalview)
			return true;
		return false;
	}
}

int Cam_TrackNum(playerview_t *pv)
{
	if (!pv->cam_auto)
		return -1;
	return pv->cam_spec_track;
}

void Cam_Unlock(playerview_t *pv)
{
	if (pv->cam_auto)
	{
		CL_SendClientCommand(true, "ptrack");
		pv->cam_auto = CAM_NONE;
		pv->cam_locked = false;
		pv->viewentity = (cls.demoplayback)?0:(pv->playernum+1);	//free floating
		Sbar_Changed();
	}
}

void Cam_Lock(playerview_t *pv, int playernum)
{
	int i;

	pv->cam_lastviewtime = -1000;

	CL_SendClientCommand(true, "ptrack %i", playernum);

	pv->cam_spec_track = playernum;
	pv->cam_locked = false;
	pv->viewentity = (cls.demoplayback)?0:(pv->playernum+1);	//free floating until actually locked

	
	Skin_FlushPlayers();

	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
	{
		memcpy(&pv->stats, cl.players[playernum].stats, sizeof(pv->stats));
		pv->cam_locked = true;	//instantly lock if the player is valid.
		pv->viewentity = playernum+1;
	}

	Sbar_Changed();

	for (i = 0; i < cl.allocated_client_slots; i++)
		CL_NewTranslation(i);
}

trace_t Cam_DoTrace(vec3_t vec1, vec3_t vec2)
{
#if 0
	memset(&pmove, 0, sizeof(pmove));

	pmove.numphysent = 1;
	memset(&pmove.physents[0], 0, sizeof(physent_t));
	VectorClear (pmove.physents[0].origin);
	pmove.physents[0].model = cl.worldmodel;
#endif

	VectorCopy (vec1, pmove.origin);
	return PM_PlayerTrace(pmove.origin, vec2, MASK_PLAYERSOLID);
}
	
// Returns distance or 9999 if invalid for some reason
static float Cam_TryFlyby(vec3_t selforigin, vec3_t playerorigin, vec3_t vec, qboolean checkvis)
{
	vec3_t v;
	trace_t trace;
	float len;

	pmove.player_mins[0] = pmove.player_mins[1] = -16;
	pmove.player_mins[2] = -24;
	pmove.player_maxs[0] = pmove.player_maxs[1] = 16;
	pmove.player_maxs[2] = 32;

	VectorAngles(vec, NULL, v);
//	v[0] = -v[0];
	VectorCopy (v, pmove.angles);
	VectorNormalize(vec);
	VectorMA(playerorigin, 800, vec, v);
	// v is endpos
	// fake a player move
	trace = Cam_DoTrace(playerorigin, v);
	if (/*trace.inopen ||*/ trace.inwater)
		return 9999;
	VectorCopy(trace.endpos, vec);
	VectorSubtract(trace.endpos, playerorigin, v);
	len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
	if (len < 32 || len > 800)
		return 9999;
	if (checkvis)
	{
		VectorSubtract(trace.endpos, selforigin, v);
		len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

		trace = Cam_DoTrace(selforigin, vec);
		if (trace.fraction != 1 || trace.inwater)
			return 9999;
	}
	return len;
}

// Is player visible?
static qboolean Cam_IsVisible(vec3_t playerorigin, vec3_t vec)
{
	trace_t trace;
	vec3_t v;
	float d;

	trace = Cam_DoTrace(playerorigin, vec);
	if (trace.fraction != 1 || /*trace.inopen ||*/ trace.inwater)
		return false;
	// check distance, don't let the player get too far away or too close
	VectorSubtract(playerorigin, vec, v);
	d = vlen(v);
	if (d < 16)
		return false;
	return true;
}

static qboolean InitFlyby(playerview_t *pv, vec3_t selforigin, vec3_t playerorigin, vec3_t playerviewangles, int checkvis) 
{
    float f, max;
    vec3_t vec, vec2;
	vec3_t forward, right, up;

	VectorCopy(playerviewangles, vec);
    vec[0] = 0;
	AngleVectors (vec, forward, right, up);
//	for (i = 0; i < 3; i++)
//		forward[i] *= 3;

    max = 1000;
	VectorAdd(forward, up, vec2);
	VectorAdd(vec2, right, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorAdd(forward, up, vec2);
	VectorSubtract(vec2, right, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorAdd(forward, right, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorSubtract(forward, right, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorAdd(forward, up, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorSubtract(forward, up, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorAdd(up, right, vec2);
	VectorSubtract(vec2, forward, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorSubtract(up, right, vec2);
	VectorSubtract(vec2, forward, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	// invert
	VectorNegate(forward, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorCopy(forward, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	// invert
	VectorNegate(right, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorCopy(right, vec2);
    if ((f = Cam_TryFlyby(selforigin, playerorigin, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }

	// ack, can't find him
    if (max >= 1000)
	{
//		Cam_Unlock();
		return false;
	}

	pv->cam_locked = true;
	pv->viewentity = pv->cam_spec_track+1;
	VectorCopy(vec, pv->cam_desired_position); 
	return true;
}

static void Cam_CheckHighTarget(playerview_t *pv)
{
	int i, j, max;
	player_info_t	*s;
	playerview_t *spv;

	j = -1;
	for (i = 0, max = -9999; i < cl.allocated_client_slots; i++)
	{
		s = &cl.players[i];
		if (s->name[0] && !s->spectator && s->frags > max)
		{
			//skip it if an earlier seat is watching it already
			for (spv = pv-1; spv >= cl.playerview && spv < &cl.playerview[cl.splitclients]; spv--)
			{
				if (Cam_TrackNum(spv) == i)
					break;
			}
			if (!(spv >= cl.playerview && spv < &cl.playerview[cl.splitclients]))
			{
				max = s->frags;
				j = i;
			}
		}
	}
	if (j >= 0)
	{
		if (!pv->cam_locked || cl.players[j].frags > cl.players[pv->cam_spec_track].frags)
		{
			Cam_Lock(pv, j);
			//un-lock any higher seats watching our new target. this keeps things ordered.
			for (spv = pv+1; spv >= cl.playerview && spv < &cl.playerview[cl.splitclients]; spv++)
			{
				if (Cam_TrackNum(spv) == j)
					spv->cam_locked = false;
			}
		}
	}
	else
		Cam_Unlock(pv);
}

void Cam_SelfTrack(playerview_t *pv)
{
	vec3_t vec;
	if (!cl.worldmodel || cl.worldmodel->loadstate != MLS_LOADED)
		return;

	if (selfcam == 1)
	{	//view-from-eyes
	}
	else
	{
		if (selfcam == 2)
		{	//fixme:
			vec3_t forward, right, up;
			trace_t tr;
			AngleVectors(r_refdef.viewangles, forward, right, up);
			VectorMA(pv->simorg, -128, forward, pv->cam_desired_position);
			tr = Cam_DoTrace(pv->simorg, pv->cam_desired_position);
			VectorCopy(tr.endpos, pv->cam_desired_position);
		}
		else
		{	//view from a random wall
			if (!pv->cam_locked || !Cam_IsVisible(pv->simorg, pv->cam_desired_position))
			{
				if (!pv->cam_locked || realtime - pv->cam_lastviewtime > 0.1)
				{
					if (!InitFlyby(pv, pv->cam_desired_position, pv->simorg, pv->simangles, true))
						InitFlyby(pv, pv->cam_desired_position, pv->simorg, pv->simangles, false);
					pv->cam_lastviewtime = realtime;
				}
			}
			else
			{
				pv->cam_lastviewtime = realtime;
			}

			//tracking failed.
			if (!pv->cam_locked)
				return;
		}


		// move there locally immediately
		VectorCopy(pv->cam_desired_position, r_refdef.vieworg);

		VectorSubtract(pv->simorg, pv->cam_desired_position, vec);
		VectorAngles(vec, NULL, r_refdef.viewangles);
		r_refdef.viewangles[0] = -r_refdef.viewangles[0];
	}
}

// ZOID
//
// Take over the user controls and track a player.
// We find a nice position to watch the player and move there
void Cam_Track(playerview_t *pv, usercmd_t *cmd)
{
	player_state_t *player, *self;
	inframe_t *frame;
	vec3_t vec;
	float len;

	if (!cl.spectator || !cl.worldmodel)	//can happen when the server changes level
		return;
	
	if (cl_hightrack.value && !pv->cam_locked)
		Cam_CheckHighTarget(pv);

	if (!pv->cam_auto || cls.state != ca_active || cl.worldmodel || cl.worldmodel->loadstate != MLS_LOADED)
		return;

	if (pv->cam_locked && (!cl.players[pv->cam_spec_track].name[0] || cl.players[pv->cam_spec_track].spectator))
	{
		pv->cam_locked = false;
		if (cl_hightrack.value)
			Cam_CheckHighTarget(pv);
		else
			Cam_Unlock(pv);
		return;
	}

	frame = &cl.inframes[cl.validsequence & UPDATE_MASK];
	player = frame->playerstate + pv->cam_spec_track;
	self = frame->playerstate + pv->playernum;

	if (!pv->cam_locked || !Cam_IsVisible(player->origin, pv->cam_desired_position))
	{
		if (!pv->cam_locked || realtime - pv->cam_lastviewtime > 0.1)
		{
			if (!InitFlyby(pv, self->origin, player->origin, player->viewangles, true))
				InitFlyby(pv, self->origin, player->origin, player->viewangles, false);
			pv->cam_lastviewtime = realtime;
		}
	}
	else
	{
		pv->cam_lastviewtime = realtime;
	}

	//tracking failed.
	if (!pv->cam_locked || !pv->cam_auto)
		return;


	if (cl_chasecam.value || scr_chatmode == 2)
	{
		float *neworg;
//		float *newang;
		if (pv->nolocalplayer)
			neworg = cl.lerpents[pv->viewentity].origin;
		else
			neworg = player->origin;

		if (scr_chatmode != 2)
			pv->cam_lastviewtime = realtime;

//		VectorCopy(newang, pv->viewangles);
		if (memcmp(neworg, &self->origin, sizeof(vec3_t)) != 0)
		{
			if (!cls.demoplayback)
			{
				MSG_WriteByte (&cls.netchan.message, clc_tmove);
				MSG_WriteCoord (&cls.netchan.message, neworg[0]);
				MSG_WriteCoord (&cls.netchan.message, neworg[1]);
				MSG_WriteCoord (&cls.netchan.message, neworg[2]);
			}
			// move there locally immediately
			VectorCopy(neworg, self->origin);
		}
		self->weaponframe = player->weaponframe;

		return;
	}
	
	// Ok, move to our desired position and set our angles to view
	// the player
	VectorSubtract(pv->cam_desired_position, self->origin, vec);
	len = vlen(vec);
	cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;
	if (len > 16)
	{ // close enough?
		MSG_WriteByte (&cls.netchan.message, clc_tmove);
		MSG_WriteCoord (&cls.netchan.message, pv->cam_desired_position[0]);
		MSG_WriteCoord (&cls.netchan.message, pv->cam_desired_position[1]);
		MSG_WriteCoord (&cls.netchan.message, pv->cam_desired_position[2]);
	}

	// move there locally immediately
	VectorCopy(pv->cam_desired_position, self->origin);

	VectorSubtract(player->origin, pv->cam_desired_position, vec);
	VectorAngles(vec, NULL, pv->viewangles);
	pv->viewangles[0] = -pv->viewangles[0];
}

void Cam_SetAutoTrack(int userid)
{	//this is a hint from the server about who to track

}

void Cam_TrackCrosshairedPlayer(playerview_t *pv)
{
	inframe_t *frame;
	player_state_t *player;
	int i;
	float dot = 0.1, bestdot=0;
	int best = -1;
	vec3_t selforg;
	vec3_t dir;

	frame = &cl.inframes[cl.validsequence & UPDATE_MASK];
	player = frame->playerstate + pv->playernum;
	VectorCopy(player->origin, selforg);

	for (i = 0; i < cl.allocated_client_slots; i++)
	{
		player = frame->playerstate + i;
		VectorSubtract(player->origin, selforg, dir);
		VectorNormalize(dir);
		dot = DotProduct(vpn, dir);
		if (dot > bestdot)
		{
			bestdot = dot;
			best = i;
		}
	}
	Con_Printf("Track %i? %f\n", best, bestdot);
	if (best != -1)	//did we actually get someone?
	{
		pv->cam_auto++;
		Cam_Lock(pv, best);
	}
}

void Cam_FinishMove(playerview_t *pv, usercmd_t *cmd)
{
	int i;
	player_info_t	*s;
	int end;
	extern cvar_t cl_demospeed, cl_splitscreen;

	if (cls.state != ca_active)
		return;

	if (!cl.spectator && (cls.demoplayback != DPB_MVD && cls.demoplayback != DPB_EZTV)) // only in spectator mode
		return;

	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
	{
		int nb;
		nb = (cmd->sidemove<0)?4:0;
		nb |= (cmd->sidemove>0)?8:0;
		nb |= (cmd->forwardmove<0)?16:0;
		nb |= (cmd->forwardmove>0)?32:0;
		nb |= (cmd->upmove<0)?64:0;
		nb |= (cmd->upmove>0)?128:0;
		if (Cam_TrackNum(pv) >= 0)
		{
			if (nb & (nb ^ pv->cam_oldbuttons) & 4)
				Cvar_SetValue(&cl_demospeed, max(cl_demospeed.value - 0.1, 0));
			if (nb & (nb ^ pv->cam_oldbuttons) & 8)
				Cvar_SetValue(&cl_demospeed, min(cl_demospeed.value + 0.1, 10));
			if (nb & (nb ^ pv->cam_oldbuttons) & (4|8))
				Con_Printf("playback speed: %g%%\n", cl_demospeed.value*100);
			if (nb & (nb ^ pv->cam_oldbuttons) & 16)
				Cbuf_AddText("demo_jump +10", RESTRICT_LOCAL);
			if (nb & (nb ^ pv->cam_oldbuttons) & 32)
				Cbuf_AddText("demo_jump -10", RESTRICT_LOCAL);
			if (nb & (nb ^ pv->cam_oldbuttons) & (4|8))
				Con_Printf("playback speed: %g%%\n", cl_demospeed.value*100);
			if (nb & (nb ^ pv->cam_oldbuttons) & 64)
				Cvar_SetValue(&cl_splitscreen, max(cl_splitscreen.ival - 1, 0));
			if (nb & (nb ^ pv->cam_oldbuttons) & 128)
				Cvar_SetValue(&cl_splitscreen, min(cl_splitscreen.ival + 1, MAX_SPLITS-1));
		}
		pv->cam_oldbuttons = (pv->cam_oldbuttons & 3) | (nb & ~3);
		if (cmd->impulse)
		{
			int pl = cmd->impulse;
			for (i = 0; ; i++)
			{
				if (i == MAX_CLIENTS)
				{
					if (pl == cmd->impulse)
						break;
					i = 0;
				}

				s = &cl.players[i];
				if (s->name[0] && !s->spectator)
				{
					pl--;
					if (!pl)
					{
						Cam_Lock(pv, i);
						if (pv >= &cl.playerview[0] && pv < &cl.playerview[cl.splitclients])
						{
							pl = 1;
							pv++;
						}
						else
							break;
					}
				}
			}
			return;
		}
	}

	if (cmd->buttons & BUTTON_ATTACK) 
	{
		if (!(pv->cam_oldbuttons & BUTTON_ATTACK)) 
		{
			pv->cam_oldbuttons |= BUTTON_ATTACK;
			pv->cam_auto++;

			if (pv->cam_auto > CAM_TRACK) 
			{
				Cam_Unlock(pv);
				VectorCopy(pv->viewangles, cmd->angles);
				return;
			}
		}
		else
			return;
	}
	else
	{
		pv->cam_oldbuttons &= ~BUTTON_ATTACK;
		if (!pv->cam_auto)
		{
			if ((cmd->buttons & BUTTON_JUMP) && !(pv->cam_oldbuttons & BUTTON_JUMP))
				Cam_TrackCrosshairedPlayer(pv);
			pv->cam_oldbuttons = (pv->cam_oldbuttons&~BUTTON_JUMP) | (cmd->buttons & BUTTON_JUMP);
			return;
		}
	}

	if (pv->cam_auto && cl_hightrack.ival) 
	{
		Cam_CheckHighTarget(pv);
		return;
	}

	if (pv->cam_locked) 
	{
		if ((cmd->buttons & BUTTON_JUMP) && (pv->cam_oldbuttons & BUTTON_JUMP))
			return;		// don't pogo stick

		if (!(cmd->buttons & BUTTON_JUMP)) 
		{
			pv->cam_oldbuttons &= ~BUTTON_JUMP;
			return;
		}
		pv->cam_oldbuttons |= BUTTON_JUMP;	// don't jump again until released
	}

//	Con_Printf("Selecting track target...\n");

	if (pv->cam_locked && pv->cam_auto)
		end = (pv->cam_spec_track + 1) % MAX_CLIENTS;
	else
		end = pv->cam_spec_track;
	i = end;
	do 
	{
		s = &cl.players[i];
		if (s->name[0] && !s->spectator) 
		{
			Cam_Lock(pv, i);
			return;
		}
		i = (i + 1) % cl.allocated_client_slots;
	} while (i != end);
	// stay on same guy?
	i = pv->cam_spec_track;
	s = &cl.players[i];
	if (s->name[0] && !s->spectator) 
	{
		Cam_Lock(pv, i);
		return;
	}
	Con_Printf("No target found ...\n");
	pv->cam_auto = pv->cam_locked = false;
}

void Cam_Reset(void)
{
	int pnum;
	for (pnum = 0; pnum < MAX_SPLITS; pnum++)
	{
		playerview_t *pv = &cl.playerview[pnum];
		pv->cam_auto = CAM_NONE;
		pv->cam_spec_track = 0;
	}
}

void Cam_TrackPlayer(int seat, char *cmdname, char *plrarg)
{
	playerview_t *pv = &cl.playerview[seat];
	int slot;
	player_info_t	*s;

	if (seat >= MAX_SPLITS)
		return;

	if (cls.state <= ca_connected)
	{
		Con_Printf("Not connected.\n");
		return;
	}

	if (!cl.spectator)
	{
		Con_Printf("Not spectating.\n");
		return;
	}

	if (!Q_strcasecmp(plrarg, "off"))
	{
		Cam_Unlock(pv);
		return;
	}

	// search nicks first
	for (slot = 0; slot < cl.allocated_client_slots; slot++)
	{
		s = &cl.players[slot];
		if (s->name[0] && !s->spectator && !Q_strcasecmp(s->name, plrarg))
			break;
	}

	if (slot == cl.allocated_client_slots)
	{
		// didn't find nick, so search userids
		int userid;
		char *c;

		// check if given arg is in fact a number
		c = plrarg;
		while (*c)
		{
			if (*c < '0' || *c > '9')
			{
				Con_Printf("Couldn't find nick %s\n", plrarg);
				return;
			}
			c++;
		}

		userid = atoi(plrarg);

		for (slot = 0; slot < cl.allocated_client_slots; slot++)
		{
			s = &cl.players[slot];
			if (s->name[0] && !s->spectator && s->userid == userid)
				break;
		}

		if (slot == cl.allocated_client_slots)
		{
			Con_Printf("Couldn't find userid %i\n", userid);
			return;
		}
	}

	pv->cam_auto = CAM_TRACK;
	Cam_Lock(pv, slot);
	//and force the lock here and now
	pv->cam_locked = true;
	pv->viewentity = slot+1;
}

void Cam_Track_f(void)
{
	int i, j;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("Usage: %s userid|nick|off\n", Cmd_Argv(0));
		return;
	}

	i = 1;
	j = Cmd_Argc() - 1;
	if (j > MAX_SPLITS)
		j = MAX_SPLITS;

	while (j > 0)
	{
		Cam_TrackPlayer(i - 1, Cmd_Argv(0), Cmd_Argv(i));
		i++;
		j--;
	}
}

void Cam_Track1_f(void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Printf("Usage: %s userid|nick|off\n", Cmd_Argv(0));
		return;
	}

	Cam_TrackPlayer(0, Cmd_Argv(0), Cmd_Argv(1));
}

void Cam_Track2_f(void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Printf("Usage: %s userid|nick|off\n", Cmd_Argv(0));
		return;
	}

	Cam_TrackPlayer(1, Cmd_Argv(0), Cmd_Argv(1));
}

void Cam_Track3_f(void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Printf("Usage: %s userid|nick|off\n", Cmd_Argv(0));
		return;
	}

	Cam_TrackPlayer(2, Cmd_Argv(0), Cmd_Argv(1));
}

void Cam_Track4_f(void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Printf("Usage: %s userid|nick|off\n", Cmd_Argv(0));
		return;
	}

	Cam_TrackPlayer(3, Cmd_Argv(0), Cmd_Argv(1));
}

void CL_InitCam(void)
{
	Cvar_Register (&cl_hightrack, cl_spectatorgroup);
	Cvar_Register (&cl_chasecam, cl_spectatorgroup);
//	Cvar_Register (&cl_camera_maxpitch, cl_spectatorgroup);
//	Cvar_Register (&cl_camera_maxyaw, cl_spectatorgroup);
//	Cvar_Register (&cl_selfcam, cl_spectatorgroup);

	Cmd_AddCommand("track", Cam_Track_f);
	Cmd_AddCommand("track1", Cam_Track1_f);
	Cmd_AddCommand("track2", Cam_Track2_f);
	Cmd_AddCommand("track3", Cam_Track3_f);
	Cmd_AddCommand("track4", Cam_Track4_f);
}


