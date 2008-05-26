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
// sv_user.c -- server code for moving users

#include "qwsvdef.h"

#ifndef CLIENTONLY

#include <ctype.h>
#define Q2EDICT_NUM(i) (q2edict_t*)((char *)ge->edicts+i*ge->edict_size)
#define Q2NUM_FOR_EDICT(ent) (((char *)ent - (char *)ge->edicts) / ge->edict_size)
hull_t *SV_HullForEntity (edict_t *ent, int hullnum, vec3_t mins, vec3_t maxs, vec3_t offset);

edict_t	*sv_player;

usercmd_t	cmd;

#ifdef SERVERONLY
cvar_t	cl_rollspeed = SCVAR("cl_rollspeed", "200");
cvar_t	cl_rollangle = SCVAR("cl_rollangle", "2.0");
#else
extern cvar_t	cl_rollspeed;
extern cvar_t	cl_rollangle;
#endif
cvar_t	sv_spectalk = SCVAR("sv_spectalk", "1");

cvar_t	sv_mapcheck	= SCVAR("sv_mapcheck", "1");

cvar_t	sv_cheatpc = SCVAR("sv_cheatpc", "125");
cvar_t	sv_cheatspeedchecktime = SCVAR("sv_cheatspeedchecktime", "30");
cvar_t	sv_playermodelchecks = SCVAR("sv_playermodelchecks", "1");

cvar_t	sv_cmdlikercon = SCVAR("sv_cmdlikercon", "0");	//set to 1 to allow a password of username:password instead of the correct rcon password.
cvar_t cmd_allowaccess = SCVAR("cmd_allowaccess", "0");	//set to 1 to allow cmd to execute console commands on the server.
cvar_t cmd_gamecodelevel = SCVAR("cmd_gamecodelevel", "50");	//execution level which gamecode is told about (for unrecognised commands)

cvar_t	sv_nomsec = SCVAR("sv_nomsec", "0");
cvar_t	sv_edgefriction = SCVAR("sv_edgefriction", "2");

cvar_t	sv_brokenmovetypes = SCVAR("sv_brokenmovetypes", "0");

cvar_t	sv_chatfilter = SCVAR("sv_chatfilter", "0");

cvar_t	sv_floodprotect = SCVAR("sv_floodprotect", "1");
cvar_t	sv_floodprotect_messages = SCVAR("sv_floodprotect_messages", "4");
cvar_t	sv_floodprotect_interval = SCVAR("sv_floodprotect_interval", "4");
cvar_t  sv_floodprotect_silencetime = SCVAR("sv_floodprotect_silencetime", "10");
cvar_t	sv_floodprotect_suicide = SCVAR("sv_floodprotect_suicide", "1");
cvar_t	sv_floodprotect_sendmessage = FCVAR("sv_floodprotect_sendmessage", "floodprotmsg", "", 0);

cvar_t	votelevel	= SCVAR("votelevel", "0");
cvar_t	voteminimum	= SCVAR("voteminimum", "4");
cvar_t	votepercent = SCVAR("votepercent", "-1");
cvar_t	votetime = SCVAR("votetime", "10");

cvar_t	pr_allowbutton1 = SCVARF("pr_allowbutton1", "1", CVAR_LATCH);
extern cvar_t sv_minping;


extern cvar_t	pm_bunnyspeedcap;
extern cvar_t	pm_ktjump;
extern cvar_t	pm_slidefix;
extern cvar_t	pm_slidyslopes;
extern cvar_t	pm_airstep;
extern cvar_t	pm_walljump;
cvar_t sv_pushplayers = SCVAR("sv_pushplayers", "0");

//yes, realip cvars need to be fully initialised or realip will be disabled
cvar_t sv_getrealip = SCVAR("sv_getrealip", "0");
cvar_t sv_realiphostname_ipv4 = SCVAR("sv_realiphostname_ipv4", "");
cvar_t sv_realiphostname_ipv6 = SCVAR("sv_realiphostname_ipv6", "");
cvar_t sv_realip_timeout = SCVAR("sv_realip_timeout", "10");

char sv_votinggroup[] = "server voting";


extern char cvargroup_serverpermissions[];
extern char cvargroup_serverinfo[];
extern char cvargroup_serverphysics[];
extern char cvargroup_servercontrol[];

extern	vec3_t	player_mins, player_maxs;

extern cvar_t pausable;


void SV_PreRunCmd(void);
void SV_RunCmd (usercmd_t *ucmd, qboolean recurse);
void SV_PostRunCmd(void);

/*
============================================================

USER STRINGCMD EXECUTION

host_client and sv_player will be valid.
============================================================
*/

qboolean SV_CheckRealIP(client_t *client, qboolean force)
{
	//returns true if they have a real ip
	char *serverip;
	char *msg;

	if (!sv_getrealip.value)
		return true;

	if (client->netchan.remote_address.type == NA_LOOPBACK)
		return true;	//the loopback client doesn't have to pass realip checks

	if (client->realip_status == 3)
		return true;	//we know that the ip is authentic
	if (client->realip_status == 2)
	{
		ClientReliableWrite_Begin(client, svc_print, 256);
		ClientReliableWrite_Byte(client, PRINT_HIGH);
		ClientReliableWrite_String(client, "Couldn't verify your real ip\n");
		return true;	//client doesn't support certainty.
	}
	if (client->realip_status == -1)
		return true;	//can't get a better answer

	if (realtime - host_client->connection_started > sv_realip_timeout.value)
	{
		client->realip_status = -1;
		ClientReliableWrite_Begin(client, svc_print, 256);
		ClientReliableWrite_Byte(client, PRINT_HIGH);
		ClientReliableWrite_String(client, "Couldn't determine your real ip\n");
		if (sv_getrealip.value == 2)
		{
			SV_DropClient(client);
			return false;
		}
		return true;
	}


	if (client->realip_status == 1)
	{
		msg = va("\xff\xff\xff\xff%c %i", A2A_PING, client->realip_ping);
		NET_SendPacket(NS_SERVER, strlen(msg), msg, client->realip);
	}
	else
	{
		if (client->netchan.remote_address.type == NA_IPV6)
		{
			serverip = sv_realiphostname_ipv6.string;
//			serverip = NET_AdrToString (net_local_sv_ip6adr);
		}
		else
		{
			serverip = sv_realiphostname_ipv4.string;
//			serverip = NET_AdrToString (net_local_sv_ipadr);
		}

		if (!*serverip)
		{
			Con_Printf("realip not fully configured\n");
			client->realip_status = -2;
			return true;
		}

		ClientReliableWrite_Begin(client, svc_stufftext, 256);
		ClientReliableWrite_String(client, va("packet %s \"realip %i %i\"\n", serverip, client-svs.clients, client->realip_num));
	}
	return false;
}

/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_New_f (void)
{
	char		*gamedir;
	int			playernum;
	int splitnum;
	client_t *split;

	if (host_client->state == cs_spawned)
		return;

/*	splitt delay
	host_client->state = cs_connected;
	host_client->connection_started = realtime;
#ifdef SVRANKING
	host_client->stats_started = realtime;
#endif*/

	// send the info about the new client to all connected clients
//	SV_FullClientUpdate (host_client, &sv.reliable_datagram);
//	host_client->sendinfo = true;

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0] || !strcmp(gamedir, "fte"))
		gamedir = "qw";

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
/*	if (host_client->num_backbuf)
	{
		Con_TPrintf(STL_BACKBUFSET, host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear(&host_client->netchan.message);
	}
*/
	if (sizeofcoord > 2 && !(host_client->fteprotocolextensions & PEXT_FLOATCOORDS))
	{
		SV_ClientPrintf(host_client, 2, "\n\n\n\nSorry, but your client does not appear to support FTE's bigcoords\nFTE users will need to set cl_nopext to 0 and then reconnect, or to upgrade\n");
		Con_Printf("%s does not support bigcoords\n", host_client->name);
		return;
	}

	ClientReliableCheckBlock(host_client, 800);	//okay, so it might be longer, but I'm too lazy to work out the real size.

	// send the serverdata
	ClientReliableWrite_Byte (host_client, ISQ2CLIENT(host_client)?svcq2_serverdata:svc_serverdata);
#ifdef PROTOCOL_VERSION_FTE
	if (host_client->fteprotocolextensions)//let the client know
	{
		MSG_WriteLong (&host_client->netchan.message, PROTOCOL_VERSION_FTE);
		if (sizeofcoord == 2)	//we're not using float orgs on this level.
			MSG_WriteLong (&host_client->netchan.message, host_client->fteprotocolextensions&~PEXT_FLOATCOORDS);
		else
			MSG_WriteLong (&host_client->netchan.message, host_client->fteprotocolextensions);
	}
#endif
	ClientReliableWrite_Long (host_client, ISQ2CLIENT(host_client)?PROTOCOL_VERSION_Q2:PROTOCOL_VERSION_QW);
	ClientReliableWrite_Long (host_client, svs.spawncount);
	if (ISQ2CLIENT(host_client))
		ClientReliableWrite_Byte (host_client, 0);
	ClientReliableWrite_String (host_client, gamedir);

	splitnum = 0;
	for (split = host_client; split; split = split->controlled)
	{
#ifdef Q2SERVER
		if (!svprogfuncs)
			playernum = Q2NUM_FOR_EDICT(split->q2edict)-1;
		else
#endif
			playernum = NUM_FOR_EDICT(svprogfuncs, split->edict)-1;
		if (sv.demostate)
		{
			playernum = (MAX_CLIENTS-1-splitnum)|128;
		}
		else if (split->spectator)
			playernum |= 128;

		if (sv.state == ss_cinematic)
			playernum = -1;

		if (ISQ2CLIENT(host_client))
			ClientReliableWrite_Short (host_client, playernum);
		else
			ClientReliableWrite_Byte (host_client, playernum);

		split->state = cs_connected;
		split->connection_started = realtime;
	#ifdef SVRANKING
		split->stats_started = realtime;
	#endif
		splitnum++;
	}

	if (host_client->fteprotocolextensions & PEXT_SPLITSCREEN)
		ClientReliableWrite_Byte (host_client, 128);

	// send full levelname
	if (sv.demostatevalid)
		ClientReliableWrite_String (host_client, sv.demfullmapname);
	else
		ClientReliableWrite_String (host_client, sv.mapname);

	//
	// game server
	//
#ifdef Q2SERVER
	if (ISQ2CLIENT(host_client))
	{
		if (sv.state != ss_cinematic)
		{
//			host_client->q2edict = Q2NUM_FOR_EDICT(split->q2edict)-1;
			memset (&host_client->lastcmd, 0, sizeof(host_client->lastcmd));

			// begin fetching configstrings
			ClientReliableWrite_Byte (host_client, svcq2_stufftext);
			ClientReliableWrite_String (host_client, va("cmd configstrings %i 0\n",svs.spawncount) );
		}
		return;
	}
#endif
	// send the movevars
	ClientReliableWrite_Float(host_client, movevars.gravity);
	ClientReliableWrite_Float(host_client, movevars.stopspeed);
	ClientReliableWrite_Float(host_client, movevars.maxspeed);
	ClientReliableWrite_Float(host_client, movevars.spectatormaxspeed);
	ClientReliableWrite_Float(host_client, movevars.accelerate);
	ClientReliableWrite_Float(host_client, movevars.airaccelerate);
	ClientReliableWrite_Float(host_client, movevars.wateraccelerate);
	ClientReliableWrite_Float(host_client, movevars.friction);
	ClientReliableWrite_Float(host_client, movevars.waterfriction);
	ClientReliableWrite_Float(host_client, movevars.entgravity);

	// send server info string
	if (sv.demostatevalid)
	{
		ClientReliableCheckBlock(host_client, 20 + strlen(sv.demoinfo));
		ClientReliableWrite_Byte (host_client, svc_stufftext);
		ClientReliableWrite_String (host_client, va("fullserverinfo \"%s\"\n", sv.demoinfo) );
	}
	else
	{
		ClientReliableCheckBlock(host_client, 20 + strlen(svs.info));
		ClientReliableWrite_Byte (host_client, svc_stufftext);
		ClientReliableWrite_String (host_client, va("fullserverinfo \"%s\"\n", svs.info) );
	}

	host_client->csqcactive = false;

	host_client->realip_num = rand()+(host_client->challenge<<16);
	SV_CheckRealIP(host_client, false);

	// send music
	ClientReliableCheckBlock(host_client, 2);

	ClientReliableWrite_Byte (host_client, svc_cdtrack);
	if (svprogfuncs)
		ClientReliableWrite_Byte (host_client, sv.edicts->v->sounds);
	else
		ClientReliableWrite_Byte (host_client, 0);

	SV_LogPlayer(host_client, "new (QW)");
}
#define GAME_DEATHMATCH 0
#define GAME_COOP 1
void SVNQ_New_f (void)
{
	extern cvar_t coop;
	char			message[2048];
	int i;

	MSG_WriteByte (&host_client->netchan.message, svc_print);
#ifdef DISTRIBUTION
	sprintf (message, "%c\n" DISTRIBUTION " QuakeWorld build %i server\n", 2, build_number());
#else
	sprintf (message, "%c\nQUAKEWORLD BUILD %i SERVER\n", 2, build_number());
#endif
	MSG_WriteString (&host_client->netchan.message,message);

	if (host_client->protocol == SCP_DARKPLACES7)
	{
		extern cvar_t allow_download;
		char *f;

		if (allow_download.value)
		{
			MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
			MSG_WriteString (&host_client->netchan.message, "cl_serverextension_download 1\n");
		}

		f = COM_LoadTempFile("csprogs.dat");
		if (f)
		{
			MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
			MSG_WriteString (&host_client->netchan.message, va("csqc_progname %s\n", "csprogs.dat"));
			MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
			MSG_WriteString (&host_client->netchan.message, va("csqc_progsize %i\n", com_filesize));
			MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
			MSG_WriteString (&host_client->netchan.message, va("csqc_progcrc %i\n", QCRC_Block(f, com_filesize)));

			host_client->csqcactive = true;
		}
	}

	MSG_WriteByte (&host_client->netchan.message, svc_serverdata);
	switch(host_client->protocol)
	{
#ifdef NQPROT
	case SCP_NETQUAKE:
		SV_LogPlayer(host_client, "new (NQ)");
		MSG_WriteLong (&host_client->netchan.message, NQ_PROTOCOL_VERSION);
		MSG_WriteByte (&host_client->netchan.message, 16);
		break;
	case SCP_DARKPLACES6:
		SV_LogPlayer(host_client, "new (DP6)");
		MSG_WriteLong (&host_client->netchan.message, DP6_PROTOCOL_VERSION);
		MSG_WriteByte (&host_client->netchan.message, sv.allocated_client_slots);
		break;
	case SCP_DARKPLACES7:
		SV_LogPlayer(host_client, "new (DP7)");
		MSG_WriteLong (&host_client->netchan.message, DP7_PROTOCOL_VERSION);
		MSG_WriteByte (&host_client->netchan.message, sv.allocated_client_slots);
		break;
#endif
	default:
		host_client->drop = true;
		break;
	}

	if (!coop.value && deathmatch.value)
		MSG_WriteByte (&host_client->netchan.message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&host_client->netchan.message, GAME_COOP);

	strcpy (message, sv.mapname);

	MSG_WriteString (&host_client->netchan.message,message);

	for (i = 1; sv.strings.model_precache[i] ; i++)
		MSG_WriteString (&host_client->netchan.message, sv.strings.model_precache[i]);
	MSG_WriteByte (&host_client->netchan.message, 0);

	for (i = 1; *sv.strings.sound_precache[i] ; i++)
		MSG_WriteString (&host_client->netchan.message, sv.strings.sound_precache[i]);
	MSG_WriteByte (&host_client->netchan.message, 0);

// send music
	MSG_WriteByte (&host_client->netchan.message, svc_cdtrack);
	MSG_WriteByte (&host_client->netchan.message, sv.edicts->v->sounds);
	MSG_WriteByte (&host_client->netchan.message, sv.edicts->v->sounds);

// set view
	MSG_WriteByte (&host_client->netchan.message, svc_setview);
	MSG_WriteShort (&host_client->netchan.message, NUM_FOR_EDICT(svprogfuncs, host_client->edict));

	MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
	MSG_WriteByte (&host_client->netchan.message, 1);

	MSG_WriteByte (&host_client->netchan.message, svc_setpause);
	MSG_WriteByte (&host_client->netchan.message, sv.paused);

//	host_client->sendsignon = true;
//	host_client->spawned = false;		// need prespawn, spawn, etc
}





#ifdef Q2SERVER
void SVQ2_ConfigStrings_f (void)
{
	int			start;
	char *str;

	Con_DPrintf ("Configstrings() from %s\n", host_client->name);

	if (host_client->state != cs_connected)
	{
		Con_DPrintf ("configstrings not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Con_DPrintf ("SV_Configstrings_f from different level\n");
		SV_New_f ();
		return;
	}

	start = atoi(Cmd_Argv(2));

	if (start < 0)
	{
		Con_Printf ("SV_Configstrings_f: %s tried crashing us\n", host_client->name);
		host_client->drop = true;
		return;
	}

	// write a packet full of data

	while ( host_client->netchan.message.cursize < MAX_QWMSGLEN/2
		&& start < Q2MAX_CONFIGSTRINGS)
	{
		str = sv.strings.configstring[start];
		if (*str)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, str);
		}
			/*
		//choose range to grab from.
		if (start < Q2CS_CDTRACK)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, sv.name);
		}
		else if (start < Q2CS_SKY)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			if (svprogfuncs)
				MSG_WriteString (&host_client->netchan.message, va("%i", sv.edicts->v->sounds));
			else
				MSG_WriteString (&host_client->netchan.message, "0");
		}
		else if (start < Q2CS_SKYAXIS)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, "unit1_");
		}
		else if (start < Q2CS_SKYROTATE)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, "0");
		}
		else if (start < Q2CS_STATUSBAR)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, "0");
		}
		else if (start < Q2CS_AIRACCEL)
		{//show status bar
			if (start == Q2CS_STATUSBAR)
			{
				MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
				MSG_WriteShort (&host_client->netchan.message, start);
				MSG_WriteString (&host_client->netchan.message, sv.statusbar);
			}
		}
		else if (start < Q2CS_MAXCLIENTS)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, va("%i", (int)1));
		}
		else if (start < Q2CS_MAPCHECKSUM)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, va("%i", (int)32));
		}
		else if (start < Q2CS_MODELS)
		{
			extern int map_checksum;
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, va("%i", map_checksum));
		}
		else if (start < Q2CS_SOUNDS)
		{
			if (*sv.model_precache[start-Q2CS_MODELS])
			{
				MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
				MSG_WriteShort (&host_client->netchan.message, start);
				MSG_WriteString (&host_client->netchan.message, sv.model_precache[start-Q2CS_MODELS]);
			}
		}
		else if (start < Q2CS_IMAGES)
		{
			if (*sv.sound_precache[start-Q2CS_SOUNDS])
			{
				MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
				MSG_WriteShort (&host_client->netchan.message, start);
				MSG_WriteString (&host_client->netchan.message, sv.sound_precache[start-Q2CS_SOUNDS]);
			}
		}
		else if (start < Q2CS_LIGHTS)
		{
			if (*sv.image_precache[start-Q2CS_IMAGES])
			{
				MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
				MSG_WriteShort (&host_client->netchan.message, start);
				MSG_WriteString (&host_client->netchan.message, sv.image_precache[start-Q2CS_IMAGES]);
			}
		}
		else if (start < Q2CS_ITEMS)
		{
			if (start-Q2CS_LIGHTS < MAX_LIGHTSTYLES && sv.lightstyles[start-Q2CS_LIGHTS])
			{
				MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
				MSG_WriteShort (&host_client->netchan.message, start);
				MSG_WriteString (&host_client->netchan.message, sv.lightstyles[start-Q2CS_LIGHTS]);
			}
		}
		else if (start < Q2CS_PLAYERSKINS)
		{
//			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
//			MSG_WriteShort (&host_client->netchan.message, start);
//			MSG_WriteString (&host_client->netchan.message, sv.configstrings[start-Q2CS_ITEMS]);
		}
		else if (start < Q2CS_GENERAL)
		{
//			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
//			MSG_WriteShort (&host_client->netchan.message, start);
//			MSG_WriteString (&host_client->netchan.message, sv.configstrings[start]);
		}
		else
		{
//			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
//			MSG_WriteShort (&host_client->netchan.message, start);
//			MSG_WriteString (&host_client->netchan.message, sv.configstrings[start]);
		}
		*/
		start++;
	}

	// send next command

	if (start == Q2MAX_CONFIGSTRINGS)
	{
		MSG_WriteByte (&host_client->netchan.message, svcq2_stufftext);
		MSG_WriteString (&host_client->netchan.message, va("cmd baselines %i 0\n",svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&host_client->netchan.message, svcq2_stufftext);
		MSG_WriteString (&host_client->netchan.message, va("cmd configstrings %i %i\n",svs.spawncount, start) );
	}
}
#endif

#ifdef Q2SERVER
void SVQ2_BaseLines_f (void)
{
	int		start;
	q2entity_state_t	nullstate;
	q2entity_state_t	*base;

	extern q2entity_state_t	sv_baselines[Q2MAX_EDICTS];

	Con_DPrintf ("Baselines() from %s\n", host_client->name);

	if (host_client->state != cs_connected)
	{
		Con_Printf ("baselines not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Con_Printf ("SV_Baselines_f from different level\n");
		SV_New_f ();
		return;
	}

	start = atoi(Cmd_Argv(2));

	memset (&nullstate, 0, sizeof(nullstate));

	// write a packet full of data

	while ( host_client->netchan.message.cursize <  MAX_QWMSGLEN/2
		&& start < Q2MAX_EDICTS)
	{
		base = &sv_baselines[start];
		if (base->modelindex || base->sound || base->effects)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_spawnbaseline);
			MSGQ2_WriteDeltaEntity (&nullstate, base, &host_client->netchan.message, true, true);
		}
		start++;
	}

	// send next command

	if (start == Q2MAX_EDICTS)
	{
		MSG_WriteByte (&host_client->netchan.message, svcq2_stufftext);
		MSG_WriteString (&host_client->netchan.message, va("precache %i\n", svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&host_client->netchan.message, svcq2_stufftext);
		MSG_WriteString (&host_client->netchan.message, va("cmd baselines %i %i\n",svs.spawncount, start) );
	}
}

void SVQ2_NextServer_f (void)
{
	if (!*sv.modelname && atoi(Cmd_Argv(1)) == svs.spawncount)
	{
		cvar_t *nsv = Cvar_FindVar("nextserver");
		if (!nsv || !*nsv->string)
			return;

		svs.spawncount++;	// make sure another doesn't sneak in

		Cbuf_AddText(nsv->string, RESTRICT_LOCAL);
		Cbuf_AddText("\n", RESTRICT_LOCAL);
		Cvar_Set(nsv, "");
	}
}
#endif

void SV_PK3List_f (void)
{
#ifndef PEXT_PK3DOWNLOADS
	Con_Printf ("pk3list not valid -- It's not implemented!\n");
	return;
#else
	int crc;
	char *name;
	int i;


	if (host_client->state != cs_connected)
	{	//fixme: send prints instead
		Con_Printf ("pk3list not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount && !sv.msgfromdemo)
	{
		Con_Printf ("SV_PK3List_f from different level\n");
		SV_New_f ();
		return;
	}

	i = atoi(Cmd_Argv(2));

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf)
	{
		Con_Printf("WARNING %s: [SV_Soundlist] Back buffered (%d), clearing", host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear(&host_client->netchan.message);
	}
	if (i < 0)
	{
		Con_Printf ("SV_PK3List_f: %s tried to crash us\n", host_client->name);
		SV_DropClient(host_client);
		return;
	}

	for (; ; i++)
	{
		if (host_client->netchan.message.cursize < (MAX_QWMSGLEN/2))
		{	//user's buffer was too small
			MSG_WriteByte(&host_client->netchan.message, svc_stufftext);
			MSG_WriteString(&host_client->netchan.message, va("cmd pk3list %i %i\n", svs.spawncount, 0));
			return;	//and stop before we flood them
		}

		name = COM_GetPathInfo(i, &crc);

		if (name && *name)
		{
			MSG_WriteByte(&host_client->netchan.message, svc_stufftext);
			MSG_WriteString(&host_client->netchan.message, va("echo packfile %s\n", name));
			continue;	//try the next.
		}
		//that's all folks, move on to sound.
		MSG_WriteByte(&host_client->netchan.message, svc_stufftext);
		MSG_WriteString(&host_client->netchan.message, va("soundlist %i 0\n", svs.spawncount));
		return;
	}
#endif
}

/*
==================
SV_Soundlist_f
==================
*/
void SV_Soundlist_f (void)
{
	int i;
	//char		**s;
	unsigned n;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("soundlist not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount && !sv.msgfromdemo)
	{
		Con_Printf ("SV_Soundlist_f from different level\n");
		SV_New_f ();
		return;
	}

	n = atoi(Cmd_Argv(2));

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Con_Printf("WARNING %s: [SV_Soundlist] Back buffered (%d), clearing", host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear(&host_client->netchan.message);
	}

	if (n >= MAX_SOUNDS)
	{
		SV_EndRedirect();
		Con_Printf ("SV_Soundlist_f: %s send an invalid index\n", host_client->name);
		SV_DropClient(host_client);
		return;
	}

	MSG_WriteByte (&host_client->netchan.message, svc_soundlist);
	MSG_WriteByte (&host_client->netchan.message, n);
	if (sv.democausesreconnect)	//read the list from somewhere else
	{
		for (i = 1+n;
			*sv.demsound_precache[i] && host_client->netchan.message.cursize < (MAX_QWMSGLEN/2);
			i++, n++)
			MSG_WriteString (&host_client->netchan.message, sv.demsound_precache[i]);


		if (!*sv.demsound_precache[i])
			n = 0;
	}
	else
	{
		for (i = 1+n;
			*sv.strings.sound_precache[i] && host_client->netchan.message.cursize < (MAX_QWMSGLEN/2);
			i++, n++)
			MSG_WriteString (&host_client->netchan.message, sv.strings.sound_precache[i]);

		if (!*sv.strings.sound_precache[i])
			n = 0;
	}
	MSG_WriteByte (&host_client->netchan.message, 0);

	// next msg
	MSG_WriteByte (&host_client->netchan.message, n);
}

/*
==================
SV_Modellist_f
==================
*/
void SV_Modellist_f (void)
{
	int i;
	unsigned n;
	int maxclientsupportedmodels;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("modellist not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount && !sv.msgfromdemo)
	{
		Con_Printf ("SV_Modellist_f from different level\n");
		SV_New_f ();
		return;
	}

	n = atoi(Cmd_Argv(2));

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Con_Printf("WARNING %s: [SV_Modellist] Back buffered (%d), clearing", host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear(&host_client->netchan.message);
	}

	if (n >= MAX_MODELS)
	{
		SV_EndRedirect();
		Con_Printf ("SV_Modellist_f: %s send an invalid index\n", host_client->name);
		SV_DropClient(host_client);
		return;
	}

	if (n >= 255)
	{
		MSG_WriteByte (&host_client->netchan.message, svcfte_modellistshort);
		MSG_WriteShort (&host_client->netchan.message, n);
	}
	else
	{
		MSG_WriteByte (&host_client->netchan.message, svc_modellist);
		MSG_WriteByte (&host_client->netchan.message, n);
	}

	maxclientsupportedmodels = 256;
	if (host_client->fteprotocolextensions & PEXT_MODELDBL)
		maxclientsupportedmodels *= 2;

	if (sv.democausesreconnect)	//read the list from somewhere else
	{
		for (i = 1+n;
			*sv.demmodel_precache[i] && ((n&255)==0||host_client->netchan.message.cursize < (MAX_QWMSGLEN/2));
			i++, n++)
			MSG_WriteString (&host_client->netchan.message, sv.demmodel_precache[i]);

		if (!*sv.demmodel_precache[i])
			n = 0;
	}
	else
	{
		for (i = 1+n;
			i < maxclientsupportedmodels && sv.strings.model_precache[i] && host_client->netchan.message.cursize < (MAX_QWMSGLEN/2);	//make sure we don't send a 0 next...
			i++)
		{
			MSG_WriteString (&host_client->netchan.message, sv.strings.model_precache[i]);
			if (((n&255)==255) && n != i-1)
				break;
		}
		n = i-1;

		if (!sv.strings.model_precache[i])
			n = 0;
	}

	if (i == maxclientsupportedmodels)
		n = 0;	//doh!

	MSG_WriteByte (&host_client->netchan.message, 0);

	// next msg
	MSG_WriteByte (&host_client->netchan.message, n);
}

/*
==================
SV_PreSpawn_f
==================
*/
void SV_PreSpawn_f (void)
{
	unsigned	buf, bufs;
	unsigned	check;

	unsigned statics;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount && !sv.msgfromdemo)
	{
		Con_Printf ("SV_PreSpawn_f from different level\n");
		SV_New_f ();
		return;
	}

	if (sv.democausesreconnect)
		bufs = sv.num_demosignon_buffers;
	else
		bufs = sv.num_signon_buffers;
	statics = sv.numextrastatics;
	buf = atoi(Cmd_Argv(2));

	if (buf >= bufs+statics+sv.num_edicts+255)
	{
		SV_EndRedirect();
		Con_Printf ("SV_Modellist_f: %s send an invalid index\n", host_client->name);
		SV_DropClient(host_client);
		return;
	}

	if (!buf)
	{
		// should be three numbers following containing checksums
		check = atoi(Cmd_Argv(3));

//		Con_DPrintf("Client check = %d\n", check);

		if (sv_mapcheck.value && check != sv.worldmodel->checksum &&
			check != LittleLong(sv.worldmodel->checksum2))
		if (!sv.demofile || (sv.demofile && !sv.democausesreconnect))	//demo playing causes no check. If it's the return level, check anyway to avoid that loophole.
		{
			SV_ClientTPrintf (host_client, PRINT_HIGH,
				STL_MAPCHEAT,
				sv.modelname, check, sv.worldmodel->checksum, sv.worldmodel->checksum2);
			SV_DropClient (host_client);
			return;
		}
		host_client->checksum = check;
	}

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Con_Printf("WARNING %s: [SV_PreSpawn] Back buffered (%d), clearing", host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear(&host_client->netchan.message);
	}

	if (buf >= bufs && !sv.democausesreconnect)
	{
		int i;
		entity_state_t from;
		entity_state_t *state;
		edict_t *ent;
		svcustomtents_t *ctent;


		memset(&from, 0, sizeof(from));
		while (host_client->netchan.message.cursize < (host_client->netchan.message.maxsize/2))	//static entities
		{
			if (buf - bufs >= sv.numextrastatics)
				break;

			state = &sv.extendedstatics[buf - bufs];

			if (host_client->fteprotocolextensions & PEXT_SPAWNSTATIC2)
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnstatic2);
				SV_WriteDelta(&from, state, &host_client->netchan.message, true, host_client->fteprotocolextensions);
			}
			else if (state->modelindex < 256)
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnstatic);

				MSG_WriteByte (&host_client->netchan.message, state->modelindex&255);

				MSG_WriteByte (&host_client->netchan.message, state->frame);
				MSG_WriteByte (&host_client->netchan.message, (int)state->colormap);
				MSG_WriteByte (&host_client->netchan.message, (int)state->skinnum);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&host_client->netchan.message, state->origin[i]);
					MSG_WriteAngle(&host_client->netchan.message, state->angles[i]);
				}
			}
			buf++;
		}
		while (host_client->netchan.message.cursize < (host_client->netchan.message.maxsize/2))	//baselines
		{
			if (buf - bufs - sv.numextrastatics >= sv.num_edicts)
				break;

			ent = EDICT_NUM(svprogfuncs, buf - bufs - sv.numextrastatics);

			state = &ent->baseline;
			if (!state->number || !state->modelindex)
			{	//ent doesn't have a baseline
				buf++;
				continue;
			}

			if (!ent)
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline);

				MSG_WriteShort (&host_client->netchan.message, buf - bufs - sv.numextrastatics);

				MSG_WriteByte (&host_client->netchan.message, 0);

				MSG_WriteByte (&host_client->netchan.message, 0);
				MSG_WriteByte (&host_client->netchan.message, 0);
				MSG_WriteByte (&host_client->netchan.message, 0);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&host_client->netchan.message, 0);
					MSG_WriteAngle(&host_client->netchan.message, 0);
				}
			}
			else if (host_client->fteprotocolextensions & PEXT_SPAWNSTATIC2)
			{
				MSG_WriteByte(&host_client->netchan.message, svcfte_spawnbaseline2);
				SV_WriteDelta(&from, state, &host_client->netchan.message, true, host_client->fteprotocolextensions);
			}
			else if (state->modelindex < 256)
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline);

				MSG_WriteShort (&host_client->netchan.message, buf - bufs - sv.numextrastatics);

				MSG_WriteByte (&host_client->netchan.message, state->modelindex);

				MSG_WriteByte (&host_client->netchan.message, state->frame);
				MSG_WriteByte (&host_client->netchan.message, (int)state->colormap);
				MSG_WriteByte (&host_client->netchan.message, (int)state->skinnum);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&host_client->netchan.message, state->origin[i]);
					MSG_WriteAngle(&host_client->netchan.message, state->angles[i]);
				}
			}

			buf++;
		}
		while (host_client->netchan.message.cursize < (host_client->netchan.message.maxsize/2))
		{
			i = buf - bufs - sv.numextrastatics - sv.num_edicts;
			if (i >= 255)
				break;

			ctent = &sv.customtents[i];

			buf++;
			if (!*ctent->particleeffecttype)
			{	//effect isn't registered.
				continue;
			}

			if (host_client->fteprotocolextensions & PEXT_CUSTOMTEMPEFFECTS)
			{
				MSG_WriteByte(&host_client->netchan.message, svcfte_customtempent);
				MSG_WriteByte(&host_client->netchan.message, 255);
				MSG_WriteByte(&host_client->netchan.message, i);
				MSG_WriteByte(&host_client->netchan.message, ctent->netstyle);
				MSG_WriteString(&host_client->netchan.message, ctent->particleeffecttype);
				if (ctent->netstyle & CTE_STAINS)
				{
					MSG_WriteChar(&host_client->netchan.message, ctent->stain[0]);
					MSG_WriteChar(&host_client->netchan.message, ctent->stain[0]);
					MSG_WriteChar(&host_client->netchan.message, ctent->stain[0]);
					MSG_WriteByte(&host_client->netchan.message, ctent->radius);
				}
				if (ctent->netstyle & CTE_GLOWS)
				{
					MSG_WriteByte(&host_client->netchan.message, ctent->dlightrgb[0]);
					MSG_WriteByte(&host_client->netchan.message, ctent->dlightrgb[1]);
					MSG_WriteByte(&host_client->netchan.message, ctent->dlightrgb[2]);
					MSG_WriteByte(&host_client->netchan.message, ctent->dlightradius);
					MSG_WriteByte(&host_client->netchan.message, ctent->dlighttime);
				}
			}
		}
	}
	else if (buf >= bufs)
	{
		buf = bufs+sv.numextrastatics+sv.num_edicts+255;
	}
	else
	{
		if (sv.democausesreconnect)
		{
			if (host_client->netchan.message.cursize+sv.signon_buffer_size[buf]+30 < host_client->netchan.message.maxsize)
			{
				SZ_Write (&host_client->netchan.message,
					sv.demosignon_buffers[buf],
					sv.demosignon_buffer_size[buf]);
				buf++;
			}
		}
		else
		{
			if (host_client->netchan.message.cursize+sv.signon_buffer_size[buf]+30 < host_client->netchan.message.maxsize)
			{
				SZ_Write (&host_client->netchan.message,
					sv.signon_buffers[buf],
					sv.signon_buffer_size[buf]);
					buf++;
			}
		}
	}
	if (buf == bufs+sv.numextrastatics+sv.num_edicts+255)
	{	// all done prespawning
		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message, va("cmd spawn %i\n",svs.spawncount) );
	}
	else
	{	// need to prespawn more
		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message,
			va("cmd prespawn %i %i\n", svs.spawncount, buf) );
	}
}

/*
==================
SV_Spawn_f
==================
*/
void SV_Spawn_f (void)
{
	int		i;
	client_t	*client, *split;
	edict_t	*ent;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("Spawn not valid -- already spawned\n");
		return;
	}

// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount && !sv.msgfromdemo)
	{
		Con_Printf ("SV_Spawn_f from different level\n");
		SV_New_f ();
		return;
	}

// send all current names, colors, and frag counts
	// FIXME: is this a good thing?
	SZ_Clear (&host_client->netchan.message);

// send current status of all other players

	// normally this could overflow, but no need to check due to backbuf
	for (i=0, client = svs.clients ; i<MAX_CLIENTS ; i++, client++)
		SV_FullClientUpdateToClient (client, host_client);

// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.democausesreconnect)
		{
			if (i >= MAX_STANDARDLIGHTSTYLES)
				continue;
			ClientReliableWrite_Begin (host_client, svc_lightstyle,
				3 + (sv.demolightstyles[i] ? strlen(sv.demolightstyles[i]) : 1));
			ClientReliableWrite_Byte (host_client, (char)i);
			ClientReliableWrite_String (host_client, sv.demolightstyles[i]);
		}
		else
		{
			if (i >= MAX_STANDARDLIGHTSTYLES)
				if (!sv.strings.lightstyles[i])
					continue;
#ifdef PEXT_LIGHTSTYLECOL
			if ((host_client->fteprotocolextensions & PEXT_LIGHTSTYLECOL) && sv.strings.lightstylecolours[i]!=7)
			{
				ClientReliableWrite_Begin (host_client, svcfte_lightstylecol,
					3 + (sv.strings.lightstyles[i] ? strlen(sv.strings.lightstyles[i]) : 1));
				ClientReliableWrite_Byte (host_client, (char)i);
				ClientReliableWrite_Char (host_client, sv.strings.lightstylecolours[i]);
				ClientReliableWrite_String (host_client, sv.strings.lightstyles[i]);
			}
			else
#endif
			{
				ClientReliableWrite_Begin (host_client, svc_lightstyle,
					3 + (sv.strings.lightstyles[i] ? strlen(sv.strings.lightstyles[i]) : 1));
				ClientReliableWrite_Byte (host_client, (char)i);
				ClientReliableWrite_String (host_client, sv.strings.lightstyles[i]);
			}
		}
	}

	// set up the edict
	for (split = host_client; split; split = split->controlled)
	{
		ent = split->edict;

		if (split->istobeloaded)	//minimal setup
		{
			split->entgravity = ent->xv->gravity;
			split->maxspeed = ent->xv->maxspeed;
		}
		else
		{
			SV_SetUpClientEdict(split, ent);
		}

	//
	// force stats to be updated
	//
		memset (host_client->statsi, 0, sizeof(host_client->statsi));
		memset (host_client->statsf, 0, sizeof(host_client->statsf));
		memset (host_client->statss, 0, sizeof(host_client->statss));
	}

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALSECRETS);
	ClientReliableWrite_Long (host_client, pr_global_struct->total_secrets);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALMONSTERS);
	ClientReliableWrite_Long (host_client, pr_global_struct->total_monsters);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_SECRETS);
	ClientReliableWrite_Long (host_client, pr_global_struct->found_secrets);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_MONSTERS);
	ClientReliableWrite_Long (host_client, pr_global_struct->killed_monsters);

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
	ClientReliableWrite_String (host_client, "skins\n" );

}

/*
==================
SV_SpawnSpectator
==================
*/
void SV_SpawnSpectator (void)
{
	int		i;
	edict_t	*e;

	VectorClear (sv_player->v->origin);
	VectorClear (sv_player->v->view_ofs);
	sv_player->v->view_ofs[2] = DEFAULT_VIEWHEIGHT;
	sv_player->v->movetype = MOVETYPE_NOCLIP;

	// search for an info_playerstart to spawn the spectator at
	//this is only useful when a mod doesn't nativly support spectators. old qw on nq mods.

	for (i=MAX_CLIENTS+1 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(svprogfuncs, i);
		if (!strcmp(PR_GetString(svprogfuncs, e->v->classname), "info_player_start"))
		{
			VectorCopy (e->v->origin, sv_player->v->origin);
			return;
		}
	}

}

/*
==================
SV_Begin_f
==================
*/
void SV_Begin_f (void)
{
	client_t	*split, *oh;
	unsigned pmodel = 0, emodel = 0;
	int		i;
	qboolean sendangles=false;

	if (!SV_CheckRealIP(host_client, true))
	{
		if (host_client->protocol == SCP_QUAKE2)
			ClientReliableWrite_Begin (host_client, svcq2_stufftext, 13+strlen(Cmd_Args()));
		else
			ClientReliableWrite_Begin (host_client, svc_stufftext, 13+strlen(Cmd_Args()));
		ClientReliableWrite_String (host_client, va("cmd begin %s\n", Cmd_Args()));
		return;
	}

	if (host_client->state == cs_spawned)
		return; // don't begin again

	for (split = host_client; split; split = split->controlled)
		split->state = cs_spawned;

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount && !sv.msgfromdemo)
	{
		Con_Printf ("SV_Begin_f from different level\n");
		SV_New_f ();
		return;
	}

	if (progstype == PROG_H2)
		host_client->edict->xv->playerclass = host_client->playerclass;	//make sure it's set the same as the userinfo

	for (split = host_client; split; split = split->controlled)
	{
#ifdef Q2SERVER
		if (ge)
		{
			ge->ClientBegin(split->q2edict);
			split->istobeloaded = false;
		}
		else
#endif
		if (split->istobeloaded)
		{
			func_t f;
			sendangles = true;
			split->istobeloaded = false;

			f = PR_FindFunction(svprogfuncs, "RestoreGame", PR_ANY);
			if (f)
			{
				pr_global_struct->time = sv.time;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
				PR_ExecuteProgram (svprogfuncs, f);
			}
		}
		else
		{
			if (split->spectator)
			{
				SV_SpawnSpectator ();

				if (SpectatorConnect)
				{
					//keep the spectator tracking the player from the previous map
					if (split->spec_track > 0)
						split->edict->v->goalentity = EDICT_TO_PROG(svprogfuncs, svs.clients[split->spec_track-1].edict);
					else
						split->edict->v->goalentity = 0;


					// copy spawn parms out of the client_t
					for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
					{
						if (spawnparamglobals[i])
							*spawnparamglobals[i] = split->spawn_parms[i];
					}

					// call the spawn function
					pr_global_struct->time = sv.time;
					pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
					PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
				}
			}
			else
			{
				if (svprogfuncs)
				{
					eval_t *eval, *eval2;
					eval = PR_FindGlobal(svprogfuncs, "ClientReEnter", 0);
					if (eval && split->spawninfo)
					{
						globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
						int j;
						edict_t *ent;
						ent = split->edict;
						j = strlen(split->spawninfo);
						SV_UnlinkEdict(ent);
						svprogfuncs->restoreent(svprogfuncs, split->spawninfo, &j, ent);

						eval2 = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "stats_restored", NULL);
						if (eval2)
							eval2->_float = 1;
						for (j=0 ; j< NUM_SPAWN_PARMS ; j++)
						{
							if (spawnparamglobals[j])
								*spawnparamglobals[j] = split->spawn_parms[j];
						}
						pr_global_struct->time = sv.time;
						pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
						G_FLOAT(OFS_PARM0) = sv.time - split->spawninfotime;
						PR_ExecuteProgram(svprogfuncs, eval->function);
					}
					else
					{
						// copy spawn parms out of the client_t
						for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
						{
							if (spawnparamglobals[i])
								*spawnparamglobals[i] = split->spawn_parms[i];
						}

						// call the spawn function
#ifdef VM_Q1
						if (svs.gametype == GT_Q1QVM)
							Q1QVM_ClientConnect(split);
						else
#endif
						{
							pr_global_struct->time = sv.time;
							pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
							PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);
					
							// actually spawn the player
							pr_global_struct->time = sv.time;
							pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
							PR_ExecuteProgram (svprogfuncs, pr_global_struct->PutClientInServer);
						}

						oh = host_client;
						SV_PreRunCmd();
						{
							usercmd_t cmd;
							memset(&cmd, 0, sizeof(cmd));
							cmd.msec = 0;
#define ANGLE2SHORT(x) (x) * (65536/360.0)
							cmd.angles[0] = ANGLE2SHORT(split->edict->v->v_angle[0]);
							cmd.angles[1] = ANGLE2SHORT(split->edict->v->v_angle[1]);
							cmd.angles[2] = ANGLE2SHORT(split->edict->v->v_angle[2]);
							SV_RunCmd(&cmd, false);
						}
						SV_PostRunCmd();
						host_client = oh;
					}
				}
			}
		}
	}

	// clear the net statistics, because connecting gives a bogus picture
	host_client->netchan.frame_latency = 0;
	host_client->netchan.frame_rate = 0;
	host_client->netchan.drop_count = 0;
	host_client->netchan.good_count = 0;

	//check he's not cheating
	if (progstype == PROG_QW)
	{
		pmodel = atoi(Info_ValueForKey (host_client->userinfo, "pmodel"));
		emodel = atoi(Info_ValueForKey (host_client->userinfo, "emodel"));

		if (pmodel != sv.model_player_checksum ||
			emodel != sv.eyes_player_checksum)
			SV_BroadcastTPrintf (PRINT_HIGH, STL_POSSIBLEMODELCHEAT, host_client->name);
	}

	// if we are paused, tell the client
	if (sv.paused)
	{
		if (!ISQ2CLIENT(host_client))
		{
			ClientReliableWrite_Begin (host_client, svc_setpause, 2);
			ClientReliableWrite_Byte (host_client, sv.paused);
		}
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_SERVERPAUSED);
	}

	if (sendangles)
	{
//
// send a fixangle over the reliable channel to make sure it gets there
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
		MSG_WriteByte (&host_client->netchan.message, svc_setangle);
		MSG_WriteAngle (&host_client->netchan.message, 0 );
		MSG_WriteAngle (&host_client->netchan.message, host_client->edict->v->angles[1] );
		MSG_WriteAngle (&host_client->netchan.message, 0 );
	}
}

//=============================================================================

//dp downloads are a 2-stream system
//the server->client stream is as you'd expect. except that its unreliable rather than reliable
//the client->server stream contains no actual data.
//when c2s has a hole, the s2c stream is reset to the last-known 'good' position.
//eventually the client is left acking packets with no data in them, the server then tells the client that the download is complete.
//the client does no checks to see if there's a hole, other than the crc

//so any single lost packet (even client->server) means that the entire stream will be set back by your ping time
void SV_DarkPlacesDownloadChunk(client_t *cl, sizebuf_t *msg)
{
#define MAXDPDOWNLOADCHUNK 1024
	char buffer[MAXDPDOWNLOADCHUNK];

	int size, start;

	if (!ISNQCLIENT(cl))
		return;
	if (!cl->download)
		return;

	if (!cl->downloadstarted)
		return;

	if (cl->num_backbuf)
		return;
	
	size = 1024;	//fixme

	if (size > cl->datagram.maxsize - cl->datagram.cursize)
		size = cl->datagram.maxsize - cl->datagram.cursize - 16;
	
	if (size > MAXDPDOWNLOADCHUNK)	//don't clog it too much
		size = MAXDPDOWNLOADCHUNK;

	start = VFS_TELL(cl->download);
	if (start+size > cl->downloadsize)	//clamp to the size of the file.
		size = cl->downloadsize - start;

	size = VFS_READ(cl->download, buffer, size);
	if (size < 0)
		size = 0;

	MSG_WriteByte(msg, svcdp_downloaddata);
	MSG_WriteLong (msg, start);
	MSG_WriteShort (msg, size);
	SZ_Write(msg, buffer, size);
}

void SVDP_StartDownload_f(void)
{
	if (host_client->protocol != SCP_DARKPLACES7)
		return;
	if (!host_client->download)
		return;
	host_client->downloadstarted = true;
}

void SV_DarkPlacesDownloadAck(client_t *cl)
{
	int start = MSG_ReadLong();
	int size = (unsigned short)MSG_ReadShort();

	if (!cl->download)
		return;

	if (start != cl->downloadacked)
	{
		//packetloss
		VFS_SEEK(cl->download, cl->downloadacked);
	}
	else if (size != 0)
	{
		cl->downloadacked += size;	//successful packet
		cl->downloadcount = cl->downloadacked;
	}
	else
	{
		char *s;
		unsigned short crc;
		int pos=0, csize;
		qbyte chunk[1024];
		QCRC_Init(&crc);
		VFS_SEEK(host_client->download, 0);
		while (pos < host_client->downloadsize)
		{
			csize = sizeof(chunk);
			if (pos + csize > host_client->downloadsize)
				csize = host_client->downloadsize - pos;
			VFS_READ(host_client->download, chunk, csize);
			QCRC_AddBlock(&crc, chunk, csize);
			pos += csize;
		}

		s = va("\ncl_downloadfinished %i %i \"\"\n", host_client->downloadsize, crc);
		ClientReliableWrite_Begin (cl, svc_stufftext, 2+strlen(s));
		ClientReliableWrite_String(cl, s);

		VFS_CLOSE(host_client->download);
		host_client->download = NULL;
		host_client->downloadsize = 0;
	}
}

void SV_NextChunkedDownload(int chunknum)
{
#define CHUNKSIZE 1024
	char buffer[CHUNKSIZE];
	int i;

	if (host_client->datagram.cursize + CHUNKSIZE+5+50 > host_client->datagram.maxsize)
		return;	//choked!

	if (VFS_SEEK (host_client->download, chunknum*CHUNKSIZE) == false)
		return;

	i = VFS_READ (host_client->download, buffer, CHUNKSIZE);

	if (i > 0)
	{
		if (i != CHUNKSIZE)
			memset(buffer+i, 0, CHUNKSIZE-i);

		MSG_WriteByte(&host_client->datagram, svc_download);
		MSG_WriteLong(&host_client->datagram, chunknum);
		SZ_Write(&host_client->datagram, buffer, CHUNKSIZE);
	}
}

/*
==================
SV_NextDownload_f
==================
*/
void SV_NextDownload_f (void)
{
	qbyte	buffer[1024];
	int		r;
	int		percent;
	int		size;

	if (!host_client->download)
		return;

#ifdef PEXT_CHUNKEDDOWNLOADS
	if (host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
	{
		SV_NextChunkedDownload(atoi(Cmd_Argv(1)));
		return;
	}
#endif

	r = host_client->downloadsize - host_client->downloadcount;
	/*
#ifdef PEXT_ZLIBDL
	if (host_client->protocolextensions & PEXT_ZLIBDL)
	{
		if (r>1024)	//expect a little more.
			r=1024;
	}
	else
#endif
		*/
	if (r > 768)
		r = 768;
	r = VFS_READ (host_client->download, buffer, r);
	ClientReliableWrite_Begin (host_client, ISQ2CLIENT(host_client)?svcq2_download:svc_download, 6+r);
	ClientReliableWrite_Short (host_client, r);

	host_client->downloadcount += r;
	size = host_client->downloadsize;

	if (host_client->downloadcount < size)
	{
		if (!size)
			size = 1;

		percent = (double)host_client->downloadcount*100.0/size;
		percent = bound(0, percent, 99);
	}
	else
		percent = 100;

#ifdef PEXT_ZLIBDL
	if (host_client->fteprotocolextensions & PEXT_ZLIBDL)
	{
		ClientReliableWrite_Byte (host_client, percent+101);
		ClientReliableWrite_ZLib (host_client, buffer, r);
	}
	else
#endif
	{
		ClientReliableWrite_Byte (host_client, percent);
		ClientReliableWrite_SZ (host_client, buffer, r);
	}

	if (host_client->downloadcount < host_client->downloadsize)
		return;

	VFS_CLOSE (host_client->download);
	host_client->download = NULL;

}

void VARGS OutofBandPrintf(netadr_t where, char *fmt, ...)
{
	va_list		argptr;
	char	send[1024];

	send[0] = 0xff;
	send[1] = 0xff;
	send[2] = 0xff;
	send[3] = 0xff;
	send[4] = A2C_PRINT;
	va_start (argptr, fmt);
	vsprintf (send+5, fmt, argptr);
	va_end (argptr);

	NET_SendPacket (NS_SERVER, strlen(send)+1, send, where);
}

/*
==================
SV_NextUpload
==================
*/
void SV_NextUpload (void)
{
	int		percent;
	int		size;

	if (!*host_client->uploadfn)
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_UPLOADDENIED);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
		ClientReliableWrite_String (host_client, "stopul");

		// suck out rest of packet
		size = MSG_ReadShort ();	MSG_ReadByte ();
		msg_readcount += size;
		return;
	}

	size = MSG_ReadShort ();
	percent = MSG_ReadByte ();

	if (!host_client->upload)
	{
		host_client->upload = FS_OpenVFS(host_client->uploadfn, "wb", FS_GAME);
		if (!host_client->upload)
		{
			Sys_Printf("Can't create %s\n", host_client->uploadfn);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
			ClientReliableWrite_String (host_client, "stopul");
			*host_client->uploadfn = 0;
			return;
		}
		Con_Printf("Receiving %s from %d...\n", host_client->uploadfn, host_client->userid);
		if (host_client->remote_snap)
			OutofBandPrintf(host_client->snap_from, "Server receiving %s from %d...\n", host_client->uploadfn, host_client->userid);
	}

	VFS_WRITE (host_client->upload, net_message.data + msg_readcount, size);
	msg_readcount += size;

	if (percent != 100)
	{
		ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
		ClientReliableWrite_String (host_client, "nextul\n");
	}
	else
	{
		VFS_CLOSE (host_client->upload);
		host_client->upload = NULL;

		Con_Printf("%s upload completed.\n", host_client->uploadfn);

		if (host_client->remote_snap)
		{
			char *p;

			if ((p = strchr(host_client->uploadfn, '/')) != NULL)
				p++;
			else
				p = host_client->uploadfn;
			OutofBandPrintf(host_client->snap_from, "%s upload completed.\nTo download, enter:\ndownload %s\n",
				host_client->uploadfn, p);
		}
	}

}

//Use of this function is on name only.
//Be aware that the maps directory should be restricted based on weather the file was from a pack file
//this is to preserve copyright - please do not breach due to a bug.
qboolean SV_AllowDownload (char *name)
{
	extern	cvar_t	allow_download;
	extern	cvar_t	allow_download_skins;
	extern	cvar_t	allow_download_models;
	extern	cvar_t	allow_download_sounds;
	extern	cvar_t	allow_download_demos;
	extern	cvar_t	allow_download_maps;
	extern	cvar_t	allow_download_textures;
	extern	cvar_t	allow_download_pk3s;
	extern	cvar_t	allow_download_wads;
	extern	cvar_t	allow_download_root;
	extern	cvar_t	allow_download_configs;

	//allowed at all?
	if (!allow_download.value)
		return false;

	//no subdirs?
	if (strstr (name, ".."))	//no under paths.
		return false;
	if (*name == '.')	//relative is pointless
		return false;
	if (*name == '/')	//no absolute.
		return false;
	if (strchr(name, '\\'))	//no windows paths - grow up lame windows users.
		return false;

	if (strncmp(name,	"maps/", 5) == 0)
		return !!allow_download_maps.value;

	//skins?
	if (strncmp(name,	"skins/", 6) == 0)
		return !!allow_download_skins.value;
	//models
	if ((strncmp(name,	"progs/", 6) == 0) ||
		(strncmp(name,	"models/", 7) == 0))
		return !!allow_download_models.value;
	//sound
	if (strncmp(name,	"sound/", 6) == 0)
		return !!allow_download_sounds.value;
	//demos
	if (strncmp(name,	"demos/", 6) == 0)
		return !!allow_download_demos.value;

	//textures
	if (strncmp(name,	"textures/", 9) == 0)
		return !!allow_download_textures.value;

	//wads
	if (strncmp(name,	"wads/", 5) == 0)
		return !!allow_download_wads.value;
	if (!strcmp("wad", COM_FileExtension(name)))
		return !!allow_download_wads.value;

	//pk3s.
	if (!strcmp("pk3", COM_FileExtension(name)) || !strcmp("pak", COM_FileExtension(name)))
		if (strnicmp(name, "pak", 3))	//don't give out q3 pk3 files.
			return !!allow_download_pk3s.value;

	if (!strcmp("cfg", COM_FileExtension(name)))
		return !!allow_download_configs.value;

	//root of gamedir
	if (!strchr(name, '/') && !allow_download_root.value)
	{
		if (strcmp(name, "csprogs.dat"))	//we always allow csprogs.dat to be downloaded.
			return false;
	}

	//any other subdirs are allowed
	return true;
}
/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f(void)
{
	char *name = Cmd_Argv(1);
	extern	cvar_t	allow_download_anymap, allow_download_pakcontents;
	extern cvar_t sv_demoDir;

	if (ISNQCLIENT(host_client) && host_client->protocol != SCP_DARKPLACES7)
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Your client isn't meant to support downloads\n");
		return;
	}

	// MVD hacked junk
	if (!strncmp(name, "demonum/", 8))
	{
		char *mvdname = SV_MVDNum(atoi(name+8));

		if (!mvdname)
		{
			SV_ClientPrintf (host_client, PRINT_HIGH, "%s is an invalid MVD demonum.\n", name+8);
			Sys_Printf ("%s requested invalid demonum %s\n", host_client->name, name+8);
		}
		else if (ISQ2CLIENT(host_client))
		{
			Sys_Printf ("Rejected MVD download of %s to %s (Q2 client)\n", mvdname, host_client->name);
			ClientReliableWrite_Begin (host_client, svcq2_download, 4);
			ClientReliableWrite_Short (host_client, -1);
			ClientReliableWrite_Byte (host_client, 0);
			return;
		}
		else
			SV_ClientPrintf (host_client, PRINT_HIGH, "Sending demo %s...\n", mvdname);

		// this is needed to cancel the current download
		if (ISNQCLIENT(host_client))
		{
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+12);
			ClientReliableWrite_String (host_client, "\nstopdownload\n");
		}
		else
#ifdef PEXT_CHUNKEDDOWNLOADS
		if (host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
		{
			ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(name));
			ClientReliableWrite_Long (host_client, -1);
			ClientReliableWrite_Long (host_client, -1);
			ClientReliableWrite_String (host_client, name);
		}
		else
#endif
		{
			ClientReliableWrite_Begin (host_client, svc_download, 4);
			ClientReliableWrite_Short (host_client, -1);
			ClientReliableWrite_Byte (host_client, 0);
		}

		if (mvdname)
		{
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+15+strlen(mvdname)); 
			ClientReliableWrite_String (host_client, va("\ndownload demos/%s\n", mvdname));
		}
		return;
	}

// hacked by zoid to allow more conrol over download
	if (!SV_AllowDownload(name))
	{	// don't allow anything with .. path
		if (ISNQCLIENT(host_client))
		{
			SV_PrintToClient(host_client, PRINT_HIGH, "Download rejected by server settings\n");

			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+12);
			ClientReliableWrite_String (host_client, "\nstopdownload\n");
		}
#ifdef PEXT_CHUNKEDDOWNLOADS
		else if (host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
		{
			ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(name));
			ClientReliableWrite_Long (host_client, -1);
			ClientReliableWrite_Long (host_client, -2);
			ClientReliableWrite_String (host_client, name);
		}
		else
#endif
		{
			ClientReliableWrite_Begin (host_client, ISQ2CLIENT(host_client)?svcq2_download:svc_download, 4);
			ClientReliableWrite_Short (host_client, -1);
			ClientReliableWrite_Byte (host_client, 0);
		}
		return;
	}

	if (host_client->download)
	{
		VFS_CLOSE (host_client->download);
		host_client->download = NULL;
	}

	// lowercase name (needed for casesen file systems)
	{
		char *p;

		for (p = name; *p; p++)
			*p = (char)tolower(*p);
	}

	// yet another hack for MVD junk
	if (!strncmp(name, "demos/", 6))
		host_client->download = FS_OpenVFS(va("%s/%s", sv_demoDir.string, name+6), "rb", FS_GAME);
	else
		host_client->download = FS_OpenVFS(name, "rb", FS_GAME);
	host_client->downloadcount = 0;

	if (!host_client->download
		// special check for maps, if it came from a pak file, don't allow
		// download  ZOID
		|| ((!allow_download_pakcontents.value || (!allow_download_anymap.value && strncmp(name, "maps/", 5) == 0)) && com_file_copyprotected))
	{
		if (host_client->download)
		{
			VFS_CLOSE(host_client->download);
			host_client->download = NULL;
		}

		Sys_Printf ("Couldn't download %s to %s\n", name, host_client->name);
		if (ISNQCLIENT(host_client))
		{
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+12);
			ClientReliableWrite_String (host_client, "\nstopdownload\n");
		}
		else
#ifdef PEXT_CHUNKEDDOWNLOADS
		if (host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
		{
			ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(name));
			ClientReliableWrite_Long (host_client, -1);
			ClientReliableWrite_Long (host_client, -1);
			ClientReliableWrite_String (host_client, name);
		}
		else
#endif
		{
			ClientReliableWrite_Begin (host_client, ISQ2CLIENT(host_client)?svcq2_download:svc_download, 4);
			ClientReliableWrite_Short (host_client, -1);
			ClientReliableWrite_Byte (host_client, 0);
		}
		return;
	}

	host_client->downloadsize = VFS_GETLEN(host_client->download);

#ifdef PEXT_CHUNKEDDOWNLOADS
	if (host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
	{
		if (host_client->download->seekingisabadplan)
		{	//if seeking is a bad plan (for whatever reason - usually because of zip files)
			//create a temp file instead
			int i, len;
			char buffer[8192];
			vfsfile_t *tmp;
			tmp = FS_OpenTemp();

			for (i = 0; ; i+=len)
			{
				len = sizeof(buffer);
				if (len > host_client->downloadsize-i)
					len = host_client->downloadsize-i;
				if (len == 0)
					break;
				VFS_READ(host_client->download, buffer, len);
				VFS_WRITE(tmp, buffer, len);
			}
			VFS_CLOSE(host_client->download);
			host_client->download = tmp;
		}

		ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(name));
		ClientReliableWrite_Long (host_client, -1);
		ClientReliableWrite_Long (host_client, host_client->downloadsize);
		ClientReliableWrite_String (host_client, name);
	}
#endif

	if (ISNQCLIENT(host_client))
	{
		char *s = va("\ncl_downloadbegin %i %s\n", host_client->downloadsize, name);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(s));
		ClientReliableWrite_String (host_client, s);
	}
	else
		SV_NextDownload_f ();

	SV_EndRedirect();
	Con_Printf ("Downloading %s to %s\n", name, host_client->name);
}

void SV_StopDownload_f(void)
{
	//this doesn't mean the download failed or was canceled.
	if (host_client->download)
	{
		VFS_CLOSE (host_client->download);
		host_client->download = NULL;
	}
	else
		Con_Printf ("But you're not downloading anything\n");

	host_client->downloadstarted = false;
}

//=============================================================================


/*
==================
SV_SayOne_f
==================
*/
void SV_SayOne_f (void)
{
	char	text[1024];
	client_t	*to;
	int i;
	char *s, *s2;
	int clnum=-1;

	if (Cmd_Argc () < 3)
		return;

	while((to = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if (host_client->spectator)
		{
			if (!sv_spectalk.value || to->spectator)
				sprintf (text, "[SPEC] {%s}:", host_client->name);
			else
				continue;
		}
		else
			sprintf (text, "{%s}:", host_client->name);

		if (host_client->ismuted)
		{
			SV_ClientTPrintf(host_client, PRINT_CHAT,
					STL_YOUAREMUTED);
			return;
		}


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

	//filter out '\n' and '\r'
		s = text;
		s2 = text;
		while(*s2)
		{
			if (*s2 == '\r' || *s2 == '\n')
			{
				s2++;
				continue;
			}
			*s = *s2;
			s++;
			s2++;
		}
		*s = '\0';

		strcat(text, "\n");
		SV_ClientPrintf(to, PRINT_CHAT, "%s", text);
	}

	if (clnum==-1)	//none found
	{
		SV_ClientTPrintf(host_client, PRINT_CHAT, STL_NAMEDCLIENTDOESNTEXIST);
		return;
	}
}

float SV_CheckFloodProt(client_t *client)
{
	if (!sv_floodprotect.value)
		return 0;
	if (sv_floodprotect_messages.value <= 0 || sv_floodprotect_interval.value <= 0)
		return 0;
	if (sv.paused)
		return 0;
	if (realtime < client->lockedtill)
		return client->lockedtill - realtime;

	if (client->floodprotmessage > sv_floodprotect_messages.value)
	{
		client->lockedtill = realtime + sv_floodprotect_silencetime.value;
		client->floodprotmessage = 0.0;
		client->lastspoke = 0.0;
		if (sv_floodprotect_sendmessage.string[0])
			 SV_ClientPrintf(client, PRINT_CHAT, "FloodProt: %s\n", sv_floodprotect_sendmessage.string);
		return sv_floodprotect_silencetime.value;
	}

	return 0;
}

void SV_PushFloodProt(client_t *client)
{
	if (!sv_floodprotect.value)
		return;
	if (sv_floodprotect_messages.value <= 0 || sv_floodprotect_interval.value <= 0)
		return;
	if (sv.paused)
		return;

	if (client->lastspoke)
	{
		client->floodprotmessage -= (realtime - client->lastspoke) 
			* sv_floodprotect_messages.value
			/ sv_floodprotect_interval.value;
		client->floodprotmessage = max(0, client->floodprotmessage);
		client->floodprotmessage++;
	}
	else
		client->floodprotmessage = 1.0;
	client->lastspoke = realtime;
}

/*
==================
SV_Say
==================
*/
void SV_Say (qboolean team)
{
	client_t *client;
	int		j;
	char	*p;
	char	text[1024];
	char	t1[32], *t2;
	int cls = 0;
	float floodtime;

	qboolean sent[MAX_CLIENTS];	//so we don't send to the same splitscreen connection twice. (it's ugly)
	int cln;

	qboolean mvdrecording;

	char *s, *s2;

	if (Cmd_Argc () < 2)
		return;

	Sys_ServerActivity();

	memset(sent, 0, sizeof(sent));

	if (team)
	{
		Q_strncpyz (t1, Info_ValueForKey (host_client->userinfo, "team"), sizeof(t1));
	}

	if (host_client->spectator && (!sv_spectalk.value || team))
		sprintf (text, "[SPEC] %s: ", host_client->name);
	else if (team)
		sprintf (text, "(%s): ", host_client->name);
	else
		sprintf (text, "%s: ", host_client->name);

	if (host_client->ismuted)
	{
		SV_ClientTPrintf(host_client, PRINT_CHAT,
			STL_MUTEDCHAT);
		return;
	}

#ifdef VM_Q1
	if (Q1QVM_ClientSay(sv_player, team))
		return;
#endif

	if ((floodtime=SV_CheckFloodProt(host_client)))
	{
		SV_ClientTPrintf(host_client, PRINT_CHAT,
				STL_FLOODPROTTIME,
					(int) (floodtime));
		return;
	}
	SV_PushFloodProt(host_client);

	p = Cmd_Args();

	if (*p == '"')
	{
		p++;
		p[Q_strlen(p)-1] = 0;
	}

	if (strlen(text)+strlen(p)+2 >= sizeof(text)-10)
	{
		SV_ClientTPrintf(host_client, PRINT_CHAT, STL_BUFFERPROTECTION);
		return;
	}
	if (svprogfuncs)
		if (PR_QCChat(p, team))	//true if handled.
			return;

	Q_strcat(text, p);

	//filter out '\n' and '\r'
	if (sv_chatfilter.value)
	{
		s = text;
		s2 = text;
		while(*s2)
		{
			if (*s2 == '\r' || *s2 == '\n')
			{
				s2++;
				continue;
			}
			*s = *s2;
			s++;
			s2++;
		}
		*s = '\0';
	}

	Q_strcat(text, "\n");

	Sys_Printf ("%s", text);

	mvdrecording = sv.mvdrecording;
	sv.mvdrecording = false;	//so that the SV_ClientPrintf doesn't send to all players.
	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state != cs_spawned && client->state != cs_connected)
			continue;
		if (host_client->spectator && !sv_spectalk.value)
			if (!client->spectator)
				continue;

		if (team)
		{
			// the spectator team
			if (host_client->spectator) {
				if (!client->spectator)
					continue;
			} else {
				t2 = Info_ValueForKey (client->userinfo, "team");
				if (strcmp(t1, t2) || client->spectator)
					continue;	// on different teams
			}
		}

		cls |= 1 << j;

//make sure we don't send the say to the same client 20 times due to splitscreen
		if (client->controller)
			cln = client->controller - svs.clients;
		else
			cln = client - svs.clients;
		if (sent[cln])
			continue;
		else
			sent[cln] = true;

		SV_ClientPrintf(client, PRINT_CHAT, "%s", text);
	}
	sv.mvdrecording = mvdrecording;

	if (!sv.mvdrecording || !cls)
		return;

	// non-team messages should be seen always, even if not tracking any player
	if (!team && ((host_client->spectator && sv_spectalk.value) || !host_client->spectator))
	{
		MVDWrite_Begin (dem_all, 0, strlen(text)+3);
	}
	else
		MVDWrite_Begin (dem_multiple, cls, strlen(text)+3);

	MSG_WriteByte ((sizebuf_t*)demo.dbuf, svc_print);
	MSG_WriteByte ((sizebuf_t*)demo.dbuf, PRINT_CHAT);
	MSG_WriteString ((sizebuf_t*)demo.dbuf, text);
}


/*
==================
SV_Say_f
==================
*/
void SV_Say_f(void)
{
	SV_Say (false);
}
/*
==================
SV_Say_Team_f
==================
*/
void SV_Say_Team_f(void)
{
	SV_Say (true);
}



//============================================================================

/*
=================
SV_Pings_f

The client is showing the scoreboard, so send new ping times for all
clients
=================
*/
void SV_Pings_f (void)
{
	client_t *client;
	int		j;

	if (sv.demofile)
	{
		for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
		{
			if (!*sv.recordedplayer[j].userinfo)
				continue;
			ClientReliableWrite_Begin (host_client, svc_updateping, 4);
			ClientReliableWrite_Byte (host_client, j);
			ClientReliableWrite_Short (host_client, sv.recordedplayer[j].ping);
			ClientReliableWrite_Begin (host_client, svc_updatepl, 4);
			ClientReliableWrite_Byte (host_client, j);
			ClientReliableWrite_Byte (host_client, sv.recordedplayer[j].pl);
		}
		return;
	}

	if (ISNQCLIENT(host_client))
	{
		char *s;
		ClientReliableWrite_Begin(host_client, svc_stufftext, 15+10*MAX_CLIENTS);
		ClientReliableWrite_SZ(host_client, "pingplreport", 12);
		for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
		{
			s = va(" %i %i", SV_CalcPing(client), client->lossage);
			ClientReliableWrite_SZ(host_client, s, strlen(s));
		}
		ClientReliableWrite_Byte (host_client, '\n');
		ClientReliableWrite_Byte (host_client, '\0');

	}
	else
	{
		for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
		{
			if (client->state != cs_spawned)
				continue;

			ClientReliableWrite_Begin (host_client, svc_updateping, 4);
			ClientReliableWrite_Byte (host_client, j);
			ClientReliableWrite_Short (host_client, SV_CalcPing(client));
			ClientReliableWrite_Begin (host_client, svc_updatepl, 4);
			ClientReliableWrite_Byte (host_client, j);
			ClientReliableWrite_Byte (host_client, client->lossage);
		}
	}
}



/*
==================
SV_Kill_f
==================
*/
void SV_Kill_f (void)
{
	float floodtime;

#ifdef VM_Q1
	if (svs.gametype == GT_Q1QVM)
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		Q1QVM_ClientCommand();
		return;
	}
#endif

	if (sv_player->v->health <= 0)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_NOSUICIDEWHENDEAD);
		return;
	}

	if (sv_floodprotect_suicide.value)
	{
		if ((floodtime = SV_CheckFloodProt(host_client)))
		{
			SV_ClientPrintf (host_client, PRINT_HIGH, "You can't suicide for %i seconds\n", (int)floodtime);
			return;
		}
		SV_PushFloodProt(host_client);
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientKill);
}

/*
==================
SV_TogglePause
==================
*/
qboolean SV_TogglePause (client_t *initiator)
{
	int i;
	client_t *cl;
	int newv;

	newv = sv.paused^1;

	if (!PR_ShouldTogglePause(initiator, newv))
		return false;

	sv.paused = newv;

	sv.pausedstart = Sys_DoubleTime();

	// send notification to all clients
	for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (!ISQ2CLIENT(cl) && !cl->controller)
		{
			ClientReliableWrite_Begin (cl, svc_setpause, 2);
			ClientReliableWrite_Byte (cl, sv.paused);
		}
	}

	return true;
}


/*
==================
SV_Pause_f
==================
*/
void SV_Pause_f (void)
{
	if (!pausable.value)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_CANTPAUSE);
		return;
	}

	if (host_client->spectator && !svs.demoplayback)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_CANTPAUSESPEC);
		return;
	}

	if (SV_TogglePause(host_client))
	{
		if (sv.paused)
			SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTPAUSED, host_client->name);
		else
			SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTUNPAUSED, host_client->name);
	}

}


/*
=================
SV_Drop_f

The client is going to disconnect, so remove the connection immediately
=================
*/
void SV_Drop_f (void)
{
	SV_EndRedirect ();
	if (!host_client->spectator)
		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTDROPPED, host_client->name);
	SV_DropClient (host_client);
}

/*
=================
SV_PTrack_f

Change the bandwidth estimate for a client
=================
*/
void SV_PTrack_f (void)
{
	int		i;
	edict_t *ent, *tent;

	if (!host_client->spectator && !sv.demofile)
		return;

	if (Cmd_Argc() != 2)
	{
		// turn off tracking
		host_client->spec_track = 0;
		ent = EDICT_NUM(svprogfuncs, host_client - svs.clients + 1);
		tent = EDICT_NUM(svprogfuncs, 0);
		ent->v->goalentity = EDICT_TO_PROG(svprogfuncs, tent);
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (*sv.recordedplayer[i].userinfo)
	{
		host_client->spec_track = i+1;
		return;
	}

	if (i < 0 || i >= MAX_CLIENTS || svs.clients[i].state != cs_spawned ||
		svs.clients[i].spectator)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_INVALIDTRACKCLIENT);
		host_client->spec_track = 0;
		ent = EDICT_NUM(svprogfuncs, host_client - svs.clients + 1);
		tent = EDICT_NUM(svprogfuncs, 0);
		ent->v->goalentity = EDICT_TO_PROG(svprogfuncs, tent);
		return;
	}
	host_client->spec_track = i + 1; // now tracking

	ent = EDICT_NUM(svprogfuncs, host_client - svs.clients + 1);
	tent = EDICT_NUM(svprogfuncs, i + 1);
	ent->v->goalentity = EDICT_TO_PROG(svprogfuncs, tent);
}


/*
=================
SV_Rate_f

Change the bandwidth estimate for a client
=================
*/
void SV_Rate_f (void)
{
	if (Cmd_Argc() != 2)
	{
		SV_ClientPrintf (host_client, PRINT_HIGH, "Effective rate %i\n", SV_RateForClient(host_client));
		return;
	}

	host_client->rate = atoi(Cmd_Argv(1));

	SV_ClientTPrintf (host_client, PRINT_HIGH, STL_RATESETTO, SV_RateForClient(host_client));
}


/*
=================
SV_Msg_f

Change the message level for a client
=================
*/
void SV_Msg_f (void)
{
	if (Cmd_Argc() != 2)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_CURRENTMSGLEVEL,
			host_client->messagelevel);
		return;
	}

	host_client->messagelevel = atoi(Cmd_Argv(1));

	SV_ClientTPrintf (host_client, PRINT_HIGH, STL_MSGLEVELSET, host_client->messagelevel);
}

qboolean SV_UserInfoIsBasic(char *infoname)
{
	int i;
	char *basicinfos[] = {

		"name",
		"team",
		"skin",
		"topcolor",
		"bottomcolor",

		NULL};

	for (i = 0; basicinfos[i]; i++)
	{
		if (*infoname == '*' || !strcmp(infoname, basicinfos[i]))
			return true;
	}
	return false;
}

/*
==================
SV_SetInfo_f

Allow clients to change userinfo
==================
*/
void SV_SetInfo_f (void)
{
	int i, j;
	char oldval[MAX_INFO_STRING];
	char *key, *val;
	qboolean basic;	//infos that we send to any old qw client.
	client_t *client;


	if (Cmd_Argc() == 1)
	{
		Con_Printf ("User info settings:\n");
		Info_Print (host_client->userinfo);
		return;
	}

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv(1)[0] == '*')
		return;		// don't set priveledged values

	if (strstr(Cmd_Argv(1), "\\") || strstr(Cmd_Argv(2), "\\"))
		return;		// illegal char

	Q_strncpyz(oldval, Info_ValueForKey(host_client->userinfo, Cmd_Argv(1)), MAX_INFO_STRING);

#ifdef VM_Q1
	if (Q1QVM_UserInfoChanged(sv_player))	
		return;
#endif

	Info_SetValueForKey (host_client->userinfo, Cmd_Argv(1), Cmd_Argv(2), sizeof(host_client->userinfo));
// name is extracted below in ExtractFromUserInfo
//	strncpy (host_client->name, Info_ValueForKey (host_client->userinfo, "name")
//		, sizeof(host_client->name)-1);
//	SV_FullClientUpdate (host_client, &sv.reliable_datagram);
//	host_client->sendinfo = true;

	if (!strcmp(Info_ValueForKey(host_client->userinfo, Cmd_Argv(1)), oldval))
		return; // key hasn't changed

	// process any changed values
	SV_ExtractFromUserinfo (host_client);

	if (progstype != PROG_QW && !strcmp(Cmd_Argv(1), "bottomcolor"))
	{	//team fortress has a nasty habit of booting people without this
		sv_player->v->team = atoi(Cmd_Argv(2))+1;
	}

	if (*Cmd_Argv(1) != '_')
	{
		i = host_client - svs.clients;
		key = Cmd_Argv(1);
		val = Info_ValueForKey(host_client->userinfo, key);

		basic = SV_UserInfoIsBasic(key);

		if (basic)
			Info_SetValueForKey (host_client->userinfobasic, key, val, sizeof(host_client->userinfobasic));
		for (j = 0; j < MAX_CLIENTS; j++)
		{
			client = svs.clients+j;
			if (client->state < cs_connected)
				continue;	// reliables go to all connected or spawned
			if (client->controller)
				continue;	//splitscreen

			if (client->protocol == SCP_BAD)
				continue;	//botclient

			if (ISQWCLIENT(client))
			{
				if (basic || (client->fteprotocolextensions & PEXT_BIGUSERINFOS))
				{
					MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
					MSG_WriteByte (&sv.reliable_datagram, i);
					MSG_WriteString (&sv.reliable_datagram, key);
					MSG_WriteString (&sv.reliable_datagram, val);
				}
			}
		}

		if (sv.mvdrecording)
		{
			MVDWrite_Begin (dem_all, 0, strlen(key)+strlen(val)+4);
			MSG_WriteByte ((sizebuf_t*)demo.dbuf, svc_setinfo);
			MSG_WriteByte ((sizebuf_t*)demo.dbuf, i);
			MSG_WriteString ((sizebuf_t*)demo.dbuf, key);
			MSG_WriteString ((sizebuf_t*)demo.dbuf, val);
		}
	}

	SV_LogPlayer(host_client, "userinfo changed");

	PR_ClientUserInfoChanged(Cmd_Argv(1), oldval, Info_ValueForKey(host_client->userinfo, Cmd_Argv(1)));
}

/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
void SV_ShowServerinfo_f (void)
{
	Info_Print (svs.info);
}

void SV_NoSnap_f(void)
{
	SV_LogPlayer(host_client, "refused snap");

	if (*host_client->uploadfn) {
		*host_client->uploadfn = 0;
		SV_BroadcastTPrintf (PRINT_HIGH, STL_SNAPREFUSED, host_client->name);
	}
}

//3 votes per player.
typedef struct voteinfo_s {
	struct voteinfo_s *next;
	float timeout;
	int clientid;
	char command[1];
} voteinfo_t;
voteinfo_t *voteinfo;


void VoteAdd (char *cmd, int id)
{
	voteinfo_t *vote;
	vote = Z_Malloc(sizeof(voteinfo_t)+strlen(cmd));	//null term is part of voteinfo_t
	strcpy(vote->command, cmd);
	vote->clientid = id;
	vote->timeout = realtime+votetime.value*60;
	vote->next = voteinfo;
	voteinfo = vote;
}

void VoteRemoveCommands(char *command, int id)	//all of one command
{
	voteinfo_t *vote, *prev;
	prev = NULL;
	for (vote = voteinfo; vote; vote = vote->next)
	{
		if ((!command || !strcmp(vote->command, command)) && (vote->clientid == id || id == -1))
		{
			if (prev)
				prev->next = vote->next;
			else
				voteinfo = vote->next;

			Z_Free(vote);
			VoteRemoveCommands(command, id);
			return;
		}
		else
			prev = vote;
	}
}
void VoteFlushAll(void)
{
	VoteRemoveCommands(NULL, -1);
}

int VoteCount(char *command, int id)	//all of one command
{
	voteinfo_t *vote;
	int num=0;
	for (vote = voteinfo; vote; vote = vote->next)
	{
		if (!command || !strcmp(vote->command, command))
		{
			if (vote->clientid == id || id == -1)
				num++;
		}
	}

	return num;
}

void VoteCheckTimes(void)
{
	voteinfo_t *vote, *prev;
	prev = NULL;
	for (vote = voteinfo; vote; vote = vote->next)
	{
		if (vote->timeout < realtime)
		{
			if (prev)
				prev->next = vote->next;
			else
				voteinfo = vote->next;

			Z_Free(vote);
		}
		else
			prev = vote;
	}
}

void SV_Vote_f (void)
{
	char *command = Cmd_Args();
	char *base;
	int id = host_client->userid;
	int num;
	int totalusers = 0;
	qboolean passes;

	if (!votelevel.value)
	{
		Con_TPrintf(STL_NOVOTING);
		return;
	}
	if (host_client->ismuted)
	{
		Con_TPrintf(STL_MUTEDVOTE);
		return;
	}

	Cmd_ExecLevel = votelevel.value;
	base = command;
	while(*base>' ')
		base++;
	if (*base)
		*base = '\0';
	else
		base = NULL;
	if (strchr(command, ';') || !strcmp(command, "if"))
	{
		Con_TPrintf(STL_BADVOTE);
		return;
	}
	num = Cmd_Level(command);
	if (base)
		*base = ' ';
	if (num != Cmd_ExecLevel)
	{
		Con_TPrintf(STL_BADVOTE);
		return;
	}


	VoteCheckTimes();

	for (num = 0; num < sv.allocated_client_slots; num++)
		if (svs.clients[num].state == cs_spawned)
			totalusers++;

	if (VoteCount(command, id))
	{
		VoteRemoveCommands(command, id);
		Con_TPrintf(STL_OLDVOTEREMOVED);
		return;
	}
	if (VoteCount(NULL, id)>=3)
	{
		VoteRemoveCommands(NULL, id);
		Con_TPrintf(STL_VOTESREMOVED);
	}

	num = VoteCount(command, -1)+1;

	passes = true;
	if (votepercent.value < 0 && (float)(totalusers-num) >= 0.5*totalusers)
		passes = false;
	if (votepercent.value >= 0 && num <= totalusers*votepercent.value/100)
		passes = false;
	if (num < voteminimum.value)
		passes = false;

	if (passes)	//>min number of votes, and meets the percent required
	{
		SV_BroadcastTPrintf(PRINT_HIGH, STL_FINALVOTE, host_client->name, command);

		VoteRemoveCommands(command, -1);
		Cbuf_AddText(command, votelevel.value);
		Cbuf_AddText("\n", votelevel.value);
		//Cmd_ExecuteString (command, votelevel.value);
		return;
	}
	else	//otherwise, try later.
	{
		SV_BroadcastTPrintf(PRINT_HIGH, STL_VOTE, host_client->name, command);

		VoteAdd(command, id);
	}
}

extern qboolean	sv_allow_cheats;

void Cmd_Notarget_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	SV_LogPlayer(host_client, "notarget cheat");
	if ((int) (sv_player->v->flags = (int) sv_player->v->flags ^ FL_NOTARGET) & FL_NOTARGET)
		SV_ClientPrintf (host_client, PRINT_HIGH, "notarget ON\n");
	else
		SV_ClientPrintf (host_client, PRINT_HIGH, "notarget OFF\n");
}

//Sets client to godmode
void Cmd_God_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	SV_LogPlayer(host_client, "god cheat");
	if ((int) (sv_player->v->flags = (int) sv_player->v->flags ^ FL_GODMODE) & FL_GODMODE)
		SV_ClientPrintf (host_client, PRINT_HIGH, "godmode ON\n");
	else
		SV_ClientPrintf (host_client, PRINT_HIGH, "godmode OFF\n");
}


void Cmd_Give_f (void)
{
	char *t;
	int v;

	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	t = Cmd_Argv(1);
	v = atoi (Cmd_Argv(2));

	SV_LogPlayer(host_client, "give cheat");
	if (strlen(t) == 1 && (Cmd_Argc() == 3 || (*t>='0' && *t <= '9')))
	{
		switch (t[0])
		{
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			sv_player->v->items = (int) sv_player->v->items | IT_SHOTGUN<< (t[0] - '2');
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
		}
	}
	else if (host_client->netchan.remote_address.type == NA_LOOPBACK)	//we don't want clients doing nasty things... like setting movetype 3123
	{
		int oldself;
		oldself = pr_global_struct->self;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		Con_Printf("Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, Cmd_Args()));
		pr_global_struct->self = oldself;
	}

}

void Cmd_Noclip_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	SV_LogPlayer(host_client, "noclip cheat");
	if (sv_player->v->movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v->movetype = MOVETYPE_NOCLIP;
		sv_player->v->solid = SOLID_TRIGGER;
		SV_ClientPrintf (host_client, PRINT_HIGH, "noclip ON\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		if (sv_player->v->health > 0)
			sv_player->v->solid = SOLID_SLIDEBOX;
		else
			sv_player->v->solid = SOLID_NOT;
		SV_ClientPrintf (host_client, PRINT_HIGH, "noclip OFF\n");
	}
}

void Cmd_Fly_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	SV_LogPlayer(host_client, "fly cheat");
	if (sv_player->v->movetype != MOVETYPE_FLY)
	{
		sv_player->v->movetype = MOVETYPE_FLY;
		SV_ClientPrintf (host_client, PRINT_HIGH, "flymode ON\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		if (sv_player->v->health > 0)
			sv_player->v->solid = SOLID_SLIDEBOX;
		else
			sv_player->v->solid = SOLID_NOT;
		SV_ClientPrintf (host_client, PRINT_HIGH, "flymode OFF\n");
	}
}

/*
====================
Host_SetPos_f  UDC
By Alex Shadowalker (and added to fte because he kept winging)
====================
*/
void Cmd_SetPos_f(void)
{
	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if (Cmd_Argc() != 4)
	{
		Con_Printf ("setpos %i %i %i\n", (int)sv_player->v->origin[0], (int)sv_player->v->origin[1], (int)sv_player->v->origin[2]);
		return;
	}
	SV_LogPlayer(host_client, "setpos cheat");
	if (sv_player->v->movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v->movetype = MOVETYPE_NOCLIP;
		Con_Printf("noclip on\n");
	}

	sv_player->v->origin[0] = atof(Cmd_Argv(1));
	sv_player->v->origin[1] = atof(Cmd_Argv(2));
	sv_player->v->origin[2] = atof(Cmd_Argv(3));
	SV_LinkEdict (sv_player, false);
}

void ED_ClearEdict (progfuncs_t *progfuncs, edict_t *e);
void SV_SetUpClientEdict (client_t *cl, edict_t *ent)
{
	extern int pr_teamfield;
#ifdef VM_Q1
	if (svs.gametype == GT_Q1QVM)
	{
		string_t preserve;
		preserve = ent->v->netname;
		Q1QVMED_ClearEdict(ent, true);
		Con_Printf("client netname: %x\n", preserve);
		ent->v->netname = preserve;
	}
	else
#endif
	{
		if (progstype != PROG_NQ)	//allow frikbots to work in NQ mods (but not qw!)
			ED_ClearEdict(svprogfuncs, ent);
		ent->v->netname = PR_SetString(svprogfuncs, cl->name);
	}
	ED_Spawned(ent);
	ent->isfree = false;

	ent->v->colormap = NUM_FOR_EDICT(svprogfuncs, ent);

	if (pr_teamfield)
		((string_t *)ent->v)[pr_teamfield] = (string_t)(cl->team-svprogfuncs->stringtable);

	{
		int tc = atoi(Info_ValueForKey(cl->userinfo, "topcolor"));
		int bc = atoi(Info_ValueForKey(cl->userinfo, "bottomcolor"));
		if (tc < 0 || tc > 13)
			tc = 0;
		if (bc < 0 || bc > 13)
			bc = 0;
		ent->xv->clientcolors = 16*tc + bc;
	}


	ent->xv->gravity = cl->entgravity = 1.0;
	ent->xv->maxspeed = cl->maxspeed = sv_maxspeed.value;
	ent->v->movetype = MOVETYPE_NOCLIP;
}
/*
==================
Cmd_Join_f

Set client to player mode without reconnecting
==================
*/
void Cmd_Join_f (void)
{
	int		i;
	client_t	*cl;
	int		numclients;
	extern cvar_t	maxclients;

	if (host_client->state != cs_spawned)
		return;
	if (!host_client->spectator)
		return;		// already a player

	if (svs.gametype != GT_PROGS)
	{
		Con_Printf ("Sorry, not implemented in this gamecode type. Try moaning at the dev team\n");
		return;
	}

	if (!(host_client->zquake_extensions & Z_EXT_JOIN_OBSERVE))
	{
		Con_Printf ("Your QW client doesn't support this command\n");
		return;
	}

	if (password.string[0] && stricmp(password.string, "none"))
	{
		Con_Printf ("This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "player", "player");
		return;
	}

	// count players already on server
	numclients = 0;
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		if (cl->state != cs_free && !cl->spectator)
			numclients++;
	}
	if (numclients >= maxclients.value)
	{
		Con_Printf ("Can't join, all player slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	if (SpectatorDisconnect)
		PR_ExecuteProgram (svprogfuncs, SpectatorDisconnect);

	host_client->old_frags = 0;
	SV_SetUpClientEdict (host_client, host_client->edict);

	// turn the spectator into a player
	host_client->spectator = false;
	Info_RemoveKey (host_client->userinfo, "*spectator");
	Info_RemoveKey (host_client->userinfobasic, "*spectator");

	// FIXME, bump the client's userid?

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
	{
		if (spawnparamglobals[i])
			host_client->spawn_parms[i] = *spawnparamglobals[i];
		else
			host_client->spawn_parms[i] = 0;
	}

	// call the spawn function
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);

	// actually spawn the player
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PutClientInServer);

	// send notification to all clients
	host_client->sendinfo = true;

	SV_LogPlayer(host_client, "joined");
}


/*
==================
Cmd_Observe_f

Set client to spectator mode without reconnecting
==================
*/
void Cmd_Observe_f (void)
{
	int		i;
	client_t	*cl;
	int		numspectators;
	extern cvar_t	maxspectators, spectator_password;

	if (host_client->state != cs_spawned)
		return;
	if (host_client->spectator)
		return;		// already a spectator
	
	if (svs.gametype != GT_PROGS)
	{
		Con_Printf ("Sorry, not implemented in this gamecode type. Try moaning at the dev team\n");
		return;
	}

	if (!(host_client->zquake_extensions & Z_EXT_JOIN_OBSERVE))
	{
		Con_Printf ("Your QW client doesn't support this command\n");
		return;
	}

	if (spectator_password.string[0] && stricmp(spectator_password.string, "none"))
	{
		Con_Printf ("This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "spectator", "spectator");
		return;
	}

	// count spectators already on server
	numspectators = 0;
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++) {
		if (cl->state != cs_free && cl->spectator)
			numspectators++;
	}
	if (numspectators >= maxspectators.value)
	{
		Con_Printf ("Can't join, all spectator slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);

	host_client->old_frags = 0;
	SV_SetUpClientEdict (host_client, host_client->edict);

	// turn the player into a spectator
	host_client->spectator = true;
	Info_SetValueForStarKey (host_client->userinfo, "*spectator", "1", sizeof(host_client->userinfo));
	Info_SetValueForStarKey (host_client->userinfobasic, "*spectator", "1", sizeof(host_client->userinfobasic));

	// FIXME, bump the client's userid?

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
	{
		if (spawnparamglobals[i])
			host_client->spawn_parms[i] = *spawnparamglobals[i];
		else
			host_client->spawn_parms[i] = 0;
	}

	SV_SpawnSpectator ();

	// call the spawn function
	if (SpectatorConnect)
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
	}
	else
		sv_player->v->movetype = MOVETYPE_NOCLIP;

	// send notification to all clients
	host_client->sendinfo = true;

	SV_LogPlayer(host_client, "observing");
}

void Cmd_FPSList_f(void)
{
	client_t *cl;
	int c;
	int f;
	double minf = DBL_MIN, maxf = DBL_MAX, this;
	double ftime;
	int frames;


	for (c = 0; c < sv.allocated_client_slots; c++)
	{
		cl = &svs.clients[c];
		ftime = 0;
		frames = 0;

		if (!cl->state)
			continue;

		if (cl->frameunion.frames)
		{
			for (f = 0; f < UPDATE_BACKUP; f++)
			{
				if (cl->frameunion.frames[f].move_msecs >= 0)
				{
					this = 1000.0f/cl->frameunion.frames[f].move_msecs;
					ftime += this;
					if (minf < this)
						minf = this;
					if (maxf > this)
						maxf = this;
					frames++;
				}
			}
		}

		if (frames)
			Con_Printf("%s: %ffps (min%f max %f\n", cl->name, ftime/frames, minf, maxf);
		else
			Con_Printf("%s: no information available\n", cl->name);
	}
}

void SV_EnableClientsCSQC(void)
{
#ifdef PEXT_CSQC
	if (host_client->fteprotocolextensions & PEXT_CSQC || atoi(Cmd_Argv(1)))
		host_client->csqcactive = true;
	else
		Con_Printf("CSQC entities not enabled - no support from network protocol\n");
#endif
}
void SV_DisableClientsCSQC(void)
{
#ifdef PEXT_CSQC
	host_client->csqcactive = false;
#endif
}

void SV_MVDList_f (void);
void SV_MVDInfo_f (void);
typedef struct
{
	char	*name;
	void	(*func) (void);
	qboolean	noqchandling;
} ucmd_t;

ucmd_t ucmds[] =
{
	{"new", SV_New_f, true},
	{"pk3list",	SV_PK3List_f, true},
	{"modellist", SV_Modellist_f, true},
	{"soundlist", SV_Soundlist_f, true},
	{"prespawn", SV_PreSpawn_f, true},
	{"spawn", SV_Spawn_f, true},
	{"begin", SV_Begin_f, true},

	{"join", Cmd_Join_f},
	{"observe", Cmd_Observe_f},

	{"drop", SV_Drop_f},
	{"disconnect", SV_Drop_f},
	{"pings", SV_Pings_f},

// issued by hand at client consoles
	{"rate", SV_Rate_f},
	{"kill", SV_Kill_f},
	{"pause", SV_Pause_f},
	{"msg", SV_Msg_f},

	{"sayone", SV_SayOne_f},
	{"say", SV_Say_f},
	{"say_team", SV_Say_Team_f},

	{"setinfo", SV_SetInfo_f},

	{"serverinfo", SV_ShowServerinfo_f},

	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},

	{"ptrack", SV_PTrack_f}, //ZOID - used with autocam
	{"enablecsqc", SV_EnableClientsCSQC},
	{"disablecsqc", SV_DisableClientsCSQC},

	{"snap", SV_NoSnap_f},
	{"vote", SV_Vote_f},

#ifdef SVRANKING
	{"topten", Rank_ListTop10_f},
#endif

	{"efpslist", Cmd_FPSList_f},	//don't conflict with the ktpro one
	{"god", Cmd_God_f},
	{"give", Cmd_Give_f},
	{"noclip", Cmd_Noclip_f},
	{"fly", Cmd_Fly_f},
	{"notarget", Cmd_Notarget_f},
	{"setpos", Cmd_SetPos_f},

	{"stopdownload", SV_StopDownload_f},
	{"demolist", SV_MVDList_f},
	{"demoinfo", SV_MVDInfo_f},

	{NULL, NULL}
};

#ifdef Q2SERVER
ucmd_t ucmdsq2[] = {
	{"new", SV_New_f, true},
	{"configstrings", SVQ2_ConfigStrings_f, true},
	{"baselines", SVQ2_BaseLines_f, true},
	{"begin", SV_Begin_f, true},

//	{"setinfo", SV_SetInfo_f, true},

	{"serverinfo", SV_ShowServerinfo_f, true},
	{"info", SV_ShowServerinfo_f, true},

	{"download", SV_BeginDownload_f, true},
	{"nextdl", SV_NextDownload_f, true},

	{"nextserver", SVQ2_NextServer_f, true},

	{"vote", SV_Vote_f, true},

//#ifdef SVRANKING
//	{"topten", Rank_ListTop10_f, true},
//#endif

	{"drop", SV_Drop_f, true},
	{"disconnect", SV_Drop_f, true},

	{NULL, NULL}
};
#endif

extern ucmd_t nqucmds[];
/*
==================
SV_ExecuteUserCommand
==================
*/
void SV_ExecuteUserCommand (char *s, qboolean fromQC)
{
	ucmd_t	*u;
	client_t *oldhost = host_client;

	Con_DPrintf("Client command: %s\n", s);

	Cmd_TokenizeString (s, false, false);
	sv_player = host_client->edict;

	Cmd_ExecLevel=1;

	if (host_client->controlled && atoi(Cmd_Argv(0))>0)	//now see if it's meant to be from a slave client
	{
		int pnum = atoi(Cmd_Argv(0));
		client_t *s;
		for (s = host_client; s; s = s->controlled)
		{
			if (!--pnum)
			{
				host_client = s;
				break;
			}
		}
		sv_player = host_client->edict;
		Cmd_ShiftArgs(1, false);
	}

#ifdef Q2SERVER
	if (ISQ2CLIENT(host_client))
		u = ucmdsq2;
	else
#endif
#ifdef NQPROT
	if (ISNQCLIENT(host_client))
		u = nqucmds;
	else
#endif
		u=ucmds;

	for ( ; u->name ; u++)
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			if (!fromQC && !u->noqchandling)
				if (PR_KrimzonParseCommand(s))	//KRIMZON_SV_PARSECLIENTCOMMAND has the opertunity to parse out certain commands.
				{
					host_client = oldhost;
					return;
				}
			SV_BeginRedirect (RD_CLIENT, host_client->language);
			if (u->func)
				u->func ();
			host_client = oldhost;
			SV_EndRedirect ();
			return;
		}

	if (!u->name)
	{
		if (!fromQC)
			if (PR_UserCmd(s))			//Q2 and MVDSV command handling only happens if the engine didn't recognise it.
			{
				host_client = oldhost;
				return;
			}
#ifdef SVRANKING
		if (sv_cmdlikercon.value && host_client->rankid)
		{
			char remaining[1024];
			int i;
			rankstats_t stats;

			if (!Rank_GetPlayerStats(host_client->rankid, &stats))
			{
				host_client = oldhost;
				return;
			}

			Con_Printf ("cmd from %s:\n%s\n"
				, host_client->name, net_message.data+4);

			SV_BeginRedirect (RD_CLIENT, host_client->language);

			remaining[0] = 0;

			for (i=0 ; i<Cmd_Argc() ; i++)
			{
				if (strlen(remaining)+strlen(Cmd_Argv(i))>=sizeof(remaining)-1)
				{
					Con_Printf("cmd was too long\n");
					host_client = oldhost;
					SV_EndRedirect ();
					Con_Printf ("cmd from %s:\n%s\n"
						, NET_AdrToString (net_from), "Was too long - possible buffer overflow attempt");
					return;
				}
				strcat (remaining, Cmd_Argv(i) );
				strcat (remaining, " ");
			}

			Cmd_ExecuteString (remaining, stats.trustlevel);
			host_client = oldhost;
			SV_EndRedirect ();
			return;
		}
#endif
		Con_Printf ("Bad user command: %s\n", Cmd_Argv(0));
	}

	host_client = oldhost;
	SV_EndRedirect ();
}
#ifdef NQPROT
void SVNQ_Spawn_f (void)
{
	extern cvar_t sv_gravity;
	int		i;
	client_t	*client;
	edict_t	*ent;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("spawn not valid -- already spawned\n");
		return;
	}

// send all current names, colors, and frag counts
	// FIXME: is this a good thing?
	SZ_Clear (&host_client->netchan.message);

// send current status of all other players

	// normally this could overflow, but no need to check due to backbuf
	for (i=0, client = svs.clients; i<sv.allocated_client_slots ; i++, client++)
		SV_FullClientUpdateToClient (client, host_client);

// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (i >= MAX_STANDARDLIGHTSTYLES && host_client->protocol != SCP_DARKPLACES7)
			break;	//dp7 clients support more lightstyles.

		ClientReliableWrite_Begin (host_client, svc_lightstyle,
			3 + (sv.strings.lightstyles[i] ? strlen(sv.strings.lightstyles[i]) : 1));
		ClientReliableWrite_Byte (host_client, (char)i);
		ClientReliableWrite_String (host_client, sv.strings.lightstyles[i]);
	}

	// set up the edict
	ent = host_client->edict;

	if (host_client->istobeloaded)	//minimal setup
	{
		host_client->entgravity = ent->xv->gravity*sv_gravity.value;
		host_client->maxspeed = ent->xv->maxspeed;
	}
	else
	{
		memset (ent->v, 0, pr_edict_size);
		ED_Spawned(ent);

		ent->v->colormap = NUM_FOR_EDICT(svprogfuncs, ent);
		ent->v->team = 0;	// FIXME
		ent->v->netname = PR_SetString(svprogfuncs, host_client->name);

		host_client->entgravity = ent->xv->gravity = 1.0;
		host_client->entgravity*=sv_gravity.value;
		host_client->maxspeed = ent->xv->maxspeed = sv_maxspeed.value;
	}

//
// force stats to be updated
//
	memset (host_client->statsi, 0, sizeof(host_client->statsi));
	memset (host_client->statsf, 0, sizeof(host_client->statsf));
	memset (host_client->statss, 0, sizeof(host_client->statss));

	ClientReliableWrite_Begin (host_client, svc_updatestat, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALSECRETS);
	ClientReliableWrite_Long (host_client, pr_global_struct->total_secrets);

	ClientReliableWrite_Begin (host_client, svc_updatestat, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALMONSTERS);
	ClientReliableWrite_Long (host_client, pr_global_struct->total_monsters);

	ClientReliableWrite_Begin (host_client, svc_updatestat, 6);
	ClientReliableWrite_Byte (host_client, STAT_SECRETS);
	ClientReliableWrite_Long (host_client, pr_global_struct->found_secrets);

	ClientReliableWrite_Begin (host_client, svc_updatestat, 6);
	ClientReliableWrite_Byte (host_client, STAT_MONSTERS);
	ClientReliableWrite_Long (host_client, pr_global_struct->killed_monsters);



	SZ_Write (&host_client->netchan.message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
	MSG_WriteByte (&host_client->netchan.message, 3);


	host_client->send_message = true;

}
void SVNQ_Begin_f (void)
{
	unsigned pmodel = 0, emodel = 0;
	int		i;
	qboolean sendangles=false;

	if (host_client->state == cs_spawned)
		return; // don't begin again

	host_client->state = cs_spawned;

	if (host_client->istobeloaded)
	{
		sendangles = true;
		host_client->istobeloaded = false;
	}
	else
	{
		if (host_client->spectator)
		{
			SV_SpawnSpectator ();

			if (SpectatorConnect)
			{
				// copy spawn parms out of the client_t
				for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
				{
					if (spawnparamglobals[i])
						*spawnparamglobals[i] = host_client->spawn_parms[i];
				}

				// call the spawn function
				pr_global_struct->time = sv.time;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
				PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
			}
		}
		else
		{
			// copy spawn parms out of the client_t
			for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			{
				if (spawnparamglobals[i])
					*spawnparamglobals[i] = host_client->spawn_parms[i];
			}

			// call the spawn function
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);

			// actually spawn the player
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->PutClientInServer);
		}
	}

	// clear the net statistics, because connecting gives a bogus picture
	host_client->netchan.frame_latency = 0;
	host_client->netchan.frame_rate = 0;
	host_client->netchan.drop_count = 0;
	host_client->netchan.good_count = 0;

	//check he's not cheating

	if (sv_playermodelchecks.value)
	{
		pmodel = atoi(Info_ValueForKey (host_client->userinfo, "pmodel"));
		emodel = atoi(Info_ValueForKey (host_client->userinfo, "emodel"));

		if (pmodel != sv.model_player_checksum ||
			emodel != sv.eyes_player_checksum)
			SV_BroadcastTPrintf (PRINT_HIGH, STL_POSSIBLEMODELCHEAT, host_client->name);
	}


	// if we are paused, tell the client
	if (sv.paused)
	{
		if (!ISQ2CLIENT(host_client))
		{
			ClientReliableWrite_Begin (host_client, svc_setpause, 2);
			ClientReliableWrite_Byte (host_client, sv.paused);
		}
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_SERVERPAUSED);
	}

	if (sendangles)
	{
//
// send a fixangle over the reliable channel to make sure it gets there
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
		MSG_WriteByte (&host_client->netchan.message, svc_setangle);
		MSG_WriteAngle (&host_client->netchan.message, 0 );
		MSG_WriteAngle (&host_client->netchan.message, host_client->edict->v->angles[1] );
		MSG_WriteAngle (&host_client->netchan.message, 0 );
	}



	SZ_Write (&host_client->netchan.message, sv.signon.data, sv.signon.cursize);
//	MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
//	MSG_WriteByte (&host_client->netchan.message, 4);


	host_client->send_message = true;
}
void SVNQ_PreSpawn_f (void)
{
	edict_t *ent;
	entity_state_t *state;
	int i, e;
	if (host_client->state != cs_connected)
	{
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	for (e = 1; e < sv.num_edicts; e++)
	{
		ent = EDICT_NUM(svprogfuncs, e);
		state = &ent->baseline;

		if (!state->number || !state->modelindex)
		{	//ent doesn't have a baseline
			continue;
		}

		if (!ent)
		{
			MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline);

			MSG_WriteShort (&host_client->netchan.message, e);

			MSG_WriteByte (&host_client->netchan.message, 0);

			MSG_WriteByte (&host_client->netchan.message, 0);
			MSG_WriteByte (&host_client->netchan.message, 0);
			MSG_WriteByte (&host_client->netchan.message, 0);
			for (i=0 ; i<3 ; i++)
			{
				MSG_WriteCoord(&host_client->netchan.message, 0);
				MSG_WriteAngle(&host_client->netchan.message, 0);
			}
		}
		else
		{
			if (host_client->protocol != SCP_NETQUAKE && (state->modelindex > 255 || state->frame > 255))
			{
				MSG_WriteByte(&host_client->netchan.message, svcdp_spawnbaseline2);

				MSG_WriteShort (&host_client->netchan.message, e);

				MSG_WriteShort (&host_client->netchan.message, state->modelindex);
				MSG_WriteShort (&host_client->netchan.message, state->frame);
			}
			else
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline);

				MSG_WriteShort (&host_client->netchan.message, e);

				MSG_WriteByte (&host_client->netchan.message, state->modelindex&255);
				MSG_WriteByte (&host_client->netchan.message, state->frame&255);
			}

			MSG_WriteByte (&host_client->netchan.message, (int)state->colormap);
			MSG_WriteByte (&host_client->netchan.message, (int)state->skinnum);
			for (i=0 ; i<3 ; i++)
			{
				MSG_WriteCoord(&host_client->netchan.message, state->origin[i]);
				MSG_WriteAngle(&host_client->netchan.message, state->angles[i]);
			}
		}
	}

	for (i = 0; i < sv.num_signon_buffers; i++)
		SZ_Write (&host_client->netchan.message, sv.signon_buffers[i], sv.signon_buffer_size[i]);

	MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
	MSG_WriteByte (&host_client->netchan.message, 2);


	host_client->send_message = true;
}
void SVNQ_NQInfo_f (void)
{
	Cmd_TokenizeString(va("setinfo \"%s\" \"%s\"\n", Cmd_Argv(0), Cmd_Argv(1)), false, false);
	SV_SetInfo_f();
}

void SVNQ_NQColour_f (void)
{
	int top;
	int bottom;

	int playercolor;

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	playercolor = top*16 + bottom;

	if (progstype != PROG_QW)
		host_client->edict->v->team = bottom + 1;

	Info_SetValueForKey(host_client->userinfo, "topcolor", va("%i", top), sizeof(host_client->userinfo));
	Info_SetValueForKey(host_client->userinfo, "bottomcolor", va("%i", bottom), sizeof(host_client->userinfo));
	switch(bottom)
	{
	case 4:
		Info_SetValueForKey(host_client->userinfo, "team", "red", sizeof(host_client->userinfo));
		break;
	case 14:
		Info_SetValueForKey(host_client->userinfo, "team", "blue", sizeof(host_client->userinfo));
		break;
	default:
		Info_SetValueForKey(host_client->userinfo, "team", va("t%i", bottom+1), sizeof(host_client->userinfo));
		break;
	}

	SV_ExtractFromUserinfo (host_client);

	MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, "topcolor");
	MSG_WriteString (&sv.reliable_datagram, Info_ValueForKey(host_client->userinfo, "topcolor"));

	MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, "bottomcolor");
	MSG_WriteString (&sv.reliable_datagram, Info_ValueForKey(host_client->userinfo, "bottomcolor"));

	MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, "team");
	MSG_WriteString (&sv.reliable_datagram, Info_ValueForKey(host_client->userinfo, "team"));

	MSG_WriteByte (&sv.nqreliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.nqreliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.nqreliable_datagram, playercolor);
}

void SVNQ_Ping_f(void)
{
	int i;
	client_t *cl;

	Con_Printf ("Ping times:\n");
	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
		if (!cl->state)
			continue;

		Con_Printf ("%3i %s\n", SV_CalcPing (cl), cl->name);
	}
}

ucmd_t nqucmds[] =
{
	{"new",			SVNQ_New_f, true},

	{"status",		NULL},

	{"god",			Cmd_God_f},
	{"give",		Cmd_Give_f},
	{"notarget",	Cmd_Notarget_f},
	{"fly",			NULL},
	{"noclip",		Cmd_Noclip_f},
	{"pings",		SV_Pings_f},


	{"name",		SVNQ_NQInfo_f},
	{"say",			SV_Say_f},
	{"say_team",	SV_Say_Team_f},
	{"tell",		SV_SayOne_f},
	{"color",		SVNQ_NQColour_f},
	{"kill",		SV_Kill_f},
	{"pause",		SV_Pause_f},
	{"spawn",		SVNQ_Spawn_f, true},
	{"begin",		SVNQ_Begin_f, true},
	{"prespawn",	SVNQ_PreSpawn_f, true},
	{"kick",		NULL},
	{"ping",		SVNQ_Ping_f},
	{"ban",			NULL},
	{"vote",		SV_Vote_f},

	{"download",	SV_BeginDownload_f},
	{"sv_startdownload",	SVDP_StartDownload_f},

	{"setinfo", SV_SetInfo_f},
	{"playermodel",	NULL},
	{"playerskin",	NULL},
	{"rate",		SV_Rate_f},

#ifdef SVRANKING
	{"topten",		Rank_ListTop10_f},
#endif

	{NULL, NULL}
};
/*
void SVNQ_ExecuteUserCommand (char *s)
{
	client_t *oldhost = host_client;
	ucmd_t	*u;

	Cmd_TokenizeString (s, false, false);
	sv_player = host_client->edict;

	Cmd_ExecLevel=1;

	for (u=nqucmds ; u->name ; u++)
	{
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			if (/ *!fromQC && * /!u->noqchandling)
				if (PR_UserCmd(s))
				{
					host_client = oldhost;
					return;
				}

			if (!u->func)
			{
				SV_BeginRedirect (RD_CLIENT, host_client->language);
				Con_Printf("Command was disabled\n");
				SV_EndRedirect ();
			}
			else
			{
				SV_BeginRedirect (RD_CLIENT, host_client->language);
				u->func ();
				SV_EndRedirect ();
			}

			host_client = oldhost;
			return;
		}
	}

	if (!u->name)
		Con_Printf("%s tried to \"%s\"\n", host_client->name, s);
}
*/
#endif


int implevels[256];
qboolean SV_FiltureImpulse(int imp, int level)
{
	if (imp < 0 || imp > 255)
		return true;	//erm

	if (implevels[imp] > level)
		return false;

	return true;
}
void SV_FilterImpulseInit(void)
{
	char buffer[1024];
	char *s;
	int lev;
	int imp;
	memset(implevels, 0, sizeof(implevels));

	s = COM_LoadStackFile("impfiltr.cfg", buffer, sizeof(buffer));
	if (!s)
		Con_DPrintf("impfiltr.cfg not found. Impulse filters are disabled\n");

	while(s)
	{
		s = COM_Parse(s);
		if (!s)
			return;
		imp = atoi(com_token);
		s = COM_Parse(s);
		if (!s)
		{
			Con_Printf("Unexpected eof in impfiltr.cfg\n");
			return;
		}
		lev = atoi(com_token);
		if (imp > 255 || imp < 0 || lev < 0 || lev > RESTRICT_MAX)
			Con_Printf("impfiltr.cfg - bad paramters\n");
		else
			implevels[imp] = lev;
	}
}
/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/
#ifdef SERVERONLY
/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t	forward, right, up;
	float	sign;
	float	side;
	float	value;

	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);

	value = cl_rollangle.value;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;

	return side*sign;

}
#endif



//============================================================================

vec3_t	pmove_mins, pmove_maxs;

/*
====================
AddLinksToPmove

====================
*/
void AddLinksToPmove ( areanode_t *node )
{
	int Q1_HullPointContents (hull_t *hull, int num, vec3_t p);
	link_t		*l, *next;
	edict_t		*check;
	int			pl;
	int			i;
	physent_t	*pe;

	model_t *model;

	pl = EDICT_TO_PROG(svprogfuncs, sv_player);

	// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->v->owner == pl)
			continue;		// player's own missile
		if (check->v->solid == SOLID_BSP
			|| check->v->solid == SOLID_BBOX
			|| check->v->solid == SOLID_SLIDEBOX)
		{
			if (check == sv_player)
				continue;

			for (i=0 ; i<3 ; i++)
				if (check->v->absmin[i] > pmove_maxs[i]
				|| check->v->absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;

			if (pmove.numphysent == MAX_PHYSENTS)
				break;
			pe = &pmove.physents[pmove.numphysent];
			pe->notouch = !((int)sv_player->xv->dimension_solid & (int)check->xv->dimension_hit);
			if (!((int)sv_player->xv->dimension_hit & (int)check->xv->dimension_solid))
				continue;
			pmove.numphysent++;

			VectorCopy (check->v->origin, pe->origin);
			pe->info = NUM_FOR_EDICT(svprogfuncs, check);
			if (check->v->solid == SOLID_BSP)
			{
				if(progstype != PROG_H2)
					pe->angles[0]*=-1;	//quake is wierd. I guess someone fixed it hexen2... or my code is buggy or something...
				pe->model = sv.models[(int)(check->v->modelindex)];
				VectorCopy (check->v->angles, pe->angles);
			}
			else
			{
				pe->model = NULL;
				VectorCopy (check->v->mins, pe->mins);
				VectorCopy (check->v->maxs, pe->maxs);
				VectorClear (pe->angles);
			}
		}
	}
	if (sv_player->v->mins[2] != 24)	//crouching/dead
	for (l = node->trigger_edicts.next ; l != &node->trigger_edicts ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->v->owner == pl)
			continue;		// player's own missile
		if (check->v->solid == SOLID_LADDER)
		{
			for (i=0 ; i<3 ; i++)
				if (check->v->absmin[i] > pmove_maxs[i]
				|| check->v->absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;

			if (!((int)sv_player->xv->dimension_hit & (int)check->xv->dimension_solid))
				continue;

			model = sv.models[(int)check->v->modelindex];
			if (model)
	// test the point
			if ( model->funcs.PointContents (model, sv_player->v->origin) == FTECONTENTS_SOLID )
				sv_player->xv->fteflags = (int)sv_player->xv->fteflags | FF_LADDER;	//touch that ladder!
		}
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if ( pmove_maxs[node->axis] > node->dist )
		AddLinksToPmove ( node->children[0] );
	if ( pmove_mins[node->axis] < node->dist )
		AddLinksToPmove ( node->children[1] );
}


/*
================
AddAllEntsToPmove

For debugging
================
*/
void AddAllEntsToPmove (void)
{
	int			e;
	edict_t		*check;
	int			i;
	physent_t	*pe;
	int			pl;

	pl = EDICT_TO_PROG(svprogfuncs, sv_player);
	for (e=1 ; e<sv.num_edicts ; e++)
	{
		check = EDICT_NUM(svprogfuncs, e);
		if (check->isfree)
			continue;
		if (check->v->owner == pl)
			continue;
		if (check->v->solid == SOLID_BSP
			|| check->v->solid == SOLID_BBOX
			|| check->v->solid == SOLID_SLIDEBOX)
		{
			if (check == sv_player)
				continue;

			for (i=0 ; i<3 ; i++)
				if (check->v->absmin[i] > pmove_maxs[i]
				|| check->v->absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;
			pe = &pmove.physents[pmove.numphysent];

			VectorCopy (check->v->origin, pe->origin);
			pmove.physents[pmove.numphysent].info = e;
			if (check->v->solid == SOLID_BSP)
			{
				VectorCopy (check->v->angles, pe->angles);
				pe->model = sv.models[(int)(check->v->modelindex)];
			}
			else
			{
				pe->angles[0] = pe->angles[1] = pe->angles[2] = 0;
				pe->model = NULL;
				VectorCopy (check->v->mins, pe->mins);
				VectorCopy (check->v->maxs, pe->maxs);
			}

			if (++pmove.numphysent == MAX_PHYSENTS)
				break;
		}
	}
}

int SV_PMTypeForClient (client_t *cl)
{
	if (sv.demostatevalid)
	{	//force noclip... This does create problems for closing demos.
		if (cl->zquake_extensions & Z_EXT_PM_TYPE_NEW)
			return PM_SPECTATOR;
		return PM_OLD_SPECTATOR;
	}

	if (sv_brokenmovetypes.value)	//this is to mimic standard qw servers, which don't support movetypes other than MOVETYPE_FLY.
	{								//it prevents bugs from being visible in unsuspecting mods.
		if (cl->spectator)
		{
			if (cl->zquake_extensions & Z_EXT_PM_TYPE_NEW)
				return PM_SPECTATOR;
			return PM_OLD_SPECTATOR;
		}

		if (cl->edict->v->health <= 0)
			return PM_DEAD;
		return PM_NORMAL;
	}

	if (cl->edict->v->movetype == MOVETYPE_NOCLIP)
	{
		if (cl->zquake_extensions & Z_EXT_PM_TYPE_NEW)
			return PM_SPECTATOR;
		return PM_OLD_SPECTATOR;
	}

	if (cl->edict->v->movetype == MOVETYPE_FLY)
		return PM_FLY;

	if (cl->edict->v->movetype == MOVETYPE_NONE)
		return PM_NONE;

	if (cl->edict->v->health <= 0)
		return PM_DEAD;

	return PM_NORMAL;
}


//called for common csqc/server code (supposedly)
void SV_RunEntity (edict_t *ent);

/*
===========
SV_PreRunCmd
===========
Done before running a player command.  Clears the touch array
*/
qbyte playertouch[(MAX_EDICTS+7)/8];

void SV_PreRunCmd(void)
{
	memset(playertouch, 0, sizeof(playertouch));
}

/*
===========
SV_RunCmd
===========
*/
void SV_RunCmd (usercmd_t *ucmd, qboolean recurse)
{
	edict_t		*ent;
	int			i, n;
	int			oldmsec;
	double  tmp_time;
	qboolean jumpable;


	// DMW copied this KK hack copied from QuakeForge anti-cheat
	// (also extra inside parm on all SV_RunCmds that follow)

	// To prevent a infinite loop
	if (!recurse)
	{
		host_client->msecs += ucmd->msec;

		if ((tmp_time = realtime - host_client->last_check) >= sv_cheatspeedchecktime.value)
		{
			tmp_time = tmp_time * 1000.0 * sv_cheatpc.value/100.0;
		    if (host_client->msecs > tmp_time)
			{
				host_client->msec_cheating++;
				SV_BroadcastTPrintf(PRINT_HIGH,
						STL_SPEEDCHEATPOSSIBLE,
							host_client->msecs, tmp_time,
							host_client->msec_cheating, host_client->name);

				if (host_client->msec_cheating >= 2)
				{
					SV_BroadcastTPrintf(PRINT_HIGH,
							STL_SPEEDCHEATKICKED,
								host_client->name, NET_AdrToString(host_client->netchan.remote_address));
					host_client->drop = true;	//drop later
				}
		    }

		    host_client->msecs = 0;
		    host_client->last_check = realtime;
		}
	}
	// end KK hack copied from QuakeForge anti-cheat
	//it's amazing how code get's copied around...


	cmd = *ucmd;

	// chop up very long commands
	if (cmd.msec > 50)
	{
		oldmsec = ucmd->msec;
		cmd.msec = oldmsec/2;
		SV_RunCmd (&cmd, true);
		cmd.msec = oldmsec/2 + (oldmsec&1);	//give them back their msec.
		cmd.impulse = 0;
		SV_RunCmd (&cmd, true);
		return;
	}

	host_frametime = ucmd->msec * 0.001;
	host_frametime *= sv.gamespeed;
	if (host_frametime > 0.1)
		host_frametime = 0.1;

	if (sv.demostatevalid)
	{	//spectators watching MVDs do not affect the running progs.
		player_mins[0] = -16;
		player_mins[1] = -16;
		player_mins[2] = -24;

		player_maxs[0] = 16;
		player_maxs[1] = 16;
		player_maxs[2] = 32;

		pmove.angles[0] = SHORT2ANGLE(ucmd->angles[0]);
		pmove.angles[1] = SHORT2ANGLE(ucmd->angles[1]);
		pmove.angles[2] = SHORT2ANGLE(ucmd->angles[2]);

		VectorCopy (host_client->specorigin, pmove.origin);
		VectorCopy (host_client->specvelocity, pmove.velocity);

		if (host_client->zquake_extensions & Z_EXT_PM_TYPE_NEW)
			pmove.pm_type = PM_SPECTATOR;
		else
			pmove.pm_type = PM_OLD_SPECTATOR;
		pmove.jump_held = host_client->jump_held;
		pmove.jump_msec = 0;
		pmove.waterjumptime = 0;
		pmove.numphysent = 1;
		pmove.physents[0].model = sv.worldmodel;
		pmove.cmd = *ucmd;
		pmove.hullnum = SV_HullNumForPlayer(0, player_mins, player_maxs);

		movevars.entgravity = 0;
		movevars.maxspeed = 0;
		movevars.bunnyspeedcap = pm_bunnyspeedcap.value;
		movevars.ktjump = pm_ktjump.value;
		movevars.slidefix = (pm_slidefix.value != 0);
		movevars.airstep = (pm_airstep.value != 0);
		movevars.walljump = (pm_walljump.value);
		movevars.slidyslopes = (pm_slidyslopes.value!=0);

		for (i=0 ; i<3 ; i++)
		{
			pmove_mins[i] = pmove.origin[i] - 256;
			pmove_maxs[i] = pmove.origin[i] + 256;
		}

		PM_PlayerMove (sv.gamespeed);

		VectorCopy (pmove.origin, host_client->specorigin);
		VectorCopy (pmove.velocity, host_client->specvelocity);

		return;
	}

#ifdef SVCHAT
	if (SV_ChatMove(ucmd->impulse))
	{
		ucmd->buttons = 0;
		ucmd->impulse = 0;
		ucmd->forwardmove = ucmd->sidemove = ucmd->upmove = 0;
	}
#endif

	if (!sv_player->v->fixangle)
	{
		sv_player->v->v_angle[0] = SHORT2ANGLE(ucmd->angles[0]);
		sv_player->v->v_angle[1] = SHORT2ANGLE(ucmd->angles[1]);
		sv_player->v->v_angle[2] = SHORT2ANGLE(ucmd->angles[2]);
	}

	if (progstype == PROG_H2)
		sv_player->xv->light_level = 128;	//hmm... HACK!!!

	sv_player->v->button0 = ucmd->buttons & 1;
	sv_player->v->button2 = (ucmd->buttons >> 1) & 1;
	if (pr_allowbutton1.value)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
		sv_player->v->button1 = ((ucmd->buttons >> 2) & 1);
// DP_INPUTBUTTONS
	sv_player->xv->button3 = ((ucmd->buttons >> 2) & 1);
	sv_player->xv->button4 = ((ucmd->buttons >> 3) & 1);
	sv_player->xv->button5 = ((ucmd->buttons >> 4) & 1);
	sv_player->xv->button6 = ((ucmd->buttons >> 5) & 1);
	sv_player->xv->button7 = ((ucmd->buttons >> 6) & 1);
	sv_player->xv->button8 = ((ucmd->buttons >> 7) & 1);
	if (ucmd->impulse && SV_FiltureImpulse(ucmd->impulse, host_client->trustlevel))
		sv_player->v->impulse = ucmd->impulse;

	if (host_client->iscuffed)
	{
		sv_player->v->impulse = 0;
		sv_player->v->button0 = 0;
	}

	if (host_client->state && host_client->protocol != SCP_BAD)
	{
		sv_player->xv->movement[0] = ucmd->forwardmove * host_frametime;
		sv_player->xv->movement[1] = ucmd->sidemove * host_frametime;
		sv_player->xv->movement[2] = ucmd->upmove * host_frametime;
	}

	SV_CheckVelocity(sv_player);

//
// angles
// show 1/3 the pitch angle and all the roll angle
	if (sv_player->v->health > 0)
	{
		if (!sv_player->v->fixangle)
		{
			sv_player->v->angles[PITCH] = -sv_player->v->v_angle[PITCH]/3;
			sv_player->v->angles[YAW] = sv_player->v->v_angle[YAW];
		}
		sv_player->v->angles[ROLL] =
			V_CalcRoll (sv_player->v->angles, sv_player->v->velocity)*4;
	}

	if (SV_PlayerPhysicsQC)
	{	//csqc independant physics support
		pr_global_struct->frametime = host_frametime;
		pr_global_struct->time = sv.time;
		SV_RunEntity(sv_player);
		return;
	}


	if (!host_client->spectator)
	{
		vec_t oldvz;
		pr_global_struct->frametime = host_frametime;

		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		oldvz = sv_player->v->velocity[2];
		if (progstype != PROG_QW)
		{
#define FL_JUMPRELEASED 4096
			jumpable = (int)sv_player->v->flags & FL_JUMPRELEASED;

			pmove.waterjumptime = sv_player->v->teleport_time;
			if (pmove.waterjumptime > sv.time)
				sv_player->v->flags = (int)sv_player->v->flags | FL_WATERJUMP;
			sv_player->v->teleport_time = sv.time + pmove.waterjumptime;
		}
		else
			jumpable = false;
		
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_PlayerPreThink();
		else
#endif
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPreThink);

		if (progstype != PROG_QW)
		{
			if (sv_player->v->velocity[2] == 225 && sv_player->v->teleport_time > sv.time)
			{
				sv_player->v->velocity[2] = oldvz;
		//		Con_Printf("Waterjump detected\n");
			}
			sv_player->v->flags = (int)sv_player->v->flags & ~FL_WATERJUMP;
			sv_player->v->teleport_time = pmove.waterjumptime;
			if (jumpable && !((int)sv_player->v->flags & FL_JUMPRELEASED))	//hmm... a jump was hit.
				sv_player->v->velocity[2] -= 270;
		}

		SV_RunThink (sv_player);
	}

//	memset(&pmove, 0, sizeof(pmove));
//	memset(&movevars, 0, sizeof(movevars));

	player_mins[0] = sv_player->v->mins[0];
	player_mins[1] = sv_player->v->mins[1];
	player_mins[2] = sv_player->v->mins[2];

	player_maxs[0] = sv_player->v->maxs[0];
	player_maxs[1] = sv_player->v->maxs[1];
	player_maxs[2] = sv_player->v->maxs[2];

	for (i=0 ; i<3 ; i++)
		pmove.origin[i] = sv_player->v->origin[i];// + (sv_player->v->mins[i] - player_mins[i]);

	VectorCopy (sv_player->v->velocity, pmove.velocity);
	VectorCopy (sv_player->v->v_angle, pmove.angles);

	pmove.pm_type = SV_PMTypeForClient (host_client);
	pmove.jump_held = host_client->jump_held;
	pmove.jump_msec = 0;
	if (progstype != PROG_QW)	//this is just annoying.
		pmove.waterjumptime = sv_player->v->teleport_time - sv.time;
	else
		pmove.waterjumptime = sv_player->v->teleport_time;
	pmove.numphysent = 1;
	pmove.physents[0].model = sv.worldmodel;
	pmove.cmd = *ucmd;
	if (sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3)
		pmove.hullnum = ((int)sv_player->xv->fteflags&FF_CROUCHING)?3:1;
	else
		pmove.hullnum = SV_HullNumForPlayer(sv_player->xv->hull, sv_player->v->mins, sv_player->v->maxs);

	movevars.entgravity = host_client->entgravity/movevars.gravity;
	movevars.maxspeed = host_client->maxspeed;
	movevars.bunnyspeedcap = pm_bunnyspeedcap.value;
	movevars.ktjump = pm_ktjump.value;
	movevars.slidefix = (pm_slidefix.value != 0);
	movevars.airstep = (pm_airstep.value != 0);
	movevars.walljump = (pm_walljump.value);
	movevars.slidyslopes = (pm_slidyslopes.value!=0);

	if (sv_player->xv->hasted)
		movevars.maxspeed*=sv_player->xv->hasted;

	for (i=0 ; i<3 ; i++)
	{
		pmove_mins[i] = pmove.origin[i] - 256;
		pmove_maxs[i] = pmove.origin[i] + 256;
	}
	sv_player->xv->fteflags = (int)sv_player->xv->fteflags & ~FF_LADDER;	//assume not touching ladder trigger
#if 1
	AddLinksToPmove ( sv_areanodes );
#else
	AddAllEntsToPmove ();
#endif

	if ((int)sv_player->xv->fteflags & FF_LADDER)
		pmove.onladder = true;
	else
		pmove.onladder = false;

#if 0
{
	int before, after;

before = PM_TestPlayerPosition (pmove.origin);
	PlayerMove ();
after = PM_TestPlayerPosition (pmove.origin);

if (sv_player->v->health > 0 && before && !after )
	Con_Printf ("player %s got stuck in playermove!!!!\n", host_client->name);
}
#else
	PM_PlayerMove (sv.gamespeed);
#endif

	host_client->jump_held = pmove.jump_held;
	if (progstype != PROG_QW)	//this is just annoying.
		sv_player->v->teleport_time = pmove.waterjumptime + sv.time;
	else
		sv_player->v->teleport_time = pmove.waterjumptime;
	sv_player->v->waterlevel = pmove.waterlevel;

	if (pmove.watertype & FTECONTENTS_SOLID)
		sv_player->v->watertype = Q1CONTENTS_SOLID;
	else if (pmove.watertype & FTECONTENTS_SKY)
		sv_player->v->watertype = Q1CONTENTS_SKY;
	else if (pmove.watertype & FTECONTENTS_LAVA)
		sv_player->v->watertype = Q1CONTENTS_LAVA;
	else if (pmove.watertype & FTECONTENTS_SLIME)
		sv_player->v->watertype = Q1CONTENTS_SLIME;
	else if (pmove.watertype & FTECONTENTS_WATER)
		sv_player->v->watertype = Q1CONTENTS_WATER;
	else
		sv_player->v->watertype = Q1CONTENTS_EMPTY;

	if (pmove.onground)
	{
		sv_player->v->flags = (int)sv_player->v->flags | FL_ONGROUND;
		sv_player->v->groundentity = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, pmove.physents[pmove.groundent].info));
	}
	else
		sv_player->v->flags = (int)sv_player->v->flags & ~FL_ONGROUND;

	for (i=0 ; i<3 ; i++)
		sv_player->v->origin[i] = pmove.origin[i];// - (sv_player->v->mins[i] - player_mins[i]);

#if 0
	// truncate velocity the same way the net protocol will
	for (i=0 ; i<3 ; i++)
		sv_player->v->velocity[i] = (int)pmove.velocity[i];
#else
	VectorCopy (pmove.velocity, sv_player->v->velocity);
#endif

	VectorCopy (pmove.angles, sv_player->v->v_angle);

	player_mins[0] = -16;
	player_mins[1] = -16;
	player_mins[2] = -24;

	player_maxs[0] = 16;
	player_maxs[1] = 16;
	player_maxs[2] = 32;

	if (!host_client->spectator)
	{
		// link into place and touch triggers
		SV_LinkEdict (sv_player, true);

/*		for (i = 0; i < pmove.numphysent; i++)
		{

		}
*/
		// touch other objects
		for (i=0 ; i<pmove.numtouch ; i++)
		{
			if (sv_pushplayers.value)
			{
				n = pmove.physents[pmove.touchindex[i]].info;
				if (n && n <= sv.allocated_client_slots)
				{
					float vel;
					vec3_t dir;
					vec3_t svel;
					ent = EDICT_NUM(svprogfuncs, n);
					VectorSubtract(ent->v->origin, sv_player->v->origin, dir);
					VectorNormalize(dir);
					VectorCopy(sv_player->v->velocity, svel);
					VectorNormalize(svel);
					vel = DotProduct(svel, dir);
					VectorMA(ent->v->velocity, sv_pushplayers.value*vel, dir, ent->v->velocity);
				}
			}


			if (pmove.physents[pmove.touchindex[i]].notouch)
				continue;
			n = pmove.physents[pmove.touchindex[i]].info;
			ent = EDICT_NUM(svprogfuncs, n);
			if (!ent->v->touch || (playertouch[n/8]&(1<<(n%8))))
				continue;

			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
			pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, sv_player);
			pr_global_struct->time = sv.time;
#ifdef VM_Q1
			if (svs.gametype == GT_Q1QVM)
				Q1QVM_Touch();
			else
#endif
				PR_ExecuteProgram (svprogfuncs, ent->v->touch);
			playertouch[n/8] |= 1 << (n%8);
		}
	}
}

/*
===========
SV_PostRunCmd
===========
Done after running a player command.
*/
void SV_PostRunCmd(void)
{
	// run post-think

	if (!svprogfuncs)
		return;

#ifdef VM_Q1
	if (svs.gametype == GT_Q1QVM)
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		Q1QVM_PostThink();
	}
	else
#endif
	{
		if (!host_client->spectator)
		{
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

			PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPostThink);

			SV_RunNewmis ();
		}
		else if (SpectatorThink)
		{
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, SpectatorThink);
		}
	}
}

/*
===================
SV_ExecuteClientMessage

The current net_message is parsed for the given client
===================
*/
void SV_ClientThink (void);
void SV_ExecuteClientMessage (client_t *cl)
{
	client_t *split;
	int		c;
	char	*s;
	usercmd_t	oldest, oldcmd, newcmd;
	client_frame_t	*frame;
	vec3_t o;
	qboolean	move_issued = false; //only allow one move command
	int		checksumIndex;
	qbyte	checksum, calculatedChecksum;
	int		seq_hash, i;

	// calc ping time
	if (cl->frameunion.frames)
	{	//split screen doesn't always have frames.
		frame = &cl->frameunion.frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];

		if (cl->lastsequence_acknoledged + UPDATE_BACKUP > cl->netchan.incoming_acknowledged)
		{
			frame->ping_time = realtime - frame->senttime;	//no more phenomanally low pings please

			if (cl->spectator)
				cl->delay = 0;
			else
			{
				if (frame->ping_time*1000 > sv_minping.value+1)
				{
					cl->delay -= 0.001;
					if (cl->delay < 0)
						cl->delay = 0;
				}
				if (frame->ping_time*1000 < sv_minping.value)
				{
					cl->delay += 0.001;
					if (cl->delay > 1)
						cl->delay = 1;
				}
			}
		}
#ifdef PEXT_CSQC
		if (cl->lastsequence_acknoledged + UPDATE_BACKUP > cl->netchan.incoming_acknowledged)
		{
			for (i = cl->lastsequence_acknoledged+1; i < cl->netchan.incoming_acknowledged; i++)
				SV_CSQC_DroppedPacket(cl, i);
		}
		else
			SV_CSQC_DropAll(cl);
#endif
		cl->lastsequence_acknoledged = cl->netchan.incoming_acknowledged;
	}
	else
	{
		Con_Printf("Server bug: No frames!\n");
		cl->send_message = false;
		return;	//shouldn't happen...
	}

	// make sure the reply sequence number matches the incoming
	// sequence number
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped

	// save time for ping calculations
	if (cl->frameunion.frames)
	{	//split screen doesn't always have frames.
		cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = realtime;
		cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;
		cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].move_msecs = -1;
	}

	host_client = cl;
	sv_player = host_client->edict;

	seq_hash = cl->netchan.incoming_sequence;

	// mark time so clients will know how much to predict
	// other players
 	cl->localtime = sv.time;
	cl->delta_sequence = -1;	// no delta unless requested
	while (1)
	{
		if (msg_badread)
		{
			Con_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte ();
haveannothergo:
		if (c == -1)
			break;

		switch (c)
		{
		default:
			Con_Printf ("SV_ReadClientMessage: unknown command char %i\n", c);
			SV_DropClient (cl);
			return;

		case clc_nop:
			break;

		case clc_delta:
			cl->delta_sequence = MSG_ReadByte ();
			break;

		case clc_move:
			if (move_issued)
				return;		// someone is trying to cheat...

			move_issued = true;

			checksumIndex = MSG_GetReadCount();
			checksum = (qbyte)MSG_ReadByte ();

			// read loss percentage
			cl->lossage = MSG_ReadByte();

			for (split = cl; cl; cl = cl->controlled)	//FIXME
			{
				host_client = cl;
				cl->localtime = sv.time;
				sv_player = cl->edict;

				MSG_ReadDeltaUsercmd (&nullcmd, &oldest);
				MSG_ReadDeltaUsercmd (&oldest, &oldcmd);
				MSG_ReadDeltaUsercmd (&oldcmd, &newcmd);

				if (cl->frameunion.frames)
					cl->frameunion.frames[cl->netchan.outgoing_sequence&UPDATE_MASK].move_msecs = newcmd.msec;

				if ( cl->state == cs_spawned )
				{
					if (split == cl)
					{
						// if the checksum fails, ignore the rest of the packet
						calculatedChecksum = COM_BlockSequenceCRCByte(
							net_message.data + checksumIndex + 1,
							MSG_GetReadCount() - checksumIndex - 1,
							seq_hash);

						if (calculatedChecksum != checksum)
						{
							Con_DPrintf ("Failed command checksum for %s(%d) (%d != %d)\n",
								cl->name, cl->netchan.incoming_sequence, checksum, calculatedChecksum);

							for (cl = cl->controlled; cl; cl = cl->controlled)	//FIXME
							{
								MSG_ReadDeltaUsercmd (&nullcmd, &oldest);
								MSG_ReadDeltaUsercmd (&oldest, &oldcmd);
								MSG_ReadDeltaUsercmd (&oldcmd, &newcmd);
							}
							break;;
						}
					}

					if (cl->iscrippled)
					{
						cl->lastcmd.forwardmove = 0;	//hmmm.... does this work well enough?
						oldest.forwardmove = 0;
						newcmd.forwardmove = 0;

						cl->lastcmd.sidemove = 0;
						oldest.sidemove = 0;
						newcmd.sidemove = 0;

						cl->lastcmd.upmove = 0;
						oldest.upmove = 0;
						newcmd.upmove = 0;
					}


					if (!sv.paused)
					{
						if (sv_nomsec.value)
						{
							cl->isindependant = false;
							if (!sv_player->v->fixangle)
							{
								sv_player->v->v_angle[0] = newcmd.angles[0]* (360.0/65536);
								sv_player->v->v_angle[1] = newcmd.angles[1]* (360.0/65536);
								sv_player->v->v_angle[2] = newcmd.angles[2]* (360.0/65536);
							}

							if (newcmd.impulse)// && SV_FiltureImpulse(newcmd.impulse, host_client->trustlevel))
								sv_player->v->impulse = newcmd.impulse;

							sv_player->v->button0 = newcmd.buttons & 1;
							sv_player->v->button2 = (newcmd.buttons >> 1) & 1;
							if (pr_allowbutton1.value)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
								sv_player->v->button1 = ((newcmd.buttons >> 2) & 1);
						// DP_INPUTBUTTONS
							sv_player->xv->button3 = ((newcmd.buttons >> 2) & 1);
							sv_player->xv->button4 = ((newcmd.buttons >> 3) & 1);
							sv_player->xv->button5 = ((newcmd.buttons >> 4) & 1);
							sv_player->xv->button6 = ((newcmd.buttons >> 5) & 1);
							sv_player->xv->button7 = ((newcmd.buttons >> 6) & 1);
							sv_player->xv->button8 = ((newcmd.buttons >> 7) & 1);


							cl->lastcmd = newcmd;
							cl->lastcmd.buttons = 0; // avoid multiple fires on lag

							if (msg_badread)
							{
								Con_Printf ("SV_ReadClientMessage: badread\n");
								SV_DropClient (cl);
								return;
							}
							c = MSG_ReadByte ();
							if (c != clc_move)
							{
								host_client = cl = split;
								sv_player = cl->edict;
								goto haveannothergo;
							}
							continue;
						}
						cl->isindependant = true;
						SV_PreRunCmd();

						if (net_drop < 20)
						{
							while (net_drop > 2)
							{
								SV_RunCmd (&cl->lastcmd, false);
								net_drop--;
							}
							if (net_drop > 1)
								SV_RunCmd (&oldest, false);
							if (net_drop > 0)
								SV_RunCmd (&oldcmd, false);
						}
						SV_RunCmd (&newcmd, false);

						if (!SV_PlayerPhysicsQC)
							SV_PostRunCmd();

					}

					cl->lastcmd = newcmd;
					cl->lastcmd.buttons = 0; // avoid multiple fires on lag
				}

				if (msg_badread)
				{
					Con_Printf ("SV_ReadClientMessage: badread\n");
					SV_DropClient (cl);
					return;
				}

				c = MSG_ReadByte ();
				if (c != clc_move)
				{
					host_client = cl = split;
					sv_player = cl->edict;
					goto haveannothergo;
				}
			}
			host_client = cl = split;
			sv_player = cl->edict;
			break;


		case clc_stringcmd:
			s = MSG_ReadString ();
			SV_ExecuteUserCommand (s, false);

			host_client = cl;
			sv_player = cl->edict;
			break;

		case clc_tmove:
			o[0] = MSG_ReadCoord();
			o[1] = MSG_ReadCoord();
			o[2] = MSG_ReadCoord();
			// only allowed by spectators
			if (sv.mvdplayback)
			{
				VectorCopy(o, host_client->specorigin);
			}
			else if (host_client->spectator)
			{
				VectorCopy(o, sv_player->v->origin);
				SV_LinkEdict(sv_player, false);
			}
			break;

		case clc_upload:
			SV_NextUpload();
			break;

		}
	}

	host_client = NULL;
	sv_player = NULL;
}
#ifdef Q2SERVER
void SVQ2_ExecuteClientMessage (client_t *cl)
{
	enum clcq2_ops_e		c;
	char	*s;
	usercmd_t	oldest, oldcmd, newcmd;
	q2client_frame_t	*frame;
	qboolean	move_issued = false; //only allow one move command
	int		checksumIndex;
	qbyte	checksum, calculatedChecksum;
	int		seq_hash;
	int lastframe;

	if (!ge)
	{
		Con_Printf("Q2 client without Q2 server\n");
		SV_DropClient(cl);
	}

	// calc ping time
	frame = &cl->frameunion.q2frames[cl->netchan.incoming_acknowledged & Q2UPDATE_MASK];

	// make sure the reply sequence number matches the incoming
	// sequence number
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped

	// save time for ping calculations
	cl->frameunion.q2frames[cl->netchan.outgoing_sequence & Q2UPDATE_MASK].senttime = realtime;
//	cl->q2frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;

	host_client = cl;
	sv_player = host_client->edict;

	seq_hash = cl->netchan.incoming_sequence;

	// mark time so clients will know how much to predict
	// other players
 	cl->localtime = sv.time;
	cl->delta_sequence = -1;	// no delta unless requested
	while (1)
	{
		if (msg_badread)
		{
			Con_Printf ("SVQ2_ExecuteClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte ();
		if (c == -1)
			break;

		switch (c)
		{
		default:
			Con_Printf ("SV_ReadClientMessage: unknown command char %i\n", c);
			SV_DropClient (cl);
			return;

		case clcq2_nop:
			break;

		case clcq2_move:
			if (move_issued)
				return;		// someone is trying to cheat...

			move_issued = true;

			checksumIndex = MSG_GetReadCount();
			checksum = (qbyte)MSG_ReadByte ();


			lastframe = MSG_ReadLong();
			if (lastframe != host_client->delta_sequence) {
				cl->delta_sequence = lastframe;
			}

			MSGQ2_ReadDeltaUsercmd (&nullcmd, &oldest);
			MSGQ2_ReadDeltaUsercmd (&oldest, &oldcmd);
			MSGQ2_ReadDeltaUsercmd (&oldcmd, &newcmd);

			if ( cl->state != cs_spawned )
				break;

			// if the checksum fails, ignore the rest of the packet
			calculatedChecksum = Q2COM_BlockSequenceCRCByte(
				net_message.data + checksumIndex + 1,
				MSG_GetReadCount() - checksumIndex - 1,
				seq_hash);

			if (calculatedChecksum != checksum)
			{
				Con_DPrintf ("Failed command checksum for %s(%d) (%d != %d)\n",
					cl->name, cl->netchan.incoming_sequence, checksum, calculatedChecksum);
				return;
			}

			if (cl->iscrippled)
			{
				cl->lastcmd.forwardmove = 0;	//hmmm.... does this work well enough?
				oldest.forwardmove = 0;
				newcmd.forwardmove = 0;

				cl->lastcmd.sidemove = 0;
				oldest.sidemove = 0;
				newcmd.sidemove = 0;

				cl->lastcmd.upmove = 0;
				oldest.upmove = 0;
				newcmd.upmove = 0;
			}

			if (!sv.paused)
			{
				if (net_drop < 20)
				{
					while (net_drop > 2)
					{
						ge->ClientThink (host_client->q2edict, (q2usercmd_t*)&cl->lastcmd);
						net_drop--;
					}
					if (net_drop > 1)
						ge->ClientThink (host_client->q2edict, (q2usercmd_t*)&oldest);
					if (net_drop > 0)
						ge->ClientThink (host_client->q2edict, (q2usercmd_t*)&oldcmd);
				}
				ge->ClientThink (host_client->q2edict, (q2usercmd_t*)&newcmd);
			}

			cl->lastcmd = newcmd;
			break;

		case clcq2_userinfo:
			strncpy (cl->userinfo, MSG_ReadString (), sizeof(cl->userinfo)-1);
			ge->ClientUserinfoChanged (cl->q2edict, cl->userinfo);	//tell the gamecode
			SV_ExtractFromUserinfo(cl);	//let the server routines know
			break;

		case clcq2_stringcmd:
			s = MSG_ReadString ();
			SV_ExecuteUserCommand (s, false);

			host_client = cl;
			sv_player = cl->edict;

			if (cl->state == cs_zombie)
				return;	// disconnect command
			break;
		}
	}
}
#endif
#ifdef NQPROT
void SVNQ_ReadClientMove (usercmd_t *move)
{
	int		i;
	int		bits;
	client_frame_t	*frame;

	frame = &host_client->frameunion.frames[host_client->netchan.incoming_acknowledged & UPDATE_MASK];

	if (host_client->protocol == SCP_DARKPLACES7)
		host_client->last_sequence = MSG_ReadLong ();
	else
		host_client->last_sequence = 0;
	frame->ping_time = sv.time - MSG_ReadFloat ();


// read current angles
	for (i=0 ; i<3 ; i++)
	{
		host_client->edict->v->v_angle[i] = MSG_ReadAngle ();

		move->angles[i] = (host_client->edict->v->v_angle[i] * 256*256)/360;
	}

// read movement
	move->forwardmove = MSG_ReadShort ();
	move->sidemove = MSG_ReadShort ();
	move->upmove = MSG_ReadShort ();

	move->msec=(1/72.0f)*1000;//MSG_ReadFloat;

// read buttons
	if (host_client->protocol == SCP_DARKPLACES6 || host_client->protocol == SCP_DARKPLACES7)
		bits = MSG_ReadLong();
	else
		bits = MSG_ReadByte ();
	move->buttons = bits;

	i = MSG_ReadByte ();
	if (i)
		move->impulse = i;

	if (host_client->protocol == SCP_DARKPLACES6 || host_client->protocol == SCP_DARKPLACES7)
	{
		float f;
		int entnum;
		eval_t *cursor_screen, *cursor_start, *cursor_impact, *cursor_entitynumber;

		cursor_screen	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_screen", NULL);
		cursor_start	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_start", NULL);
		cursor_impact	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_impact", NULL);
		cursor_entitynumber	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_entitynumber", NULL);

		f = MSG_ReadShort() * (1.0f / 32767.0f);
		if (cursor_screen) cursor_screen->_vector[0] = f;
		f = MSG_ReadShort() * (1.0f / 32767.0f);
		if (cursor_screen) cursor_screen->_vector[1] = f;

		f = MSG_ReadFloat();
		if (cursor_start) cursor_start->_vector[0] = f;
		f = MSG_ReadFloat();
		if (cursor_start) cursor_start->_vector[1] = f;
		f = MSG_ReadFloat();
		if (cursor_start) cursor_start->_vector[2] = f;

		f = MSG_ReadFloat();
		if (cursor_impact) cursor_impact->_vector[0] = f;
		f = MSG_ReadFloat();
		if (cursor_impact) cursor_impact->_vector[1] = f;
		f = MSG_ReadFloat();
		if (cursor_impact) cursor_impact->_vector[2] = f;

		entnum = (unsigned short)MSG_ReadShort();
		if (entnum >= sv.max_edicts)
		{
			Con_DPrintf("SV_ReadClientMessage: client send bad cursor_entitynumber\n");
			entnum = 0;
		}
		// as requested by FrikaC, cursor_trace_ent is reset to world if the
		// entity is free at time of receipt
		if (EDICT_NUM(svprogfuncs, entnum)->isfree)
			entnum = 0;
		if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

		if (cursor_entitynumber) cursor_entitynumber->edict = entnum;
	}


	if (i && SV_FiltureImpulse(i, host_client->trustlevel))
		host_client->edict->v->impulse = i;

	host_client->edict->v->button0 = bits & 1;
	host_client->edict->v->button2 = (bits >> 1) & 1;
	if (pr_allowbutton1.value)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
		host_client->edict->v->button1 = ((bits >> 2) & 1);
// DP_INPUTBUTTONS
	host_client->edict->xv->button3 = ((bits >> 2) & 1);
	host_client->edict->xv->button4 = ((bits >> 3) & 1);
	host_client->edict->xv->button5 = ((bits >> 4) & 1);
	host_client->edict->xv->button6 = ((bits >> 5) & 1);
	host_client->edict->xv->button7 = ((bits >> 6) & 1);
	host_client->edict->xv->button8 = ((bits >> 7) & 1);

	if (host_client->last_sequence)
		SV_RunEntity(host_client->edict);
}

void SVNQ_ExecuteClientMessage (client_t *cl)
{
	int		c;
	char	*s;
	client_frame_t	*frame;
	int		seq_hash;

	cl->netchan.outgoing_sequence++;
	cl->netchan.incoming_acknowledged = cl->netchan.outgoing_sequence-1;

	// calc ping time
	frame = &cl->frameunion.frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];
	frame->ping_time = -1;

	// make sure the reply sequence number matches the incoming
	// sequence number
//	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
//	else
//		cl->send_message = false;	// don't reply, sequences have slipped

	// save time for ping calculations
	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = realtime;
	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;

	host_client = cl;
	sv_player = host_client->edict;

	seq_hash = cl->netchan.incoming_sequence;

	// mark time so clients will know how much to predict
	// other players
 	cl->localtime = sv.time;
	while (1)
	{
		if (msg_badread)
		{
			Con_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte ();
		if (c == -1)
			break;

		switch (c)
		{
		default:
			Con_Printf ("SV_ReadClientMessage: unknown command char %i\n", c);
			SV_DropClient (cl);
			return;

		case clc_disconnect:
			SV_DropClient (cl);
			break;
		case clc_nop:
			break;

		case clc_delta:
			cl->delta_sequence = MSG_ReadByte ();
			break;

		case clc_move:
			SVNQ_ReadClientMove (&host_client->lastcmd);
//			cmd = host_client->lastcmd;
//			SV_ClientThink();
			break;

		case clc_stringcmd:
			s = MSG_ReadString ();
			SV_ExecuteUserCommand (s, false);

			host_client = cl;
			sv_player = cl->edict;
			break;

		case clcdp_ackdownloaddata:
			SV_DarkPlacesDownloadAck(host_client);
			break;
		}
	}
}
#endif
/*
==============
SV_UserInit
==============
*/
void SV_UserInit (void)
{
#ifdef SERVERONLY
	Cvar_Register (&cl_rollspeed, "Prediction stuff");
	Cvar_Register (&cl_rollangle, "Prediction stuff");
#endif
	Cvar_Register (&sv_chatfilter, cvargroup_serverpermissions);
	Cvar_Register (&sv_spectalk, cvargroup_servercontrol);
	Cvar_Register (&sv_mapcheck, cvargroup_servercontrol);

	Cvar_Register (&sv_cheatpc, cvargroup_servercontrol);
	Cvar_Register (&sv_cheatspeedchecktime, cvargroup_servercontrol);
	Cvar_Register (&sv_playermodelchecks, cvargroup_servercontrol);

	Cvar_Register (&sv_getrealip, cvargroup_servercontrol);
	Cvar_Register (&sv_realiphostname_ipv4, cvargroup_servercontrol);
	Cvar_Register (&sv_realiphostname_ipv6, cvargroup_servercontrol);
	Cvar_Register (&sv_realip_timeout, cvargroup_servercontrol);

	Cvar_Register (&sv_pushplayers, cvargroup_servercontrol);

	Cvar_Register (&sv_floodprotect, cvargroup_servercontrol);
	Cvar_Register (&sv_floodprotect_interval, cvargroup_servercontrol);
	Cvar_Register (&sv_floodprotect_messages, cvargroup_servercontrol);
	Cvar_Register (&sv_floodprotect_silencetime, cvargroup_servercontrol);
	Cvar_Register (&sv_floodprotect_suicide, cvargroup_servercontrol);
	Cvar_Register (&sv_floodprotect_sendmessage, cvargroup_servercontrol);

	Cvar_Register (&sv_cmdlikercon, cvargroup_serverpermissions);
	Cvar_Register(&cmd_gamecodelevel, "Access controls");
	Cvar_Register(&cmd_allowaccess, "Access controls");

	Cvar_Register (&votelevel, sv_votinggroup);
	Cvar_Register (&voteminimum, sv_votinggroup);
	Cvar_Register (&votepercent, sv_votinggroup);
	Cvar_Register (&votetime, sv_votinggroup);

	Cvar_Register (&sv_brokenmovetypes, "Backwards compatability");

	Cvar_Register (&sv_edgefriction, "netquake compatability");
}





























































static vec3_t forward, right, up, wishdir;
static float *origin, *velocity, *angles;
static float wishspeed;
extern cvar_t sv_accelerate, sv_friction;
static qboolean onground;


/*
==================
SV_UserFriction

==================
*/
void SV_UserFriction (void)
{
	extern cvar_t sv_stopspeed;
	float	*vel;
	float	speed, newspeed, control;
	vec3_t	start, stop;
	float	friction;
	trace_t	trace;

	vel = velocity;

	speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1]);
	if (!speed)
		return;

// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = origin[0] + vel[0]/speed*16;
	start[1] = stop[1] = origin[1] + vel[1]/speed*16;
	start[2] = origin[2] + sv_player->v->mins[2];
	stop[2] = start[2] - 34;

	trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, sv_player);

	if (trace.fraction == 1.0)
		friction = sv_friction.value*sv_edgefriction.value;
	else
		friction = sv_friction.value;

//	val = GetEdictFieldValue(sv_player, "friction", &frictioncache);
//	if (val && val->_float)
//		friction *= val->_float;

// apply friction
	control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
	newspeed = speed - host_frametime*control*friction;

	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}

void SV_Accelerate (void)
{
	int			i;
	float		addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value*host_frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed*wishdir[i];
}

void SV_AirAccelerate (vec3_t wishveloc)
{
	int			i;
	float		addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalize (wishveloc);
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct (velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
//	accelspeed = sv_accelerate.value * host_frametime;
	accelspeed = sv_accelerate.value*wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed*wishveloc[i];
}

/*
===================
SV_AirMove

===================
*/
void SV_AirMove (void)
{
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	float scale, maxspeed;

	AngleVectors (sv_player->v->angles, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

// hack to not let you back into teleporter
	if (sv.time < sv_player->v->teleport_time && fmove < 0)
		fmove = 0;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*fmove + right[i]*smove;

	if ( (int)sv_player->v->movetype != MOVETYPE_WALK)
		wishvel[2] = cmd.upmove;
	else
		wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
//	val = GetEdictFieldValue(sv_player, "scale", &scalecache);
//	if (!val || !val->_float)
		scale = 1;
//	else
//		scale = val->_float;

	maxspeed=sv_player->xv->maxspeed;//FIXME: This isn't fully compatible code...
	if (sv_player->xv->hasted)
		maxspeed*=sv_player->xv->hasted;

	if (wishspeed > maxspeed*scale)
	{
		VectorScale (wishvel, maxspeed/wishspeed, wishvel);
		wishspeed = maxspeed*scale;
	}

	if ( sv_player->v->movetype == MOVETYPE_NOCLIP)
	{	// noclip
		VectorCopy (wishvel, velocity);
	}
	else if ( onground )
	{
		SV_UserFriction ();
		SV_Accelerate ();
	}
	else
	{	// not on ground, so little effect on velocity
		SV_AirAccelerate (wishvel);
	}
}

void SV_WaterMove (void)
{
	int		i;
	vec3_t	wishvel;
	float	speed, newspeed, wishspeed, addspeed, accelspeed;
	float scale;
	float maxspeed;

//
// user intentions
//
	AngleVectors (sv_player->v->v_angle, forward, right, up);

	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*cmd.forwardmove + right[i]*cmd.sidemove;

	if (!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else
		wishvel[2] += cmd.upmove;

	wishspeed = Length(wishvel);
//	val = GetEdictFieldValue(sv_player, "scale", &scalecache);
//	if (!val || !val->_float)
		scale = 1;
//	else
//		scale = val->_float;

//	val = GetEdictFieldValue(sv_player, "maxspeed", &maxspeedcache);
//	if (val && val->_float)
//		maxspeed = sv_maxspeed.value*val->_float;
//	else
		maxspeed = host_client->maxspeed;
	if (wishspeed > maxspeed*scale)
	{
		VectorScale (wishvel, maxspeed/wishspeed, wishvel);
		wishspeed = maxspeed*scale;
	}
	wishspeed *= 0.7;

//
// water friction
//
	speed = Length (velocity);
	if (speed)
	{
//		val = GetEdictFieldValue(sv_player, "friction", &frictioncache);
//		if (val&&val->_float)
//			newspeed = speed - host_frametime * speed * sv_friction.value*val->_float;
//		else
			newspeed = speed - host_frametime * speed * sv_friction.value;
		if (newspeed < 0)
			newspeed = 0;
		VectorScale (velocity, newspeed/speed, velocity);
	}
	else
		newspeed = 0;

//
// water acceleration
//
	if (!wishspeed)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed <= 0)
		return;

	VectorNormalize (wishvel);
	accelspeed = sv_accelerate.value * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed * wishvel[i];
}

void SV_WaterJump (void)
{
	if (sv.time > sv_player->v->teleport_time
	|| !sv_player->v->waterlevel)
	{
		sv_player->v->flags = (int)sv_player->v->flags & ~FL_WATERJUMP;
		sv_player->v->teleport_time = 0;
	}
	sv_player->v->velocity[0] = sv_player->v->movedir[0];
	sv_player->v->velocity[1] = sv_player->v->movedir[1];
}



void SV_ClientThink (void)
{
	vec3_t		v_angle;

	cmd = host_client->lastcmd;
	sv_player = host_client->edict;

	if (host_client->state && host_client->protocol != SCP_BAD)
	{
		sv_player->xv->movement[0] = cmd.forwardmove;
		sv_player->xv->movement[1] = cmd.sidemove;
		sv_player->xv->movement[2] = cmd.upmove;
	}

	if (SV_PlayerPhysicsQC)
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		PR_ExecuteProgram (svprogfuncs, SV_PlayerPhysicsQC);
		return;
	}

	if (sv_player->v->movetype == MOVETYPE_NONE)
		return;

	onground = (int)sv_player->v->flags & FL_ONGROUND;

	origin = sv_player->v->origin;
	velocity = sv_player->v->velocity;

//	DropPunchAngle ();

//
// if dead, behave differently
//
	if (sv_player->v->health <= 0)
		return;

//
// angles
// show 1/3 the pitch angle and all the roll angle
	angles = sv_player->v->angles;

	VectorCopy (sv_player->v->v_angle, v_angle);
//	VectorAdd (sv_player->v->v_angle, sv_player->v->punchangle, v_angle);
	angles[ROLL] = V_CalcRoll (sv_player->v->angles, sv_player->v->velocity)*4;
	if (!sv_player->v->fixangle)
	{
		angles[PITCH] = -v_angle[PITCH]/3;
		angles[YAW] = v_angle[YAW];
	}

	if ( (int)sv_player->v->flags & FL_WATERJUMP )
	{
		SV_WaterJump ();
		return;
	}
//
// walk
//
	if ( (sv_player->v->waterlevel >= 2)
	&& (sv_player->v->movetype != MOVETYPE_NOCLIP) )
	{
		SV_WaterMove ();
		return;
	}

	SV_AirMove ();
}

#endif
