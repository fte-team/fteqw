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

#include <ctype.h>

#include "quakedef.h"
#include "winquake.h"

#define	PM_SPECTATORMAXSPEED	500
#define	PM_STOPSPEED	100
#define	PM_MAXSPEED			320
#define BUTTON_JUMP 2
#define BUTTON_ATTACK 1
#define MAX_ANGLE_TURN 10

vec3_t desired_position[MAX_SPLITS]; // where the camera wants to be
static qboolean locked[MAX_SPLITS];
static int oldbuttons[MAX_SPLITS];

char cl_spectatorgroup[] = "Spectator Tracking";

// track high fragger
cvar_t cl_hightrack = SCVAR("cl_hightrack", "0");

cvar_t cl_chasecam = SCVAR("cl_chasecam", "1");
cvar_t cl_selfcam = SCVAR("cl_selfcam", "1");

//cvar_t cl_camera_maxpitch = {"cl_camera_maxpitch", "10" };
//cvar_t cl_camera_maxyaw = {"cl_camera_maxyaw", "30" };

vec3_t cam_viewangles[MAX_SPLITS];
double cam_lastviewtime[MAX_SPLITS];

int spec_track[MAX_SPLITS]; // player# of who we are tracking
int autocam[MAX_SPLITS];

int selfcam=1;

static float vlen(vec3_t v)
{
	return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

// returns true if weapon model should be drawn in camera mode
qboolean Cam_DrawViewModel(int pnum)
{
	if (cl.spectator)
	{
		if (autocam[pnum] && locked[pnum] && cl_chasecam.ival)
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

// returns true if we should draw this player, we don't if we are chase camming
qboolean Cam_DrawPlayer(int pnum, int playernum)
{
//	if (playernum == cl.playernum[pnum])
//		return false;
	if (cl.spectator)
	{
		if (autocam[pnum] && locked[pnum] && (cl_chasecam.value||scr_chatmode==2) && 
			spec_track[pnum] == playernum && r_secondaryview != 2)
			return false;
	}
	else
	{
		if (selfcam == 1 && !r_refdef.externalview)
			if (playernum == (cl.viewentity[pnum]?cl.viewentity[pnum]-1:(cl.playernum[pnum])))
				return false;
	}
	return true;
}

int Cam_TrackNum(int pnum)
{
	if (!autocam[pnum])
		return -1;
	return spec_track[pnum];
}

void Cam_Unlock(int pnum)
{
	if (autocam[pnum])
	{
		CL_SendClientCommand(true, "ptrack");
		autocam[pnum] = CAM_NONE;
		locked[pnum] = false;
		Sbar_Changed();
	}
}

void Cam_Lock(int pnum, int playernum)
{
	int i;

	cam_lastviewtime[pnum] = -1000;

	CL_SendClientCommand(true, "ptrack %i", playernum);

	spec_track[pnum] = playernum;
	locked[pnum] = false;
	
	Skin_FlushPlayers();

	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
	{
		memcpy(&cl.stats[pnum], cl.players[playernum].stats, sizeof(cl.stats[pnum]));
	}

	Sbar_Changed();

	for (i = 0; i < MAX_CLIENTS; i++)
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
	return PM_PlayerTrace(pmove.origin, vec2);
}

extern vec3_t	player_mins;
extern vec3_t	player_maxs;
	
// Returns distance or 9999 if invalid for some reason
static float Cam_TryFlyby(vec3_t selforigin, vec3_t playerorigin, vec3_t vec, qboolean checkvis)
{
	vec3_t v;
	trace_t trace;
	float len;

	player_mins[0] = player_mins[1] = -16;
	player_mins[2] = -24;
	player_maxs[0] = player_maxs[1] = 16;
	player_maxs[2] = 32;

	vectoangles(vec, v);
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

static qboolean InitFlyby(int pnum, vec3_t selforigin, vec3_t playerorigin, vec3_t playerviewangles, int checkvis) 
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

	locked[pnum] = true;
	VectorCopy(vec, desired_position[pnum]); 
	return true;
}

static void Cam_CheckHighTarget(int pnum)
{
	int i, j, max;
	player_info_t	*s;
	int sp;

	j = -1;
	for (i = 0, max = -9999; i < MAX_CLIENTS; i++)
	{
		s = &cl.players[i];
		if (s->name[0] && !s->spectator && s->frags > max)
		{
			for (sp = pnum-1; sp >= 0; sp--)
			{
				if (Cam_TrackNum(sp) == i)
					break;
			}
			if (sp == -1)
			{
				max = s->frags;
				j = i;
			}
		}
	}
	if (j >= 0)
	{
		if (!locked[pnum] || cl.players[j].frags > cl.players[spec_track[pnum]].frags)
		{
			Cam_Lock(pnum, j);
			for (sp = pnum+1; sp < cl.splitclients; sp++)
			{
				if (Cam_TrackNum(sp) == j)
					locked[sp] = false;
			}
		}
	}
	else
		Cam_Unlock(pnum);
}

void Cam_SelfTrack(int pnum)
{
	vec3_t vec;
	if (!cl.worldmodel || cl.worldmodel->needload)
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
			VectorMA(cl.simorg[pnum], -128, forward, desired_position[pnum]);
			tr = Cam_DoTrace(cl.simorg[pnum], desired_position[pnum]);
			VectorCopy(tr.endpos, desired_position[pnum]);
		}
		else
		{	//view from a random wall
			if (!locked[pnum] || !Cam_IsVisible(cl.simorg[pnum], desired_position[pnum]))
			{
				if (!locked[pnum] || realtime - cam_lastviewtime[pnum] > 0.1)
				{
					if (!InitFlyby(pnum, desired_position[pnum], cl.simorg[pnum], cl.simangles[pnum], true))
						InitFlyby(pnum, desired_position[pnum], cl.simorg[pnum], cl.simangles[pnum], false);
					cam_lastviewtime[pnum] = realtime;
				}
			}
			else
			{
				cam_lastviewtime[pnum] = realtime;
			}

			//tracking failed.
			if (!locked[pnum])
				return;
		}


		// move there locally immediately
		VectorCopy(desired_position[pnum], r_refdef.vieworg);

		VectorSubtract(cl.simorg[pnum], desired_position[pnum], vec);
		vectoangles(vec, r_refdef.viewangles);
		r_refdef.viewangles[0] = -r_refdef.viewangles[0];
	}
}

// ZOID
//
// Take over the user controls and track a player.
// We find a nice position to watch the player and move there
void Cam_Track(int pnum, usercmd_t *cmd)
{
	player_state_t *player, *self;
	frame_t *frame;
	vec3_t vec;
	float len;

	if (!cl.spectator || !cl.worldmodel)	//can happen when the server changes level
		return;
	
	if (cl_hightrack.value && !locked[pnum])
		Cam_CheckHighTarget(pnum);

	if (!autocam[pnum] || cls.state != ca_active)
		return;

	if (locked[pnum] && (!cl.players[spec_track[pnum]].name[0] || cl.players[spec_track[pnum]].spectator))
	{
		locked[pnum] = false;
		if (cl_hightrack.value)
			Cam_CheckHighTarget(pnum);
		else
			Cam_Unlock(pnum);
		return;
	}

	frame = &cl.frames[cl.validsequence & UPDATE_MASK];
	player = frame->playerstate + spec_track[pnum];
	self = frame->playerstate + cl.playernum[pnum];

	if (!locked[pnum] || !Cam_IsVisible(player->origin, desired_position[pnum]))
	{
		if (!locked[pnum] || realtime - cam_lastviewtime[pnum] > 0.1)
		{
			if (!InitFlyby(pnum, self->origin, player->origin, player->viewangles, true))
				InitFlyby(pnum, self->origin, player->origin, player->viewangles, false);
			cam_lastviewtime[pnum] = realtime;
		}
	}
	else
	{
		cam_lastviewtime[pnum] = realtime;
	}

	//tracking failed.
	if (!locked[pnum] || !autocam[pnum])
		return;


	if (cl_chasecam.value || scr_chatmode == 2)
	{
		if (scr_chatmode != 2)
			cam_lastviewtime[pnum] = realtime;

		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;

		VectorCopy(player->viewangles, cl.viewangles[pnum]);
		if (memcmp(player->origin, &self->origin, sizeof(player->origin)) != 0)
		{
			if (!cls.demoplayback)
			{
				MSG_WriteByte (&cls.netchan.message, clc_tmove);
				MSG_WriteCoord (&cls.netchan.message, player->origin[0]);
				MSG_WriteCoord (&cls.netchan.message, player->origin[1]);
				MSG_WriteCoord (&cls.netchan.message, player->origin[2]);
			}
			// move there locally immediately
			VectorCopy(player->origin, self->origin);
		}
		self->weaponframe = player->weaponframe;

		return;
	}
	
	// Ok, move to our desired position and set our angles to view
	// the player
	VectorSubtract(desired_position[pnum], self->origin, vec);
	len = vlen(vec);
	cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;
	if (len > 16)
	{ // close enough?
		MSG_WriteByte (&cls.netchan.message, clc_tmove);
		MSG_WriteCoord (&cls.netchan.message, desired_position[pnum][0]);
		MSG_WriteCoord (&cls.netchan.message, desired_position[pnum][1]);
		MSG_WriteCoord (&cls.netchan.message, desired_position[pnum][2]);
	}

	// move there locally immediately
	VectorCopy(desired_position[pnum], self->origin);

	VectorSubtract(player->origin, desired_position[pnum], vec);
	vectoangles(vec, cl.viewangles[pnum]);
	cl.viewangles[pnum][0] = -cl.viewangles[pnum][0];
}

void Cam_SetAutoTrack(int userid)
{	//this is a hint from the server about who to track

}

void Cam_TrackCrosshairedPlayer(int pnum)
{
	frame_t *frame;
	player_state_t *player;
	int i;
	float dot = 0.1, bestdot=0;
	int best = -1;
	vec3_t selforg;
	vec3_t dir;

	frame = &cl.frames[cl.validsequence & UPDATE_MASK];
	player = frame->playerstate + cl.playernum[pnum];
	VectorCopy(player->origin, selforg);

	for (i = 0; i < MAX_CLIENTS; i++)
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
		autocam[pnum]++;
		Cam_Lock(pnum, best);
	}
}

void Cam_FinishMove(int pnum, usercmd_t *cmd)
{
	int i;
	player_info_t	*s;
	int end;

	if (cls.state != ca_active)
		return;

	if (!cl.spectator && (cls.demoplayback != DPB_MVD && cls.demoplayback != DPB_EZTV)) // only in spectator mode
		return;

	if (cmd->buttons & BUTTON_ATTACK) 
	{
		if (!(oldbuttons[pnum] & BUTTON_ATTACK)) 
		{

			oldbuttons[pnum] |= BUTTON_ATTACK;
			autocam[pnum]++;

			if (autocam[pnum] > CAM_TRACK) 
			{
				Cam_Unlock(pnum);
				VectorCopy(cl.viewangles[pnum], cmd->angles);
				return;
			}
		}
		else
			return;
	}
	else
	{
		oldbuttons[pnum] &= ~BUTTON_ATTACK;
		if (!autocam[pnum])
		{
			if ((cmd->buttons & BUTTON_JUMP) && !(oldbuttons[pnum] & BUTTON_JUMP))
				Cam_TrackCrosshairedPlayer(pnum);
			oldbuttons[pnum] = (oldbuttons[pnum]&~BUTTON_JUMP) | (cmd->buttons & BUTTON_JUMP);
			return;
		}
	}

	if (autocam[pnum] && cl_hightrack.ival) 
	{
		Cam_CheckHighTarget(pnum);
		return;
	}

	if (locked[pnum]) 
	{
		if ((cmd->buttons & BUTTON_JUMP) && (oldbuttons[pnum] & BUTTON_JUMP))
			return;		// don't pogo stick

		if (!(cmd->buttons & BUTTON_JUMP)) 
		{
			oldbuttons[pnum] &= ~BUTTON_JUMP;
			return;
		}
		oldbuttons[pnum] |= BUTTON_JUMP;	// don't jump again until released
	}

//	Con_Printf("Selecting track target...\n");

	if (locked[pnum] && autocam[pnum])
		end = (spec_track[pnum] + 1) % MAX_CLIENTS;
	else
		end = spec_track[pnum];
	i = end;
	do 
	{
		s = &cl.players[i];
		if (s->name[0] && !s->spectator) 
		{
			Cam_Lock(pnum, i);
			return;
		}
		i = (i + 1) % MAX_CLIENTS;
	} while (i != end);
	// stay on same guy?
	i = spec_track[pnum];
	s = &cl.players[i];
	if (s->name[0] && !s->spectator) 
	{
		Cam_Lock(pnum, i);
		return;
	}
	Con_Printf("No target found ...\n");
	autocam[pnum] = locked[pnum] = false;
}

void Cam_Reset(void)
{
	int pnum;
	for (pnum = 0; pnum < MAX_SPLITS; pnum++)
	{
		autocam[pnum] = CAM_NONE;
		spec_track[pnum] = 0;
	}
}

void Cam_TrackPlayer(int pnum, char *cmdname, char *plrarg)
{
	int slot;
	player_info_t	*s;

	if (pnum >= MAX_SPLITS)
	{
		Con_Printf("This command is unavailable in this compilation.\n");
		return;
	}

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
		Cam_Unlock(pnum);
		return;
	}

	// search nicks first
	for (slot = 0; slot < MAX_CLIENTS; slot++)
	{
		s = &cl.players[slot];
		if (s->name[0] && !s->spectator && !Q_strcasecmp(s->name, plrarg))
			break;
	}

	if (slot == MAX_CLIENTS)
	{
		// didn't find nick, so search userids
		int userid;
		char *c;

		// check if given arg is in fact a number
		c = plrarg;
		while (*c)
		{
			if (!isdigit(*c))
			{
				Con_Printf("Couldn't find nick %s\n", plrarg);
				return;
			}
			c++;
		}

		userid = atoi(plrarg);

		for (slot = 0; slot < MAX_CLIENTS; slot++)
		{
			s = &cl.players[slot];
			if (s->name[0] && !s->spectator && s->userid == userid)
				break;
		}

		if (slot == MAX_CLIENTS)
		{
			Con_Printf("Couldn't find userid %i\n", userid);
			return;
		}
	}

	autocam[pnum] = CAM_TRACK;
	Cam_Lock(pnum, slot);
	locked[pnum] = true;
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


