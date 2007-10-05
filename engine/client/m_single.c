//read menu.h

#include "quakedef.h"
#include "winquake.h"
#ifndef CLIENTONLY
//=============================================================================
/* LOAD/SAVE MENU */

#define FTESAVEGAME_VERSION 25000

typedef struct {
	int issave;
	int cursorpos;
	menutext_t *cursoritem;
} loadsavemenuinfo_t;

#define	MAX_SAVEGAMES		20
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int		loadable[MAX_SAVEGAMES];

menubutton_t *VARGS MC_AddConsoleCommandf(menu_t *menu, int x, int y, char *text, char *command, ...);

void M_ScanSaves (void)
{
	int		i, j;
	char	line[MAX_OSPATH];
	vfsfile_t	*f;
	int		version;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;

		snprintf (line, sizeof(line), "saves/s%i/info.fsv", i);
		f = FS_OpenVFS (line, "rb", FS_GAME);
		if (f)
		{
			VFS_GETS(f, line, sizeof(line));
			version = atoi(line);
			if (version < FTESAVEGAME_VERSION || version >= FTESAVEGAME_VERSION+GT_MAX)
			{
				Q_strncpyz (m_filenames[i], "Incompatible version", sizeof(m_filenames[i]));			
				VFS_CLOSE (f);
				continue;
			}

			VFS_GETS(f, line, sizeof(line));
			Q_strncpyz (m_filenames[i], line, sizeof(m_filenames[i]));


		// change _ back to space
			for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
				if (m_filenames[i][j] == '_')
					m_filenames[i][j] = ' ';
			loadable[i] = true;
			VFS_CLOSE (f);

			continue;
		}
	}
}

void M_Menu_Save_f (void)
{
	menuoption_t *op = NULL;
	menu_t *menu;
	int		i;

	if (!sv.state)
		return;

	if (cl.intermission)
		return;

	key_dest = key_menu;
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(loadsavemenuinfo_t));
	menu->data = menu+1;
	
	MC_AddCenterPicture (menu, 4, "gfx/p_save.lmp");	
	menu->cursoritem = (menuoption_t *)MC_AddRedText(menu, 8, 32, NULL, false);	

	M_ScanSaves ();

	for (i=0 ; i< MAX_SAVEGAMES; i++)
	{
		op = (menuoption_t *)MC_AddConsoleCommandf(menu, 16, 32+8*i, m_filenames[i], "savegame s%i\ntogglemenu\n", i);
		if (!menu->selecteditem)
			menu->selecteditem = op;
	}
}
void M_Menu_Load_f (void)
{
	menuoption_t *op = NULL;
	menu_t *menu;
	int		i;

	key_dest = key_menu;
	m_state = m_complex;
	
	menu = M_CreateMenu(sizeof(loadsavemenuinfo_t));
	menu->data = menu+1;
	
	MC_AddCenterPicture(menu, 4, "gfx/p_load.lmp");	
	menu->cursoritem = (menuoption_t *)MC_AddRedText(menu, 8, 32, NULL, false);	

	M_ScanSaves ();

	for (i=0 ; i< MAX_SAVEGAMES; i++)
	{
		if (loadable[i])
			op = (menuoption_t *)MC_AddConsoleCommandf(menu, 16, 32+8*i, m_filenames[i], "loadgame s%i\ntogglemenu\n", i);
		else
			MC_AddWhiteText(menu, 16, 32+8*i, m_filenames[i], false);
		if (!menu->selecteditem && op)
			menu->selecteditem = op;
	}
}


#endif

void M_Menu_SinglePlayer_f (void)
{
	int mgt;
#ifndef CLIENTONLY
	menubutton_t *b;
#endif
	menu_t *menu;
	mpic_t *p;

	key_dest = key_menu;
	m_state = m_complex;

	mgt = M_GameType();
	if (mgt == MGT_QUAKE2)
	{	//q2...
		menu = M_CreateMenu(0);

		MC_AddCenterPicture(menu, 4, "pics/m_banner_game");

		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommand	(menu, 64, 40,	"Easy",		"skill 0;deathmatch 0; coop 0;newgame\n");
		MC_AddConsoleCommand	(menu, 64, 48,	"Medium",	"skill 1;deathmatch 0; coop 0;newgame\n");
		MC_AddConsoleCommand	(menu, 64, 56,	"Hard",		"skill 2;deathmatch 0; coop 0;newgame\n");

		MC_AddConsoleCommand	(menu, 64, 72,	"Load Game", "menu_load\n");
		MC_AddConsoleCommand	(menu, 64, 80,	"Save Game", "menu_save\n");

		menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 48, 40, NULL, false);
		return;
	}
	else if (mgt == MGT_HEXEN2)
	{	//h2
		cvar_t *pc;
		static char *classlist[] = {
			"Random",
			"Barbarian",
			"Crusader",
			"Paladin",
			"Assasin",
			NULL
		};
		static char *classvalues[] = {
			"",
			"1",
			"2",
			"3",
			"4",
			NULL
		};
		menu = M_CreateMenu(0);
		MC_AddPicture(menu, 16, 0, "gfx/menu/hplaque.lmp");
		MC_AddCenterPicture(menu, 0, "gfx/menu/title1.lmp");

		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommand	(menu, 64, 64,	"Easy",		"togglemenu\nskill 0;deathmatch 0; coop 0;map demo1\n");
		MC_AddConsoleCommand	(menu, 64, 72,	"Medium",	"togglemenu\nskill 1;deathmatch 0; coop 0;map demo1\n");
		MC_AddConsoleCommand	(menu, 64, 80,	"Hard",		"togglemenu\nskill 2;deathmatch 0; coop 0;map demo1\n");

		MC_AddConsoleCommand	(menu, 64, 96,	"Load Game", "menu_load\n");
		MC_AddConsoleCommand	(menu, 64, 104,	"Save Game", "menu_save\n");

		pc = Cvar_Get("cl_playerclass", "1", CVAR_USERINFO|CVAR_ARCHIVE, "Hexen2");
		if (pc)
			MC_AddCvarCombo (menu, 64, 104+16,	"Player class", pc, (const char **)classlist, (const char **)classvalues);

		return;
	}
	else if (QBigFontWorks())
	{
		menu = M_CreateMenu(0);
		MC_AddPicture(menu, 16, 0, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 0, "gfx/p_option.lmp");

		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommandQBigFont	(menu, 72, 32,	"New Game",		"togglemenu\nmaxclients 1;deathmatch 0;coop 0;map start\n");
		MC_AddConsoleCommandQBigFont	(menu, 72, 52,	"Load Game", "menu_load\n");
		MC_AddConsoleCommandQBigFont	(menu, 72, 72,	"Save Game", "menu_save\n");

		menu->cursoritem = (menuoption_t*)MC_AddCursor(menu, 54, 32);
		return;
	}
	else
	{	//q1
		menu = M_CreateMenu(0);
		MC_AddPicture(menu, 16, 4, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 4, "gfx/p_option.lmp");
	}

	p = Draw_SafeCachePic("gfx/sp_menu.lmp");
	if (!p)
	{
		MC_AddBox (menu, 60, 10*8, 23, 4);

		MC_AddWhiteText(menu, 92, 12*8, "Could find file", false);
		MC_AddWhiteText(menu, 92, 13*8, "gfx/sp_menu.lmp", false);
	}
	else
	{
#ifdef CLIENTONLY
	MC_AddBox (menu, 60, 10*8, 23, 4);

	MC_AddWhiteText(menu, 92, 12*8, "QuakeWorld is for", false);
	MC_AddWhiteText(menu, 92, 13*8, "Internet play only", false);
#else
	MC_AddPicture(menu, 72, 32, "gfx/sp_menu.lmp");
	
	b = MC_AddConsoleCommand	(menu, 16, 32,	"", "togglemenu\nmaxclients 1;deathmatch 0;coop 0;map start\n");
	menu->selecteditem = (menuoption_t *)b;
	b->common.width = p->width;
	b->common.height = 20;
	b = MC_AddConsoleCommand	(menu, 16, 52,	"", "menu_load\n");
	b->common.width = p->width;
	b->common.height = 20;
	b = MC_AddConsoleCommand	(menu, 16, 72,	"", "menu_save\n");
	b->common.width = p->width;
	b->common.height = 20;

	menu->cursoritem = (menuoption_t*)MC_AddCursor(menu, 54, 32);
#endif
	}
}



typedef struct {
	menucustom_t *list;
	int nummatches;
	int selected;
	int firstshown;
	int maxmatches;
	int pathlen;

	char *command[64];	//these let the menu be used for nearly any sort of file browser.
	char *ext[64];
	int numext;

	struct demofilelist_s{
		char name[64];
		int size;
	} *options;
} demomenu_t;

void M_DemoDraw(int x, int y, menucustom_t *control, menu_t *menu)
{
	char *text;
	int i;
	demomenu_t *info = menu->data;

	if (info->firstshown > info->selected)
		info->firstshown = info->selected;
	if ((vid.height - y)/8 < info->selected - info->firstshown+2)
		info->firstshown = info->selected - (vid.height - y)/8+2;

	i = info->firstshown;
	while(i < info->nummatches)
	{
		if (info->options[i].size)
			text = va("%-30s %6iKB", info->options[i].name+info->pathlen, info->options[i].size/1024);
		else
			text = info->options[i].name+info->pathlen;
		if (i == info->selected)
			Draw_Alt_String(x+8, y+8, text);
		else
			Draw_String(x+8, y+8, text);
		y+=8;
		i++;
	}
}
void ShowDemoMenu (menu_t *menu, char *path);
qboolean M_DemoKey(menucustom_t *control, menu_t *menu, int key)
{
	demomenu_t *info = menu->data;
	if (!info->nummatches)
		return false;
	switch (key)
	{
	case K_UPARROW:
		info->selected--;
		if (info->selected < 0)
			info->selected = 0;
		return true;
	case K_DOWNARROW:
		info->selected++;
		if (info->selected > info->nummatches-1)
			info->selected = info->nummatches-1;
		return true;
	case K_PGUP:
		info->selected-=10;
		if (info->selected < 0)
			info->selected = 0;
		return true;
	case K_PGDN:
		info->selected+=10;
		if (info->selected > info->nummatches-1)
			info->selected = info->nummatches-1;
		return true;
	case K_ENTER:
		if (info->options[info->selected].name[strlen(info->options[info->selected].name)-1] == '/')	//last char is a slash
			ShowDemoMenu(menu, va("%s", info->options[info->selected].name));
		else
		{
			int extnum;
			for (extnum = 0; extnum < info->numext; extnum++)
				if (!stricmp(info->options[info->selected].name + strlen(info->options[info->selected].name)-4, info->ext[extnum]))
					break;

			if (extnum == info->numext)	//wasn't on our list of extensions.
				extnum = 0;

			Cbuf_AddText(va("%s \"%s\"\n", info->command[extnum], info->options[info->selected].name), RESTRICT_LOCAL);
		}
		return true;
	}
	return false;
}

int DemoAddItem(char *filename, int size, void *parm)
{
	int match;
	int extnum;
	demomenu_t *menu = parm;
	if (filename[strlen(filename)-1] != '/')
	{
		for (extnum = 0; extnum < menu->numext; extnum++)
			if (!stricmp(filename + strlen(filename)-4, menu->ext[extnum]))
				break;

		if (extnum == menu->numext)	//wasn't on our list of extensions.
			return true;
	}
	else
	{
		//directory
	}
	if (menu->maxmatches < menu->nummatches+10)
	{
		menu->maxmatches = menu->nummatches+10;
		menu->options = BZ_Realloc(menu->options, menu->maxmatches*sizeof(struct demofilelist_s));
	}
	for (match = 0; match < menu->nummatches; match++)
		if (!strcmp(menu->options[match].name, filename))
			return true;	//already got that one
	Q_strncpyz(menu->options[menu->nummatches].name, filename, sizeof(menu->options[menu->nummatches].name));
	menu->options[menu->nummatches].size = size;
	menu->nummatches++;

	return true;
}

void M_Demo_Remove (menu_t *menu)
{
	demomenu_t *info = menu->data;
	if (info->options)
		BZ_Free(info->options);
}

void ShowDemoMenu (menu_t *menu, char *path)
{
	int c;
	char *s;
	char match[256];
	while (!strcmp(path+strlen(path)-3, "../"))
	{
		c = 0;
		for (s = path+strlen(path)-3; s >= path; s--)
		{
			if (*s == '/')
			{
				c++;
				s[1] = '\0';
				if (c == 2)
					break;
			}
		}
		if (c<2)
			*path = '\0';
	}
	((demomenu_t*)menu->data)->nummatches = 0;
	((demomenu_t*)menu->data)->firstshown = 0;
	((demomenu_t*)menu->data)->selected = 0;
	((demomenu_t*)menu->data)->pathlen = strlen(path);

	if (*path)
	{
		sprintf(match, "%s../", path);
		DemoAddItem(match, 0, menu->data);
	}
	sprintf(match, "%s*", path);

	COM_EnumerateFiles(match, DemoAddItem, menu->data);
}

void M_Menu_Demos_f (void)
{
	demomenu_t *info;
	menu_t *menu;	

	key_dest = key_menu;
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(demomenu_t));
	menu->remove = M_Demo_Remove;
	info = menu->data;

	info->command[0] = "playdemo";
	info->ext[0] = ".qwd";
	info->command[1] = "playdemo";
	info->ext[1] = ".dem";
	info->command[2] = "playdemo";
	info->ext[2] = ".dm2";
	info->command[3] = "playdemo";
	info->ext[3] = ".mvd";
	//there are also qizmo demos (.qwz) out there...
	//we don't support them, but if we were to ask quizmo to decode them for us, we could do.
	info->numext = 4;

	MC_AddWhiteText(menu, 24, 8, "Choose a Demo", false);
	MC_AddWhiteText(menu, 16, 24, "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", false);

	info->list = MC_AddCustom(menu, 0, 32, NULL);
	info->list->draw = M_DemoDraw;
	info->list->key = M_DemoKey;

	menu->selecteditem = (menuoption_t*)info->list;

	ShowDemoMenu(menu, "");
}

void M_Menu_ParticleSets_f (void)
{
	demomenu_t *info;
	menu_t *menu;	

	key_dest = key_menu;
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(demomenu_t));
	menu->remove = M_Demo_Remove;
	info = menu->data;

	info->command[0] = "r_particlesdesc";
	info->ext[0] = ".cfg";
	info->numext = 1;

	MC_AddWhiteText(menu, 24, 8, "Choose a Particle Set", false);
	MC_AddWhiteText(menu, 16, 24, "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", false);

	info->list = MC_AddCustom(menu, 0, 32, NULL);
	info->list->draw = M_DemoDraw;
	info->list->key = M_DemoKey;

	menu->selecteditem = (menuoption_t*)info->list;

	ShowDemoMenu(menu, "particles/");
}

void M_Menu_MediaFiles_f (void)
{
	demomenu_t *info;
	menu_t *menu;	

	key_dest = key_menu;
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(demomenu_t));
	menu->remove = M_Demo_Remove;
	info = menu->data;

	info->ext[0] = ".m3u";
	info->command[0] = "mediaplaylist";
	info->ext[1] = ".mp3";
	info->command[1] = "mediaadd";
	info->ext[2] = ".wav";
	info->command[2] = "mediaadd";
	info->ext[3] = ".ogg";	//will this ever be added properly?
	info->command[3] = "mediaadd";
	info->ext[4] = ".roq";
	info->command[4] = "playfilm";
	info->numext = 5;

#ifdef _WIN32	//avis are only playable on windows due to a windows dll being used to decode them.
	info->ext[info->numext] = ".avi";
	info->command[info->numext] = "playfilm";
	info->numext++;
#endif

	MC_AddWhiteText(menu, 24, 8, "Media List", false);
	MC_AddWhiteText(menu, 16, 24, "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", false);

	info->list = MC_AddCustom(menu, 0, 32, NULL);
	info->list->draw = M_DemoDraw;
	info->list->key = M_DemoKey;

	menu->selecteditem = (menuoption_t*)info->list;

	ShowDemoMenu(menu, "");
}


