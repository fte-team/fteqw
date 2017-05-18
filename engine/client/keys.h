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

#ifndef __CLIENT_KEYS_H__
#define __CLIENT_KEYS_H__

enum
{	//fte's assumed gamepad axis
	GPAXIS_LT_RIGHT	= 0,
	GPAXIS_LT_DOWN	= 1,
	GPAXIS_LT_AUX	= 2,

	GPAXIS_RT_RIGHT	= 3,
	GPAXIS_RT_DOWN	= 4,
	GPAXIS_RT_AUX	= 5,

//gah
#define GPAXIS_LT_TRIGGER GPAXIS_LT_AUX
#define GPAXIS_RT_TRIGGER GPAXIS_RT_AUX
};

#if 1
//gamepad alises, because I'm too lazy to define actual keys that are distinct from joysticks
#define K_GP_A				K_JOY2
#define K_GP_B				K_JOY4
#define K_GP_X				K_JOY1
#define K_GP_Y				K_JOY3
#define K_GP_LEFT_SHOULDER	K_AUX1
#define K_GP_RIGHT_SHOULDER	K_AUX2
#define K_GP_LEFT_TRIGGER	K_AUX9
#define K_GP_RIGHT_TRIGGER	K_AUX10
#define K_GP_BACK			K_AUX6
#define K_GP_START			K_AUX5
#define K_GP_LEFT_THUMB		K_AUX3
#define K_GP_RIGHT_THUMB	K_AUX4
#define K_GP_DPAD_UP		K_UPARROW
#define K_GP_DPAD_DOWN		K_DOWNARROW
#define K_GP_DPAD_LEFT		K_LEFTARROW
#define K_GP_DPAD_RIGHT		K_RIGHTARROW
#define K_GP_GUIDE			K_AUX7
#define K_GP_UNKNOWN		K_AUX8
#endif

//
// these are the key numbers that should be passed to Key_Event
//
enum {
K_TAB			= 9,
K_ENTER			= 13,
K_ESCAPE		= 27,
K_SPACE			= 32,

// normal keys should be passed as lowercased ascii

K_BACKSPACE		= 127,


K_SCRLCK,
K_CAPSLOCK,
K_POWER,
K_PAUSE,

K_UPARROW,
K_DOWNARROW,
K_LEFTARROW,
K_RIGHTARROW,

K_LALT,
K_LCTRL,
K_LSHIFT,
K_INS,
K_DEL,
K_PGDN,
K_PGUP,
K_HOME,
K_END,

K_F1,
K_F2,
K_F3,
K_F4,
K_F5,
K_F6,
K_F7,
K_F8,
K_F9,
K_F10,
K_F11,
K_F12,
K_F13,
K_F14,
K_F15,

K_KP_HOME,
K_KP_UPARROW,
K_KP_PGUP,
K_KP_LEFTARROW,
K_KP_5,
K_KP_RIGHTARROW,
K_KP_END,
K_KP_DOWNARROW,
K_KP_PGDN,
K_KP_ENTER,
K_KP_INS,
K_KP_DEL,
K_KP_SLASH,
K_KP_MINUS,
K_KP_PLUS,
K_KP_NUMLOCK,
K_KP_STAR,
K_KP_EQUALS,

//
// mouse buttons generate virtual keys
//
K_MOUSE1,
K_MOUSE2,
K_MOUSE3,
K_MOUSE4,
K_MOUSE5,
K_MOUSE6,
K_MOUSE7,
K_MOUSE8,
K_MOUSE9,
K_MOUSE10,

// JACK: Intellimouse(c) Mouse Wheel Support
K_MWHEELUP,
K_MWHEELDOWN, // 189

#ifndef K_GP_A
K_GP_A = 190,
K_GP_B = 191,
K_GP_X = 192,
K_GP_Y = 193,
K_GP_LEFT_SHOULDER = 194,
K_GP_RIGHT_SHOULDER = 195,
K_GP_LEFT_TRIGGER = 196,
K_GP_RIGHT_TRIGGER = 197,
K_GP_BACK = 198,
K_GP_START = 199,
K_GP_LEFT_THUMB = 200,
K_GP_RIGHT_THUMB = 201,
K_GP_GUIDE = 202,
#endif

//
// joystick buttons
//
K_JOY1			= 203,
K_JOY2			= 204,
K_JOY3			= 205,
K_JOY4			= 206,

//
// aux keys are for multi-buttoned joysticks to generate so they can use
// the normal binding process
//
K_AUX1			= 207,
K_AUX2			= 208,
K_AUX3			= 209,
K_AUX4			= 210,
K_AUX5			= 211,
K_AUX6			= 212,
K_AUX7			= 213,
K_AUX8			= 214,
K_AUX9			= 215,
K_AUX10			= 216,
K_AUX11			= 217,
K_AUX12			= 218,
K_AUX13			= 219,
K_AUX14			= 220,
K_AUX15			= 221,
K_AUX16			= 222,
K_AUX17			= 223,
K_AUX18			= 224,
K_AUX19			= 225,
K_AUX20			= 226,
K_AUX21			= 227,
K_AUX22			= 228,
K_AUX23			= 229,
K_AUX24			= 230,
K_AUX25			= 231,
K_AUX26			= 232,
K_AUX27			= 233,
K_AUX28			= 234,
K_AUX29			= 235,
K_AUX30			= 236,
K_AUX31			= 237,
K_AUX32			= 238,
K_LWIN			= 239,
K_RWIN			= 240,
K_APP			= 241,
K_SEARCH		= 242,
K_VOLUP			= 243,
K_VOLDOWN		= 244,
K_RALT			= 245,
K_RCTRL			= 246,
K_RSHIFT		= 247,
K_PRINTSCREEN	= 248,

//K_UNUSED	= 249,
//K_UNUSED	= 250,

#ifndef K_GP_DPAD_UP
K_GP_DPAD_UP = 251,
K_GP_DPAD_DOWN = 252,
K_GP_DPAD_LEFT = 253,
K_GP_DPAD_RIGHT = 254,
K_GP_UNKNOWN = 255,
#endif

K_MAX			= 256
};

#define KEY_MODIFIER_SHIFT		(1<<0)
#define KEY_MODIFIER_ALT		(1<<1)
#define KEY_MODIFIER_CTRL		(1<<2)
#define KEY_MODIFIER_ALTBINDMAP	(1<<3)
#define	KEY_MODIFIERSTATES		(1<<4)

//legacy aliases, lest we ever forget!
#define K_SHIFT K_LSHIFT
#define K_CTRL K_LCTRL
#define K_ALT K_LALT

typedef enum	//highest has priority
{
	kdm_game		= 1u<<0,	//should always be set
	kdm_message		= 1u<<1,
	kdm_gmenu		= 1u<<2,	//menu.dat
	kdm_emenu		= 1u<<3,	//engine's menus
	kdm_editor		= 1u<<4,
	kdm_console		= 1u<<5,
	kdm_cwindows	= 1u<<6,
} keydestmask_t;

//unsigned int Key_Dest_Get(void);	//returns highest priority destination
#define Key_Dest_Add(kdm) (key_dest_mask |= (kdm))
#define Key_Dest_Remove(kdm) (key_dest_mask &= ~(kdm))
#define Key_Dest_Has(kdm) (key_dest_mask & (kdm))
#define Key_Dest_Toggle(kdm) do {if (key_dest_mask & kdm) Key_Dest_Remove(kdm); else Key_Dest_Add(kdm);}while(0)

extern unsigned int key_dest_absolutemouse;	//if the active key dest bit is set, the mouse is absolute.
extern unsigned int key_dest_mask;
extern char *keybindings[K_MAX][16];
extern	int		key_repeats[K_MAX];
extern	int		key_count;			// incremented every key event
extern	int		key_lastpress;

enum
{
	kc_game,
	kc_menu,
	kc_console,
	kc_max
};
extern struct key_cursor_s
{
	char name[MAX_QPATH];
	float hotspot[2];
	float scale;
	qboolean dirty;
	void *handle;
} key_customcursor[kc_max];

extern unsigned char *chat_buffer;
extern	int chat_bufferpos;
extern	qboolean	chat_team;

void Key_Event (unsigned int devid, int key, unsigned int unicode, qboolean down);
void Key_Init (void);
void IN_WriteButtons(vfsfile_t *f, qboolean all);
void Key_WriteBindings (struct vfsfile_s *f);
void Key_SetBinding (int keynum, int modifier, char *binding, int cmdlevel);
void Key_ClearStates (void);
void Key_Unbindall_f (void);	//aka: Key_Shutdown
void Key_ConsoleReplace(const char *instext);

struct console_s;
qboolean Key_GetConsoleSelectionBox(struct console_s *con, int *sx, int *sy, int *ex, int *ey);
qboolean Key_MouseShouldBeFree(void);

#endif

