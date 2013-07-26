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
// cl.input.c  -- builds an intended movement command to send to the server

#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"	//fps indep stuff.
#endif

float in_sensitivityscale = 1;

void CL_SpareMsec_Callback (struct cvar_s *var, char *oldvalue);

cvar_t	cl_nodelta = CVAR("cl_nodelta","0");

cvar_t	cl_c2spps = CVAR("cl_c2spps", "0");
cvar_t	cl_c2sImpulseBackup = SCVAR("cl_c2sImpulseBackup","3");
cvar_t	cl_netfps = CVAR("cl_netfps", "150");
cvar_t	cl_sparemsec = CVARC("cl_sparemsec", "10", CL_SpareMsec_Callback);
cvar_t  cl_queueimpulses = CVAR("cl_queueimpulses", "0");
cvar_t	cl_smartjump = CVAR("cl_smartjump", "1");

cvar_t	cl_prydoncursor = CVAR("cl_prydoncursor", "");	//for dp protocol
cvar_t	cl_instantrotate = CVARF("cl_instantrotate", "1", CVAR_SEMICHEAT);
cvar_t in_xflip = {"in_xflip", "0"};

cvar_t	prox_inmenu = CVAR("prox_inmenu", "0");

usercmd_t independantphysics[MAX_SPLITS];
vec3_t mousemovements[MAX_SPLITS];

/*kinda a hack...*/
static int		con_splitmodifier;
cvar_t	cl_forcesplitclient = CVAR("in_forcesplitclient", "0");
extern cvar_t cl_splitscreen;
int CL_TargettedSplit(qboolean nowrap)
{
	char *c;
	int pnum;
	int mod;
	if (nowrap)
		mod = MAX_SPLITS;
	else
		mod = cl.splitclients;
	if (mod < 1)
		return 0;
	c = Cmd_Argv(0);
	pnum = atoi(c+strlen(c)-1);
	if (pnum && !(c[1] == 'b'&&c[2] == 'u' && !atoi(c+strlen(c)-2)))
	{
		pnum--;
		return pnum;
	}

	if (con_splitmodifier > 0)
		return (con_splitmodifier - 1) % mod;
	else if (cl_forcesplitclient.ival > 0)
		return (cl_forcesplitclient.ival-1) % mod;
	else
		return 0;
}

void CL_Split_f(void)
{
	int tmp;
	char *c;
	c = Cmd_Argv(0);
	tmp = con_splitmodifier;
	if (*c == '+' || *c == '-')
	{
		con_splitmodifier = c[2];
		Cmd_ExecuteString(va("%c%s", *c, Cmd_Args()), Cmd_ExecLevel);
	}
	else
	{
		con_splitmodifier = c[1];
		Cmd_ExecuteString(Cmd_Args(), Cmd_ExecLevel);
	}
	con_splitmodifier = tmp;
}
void CL_SplitA_f(void)
{
	int tmp;
	char *c, *args;
	c = Cmd_Argv(0);
	args = COM_Parse(Cmd_Args());
	if (!args)
		return;
	while(*args == ' ' || *args == '\t')
		args++;
	tmp = con_splitmodifier;
	con_splitmodifier = atoi(com_token);
	if (*c == '+' || *c == '-')
		Cmd_ExecuteString(va("%c%s", *c, args), Cmd_ExecLevel);
	else
		Cmd_ExecuteString(args, Cmd_ExecLevel);
	con_splitmodifier = tmp;
}

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/


kbutton_t	in_mlook, in_klook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_jump, in_attack;
kbutton_t	in_up, in_down;

kbutton_t	in_button3, in_button4, in_button5, in_button6, in_button7, in_button8;

#define IN_IMPULSECACHE 256
int			in_impulse[MAX_SPLITS][IN_IMPULSECACHE];
int			in_nextimpulse[MAX_SPLITS];
int			in_impulsespending[MAX_SPLITS];

float		cursor_screen[2];
qboolean	cursor_active;





void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;

	int pnum = CL_TargettedSplit(false);
	
	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c)&255;
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[pnum][0] || k == b->down[pnum][1])
		return;		// repeating key
	
	if (!b->down[pnum][0])
		b->down[pnum][0] = k;
	else if (!b->down[pnum][1])
		b->down[pnum][1] = k;
	else
	{
		Con_Printf ("Three keys down for a button!\n");
		return;
	}
	
	if (b->state[pnum] & 1)
		return;		// still down
	b->state[pnum] |= 1 + 2;	// down + impulse down
}

void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;

	int pnum = CL_TargettedSplit(false);
	
	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c)&255;
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[pnum][0] = b->down[pnum][1] = 0;
		b->state[pnum] = 4;	// impulse up
		return;
	}

	if (b->down[pnum][0] == k)
		b->down[pnum][0] = 0;
	else if (b->down[pnum][1] == k)
		b->down[pnum][1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[pnum][0] || b->down[pnum][1])
		return;		// some other key is still holding it down

	if (!(b->state[pnum] & 1))
		return;		// still up (this should not happen)
	b->state[pnum] &= ~1;		// now up
	b->state[pnum] |= 4; 		// impulse up
}

void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_MLookDown (void) {KeyDown(&in_mlook);}
void IN_MLookUp (void)
{
	int pnum = CL_TargettedSplit(false);
	KeyUp(&in_mlook);
	if ( !(in_mlook.state[pnum]&1) &&  lookspring.ival)
		V_StartPitchDrift(&cl.playerview[pnum]);
}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ForwardDown(void) {KeyDown(&in_forward);}
void IN_ForwardUp(void) {KeyUp(&in_forward);}
void IN_BackDown(void) {KeyDown(&in_back);}
void IN_BackUp(void) {KeyUp(&in_back);}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {KeyDown(&in_moveright);}
void IN_MoverightUp(void) {KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

void IN_AttackDown(void) {KeyDown(&in_attack);}
void IN_AttackUp(void) {KeyUp(&in_attack);}

void IN_UseDown (void) {KeyDown(&in_use);}
void IN_UseUp (void) {KeyUp(&in_use);}
void IN_JumpDown (void)
{
	qboolean condition;


	int pnum = CL_TargettedSplit(false);
	


	condition = (cls.state == ca_active && cl_smartjump.ival && !prox_inmenu.ival);
#ifdef Q2CLIENT
	if (condition && cls.protocol == CP_QUAKE2)
		KeyDown(&in_up);
	else
#endif
		if (condition && cl.playerview[pnum].stats[STAT_HEALTH] > 0 && !cls.demoplayback && !cl.spectator && 
		cl.inframes[cl.validsequence&UPDATE_MASK].playerstate[cl.playerview[pnum].playernum].messagenum == cl.validsequence && cl.playerview[pnum].waterlevel >= 2 && (!cl.teamfortress || !(in_forward.state[pnum] & 1))
	)
		KeyDown(&in_up);
	else if (condition && cl.spectator && Cam_TrackNum(&cl.playerview[pnum]) == -1)
		KeyDown(&in_up);
	else
		KeyDown(&in_jump);
}
void IN_JumpUp (void)
{
	if (cl_smartjump.ival)
		KeyUp(&in_up);
	KeyUp(&in_jump);
}


void IN_Button3Down(void) {KeyDown(&in_button3);}
void IN_Button3Up(void) {KeyUp(&in_button3);}
void IN_Button4Down(void) {KeyDown(&in_button4);}
void IN_Button4Up(void) {KeyUp(&in_button4);}
void IN_Button5Down(void) {KeyDown(&in_button5);}
void IN_Button5Up(void) {KeyUp(&in_button5);}
void IN_Button6Down(void) {KeyDown(&in_button6);}
void IN_Button6Up(void) {KeyUp(&in_button6);}
void IN_Button7Down(void) {KeyDown(&in_button7);}
void IN_Button7Up(void) {KeyUp(&in_button7);}
void IN_Button8Down(void) {KeyDown(&in_button8);}
void IN_Button8Up(void) {KeyUp(&in_button8);}

float in_rotate;
void IN_Rotate_f (void) {in_rotate += atoi(Cmd_Argv(1));}


//is this useful?

//This function incorporates Tonik's impulse  8 7 6 5 4 3 2 1 to select the prefered weapon on the basis of having it.
//It also incorporates split screen input as well as impulse buffering
void IN_Impulse (void)
{
	int newimp;
	int best, i, imp, items;
	int pnum = CL_TargettedSplit(false);

	newimp = Q_atoi(Cmd_Argv(1));

	if (Cmd_Argc() > 2)
	{
		items = cl.playerview[pnum].stats[STAT_ITEMS];
		best = 0;

		for (i = Cmd_Argc() - 1; i > 0; i--)
		{
			imp = Q_atoi(Cmd_Argv(i));
			if (imp < 1 || imp > 8)
				continue;

			switch (imp)
			{
				case 1:
					if (items & IT_AXE)
						best = 1;
					break;
				case 2:
					if (items & IT_SHOTGUN && cl.playerview[pnum].stats[STAT_SHELLS] >= 1)
						best = 2;
					break;
				case 3:
					if (items & IT_SUPER_SHOTGUN && cl.playerview[pnum].stats[STAT_SHELLS] >= 2)
						best = 3;
					break;
				case 4:
					if (items & IT_NAILGUN && cl.playerview[pnum].stats[STAT_NAILS] >= 1)
						best = 4;
					break;
				case 5:
					if (items & IT_SUPER_NAILGUN && cl.playerview[pnum].stats[STAT_NAILS] >= 2)
						best = 5;
					break;
				case 6:
					if (items & IT_GRENADE_LAUNCHER && cl.playerview[pnum].stats[STAT_ROCKETS] >= 1)
						best = 6;
					break;
				case 7:
					if (items & IT_ROCKET_LAUNCHER && cl.playerview[pnum].stats[STAT_ROCKETS] >= 1)
						best = 7;
					break;
				case 8:
					if (items & IT_LIGHTNING && cl.playerview[pnum].stats[STAT_CELLS] >= 1)
						best = 8;
			}
		}

		if (best)
			newimp = best;
	}

	if (in_impulsespending[pnum]>=IN_IMPULSECACHE)
	{
		Con_Printf("Too many impulses, ignoring %i\n", newimp);
		return;
	}

	if (cl_queueimpulses.ival)
	{
		in_impulse[pnum][(in_nextimpulse[pnum]+in_impulsespending[pnum])%IN_IMPULSECACHE] = newimp;
		in_impulsespending[pnum]++;
	}
	else
	{
		in_impulse[pnum][(in_nextimpulse[pnum])%IN_IMPULSECACHE] = newimp;
		in_impulsespending[pnum]=1;
	}
}

void IN_Restart (void)
{
	IN_Shutdown();
	IN_ReInit();
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState (kbutton_t *key, int pnum)
{
	float		val;
	qboolean	impulsedown, impulseup, down;
	
	impulsedown = key->state[pnum] & 2;
	impulseup = key->state[pnum] & 4;
	down = key->state[pnum] & 1;
	val = 0;
	
	if (impulsedown && !impulseup)
	{
		if (down)
			val = 0.5;	// pressed and held this frame
		else
			val = 0;	//	I_Error ();
	}
	if (impulseup && !impulsedown)
	{
		if (down)
			val = 0;	//	I_Error ();
		else
			val = 0;	// released this frame
	}
	if (!impulsedown && !impulseup)
	{
		if (down)
			val = 1.0;	// held the entire frame
		else
			val = 0;	// up the entire frame
	}
	if (impulsedown && impulseup)
	{
		if (down)
			val = 0.75;	// released and re-pressed this frame
		else
			val = 0.25;	// pressed and released this frame
	}

	key->state[pnum] &= 1;		// clear impulses
	
	return val;
}

void CL_ProxyMenuHook(char *command, kbutton_t *key)
{
	if ((key->state[0] & 3) == 3)	//2 is impulse down, 1 is held down
	{
		key->state[0] = 0;		// clear impulses

		Cbuf_AddText(command, RESTRICT_DEFAULT);
	}
}

void CL_ProxyMenuHooks(void)
{
	if (!prox_inmenu.ival)
		return;

	CL_ProxyMenuHook("say proxy:menu down\n", &in_back);
	CL_ProxyMenuHook("say proxy:menu up\n", &in_forward);

	CL_ProxyMenuHook("say proxy:menu left\n", &in_left);
	CL_ProxyMenuHook("say proxy:menu right\n", &in_right);

	CL_ProxyMenuHook("say proxy:menu left\n", &in_moveleft);
	CL_ProxyMenuHook("say proxy:menu right\n", &in_moveright);

	CL_ProxyMenuHook("say proxy:menu use\n", &in_jump);
}


//==========================================================================

cvar_t	cl_upspeed = SCVARF("cl_upspeed","400", CVAR_ARCHIVE);
cvar_t	cl_forwardspeed = SCVARF("cl_forwardspeed","400", CVAR_ARCHIVE);
cvar_t	cl_backspeed = SCVARF("cl_backspeed","400", CVAR_ARCHIVE);
cvar_t	cl_sidespeed = SCVARF("cl_sidespeed","400", CVAR_ARCHIVE);

cvar_t	cl_movespeedkey = SCVAR("cl_movespeedkey","2.0");

cvar_t	cl_yawspeed = SCVAR("cl_yawspeed","140");
cvar_t	cl_pitchspeed = SCVAR("cl_pitchspeed","150");

cvar_t	cl_anglespeedkey = SCVAR("cl_anglespeedkey","1.5");

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles (int pnum, double frametime)
{
	float	speed, quant;
	float	up, down;
	
	if (in_speed.state[pnum] & 1)
	{
		if (ruleset_allow_frj.ival)
			speed = frametime * cl_anglespeedkey.ival;
		else
			speed = frametime * bound(-2, cl_anglespeedkey.ival, 2);
	}
	else
		speed = frametime;

	if (in_rotate && pnum==0 && !(cl.fpd & FPD_LIMIT_YAW))
	{
		quant = in_rotate;
		if (!cl_instantrotate.ival)
			quant *= speed;
		in_rotate -= quant;
		if (ruleset_allow_frj.ival)
			cl.playerview[pnum].viewanglechange[YAW] += quant;
	}

	if (!(in_strafe.state[pnum] & 1))
	{
		quant = cl_yawspeed.ival;
		if (cl.fpd & FPD_LIMIT_YAW || !ruleset_allow_frj.ival)
			quant = bound(-900, quant, 900);
		cl.playerview[pnum].viewanglechange[YAW] -= speed*quant * CL_KeyState (&in_right, pnum);
		cl.playerview[pnum].viewanglechange[YAW] += speed*quant * CL_KeyState (&in_left, pnum);
	}
	if (in_klook.state[pnum] & 1)
	{
		V_StopPitchDrift (&cl.playerview[pnum]);
		quant = cl_pitchspeed.ival;
		if (cl.fpd & FPD_LIMIT_PITCH || !ruleset_allow_frj.ival)
			quant = bound(-700, quant, 700);
		cl.playerview[pnum].viewanglechange[PITCH] -= speed*quant * CL_KeyState (&in_forward, pnum);
		cl.playerview[pnum].viewanglechange[PITCH] += speed*quant * CL_KeyState (&in_back, pnum);
	}
	
	up = CL_KeyState (&in_lookup, pnum);
	down = CL_KeyState(&in_lookdown, pnum);

	quant = cl_pitchspeed.ival;
	if (!ruleset_allow_frj.ival)
		quant = bound(-700, quant, 700);	
	cl.playerview[pnum].viewanglechange[PITCH] -= speed*cl_pitchspeed.ival * up;
	cl.playerview[pnum].viewanglechange[PITCH] += speed*cl_pitchspeed.ival * down;

	if (up || down)
		V_StopPitchDrift (&cl.playerview[pnum]);	
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove (usercmd_t *cmd, int pnum, float extra, float wantfps)
{
	float scale = 1;//extra/1000.0f * 1/wantfps;

//
// adjust for speed key
//
	if (in_speed.state[pnum] & 1)
		scale *= cl_movespeedkey.value;

	if (in_strafe.state[pnum] & 1)
	{
		cmd->sidemove += scale*cl_sidespeed.value * CL_KeyState (&in_right, pnum);
		cmd->sidemove -= scale*cl_sidespeed.value * CL_KeyState (&in_left, pnum);
	}

	cmd->sidemove += scale*cl_sidespeed.value * CL_KeyState (&in_moveright, pnum);
	cmd->sidemove -= scale*cl_sidespeed.value * CL_KeyState (&in_moveleft, pnum);

	if(in_xflip.ival) cmd->sidemove *= -1;

	cmd->upmove += scale*cl_upspeed.value * CL_KeyState (&in_up, pnum);
	cmd->upmove -= scale*cl_upspeed.value * CL_KeyState (&in_down, pnum);

	if (! (in_klook.state[pnum] & 1) )
	{	
		cmd->forwardmove += scale*cl_forwardspeed.value * CL_KeyState (&in_forward, pnum);
		cmd->forwardmove -= scale*cl_backspeed.value * CL_KeyState (&in_back, pnum);
	}
}

int MakeChar (int i)
{
	if (i < -127*4)
		i = -127*4;
	if (i > 127*4)
		i = 127*4;
	return i;
}

void CL_ClampPitch (int pnum)
{
	float mat[16];
	float roll;
	static float oldtime;
	float timestep = realtime - oldtime;
	oldtime = realtime;

	if (cl.intermission)
	{
		memset(cl.playerview[pnum].viewanglechange, 0, sizeof(cl.playerview[pnum].viewanglechange));
		return;
	}
#if 0
	if (cl.pmovetype[pnum] == PM_WALLWALK)
	{
		vec3_t impact;
		vec3_t norm;
		float mat2[16];
		vec3_t cross;
		vec3_t view[4];
		float dot;
		AngleVectors(cl.viewangles[pnum], view[0], view[1], view[2]);
		Matrix4x4_RM_FromVectors(mat, view[0], view[1], view[2], vec3_origin);

		Matrix4_Multiply(Matrix4x4_CM_NewRotation(-cl.viewanglechange[pnum][PITCH], 0, 1, 0), mat, mat2);
		Matrix4_Multiply(Matrix4x4_CM_NewRotation(cl.viewanglechange[pnum][YAW], 0, 0, 1), mat2, mat);

		Matrix3x4_RM_ToVectors(mat, view[0], view[1], view[2], view[3]);

		VectorMA(cl.simorg[pnum], -48, view[2], view[3]);
		if (!TraceLineN(cl.simorg[pnum], view[3], impact, norm))
		{
			norm[0] = 0;
			norm[1] = 0;
			norm[2] = 1;
		}

		/*keep the roll relative to the 'ground'*/
		CrossProduct(norm, view[2], cross);
		dot = DotProduct(view[0], cross);
		roll = timestep * 360 * -(dot);
		Matrix4_Multiply(Matrix4x4_CM_NewRotation(roll, 1, 0, 0), mat, mat2);

		Matrix3x4_RM_ToVectors(mat2, view[0], view[1], view[2], view[3]);
		VectorAngles(view[0], view[2], cl.viewangles[pnum]);
		cl.viewangles[pnum][PITCH]=360 - cl.viewangles[pnum][PITCH];
		VectorClear(cl.viewanglechange[pnum]);

		return;
	}
#endif
#if 1
	if ((cl.playerview[pnum].gravitydir[2] != -1 || cl.playerview[pnum].viewangles[2]))
	{
		float surfm[16], invsurfm[16];
		float viewm[16];
		vec3_t view[4];
		vec3_t surf[3];
		vec3_t vang;
		void PerpendicularVector( vec3_t dst, const vec3_t src );

		/*calc current view matrix relative to the surface*/
		AngleVectors(cl.playerview[pnum].viewangles, view[0], view[1], view[2]);
		VectorNegate(view[1], view[1]);

		/*calculate the surface axis with up from the pmove code and right/forwards relative to the player's directions*/
		if (!cl.playerview[pnum].gravitydir[0] && !cl.playerview[pnum].gravitydir[1] && !cl.playerview[pnum].gravitydir[2])
		{
			VectorSet(surf[2], 0, 0, 1);
		}
		else
		{
			VectorNegate(cl.playerview[pnum].gravitydir, surf[2]);
		}
		VectorNormalize(surf[2]);
		PerpendicularVector(surf[1], surf[2]);
		VectorNormalize(surf[1]);
		CrossProduct(surf[2], surf[1], surf[0]);
		VectorNegate(surf[0], surf[0]);
		VectorNormalize(surf[0]);
		Matrix4x4_RM_FromVectors(surfm, surf[0], surf[1], surf[2], vec3_origin);
		Matrix3x4_InvertTo4x4_Simple(surfm, invsurfm);

		/*calc current view matrix relative to the surface*/
		Matrix4x4_RM_FromVectors(viewm, view[0], view[1], view[2], vec3_origin);
		Matrix4_Multiply(viewm, invsurfm, mat);
		/*convert that back to angles*/
		Matrix3x4_RM_ToVectors(mat, view[0], view[1], view[2], view[3]);
		VectorAngles(view[0], view[2], vang);
		vang[PITCH] *= -1;

		/*edit it*/
		if (vang[PITCH] < -180)
			vang[PITCH] += 360;
		if (vang[PITCH] > 180)
			vang[PITCH] -= 360;
		if (vang[ROLL] > 180)
			vang[ROLL] -= 360;
		vang[PITCH] += cl.playerview[pnum].viewanglechange[PITCH];
		vang[YAW] += cl.playerview[pnum].viewanglechange[YAW];
		/*keep the player looking relative to their ground (smoothlyish)*/
		if (!vang[ROLL])
		{
			if (!cl.playerview[pnum].viewanglechange[PITCH] && !cl.playerview[pnum].viewanglechange[YAW] && !cl.playerview[pnum].viewanglechange[ROLL])
				return;
		}
		else
		{
			if (fabs(vang[ROLL]) < host_frametime*180)
				vang[ROLL] = 0;
			else if (vang[ROLL] > 0)
				vang[ROLL] -= host_frametime*180;
			else
				vang[ROLL] += host_frametime*180;
		}
		VectorClear(cl.playerview[pnum].viewanglechange);
		/*clamp pitch*/
		if (vang[PITCH] > cl.maxpitch)
			vang[PITCH] = cl.maxpitch;
		if (vang[PITCH] < cl.minpitch)
			vang[PITCH] = cl.minpitch;

		/*turn those angles back to a matrix*/
		AngleVectors(vang, view[0], view[1], view[2]);
		VectorNegate(view[1], view[1]);
		Matrix4x4_RM_FromVectors(mat, view[0], view[1], view[2], vec3_origin);
		/*rotate back into world space*/
		Matrix4_Multiply(mat, surfm, viewm);
		/*and figure out the final result*/
		Matrix3x4_RM_ToVectors(viewm, view[0], view[1], view[2], view[3]);
		VectorAngles(view[0], view[2], cl.playerview[pnum].viewangles);
		cl.playerview[pnum].viewangles[PITCH] *= -1;

		if (cl.playerview[pnum].viewangles[PITCH] < -180)
			cl.playerview[pnum].viewangles[PITCH] += 360;
		return;
	}
#endif
	cl.playerview[pnum].viewangles[PITCH] += cl.playerview[pnum].viewanglechange[PITCH];
	cl.playerview[pnum].viewangles[YAW] += cl.playerview[pnum].viewanglechange[YAW];
	cl.playerview[pnum].viewangles[ROLL] += cl.playerview[pnum].viewanglechange[ROLL];
	VectorClear(cl.playerview[pnum].viewanglechange);

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
	{
		float	pitch;
		pitch = SHORT2ANGLE(cl.q2frame.playerstate.pmove.delta_angles[PITCH]);
		if (pitch > 180)
			pitch -= 360;

		if (cl.playerview[pnum].viewangles[PITCH] + pitch < -360)
			cl.playerview[pnum].viewangles[PITCH] += 360; // wrapped
		if (cl.playerview[pnum].viewangles[PITCH] + pitch > 360)
			cl.playerview[pnum].viewangles[PITCH] -= 360; // wrapped

		if (cl.playerview[pnum].viewangles[PITCH] + pitch > cl.maxpitch)
			cl.playerview[pnum].viewangles[PITCH] = cl.maxpitch - pitch;
		if (cl.playerview[pnum].viewangles[PITCH] + pitch < cl.minpitch)
			cl.playerview[pnum].viewangles[PITCH] = cl.minpitch - pitch;
	}
	else
#endif
#ifdef Q3CLIENT
		if (cls.protocol == CP_QUAKE3)	//q3 expects the cgame to do it
	{
			//no-op
	}
	else
#endif
	{
		if (cl.playerview[pnum].viewangles[PITCH] > cl.maxpitch)
			cl.playerview[pnum].viewangles[PITCH] = cl.maxpitch;
		if (cl.playerview[pnum].viewangles[PITCH] < cl.minpitch)
			cl.playerview[pnum].viewangles[PITCH] = cl.minpitch;
	} 

//	if (cl.viewangles[pnum][ROLL] > 50)
//		cl.viewangles[pnum][ROLL] = 50;
//	if (cl.viewangles[pnum][ROLL] < -50)
//		cl.viewangles[pnum][ROLL] = -50;
	roll = timestep*cl.playerview[pnum].viewangles[ROLL]*30;
	if ((cl.playerview[pnum].viewangles[ROLL]-roll < 0) != (cl.playerview[pnum].viewangles[ROLL]<0))
		cl.playerview[pnum].viewangles[ROLL] = 0;
	else
		cl.playerview[pnum].viewangles[ROLL] -= timestep*cl.playerview[pnum].viewangles[ROLL]*3;
}

/*
==============
CL_FinishMove
==============
*/
void CL_FinishMove (usercmd_t *cmd, int msecs, int pnum)
{
	int	i;
	int bits;

	CL_ClampPitch(pnum);

//
// always dump the first two message, because it may contain leftover inputs
// from the last level
//
	if (cl.movesequence <= 2)
	{
		cmd->buttons = 0;
		return;
	}
//
// figure button bits
//

	bits = 0;
	if (in_attack .state[pnum] & 3)	bits |=   1; in_attack.state[pnum]	&= ~2;
	if (in_jump   .state[pnum] & 3)	bits |=   2; in_jump.state[pnum]	&= ~2;
	if (in_use    .state[pnum] & 3)	bits |=   4; in_use.state[pnum]		&= ~2;
	if (in_button3.state[pnum] & 3)	bits |=   4; in_button3.state[pnum] &= ~2;	//yup, flag 4 twice.
	if (in_button4.state[pnum] & 3)	bits |=   8; in_button4.state[pnum] &= ~2;
	if (in_button5.state[pnum] & 3)	bits |=  16; in_button5.state[pnum] &= ~2;
	if (in_button6.state[pnum] & 3)	bits |=  32; in_button6.state[pnum] &= ~2;
	if (in_button7.state[pnum] & 3)	bits |=  64; in_button7.state[pnum] &= ~2;
	if (in_button8.state[pnum] & 3)	bits |= 128; in_button8.state[pnum] &= ~2;
	cmd->buttons = bits;

	// send milliseconds of time to apply the move
	cmd->msec = msecs;

	for (i=0 ; i<3 ; i++)
		cmd->angles[i] = ((int)(cl.playerview[pnum].viewangles[i]*65536.0/360)&65535);

	if (in_impulsespending[pnum] && !cl.paused)
	{
		in_nextimpulse[pnum]++;
		in_impulsespending[pnum]--;
		cmd->impulse = in_impulse[pnum][(in_nextimpulse[pnum]-1)%IN_IMPULSECACHE];
	}
	else
		cmd->impulse = 0;
}

void CL_DrawPrydonCursor(void)
{
	if (cursor_active && cl_prydoncursor.ival > 0)
	{
		SCR_DrawCursor(cl_prydoncursor.ival);
		V_StopPitchDrift (0);
	}
}

void CL_UpdatePrydonCursor(usercmd_t *from, float cursor_screen[2], vec3_t cursor_start, vec3_t cursor_impact, int *entnum)
{
	vec3_t cursor_end;

	vec3_t temp;
	vec3_t cursor_impact_normal;

	cursor_active = true;

	if (!cl_prydoncursor.ival)
	{	//center the cursor
		cursor_screen[0] = 0;
		cursor_screen[1] = 0;
	}
	else
	{
		cursor_screen[0] = mousecursor_x/(vid.width/2.0f) - 1;
		cursor_screen[1] = mousecursor_y/(vid.height/2.0f) - 1;
		if (cursor_screen[0] < -1)
			cursor_screen[0] = -1;
		if (cursor_screen[1] < -1)
			cursor_screen[1] = -1;

		if (cursor_screen[0] > 1)
			cursor_screen[0] = 1;
		if (cursor_screen[1] > 1)
			cursor_screen[1] = 1;
	}

	/*
	if (cl.cmd.cursor_screen[0] < -1)
	{
		cl.viewangles[YAW] -= m_yaw.value * (cl.cmd.cursor_screen[0] - -1) * vid.realwidth * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[0] = -1;
	}
	if (cl.cmd.cursor_screen[0] > 1)
	{
		cl.viewangles[YAW] -= m_yaw.value * (cl.cmd.cursor_screen[0] - 1) * vid.realwidth * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[0] = 1;
	}
	if (cl.cmd.cursor_screen[1] < -1)
	{
		cl.viewangles[PITCH] += m_pitch.value * (cl.cmd.cursor_screen[1] - -1) * vid.realheight * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[1] = -1;
	}
	if (cl.cmd.cursor_screen[1] > 1)
	{
		cl.viewangles[PITCH] += m_pitch.value * (cl.cmd.cursor_screen[1] - 1) * vid.realheight * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[1] = 1;
	}
	*/

	VectorClear(cursor_start);
	temp[0] = (cursor_screen[0]+1)/2;
	temp[1] = (-cursor_screen[1]+1)/2;
	temp[2] = 1;

	VectorCopy(r_origin, cursor_start);
	Matrix4x4_CM_UnProject(temp, cursor_end, cl.playerview[0].viewangles, cursor_start, r_refdef.fov_x, r_refdef.fov_y);

	CL_SetSolidEntities();
	//don't bother with players, they don't exist in NQ...

	TraceLineN(cursor_start, cursor_end, cursor_impact, cursor_impact_normal);

	*entnum = 0;

//	P_RunParticleEffect(cursor_impact, vec3_origin, 15, 16);
}

#ifdef NQPROT
void CLNQ_SendMove (usercmd_t *cmd, int pnum, sizebuf_t *buf)
{
	int i;

	if (cls.demoplayback!=DPB_NONE)
		return;	//err... don't bother... :)
//
// always dump the first two message, because it may contain leftover inputs
// from the last level
//
	if (cl.movesequence <= 2 || cls.state == ca_connected)
	{
		MSG_WriteByte (buf, clc_nop);
		return;
	}

	MSG_WriteByte (buf, clc_move);

	if (cls.protocol_nq >= CPNQ_DP7 || (cls.fteprotocolextensions2 & PEXT2_PREDINFO))
		MSG_WriteLong(buf, cl.movesequence);

	MSG_WriteFloat (buf, cl.gametime);	// so server can get ping times

	for (i=0 ; i<3 ; i++)
	{
		if ((cls.protocol_nq == CPNQ_FITZ666 || cls.protocol_nq == CPNQ_PROQUAKE3_4) && buf->prim.anglesize <= 1)
		{
			//fitz/proquake protocols are always 16bit for this angle and 8bit elsewhere. rmq is always at least 16bit
			//the above logic should satify everything.
			MSG_WriteAngle16 (buf, cl.playerview[pnum].viewangles[i]);
		}
		else
			MSG_WriteAngle (buf, cl.playerview[pnum].viewangles[i]);
	}
	
	MSG_WriteShort (buf, cmd->forwardmove);
	MSG_WriteShort (buf, cmd->sidemove);
	MSG_WriteShort (buf, cmd->upmove);

	if (cls.protocol_nq >= CPNQ_DP6 || (cls.fteprotocolextensions2 & PEXT2_PRYDONCURSOR))
	{
		vec3_t cursor_start, cursor_impact;
		int cursor_entitynumber;

		CL_UpdatePrydonCursor(cmd, cursor_screen, cursor_start, cursor_impact, &cursor_entitynumber);

		MSG_WriteLong (buf, cmd->buttons);
		MSG_WriteByte (buf, cmd->impulse);
		MSG_WriteShort (buf, cursor_screen[0] * 32767.0f);
		MSG_WriteShort (buf, cursor_screen[1] * 32767.0f);
		MSG_WriteFloat (buf, cursor_start[0]);
		MSG_WriteFloat (buf, cursor_start[1]);
		MSG_WriteFloat (buf, cursor_start[2]);
		MSG_WriteFloat (buf, cursor_impact[0]);
		MSG_WriteFloat (buf, cursor_impact[1]);
		MSG_WriteFloat (buf, cursor_impact[2]);
		MSG_WriteEntity (buf, cursor_entitynumber);
	}
	else
	{
		MSG_WriteByte (buf, cmd->buttons);
		MSG_WriteByte (buf, cmd->impulse);
	}
}

void Name_Callback(struct cvar_s *var, char *oldvalue)
{
	if (cls.state <= ca_connected)
		return;

	if (cls.protocol != CP_NETQUAKE)
		return;

	CL_SendClientCommand(true, "name \"%s\"\n", var->string);
}

void CLNQ_SendCmd(sizebuf_t *buf)
{
	int i;
	usercmd_t *cmd;

	i = cl.movesequence & UPDATE_MASK;
	cl.outframes[i].senttime = realtime;
	cmd = &cl.outframes[i].cmd[0];
	*cmd = independantphysics[0];
	cmd->lightlevel = 0;
#ifdef CSQC_DAT
	CSQC_Input_Frame(0, cmd);
#endif
	memset(&independantphysics[0], 0, sizeof(independantphysics[0]));



	//inputs are only sent once we receive an entity.
	if (cls.signon == 4)
	{
	// send the unreliable message
		if (independantphysics[0].impulse && !cls.netchan.message.cursize)
			CLNQ_SendMove (cmd, 0, &cls.netchan.message);
		else
			CLNQ_SendMove (cmd, 0, buf);
	}

	for (i = 0; i < cl.numackframes; i++)
	{
		MSG_WriteByte(buf, clcdp_ackframe);
		MSG_WriteLong(buf, cl.ackframes[i]);
	}
	cl.numackframes = 0;
}
#else
void Name_Callback(struct cvar_s *var, char *oldvalue)
{

}
#endif

float CL_FilterTime (double time, float wantfps, qboolean ignoreserver)	//now returns the extra time not taken in this slot. Note that negative 1 means uncapped.
{
	float fps, fpscap;

	if (cls.timedemo || cls.protocol == CP_QUAKE3)
		return -1;

	/*ignore the server if we're playing demos, sending to the server only as replies, or if its meant to be disabled (netfps depending on where its called from)*/
	if (cls.demoplayback != DPB_NONE || cls.protocol != CP_QUAKEWORLD || ignoreserver)
	{
		if (!wantfps)
			return -1;
		fps = max (30.0, wantfps);
	}
	else
	{
		fpscap = cls.maxfps ? max (30.0, cls.maxfps) : 0x7fff;
#ifdef IRCCONNECT
		if (cls.netchan.remote_address.type == NA_IRC)
			fps = bound (0.1, wantfps, fpscap);	//if we're connected via irc, allow a greatly reduced minimum cap
		else
#endif
		if (wantfps < 1)
			fps = fpscap;
		else
			fps = bound (6.7, wantfps, fpscap);	//we actually cap ourselves to 150msecs (1000/7 = 142)
	}

	if (time < 1000 / fps)
		return 0;

	return time - (1000 / fps);
}

qboolean allowindepphys;

typedef struct clcmdbuf_s {
	struct clcmdbuf_s *next;
	int len;
	qboolean reliable;
	char command[4];	//this is dynamically allocated, so this is variably sized.
} clcmdbuf_t;
clcmdbuf_t *clientcmdlist;
void VARGS CL_SendClientCommand(qboolean reliable, char *format, ...)
{
	qboolean oldallow;
	va_list		argptr;
	char		string[2048];
	clcmdbuf_t *buf, *prev;

	if (cls.demoplayback && cls.demoplayback != DPB_EZTV)
		return;	//no point.

	va_start (argptr, format);
	Q_vsnprintfz (string,sizeof(string), format,argptr);
	va_end (argptr);


	Con_DPrintf("Queing stringcmd %s\n", string);

#ifdef Q3CLIENT
	if (cls.protocol == CP_QUAKE3)
	{
		CLQ3_SendClientCommand("%s", string);
		return;
	}
#endif

	oldallow = allowindepphys;
	CL_AllowIndependantSendCmd(false);

	buf = Z_Malloc(sizeof(*buf)+strlen(string));
	strcpy(buf->command, string);
	buf->len = strlen(buf->command);
	buf->reliable = reliable;

	//add to end of the list so that the first of the list is the first to be sent.
	if (!clientcmdlist)
		clientcmdlist = buf;
	else
	{
		for (prev = clientcmdlist; prev->next; prev=prev->next)
			;
		prev->next = buf;
	}

	CL_AllowIndependantSendCmd(oldallow);
}

int CL_RemoveClientCommands(char *command)
{
	clcmdbuf_t *next, *first;
	int removed = 0;
	int len = strlen(command);

	CL_AllowIndependantSendCmd(false);

	if (!clientcmdlist)
		return 0;

	while(!strncmp(clientcmdlist->command, command, len))
	{
		next = clientcmdlist->next;
		Z_Free(clientcmdlist);
		clientcmdlist=next;
		removed++;

		if (!clientcmdlist)
			return removed;
	}
	first = clientcmdlist;
	while(first->next)
	{
		if (!strncmp(first->next->command, command, len))
		{
			next = first->next->next;
			Z_Free(first->next);
			first->next = next;
			removed++;
		}
		else
			first = first->next;
	}

	return removed;
}

void CL_FlushClientCommands(void)
{
	clcmdbuf_t *next;
	CL_AllowIndependantSendCmd(false);

	while(clientcmdlist)
	{
		Con_DPrintf("Flushed command %s\n", clientcmdlist->command);
		next = clientcmdlist->next;
		Z_Free(clientcmdlist);
		clientcmdlist=next;
	}
}

qboolean runningindepphys;
#ifdef MULTITHREAD
void *indeplock;
void *indepthread;

void CL_AllowIndependantSendCmd(qboolean allow)
{
	if (!runningindepphys)
		return;

	if (allowindepphys != allow && runningindepphys)
	{
		if (allow)
			Sys_UnlockMutex(indeplock);
		else
			Sys_LockMutex(indeplock);
		allowindepphys = allow;
	}
}

int CL_IndepPhysicsThread(void *param)
{
	double sleeptime;
	double fps;
	double time, lasttime;
	double spare;
	lasttime = Sys_DoubleTime();
	while(runningindepphys)
	{
		time = Sys_DoubleTime();
		spare = CL_FilterTime((time - lasttime)*1000, cl_netfps.value, false);
		if (spare)
		{
			//don't let them bank too much and get sudden bursts
			if (spare > 15)
				spare = 15;

			time -= spare/1000.0f;
			Sys_LockMutex(indeplock);
			if (cls.state)
				CL_SendCmd(time - lasttime, false);
			lasttime = time;
			Sys_UnlockMutex(indeplock);
		}

		fps = cl_netfps.value;
		if (fps < 4)
			fps = 4;
		while (fps < 100)
			fps*=2;

		sleeptime = 1/fps;

		Sys_Sleep(sleeptime);
	}
	return 0;
}

void CL_UseIndepPhysics(qboolean allow)
{
	if (runningindepphys == allow)
		return;

	if (allow)
	{	//enable it
		indeplock = Sys_CreateMutex();
		runningindepphys = true;

		indepthread = Sys_CreateThread("indepphys", CL_IndepPhysicsThread, NULL, THREADP_HIGHEST, 8192);
		allowindepphys = true;
	}
	else
	{
		//shut it down.
		runningindepphys = false;	//tell thread to exit gracefully
		Sys_LockMutex(indeplock);
		Sys_WaitOnThread(indepthread);
		Sys_UnlockMutex(indeplock);
		Sys_DestroyMutex(indeplock);
	}
}
#else
void CL_AllowIndependantSendCmd(qboolean allow)
{
}
void CL_UseIndepPhysics(qboolean allow)
{
}
#endif

void CL_SpareMsec_Callback (struct cvar_s *var, char *oldvalue)
{
	if (var->value > 50)
	{
		Cvar_ForceSet(var, "50");
		return;
	}
	else if (var->value < 0)
	{
		Cvar_ForceSet(var, "0");
		return;
	}
}


/*
=================
CL_SendCmd
=================
*/
vec3_t accum[MAX_SPLITS];
qboolean CL_WriteDeltas (int plnum, sizebuf_t *buf)
{
	int i;
	usercmd_t *cmd, *oldcmd;
	qboolean dontdrop = false;


	i = (cls.netchan.outgoing_sequence-2) & UPDATE_MASK;
	cmd = &cl.outframes[i].cmd[plnum];
	if (cl_c2sImpulseBackup.ival >= 2)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (buf, &nullcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence-1) & UPDATE_MASK;
	if (cl_c2sImpulseBackup.ival >= 3)
		dontdrop = dontdrop || cmd->impulse;
	cmd = &cl.outframes[i].cmd[plnum];
	MSG_WriteDeltaUsercmd (buf, oldcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
	if (cl_c2sImpulseBackup.ival >= 1)
		dontdrop = dontdrop || cmd->impulse;
	cmd = &cl.outframes[i].cmd[plnum];
	MSG_WriteDeltaUsercmd (buf, oldcmd, cmd);

	return dontdrop;
}

#ifdef Q2CLIENT
qboolean CLQ2_SendCmd (sizebuf_t *buf)
{
	int seq_hash;
	qboolean dontdrop;
	usercmd_t *cmd;
	int checksumIndex, i;
	qbyte lightlev;

	seq_hash = cls.netchan.outgoing_sequence;

// send this and the previous cmds in the message, so
// if the last packet was dropped, it can be recovered
	i = cls.netchan.outgoing_sequence & UPDATE_MASK;
	cmd = &cl.outframes[i].cmd[0];

	if (cls.resendinfo)
	{
		MSG_WriteByte (&cls.netchan.message, clcq2_userinfo);
		MSG_WriteString (&cls.netchan.message, cls.userinfo[0]);

		cls.resendinfo = false;
	}

	MSG_WriteByte (buf, clcq2_move);

	// save the position for a checksum qbyte
	checksumIndex = buf->cursize;
	MSG_WriteByte (buf, 0);

	if (!cl.q2frame.valid || cl_nodelta.ival)
		MSG_WriteLong (buf, -1);	// no compression
	else
		MSG_WriteLong (buf, cl.q2frame.serverframe);

	lightlev = R_LightPoint(cl.playerview[0].simorg);

//	msecs = msecs - (double)msecstouse;

	i = cls.netchan.outgoing_sequence & UPDATE_MASK;
	cmd = &cl.outframes[i].cmd[0];
	*cmd = independantphysics[0];
	
	cmd->lightlevel = lightlev;

	cl.outframes[i].senttime = realtime;
	memset(&independantphysics[0], 0, sizeof(independantphysics[0]));

	if (cmd->buttons)
		cmd->buttons |= 128;	//fixme: this isn't really what's meant by the anykey.

// calculate a checksum over the move commands
	dontdrop = CL_WriteDeltas(0, buf);

	buf->data[checksumIndex] = Q2COM_BlockSequenceCRCByte(
		buf->data + checksumIndex + 1, buf->cursize - checksumIndex - 1,
		seq_hash);

	if (cl.sendprespawn)
		buf->cursize = 0;	//tastyspleen.net is alergic.

	return dontdrop;
}
#endif

qboolean CLQW_SendCmd (sizebuf_t *buf)
{
	int seq_hash;
	qboolean dontdrop = false;
	usercmd_t *cmd;
	int checksumIndex, firstsize, plnum;
	int clientcount, lost;
	int curframe;
	int st = buf->cursize;

	cl.movesequence = cls.netchan.outgoing_sequence;	//make sure its correct even over map changes.
	curframe = cls.netchan.outgoing_sequence & UPDATE_MASK;
	seq_hash = cls.netchan.outgoing_sequence;

// send this and the previous cmds in the message, so
// if the last packet was dropped, it can be recovered
	clientcount = cl.splitclients;

	if (!clientcount)
		clientcount = 1;


	for (plnum = 0; plnum<clientcount; plnum++)
	{
		cmd = &cl.outframes[curframe].cmd[plnum];
		*cmd = independantphysics[plnum];
		
		cmd->lightlevel = 0;
#ifdef CSQC_DAT
		CSQC_Input_Frame(plnum, cmd);
#endif
		memset(&independantphysics[plnum], 0, sizeof(independantphysics[plnum]));
	}
	cl.outframes[curframe].senttime = realtime;

	if ((cls.fteprotocolextensions2 & PEXT2_PRYDONCURSOR) && (*cl_prydoncursor.string && cl_prydoncursor.ival >= 0) && cls.state == ca_active)
	{
		vec3_t cursor_start, cursor_impact;
		int cursor_entitynumber = 0;
		cmd = &cl.outframes[curframe].cmd[0];
		CL_UpdatePrydonCursor(cmd, cursor_screen, cursor_start, cursor_impact, &cursor_entitynumber);
		MSG_WriteByte (buf, clc_prydoncursor);
		MSG_WriteShort(buf, cursor_screen[0] * 32767.0f);
		MSG_WriteShort(buf, cursor_screen[1] * 32767.0f);
		MSG_WriteFloat(buf, cursor_start[0]);
		MSG_WriteFloat(buf, cursor_start[1]);
		MSG_WriteFloat(buf, cursor_start[2]);
		MSG_WriteFloat(buf, cursor_impact[0]);
		MSG_WriteFloat(buf, cursor_impact[1]);
		MSG_WriteFloat(buf, cursor_impact[2]);
		MSG_WriteEntity(buf, cursor_entitynumber);
	}
	else
		cursor_active = false;

	MSG_WriteByte (buf, clc_move);

	// save the position for a checksum qbyte
	checksumIndex = buf->cursize;
	MSG_WriteByte (buf, 0);

	// write our lossage percentage
	lost = CL_CalcNet();
	MSG_WriteByte (buf, (qbyte)lost);

	firstsize=0;
	for (plnum = 0; plnum<clientcount; plnum++)
	{
		cmd = &cl.outframes[curframe].cmd[plnum];

		if (plnum)
			MSG_WriteByte (buf, clc_move);

		dontdrop = CL_WriteDeltas(plnum, buf) || dontdrop;

		if (!firstsize)
			firstsize = buf->cursize;
	}

// calculate a checksum over the move commands

	buf->data[checksumIndex] = COM_BlockSequenceCRCByte(
		buf->data + checksumIndex + 1, firstsize - checksumIndex - 1,
		seq_hash);

	// request delta compression of entities
	if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP-1)
		cl.validsequence = 0;

	//delta_sequence is the _expected_ previous sequences, so is set before it arrives.
	if (cl.validsequence && !cl_nodelta.ival && cls.state == ca_active && !cls.demorecording)
	{
		cl.inframes[cls.netchan.outgoing_sequence&UPDATE_MASK].delta_sequence = cl.validsequence;
		MSG_WriteByte (buf, clc_delta);
//		Con_Printf("%i\n", cl.validsequence);
		MSG_WriteByte (buf, cl.validsequence&255);
	}
	else
		cl.inframes[cls.netchan.outgoing_sequence&UPDATE_MASK].delta_sequence = -1;

	if (cl.sendprespawn)
		buf->cursize = st;	//tastyspleen.net is alergic.

	return dontdrop;
}

void CL_SendCmd (double frametime, qboolean mainloop)
{
	sizebuf_t	buf;
	qbyte		data[1024];
	int			i, plnum;
	usercmd_t	*cmd;
	float wantfps;
	qboolean fullsend;

	static float	pps_balance = 0;
	static int	dropcount = 0;
	static double msecs;
	int msecstouse;
	qboolean	dontdrop=false;

	clcmdbuf_t *next;

	if (runningindepphys)
	{
		double curtime;
		static double lasttime;
		curtime = Sys_DoubleTime();
		frametime = curtime - lasttime;
		lasttime = curtime;
	}

	CL_ProxyMenuHooks();

	if (cls.demoplayback != DPB_NONE)
	{
		if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
		{
			extern cvar_t cl_splitscreen;
			cl.ackedmovesequence = cl.movesequence;
			i = cl.movesequence & UPDATE_MASK;
			cl.movesequence++;
			cl.outframes[i].senttime = realtime;		// we haven't gotten a reply yet
//			cl.outframes[i].receivedtime = -1;		// we haven't gotten a reply yet

			if (cl.splitclients > cl_splitscreen.ival+1)
			{
				cl.splitclients = cl_splitscreen.ival+1;
				if (cl.splitclients < 1)
					cl.splitclients = 1;
			}
			for (plnum = 0; plnum < cl.splitclients; plnum++)
			{
				cmd = &cl.outframes[i].cmd[plnum];

				memset(cmd, 0, sizeof(*cmd));
				msecs += frametime*1000;
				if (msecs > 50)
					msecs = 50;
				cmd->msec = msecs;
				msecs -= cmd->msec;
				independantphysics[0].msec = 0;

				CL_AdjustAngles (plnum, frametime);
				// get basic movement from keyboard
				CL_BaseMove (cmd, plnum, 1, 1);

				// allow mice or other external controllers to add to the move
				IN_Move (mousemovements[plnum], plnum);
				independantphysics[plnum].forwardmove += mousemovements[plnum][0];
				independantphysics[plnum].sidemove += mousemovements[plnum][1];
				independantphysics[plnum].upmove += mousemovements[plnum][2];
				VectorClear(mousemovements[plnum]);

				// if we are spectator, try autocam
				if (cl.spectator)
					Cam_Track(&cl.playerview[plnum], cmd);

				CL_FinishMove(cmd, cmd->msec, plnum);

				Cam_FinishMove(&cl.playerview[plnum], cmd);
			}

			while (clientcmdlist)
			{
				next = clientcmdlist->next;
				CL_Demo_ClientCommand(clientcmdlist->command);
				Con_DPrintf("Sending stringcmd %s\n", clientcmdlist->command);
				Z_Free(clientcmdlist);
				clientcmdlist = next;
			}

			cls.netchan.outgoing_sequence = cl.movesequence;
		}

		IN_Move (NULL, 0);
		return; // sendcmds come from the demo
	}

	memset(&buf, 0, sizeof(buf));
	buf.maxsize = sizeof(data);
	buf.cursize = 0;
	buf.data = data;
	buf.prim = cls.netchan.message.prim;

#ifdef IRCCONNECT
	if (cls.netchan.remote_address.type != NA_IRC)
#endif
	if (msecs>150)	//q2 has 200 slop.
		msecs=150;

	msecs += frametime*1000;
//	Con_Printf("%f\n", msecs);

	if (msecs<0)
		msecs=0;	//erm.

	msecstouse = (int)msecs; //casts round down.
	if (msecstouse == 0)
		return;
#ifdef IRCCONNECT
	if (cls.netchan.remote_address.type != NA_IRC)
#endif
	if (msecstouse > 200) // cap at 200 to avoid servers splitting movement more than four times
		msecstouse = 200;

	// align msecstouse to avoid servers wasting our msecs
	if (msecstouse > 100)
		msecstouse &= ~3; // align to 4
	else if (msecstouse > 50)
		msecstouse &= ~1; // align to 2

	wantfps = cl_netfps.value;
	fullsend = true;

	if (!runningindepphys)
	{
		// while we're not playing send a slow keepalive fullsend to stop mvdsv from screwing up
		if (cls.state < ca_active && !cls.downloadmethod)
		{
			#ifdef IRCCONNECT	//don't spam irc.
			if (cls.netchan.remote_address.type == NA_IRC)
				wantfps = 0.5;
			else
			#endif
				wantfps = 12.5;
		}
		if (cl_netfps.value > 0 || !fullsend)
		{
			int spare;
			spare = CL_FilterTime(msecstouse, wantfps, false);
			if (!spare && (msecstouse < 200
#ifdef IRCCONNECT
				|| cls.netchan.remote_address.type == NA_IRC
#endif
				))
				fullsend = false;
			if (spare > cl_sparemsec.ival)
				spare = cl_sparemsec.ival;
			if (spare > 0)
				msecstouse -= spare;
		}
	}

#ifdef HLCLIENT
	if (!CLHL_BuildUserInput(msecstouse, &independantphysics[0]))
#endif
	for (plnum = 0; plnum < cl.splitclients; plnum++)
	{
//		CL_BaseMove (&independantphysics[plnum], plnum, (msecstouse - independantphysics[plnum].msec), wantfps);
		CL_AdjustAngles (plnum, frametime);
		IN_Move (mousemovements[plnum], plnum);
		CL_ClampPitch(plnum);
		independantphysics[plnum].forwardmove += mousemovements[plnum][0];
		independantphysics[plnum].sidemove += mousemovements[plnum][1];
		independantphysics[plnum].upmove += mousemovements[plnum][2];
		VectorClear(mousemovements[plnum]);
		
		for (i=0 ; i<3 ; i++)
			independantphysics[plnum].angles[i] = ((int)(cl.playerview[plnum].viewangles[i]*65536.0/360)&65535);

		if (!independantphysics[plnum].msec)
		{
			CL_BaseMove (&independantphysics[plnum], plnum, (msecstouse - independantphysics[plnum].msec), wantfps);
			CL_FinishMove(&independantphysics[plnum], msecstouse, plnum);
		}

		// if we are spectator, try autocam
	//	if (cl.spectator)
		Cam_Track(&cl.playerview[plnum], &independantphysics[plnum]);
		Cam_FinishMove(&cl.playerview[plnum], &independantphysics[plnum]);
		independantphysics[plnum].msec = msecstouse;
	}

	//the main loop isn't allowed to send
	if (runningindepphys && mainloop)
		return;

//	if (skipcmd)
//		return;

	if (!fullsend)
		return; // when we're actually playing we try to match netfps exactly to avoid gameplay problems

#ifdef NQPROT
	if (cls.protocol != CP_NETQUAKE || cls.netchan.nqreliable_allowed)
#endif
	{
		CL_SendDownloadReq(&buf);

		while (clientcmdlist)
		{
			next = clientcmdlist->next;
			if (clientcmdlist->reliable)
			{
				if (cls.netchan.message.cursize + 2+strlen(clientcmdlist->command)+100 > cls.netchan.message.maxsize)
					break;
				MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
				MSG_WriteString (&cls.netchan.message, clientcmdlist->command);
			}
			else
			{
				if (buf.cursize + 2+strlen(clientcmdlist->command)+100 <= buf.maxsize)
				{
					MSG_WriteByte (&buf, clc_stringcmd);
					MSG_WriteString (&buf, clientcmdlist->command);
				}
			}
			Con_DPrintf("Sending stringcmd %s\n", clientcmdlist->command);
			Z_Free(clientcmdlist);
			clientcmdlist = next;
		}
	}

	// if we're not doing clc_moves and etc, don't continue unless we wrote something previous
	// or we have something on the reliable buffer (or we're loopback and don't care about flooding)
	if (!fullsend && cls.netchan.remote_address.type != NA_LOOPBACK && buf.cursize < 1 && cls.netchan.message.cursize < 1)
		return; 

	if (fullsend)
	{
		switch (cls.protocol)
		{
#ifdef NQPROT
		case CP_NETQUAKE:
			msecs -= (double)msecstouse;
			CLNQ_SendCmd (&buf);
			break;
#endif
		case CP_QUAKEWORLD:
			msecs -= (double)msecstouse;
			dontdrop = CLQW_SendCmd (&buf);
			break;
#ifdef Q2CLIENT
		case CP_QUAKE2:
			msecs -= (double)msecstouse;
			dontdrop = CLQ2_SendCmd (&buf);
			break;
#endif
#ifdef Q3CLIENT
		case CP_QUAKE3:
			CLQ3_SendCmd(&independantphysics[0]);
			memset(&independantphysics[0], 0, sizeof(independantphysics[0]));
			return; // Q3 does it's own thing
#endif
		default:
			Host_EndGame("Invalid protocol in CL_SendCmd: %i", cls.protocol);
			return;
		}
	}

	i = cl.movesequence & UPDATE_MASK;
	cmd = &cl.outframes[i].cmd[0];

	if (cls.demorecording)
		CL_WriteDemoCmd(cmd);

	cl.movesequence++;

#ifdef IRCCONNECT
	if (cls.netchan.remote_address.type == NA_IRC)
	{
		if (dropcount >= 2)
		{
			dropcount = 0;
		}
		else
		{
			// don't count this message when calculating PL
			cl.inframes[i].latency = -3;
			// drop this message
			cls.netchan.outgoing_sequence++;
			dropcount++;
			return;
		}
	}
	else
#endif
	//shamelessly stolen from fuhquake
		if (cl_c2spps.ival>0)
	{
		pps_balance += frametime;
		// never drop more than 2 messages in a row -- that'll cause PL
		// and don't drop if one of the last two movemessages have an impulse
		if (pps_balance > 0 || dropcount >= 2 || dontdrop)
		{
			float	pps;
			pps = cl_c2spps.ival;
			if (pps < 10) pps = 10;
			if (pps > 72) pps = 72;
			pps_balance -= 1 / pps;
			// bound pps_balance. FIXME: is there a better way?
			if (pps_balance > 0.1) pps_balance = 0.1;
			if (pps_balance < -0.1) pps_balance = -0.1;
			dropcount = 0;
		}
		else
		{
			// don't count this message when calculating PL
			cl.inframes[i].latency = -3;
			// drop this message
			cls.netchan.outgoing_sequence++;
			dropcount++;
			return;
		}
	}
	else
	{
		pps_balance = 0;
		dropcount = 0;
	}

#ifdef VOICECHAT
	if (cls.protocol == CP_QUAKE2)
		S_Voip_Transmit(clcq2_voicechat, &buf);
	else
		S_Voip_Transmit(clc_voicechat, &buf);
#endif

//
// deliver the message
//
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data, 2500);	

	if (cls.netchan.fatal_error)
	{
		cls.netchan.fatal_error = false;
		cls.netchan.message.overflowed = false;
		cls.netchan.message.cursize = 0;
	}
}

void CL_SendCvar_f (void)
{
	cvar_t *var;
	char *val;
	char *name = Cmd_Argv(1);

	var = Cvar_FindVar(name);
	if (!var)
		val = "";
	else if (var->flags & CVAR_NOUNSAFEEXPAND)
		val = "";
	else
		val = var->string;
	CL_SendClientCommand(true, "sentcvar %s \"%s\"", name, val);
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	static char pcmd[MAX_SPLITS][3][5];
	int sp;
#define inputnetworkcvargroup "client networking options"
	cl.splitclients = 1;

	Cmd_AddCommand("rotate", IN_Rotate_f);
	Cmd_AddCommand("in_restart", IN_Restart);
	Cmd_AddCommand("sendcvar", CL_SendCvar_f);

	Cvar_Register (&in_xflip, inputnetworkcvargroup);
	Cvar_Register (&cl_nodelta, inputnetworkcvargroup);

	Cvar_Register (&prox_inmenu, inputnetworkcvargroup);

	Cvar_Register (&cl_c2sImpulseBackup, inputnetworkcvargroup);
	Cvar_Register (&cl_c2spps, inputnetworkcvargroup);
	Cvar_Register (&cl_queueimpulses, inputnetworkcvargroup);
	Cvar_Register (&cl_netfps, inputnetworkcvargroup);
	Cvar_Register (&cl_sparemsec, inputnetworkcvargroup);

	Cvar_Register (&cl_smartjump, inputnetworkcvargroup);

	Cvar_Register (&cl_prydoncursor, inputnetworkcvargroup);
	Cvar_Register (&cl_instantrotate, inputnetworkcvargroup);
	Cvar_Register (&cl_forcesplitclient, inputnetworkcvargroup);

	for (sp = 0; sp < MAX_SPLITS; sp++)
	{
		Q_snprintfz(pcmd[sp][0], sizeof(pcmd[sp][0]), "p%i", sp+1);
		Q_snprintfz(pcmd[sp][1], sizeof(pcmd[sp][1]), "+p%i", sp+1);
		Q_snprintfz(pcmd[sp][2], sizeof(pcmd[sp][2]), "-p%i", sp+1);
		Cmd_AddCommand (pcmd[sp][0],	CL_Split_f);
		Cmd_AddCommand (pcmd[sp][1],	CL_Split_f);
		Cmd_AddCommand (pcmd[sp][2],	CL_Split_f);

/*default mlook to pressed, (on android we split the two sides of the screen)*/
		in_mlook.state[sp] = 1;
	}

	/*then alternative arged ones*/
	Cmd_AddCommand ("p",			CL_SplitA_f);
	Cmd_AddCommand ("+p",			CL_SplitA_f);
	Cmd_AddCommand ("-p",			CL_SplitA_f);
	
	Cmd_AddCommand ("+moveup",		IN_UpDown);
	Cmd_AddCommand ("-moveup",		IN_UpUp);
	Cmd_AddCommand ("+movedown",	IN_DownDown);
	Cmd_AddCommand ("-movedown",	IN_DownUp);
	Cmd_AddCommand ("+left",		IN_LeftDown);
	Cmd_AddCommand ("-left",		IN_LeftUp);
	Cmd_AddCommand ("+right",		IN_RightDown);
	Cmd_AddCommand ("-right",		IN_RightUp);
	Cmd_AddCommand ("+forward",		IN_ForwardDown);
	Cmd_AddCommand ("-forward",		IN_ForwardUp);
	Cmd_AddCommand ("+back",		IN_BackDown);
	Cmd_AddCommand ("-back",		IN_BackUp);
	Cmd_AddCommand ("+lookup",		IN_LookupDown);
	Cmd_AddCommand ("-lookup",		IN_LookupUp);
	Cmd_AddCommand ("+lookdown",	IN_LookdownDown);
	Cmd_AddCommand ("-lookdown",	IN_LookdownUp);
	Cmd_AddCommand ("+strafe",		IN_StrafeDown);
	Cmd_AddCommand ("-strafe",		IN_StrafeUp);
	Cmd_AddCommand ("+moveleft",	IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft",	IN_MoveleftUp);
	Cmd_AddCommand ("+moveright",	IN_MoverightDown);
	Cmd_AddCommand ("-moveright",	IN_MoverightUp);
	Cmd_AddCommand ("+speed",		IN_SpeedDown);
	Cmd_AddCommand ("-speed",		IN_SpeedUp);
	Cmd_AddCommand ("+attack",		IN_AttackDown);
	Cmd_AddCommand ("-attack",		IN_AttackUp);
	Cmd_AddCommand ("+use",			IN_UseDown);
	Cmd_AddCommand ("-use",			IN_UseUp);
	Cmd_AddCommand ("+jump",		IN_JumpDown);
	Cmd_AddCommand ("-jump",		IN_JumpUp);
	Cmd_AddCommand ("impulse",		IN_Impulse);
	Cmd_AddCommand ("+klook",		IN_KLookDown);
	Cmd_AddCommand ("-klook",		IN_KLookUp);
	Cmd_AddCommand ("+mlook",		IN_MLookDown);
	Cmd_AddCommand ("-mlook",		IN_MLookUp);

	Cmd_AddCommand ("+button3",		IN_Button3Down);
	Cmd_AddCommand ("-button3",		IN_Button3Up);
	Cmd_AddCommand ("+button4",		IN_Button4Down);
	Cmd_AddCommand ("-button4",		IN_Button4Up);
	Cmd_AddCommand ("+button5",		IN_Button5Down);
	Cmd_AddCommand ("-button5",		IN_Button5Up);
	Cmd_AddCommand ("+button6",		IN_Button6Down);
	Cmd_AddCommand ("-button6",		IN_Button6Up);
	Cmd_AddCommand ("+button7",		IN_Button7Down);
	Cmd_AddCommand ("-button7",		IN_Button7Up);
	Cmd_AddCommand ("+button8",		IN_Button8Down);
	Cmd_AddCommand ("-button8",		IN_Button8Up);
}
