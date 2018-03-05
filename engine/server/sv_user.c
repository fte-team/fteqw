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

#include "quakedef.h"
#include "fs.h"

#ifndef CLIENTONLY
#include "pr_common.h"

#include <ctype.h>
#define Q2EDICT_NUM(i) (q2edict_t*)((char *)ge->edicts+i*ge->edict_size)
#define Q2NUM_FOR_EDICT(ent) (((char *)ent - (char *)ge->edicts) / ge->edict_size)
hull_t *SV_HullForEntity (edict_t *ent, int hullnum, vec3_t mins, vec3_t maxs, vec3_t offset);

edict_t	*sv_player;

usercmd_t	cmd;

extern cvar_t dpcompat_nopreparse;
#ifdef SERVERONLY
cvar_t	cl_rollspeed = CVAR("cl_rollspeed", "200");
cvar_t	cl_rollangle = CVAR("cl_rollangle", "2.0");
#else
extern cvar_t	cl_rollspeed;
extern cvar_t	cl_rollangle;
#endif
cvar_t	sv_spectalk	= CVAR("sv_spectalk", "1");

cvar_t	sv_mapcheck	= CVAR("sv_mapcheck", "1");

cvar_t	sv_fullredirect = CVARD("sv_fullredirect", "", "This is the ip:port to redirect players to when the server is full");
cvar_t	sv_antilag			= CVARFD("sv_antilag", "", CVAR_SERVERINFO, "Attempt to backdate impacts to compensate for lag via the MOVE_ANTILAG feature.\n0=completely off.\n1=mod-controlled (default).\n2=forced, which might break certain uses of traceline.\n3=Also attempt to recalculate trace start positions to avoid lagged knockbacks.");
cvar_t	sv_antilag_frac		= CVARF("sv_antilag_frac", "", CVAR_SERVERINFO);
#ifndef NEWSPEEDCHEATPROT
cvar_t	sv_cheatpc				= CVARD("sv_cheatpc", "125", "If the client tried to claim more than this percentage of time within any speed-cheat period, the client will be deemed to have cheated.");
cvar_t	sv_cheatspeedchecktime	= CVARD("sv_cheatspeedchecktime", "30", "The interval between each speed-cheat check.");
#endif
cvar_t	sv_playermodelchecks	= CVAR("sv_playermodelchecks", "0");
cvar_t	sv_ping_ignorepl		= CVARD("sv_ping_ignorepl", "0", "If 1, ping times reported for players will ignore the effects of packetloss on ping times. 0 is slightly more honest, but less useful for connection diagnosis.");
cvar_t	sv_protocol_nq		= CVARD("sv_protocol_nq", "", "Specifies the default protocol to use for new NQ clients. This is only relevent for clients that do not report their supported protocols. Supported values are\n0 = autodetect\n15 = vanilla\n666 = fitzquake\n999 = rmq protocol\nThe sv_bigcoords cvar forces upgrades as required.");

cvar_t	sv_minpitch		 = CVARAFD("minpitch", "",	"sv_minpitch", CVAR_SERVERINFO, "Assumed to be -70");
cvar_t	sv_maxpitch		 = CVARAFD("maxpitch", "",	"sv_maxpitch", CVAR_SERVERINFO, "Assumed to be 80");

cvar_t	sv_cmdlikercon	= CVAR("sv_cmdlikercon", "0");	//set to 1 to allow a password of username:password instead of the correct rcon password.
cvar_t cmd_allowaccess	= CVAR("cmd_allowaccess", "0");	//set to 1 to allow cmd to execute console commands on the server.
cvar_t cmd_gamecodelevel	= CVAR("cmd_gamecodelevel", STRINGIFY(RESTRICT_LOCAL));	//execution level which gamecode is told about (for unrecognised commands)

cvar_t	sv_pure	= CVARFD("sv_pure", "", CVAR_SERVERINFO, "The most evil cvar in the world, many clients will ignore this.\n0=standard quake rules.\n1=clients should prefer files within packages present on the server.\n2=clients should use *only* files within packages present on the server.\nDue to quake 1.01/1.06 differences, a setting of 2 only works in total conversions.");
cvar_t	sv_nqplayerphysics	= CVARAD("sv_nqplayerphysics", "0", "sv_nomsec", "Disable player prediction and run NQ-style player physics instead. This can be used for compatibility with mods that expect exact behaviour.");
cvar_t	sv_edgefriction	= CVARAF("sv_edgefriction", "2",
								 "edgefriction", 0);

#ifndef NOLEGACY
cvar_t	sv_brokenmovetypes	= CVARD("sv_brokenmovetypes", "0", "Emulate vanilla quakeworld by forcing MOVETYPE_WALK on all players. Shouldn't be used for any games other than QuakeWorld.");
#endif

cvar_t	sv_chatfilter	= CVAR("sv_chatfilter", "0");

cvar_t	sv_floodprotect				= CVAR("sv_floodprotect", "1");
cvar_t	sv_floodprotect_messages	= CVAR("sv_floodprotect_messages", "4");
cvar_t	sv_floodprotect_interval	= CVAR("sv_floodprotect_interval", "4");
cvar_t  sv_floodprotect_silencetime	= CVAR("sv_floodprotect_silencetime", "10");
cvar_t	sv_floodprotect_suicide		= CVAR("sv_floodprotect_suicide", "1");
cvar_t	sv_floodprotect_sendmessage	= CVARAF("sv_floodprotect_sendmessage", "",
											 "floodprotmsg", 0);

cvar_t	votelevel	= CVARD("votelevel", "0", "This is the restriction level of commands that players may vote for. You can reconfigure commands, cvars, or aliases individually. Additionally, aliases can be configured via aliaslevel to be executed at a different level from their restriction level. This can be used to indirectly allow voting for 'map dm4' for instance, without allowing people to vote for every map.");
cvar_t	voteminimum	= CVARD("voteminimum", "4", "At least this many players must vote the same way for the vote to pass.");
cvar_t	votepercent	= CVARD("votepercent", "-1", "At least this percentage of players must vote the same way for the vote to pass.");
cvar_t	votetime	= CVARD("votetime", "10", "Votes will be discarded after this many minutes");

cvar_t	pr_allowbutton1 = CVARFD("pr_allowbutton1", "1", CVAR_LATCH, "The button1 field is believed to have been intended to work with the +use command, but it was never hooked up. In NetQuake, this field was often repurposed for other things as it was not otherwise used (and cannot be removed without breaking the crc), while third-party QuakeWorld engines did decide to implement it as believed was intended. As a result, this cvar only applies to QuakeWorld mods and a value of 1 is only likely to cause issues with NQ mods that were ported to QW.");
extern cvar_t sv_minping;


extern cvar_t	pm_bunnyspeedcap;
extern cvar_t	pm_ktjump;
extern cvar_t	pm_slidefix;
extern cvar_t	pm_slidyslopes;
extern cvar_t	pm_airstep;
extern cvar_t	pm_stepdown;
extern cvar_t	pm_walljump;
extern cvar_t	pm_watersinkspeed;
extern cvar_t	pm_flyfriction;
cvar_t sv_pushplayers = CVAR("sv_pushplayers", "0");

//yes, realip cvars need to be fully initialised or realip will be disabled
cvar_t sv_getrealip = CVARD("sv_getrealip", "0", "Attempt to obtain a more reliable IP for clients, rather than just their proxy.\n0: Don't attempt.\n1: Unreliable checks.\n2: Validate if possible.\n3: Mandatory validation.");
cvar_t sv_realip_kick = CVARD("sv_realip_kick", "0", "Kicks clients if their realip could not be validated to the level specified by sv_getrealip.");
cvar_t sv_realiphostname_ipv4 = CVARD("sv_realiphostname_ipv4", "", "This is the server's public ip:port. This is needed for realip to work when the autodetected/local ip is not globally routable");
cvar_t sv_realiphostname_ipv6 = CVARD("sv_realiphostname_ipv6", "", "This is the server's public ip:port. This is needed for realip to work when the autodetected/local ip is not globally routable");
cvar_t sv_realip_timeout = CVAR("sv_realip_timeout", "10");

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

extern cvar_t pausable;

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
			client->drop = true;
			return false;
		}
		if (!client->realip_status)
			client->realip_status = -1;
		return true;
	}


	if (client->realip_status == 1)
	{
		msg = va("\xff\xff\xff\xff%c %i", A2A_PING, client->realip_ping);
		NET_SendPacket(NS_SERVER, strlen(msg), msg, &client->realip);
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

	host_client->prespawn_stage = PRESPAWN_INVALID;
	host_client->prespawn_idx = 0;

	host_client->isindependant = false;

	if (host_client->state == cs_spawned)
		return;

	if (host_client->redirect)
	{
		if (host_client->redirect == 1)
		{
			char *msg = va("connect \"%s\"\n", sv_fullredirect.string);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(msg));
			ClientReliableWrite_String (host_client, msg);
		}
		return;
	}
	else
	{
		char *srv = Info_ValueForKey(host_client->userinfo, "*redirect");
		if (*srv)
		{
			char *msg = va("connect \"%s\"\n", srv);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(msg));
			ClientReliableWrite_String (host_client, msg);
			return;
		}
	}

	if (dpcompat_nopreparse.ival && progstype != PROG_QW)
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "This server now has network preparsing disabled, and thus only supports NetQuake clients\n");
		Con_Printf("%s was not using NQ protocols\n", host_client->name);
		host_client->drop = true;
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
	if (!gamedir[0])
	{
		if (ISQWCLIENT(host_client) || ISQ2CLIENT(host_client))
			gamedir = FS_GetGamedir(true);
		else
			gamedir = "";
	}

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
/*	if (host_client->num_backbuf)
	{
		Con_Printf("WARNING %s: [SV_New] Back buffered (%d0, clearing)\n", host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear(&host_client->netchan.message);
	}
*/
	if (svs.netprim.coordsize > 2 && !(host_client->fteprotocolextensions & PEXT_FLOATCOORDS))
	{
		SV_ClientPrintf(host_client, 2, "\n\n\n\nPlease set cl_nopext to 0 and then reconnect.\nIf that doesn't work, please update your engine - "ENGINEWEBSITE"\n");
		Con_Printf("%s does not support bigcoords\n", host_client->name);
		host_client->drop = true;
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
			ClientReliableWrite_Long (host_client, host_client->fteprotocolextensions|PEXT_FLOATCOORDS);
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

	if (host_client->fteprotocolextensions2 & PEXT2_MAXPLAYERS)
	{
		/*is this a sane way to do it? or should we split the spectator thing off entirely?*/
		ClientReliableWrite_Byte (host_client, sv.allocated_client_slots);
		splitnum = 0;
		for (split = host_client, splitnum = 0; split; split = split->controlled)
			splitnum++;
		ClientReliableWrite_Byte (host_client, (host_client->spectator?128:0) | splitnum); //read each player's userinfo to see if its a spectator or not. this hint is merely a cheat.
		for (split = host_client; split; split = split->controlled)
		{
			playernum = split - svs.clients;// NUM_FOR_EDICT(svprogfuncs, split->edict)-1;
			if (sv.state == ss_cinematic)
				playernum = -1;
			ClientReliableWrite_Byte (host_client, playernum);

			split->state = cs_connected;
			split->connection_started = realtime;
		#ifdef SVRANKING
			split->stats_started = realtime;
		#endif
			splitnum++;
		}
	}
	else
	{
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

			split->state = cs_connected;
			split->connection_started = realtime;
		#ifdef SVRANKING
			split->stats_started = realtime;
		#endif
			splitnum++;

			if (ISQ2CLIENT(host_client))
			{
				ClientReliableWrite_Short (host_client, playernum);
				break;
			}
			else
				ClientReliableWrite_Byte (host_client, playernum);
		}

		if (!ISQ2CLIENT(host_client))
		{
			if (host_client->fteprotocolextensions & PEXT_SPLITSCREEN)
				ClientReliableWrite_Byte (host_client, 128);
		}
	}

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

	host_client->csqcactive = false;

	host_client->realip_num = rand()+(host_client->challenge<<16);
	SV_CheckRealIP(host_client, false);

	SV_LogPlayer(host_client, "new (QW)");

	host_client->prespawn_stage = PRESPAWN_SERVERINFO;
	host_client->prespawn_idx = 0;
}

#ifdef NQPROT
void SVNQ_New_f (void)
{
	extern cvar_t coop;
	char	message[2048];
	char	build[256], mapname[128];
	int		i;
	int		op;
	unsigned int protext1 = 0, protext2 = 0, protmain = 0, protfl = 0;
	char *protoname;
	extern cvar_t sv_listen_nq;
	const char *gamedir;
	unsigned int modelcount, soundcount;
	extern cvar_t allow_download;

	host_client->prespawn_stage = PRESPAWN_INVALID;
	host_client->prespawn_idx = 0;

	host_client->isindependant = false;

	host_client->send_message = true;
	if (host_client->redirect)
	{
		if (host_client->redirect == 1)
		{
			char *msg = va("connect \"%s\"\n", sv_fullredirect.string);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(msg));
			ClientReliableWrite_String (host_client, msg);
		}
		return;
	}
	if (host_client->drop)
		return;

	if (!host_client->pextknown && sv_listen_nq.ival != 1)	//1 acts as a legacy mode, used for clients that can't cope with cmd before serverdata (either because they crash out or because they refuse to send reliables until after they got the first serverdata)
	{
		if (!host_client->supportedprotocols && host_client->netchan.remote_address.type != NA_LOOPBACK)
		{	//don't override cl_loopbackprotocol's choice
			char *msg = "cmd protocols\n";
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(msg));
			ClientReliableWrite_String (host_client, msg);
		}
		{
			char *msg = "cmd pext\n";
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(msg));
			ClientReliableWrite_String (host_client, msg);
		}
		return;
	}

	if (dpcompat_nopreparse.ival && progstype == PROG_QW)
	{
		SV_PrintToClient(host_client, PRINT_HIGH, "This server now has network preparsing disabled, and thus only supports QuakeWorld clients\n");
		Con_Printf("%s was not using QW protocols\n", host_client->name);
		host_client->drop = true;
		return;
	}

	host_client->csqcactive = false;

	protext1 = host_client->fteprotocolextensions;
	protext2 = host_client->fteprotocolextensions2;
	protmain = PROTOCOL_VERSION_NQ;
	protfl = 0;
	//force floatcoords as required.
	if (sv.nqdatagram.prim.coordsize >= 4)
		protext1 |= PEXT_FLOATCOORDS;
	else
		protext1 &= ~PEXT_FLOATCOORDS;

	op = host_client->protocol;
	if (host_client->supportedprotocols)
	{
		int i;
		static const struct
		{
			int id;
			qboolean big;	//used as a filter to exclude protocols that don't match our coord+angles mode
		} preferedprot[] =
		{
			{SCP_DARKPLACES7, true},
			{SCP_DARKPLACES6, true},
			{SCP_FITZ666, true},	//actually 999... shh...
			{SCP_FITZ666, false},
			{SCP_BJP3, false}	//should we only use this when we have >255 models/sounds?
		};
		for (i = 0; i < countof(preferedprot); i++)
		{
			if (preferedprot[i].big == !!(protext1 & PEXT_FLOATCOORDS))
			{
				if (host_client->supportedprotocols & (1u<<preferedprot[i].id))
				{
					host_client->protocol = preferedprot[i].id;
					break;
				}
			}
		}
	}

	switch(host_client->protocol)
	{
	case SCP_NETQUAKE:
	case SCP_BJP3:
	case SCP_FITZ666:
		SV_LogPlayer(host_client, "new (NQ)");
		if (host_client->protocol == SCP_FITZ666 ||
			sv.nqdatagram.prim.anglesize != 1 || sv.nqdatagram.prim.coordsize != 2)
		{
			protfl =
					((sv.nqdatagram.prim.coordsize==4)?RMQFL_FLOATCOORD:0) |
					((sv.nqdatagram.prim.anglesize==2)?RMQFL_SHORTANGLE:0);
			host_client->protocol = SCP_FITZ666; /*mneh, close enough, the rmq stuff is just modifiers*/

			if (protfl)
			{
				protext1 &= ~PEXT_FLOATCOORDS;		//never report floatcoords when using rmq protocol, as the base protocol allows us to be more specific anyway.
				protmain = PROTOCOL_VERSION_RMQ;
				protoname = "RMQ";
			}
			else
			{
				protmain = PROTOCOL_VERSION_FITZ;
				protoname = "666";
			}
		}
		else if (host_client->protocol == SCP_BJP3)
		{
			protmain = PROTOCOL_VERSION_BJP3;
			protoname = "BJP3";
		}
		else
		{
			if (!host_client->supportedprotocols && !host_client->fteprotocolextensions && !host_client->fteprotocolextensions2)
			{
				for (modelcount = 1; modelcount < MAX_PRECACHE_MODELS && sv.strings.model_precache[modelcount]; modelcount++)
					;
				for (soundcount = 1; soundcount < MAX_PRECACHE_SOUNDS && sv.strings.sound_precache[soundcount]; soundcount++)
					;
				if (modelcount > 255 || soundcount > 255 || sv.world.num_edicts > 600)
				{
					Q_snprintfz (message, sizeof(message), "\x02\n"
						"!!! THIS MAP REQUIRES AN IMPROVED PROTOCOL,\n"
						"!!! BUT YOUR CLIENT DOESN'T APPEAR TO SUPPORT ANY.\n"
						"!!! EXPECT MISSING MODELS, SOUNDS, OR ENTITIES\n");
						//if you're reading this message to try to avoid your client being described as shitty, implement support for 'cmd protocol' and maybe 'cmd pext' stuffcmds.
						//simply put, I can't use 666 if I don't know that its safe to do so.
					MSG_WriteByte (&host_client->netchan.message, svc_print);
					MSG_WriteString (&host_client->netchan.message,message);
				}
			}

			host_client->protocol = SCP_NETQUAKE;	//identical other than the client->server angles
			protmain = PROTOCOL_VERSION_NQ;
			protoname = "NQ";
		}
		break;
	case SCP_DARKPLACES6:
		SV_LogPlayer(host_client, "new (DP6)");
		protmain = PROTOCOL_VERSION_DP6;
		protext1 &= ~PEXT_FLOATCOORDS;	//always enabled, try not to break things
		protext2 = host_client->fteprotocolextensions2 = host_client->fteprotocolextensions2 & ~(PEXT2_PREDINFO|PEXT2_REPLACEMENTDELTAS);	//always disabled. kinda interferes with expectations.
		protoname = "DPP6";
		break;
	case SCP_DARKPLACES7:
		SV_LogPlayer(host_client, "new (DP7)");
		protmain = PROTOCOL_VERSION_DP7;
		protext1 &= ~PEXT_FLOATCOORDS;	//always enabled, try not to break things
		protext2 = host_client->fteprotocolextensions2 = host_client->fteprotocolextensions2 & ~(PEXT2_PREDINFO|PEXT2_REPLACEMENTDELTAS);	//always disabled. kinda interferes with expectations.
		protoname = "DPP7";
		break;
	default:
		host_client->drop = true;
		protoname = "Unknown";
		break;
	}

#ifdef OFFICIAL_RELEASE
	Q_snprintfz(build, sizeof(build), "v%i.%02i", FTE_VER_MAJOR, FTE_VER_MINOR);
#else
#if defined(SVNREVISION)
	if (strcmp(STRINGIFY(SVNREVISION), "-"))
		Q_snprintfz(build, sizeof(build), "SVN %s", STRINGIFY(SVNREVISION));
	else
#endif
		Q_snprintfz(build, sizeof(build), "%s", __DATE__);
#endif

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
	{
		gamedir = FS_GetGamedir(true);
#ifndef NOLEGACY
		if (!strcmp(gamedir, "qw"))	//hack: hide the qw dir from nq clients.
			gamedir = "";
#endif
	}
	COM_FileBase(sv.modelname, mapname, sizeof(mapname));

	
	if (op != host_client->protocol)
		SV_ClientProtocolExtensionsChanged(host_client);

//	if (host_client->netchan.remote_address.type != NA_LOOPBACK) //don't display this to localhost, because its just spam at the end of the day. you don't want to see it in single player.
	{
		//note that certain clients glitch out if this does not have a trailing new line.
		//note that those clients will also glitch out from vanilla servers too.
		//vanilla prints something like: VERSION 1.08 SERVER (%i CRC)
		//which isn't all that useful. so lets customise it to advertise properly, as well as provide gamedir and map (file)name info
		if (protext2 & PEXT2_REPLACEMENTDELTAS)
		{
			Q_snprintfz (message, sizeof(message), "%c\n%s - "DISTRIBUTION" (FTENQ, %s) - %s", 2, gamedir,
				build,	mapname);
		}
		else
		{
			Q_snprintfz (message, sizeof(message), "%c\n%s - "DISTRIBUTION" (%s%s%s, %s) - %s", 2, gamedir,
				protoname,(protext1||(protext2&~PEXT2_VOICECHAT))?"+":"",(protext2&PEXT2_VOICECHAT)?"Voip":"",
				build,	mapname);
		}
		MSG_WriteByte (&host_client->netchan.message, svc_print);
		MSG_WriteString (&host_client->netchan.message,message);
	}

	if (host_client->protocol == SCP_DARKPLACES6 || host_client->protocol == SCP_DARKPLACES7)
	{
		size_t sz;
		char *f;

		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message, "cl_serverextension_download 1\n");

		f = COM_LoadTempFile("csprogs.dat", &sz);
		if (f)
		{
			MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
			MSG_WriteString (&host_client->netchan.message, va("csqc_progname %s\n", "csprogs.dat"));
			MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
			MSG_WriteString (&host_client->netchan.message, va("csqc_progsize %u\n", (unsigned int)sz));
			MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
			MSG_WriteString (&host_client->netchan.message, va("csqc_progcrc %i\n", QCRC_Block(f, sz)));

			MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
			MSG_WriteString (&host_client->netchan.message, "cmd enablecsqc\n");
		}
	}
	else if (allow_download.value && (protext1||protext2))
	{	//technically this is a DP extension, but is separate from actual protocols and shouldn't harm anything.
		//it is annoying to have prints about unknown commands however, hence the above pext checks (which are unfortunate).
		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message, "cl_serverextension_download 1\n");
	}

	MSG_WriteByte (&host_client->netchan.message, svc_serverdata);
	if (protext1)
	{
		MSG_WriteLong (&host_client->netchan.message, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (&host_client->netchan.message, protext1);
	}
	if (protext2)
	{
		MSG_WriteLong (&host_client->netchan.message, PROTOCOL_VERSION_FTE2);
		MSG_WriteLong (&host_client->netchan.message, protext2);
	}
	MSG_WriteLong (&host_client->netchan.message, protmain);
	if (protmain == PROTOCOL_VERSION_RMQ)
		MSG_WriteLong (&host_client->netchan.message, protfl);

	if (protext2 & PEXT2_PREDINFO)
		MSG_WriteString(&host_client->netchan.message, gamedir);

	MSG_WriteByte (&host_client->netchan.message, (sv.allocated_client_slots>host_client->max_net_clients)?host_client->max_net_clients:sv.allocated_client_slots);

	if (!coop.value && deathmatch.value)
		MSG_WriteByte (&host_client->netchan.message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&host_client->netchan.message, GAME_COOP);

	MSG_WriteString (&host_client->netchan.message, sv.mapname);


	//fixme: don't send too many models.
	for (i = 1; sv.strings.model_precache[i] ; i++)
		MSG_WriteString (&host_client->netchan.message, sv.strings.model_precache[i]);
	MSG_WriteByte (&host_client->netchan.message, 0);

	//fixme: don't send too many sounds.
	for (i = 1; sv.strings.sound_precache[i] ; i++)
		MSG_WriteString (&host_client->netchan.message, sv.strings.sound_precache[i]);
	MSG_WriteByte (&host_client->netchan.message, 0);

// set view
	MSG_WriteByte (&host_client->netchan.message, svc_setview);
	MSG_WriteEntity (&host_client->netchan.message, (host_client - svs.clients)+1);//NUM_FOR_EDICT(svprogfuncs, host_client->edict));

	if (!(host_client->fteprotocolextensions2 & PEXT2_PREDINFO))
	{	//old clients can't cope with reliables until they finish loading the models specified above.
		//we need to wait before sending any more
		//updated clients can wait a bit, and use signonnum 1 to tell them when to start loading stuff.
		MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
		MSG_WriteByte (&host_client->netchan.message, 1);
		host_client->netchan.nqunreliableonly = 2;
	}

//	host_client->sendsignon = true;
//	host_client->spawned = false;		// need prespawn, spawn, etc

	if (op != host_client->protocol)
		host_client->prespawn_stage = PRESPAWN_PROTOCOLSWITCH;
	else
		host_client->prespawn_stage = PRESPAWN_SERVERINFO;
	host_client->prespawn_idx = 0;
}
#endif




#ifdef Q2SERVER
void SVQ2_ConfigStrings_f (void)
{
	unsigned int			start;
	const char *str;

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

	while ( host_client->netchan.message.cursize < host_client->netchan.message.maxsize/2
		&& start < Q2MAX_CONFIGSTRINGS)
	{
		str = sv.strings.configstring[start];
		if (str && *str)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, str);
		}
		start++;
	}

	//model overflows
	if (start == Q2MAX_CONFIGSTRINGS)
		start = 0x8000;
	while ( host_client->netchan.message.cursize < host_client->netchan.message.maxsize/2
		&& start < 0x8000+MAX_PRECACHE_MODELS)
	{
		str = sv.strings.q2_extramodels[start-0x8000];
		if (str && *str)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, str);
		}
		start++;
	}

	//sound overflows
	if (start == 0x8000+MAX_PRECACHE_MODELS)
		start = 0xc000;
	while ( host_client->netchan.message.cursize < host_client->netchan.message.maxsize/2
		&& start < 0xc000+MAX_PRECACHE_SOUNDS)
	{
		str = sv.strings.q2_extrasounds[start-0xc000];
		if (str && *str)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, str);
		}
		start++;
	}

	// send next command

	if (start == 0xc000+MAX_PRECACHE_SOUNDS)
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

	while ( host_client->netchan.message.cursize <  host_client->netchan.message.maxsize/2
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

/*
==================
SV_Soundlist_f
==================
*/
void SVQW_Soundlist_f (void)
{
	host_client->prespawn_allow_soundlist = true;
}

/*
==================
SV_Modellist_f
==================
*/
void SVQW_Modellist_f (void)
{
	host_client->prespawn_allow_modellist = true;
}

void SV_SendClientPrespawnInfo(client_t *client)
{
	qboolean started;
	int i;
	entity_state_t *state;
	staticsound_state_t *sound;
	edict_t *ent;
	svcustomtents_t *ctent;
	int maxsize;
	//much of this function is written to fill packets enough to overflow them (assuming max packet sizes are large enough), but some bits are lazy and just backbuffer as needed.
	//FIXME: have per-stage indicies, to allow returning to a previous stage when new precaches or whatever get added

	if (client->num_backbuf || client->prespawn_stage == PRESPAWN_COMPLETED)
	{
		//don't spam too much.
		return;
	}

	//just because we CAN generate huge messages doesn't meant that we should.
	//try to keep packets within reasonable sizes so that we don't trigger insane burst+packetloss on map changes.
	maxsize = client->netchan.message.maxsize/2;
	if (client->netchan.fragmentsize && maxsize > client->netchan.fragmentsize-200)
	{
		maxsize = client->netchan.fragmentsize-200;
		if (maxsize < 500)
			maxsize = 500;
	}

	if (client->prespawn_stage == PRESPAWN_PROTOCOLSWITCH)
	{
		if (client->prespawn_idx++ > 10)
		{
			client->prespawn_idx = 0;
			client->prespawn_stage++;
		}
		else
			return;
	}

	if (client->prespawn_stage == PRESPAWN_SERVERINFO)
	{
		char buffer[1024];

		while (client->netchan.message.cursize < maxsize)
		{
			if (client->prespawn_idx == 0)
			{
				if (!ISNQCLIENT(client) || (client->fteprotocolextensions2 & PEXT2_PREDINFO))
				{	//nq does not normally get serverinfo sent to it.
					ClientReliableWrite_Begin(client, svc_stufftext, 20 + strlen(svs.info));
					ClientReliableWrite_String (client, va("fullserverinfo \"%s\"\n", svs.info) );
				}
				else if (sv.csqcdebug)
				{
					ClientReliableWrite_Begin(client, svc_stufftext, 22 + strlen(svs.info));
					ClientReliableWrite_String (client, va("//fullserverinfo \"%s\"\n", svs.info) );
				}
			}
			else if (client->prespawn_idx == 1)
			{
				FS_GetPackNames(buffer, sizeof(buffer), 2, true); /*retain extensions, or we'd have to assume pk3*/
				ClientReliableWrite_Begin(client, svc_stufftext, 1+11+strlen(buffer)+1+1);
				ClientReliableWrite_SZ(client, "//paknames ", 11);
				ClientReliableWrite_SZ(client, buffer, strlen(buffer));
				ClientReliableWrite_String(client, "\n");
			}
			else if (client->prespawn_idx == 2)
			{
				FS_GetPackHashes(buffer, sizeof(buffer), false);
				ClientReliableWrite_Begin(client, svc_stufftext, 1+7+strlen(buffer)+1+1);
				ClientReliableWrite_SZ(client, "//paks ", 7);
				ClientReliableWrite_SZ(client, buffer, strlen(buffer));
				ClientReliableWrite_String(client, "\n");
			}
			else if (client->prespawn_idx == 3)
			{
				if (ISNQCLIENT(client) && (client->fteprotocolextensions2 & PEXT2_PREDINFO))
				{
					ClientReliableWrite_Begin(client, svc_signonnum, 2);
					ClientReliableWrite_Byte (client, 1);
				}
			}
			else if (client->prespawn_idx == 4)
			{
				int track = 0;

				if (progstype == PROG_H2)
					track = sv.h2cdtrack;	//hexen2 has a special hack
				else if (svprogfuncs)
					track = ((edict_t*)sv.world.edicts)->v->sounds;

				ClientReliableWrite_Begin(client, svc_cdtrack, 2);
				ClientReliableWrite_Byte (client, track);
				if (ISNQCLIENT(client))
					ClientReliableWrite_Byte (client, track);

				if (!track && *sv.h2miditrack)
					SV_StuffcmdToClient(client, va("music \"%s\"\n", sv.h2miditrack));
			}
			else if (client->prespawn_idx == 5)
			{
				ClientReliableWrite_Begin(client, svc_setpause, 2);
				ClientReliableWrite_Byte (client, sv.paused!=0);
			}
			else
			{
				client->prespawn_stage++;
				client->prespawn_idx = 0;
				client->prespawn_idx2 = 0;
				break;
			}
			client->prespawn_idx++;
		}
	}

	if (client->prespawn_stage == PRESPAWN_CSPROGS)
	{
		extern cvar_t sv_demo_write_csqc;
		if (client == &demo.recorder && sv_demo_write_csqc.ival)	//we only really want to do this for demos. actual clients can make the request themselves.
		if (client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)	//there's many different download mechanisms...
		{
			if (!client->prespawn_idx && !client->download)
			{
				extern cvar_t sv_csqc_progname;
				Q_strncpyz(client->downloadfn, sv_csqc_progname.string, sizeof(client->downloadfn));
				client->download = FS_OpenVFS(sv_csqc_progname.string, "rb", FS_GAME);
				client->downloadcount = 0;
				client->prespawn_idx = 1;
				if (client->download)
				{
					client->downloadsize = VFS_GETLEN(client->download);

					//send the size+filename
					ClientReliableWrite_Begin (client, svc_download, 18+strlen(client->downloadfn));
					ClientReliableWrite_Long (client, -1);	//offset
					if (client->downloadsize >= 0x7fffffff)
					{	//avoid unsigned values.
						ClientReliableWrite_Long (client, 0x80000000);	//signal that its 64bit
						ClientReliableWrite_Long (client, qofs_Low(client->downloadsize));
						ClientReliableWrite_Long (client, qofs_High(client->downloadsize));
					}
					else
						ClientReliableWrite_Long (client, client->downloadsize);
					ClientReliableWrite_String (client, client->downloadfn);
				}
			}
			//send the data while possible+needed
			if (client->prespawn_idx && client->download)
			{
				while (client->downloadcount < client->downloadsize)
				{
					qbyte chunk[DLBLOCKSIZE];
					int sz;
					if (client->netchan.message.maxsize - client->netchan.message.cursize < 1100)
						return;	//don't flood...
					sz = VFS_READ(client->download, chunk, DLBLOCKSIZE);
					if (sz <= 0)
						break;
					if (sz < DLBLOCKSIZE)
					{
						memset(chunk+sz, 0, DLBLOCKSIZE-sz);	//zero-fill if the chunk is at the end.
						sz = DLBLOCKSIZE;
					}

					ClientReliableWrite_Begin (client, svc_download, 5+sz);
					ClientReliableWrite_Long(client, client->downloadcount/DLBLOCKSIZE);
					ClientReliableWrite_SZ(client, chunk, sz);
					client->downloadcount += sz;
				}

				//don't need to write completion. the client should be tracking that itself with chunks.
				VFS_CLOSE(client->download);
				client->download = NULL;
			}
		}

		client->prespawn_stage++;
		client->prespawn_idx = 0;
	}

	if (client->prespawn_stage == PRESPAWN_SOUNDLIST)
	{
		if (!ISQWCLIENT(client))
			client->prespawn_stage++;	//nq sends sound lists as part of the svc_serverdata
		else
		{
			int maxclientsupportedsounds = 256;
#ifdef PEXT_SOUNDDBL
			if (client->fteprotocolextensions & PEXT_SOUNDDBL)
				maxclientsupportedsounds = MAX_PRECACHE_SOUNDS;
#endif
#ifdef PEXT_SOUNDDBL
			if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
				maxclientsupportedsounds = MAX_PRECACHE_SOUNDS;
#endif
			started = false;

			//allows stalling for the soundlist command, for compat.
			if (!client->prespawn_allow_soundlist)
				if (!(client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
					return;

			while (client->netchan.message.cursize < maxsize)
			{
				if (!started)
				{
					started = true;
					client->prespawn_allow_soundlist = false;
	#if defined(PEXT_SOUNDDBL) || defined(PEXT2_REPLACEMENTDELTAS)
					if (client->prespawn_idx > 255)
					{
						MSG_WriteByte (&client->netchan.message, svcfte_soundlistshort);
						MSG_WriteShort (&client->netchan.message, client->prespawn_idx);
					}
					else
	#endif
					{
						MSG_WriteByte (&client->netchan.message, svc_soundlist);
						MSG_WriteByte (&client->netchan.message, client->prespawn_idx);
					}
				}
				client->prespawn_idx++;

				if (client->prespawn_idx >= maxclientsupportedsounds || !sv.strings.sound_precache[client->prespawn_idx])
				{
					//write final-end-of-list
					MSG_WriteByte (&client->netchan.message, 0);
					MSG_WriteByte (&client->netchan.message, 0);
					started = 0;

					if (sv.strings.sound_precache[client->prespawn_idx] && !(client->plimitwarned & PLIMIT_SOUNDS))
					{
						client->plimitwarned |= PLIMIT_SOUNDS;
						SV_ClientPrintf(client, PRINT_HIGH, "WARNING: Your client's network protocol only supports %i sounds. Please upgrade or enable extensions.\n", client->prespawn_idx);
					}

					client->prespawn_stage++;
					client->prespawn_idx = 0;
					break;
				}
				else
					MSG_WriteString (&client->netchan.message, sv.strings.sound_precache[client->prespawn_idx]);
			}
			if (started)
			{
				//write end-of-packet
				MSG_WriteByte (&client->netchan.message, 0);
				MSG_WriteByte (&client->netchan.message, (client->prespawn_idx&0xff)?client->prespawn_idx&0xff:0xff);

			}
		}
	}

	if (client->prespawn_stage == PRESPAWN_VWEPMODELLIST)
	{
		//no indicies. the protocol can't cope with them.
		if (client->zquake_extensions & Z_EXT_VWEP)
		{
			char mname[MAX_QPATH];
			char ext[8];
			char vweaplist[2048] = "//vwep";

			for (i = 0; sv.strings.vw_model_precache[i]; i++)
			{
				//grab the model name... without a progs/ prefix if it has one
				if (!strncmp(sv.strings.vw_model_precache[i], "progs/", 6))
					Q_strncpy(mname, sv.strings.vw_model_precache[i]+6, sizeof(mname));
				else
					Q_strncpy(mname, sv.strings.vw_model_precache[i], sizeof(mname));

				//strip .mdl extensions, for compat with ezquake
				COM_FileExtension(mname, ext, sizeof(ext));
				if (!strcmp(ext, "mdl"))
					COM_StripExtension(mname, mname, sizeof(mname));

				//add it to the vweap command, taking care of any remaining spaces in names.
				if (strchr(mname, ' ') || !*mname)
					Q_strncatz(vweaplist, va(" \"%s\"", mname), sizeof(vweaplist));
				else
					Q_strncatz(vweaplist, va(" %s", mname), sizeof(vweaplist));
			}

			if (strlen(vweaplist) <= sizeof(vweaplist)-2)
			{
				Q_strncatz(vweaplist, "\n", sizeof(vweaplist));

				ClientReliableWrite_Begin(client, svc_stufftext, 2+strlen(vweaplist));
				ClientReliableWrite_String(client, vweaplist);
			}
		}
		client->prespawn_stage++;
	}

	if (client->prespawn_stage == PRESPAWN_MODELLIST)
	{
		if (!ISQWCLIENT(client))
			client->prespawn_stage++;
		else
		{
			started = false;

			//allows stalling for the modellist command, for compat.
			if (!client->prespawn_allow_modellist)
				if (!(client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
					return;

			while (client->netchan.message.cursize < maxsize)
			{
				if (!started)
				{
					started = true;
					client->prespawn_allow_modellist = false;
	#if defined(PEXT_SOUNDDBL) || defined(PEXT2_REPLACEMENTDELTAS)
					if (client->prespawn_idx > 255)
					{
						MSG_WriteByte (&client->netchan.message, svcfte_modellistshort);
						MSG_WriteShort (&client->netchan.message, client->prespawn_idx);
					}
					else
	#endif
					{
						MSG_WriteByte (&client->netchan.message, svc_modellist);
						MSG_WriteByte (&client->netchan.message, client->prespawn_idx);
					}
				}
				client->prespawn_idx++;

				if (client->prespawn_idx >= client->maxmodels || !sv.strings.model_precache[client->prespawn_idx])
				{
					//write final-end-of-list
					MSG_WriteByte (&client->netchan.message, 0);
					MSG_WriteByte (&client->netchan.message, 0);
					started = 0;

					if (sv.strings.model_precache[client->prespawn_idx] && !(client->plimitwarned & PLIMIT_MODELS))
					{
						client->plimitwarned |= PLIMIT_MODELS;
						SV_ClientPrintf(client, PRINT_HIGH, "WARNING: Your client's network protocol only supports %i models. Please upgrade or enable extensions.\n", client->prespawn_idx);
					}

					client->prespawn_stage++;
					client->prespawn_idx = 0;
					break;
				}
				else
					MSG_WriteString (&client->netchan.message, sv.strings.model_precache[client->prespawn_idx]);
			}
			if (started)
			{
				//write end-of-packet
				MSG_WriteByte (&client->netchan.message, 0);
				MSG_WriteByte (&client->netchan.message, (client->prespawn_idx&0xff)?client->prespawn_idx&0xff:0xff);
			}
		}
	}

	if (client->prespawn_stage == PRESPAWN_MAPCHECK)
	{
		//can't progress beyond this as we're waiting for the client.
//		host_client->prespawn_idx = client->prespawn_idx;
		return;
	}

	if (client->prespawn_stage == PRESPAWN_PARTICLES)
	{
		if (!(client->fteprotocolextensions & PEXT_CSQC))
			client->prespawn_idx = MAX_SSPARTICLESPRE;
		while (client->netchan.message.cursize < maxsize)
		{
			if (client->prespawn_idx >= MAX_SSPARTICLESPRE)
			{
				client->prespawn_stage++;
				client->prespawn_idx = 0;
				break;
			}

			if (sv.strings.particle_precache[client->prespawn_idx])
			{
				ClientReliableWrite_Begin (client, ISNQCLIENT(client)?svcdp_precache:svcfte_precache, 4 + strlen(sv.strings.particle_precache[client->prespawn_idx]));
				ClientReliableWrite_Short (client, client->prespawn_idx | PC_PARTICLE);
				ClientReliableWrite_String (client, sv.strings.particle_precache[client->prespawn_idx]);
			}
			client->prespawn_idx++;
		}
	}

	if (client->prespawn_stage == PRESPAWN_CUSTOMTENTS)
	{
		while (client->netchan.message.cursize < maxsize)
		{
			if (client->prespawn_idx >= 255)
			{
				client->prespawn_stage++;
				client->prespawn_idx = 0;
				break;
			}

			ctent = &sv.customtents[client->prespawn_idx];

			if (*ctent->particleeffecttype)
			{
				if (client->fteprotocolextensions & PEXT_CUSTOMTEMPEFFECTS)
				{
					MSG_WriteByte(&client->netchan.message, svcfte_customtempent);
					MSG_WriteByte(&client->netchan.message, 255);
					MSG_WriteByte(&client->netchan.message, client->prespawn_idx);
					MSG_WriteByte(&client->netchan.message, ctent->netstyle);
					MSG_WriteString(&client->netchan.message, ctent->particleeffecttype);
					if (ctent->netstyle & CTE_STAINS)
					{
						MSG_WriteChar(&client->netchan.message, ctent->stain[0]);
						MSG_WriteChar(&client->netchan.message, ctent->stain[0]);
						MSG_WriteChar(&client->netchan.message, ctent->stain[0]);
						MSG_WriteByte(&client->netchan.message, ctent->radius);
					}
					if (ctent->netstyle & CTE_GLOWS)
					{
						MSG_WriteByte(&client->netchan.message, ctent->dlightrgb[0]);
						MSG_WriteByte(&client->netchan.message, ctent->dlightrgb[1]);
						MSG_WriteByte(&client->netchan.message, ctent->dlightrgb[2]);
						MSG_WriteByte(&client->netchan.message, ctent->dlightradius);
						MSG_WriteByte(&client->netchan.message, ctent->dlighttime);
					}
				}
			}
			client->prespawn_idx++;
		}
	}

	if (client->prespawn_stage == PRESPAWN_SIGNON_BUF)
	{
		while (client->netchan.message.cursize < maxsize)
		{
			if (client->prespawn_idx >= sv.num_signon_buffers)
			{
				client->prespawn_stage++;
				client->prespawn_idx = 0;
				break;
			}

			if (client->netchan.message.cursize+sv.signon_buffer_size[client->prespawn_idx]+30 < client->netchan.message.maxsize)
			{
				SZ_Write (&client->netchan.message,
					sv.signon_buffers[client->prespawn_idx],
					sv.signon_buffer_size[client->prespawn_idx]);
			}
			else
				break;
			client->prespawn_idx++;
		}
	}

	if (client->prespawn_stage == PRESPAWN_SPAWNSTATIC)
	{
		int maxstatics = sv.num_static_entities;
		if (maxstatics > 1024 && ISDPCLIENT(client))
			maxstatics = 1024;
		while (client->netchan.message.cursize < maxsize)	//static entities
		{
			if (client->prespawn_idx >= maxstatics)
			{
				client->prespawn_stage++;
				client->prespawn_idx = 0;
				break;
			}

			state = &sv_staticentities[client->prespawn_idx];
			client->prespawn_idx++;

			if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
			{
				MSG_WriteByte(&client->netchan.message, svcfte_spawnstatic2);
				SVFTE_EmitBaseline(state, false, &client->netchan.message, client->fteprotocolextensions2);
				continue;
			}
			if (client->fteprotocolextensions & PEXT_SPAWNSTATIC2)
			{
				/*if it uses some new feature, use the updated spawnstatic*/
				if (state->hexen2flags || state->trans || state->modelindex >= 256 || state->frame > 255 || state->scale || state->abslight)
				{
					MSG_WriteByte(&client->netchan.message, svcfte_spawnstatic2);
					SVQW_WriteDelta(&nullentitystate, state, &client->netchan.message, true, client->fteprotocolextensions);
					continue;
				}
			}
			if (client->protocol == SCP_BJP3)
			{
				MSG_WriteByte(&client->netchan.message, svc_spawnstatic);

				MSG_WriteShort (&client->netchan.message, state->modelindex);

				MSG_WriteByte (&client->netchan.message, state->frame);
				MSG_WriteByte (&client->netchan.message, (int)state->colormap);
				MSG_WriteByte (&client->netchan.message, (int)state->skinnum);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&client->netchan.message, state->origin[i]);
					MSG_WriteAngle(&client->netchan.message, state->angles[i]);
				}
				continue;
			}
			/*couldn't use protocol extensions?
			  use the fallback, unless the model is invalid as that's silly*/
			if (state->modelindex < 256)
			{
				MSG_WriteByte(&client->netchan.message, svc_spawnstatic);

				MSG_WriteByte (&client->netchan.message, state->modelindex);

				MSG_WriteByte (&client->netchan.message, state->frame);
				MSG_WriteByte (&client->netchan.message, (int)state->colormap);
				MSG_WriteByte (&client->netchan.message, (int)state->skinnum);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&client->netchan.message, state->origin[i]);
					MSG_WriteAngle(&client->netchan.message, state->angles[i]);
				}
				continue;
			}
		}
	}
	if (client->prespawn_stage == PRESPAWN_AMBIENTSOUND)
	{
		while (client->netchan.message.cursize < maxsize)	//static entities
		{
			qboolean large = false;

			if (client->prespawn_idx >= sv.num_static_sounds)
			{
				client->prespawn_stage++;
				client->prespawn_idx = 0;
				client->prespawn_idx2 = 0;
				break;
			}

			sound = &sv_staticsounds[client->prespawn_idx];
			client->prespawn_idx++;

			/*if (client->protocol == SCP_BJP2)
			{
				large = true;
				MSG_WriteByte(&client->netchan.message, svc_spawnstaticsound);
			}
			else */if (sound->soundnum > 0xff)
			{
				large = true;
				if (client->protocol == SCP_BJP3)
					continue;	//not supported
				else if (ISDPCLIENT(client))
					MSG_WriteByte(&client->netchan.message, svcdp_spawnstaticsound2);
				else if (ISNQCLIENT(client))
					MSG_WriteByte(&client->netchan.message, svcfitz_spawnstaticsound2);
				else
					continue; //not supported
			}
			else
				MSG_WriteByte(&client->netchan.message, svc_spawnstaticsound);

			for (i=0 ; i<3 ; i++)
				MSG_WriteCoord(&client->netchan.message, sound->position[i]);
			if (large)
				MSG_WriteShort(&client->netchan.message, sound->soundnum);
			else
				MSG_WriteByte(&client->netchan.message, sound->soundnum);
			MSG_WriteByte(&client->netchan.message, sound->volume);
			MSG_WriteByte(&client->netchan.message, sound->attenuation);
		}
	}

	if (client->prespawn_stage == PRESPAWN_BASELINES)
	{
		while (client->netchan.message.cursize < maxsize)	//baselines
		{
			if (client->prespawn_idx >= sv.world.num_edicts)
			{
				client->prespawn_stage++;
				client->prespawn_idx = 0;
				break;
			}

			ent = EDICT_NUM(svprogfuncs, client->prespawn_idx);

			if (!ent)
				state = &nullentitystate;
			else
				state = &ent->baseline;

			if (!state->number || !state->modelindex)
			{	//ent doesn't have a baseline
				client->prespawn_idx++;
				continue;
			}

			if (state->number >= client->max_net_ents || state->modelindex >= client->maxmodels)
			{
				/*can't send this ent*/
			}
			else if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
			{
				MSG_WriteByte(&client->netchan.message, svcfte_spawnbaseline2);
				SVFTE_EmitBaseline(state, true, &client->netchan.message, client->fteprotocolextensions2);
			}
			else if (client->fteprotocolextensions & PEXT_SPAWNSTATIC2)
			{
				MSG_WriteByte(&client->netchan.message, svcfte_spawnbaseline2);
				SVQW_WriteDelta(&nullentitystate, state, &client->netchan.message, true, client->fteprotocolextensions);
			}
			else if (ISDPCLIENT(client) && (state->modelindex > 255 || state->frame > 255))
			{
				MSG_WriteByte(&client->netchan.message, svcdp_spawnbaseline2);
				MSG_WriteEntity (&client->netchan.message, state->number);

				MSG_WriteShort (&client->netchan.message, state->modelindex);
				MSG_WriteShort (&client->netchan.message, state->frame);
				MSG_WriteByte (&client->netchan.message, (int)state->colormap);
				MSG_WriteByte (&client->netchan.message, (int)state->skinnum);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&client->netchan.message, state->origin[i]);
					MSG_WriteAngle(&client->netchan.message, state->angles[i]);
				}
			}
			else if (client->protocol == SCP_FITZ666 && (state->modelindex > 255 || state->frame > 255 || state->trans != 255 || state->scale != 16))
			{
				int fl = 0;
				if (state->modelindex > 255)	fl |= FITZ_B_LARGEMODEL;
				if (state->frame > 255)			fl |= FITZ_B_LARGEFRAME;
				if (state->trans != 255)		fl |= FITZ_B_ALPHA;
				if (state->scale != 16)			fl |= RMQFITZ_B_SCALE;

				MSG_WriteByte(&client->netchan.message, svcfitz_spawnbaseline2);
				MSG_WriteEntity (&client->netchan.message, state->number);
				MSG_WriteByte(&client->netchan.message, fl);
				if (fl & FITZ_B_LARGEMODEL)
					MSG_WriteShort (&client->netchan.message, state->modelindex);
				else
					MSG_WriteByte (&client->netchan.message, state->modelindex);
				if (fl & FITZ_B_LARGEFRAME)
					MSG_WriteShort (&client->netchan.message, state->frame);
				else
					MSG_WriteByte (&client->netchan.message, state->frame);
				MSG_WriteByte (&client->netchan.message, state->colormap);
				MSG_WriteByte (&client->netchan.message, state->skinnum);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&client->netchan.message, state->origin[i]);
					MSG_WriteAngle(&client->netchan.message, state->angles[i]);
				}
				if (fl & FITZ_B_ALPHA)
					MSG_WriteByte (&client->netchan.message, state->trans);
				if (fl & RMQFITZ_B_SCALE)
					MSG_WriteByte (&client->netchan.message, state->scale);
			}
			else if (state->modelindex)
			{
				MSG_WriteByte(&client->netchan.message, svc_spawnbaseline);
				MSG_WriteEntity (&client->netchan.message, state->number);

				if (client->protocol == SCP_BJP3)
					MSG_WriteShort (&client->netchan.message, state->modelindex&0xffff);
				else if (state->modelindex > 255)
					MSG_WriteByte (&client->netchan.message, 0);	//invalid modelindex. at least try to give something
				else
					MSG_WriteByte (&client->netchan.message, state->modelindex&0xff);
				MSG_WriteByte (&client->netchan.message, state->frame & 0xff);
				MSG_WriteByte (&client->netchan.message, state->colormap & 0xff);
				MSG_WriteByte (&client->netchan.message, state->skinnum & 0xff);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&client->netchan.message, state->origin[i]);
					MSG_WriteAngle(&client->netchan.message, state->angles[i]);
				}
			}

			client->prespawn_idx++;
		}
	}
	if (client->prespawn_stage == PRESPAWN_SPAWN)
	{
		//we'll spawn the client and then send all the updating stuff only when we know the channel is clear, by pinging the client for it.
		if (ISNQCLIENT(client))
		{
			//effectively a cmd spawn... but also causes the client to actually send the player's name too.
			ClientReliableWrite_Begin (client, svc_signonnum, 2);
			ClientReliableWrite_Byte (client, 2);
		}
		else
		{
			char *cmd = va("cmd spawn %i\n",svs.spawncount);
			ClientReliableWrite_Begin(client, svc_stufftext, 2+strlen(cmd));
			ClientReliableWrite_String(client, cmd);
		}
		client->prespawn_stage++;
		client->prespawn_idx = 0;
		client->prespawn_idx2 = 0;
	}

	//this is extra stuff that will happen after we're on the server

	if (client->prespawn_stage == PRESPAWN_BRUSHES)
	{	//when brush editing, connecting clients need a copy of all the brushes.
		while (client->netchan.message.cursize < maxsize)
		{
#ifdef TERRAIN
			if (!SV_Prespawn_Brushes(&client->netchan.message, &client->prespawn_idx, &client->prespawn_idx2))
#endif
			{
				client->prespawn_stage++;
				client->prespawn_idx = 0;
				client->prespawn_idx2 = 0;
				break;
			}
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
	unsigned	check;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}
	if (host_client->prespawn_stage != PRESPAWN_MAPCHECK)
	{
		Con_Printf("Wrong stage for prespawn command\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount && !sv.msgfromdemo)
	{
		Con_Printf ("SV_PreSpawn_f from different level\n");
		//FIXME: we shouldn't need the following line.
		SV_New_f ();
		return;
	}

	if (host_client->prespawn_stage == PRESPAWN_MAPCHECK)
	{
		check = atoi(Cmd_Argv(3));

//		Con_DPrintf("Client check = %d\n", check);

		if (sv_mapcheck.value && check != sv.world.worldmodel->checksum &&
			COM_RemapMapChecksum(sv.world.worldmodel, check) != COM_RemapMapChecksum(sv.world.worldmodel, LittleLong(sv.world.worldmodel->checksum2)))
#ifdef SERVER_DEMO_PLAYBACK
		if (!sv.demofile || (sv.demofile && !sv.democausesreconnect))	//demo playing causes no check. If it's the return level, check anyway to avoid that loophole.
#endif
		{
			char *msg;
			SV_ClientTPrintf (host_client, PRINT_HIGH,
				"Map model file does not match (%s), %i != %i/%i.\nYou may need a new version of the map, or the proper install files.\n",
				sv.modelname, check, sv.world.worldmodel->checksum, sv.world.worldmodel->checksum2);


			msg = va("\n//kickfile \"%s\"\n", sv.modelname);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 3+strlen(msg));
			ClientReliableWrite_String (host_client, msg);
			SV_DropClient (host_client);
			return;
		}
		host_client->checksum = check;
		host_client->prespawn_stage = PRESPAWN_MAPCHECK+1;
		host_client->prespawn_idx = 0;
		return;
	}
}

/*
==================
SV_Spawn_f
==================
*/
void SVQW_Spawn_f (void)
{
	int		i;
	client_t	*client, *split;
	edict_t	*ent;

#ifdef QUAKESTATS
	int secret_total, secret_found, monsters_total, monsters_found;
#endif

	if (host_client->state != cs_connected)
	{
		Con_Printf ("Spawn not valid -- already spawned\n");
		return;
	}
	if (host_client->prespawn_stage <= PRESPAWN_SPAWN)
	{
		Con_Printf ("%s sent spawn without prespawn!\n", host_client->name);
		SV_New_f ();
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
	for (i=0, client = svs.clients ; i<svs.allocated_client_slots ; i++, client++)
		SV_FullClientUpdate(client, host_client);
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
			if ((host_client->fteprotocolextensions & PEXT_LIGHTSTYLECOL) && (sv.lightstylecolours[i][0]!=1||sv.lightstylecolours[i][1]!=1||sv.lightstylecolours[i][2]!=1) && sv.strings.lightstyles[i])
			{
				ClientReliableWrite_Begin (host_client, svcfte_lightstylecol,
					10 + (sv.strings.lightstyles[i] ? strlen(sv.strings.lightstyles[i]) : 1));
				ClientReliableWrite_Byte (host_client, (char)i);
				ClientReliableWrite_Char (host_client, 0x87);
				ClientReliableWrite_Short (host_client, sv.lightstylecolours[i][0]*1024);
				ClientReliableWrite_Short (host_client, sv.lightstylecolours[i][1]*1024);
				ClientReliableWrite_Short (host_client, sv.lightstylecolours[i][2]*1024);
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
#ifdef HEXEN2
				split->playerclass = ent->xv->playerclass;
#endif
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

#ifdef QUAKESTATS
		secret_total = pr_global_struct->total_secrets;
		secret_found = pr_global_struct->found_secrets;
		monsters_total = pr_global_struct->total_monsters;
		monsters_found = pr_global_struct->killed_monsters;
#endif
	}

#ifdef QUAKESTATS
	ClientReliableWrite_Begin (host_client, svcqw_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALSECRETS);
	ClientReliableWrite_Long (host_client, secret_total);

	ClientReliableWrite_Begin (host_client, svcqw_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALMONSTERS);
	ClientReliableWrite_Long (host_client, monsters_total);

	ClientReliableWrite_Begin (host_client, svcqw_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_SECRETS);
	ClientReliableWrite_Long (host_client, secret_found);

	ClientReliableWrite_Begin (host_client, svcqw_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_MONSTERS);
	ClientReliableWrite_Long (host_client, monsters_found);
#endif

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

	for (i=svs.allocated_client_slots+1 ; i<sv.world.num_edicts ; i++)
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
#ifdef HEXEN2
	if (progstype == PROG_H2 && split->playerclass)
		split->edict->xv->playerclass = split->playerclass;	//make sure it's set the same as the userinfo
#endif

	if (split->spawned)
		return;
	split->spawned = true;

#ifdef Q2SERVER
	if (ge)
	{
		ge->ClientUserinfoChanged (split->q2edict, split->userinfo);	//tell the gamecode
		SV_ExtractFromUserinfo(split, true);	//let the server routines know

		ge->ClientBegin(split->q2edict);
		split->istobeloaded = false;
		sv.spawned_client_slots++;
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
#ifndef NOLEGACY
		sv_player->xv->clientcolors = split->playercolor;
		if (progstype != PROG_QW)
		{	//some redundant things, purely for dp compat
			eval_t *eval;
			edict_t *ent = split->edict;
			sv_player->v->team = split->playercolor&15;

			eval = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "playermodel", ev_string, NULL);
			if (eval)
				svprogfuncs->SetStringField(svprogfuncs, ent, &eval->string, Info_ValueForKey(split->userinfo, "model"), false);

			eval = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "playerskin", ev_string, NULL);
			if (eval)
				svprogfuncs->SetStringField(svprogfuncs, ent, &eval->string, Info_ValueForKey(split->userinfo, "skin"), false);

			eval = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "netaddress", ev_string, NULL);
			if (eval)
			{
				char buf[256];
				svprogfuncs->SetStringField(svprogfuncs, ent, &eval->string, NET_AdrToString(buf, sizeof(buf), &split->netchan.remote_address), false);
			}
		}
#endif


		if (split->spectator)
		{
			SV_SpawnSpectator ();

			if (SpectatorConnect)
			{
				//keep the spectator tracking the player from the previous map
				if (split->spec_track > 0)
					split->edict->v->goalentity = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, split->spec_track));
				else
					split->edict->v->goalentity = 0;


				// copy spawn parms out of the client_t
				SV_SpawnParmsToQC(split);

				// call the spawn function
				pr_global_struct->time = sv.world.physicstime;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
				PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
			}
			sv.spawned_observer_slots++;
		}
		else
		{
			sv.spawned_client_slots++;

			if (svprogfuncs)
			{
				eval_t *eval, *eval2;
				eval = PR_FindGlobal(svprogfuncs, "ClientReEnter", 0, NULL);
				if (eval && eval->function && split->spawninfo)
				{
					globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
					size_t j;
					edict_t *ent;
					ent = split->edict;
					j = strlen(split->spawninfo);
					World_UnlinkEdict((wedict_t*)ent);
					svprogfuncs->restoreent(svprogfuncs, split->spawninfo, &j, ent);

					eval2 = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "stats_restored", ev_float, NULL);
					if (eval2)
						eval2->_float = 1;
					SV_SpawnParmsToQC(split);
					pr_global_struct->time = sv.world.physicstime;
					pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
					G_FLOAT(OFS_PARM0) = sv.time - split->spawninfotime;
					PR_ExecuteProgram(svprogfuncs, eval->function);
				}
				else
				{
					// copy spawn parms out of the client_t
					SV_SpawnParmsToQC(split);

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
						sv.skipbprintclient = host_client;
						if (pr_global_ptrs->ClientConnect)
							PR_ExecuteProgram (svprogfuncs, *pr_global_ptrs->ClientConnect);
						sv.skipbprintclient = NULL;

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
							split->edict->v->movetype = MOVETYPE_NOCLIP;
						}
						VectorCopy(split->edict->v->origin, split->edict->v->oldorigin);	//make sure oldorigin isn't 0 0 0 or anything too clumsy like that. stuck somewhere killable is better than stuck outside the map.
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

#ifndef NOLEGACY
	split->dp_ping = NULL;
	split->dp_pl = NULL;
	if (progstype == PROG_NQ)
	{
		split->dp_ping = (float*)sv.world.progs->GetEdictFieldValue(sv.world.progs, sv_player, "ping", ev_float, NULL);
		split->dp_pl = (float*)sv.world.progs->GetEdictFieldValue(sv.world.progs, sv_player, "ping_packetloss", ev_float, NULL);
	}
#endif
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
			SV_BroadcastTPrintf (PRINT_HIGH, "warning: %s eyes or player model does not match\n", host_client->name);
	}

	// if we are paused, tell the client
	if (sv.paused)
	{
		if (!ISQ2CLIENT(host_client))
		{
			ClientReliableWrite_Begin (host_client, svc_setpause, 2);
			ClientReliableWrite_Byte (host_client, sv.paused!=0);
		}
		if (sv.paused&~PAUSE_AUTO)
			SV_ClientTPrintf(host_client, PRINT_HIGH, "server is paused\n");
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

	if (size > msg->maxsize - msg->cursize)
		size = msg->maxsize - msg->cursize;
	if (size <= 7)
		return;	//no space.
	size -= 7;

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
	host_client->downloadacked = true;
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

		s = va("\ncl_downloadfinished %u %i \"\"\n", (unsigned int)host_client->downloadsize, crc);
		ClientReliableWrite_Begin (cl, svc_stufftext, 2+strlen(s));
		ClientReliableWrite_String(cl, s);

		VFS_CLOSE(host_client->download);
		host_client->send_message = true;
		host_client->download = NULL;
		host_client->downloadsize = 0;
	}
}

static void SV_NextChunkedDownload(unsigned int chunknum, int ezpercent, int ezfilenum, int chunks)
{
	char buffer[DLBLOCKSIZE];
	qbyte oobdata[1+ (sizeof("\\chunk")-1) + 4 + 1 + 4 + DLBLOCKSIZE];
	sizebuf_t *msg, msg_oob;
	int i;
	int error = false;
//can't support this yet. at least forcing to 1 avoids too bad infinite loops. this can be a nasty dos attack on a server.  	if (chunks < 1)
		chunks = 1;

	msg = &host_client->datagram;

	if (chunknum == -1)
		error = 2;	//silent, don't report it
	else if (chunknum*DLBLOCKSIZE > host_client->downloadsize)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, "Warning: Invalid file chunk requested %u to %u of %u.\n", chunknum*DLBLOCKSIZE, (chunknum+1)*DLBLOCKSIZE, host_client->downloadsize);
		error = 2;
	}

	if (!error && VFS_SEEK (host_client->download, (qofs_t)chunknum*DLBLOCKSIZE) == false)
		error = true;
	else
	{
		if (host_client->downloadcount < chunknum*DLBLOCKSIZE)
			host_client->downloadcount = chunknum*DLBLOCKSIZE;
	}

	
	while (!error && chunks > 0)
	{
		if ((host_client->datagram.cursize + DLBLOCKSIZE+5+50 > host_client->datagram.maxsize) || (host_client->datagram.cursize + DLBLOCKSIZE+5 > 1400))
		{
			//would overflow the packet, or result in (ethernet) fragmentation and high packet loss.
			msg = &msg_oob;

			if (!ezfilenum)	//can't oob it
				return;

			if (host_client->waschoked)
				return;	//don't let chunked downloads flood out the standard packets.

			if (!Netchan_CanPacket(&host_client->netchan, SV_RateForClient(host_client)))
				return;
		}

		i = VFS_READ (host_client->download, buffer, DLBLOCKSIZE);

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

			if (i != DLBLOCKSIZE)
				memset(buffer+i, 0, DLBLOCKSIZE-i);

			MSG_WriteByte(msg, svc_download);
			MSG_WriteLong(msg, chunknum);
			SZ_Write(msg, buffer, DLBLOCKSIZE);

			if (msg == &msg_oob)
			{
				Netchan_OutOfBand(NS_SERVER, &host_client->netchan.remote_address, msg_oob.cursize, msg_oob.data);
				Netchan_Block(&host_client->netchan, msg_oob.cursize, SV_RateForClient(host_client));
				host_client->netchan.bytesout += msg_oob.cursize;
			}
		}
		else if (i < 0)
			error = true;

		chunks--;
		chunknum++;
	}

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
			SV_NextChunkedDownload(atoi(Cmd_Argv(1)), atoi(Cmd_Argv(2)), atoi(Cmd_Argv(3)), atoi(Cmd_Argv(4)));
		else
			SV_NextChunkedDownload(atoi(Cmd_Argv(1)), atoi(Cmd_Argv(2)), atoi(Cmd_Argv(3)), atoi(Cmd_Argv(4)));
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

void VARGS OutofBandPrintf(netadr_t *where, char *fmt, ...)
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
		SV_ClientTPrintf(host_client, PRINT_HIGH, "Upload denied\n");
		ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
		ClientReliableWrite_String (host_client, "stopul\n");

		// suck out rest of packet
		size = MSG_ReadShort ();	MSG_ReadByte ();
		msg_readcount += size;
		return;
	}

	size = MSG_ReadShort ();
	percent = MSG_ReadByte ();

	if (!host_client->upload)
	{
		FS_CreatePath(host_client->uploadfn, FS_GAMEONLY);
		host_client->upload = FS_OpenVFS(host_client->uploadfn, "wb", FS_GAMEONLY);
		if (!host_client->upload)
		{
			Sys_Printf("Can't create %s\n", host_client->uploadfn);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
			ClientReliableWrite_String (host_client, "stopul\n");
			*host_client->uploadfn = 0;
			return;
		}
		Con_Printf("Receiving %s from %d...\n", host_client->uploadfn, host_client->userid);
		if (host_client->remote_snap)
			OutofBandPrintf(&host_client->snap_from, "Server receiving %s from %d...\n", host_client->uploadfn, host_client->userid);
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
			OutofBandPrintf(&host_client->snap_from, "%s upload completed.\nTo download, enter:\ndownload %s\n",
				host_client->uploadfn, p);
		}
		*host_client->uploadfn = 0;	//don't let it get overwritten again
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
			unsigned char receiver[(MAX_CLIENTS+7)/8];
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
	//voice data does not get echoed to the sender unless sv_voip_echo is on too, which is rarely the case, so no worries about leaking the mute+deaf talking-to-yourself thing
	if (bytes > sizeof(ring->data) || (host_client->penalties & BAN_MUTE) || !sv_voip.ival)
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
	if (vt == VT_TEAM && (!teamplay.ival || coop.ival))
		vt = VT_ALL;

	/*figure out which team members are meant to receive it*/
	for (j = 0; j < (MAX_CLIENTS+7)/8; j++)
		ring->receiver[j] = 0;
	for (j = 0, cl = svs.clients; j < svs.allocated_client_slots; j++, cl++)
	{
		if (cl->state != cs_spawned && cl->state != cs_connected)
			continue;
		/*spectators may only talk to spectators*/
		if (host_client->spectator && !sv_spectalk.ival)
			if (!cl->spectator)
				continue;

		if (cl->penalties & BAN_DEAF)
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
		else if (vt == VT_SPECSELF)
		{
			//spectators spectating self hear it.
			//self hears it (for demos, voip_echo stops it from actually being echoed)
			if (!host_client->spectator || host_client->spec_track != host_client-svs.clients)
				if (j != host_client-svs.clients)
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
		sizebuf_t *msg;
		// non-team messages should be seen always, even if not tracking any player
		if (vt == VT_ALL && (!host_client->spectator || sv_spectalk.ival))
		{
			msg = MVDWrite_Begin (dem_all, 0, ring->datalen+6);
		}
		else
		{
			unsigned int cls;
			cls = ring->receiver[0] |
				(ring->receiver[1]<<8) |
				(ring->receiver[2]<<16) |
				(ring->receiver[3]<<24);
			msg = MVDWrite_Begin (dem_multiple, cls, ring->datalen+6);
		}

		MSG_WriteByte(msg, svcfte_voicechat);
		MSG_WriteByte(msg, ring->sender);
		MSG_WriteByte(msg, ring->gen);
		MSG_WriteByte(msg, ring->seq);
		MSG_WriteShort(msg, ring->datalen);
		SZ_Write(msg, ring->data, ring->datalen);
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
		if (host_client->spectator && host_client->spec_track && host_client->spec_track <= sv.allocated_client_slots)
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
	if (other >= svs.allocated_client_slots)
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
	char *v = Cmd_Argv(2);
	qboolean verbose = *v?atoi(v):host_client->voice_active;
	if (!strcmp(t, "team"))
	{
		host_client->voice_target = VT_TEAM;
		if (verbose)
		{
			if (teamplay.ival)
				SV_ClientTPrintf(host_client, PRINT_HIGH, "Now sending voice to team\n");
			else
				SV_ClientTPrintf(host_client, PRINT_HIGH, "Now sending voice to all (no teamplay)\n");
		}
	}
	else if (!strcmp(t, "all"))
	{
		host_client->voice_target = VT_ALL;
		if (verbose)
			SV_ClientTPrintf(host_client, PRINT_HIGH, "Now sending voice to all\n");
	}
	else if (!strcmp(t, "specself"))
	{
		host_client->voice_target = VT_SPECSELF;
		if (verbose)
			SV_ClientTPrintf(host_client, PRINT_HIGH, "Now sending voice to your personal admirers\n");
	}
	else if (!strcmp(t, "nonmuted"))
	{
		host_client->voice_target = VT_NONMUTED;
		if (verbose)
			SV_ClientTPrintf(host_client, PRINT_HIGH, "Now sending voice to all people you've not ignored\n");
	}
	else if (*t >= '0' && *t <= '9')
	{
		other = atoi(t);
		if (other >= svs.allocated_client_slots)
		{
			if (verbose)
				SV_ClientTPrintf(host_client, PRINT_HIGH, "Invalid client\n");
			return;
		}
		host_client->voice_target = VT_PLAYERSLOT0 + other;
		if (verbose)
		{
			if (host_client->state >= cs_connected)
				SV_ClientTPrintf(host_client, PRINT_HIGH, "Now sending voice only to %s\n", host_client->name);
			else
				SV_ClientTPrintf(host_client, PRINT_HIGH, "Now sending voice only to player slot %i, if someone occupies it\n", other);
		}
	}
	else
	{
		/*don't know who you mean, futureproofing*/
		host_client->voice_target = VT_TEAM;
		if (verbose)
			SV_ClientTPrintf(host_client, PRINT_HIGH, "Now sending voice to team\n");
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
	extern	cvar_t	allow_download_particles;
	extern	cvar_t	allow_download_demos;
	extern	cvar_t	allow_download_maps;
	extern	cvar_t	allow_download_textures;
	extern	cvar_t	allow_download_packages;
	extern	cvar_t	allow_download_wads;
	extern	cvar_t	allow_download_root;
	extern	cvar_t	allow_download_logs;
	extern	cvar_t	allow_download_configs;
	extern	cvar_t	allow_download_locs;
	extern	cvar_t	allow_download_copyrighted;
	extern	cvar_t	allow_download_other;
	char cleanname[MAX_QPATH];
	char ext[8];
	int i=0;
	if (strlen(name) >= MAX_QPATH)
		return false;
	do
	{
		cleanname[i++] = *name;
	} while(*name++);
	name = cleanname;

	//allowed at all?
	if (!allow_download.ival)
		return false;

	//no subdirs?
	if (strstr (name, ".."))	//no under paths.
		return false;
	if (*name == '.')	//relative is pointless
		return false;
	if (*name == '/')	//no absolute.
		return false;
	if (strchr(name, '\\'))	//no windows paths - grow up you lame windows users.
		return false;

	COM_FileExtension(name, ext, sizeof(ext));

	//block attempts to download logs
	if (!Q_strcasecmp("log", ext))
		return !!allow_download_logs.value;
	if (Q_strncasecmp(name,	"logs/", 5) == 0)
		return !!allow_download_logs.value;

	if (!Q_strncasecmp(name, "package/", 8))
	{
		//eg: package/id1/foobar.pk3
		if (!Q_strcasecmp("pk4", ext) || !Q_strcasecmp("pk3", ext) || !Q_strcasecmp("pak", ext) || (!Q_strncasecmp(name, "package/downloads/", 18) && !Q_strcasecmp("zip", ext)))
		{
			if (!allow_download_packages.ival)
				return false;
			/*do not permit 'id1/pak1.pak' or 'baseq3/pak0.pk3' or any similarly named packages. such packages would violate copyright, and must be obtained through other means (like buying the damn game)*/
			if (!allow_download_copyrighted.ival && !FS_GetPackageDownloadable(name+8))
				return false;

			return true;
		}
		return false;
	}

	if (Q_strncasecmp(name,	"maps/", 5) == 0)
		return !!allow_download_maps.value;

	//skins?
	if (Q_strncasecmp(name,	"skins/", 6) == 0)
		return !!allow_download_skins.value;
	//models
	if ((Q_strncasecmp(name,	"progs/", 6) == 0) ||
		(Q_strncasecmp(name,	"models/", 7) == 0))
		return !!allow_download_models.value;
	//sound
	if (Q_strncasecmp(name,	"sound/", 6) == 0)
		return !!allow_download_sounds.value;
	//particles
	if (Q_strncasecmp(name,	"particles/", 6) == 0)
		return !!allow_download_particles.value;
	//demos
	if (Q_strncasecmp(name,	"demos/", 6) == 0)
		return !!allow_download_demos.value;

	//textures
	if (Q_strncasecmp(name,	"textures/", 9) == 0)
		return !!allow_download_textures.value;

	if (Q_strncasecmp(name,	"locs/", 5) == 0)
		return !!allow_download_locs.value;

	//wads
	if (Q_strncasecmp(name,	"wads/", 5) == 0)
		return !!allow_download_wads.value;
	if (!strchr(name, '/') && !Q_strcasecmp("wad", ext))
		return !!allow_download_wads.value;

	//configs
	if (Q_strncasecmp(name,	"config/", 7) == 0)
		return !!allow_download_configs.value;
	if (!Q_strcasecmp("cfg", ext))
		return !!allow_download_configs.value;

	//pak/pk3s.
	if (!strchr(name, '/') && (!Q_strcasecmp("pk4", ext) || !Q_strcasecmp("pk3", ext) || !Q_strcasecmp("pak", ext)))
	{
		if (Q_strncasecmp(name, "pak", 3))	//don't give out core pak/pk3 files. This matches q3 logic.
			return !!allow_download_packages.value;
		else
			return !!allow_download_packages.value && !!allow_download_copyrighted.value;
	}

	//root of gamedir
	if (!strchr(name, '/') && !allow_download_root.value)
	{
		if (!strcmp(name, "csprogs.dat"))	//we always allow csprogs.dat to be downloaded (if downloads are permitted).
			return true;
		return false;
	}

	//any other subdirs are allowed
	return !!allow_download_other.value;
}

static int SV_LocateDownload(const char *name, flocation_t *loc, char **replacementname, qboolean redirectpaks)
{
	extern	cvar_t	allow_download_anymap, allow_download_pakcontents;
	extern cvar_t sv_demoDir;
	qboolean protectedpak;
	qboolean found;
	static char tmpname[MAX_QPATH];

	if (replacementname)
		*replacementname = NULL;

	//mvdsv demo downloading support demonum/ -> demos/XXXX (sets up the client paths)
	if (!Q_strncasecmp(name, "demonum/", 8))
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
			name = *replacementname = va("demos/%s", mvdname);
			return DLERR_REDIRECTFILE;
		}
	}

	if (!SV_AllowDownload(name))
	{
		Sys_Printf ("%s denied download of %s due to path/name rules\n", host_client->name, name);
		return DLERR_PERMISSIONS;	//not permitted (even if it exists).
	}

	//mvdsv demo downloading support. demos/ -> demodir (sets up the server paths)
	if (!Q_strncasecmp(name, "demos/", 6))
	{
		Q_snprintfz(tmpname, sizeof(tmpname), "%s/%s", sv_demoDir.string, name+6);
		name = tmpname;
	}

	if (!Q_strncasecmp(name, "package/", 8))
	{
		vfsfile_t *f = FS_OpenVFS(name+8, "rb", FS_ROOT);
		if (f)
		{
			loc->len = VFS_GETLEN(f);
			VFS_CLOSE(f);
			return DLERR_PACKAGE;	//found package
		}
		else
			return DLERR_FILENOTFOUND;	//not found/unable to open
	}
#ifdef TERRAIN
	else if (Terrain_LocateSection(name, loc))
	{
		found = true;
	}
#endif
	else
		found = FS_FLocateFile(name, FSLF_IFFOUND, loc);

	if (!found && replacementname)
	{
		size_t alt;
		static const char *alternatives[][4] = {
			//orig-path, orig-ext, new-path, new-ext
			//nexuiz qc names [sound/]sound/foo.wav but expects sound/foo.ogg and variations of that (the [sound/] is implied, but ignored)
			{"",		"", ".wav", ".ogg"},	//nexuiz qc names .wav, but the paks use .ogg
			{"sound/", "",	".wav",	".wav"},	//nexuiz qc names sound/ but that's normally implied, resulting in doubles that don't exist in the filesystem
			{"sound/", "",	".wav",	".ogg"} 	//both of nexuiz's issues at the same time
		};

		for (alt = 0; alt < countof(alternatives); alt++)
		{
			char tryalt[MAX_QPATH];
			char ext[8];
			if (Q_strncasecmp(name, alternatives[alt][0], strlen(alternatives[alt][0])))
			{
				if (*alternatives[alt][2])
				{
					if (Q_strcasecmp(COM_FileExtension(name, ext, sizeof(ext)), alternatives[alt][2]+1))
						continue;
					memcpy(tryalt, alternatives[alt][1], strlen(alternatives[alt][1]));
					COM_StripExtension(name+strlen(alternatives[alt][0]), tryalt+strlen(alternatives[alt][1]), sizeof(tryalt)-strlen(alternatives[alt][1]));
					COM_DefaultExtension(tryalt, alternatives[alt][3], sizeof(tryalt));
				}
				else
				{
					memcpy(tryalt, alternatives[alt][1], strlen(alternatives[alt][1]));
					Q_strncpyz(tryalt+strlen(alternatives[alt][1]), name+strlen(alternatives[alt][0]), sizeof(tryalt)-strlen(alternatives[alt][1]));
				}
				found = FS_FLocateFile(tryalt, FSLF_IFFOUND, loc);
				if (found)
				{
					Q_snprintfz(tmpname, sizeof(tmpname), "%s", tryalt);
					name = *replacementname = tmpname;
					break;
				}
			}
		}
	}

	if (found)
	{
		protectedpak = loc->search && (loc->search->flags & SPF_COPYPROTECTED);

		// special check for maps, if it came from a pak file, don't allow download
		if (protectedpak)
		{
			if (!allow_download_anymap.value && !Q_strncasecmp(name, "maps/", 5))
			{
				Sys_Printf ("%s denied download of %s - it is in a pak\n", host_client->name, name+8);
				return DLERR_PERMISSIONS;
			}
		}

		if (replacementname)
		{
#if 1
			char *pakname = FS_GetPackageDownloadFilename(loc);
			if (pakname && strchr(pakname, '/'))
			{
				extern cvar_t allow_download_packages,allow_download_copyrighted;	//non authoritive, but should normally match.
				if (allow_download_packages.ival)
				{
					if (allow_download_copyrighted.ival || !protectedpak)
					{
						Q_snprintfz(tmpname, sizeof(tmpname), "package/%s", pakname);
						*replacementname = tmpname;
						return DLERR_REDIRECTPACK;	//redirect
					}
				}
			}
#else
			char *pakname = FS_WhichPackForLocation(loc, false);
			if (pakname && SV_AllowDownload(pakname))
			{
				//return loc of the pak instead.
				if (FS_FLocateFile(name, FSLF_IFFOUND, loc))
				{
					//its inside a pak file, return the name of this file instead
					*replacementname = pakname;
					return DLERR_REDIRECTPACK;	//redirect
				}
				else
					Con_Printf("Failed to read %s\n", pakname);
			}
#endif
		}

		if (protectedpak)
		{	//if its in a pak file, don't allow downloads if we don't allow the contents of paks to be sent.
			if (!allow_download_pakcontents.value)
			{
				Sys_Printf ("%s denied download of %s - it is in a pak\n", host_client->name, name+8);
				return DLERR_PERMISSIONS;
			}
		}

		if (replacementname && *replacementname)
			return DLERR_REDIRECTPACK;
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
	case DLERR_REDIRECTFILE: /*redirect (extension difference or so)*/
	case DLERR_REDIRECTPACK: /*redirect (the containing package)*/
		name = va("dlsize \"%s\" r \"%s\"\n", name, redirected);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(name));
		ClientReliableWrite_String (host_client, name);
		break;
	default:
	case DLERR_FILENOTFOUND: /*not found*/
		name = va("dlsize \"%s\" e\n", name);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(name));
		ClientReliableWrite_String (host_client, name);
		break;
	case DLERR_PERMISSIONS: /*permission*/
		name = va("dlsize \"%s\" p\n", name);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(name));
		ClientReliableWrite_String (host_client, name);
		break;
	case DLERR_PACKAGE: /*requested file is a package*/
	case 0: /*file exists*/
		name = va("dlsize \"%s\" %u\n", name, (unsigned int)loc.len);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(name));
		ClientReliableWrite_String (host_client, name);
		break;
	}
}

void SV_DemoDownload_f(void)
{
	int arg;
	unsigned long num;
	const char *name, *mvdname;
	char mvdnamebuffer[MAX_QPATH];
	if (Cmd_Argc() < 2)
	{
		//fixme: help text, or possibly just display links to the last 10 demos?
		return;
	}
	if (Cmd_Argc() == 2)
	{
		name = Cmd_Argv(1);
		if (!strcmp(name, "\\") || !Q_strcasecmp(name, "stop") || !Q_strcasecmp(name, "cancel"))
		{
			//fte servers don't do download queues, as it is impossible to avoid race conditions with vanilla clients anyway.
			return;
		}
	}

	for (arg = 1; arg < Cmd_Argc(); arg++)
	{
		name = Cmd_Argv(arg);
		if (*name == '.')
		{	//just count the dots
			for (num = 0, mvdname = name; *mvdname == '.'; mvdname++)
				num++;
			if (*mvdname)
			{
				SV_ClientPrintf (host_client, PRINT_HIGH, "invalid demo id %s\n", name);
				continue;
			}
			mvdname = SV_MVDLastNum(num);
		}
		else
		{
			char *e;
			num = strtoul(name, &e, 10);
			if (!num || *e)
			{
				SV_ClientPrintf (host_client, PRINT_HIGH, "invalid demo id %s\n", name);
				continue;
			}
			mvdname = SV_MVDNum(mvdnamebuffer, sizeof(mvdnamebuffer), num);
		}

		if (!mvdname)
			SV_ClientPrintf (host_client, PRINT_HIGH, "%s is an invalid MVD demonum.\n", name);
		else
		{
			const char *s = va("download \"demos/%s\"\n", mvdname);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(s));
			ClientReliableWrite_String (host_client, s);
		}
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

/*
	if (ISQWCLIENT(host_client) && !strcmp(name, "ezquake-security.dll"))
	{
		vfsfile_t *f = FS_OpenVFS("evil.dll", "rb", FS_GAME);
		if (f)
		{
			int chunk, o = 0;
			int l = VFS_GETLEN(f);
			char *data = malloc(l);
			VFS_READ(f, data, l);
			VFS_CLOSE(f);

			while (o < l)
			{
				chunk = 768;
				if (o + chunk > l)
					chunk = l - o;

				ClientReliableWrite_Begin (host_client, ISQ2CLIENT(host_client)?svcq2_download:svc_download, 6+chunk);
				ClientReliableWrite_Short (host_client, chunk);
				ClientReliableWrite_Byte (host_client, (o+chunk == l)?100:0);	//lame, whatever.
				ClientReliableWrite_SZ (host_client, data + o, chunk);
				o += chunk;
			}
		}

		ClientReliableWrite_Begin	(host_client, svc_stufftext, 128);
		ClientReliableWrite_String	(host_client, "cmd new\n");
		return;
	}
*/

	*host_client->downloadfn = 0;

	if (host_client->download)
	{
		VFS_CLOSE (host_client->download);
		host_client->download = NULL;
	}

	result = SV_LocateDownload(name, &loc, &redirection, false);

	if (result == DLERR_PACKAGE)
	{
		//package download
		result = 0;
		host_client->download = FS_OpenVFS(name+8, "rb", FS_ROOT);
	}
	else
	{
		//redirection protocol-specific code goes here.
		if (result == DLERR_REDIRECTPACK || result == DLERR_REDIRECTFILE)
		{
#ifdef PEXT_CHUNKEDDOWNLOADS
			//ezquake etc cannot cope with proper redirects
			if ((host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS) && (host_client->fteprotocolextensions & PEXT_CSQC))
			{
				//redirect the client (before the message saying download failed)
//				char *s = va("dlsize \"%s\" r \"%s\"\n", name, redirection);
//				ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(s));
//				ClientReliableWrite_String (host_client, s);

				ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(name));
				ClientReliableWrite_Long (host_client, -1);
				ClientReliableWrite_Long (host_client, DLERR_REDIRECTFILE);
				ClientReliableWrite_String (host_client, redirection);
				if (ISNQCLIENT(host_client))
					host_client->send_message = true;
				return;
			}
			else if (result == DLERR_REDIRECTFILE && host_client->protocol == SCP_QUAKEWORLD)
			{	//crappy hack for crappy clients. tell them to download the new file instead without telling them about any failure.
				//this will seriously mess with any download queues or anything like that
				//this doesn't apply to packages, because these shitty clients won't know to actually load said packages before attempting to request more files, meaning the same package gets downloaded 80 times and then only actually used AFTER they restart the client.
				char *s = va("download \"%s\"\n", redirection);
				ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(s));
				ClientReliableWrite_String (host_client, s);
				if (ISNQCLIENT(host_client))
					host_client->send_message = true;
				return;
			}
#endif
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
		case DLERR_FILENOTFOUND:
		default:
			result = DLERR_FILENOTFOUND;
			error = "Download could not be found\n";
			break;
		case DLERR_UNKNOWN:
			error = "Filesystem error\n";
			break;
		case DLERR_PERMISSIONS:
			error = "Download permission denied\n";
			break;
		case DLERR_REDIRECTFILE:
			result = DLERR_PERMISSIONS;
			error = "Client doesn't support file redirection\n";
			break;
		case DLERR_REDIRECTPACK:
			result = DLERR_PERMISSIONS;
			error = "Package contents not available individually\n";
			break;
		}
#ifdef PEXT_CHUNKEDDOWNLOADS
		if (host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
		{
			ClientReliableWrite_Begin (host_client, svc_download, 10+strlen(name));
			ClientReliableWrite_Long (host_client, -1);
			ClientReliableWrite_Long (host_client, result);
			ClientReliableWrite_String (host_client, name);
		}
		else
#endif
		if (ISNQCLIENT(host_client))
		{	//dp's download protocol
			SV_PrintToClient(host_client, PRINT_HIGH, error);

			ClientReliableWrite_Begin (host_client, svc_stufftext, 2+12);
			ClientReliableWrite_String (host_client, "\nstopdownload\n");
		}
		else
		{
			SV_PrintToClient(host_client, PRINT_HIGH, error);

			ClientReliableWrite_Begin (host_client, ISQ2CLIENT(host_client)?svcq2_download:svc_download, 4);
			ClientReliableWrite_Short (host_client, -1);
			ClientReliableWrite_Byte (host_client, 0);
		}
		if (ISNQCLIENT(host_client))
			host_client->send_message = true;
		return;
	}

	Q_strncpyz(host_client->downloadfn, name, sizeof(host_client->downloadfn));
	host_client->downloadcount = 0;

	host_client->downloadsize = VFS_GETLEN(host_client->download);

#ifdef PEXT_CHUNKEDDOWNLOADS
	if (host_client->fteprotocolextensions & PEXT_CHUNKEDDOWNLOADS)
	{
		if (host_client->download->seekstyle != SS_SEEKABLE)
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

		ClientReliableWrite_Begin (host_client, svc_download, 18+strlen(host_client->downloadfn));
		ClientReliableWrite_Long (host_client, -1);
		if (host_client->downloadsize >= 0x7fffffff)
		{	//avoid unsigned values.
			ClientReliableWrite_Long (host_client, 0x80000000);	//signal that its 64bit
			ClientReliableWrite_Long (host_client, qofs_Low(host_client->downloadsize));
			ClientReliableWrite_Long (host_client, qofs_High(host_client->downloadsize));
		}
		else
			ClientReliableWrite_Long (host_client, host_client->downloadsize);
		ClientReliableWrite_String (host_client, host_client->downloadfn);
	}
	else
#endif
		if (ISNQCLIENT(host_client))
	{
		//FIXME support 64bit files
		char *s = va("\ncl_downloadbegin %u %s\n", (unsigned int)host_client->downloadsize, host_client->downloadfn);
		ClientReliableWrite_Begin (host_client, svc_stufftext, 2+strlen(s));
		ClientReliableWrite_String (host_client, s);
		host_client->send_message = true;
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
		SV_ClientPrintf(host_client, PRINT_HIGH, "Can't stop download - not downloading anything\n");

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

	if ((host_client->penalties & BAN_MUTE) && !(host_client->penalties & (BAN_DEAF|BAN_STEALTH)))
	{
		SV_ClientTPrintf(host_client, PRINT_CHAT, "You are muted\n");
		return;
	}

	while((to = SV_GetClientForString(Cmd_Argv(1), &clnum)))
	{
		if ((host_client->penalties & BAN_MUTE) && to != host_client)
			continue;
		if (host_client->spectator)
		{
			if (!sv_spectalk.value || to->spectator)
				Q_snprintfz (text, sizeof(text), "[SPEC] {%s}:", host_client->name);
			else
				continue;
		}
		else
			Q_snprintfz (text, sizeof(text), "{%s}:", host_client->name);

		if (to->penalties & BAN_DEAF)
			continue;

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
		SV_ClientTPrintf(host_client, PRINT_CHAT, "client does not exist\n");
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
	sizebuf_t *msg;

	qboolean sent[MAX_CLIENTS];	//so we don't send to the same splitscreen connection twice. (it's ugly)
	int cln;

	qboolean mvdrecording;

	char *s, *s2;

	if (Cmd_Argc () < 2)
		return;

	if (!(host_client->penalties & BAN_MUTE))
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

	if ((host_client->penalties & BAN_MUTE) && !(host_client->penalties & (BAN_DEAF|BAN_STEALTH)))
	{
		SV_ClientTPrintf(host_client, PRINT_CHAT, "You cannot chat while muted\n");
		return;
	}

#ifdef VM_Q1
	if (Q1QVM_ClientSay(sv_player, team))
		return;
#endif

	if ((floodtime=SV_CheckFloodProt(host_client)))
	{
		SV_ClientTPrintf(host_client, PRINT_CHAT, "You can't talk for %i more seconds\n", (int) (floodtime));
		return;
	}
	SV_PushFloodProt(host_client);

	p = Cmd_Args();

	if (*p == '"')
	{
		char *e = p + strlen(p)-1;
		*p++ = 0;
		if (*e == '\"')
			*e = 0;
	}

	if (strlen(text)+strlen(p)+2 >= sizeof(text)-10)
	{
		SV_ClientTPrintf(host_client, PRINT_CHAT, "buffer overflow protection: failiure\n");
		return;
	}
	if (svprogfuncs)
		if (PR_QCChat(p, team))	//true if handled.
			return;

	if (strstr(p, "password"))
	{
		Z_Free(host_client->centerprintstring);
		host_client->centerprintstring = Z_StrDup("big brother is watching you");
	}

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

	if (!(host_client->penalties & BAN_MUTE))
		Sys_Printf ("%s", text);

	mvdrecording = sv.mvdrecording;
	sv.mvdrecording = false;	//so that the SV_ClientPrintf doesn't send to all players.
	for (j = 0, client = svs.clients; j < svs.allocated_client_slots; j++, client++)
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

		if (host_client->penalties & BAN_MUTE)
		{
			if (client != host_client)
				continue;
		}
		else if (client->penalties & BAN_DEAF)
		{
			if (client != host_client || !(host_client->penalties & BAN_STEALTH))
				continue;
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
		msg = MVDWrite_Begin (dem_all, 0, strlen(text)+3);
	else
		msg = MVDWrite_Begin (dem_multiple, cls, strlen(text)+3);

	MSG_WriteByte (msg, svc_print);
	MSG_WriteByte (msg, PRINT_CHAT);
	MSG_WriteString (msg, text);
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
		for (j = 0, client = svs.clients; j < svs.allocated_client_slots; j++, client++)
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
		ClientReliableWrite_Begin(host_client, svc_stufftext, 15+10*sv.allocated_client_slots);
		ClientReliableWrite_SZ(host_client, "pingplreport", 12);
		for (j = 0, client = svs.clients; j < sv.allocated_client_slots && j < host_client->max_net_clients; j++, client++)
		{
			s = va(" %i %i", SV_CalcPing(client, false), client->lossage);
			ClientReliableWrite_SZ(host_client, s, strlen(s));
		}
		ClientReliableWrite_Byte (host_client, '\n');
		ClientReliableWrite_Byte (host_client, '\0');

	}
	else
	{
		for (j = 0, client = svs.clients; j < sv.allocated_client_slots && j < host_client->max_net_clients; j++, client++)
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

	switch(svs.gametype)
	{
#ifdef VM_Q1
	case GT_Q1QVM:
		pr_global_struct->time = sv.world.physicstime;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		Q1QVM_ClientCommand();
		return;
#endif
#ifdef VM_LUA
	case GT_LUA:
#endif
	case GT_PROGS:
		break;
	default:
		return;	//should have its own parsing.
	}

	if (sv_player->v->health <= 0)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, "Can't suicide -- Already dead\n");
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

	if (pr_global_ptrs->ClientKill)
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

	newv = sv.paused^PAUSE_EXPLICIT;

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
		SV_ClientTPrintf (host_client, PRINT_HIGH, "Can't pause. Not allowed\n");
		return;
	}

	if (host_client->spectator && !svs.demoplayback)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, "Spectators may not pause the game\n");
		return;
	}

	if (SV_TogglePause(host_client))
	{
		if (sv.paused & PAUSE_EXPLICIT)
			SV_BroadcastTPrintf (PRINT_HIGH, "%s paused the game\n", host_client->name);
		else
			SV_BroadcastTPrintf (PRINT_HIGH, "%s unpaused the game\n", host_client->name);
	}

}

static void SV_UpdateSeats(client_t *controller)
{
	client_t *cl;
	int curclients;
	
	if (controller->protocol == SCP_QUAKE2)
		return;	//wait for the clientinfo stuff instead.

	for (curclients = 0, cl = controller; cl; cl = cl->controlled)
		curclients++;

	ClientReliableWrite_Begin(controller, svc_signonnum, 2+curclients);
	ClientReliableWrite_Byte(controller, curclients);
	for (curclients = 0, cl = controller; cl; cl = cl->controlled, curclients++)
	{
		ClientReliableWrite_Byte(controller, cl - svs.clients);
	}

	for (curclients = 0, cl = controller; cl; cl = cl->controlled, curclients++)
	{
// send a fixangle over the reliable channel to make sure it gets there
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
		ClientReliableWrite_Begin(controller, svcfte_choosesplitclient, 2+curclients);
		ClientReliableWrite_Byte (controller, curclients);
		ClientReliableWrite_Byte (controller, svc_setangle);
		if (cl->edict->v->fixangle)
		{
			ClientReliableWrite_Angle(controller, cl->edict->v->angles[0]);
			ClientReliableWrite_Angle(controller, cl->edict->v->angles[1]);
			ClientReliableWrite_Angle(controller, 0);//cl->edict->v->angles[2]);
			cl->edict->v->fixangle = 0;
		}
		else
		{
			ClientReliableWrite_Angle(controller, cl->edict->v->v_angle[0]);
			ClientReliableWrite_Angle(controller, cl->edict->v->v_angle[1]);
			ClientReliableWrite_Angle(controller, 0);//cl->edict->v->v_angle[2]);
		}
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
	client_t *prev;

	SV_EndRedirect ();
	if (!host_client->drop)
	{
		if (host_client->redirect == 2)
			SV_BroadcastPrintf (PRINT_HIGH, "%s transfered to %s\n", host_client->name, host_client->transfer);
		else if (host_client->redirect)
			SV_BroadcastPrintf (PRINT_HIGH, "%s redirected to %s\n", host_client->name, sv_fullredirect.string);
		else
		{
			if (!host_client->spectator)
				SV_BroadcastTPrintf (PRINT_HIGH, "%s dropped\n", host_client->name);
		}
		host_client->drop = true;
	}

	//if splitscreen, orphan the dropper
	if (host_client->controller)
	{
		for (prev = host_client->controller; prev; prev = prev->controlled)
		{
			if (prev->controlled == host_client)
			{
				prev->controlled = host_client->controlled;
				host_client->netchan.remote_address.type = NA_INVALID;	//so the remaining client doesn't get the kick too.
				host_client->protocol = SCP_BAD;	//make it a bit like a bot, so we don't try sending any datagrams/reliables at someone that isn't able to receive anything.

				SV_UpdateSeats(host_client->controller);
				host_client->controller->joinobservelockeduntil = realtime + 3;
				host_client->controlled = NULL;
				host_client->controller = NULL;
				break;
			}
		}
	}
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

		if (ISNQCLIENT(host_client))
		{
			ClientReliableWrite_Begin(host_client, svc_setview, 4);
			ClientReliableWrite_Entity(host_client, host_client - svs.clients + 1);
		}
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

	if (!SV_CanTrack(host_client, i+1))
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, "invalid player to track\n");
		host_client->spec_track = 0;
		ent = EDICT_NUM(svprogfuncs, host_client - svs.clients + 1);
		tent = EDICT_NUM(svprogfuncs, 0);
		ent->v->goalentity = EDICT_TO_PROG(svprogfuncs, tent);

		if (ISNQCLIENT(host_client))
		{
			ClientReliableWrite_Begin(host_client, svc_setview, 4);
			ClientReliableWrite_Entity(host_client, host_client - svs.clients + 1);
		}
		return;
	}
	host_client->spec_track = i + 1; // now tracking

	ent = EDICT_NUM(svprogfuncs, host_client - svs.clients + 1);
	tent = EDICT_NUM(svprogfuncs, i + 1);
	ent->v->goalentity = EDICT_TO_PROG(svprogfuncs, tent);

	if (ISNQCLIENT(host_client))
	{
		ClientReliableWrite_Begin(host_client, svc_setview, 4);
		ClientReliableWrite_Entity(host_client, i + 1);
	}
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
		int rate = SV_RateForClient(host_client);
		if (!rate)
			SV_ClientPrintf (host_client, PRINT_HIGH, "Effective rate is unlimited\n");
		else
			SV_ClientPrintf (host_client, PRINT_HIGH, "Effective rate %i\n", rate);
		return;
	}

	Info_SetValueForKey (host_client->userinfo, "rate", Cmd_Argv(1), sizeof(host_client->userinfo));
	SV_ExtractFromUserinfo (host_client, true);

	if (host_client->state > cs_connected)
		SV_ClientTPrintf (host_client, PRINT_HIGH, "rate is changed to %i\n", SV_RateForClient(host_client));
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
		SV_ClientTPrintf (host_client, PRINT_HIGH, "current msg level is %i\n",
			host_client->messagelevel);
		return;
	}

	host_client->messagelevel = atoi(Cmd_Argv(1));

	SV_ClientTPrintf (host_client, PRINT_HIGH, "new msg level set to %i\n", host_client->messagelevel);
}

qboolean SV_UserInfoIsBasic(const char *infoname)
{
	int i;
	char *basicinfos[] = {

		"name",
		"team",
		"skin",
		"topcolor",
		"bottomcolor",
		"chat",	//ezquake's afk indicators

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
	char oldval[MAX_INFO_KEY];
	char *key, *val;

	if (Cmd_Argc() == 1)
	{
		SV_ClientPrintf(host_client, PRINT_HIGH, "User info settings:\n");
		Info_Print (host_client->userinfo, "");
		return;
	}

	if (Cmd_Argc() != 3)
	{
		SV_ClientPrintf(host_client, PRINT_HIGH, "usage: setinfo [ <key> <value> ]\n");
		return;
	}

	key = Cmd_Argv(1);

	if (key[0] == '*')
		return;		// don't set priveledged values

	if (strstr(key, "\\") || strstr(Cmd_Argv(2), "\\"))
		return;		// illegal char

	Q_strncpyz(oldval, Info_ValueForKey(host_client->userinfo, key), sizeof(oldval));

#ifdef VM_Q1
	if (Q1QVM_UserInfoChanged(sv_player))
		return;
#endif

	Info_SetValueForKey (host_client->userinfo, key, Cmd_Argv(2), sizeof(host_client->userinfo));
// name is extracted below in ExtractFromUserInfo
//	strncpy (host_client->name, Info_ValueForKey (host_client->userinfo, "name")
//		, sizeof(host_client->name)-1);
//	SV_FullClientUpdate (host_client, &sv.reliable_datagram);
//	host_client->sendinfo = true;

	if (!strcmp(Info_ValueForKey(host_client->userinfo, key), oldval))
		return; // key hasn't changed

#ifdef Q2SERVER
	if (svs.gametype == GT_QUAKE2)
	{
		ge->ClientUserinfoChanged (host_client->q2edict, host_client->userinfo);	//tell the gamecode
		SV_ExtractFromUserinfo(host_client, true);	//let the server routines know
		return;
	}
#endif

	if (progstype != PROG_QW && !strcmp(key, "bottomcolor"))
	{	//team fortress has a nasty habit of booting people without this
		sv_player->v->team = atoi(Cmd_Argv(2))+1;
	}
#ifndef NOLEGACY
	if (progstype != PROG_QW && !strcmp(key, "model"))
	{
		eval_t *eval = svprogfuncs->GetEdictFieldValue(svprogfuncs, sv_player, "playermodel", ev_string, NULL);
		if (eval)
			svprogfuncs->SetStringField(svprogfuncs, sv_player, &eval->string, Cmd_Argv(2), false);
	}
	if (progstype != PROG_QW && !strcmp(key, "skin"))
	{
		eval_t *eval = svprogfuncs->GetEdictFieldValue(svprogfuncs, sv_player, "playerskin", ev_string, NULL);
		if (eval)
			svprogfuncs->SetStringField(svprogfuncs, sv_player, &eval->string, Cmd_Argv(2), false);
	}
#endif

	// process any changed values
	SV_ExtractFromUserinfo (host_client, true);

	if (*key != '_')
	{
		val = Info_ValueForKey(host_client->userinfo, key);

		SV_BroadcastUserinfoChange(host_client, SV_UserInfoIsBasic(key), key, val);
	}

	//doh't spam chat changes. they're not interesting, and just spammy.
	if (strcmp(key, "chat"))
		SV_LogPlayer(host_client, "userinfo changed");

	PR_ClientUserInfoChanged(key, oldval, Info_ValueForKey(host_client->userinfo, key));
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
	Info_Print (svs.info, "");
	SV_EndRedirect();
}

void SV_NoSnap_f(void)
{
	SV_LogPlayer(host_client, "refused snap");

	if (*host_client->uploadfn)
	{
		*host_client->uploadfn = 0;
		SV_BroadcastTPrintf (PRINT_HIGH, "%s refused remote screenshot\n", host_client->name);
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

	if (!votelevel.value || ((host_client->penalties & (BAN_MUTE|BAN_DEAF)) == (BAN_MUTE|BAN_DEAF)))
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, "Voting is dissallowed on this server\n");
		return;
	}
	if (!*command)
	{
		char cmds[900];
		Cmd_EnumerateLevel(votelevel.value, cmds, sizeof(cmds));
		SV_ClientTPrintf(host_client, PRINT_HIGH, "Allowed commands:\n%s\n", cmds);
		return;
	}
	if (host_client->penalties & BAN_MUTE)
	{
		//pretend to vote for it
		if (host_client->penalties & BAN_STEALTH)
			SV_ClientTPrintf(host_client, PRINT_HIGH, "%s casts a vote for '%s'\n", host_client->name, command);
		else
			SV_ClientTPrintf(host_client, PRINT_HIGH, "Sorry, you cannot vote when muted as it may allow you to send a message.\n");
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
		SV_ClientTPrintf(host_client, PRINT_HIGH, "You arn't allowed to vote for that\n");
		return;
	}
	num = Cmd_Level(command);
	if (base)
		*base = ' ';
	if (num != Cmd_ExecLevel)
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, "You arn't allowed to vote for that\n");
		return;
	}


	VoteCheckTimes();

	for (num = 0; num < sv.allocated_client_slots; num++)
		if (svs.clients[num].state == cs_spawned)
			totalusers++;

	if (VoteCount(command, id))
	{
		VoteRemoveCommands(command, id);
		SV_ClientTPrintf(host_client, PRINT_HIGH, "Old vote removed.\n");
		return;
	}
	if (VoteCount(NULL, id)>=3)
	{
		VoteRemoveCommands(NULL, id);
		SV_ClientTPrintf(host_client, PRINT_HIGH, "All votes removed.\n");
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
		SV_BroadcastTPrintf(PRINT_HIGH, "%s casts final vote for '%s'\n", host_client->name, command);

		VoteRemoveCommands(command, -1);
		Cbuf_AddText(command, votelevel.value);
		Cbuf_AddText("\n", votelevel.value);
		//Cmd_ExecuteString (command, votelevel.value);
		return;
	}
	else	//otherwise, try later.
	{
		SV_BroadcastTPrintf(PRINT_HIGH, "%s casts a vote for '%s'\n", host_client->name, command);

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

	if (svs.gametype != GT_PROGS)
		return;

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
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	if (svs.gametype != GT_PROGS)
		return;

	SV_LogPlayer(host_client, "god cheat");
	if ((int) (sv_player->v->flags = (int) sv_player->v->flags ^ FL_GODMODE) & FL_GODMODE)
		SV_ClientTPrintf (host_client, PRINT_HIGH, "godmode ON\n");
	else
		SV_ClientTPrintf (host_client, PRINT_HIGH, "godmode OFF\n");
}


void Cmd_Give_f (void)
{
#ifdef HLSERVER
	if (svs.gametype == GT_HALFLIFE)
	{
		HLSV_ClientCommand(host_client);
		return;
	}
#endif

	if (!SV_MayCheat())
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	if (!svprogfuncs)
		return;

	SV_LogPlayer(host_client, "give cheat");
#ifdef QUAKESTATS
	{
		char *t = Cmd_Argv(1);
		if (strlen(t) == 1 && (Cmd_Argc() == 3 || (*t>='0' && *t <= '9')))
		{
			int v = atoi (Cmd_Argv(2));
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
			default:
				SV_TPrintToClient(host_client, PRINT_HIGH, "give: unknown item\n");
			}
			return;
		}
	}
#endif
	if (svprogfuncs->EvaluateDebugString)
	{
		if (developer.value < 2 && host_client->netchan.remote_address.type != NA_LOOPBACK)	//we don't want clients doing nasty things... like setting movetype 3123
		{
			SV_TPrintToClient(host_client, PRINT_HIGH, "'give' debugging command requires developer 2 set on the server before you may use it\n");
		}
		else
		{
			int oldself;
			oldself = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			SV_ClientTPrintf(host_client, PRINT_HIGH, "Result: %s\n", svprogfuncs->EvaluateDebugString(svprogfuncs, Cmd_Args()));
			pr_global_struct->self = oldself;
		}
	}
}

void Cmd_Spiderpig_f(void)
{
	if (!SV_MayCheat())
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	if (!svprogfuncs)
		return;

	SV_LogPlayer(host_client, "spiderpig cheat");
	if (sv_player->v->movetype != MOVETYPE_WALLWALK)
	{
		sv_player->v->movetype = MOVETYPE_WALLWALK;
		sv_player->v->solid = SOLID_TRIGGER;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "Spider-Pig, Spider-Pig, does whatever a Spider-Pig does...\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		if (sv_player->v->health > 0)
			sv_player->v->solid = SOLID_SLIDEBOX;
		else
			sv_player->v->solid = SOLID_NOT;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "Spider-Pig, Spider-Pig!\n");
	}
}
void Cmd_Noclip_f (void)
{
#ifdef HLSERVER
	if (svs.gametype == GT_HALFLIFE)
	{
		HLSV_ClientCommand(host_client);
		return;
	}
#endif

	if (!SV_MayCheat())
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	if (!svprogfuncs)
		return;

	SV_LogPlayer(host_client, "noclip cheat");
	if (sv_player->v->movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v->movetype = MOVETYPE_NOCLIP;
		sv_player->v->solid = SOLID_TRIGGER;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "noclip ON\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		if (sv_player->v->health > 0)
			sv_player->v->solid = SOLID_SLIDEBOX;
		else
			sv_player->v->solid = SOLID_NOT;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "noclip OFF\n");
	}
}

void Cmd_6dof_f (void)
{
	if (!SV_MayCheat())
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	if (!svprogfuncs)
		return;

	SV_LogPlayer(host_client, "6dof cheat");
	if (sv_player->v->movetype != MOVETYPE_6DOF)
	{
		sv_player->v->movetype = MOVETYPE_6DOF;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "6dof mode ON\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		if (sv_player->v->health > 0)
			sv_player->v->solid = SOLID_SLIDEBOX;
		else
			sv_player->v->solid = SOLID_NOT;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "6dof mode OFF\n");
	}
}

void Cmd_Fly_f (void)
{
	if (!SV_MayCheat())
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	if (!svprogfuncs)
		return;

	SV_LogPlayer(host_client, "fly cheat");
	if (sv_player->v->movetype != MOVETYPE_FLY)
	{
		sv_player->v->movetype = MOVETYPE_FLY;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "flymode ON\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		if (sv_player->v->health > 0)
			sv_player->v->solid = SOLID_SLIDEBOX;
		else
			sv_player->v->solid = SOLID_NOT;
		SV_ClientTPrintf (host_client, PRINT_HIGH, "flymode OFF\n");
	}
}

#ifdef SUBSERVERS
void Cmd_SSV_Transfer_f(void)
{
	char *dest = Cmd_Argv(1);
	if (!SV_MayCheat())
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	SSV_InitiatePlayerTransfer(host_client, dest);
}

void Cmd_SSV_AllSay_f(void)
{
	char *text = Cmd_Args();
	if (!SV_MayCheat())
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	SSV_Send("", host_client->name, "say", text);
}

void Cmd_SSV_Join_f(void)
{
	int i;
	char *who = Cmd_Argv(1);
	if (!SV_MayCheat())
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (!strcmp(who, svs.clients[i].name))
		{
//			VectorCopy(svs.clients[i].edict->v->origin, sv_player->v->oldorigin);
			VectorCopy(svs.clients[i].edict->v->origin, sv_player->v->origin);
			World_LinkEdict (&sv.world, (wedict_t*)sv_player, false);

			sv_player->xv->dimension_hit	= (int)sv_player->xv->dimension_hit & ~128;
			sv_player->xv->dimension_solid	= (int)sv_player->xv->dimension_solid & 128;
			svs.clients[i].edict->xv->dimension_hit		= (int)svs.clients[i].edict->xv->dimension_hit & ~128;
			svs.clients[i].edict->xv->dimension_solid	= (int)svs.clients[i].edict->xv->dimension_solid & 128;
			return;
		}
	}
	SSV_Send(who, host_client->name, "join", "");
}
#endif

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
		SV_TPrintToClient(host_client, PRINT_HIGH, "Cheats are not allowed on this server\n");
		return;
	}

	if (!svprogfuncs)
		return;

	if (Cmd_Argc() != 4)
	{
		SV_ClientPrintf(host_client, PRINT_HIGH, "setpos %f %f %f\n", sv_player->v->origin[0], sv_player->v->origin[1], sv_player->v->origin[2]);
		return;
	}
	SV_LogPlayer(host_client, "setpos cheat");
	if (sv_player->v->movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v->movetype = MOVETYPE_NOCLIP;
		SV_TPrintToClient(host_client, PRINT_HIGH, "noclip on\n");
	}

	//make sure they're not going to whizz away from it
	VectorClear(sv_player->v->velocity);

	sv_player->v->origin[0] = atof(Cmd_Argv(1));
	sv_player->v->origin[1] = atof(Cmd_Argv(2));
	sv_player->v->origin[2] = atof(Cmd_Argv(3));
	World_LinkEdict (&sv.world, (wedict_t*)sv_player, false);
}

void SV_SetUpClientEdict (client_t *cl, edict_t *ent)
{
#ifdef VM_Q1
	if (svs.gametype == GT_Q1QVM)
	{
		string_t preserve;
		preserve = ent->v->netname;
		if (progstype != PROG_NQ)	//allow frikbots to work in NQ mods (but not qw!)
			ED_Clear(svprogfuncs, ent);
		ent->v->netname = preserve;
	}
	else
#endif
	{
		if (progstype != PROG_NQ)	//allow frikbots to work in NQ mods (but not qw!)
			ED_Clear(svprogfuncs, ent);
		svprogfuncs->SetStringField(svprogfuncs, ent, &ent->v->netname, cl->name, true);
	}
	ED_Spawned(ent, false);
	ent->ereftype = ER_ENTITY;

	ent->v->colormap = NUM_FOR_EDICT(svprogfuncs, ent);

#ifndef NOLEGACY
	{
		extern int pr_teamfield;
		if (pr_teamfield)
			((string_t *)ent->v)[pr_teamfield] = (string_t)(cl->team-svprogfuncs->stringtable);
	}

	{
		int tc = atoi(Info_ValueForKey(cl->userinfo, "topcolor"));
		int bc = atoi(Info_ValueForKey(cl->userinfo, "bottomcolor"));
		if (tc < 0 || tc > 13)
			tc = 0;
		if (bc < 0 || bc > 13)
			bc = 0;
		ent->xv->clientcolors = 16*tc + bc;
	}

	cl->dp_ping = NULL;
	cl->dp_pl = NULL;
	if (progstype == PROG_NQ)
	{
		cl->dp_ping = (float*)sv.world.progs->GetEdictFieldValue(sv.world.progs, ent, "ping", ev_float, NULL);
		cl->dp_pl = (float*)sv.world.progs->GetEdictFieldValue(sv.world.progs, ent, "ping_packetloss", ev_float, NULL);
	}
#endif


	ent->xv->gravity = cl->entgravity = 1.0;
	ent->xv->maxspeed = cl->maxspeed = sv_maxspeed.value;
	ent->v->movetype = MOVETYPE_NOCLIP;

	ent->v->frags = 0;
	cl->connection_started = realtime;
}

//dynamically add/remove a splitscreen client
static void Cmd_AddSeat_f(void)
{
	client_t *cl, *prev;
	qboolean changed = false;
	//don't allow an altseat to add. paranoia.
	if (host_client->controller)
		return;

	if (host_client->state != cs_spawned)
		return;

	if (!(host_client->fteprotocolextensions & PEXT_SPLITSCREEN))
		return;

	if (Cmd_Argc()>1)
	{
		int num = atoi(Cmd_Argv(1));
		int count;

		if (!num || host_client->joinobservelockeduntil > realtime)
			return;
		host_client->joinobservelockeduntil = realtime + 2;

		for (count = 1, prev = host_client, cl = host_client->controlled; cl; cl = cl->controlled)
		{
			if (count >= num)
			{
				for(; cl; cl = prev->controlled)
				{
					//unlink it
					prev->controlled = cl->controlled;
					cl->controller = NULL;
					cl->controlled = NULL;

					//make it into a pseudo-bot
					cl->netchan.remote_address.type = NA_INVALID;	//so the remaining client doesn't get the kick too.
					cl->protocol = SCP_BAD;	//make it a bit like a bot, so we don't try sending any datagrams/reliables at someone that isn't able to receive anything.

					//okay, it can get lost now.
					cl->drop = true;
				}
				host_client->controller->joinobservelockeduntil = realtime + 3;
				changed = true;
				break;
			}
			prev = cl;
			count++;
		}

		if (!changed && count <= num)
			changed = !!SV_AddSplit(host_client, Cmd_Argv(2), num);
	}
	else
	{
		cl = NULL;
/*		if (host_client->joinobservelockeduntil > realtime)
		{
			SV_TPrintToClient(host_client, PRINT_HIGH, va("Please wait %.1g more seconds\n", host_client->joinobservelockeduntil-realtime));
			return;
		}
		host_client->joinobservelockeduntil = realtime + 2;

		cl = SV_AddSplit(host_client, host_client->userinfo, 0);
*/
	}

	if (cl || changed)
		SV_UpdateSeats(host_client);
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
	int seats;

	if (host_client->controller)
	{
		host_client = host_client->controller;
		sv_player = host_client->edict;
	}

	if (host_client->state != cs_spawned)
		return;

	if (svs.gametype != GT_PROGS && svs.gametype != GT_Q1QVM)
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Sorry, not implemented in this gamecode type. Try moaning at the dev team\n");
		return;
	}

	if (!ISNQCLIENT(host_client) && !(host_client->zquake_extensions & Z_EXT_JOIN_OBSERVE))
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Your game client doesn't support this command\n");
		return;
	}

	if (!host_client->spectator)
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, "You are not currently spectating.\n");
		return;
	}
	if (host_client->joinobservelockeduntil > realtime)
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, va("Please wait %.1g more seconds\n", host_client->joinobservelockeduntil-realtime));
		return;
	}
	host_client->joinobservelockeduntil = realtime + 2;

	if (password.string[0] && stricmp(password.string, "none"))
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, "This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "player", "player");
		return;
	}

	if (host_client->penalties & BAN_SPECONLY)
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, "You are banned from joining the game.\n");
		return;
	}

	// count players already on server
	numclients = 0;
	seats = 0;
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		if (cl->state != cs_free && !cl->spectator)
			numclients++;
		if ((cl == host_client || cl->controller == host_client) && cl->spectator)
			seats++;
	}
	if (numclients+seats > maxclients.value)
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Can't join, all player slots full\n");
		return;
	}

	if (ISNQCLIENT(host_client))
	{	//make sure the nq client is viewing from its own player entity again
		ClientReliableWrite_Begin(host_client, svc_setview, 4);
		ClientReliableWrite_Entity(host_client, host_client - svs.clients + 1);
	}

	for (; host_client; host_client = host_client->controlled)
	{
		sv_player = host_client->edict;
		if (!host_client->spectator)
			continue;

		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_DropClient(host_client);
		else
#endif
			if (SpectatorDisconnect)
			PR_ExecuteProgram (svprogfuncs, SpectatorDisconnect);
		sv.spawned_observer_slots--;

		SV_SetUpClientEdict (host_client, host_client->edict);

		// turn the spectator into a player
		host_client->spectator = false;
		Info_RemoveKey (host_client->userinfo, "*spectator");

		// FIXME, bump the client's userid?

		// call the progs to get default spawn parms for the new client
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_SetNewParms();
		else
#endif
			if (pr_global_ptrs->SetNewParms)
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);

		SV_SpawnParmsToClient(host_client);

#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_ClientConnect(host_client);
		else
#endif
		{
			// call the spawn function
			pr_global_struct->time = sv.world.physicstime;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);

			// actually spawn the player
			pr_global_struct->time = sv.world.physicstime;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->PutClientInServer);
		}
		sv.spawned_client_slots++;

		// send notification to all clients
		host_client->old_frags = host_client->edict->v->frags;
		host_client->sendinfo = true;

		SV_LogPlayer(host_client, "joined");
	}
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
	int seats;

	if (host_client->controller)
	{
		host_client = host_client->controller;
		sv_player = host_client->edict;
	}

	if (host_client->state != cs_spawned)
		return;

	if (svs.gametype != GT_PROGS && svs.gametype != GT_Q1QVM)
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Sorry, not implemented in this gamecode type. Try moaning at the dev team\n");
		return;
	}

	if (!ISNQCLIENT(host_client) && !(host_client->zquake_extensions & Z_EXT_JOIN_OBSERVE))
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Your game client doesn't support this command\n");
		return;
	}

	if (host_client->spectator)
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, "You are already spectating.\n");
		return;
	}
	if (host_client->joinobservelockeduntil > realtime)
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, va("Please wait %.1g more seconds\n", host_client->joinobservelockeduntil-realtime));
		return;
	}
	host_client->joinobservelockeduntil = realtime + 2;

	if (spectator_password.string[0] && stricmp(spectator_password.string, "none"))
	{
		SV_ClientTPrintf(host_client, PRINT_HIGH, "This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "spectator", "spectator");
		return;
	}

	// count spectators already on server
	numspectators = 0;
	seats = 0;
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		if (cl->state != cs_free && cl->spectator)
			numspectators++;
		if ((cl == host_client || cl->controller == host_client) && !cl->spectator)
			seats++;
	}
	if (numspectators+seats > maxspectators.value)
	{
		SV_TPrintToClient(host_client, PRINT_HIGH, "Can't join, all spectator slots full\n");
		return;
	}

	for (; host_client; host_client = host_client->controlled)
	{
		sv_player = host_client->edict;
		if (host_client->spectator)
			continue;

		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_DropClient(host_client);
		else
#endif
		{
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);
		}
		sv.spawned_client_slots--;

		SV_SetUpClientEdict (host_client, host_client->edict);

		// turn the player into a spectator
		host_client->spectator = true;
		Info_SetValueForStarKey (host_client->userinfo, "*spectator", "1", sizeof(host_client->userinfo));

		// FIXME, bump the client's userid?

		// call the progs to get default spawn parms for the new client
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_SetNewParms();
		else
#endif
			if (pr_global_ptrs->SetNewParms)
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);

		SV_SpawnParmsToClient(host_client);
		SV_SpawnSpectator ();

		// call the spawn function
#ifdef VM_Q1
		if (svs.gametype == GT_Q1QVM)
			Q1QVM_ClientConnect(host_client);
		else
#endif
		{
			if (SpectatorConnect)
			{
				pr_global_struct->time = sv.world.physicstime;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
				PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
			}
			else
			{
				sv_player->v->movetype = MOVETYPE_NOCLIP;
				sv_player->v->model = 0;
				sv_player->v->modelindex = 0;
			}
		}
		sv.spawned_observer_slots++;

		// send notification to all clients
		host_client->old_frags = host_client->edict->v->frags;
		host_client->sendinfo = true;

		SV_LogPlayer(host_client, "observing");
	}
}

void SV_CalcNetRates(client_t *cl, double *ftime, int *frames, double *minf, double *maxf)
{
	int f;
	int fmsec;
	*minf = 1000;
	*maxf = 0;
	*ftime = 0;
	*frames = 0;

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
						fmsec = 1001;
					}
					else
					{
						fmsec = 1000.0f/cl->frameunion.frames[f].move_msecs;
					}
					*ftime += fmsec;
					if (*minf > fmsec)
						*minf = fmsec;
					if (*maxf < fmsec)
						*maxf = fmsec;
					*frames+=1;
				}
			}
		}
	}
}

static void Cmd_FPSList_f(void)
{
	client_t *cl;
	int c;
	double minf, maxf;
	double ftime;
	int frames;
	char *protoname;


	for (c = 0; c < sv.allocated_client_slots; c++)
	{
		cl = &svs.clients[c];
		if (!cl->state)
			continue;

		SV_CalcNetRates(cl, &ftime, &frames, &minf, &maxf);

		switch(cl->protocol)
		{
		case SCP_QUAKEWORLD: protoname = (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)?"fteqw":(cl->fteprotocolextensions||cl->fteprotocolextensions2?"qw":"qwid"); break;
		case SCP_QUAKE2: protoname = "q2"; break;
		case SCP_QUAKE3: protoname = "q3"; break;
		case SCP_NETQUAKE: protoname = (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)?"ftenq":"nq"; break;
		case SCP_BJP3: protoname = (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)?"ftenq":"bjp3"; break;
		case SCP_FITZ666: protoname = (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)?"ftenq":"fitz"; break;
		case SCP_DARKPLACES6: protoname = "dpp6"; break;
		case SCP_DARKPLACES7: protoname = "dpp7"; break;
		default: protoname = "?"; break;
		}

		if (frames)
			SV_ClientPrintf(host_client, PRINT_HIGH, "%s: %gfps (%g - %g), c2s: %ibps, s2c: %ibps, ping %ims(-%i), pl %i%% %s\n", cl->name, ftime/frames, minf, maxf, (int)cl->inrate, (int)cl->outrate, SV_CalcPing(cl, false), (int)(1000*cl->delay), cl->lossage, protoname);
		else
			SV_ClientPrintf(host_client, PRINT_HIGH, "%s: unknown framerate, c2s: %ibps, s2c: %ibps, ping %ims(-%i), pl %i%% %s\n", cl->name, (int)cl->inrate, (int)cl->outrate, SV_CalcPing(cl, false), (int)(1000*cl->delay), cl->lossage, protoname);
	}
}

void SV_EnableClientsCSQC(void)
{
	if (host_client->controller)
		return;

	host_client->csqcactive = true;
}
void SV_DisableClientsCSQC(void)
{
#ifdef PEXT_CSQC
	host_client->csqcactive = false;
#endif
}

void SV_UserCmdMVDList_f (void);
static void SV_STFU_f(void)
{
	char *msg;
	SV_ClientPrintf(host_client, 255, "stfu\n");
	msg = "cl_antilag 0\n";
	ClientReliableWrite_Begin(host_client, svc_stufftext, 2+strlen(msg));
	ClientReliableWrite_String(host_client, msg);
}

#ifdef NQPROT
static void SVNQ_Spawn_f (void)
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
		SV_FullClientUpdate(client, host_client);
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

	if (!ent)
	{
	}
	else if (host_client->istobeloaded)	//minimal setup
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
		svprogfuncs->SetStringField(svprogfuncs, ent, &ent->v->netname, host_client->name, true);

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

	if (pr_global_ptrs->total_secrets)
	{
		ClientReliableWrite_Begin (host_client, svcnq_updatestatlong, 6);
		ClientReliableWrite_Byte (host_client, STAT_TOTALSECRETS);
		ClientReliableWrite_Long (host_client, pr_global_struct->total_secrets);
	}
	if (pr_global_ptrs->total_monsters)
	{
		ClientReliableWrite_Begin (host_client, svcnq_updatestatlong, 6);
		ClientReliableWrite_Byte (host_client, STAT_TOTALMONSTERS);
		ClientReliableWrite_Long (host_client, pr_global_struct->total_monsters);
	}
	if (pr_global_ptrs->found_secrets)
	{
		ClientReliableWrite_Begin (host_client, svcnq_updatestatlong, 6);
		ClientReliableWrite_Byte (host_client, STAT_SECRETS);
		ClientReliableWrite_Long (host_client, pr_global_struct->found_secrets);
	}
	if (pr_global_ptrs->killed_monsters)
	{
		ClientReliableWrite_Begin (host_client, svcnq_updatestatlong, 6);
		ClientReliableWrite_Byte (host_client, STAT_MONSTERS);
		ClientReliableWrite_Long (host_client, pr_global_struct->killed_monsters);
	}

	//SV_Begin_Core(host_client);

	ClientReliableWrite_Begin (host_client, svc_signonnum, 2);
	ClientReliableWrite_Byte (host_client, 3);

	host_client->send_message = true;
}
static void SVNQ_Begin_f (void)
{
	unsigned pmodel = 0, emodel = 0;
	qboolean sendangles=false;

	if (host_client->state == cs_spawned)
		return; // don't begin again

	host_client->state = cs_spawned;

	SV_Begin_Core(host_client);

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
			SV_BroadcastTPrintf (PRINT_HIGH, "warning: %s eyes or player model not verified\n", host_client->name);
	}


	// if we are paused, tell the client
	if (sv.paused)
	{
		if (!ISQ2CLIENT(host_client))
		{
			ClientReliableWrite_Begin (host_client, svc_setpause, 2);
			ClientReliableWrite_Byte (host_client, sv.paused!=0);
		}
		if (sv.paused&~PAUSE_AUTO)
			SV_ClientTPrintf(host_client, PRINT_HIGH, "server is paused\n");
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

//	MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
//	MSG_WriteByte (&host_client->netchan.message, 4);


	host_client->send_message = true;


	SV_PreRunCmd();
	host_client->lastcmd.msec = 0;
	SV_RunCmd (&host_client->lastcmd, false);
	SV_PostRunCmd();
}
static void SVNQ_PreSpawn_f (void)
{
	if (host_client->prespawn_stage < PRESPAWN_MAPCHECK)
		SV_StuffcmdToClient(host_client, va("cmd prespawn %s\n", Cmd_Args()));
	else if (host_client->prespawn_stage == PRESPAWN_MAPCHECK)
	{
		host_client->checksum = ~0u;
		host_client->prespawn_stage = PRESPAWN_MAPCHECK+1;
		host_client->prespawn_idx = 0;

		if (sv_mapcheck.value)
		{
			const char *prot = "";
			switch(host_client->protocol)
			{
			default:
				break;
			case SCP_NETQUAKE:
				prot = " (nq)";
				break;
			case SCP_BJP3:
				prot = " (bjp3)";
				break;
			case SCP_FITZ666:
				prot = " (fitz)";
				break;
			case SCP_DARKPLACES6:
				prot = " (dpp6)";
				break;
			case SCP_DARKPLACES7:
				prot = " (dpp7)";
				break;
			}
			Con_DPrintf("Warning: %s cannot be enforced on player %s%s.\n", sv_mapcheck.name, host_client->name, prot);	//as you can fake it in a client anyway, this is hardly a significant issue.
		}
	}

	host_client->send_message = true;
}
static void SVNQ_NQInfo_f (void)
{
	Cmd_TokenizeString(va("setinfo \"%s\" \"%s\"\n", Cmd_Argv(0), Cmd_Argv(1)), false, false);
	SV_SetInfo_f();
}

static void SVNQ_NQColour_f (void)
{
	char *val;
	int top;
	int bottom;

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

	if (progstype != PROG_QW && host_client->edict)
		host_client->edict->v->team = bottom + 1;

	val = va("%i", top);
	if (strcmp(val, Info_ValueForKey(host_client->userinfo, "topcolor")))
	{
		Info_SetValueForKey(host_client->userinfo, "topcolor", val, sizeof(host_client->userinfo));
		SV_BroadcastUserinfoChange(host_client, true, "topcolor", NULL);
	}

	val = va("%i", bottom);
	if (strcmp(val, Info_ValueForKey(host_client->userinfo, "bottomcolor")))
	{
		Info_SetValueForKey(host_client->userinfo, "bottomcolor", val, sizeof(host_client->userinfo));
		SV_BroadcastUserinfoChange(host_client, true, "bottomcolor", NULL);
	}

	switch(bottom)
	{
	case 4:
		val = "red";
		break;
	case 14:
		val = "blue";
		break;
	default:
		val = va("t%i", bottom+1);
		break;
	}
	if (strcmp(val, Info_ValueForKey(host_client->userinfo, "team")))
	{
		Info_SetValueForKey(host_client->userinfo, "team", val, sizeof(host_client->userinfo));
		SV_BroadcastUserinfoChange(host_client, true, "team", NULL);
	}

	SV_ExtractFromUserinfo (host_client, true);
}

static void SVNQ_DPModel_f (void)
{
	Cmd_TokenizeString(va("setinfo model \"%s\"\n", Cmd_Argv(1)), false, false);
	SV_SetInfo_f();
}
static void SVNQ_DPSkin_f (void)
{
	Cmd_TokenizeString(va("setinfo skin \"%s\"\n", Cmd_Argv(1)), false, false);
	SV_SetInfo_f();
}

static void SVNQ_Ping_f(void)
{
	int i;
	client_t *cl;

	//don't translate this, most advanced clients (including us) automate and parse them, the results being visible in the scoreboard and NOT the console.
	//translating these prints can thus confuse things greatly.
	SV_PrintToClient(host_client, PRINT_HIGH, "Client ping times:\n");
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		if (!cl->state)
			continue;

		SV_PrintToClient(host_client, PRINT_HIGH, va("%3i %s\n", SV_CalcPing (cl, false), cl->name));
	}
}
static void SVNQ_Status_f(void)
{	//note: numerous NQ clients poll for this...
	//so try to ensure that we adhere to various rules...
	//we have a different function for server operators to use which contains more info.
	int i;
	client_t *cl;
	int count;
	extern cvar_t maxclients, maxspectators;

	/*
	int nummodels, numsounds;
	for (nummodels = 1; nummodels < MAX_PRECACHE_MODELS; nummodels++)
		if (!sv.strings.model_precache[nummodels])
			break;
	for (numsounds = 1; numsounds < MAX_PRECACHE_SOUNDS; numsounds++)
		if (!sv.strings.sound_precache[numsounds])
			break;*/

	SV_PrintToClient(host_client, PRINT_HIGH, va("host:    %s\n", hostname.string));	//must be first, with same first 9 chars
	SV_PrintToClient(host_client, PRINT_HIGH, va("version: %s\n", version_string()));
//	SV_PrintToClient(host_client, PRINT_HIGH, va("IPv4:     \n", ));
//	SV_PrintToClient(host_client, PRINT_HIGH, va("IPv6:     \n", ));
	SV_PrintToClient(host_client, PRINT_HIGH, va("map:     %s\n", svs.name));
/*	for (count = 1; count < MAX_PRECACHE_MODELS; count++)
		if (!sv.strings.model_precache[count])
			break;
	SV_PrintToClient(host_client, PRINT_HIGH, va("models:  %i/%i\n", count-1, MAX_PRECACHE_MODELS-1));*/
/*	for (count = 1; count < MAX_PRECACHE_SOUNDS; count++)
		if (!sv.strings.sound_precache[count])
			break;
	SV_PrintToClient(host_client, PRINT_HIGH, va("sounds:  %i/%i\n", count-1, MAX_PRECACHE_SOUNDS-1));*/
//	SV_PrintToClient(host_client, PRINT_HIGH, va("entities:%i/%i\n", sv.world.num_edicts, sv.world.max_edicts));
	for (count=0,i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		if (cl->state)
			count++;
	}
	SV_PrintToClient(host_client, PRINT_HIGH, va("players: %i active (%i max)\n\n", count, min(maxclients.ival+maxspectators.ival,sv.allocated_client_slots)));//must be last
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++)
	{
		int hours, mins, secs;
		if (!cl->state)
			continue;
		secs = realtime - cl->connection_started;
		mins = secs/60;
		secs -= mins*60;
		hours = mins/60;
		mins -= hours*60;

		SV_PrintToClient(host_client, PRINT_HIGH, va("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", i+1, cl->name, cl->old_frags, hours, mins, secs));
		SV_PrintToClient(host_client, PRINT_HIGH, va("   %s\n", SV_PlayerPublicAddress(cl)));
	}
}

static void SVNQ_Protocols_f(void)
{
	int i;
	host_client->supportedprotocols = 0;
	for (i = 1; i < Cmd_Argc(); )
	{
		switch(strtoul(Cmd_Argv(i++), NULL, 0))
		{
		case PROTOCOL_VERSION_BJP3:
			host_client->supportedprotocols |= 1u<<SCP_BJP3;
			break;
		case PROTOCOL_VERSION_FITZ:
			host_client->supportedprotocols |= 1u<<SCP_FITZ666;
			break;
		case PROTOCOL_VERSION_RMQ:
			host_client->supportedprotocols |= 1u<<SCP_FITZ666;
			break;
		case PROTOCOL_VERSION_DP6:
			host_client->supportedprotocols |= 1u<<SCP_DARKPLACES6;
			break;
		case PROTOCOL_VERSION_DP7:
			host_client->supportedprotocols |= 1u<<SCP_DARKPLACES7;
			break;
		default:	//don't know what you are. at least don't bug out.
			host_client->supportedprotocols |= 1u<<SCP_NETQUAKE;
			break;
		}
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

	host_client->fteprotocolextensions = 0;
	host_client->fteprotocolextensions2 = 0;
	for (i = 1; i < Cmd_Argc(); )
	{
		tag = Cmd_Argv(i++);
		val = Cmd_Argv(i++);
		switch(strtoul(tag, NULL, 0))
		{
		case PROTOCOL_VERSION_FTE:
			host_client->fteprotocolextensions = strtoul(val, NULL, 0) & Net_PextMask(1, ISNQCLIENT(host_client));
			break;
		case PROTOCOL_VERSION_FTE2:
			host_client->fteprotocolextensions2 = strtoul(val, NULL, 0) & Net_PextMask(2, ISNQCLIENT(host_client));
			break;
		}
	}

	Con_DPrintf("%s now using pext: %x, %x\n", host_client->name, host_client->fteprotocolextensions, host_client->fteprotocolextensions2);

	if (!host_client->supportedprotocols && Cmd_Argc() == 1)
		Con_DPrintf("%s has a shitty client.\n", host_client->name);

	SV_ClientProtocolExtensionsChanged(host_client);

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
	{"modellist", SVQW_Modellist_f, true},
	{"soundlist", SVQW_Soundlist_f, true},
	{"prespawn", SVQW_PreSpawn_f, true},
	{"spawn", SVQW_Spawn_f, true},
	{"begin", SV_Begin_f, true},

	/*ezquake warning*/
	{"al", SV_STFU_f, true},	//can probably be removed now.

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
	{"demolist", SV_UserCmdMVDList_f},
	{"dlist", SV_UserCmdMVDList_f},	//apparently people are too lazy to type.
	{"demoinfo", SV_MVDInfo_f},
	{"dl", SV_DemoDownload_f},

	{"stopdownload", SV_StopDownload_f},
	{"dlsize", SV_DownloadSize_f},
	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f, true},

	/*quakeworld specific things*/
	{"addseat", Cmd_AddSeat_f},	//splitscreen
	{"join", Cmd_Join_f},
	{"observe", Cmd_Observe_f},
	{"ptrack", SV_PTrack_f}, //ZOID - used with autocam
	{"snap", SV_NoSnap_f},	//cheat detection

	{"enablecsqc", SV_EnableClientsCSQC, true},
	{"disablecsqc", SV_DisableClientsCSQC, true},

	{"vote", SV_Vote_f},

#ifdef SVRANKING
	{"topten", Rank_ListTop10_f},
#endif

	{"efpslist", Cmd_FPSList_f},	//don't conflict with the ktpro one
	{"god", Cmd_God_f},
	{"give", Cmd_Give_f},
	{"noclip", Cmd_Noclip_f},
	{"spiderpig", Cmd_Spiderpig_f},
	{"6dof", Cmd_6dof_f},
	{"fly", Cmd_Fly_f},
	{"notarget", Cmd_Notarget_f},
	{"setpos", Cmd_SetPos_f},
#ifdef SUBSERVERS
	{"ssvtransfer", Cmd_SSV_Transfer_f},//transfer the player to a different map/server
	{"ssvsay",		Cmd_SSV_AllSay_f},	//transfer the player to a different map/server
	{"ssvjoin",		Cmd_SSV_Join_f},	//transfer the player to a different map/server
#endif

#ifdef NQPROT
	{"name",		SVNQ_NQInfo_f},
#endif

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

	{"serverinfo", SV_ShowServerinfo_f, true},
	{"info", SV_ShowServerinfo_f, true},

	{"download", SV_BeginDownload_f, true},
	{"nextdl", SV_NextDownload_f, true},

	{"nextserver", SVQ2_NextServer_f, true},

	//fte stuff
	{"setinfo", SV_SetInfo_f, true},
	{"ftevote", SV_Vote_f, true},	//voting... kinda messed up by 'vote' being common in mods
	{"addseat", Cmd_AddSeat_f, true},		//for splitscreen

//#ifdef SVRANKING
//	{"topten", Rank_ListTop10_f, true},
//#endif

	//quakeworld uses 'drop', quake2 commonly uses that to chuck items away / to a friend.
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

	{"status",		SVNQ_Status_f},


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
	{"playermodel",	SVNQ_DPModel_f},
	{"playerskin",	SVNQ_DPSkin_f},
	{"rate",		SV_Rate_f},
	{"rate_burstsize",	NULL},

#ifdef SVRANKING
	{"topten",		Rank_ListTop10_f},
#endif

	/*various misc extensions*/
	{"protocols",	SVNQ_Protocols_f, true},
	{"pext",		SV_Pext_f, true},
	{"enablecsqc",	SV_EnableClientsCSQC},
	{"disablecsqc",	SV_DisableClientsCSQC},
	{"challengeconnect", NULL},

	/*spectating, this should be fun...*/
	{"join", Cmd_Join_f},
	{"observe", Cmd_Observe_f},
	{"ptrack", SV_PTrack_f}, //ZOID - used with autocam

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
void SV_ExecuteUserCommand (const char *s, qboolean fromQC)
{
	ucmd_t	*u;
	client_t *oldhost = host_client;

	if (host_client->state < cs_connected)
		return;

	Con_DLPrintf((host_client->netchan.remote_address.type==NA_LOOPBACK)?2:1, "Client command: %s\n", s);

	Cmd_TokenizeString (s, false, false);
	sv_player = host_client->edict;

	Cmd_ExecLevel=1;

	if (!fromQC && host_client->controlled)	//now see if it's meant to be from a slave client
	{	//'cmd 2 say hi' should 
		char *a=Cmd_Argv(0), *e;
		int pnum = strtoul(a, &e, 10);
		
		if (e == a+1 && pnum >= 1 && pnum <= MAX_SPLITS)
		{
			client_t *sp;
			for (sp = host_client; sp; sp = sp->controlled)
			{
				if (!--pnum)
				{
					host_client = sp;
					break;
				}
			}

			sv_player = host_client->edict;
			s = Cmd_Args();
			Cmd_ShiftArgs(1, false);
		}
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
			char adr[MAX_ADR_SIZE];
			char remaining[1024];
			int i;
			rankstats_t stats;

			if (!Rank_GetPlayerStats(host_client->rankid, &stats))
			{
				host_client = oldhost;
				return;
			}

			Log_String(LOG_RCON, va("cmd from %s - %s:\n%s\n"
				, NET_AdrToString (adr, sizeof(adr), &net_from), host_client->name, s));

			Con_TPrintf ("cmd from %s:\n%s\n"
				, host_client->name, s);

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
						, NET_AdrToString (adr, sizeof(adr), &net_from), "Was too long - possible buffer overflow attempt");
					return;
				}
				Q_strncatz(remaining, Cmd_Argv(i), sizeof(remaining));
				Q_strncatz(remaining, " ", sizeof(remaining));
			}

			Cmd_ExecuteString (remaining, stats.trustlevel);
			host_client = oldhost;
			SV_EndRedirect ();
			return;
		}
#endif
		Con_TPrintf ("Bad user command: %s\n", Cmd_Argv(0));
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

	s = COM_LoadStackFile("impfiltr.cfg", buffer, sizeof(buffer), NULL);
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

static qboolean AddEntityToPmove(world_t *w, wedict_t *player, wedict_t *check)
{
	physent_t	*pe;
	int			solid = check->v->solid;
	int			q1contents;

	if (pmove.numphysent == MAX_PHYSENTS)
		return false;
	pe = &pmove.physents[pmove.numphysent];
	pe->notouch = !((int)player->xv->dimension_solid & (int)check->xv->dimension_hit);
	if (!((int)player->xv->dimension_hit & (int)check->xv->dimension_solid))
		return true;
	if (!check->v->size[0])	//points are not meant to be solid
		return true;
	pmove.numphysent++;

	VectorCopy (check->v->origin, pe->origin);
	pe->info = NUM_FOR_EDICT(w->progs, check);
	pe->nonsolid = solid == SOLID_TRIGGER;
	pe->isportal = solid == SOLID_PORTAL;
	q1contents = (int)check->v->skin;
	if (solid == SOLID_LADDER)
		q1contents = Q1CONTENTS_LADDER;	//legacy crap
	switch(q1contents)
	{
	case Q1CONTENTS_SOLID:
		pe->nonsolid = false;
		pe->forcecontentsmask = FTECONTENTS_SOLID;
		break;
	case Q1CONTENTS_WATER:
		pe->nonsolid = true;
		pe->forcecontentsmask = FTECONTENTS_WATER;
		break;
	case Q1CONTENTS_LAVA:
		pe->nonsolid = true;
		pe->forcecontentsmask = FTECONTENTS_LAVA;
		break;
	case Q1CONTENTS_SLIME:
		pe->nonsolid = true;
		pe->forcecontentsmask = FTECONTENTS_SLIME;
		break;
	case Q1CONTENTS_SKY:
		pe->nonsolid = true;
		pe->forcecontentsmask = FTECONTENTS_SKY;
		break;
	case Q1CONTENTS_LADDER:
		pe->nonsolid = true;
		pe->forcecontentsmask = FTECONTENTS_LADDER;
		break;
	default:
		pe->forcecontentsmask = 0;
		break;
	}
	if (solid == SOLID_PORTAL || solid == SOLID_BSP)
	{
		if(progstype != PROG_H2)
			pe->angles[0]*=r_meshpitch.value;	//quake is wierd. I guess someone fixed it hexen2... or my code is buggy or something...
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
	return true;
}

#if 1
#ifdef USEAREAGRID
extern size_t areagridsequence;
static void AddLinksToPmove (world_t *w, wedict_t *player, areagridlink_t *node)
{
	int Q1_HullPointContents (hull_t *hull, int num, vec3_t p);
	link_t		*l, *next;
	wedict_t		*check;
	int			pl;
	int			i;
	int			solid;

	pl = EDICT_TO_PROG(w->progs, player);

	// touch linked edicts
	for (l = node->l.next ; l != &node->l ; l = next)
	{
		next = l->next;
		check = ((areagridlink_t*)l)->ed;

		if (check->gridareasequence == areagridsequence)
			continue;
		check->gridareasequence = areagridsequence;

		if (check->v->owner == pl)
			continue;		// player's own missile
		if (check == player)
			continue;
		solid = check->v->solid;
		if (
			(solid == SOLID_TRIGGER && check->v->skin < 0)
			|| solid == SOLID_BSP
			|| solid == SOLID_PORTAL
			|| solid == SOLID_BBOX
			|| solid == SOLID_SLIDEBOX
			|| solid == SOLID_LADDER
			//|| (solid == SOLID_PHASEH2 && progstype == PROG_H2) //logically matches hexen2, but I hate it
			)
		{

			for (i=0 ; i<3 ; i++)
				if (check->v->absmin[i] > pmove_maxs[i]
				|| check->v->absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;

			if (!AddEntityToPmove(w, player, check))
				break;
		}
	}
}

//ignores mins/maxs.
//portals are expected to be weird. player movement code is nasty.
static void AddPortalsToPmove (world_t *w, wedict_t *player, areagridlink_t *node)
{
	link_t		*l, *next;
	wedict_t	*check;
	int			pl;
//	int			i;
	int			solid;

	pl = EDICT_TO_PROG(w->progs, player);

	// touch linked edicts
	for (l = node->l.next ; l != &node->l ; l = next)
	{
		next = l->next;
		check = ((areagridlink_t*)l)->ed;

		if (check->gridareasequence == areagridsequence)
			continue;
		check->gridareasequence = areagridsequence;

		if (check->v->owner == pl)
			continue;		// player's own missile
		if (check == player)
			continue;
		solid = check->v->solid;
		if (
			(solid == SOLID_TRIGGER && check->v->skin < 0)
			|| solid == SOLID_BSP
			|| solid == SOLID_PORTAL
			|| solid == SOLID_BBOX
			|| solid == SOLID_SLIDEBOX
			|| solid == SOLID_LADDER
			//|| (solid == SOLID_PHASEH2 && progstype == PROG_H2) //logically matches hexen2, but I hate it
			)
		{
			if (!AddEntityToPmove(w, player, check))
				break;
		}
	}
}
void AddAllLinksToPmove (world_t *w, wedict_t *player)
{
	int ming[2], maxg[2], g[2];
	CALCAREAGRIDBOUNDS(w, pmove_mins, pmove_maxs);

	areagridsequence++;

	AddLinksToPmove(w, player, &w->jumboarea);
	for (g[0] = ming[0]; g[0] < maxg[0]; g[0]++)
		for (g[1] = ming[1]; g[1] < maxg[1]; g[1]++)
			AddLinksToPmove(w, player, &w->gridareas[g[0] + g[1]*w->gridsize[0]]);

	AddPortalsToPmove(w, player, &w->portallist);
}
#else
/*
====================
AddLinksToPmove

====================
*/
void AddLinksToPmove (world_t *w, wedict_t *player, areanode_t *node)
{
	int Q1_HullPointContents (hull_t *hull, int num, vec3_t p);
	link_t		*l, *next;
	wedict_t		*check;
	int			pl;
	int			i;
	int			solid;

	pl = EDICT_TO_PROG(w->progs, player);

	// touch linked edicts
	for (l = node->edicts.next ; l != &node->edicts ; l = next)
	{
		next = l->next;
		check = (wedict_t*)EDICT_FROM_AREA(l);

		if (check->v->owner == pl)
			continue;		// player's own missile
		if (check == player)
			continue;
		solid = check->v->solid;
		if (
			(solid == SOLID_TRIGGER && check->v->skin < 0)
			|| solid == SOLID_BSP
			|| solid == SOLID_PORTAL
			|| solid == SOLID_BBOX
			|| solid == SOLID_SLIDEBOX
			|| solid == SOLID_LADDER
			//|| (solid == SOLID_PHASEH2 && progstype == PROG_H2) //logically matches hexen2, but I hate it
			)
		{

			for (i=0 ; i<3 ; i++)
				if (check->v->absmin[i] > pmove_maxs[i]
				|| check->v->absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;

			if (!AddEntityToPmove(w, player, check))
				break;
		}
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if (pmove_maxs[node->axis] > node->dist)
		AddLinksToPmove (w, player, node->children[0]);
	if (pmove_mins[node->axis] < node->dist)
		AddLinksToPmove (w, player, node->children[1]);
}

//ignores mins/maxs.
//portals are expected to be weird. player movement code is nasty.
void AddLinksToPmove_Force (world_t *w, wedict_t *player, areanode_t *node)
{
	link_t		*l, *next;
	wedict_t		*check;
	int			pl;
//	int			i;
	int			solid;

	pl = EDICT_TO_PROG(w->progs, player);

	// touch linked edicts
	for (l = node->edicts.next ; l != &node->edicts ; l = next)
	{
		next = l->next;
		check = (wedict_t*)EDICT_FROM_AREA(l);

		if (check->v->owner == pl)
			continue;		// player's own missile
		if (check == player)
			continue;
		solid = check->v->solid;
		if (
			(solid == SOLID_TRIGGER && check->v->skin < 0)
			|| solid == SOLID_BSP
			|| solid == SOLID_PORTAL
			|| solid == SOLID_BBOX
			|| solid == SOLID_SLIDEBOX
			|| solid == SOLID_LADDER
			//|| (solid == SOLID_PHASEH2 && progstype == PROG_H2) //logically matches hexen2, but I hate it
			)
		{
			if (!AddEntityToPmove(w, player, check))
				break;
		}
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if (pmove_maxs[node->axis] > node->dist)
		AddLinksToPmove_Force (w, player, node->children[0]);
	if (pmove_mins[node->axis] < node->dist)
		AddLinksToPmove_Force (w, player, node->children[1]);
}
#endif

#else
/*
================
AddAllEntsToPmove

For debugging
================
*/
void AddAllEntsToPmove (wedict_t *player, world_t *w)
{
	int			e;
	wedict_t		*check;
	int			i;
	int			pl;

	pl = EDICT_TO_PROG(w->progs, player);
	for (e=1 ; e<w->num_edicts ; e++)
	{
		check = EDICT_NUM(w->progs, e);
		if (ED_ISFREE(check))
			continue;
		if (check->v->owner == pl)
			continue;
		if (check->v->solid == SOLID_BSP
			|| check->v->solid == SOLID_BBOX
			|| check->v->solid == SOLID_SLIDEBOX)
		{
			if (check == player)
				continue;

			for (i=0 ; i<3 ; i++)
				if (check->v->absmin[i] > pmove_maxs[i]
				|| check->v->absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;

			if (!AddEntityToPmove(player, check))
				break;
		}
	}
}
#endif

int SV_PMTypeForClient (client_t *cl, edict_t *ent)
{
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demostatevalid)
	{	//force noclip... This does create problems for closing demos.
		if (cl->zquake_extensions & Z_EXT_PM_TYPE_NEW)
			return PM_SPECTATOR;
		return PM_OLD_SPECTATOR;
	}
#endif

#ifndef NOLEGACY
	if (sv_brokenmovetypes.value)	//this is to mimic standard qw servers, which don't support movetypes other than MOVETYPE_FLY.
	{								//it prevents bugs from being visible in unsuspecting mods.
		if (cl && cl->spectator)
		{
			if ((cl->zquake_extensions & Z_EXT_PM_TYPE_NEW) || (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
				return PM_SPECTATOR;
			return PM_OLD_SPECTATOR;
		}

		if (ent->v->health <= 0)
			return PM_DEAD;
		return PM_NORMAL;
	}
#endif

	switch((int)ent->v->movetype)
	{
	case MOVETYPE_NOCLIP:
		/*older/vanilla clients have a b0rked spectator mode that we don't want to break*/
		if (cl)
			if ((cl->zquake_extensions & Z_EXT_PM_TYPE_NEW) || (cl->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
				return PM_SPECTATOR;
		return PM_OLD_SPECTATOR;

	case MOVETYPE_WALLWALK:
		return PM_WALLWALK;
	
	case MOVETYPE_6DOF:
		return PM_6DOF;

	case MOVETYPE_FLY_WORLDONLY:
	case MOVETYPE_FLY:
		return PM_FLY;

	case MOVETYPE_NONE:
		return PM_NONE;

#ifdef NOLEGACY
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		return PM_DEAD;
#endif

	case MOVETYPE_WALK:
	default:
#ifndef NOLEGACY
		if (cl && ent->v->health <= 0)
			return PM_DEAD;
#endif
		return PM_NORMAL;
	}
}


/*
===========
SV_PreRunCmd
===========
Done before running a player command.  Clears the touch array
*/
qbyte *playertouch;
size_t playertouchmax;

void SV_PreRunCmd(void)
{
	size_t max = MAX_EDICTS;//(sv.world.num_edicts+7)&~7;
	if (max > playertouchmax)
	{
		playertouchmax = max;
		BZ_Free(playertouch);
		playertouch = BZ_Malloc((playertouchmax>>3)+1);
	}
	memset(playertouch, 0, playertouchmax>>3);
}
void SV_RunCmdCleanup(void)
{
	BZ_Free(playertouch);
	playertouch = NULL;
	playertouchmax = 0;
}

void Sh_CalcPointLight(vec3_t point, vec3_t light);

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
	qboolean jumpable;
	vec3_t new_vel;
	vec3_t old_vel;

	// To prevent a infinite loop
	if (!recurse)
	{
#ifdef NEWSPEEDCHEATPROT
		if (ucmd->msec && host_client->msecs > 500)
			host_client->msecs = 500;
		if (ucmd->msec > host_client->msecs)
		{	//they're over their timeslice allocation
			//if they're not taking the piss then be prepared to truncate the frame. this should hide clockskew without allowing full-on speedcheats.
			if (ucmd->msec > 10)
				ucmd->msec -= 1;
			if (ucmd->msec > host_client->msecs)
				return;
			ucmd->msec = host_client->msecs;
		}
		host_client->msecs -= ucmd->msec;
#else
		// DMW copied this KK hack copied from QuakeForge anti-cheat
		// (also extra inside parm on all SV_RunCmds that follow)

		//FIXME: update protocol to use server's timestamps instead of msecs over the wire, obsoleting speed cheat checks (by allowing the server to clamp sanely).

		if (!host_client->last_check)
		{
			host_client->msecs = 0;
			host_client->last_check = realtime;
		}
		host_client->msecs += ucmd->msec;

		if ((tmp_time = realtime - host_client->last_check) >= sv_cheatspeedchecktime.value)
		{
			extern int	isPlugin;
			double  tmp_time;
			tmp_time = tmp_time * 1000.0 * sv_cheatpc.value/100.0;
			if (host_client->msecs > tmp_time &&
				isPlugin < 2)	//qc-debugging can result in WEIRD timings, so don't warn about weird timings if we're likely to get blocked out for long periods
			{
				host_client->msec_cheating++;
				SV_BroadcastTPrintf(PRINT_HIGH,
						"Speed cheat possibility, analyzing:\n  %d>%.1f %d for: %s\n",
							host_client->msecs, tmp_time,
							host_client->msec_cheating, host_client->name);

				if (host_client->msec_cheating >= 2)
				{
					char adr[MAX_ADR_SIZE];
					SV_BroadcastTPrintf(PRINT_HIGH,
							"%s was kicked for speedcheating (%s)\n",
								host_client->name, NET_AdrToString(adr, sizeof(adr), &host_client->netchan.remote_address));
					host_client->drop = true;	//drop later
				}
			}

			host_client->msecs = 0;
			host_client->last_check = realtime;
		}
		// end KK hack copied from QuakeForge anti-cheat
		//it's amazing how code get's copied around...
#endif
	}

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
		pmove.capsule = (sv_player->xv->geomtype == GEOMTYPE_CAPSULE);

		movevars.entgravity = 0;
		movevars.maxspeed = 0;
		movevars.bunnyspeedcap = pm_bunnyspeedcap.value;
		movevars.ktjump = pm_ktjump.value;
		movevars.slidefix = (pm_slidefix.value != 0);
		movevars.airstep = (pm_airstep.value != 0);
		movevars.stepdown = (pm_stepdown.value != 0);
		movevars.walljump = (pm_walljump.value);
		movevars.slidyslopes = (pm_slidyslopes.value!=0);
		movevars.watersinkspeed = *pm_watersinkspeed.string?pm_watersinkspeed.value:60;
		movevars.flyfriction = *pm_flyfriction.string?pm_flyfriction.value:4;

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

#ifdef HEXEN2
	if (progstype == PROG_H2)
	{
		//fixme: should probably support rtlights, but this is a server, so urgh.
		if (sv.world.worldmodel && sv.world.worldmodel->funcs.LightPointValues)
		{
			vec3_t diffuse, ambient, dir;
			float lev = 0;
#if defined(RTLIGHTS) && !defined(SERVERONLY)
			Sh_CalcPointLight(sv_player->v->origin, ambient);
			lev += VectorLength(ambient);

			if (!r_shadow_realtime_world.ival || r_shadow_realtime_world_lightmaps.value)
#endif
			{
				sv.world.worldmodel->funcs.LightPointValues(sv.world.worldmodel, sv_player->v->origin, ambient, diffuse, dir);
				lev += (VectorLength(ambient) + VectorLength(diffuse)/2.0)/256;
			}
			sv_player->xv->light_level = lev * 255;
		}
		else	
			sv_player->xv->light_level = 128;	//don't know, some dummy value.
	}
#endif

	sv_player->v->button0 = ucmd->buttons & 1;
	sv_player->v->button2 = (ucmd->buttons >> 1) & 1;
	if (pr_allowbutton1.ival && progstype == PROG_QW)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
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

	if (host_client->penalties & BAN_CUFF)
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
	if (sv_player->v->health > 0 && sv_player->v->movetype)
	{
		if (sv_player->v->movetype == MOVETYPE_6DOF)
		{
			sv_player->v->angles[PITCH] = r_meshpitch.value * sv_player->v->v_angle[PITCH];
			sv_player->v->angles[YAW] = sv_player->v->v_angle[YAW];
			sv_player->v->angles[ROLL] = sv_player->v->v_angle[ROLL];
		}
		else
		{
			if (!sv_player->v->fixangle)
			{
				sv_player->v->angles[PITCH] = r_meshpitch.value * sv_player->v->v_angle[PITCH]/3;
				sv_player->v->angles[YAW] = sv_player->v->v_angle[YAW];
			}
			sv_player->v->angles[ROLL] =
				V_CalcRoll (sv_player->v->angles, sv_player->v->velocity)*4;
		}
	}

	if (SV_PlayerPhysicsQC && !host_client->spectator)
	{	//player movement tweaks that fuck over player prediction.
		pr_global_struct->frametime = host_frametime;
		pr_global_struct->time = sv.time;
		WPhys_RunEntity(&sv.world, (wedict_t*)sv_player);
		return;
	}


	for (i = 0; i < 3; i++)
	{
		if (sv_player->xv->punchangle[i] < 0)
		{
			sv_player->xv->punchangle[i] += 10 * host_frametime;
			if (sv_player->xv->punchangle[i] > 0)
				sv_player->xv->punchangle[i] = 0;
		}
		if (sv_player->xv->punchangle[i] > 0)
		{
			sv_player->xv->punchangle[i] -= 10 * host_frametime;
			if (sv_player->xv->punchangle[i] < 0)
				sv_player->xv->punchangle[i] = 0;
		}
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

		if (host_client->state && host_client->protocol == SCP_BAD)
		{
			//botclients update their movement during prethink. make sure we use that stuff.
			ucmd->angles[0] = (int)(sv_player->v->v_angle[0] * (65535/360.0f));
			ucmd->angles[1] = (int)(sv_player->v->v_angle[1] * (65535/360.0f));
			ucmd->angles[2] = (int)(sv_player->v->v_angle[2] * (65535/360.0f));

			if (host_frametime)
			{
				ucmd->forwardmove = sv_player->xv->movement[0] / host_frametime;
				ucmd->sidemove = sv_player->xv->movement[1] / host_frametime;
				ucmd->upmove = sv_player->xv->movement[2] / host_frametime;
			}
			else
			{
				ucmd->forwardmove = 0;
				ucmd->sidemove = 0;
				ucmd->upmove = 0;
			}
		}
	}

//	memset(&pmove, 0, sizeof(pmove));
//	memset(&movevars, 0, sizeof(movevars));

	pmove.player_mins[0] = sv_player->v->mins[0];
	pmove.player_mins[1] = sv_player->v->mins[1];
	pmove.player_mins[2] = sv_player->v->mins[2];

	pmove.player_maxs[0] = sv_player->v->maxs[0];
	pmove.player_maxs[1] = sv_player->v->maxs[1];
	pmove.player_maxs[2] = sv_player->v->maxs[2];

	if (sv_player->xv->gravitydir[2] || sv_player->xv->gravitydir[1] || sv_player->xv->gravitydir[0])
		VectorCopy(sv_player->xv->gravitydir, pmove.gravitydir);
	else
		VectorCopy(sv.world.g.defaultgravitydir, pmove.gravitydir);

	VectorCopy(sv_player->v->origin, pmove.origin);
	VectorCopy(sv_player->v->oldorigin, pmove.safeorigin);
	pmove.safeorigin_known = progstype != PROG_QW;

	VectorCopy (sv_player->v->velocity, pmove.velocity);
	VectorCopy (sv_player->v->v_angle, pmove.angles);

	pmove.pm_type = SV_PMTypeForClient (host_client, sv_player);
	pmove.jump_held = host_client->jump_held;
	pmove.jump_msec = 0;
	if (progstype != PROG_QW)	//this is just annoying.
		pmove.waterjumptime = sv_player->v->teleport_time - sv.time;
	else
		pmove.waterjumptime = sv_player->v->teleport_time;
	pmove.numphysent = 1;
	pmove.physents[0].model = sv.world.worldmodel;
	pmove.cmd = *ucmd;
	pmove.skipent = -1;
	pmove.capsule = (sv_player->xv->geomtype == GEOMTYPE_CAPSULE);

	movevars.entgravity = host_client->entgravity/movevars.gravity;
	movevars.maxspeed = host_client->maxspeed;
	movevars.bunnyspeedcap = pm_bunnyspeedcap.value;
	movevars.ktjump = pm_ktjump.value;
	movevars.slidefix = (pm_slidefix.value != 0);
	movevars.airstep = (pm_airstep.value != 0);
	movevars.stepdown = (pm_stepdown.value != 0);
	movevars.walljump = (pm_walljump.value);
	movevars.slidyslopes = (pm_slidyslopes.value!=0);
	movevars.watersinkspeed = *pm_watersinkspeed.string?pm_watersinkspeed.value:60;
	movevars.flyfriction = *pm_flyfriction.string?pm_flyfriction.value:4;

// should already be folded into host_client->maxspeed
//	if (sv_player->xv->hasted)
//		movevars.maxspeed*=sv_player->xv->hasted;

	for (i=0 ; i<3 ; i++)
	{
		pmove_mins[i] = pmove.origin[i] - 256;
		pmove_maxs[i] = pmove.origin[i] + 256;
	}
	sv_player->xv->pmove_flags = (int)sv_player->xv->pmove_flags & ~PMF_LADDER;	//assume not touching ladder trigger
	AddAllLinksToPmove (&sv.world, (wedict_t*)sv_player);

	if ((int)sv_player->xv->pmove_flags & PMF_LADDER)
		pmove.onladder = true;
	else
		pmove.onladder = false;

	pmove.world = &sv.world;
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
	pmove.world = NULL;

	{
		vec3_t delta;
		delta[0] = pmove.angles[0] - SHORT2ANGLE(pmove.cmd.angles[0]);
		delta[1] = pmove.angles[1] - SHORT2ANGLE(pmove.cmd.angles[1]);
		delta[2] = pmove.angles[2] - SHORT2ANGLE(pmove.cmd.angles[2]);
		if (delta[0] || delta[1] || delta[2])
		{
			if (host_client->fteprotocolextensions2 & PEXT2_SETANGLEDELTA)
			{
				client_t *cl = ClientReliableWrite_BeginSplit(host_client, svcfte_setangledelta, 7);
				for (i=0 ; i < 3 ; i++)
					ClientReliableWrite_Angle16 (cl, delta[i]);
			}
			else
			{
				client_t *cl = ClientReliableWrite_BeginSplit(host_client, svc_setangle, 7);
				for (i=0 ; i < 3 ; i++)
					ClientReliableWrite_Angle (cl, pmove.angles[i]);
			}
		}

	}

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

	VectorCopy (pmove.safeorigin, sv_player->v->oldorigin);
	VectorCopy (pmove.origin, sv_player->v->origin);
	if (!sv_player->v->fixangle)
	{
		VectorCopy (pmove.angles, sv_player->v->v_angle);

		//some clients (especially NQ ones) attempt to cheat. don't let it benefit them.
		//some things would break though.
		if ((!sv_player->xv->gravitydir[0] && !sv_player->xv->gravitydir[1] && !sv_player->xv->gravitydir[2]) && sv_player->v->movetype == MOVETYPE_WALK)
		{
			float minpitch = *sv_minpitch.string?sv_minpitch.value:-70;
			float maxpitch = *sv_maxpitch.string?sv_maxpitch.value:80;
			if (sv_player->v->v_angle[0] < minpitch)
				sv_player->v->v_angle[0] = minpitch;
			if (sv_player->v->v_angle[0] > maxpitch)
				sv_player->v->v_angle[0] = maxpitch;
		}
	}

//	VectorCopy (pmove.gravitydir, sv_player->xv->gravitydir);
	if (pmove.gravitydir[0] || pmove.gravitydir[1] || (pmove.gravitydir[2] && pmove.gravitydir[2] != -1))
	{
		if (!sv_player->v->fixangle)
		{
			//FIXME: bound to pmove.gravitydir
			vec3_t view[3];
			vec3_t surf[3];
			vec3_t fwd, up;
			AngleVectors(sv_player->v->v_angle, view[0], view[1], view[2]);
			/*calculate the surface axis with up from the pmove code and right/forwards relative to the player's directions*/
			VectorNegate(pmove.gravitydir, surf[2]);
			CrossProduct(view[0], surf[2], surf[1]);
			VectorNormalize(surf[1]);
			CrossProduct(surf[2], surf[1], surf[0]);
			/*interpolate the forward direction to be 1/3rd the player, and 2/3rds the surface forward*/
			VectorInterpolate(surf[0], 0.333, view[0], fwd);
			CrossProduct(surf[1], fwd, up);
			/*we have our player's new axis*/
			VectorAngles(fwd, up, sv_player->v->angles, true);
		}
	}

	pmove.player_mins[0] = -16;
	pmove.player_mins[1] = -16;
	pmove.player_mins[2] = -24;

	pmove.player_maxs[0] = 16;
	pmove.player_maxs[1] = 16;
	pmove.player_maxs[2] = 32;

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

			if (n >= playertouchmax || playertouch[n>>3]&(1<<(n&7)))
				continue;
			playertouch[n>>3] |= 1 << (n&7);

			if (ent->v->touch)
			{
				if (progstype != PROG_QW && VectorCompare(sv_player->v->velocity, old_vel))
				{
					VectorCopy(pmove.touchvel[i], old_vel);
					VectorCopy(pmove.touchvel[i], sv_player->v->velocity);
				}
				sv.world.Event_Touch(&sv.world, (wedict_t*)ent, (wedict_t*)sv_player);
			}

			if (sv_player->v->touch && !ED_ISFREE(ent))
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
		cursor_screen	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_screen", ev_vector, NULL);
		cursor_start	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_trace_start", ev_vector, NULL);
		cursor_impact	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_trace_endpos", ev_vector, NULL);
		cursor_entitynumber	= svprogfuncs->GetEdictFieldValue(svprogfuncs, host_client->edict, "cursor_trace_ent", ev_entity, NULL);
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

	entnum = MSGSV_ReadEntity(host_client);
	if (entnum >= sv.world.max_edicts)
	{
		Con_DPrintf("SV_ReadPrydonCursor: client sent bad cursor_entitynumber\n");
		entnum = 0;
	}
	// as requested by FrikaC, cursor_trace_ent is reset to world if the
	// entity is free at time of receipt
	if (!svprogfuncs || ED_ISFREE(EDICT_NUM(svprogfuncs, entnum)))
		entnum = 0;
	if (msg_badread) Con_Printf("SV_ReadPrydonCursor: badread at %s:%i\n", __FILE__, __LINE__);

	if (cursor_entitynumber) cursor_entitynumber->edict = entnum;
}

void SV_ReadQCRequest(void)
{
	int e;
	char args[8];
	char *rname, *fname;
	func_t f;
	int i;
	globalvars_t *pr_globals;
	client_t *cl = host_client;

	if (!svprogfuncs)
	{
		msg_badread = true;
		return;
	}

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	for (i = 0; ; )
	{
		qbyte ev = MSG_ReadByte();
		if (ev >= 200 && ev < 200+MAX_SPLITS)
		{
			ev -= 200;
			while (ev-- && cl)
				cl = cl->controlled;
			continue;
		}
		if (i >= sizeof(args)-1)
		{
			if (ev != ev_void)
			{
				msg_badread = true;
				return;
			}
			goto done;
		}
		switch(ev)
		{
		default:
			args[i] = '?';
			G_INT(OFS_PARM0+i*3) = MSG_ReadLong();
			break;
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
			e = MSGSV_ReadEntity(host_client);
			if (e < 0 || e >= sv.world.num_edicts)
				e = 0;
			G_INT(OFS_PARM0+i*3) = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, e));
			break;
		}
		i++;
	}

done:
	args[i] = 0;
	rname = MSG_ReadString();
	if (i)
		fname = va("CSEv_%s_%s", rname, args);
	else
		fname = va("CSEv_%s", rname);
	f = PR_FindFunction(svprogfuncs, fname, PR_ANY);
	if (!f)
	{
		if (i)
			rname = va("Cmd_%s_%s", rname, args);
		else
			rname = va("Cmd_%s", rname);
		f = PR_FindFunction(svprogfuncs, rname, PR_ANY);
	}
	if (!cl)
		;	//bad seat! not going to warn as they might have been removed recently
	else if (f)
	{
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, cl->edict);
		PR_ExecuteProgram(svprogfuncs, f);
	}
	else
		SV_ClientPrintf(host_client, PRINT_HIGH, "qcrequest \"%s\" not supported\n", fname);
}

/*
===================
SV_ExecuteClientMessage

The current net_message is parsed for the given client
===================
*/
void SV_ExecuteClientMessage (client_t *cl)
{
	client_t *split = cl;
	int		c;
	char	*s;
	usercmd_t	oldest, oldcmd, newcmd;
	client_frame_t	*frame;
	vec3_t o;
	int		checksumIndex;
	qbyte	checksum, calculatedChecksum;
	int		seq_hash, i;

	// calc ping time
	if (cl->frameunion.frames)
	{	//split screen doesn't always have frames.
		frame = &cl->frameunion.frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];

		if (cl->lastsequence_acknowledged + UPDATE_BACKUP > cl->netchan.incoming_acknowledged)
		{
			/*note that if there is packetloss, we can change a single frame's ping_time multiple times
			  this means that the 'ping' is more latency than ping times*/
			if (frame->ping_time == -1 || !sv_ping_ignorepl.ival)
				frame->ping_time = realtime - frame->senttime;	//no more phenomanally low pings please

			if (cl->spectator)
				cl->delay = 0;
			else
			{
				float diff = frame->ping_time*1000 - sv_minping.value;
				if (fabs(diff) > 1)
				{
					//FIXME: we should use actual arrival times instead, so we don't get so much noise and seesawing.
					diff = bound(-25, diff, 25);	//don't swing wildly
					cl->delay -= 0.001*(diff/25);	//scale towards the ideal value
					cl->delay = bound(0, cl->delay, 1);	//but make sure things don't go crazy
				}
			}
			if (cl->penalties & BAN_LAGGED)
				if (cl->delay < 0.2)
					cl->delay = 0.2;
		}

		if (sv_antilag.ival || !*sv_antilag.string)
		{
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

			cl->laggedents_frac = !*sv_antilag_frac.string?1:sv_antilag_frac.value;
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
			Con_Printf ("SVQW_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte ();
		if (c == -1)
			break;

//		Con_Printf("(%s) %i: %i\n", cl->name, msg_readcount, c);

		switch (c)
		{
		default:
			Con_Printf ("SVQW_ReadClientMessage: unknown command char %i\n", c);
			SV_DropClient (cl);
			return;

		case clc_nop:
			break;

		case clc_delta:
			cl->delta_sequence = MSG_ReadByte ();
			break;

		case clc_move:
			if (split == cl)
			{
				//only the first player is checksummed. its pointless as a security measure now quake is open source.
				checksumIndex = MSG_GetReadCount();
				checksum = (qbyte)MSG_ReadByte ();

				//only the first player has packetloss calculated.
				split->lossage = MSG_ReadByte();
			}
			else
			{
				checksumIndex = checksum = 0;
				if (split)
					split->lossage = cl->lossage;
			}
			MSG_ReadDeltaUsercmd (&nullcmd, &oldest, PROTOCOL_VERSION_QW);
			MSG_ReadDeltaUsercmd (&oldest, &oldcmd, PROTOCOL_VERSION_QW);
			MSG_ReadDeltaUsercmd (&oldcmd, &newcmd, PROTOCOL_VERSION_QW);
			if (!split)
				break;		// either someone is trying to cheat, or they sent input commands for splitscreen clients they no longer own.

			split->localtime = sv.time;

			if (split->frameunion.frames)
				split->frameunion.frames[split->netchan.outgoing_sequence&UPDATE_MASK].move_msecs = newcmd.msec;

			if (split->state == cs_spawned)
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
							split->name, split->netchan.incoming_sequence, checksum, calculatedChecksum);
						break;
					}
				}

				if (split->penalties & BAN_CRIPPLED)
				{
					split->lastcmd.forwardmove = 0;	//hmmm.... does this work well enough?
					oldest.forwardmove = 0;
					oldcmd.forwardmove = 0;
					newcmd.forwardmove = 0;

					split->lastcmd.sidemove = 0;
					oldest.sidemove = 0;
					oldcmd.sidemove = 0;
					newcmd.sidemove = 0;

					split->lastcmd.upmove = 0;
					oldest.upmove = 0;
					oldcmd.upmove = 0;
					newcmd.upmove = 0;
				}

				host_client = split;
				sv_player = split->edict;
#ifdef HLSERVER
				if (svs.gametype == GT_HALFLIFE)
				{
					SVHL_RunPlayerCommand(split, &oldest, &oldcmd, &newcmd);
				}
				else
#endif
					if (!sv.paused && sv.world.worldmodel && sv.world.worldmodel->loadstate == MLS_LOADED)
				{
					if (sv_nqplayerphysics.ival || split->state < cs_spawned)
					{
						//store the info for the physics code to pick up the next time it ticks.
						//yeah, nq sucks.
						split->isindependant = false;
						if (!split->edict->v->fixangle)
						{
							split->edict->v->v_angle[0] = newcmd.angles[0]* (360.0/65536);
							split->edict->v->v_angle[1] = newcmd.angles[1]* (360.0/65536);
							split->edict->v->v_angle[2] = newcmd.angles[2]* (360.0/65536);
						}

						if (newcmd.impulse)// && SV_FilterImpulse(newcmd.impulse, host_client->trustlevel))
							split->edict->v->impulse = newcmd.impulse;

						split->edict->v->button0 = newcmd.buttons & 1;
						split->edict->v->button2 = (newcmd.buttons >> 1) & 1;
						if (pr_allowbutton1.ival)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
							split->edict->v->button1 = ((newcmd.buttons >> 2) & 1);
					// DP_INPUTBUTTONS
						split->edict->xv->button3 = ((newcmd.buttons >> 2) & 1);
						split->edict->xv->button4 = ((newcmd.buttons >> 3) & 1);
						split->edict->xv->button5 = ((newcmd.buttons >> 4) & 1);
						split->edict->xv->button6 = ((newcmd.buttons >> 5) & 1);
						split->edict->xv->button7 = ((newcmd.buttons >> 6) & 1);
						split->edict->xv->button8 = ((newcmd.buttons >> 7) & 1);
					}
					else
					{
						//run player physics instantly.
						split->isindependant = true;
						SV_PreRunCmd();

						if (net_drop < 20)
						{
							while (net_drop > 2)
							{
								SV_RunCmd (&split->lastcmd, false);
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

				}
				else
				{
					if (newcmd.impulse)// && SV_FilterImpulse(newcmd.impulse, host_client->trustlevel))
						sv_player->v->impulse = newcmd.impulse;
				}

				split->lastcmd = newcmd;
				split->lastcmd.buttons = 0; // avoid multiple fires on lag
			}
			split = split->controlled;	//so the next splitscreen client gets the next packet.
			host_client = cl;
			sv_player = cl->edict;
			break;

		case clcfte_prydoncursor:
			SV_ReadPrydonCursor();
			break;
		case clcfte_qcrequest:
			SV_ReadQCRequest();
			break;
#ifdef TERRAIN
		case clcfte_brushedit:
			if (!SV_Parse_BrushEdit())
			{
				SV_DropClient (cl);
				return;
			}
			break;
#endif

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
		case clcfte_voicechat:
			SV_VoiceReadPacket();
			break;
#endif
		case clcdp_ackframe:
			cl->delta_sequence = MSG_ReadLong();
			SV_AckEntityFrame(cl, cl->delta_sequence);
			break;
		}
	}

	SV_AckEntityFrame(cl, cl->netchan.incoming_acknowledged);

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
	int		move_issued = 0; //only allow one move command
	int		checksumIndex;
	qbyte	checksum, calculatedChecksum;
	int		seq_hash;
	int lastframe;
	client_t *split;

	if (!ge)
	{
		Con_Printf("Q2 client without Q2 server\n");
		SV_DropClient(cl);
	}

	// make sure the reply sequence number matches the incoming
	// sequence number
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped

	// calc ping time
	if (cl->netchan.outgoing_sequence - cl->netchan.incoming_acknowledged > Q2UPDATE_MASK)
	{
		cl->delay -= 0.001;
		if (cl->delay < 0)
			cl->delay = 0;
	}
	else
	{
		frame = &cl->frameunion.q2frames[cl->netchan.incoming_acknowledged & Q2UPDATE_MASK];
		if (frame->senttime != -1)
		{
			int ping_time = (int)(realtime*1000) - frame->senttime;	//no more phenomanally low pings please
			if (ping_time > sv_minping.value+1)
			{
				cl->delay -= 0.001;
				if (cl->delay < 0)
					cl->delay = 0;
			}
			if (ping_time < sv_minping.value)
			{
				cl->delay += 0.001;
				if (cl->delay > 1)
					cl->delay = 1;
			}
			frame->senttime = -1;
			frame->ping_time = ping_time;
		}
	}

	// save time for ping calculations
//	cl->frameunion.q2frames[cl->netchan.outgoing_sequence & Q2UPDATE_MASK].senttime = realtime*1000;
//	cl->frameunion.q2frames[cl->netchan.outgoing_sequence & Q2UPDATE_MASK].ping_time = -1;

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
			Con_Printf ("SVQ2_ReadClientMessage: unknown command char %i\n", c);
			SV_DropClient (cl);
			return;

		case clcq2_nop:
			break;

		case clcq2_move:
			if (move_issued >= MAX_SPLITS)
				return;		// someone is trying to cheat...

			for (checksumIndex = 0, split = cl; split && checksumIndex < move_issued; checksumIndex++)
				split = split->controlled;

			if (move_issued)
			{
				checksumIndex = -1;
				checksum = 0;
			}
			else
			{
				checksumIndex = MSG_GetReadCount();
				checksum = (qbyte)MSG_ReadByte ();


				lastframe = MSG_ReadLong();
				if (lastframe != split->delta_sequence)
				{
					split->delta_sequence = lastframe;
				}
			}

			MSGQ2_ReadDeltaUsercmd (&nullcmd, &oldest);
			MSGQ2_ReadDeltaUsercmd (&oldest, &oldcmd);
			MSGQ2_ReadDeltaUsercmd (&oldcmd, &newcmd);

			if ( split && split->state == cs_spawned )
			{
				if (checksumIndex != -1)
				{
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
				}

				if (split->penalties & BAN_CRIPPLED)
				{
					split->lastcmd.forwardmove = 0;	//hmmm.... does this work well enough?
					oldest.forwardmove = 0;
					newcmd.forwardmove = 0;

					split->lastcmd.sidemove = 0;
					oldest.sidemove = 0;
					newcmd.sidemove = 0;

					split->lastcmd.upmove = 0;
					oldest.upmove = 0;
					newcmd.upmove = 0;
				}

				split->q2edict->client->ping = SV_CalcPing (split, false);
				if (!sv.paused)
				{
					if (net_drop < 20)
					{
						while (net_drop > 2)
						{
							ge->ClientThink (split->q2edict, (q2usercmd_t*)&split->lastcmd);
							net_drop--;
						}
						if (net_drop > 1)
							ge->ClientThink (split->q2edict, (q2usercmd_t*)&oldest);
						if (net_drop > 0)
							ge->ClientThink (split->q2edict, (q2usercmd_t*)&oldcmd);
					}
					ge->ClientThink (split->q2edict, (q2usercmd_t*)&newcmd);
				}

				split->lastcmd = newcmd;
			}
			move_issued++;
			break;

		case clcq2_userinfo:
			strncpy (cl->userinfo, MSG_ReadString (), sizeof(cl->userinfo)-1);
			ge->ClientUserinfoChanged (cl->q2edict, cl->userinfo);	//tell the gamecode
			SV_ExtractFromUserinfo(cl, true);	//let the server routines know
			break;

		case clcq2_stringcmd:
			s = MSG_ReadString ();
			SV_ExecuteUserCommand (s, false);

			host_client = cl;
			sv_player = cl->edict;

			if (cl->state < cs_connected)
				return;	// disconnect command
			break;

#ifdef VOICECHAT
		case clcq2_voicechat:
			SV_VoiceReadPacket();
			break;
#endif
		}
	}
}
#endif
#ifdef NQPROT
void SVNQ_ReadClientMove (usercmd_t *move, qboolean forceangle16)
{
	int		i;
	int		bits;
	client_frame_t	*frame;
	float timesincelast;
	float cltime;

	frame = &host_client->frameunion.frames[host_client->netchan.incoming_acknowledged & UPDATE_MASK];

	if (host_client->protocol == SCP_DARKPLACES7)
		host_client->last_sequence = MSG_ReadLong ();
	else if (host_client->fteprotocolextensions2 & PEXT2_PREDINFO)
	{
		int seq = (unsigned short)MSG_ReadShort ();
		if (seq < (host_client->last_sequence&0xffff))
			host_client->last_sequence += 0x10000;	//wrapped
		host_client->last_sequence = (host_client->last_sequence&0xffff0000) | seq;
	}
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
	move->servertime = move->fservertime*1000;

	frame->ping_time = sv.time - cltime;


	if (frame->ping_time*1000 > sv_minping.value+1)
	{
		host_client->delay -= 0.001;
		if (host_client->delay < 0)
			host_client->delay = 0;
	}
	if (frame->ping_time*1000 < sv_minping.value)
	{
		host_client->delay += 0.001;
		if (host_client->delay > 1)
			host_client->delay = 1;
	}


// read current angles
	for (i=0 ; i<3 ; i++)
	{
		if (forceangle16)
			host_client->edict->v->v_angle[i] = MSG_ReadAngle16 ();
		else
			host_client->edict->v->v_angle[i] = MSG_ReadAngle ();

		move->angles[i] = ANGLE2SHORT(host_client->edict->v->v_angle[i]);
	}

// read movement
	move->forwardmove = MSG_ReadShort ();
	move->sidemove = MSG_ReadShort ();
	move->upmove = MSG_ReadShort ();

	move->msec=bound(0, timesincelast*1000, 250);
	frame->move_msecs = timesincelast*1000;

// read buttons
	if (host_client->protocol == SCP_DARKPLACES6 || host_client->protocol == SCP_DARKPLACES7 || (host_client->fteprotocolextensions2 & PEXT2_PRYDONCURSOR))
		bits = MSG_ReadLong();
	else
		bits = MSG_ReadByte ();
	if (host_client->spectator)
	{
		qboolean tracknext = false;
		if (bits & (move->buttons ^ bits) & BUTTON_ATTACK)
		{	//enable/disable tracking
			if (host_client->spec_track)
			{	//disable tracking
				host_client->spec_track = 0;
				host_client->edict->v->goalentity = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, 0));
				ClientReliableWrite_Begin(host_client, svc_setview, 4);
				ClientReliableWrite_Entity(host_client, host_client - svs.clients + 1);
			}
			else	//otherwise track the next person, if we can
				tracknext = true;
		}
		if ((bits & (move->buttons ^ bits) & BUTTON_JUMP) && host_client->spec_track)
			tracknext = true;

		if (tracknext)
		{	//track the next player
			for (i = host_client->spec_track+1; i < sv.allocated_client_slots; i++)
			{
				if (SV_CanTrack(host_client, i))
					break;
			}
			//try a previous one instead of disabling
			if (i == sv.allocated_client_slots)
			{
				for (i = 1; i < host_client->spec_track; i++)
				{
					if (SV_CanTrack(host_client, i))
						break;
				}
				if (i >= host_client->spec_track)
					i = 0;
			}

			host_client->spec_track = i;
			host_client->edict->v->goalentity = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, i));
			ClientReliableWrite_Begin(host_client, svc_setview, 4);
			ClientReliableWrite_Entity(host_client, i?i:(host_client - svs.clients + 1));

			if (i)
				SV_ClientTPrintf (host_client, PRINT_HIGH, "tracking %s\n", svs.clients[i-1].name);
		}
	}
	move->buttons = bits;

	i = MSG_ReadByte ();
	move->impulse = i;

	if (host_client->protocol == SCP_DARKPLACES6 || host_client->protocol == SCP_DARKPLACES7 || (host_client->fteprotocolextensions2 & PEXT2_PRYDONCURSOR))
	{
		SV_ReadPrydonCursor();
	}

	if (SV_RunFullQCMovement(host_client, move))
	{
		host_client->msecs -= move->msec;
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


//	if (i && SV_FilterImpulse(i, host_client->trustlevel))
//		host_client->edict->v->impulse = i;

	host_client->edict->v->button0 = bits & 1;
	host_client->edict->v->button2 = (bits >> 1) & 1;
	if (pr_allowbutton1.ival && progstype == PROG_QW)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
		host_client->edict->v->button1 = ((bits >> 2) & 1);
// DP_INPUTBUTTONS
	host_client->edict->xv->button3 = ((bits >> 2) & 1);
	host_client->edict->xv->button4 = ((bits >> 3) & 1);
	host_client->edict->xv->button5 = ((bits >> 4) & 1);
	host_client->edict->xv->button6 = ((bits >> 5) & 1);
	host_client->edict->xv->button7 = ((bits >> 6) & 1);
	host_client->edict->xv->button8 = ((bits >> 7) & 1);

	if (host_client->last_sequence && !sv_nqplayerphysics.ival && host_client->state == cs_spawned)
	{
		host_frametime = timesincelast;

		host_client->isindependant = true;
		SV_PreRunCmd();
		SV_RunCmd (move, false);
		SV_PostRunCmd();
		move->impulse = 0;
	}
	else
	{
		host_client->last_sequence = 0;	//let the client know that prediction is fucked, by not acking any input frames.
		if (i)
			host_client->edict->v->impulse = i;
		host_client->isindependant = false;
	}
}

void SVNQ_ExecuteClientMessage (client_t *cl)
{
	int		c;
	char	*s;
//	client_frame_t	*frame;
	qboolean forceangle16;

	cl->netchan.outgoing_sequence++;
	cl->netchan.incoming_acknowledged = cl->netchan.outgoing_sequence-1;

	// calc ping time
//	frame = &cl->frameunion.frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];
//	frame->ping_time = -1;

	// make sure the reply sequence number matches the incoming
	// sequence number
//	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
//	else
//		cl->send_message = false;	// don't reply, sequences have slipped

	// save time for ping calculations
//	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = realtime;
//	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;
//	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].move_msecs = -1;
//	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].packetsizein = net_message.cursize;
//	cl->frameunion.frames[cl->netchan.outgoing_sequence & UPDATE_MASK].packetsizeout = 0;

	host_client = cl;
	sv_player = host_client->edict;

	// mark time so clients will know how much to predict
	// other players
 	cl->localtime = sv.time;
	while (1)
	{
		if (msg_badread)
		{
			Con_Printf ("SVNQ_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte ();
		if (c == -1)
			break;

		switch (c)
		{
		default:
			Con_Printf ("SVNQ_ReadClientMessage: unknown command char %i\n", c);
			SV_DropClient (cl);
			return;

		case clc_disconnect:
			host_client = cl;
			sv_player = cl->edict;
			SV_Drop_f();
			return;
		case clc_nop:
			break;

//		case clc_delta:			//not in NQ
//			cl->delta_sequence = MSG_ReadByte ();
//			break;

		case clc_move:	//bytes: 16(nq), 19(proquake/fitz), 56(dp7)
			if (cl->state != cs_spawned)
				return;	//shouldn't be sending moves at this point. typically they're stale, left from the previous map. this results in crashes if the protocol is different.

			forceangle16 = false;
			switch(cl->protocol)
			{
			case SCP_FITZ666:
				forceangle16 = true;
				break;
			case SCP_NETQUAKE:
			case SCP_BJP3:
				//Hack to work around buggy DP clients that don't reset the proquake hack for the next server
				//this ONLY works because the other clc commands are very unlikely to both be 3 bytes big and sent unreliably
				//aka: DP ProQuake angles hack hack
				//note that if a client then decides to use 16bit coords because of this hack then it would be the 'fte dp proquake angles hack hack hack'....
				if ((net_message.cursize-(msg_readcount-1) == 16 &&  cl->proquake_angles_hack) ||
					(net_message.cursize-(msg_readcount-1) == 19 && !cl->proquake_angles_hack))
				{
					cl->proquake_angles_hack ^= 1;
					SV_ClientPrintf(cl, PRINT_HIGH, "Client sent "S_COLOR_RED"wrong"S_COLOR_WHITE" clc_move size, switching to %u-bit angles to try to compensate\n", cl->proquake_angles_hack?16:8);
				}
				forceangle16 = cl->proquake_angles_hack;
				break;
			case SCP_BAD:
			case SCP_QUAKEWORLD:
			case SCP_QUAKE2:
			case SCP_QUAKE3:
			case SCP_DARKPLACES6:
			case SCP_DARKPLACES7:
				break;
			}
				
			SVNQ_ReadClientMove (&cl->lastcmd, forceangle16);
//			cmd = host_client->lastcmd;
//			SV_ClientThink();
			break;

		case clc_stringcmd:
			s = MSG_ReadString ();
			SV_ExecuteUserCommand (s, false);

			host_client = cl;
			sv_player = cl->edict;
			if (cl->state < cs_connected)
				return;
			break;

		case clcfte_qcrequest:
			SV_ReadQCRequest();

			host_client = cl;
			sv_player = cl->edict;
			if (cl->state < cs_connected)
				return;
			break;

		case clcdp_ackframe:
			cl->delta_sequence = MSG_ReadLong();
			if (cl->delta_sequence == -1 && cl->pendingdeltabits)
				cl->pendingdeltabits[0] = UF_REMOVE;
			SV_AckEntityFrame(cl, cl->delta_sequence);
//			if (cl->frameunion.frames[cl->delta_sequence&UPDATE_MASK].sequence == cl->delta_sequence)
//				if (cl->frameunion.frames[cl->delta_sequence&UPDATE_MASK].ping_time < 0)
//					cl->frameunion.frames[cl->delta_sequence&UPDATE_MASK].ping_time = realtime - cl->frameunion.frames[cl->delta_sequence&UPDATE_MASK].senttime;
			break;
		case clcdp_ackdownloaddata:
			SV_DarkPlacesDownloadAck(cl);
			break;

#ifdef VOICECHAT
		case clcfte_voicechat:
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

	Cvar_Register (&sv_minpitch, cvargroup_servercontrol);
	Cvar_Register (&sv_maxpitch, cvargroup_servercontrol);

	Cvar_Register (&sv_fullredirect, cvargroup_servercontrol);
	Cvar_Register (&sv_antilag, cvargroup_servercontrol);
	Cvar_Register (&sv_antilag_frac, cvargroup_servercontrol);
#ifndef NEWSPEEDCHEATPROT
	Cvar_Register (&sv_cheatpc, cvargroup_servercontrol);
	Cvar_Register (&sv_cheatspeedchecktime, cvargroup_servercontrol);
#endif
	Cvar_Register (&sv_playermodelchecks, cvargroup_servercontrol);

	Cvar_Register (&sv_getrealip, cvargroup_servercontrol);
	Cvar_Register (&sv_realip_kick, cvargroup_servercontrol);
	Cvar_Register (&sv_realiphostname_ipv4, cvargroup_servercontrol);
	Cvar_Register (&sv_realiphostname_ipv6, cvargroup_servercontrol);
	Cvar_Register (&sv_realip_timeout, cvargroup_servercontrol);

	Cvar_Register (&sv_pushplayers, cvargroup_servercontrol);
	Cvar_Register (&sv_protocol_nq, cvargroup_servercontrol);

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

#ifndef NOLEGACY
	Cvar_Register (&sv_brokenmovetypes, "Backwards compatability");
#endif

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
#ifdef HEXEN2
	if (sv_player->xv->hasted)
		maxspeed*=sv_player->xv->hasted;
#endif

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
	int i;
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
	for (i = 0; i < 3; i++)
	{
		if (sv_player->xv->punchangle[i] < 0)
		{
			sv_player->xv->punchangle[i] += 10 * host_frametime;
			if (sv_player->xv->punchangle[i] > 0)
				sv_player->xv->punchangle[i] = 0;
		}
		if (sv_player->xv->punchangle[i] > 0)
		{
			sv_player->xv->punchangle[i] -= 10 * host_frametime;
			if (sv_player->xv->punchangle[i] < 0)
				sv_player->xv->punchangle[i] = 0;
		}
	}

//
// if dead, behave differently
//
	if (sv_player->v->health <= 0 && !host_client->spectator)
		return;

//
// angles
// show 1/3 the pitch angle and all the roll angle
	angles = sv_player->v->angles;

	VectorCopy (sv_player->v->v_angle, v_angle);
//	VectorAdd (sv_player->v->v_angle, sv_player->v->punchangle, v_angle);
	//FIXME: gravitydir stuff, the roll value gets destroyed for intents
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
