/*
	teamplay.c

	Teamplay enhancements

	Copyright (C) 2000-2001       Anton Gavrilov

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA
*/

//Hacked by spike.
//things to fix:
//TP_SearchForMsgTriggers: should only allow safe commands. work out what the meaning of safe commands is.


#include "bothdefs.h"
#if 1 //def ZQUAKETEAMPLAY

#include "quakedef.h"
//#include "version.h"
#include "sound.h"
//#include "pmove.h"
#include <time.h>
#include <ctype.h>

typedef qboolean qbool;

#define SP 0

#define teamfortress spectator
#define framecount qport
#define Com_Printf Con_Printf

#define strlcpy Q_strncpyz
#define Q_stricmp stricmp
#define Q_strnicmp strnicmp

/*#define isalpha(x) (((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z'))
#define isdigit(x) ((x) >= '0' && (x) <= '9')
#define isxdigit(x) (isdigit(x) || ((x) >= 'a' && (x) <= 'f'))
*/
#define Q_rint(f) ((int)((f)+0.5))

void Cmd_AddMacro(char *s, char *(*f)(void));

#ifndef HAVE_STRLCAT
size_t strlcat (char *dst, const char *src, size_t size)
{
	int dstlen = strlen(dst);
	int srclen = strlen(src);
	int len = dstlen + srclen;

	if (len < size)
	{
		// it'll fit
		memcpy (dst + dstlen, src, srclen + 1);
		return len;
	}

	if (dstlen >= size - 1)
		return srclen + size;

	if (size == 0)
		return srclen;

	memcpy (dst + dstlen, src, size - 1 - dstlen);
	dst[size - 1] = 0;

	return len;
}
#endif

void Q_snprintfz (char *dest, size_t size, char *fmt, ...)
{
	va_list		argptr;

	va_start (argptr, fmt);
#ifdef _WIN32
	_vsnprintf (dest, size, fmt, argptr);
#else
	vsnprintf (dest, size, fmt, argptr);
#endif
	va_end (argptr);

	dest[size-1] = 0;
}

cvar_t	cl_fakename = {"cl_fakename", ""};


cvar_t	cl_parseSay = {"cl_parseSay", "1"};
cvar_t	cl_parseFunChars = {"cl_parseFunChars", "1"};
cvar_t	cl_triggers = {"cl_triggers", "0"};
cvar_t	tp_forceTriggers = {"tp_forceTriggers", "0"};
cvar_t	tp_loadlocs = {"tp_loadlocs", "1"};

cvar_t	cl_teamskin = {"teamskin", ""};
cvar_t	cl_enemyskin = {"enemyskin", ""};

cvar_t  tp_soundtrigger = {"tp_soundtrigger", "~"};

cvar_t	tp_name_axe = {"tp_name_axe", "axe"};
cvar_t	tp_name_sg = {"tp_name_sg", "sg"};
cvar_t	tp_name_ssg = {"tp_name_ssg", "ssg"};
cvar_t	tp_name_ng = {"tp_name_ng", "ng"};
cvar_t	tp_name_sng = {"tp_name_sng", "sng"};
cvar_t	tp_name_gl = {"tp_name_gl", "gl"};
cvar_t	tp_name_rl = {"tp_name_rl", "rl"};
cvar_t	tp_name_lg = {"tp_name_lg", "lg"};
cvar_t	tp_name_ra = {"tp_name_ra", "ra"};
cvar_t	tp_name_ya = {"tp_name_ya", "ya"};
cvar_t	tp_name_ga = {"tp_name_ga", "ga"};
cvar_t	tp_name_quad = {"tp_name_quad", "quad"};
cvar_t	tp_name_pent = {"tp_name_pent", "pent"};
cvar_t	tp_name_ring = {"tp_name_ring", "ring"};
cvar_t	tp_name_suit = {"tp_name_suit", "suit"};
cvar_t	tp_name_shells = {"tp_name_shells", "shells"};
cvar_t	tp_name_nails = {"tp_name_nails", "nails"};
cvar_t	tp_name_rockets = {"tp_name_rockets", "rockets"};
cvar_t	tp_name_cells = {"tp_name_cells", "cells"};
cvar_t	tp_name_mh = {"tp_name_mh", "mega"};
cvar_t	tp_name_health = {"tp_name_health", "health"};
cvar_t	tp_name_backpack = {"tp_name_backpack", "pack"};
cvar_t	tp_name_flag = {"tp_name_flag", "flag"};
cvar_t	tp_name_nothing = {"tp_name_nothing", "nothing"};
cvar_t	tp_name_someplace = {"tp_name_someplace", "someplace"};
cvar_t	tp_name_at = {"tp_name_at", "at"};
cvar_t	tp_need_ra = {"tp_need_ra", "50"};
cvar_t	tp_need_ya = {"tp_need_ya", "50"};
cvar_t	tp_need_ga = {"tp_need_ga", "50"};
cvar_t	tp_need_health = {"tp_need_health", "50"};
cvar_t	tp_need_weapon = {"tp_need_weapon", "35687"};
cvar_t	tp_need_rl = {"tp_need_rl", "1"};
cvar_t	tp_need_rockets = {"tp_need_rockets", "5"};
cvar_t	tp_need_cells = {"tp_need_cells", "20"};
cvar_t	tp_need_nails = {"tp_need_nails", "40"};
cvar_t	tp_need_shells = {"tp_need_shells", "10"};

extern cvar_t	host_mapname;

void TP_FindModelNumbers (void);
void TP_FindPoint (void);
char *TP_LocationName (vec3_t location);


#define MAX_LOC_NAME 48

// this structure is cleared after entering a new map
typedef struct tvars_s {
	int		health;
	int		items;
	int		olditems;
	int		stat_framecounts[MAX_CL_STATS];
	int		activeweapon;
	float	respawntrigger_time;
	float	deathtrigger_time;
	float	f_version_reply_time;
	char	lastdeathloc[MAX_LOC_NAME];
	char	tookname[32];
	char	tookloc[MAX_LOC_NAME];
	float	tooktime;

	int		pointframe;		// cls.framecount for which point* vars are valid
	char	pointname[32];
	vec3_t	pointorg;
	char	pointloc[MAX_LOC_NAME];
} tvars_t;

tvars_t vars;

//===========================================================================
//								TRIGGERS
//===========================================================================

void TP_ExecTrigger (char *s)
{
	char *astr;

	if (!cl_triggers.value || cls.demoplayback)
		return;

	astr = Cmd_AliasExist(s, RESTRICT_LOCAL);
	if (astr)
	{
		char *p;
		qbool quote = false;

		for (p=astr ; *p ; p++)
		{
			if (*p == '"')
				quote = !quote;
			if (!quote && *p == ';')
			{
				// more than one command, add it to the command buffer
				Cbuf_AddText (astr, RESTRICT_LOCAL);
				Cbuf_AddText ("\n", RESTRICT_LOCAL);
				return;
			}
		}
		// a single line, so execute it right away
		Cmd_ExecuteString (astr, RESTRICT_LOCAL);
		return;
	}
}


/*
==========================================================================
						        MACRO FUNCTIONS
==========================================================================
*/

#define MAX_MACRO_VALUE	256
static char	macro_buf[MAX_MACRO_VALUE] = "";

// buffer-size-safe helper functions
void MacroBuf_strcat (char *str) {
	strlcat (macro_buf, str, sizeof(macro_buf));
}
void MacroBuf_strcat_with_separator (char *str) {
	if (macro_buf[0])
		strlcat (macro_buf, "/", sizeof(macro_buf));
	strlcat (macro_buf, str, sizeof(macro_buf));
}



char *Macro_Quote (void)
{
	return "\"";
}

char *Macro_Latency (void)
{
	sprintf(macro_buf, "%i", Q_rint(cls.latency*1000));
	return macro_buf;
}

char *Macro_Health (void)
{
	sprintf(macro_buf, "%i", cl.stats[SP][STAT_HEALTH]);
	return macro_buf;
}

char *Macro_Armor (void)
{
	sprintf(macro_buf, "%i", cl.stats[SP][STAT_ARMOR]);
	return macro_buf;
}

char *Macro_Shells (void)
{
	sprintf(macro_buf, "%i", cl.stats[SP][STAT_SHELLS]);
	return macro_buf;
}

char *Macro_Nails (void)
{
	sprintf(macro_buf, "%i", cl.stats[SP][STAT_NAILS]);
	return macro_buf;
}

char *Macro_Rockets (void)
{
	sprintf(macro_buf, "%i", cl.stats[SP][STAT_ROCKETS]);
	return macro_buf;
}

char *Macro_Cells (void)
{
	sprintf(macro_buf, "%i", cl.stats[SP][STAT_CELLS]);
	return macro_buf;
}

char *Macro_Ammo (void)
{
	sprintf(macro_buf, "%i", cl.stats[SP][STAT_AMMO]);
	return macro_buf;
}

char *Macro_Weapon (void)
{
	switch (cl.stats[SP][STAT_ACTIVEWEAPON])
	{
	case IT_AXE: return "axe";
	case IT_SHOTGUN: return "sg";
	case IT_SUPER_SHOTGUN: return "ssg";
	case IT_NAILGUN: return "ng";
	case IT_SUPER_NAILGUN: return "sng";
	case IT_GRENADE_LAUNCHER: return "gl";
	case IT_ROCKET_LAUNCHER: return "rl";
	case IT_LIGHTNING: return "lg";
	default:
		return "";
	}
}

char *Macro_Weapons (void) {	
	macro_buf[0] = 0;

	if (cl.stats[SP][STAT_ITEMS] & IT_LIGHTNING)
		strcpy(macro_buf, "lg");
	if (cl.stats[SP][STAT_ITEMS] & IT_ROCKET_LAUNCHER)
		MacroBuf_strcat_with_separator ("rl");
	if (cl.stats[SP][STAT_ITEMS] & IT_GRENADE_LAUNCHER)
		MacroBuf_strcat_with_separator ("gl");
	if (cl.stats[SP][STAT_ITEMS] & IT_SUPER_NAILGUN)
		MacroBuf_strcat_with_separator ("sng");
	if (cl.stats[SP][STAT_ITEMS] & IT_NAILGUN)
		MacroBuf_strcat_with_separator ("ng");
	if (cl.stats[SP][STAT_ITEMS] & IT_SUPER_SHOTGUN)
		MacroBuf_strcat_with_separator ("ssg");
	if (cl.stats[SP][STAT_ITEMS] & IT_SHOTGUN)
		MacroBuf_strcat_with_separator ("sg");
	if (cl.stats[SP][STAT_ITEMS] & IT_AXE)
		MacroBuf_strcat_with_separator ("axe");
//	if (!macro_buf[0])	
//		strlcpy(macro_buf, tp_name_none.string, sizeof(macro_buf));

	return macro_buf;
}

char *Macro_WeaponAndAmmo (void)
{
	char buf[sizeof(macro_buf)];
	Q_snprintfz (buf, sizeof(buf), "%s:%s", Macro_Weapon(), Macro_Ammo());
	strcpy (macro_buf, buf);
	return macro_buf;
}

char *Macro_WeaponNum (void)
{
	switch (cl.stats[SP][STAT_ACTIVEWEAPON])
	{
	case IT_AXE: return "1";
	case IT_SHOTGUN: return "2";
	case IT_SUPER_SHOTGUN: return "3";
	case IT_NAILGUN: return "4";
	case IT_SUPER_NAILGUN: return "5";
	case IT_GRENADE_LAUNCHER: return "6";
	case IT_ROCKET_LAUNCHER: return "7";
	case IT_LIGHTNING: return "8";
	default:
		return "0";
	}
}

int	_Macro_BestWeapon (void)
{
	if (cl.stats[SP][STAT_ITEMS] & IT_ROCKET_LAUNCHER)
		return IT_ROCKET_LAUNCHER;
	else if (cl.stats[SP][STAT_ITEMS] & IT_LIGHTNING)
		return IT_LIGHTNING;
	else if (cl.stats[SP][STAT_ITEMS] & IT_GRENADE_LAUNCHER)
		return IT_GRENADE_LAUNCHER;
	else if (cl.stats[SP][STAT_ITEMS] & IT_SUPER_NAILGUN)
		return IT_SUPER_NAILGUN;
	else if (cl.stats[SP][STAT_ITEMS] & IT_NAILGUN)
		return IT_NAILGUN;
	else if (cl.stats[SP][STAT_ITEMS] & IT_SUPER_SHOTGUN)
		return IT_SUPER_SHOTGUN;
	else if (cl.stats[SP][STAT_ITEMS] & IT_SHOTGUN)
		return IT_SHOTGUN;
	else if (cl.stats[SP][STAT_ITEMS] & IT_AXE)
		return IT_AXE;
	else
		return 0;
}

char *Macro_BestWeapon (void)
{
	switch (_Macro_BestWeapon())
	{
	case IT_AXE: return "axe";
	case IT_SHOTGUN: return "sg";
	case IT_SUPER_SHOTGUN: return "ssg";
	case IT_NAILGUN: return "ng";
	case IT_SUPER_NAILGUN: return "sng";
	case IT_GRENADE_LAUNCHER: return "gl";
	case IT_ROCKET_LAUNCHER: return "rl";
	case IT_LIGHTNING: return "lg";
	default:
		return "";
	}
}

char *Macro_BestAmmo (void)
{
	switch (_Macro_BestWeapon())
	{
	case IT_SHOTGUN: case IT_SUPER_SHOTGUN: 
		sprintf(macro_buf, "%i", cl.stats[0][STAT_SHELLS]);
		return macro_buf;

	case IT_NAILGUN: case IT_SUPER_NAILGUN:
		sprintf(macro_buf, "%i", cl.stats[0][STAT_NAILS]);
		return macro_buf;

	case IT_GRENADE_LAUNCHER: case IT_ROCKET_LAUNCHER:
		sprintf(macro_buf, "%i", cl.stats[0][STAT_ROCKETS]);
		return macro_buf;

	case IT_LIGHTNING:
		sprintf(macro_buf, "%i", cl.stats[0][STAT_CELLS]);
		return macro_buf;

	default:
		return "0";
	}
}

// needed for %b parsing
char *Macro_BestWeaponAndAmmo (void)
{
	char buf[MAX_MACRO_VALUE];
	sprintf (buf, "%s:%s", Macro_BestWeapon(), Macro_BestAmmo());
	strcpy (macro_buf, buf);
	return macro_buf;
}

char *Macro_ArmorType (void)
{
	if (cl.stats[SP][STAT_ITEMS] & IT_ARMOR1)
		return "g";
	else if (cl.stats[SP][STAT_ITEMS] & IT_ARMOR2)
		return "y";
	else if (cl.stats[SP][STAT_ITEMS] & IT_ARMOR3)
		return "r";
	else
		return "";	// no armor at all
}

char *Macro_Powerups (void)
{
	int effects;

	macro_buf[0] = 0;

	if (cl.stats[SP][STAT_ITEMS] & IT_QUAD)
		strcpy(macro_buf, "quad");

	if (cl.stats[SP][STAT_ITEMS] & IT_INVULNERABILITY) {
		if (macro_buf[0])
			strcat(macro_buf, "/");
		strcat(macro_buf, "pent");
	}

	if (cl.stats[SP][STAT_ITEMS] & IT_INVISIBILITY) {
		if (macro_buf[0])
			strcat(macro_buf, "/");
		strcat(macro_buf, "ring");
	}

	effects = cl.frames[cl.parsecount&UPDATE_MASK].playerstate[cl.playernum[SP]].effects;
	if ( (effects & (EF_FLAG1|EF_FLAG2)) ||		// CTF
		(cl.teamfortress && cl.stats[SP][STAT_ITEMS] & (IT_KEY1|IT_KEY2)) ) // TF
	{
		if (macro_buf[0])
			strcat(macro_buf, "/");
		strcat(macro_buf, "flag");
	}

	return macro_buf;
}

char *Macro_Location (void)
{
	return TP_LocationName (cl.simorg[SP]);
}

char *Macro_LastDeath (void)
{
	if (vars.deathtrigger_time)
		return vars.lastdeathloc;
	else
		return tp_name_someplace.string;
}

char *Macro_Location2 (void)
{
	if (vars.deathtrigger_time && realtime - vars.deathtrigger_time <= 5)
		return vars.lastdeathloc;
	return Macro_Location();
}

char *Macro_Time (void)
{
	time_t		t;
	struct tm	*ptm;

	time (&t);
	ptm = localtime (&t);
	if (!ptm)
		return "#bad date#";
	strftime (macro_buf, sizeof(macro_buf)-1, "%H:%M", ptm);
	return macro_buf;
}

char *Macro_Date (void)
{
	time_t		t;
	struct tm	*ptm;

	time (&t);
	ptm = localtime (&t);
	if (!ptm)
		return "#bad date#";
	strftime (macro_buf, sizeof(macro_buf)-1, "%d.%m.%Y", ptm);
	return macro_buf;
}

// returns the last item picked up
char *Macro_Took (void)
{
	if (!vars.tooktime || realtime > vars.tooktime + 20)
		strlcpy (macro_buf, tp_name_nothing.string, sizeof(macro_buf));
	else
		strcpy (macro_buf, vars.tookname);
	return macro_buf;
}

// returns location of the last item picked up
char *Macro_TookLoc (void)
{
	if (!vars.tooktime || realtime > vars.tooktime + 20)
		strlcpy (macro_buf, tp_name_someplace.string, sizeof(macro_buf));
	else
		strcpy (macro_buf, vars.tookloc);
	return macro_buf;
}


// %i macro - last item picked up in "name at location" style
char *Macro_TookAtLoc (void)
{
	if (!vars.tooktime || realtime > vars.tooktime + 20)
		strncpy (macro_buf, tp_name_nothing.string, sizeof(macro_buf)-1);
	else
	{
		strlcpy (macro_buf, va("%s %s %s", vars.tookname,
			tp_name_at.string, vars.tookloc), sizeof(macro_buf));
	}
	return macro_buf;
}

// pointing calculations are CPU expensive, so the results are cached
// in vars.pointname & vars.pointloc
char *Macro_PointName (void)
{
	if (cls.framecount != vars.pointframe)
		TP_FindPoint ();
	return vars.pointname;
}

char *Macro_PointLocation (void)
{
	if (cls.framecount != vars.pointframe)
		TP_FindPoint ();
	if (vars.pointloc[0])
		return vars.pointloc;
	else {
		strlcpy (macro_buf, tp_name_someplace.string, sizeof(macro_buf));
		return macro_buf;
	}
}

char *Macro_PointNameAtLocation (void)
{
	if (cls.framecount != vars.pointframe)
		TP_FindPoint ();
	if (vars.pointloc[0])
		return va ("%s %s %s", vars.pointname, tp_name_at.string, vars.pointloc);
	else
		return vars.pointname;
}

char *Macro_Need (void)
{
	int i, weapon;
	char	*needammo = NULL;

	macro_buf[0] = 0;

	// check armor
	if (   ((cl.stats[SP][STAT_ITEMS] & IT_ARMOR1) && cl.stats[SP][STAT_ARMOR] < tp_need_ga.value)
		|| ((cl.stats[SP][STAT_ITEMS] & IT_ARMOR2) && cl.stats[SP][STAT_ARMOR] < tp_need_ya.value)
		|| ((cl.stats[SP][STAT_ITEMS] & IT_ARMOR3) && cl.stats[SP][STAT_ARMOR] < tp_need_ra.value)
		|| (!(cl.stats[SP][STAT_ITEMS] & (IT_ARMOR1|IT_ARMOR2|IT_ARMOR3))
			&& (tp_need_ga.value || tp_need_ya.value || tp_need_ra.value)))
		strcpy (macro_buf, "armor");

	// check health
	if (tp_need_health.value && cl.stats[SP][STAT_HEALTH] < tp_need_health.value) {
		MacroBuf_strcat_with_separator ("health");
	}

	if (cl.teamfortress)
	{
		// in TF, we have all weapons from the start,
		// and ammo is checked differently
		if (cl.stats[SP][STAT_ROCKETS] < tp_need_rockets.value)
			MacroBuf_strcat_with_separator ("rockets");
		if (cl.stats[SP][STAT_SHELLS] < tp_need_shells.value)
			MacroBuf_strcat_with_separator ("shells");
		if (cl.stats[SP][STAT_NAILS] < tp_need_nails.value)
			MacroBuf_strcat_with_separator ("nails");
		if (cl.stats[SP][STAT_CELLS] < tp_need_cells.value)
			MacroBuf_strcat_with_separator ("cells");
		goto done;
	}

	// check weapon
	weapon = 0;
	for (i=strlen(tp_need_weapon.string)-1 ; i>=0 ; i--) {
		switch (tp_need_weapon.string[i]) {
			case '2': if (cl.stats[SP][STAT_ITEMS] & IT_SHOTGUN) weapon = 2; break;
			case '3': if (cl.stats[SP][STAT_ITEMS] & IT_SUPER_SHOTGUN) weapon = 3; break;
			case '4': if (cl.stats[SP][STAT_ITEMS] & IT_NAILGUN) weapon = 4; break;
			case '5': if (cl.stats[SP][STAT_ITEMS] & IT_SUPER_NAILGUN) weapon = 5; break;
			case '6': if (cl.stats[SP][STAT_ITEMS] & IT_GRENADE_LAUNCHER) weapon = 6; break;
			case '7': if (cl.stats[SP][STAT_ITEMS] & IT_ROCKET_LAUNCHER) weapon = 7; break;
			case '8': if (cl.stats[SP][STAT_ITEMS] & IT_LIGHTNING) weapon = 8; break;
		}
		if (weapon)
			break;
	}

	if (!weapon) {
		MacroBuf_strcat_with_separator ("weapon");
	} else {
		if (tp_need_rl.value && !(cl.stats[SP][STAT_ITEMS] & IT_ROCKET_LAUNCHER)) {
			MacroBuf_strcat_with_separator ("rl");
		}

		switch (weapon) {
			case 2: case 3: if (cl.stats[SP][STAT_SHELLS] < tp_need_shells.value) needammo = "shells"; break;
			case 4: case 5: if (cl.stats[SP][STAT_NAILS] < tp_need_nails.value) needammo = "nails"; break;
			case 6: case 7: if (cl.stats[SP][STAT_ROCKETS] < tp_need_rockets.value) needammo = "rockets"; break;
			case 8: if (cl.stats[SP][STAT_CELLS] < tp_need_cells.value) needammo = "cells"; break;
		}
		if (needammo) {
			MacroBuf_strcat_with_separator (needammo);
		}
	}

done:
	if (!macro_buf[0])
		strcpy (macro_buf, "nothing");

	return macro_buf;
}

char *Macro_TF_Skin (void)
{
	char *myskin;

	myskin = Info_ValueForKey(cl.players[cl.playernum[SP]].userinfo, "skin");
	if (!cl.teamfortress)
		strcpy(macro_buf, myskin);
	else
	{
		if (!Q_stricmp(myskin, "tf_demo"))
			strcpy(macro_buf, "demoman");
		else if (!Q_stricmp(myskin, "tf_eng"))
			strcpy (macro_buf, "engineer");
		else if (!Q_stricmp(myskin, "tf_hwguy"))
			strcpy(macro_buf, "hwguy");
		else if (!Q_stricmp(myskin, "tf_medic"))
			strcpy(macro_buf, "medic");
		else if (!Q_stricmp(myskin, "tf_pyro"))
			strcpy(macro_buf, "pyro");
		else if (!Q_stricmp(myskin, "tf_scout"))
			strcpy(macro_buf, "scout");
		else if (!Q_stricmp(myskin, "tf_snipe"))
			strcpy(macro_buf, "sniper");
		else if (!Q_stricmp(myskin, "tf_sold"))
			strcpy(macro_buf, "soldier");
		else if (!Q_stricmp(myskin, "tf_spy"))
			strcpy(macro_buf, "spy");
		else
			strcpy(macro_buf, myskin);
	}
	return macro_buf;
}


void TP_InitMacros(void)
{
	Cmd_AddMacro("qt", Macro_Quote);
	Cmd_AddMacro("latency", Macro_Latency);
	Cmd_AddMacro("health", Macro_Health);
	Cmd_AddMacro("armortype", Macro_ArmorType);
	Cmd_AddMacro("armor", Macro_Armor);
	Cmd_AddMacro("shells", Macro_Shells);
	Cmd_AddMacro("nails", Macro_Nails);
	Cmd_AddMacro("rockets", Macro_Rockets);
	Cmd_AddMacro("cells", Macro_Cells);
	Cmd_AddMacro("weaponnum", Macro_WeaponNum);
	Cmd_AddMacro("weapons", Macro_Weapons);
	Cmd_AddMacro("weapon", Macro_Weapon);
	Cmd_AddMacro("ammo", Macro_Ammo);
	Cmd_AddMacro("bestweapon", Macro_BestWeapon);
	Cmd_AddMacro("bestammo", Macro_BestAmmo);
	Cmd_AddMacro("powerups", Macro_Powerups);
	Cmd_AddMacro("location", Macro_Location);
	Cmd_AddMacro("deathloc", Macro_LastDeath);
	Cmd_AddMacro("time", Macro_Time);
	Cmd_AddMacro("date", Macro_Date);
	Cmd_AddMacro("tookatloc", Macro_TookAtLoc);
	Cmd_AddMacro("took", Macro_Took);
	Cmd_AddMacro("tf_skin", Macro_TF_Skin);

}

#define MAX_MACRO_STRING 1024

/*
=============
TP_ParseMacroString

Parses %a-like expressions
=============
*/
char *TP_ParseMacroString (char *s)
{
	static char	buf[MAX_MACRO_STRING];
	int		i = 0;
	char	*macro_string;

	if (!cl_parseSay.value)
		return s;

	while (*s && i < MAX_MACRO_STRING-1)
	{
		// check %[P], etc
		if (*s == '%' && s[1]=='[' && s[2] && s[3]==']')
		{
			static char mbuf[MAX_MACRO_VALUE];
			switch (s[2]) {
			case 'a':
				macro_string = Macro_ArmorType();
				if (!macro_string[0])
					macro_string = "a";
				if (cl.stats[SP][STAT_ARMOR] < 30)
					sprintf (mbuf, "\x10%s:%i\x11", macro_string, cl.stats[SP][STAT_ARMOR]);
				else
					sprintf (mbuf, "%s:%i", macro_string, cl.stats[SP][STAT_ARMOR]);
				macro_string = mbuf;
				break;
				
			case 'h':
				if (cl.stats[SP][STAT_HEALTH] >= 50)
					sprintf (macro_buf, "%i", cl.stats[SP][STAT_HEALTH]);
				else
					sprintf (macro_buf, "\x10%i\x11", cl.stats[SP][STAT_HEALTH]);
				macro_string = macro_buf;
				break;
				
			case 'p':
			case 'P':
				macro_string = Macro_Powerups();
				if (macro_string[0])
					sprintf (mbuf, "\x10%s\x11", macro_string);
				else
					mbuf[0] = 0;
				macro_string = mbuf;
				break;
				
				// todo: %[w], %[b]
				
			default:
				buf[i++] = *s++;
				continue;
			}
			if (i + strlen(macro_string) >= MAX_MACRO_STRING-1)
				Sys_Error("TP_ParseMacroString: macro string length > MAX_MACRO_STRING)");
			strcpy (&buf[i], macro_string);
			i += strlen(macro_string);
			s += 4;	// skip %[<char>]
			continue;
		}
		
		// check %a, etc
		if (*s == '%')
		{
			switch (s[1])
			{
				case 'a': macro_string = Macro_Armor(); break;
				case 'A': macro_string = Macro_ArmorType(); break;
				case 'b': macro_string = Macro_BestWeaponAndAmmo(); break;
				case 'c': macro_string = Macro_Cells(); break;
				case 'd': macro_string = Macro_LastDeath(); break;
				case 'h': macro_string = Macro_Health(); break;
				case 'i': macro_string = Macro_TookAtLoc(); break;
				case 'l': macro_string = Macro_Location(); break;
				case 'L': macro_string = Macro_Location2(); break;
				case 'P':
				case 'p': macro_string = Macro_Powerups(); break;
				case 'r': macro_string = Macro_Rockets(); break;
				case 'u': macro_string = Macro_Need(); break;
				case 'w': macro_string = Macro_WeaponAndAmmo(); break;
				case 'x': macro_string = Macro_PointName(); break;
				case 'y': macro_string = Macro_PointLocation(); break;
				case 't': macro_string = Macro_PointNameAtLocation(); break;
				case 'S': macro_string = Macro_TF_Skin(); break;
				default: 
					buf[i++] = *s++;
					continue;
			}
			if (i + strlen(macro_string) >= MAX_MACRO_STRING-1)
				Sys_Error("TP_ParseMacroString: macro string length > MAX_MACRO_STRING)");
			strcpy (&buf[i], macro_string);
			i += strlen(macro_string);
			s += 2;	// skip % and letter
			continue;
		}

		buf[i++] = *s++;
	}
	buf[i] = 0;

	return buf;
}


/*
==============
TP_ParseFunChars

Doesn't check for overflows, so strlen(s) should be < MAX_MACRO_STRING
==============
*/
char *TP_ParseFunChars (char *s, qbool chat)
{
	static char	 buf[MAX_MACRO_STRING];
	char		*out = buf;
	int			 c;

	if (!cl_parseFunChars.value)
		return s;

	while (*s) {
		if (*s == '$' && s[1] == 'x') {
			int i;
			// check for $x10, $x8a, etc
			c = tolower((int)(unsigned char)s[2]);
			if ( isdigit(c) )
				i = (c - (int)'0') << 4;
			else if ( isxdigit(c) )
				i = (c - (int)'a' + 10) << 4;
			else goto skip;
			c = tolower((int)(unsigned char)s[3]);
			if ( isdigit(c) )
				i += (c - (int)'0');
			else if ( isxdigit(c) )
				i += (c - (int)'a' + 10);
			else goto skip;
			if (!i)
				i = (int)' ';
			*out++ = (char)i;
			s += 4;
			continue;
		}
		if (*s == '$' && s[1]) {
			c = 0;
			switch (s[1]) {
				case '\\': c = 0x0D; break;
				case ':': c = 0x0A; break;
				case '[': c = 0x10; break;
				case ']': c = 0x11; break;
				case 'G': c = 0x86; break;
				case 'R': c = 0x87; break;
				case 'Y': c = 0x88; break;
				case 'B': c = 0x89; break;
				case '(': c = 0x80; break;
				case '=': c = 0x81; break;
				case ')': c = 0x82; break;
				case 'a': c = 0x83; break;
				case '<': c = 0x1d; break;
				case '-': c = 0x1e; break;
				case '>': c = 0x1f; break;
				case ',': c = 0x1c; break;
				case '.': c = 0x9c; break;
				case 'b': c = 0x8b; break;
				case 'c':
				case 'd': c = 0x8d; break;
				case '$': c = '$'; break;
				case '^': c = '^'; break;
			}
			if ( isdigit((int)(unsigned char)s[1]) )
				c = s[1] - (int)'0' + 0x12;
			if (c) {
				*out++ = (char)c;
				s += 2;
				continue;
			}
		}
		if (!chat && *s == '^' && s[1] && s[1] != ' ') {
			*out++ = s[1] | 128;
			s += 2;
			continue;
		}
skip:			
		*out++ = *s++;
	}
	*out = 0;

	return buf;
}

/*
=============================================================================

							PROXY .LOC FILES

=============================================================================
*/

typedef struct locdata_s {
	vec3_t coord;
	char name[MAX_LOC_NAME];
} locdata_t;

#define MAX_LOC_ENTRIES 4096

locdata_t locdata[MAX_LOC_ENTRIES];	// FIXME: allocate dynamically?
int	loc_numentries;


void TP_LoadLocFile (char *filename, qbool quiet)
{
	char	fullpath[MAX_QPATH];
	char	*buf, *p;
	char	line[1024];
	int		i, argc;
	int		errorcount = 0;
	locdata_t	*loc;

	if (!*filename)
		return;

	Q_snprintfz (fullpath, sizeof(fullpath) - 4, "locs/%s", filename);
	COM_DefaultExtension (fullpath, ".loc");

	buf = (char *) COM_LoadTempFile (fullpath);
	if (!buf) {
		if (!quiet)
			Com_Printf ("Could not load %s\n", fullpath);
		return;
	}

	loc_numentries = 0;

	// parse the file
	// we rely on the fact that FS_Load*File always appends a 0 at the end
	p = buf;
	while (1) {
		if (!*p)
			break;		// end of file

		// get a line out
		for (i = 0; i < sizeof(line)-1; ) {
			char c = *p++;
			if (!c || c == 10)
				break;
			if (c != 13)
				line[i++] = c;
		}
		line[i] = 0;

		Cmd_TokenizeString (line);

		argc = Cmd_Argc();
		if (!argc)
			continue;

		if (argc < 4) {
			errorcount++;
			continue;
		}

		if (atoi(Cmd_Argv(0)) == 0 && Cmd_Argv(0)[0] != '0') {
			// first token is not a number
			errorcount++;
			continue;
		}

		if (loc_numentries >= MAX_LOC_ENTRIES)
			continue;

		loc = &locdata[loc_numentries];
		loc_numentries++;

		for (i = 0; i < 3; i++)
			loc->coord[i] = atoi(Cmd_Argv(i)) / 8.0;

		loc->name[0] = 0;
		loc->name[sizeof(loc->name)-1] = 0;	// can't rely on strncat
		for (i = 3; i < argc; i++) {
			if (i != 3)
				strncat (loc->name, " ", sizeof(loc->name)-1);
			strncat (loc->name, Cmd_Argv(i), sizeof(loc->name)-1);
		}
	}

	if (!quiet)
		Com_Printf ("Loaded %s (%i points)\n", fullpath, loc_numentries);
}

void TP_LoadLocFile_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf ("loadloc <filename> : load a loc file\n");
		return;
	}

	TP_LoadLocFile (Cmd_Argv(1), false);
}

char *TP_LocationName (vec3_t location)
{
	int		i, minnum;
	float	dist, mindist;
	vec3_t	vec;
	static qbool	recursive;
	static char buf[1024];
	
	if (!loc_numentries || (cls.state != ca_active))
		return tp_name_someplace.string;

	if (recursive)
		return "";

	minnum = 0;
	mindist = 9999999;

	for (i = 0; i < loc_numentries; i++) {
		VectorSubtract (location, locdata[i].coord, vec);
		dist = VectorLength (vec);
		if (dist < mindist) {
			minnum = i;
			mindist = dist;
		}
	}

	recursive = true;
	Cmd_ExpandString (locdata[minnum].name, buf, sizeof(buf), Cmd_ExecLevel);
	recursive = false;

	return buf;
}

/*
=============================================================================

							MESSAGE TRIGGERS

=============================================================================
*/

// FIXME, we don't provide a way to remove triggers
// allocated heap memory is not freed when the engine shuts down

typedef struct msg_trigger_s {
	char	name[32];
	char	string[64];
	int		level;
	struct msg_trigger_s *next;
} msg_trigger_t;

static msg_trigger_t *msg_triggers;

msg_trigger_t *TP_FindTrigger (char *name)
{
	msg_trigger_t *t;

	for (t=msg_triggers; t; t=t->next)
		if (!strcmp(t->name, name))
			return t;

	return NULL;
}


void TP_MsgTrigger_f (void)
{
	int		c;
	char	*name;
	msg_trigger_t	*trig;

	c = Cmd_Argc();

	if (c > 5) {
		Com_Printf ("msg_trigger <trigger name> \"string\" [-l <level>]\n");
		return;
	}

	if (c == 1) {
		if (!msg_triggers)
			Com_Printf ("no triggers defined\n");
		else
		for (trig=msg_triggers; trig; trig=trig->next)
			Com_Printf ("%s : \"%s\"\n", trig->name, trig->string);
		return;
	}

	name = Cmd_Argv(1);
	if (strlen(name) > 31) {
		Com_Printf ("trigger name too long\n");
		return;
	}

	if (c == 2) {
		trig = TP_FindTrigger (name);
		if (trig)
			Com_Printf ("%s: \"%s\"\n", trig->name, trig->string);
		else
			Com_Printf ("trigger \"%s\" not found\n", name);
		return;
	}

	if (c >= 3) {
		if (strlen(Cmd_Argv(2)) > 63) {
			Com_Printf ("trigger string too long\n");
			return;
		}
		
		trig = TP_FindTrigger (name);

		if (!trig) {
			// allocate new trigger
			trig = Z_Malloc (sizeof(msg_trigger_t));
			trig->next = msg_triggers;
			msg_triggers = trig;
			strcpy (trig->name, name);	// safe (length checked earlier)
			trig->level = PRINT_HIGH;
		}

		strcpy (trig->string, Cmd_Argv(2));	// safe (length checked earlier)
		if (c == 5 && !Q_stricmp (Cmd_Argv(3), "-l")) {
			if (!strcmp(Cmd_Argv(4), "t"))
				trig->level = 4;
			else {
				trig->level = Q_atoi (Cmd_Argv(4));
				if ((unsigned)trig->level > PRINT_CHAT)
					trig->level = PRINT_HIGH;
			}
		}
	}
}


void TP_SearchForMsgTriggers (char *s, int level)
{
	msg_trigger_t	*t;
	char *string;

	if (cls.demoplayback)
		return;

	for (t=msg_triggers; t; t=t->next)
		if ((t->level == level || (t->level == 3 && level == 4))
			&& t->string[0] && strstr(s, t->string))
		{
			if (level == PRINT_CHAT && (
				strstr (s, "f_version") || strstr (s, "f_system") ||
				strstr (s, "f_speed") || strstr (s, "f_modified")))
				continue; 	// don't let llamas fake proxy replies

			string = Cmd_AliasExist (t->name, RESTRICT_LOCAL);
			if (string)
			{
				Cbuf_AddText (string, RESTRICT_LOCAL);
//				Cbuf_ExecuteLevel (RESTRICT_LOCAL);
			}
			else
				Com_Printf ("trigger \"%s\" has no matching alias\n", t->name);
		}
}

/*
void TP_CheckVersionRequest (char *s)
{
	char buf[11];
	int	i;

	if (cl.spectator)
		return;

	if (vars.f_version_reply_time
		&& realtime - vars.f_version_reply_time < 20)
		return;	// don't reply again if 20 seconds haven't passed

	while (1)
	{
		switch (*s++)
		{
		case 0:
		case '\n':
			return;
		case ':':
		case (char)(':'|128):		// hmm.... why is this here?
			goto ok;
		}
	}
	return;

ok:
	for (i = 0; i < 11 && s[i]; i++)
		buf[i] = s[i] &~ 128;			// strip high bit

	if (!strncmp(buf, " f_version\n", 11) || !strncmp(buf, " z_version\n", 11))
	{
		Cbuf_AddText (va("say ZQuake version %s "
			QW_PLATFORM ":" QW_RENDERER "\n", VersionString()));
		vars.f_version_reply_time = realtime;
	}
}*/


int	TP_CountPlayers (void)
{
	int	i, count;

	count = 0;
	for (i = 0; i < MAX_CLIENTS ; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator)
			count++;
	}

	return count;
}

char *TP_EnemyTeam (void)
{
	int			i;
	char		myteam[MAX_INFO_KEY];
	static char	enemyteam[MAX_INFO_KEY];

	strcpy (myteam, Info_ValueForKey(cls.userinfo, "team"));

	for (i = 0; i < MAX_CLIENTS ; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator)
		{
			strcpy (enemyteam, Info_ValueForKey(cl.players[i].userinfo, "team"));
			if (strcmp(myteam, enemyteam) != 0)
				return enemyteam;
		}
	}
	return "";
}

char *TP_PlayerName (void)
{
	static char	myname[MAX_INFO_KEY];
	strcpy (myname, Info_ValueForKey(cl.players[cl.playernum[SP]].userinfo, "name"));
	return myname;
}

char *TP_PlayerTeam (void)
{
	static char	myteam[MAX_INFO_KEY];
	strcpy (myteam, Info_ValueForKey(cl.players[cl.playernum[SP]].userinfo, "team"));
	return myteam;
}

char *TP_EnemyName (void)
{
	int			i;
	char		*myname;
	static char	enemyname[MAX_INFO_KEY];

	myname = TP_PlayerName ();

	for (i = 0; i < MAX_CLIENTS ; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator)
		{
			strcpy (enemyname, Info_ValueForKey(cl.players[i].userinfo, "name"));
			if (!strcmp(enemyname, myname))
				return enemyname;
		}
	}
	return "";
}

char *TP_MapName (void)
{
	return host_mapname.string;
}

/*
=============================================================================
						TEAMCOLOR & ENEMYCOLOR
=============================================================================
*/

int		cl_teamtopcolor = -1;
int		cl_teambottomcolor = -1;
int		cl_enemytopcolor = -1;
int		cl_enemybottomcolor = -1;

void TP_TeamColor_f (void)
{
	int	top, bottom;
	int	i;

	if (Cmd_Argc() == 1)
	{
		if (cl_teamtopcolor < 0)
			Com_Printf ("\"teamcolor\" is \"off\"\n");
		else
			Com_Printf ("\"teamcolor\" is \"%i %i\"\n", 
				cl_teamtopcolor,
				cl_teambottomcolor);
		return;
	}

	if (!strcmp(Cmd_Argv(1), "off"))
	{
		cl_teamtopcolor = -1;
		cl_teambottomcolor = -1;
		if (qrenderer)	//make sure we have the renderer initialised...
			for (i = 0; i < MAX_CLIENTS; i++)
				CL_NewTranslation(i);
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else {
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}
	
	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;
	
//	if (top != cl_teamtopcolor || bottom != cl_teambottomcolor)
	{
		cl_teamtopcolor = top;
		cl_teambottomcolor = bottom;

		if (qrenderer)	//make sure we have the renderer initialised...
			for (i = 0; i < MAX_CLIENTS; i++)
				CL_NewTranslation(i);
	}
}

void TP_EnemyColor_f (void)
{
	int	top, bottom;
	int	i;

	if (Cmd_Argc() == 1)
	{
		if (cl_enemytopcolor < 0)
			Com_Printf ("\"enemycolor\" is \"off\"\n");
		else
			Com_Printf ("\"enemycolor\" is \"%i %i\"\n", 
				cl_enemytopcolor,
				cl_enemybottomcolor);
		return;
	}

	if (!strcmp(Cmd_Argv(1), "off"))
	{
		cl_enemytopcolor = -1;
		cl_enemybottomcolor = -1;
		if (qrenderer)	//make sure we have the renderer initialised...
			for (i = 0; i < MAX_CLIENTS; i++)
				CL_NewTranslation(i);
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else {
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}
	
	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

//	if (top != cl_enemytopcolor || bottom != cl_enemybottomcolor)
	{
		cl_enemytopcolor = top;
		cl_enemybottomcolor = bottom;

		if (qrenderer)	//make sure we have the renderer initialised...
			for (i = 0; i < MAX_CLIENTS; i++)
				CL_NewTranslation(i);
	}
}

//===================================================================

void TP_NewMap (void)
{
	static char last_map[MAX_QPATH];
	char locname[MAX_OSPATH];

	memset (&vars, 0, sizeof(vars));
	TP_FindModelNumbers ();

	// FIXME, just try to load the loc file no matter what?
	if (strcmp(host_mapname.string, last_map))
	{	// map name has changed
		loc_numentries = 0;	// clear loc file
		if (tp_loadlocs.value && cl.deathmatch && !cls.demoplayback) {
			Q_snprintfz (locname, sizeof(locname), "%s.loc", host_mapname.string);
			TP_LoadLocFile (locname, true);
		}
		strlcpy (last_map, host_mapname.string, sizeof(last_map));
	}

	TP_ExecTrigger ("f_newmap");
}

/*
======================
TP_CategorizeMessage

returns a combination of these values:
0 -- unknown (probably generated by the server)
1 -- normal
2 -- team message
4 -- spectator
Note that sometimes we can't be sure who really sent the message,
e.g. when there's a player "unnamed" in your team and "(unnamed)"
in the enemy team. The result will be 3 (1+2)

Never returns 2 if we are a spectator.
======================
*/
int TP_CategorizeMessage (char *s, int *offset)
{
	int		i, msglen, len;
	int		flags;
	player_info_t	*player;
	char	*name;

	flags = 0;
	msglen = strlen(s);
	if (!msglen)
		return 0;

	*offset = 0;

	for (i=0, player=cl.players ; i < MAX_CLIENTS ; i++, player++)
	{
		if (!player->name[0])
			continue;
		name = Info_ValueForKey (player->userinfo, "name");
		len = strlen(name);
		// check messagemode1
		if (len+2 <= msglen && s[len] == ':' && s[len+1] == ' '	&&
			!strncmp(name, s, len))
		{
			if (player->spectator)
				flags |= 4;
			else
				flags |= 1;
			*offset = len + 2;
		}
		// check messagemode2
		else if (s[0] == '(' && !cl.spectator && len+4 <= msglen &&
			!strncmp(s+len+1, "): ", 3) &&
			!strncmp(name, s+1, len))
		{
			// no team messages in teamplay 0, except for our own
			if (i == cl.playernum[SP] || ( cl.teamplay &&
				!strcmp(cl.players[cl.playernum[SP]].team, player->team)) )
				flags |= 2;
			*offset = len + 4;
		}
	}

	return flags;
}

//===================================================================
// Pickup triggers
//

// symbolic names used in tp_took, tp_pickup, tp_point commands
char *pknames[] = {"quad", "pent", "ring", "suit", "ra", "ya",	"ga",
"mh", "health", "lg", "rl", "gl", "sng", "ng", "ssg", "pack",
"cells", "rockets", "nails", "shells", "flag", "pointed"};

#define it_quad		(1<<0)
#define it_pent		(1<<1)
#define it_ring		(1<<2)
#define it_suit		(1<<3)
#define it_ra		(1<<4)
#define it_ya		(1<<5)
#define it_ga		(1<<6)
#define it_mh		(1<<7)
#define it_health	(1<<8)
#define it_lg		(1<<9)
#define it_rl		(1<<10)
#define it_gl		(1<<11)
#define it_sng		(1<<12)
#define it_ng		(1<<13)
#define it_ssg		(1<<14)
#define it_pack		(1<<15)
#define it_cells	(1<<16)
#define it_rockets	(1<<17)
#define it_nails	(1<<18)
#define it_shells	(1<<19)
#define it_flag		(1<<20)
#define it_pointed	(1<<21)		// only valid for tp_took
#define NUM_ITEMFLAGS 22

#define it_powerups	(it_quad|it_pent|it_ring)
#define it_weapons	(it_lg|it_rl|it_gl|it_sng|it_ng|it_ssg)
#define it_armor	(it_ra|it_ya|it_ga)
#define it_ammo		(it_cells|it_rockets|it_nails|it_shells)

#define default_pkflags (it_powerups|it_suit|it_armor|it_weapons|it_mh| \
				it_rockets|it_pack|it_flag)

#define default_tookflags (it_powerups|it_ra|it_ya|it_lg|it_rl|it_mh|it_flag|it_pointed)

#define default_pointflags (it_powerups|it_suit|it_armor|it_mh| \
				it_lg|it_rl|it_gl|it_sng|it_rockets|it_pack|it_flag)

int pkflags = default_pkflags;
int tookflags = default_tookflags;
int pointflags = default_pointflags;


static void FlagCommand (int *flags, int defaultflags)
{
	int		i, j, c;
	char	*p;
	char	str[255] = "";
	qbool	removeflag = false;
	int		flag;
	
	c = Cmd_Argc ();
	if (c == 1)
	{
		if (!*flags)
			strcpy (str, "nothing");
		for (i=0 ; i<NUM_ITEMFLAGS ; i++)
			if (*flags & (1<<i))
			{
				if (*str)
					strcat (str, " ");
				strcat (str, pknames[i]);
			}
		Com_Printf ("%s\n", str);
		return;
	}

	if (*Cmd_Argv(1) != '+' && *Cmd_Argv(1) != '-')
		*flags = 0;

	for (i=1 ; i<c ; i++)
	{
		p = Cmd_Argv (i);
		if (*p == '+') {
			removeflag = false;
			p++;
		} else if (*p == '-') {
			removeflag = true;
			p++;
		}

		flag = 0;
		for (j=0 ; j<NUM_ITEMFLAGS ; j++) {
			if ((1<<j) == it_pointed && defaultflags != default_tookflags /* FIXME FIXME */)
				continue;
			if (!Q_strnicmp (p, pknames[j], 3)) {
				flag = 1<<j;
				break;
			}
		}

		if (!flag) {
			if (!Q_stricmp (p, "armor"))
				flag = it_ra|it_ya|it_ga;
			else if (!Q_stricmp (p, "weapons"))
				flag = it_lg|it_rl|it_gl|it_sng|it_ng|it_ssg;
			else if (!Q_stricmp (p, "powerups"))
				flag = it_quad|it_pent|it_ring;
			else if (!Q_stricmp (p, "ammo"))
				flag = it_cells|it_rockets|it_nails|it_shells;
			else if (!Q_stricmp (p, "default"))
				flag = defaultflags;
			else if (!Q_stricmp (p, "all")){
				flag = (1<<NUM_ITEMFLAGS)-1;
				if (defaultflags != default_tookflags /* FIXME FIXME */)
					flag &= ~it_pointed;
			}
		}

		if (removeflag)
			*flags &= ~flag;
		else
			*flags |= flag;
	}
}

void TP_Took_f (void)
{
	FlagCommand (&tookflags, default_tookflags);
}

void TP_Pickup_f (void)
{
	FlagCommand (&pkflags, default_pkflags);
}

void TP_Point_f (void)
{
	FlagCommand (&pointflags, default_pointflags);
}


/*
// FIXME: maybe use sound indexes so we don't have to make strcmp's
// every time?

#define S_LOCK4		1	// weapons/lock4.wav
#define S_PKUP		2	// weapons/pkup.wav
#define S_HEALTH25	3	// items/health1.wav
#define S_HEALTH15	4	// items/r_item1.wav
#define S_MHEALTH	5	// items/r_item2.wav
#define S_DAMAGE	6	// items/damage.wav
#define S_EYES		7	// items/inv1.wav
#define S_PENT		8	// items/protect.wav
#define S_ARMOR		9	// items/armor1.wav

static char *tp_soundnames[] =
{
	"weapons/lock4.wav",
	"weapons/pkup.wav",
	"items/health1.wav",
	"items/r_item1.wav",
	"items/r_item2.wav",
	"items/damage.wav",
	"items/inv1.wav",
	"items/protect.wav"
	"items/armor1.wav"
};

#define TP_NUMSOUNDS (sizeof(tp_soundnames)/sizeof(tp_soundnames[0]))

int	sound_numbers[MAX_SOUNDS];

void TP_FindSoundNumbers (void)
{
	int		i, j;
	char	*s;
	for (i=0 ; i<MAX_SOUNDS ; i++)
	{
		s = &cl.sound_name[i];
		for (j=0 ; j<TP_NUMSOUNDS ; j++)
			...
	}
}
*/

typedef struct {
	int		itemflag;
	cvar_t	*cvar;
	char	*modelname;
	vec3_t	offset;		// offset of model graphics center
	float	radius;		// model graphics radius
	int		flags;		// TODO: "NOPICKUP" (disp), "TEAMENEMY" (flag, disp)
} item_t;

item_t	tp_items[] = {
	{	it_quad,	&tp_name_quad,	"progs/quaddama.mdl",
		{0, 0, 24},	25,
	},
	{	it_pent,	&tp_name_pent,	"progs/invulner.mdl",
		{0, 0, 22},	25,
	},
	{	it_ring,	&tp_name_ring,	"progs/invisibl.mdl",
		{0, 0, 16},	12,
	},
	{	it_suit,	&tp_name_suit,	"progs/suit.mdl",
		{0, 0, 24}, 20,
	},
	{	it_lg,		&tp_name_lg,	"progs/g_light.mdl",
		{0, 0, 30},	20,
	},
	{	it_rl,		&tp_name_rl,	"progs/g_rock2.mdl",
		{0, 0, 30},	20,
	},
	{	it_gl,		&tp_name_gl,	"progs/g_rock.mdl",
		{0, 0, 30},	20,
	},
	{	it_sng,		&tp_name_sng,	"progs/g_nail2.mdl",
		{0, 0, 30},	20,
	},
	{	it_ng,		&tp_name_ng,	"progs/g_nail.mdl",
		{0, 0, 30},	20,
	},
	{	it_ssg,		&tp_name_ssg,	"progs/g_shot.mdl",
		{0, 0, 30},	20,
	},
	{	it_cells,	&tp_name_cells,	"maps/b_batt0.bsp",
		{16, 16, 24},	18,
	},
	{	it_cells,	&tp_name_cells,	"maps/b_batt1.bsp",
		{16, 16, 24},	18,
	},
	{	it_rockets,	&tp_name_rockets,"maps/b_rock0.bsp",
		{8, 8, 20},	18,
	},
	{	it_rockets,	&tp_name_rockets,"maps/b_rock1.bsp",
		{16, 8, 20},	18,
	},
	{	it_nails,	&tp_name_nails,	"maps/b_nail0.bsp",
		{16, 16, 10},	18,
	},
	{	it_nails,	&tp_name_nails,	"maps/b_nail1.bsp",
		{16, 16, 10},	18,
	},
	{	it_shells,	&tp_name_shells,"maps/b_shell0.bsp",
		{16, 16, 10},	18,
	},
	{	it_shells,	&tp_name_shells,"maps/b_shell1.bsp",
		{16, 16, 10},	18,
	},
	{	it_health,	&tp_name_health,"maps/b_bh10.bsp",
		{16, 16, 8},	18,
	},
	{	it_health,	&tp_name_health,"maps/b_bh25.bsp",
		{16, 16, 8},	18,
	},
	{	it_mh,		&tp_name_mh,	"maps/b_bh100.bsp",
		{16, 16, 14},	20,
	},
	{	it_pack,	&tp_name_backpack, "progs/backpack.mdl",
		{0, 0, 18},	18,
	},
	{	it_flag,	&tp_name_flag,	"progs/tf_flag.mdl",
		{0, 0, 14},	25,
	},
	{	it_flag,	&tp_name_flag,	"progs/tf_stan.mdl",
		{0, 0, 45},	40,
	},
	{	it_ra|it_ya|it_ga, NULL,	"progs/armor.mdl",
		{0, 0, 24},	22,
	}

};

#define NUMITEMS (sizeof(tp_items) / sizeof(tp_items[0]))

item_t	*model2item[MAX_MODELS];

void TP_FindModelNumbers (void)
{
	int		i, j;
	char	*s;
	item_t	*item;

	for (i=0 ; i<MAX_MODELS ; i++) {
		model2item[i] = NULL;
		s = cl.model_name[i];
		if (!s)
			continue;
		for (j=0, item=tp_items ; j<NUMITEMS ; j++, item++)
			if (!strcmp(s, item->modelname))
				model2item[i] = item;
	}
}


// on success, result is non-zero
// on failure, result is zero
// for armors, returns skinnum+1 on success
static int FindNearestItem (int flags, item_t **pitem)
{
	frame_t		*frame;
	packet_entities_t	*pak;
	entity_state_t		*ent;
	int	i = 0, bestidx = 0, bestskin = 0;
	float bestdist = 0.0, dist = 0.0;
	vec3_t	org, v;
	item_t	*item;

	VectorCopy (cl.frames[cl.validsequence&UPDATE_MASK]
		.playerstate[cl.playernum[SP]].origin, org);

	// look in previous frame 
	frame = &cl.frames[cl.oldvalidsequence&UPDATE_MASK];
	pak = &frame->packet_entities;
	bestdist = 100.0f;
	bestidx = 0;
	*pitem = NULL;
	for (i=0,ent=pak->entities ; i<pak->num_entities ; i++,ent++)
	{
		item = model2item[ent->modelindex];
		if (!item)
			continue;
		if ( ! (item->itemflag & flags) )
			continue;

		VectorCopy(ent->origin, v);
		VectorSubtract (v, org, v);
		VectorAdd (v, item->offset, v);
		dist = VectorLength (v);
//		Com_Printf ("%s %f\n", item->modelname, dist);

		if (dist <= bestdist) {
			bestdist = dist;
			bestidx = ent->modelindex;
			bestskin = ent->skinnum;
			*pitem = item;
		}
	}

	if (bestidx && (*pitem)->itemflag == it_armor)
		return bestskin + 1;	// 1=green, 2=yellow, 3=red

	return bestidx;
}


static int CountTeammates (void)
{
	int	i, count;
	player_info_t	*player;
	char	*myteam;

	if (tp_forceTriggers.value)
		return 1;

	if (!cl.teamplay)
		return 0;

	count = 0;
	myteam = cl.players[cl.playernum[SP]].team;
	for (i=0, player=cl.players; i < MAX_CLIENTS ; i++, player++) {
		if (player->name[0] && !player->spectator && (i != cl.playernum[SP])
									&& !strcmp(player->team, myteam))
			count++;
	}

	return count;
}

static void ExecTookTrigger (char *s, int flag, vec3_t org)
{
	qbool	report;

	// decide whether this pickup should be reported
	if ( !((pkflags|tookflags) & flag) )
			return;

	vars.tooktime = realtime;
	strncpy (vars.tookname, s, sizeof(vars.tookname)-1);
	strncpy (vars.tookloc, TP_LocationName (org), sizeof(vars.tookloc)-1);

	if (flag & it_weapons) {
		if (cl.deathmatch == 2 || cl.deathmatch == 3)
			return;
	}

	report = (tookflags & flag) ? true : false;

	if (!report) {
		if ((tookflags & it_pointed) && !strcmp(vars.pointname, s)) {
			vec3_t	dist;
			VectorSubtract (org, vars.pointorg, dist);
			//Com_DPrintf ("dist: %f\n", VectorLength(dist));
			if (VectorLength(dist) < 80) {		// tune this!
				// ok, this looks like the item we have pointed at
				report = true;
			}
		}
	}

	if (report && CountTeammates()) {
		TP_ExecTrigger ("f_took");
	}
}


void TP_CheckPickupSound (char *s, vec3_t org)
{
	if (cl.spectator)
		return;

	if (!strcmp(s, "items/damage.wav"))
		ExecTookTrigger (tp_name_quad.string, it_quad, org);
	else if (!strcmp(s, "items/protect.wav"))
		ExecTookTrigger (tp_name_pent.string, it_pent, org);
	else if (!strcmp(s, "items/inv1.wav"))
		ExecTookTrigger (tp_name_ring.string, it_ring, org);
	else if (!strcmp(s, "items/suit.wav"))
		ExecTookTrigger (tp_name_suit.string, it_suit, org);
	else if (!strcmp(s, "items/health1.wav") ||
			 !strcmp(s, "items/r_item1.wav"))
		ExecTookTrigger (tp_name_health.string, it_health, org);
	else if (!strcmp(s, "items/r_item2.wav"))
		ExecTookTrigger (tp_name_mh.string, it_mh, org);
	else
		goto more;
	return;

more:
	if (!cl.validsequence || !cl.oldvalidsequence)
		return;

	// weapons
	if (!strcmp(s, "weapons/pkup.wav"))
	{
		item_t	*item;
		if (FindNearestItem (it_weapons, &item)) {
			ExecTookTrigger (item->cvar->string, item->itemflag, org);
		}
		else {
			// we don't know what entity caused the sound, try to guess...
			if (vars.stat_framecounts[STAT_ITEMS] == cls.framecount) {
				if (vars.items & ~vars.olditems & IT_LIGHTNING)
					ExecTookTrigger (tp_name_lg.string, it_lg, cl.simorg[SP]);
				else if (vars.items & ~vars.olditems & IT_ROCKET_LAUNCHER)
					ExecTookTrigger (tp_name_rl.string, it_rl, cl.simorg[SP]);
				else if (vars.items & ~vars.olditems & IT_GRENADE_LAUNCHER)
					ExecTookTrigger (tp_name_gl.string, it_gl, cl.simorg[SP]);
				else if (vars.items & ~vars.olditems & IT_SUPER_NAILGUN)
					ExecTookTrigger (tp_name_sng.string, it_sng, cl.simorg[SP]);
				else if (vars.items & ~vars.olditems & IT_NAILGUN)
					ExecTookTrigger (tp_name_ng.string, it_ng, cl.simorg[SP]);
				else if (vars.items & ~vars.olditems & IT_SUPER_SHOTGUN)
					ExecTookTrigger (tp_name_ssg.string, it_ssg, cl.simorg[SP]);
			}
		}
		return;
	}

	// armor
	if (!strcmp(s, "items/armor1.wav"))	{
		item_t	*item;
		qbool armor_updated;
		int armortype;

		armor_updated = (vars.stat_framecounts[STAT_ARMOR] == cls.framecount);
		armortype = FindNearestItem (it_armor, &item);
		if (armortype == 1 || (!armortype && armor_updated && cl.stats[SP][STAT_ARMOR] == 100))
			ExecTookTrigger (tp_name_ga.string, it_ga, org);
		else if (armortype == 2 || (!armortype && armor_updated && cl.stats[SP][STAT_ARMOR] == 150))
			ExecTookTrigger (tp_name_ya.string, it_ya, org);
		else if (armortype == 3 || (!armortype && armor_updated && cl.stats[SP][STAT_ARMOR] == 200))
			ExecTookTrigger (tp_name_ra.string, it_ra, org);
		return;
	}

	if (!strcmp(s, "items/armor1.wav"))
	{
		item_t	*item;
		switch (FindNearestItem (it_armor, &item)) {
			case 1: ExecTookTrigger (tp_name_ga.string, it_ga, org); break;
			case 2: ExecTookTrigger (tp_name_ya.string, it_ya, org); break;
			case 3: ExecTookTrigger (tp_name_ra.string, it_ra, org); break;
		}
		return;
	}

	// backpack or ammo
	if (!strcmp (s, "weapons/lock4.wav"))
	{
		item_t	*item;
		if (!FindNearestItem (it_ammo|it_pack, &item))
			return;
		ExecTookTrigger (item->cvar->string, item->itemflag, org);
	}
}


void TP_FindPoint (void)
{
	packet_entities_t	*pak;
	entity_state_t		*ent;
	int	i;
	vec3_t	forward, right, up;
	float	best = 0.0;
	entity_state_t	*bestent = NULL;
	vec3_t	ang;
	vec3_t	vieworg, entorg;
	item_t	*item = NULL, *bestitem = NULL;

	ang[0] = cl.viewangles[SP][0];
	ang[1] = cl.viewangles[SP][1];
	ang[2] = 0;
	AngleVectors (ang, forward, right, up);
	VectorCopy (cl.simorg[SP], vieworg);
	vieworg[2] += 22;	// adjust for view height

	if (!cl.validsequence)
		goto nothing;

	best = -1;

	pak = &cl.frames[cl.validsequence&UPDATE_MASK].packet_entities;
	for (i=0,ent=pak->entities ; i<pak->num_entities ; i++,ent++)
	{
		vec3_t	v, v2, v3;
		float dist, miss, rank;

		item = model2item[ent->modelindex];
		if (!item)
			continue;
		if (! (item->itemflag & pointflags) )
			continue;
		// special check for armors
		if (item->itemflag == (it_ra|it_ya|it_ga)) {
			switch (ent->skinnum) {
				case 0: if (!(pointflags & it_ga)) continue; break;
				case 1: if (!(pointflags & it_ya)) continue; break;
				default: if (!(pointflags & it_ra)) continue;
			}
		}

		VectorCopy(ent->origin, entorg);
		VectorAdd (entorg, item->offset, entorg);
		VectorSubtract (entorg, vieworg, v);

		dist = DotProduct (v, forward);
		if (dist < 10)
			continue;
		VectorScale (forward, dist, v2);
		VectorSubtract (v2, v, v3);
		miss = VectorLength (v3);
		if (miss > 300)
			continue;
		if (miss > dist*1.7)
			continue;		// over 60 degrees off
		if (dist < 3000.0/8.0)
			rank = miss * (dist*8.0*0.0002f + 0.3f);
		else
			rank = miss;
		
		if (rank < best || best < 0) {
			// check if we can actually see the object
			vec3_t	end;
			trace_t	trace;
			float	radius;

			radius = item->radius;
			if (ent->effects & (EF_BLUE|EF_RED|EF_DIMLIGHT|EF_BRIGHTLIGHT))
				radius = 200;

			if (dist <= radius)
				goto ok;

			// FIXME: is it ok to use PM_TraceLine here?
			// physent list might not have been built yet...

			VectorSubtract (vieworg, entorg, v);
			VectorNormalize (v);
			VectorMA (entorg, radius, v, end);
			trace = PM_PlayerTrace (vieworg, end);
			if (trace.fraction == 1)
				goto ok;

			VectorMA (entorg, radius, right, end);
			VectorSubtract (vieworg, end, v);
			VectorNormalize (v);
			VectorMA (end, radius, v, end);
			trace = PM_PlayerTrace (vieworg, end);
			if (trace.fraction == 1)
				goto ok;

			VectorMA (entorg, -radius, right, end);
			VectorSubtract (vieworg, end, v);
			VectorNormalize (v);
			VectorMA (end, radius, v, end);
			trace = PM_PlayerTrace (vieworg, end);
			if (trace.fraction == 1)
				goto ok;

			VectorMA (entorg, radius, up, end);
			VectorSubtract (vieworg, end, v);
			VectorNormalize (v);
			VectorMA (end, radius, v, end);
			trace = PM_PlayerTrace (vieworg, end);
			if (trace.fraction == 1)
				goto ok;

			// use half the radius, otherwise it's possible to see
			// through floor in some places
			VectorMA (entorg, -radius/2, up, end);
			VectorSubtract (vieworg, end, v);
			VectorNormalize (v);
			VectorMA (end, radius, v, end);
			trace = PM_PlayerTrace (vieworg, end);
			if (trace.fraction == 1)
				goto ok;

			continue;	// not visible
ok:
			best = rank;
			bestent = ent;
			bestitem = item;
		}
	}

	if (best >= 0) {
		char *p;
		if (!bestitem->cvar) {
			// armors are special
			switch (bestent->skinnum) {
				case 0: p = tp_name_ga.string; break;
				case 1: p = tp_name_ya.string; break;
				default: p = tp_name_ra.string;
			}
		} else
			p = bestitem->cvar->string;

		strlcpy (vars.pointname, p, sizeof(vars.pointname));
		VectorCopy (bestent->origin, entorg);
		strlcpy (vars.pointloc, TP_LocationName (entorg), sizeof(vars.pointloc));
		VectorCopy (entorg, vars.pointorg);
	}
	else {
nothing:
		strlcpy (vars.pointname, tp_name_nothing.string, sizeof(vars.pointname));
		vars.pointloc[0] = 0;
	}

	vars.pointframe = cls.framecount;
}


void TP_StatChanged (int stat, int value)
{
	int		i;

	if (stat == STAT_HEALTH)
	{
		if (value > 0) {
			if (vars.health <= 0) {
				// we just respawned
				vars.respawntrigger_time = realtime;

				if (!cl.spectator && CountTeammates())
					TP_ExecTrigger ("f_respawn");
			}
			vars.health = value;
			return;
		}
		if (vars.health > 0) {		// We have just died
			vars.deathtrigger_time = realtime;
			strcpy (vars.lastdeathloc, Macro_Location());
			if (!cl.spectator && CountTeammates()) {
				if (cl.teamfortress && (cl.stats[SP][STAT_ITEMS] & (IT_KEY1|IT_KEY2))
					&& Cmd_AliasExist("f_flagdeath", RESTRICT_LOCAL))
					TP_ExecTrigger ("f_flagdeath");
				else
					TP_ExecTrigger ("f_death");
			}
		}
		vars.health = value;
	}
	else if (stat == STAT_ITEMS)
	{
		i = value &~ vars.items;

		if (i & (IT_KEY1|IT_KEY2)) {
			if (cl.teamfortress && !cl.spectator)
				ExecTookTrigger (tp_name_flag.string, it_flag,
				cl.frames[cl.validsequence&UPDATE_MASK].playerstate[cl.playernum[SP]].origin);
		}

		vars.olditems = vars.items;
		vars.items = value;
	}
	else if (stat == STAT_ACTIVEWEAPON)
	{
		if (cl.stats[SP][STAT_ACTIVEWEAPON] != vars.activeweapon)
			TP_ExecTrigger ("f_weaponchange");
		vars.activeweapon = cl.stats[SP][STAT_ACTIVEWEAPON];
	}

	vars.stat_framecounts[stat] = cls.framecount;
}


/*
======================
TP_CheckSoundTrigger

Find and execute sound triggers.
A sound trigger must be terminated by either a CR or LF.
Returns true if a sound was found and played
======================
*/
qbool TP_CheckSoundTrigger (char *str)
{
	int		i, j;
	int		start, length;
	char	soundname[MAX_OSPATH];
	FILE	*f;

	if (!tp_soundtrigger.string[0])
		return false;

	for (i=strlen(str)-1 ; i ; i--)
	{
		if (str[i] != 0x0A && str[i] != 0x0D)
			continue;

		for (j = i-1 ; j >= 0 ; j--)
		{
			// quick check for chars that cannot be used
			// as sound triggers but might be part of a file name
			if ( isalpha((int)(unsigned char)str[j]) ||
					 isdigit((int)(unsigned char)str[j]) )
				continue;	// file name or chat

			if (strchr(tp_soundtrigger.string, str[j]))
			{
				// this might be a sound trigger

				start = j + 1;
				length = i - start;

				if (!length)
					break;
				if (length >= MAX_QPATH)
					break;

				strlcpy (soundname, str + start, length + 1);
				if (strstr(soundname, ".."))
					break;	// no thank you

				// clean up the message
				strcpy (str + j, str + i);

				if (!snd_initialized)
					return false;

				COM_DefaultExtension (soundname, ".wav");

				// make sure we have it on disk (FIXME)
				COM_FOpenFile (va("sound/%s", soundname), &f);
				if (!f)
					return false;
				fclose (f);

				// now play the sound
				S_LocalSound (soundname);
				return true;
			}

			if (str[j] <= ' ' || strchr("\"&'*,:;<>?\\|\x7f", str[j]))
				break;	// we don't allow these in a file name
		}
	}

	return false;
}


#define MAX_FILTER_LENGTH 4
char filter_strings[8][MAX_FILTER_LENGTH+1];
int	num_filters = 0;

/*
======================
TP_FilterMessage

returns false if the message shouldn't be printed
matching filters are stripped from the message
======================
*/
qbool TP_FilterMessage (char *s)
{
	int i, j, len, maxlen;

	if (!num_filters)
		return true;

	len = strlen (s);
	if (len < 2 || s[len-1] != '\n' || s[len-2] == '#')
		return true;

	maxlen = MAX_FILTER_LENGTH + 1;
	for (i=len-2 ; i >= 0 && maxlen > 0 ; i--, maxlen--) {
		if (s[i] == ' ')
			return true;
		if (s[i] == '#')
			break;
	}
	if (i < 0 || !maxlen)
		return true;	// no filter at all

	s[len-1] = 0;	// so that strcmp works properly

	for (j=0 ; j<num_filters ; j++)
		if (!strcmp(s + i + 1, filter_strings[j]))
		{
			// strip the filter from message
			if (i && s[i-1] == ' ')
			{	// there's a space just before the filter, remove it
				// so that soundtriggers like ^blah #att work 
				s[i-1] = '\n';
				s[i] = 0;
			} else {
				s[i] = '\n';
				s[i+1] = 0;
			}
			return true;
		}

	s[len-1] = '\n';
	return false;	// this message is not for us, don't print it
}

void TP_MsgFilter_f (void)
{
	int c, i;
	char *s;

	c = Cmd_Argc ();
	if (c == 1) {
		if (!num_filters) {
			Com_Printf ("No filters defined\n");
			return;
		}
		for (i=0 ; i<num_filters ; i++)
			Com_Printf ("%s#%s", i ? " " : "", filter_strings[i]);
		Com_Printf ("\n");
		return;
	}

	if (c == 2 && (Cmd_Argv(1)[0] == 0 || !strcmp(Cmd_Argv(1), "clear"))) {
		num_filters = 0;
		return;
	}

	num_filters = 0;
	for (i=1 ; i < c ; i++) {
		s = Cmd_Argv(i);
		if (*s != '#') {
			Com_Printf ("A filter must start with \"#\"\n");
			return;
		}
		if (strchr(s+1, ' ')) {
			Com_Printf ("A filter may not contain spaces\n");
			return;
		}
		strlcpy (filter_strings[num_filters], s+1, sizeof(filter_strings[0]));
		num_filters++;
		if (num_filters >= 8)
			break;
	}
}

void TP_Init (void)
{
#define TEAMPLAYVARS	"Teamplay Variables"

	Cvar_Register (&cl_parseFunChars,	TEAMPLAYVARS);
	Cvar_Register (&cl_parseSay,		TEAMPLAYVARS);
	Cvar_Register (&cl_triggers,		TEAMPLAYVARS);
	Cvar_Register (&tp_forceTriggers,	TEAMPLAYVARS);
	Cvar_Register (&tp_loadlocs,		TEAMPLAYVARS);
	Cvar_Register (&cl_teamskin,		TEAMPLAYVARS);
	Cvar_Register (&cl_enemyskin,		TEAMPLAYVARS);
	Cvar_Register (&tp_soundtrigger,	TEAMPLAYVARS);
	Cvar_Register (&tp_name_axe,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_sg,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_ssg,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_ng,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_sng,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_gl,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_rl,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_lg,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_ra,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_ya,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_ga,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_quad,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_pent,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_ring,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_suit,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_shells,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_nails,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_rockets,	TEAMPLAYVARS);
	Cvar_Register (&tp_name_cells,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_mh,			TEAMPLAYVARS);
	Cvar_Register (&tp_name_health,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_backpack,	TEAMPLAYVARS);
	Cvar_Register (&tp_name_flag,		TEAMPLAYVARS);
	Cvar_Register (&tp_name_nothing,	TEAMPLAYVARS);
	Cvar_Register (&tp_name_someplace,	TEAMPLAYVARS);
	Cvar_Register (&tp_name_at,			TEAMPLAYVARS);
	Cvar_Register (&tp_need_ra,			TEAMPLAYVARS);
	Cvar_Register (&tp_need_ya,			TEAMPLAYVARS);
	Cvar_Register (&tp_need_ga,			TEAMPLAYVARS);
	Cvar_Register (&tp_need_health,		TEAMPLAYVARS);
	Cvar_Register (&tp_need_weapon,		TEAMPLAYVARS);
	Cvar_Register (&tp_need_rl,			TEAMPLAYVARS);
	Cvar_Register (&tp_need_rockets,	TEAMPLAYVARS);
	Cvar_Register (&tp_need_cells,		TEAMPLAYVARS);
	Cvar_Register (&tp_need_nails,		TEAMPLAYVARS);
	Cvar_Register (&tp_need_shells,		TEAMPLAYVARS);

	Cvar_Register (&cl_fakename,		TEAMPLAYVARS);

	Cmd_AddCommand ("loadloc", TP_LoadLocFile_f);
	Cmd_AddCommand ("filter", TP_MsgFilter_f);
	Cmd_AddCommand ("msg_trigger", TP_MsgTrigger_f);
	Cmd_AddCommand ("teamcolor", TP_TeamColor_f);
	Cmd_AddCommand ("enemycolor", TP_EnemyColor_f);
	Cmd_AddCommand ("tp_took", TP_Took_f);
	Cmd_AddCommand ("tp_pickup", TP_Pickup_f);
	Cmd_AddCommand ("tp_point", TP_Point_f);

	TP_InitMacros();
}









void CL_Say (qboolean team)
{
	extern cvar_t cl_fakename;
	char	text[1024], sendtext[1024], *s;

	if (Cmd_Argc() < 2)
	{
		if (team)
			Con_Printf ("%s <text>: send a team message\n", Cmd_Argv(0));
		return;
	}

	if (cls.state == ca_disconnected)
	{
		Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, team ? "say_team " : "say ");

	s = TP_ParseMacroString (Cmd_Args());
	Q_strncpyz (text, TP_ParseFunChars (s, true), sizeof(text));

	sendtext[0] = 0;
	if (team && !cl.spectator && cl_fakename.string[0] &&
		!strchr(s, '\x0d') /* explicit $\ in message overrides cl_fakename */)
	{
		char buf[1024];
		Cmd_ExpandString (cl_fakename.string, buf, sizeof(buf), Cmd_ExecLevel);
		strcpy (buf, TP_ParseMacroString (buf));
		Q_snprintfz (sendtext, sizeof(sendtext), "\x0d%s: ", TP_ParseFunChars(buf, true));
	}

	strlcat (sendtext, text, sizeof(sendtext));

	if (sendtext[0] < 32)
		SZ_Print (&cls.netchan.message, "\"");	// add quotes so that old servers parse the message correctly

	SZ_Print (&cls.netchan.message, sendtext);

	if (sendtext[0] < 32)
		SZ_Print (&cls.netchan.message, "\"");	// add quotes so that old servers parse the message correctly
}


void CL_Say_f (void)
{
	CL_Say (false);
}

void CL_SayTeam_f (void)
{
	CL_Say (true);
}
#endif


