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
cvar_t	cl_rollspeed = {"cl_rollspeed", "200"};
cvar_t	cl_rollangle = {"cl_rollangle", "2.0"};
#else
extern cvar_t	cl_rollspeed;
extern cvar_t	cl_rollangle;
#endif
cvar_t	sv_spectalk = {"sv_spectalk", "1"};

cvar_t	sv_mapcheck	= {"sv_mapcheck", "1"};

cvar_t	sv_cheatpc = {"sv_cheatpc", "125"};
cvar_t	sv_cheatspeedchecktime = {"sv_cheatspeedchecktime", "30"};
cvar_t	sv_playermodelchecks = {"sv_playermodelchecks", "1"};

cvar_t	sv_cmdlikercon = {"sv_cmdlikercon", "0"};

cvar_t	sv_nomsec = {"sv_nomsec", "0"};
cvar_t	sv_edgefriction = {"sv_edgefriction", "2"};

cvar_t	sv_brokenmovetypes = {"sv_brokenmovetypes", "0"};

cvar_t	sv_chatfilter = {"sv_chatfilter", "0"};

cvar_t	votelevel	= {"votelevel", "0"};
cvar_t	voteminimum	= {"voteminimum", "4"};
cvar_t	votepercent = {"votepercent", "-1"};
cvar_t	votetime = {"votetime", "10"};

cvar_t	pr_allowbutton1 = {"pr_allowbutton1", "1", NULL, CVAR_LATCH};


extern cvar_t	pm_bunnyspeedcap;
extern cvar_t	pm_ktjump;
extern cvar_t	pm_slidefix;
extern cvar_t	pm_airstep;

char sv_votinggroup[] = "server voting";


extern char cvargroup_serverpermissions[];
extern char cvargroup_serverinfo[];
extern char cvargroup_serverphysics[];
extern char cvargroup_servercontrol[];

extern	vec3_t	player_mins, player_maxs;

extern int fp_messages, fp_persecond, fp_secondsdead;
extern char fp_msg[];
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
	client_t *split;

	if (host_client->state == cs_spawned)
		return;
#ifdef NQPROT
	host_client->nqprot = false;
#endif
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
		gamedir = "qw";

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf)
	{
		Con_TPrintf(STL_BACKBUFSET, host_client->name, host_client->netchan.message.cursize); 
		host_client->num_backbuf = 0;
		SZ_Clear(&host_client->netchan.message);
	}

	if (sizeofcoord > 2 && !(host_client->fteprotocolextensions & PEXT_FLOATCOORDS))
	{
		SV_ClientPrintf(host_client, 2, "\n\n\n\nSorry, but your client does not appear to support FTE's bigcoords\nFTE users will need to set cl_nopext to 0 and then reconnect, or to upgrade\n");
		Con_Printf("%s does not support bigcoords\n", host_client->name);
		return;
	}


	// send the serverdata
	MSG_WriteByte (&host_client->netchan.message, host_client->isq2client?svcq2_serverdata:svc_serverdata);
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
	MSG_WriteLong (&host_client->netchan.message, host_client->isq2client?host_client->isq2client:PROTOCOL_VERSION);
	MSG_WriteLong (&host_client->netchan.message, svs.spawncount);
	if (host_client->isq2client)
		MSG_WriteByte (&host_client->netchan.message, 0);
	MSG_WriteString (&host_client->netchan.message, gamedir);

	for (split = host_client; split; split = split->controlled)
	{
#ifdef Q2SERVER
		if (!svprogfuncs)
			playernum = Q2NUM_FOR_EDICT(split->q2edict)-1;
		else
#endif
			playernum = NUM_FOR_EDICT(svprogfuncs, split->edict)-1;
		if (sv.demostate)
			playernum = (MAX_CLIENTS-1)|128;
		else if (split->spectator)
			playernum |= 128;

		if (sv.state == ss_cinematic)
			playernum = -1;

		if (host_client->isq2client)
			MSG_WriteShort (&host_client->netchan.message, playernum);
		else
			MSG_WriteByte (&host_client->netchan.message, playernum);

		split->state = cs_connected;
		split->connection_started = realtime;
	#ifdef SVRANKING
		split->stats_started = realtime;
	#endif
	}
	if (host_client->fteprotocolextensions & PEXT_SPLITSCREEN)
		MSG_WriteByte (&host_client->netchan.message, 128);

	// send full levelname
	MSG_WriteString (&host_client->netchan.message, sv.mapname);

	//
	// game server
	// 
#ifdef Q2SERVER
	if (host_client->isq2client)
	{
		if (sv.state != ss_cinematic)
		{
//			host_client->q2edict = Q2NUM_FOR_EDICT(split->q2edict)-1;
			memset (&host_client->lastcmd, 0, sizeof(host_client->lastcmd));

			// begin fetching configstrings
			MSG_WriteByte (&host_client->netchan.message, svcq2_stufftext);
			MSG_WriteString (&host_client->netchan.message, va("cmd configstrings %i 0\n",svs.spawncount) );
		}
		return;
	}
#endif
	// send the movevars
	MSG_WriteFloat(&host_client->netchan.message, movevars.gravity);
	MSG_WriteFloat(&host_client->netchan.message, movevars.stopspeed);
	MSG_WriteFloat(&host_client->netchan.message, movevars.maxspeed);
	MSG_WriteFloat(&host_client->netchan.message, movevars.spectatormaxspeed);
	MSG_WriteFloat(&host_client->netchan.message, movevars.accelerate);
	MSG_WriteFloat(&host_client->netchan.message, movevars.airaccelerate);
	MSG_WriteFloat(&host_client->netchan.message, movevars.wateraccelerate);
	MSG_WriteFloat(&host_client->netchan.message, movevars.friction);
	MSG_WriteFloat(&host_client->netchan.message, movevars.waterfriction);
	MSG_WriteFloat(&host_client->netchan.message, movevars.entgravity);

	// send server info string
	MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
	MSG_WriteString (&host_client->netchan.message, va("fullserverinfo \"%s\"\n", svs.info) );

	// send music
	MSG_WriteByte (&host_client->netchan.message, svc_cdtrack);
	if (svprogfuncs)
		MSG_WriteByte (&host_client->netchan.message, sv.edicts->v.sounds);
	else
		MSG_WriteByte (&host_client->netchan.message, 0);
}
#define NQ_PROTOCOL_VERSION 15
#define GAME_DEATHMATCH 0
#define GAME_COOP 1
void SVNQ_New_f (void)
{
	extern cvar_t coop;
	char			message[2048];
	int i;
#ifdef NQPROT
	host_client->nqprot = true;
#endif
	MSG_WriteByte (&host_client->netchan.message, svc_print);
#ifdef DISTRIBUTION
	sprintf (message, "%c\n" DISTRIBUTION " QuakeWorld version %4.2f server\n", 2, VERSION);	
#else
	sprintf (message, "%c\nQUAKEWORLD VERSION %4.2f SERVER\n", 2, VERSION);
#endif
	MSG_WriteString (&host_client->netchan.message,message);

	MSG_WriteByte (&host_client->netchan.message, svc_serverdata);
	MSG_WriteLong (&host_client->netchan.message, NQ_PROTOCOL_VERSION);
	MSG_WriteByte (&host_client->netchan.message, 16);

	if (!coop.value && deathmatch.value)
		MSG_WriteByte (&host_client->netchan.message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&host_client->netchan.message, GAME_COOP);

	strcpy (message, sv.mapname);

	MSG_WriteString (&host_client->netchan.message,message);

	for (i = 1; *sv.model_precache[i] ; i++)
		MSG_WriteString (&host_client->netchan.message, sv.model_precache[i]);
	MSG_WriteByte (&host_client->netchan.message, 0);

	for (i = 1; *sv.sound_precache[i] ; i++)
		MSG_WriteString (&host_client->netchan.message, sv.sound_precache[i]);
	MSG_WriteByte (&host_client->netchan.message, 0);


// send music
	MSG_WriteByte (&host_client->netchan.message, svc_cdtrack);
	MSG_WriteByte (&host_client->netchan.message, sv.edicts->v.sounds);
	MSG_WriteByte (&host_client->netchan.message, sv.edicts->v.sounds);

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
	extern int map_checksum;
	int			start;

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

		//choose range to grab from.
		if (start < Q2CS_CDTRACK)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			MSG_WriteString (&host_client->netchan.message, sv.name);
/*			if (svprogfuncs)
				MSG_WriteString (&host_client->netchan.message, va("%s", PR_GetString(svprogfuncs, sv.edicts->v.message)));
			else
				MSG_WriteString (&host_client->netchan.message, "LEVEL NAME");
*/		}
		else if (start < Q2CS_SKY)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_configstring);
			MSG_WriteShort (&host_client->netchan.message, start);
			if (svprogfuncs)
				MSG_WriteString (&host_client->netchan.message, va("%i", sv.edicts->v.sounds));
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
	int crc;
	char *name;
	int i;

	if (host_client->state != cs_connected)
	{	//fixme: send prints instead
		Con_Printf ("pk3list not valid -- allready spawned\n");
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
	if (host_client->num_backbuf) {
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

	for (i; ; i++)
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
			continue;	//okay, that was all we could find.
		}
		MSG_WriteByte(&host_client->netchan.message, svc_stufftext);
		MSG_WriteString(&host_client->netchan.message, va("soundlist %i 0\n", svs.spawncount));
		return;
	}
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
	int			n;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("soundlist not valid -- allready spawned\n");
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

	if (n < 0)
	{
		Con_Printf ("SV_Soundlist_f: %s tried to crash us\n", host_client->name);
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
			*sv.sound_precache[i] && host_client->netchan.message.cursize < (MAX_QWMSGLEN/2); 
			i++, n++)
			MSG_WriteString (&host_client->netchan.message, sv.sound_precache[i]);

		if (!*sv.sound_precache[i])
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
	int			n;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("modellist not valid -- allready spawned\n");
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

	if (n < 0)
	{
		Con_Printf ("SV_Modellist_f: %s tried to crash us\n", host_client->name);
		SV_DropClient(host_client);
		return;
	}

	if (n >= 255)
	{
		MSG_WriteByte (&host_client->netchan.message, svc_modellistshort);
		MSG_WriteShort (&host_client->netchan.message, n);
	}
	else
	{
		MSG_WriteByte (&host_client->netchan.message, svc_modellist);
		MSG_WriteByte (&host_client->netchan.message, n);
	}

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
			*sv.model_precache[i] && (((n&255)==0)||(host_client->netchan.message.cursize < (MAX_QWMSGLEN/2)));	//make sure we don't send a 0 next...
			i++, n++)
			MSG_WriteString (&host_client->netchan.message, sv.model_precache[i]);

		if (!*sv.model_precache[i])
			n = 0;
	}

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
		Con_Printf ("prespawn not valid -- allready spawned\n");
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
		buf = 0;
	if (buf < 0)
		buf = 0;

	if (!buf)
	{
		// should be three numbers following containing checksums
		check = atoi(Cmd_Argv(3));

//		Con_DPrintf("Client check = %d\n", check);

		if (sv_mapcheck.value && check != sv.worldmodel->checksum &&
			check != sv.worldmodel->checksum2)
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

	if (buf >= bufs)
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
			else
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

			if (ent->tagent)
			{
				MSG_WriteByte(&host_client->netchan.message, svc_setattachment);
				MSG_WriteShort(&host_client->netchan.message, ent->entnum);
				MSG_WriteShort(&host_client->netchan.message, ent->tagent);
				MSG_WriteShort(&host_client->netchan.message, ent->tagindex);
			}

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
				MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline2);
				SV_WriteDelta(&from, state, &host_client->netchan.message, true, host_client->fteprotocolextensions);
			}
			else
			{
				MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline);

				MSG_WriteShort (&host_client->netchan.message, buf - bufs - sv.numextrastatics);

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
				MSG_WriteByte(&host_client->netchan.message, svc_customtempent);
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
	else 
	{
		if (sv.democausesreconnect)
			SZ_Write (&host_client->netchan.message, 
				sv.demosignon_buffers[buf],
				sv.demosignon_buffer_size[buf]);
		else
			SZ_Write (&host_client->netchan.message, 
				sv.signon_buffers[buf],
				sv.signon_buffer_size[buf]);
		buf++;
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

static void SetUpClientEdict (client_t *cl, edict_t *ent);
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
		Con_Printf ("Spawn not valid -- allready spawned\n");
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
			ClientReliableWrite_Begin (host_client, svc_lightstyle, 
				3 + (sv.demolightstyles[i] ? strlen(sv.demolightstyles[i]) : 1));
			ClientReliableWrite_Byte (host_client, (char)i);
			ClientReliableWrite_String (host_client, sv.demolightstyles[i]);
		}
		else
#ifdef PEXT_LIGHTSTYLECOL
			 if (host_client->fteprotocolextensions & PEXT_LIGHTSTYLECOL && sv.lightstylecolours[i]!=7)
		{
			ClientReliableWrite_Begin (host_client, svc_lightstylecol, 
				3 + (sv.lightstyles[i] ? strlen(sv.lightstyles[i]) : 1));
			ClientReliableWrite_Byte (host_client, (char)i);
			ClientReliableWrite_Char (host_client, sv.lightstylecolours[i]);
			ClientReliableWrite_String (host_client, sv.lightstyles[i]);
		}
		else
#endif
		{
			ClientReliableWrite_Begin (host_client, svc_lightstyle, 
				3 + (sv.lightstyles[i] ? strlen(sv.lightstyles[i]) : 1));
			ClientReliableWrite_Byte (host_client, (char)i);
			ClientReliableWrite_String (host_client, sv.lightstyles[i]);
		}
	}

	// set up the edict
	for (split = host_client; split; split = split->controlled)
	{
		ent = split->edict;

		if (split->istobeloaded)	//minimal setup
		{
			split->entgravity = ent->v.gravity;
			split->maxspeed = ent->v.maxspeed;
		}
		else
		{
			SetUpClientEdict(split, ent);
		}

	//
	// force stats to be updated
	//
		memset (host_client->stats, 0, sizeof(host_client->stats));
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

	VectorCopy (vec3_origin, sv_player->v.origin);
	VectorCopy (vec3_origin, sv_player->v.view_ofs);
	sv_player->v.view_ofs[2] = DEFAULT_VIEWHEIGHT;
	sv_player->v.movetype = MOVETYPE_NOCLIP;

	// search for an info_playerstart to spawn the spectator at
	for (i=MAX_CLIENTS+1 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(svprogfuncs, i);
		if (!strcmp(PR_GetString(svprogfuncs, e->v.classname), "info_player_start"))
		{
			VectorCopy (e->v.origin, sv_player->v.origin);
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

	for (split = host_client; split; split = split->controlled)
	{
		if (split->istobeloaded)
		{
			sendangles = true;
			split->istobeloaded = false;
		}
		else
		{
			if (split->spectator)
			{
				SV_SpawnSpectator ();

				if (SpectatorConnect)
				{
					// copy spawn parms out of the client_t
					for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
						(&pr_global_struct->parm1)[i] = split->spawn_parms[i];

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
							(&pr_global_struct->parm1)[j] = split->spawn_parms[j];
						pr_global_struct->time = sv.time;
						pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
						G_FLOAT(OFS_PARM0) = sv.time - split->spawninfotime;
						PR_ExecuteProgram(svprogfuncs, eval->function);
					}
					else
					{
						// copy spawn parms out of the client_t
						for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
							(&pr_global_struct->parm1)[i] = split->spawn_parms[i];

						// call the spawn function
						pr_global_struct->time = sv.time;
						pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);
						PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientConnect);

						// actually spawn the player
						pr_global_struct->time = sv.time;
						pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, split->edict);			
						PR_ExecuteProgram (svprogfuncs, pr_global_struct->PutClientInServer);

						oh = host_client;
						SV_PreRunCmd();
						{
							usercmd_t cmd;
							memset(&cmd, 0, sizeof(cmd));
							cmd.msec = 0;
#define ANGLE2SHORT(x) (x) * (65536/360.0)
							cmd.angles[0] = ANGLE2SHORT(split->edict->v.v_angle[0]);
							cmd.angles[1] = ANGLE2SHORT(split->edict->v.v_angle[1]);
							cmd.angles[2] = ANGLE2SHORT(split->edict->v.v_angle[2]);
							SV_RunCmd(&cmd, false);
						}
						SV_PostRunCmd();
						host_client = oh;
					}
				}
	#ifdef Q2SERVER
				else
				{
					ge->ClientBegin(split->q2edict);
				}
	#endif
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
		if (!host_client->isq2client)
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
		MSG_WriteAngle (&host_client->netchan.message, host_client->edict->v.angles[1] );
		MSG_WriteAngle (&host_client->netchan.message, 0 );
	}
}

//=============================================================================

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
	r = fread (buffer, 1, r, host_client->download);
	ClientReliableWrite_Begin (host_client, host_client->isq2client?svcq2_download:svc_download, 6+r);
	ClientReliableWrite_Short (host_client, r);

	host_client->downloadcount += r;
	size = host_client->downloadsize;
	if (!size)
		size = 1;
	percent = host_client->downloadcount*100/size;

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

	if (host_client->downloadcount != host_client->downloadsize)
		return;

	fclose (host_client->download);
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
		host_client->upload = fopen(host_client->uploadfn, "wb");
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

	fwrite (net_message.data + msg_readcount, 1, size, host_client->upload);
	msg_readcount += size;

Con_DPrintf ("UPLOAD: %d received\n", size);

	if (percent != 100)
	{
		ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
		ClientReliableWrite_String (host_client, "nextul\n");
	}
	else
	{
		fclose (host_client->upload);
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
//	extern	int		file_from_pak; // ZOID did file come from pak?

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

	if (strncmp(name,	"maps/", 5) == 0 && (allow_download_maps.value))
		return true;

	//skins?
	if (strncmp(name,	"skins/", 6) == 0 && allow_download_skins.value)
		return true;
	//models
	if ((strncmp(name,	"progs/", 6) == 0||
		strncmp(name,	"models/", 7) == 0) && allow_download_models.value)
		return true;
	//sound
	if (strncmp(name,	"sound/", 6) == 0 && allow_download_sounds.value)
		return true;
	//demos
	if (strncmp(name,	"demos/", 6) == 0 && allow_download_demos.value)
		return true;

	//textures
	if (strncmp(name,	"texures/", 8) == 0 && allow_download_textures.value)
		return true;

	//wads
	if (strncmp(name,	"wads/", 5) == 0 && allow_download_wads.value)
		return true;
	if (!strcmp(".wad", COM_FileExtension(name)) && allow_download_wads.value)
		return true;

	//pk3s.
	if (!strcmp(".pk3", COM_FileExtension(name)) && allow_download_pk3s.value)
		if (strnicmp(name, "pak", 3))	//don't give out q3 pk3 files.
			return true;

	//root of gamedir
	if (!strchr(name, '/') && !allow_download_root.value)
		return false;

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
	char	*name;
	extern	cvar_t	allow_download_anymap;
	extern	int		file_from_pak; // ZOID did file come from pak?


	name = Cmd_Argv(1);
// hacked by zoid to allow more conrol over download
	if (!SV_AllowDownload(name))
	{	// don't allow anything with .. path
		ClientReliableWrite_Begin (host_client, host_client->isq2client?svcq2_download:svc_download, 4);
		ClientReliableWrite_Short (host_client, -1);
		ClientReliableWrite_Byte (host_client, 0);
		return;
	}

	if (host_client->download)
	{
		fclose (host_client->download);
		host_client->download = NULL;
	}

	// lowercase name (needed for casesen file systems)
	{
		char *p;

		for (p = name; *p; p++)
			*p = (char)tolower(*p);
	}

	host_client->downloadsize = COM_FOpenFile (name, &host_client->download);
	host_client->downloadcount = 0;

	if (!host_client->download
		// special check for maps, if it came from a pak file, don't allow
		// download  ZOID
		|| (!allow_download_anymap.value && strncmp(name, "maps/", 5) == 0 && file_from_pak))
	{
		if (host_client->download)
		{
			fclose(host_client->download);
			host_client->download = NULL;
		}

		Sys_Printf ("Couldn't download %s to %s\n", name, host_client->name);
		ClientReliableWrite_Begin (host_client, host_client->isq2client?svcq2_download:svc_download, 4);
		ClientReliableWrite_Short (host_client, -1);
		ClientReliableWrite_Byte (host_client, 0);
		return;
	}

	SV_NextDownload_f ();
	SV_EndRedirect();
	Con_Printf ("Downloading %s to %s\n", name, host_client->name);
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

/*
==================
SV_Say
==================
*/
void SV_Say (qboolean team)
{
	client_t *client;
	int		j, tmp;
	char	*p;
	char	text[1024];
	char	t1[32], *t2;
	int cls = 0;

	qboolean sent[MAX_CLIENTS];	//so we don't send to the same splitscreen connection twice. (it's ugly)
	int cln;

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

	if (fp_messages)
	{
		if (!sv.paused && realtime<host_client->lockedtill)
		{
			SV_ClientTPrintf(host_client, PRINT_CHAT,
				STL_FLOODPROTTIME, 
					(int) (host_client->lockedtill - realtime));
			return;
		}
		tmp = host_client->whensaidhead - fp_messages + 1;
		if (tmp < 0)
			tmp = 10+tmp;
		if (!sv.paused &&
			host_client->whensaid[tmp] && (realtime-host_client->whensaid[tmp] < fp_persecond))
		{
			host_client->lockedtill = realtime + fp_secondsdead;
			if (fp_msg[0])
				SV_ClientPrintf(host_client, PRINT_CHAT,
					"FloodProt: %s\n", fp_msg);
			else
				SV_ClientTPrintf(host_client, PRINT_CHAT,
					STL_FLOODPROTACTIVE, fp_secondsdead);
			return;
		}
		host_client->whensaidhead++;
		if (host_client->whensaidhead > 9)
			host_client->whensaidhead = 0;
		host_client->whensaid[host_client->whensaidhead] = realtime;
	}

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

	if (!sv.mvdrecording || !cls)
		return;

	// non-team messages should be seen allways, even if not tracking any player
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



/*
==================
SV_Kill_f
==================
*/
void SV_Kill_f (void)
{
	if (sv_player->v.health <= 0)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_NOSUISIDEWHENDEAD);
		return;
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
void SV_TogglePause (void)
{
	int i;
	client_t *cl;

	sv.paused ^= 1;

	// send notification to all clients
	for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (!cl->isq2client && !cl->controller)
		{
			ClientReliableWrite_Begin (cl, svc_setpause, 2);
			ClientReliableWrite_Byte (cl, sv.paused);
		}
	}
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

	SV_TogglePause();
	if (sv.paused)
		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTPAUSED, host_client->name);
	else
		SV_BroadcastTPrintf (PRINT_HIGH, STL_CLIENTUNPAUSED, host_client->name);

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
		ent->v.goalentity = EDICT_TO_PROG(svprogfuncs, tent);
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
		ent->v.goalentity = EDICT_TO_PROG(svprogfuncs, tent);
		return;
	}
	host_client->spec_track = i + 1; // now tracking

	ent = EDICT_NUM(svprogfuncs, host_client - svs.clients + 1);
	tent = EDICT_NUM(svprogfuncs, i + 1);
	ent->v.goalentity = EDICT_TO_PROG(svprogfuncs, tent);
}


/*
=================
SV_Rate_f

Change the bandwidth estimate for a client
=================
*/
void SV_Rate_f (void)
{
	extern cvar_t sv_maxrate;
	int		rate;
	
	if (Cmd_Argc() != 2)
	{
		SV_ClientTPrintf (host_client, PRINT_HIGH, STL_CURRENTRATE,
			host_client->rate);
		return;
	}
	
	rate = atoi(Cmd_Argv(1));
	if (rate < 500)
		rate = 500;
	if (rate > sv_maxrate.value)
		rate = sv_maxrate.value;

	SV_ClientTPrintf (host_client, PRINT_HIGH, STL_RATESETTO, rate);
	host_client->rate = rate;
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

/*
==================
SV_SetInfo_f

Allow clients to change userinfo
==================
*/
void SV_SetInfo_f (void)
{
	int i;
	char oldval[MAX_INFO_STRING];


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

	strcpy(oldval, Info_ValueForKey(host_client->userinfo, Cmd_Argv(1)));

	Info_SetValueForKey (host_client->userinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_INFO_STRING);
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
	{
		sv_player->v.team = atoi(Cmd_Argv(2))+1;
	}

	PR_ClientUserInfoChanged(Cmd_Argv(1), oldval, Cmd_Argv(2));

	if (*Cmd_Argv(1) == '_')
		return;

	i = host_client - svs.clients;
	MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
	MSG_WriteByte (&sv.reliable_datagram, i);
	MSG_WriteString (&sv.reliable_datagram, Cmd_Argv(1));
	MSG_WriteString (&sv.reliable_datagram, Info_ValueForKey(host_client->userinfo, Cmd_Argv(1)));
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

//Sets client to godmode
void Cmd_God_f (void)
{
	if ((int) (sv_player->v.flags = (int) sv_player->v.flags ^ FL_NOTARGET) & FL_NOTARGET)
		SV_ClientPrintf (host_client, PRINT_HIGH, "notarget ON\n");
	else
		SV_ClientPrintf (host_client, PRINT_HIGH, "notarget OFF\n");



	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if ((int) (sv_player->v.flags = (int) sv_player->v.flags ^ FL_GODMODE) & FL_GODMODE)
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
			sv_player->v.items = (int) sv_player->v.items | IT_SHOTGUN<< (t[0] - '2');
			break;

		case 's':
			sv_player->v.ammo_shells = v;
			break;		
		case 'n':
			sv_player->v.ammo_nails = v;
			break;		
		case 'r':
			sv_player->v.ammo_rockets = v;
			break;		
		case 'h':
			sv_player->v.health = v;
			break;		
		case 'c':
			sv_player->v.ammo_cells = v;
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

	if (sv_player->v.movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		sv_player->v.solid = SOLID_TRIGGER;
		SV_ClientPrintf (host_client, PRINT_HIGH, "noclip ON\n");
	}
	else
	{
		sv_player->v.movetype = MOVETYPE_WALK;
		if (sv_player->v.health > 0)
			sv_player->v.solid = SOLID_SLIDEBOX;
		else
			sv_player->v.solid = SOLID_NOT;
		SV_ClientPrintf (host_client, PRINT_HIGH, "noclip OFF\n");
	}
}

/*
====================
Host_SetPos_f  UDC
By Alex Shadowalker
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
		Con_Printf ("setpos %i %i %i\n", (int)sv_player->v.origin[0], (int)sv_player->v.origin[1], (int)sv_player->v.origin[2]);
		return;
	}
	if (sv_player->v.movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		Con_Printf("noclip on\n");
	}

	sv_player->v.origin[0] = atof(Cmd_Argv(1));
	sv_player->v.origin[1] = atof(Cmd_Argv(2));
	sv_player->v.origin[2] = atof(Cmd_Argv(3));
	SV_LinkEdict (sv_player, false);
}

void ED_ClearEdict (progfuncs_t *progfuncs, edict_t *e);
static void SetUpClientEdict (client_t *cl, edict_t *ent)
{
	extern int pr_teamfield;
	ED_ClearEdict(svprogfuncs, ent);
	ED_Spawned(ent);
	ent->isfree = false;
	
	ent->v.colormap = NUM_FOR_EDICT(svprogfuncs, ent);
	ent->v.netname = PR_SetString(svprogfuncs, cl->name);

	if (pr_teamfield)
		((string_t *)&ent->v)[pr_teamfield] = (string_t)PR_SetString(svprogfuncs, cl->team);


	ent->v.gravity = cl->entgravity = 1.0;
	ent->v.maxspeed = cl->maxspeed = sv_maxspeed.value;
	ent->v.movetype = MOVETYPE_NOCLIP;
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

	if (!(host_client->zquake_extensions & Z_EXT_JOIN_OBSERVE)) {
		Con_Printf ("Your QW client doesn't support this command\n");
		return;
	}

	if (password.string[0] && stricmp(password.string, "none")) {
		Con_Printf ("This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "player", "player");
		return;
	}

	// count players already on server
	numclients = 0;
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++) {
		if (cl->state != cs_free && !cl->spectator)
			numclients++;
	}
	if (numclients >= maxclients.value) {
		Con_Printf ("Can't join, all player slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	if (SpectatorDisconnect)
		PR_ExecuteProgram (svprogfuncs, SpectatorDisconnect);

	host_client->old_frags = 0;
	SetUpClientEdict (host_client, host_client->edict);

	// turn the spectator into a player
	host_client->spectator = false;
	Info_RemoveKey (host_client->userinfo, "*spectator");

	// FIXME, bump the client's userid?

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		host_client->spawn_parms[i] = (&pr_global_struct->parm1)[i];

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

	if (!(host_client->zquake_extensions & Z_EXT_JOIN_OBSERVE)) {
		Con_Printf ("Your QW client doesn't support this command\n");
		return;
	}

	if (spectator_password.string[0] && stricmp(spectator_password.string, "none")) {
		Con_Printf ("This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "spectator", "spectator");
		return;
	}

	// count spectators already on server
	numspectators = 0;
	for (i=0,cl=svs.clients ; i<sv.allocated_client_slots ; i++,cl++) {
		if (cl->state != cs_free && cl->spectator)
			numspectators++;
	}
	if (numspectators >= maxspectators.value) {
		Con_Printf ("Can't join, all spectator slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);

	host_client->old_frags = 0;
	SetUpClientEdict (host_client, host_client->edict);

	// turn the player into a spectator
	host_client->spectator = true;
	Info_SetValueForStarKey (host_client->userinfo, "*spectator", "1", MAX_INFO_STRING);

	// FIXME, bump the client's userid?

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetNewParms);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		host_client->spawn_parms[i] = (&pr_global_struct->parm1)[i];

	SV_SpawnSpectator ();
	
	// call the spawn function
	if (SpectatorConnect) {
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		PR_ExecuteProgram (svprogfuncs, SpectatorConnect);
	}

	// send notification to all clients
	host_client->sendinfo = true;
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

	{"snap", SV_NoSnap_f},
	{"vote", SV_Vote_f},

#ifdef SVRANKING
	{"topten", Rank_ListTop10_f},
#endif

	{"god", Cmd_God_f},
	{"give", Cmd_Give_f},
	{"noclip", Cmd_Noclip_f},
	{"setpos", Cmd_SetPos_f},

//	{"stopdownload", SV_StopDownload_f},
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

	{"setinfo", SV_SetInfo_f, true},

	{"serverinfo", SV_ShowServerinfo_f, true},

	{"download", SV_BeginDownload_f, true},
	{"nextdl", SV_NextDownload_f, true},

	{"nextserver", SVQ2_NextServer_f, true},

	{"vote", SV_Vote_f, true},

#ifdef SVRANKING
	{"topten", Rank_ListTop10_f, true},
#endif

	{"drop", SV_Drop_f, true},
	{"disconnect", SV_Drop_f, true},

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

	Con_DPrintf("Client command: %s\n", s);
	
	Cmd_TokenizeString (s);
	sv_player = host_client->edict;

	Cmd_ExecLevel=1;

	if (atoi(Cmd_Argv(0))>0)	//now see if it's meant to be from a slave client
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
		Cmd_ShiftArgs(1);
	}

#ifdef Q2SERVER
	if (host_client->isq2client)
		u = ucmdsq2;
	else
#endif
		u=ucmds;

	for ( ; u->name ; u++)
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			if (!fromQC && !u->noqchandling)
				if (PR_UserCmd(s))
				{
					host_client = oldhost;
					return;
				}
			SV_BeginRedirect (RD_CLIENT);
			u->func ();
			host_client = oldhost;
			SV_EndRedirect ();
			return;
		}

	if (!u->name)
	{
		if (!fromQC)
			if (PR_UserCmd(s))
			{
				host_client = oldhost;
				return;
			}

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

			SV_BeginRedirect (RD_CLIENT);

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
		Con_Printf ("Bad user command: %s\n", Cmd_Argv(0));
	}

	host_client = oldhost;
	SV_EndRedirect ();
}
#ifdef NQPROT
void SVNQ_Spawn_f (void)
{
	int		i;
	client_t	*client;
	edict_t	*ent;

	if (host_client->state != cs_connected)
	{
		Con_Printf ("spawn not valid -- allready spawned\n");
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
		ClientReliableWrite_Begin (host_client, svc_lightstyle, 
			3 + (sv.lightstyles[i] ? strlen(sv.lightstyles[i]) : 1));
		ClientReliableWrite_Byte (host_client, (char)i);
		ClientReliableWrite_String (host_client, sv.lightstyles[i]);
	}

	// set up the edict
	ent = host_client->edict;

	if (host_client->istobeloaded)	//minimal setup
	{
		host_client->entgravity = ent->v.gravity;
		host_client->maxspeed = ent->v.maxspeed;
	}
	else
	{
		memset (&ent->v, 0, pr_edict_size-svprogparms.edictsize);
		ED_Spawned(ent);

		ent->v.colormap = NUM_FOR_EDICT(svprogfuncs, ent);
		ent->v.team = 0;	// FIXME
		ent->v.netname = PR_SetString(svprogfuncs, host_client->name);

		host_client->entgravity = ent->v.gravity = 1.0;	
		host_client->maxspeed = ent->v.maxspeed = sv_maxspeed.value;
	}

//
// force stats to be updated
//
	memset (host_client->stats, 0, sizeof(host_client->stats));

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
					(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

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
				(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

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
		if (!host_client->isq2client)
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
		MSG_WriteAngle (&host_client->netchan.message, host_client->edict->v.angles[1] );
		MSG_WriteAngle (&host_client->netchan.message, 0 );
	}


	
	SZ_Write (&host_client->netchan.message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
	MSG_WriteByte (&host_client->netchan.message, 4);
	

	host_client->send_message = true;
}
void SVNQ_PreSpawn_f (void)
{
	edict_t *ent;
	entity_state_t *state;
	int i, e;
	if (host_client->state != cs_connected)
	{
		Con_Printf ("prespawn not valid -- allready spawned\n");
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
			MSG_WriteByte(&host_client->netchan.message, svc_spawnbaseline);

			MSG_WriteShort (&host_client->netchan.message, e);

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
	}
	
	for (i = 0; i < sv.num_signon_buffers; i++)
		SZ_Write (&host_client->netchan.message, sv.signon_buffers[i], sv.signon_buffer_size[i]);
	
	MSG_WriteByte (&host_client->netchan.message, svc_signonnum);
	MSG_WriteByte (&host_client->netchan.message, 2);
	

	host_client->send_message = true;
}
void SVNQ_NQInfo_f (void)
{
	Cmd_TokenizeString(va("setinfo \"%s\" \"%s\"\n", Cmd_Argv(0), Cmd_Argv(1)));
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
		host_client->edict->v.team = bottom + 1;

	Info_SetValueForKey(host_client->userinfo, "topcolor", va("%i", top), MAX_INFO_STRING);
	Info_SetValueForKey(host_client->userinfo, "bottomcolor", va("%i", bottom), MAX_INFO_STRING);
	switch(bottom)
	{
	case 4:
		Info_SetValueForKey(host_client->userinfo, "team", "red", MAX_INFO_STRING);
		break;
	case 14:
		Info_SetValueForKey(host_client->userinfo, "team", "blue", MAX_INFO_STRING);
		break;
	default:
		Info_SetValueForKey(host_client->userinfo, "team", va("t%i", bottom+1), MAX_INFO_STRING);
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

ucmd_t nqucmds[] =
{
	{"status",		NULL},

	{"god",			Cmd_God_f},
	{"give",		Cmd_Give_f},
	{"notarget",	NULL},
	{"fly",			NULL},
	{"noclip",		Cmd_Noclip_f},


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
	{"ping",		NULL},
	{"ban",			NULL},
	{"vote",		SV_Vote_f},

#ifdef SVRANKING
	{"topten",		Rank_ListTop10_f},
#endif
	
	{NULL, NULL}
};

void SVNQ_ExecuteUserCommand (char *s)
{
	client_t *oldhost = host_client;
	ucmd_t	*u;
	
	Cmd_TokenizeString (s);
	sv_player = host_client->edict;

	Cmd_ExecLevel=1;

	for (u=nqucmds ; u->name ; u++)
	{
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			if (/*!fromQC && */!u->noqchandling)
				if (PR_UserCmd(s))
				{
					host_client = oldhost;
					return;
				}

			if (!u->func)
			{
				SV_BeginRedirect (RD_CLIENT);
				Con_Printf("Command was disabled\n");
				SV_EndRedirect ();
			}
			else
			{
				SV_BeginRedirect (RD_CLIENT);
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
		Con_Printf("impfiltr.cfg not found. Impulse filters are disabled\n");

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
	float os, omt;

	hull_t *hull;
vec3_t offset;
	pl = EDICT_TO_PROG(svprogfuncs, sv_player);	

	// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->v.owner == pl)
			continue;		// player's own missile
		if (check->v.solid == SOLID_BSP 
			|| check->v.solid == SOLID_BBOX 
			|| check->v.solid == SOLID_SLIDEBOX)
		{
			if (check == sv_player)
				continue;

			for (i=0 ; i<3 ; i++)
				if (check->v.absmin[i] > pmove_maxs[i]
				|| check->v.absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;

			if (pmove.numphysent == MAX_PHYSENTS)
				break;
			pe = &pmove.physents[pmove.numphysent];
			pe->notouch = !((int)sv_player->v.dimension_solid & (int)check->v.dimension_hit);
			if (!((int)sv_player->v.dimension_hit & (int)check->v.dimension_solid))
				continue;
			pmove.numphysent++;

			VectorCopy (check->v.origin, pe->origin);
			VectorCopy (check->v.angles, pe->angles);
			pe->info = NUM_FOR_EDICT(svprogfuncs, check);
			if (check->v.solid == SOLID_BSP)
			{
				if(progstype != PROG_H2)
					pe->angles[0]*=-1;	//quake is wierd. I guess someone fixed it hexen2... or my code is buggy or something...
				pe->model = sv.models[(int)(check->v.modelindex)];
			}
			else
			{
				pe->model = NULL;
				VectorCopy (check->v.mins, pe->mins);
				VectorCopy (check->v.maxs, pe->maxs);
			}
		}
	}
	if (sv_player->v.mins[2] != 24)	//crouching/dead
	for (l = node->trigger_edicts.next ; l != &node->trigger_edicts ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->v.owner == pl)
			continue;		// player's own missile
		if (check->v.solid == SOLID_LADDER)
		{
			for (i=0 ; i<3 ; i++)
				if (check->v.absmin[i] > pmove_maxs[i]
				|| check->v.absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;

			if (!((int)sv_player->v.dimension_hit & (int)check->v.dimension_solid))
				continue;

//			check->v.model = "a";
			os = check->v.solid;
			omt = check->v.movetype;
			check->v.solid = SOLID_BSP;
			check->v.movetype = MOVETYPE_PUSH;
			hull = SV_HullForEntity (check, 0, sv_player->v.mins, sv_player->v.maxs, offset);			
			check->v.movetype = omt;
			check->v.solid = os;

	// test the point
			if ( hull->funcs.HullPointContents (hull, sv_player->v.origin) == FTECONTENTS_SOLID )
				sv_player->v.fteflags = (int)sv_player->v.fteflags | FF_LADDER;	//touch that ladder!
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
		if (check->v.owner == pl)
			continue;
		if (check->v.solid == SOLID_BSP 
			|| check->v.solid == SOLID_BBOX 
			|| check->v.solid == SOLID_SLIDEBOX)
		{
			if (check == sv_player)
				continue;

			for (i=0 ; i<3 ; i++)
				if (check->v.absmin[i] > pmove_maxs[i]
				|| check->v.absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;
			pe = &pmove.physents[pmove.numphysent];

			VectorCopy (check->v.origin, pe->origin);
			VectorCopy (check->v.angles, pe->angles);
			pmove.physents[pmove.numphysent].info = e;
			if (check->v.solid == SOLID_BSP)
				pe->model = sv.models[(int)(check->v.modelindex)];
			else
			{
				pe->angles[0] = pe->angles[1] = pe->angles[2] = 0;
				pe->model = NULL;
				VectorCopy (check->v.mins, pe->mins);
				VectorCopy (check->v.maxs, pe->maxs);
			}

			if (++pmove.numphysent == MAX_PHYSENTS)
				break;
		}
	}
}

int SV_PMTypeForClient (client_t *cl)
{
	if (cl->edict->v.movetype == MOVETYPE_NOCLIP)
	{
		if (cl->zquake_extensions & Z_EXT_PM_TYPE_NEW)
			return PM_SPECTATOR;
		return PM_OLD_SPECTATOR;
	}

	if (cl->edict->v.movetype == MOVETYPE_FLY)
		return PM_FLY;

	if (cl->edict->v.movetype == MOVETYPE_NONE)
	{
		if (sv_brokenmovetypes.value)
			return PM_NORMAL;
		return PM_NONE;
	}

	if (cl->edict->v.health <= 0)
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

#ifdef Q2SERVER
	if (!svprogfuncs)
	{
		ge->ClientThink (host_client->q2edict, ucmd);
		return;
	}
#endif


	
	// DMW copied this KK hack copied from QuakeForge anti-cheat
	// (also extra inside parm on all SV_RunCmds that follow)	

	// To prevent a infinite loop
	if (!recurse) {
		host_client->msecs += ucmd->msec;

		if ((tmp_time = realtime - host_client->last_check) >= sv_cheatspeedchecktime.value) {
			tmp_time = tmp_time * 1000.0 * sv_cheatpc.value/100.0;
		    if (host_client->msecs > tmp_time) {
				host_client->msec_cheating++;
				SV_BroadcastTPrintf(PRINT_HIGH, 
						STL_SPEEDCHEATPOSSIBLE,
							host_client->msecs, tmp_time,
							host_client->msec_cheating, host_client->name);

				if (host_client->msec_cheating >= 2) {
					SV_BroadcastTPrintf(PRINT_HIGH, 
							STL_SPEEDCHEATKICKED, 
								host_client->name, NET_AdrToString(host_client->netchan.remote_address));
					SV_DropClient(host_client);
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
		cmd.msec = oldmsec/2 + (oldmsec&1);	//give them back thier msec.
		cmd.impulse = 0;
		SV_RunCmd (&cmd, true);
		return;
	}

	host_frametime = ucmd->msec * 0.001;
	if (host_frametime > 0.1)
		host_frametime = 0.1;

#ifdef SVCHAT
	if (SV_ChatMove(ucmd->impulse))
	{
		ucmd->buttons = 0;
		ucmd->impulse = 0;
		ucmd->forwardmove = ucmd->sidemove = ucmd->upmove = 0;
	}
#endif

	if (!sv_player->v.fixangle)
	{
		sv_player->v.v_angle[0] = SHORT2ANGLE(ucmd->angles[0]);
		sv_player->v.v_angle[1] = SHORT2ANGLE(ucmd->angles[1]);
		sv_player->v.v_angle[2] = SHORT2ANGLE(ucmd->angles[2]);
	}

	if (progstype == PROG_H2)
		sv_player->v.light_level = 128;	//hmm... HACK!!!

	sv_player->v.button0 = ucmd->buttons & 1;
	sv_player->v.button2 = (ucmd->buttons >> 1) & 1;
	if (pr_allowbutton1.value)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
		sv_player->v.button1 = ((ucmd->buttons >> 2) & 1);
// DP_INPUTBUTTONS
	sv_player->v.button3 = ((ucmd->buttons >> 2) & 1);
	sv_player->v.button4 = ((ucmd->buttons >> 3) & 1);
	sv_player->v.button5 = ((ucmd->buttons >> 4) & 1);
	sv_player->v.button6 = ((ucmd->buttons >> 5) & 1);
	sv_player->v.button7 = ((ucmd->buttons >> 6) & 1);
	sv_player->v.button8 = ((ucmd->buttons >> 7) & 1);
	if (ucmd->impulse && SV_FiltureImpulse(ucmd->impulse, host_client->trustlevel))
		sv_player->v.impulse = ucmd->impulse;

	if (host_client->iscuffed)
	{
		sv_player->v.impulse = 0;
		sv_player->v.button0 = 0;
	}

	sv_player->v.movement[0] = ucmd->forwardmove * host_frametime;
	sv_player->v.movement[1] = ucmd->sidemove * host_frametime;
	sv_player->v.movement[2] = ucmd->upmove * host_frametime;

	SV_CheckVelocity(sv_player);

//
// angles
// show 1/3 the pitch angle and all the roll angle	
	if (sv_player->v.health > 0)
	{
		if (!sv_player->v.fixangle)
		{
			sv_player->v.angles[PITCH] = -sv_player->v.v_angle[PITCH]/3;
			sv_player->v.angles[YAW] = sv_player->v.v_angle[YAW];
		}
		sv_player->v.angles[ROLL] = 
			V_CalcRoll (sv_player->v.angles, sv_player->v.velocity)*4;
	}

	if (!host_client->spectator)
	{
		vec_t oldvz;
		pr_global_struct->frametime = host_frametime;

		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		oldvz = sv_player->v.velocity[2];
		if (progstype != PROG_QW)
		{
#define FL_JUMPRELEASED 4096
			jumpable = (int)sv_player->v.flags & FL_JUMPRELEASED;

			pmove.waterjumptime = sv_player->v.teleport_time;
			if (pmove.waterjumptime > sv.time)
				sv_player->v.flags = (int)sv_player->v.flags | FL_WATERJUMP;
			sv_player->v.teleport_time = sv.time + pmove.waterjumptime;
		}
		else
			jumpable = false;
		PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPreThink);
		
		if (progstype != PROG_QW)
		{
			if (sv_player->v.velocity[2] == 225 && sv_player->v.teleport_time > sv.time)
			{
				sv_player->v.velocity[2] = oldvz;
		//		Con_Printf("Waterjump detected\n");
			}
			sv_player->v.flags = (int)sv_player->v.flags & ~FL_WATERJUMP;
			sv_player->v.teleport_time = pmove.waterjumptime;
			if (jumpable && !((int)sv_player->v.flags & FL_JUMPRELEASED))	//hmm... a jump was hit.
				sv_player->v.velocity[2] -= 270;
		}

		SV_RunThink (sv_player);
	}

//	memset(&pmove, 0, sizeof(pmove));
//	memset(&movevars, 0, sizeof(movevars));

	player_mins[0] = sv_player->v.mins[0];
	player_mins[1] = sv_player->v.mins[1];
	player_mins[2] = sv_player->v.mins[2];

	player_maxs[0] = sv_player->v.maxs[0];
	player_maxs[1] = sv_player->v.maxs[1];
	player_maxs[2] = sv_player->v.maxs[2];

	for (i=0 ; i<3 ; i++)
		pmove.origin[i] = sv_player->v.origin[i];// + (sv_player->v.mins[i] - player_mins[i]);

	VectorCopy (sv_player->v.velocity, pmove.velocity);
	VectorCopy (sv_player->v.v_angle, pmove.angles);

	pmove.pm_type = SV_PMTypeForClient (host_client);
	pmove.jump_held = host_client->jump_held;
	pmove.jump_msec = 0;
	if (progstype != PROG_QW)	//this is just annoying.
		pmove.waterjumptime = sv_player->v.teleport_time - sv.time;
	else
		pmove.waterjumptime = sv_player->v.teleport_time;
	pmove.numphysent = 1;
	pmove.physents[0].model = sv.worldmodel;
	pmove.cmd = *ucmd;
	if (sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3)
		pmove.hullnum = ((int)sv_player->v.fteflags&FF_CROUCHING)?3:1;
	else
		pmove.hullnum = SV_HullNumForPlayer(sv_player->v.hull, sv_player->v.mins, sv_player->v.maxs);

	movevars.entgravity = host_client->entgravity;
	movevars.maxspeed = host_client->maxspeed;
	movevars.bunnyspeedcap = pm_bunnyspeedcap.value;
	movevars.ktjump = pm_ktjump.value;
	movevars.slidefix = (pm_slidefix.value != 0);
	movevars.airstep = (pm_airstep.value != 0);

	if (sv_player->v.hasted)
		movevars.maxspeed*=sv_player->v.hasted;

	for (i=0 ; i<3 ; i++)
	{
		pmove_mins[i] = pmove.origin[i] - 256;
		pmove_maxs[i] = pmove.origin[i] + 256;
	}
	sv_player->v.fteflags = (int)sv_player->v.fteflags & ~FF_LADDER;	//assume not touching ladder trigger
#if 1
	AddLinksToPmove ( sv_areanodes );
#else
	AddAllEntsToPmove ();
#endif

	if ((int)sv_player->v.fteflags & FF_LADDER)
		pmove.onladder = true;
	else
		pmove.onladder = false;

#if 0
{
	int before, after;

before = PM_TestPlayerPosition (pmove.origin);
	PlayerMove ();
after = PM_TestPlayerPosition (pmove.origin);

if (sv_player->v.health > 0 && before && !after )
	Con_Printf ("player %s got stuck in playermove!!!!\n", host_client->name);
}
#else
	PM_PlayerMove ();
#endif

	host_client->jump_held = pmove.jump_held;
	if (progstype != PROG_QW)	//this is just annoying.
		sv_player->v.teleport_time = pmove.waterjumptime + sv.time;
	else
		sv_player->v.teleport_time = pmove.waterjumptime;
	sv_player->v.waterlevel = pmove.waterlevel;

	if (pmove.watertype & FTECONTENTS_SOLID)
		sv_player->v.watertype = Q1CONTENTS_SOLID;
	else if (pmove.watertype & FTECONTENTS_SKY)
		sv_player->v.watertype = Q1CONTENTS_SKY;
	else if (pmove.watertype & FTECONTENTS_LAVA)
		sv_player->v.watertype = Q1CONTENTS_LAVA;
	else if (pmove.watertype & FTECONTENTS_SLIME)
		sv_player->v.watertype = Q1CONTENTS_SLIME;
	else if (pmove.watertype & FTECONTENTS_WATER)
		sv_player->v.watertype = Q1CONTENTS_WATER;
	else
		sv_player->v.watertype = Q1CONTENTS_EMPTY;

	if (pmove.onground)
	{
		sv_player->v.flags = (int)sv_player->v.flags | FL_ONGROUND;
		sv_player->v.groundentity = EDICT_TO_PROG(svprogfuncs, EDICT_NUM(svprogfuncs, pmove.physents[pmove.groundent].info));
	}
	else
		sv_player->v.flags = (int)sv_player->v.flags & ~FL_ONGROUND;

	for (i=0 ; i<3 ; i++)
		sv_player->v.origin[i] = pmove.origin[i];// - (sv_player->v.mins[i] - player_mins[i]);

#if 0
	// truncate velocity the same way the net protocol will
	for (i=0 ; i<3 ; i++)
		sv_player->v.velocity[i] = (int)pmove.velocity[i];
#else
	VectorCopy (pmove.velocity, sv_player->v.velocity);
#endif

	VectorCopy (pmove.angles, sv_player->v.v_angle);

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
			if (pmove.physents[pmove.touchindex[i]].notouch)
				continue;
			n = pmove.physents[pmove.touchindex[i]].info;
			ent = EDICT_NUM(svprogfuncs, n);
			if (!ent->v.touch || (playertouch[n/8]&(1<<(n%8))))
				continue;

			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
			pr_global_struct->other = EDICT_TO_PROG(svprogfuncs, sv_player);
			PR_ExecuteProgram (svprogfuncs, ent->v.touch);
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

	if (!host_client->spectator) {
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);

		PR_ExecuteProgram (svprogfuncs, pr_global_struct->PlayerPostThink);

		SV_RunNewmis ();
	} else if (SpectatorThink) {
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		PR_ExecuteProgram (svprogfuncs, SpectatorThink);
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
	int		seq_hash;

	// calc ping time
	if (cl->frames)
	{	//split screen doesn't always have frames.
		frame = &cl->frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];
		frame->ping_time = realtime - frame->senttime;
	}
	else
		return;	//shouldn't happen...

	// make sure the reply sequence number matches the incoming
	// sequence number 
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped		

	// save time for ping calculations
	if (cl->frames)
	{	//split screen doesn't always have frames.
		cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = realtime;
		cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;
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

							for (; cl; cl = cl->controlled)	//FIXME
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
						if (sv_nomsec.value || SV_PlayerPhysicsQC)
						{
							if (!sv_player->v.fixangle)
							{
								sv_player->v.v_angle[0] = newcmd.angles[0]* (360.0/65536);
								sv_player->v.v_angle[1] = newcmd.angles[1]* (360.0/65536);
								sv_player->v.v_angle[2] = newcmd.angles[2]* (360.0/65536);
							}

							if (newcmd.impulse)// && SV_FiltureImpulse(newcmd.impulse, host_client->trustlevel))
								sv_player->v.impulse = newcmd.impulse;

							sv_player->v.button0 = newcmd.buttons & 1;
							sv_player->v.button2 = (newcmd.buttons >> 1) & 1;
							if (pr_allowbutton1.value)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
								sv_player->v.button1 = ((newcmd.buttons >> 2) & 1);
						// DP_INPUTBUTTONS
							sv_player->v.button3 = ((newcmd.buttons >> 2) & 1);
							sv_player->v.button4 = ((newcmd.buttons >> 3) & 1);
							sv_player->v.button5 = ((newcmd.buttons >> 4) & 1);
							sv_player->v.button6 = ((newcmd.buttons >> 5) & 1);
							sv_player->v.button7 = ((newcmd.buttons >> 6) & 1);
							sv_player->v.button8 = ((newcmd.buttons >> 7) & 1);


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
			if (host_client->spectator) {
				VectorCopy(o, sv_player->v.origin);
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

	// calc ping time
	frame = &cl->q2frames[cl->netchan.incoming_acknowledged & Q2UPDATE_MASK];

	// make sure the reply sequence number matches the incoming
	// sequence number 
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped		

	// save time for ping calculations
	cl->q2frames[cl->netchan.outgoing_sequence & Q2UPDATE_MASK].senttime = realtime;
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
				/*if (sv_nomsec.value)
				{
					cmd = newcmd;
					SV_ClientThink ();

					cl->lastcmd = newcmd;
					cl->lastcmd.buttons = 0; // avoid multiple fires on lag
					break;
				}*/
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

				SV_PostRunCmd();
			}

			cl->lastcmd = newcmd;
			cl->lastcmd.buttons = 0; // avoid multiple fires on lag
			break;

		case clcq2_userinfo:
			s = MSG_ReadString ();
			break;

		case clcq2_stringcmd:	
			s = MSG_ReadString ();
			SV_ExecuteUserCommand (s, false);

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
	
	MSG_ReadFloat ();	//message time (nq uses it to time pings - qw ignores it)
	

// read current angles	
	for (i=0 ; i<3 ; i++)
	{
		host_client->edict->v.v_angle[i] = MSG_ReadAngle ();

		move->angles[i] = (host_client->edict->v.v_angle[i] * 256*256)/360;
	}
		
// read movement
	move->forwardmove = MSG_ReadShort ();
	move->sidemove = MSG_ReadShort ();
	move->upmove = MSG_ReadShort ();

	move->msec=100;
	
// read buttons
	bits = MSG_ReadByte ();
	move->buttons = bits;	

	i = MSG_ReadByte ();
	if (i)
		move->impulse = i;




	if (i && SV_FiltureImpulse(i, host_client->trustlevel))
		host_client->edict->v.impulse = i;

	host_client->edict->v.button0 = bits & 1;
	host_client->edict->v.button2 = (bits >> 1) & 1;
	if (pr_allowbutton1.value)	//many mods use button1 - it's just a wasted field to many mods. So only work it if the cvar allows.
		host_client->edict->v.button1 = ((bits >> 2) & 1);
// DP_INPUTBUTTONS
	host_client->edict->v.button3 = ((bits >> 2) & 1);
	host_client->edict->v.button4 = ((bits >> 3) & 1);
	host_client->edict->v.button5 = ((bits >> 4) & 1);
	host_client->edict->v.button6 = ((bits >> 5) & 1);
	host_client->edict->v.button7 = ((bits >> 6) & 1);
	host_client->edict->v.button8 = ((bits >> 7) & 1);
}

void SVNQ_ExecuteClientMessage (client_t *cl)
{
	int		c;
	char	*s;
	client_frame_t	*frame;	
	int		seq_hash;

	MSG_BeginReading ();

	// calc ping time
	frame = &cl->frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];
	frame->ping_time = realtime - frame->senttime;

	// make sure the reply sequence number matches the incoming
	// sequence number 
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped		

	// save time for ping calculations
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = realtime;
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;

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
			SVNQ_ExecuteUserCommand (s);
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

	Cvar_Register (&sv_cmdlikercon, cvargroup_serverpermissions);

	Cvar_Register (&votelevel, sv_votinggroup);
	Cvar_Register (&voteminimum, sv_votinggroup);
	Cvar_Register (&votepercent, sv_votinggroup);
	Cvar_Register (&votetime, sv_votinggroup);

	Cvar_Register (&sv_brokenmovetypes, "");

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
	start[2] = origin[2] + sv_player->v.mins[2];
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

	AngleVectors (sv_player->v.angles, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;
	
// hack to not let you back into teleporter
	if (sv.time < sv_player->v.teleport_time && fmove < 0)
		fmove = 0;
		
	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*fmove + right[i]*smove;

	if ( (int)sv_player->v.movetype != MOVETYPE_WALK)
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

	maxspeed=sv_player->v.maxspeed;//FIXME: This isn't fully compatable code...

	if (wishspeed > maxspeed*scale)
	{
		VectorScale (wishvel, maxspeed/wishspeed, wishvel);
		wishspeed = maxspeed*scale;
	}
	
	if ( sv_player->v.movetype == MOVETYPE_NOCLIP)
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
	AngleVectors (sv_player->v.v_angle, forward, right, up);

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
	if (sv.time > sv_player->v.teleport_time
	|| !sv_player->v.waterlevel)
	{
		sv_player->v.flags = (int)sv_player->v.flags & ~FL_WATERJUMP;
		sv_player->v.teleport_time = 0;
	}
	sv_player->v.velocity[0] = sv_player->v.movedir[0];
	sv_player->v.velocity[1] = sv_player->v.movedir[1];
}



void SV_ClientThink (void)
{
	vec3_t		v_angle;

	cmd = host_client->lastcmd;
	sv_player = host_client->edict;

	sv_player->v.movement[0] = cmd.forwardmove;
	sv_player->v.movement[1] = cmd.sidemove;
	sv_player->v.movement[2] = cmd.upmove;

	if (SV_PlayerPhysicsQC)
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, sv_player);
		PR_ExecuteProgram (svprogfuncs, SV_PlayerPhysicsQC);
		return;
	}

	if (sv_player->v.movetype == MOVETYPE_NONE)
		return;
	
	onground = (int)sv_player->v.flags & FL_ONGROUND;

	origin = sv_player->v.origin;
	velocity = sv_player->v.velocity;

//	DropPunchAngle ();
	
//
// if dead, behave differently
//
	if (sv_player->v.health <= 0)
		return;

//
// angles
// show 1/3 the pitch angle and all the roll angle
	angles = sv_player->v.angles;
	
	VectorCopy (sv_player->v.v_angle, v_angle);
//	VectorAdd (sv_player->v.v_angle, sv_player->v.punchangle, v_angle);
	angles[ROLL] = V_CalcRoll (sv_player->v.angles, sv_player->v.velocity)*4;
	if (!sv_player->v.fixangle)
	{
		angles[PITCH] = -v_angle[PITCH]/3;
		angles[YAW] = v_angle[YAW];
	}

	if ( (int)sv_player->v.flags & FL_WATERJUMP )
	{
		SV_WaterJump ();
		return;
	}
//
// walk
//
	if ( (sv_player->v.waterlevel >= 2)
	&& (sv_player->v.movetype != MOVETYPE_NOCLIP) )
	{
		SV_WaterMove ();
		return;
	}

	SV_AirMove ();
}

#endif
