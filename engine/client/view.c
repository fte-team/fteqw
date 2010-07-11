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
// view.c -- player eye positioning

#include "quakedef.h"

#include "winquake.h"

#ifdef FISH
void R_RenderView_fisheye(void);
cvar_t ffov = SCVAR("ffov", "0");
cvar_t fviews = SCVAR("fviews", "6");
#endif

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

#ifdef SIDEVIEWS
cvar_t	vsec_enabled[SIDEVIEWS] = {SCVAR("v2_enabled", "2"),	SCVAR("v3_enabled", "0"),	SCVAR("v4_enabled", "0"),	SCVAR("v5_enabled", "0")};
cvar_t	vsec_x[SIDEVIEWS]		= {SCVAR("v2_x", "0"),			SCVAR("v3_x", "0.25"),		SCVAR("v4_x", "0.5"),		SCVAR("v5_x", "0.75")};
cvar_t	vsec_y[SIDEVIEWS]		= {SCVAR("v2_y", "0"),			SCVAR("v3_y", "0"),			SCVAR("v4_y", "0"),			SCVAR("v5_y", "0")};
cvar_t	vsec_scalex[SIDEVIEWS]	= {SCVAR("v2_scalex", "0.25"),	SCVAR("v3_scalex", "0.25"),	SCVAR("v4_scalex", "0.25"),	SCVAR("v5_scalex", "0.25")};
cvar_t	vsec_scaley[SIDEVIEWS]	= {SCVAR("v2_scaley", "0.25"),	SCVAR("v3_scaley", "0.25"),	SCVAR("v4_scaley", "0.25"),	SCVAR("v5_scaley", "0.25")};
cvar_t	vsec_yaw[SIDEVIEWS]		= {SCVAR("v2_yaw", "180"),		SCVAR("v3_yaw", "90"),		SCVAR("v4_yaw", "270"),		SCVAR("v5_yaw", "0")};
#endif

cvar_t	lcd_x = SCVAR("lcd_x", "0");	// FIXME: make this work sometime...

cvar_t	cl_rollspeed = SCVAR("cl_rollspeed", "200");
cvar_t	cl_rollangle = SCVAR("cl_rollangle", "2.0");
cvar_t	v_deathtilt = SCVAR("v_deathtilt", "1");

cvar_t	cl_bob = SCVAR("cl_bob","0.02");
cvar_t	cl_bobcycle = SCVAR("cl_bobcycle","0.6");
cvar_t	cl_bobup = SCVAR("cl_bobup","0.5");

cvar_t	v_kicktime = SCVAR("v_kicktime", "0.5");
cvar_t	v_kickroll = SCVAR("v_kickroll", "0.6");
cvar_t	v_kickpitch = SCVAR("v_kickpitch", "0.6");

cvar_t	v_iyaw_cycle = SCVAR("v_iyaw_cycle", "2");
cvar_t	v_iroll_cycle = SCVAR("v_iroll_cycle", "0.5");
cvar_t	v_ipitch_cycle = SCVAR("v_ipitch_cycle", "1");
cvar_t	v_iyaw_level = SCVAR("v_iyaw_level", "0.3");
cvar_t	v_iroll_level = SCVAR("v_iroll_level", "0.1");
cvar_t	v_ipitch_level = SCVAR("v_ipitch_level", "0.3");
cvar_t	v_idlescale = SCVAR("v_idlescale", "0");

cvar_t	crosshair = SCVARF("crosshair", "0", CVAR_ARCHIVE);
cvar_t	crosshaircolor = SCVARF("crosshaircolor", "79", CVAR_ARCHIVE | CVAR_RENDERERCALLBACK);
cvar_t	crosshairsize = SCVARF("crosshairsize", "8", CVAR_ARCHIVE);

cvar_t  cl_crossx = SCVARF("cl_crossx", "0", CVAR_ARCHIVE);
cvar_t  cl_crossy = SCVARF("cl_crossy", "0", CVAR_ARCHIVE);
cvar_t	crosshaircorrect = SCVARF("crosshaircorrect", "0", CVAR_SEMICHEAT);
cvar_t	crosshairimage = SCVARF("crosshairimage", "", CVAR_RENDERERCALLBACK);
cvar_t	crosshairalpha = SCVAR("crosshairalpha", "1");

cvar_t	gl_cshiftpercent = SCVAR("gl_cshiftpercent", "100");
cvar_t	gl_cshiftenabled = SCVAR("gl_polyblend", "1");

cvar_t	v_bonusflash = SCVAR("v_bonusflash", "1");

cvar_t  v_contentblend = SCVARF("v_contentblend", "1", CVAR_ARCHIVE);
cvar_t	v_damagecshift = SCVAR("v_damagecshift", "1");
cvar_t	v_quadcshift = SCVAR("v_quadcshift", "1");
cvar_t	v_suitcshift = SCVAR("v_suitcshift", "1");
cvar_t	v_ringcshift = SCVAR("v_ringcshift", "1");
cvar_t	v_pentcshift = SCVAR("v_pentcshift", "1");
cvar_t	v_gunkick = SCVAR("v_gunkick", "0");

cvar_t	v_viewheight = SCVAR("v_viewheight", "0");

cvar_t	scr_autoid = SCVAR("scr_autoid", "1");


extern cvar_t cl_chasecam;

float	v_dmg_time[MAX_SPLITS], v_dmg_roll[MAX_SPLITS], v_dmg_pitch[MAX_SPLITS];

extern	int			in_forward, in_forward2, in_back;

frame_t		*view_frame;
player_state_t		*view_message;

/*
===============
V_CalcRoll

===============
*/
float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t	forward, right, up;
	float	sign;
	float	side;
	float	value;
	
	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	
	value = cl_rollangle.value;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;
	
	return side*sign;
	
}


/*
===============
V_CalcBob

===============
*/
float V_CalcBob (int pnum)
{
	static	double	bobtime[MAX_SPLITS];
	static float	bob[MAX_SPLITS];
	float	cycle;
	
	if (cl.spectator)
		return 0;

	if (!cl.onground[pnum] || cl.paused)
		return bob[pnum];		// just use old value

	if (cl_bobcycle.value <= 0)
		return 0;

	bobtime[pnum] += host_frametime;
	cycle = bobtime[pnum] - (int)(bobtime[pnum]/cl_bobcycle.value)*cl_bobcycle.value;
	cycle /= cl_bobcycle.value;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

// bob is proportional to simulated velocity in the xy plane
// (don't count Z, or jumping messes it up)

	bob[pnum] = sqrt(cl.simvel[pnum][0]*cl.simvel[pnum][0] + cl.simvel[pnum][1]*cl.simvel[pnum][1]) * cl_bob.value;
	bob[pnum] = bob[pnum]*0.3 + bob[pnum]*0.7*sin(cycle);
	if (bob[pnum] > 4)
		bob[pnum] = 4;
	else if (bob[pnum] < -7)
		bob[pnum] = -7;
	return bob[pnum];
	
}


//=============================================================================


cvar_t	v_centermove = SCVAR("v_centermove", "0.15");
cvar_t	v_centerspeed = SCVAR("v_centerspeed","500");


void V_StartPitchDrift (int pnum)
{
#if 1
	if (cl.laststop[pnum] == cl.time)
	{
		return;		// something else is keeping it from drifting
	}
#endif
	if (cl.nodrift || !cl.pitchvel)
	{
		cl.pitchvel[pnum] = v_centerspeed.value;
		cl.nodrift[pnum] = false;
		cl.driftmove[pnum] = 0;
	}
}

void V_StopPitchDrift (int pnum)
{
	cl.laststop[pnum] = cl.time;
	cl.nodrift[pnum] = true;
	cl.pitchvel[pnum] = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when 
===============
*/
void V_DriftPitch (int pnum)
{
	float		delta, move;

	if (!cl.onground || cls.demoplayback )
	{
		cl.driftmove[pnum] = 0;
		cl.pitchvel[pnum] = 0;
		return;
	}

// don't count small mouse motion
	if (cl.nodrift[pnum])
	{
		if ( fabs(cl.frames[(cls.netchan.outgoing_sequence-1)&UPDATE_MASK].cmd[pnum].forwardmove) < 200)
			cl.driftmove[pnum] = 0;
		else
			cl.driftmove[pnum] += host_frametime;
	
		if ( cl.driftmove[pnum] > v_centermove.value)
		{
			V_StartPitchDrift (pnum);
		}
		return;
	}
	
	delta = 0 - cl.viewangles[pnum][PITCH];

	if (!delta)
	{
		cl.pitchvel[pnum] = 0;
		return;
	}

	move = host_frametime * cl.pitchvel[pnum];
	cl.pitchvel[pnum] += host_frametime * v_centerspeed.value;
	
//Con_Printf ("move: %f (%f)\n", move, host_frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel[pnum] = 0;
			move = delta;
		}
		cl.viewangles[pnum][PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel[pnum] = 0;
			move = -delta;
		}
		cl.viewangles[pnum][PITCH] -= move;
	}
}





/*
============================================================================== 
 
						PALETTE FLASHES 
 
============================================================================== 
*/ 
 
 
cshift_t	cshift_empty = { {130,80,50}, 0 };
cshift_t	cshift_water = { {130,80,50}, 128 };
cshift_t	cshift_slime = { {0,25,5}, 150 };
cshift_t	cshift_lava = { {255,80,0}, 150 };

cshift_t	cshift_server = { {130,80,50}, 0 };

cvar_t		v_gamma = SCVARF("gamma", "0.8", CVAR_ARCHIVE|CVAR_RENDERERCALLBACK);
cvar_t		v_contrast = SCVARF("contrast", "1.4", CVAR_ARCHIVE);

qbyte		gammatable[256];	// palette is sent through this


unsigned short		ramps[3][256];
//extern qboolean		gammaworks;
float		sw_blend[4];		// rgba 0.0 - 1.0
float		hw_blend[4];		// rgba 0.0 - 1.0
/*
void BuildGammaTable (float g)
{
	int		i, inf;
	
	if (g == 1.0)
	{
		for (i=0 ; i<256 ; i++)
			gammatable[i] = i;
		return;
	}
	
	for (i=0 ; i<256 ; i++)
	{
		inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		gammatable[i] = inf;
	}
}*/
void BuildGammaTable (float g, float c)
{
	int i, inf;

//	g = bound (0.1, g, 3);
//	c = bound (1, c, 3);

	if (g == 1 && c == 1)
	{
		for (i = 0; i < 256; i++)
			gammatable[i] = i;
		return;
	}

	for (i = 0; i < 256; i++)
	{
		inf = 255 * pow ((i + 0.5) / 255.5 * c, g) + 0.5;
		if (inf < 0)
			inf = 0;
		else if (inf > 255)
			inf = 255;		
		gammatable[i] = inf;
	}
}

/*
=================
V_CheckGamma
=================
*/
#if defined(GLQUAKE) || defined(D3DQUAKE)
void GLV_Gamma_Callback(struct cvar_s *var, char *oldvalue)
{
	BuildGammaTable (v_gamma.value, v_contrast.value);
	vid.recalc_refdef = 1; // force a surface cache flush
	GLV_UpdatePalette (true, 0);
}
#endif

qboolean V_CheckGamma (void)
{
	return false;
}



/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (int pnum)
{
	int		armor, blood;
	vec3_t	from;
	int		i;
	vec3_t	forward, right, up;
	float	side;
	float	count;
	
	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	if (v_damagecshift.value >= 0)
		count *= v_damagecshift.value;

	cl.faceanimtime[pnum] = cl.time + 0.2;		// but sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood)		
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

//
// calculate view angle kicks
//
	VectorSubtract (from, cl.simorg[pnum], from);
	VectorNormalize (from);
	
	AngleVectors (cl.simangles[pnum], forward, right, up);

	side = DotProduct (from, right);
	v_dmg_roll[pnum] = count*side*v_kickroll.value;
	
	side = DotProduct (from, forward);
	v_dmg_pitch[pnum] = count*side*v_kickpitch.value;

	v_dmg_time[pnum] = v_kicktime.value;
}


/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
	if (Cmd_Argc() != 5 && Cmd_Argc() != 1)	//this is actually to warn of a malice bug (and prevent a totally black screen) more than it is to help the user. :/
	{										//The 1 is so teamfortress can use it to clear.
		if (Cmd_FromGamecode())	//nehahra does nasty things and becomes unplayable.
		{
			cl.cshifts[CSHIFT_SERVER].destcolor[0] = 0;
			cl.cshifts[CSHIFT_SERVER].destcolor[1] = 0;
			cl.cshifts[CSHIFT_SERVER].destcolor[2] = 0;
			cl.cshifts[CSHIFT_SERVER].percent = 0;
		}
		Con_Printf("v_cshift: v_cshift <r> <g> <b> <alpha>\n");
		return;
	}
	if (Cmd_FromGamecode())
	{
		cl.cshifts[CSHIFT_SERVER].destcolor[0] = atoi(Cmd_Argv(1));
		cl.cshifts[CSHIFT_SERVER].destcolor[1] = atoi(Cmd_Argv(2));
		cl.cshifts[CSHIFT_SERVER].destcolor[2] = atoi(Cmd_Argv(3));
		cl.cshifts[CSHIFT_SERVER].percent = atoi(Cmd_Argv(4));
		return;
	}
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	if (v_bonusflash.value || !Cmd_FromGamecode())
	{
		cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
		cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
		cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
		cl.cshifts[CSHIFT_BONUS].percent = 50*v_bonusflash.value;
	}
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift

FIXME: Uses Q1 contents
=============
*/
void V_SetContentsColor (int contents)
{
	int i;
	switch (contents)
	{
	case Q1CONTENTS_EMPTY:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	case Q1CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;
	case Q1CONTENTS_SOLID:
	case Q1CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;
	default:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}

	cl.cshifts[CSHIFT_CONTENTS].percent *= v_contentblend.value;

	if (cl.cshifts[CSHIFT_SERVER].percent)
	{	//bound contents so it can't go negative
		if (cl.cshifts[CSHIFT_CONTENTS].percent < 0)
			cl.cshifts[CSHIFT_CONTENTS].percent = 0;

		for (i = 0; i < 3; i++)
			if (cl.cshifts[CSHIFT_CONTENTS].destcolor[0] < 0)
				cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 0;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
	int im = 0;
	int s;

	//we only have one palette, so combine the mask

	for (s = 0; s < cl.splitclients; s++)
		im |= cl.stats[s][STAT_ITEMS];

	if (im & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30*v_quadcshift.value;
	}
	else if (im & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20*v_suitcshift.value;
	}
	else if (im & IT_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100*v_ringcshift.value;
	}
	else if (im & IT_INVULNERABILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30*v_pentcshift.value;
	}
	else
		cl.cshifts[CSHIFT_POWERUP].percent = 0;

	if (cl.cshifts[CSHIFT_POWERUP].percent<0)
		cl.cshifts[CSHIFT_POWERUP].percent=0;
}


/*
=============
V_CalcBlend
=============
*/
#if defined(GLQUAKE) || defined(D3DQUAKE)

void GLV_CalcBlend (float *hw_blend)
{
	float	a2;
	int		j;
	float *blend;

	memset(hw_blend, 0, sizeof(float)*4);
	memset(sw_blend, 0, sizeof(float)*4);

	//don't apply it to the server, we'll blend the two later if the user has no hardware gamma (if they do have it, we use just the server specified value) This way we avoid winnt users having a cheat with flashbangs and stuff.
	for (j=0 ; j<NUM_CSHIFTS ; j++)	
	{
		if (j != CSHIFT_SERVER)
		{
			if (!gl_cshiftpercent.value || !gl_cshiftenabled.ival)
				continue;

			a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;
		}
		else
		{
			a2 = cl.cshifts[j].percent / 255.0;	//don't allow modification of this one.
		}

		if (!a2)
			continue;

		if (j == CSHIFT_SERVER)
		{
			/*server blend always goes into sw, ALWAYS*/
			blend = sw_blend;
		}
		else
		{
			if (/*j == CSHIFT_BONUS || j == CSHIFT_DAMAGE ||*/ gl_nohwblend.ival)
				blend = sw_blend;
			else	//powerup or contents?
				blend = hw_blend;
		}

		blend[3] = blend[3] + a2*(1-blend[3]);
		a2 = a2/blend[3];
		blend[0] = blend[0]*(1-a2) + cl.cshifts[j].destcolor[0]*a2/255.0;
		blend[1] = blend[1]*(1-a2) + cl.cshifts[j].destcolor[1]*a2/255.0;
		blend[2] = blend[2]*(1-a2) + cl.cshifts[j].destcolor[2]*a2/255.0;
	}

	if (hw_blend[3] > 1)
		hw_blend[3] = 1;
	if (hw_blend[3] < 0)
		hw_blend[3] = 0;
	if (sw_blend[3] > 1)
		sw_blend[3] = 1;
	if (sw_blend[3] < 0)
		sw_blend[3] = 0;
}

/*
=============
V_UpdatePalette
=============
*/
void GLV_UpdatePalette (qboolean force, double ftime)
{
	int		i;
	float	newhw_blend[4];
	int		ir, ig, ib;

	RSpeedMark();

	V_CalcPowerupCshift ();

// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= ftime*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= ftime*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	GLV_CalcBlend(newhw_blend);

	if (hw_blend[0] != newhw_blend[0] || hw_blend[1] != newhw_blend[1] || hw_blend[2] != newhw_blend[2] || hw_blend[3] != newhw_blend[3] || force)
	{
		float r,g,b,a;
		Vector4Copy(newhw_blend, hw_blend);

		a = hw_blend[3];
		r = 255*hw_blend[0]*a;
		g = 255*hw_blend[1]*a;
		b = 255*hw_blend[2]*a;

		a = 1-a;
		for (i=0 ; i<256 ; i++)
		{
			ir = i*a + r;
			ig = i*a + g;
			ib = i*a + b;
			if (ir > 255)
				ir = 255;
			if (ig > 255)
				ig = 255;
			if (ib > 255)
				ib = 255;

			ramps[0][i] = gammatable[ir]<<8;
			ramps[1][i] = gammatable[ig]<<8;
			ramps[2][i] = gammatable[ib]<<8;
		}

		VID_ShiftPalette (NULL);
	}

	RSpeedEnd(RSPEED_PALETTEFLASHES);
}
#endif
/*
=============
V_UpdatePalette
=============
*/

void V_ClearCShifts (void)
{
	int i;

	for (i = 0; i < NUM_CSHIFTS; i++)
		cl.cshifts[i].percent = 0;
}

/* 
============================================================================== 
 
						VIEW RENDERING 
 
============================================================================== 
*/ 

float angledelta (float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}

/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle (int pnum)
{	
	float	yaw, pitch, move;
	static float oldyaw = 0;
	static float oldpitch = 0;
	
	yaw = r_refdef.viewangles[YAW];
	pitch = -r_refdef.viewangles[PITCH];

	yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
	if (yaw > 10)
		yaw = 10;
	if (yaw < -10)
		yaw = -10;
	pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
	if (pitch > 10)
		pitch = 10;
	if (pitch < -10)
		pitch = -10;
	move = host_frametime*20;
	if (yaw > oldyaw)
	{
		if (oldyaw + move < yaw)
			yaw = oldyaw + move;
	}
	else
	{
		if (oldyaw - move > yaw)
			yaw = oldyaw - move;
	}
	
	if (pitch > oldpitch)
	{
		if (oldpitch + move < pitch)
			pitch = oldpitch + move;
	}
	else
	{
		if (oldpitch - move > pitch)
			pitch = oldpitch - move;
	}
	
	oldyaw = yaw;
	oldpitch = pitch;

	cl.viewent[pnum].angles[YAW] = r_refdef.viewangles[YAW] + yaw;
	cl.viewent[pnum].angles[PITCH] = - (r_refdef.viewangles[PITCH] + pitch);

	cl.viewent[pnum].angles[PITCH]*=-1;
	AngleVectors(cl.viewent[pnum].angles, cl.viewent[pnum].axis[0], cl.viewent[pnum].axis[1], cl.viewent[pnum].axis[2]);
	VectorInverse(cl.viewent[pnum].axis[1]);
	cl.viewent[pnum].angles[PITCH]*=-1;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (int pnum)
{
// absolutely bound refresh reletive to entity clipping hull
// so the view can never be inside a solid wall

	if (r_refdef.vieworg[0] < cl.simorg[pnum][0] - 14)
		r_refdef.vieworg[0] = cl.simorg[pnum][0] - 14;
	else if (r_refdef.vieworg[0] > cl.simorg[pnum][0] + 14)
		r_refdef.vieworg[0] = cl.simorg[pnum][0] + 14;
	if (r_refdef.vieworg[1] < cl.simorg[pnum][1] - 14)
		r_refdef.vieworg[1] = cl.simorg[pnum][1] - 14;
	else if (r_refdef.vieworg[1] > cl.simorg[pnum][1] + 14)
		r_refdef.vieworg[1] = cl.simorg[pnum][1] + 14;
	if (r_refdef.vieworg[2] < cl.simorg[pnum][2] - 22)
		r_refdef.vieworg[2] = cl.simorg[pnum][2] - 22;
	else if (r_refdef.vieworg[2] > cl.simorg[pnum][2] + 30)
		r_refdef.vieworg[2] = cl.simorg[pnum][2] + 30;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (int pnum)
{
	//defaults: for use if idlescale is locked and the var isn't.
	float yaw_cycle		= 2;
	float roll_cycle	= 0.5;
	float pitch_cycle	= 1;
	float yaw_level		= 0.3;
	float roll_level	= 0.1;
	float pitch_level	= 0.3;

	if (v_iyaw_cycle.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		yaw_cycle = v_iyaw_cycle.value;
	if (v_iroll_cycle.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		roll_cycle = v_iroll_cycle.value;
	if (v_ipitch_cycle.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		pitch_cycle = v_ipitch_cycle.value;

	if (v_iyaw_level.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		yaw_level = v_iyaw_level.value;
	if (v_iroll_level.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		roll_level = v_iroll_level.value;
	if (v_ipitch_level.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		pitch_level = v_ipitch_level.value;

	r_refdef.viewangles[ROLL] += v_idlescale.value * sin(cl.time*roll_cycle) * roll_level;
	r_refdef.viewangles[PITCH] += v_idlescale.value * sin(cl.time*pitch_cycle) * pitch_level;
	r_refdef.viewangles[YAW] += v_idlescale.value * sin(cl.time*yaw_cycle) * yaw_level;

	cl.viewent[pnum].angles[ROLL] -= v_idlescale.value * sin(cl.time*roll_cycle) * roll_level;
	cl.viewent[pnum].angles[PITCH] -= v_idlescale.value * sin(cl.time*pitch_cycle) * pitch_level;
	cl.viewent[pnum].angles[YAW] -= v_idlescale.value * sin(cl.time*yaw_cycle) * yaw_level;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (int pnum)
{
	float		side;
	float	adjspeed;

	side = V_CalcRoll (cl.simangles[pnum], cl.simvel[pnum]);

	adjspeed = fabs(cl_rollangle.value);
	if (adjspeed<1)
		adjspeed=1;
	if (adjspeed>45)
		adjspeed = 45;
	adjspeed*=20;
	if (side > cl.rollangle[pnum])
	{
		cl.rollangle[pnum] += host_frametime * adjspeed;
		if (cl.rollangle[pnum] > side)
			cl.rollangle[pnum] = side;
	}
	else if (side < cl.rollangle[pnum])
	{
		cl.rollangle[pnum] -= host_frametime * adjspeed;
		if (cl.rollangle[pnum] < side)
			cl.rollangle[pnum] = side;
	}
	r_refdef.viewangles[ROLL] += cl.rollangle[pnum];

	if (v_dmg_time[pnum] > 0)
	{
		r_refdef.viewangles[ROLL] += v_dmg_time[pnum]/v_kicktime.value*v_dmg_roll[pnum];
		r_refdef.viewangles[PITCH] += v_dmg_time[pnum]/v_kicktime.value*v_dmg_pitch[pnum];
		v_dmg_time[pnum] -= host_frametime;
	}

}


/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef (int pnum)
{
	entity_t	*view;
	float		old;

// view is the weapon model
	view = &cl.viewent[pnum];

	VectorCopy (cl.simorg[pnum], r_refdef.vieworg);
	VectorCopy (cl.simangles[pnum], r_refdef.viewangles);
	view->model = NULL;

// always idle in intermission
	old = v_idlescale.value;
	v_idlescale.value = 1;
	V_AddIdle (pnum);
	v_idlescale.value = old;
}

/*
==================
V_CalcRefdef

==================
*/
void V_CalcRefdef (int pnum)
{
	entity_t	*view;
	int			i;
	vec3_t		forward, right, up;
	float		bob;

	r_refdef.currentplayernum = pnum;

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
		return;
#endif

// view is the weapon model (only visible from inside body)
	view = &cl.viewent[pnum];

	if (v_viewheight.value < -7)
		bob=-7;
	else if (v_viewheight.value > 4)
		bob=4;
	else if (v_viewheight.value)
		bob=v_viewheight.value;
	else
		bob = V_CalcBob (pnum);
	
// refresh position from simulated origin
	VectorCopy (cl.simorg[pnum], r_refdef.vieworg);

	r_refdef.useperspective = true;

// never let it sit exactly on a node line, because a water plane can
// dissapear when viewed with the eye exactly on it.
// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
	r_refdef.vieworg[0] += 1.0/16;
	r_refdef.vieworg[1] += 1.0/16;
	r_refdef.vieworg[2] += 1.0/16;

	VectorCopy (cl.simangles[pnum], r_refdef.viewangles);
	V_CalcViewRoll (pnum);
	V_AddIdle (pnum);

	if (view_message && view_message->flags & PF_GIB)
		r_refdef.vieworg[2] += 8;	// gib view height
	else if (view_message && view_message->flags & PF_DEAD)
		r_refdef.vieworg[2] -= 16;	// corpse view height
	else
		r_refdef.vieworg[2] += cl.viewheight[pnum];

	r_refdef.vieworg[2] += cl.crouch[pnum];

	if (view_message && view_message->flags & PF_DEAD && v_deathtilt.value)		// PF_GIB will also set PF_DEAD
	{
		if (!cl.spectator || !cl_chasecam.ival)
			r_refdef.viewangles[ROLL] = 80*v_deathtilt.value;	// dead view angle
	}
	else
	{
		// v_viewheight only affects the view if the player is alive
		r_refdef.vieworg[2] += bob;
	}

// offsets
	AngleVectors (cl.simangles[pnum], forward, right, up);
	
// set up gun position
	VectorCopy (cl.simangles[pnum], view->angles);
	
	CalcGunAngle (pnum);

	VectorCopy (r_refdef.vieworg, view->origin);

	for (i=0 ; i<3 ; i++)
	{
		view->origin[i] += forward[i]*bob*0.4;
//		view->origin[i] += right[i]*sin(cl.time*5.5342452354235)*0.1;
//		view->origin[i] += up[i]*bob*0.8;
	}

// fudge position around to keep amount of weapon visible
// roughly equal with different FOV
	if (scr_viewsize.value == 110)
		view->origin[2] += 1;
	else if (scr_viewsize.value == 100)
		view->origin[2] += 2;
	else if (scr_viewsize.value == 90)
		view->origin[2] += 1;
	else if (scr_viewsize.value == 80)
		view->origin[2] += 0.5;

	if (!view_message || view_message->flags & (PF_GIB|PF_DEAD) || (unsigned int)cl.stats[pnum][STAT_WEAPON] >= MAX_MODELS)
 		view->model = NULL;
 	else
		view->model = cl.model_precache[cl.stats[pnum][STAT_WEAPON]];
#ifdef HLCLIENT
	if (!CLHL_AnimateViewEntity(view))
#endif
		view->framestate.g[FS_REG].frame[0] = view_message?view_message->weaponframe:0;

// set up the refresh position
	if (v_gunkick.value)
		r_refdef.viewangles[PITCH] += cl.punchangle[pnum]*v_gunkick.value;

	r_refdef.time = realtime;

// smooth out stair step ups


	{
		extern model_t *loadmodel;
		loadmodel = cl.worldmodel;
	}
}

/*
=============
DropPunchAngle
=============
*/
void DropPunchAngle (int pnum)
{
	if (cl.punchangle[pnum] < 0)
	{
		cl.punchangle[pnum] += 10*host_frametime;
		if (cl.punchangle[pnum] > 0)
			cl.punchangle[pnum] = 0;
	}
	else
	{
		cl.punchangle[pnum] -= 10*host_frametime;
		if (cl.punchangle[pnum] < 0)
			cl.punchangle[pnum] = 0;
	}
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
extern vrect_t scr_vrect;

int gl_ztrickdisabled;
qboolean r_secondaryview;
#ifdef SIDEVIEWS

#ifdef PEXT_VIEW2
entity_t *CL_EntityNum(int num)
{
	int i;
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		if (cl_visedicts[i].keynum == num)
			return &cl_visedicts[i];
	}
	return NULL;
}
#endif
#endif

float CalcFov (float fov_x, float width, float height);
void SCR_VRectForPlayer(vrect_t *vrect, int pnum)
{
#if MAX_SPLITS > 4
#pragma warning "Please change this function to cope with the new MAX_SPLITS value"
#endif
	switch(cl.splitclients)
	{
	case 1:
		vrect->width = scr_vrect.width;
		vrect->height = scr_vrect.height;
		vrect->x = scr_vrect.x;
		vrect->y = scr_vrect.y;

		if (scr_chatmode == 2)
		{
			vrect->height/=2;
			vrect->y += vrect->height;
		}
		break;

	case 2:	//horizontal bands
	case 3:
#ifdef GLQUAKE
		if (qrenderer == QR_OPENGL && vid.pixelwidth > vid.pixelheight * 2)
		{	//over twice as wide as high, assume duel moniter, horizontal.
			vrect->width = vid.width/cl.splitclients;
			vrect->height = vid.height;
			vrect->x = 0 + vrect->width*pnum;
			vrect->y = 0;
		}
		else
#endif
		{
			vrect->width = vid.width;
			vrect->height = vid.height/cl.splitclients;
			vrect->x = 0;
			vrect->y = 0 + vrect->height*pnum;
		}

		break;

	case 4:	//4 squares
		vrect->width = vid.width/2;
		vrect->height = vid.height/2;
		vrect->x = (pnum&1) * vrect->width;
		vrect->y = (pnum&2)/2 * vrect->height;
		break;

	default:
		Sys_Error("cl.splitclients is invalid.");
	}

	r_refdef.fov_x = scr_fov.value;
	if (cl.stats[pnum][STAT_VIEWZOOM])
		r_refdef.fov_x *= cl.stats[pnum][STAT_VIEWZOOM]/255.0f;

	if (vrect->width < (vrect->height*640)/432)
	{
		r_refdef.fov_y = CalcFov(r_refdef.fov_x, (vrect->width*vid.pixelwidth)/vid.width, (vrect->height*vid.pixelheight)/vid.height);
//		r_refdef.fov_x = CalcFov(r_refdef.fov_y, 432, 640);
	}
	else
	{
		r_refdef.fov_y = CalcFov(r_refdef.fov_x, 640, 432);
		r_refdef.fov_x = CalcFov(r_refdef.fov_y, vrect->height, vrect->width);
	}
}

void Draw_ExpandedString(int x, int y, conchar_t *str);
extern vec3_t nametagorg[MAX_CLIENTS];
extern qboolean nametagseen[MAX_CLIENTS];
void R_DrawNameTags(void)
{
	conchar_t buffer[256];
	int i;
	int len;
	vec3_t center;
	vec3_t tagcenter;

	if (!cl.spectator && !cls.demoplayback)
		return;
	if (!scr_autoid.ival)
		return;
	if (cls.state != ca_active || !cl.validsequence)
		return;

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		void GL_Set2D (void);
		GL_Set2D();
	}
#endif

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!nametagseen[i])
			continue;
		if (i == cl.playernum[r_refdef.currentplayernum])
			continue;	// Don't draw tag for the local player

		if (TP_IsPlayerVisible(nametagorg[i]))
		{
			VectorCopy(nametagorg[i], tagcenter);
			tagcenter[2] += 32;
			Matrix4_Project(tagcenter, center, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y);
			if (center[2] > 1)
				continue;

			len = COM_ParseFunString(CON_WHITEMASK, cl.players[i].name, buffer, sizeof(buffer), false) - buffer;
			Draw_ExpandedString(center[0]*r_refdef.vrect.width+r_refdef.vrect.x - len*4, (1-center[1])*r_refdef.vrect.height+r_refdef.vrect.y, buffer);
		}
	}
}

void V_RenderPlayerViews(int plnum)
{
	int oldnuments;
#ifdef SIDEVIEWS
	int viewnum;
#endif
	SCR_VRectForPlayer(&r_refdef.vrect, plnum);
	view_message = &view_frame->playerstate[cl.playernum[plnum]];
#ifdef NQPROT
	if (cls.protocol == CP_NETQUAKE)
		view_message->weaponframe = cl.stats[0][STAT_WEAPONFRAME];
#endif
	cl.simangles[plnum][ROLL] = 0;	// FIXME @@@ 


	DropPunchAngle (plnum);
	if (cl.intermission)
	{	// intermission / finale rendering
		V_CalcIntermissionRefdef (plnum);	
	}
	else
	{
		V_DriftPitch (plnum);
		V_CalcRefdef (plnum);
	}

	oldnuments = cl_numvisedicts;
	CL_LinkViewModel ();

	Cam_SelfTrack(plnum);
	{
		R_RenderView ();
		R_DrawNameTags();
	}

	cl_numvisedicts = oldnuments;

	if (scr_chatmode == 2)
	{
		extern vec3_t desired_position[MAX_SPLITS];
		vec3_t dir;
		extern void vectoangles(vec3_t vec, vec3_t ang);

		gl_ztrickdisabled|=16;
		r_refdef.vrect.y -= r_refdef.vrect.height;
		vid.recalc_refdef=true;
		r_secondaryview = 2;

		VectorSubtract(r_refdef.vieworg, desired_position[plnum], dir);
		vectoangles(dir, r_refdef.viewangles);
		r_refdef.viewangles[0] = -r_refdef.viewangles[0];	//flip the pitch. :(


		VectorCopy(desired_position[plnum], r_refdef.vieworg);
		R_RenderView ();
		vid.recalc_refdef=true;
	}
	else
		gl_ztrickdisabled&=~16;


#ifdef SIDEVIEWS
/*	//adjust main view height to strip off the rearviews at the top
	if (vsecwidth >= 1)
	{		
		r_refdef.vrect.y -= vsecheight;
		r_refdef.vrect.height += vsecheight;
	}
*/
#ifdef GLQUAKE
	gl_ztrickdisabled&=~1;
#endif
	for (viewnum = 0; viewnum < SIDEVIEWS; viewnum++)
	if (vsec_scalex[viewnum].value>0&&vsec_scaley[viewnum].value>0
		&& ((vsec_enabled[viewnum].value && vsec_enabled[viewnum].value != 2 && cls.allow_rearview) 	//rearview if v2_enabled = 1 and not 2
		|| (vsec_enabled[viewnum].value && cl.stats[plnum][STAT_VIEW2]&&viewnum==0)))			//v2 enabled if v2_enabled is non-zero
	{
		vrect_t oldrect;
		vec3_t oldangles;
		vec3_t oldposition;
//		int oldviewent;
		struct entity_s *e;
		float ofx;
		float ofy;

		gl_ztrickdisabled|=1;
		vid.recalc_refdef=true;

		r_secondaryview = true;

		if (vsec_x[viewnum].value < 0)
			vsec_x[viewnum].value = 0;
		if (vsec_y[viewnum].value < 0)
			vsec_y[viewnum].value = 0;

		if (vsec_scalex[viewnum].value+vsec_x[viewnum].value > 1)
			continue;
		if (vsec_scaley[viewnum].value+vsec_y[viewnum].value > 1)
			continue;

		oldrect = r_refdef.vrect;
		memcpy(oldangles, r_refdef.viewangles, sizeof(vec3_t));
		memcpy(oldposition, r_refdef.vieworg, sizeof(vec3_t));
		ofx = r_refdef.fov_x;
		ofy = r_refdef.fov_y;

		r_refdef.vrect.x += r_refdef.vrect.width*vsec_x[viewnum].value;
		r_refdef.vrect.y += r_refdef.vrect.height*vsec_y[viewnum].value;
		r_refdef.vrect.width *= vsec_scalex[viewnum].value;
		r_refdef.vrect.height *= vsec_scaley[viewnum].value;
#ifdef PEXT_VIEW2
			//secondary view entity.
		e=NULL;
		if (viewnum==0&&cl.stats[plnum][STAT_VIEW2])
		{
			e = CL_EntityNum (cl.stats[plnum][STAT_VIEW2]);
		}
		if (e)
		{
			float s;
			memcpy(r_refdef.viewangles, e->angles, sizeof(vec3_t));				
			memcpy(r_refdef.vieworg, e->origin, sizeof(vec3_t));				
//				cl.viewentity = cl.viewentity2;

//				s =	(realtime - e->lerptime)*10;
//				if (s > 1) s=1;
			s=0;
			r_refdef.vieworg[0]=r_refdef.vieworg[0];//*s+(1-s)*e->lerporigin[0];
			r_refdef.vieworg[1]=r_refdef.vieworg[1];//*s+(1-s)*e->lerporigin[1];
			r_refdef.vieworg[2]=r_refdef.vieworg[2];//*s+(1-s)*e->lerporigin[2];

			r_refdef.viewangles[0]=e->angles[0];//*s+(1-s)*e->msg_angles[1][0];
			r_refdef.viewangles[1]=e->angles[1];//*s+(1-s)*e->msg_angles[1][1];
			r_refdef.viewangles[2]=e->angles[2];//*s+(1-s)*e->msg_angles[1][2];
			r_refdef.viewangles[PITCH] *= -1;

			r_refdef.externalview = true;	//show the player

			R_RenderView ();
//				r_framecount = old_framecount;
		}
		else
#endif
		{
			//rotate the view, keeping pitch and roll.
			r_refdef.viewangles[YAW] += vsec_yaw[viewnum].value;
			r_refdef.viewangles[ROLL] += sin(vsec_yaw[viewnum].value / 180 * 3.14) * r_refdef.viewangles[PITCH];
			r_refdef.viewangles[PITCH] *= -cos((vsec_yaw[viewnum].value / 180 * 3.14)+3.14);
			if (vsec_enabled[viewnum].value!=2)
			{
				R_RenderView ();
			}
		}

		r_refdef.vrect = oldrect;
		memcpy(r_refdef.viewangles, oldangles, sizeof(vec3_t));		
		memcpy(r_refdef.vieworg, oldposition, sizeof(vec3_t));
		r_refdef.fov_x = ofx;
		r_refdef.fov_y = ofy;

		vid.recalc_refdef=true;
	}
#endif
	r_refdef.externalview = false;
}

void V_RenderView (void)
{
	int viewnum;
#ifdef PEXT_BULLETENS
	//avoid redoing the bulleten boards for rear view as well.
	static qboolean alreadyrendering = false;
#endif

	R_LessenStains();

	if (cls.state != ca_active)
		return;

	if (r_worldentity.model)
	{
		RSpeedMark();

		CL_AllowIndependantSendCmd(false);

		//work out which packet entities are solid
		CL_SetSolidEntities ();

		CL_EmitEntities();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(false);

		// do client side motion prediction
		CL_PredictMove ();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(true);

		// build a refresh entity list
//		CL_EmitEntities ();

		CL_AllowIndependantSendCmd(true);

		RSpeedEnd(RSPEED_LINKENTITIES);
	}

	view_frame = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];

	R_PushDlights ();


#ifdef PEXT_BULLETENS
	if (!alreadyrendering)
		R_SetupBulleten ();
	alreadyrendering=true;
#endif

	if (cl.splitclients>1)
		gl_ztrickdisabled|=8;
	else
		gl_ztrickdisabled&=~8;

	r_secondaryview = 0;
	for (viewnum = 0; viewnum < cl.splitclients; viewnum++)
	{
		V_RenderPlayerViews(viewnum);
	}

#ifdef PEXT_BULLETENS
	alreadyrendering=false;
#endif
}

//============================================================================

/*
=============
V_Init
=============
*/
void V_Init (void)
{
#define VIEWVARS "View variables"
#ifdef SIDEVIEWS
	int i;
#endif
	Cmd_AddCommand ("v_cshift", V_cshift_f);	
	Cmd_AddCommand ("bf", V_BonusFlash_f);
//	Cmd_AddCommand ("centerview", V_StartPitchDrift);

	Cvar_Register (&v_centermove, VIEWVARS);
	Cvar_Register (&v_centerspeed, VIEWVARS);

	Cvar_Register (&v_idlescale, VIEWVARS);
	Cvar_Register (&v_iyaw_cycle, VIEWVARS);
	Cvar_Register (&v_iroll_cycle, VIEWVARS);
	Cvar_Register (&v_ipitch_cycle, VIEWVARS);
	Cvar_Register (&v_iyaw_level, VIEWVARS);
	Cvar_Register (&v_iroll_level, VIEWVARS);
	Cvar_Register (&v_ipitch_level, VIEWVARS);

	Cvar_Register (&v_contentblend, VIEWVARS);
	Cvar_Register (&v_damagecshift, VIEWVARS);
	Cvar_Register (&v_quadcshift, VIEWVARS);
	Cvar_Register (&v_suitcshift, VIEWVARS);
	Cvar_Register (&v_ringcshift, VIEWVARS);
	Cvar_Register (&v_pentcshift, VIEWVARS);
	Cvar_Register (&v_gunkick, VIEWVARS);

	Cvar_Register (&v_bonusflash, VIEWVARS);

	Cvar_Register (&v_viewheight, VIEWVARS);

	Cvar_Register (&crosshaircolor, VIEWVARS);
	Cvar_Register (&crosshair, VIEWVARS);
	Cvar_Register (&crosshairsize, VIEWVARS);
	Cvar_Register (&crosshaircorrect, VIEWVARS);
	Cvar_Register (&crosshairimage, VIEWVARS);
	Cvar_Register (&crosshairalpha, VIEWVARS);
	Cvar_Register (&cl_crossx, VIEWVARS);
	Cvar_Register (&cl_crossy, VIEWVARS);
	Cvar_Register (&gl_cshiftpercent, VIEWVARS);
	Cvar_Register (&gl_cshiftenabled, VIEWVARS);

	Cvar_Register (&cl_rollspeed, VIEWVARS);
	Cvar_Register (&cl_rollangle, VIEWVARS);
	Cvar_Register (&cl_bob, VIEWVARS);
	Cvar_Register (&cl_bobcycle, VIEWVARS);
	Cvar_Register (&cl_bobup, VIEWVARS);

	Cvar_Register (&v_kicktime, VIEWVARS);
	Cvar_Register (&v_kickroll, VIEWVARS);
	Cvar_Register (&v_kickpitch, VIEWVARS);

	Cvar_Register (&v_deathtilt, VIEWVARS);

	Cvar_Register (&scr_autoid, VIEWVARS);

#ifdef SIDEVIEWS
#define SECONDARYVIEWVARS "Secondary view vars"
	for (i = 0; i < SIDEVIEWS; i++)
	{
		Cvar_Register (&vsec_enabled[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_x[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_y[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_scalex[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_scaley[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_yaw[i], SECONDARYVIEWVARS);
	}
#endif

#ifdef FISH
	Cvar_Register (&ffov, VIEWVARS);
	Cvar_Register (&fviews, VIEWVARS);
#endif

	BuildGammaTable (1.0, 1.0);	// no gamma yet
	Cvar_Register (&v_gamma, VIEWVARS);
	Cvar_Register (&v_contrast, VIEWVARS);
}
