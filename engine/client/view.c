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

#ifdef SWQUAKE
#include "r_local.h"
#endif

#include "winquake.h"

#ifdef FISH
void R_RenderView_fisheye(void);
cvar_t ffov = {"ffov", "160"};
cvar_t fviews = {"fviews", "6"};
#endif

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

#ifdef SIDEVIEWS
cvar_t	vsec_enabled[SIDEVIEWS] = {{"v2_enabled", "1"},		{"v3_enabled", "0"},	{"v4_enabled", "0"},	{"v5_enabled", "0"}};
cvar_t	vsec_x[SIDEVIEWS]		= {{"v2_x", "0"},			{"v3_x", "0.25"},		{"v4_x", "0.5"},		{"v5_x", "0.75"}};
cvar_t	vsec_y[SIDEVIEWS]		= {{"v2_y", "0"},			{"v3_y", "0"},			{"v4_y", "0"},			{"v5_y", "0"}};
cvar_t	vsec_scalex[SIDEVIEWS]	= {{"v2_scalex", "0.25"},	{"v3_scalex", "0.25"},	{"v4_scalex", "0.25"},	{"v5_scalex", "0.25"}};
cvar_t	vsec_scaley[SIDEVIEWS]	= {{"v2_scaley", "0.25"},	{"v3_scaley", "0.25"},	{"v4_scaley", "0.25"},	{"v5_scaley", "0.25"}};
cvar_t	vsec_yaw[SIDEVIEWS]		= {{"v2_yaw", "180"},		{"v3_yaw", "90"},		{"v4_yaw", "270"},		{"v5_yaw", "0"}};
#endif

cvar_t	lcd_x = {"lcd_x", "0"};	// FIXME: make this work sometime...

cvar_t	cl_rollspeed = {"cl_rollspeed", "200"};
cvar_t	cl_rollangle = {"cl_rollangle", "2.0"};

cvar_t	cl_bob = {"cl_bob","0.02"};
cvar_t	cl_bobcycle = {"cl_bobcycle","0.6"};
cvar_t	cl_bobup = {"cl_bobup","0.5"};

cvar_t	v_kicktime = {"v_kicktime", "0.5"};
cvar_t	v_kickroll = {"v_kickroll", "0.6"};
cvar_t	v_kickpitch = {"v_kickpitch", "0.6"};

cvar_t	v_iyaw_cycle = {"v_iyaw_cycle", "2", NULL};
cvar_t	v_iroll_cycle = {"v_iroll_cycle", "0.5", NULL};
cvar_t	v_ipitch_cycle = {"v_ipitch_cycle", "1", NULL};
cvar_t	v_iyaw_level = {"v_iyaw_level", "0.3", NULL};
cvar_t	v_iroll_level = {"v_iroll_level", "0.1", NULL};
cvar_t	v_ipitch_level = {"v_ipitch_level", "0.3", NULL};
cvar_t	v_idlescale = {"v_idlescale", "0", NULL};

cvar_t	crosshair = {"crosshair", "0", NULL, CVAR_ARCHIVE};
cvar_t	crosshaircolor = {"crosshaircolor", "79", NULL, CVAR_ARCHIVE};
cvar_t	crosshairsize = {"crosshairsize", "8", NULL, CVAR_ARCHIVE};

cvar_t  cl_crossx = {"cl_crossx", "0", NULL, CVAR_ARCHIVE};
cvar_t  cl_crossy = {"cl_crossy", "0", NULL, CVAR_ARCHIVE};
cvar_t	crosshaircorrect = {"crosshaircorrect", "0", NULL, CVAR_SEMICHEAT};

cvar_t	gl_cshiftpercent = {"gl_cshiftpercent", "100"};

cvar_t	v_bonusflash = {"v_bonusflash", "0"};

cvar_t  v_contentblend = {"v_contentblend", "0"};
cvar_t	v_damagecshift = {"v_damagecshift", "0"};
cvar_t	v_quadcshift = {"v_quadcshift", "0"};
cvar_t	v_suitcshift = {"v_suitcshift", "0"};
cvar_t	v_ringcshift = {"v_ringcshift", "0"};
cvar_t	v_pentcshift = {"v_pentcshift", "0"};

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


cvar_t	v_centermove = {"v_centermove", "0.15"};
cvar_t	v_centerspeed = {"v_centerspeed","500"};


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

cvar_t		v_gamma = {"gamma", "1", NULL, CVAR_ARCHIVE};
cvar_t		v_contrast = {"contrast", "1", NULL, CVAR_ARCHIVE};

qbyte		gammatable[256];	// palette is sent through this


unsigned short		ramps[3][256];
extern qboolean		gammaworks;
float		v_blend[4];		// rgba 0.0 - 1.0
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

	if (g == 1 && c == 1) {
		for (i = 0; i < 256; i++)
			gammatable[i] = i;
		return;
	}

	for (i = 0; i < 256; i++) {
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
qboolean V_CheckGamma (void)
{
	if (v_gamma.modified || v_contrast.modified)
	{
		v_contrast.modified = false;
		v_gamma.modified = false;

		BuildGammaTable (v_gamma.value, v_contrast.value);
		vid.recalc_refdef = 1;				// force a surface cache flush
		
		return true;
	}
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
	if (Cmd_FromServer())
	{
		cl.cshifts[CSHIFT_SERVER].destcolor[0] = atoi(Cmd_Argv(1));
		cl.cshifts[CSHIFT_SERVER].destcolor[1] = atoi(Cmd_Argv(2));
		cl.cshifts[CSHIFT_SERVER].destcolor[2] = atoi(Cmd_Argv(3));
		cl.cshifts[CSHIFT_SERVER].percent = atoi(Cmd_Argv(4));

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
	if (v_bonusflash.value || !Cmd_FromServer())
	{
		cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
		cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
		cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
		cl.cshifts[CSHIFT_BONUS].percent = 50;
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
#if defined(RGLQUAKE)
void GLV_CalcBlend (void)
{
	float	r, g, b, a, a2;
	int		j;

	r = 0;
	g = 0;
	b = 0;
	a = 0;

	for (j=0 ; j<NUM_CSHIFTS ; j++)	
	{
		if (j != CSHIFT_SERVER)
		{
			if (!gl_cshiftpercent.value || !gl_polyblend.value)
				continue;

			a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;
		}
		else
		{
			a2 = cl.cshifts[j].percent / 255.0;
		}

//		a2 = (cl.cshifts[j].percent/2)/255.0;
		if (!a2)
			continue;
		a = a + a2*(1-a);
//Con_Printf ("j:%i a:%f\n", j, a);
		a2 = a2/a;
		r = r*(1-a2) + cl.cshifts[j].destcolor[0]*a2;
		g = g*(1-a2) + cl.cshifts[j].destcolor[1]*a2;
		b = b*(1-a2) + cl.cshifts[j].destcolor[2]*a2;
	}

	v_blend[0] = r/255.0;
	v_blend[1] = g/255.0;
	v_blend[2] = b/255.0;
	v_blend[3] = a;
	if (v_blend[3] > 1)
		v_blend[3] = 1;
	if (v_blend[3] < 0)
		v_blend[3] = 0;
}

/*
=============
V_UpdatePalette
=============
*/
void GLV_UpdatePalette (void)
{
	qboolean ogw;
	int		i, j;
	qboolean	new;
//	qbyte	*basepal, *newpal;
//	qbyte	pal[768];
	float	r,g,b,a;
	int		ir, ig, ib;
	qboolean force;
	extern cvar_t vid_hardwaregamma;

	float hwg;

	RSpeedMark();

	V_CalcPowerupCshift ();

// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	new = false;

	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
		{
			new = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
			{
				new = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
	}

	force = V_CheckGamma ();

	hwg = vid_hardwaregamma.value;
	if (vid_hardwaregamma.modified && !hwg)
	{
		vid_hardwaregamma.value = hwg;
		force = true;
	}

	if (!new && !force)
	{
		RSpeedEnd(RSPEED_PALETTEFLASHES);
		return;
	}

	GLV_CalcBlend ();

//Con_Printf("b: %4.2f %4.2f %4.2f %4.6f\n", v_blend[0],	v_blend[1],	v_blend[2],	v_blend[3]);

	a = v_blend[3];
	r = 255*v_blend[0]*a;
	g = 255*v_blend[1]*a;
	b = 255*v_blend[2]*a;

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

	ogw = gammaworks;
	VID_ShiftPalette (NULL);
	vid_hardwaregamma.value = hwg;
	if (ogw != gammaworks)
	{
		Con_DPrintf("Gamma working state %i\n", gammaworks);
	}

	RSpeedEnd(RSPEED_PALETTEFLASHES);
}
#endif
/*
=============
V_UpdatePalette
=============
*/
#ifdef SWQUAKE
void SWV_UpdatePalette (void)
{
	int		i, j;
	qboolean	new;
	qbyte	*basepal, *newpal;
	qbyte	pal[768];
	int		r,g,b;
	qboolean force;

	V_CalcPowerupCshift ();
	
	new = false;
	
	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
		{
			new = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
			{
				new = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
	}
	
// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	force = V_CheckGamma ();

	if (r_pixbytes == 4)	//doesn't support palette cycling. It messes up caches.
	{
		if (!force)
			return;
		basepal = host_basepal;
		newpal = pal;

		for (i=0 ; i<256 ; i++)
		{
			r = basepal[0];
			g = basepal[1];
			b = basepal[2];
			basepal += 3;

			newpal[0] = gammatable[r];
			newpal[1] = gammatable[g];
			newpal[2] = gammatable[b];
			newpal += 3;
		}

		VID_ShiftPalette (pal);
		return;
	}
	if (!new && !force)
		return;
			
	basepal = host_basepal;
	newpal = pal;
	
	for (i=0 ; i<256 ; i++)
	{
		r = basepal[0];
		g = basepal[1];
		b = basepal[2];
		basepal += 3;
	
		for (j=0 ; j<NUM_CSHIFTS ; j++)	
		{
			r += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[0]-r))>>8;
			g += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[1]-g))>>8;
			b += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[2]-b))>>8;
		}
		
		newpal[0] = gammatable[r];
		newpal[1] = gammatable[g];
		newpal[2] = gammatable[b];
		newpal += 3;
	}

	VID_ShiftPalette (pal);	
}

#endif	// SWQUAKE

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

// allways idle in intermission
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

#ifdef Q2CLIENT
	if (cls.q2server)
		return;
#endif

	r_refdef.currentplayernum = pnum;

	V_DriftPitch (pnum);

// view is the weapon model (only visible from inside body)
	view = &cl.viewent[pnum];

	bob = V_CalcBob (pnum);
	
// refresh position from simulated origin
	VectorCopy (cl.simorg[pnum], r_refdef.vieworg);

	r_refdef.vieworg[2] += bob;

// never let it sit exactly on a node line, because a water plane can
// dissapear when viewed with the eye exactly on it.
// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
	r_refdef.vieworg[0] += 1.0/16;
	r_refdef.vieworg[1] += 1.0/16;
	r_refdef.vieworg[2] += 1.0/16;

	VectorCopy (cl.simangles[pnum], r_refdef.viewangles);
	V_CalcViewRoll (pnum);
	V_AddIdle (pnum);

#ifdef Q2CLIENT
	if (!cls.q2server)
#endif
	{
		if (view_message->flags & PF_GIB)
			r_refdef.vieworg[2] += 8;	// gib view height
		else if (view_message->flags & PF_DEAD)
			r_refdef.vieworg[2] -= 16;	// corpse view height
		else
			r_refdef.vieworg[2] += cl.viewheight[pnum];

		r_refdef.vieworg[2] += cl.crouch[pnum];
	}

	if (view_message->flags & PF_DEAD)		// PF_GIB will also set PF_DEAD
	{
		if (!cl.spectator || !cl_chasecam.value)
			r_refdef.viewangles[ROLL] = 80;	// dead view angle
	}


// offsets
	AngleVectors (cl.simangles[pnum], forward, right, up);
	
// set up gun position
	VectorCopy (cl.simangles[pnum], view->angles);
	
	CalcGunAngle (pnum);

	VectorCopy (cl.simorg[pnum], view->origin);
	view->origin[2] += cl.viewheight[pnum];
	view->origin[2] += cl.crouch[pnum];

	for (i=0 ; i<3 ; i++)
	{
		view->origin[i] += forward[i]*bob*0.4;
//		view->origin[i] += right[i]*bob*0.4;
//		view->origin[i] += up[i]*bob*0.8;
	}
	view->origin[2] += bob;

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

#ifdef Q2CLIENT
	if (cls.q2server)
		view->model = NULL;
	else
#endif
	if (view_message->flags & (PF_GIB|PF_DEAD) )
 		view->model = NULL;
 	else
		view->model = cl.model_precache[cl.stats[pnum][STAT_WEAPON]];
	view->frame = view_message->weaponframe;
	view->colormap = vid.colormap;

// set up the refresh position
	r_refdef.viewangles[PITCH] += cl.punchangle[pnum];

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
	cl.punchangle[pnum] -= 10*host_frametime;
	if (cl.punchangle[pnum] < 0)
		cl.punchangle[pnum] = 0;
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
		break;

	case 2:	//horizontal bands
	case 3:
		vrect->width = vid.width;
		vrect->height = vid.height/cl.splitclients;
		vrect->x = 0;
		vrect->y = 0 + vrect->height*pnum;

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
	r_refdef.fov_y = CalcFov(r_refdef.fov_x, vrect->width, vrect->height);
}

void V_RenderPlayerViews(int plnum)
{
	int viewnum;
	SCR_VRectForPlayer(&r_refdef.vrect, plnum);
	view_message = &view_frame->playerstate[cl.playernum[plnum]];
#ifdef NQPROT
	if (cls.netcon)
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
		V_CalcRefdef (plnum);
	}

#ifdef SWQUAKE
		r_viewchanged = true;
#endif

#if defined(FISH) && defined(SWQUAKE)
	if (ffov.value && cls.allow_fish && qrenderer == QR_SOFTWARE)
		R_RenderView_fisheye();
	else
#endif
		R_RenderView ();
	
	r_secondaryview = 2;





#ifdef SIDEVIEWS
/*	//adjust main view height to strip off the rearviews at the top
	if (vsecwidth >= 1)
	{		
		r_refdef.vrect.y -= vsecheight;
		r_refdef.vrect.height += vsecheight;
	}
*/
#ifdef RGLQUAKE
	gl_ztrickdisabled&=~1;
#endif
	for (viewnum = 0; viewnum < SIDEVIEWS; viewnum++)
	if (vsec_enabled[viewnum].value && vsec_scalex[viewnum].value>0&&vsec_scaley[viewnum].value>0 && (cls.allow_rearview||(cl.stats[plnum][STAT_VIEW2]&&viewnum==0)))	//will the server allow us to?
	{
		vrect_t oldrect;
		vec3_t oldangles;
		vec3_t oldposition;
//		int oldviewent;
		struct entity_s *e;
		float ofx;
		float ofy;

		gl_ztrickdisabled|=1;
#ifdef SWQUAKE
		r_viewchanged = true;
#endif
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

#ifdef SWQUAKE
		r_viewchanged = true;
#endif
		vid.recalc_refdef=true;
	}
#endif
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

	Cvar_Register (&v_bonusflash, VIEWVARS);

	Cvar_Register (&crosshaircolor, VIEWVARS);
	Cvar_Register (&crosshair, VIEWVARS);
	Cvar_Register (&crosshairsize, VIEWVARS);
	Cvar_Register (&crosshaircorrect, VIEWVARS);
	Cvar_Register (&cl_crossx, VIEWVARS);
	Cvar_Register (&cl_crossy, VIEWVARS);
	Cvar_Register (&gl_cshiftpercent, VIEWVARS);

	Cvar_Register (&cl_rollspeed, VIEWVARS);
	Cvar_Register (&cl_rollangle, VIEWVARS);
	Cvar_Register (&cl_bob, VIEWVARS);
	Cvar_Register (&cl_bobcycle, VIEWVARS);
	Cvar_Register (&cl_bobup, VIEWVARS);

	Cvar_Register (&v_kicktime, VIEWVARS);
	Cvar_Register (&v_kickroll, VIEWVARS);
	Cvar_Register (&v_kickpitch, VIEWVARS);


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





















#if defined(FISH) && defined(SWQUAKE)



typedef unsigned char B;

#define BOX_FRONT  0
#define BOX_BEHIND 2
#define BOX_LEFT   3
#define BOX_RIGHT  1
#define BOX_TOP    4
#define BOX_BOTTOM 5

#define PI 3.141592654

#define DEG(x) (x / PI * 180.0)
#define RAD(x) (x * PI / 180.0)

struct my_coords
	{
	double x, y, z;
	};

struct my_angles
	{
	double yaw, pitch, roll;
	};

void x_rot(struct my_coords *c, double pitch);
void y_rot(struct my_coords *c, double yaw);
void z_rot(struct my_coords *c, double roll);
void my_get_angles(struct my_coords *in_o, struct my_coords *in_u, struct my_angles *a);

// get_ypr()

void get_ypr(double yaw, double pitch, double roll, int side, struct my_angles *a)
  {
  struct my_coords o, u;

  // get 'o' (observer) and 'u' ('this_way_up') depending on box side

  switch(side)
    {
    case BOX_FRONT:
      //printf("(FRONT)");
      o.x =  0.0; o.y =  0.0; o.z =  1.0;
      u.x =  0.0; u.y =  1.0; u.z =  0.0; break;
    case BOX_BEHIND:
      //printf("(BEHIND)");
      o.x =  0.0; o.y =  0.0; o.z = -1.0;
      u.x =  0.0; u.y =  1.0; u.z =  0.0; break;
    case BOX_LEFT:
      //printf("(LEFT)");
      o.x = -1.0; o.y =  0.0; o.z =  0.0;
      u.x = -1.0; u.y =  1.0; u.z =  0.0; break;
    case BOX_RIGHT:
      o.x =  1.0; o.y =  0.0; o.z =  0.0;
      //printf("(RIGHT)");
      u.x =  0.0; u.y =  1.0; u.z =  0.0; break;
    case BOX_TOP:
      //printf("(TOP)");
      o.x =  0.0; o.y = -1.0; o.z =  0.0;
      u.x =  0.0; u.y =  0.0; u.z = -1.0; break;
    case BOX_BOTTOM:
      //printf("(BOTTOM)");
      o.x =  0.0; o.y =  1.0; o.z =  0.0;
      u.x =  0.0; u.y =  0.0; u.z = -1.0; break;
    }

  //printf(" - [inputs: yaw = %.4f, pitch = %.4f, roll = %.4f]\n", yaw, pitch, roll);

  z_rot(&o, roll); z_rot(&u, roll);
  x_rot(&o, pitch); x_rot(&u, pitch);
  y_rot(&o, yaw); y_rot(&u, yaw);

  my_get_angles(&o, &u, a);

  /* normalise angles */

  while (a->yaw   <   0.0) a->yaw   += 360.0;
  while (a->yaw   > 360.0) a->yaw   -= 360.0;
  while (a->pitch <   0.0) a->pitch += 360.0;
  while (a->pitch > 360.0) a->pitch -= 360.0;
  while (a->roll  <   0.0) a->roll  += 360.0;
  while (a->roll  > 360.0) a->roll  -= 360.0;

  //printf("get_ypr -> %.4f, %.4f, %.4f\n", a->yaw, a->pitch, a->roll);
  }

/* my_get_angles */

void my_get_angles(struct my_coords *in_o, struct my_coords *in_u, struct my_angles *a)
  {
  double rad_yaw, rad_pitch;
  struct my_coords o, u;

  a->pitch = 0.0;
  a->yaw = 0.0;
  a->roll = 0.0;

  // make a copy of the coords

  o.x = in_o->x; o.y = in_o->y; o.z = in_o->z;
  u.x = in_u->x; u.y = in_u->y; u.z = in_u->z;

  //printf("%.4f, %.4f, %.4f - \n", o.x, o.y, o.z);

  // special case when looking straight up or down

  if ((o.x == 0.0) && (o.z == 0.0))
    {
    // printf("special!\n");
    a->yaw   = 0.0;
    if (o.y > 0.0) { a->pitch = -90.0; a->roll = 180.0 - DEG(atan2(u.x, u.z)); } // down
    else           { a->pitch =  90.0; a->roll = DEG(atan2(u.x, u.z)); } // up
    return;
    }

/******************************************************************************/

  // get yaw angle and then rotate o and u so that yaw = 0

  rad_yaw = atan2(-o.x, o.z);
  a->yaw  = DEG(rad_yaw);

  y_rot(&o, -rad_yaw);
  y_rot(&u, -rad_yaw);

  //printf("%.4f, %.4f, %.4f - stage 1\n", o.x, o.y, o.z);

  // get pitch and then rotate o and u so that pitch = 0

  rad_pitch = atan2(-o.y, o.z);
  a->pitch  = DEG(rad_pitch);

  x_rot(&o, -rad_pitch);
  x_rot(&u, -rad_pitch);

  //printf("%.4f, %.4f, %.4f - stage 2\n", u.x, u.y, u.z);

  // get roll

  a->roll = DEG(-atan2(u.x, u.y));

  //printf("yaw = %.4f, pitch = %.4f, roll = %.4f\n", a->yaw, a->pitch, a->roll);
  }

/*******************************************************************************/

/* x_rot (pitch) */

void x_rot(struct my_coords *c, double pitch)
	{
	double nx, ny, nz;

	nx = c->x;
	ny = (c->y * cos(pitch)) - (c->z * sin(pitch));
	nz = (c->y * sin(pitch)) + (c->z * cos(pitch));

	c->x = nx; c->y = ny; c->z = nz;

	/*printf("x_rot: %.4f, %.4f, %.4f\n", c->x, c->y, c->z);*/
	}

/* y_rot (yaw) */

void y_rot(struct my_coords *c, double yaw)
	{
	double nx, ny, nz;

	nx = (c->x * cos(yaw)) - (c->z * sin(yaw));
	ny = c->y;
	nz = (c->x * sin(yaw)) + (c->z * cos(yaw));

	c->x = nx; c->y = ny; c->z = nz;
	}

/* z_rot (roll) */

void z_rot(struct my_coords *c, double roll)
	{
	double nx, ny, nz;

	nx = (c->x * cos(roll)) - (c->y * sin(roll));
	ny = (c->x * sin(roll)) + (c->y * cos(roll));
	nz = c->z;

	c->x = nx; c->y = ny; c->z = nz;
	}

void rendercopy(int *dest)
{
  int *p = (int*)vid.buffer;
  int x, y;
  int nw = (vid.width/4) * r_pixbytes;
  R_PushDlights();
  R_RenderView();
  for(y = 0;y<vid.height;y++) {
    for(x = 0;x<nw;x++,dest++) *dest = p[x];
    p += (vid.rowbytes/4) * r_pixbytes;
  };
};

void renderlookup(B **offs, B* bufs) {
  B *p = (B*)vid.buffer;
  int x, y;
  for(y = 0;y<vid.height;y++) {
    for(x = 0;x<vid.width;x++,offs++) p[x] = **offs;
    p += vid.rowbytes;
  };
};

void renderlookup32(unsigned int **offs, unsigned int* bufs) {
  unsigned int *p = (unsigned int*)vid.buffer;
  int x, y;
  for(y = 0;y<vid.height;y++) {
    for(x = 0;x<vid.width;x++,offs++) p[x] = **offs;
    p += vid.rowbytes;
  };
};

void fisheyelookuptable(B **buf, int width, int height, B *scrp, double fov) {
  int x, y;
  
  for(y = 0;y<height;y++) for(x = 0;x<width;x++) {
    double dx = x-width/2;
    double dy = -(y-height/2);
    double yaw = sqrt(dx*dx+dy*dy)*fov/((double)width);
    double roll = -atan2(dy,dx);
    double sx = sin(yaw) * cos(roll);
    double sy = sin(yaw) * sin(roll);
    double sz = cos(yaw);

    // determine which side of the box we need
    double abs_x = fabs(sx);
    double abs_y = fabs(sy);
    double abs_z = fabs(sz);			
    int side;
    double xs=0, ys=0;
    if (abs_x > abs_y) {
      if (abs_x > abs_z) { side = ((sx > 0.0) ? BOX_RIGHT : BOX_LEFT);   }
      else               { side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND); }
    } else {
      if (abs_y > abs_z) { side = ((sy > 0.0) ? BOX_TOP   : BOX_BOTTOM); }
      else               { side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND); }
    }

    #define RC(x) ((x / 2.06) + 0.5)
    #define R2(x) RC(x)//((x / 2.03) + 0.5)

    // scale up our vector [x,y,z] to the box
    switch(side) {
      case BOX_FRONT:  xs = RC( sx /  sz); ys = R2( sy /  sz); break;
      case BOX_BEHIND: xs = RC(-sx / -sz); ys = R2( sy / -sz); break;
      case BOX_LEFT:   xs = RC( sz / -sx); ys = R2( sy / -sx); break;
      case BOX_RIGHT:  xs = RC(-sz /  sx); ys = R2( sy /  sx); break;
      case BOX_TOP:    xs = RC( sx /  sy); ys = R2( sz / -sy); break; //bot
      case BOX_BOTTOM: xs = RC(-sx /  sy); ys = R2( sz / -sy); break; //top??
    }

    if (xs <  0.0) xs = 0.0;
    if (xs >= 1.0) xs = 0.999;
    if (ys <  0.0) ys = 0.0;
    if (ys >= 1.0) ys = 0.999;
    *buf++=scrp+((((int)(xs*(double)width))+
                 ((int)(ys*(double)height))*width)+
                 side*width*height)*r_pixbytes;
  };
};

void renderside(B* bufs, double yaw, double pitch, double roll, int side) {
  struct my_angles a;
  get_ypr(RAD(yaw), RAD(pitch), RAD(roll), side, &a);
  if (side == BOX_RIGHT) { a.roll = -a.roll; a.pitch = -a.pitch; }
  if (side == BOX_LEFT)  { a.roll = -a.roll; a.pitch = -a.pitch; }
  if (side == BOX_TOP)   { a.yaw += 180.0; a.pitch = 180.0 - a.pitch; }
  r_refdef.viewangles[YAW] = a.yaw;
  r_refdef.viewangles[PITCH] = a.pitch;
  r_refdef.viewangles[ROLL] = a.roll;
  rendercopy((int *)bufs);
};

//extern int istimedemo;

void R_RenderView_fisheye(void)
{
  int width = vid.width; //r_refdef.vrect.width;
  int height = vid.height; //r_refdef.vrect.height;
  int scrsize = width*height*r_pixbytes;
  int fov = (int)ffov.value;
  int views = (int)fviews.value;
  double yaw = r_refdef.viewangles[YAW];
  double pitch = r_refdef.viewangles[PITCH];
  double roll = 0;//r_refdef.viewangles[ROLL];
  static int pwidth = -1;
  static int pheight = -1;
  static int pfov = -1;
  static int pviews = -1;
  static B *scrbufs = NULL;  
  static B **offs = NULL;
  //Con_Printf("renderfisheye: %d %d %d\n",vid.height,vid.width,vid.rowbytes);

  Cvar_Set(&scr_fov, "90");
  Cvar_Set(&scr_viewsize, "120");

  if(fov<1) fov = 1;

  if(pwidth!=width || pheight!=height || pfov!=fov) {	 
    if(scrbufs) BZ_Free(scrbufs);
    if(offs) BZ_Free(offs);
    scrbufs = (B*)BZ_Malloc(scrsize*6); // front|right|back|left|top|bottom
    offs = (B**)BZ_Malloc(scrsize*sizeof(B*));
    if(!scrbufs || !offs) Sys_Error("Out of mem"); // the rude way
    pwidth = width;
    pheight = height;
    pfov = fov;
    fisheyelookuptable(offs,width,height,scrbufs,((double)fov)*PI/180.0);
  };

  if(views!=pviews) {
    int i;
    pviews = views;
    for(i = 0;i<scrsize*6;i++) scrbufs[i] = 0;
  };

  switch(views) {
    case 6:  renderside(scrbufs+scrsize*2,yaw,pitch,roll, BOX_BEHIND);
    case 5:  renderside(scrbufs+scrsize*5,yaw,pitch,roll, BOX_BOTTOM);
    case 4:  renderside(scrbufs+scrsize*4,yaw,pitch,roll, BOX_TOP);
    case 3:  renderside(scrbufs+scrsize*3,yaw,pitch,roll, BOX_LEFT);
    case 2:  renderside(scrbufs+scrsize,  yaw,pitch,roll, BOX_RIGHT);
    default: renderside(scrbufs,          yaw,pitch,roll, BOX_FRONT);
  };

  r_refdef.viewangles[YAW] = yaw;
  r_refdef.viewangles[PITCH] = pitch;
  r_refdef.viewangles[ROLL] = roll;
  if (r_pixbytes == 4)
	renderlookup32((unsigned int **)offs,(unsigned int *)scrbufs);
  else
	renderlookup(offs,scrbufs);
};


#endif

