/*

	Teamplay.c
	Contains various console stuff for improving the performance of a team...
	Cheats I hear you say?...

	Personally, I'd rather call it a hack job.
	Most of it is dependant upon specific mod types - TF.


	As far as split screen goes, this is all relative to player 0. We don't provide more than one say command.
*/

#include "quakedef.h"

#if 0 //ndef ZQUAKETEAMPLAY

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

/*
===============
CL_Say

Handles both say and say_team
===============
*/

void CL_Say_f (void)
{
	char output[8192];
	char string[256];
	char *msg;
	int c;
	output[0] = '\0';
	if (cls.state == ca_disconnected || cls.demoplayback)
	{
#ifndef CLIENT_ONLY
		if (sv.state)
			SV_ConSay_f();
		else
#endif
			Con_TPrintf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (!strcmp("sayone", Cmd_Argv(0)))
	{
		if (strcmp(Info_ValueForKey(cl.serverinfo, "*distrib"), DISTRIBUTION) || atoi(Info_ValueForKey(cl.serverinfo, "*ver")) < PRE_SAYONE)
		{
			Con_Printf ("%s is only available with server support\n", Cmd_Argv(0));
			return;
		}
	}

	Q_strncpyz(output, Cmd_Argv(0), sizeof(string));
	for (msg = output; *msg; msg++)
		if (*msg >= 'A' && *msg <= 'Z')
			*msg = *msg - 'A' + 'a';

	msg = Cmd_Args();

	if (Cmd_Argc() > 1)
	{
		Q_strncatz(output, " \"", sizeof(output));

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
					Q_strncatz(output, name.string, sizeof(output));
					msg++;
					continue;
				case 'h':
					Q_strncatz(output, va("%i", cl.stats[0][STAT_HEALTH]), sizeof(output));
					msg++;
					continue;
				case 'a':
					Q_strncatz(output, va("%i", cl.stats[0][STAT_ARMOR]), sizeof(output));
					msg++;
					continue;
				case 'A':
					Q_strncatz(output, TP_ArmourType(), sizeof(output));
					msg++;
					continue;
				case 'l':
					Q_strncatz(output, CL_LocationName(cl.simorg[0]), sizeof(output));
					msg++;
					continue;
				case 'S':
					Q_strncatz(output, TP_ClassForTFSkin(), sizeof(output));
					msg++;
					continue;
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
		
			Q_strncatz(output, va("%c", c), sizeof(output));

			msg++;
		}
		Q_strncatz(output, "\"", sizeof(output));
	}

	CL_SendClientCommand("%s", output);
}

void TP_Init(void)
{
}

void TP_CheckPickupSound(char *s, vec3_t org)
{
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
			break;	//if someone says just one word, we can take any tidles in their name to be a voice command
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
