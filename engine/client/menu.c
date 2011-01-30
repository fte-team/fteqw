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
#include "winquake.h"
#include "shader.h"

void M_Menu_Audio_f (void);
void M_Menu_Demos_f (void);

m_state_t m_state;

extern menu_t *menu_script;

qboolean	m_recursiveDraw;

int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define SerialConfig	(m_net_cursor == 0)
#define DirectConfig	(m_net_cursor == 1)
#define	IPXConfig		(m_net_cursor == 2)
#define	TCPIPConfig		(m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

cvar_t m_helpismedia = SCVAR("m_helpismedia", "0");

//=============================================================================
/* Support Routines */

void M_Print (int cx, int cy, qbyte *str)
{
	Draw_AltFunString(cx + ((vid.width - 320)>>1), cy, str);
}
void M_PrintWhite (int cx, int cy, qbyte *str)
{
	Draw_FunString(cx + ((vid.width - 320)>>1), cy, str);
}
void M_DrawScalePic (int x, int y, int w, int h, mpic_t *pic)
{
	Draw_ScalePic (x + ((vid.width - 320)>>1), y, w, h, pic);
}

qbyte identityTable[256];
qbyte translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
	int		j;
	qbyte	*dest, *source;

	int pc = Cvar_Get("cl_playerclass", "1", 0, "foo")->value;
	if (h2playertranslations && pc)
	{
		int i;
		unsigned int color_offsets[5] = {2*14*256,0,1*14*256,2*14*256,2*14*256};
		unsigned char *colorA, *colorB, *sourceA, *sourceB;
		colorA = h2playertranslations + 256 + color_offsets[pc-1];
		colorB = colorA + 256;
		sourceA = colorB + (top * 256);
		sourceB = colorB + (bottom * 256);
		for(i=0;i<256;i++)
		{
			if (bottom > 0 && (colorB[i] != 255)) 
				translationTable[i] = sourceB[i];
			else if (top > 0 && (colorA[i] != 255)) 
				translationTable[i] = sourceA[i];
			else
				translationTable[i] = i;
		}
	}
	else
	{
		top *= 16;
		bottom *= 16;
		for (j = 0; j < 256; j++)
			identityTable[j] = j;
		dest = translationTable;
		source = identityTable;
		memcpy (dest, source, 256);

		if (top < 128)	// the artists made some backwards ranges.  sigh.
			memcpy (dest + TOP_RANGE, source + top, 16);
		else
			for (j=0 ; j<16 ; j++)
				dest[TOP_RANGE+j] = source[top+15-j];

		if (bottom < 128)
			memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
		else
			for (j=0 ; j<16 ; j++)
				dest[BOTTOM_RANGE+j] = source[bottom+15-j];
	}
}

/*
void M_DrawTransPicTranslate (int x, int y, mpic_t *pic)
{
	Draw_TransPicTranslate (x + ((vid.width - 320)>>1), y, pic, translationTable);
}*/


void M_DrawTextBox (int x, int y, int width, int lines)
{
	mpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_SafeCachePic ("gfx/box_tl.lmp");
	if (!p)
		return;	//assume we can't find any
	M_DrawScalePic (cx, cy, 8, 8, p);
	p = Draw_SafeCachePic ("gfx/box_ml.lmp");
	if (p)
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawScalePic (cx, cy, 8, 8, p);
		}
	p = Draw_SafeCachePic ("gfx/box_bl.lmp");
	if (p)
		M_DrawScalePic (cx, cy+8, 8, 8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_SafeCachePic ("gfx/box_tm.lmp");
		if (p)
			M_DrawScalePic (cx, cy, 16, 8, p);
		p = Draw_SafeCachePic ("gfx/box_mm.lmp");
		if (p)
			for (n = 0; n < lines; n++)
			{
				cy += 8;
				if (n == 1)
				{
					p = Draw_SafeCachePic ("gfx/box_mm2.lmp");
					if (!p)
						break;
				}
				M_DrawScalePic (cx, cy, 16, 8, p);
			}
		p = Draw_SafeCachePic ("gfx/box_bm.lmp");
		if (p)
			M_DrawScalePic (cx, cy+8, 16, 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_SafeCachePic ("gfx/box_tr.lmp");
	if (p)
		M_DrawScalePic (cx, cy, 8, 8, p);
	p = Draw_SafeCachePic ("gfx/box_mr.lmp");
	if (p)
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawScalePic (cx, cy, 8, 8, p);
		}
	p = Draw_SafeCachePic ("gfx/box_br.lmp");
	if (p)
		M_DrawScalePic (cx, cy+8, 8, 8, p);
}

//=============================================================================

int m_save_demonum;

void M_CloseMenu_f (void)
{
	if (key_dest != key_menu)
		return;
	M_RemoveAllMenus();
	key_dest = key_game;
	m_state = m_none;
}
/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	if (m_state)
	{
		key_dest = key_menu;
		return;
	}

#ifdef MENU_DAT
	if (MP_Toggle())
		return;
#endif
#ifdef VM_UI
	if (UI_OpenMenu())
		return;
#endif

	if (key_dest == key_menu)
	{
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
	{
		if (cls.state != ca_active)
			M_Menu_Main_f();
		else
			Con_ToggleConsole_f ();
	}
	else
	{
		M_Menu_Main_f ();
	}
}

//=============================================================================
/* KEYS MENU */

typedef struct {
	char *command;
	char *name;
} bindnames_t;

bindnames_t qwbindnames[] =
{
{"+attack", 		"attack        "},
{"impulse 10", 		"change weapon "},
{"impulse 12", 		"prev weapon   "},
{"+jump", 			"jump / swim up"},
{"+forward", 		"walk forward  "},
{"+back", 			"backpedal     "},
{"+left", 			"turn left     "},
{"+right", 			"turn right    "},
{"+speed", 			"run           "},
{"+moveleft", 		"step left     "},
{"+moveright", 		"step right    "},
{"+strafe", 		"sidestep      "},
{"+lookup", 		"look up       "},
{"+lookdown", 		"look down     "},
{"centerview", 		"center view   "},
{"+mlook", 			"mouse look    "},
{"+klook", 			"keyboard look "},
{"+moveup",			"swim up       "},
{"+movedown",		"swim down     "},
#ifdef VOICECHAT
{"+voip",			"voice chat    "},
#endif
{NULL}
};

#ifdef Q2CLIENT
bindnames_t q2bindnames[] =
{
{"+attack", 		"attack        "},
{"cmd weapnext", 	"next weapon   "},
{"+forward", 		"walk forward  "},
{"+back", 			"backpedal     "},
{"+left", 			"turn left     "},
{"+right", 			"turn right    "},
{"+speed", 			"run           "},
{"+moveleft", 		"step left     "},
{"+moveright", 		"step right    "},
{"+strafe", 		"sidestep      "},
{"+lookup", 		"look up       "},
{"+lookdown", 		"look down     "},
{"centerview", 		"center view   "},
{"+mlook", 			"mouse look    "},
{"+klook", 			"keyboard look "},
{"+moveup",			"up / jump     "},
{"+movedown",		"down / crouch "},

{"cmd inven",		"inventory     "},
{"cmd invuse",		"use item      "},
{"cmd invdrop",		"drop item     "},
{"cmd invprev",		"prev item     "},
{"cmd invnext",		"next item     "},

{"cmd help", 		"help computer "},
{NULL}
};
#endif


bindnames_t h2bindnames[] =
{
{"+attack", 		"attack        "},
{"impulse 10", 		"change weapon "},
{"+jump", 			"jump / swim up"},
{"+forward", 		"walk forward  "},
{"+back", 			"backpedal     "},
{"+left", 			"turn left     "},
{"+right", 			"turn right    "},
{"+speed", 			"run           "},
{"+moveleft", 		"step left     "},
{"+moveright", 		"step right    "},
{"+strafe", 		"sidestep      "},
{"+crouch",			"crouch        "},
{"+lookup", 		"look up       "},
{"+lookdown", 		"look down     "},
{"centerview", 		"center view   "},
{"+mlook", 			"mouse look    "},
{"+klook", 			"keyboard look "},
{"+moveup",			"swim up       "},
{"+movedown",		"swim down     "},
{"impulse 13", 		"lift object   "},
{"invuse",			"use inv item  "},
{"impulse 44",		"drop inv item "},
{"+showinfo",		"full inventory"},
{"+showdm",			"info / frags  "},
//{"toggle_dm",		"toggle frags  "},
{"+infoplaque",		"objectives    "},	//requires pulling info out of the mod... on the client.
{"invleft",			"inv move left "},
{"invright",		"inv move right"},
{"impulse 100",		"inv:torch     "},
{"impulse 101",		"inv:qrtz flask"},
{"impulse 102",		"inv:mystic urn"},
{"impulse 103",		"inv:krater    "},
{"impulse 104",		"inv:chaos devc"},
{"impulse 105",		"inv:tome power"},
{"impulse 106",		"inv:summon stn"},
{"impulse 107",		"inv:invisiblty"},
{"impulse 108",		"inv:glyph     "},
{"impulse 109",		"inv:boots     "},
{"impulse 110",		"inv:repulsion "},
{"impulse 111",		"inv:bo peep   "},
{"impulse 112",		"inv:flight    "},
{"impulse 113",		"inv:force cube"},
{"impulse 114",		"inv:icon defn "},
#ifdef VOICECHAT
{"+voip",			"voice chat    "},
#endif
{NULL}
};

bindnames_t *bindnames;
int numbindnames;

int		keys_cursor;
int		bind_grab;

void M_Menu_Keys_f (void)
{
	int y;
	menu_t *menu;
	int mgt;
	extern cvar_t cl_splitscreen, cl_forcesplitclient;

	key_dest = key_menu;
	m_state = m_complex;

	menu = M_CreateMenu(0);
	mgt = M_GameType();
#ifdef Q2CLIENT
	if (mgt == MGT_QUAKE2)	//quake2 main menu.
	{
		y = 48;
		bindnames = q2bindnames;
	}
	else
#endif
	if (mgt == MGT_HEXEN2)
	{
		MC_AddCenterPicture(menu, 0, 60, "gfx/menu/title6.lmp");
		y = 64;
		bindnames = h2bindnames;
	}
	else
	{
		MC_AddCenterPicture(menu, 4, 24, "gfx/ttl_cstm.lmp");
		y = 48;

		bindnames = qwbindnames;
	}

	if (cl_splitscreen.ival)
	{
		static char *texts[MAX_SPLITS+2] =
		{
			"Depends on device",
			"Player 1",
			"Player 2",
			"Player 3",
			"Player 4",
			NULL
		};
		static char *values[MAX_SPLITS+1] =
		{
			"0",
			"1",
			"2",
			"3",
			"4"
		};
		MC_AddCvarCombo(menu, 16, y, "Force client", &cl_forcesplitclient, texts, values);
		y+=8;
	}

	while (bindnames->name)
	{
		MC_AddBind(menu, 16, y, bindnames->name, bindnames->command);
		y += 8;

		bindnames++;
	}
}

int M_FindKeysForBind (char *command, int *keylist, int total)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	for (count = 0; count < total; count++)
		keylist[count] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j][0];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			keylist[count] = j;
			count++;
			if (count == total)
				break;
		}
	}
	return count;
}

void M_FindKeysForCommand (int pnum, char *command, int *twokeys)
{
	char prefix[5];

	if (*command == '+' || *command == '-')
	{
		prefix[0] = *command;
		prefix[1] = 0;
		if (pnum != 0)
		{
			prefix[1] = 'p';
			prefix[2] = '0'+pnum;
			prefix[3] = ' ';
			prefix[4] = 0;
		}
		command++;
	}
	else
	{
		prefix[0] = 0;
		if (pnum != 0)
		{
			prefix[0] = 'p';
			prefix[1] = '0'+pnum;
			prefix[2] = ' ';
			prefix[3] = 0;
		}
	}
	M_FindKeysForBind(va("%s%s", prefix, command), twokeys, 2);
}

void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j][0];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, ~0, "", RESTRICT_LOCAL);
	}
}

//=============================================================================
/* HELP MENU */

int		help_page;
char *helpstyle;
int		num_help_pages;
int	helppagemin;


void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	help_page = 0;

	if (COM_FDepthFile("gfx/help1.lmp", true) < COM_FDepthFile("gfx/menu/help1.lmp", true))
	{
		helpstyle = "gfx/help%i.lmp";
		num_help_pages = 6;
		helppagemin=0;
	}
	else
	{
		helpstyle = "gfx/menu/help%02i.lmp";
		num_help_pages = 5;
		helppagemin = 1;
	}
}



void M_Help_Draw (void)
{
	mpic_t *pic;
	pic = Draw_SafeCachePic(va(helpstyle, help_page+helppagemin));
	if (!pic)
		M_Menu_Main_f ();
	else
		M_DrawScalePic (0, 0, 320, 200, pic);
}


void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu2.wav");
		if (++help_page >= num_help_pages)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu2.wav");
		if (--help_page < 0)
			help_page = num_help_pages-1;
		break;
	}

}

//=============================================================================
/* QUIT MENU */

int		msgNumber;
int		m_quit_prevstate;
qboolean	wasInMenus;

char *quitMessage [] =
{
/* .........1.........2.... */
  "  Are you gonna quit    ",
  "  this game just like   ",
  "   everything else?     ",
  "                        ",

  " Milord, methinks that  ",
  "   thou art a lowly     ",
  " quitter. Is this true? ",
  "                        ",

  " Do I need to bust your ",
  "  face open for trying  ",
  "        to quit?        ",
  "                        ",

  " Man, I oughta smack you",
  "   for trying to quit!  ",
  "     Press Y to get     ",
  "      smacked out.      ",

  " Press Y to quit like a ",
  "   big loser in life.   ",
  "  Press N to stay proud ",
  "    and successful!     ",

  "   If you press Y to    ",
  "  quit, I will summon   ",
  "  Satan all over your   ",
  "      hard drive!       ",

  "  Um, Asmodeus dislikes ",
  " his children trying to ",
  " quit. Press Y to return",
  "   to your Tinkertoys.  ",

  "  If you quit now, I'll ",
  "  throw a blanket-party ",
  "   for you next time!   ",
  "                        "
};
/*
void OldM_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = rand()&7;
}


void M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case 'Y':
	case 'y':
		key_dest = key_console;
		CL_Disconnect ();
		Sys_Quit ();
		break;

	default:
		break;
	}

}

void M_Quit_Draw (void)
{
#define VSTR(x) #x
#define VSTR2(x) VSTR(x)
	char *cmsg[] = {
//    0123456789012345678901234567890123456789
	"0            QuakeWorld",
	"1          version " VSTR2(VERSION),
	"1modified by Forethought Entertainment",
	"0Based on QuakeWorld Version 2.40",
	"1",
	"0Additional Programming",
	"1 David Walton",
	"1",
	"0Id Software is not responsible for",
    "0providing technical support for",
	"0QUAKEWORLD(tm). (c)1996 Id Software,",
	"0Inc.  All Rights Reserved.",
	"0QUAKEWORLD(tm) is a trademark of Id",
	"0Software, Inc.",
	"1NOTICE: THE COPYRIGHT AND TRADEMARK",
	"1NOTICES APPEARING  IN YOUR COPY OF",
	"1QUAKE(r) ARE NOT MODIFIED BY THE USE",
	"1OF QUAKEWORLD(tm) AND REMAIN IN FULL",
	"1FORCE.",
	"0NIN(r) is a registered trademark",
	"0licensed to Nothing Interactive, Inc.",
	"0All rights reserved. Press y to exit",
	NULL };
	char **p;
	int y;

	if (wasInMenus)
	{
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}
#if 1
	M_DrawTextBox (0, 0, 38, 23);
	y = 12;
	for (p = cmsg; *p; p++, y += 8) {
		if (**p == '0')
			M_PrintWhite (16, y, *p + 1);
		else
			M_Print (16, y,	*p + 1);
	}
#else
	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  quitMessage[msgNumber*4+0]);
	M_Print (64, 92,  quitMessage[msgNumber*4+1]);
	M_Print (64, 100, quitMessage[msgNumber*4+2]);
	M_Print (64, 108, quitMessage[msgNumber*4+3]);
#endif
}
*/
qboolean MC_Quit_Key (int key, menu_t *menu)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		M_RemoveMenu(menu);
		break;

	case 'Y':
	case 'y':
		M_RemoveMenu(menu);
		key_dest = key_console;
		CL_Disconnect ();
		Sys_Quit ();
		break;

	default:
		break;
	}

	return true;
}

menu_t quitmenu;
void M_Menu_Quit_f (void)
{
	int		i;

	if (1)
	{
		CL_Disconnect ();
		Sys_Quit ();
	}
	else
	{
		key_dest = key_menu;
		m_state = m_complex;

		M_RemoveMenu(&quitmenu);
		memset(&quitmenu, 0, sizeof(quitmenu));
		M_AddMenuFront(&quitmenu);
		quitmenu.exclusive = false;
		quitmenu.key = MC_Quit_Key;


		i = rand()&7;

		MC_AddWhiteText(&quitmenu, 64, 84, quitMessage[i*4+0], false);
		MC_AddWhiteText(&quitmenu, 64, 92, quitMessage[i*4+1], false);
		MC_AddWhiteText(&quitmenu, 64, 100, quitMessage[i*4+2], false);
		MC_AddWhiteText(&quitmenu, 64, 108, quitMessage[i*4+3], false);
		MC_AddBox (&quitmenu, 56, 76, 24, 4);
	}
}

//=============================================================================
/* Menu Subsystem */

void M_Menu_ServerList2_f(void);
void M_QuickConnect_f(void);

void M_Menu_MediaFiles_f (void);
void M_Menu_FPS_f (void);
void M_Menu_Shadow_Lighting_f (void);
void M_Menu_3D_f (void);
void M_Menu_Textures_f (void);
void M_Menu_Teamplay_f (void);
void M_Menu_Teamplay_Locations_f (void);
void M_Menu_Teamplay_Needs_f (void);
void M_Menu_Teamplay_Items_f (void);
void M_Menu_Teamplay_Items_Armor_f (void);
void M_Menu_Teamplay_Items_Weapons_f (void);
void M_Menu_Teamplay_Items_Powerups_f (void);
void M_Menu_Teamplay_Items_Ammo_Health_f (void);
void M_Menu_Teamplay_Items_Team_Fortress_f (void);
void M_Menu_Teamplay_Items_Status_Location_Misc_f (void);
void M_Menu_Singleplayer_Cheats_f (void);
void M_Menu_Singleplayer_Cheats_Quake2_f (void);
void M_Menu_Singleplayer_Cheats_Hexen2_f (void);
void M_Menu_Particles_f (void);
void M_Menu_ParticleSets_f (void);
void M_Menu_Audio_Speakers_f (void);
void Menu_DownloadStuff_f (void);
static qboolean internalmenusregistered;
void M_Init_Internal (void)
{
#ifdef MENU_DAT
	MP_Shutdown();
#endif

	if (internalmenusregistered)
		return;
	internalmenusregistered = true;

#ifndef CLIENTONLY
	Cmd_AddRemCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddRemCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddRemCommand ("menu_loadgame", M_Menu_Load_f);	//q2...
#endif
	Cmd_AddRemCommand ("menu_single", M_Menu_SinglePlayer_f);
	Cmd_AddRemCommand ("menu_multi", M_Menu_MultiPlayer_f);
	Cmd_AddRemCommand ("menu_demo", M_Menu_Demos_f);

	Cmd_AddRemCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddRemCommand ("help", M_Menu_Help_f);
	Cmd_AddRemCommand ("menu_quit", M_Menu_Quit_f);
	Cmd_AddRemCommand ("menu_media", M_Menu_Media_f);
	Cmd_AddRemCommand ("menu_mediafiles", M_Menu_MediaFiles_f);

#ifdef CL_MASTER
	Cmd_AddRemCommand ("menu_servers", M_Menu_ServerList2_f);

	Cmd_AddRemCommand ("menu_serversold", M_Menu_ServerList_f);
	Cmd_AddRemCommand ("menu_slist", M_Menu_ServerList2_f);
#endif
	Cmd_AddRemCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddRemCommand ("menu_newmulti", M_Menu_GameOptions_f);

	Cmd_AddRemCommand ("menu_main", M_Menu_Main_f);	//I've moved main to last because that way tab give us main and not quit.

	Cmd_AddRemCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddRemCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddRemCommand ("menu_audio", M_Menu_Audio_f);
#ifndef __CYGWIN__
	Cmd_AddRemCommand ("menu_speakers", M_Menu_Audio_Speakers_f);
#endif
	Cmd_AddRemCommand ("menu_spcheats", M_Menu_Singleplayer_Cheats_f);
	Cmd_AddRemCommand ("menu_quake2_spcheats", M_Menu_Singleplayer_Cheats_Quake2_f);
	Cmd_AddRemCommand ("menu_hexen2_spcheats", M_Menu_Singleplayer_Cheats_Hexen2_f);
	Cmd_AddRemCommand ("menu_fps", M_Menu_FPS_f);
	Cmd_AddRemCommand ("menu_3d" , M_Menu_3D_f);
	Cmd_AddRemCommand ("menu_shadow_lighting", M_Menu_Shadow_Lighting_f);
	Cmd_AddRemCommand ("menu_textures", M_Menu_Textures_f);
	Cmd_AddRemCommand ("menu_teamplay", M_Menu_Teamplay_f);
	Cmd_AddRemCommand ("menu_teamplay_locations", M_Menu_Teamplay_Locations_f);
	Cmd_AddRemCommand ("menu_teamplay_needs", M_Menu_Teamplay_Needs_f);
	Cmd_AddRemCommand ("menu_teamplay_items", M_Menu_Teamplay_Items_f);
	Cmd_AddRemCommand ("menu_teamplay_armor", M_Menu_Teamplay_Items_Armor_f);
	Cmd_AddRemCommand ("menu_teamplay_weapons", M_Menu_Teamplay_Items_Weapons_f);
	Cmd_AddRemCommand ("menu_teamplay_powerups", M_Menu_Teamplay_Items_Powerups_f);
	Cmd_AddRemCommand ("menu_teamplay_ammo_health", M_Menu_Teamplay_Items_Ammo_Health_f);
	Cmd_AddRemCommand ("menu_teamplay_team_fortress", M_Menu_Teamplay_Items_Team_Fortress_f);
	Cmd_AddRemCommand ("menu_teamplay_status_location_misc", M_Menu_Teamplay_Items_Status_Location_Misc_f);
	Cmd_AddRemCommand ("menu_particles", M_Menu_Particles_f);
	Cmd_AddRemCommand ("menu_particlesets", M_Menu_ParticleSets_f);

#ifdef WEBCLIENT
	Cmd_AddRemCommand ("menu_download", Menu_DownloadStuff_f);
#endif

	Cmd_AddRemCommand ("quickconnect", M_QuickConnect_f);
}

void M_DeInit_Internal (void)
{
	M_RemoveAllMenus();

	if (!internalmenusregistered)
		return;
	internalmenusregistered = false;

#ifndef CLIENTONLY
	Cmd_RemoveCommand ("menu_save");
	Cmd_RemoveCommand ("menu_load");
	Cmd_RemoveCommand ("menu_loadgame");	//q2...
#endif
	Cmd_RemoveCommand ("menu_single");
	Cmd_RemoveCommand ("menu_multi");
	Cmd_RemoveCommand ("menu_demo");

	Cmd_RemoveCommand ("menu_keys");
	Cmd_RemoveCommand ("help");
	Cmd_RemoveCommand ("menu_quit");
	Cmd_RemoveCommand ("menu_media");
	Cmd_RemoveCommand ("menu_mediafiles");

#ifdef CL_MASTER
	Cmd_RemoveCommand ("menu_servers");
	Cmd_RemoveCommand ("menu_serversold");
	Cmd_RemoveCommand ("menu_slist");
#endif
	Cmd_RemoveCommand ("menu_setup");
	Cmd_RemoveCommand ("menu_newmulti");

	Cmd_RemoveCommand ("menu_options");
	Cmd_RemoveCommand ("menu_video");
	Cmd_RemoveCommand ("menu_audio");
	Cmd_RemoveCommand ("menu_speakers");
	Cmd_RemoveCommand ("menu_teamplay");
	Cmd_RemoveCommand ("menu_teamplay_locations");
	Cmd_RemoveCommand ("menu_teamplay_needs");
	Cmd_RemoveCommand ("menu_teamplay_items");
	Cmd_RemoveCommand ("menu_teamplay_armor");
	Cmd_RemoveCommand ("menu_teamplay_weapons");
	Cmd_RemoveCommand ("menu_teamplay_powerups");
	Cmd_RemoveCommand ("menu_teamplay_ammo_health");
	Cmd_RemoveCommand ("menu_teamplay_team_fortress");
	Cmd_RemoveCommand ("menu_teamplay_status_location_misc");
	Cmd_RemoveCommand ("menu_spcheats");
	Cmd_RemoveCommand ("menu_hexen2_spcheats");
	Cmd_RemoveCommand ("menu_quake2_spcheats");
	Cmd_RemoveCommand ("menu_fps");
	Cmd_RemoveCommand ("menu_3d");
	Cmd_RemoveCommand ("menu_shadow_lighting");
	Cmd_RemoveCommand ("menu_textures");
	Cmd_RemoveCommand ("menu_particles");
	Cmd_RemoveCommand ("menu_particlesets");

	Cmd_RemoveCommand ("menu_download");


	Cmd_RemoveCommand ("menu_main");	//I've moved main to last because that way tab give us main and not quit.
	Cmd_RemoveCommand ("quickconnect");
}

void M_Shutdown(void)
{
#ifdef MENU_DAT
	MP_Shutdown();
#endif
	M_DeInit_Internal();
}

void M_Reinit(void)
{
#ifdef MENU_DAT
	if (!MP_Init())
#endif
	{
		M_Init_Internal();
	}
}

void FPS_Preset_f(void);

//menu.dat is loaded later... after the video and everything is up.
void M_Init (void)
{

	Cmd_AddCommand("togglemenu", M_ToggleMenu_f);
	Cmd_AddCommand("closemenu", M_CloseMenu_f);
	Cmd_AddCommand("fps_preset", FPS_Preset_f);

	Cvar_Register(&m_helpismedia, "Menu thingumiebobs");

	Media_Init();
#ifdef CL_MASTER
	M_Serverlist_Init();
#endif
	M_Script_Init();

	M_Reinit();
}


void M_Draw (int uimenu)
{
	if (uimenu)
	{
		if (uimenu == 2)
			Draw_FadeScreen ();
#ifdef VM_UI
		UI_DrawMenu();
#endif
	}

	if (m_state != m_complex)
	{
		M_RemoveAllMenus();
	}
	if (key_dest != key_menu)
	{
		m_state = m_none;
		return;
	}

	if (m_state == m_none)
		return;

	if ((!menu_script || scr_con_current) && !m_recursiveDraw)
	{
		Draw_FadeScreen ();
	}
	else
	{
		m_recursiveDraw = false;
	}

	Draw_ImageColours(1, 1, 1, 1);

	switch (m_state)
	{
	case m_none:
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_slist:
		M_ServerList_Draw ();
		break;

	case m_media:
		M_Media_Draw ();
		break;

	case m_complex:
		M_Complex_Draw ();
		break;
#ifdef PLUGINS
	case m_plugin:
		Plug_Menu_Event (0, (int)(realtime*1000));
		break;
#endif
#ifdef MENU_DAT
	case m_menu_dat:
//		MP_Draw();
		return;
#endif
	}
}


void M_Keydown (int key, int unicode)
{
	switch (m_state)
	{
	case m_none:
		key_dest = key_console;
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_slist:
		M_ServerList_Key (key);
		return;

	case m_media:
		M_Media_Key (key);
		return;

	case m_complex:
		M_Complex_Key (key, unicode);
		return;
#ifdef PLUGINS
	case m_plugin:
		Plug_Menu_Event (1, key);
		return;
#endif
#ifdef MENU_DAT
	case m_menu_dat:
		MP_Keydown(key, unicode);
		return;
#endif
	}
}


void M_Keyup (int key, int unicode)
{
	switch (m_state)
	{
#ifdef PLUGINS
	case m_plugin:
		Plug_Menu_Event (2, key);
		return;
#endif
#ifdef MENU_DAT
	case m_menu_dat:
		MP_Keyup(key, unicode);
		return;
#endif
	default:
		break;
	}
}

// Generic function to choose which game menu to draw
int M_GameType (void)
{
	static int cached;
	static unsigned int cachedrestarts;

	if (FS_Restarted(&cachedrestarts))
	{
#if defined(Q2CLIENT)
		int q1, h2, q2;

		q1 = COM_FDepthFile("gfx/sp_menu.lmp", true);
		h2 = COM_FDepthFile("gfx/menu/title2.lmp", true);
		q2 = COM_FDepthFile("pics/m_banner_game.pcx", true);

		if (q2 < h2 && q2 < q1)
			cached = MGT_QUAKE2;
		else if (h2 < q1)
			cached = MGT_HEXEN2;
		else
#endif
			cached = MGT_QUAKE1;
	}

	return cached;
}


