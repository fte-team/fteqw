#include "quakedef.h"

#define MAX_WEAPONS 64 //fixme: make dynamic.

typedef enum {
	//one componant
	ff_death,
	ff_tkdeath,
	ff_suicide,
	ff_bonusfrag,
	ff_tkbonus,
	ff_flagtouch,
	ff_flagcaps,
	ff_flagdrops,

	//two componant
	ff_frags,		//must be the first of the two componant
	ff_fragedby,
	ff_tkills,
	ff_tkilledby,
} fragfilemsgtypes_t;

typedef struct statmessage_s {
	fragfilemsgtypes_t type;
	int wid;
	char *msgpart1;
	char *msgpart2;
	struct statmessage_s *next;
} statmessage_t;

typedef unsigned short stat;
typedef struct {
	stat totaldeaths;
	stat totalsuicides;
	stat totalteamkills;
	stat totalkills;
	stat totaltouches;
	stat totalcaps;
	stat totaldrops;

	//I was going to keep track of kills with a certain gun - too much memory
	//track only your own and total weapon kills rather than per client
	struct wt_s {
		//these include you.
		stat kills;
		stat teamkills;
		stat suicides;		

		stat ownkills;
		stat owndeaths;
		stat ownteamkills;
		stat ownteamdeaths;
		stat ownsuicides;
		char *fullname;
		char *abrev;
		char *codename;
	} weapontotals[MAX_WEAPONS];

	struct ct_s {
		stat caps;		//times they captured the flag
		stat drops;		//times lost the flag
		stat grabs;		//times grabbed flag

		stat owndeaths;	//times you killed them
		stat ownkills;	//times they killed you
		stat deaths;	//times they died (including by you)
		stat kills;		//times they killed (including by you)
		stat teamkills;	//times they killed a team member.
		stat teamdeaths;	//times they died to a team member.
		stat suisides;	//times they were stupid.
	} clienttotals[MAX_CLIENTS];

	qboolean readcaps;
	qboolean readkills;
	statmessage_t *message;
} fragstats_t;

static fragstats_t fragstats;

int Stats_GetKills(int playernum)
{
	return fragstats.clienttotals[playernum].kills;
}
int Stats_GetTKills(int playernum)
{
	return fragstats.clienttotals[playernum].teamkills;
}
int Stats_GetDeaths(int playernum)
{
	return fragstats.clienttotals[playernum].deaths;
}
int Stats_GetTouches(int playernum)
{
	return fragstats.clienttotals[playernum].grabs;
}
int Stats_GetCaptures(int playernum)
{
	return fragstats.clienttotals[playernum].caps;
}

qboolean Stats_HaveFlags(void)
{
	return fragstats.readcaps;
}
qboolean Stats_HaveKills(void)
{
	return fragstats.readkills;
}

void VARGS Stats_Message(char *msg, ...)
{
}

void Stats_Evaluate(fragfilemsgtypes_t mt, int wid, int p1, int p2)
{
	qboolean u1;
	qboolean u2;

	if (mt == ff_frags || mt == ff_tkills)
	{
		int tmp = p1;
		p1 = p2;
		p2 = tmp;
	}

	u1 = (p1 == (cl.playerview[0].playernum));
	u2 = (p2 == (cl.playerview[0].playernum));

	switch(mt)
	{
	case ff_death:
		if (u1)
		{
			fragstats.weapontotals[wid].owndeaths++;
			fragstats.weapontotals[wid].ownkills++;
		}
		fragstats.weapontotals[wid].kills++;
		fragstats.clienttotals[p1].deaths++;
		fragstats.totaldeaths++;
		break;
	case ff_suicide:
		if (u1)
		{
			fragstats.weapontotals[wid].ownsuicides++;
			fragstats.weapontotals[wid].owndeaths++;
			fragstats.weapontotals[wid].ownkills++;

			Stats_Message("You are a fool\n");
		}
		fragstats.weapontotals[wid].suicides++;
		fragstats.weapontotals[wid].kills++;
		fragstats.clienttotals[p1].suisides++;
		fragstats.clienttotals[p1].deaths++;
		fragstats.totalsuicides++;
		fragstats.totaldeaths++;
		break;
	case ff_bonusfrag:
		if (u1)
			fragstats.weapontotals[wid].ownkills++;
		fragstats.weapontotals[wid].kills++;
		fragstats.clienttotals[p1].kills++;
		fragstats.totalkills++;
		break;
	case ff_tkbonus:
		if (u1)
			fragstats.weapontotals[wid].ownkills++;
		fragstats.weapontotals[wid].kills++;
		fragstats.clienttotals[p1].kills++;
		fragstats.totalkills++;

		if (u1)
			fragstats.weapontotals[wid].ownteamkills++;
		fragstats.weapontotals[wid].teamkills++;
		fragstats.clienttotals[p1].teamkills++;
		fragstats.totalteamkills++;
		break;
	case ff_flagtouch:
		fragstats.clienttotals[p1].grabs++;
		fragstats.totaltouches++;

		if (u1)
		{
			Stats_Message("You grabbed the flag\n");
			Stats_Message("flag grabs: %i (%i)\n", fragstats.clienttotals[p1].grabs, fragstats.totaltouches);
		}
		break;
	case ff_flagcaps:
		fragstats.clienttotals[p1].caps++;
		fragstats.totalcaps++;

		if (u1)
		{
			Stats_Message("You captured the flag\n");
			Stats_Message("flag captures: %i (%i)\n", fragstats.clienttotals[p1].caps, fragstats.totalcaps);
		}
		break;
	case ff_flagdrops:
		fragstats.clienttotals[p1].drops++;
		fragstats.totaldrops++;

		if (u1)
		{
			Stats_Message("You dropped the flag\n");
			Stats_Message("flag drops: %i (%i)\n", fragstats.clienttotals[p1].drops, fragstats.totaldrops);
		}
		break;

	//p1 died, p2 killed
	case ff_frags:
	case ff_fragedby:
		fragstats.weapontotals[wid].kills++;

		fragstats.clienttotals[p1].deaths++;
		fragstats.totaldeaths++;
		if (u1)
		{
			fragstats.weapontotals[wid].owndeaths++;
			Stats_Message("%s killed you\n", cl.players[p2].name);
			Stats_Message("%s deaths: %i (%i/%i)\n", fragstats.weapontotals[wid].fullname, fragstats.clienttotals[p2].owndeaths, fragstats.weapontotals[wid].owndeaths, fragstats.totaldeaths);
		}

		fragstats.clienttotals[p2].kills++;
		fragstats.totalkills++;
		if (u2)
		{
			fragstats.weapontotals[wid].ownkills++;
			Stats_Message("You killed %s\n", cl.players[p1].name);
			Stats_Message("%s kills: %i (%i/%i)\n", fragstats.weapontotals[wid].fullname, fragstats.clienttotals[p2].kills, fragstats.weapontotals[wid].kills, fragstats.totalkills);
		}
		break;
	case ff_tkdeath:
		//killed by a teammate, but we don't know who
		//kinda useless, but this is all some mods give us
		fragstats.weapontotals[wid].teamkills++;
		fragstats.weapontotals[wid].kills++;
		fragstats.totalkills++;		//its a kill, but we don't know who from
		fragstats.totalteamkills++;

		fragstats.clienttotals[p1].teamdeaths++;
		fragstats.clienttotals[p1].deaths++;
		fragstats.totaldeaths++;
		break;

	case ff_tkills:
	case ff_tkilledby:
		//p1 killed by p2 (kills is already inverted)
		fragstats.weapontotals[wid].teamkills++;
		fragstats.weapontotals[wid].kills++;

		if (u1)
		{
			fragstats.weapontotals[wid].ownteamdeaths++;
			fragstats.weapontotals[wid].owndeaths++;
		}
		fragstats.clienttotals[p1].teamdeaths++;
		fragstats.clienttotals[p1].deaths++;
		fragstats.totaldeaths++;

		if (u2)
		{
			fragstats.weapontotals[wid].ownkills++;
			fragstats.weapontotals[wid].ownkills++;
		}
		fragstats.clienttotals[p2].teamkills++;
		fragstats.clienttotals[p2].kills++;
		fragstats.totalkills++;

		fragstats.totalteamkills++;
		break;
	}
}

static int Stats_FindWeapon(char *codename, qboolean create)
{
	int i;

	if (!strcmp(codename, "NONE"))
		return 0;
	if (!strcmp(codename, "NULL"))
		return 0;
	if (!strcmp(codename, "NOWEAPON"))
		return 0;

	for (i = 1; i < MAX_WEAPONS; i++)
	{
		if (!fragstats.weapontotals[i].codename)
		{
			fragstats.weapontotals[i].codename = Z_Malloc(strlen(codename)+1);
			strcpy(fragstats.weapontotals[i].codename, codename);
			return i;
		}

		if (!stricmp(fragstats.weapontotals[i].codename, codename))
		{
			if (create)
				return -2;
			return i;
		}
	}
	return -1;
}

static void Stats_StatMessage(fragfilemsgtypes_t type, int wid, char *token1, char *token2)
{
	statmessage_t *ms;
	char *t;
	ms = Z_Malloc(sizeof(statmessage_t) + strlen(token1)+1 + (token2 && *token2?strlen(token2)+1:0));
	t = (char *)(ms+1);
	ms->msgpart1 = t;
	strcpy(t, token1);
	if (token2 && *token2)
	{
		t += strlen(t)+1;
		ms->msgpart2 = t;
		strcpy(t, token2);
	}
	ms->type = type;
	ms->wid = wid;

	ms->next = fragstats.message;
	fragstats.message = ms;

	//we have a message type, save the fact that we have it.
	if (type == ff_flagtouch || type == ff_flagcaps || type == ff_flagdrops)
		fragstats.readcaps = true;
	if (type == ff_frags || type == ff_fragedby)
		fragstats.readkills = true;
}

static void Stats_Clear(void)
{
	int i;
	statmessage_t *ms;

	while (fragstats.message)
	{
		ms = fragstats.message;
		fragstats.message = ms->next;
		Z_Free(ms);
	}

	for (i = 1; i < MAX_WEAPONS; i++)
	{
		if (fragstats.weapontotals[i].codename)	Z_Free(fragstats.weapontotals[i].codename);
		if (fragstats.weapontotals[i].fullname)	Z_Free(fragstats.weapontotals[i].fullname);
		if (fragstats.weapontotals[i].abrev)	Z_Free(fragstats.weapontotals[i].abrev);
	}

	memset(&fragstats, 0, sizeof(fragstats));
}

#define Z_Copy(tk) tz = Z_Malloc(strlen(tk)+1);strcpy(tz, tk)	//remember the braces

static void Stats_LoadFragFile(char *name)
{
	char filename[MAX_QPATH];
	char *file;
	char *end;
	char *tk, *tz;
	char oend;

	Stats_Clear();

	strcpy(filename, name);
	COM_DefaultExtension(filename, ".dat", sizeof(filename));

	file = COM_LoadTempFile(filename);
	if (!file || !*file)
	{
		Con_DPrintf("Couldn't load %s\n", filename);
		return;
	}
	else
		Con_DPrintf("Loaded %s\n", filename);

	oend = 1;
	for (;*file;)
	{
		if (!oend)
			break;
		for (end = file; *end && *end != '\n'; end++)
			;
		oend = *end;
		*end = '\0';
		Cmd_TokenizeString(file, true, false);
		file = end+1;
		if (!Cmd_Argc())
			continue;

		tk = Cmd_Argv(0);
		if (!stricmp(tk, "#fragfile"))
		{
			tk = Cmd_Argv(1);
				 if (!stricmp(tk, "version"))		{}
			else if (!stricmp(tk, "gamedir"))		{}
			else Con_Printf("Unrecognised #meta \"%s\"\n", tk);
		}
		else if (!stricmp(tk, "#meta"))
		{
			tk = Cmd_Argv(1);
				 if (!stricmp(tk, "title"))			{}
			else if (!stricmp(tk, "description"))	{}
			else if (!stricmp(tk, "author"))		{}
			else if (!stricmp(tk, "email"))			{}
			else if (!stricmp(tk, "webpage"))		{}
			else {Con_Printf("Unrecognised #meta \"%s\"\n", tk);continue;}
		}
		else if (!stricmp(tk, "#define"))
		{
			tk = Cmd_Argv(1);
			if (!stricmp(tk, "weapon_class") ||
				!stricmp(tk, "wc"))	
			{
				int wid;

				tk = Cmd_Argv(2);

				wid = Stats_FindWeapon(tk, true);
				if (wid == -1)
				{Con_Printf("Too many weapon definitions. The max is %i\n", MAX_WEAPONS);continue;}
				else if (wid < -1)
				{Con_Printf("Weapon \"%s\" is already defined\n", tk);continue;}
				else
				{
					fragstats.weapontotals[wid].fullname = Z_Copy(Cmd_Argv(3));
					fragstats.weapontotals[wid].abrev = Z_Copy(Cmd_Argv(4));
				}
			}
			else if (!stricmp(tk, "obituary") ||
					 !stricmp(tk, "obit"))
			{
				int fftype;
				tk = Cmd_Argv(2);

					 if (!stricmp(tk, "PLAYER_DEATH"))			{fftype = ff_death;}
				else if (!stricmp(tk, "PLAYER_SUICIDE"))		{fftype = ff_suicide;}
				else if (!stricmp(tk, "X_FRAGS_UNKNOWN"))		{fftype = ff_bonusfrag;}
				else if (!stricmp(tk, "X_TEAMKILLS_UNKNOWN"))	{fftype = ff_tkbonus;}
				else if (!stricmp(tk, "X_TEAMKILLED_UNKNOWN"))	{fftype = ff_tkdeath;}
				else if (!stricmp(tk, "X_FRAGS_Y"))				{fftype = ff_frags;}
				else if (!stricmp(tk, "X_FRAGGED_BY_Y"))		{fftype = ff_fragedby;}
				else if (!stricmp(tk, "X_TEAMKILLS_Y"))			{fftype = ff_tkills;}
				else if (!stricmp(tk, "X_TEAMKILLED_BY_Y"))		{fftype = ff_tkilledby;}
				else {Con_Printf("Unrecognised obituary \"%s\"\n", tk);continue;}

				Stats_StatMessage(fftype, Stats_FindWeapon(Cmd_Argv(3), false), Cmd_Argv(4), Cmd_Argv(5));
			}
			else if (!stricmp(tk, "flag_alert") ||
					 !stricmp(tk, "flag_msg"))
			{
				int fftype;
				tk = Cmd_Argv(2);

					 if (!stricmp(tk, "X_TOUCHES_FLAG"))		{fftype = ff_flagtouch;}
				else if (!stricmp(tk, "X_GETS_FLAG"))			{fftype = ff_flagtouch;}
				else if (!stricmp(tk, "X_TAKES_FLAG"))			{fftype = ff_flagtouch;}
				else if (!stricmp(tk, "X_CAPTURES_FLAG"))		{fftype = ff_flagcaps;}
				else if (!stricmp(tk, "X_CAPS_FLAG"))			{fftype = ff_flagcaps;}
				else if (!stricmp(tk, "X_SCORES"))				{fftype = ff_flagcaps;}
				else if (!stricmp(tk, "X_DROPS_FLAG"))			{fftype = ff_flagdrops;}
				else if (!stricmp(tk, "X_FUMBLES_FLAG"))		{fftype = ff_flagdrops;}
				else if (!stricmp(tk, "X_LOSES_FLAG"))			{fftype = ff_flagdrops;}
				else {Con_Printf("Unrecognised flag alert \"%s\"\n", tk);continue;}

				Stats_StatMessage(fftype, 0, Cmd_Argv(3), NULL);
			}
			else
			{Con_Printf("Unrecognised directive \"%s\"\n", tk);continue;}
		}
		else
		{Con_Printf("Unrecognised directive \"%s\"\n", tk);continue;}
	}
}

int qm_strcmp(char *s1, char *s2)//not like strcmp at all...
{
	while(*s1)
	{
		if ((*s1++&0x7f)!=(*s2++&0x7f))
			return 1;
	}
	return 0;
}

int qm_stricmp(char *s1, char *s2)//not like strcmp at all...
{
	int c1,c2;
	while(*s1)
	{
		c1 = *s1++&0x7f;
		c2 = *s2++&0x7f;

		if (c1 >= 'A' && c1 <= 'Z')
			c1 = c1 - 'A' + 'a';

		if (c2 >= 'A' && c2 <= 'Z')
			c2 = c2 - 'A' + 'a';

		if (c1!=c2)
			return 1;
	}
	return 0;
}


static int Stats_ExtractName(char **line)
{
	int i;
	int bm;
	int ml = 0;
	int l;
	bm = -1;
	for (i = 0; i < cl.allocated_client_slots; i++)
	{
		if (!qm_strcmp(cl.players[i].name, *line))
		{
			l = strlen(cl.players[i].name);
			if (l > ml)
			{
				bm = i;
				ml = l;
			}
		}
	}
	*line += ml;
	return bm;
}

void Stats_ParsePrintLine(char *line)
{
	statmessage_t *ms;
	int p1;
	int p2;
	char *m2;

	p1 = Stats_ExtractName(&line);
	if (p1<0)	//reject it.
	{
		return;
	}
	
	for (ms = fragstats.message; ms; ms = ms->next)
	{
		if (!qm_stricmp(ms->msgpart1, line))
		{
			if (ms->type >= ff_frags)
			{	//two players
				m2 = line + strlen(ms->msgpart1);
				p2 = Stats_ExtractName(&m2);
				if (!ms->msgpart2)
					continue;
				if (!qm_stricmp(ms->msgpart2, m2))
				{
					Stats_Evaluate(ms->type, ms->wid, p1, p2);
					return;	//done.
				}
			}
			else
			{	//one player
				Stats_Evaluate(ms->type, ms->wid, p1, p1);
				return;	//done.
			}
		}
	}
}

void Stats_NewMap(void)
{
	Stats_LoadFragFile("fragfile");
}

