//read menu.h

#include "quakedef.h"
#include "winquake.h"
#include "shader.h"
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
	
	MC_AddCenterPicture (menu, 4, 24, "gfx/p_save.lmp");	
	menu->cursoritem = (menuoption_t *)MC_AddRedText(menu, 8, 32, NULL, false);	

	M_ScanSaves ();

	for (i=0 ; i< MAX_SAVEGAMES; i++)
	{
		op = (menuoption_t *)MC_AddConsoleCommandf(menu, 16, 32+8*i, m_filenames[i], "savegame s%i\nclosemenu\n", i);
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
	
	MC_AddCenterPicture(menu, 4, 24, "gfx/p_load.lmp");	
	menu->cursoritem = (menuoption_t *)MC_AddRedText(menu, 8, 32, NULL, false);	

	M_ScanSaves ();

	for (i=0 ; i< MAX_SAVEGAMES; i++)
	{
		if (loadable[i])
			op = (menuoption_t *)MC_AddConsoleCommandf(menu, 16, 32+8*i, m_filenames[i], "loadgame s%i\nclosemenu\n", i);
		else
			MC_AddWhiteText(menu, 16, 32+8*i, m_filenames[i], false);
		if (!menu->selecteditem && op)
			menu->selecteditem = op;
	}
}


#endif

void M_Menu_SinglePlayer_f (void)
{
	menu_t *menu;
#ifndef CLIENTONLY
	int mgt;
	menubutton_t *b;
	mpic_t *p;
#endif

	key_dest = key_menu;
	m_state = m_complex;

#ifdef CLIENTONLY
	menu = M_CreateMenu(0);

	MC_AddWhiteText(menu, 84, 12*8, "This build is unable", false);
	MC_AddWhiteText(menu, 84, 13*8, "to start a local game", false);

	MC_AddBox (menu, 60, 10*8, 25, 4);
#else

	mgt = M_GameType();
	if (mgt == MGT_QUAKE2)
	{	//q2...
		menu = M_CreateMenu(0);

		MC_AddCenterPicture(menu, 4, 24, "pics/m_banner_game");

		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommand	(menu, 64, 40,	"Easy",		"closemenu; skill 0;deathmatch 0; coop 0;newgame\n");
		MC_AddConsoleCommand	(menu, 64, 48,	"Medium",	"closemenu; skill 1;deathmatch 0; coop 0;newgame\n");
		MC_AddConsoleCommand	(menu, 64, 56,	"Hard",		"closemenu; skill 2;deathmatch 0; coop 0;newgame\n");

		MC_AddConsoleCommand	(menu, 64, 72,	"Load Game", "menu_load\n");
		MC_AddConsoleCommand	(menu, 64, 80,	"Save Game", "menu_save\n");

		menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 48, 40, NULL, false);
		return;
	}
	else if (mgt == MGT_HEXEN2)
	{	//h2
		int y;
		int i;
		cvar_t *pc;
		qboolean havemp;
		static char *classlistmp[] = {
			"Paladin",
			"Crusader",
			"Necromancer",
			"Assasin",
			"Demoness"
		};
		menubutton_t *b;
		havemp = COM_FCheckExists("maps/keep1.bsp");
		menu = M_CreateMenu(0);
		MC_AddPicture(menu, 16, 0, 35, 176, "gfx/menu/hplaque.lmp");

		Cvar_Get("cl_playerclass", "1", CVAR_USERINFO|CVAR_ARCHIVE, "Hexen2");

		y = 64-20;

		if (!strncmp(Cmd_Argv(1), "class", 5))
		{
			int pnum;
			extern cvar_t cl_splitscreen;
			pnum = atoi(Cmd_Argv(1)+5);
			if (!pnum)
				pnum = 1;

			MC_AddCenterPicture(menu, 0, 60, "gfx/menu/title2.lmp");

			if (cl_splitscreen.ival)
				MC_AddBufferedText(menu, 80, (y+=8)+12, va("Player %i\n", pnum), false, true); 

			for (i = 0; i < 4+havemp; i++)
			{
				b = MC_AddConsoleCommandHexen2BigFont(menu, 80, y+=20,		classlistmp[i],
						va("p%i setinfo cl_playerclass %i; menu_single %s %s\n",
							pnum,
							i+1,
							((pnum+1 > cl_splitscreen.ival+1)?"skill":va("class%i",pnum+1)),
							Cmd_Argv(2)));
				if (!menu->selecteditem)
					menu->selecteditem = (menuoption_t*)b;
			}
		}
		else if (!strncmp(Cmd_Argv(1), "skill", 5))
		{
			static char *skillnames[6][4] =
			{
				{
					"Easy",
					"Medium",
					"Hard",
					"Nightmare"
				},
				{
					"Apprentice",
					"Squire",
					"Adept",
					"Lord"
				},
				{
					"Gallant",
					"Holy Avenger",
					"Divine Hero",
					"Legend"
				},
				{
					"Sorcerer",
					"Dark Servant",
					"Warlock",
					"Lich King"
				},
				{
					"Rogue",
					"Cutthroat",
					"Executioner",
					"Widow Maker"
				},
				{
					"Larva",
					"Spawn",
					"Fiend",
					"She Bitch"
				}
			};
			char **sn = skillnames[0];
			pc = Cvar_Get("cl_playerclass", "1", CVAR_USERINFO|CVAR_ARCHIVE, "Hexen2");
			if (pc && (unsigned)pc->ival <= 5)
				sn = skillnames[pc->ival];

			MC_AddCenterPicture(menu, 0, 60, "gfx/menu/title5.lmp");
			for (i = 0; i < 4; i++)
			{
				b = MC_AddConsoleCommandHexen2BigFont(menu, 80, y+=20,	sn[i],	va("skill %i; closemenu; disconnect; deathmatch 0; coop 0;wait;map %s\n", i, Cmd_Argv(2)));
				if (!menu->selecteditem)
					menu->selecteditem = (menuoption_t*)b;
			}
		}
		else
		{
			MC_AddCenterPicture(menu, 0, 60, "gfx/menu/title1.lmp");
			if (havemp)
			{
				menu->selecteditem = (menuoption_t*)
				MC_AddConsoleCommandHexen2BigFont(menu, 80, y+=20,	"New Mission",	"menu_single class keep1\n");
				MC_AddConsoleCommandHexen2BigFont(menu, 80, y+=20,	"Old Mission",	"menu_single class demo1\n");
			}
			else
			{
				menu->selecteditem = (menuoption_t*)
				MC_AddConsoleCommandHexen2BigFont(menu, 80, y+=20,	"New Game",		"menu_single class demo1\n");
			}
			MC_AddConsoleCommandHexen2BigFont(menu, 80, y+=20,		"Save Game",	"menu_save\n");
			MC_AddConsoleCommandHexen2BigFont(menu, 80, y+=20,		"Load Game",	"menu_load\n");
		}

		/*
		pc = Cvar_Get("cl_playerclass", "1", CVAR_USERINFO|CVAR_ARCHIVE, "Hexen2");
		if (pc)
			MC_AddCvarCombo (menu, 64, y+=8,	"Player class", pc, havemp?(const char **)classlistmp:(const char **)classlist, (const char **)(classvalues+havemp));
		y+=8;

		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommand	(menu, 64, y+=8,	"Classic: Easy",		"closemenu\nskill 0;deathmatch 0; coop 0;disconnect;wait;map demo1\n");
		MC_AddConsoleCommand	(menu, 64, y+=8,	"Classic: Medium",	"closemenu\nskill 1;deathmatch 0; coop 0;disconnect;wait;map demo1\n");
		MC_AddConsoleCommand	(menu, 64, y+=8,	"Classic: Hard",		"closemenu\nskill 2;deathmatch 0; coop 0;disconnect;wait;map demo1\n");
		y+=8;

		if (havemp)
		{
			MC_AddConsoleCommand(menu, 64, y+=8,	"Expansion: Easy",		"closemenu\nskill 0;deathmatch 0; coop 0;disconnect;wait;map keep1\n");
			MC_AddConsoleCommand(menu, 64, y+=8,	"Expansion: Medium",	"closemenu\nskill 1;deathmatch 0; coop 0;disconnect;wait;map keep1\n");
			MC_AddConsoleCommand(menu, 64, y+=8,	"Expansion: Hard",		"closemenu\nskill 2;deathmatch 0; coop 0;disconnect;wait;map keep1\n");
			y+=8;
		}

		MC_AddConsoleCommand	(menu, 64, y+=8,	"Load Game", "menu_load\n");
		MC_AddConsoleCommand	(menu, 64, y+=8,	"Save Game", "menu_save\n");
		*/

		menu->cursoritem = (menuoption_t *)MC_AddCursor(menu, 56, menu->selecteditem?menu->selecteditem->common.posy:0);

		return;
	}
	else if (QBigFontWorks())
	{
		menu = M_CreateMenu(0);
		MC_AddPicture(menu, 16, 4, 32, 144, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 0, 24, "gfx/p_option.lmp");

		menu->selecteditem = (menuoption_t*)
		MC_AddConsoleCommandQBigFont	(menu, 72, 32,	"New Game",		"closemenu\nmaxclients 1;deathmatch 0;coop 0;map start\n");
		MC_AddConsoleCommandQBigFont	(menu, 72, 52,	"Load Game", "menu_load\n");
		MC_AddConsoleCommandQBigFont	(menu, 72, 72,	"Save Game", "menu_save\n");

		menu->cursoritem = (menuoption_t*)MC_AddCursor(menu, 54, 32);
		return;
	}
	else
	{	//q1
		menu = M_CreateMenu(0);
		MC_AddPicture(menu, 16, 4, 32, 144, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 4, 24, "gfx/p_option.lmp");
	}

	p = R2D_SafeCachePic("gfx/sp_menu.lmp");
	if (!p)
	{
		MC_AddBox (menu, 60, 10*8, 23, 4);

		MC_AddWhiteText(menu, 92, 12*8, "Could find file", false);
		MC_AddWhiteText(menu, 92, 13*8, "gfx/sp_menu.lmp", false);
	}
	else
	{
		MC_AddPicture(menu, 72, 32, 232, 64, "gfx/sp_menu.lmp");

		b = MC_AddConsoleCommand	(menu, 16, 32,	"", "closemenu\nmaxclients 1;deathmatch 0;coop 0;map start\n");
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
	}
#endif
}


typedef struct demoitem_s {
	char name[MAX_QPATH];
	qboolean isdir;
	int size;
	struct demoitem_s *next;
	struct demoitem_s *prev;
} demoitem_t;

typedef struct {
	menucustom_t *list;
	demoitem_t *selected;
	demoitem_t *firstitem;
	int pathlen;

	char *command[64];	//these let the menu be used for nearly any sort of file browser.
	char *ext[64];
	int numext;

	demoitem_t *items;
} demomenu_t;

static void M_DemoDraw(int x, int y, menucustom_t *control, menu_t *menu)
{
	char *text;
	demomenu_t *info = menu->data;
	demoitem_t *item, *lostit;
	int ty;

	ty = vid.height-24;
	item = info->selected;
	while(item)
	{
		if (info->firstitem == item)
			break;
		if (ty < y)
		{
			//we couldn't find it
			for (lostit = info->firstitem; lostit; lostit = lostit->prev)
			{
				if (info->selected == lostit)
				{
					item = lostit;
					break;
				}
			}
			info->firstitem = item;
			break;
		}
		item = item->prev;
		ty-=8;
	}
	if (!item)
		info->firstitem = info->items;


	item = info->firstitem;
	while(item)
	{
		if (y+8 >= vid.height)
			return;
		if (!item->isdir)
			text = va("%-32.32s%6iKB", item->name+info->pathlen, item->size/1024);
		else
			text = item->name+info->pathlen;
		if (item == info->selected)
			Draw_AltFunString(x, y+8, text);
		else
			Draw_FunString(x, y+8, text);
		y+=8;
		item = item->next;
	}
}
static void ShowDemoMenu (menu_t *menu, char *path);
static qboolean M_DemoKey(menucustom_t *control, menu_t *menu, int key)
{
	demomenu_t *info = menu->data;
	int i;

	switch (key)
	{
	case K_MWHEELUP:
	case K_UPARROW:
		if (info->selected && info->selected->prev)
			info->selected = info->selected->prev;
		return true;
	case K_MWHEELDOWN:
	case K_DOWNARROW:
		if (info->selected && info->selected->next)
			info->selected = info->selected->next;
		return true;
	case K_HOME:
		info->selected = info->items;
		return true;
	case K_END:
		info->selected = info->items;
		while(info->selected->next)
			info->selected = info->selected->next;
		return true;
	case K_PGUP:
		for (i = 0; i < 10; i++)
		{
			if (info->selected && info->selected->prev)
				info->selected = info->selected->prev;
		}
		return true;
	case K_PGDN:
		for (i = 0; i < 10; i++)
		{
			if (info->selected && info->selected->next)
				info->selected = info->selected->next;
		}
		return true;
	case K_ENTER:
		if (info->selected)
		{
			if (info->selected->isdir)
				ShowDemoMenu(menu, va("%s", info->selected->name));
			else
			{
				int extnum;
				for (extnum = 0; extnum < info->numext; extnum++)
					if (!stricmp(info->selected->name + strlen(info->selected->name)-4, info->ext[extnum]))
						break;

				if (extnum == info->numext)	//wasn't on our list of extensions.
					extnum = 0;

				Cbuf_AddText(va("%s \"%s\"\n", info->command[extnum], info->selected->name), RESTRICT_LOCAL);
				M_ToggleMenu_f();
				
			}
		}
		return true;
	}
	return false;
}

static int DemoAddItem(const char *filename, int size, void *parm)
{
	int extnum;
	demomenu_t *menu = parm;
	demoitem_t *link, *newi;
	int side;
	qboolean isdir;
	char tempfname[MAX_QPATH];

	char *i;

	i = strchr(filename+menu->pathlen, '/');
	if (i == NULL)
	{
		for (extnum = 0; extnum < menu->numext; extnum++)
			if (!stricmp(filename + strlen(filename)-4, menu->ext[extnum]))
				break;

		if (extnum == menu->numext)	//wasn't on our list of extensions.
			return true;
		isdir = false;
	}
	else
	{
		i++;
		if (i-filename > sizeof(tempfname)-2)
			return true;	//too long to fit in our buffers anyway
		strncpy(tempfname, filename, i-filename);
		tempfname[i-filename] = 0;
		filename = tempfname;

		size = 0;
		isdir = true;
	}

	if (!menu->items)
		menu->items = newi = BZ_Malloc(sizeof(*newi));
	else
	{
		link = menu->items;
		for(;;)
		{
			if (link->isdir != isdir)	//bias directories, so they sink
				side = (link->isdir > isdir)?1:-1;
			else
				side = stricmp(link->name, filename);
			if (side == 0)
				return true;	//already got this file
			else if (side > 0)
			{
				if (!link->prev)
				{
					link->prev = newi = BZ_Malloc(sizeof(*newi));
					break;
				}
				link = link->prev;
			}
			else
			{
				if (!link->next)
				{
					link->next = newi = BZ_Malloc(sizeof(*newi));
					break;
				}
				link = link->next;
			}
		}
	}
	
	Q_strncpyz(newi->name, filename, sizeof(newi->name));
	newi->size = size;
	newi->isdir = isdir;
	newi->prev = NULL;
	newi->next = NULL;

	return true;
}

//converts the binary tree into sorted linked list
static void M_Demo_Flatten(demomenu_t *info)
{
	demoitem_t *btree = info->items, *item, *lastitem;
	demoitem_t *listhead = NULL, *listlast = NULL;

	while(btree)
	{
		if (!btree->prev)
		{	//none on left side, descend down right removing head node
			item = btree;
			btree = btree->next;
		}
		else
		{
			item = btree;
			lastitem = item;
			for (;;)
			{
				if (!item->prev)
				{
					lastitem->prev = item->next;
					break;
				}
				lastitem = item;
				item = lastitem->prev;
			}
		}
		if (listlast)
		{
			listlast->next = item;
			item->prev = listlast;
			listlast = item;
		}
		else
		{
			listhead = listlast = item;
			item->prev = NULL;
		}
	}
	if (listlast)
		listlast->next = NULL;
	info->items = listhead;
	info->selected = listhead;
	info->firstitem = listhead;
}

static void M_Demo_Flush (demomenu_t *info)
{
	demoitem_t *item;
	while (info->items)
	{
		item = info->items;
		info->items = item->next;
		BZ_Free(item);
	}
	info->items = NULL;
	info->selected = NULL;
	info->firstitem = NULL;
}

static void M_Demo_Remove (menu_t *menu)
{
	demomenu_t *info = menu->data;
	M_Demo_Flush(info);
}

static void ShowDemoMenu (menu_t *menu, char *path)
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
	((demomenu_t*)menu->data)->selected = NULL;
	((demomenu_t*)menu->data)->pathlen = strlen(path);

	M_Demo_Flush(menu->data);
	if (*path)
	{
		sprintf(match, "%s../", path);
		DemoAddItem(match, 0, menu->data);
	}
	sprintf(match, "%s*", path);

	COM_EnumerateFiles(match, DemoAddItem, menu->data);
	M_Demo_Flatten(menu->data);
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


