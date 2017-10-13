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
float IN_DetermineMouseRate(void);

void IN_Shutdown (void);

void IN_Commands (void);
// oportunity for devices to stick commands on the script buffer

qboolean IN_MouseDevIsTouch(unsigned int devid);	//check if a mouse devid is a touch screen, and thus if we should check the cursor and simulate a ui event or not
int IN_TranslateMButtonPress(unsigned int devid);	//allow the touchscreen code to swallow mouse1 as a begin-looking event

void IN_Move (float *movements, int pnum, float frametime);
// add additional movement on top of the keyboard move cmd

extern cvar_t in_xflip;

extern float mousecursor_x, mousecursor_y;
extern float mousemove_x, mousemove_y;

void IN_ActivateMouse(void);
void IN_DeactivateMouse(void);

int CL_TargettedSplit(qboolean nowrap);

//specific events for the system-specific input code to call. may be called outside the main thread (so long as you don't call these simultaneously - ie: use a mutex or only one input thread).
void IN_KeyEvent(unsigned int devid, int down, int keycode, int unicode);		//don't use IN_KeyEvent for mice if you ever use abs mice...
void IN_MouseMove(unsigned int devid, int abs, float x, float y, float z, float size);
void IN_JoystickAxisEvent(unsigned int devid, int axis, float value);
void IN_Accelerometer(unsigned int devid, float x, float y, float z);
void IN_Gyroscope(unsigned int devid, float pitch, float yaw, float roll);

//system-specific functions
void INS_Move (float *movements, int pnum);
void INS_Accumulate (void);
void INS_ClearStates (void);
void INS_ReInit (void);
void INS_Init (void);
void INS_Shutdown (void);
void INS_Commands (void);	//final chance to call IN_MouseMove/IN_KeyEvent each frame
void INS_EnumerateDevices(void *ctx, void(*callback)(void *ctx, const char *type, const char *devicename, unsigned int *qdevid));
void INS_SetupControllerAudioDevices(qboolean enabled);	//creates audio devices for each controller (where controllers have their own audio devices)

#define DEVID_UNSET ~0u

extern cvar_t	cl_nodelta;
extern cvar_t	cl_c2spps;
extern cvar_t	cl_c2sImpulseBackup;
extern cvar_t	cl_netfps;
extern cvar_t	cl_sparemsec;
extern cvar_t	cl_queueimpulses;
extern cvar_t	cl_smartjump;
extern cvar_t	cl_run;
extern cvar_t	cl_fastaccel;
extern cvar_t	cl_rollspeed;
extern cvar_t	cl_prydoncursor;
extern cvar_t	cl_instantrotate;
extern cvar_t	in_xflip;
extern cvar_t	prox_inmenu;
extern cvar_t	cl_forceseat;
