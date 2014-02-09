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
// input.h -- external (non-keyboard) input devices

void IN_ReInit (void);

void IN_Init (void);

void IN_Shutdown (void);

void IN_Commands (void);
// oportunity for devices to stick commands on the script buffer

void IN_Move (float *movements, int pnum);
// add additional movement on top of the keyboard move cmd

extern cvar_t in_xflip;

extern float mousecursor_x, mousecursor_y;
extern float mousemove_x, mousemove_y;

void IN_ActivateMouse(void);
void IN_DeactivateMouse(void);

int CL_TargettedSplit(qboolean nowrap);

//specific events for the system-specific input code to call. may be called outside the main thread (so long as you don't call these simultaneously - ie: use a mutex or only one input thread).
void IN_KeyEvent(int devid, int down, int keycode, int unicode);		//don't use IN_KeyEvent for mice if you ever use abs mice...
void IN_MouseMove(int devid, int abs, float x, float y, float z, float size);

//system-specific functions
void INS_Move (float *movements, int pnum);
void INS_Accumulate (void);
void INS_ClearStates (void);
void INS_ReInit (void);
void INS_Init (void);
void INS_Shutdown (void);
void INS_Commands (void);	//final chance to call IN_MouseMove/IN_KeyEvent each frame
