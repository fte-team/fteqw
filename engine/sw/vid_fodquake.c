/*
Copyright (C) 2007 Mark Olsen

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

#include "quakedef.h"

/* Key compatibility */
#define K_LCTRL K_CTRL
#define K_RCTRL K_CTRL
#define K_LSHIFT K_SHIFT
#define K_RSHIFT K_SHIFT
#define K_LALT K_ALT
#define K_RALT K_ALT

#define KP_STAR K_KP_STAR
#define KP_NUMLOCK K_KP_NUMLOCK
#define KP_DEL K_KP_DEL
#define KP_INS K_KP_INS
#define KP_HOME K_KP_HOME
#define KP_END K_KP_END
#define KP_PGUP K_KP_PGUP
#define KP_PGDN K_KP_PGDN
#define KP_UPARROW K_KP_UPARROW
#define KP_DOWNARROW K_KP_DOWNARROW
#define KP_LEFTARROW K_KP_LEFTARROW
#define KP_RIGHTARROW K_KP_RIGHTARROW
#define KP_5 K_KP_5
#define KP_MINUS K_KP_MINUS
#define KP_PLUS K_KP_PLUS
#define KP_SLASH K_KP_SLASH
#define KP_ENTER K_KP_ENTER
#define K_MENU K_APP

/* CVar stuff */
static void Fod_Cvar_Register(cvar_t *cvar)
{
	Cvar_Register(cvar, "Input Controls");
}

#define Cvar_SetCurrentGroup(x) do { } while(0)
#define Cvar_ResetCurrentGroup() do { } while(0)
#define Cvar_Register Fod_Cvar_Register

/* Some differences between Fodquake and FTE */
#define Com_Printf Con_Printf
#define Q_Malloc Z_Malloc

#include "fod/vid_x11.c"
#include "fod/in_x11.c"

#undef Cvar_Register

qbyte vid_curpal[768];

cvar_t m_filter = {"m_filter","0", NULL, CVAR_ARCHIVE};
cvar_t m_accel = {"m_accel", "0"};
#ifdef IN_XFLIP
cvar_t in_xflip = {"in_xflip", "0"};
#endif

extern int mousecursor_x, mousecursor_y;

static void *fod_display;
static int fod_width;
static int fod_height;

qboolean SWVID_Init(rendererstate_t *info, unsigned char *palette)
{
	fod_display = Sys_Video_Open(info->width, info->height, info->bpp, info->fullscreen, vid_curpal);
	if (fod_display)
	{
		fod_width = info->width;
		fod_height = info->height;

		vid.width = info->width;
		vid.height = info->height;

		vid.maxwarpwidth = WARP_WIDTH;
		vid.maxwarpheight = WARP_HEIGHT;

		vid.numpages = Sys_Video_GetNumBuffers(fod_display);
		vid.rowbytes = Sys_Video_GetBytesPerRow(fod_display);
		vid.buffer = Sys_Video_GetBuffer(fod_display);
		vid.colormap = host_colormap;
		vid.direct = 0;
		vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

		vid.conbuffer = vid.buffer;
		vid.conrowbytes = vid.rowbytes;
		vid.conwidth = vid.width;
		vid.conheight = vid.height;

		r_pixbytes = info->bpp/8;

		VID_SetPalette(palette);

		Sys_Video_GrabMouse(fod_display, 1);

		S_Startup();

		return true;
	}

	return false;
}

void SWVID_ShiftPalette(unsigned char *p)
{
	SWVID_SetPalette(p);
}

void SWVID_SetPalette(unsigned char *palette)
{
	Sys_Video_SetPalette(fod_display, palette);
}

void SWVID_Shutdown(void)
{
	if (fod_display)
	{
		Sys_Video_Close(fod_display);

		fod_display = 0;
	}
}

void SWVID_Update(vrect_t *rects)
{
	Sys_Video_Update(fod_display, rects);

	vid.buffer = Sys_Video_GetBuffer(fod_display);
	vid.conbuffer = vid.buffer;
}

void Sys_SendKeyEvents(void)
{
}

void SWD_BeginDirectRect(int x, int y, qbyte *pbitmap, int width, int height)
{
}

void SWD_EndDirectRect(int x, int y, int width, int height)
{
}

void SWVID_LockBuffer (void)
{
}

void SWVID_UnlockBuffer (void)
{
}

int SWVID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void SWVID_ForceLockState (int lk)
{
}

void SWVID_HandlePause (qboolean pause)
{
}

void SWVID_SetCaption(char *text)
{
	Sys_Video_SetWindowTitle(fod_display, text);
}

/*** Input ***/

void IN_ReInit(void)
{
}

void IN_Init(void)
{
	Sys_Video_CvarInit();

	Cvar_Register (&m_filter, "Input Controls");
	Cvar_Register (&m_accel, "Input Controls");
#ifdef IN_XFLIP
	Cvar_Register (&in_xflip, "Input Controls");
#endif
}

void IN_Shutdown(void)
{
}

void IN_Commands(void)
{
	if (fod_display)
		Sys_Video_GetEvents(fod_display);
}

void IN_Move(usercmd_t *cmd, int pnum)
{
	float mx, my;
	float mouse_deltadist;
	float mouse_x;
	float mouse_y;
	static float old_mouse_x;
	static float old_mouse_y;
	int fod_mouse_x;
	int fod_mouse_y;

	if (!fod_display)
		return;

	Sys_Video_GetMouseMovement(fod_display, &fod_mouse_x, &fod_mouse_y);

	mouse_x = fod_mouse_x;
	mouse_y = fod_mouse_y;

	if (pnum != 0)
		return;	//we're lazy today.

	mousecursor_x += mouse_x;
	mousecursor_y += mouse_y;

	if (mousecursor_x < 0)
		mousecursor_x = 0;
	if (mousecursor_y < 0)
		mousecursor_y = 0;
	if (mousecursor_x > vid.width)
		mousecursor_x = vid.width;
	if (mousecursor_y > vid.height)
		mousecursor_y = vid.height;
   
	if (m_filter.value)
	{
		float fraction = bound(0, m_filter.value, 2) * 0.5;
		mouse_x = (mouse_x*(1-fraction) + old_mouse_x*fraction);
		mouse_y = (mouse_y*(1-fraction) + old_mouse_y*fraction);
	}

	old_mouse_x = mx;
	old_mouse_y = my;
   
	if (m_accel.value)
	{
		mouse_deltadist = sqrt(mx*mx + my*my);
		mouse_x *= (mouse_deltadist*m_accel.value + sensitivity.value*in_sensitivityscale);
		mouse_y *= (mouse_deltadist*m_accel.value + sensitivity.value*in_sensitivityscale);
	}
	else
	{
		mouse_x *= sensitivity.value*in_sensitivityscale;
		mouse_y *= sensitivity.value*in_sensitivityscale;
	}

#ifdef IN_XFLIP
	if(in_xflip.value) mouse_x *= -1;
#endif
   
	if ( (in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1) ))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[pnum][YAW] -= m_yaw.value * mouse_x;
	if (in_mlook.state[pnum] & 1)
		V_StopPitchDrift (pnum);
   
	if ( (in_mlook.state[pnum] & 1) && !(in_strafe.state[pnum] & 1))
	{
		cl.viewangles[pnum][PITCH] += m_pitch.value * mouse_y;
		CL_ClampPitch(pnum);
	}
	else
	{
		if (cmd)
		{
			if ((in_strafe.state[pnum] & 1) && noclip_anglehack)
				cmd->upmove -= m_forward.value * mouse_y;
			else
				cmd->forwardmove -= m_forward.value * mouse_y;
		}
	}
	mouse_x = mouse_y = 0.0;
}

