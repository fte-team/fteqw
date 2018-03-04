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
#include "quakedef.h"
#include "pr_common.h"

#ifndef CLIENTONLY

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif



int	sv_allow_cheats;
qboolean SV_MayCheat(void)
{
	if (sv_allow_cheats == 2)
		return sv.allocated_client_slots == 1;
	return sv_allow_cheats!=0;
}

extern cvar_t cl_warncmd;
cvar_t sv_cheats = CVARF("sv_cheats", "0", CVAR_LATCH);
	extern		redirect_t	sv_redirected;

extern cvar_t sv_public;

static const struct banflags_s
{
	unsigned int banflag;
	const char *names[2];
} banflags[] =
{
	{BAN_BAN,		{"ban"}},
	{BAN_PERMIT,	{"safe",		"permit"}},
	{BAN_CUFF,		{"cuff"}},
	{BAN_MUTE,		{"mute"}},
	{BAN_CRIPPLED,	{"cripple"}},
	{BAN_DEAF,		{"deaf"}},
	{BAN_LAGGED,	{"lag",		"lagged"}},
	{BAN_VIP,		{"vip"}},
	{BAN_BLIND,		{"blind"}},
	{BAN_SPECONLY,	{"spec"}},
	{BAN_STEALTH,	{"stealth"}},
	{BAN_MAPPER,	{"mapper"}},

	{BAN_USER1,		{"user1"}},
	{BAN_USER2,		{"user2"}},
	{BAN_USER3,		{"user3"}},
	{BAN_USER4,		{"user4"}},
	{BAN_USER5,		{"user5"}},
	{BAN_USER6,		{"user6"}},
	{BAN_USER7,		{"user7"}},
	{BAN_USER8,		{"user8"}}
};

//generic helper function for naming players.
client_t *SV_GetClientForString(const char *name, int *id)
{
	int i;
	const char *s;
	char nicename[80];
	char niceclname[80];
	client_t *cl;

	int first=0;
	if (id && *id != -1)
		first = *id;

	if (!strcmp(name, "*"))	//match with all
	{
		for (i = first, cl = svs.clients+first; i < sv.allocated_client_slots; i++, cl++)
		{
			if (cl->state<=cs_loadzombie)
				continue;

			*id=i+1;
			return cl;
		}
		*id=sv.allocated_client_slots;
		return NULL;
	}

	//check to make sure it's all an int

	for (s = name; *s; s++)
	{
		if (*s < '0' || *s > '9')
			break;
	}

	//we got to the end of the string and found only numbers. - it's a uid.
	if (!*s)
	{
		int uid = Q_atoi(name);
		for (i = first, cl = svs.clients; i < sv.allocated_client_slots; i++, cl++)
		{
			if (cl->state<=cs_loadzombie)
				continue;
			if (cl->userid == uid)
			{
				if (id)
					*id=sv.allocated_client_slots;
				return cl;
			}
		}

		return NULL;
	}

	for (i = first, cl = svs.clients+first; i < sv.allocated_client_slots; i++, cl++)
	{
		if (cl->state<=cs_loadzombie)
			continue;


		deleetstring(niceclname, cl->name);
		deleetstring(nicename, name);

		if (strstr(niceclname, nicename))
		{
			if (id)
				*id=i+1;
			return cl;
		}
	}

	return NULL;
}

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
==================
SV_Quit_f
==================
*/
static void SV_Quit_f (void)
{
	if (sv.state >= ss_loading)
		SV_FinalMessage ("server shutdown\n");
	Con_TPrintf ("Shutting down.\n");
	SV_Shutdown ();
	Sys_Quit ();
}

/*
============
SV_Fraglogfile_f
============
*/
static void SV_Fraglogfile_f (void)
{
	char	name[MAX_OSPATH];
	int		i;

	if (sv_fraglogfile)
	{
		Con_TPrintf ("Frag file logging off.\n");
		VFS_CLOSE (sv_fraglogfile);
		sv_fraglogfile = NULL;
		return;
	}

	// find an unused name
	for (i=0 ; i<1000 ; i++)
	{
		sprintf (name, "frag_%i.log", i);
		sv_fraglogfile = FS_OpenVFS(name, "rb", FS_GAME);
		if (!sv_fraglogfile)
		{	// can't read it, so create this one
			sv_fraglogfile = FS_OpenVFS (name, "wb", FS_GAME);
			if (!sv_fraglogfile)
				i=1000;	// give error
			break;
		}
		VFS_CLOSE (sv_fraglogfile);
	}
	if (i==1000)
	{
		Con_TPrintf ("Can't open any logfiles.\n");
		sv_fraglogfile = NULL;
		return;
	}

	Con_TPrintf ("Logging frags to %s.\n", name);
}


/*
==================
SV_SetPlayer

Sets host_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
static qboolean SV_SetPlayer (void)
{
	client_t	*cl;
	int			i;
	int			idnum;

	idnum = atoi(Cmd_Argv(1));

	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == idnum)
		{
			host_client = cl;
			sv_player = host_client->edict;
			return true;
		}
	}
	Con_TPrintf ("Userid %i is not on the server\n", idnum);
	return false;
}


/*
==================
SV_God_f

Sets client to godmode
==================
*/
static void SV_God_f (void)
{
	if (!SV_MayCheat())
	{
		Con_TPrintf ("Please set sv_cheats 1 and restart the map first.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	SV_LogPlayer(host_client, "god cheat");
	sv_player->v->flags = (int)sv_player->v->flags ^ FL_GODMODE;
	if ((int)sv_player->v->flags & FL_GODMODE)
		SV_ClientTPrintf (host_client, PRINT_HIGH, "godmode ON\n");
	else
		SV_ClientTPrintf (host_client, PRINT_HIGH, "godmode OFF\n");
}


static void SV_Noclip_f (void)
{
	if (!SV_MayCheat())
	{
		Con_TPrintf ("Please set sv_cheats 1 and restart the map first.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	SV_LogPlayer(host_client, "noclip cheat");
	if (sv_player->v->movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v->movetype = MOVETYPE_NOCLIP;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "noclip ON\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "noclip OFF\n");
	}
}

#ifdef QUAKESTATS
/*
==================
SV_Give_f
==================
*/
static void SV_Give_f (void)
{
	char	*t = Cmd_Argv(2);
	int		v;

	if (!svprogfuncs)
		return;

	if (!strcmp(t, "damn"))
	{
		Con_TPrintf ("%s not given.\n", t);
		return;
	}

	if (!SV_MayCheat())
	{
		Con_TPrintf ("Please set sv_cheats 1 and restart the map first.\n");
		return;
	}

/*	if (developer.value)
	{
		int oldself;
		oldself = pr_global_struct->self;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.world.edicts);
		Con_Printf("Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, Cmd_Args()));
		pr_global_struct->self = oldself;
	}
*/
	if (!SV_SetPlayer ())
	{
		return;
	}

	SV_LogPlayer(host_client, "give cheat");

	v = atoi (Cmd_Argv(3));

	switch ((t[1]==0)?t[0]:0)
	{
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		sv_player->v->items = (int)sv_player->v->items | IT_SHOTGUN<< (t[0] - '2');
		break;

	case 's':
		sv_player->v->ammo_shells = v;
		break;
	case 'n':
		sv_player->v->ammo_nails = v;
		break;
	case 'r':
		sv_player->v->ammo_rockets = v;
		break;
	case 'h':
		sv_player->v->health = v;
		break;
	case 'c':
		sv_player->v->ammo_cells = v;
		break;
/*	default:
		{
			int oldself;
			oldself = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			Cmd_ShiftArgs(1, false);
			Con_TPrintf("Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, Cmd_Args()));
			pr_global_struct->self = oldself;
		}
*/
	}
}
#endif

static int QDECL ShowMapList (const char *name, qofs_t flags, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	char stripped[64];
	if (name[5] == 'b' && name[6] == '_')	//skip box models
		return true;
	COM_StripExtension(name+5, stripped, sizeof(stripped)); 
	Con_Printf("^[[%s]\\map\\%s^]\n", stripped, stripped);
	return true;
}
static int QDECL ShowMapListExt (const char *name, qofs_t flags, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	if (name[5] == 'b' && name[6] == '_')	//skip box models
		return true;
	Con_Printf("^[[%s]\\map\\%s^]\n", name+5, name+5);
	return true;
}
static void SV_MapList_f(void)
{
	COM_EnumerateFiles("maps/*.bsp", ShowMapList, NULL);
	COM_EnumerateFiles("maps/*.map", ShowMapListExt, NULL);
	COM_EnumerateFiles("maps/*.cm", ShowMapList, NULL);
	COM_EnumerateFiles("maps/*.hmp", ShowMapList, NULL);
}

static int QDECL CompleteMapList (const char *name, qofs_t flags, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	struct xcommandargcompletioncb_s *ctx = parm;
	char stripped[64];
	if (name[5] == 'b' && name[6] == '_')	//skip box models
		return true;
	
	COM_StripExtension(name+5, stripped, sizeof(stripped));
	ctx->cb(stripped, ctx);
	return true;
}
static int QDECL CompleteMapListExt (const char *name, qofs_t flags, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	struct xcommandargcompletioncb_s *ctx = parm;
	if (name[5] == 'b' && name[6] == '_')	//skip box models
		return true;

	ctx->cb(name+5, ctx);
	return true;
}
static void SV_Map_c(int argn, const char *partial, struct xcommandargcompletioncb_s *ctx)
{
	if (argn == 1)
	{
		COM_EnumerateFiles(va("maps/%s*.bsp", partial), CompleteMapList, ctx);
		COM_EnumerateFiles(va("maps/%s*.map", partial), CompleteMapListExt, ctx);
		COM_EnumerateFiles(va("maps/%s*.cm", partial), CompleteMapList, ctx);
		COM_EnumerateFiles(va("maps/%s*.hmp", partial), CompleteMapList, ctx);
	}
}

//static void gtcallback(struct cvar_s *var, char *oldvalue)
//{
//	Con_Printf("g_gametype changed\n");
//}

/*
======================
SV_Map_f

handle a
map <mapname>
command from the console or progs.

quirks:
a leading '*' means new unit, meaning all old map state is flushed regardless of startspot
a '+' means 'set nextmap cvar to the following value and otherwise ignore, for q2 compat. only applies if there's also a '.' and the specified bsp doesn't exist, for q1 compat.
just a '.' is taken to mean 'restart'. parms are not changed from their current values, startspot is also unchanged.

'map' will change map, for most games. strips parms+serverflags+cache. note that NQ kicks everyone (NQ expects you to use changelevel for that).
'changelevel' will not flush the level cache, for h2 compat (won't save current level state in such a situation, as nq would prefer not)
'gamemap' will save the game to 'save0' after loading, for q2 compat
'spmap' is for q3 and sets 'gametype' to '2', otherwise identical to 'map'. all other map commands will reset it to '0' if its '2' at the time.
======================
*/
void SV_Map_f (void)
{
	char	level[MAX_QPATH];
	char	spot[MAX_QPATH];
	char	expanded[MAX_QPATH];
	char	*nextserver;
	qboolean preserveplayers= false;
	qboolean isrestart		= false;	//don't hurt settings
	qboolean newunit		= false;	//no hubcache
	qboolean flushparms		= false;	//flush parms+serverflags
	qboolean cinematic		= false;	//new map is .cin / .roq or something
	qboolean q2savetos0		= false;
#ifdef Q3SERVER
	qboolean q3singleplayer	= false;	//forces g_gametype to 2 (otherwise clears if it was 2).
#endif

	qboolean waschangelevel	= false;
	int i;
	char *startspot;

	nextserver = 0;

#ifndef SERVERONLY
	if (!Renderer_Started() && !isDedicated)
	{
		Cbuf_AddText(va("wait;%s %s\n", Cmd_Argv(0), Cmd_Args()), Cmd_ExecLevel);
		return;
	}
#endif

	if (!Q_strcasecmp(Cmd_Argv(0), "map_restart"))
	{
		float delay = atof(Cmd_Argv(1));
		if (delay)
			Con_DPrintf ("map_restart delay not implemented yet\n");
		Q_strncpyz (level, ".", sizeof(level));
		startspot = NULL;

		//FIXME: if precaches+statics don't change, don't do the whole networking thing.
	}
	else
	{

		if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
		{
			if (Cmd_IsInsecure())
				return;
			Con_TPrintf ("Available maps:\n", Cmd_Argv(0));
			SV_MapList_f();
			return;
		}

		Q_strncpyz (level, Cmd_Argv(1), sizeof(level));
		startspot = ((Cmd_Argc() == 2)?NULL:Cmd_Argv(2));
	}

	q2savetos0 = !strcmp(Cmd_Argv(0), "gamemap") && !isDedicated;	//q2
#ifdef Q3SERVER
	q3singleplayer = !strcmp(Cmd_Argv(0), "spmap");
#endif
	flushparms = !strcmp(Cmd_Argv(0), "map") || !strcmp(Cmd_Argv(0), "spmap");
	newunit = flushparms || (!strcmp(Cmd_Argv(0), "changelevel") && !startspot);

	sv.mapchangelocked = false;

	if (strcmp(level, "."))	//restart current
	{
		snprintf (expanded, sizeof(expanded), "maps/%s.bsp", level); // this function and the if statement below, is a quake bugfix which stopped a map called "dm6++.bsp" from loading because of the + sign, quake2 map syntax interprets + character as "intro.cin+base1.bsp", to play a cinematic then load a map after
		if (!COM_FCheckExists (expanded))
		{
			nextserver = strchr(level, '+');
			if (nextserver)
			{
				*nextserver = '\0';
				nextserver++;
			}
		}
	}

	if (startspot)
	{
		strcpy(spot, startspot);
		startspot = spot;
	}
	else if ((startspot = strchr(level, '$')))
	{
		strcpy(spot, startspot+1);
		*startspot = '\0';
		startspot = spot;
	}
	else
		startspot = NULL;

	if (!strcmp(level, "."))	//restart current
	{
		//grab the current map name
		Q_strncpyz(level, svs.name, sizeof(level));
		isrestart = true;
		flushparms = false;
		newunit = false;
		q2savetos0 = false;

		if (!*level)
		{
			sv.mapchangelocked = true;
			if (Cmd_AliasExist("startmap_dm", RESTRICT_LOCAL))
			{
				Cbuf_AddText("startmap_dm", Cmd_ExecLevel);
				return;
			}
			Q_strncpyz(level, "start", sizeof(level));
		}

		if (startspot && !strcmp(startspot, "."))
		{
			preserveplayers = true;
			startspot = NULL;
		}
		if (!startspot)
		{
			//revert the startspot if its not overridden
			Q_strncpyz(spot, Info_ValueForKey(svs.info, "*startspot"), sizeof(spot));
			startspot = spot;
		}
	}

	// check to make sure the level exists
	if (*level == '*')
	{
		memmove(level, level+1, strlen(level));
		newunit=true;
	}
#ifndef SERVERONLY
	SCR_ImageName(level);
	SCR_SetLoadingStage(LS_SERVER);
	SCR_SetLoadingFile("finalize server");
#else
	#define SCR_SetLoadingFile(s)
#endif

	COM_FlushFSCache(false, true);

#ifdef Q2SERVER
	if (strlen(level) > 4 &&
		(!strcmp(level + strlen(level)-4, ".cin") ||
		!strcmp(level + strlen(level)-4, ".roq") ||
		!strcmp(level + strlen(level)-4, ".pcx") ||
		!strcmp(level + strlen(level)-4, ".avi")))
	{
		cinematic = true;
	}
	else
#endif
#ifdef TERRAIN
	//'map doesntexist.map' should just auto-generate that map or something
	if (!Q_strcasecmp("map", COM_FileExtension(level, expanded, sizeof(expanded))))
		;
	else
#endif
	{
		char *exts[] = {"maps/%s", "maps/%s.bsp", "maps/%s.cm", "maps/%s.hmp", /*"maps/%s.map",*/ NULL};
		int i, j;

		for (i = 0; exts[i]; i++)
		{
			snprintf (expanded, sizeof(expanded), exts[i], level);
			if (COM_FCheckExists (expanded))
				break;
		}
		if (!exts[i])
		{
			for (i = 0; exts[i]; i++)
			{
				//doesn't exist, so try lowercase. Q3 does this.
				for (j = 0; j < sizeof(level) && level[j]; j++)
				{
					if (level[j] >= 'A' && level[j] <= 'Z')
						level[j] = level[j] - 'A' + 'a';
				}
				snprintf (expanded, sizeof(expanded), exts[i], level);
				if (COM_FCheckExists (expanded))
					break;
			}
			if (!exts[i])
			{
				// FTE is still a Quake engine so report BSP missing
				snprintf (expanded, sizeof(expanded), exts[0], level);
				Con_TPrintf ("Can't find %s\n", expanded);
#ifndef SERVERONLY
				SCR_SetLoadingStage(LS_NONE);
#endif

				if (SSV_IsSubServer())
					Cbuf_AddText("\nquit\n", RESTRICT_LOCAL);
				return;
			}
		}
	}

	if (sv.mvdrecording)
		SV_MVDStop_f();

#ifndef SERVERONLY
	if (!isDedicated)	//otherwise, info used on map loading isn't present
	{
		cl.haveserverinfo = true;
		Q_strncpyz (cl.serverinfo, svs.info, sizeof(cl.serverinfo));
		CL_CheckServerInfo();
	}

	if (!sv.state && cls.state)
		CL_Disconnect();
#endif

	if (!isrestart)
		SV_SaveSpawnparms ();

	if (newunit)
		SV_FlushLevelCache();	//forget all on new unit
	else if (startspot && !isrestart && !newunit)
	{
#ifdef Q2SERVER
		if (ge)
		{
			qboolean savedinuse[MAX_CLIENTS];
			for (i=0 ; i<sv.allocated_client_slots; i++)
			{
				savedinuse[i] = svs.clients[i].q2edict->inuse;
				svs.clients[i].q2edict->inuse = false;
			}
			SV_SaveLevelCache(NULL, false);
			for (i=0 ; i<sv.allocated_client_slots; i++)
			{
				svs.clients[i].q2edict->inuse = savedinuse[i];
			}
		}
		else
#endif
			SV_SaveLevelCache(NULL, false);
	}

#ifdef Q3SERVER
	{
		cvar_t *gametype;

		gametype = Cvar_Get("mapname", "", CVAR_LATCH|CVAR_SERVERINFO, "Q3 compatability");
		gametype->flags |= CVAR_SERVERINFO;
		Cvar_ForceSet(gametype, level);

		gametype = Cvar_Get("g_gametype", "", CVAR_LATCH|CVAR_SERVERINFO, "Q3 compatability");
//		gametype->callback = gtcallback;
		if (q3singleplayer)
			Cvar_ForceSet(gametype, "2");//singleplayer
		else if (gametype->value == 2)
			Cvar_ForceSet(gametype, "");//force to ffa deathmatch
	}
#endif

#if defined(MENU_DAT) && !defined(SERVERONLY)
	if (Key_Dest_Has(kdm_gmenu))
		MP_Toggle(0);
#endif

	if (preserveplayers && svprogfuncs)
	{
		for (i=0 ; i<svs.allocated_client_slots ; i++)	//we need to drop all q2 clients. We don't mix q1w with q2.
		{
			char buffer[8192], *buf;
			size_t bufsize = 0;
			if (svs.clients[i].state>cs_connected)
			{
				buf = svprogfuncs->saveent(svprogfuncs, buffer, &bufsize, sizeof(buffer), svs.clients[i].edict);
				if (svs.clients[i].spawninfo)
					Z_Free(svs.clients[i].spawninfo);
				svs.clients[i].spawninfo = Z_Malloc(bufsize+1);
				memcpy(svs.clients[i].spawninfo, buf, bufsize+1);
				svs.clients[i].spawninfotime = sv.time;
			}
		}
	}
	else
	{
		for (i=0 ; i<svs.allocated_client_slots ; i++)	//we need to drop all q2 clients. We don't mix q1w with q2.
		{
			if (svs.clients[i].state>cs_connected)	//so that we don't send a datagram
				svs.clients[i].state=cs_connected;
		}
	}

#ifndef SERVERONLY
	S_StopAllSounds (true);
//	SCR_BeginLoadingPlaque();
	SCR_ImageName(level);
#endif

//	if (!preserveplayers)
	{
		for (i=0, host_client = svs.clients ; i<svs.allocated_client_slots ; i++, host_client++)
		{
			/*pass the new map's name as an extension, so appropriate loading screens can be shown*/
			if (host_client->controller == NULL)
			{
				if (ISNQCLIENT(host_client))
				{
					if (ISDPCLIENT(host_client))
					{
						//DP clients cannot cope with being told the next map's name
						SV_StuffcmdToClient(host_client, "reconnect\n");
					}
					else
						SV_StuffcmdToClient(host_client, va("reconnect \"%s\"\n", level));
				}
				else
					SV_StuffcmdToClient(host_client, va("changing \"%s\"\n", level));
			}
			host_client->prespawn_stage = PRESPAWN_INVALID;
			host_client->prespawn_idx = 0;
		}
		SV_SendMessagesToAll ();

		if (flushparms)
			svs.serverflags = 0;
	}

	SCR_SetLoadingFile("spawnserver");
	if (newunit || !startspot || cinematic || !SV_LoadLevelCache(NULL, level, startspot, false))
	{
		if (waschangelevel && !startspot)
			startspot = "";
		SV_SpawnServer (level, startspot, false, cinematic);
	}
	SCR_SetLoadingFile("server spawned");

	//SV_BroadcastCommand ("cmd new\n");
	for (i=0, host_client = svs.clients ; i<svs.allocated_client_slots ; i++, host_client++)
	{	//this expanded code cuts out a packet when changing maps...
		//but more usefully, it stops dp(and probably nq too) from timing out.
		//make sure its all reset.
		host_client->sentents.num_entities = 0;
		host_client->ratetime = 0;
		if (host_client->pendingdeltabits)
			host_client->pendingdeltabits[0] = UF_REMOVE;

		if (flushparms)
		{
			if (host_client->spawninfo)
				Z_Free(host_client->spawninfo);
			host_client->spawninfo = NULL;
			memset(host_client->spawn_parms, 0, sizeof(host_client->spawn_parms));
			if (host_client->state > cs_zombie)
				SV_GetNewSpawnParms(host_client);
		}

		if (preserveplayers && svprogfuncs && host_client->state == cs_spawned && host_client->spawninfo)
		{
			size_t j = 0;
			svprogfuncs->restoreent(svprogfuncs, host_client->spawninfo, &j, host_client->edict);
			host_client->istobeloaded = true;
			host_client->state=cs_connected;
			if (host_client->spectator)
				sv.spawned_observer_slots++;
			else
				sv.spawned_client_slots++;
		}

		if (host_client->controller)
			continue;
		if (host_client->state>=cs_connected)
		{
			if (host_client->protocol == SCP_QUAKE3)
				continue;
			if (host_client->protocol == SCP_BAD)
				continue;

#ifdef NQPROT
			if (ISNQCLIENT(host_client))
			{
				SVNQ_New_f();
				host_client->send_message = true;
			}
			else
#endif
				SV_New_f();
		}
	}

	if (!isrestart)
	{
		cvar_t *nsv;
		nsv = Cvar_Get("nextserver", "", 0, "");
		if (nextserver)
			Cvar_Set(nsv, va("gamemap \"%s\"", nextserver));
		else
			Cvar_Set(nsv, "");
	}

	if (q2savetos0)
	{
		if (sv.state != ss_cinematic)	//too weird.
			SV_Savegame("s0", true);
	}


	if (isDedicated)
		Mod_Purge(MP_MAPCHANGED);
}

static void SV_KillServer_f(void)
{
	SV_UnspawnServer();
}


/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
static void SV_Kick_f (void)
{
	client_t	*cl;
	int clnum=-1;

	if (!sv.state)
		return;

	if (!strcmp(Cmd_Argv(1), "#"))
	{
		clnum = atoi(Cmd_Argv(2)) - 1;
		if (clnum >= 0 && clnum < sv.allocated_client_slots)
		{
			cl = &svs.clients[clnum];
			if (cl->state >= cs_connected)
			{
				SV_BroadcastTPrintf (PRINT_HIGH, "%s was kicked\n", cl->name);
				// print directly, because the dropped client won't get the
				// SV_BroadcastPrintf message
				SV_ClientTPrintf (cl, PRINT_HIGH, "You were kicked\n");

				SV_LogPlayer(cl, "kicked");
				SV_DropClient (cl);
			}
		}
		return;
	}

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		SV_BroadcastTPrintf (PRINT_HIGH, "%s was kicked\n", cl->name);
		// print directly, because the dropped client won't get the
		// SV_BroadcastPrintf message
		SV_ClientTPrintf (cl, PRINT_HIGH, "You were kicked\n");

		SV_LogPlayer(cl, "kicked");
		SV_DropClient (cl);
	}

	if (clnum == -1)
		Con_TPrintf ("Couldn't find user number %s\n", Cmd_Argv(1));
}

/*for q3's kick bot menu*/
static void SV_KickSlot_f (void)
{
	client_t	*cl;
	int clnum=atoi(Cmd_Argv(1));

	if (!sv.state)
		return;

	if (clnum < sv.allocated_client_slots && svs.clients[clnum].state)
	{
		cl = &svs.clients[clnum];

		SV_BroadcastTPrintf (PRINT_HIGH, "%s was kicked\n", cl->name);
		// print directly, because the dropped client won't get the
		// SV_BroadcastPrintf message
		SV_ClientTPrintf (cl, PRINT_HIGH, "You were kicked\n");

		SV_LogPlayer(cl, "kicked");
		SV_DropClient (cl);
	}
	else
		Con_Printf("Client %i is not active\n", clnum);
}

//ipv4ify if its an ipv6 ipv4-mapped address.
static netadr_t *NET_IPV4ify(netadr_t *a, netadr_t *tmp)
{
	if (a->type == NA_IPV6 &&
		!*(int*)&a->address.ip6[0] &&
		!*(int*)&a->address.ip6[4] &&
		!*(short*)&a->address.ip6[8] &&
		*(short*)&a->address.ip6[10]==(short)0xffff)
	{
		tmp->type = NA_IP;
		tmp->connum = a->connum;
		tmp->scopeid = a->scopeid;
		tmp->port = a->port;
		tmp->address.ip[0] = a->address.ip6[12];
		tmp->address.ip[1] = a->address.ip6[13];
		tmp->address.ip[2] = a->address.ip6[14];
		tmp->address.ip[3] = a->address.ip6[15];
		a = tmp;
	}
	return a;
}

//will kick clients if they got banned (without being safe)
void SV_EvaluatePenalties(client_t *cl)
{
	bannedips_t *banip;
	unsigned int penalties = 0, delta, p;
	char *penaltyreason[countof(banflags)] = {NULL};
	const char *activepenalties[countof(banflags)];
	char *reasons[countof(banflags)] = {NULL};
	int numpenalties = 0;
	int numreasons = 0;
	int i;
	netadr_t tmp, *a;

	if (cl->realip.type != NA_INVALID)
	{
		a = NET_IPV4ify(&cl->realip, &tmp);
		for (banip = svs.bannedips; banip; banip=banip->next)
		{
			if (NET_CompareAdrMasked(a, &banip->adr, &banip->adrmask))
			{
				for (i = 0; i < sizeof(penaltyreason)/sizeof(penaltyreason[0]); i++)
				{
					p = 1u<<i;
					if (banip->banflags & p)
					{
						if (!penaltyreason[i])
							penaltyreason[i] = banip->reason;
						penalties |= p;
					}
				}
			}
		}
	}
	a = NET_IPV4ify(&cl->netchan.remote_address, &tmp);
	for (banip = svs.bannedips; banip; banip=banip->next)
	{
		if (NET_CompareAdrMasked(a, &banip->adr, &banip->adrmask))
		{
			for (i = 0; i < sizeof(penaltyreason)/sizeof(penaltyreason[0]); i++)
			{
				p = 1u<<i;
				if (banip->banflags & p)
				{
					if (!penaltyreason[i])
						penaltyreason[i] = banip->reason;
					penalties |= p;
				}
			}
		}
	}

	delta = cl->penalties ^ penalties;
	cl->penalties = penalties;

	if ((penalties & (BAN_BAN | BAN_PERMIT)) == BAN_BAN)
	{
		//we should only reach here by a player getting banned mid-game.
		if (penaltyreason[BAN_BAN])
			SV_BroadcastPrintf(PRINT_HIGH, "%s was banned: %s\n", cl->name, penaltyreason[BAN_BAN]);
		else
			SV_BroadcastPrintf(PRINT_HIGH, "%s was banned\n", cl->name);
		cl->drop = true;
	}

	//don't announce these now.
	delta &= ~(BAN_BAN | BAN_PERMIT);

	//deaf+mute sees no (other) penalty messages
	if (((penalties|delta) & (BAN_MUTE|BAN_DEAF)) == (BAN_MUTE|BAN_DEAF))
		delta &= ~(BAN_MUTE|BAN_DEAF);

	if (penalties & BAN_STEALTH)
		delta = 0;	//don't announce ANY.

	if (cl->controller)
		delta = 0;	//don't spam it for every player in a splitscreen client.

	if (delta & BAN_VIP)
	{
		delta &= ~BAN_VIP;	//don't refer to this as a penalty
		if (penalties & BAN_VIP)
			SV_PrintToClient(cl, PRINT_HIGH, "You are a VIP, apparently\n");
		else
			SV_PrintToClient(cl, PRINT_HIGH, "VIP expired\n");
	}

	for (i = 0; i < countof(banflags); i++)
	{
		p = banflags[i].banflag;
		if (delta & p)
		{
			if (penalties & p)
			{
				if (banflags[i].names[0])
					activepenalties[numpenalties++] = banflags[i].names[0];
				if (reasons[i] && *reasons[i])
					reasons[numreasons++] = reasons[i];
			}
			else
				SV_PrintToClient(cl, PRINT_HIGH, va("Penalty expired: %s\n", banflags[i].names[0]));
		}
	}

	if (numpenalties)
	{
		char penaltystring[1024];
		int i, j;
		Q_strncpyz(penaltystring, "You are penalised: ", sizeof(penaltystring));
		for (i = 0; i < numpenalties; i++)
		{
			if (i && i == numpenalties-1)
				Q_strncatz(penaltystring, " and ", sizeof(penaltystring));
			else if (i)
				Q_strncatz(penaltystring, ", ", sizeof(penaltystring));
			Q_strncatz(penaltystring, activepenalties[i], sizeof(penaltystring));
		}
		Q_strncatz(penaltystring, "\n", sizeof(penaltystring));
		SV_PrintToClient(cl, PRINT_HIGH, penaltystring);
		for (i = 0; i < numreasons; i++)
		{
			if (*reasons[i])
			{
				for(j = 0; j < i; j++)
					if (!strcmp(reasons[i], reasons[j]))
						break;
				if (i == j)
					SV_PrintToClient(cl, PRINT_HIGH, va("  %s\n", reasons[i]));
			}
		}
	}

	if (delta & BAN_VIP)
		Info_SetValueForStarKey(cl->userinfo, "*VIP", (cl->penalties & BAN_VIP)?"1":"", sizeof(cl->userinfo));
	if (delta & BAN_MAPPER)
		Info_SetValueForStarKey(cl->userinfo, "*mapper", (cl->penalties & BAN_MAPPER)?"1":"", sizeof(cl->userinfo));
}

static time_t reevaluatebantime;
static qboolean reevaluatebans;
//could use time(NULL) instead, but this avoids a system call.
static time_t SV_BanTime(void)
{
	static double bantimemark;
	static time_t banstarttime;
	if (!banstarttime)
	{
		banstarttime = time(NULL);
		bantimemark = realtime;
	}
	return banstarttime + (realtime - bantimemark);
}
//removes anything with an expiry time in the past.
//avoids walking the list if there's nothing changed.
//can be used to force penalty reevaluation.
void SV_KillExpiredBans(void)
{
	bannedips_t **link, *banip;
	time_t curtime = SV_BanTime();
	int i;
	if (reevaluatebantime && curtime > reevaluatebantime)
	{
		reevaluatebantime = 0;
		for(link = &svs.bannedips; (banip = *link) != NULL; )
		{
			if (banip->expiretime)
			{
				if (banip->expiretime < curtime)
				{
					reevaluatebans = true;
					*link = banip->next;
					Z_Free(banip);
					continue;
				}
				if (!reevaluatebantime || reevaluatebantime > banip->expiretime)
					reevaluatebantime = banip->expiretime+1;
			}
			link = &banip->next;
		}
	}

	if (reevaluatebans)
	{
		reevaluatebans = false;
		for (i = 0; i < svs.allocated_client_slots; i++)
		{
			if (svs.clients[i].state<=cs_loadzombie)
				continue;

			SV_EvaluatePenalties(&svs.clients[i]);
		}
	}
}

//adds a new ban/penalty.
//will remove old penalties if the new one has a longer duration, otherwise will ignore the add.
static qboolean SV_AddBanEntry(bannedips_t *proto, char *reason)
{
	bannedips_t *nb, **link;
	nb = svs.bannedips;
	while (nb)
	{
		if (NET_CompareAdr(&nb->adr, &proto->adr) && NET_CompareAdr(&nb->adrmask, &proto->adrmask))
		{
			//found a match, figure out which lasts longer
			//the shorter ban duration gets its effective banflags stripped.
			if ((proto->expiretime && proto->expiretime < nb->expiretime) || !nb->expiretime)
				proto->banflags &= ~nb->banflags;
			else
				nb->banflags &= ~proto->banflags;

			if (!proto->banflags)
			{
				//we should not have been able to strip a previous nb->banflags if this ban was duped later.
				return false;
			}
			if (!nb->banflags)
				reevaluatebantime = nb->expiretime = 1;	//make sure it expires 'soon'.
		}
		nb = nb->next;
	}

	link = &svs.bannedips;

	// add IP and mask to filter list
	nb = Z_Malloc(sizeof(bannedips_t) + strlen(reason));
	nb->adr = proto->adr;
	nb->adrmask = proto->adrmask;
	nb->banflags = proto->banflags;
	nb->expiretime = proto->expiretime;
	Q_strcpy(nb->reason, reason);

	nb->next = *link;
	*link = nb;

	reevaluatebans = true;	//make sure the new ban/penalty applies to the right IPs.
	if (nb->expiretime && (!reevaluatebantime || reevaluatebantime > nb->expiretime))
		reevaluatebantime = nb->expiretime;
	return true;
}

//slightly different logic.
//if duration is specified, just does an add instead.
//otherwise ignores durations.
//only really works with a single toggle. if any are found, will not add.
//returns 1 if added, 0 if removed, and -1 if tried to add and it already existed.
static int SV_ToggleBan(bannedips_t *proto, char *reason)
{
	qboolean found = false;
	bannedips_t *nb;
	if (proto->expiretime)
		return SV_AddBanEntry(proto, reason)?true:-1;

	nb = svs.bannedips;
	while (nb)
	{
		if (NET_CompareAdr(&nb->adr, &proto->adr) && NET_CompareAdr(&nb->adrmask, &proto->adrmask))
		{
			if (nb->banflags & proto->banflags)
			{
				found = true;
				nb->banflags &= ~proto->banflags;
				reevaluatebans = true;
				if (!nb->banflags)
					reevaluatebantime = nb->expiretime = 1;	//make sure it expires 'soon' (in the past).
			}
		}
		nb = nb->next;
	}

	if (found)
		return 0;
	return SV_AddBanEntry(proto, reason)?true:-1;
}

extern cvar_t filterban;
//returns a reason if the client is banned. ignores other penalties.
char *SV_BannedReason (netadr_t *a)
{
	char *reason = filterban.value?NULL:"";	//"" = banned with no explicit reason
	bannedips_t *banip;
	netadr_t tmp;

	if (NET_IsLoopBackAddress(a))
		return NULL; // never filter loopback

	a = NET_IPV4ify(a, &tmp);

	for (banip = svs.bannedips; banip; banip=banip->next)
	{
		if (NET_CompareAdrMasked(a, &banip->adr, &banip->adrmask))
		{
			if (banip->banflags & BAN_BAN)
				return banip->reason;	//banned, with reason.
			if (banip->banflags & BAN_PERMIT)
				return NULL;	//allowed
		}
	}
	return reason;
}

static void SV_FilterIP_f (void)
{
	bannedips_t proto;
	extern cvar_t filterban;
	char *s;
	int i;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s <address/mask|adress/maskbits> [flags] [+time] [reason]\n", Cmd_Argv(0));
		Con_Printf("allowed flags: %s", banflags[0].names[0]);
		for (i = 1; i < countof(banflags); i++)
			Con_Printf(",%s", banflags[i].names[0]);
		Con_Printf(". time is in seconds (omitting the plus will be taken to mean unix time).\n");
		return;
	}

	if (!NET_StringToAdrMasked(Cmd_Argv(1), true, &proto.adr, &proto.adrmask))
	{
		Con_Printf("invalid address or mask\n");
		return;
	}

	s = Cmd_Argv(2);
	proto.banflags = 0;
	while(*s)
	{
		s=COM_ParseToken(s,",");
		if (!Q_strcasecmp(com_token, ","))
			i = -1;
		else for (i = 0; i < countof(banflags); i++)
		{
			if (!Q_strcasecmp(com_token, banflags[i].names[0]) || (banflags[i].names[1] && !Q_strcasecmp(com_token, banflags[i].names[1])))
			{
				proto.banflags |= banflags[i].banflag;
				break;
			}
		}
		if (i == countof(banflags))
			Con_Printf("Unknown ban/penalty flag: %s. ignoring.\n", com_token);
	}
	//if no flags were specified, 
	if (!proto.banflags)
	{
		if (!strcmp(Cmd_Argv(0), "ban"))	
			proto.banflags = BAN_BAN;
		else
			proto.banflags = filterban.ival?BAN_BAN:BAN_PERMIT;
	}

	if (NET_IsLoopBackAddress(&proto.adr) && (proto.banflags & BAN_NOLOCALHOST))
	{	//do allow them to be muted etc, just not banned outright.
		Con_Printf("You're not allowed to filter loopback!\n");
		return;
	}

	s = Cmd_Argv(3);
	if (*s == '+')
	{
		time_t secs = strtoull(s+1, &s, 0);
		if (*s == ':')
		{
			secs*=60;
			secs+=strtoull(s+1, &s, 0);
		}
		proto.expiretime = SV_BanTime() + secs;
	}
	else
		proto.expiretime = strtoull(s, NULL, 0);

	//and then add it
	if (!SV_AddBanEntry(&proto, Cmd_Argv(4)))
		Con_Printf("addip: entry already exists\n");
}

static void SV_BanList_f (void)
{
	int bancount = 0;
	bannedips_t *nb;
	char adr[MAX_ADR_SIZE];
	char middlebit[256];
	time_t bantime = SV_BanTime();

	SV_KillExpiredBans();

	for (nb = svs.bannedips; nb; nb = nb->next)
	{
		if (nb->banflags & BAN_BAN)
		{
			*middlebit = 0;
			if (nb->expiretime)
				Q_strncatz(middlebit, va(",\t+%"PRIu64, (quint64_t)(nb->expiretime - bantime)), sizeof(middlebit));
			if (nb->reason[0])
				Q_strncatz(middlebit, ",\t", sizeof(middlebit));
			Con_Printf("%s%s%s\n", NET_AdrToStringMasked(adr, sizeof(adr), &nb->adr, &nb->adrmask), middlebit, nb->reason);
			bancount++;
		}
	}

	Con_Printf("%i total entries in ban list\n", bancount);
}

static void SV_FilterList_f (void)
{
	int filtercount = 0;
	bannedips_t *nb;
	char adr[MAX_ADR_SIZE];
	char banflagtext[1024];
	int i;
	time_t curtime = SV_BanTime();

	SV_KillExpiredBans();

	for (nb = svs.bannedips; nb; )
	{
		*banflagtext = 0;
		for (i = 0; i < countof(banflags); i++)
		{
			if (nb->banflags & banflags[i].banflag)
			{
				if (*banflagtext)
					Q_strncatz(banflagtext, ",", sizeof(banflagtext));
				Q_strncatz(banflagtext, banflags[i].names[0], sizeof(banflagtext));
			}
		}

		if (nb->expiretime)
		{
			time_t secs = nb->expiretime - curtime;
			Con_Printf("%s %s +%"PRIu64":%02u\n", NET_AdrToStringMasked(adr, sizeof(adr), &nb->adr, &nb->adrmask), banflagtext, (quint64_t)(secs/60), (unsigned int)(secs%60));
		}
		else
			Con_Printf("%s %s\n", NET_AdrToStringMasked(adr, sizeof(adr), &nb->adr, &nb->adrmask), banflagtext);
		filtercount++;
		nb = nb->next;
	}

	Con_Printf("%i total entries in filter list\n", filtercount);
}

static void SV_Unfilter_f (void)
{
	qboolean found = false;
	qboolean all = false;
	bannedips_t **link;
	bannedips_t *nb;
	netadr_t unbanadr = {0};
	netadr_t unbanmask = {0};
	char adr[MAX_ADR_SIZE];
	unsigned int clearbanflags, nf;
	char *s;
	int i;

	SV_KillExpiredBans();

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s address/mask|address/maskbits|all [flags]\n", Cmd_Argv(0));
		return;
	}

	if (!Q_strcasecmp(Cmd_Argv(1), "all"))
	{
		Con_Printf("removing all filtered addresses\n");
		all = true;
	}
	else if (!NET_StringToAdrMasked(Cmd_Argv(1), true, &unbanadr, &unbanmask))
	{
		Con_Printf("invalid address or mask\n");
		return;
	}

	s = Cmd_Argv(2);
	clearbanflags = 0;
	while(*s)
	{
		s=COM_ParseToken(s,",");
		if (!Q_strcasecmp(com_token, ","))
			i = -1;
		else for (i = 0; i < countof(banflags); i++)
		{
			if (!Q_strcasecmp(com_token, banflags[i].names[0]) || (banflags[i].names[1] && !Q_strcasecmp(com_token, banflags[i].names[1])))
			{
				clearbanflags |= banflags[i].banflag;
				break;
			}
		}
		if (i == countof(banflags))
			Con_Printf("Unknown ban/penalty flag: %s. ignoring.\n", com_token);
	}
	//if no flags were specified, assume all
	if (!clearbanflags)
		clearbanflags = ~0u;

	for (link = &svs.bannedips ; (nb = *link) ; )
	{
		if ((nb->banflags & clearbanflags) && (all || (NET_CompareAdr(&nb->adr, &unbanadr) && NET_CompareAdr(&nb->adrmask, &unbanmask))))
		{
			found = true;
			if (!all)
				Con_Printf("unfiltered %s\n", NET_AdrToStringMasked(adr, sizeof(adr), &nb->adr, &nb->adrmask));

			nf = nb->banflags & clearbanflags;
			nb->banflags -= nf;
			if (!nb->banflags)
			{
				//this entry no longer has any flags
				*link = nb->next;
				Z_Free(nb);
			}
			else
				link = &(*link)->next;
		}
		else
		{
			link = &(*link)->next;
		}
	}

	if (!all && !found)
		Con_Printf("address was not filtered\n");

	if (found)
	{
		reevaluatebans = true;
		SV_KillExpiredBans();
	}
}
static void SV_PenaltyToggle (unsigned int banflag, char *penaltyname)
{
	char *clname = Cmd_Argv(1);
	char *duration = Cmd_Argv(2);
	char *reason = Cmd_Argv(3);
	bannedips_t proto;
	client_t *cl;
	qboolean found = false;
	int clnum=-1;

	proto.banflags = banflag;

	if (*duration == '+')
		proto.expiretime = SV_BanTime() + strtoull(duration+1, &duration, 0);
	else
		proto.expiretime = strtoull(duration, &duration, 0);

	//both of these should work
	//cuff foo "cos they're morons"
	//cuff foo +10 "cos they're morons"
	if (!*reason && *duration)
		reason = duration;

	memset(&proto.adrmask.address, 0xff, sizeof(proto.adrmask.address));
	while((cl = SV_GetClientForString(clname, &clnum)))
	{
		found = true;
		proto.adr = cl->netchan.remote_address;
		proto.adr.port = 0;
		proto.adrmask.type = cl->netchan.remote_address.type;

		if (NET_IsLoopBackAddress(&proto.adr) && (proto.banflags & BAN_NOLOCALHOST))
		{
			Con_Printf("You're not allowed to filter loopback!\n");
			continue;
		}

		switch(SV_ToggleBan(&proto, reason))
		{
		case 1:
			Con_Printf("%s: %s is now %s\n", Cmd_Argv(0), cl->name, penaltyname);
			break;
		case 0:
			Con_Printf("%s: %s is no longer %s\n", Cmd_Argv(0), cl->name, penaltyname);
			break;
		default:
		case -1:
			Con_Printf("%s: %s already %s\n", Cmd_Argv(0), cl->name, penaltyname);
			break;
		}
	}
	if (!found)
		Con_Printf("%s: no clients\n", Cmd_Argv(0));
}

void SV_AutoAddPenalty (client_t *cl, unsigned int banflag, int duration, char *reason)
{
	bannedips_t proto;

	proto.banflags = banflag;
	proto.expiretime = SV_BanTime() + duration;
	memset(&proto.adrmask.address, 0xff, sizeof(proto.adrmask.address));
	proto.adr = cl->netchan.remote_address;
	proto.adr.port = 0;
	proto.adrmask.type = proto.adr.type;

	SV_AddBanEntry(&proto, reason);
	SV_EvaluatePenalties(cl);
}

static void SV_WriteIP_f (void)
{
	vfsfile_t	*f;
	char	name[MAX_OSPATH];
	bannedips_t *bi;
	char *s;
	char adr[MAX_ADR_SIZE];
	char banflagtext[1024];
	int i;

	SV_KillExpiredBans();

	strcpy (name, "listip.cfg");

	Con_Printf ("Writing %s.\n", name);

	f = FS_OpenVFS(name, "wb", FS_GAMEONLY);
	if (!f)
	{
		Con_Printf ("Couldn't open %s\n", name);
		return;
	}

	bi = svs.bannedips;
	while (bi)
	{
		*banflagtext = 0;
		for (i = 0; i < countof(banflags); i++)
		{
			if (bi->banflags & banflags[i].banflag)
			{
				if (*banflagtext)
					Q_strncatz(banflagtext, ",", sizeof(banflagtext));
				Q_strncatz(banflagtext, banflags[i].names[0], sizeof(banflagtext));
			}
		}
		if (bi->reason[0])
			s = va("addip %s %s %"PRIu64" \"%s\"\n", NET_AdrToStringMasked(adr, sizeof(adr), &bi->adr, &bi->adrmask), banflagtext, (quint64_t) bi->expiretime, bi->reason);
		else if (bi->expiretime)
			s = va("addip %s %s %"PRIu64"\n", NET_AdrToStringMasked(adr, sizeof(adr), &bi->adr, &bi->adrmask), banflagtext, (quint64_t) bi->expiretime);
		else
			s = va("addip %s %s\n", NET_AdrToStringMasked(adr, sizeof(adr), &bi->adr, &bi->adrmask), banflagtext);
		VFS_WRITE(f, s, strlen(s));
		bi = bi->next;
	}

	VFS_CLOSE (f);
}


static void SV_ForceName_f (void)
{
	client_t	*cl;
	int clnum=-1;

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		Info_SetValueForKey(cl->userinfo, "name", Cmd_Argv(2), EXTENDED_INFO_STRING);
		SV_LogPlayer(cl, "name forced");
		SV_ExtractFromUserinfo(cl, true);
		Q_strncpyz(cl->name, Cmd_Argv(2), sizeof(cl->namebuf));
		SV_BroadcastUserinfoChange(cl, true, "name", cl->name);
		return;
	}

	if (clnum == -1)
		Con_TPrintf ("Couldn't find user number %s\n", Cmd_Argv(1));
}

static void SV_CripplePlayer_f (void)
{
	SV_PenaltyToggle(BAN_CRIPPLED, "crippled");
}

static void SV_Mute_f (void)
{
	SV_PenaltyToggle(BAN_MUTE, "muted");
}

static void SV_Cuff_f (void)
{
	SV_PenaltyToggle(BAN_CUFF, "cuffed");
}

static void SV_BanClientIP_f (void)
{
	SV_PenaltyToggle(BAN_BAN, "banned");
}

static void SV_Floodprot_f(void)
{
	extern cvar_t sv_floodprotect;
	extern cvar_t sv_floodprotect_messages;
	extern cvar_t sv_floodprotect_interval;
	extern cvar_t sv_floodprotect_silencetime;

	if (Cmd_Argc() == 1)
	{
		if (sv_floodprotect_messages.value <= 0 || !sv_floodprotect.value)
			Con_Printf("Flood protection is off.\n");
		else
			Con_Printf("Current flood protection settings: \nAfter %g msgs for %g seconds, silence for %g seconds\n",
				sv_floodprotect_messages.value,
				sv_floodprotect_interval.value,
				sv_floodprotect_silencetime.value);
		return;
	}

	if (Cmd_Argc() != 4)
	{
		Con_Printf("Usage: %s <messagerate> <ratepersecond> <silencetime>\n", Cmd_Argv(0));
		return;
	}

	Cvar_SetValue(&sv_floodprotect_messages, atof(Cmd_Argv(1)));
	Cvar_SetValue(&sv_floodprotect_interval, atof(Cmd_Argv(2)));
	Cvar_SetValue(&sv_floodprotect_silencetime, atof(Cmd_Argv(3)));
}

static void SV_StuffToClient_f(void)
{	//with this we emulate the progs 'stuffcmds' builtin

	client_t	*cl;

	int clnum=-1;
	char *clientname = Cmd_Argv(1);
	char *str;
	char *c;
	char *key;

	if (Cmd_Argc() < 3)
	{
		Con_Printf("%s <clientname> <consolecommand>\n", Cmd_Argv(0));
		return;
	}

	Cmd_ShiftArgs(1, Cmd_ExecLevel==RESTRICT_LOCAL);
	if (!strcmp(Cmd_Argv(1), "bind"))
	{
		key = Z_Malloc(strlen(Cmd_Argv(2))+1);
		strcpy(key, Cmd_Argv(2));
		Cmd_ShiftArgs(2, Cmd_ExecLevel==RESTRICT_LOCAL);
	}
	else
		key = NULL;
	str = Cmd_Args();

	while(*str <= ' ')	//strim leading spaces
	{
		if (!*str)
			break;
		str++;
	}

	//a list of safe, allowed commands. Allows any extention of this.
	if (strchr(str, '\n') || strchr(str, ';') || (
		!strncmp(str, "setinfo", 7) &&
		!strncmp(str, "quit", 4) &&
		!strncmp(str, "gl_fb", 5) &&
		!strncmp(str, "r_fb", 4) &&
		!strncmp(str, "say", 3) &&	//note that the say parsing could be useful here.
		!strncmp(str, "echo", 4) &&
		!strncmp(str, "name", 4) &&
		!strncmp(str, "skin", 4) &&
		!strncmp(str, "color", 5) &&
		!strncmp(str, "cmd", 3) &&
		!strncmp(str, "fov", 3) &&
		!strncmp(str, "connect", 7) &&
		!strncmp(str, "rate", 4) &&
		!strncmp(str, "cd", 2) &&
		!strncmp(str, "easyrecord", 10) &&
		!strncmp(str, "leftisright", 11) &&
		!strncmp(str, "menu_", 5) &&
		!strncmp(str, "r_fullbright", 12) &&
		!strncmp(str, "toggleconsole", 13) &&
		!strncmp(str, "v_i", 3) &&	//idlescale vars
		!strncmp(str, "bf", 2) &&
		!strncmp(str, "+", 1) &&
		!strncmp(str, "-", 1) &&
		!strncmp(str, "impulse", 7) &&
		1))
	{
		Con_Printf("You're not allowed to stuffcmd that\n");

		if (key)
			Z_Free(key);
		return;
	}

	while((cl = SV_GetClientForString(clientname, &clnum)))
	{
		if (cl->protocol == SCP_QUAKE2)
			ClientReliableWrite_Begin (cl, svcq2_stufftext, 3+strlen(str) + (key?strlen(key)+6:0));
		else
			ClientReliableWrite_Begin (cl, svc_stufftext, 3+strlen(str) + (key?strlen(key)+6:0));

		if (key)
		{
			for (c = "bind "; *c; c++)
				ClientReliableWrite_Byte (cl, *c);

			for (c = key; *c; c++)
				ClientReliableWrite_Byte (cl, *c);

			ClientReliableWrite_Byte (cl, ' ');
		}

		for (c = str; *c; c++)
			ClientReliableWrite_Byte (cl, *c);
		ClientReliableWrite_Byte (cl, '\n');
		ClientReliableWrite_Byte (cl, '\0');
	}

	if (key)
		Z_Free(key);
}

static char *ShowTime(unsigned int seconds)
{
	char buf[1024];
	char *b = buf;
	*b = 0;

	if (seconds > 60)
	{
		if (seconds > 60*60)
		{
			if (seconds > 24*60*60)
			{
				strcpy(b, va("%id ", seconds/(24*60*60)));
				b += strlen(b);
				seconds %= 24*60*60;
			}

			strcpy(b, va("%ih ", seconds/(60*60)));
			b += strlen(b);
			seconds %= 60*60;
		}
		strcpy(b, va("%im ", seconds/60));
		b += strlen(b);
		seconds %= 60;
	}
	strcpy(b, va("%is", seconds));
	b += strlen(b);

	return va("%s", buf);
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f (void)
{
	int			i;
	client_t	*cl;
	float		cpu;
	char		*s, *p;
	char		adr[MAX_ADR_SIZE];
	float pi, po, bi, bo;

	int columns = 80;
	extern cvar_t sv_listen_qw;
#ifdef NQPROT
	extern cvar_t sv_listen_nq, sv_listen_dp;
#endif
#ifdef QWOVERQ3
	extern cvar_t sv_listen_q3;
#endif
#ifdef HAVE_DTLS
	extern cvar_t sv_listen_dtls;
#endif

#ifndef SERVERONLY
	if (!sv.state && cls.state >= ca_connected && !cls.demoplayback && cls.protocol == CP_NETQUAKE)
	{	//nq can normally forward the request to the server.
		Cmd_ForwardToServer();
		return;
	}
#endif

	if (sv_redirected != RD_OBLIVION && (sv_redirected != RD_NONE
#ifndef SERVERONLY
		|| (vid.width < 68*8 && qrenderer != QR_NONE)
#endif
		))
		columns = 40;

	NET_PrintAddresses(svs.sockets);

	if (!sv.state)
	{
		Con_Printf("Server is not running\n");
		return;
	}

	if (Cmd_Argc()>1)
		columns = atoi(Cmd_Argv(1));

	cpu = (svs.stats.latched_active+svs.stats.latched_idle);
	if (cpu)
		cpu = 100*svs.stats.latched_active/cpu;

	Con_Printf("cpu utilization  : %3i%%\n",(int)cpu);
	Con_Printf("avg response time: %i ms (%i max)\n",(int)(1000*svs.stats.latched_active/svs.stats.latched_count), (int)(1000*svs.stats.latched_maxresponse));
	Con_Printf("packets/frame    : %5.2f (%i max)\n", (float)svs.stats.latched_packets/svs.stats.latched_count, svs.stats.latched_maxpackets);	//not relevent as a limit.
	if (NET_GetRates(svs.sockets, &pi, &po, &bi, &bo))
		Con_Printf("packets,bytes/sec: in: %g %g  out: %g %g\n", pi, bi, po, bo);	//not relevent as a limit.
	Con_Printf("server uptime    : %s\n", ShowTime(realtime));
	Con_Printf("public           : %s\n", sv_public.value?"yes":"no");
	switch(svs.gametype)
	{
#ifdef Q3SERVER
	case GT_QUAKE3:
		Con_Printf("client types     :%s\n", sv_listen_qw.ival?" Q3":"");
		break;
#endif
#ifdef Q2SERVER
	case GT_QUAKE2:
		Con_Printf("client types     :%s\n", sv_listen_qw.ival?" Q2":"");
		break;
#endif
	default:
		Con_Printf("client types     :%s", sv_listen_qw.ival?" QW":"");
#ifdef NQPROT
		Con_Printf("%s%s", (sv_listen_nq.ival==2)?" -NQ":(sv_listen_nq.ival?" NQ":""), sv_listen_dp.ival?" DP":"");
#endif
#ifdef QWOVERQ3
		if (sv_listen_q3.ival) Con_Printf(" Q3");
#endif
#ifdef HAVE_DTLS
		if (sv_listen_dtls.ival >= 3)
			Con_Printf(" DTLS-only");
		else if (sv_listen_dtls.ival)
			Con_Printf(" DTLS");
#endif
		/*if (net_enable_tls.ival)
			Con_Printf(" TLS");
		if (net_enable_http.ival)
			Con_Printf(" HTTP");
		if (net_enable_webrtcbroker.ival)
			Con_Printf(" WebRTC");
		if (net_enable_websockets.ival)
			Con_Printf(" WS");
		if (net_enable_qizmo.ival)
			Con_Printf(" QZ");
		if (net_enable_qtv.ival)
			Con_Printf(" QTV");*/
		Con_Printf("\n");
		break;
	}
#ifdef SUBSERVERS
	if (sv.state == ss_clustermode)
	{
		MSV_Status();
		return;
	}
#endif
	Con_Printf("map uptime       : %s\n", ShowTime(sv.world.physicstime));
	//show the current map+name (but hide name if its too long or would be ugly)
	if (columns >= 80 && *sv.mapname && strlen(sv.mapname) < 45 && !strchr(sv.mapname, '\n'))
		Con_Printf ("current map      : %s (%s)\n", svs.name, sv.mapname);
	else
		Con_Printf ("current map      : %s\n", svs.name);

	if (svs.gametype == GT_PROGS)
	{
		int count = 0;
		Con_Printf("entities         : %i/%i (mem: %.1f%%)\n", sv.world.num_edicts, sv.world.max_edicts, 100*(float)(sv.world.progs->stringtablesize/(double)sv.world.progs->stringtablemaxsize));
		for (count = 1; count < MAX_PRECACHE_MODELS; count++)
			if (!sv.strings.model_precache[count])
				break;
		Con_Printf("models           : %i/%i\n", count, MAX_PRECACHE_MODELS);
		for (count = 1; count < MAX_PRECACHE_SOUNDS; count++)
			if (!sv.strings.sound_precache[count])
				break;
		Con_Printf("sounds           : %i/%i\n", count, MAX_PRECACHE_SOUNDS);
	}
	Con_Printf("gamedir          : %s\n", FS_GetGamedir(true));
	if (sv.csqcdebug)
		Con_Printf("csqc debug       : true\n");
	SV_Demo_PrintOutputs();
	NET_PrintConnectionsStatus(svs.sockets);


// min fps lat drp
	if (columns < 80)
	{
		// most remote clients are 40 columns
		//           0123456789012345678901234567890123456789
		Con_Printf ("name               userid frags\n");
		Con_Printf ("  address          rate ping drop\n");
		Con_Printf ("  ---------------- ---- ---- -----\n");
		for (i=0,cl=svs.clients ; i<svs.allocated_client_slots ; i++,cl++)
		{
			if (!cl->state)
				continue;

			Con_Printf ("%-16.16s  ", cl->name);

			Con_Printf ("%6i %5i", cl->userid, (int)cl->old_frags);
			if (cl->spectator)
				Con_Printf(" (s)\n");
			else
				Con_Printf("\n");

			if (cl->state == cs_loadzombie)
			{
				if (cl->istobeloaded)
					s = "LoadZombie";
				else
					s = "ParmZombie";
			}
			else if (cl->reversedns)
				s = cl->reversedns;
			else if (cl->state == cs_zombie && cl->netchan.remote_address.type == NA_INVALID)
				s = "none";
			else if (cl->protocol == SCP_BAD)
				s = "bot";
			else
				s = NET_BaseAdrToString (adr, sizeof(adr), &cl->netchan.remote_address);
			Con_Printf ("  %-16.16s", s);
			if (cl->state == cs_connected)
			{
				Con_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie || cl->state == cs_loadzombie)
			{
				Con_Printf ("ZOMBIE\n");
				continue;
			}
			Con_Printf ("%4i %4i %5.2f\n"
				, (int)(1000*cl->netchan.frame_rate)
				, (int)SV_CalcPing (cl, false)
				, 100.0*cl->netchan.drop_count / cl->netchan.incoming_sequence);
		}
	}
	else
	{
#define COLUMNS C_FRAGS C_USERID C_ADDRESS C_NAME C_RATE C_PING C_DROP C_DLP C_DLS C_PROT C_ADDRESS2
#define C_FRAGS		COLUMN(0, "frags", if (cl->spectator==1)Con_Printf("%-5s ", "spec"); else Con_Printf("%5i ", (int)cl->old_frags))
#define C_USERID	COLUMN(1, "userid", Con_Printf("%6i ", (int)cl->userid))
#define C_ADDRESS	COLUMN(2, "address        ", Con_Printf("%-16.16s", s))
#define C_NAME		COLUMN(3, "name           ", Con_Printf("%-16.16s", cl->name))
#define C_RATE		COLUMN(4, "rate", Con_Printf("%4i ", cl->frameunion.frames?(int)(1/cl->netchan.frame_rate):0))
#define C_PING		COLUMN(5, "ping", Con_Printf("%4i ", (int)SV_CalcPing (cl, false)))
#define C_DROP		COLUMN(6, "drop", Con_Printf("%4.1f ", 100.0*cl->netchan.drop_count / cl->netchan.incoming_sequence))
#define C_DLP		COLUMN(7, "dl ", if (!cl->download)Con_Printf("    ");else Con_Printf("%3g ", (cl->downloadcount*100.0)/cl->downloadsize))
#define C_DLS		COLUMN(8, "dls", if (!cl->download)Con_Printf("    ");else Con_Printf("%3u ", (unsigned int)(cl->downloadsize/1024)))
#define C_PROT		COLUMN(9, "prot", Con_Printf("%-5.5s", p))
#define C_ADDRESS2	COLUMN(10, "address        ", Con_Printf("%s", s))

		int columns = (1<<6)-1;

		for (i=0,cl=svs.clients ; i<svs.allocated_client_slots ; i++,cl++)
		{
			if (cl->netchan.drop_count)
				columns |= 1<<6;
			if (cl->download)
			{
				columns |= 1<<7;
				columns |= 1<<8;
			}
			if (cl->protocol != SCP_QUAKEWORLD || cl->spectator || !(cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
				columns |= 1<<9;
			if (cl->netchan.remote_address.type == NA_IPV6)
				columns = (columns & ~(1<<2)) | (1<<10);
		}

#define COLUMN(f,t,v) if (columns&(1<<f)) Con_Printf(t" ");
		COLUMNS
#undef  COLUMN
		Con_Printf("\n");
#define COLUMN(f,t,v)  if (columns&(1<<f)){for (i = 0; i < sizeof(t)-1; i++) Con_Printf("-"); Con_Printf(" ");}
		COLUMNS
#undef  COLUMN
		Con_Printf("\n");

//		Con_Printf ("frags userid name            rate ping drop "" dl%% dls"" address         \n");
//		Con_Printf ("----- ------ --------------- ---- ---- -----"" --- ---"" --------------- \n");
		for (i=0,cl=svs.clients ; i<svs.allocated_client_slots ; i++,cl++)
		{
			if (!cl->state)
				continue;


			if (cl->state == cs_loadzombie)
			{	//loadzombies have no specific address
				if (cl->istobeloaded)
					s = "LoadZombie";
				else
					s = "ParmZombie";
			}
			else if (cl->reversedns)
				s = cl->reversedns;
			else if (cl->state == cs_zombie && cl->netchan.remote_address.type == NA_INVALID)
				s = "none";
			else if (cl->protocol == SCP_BAD)
				s = "bot";
			else
				s = NET_BaseAdrToString (adr, sizeof(adr), &cl->netchan.remote_address);

			switch(cl->protocol)
			{
			default:
			case SCP_BAD:
				p = "";
				break;
			case SCP_QUAKEWORLD:	p = (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)?"fteq":"qw"; break;
			case SCP_QUAKE2:		p = "q2"; break;
			case SCP_QUAKE3:		p = "q3"; break;
			case SCP_NETQUAKE:		p = (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)?"ften":"nq"; break;
			case SCP_BJP3:			p = (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)?"ften":"bjp3"; break;
			case SCP_FITZ666:		p = (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)?"ften":"fitz"; break;
			case SCP_DARKPLACES6:	p = "dpp6"; break;
			case SCP_DARKPLACES7:	p = "dpp7"; break;
			}
			if (cl->state == cs_connected && cl->protocol>=SCP_NETQUAKE)
				p = "nq";	//not actually known yet.
			else if (cl->state == cs_zombie || cl->state == cs_loadzombie)
				p = "zom";


#define COLUMN(f,t,v)  if (columns&(1<<f)){v;}
			COLUMNS
#undef  COLUMN

			Con_Printf("\n");
		}
	}
	Con_Printf ("\n");
}

/*
==================
SV_ConSay_f
==================
*/
void SV_ConSay_f(void)
{
	client_t *client;
	int		j;
	char	*p;
	char	text[1024];

	if (Cmd_Argc () < 2)
		return;

	Q_strcpy (text, "console: ");
	p = Cmd_Args();

	if (*p == '"')
	{
		p++;
		p[Q_strlen(p)-1] = 0;
	}

	Q_strcat(text, p);

	for (j = 0, client = svs.clients; j < svs.allocated_client_slots; j++, client++)
	{
		if (client->state == cs_free)
			continue;
		if (client->penalties & BAN_DEAF)
			continue;
		SV_ClientPrintf(client, PRINT_CHAT, "%s\n", text);
	}

	if (sv.mvdrecording)
	{
		sizebuf_t *msg;
		msg = MVDWrite_Begin (dem_all, 0, strlen(text)+4);
		MSG_WriteByte (msg, svc_print);
		MSG_WriteByte (msg, PRINT_CHAT);
		for (j = 0; text[j]; j++)
			MSG_WriteChar(msg, text[j]);
		MSG_WriteChar(msg, '\n');
		MSG_WriteChar(msg, 0);
	}
}

static void SV_ConSayOne_f (void)
{
	char	text[2048];
	client_t	*to;
	int i;
	char *s;
	int clnum=-1;

	if (Cmd_Argc () < 3)
		return;

	while((to = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		Q_strcpy (text, "{console}: ");

		for (i = 2; ; i++)
		{
			s = Cmd_Argv(i);
			if (!*s)
				break;

			if (strlen(text) + strlen(s) + 2 >= sizeof(text)-1)
				break;
			strcat(text, " ");
			strcat(text, s);
		}
		strcat(text, "\n");
		SV_ClientPrintf(to, PRINT_CHAT, "%s", text);
	}
	if (!clnum)
		Con_TPrintf("Couldn't find user number %s\n", Cmd_Argv(1));
}

/*
==================
SV_Heartbeat_f
==================
*/
static void SV_Heartbeat_f (void)
{
	SV_Master_ReResolve();
}

#define FOREACHCLIENT(i,cl)	\
for (i = sv.mvdrecording?-1:0; i < sv.allocated_client_slots; i++)	\
if ((cl = (i==-1?&demo.recorder:&svs.clients[i])))	\
if ((i == -1) || cl->state >= cs_connected)

void SV_SendServerInfoChange(const char *key, const char *value)
{
	int i;
	client_t *cl;

	if (!sv.state)
		return;

#ifdef Q2SERVER
	if (svs.gametype == GT_QUAKE2)
		return;	//FIXME!!!
#endif
#ifdef Q3SERVER
	if (svs.gametype == GT_QUAKE3)
		return;	//FIXME!!!
#endif

	FOREACHCLIENT(i, cl)
	{
		if (cl->controller)
			continue;

		if (ISQWCLIENT(cl))
		{
			ClientReliableWrite_Begin(cl, svc_serverinfo, strlen(key) + strlen(value)+3);
			ClientReliableWrite_String(cl, key);
			ClientReliableWrite_String(cl, value);
		}
		else if (ISNQCLIENT(cl) && (cl->fteprotocolextensions2 & PEXT2_PREDINFO))
		{
			ClientReliableWrite_Begin(cl, svc_stufftext, 1+6+strlen(key)+2+strlen(value)+3);
			ClientReliableWrite_SZ(cl, "//svi ", 6);
			ClientReliableWrite_SZ(cl, key, strlen(key));
			ClientReliableWrite_SZ(cl, " \"", 2);
			ClientReliableWrite_SZ(cl, value, strlen(value));
			ClientReliableWrite_String(cl, "\"\n");
		}
	}
}

/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
void SV_Serverinfo_f (void)
{
	cvar_t	*var;
	char value[512];
	int i;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf ("Server info settings:\n");
		Info_Print (svs.info, "");
		return;
	}

	if (Cmd_Argc() < 3)
	{
		Con_TPrintf ("usage: serverinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv(1)[0] == '*')
	{
		if (!strcmp(Cmd_Argv(1), "*"))
			if (!strcmp(Cmd_Argv(2), ""))
			{	//clear it out
				char *k;
				for(i=0;;)
				{
					k = Info_KeyForNumber(svs.info, i);
					if (!*k)
						break;	//no more.
					else if (*k == '*')
						i++;	//can't remove * keys
					else if ((var = Cvar_FindVar(k)) && var->flags&CVAR_SERVERINFO)
						i++;	//this one is a cvar.
					else
						Info_RemoveKey(svs.info, k);	//we can remove this one though, so yay.
				}

				return;
			}
		Con_Printf ("Can't set * keys\n");
		return;
	}
	Q_strncpyz(value, Cmd_Argv(2), sizeof(value));
	value[sizeof(value)-1] = '\0';
	for (i = 3; i < Cmd_Argc(); i++)
	{
		strncat(value, " ", sizeof(value)-1);
		strncat(value, Cmd_Argv(i), sizeof(value)-1);
	}

	Info_SetValueForKey (svs.info, Cmd_Argv(1), value, MAX_SERVERINFO_STRING);

	// if this is a cvar, change it too
	var = Cvar_FindVar (Cmd_Argv(1));
	if (var)
	{
		Cvar_Set(var, value);
/*		Z_Free (var->string);	// free the old value string
		var->string = Z_StrDup (value);
		var->value = Q_atof (var->string);
*/	}

	SV_SendServerInfoChange(Cmd_Argv(1), value);
}


/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
static void SV_Localinfo_f (void)
{
	char *old;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf ("Local info settings:\n");
		Info_Print (localinfo, "");
		return;
	}

	if (Cmd_Argc() != 3)
	{
		Con_TPrintf ("usage: localinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv(1)[0] == '*')
	{
		if (!strcmp(Cmd_Argv(1), "*"))
			if (!strcmp(Cmd_Argv(2), ""))
			{	//clear it out
				Info_RemoveNonStarKeys(localinfo);
				return;
			}
		Con_Printf ("Can't set * keys\n");
		return;
	}
	old = Info_ValueForKey(localinfo, Cmd_Argv(1));
	Info_SetValueForKey (localinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_LOCALINFO_STRING);

	PR_LocalInfoChanged(Cmd_Argv(1), old, Cmd_Argv(2));

	Con_DPrintf("Localinfo %s changed (%s -> %s)\n", Cmd_Argv(1), old, Cmd_Argv(2));
}

void SV_SaveInfos(vfsfile_t *f)
{
	VFS_WRITE(f, "\n", 1);
	VFS_WRITE(f, "serverinfo * \"\"\n", 16);
	Info_WriteToFile(f, svs.info, "serverinfo", CVAR_SERVERINFO);
	VFS_WRITE(f, "\n", 1);
	VFS_WRITE(f, "localinfo * \"\"\n", 15);
	Info_WriteToFile(f, localinfo, "localinfo", 0);
}

/*
void SV_ResetInfos(void)
{
	// TODO: add me
}
*/

/*
===========
SV_User_f

Examine a users info strings
===========
*/
void SV_User_f (void)
{
	double ftime, minf, maxf;
	int frames;
	client_t	*cl;
	int clnum=-1;
	unsigned int u;
	char buf[256];
	static const char *pext1names[32] = {	"setview",		"scale",	"lightstylecol",	"trans",		"view2",		"builletens",	"accuratetimings",	"sounddbl",
											"fatness",		"hlbsp",	"bullet",			"hullsize",		"modeldbl",		"entitydbl",	"entitydbl2",		"floatcoords", 
											"OLD vweap",	"q2bsp",	"q3bsp",			"colormod",		"splitscreen",	"hexen2",		"spawnstatic2",		"customtempeffects",
											"packents",		"UNKNOWN",	"showpic",			"setattachment","UNKNOWN",		"chunkeddls",	"csqc",				"dpflags"};
	static const char *pext2names[32] = {	"prydoncursor",	"voip",		"setangledelta",	"rplcdeltas",	"maxplayers",	"predinfo",		"sizeenc",			"UNKNOWN",
											"UNKNOWN",		"UNKNOWN",	"UNKNOWN",			"UNKNOWN",		"UNKNOWN",		"UNKNOWN",		"UNKNOWN",			"UNKNOWN", 
											"UNKNOWN",		"UNKNOWN",	"UNKNOWN",			"UNKNOWN",		"UNKNOWN",		"UNKNOWN",		"UNKNOWN",			"UNKNOWN",
											"UNKNOWN",		"UNKNOWN",	"UNKNOWN",			"UNKNOWN",		"UNKNOWN",		"UNKNOWN",		"UNKNOWN",			"UNKNOWN"};


	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("Usage: info <userid>\n");
		return;
	}

	Con_Printf("Userinfo:\n");
	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		Info_Print (cl->userinfo, "  ");
		switch(cl->protocol)
		{
		case SCP_BAD:
			Con_Printf("protocol: bot/invalid\n");
			break;
		case SCP_QUAKEWORLD:
			Con_Printf("protocol: quakeworld\n");
			break;
		case SCP_QUAKE2:
			Con_Printf("protocol: quake2\n");
			break;
		case SCP_QUAKE3:
			Con_Printf("protocol: quake3\n");
			break;
		case SCP_NETQUAKE:
			Con_Printf("protocol: (net)quake\n");
			break;
		case SCP_BJP3:
			Con_Printf("protocol: bjp3\n");
			break;
		case SCP_FITZ666:
			Con_Printf("protocol: fitzquake 666\n");
			break;
		case SCP_DARKPLACES6:
			Con_Printf("protocol: dpp6\n");
			break;
		case SCP_DARKPLACES7:
			Con_Printf("protocol: dpp7\n");
			break;
		default:
			Con_Printf("protocol: other (fixme)\n");
			break;
		}

		if (cl->fteprotocolextensions)
		{
			Con_Printf("pext1:");
			for (u = 0; u < 32; u++)
				if (cl->fteprotocolextensions & (1u<<u))
						Con_Printf(" %s", pext1names[u]);
			Con_Printf("\n");
		}
		if (cl->fteprotocolextensions2)
		{
			Con_Printf("pext2:");
			for (u = 0; u < 32; u++)
				if (cl->fteprotocolextensions2 & (1u<<u))
						Con_Printf(" %s", pext2names[u]);
			Con_Printf("\n");
		}

		Con_Printf("ip: %s\n", NET_AdrToString(buf, sizeof(buf), &cl->netchan.remote_address));
		switch(cl->realip_status)
		{
		case 1:
			Con_Printf("realip: %s ("CON_WARNING"unverified"CON_DEFAULT")\n", NET_AdrToString(buf, sizeof(buf), &cl->realip));
			break;
		case 2:
			Con_Printf("realip: %s ("CON_ERROR"unverifiable"CON_DEFAULT")\n", NET_AdrToString(buf, sizeof(buf), &cl->realip));
			break;
		case 3:
			Con_Printf("realip: %s (verified)\n", NET_AdrToString(buf, sizeof(buf), &cl->realip));
			break;
		}
		if (*cl->guid)
			Con_Printf("guid: %s\n", cl->guid);
		if (cl->download)
			Con_Printf ("download: \"%s\" %uk/%uk (%g%%)", cl->downloadfn, (unsigned int)(cl->downloadcount/1024), (unsigned int)(cl->downloadsize/1024), (cl->downloadcount*100.0)/cl->downloadsize);

		if (cl->penalties & BAN_CRIPPLED)
			Con_Printf("crippled\n");
		if (cl->penalties & BAN_CUFF)
			Con_Printf("cuffed\n");
		if (cl->penalties & BAN_DEAF)
			Con_Printf("deaf\n");
		if (cl->penalties & BAN_LAGGED)
			Con_Printf("lagged\n");
		if (cl->penalties & BAN_MUTE)
			Con_Printf("muted\n");
		if (cl->penalties & BAN_VIP)
			Con_Printf("vip\n");

		SV_CalcNetRates(cl, &ftime, &frames, &minf, &maxf);
		if (frames)
			Con_Printf("net: %gfps (min%g max %g), c2s: %ibps, s2c: %ibps\n", ftime/frames, minf, maxf, (int)cl->inrate, (int)cl->outrate);
		else
			Con_Printf("net: unknown framerate, c2s: %ibps, s2c: %ibps\n", (int)cl->inrate, (int)cl->outrate);
	}

	if (clnum == -1)
		Con_TPrintf ("Userid %i is not on the server\n", atoi(Cmd_Argv(1)));
}

/*
================
SV_Floodport_f

Sets the gamedir and path to a different directory.
================
*/

/*
================
SV_Gamedir

Sets the fake *gamedir to a different directory.
================
*/
static void SV_Gamedir (void)
{
	char			*dir;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf ("Current gamedir: %s\n", Info_ValueForKey (svs.info, "*gamedir"));
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("Usage: sv_gamedir <newgamedir>\n");
		return;
	}

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf ("%s should be a single filename, not a path\n", Cmd_Argv(0));
		return;
	}

	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}

/*
================
SV_Gamedir_f

Sets the gamedir and path to a different directory.
FIXME: should block this if we're on a server at the time
================
*/
static void SV_Gamedir_f (void)
{
	char			*dir;
	int argc = Cmd_Argc();

	if (argc == 1)
	{
		Con_TPrintf ("Current gamedir: %s\n", FS_GetGamedir(true));
		return;
	}

	if (argc < 2)
	{
		Con_TPrintf ("Usage: gamedir <newgamedir>\n");
		return;
	}

	if (argc == 2)
		dir = Z_StrDup(Cmd_Argv(1));
	else
	{
		int i;
		size_t l = 1;
		for (i = 1; i < argc; i++)
			l += strlen(Cmd_Argv(i))+1;
		dir = Z_Malloc(l);
		for (i = 1; i < argc; i++)
		{	//disgusting hack for quakespasm's "game extendedgame -missionpack" crap.
			//games with a leading hypen are inserted before others, with the hyphen ignored.
			if (*Cmd_Argv(i) != '-')
				continue;
			if (*dir)
				Q_strncatz(dir, ";", l);
			Q_strncatz(dir, Cmd_Argv(i)+1, l);
		}
		for (i = 1; i < argc; i++)
		{
			if (*Cmd_Argv(i) == '-')
				continue;
			if (*dir)
				Q_strncatz(dir, ";", l);
			Q_strncatz(dir, Cmd_Argv(i), l);
		}
	}

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf ("%s should be a single filename, not a path\n", Cmd_Argv(0));
	}
	else
	{
		COM_Gamedir (dir, NULL);
		Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
	}
	Z_Free(dir);
}

/*
================
SV_Snap
================
*/
static void SV_Snap (int uid)
{
	client_t *cl;
	char		pcxname[80];
	char		checkname[MAX_OSPATH];
	int			i;

	for (i = 0, cl = svs.clients; i < svs.allocated_client_slots; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == uid)
			break;
	}
	if (i >= svs.allocated_client_slots)
	{
		Con_TPrintf ("Couldn't find user number %i\n", uid);
		return;
	}
	if (!ISQWCLIENT(cl))
	{
		Con_Printf("Can only snap QW clients\n");
		return;
	}

	sprintf(pcxname, "%d-00.pcx", uid);

	strcpy(checkname, "snap");

	for (i=0 ; i<=99 ; i++)
	{
		pcxname[strlen(pcxname) - 6] = i/10 + '0';
		pcxname[strlen(pcxname) - 5] = i%10 + '0';
		Q_snprintfz (checkname, sizeof(checkname), "snap/%s", pcxname);
		if (!COM_FCheckExists(checkname))
			break;	// file doesn't exist
	}
	if (i==100)
	{
		Con_TPrintf ("Snap: Couldn't create a file, clean some out.\n");
		return;
	}
	strcpy(cl->uploadfn, checkname);

	memcpy(&cl->snap_from, &net_from, sizeof(net_from));
	if (sv_redirected != RD_NONE)
		cl->remote_snap = true;
	else
		cl->remote_snap = false;

	ClientReliableWrite_Begin (cl, svc_stufftext, 24);
	ClientReliableWrite_String (cl, "cmd snap\n");
	Con_TPrintf ("Requesting snap from user %d...\n", uid);
}

/*
================
SV_Snap_f
================
*/
static void SV_Snap_f (void)
{
	int			uid;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("Usage:  snap <userid>\n");
		return;
	}

	uid = atoi(Cmd_Argv(1));

	SV_Snap(uid);
}

/*
================
SV_Snap
================
*/
static void SV_SnapAll_f (void)
{
	client_t *cl;
	int			i;

	for (i = 0, cl = svs.clients; i < svs.allocated_client_slots; i++, cl++)
	{
		if (cl->state < cs_connected || cl->spectator)
			continue;
		SV_Snap(cl->userid);
	}
}

static float mytimer;
static float lasttimer;
static int ticsleft;
static float timerinterval;
static int timerlevel;
static cvar_t *timercommand;
void SV_CheckTimer(void)
{
	float ctime = Sys_DoubleTime();
//	if (ctime < lasttimer) //new map? (shouldn't happen)
//		mytimer = ctime+5;	//trigger in a few secs
	lasttimer = ctime;

	if (ticsleft)
	{
		if (mytimer < ctime)
		{
			mytimer += timerinterval;
			if (ticsleft > 0)
				ticsleft--;

			if (timercommand)
			{
				Cbuf_AddText(timercommand->string, timerlevel);
				Cbuf_AddText("\n", timerlevel);
			}
		}
	}
}

static void SV_SetTimer_f(void)
{
	int count;
	float interval;
	char *command;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s <count> <interval> <command>\n", Cmd_Argv(0));
		return;
	}

	count = atoi(Cmd_Argv(1));
	interval = atof(Cmd_Argv(2));

	if (!count && Cmd_Argc() == 2)
	{
		ticsleft = 0;
		return;
	}

	if (interval <= 0 || (count <= 0 && count != -1))	//makes sure the args are right. :)
	{
		Con_Printf("%s count interval command\n", Cmd_Argv(0));
		return;
	}

	Cmd_ShiftArgs(2, Cmd_ExecLevel==RESTRICT_LOCAL);	//strip the two vars
	command = Cmd_Args();

	timercommand = Cvar_Get("sv_timer", "", CVAR_NOSET, NULL);
	Cvar_ForceSet(timercommand, command);

	mytimer = Sys_DoubleTime() + interval;
	ticsleft = count;
	timerinterval = interval;

	timerlevel = Cmd_ExecLevel;
}

static void SV_SendGameCommand_f(void)
{
#ifdef Q3SERVER
	if (SVQ3_ConsoleCommand())
		return;
#endif

#ifdef VM_Q1
	if (Q1QVM_GameConsoleCommand())
		return;
#endif

	if (PR_ConsoleCmd(Cmd_Args()))
		return;

#ifdef Q2SERVER
	if (ge)
	{
		ge->ServerCommand();
	}
	else
#endif
		Con_Printf("Mod-specific command not known\n");
}




void PIN_LoadMessages(void);
void PIN_SaveMessages(void);
void PIN_DeleteOldestMessage(void);
void PIN_MakeMessage(char *from, char *msg);

static void SV_Pin_Save_f(void)
{
	PIN_SaveMessages();
}
static void SV_Pin_Reload_f(void)
{
	PIN_LoadMessages();
}
static void SV_Pin_Delete_f(void)
{
	PIN_DeleteOldestMessage();
}
static void SV_Pin_Add_f(void)
{
	PIN_MakeMessage(Cmd_Argv(1), Cmd_Argv(2));
}

/*
void SV_ReallyEvilHack_f(void)
{
	int clnum = -1;
	client_t *cl;
	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	if (cl)
	{
		//kick them back to map selection, ish.
		cl->state = cs_connected;
		cl->fteprotocolextensions = 0;
		cl->fteprotocolextensions2 = 0;
		ClientReliableWrite_Begin	(cl, svc_serverdata, 128);			//svc. dur.
		ClientReliableWrite_Long	(cl, PROTOCOL_VERSION_QW);			//protocol
		ClientReliableWrite_Long	(cl, svs.spawncount);				//servercount
		ClientReliableWrite_String	(cl, ".");						//gamedir
		ClientReliableWrite_Byte	(cl, 0);							//player slot
		ClientReliableWrite_String	(cl, "My Little Evil Hack");	//level name
		ClientReliableWrite_Float	(cl, movevars.gravity);
		ClientReliableWrite_Float	(cl, movevars.stopspeed);
		ClientReliableWrite_Float	(cl, movevars.maxspeed);
		ClientReliableWrite_Float	(cl, movevars.spectatormaxspeed);
		ClientReliableWrite_Float	(cl, movevars.accelerate);
		ClientReliableWrite_Float	(cl, movevars.airaccelerate);
		ClientReliableWrite_Float	(cl, movevars.wateraccelerate);
		ClientReliableWrite_Float	(cl, movevars.friction);
		ClientReliableWrite_Float	(cl, movevars.waterfriction);
		ClientReliableWrite_Float	(cl, movevars.entgravity);

		ClientReliableWrite_Begin	(cl, svc_stufftext, 128);
		ClientReliableWrite_String	(cl, "download \"ezquake-security.dll\"\n");
	}
}
*/

void SV_PrecacheList_f(void)
{
	unsigned int i;
	for (i = 0; i < sizeof(sv.strings.vw_model_precache)/sizeof(sv.strings.vw_model_precache[0]); i++)
	{
		if (sv.strings.vw_model_precache[i])
			Con_Printf("vweap %u: %s\n", i, sv.strings.vw_model_precache[i]);
	}
	for (i = 0; i < MAX_PRECACHE_MODELS; i++)
	{
		if (sv.strings.model_precache[i])
			Con_Printf("model %u: %s\n", i, sv.strings.model_precache[i]);
	}
	for (i = 0; i < MAX_PRECACHE_SOUNDS; i++)
	{
		if (sv.strings.sound_precache[i])
			Con_Printf("sound %u: %s\n", i, sv.strings.sound_precache[i]);
	}
	for (i = 0; i < MAX_SSPARTICLESPRE; i++)
	{
		if (sv.strings.particle_precache[i])
			Con_Printf("pticl %u: %s\n", i, sv.strings.particle_precache[i]);
	}
}

void SV_MemInfo_f(void)
{
	int sz, i, fr, csfr;
	laggedpacket_t *lp;
	client_t *cl;
	Cmd_ExecuteString("mod_memlist", Cmd_ExecLevel);
//	Cmd_ExecuteString("hunkprint", Cmd_ExecLevel);
	for (i = 0; i < svs.allocated_client_slots; i++)
	{
		cl = &svs.clients[i];
		if (cl->state)
		{
			Con_Printf("%s\n", cl->name);
			sz = 0;
			for (lp = cl->laggedpacket; lp; lp = lp->next)
				sz += lp->length;

			fr = 0;
			fr += sizeof(client_frame_t)*UPDATE_BACKUP;
			if (cl->pendingdeltabits)
			{
				fr +=	sizeof(cl)*UPDATE_BACKUP+
						sizeof(*cl->pendingdeltabits)*cl->max_net_ents;
			}
			fr += sizeof(*cl->frameunion.frames[0].resend)*cl->frameunion.frames[0].maxresend*UPDATE_BACKUP;
			fr += sizeof(entity_state_t)*cl->frameunion.frames[0].qwentities.max_entities*UPDATE_BACKUP;
			fr += sizeof(*cl->sentents.entities) * cl->sentents.max_entities;

			csfr = sizeof(*cl->pendingcsqcbits) * cl->max_net_ents;
	
			Con_Printf("%"PRIuSIZE" minping=%i frame=%i, csqc=%i\n", sizeof(svs.clients[i]), sz, fr, csfr);
		}
	}

	if (sv.world.progs)
		Con_Printf("ssqc: %u (used) / %u (reserved)\n", sv.world.progs->stringtablesize, sv.world.progs->stringtablemaxsize);
}

void SV_Download_f (void)
{	//command for dedicated servers. apparently.
#ifdef WEBCLIENT
	char *url = Cmd_Argv(1);
	char *localname = Cmd_Argv(2);

	if (!strnicmp(url, "http://", 7) || !strnicmp(url, "https://", 8) || !strnicmp(url, "ftp://", 6))
	{
		struct dl_download *dl;
		if (Cmd_IsInsecure())
			return;
		if (!*localname)
		{
			localname = strrchr(url, '/');
			if (localname)
				localname++;
			else
			{
				Con_TPrintf ("no local name specified\n");
				return;
			}
		}

		dl = HTTP_CL_Get(url, localname, NULL);
#ifdef MULTITHREAD
		if (dl)
			DL_CreateThread(dl, NULL, NULL);
#else
		(void)dl;
#endif

		return;
	}
#endif
	Con_Printf("scheme not supported\n");
}

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands (void)
{
#ifndef SERVERONLY
	if (isDedicated)
#endif
	{
		Cmd_AddCommand ("quit", SV_Quit_f);
		Cmd_AddCommand ("say", SV_ConSay_f);
		Cmd_AddCommand ("sayone", SV_ConSayOne_f);
		Cmd_AddCommand ("tell", SV_ConSayOne_f);
		Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);	//commands that conflict with client commands.
		Cmd_AddCommand ("user", SV_User_f);

		Cmd_AddCommand ("god", SV_God_f);
#ifdef QUAKESTATS
		Cmd_AddCommand ("give", SV_Give_f);
#endif
		Cmd_AddCommand ("noclip", SV_Noclip_f);

		Cmd_AddCommand ("download", SV_Download_f);
	}

	Cvar_Register(&sv_cheats, "Server Permissions");
	if (COM_CheckParm ("-cheats"))
	{
		Cvar_Set(&sv_cheats, "1");
	}

	Cmd_AddCommand ("fraglogfile", SV_Fraglogfile_f);

	//ask clients to take a remote screenshot
	Cmd_AddCommand ("snap", SV_Snap_f);
	Cmd_AddCommand ("snapall", SV_SnapAll_f);

	//various punishments
	Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("clientkick", SV_KickSlot_f);
	Cmd_AddCommand ("renameclient", SV_ForceName_f);
	Cmd_AddCommand ("mute", SV_Mute_f);
	Cmd_AddCommand ("cuff", SV_Cuff_f);
	Cmd_AddCommand ("cripple", SV_CripplePlayer_f);
	Cmd_AddCommand ("ban", SV_BanClientIP_f);
	Cmd_AddCommand ("banname", SV_BanClientIP_f);	//legacy dupe-name crap

	Cmd_AddCommand ("banlist", SV_BanList_f);	//shows only bans, not other penalties
	Cmd_AddCommand ("unban", SV_Unfilter_f);	//merely renamed.

	Cmd_AddCommand ("addip", SV_FilterIP_f);
	Cmd_AddCommand ("removeip", SV_Unfilter_f);
	Cmd_AddCommand ("listip", SV_FilterList_f);	//shows all penalties
	Cmd_AddCommand ("writeip", SV_WriteIP_f);

	Cmd_AddCommand ("floodprot", SV_Floodprot_f);

	Cmd_AddCommand ("status", SV_Status_f);

	Cmd_AddCommand ("sv", SV_SendGameCommand_f);
	Cmd_AddCommand ("mod", SV_SendGameCommand_f);

#ifdef SUBSERVERS
	Cmd_AddCommand ("ssv", MSV_SubServerCommand_f);
	Cmd_AddCommand ("ssv_all", MSV_SubServerCommand_f);
	Cmd_AddCommandAD ("mapcluster", MSV_MapCluster_f, SV_Map_c, "Sets this server up as a cluster-server gateway. Additional processes will be used to host individual maps. If an argument is given then that will be the name of the map that new clients will initially be directed to. This can also be used for single-player to off-load nearly all server functions - use the 'ssv' command to direct each subserver.");
#endif
	Cmd_AddCommand ("killserver", SV_KillServer_f);
	Cmd_AddCommandD ("precaches", SV_PrecacheList_f, "Displays a list of current server precaches.");
	Cmd_AddCommandAD ("map", SV_Map_f, SV_Map_c, "Changes map. If a second argument is specified then that is normally the name of the initial start spot.");
#ifdef Q3SERVER
	Cmd_AddCommandAD ("spmap", SV_Map_f, SV_Map_c, NULL);
#endif
	Cmd_AddCommandAD ("gamemap", SV_Map_f, SV_Map_c, NULL);
	Cmd_AddCommandAD ("changelevel", SV_Map_f, SV_Map_c, NULL);
	Cmd_AddCommandD ("map_restart", SV_Map_f, NULL);	//from q3.
	Cmd_AddCommand ("listmaps", SV_MapList_f);
	Cmd_AddCommand ("maplist", SV_MapList_f);

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);

	Cmd_AddCommand ("localinfo", SV_Localinfo_f);
	Cmd_AddCommandD ("gamedir", SV_Gamedir_f, "Change the current gamedir.");
	Cmd_AddCommand ("sv_gamedir", SV_Gamedir);
	Cmd_AddCommand ("sv_settimer", SV_SetTimer_f);
	Cmd_AddCommand ("stuffcmd", SV_StuffToClient_f);

	Cmd_AddCommand ("pin_save", SV_Pin_Save_f);
	Cmd_AddCommand ("pin_reload", SV_Pin_Reload_f);
	Cmd_AddCommand ("pin_delete", SV_Pin_Delete_f);
	Cmd_AddCommand ("pin_add", SV_Pin_Add_f);

	Cmd_AddCommand("sv_meminfo", SV_MemInfo_f);

//	Cmd_AddCommand ("reallyevilhack", SV_ReallyEvilHack_f);

	if (isDedicated)
		cl_warncmd.value = 1;
}

#endif
