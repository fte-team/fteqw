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

K_ALT,
K_CTRL,
K_SHIFT,
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

K_MAX			= 256
};

#define K_LSHIFT K_SHIFT
#define K_RSHIFT K_SHIFT
#define K_RCTRL K_CTRL
#define K_LCTRL K_CTRL
#define K_RALT K_CTRL
#define K_LALT K_CTRL

typedef enum {key_game, key_console, key_message, key_menu, key_editor} keydest_t;

extern keydest_t	key_dest;
extern char *keybindings[K_MAX][8];
extern	int		key_repeats[K_MAX];
extern	int		key_count;			// incremented every key event
extern	int		key_lastpress;

extern char chat_buffer[];
extern	int chat_bufferlen;
extern	qboolean	chat_team;

void Key_Event (int devid, int key, unsigned int unicode, qboolean down);
void Key_Init (void);
void Key_WriteBindings (vfsfile_t *f);
void Key_SetBinding (int keynum, int modifier, char *binding, int cmdlevel);
void Key_ClearStates (void);
void Key_Unbindall_f (void);	//aka: Key_Shutdown

qboolean Key_GetConsoleSelectionBox(int *sx, int *sy, int *ex, int *ey);
qboolean Key_MouseShouldBeFree(void);

#endif

