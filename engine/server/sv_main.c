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

netadr_t	master_adr[MAX_MASTERS];	// address of group servers

client_t	*host_client;			// current client

cvar_t	sv_mintic = {"sv_mintic","0.03"};	// bound the size of the
cvar_t	sv_maxtic = {"sv_maxtic","0.1"};	// physics time tic 
cvar_t	sv_nailhack = {"sv_nailhack","0"};	// physics time tic 


cvar_t	timeout = {"timeout","65"};		// seconds without any message
cvar_t	zombietime = {"zombietime", "2"};	// seconds to sink messages
											// after disconnect
#ifdef SERVERONLY
cvar_t	developer = {"developer","0"};		// show extra messages

cvar_t	rcon_password = {"rcon_password", ""};	// password for remote server commands
cvar_t	password = {"password", ""};	// password for entering the game
#else
extern cvar_t	developer;
extern cvar_t	rcon_password;
extern cvar_t	password;
#endif
cvar_t	spectator_password = {"spectator_password", ""};	// password for entering as a sepctator

cvar_t	allow_download = {"allow_download", "1"};
cvar_t	allow_download_skins = {"allow_download_skins", "1"};
cvar_t	allow_download_models = {"allow_download_models", "1"};
cvar_t	allow_download_sounds = {"allow_download_sounds", "1"};
cvar_t	allow_download_demos = {"allow_download_demos", "1"};
cvar_t	allow_download_maps = {"allow_download_maps", "1"};
cvar_t	allow_download_anymap = {"allow_download_pakmaps", "0"};
cvar_t	allow_download_root = {"allow_download_root", "0"};
cvar_t	allow_download_textures = {"allow_download_textures", "1"};
cvar_t	allow_download_pk3s = {"allow_download_pk3s", "1"};
cvar_t	allow_download_wads = {"allow_download_wads", "1"};

cvar_t sv_public = {"sv_public", "1"};
cvar_t sv_highchars = {"sv_highchars", "1"};
cvar_t sv_loadentfiles = {"sv_loadentfiles", "1"};
cvar_t sv_maxrate = {"sv_maxrate", "10000"};
cvar_t sv_maxdrate = {"sv_maxdrate", "10000"};

cvar_t sv_bigcoords = {"sv_bigcoords", "", NULL, CVAR_SERVERINFO};

cvar_t sv_phs = {"sv_phs", "1"};
cvar_t sv_resetparms = {"sv_resetparms", "0"};

cvar_t sv_master = {"sv_master", "0"};
cvar_t sv_masterport = {"sv_masterport", "0"};

cvar_t	sv_voicechat = {"sv_voicechat", "0"};	//still development.

cvar_t pausable	= {"pausable", "1"};


//
// game rules mirrored in svs.info
//
cvar_t	fraglimit		= {"fraglimit",			"" ,	NULL, CVAR_SERVERINFO};
cvar_t	timelimit		= {"timelimit",			"" ,	NULL, CVAR_SERVERINFO};
cvar_t	teamplay		= {"teamplay",			"" ,	NULL, CVAR_SERVERINFO};
cvar_t	samelevel		= {"samelevel",			"" ,	NULL, CVAR_SERVERINFO};
cvar_t	maxclients		= {"maxclients",		"8",	NULL, CVAR_SERVERINFO};
cvar_t	maxspectators	= {"maxspectators",		"8",	NULL, CVAR_SERVERINFO};
#ifdef SERVERONLY
cvar_t	deathmatch		= {"deathmatch",		"1",	NULL, CVAR_SERVERINFO};			// 0, 1, or 2
#else
cvar_t	deathmatch		= {"deathmatch",		"0",	NULL, CVAR_SERVERINFO};			// 0, 1, or 2
#endif
cvar_t	coop			= {"coop",				"" ,	NULL, CVAR_SERVERINFO};
cvar_t	skill			= {"skill",				"" ,	NULL, CVAR_SERVERINFO};			// 0, 1, 2 or 3
cvar_t	spawn			= {"spawn",				"" ,	NULL, CVAR_SERVERINFO};
cvar_t	watervis		= {"watervis",			"" ,	NULL, CVAR_SERVERINFO};
cvar_t	rearview		= {"rearview",			"" ,	NULL, CVAR_SERVERINFO};
cvar_t	allow_luma		= {"allow_luma",		"1",	NULL, CVAR_SERVERINFO};
cvar_t	allow_bump		= {"allow_bump",		"1",	NULL, CVAR_SERVERINFO};
cvar_t	allow_skybox	= {"allow_skybox",		"1",	NULL, CVAR_SERVERINFO};
cvar_t	sv_allow_splitscreen = {"allow_splitscreen", "",NULL,CVAR_SERVERINFO};
cvar_t	fbskins			= {"fbskins",			"0",	NULL, CVAR_SERVERINFO};	//to get rid of lame fuhquake fbskins
cvar_t	mirrors			= {"mirrors",			"" ,	NULL, CVAR_SERVERINFO};

cvar_t	sv_motd[]		={	{"sv_motd1",			""},
							{"sv_motd2",			""},
							{"sv_motd3",			""},
							{"sv_motd4",			""}	};

cvar_t sv_compatablehulls = {"sv_compatablehulls", "1"};

cvar_t	hostname = {"hostname","unnamed", NULL, CVAR_SERVERINFO};

cvar_t	secure = {"secure", "", NULL, CVAR_SERVERINFO};

extern cvar_t sv_nomsec;

char cvargroup_serverpermissions[] = "server permission variables";
char cvargroup_serverinfo[] = "serverinfo variables";
char cvargroup_serverphysics[] = "server physics variables";
char cvargroup_servercontrol[] = "server control variables";

FILE	*sv_logfile;
FILE	*sv_fraglogfile;

void SV_FixupName(char *in, char *out);
void SV_AcceptClient (netadr_t adr, int userid, char *userinfo);
void Master_Shutdown (void);

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
	if (sv_logfile)
	{
		fclose (sv_logfile);
		sv_logfile = NULL;
	}
	if (sv_fraglogfile)
	{
		fclose (sv_fraglogfile);
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
	_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

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
	MSG_WriteByte (&drop->netchan.message, drop->isq2client?svcq2_disconnect:svc_disconnect);

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
					rs.parm[j] = (&pr_global_struct->parm1)[j];
				Rank_SetPlayerStats(drop->rankid, &rs);
			}
		}
	}
#endif
#ifdef SVCHAT
	SV_WipeChat(drop);
#endif

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
#ifdef Q2SERVER
	else if (ge && drop->q2edict)	//could be in sv_spawnserver.
		ge->ClientDisconnect(drop->q2edict);
#endif

	if (drop->spectator)
		Con_Printf ("Spectator %s removed\n",drop->name);
	else
		Con_Printf ("Client %s removed\n",drop->name);

	if (drop->download)
	{
		fclose (drop->download);
		drop->download = NULL;
	}
	if (drop->upload)
	{
		fclose (drop->upload);
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

	while(drop->enteffects)
	{
		specialenteffects_t *next = drop->enteffects->next;
		Z_Free(drop->enteffects);
		drop->enteffects = next;
	}

	drop->old_frags = 0;
	drop->kills = 0;
	drop->deaths = 0;
	if (svprogfuncs && drop->edict)
		drop->edict->v.frags = 0;
	drop->name[0] = 0;
	memset (drop->userinfo, 0, sizeof(drop->userinfo));

#ifdef Q2SERVER
	if (drop->isq2client)
	{
		if (drop->q2frames)
		{
			Z_Free(drop->q2frames);
			drop->q2frames = NULL;
		}
	}
	else
#endif
		if (drop->frames)
	{
		Z_Free(drop->frames);
		drop->frames = NULL;
	}

// send notification to all remaining clients
	SV_FullClientUpdate (drop, &sv.reliable_datagram);
#ifdef NQPROT
	SVNQ_FullClientUpdate (drop, &sv.nqreliable_datagram);


	if (drop->netchan.qsocket)
	{
		NET_Close(drop->netchan.qsocket);
		drop->netchan.qsocket = NULL;
	}
#endif

	if (drop->controlled)
	{
		drop->controlled->controller = NULL;
		SV_DropClient(drop->controlled);
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

	if (!cl->frames || cl->isq2client)
		return 0;

	ping = 0;
	count = 0;
	for (frame = cl->frames, i=0 ; i<UPDATE_BACKUP ; i++, frame++)
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

/*
===================
SV_FullClientUpdate

Writes all update values to a sizebuf
===================
*/
void SV_FullClientUpdate (client_t *client, sizebuf_t *buf)
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

	strcpy (info, client->userinfo);
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
	if (cl->nqprot)
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
		ClientReliableCheckBlock(cl, 24 + strlen(client->userinfo));
		if (cl->num_backbuf) {
			SV_FullClientUpdate (client, &cl->backbuf);
			ClientReliable_FinishWrite(cl);
		} else
			SV_FullClientUpdate (client, &cl->netchan.message);
	}
}


/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see
This message can be up to around 5k with worst case string lengths.
================
*/
void SVC_Status (void)
{
	int		i;
	client_t	*cl;
	int		ping;
	int		top, bottom;

	int slots=0;

	Cmd_TokenizeString ("status");
	SV_BeginRedirect (RD_PACKET);
	Con_Printf ("%s\n", svs.info);
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		cl = &svs.clients[i];
		if ((cl->state == cs_connected || cl->state == cs_spawned || cl->name[0]) && !cl->spectator)
		{
			top = atoi(Info_ValueForKey (cl->userinfo, "topcolor"));
			bottom = atoi(Info_ValueForKey (cl->userinfo, "bottomcolor"));
			top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
			bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
			ping = SV_CalcPing (cl);
			if (!cl->state)	//show bots differently. Just to be courteous.
				Con_Printf ("%i %i %i %i \"BOT:%s\" \"%s\" %i %i\n", cl->userid, 
					cl->old_frags, (int)(realtime - cl->connection_started)/60,
					ping, cl->name, Info_ValueForKey (cl->userinfo, "skin"), top, bottom);
			else
				Con_Printf ("%i %i %i %i \"%s\" \"%s\" %i %i\n", cl->userid, 
					cl->old_frags, (int)(realtime - cl->connection_started)/60,
					ping, cl->name, Info_ValueForKey (cl->userinfo, "skin"), top, bottom);
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
		Con_Printf ("5010 32 85 162 \"€ƒ‚²¥ÄOÎÅ\" \"hacker\" 13 13\n");
	if (slots>8)
		Con_Printf ("5011 32 85 162 \"ÁlòeaäùÄEÁÄ‘\" \"hacker\" 4 4\n");
	*/
	SV_EndRedirect ();
}

void SVC_InfoQ2 (void)
{
	char	string[64];
	int		i, count;
	int		version;

	if (maxclients.value == 1)
		return;		// ignore in single player

	version = atoi (Cmd_Argv(1));

	if (version != PROTOCOL_VERSION_Q2)
		_snprintf (string, sizeof(string), "%s: wrong version\n", hostname.string);
	else
	{
		count = 0;
		for (i=0 ; i<maxclients.value ; i++)
			if (svs.clients[i].state >= cs_connected)
				count++;

		_snprintf (string, sizeof(string), "%16s %8s %2i/%2i\n", hostname.string, sv.name, count, (int)maxclients.value);
	}

	Netchan_OutOfBandPrint (NS_SERVER, net_from, "info\n%s", string);
}

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
	{	// they allready have this data, or we aren't logging frags
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
		if (!svprogfuncs)//htons(net_from.port) == PORT_CLIENT)	//quake 2 client port - this is a hack based on an evil assumtion.
			buf = va("challenge %i", svs.challenges[i].challenge);
		else
			buf = va("%c%i", S2C_CHALLENGE, svs.challenges[i].challenge);

		over = buf + strlen(buf) + 1;

		if (svprogfuncs)
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
		Netchan_OutOfBand(NS_SERVER, net_from, strlen(buf)+1+4+4, buf);
	}

//	Netchan_OutOfBandPrint (net_from, "%c%i", S2C_CHALLENGE, 
//				svs.challenges[i].challenge);
}
#ifdef NQPROT
void SVC_GetChallengeQSocket (qsocket_t *sock)
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
		buf = va("    %c%i", S2C_CHALLENGE, svs.challenges[i].challenge);

		over = buf + strlen(buf) + 1;
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

		{
			sizebuf_t msg;
			msg.data = buf;
			*(int *)buf = -1;
			msg.cursize = strlen(buf)+1+4+4;			
			NET_SendMessage(sock, &msg);
		}
//		Netchan_OutOfBand(svs.socket, net_from, strlen(buf)+1+4+4, buf);
	}

//	Netchan_OutOfBandPrint (net_from, "%c%i", S2C_CHALLENGE, 
//				svs.challenges[i].challenge);
}
#endif


void SVC_DecodeConnect (void)
{
	qbyte msg[1024];	
	int msglen = net_message.cursize;
	int firstcoded;

	netadr_t validater;
	validater.type = NA_IP;
	validater.ip[0] = 127;
	validater.ip[1] = 0;
	validater.ip[2] = 0;
	validater.ip[3] = 1;
	validater.port = BigShort(20235);

	msg[0] = 255;msg[1] = 255;msg[2] = 255;msg[3] = 255;
	msg[4] = 'v';
	msg[5] = 6+13+6;
	msglen = 6;
	strcpy(msg+msglen, "validconnect ");
	msglen+=strlen(msg+msglen);
	strcpy(msg+msglen, NET_AdrToString(net_from));	
	strcat(msg+msglen, " ");
	msglen+=strlen(msg+msglen);
//	memcpy(msg+msglen, &net_from, 6);	
//	msglen+=6;
	firstcoded = msglen;
	memcpy(msg+msglen, net_message.data+18, net_message.cursize-18);
	msglen += net_message.cursize-18;

	msg[5] = firstcoded;
	Netchan_OutOfBand(NS_SERVER, validater, msglen-4, msg+4);
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
			cl->spawn_parms[i] = (&pr_global_struct->parm1)[i];	
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
		_vsnprintf (string+6,sizeof(string)-1-6, format+1,argptr);
	}
	else
	{
		string[0] = A2C_PRINT;
		string[1] = '\n';
		_vsnprintf (string+2,sizeof(string)-1-2, format,argptr);
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
		_vsnprintf (string+6,sizeof(string)-1-6, format+1,argptr);
	}
	else
	{
		string[0] = A2C_PRINT;
		_vsnprintf (string+1,sizeof(string)-1-1, format,argptr);
	}
	va_end (argptr);


	Netchan_OutOfBand (NS_SERVER, adr, strlen(string), (qbyte *)string);
}

/*
==================
SVC_DirectConnect

A connection request that did not come from the master
==================
*/
int	nextuserid;
void SVC_DirectConnect
#ifdef NQPROT
		(qsocket_t *socket)
#else
		(void)
#endif
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

	int numssclients;

	qboolean isquake2client = false;

	unsigned int protextsupported=0;
	

	char *name;

	numssclients = 1;
	if (atoi(Cmd_Argv(0)+7))
	{
		if (!socket)
			numssclients = atoi(Cmd_Argv(0)+7);
		if (numssclients<1)
		{
			SV_OutOfBandPrintf (false, net_from, "\nServer is version %4.2f.\n", VERSION);
			Con_Printf ("* rejected connect from broken client\n");
			return;
		}
	}

	version = atoi(Cmd_Argv(1));
	if (version >= 31 && version <= 34)
	{
		numssclients = 1;

		//quake2
		isquake2client = version;
	}
	else if (version != PROTOCOL_VERSION)
	{
		SV_OutOfBandPrintf (false, net_from, "\nServer is version %4.2f.\n", VERSION);
		Con_Printf ("* rejected connect from version %i\n", version);
		return;
	}

	qport = atoi(Cmd_Argv(2));

	challenge = atoi(Cmd_Argv(3));

	// note an extra qbyte is needed to replace spectator key
	for (i = 0; i < numssclients; i++)
		Q_strncpyz (userinfo[i], Cmd_Argv(4+i), sizeof(userinfo[i])-1);

	if (!isquake2client)	//readd?
	{
		while(!msg_badread)
		{
			Cmd_TokenizeString(MSG_ReadStringLine());
			switch(Q_atoi(Cmd_Argv(0)))
			{
			case PROTOCOL_VERSION_FTE:
				protextsupported = Q_atoi(Cmd_Argv(1));
				Con_DPrintf("Client supports 0x%x fte extensions\n", protextsupported);
				break;
			case PROTOCOL_VERSION_HUFFMAN:
				huffcrc = Q_atoi(Cmd_Argv(1));
				Con_Printf("Client supports huffman compression\n", huffcrc);
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
	// see if the challenge is valid
		for (i=0 ; i<MAX_CHALLENGES ; i++)
		{
			if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
			{
				if (challenge == svs.challenges[i].challenge)
					break;		// good
				SV_OutOfBandPrintf (isquake2client, net_from, "\nBad challenge.\n");
				return;
			}
		}
	if (i == MAX_CHALLENGES)
	{
		SV_OutOfBandPrintf (isquake2client, net_from, "No challenge for address.\n");
		return;
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
				SV_OutOfBandPrintf (isquake2client, net_from, "\nrequires a spectator password\n\n");
				return;
			}
			Info_RemoveKey (userinfo[0], "spectator"); // remove key
			Info_SetValueForStarKey (userinfo[0], "*spectator", "1", MAX_INFO_STRING);
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
				SV_OutOfBandPrintf (isquake2client, net_from, "\nserver requires a password\n\n");
				return;
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
#ifdef NQPROT
	newcl->nqprot = false;		
#endif

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

	// if there is allready a slot for this ip, drop it
	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
		if (cl->state == cs_free)
			continue;
		if (NET_CompareBaseAdr (adr, cl->netchan.remote_address)
			&& ( cl->netchan.qport == qport 
			|| adr.port == cl->netchan.remote_address.port ))
		{
			if (cl->state == cs_connected) {
				Con_Printf("%s:dup connect\n", NET_AdrToString (adr));
				nextuserid--;
				return;
			}

			if (!socket)
			{
				Con_Printf ("%s:reconnect\n", NET_AdrToString (adr));
//				SV_DropClient (cl);
			}
			break;
		}
	}

	name = Info_ValueForKey (temp.userinfo, "name");

	if (!isquake2client &&!atoi(Info_ValueForKey (temp.userinfo, "iknow")))
	{
		if (sv.worldmodel->fromgame == fg_halflife && !(newcl->fteprotocolextensions & PEXT_HLBSP))
		{
			if (atof(Info_ValueForKey (temp.userinfo, "*FuhQuake")) < 0.3)
			{
				SV_OutOfBandPrintf (isquake2client, adr, "%c\nThe server is using a halflife level and we don't think your client supports this\nuse 'setinfo iknow 1' to ignore this check\nYou can go to "ENGINEWEBSITE" to get a compatable client\n\nYou may need to enable an option\n\n", A2C_PRINT);
				Con_Printf("player %s was dropped due to incompatable client\n", name);
				return;
			}
		}
#ifdef PEXT_Q2BSP
		else if (sv.worldmodel->fromgame == fg_quake2 && !(newcl->fteprotocolextensions & PEXT_Q2BSP))
		{
			SV_OutOfBandPrintf (isquake2client, adr, "%c\nThe server is using a quake 2 level and we don't think your client supports this\nuse 'setinfo iknow 1' to ignore this check\nYou can go to "ENGINEWEBSITE" to get a compatable client\n\nYou may need to enable an option\n\n", A2C_PRINT);
			Con_Printf("player %s was dropped due to incompatable client\n", name);
			return;
		}
#endif
#ifdef PEXT_Q3BSP
		else if (sv.worldmodel->fromgame == fg_quake3 && !(newcl->fteprotocolextensions & PEXT_Q3BSP))
		{
			SV_OutOfBandPrintf (isquake2client, adr, "%c\nThe server is using a quake 3 level and we don't think your client supports this\nuse 'setinfo iknow 1' to ignore this check\nYou can go to "ENGINEWEBSITE" to get a compatable client\n\nYou may need to enable an option\n\n", A2C_PRINT);
			Con_Printf("player %s was dropped due to incompatable client\n", name);
			return;
		}
#endif
	}

	SV_FixupName(name, name);

	if (!*name)
		name = "Hidden";
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
#ifdef NQPROT
		else if (socket && cl->netchan.qsocket == socket)
		{
			newcl = cl;	//replace this slot - it was a place holder for a qw protocol over nq transports.
		}
#endif
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
			SV_OutOfBandPrintf (isquake2client, adr, "\nserver is full\n\n");
			return;
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
				SV_OutOfBandPrintf (isquake2client, adr, "\nserver is full\n\n");
			}
			return;
		}
	}


	{
		bannedips_t *banip;
		for (banip = svs.bannedips; banip; banip=banip->next)
		{
			if (banip->adr.port)
			{
				if (NET_CompareAdr (adr, newcl->netchan.remote_address))
				{
					SV_OutOfBandPrintf (isquake2client, adr, "\nYou were banned.\nContact the administrator to complain.\n");
					return;
				}
			}
			else
			{
				if (NET_CompareBaseAdr (adr, newcl->netchan.remote_address))
				{
					SV_OutOfBandPrintf (isquake2client, adr, "\nYou were banned.\nContact the administrator to complain.\n");
					return;
				}
			}
		}
		//yay, a legit client who we havn't banned yet.
	}

	edictnum = (newcl-svs.clients)+1;
	if (svprogfuncs)
	{
		ent = EDICT_NUM(svprogfuncs, edictnum);	
#ifdef Q2SERVER
		temp.q2edict = NULL;
#endif
		temp.edict = ent;






		// build a new connection
		// accept the new client
		// this is the only place a client_t is ever initialized
		temp.frames = newcl->frames;	//don't touch these.
		if (temp.frames)
			Z_Free(temp.frames);

		temp.frames = Z_Malloc((sizeof(client_frame_t)+sizeof(entity_state_t)*maxpacketentities)*UPDATE_BACKUP);
		for (i = 0; i < UPDATE_BACKUP; i++)
		{
			temp.frames[i].entities.max_entities = maxpacketentities;
			temp.frames[i].entities.entities = (entity_state_t*)(temp.frames+UPDATE_BACKUP) + i*temp.frames[i].entities.max_entities;
		}
	}
#ifdef Q2SERVER
	else
	{
		q2ent = Q2EDICT_NUM(edictnum);	
		temp.edict = NULL;
		temp.q2edict = q2ent;

		if (!ge->ClientConnect(q2ent, temp.userinfo))
			return;



		// build a new connection
		// accept the new client
		// this is the only place a client_t is ever initialized
		temp.q2frames = newcl->q2frames;	//don't touch these.
		if (temp.q2frames)
			Z_Free(temp.q2frames);

		temp.q2frames = Z_Malloc(sizeof(q2client_frame_t)*Q2UPDATE_BACKUP);
	}
#endif
	

	*newcl = temp;

	newcl->zquake_extensions = atoi(Info_ValueForKey(newcl->userinfo, "*z_ext"));

	//dmw - delayed - Netchan_OutOfBandPrint (adr, "%c", S2C_CONNECTION );
	
	Netchan_Setup (NS_SERVER, &newcl->netchan , adr, qport);

	if (huffcrc)
		newcl->netchan.compress = true;
	else
		newcl->netchan.compress = false;

	newcl->state = cs_connected;

	newcl->datagram.allowoverflow = true;
	newcl->datagram.data = newcl->datagram_buf;
	newcl->datagram.maxsize = sizeof(newcl->datagram_buf);

	// spectator mode can ONLY be set at join time
	newcl->spectator = spectator;

	newcl->isq2client = isquake2client;
	
	// parse some info from the info strings
	SV_ExtractFromUserinfo (newcl);

	// JACK: Init the floodprot stuff.
	for (i=0; i<10; i++)
		newcl->whensaid[i] = 0.0;
	newcl->whensaidhead = 0;
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
				SV_OutOfBandPrintf (isquake2client, adr, "\nBad password/username\nThis server requires logins. Please see the serverinfo for website and info on how to register.\n");
				newcl->state = cs_free;
				return;
			}

//			SV_OutOfBandPrintf (isquake2client, adr, "\nWARNING: You have not got a place on the ranking system, probably because a user with the same name has already connected and your pwds differ.\n\n");

			SV_GetNewSpawnParms(newcl);
		}
		else
		{
			rankstats_t rs;
			if (!Rank_GetPlayerStats(newcl->rankid, &rs))
			{
				SV_OutOfBandPrintf (isquake2client, adr, "\nRankings/Account system failed\n");
				newcl->state = cs_free;
				return;
			}

			if (rs.flags1 & RANK_BANNED)
			{			
				SV_OutOfBandTPrintf (isquake2client, adr, newcl->language, STL_YOUAREBANNED);
				Con_Printf("banned player %s is trying to connect\n", newcl->name);
				newcl->name[0] = 0;
				memset (newcl->userinfo, 0, sizeof(newcl->userinfo));
				newcl->state = cs_free;
				return;
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

				SV_OutOfBandPrintf (isquake2client, adr, s);
			}
			else if (!cl->istobeloaded)
			{
				SV_GetNewSpawnParms(newcl);

				SV_OutOfBandTPrintf (isquake2client, adr, newcl->language, STL_FIRSTGREETING, newcl->name, (int)rs.timeonserver);
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
#ifdef NQPROT
		if (socket)
		{
			sizebuf_t msg;
			char data[64];
			*(int*)data = -1;
			data[4] = S2C_CONNECTION;
			data[5] = 0;
			msg.data = data;
			msg.cursize = 5;
			newcl->netchan.qsocket = socket;
			NET_SendMessage(socket, &msg);
		}
		else
#endif
			 if (isquake2client)
			Netchan_OutOfBandPrint (NS_SERVER, adr, "client_connect");
		else
			Netchan_OutOfBandPrint (NS_SERVER, adr, "%c", S2C_CONNECTION );

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

		temp.frames = cl->frames;	//don't touch these.
		temp.edict = cl->edict;
		memcpy(cl, newcl, sizeof(client_t));
		cl->frames = temp.frames;
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
			Info_SetValueForStarKey (cl->userinfo, "*spectator", "1", MAX_INFO_STRING);
		}
		else
			Info_RemoveKey (cl->userinfo, "*spectator");

		SV_ExtractFromUserinfo (cl);

		SV_GetNewSpawnParms(cl);
	}
	newcl->controller = NULL;


	Sys_ServerActivity();
}
void SV_ClearClient(client_t *cl)
{
	client_frame_t *frames = cl->frames;
	memset(cl, 0, sizeof(client_t));
	cl->frames = frames;
	if (frames)
		memset(frames, 0, sizeof(client_frame_t)*UPDATE_BACKUP);
}
#ifdef NQPROT
//FIXME: move to header
void SV_New_f (void);
void SVNQ_New_f (void);
void SVNQ_CheckForNewClients(void)
{
	client_t	*newcl, *cl;
	struct qsocket_s	*ret;
	int				i;

	netadr_t addr;

	edict_t		*ent;

#ifdef Q2SERVER
	if (ge)	//disable nq entirly if using q2.
		return;
#endif
		
//
// check for new connections
//
	while (1)
	{
		ret = NET_CheckNewConnections ();
		if (!ret)
			break;

		*(int*)net_from.ip = 0;		

/*		SVC_DirectConnect();
		net_activeconnections++;
		return;
*/

		newcl = NULL;
		for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (cl->netchan.qsocket == ret)
				SV_DropClient(cl);			
		}

		for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (cl->state != cs_free)
				continue;
			
			break;
		}
		if (i==MAX_CLIENTS)
			Sys_Error("Erm... Out of client slots. Hmm..");

		SV_ClearClient(cl);
		if (cl->frames)
			Z_Free(cl->frames);
		cl->frames = Z_Malloc((sizeof(client_frame_t)+sizeof(entity_state_t)*MAX_STANDARD_PACKET_ENTITIES)*UPDATE_BACKUP);
		for (i = 0; i < UPDATE_BACKUP; i++)
		{
			cl->frames[i].entities.max_entities = MAX_STANDARD_PACKET_ENTITIES;
			cl->frames[i].entities.entities = (entity_state_t*)(cl->frames+UPDATE_BACKUP) + i*cl->frames[i].entities.max_entities;
		}

		net_from.port = cl - svs.clients;
		cl->state = cs_zombie;
		cl->userid = nextuserid++;		

		cl->language = LANGDEFAULT;
		
		ent = EDICT_NUM(svprogfuncs, (cl-svs.clients)+1);	
		cl->edict = ent;

		host_client = cl;
		if (!strcmp(ret->address, "LOCAL"))
		{
			addr.ip[0] = 0;
			addr.ip[1] = 0;
			addr.ip[2] = 0;
			addr.ip[3] = 0;
			addr.port = 0;
		}
		else
			NET_StringToAdr(ret->address, &addr);
		Netchan_Setup(ret->socket, &cl->netchan, addr, 0);
		cl->netchan.message.maxsize = MAX_NQMSGLEN;
		cl->netchan.qsocket = ret;
		
		cl->send_message = true;

		cl->datagram.allowoverflow = true;
		cl->datagram.data = cl->datagram_buf;
		cl->datagram.maxsize = MAX_NQDATAGRAM;

		if (!ret->qwprotocol)
		{
			cl->state = cs_connected;	//this is a real player
			*cl->userinfo = '\0';
			Info_SetValueForKey(cl->userinfo, "name", "unnamed", MAX_INFO_STRING);
			SV_ExtractFromUserinfo(cl);

			if (pr_global_struct->SetNewParms)
				PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);
			for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
				cl->spawn_parms[i] = (&pr_global_struct->parm1)[i];	

			SVNQ_New_f();
		}
		else
		{
			//this is just to recieve the connectionless messages over a specific connection...
			cl->state = cs_zombie;	//zombie for a nice timeout
			cl->connection_started = -1;	//drop when qsocket fails.
			cl->sendinfo = false;
			SVC_GetChallengeQSocket(ret);
//			SV_New_f();
		}

//		Netchan_Transmit(&cl->netchan, cl->netchan.message.cursize, cl->netchan.message.data);
//		cl->netchan.message.cursize = 0;
//		NET_SendMessage(ret, &cl->netchan.message);

		net_activeconnections++;
	}
}
#endif
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

	if (!Rcon_Validate ())
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
			rid = Rank_GetPlayerID(s, atoi(colon), false);
			if (rid)
			{
				if (!Rank_GetPlayerStats(rid, &stats))
					return;

					
				Con_Printf ("Rcon from %s:\n%s\n"
					, NET_AdrToString (net_from), net_message.data+4);

				SV_BeginRedirect (RD_PACKET);

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

		Con_Printf ("Bad rcon from %s:\n%s\n"
			, NET_AdrToString (net_from), net_message.data+4);

		SV_BeginRedirect (RD_PACKET);

		Con_Printf ("Bad rcon_password.\n");

	}
	else
	{

		Con_Printf ("Rcon from %s:\n%s\n"
			, NET_AdrToString (net_from), net_message.data+4);

		SV_BeginRedirect (RD_PACKET);

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


/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
#ifdef NQPROT
qboolean SV_ConnectionlessPacket (qsocket_t *sock)
#else
qboolean SV_ConnectionlessPacket (void)
#endif
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

	Cmd_TokenizeString (s);

	c = Cmd_Argv(0);

	if (!strcmp(c, "ping") || ( c[0] == A2A_PING && (c[1] == 0 || c[1] == '\n')) )
		SVC_Ping ();
	else if (c[0] == A2A_ACK && (c[1] == 0 || c[1] == '\n') )
		Con_Printf ("A2A_ACK from %s\n", NET_AdrToString (net_from));
	else if (!strcmp(c,"status"))
		SVC_Status ();
	else if (!strcmp(c,"log"))
		SVC_Log ();
	else if (!strcmp(c, "info"))
		SVC_InfoQ2 ();
	else if (!strncmp(c,"connect", 7))
	{
		if (secure.value)	//FIXME: possible problem for nq clients when enabled
			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nThis server requires client validation.\nPlease use the "DISTRIBUTION" validation program\n", A2C_PRINT);
		else
		{
#ifdef NQPROT
			SVC_DirectConnect (sock);
#else
			SVC_DirectConnect ();
#endif
			return true;
		}
	}
	else if (!strcmp(c,"verifyconnect"))
		SVC_DecodeConnect ();		
	else if (!strcmp(c,"validconnect"))
	{
		if (net_from.type != NA_IP || ((*(unsigned *)net_from.ip != *(unsigned *)net_local_ipadr.ip
			&& *(unsigned *)net_from.ip != htonl(INADDR_LOOPBACK))) )
		{
			Con_Printf ("Validified connect packet from remote host.  Ignored.\n");			
		}
		else
		{
			//these messages contain the ip address that it was origionally from (before being farmed out)
			NET_StringToAdr(Cmd_Argv(1), &net_from);
			Cmd_ShiftArgs(1);	//get rid of the ip address...		
#ifdef NQPROT
			SVC_DirectConnect (sock);
#else
			SVC_DirectConnect ();
#endif
			return true;
		}
	}
	else if (!strcmp(c,"getchallenge"))
	{
#ifdef NQPROT
		if (sock)
			SVC_GetChallengeQSocket (sock);
		else
#endif
			SVC_GetChallenge ();
	}
	else if (!strcmp(c, "rcon"))
		SVC_RemoteCommand ();
	else
		Con_Printf ("bad connectionless packet from %s:\n%s\n"
		, NET_AdrToString (net_from), s);

	return false;
}

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


typedef struct
{
	unsigned	mask;
	unsigned	compare;
} ipfilter_t;

#define	MAX_IPFILTERS	1024

ipfilter_t	ipfilters[MAX_IPFILTERS];
int			numipfilters;

cvar_t	filterban = {"filterban", "1"};

/*
=================
StringToFilter
=================
*/
qboolean StringToFilter (char *s, ipfilter_t *f)
{
	char	num[128];
	int		i, j;
	qbyte	b[4];
	qbyte	m[4];
	
	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}
	
	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			Con_Printf ("Bad filter address: %s\n", s);
			return false;
		}
		
		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}
	
	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;
	
	return true;
}

/*
=================
SV_AddIP_f
=================
*/
void SV_AddIP_f (void)
{
	int		i;
	
	for (i=0 ; i<numipfilters ; i++)
		if (ipfilters[i].compare == 0xffffffff)
			break;		// free spot
	if (i == numipfilters)
	{
		if (numipfilters == MAX_IPFILTERS)
		{
			Con_Printf ("IP filter list is full\n");
			return;
		}
		numipfilters++;
	}
	
	if (!StringToFilter (Cmd_Argv(1), &ipfilters[i]))
		ipfilters[i].compare = 0xffffffff;
}

/*
=================
SV_RemoveIP_f
=================
*/
void SV_RemoveIP_f (void)
{
	ipfilter_t	f;
	int			i, j;

	if (!StringToFilter (Cmd_Argv(1), &f))
		return;
	for (i=0 ; i<numipfilters ; i++)
		if (ipfilters[i].mask == f.mask
		&& ipfilters[i].compare == f.compare)
		{
			for (j=i+1 ; j<numipfilters ; j++)
				ipfilters[j-1] = ipfilters[j];
			numipfilters--;
			Con_Printf ("Removed.\n");
			return;
		}
	Con_Printf ("Didn't find %s.\n", Cmd_Argv(1));
}

/*
=================
SV_ListIP_f
=================
*/
void SV_ListIP_f (void)
{
	int		i;
	qbyte	b[4];

	Con_Printf ("Filter list:\n");
	for (i=0 ; i<numipfilters ; i++)
	{
		*(unsigned *)b = ipfilters[i].compare;
		Con_Printf ("%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
	}
}

/*
=================
SV_WriteIP_f
=================
*/
void SV_WriteIP_f (void)
{
	FILE	*f;
	char	name[MAX_OSPATH];
	qbyte	b[4];
	int		i;

	sprintf (name, "%s/listip.cfg", com_gamedir);

	Con_Printf ("Writing %s.\n", name);

	f = fopen (name, "wb");
	if (!f)
	{
		Con_Printf ("Couldn't open %s\n", name);
		return;
	}
	
	for (i=0 ; i<numipfilters ; i++)
	{
		*(unsigned *)b = ipfilters[i].compare;
		fprintf (f, "addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}
	
	fclose (f);
}

/*
=================
SV_SendBan
=================
*/
void SV_SendBan (void)
{
	char		data[128];

	data[0] = data[1] = data[2] = data[3] = 0xff;
	data[4] = A2C_PRINT;
	data[5] = 0;
	strcat (data, "\nbanned.\n");
	
	NET_SendPacket (NS_SERVER, strlen(data), data, net_from);
}

/*
=================
SV_FilterPacket
=================
*/
qboolean SV_FilterPacket (void)
{
	int		i;
	unsigned	in;
	
	in = *(unsigned *)net_from.ip;

	for (i=0 ; i<numipfilters ; i++)
		if ( (in & ipfilters[i].mask) == ipfilters[i].compare)
			return filterban.value;

	return !filterban.value;
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

#ifdef NQPROT
//FIXME: move to header
void SVNQ_ExecuteClientMessage (client_t *cl);
qboolean SVNQ_ReadClientMessages (void)
{
	int		ret;
	int			qport;

	if (host_client->state == cs_free)
		return false;

	if (!host_client->netchan.qsocket)
		return false;

	
	do
	{
		ret = NET_GetMessage (host_client->netchan.qsocket);

		net_from.port = host_client - svs.clients;
		if (ret == -1)
		{
			Sys_Printf ("SV_ReadClientMessage: NET_GetMessage failed\n");
			SV_DropClient(host_client);
			host_client->state = cs_free;
			return false;
		}
		if (!ret)
		{
			if (host_client->state == cs_spawned && !host_client->netchan.qsocket->qwprotocol)
			{
				extern usercmd_t	cmd;
				host_client->send_message = true;
				sv_player = host_client->edict;
				cmd = host_client->lastcmd;
				SV_ClientThink();
				host_client->lastcmd.impulse = cmd.impulse;
			}
			return true;
		}
		
		
					
		MSG_BeginReading ();
				// check for connectionless packet (0xffffffff) first
		if (*(int *)net_message.data == -1)
		{
			if (SV_ConnectionlessPacket (host_client->netchan.qsocket))								
				return true;			
			continue;
		}

//		if (host_client->state == cs_free)
//			continue;

		if (host_client->nqprot)
		{
			host_client->netchan.last_received = realtime;
			SVNQ_ExecuteClientMessage (host_client);
		}
		else
		{
			// read the qport out of the message so we can fix up
			// stupid address translating routers
			MSG_BeginReading ();
			MSG_ReadLong ();		// sequence number
			MSG_ReadLong ();		// sequence number
			qport = MSG_ReadShort () & 0xffff;

			memcpy(&host_client->netchan.remote_address, &net_from, sizeof(net_from));
			if (!NET_CompareBaseAdr (net_from, host_client->netchan.remote_address))
				continue;
	//		if (host_client->netchan.qport != qport)
	//			continue;
			if (host_client->netchan.remote_address.port != net_from.port)
			{
				Con_DPrintf ("SV_ReadPackets: fixing up a translated port\n");
				host_client->netchan.remote_address.port = net_from.port;
			}
			if (Netchan_Process(&host_client->netchan))
			{	// this is a valid, sequenced packet, so process it
				svs.stats.packets++;			
				host_client->send_message = true;	// reply at end of frame
				if (host_client->state != cs_zombie)
					SV_ExecuteClientMessage (host_client);
			}
		}		
	} while (ret);

	if (host_client->state == cs_spawned && !host_client->netchan.qsocket->qwprotocol)
	{
		extern usercmd_t	cmd;
		host_client->send_message = true;
		sv_player = host_client->edict;
		cmd = host_client->lastcmd;
		SV_ClientThink();
	}
	return true;
}
#endif
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
		if (SV_FilterPacket ())
		{
			SV_SendBan ();	// tell them we aren't listening...
			continue;
		}

		// check for connectionless packet (0xffffffff) first
		if (*(int *)net_message.data == -1)
		{
			SV_ConnectionlessPacket
#ifdef NQPROT
				(NULL);
#else
				();
#endif
			continue;
		}
		
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
					if (cl->isq2client)
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
	
		// packet is not from a known client
		//	Con_Printf ("%s:sequenced packet without connection\n"
		// ,NET_AdrToString(net_from));
	}
#ifdef NQPROT
	for (i=0, host_client = svs.clients ; i<MAX_CLIENTS ; i++, host_client++)
		SVNQ_ReadClientMessages();
#endif
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
			if (cl->netchan.last_received < droptime && cl->netchan.remote_address.type != NA_LOOPBACK) {
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
		SV_TogglePause();
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
	if (cl->download)
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

	pr_global_struct->time = sv.time;

	memset (&svs.clients[i].edict->v, 0, pr_edict_size-svprogparms.edictsize);

	svs.clients[i].edict->v.netname = PR_SetString(svprogfuncs, "Console");

	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);			
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PutClientInServer);

	svs.clients[i].edict->v.impulse = atoi(Cmd_Argv(1));

	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPreThink);

	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, svs.clients[i].edict->v.think);
	{
		char buffer[256] = "self.ishuman";
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
		Con_Printf("Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, buffer));
	}
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPostThink);

	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, svs.clients[i].edict);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);
}

/*
==================
SV_Frame

==================
*/
void SV_Frame (float time)
{
	static double	start, end;
	
	start = Sys_DoubleTime ();
	svs.stats.idle += start - end;
	end = start;
	
// keep the random time dependent
	rand ();

// decide the simulation time
	if (!sv.paused) {
#ifndef SERVERONLY
		if (isDedicated)
#endif
			realtime += time;
		sv.time += time;
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

	if (sv.state < ss_active)
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

#ifdef NQPROT
	SVNQ_CheckForNewClients();
#endif

	if (sv.multicast.cursize)
	{
		Con_Printf("Unterminated multicast\n");
		sv.multicast.cursize=0;
	}

	// move autonomous things around if enough time has passed
	if (!sv.paused)
		if (SV_Physics ())
			return;

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
		SV_GetConsoleCommands ();

// process console commands
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
void SV_InitLocal (void)
{
	int		i;
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

	SV_InitOperatorCommands	();
	SV_UserInit ();
	
#ifndef SERVERONLY
	if (isDedicated)
#endif
	{
		Cvar_Register (&developer,	cvargroup_servercontrol);

		Cvar_Register (&password,	cvargroup_servercontrol);
		Cvar_Register (&rcon_password,	cvargroup_servercontrol);
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
	Cvar_Register (&pm_airstep,				cvargroup_serverphysics);

	Cvar_Register (&sv_compatablehulls,		cvargroup_serverphysics);

	for (i = 0; i < sizeof(sv_motd)/sizeof(sv_motd[0]); i++)
		Cvar_Register(&sv_motd[i],	cvargroup_serverinfo);

	Cvar_Register (&sv_aim,	cvargroup_servercontrol);

	Cvar_Register (&sv_resetparms,	cvargroup_servercontrol);

	Cvar_Register (&sv_public,	cvargroup_servercontrol);

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
	Cvar_Register (&allow_download_textures,cvargroup_serverpermissions);
	Cvar_Register (&allow_download_pk3s,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_wads,	cvargroup_serverpermissions);
	Cvar_Register (&allow_download_root,	cvargroup_serverpermissions);
	Cvar_Register (&secure,	cvargroup_serverpermissions);

	Cvar_Register (&sv_highchars,	cvargroup_servercontrol);

	Cvar_Register (&sv_phs,	cvargroup_servercontrol);

	Cvar_Register (&sv_nomsec,	cvargroup_serverphysics);
	Cvar_Register (&pr_allowbutton1, cvargroup_servercontrol);

	Cvar_Register (&pausable,	cvargroup_servercontrol);

	Cvar_Register (&sv_voicechat,	cvargroup_servercontrol);
	Cvar_Register (&sv_maxrate, cvargroup_servercontrol);
	Cvar_Register (&sv_maxdrate, cvargroup_servercontrol);

	Cvar_Register (&sv_nailhack, cvargroup_servercontrol);

	Cmd_AddCommand ("addip", SV_AddIP_f);
	Cmd_AddCommand ("removeip", SV_RemoveIP_f);
	Cmd_AddCommand ("listip", SV_ListIP_f);
	Cmd_AddCommand ("writeip", SV_WriteIP_f);

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
	svs.fteprotocolextensions |= PEXT_SEEF1;
	svs.fteprotocolextensions |= PEXT_SPLITSCREEN;
	svs.fteprotocolextensions |= PEXT_HEXEN2;
	svs.fteprotocolextensions |= PEXT_SPAWNSTATIC2;
	svs.fteprotocolextensions |= PEXT_CUSTOMTEMPEFFECTS;
	svs.fteprotocolextensions |= PEXT_256PACKETENTITIES;
//	svs.fteprotocolextensions |= PEXT_64PLAYERS;
	svs.fteprotocolextensions |= PEXT_SHOWPIC;
	svs.fteprotocolextensions |= PEXT_SETATTACHMENT;

//	if (svs.protocolextensions)
//		Info_SetValueForStarKey (svs.info, "*"DISTRIBUTION"_ext", va("%x", svs.protocolextensions), MAX_SERVERINFO_STRING);

#ifdef VERSION3PART
	Info_SetValueForStarKey (svs.info, "*version", va("%s %4.3f-%i", DISTRIBUTION, VERSION, build_number()), MAX_SERVERINFO_STRING);
#else
	Info_SetValueForStarKey (svs.info, "*version", va("%s %4.2f-%i", DISTRIBUTION, VERSION, build_number()), MAX_SERVERINFO_STRING);
#endif

	Info_SetValueForStarKey (svs.info, "*z_ext", va("%i", SUPPORTED_EXTENSIONS), MAX_SERVERINFO_STRING);

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
}


//============================================================================

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
	int			i;

	if (realtime - svs.last_heartbeat < HEARTBEAT_SECONDS)
		return;		// not time to send yet

	svs.last_heartbeat = realtime;

	//
	// count active users
	//
	active = 0;
	for (i=0 ; i<MAX_CLIENTS ; i++)
		if (svs.clients[i].state == cs_connected ||
		svs.clients[i].state == cs_spawned )
			active++;

	svs.heartbeat_sequence++;
	sprintf (string, "%c\n%i\n%i\n", S2M_HEARTBEAT,
		svs.heartbeat_sequence, active);


	// send to group master
	for (i=0 ; i<MAX_MASTERS ; i++)
		if (master_adr[i].port)
		{
			Con_Printf ("Sending heartbeat to %s\n", NET_AdrToString (master_adr[i]));
			NET_SendPacket (NS_SERVER, strlen(string), string, master_adr[i]);
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
	for (i=0 ; i<MAX_MASTERS ; i++)
		if (master_adr[i].port)
		{
			Con_Printf ("Sending heartbeat to %s\n", NET_AdrToString (master_adr[i]));
			NET_SendPacket (NS_SERVER, strlen(string), string, master_adr[i]);
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

	if (!*out) {	//reached end and it was all whitespace
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
	int newid;
	int j;
	rankstats_t rs;
	newid = Rank_GetPlayerID(newname, atoi(Info_ValueForKey (cl->userinfo, "_pwd")), true);	//'_' keys are always stripped. On any server. So try and use that so persistant data won't give out the password when connecting to a different server
	if (!newid)
		newid = Rank_GetPlayerID(newname, atoi(Info_ValueForKey (cl->userinfo, "password")), true);
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
				rs.parm[j] = (&pr_global_struct->parm1)[j];
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
	Q_strncpyz (cl->team, val, sizeof(cl->team));

	// name for C code
	val = Info_ValueForKey (cl->userinfo, "name");
	val[40] = 0;	//trim to smallish length now (to allow for adding more.

	SV_FixupName(val, newname);

	if (!val[0])
		strcpy(newname, "Hidden");
	else if (!stricmp(val, "console"))
	{
		strcpy(newname, "Not Console");
	}

	// check to see if another user by the same name exists
	while (1) {
		for (i=0, client = svs.clients ; i<MAX_CLIENTS ; i++, client++) {
			if (client->state != cs_spawned || client == cl)
				continue;
			if (!stricmp(client->name, newname))
				break;
		}
		if (i != MAX_CLIENTS) { // dup name
			if (strlen(newname) > sizeof(cl->name) - 1)
				newname[sizeof(cl->name) - 4] = 0;
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
	
	if (strncmp(newname, cl->name, sizeof(cl->name)-1))
	{		
		if (cl->ismuted)
			SV_ClientTPrintf (cl, PRINT_HIGH, STL_NONAMEASMUTE);
		else
		{

			Info_SetValueForKey (cl->userinfo, "name", newname, MAX_INFO_STRING);
			if (!sv.paused) {
				if (!cl->lastnametime || realtime - cl->lastnametime > 5) {
					cl->lastnamecount = 0;
					cl->lastnametime = realtime;
				} else if (cl->lastnamecount++ > 4) {
					SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTKICKEDNAMESPAM, cl->name);
					SV_ClientTPrintf (cl, PRINT_HIGH, STL_YOUWEREKICKEDNAMESPAM);
					SV_DropClient (cl); 
					return;
				}
			}

			if (cl->state >= cs_spawned && !cl->spectator)
			{				
				SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTNAMECHANGE, cl->name, val);
			}
			Q_strncpyz (cl->name, newname, sizeof(cl->name));


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

	Info_SetValueForKey(cl->userinfo, "name", newname, MAX_INFO_STRING);

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
		cl->drate = 2500;

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
	int playercolor;
	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;
	playercolor = top*16 + bottom;
	MSG_WriteByte (&sv.nqreliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.nqreliable_datagram, cl-svs.clients);
	MSG_WriteByte (&sv.nqreliable_datagram, playercolor);
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
#ifndef SERVERONLY
	if (isDedicated)
#endif
	{
		NET_Init ();
		Netchan_Init ();
	}

	// heartbeats will allways be sent to the id master
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
		Sys_Init ();
		PM_Init ();
		Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
		host_hunklevel = Hunk_LowMark ();

		host_initialized = true;
		
		Con_TPrintf (TL_EXEDATETIME, __DATE__, __TIME__);
		Con_TPrintf (TL_HEAPSIZE,parms->memsize/ (1024*1024.0));	

		Con_TPrintf (TL_SERVERVERSION, DISTRIBUTION, VERSION, build_number());

		Con_TPrintf (STL_INITED);

		Cbuf_InsertText ("exec server.cfg\nexec ftesrv.cfg\n", RESTRICT_LOCAL);	
		
	// process command line arguments
		Cbuf_AddText("stuffcmds", RESTRICT_LOCAL);
		Cbuf_Execute ();

	// if a map wasn't specified on the command line, spawn start.map
		if (sv.state == ss_dead)
			Cmd_ExecuteString ("map start", RESTRICT_LOCAL);
		if (sv.state == ss_dead)
			SV_Error ("Couldn't spawn a server");

	}
}

#endif
