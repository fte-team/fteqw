/*
Copyright (C) 2006-2007 Mark Olsen

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

#include <exec/exec.h>
#include <intuition/intuition.h>
#include <intuition/extensions.h>
#include <intuition/intuitionbase.h>
#include <devices/input.h>

#include <proto/exec.h>
#include <proto/intuition.h>

#include <clib/alib_protos.h>

#include "quakedef.h"
#include "input.h"

#include "in_morphos.h"

cvar_t in_xflip = {"in_xflip", "0"};

struct InputEvent imsgs[MAXIMSGS];
extern struct IntuitionBase *IntuitionBase;
extern struct Window *window;
extern struct Screen *screen;

int imsglow = 0;
int imsghigh = 0;

extern qboolean mouse_active;

static struct Interrupt InputHandler;

static struct Interrupt InputHandler;
static struct MsgPort *inputport = 0;
static struct IOStdReq *inputreq = 0;
static BYTE inputret = -1;

cvar_t m_filter = {"m_filter", "1", CVAR_ARCHIVE};

extern cvar_t _windowed_mouse;

float mouse_x, mouse_y;
float old_mouse_x, old_mouse_y;

#define DEBUGRING(x)

void IN_Shutdown(void)
{
	if (inputret == 0)
	{
		inputreq->io_Data = (void *)&InputHandler;
		inputreq->io_Command = IND_REMHANDLER;
		DoIO((struct IORequest *)inputreq);

		CloseDevice((struct IORequest *)inputreq);

		inputret = -1;
	}

	if (inputreq)
	{
		DeleteStdIO(inputreq);

		inputreq = 0;
	}

	if (inputport)
	{
		DeletePort(inputport);

		inputport = 0;
	}
}

void IN_ReInit()
{
/*	Cvar_Register (&m_filter, "input controls");*/

	inputport = CreatePort(0, 0);
	if (inputport == 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to create message port");
	}

	inputreq = CreateStdIO(inputport);
	if (inputreq == 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to create IO request");
	}

	inputret = OpenDevice("input.device", 0, (struct IORequest *)inputreq, 0);
	if (inputret != 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to open input.device");
	}

	InputHandler.is_Node.ln_Type = NT_INTERRUPT;
	InputHandler.is_Node.ln_Pri = 100;
	InputHandler.is_Node.ln_Name = "FTEQW input handler";
	InputHandler.is_Data = 0;
	InputHandler.is_Code = (void(*)())&myinputhandler;
	inputreq->io_Data = (void *)&InputHandler;
	inputreq->io_Command = IND_ADDHANDLER;
	DoIO((struct IORequest *)inputreq);
}

void IN_Init(void)
{
	Cvar_Register (&in_xflip, "input controls");
	IN_ReInit();
}

static void ExpireRingBuffer()
{
	int i = 0;

	while(imsgs[imsglow].ie_Class == IECLASS_NULL && imsglow != imsghigh)
	{
		imsglow++;
		i++;
		imsglow%= MAXIMSGS;
	}

	DEBUGRING(dprintf("Expired %d messages\n", i));
}

void IN_Commands(void)
{
	int a;
	char key;
	int i;
	int down;
	struct InputEvent ie;

	for(i = imsglow;i != imsghigh;i++, i%= MAXIMSGS)
	{
		DEBUGRING(dprintf("%d %d\n", i, imsghigh));
		if ((window->Flags & WFLG_WINDOWACTIVE))
		{
			if (imsgs[i].ie_Class == IECLASS_NEWMOUSE)
			{
				key = 0;

				if (imsgs[i].ie_Code == NM_WHEEL_UP)
					key = K_MWHEELUP;
				else if (imsgs[i].ie_Code == NM_WHEEL_DOWN)
					key = K_MWHEELDOWN;

				if (imsgs[i].ie_Code == NM_BUTTON_FOURTH)
				{
					Key_Event(K_MOUSE4, true);
				}
				else if (imsgs[i].ie_Code == (NM_BUTTON_FOURTH|IECODE_UP_PREFIX))
				{
					Key_Event(K_MOUSE4, false);
				}

				if (key)
				{
					Key_Event(key, 1);
					Key_Event(key, 0);
				}

			}
			else if (imsgs[i].ie_Class == IECLASS_RAWKEY)
			{
				down = !(imsgs[i].ie_Code&IECODE_UP_PREFIX);
				imsgs[i].ie_Code&=~IECODE_UP_PREFIX;

				memcpy(&ie, &imsgs[i], sizeof(ie));

				key = 0;
				if (imsgs[i].ie_Code <= 255)
					key = keyconv[imsgs[i].ie_Code];

				if (key)
					Key_Event(key, down);
				else
				{
					if (developer.value)
						Con_Printf("Unknown key %d\n", imsgs[i].ie_Code);
				}
			}

			else if (imsgs[i].ie_Class == IECLASS_RAWMOUSE)
			{
				if (imsgs[i].ie_Code == IECODE_LBUTTON)
					Key_Event(K_MOUSE1, true);
				else if (imsgs[i].ie_Code == (IECODE_LBUTTON|IECODE_UP_PREFIX))
					Key_Event(K_MOUSE1, false);
				else if (imsgs[i].ie_Code == IECODE_RBUTTON)
					Key_Event(K_MOUSE2, true);
				else if (imsgs[i].ie_Code == (IECODE_RBUTTON|IECODE_UP_PREFIX))
					Key_Event(K_MOUSE2, false);
				else if (imsgs[i].ie_Code == IECODE_MBUTTON)
					Key_Event(K_MOUSE3, true);
				else if (imsgs[i].ie_Code == (IECODE_MBUTTON|IECODE_UP_PREFIX))
					Key_Event(K_MOUSE3, false);

				mouse_x+= imsgs[i].ie_position.ie_xy.ie_x;
				mouse_y+= imsgs[i].ie_position.ie_xy.ie_y;
			}

		}

		imsgs[i].ie_Class = IECLASS_NULL;

	}

	ExpireRingBuffer();

}

void IN_Move (float *movements, int pnum)
{
	extern int mousecursor_x, mousecursor_y;
	extern int mousemove_x, mousemove_y;

	if (pnum != 0)
		return;	//we're lazy today.

	if (m_filter.value) {
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

#ifdef IN_XFLIP
	if(in_xflip.value) mouse_x *= -1;
#endif

   	if (Key_MouseShouldBeFree())
	{
		mousemove_x += mouse_x;
		mousemove_y += mouse_y;
		mousecursor_x += mouse_x;
		mousecursor_y += mouse_y;

		if (mousecursor_y<0)
			mousecursor_y=0;
		if (mousecursor_x<0)
			mousecursor_x=0;

		if (mousecursor_x >= vid.width)
			mousecursor_x = vid.width - 1;

		if (mousecursor_y >= vid.height)
			mousecursor_y = vid.height - 1;

		mouse_x = mouse_y = 0;
#ifdef VM_UI
		UI_MousePosition(mousecursor_x, mousecursor_y);
#endif
	}


	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;
   
	if ( (in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1) ))
	{
		if (cmd)
			movements[1] += m_side.value * mouse_x;
	}
	else
	{
		cl.viewangles[pnum][YAW] -= m_yaw.value * mouse_x;
	}
	if (in_mlook.state[pnum] & 1)
		V_StopPitchDrift (pnum);
   
	if ( (in_mlook.state[pnum] & 1) && !(in_strafe.state[pnum] & 1)) {
		cl.viewangles[pnum][PITCH] += m_pitch.value * mouse_y;
		CL_ClampPitch(pnum);
	} else {
		if (cmd)
		{
			if ((in_strafe.state[pnum] & 1) && noclip_anglehack)
				movements[2] -= m_forward.value * mouse_y;
			else
				movements[0] -= m_forward.value * mouse_y;
		}
	}
	mouse_x = mouse_y = 0.0;
}

char keyconv[] =
{
	'`', /* 0 */
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0', /* 10 */
	'-',
	'=',
	0,
	0,
	K_INS,
	'q',
	'w',
	'e',
	'r',
	't', /* 20 */
	'y',
	'u',
	'i',
	'o',
	'p',
	'[',
	']',
	0,
	K_KP_END,
	K_KP_DOWNARROW, /* 30 */
	K_KP_PGDN,
	'a',
	's',
	'd',
	'f',
	'g',
	'h',
	'j',
	'k',
	'l', /* 40 */
	';',
	'\'',
	'\\',
	0,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	'<',
	'z',
	'x', /* 50 */
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.',
	'/',
	0,
	K_KP_DEL, /* 60 */
	K_KP_HOME,
	K_KP_UPARROW,
	K_KP_PGUP,
	' ',
	K_BACKSPACE,
	K_TAB,
	K_KP_ENTER,
	K_ENTER,
	K_ESCAPE,
	K_DEL, /* 70 */
	K_INS,
	K_PGUP,
	K_PGDN,
	K_KP_MINUS,
	K_F11,
	K_UPARROW,
	K_DOWNARROW,
	K_RIGHTARROW,
	K_LEFTARROW,
	K_F1, /* 80 */
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	0, /* 90 */
	0,
	K_KP_SLASH,
	0,
	K_KP_PLUS,
	0,
	K_SHIFT,
	K_SHIFT,
	K_CAPSLOCK,
	K_CTRL,
	K_ALT, /* 100 */
	K_ALT,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	K_PAUSE, /* 110 */
	K_F12,
	K_HOME,
	K_END,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 120 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 130 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 140 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 150 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 160 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 170 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 180 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 190 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 200 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 210 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 220 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 230 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 240 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 250 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

struct InputEvent *myinputhandler_real(void);

struct EmulLibEntry myinputhandler =
{
	TRAP_LIB, 0, (void(*)(void))myinputhandler_real
};

struct InputEvent *myinputhandler_real()
{
	struct InputEvent *moo = (struct InputEvent *)REG_A0;

	struct InputEvent *coin;

	int screeninfront;

	coin = moo;

	if (screen)
	{
#if 0
		if (IntuitionBase->LibNode.lib_Version > 50 || (IntuitionBase == 50 && IntuitionBase->LibNode.lib_Revision >= 56))
			GetAttr(screen, SA_Displayed, &screeninfront);
		else
#endif
			screeninfront = screen==IntuitionBase->FirstScreen;
	}
	else
		screeninfront = 1;

	do
	{
		if (coin->ie_Class == IECLASS_RAWMOUSE || coin->ie_Class == IECLASS_RAWKEY || coin->ie_Class == IECLASS_NEWMOUSE)
		{
/*			kprintf("Mouse\n");*/

			if ((imsghigh > imsglow && !(imsghigh == MAXIMSGS-1 && imsglow == 0)) || (imsghigh < imsglow && imsghigh != imsglow-1) || imsglow == imsghigh)
			{
				memcpy(&imsgs[imsghigh], coin, sizeof(imsgs[0]));
				imsghigh++;
				imsghigh%= MAXIMSGS;
			}
			else
			{
				DEBUGRING(kprintf("FTE: message dropped, imsglow = %d, imsghigh = %d\n", imsglow, imsghigh));
			}

			if (/*mouse_active && */(window->Flags & WFLG_WINDOWACTIVE) && coin->ie_Class == IECLASS_RAWMOUSE && screeninfront && window->MouseX > 0 && window->MouseY > 0)
			{
				if (_windowed_mouse.value)
				{
#if 0
					coin->ie_Class = IECLASS_NULL;
#else
					coin->ie_position.ie_xy.ie_x = 0;
					coin->ie_position.ie_xy.ie_y = 0;
#endif
				}
			}
		}

		coin = coin->ie_NextEvent;
	} while(coin);

	return moo;
}

