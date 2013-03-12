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
#include "qwsvdef.h"

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
cvar_t sv_cheats = SCVARF("sv_cheats", "0", CVAR_LATCH);
	extern		redirect_t	sv_redirected;

extern cvar_t sv_public;

//generic helper function for naming players.
client_t *SV_GetClientForString(char *name, int *id)
{
	int i;
	char *s;
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
			if (cl->state<=cs_zombie)
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
			if (cl->state<=cs_zombie)
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
		if (cl->state<=cs_zombie)
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
====================
SV_SetMaster_f

Make a master server current
====================
*/
void Master_ClearAll(void);
void Master_ReResolve(void);
void Master_Add(char *stringadr);

void SV_SetMaster_f (void)
{
	int		i;

	Cvar_Set(&sv_public, "1");	//go public.

	Master_ClearAll();

	if (!strcmp(Cmd_Argv(1), "none"))
	{
		Con_Printf ("Entering no-master mode\n");
		return;
	}

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		Master_Add(Cmd_Argv(i));
	}

	svs.last_heartbeat = -99999;
}

/*
==================
SV_Quit_f
==================
*/
void SV_Quit_f (void)
{
	SV_FinalMessage ("server shutdown\n");
	Con_TPrintf (STL_SHUTTINGDOWN);
	SV_Shutdown ();
	Sys_Quit ();
}

/*
============
SV_Fraglogfile_f
============
*/
void SV_Fraglogfile_f (void)
{
	char	name[MAX_OSPATH];
	int		i;

	if (sv_fraglogfile)
	{
		Con_TPrintf (STL_FLOGGINGOFF);
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
		Con_TPrintf (STL_FLOGGINGFAILED);
		sv_fraglogfile = NULL;
		return;
	}

	Con_TPrintf (STL_FLOGGINGTO, name);
}


/*
==================
SV_SetPlayer

Sets host_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
qboolean SV_SetPlayer (void)
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
	Con_TPrintf (STL_USERIDNOTONSERVER, idnum);
	return false;
}


/*
==================
SV_God_f

Sets client to godmode
==================
*/
void SV_God_f (void)
{
	if (!SV_MayCheat())
	{
		Con_TPrintf (STL_NEEDCHEATPARM);
		return;
	}

	if (!SV_SetPlayer ())
		return;

	SV_LogPlayer(host_client, "god cheat");
	sv_player->v->flags = (int)sv_player->v->flags ^ FL_GODMODE;
	if ((int)sv_player->v->flags & FL_GODMODE)
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_GODON);
	else
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_GODOFF);
}


void SV_Noclip_f (void)
{
	if (!SV_MayCheat())
	{
		Con_TPrintf (STL_NEEDCHEATPARM);
		return;
	}

	if (!SV_SetPlayer ())
		return;

	SV_LogPlayer(host_client, "noclip cheat");
	if (sv_player->v->movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v->movetype = MOVETYPE_NOCLIP;
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_NOCLIPON);
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_NOCLIPOFF);
	}
}


/*
==================
SV_Give_f
==================
*/
void SV_Give_f (void)
{
	char	*t;
	int		v;

	if (!SV_MayCheat())
	{
		Con_TPrintf (STL_NEEDCHEATPARM);
		return;
	}

	if (developer.value)
	{
		int oldself;
		oldself = pr_global_struct->self;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv.world.edicts);
		Con_Printf("Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, Cmd_Args()));
		pr_global_struct->self = oldself;
	}

	if (!SV_SetPlayer ())
	{
		return;
	}

	SV_LogPlayer(host_client, "give cheat");

	if (!svprogfuncs)
		return;

	t = Cmd_Argv(2);
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
	default:
		{
			int oldself;
			oldself = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			Cmd_ShiftArgs(1, false);
			Con_Printf("Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, Cmd_Args()));
			pr_global_struct->self = oldself;
		}
	}
}

int ShowMapList (const char *name, int flags, void *parm)
{
	if (name[5] == 'b' && name[6] == '_')	//skip box models
		return true;
	Con_Printf("%s\n", name+5);
	return true;
}
void SV_MapList_f(void)
{
	COM_EnumerateFiles("maps/*.bsp", ShowMapList, NULL);
	COM_EnumerateFiles("maps/*.cm", ShowMapList, NULL);
	COM_EnumerateFiles("maps/*.hmp", ShowMapList, NULL);
}

void gtcallback(struct cvar_s *var, char *oldvalue)
{
	Con_Printf("g_gametype changed\n");
}

/*
======================
SV_Map_f

handle a
map <mapname>
command from the console or progs.

quirks:
a leading '*' means new unit, meaning all old state is flushed regardless of startspot
a '+' means 'set nextmap cvar to the following value and otherwise ignore, for q2 compat. only applies if there's also a '.' and the specified bsp doesn't exist, for q1 compat.
just a '.' is taken to mean 'restart'. parms are not changed from their current values, startspot is also unchanged.

'map' will change map, for most games. note that NQ kicks everyone (NQ expects you to use changelevel for that).
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
	qboolean issamelevel = false;
	qboolean newunit = false;
	qboolean cinematic = false;
	qboolean waschangelevel = false;
	qboolean wasspmap = false;
	qboolean wasgamemap = false;
	int i;
	char *startspot;

	nextserver = 0;

#ifndef SERVERONLY
	if (!Renderer_Started() && !isDedicated)
	{
		Cbuf_AddText(va("wait;map %s\n", Cmd_Args()), Cmd_ExecLevel);
		return;
	}
#endif

	if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
	{
		Con_TPrintf (STL_MAPCOMMANDUSAGE);
		return;
	}

	sv.mapchangelocked = false;

	Q_strncpyz (level, Cmd_Argv(1), sizeof(level));
	startspot = ((Cmd_Argc() == 2)?NULL:Cmd_Argv(2));

	waschangelevel = !strcmp(Cmd_Argv(0), "changelevel");
	wasspmap = !strcmp(Cmd_Argv(0), "spmap");
	wasgamemap = !strcmp(Cmd_Argv(0), "gamemap");

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
		COM_StripExtension(COM_SkipPath(sv.modelname), level, sizeof(level));
		issamelevel = true;

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

		//override the startspot
		Q_strncpyz(spot, Info_ValueForKey(svs.info, "*startspot"), sizeof(spot));
		startspot = spot;
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

	COM_FlushFSCache();

	if (strlen(level) > 4 &&
		(!strcmp(level + strlen(level)-4, ".cin") ||
		!strcmp(level + strlen(level)-4, ".roq") ||
		!strcmp(level + strlen(level)-4, ".avi")))
	{
		cinematic = true;
	}
	else
	{
		char *exts[] = {"maps/%s.bsp", "maps/%s.cm", "maps/%s.hmp", NULL};
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
				Con_TPrintf (STL_CANTFINDMAP, expanded);
				return;
			}
		}
	}

	if (sv.mvdrecording)
		SV_MVDStop_f();

#ifndef SERVERONLY
	if (!isDedicated)	//otherwise, info used on map loading isn't present
		Cmd_ExecuteString(va("fullserverinfo \"%s\"\n", svs.info), RESTRICT_SERVER);

	if (!sv.state && cls.state)
		CL_Disconnect();
#endif

	SV_SaveSpawnparms (issamelevel);

	if (startspot && !issamelevel && !newunit)
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

		gametype = Cvar_Get("g_gametype", "0", CVAR_LATCH|CVAR_SERVERINFO, "Q3 compatability");
		gametype->callback = gtcallback;
		if (wasspmap)
			Cvar_ForceSet(gametype, "2");//singleplayer
		else if (gametype->value == 2)
			Cvar_ForceSet(gametype, "0");//force to ffa deathmatch
	}
#endif

	for (i=0 ; i<MAX_CLIENTS ; i++)	//we need to drop all q2 clients. We don't mix q1w with q2.
	{
		if (svs.clients[i].state>cs_connected)	//so that we don't send a datagram
			svs.clients[i].state=cs_connected;
	}

#ifndef SERVERONLY
	S_StopAllSounds (true);
//	SCR_BeginLoadingPlaque();
	SCR_ImageName(level);
#endif

	for (i=0, host_client = svs.clients ; i<MAX_CLIENTS ; i++, host_client++)
	{
		/*pass the new map's name as an extension, so appropriate loading screens can be shown*/
		if (ISNQCLIENT(host_client))
			SV_StuffcmdToClient(host_client, va("restart \"%s\"\n", level));
		else
			SV_StuffcmdToClient(host_client, va("changing \"%s\"\n", level));
	}
	SV_SendMessagesToAll ();

	SCR_SetLoadingFile("spawnserver");
	if (newunit || !startspot || cinematic || !SV_LoadLevelCache(NULL, level, startspot, false))
	{
		if (waschangelevel && !startspot)
			startspot = "";
		SV_SpawnServer (level, startspot, false, cinematic);
	}
	SCR_SetLoadingFile("server spawned");

	//SV_BroadcastCommand ("cmd new\n");
	for (i=0, host_client = svs.clients ; i<MAX_CLIENTS ; i++, host_client++)
	{	//this expanded code cuts out a packet when changing maps...
		//but more usefully, it stops dp(and probably nq too) from timing out.
		if (host_client->controller)
			continue;
		if (host_client->state>=cs_connected)
		{
			if (host_client->protocol == SCP_QUAKE3)
				continue;
			if (host_client->protocol == SCP_BAD)
				continue;

			if (ISNQCLIENT(host_client))
				SVNQ_New_f();
			else
				SV_New_f();
		}
	}

	if (!issamelevel)
	{
		cvar_t *nsv;
		nsv = Cvar_Get("nextserver", "", 0, "");
		if (nextserver)
			Cvar_Set(nsv, va("gamemap \"%s\"", nextserver));
		else
			Cvar_Set(nsv, "");
	}

	if (wasgamemap)
	{
		SV_Savegame("s0");
	}
}

void SV_KillServer_f(void)
{
	SV_UnspawnServer();
}


/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
void SV_Kick_f (void)
{
	client_t	*cl;
	int clnum=-1;

	if (!sv.state)
		return;

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTWASKICKED, cl->name);
		// print directly, because the dropped client won't get the
		// SV_BroadcastPrintf message
		SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREKICKED);

		SV_LogPlayer(cl, "kicked");
		SV_DropClient (cl);
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

/*for q3's kick bot menu*/
void SV_KickSlot_f (void)
{
	client_t	*cl;
	int clnum=atoi(Cmd_Argv(1));

	if (!sv.state)
		return;

	if (clnum < sv.allocated_client_slots && svs.clients[clnum].state)
	{
		cl = &svs.clients[clnum];

		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTWASKICKED, cl->name);
		// print directly, because the dropped client won't get the
		// SV_BroadcastPrintf message
		SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREKICKED);

		SV_LogPlayer(cl, "kicked");
		SV_DropClient (cl);
	}
	else
		Con_Printf("Client %i is not active\n", clnum);
}

void SV_BanName_f (void)
{
	client_t	*cl;
	int clnum=-1;
	char *reason = NULL;
	int reasonsize = 0;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s userid|nick [reason]\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() > 2)
	{
		reason = Cmd_Argv(2);
		reasonsize = strlen(reason);
	}

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	if (cl)
	{
		bannedips_t *nb;

		if (NET_IsLoopBackAddress(cl->netchan.remote_address))
		{
			Con_Printf("You're not allowed to ban loopback!\n");
			continue;
		}

		nb = Z_Malloc(sizeof(bannedips_t)+reasonsize);
		nb->next = svs.bannedips;
		nb->adr = cl->netchan.remote_address;
		NET_IntegerToMask(&nb->adr, &nb->adrmask, -1); // fill mask
		if (*Cmd_Argv(2))	//explicit blocking of all ports of a client ip
			nb->adr.port = 0;
		svs.bannedips = nb;
		if (reasonsize)
			Q_strcpy(nb->reason, reason);

		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTWASBANNED, cl->name);
		// print directly, because the dropped client won't get the
		// SV_BroadcastPrintf message
		SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREBANNED);
		SV_LogPlayer(cl, "banned name");
		SV_DropClient (cl);
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_KickBanIP(netadr_t banadr, netadr_t banmask, char *reason)
{
	qboolean shouldkick;
	client_t *cl;
	int i;
	unsigned int reasonsize;
	bannedips_t *nb;

	if (reason)
		reasonsize = strlen(reason);
	else
		reasonsize = 0;

	// loop through clients and kick the ones that match
	for (i = 0, cl = svs.clients; i < sv.allocated_client_slots; i++, cl++)
	{
		if (cl->state<=cs_zombie)
			continue;

		shouldkick = false;

		if (NET_CompareAdrMasked(cl->netchan.remote_address, banadr, banmask))
			shouldkick = true;
		else if (cl->realip_status >= 1)
			if (NET_CompareAdrMasked(cl->realip, banadr, banmask))
				shouldkick = true;

		if (shouldkick)
		{
			// match, so kick
			SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTWASBANNED, cl->name);
			// print directly, because the dropped client won't get the
			// SV_BroadcastPrintf message
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREBANNED);
			SV_LogPlayer(cl, "banned ip");
			SV_DropClient (cl);
		}
	}

	// add IP and mask to ban list
	nb = Z_Malloc(sizeof(bannedips_t)+reasonsize);
	nb->next = svs.bannedips;
	nb->adr = banadr;
	nb->adrmask = banmask;
	svs.bannedips = nb;
	if (reasonsize)
		Q_strcpy(nb->reason, reason);
}

void SV_BanIP_f (void)
{
	netadr_t banadr;
	netadr_t banmask;
	char *reason = NULL;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s address/mask|adress/maskbits [reason]\n", Cmd_Argv(0));
		return;
	}

	if (!NET_StringToAdrMasked(Cmd_Argv(1), &banadr, &banmask))
	{
		Con_Printf("invalid address or mask\n");
		return;
	}

	if (NET_IsLoopBackAddress(banadr))
	{
		Con_Printf("You're not allowed to ban loopback!\n");
		return;
	}

	if (Cmd_Argc() > 2)
		reason = Cmd_Argv(2);

	SV_KickBanIP(banadr, banmask, reason);
}

void SV_BanClientIP_f (void)
{
	netadr_t banmask;
	client_t	*cl;
	char *reason = NULL;
	int clnum=-1;

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if (NET_IsLoopBackAddress(cl->netchan.remote_address))
		{
			Con_Printf("You're not allowed to ban loopback!\n");
			continue;
		}

		if (cl->realip_status>0)
		{
			memset(&banmask.address, 0xff, sizeof(banmask.address));
			banmask.type = cl->netchan.remote_address.type;
			SV_KickBanIP(cl->realip, banmask, reason);
		}
		else
		{
			memset(&banmask.address, 0xff, sizeof(banmask.address));
			banmask.type = cl->netchan.remote_address.type;
			SV_KickBanIP(cl->netchan.remote_address, banmask, reason);
		}
	}
}

void SV_FilterIP_f (void)
{
	netadr_t banadr;
	netadr_t banmask;
	int i;
	client_t	*cl;
	bannedips_t *nb;
	extern cvar_t filterban;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s address/mask|adress/maskbits\n", Cmd_Argv(0));
		return;
	}

	if (!NET_StringToAdrMasked(Cmd_Argv(1), &banadr, &banmask))
	{
		Con_Printf("invalid address or mask\n");
		return;
	}

	if (NET_IsLoopBackAddress(banadr))
	{
		Con_Printf("You're not allowed to filter loopback!\n");
		return;
	}

	nb = svs.bannedips;
	while (nb)
	{
		if (NET_CompareAdr(nb->adr, banadr) && NET_CompareAdr(nb->adrmask, banmask))
		{
			Con_Printf("%s is already banned\n", Cmd_Argv(1));
			break;
		}
		nb = nb->next;
	}

	// loop through clients and kick the ones that match
	for (i = 0, cl = svs.clients; i < sv.allocated_client_slots; i++, cl++)
	{
		if (cl->state<=cs_zombie)
			continue;

		if (filterban.value && NET_CompareAdrMasked(cl->netchan.remote_address, banadr, banmask))
			SV_DropClient (cl);
	}

	// add IP and mask to filter list
	nb = Z_Malloc(sizeof(bannedips_t));
	nb->next = svs.bannedips;
	nb->adr = banadr;
	nb->adrmask = banmask;
	nb->type = BAN_FILTER;
	*nb->reason = 0;
	svs.bannedips = nb;
}

void SV_BanList_f (void)
{
	int bancount = 0;
	bannedips_t *nb = svs.bannedips;
	char adr[MAX_ADR_SIZE];

	while (nb)
	{
		if (nb->reason[0])
			Con_Printf("%s, %s\n", NET_AdrToStringMasked(adr, sizeof(adr), nb->adr, nb->adrmask), nb->reason);
		else
			Con_Printf("%s\n", NET_AdrToStringMasked(adr, sizeof(adr), nb->adr, nb->adrmask));
		bancount++;
		nb = nb->next;
	}

	Con_Printf("%i total entries in ban list\n", bancount);
}

void SV_FilterList_f (void)
{
	int filtercount = 0;
	bannedips_t *nb = svs.bannedips;
	char adr[MAX_ADR_SIZE];

	while (nb)
	{
		Con_Printf("%s\n", NET_AdrToStringMasked(adr, sizeof(adr), nb->adr, nb->adrmask));
		filtercount++;
		nb = nb->next;
	}

	Con_Printf("%i total entries in filter list\n", filtercount);
}

void SV_Unban_f (void)
{
	qboolean all = false;
	bannedips_t *nb = svs.bannedips;
	bannedips_t *nbnext;
	netadr_t unbanadr = {0};
	netadr_t unbanmask = {0};
	char adr[MAX_ADR_SIZE];

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s address/mask|address/maskbits|all\n", Cmd_Argv(0));
		return;
	}

	if (!Q_strcasecmp(Cmd_Argv(1), "all"))
		all = true;
	else if (!NET_StringToAdrMasked(Cmd_Argv(1), &unbanadr, &unbanmask))
	{
		Con_Printf("invalid address or mask\n");
		return;
	}

	while (nb)
	{
		nbnext = nb->next;
		if (all || (NET_CompareAdr(nb->adr, unbanadr) && NET_CompareAdr(nb->adrmask, unbanmask)))
		{
			if (!all)
				Con_Printf("unbanned %s\n", NET_AdrToStringMasked(adr, sizeof(adr), nb->adr, nb->adrmask));
			if (svs.bannedips == nb)
				svs.bannedips = nbnext;
			Z_Free(nb);

			if (!all)
				break;
		}

		nb = nbnext;
	}
}

void SV_Unfilter_f (void)
{
	qboolean all = false;
	bannedips_t *nb = svs.bannedips;
	bannedips_t *nbnext;
	netadr_t unbanadr = {0};
	netadr_t unbanmask = {0};
	char adr[MAX_ADR_SIZE];

	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s address/mask|address/maskbits|all\n", Cmd_Argv(0));
		return;
	}

	if (!Q_strcasecmp(Cmd_Argv(1), "all"))
		all = true;
	else if (!NET_StringToAdrMasked(Cmd_Argv(1), &unbanadr, &unbanmask))
	{
		Con_Printf("invalid address or mask\n");
		return;
	}

	while (nb)
	{
		nbnext = nb->next;
		if (all || (NET_CompareAdr(nb->adr, unbanadr) && NET_CompareAdr(nb->adrmask, unbanmask)))
		{
			if (!all)
				Con_Printf("unfiltered %s\n", NET_AdrToStringMasked(adr, sizeof(adr), nb->adr, nb->adrmask));
			if (svs.bannedips == nb)
				svs.bannedips = nbnext;
			Z_Free(nb);
			break;
		}

		nb = nbnext;
	}
}

void SV_WriteIP_f (void)
{
	vfsfile_t	*f;
	char	name[MAX_OSPATH];
	bannedips_t *bi;
	char *s;
	char adr[MAX_ADR_SIZE];

	strcpy (name, "listip.cfg");

	Con_Printf ("Writing %s.\n", name);

	f = FS_OpenVFS(name, "wb", FS_GAME);
	if (!f)
	{
		Con_Printf ("Couldn't open %s\n", name);
		return;
	}

	bi = svs.bannedips;
	while (bi)
	{
		if (bi->type == BAN_BAN)
			s = "banip";
		else
			s = "addip";
		if (bi->reason[0])
			s = va("%s %s \"%s\"\n", s, NET_AdrToStringMasked(adr, sizeof(adr), bi->adr, bi->adrmask), bi->reason);
		else
			s = va("%s %s\n", s, NET_AdrToStringMasked(adr, sizeof(adr), bi->adr, bi->adrmask));
		VFS_WRITE(f, s, strlen(s));
		bi = bi->next;
	}

	VFS_CLOSE (f);
}


void SV_ForceName_f (void)
{
	client_t	*cl;
	int clnum=-1;
	int i;

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		Info_SetValueForKey(cl->userinfo, "name", Cmd_Argv(2), MAX_INFO_STRING);
		SV_LogPlayer(cl, "name forced");
		SV_ExtractFromUserinfo(cl, true);
		Q_strncpyz(cl->name, Cmd_Argv(2), sizeof(cl->namebuf));
		i = cl - svs.clients;
		MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
		MSG_WriteByte (&sv.reliable_datagram, i);
		MSG_WriteString (&sv.reliable_datagram, "name");
		MSG_WriteString (&sv.reliable_datagram, cl->name);

		return;
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_CripplePlayer_f (void)
{
	client_t	*cl;
	int clnum=-1;

	int persist = *Cmd_Argv(2);

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if (!cl->iscrippled)
		{
			SV_LogPlayer(cl, "crippled");
			if (persist && cl->rankid)
			{
				cl->iscrippled = 2;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISCRIPPLEDPERMANENTLY, cl->name);
			}
			else
			{
				cl->iscrippled = true;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISCRIPPLED, cl->name);
			}
		}
		else
		{
			SV_LogPlayer(cl, "uncrippled");
			cl->iscrippled = false;
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUARNTCRIPPLED);
		}
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_Mute_f (void)
{
	client_t	*cl;
	int clnum=-1;

	int persist = *Cmd_Argv(2);

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if (!cl->ismuted)
		{
			SV_LogPlayer(cl, "muted");
			if (persist && cl->rankid)
			{
				cl->ismuted = 2;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISMUTEDPERMANENTLY, cl->name);
			}
			else
			{
				cl->ismuted = true;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISMUTED, cl->name);
			}
		}
		else
		{
			SV_LogPlayer(cl, "unmuted");
			cl->ismuted = false;
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUARNTMUTED);
		}
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_Cuff_f (void)
{
	client_t	*cl;
	int clnum=-1;

	int persist = *Cmd_Argv(2);

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if (!cl->iscuffed)
		{
			SV_LogPlayer(cl, "cuffed");
			if (persist && cl->rankid)
			{
				cl->iscuffed = 2;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISCUFFEDPERMANENTLY, cl->name);
			}
			else
			{
				cl->iscuffed = true;
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTISCUFFED, cl->name);
			}
		}
		else
		{
			SV_LogPlayer(cl, "uncuffed");
			cl->iscuffed = false;
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUARNTCUFFED);
		}
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERDOESNTEXIST, Cmd_Argv(1));
}

void SV_Floodprot_f(void)
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

void SV_StuffToClient_f(void)
{	//with this we emulate the progs 'stuffcmds' builtin

	client_t	*cl;

	int clnum=-1;
	char *clientname = Cmd_Argv(1);
	char *str;
	char *c;
	char *key;

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

char *ShowTime(unsigned int seconds)
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
void SV_Status_f (void)
{
	int			i, j, l;
	client_t	*cl;
	float		cpu, avg, pak;
	char		*s;
	char		adr[MAX_ADR_SIZE];

	int columns = 80;
	extern cvar_t sv_listen_qw, sv_listen_nq, sv_listen_dp, sv_listen_q3;

	if (sv_redirected != RD_OBLIVION && (sv_redirected != RD_NONE
#ifndef SERVERONLY
		|| (vid.width < 68*8 && qrenderer != QR_NONE)
#endif
		))
		columns = 40;

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
	avg = 1000*svs.stats.latched_active / STATFRAMES;
	pak = (float)svs.stats.latched_packets/ STATFRAMES;

	NET_PrintAddresses(svs.sockets);

	Con_Printf("cpu utilization  : %3i%%\n",(int)cpu);
	Con_Printf("avg response time: %i ms\n",(int)avg);
	Con_Printf("packets/frame    : %5.2f\n", pak);	//not relevent as a limit.
	Con_Printf("server uptime    : %s\n", ShowTime(realtime));
	Con_Printf("map uptime       : %s\n", ShowTime(sv.world.physicstime));
	//show the current map+name (but hide name if its too long or would be ugly)
	if (columns >= 80 && *sv.mapname && strlen(sv.mapname) < 45 && !strchr(sv.mapname, '\n'))
		Con_Printf ("current map      : %s (%s)\n", sv.name, sv.mapname);
	else
		Con_Printf ("current map      : %s\n", sv.name);

	Con_Printf("entities         : %i/%i\n", sv.world.num_edicts, sv.world.max_edicts);
	Con_Printf("gamedir          : %s\n", FS_GetGamedir());
	if (sv.csqcdebug)
		Con_Printf("csqc debug       : true\n");
	if (sv.mvdrecording)
		Con_Printf("recording        : %s\n", SV_Demo_CurrentOutput());
	Con_Printf("public           : %s\n", sv_public.value?"yes":"no");
	Con_Printf("client types     :%s%s%s%s\n", sv_listen_qw.ival?" QW":"", sv_listen_nq.ival?" NQ":"", sv_listen_dp.ival?" DP":"", sv_listen_q3.ival?" Q3":"");

// min fps lat drp
	if (columns < 80)
	{
		// most remote clients are 40 columns
		//           0123456789012345678901234567890123456789
		Con_Printf ("name               userid frags\n");
        Con_Printf ("  address          rate ping drop\n");
		Con_Printf ("  ---------------- ---- ---- -----\n");
		for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (!cl->state)
				continue;

			Con_Printf ("%-16.16s  ", cl->name);

			Con_Printf ("%6i %5i", cl->userid, (int)cl->old_frags);
			if (cl->spectator)
				Con_Printf(" (s)\n");
			else
				Con_Printf("\n");

			if (cl->istobeloaded && cl->state == cs_zombie)
				s = "LoadZombie";
			else if (cl->protocol == SCP_BAD)
				s = "bot";
			else
				s = NET_BaseAdrToString (adr, sizeof(adr), cl->netchan.remote_address);
			Con_Printf ("  %-16.16s", s);
			if (cl->state == cs_connected)
			{
				Con_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie)
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
		Con_Printf ("frags userid address         name            rate ping drop  qport dl%% dls\n");
		Con_Printf ("----- ------ --------------- --------------- ---- ---- ----- ----- --- ----\n");
		for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (!cl->state)
				continue;
			Con_Printf ("%5i %6i ", (int)cl->old_frags,  cl->userid);

			if (cl->istobeloaded && cl->state == cs_zombie)
				s = "LoadZombie";
			else if (cl->protocol == SCP_BAD)
				s = "bot";
			else
				s = NET_BaseAdrToString (adr, sizeof(adr), cl->netchan.remote_address);
			Con_Printf ("%s", s);
			l = 16 - strlen(s);
			for (j=0 ; j<l ; j++)
				Con_Printf (" ");

			Con_Printf ("%s", cl->name);
			l = 16 - strlen(cl->name);
			for (j=0 ; j<l ; j++)
				Con_Printf (" ");
			if (cl->state == cs_connected)
			{
				Con_Printf ("CONNECTING           ");
			}
			else if (cl->state == cs_zombie)
			{
				Con_Printf ("ZOMBIE               ");
			}
			else
				Con_Printf ("%4i %4i %5.1f %4i"
				, (int)(1000*cl->netchan.frame_rate)
				, (int)SV_CalcPing (cl, false)
				, 100.0*cl->netchan.drop_count / cl->netchan.incoming_sequence
				, cl->netchan.qport);
			if (cl->download)
			{
				Con_Printf (" %3i %4i", (cl->downloadcount*100)/cl->downloadsize, cl->downloadsize/1024);
			}
			if (cl->spectator)
				Con_Printf(" (s)\n");
			else
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

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state == cs_free)
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

void SV_ConSayOne_f (void)
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
		Con_TPrintf(STL_USERDOESNTEXIST, Cmd_Argv(1));
}

/*
==================
SV_Heartbeat_f
==================
*/
void SV_Heartbeat_f (void)
{
	Master_ReResolve();
	svs.last_heartbeat = -9999;
}

void SV_SendServerInfoChange(char *key, const char *value)
{
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

	MSG_WriteByte (&sv.reliable_datagram, svc_serverinfo);
	MSG_WriteString (&sv.reliable_datagram, key);
	MSG_WriteString (&sv.reliable_datagram, value);
}

/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
char *CopyString(char *s);
extern char *Info_KeyForNumber(char *s, int num);
void SV_Serverinfo_f (void)
{
	cvar_t	*var;
	char value[512];
	int i;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (STL_SERVERINFOSETTINGS);
		Info_Print (svs.info);
		return;
	}

	if (Cmd_Argc() < 3)
	{
		Con_TPrintf (STL_SERVERINFOSYNTAX);
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
		Con_TPrintf (TL_STARKEYPROTECTED);
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
		var->string = CopyString (value);
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
char *CopyString(char *s);
void SV_Localinfo_f (void)
{
	char *old;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (STL_LOCALINFOSETTINGS);
		Info_Print (localinfo);
		return;
	}

	if (Cmd_Argc() != 3)
	{
		Con_TPrintf (STL_LOCALINFOSYNTAX);
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
		Con_TPrintf (TL_STARKEYPROTECTED);
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
	client_t	*cl;
	int clnum=-1;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_USERINFOSYNTAX);
		return;
	}

	while((cl = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		Info_Print (cl->userinfo);
	}

	if (clnum == -1)
		Con_TPrintf (STL_USERIDNOTONSERVER, atoi(Cmd_Argv(1)));
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
void SV_Gamedir (void)
{
	char			*dir;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (STL_CURRENTGAMEDIR, Info_ValueForKey (svs.info, "*gamedir"));
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_SVGAMEDIRUSAGE);
		return;
	}

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf (STL_GAMEDIRCANTBEPATH);
		return;
	}

	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}

/*
================
SV_Gamedir_f

Sets the gamedir and path to a different directory.
================
*/
void SV_Gamedir_f (void)
{
	char			*dir;

	if (Cmd_Argc() == 1)
	{
		Con_TPrintf (STL_CURRENTGAMEDIR, FS_GetGamedir());
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_GAMEDIRUSAGE);
		return;
	}

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf (STL_GAMEDIRCANTBEPATH);
		return;
	}

	COM_Gamedir (dir);
	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}


extern char	gamedirfile[MAX_OSPATH];
/*
================
SV_Snap
================
*/
void SV_Snap (int uid)
{
	client_t *cl;
	char		pcxname[80];
	char		checkname[MAX_OSPATH];
	int			i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == uid)
			break;
	}
	if (i >= MAX_CLIENTS)
	{
		Con_TPrintf (STL_USERDOESNTEXIST);
		return;
	}
	if (!ISQWCLIENT(cl))
	{
		Con_Printf("Can only snap QW clients\n");
		return;
	}

	sprintf(pcxname, "%d-00.pcx", uid);

	strcpy(checkname, "snap");
	Sys_mkdir(gamedirfile);
	Sys_mkdir(checkname);

	for (i=0 ; i<=99 ; i++)
	{
		pcxname[strlen(pcxname) - 6] = i/10 + '0';
		pcxname[strlen(pcxname) - 5] = i%10 + '0';
		Q_snprintfz (checkname, sizeof(checkname), "%s/snap/%s", gamedirfile, pcxname);
		if (!COM_FCheckExists(checkname))
			break;	// file doesn't exist
	}
	if (i==100)
	{
		Con_TPrintf (STL_SNAPTOOMANYFILES);
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
	Con_TPrintf (STL_SNAPREQUEST, uid);
}

/*
================
SV_Snap_f
================
*/
void SV_Snap_f (void)
{
	int			uid;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_SNAPUSAGE);
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
void SV_SnapAll_f (void)
{
	client_t *cl;
	int			i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (cl->state < cs_connected || cl->spectator)
			continue;
		SV_Snap(cl->userid);
	}
}

float mytimer;
float lasttimer;
int ticsleft;
float timerinterval;
int timerlevel;
cvar_t *timercommand;
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

void SV_SetTimer_f(void)
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

void SV_SendGameCommand_f(void)
{
#ifdef Q3SERVER
	if (SVQ3_ConsoleCommand())
		return;
#endif

#ifdef VM_Q1
	if (Q1QVM_GameConsoleCommand())
		return;
#endif

#ifdef Q2SERVER
	if (ge)
	{
		ge->ServerCommand();
	}
	else
#endif
		Con_Printf("This command requires a Q2 sever\n");
}




void PIN_LoadMessages(void);
void PIN_SaveMessages(void);
void PIN_DeleteOldestMessage(void);
void PIN_MakeMessage(char *from, char *msg);

void SV_Pin_Save_f(void)
{
	PIN_SaveMessages();
}
void SV_Pin_Reload_f(void)
{
	PIN_LoadMessages();
}
void SV_Pin_Delete_f(void)
{
	PIN_DeleteOldestMessage();
}
void SV_Pin_Add_f(void)
{
	PIN_MakeMessage(Cmd_Argv(1), Cmd_Argv(2));
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
		Cmd_AddCommand ("give", SV_Give_f);
		Cmd_AddCommand ("noclip", SV_Noclip_f);
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
	Cmd_AddCommand ("mute", SV_Mute_f);
	Cmd_AddCommand ("cuff", SV_Cuff_f);
	Cmd_AddCommand ("renameclient", SV_ForceName_f);
	Cmd_AddCommand ("cripple", SV_CripplePlayer_f);
	Cmd_AddCommand ("banname", SV_BanName_f);
	Cmd_AddCommand ("banlist", SV_BanList_f);
	Cmd_AddCommand ("banip", SV_BanIP_f);
	Cmd_AddCommand ("ban", SV_BanClientIP_f);
	Cmd_AddCommand ("unban", SV_Unban_f);
//	Cmd_AddCommand ("ban", SV_BanName_f);
	Cmd_AddCommand ("status", SV_Status_f);

	Cmd_AddCommand ("addip", SV_FilterIP_f);
	Cmd_AddCommand ("removeip", SV_Unfilter_f);
	Cmd_AddCommand ("listip", SV_FilterList_f);
	Cmd_AddCommand ("writeip", SV_WriteIP_f);

	Cmd_AddCommand ("floodprot", SV_Floodprot_f);

	Cmd_AddCommand ("sv", SV_SendGameCommand_f);
	Cmd_AddCommand ("mod", SV_SendGameCommand_f);

	Cmd_AddCommand ("killserver", SV_KillServer_f);
	Cmd_AddCommand ("map", SV_Map_f);
#ifdef Q3SERVER
	Cmd_AddCommand ("spmap", SV_Map_f);
#endif
	Cmd_AddCommand ("gamemap", SV_Map_f);
	Cmd_AddCommand ("changelevel", SV_Map_f);
	Cmd_AddCommand ("listmaps", SV_MapList_f);
	Cmd_AddCommand ("setmaster", SV_SetMaster_f);

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);

	Cmd_AddCommand ("localinfo", SV_Localinfo_f);
	Cmd_AddCommand ("gamedir", SV_Gamedir_f);
	Cmd_AddCommand ("sv_gamedir", SV_Gamedir);
	Cmd_AddCommand ("sv_settimer", SV_SetTimer_f);
	Cmd_AddCommand ("stuffcmd", SV_StuffToClient_f);

	Cmd_AddCommand ("pin_save", SV_Pin_Save_f);
	Cmd_AddCommand ("pin_reload", SV_Pin_Reload_f);
	Cmd_AddCommand ("pin_delete", SV_Pin_Delete_f);
	Cmd_AddCommand ("pin_add", SV_Pin_Add_f);

	if (isDedicated)
		cl_warncmd.value = 1;
}

#endif
