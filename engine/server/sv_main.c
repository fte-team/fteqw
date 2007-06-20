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
#include "netinc.h"
#include <sys/types.h>
#ifndef CLIENTONLY
#define Q2EDICT_NUM(i) (q2edict_t*)((char *)ge->edicts+i*ge->edict_size)

#ifdef _WIN32
#include "winquake.h"
#else
#include <netinet/in.h>
#endif

void SV_Savegame_f (void);
void SV_Loadgame_f (void);
#define INVIS_CHAR1 12
#define INVIS_CHAR2 (char)138
#define INVIS_CHAR3 (char)160

quakeparms_t host_parms;

qboolean	host_initialized;		// true if into command execution (compatability)

double		host_frametime;
double		realtime;				// without any filtering or bounding

int			host_hunklevel;

// callbacks
void SV_Masterlist_Callback(struct cvar_s *var, char *oldvalue);
void SV_Tcpport_Callback(struct cvar_s *var, char *oldvalue);
void SV_Port_Callback(struct cvar_s *var, char *oldvalue);
void SV_PortIPv6_Callback(struct cvar_s *var, char *oldvalue);
void SV_PortIPX_Callback(struct cvar_s *var, char *oldvalue);

typedef struct {
	qboolean	isdp;
	cvar_t		cv;
	netadr_t	adr;
} sv_masterlist_t;
sv_masterlist_t sv_masterlist[] = {
	{false, SCVARC("sv_master1", "", SV_Masterlist_Callback)},
	{false, SCVARC("sv_master2", "", SV_Masterlist_Callback)},
	{false, SCVARC("sv_master3", "", SV_Masterlist_Callback)},
	{false, SCVARC("sv_master4", "", SV_Masterlist_Callback)},
	{false, SCVARC("sv_master5", "", SV_Masterlist_Callback)},
	{false, SCVARC("sv_master6", "", SV_Masterlist_Callback)},
	{false, SCVARC("sv_master7", "", SV_Masterlist_Callback)},
	{false, SCVARC("sv_master8", "", SV_Masterlist_Callback)},

	{true, SCVARC("sv_masterextra1", "ghdigital.com", SV_Masterlist_Callback)}, //69.59.212.88
	{true, SCVARC("sv_masterextra2", "dpmaster.deathmask.net", SV_Masterlist_Callback)}, //209.164.24.243
	{true, SCVARC("sv_masterextra3", "12.166.196.192", SV_Masterlist_Callback)}, //blaze.mindphukd.org (doesn't resolve currently but works as an ip)
	{false, SCVAR(NULL, NULL)}
};

client_t	*host_client;			// current client

// bound the size of the physics time tic
#ifdef SERVERONLY
cvar_t	sv_mintic = SCVAR("sv_mintic","0.03");
#else
cvar_t	sv_mintic = SCVAR("sv_mintic","0");	//client builds can think as often as they want.
#endif
cvar_t	sv_maxtic = SCVAR("sv_maxtic","0.1");
cvar_t	sv_nailhack = SCVAR("sv_nailhack","0");


cvar_t	timeout = SCVAR("timeout","65");		// seconds without any message
cvar_t	zombietime = SCVAR("zombietime", "2");	// seconds to sink messages
											// after disconnect
#ifdef SERVERONLY
cvar_t	developer = SCVAR("developer","0");		// show extra messages

cvar_t	rcon_password = SCVARF("rcon_password", "", CVAR_NOUNSAFEEXPAND);	// password for remote server commands
cvar_t	password = SCVARF("password", "", CVAR_NOUNSAFEEXPAND);	// password for entering the game
#else
extern cvar_t	developer;
extern cvar_t	rcon_password;
extern cvar_t	password;
#endif
cvar_t	spectator_password = SCVARF("spectator_password", "", CVAR_NOUNSAFEEXPAND);	// password for entering as a sepctator

cvar_t	allow_download = SCVAR("allow_download", "1");
cvar_t	allow_download_skins = SCVAR("allow_download_skins", "1");
cvar_t	allow_download_models = SCVAR("allow_download_models", "1");
cvar_t	allow_download_sounds = SCVAR("allow_download_sounds", "1");
cvar_t	allow_download_demos = SCVAR("allow_download_demos", "1");
cvar_t	allow_download_maps = SCVAR("allow_download_maps", "1");
cvar_t	allow_download_anymap = SCVAR("allow_download_pakmaps", "0");
cvar_t	allow_download_pakcontents = SCVAR("allow_download_pakcontents", "1");
cvar_t	allow_download_root = SCVAR("allow_download_root", "0");
cvar_t	allow_download_textures = SCVAR("allow_download_textures", "1");
cvar_t	allow_download_pk3s = SCVAR("allow_download_pk3s", "1");
cvar_t	allow_download_wads = SCVAR("allow_download_wads", "1");
cvar_t	allow_download_configs = SCVAR("allow_download_configs", "0");

cvar_t sv_public = SCVAR("sv_public", "0");
cvar_t sv_listen_qw = FCVAR("sv_listen_qw", "sv_listen", "1", 0);
cvar_t sv_listen_nq = SCVAR("sv_listen_nq", "0");
cvar_t sv_listen_dp = SCVAR("sv_listen_dp", "1");
cvar_t sv_reportheartbeats = SCVAR("sv_reportheartbeats", "1");
cvar_t sv_highchars = SCVAR("sv_highchars", "1");
cvar_t sv_loadentfiles = SCVAR("sv_loadentfiles", "1");
cvar_t sv_maxrate = SCVAR("sv_maxrate", "10000");
cvar_t sv_maxdrate = SCVAR("sv_maxdrate", "10000");

cvar_t sv_bigcoords = SCVARF("sv_bigcoords", "", CVAR_SERVERINFO);

cvar_t sv_phs = SCVAR("sv_phs", "1");
cvar_t sv_resetparms = SCVAR("sv_resetparms", "0");

cvar_t sv_master = SCVAR("sv_master", "0");
cvar_t sv_masterport = SCVAR("sv_masterport", "0");

cvar_t	sv_voicechat = SCVAR("sv_voicechat", "0");	//still development.
cvar_t	sv_gamespeed = SCVAR("sv_gamespeed", "1");
cvar_t	sv_csqcdebug = SCVAR("sv_csqcdebug", "0");
#ifdef TCPCONNECT
cvar_t	sv_port_tcp = SCVARC("sv_port_tcp", "0", SV_Tcpport_Callback);
#endif
cvar_t  sv_port = SCVARC("sv_port", "27500", SV_Port_Callback);
#ifdef IPPROTO_IPV6
cvar_t  sv_port_ipv6 = SCVARC("sv_port_ipv6", "27500", SV_PortIPv6_Callback);
#endif
#ifdef USEIPX
cvar_t  sv_port_ipx = SCVARC("sv_port_ipx", "27500", SV_PortIPX_Callback);
#endif

cvar_t pausable	= SCVAR("pausable", "1");


//
// game rules mirrored in svs.info
//
cvar_t	fraglimit		= SCVARF("fraglimit",		"" ,	CVAR_SERVERINFO);
cvar_t	timelimit		= SCVARF("timelimit",		"" ,	CVAR_SERVERINFO);
cvar_t	teamplay		= SCVARF("teamplay",		"" ,	CVAR_SERVERINFO);
cvar_t	samelevel		= SCVARF("samelevel",		"" ,	CVAR_SERVERINFO);
cvar_t	maxclients		= SCVARF("maxclients",		"8",	CVAR_SERVERINFO);
cvar_t	maxspectators	= SCVARF("maxspectators",	"8",	CVAR_SERVERINFO);
#ifdef SERVERONLY
cvar_t	deathmatch		= SCVARF("deathmatch",		"1",	CVAR_SERVERINFO);			// 0, 1, or 2
#else
cvar_t	deathmatch		= SCVARF("deathmatch",		"0",	CVAR_SERVERINFO);			// 0, 1, or 2
#endif
cvar_t	coop			= SCVARF("coop",			"" ,	CVAR_SERVERINFO);
cvar_t	skill			= SCVARF("skill",			"" ,	CVAR_SERVERINFO);			// 0, 1, 2 or 3
cvar_t	spawn			= SCVARF("spawn",			"" ,	CVAR_SERVERINFO);
cvar_t	watervis		= SCVARF("watervis",		"" ,	CVAR_SERVERINFO);
cvar_t	rearview		= SCVARF("rearview",		"" ,	CVAR_SERVERINFO);
cvar_t	allow_luma		= SCVARF("allow_luma",		"1",	CVAR_SERVERINFO);
cvar_t	allow_bump		= SCVARF("allow_bump",		"1",	CVAR_SERVERINFO);
cvar_t	allow_skybox	= SCVARF("allow_skybox",	"",		CVAR_SERVERINFO);
cvar_t	sv_allow_splitscreen = SCVARF("allow_splitscreen","",CVAR_SERVERINFO);
cvar_t	fbskins			= SCVARF("fbskins",			"1",	CVAR_SERVERINFO);	//to get rid of lame fuhquake fbskins
cvar_t	mirrors			= SCVARF("mirrors",			"" ,	CVAR_SERVERINFO);

cvar_t	sv_motd[]		={	SCVAR("sv_motd1",		""),
							SCVAR("sv_motd2",		""),
							SCVAR("sv_motd3",		""),
							SCVAR("sv_motd4",		"")	};

cvar_t sv_compatablehulls = SCVAR("sv_compatablehulls", "1");

cvar_t	hostname = SCVARF("hostname","unnamed", CVAR_SERVERINFO);

cvar_t	secure = SCVARF("secure", "", CVAR_SERVERINFO);

extern cvar_t sv_nomsec;

char cvargroup_serverpermissions[] = "server permission variables";
char cvargroup_serverinfo[] = "serverinfo variables";
char cvargroup_serverphysics[] = "server physics variables";
char cvargroup_servercontrol[] = "server control variables";

vfsfile_t	*sv_fraglogfile;

void SV_FixupName(char *in, char *out);
void SV_AcceptClient (netadr_t adr, int userid, char *userinfo);
void Master_Shutdown (void);
void PR_SetPlayerClass(client_t *cl, int classnum, qboolean fromqc);
bannedips_t *SV_BannedAddress (netadr_t *a);

//============================================================================

qboolean ServerPaused(void)
{
	return sv.paused;
}

/*
================
SV_Shutdown

Quake calls this before calling Sys_Quit or Sys_Error
================
*/
void SV_Shutdown (void)
{
	Master_Shutdown ();
	if (sv_fraglogfile)
	{
		VFS_CLOSE (sv_fraglogfile);
		sv_fraglogfile = NULL;
	}

	PR_Deinit();

	if (sv.mvdrecording)
		SV_MVDStop_f();

	NET_Shutdown ();
#ifdef IWEB_H__
	IWebShutdown();
#endif
}

/*
================
SV_Error

Sends a datagram to all the clients informing them of the server crash,
then exits
================
*/
void VARGS SV_Error (char *error, ...)
{
	va_list		argptr;
	static	char		string[1024];
	static	qboolean inerror = false;

	if (inerror)
	{
		Sys_Error ("SV_Error: recursively entered (%s)", string);
	}

	inerror = true;

	va_start (argptr,error);
	vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	{
		extern cvar_t pr_ssqc_coreonerror;

		if (svprogfuncs && pr_ssqc_coreonerror.value)
		{
			int size = 1024*1024*8;
			char *buffer = BZ_Malloc(size);
			svprogfuncs->save_ents(svprogfuncs, buffer, &size, 3);
			COM_WriteFile("ssqccore.txt", buffer, size);
			BZ_Free(buffer);
		}
	}


	SV_EndRedirect();

	Con_Printf ("SV_Error: %s\n",string);

	if (sv.state)
		SV_FinalMessage (va("server crashed: %s\n", string));

	SV_UnspawnServer();
#ifndef SERVERONLY
	if (cls.state)
	{
		if (sv.state)
			SV_UnspawnServer();

		inerror = false;
		Host_EndGame("SV_Error: %s\n",string);
		return;
	}

	if (!isDedicated)	//dedicated servers crash...
	{
		extern jmp_buf 	host_abort;
		SCR_EndLoadingPlaque();
		inerror=false;
		longjmp (host_abort, 1);
		return;
	}
#endif

	SV_Shutdown ();

	Sys_Error ("SV_Error: %s\n",string);
}

/*
==================
SV_FinalMessage

Used by SV_Error and SV_Quit_f to send a final message to all connected
clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage (char *message)
{
	int			i;
	client_t	*cl;

	SZ_Clear (&sv.datagram);
	MSG_WriteByte (&sv.datagram, svc_print);
	MSG_WriteByte (&sv.datagram, PRINT_HIGH);
	MSG_WriteString (&sv.datagram, message);
	MSG_WriteByte (&sv.datagram, svc_disconnect);

	for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
		if (cl->state >= cs_spawned)
			if (ISNQCLIENT(cl) || ISQWCLIENT(cl))
				Netchan_Transmit (&cl->netchan, sv.datagram.cursize
						, sv.datagram.data, 10000);
}



/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing.
=====================
*/
void SV_DropClient (client_t *drop)
{
	if (drop->controller)
	{
		SV_DropClient(drop->controller);
		return;
	}
	// add the disconnect
	if (drop->state != cs_zombie)
	{
		switch (drop->protocol)
		{
		case SCP_QUAKE2:
			MSG_WriteByte (&drop->netchan.message, svcq2_disconnect);
			break;
		case SCP_QUAKEWORLD:
		case SCP_NETQUAKE:
		case SCP_DARKPLACES6:
		case SCP_DARKPLACES7:
			MSG_WriteByte (&drop->netchan.message, svc_disconnect);
			break;
		}
	}

#ifdef SVRANKING
	if (drop->state == cs_spawned)
	{
		int j;
		rankstats_t rs;
		if (drop->rankid)
		{
			if (Rank_GetPlayerStats(drop->rankid, &rs))
			{
				rs.timeonserver += realtime - drop->stats_started;
				drop->stats_started = realtime;
				rs.kills += drop->kills;
				rs.deaths += drop->deaths;

				rs.flags1 &= ~(RANK_CUFFED|RANK_MUTED|RANK_CRIPPLED);
				if (drop->iscuffed==2)
					rs.flags1 |= RANK_CUFFED;
				if (drop->ismuted==2)
					rs.flags1 |= RANK_MUTED;
				if (drop->iscrippled==2)
					rs.flags1 |= RANK_CRIPPLED;
				drop->kills=0;
				drop->deaths=0;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, drop->edict);
				if (pr_nqglobal_struct->SetChangeParms)
					PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetChangeParms);
				for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
					if (spawnparamglobals[j])
						rs.parm[j] = *spawnparamglobals[j];
				Rank_SetPlayerStats(drop->rankid, &rs);
			}
		}
	}
#endif
#ifdef SVCHAT
	SV_WipeChat(drop);
#endif
	switch(svs.gametype)
	{
#ifdef Q3SERVER
	case GT_QUAKE3:
		SVQ3_DropClient(drop);
		break;
#endif

#ifdef Q2SERVER
	case GT_QUAKE2:
		if (ge)
			ge->ClientDisconnect(drop->q2edict);
		break;
#endif

	case GT_PROGS:
		if (svprogfuncs)
		{
			if (drop->state == cs_spawned)
			{
				if (!drop->spectator)
				{
					// call the prog function for removing a client
					// this will set the body to a dead frame, among other things
					pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, drop->edict);
					PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);
				}
				else if (SpectatorDisconnect)
				{
					// call the prog function for removing a client
					// this will set the body to a dead frame, among other things
					pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, drop->edict);
					PR_ExecuteProgram (svprogfuncs, SpectatorDisconnect);
				}
			}

			if (drop->spawninfo)
				Z_Free(drop->spawninfo);
			drop->spawninfo = NULL;
		}
		break;
	}

	if (drop->spectator)
		Con_Printf ("Spectator %s removed\n",drop->name);
	else
		Con_Printf ("Client %s removed\n",drop->name);

	if (drop->download)
	{
		VFS_CLOSE (drop->download);
		drop->download = NULL;
	}
	if (drop->upload)
	{
		VFS_CLOSE (drop->upload);
		drop->upload = NULL;
	}
	*drop->uploadfn = 0;

#ifndef SERVERONLY
	if (drop->netchan.remote_address.type == NA_LOOPBACK)
	{
		Netchan_Transmit(&drop->netchan, 0, "", SV_RateForClient(drop));
		CL_Disconnect();
		drop->state = cs_free;	//don't do zombie stuff
	}
	else
#endif
	{
		drop->state = cs_zombie;		// become free in a few seconds
		drop->connection_started = realtime;	// for zombie timeout
	}
	drop->istobeloaded = false;

	drop->old_frags = 0;
#ifdef SVRANKING
	drop->kills = 0;
	drop->deaths = 0;
#endif
	if (svprogfuncs && drop->edict)
		drop->edict->v->frags = 0;
	drop->namebuf[0] = 0;
	drop->name = drop->namebuf;
	memset (drop->userinfo, 0, sizeof(drop->userinfo));
	memset (drop->userinfobasic, 0, sizeof(drop->userinfobasic));

	if (drop->frameunion.frames)	//union of the same sort of structure
	{
		Z_Free(drop->frameunion.frames);
		drop->frameunion.frames = NULL;
	}

	if (svs.gametype == GT_PROGS)	//gamecode should do it all for us.
	{
// send notification to all remaining clients
		SV_FullClientUpdate (drop, &sv.reliable_datagram, 0);
#ifdef NQPROT
		SVNQ_FullClientUpdate (drop, &sv.nqreliable_datagram);
#endif
	}

	if (drop->controlled)
	{
		drop->controlled->controller = NULL;
		SV_DropClient(drop->controlled);
	}
}


//====================================================================

typedef struct pinnedmessages_s {
	struct pinnedmessages_s *next;
	char setby[64];
	char message[1024];
} pinnedmessages_t;
pinnedmessages_t *pinned;
qboolean dopinnedload = true;
void PIN_DeleteOldestMessage(void);
void PIN_MakeMessage(char *from, char *msg);

void PIN_LoadMessages(void)
{
	char setby[64];
	char message[1024];

	int i;
	char *file;
	char *lstart;

	dopinnedload = false;

	while(pinned)
		PIN_DeleteOldestMessage();

	file = COM_LoadMallocFile("pinned.txt");
	if (!file)
		return;

	lstart = file;
	for(;;)
	{
		while (*lstart <= ' ' && *lstart)
			lstart++;

		for (i = 0; *lstart && i < sizeof(message)-1; i++)
		{
			if (*lstart == '\n' || *lstart == '\r')
				break;
			message[i] = *lstart++;
		}
		message[i] = '\0';

		while (*lstart <= ' ' && *lstart)
			lstart++;

		for (i = 0; *lstart && i < sizeof(setby)-1; i++)
		{
			if (*lstart == '\n' || *lstart == '\r')
				break;
			setby[i] = *lstart++;
		}
		setby[i] = '\0';

		if (!*setby)
			break;

		PIN_MakeMessage(setby, message);
	}

	BZ_Free(file);
}
void PIN_SaveMessages(void)
{
	pinnedmessages_t *p;
	FILE *f;

	f = COM_WriteFileOpen("pinned.txt");
	if (!f)
	{
		Con_Printf("couldn't write anything\n");
		return;
	}

	for (p = pinned; p; p = p->next)
		fprintf(f, "%s\r\n\t%s\r\n\n", p->message, p->setby);

	fclose(f);
}
void PIN_DeleteOldestMessage(void)
{
	pinnedmessages_t *old = pinned;
	if (old)
	{
		pinned = pinned->next;
		Z_Free(old);
	}
}
void PIN_MakeMessage(char *from, char *msg)
{
	pinnedmessages_t *p;
	pinnedmessages_t *newp;

	newp = BZ_Malloc(sizeof(pinnedmessages_t));
	Q_strncpyz(newp->setby, from, sizeof(newp->setby));
	Q_strncpyz(newp->message, msg, sizeof(newp->message));
	newp->next = NULL;

	if (!pinned)
		pinned = newp;
	else
	{
		for (p = pinned; ; p = p->next)
		{
			if (!p->next)
			{
				p->next = newp;
				break;
			}
		}
	}
}
void PIN_ShowMessages(client_t *cl)
{
	pinnedmessages_t *p;
	if (dopinnedload)
		PIN_LoadMessages();

	if (!pinned)
		return;

	SV_ClientPrintf(cl, PRINT_HIGH, "\n\n\n");
	for (p = pinned; p; p = p->next)
	{
		SV_ClientPrintf(cl, PRINT_HIGH, "%s\n\n        %s\n", p->message, p->setby);
		SV_ClientPrintf(cl, PRINT_HIGH, "\n\n\n");
	}

}

//====================================================================

/*
===================
SV_CalcPing

===================
*/
int SV_CalcPing (client_t *cl)
{
	float		ping;
	int			i;
	int			count;
	register	client_frame_t *frame;

	if (!cl->frameunion.frames)
		return 0;

	switch (cl->protocol)
	{
	case SCP_NETQUAKE:
	case SCP_DARKPLACES6:
	case SCP_DARKPLACES7:
	case SCP_QUAKEWORLD:
		ping = 0;
		count = 0;
		for (frame = cl->frameunion.frames, i=0 ; i<UPDATE_BACKUP ; i++, frame++)
		{
			if (frame->ping_time > 0)
			{
				ping += frame->ping_time;
				count++;
			}
		}
		if (!count)
			return 9999;
		ping /= count;

		return ping*1000;
	}
	return 0;
}

void SV_GenerateBasicUserInfo(client_t *cl)
{
	char *key, *s;
	int i;
	for (i= 1; (key = Info_KeyForNumber(cl->userinfo, i)); i++)
	{
		if (!*key)
			break;
		if (!SV_UserInfoIsBasic(key))
			continue;

		s = Info_ValueForKey(cl->userinfo, key);
		Info_SetValueForStarKey (cl->userinfobasic, key, s, sizeof(cl->userinfobasic));
	}
}

/*
===================
SV_FullClientUpdate

Writes all update values to a sizebuf
===================
*/
void SV_FullClientUpdate (client_t *client, sizebuf_t *buf, unsigned int ftepext)
{
	int		i;
	char	info[MAX_INFO_STRING];

	i = client - svs.clients;

	if (sv.demofile)
	{
		MSG_WriteByte (buf, svc_updatefrags);
		MSG_WriteByte (buf, i);
		MSG_WriteShort (buf, sv.recordedplayer[i].frags);

		MSG_WriteByte (buf, svc_updateping);
		MSG_WriteByte (buf, i);
		MSG_WriteShort (buf, sv.recordedplayer[i].ping);

		MSG_WriteByte (buf, svc_updatepl);
		MSG_WriteByte (buf, i);
		MSG_WriteByte (buf, sv.recordedplayer[i].pl);

		MSG_WriteByte (buf, svc_updateentertime);
		MSG_WriteByte (buf, i);
		MSG_WriteFloat (buf, 0);

		strcpy (info, sv.recordedplayer[i].userinfo);
		Info_RemoveKey(info, "password");		//main password key
		Info_RemoveKey(info, "*ip");		//don't broadcast this in playback
		Info_RemovePrefixedKeys (info, '_');	// server passwords, etc

		MSG_WriteByte (buf, svc_updateuserinfo);
		MSG_WriteByte (buf, i);
		MSG_WriteLong (buf, sv.recordedplayer[i].userid);
		MSG_WriteString (buf, info);
		return;
	}

//Sys_Printf("SV_FullClientUpdate:  Updated frags for client %d\n", i);

	MSG_WriteByte (buf, svc_updatefrags);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, client->old_frags);

	MSG_WriteByte (buf, svc_updateping);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, SV_CalcPing (client));

	MSG_WriteByte (buf, svc_updatepl);
	MSG_WriteByte (buf, i);
	MSG_WriteByte (buf, client->lossage);

	MSG_WriteByte (buf, svc_updateentertime);
	MSG_WriteByte (buf, i);
	MSG_WriteFloat (buf, realtime - client->connection_started);

	if (ftepext & PEXT_BIGUSERINFOS)
		strcpy (info, client->userinfo);
	else
		strcpy (info, client->userinfobasic);
	Info_RemoveKey(info, "password");		//main password key
	Info_RemovePrefixedKeys (info, '_');	// server passwords, etc

	MSG_WriteByte (buf, svc_updateuserinfo);
	MSG_WriteByte (buf, i);
	MSG_WriteLong (buf, client->userid);
	MSG_WriteString (buf, info);
}
#ifdef NQPROT
void SVNQ_FullClientUpdate (client_t *client, sizebuf_t *buf)
{
	int playercolor, top, bottom;
	int		i;

	i = client - svs.clients;

	if (i >= 16)
		return;

//Sys_Printf("SV_FullClientUpdate:  Updated frags for client %d\n", i);

	MSG_WriteByte (buf, svc_updatefrags);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, client->old_frags);

	MSG_WriteByte (buf, svc_updatename);
	MSG_WriteByte (buf, i);
	MSG_WriteString (buf, Info_ValueForKey(client->userinfo, "name"));

	top = atoi(Info_ValueForKey(client->userinfo, "topcolor"));
	bottom = atoi(Info_ValueForKey(client->userinfo, "bottomcolor"));
	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;
	playercolor = top*16 + bottom;
	MSG_WriteByte (buf, svc_updatecolors);
	MSG_WriteByte (buf, i);
	MSG_WriteByte (buf, playercolor);
}
#endif
/*
===================
SV_FullClientUpdateToClient

Writes all update values to a client's reliable stream
===================
*/
void SV_FullClientUpdateToClient (client_t *client, client_t *cl)
{
#ifdef NQPROT
	if (!ISQWCLIENT(cl))
	{
		ClientReliableCheckBlock(cl, 24 + strlen(client->userinfo));
		if (cl->num_backbuf) {
			SVNQ_FullClientUpdate (client, &cl->backbuf);
			ClientReliable_FinishWrite(cl);
		} else
			SVNQ_FullClientUpdate (client, &cl->netchan.message);
	}
	else
#endif
	{
		if (sv.demofile)
		{
			int i = client - svs.clients;
			ClientReliableCheckBlock(cl, 24 + strlen(sv.recordedplayer[i].userinfo));
		}
		else
			ClientReliableCheckBlock(cl, 24 + strlen(client->userinfo));
		if (cl->num_backbuf) {
			SV_FullClientUpdate (client, &cl->backbuf, cl->fteprotocolextensions);
			ClientReliable_FinishWrite(cl);
		} else
			SV_FullClientUpdate (client, &cl->netchan.message, cl->fteprotocolextensions);
	}
}


/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

#define STATUS_OLDSTYLE					0
#define	STATUS_SERVERINFO				1
#define	STATUS_PLAYERS					2
#define	STATUS_SPECTATORS				4
#define	STATUS_SPECTATORS_AS_PLAYERS	8 //for ASE - change only frags: show as "S"

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see
This message can be up to around 5k with worst case string lengths.
================
*/
void SVC_Status (void)
{
	int displayflags;
	int		i;
	client_t	*cl;
	char *name;
	int		ping;
	int		top, bottom;
	char frags[64];

	int slots=0;

	displayflags = atoi(Cmd_Argv(1));
	if (displayflags == STATUS_OLDSTYLE)
		displayflags = STATUS_SERVERINFO|STATUS_PLAYERS;

	Cmd_TokenizeString ("status", false, false);
	SV_BeginRedirect (RD_PACKET, LANGDEFAULT);
	if (displayflags&STATUS_SERVERINFO)
		Con_Printf ("%s\n", svs.info);
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		cl = &svs.clients[i];
		if ((cl->state == cs_connected || cl->state == cs_spawned || cl->name[0]) && ((cl->spectator && displayflags&STATUS_SPECTATORS) || (!cl->spectator && displayflags&STATUS_PLAYERS)))
		{
			top = atoi(Info_ValueForKey (cl->userinfo, "topcolor"));
			bottom = atoi(Info_ValueForKey (cl->userinfo, "bottomcolor"));
			top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
			bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
			ping = SV_CalcPing (cl);
			name = cl->name;

			if (!cl->state)	//show bots differently. Just to be courteous.
				Con_Printf ("%i %i %i %i \"BOT:%s\" \"%s\" %i %i\n", cl->userid,
					cl->old_frags, (int)(realtime - cl->connection_started)/60,
					ping, cl->name, Info_ValueForKey (cl->userinfo, "skin"), top, bottom);
			else
			{
				if (cl->spectator)
				{	//silly mvdsv stuff
					if (displayflags & STATUS_SPECTATORS_AS_PLAYERS)
					{
						frags[0] = 'S';
						frags[1] = '\0';
					}
					else
					{
						ping = -ping;
						sprintf(frags, "%i", -9999);
						name  = va("\\s\\%s", name);
					}
				}
				else
					sprintf(frags, "%i", cl->old_frags);

				Con_Printf ("%i %s %i %i \"%s\" \"%s\" %i %i\n", cl->userid,
					frags, (int)(realtime - cl->connection_started)/60,
					ping, name, Info_ValueForKey (cl->userinfo, "skin"), top, bottom);
			}
		}
		else
			slots++;
	}

//No. Not a good idea.
/*	if (slots>16)
		Con_Printf ("5016 35 54 114 \"annigilator\" \"soldier\" 0 0\n");
	if (slots>14)
		Con_Printf ("5012 32 85 162 \"FatBastard\" \"hacker\" 1 4\n");
	if (slots>12)
		Con_Printf ("5013 23 64 94 \"DeathBunny\" \"soldier\" 13 13\n");
	if (slots>10)
		Con_Printf ("5010 32 85 162 \"��\" \"hacker\" 13 13\n");
	if (slots>8)
		Con_Printf ("5011 32 85 162 \"��a���\" \"hacker\" 4 4\n");
	*/
	SV_EndRedirect ();
}

#ifdef NQPROT
void SVC_GetInfo (char *challenge)
{
	//dpmaster support

	client_t	*cl;
	int numclients = 0;
	int i;
	char *resp;

	if (!*challenge)
		challenge = NULL;

	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		cl = &svs.clients[i];
		if ((cl->state == cs_connected || cl->state == cs_spawned || cl->name[0]) && !cl->spectator)
			numclients++;
	}


	resp = va("\377\377\377\377infoResponse\x0A"
						"\\gamename\\%s"
						"\\protocol\\%i"
						"\\clients\\%d"
						"\\sv_maxclients\\%s\\mapname\\%s"
						"%s"
						"%s%s",
						com_gamename.string,
						NET_PROTOCOL_VERSION,
						numclients,
						maxclients.string, Info_ValueForKey(svs.info, "map"),
						svs.info,
						challenge ? "\\challenge\\" : "", challenge ? challenge : "");
	Info_RemoveKey(resp + 17, "maxclients");
	Info_RemoveKey(resp + 17, "map");

	NET_SendPacket (NS_SERVER, strlen(resp), resp, net_from);
}
#endif

#ifdef Q2SERVER
void SVC_InfoQ2 (void)
{
	char	string[64];
	int		i, count;
	int		version;

	if (maxclients.value == 1)
		return;		// ignore in single player

	version = atoi (Cmd_Argv(1));

	if (version != PROTOCOL_VERSION_Q2)
		snprintf (string, sizeof(string), "%s: wrong version\n", hostname.string);
	else
	{
		count = 0;
		for (i=0 ; i<maxclients.value ; i++)
			if (svs.clients[i].state >= cs_connected)
				count++;

		snprintf (string, sizeof(string), "%16s %8s %2i/%2i\n", hostname.string, sv.name, count, (int)maxclients.value);
	}

	Netchan_OutOfBandPrint (NS_SERVER, net_from, "info\n%s", string);
}
#endif

/*
===================
SV_CheckLog

===================
*/
#define	LOG_HIGHWATER	4096
#define	LOG_FLUSH		10*60
void SV_CheckLog (void)
{
	sizebuf_t	*sz;

	sz = &svs.log[svs.logsequence&1];

	// bump sequence if allmost full, or ten minutes have passed and
	// there is something still sitting there
	if (sz->cursize > LOG_HIGHWATER
	|| (realtime - svs.logtime > LOG_FLUSH && sz->cursize) )
	{
		// swap buffers and bump sequence
		svs.logtime = realtime;
		svs.logsequence++;
		sz = &svs.log[svs.logsequence&1];
		sz->cursize = 0;
		Con_Printf ("beginning fraglog sequence %i\n", svs.logsequence);
	}

}

/*
================
SVC_Log

Responds with all the logged frags for ranking programs.
If a sequence number is passed as a parameter and it is
the same as the current sequence, an A2A_NACK will be returned
instead of the data.
================
*/
void SVC_Log (void)
{
	int		seq;
	char	data[MAX_DATAGRAM+64];

	if (Cmd_Argc() == 2)
		seq = atoi(Cmd_Argv(1));
	else
		seq = -1;

	if (seq == svs.logsequence-1 || !sv_fraglogfile)
	{	// they already have this data, or we aren't logging frags
		data[0] = A2A_NACK;
		NET_SendPacket (NS_SERVER, 1, data, net_from);
		return;
	}

	Con_DPrintf ("sending log %i to %s\n", svs.logsequence-1, NET_AdrToString(net_from));

	sprintf (data, "stdlog %i\n", svs.logsequence-1);
	strcat (data, (char *)svs.log_buf[((svs.logsequence-1)&1)]);

	NET_SendPacket (NS_SERVER, strlen(data)+1, data, net_from);
}

/*
================
SVC_Ping

Just responds with an acknowledgement
================
*/
void SVC_Ping (void)
{
	char	data;

	data = A2A_ACK;

	NET_SendPacket (NS_SERVER, 1, &data, net_from);
}

//from net_from
int SV_NewChallenge (void)
{
	int		i;
	int		oldest;
	int		oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for (i = 0 ; i < MAX_CHALLENGES ; i++)
	{
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
			return svs.challenges[i].challenge;
		if (svs.challenges[i].time < oldestTime)
		{
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	// overwrite the oldest
	svs.challenges[oldest].challenge = (rand() << 16) ^ rand();
	svs.challenges[oldest].adr = net_from;
	svs.challenges[oldest].time = realtime;

	return svs.challenges[oldest].challenge;
}

/*
=================
SVC_GetChallenge

Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/
void SVC_GetChallenge (void)
{
#ifdef HUFFNETWORK
	int compressioncrc;
#endif
	int		i;
	int		oldest;
	int		oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for (i = 0 ; i < MAX_CHALLENGES ; i++)
	{
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
			break;
		if (svs.challenges[i].time < oldestTime)
		{
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES)
	{
		// overwrite the oldest
		svs.challenges[oldest].challenge = (rand() << 16) ^ rand();
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = realtime;
		i = oldest;
	}

	// send it back
	{
		char *buf;
		int lng;
		char *over;

#ifdef Q3SERVER
		if (svs.gametype == GT_QUAKE3)	//q3 servers
			buf = va("challengeResponse %i", svs.challenges[i].challenge);
		else
#endif
#ifdef Q2SERVER
			if (svs.gametype == GT_QUAKE2)	//quake 2 servers give a different challenge responce
			buf = va("challenge %i", svs.challenges[i].challenge);
		else
#endif
			buf = va("%c%i", S2C_CHALLENGE, svs.challenges[i].challenge);

		over = buf + strlen(buf) + 1;

		if (svs.gametype == GT_PROGS)
		{
#ifdef PROTOCOL_VERSION_FTE
			//tell the client what fte extensions we support
			if (svs.fteprotocolextensions)
			{
				lng = LittleLong(PROTOCOL_VERSION_FTE);
				memcpy(over, &lng, sizeof(int));
				over+=4;

				lng = LittleLong(svs.fteprotocolextensions);
				memcpy(over, &lng, sizeof(long));
				over+=4;
			}
#endif

#ifdef HUFFNETWORK
			compressioncrc = Huff_PreferedCompressionCRC();
			if (compressioncrc)
			{
				lng = LittleLong((('H'<<0) + ('U'<<8) + ('F'<<16) + ('F' << 24)));
				memcpy(over, &lng, sizeof(int));
				over+=4;

				lng = LittleLong(compressioncrc);
				memcpy(over, &lng, sizeof(long));
				over+=4;
			}
#endif
		}
		if (sv_listen_qw.value || svs.gametype != GT_PROGS)
			Netchan_OutOfBand(NS_SERVER, net_from, over-buf, buf);

		if (sv_listen_dp.value)
		{
		//dp (protocol6 upwards) can respond to this (and fte won't get confused because the challenge will be wrong)
			buf = va("challenge "DISTRIBUTION"%i", svs.challenges[i].challenge);
			Netchan_OutOfBand(NS_SERVER, net_from, strlen(buf)+1, buf);
		}
	}

//	Netchan_OutOfBandPrint (net_from, "%c%i", S2C_CHALLENGE,
//				svs.challenges[i].challenge);
}

void SV_GetNewSpawnParms(client_t *cl)
{
	int i;
	if (svprogfuncs)	//q2 dlls don't use parms in this mannor. It's all internal to the dll.
	{
		// call the progs to get default spawn parms for the new client
		if (pr_nqglobal_struct->SetNewParms)
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		{
			if (spawnparamglobals[i])
				cl->spawn_parms[i] = *spawnparamglobals[i];
			else
				cl->spawn_parms[i] = 0;
		}
	}
}

void VARGS SV_OutOfBandPrintf (int q2, netadr_t adr, char *format, ...)
{
	va_list		argptr;
	static char		string[8192];		// ??? why static?

	va_start (argptr, format);
	if (q2)
	{
		strcpy(string, "print\n");
		vsnprintf (string+6,sizeof(string)-1-6, format+1,argptr);
	}
	else
	{
		string[0] = A2C_PRINT;
		string[1] = '\n';
		vsnprintf (string+2,sizeof(string)-1-2, format,argptr);
	}
	va_end (argptr);


	Netchan_OutOfBand (NS_SERVER, adr, strlen(string), (qbyte *)string);
}
void VARGS SV_OutOfBandTPrintf (int q2, netadr_t adr, int language, translation_t text, ...)
{
	va_list		argptr;
	static char		string[8192];		// ??? why static?
	char *format = langtext(text, language);

	va_start (argptr, text);
	if (q2)
	{
		strcpy(string, "print\n");
		vsnprintf (string+6,sizeof(string)-1-6, format+1,argptr);
	}
	else
	{
		string[0] = A2C_PRINT;
		vsnprintf (string+1,sizeof(string)-1-1, format,argptr);
	}
	va_end (argptr);


	Netchan_OutOfBand (NS_SERVER, adr, strlen(string), (qbyte *)string);
}

qboolean SV_ChallengePasses(int challenge)
{
	int i;
	for (i=0 ; i<MAX_CHALLENGES ; i++)
	{	//one per ip.
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
		{
			if (challenge == svs.challenges[i].challenge)
				return true;
			return false;
		}
	}
	return false;
}

void VARGS SV_RejectMessage(int protocol, char *format, ...)
{
	va_list		argptr;
	char		string[8192];
	int len;

	va_start (argptr, format);
	switch(protocol)
	{
#ifdef NQPROT
	case SCP_NETQUAKE:
		string[4] = CCREP_REJECT;
		vsnprintf (string+5,sizeof(string)-1-5, format,argptr);
		len = strlen(string+4)+1+4;
		*(int*)string = BigLong(NETFLAG_CTL|len);
		NET_SendPacket(NS_SERVER, len, string, net_from);
		return;
	case SCP_DARKPLACES6:
	case SCP_DARKPLACES7:
		strcpy(string, "reject ");
		vsnprintf (string+7,sizeof(string)-1-7, format,argptr);
		len = strlen(string);
		break;
#endif

	case SCP_QUAKE2:
	default:
		strcpy(string, "print\n");
		vsnprintf (string+6,sizeof(string)-1-6, format,argptr);
		len = strlen(string);
		break;

	case SCP_QUAKEWORLD:
		string[0] = A2C_PRINT;
		string[1] = '\n';
		vsnprintf (string+2,sizeof(string)-1-2, format,argptr);
		len = strlen(string);
		break;
	}
	va_end (argptr);

	Netchan_OutOfBand (NS_SERVER, net_from, len, (qbyte *)string);
}

void SV_AcceptMessage(int protocol)
{
	char		string[8192];
	sizebuf_t	sb;
	int len;

	memset(&sb, 0, sizeof(sb));
	sb.maxsize = sizeof(string);
	sb.data = string;

	switch(protocol)
	{
#ifdef NQPROT
	case SCP_NETQUAKE:
		SZ_Clear(&sb);
		MSG_WriteLong(&sb, 0);
		MSG_WriteByte(&sb, CCREP_ACCEPT);
		MSG_WriteLong(&sb, ShortSwap(net_local_sv_ipadr.port));
		*(int*)sb.data = BigLong(NETFLAG_CTL|sb.cursize);
		NET_SendPacket(NS_SERVER, sb.cursize, sb.data, net_from);
		return;
	case SCP_DARKPLACES6:
	case SCP_DARKPLACES7:
		strcpy(string, "accept");
		len = strlen(string);
		break;
#endif

	case SCP_QUAKE2:
	default:
		strcpy(string, "client_connect\n");
		len = strlen(string);
		break;

	case SCP_QUAKEWORLD:
		string[0] = S2C_CONNECTION;
		string[1] = '\n';
		string[2] = '\0';
		len = strlen(string);
		break;
	}

	Netchan_OutOfBand (NS_SERVER, net_from, len, (qbyte *)string);
}

/*
==================
SVC_DirectConnect

A connection request that did not come from the master
==================
*/
int	nextuserid;
client_t *SVC_DirectConnect(void)
{
	char		userinfo[MAX_CLIENTS][2048];
	netadr_t	adr;
	int			i;
	client_t	*cl, *newcl;
	client_t	temp;
	edict_t		*ent;
#ifdef Q2SERVER
	q2edict_t	*q2ent;
#endif
	int			edictnum;
	char		*s;
	int			clients, spectators;
	qboolean	spectator;
	int			qport;
	int			version;
	int			challenge;
	int			huffcrc = 0;

	int maxpacketentities;

	int numssclients = 1;

	int protocol;

	unsigned int protextsupported=0;


	char *name;

	if (*(Cmd_Argv(0)+7) == '\\')
	{
		if (!sv_listen_dp.value)
			return NULL;
		Q_strncpyz (userinfo[0], net_message.data + 11, sizeof(userinfo[0])-1);

		if (strcmp(Info_ValueForKey(userinfo[0], "protocol"), "darkplaces 3"))
		{
			SV_RejectMessage (SCP_BAD, "Server is "DISTRIBUTION" build %i.\n", build_number());
			Con_Printf ("* rejected connect from incompatable client\n");
			return NULL;
		}
		//it's a darkplaces client.

		s = Info_ValueForKey(userinfo[0], "protocols");
		if (sizeofcoord != 4)
		{	//we allow nq with sv_listen_nq 0...
			//reason: dp is too similar for concerns about unsupported code, while the main reason why we disable nq is because of the lack of challenges
			//(and no, this isn't a way to bypass invalid challenges)
			protocol = SCP_NETQUAKE;
			Con_Printf ("* DP without sv_bigcoords 1\n");
		}
		else if (strstr(s, "DP7"))
			protocol = SCP_DARKPLACES7;
		else
			protocol = SCP_DARKPLACES6;

		s = Info_ValueForKey(userinfo[0], "challenge");
		if (!strncmp(s, DISTRIBUTION, strlen(DISTRIBUTION)))
			challenge = atoi(s+3);
		else
			challenge = atoi(s);

		s = Info_ValueForKey(userinfo[0], "name");
		if (!*s)
			Info_SetValueForKey(userinfo[0], "name", "CONNECTING", sizeof(userinfo[0]));

		qport = 0;
	}
	else
	{
		if (atoi(Cmd_Argv(0)+7))
		{
			numssclients = atoi(Cmd_Argv(0)+7);
			if (numssclients<1 || numssclients > 4)
			{
				SV_RejectMessage (SCP_BAD, "Server is "DISTRIBUTION" build %i.\n", build_number());
				Con_Printf ("* rejected connect from broken client\n");
				return NULL;
			}
		}

		version = atoi(Cmd_Argv(1));
		if (version >= 31 && version <= 34)
		{
			numssclients = 1;
			protocol = SCP_QUAKE2;
		}
		else if (version == 3)
		{
			numssclients = 1;
			protocol = SCP_NETQUAKE;
		}
		else if (version != PROTOCOL_VERSION)
		{
			SV_RejectMessage (SCP_BAD, "Server is protocol version %i, received %i\n", PROTOCOL_VERSION, version);
			Con_Printf ("* rejected connect from version %i\n", version);
			return NULL;
		}
		else
			protocol = SCP_QUAKEWORLD;

		qport = atoi(Cmd_Argv(2));

		challenge = atoi(Cmd_Argv(3));

		// note an extra qbyte is needed to replace spectator key
		for (i = 0; i < numssclients; i++)
			Q_strncpyz (userinfo[i], Cmd_Argv(4+i), sizeof(userinfo[i])-1);
	}

	if (protocol == SCP_QUAKEWORLD)	//readd?
	{
		if (!sv_listen_qw.value && net_from.type != NA_LOOPBACK)
		{
			SV_RejectMessage (SCP_BAD, "QuakeWorld protocols are not permitted on this server.\n");
			Con_Printf ("* rejected connect from quakeworld\n");
			return NULL;
		}


		while(!msg_badread)
		{
			Cmd_TokenizeString(MSG_ReadStringLine(), false, false);
			switch(Q_atoi(Cmd_Argv(0)))
			{
			case PROTOCOL_VERSION_FTE:
				protextsupported = Q_atoi(Cmd_Argv(1));
				Con_DPrintf("Client supports 0x%x fte extensions\n", protextsupported);
				break;
			case PROTOCOL_VERSION_HUFFMAN:
				huffcrc = Q_atoi(Cmd_Argv(1));
				Con_DPrintf("Client supports huffman compression\n", huffcrc);
				break;
			}
		}
		msg_badread=false;
	}

	if (protextsupported & PEXT_256PACKETENTITIES)
		maxpacketentities = MAX_EXTENDED_PACKET_ENTITIES;
	else
		maxpacketentities = MAX_STANDARD_PACKET_ENTITIES;

	if (!sv_allow_splitscreen.value)
		numssclients = 1;

	if (!(protextsupported & PEXT_SPLITSCREEN))
		numssclients = 1;

	if (sv.msgfromdemo || net_from.type == NA_LOOPBACK)	//normal rules don't apply
		i=0;
	else
	{
	// see if the challenge is valid
		if (!SV_ChallengePasses(challenge))
		{
			SV_RejectMessage (protocol, "Bad challenge.\n");
			return NULL;
		}
	}

	// check for password or spectator_password
	if (svprogfuncs)
	{
		s = Info_ValueForKey (userinfo[0], "spectator");
		if (s[0] && strcmp(s, "0"))
		{
			if (spectator_password.string[0] &&
				stricmp(spectator_password.string, "none") &&
				strcmp(spectator_password.string, s) )
			{	// failed
				Con_Printf ("%s:spectator password failed\n", NET_AdrToString (net_from));
				SV_RejectMessage (protocol, "requires a spectator password\n\n");
				return NULL;
			}
			Info_RemoveKey (userinfo[0], "spectator"); // remove key
			Info_SetValueForStarKey (userinfo[0], "*spectator", "1", sizeof(userinfo[0]));
			spectator = true;
		}
		else
		{
			s = Info_ValueForKey (userinfo[0], "password");
			if (password.string[0] &&
				stricmp(password.string, "none") &&
				strcmp(password.string, s) )
			{
				Con_Printf ("%s:password failed\n", NET_AdrToString (net_from));
				SV_RejectMessage (protocol, "server requires a password\n\n");
				return NULL;
			}
			spectator = false;
			Info_RemoveKey (userinfo[0], "password"); // remove passwd
			Info_RemoveKey (userinfo[0], "*spectator"); // remove key
		}
	}
	else
		spectator = false;//q2 does all of it's checks internally, and deals with spectator ship too

	adr = net_from;
	nextuserid++;	// so every client gets a unique id

	newcl = &temp;
	memset (newcl, 0, sizeof(client_t));

	newcl->userid = nextuserid;
	newcl->fteprotocolextensions = protextsupported;
	newcl->protocol = protocol;

	if (sv.msgfromdemo)
		newcl->wasrecorded = true;

	// works properly
	if (!sv_highchars.value)
	{
		qbyte *p, *q;

		for (p = (qbyte *)newcl->userinfo, q = (qbyte *)userinfo;
			*q && p < (qbyte *)newcl->userinfo + sizeof(newcl->userinfo)-1; q++)
			if (*q > 31 && *q <= 127)
				*p++ = *q;
	}
	else
		Q_strncpyS (newcl->userinfo, userinfo[0], sizeof(newcl->userinfo)-1);
	newcl->userinfo[sizeof(newcl->userinfo)-1] = '\0';

	// if there is already a slot for this ip, drop it
	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
		if (cl->state == cs_free)
			continue;
		if (NET_CompareBaseAdr (adr, cl->netchan.remote_address)
			&& ( cl->netchan.qport == qport
			|| adr.port == cl->netchan.remote_address.port ))
		{
			if (cl->state == cs_connected)
			{
				if (cl->protocol != protocol)
					Con_Printf("%s: diff prot connect\n", NET_AdrToString (adr));
				else
					Con_Printf("%s:dup connect\n", NET_AdrToString (adr));
				nextuserid--;
				return NULL;
			}
			{
				Con_Printf ("%s:reconnect\n", NET_AdrToString (adr));
//				SV_DropClient (cl);
			}
			break;
		}
	}

	name = Info_ValueForKey (temp.userinfo, "name");

	if (protocol == SCP_QUAKEWORLD &&!atoi(Info_ValueForKey (temp.userinfo, "iknow")))
	{
		if (sv.worldmodel->fromgame == fg_halflife && !(newcl->fteprotocolextensions & PEXT_HLBSP))
		{
			if (atof(Info_ValueForKey (temp.userinfo, "*FuhQuake")) < 0.3)
			{
				SV_RejectMessage (protocol, "The server is using a halflife level and we don't think your client supports this\nuse 'setinfo iknow 1' to ignore this check\nYou can go to "ENGINEWEBSITE" to get a compatable client\n\nYou may need to enable an option\n\n");
//				Con_Printf("player %s was dropped due to incompatable client\n", name);
//				return;
			}
		}
#ifdef PEXT_Q2BSP
		else if (sv.worldmodel->fromgame == fg_quake2 && !(newcl->fteprotocolextensions & PEXT_Q2BSP))
		{
			SV_RejectMessage (protocol, "The server is using a quake 2 level and we don't think your client supports this\nuse 'setinfo iknow 1' to ignore this check\nYou can go to "ENGINEWEBSITE" to get a compatable client\n\nYou may need to enable an option\n\n");
//			Con_Printf("player %s was dropped due to incompatable client\n", name);
//			return;
		}
#endif
#ifdef PEXT_Q3BSP
		else if (sv.worldmodel->fromgame == fg_quake3 && !(newcl->fteprotocolextensions & PEXT_Q3BSP))
		{
			SV_RejectMessage (protocol, "The server is using a quake 3 level and we don't think your client supports this\nuse 'setinfo iknow 1' to ignore this check\nYou can go to "ENGINEWEBSITE" to get a compatable client\n\nYou may need to enable an option\n\n");
//			Con_Printf("player %s was dropped due to incompatable client\n", name);
//			return;
		}
#endif
	}

	SV_FixupName(name, name);

	if (!*name)
	{
		name = "Hidden";
	}
	else if (!stricmp(name, "console"))
		name = "Not Console";	//have fun dudes.

	// count up the clients and spectators
	clients = 0;
	spectators = 0;
	newcl = NULL;
	if (!sv.allocated_client_slots)
		Con_Printf("Apparently, there are no client slots allocated. This shouldn't be happening\n");
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		if (cl->state == cs_free)
			continue;
		if (cl->spectator)
			spectators++;
		else
			clients++;

		if (cl->istobeloaded && cl->state == cs_zombie)
		{
			if (!newcl)
			{
				if (!strcmp(cl->name, name) || !*cl->name)	//named, or first come first serve.
				{
					newcl = cl;
					temp.istobeloaded = cl->istobeloaded;
					break;
				}
			}
		}
	}

	if (!newcl)	//client has no slot. It's possible to bipass this if server is loading a game. (or a duplicated qsocket)
	{
		// if at server limits, refuse connection
		if ( maxclients.value > MAX_CLIENTS )
			Cvar_SetValue (&maxclients, MAX_CLIENTS);
		if (maxspectators.value > MAX_CLIENTS)
			Cvar_SetValue (&maxspectators, MAX_CLIENTS);
//		if (maxspectators.value + maxclients.value > MAX_CLIENTS)	//maybe a server wishes to allow this sort of thing?
//			Cvar_SetValue ("maxspectators", MAX_CLIENTS - maxspectators.value + maxclients.value);
		if (svprogfuncs && ((spectator && spectators >= (int)maxspectators.value)
			|| (!spectator && clients >= (int)maxclients.value)
			|| (clients + spectators >= sv.allocated_client_slots) ))
		{
			Con_Printf ("%s:full connect\n", NET_AdrToString (adr));
			SV_RejectMessage (protocol, "\nserver is full\n\n");
			return NULL;
		}

	// find a client slot
		for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
		{
			if (cl->state == cs_free)
			{
				newcl = cl;
				break;
			}
		}
		if (!newcl)
		{
			if (svprogfuncs)
				Con_Printf ("WARNING: miscounted available clients\n");
			else
			{
				Con_Printf ("%s:full connect\n", NET_AdrToString (adr));
				SV_RejectMessage (protocol, "server is full\n\n");
			}
			return NULL;
		}
	}

	{
		bannedips_t *banip = SV_BannedAddress(&adr);
		if (banip)
		{
			if (banip->reason[0])
				SV_RejectMessage (protocol, "You were banned.\nReason: %s\n", banip->reason);
			else
				SV_RejectMessage (protocol, "You were banned.\n");
			return NULL;
		}
	}

	edictnum = (newcl-svs.clients)+1;
	switch (svs.gametype)
	{
	case GT_PROGS:
		ent = EDICT_NUM(svprogfuncs, edictnum);
#ifdef Q2SERVER
		temp.q2edict = NULL;
#endif
		temp.edict = ent;






		// build a new connection
		// accept the new client
		// this is the only place a client_t is ever initialized
		temp.frameunion.frames = newcl->frameunion.frames;	//don't touch these.
		if (temp.frameunion.frames)
			Z_Free(temp.frameunion.frames);

		temp.frameunion.frames = Z_Malloc((sizeof(client_frame_t)+sizeof(entity_state_t)*maxpacketentities)*UPDATE_BACKUP);
		for (i = 0; i < UPDATE_BACKUP; i++)
		{
			temp.frameunion.frames[i].entities.max_entities = maxpacketentities;
			temp.frameunion.frames[i].entities.entities = (entity_state_t*)(temp.frameunion.frames+UPDATE_BACKUP) + i*temp.frameunion.frames[i].entities.max_entities;
		}
		break;

#ifdef Q2SERVER
	case GT_QUAKE2:
		q2ent = Q2EDICT_NUM(edictnum);
		temp.edict = NULL;
		temp.q2edict = q2ent;

		if (!ge->ClientConnect(q2ent, temp.userinfo))
			return NULL;

		ge->ClientUserinfoChanged(q2ent, temp.userinfo);

		// build a new connection
		// accept the new client
		// this is the only place a client_t is ever initialized
		temp.frameunion.q2frames = newcl->frameunion.q2frames;	//don't touch these.
		if (temp.frameunion.q2frames)
			Z_Free(temp.frameunion.q2frames);

		temp.frameunion.q2frames = Z_Malloc(sizeof(q2client_frame_t)*Q2UPDATE_BACKUP);
		break;
#endif
	default:
		Sys_Error("Bad svs.gametype in SVC_DirectConnect");
		break;
	}


	{
		char *n, *t;
		n = newcl->name;
		t = newcl->team;
		*newcl = temp;
		newcl->name = n;
		newcl->team = t;
	}

	newcl->zquake_extensions = atoi(Info_ValueForKey(newcl->userinfo, "*z_ext"));
	if (*Info_ValueForKey(newcl->userinfo, "*fuhquake"))	//fuhquake doesn't claim to support z_ext but does look at our z_ext serverinfo key.
	{														//so switch on the bits that it should be sending.
		newcl->zquake_extensions |= Z_EXT_PM_TYPE|Z_EXT_PM_TYPE_NEW;
	}

	//dmw - delayed - Netchan_OutOfBandPrint (adr, "%c", S2C_CONNECTION );

	Netchan_Setup (NS_SERVER, &newcl->netchan , adr, qport);

	if (huffcrc)
		newcl->netchan.compress = true;
	else
		newcl->netchan.compress = false;

	newcl->protocol = protocol;
#ifdef NQPROT
	newcl->netchan.isnqprotocol = ISNQCLIENT(newcl);
#endif

	newcl->state = cs_connected;

	newcl->datagram.allowoverflow = true;
	newcl->datagram.data = newcl->datagram_buf;
	newcl->datagram.maxsize = sizeof(newcl->datagram_buf);

	// spectator mode can ONLY be set at join time
	newcl->spectator = spectator;

	// parse some info from the info strings
	SV_ExtractFromUserinfo (newcl);
	SV_GenerateBasicUserInfo (newcl);

	// JACK: Init the floodprot stuff.
	newcl->floodprotmessage = 0.0;
	newcl->lastspoke = 0.0;
	newcl->lockedtill = 0;

#ifdef SVRANKING
	if (svs.demorecording || (svs.demoplayback && newcl->wasrecorded))	//disable rankings. Could cock things up.
	{
		SV_GetNewSpawnParms(newcl);
	}
	else
	{
//rankid is figured out in extract from user info
		if (!newcl->rankid)	//failed to get a userid
		{
			if (rank_needlogin.value)
			{
				SV_RejectMessage (protocol, "Bad password/username\nThis server requires logins. Please see the serverinfo for website and info on how to register.\n");
				newcl->state = cs_free;
				return NULL;
			}

//			SV_OutOfBandPrintf (isquake2client, adr, "\nWARNING: You have not got a place on the ranking system, probably because a user with the same name has already connected and your pwds differ.\n\n");

			SV_GetNewSpawnParms(newcl);
		}
		else
		{
			rankstats_t rs;
			if (!Rank_GetPlayerStats(newcl->rankid, &rs))
			{
				SV_RejectMessage (protocol, "Rankings/Account system failed\n");
				Con_Printf("banned player %s is trying to connect\n", newcl->name);
				newcl->name[0] = 0;
				memset (newcl->userinfo, 0, sizeof(newcl->userinfo));
				newcl->state = cs_free;
				return NULL;
			}

			if (rs.flags1 & RANK_MUTED)
			{
				SV_BroadcastTPrintf(PRINT_MEDIUM, STL_CLIENTISSTILLMUTED, newcl->name);
			}
			if (rs.flags1 & RANK_CUFFED)
			{
				SV_BroadcastTPrintf(PRINT_LOW, STL_CLIENTISSTILLCUFFED, newcl->name);
			}
			if (rs.flags1 & RANK_CRIPPLED)
			{
				SV_BroadcastTPrintf(PRINT_HIGH, STL_CLIENTISSTILLCRIPPLED, newcl->name);
			}

			if (rs.timeonserver)
			{
				if (cl->istobeloaded)
				{	//do nothing.
				}
				else if (sv_resetparms.value)
				{
					SV_GetNewSpawnParms(newcl);
				}
				else
				{
					for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
						newcl->spawn_parms[i] = rs.parm[i];
				}

				if (rs.timeonserver > 3*60)	//woo. Ages.
					s = va(langtext(STL_BIGGREETING, newcl->language), newcl->name, (int)(rs.timeonserver/(60*60)), (int)((int)(rs.timeonserver/60)%(60)));
				else	//measure this guy in minuites.
					s = va(langtext(STL_SHORTGREETING, newcl->language), newcl->name, (int)(rs.timeonserver/60));

				SV_OutOfBandPrintf (protocol == SCP_QUAKE2, adr, s);
			}
			else if (!cl->istobeloaded)
			{
				SV_GetNewSpawnParms(newcl);

				SV_OutOfBandTPrintf (protocol == SCP_QUAKE2, adr, newcl->language, STL_FIRSTGREETING, newcl->name, (int)rs.timeonserver);
			}
			//else loaded players already have thier initial parms set
		}
	}
#else
	// call the progs to get default spawn parms for the new client
	if (!cl->istobeloaded)
	{
		SV_GetNewSpawnParms(newcl);
	}
#endif


	if (!newcl->wasrecorded)
	{
		SV_AcceptMessage (protocol);

		if (newcl->spectator)
		{
			SV_BroadcastTPrintf(PRINT_LOW, STL_SPECTATORCONNECTED, newcl->name);
//			Con_Printf ("Spectator %s connected\n", newcl->name);
		}
		else
		{
			SV_BroadcastTPrintf(PRINT_LOW, STL_CLIENTCONNECTED, newcl->name);
//			Con_DPrintf ("Client %s connected\n", newcl->name);
		}
	}
	else
	{
		if (newcl->spectator)
		{
			SV_BroadcastTPrintf(PRINT_LOW, STL_RECORDEDSPECTATORCONNECTED, newcl->name);
//			Con_Printf ("Recorded spectator %s connected\n", newcl->name);
		}
		else
		{
			SV_BroadcastTPrintf(PRINT_LOW, STL_RECORDEDCLIENTCONNECTED, newcl->name);
//			Con_DPrintf ("Recorded client %s connected\n", newcl->name);
		}
	}
	newcl->sendinfo = true;

	for (i = 0; i < sizeof(sv_motd)/sizeof(sv_motd[0]); i++)
	{
		if (*sv_motd[i].string)
			SV_ClientPrintf(newcl, PRINT_CHAT, "%s\n", sv_motd[i].string);
	}

	newcl->fteprotocolextensions &= ~PEXT_SPLITSCREEN;
	for (clients = 1; clients < numssclients; clients++)
	{
		for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
		{
			if (cl->state == cs_free)
			{
				break;
			}
		}
		if (i == sv.allocated_client_slots)
			break;

		newcl->fteprotocolextensions |= PEXT_SPLITSCREEN;

		temp.frameunion.frames = cl->frameunion.frames;	//don't touch these.
		temp.edict = cl->edict;
		memcpy(cl, newcl, sizeof(client_t));
		cl->frameunion.frames = temp.frameunion.frames;
		cl->edict = temp.edict;

		cl->fteprotocolextensions |= PEXT_SPLITSCREEN;

		if (newcl->controller)
		{
			newcl->controller->controlled = cl;
			newcl->controller = cl;
		}
		else
		{
			newcl->controlled = cl;
			newcl->controller = cl;
		}
		cl->controller = newcl;
		cl->controlled = NULL;

		Q_strncpyS (cl->userinfo, userinfo[clients], sizeof(cl->userinfo)-1);
		cl->userinfo[sizeof(cl->userinfo)-1] = '\0';

		if (spectator)
		{
			Info_RemoveKey (cl->userinfo, "spectator");
			Info_SetValueForStarKey (cl->userinfo, "*spectator", "1", sizeof(cl->userinfo));
		}
		else
			Info_RemoveKey (cl->userinfo, "*spectator");

		SV_ExtractFromUserinfo (cl);

		SV_GetNewSpawnParms(cl);
	}
	newcl->controller = NULL;


	Sys_ServerActivity();

	PIN_ShowMessages(newcl);

	if (ISNQCLIENT(newcl))
	{
		newcl->netchan.message.maxsize = sizeof(newcl->netchan.message_buf);
		host_client = newcl;
		SVNQ_New_f();
	}



	return newcl;
}


int Rcon_Validate (void)
{
	if (!strlen (rcon_password.string))
		return 0;

	if (strcmp (Cmd_Argv(1), rcon_password.string) )
		return 0;

	return 1;
}

/*
===============
SVC_RemoteCommand

A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
===============
*/
void SVC_RemoteCommand (void)
{
	int		i;
	char	remaining[1024];

	{
		bannedips_t *banip = SV_BannedAddress(&net_from);
		if (banip)
		{
			Con_Printf ("Rcon from banned ip %s\n", NET_AdrToString (net_from));
			return;
		}
	}

	if (!Rcon_Validate ())
	{
#ifdef SVRANKING
		if (cmd_allowaccess.value)	//try and find a username, match the numeric password
		{
			int rid;
			char *s = Cmd_Argv(1);
			char *colon=NULL, *c2;
			rankstats_t stats;
			c2=s;
			for(;;)
			{
				c2 = strchr(c2, ':');
				if (!c2)
					break;
				colon = c2;
				c2++;
			}
			if (colon)	//oh, could this be a specific username?
			{
				*colon = '\0';
				colon++;
				rid = Rank_GetPlayerID(s, atoi(colon), false, true);
				if (rid)
				{
					if (!Rank_GetPlayerStats(rid, &stats))
						return;


					Con_Printf ("Rcon from %s:\n%s\n"
						, NET_AdrToString (net_from), net_message.data+4);

					SV_BeginRedirect (RD_PACKET, LANGDEFAULT);

					remaining[0] = 0;

					for (i=2 ; i<Cmd_Argc() ; i++)
					{
						if (strlen(remaining)+strlen(Cmd_Argv(i))>=sizeof(remaining)-2)
						{
							Con_Printf("Rcon was too long\n");
							SV_EndRedirect ();
							Con_Printf ("Rcon from %s:\n%s\n"
								, NET_AdrToString (net_from), "Was too long - possible buffer overflow attempt");
							return;
						}
						strcat (remaining, Cmd_Argv(i) );
						strcat (remaining, " ");
					}

					Cmd_ExecuteString (remaining, stats.trustlevel);

					SV_EndRedirect ();
					return;
				}
			}
		}
#endif

		Con_Printf ("Bad rcon from %s:\n%s\n"
			, NET_AdrToString (net_from), net_message.data+4);

		SV_BeginRedirect (RD_PACKET, LANGDEFAULT);

		Con_Printf ("Bad rcon_password.\n");

	}
	else
	{

		Con_Printf ("Rcon from %s:\n%s\n"
			, NET_AdrToString (net_from), net_message.data+4);

		SV_BeginRedirect (RD_PACKET, LANGDEFAULT);

		remaining[0] = 0;

		for (i=2 ; i<Cmd_Argc() ; i++)
		{
			if (strlen(remaining)+strlen(Cmd_Argv(i))>=sizeof(remaining)-2)
			{
				Con_Printf("Rcon was too long\n");
				SV_EndRedirect ();
				Con_Printf ("Rcon from %s:\n%s\n"
					, NET_AdrToString (net_from), "Was too long - possible buffer overflow attempt");
				return;
			}
			strcat (remaining, Cmd_Argv(i) );
			strcat (remaining, " ");
		}

		Cmd_ExecuteString (remaining, rcon_level.value);

	}

	SV_EndRedirect ();
}

void SVC_RealIP (void)
{
	unsigned int slotnum;
	int cookie;
	slotnum = atoi(Cmd_Argv(1));
	cookie = atoi(Cmd_Argv(2));

	if (slotnum >= MAX_CLIENTS)
	{
		//a malitious user
		return;
	}

	if (cookie != svs.clients[slotnum].realip_num)
	{
		//could be someone trying to kick someone else
		//so we can't kick, as much as we might like to.
		return;
	}

	if (svs.clients[slotnum].realip_status)
		return;


	svs.clients[slotnum].realip_status = 1;
	svs.clients[slotnum].realip = net_from;
}

void SVC_ACK (void)
{
	int slotnum;
	for (slotnum = 0; slotnum < MAX_CLIENTS; slotnum++)
	{
		if (svs.clients[slotnum].state)
		{
			if (svs.clients[slotnum].realip_status == 1 && NET_CompareAdr(svs.clients[slotnum].realip, net_from))
			{
				if (!*Cmd_Argv(1))
					svs.clients[slotnum].realip_status = 2;
				else if (atoi(Cmd_Argv(1)) == svs.clients[slotnum].realip_ping &&
						atoi(Cmd_Argv(2)) == svs.clients[slotnum].realip_num)
				{
					svs.clients[slotnum].realip_status = 3;
				}
				else
				{
					Netchan_OutOfBandPrint(NS_SERVER, net_from, "realip not accepted. Please stop hacking.\n");
				}
				return;
			}
		}
	}
	Con_Printf ("A2A_ACK from %s\n", NET_AdrToString (net_from));
}

/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
qboolean SV_ConnectionlessPacket (void)
{
	char	*s;
	char	*c;

	MSG_BeginReading ();

	if (net_message.cursize >= MAX_QWMSGLEN)	//add a null term in message space
	{
		Con_Printf("Oversized packet from %s\n", NET_AdrToString (net_from));
		net_message.cursize=MAX_QWMSGLEN-1;
	}
	net_message.data[net_message.cursize] = '\0';	//terminate it properly. Just in case.

	MSG_ReadLong ();		// skip the -1 marker

	s = MSG_ReadStringLine ();

	Cmd_TokenizeString (s, false, false);

	c = Cmd_Argv(0);

	if (!strcmp(c, "ping") || ( c[0] == A2A_PING && (c[1] == 0 || c[1] == '\n')) )
		SVC_Ping ();
	else if (c[0] == A2A_ACK && (c[1] == 0 || c[1] == '\n') )
		SVC_ACK ();
	else if (!strcmp(c,"status"))
		SVC_Status ();
	else if (!strcmp(c,"log"))
		SVC_Log ();
#ifdef Q2SERVER
	else if (!strcmp(c, "info"))
		SVC_InfoQ2 ();
#endif
	else if (!strncmp(c,"connect", 7))
	{
#ifdef Q3SERVER
		if (svs.gametype == GT_QUAKE3)
		{
			SVQ3_DirectConnect();
			return true;
		}
		else
#endif
			if (secure.value)	//FIXME: possible problem for nq clients when enabled
			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nThis server requires client validation.\nPlease use the "DISTRIBUTION" validation program\n", A2C_PRINT);
		else
		{
			SVC_DirectConnect ();
			return true;
		}
	}
	else if (!strcmp(c,"\xad\xad\xad\xad""getchallenge"))
	{
		SVC_GetChallenge ();
	}
	else if (!strcmp(c,"getchallenge"))
	{
		SVC_GetChallenge ();
	}
#ifdef NQPROT
	else if (!strcmp(c, "getinfo"))
		SVC_GetInfo(Cmd_Args());
#endif
	else if (!strcmp(c, "rcon"))
		SVC_RemoteCommand ();
	else if (!strcmp(c, "realip"))
		SVC_RealIP ();
	else if (!PR_GameCodePacket(net_message.data+4))
		Con_Printf ("bad connectionless packet from %s:\n%s\n"
		, NET_AdrToString (net_from), s);

	return false;
}

#ifdef NQPROT
void SVNQ_ConnectionlessPacket(void)
{
	sizebuf_t sb;
	int header;
	int length;
	char *str;
	char buffer[256];
	if (net_from.type == NA_LOOPBACK)
		return;

	if (!sv_listen_nq.value)
		return;
	if (sv_bigcoords.value)
		return;	//no, start using dp7 instead.

	MSG_BeginReading();
	header = LongSwap(MSG_ReadLong());
	if (!(header & NETFLAG_CTL))
		return;	//no idea what it is.

	length = header & NETFLAG_LENGTH_MASK;
	if (length != net_message.cursize)
		return;	//corrupt or not ours

	switch(MSG_ReadByte())
	{
	case CCREQ_CONNECT:
		sb.maxsize = sizeof(buffer);
		sb.data = buffer;
		if (strcmp(MSG_ReadString(), NET_GAMENAME_NQ))
		{
			SZ_Clear(&sb);
			MSG_WriteLong(&sb, 0);
			MSG_WriteByte(&sb, CCREP_REJECT);
			MSG_WriteString(&sb, "Incorrect game\n");
			*(int*)sb.data = BigLong(NETFLAG_CTL+sb.cursize);
			NET_SendPacket(NS_SERVER, sb.cursize, sb.data, net_from);
			return;	//not our game.
		}
		if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		{
			SZ_Clear(&sb);
			MSG_WriteLong(&sb, 0);
			MSG_WriteByte(&sb, CCREP_REJECT);
			MSG_WriteString(&sb, "Incorrect version\n");
			*(int*)sb.data = BigLong(NETFLAG_CTL+sb.cursize);
			NET_SendPacket(NS_SERVER, sb.cursize, sb.data, net_from);
			return;	//not our version...
		}
		str = va("connect %i %i %i \"\\name\\unconnected\"", NET_PROTOCOL_VERSION, 0, SV_NewChallenge());
		Cmd_TokenizeString (str, false, false);

		SVC_DirectConnect();
		break;
	}
}
#endif

/*
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


==============================================================================
*/

cvar_t	filterban = SCVAR("filterban", "1");

/*
=================
SV_FilterPacket
=================
*/
qboolean SV_FilterPacket (netadr_t *a)
{
	filteredips_t *banip;

	if (NET_IsLoopBackAddress(*a))
		return 0; // never filter loopback

	for (banip = svs.filteredips; banip; banip=banip->next)
	{
		if (NET_CompareAdrMasked(*a, banip->adr, banip->adrmask))
			return filterban.value;
	}
	return !filterban.value;
}

// SV_BannedAdress, run through ban address list and return corresponding bannedips_t
// pointer, otherwise return NULL if not in the list
bannedips_t *SV_BannedAddress (netadr_t *a)
{
	bannedips_t *banip;
	for (banip = svs.bannedips; banip; banip=banip->next)
	{
		if (NET_CompareAdrMasked(*a, banip->adr, banip->adrmask))
			return banip;
	}
	return NULL;
}

//send a network packet to a new non-connected client.
//this is to combat tunneling
void SV_OpenRoute_f(void)
{
	netadr_t to;
	char data[64];

	NET_StringToAdr(Cmd_Argv(1), &to);
	if (!to.port)
		to.port = PORT_CLIENT;

	sprintf(data, "\xff\xff\xff\xff%c", S2C_CONNECTION);

	Netchan_OutOfBandPrint(NS_SERVER, to, "hello");
//	NET_SendPacket (strlen(data)+1, data, to);
}

//============================================================================

/*
=================
SV_ReadPackets
=================
*/
//FIMXE: move to header
qboolean SV_GetPacket (void);
void SV_ReadPackets (void)
{
	int			i;
	client_t	*cl;
	qboolean	good;
	int			qport;

	good = false;
	while (SV_GetPacket ())
	{
		if (SV_FilterPacket (&net_from))
			continue;

		// check for connectionless packet (0xffffffff) first
		if (*(int *)net_message.data == -1)
		{
			SV_ConnectionlessPacket();
			continue;
		}

#ifdef Q3SERVER
		if (svs.gametype == GT_QUAKE3)
		{
			SVQ3_HandleClient();
			continue;
		}
#endif

		// read the qport out of the message so we can fix up
		// stupid address translating routers
		MSG_BeginReading ();
		MSG_ReadLong ();		// sequence number
		MSG_ReadLong ();		// sequence number
		qport = MSG_ReadShort () & 0xffff;

		// check for packets from connected clients
		for (i=0, cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (cl->state == cs_free)
				continue;
			if (!NET_CompareBaseAdr (net_from, cl->netchan.remote_address))
				continue;
#ifdef NQPROT
			if (ISNQCLIENT(cl) && cl->netchan.remote_address.port == net_from.port)
			{
				if (cl->state != cs_zombie)
				{
					if (NQNetChan_Process(&cl->netchan))
					{
						svs.stats.packets++;
						SVNQ_ExecuteClientMessage(cl);
					}
				}
				break;
			}
#endif
			if (cl->netchan.qport != qport)
				continue;
			if (cl->netchan.remote_address.port != net_from.port)
			{
				Con_DPrintf ("SV_ReadPackets: fixing up a translated port\n");
				cl->netchan.remote_address.port = net_from.port;
			}
			if (Netchan_Process(&cl->netchan))
			{	// this is a valid, sequenced packet, so process it
				svs.stats.packets++;
				good = true;
				if (cl->state != cs_zombie)
				{
					cl->send_message = true;	// reply at end of frame

#ifdef Q2SERVER
					if (cl->protocol == SCP_QUAKE2)
						SVQ2_ExecuteClientMessage(cl);
					else
#endif
						SV_ExecuteClientMessage (cl);
				}
			}
			break;
		}

		if (i != MAX_CLIENTS)
			continue;

#ifdef NQPROT
		SVNQ_ConnectionlessPacket();
#endif

		// packet is not from a known client
		//	Con_Printf ("%s:sequenced packet without connection\n"
		// ,NET_AdrToString(net_from));
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client in timeout.value
seconds, drop the conneciton.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
void SV_CheckTimeouts (void)
{
	int		i;
	client_t	*cl;
	float	droptime;
	int	nclients;

	droptime = realtime - timeout.value;
	nclients = 0;

	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
		if (cl->state == cs_connected || cl->state == cs_spawned) {
			if (!cl->spectator)
				nclients++;
			if (cl->netchan.last_received < droptime && cl->netchan.remote_address.type != NA_LOOPBACK && cl->protocol != SCP_BAD) {
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTTIMEDOUT, cl->name);
				SV_DropClient (cl);
				cl->state = cs_free;	// don't bother with zombie state
			}
		}
		if (cl->state == cs_zombie &&
			realtime - cl->connection_started > zombietime.value)
		{
			if (cl->connection_started == -1)
			{
				continue;
			}
			cl->state = cs_free;	// can now be reused
			if (cl->istobeloaded)
			{
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, cl->edict);
				PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);

				host_client->istobeloaded=false;

				SV_BroadcastTPrintf (PRINT_HIGH, STL_LOADZOMIBETIMEDOUT, cl->name);
			}
		}
	}
	if (sv.paused && !nclients) {
		// nobody left, unpause the server
		if (SV_TogglePause(NULL))
			SV_BroadcastTPrintf(PRINT_HIGH, STL_CLIENTLESSUNPAUSE);
	}
}

/*
===================
SV_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
void SV_GetConsoleCommands (void)
{
	char	*cmd;

	while (1)
	{
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd, RESTRICT_LOCAL);
	}
}


int SV_RateForClient(client_t *cl)
{
	int rate;
	if (cl->download && cl->drate)
	{
		rate = cl->drate;
		if (rate > sv_maxdrate.value)
			rate = sv_maxdrate.value;
	}
	else
	{
		rate = cl->rate;
		if (rate > sv_maxrate.value)
			rate = sv_maxrate.value;
	}
	if (rate < 500)
		rate = 500;

	return rate;
}
/*
===================
SV_CheckVars

===================
*/
void SV_CheckVars (void)
{
	static char *pw, *spw;
	int			v;

	if (password.string == pw && spectator_password.string == spw)
		return;
	pw = password.string;
	spw = spectator_password.string;

	v = 0;
	if (pw && pw[0] && strcmp(pw, "none"))
		v |= 1;
	if (spw && spw[0] && strcmp(spw, "none"))
		v |= 2;

	Con_DPrintf ("Updated needpass.\n");
	if (!v)
		Info_SetValueForKey (svs.info, "needpass", "", MAX_SERVERINFO_STRING);
	else
		Info_SetValueForKey (svs.info, "needpass", va("%i",v), MAX_SERVERINFO_STRING);
}

#ifdef Q2SERVER
void SVQ2_ClearEvents(void)
{
	q2edict_t	*ent;
	int		i;

	for (i=0 ; i<ge->num_edicts ; i++, ent++)
	{
		ent = Q2EDICT_NUM(i);
		// events only last for a single message
		ent->s.event = 0;
	}
}
#endif


/*
==================
SV_Impulse_f

Spawns a client, uses an impulse, uses that clients think then removes the client.
==================
*/
void SV_Impulse_f (void)
{
	int i;
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (svs.clients[i].state == cs_free)
		{
			break;
		}
	}

	if (i == sv.allocated_client_slots)
	{
		Con_Printf("No empty player slots\n");
		return;
	}
	if (!svprogfuncs)
		return;

	pr_global_struct->time = sv.time;

	svs.clients[i].state = cs_connected;

	SV_SetUpClientEdict(&svs.clients[i], svs.clients[i].edict);

	svs.clients[i].edict->v->netname = PR_SetString(svprogfuncs, "Console");

	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PutClientInServer);

	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPreThink);
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, svs.clients[i].edict->v->think);
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPostThink);

	svs.clients[i].edict->v->impulse = atoi(Cmd_Argv(1));

	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPreThink);
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, svs.clients[i].edict->v->think);
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPostThink);

	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);

	svs.clients[i].state = cs_free;
}

/*
==================
SV_Frame

==================
*/
void SV_Frame (void)
{
	extern cvar_t pr_imitatemvdsv;
	static double	start, end;
	float oldtime;

	start = Sys_DoubleTime ();
	svs.stats.idle += start - end;
	end = start;
	svs.framenum++;

// keep the random time dependent
	rand ();

	if (!sv.gamespeed)
		sv.gamespeed = 1;

// decide the simulation time
	{
		oldtime = sv.time;
		sv.time = (Sys_DoubleTime() - sv.starttime)*sv.gamespeed;
		if (sv.time < oldtime)
			sv.time = oldtime;	//urm

		if (sv.paused)
		{
			sv.starttime += sv.time - oldtime;	//move the offset
			sv.time = oldtime;	//and keep time as it was.
		}

#ifndef SERVERONLY
		if (isDedicated)
#endif
			realtime += sv.time - oldtime;

	}


#ifdef IWEB_H__
	IWebRun();
#endif

	if (sv_master.value)
	{
		if (sv_masterport.value)
			SVM_Think(sv_masterport.value);
		else
			SVM_Think(PORT_MASTER);
	}

	{
void SV_MVDStream_Poll(void);
	SV_MVDStream_Poll();
	}

	if (sv.state < ss_active || !sv.worldmodel)
	{
#ifndef SERVERONLY
	// check for commands typed to the host
		if (isDedicated)
#endif
		{
			SV_GetConsoleCommands ();
			Cbuf_Execute ();
		}
		return;
	}

#ifdef VOICECHAT
	SVVC_Frame(sv_voicechat.value);
#endif

// check timeouts
	SV_CheckTimeouts ();

	SV_CheckTimer ();

// toggle the log buffer if full
	SV_CheckLog ();

// get packets
	SV_ReadPackets ();

	if (pr_imitatemvdsv.value)
	{
		Cbuf_Execute ();
		if (sv.state < ss_active)	//whoops...
			return;
	}

	if (sv.multicast.cursize)
	{
		Con_Printf("Unterminated multicast\n");
		sv.multicast.cursize=0;
	}

	// move autonomous things around if enough time has passed
	if (!sv.paused)
	{
		if (SV_Physics ())
			return;
	}
	else
	{
		PR_GameCodePausedTic(Sys_DoubleTime() - sv.pausedstart);
	}

	while(SV_ReadMVD());

	if (sv.multicast.cursize)
	{
		Con_Printf("Unterminated multicast\n");
		sv.multicast.cursize=0;
	}

#ifndef SERVERONLY
// check for commands typed to the host
	if (isDedicated)
#endif
	{
#ifdef PLUGINS
		Plug_Tick();
#endif

		SV_GetConsoleCommands ();

// process console commands
		if (!pr_imitatemvdsv.value)
			Cbuf_Execute ();
	}

	if (sv.state < ss_active)	//whoops...
		return;

	SV_CheckVars ();

// send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

//	demo_start = Sys_DoubleTime ();
	SV_SendMVDMessage();
//	demo_end = Sys_DoubleTime ();
//	svs.stats.demo += demo_end - demo_start;

// send a heartbeat to the master if needed
	Master_Heartbeat ();


#ifdef Q2SERVER
	if (ge && ge->edicts)
		SVQ2_ClearEvents();
#endif

// collect timing statistics
	end = Sys_DoubleTime ();
	svs.stats.active += end-start;
	if (++svs.stats.count == STATFRAMES)
	{
		svs.stats.latched_active = svs.stats.active;
		svs.stats.latched_idle = svs.stats.idle;
		svs.stats.latched_packets = svs.stats.packets;
		svs.stats.active = 0;
		svs.stats.idle = 0;
		svs.stats.packets = 0;
		svs.stats.count = 0;
	}
}

/*
===============
SV_InitLocal
===============
*/
extern void Log_Init (void);

void SV_InitLocal (void)
{
	int		i, p;
	extern	cvar_t	sv_maxvelocity;
	extern	cvar_t	sv_gravity;
	extern	cvar_t	sv_aim;
	extern	cvar_t	sv_stopspeed;
	extern	cvar_t	sv_spectatormaxspeed;
	extern	cvar_t	sv_accelerate;
	extern	cvar_t	sv_airaccelerate;
	extern	cvar_t	sv_wateraccelerate;
	extern	cvar_t	sv_friction;
	extern	cvar_t	sv_waterfriction;
	extern	cvar_t	pr_allowbutton1;

	extern	cvar_t	pm_bunnyspeedcap;
	extern	cvar_t	pm_ktjump;
	extern	cvar_t	pm_slidefix;
	extern	cvar_t	pm_airstep;
	extern	cvar_t	pm_walljump;
	extern	cvar_t	pm_slidyslopes;
	extern	cvar_t	pm_stepheight;

	SV_InitOperatorCommands	();
	SV_UserInit ();

#ifndef SERVERONLY
	if (isDedicated)
#endif
	{
		Cvar_Register (&developer,	cvargroup_servercontrol);

		Cvar_Register (&password,	cvargroup_servercontrol);
		Cvar_Register (&rcon_password,	cvargroup_servercontrol);

		Log_Init();
	}
	rcon_password.restriction = RESTRICT_MAX;	//no cheatie rconers changing rcon passwords...
	Cvar_Register (&spectator_password,	cvargroup_servercontrol);

	Cvar_Register (&sv_mintic,	cvargroup_servercontrol);
	Cvar_Register (&sv_maxtic,	cvargroup_servercontrol);

	Cvar_Register (&fraglimit,	cvargroup_serverinfo);
	Cvar_Register (&timelimit,	cvargroup_serverinfo);
	Cvar_Register (&teamplay,	cvargroup_serverinfo);
	Cvar_Register (&coop,	cvargroup_serverinfo);
	Cvar_Register (&skill,	cvargroup_serverinfo);
	Cvar_Register (&samelevel,	cvargroup_serverinfo);
	Cvar_Register (&maxclients,	cvargroup_serverinfo);
	Cvar_Register (&maxspectators,	cvargroup_serverinfo);
	Cvar_Register (&hostname,	cvargroup_serverinfo);
	Cvar_Register (&deathmatch,	cvargroup_serverinfo);
	Cvar_Register (&spawn,	cvargroup_servercontrol);

	//arguably cheats. Must be switched on to use.
	Cvar_Register (&watervis,	cvargroup_serverinfo);
	Cvar_Register (&rearview,	cvargroup_serverinfo);
	Cvar_Register (&mirrors,	cvargroup_serverinfo);
	Cvar_Register (&allow_luma,	cvargroup_serverinfo);
	Cvar_Register (&allow_bump,	cvargroup_serverinfo);
	Cvar_Register (&allow_skybox,	cvargroup_serverinfo);
	Cvar_Register (&sv_allow_splitscreen,	cvargroup_serverinfo);
	Cvar_Register (&fbskins,	cvargroup_serverinfo);

	Cvar_Register (&timeout,	cvargroup_servercontrol);
	Cvar_Register (&zombietime,	cvargroup_servercontrol);

	Cvar_Register (&sv_loadentfiles,	cvargroup_servercontrol);

	Cvar_Register (&sv_maxvelocity,			cvargroup_serverphysics);
	Cvar_Register (&sv_gravity,				cvargroup_serverphysics);
	Cvar_Register (&sv_stopspeed,			cvargroup_serverphysics);
	Cvar_Register (&sv_maxspeed,			cvargroup_serverphysics);
	Cvar_Register (&sv_spectatormaxspeed,	cvargroup_serverphysics);
	Cvar_Register (&sv_accelerate,			cvargroup_serverphysics);
	Cvar_Register (&sv_airaccelerate,		cvargroup_serverphysics);
	Cvar_Register (&sv_wateraccelerate,		cvargroup_serverphysics);
	Cvar_Register (&sv_friction,			cvargroup_serverphysics);
	Cvar_Register (&sv_waterfriction,		cvargroup_serverphysics);

	Cvar_Register (&sv_bigcoords,			cvargroup_serverphysics);

	Cvar_Register (&pm_bunnyspeedcap,		cvargroup_serverphysics);
	Cvar_Register (&pm_ktjump,				cvargroup_serverphysics);
	Cvar_Register (&pm_slidefix,			cvargroup_serverphysics);
	Cvar_Register (&pm_slidyslopes,			cvargroup_serverphysics);
	Cvar_Register (&pm_airstep,				cvargroup_serverphysics);
	Cvar_Register (&pm_walljump,			cvargroup_serverphysics);
	Cvar_Register (&pm_stepheight,			cvargroup_serverphysics);

	Cvar_Register (&sv_compatablehulls,		cvargroup_serverphysics);

	for (i = 0; i < sizeof(sv_motd)/sizeof(sv_motd[0]); i++)
		Cvar_Register(&sv_motd[i],	cvargroup_serverinfo);

	Cvar_Register (&sv_aim,	cvargroup_servercontrol);

	Cvar_Register (&sv_resetparms,	cvargroup_servercontrol);

	Cvar_Register (&sv_public,	cvargroup_servercontrol);
	Cvar_Register (&sv_listen_qw,	cvargroup_servercontrol);
	Cvar_Register (&sv_listen_nq,	cvargroup_servercontrol);
	Cvar_Register (&sv_listen_dp,	cvargroup_servercontrol);
	sv_listen_qw.restriction = RESTRICT_MAX;

#ifdef TCPCONNECT
	Cvar_Register (&sv_port_tcp,	cvargroup_servercontrol);
	sv_port_tcp.restriction = RESTRICT_MAX;
#endif
#ifdef IPPROTO_IPV6
	Cvar_Register (&sv_port_ipv6,	cvargroup_servercontrol);
	sv_port_ipv6.restriction = RESTRICT_MAX;
#endif
#ifdef USEIPX
	Cvar_Register (&sv_port_ipx,	cvargroup_servercontrol);
	sv_port_ipx.restriction = RESTRICT_MAX;
#endif
	Cvar_Register (&sv_port,	cvargroup_servercontrol);
	sv_port.restriction = RESTRICT_MAX;

	Cvar_Register (&sv_reportheartbeats, cvargroup_servercontrol);

#ifndef SERVERONLY
	if (isDedicated)
#endif
		Cvar_Set(&sv_public, "1");

	Cvar_Register (&sv_master,	cvargroup_servercontrol);
	Cvar_Register (&sv_masterport,	cvargroup_servercontrol);

	Cvar_Register (&filterban,	cvargroup_servercontrol);

	Cvar_Register (&allow_download,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_skins,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_models,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_sounds,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_maps,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_demos,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_anymap,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_pakcontents,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_textures,cvargroup_serverpermissions);
	Cvar_Register (&allow_download_configs,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_pk3s,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_wads,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_root,	cvargroup_serverpermissions);
	Cvar_Register (&secure,	cvargroup_serverpermissions);

	Cvar_Register (&sv_highchars,	cvargroup_servercontrol);

	Cvar_Register (&sv_phs,	cvargroup_servercontrol);

	Cvar_Register (&sv_csqcdebug, cvargroup_servercontrol);

	Cvar_Register (&sv_gamespeed, cvargroup_serverphysics);
	Cvar_Register (&sv_nomsec,	cvargroup_serverphysics);
	Cvar_Register (&pr_allowbutton1, cvargroup_servercontrol);

	Cvar_Register (&pausable,	cvargroup_servercontrol);

	Cvar_Register (&sv_voicechat,	cvargroup_servercontrol);
	Cvar_Register (&sv_maxrate, cvargroup_servercontrol);
	Cvar_Register (&sv_maxdrate, cvargroup_servercontrol);

	Cvar_Register (&sv_nailhack, cvargroup_servercontrol);

	Cmd_AddCommand ("sv_impulse", SV_Impulse_f);

	Cmd_AddCommand ("openroute", SV_OpenRoute_f);

	Cmd_AddCommand ("savegame", SV_Savegame_f);
	Cmd_AddCommand ("loadgame", SV_Loadgame_f);
	Cmd_AddCommand ("save", SV_Savegame_f);
	Cmd_AddCommand ("load", SV_Loadgame_f);

	SV_MVDInit();

	for (i=0 ; i<MAX_MODELS ; i++)
		sprintf (localmodels[i], "*%i", i);

#ifdef PEXT_SCALE
	svs.fteprotocolextensions |= PEXT_SCALE;
#endif
#ifdef PEXT_LIGHTSTYLECOL
	svs.fteprotocolextensions |= PEXT_LIGHTSTYLECOL;
#endif
#ifdef PEXT_TRANS
	svs.fteprotocolextensions |= PEXT_TRANS;
#endif
#ifdef PEXT_VIEW2
	svs.fteprotocolextensions |= PEXT_VIEW2;
#endif
#ifdef PEXT_BULLETENS
	svs.fteprotocolextensions |= PEXT_BULLETENS;
#endif
	svs.fteprotocolextensions |= PEXT_ACCURATETIMINGS;
#ifdef PEXT_ZLIBDL
	svs.fteprotocolextensions |= PEXT_ZLIBDL;
#endif
#ifdef PEXT_LIGHTUPDATES
	svs.fteprotocolextensions |= PEXT_LIGHTUPDATES;
#endif
#ifdef PEXT_FATNESS
	svs.fteprotocolextensions |= PEXT_FATNESS;
#endif
#ifdef PEXT_HLBSP
	svs.fteprotocolextensions |= PEXT_HLBSP;
#endif
#ifdef PEXT_Q2BSP
	svs.fteprotocolextensions |= PEXT_Q2BSP;
#endif
#ifdef PEXT_Q3BSP
	svs.fteprotocolextensions |= PEXT_Q3BSP;
#endif
#ifdef PEXT_TE_BULLET
	svs.fteprotocolextensions |= PEXT_TE_BULLET;
#endif
#ifdef PEXT_HULLSIZE
	svs.fteprotocolextensions |= PEXT_HULLSIZE;
#endif
#ifdef PEXT_SETVIEW
	svs.fteprotocolextensions |= PEXT_SETVIEW;
#endif
#ifdef PEXT_MODELDBL
	svs.fteprotocolextensions |= PEXT_MODELDBL;
#endif
#ifdef PEXT_FLOATCOORDS
	svs.fteprotocolextensions |= PEXT_FLOATCOORDS;
#endif
	svs.fteprotocolextensions |= PEXT_SPLITSCREEN;
	svs.fteprotocolextensions |= PEXT_HEXEN2;
	svs.fteprotocolextensions |= PEXT_COLOURMOD;
	svs.fteprotocolextensions |= PEXT_SPAWNSTATIC2;
	svs.fteprotocolextensions |= PEXT_CUSTOMTEMPEFFECTS;
	svs.fteprotocolextensions |= PEXT_256PACKETENTITIES;
	svs.fteprotocolextensions |= PEXT_ENTITYDBL;
	svs.fteprotocolextensions |= PEXT_ENTITYDBL2;
//	svs.fteprotocolextensions |= PEXT_64PLAYERS;
	svs.fteprotocolextensions |= PEXT_SHOWPIC;
	svs.fteprotocolextensions |= PEXT_SETATTACHMENT;

#ifdef PEXT_PK3DOWNLOADS
	svs.fteprotocolextensions |= PEXT_PK3DOWNLOADS;
#endif

#ifdef PEXT_CHUNKEDDOWNLOADS
	svs.fteprotocolextensions |= PEXT_CHUNKEDDOWNLOADS;
#endif

#ifdef PEXT_CSQC
	svs.fteprotocolextensions |= PEXT_CSQC;
#endif
#ifdef PEXT_DPFLAGS
	svs.fteprotocolextensions |= PEXT_DPFLAGS;
#endif

//	if (svs.protocolextensions)
//		Info_SetValueForStarKey (svs.info, "*"DISTRIBUTION"_ext", va("%x", svs.protocolextensions), MAX_SERVERINFO_STRING);

	Info_SetValueForStarKey (svs.info, "*version", va("%s %i", DISTRIBUTION, build_number()), MAX_SERVERINFO_STRING);

	Info_SetValueForStarKey (svs.info, "*z_ext", va("%i", SUPPORTED_Z_EXTENSIONS), MAX_SERVERINFO_STRING);

	// init fraglog stuff
	svs.logsequence = 1;
	svs.logtime = realtime;
	svs.log[0].data = svs.log_buf[0];
	svs.log[0].maxsize = sizeof(svs.log_buf[0]);
	svs.log[0].cursize = 0;
	svs.log[0].allowoverflow = true;
	svs.log[1].data = svs.log_buf[1];
	svs.log[1].maxsize = sizeof(svs.log_buf[1]);
	svs.log[1].cursize = 0;
	svs.log[1].allowoverflow = true;

	// parse params for cvars
	p = COM_CheckParm ("-port");
	if (!p)
		p = COM_CheckParm ("-svport");
	if (p && p < com_argc)
	{
		int port = atoi(com_argv[p+1]);
		if (!port)
			port = PORT_SERVER;
		Cvar_SetValue(&sv_port, port);
#ifdef IPPROTO_IPV6
		Cvar_SetValue(&sv_port_ipv6, port);
#endif
#ifdef USEIPX
		Cvar_SetValue(&sv_port_ipx, port);
#endif
	}

}


//============================================================================

void SV_Masterlist_Callback(struct cvar_s *var, char *oldvalue)
{
	int i;
	char	data[2];

	for (i = 0; sv_masterlist[i].cv.name; i++)
	{
		if (var == &sv_masterlist[i].cv)
			break;
	}

	if (!sv_masterlist[i].cv.name)
		return;

	if (!*var->string)
	{
		sv_masterlist[i].adr.port = 0;
		return;
	}

	if (!NET_StringToAdr(var->string, &sv_masterlist[i].adr))
	{
		sv_masterlist[i].adr.port = 0;
		Con_Printf ("Couldn't resolve master \"%s\"\n", var->string);
	}
	else
	{
		if (sv_masterlist[i].isdp)
		{
			if (sv_masterlist[i].adr.port == 0)
				sv_masterlist[i].adr.port = BigShort (27950);
		}
		else
		{
			if (sv_masterlist[i].adr.port == 0)
				sv_masterlist[i].adr.port = BigShort (27000);

			data[0] = A2A_PING;
			data[1] = 0;
			if (sv.state)
				NET_SendPacket (NS_SERVER, 2, data, sv_masterlist[i].adr);
		}
	}
}

/*
================
Master_Heartbeat

Send a message to the master every few minutes to
let it know we are alive, and log information
================
*/
#define	HEARTBEAT_SECONDS	300
void Master_Heartbeat (void)
{
	char		string[2048];
	int			active;
	int			i, j;
	qboolean	madeqwstring = false;

	if (!sv_public.value)
		return;

	if (realtime - svs.last_heartbeat < HEARTBEAT_SECONDS)
		return;		// not time to send yet

	svs.last_heartbeat = realtime;

	svs.heartbeat_sequence++;

	// send to group master
	for (i = 0; sv_masterlist[i].cv.name; i++)
	{
		if (sv_masterlist[i].adr.port)
		{
			switch(sv_masterlist[i].isdp)
			{
			case false:
				if (!madeqwstring)
				{
					// count active users
					active = 0;
					for (j=0 ; j<MAX_CLIENTS ; j++)
						if (svs.clients[j].state == cs_connected ||
						svs.clients[j].state == cs_spawned )
							active++;

					sprintf (string, "%c\n%i\n%i\n", S2M_HEARTBEAT,
						svs.heartbeat_sequence, active);

					madeqwstring = true;
				}

				if (sv_reportheartbeats.value)
					Con_Printf ("Sending heartbeat to %s\n", NET_AdrToString (sv_masterlist[i].adr));

				NET_SendPacket (NS_SERVER, strlen(string), string, sv_masterlist[i].adr);
				break;
			case true:
				if (sv_listen_dp.value)	//set listen to 1 to allow qw connections, 2 to allow nq connections too.
				{
					if (sv_reportheartbeats.value)
						Con_Printf ("Sending heartbeat to %s\n", NET_AdrToString (sv_masterlist[i].adr));

					{
						char *str = "\377\377\377\377heartbeat DarkPlaces\x0A";
						NET_SendPacket (NS_SERVER, strlen(str), str, sv_masterlist[i].adr);
					}
				}
				break;
			}
		}
	}
}

void Master_Add(char *stringadr)
{
	int i;

	for (i = 0; sv_masterlist[i].cv.name; i++)
	{
		if (!*sv_masterlist[i].cv.string)
			break;
	}

	if (!sv_masterlist[i].cv.name)
	{
		Con_Printf ("Too many masters\n");
		return;
	}

	Cvar_Set(&sv_masterlist[i].cv, stringadr);

	svs.last_heartbeat = -99999;
}

void Master_ClearAll(void)
{
	int i;
	for (i = 0; sv_masterlist[i].cv.name; i++)
	{
		Cvar_Set(&sv_masterlist[i].cv, "");
	}
}

/*
=================
Master_Shutdown

Informs all masters that this server is going down
=================
*/
void Master_Shutdown (void)
{
	char		string[2048];
	int			i;

	sprintf (string, "%c\n", S2M_SHUTDOWN);

	// send to group master
	for (i = 0; sv_masterlist[i].cv.name; i++)
		if (sv_masterlist[i].adr.port)
		{
			switch(sv_masterlist[i].isdp)
			{
			case false:
				if (sv_reportheartbeats.value)
					Con_Printf ("Sending heartbeat to %s\n", NET_AdrToString (sv_masterlist[i].adr));

				NET_SendPacket (NS_SERVER, strlen(string), string, sv_masterlist[i].adr);
				break;
			}
		}
}

#define iswhite(c) (c == ' ' || c == INVIS_CHAR1 || c == INVIS_CHAR2 || c == INVIS_CHAR3)
#define isinvalid(c) (c == '\r' || c == '\n')
//is allowed to shorten, out must be as long as in and min of "unnamed"+1
void SV_FixupName(char *in, char *out)
{
	char *s, *p;

	s = out;
	while(iswhite(*in) || isinvalid(*in))
		in++;
	while(*in)
	{
		if (isinvalid(*in))
		{
			in++;
			continue;
		}
		*s++ = *in++;
	}
	*s = '\0';

	if (!*out)
	{	//reached end and it was all whitespace
		//white space only
		strcpy(out, "unnamed");
		p = out;
	}

	for (p = out + strlen(out) - 1; p != out && iswhite(*p) ; p--)
		;
	p[1] = 0;
}


qboolean ReloadRanking(client_t *cl, char *newname)
{
#ifdef SVRANKING
	int newid;
	int j;
	rankstats_t rs;
	newid = Rank_GetPlayerID(newname, atoi(Info_ValueForKey (cl->userinfo, "_pwd")), true, false);	//'_' keys are always stripped. On any server. So try and use that so persistant data won't give out the password when connecting to a different server
	if (!newid)
		newid = Rank_GetPlayerID(newname, atoi(Info_ValueForKey (cl->userinfo, "password")), true, false);
	if (newid)
	{
		if (cl->rankid && cl->state >= cs_spawned)//apply current stats
		{
			if (!Rank_GetPlayerStats(cl->rankid, &rs))
				return false;
			rs.timeonserver += realtime - cl->stats_started;
			cl->stats_started = realtime;
			rs.kills += cl->kills;
			rs.deaths += cl->deaths;

			rs.flags1 &= ~(RANK_CUFFED|RANK_MUTED|RANK_CRIPPLED);
			if (cl->iscuffed==2)
				rs.flags1 |= RANK_CUFFED;
			if (cl->ismuted==2)
				rs.flags1 |= RANK_MUTED;
			if (cl->iscrippled==2)
				rs.flags1 |= RANK_CRIPPLED;
			cl->kills=0;
			cl->deaths=0;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, cl->edict);
			if (pr_nqglobal_struct->SetChangeParms)
				PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetChangeParms);
			for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
				if (spawnparamglobals[j])
					rs.parm[j] = *spawnparamglobals[j];
			Rank_SetPlayerStats(cl->rankid, &rs);
		}
		if (!Rank_GetPlayerStats(newid, &rs))
			return false;
		cl->rankid = newid;
		if (rs.flags1 & RANK_CUFFED)
			cl->iscuffed=2;
		else if (cl->iscuffed)	//continue being cuffed, but don't inflict the new user with persistant cuffing.
			cl->iscuffed=1;
		if (rs.flags1 & RANK_MUTED)
			cl->ismuted=2;
		else if (cl->ismuted)
			cl->ismuted=1;
		if (rs.flags1 & RANK_CRIPPLED)
			cl->iscrippled=2;
		else if (cl->iscrippled)
			cl->iscrippled=1;

		cl->trustlevel = rs.trustlevel;
		return true;
	}
#endif
	return false;
}
/*
=================
SV_ExtractFromUserinfo

Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void SV_ExtractFromUserinfo (client_t *cl)
{
	char	*val, *p;
	int		i;
	client_t	*client;
	int		dupc = 1;
	char	newname[80];

	val = Info_ValueForKey (cl->userinfo, "team");
	val[40] = 0;	//trim to smallish length now (to allow for adding more.
	Q_strncpyz (cl->team, val, sizeof(cl->teambuf));

	// name for C code
	val = Info_ValueForKey (cl->userinfo, "name");
	val[40] = 0;	//trim to smallish length now (to allow for adding more.

	if (cl->protocol != SCP_BAD || *val)
		SV_FixupName(val, newname);
	else
		newname[0] = 0;

	if (!val[0] && cl->protocol != SCP_BAD)
		strcpy(newname, "Hidden");
	else if (!stricmp(val, "console"))
	{
		strcpy(newname, "Not Console");
	}

	// check to see if another user by the same name exists
	while (1) {
		for (i=0, client = svs.clients ; i<MAX_CLIENTS ; i++, client++) {
			if (client->state < cs_connected || client == cl)
				continue;
			if (!stricmp(client->name, newname))
				break;
		}
		if (i != MAX_CLIENTS) { // dup name
			if (strlen(newname) > sizeof(cl->namebuf) - 1)
				newname[sizeof(cl->namebuf) - 4] = 0;
			p = newname;

			if (newname[0] == '(')
			{
				if (newname[2] == ')')
					p = newname + 3;
				else if (val[3] == ')')
					p = newname + 4;
			}

			memmove(newname+10, p, strlen(p)+1);

			sprintf(newname, "(%d)%-.40s", dupc++, newname+10);
		} else
			break;
	}

	if (strncmp(newname, cl->name, sizeof(cl->namebuf)-1))
	{
		if (cl->ismuted && *cl->name)
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_NONAMEASMUTE);
		else
		{

			Info_SetValueForKey (cl->userinfo, "name", newname, sizeof(cl->userinfo));
			if (!sv.paused && *cl->name)
			{
				if (!cl->lastnametime || realtime - cl->lastnametime > 5)
				{
					cl->lastnamecount = 0;
					cl->lastnametime = realtime;
				}
				else if (cl->lastnamecount++ > 4)
				{
					SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTKICKEDNAMESPAM, cl->name);
					SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREKICKEDNAMESPAM);
					SV_DropClient (cl);
					return;
				}
			}

			if (*cl->name && cl->state >= cs_spawned && !cl->spectator)
			{
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTNAMECHANGE, cl->name, val);
			}
			Q_strncpyz (cl->name, newname, sizeof(cl->namebuf));


#ifdef SVRANKING
			if (ReloadRanking(cl, newname))
			{
#endif
#ifdef SVRANKING
			}
			else if (cl->state >= cs_spawned)
				SV_ClientPrintf(cl, PRINT_HIGH, "Your rankings name has not been changed\n");
#endif
		}
	}

	Info_SetValueForKey(cl->userinfo, "name", newname, sizeof(cl->userinfo));

	val = Info_ValueForKey (cl->userinfo, "lang");
	cl->language = atoi(val);
	if (!cl->language)
		cl->language = LANGDEFAULT;

	val = Info_ValueForKey (cl->userinfo, "nogib");
	if (atoi(val))
		cl->gibfilter = true;
	else
		cl->gibfilter = false;

	// rate command
	val = Info_ValueForKey (cl->userinfo, "rate");
	if (strlen(val))
		cl->rate = atoi(val);
	else
		cl->rate = 2500;

	val = Info_ValueForKey (cl->userinfo, "drate");
	if (strlen(val))
		cl->drate = atoi(val);
	else
		cl->drate = 0;	//0 disables the downloading check

	val = Info_ValueForKey (cl->userinfo, "cl_playerclass");
	if (val)
	{
		PR_SetPlayerClass(cl, atoi(val), false);
	}

	// msg command
	val = Info_ValueForKey (cl->userinfo, "msg");
	if (strlen(val))
	{
		cl->messagelevel = atoi(val);
	}
#ifdef NQPROT
	{
		int top = atoi(Info_ValueForKey(cl->userinfo, "topcolor"));
		int bottom = atoi(Info_ValueForKey(cl->userinfo, "bottomcolor"));
		top &= 15;
		if (top > 13)
			top = 13;
		bottom &= 15;
		if (bottom > 13)
			bottom = 13;
		cl->playercolor = top*16 + bottom;
		if (svs.gametype == GT_PROGS)
		{
			cl->edict->v->clientcolors = cl->playercolor;
			MSG_WriteByte (&sv.nqreliable_datagram, svc_updatecolors);
			MSG_WriteByte (&sv.nqreliable_datagram, cl-svs.clients);
			MSG_WriteByte (&sv.nqreliable_datagram, cl->playercolor);
		}
	}
#endif
}

//============================================================================

/*
====================
SV_InitNet
====================
*/
void SV_InitNet (void)
{
	int i;

#ifndef SERVERONLY
	if (isDedicated)
#endif
	{
		NET_Init ();
		Netchan_Init ();
	}

	for (i = 0; sv_masterlist[i].cv.name; i++)
		Cvar_Register(&sv_masterlist[i].cv, "master servers");

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = -99999;		// send immediately
//	NET_StringToAdr ("192.246.40.70:27000", &idmaster_adr);
}


/*
====================
SV_Init
====================
*/
//FIXME: move to header
void SV_Demo_Init(void);
void SV_Init (quakeparms_t *parms)
{
#ifndef SERVERONLY
	if (isDedicated)
#endif
	{
		COM_InitArgv (parms->argc, parms->argv);

		if (COM_CheckParm ("-minmemory"))
			parms->memsize = MINIMUM_MEMORY;

		host_parms = *parms;

//		if (parms->memsize < MINIMUM_MEMORY)
//			SV_Error ("Only %4.1f megs of memory reported, can't execute game", parms->memsize / (float)0x100000);

		Memory_Init (parms->membase, parms->memsize);

		Sys_Init();

		COM_ParsePlusSets();

		Cbuf_Init ();
		Cmd_Init ();
#ifndef SERVERONLY
		R_SetRenderer(QR_NONE);
#endif
		COM_Init ();
		Mod_Init ();
	}

	PR_Init ();

	SV_InitNet ();

	SV_InitLocal ();

#ifdef IWEB_H__
	IWebInit();
#endif

	SV_Demo_Init();

#ifdef SVRANKING
	Rank_RegisterCommands();
#endif
	Cbuf_AddText("alias restart \"map .\"\nalias newgame \"map start\"\n", RESTRICT_LOCAL);

#ifndef SERVERONLY
	if (isDedicated)
#endif
	{
		PM_Init ();

#ifdef PLUGINS
		Plug_Init();
#endif

		Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
		host_hunklevel = Hunk_LowMark ();

		host_initialized = true;


		Con_TPrintf (TL_EXEDATETIME, __DATE__, __TIME__);
		Con_TPrintf (TL_HEAPSIZE,parms->memsize/ (1024*1024.0));

		Con_TPrintf (TL_VERSION, DISTRIBUTION, build_number());

		Con_TPrintf (STL_INITED);

		Cbuf_InsertText ("exec server.cfg\nexec ftesrv.cfg\n", RESTRICT_LOCAL, false);

	// process command line arguments
		Cbuf_Execute ();

		Cmd_StuffCmds();

		Cbuf_Execute ();

	// if a map wasn't specified on the command line, spawn start.map
		if (sv.state == ss_dead)
			Cmd_ExecuteString ("map start", RESTRICT_LOCAL);
		if (sv.state == ss_dead)
		{
			cvar_t *ml;
			ml = Cvar_Get("g_maplist", "dm1 dm2 dm3 dm4 dm5 dm6", 0, "");
			Cmd_TokenizeString(ml->string, false, false);
			if (Cmd_Argc())
				Cmd_ExecuteString(va("map %s", Cmd_Argv(rand()%Cmd_Argc())), RESTRICT_LOCAL);
		}
		if (sv.state == ss_dead)
			SV_Error ("Couldn't spawn a server");

	}
}

#endif
