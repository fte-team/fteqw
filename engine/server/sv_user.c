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
#include "pr_common.h"

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
cvar_t	sv_spectalk	= SCVAR("sv_spectalk", "1");

cvar_t	sv_mapcheck	= SCVAR("sv_mapcheck", "1");

cvar_t	sv_fullredirect = CVARD("sv_fullredirect", "", "This is the ip:port to redirect players to when the server is full");
cvar_t	sv_antilag			= CVARFD("sv_antilag", "1", CVAR_SERVERINFO, "Attempt to backdate impacts to compensate for lag. 0=completely off. 1=mod-controlled. 2=forced, which might break certain uses of traceline.");
cvar_t	sv_antilag_frac		= CVARF("sv_antilag_frac", "1", CVAR_SERVERINFO);
cvar_t	sv_cheatpc				= CVAR("sv_cheatpc", "125");
cvar_t	sv_cheatspeedchecktime	= CVAR("sv_cheatspeedchecktime", "30");
cvar_t	sv_playermodelchecks	= CVAR("sv_playermodelchecks", "0");

cvar_t	sv_cmdlikercon	= SCVAR("sv_cmdlikercon", "0");	//set to 1 to allow a password of username:password instead of the correct rcon password.
cvar_t cmd_allowaccess	= SCVAR("cmd_allowaccess", "0");	//set to 1 to allow cmd to execute console commands on the server.
cvar_t cmd_gamecodelevel	= SCVAR("cmd_gamecodelevel", "50");	//execution level which gamecode is told about (for unrecognised commands)

cvar_t	sv_pure	= CVARFD("sv_pure", "", CVAR_SERVERINFO, "The most evil cvar in the world.");
cvar_t	sv_nomsec	= CVARD("sv_nomsec", "0", "Ignore client msec times, runs using NQ physics instead.");
cvar_t	sv_edgefriction	= CVARAF("sv_edgefriction", "2",
								 "edgefriction", 0);

cvar_t	sv_brokenmovetypes	= CVARD("sv_brokenmovetypes", "0", "Emulate standard quakeworld movetypes. Shouldn't be used for any games other than QuakeWorld.");

cvar_t	sv_chatfilter	= CVAR("sv_chatfilter", "0");

cvar_t	sv_floodprotect				= CVAR("sv_floodprotect", "1");
cvar_t	sv_floodprotect_messages	= CVAR("sv_floodprotect_messages", "4");
cvar_t	sv_floodprotect_interval	= CVAR("sv_floodprotect_interval", "4");
cvar_t  sv_floodprotect_silencetime	= CVAR("sv_floodprotect_silencetime", "10");
cvar_t	sv_floodprotect_suicide		= CVAR("sv_floodprotect_suicide", "1");
cvar_t	sv_floodprotect_sendmessage	= CVARAF("sv_floodprotect_sendmessage", "",
											 "floodprotmsg", 0);

cvar_t	votelevel	= SCVAR("votelevel", "0");
cvar_t	voteminimum	= SCVAR("voteminimum", "4");
cvar_t	votepercent	= SCVAR("votepercent", "-1");
cvar_t	votetime	= SCVAR("votetime", "10");

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
cvar_t sv_getrealip = CVARD("sv_getrealip", "0", "Attempt to obtain a more reliable IP for clients, rather than just their proxy.");
cvar_t sv_realip_kick = SCVAR("sv_realip_kick", "0");
cvar_t sv_realiphostname_ipv4 = CVARD("sv_realiphostname_ipv4", "", "This is the server's public ip:port. This is needed for realip to work when the autodetected/local ip is not globally routable");
cvar_t sv_realiphostname_ipv6 = CVARD("sv_realiphostname_ipv6", "", "This is the server's public ip:port. This is needed for realip to work when the autodetected/local ip is not globally routable");
cvar_t sv_realip_timeout = SCVAR("sv_realip_timeout", "10");

#ifdef VOICECHAT
cvar_t sv_voip = CVARD("sv_voip", "1", "Enable reception of voice packets.");
cvar_t sv_voip_record = CVARD("sv_voip_record", "0", "Record voicechat into mvds. Requires player support. 0=noone, 1=everyone, 2=spectators only");
cvar_t sv_voip_echo = CVARD("sv_voip_echo", "0", "Echo voice packets back to their sender, a debug/test setting.");
#endif

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

	if (sv_getrealip.value <= client->realip_status || sv_getrealip.value > 3)
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
		return true;	//this client timed out.

	if (realtime - client->connection_started > sv_realip_timeout.value)
	{
		ClientReliableWrite_Begin(client, svc_print, 256);
		ClientReliableWrite_Byte(client, PRINT_HIGH);
		if (client->realip_status > 0)
			ClientReliableWrite_String(client, "Couldn't verify your real ip\n");
		else
			ClientReliableWrite_String(client, "Couldn't determine your real ip\n");
		if (sv_realip_kick.value > host_client->realip_status)
		{
			SV_DropClient(client);
			return false;
		}
		if (!client->realip_status)
			client->realip_status = -1;
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
			client->realip_status = -1;
			return true;
		}

		ClientReliableWrite_Begin(client, svc_stufftext, 256);
		ClientReliableWrite_String(client, va("packet %s \"realip %i %i\"\n", serverip, (int)(client-svs.clients), client->realip_num));
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

	if (host_client->redirect)
	{
		char *msg = va("connect \"%s\"\n", sv_fullredirect.string);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(msg));
		ClientReliableWrite_String (host_client, msg);
		return;
	}

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
	{
		if (ISQWCLIENT(host_client))
			gamedir = "qw";
		else
			gamedir = "";
	}

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
/*	if (host_client->num_backbuf)
	{
		Con_TPrintf(STL_BACKBUFSET, host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear(&host_client->netchan.message);
	}
*/
	if (svs.netprim.coordsize > 2 && !(host_client->fteprotocolextensions & PEXT_FLOATCOORDS))
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
		ClientReliableWrite_Long (host_client, PROTOCOL_VERSION_FTE);
		if (svs.netprim.coordsize == 2)	//we're not using float orgs on this level.
			ClientReliableWrite_Long (host_client, host_client->fteprotocolextensions&~PEXT_FLOATCOORDS);
		else
			ClientReliableWrite_Long (host_client, host_client->fteprotocolextensions);
	}
	if (host_client->fteprotocolextensions2)//let the client know
	{
		ClientReliableWrite_Long (host_client, PROTOCOL_VERSION_FTE2);
		ClientReliableWrite_Long (host_client, host_client->fteprotocolextensions2);
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
		switch(svs.gametype)
		{
#ifdef HLSERVER
		case GT_HALFLIFE:
			playernum = split - svs.clients;
			break;
#endif
#ifdef Q2SERVER
		case GT_QUAKE2:
			playernum = Q2NUM_FOR_EDICT(split->q2edict)-1;
			break;
#endif
		default:
			playernum = NUM_FOR_EDICT(svprogfuncs, split->edict)-1;
		}
#ifdef SERVER_DEMO_PLAYBACK
		if (sv.demostate)
		{
			playernum = (MAX_CLIENTS-1-splitnum)|128;
		}
		else
#endif
			if (split->spectator)
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
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demostatevalid)
		ClientReliableWrite_String (host_client, sv.demfullmapname);
	else
#endif
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

#ifdef SERVER_DEMO_PLAYBACK
	// send server info string
	if (sv.demostatevalid)
	{
		ClientReliableWrite_Begin(host_client, svc_stufftext, 20 + strlen(sv.demoinfo));
		ClientReliableWrite_String (host_client, va("fullserverinfo \"%s\"\n", sv.demoinfo) );
	}
	else
#endif
	{
		ClientReliableWrite_Begin(host_client, svc_stufftext, 20 + strlen(svs.info));
		ClientReliableWrite_String (host_client, va("fullserverinfo \"%s\"\n", svs.info) );
	}

	host_client->csqcactive = false;

	host_client->realip_num = rand()+(host_client->challenge<<16);
	SV_CheckRealIP(host_client, false);

	// send music
	ClientReliableWrite_Begin(host_client, svc_cdtrack, 2);
	if (progstype == PROG_H2)
		ClientReliableWrite_Byte (host_client, sv.h2cdtrack);
	else if (svprogfuncs)
		ClientReliableWrite_Byte (host_client, ((edict_t*)sv.world.edicts)->v->sounds);
	else
		ClientReliableWrite_Byte (host_client, 0);

	SV_LogPlayer(host_client, "new (QW)");



	{
		char buffer[1024];

		FS_GetPackNames(buffer, sizeof(buffer), 2, true); /*retain extensions, or we'd have to assume pk3*/
		ClientReliableWrite_Begin(host_client, svc_stufftext, 1+11+strlen(buffer)+1+1);
		ClientReliableWrite_SZ(host_client, "//paknames ", 11);
		ClientReliableWrite_SZ(host_client, buffer, strlen(buffer));
		ClientReliableWrite_String(host_client, "\n");

		FS_GetPackHashes(buffer, sizeof(buffer), false);
		ClientReliableWrite_Begin(host_client, svc_stufftext, 1+7+strlen(buffer)+1+1);
		ClientReliableWrite_SZ(host_client, "//paks ", 7);
		ClientReliableWrite_SZ(host_client, buffer, strlen(buffer));
		ClientReliableWrite_String(host_client, "\n");
	}
}

#define GAME_DEATHMATCH 0
#define GAME_COOP 1
void SVNQ_New_f (void)
{
	extern cvar_t coop;
	char			message[2048];
	int i;

	if (host_client->redirect)
	{
		char *msg = va("connect \"%s\"\n", sv_fullredirect.string);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(msg));
		ClientReliableWrite_String (host_client, msg);
		return;
	}

	if (!host_client->pextknown)
	{
		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message, "cmd pext\n");
		return;
	}

	MSG_WriteByte (&host_client->netchan.message, svc_print);
	Q_snprintfz (message, sizeof(message), "%c\n%s server\n", 2, version_string());
	MSG_WriteString (&host_client->netchan.message,message);

	if (host_client->protocol == SCP_DARKPLACES6 || host_client->protocol == SCP_DARKPLACES7)
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

	if (host_client->fteprotocolextensions)
	{
		MSG_WriteLong (&host_client->netchan.message, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (&host_client->netchan.message, host_client->fteprotocolextensions);
	}
	if (host_client->fteprotocolextensions2)
	{
		MSG_WriteLong (&host_client->netchan.message, PROTOCOL_VERSION_FTE2);
		MSG_WriteLong (&host_client->netchan.message, host_client->fteprotocolextensions2);
	}

	switch(host_client->protocol)
	{
#ifdef NQPROT
	case SCP_NETQUAKE:
	case SCP_FITZ666:
		SV_LogPlayer(host_client, "new (NQ)");
		if (sv.nqdatagram.prim.anglesize != 1 || sv.nqdatagram.prim.coordsize != 2)
		{
			int rmqfl =
					((sv.nqdatagram.prim.coordsize==4)?RMQFL_FLOATCOORD:0) |
					((sv.nqdatagram.prim.anglesize==2)?RMQFL_SHORTANGLE:0);
			host_client->protocol = SCP_FITZ666; /*mneh, close enough, the rmq stuff is just modifiers*/

			if (rmqfl)
			{
				MSG_WriteLong (&host_client->netchan.message, RMQ_PROTOCOL_VERSION);
				MSG_WriteLong (&host_client->netchan.message, rmqfl);
			}
			else
			{
				MSG_WriteLong (&host_client->netchan.message, FITZ_PROTOCOL_VERSION);
			}
		}
		else
		{
			host_client->protocol = SCP_NETQUAKE;
			MSG_WriteLong (&host_client->netchan.message, NQ_PROTOCOL_VERSION);
		}
		MSG_WriteByte (&host_client->netchan.message, (sv.allocated_client_slots>16)?16:sv.allocated_client_slots);
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
	if (progstype == PROG_H2)
	{
		MSG_WriteByte (&host_client->netchan.message, sv.h2cdtrack);
		MSG_WriteByte (&host_client->netchan.message, sv.h2cdtrack);
	}
	else
	{
		MSG_WriteByte (&host_client->netchan.message, ((edict_t*)sv.world.edicts)->v->sounds);
		MSG_WriteByte (&host_client->netchan.message, ((edict_t*)sv.world.edicts)->v->sounds);
	}

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
		char *msg = va("cmd pk3list %s %s\n", Cmd_Argv(1), Cmd_Argv(2));
		Con_TPrintf(STL_BACKBUFSET, host_client->name, host_client->netchan.message.cursize);
		ClientReliableWrite_Begin(host_client, svc_stufftext, 2+strlen(msg));
		ClientReliableWrite_String(&host_client->netchan.message, msg);
		return;
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
	unsigned int i;
	//char		**s;
	unsigned int n;
	unsigned int maxclientsupportedsounds;

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
	if (host_client->num_backbuf)
	{
		char *msg = va("cmd soundlist %s %s\n", Cmd_Argv(1), Cmd_Argv(2));
		Con_TPrintf(STL_BACKBUFSET, host_client->name, host_client->netchan.message.cursize);
		ClientReliableWrite_Begin(host_client, svc_stufftext, 1+strlen(msg));
		ClientReliableWrite_String(host_client, msg);
		return;
	}

	if (n >= MAX_SOUNDS)
	{
		SV_EndRedirect();
		Con_Printf ("SV_Soundlist_f: %s send an invalid index\n", host_client->name);
		SV_DropClient(host_client);
		return;
	}

#ifdef PEXT_SOUNDDBL
	if (n > 255)
	{
		MSG_WriteByte (&host_client->netchan.message, svcfte_soundlistshort);
		MSG_WriteShort (&host_client->netchan.message, n);
	}
	else
#endif
	{
		MSG_WriteByte (&host_client->netchan.message, svc_soundlist);
		MSG_WriteByte (&host_client->netchan.message, n);
	}

	maxclientsupportedsounds = 256;
#ifdef PEXT_SOUNDDBL
	if (host_client->fteprotocolextensions & PEXT_SOUNDDBL)
		maxclientsupportedsounds *= 2;
#endif

#ifdef SERVER_DEMO_PLAYBACK
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
#endif
	{
		for (i = 1+n;
			i < maxclientsupportedsounds && *sv.strings.sound_precache[i] && host_client->netchan.message.cursize < (MAX_QWMSGLEN/2);
			i++, n++)
		{
			MSG_WriteString (&host_client->netchan.message, sv.strings.sound_precache[i]);
			if (((n&255)==255) && n != i-1)
				break;
		}

		if (!*sv.strings.sound_precache[i])
			n = 0;
	}
	MSG_WriteByte (&host_client->netchan.message, 0);

	// next msg
	MSG_WriteByte (&host_client->netchan.message, n & 0xff);
}

/*
==================
SV_Modellist_f
==================
*/
void SV_Modellist_f (void)
{
	unsigned int i;
	unsigned int n;
	qboolean initial;

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

	if (n >= MAX_MODELS)
	{
		SV_EndRedirect();
		Con_Printf ("SV_Modellist_f: %s send an invalid index\n", host_client->name);
		SV_DropClient(host_client);
		return;
	}

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf)
	{
		char *msg = va("cmd modellist %s %s\n", Cmd_Argv(1), Cmd_Argv(2));
		Con_TPrintf(STL_BACKBUFSET, host_client->name, host_client->netchan.message.cursize);
		ClientReliableWrite_Begin(host_client, svc_stufftext, 1+strlen(msg));
		ClientReliableWrite_String(host_client, msg);
		return;
	}

	initial = (n==0);

#ifdef PEXT_MODELDBL
	if (n > 255)
	{
		MSG_WriteByte (&host_client->netchan.message, svcfte_modellistshort);
		MSG_WriteShort (&host_client->netchan.message, n);
	}
	else
#endif
	{
		MSG_WriteByte (&host_client->netchan.message, svc_modellist);
		MSG_WriteByte (&host_client->netchan.message, n);
	}

#ifdef SERVER_DEMO_PLAYBACK
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
#endif
	{
		for (i = 1+n;
			i < host_client->maxmodels && sv.strings.model_precache[i] && (((i-1)&255)==0 || host_client->netchan.message.cursize < (MAX_QWMSGLEN/2));	//make sure we don't send a 0 next...
			i++)
		{
			MSG_WriteString (&host_client->netchan.message, sv.strings.model_precache[i]);
		}
		n = i-1;

		if (!sv.strings.model_precache[i])
			n = 0;
	}

	if (i == host_client->maxmodels)
		n = 0;	//doh!

	MSG_WriteByte (&host_client->netchan.message, 0);

	// next msg
	MSG_WriteByte (&host_client->netchan.message, n & 0xff);


	if (initial && (host_client->zquake_extensions & Z_EXT_VWEP))
	{
		char mname[MAX_QPATH];
		char vweaplist[1024] = "//vwep";
		//int pos = strlen(vweaplist); // warning: unused variable ‘pos’

		for (i = 0; sv.strings.vw_model_precache[i]; i++)
		{
			//grab the model name... without a progs/ prefix if it has one
			if (!strncmp(sv.strings.vw_model_precache[i], "progs/", 6))
				Q_strncpy(mname, sv.strings.vw_model_precache[i]+6, sizeof(mname));
			else
				Q_strncpy(mname, sv.strings.vw_model_precache[i], sizeof(mname));

			//strip .mdl extensions
			if (!strcmp(COM_FileExtension(mname), "mdl"))
				COM_StripExtension(mname, mname, sizeof(mname));

			//add it to the vweap command, taking care of any remaining spaces in names.
			if (strchr(mname, ' '))
				Q_strncatz(vweaplist, va(" \"%s\"", mname), sizeof(vweaplist));
			else
				Q_strncatz(vweaplist, va(" %s", mname), sizeof(vweaplist));
		}

		if (strlen(vweaplist) <= sizeof(vweaplist)-2)
		{
			Q_strncatz(vweaplist, "\n", sizeof(vweaplist));

			ClientReliableWrite_Begin(host_client, svc_stufftext, 2+strlen(vweaplist));
			ClientReliableWrite_String(host_client, vweaplist);
		}
	}
}

/*
==================
SV_PreSpawn_f
==================
*/
void SVQW_PreSpawn_f (void)
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

#ifdef SERVER_DEMO_PLAYBACK
	if (sv.democausesreconnect)
		bufs = sv.num_demosignon_buffers;
	else
#endif
		bufs = sv.num_signon_buffers;
	statics = sv.num_static_entities;
	buf = atoi(Cmd_Argv(2));

	if (buf >= bufs+statics+sv.world.num_edicts+255)
	{
		SV_EndRedirect();
		Con_Printf ("SV_Modellist_f: %s send an invalid index\n", host_client->name);
		SV_DropClient(host_client);
		return;
	}

	if (!buf)
	{
		// should be three numbers following containing checksums
		check = COM_RemapMapChecksum(atoi(Cmd_Argv(3)));

//		Con_DPrintf("Client check = %d\n", check);

		if (sv_mapcheck.value && check != sv.world.worldmodel->checksum &&
			check != COM_RemapMapChecksum(LittleLong(sv.world.worldmodel->checksum2)))
#ifdef SERVER_DEMO_PLAYBACK
		if (!sv.demofile || (sv.demofile && !sv.democausesreconnect))	//demo playing causes no check. If it's the return level, check anyway to avoid that loophole.
#endif
		{
			char *msg;
			SV_ClientTPrintf (host_client, PRINT_HIGH,
				STL_MAPCHEAT,
				sv.modelname, check, sv.world.worldmodel->checksum, sv.world.worldmodel->checksum2);


			msg = va("\n//kickfile \"%s\"\n", sv.modelname);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 3+strlen(msg));
			ClientReliableWrite_String (host_client, msg);
			SV_DropClient (host_client);
			return;
		}
		host_client->checksum = check;
	}

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf)
	{
		char *msg = va("cmd prespawn %s %s %s\n", Cmd_Argv(1), Cmd_Argv(2), Cmd_Argv(3));
		Con_TPrintf(STL_BACKBUFSET, host_client->name, host_client->netchan.message.cursize);
		ClientReliableWrite_Begin(host_client, svc_stufftext, 1+strlen(msg));
		ClientReliableWrite_String(host_client, msg);
		return;
	}

	if (buf >= bufs
#ifdef SERVER_DEMO_PLAYBACK
		&& !sv.democausesreconnect
#endif
		)
	{
		int i;
		entity_state_t from;
		entity_state_t *state;
		edict_t *ent;
		svcustomtents_t *ctent;


		memset(&from, 0, sizeof(from));
		while (host_client->netchan.message.cursize < (host_client->netchan.message.maxsize/2))	//static entities
		{
			if (buf - bufs >= sv.num_static_entities)
				break;

			state = &sv_staticentities[buf - bufs];
			buf++;

			if (host_client->fteprotocolextensions & PEXT_SPAWNSTATIC2)
			{
				/*if it uses some new feature, use the updated spawnstatic*/
				if (state->hexen2flags || state->trans || state->modelindex >= 256 || state->frame > 255 || state->scale || state->abslight)
				{
					MSG_WriteByte(&host_client->netchan.message, svc_spawnstatic2);
					SV_WriteDelta(&from, state, &host_client->netchan.message, true, host_client->fteprotocolextensions);
					continue;
				}
			}
			/*couldn't use protocol extensions?
			  use the fallback, unless the model is invalid as that's silly*/
			if (state->modelindex < 256)
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnstatic);

				MSG_WriteByte (&host_client->netchan.message, state->modelindex);

				MSG_WriteByte (&host_client->netchan.message, state->frame);
				MSG_WriteByte (&host_client->netchan.message, (int)state->colormap);
				MSG_WriteByte (&host_client->netchan.message, (int)state->skinnum);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&host_client->netchan.message, state->origin[i]);
					MSG_WriteAngle(&host_client->netchan.message, state->angles[i]);
				}
				continue;
			}
		}
		while (host_client->netchan.message.cursize < (host_client->netchan.message.maxsize/2))	//baselines
		{
			if (buf - bufs - sv.num_static_entities >= sv.world.num_edicts)
				break;

			ent = EDICT_NUM(svprogfuncs, buf - bufs - sv.num_static_entities);

			state = &ent->baseline;
			if (!state->number || !state->modelindex)
			{	//ent doesn't have a baseline
				buf++;
				continue;
			}

			if (!ent)
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline);

				MSG_WriteShort (&host_client->netchan.message, buf - bufs - sv.num_static_entities);

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
			else if (state->number >= host_client->max_net_ents || state->modelindex >= host_client->maxmodels)
			{
				/*can't send this ent*/
			}
			else if (host_client->fteprotocolextensions & PEXT_SPAWNSTATIC2)
			{
				MSG_WriteByte(&host_client->netchan.message, svcfte_spawnbaseline2);
				SV_WriteDelta(&from, state, &host_client->netchan.message, true, host_client->fteprotocolextensions);
			}
			else if (state->modelindex < 256)
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline);

				MSG_WriteShort (&host_client->netchan.message, buf - bufs - sv.num_static_entities);

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
			i = buf - bufs - sv.num_static_entities - sv.world.num_edicts;
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
		buf = bufs+sv.num_static_entities+sv.world.num_edicts+255;
	}
	else
	{
#ifdef SERVER_DEMO_PLAYBACK
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
#endif
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
	if (buf == bufs+sv.num_static_entities+sv.world.num_edicts+255)
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

	int secret_total, secret_found, monsters_total, monsters_found;

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
	SV_MVD_FullClientUpdate(NULL, host_client);

// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
#ifdef SERVER_DEMO_PLAYBACK
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
#endif
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

#ifdef HLSERVER
	if (svs.gametype == GT_HALFLIFE)
	{
		for (split = host_client; split; split = split->controlled)
		{
			split->entgravity = 1;
			split->maxspeed = 320;

			SVHL_PutClientInServer(split);
		}

		secret_total = 0;
		secret_found = 0;
		monsters_total = 0;
		monsters_found = 0;
	}
	else
#endif
	{
		// set up the edict
		for (split = host_client; split; split = split->controlled)
		{
			ent = split->edict;

			if (split->istobeloaded)	//minimal setup
			{
				split->entgravity = ent->xv->gravity;
				split->maxspeed = ent->xv->maxspeed;
				split->playerclass = ent->xv->playerclass;
			}
			else
			{
				SV_SetUpClientEdict(split, ent);
			}

		//
		// force stats to be updated
		//
			memset (split->statsi, 0, sizeof(split->statsi));
			memset (split->statsf, 0, sizeof(split->statsf));
			memset (split->statss, 0, sizeof(split->statss));
		}

		secret_total = pr_global_struct->total_secrets;
		secret_found = pr_global_struct->found_secrets;
		monsters_total = pr_global_struct->total_monsters;
		monsters_found = pr_global_struct->killed_monsters;
	}
	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALSECRETS);
	ClientReliableWrite_Long (host_client, secret_total);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALMONSTERS);
	ClientReliableWrite_Long (host_client, monsters_total);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_SECRETS);
	ClientReliableWrite_Long (host_client, secret_found);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_MONSTERS);
	ClientReliableWrite_Long (host_client, monsters_found);
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

	for (i=MAX_CLIENTS+1 ; i<sv.world.num_edicts ; i++)
	{
		e = EDICT_NUM(svprogfuncs, i);
		if (!strcmp(PR_GetString(svprogfuncs, e->v->classname), "info_player_start"))
		{
			VectorCopy (e->v->origin, sv_player->v->origin);
			return;
		}
	}

}

void SV_Begin_Core(client_t *split)
{	//this is the client-protocol-independant core, for q1/q2 gamecode

	client_t	*oh;
	int		i;
	if (progstype == PROG_H2 && split->playerclass)
		split->edict->xv->playerclass = split->playerclass;	//make sure it's set the same as the userinfo

#ifdef Q2SERVER
	if (ge)
	{
		ge->ClientUserinfoChanged (split->q2edict, split->userinfo);	//tell the gamecode
		SV_ExtractFromUserinfo(split);	//let the server routines know

		ge->ClientBegin(split->q2edict);
		split->istobeloaded = false;
	}
	else
#endif
	if (split->istobeloaded)
	{
		func_t f;
		split->istobeloaded = false;

		f = PR_FindFunction(svprogfuncs, "RestoreGame", PR_ANY);
		if (f)
		{
			pr_global_struct->time = sv.world.physicstime;
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
					if (pr_global_ptrs->spawnparamglobals[i])
						*pr_global_ptrs->spawnparamglobals[i] = split->spawn_parms[i];
				}

				// call the spawn function
				pr_global_struct->time = sv.world.physicstime;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
				PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
			}
		}
		else
		{
			sv.spawned_client_slots++;

			if (svprogfuncs)
			{
				eval_t *eval, *eval2;
				eval = PR_FindGlobal(svprogfuncs, "ClientReEnter", 0, NULL);
				if (eval && split->spawninfo)
				{
					globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
					int j;
					edict_t *ent;
					ent = split->edict;
					j = strlen(split->spawninfo);
					World_UnlinkEdict((wedict_t*)ent);
					svprogfuncs->restoreent(svprogfuncs, split->spawninfo, &j, ent);

					eval2 = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "stats_restored", NULL);
					if (eval2)
						eval2->_float = 1;
					for (j=0 ; j< NUM_SPAWN_PARMS ; j++)
					{
						if (pr_global_ptrs->spawnparamglobals[j])
							*pr_global_ptrs->spawnparamglobals[j] = split->spawn_parms[j];
					}
					pr_global_struct->time = sv.world.physicstime;
					pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
					G_FLOAT(OFS_PARM0) = sv.time - split->spawninfotime;
					PR_ExecuteProgram(svprogfuncs, eval->function);
				}
				else
				{
					// copy spawn parms out of the client_t
					for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
					{
						if (pr_global_ptrs->spawnparamglobals[i])
							*pr_global_ptrs->spawnparamglobals[i] = split->spawn_parms[i];
					}

					// call the spawn function
#ifdef VM_Q1
					if (svs.gametype == GT_Q1QVM)
						Q1QVM_ClientConnect(split);
					else
#endif
					{
						globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

						pr_global_struct->time = sv.world.physicstime;
						pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
						if (pr_globals)
							G_FLOAT(OFS_PARM0) = split->csqcactive;	//this arg is part of EXT_CSQC_1, but doesn't have to be supported by the mod
						if (pr_global_ptrs->ClientConnect)
							PR_ExecuteProgram (svprogfuncs, *pr_global_ptrs->ClientConnect);

						// actually spawn the player
						pr_global_struct->time = sv.world.physicstime;
						pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
						if (pr_global_ptrs->PutClientInServer)
							PR_ExecuteProgram (svprogfuncs, *pr_global_ptrs->PutClientInServer);
						else
						{
							split->edict->v->health = 100;
							split->edict->v->mins[0] = -16;
							split->edict->v->mins[1] = -16;
							split->edict->v->mins[2] = -24;
							split->edict->v->maxs[0] = 16;
							split->edict->v->maxs[1] = 16;
							split->edict->v->maxs[2] = 32;
						}
					}

					oh = host_client;
					host_client = split;
					sv_player = host_client->edict;
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
					sv_player = host_client->edict;
				}
			}
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
	client_t	*split;
	unsigned pmodel = 0, emodel = 0;
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

	if (host_client->istobeloaded)
		sendangles = true;
	if (host_client->protocol == SCP_QUAKE2)
		sendangles = false;


	for (split = host_client; split; split = split->controlled)
	{	//tell the gamecode they're ready
		SV_Begin_Core(split);
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
			ClientReliableWrite_Byte (host_client, sv.paused!=0);
		}
		if (sv.paused&~4)
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

void SV_NextChunkedDownload(unsigned int chunknum, int ezpercent, int ezfilenum)
{
#define CHUNKSIZE 1024
	char buffer[CHUNKSIZE];
	qbyte oobdata[1+ (sizeof("\\chunk")-1) + 4 + 1 + 4 + CHUNKSIZE];
	sizebuf_t *msg, msg_oob;
	int i;
	int error = false;

	msg = &host_client->datagram;

	if (chunknum*CHUNKSIZE > host_client->downloadsize)
	{
		SV_ClientPrintf (host_client, PRINT_HIGH, "Invalid file chunk requested %u to %u of %u.\n", chunknum*CHUNKSIZE, (chunknum+1)*CHUNKSIZE, host_client->downloadsize);
		error = 2;
	}

	if (!error && VFS_SEEK (host_client->download, chunknum*CHUNKSIZE) == false)
		error = true;
	else
	{
		if (host_client->downloadcount < chunknum*CHUNKSIZE)
			host_client->downloadcount = chunknum*CHUNKSIZE;
	}

	if (host_client->datagram.cursize + CHUNKSIZE+5+50 > host_client->datagram.maxsize)
	{
		//would overflow the packet.
		msg = &msg_oob;

		if (!ezfilenum)
			return;

		if (host_client->waschoked)
			return;	//don't let chunked downloads flood out the standard packets.

		if (!Netchan_CanPacket(&host_client->netchan, SV_RateForClient(host_client)))
			return;
	}

	if (error)
		i = 0;
	else
		i = VFS_READ (host_client->download, buffer, CHUNKSIZE);

	if (i > 0)
	{
		if (msg == &msg_oob)
		{
			msg = &msg_oob;
			msg->cursize = 0;
			msg->maxsize = sizeof(oobdata);
			msg->currentbit = 0;
			msg->packing = SZ_RAWBYTES;
			msg->allowoverflow = 0;
			msg->overflowed = 0;
			msg->data = oobdata;
			MSG_WriteByte(msg, A2C_PRINT);
			SZ_Write(msg, "\\chunk", 6);
			MSG_WriteLong(msg, ezfilenum);	//echoing the file num is used so the packets don't go out of sync.
		}

		if (i != CHUNKSIZE)
			memset(buffer+i, 0, CHUNKSIZE-i);

		MSG_WriteByte(msg, svc_download);
		MSG_WriteLong(msg, chunknum);
		SZ_Write(msg, buffer, CHUNKSIZE);

		if (msg == &msg_oob)
		{
			Netchan_OutOfBand(NS_SERVER, host_client->netchan.remote_address, msg_oob.cursize, msg_oob.data);
			Netchan_Block(&host_client->netchan, msg_oob.cursize, SV_RateForClient(host_client));
		}
	}
	else if (i < 0)
		error = true;

	if (error)
	{
		VFS_CLOSE (host_client->download);
		host_client->download = NULL;

		if (error != 2)
		{/*work around for ezquake*/
			ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(host_client->downloadfn));
			ClientReliableWrite_Long (host_client, -1);
			ClientReliableWrite_Long (host_client, -3);
			ClientReliableWrite_String (host_client, host_client->downloadfn);
		}


		host_client->downloadstarted = false;
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
		if (Cmd_Argc() < 2)
			SV_NextChunkedDownload(atoi(Cmd_Argv(1)), atoi(Cmd_Argv(2)), atoi(Cmd_Argv(3)));
		else
			SV_NextChunkedDownload(atoi(Cmd_Argv(1)), atoi(Cmd_Argv(2)), atoi(Cmd_Argv(3)));
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
	vsnprintf (send+5, sizeof(send)-5, fmt, argptr);
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
		host_client->upload = FS_OpenVFS(host_client->uploadfn, "wb", FS_GAMEONLY);
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

#ifdef VOICECHAT
/*
Pivicy issues:
By sending voice chat to a server, you are unsure who might be listening.
Voice can be recorded to an mvd, potentially including voice.
Spectators tracvking you are able to hear team chat of your team.
You're never quite sure if anyone might join the server and your team before you finish saying a sentance.
You run the risk of sounds around you being recorded by quake, including but not limited to: TV channels, loved ones, phones, YouTube videos featuring certain moans.
Default on non-team games is to broadcast.
*/

#define VOICE_RING_SIZE 512 /*POT*/
struct
{
	struct voice_ring_s
	{
			unsigned int sender;
			unsigned char receiver[MAX_CLIENTS/8];
			unsigned char gen;
			unsigned char seq;
			unsigned int datalen;
			unsigned char data[1024];
	} ring[VOICE_RING_SIZE];
	unsigned int write;
} voice;
void SV_VoiceReadPacket(void)
{
	unsigned int vt = host_client->voice_target;
	unsigned int j;
	struct voice_ring_s *ring;
	unsigned short bytes;
	client_t *cl;
	unsigned char gen = MSG_ReadByte();
	unsigned char seq = MSG_ReadByte();
	/*read the data from the client*/
	bytes = MSG_ReadShort();
	ring = &voice.ring[voice.write & (VOICE_RING_SIZE-1)];
	if (bytes > sizeof(ring->data) || host_client->ismuted || !sv_voip.ival)
	{
		MSG_ReadSkip(bytes);
		return;
	}
	else
	{
		voice.write++;
		MSG_ReadData(ring->data, bytes);
	}

	ring->datalen = bytes;
	ring->sender = host_client - svs.clients;
	ring->gen = gen;
	ring->seq = seq;

	/*broadcast it its to their team, and its not teamplay*/
	if (vt == VT_TEAM && !teamplay.ival)
		vt = VT_ALL;

	/*figure out which team members are meant to receive it*/
	for (j = 0; j < MAX_CLIENTS/8; j++)
		ring->receiver[j] = 0;
	for (j = 0, cl = svs.clients; j < sv.allocated_client_slots; j++, cl++)
	{
		if (cl->state != cs_spawned && cl->state != cs_connected)
			continue;
		/*spectators may only talk to spectators*/
		if (host_client->spectator && !sv_spectalk.ival)
			if (!cl->spectator)
				continue;

		if (vt == VT_TEAM)
		{
			// the spectator team
			if (host_client->spectator)
			{
				if (!cl->spectator)
					continue;
			}
			else
			{
				if (strcmp(cl->team, host_client->team) || cl->spectator)
					continue;	// on different teams
			}
		}
		else if (vt == VT_NONMUTED)
		{
			if (host_client->voice_mute[j>>3] & (1<<(j&3)))
				continue;
		}
		else if (vt >= VT_PLAYERSLOT0)
		{
			if (j != vt - VT_PLAYERSLOT0)
				continue;
		}

		ring->receiver[j>>3] |= 1<<(j&3);
	}

	if (sv.mvdrecording && sv_voip_record.ival && !(sv_voip_record.ival == 2 && !host_client->spectator))
	{
		// non-team messages should be seen always, even if not tracking any player
		if (vt == VT_ALL && (!host_client->spectator || sv_spectalk.ival))
		{
			MVDWrite_Begin (dem_all, 0, ring->datalen+6);
		}
		else
		{
			unsigned int cls;
			cls = ring->receiver[0] |
				(ring->receiver[1]<<8) |
				(ring->receiver[2]<<16) |
				(ring->receiver[3]<<24);
			MVDWrite_Begin (dem_multiple, cls, ring->datalen+6);
		}

		MSG_WriteByte( &demo.dbuf->sb, svcfte_voicechat);
		MSG_WriteByte( &demo.dbuf->sb, ring->sender);
		MSG_WriteByte( &demo.dbuf->sb, ring->gen);
		MSG_WriteByte( &demo.dbuf->sb, ring->seq);
		MSG_WriteShort(&demo.dbuf->sb, ring->datalen);
		SZ_Write(      &demo.dbuf->sb, ring->data, ring->datalen);
	}
}
void SV_VoiceInitClient(client_t *client)
{
	client->voice_target = VT_TEAM;
	client->voice_active = false;
	client->voice_read = voice.write;
	memset(client->voice_mute, 0, sizeof(client->voice_mute));
}
void SV_VoiceSendPacket(client_t *client, sizebuf_t *buf)
{
	unsigned int clno;
	qboolean send;
	struct voice_ring_s *ring;

	if (client->controller)
		client = client->controller;
	clno = client - svs.clients;

	if (!(client->fteprotocolextensions2 & PEXT2_VOICECHAT))
		return;
	if (!client->voice_active || client->num_backbuf)
	{
		client->voice_read = voice.write;
		return;
	}

	while(client->voice_read < voice.write)
	{
		/*they might be too far behind*/
		if (client->voice_read+VOICE_RING_SIZE < voice.write)
			client->voice_read = voice.write - VOICE_RING_SIZE;

		ring = &voice.ring[(client->voice_read) & (VOICE_RING_SIZE-1)];

		/*figure out if it was for us*/
		send = false;
		if (ring->receiver[clno>>3] & (1<<(clno&3)))
			send = true;

		/*if you're spectating, you can hear whatever your tracked player can hear*/
		if (host_client->spectator && host_client->spec_track)
			if (ring->receiver[(host_client->spec_track-1)>>3] & (1<<((host_client->spec_track-1)&3)))
				send = true;


		if (client->voice_mute[ring->sender>>3] & (1<<(ring->sender&3)))
			send = false;

		if (ring->sender == clno && !sv_voip_echo.ival)
			send = false;

		/*additional ways to block voice*/
		if (client->download)
			send = false;

		if (send)
		{
			if (buf->maxsize - buf->cursize < ring->datalen+5)
				break;
			MSG_WriteByte(buf, svcfte_voicechat);
			MSG_WriteByte(buf, ring->sender);
			MSG_WriteByte(buf, ring->gen);
			MSG_WriteByte(buf, ring->seq);
			MSG_WriteShort(buf, ring->datalen);
			SZ_Write(buf, ring->data, ring->datalen);
		}
		client->voice_read++;
	}
}

void SV_Voice_Ignore_f(void)
{
	unsigned int other;
	int type = 0;

	if (Cmd_Argc() < 2)
	{
		/*only a name = toggle*/
		type = 0;
	}
	else
	{
		/*mute if 1, unmute if 0*/
		if (atoi(Cmd_Argv(2)))
			type = 1;
		else
			type = -1;
	}
	other = atoi(Cmd_Argv(1));
	if (other >= MAX_CLIENTS)
		return;

	switch(type)
	{
	case -1:
		host_client->voice_mute[other>>3] &= ~(1<<(other&3));
		break;
	case 0:
		host_client->voice_mute[other>>3] ^= (1<<(other&3));
		break;
	case 1:
		host_client->voice_mute[other>>3] |= (1<<(other&3));
	}
}
void SV_Voice_Target_f(void)
{
	unsigned int other;
	char *t = Cmd_Argv(1);
	if (!strcmp(t, "team"))
		host_client->voice_target = VT_TEAM;
	else if (!strcmp(t, "all"))
		host_client->voice_target = VT_ALL;
	else if (!strcmp(t, "nonmuted"))
		host_client->voice_target = VT_NONMUTED;
	else if (*t >= '0' && *t <= '9')
	{
		other = atoi(t);
		if (other >= MAX_CLIENTS)
			return;
		host_client->voice_target = VT_PLAYERSLOT0 + other;
	}
	else
	{
		/*don't know who you mean, futureproofing*/
		host_client->voice_target = VT_TEAM;
	}
}
void SV_Voice_MuteAll_f(void)
{
	host_client->voice_active = false;
}
void SV_Voice_UnmuteAll_f(void)
{
	host_client->voice_active = true;
}
#endif

//Use of this function is on name only.
//Be aware that the maps directory should be restricted based on weather the file was from a pack file
//this is to preserve copyright - please do not breach due to a bug.
qboolean SV_AllowDownload (const char *name)
{
	extern	cvar_t	allow_download;
	extern	cvar_t	allow_download_skins;
	extern	cvar_t	allow_download_models;
	extern	cvar_t	allow_download_sounds;
	extern	cvar_t	allow_download_demos;
	extern	cvar_t	allow_download_maps;
	extern	cvar_t	allow_download_textures;
	extern	cvar_t	allow_download_packages;
	extern	cvar_t	allow_download_wads;
	extern	cvar_t	allow_download_root;
	extern	cvar_t	allow_download_configs;
	extern	cvar_t	allow_download_copyrighted;
	char cleanname[MAX_QPATH];
	int i=0;
	if (strlen(name) >= MAX_QPATH)
		return false;
	do
	{
		cleanname[i++] = *name;
	} while(*name++);
	name = cleanname;

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

	if (!strncmp(name, "package/", 8))
	{
		if (!strcmp("pk4", COM_FileExtension(name)) || !strcmp("pk3", COM_FileExtension(name)) || !strcmp("pak", COM_FileExtension(name)))
		{
			/*do not permit 'id1/pak1.pak' or 'baseq3/pak0.pk3' or any similarly named packages. such packages would violate copyright, and must be obtained through other means (like buying the damn game)*/
			if (FS_GetPackageDownloadable(name+8))
				return !!allow_download_packages.value;
			else
				return !!allow_download_copyrighted.ival;
		}
		return false;
	}

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

	//pak/pk3s.
	if (!strcmp("pk4", COM_FileExtension(name)) || !strcmp("pk3", COM_FileExtension(name)) || !strcmp("pak", COM_FileExtension(name)))
	{
		if (strnicmp(name, "pak", 3))	//don't give out core pak/pk3 files. This matches q3 logic.
			return !!allow_download_packages.value;
		else
			return !!allow_download_copyrighted.value;
	}

	if (!strcmp("cfg", COM_FileExtension(name)))
		return !!allow_download_configs.value;

	//root of gamedir
	if (!strchr(name, '/') && !allow_download_root.value)
	{
		if (strcmp(name, "csprogs.dat"))	//we always allow csprogs.dat to be downloaded (if downloads are permitted).
			return false;
	}

	//any other subdirs are allowed
	return true;
}

static int SV_LocateDownload(char *name, flocation_t *loc, char **replacementname, qboolean redirectpaks)
{
	extern	cvar_t	allow_download_anymap, allow_download_pakcontents;
	extern cvar_t sv_demoDir;
	qboolean protectedpak;
	qboolean found;

	if (replacementname)
		*replacementname = NULL;

	//mangle the name by making it lower case.
	{
		char *p;

		for (p = name; *p; p++)
			*p = (char)tolower(*p);
	}

	

	if (!SV_AllowDownload(name))
		return -2;	//not permitted (even if it exists).

	//mvdsv demo downloading support demonum/ -> demos/XXXX (sets up the client paths)
	if (!strncmp(name, "demonum/", 8))
	{
		if (replacementname)
		{
			char mvdnamebuffer[MAX_QPATH];
			char *mvdname = SV_MVDNum(mvdnamebuffer, sizeof(mvdnamebuffer), atoi(name+8));
			if (!mvdname)
			{
				SV_ClientPrintf (host_client, PRINT_HIGH, "%s is an invalid MVD demonum.\n", name+8);
				Sys_Printf ("%s requested invalid demonum %s\n", host_client->name, name+8);
				return -1;	//not found
			}
			*replacementname = va("demos/%s\n", mvdname);
			return -4;	//redirect
		}
	}

	//mvdsv demo downloading support. demos/ -> demodir (sets up the server paths)
	if (!strncmp(name, "demos/", 6))
		name = va("%s/%s", sv_demoDir.string, name+6);

	if (!strncmp(name, "package/", 8))
	{
		vfsfile_t *f = FS_OpenVFS(name+8, "rb", FS_ROOT);
		if (f)
		{
			VFS_CLOSE(f);
			return -5;	//found package
		}
		else
			return -1;	//not found/unable to open
	}
	else
		found = FS_FLocateFile(name, FSLFRT_IFFOUND, loc);

	//nexuiz names certain files as .wav but they're really .ogg on disk.
	if (!found && replacementname)
	{
		if (!strcmp(COM_FileExtension(name), "wav"))
		{
			char tryogg[MAX_QPATH];
			COM_StripExtension(name, tryogg, sizeof(tryogg));
			COM_DefaultExtension(tryogg, ".ogg", sizeof(tryogg));

			found = FS_FLocateFile(tryogg, FSLFRT_IFFOUND, loc);
			if (found)
			{
				name = *replacementname = va("%s", tryogg);
			}
		}
	}
	//nexuiz also names files with absolute paths (yet sounds are meant to have an extra prefix)
	//this results in clients asking for sound/sound/blah.wav (or sound/sound/blah.ogg for nexuiz)
	if (!found && replacementname)
	{
		if (!strncmp(name, "sound/", 6))
		{
			int result;
			result = SV_LocateDownload(name+6, loc, replacementname, redirectpaks);
			if (!result)
			{	//if that was successful... redirect to it.
				result = -4;
				*replacementname = name+6;
			}
			return result;
		}
	}

	if (found)
	{
		protectedpak = com_file_copyprotected;

		// special check for maps, if it came from a pak file, don't allow download
		if (protectedpak)
		{
			if (!allow_download_anymap.value && !strncmp(name, "maps/", 5))
				return -2;
		}

		if (replacementname)
		{
			char *pakname = FS_WhichPackForLocation(loc);
			if (pakname && SV_AllowDownload(pakname))
			{
				//return loc of the pak instead.
				if (FS_FLocateFile(name, FSLFRT_IFFOUND, loc))
				{
					//its inside a pak file, return the name of this file instead
					*replacementname = pakname;
					return -4;	//redirect
				}
				else
					Con_Printf("Failed to read %s\n", pakname);
			}
		}

		if (protectedpak)
		{	//if its in a pak file, don't allow downloads if we don't allow the contents of paks to be sent.
			if (!allow_download_pakcontents.value)
				return -2;
		}

		if (replacementname && *replacementname)
			return -4;
		return 0;
	}
	return -1;	//not found
}

//this function is only meaningful for nq/qw
void SV_DownloadSize_f(void)
{
	flocation_t loc;
	char *name = Cmd_Argv(1);
	char *redirected = "";

	switch(SV_LocateDownload(name, &loc, &redirected, true))
	{
	case -4: /*redirect*/
		name = va("dlsize \"%s\" r \"%s\"\n", name, redirected);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(name));
		ClientReliableWrite_String (host_client, name);
		break;
	default:
	case -1: /*not found*/
		name = va("dlsize \"%s\" e\n", name);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(name));
		ClientReliableWrite_String (host_client, name);
		break;
	case -2: /*permission*/
		name = va("dlsize \"%s\" p\n", name);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(name));
		ClientReliableWrite_String (host_client, name);
		break;
	case -5: /*package*/
	case 0: /*exists*/
		name = va("dlsize \"%s\" %u\n", name, loc.len);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(name));
		ClientReliableWrite_String (host_client, name);
		break;
	}
}

/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f(void)
{
	char *name = Cmd_Argv(1);
	char *redirection = NULL;
	extern	cvar_t	allow_download_anymap, allow_download_pakcontents;
	extern cvar_t sv_demoDir;
	flocation_t loc;
	int result;

	if (ISNQCLIENT(host_client) && host_client->protocol != SCP_DARKPLACES7)
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Your client isn't meant to support downloads\n");
		return;
	}

	*host_client->downloadfn = 0;

	if (host_client->download)
	{
		VFS_CLOSE (host_client->download);
		host_client->download = NULL;
	}

	result = SV_LocateDownload(name, &loc, &redirection, false);

	if (result == -5)
	{
		result = 0;
		host_client->download = FS_OpenVFS(name+8, "rb", FS_ROOT);
	}
	else
	{
		//redirection protocol-specific code goes here.
		if (result == -4)
		{
		}

		if (result == 0)
		{	//if we are allowed and could find it
			host_client->download = FS_OpenReadLocation(&loc);
		}
	}

	if (!host_client->download)
		result = -1;	//this isn't likely, but hey.

	//handle errors
	if (result != 0)
	{	// don't allow anything with .. path
		char *error;
		switch(result)
		{
		default:
			error = "Download could not be found\n";
			break;
		case -2:
			error = "Download permission denied\n";
			break;
		case -4:
			result = -1;
			error = "";
			break;
		}
		if (ISNQCLIENT(host_client))
		{
			SV_PrintToClient(host_client, PRINT_HIGH, error);

			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+12);
			ClientReliableWrite_String (host_client, "\nstopdownload\n");
		}
#ifdef PEXT_CHUNKEDDOWNLOADS
		else if (host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
		{
			ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(name));
			ClientReliableWrite_Long (host_client, -1);
			ClientReliableWrite_Long (host_client, result);
			ClientReliableWrite_String (host_client, name);
		}
		else
#endif
		{
			SV_PrintToClient(host_client, PRINT_HIGH, error);

			ClientReliableWrite_Begin (host_client, ISQ2CLIENT(host_client)?svcq2_download:svc_download, 4);
			ClientReliableWrite_Short (host_client, -1);
			ClientReliableWrite_Byte (host_client, 0);
		}

		//it errored because it was a redirection.
		//ask the client to grab the alternate file instead.
		if (redirection)
		{
			//tell the client to download the new one.
			ClientReliableWrite_Begin (host_client, ISQ2CLIENT(host_client)?svcq2_stufftext:svc_stufftext, 2+strlen(redirection));
			ClientReliableWrite_String (host_client, va("\ndownload \"%s\"\n", redirection));
		}
		return;
	}

	Q_strncpyz(host_client->downloadfn, name, sizeof(host_client->downloadfn));
	host_client->downloadcount = 0;

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

		ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(host_client->downloadfn));
		ClientReliableWrite_Long (host_client, -1);
		ClientReliableWrite_Long (host_client, host_client->downloadsize);
		ClientReliableWrite_String (host_client, host_client->downloadfn);
	}
	else
#endif

	if (ISNQCLIENT(host_client))
	{
		char *s = va("\ncl_downloadbegin %i %s\n", host_client->downloadsize, host_client->downloadfn);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(s));
		ClientReliableWrite_String (host_client, s);
	}
	else
		SV_NextDownload_f ();

	SV_EndRedirect();
	Con_Printf ("Downloading %s to %s\n", host_client->downloadfn, host_client->name);
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
		SV_ClientPrintf(host_client, PRINT_HIGH, "But you're not downloading anything\n");

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
				Q_snprintfz (text, sizeof(text), "[SPEC] {%s}:", host_client->name);
			else
				continue;
		}
		else
			Q_snprintfz (text, sizeof(text), "{%s}:", host_client->name);

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
		Q_snprintfz (text, sizeof(text), "[SPEC] %s: ", host_client->name);
	else if (team)
		Q_snprintfz (text, sizeof(text), "(%s): ", host_client->name);
	else
		Q_snprintfz (text, sizeof(text), "%s: ", host_client->name);

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

	MSG_WriteByte (&demo.dbuf->sb, svc_print);
	MSG_WriteByte (&demo.dbuf->sb, PRINT_CHAT);
	MSG_WriteString (&demo.dbuf->sb, text);
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

#ifdef SERVER_DEMO_PLAYBACK
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
#endif
	if (ISNQCLIENT(host_client))
	{
		char *s;
		ClientReliableWrite_Begin(host_client, svc_stufftext, 15+10*MAX_CLIENTS);
		ClientReliableWrite_SZ(host_client, "pingplreport", 12);
		for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
		{
			s = va(" %i %i", SV_CalcPing(client, false), client->lossage);
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
			ClientReliableWrite_Short (host_client, SV_CalcPing(client, false));
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

#ifdef HLSERVER
	if (svs.gametype == GT_HALFLIFE)
	{
		HLSV_ClientCommand(host_client);
		return;
	}
#endif

#ifdef VM_Q1
	if (svs.gametype == GT_Q1QVM)
	{
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		Q1QVM_ClientCommand();
		return;
	}
#endif
	if (svs.gametype != GT_PROGS)
		return;

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

	pr_global_struct->time = sv.world.physicstime;
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
	int newv;

	newv = sv.paused^1;

	if (!PR_ShouldTogglePause(initiator, newv))
		return false;

	sv.paused = newv;

	sv.pausedstart = Sys_DoubleTime();

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
	extern cvar_t sv_fullredirect;

	SV_EndRedirect ();
	if (host_client->redirect)
		SV_BroadcastPrintf (PRINT_HIGH, "%s redirected to %s\n", host_client->name, sv_fullredirect.string);
	else
	{
		if (!host_client->spectator)
			SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTDROPPED, host_client->name);
	}
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

	if (!host_client->spectator
#ifdef SERVER_DEMO_PLAYBACK
		&& !sv.demofile
#endif
		)
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
#ifdef SERVER_DEMO_PLAYBACK
	if (*sv.recordedplayer[i].userinfo)
	{
		host_client->spec_track = i+1;
		return;
	}
#endif

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

	Info_SetValueForKey (host_client->userinfo, "rate", Cmd_Argv(1), sizeof(host_client->userinfo));
	SV_ExtractFromUserinfo (host_client);

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
		SV_ClientPrintf(host_client, PRINT_HIGH, "User info settings:\n");
		Info_Print (host_client->userinfo);
		return;
	}

	if (Cmd_Argc() != 3)
	{
		SV_ClientPrintf(host_client, PRINT_HIGH, "usage: setinfo [ <key> <value> ]\n");
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
					ClientReliableWrite_Begin(client, svc_setinfo, 1+1+strlen(key)+1+strlen(val)+1);
					ClientReliableWrite_Char(client, i);
					ClientReliableWrite_String(client, key);
					ClientReliableWrite_String(client, val);
				}
			}
		}

		if (sv.mvdrecording)
		{
			MVDWrite_Begin (dem_all, 0, strlen(key)+strlen(val)+4);
			MSG_WriteByte (&demo.dbuf->sb, svc_setinfo);
			MSG_WriteByte (&demo.dbuf->sb, i);
			MSG_WriteString (&demo.dbuf->sb, key);
			MSG_WriteString (&demo.dbuf->sb, val);
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
	SV_BeginRedirect(RD_CLIENT, host_client->language);
	Info_Print (svs.info);
	SV_EndRedirect();
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
	for (vote = voteinfo; vote; )
	{
		if (vote->timeout < realtime)
		{
			if (prev)
				prev->next = vote->next;
			else
				voteinfo = vote->next;

			Z_Free(vote);

			if (prev)
				vote = prev;
			else
				vote = voteinfo;
		}
		else
			prev = vote;

		vote = vote->next;
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
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_NOVOTING);
		return;
	}
	if (host_client->ismuted)
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_MUTEDVOTE);
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
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_BADVOTE);
		return;
	}
	num = Cmd_Level(command);
	if (base)
		*base = ' ';
	if (num != Cmd_ExecLevel)
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_BADVOTE);
		return;
	}


	VoteCheckTimes();

	for (num = 0; num < sv.allocated_client_slots; num++)
		if (svs.clients[num].state == cs_spawned)
			totalusers++;

	if (VoteCount(command, id))
	{
		VoteRemoveCommands(command, id);
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_OLDVOTEREMOVED);
		return;
	}
	if (VoteCount(NULL, id)>=3)
	{
		VoteRemoveCommands(NULL, id);
		SV_ClientTPrintf(host_client, PRINT_HIGH, STL_VOTESREMOVED);
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

void Cmd_Notarget_f (void)
{
	if (!SV_MayCheat())
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
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
	if (!SV_MayCheat())
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
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

#ifdef HLSERVER
	if (svs.gametype == GT_HALFLIFE)
	{
		HLSV_ClientCommand(host_client);
		return;
	}
#endif

	if (!SV_MayCheat())
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
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
	else
	{
		if (developer.value < 2 && host_client->netchan.remote_address.type != NA_LOOPBACK)	//we don't want clients doing nasty things... like setting movetype 3123
		{
			SV_PrintToClient(host_client, PRINT_HIGH, "'give' debugging command requires developer 2 set on the server before you may use it\n");
		}
		else
		{
			int oldself;
			oldself = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			SV_ClientPrintf(host_client, PRINT_HIGH, "Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, Cmd_Args()));
			pr_global_struct->self = oldself;
		}
	}
}

void Cmd_Noclip_f (void)
{
	if (!SV_MayCheat())
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
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
	if (!SV_MayCheat())
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
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
	if (!SV_MayCheat())
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	if (Cmd_Argc() != 4)
	{
		SV_ClientPrintf(host_client, PRINT_HIGH, "setpos %f %f %f\n", sv_player->v->origin[0], sv_player->v->origin[1], sv_player->v->origin[2]);
		return;
	}
	SV_LogPlayer(host_client, "setpos cheat");
	if (sv_player->v->movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v->movetype = MOVETYPE_NOCLIP;
		SV_PrintToClient(host_client, PRINT_HIGH, "noclip on\n");
	}

	sv_player->v->origin[0] = atof(Cmd_Argv(1));
	sv_player->v->origin[1] = atof(Cmd_Argv(2));
	sv_player->v->origin[2] = atof(Cmd_Argv(3));
	World_LinkEdict (&sv.world, (wedict_t*)sv_player, false);
}

void SV_SetUpClientEdict (client_t *cl, edict_t *ent)
{
	extern int pr_teamfield;
#ifdef VM_Q1
	if (svs.gametype == GT_Q1QVM)
	{
		string_t preserve;
		preserve = ent->v->netname;
		Q1QVMED_ClearEdict(ent, true);
		ent->v->netname = preserve;
	}
	else
#endif
	{
		if (progstype != PROG_NQ)	//allow frikbots to work in NQ mods (but not qw!)
			ED_Clear(svprogfuncs, ent);
		ent->v->netname = PR_SetString(svprogfuncs, cl->name);
	}
	ED_Spawned(ent, false);
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

	ent->v->frags = 0;
	cl->connection_started = realtime;
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
		SV_PrintToClient(host_client, PRINT_HIGH, "Sorry, not implemented in this gamecode type. Try moaning at the dev team\n");
		return;
	}

	if (!(host_client->zquake_extensions & Z_EXT_JOIN_OBSERVE))
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Your QW client doesn't support this command\n");
		return;
	}

	if (password.string[0] && stricmp(password.string, "none"))
	{
		SV_ClientPrintf(host_client, PRINT_HIGH, "This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "player", "player");
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
		SV_PrintToClient(host_client, PRINT_HIGH, "Can't join, all player slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	if (SpectatorDisconnect)
		PR_ExecuteProgram (svprogfuncs, SpectatorDisconnect);

	SV_SetUpClientEdict (host_client, host_client->edict);

	// turn the spectator into a player
	host_client->spectator = false;
	Info_RemoveKey (host_client->userinfo, "*spectator");
	Info_RemoveKey (host_client->userinfobasic, "*spectator");

	// FIXME, bump the client's userid?

	// call the progs to get default spawn parms for the new client
	if (pr_global_ptrs->SetNewParms)
		PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
	{
		if (pr_global_ptrs->spawnparamglobals[i])
			host_client->spawn_parms[i] = *pr_global_ptrs->spawnparamglobals[i];
		else
			host_client->spawn_parms[i] = 0;
	}

	// call the spawn function
	pr_global_struct->time = sv.world.physicstime;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);

	// actually spawn the player
	pr_global_struct->time = sv.world.physicstime;
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->PutClientInServer);
	sv.spawned_client_slots++;

	// send notification to all clients
	host_client->old_frags = host_client->edict->v->frags;
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
		SV_PrintToClient(host_client, PRINT_HIGH, "Sorry, not implemented in this gamecode type. Try moaning at the dev team\n");
		return;
	}

	if (!(host_client->zquake_extensions & Z_EXT_JOIN_OBSERVE))
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "Your QW client doesn't support this command\n");
		return;
	}

	if (spectator_password.string[0] && stricmp(spectator_password.string, "none"))
	{
		SV_ClientPrintf(host_client, PRINT_HIGH, "This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "spectator", "spectator");
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
		SV_PrintToClient(host_client, PRINT_HIGH, "Can't join, all spectator slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);
	sv.spawned_client_slots--;

	SV_SetUpClientEdict (host_client, host_client->edict);

	// turn the player into a spectator
	host_client->spectator = true;
	Info_SetValueForStarKey (host_client->userinfo, "*spectator", "1", sizeof(host_client->userinfo));
	Info_SetValueForStarKey (host_client->userinfobasic, "*spectator", "1", sizeof(host_client->userinfobasic));

	// FIXME, bump the client's userid?

	// call the progs to get default spawn parms for the new client
	if (pr_global_ptrs->SetNewParms)
		PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
	{
		if (pr_global_ptrs->spawnparamglobals[i])
			host_client->spawn_parms[i] = *pr_global_ptrs->spawnparamglobals[i];
		else
			host_client->spawn_parms[i] = 0;
	}

	SV_SpawnSpectator ();

	// call the spawn function
	if (SpectatorConnect)
	{
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
	}
	else
		sv_player->v->movetype = MOVETYPE_NOCLIP;

	// send notification to all clients
	host_client->old_frags = host_client->edict->v->frags;
	host_client->sendinfo = true;

	SV_LogPlayer(host_client, "observing");
}

void Cmd_FPSList_f(void)
{
	client_t *cl;
	int c;
	int f;
	double minf = 1000, maxf = 0, this;
	double ftime;
	int frames;
	int inbytes;
	int outbytes;
	int msecs;


	for (c = 0; c < sv.allocated_client_slots; c++)
	{
		cl = &svs.clients[c];
		ftime = 0;
		frames = 0;
		inbytes = 0;
		outbytes = 0;
		msecs = 0;

		if (!cl->state)
			continue;

		if (ISQWCLIENT(cl) || ISNQCLIENT(cl))
		{
			if (cl->frameunion.frames)
			{
				for (f = 0; f < UPDATE_BACKUP; f++)
				{
					if (cl->frameunion.frames[f].move_msecs >= 0)
					{
						if (!cl->frameunion.frames[f].move_msecs)
						{
							this = 1001;
							msecs+=1;
						}
						else
						{
							this = 1000.0f/cl->frameunion.frames[f].move_msecs;
							msecs += cl->frameunion.frames[f].move_msecs;
						}
						ftime += this;
						if (minf > this)
							minf = this;
						if (maxf < this)
							maxf = this;
						frames++;

						inbytes += cl->frameunion.frames[f].packetsizein;
						outbytes += cl->frameunion.frames[f].packetsizeout;
					}
				}
			}
		}

		if (frames)
			SV_ClientPrintf(host_client, PRINT_HIGH, "%s: %ffps (min%.2f max %.2f), in: %.2fbps, out: %.2fbps\n", cl->name, ftime/frames, minf, maxf, (1000.0f*inbytes)/msecs, (1000.0f*outbytes)/msecs);
		else
			SV_ClientPrintf(host_client, PRINT_HIGH, "%s: no information available\n", cl->name);
	}
}

void SV_EnableClientsCSQC(void)
{
#ifdef PEXT_CSQC
	if (host_client->fteprotocolextensions & PEXT_CSQC || atoi(Cmd_Argv(1)))
		host_client->csqcactive = true;
	else
		SV_ClientPrintf(host_client, PRINT_HIGH, "CSQC entities not enabled - no support from network protocol\n");
#endif
}
void SV_DisableClientsCSQC(void)
{
#ifdef PEXT_CSQC
	host_client->csqcactive = false;
#endif
}

void SV_UserCmdMVDList_f (void);
void SV_STFU_f(void)
{
	char *msg;
	SV_ClientPrintf(host_client, 255, "stfu\n");
	msg = "cl_antilag 0\n";
	ClientReliableWrite_Begin(host_client, svc_stufftext, 2+strlen(msg));
	ClientReliableWrite_String(host_client, msg);
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
	SV_MVD_FullClientUpdate(NULL, host_client);

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
		ED_Clear(svprogfuncs, ent);
		ED_Spawned(ent, false);

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
					if (pr_global_ptrs->spawnparamglobals[i])
						*pr_global_ptrs->spawnparamglobals[i] = host_client->spawn_parms[i];
				}

				// call the spawn function
				pr_global_struct->time = sv.world.physicstime;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
				PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
			}
		}
		else
		{
			sv.spawned_client_slots++;

			// copy spawn parms out of the client_t
			for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			{
				if (pr_global_ptrs->spawnparamglobals[i])
					*pr_global_ptrs->spawnparamglobals[i] = host_client->spawn_parms[i];
			}

			// call the spawn function
			pr_global_struct->time = sv.world.physicstime;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);

			// actually spawn the player
			pr_global_struct->time = sv.world.physicstime;
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
			SV_BroadcastPrintf (PRINT_HIGH, "warning: %s eyes or player model not verified\n", host_client->name);
	}


	// if we are paused, tell the client
	if (sv.paused)
	{
		if (!ISQ2CLIENT(host_client))
		{
			ClientReliableWrite_Begin (host_client, svc_setpause, 2);
			ClientReliableWrite_Byte (host_client, sv.paused!=0);
		}
		if (sv.paused&~4)
			SV_ClientTPrintf(host_client, PRINT_HIGH, STL_SERVERPAUSED!=0);
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
	int buf = atoi(Cmd_Argv(1));
	int st;
	if (host_client->state != cs_connected)
	{
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	st = 0;
	if (buf >= st)
	{
		while (host_client->netchan.message.cursize < (host_client->netchan.message.maxsize/2))	//baselines
		{
			e = buf-st;
			if (e >= sv.world.num_edicts)
			{
				if (e < sv.world.max_edicts)
					buf += sv.world.max_edicts - sv.world.num_edicts;
				break;
			}
			buf++;

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
				if (ISDPCLIENT(host_client) && (state->modelindex > 255 || state->frame > 255))
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
	}
	st += sv.world.max_edicts;

	if (buf >= st)
	{
		while (host_client->netchan.message.cursize < (host_client->netchan.message.maxsize/2))
		{
			i = buf-st;
			if (i >= sv.num_signon_buffers)
				break;
			buf++;
			SZ_Write (&host_client->netchan.message, sv.signon_buffers[i], sv.signon_buffer_size[i]);
		}
	}
	st += sv.num_signon_buffers;

	if (st == buf)
	{
		MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
		MSG_WriteByte (&host_client->netchan.message, 2);
	}
	else
	{
		char *s = va("cmd prespawn %i\n", buf);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(s));
		ClientReliableWrite_String (host_client, s);
	}

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

	SV_PrintToClient(host_client, PRINT_HIGH, "Client ping times:\n");
	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
		if (!cl->state)
			continue;

		SV_PrintToClient(host_client, PRINT_HIGH, va("%3i %s\n", SV_CalcPing (cl, false), cl->name));
	}
}

void SV_Pext_f(void)
{
	int i;
	char *tag;
	char *val;

	if (host_client->pextknown)
		return;
	host_client->pextknown = true;

	for (i = 1; i < Cmd_Argc(); )
	{
		tag = Cmd_Argv(i++);
		val = Cmd_Argv(i++);
		switch(strtoul(tag, NULL, 0))
		{
		case PROTOCOL_VERSION_FTE:
			host_client->fteprotocolextensions = strtoul(val, NULL, 0) & svs.fteprotocolextensions;
			break;
		case PROTOCOL_VERSION_FTE2:
			host_client->fteprotocolextensions2 = strtoul(val, NULL, 0) & svs.fteprotocolextensions2;
			break;
		}
	}

	if (ISNQCLIENT(host_client))
		SVNQ_New_f();
	else
		SV_New_f();
}

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
	/*connection process*/
	{"new", SV_New_f, true},
#ifdef PEXT_PK3DOWNLOADS
	{"pk3list",	SV_PK3List_f, true},
#endif
	{"modellist", SV_Modellist_f, true},
	{"soundlist", SV_Soundlist_f, true},
	{"prespawn", SVQW_PreSpawn_f, true},
	{"spawn", SV_Spawn_f, true},
	{"begin", SV_Begin_f, true},

	/*ezquake warning*/
	{"al", SV_STFU_f, true},

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

	/*demo/download commands*/
	{"stopdownload", SV_StopDownload_f},
	{"demolist", SV_UserCmdMVDList_f},
	{"demoinfo", SV_MVDInfo_f},
	{"dlsize", SV_DownloadSize_f},
	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},

	/*quakeworld specific things*/
	{"join", Cmd_Join_f},
	{"observe", Cmd_Observe_f},
	{"snap", SV_NoSnap_f},
	{"ptrack", SV_PTrack_f}, //ZOID - used with autocam

	{"enablecsqc", SV_EnableClientsCSQC},
	{"disablecsqc", SV_DisableClientsCSQC},

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

#ifdef VOICECHAT
	{"voicetarg", SV_Voice_Target_f},
	{"vignore", SV_Voice_Ignore_f},	/*ignore/mute specific player*/
	{"muteall", SV_Voice_MuteAll_f},	/*disables*/
	{"unmuteall", SV_Voice_UnmuteAll_f}, /*reenables*/
#endif

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

#ifdef NQPROT
ucmd_t nqucmds[] =
{
	{"new",			SVNQ_New_f, true},
	{"spawn",		SVNQ_Spawn_f, true},
	{"begin",		SVNQ_Begin_f, true},
	{"prespawn",	SVNQ_PreSpawn_f, true},

	{"status",		NULL},


	{"god",			Cmd_God_f},
	{"give",		Cmd_Give_f},
	{"notarget",	Cmd_Notarget_f},
	{"fly",			Cmd_Fly_f},
	{"noclip",		Cmd_Noclip_f},

	{"say",			SV_Say_f},
	{"say_team",	SV_Say_Team_f},
	{"tell",		SV_SayOne_f},
	{"efpslist",	Cmd_FPSList_f},	//don't conflict with the ktpro one

	{"pings",		SV_Pings_f},
	{"ping",		SVNQ_Ping_f},

	{"kill",		SV_Kill_f},
	{"pause",		SV_Pause_f},
	{"kick",		NULL},
	{"ban",			NULL},
	{"vote",		SV_Vote_f},

	/*DP download protocol*/
	{"dlsize",		SV_DownloadSize_f},
	{"download",	SV_BeginDownload_f},
	{"sv_startdownload",	SVDP_StartDownload_f},

	/*userinfo stuff*/
	{"setinfo",		SV_SetInfo_f},
	{"name",		SVNQ_NQInfo_f},
	{"color",		SVNQ_NQColour_f},
	{"playermodel",	NULL},
	{"playerskin",	NULL},
	{"rate",		SV_Rate_f},

#ifdef SVRANKING
	{"topten",		Rank_ListTop10_f},
#endif

	/*various misc extensions*/
	{"pext",		SV_Pext_f},
	{"enablecsqc",	SV_EnableClientsCSQC},
	{"disablecsqc",	SV_DisableClientsCSQC},
	{"challengeconnect", NULL},

#ifdef VOICECHAT
	{"voicetarg",	SV_Voice_Target_f},
	{"vignore",		SV_Voice_Ignore_f},	/*ignore/mute specific player*/
	{"muteall",		SV_Voice_MuteAll_f},	/*disables*/
	{"unmuteall",	SV_Voice_UnmuteAll_f}, /*reenables*/
#endif

	{NULL, NULL}
};
#endif

/*
==================
SV_ExecuteUserCommand
==================
*/
void SV_ExecuteUserCommand (char *s, qboolean fromQC)
{
	ucmd_t	*u;
	client_t *oldhost = host_client;
	char adr[MAX_ADR_SIZE];

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
//			SV_BeginRedirect (RD_CLIENT, host_client->language);
			if (u->func)
				u->func ();
			host_client = oldhost;
//			SV_EndRedirect ();
			return;
		}

	if (!u->name)
	{
#ifdef HLSERVER
		if (HLSV_ClientCommand(host_client))
		{
			host_client = oldhost;
			return;
		}
#endif

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
						, NET_AdrToString (adr, sizeof(adr), net_from), "Was too long - possible buffer overflow attempt");
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

int implevels[256];
qboolean SV_FilterImpulse(int imp, int level)
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
void AddLinksToPmove ( edict_t *player, areanode_t *node )
{
	int Q1_HullPointContents (hull_t *hull, int num, vec3_t p);
	link_t		*l, *next;
	edict_t		*check;
	int			pl;
	int			i;
	physent_t	*pe;

	model_t *model;

	pl = EDICT_TO_PROG(svprogfuncs, player);

	// touch linked edicts
	for (l = node->edicts.next ; l != &node->edicts ; l = next)
	{
		next = l->next;
		check = (edict_t*)EDICT_FROM_AREA(l);

		if (check->v->owner == pl)
			continue;		// player's own missile
		if (check->v->solid == SOLID_BSP
			|| check->v->solid == SOLID_BBOX
			|| check->v->solid == SOLID_SLIDEBOX
			//|| (check->v->solid == SOLID_PHASEH2 && progstype == PROG_H2) //logically matches hexen2, but I hate it
			)
		{
			if (check == player)
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
			pe->notouch = !((int)player->xv->dimension_solid & (int)check->xv->dimension_hit);
			if (!((int)player->xv->dimension_hit & (int)check->xv->dimension_solid))
				continue;
			if (!check->v->size[0])	//points are not meant to be solid
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
	if (player->v->mins[2] != 24)	//crouching/dead
	for (l = node->edicts.next ; l != &node->edicts ; l = next)
	{
		next = l->next;
		check = (edict_t*)EDICT_FROM_AREA(l);

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

			if (!((int)player->xv->dimension_hit & (int)check->xv->dimension_solid))
				continue;

			model = sv.models[(int)check->v->modelindex];
			if (model)
			{
				vec3_t axis[3];
				AngleVectors(check->v->angles, axis[0], axis[1], axis[2]);
				VectorNegate(axis[1], axis[1]);
	// test the point
				if (model->funcs.PointContents (model, axis, player->v->origin) == FTECONTENTS_SOLID)
					player->xv->pmove_flags = (int)player->xv->pmove_flags | PMF_LADDER;	//touch that ladder!
			}
		}
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if (pmove_maxs[node->axis] > node->dist)
		AddLinksToPmove (player, node->children[0]);
	if (pmove_mins[node->axis] < node->dist)
		AddLinksToPmove (player, node->children[1]);
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
	for (e=1 ; e<sv.world.num_edicts ; e++)
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
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demostatevalid)
	{	//force noclip... This does create problems for closing demos.
		if (cl->zquake_extensions & Z_EXT_PM_TYPE_NEW)
			return PM_SPECTATOR;
		return PM_OLD_SPECTATOR;
	}
#endif

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
	char adr[MAX_ADR_SIZE];
	vec3_t new_vel;
	vec3_t old_vel;

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
								host_client->name, NET_AdrToString(adr, sizeof(adr), host_client->netchan.remote_address));
					host_client->drop = true;	//drop later
				}
		    }

		    host_client->msecs = 0;
		    host_client->last_check = realtime;
		}
	}
	// end KK hack copied from QuakeForge anti-cheat
	//it's amazing how code get's copied around...

	if (SV_RunFullQCMovement(host_client, ucmd))
	{
		return;
	}


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

#ifdef SERVER_DEMO_PLAYBACK
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
		pmove.physents[0].model = sv.world.worldmodel;
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
#endif

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
	if (pr_allowbutton1.ival)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
		sv_player->v->button1 = ((ucmd->buttons >> 2) & 1);
// DP_INPUTBUTTONS
	sv_player->xv->button3 = ((ucmd->buttons >> 2) & 1);
	sv_player->xv->button4 = ((ucmd->buttons >> 3) & 1);
	sv_player->xv->button5 = ((ucmd->buttons >> 4) & 1);
	sv_player->xv->button6 = ((ucmd->buttons >> 5) & 1);
	sv_player->xv->button7 = ((ucmd->buttons >> 6) & 1);
	sv_player->xv->button8 = ((ucmd->buttons >> 7) & 1);
	if (ucmd->impulse && SV_FilterImpulse(ucmd->impulse, host_client->trustlevel))
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

	WPhys_CheckVelocity(&sv.world, (wedict_t*)sv_player);

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

	if (SV_PlayerPhysicsQC && !host_client->spectator)
	{	//csqc independant physics support
		pr_global_struct->frametime = host_frametime;
		pr_global_struct->time = sv.time;
		WPhys_RunEntity(&sv.world, (wedict_t*)sv_player);
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
			jumpable = ((int)sv_player->v->flags & FL_JUMPRELEASED) && ((int)sv_player->v->flags & FL_ONGROUND);

			pmove.waterjumptime = sv_player->v->teleport_time;
			if (pmove.waterjumptime > sv.time)
				sv_player->v->flags = (int)sv_player->v->flags | FL_WATERJUMP;
		}
		else
			jumpable = false;

#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_PlayerPreThink();
		else
#endif
			if (pr_global_ptrs->PlayerPreThink)
				PR_ExecuteProgram (svprogfuncs, *pr_global_ptrs->PlayerPreThink);

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

		WPhys_RunThink (&sv.world, (wedict_t*)sv_player);
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
	pmove.physents[0].model = sv.world.worldmodel;
	pmove.cmd = *ucmd;
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
	sv_player->xv->pmove_flags = (int)sv_player->xv->pmove_flags & ~PMF_LADDER;	//assume not touching ladder trigger
#if 1
	AddLinksToPmove ( sv_player, sv.world.areanodes );
#else
	AddAllEntsToPmove ();
#endif

	if ((int)sv_player->xv->pmove_flags & PMF_LADDER)
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
	{
		if (pmove.waterjumptime)
			sv_player->v->teleport_time = pmove.waterjumptime + sv.time;
	}
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

	VectorCopy (pmove.origin, sv_player->v->origin);
	VectorCopy (pmove.angles, sv_player->v->v_angle);

	player_mins[0] = -16;
	player_mins[1] = -16;
	player_mins[2] = -24;

	player_maxs[0] = 16;
	player_maxs[1] = 16;
	player_maxs[2] = 32;

	VectorCopy(sv_player->v->velocity, old_vel);
	VectorCopy(pmove.velocity, new_vel);
	if (progstype == PROG_QW)
		VectorCopy(new_vel, sv_player->v->velocity);


	if (!host_client->spectator)
	{
		// link into place and touch triggers
		World_LinkEdict (&sv.world, (wedict_t*)sv_player, true);

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
			if (playertouch[n/8]&(1<<(n%8)))
				continue;

			if (ent->v->touch)
			{
				if (progstype != PROG_QW && VectorCompare(sv_player->v->velocity, old_vel))
				{
					VectorCopy(pmove.touchvel[i], old_vel);
					VectorCopy(pmove.touchvel[i], sv_player->v->velocity);
				}
				sv.world.Event_Touch(&sv.world, (wedict_t*)ent, (wedict_t*)sv_player);
			}
			playertouch[n/8] |= 1 << (n%8);

			if (sv_player->v->touch && !ent->isfree)
				sv.world.Event_Touch(&sv.world, (wedict_t*)sv_player, (wedict_t*)ent);
		}
	}

	if (progstype != PROG_QW)
	{
		if (VectorCompare(sv_player->v->velocity, old_vel))
			VectorCopy(new_vel, sv_player->v->velocity);
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

			if (pr_global_ptrs->PlayerPostThink)
				PR_ExecuteProgram (svprogfuncs, *pr_global_ptrs->PlayerPostThink);

			WPhys_RunNewmis (&sv.world);
		}
		else if (SpectatorThink)
		{
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, SpectatorThink);
		}
	}
}

void SV_ReadPrydonCursor(void)
{
	float f;
	int entnum;
	eval_t *cursor_screen, *cursor_start, *cursor_impact, *cursor_entitynumber;

	if (svprogfuncs)
	{
		cursor_screen	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_screen", NULL);
		cursor_start	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_trace_start", NULL);
		cursor_impact	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_trace_endpos", NULL);
		cursor_entitynumber	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_trace_ent", NULL);
	}
	else
	{
		cursor_screen	= NULL;
		cursor_start	= NULL;
		cursor_impact	= NULL;
		cursor_entitynumber	= NULL;
	}

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
	if (entnum >= sv.world.max_edicts)
	{
		Con_DPrintf("SV_ReadPrydonCursor: client send bad cursor_entitynumber\n");
		entnum = 0;
	}
	// as requested by FrikaC, cursor_trace_ent is reset to world if the
	// entity is free at time of receipt
	if (!svprogfuncs || EDICT_NUM(svprogfuncs, entnum)->isfree)
		entnum = 0;
	if (msg_badread) Con_Printf("SV_ReadPrydonCursor: badread at %s:%i\n", __FILE__, __LINE__);

	if (cursor_entitynumber) cursor_entitynumber->edict = entnum;
}

void SV_ReadQCRequest(void)
{
	int e;
	char args[7];
	char *rname;
	func_t f;
	int i;
	globalvars_t *pr_globals;

	if (!svprogfuncs)
	{
		msg_badread = true;
		return;
	}

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	for (i = 0; ; i++)
	{
		if (i >= sizeof(args))
		{
			if (MSG_ReadByte() != ev_void)
			{
				msg_badread = true;
				return;
			}
			goto done;
		}
		switch(MSG_ReadByte())
		{
		case ev_void:
			goto done;
		case ev_float:
			args[i] = 'f';
			G_FLOAT(OFS_PARM0+i*3) = MSG_ReadFloat();
			break;
		case ev_vector:
			args[i] = 'v';
			G_FLOAT(OFS_PARM0+i*3+0) = MSG_ReadFloat();
			G_FLOAT(OFS_PARM0+i*3+1) = MSG_ReadFloat();
			G_FLOAT(OFS_PARM0+i*3+2) = MSG_ReadFloat();
			break;
		case ev_integer:
			args[i] = 'i';
			G_INT(OFS_PARM0+i*3) = MSG_ReadLong();
			break;
		case ev_string:
			args[i] = 's';
			G_INT(OFS_PARM0+i*3) = PR_TempString(svprogfuncs, MSG_ReadString());
			break;
		case ev_entity:
			args[i] = 'e';
			e = MSG_ReadShort();
			if (e < 0 || e >= sv.world.num_edicts)
				e = 0;
			G_INT(OFS_PARM0+i*3) = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, e));
			break;
		}
	}

done:
	args[i] = 0;
	rname = MSG_ReadString();
	if (i)
		rname = va("Cmd_%s_%s", rname, args);
	else
		rname = va("Cmd_%s", rname);
	f = PR_FindFunction(svprogfuncs, rname, PR_ANY);
	if (f)
	{
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		PR_ExecuteProgram(svprogfuncs, f);
	}
	else
		SV_ClientPrintf(host_client, PRINT_HIGH, "qcrequest \"%s\" not supported\n", rname);
}

/*
===================
SV_ExecuteClientMessage

The current net_message is parsed for the given client
===================
*/
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

		if (sv_antilag.ival)
		{
			/*
			extern cvar_t temp1;
			if (temp1.ival)
			frame = &cl->frameunion.frames[(cl->netchan.incoming_acknowledged+temp1.ival) & UPDATE_MASK];
			*/
#ifdef warningmsg
#pragma warningmsg("FIXME: make antilag optionally support non-player ents too")
#endif
			for (i = 0; i < sv.allocated_client_slots; i++)
			{
				cl->laggedents[i].present = frame->playerpresent[i];
				if (cl->laggedents[i].present)
					VectorCopy(frame->playerpositions[i], cl->laggedents[i].laggedpos);
			}
			cl->laggedents_count = sv.allocated_client_slots;

			cl->laggedents_frac = sv_antilag_frac.value;
		}
		else
			cl->laggedents_count = 0;
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
		cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].packetsizein = net_message.cursize;
		cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].packetsizeout = 0;
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
							break;
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

#ifdef HLSERVER
					if (svs.gametype == GT_HALFLIFE)
					{
						SVHL_RunPlayerCommand(cl, &oldest, &oldcmd, &newcmd);
					}
					else
#endif
					if (!sv.paused)
					{
						if (sv_nomsec.ival)
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
							if (pr_allowbutton1.ival)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
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

						if (!SV_PlayerPhysicsQC || host_client->spectator)
							SV_PostRunCmd();

					}
					else
					{
						if (newcmd.impulse)// && SV_FiltureImpulse(newcmd.impulse, host_client->trustlevel))
							sv_player->v->impulse = newcmd.impulse;
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

		case clc_prydoncursor:
			SV_ReadPrydonCursor();
			break;
		case clc_qcrequest:
			SV_ReadQCRequest();
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
#ifdef SERVER_DEMO_PLAYBACK
			if (sv.mvdplayback)
			{
				VectorCopy(o, host_client->specorigin);
			}
			else
#endif
			if (host_client->spectator)
			{
				VectorCopy(o, sv_player->v->origin);
				World_LinkEdict(&sv.world, (wedict_t*)sv_player, false);
			}
			break;

		case clc_upload:
			SV_NextUpload();
			break;
#ifdef VOICECHAT
		case clc_voicechat:
			SV_VoiceReadPacket();
			break;
#endif
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

#ifdef VOICECHAT
		case clc_voicechat:
			SV_VoiceReadPacket();
			break;
#endif
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
	float timesincelast;
	float cltime;

	frame = &host_client->frameunion.frames[host_client->netchan.incoming_acknowledged & UPDATE_MASK];

	if (host_client->protocol == SCP_DARKPLACES7)
		host_client->last_sequence = MSG_ReadLong ();
	else
		host_client->last_sequence = 0;
	cltime = MSG_ReadFloat ();
	if (cltime < move->fservertime)
		cltime = move->fservertime;
	if (cltime > sv.time)
		cltime = sv.time;
	if (cltime < sv.time - 2)	//if you do lag more than this, you won't get your free time.
		cltime = sv.time - 2;
	timesincelast = cltime - move->fservertime;
	move->fservertime = cltime;
	move->servertime = move->fservertime;

	frame->ping_time = sv.time - cltime;


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

	move->msec=timesincelast*1000;//MSG_ReadFloat;


	frame->move_msecs = move->msec;

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
		SV_ReadPrydonCursor();
	}

	if (SV_RunFullQCMovement(host_client, move))
	{
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_PostThink();
		else
#endif
		{
			if (pr_global_struct->PlayerPostThink)
				PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPostThink);
		}
		host_client->isindependant = true;
		return;
	}


	if (i && SV_FilterImpulse(i, host_client->trustlevel))
		host_client->edict->v->impulse = i;

	host_client->edict->v->button0 = bits & 1;
	host_client->edict->v->button2 = (bits >> 1) & 1;
	if (pr_allowbutton1.ival)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
		host_client->edict->v->button1 = ((bits >> 2) & 1);
// DP_INPUTBUTTONS
	host_client->edict->xv->button3 = ((bits >> 2) & 1);
	host_client->edict->xv->button4 = ((bits >> 3) & 1);
	host_client->edict->xv->button5 = ((bits >> 4) & 1);
	host_client->edict->xv->button6 = ((bits >> 5) & 1);
	host_client->edict->xv->button7 = ((bits >> 6) & 1);
	host_client->edict->xv->button8 = ((bits >> 7) & 1);

	if (host_client->last_sequence)
	{
		host_frametime = timesincelast;
		WPhys_RunEntity(&sv.world, (wedict_t*)host_client->edict);
		host_client->isindependant = true;
	}
	else
		host_client->isindependant = false;
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
	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].move_msecs = -1;
	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].packetsizein = net_message.cursize;
	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].packetsizeout = 0;

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
			host_client = cl;
			sv_player = cl->edict;
			SV_Drop_f();
			break;
		case clc_nop:
			break;

//		case clc_delta:
//			cl->delta_sequence = MSG_ReadByte ();
//			break;

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

		case clcdp_ackframe:
			cl->delta_sequence = MSG_ReadLong();
			break;
		case clcdp_ackdownloaddata:
			SV_DarkPlacesDownloadAck(cl);
			break;

#ifdef VOICECHAT
		case clc_voicechat:
			SV_VoiceReadPacket();
			break;
#endif
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
#ifdef VOICECHAT
	Cvar_Register (&sv_voip, cvargroup_serverpermissions);
	Cvar_Register (&sv_voip_echo, cvargroup_serverpermissions);
	Cvar_Register (&sv_voip_record, cvargroup_serverpermissions);
#endif
#ifdef SERVERONLY
	Cvar_Register (&cl_rollspeed, "Prediction stuff");
	Cvar_Register (&cl_rollangle, "Prediction stuff");
#endif
	Cvar_Register (&sv_chatfilter, cvargroup_serverpermissions);
	Cvar_Register (&sv_spectalk, cvargroup_servercontrol);
	Cvar_Register (&sv_mapcheck, cvargroup_servercontrol);

	Cvar_Register (&sv_fullredirect, cvargroup_servercontrol);
	Cvar_Register (&sv_antilag, cvargroup_servercontrol);
	Cvar_Register (&sv_antilag_frac, cvargroup_servercontrol);
	Cvar_Register (&sv_cheatpc, cvargroup_servercontrol);
	Cvar_Register (&sv_cheatspeedchecktime, cvargroup_servercontrol);
	Cvar_Register (&sv_playermodelchecks, cvargroup_servercontrol);

	Cvar_Register (&sv_getrealip, cvargroup_servercontrol);
	Cvar_Register (&sv_realip_kick, cvargroup_servercontrol);
	Cvar_Register (&sv_realiphostname_ipv4, cvargroup_servercontrol);
	Cvar_Register (&sv_realiphostname_ipv6, cvargroup_servercontrol);
	Cvar_Register (&sv_realip_timeout, cvargroup_servercontrol);

	Cvar_Register (&sv_pushplayers, cvargroup_servercontrol);

	Cvar_Register (&sv_pure, cvargroup_servercontrol);
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
static void SV_UserFriction (void)
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

	trace = World_Move (&sv.world, start, vec3_origin, vec3_origin, stop, true, (wedict_t*)sv_player);

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

static void SV_Accelerate (void)
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

static void SV_AirAccelerate (vec3_t wishveloc)
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
static void SV_AirMove (void)
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

static void SV_WaterMove (void)
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

static void SV_WaterJump (void)
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

	if (SV_PlayerPhysicsQC && !host_client->spectator)
	{
		pr_global_struct->time = sv.world.physicstime;
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
