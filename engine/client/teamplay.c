/*

	Teamplay.c
	Contains various console stuff for improving the performance of a team...
	Cheats I hear you say?...

	Personally, I'd rather call it a hack job.
	Most of it is dependant upon specific mod types - TF.


	As far as split screen goes, this is all relative to player 0. We don't provide more than one say command.
*/

#include "quakedef.h"

#ifndef ZQUAKETEAMPLAY

cvar_t tp_name_armortype_ga	= {"tp_name_armortype_ga", "g"};
cvar_t tp_name_armortype_ya	= {"tp_name_armortype_ya", "y"};
cvar_t tp_name_armortype_ra	= {"tp_name_armortype_ra", "r"};
cvar_t tp_name_none			= {"tp_name_none", ""};


#define translatetext(i) #i



///////////////////////////////////////////////////////////////////
//Macros.

char *TP_ClassForTFSkin(void)
{
	char *skin;
	skin = Info_ValueForKey(cls.userinfo, "skin");
	if (!*skin)
		return "Classless";
	if (skin[0] != 't' && skin[1] != 'f' && skin[2] != '_')
		return skin;
	if (!strcmp(skin, "tf_sold"))
		return translatetext(TLTP_CLASS_SOLIDER);
	if (!strcmp(skin, "tf_demo"))
		return translatetext(TLTP_CLASS_DEMOGUY);
	if (!strcmp(skin, "tf_eng"))
		return translatetext(TLTP_CLASS_ENGINEER);
	if (!strcmp(skin, "tf_snipe"))
		return translatetext(TLTP_CLASS_SNIPER);
	if (!strcmp(skin, "tf_hwguy"))
		return translatetext(TLTP_CLASS_HWGUY);
	if (!strcmp(skin, "tf_medic"))
		return translatetext(TLTP_CLASS_MEDIC);
	if (!strcmp(skin, "tf_pyro"))
		return translatetext(TLTP_CLASS_PYRO);
	if (!strcmp(skin, "tf_scout"))
		return translatetext(TLTP_CLASS_SCOUT);
	if (!strcmp(skin, "tf_spy"))
		return translatetext(TLTP_CLASS_SPY);

	return skin;
}

void *TP_ArmourType(void)
{
	if (cl.stats[0][STAT_ITEMS] & IT_ARMOR1)
		return tp_name_armortype_ga.string;
	else if (cl.stats[0][STAT_ITEMS] & IT_ARMOR2)
		return tp_name_armortype_ya.string;
	else if (cl.stats[0][STAT_ITEMS] & IT_ARMOR3)
		return tp_name_armortype_ra.string;
	else
		return tp_name_none.string;
}







///////////////////////////////////////////////////////////////////
//Locs

typedef struct location_s {
	vec3_t pos;
	struct location_s *next;
	char name[0];
} location_t;
location_t *location;

char LocationLevel[64];

void CL_LoadLocs(void)
{
	location_t *newloc;
	vec3_t pos;

	char *file;
	char *end;
	char name[MAX_QPATH];
//	if (!strcmp(LocationLevel, cl.model_name[1]))
//		return;

	while(location)
	{
		newloc = location->next;
		Z_Free(location);
		location = newloc;
	}

	strcpy(LocationLevel, cl.model_name[1]);

	COM_StripExtension(COM_SkipPath(LocationLevel), name);
	file = COM_LoadTempFile(va("locs/%s.loc", name));

	if (!file)
		return;
	for(;;)
	{
		file = COM_Parse(file);
		pos[0] = atof(com_token)/8;
		file = COM_Parse(file);
		pos[1] = atof(com_token)/8;
		file = COM_Parse(file);
		pos[2] = atof(com_token)/8;

		while(*file && *file <= '\0')
			file++;

		if (!file)
			return;
		end = strchr(file, '\n');	
		if (!end)
		{
			end = file + strlen(file);
		}
		newloc = Z_Malloc(sizeof(location_t) + end-file+1);
		newloc->next = location;
		location = newloc;

		Q_strncpyz(newloc->name, file, end-file);
		VectorCopy(pos, newloc->pos);


		if (!*end)
			return;
		file = end+1;
	}
}

char *CL_LocationName(float *pos)
{
	location_t *loc;
	vec3_t dir;
	char *best;
	float dist, bestdist;

	CL_LoadLocs();

	if (!location)
		return "somewhere";

	//get the initial one
	best = location->name;
	VectorSubtract(location->pos, pos, dir);
	bestdist = VectorNormalize(dir);

	//check for a closer one.
	for (loc = location->next; loc; loc=loc->next)
	{
		VectorSubtract(loc->pos, pos, dir);
		dist = VectorNormalize(dir);
		if (dist < bestdist)
		{
			best = loc->name;
			bestdist = dist;
		}
	}

	return best;
}



//////////////////////////////////////////////////////////
//Commands
#define INVIS_CHAR1 12
#define INVIS_CHAR2 138
#define INVIS_CHAR3 160

#ifdef ZQUAKETEAMPLAY
/*
===============
CL_Say

Handles both say and say_team
===============
*/
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
		Cmd_ExpandString (cl_fakename.string, buf);
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

#else
void CL_Say_f (void)
{
	char string[256];
	char *msg;
	int c;
	if (cls.state == ca_disconnected || cls.demoplayback)
	{
#ifndef CLIENT_ONLY
		if (sv.state)
			SV_ConSay_f();
		else
#endif
			Con_TPrintf (TL_CANTXNOTCONNECTED, Cmd_Argv(0));
		return;
	}

	if (!strcmp("sayone", Cmd_Argv(0)))
	{
		if (strcmp(Info_ValueForKey(cl.serverinfo, "*distrib"), DISTRIBUTION) || atoi(Info_ValueForKey(cl.serverinfo, "*ver")) < PRE_SAYONE)
		{
			Con_TPrintf (TLC_REQUIRESSERVERMOD, Cmd_Argv(0));
			return;
		}
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	Q_strncpyz(string, Cmd_Argv(0), sizeof(string));
	for (msg = string; *msg; msg++)
		if (*msg >= 'A' && *msg <= 'Z')
			*msg = *msg - 'A' + 'a';
	SZ_Print (&cls.netchan.message, string);
	cls.netchan.message.cursize--;

	msg = Cmd_Args();

	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message," ");
		cls.netchan.message.cursize--;
		MSG_WriteChar(&cls.netchan.message, '\"');
		while(*msg)
		{
			c = *msg;

			if (c == '%')
			{
				char *message = NULL;
				msg++;

				if (message == NULL)
				switch(*msg)
				{
				case 'n':
					SZ_Print(&cls.netchan.message, name.string);
					cls.netchan.message.cursize--;
					msg++;
					continue;
				case 'h':
					SZ_Print(&cls.netchan.message, va("%i", cl.stats[0][STAT_HEALTH]));
					cls.netchan.message.cursize--;	//remove the null term
					msg++;
					continue;
				case 'a':
					SZ_Print(&cls.netchan.message, va("%i", cl.stats[0][STAT_ARMOR]));
					cls.netchan.message.cursize--;
					msg++;
					continue;
				case 'A':
					SZ_Print(&cls.netchan.message, TP_ArmourType());
					cls.netchan.message.cursize--;
					msg++;
					continue;
				case 'l':
					SZ_Print(&cls.netchan.message, CL_LocationName(r_refdef.vieworg));
					cls.netchan.message.cursize--;
					msg++;
					continue;
				case 'S':
					SZ_Print(&cls.netchan.message, TP_ClassForTFSkin());
					cls.netchan.message.cursize--;
					msg++;
					continue;
				case '\0':	//whoops.
					MSG_WriteChar(&cls.netchan.message, '%');
					MSG_WriteChar(&cls.netchan.message, '\"');
					MSG_WriteChar(&cls.netchan.message, *msg);
					return;
				case '%':
					c  = '%';
					break;
				default:
					c  = '%';
					msg--;
					break;
				}
			}
			else if (c == '$')
			{
				msg++;
				switch(*msg)
				{
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
				case 'x':
					c = INVIS_CHAR1;
					break;
				case 'y':
					c = INVIS_CHAR2;
					break;
				case 'z':
					c = INVIS_CHAR3;
					break;
				default:
					msg--;
					break;
				}

			}
		
			MSG_WriteChar(&cls.netchan.message, c);

			msg++;
		}
		MSG_WriteChar(&cls.netchan.message, '\"');
	}
	
	MSG_WriteChar(&cls.netchan.message, '\0');
}

#endif




qboolean TP_SoundTrigger(char *message)	//if there is a trigger there, play it. Return true if we found one, stripping off the file (it's neater that way).
{
	char *strip;
	char *lineend = NULL;
	char soundname[128];
	int filter = 0;

	for (strip = message+strlen(message)-1; *strip && strip >= message; strip--)
	{
		if (*strip == '#')
			filter++;
		if (*strip == ':')
			break;	//if someone says just one word, we can take any tidles in thier name to be a voice command
		if (*strip == '\n')
			lineend = strip;
		else if (*strip <= ' ')
		{
			if (filter == 0 || filter == 1)	//allow one space in front of a filter.
			{
				filter++;
				continue;
			}
			break;
		}
		else if (*strip == '~')
		{
			//looks like a trigger, whoopie!
			if (lineend-strip > sizeof(soundname)-1)
			{
				Con_Printf("Sound trigger's file-name was too long\n");
				return false;
			}
			Q_strncpyz(soundname, strip+1, lineend-strip);
			memmove(strip, lineend, strlen(lineend)+1);

			Cbuf_AddText(va("play %s\n", soundname), RESTRICT_LOCAL);
			return true;
		}
	}
	return false;
}

void TP_Init(void)
{
}

void TP_CheckPickupSound(char *s, vec3_t org)
{
}


#endif
