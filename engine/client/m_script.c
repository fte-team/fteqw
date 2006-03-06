//read menu.h

#include "quakedef.h"

int selectitem;
menu_t *menu_script;
cvar_t menualias = SCVAR("menualias", "");

void M_Script_Remove (menu_t *menu)
{
	menu_script = NULL;
	Cvar_Set(&menualias, "");
}
qboolean M_Script_Key (int key, menu_t *menu)
{
	if (menu->selecteditem && menu->selecteditem->common.type == mt_edit)
		return false;
	if (key >= '0' && key <= '9')
	{
		if (key == '0')	//specal case so that "hello" < "0"... (plus matches impulses)
			Cbuf_AddText(va("set option %i\n%s\n", key-'0'+1, menualias.string), RESTRICT_LOCAL);
		return true;
	}
	return false;
}

void M_MenuS_Clear_f (void)
{
	if (menu_script)
	{
		M_RemoveMenu(menu_script);
		menu_script = NULL;
	}

//	Cvar_Set(menualias.name, "");
}

void M_MenuS_Script_f (void)	//create a menu.
{
	int items;
	extern menu_t *currentmenu;
	menu_t *oldmenu;
	char *alias = Cmd_Argv(1);
//	if (key_dest != key_console)
		key_dest = key_menu;
	m_state = m_complex;

	selectitem = 0;
	items=0;

	if (menu_script)
	{
		menuoption_t *option;
		for (option = menu_script->options; option; option = option->common.next)
		{
			if (option->common.type == mt_button)
			{
				if (menu_script->selecteditem == option)
					selectitem = items;
				items++;
			}
		}
		selectitem = items - selectitem-1;
		M_MenuS_Clear_f();
	}

	oldmenu = currentmenu;

	menu_script = M_CreateMenu(0);
	if (oldmenu)
	{
		M_HideMenu(oldmenu);	//bring to front
		M_AddMenu(oldmenu);
	}	
	menu_script->remove = M_Script_Remove;
	menu_script->key = M_Script_Key;
	
	if (Cmd_Argc() == 1 || !*alias)
		Cvar_Set(&menualias, "_");
	else
		Cvar_Set(&menualias, alias);
}

void M_MenuS_Box_f (void)
{
	int x = atoi(Cmd_Argv(1));
	int y = atoi(Cmd_Argv(2));
	int width = atoi(Cmd_Argv(3));
	int height = atoi(Cmd_Argv(4));

	if (!menu_script)
	{
		Con_Printf("%s with no active menu\n", Cmd_Argv(0));
		return;
	}

	MC_AddBox(menu_script, x, y, width/8, height/8);
}

void M_MenuS_CheckBox_f (void)
{
	int x = atoi(Cmd_Argv(1));
	int y = atoi(Cmd_Argv(2));
	char *text = Cmd_Argv(3);
	char *cvarname = Cmd_Argv(4);
	int bitmask = atoi(Cmd_Argv(5));
	cvar_t *cvar;

	if (!menu_script)
	{
		Con_Printf("%s with no active menu\n", Cmd_Argv(0));
		return;
	}
	cvar = Cvar_Get(cvarname, text, 0, "User variables");
	if (!cvar)
		return;
	MC_AddCheckBox(menu_script, x, y, text, cvar, bitmask);
}

void M_MenuS_Slider_f (void)
{
	int x = atoi(Cmd_Argv(1));
	int y = atoi(Cmd_Argv(2));
	char *text = Cmd_Argv(3);
	char *cvarname = Cmd_Argv(4);
	float min = atof(Cmd_Argv(5));
	float max = atof(Cmd_Argv(6));
	cvar_t *cvar;

	if (!menu_script)
	{
		Con_Printf("%s with no active menu\n", Cmd_Argv(0));
		return;
	}
	cvar = Cvar_Get(cvarname, text, 0, "User variables");
	if (!cvar)
		return;
	MC_AddSlider(menu_script, x, y, text, cvar, min, max);
}

void M_MenuS_Picture_f (void)
{
	int x = atoi(Cmd_Argv(1));
	int y = atoi(Cmd_Argv(2));
	char *picname = Cmd_Argv(3);

	if (!menu_script)
	{
		Con_Printf("%s with no active menu\n", Cmd_Argv(0));
		return;
	}

	MC_AddPicture(menu_script, x, y, picname);
}

void M_MenuS_Edit_f (void)
{
	int x = atoi(Cmd_Argv(1));
	int y = atoi(Cmd_Argv(2));
	char *text = Cmd_Argv(3);
	char *def = Cmd_Argv(4);

	if (!menu_script)
	{
		Con_Printf("%s with no active menu\n", Cmd_Argv(0));
		return;
	}

	MC_AddEditCvar(menu_script, x, y, text, def);
}

void M_MenuS_Text_f (void)
{
	menuoption_t *option;
	int x = atoi(Cmd_Argv(1));
	int y = atoi(Cmd_Argv(2));
	char *text = Cmd_Argv(3);
	char *command = Cmd_Argv(4);

	if (!menu_script)
	{
		Con_Printf("%s with no active menu\n", Cmd_Argv(0));
		return;
	}
	if (Cmd_Argc() == 4)
		MC_AddBufferedText(menu_script, x, y, text, false, false);
	else
	{
		option = (menuoption_t *)MC_AddConsoleCommand(menu_script, x, y, text, va("set option %s\n%s\n", command, menualias.string));
		if (selectitem-- == 0)
			menu_script->selecteditem = option;
	}
}

void M_MenuS_TextBig_f (void)
{
	menuoption_t *option;
	int x = atoi(Cmd_Argv(1));
	int y = atoi(Cmd_Argv(2));
	char *text = Cmd_Argv(3);
	char *command = Cmd_Argv(4);

	if (!menu_script)
	{
		Con_Printf("%s with no active menu\n", Cmd_Argv(0));
		return;
	}
	if (*command)
		MC_AddConsoleCommandQBigFont(menu_script, x, y, text, command);
	else
	{
		option = (menuoption_t *)MC_AddConsoleCommand(menu_script, x, y, text, va("set option %s\n%s\n", command, menualias.string));
		if (selectitem-- == 0)
			menu_script->selecteditem = option;
	}
}

void M_MenuS_Bind_f (void)
{
	int x = atoi(Cmd_Argv(1));
	int y = atoi(Cmd_Argv(2));
	char *caption = Cmd_Argv(3);
	char *command = Cmd_Argv(4);

	if (!menu_script)
	{
		Con_Printf("%s with no active menu\n", Cmd_Argv(0));
		return;
	}

	if (!*caption)
		caption = command;

	MC_AddBind(menu_script, x, y, command, caption);
}

/*
menuclear
menualias menucallback

menubox 0 0 320 8
menutext 0 0 "GO GO GO!!!" 		"radio21"
menutext 0 8 "Fall back" 		"radio22"
menutext 0 8 "Stick together" 		"radio23"
menutext 0 16 "Get in position"		"radio24"
menutext 0 24 "Storm the front"	 	"radio25"
menutext 0 24 "Report in"	 	"radio26"
menutext 0 24 "Cancel"	
*/
void M_Script_Init(void)
{
	Cmd_AddCommand("menuclear",	M_MenuS_Clear_f);
	Cmd_AddCommand("conmenu",	M_MenuS_Script_f);
	Cmd_AddCommand("menubox",	M_MenuS_Box_f);
	Cmd_AddCommand("menuedit",	M_MenuS_Edit_f);
	Cmd_AddCommand("menutext",	M_MenuS_Text_f);
	Cmd_AddCommand("menutextbig",	M_MenuS_TextBig_f);
	Cmd_AddCommand("menupic",	M_MenuS_Picture_f);
	Cmd_AddCommand("menucheck",	M_MenuS_CheckBox_f);
	Cmd_AddCommand("menuslider",	M_MenuS_Slider_f);
	Cmd_AddCommand("menubind",	M_MenuS_Bind_f);

	Cvar_Register(&menualias, "Scripting");
}
