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

static vec3_t desired_position[MAX_SPLITS]; // where the camera wants to be
static qboolean locked[MAX_SPLITS];
static int oldbuttons[MAX_SPLITS];

char cl_spectatorgroup[] = "Spectator Tracking";

// track high fragger
cvar_t cl_hightrack = {"cl_hightrack", "0" };

cvar_t cl_chasecam = {"cl_chasecam", "1"};

//cvar_t cl_camera_maxpitch = {"cl_camera_maxpitch", "10" };
//cvar_t cl_camera_maxyaw = {"cl_camera_maxyaw", "30" };

vec3_t cam_viewangles[MAX_SPLITS];
double cam_lastviewtime[MAX_SPLITS];

int spec_track[MAX_SPLITS]; // player# of who we are tracking
int autocam[MAX_SPLITS];

void vectoangles(vec3_t vec, vec3_t ang)
{
	float	forward;
	float	yaw, pitch;
	
	if (vec[1] == 0 && vec[0] == 0)
	{
		yaw = 0;
		if (vec[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (int) (atan2(vec[1], vec[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (vec[0]*vec[0] + vec[1]*vec[1]);
		pitch = (int) (atan2(vec[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	ang[0] = pitch;
	ang[1] = yaw;
	ang[2] = 0;
}

static float vlen(vec3_t v)
{
	return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

// returns true if weapon model should be drawn in camera mode
qboolean Cam_DrawViewModel(int pnum)
{
	if (!cl.spectator)
		return true;

	if (autocam[pnum] && locked[pnum] && cl_chasecam.value)
		return true;
	return false;
}

// returns true if we should draw this player, we don't if we are chase camming
qboolean Cam_DrawPlayer(int pnum, int playernum)
{
//	if (playernum == cl.playernum[pnum])
//		return false;
	if (cl.spectator && autocam[pnum] && locked[pnum] && cl_chasecam.value && 
		spec_track[pnum] == playernum)
		return false;
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
		CL_SendClientCommand("ptrack");
		autocam[pnum] = CAM_NONE;
		locked[pnum] = false;
		Sbar_Changed();
	}
}

void Cam_Lock(int pnum, int playernum)
{

	cam_lastviewtime[pnum] = -1000;

	CL_SendClientCommand("ptrack %i", playernum);

	spec_track[pnum] = playernum;
	locked[pnum] = false;
	
	Skin_FlushPlayers();

	if (cls.demoplayback == DPB_MVD)
	{
		memcpy(&cl.stats[pnum], cl.players[playernum].stats, sizeof(cl.stats[pnum]));
	}

	Sbar_Changed();
}

trace_t Cam_DoTrace(vec3_t vec1, vec3_t vec2)
{
#if 0
	memset(&pmove, 0, sizeof(pmove));

	pmove.numphysent = 1;
	VectorCopy (vec3_origin, pmove.physents[0].origin);
	pmove.physents[0].model = cl.worldmodel;
#endif

	VectorCopy (vec1, pmove.origin);
	return PM_PlayerTrace(pmove.origin, vec2);
}

extern vec3_t	player_mins;
extern vec3_t	player_maxs;
	
// Returns distance or 9999 if invalid for some reason
static float Cam_TryFlyby(player_state_t *self, player_state_t *player, vec3_t vec, qboolean checkvis)
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
	VectorMA(player->origin, 800, vec, v);
	// v is endpos
	// fake a player move
	trace = Cam_DoTrace(player->origin, v);
	if (/*trace.inopen ||*/ trace.inwater)
		return 9999;
	VectorCopy(trace.endpos, vec);
	VectorSubtract(trace.endpos, player->origin, v);
	len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
	if (len < 32 || len > 800)
		return 9999;
	if (checkvis)
	{
		VectorSubtract(trace.endpos, self->origin, v);
		len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

		trace = Cam_DoTrace(self->origin, vec);
		if (trace.fraction != 1 || trace.inwater)
			return 9999;
	}
	return len;
}

// Is player visible?
static qboolean Cam_IsVisible(player_state_t *player, vec3_t vec)
{
	trace_t trace;
	vec3_t v;
	float d;

	trace = Cam_DoTrace(player->origin, vec);
	if (trace.fraction != 1 || /*trace.inopen ||*/ trace.inwater)
		return false;
	// check distance, don't let the player get too far away or too close
	VectorSubtract(player->origin, vec, v);
	d = vlen(v);
	if (d < 16)
		return false;
	return true;
}

static qboolean InitFlyby(int pnum, player_state_t *self, player_state_t *player, int checkvis) 
{
    float f, max;
    vec3_t vec, vec2;
	vec3_t forward, right, up;

	VectorCopy(player->viewangles, vec);
    vec[0] = 0;
	AngleVectors (vec, forward, right, up);
//	for (i = 0; i < 3; i++)
//		forward[i] *= 3;

    max = 1000;
	VectorAdd(forward, up, vec2);
	VectorAdd(vec2, right, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorAdd(forward, up, vec2);
	VectorSubtract(vec2, right, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorAdd(forward, right, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorSubtract(forward, right, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorAdd(forward, up, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorSubtract(forward, up, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorAdd(up, right, vec2);
	VectorSubtract(vec2, forward, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorSubtract(up, right, vec2);
	VectorSubtract(vec2, forward, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	// invert
	VectorSubtract(vec3_origin, forward, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorCopy(forward, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	// invert
	VectorSubtract(vec3_origin, right, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
	{
        max = f;
		VectorCopy(vec2, vec);
    }
	VectorCopy(right, vec2);
    if ((f = Cam_TryFlyby(self, player, vec2, checkvis)) < max)
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

	j = -1;
	for (i = 0, max = -9999; i < MAX_CLIENTS; i++)
	{
		s = &cl.players[i];
		if (s->name[0] && !s->spectator && s->frags > max)
		{
			max = s->frags;
			j = i;
		}
	}
	if (j >= 0)
	{
		if (!locked[pnum] || cl.players[j].frags > cl.players[spec_track[pnum]].frags)
			Cam_Lock(pnum, j);
	}
	else
		Cam_Unlock(pnum);
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

	if (!locked[pnum] || !Cam_IsVisible(player, desired_position[pnum]))
	{
		if (!locked[pnum] || realtime - cam_lastviewtime[pnum] > 0.1)
		{
			if (!InitFlyby(pnum, self, player, true))
				InitFlyby(pnum, self, player, false);
			cam_lastviewtime[pnum] = realtime;
		}
	}
	else
		cam_lastviewtime[pnum] = realtime;
	
	// couldn't track for some reason
	if (!locked[pnum] || !autocam[pnum])
		return;

	if (cl_chasecam.value)
	{
		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;

		VectorCopy(player->viewangles, cl.viewangles[pnum]);
		VectorCopy(player->origin, desired_position[pnum]);
		if (memcmp(&desired_position[pnum], &self->origin, sizeof(desired_position[pnum])) != 0)
		{
			MSG_WriteByte (&cls.netchan.message, clc_tmove);
			MSG_WriteCoord (&cls.netchan.message, desired_position[pnum][0]);
			MSG_WriteCoord (&cls.netchan.message, desired_position[pnum][1]);
			MSG_WriteCoord (&cls.netchan.message, desired_position[pnum][2]);
			// move there locally immediately
			VectorCopy(desired_position[pnum], self->origin);
		}
		self->weaponframe = player->weaponframe;

	}
	else
	{
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
}

void Cam_FinishMove(int pnum, usercmd_t *cmd)
{
	int i;
	player_info_t	*s;
	int end;

	if (cls.state != ca_active)
		return;

	if (!cl.spectator && cls.demoplayback != DPB_MVD) // only in spectator mode
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
			return;
	}

	if (autocam[pnum] && cl_hightrack.value) 
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

void CL_InitCam(void)
{
	Cvar_Register (&cl_hightrack, cl_spectatorgroup);
	Cvar_Register (&cl_chasecam, cl_spectatorgroup);
//	Cvar_Register (&cl_camera_maxpitch, cl_spectatorgroup);
//	Cvar_Register (&cl_camera_maxyaw, cl_spectatorgroup);
}


