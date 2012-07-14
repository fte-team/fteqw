/*
 
 Copyright (C) 2001-2002       A Nourai
 Copyright (C) 2006            Jacek Piszczek (Mac OSX port)
 
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

#include "quakedef.h"

float mouse_x,mouse_y;
float old_mouse_x,old_mouse_y;

cvar_t m_filter = SCVARF("m_filter", "1", CVAR_ARCHIVE);

void IN_Init (void)
{
	Cvar_Register (&m_filter, "input values");
}

void IN_ReInit(void)
{
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}
// oportunity for devices to stick commands on the script buffer

void IN_ModeChanged (void)
{
}
// called whenever screen dimensions change

void IN_Move (float *movements, int pnum)
{
	float tx, ty, filterfrac;
    
#ifdef PEXT_CSQC
	if (CSQC_MouseMove(mouse_x, mouse_y, 0))
	{
		mouse_x = 0;
		mouse_y = 0;
	}
#endif

	tx = mouse_x;
	ty = mouse_y;
    
	if (m_filter.value)
	{
		filterfrac = bound(0, m_filter.value, 1) / 2.0;
		mouse_x = (tx * (1 - filterfrac) + old_mouse_x * filterfrac);
		mouse_y = (ty * (1 - filterfrac) + old_mouse_y * filterfrac);
	}
    
	old_mouse_x = tx;
	old_mouse_y = ty;
    
	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;
    
	if ((in_strafe.state[pnum] & 1) || (lookstrafe.value && in_mlook.state[pnum]))
	{
		movements[1] += m_side.value * mouse_x;
	}
	else
	{
		cl.playerview[pnum].viewanglechange[YAW] -= m_yaw.value * mouse_x;
	}

	if (in_mlook.state[pnum])
		 V_StopPitchDrift(pnum);
    
	if (in_mlook.state[pnum] && !(in_strafe.state[pnum] & 1))
	{
		cl.playerview[pnum].viewanglechange[PITCH] += m_pitch.value * mouse_y;
	}
	else
	{
		movements[0] -= m_forward.value * mouse_y;
	}
    
	mouse_x = mouse_y = 0.0;
}
// add additional movement on top of the keyboard move cmd

