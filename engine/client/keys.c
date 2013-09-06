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
#include "quakedef.h"
#ifdef _WIN32
#include "winquake.h"
#endif
/*

key up events are sent even if in console mode

*/
void Editor_Key(int key, int unicode);
void Key_ConsoleInsert(char *instext);
void Key_ClearTyping (void);

#define		KEY_MODIFIERSTATES 8
unsigned char	*key_lines[CON_EDIT_LINES_MASK+1];
int		key_linepos;
int		shift_down=false;
int		key_lastpress;

int		edit_line=0;
int		history_line=0;

keydest_t	key_dest;

int		key_count;			// incremented every key event

char	*keybindings[K_MAX][KEY_MODIFIERSTATES];
qbyte	bindcmdlevel[K_MAX][KEY_MODIFIERSTATES];
qboolean	consolekeys[K_MAX];	// if true, can't be rebound while in console
qboolean	menubound[K_MAX];	// if true, can't be rebound while in menu
int		keyshift[K_MAX];		// key to map to if shift held down in console
int		key_repeats[K_MAX];	// if > 1, it is autorepeating
qboolean	keydown[K_MAX];

qboolean deltaused[K_MAX][KEY_MODIFIERSTATES];

void Con_Selectioncolour_Callback(struct cvar_s *var, char *oldvalue);

extern cvar_t con_displaypossibilities;
cvar_t con_selectioncolour = CVARFC("con_selectioncolour", "0", CVAR_RENDERERCALLBACK, Con_Selectioncolour_Callback);
cvar_t con_echochat = CVAR("con_echochat", "0");
extern cvar_t cl_chatmode;

static int KeyModifier (qboolean shift, qboolean alt, qboolean ctrl)
{
	int stateset = 0;
	if (shift)
		stateset |= 1;
	if (alt)
		stateset |= 2;
	if (ctrl)
		stateset |= 4;

	return stateset;
}

typedef struct
{
	char	*name;
	int		keynum;
} keyname_t;

keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"LALT", K_LALT},
	{"RALT", K_RALT},
	{"LCTRL", K_LCTRL},
	{"RCTRL", K_RCTRL},
	{"LSHIFT", K_LSHIFT},
	{"RSHIFT", K_RSHIFT},
	{"ALT", K_LALT},	//depricated name
	{"CTRL", K_CTRL},	//depricated name
	{"SHIFT", K_SHIFT},	//depricated name
	
	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	
	{"KP_HOME",		K_KP_HOME},
	{"KP_UPARROW",	K_KP_UPARROW},
	{"KP_PGUP",		K_KP_PGUP},
	{"KP_LEFTARROW", K_KP_LEFTARROW},
	{"KP_5",		K_KP_5},
	{"KP_RIGHTARROW", K_KP_RIGHTARROW},
	{"KP_END",		K_KP_END},
	{"KP_DOWNARROW",	K_KP_DOWNARROW},
	{"KP_PGDN",		K_KP_PGDN},
	{"KP_ENTER",	K_KP_ENTER},
	{"KP_INS",		K_KP_INS},
	{"KP_DEL",		K_KP_DEL},
	{"KP_SLASH",	K_KP_SLASH},
	{"KP_MINUS",	K_KP_MINUS},
	{"KP_PLUS",		K_KP_PLUS},
	{"KP_NUMLOCK",	K_KP_NUMLOCK},
	{"KP_STAR",		K_KP_STAR},
	{"KP_EQUALS",	K_KP_EQUALS},

	//fuhquake compatible.
	{"KP_0",		K_KP_INS},
	{"KP_1",		K_KP_END},
	{"KP_2",		K_KP_DOWNARROW},
	{"KP_3",		K_KP_PGDN},
	{"KP_4",		K_KP_LEFTARROW},
	{"KP_6",		K_KP_RIGHTARROW},
	{"KP_7",		K_KP_HOME},
	{"KP_8",		K_KP_UPARROW},
	{"KP_9",		K_KP_PGUP},

	{"MOUSE1",	K_MOUSE1},
	{"MOUSE2",	K_MOUSE2},
	{"MOUSE3",	K_MOUSE3},
	{"MOUSE4",	K_MOUSE4},
	{"MOUSE5",	K_MOUSE5},
	{"MOUSE6",	K_MOUSE6},
	{"MOUSE7",	K_MOUSE7},
	{"MOUSE8",	K_MOUSE8},
	{"MOUSE9",	K_MOUSE9},
	{"MOUSE10",	K_MOUSE10},

	{"LWIN",	K_LWIN},
	{"RWIN",	K_RWIN},
	{"APP",		K_APP},
	{"MENU",	K_APP},
	{"SEARCH",	K_SEARCH},
	{"POWER",	K_POWER},
	{"VOLUP",	K_VOLUP},
	{"VOLDOWN",	K_VOLDOWN},
 
	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},
	{"AUX17", K_AUX17},
	{"AUX18", K_AUX18},
	{"AUX19", K_AUX19},
	{"AUX20", K_AUX20},
	{"AUX21", K_AUX21},
	{"AUX22", K_AUX22},
	{"AUX23", K_AUX23},
	{"AUX24", K_AUX24},
	{"AUX25", K_AUX25},
	{"AUX26", K_AUX26},
	{"AUX27", K_AUX27},
	{"AUX28", K_AUX28},
	{"AUX29", K_AUX29},
	{"AUX30", K_AUX30},
	{"AUX31", K_AUX31},
	{"AUX32", K_AUX32},

	{"PAUSE", K_PAUSE},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"PRINTSCREEN", K_PRINTSCREEN},
	{"CAPSLOCK", K_CAPSLOCK},
	{"SCROLLLOCK", K_SCRLCK},

	{"SEMICOLON", ';'},	// because a raw semicolon seperates commands

	{"TILDE", '~'},
	{"BACKQUOTE", '`'},
	{"BACKSLASH", '\\'},

	{NULL,0}
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

qboolean Cmd_IsCommand (char *line)
{
	char	command[128];
	char	*cmd, *s;
	int		i;

	s = line;

	for (i=0 ; i<127 ; i++)
		if (s[i] <= ' ' || s[i] == ';')
			break;
		else
			command[i] = s[i];
	command[i] = 0;

	cmd = Cmd_CompleteCommand (command, true, false, -1, NULL);
	if (!cmd  || strcmp (cmd, command) )
		return false;		// just a chat message
	return true;
}

#define COLUMNWIDTH 20
#define MINCOLUMNWIDTH 18

int PaddedPrint (char *s, int x)
{
	Con_Printf ("^4%s\t", s);
	x+=strlen(s);

	return x;
}

int con_commandmatch;
void CompleteCommand (qboolean force)
{
	char	*cmd, *s;
	char *desc;

	s = key_lines[edit_line]+1;
	if (*s == '\\' || *s == '/')
		s++;

	for (cmd = s; *cmd; cmd++)
	{
		if (*cmd == ' ' || *cmd == '\t')
			break;
	}
	if (*cmd)
		cmd = s;
	else
	{
		//check for singular matches and complete if found
		cmd = Cmd_CompleteCommand (s, true, true, 2, NULL);
		if (!cmd || force)
		{
			if (!force)
				cmd = Cmd_CompleteCommand (s, false, true, 1, &desc);
			else
				cmd = Cmd_CompleteCommand (s, true, true, con_commandmatch, &desc);
			if (cmd)
			{
				//complete to that (maybe partial) cmd.
				Key_ClearTyping();
				Key_ConsoleInsert("/");
				Key_ConsoleInsert(cmd);
				s = key_lines[edit_line]+2;

				//if its the only match, add a space ready for arguments.
				cmd = Cmd_CompleteCommand (s, true, true, 0, NULL);
				if (cmd && !strcmp(s, cmd))
				{
					Key_ConsoleInsert(" ");
				}

				if (!con_commandmatch)
					con_commandmatch = 1;

				if (desc)
					Con_Footerf(false, "%s: %s", cmd, desc);
				else
					Con_Footerf(false, "");
				return;
			}
		}
		//complete to a partial match.
		cmd = Cmd_CompleteCommand (s, false, true, 0, &desc);
		if (cmd)
		{
			int i = key_lines[edit_line][1] == '/'?2:1;
			if (i != 2 || strcmp(key_lines[edit_line]+i, cmd))
			{	//if successful, use that instead.
				Key_ClearTyping();
				Key_ConsoleInsert("/");
				Key_ConsoleInsert(cmd);

				s = key_lines[edit_line]+1;	//readjust to cope with the insertion of a /
				if (*s == '\\' || *s == '/')
					s++;
			}
		}
	}
	con_commandmatch++;
	cmd = Cmd_CompleteCommand(s, true, true, con_commandmatch, &desc);
	if (!cmd)
	{
		con_commandmatch = 1;
		cmd = Cmd_CompleteCommand(s, true, true, con_commandmatch, &desc);
	}
	if (cmd)
	{
		cvar_t *var = Cvar_FindVar(cmd);
		if (var)
		{
			if (desc)
				Con_Footerf(false, "%s %s\n%s", cmd, var->string, desc);
			else
				Con_Footerf(false, "%s %s", cmd, var->string);
		}
		else
		{
			if (desc)
				Con_Footerf(false, "%s: %s", cmd, desc);
			else
				Con_Footerf(false, "");
		}
	}
	else
	{
		Con_Footerf(false, "");
		con_commandmatch = 1;
	}
}

//lines typed at the main console enter here
void Con_ExecuteLine(console_t *con, char *line)
{
	qboolean waschat = false;
	char deutf8[8192];
	if (com_parseutf8.ival <= 0)
	{
		unsigned int unicode;
		int err;
		int len = 0;
		while(*line)
		{
			unicode = utf8_decode(&err, line, &line);
			if (com_parseutf8.ival < 0)
				len += iso88591_encode(deutf8+len, unicode, sizeof(deutf8)-1 - len);
			else
				len += qchar_encode(deutf8+len, unicode, sizeof(deutf8)-1 - len);
		}
		deutf8[len] = 0;
		line = deutf8;
	}

	con_commandmatch=1;
	Con_Footerf(false, "");

	if (cls.state >= ca_connected && cl_chatmode.value == 2)
	{
		waschat = true;
		if (keydown[K_CTRL])
			Cbuf_AddText ("say_team ", RESTRICT_LOCAL);
		else if (keydown[K_SHIFT])
			Cbuf_AddText ("say ", RESTRICT_LOCAL);
		else
			waschat = false;
	}
	while (*line == ' ')
		line++;
	if (waschat)
		Cbuf_AddText (line, RESTRICT_LOCAL);
	else
	{
		if (line[0] == '\\' || line[0] == '/')
			Cbuf_AddText (line+1, RESTRICT_LOCAL);	// skip the >
		else if (cl_chatmode.value == 2 && Cmd_IsCommand(line))
			Cbuf_AddText (line, RESTRICT_LOCAL);	// valid command
	#ifdef Q2CLIENT
		else if (cls.protocol == CP_QUAKE2)
			Cbuf_AddText (line, RESTRICT_LOCAL);	// send the command to the server via console, and let the server convert to chat
	#endif
		else if (*line)
		{	// convert to a chat message
			if ((cl_chatmode.value == 1 || ((cls.state >= ca_connected && cl_chatmode.value == 2) && (strncmp(line, "say ", 4)))))
			{
				if (keydown[K_CTRL])
					Cbuf_AddText ("say_team ", RESTRICT_LOCAL);
				else
					Cbuf_AddText ("say ", RESTRICT_LOCAL);
				waschat = true;
			}
			Cbuf_AddText (line, RESTRICT_LOCAL);	// skip the >
		}
	}

	Cbuf_AddText ("\n", RESTRICT_LOCAL);
	if (!waschat || con_echochat.value)
		Con_Printf ("]%s\n",line);

	if (cls.state == ca_disconnected)
		SCR_UpdateScreen ();	// force an update, because the command
									// may take some time
}

vec3_t sccolor;

void Con_Selectioncolour_Callback(struct cvar_s *var, char *oldvalue)
{
	if (qrenderer != QR_NONE)
		SCR_StringToRGB(var->string, sccolor, 1);
}

qboolean Key_GetConsoleSelectionBox(console_t *con, int *sx, int *sy, int *ex, int *ey)
{
	*sx = *sy = *ex = *ey = 0;

	if (con->mousedown[2] == 1)
	{
		while (con->mousecursor[1] - con->mousedown[1] > 8 && con->display->older)
		{
			con->mousedown[1] += 8;
			con->display = con->display->older;
		}
		while (con->mousecursor[1] - con->mousedown[1] < -8 && con->display->newer)
		{
			con->mousedown[1] -= 8;
			con->display = con->display->newer;
		}

		*sx = con->mousecursor[0];
		*sy = con->mousecursor[1];
		*ex = con->mousecursor[0];
		*ey = con->mousecursor[1];
		return true;
	}
	else if (con->mousedown[2] == 2)
	{
		*sx = con->mousedown[0];
		*sy = con->mousedown[1];
		*ex = con->mousecursor[0];
		*ey = con->mousecursor[1];
		return true;
	}
	return false;
}

/*insert the given text at the console input line at the current cursor pos*/
void Key_ConsoleInsert(char *instext)
{
	int i;
	int len, olen;
	char *old;
	if (!*instext)
		return;

	old = key_lines[edit_line];
	len = strlen(instext);
	olen = strlen(old);
	key_lines[edit_line] = BZ_Malloc(olen + len + 1);
	memcpy(key_lines[edit_line], old, key_linepos);
	memcpy(key_lines[edit_line]+key_linepos, instext, len);
	memcpy(key_lines[edit_line]+key_linepos+len, old+key_linepos, olen - key_linepos+1);
	Z_Free(old);
	for (i = key_linepos; i < key_linepos+len; i++)
	{
		if (key_lines[edit_line][i] == '\r')
			key_lines[edit_line][i] = ' ';
		else if (key_lines[edit_line][i] == '\n')
			key_lines[edit_line][i] = ';';
	}
	key_linepos += len;
}

void Key_DefaultLinkClicked(char *text, char *info)
{
	char *c;
	/*the engine supports specific default links*/
	/*we don't support everything. a: there's no point. b: unbindall links are evil.*/
	c = Info_ValueForKey(info, "player");
	if (*c)
	{
		unsigned int player = atoi(c);
		int i;
		if (player >= MAX_CLIENTS || !*cl.players[player].name)
			return;

		c = Info_ValueForKey(info, "action");
		if (*c)
		{
			if (!strcmp(c, "mute"))
			{
				if (!cl.players[player].vignored)
				{
					cl.players[player].vignored = true;
					Con_Printf("^[%s\\player\\%i^] muted\n", cl.players[player].name, player);
				}
				else
				{
					cl.players[player].vignored = false;
					Con_Printf("^[%s\\player\\%i^] unmuted\n", cl.players[player].name, player);
				}
			}
			else if (!strcmp(c, "ignore"))
			{
				if (!cl.players[player].ignored)
				{
					cl.players[player].ignored = true;
					cl.players[player].vignored = true;
					Con_Printf("^[%s\\player\\%i^] ignored\n", cl.players[player].name, player);
				}
				else
				{
					cl.players[player].ignored = false;
					cl.players[player].vignored = false;
					Con_Printf("^[%s\\player\\%i^] unignored\n", cl.players[player].name, player);
				}
			}
			else if (!strcmp(c, "spec"))
			{
				Cam_TrackPlayer(0, "spectate", cl.players[player].name);
			}
			else if (!strcmp(c, "kick"))
			{
#ifndef CLIENTONLY
				if (sv.active)
				{
					//use the q3 command, because we can.
					Cbuf_AddText(va("\nclientkick %i\n", player), RESTRICT_LOCAL);
				}
				else
#endif
					Cbuf_AddText(va("\nrcon kick %s\n", cl.players[player].name), RESTRICT_LOCAL);
			}
			else if (!strcmp(c, "ban"))
			{
#ifndef CLIENTONLY
				if (sv.active)
				{
					//use the q3 command, because we can.
					Cbuf_AddText(va("\nbanname %s QuickBan\n", cl.players[player].name), RESTRICT_LOCAL);
				}
				else
#endif
					Cbuf_AddText(va("\nrcon banname %s QuickBan\n", cl.players[player].name), RESTRICT_LOCAL);
			}
			return;
		}

		Con_Footerf(false, "^m#^m ^[%s\\player\\%i^]: %if %ims", cl.players[player].name, player, cl.players[player].frags, cl.players[player].ping);

		for (i = 0; i < cl.splitclients; i++)
		{
			if (cl.playerview[i].playernum == player)
				break;
		}
		if (i == cl.splitclients)
		{
			extern cvar_t rcon_password;
			if (cl.spectator || cls.demoplayback)
			{
				//we're spectating, or an mvd
				Con_Footerf(true, " ^[Spectate\\player\\%i\\action\\spec^]", player);
			}
			else
			{
				//we're playing.
				if (cls.protocol == CP_QUAKEWORLD && strcmp(cl.players[cl.playerview[0].playernum].team, cl.players[player].team))
					Con_Footerf(true, " ^[[Join Team %s]\\cmd\\setinfo team %s^]", cl.players[player].team, cl.players[player].team);
			}
			Con_Footerf(true, " ^[%sgnore\\player\\%i\\action\\ignore^]", cl.players[player].ignored?"Uni":"I", player);
	//		if (cl_voip_play.ival)
				Con_Footerf(true, " ^[%sute\\player\\%i\\action\\mute^]", cl.players[player].vignored?"Unm":"M",  player);

			if (!cls.demoplayback && (*rcon_password.string
#ifndef CLIENTONLY
				|| (sv.state && svs.clients[player].netchan.remote_address.type != NA_LOOPBACK)
#endif
				))
			{
				Con_Footerf(true, " ^[Kick\\player\\%i\\action\\kick^]", player);
				Con_Footerf(true, " ^[Ban\\player\\%i\\action\\ban^]", player);
			}
		}
		else
		{
			char cmdprefix[6];
			snprintf(cmdprefix, sizeof(cmdprefix), "%i ", i);

			//hey look! its you!

			if (cl.spectator || cls.demoplayback)
			{
				//need join option here or something
			}
			else
			{
				Con_Footerf(true, " ^[Suicide\\cmd\\kill^]");
	#ifndef CLIENTONLY
				if (!sv.state)
					Con_Footerf(true, " ^[Disconnect\\cmd\\disconnect^]");
				if (cls.allow_cheats || (sv.state && sv.allocated_client_slots == 1))
	#else
				Con_Footerf(true, " ^[Disconnect\\cmd\\disconnect^]");
				if (cls.allow_cheats)
	#endif
				{
					Con_Footerf(true, " ^[Noclip\\cmd\\noclip^]");
					Con_Footerf(true, " ^[Fly\\cmd\\fly^]");
					Con_Footerf(true, " ^[God\\cmd\\god^]");
					Con_Footerf(true, " ^[Give\\impulse\\9^]");
				}
			}
		}
		return;
	}
	c = Info_ValueForKey(info, "menu");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nmenu_cmd conlink %s\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "connect");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nconnect %s\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "join");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\njoin %s\n", c), RESTRICT_LOCAL);
		return;
	}
	/*c = Info_ValueForKey(info, "url");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nplayfilm %s\n", c), RESTRICT_LOCAL);
		return;
	}*/
	c = Info_ValueForKey(info, "observe");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nobserve %s\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "qtv");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nqtvplay %s\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "demo");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nplaydemo %s\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "cmd");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\ncmd %s\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "edit");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nedit %s\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "impulse");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nimpulse %s\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "film");
	if (*c && !strchr(c, ';') && !strchr(c, '\n'))
	{
		Cbuf_AddText(va("\nplayfilm \"%s\"\n", c), RESTRICT_LOCAL);
		return;
	}
	c = Info_ValueForKey(info, "desc");
	if (*c)
	{
		Con_Footerf(false, "%s", c);
		return;
	}
	if (!*info && *text == '/')
	{
		Z_Free(key_lines[edit_line]);
		key_lines[edit_line] = BZ_Malloc(strlen(text) + 2);
		key_lines[edit_line][0] = ']';
		strcpy(key_lines[edit_line]+1, text);
		key_linepos = strlen(key_lines[edit_line]);
		return;
	}
}

void Key_ConsoleRelease(console_t *con, int key, int unicode)
{
	char *buffer;
	if (key == K_MOUSE1)
	{
		con->mousedown[2] = 0;
		if (abs(con->mousedown[0] - con->mousecursor[0]) < 5 && abs(con->mousedown[1] - con->mousecursor[1]) < 5)
		{
			buffer = Con_CopyConsole(false);
			Con_Footerf(false, "");
			if (!buffer)
				return;
			if (keydown[K_SHIFT])
			{
				int len;
				len = strlen(buffer);
				//strip any trailing dots/elipsis
				while (len > 1 && !strcmp(buffer+len-1, "."))
				{
					len-=1;
					buffer[len] = 0;
				}
				//strip any enclosing quotes
				while (*buffer == '\"' && len > 2 && !strcmp(buffer+len-1, "\""))
				{
					len-=2;
					memmove(buffer, buffer+1, len);
					buffer[len] = 0;
				}
				Key_ConsoleInsert(buffer);
			}
			else
			{
				if (buffer[0] == '^' && buffer[1] == '[')
				{
					//looks like it might be a link!
					char *end = NULL;
					char *info;
					for (info = buffer + 2; *info; )
					{
						if (info[0] == '^' && info[1] == ']')
						{
							break;
						}
						if (*info == '\\')
							break;
						else if (info[0] == '^' && info[1] == '^')
							info+=2;
						else
							info++;
					}
					for(end = info; *end; )
					{
						if (end[0] == '^' && end[1] == ']')
						{
							//okay, its a valid link that they clicked
							*end = 0;
#ifdef PLUGINS
							if (!Plug_ConsoleLink(buffer+2, info))
#endif
#ifdef CSQC_DAT
							if (!CSQC_ConsoleLink(buffer+2, info))
#endif
							{
								Key_DefaultLinkClicked(buffer+2, info);
							}

							break;
						}
						if (end[0] == '^' && end[1] == '^')
							end+=2;
						else
							end++;
					}
				}
			}
			Z_Free(buffer);
		}
		else
			Con_Footerf(false, "");
	}
	if (key == K_MOUSE2 && con->mousedown[2] == 2)
	{
		con->mousedown[2] = 0;
		buffer = Con_CopyConsole(true);	//don't keep markup if we're copying to the clipboard
		if (!buffer)
			return;
		Sys_SaveClipboard(buffer);
		Z_Free(buffer);

	}
}
//if the referenced (trailing) chevron is doubled up, then it doesn't act as part of any markup and should be ignored for such things.
static qboolean utf_specialchevron(unsigned char *start, unsigned char *chev)
{
	int count = 0;
	while (chev >= start)
	{
		if (*chev-- == '^')
			count++;
		else
			break;
	}
	return count&1;
}
//move the cursor one char to the left. cursor must be within the 'start' string.
static unsigned char *utf_left(unsigned char *start, unsigned char *cursor)
{
	if (cursor == start)
		return cursor;
	if (1)//com_parseutf8.ival>0)
	{
		cursor--;
		while ((*cursor & 0xc0) == 0x80 && cursor > start)
			cursor--;
	}
	else
		cursor--;

	//FIXME: should verify that the ^ isn't doubled.
	if (*cursor == ']' && cursor > start && utf_specialchevron(start, cursor-1))
	{
		//just stepped onto a link
		unsigned char *linkstart;
		linkstart = cursor-1;
		while(linkstart >= start)
		{
			//FIXME: should verify that the ^ isn't doubled.
			if (utf_specialchevron(start, linkstart) && linkstart[1] == '[')
				return linkstart;
			linkstart--;
		}
	}

	return cursor;
}

//move the cursor one char to the right.
static unsigned char *utf_right(unsigned char *start, unsigned char *cursor)
{
	//FIXME: should make sure this is not doubled.
	if (utf_specialchevron(start, cursor) && cursor[1] == '[')
	{
		//just stepped over a link
		char *linkend;
		linkend = cursor+2;
		while(*linkend)
		{
			if (utf_specialchevron(start, linkend) && linkend[1] == ']')
				return linkend+2;
			else
				linkend++;
		}
		return linkend;
	}

	if (1)//com_parseutf8.ival>0)
	{
		int skip = 1;
		//figure out the length of the char
		if ((*cursor & 0xc0) == 0x80)
			skip = 1;	//error
		else if ((*cursor & 0xe0) == 0xc0)
			skip = 2;
		else if ((*cursor & 0xf0) == 0xe0)
			skip = 3;
		else if ((*cursor & 0xf1) == 0xf0)
			skip = 4;
		else if ((*cursor & 0xf3) == 0xf1)
			skip = 5;
		else if ((*cursor & 0xf7) == 0xf3)
			skip = 6;
		else if ((*cursor & 0xff) == 0xf7)
			skip = 7;
		else skip = 1;

		while (*cursor && skip)
		{
			cursor++;
			skip--;
		}
	}
	else if (*cursor)
		cursor++;

	return cursor;
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
qboolean Key_Console (console_t *con, unsigned int unicode, int key)
{
	qboolean ctrl = keydown[K_LCTRL] || keydown[K_RCTRL];
	qboolean shift = keydown[K_LSHIFT] || keydown[K_RSHIFT];
	char	*clipText;
	char utf8[8];

	//weirdness for the keypad.
	if ((unicode >= '0' && unicode <= '9') || unicode == '.')
		key = 0;

	if (con->redirect)
	{
		if (key == K_TAB)
		{	// command completion
			if (ctrl || shift)
			{
				Con_CycleConsole();
				return true;
			}
		}
		con->redirect(con_current, key);
		return true;
	}

	if ((key == K_MOUSE1 || key == K_MOUSE2))
	{
		int xpos, ypos;
		xpos = (int)((con->mousecursor[0]*vid.width)/(vid.pixelwidth*8));
		ypos = (int)((con->mousecursor[1]*vid.height)/(vid.pixelheight*8));
		con->mousedown[0] = con->mousecursor[0];
		con->mousedown[1] = con->mousecursor[1];
		if (ypos == 0 && con_mouseover)
		{
			if (key == K_MOUSE2)
				Con_Destroy (con_mouseover);
			else
				con_current = con_mouseover;
		}
		else if (key == K_MOUSE2)
			con->mousedown[2] = 2;
		else 
			con->mousedown[2] = 1;

		return true;
	}
	
	if (key == K_ENTER || key == K_KP_ENTER)
	{	// backslash text are commands, else chat
		int oldl = edit_line;
		edit_line = (edit_line + 1) & (CON_EDIT_LINES_MASK);
		history_line = edit_line;
		Z_Free(key_lines[edit_line]);
		key_lines[edit_line] = BZ_Malloc(2);
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = '\0';
		key_linepos = 1;

		if (con->linebuffered)
			con->linebuffered(con, key_lines[oldl]+1);
		con_commandmatch = 0;

		return true;
	}

	if (key == K_SPACE && ctrl && con->commandcompletion)
	{
		char *txt = key_lines[edit_line]+1;
		if (*txt == '/')
			txt++;
		if (Cmd_CompleteCommand(txt, true, true, con->commandcompletion, NULL))
		{
			CompleteCommand (true);
			return true;
		}
	}

	if (key == K_TAB)
	{	// command completion
		if (shift)
		{
			Con_CycleConsole();
			return true;
		}

		if (con->commandcompletion)
			CompleteCommand (ctrl);
		return true;
	}
	if (key != K_CTRL && key != K_SHIFT && con_commandmatch)
		con_commandmatch=1;
	
	if (key == K_LEFTARROW || key == K_KP_LEFTARROW)
	{
		if (ctrl)
		{
			//ignore whitespace if we're at the end of the word
			while (key_linepos > 0 && key_lines[edit_line][key_linepos-1] == ' ')
				key_linepos = utf_left(key_lines[edit_line]+1, key_lines[edit_line] + key_linepos) - key_lines[edit_line];
			//keep skipping until we find the start of that word
			while (ctrl && key_linepos > 1 && key_lines[edit_line][key_linepos-1] != ' ')
				key_linepos = utf_left(key_lines[edit_line]+1, key_lines[edit_line] + key_linepos) - key_lines[edit_line];
		}
		else
			key_linepos = utf_left(key_lines[edit_line]+1, key_lines[edit_line] + key_linepos) - key_lines[edit_line];
		return true;
	}
	if (key == K_RIGHTARROW || key == K_KP_RIGHTARROW)
	{
		if (key_lines[edit_line][key_linepos])
		{
			key_linepos = utf_right(key_lines[edit_line]+1, key_lines[edit_line] + key_linepos) - key_lines[edit_line];
			if (ctrl)
			{
				//skip over the word
				while (key_lines[edit_line][key_linepos] && key_lines[edit_line][key_linepos] != ' ')
					key_linepos = utf_right(key_lines[edit_line]+1, key_lines[edit_line] + key_linepos) - key_lines[edit_line];
				//as well as any trailing whitespace
				while (key_lines[edit_line][key_linepos] == ' ')
					key_linepos = utf_right(key_lines[edit_line]+1, key_lines[edit_line] + key_linepos) - key_lines[edit_line];
			}
			return true;
		}
		else
			unicode = ' ';
	}

	if (key == K_DEL || key == K_KP_DEL)
	{
		if (key_lines[edit_line][key_linepos])
		{
			int charlen = utf_right(key_lines[edit_line]+1, key_lines[edit_line] + key_linepos) - (key_lines[edit_line] + key_linepos);
			memmove(key_lines[edit_line]+key_linepos, key_lines[edit_line]+key_linepos+charlen, strlen(key_lines[edit_line]+key_linepos+charlen)+1);
			return true;
		}
		else
			key = K_BACKSPACE;
	}

	if (key == K_BACKSPACE)
	{
		if (key_linepos > 1)
		{
			int charlen = (key_lines[edit_line]+key_linepos) - utf_left(key_lines[edit_line]+1, key_lines[edit_line] + key_linepos);
			memmove(key_lines[edit_line]+key_linepos-charlen, key_lines[edit_line]+key_linepos, strlen(key_lines[edit_line]+key_linepos)+1);
			key_linepos -= charlen;
		}
		if (!key_lines[edit_line][1])
			con_commandmatch = 0;
		return true;
	}

	if (key == K_UPARROW || key == K_KP_UPARROW)
	{
		do
		{
			history_line = (history_line - 1) & CON_EDIT_LINES_MASK;
		} while (history_line != edit_line
				&& !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line+1)&CON_EDIT_LINES_MASK;
		key_lines[edit_line] = BZ_Realloc(key_lines[edit_line], strlen(key_lines[history_line])+1);
		Q_strcpy(key_lines[edit_line], key_lines[history_line]);
		key_linepos = Q_strlen(key_lines[edit_line]);

		key_lines[edit_line][0] = ']';
		if (!key_lines[edit_line][1])
			con_commandmatch = 0;
		return true;
	}

	if (key == K_DOWNARROW || key == K_KP_DOWNARROW)
	{
		if (history_line == edit_line)
		{
			key_lines[edit_line][0] = ']';
			key_lines[edit_line][1] = '\0';
			key_linepos=1;
			con_commandmatch = 0;
			return true;
		}
		do
		{
			history_line = (history_line + 1) & CON_EDIT_LINES_MASK;
		}
		while (history_line != edit_line
			&& !key_lines[history_line][1]);
		if (history_line == edit_line)
		{
			key_lines[edit_line][0] = ']';
			key_lines[edit_line][1] = '\0';
			key_linepos = 1;
		}
		else
		{
			key_lines[edit_line] = BZ_Realloc(key_lines[edit_line], strlen(key_lines[history_line])+1);
			Q_strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = Q_strlen(key_lines[edit_line]);
		}
		return true;
	}

	if (key == K_PGUP || key == K_KP_PGUP || key==K_MWHEELUP)
	{
		int i = 2;
		if (ctrl)
			i = 8;
		if (!con->display)
			return true;
		if (con->display == con->current)
			i+=2;	//skip over the blank input line, and extra so we actually move despite the addition of the ^^^^^ line
		while (i-->0)
		{
			if (con->display->older == NULL)
				break;
			con->display = con->display->older;
		}
		return true;
	}
	if (key == K_PGDN || key == K_KP_PGDN || key==K_MWHEELDOWN)
	{
		int i = 2;
		if (ctrl)
			i = 8;
		if (!con->display)
			return true;
		while (i-->0)
		{
			if (con->display->newer == NULL)
				break;
			con->display = con->display->newer;
		}
		if (con->display->newer && con->display->newer == con->current)
			con->display = con->current;
		return true;
	}

	if (key == K_HOME || key == K_KP_HOME)
	{
		if (ctrl)
			con->display = con->oldest;
		else
			key_linepos = 1;
		return true;
	}

	if (key == K_END || key == K_KP_END)
	{
		if (ctrl)
			con->display = con->current;
		else
			key_linepos = strlen(key_lines[edit_line]);
		return true;
	}

	//beware that windows translates ctrl+c and ctrl+v to a control char
	if (((unicode=='C' || unicode=='c' || unicode==3) && ctrl) || (ctrl && key == K_INS))
	{
		Sys_SaveClipboard(key_lines[edit_line]+1);
		return true;
	}

	if (((unicode=='V' || unicode=='v' || unicode==22) && ctrl) || (shift && key == K_INS))
	{
		clipText = Sys_GetClipboard();
		if (clipText)
		{
			Key_ConsoleInsert(clipText);
			Sys_CloseClipboard(clipText);
		}
		return true;
	}

	if (unicode < ' ')
	{
		//if the user is entering control codes, then the ctrl+foo mechanism is probably unsupported by the unicode input stuff, so give best-effort replacements.
		switch(unicode)
		{
		case 27/*'['*/: unicode = 0xe010; break;
		case 29/*']'*/: unicode = 0xe011; break;
		case 7/*'g'*/: unicode = 0xe086; break;
		case 18/*'r'*/: unicode = 0xe087; break;
		case 25/*'y'*/: unicode = 0xe088; break;
		case 2/*'b'*/: unicode = 0xe089; break;
		case 19/*'s'*/: unicode = 0xe080; break;
		case 4/*'d'*/: unicode = 0xe081; break;
		case 6/*'f'*/: unicode = 0xe082; break;
		case 1/*'a'*/: unicode = 0xe083; break;
		case 21/*'u'*/: unicode = 0xe01d; break;
		case 9/*'i'*/: unicode = 0xe01e; break;
		case 15/*'o'*/: unicode = 0xe01f; break;
		case 10/*'j'*/: unicode = 0xe01c; break;
		case 16/*'p'*/: unicode = 0xe09c; break;
		case 13/*'m'*/: unicode = 0xe08b; break;
		case 11/*'k'*/: unicode = 0xe08d; break;
		case 14/*'n'*/: unicode = '\r'; break;
		default:
//			if (unicode)
//				Con_Printf("escape code %i\n", unicode);

			//even if we don't print these, we still need to cancel them in the caller.
			if (key == K_LALT || key == K_RALT ||
				key == K_LCTRL || key == K_RCTRL ||
				key == K_LSHIFT || key == K_RSHIFT)
				return true;
			return false;
		}
	}
	else if (com_parseutf8.ival >= 0)	//don't do this for iso8859-1. the major user of that is hexen2 which doesn't have these chars.
	{
		if (ctrl && !keydown[K_RALT])
		{
			if (unicode >= '0' && unicode <= '9')
				unicode = unicode - '0' + 0xe012;	// yellow number
			else switch (unicode)
			{
				case '[': unicode = 0xe010; break;
				case ']': unicode = 0xe011; break;
				case 'g': unicode = 0xe086; break;
				case 'r': unicode = 0xe087; break;
				case 'y': unicode = 0xe088; break;
				case 'b': unicode = 0xe089; break;
				case '(': unicode = 0xe080; break;
				case '=': unicode = 0xe081; break;
				case ')': unicode = 0xe082; break;
				case 'a': unicode = 0xe083; break;
				case '<': unicode = 0xe01d; break;
				case '-': unicode = 0xe01e; break;
				case '>': unicode = 0xe01f; break;
				case ',': unicode = 0xe01c; break;
				case '.': unicode = 0xe09c; break;
				case 'B': unicode = 0xe08b; break;
				case 'C': unicode = 0xe08d; break;
				case 'n': unicode = '\r'; break;
			}
		}

		if (keydown[K_LALT] && unicode > 32 && unicode < 128)
			unicode |= 0xe080;		// red char
	}

	unicode = utf8_encode(utf8, unicode, sizeof(utf8)-1);
	if (unicode)
	{
		utf8[unicode] = 0;
		Key_ConsoleInsert(utf8);
		return true;
	}

	return false;
}

//============================================================================

qboolean	chat_team;
char		chat_buffer[MAXCMDLINE];
int			chat_bufferlen = 0;

void Key_Message (int key, int unicode)
{

	if (key == K_ENTER || key == K_KP_ENTER)
	{
		if (chat_buffer[0])
		{	//send it straight into the command.
			Cmd_TokenizeString(va("%s %s", chat_team?"say_team":"say", chat_buffer), true, false);
			CL_Say(chat_team, "");
		}

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE)
	{
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	if (key == K_BACKSPACE)
	{
		if (chat_bufferlen)
		{
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == sizeof(chat_buffer)-1)
		return; // all full

	chat_buffer[chat_bufferlen++] = unicode;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================

char *Key_GetBinding(int keynum)
{
	if (keynum >= 0 && keynum < K_MAX)
		return keybindings[keynum][0];
	return NULL;
}

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum (char *str, int *modifier)
{
	keyname_t	*kn;
	char *underscore;

	if (!strnicmp(str, "std_", 4))
		*modifier = 0;
	else
	{
		*modifier = 0;
		while(1)
		{
			underscore = strchr(str, '_');
			if (!underscore || !underscore[1])
				break;	//nothing afterwards or no underscore.
			if (!strnicmp(str, "shift_", 6))
				*modifier |= 1;
			else if (!strnicmp(str, "alt_", 4))
				*modifier |= 2;
			else if (!strnicmp(str, "ctrl_", 5))
				*modifier |= 4;
			else
				break;
			str = underscore+1;	//next char.
		}
		if (!*modifier)
			*modifier = ~0;
	}
	
	if (!str || !str[0])
		return -1;
	if (!str[1])	//single char.
	{
#if 0//def _WIN32
		return VkKeyScan(str[0]);
#else
		return str[0];
#endif
	}

	if (!strncmp(str, "K_", 2))
		str+=2;

	for (kn=keynames ; kn->name ; kn++)
	{
		if (!Q_strcasecmp(str,kn->name))
			return kn->keynum;
	}
	if (atoi(str))	//assume ascii code. (prepend with a 0 if needed)
	{
		return atoi(str);
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char *Key_KeynumToString (int keynum)
{
	keyname_t	*kn;	
	static	char	tinystr[2];
	
	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127)
	{	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}
	
	for (kn=keynames ; kn->name ; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	{
		if (keynum < 10)	//don't let it be a single character
			return va("0%i", keynum);
		return va("%i", keynum);
	}

	return "<UNKNOWN KEYNUM>";
}


/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding (int keynum, int modifier, char *binding, int level)
{
	char	*newc;
	int		l;

	if (modifier == ~0)	//all of the possibilities.
	{
		for (l = 0; l < KEY_MODIFIERSTATES; l++)
			Key_SetBinding(keynum, l, binding, level);
		return;
	}
			
	if (keynum < 0 || keynum >= K_MAX)
		return;

	//just so the quit menu realises it needs to show something.
	Cvar_ConfigChanged();

// free old bindings
	if (keybindings[keynum][modifier])
	{
		Z_Free (keybindings[keynum][modifier]);
		keybindings[keynum][modifier] = NULL;
	}


	if (!binding)
	{
		keybindings[keynum][modifier] = NULL;
		return;
	}
// allocate memory for new binding
	l = Q_strlen (binding);	
	newc = Z_Malloc (l+1);
	Q_strcpy (newc, binding);
	newc[l] = 0;
	keybindings[keynum][modifier] = newc;
	bindcmdlevel[keynum][modifier] = level;
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f (void)
{
	int		b, modifier;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}
	
	b = Key_StringToKeynum (Cmd_Argv(1), &modifier);
	if (b==-1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding (b, modifier, NULL, Cmd_ExecLevel);
}

void Key_Unbindall_f (void)
{
	int		i;
	
	for (i=0 ; i<K_MAX ; i++)
		if (keybindings[i])
			Key_SetBinding (i, ~0, NULL, Cmd_ExecLevel);
}


/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f (void)
{
	int			i, c, b, modifier;
	char		cmd[1024];
	
	c = Cmd_Argc();

	if (c < 2)
	{
		Con_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1), &modifier);
	if (b==-1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[b][0])
			Con_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b][0] );
		else
			Con_Printf ("\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}

	if (c > 3)
	{
		Cmd_ShiftArgs(1, Cmd_ExecLevel==RESTRICT_LOCAL);
		Key_SetBinding (b, modifier, Cmd_Args(), Cmd_ExecLevel);
		return;
	}
	
// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	for (i=2 ; i< c ; i++)
	{
		Q_strncatz (cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (c-1))
			Q_strncatz (cmd, " ", sizeof(cmd));
	}

	Key_SetBinding (b, modifier, cmd, Cmd_ExecLevel);
}

void Key_BindLevel_f (void)
{
	int			i, c, b, modifier;
	char		cmd[1024];
	
	c = Cmd_Argc();

	if (c != 2 && c != 3)
	{
		Con_Printf ("bindat <key> [<level> <command>] : attach a command to a key for a specific level of access\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1), &modifier);
	if (b==-1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[b])
			Con_Printf ("\"%s\" (%i)= \"%s\"\n", Cmd_Argv(1), bindcmdlevel[b][modifier], keybindings[b][modifier] );
		else
			Con_Printf ("\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}

	if (Cmd_IsInsecure())
	{
		Con_Printf("Server attempted usage of bindat\n");
		return;
	}

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	for (i=3 ; i< c ; i++)
	{
		Q_strncatz (cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (c-1))
			Q_strncatz (cmd, " ", sizeof(cmd));
	}

	Key_SetBinding (b, modifier, cmd, atoi(Cmd_Argv(2)));
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings (vfsfile_t *f)
{
	const char *s;
	int		i, m;
	char *binding, *base;

	char prefix[128];
	char keybuf[256];
	char commandbuf[2048];

	for (i=0 ; i<K_MAX ; i++)	//we rebind the key with all modifiers to get the standard bind, then change the specific ones.
	{						//this does two things, it normally allows us to skip 7 of the 8 possibilities
		base = keybindings[i][0];	//plus we can use the config with other clients.
		if (!base)
			base = "";
		for (m = 0; m < KEY_MODIFIERSTATES; m++)
		{
			binding = keybindings[i][m];
			if (!binding)
				binding = "";
			if (strcmp(binding, base) || (m==0 && keybindings[i][0]) || bindcmdlevel[i][m] != bindcmdlevel[i][0])
			{
				*prefix = '\0';
				if (m & 4)
					strcat(prefix, "CTRL_");
				if (m & 2)
					strcat(prefix, "ALT_");
				if (m & 1)
					strcat(prefix, "SHIFT_");

				s = va("%s%s", prefix, Key_KeynumToString(i));
				//quote it as required
				if (i == ';' || i <= ' ' || i == '\"')
					s = COM_QuotedString(s, keybuf, sizeof(keybuf));

				if (bindcmdlevel[i][m] != bindcmdlevel[i][0])
					s = va("bindlevel %s %i %s\n", s, bindcmdlevel[i][m], COM_QuotedString(binding, commandbuf, sizeof(commandbuf)));
				else
					s = va("bind %s %s\n", s, COM_QuotedString(binding, commandbuf, sizeof(commandbuf)));
				VFS_WRITE(f, s, strlen(s));
			}
		}
	}
}


/*
===================
Key_Init
===================
*/
void Key_Init (void)
{
	int		i;

	for (i=0 ; i<=CON_EDIT_LINES_MASK ; i++)
	{
		key_lines[i] = Z_Malloc(2);
		key_lines[i][0] = ']';
	}
	key_linepos = 1;
	
//
// init ascii characters in console mode
//
	for (i=32 ; i<128 ; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_KP_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_KP_LEFTARROW] = true;
	consolekeys[K_KP_RIGHTARROW] = true;
	consolekeys[K_KP_UPARROW] = true;
	consolekeys[K_KP_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_DEL] = true;
	consolekeys[K_KP_DEL] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_KP_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_KP_END] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_KP_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_KP_PGDN] = true;
	consolekeys[K_LSHIFT] = true;
	consolekeys[K_RSHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	consolekeys[K_LCTRL] = true;
	consolekeys[K_RCTRL] = true;
	consolekeys[K_LALT] = true;
	consolekeys[K_RALT] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (i=K_MOUSE1 ; i<K_MOUSE10 ; i++)
	{
		consolekeys[i] = true;
	}
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;

	for (i=0 ; i<K_MAX ; i++)
		keyshift[i] = i;
	for (i='a' ; i<='z' ; i++)
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;
	for (i=0 ; i<12 ; i++)
		menubound[K_F1+i] = true;

//
// register our functions
//
	Cmd_AddCommand ("bind",Key_Bind_f);
	Cmd_AddCommand ("bindlevel",Key_BindLevel_f);
	Cmd_AddCommand ("unbind",Key_Unbind_f);
	Cmd_AddCommand ("unbindall",Key_Unbindall_f);

	Cvar_Register (&con_selectioncolour, "Console variables");
	Cvar_Register (&con_echochat, "Console variables");
}

qboolean Key_MouseShouldBeFree(void)
{
	//returns if the mouse should be a cursor or if it should go to the menu

	//if true, the input code is expected to return mouse cursor positions rather than deltas
	extern cvar_t cl_prydoncursor;
	extern int mouseusedforgui;
	if (mouseusedforgui)	//I don't like this
		return true;

//	if (!ActiveApp)
//		return true;

	if (key_dest == key_menu)
	{
		if (m_state == m_complex || m_state == m_plugin /*|| m_state == m_menu_dat*/)
			return true;
	}
	if (key_dest == key_console || key_dest == key_editor)
		return true;

#ifdef VM_UI
	if (UI_MenuState())
		return false;
#endif

	if (Media_PlayingFullScreen())
		return true;

	if (cl_prydoncursor.ival)
		return true;

	return false;
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Key_Event (int devid, int key, unsigned int unicode, qboolean down)
{
	char	*kb;
	char	p[16];
	char	cmd[1024];
	int keystate, oldstate;
	int conkey = consolekeys[key] || (unicode && (key != '`' || key_linepos>1));	//if the input line is empty, allow ` to toggle the console, otherwise enter it as actual text.

//	Con_Printf ("%i : %i : %i\n", key, unicode, down); //@@@

	oldstate = KeyModifier(keydown[K_LSHIFT]|keydown[K_RSHIFT], keydown[K_LALT]|keydown[K_RALT], keydown[K_LCTRL]|keydown[K_RCTRL]);

	keydown[key] = down;

	if (key == K_LSHIFT || key == K_RSHIFT || key == K_LALT || key == K_RALT || key == K_LCTRL || key == K_RCTRL)
	{
		int k;

		keystate = KeyModifier(keydown[K_LSHIFT]|keydown[K_RSHIFT], keydown[K_LALT]|keydown[K_RALT], keydown[K_LCTRL]|keydown[K_RCTRL]);

		for (k = 0; k < K_MAX; k++)
		{	//go through the old state removing all depressed keys. they are all up now.

			if (k == K_LSHIFT || k == K_RSHIFT || k == K_LALT || k == K_RALT || k == K_LCTRL || k == K_RCTRL)
				continue;

			if (deltaused[k][oldstate])
			{
				if (keybindings[k][oldstate] == keybindings[k][keystate] || !strcmp(keybindings[k][oldstate], keybindings[k][keystate]))
				{	//bindings match. skip this key
//					Con_Printf ("keeping bind %i\n", k); //@@@
					deltaused[k][oldstate] = false;
					deltaused[k][keystate] = true;
					continue;
				}

//				Con_Printf ("removing bind %i\n", k); //@@@

				deltaused[k][oldstate] = false;

				kb = keybindings[k][oldstate];
				if (kb && kb[0] == '+')
				{
					Q_snprintfz (cmd, sizeof(cmd), "-%s %i\n", kb+1, k+oldstate*256);
					Cbuf_AddText (cmd, bindcmdlevel[k][oldstate]);
				}
				if (keyshift[k] != k)
				{
					kb = keybindings[keyshift[k]][oldstate];
					if (kb && kb[0] == '+')
					{
						Q_snprintfz (cmd, sizeof(cmd), "-%s %i\n", kb+1, k+oldstate*256);
						Cbuf_AddText (cmd, bindcmdlevel[k][oldstate]);
					}
				}
			
				if (keydown[k] && (key_dest != key_console && key_dest != key_message))
				{
					deltaused[k][keystate] = true;

	//				Con_Printf ("adding bind %i\n", k); //@@@

					kb = keybindings[k][keystate];
					if (kb)
					{
						if (kb[0] == '+')
						{	// button commands add keynum as a parm
							Q_snprintfz (cmd, sizeof(cmd), "%s %i\n", kb, k+keystate*256);
							Cbuf_AddText (cmd, bindcmdlevel[k][keystate]);
						}
						else
						{
							Cbuf_AddText (kb, bindcmdlevel[k][keystate]);
							Cbuf_AddText ("\n", bindcmdlevel[k][keystate]);
						}
					}
				}
			}
		}

		keystate = oldstate = 0;
	}
	else
		keystate = oldstate;

	if (!down)
		key_repeats[key] = 0;

	key_lastpress = key;
	key_count++;
	if (key_count <= 0)
	{
		return;		// just catching keys for Con_NotifyBox
	}

// update auto-repeat status
	if (down)
	{
		key_repeats[key]++;
			
//		if (key >= 200 && !keybindings[key])	//is this too annoying?
//			Con_Printf ("%s is unbound, hit F4 to set.\n", Key_KeynumToString (key) );
	}

	if (key == K_LSHIFT || key == K_RSHIFT)
	{
		shift_down = keydown[K_LSHIFT]|keydown[K_RSHIFT];
	}

	if (key == K_ESCAPE)
		if (shift_down)
		{
			if (down)
			{
				Con_ToggleConsole_Force();
				return;
			}
		}

	//yes, csqc is allowed to steal the escape key.
	if (key != '`' && (!down || key != K_ESCAPE || (key_dest == key_game && !shift_down)))
	if (key_dest == key_game && !Media_PlayingFullScreen())
	{
#ifdef CSQC_DAT
		if (CSQC_KeyPress(key, unicode, down, devid))	//give csqc a chance to handle it.
			return;
#endif
#ifdef VM_CG
		if (CG_KeyPress(key, unicode, down))
			return;
#endif
	}

//
// handle escape specialy, so the user can never unbind it
//
	if (key == K_ESCAPE)
	{
#ifdef VM_UI
#ifdef TEXTEDITOR
		if (key_dest == key_game)
#endif
		{
			if (down && Media_PlayingFullScreen())
			{
				Media_StopFilm(false);
				return;
			}
			if (UI_KeyPress(key, unicode, down))	//Allow the UI to see the escape key. It is possible that a developer may get stuck at a menu.
				return;
		}
#endif

		if (!down)
		{
			if (key_dest == key_menu)
				M_Keyup (key, unicode);
			return;
		}
		switch (key_dest)
		{
		case key_message:
			Key_Message (key, unicode);
			break;
		case key_menu:
			M_Keydown (key, unicode);
			break;
#ifdef TEXTEDITOR
		case key_editor:
			Editor_Key (key, unicode);
			break;
#endif
		case key_game:
			if (Media_PlayingFullScreen())
			{
				Media_StopFilm(true);
				break;
			}
		case key_console:
			if (cls.state && key_dest == key_console)
				key_dest = key_game;
			else
				M_ToggleMenu_f ();
			break;
		default:
			Sys_Error ("Bad key_dest");
		}
		return;
	}

#ifndef NOMEDIA
	if (key_dest == key_game && Media_PlayingFullScreen())
	{
		Media_Send_KeyEvent(NULL, key, unicode, down?0:1);
		return;
	}
#endif

//
// key up events only generate commands if the game key binding is
// a button command (leading + sign).  These will occur even in console mode,
// to keep the character from continuing an action started before a console
// switch.  Button commands include the keynum as a parameter, so multiple
// downs can be matched with ups
//
	if (!down)
	{
		switch (key_dest)
		{
		case key_menu:
			M_Keyup (key, unicode);
			break;
		case key_console:
			con_current->mousecursor[0] = mousecursor_x;
			con_current->mousecursor[1] = mousecursor_y;
			Key_ConsoleRelease(con_current, key, unicode);
			break;
		default:
			break;
		}

		if (!deltaused[key][keystate])	//this wasn't down, so don't make it leave down state.
			return;
		deltaused[key][keystate] = false;

		if (key == K_RALT)	//simulate a singular alt for binds. really though, this code should translate to csqc/menu keycodes and back to resolve the weirdness instead.
			key = K_ALT;
		if (key == K_RCTRL)	//simulate a singular alt for binds. really though, this code should translate to csqc/menu keycodes and back to resolve the weirdness instead.
			key = K_CTRL;
		if (key == K_RSHIFT)//simulate a singular alt for binds. really though, this code should translate to csqc/menu keycodes and back to resolve the weirdness instead.
			key = K_SHIFT;

		if (devid)
			Q_snprintfz (p, sizeof(p), "p %i ", devid+1);
		else
			*p = 0;
		kb = keybindings[key][keystate];
		if (kb && kb[0] == '+')
		{
			Q_snprintfz (cmd, sizeof(cmd), "-%s%s %i\n", p, kb+1, key+oldstate*256);
			Cbuf_AddText (cmd, bindcmdlevel[key][keystate]);
		}
		if (keyshift[key] != key)
		{
			kb = keybindings[keyshift[key]][keystate];
			if (kb && kb[0] == '+')
			{
				Q_snprintfz (cmd, sizeof(cmd), "-%s%s %i\n", p, kb+1, key+oldstate*256);
				Cbuf_AddText (cmd, bindcmdlevel[key][keystate]);
			}
		}
		return;
	}

//
// during demo playback, most keys bring up the main menu
//
	if (cls.demoplayback && cls.demoplayback != DPB_MVD && cls.demoplayback != DPB_EZTV && down && conkey && key != K_TAB && key_dest == key_game)
	{
		M_ToggleMenu_f ();
		return;
	}

//
// if not a consolekey, send to the interpreter no matter what mode is
//
#ifdef VM_UI
	if (key != '`' && key != '~')
	if (key_dest == key_game || !down)
	{
		if (UI_KeyPress(key, unicode, down) && down)	//UI is allowed to take these keydowns. Keyups are always maintained.
			return;
	}
#endif

	if (key && ((key_dest == key_menu && menubound[key])
	|| (key_dest == key_console && !conkey)
	|| (key_dest == key_game && ( cls.state == ca_active || (!conkey) ) ) ))
	{
		/*don't auto-repeat binds as it breaks too many scripts*/
		if (key_repeats[key] > 1)
			return;

		deltaused[key][keystate] = true;

		if (devid)
			Q_snprintfz (p, sizeof(p), "p %i ", devid+1);
		else
			*p = 0;

		if (key == K_RALT)	//simulate a singular alt for binds. really though, this code should translate to csqc/menu keycodes and back to resolve the weirdness instead.
			key = K_ALT;
		if (key == K_RCTRL)	//simulate a singular alt for binds. really though, this code should translate to csqc/menu keycodes and back to resolve the weirdness instead.
			key = K_CTRL;
		if (key == K_RSHIFT)//simulate a singular alt for binds. really though, this code should translate to csqc/menu keycodes and back to resolve the weirdness instead.
			key = K_SHIFT;

		kb = keybindings[key][keystate];
		if (kb)
		{
			if (kb[0] == '+')
			{	// button commands add keynum as a parm
				Q_snprintfz (cmd, sizeof(cmd), "+%s%s %i\n", p, kb+1, key+oldstate*256);
				Cbuf_AddText (cmd, bindcmdlevel[key][keystate]);
			}
			else
			{
				if (*p)Cbuf_AddText (p, bindcmdlevel[key][keystate]);
				Cbuf_AddText (kb, bindcmdlevel[key][keystate]);
				Cbuf_AddText ("\n", bindcmdlevel[key][keystate]);
			}
		}

		return;
	}

	if (!down)
	{
		switch (key_dest)
		{
		case key_menu:
			M_Keyup (key, unicode);
			break;
		default:
			break;
		}
		return;		// other systems only care about key down events
	}

	switch (key_dest)
	{
	case key_message:
		Key_Message (key, unicode);
		break;
	case key_menu:
		M_Keydown (key, unicode);
		break;
#ifdef TEXTEDITOR
	case key_editor:
		Editor_Key (key, unicode);
		break;
#endif
	case key_game:
	case key_console:
		if ((key && unicode) || key == K_ENTER || key == K_KP_ENTER || key == K_TAB)
			key_dest = key_console;
		con_current->mousecursor[0] = mousecursor_x;
		con_current->mousecursor[1] = mousecursor_y;
		Key_Console (con_current, unicode, key);
		break;
	default:
		Sys_Error ("Bad key_dest");
	}
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates (void)
{
	int		i;

	for (i=0 ; i<K_MAX ; i++)
	{
		keydown[i] = false;
		key_repeats[i] = false;
	}
}

