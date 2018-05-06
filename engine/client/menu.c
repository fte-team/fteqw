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
#include "cl_master.h"

qboolean menu_mousedown;

void M_DrawScalePic (int x, int y, int w, int h, mpic_t *pic)
{
	R2D_ScalePic (x + ((vid.width - 320)>>1), y, w, h, pic);
}
void M_DrawTextBox (int x, int y, int width, int lines)
{
	mpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = R2D_SafeCachePic ("gfx/box_tl.lmp");
	switch(R_GetShaderSizes(p, NULL, NULL, false))
	{
	case -1:
		return;	//still pending
	case 0:
		R2D_ImageColours(0.0, 0.0, 0.0, 1.0);
		R2D_FillBlock(x + ((vid.width - 320)>>1), y, width*8+16, lines*8+16);
		R2D_ImageColours(1.0, 1.0, 1.0, 1.0);
		return;
	}
	M_DrawScalePic (cx, cy, 8, 8, p);
	p = R2D_SafeCachePic ("gfx/box_ml.lmp");
	if (p)
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawScalePic (cx, cy, 8, 8, p);
		}
	p = R2D_SafeCachePic ("gfx/box_bl.lmp");
	if (p)
		M_DrawScalePic (cx, cy+8, 8, 8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = R2D_SafeCachePic ("gfx/box_tm.lmp");
		if (p)
			M_DrawScalePic (cx, cy, 16, 8, p);
		p = R2D_SafeCachePic ("gfx/box_mm.lmp");
		if (p)
			for (n = 0; n < lines; n++)
			{
				cy += 8;
				if (n == 1)
				{
					p = R2D_SafeCachePic ("gfx/box_mm2.lmp");
					if (!p)
						break;
				}
				M_DrawScalePic (cx, cy, 16, 8, p);
			}
		p = R2D_SafeCachePic ("gfx/box_bm.lmp");
		if (p)
			M_DrawScalePic (cx, cy+8, 16, 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = R2D_SafeCachePic ("gfx/box_tr.lmp");
	if (p)
		M_DrawScalePic (cx, cy, 8, 8, p);
	p = R2D_SafeCachePic ("gfx/box_mr.lmp");
	if (p)
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawScalePic (cx, cy, 8, 8, p);
		}
	p = R2D_SafeCachePic ("gfx/box_br.lmp");
	if (p)
		M_DrawScalePic (cx, cy+8, 8, 8, p);
}

int M_FindKeysForBind (int bindmap, const char *command, int *keylist, int *keymods, int keycount)
{
	int		count;
	int		j, m;
	int		l, p;
	char	*b;
	int		firstmod, lastmod;

	l = strlen(command);
	count = 0;

	if (bindmap > 0 && bindmap <= KEY_MODIFIER_ALTBINDMAP)
	{
		//bindmaps don't support modifiers
		firstmod = (bindmap-1)|KEY_MODIFIER_ALTBINDMAP;
		lastmod = firstmod+1;
	}
	else
	{
		firstmod = 0;
		lastmod = KEY_MODIFIER_ALTBINDMAP;
	}

	for (j=0 ; j<K_MAX ; j++)
	{
		for (m = firstmod; m < lastmod; m++)
		{
			b = keybindings[j][m];
			if (!b)
				continue;
			if (!strncmp (b, command, l) && (!b[l] || b[l] == ' ' || b[l] == ';'))
			{
				//if ctrl_a and ctrl_shift_a do the same thing, don't report ctrl_shift_a because its redundant.
				for (p = firstmod; p < m; p++)
				{
					if (p&~m)	//ignore shift_a if we're checking ctrl_a
						continue;
					if (keybindings[j][p] && !strcmp(keybindings[j][p], b))
						break;	//break+continue
				}
				if (p != m)
					continue;

				keylist[count] = j;
				if (keymods)
					keymods[count] = m;
				count++;
				if (count == keycount)
					return count;
			}
		}
	}
	for (j = count; j < keycount; j++)
	{
		keylist[j] = -1;
		if (keymods)
			keymods[j] = 0;
	}
	return count;
}
int M_FindKeysForCommand (int bindmap, int pnum, const char *command, int *keylist, int *keymods, int keycount)
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
	return M_FindKeysForBind(bindmap, va("%s%s", prefix, command), keylist, keymods, keycount);
}

#ifndef NOBUILTINMENUS

void M_Menu_Audio_f (void);
void M_Menu_Demos_f (void);
void M_Menu_Mods_f (void);
void M_Menu_ModelViewer_f(void);

extern menu_t *menu_script;

qboolean	m_recursiveDraw;

void M_ConfigureNetSubsystem(void);

cvar_t m_helpismedia = CVAR("m_helpismedia", "0");
cvar_t m_preset_chosen = CVARF("m_preset_chosen", "0", CVAR_ARCHIVE);

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

void M_BuildTranslationTable(unsigned int pc, unsigned int top, unsigned int bottom, unsigned int *translationTable)
{
	int		j;
#ifdef HEXEN2
	if (h2playertranslations && pc)
	{
		int i;
		unsigned int color_offsets[5] = {2*14*256,0,1*14*256,2*14*256,2*14*256};
		unsigned char *colorA, *colorB, *sourceA, *sourceB;
		colorA = h2playertranslations + 256 + color_offsets[pc-1];
		colorB = colorA + 256;
		sourceA = colorB + (top * 256);
		sourceB = colorB + (bottom * 256);
		for(i=0;i<255;i++)
		{
			if (bottom > 0 && (colorB[i] != 255))
			{
				if (bottom >= 16)
				{
					unsigned int v = d_8to24rgbtable[colorB[i]];
					v = max(max((v>>0)&0xff, (v>>8)&0xff), (v>>16)&0xff);
					*((unsigned char*)&translationTable[i]+0) = (((bottom&0xff0000)>>16)*v)>>8;
					*((unsigned char*)&translationTable[i]+1) = (((bottom&0x00ff00)>> 8)*v)>>8;
					*((unsigned char*)&translationTable[i]+2) = (((bottom&0x0000ff)>> 0)*v)>>8;
					*((unsigned char*)&translationTable[i]+3) = 0xff;
				}
				else
					translationTable[i] = d_8to24rgbtable[sourceB[i]] | 0xff000000;
			}
			else if (top > 0 && (colorA[i] != 255))
			{
				if (top >= 16)
				{
					unsigned int v = d_8to24rgbtable[colorA[i]];
					v = max(max((v>>0)&0xff, (v>>8)&0xff), (v>>16)&0xff);
					*((unsigned char*)&translationTable[i]+0) = (((top&0xff0000)>>16)*v)>>8;
					*((unsigned char*)&translationTable[i]+1) = (((top&0x00ff00)>> 8)*v)>>8;
					*((unsigned char*)&translationTable[i]+2) = (((top&0x0000ff)>> 0)*v)>>8;
					*((unsigned char*)&translationTable[i]+3) = 0xff;
				}
				else
					translationTable[i] = d_8to24rgbtable[sourceA[i]] | 0xff000000;
			}
			else
				translationTable[i] = d_8to24rgbtable[i] | 0xff000000;
		}
	}
	else
#endif
	{
		for(j=0;j<255;j++)
		{
			if (j >= TOP_RANGE && j < TOP_RANGE + (1<<4))
			{
				if (top >= 16)
				{
					*((unsigned char*)&translationTable[j]+0) = (((top&0xff0000)>>16)**((unsigned char*)&d_8to24rgbtable[j&15]+0))>>8;
					*((unsigned char*)&translationTable[j]+1) = (((top&0x00ff00)>> 8)**((unsigned char*)&d_8to24rgbtable[j&15]+1))>>8;
					*((unsigned char*)&translationTable[j]+2) = (((top&0x0000ff)>> 0)**((unsigned char*)&d_8to24rgbtable[j&15]+2))>>8;
					*((unsigned char*)&translationTable[j]+3) = 0xff;
				}
				else
					translationTable[j] = d_8to24rgbtable[top<8?j-TOP_RANGE+(top<<4):(top<<4)+15-(j-TOP_RANGE)] | 0xff000000;
			}
			else if (j >= BOTTOM_RANGE && j < BOTTOM_RANGE + (1<<4))
			{
				if (bottom >= 16)
				{
					*((unsigned char*)&translationTable[j]+0) = (((bottom&0xff0000)>>16)**((unsigned char*)&d_8to24rgbtable[j&15]+0))>>8;
					*((unsigned char*)&translationTable[j]+1) = (((bottom&0x00ff00)>> 8)**((unsigned char*)&d_8to24rgbtable[j&15]+1))>>8;
					*((unsigned char*)&translationTable[j]+2) = (((bottom&0x0000ff)>> 0)**((unsigned char*)&d_8to24rgbtable[j&15]+2))>>8;
					*((unsigned char*)&translationTable[j]+3) = 0xff;
				}
				else
					translationTable[j] = d_8to24rgbtable[bottom<8?j-BOTTOM_RANGE+(bottom<<4):(bottom<<4)+15-(j-BOTTOM_RANGE)] | 0xff000000;
			}
			else
				translationTable[j] = d_8to24rgbtable[j] | 0xff000000;
		}
	}
	translationTable[255] = 0;	//alpha
}

//=============================================================================

int m_save_demonum;

void M_CloseMenu_f (void)
{
	if (!Key_Dest_Has(kdm_emenu))
		return;
	M_RemoveAllMenus(false);
	Key_Dest_Remove(kdm_emenu);
}
/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	if (topmenu)
	{
		Key_Dest_Add(kdm_emenu);
		return;
	}

#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(-1, "togglemenu"))
	{
		Key_Dest_Remove(kdm_console|kdm_cwindows);
		return;
	}
#endif
#ifdef MENU_DAT
	if (MP_Toggle(1))
	{
		Key_Dest_Remove(kdm_console|kdm_cwindows);
		return;
	}
#endif
#ifdef VM_UI
	if (UI_OpenMenu())
		return;
#endif

	//it IS a toggle, so close the menu if its already active
	if (Key_Dest_Has(kdm_emenu))
	{
		Key_Dest_Remove(kdm_emenu);
		return;
	}
	if (Key_Dest_Has(kdm_console|kdm_cwindows))
		Key_Dest_Remove(kdm_console|kdm_cwindows);
/*
	{
		if (cls.state != ca_active)
		{
			Key_Dest_Remove(kdm_console);
			M_Menu_Main_f();
		}
		else
			Con_ToggleConsole_Force ();
	}
	else*/
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
{"cmd weapprev", 	"prev weapon   "},
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
	vfsfile_t *bindslist;
#if MAX_SPLITS > 1
	extern cvar_t cl_splitscreen;
#endif

	Key_Dest_Add(kdm_emenu);

	menu = M_CreateMenu(0);
	switch(M_GameType())
	{
#ifdef Q2CLIENT
	case MGT_QUAKE2:
		//fixme: no art?
		y = 48;
		bindnames = q2bindnames;
		break;
#endif
#ifdef HEXEN2
	case MGT_HEXEN2:
		MC_AddCenterPicture(menu, 0, 60, "gfx/menu/title6.lmp");
		y = 64;
		bindnames = h2bindnames;
		break;
#endif
	default:
		MC_AddCenterPicture(menu, 4, 24, "gfx/ttl_cstm.lmp");
		y = 48;
		bindnames = qwbindnames;
		break;
	}

#if MAX_SPLITS > 1
	if (cl.splitclients || cl_splitscreen.ival || cl_forceseat.ival)
	{
		static char *texts[MAX_SPLITS+2] =
		{
			"Depends on device",
			"Player 1",
			"Player 2",
#if MAX_SPLITS >= 3
			"Player 3",
#endif
#if MAX_SPLITS >= 4
			"Player 4",
#endif
			NULL
		};
		static char *values[MAX_SPLITS+1] =
		{
			"0",
			"1",
			"2",
#if MAX_SPLITS >= 3
			"3",
#endif
#if MAX_SPLITS >= 4
			"4"
#endif
		};
		MC_AddCvarCombo(menu, 16, 170, y, "Force client", &cl_forceseat, (const char **)texts, (const char **)values);
		y+=8;
	}
#endif

	bindslist = FS_OpenVFS("bindlist.lst", "rb", FS_GAME);
	if (bindslist)
	{
		char line[1024];
		while(VFS_GETS(bindslist, line, sizeof(line)))
		{
			char *cmd, *desc, *tip;
			Cmd_TokenizeString(line, false, false);
			cmd = Cmd_Argv(0);
			desc = Cmd_Argv(1);
			tip = Cmd_Argv(2);
			if (*cmd)
			{
				if (strcmp(cmd, "-"))	//lines with a command of "-" are spacers/comments.
					MC_AddBind(menu, (320-(int)vid.width)/2, 170, y, desc, cmd, tip);
				else if (*desc)
					MC_AddRedText(menu, (320-(int)vid.width)/2, 170, y, desc, true);
				y += 8;
			}
		}
		VFS_CLOSE(bindslist);
		return;
	}

	while (bindnames->name)
	{
		MC_AddBind(menu, 16, 170, y, bindnames->name, bindnames->command, NULL);
		y += 8;

		bindnames++;
	}
}

void M_UnbindCommand (const char *command)
{
	int		j;
	int		l;
	char	*b;
	int m;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{	//FIXME: not sure what to do about bindmaps here. oh well.
		for (m = 0; m < KEY_MODIFIERSTATES; m++)
		{
			b = keybindings[j][m];
			if (!b)
				continue;
			if (!strncmp (b, command, l) )
				Key_SetBinding (j, m, "", RESTRICT_LOCAL);
		}
	}
}

//=============================================================================
/* HELP MENU */

int		help_page;
int		num_help_pages;

struct
{
	char *pattern;
	int base;
} helpstyles[] =
{
	{"gfx/help%i.dxt",0},			//quake extended
	{"gfx/help%i.tga",0},			//quake extended
	{"gfx/help%i.png",0},			//quake extended
	{"gfx/help%i.jpeg",0},			//quake extended
	{"gfx/help%i.lmp",0},			//quake
	{"gfx/menu/help%02i.lmp",1}		//hexen2
};

void M_Help_Draw (menu_t *m)
{
	int i;
	mpic_t *pic = NULL;
	for (i = 0; i < sizeof(helpstyles)/sizeof(helpstyles[0]) && !pic; i++)
	{
		pic = R2D_SafeCachePic(va(helpstyles[i].pattern, help_page+helpstyles[i].base));
		if (R_GetShaderSizes(pic, NULL, NULL, true) <= 0)
			pic = NULL;
	}
	if (!pic)
		M_Menu_Main_f ();
	else
	{
		//define default aspect ratio
		int width = 320;
		int height = 200;

		//figure out which axis we're meeting.
		if (vid.width/(float)width > vid.height/(float)height)
		{
			width = width * (vid.height/(float)height);
			height = vid.height;
		}
		else
		{
			height = height * (vid.width/(float)width);
			width = vid.width;
		}
		R2D_ScalePic ((vid.width-width)/2, (vid.height-height)/2, width, height, pic);
	}
}
qboolean M_Help_Key (int key, menu_t *m)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_GP_BACK:
	case K_MOUSE2:
		M_Menu_Main_f ();
		return true;

	case K_UPARROW:
	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
	case K_GP_DPAD_RIGHT:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		if (++help_page >= num_help_pages)
			help_page = 0;
		return true;

	case K_DOWNARROW:
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
	case K_GP_DPAD_LEFT:
		S_LocalSound ("misc/menu2.wav");
		if (--help_page < 0)
			help_page = num_help_pages-1;
		return true;
	default:
		return false;
	}
}

void M_Menu_Help_f (void)
{
	int i;
	menu_t *helpmenu = M_CreateMenu(0);
	Key_Dest_Add(kdm_emenu);

	helpmenu->predraw = M_Help_Draw;
	helpmenu->key = M_Help_Key;

	help_page = 0;

	num_help_pages = 1;
	while(num_help_pages < 100)
	{
		for (i = 0; i < sizeof(helpstyles)/sizeof(helpstyles[0]); i++)
		{
			if (COM_FCheckExists(va(helpstyles[i].pattern, num_help_pages+helpstyles[i].base)))
				break;
		}
		if (i == sizeof(helpstyles)/sizeof(helpstyles[0]))
			break;
		num_help_pages++;
	}
}


//=============================================================================
/* Various callback-based prompts */

typedef struct
{
	menu_t m;
	void (*callback)(void *, int);
	void *ctx;
	menubutton_t *b_yes;
	menubutton_t *b_no;
	menubutton_t *b_cancel;
} promptmenu_t;
static qboolean M_Menu_Prompt_Button (union menuoption_s *b,struct menu_s *gm, int key)
{
	int action;
	promptmenu_t *m = (promptmenu_t*)gm;
	void (*callback)(void *, int) = m->callback;
	void *ctx = m->ctx;

	if (key != K_ENTER && key != K_KP_ENTER && key != K_MOUSE1)
		return true;

	if (b == (menuoption_t*)m->b_yes)
		action = 0;
	else if (b == (menuoption_t*)m->b_no)
		action = 1;
	else //if (b == (menuoption_t*)m->b_cancel)
		action = -1;
	m->callback = NULL;

	M_RemoveMenu(&m->m);

	if (callback)
		callback(ctx, action);
	return true;
}
static void M_Menu_Prompt_Cancel (struct menu_s *gm)
{
	promptmenu_t *m = (promptmenu_t*)gm;
	void (*callback)(void *, int) = m->callback;
	void *ctx = m->ctx;
	m->callback = NULL;

	if (callback)
		callback(ctx, -1);
}
void M_Menu_Prompt (void (*callback)(void *, int), void *ctx, const char *messages, char *optionyes, char *optionno, char *optioncancel)
{
	promptmenu_t *m;
	char *t;
	int y;
	int x = 64, w = 224;

	Key_Dest_Add(kdm_emenu);

	m = (promptmenu_t*)M_CreateMenuInfront(sizeof(*m) - sizeof(m->m) + strlen(messages)+(optionyes?strlen(optionyes):0)+(optionno?strlen(optionno):0)+(optioncancel?strlen(optioncancel):0)+6);
	m->callback =  callback;
	m->ctx = ctx;
	m->m.remove = M_Menu_Prompt_Cancel;

	t = (char*)(m+1);
	if (optionyes)
	{
		strcpy(t, optionyes);
		optionyes = t;
		t += strlen(t)+1;
	}
	if (optionno)
	{
		strcpy(t, optionno);
		optionno = t;
		t += strlen(t)+1;
	}
	if (optioncancel)
	{
		strcpy(t, optioncancel);
		optioncancel = t;
		t += strlen(t)+1;
	}

	y = 76;
	y += 8;	//top border
	strcpy(t, messages);

	for(messages = t; t; y += 8)
	{
		messages = t;
		t = strchr(messages, '\n');
		if (t)
			*t++ = 0;
		if (*messages)
			MC_AddWhiteText(&m->m, x, x+w, y, messages, 2);
	}

	y += 8;	//blank space

	if (optionyes)
	{
		m->b_yes	= MC_AddCommand(&m->m, x, x+70, y, optionyes,		M_Menu_Prompt_Button);
		m->b_yes->rightalign = 2;
	}
	if (optionno)
	{
		m->b_no		= MC_AddCommand(&m->m, x+w/3, x+(2*w)/3, y, optionno,		M_Menu_Prompt_Button);
		m->b_no->rightalign = 2;	//actually center align
	}
	if (optioncancel)
	{
		m->b_cancel	= MC_AddCommand(&m->m, x+(2*w)/3, x+w, y, optioncancel,	M_Menu_Prompt_Button);
		m->b_cancel->rightalign = 2;
	}
	y += 8; //footer

	y += 8;	//bottom border

	m->m.selecteditem = (menuoption_t *)m->b_cancel;

	MC_AddBox (&m->m, x-8, 76, (w/8), (y-76)/8);
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
			key_dest = key_game;
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
	case K_GP_BACK:
	case 'n':
	case 'N':
		M_RemoveMenu(menu);
		break;

	case 'q':
	case 'Q':
	case 'Y':
	case 'y':
		M_RemoveMenu(menu);
		Key_Dest_Add(kdm_console);
		CL_Disconnect ();
		Sys_Quit ();
		break;

	default:
		return false;
	}

	return true;
}

void Cmd_WriteConfig_f(void);
qboolean MC_SaveQuit_Key (int key, menu_t *menu)
{
	switch (key)
	{
	case 'o':
	case 'O':
	case K_ESCAPE:
	case K_GP_BACK:
	case K_MOUSE2:
		M_RemoveMenu(menu);
		break;

	case 'q':
	case 'Q':
	case 'n':
	case 'N':
		M_RemoveMenu(menu);
#ifndef FTE_TARGET_WEB
		CL_Disconnect ();
		Sys_Quit ();
#endif
		break;

	case 'Y':
	case 'y':
		M_RemoveMenu(menu);
		Cmd_ExecuteString("cfg_save", RESTRICT_LOCAL);
#ifndef FTE_TARGET_WEB
		CL_Disconnect ();
		Sys_Quit ();
#endif
		break;

	default:
		return false;
	}

	return true;
}

//quit menu
void M_Menu_Quit_f (void)
{
	menu_t *quitmenu;
	int mode;
	extern cvar_t cfg_save_auto;
	char *arg = Cmd_Argv(1);

	MasterInfo_WriteServers();

	if (!strcmp(arg, "force"))
		mode = 0;
	else if (!strcmp(arg, "forcesave") || cfg_save_auto.ival)
	{
		Cmd_ExecuteString("cfg_save", RESTRICT_LOCAL);
		mode = 0;
	}
	else if (!strcmp(arg, "save"))
		mode = 2;
	else
	{	//prompt to save, but not otherwise.
		if (Cvar_UnsavedArchive())
			mode = 2;
		else
		{
			if (!strcmp(arg, "prompt"))
				mode = 1;
			else if (!strcmp(arg, "noprompt"))
				mode = 0;
			else
				mode = 1;
		}
	}

	switch(mode)
	{
	case 0:
#ifndef FTE_TARGET_WEB
		CL_Disconnect ();
		Sys_Quit ();
#endif
		break;
	case 2:
		Key_Dest_Add(kdm_emenu);
		Key_Dest_Remove(kdm_console);

		quitmenu = M_CreateMenuInfront(0);
		quitmenu->key = MC_SaveQuit_Key;

		MC_AddWhiteText(quitmenu, 64, 0, 84,	 "You have unsaved settings ", false);
		MC_AddWhiteText(quitmenu, 64, 0, 92,	 "    Would you like to     ", false);
		MC_AddWhiteText(quitmenu, 64, 0, 100,	 "      save them now?      ", false);

		quitmenu->selecteditem = (menuoption_t *)
#ifdef FTE_TARGET_WEB
		MC_AddConsoleCommand    (quitmenu, 64, 0, 116, "Yes",							"cfg_save; menupop\n");
		MC_AddConsoleCommand    (quitmenu, 224,0, 116,                     "Cancel",	"menupop\n");
#else
		MC_AddConsoleCommand    (quitmenu, 64, 0, 116, "Yes",							"menu_quit forcesave\n");
		MC_AddConsoleCommand    (quitmenu, 144,0, 116,           "No",					"menu_quit force\n");
		MC_AddConsoleCommand    (quitmenu, 224,0, 116,                     "Cancel",	"menupop\n");
#endif

		MC_AddBox (quitmenu, 56, 76, 25, 5);
		break;
	case 1:
		Key_Dest_Add(kdm_emenu);
		Key_Dest_Remove(kdm_console);

		quitmenu = M_CreateMenuInfront(0);
		quitmenu->key = MC_Quit_Key;


#ifdef FTE_TARGET_WEB

//		MC_AddWhiteText(quitmenu, 64, 0, 84,	 "                          ", false);
		MC_AddWhiteText(quitmenu, 64, 0, 92,	 " There is nothing to save ", false);
//		MC_AddWhiteText(quitmenu, 64, 0, 100,	 "                          ", false);

		quitmenu->selecteditem = (menuoption_t *)
		MC_AddConsoleCommand    (quitmenu, 120, 0, 116,        "Oh",			       "menupop\n");
#else
		{
			int		i = rand()&7;
			MC_AddWhiteText(quitmenu, 64, 0, 84, quitMessage[i*4+0], false);
			MC_AddWhiteText(quitmenu, 64, 0, 92, quitMessage[i*4+1], false);
			MC_AddWhiteText(quitmenu, 64, 0, 100, quitMessage[i*4+2], false);
			MC_AddWhiteText(quitmenu, 64, 0, 108, quitMessage[i*4+3], false);
		}

		quitmenu->selecteditem = (menuoption_t *)
		MC_AddConsoleCommand    (quitmenu, 100, 0, 116,        "Quit",			       "menu_quit force\n");
		MC_AddConsoleCommand    (quitmenu, 194, 0, 116,                   "Cancel",        "menupop\n");
#endif
		MC_AddBox (quitmenu, 56, 76, 24, 5);
		break;
	}
}

//=============================================================================
/* Menu Subsystem */

void M_Menu_ServerList2_f(void);
void M_QuickConnect_f(void);

void M_Menu_MediaFiles_f (void);
void M_Menu_FPS_f (void);
void M_Menu_Lighting_f (void);
void M_Menu_Render_f (void);
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
void M_Menu_Network_f(void);
void M_Menu_Singleplayer_Cheats_f (void);
void M_Menu_Particles_f (void);
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
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_loadgame", M_Menu_Load_f);	//q2...
#endif
	Cmd_AddCommand ("menu_single", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_multi", M_Menu_MultiPlayer_f);

	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
	Cmd_AddCommand ("menu_mods", M_Menu_Mods_f);
	Cmd_AddCommand ("modelviewer", M_Menu_ModelViewer_f);

#ifdef CL_MASTER
	Cmd_AddCommand ("menu_slist", M_Menu_ServerList2_f);
#endif
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_newmulti", M_Menu_GameOptions_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);	//I've moved main to last because that way tab give us main and not quit.

	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_audio", M_Menu_Audio_f);
#ifndef __CYGWIN__
	Cmd_AddCommand ("menu_speakers", M_Menu_Audio_Speakers_f);
#endif
	Cmd_AddCommand ("menu_spcheats", M_Menu_Singleplayer_Cheats_f);
	Cmd_AddCommand ("menu_fps", M_Menu_FPS_f);
	Cmd_AddCommand ("menu_render" , M_Menu_Render_f);
	Cmd_AddCommand ("menu_lighting", M_Menu_Lighting_f);
#ifdef GLQUAKE
	Cmd_AddCommand ("menu_textures", M_Menu_Textures_f);
#endif
	Cmd_AddCommand ("menu_teamplay", M_Menu_Teamplay_f);
	Cmd_AddCommand ("menu_teamplay_locations", M_Menu_Teamplay_Locations_f);
	Cmd_AddCommand ("menu_teamplay_needs", M_Menu_Teamplay_Needs_f);
	Cmd_AddCommand ("menu_teamplay_items", M_Menu_Teamplay_Items_f);
	Cmd_AddCommand ("menu_teamplay_armor", M_Menu_Teamplay_Items_Armor_f);
	Cmd_AddCommand ("menu_teamplay_weapons", M_Menu_Teamplay_Items_Weapons_f);
	Cmd_AddCommand ("menu_teamplay_powerups", M_Menu_Teamplay_Items_Powerups_f);
	Cmd_AddCommand ("menu_teamplay_ammo_health", M_Menu_Teamplay_Items_Ammo_Health_f);
	Cmd_AddCommand ("menu_teamplay_team_fortress", M_Menu_Teamplay_Items_Team_Fortress_f);
	Cmd_AddCommand ("menu_teamplay_status_location_misc", M_Menu_Teamplay_Items_Status_Location_Misc_f);
	Cmd_AddCommand ("menu_particles", M_Menu_Particles_f);
	Cmd_AddCommand ("menu_network", M_Menu_Network_f);

#ifdef CL_MASTER
	Cmd_AddCommand ("quickconnect", M_QuickConnect_f);
#endif
}

void M_DeInit_Internal (void)
{
	M_RemoveAllMenus(true);

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

	Cmd_RemoveCommand ("menu_keys");
	Cmd_RemoveCommand ("help");
	Cmd_RemoveCommand ("menu_quit");

#ifdef CL_MASTER
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
	Cmd_RemoveCommand ("menu_fps");
	Cmd_RemoveCommand ("menu_render");
	Cmd_RemoveCommand ("menu_lighting");
	Cmd_RemoveCommand ("menu_textures");
	Cmd_RemoveCommand ("menu_particles");


	Cmd_RemoveCommand ("menu_main");	//I've moved main to last because that way tab gives us main and not quit.
	Cmd_RemoveCommand ("quickconnect");
}

void M_Shutdown(qboolean total)
{
#ifdef MENU_DAT
	MP_Shutdown();
#endif
	M_RemoveAllMenus(!total);
	M_DeInit_Internal();
}

void M_Reinit(void)
{
#ifdef MENU_DAT
	if (!MP_Init())
#endif
	{
		M_Init_Internal();

		(void)CSQC_UnconnectedInit();
	}
}

void FPS_Preset_f(void);
void M_MenuPop_f(void);

//menu.dat is loaded later... after the video and everything is up.
void M_Init (void)
{

	Cmd_AddCommand("togglemenu", M_ToggleMenu_f);
	Cmd_AddCommand("closemenu", M_CloseMenu_f);
	Cmd_AddCommand("fps_preset", FPS_Preset_f);
	Cmd_AddCommand("menupop", M_MenuPop_f);

	//server browser is kinda complex, and has clipboard integration which we need to sandbox a little
#ifdef CL_MASTER
	Cmd_AddCommand ("menu_servers", M_Menu_ServerList2_f);
#endif
	//downloads menu needs sandboxing, so cannot be provided by qc.
#ifdef WEBCLIENT
	Cmd_AddCommand ("menu_download", Menu_DownloadStuff_f);
#endif
	//demo menu is allowed to see outside of the quakedir. you can't replicate that in qc's sandbox.
	Cmd_AddCommand ("menu_demo", M_Menu_Demos_f);
#ifdef HAVE_JUKEBOX
	Cmd_AddCommand ("menu_mediafiles", M_Menu_MediaFiles_f);
#endif



	Cvar_Register(&m_preset_chosen, "Menu thingumiebobs");
	Cvar_Register(&m_helpismedia, "Menu thingumiebobs");

	Media_Init();
#ifdef CL_MASTER
	M_Serverlist_Init();
#endif
	M_Script_Init();

	M_Reinit();
}
//end builtin-menu code.
#else
void M_Init_Internal (void){}
void M_DeInit_Internal (void){}
void M_Shutdown(qboolean total)
{
#ifdef MENU_DAT
	MP_Shutdown();
#endif
}
void M_Reinit(void)
{
#ifdef MENU_DAT
	if (!MP_Init())
#endif
	{
		(void)CSQC_UnconnectedInit();
	}
}
void M_Init (void)
{
	Media_Init();
	M_Reinit();
}
#endif


void M_Draw (int uimenu)
{
	qboolean stillactive = false;

	if (uimenu)
	{
		if (uimenu == 2)
			R2D_FadeScreen ();
#ifdef VM_UI
		UI_DrawMenu();
#endif
	}

#ifndef NOBUILTINMENUS
	if (!Key_Dest_Has(kdm_emenu))
	{
		M_RemoveAllMenus(false);
		menu_mousedown = false;
		return;
	}
#endif

#ifndef NOBUILTINMENUS
	if ((!menu_script || scr_con_current) && !m_recursiveDraw)
	{
		if (topmenu && topmenu->selecteditem && topmenu->selecteditem->common.type == mt_slider && (topmenu->selecteditem->slider.var == &v_gamma || topmenu->selecteditem->slider.var == &v_contrast))
			/*no menu tint if we're trying to adjust gamma*/;
		else
			R2D_FadeScreen ();
	}
	else
	{
		m_recursiveDraw = false;
	}
#endif

	R2D_ImageColours(1, 1, 1, 1);

#ifdef PLUGINS
	if (menuplug)
	{
		Plug_Menu_Event (0, (int)(realtime*1000));
		stillactive = true;
	}
#endif

#ifndef NOBUILTINMENUS
	{extern menu_t *topmenu;
	if (topmenu)
	{
		M_Complex_Draw ();
		stillactive = true;
	}}
#endif
	if (!stillactive)
		Key_Dest_Remove(kdm_emenu);
}


void M_Keydown (int key, int unicode)
{
#ifndef NOBUILTINMENUS
	if (topmenu)
	{
		if (key == K_MOUSE1)	//mouse clicks are deferred until the release event. this is for touch screens and aiming.
			menu_mousedown = true;
		else if (key == K_LSHIFT || key == K_RSHIFT || key == K_LALT || key == K_RALT || key == K_LCTRL || key == K_RCTRL)
			;	//modifiers are sent on up events instead.
		else
			M_Complex_Key (key, unicode);
		return;
	}
#endif

#ifdef PLUGINS
	if (menuplug)
	{
		Plug_Menu_Event (1, key);
		return;
	}
#endif

	Key_Dest_Remove(kdm_emenu);
}


void M_Keyup (int key, int unicode)
{
#ifndef NOBUILTINMENUS
	if (topmenu)
	{
		if (key == K_MOUSE1 && menu_mousedown)
			M_Complex_Key (key, unicode);
		else if (key == K_LSHIFT || key == K_RSHIFT || key == K_LALT || key == K_RALT || key == K_LCTRL || key == K_RCTRL)
			M_Complex_Key (key, unicode);
		menu_mousedown = false;
		return;
	}
#endif
#ifdef PLUGINS
	if (menuplug)
	{
		Plug_Menu_Event (2, key);
		return;
	}
#endif
}

// Generic function to choose which game menu to draw
int M_GameType (void)
{
	static int cached;
	static unsigned int cachedrestarts;

	if (FS_Restarted(&cachedrestarts))
	{
		struct
		{
			int gametype;
			char *path;
		} configs[] =
		{
			{MGT_QUAKE1, "gfx/sp_menu.lmp"},
#ifdef Q2CLIENT
			{MGT_QUAKE2, "pics/m_banner_game.pcx"},
#endif
#ifdef HEXEN2
			{MGT_HEXEN2, "gfx/menu/title2.lmp"},
#endif
			{0, NULL}
		};
		int bd = COM_FDepthFile(configs[0].path, true);
		int i;
		cached = configs[0].gametype;
		for (i = 1; configs[i].path; i++)
		{
			int gd = COM_FDepthFile(configs[i].path, true);
			if (bd > gd)
			{
				bd = gd;
				cached = configs[i].gametype;
			}
		}
	}

	return cached;
}


