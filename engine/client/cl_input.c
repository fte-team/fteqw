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

cvar_t	cl_nodelta = {"cl_nodelta","0"};

cvar_t	cl_c2spps = {"cl_c2spps", "0"};
cvar_t	cl_c2sImpulseBackup = {"cl_c2sImpulseBackup","3"};

cvar_t	cl_netfps = {"cl_netfps", "74"};



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
void IN_JumpDown (void) {KeyDown(&in_jump);}
void IN_JumpUp (void) {KeyUp(&in_jump);}


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




//==========================================================================

cvar_t	cl_upspeed = {"cl_upspeed","200"};
cvar_t	cl_forwardspeed = {"cl_forwardspeed","200", NULL, CVAR_ARCHIVE};
cvar_t	cl_backspeed = {"cl_backspeed","200", NULL, CVAR_ARCHIVE};
cvar_t	cl_sidespeed = {"cl_sidespeed","350"};

cvar_t	cl_movespeedkey = {"cl_movespeedkey","2.0"};

cvar_t	cl_yawspeed = {"cl_yawspeed","140"};
cvar_t	cl_pitchspeed = {"cl_pitchspeed","150"};

cvar_t	cl_anglespeedkey = {"cl_anglespeedkey","1.5"};

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles (int pnum)
{
	float	speed;
	float	up, down;
	
	if (in_speed.state[pnum] & 1)
		speed = host_frametime * cl_anglespeedkey.value;
	else
		speed = host_frametime;

	if (!(in_strafe.state[pnum] & 1))
	{
		cl.viewangles[pnum][YAW] -= speed*cl_yawspeed.value*CL_KeyState (&in_right, pnum);
		cl.viewangles[pnum][YAW] += speed*cl_yawspeed.value*CL_KeyState (&in_left, pnum);
		cl.viewangles[pnum][YAW] = anglemod(cl.viewangles[pnum][YAW]);
	}
	if (in_klook.state[pnum] & 1)
	{
		V_StopPitchDrift (pnum);
		cl.viewangles[pnum][PITCH] -= speed*cl_pitchspeed.value * CL_KeyState (&in_forward, pnum);
		cl.viewangles[pnum][PITCH] += speed*cl_pitchspeed.value * CL_KeyState (&in_back, pnum);
	}
	
	up = CL_KeyState (&in_lookup, pnum);
	down = CL_KeyState(&in_lookdown, pnum);
	
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
void CL_BaseMove (usercmd_t *cmd, int pnum)
{
	CL_AdjustAngles (pnum);
	
	VectorCopy (cl.viewangles[pnum], cmd->angles);
	if (in_strafe.state[pnum] & 1)
	{
		cmd->sidemove += cl_sidespeed.value * CL_KeyState (&in_right, pnum);
		cmd->sidemove -= cl_sidespeed.value * CL_KeyState (&in_left, pnum);
	}

	cmd->sidemove += cl_sidespeed.value * CL_KeyState (&in_moveright, pnum);
	cmd->sidemove -= cl_sidespeed.value * CL_KeyState (&in_moveleft, pnum);

#ifdef IN_XFLIP
	if(in_xflip.value) cmd->sidemove *= -1;
#endif


	cmd->upmove += cl_upspeed.value * CL_KeyState (&in_up, pnum);
	cmd->upmove -= cl_upspeed.value * CL_KeyState (&in_down, pnum);

	if (! (in_klook.state[pnum] & 1) )
	{	
		cmd->forwardmove += cl_forwardspeed.value * CL_KeyState (&in_forward, pnum);
		cmd->forwardmove -= cl_backspeed.value * CL_KeyState (&in_back, pnum);
	}	

//
// adjust for speed key
//
	if (in_speed.state[pnum] & 1)
	{
		cmd->forwardmove *= cl_movespeedkey.value;
		cmd->sidemove *= cl_movespeedkey.value;
		cmd->upmove *= cl_movespeedkey.value;
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
	if (cls.q2server)
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
	int		ms, i;
	int bits;

//
// allways dump the first two message, because it may contain leftover inputs
// from the last level
//
	if (++cl.movemessages <= 2)
		return;
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
	ms = msecs;//host_frametime * 1000;
//	if (ms > 250)
//		ms = 100;		// time was unreasonable
	cmd->msec = ms;

	//VectorCopy (cl.viewangles, cmd->angles);
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


//
// chop down so no extra bits are kept that the server wouldn't get
//
	cmd->forwardmove = MakeChar (cmd->forwardmove);
	cmd->sidemove = MakeChar (cmd->sidemove);
	cmd->upmove = MakeChar (cmd->upmove);
}

cvar_t cl_prydoncursor = {"cl_prydoncursor", "0"};
void CL_UpdatePrydonCursor(float cursor_screen[2], vec3_t cursor_start, vec3_t cursor_impact, int *entnum)
{
	float modelview[16];
	vec3_t cursor_end;
	trace_t tr;

	vec3_t temp, scale;

	if (!cl_prydoncursor.value)
	{	//center the cursor
		cursor_screen[0] = 0;
		cursor_screen[1] = 0;
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

	scale[0] = -tan(r_refdef.fov_x * M_PI / 360.0);
	scale[1] = -tan(r_refdef.fov_y * M_PI / 360.0);
	scale[2] = 1;

	// trace distance
	VectorScale(scale, 1000000, scale);


	VectorCopy(cl.simorg[0], cursor_start);
	temp[0] = cursor_screen[2] * scale[2];
	temp[1] = cursor_screen[0] * scale[0];
	temp[2] = cursor_screen[1] * scale[1];

	ML_ModelViewMatrix(modelview, cl.viewangles[0], cl.simorg[0]);
	Matrix4_Transform3(modelview, temp, cursor_end);

	tr = PM_PlayerTrace(cursor_start, cursor_end);
	VectorCopy(tr.endpos, cursor_impact);
//	CL_SelectTraceLine(cursor_start, cursor_end, cursor_impact, entnum);
	// makes sparks where cursor is
	//CL_SparkShower(cl.cmd.cursor_impact, cl.cmd.cursor_normal, 5, 0);
}

#ifdef NQPROT
void CLNQ_SendMove (usercmd_t		*cmd, int pnum)
{
	int bits;
	int i;
	sizebuf_t	buf;
	qbyte	data[128];

	float cursor_screen[2];
	vec3_t cursor_start, cursor_impact;
	int cursor_entitynumber=0;//I hate warnings as errors
	
	buf.maxsize = 128;
	buf.cursize = 0;
	buf.data = data;

	MSG_WriteByte (&buf, clc_move);

	MSG_WriteFloat (&buf, cl.time);	// so server can get ping times

	for (i=0 ; i<3 ; i++)
		MSG_WriteAngle (&buf, cl.viewangles[pnum][i]);
	
	MSG_WriteShort (&buf, cmd->forwardmove);
	MSG_WriteShort (&buf, cmd->sidemove);
	MSG_WriteShort (&buf, cmd->upmove);

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

	if (nq_dp_protocol == 6)
	{
		CL_UpdatePrydonCursor(cursor_screen, cursor_start, cursor_impact, &cursor_entitynumber);
		MSG_WriteLong (&buf, bits);
	}
	else
		MSG_WriteByte (&buf, bits);

	if (in_impulsespending[pnum])
	{
		in_nextimpulse[pnum]++;
		in_impulsespending[pnum]--;
		MSG_WriteByte(&buf, in_impulse[pnum][(in_nextimpulse[pnum]-1)%IN_IMPULSECACHE]);
	}
	else
		MSG_WriteByte (&buf, 0);


	if (nq_dp_protocol == 6)
	{
		MSG_WriteShort (&buf, cursor_screen[0] * 32767.0f);
		MSG_WriteShort (&buf, cursor_screen[1] * 32767.0f);
		MSG_WriteFloat (&buf, cursor_start[0]);
		MSG_WriteFloat (&buf, cursor_start[1]);
		MSG_WriteFloat (&buf, cursor_start[2]);
		MSG_WriteFloat (&buf, cursor_impact[0]);
		MSG_WriteFloat (&buf, cursor_impact[1]);
		MSG_WriteFloat (&buf, cursor_impact[2]);
		MSG_WriteShort (&buf, cursor_entitynumber);
	}



//
// deliver the message
//
	if (cls.demoplayback!=DPB_NONE)
		return;	//err... don't bother... :)

//
// allways dump the first two message, because it may contain leftover inputs
// from the last level
//
	if (++cl.movemessages <= 2)
		return;
	
	if (NET_SendUnreliableMessage (cls.netcon, &buf) == -1)
	{
		Con_Printf ("CL_SendMove: lost server connection\n");
		CL_Disconnect ();
	}
}
void CLNQ_SendCmd(void)
{
	extern int cl_latestframenum, nq_dp_protocol;
	usercmd_t		cmd;

	if (cls.state <= ca_connected)
		return;

	if (cls.signon == 4)
	{
		memset(&cmd, 0, sizeof(cmd));
	// get basic movement from keyboard
		CL_BaseMove (&cmd, 0);
	
	// allow mice or other external controllers to add to the move
		IN_Move (&cmd, 0);
	
	// send the unreliable message
		CLNQ_SendMove (&cmd, 0);
	}

	if (name.modified)
	{
		name.modified = false;
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, va("name \"%s\"\n", name.string));
	}

	if (nq_dp_protocol > 0)
	{
		MSG_WriteByte(&cls.netchan.message, 50);
		MSG_WriteLong(&cls.netchan.message, cl_latestframenum);
	}

	
// send the reliable message
	if (!cls.netchan.message.cursize)
		return;		// no message at all
	
	if (!NET_CanSendMessage (cls.netcon))
	{
		Con_DPrintf ("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.netchan.message) == -1)
		Host_EndGame ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.netchan.message);
}
#endif


//returns result in the form of the 
void ComponantVectors(vec3_t angles, vec3_t move, vec3_t result, float multi)
{
	vec3_t f, r, u;
	AngleVectors(angles, f, r, u);

	result[0] = DotProduct (move, f)*multi;
	result[1] = DotProduct (move, r)*multi;
	result[2] = DotProduct (move, u)*multi;
}

void AddComponant(vec3_t angles, vec3_t dest, float fm, float rm, float um)
{
	vec3_t f, r, u;
	AngleVectors(angles, f, r, u);

	VectorMA(dest, fm, f, dest);
	VectorMA(dest, rm, r, dest);
	VectorMA(dest, um, u, dest);
}

#define bound(n,v,x) v<n?n:(v>x?x:v)
qboolean CL_Net_FilterTime (double time)
{
	extern cvar_t rate;
	float fps, fpscap;

	if (cls.timedemo) 
		return true;

	if (cls.demoplayback != DPB_NONE)
	{
		if (!cl_netfps.value)
			return true;
		fps = max (30.0, cl_netfps.value);
	}
	else
	{
		fpscap = cls.maxfps ? max (30.0, cls.maxfps) : 0x7fff;
	
		if (cl_netfps.value)
			fps = bound (10.0, cl_netfps.value, fpscap);
		else
		{
//			if (com_serveractive)
//				fps = fpscap;
//			else
				fps = bound (30.0, rate.value/80.0, fpscap);
		}
	}

	if (time < 1000 / fps)
		return false;

	return true;
}

/*
=================
CL_SendCmd
=================
*/
usercmd_t independantphysics[MAX_SPLITS];
vec3_t accum[MAX_SPLITS];
void CL_SendCmd (void)
{
	sizebuf_t	buf;
	qbyte		data[512];
	int			i, plnum;
	usercmd_t	*cmd, *oldcmd;
	int			checksumIndex;
	int			lost;
	int			seq_hash;
	int firstsize;
	int extramsec;
	vec3_t v;

	qbyte lightlev;

	static float	pps_balance = 0;
	static int	dropcount = 0;
	static float msecs;
	int msecstouse;
	qboolean	dontdrop=false;

	int clientcount;

	if (cls.demoplayback != DPB_NONE)
	{
		if (cls.demoplayback == DPB_MVD)
		{
			i = cls.netchan.outgoing_sequence & UPDATE_MASK;
			cl.frames[i].senttime = realtime;		// we haven't gotten a reply yet
			cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet
			cmd = &cl.frames[i].cmd[0];

			memset(cmd, 0, sizeof(*cmd));
			cmd->msec = host_frametime*1000;
			independantphysics[0].msec = 0;

				// get basic movement from keyboard
			CL_BaseMove (cmd, 0);

			// allow mice or other external controllers to add to the move
			IN_Move (cmd, 0);

			// if we are spectator, try autocam
			if (cl.spectator)
				Cam_Track(0, cmd);

			CL_FinishMove(cmd, (int)(host_frametime*1000), 0);

			Cam_FinishMove(0, cmd);

			cls.netchan.outgoing_sequence++;
		}

		return; // sendcmds come from the demo
	}

#ifdef NQPROT
	if (cls.netcon && !cls.netcon->qwprotocol)
	{
		CLNQ_SendCmd ();
		return;
	}
#endif

	msecs += host_frametime*1000;
//	Con_Printf("%f\n", msecs);

	if (msecs>1000)	//come on... That's just stupid.
		msecs=255;
	if (msecs<0)
		msecs=0;	//erm.

	msecstouse = (int)msecs;	//casts round down.

	if (!CL_Net_FilterTime(msecstouse) && msecstouse<255)
	{
		usercmd_t new;

		for (plnum = 0; plnum < cl.splitclients; plnum++)
		{
			cmd = &new;
			memset(cmd, 0, sizeof(new));

		// get basic movement from keyboard
			CL_BaseMove (cmd, plnum);

		// allow mice or other external controllers to add to the move
			IN_Move (cmd, plnum);

			if (cl.spectator)
				Cam_Track(plnum, cmd);

			cmd->msec = msecstouse;
			extramsec = msecstouse - independantphysics[plnum].msec;

			//acumulate this frame.
			AddComponant(cl.viewangles[plnum], accum[plnum], cmd->forwardmove*extramsec, cmd->sidemove*extramsec, cmd->upmove*extramsec);

			//evaluate from accum
			ComponantVectors(cl.viewangles[plnum], accum[plnum], v, 1.0f/msecstouse);
			independantphysics[plnum].forwardmove	= v[0];//MakeChar(v[0]);
			independantphysics[plnum].sidemove		= v[1];//MakeChar(v[1]);
			independantphysics[plnum].upmove		= v[2];//MakeChar(v[2]);

			for (i=0 ; i<3 ; i++)
				independantphysics[plnum].angles[i] = ((int)(cl.viewangles[plnum][i]*65536.0/360)&65535);

			independantphysics[plnum].msec = msecstouse;
			independantphysics[plnum].buttons |= cmd->buttons;
		}
		return;
	}

	if (msecstouse > 255)
		msecstouse = 255;

	for (plnum = 0; plnum < cl.splitclients; plnum++)
	{
		// save this command off for prediction
		i = cls.netchan.outgoing_sequence & UPDATE_MASK;
		cmd = &cl.frames[i].cmd[plnum];
		memcpy(cmd, &independantphysics[plnum], sizeof(*cmd));
		cl.frames[i].senttime = realtime;
		cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet

		memset(&independantphysics[plnum], 0, sizeof(independantphysics[plnum]));
	}

	seq_hash = cls.netchan.outgoing_sequence;

// send this and the previous cmds in the message, so
// if the last packet was dropped, it can be recovered
	buf.maxsize = 128;
	buf.cursize = 0;
	buf.data = data;
	clientcount = cl.splitclients;
	if (!clientcount)
		clientcount = 1;
	if (1)	//wait for server data before sending clc_move stuff
	{
#ifdef Q2CLIENT
		if (cls.q2server)
		{
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
		msecs -= msecstouse;
		firstsize=0;
		for (plnum = 0; plnum<clientcount; plnum++)
		{
			i = cls.netchan.outgoing_sequence & UPDATE_MASK;
			cmd = &cl.frames[i].cmd[plnum];

			// get basic movement from keyboard
			CL_BaseMove (cmd, plnum);

			// allow mice or other external controllers to add to the move
			IN_Move (cmd, plnum);

			/*
			if (cl_minmsec.value>200)
				cl_minmsec.value=200;

			if (!(msecstouse > cl_minmsec.value))
			{
				cmd->msec = msecstouse;
				for (i=0 ; i<3 ; i++)
					cmd->angles[i] = ((int)(cl.viewangles[i]*65536.0/360)&65535);
				cmd->forwardmove = MakeChar (cmd->forwardmove);
				cmd->sidemove = MakeChar (cmd->sidemove);
				cmd->upmove = MakeChar (cmd->upmove);

				if (!dropcount)
					cls.netchan.outgoing_sequence++;
				dropcount = true;
				return;
			}
			else*/

			// if we are spectator, try autocam
			if (cl.spectator)
				Cam_Track(plnum, cmd);

			CL_FinishMove(cmd, msecstouse, plnum);

			Cam_FinishMove(plnum, cmd);

			for (i=0 ; i<3 ; i++)
				cmd->angles[i] = ((int)(cl.viewangles[plnum][i]*65536.0/360)&65535);

			extramsec = msecstouse - independantphysics[plnum].msec;
			//add this frame to accum
			AddComponant(cl.viewangles[plnum], accum[plnum], cmd->forwardmove*extramsec, cmd->sidemove*extramsec, cmd->upmove*extramsec);

			//evaluate from accum
			ComponantVectors(cl.viewangles[plnum], accum[plnum], v, 1.0f/msecstouse);
			cmd->forwardmove	= v[0];
			cmd->sidemove		= v[1];
			cmd->upmove		= v[2];

			memset(accum[plnum], 0, sizeof(accum[plnum]));	//clear accum

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
		if (cls.q2server)
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
	if (!cls.q2server)
#endif
		if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP-1)
			cl.validsequence = 0;

	if (
#ifdef Q2CLIENT
		!cls.q2server && 
#endif
		cl.validsequence && !cl_nodelta.value && cls.state == ca_active &&
		!cls.demorecording)
	{
		cl.frames[cls.netchan.outgoing_sequence&UPDATE_MASK].delta_sequence = cl.validsequence;
		MSG_WriteByte (&buf, clc_delta);
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
		pps_balance += host_frametime;
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
	_vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	ret = Hunk_Alloc(strlen(string)+1);
	strcpy(ret, string);

	return ret;	
}


/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	int sp;
	char spn[8];
	qboolean nosplits = COM_CheckParm("-nosplit");
#define inputnetworkcvargroup "client networking options"
	//controls for player2
	for (sp = MAX_SPLITS-1; sp >=0; sp--)
	{
		if (sp)
		{
			if (nosplits)
				continue;
			sprintf(spn, "%i", sp+1);
		}
		else
			*spn = '\0';

		Cmd_AddCommand (vahunk("+moveup%s",		spn),	IN_UpDown);
		Cmd_AddCommand (vahunk("-moveup%s",		spn),	IN_UpUp);
		Cmd_AddCommand (vahunk("+movedown%s",	spn),	IN_DownDown);
		Cmd_AddCommand (vahunk("-movedown%s",	spn),	IN_DownUp);
		Cmd_AddCommand (vahunk("+left%s",		spn),	IN_LeftDown);
		Cmd_AddCommand (vahunk("-left%s",		spn),	IN_LeftUp);
		Cmd_AddCommand (vahunk("+right%s",		spn),	IN_RightDown);
		Cmd_AddCommand (vahunk("-right%s",		spn),	IN_RightUp);
		Cmd_AddCommand (vahunk("+forward%s",	spn),	IN_ForwardDown);
		Cmd_AddCommand (vahunk("-forward%s",	spn),	IN_ForwardUp);
		Cmd_AddCommand (vahunk("+back%s",		spn),	IN_BackDown);
		Cmd_AddCommand (vahunk("-back%s",		spn),	IN_BackUp);
		Cmd_AddCommand (vahunk("+lookup%s",		spn),	IN_LookupDown);
		Cmd_AddCommand (vahunk("-lookup%s",		spn),	IN_LookupUp);
		Cmd_AddCommand (vahunk("+lookdown%s",	spn),	IN_LookdownDown);
		Cmd_AddCommand (vahunk("-lookdown%s",	spn),	IN_LookdownUp);
		Cmd_AddCommand (vahunk("+strafe%s",		spn),	IN_StrafeDown);
		Cmd_AddCommand (vahunk("-strafe%s",		spn),	IN_StrafeUp);
		Cmd_AddCommand (vahunk("+moveleft%s",	spn),	IN_MoveleftDown);
		Cmd_AddCommand (vahunk("-moveleft%s",	spn),	IN_MoveleftUp);
		Cmd_AddCommand (vahunk("+moveright%s",	spn),	IN_MoverightDown);
		Cmd_AddCommand (vahunk("-moveright%s",	spn),	IN_MoverightUp);
		Cmd_AddCommand (vahunk("+speed%s",		spn),	IN_SpeedDown);
		Cmd_AddCommand (vahunk("-speed%s",		spn),	IN_SpeedUp);
		Cmd_AddCommand (vahunk("+attack%s",		spn),	IN_AttackDown);
		Cmd_AddCommand (vahunk("-attack%s",		spn),	IN_AttackUp);
		Cmd_AddCommand (vahunk("+use%s",		spn),	IN_UseDown);
		Cmd_AddCommand (vahunk("-use%s",		spn),	IN_UseUp);
		Cmd_AddCommand (vahunk("+jump%s",		spn),	IN_JumpDown);
		Cmd_AddCommand (vahunk("-jump%s",		spn),	IN_JumpUp);
		Cmd_AddCommand (vahunk("impulse%s",		spn),	IN_Impulse);
		Cmd_AddCommand (vahunk("+klook%s",		spn),	IN_KLookDown);
		Cmd_AddCommand (vahunk("-klook%s",		spn),	IN_KLookUp);
		Cmd_AddCommand (vahunk("+mlook%s",		spn),	IN_MLookDown);
		Cmd_AddCommand (vahunk("-mlook%s",		spn),	IN_MLookUp);


		Cmd_AddCommand (vahunk("+button3%s",	spn),	IN_Button3Down);
		Cmd_AddCommand (vahunk("-button3%s",	spn),	IN_Button3Up);
		Cmd_AddCommand (vahunk("+button4%s",	spn),	IN_Button4Down);
		Cmd_AddCommand (vahunk("-button4%s",	spn),	IN_Button4Up);
		Cmd_AddCommand (vahunk("+button5%s",	spn),	IN_Button5Down);
		Cmd_AddCommand (vahunk("-button5%s",	spn),	IN_Button5Up);
		Cmd_AddCommand (vahunk("+button6%s",	spn),	IN_Button6Down);
		Cmd_AddCommand (vahunk("-button6%s",	spn),	IN_Button6Up);
		Cmd_AddCommand (vahunk("+button7%s",	spn),	IN_Button7Down);
		Cmd_AddCommand (vahunk("-button7%s",	spn),	IN_Button7Up);
		Cmd_AddCommand (vahunk("+button8%s",	spn),	IN_Button8Down);
		Cmd_AddCommand (vahunk("-button8%s",	spn),	IN_Button8Up);
	}

	Cvar_Register (&cl_nodelta, inputnetworkcvargroup);

	Cvar_Register (&cl_c2sImpulseBackup, inputnetworkcvargroup);
	Cvar_Register (&cl_c2spps, inputnetworkcvargroup);
	Cvar_Register (&cl_netfps, inputnetworkcvargroup);
}

/*
============
CL_ClearStates
============
*/
void CL_ClearStates (void)
{
}

