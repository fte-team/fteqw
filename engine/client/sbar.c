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
// sbar.c -- status bar code

#include "quakedef.h"
#include "shader.h"

#define CON_ALTMASK (CON_2NDCHARSETTEXT|CON_WHITEMASK)

#ifdef QUAKEHUD

extern cvar_t *hud_tracking_show;
extern cvar_t *hud_miniscores_show;

cvar_t scr_scoreboard_drawtitle = CVARD("scr_scoreboard_drawtitle", "1", "Wastes screen space when looking at the scoreboard.");
cvar_t scr_scoreboard_forcecolors = CVARD("scr_scoreboard_forcecolors", "0", "Makes the scoreboard colours obey enemycolor/teamcolor rules.");	//damn americans
cvar_t scr_scoreboard_newstyle = CVARD("scr_scoreboard_newstyle", "1", "Display team colours and stuff in a style popularised by Electro. Looks more modern, but might not quite fit classic huds.");	// New scoreboard style ported from Electro, by Molgrum
cvar_t scr_scoreboard_showfrags = CVARD("scr_scoreboard_showfrags", "0", "Display kills+deaths+teamkills, as determined by fragfile.dat-based conprint parsing. These may be inaccurate if you join mid-game.");
cvar_t scr_scoreboard_showflags = CVARD("scr_scoreboard_showflags", "2", "Display flag caps+touches on the scoreboard, where our fragfile.dat supports them.\n0: off\n1: on\n2: on only if someone appears to have interacted with a flag.");
cvar_t scr_scoreboard_fillalpha = CVARD("scr_scoreboard_fillalpha", "0.7", "Transparency amount for newstyle scoreboard.");
cvar_t scr_scoreboard_teamscores = CVARD("scr_scoreboard_teamscores", "1", "Makes +showscores act as +showteamscores. Because reasons.");
cvar_t scr_scoreboard_teamsort = CVARD("scr_scoreboard_teamsort", "0", "On the scoreboard, sort players by their team BEFORE their personal score.");
cvar_t scr_scoreboard_titleseperator = CVAR("scr_scoreboard_titleseperator", "1");
cvar_t sbar_teamstatus = CVARD("sbar_teamstatus", "1", "Display the last team say from each of your team members just above the sbar area.");

cvar_t cl_sbaralpha = CVARAD("cl_sbaralpha", "0.75", "scr_sbaralpha", "Specifies the transparency of the status bar. Only Takes effect when cl_sbar is set to 2.");	//with premultiplied alpha, this needs to affect the RGB values too.

//===========================================
//rogue changed and added defines
#define RIT_SHELLS              (1u<<7)
#define RIT_NAILS               (1u<<8)
#define RIT_ROCKETS             (1u<<9)
#define RIT_CELLS               (1u<<10)
#define RIT_AXE                 (1u<<11)
#define RIT_LAVA_NAILGUN        (1u<<12)
#define RIT_LAVA_SUPER_NAILGUN  (1u<<13)
#define RIT_MULTI_GRENADE       (1u<<14)
#define RIT_MULTI_ROCKET        (1u<<15)
#define RIT_PLASMA_GUN          (1u<<16)
#define RIT_ARMOR1              (1u<<23)
#define RIT_ARMOR2              (1u<<24)
#define RIT_ARMOR3              (1u<<25)
#define RIT_LAVA_NAILS          (1u<<26)
#define RIT_PLASMA_AMMO         (1u<<27)
#define RIT_MULTI_ROCKETS       (1u<<28)
#define RIT_SHIELD              (1u<<29)
#define RIT_ANTIGRAV            (1u<<30)
#define RIT_SUPERHEALTH         (1u<<31)

//===========================================
//hipnotic added defines

#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<(23+2))
#define HIT_EMPATHY_SHIELDS (1<<(23+3))





int			sb_updates;		// if >= vid.numpages, no update needed

qboolean	sbar_parsingteamstatuses;	//so we don't eat it if its not displayed

#define STAT_MINUS		10	// num frame for '-' stats digit
static apic_t		*sb_nums[2][11];
static apic_t		*sb_colon, *sb_slash;
static apic_t		*sb_ibar;
static apic_t		*sb_sbar;
static apic_t		*sb_scorebar;

static apic_t		*sb_weapons[7][8];	// 0 is active, 1 is owned, 2-5 are flashes
static apic_t		*sb_ammo[4];
static apic_t		*sb_sigil[4];
static apic_t		*sb_armor[3];
static apic_t		*sb_items[32];

static apic_t	*sb_faces[7][2];		// 0 is gibbed, 1 is dead, 2-6 are alive
							// 0 is static, 1 is temporary animation
static apic_t	*sb_face_invis;
static apic_t	*sb_face_quad;
static apic_t	*sb_face_invuln;
static apic_t	*sb_face_invis_invuln;

//rogue pictures.
static qboolean	sbar_rogue;
static apic_t      *rsb_invbar[2];
static apic_t      *rsb_weapons[5];
static apic_t      *rsb_items[2];
static apic_t      *rsb_ammo[3];
static apic_t      *rsb_teambord;
//all must be found for any to be used.

//hipnotic pictures and stuff
static qboolean	sbar_hipnotic;
static apic_t      *hsb_weapons[7][5];   // 0 is active, 1 is owned, 2-5 are flashes
static int         hipweapons[4] = {HIT_LASER_CANNON_BIT,HIT_MJOLNIR_BIT,4,HIT_PROXIMITY_GUN_BIT};
static apic_t      *hsb_items[2];
//end hipnotic

static qboolean	sbarfailed;
#ifdef HEXEN2
static qboolean	sbar_hexen2;
#endif

vrect_t		sbar_rect;	//screen area that the sbar must fit.
float		sbar_rect_left;

int			sb_lines;			// scan lines to draw

void Sbar_DeathmatchOverlay (playerview_t *pv, int start);
void Sbar_TeamOverlay (playerview_t *pv);
static void Sbar_MiniDeathmatchOverlay (playerview_t *pv);
void Sbar_ChatModeOverlay(playerview_t *pv);

static int Sbar_PlayerNum(playerview_t *pv)
{
	int num;
	num = pv->spectator?Cam_TrackNum(pv):-1;
	if (num < 0)
		num = pv->playernum;
	return num;
}

static int Sbar_TopColour(player_info_t *p)
{
	if (cl.teamfortress)
	{
		if (!Q_strcasecmp(p->team, "red"))
			return 4;
		if (!Q_strcasecmp(p->team, "blue"))
			return 13;
	}
	if (scr_scoreboard_forcecolors.ival)
		return p->ttopcolor;
	else
		return p->rtopcolor;
}

static int Sbar_BottomColour(player_info_t *p)
{
	if (cl.teamfortress)
	{
		if (!Q_strcasecmp(p->team, "red"))
			return 4;
		if (!Q_strcasecmp(p->team, "blue"))
			return 13;
	}
	if (scr_scoreboard_forcecolors.ival)
		return p->tbottomcolor;
	else
		return p->rbottomcolor;
}

#endif
//Draws a pre-marked-up string with no width limit. doesn't support new lines
void Draw_ExpandedString(float x, float y, conchar_t *str)
{
	int px, py;
	unsigned int codeflags, codepoint;
	Font_BeginString(font_default, x, y, &px, &py);
	while(*str)
	{
		str = Font_Decode(str, &codeflags, &codepoint);
		px = Font_DrawChar(px, py, codeflags, codepoint);
	}
	Font_EndString(font_default);
}

//Draws a marked-up string using the regular char set with no width limit. doesn't support new lines
void Draw_FunString(float x, float y, const void *str)
{
	conchar_t buffer[2048];
	COM_ParseFunString(CON_WHITEMASK, str, buffer, sizeof(buffer), false);

	Draw_ExpandedString(x, y, buffer);
}
//Draws a marked up string using the alt char set (legacy mode would be |128)
void Draw_AltFunString(float x, float y, const void *str)
{
	conchar_t buffer[2048];
	COM_ParseFunString(CON_ALTMASK, str, buffer, sizeof(buffer), false);

	Draw_ExpandedString(x, y, buffer);
}

//Draws a marked up string no wider than $width virtual pixels.
void Draw_FunStringWidth(float x, float y, const void *str, int width, int rightalign, qboolean highlight)
{
	conchar_t buffer[2048];
	conchar_t *w;
	int px, py;
	int fw = 0;
	unsigned int codeflags, codepoint;

	//be generous and round up, to avoid too many issues with truncations
	width = ceil((width*(float)vid.rotpixelwidth)/vid.width);

	codeflags = (highlight&1)?CON_ALTMASK:CON_WHITEMASK;
	if (highlight&2)
		codeflags |= CON_BLINKTEXT;
	COM_ParseFunString(codeflags, str, buffer, sizeof(buffer), false);

	Font_BeginString(font_default, x, y, &px, &py);
	if (rightalign)
	{
		for (w = buffer; *w; )
		{
			w = Font_Decode(w, &codeflags, &codepoint);
			fw += Font_CharWidth(codeflags, codepoint);
		}
		if (rightalign == 2)
		{
			if (fw < width)
			{
				px += (width-fw)/2;
				width = fw;
			}
		}
		else
		{
			px += width;
			if (fw > width)
				fw = width;
			px -= fw;
		}
	}

	for (w = buffer; *w; )
	{
		w = Font_Decode(w, &codeflags, &codepoint);

		width -= Font_CharWidth(codeflags, codepoint);
		if (width < 0)
			return;
		px = Font_DrawChar(px, py, codeflags, codepoint);
	}
	Font_EndString(font_default);
}
#ifdef QUAKEHUD

static qboolean largegame = false;






#ifdef Q2CLIENT
static void DrawHUDString (char *string, float x, float y, int centerwidth, qboolean alt)
{
	vec2_t fontscale = {8,8};
	R_DrawTextField(x, y, centerwidth, 1024, string, alt?CON_ALTMASK:CON_WHITEMASK, CPRINT_TALIGN, font_default, fontscale);
}
#define STAT_MINUS		10	// num frame for '-' stats digit
static char		*q2sb_nums[2][11] =
{
	{"num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
	"num_6", "num_7", "num_8", "num_9", "num_minus"},
	{"anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
	"anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"}
};

static mpic_t *Sbar_Q2CachePic(char *name)
{
	return R2D_SafeCachePic(va("pics/%s.pcx", name));
}

#define	ICON_WIDTH	24
#define	ICON_HEIGHT	24
#define	CHAR_WIDTH	16
#define	ICON_SPACE	8
static void SCR_DrawField (float x, float y, int color, float width, int value)
{
	char	num[16], *ptr;
	int		l;
	int		frame;
	mpic_t *p;
	int pw,ph;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	snprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += 2 + CHAR_WIDTH*(width - l);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		p = Sbar_Q2CachePic(q2sb_nums[color][frame]);
		if (p && R_GetShaderSizes(p, &pw, &ph, false)>0)
			R2D_ScalePic (x,y,pw, ph, p);
		x += CHAR_WIDTH;
		ptr++;
		l--;
	}
}

char *Get_Q2ConfigString(int i)
{
	if (i >= Q2CS_IMAGES && i < Q2CS_IMAGES	+ Q2MAX_IMAGES)
		return cl.image_name[i-Q2CS_IMAGES]?cl.image_name[i-Q2CS_IMAGES]:"";
	if (i >= Q2CS_ITEMS && i < Q2CS_ITEMS + Q2MAX_ITEMS)
		return cl.item_name[i-Q2CS_ITEMS]?cl.item_name[i-Q2CS_ITEMS]:"";
	if (i == Q2CS_STATUSBAR)
		return cl.q2statusbar;
	if (i == Q2CS_NAME)
		return cl.levelname;

	if (i >= Q2CS_MODELS && i < Q2CS_MODELS	+ Q2MAX_MODELS)
		return cl.model_name [i-Q2CS_MODELS];
	if (i >= Q2CS_SOUNDS && i < Q2CS_SOUNDS	+ Q2MAX_SOUNDS)
		return cl.sound_name [i-Q2CS_SOUNDS];
	if (i == Q2CS_AIRACCEL)
		return "4";
	if (i >= Q2CS_PLAYERSKINS && i < Q2CS_GENERAL+Q2MAX_GENERAL)
		return cl.configstring_general[i-Q2CS_PLAYERSKINS]?cl.configstring_general[i-Q2CS_PLAYERSKINS]:"";
//#define	Q2CS_LIGHTS				(Q2CS_IMAGES	+Q2MAX_IMAGES)
//#define	Q2CS_ITEMS				(Q2CS_LIGHTS	+Q2MAX_LIGHTSTYLES)
//#define	Q2CS_PLAYERSKINS		(Q2CS_ITEMS		+Q2MAX_ITEMS)
//#define Q2CS_GENERAL			(Q2CS_PLAYERSKINS	+Q2MAX_CLIENTS)
	return "";
}
void Sbar_ExecuteLayoutString (char *s, int seat)
{
	int		x, y;
	int		value;
	int		width;
	int		index;
	int pw, ph;
//	q2clientinfo_t	*ci;
	mpic_t *p;
	q2player_state_t *ps = &cl.q2frame.playerstate[seat];

	if (cls.state != ca_active)
		return;

	if (!s[0])
		return;

	x = sbar_rect.x;
	y = sbar_rect.y;
	width = 3;

	while (s)
	{
		s = COM_Parse (s);
		if (!strcmp(com_token, "xl"))
		{
			s = COM_Parse (s);
			x = sbar_rect.x + atoi(com_token);
			continue;
		}
		if (!strcmp(com_token, "xr"))
		{
			s = COM_Parse (s);
			x = sbar_rect.x + sbar_rect.width + atoi(com_token);
			continue;
		}
		if (!strcmp(com_token, "xv"))
		{
			s = COM_Parse (s);
			x = sbar_rect.x + (sbar_rect.width-320)/2 + atoi(com_token);
			continue;
		}

		if (!strcmp(com_token, "yt"))
		{
			s = COM_Parse (s);
			y = sbar_rect.y + atoi(com_token);
			continue;
		}
		if (!strcmp(com_token, "yb"))
		{
			s = COM_Parse (s);
			y = sbar_rect.y + sbar_rect.height + atoi(com_token);
			continue;
		}
		if (!strcmp(com_token, "yv"))
		{
			s = COM_Parse (s);
			y = sbar_rect.y + (sbar_rect.height-240)/2 + atoi(com_token);
			continue;
		}

		if (!strcmp(com_token, "pic"))
		{	// draw a pic from a stat number
			s = COM_Parse (s);
			value = ps->stats[atoi(com_token)];
			if (value >= Q2MAX_IMAGES || value < 0)
				Host_EndGame ("Pic >= Q2MAX_IMAGES");
			if (*Get_Q2ConfigString(Q2CS_IMAGES+value))
			{
//				SCR_AddDirtyPoint (x, y);
//				SCR_AddDirtyPoint (x+23, y+23);
				p = Sbar_Q2CachePic(Get_Q2ConfigString(Q2CS_IMAGES+value));
				if (p && R_GetShaderSizes(p, &pw, &ph, false)>0)
					R2D_ScalePic (x, y, pw, ph, p);
			}
			continue;
		}

		if (!strcmp(com_token, "client"))
		{	// draw a deathmatch client block
			int		score, ping, time;

			s = COM_Parse (s);
			x = sbar_rect.x + sbar_rect.width/2 - 160 + atoi(com_token);
			s = COM_Parse (s);
			y = sbar_rect.y + sbar_rect.height/2 - 120 + atoi(com_token);
//			SCR_AddDirtyPoint (x, y);
//			SCR_AddDirtyPoint (x+159, y+31);

			s = COM_Parse (s);
			value = atoi(com_token);
			if (value >= MAX_CLIENTS || value < 0)
				Host_EndGame ("client >= MAX_CLIENTS");

			s = COM_Parse (s);
			score = atoi(com_token);

			s = COM_Parse (s);
			ping = atoi(com_token);

			s = COM_Parse (s);
			time = atoi(com_token);

			Draw_AltFunString (x+32, y, cl.players[value].name);
			Draw_FunString (x+32, y+8,  "Score: ");
			Draw_AltFunString (x+32+7*8, y+8,  va("%i", score));
			Draw_FunString (x+32, y+16, va("Ping:  %i", ping));
			Draw_FunString (x+32, y+24, va("Time:  %i", time));

			p = R2D_SafeCachePic(va("players/%s_i.pcx", Info_ValueForKey(cl.players[value].userinfo, "skin")));
			if (!p || !R_GetShaderSizes(p, NULL, NULL, false))	//display a default if the icon couldn't be found.
				p = R2D_SafeCachePic("players/male/grunt_i.pcx");
			R2D_ScalePic (x, y, 32, 32, p);
			continue;
		}

		if (!strcmp(com_token, "ctf"))
		{	// draw a ctf client block
			int		score, ping;
			char	block[80];

			s = COM_Parse (s);
			x = sbar_rect.x + sbar_rect.width/2 - 160 + atoi(com_token);
			s = COM_Parse (s);
			y = sbar_rect.y + sbar_rect.height/2 - 120 + atoi(com_token);
//			SCR_AddDirtyPoint (x, y);
//			SCR_AddDirtyPoint (x+159, y+31);

			s = COM_Parse (s);
			value = atoi(com_token);
			if (value >= MAX_CLIENTS || value < 0)
				Host_EndGame ("client >= MAX_CLIENTS");

			s = COM_Parse (s);
			score = atoi(com_token);

			s = COM_Parse (s);
			ping = atoi(com_token);
			if (ping > 999)
				ping = 999;

			sprintf(block, "%3d %3d %-12.12s", score, ping, cl.players[value].name);

//			if (value == cl.playernum)
//				Draw_Alt_String (x, y, block);
//			else
				Draw_FunString (x, y, block);
			continue;
		}

		if (!strcmp(com_token, "picn"))
		{	// draw a pic from a name
			s = COM_Parse (s);
//			SCR_AddDirtyPoint (x, y);
//			SCR_AddDirtyPoint (x+23, y+23);
			p = Sbar_Q2CachePic(com_token);
			if (p && R_GetShaderSizes(p, &pw, &ph, false)>0)
				R2D_ScalePic (x, y, pw, ph, p);
			continue;
		}

		if (!strcmp(com_token, "num"))
		{	// draw a number
			s = COM_Parse (s);
			width = atoi(com_token);
			s = COM_Parse (s);
			value = ps->stats[atoi(com_token)];
			SCR_DrawField (x, y, 0, width, value);
			continue;
		}

		if (!strcmp(com_token, "hnum"))
		{	// health number
			int		color;

			width = 3;
			value = ps->stats[Q2STAT_HEALTH];
			if (value > 25)
				color = 0;	// green
			else if (value > 0)
				color = (cl.q2frame.serverframe>>2) & 1;		// flash
			else
				color = 1;

			if (ps->stats[Q2STAT_FLASHES] & 1)
			{
				p = Sbar_Q2CachePic("field_3");
				if (p && R_GetShaderSizes(p, &pw, &ph, false)>0)
					R2D_ScalePic (x, y, pw, ph, p);
			}

			SCR_DrawField (x, y, color, width, value);
			continue;
		}

		if (!strcmp(com_token, "anum"))
		{	// ammo number
			int		color;

			width = 3;
			value = ps->stats[Q2STAT_AMMO];
			if (value > 5)
				color = 0;	// green
			else if (value >= 0)
				color = (cl.q2frame.serverframe>>2) & 1;		// flash
			else
				continue;	// negative number = don't show

			if (ps->stats[Q2STAT_FLASHES] & 4)
			{
				p = Sbar_Q2CachePic("field_3");
				if (p && R_GetShaderSizes(p, &pw, &ph, false)>0)
					R2D_ScalePic (x, y, pw, ph, p);
			}

			SCR_DrawField (x, y, color, width, value);
			continue;
		}

		if (!strcmp(com_token, "rnum"))
		{	// armor number
			int		color;

			width = 3;
			value = ps->stats[Q2STAT_ARMOR];
			if (value < 1)
				continue;

			color = 0;	// green

			if (ps->stats[Q2STAT_FLASHES] & 2)
				R2D_ScalePic (x, y, 64, 64, R2D_SafeCachePic("field_3"));

			SCR_DrawField (x, y, color, width, value);
			continue;
		}


		if (!strcmp(com_token, "stat_string"))
		{
			s = COM_Parse (s);
			index = atoi(com_token);
			if (index < 0 || index >= Q2MAX_CONFIGSTRINGS)
				Host_EndGame ("Bad stat_string index");
			index = ps->stats[index];
			if (index < 0 || index >= Q2MAX_CONFIGSTRINGS)
				Host_EndGame ("Bad stat_string index");
			Draw_FunString (x, y, Get_Q2ConfigString(index));
			continue;
		}

		if (!strcmp(com_token, "cstring"))
		{
			s = COM_Parse (s);
			DrawHUDString (com_token, x, y, 320, false);
			continue;
		}

		if (!strcmp(com_token, "string"))
		{
			s = COM_Parse (s);
			Draw_FunString (x, y, com_token);
			continue;
		}

		if (!strcmp(com_token, "cstring2"))
		{
			s = COM_Parse (s);
			DrawHUDString (com_token, x, y, 320, true);
			continue;
		}

		if (!strcmp(com_token, "string2"))
		{
			s = COM_Parse (s);
			Draw_AltFunString (x, y, com_token);
			continue;
		}

		if (!strcmp(com_token, "if"))
		{	// draw a number
			s = COM_Parse (s);
			value = ps->stats[atoi(com_token)];
			if (!value)
			{	// skip to endif
				while (s && strcmp(com_token, "endif") )
				{
					s = COM_Parse (s);
				}
			}

			continue;
		}


	}
}

static void Sbar_Q2DrawInventory(int seat)
{
	int keys[1], keymods[1];
	char cmd[1024];
	const char *boundkey;
	q2player_state_t *ps = &cl.q2frame.playerstate[seat];
	unsigned int validlist[Q2MAX_ITEMS], rows, i, item, selected = ps->stats[Q2STAT_SELECTED_ITEM];
	int first;
	unsigned int maxrows = ((240-24*2-8*2)/8);
	//draw background
	float x = sbar_rect.x + (sbar_rect.width - 256)/2;
	float y = sbar_rect.y + (sbar_rect.height - 240)/2;
	if (y < sbar_rect.y)
		y = sbar_rect.y;	//try to fix small-res 3-way splitscreen slightly
	R2D_ScalePic(x, y, 256, 240, Sbar_Q2CachePic("inventory"));
	//move into the frame
	x += 24;
	y += 24;

	//figure out which items we have
	for (i = 0, rows = 0, first = -1; i < Q2MAX_ITEMS; i++)
	{
		if (!cl.inventory[seat][i])
			continue;
		if (i <= selected)
			first = rows;
		validlist[rows++] = i;
	}
	first -= maxrows/2;
	first = min(first, (signed)(rows-maxrows));
	first = max(0, first);
	rows = min(rows, first+maxrows);

	//match q2, because why not.
	Draw_FunString(x, y, "hotkey ### item");y+=8;
	Draw_FunString(x, y, "------ --- ----");y+=8;
	for (i = first; i < rows; i++)
	{
		item = validlist[i];

		Q_snprintfz(cmd, sizeof(cmd), "use %s", Get_Q2ConfigString(Q2CS_ITEMS+item));
		if (!M_FindKeysForCommand(0, 0, cmd, keys, keymods, countof(keys)))
			boundkey = "";	//we don't actually know which ones can be selected at all.
		else
			boundkey = Key_KeynumToString(keys[0], keymods[0]);

		Q_snprintfz(cmd, sizeof(cmd), "%6s %3i %s", boundkey, cl.inventory[seat][item], Get_Q2ConfigString(Q2CS_ITEMS+item));
		Draw_FunStringWidth(x, y, cmd, 256-24*2+8, false, item != selected);	y+=8;
	}
}
#endif

/*
===============
Sbar_ShowTeamScores

Tab key down
===============
*/
void Sbar_ShowTeamScores (void)
{
	int seat = CL_TargettedSplit(false);
	if (cl.playerview[seat].sb_showteamscores)
		return;

#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		;//return;
#endif

	cl.playerview[seat].sb_showteamscores = true;
	sb_updates = 0;
}

/*
===============
Sbar_DontShowTeamScores

Tab key up
===============
*/
void Sbar_DontShowTeamScores (void)
{
	int seat = CL_TargettedSplit(false);
	cl.playerview[seat].sb_showteamscores = false;
	sb_updates = 0;

#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif
}

/*
===============
Sbar_ShowScores

Tab key down
===============
*/
void Sbar_ShowScores (void)
{
	int seat = CL_TargettedSplit(false);
	if (scr_scoreboard_teamscores.ival)
	{
		Sbar_ShowTeamScores();
		return;
	}

	if (cl.playerview[seat].sb_showscores)
		return;

#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		;//return;
#endif

	cl.playerview[seat].sb_showscores = true;
	sb_updates = 0;
}

#ifdef HEXEN2
static void Sbar_Hexen2InvLeft_f(void)
{
	int seat = CL_TargettedSplit(false);
#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif
	if (cls.protocol == CP_QUAKE2)
	{
		CL_SendClientCommand(true, "invprev");
	}
	else
	{
		int tries = 15;
		playerview_t *pv = &cl.playerview[seat];
		pv->sb_hexen2_item_time = realtime;
		while (tries-- > 0)
		{
			pv->sb_hexen2_cur_item--;
			if (pv->sb_hexen2_cur_item < 0)
				pv->sb_hexen2_cur_item = 14;

			if (pv->stats[STAT_H2_CNT_TORCH+pv->sb_hexen2_cur_item] > 0)
				break;
		}
	}
}
static void Sbar_Hexen2InvRight_f(void)
{
	int seat = CL_TargettedSplit(false);
#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif
	if (cls.protocol == CP_QUAKE2)
	{
		CL_SendClientCommand(true, "invnext");
	}
	else
	{
		int tries = 15;
		playerview_t *pv = &cl.playerview[seat];
		pv->sb_hexen2_item_time = realtime;
		while (tries-- > 0)
		{
			pv->sb_hexen2_cur_item++;
			if (pv->sb_hexen2_cur_item > 14)
				pv->sb_hexen2_cur_item = 0;

			if (pv->stats[STAT_H2_CNT_TORCH+pv->sb_hexen2_cur_item] > 0)
				break;
		}
	}
}
static void Sbar_Hexen2InvUse_f(void)
{
	int seat = CL_TargettedSplit(false);
#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif

	if (cls.protocol == CP_QUAKE2)
	{
		CL_SendClientCommand(true, "invuse");
	}
	else
	{
		playerview_t *pv = &cl.playerview[seat];
		Cmd_ExecuteString(va("impulse %d\n", 100+pv->sb_hexen2_cur_item), Cmd_ExecLevel);
	}
}
static void Sbar_Hexen2ShowInfo_f(void)
{
	int seat = CL_TargettedSplit(false);
	playerview_t *pv = &cl.playerview[seat];
#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif
	pv->sb_hexen2_extra_info = true;
}
static void Sbar_Hexen2DontShowInfo_f(void)
{
	int seat = CL_TargettedSplit(false);
	playerview_t *pv = &cl.playerview[seat];
#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif
	pv->sb_hexen2_extra_info = false;
}
static void Sbar_Hexen2PInfoPlaque_f(void)
{
	int seat = CL_TargettedSplit(false);
	playerview_t *pv = &cl.playerview[seat];
#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif
	pv->sb_hexen2_infoplaque = true;
}
static void Sbar_Hexen2MInfoPlaque_f(void)
{
	int seat = CL_TargettedSplit(false);
	playerview_t *pv = &cl.playerview[seat];
#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif
	pv->sb_hexen2_infoplaque = false;
}
#endif

/*
===============
Sbar_DontShowScores

Tab key up
===============
*/
void Sbar_DontShowScores (void)
{
	int seat = CL_TargettedSplit(false);
	if (scr_scoreboard_teamscores.ival)
	{
		Sbar_DontShowTeamScores();
		return;
	}

	cl.playerview[seat].sb_showscores = false;
	sb_updates = 0;

#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(seat, Cmd_Argv(0)))
		return;
#endif
}

/*
===============
Sbar_Changed
===============
*/
void Sbar_Changed (void)
{
	sb_updates = 0;	// update next frame
}

/*
===============
Sbar_Init
===============
*/

static qboolean sbar_loaded;

static apic_t *Sbar_PicFromWad(char *name)
{
	apic_t *ret;
	char savedname[MAX_QPATH];
	Q_strncpyz(savedname, name, sizeof(savedname));
	ret = R2D_LoadAtlasedPic(savedname);

	if (ret)
		return ret;

	return NULL;
}
void Sbar_Flush (void)
{
	sbar_loaded = false;
}
void Sbar_Start (void)	//if one of these fails, skip the entire status bar.
{
	int		i;
	size_t	lumpsize;
	qbyte	lumptype;
	if (sbar_loaded)
		return;

	sbar_loaded = true;

	COM_FlushFSCache(false, true);	//make sure the fs cache is built if needed. there's lots of loading here.

	if (!wad_base)	//the wad isn't loaded. This is an indication that it doesn't exist.
	{
		sbarfailed = true;
		return;
	}

	sbarfailed = false;

	for (i=0 ; i<10 ; i++)
	{
		sb_nums[0][i] = Sbar_PicFromWad (va("num_%i",i));
		sb_nums[1][i] = Sbar_PicFromWad (va("anum_%i",i));
	}

#ifdef HEXEN2
	sbar_hexen2 = false;
	if (W_GetLumpName("tinyfont", &lumpsize, &lumptype))
		sbar_hexen2 = true;
//	if (sb_nums[0][0] && sb_nums[0][0]->width < 13)
//		sbar_hexen2 = true;
#endif

	sb_nums[0][10] = Sbar_PicFromWad ("num_minus");
	sb_nums[1][10] = Sbar_PicFromWad ("anum_minus");

	sb_colon = Sbar_PicFromWad ("num_colon");
	sb_slash = Sbar_PicFromWad ("num_slash");

	sb_weapons[0][0] = Sbar_PicFromWad ("inv_shotgun");
	sb_weapons[0][1] = Sbar_PicFromWad ("inv_sshotgun");
	sb_weapons[0][2] = Sbar_PicFromWad ("inv_nailgun");
	sb_weapons[0][3] = Sbar_PicFromWad ("inv_snailgun");
	sb_weapons[0][4] = Sbar_PicFromWad ("inv_rlaunch");
	sb_weapons[0][5] = Sbar_PicFromWad ("inv_srlaunch");
	sb_weapons[0][6] = Sbar_PicFromWad ("inv_lightng");

	sb_weapons[1][0] = Sbar_PicFromWad ("inv2_shotgun");
	sb_weapons[1][1] = Sbar_PicFromWad ("inv2_sshotgun");
	sb_weapons[1][2] = Sbar_PicFromWad ("inv2_nailgun");
	sb_weapons[1][3] = Sbar_PicFromWad ("inv2_snailgun");
	sb_weapons[1][4] = Sbar_PicFromWad ("inv2_rlaunch");
	sb_weapons[1][5] = Sbar_PicFromWad ("inv2_srlaunch");
	sb_weapons[1][6] = Sbar_PicFromWad ("inv2_lightng");

	for (i=0 ; i<5 ; i++)
	{
		sb_weapons[2+i][0] = Sbar_PicFromWad (va("inva%i_shotgun",i+1));
		sb_weapons[2+i][1] = Sbar_PicFromWad (va("inva%i_sshotgun",i+1));
		sb_weapons[2+i][2] = Sbar_PicFromWad (va("inva%i_nailgun",i+1));
		sb_weapons[2+i][3] = Sbar_PicFromWad (va("inva%i_snailgun",i+1));
		sb_weapons[2+i][4] = Sbar_PicFromWad (va("inva%i_rlaunch",i+1));
		sb_weapons[2+i][5] = Sbar_PicFromWad (va("inva%i_srlaunch",i+1));
		sb_weapons[2+i][6] = Sbar_PicFromWad (va("inva%i_lightng",i+1));
	}

	sb_ammo[0] = Sbar_PicFromWad ("sb_shells");
	sb_ammo[1] = Sbar_PicFromWad ("sb_nails");
	sb_ammo[2] = Sbar_PicFromWad ("sb_rocket");
	sb_ammo[3] = Sbar_PicFromWad ("sb_cells");

	sb_armor[0] = Sbar_PicFromWad ("sb_armor1");
	sb_armor[1] = Sbar_PicFromWad ("sb_armor2");
	sb_armor[2] = Sbar_PicFromWad ("sb_armor3");

	sb_items[0] = Sbar_PicFromWad ("sb_key1");
	sb_items[1] = Sbar_PicFromWad ("sb_key2");
	sb_items[2] = Sbar_PicFromWad ("sb_invis");
	sb_items[3] = Sbar_PicFromWad ("sb_invuln");
	sb_items[4] = Sbar_PicFromWad ("sb_suit");
	sb_items[5] = Sbar_PicFromWad ("sb_quad");

	sb_sigil[0] = Sbar_PicFromWad ("sb_sigil1");
	sb_sigil[1] = Sbar_PicFromWad ("sb_sigil2");
	sb_sigil[2] = Sbar_PicFromWad ("sb_sigil3");
	sb_sigil[3] = Sbar_PicFromWad ("sb_sigil4");

	sb_faces[4][0] = Sbar_PicFromWad ("face1");
	sb_faces[4][1] = Sbar_PicFromWad ("face_p1");
	sb_faces[3][0] = Sbar_PicFromWad ("face2");
	sb_faces[3][1] = Sbar_PicFromWad ("face_p2");
	sb_faces[2][0] = Sbar_PicFromWad ("face3");
	sb_faces[2][1] = Sbar_PicFromWad ("face_p3");
	sb_faces[1][0] = Sbar_PicFromWad ("face4");
	sb_faces[1][1] = Sbar_PicFromWad ("face_p4");
	sb_faces[0][0] = Sbar_PicFromWad ("face5");
	sb_faces[0][1] = Sbar_PicFromWad ("face_p5");

	sb_face_invis = Sbar_PicFromWad ("face_invis");
	sb_face_invuln = Sbar_PicFromWad ("face_invul2");
	sb_face_invis_invuln = Sbar_PicFromWad ("face_inv2");
	sb_face_quad = Sbar_PicFromWad ("face_quad");

	sb_ibar = Sbar_PicFromWad ("ibar");
	sb_sbar = Sbar_PicFromWad ("sbar");
	sb_scorebar = Sbar_PicFromWad ("scorebar");

	//try to detect rogue wads, and thus the stats we will be getting from the server.
	sbar_rogue = COM_CheckParm("-rogue") || !!W_GetLumpName("r_lava", &lumpsize, &lumptype);
	if (sbar_rogue)
	{
		rsb_invbar[0] = Sbar_PicFromWad ("r_invbar1");
		rsb_invbar[1] = Sbar_PicFromWad ("r_invbar2");

		rsb_weapons[0] = Sbar_PicFromWad ("r_lava");
		rsb_weapons[1] = Sbar_PicFromWad ("r_superlava");
		rsb_weapons[2] = Sbar_PicFromWad ("r_gren");
		rsb_weapons[3] = Sbar_PicFromWad ("r_multirock");
		rsb_weapons[4] = Sbar_PicFromWad ("r_plasma");

		rsb_items[0] = Sbar_PicFromWad ("r_shield1");
		rsb_items[1] = Sbar_PicFromWad ("r_agrav1");

		rsb_teambord = Sbar_PicFromWad ("r_teambord");

		rsb_ammo[0] = Sbar_PicFromWad ("r_ammolava");
		rsb_ammo[1] = Sbar_PicFromWad ("r_ammomulti");
		rsb_ammo[2] = Sbar_PicFromWad ("r_ammoplasma");
	}

	sbar_hipnotic = COM_CheckParm("-hipnotic") || !!W_GetLumpName("inv_mjolnir", &lumpsize, &lumptype);
	if (sbar_hipnotic)
	{
		hsb_weapons[0][0] = Sbar_PicFromWad ("inv_laser");
		hsb_weapons[0][1] = Sbar_PicFromWad ("inv_mjolnir");
		hsb_weapons[0][2] = Sbar_PicFromWad ("inv_gren_prox");
		hsb_weapons[0][3] = Sbar_PicFromWad ("inv_prox_gren");
		hsb_weapons[0][4] = Sbar_PicFromWad ("inv_prox");
		hsb_weapons[1][0] = Sbar_PicFromWad ("inv2_laser");
		hsb_weapons[1][1] = Sbar_PicFromWad ("inv2_mjolnir");
		hsb_weapons[1][2] = Sbar_PicFromWad ("inv2_gren_prox");
		hsb_weapons[1][3] = Sbar_PicFromWad ("inv2_prox_gren");
		hsb_weapons[1][4] = Sbar_PicFromWad ("inv2_prox");
		for (i=0 ; i<5 ; i++)
		{
			hsb_weapons[2+i][0] = Sbar_PicFromWad (va("inva%i_laser",i+1));
			hsb_weapons[2+i][1] = Sbar_PicFromWad (va("inva%i_mjolnir",i+1));
			hsb_weapons[2+i][2] = Sbar_PicFromWad (va("inva%i_gren_prox",i+1));
			hsb_weapons[2+i][3] = Sbar_PicFromWad (va("inva%i_prox_gren",i+1));
			hsb_weapons[2+i][4] = Sbar_PicFromWad (va("inva%i_prox",i+1));
		}
		hsb_items[0] = Sbar_PicFromWad ("sb_wsuit");
		hsb_items[1] = Sbar_PicFromWad ("sb_eshld");
	}
}

void Sbar_Init (void)
{
	Cvar_Register(&scr_scoreboard_drawtitle, "Scoreboard settings");
	Cvar_Register(&scr_scoreboard_forcecolors, "Scoreboard settings");
	Cvar_Register(&scr_scoreboard_newstyle, "Scoreboard settings");
	Cvar_Register(&scr_scoreboard_showfrags, "Scoreboard settings");
	Cvar_Register(&scr_scoreboard_showflags, "Scoreboard settings");
	Cvar_Register(&scr_scoreboard_fillalpha, "Scoreboard settings");
	Cvar_Register(&scr_scoreboard_teamscores, "Scoreboard settings");
	Cvar_Register(&scr_scoreboard_teamsort, "Scoreboard settings");
	Cvar_Register(&scr_scoreboard_titleseperator, "Scoreboard settings");

	Cvar_Register(&sbar_teamstatus, "Status bar settings");
	Cvar_Register(&cl_sbaralpha, "Status bar settings");

	Cmd_AddCommand ("+showscores", Sbar_ShowScores);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores);

	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores);
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores);

#ifdef HEXEN2
	//stuff to get hexen2 working out-of-the-box
	Cmd_AddCommand ("invleft", Sbar_Hexen2InvLeft_f);
	Cmd_AddCommand ("invright", Sbar_Hexen2InvRight_f);
	Cmd_AddCommand ("invprev", Sbar_Hexen2InvLeft_f);
	Cmd_AddCommand ("invnext", Sbar_Hexen2InvRight_f);
	Cmd_AddCommand ("invuse", Sbar_Hexen2InvUse_f);
	Cmd_AddCommandD ("+showinfo", Sbar_Hexen2ShowInfo_f, "Hexen2 Compat");
	Cmd_AddCommandD ("-showinfo", Sbar_Hexen2DontShowInfo_f, "Hexen2 Compat");
	Cmd_AddCommandD ("+infoplaque", Sbar_Hexen2PInfoPlaque_f, "Hexen2 Compat");
	Cmd_AddCommandD ("-infoplaque", Sbar_Hexen2MInfoPlaque_f, "Hexen2 Compat");
	Cmd_AddCommandD ("+showdm", Sbar_ShowScores, "Hexen2 Compat");
	Cmd_AddCommandD ("-showdm", Sbar_DontShowScores, "Hexen2 Compat");
	Cbuf_AddText("alias +crouch \"impulse 22\"\n", RESTRICT_LOCAL);
	Cbuf_AddText("alias -crouch \"impulse 22\"\n", RESTRICT_LOCAL);
#endif
}


//=============================================================================

// drawing routines are reletive to the status bar location
/*
=============
Sbar_DrawPic
=============
*/
static void Sbar_DrawPic (float x, float y, float w, float h, apic_t *pic)
{
	R2D_ImageAtlas(sbar_rect.x + x /* + ((sbar_rect.width - 320)>>1) */, sbar_rect.y + y + (sbar_rect.height-SBAR_HEIGHT), w, h, 0, 0, 1, 1, pic);
}
static void Sbar_DrawMPic (float x, float y, float w, float h, mpic_t *pic)
{
	R2D_ScalePic(sbar_rect.x + x /* + ((sbar_rect.width - 320)>>1) */, sbar_rect.y + y + (sbar_rect.height-SBAR_HEIGHT), w, h, pic);
}

/*
=============
Sbar_DrawSubPic
=============
JACK: Draws a portion of the picture in the status bar.
*/

static void Sbar_DrawSubPic(float x, float y, float width, float height, apic_t *pic, int srcx, int srcy, int srcwidth, int srcheight)
{
	float newsl, newtl, newsh, newth;

	newsl = (srcx)/(float)srcwidth;
	newsh = newsl + (width)/(float)srcwidth;

	newtl = (srcy)/(float)srcheight;
	newth = newtl + (height)/(float)srcheight;

	R2D_ImageAtlas (sbar_rect.x + x, sbar_rect.y + y+(sbar_rect.height-SBAR_HEIGHT), width, height, newsl, newtl, newsh, newth, pic);
}

/*
================
Sbar_DrawCharacter

Draws one solid graphics character
================
*/
void Sbar_DrawCharacter (float x, float y, int num)
{
	int px, py;
	Font_BeginString(font_default, sbar_rect.x + x + 4, sbar_rect.y + y + sbar_rect.height-SBAR_HEIGHT, &px, &py);
	Font_DrawChar(px, py, CON_WHITEMASK, num | 0xe000);
	Font_EndString(font_default);
}

/*
================
Sbar_DrawString
================
*/
void Sbar_DrawString (float x, float y, char *str)
{
	Draw_FunString (sbar_rect.x + x /*+ ((sbar_rect.width - 320)>>1) */, sbar_rect.y + y+ sbar_rect.height-SBAR_HEIGHT, str);
}

void Sbar_DrawExpandedString (float x, float y, conchar_t *str)
{
	Draw_ExpandedString (sbar_rect.x + x /*+ ((sbar_rect.width - 320)>>1) */, sbar_rect.y + y+ sbar_rect.height-SBAR_HEIGHT, str);
}

void Draw_TinyString (float x, float y, const qbyte *str)
{
	float xstart;
	int px, py;
	unsigned int codepoint;
	int error;

	if (!font_tiny)
	{
		font_tiny = Font_LoadFont("gfx/tinyfont", 8);
		if (!font_tiny)
			return;
	}

	Font_BeginString(font_tiny, x, y, &px, &py);
	xstart = px;

	while (*str)
	{
		codepoint = unicode_decode(&error, str, (char**)&str, true);

		if (codepoint == '\n')
		{
			px = xstart;
			py += Font_CharHeight();
			str++;
			continue;
		}
		px = Font_DrawChar(px, py, CON_WHITEMASK, codepoint);
	}
	Font_EndString(font_tiny);
}
void Sbar_DrawTinyString (float x, float y, char *str)
{
	Draw_TinyString (sbar_rect.x + x /*+ ((sbar_rect.width - 320)>>1) */, sbar_rect.y + y+ sbar_rect.height-SBAR_HEIGHT, str);
}
void Sbar_DrawTinyStringf (float x, float y, char *fmt, ...)
{
	va_list		argptr;
	char		string[256];

	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string)-1, fmt, argptr);
	va_end (argptr);
	
	Draw_TinyString (sbar_rect.x + x /*+ ((sbar_rect.width - 320)>>1) */, sbar_rect.y + y+ sbar_rect.height-SBAR_HEIGHT, string);
}


void Sbar_FillPC (float x, float y, float w, float h, unsigned int pcolour)
{
	if (pcolour >= 16)
	{
		R2D_ImageColours (SRGBA(((pcolour&0xff0000)>>16)/255.0f, ((pcolour&0xff00)>>8)/255.0f, (pcolour&0xff)/255.0f, 1.0));
		R2D_FillBlock (x, y, w, h);
	}
	else
	{
		R2D_ImagePaletteColour(Sbar_ColorForMap(pcolour), 1.0);
		R2D_FillBlock (x, y, w, h);
	}
}
static void Sbar_FillPCDark (float x, float y, float w, float h, unsigned int pcolour, float alpha)
{
	if (pcolour >= 16)
	{
		R2D_ImageColours (SRGBA(((pcolour&0xff0000)>>16)/1024.0f, ((pcolour&0xff00)>>8)/1024.0f, (pcolour&0xff)/1024.0f, alpha));
		R2D_FillBlock (x, y, w, h);
	}
	else
	{
		R2D_ImagePaletteColour(Sbar_ColorForMap(pcolour)-1, alpha);
		R2D_FillBlock (x, y, w, h);
	}
}


/*
=============
Sbar_itoa
=============
*/
int Sbar_itoa (int num, char *buf)
{
	char	*str;
	int		pow10;
	int		dig;

	str = buf;

	if (num < 0)
	{
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10 ; num >= pow10 && pow10>=10; pow10 *= 10)
	;

	if (pow10 > 0)
	do
	{
		pow10 /= 10;
		dig = num/pow10;
		*str++ = '0'+dig;
		num -= dig*pow10;
	} while (pow10 != 1);

	*str = 0;

	return str-buf;
}


/*
=============
Sbar_DrawNum
=============
*/
void Sbar_DrawNum (float x, float y, int num, int digits, int color)
{
	char			str[16];
	char			*ptr;
	int				l, frame;
#undef small
	int small=false;

	if (digits < 0)
	{
		small = true;
		digits*=-1;
	}

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);

	if (small)
	{
		if (l < digits)
			x += (digits-l)*8;
		while (*ptr)
		{
			Sbar_DrawCharacter(x, y, *ptr+18 - '0');
			ptr++;
			x+=8;
		}
		return;
	}
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		Sbar_DrawPic (x, y, 24, 24, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

#ifdef HEXEN2
static void Sbar_Hexen2DrawNum (float x, float y, int num, int digits)
{
	char			str[12];
	char			*ptr;
	int				l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);

	//hexen2 hud has it centered
	if (l < digits)
		x += ((digits-l)*13)/2;

	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		Sbar_DrawPic (x, y, 12, 16, sb_nums[0][frame]);
		x += 13;
		ptr++;
	}
}
#endif

//=============================================================================

int		playerteam[MAX_CLIENTS];
typedef struct {
	char team[16+1];
	int frags;
	int players;
	int plow, phigh, ptotal;
	int topcolour, bottomcolour;
	qboolean ownteam;
} team_t;
team_t teams[MAX_CLIENTS];
int teamsort[MAX_CLIENTS];
int scoreboardteams;

struct
{
	unsigned char upper;
	short frags;
} nqteam[14];
void Sbar_PQ_Team_New(unsigned int lower, unsigned int upper)
{
	if (lower >= 14)
		return;
	nqteam[lower].upper = upper;
}
void Sbar_PQ_Team_Frags(unsigned int lower, int frags)
{
	if (lower >= 14)
		return;
	nqteam[lower].frags = frags;
}
void Sbar_PQ_Team_Reset(void)
{
	memset(nqteam, 0, sizeof(nqteam));
}


#endif

unsigned int	Sbar_ColorForMap (unsigned int m)
{
	if (m >= 16)
		return m;

	m = (m > 13) ? 13 : m;

	m *= 16;
	return m < 128 ? m + 8 : m + 8;
}

int		scoreboardlines;
int		fragsort[MAX_CLIENTS];
/*
===============
Sbar_SortFrags
===============
*/
void Sbar_SortFrags (qboolean includespec, qboolean doteamsort)
{
	int		i, j, k;

	if (!cl.teamplay)
		doteamsort = false;

// sort by frags
	scoreboardlines = 0;
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (cl.players[i].name[0] &&
			(!cl.players[i].spectator || includespec))
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i=0 ; i<scoreboardlines ; i++)
		for (j = i + 1; j < scoreboardlines; j++)
		{
			int w1, w2;
#ifdef QUAKEHUD
			int t1 = playerteam[fragsort[i]];
			int t2 = playerteam[fragsort[j]];

			//teams are already sorted by frags
			w1 = t1<0?-999:-teamsort[t1];
			w2 = t2<0?-999:-teamsort[t2];
			//okay, they're on the same team then? go ahead and sort by personal frags
			if (!doteamsort || w1 == w2)
#endif
			{
				w1 = cl.players[fragsort[i]].spectator==1?-999:cl.players[fragsort[i]].frags;
				w2 = cl.players[fragsort[j]].spectator==1?-999:cl.players[fragsort[j]].frags;
			}
			if (w1 < w2)
			{
				k = fragsort[i];
				fragsort[i] = fragsort[j];
				fragsort[j] = k;
			}
		}
}

#ifdef QUAKEHUD

void Sbar_SortTeams (playerview_t *pv)
{
	int				i, j, k;
	player_info_t	*s;
	char t[16+1];
	int ownnum;

// request new ping times every two second
	scoreboardteams = 0;

	if (!cl.teamplay)
		return;

	memset(teams, 0, sizeof(teams));
// sort the teams
	for (i = 0; i < cl.allocated_client_slots; i++)
		teams[i].plow = 999;

	ownnum = Sbar_PlayerNum(pv);

	for (i = 0; i < cl.allocated_client_slots; i++)
	{
		playerteam[i] = -1;
		s = &cl.players[i];
		if (!s->name[0] || s->spectator)
			continue;

		// find his team in the list
		Q_strncpyz(t, s->team, sizeof(t));
		if (!t[0])
			continue; // not on team
		if (cls.protocol == CP_NETQUAKE)
		{
			k = Sbar_BottomColour(s);
			if (!k)	//team 0 = spectator
				continue;
			for (j = 0; j < scoreboardteams; j++)
				if (teams[j].bottomcolour == k)
				{
					break;
				}
		}
		else
		{
			for (j = 0; j < scoreboardteams; j++)
				if (!strcmp(teams[j].team, t))
				{
					break;
				}
		}

		/*if (cl.teamfortress)
		{
			teams[j].topcolour = teams[j].bottomcolour = TF_TeamToColour(t);
		}
		else*/ if (j == scoreboardteams || i == ownnum)
		{
			teams[j].topcolour = Sbar_TopColour(s);
			teams[j].bottomcolour = Sbar_BottomColour(s);
		}

		if (j == scoreboardteams)
		{ // create a team for this player
			scoreboardteams++;
			strcpy(teams[j].team, t);
		}

		playerteam[i] = j;
		teams[j].frags += s->frags;
		teams[j].players++;

		if (teams[j].plow > s->ping)
			teams[j].plow = s->ping;
		if (teams[j].phigh < s->ping)
			teams[j].phigh = s->ping;
		teams[j].ptotal += s->ping;
	}

	// sort
	for (i = 0; i < scoreboardteams; i++)
		teamsort[i] = i;

	// good 'ol bubble sort
	for (i = 0; i < scoreboardteams - 1; i++)
		for (j = i + 1; j < scoreboardteams; j++)
			if (teams[teamsort[i]].frags < teams[teamsort[j]].frags)
			{
				k = teamsort[i];
				teamsort[i] = teamsort[j];
				teamsort[j] = k;
			}
}

/*
===============
Sbar_SoloScoreboard
===============
*/
void Sbar_SoloScoreboard (void)
{
	int l;
	float time;
	char	str[80];
	int		minutes, seconds, tens, units;

	Sbar_DrawPic (0, 0, 320, 24, sb_scorebar);

	// time
	time = cl.servertime;
	minutes = time / 60;
	seconds = time - 60*minutes;
	tens = seconds / 10;
	units = seconds - 10*tens;
	sprintf (str,"Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

	// draw level name
	l = strlen (cl.levelname);
	Sbar_DrawString (232 - l*4, 12, cl.levelname);
}

void Sbar_CoopScoreboard (void)
{
	float time;
	char	str[80];
	int		minutes, seconds, tens, units;
	int		l;
	int pnum = 0;	//doesn't matter, should all be the same

	sprintf (str,"Monsters:%3i /%3i", cl.playerview[pnum].stats[STAT_MONSTERS], cl.playerview[pnum].stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString (8, 4, str);

	sprintf (str,"Secrets :%3i /%3i", cl.playerview[pnum].stats[STAT_SECRETS], cl.playerview[pnum].stats[STAT_TOTALSECRETS]);
	Sbar_DrawString (8, 12, str);

// time
	time = cl.servertime;
	minutes = time / 60;
	seconds = time - 60*minutes;
	tens = seconds / 10;
	units = seconds - 10*tens;
	sprintf (str,"Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

// draw level name
	l = strlen (cl.levelname);
	Sbar_DrawString (232 - l*4, 12, cl.levelname);
}

//=============================================================================

/*
===============
Sbar_DrawInventory
===============
*/
void Sbar_DrawInventory (playerview_t *pv)
{
	int		i;
	char	  num[6];
	conchar_t numc[6];
	float	time;
	int		flashon;
	qboolean	headsup;
	qboolean	hudswap;
	float	wleft, wtop;
	apic_t *ibar;

	headsup = !(cl_sbar.value || (scr_viewsize.value<100));
	hudswap = cl_hudswap.value; // Get that nasty float out :)

	//coord for the left of the weapons, with hud
	wleft = hudswap?sbar_rect_left:(sbar_rect.width-24);
	wtop = -180;//68-(7-0)*16;
	if (sbar_hipnotic)
		wtop -= 16*2;

	if (sbar_rogue)
	{
		if ( pv->stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
			ibar = rsb_invbar[0];
		else
			ibar = rsb_invbar[1];
	}
	else
		ibar = sb_ibar;

	if (!headsup)
	{
		if (cl_sbar.ival != 1 && scr_viewsize.value >= 100)
			R2D_ImageColours (cl_sbaralpha.value, cl_sbaralpha.value, cl_sbaralpha.value, cl_sbaralpha.value);
		Sbar_DrawPic (0, -24, 320, 24, ibar);
		R2D_ImageColours (1, 1, 1, 1);
	}

// weapons
	for (i=0 ; i<7 ; i++)
	{
		if (pv->stats[STAT_ITEMS] & (IT_SHOTGUN<<i) )
		{
			time = pv->item_gettime[i];
			flashon = (int)((cl.time - time)*10);
			if (flashon < 0)
				flashon = 0;
			if (flashon >= 10)
			{
				if ( pv->stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN<<i)  )
					flashon = 1;
				else
					flashon = 0;
			}
			else
				flashon = (flashon%5) + 2;

			if (headsup)
			{
				if (i || sbar_rect.height>200)
					Sbar_DrawSubPic (wleft,wtop+i*16, 24,16, sb_weapons[flashon][i],0,0,(i==6)?(sbar_hipnotic?32:48):24, 16);
			}
			else
			{
				Sbar_DrawPic (i*24, -16, (i==6)?(sbar_hipnotic?32:48):24, 16, sb_weapons[flashon][i]);
			}

			if (flashon > 1)
				sb_updates = 0;		// force update to remove flash
		}
	}

	if (sbar_hipnotic)
	{
		int grenadeflashing=0;
		for (i=0 ; i<4 ; i++)
		{
			if (pv->stats[STAT_ITEMS] & (1<<hipweapons[i]))
			{
				time = pv->item_gettime[hipweapons[i]];
				flashon = (int)((cl.time - time)*10);
				if (flashon >= 10)
				{
					if (pv->stats[STAT_ACTIVEWEAPON] == (1<<hipweapons[i]))
						flashon = 1;
					else
						flashon = 0;
				}
				else
					flashon = (flashon%5) + 2;

				// check grenade launcher
				if (i==2)
				{
					if (pv->stats[STAT_ITEMS] & HIT_PROXIMITY_GUN)
					{
						if (flashon)
						{
							grenadeflashing = 1;
							Sbar_DrawPic (headsup?wleft:96, headsup?wtop+4*16:-16, 24, 16, hsb_weapons[flashon][2]);
						}
					}
				}
				else if (i==3)
				{
					if (pv->stats[STAT_ITEMS] & (IT_SHOTGUN<<4))
					{
						if (flashon && !grenadeflashing)
							Sbar_DrawPic (headsup?wleft:96, headsup?wtop+4*16:-16, 24, 16, hsb_weapons[flashon][3]);
						else if (!grenadeflashing)
							Sbar_DrawPic (headsup?wleft:96, headsup?wtop+4*16:-16, 24, 16, hsb_weapons[0][3]);
					}
					else
						Sbar_DrawPic (headsup?wleft:96, headsup?wtop+4*16:-16, 24, 16, hsb_weapons[flashon][4]);
				}
//				else if (i == 1)
//					Sbar_DrawPic (176 + (i*24), -16, 24, 16, hsb_weapons[flashon][i]);
				else
				{
					if (headsup)
						Sbar_DrawPic (headsup?wleft:(176 + (i*24)), headsup?wtop+(i+7)*16:-16, 24, 16, hsb_weapons[flashon][i]);
					else
						Sbar_DrawPic (headsup?wleft:(176 + (i*24)), headsup?wtop+(i+7)*16:-16, 24, 16, hsb_weapons[flashon][i]);
				}
				if (flashon > 1)
					sb_updates = 0;      // force update to remove flash
			}
		}
	}

	if (sbar_rogue)
	{
    // check for powered up weapon.
		if ( pv->stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
		{
			for (i=0;i<5;i++)
			{
				if (pv->stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i))
				{
					if (headsup)
					{
						if (sbar_rect.height>200)
							Sbar_DrawSubPic ((hudswap) ? 0 : (sbar_rect.width-24),-68-(5-i)*16, 24, 16, rsb_weapons[i],0,0,((i==4)?48:24),16);

					}
					else
						Sbar_DrawPic ((i+2)*24, -16, (i==4)?48:24, 16, rsb_weapons[i]);
				}
			}
		}
	}

	flashon = 0;
// items
	for (i=(sbar_hipnotic?2:0) ; i<6 ; i++)
	{
		if (pv->stats[STAT_ITEMS] & (1<<(17+i)))
		{
			time = pv->item_gettime[17+i];
			if (time &&	time > cl.time - 2 && flashon )
			{	// flash frame
				sb_updates = 0;
			}
			else
				Sbar_DrawPic (192 + i*16, -16, 16, 16, sb_items[i]);
			if (time &&	time > cl.time - 2)
				sb_updates = 0;
		}
	}

	if (sbar_hipnotic)
	{
		for (i=0 ; i<2 ; i++)
		{
			if (pv->stats[STAT_ITEMS] & (1<<(24+i)))
			{
				time = pv->item_gettime[24+i];
				if (time && time > cl.time - 2 && flashon )		// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (288 + i*16, -16, 16, 16, hsb_items[i]);
				if (time && time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}

	if (sbar_rogue)
	{
	// new rogue items
		for (i=0 ; i<2 ; i++)
		{
			if (pv->stats[STAT_ITEMS] & (1<<(29+i)))
			{
				time = pv->item_gettime[29+i];
				if (time &&	time > cl.time - 2 && flashon )	// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (288 + i*16, -16, 16, 16, rsb_items[i]);
				if (time &&	time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}
	else
	{
	// sigils
		for (i=0 ; i<4 ; i++)
		{
			if (pv->stats[STAT_ITEMS] & (1<<(28+i)))
			{
				time = pv->item_gettime[28+i];
				if (time &&	time > cl.time - 2 && flashon )
				{	// flash frame
					sb_updates = 0;
				}
				else
					Sbar_DrawPic (320-32 + i*8, -16, 8, 16, sb_sigil[i]);
				if (time &&	time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}

	// ammo counts
	if (headsup)
	{
		for (i=0 ; i<4 ; i++)
			Sbar_DrawSubPic((hudswap) ? sbar_rect_left : (sbar_rect.width-42), -24 - (4-i)*11, 42, 11, ibar, 3+(i*48), 0, 320, 24);
	}
	for (i=0 ; i<4 ; i++)
	{
		snprintf (num, sizeof(num), "%4i", pv->stats[STAT_SHELLS+i] );
		numc[0] = CON_WHITEMASK|0xe000|((num[0]!=' ')?(num[0] + 18-'0'):' ');
		numc[1] = CON_WHITEMASK|0xe000|((num[1]!=' ')?(num[1] + 18-'0'):' ');
		numc[2] = CON_WHITEMASK|0xe000|((num[2]!=' ')?(num[2] + 18-'0'):' ');
		numc[3] = CON_WHITEMASK|0xe000|((num[3]!=' ')?(num[3] + 18-'0'):' ');
		numc[4] = 0;
		if (headsup)
		{
			Sbar_DrawExpandedString(((hudswap) ? sbar_rect_left+3 : (sbar_rect.width-39)) - 4, -24 - (4-i)*11, numc);
		}
		else
		{
			Sbar_DrawExpandedString((6*i+1)*8 - 2 - 4, -24, numc);
		}
	}
}

static qboolean PointInBox(float px, float py, float x, float y, float w, float h)
{
	if (px >= x && px < x+w)
		if (py >= y && py < y+h)
			return true;
	return false;
}
int Sbar_TranslateHudClick(void)
{
	int i;
	float vx = mousecursor_x - sbar_rect.x;
	float vy = mousecursor_y - (sbar_rect.y + (sbar_rect.height-SBAR_HEIGHT));

	qboolean headsup = !(cl_sbar.value || (scr_viewsize.value<100));
	qboolean hudswap = cl_hudswap.value; // Get that nasty float out :)

	//inventory. clicks do specific-weapon impulses.
	if (sb_lines > 24)
	{
		for (i=0 ; i<7 ; i++)
		{
			if (headsup)
			{
				if (i || sbar_rect.height>200)
					if (PointInBox (vx, vy, (hudswap) ? 0 : (sbar_rect.width-24),-68-(7-i)*16, 24,16))
						return '2' + i;
			}
			else
			{
				if (PointInBox (vx, vy, i*24, -16, (i==6)?48:24, 16))
					return '2' + i;
			}
		}
	}

	//armour. trigger backtick, to toggle the console (which enables the on-screen keyboard on android).
	if (PointInBox (vx, vy, 0, 0, 96, 24))
		return '`';
	//face. do showscores.
	if (PointInBox (vx, vy, 112, 0, 96, 24))
		return K_TAB;
	//currentammo+icon. trigger '/' binding, which defaults to weapon-switch (impulse 10)
	if (PointInBox (vx, vy, 224, 0, 96, 24))
		return '/';

	return 0;
}

//=============================================================================

/*
===============
Sbar_DrawFrags
===============
*/
void Sbar_DrawFrags (playerview_t *pv)
{
	int				i, k, l;
	int				top, bottom;
	float			x, y;
	int				f;
	int				ownnum;
	char			num[12];
	player_info_t	*s;

	Sbar_SortFrags (false, false);

	ownnum = Sbar_PlayerNum(pv);

// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 23;
//	xofs = (sbar_rect.width - 320)>>1;
	y = sbar_rect.height - SBAR_HEIGHT - 23;

	for (i=0 ; i<l ; i++)
	{
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;
		if (s->spectator)
			continue;

	// draw background
		top = Sbar_TopColour(s);
		bottom = Sbar_BottomColour(s);


//		Draw_Fill (xofs + x*8 + 10, y, 28, 4, top);
//		Draw_Fill (xofs + x*8 + 10, y+4, 28, 3, bottom);
		Sbar_FillPC (sbar_rect.x+x*8 + 10, sbar_rect.y+y, 28, 4, top);
		Sbar_FillPC (sbar_rect.x+x*8 + 10, sbar_rect.y+y+4, 28, 3, bottom);

		R2D_ImageColours(1, 1, 1, 1);

	// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		Sbar_DrawCharacter ( (x+1)*8 , -24, num[0]);
		Sbar_DrawCharacter ( (x+2)*8 , -24, num[1]);
		Sbar_DrawCharacter ( (x+3)*8 , -24, num[2]);

		if (k == ownnum)
		{
			Sbar_DrawCharacter (x*8+2, -24, 16);
			Sbar_DrawCharacter ( (x+4)*8-4, -24, 17);
		}
		x+=4;
	}
	R2D_ImageColours(1.0, 1.0, 1.0, 1.0);
}

//=============================================================================


/*
===============
Sbar_DrawFace
===============
*/
void Sbar_DrawFace (playerview_t *pv)
{
	int		f, anim;

	if ( (pv->stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY) )
	== (IT_INVISIBILITY | IT_INVULNERABILITY) )
	{
		Sbar_DrawPic (112, 0, 24, 24, sb_face_invis_invuln);
		return;
	}
	if (pv->stats[STAT_ITEMS] & IT_QUAD)
	{
		Sbar_DrawPic (112, 0, 24, 24, sb_face_quad );
		return;
	}
	if (pv->stats[STAT_ITEMS] & IT_INVISIBILITY)
	{
		Sbar_DrawPic (112, 0, 24, 24, sb_face_invis );
		return;
	}
	if (pv->stats[STAT_ITEMS] & IT_INVULNERABILITY)
	{
		Sbar_DrawPic (112, 0, 24, 24, sb_face_invuln);
		return;
	}

	if (pv->stats[STAT_HEALTH] >= 100)
		f = 4;
	else
		f = pv->stats[STAT_HEALTH] / 20;

	if (f < 0)
		f=0;

	if (cl.time <= pv->faceanimtime)
	{
		anim = 1;
		sb_updates = 0;		// make sure the anim gets drawn over
	}
	else
		anim = 0;
	Sbar_DrawPic (112, 0, 24, 24, sb_faces[f][anim]);
}

/*
=============
Sbar_DrawNormal
=============
*/
void Sbar_DrawNormal (playerview_t *pv)
{
	if (cl_sbar.value || (scr_viewsize.value<100))
	{
		if (cl_sbar.ival != 1 && scr_viewsize.value >= 100)
			R2D_ImageColours (cl_sbaralpha.value, cl_sbaralpha.value, cl_sbaralpha.value, cl_sbaralpha.value);
		Sbar_DrawPic (0, 0, 320, 24, sb_sbar);
		R2D_ImageColours (1, 1, 1, 1);
	}

	//hipnotic's keys appear to the right of health.
	if (sbar_hipnotic)
	{
		if (pv->stats[STAT_ITEMS] & IT_KEY1)
			Sbar_DrawPic (209, 3, 16, 9, sb_items[0]);
		if (pv->stats[STAT_ITEMS] & IT_KEY2)
			Sbar_DrawPic (209, 12, 16, 9, sb_items[1]);
	}

// armor
	if (pv->stats[STAT_ITEMS] & IT_INVULNERABILITY)
	{
		Sbar_DrawNum (24, 0, 666, 3, 1);
		Sbar_DrawMPic (0, 0, 24, 24, draw_disc);
	}
	else
	{
		if (sbar_rogue)
		{
			Sbar_DrawNum (24, 0, pv->stats[STAT_ARMOR], 3,
				pv->stats[STAT_ARMOR] <= 25);
			if (pv->stats[STAT_ITEMS] & RIT_ARMOR3)
				Sbar_DrawPic (0, 0, 24, 24, sb_armor[2]);
			else if (pv->stats[STAT_ITEMS] & RIT_ARMOR2)
				Sbar_DrawPic (0, 0, 24, 24, sb_armor[1]);
			else if (pv->stats[STAT_ITEMS] & RIT_ARMOR1)
				Sbar_DrawPic (0, 0, 24, 24, sb_armor[0]);
		}
		else
		{
			Sbar_DrawNum (24, 0, pv->stats[STAT_ARMOR], 3,
				pv->stats[STAT_ARMOR] <= 25);
			if (pv->stats[STAT_ITEMS] & IT_ARMOR3)
				Sbar_DrawPic (0, 0, 24, 24, sb_armor[2]);
			else if (pv->stats[STAT_ITEMS] & IT_ARMOR2)
				Sbar_DrawPic (0, 0, 24, 24, sb_armor[1]);
			else if (pv->stats[STAT_ITEMS] & IT_ARMOR1)
				Sbar_DrawPic (0, 0, 24, 24, sb_armor[0]);
		}
	}

// face
	Sbar_DrawFace (pv);

// health
	Sbar_DrawNum (136, 0, pv->stats[STAT_HEALTH], 3
	, pv->stats[STAT_HEALTH] <= 25);

// ammo icon
	if (sbar_rogue)
	{
		if (pv->stats[STAT_ITEMS] & RIT_SHELLS)
			Sbar_DrawPic (224, 0, 24, 24, sb_ammo[0]);
		else if (pv->stats[STAT_ITEMS] & RIT_NAILS)
			Sbar_DrawPic (224, 0, 24, 24, sb_ammo[1]);
		else if (pv->stats[STAT_ITEMS] & RIT_ROCKETS)
			Sbar_DrawPic (224, 0, 24, 24, sb_ammo[2]);
		else if (pv->stats[STAT_ITEMS] & RIT_CELLS)
			Sbar_DrawPic (224, 0, 24, 24, sb_ammo[3]);
		else if (pv->stats[STAT_ITEMS] & RIT_LAVA_NAILS)
			Sbar_DrawPic (224, 0, 24, 24, rsb_ammo[0]);
		else if (pv->stats[STAT_ITEMS] & RIT_PLASMA_AMMO)
			Sbar_DrawPic (224, 0, 24, 24, rsb_ammo[1]);
		else if (pv->stats[STAT_ITEMS] & RIT_MULTI_ROCKETS)
			Sbar_DrawPic (224, 0, 24, 24, rsb_ammo[2]);
	}
	else
	{
		if (pv->stats[STAT_ITEMS] & IT_SHELLS)
			Sbar_DrawPic (224, 0, 24, 24, sb_ammo[0]);
		else if (pv->stats[STAT_ITEMS] & IT_NAILS)
			Sbar_DrawPic (224, 0, 24, 24, sb_ammo[1]);
		else if (pv->stats[STAT_ITEMS] & IT_ROCKETS)
			Sbar_DrawPic (224, 0, 24, 24, sb_ammo[2]);
		else if (pv->stats[STAT_ITEMS] & IT_CELLS)
			Sbar_DrawPic (224, 0, 24, 24, sb_ammo[3]);
	}

	Sbar_DrawNum (248, 0, pv->stats[STAT_AMMO], 3
	, pv->stats[STAT_AMMO] <= 10);
}

qboolean Sbar_ShouldDraw (playerview_t *pv)
{
#ifdef TEXTEDITOR
	extern qboolean editoractive;
#endif
	qboolean headsup;

	if (scr_con_current == vid.height)
		return false;		// console is full screen

#ifdef TEXTEDITOR
	if (editoractive)
		return false;
#endif

#ifdef VM_UI
	if (UI_DrawStatusBar((pv->sb_showscores?1:0) + (pv->sb_showteamscores?2:0))>0)
		return false;
	if (UI_MenuState())
		return false;
#endif

	headsup = !(cl_sbar.value || (scr_viewsize.value<100));
	if ((sb_updates >= vid.numpages) && !headsup)
		return false;

	return true;
}

void Sbar_DrawScoreboard (playerview_t *pv)
{
	qboolean isdead;

	if (cls.protocol == CP_QUAKE2)
		return;

	if (Key_Dest_Has(~kdm_game))
		return;

#ifdef CSQC_DAT
	if (CSQC_DrawScores(pv))
		return;
#endif

#ifndef CLIENTONLY
	/*no scoreboard in single player (if you want bots, set deathmatch)*/
	if (sv.state && sv.allocated_client_slots == 1)
	{
		return;
	}
#endif

	isdead = false;
	if (pv->spectator && (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV))
	{
		int t = pv->cam_spec_track;
		if (t >= 0 && CAM_ISLOCKED(pv) && cl.players[t].statsf[STAT_HEALTH] <= 0)
			isdead = true;
	}
	else if (!pv->spectator && pv->statsf[STAT_HEALTH] <= 0)
		isdead = true;

	if (isdead)// && !cl.spectator)
	{
		if (cl.teamplay > 0 && !pv->sb_showscores)
			Sbar_TeamOverlay(pv);
		else
			Sbar_DeathmatchOverlay (pv, 0);
	}
	else if (pv->sb_showscores)
		Sbar_DeathmatchOverlay (pv, 0);
	else if (pv->sb_showteamscores)
		Sbar_TeamOverlay(pv);
	else
		return;

	sb_updates = 0;
}


#ifdef HEXEN2
static void Sbar_Hexen2DrawActiveStuff(playerview_t *pv)
{
	int x = r_refdef.grect.x + r_refdef.grect.width;
	mpic_t *pic;
	if (pv->stats[STAT_H2_ARTIFACT_ACTIVE] & 4)
	{
		pic = R2D_SafeCachePic(va("gfx/pwrbook%d.lmp", ((int)(cl.time*16)%15)+1));
		x -= 32;
		R2D_ScalePic(x, r_refdef.grect.y, 32, 32, pic);
		x -= 18;
	}
	if (pv->stats[STAT_H2_ARTIFACT_ACTIVE] & 1)
	{
		pic = R2D_SafeCachePic(va("gfx/durhst%d.lmp", ((int)(cl.time*16)%15)+1));
		x -= 32;
		R2D_ScalePic(x, r_refdef.grect.y, 32, 32, pic);
		x -= 18;
	}
	if (pv->stats[STAT_H2_ARTIFACT_ACTIVE] & 2)
	{
		pic = R2D_SafeCachePic(va("gfx/durshd%d.lmp", ((int)(cl.time*16)%15)+1));
		x -= 32;
		R2D_ScalePic(x, r_refdef.grect.y, 32, 32, pic);
		x -= 18;
	}
}
static void Sbar_Hexen2DrawItem(playerview_t *pv, float x, float y, int itemnum)
{
	int num;
	Sbar_DrawMPic(x, y, 29, 28, R2D_SafeCachePic(va("gfx/arti%02d.lmp", itemnum)));

	num = pv->stats[STAT_H2_CNT_TORCH+itemnum];
	if(num > 0)
	{
		if (num >= 10)
			Sbar_DrawMPic(x+20, y+21, 4, 6, R2D_SafeCachePic(va("gfx/artinum%d.lmp", num/10)));
		Sbar_DrawMPic(x+20+4, y+21, 4, 6, R2D_SafeCachePic(va("gfx/artinum%d.lmp", num%10)));
	}
}

static void Sbar_Hexen2DrawInventory(playerview_t *pv)
{
	int i;
	int x, y=-37;
	int activeleft = 0;
	int activeright = 0;

	/*always select an artifact that we actually have whether we are drawing the full bar or not.*/
	for (i = 0; i < 15; i++)
	{
		if (pv->stats[STAT_H2_CNT_TORCH+(i+pv->sb_hexen2_cur_item)%15])
		{
			pv->sb_hexen2_cur_item = (pv->sb_hexen2_cur_item + i)%15;
			break;
		}
	}

	if (pv->sb_hexen2_item_time+3 < realtime)
		return;

	for (i = pv->sb_hexen2_cur_item; i < 15; i++)
		if (pv->sb_hexen2_cur_item == i || pv->stats[STAT_H2_CNT_TORCH+i] > 0)
			activeright++;
	for (i = pv->sb_hexen2_cur_item-1; i >= 0; i--)
		if (pv->sb_hexen2_cur_item == i || pv->stats[STAT_H2_CNT_TORCH+i] > 0)
			activeleft++;

	if (activeleft > 3 + (activeright<=3?(4-activeright):0))
		activeleft = 3 + (activeright<=3?(4-activeright):0);
	x=320/2-114 + (activeleft-1)*33;
	for (i = pv->sb_hexen2_cur_item-1; x>=320/2-114; i--)
	{
		if (!pv->stats[STAT_H2_CNT_TORCH+i])
			continue;

		if (i == pv->sb_hexen2_cur_item)
			Sbar_DrawMPic(x+9, y-12, 11, 11, R2D_SafeCachePic("gfx/artisel.lmp"));
		Sbar_Hexen2DrawItem(pv, x, y, i);
		x -= 33;
	}

	x=320/2-114 + activeleft*33;
	for (i = pv->sb_hexen2_cur_item; i < 15 && x < 320/2-114+7*33; i++)
	{
		if (i != pv->sb_hexen2_cur_item && !pv->stats[STAT_H2_CNT_TORCH+i])
			continue;
		if (i == pv->sb_hexen2_cur_item)
			Sbar_DrawMPic(x+9, y-12, 11, 11, R2D_SafeCachePic("gfx/artisel.lmp"));
		Sbar_Hexen2DrawItem(pv, x, y, i);
		x+=33;
	}
}

static void Sbar_Hexen2DrawExtra (playerview_t *pv)
{
	unsigned int i, slot;
	unsigned int pclass;
	int ringpos[] = {6, 44, 81, 119};
	//char *ringimages[] = {"gfx/ring_f.lmp", "gfx/ring_w.lmp", "gfx/ring_t.lmp", "gfx/ring_r.lmp"}; //unused variable
	float val;
	char *pclassname[] = {
		"Unknown",
		"Paladin",
		"Crusader",
		"Necromancer",
		"Assasin",
		"Demoness"
	};

	if (pv->sb_hexen2_infoplaque)
	{
		int i;
		Con_Printf("Objectives:\n");
		for (i = 0; i < 64; i++)
		{
			if (pv->stats[STAT_H2_OBJECTIVE1 + i/32] & (1<<(i&31)))
				Con_Printf("%s\n", T_GetInfoString(i));
		}
		pv->sb_hexen2_infoplaque = false;
	}

	if (!pv->sb_hexen2_extra_info)
	{
		sbar_rect.y -= 46-SBAR_HEIGHT;
		return;
	}

	pclass = cl.players[pv->playernum].h2playerclass;
	if (pclass >= sizeof(pclassname)/sizeof(pclassname[0]))
		pclass = 0;


	//adjust it so there's space
	sbar_rect.y -= 46+98-SBAR_HEIGHT;

	Sbar_DrawMPic(0, 46, 160, 98, R2D_SafeCachePic("gfx/btmbar1.lmp"));
	Sbar_DrawMPic(160, 46, 160, 98, R2D_SafeCachePic("gfx/btmbar2.lmp"));

	Sbar_DrawTinyString (11, 48, pclassname[pclass]);

	Sbar_DrawTinyString (11, 58, "int");
	Sbar_DrawTinyStringf (33, 58, "%02d", pv->stats[STAT_H2_INTELLIGENCE]);

	Sbar_DrawTinyString (11, 64, "wis");
	Sbar_DrawTinyStringf (33, 64, "%02d", pv->stats[STAT_H2_WISDOM]);

	Sbar_DrawTinyString (11, 70, "dex");
	Sbar_DrawTinyStringf (33, 70, "%02d", pv->stats[STAT_H2_DEXTERITY]);


	Sbar_DrawTinyString (58, 58, "str");
	Sbar_DrawTinyStringf (80, 58, "%02d", pv->stats[STAT_H2_STRENGTH]);

	Sbar_DrawTinyString (58, 64, "lvl");
	Sbar_DrawTinyStringf (80, 64, "%02d", pv->stats[STAT_H2_LEVEL]);

	Sbar_DrawTinyString (58, 70, "exp");
	Sbar_DrawTinyStringf (80, 70, "%06d", pv->stats[STAT_H2_EXPERIENCE]);

	Sbar_DrawTinyString (11, 79, "abilities");
	if (pv->stats[STAT_H2_FLAGS] & (1<<22))
		Sbar_DrawTinyString (8, 89, T_GetString(400 + 2*(pclass-1) + 0));
	if (pv->stats[STAT_H2_FLAGS] & (1<<23))
		Sbar_DrawTinyString (8, 96, T_GetString(400 + 2*(pclass-1) + 1));

	for (i = 0; i < 4; i++)
	{
		if (pv->stats[STAT_H2_ARMOUR1+i] > 0)
		{
			Sbar_DrawMPic (164+i*40, 115, 28, 19, R2D_SafeCachePic(va("gfx/armor%d.lmp", i+1)));
			Sbar_DrawTinyStringf (168+i*40, 136, "+%d", pv->stats[STAT_H2_ARMOUR1+i]);
		}
	}
	for (i = 0; i < 4; i++)
	{
		if (pv->stats[STAT_H2_FLIGHT_T+i] > 0)
		{
			Sbar_DrawMPic (ringpos[i], 119, 32, 22, R2D_SafeCachePic(va("gfx/ring_f.lmp")));
			val = pv->stats[STAT_H2_FLIGHT_T+i];
			if (val > 100)
				val = 100;
			if (val < 0)
				val = 0;
			Sbar_DrawMPic(ringpos[i]+29 - (int)(26 * (val/(float)100)),142, 26, 1, R2D_SafeCachePic("gfx/ringhlth.lmp"));
			Sbar_DrawMPic(ringpos[i]+29, 142, 26, 1, R2D_SafeCachePic("gfx/rhlthcvr.lmp"));
		}
	}

	slot = 0;
	for (i = 0; i < 8; i++)
	{
		if (pv->statsstr[STAT_H2_PUZZLE1+i] && *pv->statsstr[STAT_H2_PUZZLE1+i])
		{
			Sbar_DrawMPic (194+(slot%4)*31, slot<4?51:82, 26, 26, R2D_SafeCachePic(va("gfx/puzzle/%s.lmp", pv->statsstr[STAT_H2_PUZZLE1+i])));
			slot++;
		}
	}

	Sbar_DrawMPic(134, 50, 49, 56, R2D_SafeCachePic(va("gfx/cport%d.lmp", pclass)));
}

static int Sbar_Hexen2ArmourValue(playerview_t *pv)
{
	int i;
	float ac = 0;
	/*
	WARNING: these values match the engine - NOT the gamecode!
	Even the gamecode's values are misleading due to an indexing bug.
	*/
	static int acv[5][4] =
	{
		{8, 6, 2, 4},
		{4, 8, 6, 2},
		{2, 4, 8, 6},
		{6, 2, 4, 8},
		{6, 2, 4, 8}
	};

	int classno;
	classno = cl.players[pv->playernum].h2playerclass;
	if (classno >= 1 && classno <= 5)
	{
		classno--;
		for (i = 0; i < 4; i++)
		{
			if (pv->stats[STAT_H2_ARMOUR1+i])
			{
				ac += acv[classno][i];
				ac += pv->stats[STAT_H2_ARMOUR1+i]/5.0;
			}
		}
	}
	return ac;
}

static void Sbar_Hexen2DrawBasic(playerview_t *pv)
{
	int chainpos;
	int val, maxval;
	Sbar_DrawMPic(0, 0, 160, 46, R2D_SafeCachePic("gfx/topbar1.lmp"));
	Sbar_DrawMPic(160, 0, 160, 46, R2D_SafeCachePic("gfx/topbar2.lmp"));
	Sbar_DrawMPic(0, -23, 51, 23, R2D_SafeCachePic("gfx/topbumpl.lmp"));
	Sbar_DrawMPic(138, -8, 39, 8, R2D_SafeCachePic("gfx/topbumpm.lmp"));
	Sbar_DrawMPic(269, -23, 51, 23, R2D_SafeCachePic("gfx/topbumpr.lmp"));

	//mana1
	maxval = pv->stats[STAT_H2_MAXMANA];
	val = pv->stats[STAT_H2_BLUEMANA];
	val = bound(0, val, maxval);
	Sbar_DrawTinyStringf(201, 22, "%03d", val);
	if(val)
	{
		Sbar_DrawMPic(190, 26-(int)((val*18.0)/(float)maxval+0.5), 3, 19, R2D_SafeCachePic("gfx/bmana.lmp"));
		Sbar_DrawMPic(190, 27, 3, 19, R2D_SafeCachePic("gfx/bmanacov.lmp"));
	}

	//mana2
	maxval = pv->stats[STAT_H2_MAXMANA];
	val = pv->stats[STAT_H2_GREENMANA];
	val = bound(0, val, maxval);
	Sbar_DrawTinyStringf(243, 22, "%03d", val);
	if(val)
	{
		Sbar_DrawMPic(232, 26-(int)((val*18.0)/(float)maxval+0.5), 3, 19, R2D_SafeCachePic("gfx/gmana.lmp"));
		Sbar_DrawMPic(232, 27, 3, 19, R2D_SafeCachePic("gfx/gmanacov.lmp"));
	}


	//health
	val = pv->stats[STAT_HEALTH];
	if (val < -99)
		val = -99;
	Sbar_Hexen2DrawNum(58, 14, val, 3);

	//armour
	val = Sbar_Hexen2ArmourValue(pv);
	Sbar_Hexen2DrawNum(105, 14, val, 2);

//	SetChainPosition(cl.v.health, cl.v.max_health);
	chainpos = (195.0f*pv->stats[STAT_HEALTH]) / pv->stats[STAT_H2_MAXHEALTH];
	if (chainpos < 0)
		chainpos = 0;
	Sbar_DrawMPic(45+((int)chainpos&7), 38, 222, 5, R2D_SafeCachePic("gfx/hpchain.lmp"));
	Sbar_DrawMPic(45+(int)chainpos, 36,	35, 9, R2D_SafeCachePic("gfx/hpgem.lmp"));
	Sbar_DrawMPic(43, 36, 10, 10, R2D_SafeCachePic("gfx/chnlcov.lmp"));
	Sbar_DrawMPic(267, 36, 10, 10, R2D_SafeCachePic("gfx/chnrcov.lmp"));


	Sbar_Hexen2DrawItem(pv, 144, 3, pv->sb_hexen2_cur_item);
}

static void Sbar_Hexen2DrawMinimal(playerview_t *pv)
{
	int y;
	y = -16;
	Sbar_DrawMPic(3, y, 31, 17, R2D_SafeCachePic("gfx/bmmana.lmp"));
	Sbar_DrawMPic(3, y+18, 31, 17, R2D_SafeCachePic("gfx/gmmana.lmp"));

	Sbar_DrawTinyStringf(10, y+6, "%03d", pv->stats[STAT_H2_BLUEMANA]);
	Sbar_DrawTinyStringf(10, y+18+6, "%03d", pv->stats[STAT_H2_GREENMANA]);

	Sbar_Hexen2DrawNum(38, y+18, pv->stats[STAT_HEALTH], 3);
}
#endif

static void Sbar_DrawTeamStatus(playerview_t *pv)
{
	int p;
	int y;
	int track;

	if (!sbar_teamstatus.ival)
		return;
	y = -32;

	track = Cam_TrackNum(pv);
	if (track == -1 || !pv->spectator)
		track = pv->playernum;

	for (p = 0; p < cl.allocated_client_slots; p++)
	{
		if (pv->playernum == p)	//self is not shown
			continue;
		if (track == p)	//nor is the person you are tracking
			continue;

		if (cl.players[p].teamstatustime < realtime)
			continue;

		if (!*cl.players[p].teamstatus)	//only show them if they have something. no blank lines thanks
			continue;
		if (strcmp(cl.players[p].team, cl.players[track].team))
			continue;

		if (*cl.players[p].name)
		{
			Sbar_DrawString (0, y, cl.players[p].teamstatus);
			y-=8;
		}
	}
	sbar_parsingteamstatuses = true;
}

qboolean Sbar_UpdateTeamStatus(player_info_t *player, char *status)
{
	qboolean aswhite = false;
	char *outb;
	int outlen;
	char *msgstart;
	char *ledstatus;

	if (*status != '\r')// && !(strchr(status, 0x86) || strchr(status, 0x87) || strchr(status, 0x88) || strchr(status, 0x89)))
	{
		if (*status != 'x' || status[1] != '\r')
			return false;
		status++;
	}

	if (*status == '\r')
	{
		while (*status == ' ' || *status == '\r')
			status++;
		ledstatus = status;
		if (*(unsigned char*)ledstatus >= 0x86 && *(unsigned char*)ledstatus <= 0x89)
		{
			msgstart = strchr(status, ':');
			if (!status)
				return false;
			if (msgstart)
				status = msgstart+1;
			else
				ledstatus = NULL;
		}
		else
				ledstatus = NULL;
	}
	else
		ledstatus = NULL;

	while (*status == ' ' || *status == '\r')
		status++;

	//fixme: handle { and } stuff (assume red?)
	outb = player->teamstatus;
	outlen = sizeof(player->teamstatus)-1;
	if (ledstatus)
	{
		*outb++ = *ledstatus;
		outlen--;
	}

	while(outlen>0 && *status)
	{
		if (*status == '{')
		{
			aswhite=true;
			status++;
			continue;
		}
		if (aswhite)
		{
			if (*status == '}')
			{
				aswhite = false;
				status++;
				continue;
			}
			*outb++ = *status++;
		}
		else
			*outb++ = *status++|128;
		outlen--;
	}

	player->teamstatustime = realtime + 10;

	*outb = '\0';

	if (sbar_teamstatus.value == 2)
		return sbar_parsingteamstatuses;
	return false;
}


static void Sbar_Voice(int y)
{
#ifdef VOICECHAT
	int loudness;
	if (!snd_voip_showmeter.ival)
		return;
	loudness = S_Voip_Loudness(snd_voip_showmeter.ival==2);
	if (loudness >= 0)
	{
		int w;
		int x=0;
		int s, i;
		float range = loudness/100.0f;
		w = 0;
		Font_BeginString(font_default, sbar_rect.x + min(320,sbar_rect.width)/2, sbar_rect.y + y + sbar_rect.height-SBAR_HEIGHT, &x, &y);
		w += Font_CharWidth(CON_WHITEMASK, 0xe080);
		w += Font_CharWidth(CON_WHITEMASK, 0xe081)*16;
		w += Font_CharWidth(CON_WHITEMASK, 0xe082);
		w += Font_CharWidth(CON_WHITEMASK, 'M');
		w += Font_CharWidth(CON_WHITEMASK, 'i');
		w += Font_CharWidth(CON_WHITEMASK, 'c');
		w += Font_CharWidth(CON_WHITEMASK, ' ');
		x -= w/2;
		x = Font_DrawChar(x, y, CON_WHITEMASK, 'M');
		x = Font_DrawChar(x, y, CON_WHITEMASK, 'i');
		x = Font_DrawChar(x, y, CON_WHITEMASK, 'c');
		x = Font_DrawChar(x, y, CON_WHITEMASK, ' ');
		x = Font_DrawChar(x, y, CON_WHITEMASK, 0xe080);
		s = x;
		for (i=0 ; i<16 ; i++)
			x = Font_DrawChar(x, y, CON_WHITEMASK, 0xe081);
		Font_DrawChar(x, y, CON_WHITEMASK, 0xe082);
		Font_DrawChar(s + (x-s) * range - Font_CharWidth(CON_WHITEMASK, 0xe083)/2, y, CON_WHITEMASK, 0xe083);
		Font_EndString(font_default);
	}
#endif
}

void SCR_StringXY(char *str, float x, float y);
void SCR_DrawClock(void);
void SCR_DrawGameClock(void);
static void Sbar_DrawUPS(playerview_t *pv)
{
	extern cvar_t show_speed;
	static double lastupstime;
	double t;
	static float lastups;
	char str[80];
	float *vel;
	int track;
extern cvar_t	show_speed_x;
extern cvar_t	show_speed_y;

	if (!show_speed.ival)
		return;

	t = Sys_DoubleTime();
	if ((t - lastupstime) >= 1.0/20)
	{
		if (pv->spectator)
			track = Cam_TrackNum(pv);
		else
			track = -1;
		if (track != -1)
			vel = cl.inframes[cl.validsequence&UPDATE_MASK].playerstate[track].velocity;
		else
			vel = pv->simvel;
		lastups = sqrt((vel[0]*vel[0]) + (vel[1]*vel[1]));
		lastupstime = t;
	}

	sprintf(str, "%3.1f UPS", lastups);
	SCR_StringXY(str, show_speed_x.value, show_speed_y.value);
}

/*
===============
Sbar_Draw
===============
*/
void Sbar_Draw (playerview_t *pv)
{
	qboolean headsup;
	char st[512];
	int sbarwidth;
	qboolean minidmoverlay;
	extern cvar_t scr_centersbar;

#ifdef CSQC_DAT
	if (CSQC_DrawHud(pv))
		return;
#endif

	headsup = !(cl_sbar.value || (scr_viewsize.value<100));
	if ((sb_updates >= vid.numpages) && !headsup)
		return;

	sbar_parsingteamstatuses = false;

#ifdef HLCLIENT
	if (CLHL_DrawHud())
		return;
#endif

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
	{
		int seat = pv - cl.playerview;
		if (seat >= cl.splitclients)
			seat = cl.splitclients-1;
		if (seat < 0)
			seat = 0;
		sbar_rect = r_refdef.grect;
		R2D_ImageColours(1, 1, 1, 1);
		if (*cl.q2statusbar)
			Sbar_ExecuteLayoutString(cl.q2statusbar, seat);
		if (*cl.q2layout && (cl.q2frame.playerstate[seat].stats[Q2STAT_LAYOUTS] & 1))
			Sbar_ExecuteLayoutString(cl.q2layout[seat], seat);
		if (cl.q2frame.playerstate[seat].stats[Q2STAT_LAYOUTS] & 2)
			Sbar_Q2DrawInventory(seat);
		return;
	}
#endif

	Sbar_Start();

	R2D_ImageColours(1, 1, 1, 1);

	minidmoverlay = cl.deathmatch && hud_miniscores_show->ival;
	sbar_rect = r_refdef.grect;

	sbarwidth = 320;
	if (minidmoverlay && r_refdef.grect.width >= 640 && cl.teamplay)
		sbarwidth += 320;
	else if (minidmoverlay && r_refdef.grect.width >= 512)
		sbarwidth += 192;
	else
		minidmoverlay = 0;

	if (scr_centersbar.ival)
	{
		float ofs = (sbar_rect.width - sbarwidth)/2;
		sbar_rect.x += ofs;
		sbar_rect.width -= ofs;
		sbar_rect_left = -ofs;
	}

	sb_updates++;

	if (cl_sbar.value == 1 || scr_viewsize.value<100)
	{
		if (sbar_rect.x>r_refdef.grect.x)
		{	// left
			R2D_TileClear (r_refdef.grect.x, r_refdef.grect.y+sbar_rect.height - sb_lines, sbar_rect.x - r_refdef.grect.x, sb_lines);
		}
		if (sbar_rect.x + 320 <= r_refdef.grect.x + sbar_rect.width && !headsup)
			R2D_TileClear (sbar_rect.x + 320, r_refdef.grect.y+sbar_rect.height - sb_lines, sbar_rect.width - (320), sb_lines);
	}

#ifdef HEXEN2
	if (sbar_hexen2)
	{
		//hexen2 hud
		if (sb_lines > 24 || pv->sb_hexen2_extra_info)
		{
			Sbar_Hexen2DrawExtra(pv);
			Sbar_Hexen2DrawBasic(pv);
		}
		else if (sb_lines > 0)
			Sbar_Hexen2DrawMinimal(pv);
		Sbar_Hexen2DrawInventory(pv);

		if (minidmoverlay)
			Sbar_MiniDeathmatchOverlay (pv);

		Sbar_Hexen2DrawActiveStuff(pv);
	}
	else
#endif
		if (sbarfailed)	//files failed to load.
	{
		//fallback hud
		if (pv->stats[STAT_HEALTH] > 0)	//when dead, show nothing
		{
//			if (scr_viewsize.value != 120)
//				Cvar_Set(&scr_viewsize, "120");

			Sbar_DrawString (0, -8, va("Health: %i", pv->stats[STAT_HEALTH]));
			Sbar_DrawString (0, -16, va(" Armor: %i", pv->stats[STAT_ARMOR]));

			Sbar_Voice(-24);
		}
	}
	else
	{
	//standard quake(world) hud.
	// main area
		if (sb_lines > 0)
		{
			if (pv->spectator)
			{
				if (pv->cam_state == CAM_FREECAM || pv->cam_state == CAM_PENDING)
				{
					if (hud_tracking_show->ival || cl_sbar.ival)
					{	//this is annoying.
						Sbar_DrawPic (0, 0, 320, 24, sb_scorebar);
						Sbar_DrawString (160-7*8,4, "SPECTATOR MODE");
						if (pv->cam_state == CAM_FREECAM)
							Sbar_DrawString(160-14*8+4, 12, "Press [ATTACK] for AutoCamera");
					}
				}
				else
				{
					if (pv->sb_showscores || pv->sb_showteamscores || pv->stats[STAT_HEALTH] <= 0)
						Sbar_SoloScoreboard ();
//					else if (cls.gamemode != GAME_DEATHMATCH)
//						Sbar_CoopScoreboard ();
					else
						Sbar_DrawNormal (pv);

					if (hud_tracking_show->ival)
					{
						Q_snprintfz(st, sizeof(st), "Tracking %-.64s",
							cl.players[pv->cam_spec_track].name);
						Sbar_DrawString(0, -8, st);
					}
				}
			}
			else if (pv->sb_showscores || pv->sb_showteamscores || (pv->stats[STAT_HEALTH] <= 0 && cl.splitclients == 1))
			{
				if (pv == cl.playerview)
				{
					if (!cls.deathmatch)
					{
						if (cl_sbar.value)
							Sbar_DrawPic (0, 0, 320, 24, sb_scorebar);
						Sbar_CoopScoreboard ();
					}
					else
						Sbar_SoloScoreboard ();
				}
			}
			else
				Sbar_DrawNormal (pv);
		}

	// top line
		if (sb_lines > 24)
		{
			if (!pv->spectator || pv->cam_state == CAM_WALLCAM || pv->cam_state == CAM_EYECAM)
				Sbar_DrawInventory (pv);
			else if (cl_sbar.ival)
				Sbar_DrawPic (0, -24, 320, 24, sb_scorebar);	//make sure we don't get HoM
			if ((!headsup || sbar_rect.width<512) && cl.deathmatch && hud_miniscores_show->ival)
				Sbar_DrawFrags (pv);
		}

		if (minidmoverlay)
			Sbar_MiniDeathmatchOverlay (pv);

		if (sb_lines > 0)
			Sbar_DrawTeamStatus(pv);
		R2D_ImageColours (1, 1, 1, 1);
	}

	if (sb_lines > 24)
		Sbar_Voice(-32);
	else if (sb_lines > 0)
		Sbar_Voice(-8);
	else
		Sbar_Voice(16);

	Sbar_DrawUPS (pv);
	SCR_DrawClock();
	SCR_DrawGameClock();
}

//=============================================================================

/*
==================
Sbar_IntermissionNumber

==================
*/
void Sbar_IntermissionNumber (float x, float y, int num, int digits, int color, qboolean left)
{
	char			str[12];
	char			*ptr;
	int				l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (!left)
		if (l < digits)
			x += (digits-l)*24;

	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		R2D_ScalePicAtlas (x,y, 16, 24, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

#define COL_TEAM_LOWAVGHIGH	COLUMN("low/avg/high", 12*8, {sprintf (num, "%3i/%3i/%3i", plow, pavg, phigh); Draw_FunString ( x, y, num); })
#define COL_TEAM_TEAM		COLUMN("team", 4*8, 		{Draw_FunStringWidth ( x, y, tm->team, 4*8, false, false); \
		if (!strncmp(cl.players[trackplayer].team, tm->team, 16))\
		{\
			Draw_FunString ( x - 1*8, y, "^Ue010");\
			Draw_FunString ( x + 4*8, y, "^Ue011");\
		}\
	})
#define COL_TEAM_TOTAL		COLUMN("total", 5*8, 		{Draw_FunString ( x, y, va("%5i", tm->frags)); })
#define COL_TEAM_PLAYERS	COLUMN("players", 7*8,		{Draw_FunString ( x, y, va("%5i", tm->players)); })
#define ALL_TEAM_COLUMNS	COL_TEAM_LOWAVGHIGH COL_TEAM_TEAM COL_TEAM_TOTAL COL_TEAM_PLAYERS


/*
==================
Sbar_TeamOverlay

team frags
added by Zoid
==================
*/
void Sbar_TeamOverlay (playerview_t *pv)
{
	mpic_t			*pic;
	int				i, k;
	int				x, y, l;
	char			num[12];
	team_t *tm;
	int plow, phigh, pavg;
	int pw,ph;

	vrect_t		gr = r_refdef.grect;
	int rank_width = 320-32*2;
	int startx;
	int trackplayer;

	if (!pv)
		pv = &cl.playerview[0];

// request new ping times every two second
	if (!cl.teamplay)
	{
		Sbar_DeathmatchOverlay(pv, 0);
		return;
	}

	y = gr.y;

	if (scr_scoreboard_drawtitle.ival)
	{
		pic = R2D_SafeCachePic ("gfx/ranking.lmp");
		if (pic && R_GetShaderSizes(pic, &pw, &ph, false)>0)
		{
			k = (pw * 24) / ph;
			R2D_ScalePic (gr.x+(gr.width-k)/2, y, k, 24, pic);
		}
		y += 24;
	}

	x = l = gr.x + (gr.width - 320)/2 + 36;

	startx = x;

	if (scr_scoreboard_newstyle.ival)
	{
		y += 8;
		// Electro's scoreboard eyecandy: Draw top border
		R2D_ImagePaletteColour (0, scr_scoreboard_fillalpha.value);
		R2D_FillBlock(startx - 3, y - 1, rank_width - 1, 1);

		// Electro's scoreboard eyecandy: Draw the title row background
		R2D_ImagePaletteColour (1, scr_scoreboard_fillalpha.value);
		R2D_FillBlock(startx - 2, y, rank_width - 3, 9);
		R2D_ImageColours (1,1,1,1);
	}

#define COLUMN(title, cwidth, code) Draw_FunString(x, y, title), x+=cwidth + 8;
	ALL_TEAM_COLUMNS
//	if (rank_width+(cwidth)+8 <= vid.width) {showcolumns |= (1<<COLUMN##title); rank_width += cwidth+8;}

//	Draw_FunString(x, y, "low/avg/high");
//	Draw_FunString(x+13*8, y, "team");
//	Draw_FunString(x+18*8, y, "total");
//	Draw_FunString(x+24*8, y, "players");
	y += 8;
//	Draw_String(x, y, "------------ ---- ----- -------");
	x = l;
#undef COLUMN

	if (scr_scoreboard_newstyle.ival)
	{
		// Electro's scoreboard eyecandy: Draw top border (under header)
		R2D_ImagePaletteColour (0, scr_scoreboard_fillalpha.value);
		R2D_FillBlock (startx - 3, y + 1, rank_width - 1, 1);
		// Electro's scoreboard eyecandy: Don't go over the black border, move the rest down
		y += 2;
		// Electro's scoreboard eyecandy: Draw left border
		R2D_FillBlock (startx - 3, y - 10, 1, 9);
		// Electro's scoreboard eyecandy: Draw right border
		R2D_FillBlock (startx - 3 + rank_width - 2, y - 10, 1, 9);
	}
	else if (scr_scoreboard_titleseperator.ival)
	{
#define COLUMN(title, cwidth, code) {char buf[64*6]; int t = (cwidth)/8; int c=0; while (t-->0) {buf[c++] = '^'; buf[c++] = 'U'; buf[c++] = 'e'; buf[c++] = '0'; buf[c++] = '1'; buf[c] = (c==5?'d':(!t?'f':'e')); c++;} buf[c] = 0; Draw_FunString(x, y, buf); x += cwidth + 8;}
		ALL_TEAM_COLUMNS
//		Draw_FunString(x, y, "^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f ^Ue01d^Ue01e^Ue01e^Ue01f ^Ue01d^Ue01e^Ue01e^Ue01e^Ue01f ^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f");
		y += 8;

#undef COLUMN
	}

// sort the teams
	Sbar_SortTeams(pv);

	if (pv->spectator)
		trackplayer = Cam_TrackNum(pv);
	else
		trackplayer = pv->playernum;

// draw the text
	for (i=0 ; i < scoreboardteams && y <= vid.height-10 ; i++)
	{
		k = teamsort[i];
		tm = teams + k;

		if (scr_scoreboard_newstyle.ival)
		{
			// Electro's scoreboard eyecandy: Render the main background transparencies behind players row
			// TODO: Alpha values on the background
			int background_color;

			if (!(strcmp("red", tm->team)))
				background_color = 4; // forced red
			else if (!(strcmp("blue", tm->team)))
				background_color = 13; // forced blue
			else
				background_color = tm->bottomcolour;

			Sbar_FillPCDark (startx - 2, y, rank_width - 3, 8, background_color, scr_scoreboard_fillalpha.value);

			R2D_ImagePaletteColour (0, scr_scoreboard_fillalpha.value);
			R2D_FillBlock (startx - 3, y, 1, 8); // Electro - Border - Left
			R2D_FillBlock (startx - 3 + rank_width - 2, y, 1, 8); // Electro - Border - Right

			R2D_ImageColours(1, 1, 1, 1);
		}

	// draw pings
		plow = tm->plow;
		if (plow < 0 || plow > 999)
			plow = 999;
		phigh = tm->phigh;
		if (phigh < 0 || phigh > 999)
			phigh = 999;
		if (!tm->players)
			pavg = 999;
		else
			pavg = tm->ptotal / tm->players;
		if (pavg < 0 || pavg > 999)
			pavg = 999;

		x = l;

#if 1
#define COLUMN(title, cwidth, code) code; x+=cwidth + 8;
		ALL_TEAM_COLUMNS
#undef COLUMN
#else
		sprintf (num, "%3i/%3i/%3i", plow, pavg, phigh);
		Draw_FunString ( x, y, num);

	// draw team
		Q_strncpyz (team, tm->team, sizeof(team));
		Draw_FunString (x + 104, y, team);

	// draw total
		sprintf (num, "%5i", tm->frags);
		Draw_FunString (x + 104 + 40, y, num);

	// draw players
		sprintf (num, "%5i", tm->players);
		Draw_FunString (x + 104 + 88, y, num);

		if (!strncmp(cl.players[trackplayer].team, tm->team, 16))
		{
			Draw_FunString ( x + 104 - 8, y, "^Ue010");
			Draw_FunString ( x + 104 + 32, y, "^Ue011");
		}
#endif
		y += 8;
	}

	if (scr_scoreboard_newstyle.ival)
	{
		R2D_ImagePaletteColour (0, scr_scoreboard_fillalpha.value);
		R2D_FillBlock (startx - 3, y, rank_width - 1, 1); // Electro - Border - Bottom
	}
	else
		y += 8;
	Sbar_DeathmatchOverlay(pv, y-gr.y);
}

/*
==================
Sbar_DeathmatchOverlay

ping time frags name
==================
*/

#define NOFILL
//for reference:
//define COLUMN(title, width, code)

#define COLUMN_PING COLUMN(ping, 4*8,					\
{														\
	int p = s->ping;									\
	if (p < 0 || p > 999) p = 999;						\
	sprintf(num, "%4i", p);								\
	Draw_FunStringWidth(x, y, num, 4*8, false, false);	\
},NOFILL)

#define COLUMN_PL COLUMN(pl, 2*8,						\
{														\
	int p = s->pl;										\
	sprintf(num, "%2i", p);								\
	Draw_FunStringWidth(x, y, num, 2*8, false, false);	\
},NOFILL)
#define COLUMN_TIME COLUMN(time, 4*8,					\
{														\
	total = realtime - s->realentertime;				\
	minutes = (int)total/60;							\
	sprintf (num, "%4i", minutes);						\
	Draw_FunStringWidth(x, y, num, 4*8, false, false);	\
},NOFILL)
#define COLUMN_FRAGS COLUMN(frags, 5*8,					\
{	\
	int cx; int cy;										\
	if (s->spectator && s->spectator != 2)				\
	{													\
		Draw_FunStringWidth(x, y, "spectator", 5*8, false, false);	\
	}													\
	else												\
	{													\
		f = s->frags;									\
		sprintf(num, "%3i",f);							\
														\
		Font_BeginString(font_default, x+8, y, &cx, &cy);				\
		Font_DrawChar(cx, cy, CON_WHITEMASK, num[0] | 0xe000);			\
		Font_BeginString(font_default, x+16, y, &cx, &cy);				\
		Font_DrawChar(cx, cy, CON_WHITEMASK, num[1] | 0xe000);			\
		Font_BeginString(font_default, x+24, y, &cx, &cy);				\
		Font_DrawChar(cx, cy, CON_WHITEMASK, num[2] | 0xe000);			\
																		\
		if ((pv->cam_state == CAM_FREECAM && k == pv->playernum) ||		\
			(pv->cam_state != CAM_FREECAM && k == pv->cam_spec_track))	\
		{																\
			Font_BeginString(font_default, x, y, &cx, &cy);				\
			Font_DrawChar(cx, cy, CON_WHITEMASK, 16 | 0xe000);			\
			Font_BeginString(font_default, x+32, y, &cx, &cy);			\
			Font_DrawChar(cx, cy, CON_WHITEMASK, 17 | 0xe000);			\
		}												\
		Font_EndString(font_default);					\
	}													\
},{														\
	if (!s->spectator)									\
	{													\
		if (largegame)									\
			Sbar_FillPC(x, y+1, 40, 3, top);			\
		else											\
			Sbar_FillPC(x, y, 40, 4, top);				\
		Sbar_FillPC(x, y+4, 40, 4, bottom);				\
	}													\
})
#define COLUMN_TEAMNAME COLUMN(team, 4*8,				\
{														\
	if (!s->spectator)									\
	{													\
		Draw_FunStringWidth(x, y, s->team, 4*8, false, false);			\
	}													\
},NOFILL)
#define COLUMN_NAME COLUMN(name, namesize,	{Draw_FunStringWidth(x, y, s->name, namesize, false, false);},NOFILL)
#define COLUMN_KILLS COLUMN(kils, 4*8, {Draw_FunStringWidth(x, y, va("%4i", Stats_GetKills(k)), 4*8, false, false);},NOFILL)
#define COLUMN_TKILLS COLUMN(tkil, 4*8, {Draw_FunStringWidth(x, y, va("%4i", Stats_GetTKills(k)), 4*8, false, false);},NOFILL)
#define COLUMN_DEATHS COLUMN(dths, 4*8, {Draw_FunStringWidth(x, y, va("%4i", Stats_GetDeaths(k)), 4*8, false, false);},NOFILL)
#define COLUMN_TOUCHES COLUMN(tchs, 4*8, {Draw_FunStringWidth(x, y, va("%4i", Stats_GetTouches(k)), 4*8, false, false);},NOFILL)
#define COLUMN_CAPS COLUMN(caps, 4*8, {Draw_FunStringWidth(x, y, va("%4i", Stats_GetCaptures(k)), 4*8, false, false);},NOFILL)
#define COLUMN_AFK COLUMN(afk, 0, {int cs = atoi(Info_ValueForKey(s->userinfo, "chat")); if (cs)Draw_FunStringWidth(x+4, y, (cs&2)?"afk":"msg", 4*8, false, false);},NOFILL)


//columns are listed here in display order
#define ALLCOLUMNS COLUMN_PING COLUMN_PL COLUMN_TIME COLUMN_FRAGS COLUMN_TEAMNAME COLUMN_NAME COLUMN_KILLS COLUMN_TKILLS COLUMN_DEATHS COLUMN_TOUCHES COLUMN_CAPS COLUMN_AFK

enum
{
#define COLUMN(title, width, code, fill) COLUMN##title,
	ALLCOLUMNS
#undef COLUMN
	COLUMN_MAX
};

#define ADDCOLUMN(id) showcolumns |= (1<<id)
void Sbar_DeathmatchOverlay (playerview_t *pv, int start)
{
	mpic_t			*pic;
	int				i, k;
	int				x, y, f;
	char			num[12];
	player_info_t	*s;
	int				total;
	int				minutes;
	int				skip = 10;
	int showcolumns;
	int startx, rank_width;

	vrect_t		gr = r_refdef.grect;
	int namesize = (cl.teamplay ? 12*8 : 16*8);

	if (!pv)
		return;

	if (largegame)
		skip = 8;

// request new ping times every two second
	if (realtime - cl.last_ping_request > 2	&& cls.demoplayback != DPB_EZTV)
	{
		if (cls.protocol == CP_QUAKEWORLD)
		{
			cl.last_ping_request = realtime;
			CL_SendClientCommand(true, "pings");
		}
		else if (cls.protocol == CP_NETQUAKE)
		{
			cl.last_ping_request = realtime;
			CL_SendClientCommand(true, "ping");
		}
	}
	if (cls.protocol == CP_NETQUAKE)
	{
		if (cl.nqplayernamechanged && cl.nqplayernamechanged < realtime)
		{
			cl.nqplayernamechanged = 0;
			cls.nqexpectingstatusresponse = true;
			CL_SendClientCommand(true, "status");
		}
	}

	if (start)
		y = start;
	else
	{
		y = 0;
		if (scr_scoreboard_drawtitle.ival)
		{
			pic = R2D_SafeCachePic ("gfx/ranking.lmp");
			if (pic)
			{
				int w, h;
				if (R_GetShaderSizes(pic, &w, &h, false)>0)
				{
					k = (w * 24) / h;
					R2D_ScalePic (gr.x + (gr.width-k)/2, gr.y, k, 24, pic);
				}
			}
			y += 24;
		}
	}

// scores
	Sbar_SortFrags(true, scr_scoreboard_teamsort.ival);

// draw the text
	if (start)
		y = start;
	else
		y = 24;

	if (scr_scoreboard_newstyle.ival)
	{
		// Electro's scoreboard eyecandy: Increase to fit the new scoreboard
		y += 8;
	}

	y += gr.y;

	showcolumns = 0;

	rank_width = 0;

#define COLUMN(title, cwidth, code, fill) if (rank_width+(cwidth)+8 <= gr.width) {showcolumns |= (1<<COLUMN##title); rank_width += cwidth+8;}
//columns are listed here in priority order (if the screen is too narrow, later ones will be hidden)
	COLUMN_NAME
	COLUMN_PING
	if (cls.protocol == CP_QUAKEWORLD)
	{
		COLUMN_PL
		COLUMN_TIME
	}
	COLUMN_FRAGS
	if (cl.teamplay)
	{
		COLUMN_TEAMNAME
	}
	if (scr_scoreboard_showflags.ival && cl.teamplay && Stats_HaveFlags(scr_scoreboard_showflags.ival&1))
	{
		COLUMN_CAPS
	}
	if (scr_scoreboard_showfrags.ival && Stats_HaveKills())
	{
		COLUMN_KILLS
		COLUMN_DEATHS
		if (cl.teamplay)
		{
			COLUMN_TKILLS
		}
	}
	if (scr_scoreboard_showflags.ival && cl.teamplay && Stats_HaveFlags(scr_scoreboard_showflags.ival&1))
	{
		COLUMN_TOUCHES
	}
	COLUMN_AFK
#undef COLUMN

	rank_width -= namesize;
	if (rank_width < 320)
	{
		namesize += 320-rank_width;
		if (namesize > 32*8)
			namesize = 32*8;
	}
	rank_width += namesize;

	startx = (gr.width-rank_width)/2;
	startx += gr.x;

	if (scr_scoreboard_newstyle.ival)
	{
		// Electro's scoreboard eyecandy: Draw top border
		R2D_ImagePaletteColour (0, scr_scoreboard_fillalpha.value);
		R2D_FillBlock(startx - 3, y - 1, rank_width - 1, 1);

		// Electro's scoreboard eyecandy: Draw the title row background
		R2D_ImagePaletteColour (1, scr_scoreboard_fillalpha.value);
		R2D_FillBlock(startx - 2, y, rank_width - 3, 9);

		R2D_ImageColours(1, 1, 1, 1);
	}

	x = startx;
#define COLUMN(title, width, code, fill) if (width && (showcolumns & (1<<COLUMN##title))) {Draw_FunString(x, y, #title); x += width+8;}
	ALLCOLUMNS
#undef COLUMN


	y += 8;

	if (scr_scoreboard_titleseperator.ival && !scr_scoreboard_newstyle.ival)
	{
		x = startx;
#define COLUMN(title, width, code, fill) \
if (showcolumns & (1<<COLUMN##title)) \
{ \
	Draw_FunString(x, y, "^Ue01d"); \
	for (i = 8; i < width-8; i+= 8) \
		Draw_FunString(x+i, y, "^Ue01e"); \
	Draw_FunString(x+i, y, "^Ue01f"); \
	x += width+8; \
}
		ALLCOLUMNS
#undef COLUMN
		y += 8;
	}

	if (scr_scoreboard_newstyle.ival)
	{
		// Electro's scoreboard eyecandy: Draw top border (under header)
		R2D_ImagePaletteColour (0, scr_scoreboard_fillalpha.value);
		R2D_FillBlock (startx - 3, y + 1, rank_width - 1, 1);
		// Electro's scoreboard eyecandy: Don't go over the black border, move the rest down
		y += 2;
		// Electro's scoreboard eyecandy: Draw left border
		R2D_FillBlock (startx - 3, y - 10, 1, 9);
		// Electro's scoreboard eyecandy: Draw right border
		R2D_FillBlock (startx - 3 + rank_width - 2, y - 10, 1, 9);
	}

	y -= skip;

	//drawfills (these are split out to aid batching)
	for (i = 0; i < scoreboardlines; i++)
	{
		char	team[5];
		unsigned int		top, bottom;

		// TODO: Sort players so that the leading teams are drawn first
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		y += skip;
		if (y > vid.height-10)
			break;

		// Electro's scoreboard eyecandy: Moved this up here for usage with the row background color
		top = Sbar_TopColour(s);
		bottom = Sbar_BottomColour(s);

		if (scr_scoreboard_newstyle.ival)
		{
			// Electro's scoreboard eyecandy: Render the main background transparencies behind players row
			// TODO: Alpha values on the background
			if ((cl.teamplay) && (!s->spectator))
			{
				int background_color;
				// Electro's scoreboard eyecandy: red vs blue are common teams, force the colours
				Q_strncpyz (team, Info_ValueForKey(s->userinfo, "team"), sizeof(team));

				if (S_Voip_Speaking(k))
					background_color = 0x00ff00;
				else if (!(strcmp("red", team)))
					background_color = 4; // forced red
				else if (!(strcmp("blue", team)))
					background_color = 13; // forced blue
				else
					background_color = bottom;

				Sbar_FillPCDark (startx - 2, y, rank_width - 3, skip, background_color, scr_scoreboard_fillalpha.value);
			}
			else if (S_Voip_Speaking(k))
				Sbar_FillPCDark (startx - 2, y, rank_width - 3, skip, 0x00ff00, scr_scoreboard_fillalpha.value);
			else
			{
				R2D_ImagePaletteColour (2, scr_scoreboard_fillalpha.value);
				R2D_FillBlock (startx - 2, y, rank_width - 3, skip);
			}

			R2D_ImagePaletteColour (0, scr_scoreboard_fillalpha.value);
			R2D_FillBlock (startx - 3, y, 1, skip); // Electro - Border - Left
			R2D_FillBlock (startx - 3 + rank_width - 2, y, 1, skip); // Electro - Border - Right
		}

		x = startx;
#define COLUMN(title, width, code, fills) \
if (showcolumns & (1<<COLUMN##title)) \
{ \
	fills \
	x += width+8; \
}
		ALLCOLUMNS
#undef COLUMN
	}
	if (scr_scoreboard_newstyle.ival)
	{
		R2D_ImagePaletteColour (0, scr_scoreboard_fillalpha.value);
		R2D_FillBlock (startx - 3, y + skip, rank_width - 1, 1); // Electro - Border - Bottom
	}
	R2D_ImageColours(1.0, 1.0, 1.0, 1.0);
	y -= i * skip;

	//text parts
	for (i = 0; i < scoreboardlines; i++)
	{
		// TODO: Sort players so that the leading teams are drawn first
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		y += skip;
		if (y > vid.height-10)
			break;

		x = startx;
#define COLUMN(title, width, code, fills) \
if (showcolumns & (1<<COLUMN##title)) \
{ \
	code \
	x += width+8; \
}
		ALLCOLUMNS
#undef COLUMN
	}

	if (y >= vid.height-10) // we ran over the screen size, squish
		largegame = true;
}

/*
==================
Sbar_MiniDeathmatchOverlay

frags name
frags team name
displayed to right of status bar if there's room
==================
*/
static void Sbar_MiniDeathmatchOverlay (playerview_t *pv)
{
	int				i, k;
	int				top, bottom;
	int				x, y, f, px, py;
	char			num[12];
	player_info_t	*s;
	int				numlines;
	char			name[64+1];
	team_t			*tm;

// scores
	Sbar_SortFrags (false, false);
	if (sbar_rect.width >= 640)
		Sbar_SortTeams(pv);

	if (!scoreboardlines)
		return; // no one there?

// draw the text
	y = sbar_rect.y + sbar_rect.height - sb_lines - 1;
	numlines = sb_lines/8;
	if (numlines < 3)
		return; // not enough room

	// find us
	for (i=0 ; i < scoreboardlines; i++)
		if (fragsort[i] == pv->playernum)
			break;

	if (i == scoreboardlines) // we're not there, we are probably a spectator, just display top
		i = 0;
	else // figure out start
		i = i - numlines/2;

	if (i > scoreboardlines - numlines)
		i = scoreboardlines - numlines;
	if (i < 0)
		i = 0;

	x = sbar_rect.x + 320 + 4;

	for (f = i, py = y; f < scoreboardlines && py < sbar_rect.y + sbar_rect.height - 8 + 1; f++)
	{
		k = fragsort[f];
		s = &cl.players[k];
		if (!s->name[0])
			continue;
	// draw ping
		top = Sbar_TopColour(s);
		bottom = Sbar_BottomColour(s);

		Sbar_FillPC ( x, py+1, 40, 3, top);
		Sbar_FillPC ( x, py+4, 40, 4, bottom);
		py += 8;
	}
	R2D_ImageColours(1, 1, 1, 1);
	for (/* */ ; i < scoreboardlines && y < sbar_rect.y + sbar_rect.height - 8 + 1; i++)
	{
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

	// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		Font_BeginString(font_default, x+8, y, &px, &py);
		Font_DrawChar ( px, py, CON_WHITEMASK, num[0] | 0xe000);
		Font_BeginString(font_default, x+16, y, &px, &py);
		Font_DrawChar ( px, py, CON_WHITEMASK, num[1] | 0xe000);
		Font_BeginString(font_default, x+24, y, &px, &py);
		Font_DrawChar ( px, py, CON_WHITEMASK, num[2] | 0xe000);

		if ((pv->spectator && k == pv->cam_spec_track && pv->cam_state != CAM_FREECAM) ||
			(!pv->spectator && k == pv->playernum))
		{
			Font_BeginString(font_default, x, y, &px, &py);
			Font_DrawChar ( px, py, CON_WHITEMASK, 16 | 0xe000);
			Font_BeginString(font_default, x+32, y, &px, &py);
			Font_DrawChar ( px, py, CON_WHITEMASK, 17 | 0xe000);
		}

		Q_strncpyz(name, s->name, sizeof(name));
	// team and name
		if (cl.teamplay)
		{
			Draw_FunStringWidth (x+48, y, s->team, 32, false, false);
			Draw_FunStringWidth (x+48+40, y, name, MAX_DISPLAYEDNAME*8, false, false);
		}
		else
			Draw_FunStringWidth (x+48, y, name, MAX_DISPLAYEDNAME*8, false, false);
		y += 8;
	}

	// draw teams if room
	if (sbar_rect.width < 640 || !cl.teamplay)
		return;

	// draw seperator
	x += 208;
//	for (y = sbar_rect.height - sb_lines; y < sbar_rect.height - 6; y += 2)
//		Draw_ColouredCharacter(x, y, CON_WHITEMASK|14);

	x += 16;

	y = sbar_rect.height - sb_lines;
	for (i=0 ; i < scoreboardteams && y <= sbar_rect.height; i++)
	{
		k = teamsort[i];
		tm = teams + k;

	// draw pings
		Draw_FunStringWidth (x, y, tm->team, 32, false, false);

	// draw total
		sprintf (num, "%5i", tm->frags);
		Draw_FunString(x + 40, y, num);

		if (!strncmp(cl.players[pv->playernum].team, tm->team, 16))
		{
			Font_BeginString(font_default, x-8, y, &px, &py);
			Font_DrawChar(px, py, CON_WHITEMASK, 16|0xe000);
			Font_BeginString(font_default, x+32, y, &px, &py);
			Font_DrawChar(px, py, CON_WHITEMASK, 17|0xe000);
			Font_EndString(font_default);
		}

		y += 8;
	}

}

void Sbar_CoopIntermission (playerview_t *pv)
{
	mpic_t	*pic;
	int		dig;
	int		num;

	sbar_rect.width = vid.width;
	sbar_rect.height = vid.height;
	sbar_rect.x = 0;
	sbar_rect.y = 0;

	pic = R2D_SafeCachePic ("gfx/complete.lmp");
	if (!pic)
		return;
	R2D_ScalePic ((sbar_rect.width - 320)/2 + 64, (sbar_rect.height - 200)/2 + 24, 192, 24, pic);

	pic = R2D_SafeCachePic ("gfx/inter.lmp");
	if (pic)
		R2D_ScalePic ((sbar_rect.width - 320)/2 + 0, (sbar_rect.height - 200)/2 + 56, 160, 144, pic);

// time
	dig = cl.completed_time/60;
	Sbar_IntermissionNumber ((sbar_rect.width - 320)/2 + 230 - 24*4, (sbar_rect.height - 200)/2 + 64, dig, 4, 0, false);
	num = cl.completed_time - dig*60;
	R2D_ScalePicAtlas ((sbar_rect.width - 320)/2 + 230,(sbar_rect.height - 200)/2 + 64, 16, 24, sb_colon);
	R2D_ScalePicAtlas ((sbar_rect.width - 320)/2 + 254,(sbar_rect.height - 200)/2 + 64, 16, 26, sb_nums[0][num/10]);
	R2D_ScalePicAtlas ((sbar_rect.width - 320)/2 + 278,(sbar_rect.height - 200)/2 + 64, 16, 24, sb_nums[0][num%10]);

//it is assumed that secrits/monsters are going to be constant for any player...
	Sbar_IntermissionNumber ((sbar_rect.width - 320)/2 + 230 - 24*4, (sbar_rect.height - 200)/2 + 104, pv->stats[STAT_SECRETS], 4, 0, false);
	R2D_ScalePicAtlas ((sbar_rect.width - 320)/2 + 230, (sbar_rect.height - 200)/2 + 104, 16, 24, sb_slash);
	Sbar_IntermissionNumber ((sbar_rect.width - 320)/2 + 254, (sbar_rect.height - 200)/2 + 104, pv->stats[STAT_TOTALSECRETS], 4, 0, true);

	Sbar_IntermissionNumber ((sbar_rect.width - 320)/2 + 230 - 24*4, (sbar_rect.height - 200)/2 + 144, pv->stats[STAT_MONSTERS], 4, 0, false);
	R2D_ScalePicAtlas ((sbar_rect.width - 320)/2 + 230,(sbar_rect.height - 200)/2 + 144, 16, 24, sb_slash);
	Sbar_IntermissionNumber ((sbar_rect.width - 320)/2 + 254, (sbar_rect.height - 200)/2 + 144, pv->stats[STAT_TOTALMONSTERS], 4, 0, true);
}
/*
==================
Sbar_IntermissionOverlay

==================
*/
void Sbar_IntermissionOverlay (playerview_t *pv)
{
#ifdef VM_UI
	if (UI_DrawIntermission()>0)
		return;
#endif

	Sbar_Start();

	if (!cls.deathmatch)
		Sbar_CoopIntermission(pv);
	else if (cl.teamplay > 0 && !pv->sb_showscores)
		Sbar_TeamOverlay (pv);
	else
		Sbar_DeathmatchOverlay (pv, 0);
}
#endif
