//read menu.h

#include "quakedef.h"
#include "winquake.h"
#include "shader.h"

extern cvar_t maxclients;

menutext_t *MC_AddWhiteText(menu_t *menu, int x, int y, const char *text, qboolean rightalign);

/* MULTIPLAYER MENU */
void M_Menu_MultiPlayer_f (void)
{
	menubutton_t *b;
	menu_t *menu;
	mpic_t *p;
	int mgt;

	p = NULL;
	key_dest = key_menu;
	m_state = m_complex;

	mgt = M_GameType();

	menu = M_CreateMenu(0);

	if (mgt == MGT_QUAKE2)
	{
		MC_AddCenterPicture(menu, 4, 24, "pics/m_banner_multiplayer");

		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommand	(menu, 64, 40,	"Join network server", "menu_slist\n");
		MC_AddConsoleCommand	(menu, 64, 40,	"Quick Connect", "quickconnect qw\n");
		MC_AddConsoleCommand	(menu, 64, 48,	"Start network server", "menu_newmulti\n");
		MC_AddConsoleCommand	(menu, 64, 56,	"Player setup", "menu_setup\n");
		MC_AddConsoleCommand	(menu, 64, 64,	"Demos", "menu_demo\n");

		menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 48, 40, NULL, false);
		return;
	}
	else if (mgt == MGT_HEXEN2)
	{
		MC_AddCenterPicture(menu, 0, 60, "gfx/menu/title4.lmp");

		mgt=64;
		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommandHexen2BigFont	(menu, 80, mgt,	"Server List ",	"menu_slist\n");mgt+=20;
		MC_AddConsoleCommandHexen2BigFont	(menu, 80, mgt,	"New Server  ",	"menu_newmulti\n");mgt+=20;
		MC_AddConsoleCommandHexen2BigFont	(menu, 80, mgt,	"Player Setup",	"menu_setup\n");mgt+=20;
		MC_AddConsoleCommandHexen2BigFont	(menu, 80, mgt,	"Demos       ",	"menu_demo\n");mgt+=20;

		menu->cursoritem = (menuoption_t *)MC_AddCursor(menu, 48, 64);
		return;
	}
	else if (QBigFontWorks())
	{
		MC_AddPicture(menu, 16, 4, 32, 144, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 4, 24, "gfx/p_multi.lmp");

		mgt=32;
		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommandQBigFont	(menu, 72, mgt,	"Server List ",	"menu_slist\n");mgt+=20;
		MC_AddConsoleCommandQBigFont	(menu, 72, mgt,	"Quick Connect", "quickconnect qw\n");mgt+=20;
		MC_AddConsoleCommandQBigFont	(menu, 72, mgt,	"New Server  ",	"menu_newmulti\n");mgt+=20;
		MC_AddConsoleCommandQBigFont	(menu, 72, mgt,	"Player Setup",	"menu_setup\n");mgt+=20;
		MC_AddConsoleCommandQBigFont	(menu, 72, mgt,	"Demos       ",	"menu_demo\n");mgt+=20;

		menu->cursoritem = (menuoption_t*)MC_AddCursor(menu, 54, 32);
		return;
	}
	else
	{
		p = R2D_SafeCachePic("gfx/mp_menu.lmp");
		if (p)
		{
			MC_AddPicture(menu, 16, 4, 32, 144, "gfx/qplaque.lmp");
			MC_AddCenterPicture(menu, 4, 24, "gfx/p_multi.lmp");
			MC_AddPicture(menu, 72, 32, 232, 64, "gfx/mp_menu.lmp");
		}
	}

	b = MC_AddConsoleCommand(menu, 72, 32, "", "menu_slist\n");
	menu->selecteditem = (menuoption_t*)b;
	b->common.height = 20;
	b->common.width = p?p->width:320;
	b = MC_AddConsoleCommand(menu, 72, 52, "", "menu_newmulti\n");
	b->common.height = 20;
	b->common.width = p?p->width:320;
	b = MC_AddConsoleCommand(menu, 72, 72, "", "menu_setup\n");
	b->common.height = 20;
	b->common.width = p?p->width:320;

	b = MC_AddConsoleCommand(menu, 72, 92, "", "menu_demo\n");
	MC_AddWhiteText(menu, 72, 92+20/2-6, "Demos", false);
	b->common.height = 20/2+2;
	b->common.width = p?p->width:320;

	b = MC_AddConsoleCommand(menu, 72, 112, "", "quickconnect qw\n");
	MC_AddWhiteText(menu, 72, 112+20/2-6, "Quick Connect", false);
	b->common.height = 20/2+2;
	b->common.width = p?p->width:320;

	menu->cursoritem = (menuoption_t*)MC_AddCursor(menu, 54, 32);
}

extern cvar_t	team, skin;
extern cvar_t	topcolor;
extern cvar_t	bottomcolor;
extern cvar_t skill;
typedef struct {
	menuedit_t *nameedit;
	menuedit_t *teamedit;
	menuedit_t *skinedit;
	menucombo_t *classedit;
	menucombo_t *modeledit;
	int topcolour;
	int lowercolour;

	int ticlass;
	int tiwidth, tiheight;
	qbyte translationimage[128*128];
} setupmenu_t;
qboolean ApplySetupMenu (union menuoption_s *option,struct menu_s *menu, int key)
{
	setupmenu_t *info = menu->data;
	if (key != K_ENTER)
		return false;
	Cvar_Set(&name, info->nameedit->text);
	Cvar_Set(&team, info->teamedit->text);
	if (info->skinedit)
		Cvar_Set(&skin, info->skinedit->text);
	if (info->classedit)
		Cvar_SetValue(Cvar_FindVar("cl_playerclass"), info->classedit->selectedoption+1);
	Cbuf_AddText(va("color %i %i\n", info->lowercolour, info->topcolour), RESTRICT_LOCAL);
	S_LocalSound ("misc/menu2.wav");
	M_RemoveMenu(menu);
	return true;
}
qboolean SetupMenuColour (union menuoption_s *option,struct menu_s *menu, int key)
{
	setupmenu_t *info = menu->data;
	if (*option->button.text == 'T')
	{
		if (key == K_ENTER || key == K_RIGHTARROW)
		{
			info->topcolour ++;
			if (info->topcolour>=14)
				info->topcolour=0;
			S_LocalSound ("misc/menu2.wav");
			return true;
		}
		if (key == K_LEFTARROW)
		{
			info->topcolour --;
			if (info->topcolour<=0)
				info->topcolour=13;
			S_LocalSound ("misc/menu2.wav");
			return true;
		}

	}
	else
	{
		if (key == K_ENTER || key == K_RIGHTARROW)
		{
			info->lowercolour ++;
			if (info->lowercolour>=14)
				info->lowercolour=0;
			S_LocalSound ("misc/menu2.wav");
			return true;
		}
		if (key == K_LEFTARROW)
		{
			info->lowercolour --;
			if (info->lowercolour<=0)
				info->lowercolour=13;
			S_LocalSound ("misc/menu2.wav");
			return true;
		}
	}
	return false;
}


typedef struct {
	char **names;
	int entries;
	int match;
} q2skinsearch_t;

int q2skin_enumerate(const char *name, int fsize, void *parm)
{
	char blah[MAX_QPATH];
	q2skinsearch_t *s = parm;

	COM_StripExtension(name+8, blah, sizeof(blah));
	if (strlen(blah) < 2)
		return false;	//this should never happen
	blah[strlen(blah)-2] = 0;

	s->names = BZ_Realloc(s->names, ((s->entries+64)&~63) * sizeof(char*));
	s->names[s->entries] = BZ_Malloc(strlen(blah)+1);
	strcpy(s->names[s->entries], blah);

	if (!strcmp(blah, skin.string))
		s->match = s->entries;

	s->entries++;
	return true;
}
void q2skin_destroy(q2skinsearch_t *s)
{
	int i;
	for (i = 0; i < s->entries; i++)
	{
		BZ_Free(s->names[i]);
	}
	BZ_Free(s);
}

qboolean MSetupQ2_ChangeSkin (struct menucustom_s *option,struct menu_s *menu, int key)
{
	setupmenu_t *info = menu->data;
	q2skinsearch_t *s = Z_Malloc(sizeof(*s));
	COM_EnumerateFiles(va("players/%s/*_i.*", info->modeledit->values[info->modeledit->selectedoption]), q2skin_enumerate, s);
	if (key == K_ENTER || key == K_RIGHTARROW)
	{
		s->match ++;
		if (s->match>=s->entries)
			s->match=0;
	}
	else if (key == K_LEFTARROW)
	{
		s->match --;
		if (s->match<=0)
			s->match=s->entries-1;
	}
	else
	{
		q2skin_destroy(s);
		return false;
	}
	if (s->entries)
		Cvar_Set(&skin, s->names[s->match]);
	S_LocalSound ("misc/menu2.wav");
	q2skin_destroy(s);
	return true;
}
void MSetupQ2_TransDraw (int x, int y, menucustom_t *option, menu_t *menu)
{
	setupmenu_t *info = menu->data;
	mpic_t	*p;


	p = R2D_SafeCachePic (va("players/%s_i", skin.string));
	if (!p)
	{
		q2skinsearch_t *s = Z_Malloc(sizeof(*s));
		COM_EnumerateFiles(va("players/%s/*_i.*", info->modeledit->values[info->modeledit->selectedoption]), q2skin_enumerate, s);
		if (s->entries)
			Cvar_Set(&skin, s->names[rand()%s->entries]);
		q2skin_destroy(s);

		p = R2D_SafeCachePic (va("players/%s_i", skin.string));
	}
	if (p)
		R2D_ScalePic (x-12, y-8, p->width, p->height, p);
}

void MSetup_TransDraw (int x, int y, menucustom_t *option, menu_t *menu)
{
	qbyte translationTable[256];
	setupmenu_t *info = menu->data;
	mpic_t	*p;
	void *f;
	qboolean reloadtimage = false;

	if (info->skinedit && info->skinedit->modified)
	{
		info->skinedit->modified = false;
		reloadtimage = true;
	}
	if (info->classedit && info->classedit->selectedoption != info->ticlass)
	{
		info->ticlass = info->classedit->selectedoption;
		reloadtimage = true;
	}

	if (reloadtimage)
	{
		if (info->classedit)	//quake2 main menu.
		{
			FS_LoadFile(va("gfx/menu/netp%i.lmp", info->ticlass+1), &f);
		}
		else
		{
			FS_LoadFile(va("gfx/player/%s.lmp", info->skinedit->text), &f);
			if (!f)
				FS_LoadFile("gfx/menuplyr.lmp", &f);
		}

		if (f)
		{
			info->tiwidth = ((int*)f)[0];
			info->tiheight = ((int*)f)[1];
			if (info->tiwidth * info->tiheight > sizeof(info->translationimage))
				info->tiwidth = info->tiheight = 0;
			memcpy(info->translationimage, (char*)f+8, info->tiwidth*info->tiheight);
			FS_FreeFile(f);
		}
	}

	p = R2D_SafeCachePic ("gfx/bigbox.lmp");
	if (p)
		R2D_ScalePic (x-12, y-8, 72, 72, p);

	M_BuildTranslationTable(info->topcolour, info->lowercolour, translationTable);
	R2D_TransPicTranslate (x, y, info->tiwidth, info->tiheight, info->translationimage, translationTable);
}

void M_Menu_Setup_f (void)
{
	int mgt;
	setupmenu_t *info;
	menu_t *menu;
	menucustom_t *ci;
	menubutton_t *b;
	static const char *classnames[] =
	{
		"Paladin",
		"Crusader",
		"Necromancer",
		"Assasin",
		"Demoness",
		NULL
	};

	mgt = M_GameType();
	if (mgt == MGT_QUAKE2)	//quake2 main menu.
	{
		if (R2D_SafeCachePic("pics/m_banner_plauer_setup"))
		{
			static const char *modeloptions[] =
			{
				"male",
				"female",
				NULL
			};
			mpic_t *p;
			menucustom_t *cu;
			m_state = m_complex;
			key_dest = key_menu;

			menu = M_CreateMenu(sizeof(setupmenu_t));
			info = menu->data;
//			menu->key = MC_Main_Key;

			MC_AddPicture(menu, 0, 4, 38, 166, "pics/m_main_plaque");
			p = R2D_SafeCachePic("pics/m_main_logo");
			if (!p)
				return;
			MC_AddPicture(menu, 0, 173, 36, 42, "pics/m_main_logo");

			menu->selecteditem = (menuoption_t*)
			(info->nameedit = MC_AddEdit(menu, 64, 40, "Your name", name.string));
			(info->modeledit = MC_AddCvarCombo(menu, 64, 72, "model", &skin, (const char **)modeloptions, (const char **)modeloptions));
			info->modeledit->selectedoption = !strncmp(skin.string, "female", 6);
			cu = MC_AddCustom(menu, 172-16, 88+16, NULL);
			cu->draw = MSetupQ2_TransDraw;
			cu->key = MSetupQ2_ChangeSkin;

/*			MC_AddSelectablePicture(mainm, 68, 13, "pics/m_main_game");
			MC_AddSelectablePicture(mainm, 68, 53, "pics/m_main_multiplayer");
			MC_AddSelectablePicture(mainm, 68, 93, "pics/m_main_options");
			MC_AddSelectablePicture(mainm, 68, 133, "pics/m_main_video");
			MC_AddSelectablePicture(mainm, 68, 173, "pics/m_main_quit");

			b = MC_AddConsoleCommand	(mainm, 68, 13,	"", "menu_single\n");
			mainm->selecteditem = (menuoption_t *)b;
			b->common.width = 12*20;
			b->common.height = 20;
			b = MC_AddConsoleCommand	(mainm, 68, 53,	"", "menu_multi\n");
			b->common.width = 12*20;
			b->common.height = 20;
			b = MC_AddConsoleCommand	(mainm, 68, 93,	"", "menu_options\n");
			b->common.width = 12*20;
			b->common.height = 20;
			b = MC_AddConsoleCommand	(mainm, 68, 133,	"", "menu_video\n");
			b->common.width = 12*20;
			b->common.height = 20;
			b = MC_AddConsoleCommand	(mainm, 68, 173,	"", "menu_quit\n");
			b->common.width = 12*20;
			b->common.height = 20;
*/
			menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 54, 32, NULL, false);
		}
		return;
	}

	key_dest = key_menu;
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(setupmenu_t));
	info = menu->data;

	MC_AddPicture(menu, 16, 4, 32, 144, "gfx/qplaque.lmp");
	MC_AddCenterPicture(menu, 4, 24, "gfx/p_multi.lmp");


//	MC_AddPicture(menu, 72, 32, Draw_CachePic ("gfx/mp_menu.lmp") );

	menu->selecteditem = (menuoption_t*)
	(info->nameedit = MC_AddEdit(menu, 64, 40, "Your name", name.string));
	(info->teamedit = MC_AddEdit(menu, 64, 56, "Your team", team.string));
	if (mgt == MGT_HEXEN2)
	{
		cvar_t *pc = Cvar_Get("cl_playerclass", "1", CVAR_USERINFO|CVAR_ARCHIVE, "Hexen2");
		(info->classedit = MC_AddCombo(menu, 64, 72, "Your class", (const char **)classnames, pc->ival-1));
	}
	else
		(info->skinedit = MC_AddEdit(menu, 64, 72, "Your skin", skin.string));

	ci = MC_AddCustom(menu, 172+32, 88, NULL);
	ci->draw = MSetup_TransDraw;
	ci->key = NULL;

	MC_AddCommand(menu, 64, 96, "Top colour", SetupMenuColour);
	MC_AddCommand(menu, 64, 120, "Lower colour", SetupMenuColour);

	MC_AddCommand(menu, 64, 152, "Accept changes", ApplySetupMenu);
	b = MC_AddConsoleCommand(menu, 64, 168, "Network Settings", "menu_network\n");
	b->common.tooltip = "Change network and client prediction settings.";
	b = MC_AddConsoleCommand(menu, 64, 176, "Teamplay Settings", "menu_teamplay\n");
	b->common.tooltip = "Change teamplay macro settings.";
	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 54, 32, NULL, false);


	info->lowercolour = bottomcolor.value;
	info->topcolour = topcolor.value;
	if (info->skinedit)
		info->skinedit->modified = true;
	info->ticlass = -1;
}




#ifdef CLIENTONLY
void M_Menu_GameOptions_f (void)
{
}
#else

typedef struct {
	menuedit_t *hostnameedit;
	menucombo_t *deathmatch;
	menucombo_t *numplayers;
	menucombo_t *teamplay;
	menucombo_t *skill;
	menucombo_t *timelimit;
	menucombo_t *fraglimit;
	menuedit_t *mapnameedit;
	menucheck_t *rundedicated;

	int topcolour;
	int lowercolour;
} newmultimenu_t;

static const char *numplayeroptions[] = {
	"2",
	"3",
	"4",
	"8",
	"12",
	"16",
	"20",
	"24",
	"32",
	NULL
};

qboolean MultiBeginGame (union menuoption_s *option,struct menu_s *menu, int key)
{
	newmultimenu_t *info = menu->data;
	if (key != K_ENTER)
		return false;

	if (cls.state)
		Cbuf_AddText("disconnect\n", RESTRICT_LOCAL);

	Cbuf_AddText(va("maxclients \"%s\"\n", numplayeroptions[info->numplayers->selectedoption]), RESTRICT_LOCAL);
	if (info->rundedicated->value)
		Cbuf_AddText("setrenderer dedicated\n", RESTRICT_LOCAL);
	Cbuf_AddText(va("hostname \"%s\"\n", info->hostnameedit->text), RESTRICT_LOCAL);
	Cbuf_AddText(va("deathmatch %i\n", info->deathmatch->selectedoption), RESTRICT_LOCAL);
	if (!info->deathmatch->selectedoption)
		Cbuf_AddText("coop 1\n", RESTRICT_LOCAL);
	else
		Cbuf_AddText("coop 0\n", RESTRICT_LOCAL);
	Cbuf_AddText(va("teamplay %i\n", info->teamplay->selectedoption), RESTRICT_LOCAL);
	Cbuf_AddText(va("skill %i\n", info->skill->selectedoption), RESTRICT_LOCAL);
	Cbuf_AddText(va("timelimit %i\n", info->timelimit->selectedoption*5), RESTRICT_LOCAL);
	Cbuf_AddText(va("fraglimit %i\n", info->fraglimit->selectedoption*10), RESTRICT_LOCAL);
	Cbuf_AddText(va("map \"%s\"\n", info->mapnameedit->text), RESTRICT_LOCAL);

	if (info->rundedicated->value)
	{
		Cbuf_AddText("echo You can use the setrenderer command to return to a graphical interface at any time\n", RESTRICT_LOCAL);
	}

	M_RemoveAllMenus();

	return true;
}
void M_Menu_GameOptions_f (void)
{
	extern cvar_t pr_maxedicts;
	static const char *deathmatchoptions[] = {
		"Cooperative",
		"Deathmatch 1",
		"Deathmatch 2",
		"Deathmatch 3",
		"Deathmatch 4",
		"Deathmatch 5",
		NULL
	};
	static const char *teamplayoptions[] = {
		"off",
		"friendly fire",
		"no friendly fire",
		NULL
	};
	static const char *skilloptions[] = {
		"Easy",
		"Medium",
		"Hard",
		"NIGHTMARE",
		NULL
	};
	static const char *timelimitoptions[] = {
		"no limit",
		"5 minutes",
		"10 minutes",
		"15 minutes",
		"20 minutes",
		"25 minutes",
		"30 minutes",
		"35 minutes",
		"40 minutes",
		"45 minutes",
		"50 minutes",
		"55 minutes",
		"1 hour",
		NULL
	};
	static const char *fraglimitoptions[] = {
		"no limit",
		"10 frags",
		"20 frags",
		"30 frags",
		"40 frags",
		"50 frags",
		"60 frags",
		"70 frags",
		"80 frags",
		"90 frags",
		"100 frags",
		NULL
	};
	newmultimenu_t *info;
	menu_t *menu;
	int y = 40;
	int mgt;
	int players;

	key_dest = key_menu;
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(newmultimenu_t));
	info = menu->data;

	mgt = M_GameType();

	if (mgt == MGT_QUAKE2)
	{
		MC_AddCenterPicture(menu, 4, 24, "pics/m_banner_start_server");
		y += 8;
	}
	else if (mgt == MGT_HEXEN2)
	{
	}
	else
	{
		MC_AddPicture(menu, 16, 4, 32, 144, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 4, 24, "gfx/p_multi.lmp");
	}

//	MC_AddPicture(menu, 72, 32, ("gfx/mp_menu.lmp") );

	menu->selecteditem = (menuoption_t*)
	MC_AddCommand						(menu, 64, y,	" Start game", MultiBeginGame);y+=16;

	info->hostnameedit	= MC_AddEdit	(menu, 64, y,	"   Hostname", name.string);y+=16;

	for (players = 0; players < sizeof(numplayeroptions)/ sizeof(numplayeroptions[0]); players++)
	{
		if (atoi(numplayeroptions[players]) >= maxclients.value)
			break;
	}

	info->numplayers	= MC_AddCombo	(menu, 64, y,			"Max players", (const char **)numplayeroptions,	players);y+=8;

	info->deathmatch	= MC_AddCombo	(menu, 64, y,			" Deathmatch", (const char **)deathmatchoptions,	deathmatch.value);y+=8;
	info->teamplay		= MC_AddCombo	(menu, 64, y,			"   Teamplay", (const char **)teamplayoptions,		teamplay.value);y+=8;
	info->skill			= MC_AddCombo	(menu, 64, y,			"      Skill", (const char **)skilloptions,			skill.value);y+=8;
	info->rundedicated	= MC_AddCheckBox(menu, 64, y,			"  dedicated", NULL, 0);y+=8;
	y+=8;
	info->timelimit		= MC_AddCombo	(menu, 64, y,			" Time Limit", (const char **)timelimitoptions,		timelimit.value/5);y+=8;
	info->fraglimit		= MC_AddCombo	(menu, 64, y,			" Frag Limit", (const char **)fraglimitoptions,		fraglimit.value/10);y+=8;
	y+=8;
	MC_AddSlider	(menu, 64-7*8, y,					"Extra edict support", &pr_maxedicts, 512, 2047, 256);y+=8;
	y+=8;
	if (mgt == MGT_QUAKE2)
		info->mapnameedit	= MC_AddEdit	(menu, 64, y,			"        map", "base1");
	else
		info->mapnameedit	= MC_AddEdit	(menu, 64, y,			"        map", "start");
	y += 16;

	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 54, 32, NULL, false);


	info->lowercolour = bottomcolor.value;
	info->topcolour = topcolor.value;
}
#endif

void M_Menu_Teamplay_f (void)
{
	static const char *noskinsoptions[] =
	{
		"Enabled",
		"Disabled",
		"No Download",
		NULL
	};
	static const char *noskinsvalues[] =
	{
		"0",
		"1",
		"2",
		NULL
	};

	extern cvar_t cl_parseSay, cl_triggers, tp_forceTriggers, tp_loadlocs, cl_parseFunChars, cl_noblink, noskins;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Options", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_COMBOCVAR("Skins", noskins, noskinsoptions, noskinsvalues, "Enable or disable player skin usage. No download will use skins but will not download them from the server."),
		MB_EDITCVARTIP("Enemy Skin", "cl_enemyskin", "Override enemy skin with this."),
		MB_EDITCVARTIP("Team Skin", "cl_teamskin", "Override teammate skin with this."),
		MB_EDITCVARTIP("Fake Name", "cl_fakename", "Name that appears in teamplay messages"),
		MB_CHECKBOXCVARTIP("Parse Fun Chars", cl_parseFunChars, 0, "Whether to parse fun characters"),
		MB_CHECKBOXCVARTIP("Parse Macros", cl_parseSay, 0, "Whether to parse teamplay macros like %l etc."),
		MB_CHECKBOXCVARTIP("Load Locs", tp_loadlocs, 0, "Whether to load teamplay locations from .loc files"),
		MB_CHECKBOXCVARTIP("No Blink", cl_noblink, 0, "No blinking characters"),
		MB_EDITCVARTIP("Sound Trigger", "tp_soundtrigger", "Character that indicates the following text is a wav file.\nExample:\nsay_team ~location.wav$\\me: I'm at %l #a"),
		MB_EDITCVARTIP("Weapon Order", "tp_weapon_order","Weapon preference order:\n8 = Lightning Gun\n7 = Rocket Launcher\n6 = Grenade Launcher\n5 = Super Nailgun\n4 = Nailgun\n3 = Super Shotgun\n2 = Shotgun\n1 = Axe"),
		MB_CHECKBOXCVARTIP("Teamplay Triggers", cl_triggers, 0, "Enable or disable teamplay triggers"),
		MB_CHECKBOXCVARTIP("Force Triggers", tp_forceTriggers, 0, "Whether to force teamplay triggers in non-teamplay play like in a 1 on 1 situation"),
		MB_SPACING(4),
		MB_CONSOLECMD("Location Names", "menu_teamplay_locations\n", "Modify team play location settings."),
		MB_CONSOLECMD("Item Needs", "menu_teamplay_needs\n", "Modify messages for item needs in team play macros."),
		MB_CONSOLECMD("Item Names", "menu_teamplay_items\n", "Modify messages for items in team play macros."),
		MB_END()
	};
	menu_t *menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Teamplay_Locations_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Location Names", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Separator", "loc_name_separator", "Location name seperator character(s)"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Super Shotgun", "loc_name_ssg", "Short name for Super Shotgun in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Nailgun", "loc_name_ng", "Short name for Nailgun in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Super Nailgun", "loc_name_sng", "Short name for Super Nailgun in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Grenade Launcher", "loc_name_gl", "Short name for Grenade Launcher in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Rocket Launcher", "loc_name_rl", "Short name for Rocket Launcher in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Lightning Gun", "loc_name_lg", "Short name for Lightning Gun in teamplay location 'reports'"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Quad Damage", "loc_name_quad", "Short name for Quad Damage in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Pentagram", "loc_name_pent", "Short name for Pentagram of Protection in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Ring of Invis", "loc_name_ring", "Short name for Ring of Invisibility in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Suit", "loc_name_suit", "Short name for Environment Suit in teamplay location 'reports'"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Green Armor", "loc_name_ga", "Short name for Green Armor in teamplay location 'reports'"),
		MB_EDITCVARSLIM("Yellow Armor", "loc_name_ya", "Short name for Yellow Armor in teamplay location 'reports'" ),
		MB_EDITCVARSLIM("Red Armor", "loc_name_ra", "Short name for Red Armor in teamplay location 'reports'"),
		// TODO: we probably need an actual back button or some such
		//MB_SPACING(4),
		//MB_CONSOLECMD("\x7f Teamplay", "menu_teamplay\n", "Return to the teamplay menu."),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Teamplay_Needs_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Needed Items", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Shells", "tp_need_shells", "Short name for Shotgun Shells in teamplay 'need' reports"),
		MB_EDITCVARSLIM("Nails", "tp_need_nails", "Short name for Nails in teamplay 'need' reports"),
		MB_EDITCVARSLIM("Rockets", "tp_need_rockets", "Short name for Rockets/Grenades in teamplay 'need' reports"),
		MB_EDITCVARSLIM("Cells", "tp_need_cells", "Short name for Power Cells in teamplay 'need' reports"),
		MB_EDITCVARSLIM("Rocket Launcher", "tp_need_rl", "Short name for Rocket Launcher in teamplay 'need' reports"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Green Armor", "tp_need_ga", "Short name for Green Armor in teamplay 'need' reports"),
		MB_EDITCVARSLIM("Yellow Armor", "tp_need_ya", "Short name for Yellow Armor in teamplay 'need' reports"),
		MB_EDITCVARSLIM("Red Armor", "tp_need_ra", "Short name for Red Armor in teamplay 'need' reports"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Health", "tp_need_health", "Short name for Health in teamplay 'need' reports"),
		MB_EDITCVARSLIM("Weapon", "tp_need_weapon", "Need weapon preference order:\n8 = Lightning Gun\n7 = Rocket Launcher\n6 = Grenade Launcher\n5 = Super Nailgun\n4 = Nailgun\n3 = Super Shotgun\n2 = Shotgun\n1 = Axe"),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Teamplay_Items_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Item Names", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_CONSOLECMD("Armor", "menu_teamplay_armor\n", "Modify team play macro armor names."),
		MB_CONSOLECMD("Weapon", "menu_teamplay_weapons\n", "Modify team play macro weapon names."),
		MB_CONSOLECMD("Powerups", "menu_teamplay_powerups\n", "Modify team play macro powerup names."),
		MB_CONSOLECMD("Ammo/Health", "menu_teamplay_ammo_health\n", "Modify team play macro ammo and health names."),
		MB_CONSOLECMD("Team Fortress", "menu_teamplay_team_fortress\n", "Modify Team Fortress exclusive team play macro names."),
		MB_CONSOLECMD("Status/Location/Misc", "menu_teamplay_status_location_misc\n", "Modify status, location, and miscellaneous team play macro names."),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 224, y);
}

void M_Menu_Teamplay_Items_Armor_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Armor Names", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Armor", "tp_name_armor", "Short name for Armor type"),
		MB_EDITCVARSLIM("Green Type -", "tp_name_armortype_ga", "Short name for Green Armor type"),
		MB_EDITCVARSLIM("Yellow Type -", "tp_name_armortype_ya", "Short name for Yellow Armor type"),
		MB_EDITCVARSLIM("Red Type -", "tp_name_armortype_ra", "Short name for Red Armor type"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Green Armor", "tp_name_ga", "Short name for Green Armor"),
		MB_EDITCVARSLIM("Yellow Armor", "tp_name_ya", "Short name for Yellow Armor"),
		MB_EDITCVARSLIM("Red Armor", "tp_name_ra", "Short name for Red Armor"),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Teamplay_Items_Weapons_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Weapon Names", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Weapon", "tp_name_weapon", "Short name for Weapon"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Axe", "tp_name_axe", "Short name for Weapon"),
		MB_EDITCVARSLIM("Shotgun", "tp_name_sg", "Short name for Shotgun"),
		MB_EDITCVARSLIM("Super Shotgun", "tp_name_ssg", "Short name for Super Shotgun"),
		MB_EDITCVARSLIM("Nailgun", "tp_name_ng", "Short name for Nailgun"),
		MB_EDITCVARSLIM("Super Nailgun", "tp_name_sng", "Short name for Super Nailgun"),
		MB_EDITCVARSLIM("Grenade Launcher", "tp_name_gl", "Short name for Grenade Launcher"),
		MB_EDITCVARSLIM("Rocket Launcher", "tp_name_rl", "Short name for Rocket Launcher"),
		MB_EDITCVARSLIM("Lightning Gun", "tp_name_lg", "Short name for Lightning Gun"),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Teamplay_Items_Powerups_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Powerup Names", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Quad Damage", "tp_name_quad", "Short name for Quad Damage"),
		MB_EDITCVARSLIM("Pentagram", "tp_name_pent", "Short name for Pentgram of Protection"),
		MB_EDITCVARSLIM("Ring of Invis", "tp_name_ring", "Short name for Ring Of Invisibilty"),
		MB_EDITCVARSLIM("Suit", "tp_name_suit", "Short name for Environment Suit"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Quaded", "tp_name_quaded", "Short name for reporting being 'Quaded'. Dying by another player who has Quad Damage"),
		MB_EDITCVARSLIM("Pented", "tp_name_pented", "Short name for reporting being 'Pented'. Dying by another player who has the Pentagram"),
		MB_EDITCVARSLIM("Eyes (Ringed)", "tp_name_eyes", "Short name for reporting being 'Ringed', Dying by another player who has Eyes (Invisibility)"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Resistance Rune", "tp_name_rune_1", "Short name for Resistance Rune"),
		MB_EDITCVARSLIM("Strength Rune", "tp_name_rune_2", "Short name for Strength Rune"),
		MB_EDITCVARSLIM("Haste Rune", "tp_name_rune_3", "Short name for Haste Rune"),
		MB_EDITCVARSLIM("Regen Rune", "tp_name_rune_4", "Short name for Regeneration Rune"),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Teamplay_Items_Ammo_Health_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Ammo/Health", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Shells", "tp_name_shells", "Short name for Shells"),
		MB_EDITCVARSLIM("Nails", "tp_name_nails", "Short name for Nails"),
		MB_EDITCVARSLIM("Rockets", "tp_name_rockets", "Short name for Rockets"),
		MB_EDITCVARSLIM("Cells", "tp_name_cells", "Short name for Cells"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Backpack", "tp_name_backpack", "Short name for Backpack"),
		MB_EDITCVARSLIM("Health", "tp_name_health", "Short name for Health"),
		MB_EDITCVARSLIM("Mega Health", "tp_name_mh", "Short name for Mega Health"),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Teamplay_Items_Team_Fortress_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Team Fortress", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Sentry Gun", "tp_name_sentry", "Short name for the Engineer's Sentry Gun"),
		MB_EDITCVARSLIM("Dispenser", "tp_name_disp", "Short name for the Engineer's Ammo Dispenser"),
		MB_EDITCVARSLIM("Flag", "tp_name_flag", "Short name for Flag"),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Teamplay_Items_Status_Location_Misc_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Teamplay Misc", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Enemy", "tp_name_enemy", "Short for Enemy in teamplay 'status' & 'location' reports"),
		MB_EDITCVARSLIM("Teammate", "tp_name_teammate", "Short for Enemy in teamplay 'status' & 'location' reports"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("At (Location)", "tp_name_at", "Short for @ (Location) in teamplay 'status' & 'location' reports"),
		MB_EDITCVARSLIM("None", "tp_name_none", "Short for None in teamplay 'status' & 'location' reports"),
		MB_EDITCVARSLIM("Nothing", "tp_name_nothing", "Short for Nothing in teamplay 'status' & 'location' reports"),
		MB_EDITCVARSLIM("Separator", "tp_name_separator", "Seperator character(s) in teamplay 'status' & 'location' reports"),
		MB_EDITCVARSLIM("Some place", "tp_name_someplace", "Short for Someplace in teamplay 'status' & 'location' reports"),
		MB_SPACING(4),
		MB_EDITCVARSLIM("Red Status", "tp_name_status_red", "Macro for Status Red in teamplay 'status' & 'location' reports"),
		MB_EDITCVARSLIM("Green Status", "tp_name_status_green", "Macro for Status Green in teamplay 'status' & 'location' reports"),
		MB_EDITCVARSLIM("Blue Status", "tp_name_status_blue", "Macro for Status Blue in teamplay 'status' & 'location' reports"),
		MB_EDITCVARSLIM("Yellow Status", "tp_name_status_yellow", "Macro for Status Yellow in teamplay 'status' & 'location' reports"),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}

void M_Menu_Network_f (void)
{
	static const char *splitopts[] = {
		"Disabled",
		"2 Screens",
		"3 Screens",
		"4 Screens",
		NULL
	};
	static const char *splitvalues[] = {"0", "1", "2", "3", NULL};
	extern cvar_t cl_download_csprogs, cl_download_redirection, requiredownloads, cl_solid_players;
	extern cvar_t cl_splitscreen, cl_predict_players;
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Network Settings", false),
		MB_TEXT("\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false),
		MB_EDITCVARSLIM("Network FPS", "cl_netfps", "Sets ammount of FPS used to communicate with server (sent and received)"),
		MB_EDITCVARSLIM("Rate", "rate", "Maximum bytes per second that the server should send to the client"),
		MB_EDITCVARSLIM("Download Rate", "drate", "Maximum bytes per second that the server should send maps and demos to the client"),
		MB_SPACING(4),
		MB_CHECKBOXCVARTIP("Require Download", requiredownloads, 0, "Ignore downloaded content sent to the client and connect immediatly"),
		MB_CHECKBOXCVARTIP("Redirect Download", cl_download_redirection, 0, "Whether the client will ignore download redirection from servers"),
		MB_CHECKBOXCVARTIP("Download CSQC", cl_download_csprogs, 0, "Whether to allow the client to download CSQC (client-side QuakeC) progs from servers"),
		MB_SPACING(4),
		MB_CHECKBOXCVARTIP("Predict Other Players", cl_predict_players, 0, "Toggle player prediction"),
		MB_CHECKBOXCVARTIP("Solid Players", cl_solid_players, 0, "When running/clipping into other players, ON make it appear they are solid, OFF will make it appear like running into a marshmellon."),
		MB_COMBOCVAR("Split-screen", cl_splitscreen, splitopts, splitvalues, "Enables split screen with a number of clients. This feature requires server support."),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, bulk, 16, 200, y);
}