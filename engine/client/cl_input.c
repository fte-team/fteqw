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

cvar_t	cl_nodelta = SCVAR("cl_nodelta","0");

cvar_t	cl_c2spps = SCVAR("cl_c2spps", "0");
cvar_t	cl_c2sImpulseBackup = SCVAR("cl_c2sImpulseBackup","3");

cvar_t	cl_netfps = SCVAR("cl_netfps", "0");

cvar_t	cl_smartjump = SCVAR("cl_smartjump", "1");

cvar_t	cl_prydoncursor = SCVAR("cl_prydoncursor", "0");	//for dp protocol
cvar_t	cl_instantrotate = SCVARF("cl_instantrotate", "1", CVAR_SEMICHEAT);

cvar_t	prox_inmenu = SCVAR("prox_inmenu", "0");

usercmd_t independantphysics[MAX_SPLITS];

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


void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;

	int pnum;
	c = Cmd_Argv(0);
	pnum = atoi(c+strlen(c)-1);
	if (c[1] == 'b'&&c[2] == 'u' && !atoi(c+strlen(c)-2))
		pnum = 0;
	else if (pnum)pnum--;
	
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

	int pnum;
	c = Cmd_Argv(0);
	pnum = atoi(c+strlen(c)-1);
	if (c[1] == 'b'&&c[2] == 'u' && !atoi(c+strlen(c)-2))
		pnum = 0;
	else if (pnum)pnum--;
	
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
void IN_MLookUp (void) {
	char	*c;
	int pnum;
	c = Cmd_Argv(0);
	pnum = atoi(c+strlen(c)-1);
	if (pnum)pnum--;
KeyUp(&in_mlook);
if ( !(in_mlook.state[pnum]&1) &&  lookspring.value)
	V_StartPitchDrift(pnum);
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


	int pnum;
	char *c;
	c = Cmd_Argv(0);
	pnum = atoi(c+strlen(c)-1);
	if (pnum)pnum--;
	


	condition = (cls.state == ca_active && cl_smartjump.value && !prox_inmenu.value);
#ifdef Q2CLIENT
	if (condition && cls.protocol == CP_QUAKE2)
		KeyDown(&in_up);
	else
#endif
		if (condition && cl.stats[pnum][STAT_HEALTH] > 0 && !cls.demoplayback && !cl.spectator && 
		cl.frames[cl.validsequence&UPDATE_MASK].playerstate[cl.playernum[pnum]].messagenum == cl.validsequence && cl.waterlevel[pnum] >= 2 && (!cl.teamfortress || !(in_forward.state[pnum] & 1))
	)
		KeyDown(&in_up);
	else if (condition && cl.spectator && Cam_TrackNum(pnum) == -1)
		KeyDown(&in_up);
	else
		KeyDown(&in_jump);
}
void IN_JumpUp (void)
{
	if (cl_smartjump.value)
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



	char	*c;
	int pnum;
	c = Cmd_Argv(0);
	pnum = atoi(c+strlen(c)-1);
	if (pnum)pnum--;

	newimp = Q_atoi(Cmd_Argv(1));

	if (Cmd_Argc() > 2)
	{
		items = cl.stats[pnum][STAT_ITEMS];
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
					if (items & IT_SHOTGUN && cl.stats[pnum][STAT_SHELLS] >= 1)
						best = 2;
					break;
				case 3:
					if (items & IT_SUPER_SHOTGUN && cl.stats[pnum][STAT_SHELLS] >= 2)
						best = 3;
					break;
				case 4:
					if (items & IT_NAILGUN && cl.stats[pnum][STAT_NAILS] >= 1)
						best = 4;
					break;
				case 5:
					if (items & IT_SUPER_NAILGUN && cl.stats[pnum][STAT_NAILS] >= 2)
						best = 5;
					break;
				case 6:
					if (items & IT_GRENADE_LAUNCHER && cl.stats[pnum][STAT_ROCKETS] >= 1)
						best = 6;
					break;
				case 7:
					if (items & IT_ROCKET_LAUNCHER && cl.stats[pnum][STAT_ROCKETS] >= 1)
						best = 7;
					break;
				case 8:
					if (items & IT_LIGHTNING && cl.stats[pnum][STAT_CELLS] >= 1)
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

	in_impulse[pnum][(in_nextimpulse[pnum]+in_impulsespending[pnum])%IN_IMPULSECACHE] = newimp;
	in_impulsespending[pnum]++;
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
	if (!prox_inmenu.value)
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

cvar_t	cl_upspeed = SCVAR("cl_upspeed","400");
cvar_t	cl_forwardspeed = SCVARF("cl_forwardspeed","400", CVAR_ARCHIVE);
cvar_t	cl_backspeed = SCVARF("cl_backspeed","400", CVAR_ARCHIVE);
cvar_t	cl_sidespeed = SCVAR("cl_sidespeed","400");

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
void CL_AdjustAngles (int pnum)
{
	float	speed, quant;
	float	up, down;
	
	if (in_speed.state[pnum] & 1)
	{
		if (ruleset_allow_frj.value)
			speed = host_frametime * cl_anglespeedkey.value;
		else
			speed = host_frametime * bound(-2, cl_anglespeedkey.value, 2);
	}
	else
		speed = host_frametime;

	if (in_rotate && pnum==0 && !(cl.fpd & FPD_LIMIT_YAW))
	{
		quant = in_rotate;
		if (!cl_instantrotate.value)
			quant *= speed;
		in_rotate -= quant;
		if (ruleset_allow_frj.value)
			cl.viewangles[pnum][YAW] += quant;
	}

	if (!(in_strafe.state[pnum] & 1))
	{
		quant = cl_yawspeed.value;
		if (cl.fpd & FPD_LIMIT_YAW || !ruleset_allow_frj.value)
			quant = bound(-900, quant, 900);
		cl.viewangles[pnum][YAW] -= speed*quant * CL_KeyState (&in_right, pnum);
		cl.viewangles[pnum][YAW] += speed*quant * CL_KeyState (&in_left, pnum);
		cl.viewangles[pnum][YAW] = anglemod(cl.viewangles[pnum][YAW]);
	}
	if (in_klook.state[pnum] & 1)
	{
		V_StopPitchDrift (pnum);
		quant = cl_pitchspeed.value;
		if (cl.fpd & FPD_LIMIT_PITCH || !ruleset_allow_frj.value)
			quant = bound(-700, quant, 700);
		cl.viewangles[pnum][PITCH] -= speed*quant * CL_KeyState (&in_forward, pnum);
		cl.viewangles[pnum][PITCH] += speed*quant * CL_KeyState (&in_back, pnum);
	}
	
	up = CL_KeyState (&in_lookup, pnum);
	down = CL_KeyState(&in_lookdown, pnum);

	quant = cl_pitchspeed.value;
	if (!ruleset_allow_frj.value)
		quant = bound(-700, quant, 700);	
	cl.viewangles[pnum][PITCH] -= speed*cl_pitchspeed.value * up;
	cl.viewangles[pnum][PITCH] += speed*cl_pitchspeed.value * down;

	if (up || down)
		V_StopPitchDrift (pnum);
		
	CL_ClampPitch(pnum);

	if (cl.viewangles[pnum][ROLL] > 50)
		cl.viewangles[pnum][ROLL] = 50;
	if (cl.viewangles[pnum][ROLL] < -50)
		cl.viewangles[pnum][ROLL] = -50;
		
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

#ifdef IN_XFLIP
	if(in_xflip.value) cmd->sidemove *= -1;
#endif


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
#ifdef Q2CLIENT
	float	pitch;
	if (cls.protocol == CP_QUAKE2)
	{
		pitch = SHORT2ANGLE(cl.q2frame.playerstate.pmove.delta_angles[PITCH]);
		if (pitch > 180)
			pitch -= 360;

		if (cl.viewangles[pnum][PITCH] + pitch < -360)
			cl.viewangles[pnum][PITCH] += 360; // wrapped
		if (cl.viewangles[pnum][PITCH] + pitch > 360)
			cl.viewangles[pnum][PITCH] -= 360; // wrapped

		if (cl.viewangles[pnum][PITCH] + pitch > cl.maxpitch)
			cl.viewangles[pnum][PITCH] = cl.maxpitch - pitch;
		if (cl.viewangles[pnum][PITCH] + pitch < cl.minpitch)
			cl.viewangles[pnum][PITCH] = cl.minpitch - pitch;
	}
	else
#endif
	{
		if (cl.viewangles[pnum][PITCH] > cl.maxpitch)
			cl.viewangles[pnum][PITCH] = cl.maxpitch;
		if (cl.viewangles[pnum][PITCH] < cl.minpitch)
			cl.viewangles[pnum][PITCH] = cl.minpitch;
	}
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

//
// always dump the first two message, because it may contain leftover inputs
// from the last level
//
	if (++cl.movemessages <= 2)
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
		cmd->angles[i] = ((int)(cl.viewangles[pnum][i]*65536.0/360)&65535);

	if (in_impulsespending[pnum])
	{
		in_nextimpulse[pnum]++;
		in_impulsespending[pnum]--;
		cmd->impulse = in_impulse[pnum][(in_nextimpulse[pnum]-1)%IN_IMPULSECACHE];
	}
	else
		cmd->impulse = 0;
}

float cursor_screen[2];

void CL_DrawPrydonCursor(void)
{
	if (cls.protocol == CP_NETQUAKE)
	if (nq_dp_protocol >= 6)
	if (cl_prydoncursor.value)
	{
		mpic_t *pic = Draw_SafeCachePic(va("gfx/prydoncursor%03i.lmp", (int)cl_prydoncursor.value));
		if (pic)
			Draw_Pic((int)((cursor_screen[0] + 1) * 0.5 * vid.width), (int)((cursor_screen[1] + 1) * 0.5 * vid.height), pic);
		else
			Draw_Character((int)((cursor_screen[0] + 1) * 0.5 * vid.width), (int)((cursor_screen[1] + 1) * 0.5 * vid.height), '+');
	}
}

void CL_UpdatePrydonCursor(usercmd_t *from, float cursor_screen[2], vec3_t cursor_start, vec3_t cursor_impact, int *entnum)
{
	vec3_t cursor_end;

	vec3_t temp;
	vec3_t cursor_impact_normal;

	if (!cl_prydoncursor.value)
	{	//center the cursor
		cursor_screen[0] = 0;
		cursor_screen[1] = 0;
	}
	else
	{
		cursor_screen[0] += from->sidemove/10000.0f;
		cursor_screen[1] -= from->forwardmove/10000.0f;
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
//	cursor_screen[0] = bound(-1, cursor_screen[0], 1);
//	cursor_screen[1] = bound(-1, cursor_screen[1], 1);

	VectorClear(cursor_start);
	temp[0] = (cursor_screen[0]+1)/2;
	temp[1] = (-cursor_screen[1]+1)/2;
	temp[2] = 1;

	Matrix4_UnProject(temp, cursor_end, cl.viewangles[0], vec3_origin, (float)vid.width/vid.height, scr_fov.value );
	VectorScale(cursor_end, 100000, cursor_end);

	VectorAdd(cursor_start, cl.simorg[0], cursor_start);
	VectorAdd(cursor_end, cl.simorg[0], cursor_end);
	cursor_start[2]+=cl.viewheight[0];
	cursor_end[2]+=cl.viewheight[0];


	CL_SetSolidEntities();
	//don't bother with players, they don't exist in NQ...

	TraceLineN(cursor_start, cursor_end, cursor_impact, cursor_impact_normal);
//	CL_SelectTraceLine(cursor_start, cursor_end, cursor_impact, entnum);
	// makes sparks where cursor is
	//CL_SparkShower(cl.cmd.cursor_impact, cl.cmd.cursor_normal, 5, 0);
//	P_RunParticleEffectType(cursor_impact, vec3_origin, 1, pt_gunshot);
//P_ParticleTrail(cursor_start, cursor_impact, 0, NULL);
}

#ifdef NQPROT
void CLNQ_SendMove (usercmd_t		*cmd, int pnum, sizebuf_t *buf)
{
	int bits;
	int i;

	vec3_t cursor_start, cursor_impact;
	int cursor_entitynumber=0;//I hate warnings as errors

	if (cls.demoplayback!=DPB_NONE)
		return;	//err... don't bother... :)
//
// always dump the first two message, because it may contain leftover inputs
// from the last level
//
	if (++cl.movemessages <= 2)
		return;

	MSG_WriteByte (buf, clc_move);

	if (nq_dp_protocol>=7)
		MSG_WriteLong(buf, cls.netchan.outgoing_sequence);

	MSG_WriteFloat (buf, cl.gametime);	// so server can get ping times

	for (i=0 ; i<3 ; i++)
		MSG_WriteAngle (buf, cl.viewangles[pnum][i]);
	
	MSG_WriteShort (buf, cmd->forwardmove);
	MSG_WriteShort (buf, cmd->sidemove);
	MSG_WriteShort (buf, cmd->upmove);

//
// send button bits
//
	bits = 0;

	if (in_attack.state[pnum] & 3 )	bits |=   1; in_attack.state[pnum]	&= ~2;
	if (in_jump.state[pnum] & 3)	bits |=   2; in_jump.state[pnum]	&= ~2;
	if (in_use.state[pnum] & 3)		bits |=   4; in_use.state[pnum]		&= ~2;
	if (in_button3.state[pnum] & 3)	bits |=   4; in_button3.state[pnum] &= ~2;	//yup, flag 4 twice.
	if (in_button4.state[pnum] & 3)	bits |=   8; in_button4.state[pnum] &= ~2;
	if (in_button5.state[pnum] & 3)	bits |=  16; in_button5.state[pnum] &= ~2;
	if (in_button6.state[pnum] & 3)	bits |=  32; in_button6.state[pnum] &= ~2;
	if (in_button7.state[pnum] & 3)	bits |=  64; in_button7.state[pnum] &= ~2;
	if (in_button8.state[pnum] & 3)	bits |= 128; in_button8.state[pnum] &= ~2;

	if (nq_dp_protocol >= 6)
	{
		CL_UpdatePrydonCursor(cmd, cursor_screen, cursor_start, cursor_impact, &cursor_entitynumber);
		MSG_WriteLong (buf, bits);
	}
	else
		MSG_WriteByte (buf, bits);

	if (in_impulsespending[pnum])
	{
		in_nextimpulse[pnum]++;
		in_impulsespending[pnum]--;
		MSG_WriteByte(buf, in_impulse[pnum][(in_nextimpulse[pnum]-1)%IN_IMPULSECACHE]);
	}
	else
		MSG_WriteByte (buf, 0);


	if (nq_dp_protocol >= 6)
	{
		MSG_WriteShort (buf, cursor_screen[0] * 32767.0f);
		MSG_WriteShort (buf, cursor_screen[1] * 32767.0f);
		MSG_WriteFloat (buf, cursor_start[0]);
		MSG_WriteFloat (buf, cursor_start[1]);
		MSG_WriteFloat (buf, cursor_start[2]);
		MSG_WriteFloat (buf, cursor_impact[0]);
		MSG_WriteFloat (buf, cursor_impact[1]);
		MSG_WriteFloat (buf, cursor_impact[2]);
		MSG_WriteShort (buf, cursor_entitynumber);
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

void CLNQ_SendCmd(void)
{
	extern int cl_latestframenum, nq_dp_protocol;
	sizebuf_t unrel;
	char unrel_buf[256];

	if (cls.state <= ca_connected)
		return;

	memset(&unrel, 0, sizeof(unrel));
	unrel.data = unrel_buf;
	unrel.maxsize = sizeof(unrel_buf);

	if (cls.signon == 4)
	{
	// send the unreliable message
		if (independantphysics[0].impulse && !cls.netchan.message.cursize)
			CLNQ_SendMove (&independantphysics[0], 0, &cls.netchan.message);
		else
			CLNQ_SendMove (&independantphysics[0], 0, &unrel);
	}

	if (nq_dp_protocol > 0 && cls.signon == 4)
	{
		MSG_WriteByte(&cls.netchan.message, 50);
		MSG_WriteLong(&cls.netchan.message, cl_latestframenum);
	}

	Netchan_Transmit(&cls.netchan, unrel.cursize, unrel.data, 2500);

	memset(&independantphysics[0], 0, sizeof(independantphysics[0]));
	cl.allowsendpacket = false;
}
#else
void Name_Callback(struct cvar_s *var, char *oldvalue)
{

}
#endif

float CL_FilterTime (double time, float wantfps)	//now returns the extra time not taken in this slot. Note that negative 1 means uncapped.
{
	float fps, fpscap;

	if (cls.timedemo || cls.protocol == CP_QUAKE3)
		return -1;

	if (cls.demoplayback != DPB_NONE || cls.protocol != CP_QUAKEWORLD)
	{
		if (!wantfps)
			return -1;
		fps = max (30.0, wantfps);
	}
	else
	{
		fpscap = cls.maxfps ? max (30.0, cls.maxfps) : 0x7fff;
	
		if (wantfps < 1)
			fps = fpscap;
		else
			fps = bound (6.7, wantfps, fpscap);	//we actually cap ourselves to 150msecs (1000/7 = 142)
	}

	if (time < 1000 / fps)
		return false;

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

	if (cls.demoplayback)
		return;	//no point.

	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
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
#ifdef _WIN32
CRITICAL_SECTION indepcriticialsection;
HANDLE indepphysicsthread;
void CL_AllowIndependantSendCmd(qboolean allow)
{
	if (!runningindepphys)
		return;

	if (allowindepphys != allow && runningindepphys)
	{
		if (allow)
			LeaveCriticalSection(&indepcriticialsection);
		else
			EnterCriticalSection(&indepcriticialsection);
		allowindepphys = allow;
	}
}

unsigned long _stdcall CL_IndepPhysicsThread(void *param)
{
	int sleeptime;
	float fps;
	float time, lasttime;
	float spare;
	lasttime = Sys_DoubleTime();
	while(1)
	{
		time = Sys_DoubleTime();
		spare = CL_FilterTime((time - lasttime)*1000, cl_netfps.value);
		if (spare)
		{
			//don't let them bank too much and get sudden bursts
			if (spare > 15)
				spare = 15;

			time -= spare/1000.0f;
			EnterCriticalSection(&indepcriticialsection);
			if (cls.state)
				CL_SendCmd(time - lasttime);
			lasttime = time;
			LeaveCriticalSection(&indepcriticialsection);
		}

		fps = cl_netfps.value;
		if (fps < 4)
			fps = 4;
		while (fps < 100)
			fps*=2;

		sleeptime = 1000/fps;

		if (sleeptime)
			Sleep(sleeptime);
		else
			Sleep(1);
	}
}

void CL_UseIndepPhysics(qboolean allow)
{
	if (runningindepphys == allow)
		return;

	if (allow)
	{	//enable it
		DWORD tid;	//*sigh*...

//		TIMECAPS tc;
//		timeGetDevCaps(&tc, sizeof(TIMECAPS));
//		Con_Printf("Timer has a resolution of %i millisecond%s\n", tc.wPeriodMin, tc.wPeriodMin!=1?"s":"");

		InitializeCriticalSection(&indepcriticialsection);
		runningindepphys = true;

		indepphysicsthread = CreateThread(NULL, 8192, CL_IndepPhysicsThread, NULL, 0, &tid);
		allowindepphys = 1;

		SetThreadPriority(independantphysics, HIGH_PRIORITY_CLASS);
	}
	else
	{
		//shut it down.

		EnterCriticalSection(&indepcriticialsection);
		TerminateThread(indepphysicsthread, 0);
		CloseHandle(indepphysicsthread);
		LeaveCriticalSection(&indepcriticialsection);
		DeleteCriticalSection(&indepcriticialsection);

		runningindepphys = false;
	}
}

#elif defined(__linux__)

#include <pthread.h>

pthread_mutex_t indepcriticalsection;
pthread_t indepphysicsthread;

void CL_AllowIndependantSendCmd(qboolean allow)
{
	if (!runningindepphys)
		return;

	if (allowindepphys != allow && runningindepphys)
	{
		if (allow)
			pthread_mutex_unlock(&indepcriticalsection);
		else
			pthread_mutex_lock(&indepcriticalsection);
		allowindepphys = allow;
	}
}

void *CL_IndepPhysicsThread(void *param)
{
	int sleeptime;
	double fps;
	double time, lasttime;
	double spare;
	lasttime = Sys_DoubleTime();
	while(runningindepphys)
	{
		time = Sys_DoubleTime();
		spare = CL_FilterTime((time - lasttime)*1000, cl_netfps.value);
		if (spare)
		{
			//don't let them bank too much and get sudden bursts
			if (spare > 15)
				spare = 15;

			time -= spare/1000.0f;
			pthread_mutex_lock(&indepcriticalsection);
			if (cls.state)
				CL_SendCmd(time - lasttime);
			lasttime = time;
			pthread_mutex_unlock(&indepcriticalsection);
		}

		fps = cl_netfps.value;
		if (fps < 4)
			fps = 4;
		while (fps < 100)
			fps*=2;

		sleeptime = (1000*1000)/fps;

		if (sleeptime)
			usleep(sleeptime);
		else
			usleep(1);
	}
	return NULL;
}

void CL_UseIndepPhysics(qboolean allow)
{
	if (runningindepphys == allow)
		return;

	if (allow)
	{	//enable it
		pthread_mutex_init(&indepcriticalsection, NULL);
		runningindepphys = true;

		pthread_create(&indepphysicsthread, NULL, CL_IndepPhysicsThread, NULL);
		allowindepphys = 1;

		//now this would be awesome, but would require root permissions... which is plain wrong!
		//however, lack of this line means its really duel-core only.
		//pthread_setschedparam(indepthread, SCHED_*, ?);
		//is there anything to weight the thread up a bit against the main thread? (considering that most of the time we'll be idling)
	}
	else
	{
		//shut it down.
		runningindepphys = false;	//tell thread to exit gracefully

		pthread_mutex_lock(&indepcriticalsection);
		pthread_join(indepphysicsthread, 0);
		pthread_mutex_unlock(&indepcriticalsection);
		pthread_mutex_destroy(&indepcriticalsection);
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

/*
=================
CL_SendCmd
=================
*/
vec3_t accum[MAX_SPLITS];
void CL_SendCmd (double frametime)
{
	extern cvar_t cl_indepphysics;
	sizebuf_t	buf;
	qbyte		data[512];
	int			i, plnum;
	usercmd_t	*cmd, *oldcmd;
	int			checksumIndex;
	int			lost;
	int			seq_hash;
	int firstsize;
	float wantfps;

	qbyte lightlev;

	static float	pps_balance = 0;
	static int	dropcount = 0;
	static double msecs;
	int msecstouse;
	qboolean	dontdrop=false;

	int clientcount;

	extern cvar_t cl_maxfps;

	CL_ProxyMenuHooks();

	if (cls.demoplayback != DPB_NONE)
	{
		if (cls.demoplayback == DPB_MVD)
		{
			extern cvar_t cl_splitscreen;
			i = cls.netchan.outgoing_sequence & UPDATE_MASK;
			cl.frames[i].senttime = realtime;		// we haven't gotten a reply yet
			cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet

			if (cl.splitclients > cl_splitscreen.value+1)
			{
				cl.splitclients = cl_splitscreen.value+1;
				if (cl.splitclients < 1)
					cl.splitclients = 1;
			}
			for (plnum = 0; plnum < cl.splitclients; plnum++)
			{
				cmd = &cl.frames[i].cmd[0];

				memset(cmd, 0, sizeof(*cmd));
				cmd->msec = frametime*1000;
				independantphysics[0].msec = 0;

				CL_AdjustAngles (plnum);
				// get basic movement from keyboard
				CL_BaseMove (cmd, plnum, 1, 1);

				// allow mice or other external controllers to add to the move
				IN_Move (cmd, plnum);

				// if we are spectator, try autocam
				if (cl.spectator)
					Cam_Track(plnum, cmd);

				CL_FinishMove(cmd, (int)(frametime*1000), plnum);

				Cam_FinishMove(plnum, cmd);
			}

			cls.netchan.outgoing_sequence++;
		}

		return; // sendcmds come from the demo
	}

	buf.maxsize = sizeof(data);
	buf.cursize = 0;
	buf.data = data;
	CL_SendDownloadReq(&buf);
	{
		clcmdbuf_t *next;
		while (clientcmdlist)
		{
			next = clientcmdlist->next;
			if (clientcmdlist->reliable)
			{
				MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
				MSG_WriteString (&cls.netchan.message, clientcmdlist->command);
			}
			else
			{
				MSG_WriteByte (&buf, clc_stringcmd);
				MSG_WriteString (&buf, clientcmdlist->command);
			}
			Con_DPrintf("Sending stringcmd %s\n", clientcmdlist->command);
			Z_Free(clientcmdlist);
			clientcmdlist = next;
		}
	}

	if (msecs>150)	//q2 has 200 slop.
		msecs=150;

	msecs += frametime*1000;
//	Con_Printf("%f\n", msecs);

	if (msecs<0)
		msecs=0;	//erm.


//	if (cls.state < ca_active)
//		msecs = 0;

	msecstouse = (int)msecs;	//casts round down.
	if (msecstouse > 255)
		msecstouse = 255;

	wantfps = cl_netfps.value<=0?cl_maxfps.value:cl_netfps.value;
	if (wantfps < cls.maxfps ? max (30.0, cls.maxfps) : 0x7fff)
		wantfps = cls.maxfps ? max (30.0, cls.maxfps) : 0x7fff;

	for (plnum = 0; plnum < cl.splitclients; plnum++)
	{
//		CL_BaseMove (&independantphysics[plnum], plnum, (msecstouse - independantphysics[plnum].msec), wantfps);
		CL_AdjustAngles (plnum);
		IN_Move (&independantphysics[plnum], plnum);
		
		for (i=0 ; i<3 ; i++)
			independantphysics[plnum].angles[i] = ((int)(cl.viewangles[plnum][i]*65536.0/360)&65535);

		if (!independantphysics[plnum].msec)
		{
			CL_BaseMove (&independantphysics[plnum], plnum, (msecstouse - independantphysics[plnum].msec), wantfps);
			CL_FinishMove(&independantphysics[plnum], msecstouse, plnum);
		}

		// if we are spectator, try autocam
	//	if (cl.spectator)
			Cam_Track(plnum, &independantphysics[plnum]);
		Cam_FinishMove(plnum, &independantphysics[plnum]);
		independantphysics[plnum].msec = msecstouse;
	}

	if (cl_netfps.value && !cl_indepphysics.value)
	{//this chunk of code is here to stop the client from using too few msecs per packet
		int spare;
		spare = CL_FilterTime(msecstouse, cl_netfps.value<=0?cl_maxfps.value:cl_netfps.value);
		if (!spare && msecstouse<255 && cls.state == ca_active)
		{
			return;
		}
		if (spare > 10)
			spare = 10;
		if (spare > 0)
		{
			msecstouse -= spare;
			for (plnum = 0; plnum < cl.splitclients; plnum++)
				independantphysics[plnum].msec = msecstouse;
		}
	}

#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE)
	{
		if (!cl.allowsendpacket)
			return;
		msecs -= msecstouse;

		i = cls.netchan.outgoing_sequence & UPDATE_MASK;
		cmd = &cl.frames[i].cmd[0];
		*cmd = independantphysics[0];
		cl.frames[i].senttime = realtime;
		cl.frames[i].receivedtime = 0;	// nq doesn't allow us to find our own packetloss

		CLNQ_SendCmd ();
		memset(&independantphysics[0], 0, sizeof(independantphysics[plnum]));
		return;
	}
#endif

#ifdef Q3CLIENT
	if (cls.protocol == CP_QUAKE3)
	{
		CLQ3_SendCmd(&independantphysics[0]);
		memset(&independantphysics[0], 0, sizeof(independantphysics[0]));
		return;
	}
#endif

//	Con_Printf("sending %i msecs\n", msecstouse);

	seq_hash = cls.netchan.outgoing_sequence;

// send this and the previous cmds in the message, so
// if the last packet was dropped, it can be recovered
	clientcount = cl.splitclients;

	if (!clientcount)
		clientcount = 1;
	if (1)	//wait for server data before sending clc_move stuff? nope, mvdsv doesn't like that.
	{
#ifdef Q2CLIENT
		if (cls.protocol == CP_QUAKE2)
		{
			i = cls.netchan.outgoing_sequence & UPDATE_MASK;
			cmd = &cl.frames[i].cmd[plnum];

			if (cls.resendinfo)
			{
				MSG_WriteByte (&cls.netchan.message, clcq2_userinfo);
				MSG_WriteString (&cls.netchan.message, cls.userinfo);

				cls.resendinfo = false;
			}


			MSG_WriteByte (&buf, clcq2_move);

			// save the position for a checksum qbyte
			checksumIndex = buf.cursize;
			MSG_WriteByte (&buf, 0);

			if (!cl.q2frame.valid || cl_nodelta.value)
				MSG_WriteLong (&buf, -1);	// no compression
			else
				MSG_WriteLong (&buf, cl.q2frame.serverframe);

			if (R_LightPoint)
				lightlev = R_LightPoint(cl.simorg[0]);
			else
				lightlev = 255;
		}
		else
#endif
		{
			MSG_WriteByte (&buf, clc_move);

			// save the position for a checksum qbyte
			checksumIndex = buf.cursize;
			MSG_WriteByte (&buf, 0);

			// write our lossage percentage
			lost = CL_CalcNet();
			MSG_WriteByte (&buf, (qbyte)lost);

			lightlev = 0;
		}
		msecs = msecs - (double)msecstouse;
		firstsize=0;
		for (plnum = 0; plnum<clientcount; plnum++)
		{
			i = cls.netchan.outgoing_sequence & UPDATE_MASK;
			cmd = &cl.frames[i].cmd[plnum];
			*cmd = independantphysics[plnum];
			cl.frames[i].senttime = realtime;
			cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet
			memset(&independantphysics[plnum], 0, sizeof(independantphysics[plnum]));

#ifdef Q2CLIENT
			if (cls.protocol == CP_QUAKE2 && cmd->buttons)
				cmd->buttons |= 128;	//fixme: this isn't really what's meant by the anykey.
#endif

			if (plnum)
				MSG_WriteByte (&buf, clc_move);

			i = (cls.netchan.outgoing_sequence-2) & UPDATE_MASK;
			cmd = &cl.frames[i].cmd[plnum];
			cmd->lightlevel = lightlev;
			if (cl_c2sImpulseBackup.value >= 2)
				dontdrop = dontdrop || cmd->impulse;
			MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
			oldcmd = cmd;

			i = (cls.netchan.outgoing_sequence-1) & UPDATE_MASK;
			if (cl_c2sImpulseBackup.value >= 3)
				dontdrop = dontdrop || cmd->impulse;
			cmd = &cl.frames[i].cmd[plnum];
			cmd->lightlevel = lightlev;
			MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
			oldcmd = cmd;

			i = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
			if (cl_c2sImpulseBackup.value >= 1)
				dontdrop = dontdrop || cmd->impulse;
			cmd = &cl.frames[i].cmd[plnum];
			cmd->lightlevel = lightlev;
			MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

			if (!firstsize)
				firstsize = buf.cursize;
		}

	// calculate a checksum over the move commands

#ifdef Q2CLIENT
		if (cls.protocol == CP_QUAKE2)
			buf.data[checksumIndex] = Q2COM_BlockSequenceCRCByte(
				buf.data + checksumIndex + 1, firstsize - checksumIndex - 1,
				seq_hash);
		else
#endif
			buf.data[checksumIndex] = COM_BlockSequenceCRCByte(
				buf.data + checksumIndex + 1, firstsize - checksumIndex - 1,
				seq_hash);
	}

	// request delta compression of entities
#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKEWORLD)
#endif
		if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP-1)
			cl.validsequence = 0;

	if (
#ifdef Q2CLIENT
		cls.protocol == CP_QUAKEWORLD && 
#endif
		cl.validsequence && !cl_nodelta.value && cls.state == ca_active &&
		!cls.demorecording)
	{
		cl.frames[cls.netchan.outgoing_sequence&UPDATE_MASK].delta_sequence = cl.validsequence;
		MSG_WriteByte (&buf, clc_delta);
//		Con_Printf("%i\n", cl.validsequence);
		MSG_WriteByte (&buf, cl.validsequence&255);
	}
	else
		cl.frames[cls.netchan.outgoing_sequence&UPDATE_MASK].delta_sequence = -1;

	i = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd[0];

	if (cls.demorecording)
		CL_WriteDemoCmd(cmd);

//shamelessly stolen from fuhquake
	if (cl_c2spps.value>0)
	{
		pps_balance += frametime;
		// never drop more than 2 messages in a row -- that'll cause PL
		// and don't drop if one of the last two movemessages have an impulse
		if (pps_balance > 0 || dropcount >= 2 || dontdrop)
		{
			float	pps;
			pps = cl_c2spps.value;
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
			cl.frames[i].receivedtime = -3;
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

static char	*VARGS vahunk(char *format, ...)
{
	va_list		argptr;
	char		string[1024];
	char *ret;
	
	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	ret = Hunk_Alloc(strlen(string)+1);
	strcpy(ret, string);

	return ret;	
}

void CL_RegisterSplitCommands(void)
{
	static int oldsplit;
	char spn[8];
	int sp;
	for (sp = 0; sp < MAX_SPLITS; sp++)
	{
		if (sp)
			sprintf(spn, "%i", sp+1);
		else
			*spn = '\0';
		if (sp < cl.splitclients)
		{
			if (oldsplit & (1<<sp))
				continue;
			oldsplit |= (1<<sp);

			Cmd_AddRemCommand (vahunk("+moveup%s",		spn),	IN_UpDown);
			Cmd_AddRemCommand (vahunk("-moveup%s",		spn),	IN_UpUp);
			Cmd_AddRemCommand (vahunk("+movedown%s",	spn),	IN_DownDown);
			Cmd_AddRemCommand (vahunk("-movedown%s",	spn),	IN_DownUp);
			Cmd_AddRemCommand (vahunk("+left%s",		spn),	IN_LeftDown);
			Cmd_AddRemCommand (vahunk("-left%s",		spn),	IN_LeftUp);
			Cmd_AddRemCommand (vahunk("+right%s",		spn),	IN_RightDown);
			Cmd_AddRemCommand (vahunk("-right%s",		spn),	IN_RightUp);
			Cmd_AddRemCommand (vahunk("+forward%s",		spn),	IN_ForwardDown);
			Cmd_AddRemCommand (vahunk("-forward%s",		spn),	IN_ForwardUp);
			Cmd_AddRemCommand (vahunk("+back%s",		spn),	IN_BackDown);
			Cmd_AddRemCommand (vahunk("-back%s",		spn),	IN_BackUp);
			Cmd_AddRemCommand (vahunk("+lookup%s",		spn),	IN_LookupDown);
			Cmd_AddRemCommand (vahunk("-lookup%s",		spn),	IN_LookupUp);
			Cmd_AddRemCommand (vahunk("+lookdown%s",	spn),	IN_LookdownDown);
			Cmd_AddRemCommand (vahunk("-lookdown%s",	spn),	IN_LookdownUp);
			Cmd_AddRemCommand (vahunk("+strafe%s",		spn),	IN_StrafeDown);
			Cmd_AddRemCommand (vahunk("-strafe%s",		spn),	IN_StrafeUp);
			Cmd_AddRemCommand (vahunk("+moveleft%s",	spn),	IN_MoveleftDown);
			Cmd_AddRemCommand (vahunk("-moveleft%s",	spn),	IN_MoveleftUp);
			Cmd_AddRemCommand (vahunk("+moveright%s",	spn),	IN_MoverightDown);
			Cmd_AddRemCommand (vahunk("-moveright%s",	spn),	IN_MoverightUp);
			Cmd_AddRemCommand (vahunk("+speed%s",		spn),	IN_SpeedDown);
			Cmd_AddRemCommand (vahunk("-speed%s",		spn),	IN_SpeedUp);
			Cmd_AddRemCommand (vahunk("+attack%s",		spn),	IN_AttackDown);
			Cmd_AddRemCommand (vahunk("-attack%s",		spn),	IN_AttackUp);
			Cmd_AddRemCommand (vahunk("+use%s",			spn),	IN_UseDown);
			Cmd_AddRemCommand (vahunk("-use%s",			spn),	IN_UseUp);
			Cmd_AddRemCommand (vahunk("+jump%s",		spn),	IN_JumpDown);
			Cmd_AddRemCommand (vahunk("-jump%s",		spn),	IN_JumpUp);
			Cmd_AddRemCommand (vahunk("impulse%s",		spn),	IN_Impulse);
			Cmd_AddRemCommand (vahunk("+klook%s",		spn),	IN_KLookDown);
			Cmd_AddRemCommand (vahunk("-klook%s",		spn),	IN_KLookUp);
			Cmd_AddRemCommand (vahunk("+mlook%s",		spn),	IN_MLookDown);
			Cmd_AddRemCommand (vahunk("-mlook%s",		spn),	IN_MLookUp);


			Cmd_AddRemCommand (vahunk("+button3%s",	spn),	IN_Button3Down);
			Cmd_AddRemCommand (vahunk("-button3%s",	spn),	IN_Button3Up);
			Cmd_AddRemCommand (vahunk("+button4%s",	spn),	IN_Button4Down);
			Cmd_AddRemCommand (vahunk("-button4%s",	spn),	IN_Button4Up);
			Cmd_AddRemCommand (vahunk("+button5%s",	spn),	IN_Button5Down);
			Cmd_AddRemCommand (vahunk("-button5%s",	spn),	IN_Button5Up);
			Cmd_AddRemCommand (vahunk("+button6%s",	spn),	IN_Button6Down);
			Cmd_AddRemCommand (vahunk("-button6%s",	spn),	IN_Button6Up);
			Cmd_AddRemCommand (vahunk("+button7%s",	spn),	IN_Button7Down);
			Cmd_AddRemCommand (vahunk("-button7%s",	spn),	IN_Button7Up);
			Cmd_AddRemCommand (vahunk("+button8%s",	spn),	IN_Button8Down);
			Cmd_AddRemCommand (vahunk("-button8%s",	spn),	IN_Button8Up);
		}
		else
		{
			if (!(oldsplit & (1<<sp)))
				continue;
			oldsplit &= ~(1<<sp);

			Cmd_RemoveCommand (vahunk("+moveup%s",		spn));
			Cmd_RemoveCommand (vahunk("-moveup%s",		spn));
			Cmd_RemoveCommand (vahunk("+movedown%s",	spn));
			Cmd_RemoveCommand (vahunk("-movedown%s",	spn));
			Cmd_RemoveCommand (vahunk("+left%s",		spn));
			Cmd_RemoveCommand (vahunk("-left%s",		spn));
			Cmd_RemoveCommand (vahunk("+right%s",		spn));
			Cmd_RemoveCommand (vahunk("-right%s",		spn));
			Cmd_RemoveCommand (vahunk("+forward%s",		spn));
			Cmd_RemoveCommand (vahunk("-forward%s",		spn));
			Cmd_RemoveCommand (vahunk("+back%s",		spn));
			Cmd_RemoveCommand (vahunk("-back%s",		spn));
			Cmd_RemoveCommand (vahunk("+lookup%s",		spn));
			Cmd_RemoveCommand (vahunk("-lookup%s",		spn));
			Cmd_RemoveCommand (vahunk("+lookdown%s",	spn));
			Cmd_RemoveCommand (vahunk("-lookdown%s",	spn));
			Cmd_RemoveCommand (vahunk("+strafe%s",		spn));
			Cmd_RemoveCommand (vahunk("-strafe%s",		spn));
			Cmd_RemoveCommand (vahunk("+moveleft%s",	spn));
			Cmd_RemoveCommand (vahunk("-moveleft%s",	spn));
			Cmd_RemoveCommand (vahunk("+moveright%s",	spn));
			Cmd_RemoveCommand (vahunk("-moveright%s",	spn));
			Cmd_RemoveCommand (vahunk("+speed%s",		spn));
			Cmd_RemoveCommand (vahunk("-speed%s",		spn));
			Cmd_RemoveCommand (vahunk("+attack%s",		spn));
			Cmd_RemoveCommand (vahunk("-attack%s",		spn));
			Cmd_RemoveCommand (vahunk("+use%s",			spn));
			Cmd_RemoveCommand (vahunk("-use%s",			spn));
			Cmd_RemoveCommand (vahunk("+jump%s",		spn));
			Cmd_RemoveCommand (vahunk("-jump%s",		spn));
			Cmd_RemoveCommand (vahunk("impulse%s",		spn));
			Cmd_RemoveCommand (vahunk("+klook%s",		spn));
			Cmd_RemoveCommand (vahunk("-klook%s",		spn));
			Cmd_RemoveCommand (vahunk("+mlook%s",		spn));
			Cmd_RemoveCommand (vahunk("-mlook%s",		spn));


			Cmd_RemoveCommand (vahunk("+button3%s",	spn));
			Cmd_RemoveCommand (vahunk("-button3%s",	spn));
			Cmd_RemoveCommand (vahunk("+button4%s",	spn));
			Cmd_RemoveCommand (vahunk("-button4%s",	spn));
			Cmd_RemoveCommand (vahunk("+button5%s",	spn));
			Cmd_RemoveCommand (vahunk("-button5%s",	spn));
			Cmd_RemoveCommand (vahunk("+button6%s",	spn));
			Cmd_RemoveCommand (vahunk("-button6%s",	spn));
			Cmd_RemoveCommand (vahunk("+button7%s",	spn));
			Cmd_RemoveCommand (vahunk("-button7%s",	spn));
			Cmd_RemoveCommand (vahunk("+button8%s",	spn));
			Cmd_RemoveCommand (vahunk("-button8%s",	spn));
		}
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
#define inputnetworkcvargroup "client networking options"
	cl.splitclients = 1;
	CL_RegisterSplitCommands();

	Cmd_AddCommand("rotate", IN_Rotate_f);
	Cmd_AddCommand("in_restart", IN_Restart);
	Cmd_AddCommand("sendcvar", CL_SendCvar_f);

	Cvar_Register (&cl_nodelta, inputnetworkcvargroup);

	Cvar_Register (&prox_inmenu, inputnetworkcvargroup);

	Cvar_Register (&cl_c2sImpulseBackup, inputnetworkcvargroup);
	Cvar_Register (&cl_c2spps, inputnetworkcvargroup);
	Cvar_Register (&cl_netfps, inputnetworkcvargroup);

	Cvar_Register (&cl_smartjump, inputnetworkcvargroup);

	Cvar_Register (&cl_prydoncursor, inputnetworkcvargroup);
	Cvar_Register (&cl_instantrotate, inputnetworkcvargroup);
}

/*
============
CL_ClearStates
============
*/
void CL_ClearStates (void)
{
}

