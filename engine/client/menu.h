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

//
// the net drivers should just set the apropriate bits in m_activenet,
// instead of having the menu code look through their internal tables
//
#define	MNET_IPX		1
#define	MNET_TCP		2

extern	int	m_activenet;

//
// menus
//
void M_Init (void);
void M_Keydown (int key);
void M_Keyup (int key);
void M_Draw (int uimenu);
void M_ToggleMenu_f (void);
qpic_t	*M_CachePic (char *path);
void M_DrawTextBox (int x, int y, int width, int lines);
void M_Menu_Quit_f (void);

struct menu_s;



void XWindows_Draw(void);
void XWindows_Key(int key);
void XWindows_Keyup(int key);
void XWindows_Init(void);



typedef enum {m_none, m_complex, m_help, m_keys, m_slist, m_media, m_xwindows} m_state_t;
extern m_state_t m_state;

typedef enum {mt_childwindow, mt_button, mt_buttonbigfont, mt_box, mt_colouredbox, mt_line, mt_edit, mt_text, mt_slider, mt_combo, mt_checkbox, mt_picture, mt_menudot, mt_custom} menutype_t;

typedef struct {	//must be first of each structure type.
	menutype_t type;
	int posx;
	int posy;
	int width;
	int height;
	qboolean iszone;
	union menuoption_s *next;
} menucommon_t;


typedef struct {
	menucommon_t common;
	const char *text;
	const char *command;
	qboolean (*key) (union menuoption_s *option, struct menu_s *, int key);
} menubutton_t;

#define MAX_EDIT_LENGTH 256
typedef struct {
	menucommon_t common;
	const char *caption;
	cvar_t *cvar;
	char text[MAX_EDIT_LENGTH];
	int cursorpos;
} menuedit_t;
typedef struct {
	menucommon_t common;
	float min;
	float max;
	float current;
	float smallchange;
	float largechange;
	cvar_t *var;
	const char *text;
} menuslider_t;

typedef enum {CHK_CHECKED, CHK_TOGGLE} chk_set_t;
typedef struct {
	menucommon_t common;
	const char *text;
	cvar_t *var;
	float value;
	qboolean (*func) (union menuoption_s *option, chk_set_t set);
} menucheck_t;

typedef struct {
	menucommon_t common;
	const char *text;
	qboolean isred;
} menutext_t;

typedef struct menucustom_s {
	menucommon_t common;
	void *data;
	void (*draw) (int x, int y, struct menucustom_s *, struct menu_s *);
	qboolean (*key) (struct menucustom_s *, struct menu_s *, int key);
} menucustom_t;

typedef struct {
	menucommon_t common;
	char *picturename;
} menupicture_t;

typedef struct {
	menucommon_t common;
	int width;
	int height;
} menubox_t;

typedef struct {
	menucommon_t common;

	const char *caption;
	const char **options;
	const char **values;
	cvar_t *cvar;
	int numoptions;
	int selectedoption;
} menucombo_t;

typedef union menuoption_s {
	menucommon_t common;
	menubutton_t button;
	menuedit_t edit;
	menucombo_t combo;
	menuslider_t slider;
	menutext_t text;
	menucustom_t custom;
	menupicture_t picture;
	menubox_t	box;
	menucheck_t check;
} menuoption_t;

typedef struct menu_s {
	int xpos;
	int ypos;
	int width;
	int height;
	int numoptions;

	qboolean iszone;
	qboolean exclusive;

	void *data;	//typecast

	void (*remove)	(struct menu_s *);
	qboolean (*key)		(int key, struct menu_s *);	//true if key was handled
	void (*event)	(struct menu_s *);
	menuoption_t *options;

	menuoption_t *selecteditem;
	
	struct menu_s *child;
	struct menu_s *parent;

	int cursorpos;
	menuoption_t *cursoritem;
} menu_t;

menutext_t *MC_AddBufferedText(menu_t *menu, int x, int y, const char *text, qboolean rightalign, qboolean red);
menutext_t *MC_AddRedText(menu_t *menu, int x, int y, const char *text, qboolean rightalign);
menutext_t *MC_AddWhiteText(menu_t *menu, int x, int y, const char *text, qboolean rightalign);
menubox_t *MC_AddBox(menu_t *menu, int x, int y, int width, int height);
menupicture_t *MC_AddPicture(menu_t *menu, int x, int y, char *picname);
menupicture_t *MC_AddCenterPicture(menu_t *menu, int y, char *picname);
menupicture_t *MC_AddCursor(menu_t *menu, int x, int y);
menuslider_t *MC_AddSlider(menu_t *menu, int x, int y, const char *text, cvar_t *var, float min, float max);
menucheck_t *MC_AddCheckBox(menu_t *menu, int x, int y, const char *text, cvar_t *var);
menubutton_t *MC_AddConsoleCommand(menu_t *menu, int x, int y, const char *text, const char *command);
menubutton_t *MC_AddCommand(menu_t *menu, int x, int y, char *text, qboolean (*command) (union menuoption_s *,struct menu_s *,int));

menucombo_t *MC_AddCombo(menu_t *menu, int x, int y, const char *caption, const char **text, int initialvalue);

menubutton_t *MC_AddCommand(menu_t *menu, int x, int y, char *text, qboolean (*command) (union menuoption_s *,struct menu_s *,int));
menuedit_t *MC_AddEdit(menu_t *menu, int x, int y, char *text, char *def);
menuedit_t *MC_AddEditCvar(menu_t *menu, int x, int y, char *text, char *name);
menucustom_t *MC_AddCustom(menu_t *menu, int x, int y, const char *data);

menu_t *M_CreateMenu (int extrasize);
void M_AddMenu (menu_t *menu);
void M_AddMenuFront (menu_t *menu);
void M_HideMenu (menu_t *menu);
void M_RemoveMenu (menu_t *menu);
void M_RemoveAllMenus (void);

void M_Complex_Key(int key);
void M_Complex_Draw(void);
void M_Script_Init(void);
void M_Serverlist_Init(void);

extern qboolean	m_entersound;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_Setup_f (void);
		void M_Menu_Net_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_SerialConfig_f (void);
	void M_Menu_ModemConfig_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_ServerList_f (void);
void M_Menu_Media_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
		void M_Net_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
		void M_Video_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);
void M_SerialConfig_Draw (void);
	void M_ModemConfig_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_ServerList_Draw (void);
void M_Media_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_Setup_Key (int key);
		void M_Net_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
		void M_Video_Key (int key);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);
void M_SerialConfig_Key (int key);
	void M_ModemConfig_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);
void M_Media_Key (int key);

void MasterInfo_Begin(void);
void M_DrawServers(void);
void M_SListKey(int key);

//drawing funcs
void M_BuildTranslationTable(int top, int bottom);
void M_DrawTransPicTranslate (int x, int y, qpic_t *pic);
void M_DrawTransPic (int x, int y, qpic_t *pic);
void M_DrawCharacter (int cx, int line, unsigned int num);
void M_Print (int cx, int cy, qbyte *str);
void M_PrintWhite (int cx, int cy, qbyte *str);
void M_DrawPic (int x, int y, qpic_t *pic);
