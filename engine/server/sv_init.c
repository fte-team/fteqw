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
extern int			total_loading_size, current_loading_size, loading_stage;
char *T_GetString(int num);

#define Q2EDICT_NUM(i) (q2edict_t*)((char *)ge->edicts+i*ge->edict_size)

server_static_t	svs;				// persistant server info
server_t		sv;					// local server

char	localmodels[MAX_MODELS][5];	// inline model names for precache

char localinfo[MAX_LOCALINFO_STRING+1]; // local game info

extern cvar_t	skill, sv_loadentfiles;
extern cvar_t	sv_cheats;
extern cvar_t	sv_bigcoords;
extern qboolean	sv_allow_cheats;

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex (char *name)
{
	int		i;
	
	if (!name || !name[0])
		return 0;

	for (i=1 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i==MAX_MODELS || !sv.model_precache[i])
	{
		if (i!=MAX_MODELS && sv.state == ss_loading)
		{
			Q_strncpyz(sv.model_precache[i], name, sizeof(sv.model_precache[i]));
			if (!strcmp(name + strlen(name) - 4, ".bsp"))
				sv.models[i] = Mod_FindName(sv.model_precache[i]);
			Con_Printf("WARNING: SV_ModelIndex: model %s not precached", name);
		}
		else
			SV_Error ("SV_ModelIndex: model %s not precached", name);
	}
	return i;
}

int SV_SafeModelIndex (char *name)
{
	int		i;
	
	if (!name || !name[0])
		return 0;

	for (i=1 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i==MAX_MODELS || !sv.model_precache[i])
	{
		return 0;
	}
	return i;
}

/*
================
SV_FlushSignon

Moves to the next signon buffer if needed
================
*/
void SV_FlushSignon (void)
{
	if (sv.signon.cursize < sv.signon.maxsize - 512)
		return;

	if (sv.num_signon_buffers == MAX_SIGNON_BUFFERS-1)
		SV_Error ("sv.num_signon_buffers == MAX_SIGNON_BUFFERS-1");

	sv.signon_buffer_size[sv.num_signon_buffers-1] = sv.signon.cursize;
	sv.signon.data = sv.signon_buffers[sv.num_signon_buffers];
	sv.num_signon_buffers++;
	sv.signon.cursize = 0;
}
void SV_FlushDemoSignon (void)
{
	if (sv.demosignon.cursize < sv.demosignon.maxsize - 512)
		return;

	if (sv.num_demosignon_buffers == MAX_SIGNON_BUFFERS-1)
		SV_Error ("sv.num_demosignon_buffers == MAX_SIGNON_BUFFERS-1");

	sv.demosignon_buffer_size[sv.num_demosignon_buffers-1] = sv.demosignon.cursize;
	sv.demosignon.data = sv.demosignon_buffers[sv.num_demosignon_buffers];
	sv.num_demosignon_buffers++;
	sv.demosignon.cursize = 0;
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
/*void SV_CreateBaseline (void)
{
	int			i;
	edict_t			*svent;
	int				entnum;	
		
	for (entnum = 0; entnum < sv.num_edicts ; entnum++)
	{
		svent = EDICT_NUM(entnum);
		if (svent->free)
			continue;
		// create baselines for all player slots,
		// and any other edict that has a visible model
		if (entnum > MAX_CLIENTS && !svent->v.modelindex)
			continue;

	//
	// create entity baseline
	//
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skinnum = svent->v.skin;
		if (entnum > 0 && entnum <= MAX_CLIENTS)
		{
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl")&255;
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex =
				SV_ModelIndex(PR_GetString(svent->v.model))&255;
		}
#ifdef PEXT_SCALE
		svent->baseline.scale = 1;
#endif
#ifdef PEXT_TRANS
		svent->baseline.trans = 1;
#endif

		//
		// flush the signon message out to a seperate buffer if
		// nearly full
		//
		SV_FlushSignon ();

		//
		// add to the message
		//
		MSG_WriteByte (&sv.signon,svc_spawnbaseline);		
		MSG_WriteShort (&sv.signon,entnum);

		MSG_WriteByte (&sv.signon, svent->baseline.modelindex);
		MSG_WriteByte (&sv.signon, svent->baseline.frame);
		MSG_WriteByte (&sv.signon, svent->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->baseline.skinnum);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, svent->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->baseline.angles[i]);
		}
	}
}
*/
void SVNQ_CreateBaseline (void)
{
	edict_t			*svent;
	int				entnum;	

	int playermodel = SV_SafeModelIndex("progs/player.mdl");
		
	for (entnum = 0; entnum < sv.num_edicts ; entnum++)
	{
		svent = EDICT_NUM(svprogfuncs, entnum);

		memset(&svent->baseline, 0, sizeof(entity_state_t));
		svent->baseline.number = entnum;

#ifdef PEXT_SCALE
		svent->baseline.scale = 1;
#endif
#ifdef PEXT_TRANS
		svent->baseline.trans = 1;
#endif

		if (svent->isfree)
			continue;
		// create baselines for all player slots,
		// and any other edict that has a visible model
		if (entnum > sv.allocated_client_slots && !svent->v.modelindex)
			continue;

	//
	// create entity baseline
	//
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skinnum = svent->v.skin;
		if (entnum > 0 && entnum <= sv.allocated_client_slots)
		{
			if (entnum > 0 && entnum <= 16)
				svent->baseline.colormap = entnum;
			else
				svent->baseline.colormap = 0;	//this would crash NQ.

			svent->baseline.modelindex = playermodel;
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex =
				SV_ModelIndex(PR_GetString(svprogfuncs, svent->v.model));
		}
		svent->baseline.modelindex&=255;
	}
}


/*
================
SV_SaveSpawnparms

Grabs the current state of the progs serverinfo flags 
and each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (qboolean dontsave)
{
	int		i, j;

	if (!sv.state)
		return;		// no progs loaded yet

	if (!svprogfuncs)
		return;

	// serverflags is the only game related thing maintained
	svs.serverflags = pr_global_struct->serverflags;

	for (i=0, host_client = svs.clients ; i<MAX_CLIENTS ; i++, host_client++)
	{
		if (host_client->state != cs_spawned)
			continue;

		// needs to reconnect
		host_client->state = cs_connected;

		if (dontsave)	//level restart requires that stats can be reset
			continue;

		// call the progs to get default spawn parms for the new client
		if (PR_FindGlobal(svprogfuncs, "ClientReEnter", 0))
		{//oooh, evil.
			char buffer[65536];
			int bufsize = sizeof(buffer);
			char *buf;
			for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
				host_client->spawn_parms[j] = 0;
			
			buf = svprogfuncs->saveent(svprogfuncs, buffer, &bufsize, host_client->edict);

			if (host_client->spawninfo)
				Z_Free(host_client->spawninfo);
			host_client->spawninfo = Z_Malloc(bufsize+1);
			memcpy(host_client->spawninfo, buf, bufsize);
			host_client->spawninfotime = sv.time;
		}
		else if (pr_nqglobal_struct->SetChangeParms)
		{
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, host_client->edict);
			PR_ExecuteProgram (svprogfuncs, pr_global_struct->SetChangeParms);
			for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
				host_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
		}

#ifdef SVRANKING
		if (host_client->rankid)
		{
			rankstats_t rs;
			if (Rank_GetPlayerStats(host_client->rankid, &rs))
			{
				rs.timeonserver += realtime - host_client->stats_started;
				host_client->stats_started = realtime;
				rs.kills += host_client->kills;
				rs.deaths += host_client->deaths;
				host_client->kills=0;
				host_client->deaths=0;
				for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
					rs.parm[j] = (&pr_global_struct->parm1)[j];
				Rank_SetPlayerStats(host_client->rankid, &rs);
			}
		}
#endif
	}
}

/*
================
SV_CalcPHS

Expands the PVS and calculates the PHS
(Potentially Hearable Set)
================
*/
void SV_CalcPHS (void)
{
	int		rowbytes, rowwords;
	int		i, j, k, l, index, num;
	int		bitbyte;
	unsigned	*dest, *src;
	qbyte	*scan;
	int		count, vcount;

	if (sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3)
	{
		//PHS calcs are pointless with Q2 bsps
		return;
	}

	if (developer.value)
		Con_TPrintf (STL_BUILDINGPHS);

	num = sv.worldmodel->numleafs;
	rowwords = (num+31)>>5;
	rowbytes = rowwords*4;

	sv.pvs = Hunk_AllocName (rowbytes*num, "phs vis");
	scan = sv.pvs;
	vcount = 0;
	for (i=0 ; i<num ; i++, scan+=rowbytes)
	{
		memcpy (scan, sv.worldmodel->funcs.LeafPVS(i, sv.worldmodel, NULL),
			rowbytes);
		if (i == 0)
			continue;
		for (j=0 ; j<num ; j++)
		{
			if ( scan[j>>3] & (1<<(j&7)) )
			{
				vcount++;
			}
		}
	}


	sv.phs = Hunk_AllocName (rowbytes*num, "phs hear");
	count = 0;
	scan = sv.pvs;
	dest = (unsigned *)sv.phs;
	for (i=0 ; i<num ; i++, dest += rowwords, scan += rowbytes)
	{
		memcpy (dest, scan, rowbytes);
		for (j=0 ; j<rowbytes ; j++)
		{
			bitbyte = scan[j];
			if (!bitbyte)
				continue;
			for (k=0 ; k<8 ; k++)
			{
				if (! (bitbyte & (1<<k)) )
					continue;
				// or this pvs row into the phs
				// +1 because pvs is 1 based
				index = ((j<<3)+k+1);
				if (index >= num)
					continue;
				src = (unsigned *)sv.pvs + index*rowwords;
				for (l=0 ; l<rowwords ; l++)
					dest[l] |= src[l];
			}
		}

		if (i == 0)
			continue;
		for (j=0 ; j<num ; j++)
			if ( ((qbyte *)dest)[j>>3] & (1<<(j&7)) )
				count++;
	}

	if (num)
		if (developer.value)
			Con_TPrintf (STL_PHSINFO, vcount/num, count/num, num);
}

unsigned SV_CheckModel(char *mdl)
{
	qbyte	stackbuf[1024];		// avoid dirtying the cache heap
	qbyte *buf;
	unsigned short crc;
//	int len;

	buf = (qbyte *)COM_LoadStackFile (mdl, stackbuf, sizeof(stackbuf));
	if (!buf)
		return 0;
	crc = CRC_Block(buf, com_filesize);
//	for (len = com_filesize; len; len--, buf++)
//		CRC_ProcessByte(&crc, *buf);

	return crc;
}

void SV_UnspawnServer (void)	//terminate the running server.
{
	int i;
	if (sv.state)
	{
		Con_TPrintf(STL_SERVERUNSPAWNED);
		SV_FinalMessage("Server unspawned\n");

		for (i = 0; i < sv.allocated_client_slots; i++)
		{
			if (svs.clients[i].state)
				SV_DropClient(svs.clients+i);
		}
#ifdef Q2SERVER
		SVQ2_ShutdownGameProgs();
#endif
		sv.worldmodel = NULL;
		sv.state = ss_dead;
		*sv.name = '\0';
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		svs.clients[i].state = 0;
		*svs.clients[i].name = '\0';
	}
	NET_CloseServer ();
}

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

This is only called from the SV_Map_f() function.
================
*/
extern cvar_t	progs;
void SV_SpawnServer (char *server, char *startspot, qboolean noents, qboolean usecinematic)
{
	func_t f;
	char *file;

	gametype_e oldgametype;

	edict_t		*ent;
#ifdef Q2SERVER
	q2edict_t		*q2ent;
#endif
	int			i;
	int spawnflagmask;

#ifndef SERVERONLY
	if (!isDedicated && !qrenderer)
	{
		R_RestartRenderer_f();
	}
#endif

	Con_DPrintf ("SpawnServer: %s\n",server);

	svs.spawncount++;		// any partially connected client will be
							// restarted

#ifndef SERVERONLY
	total_loading_size = 100;
	current_loading_size = 0;
	loading_stage = 1;
	SCR_BeginLoadingPlaque();
#endif

	NET_InitServer();

	sv.state = ss_dead;

	if (sv.gamedirchanged)
	{
		sv.gamedirchanged = false;
#ifndef SERVERONLY
		Wads_Flush();	//server code is responsable for flushing old state
#endif
		Rank_Flush();

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (svs.clients[i].state
#ifdef NQPROT
                 && !svs.clients[i].nqprot
#endif
                 )
				ReloadRanking(&svs.clients[i], svs.clients[i].name);

			if (svs.clients[i].spawninfo)	//don't remember this stuff.
				Z_Free(svs.clients[i].spawninfo);
			svs.clients[i].spawninfo = NULL;
		}
		T_FreeStrings();
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		svs.clients[i].nextservertimeupdate = 0;
		if (!svs.clients[i].state)	//bots with the net_preparse module.
			svs.clients[i].userinfo[0] = '\0';	//clear the userinfo to clear the name

		if (svs.clients[i].netchan.remote_address.type == NA_LOOPBACK)
		{	//forget this client's message buffers, so that any shared client/server network state persists (eg: float coords)
			svs.clients[i].num_backbuf = 0;
			svs.clients[i].datagram.cursize = 0;
		}
	}

	if (sv_bigcoords.value)
		sizeofcoord = 4;
	else
		sizeofcoord = 2;

	sizeofangle = 1;

	VoteFlushAll();
#ifndef SERVERONLY
	D_FlushCaches();
	cl.worldmodel = NULL;
#endif
	Mod_ClearAll ();
	Hunk_FreeToLowMark (host_hunklevel);

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.lightstyles[i])
			Z_Free(sv.lightstyles[i]);
		sv.lightstyles[i] = NULL;
	}

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof(sv));

	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.datagram.data = sv.datagram_buf;
	sv.datagram.allowoverflow = true;


	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.multicast.maxsize = sizeof(sv.multicast_buf);
	sv.multicast.data = sv.multicast_buf;

#ifdef NQPROT
	sv.nqdatagram.maxsize = sizeof(sv.nqdatagram_buf);
	sv.nqdatagram.data = sv.nqdatagram_buf;
	sv.nqdatagram.allowoverflow = true;

	sv.nqreliable_datagram.maxsize = sizeof(sv.nqreliable_datagram_buf);
	sv.nqreliable_datagram.data = sv.nqreliable_datagram_buf;
	
	sv.nqmulticast.maxsize = sizeof(sv.nqmulticast_buf);
	sv.nqmulticast.data = sv.nqmulticast_buf;
#endif

	sv.q2datagram.maxsize = sizeof(sv.q2datagram_buf);
	sv.q2datagram.data = sv.q2datagram_buf;
	sv.q2datagram.allowoverflow = true;

	sv.q2reliable_datagram.maxsize = sizeof(sv.q2reliable_datagram_buf);
	sv.q2reliable_datagram.data = sv.q2reliable_datagram_buf;
	
	sv.q2multicast.maxsize = sizeof(sv.q2multicast_buf);
	sv.q2multicast.data = sv.q2multicast_buf;

	sv.master.maxsize = sizeof(sv.master_buf);
	sv.master.data = sv.master_buf;
	
	sv.signon.maxsize = sizeof(sv.signon_buffers[0]);
	sv.signon.data = sv.signon_buffers[0];
	sv.num_signon_buffers = 1;
	sv.numextrastatics = 0;

	strcpy (sv.name, server);
#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif

	Cvar_ApplyLatches(CVAR_LATCH);

	sv.time = 0.1;	//some progs don't like time starting at 0.
					//cos of spawn funcs like self.nextthink = time...

	COM_FlushTempoaryPacks();

	if (usecinematic)
	{
		strcpy (sv.name, server);
		strcpy (sv.modelname, "");
	}
	else
	{
		strcpy (sv.name, server);
		sprintf (sv.modelname,"maps/%s.bsp", server);
	}
	sv.state = ss_loading;
	sv.worldmodel = Mod_ForName (sv.modelname, true);
	if (sv.worldmodel->needload)
		Sys_Error("%s is missing\n", sv.modelname);
	sv.state = ss_dead;

#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif
	SV_CalcPHS ();
#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif

	if (sv.worldmodel->fromgame == fg_doom)
		Info_SetValueForStarKey(svs.info, "*bspversion", "1", MAX_SERVERINFO_STRING);
	else if (sv.worldmodel->fromgame == fg_halflife)
		Info_SetValueForStarKey(svs.info, "*bspversion", "30", MAX_SERVERINFO_STRING);
	else if (sv.worldmodel->fromgame == fg_quake2)
		Info_SetValueForStarKey(svs.info, "*bspversion", "38", MAX_SERVERINFO_STRING);
	else if (sv.worldmodel->fromgame == fg_quake3)
		Info_SetValueForStarKey(svs.info, "*bspversion", "46", MAX_SERVERINFO_STRING);
	else
		Info_SetValueForStarKey(svs.info, "*bspversion", "", MAX_SERVERINFO_STRING);

	if (startspot)
		Info_SetValueForStarKey(svs.info, "*startspot", startspot, MAX_SERVERINFO_STRING);
	else
		Info_SetValueForStarKey(svs.info, "*startspot", "", MAX_SERVERINFO_STRING);

	//
	// clear physics interaction links
	//
	SV_ClearWorld ();

	strcpy(sv.sound_precache[0], "");
	strcpy(sv.model_precache[0], "");

	strcpy(sv.model_precache[1], sv.modelname);
	sv.models[1] = sv.worldmodel;
	for (i=1 ; i<sv.worldmodel->numsubmodels ; i++)
	{
		strcpy(sv.model_precache[1+i], localmodels[i]);
		sv.models[i+1] = Mod_ForName (localmodels[i], false);
	}

	//check player/eyes models for hacks
	sv.model_player_checksum = SV_CheckModel("progs/player.mdl");
	sv.eyes_player_checksum = SV_CheckModel("progs/eyes.mdl");

	if (sv_cheats.value)
	{
		sv_allow_cheats = true;
		Info_SetValueForStarKey(svs.info, "*cheats", "ON", MAX_SERVERINFO_STRING);
	}
	else
	{
		sv_allow_cheats = false;
		Info_SetValueForStarKey(svs.info, "*cheats", "", MAX_SERVERINFO_STRING);
	}

	//do we allow csprogs?
#ifdef PEXT_CSQC
	file = COM_LoadTempFile("csprogs.dat");
	if (file)
	{
		char text[64];
		sprintf(text, "0x%x", Com_BlockChecksum(file, com_filesize));
		Info_SetValueForStarKey(svs.info, "*csprogs", text, MAX_SERVERINFO_STRING);
	}
	else
		Info_SetValueForStarKey(svs.info, "*csprogs", "", MAX_SERVERINFO_STRING);
#endif
	if (svprogfuncs)	//we don't want the q1 stuff anymore.
	{
		CloseProgs(svprogfuncs);
		svprogfuncs = NULL;
	}

	sv.state = ss_loading;

	oldgametype = svs.gametype;
#ifdef Q3SERVER
	if (SVQ3_InitGame())
		svs.gametype = GT_QUAKE3;
	else
#endif
#ifdef Q2SERVER
	if ((sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3) && !*progs.string && SVQ2_InitGameProgs())	//these are the rules for running a q2 server
		svs.gametype = GT_QUAKE2;	//we loaded the dll
	else
#endif
	{
		svs.gametype = GT_PROGS;	//let's just hope this loads.
		Q_InitProgs();
	}

#ifdef Q3SERVER
	if (svs.gametype != GT_QUAKE3)
		SVQ3_ShutdownGame();
#endif
#ifdef Q2SERVER
	if (svs.gametype != GT_QUAKE2)	//we don't want the q2 stuff anymore.
		SVQ2_ShutdownGameProgs ();
#endif

//	if ((sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3) && !*progs.string && SVQ2_InitGameProgs())	//full q2 dll decision in one if statement

	if (oldgametype != svs.gametype)
	{
		for (i=0 ; i<MAX_CLIENTS ; i++)	//server type changed, so we need to drop all clients. :(
		{
			if (svs.clients[i].state)
				SV_DropClient(&svs.clients[i]);

			svs.clients[i].name[0] = '\0';						//kill all bots
		}
	}

#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif

	switch (svs.gametype)
	{
	case GT_PROGS:
		ent = EDICT_NUM(svprogfuncs, 0);
		ent->isfree = false;
		
		// leave slots at start for clients only
	//	sv.num_edicts = MAX_CLIENTS+1;
		for (i=0 ; i<MAX_CLIENTS ; i++)
		{
			svs.clients[i].viewent = 0;

			ent = ED_Alloc(svprogfuncs);//EDICT_NUM(i+1);
			svs.clients[i].edict = ent;
			ent->isfree = false;
	//ZOID - make sure we update frags right
			svs.clients[i].old_frags = 0;

			if (!svs.clients[i].state && svs.clients[i].name[0])	//this is a bot.
				svs.clients[i].name[0] = '\0';						//make it go away

#ifdef PEXT_CSQC
			memset(svs.clients[i].csqcentsequence, 0, sizeof(svs.clients[i].csqcentsequence));
			memset(svs.clients[i].csqcentversions, 0, sizeof(svs.clients[i].csqcentversions));
#endif
		}
		sv.allocated_client_slots = i;

#ifdef PEXT_CSQC
		for (i=0 ; i<MAX_EDICTS ; i++)
			sv.csqcentversion[i] = 1;	//force all csqc edicts to start off as version 1
#endif

		break;
#ifdef Q2SERVER
	case GT_QUAKE2:
		for (i=0 ; i<MAX_CLIENTS ; i++)
		{
			q2ent = Q2EDICT_NUM(i+1);
			q2ent->s.number = i+1;
			svs.clients[i].q2edict = q2ent;
		}
		sv.allocated_client_slots = i;
		break;
#endif
	}

#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif

	//
	// spawn the rest of the entities on the map
	//	

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;

	if (svprogfuncs)
	{
		extern cvar_t coop, pr_imitatemvdsv;
		char *s;
		eval_t *eval;
		ent = EDICT_NUM(svprogfuncs, 0);
		ent->isfree = false;
		ent->v.model = PR_SetString(svprogfuncs, sv.worldmodel->name);
		ent->v.modelindex = 1;		// world model
		ent->v.solid = SOLID_BSP;
		ent->v.movetype = MOVETYPE_PUSH;

		if (progstype == PROG_QW && pr_imitatemvdsv.value>0)
		{
			ent->v.targetname = PR_SetString(svprogfuncs, "mvdsv");
			s = DISTRIBUTIONLONG;
			ent->v.netname = PR_NewString(svprogfuncs, va("%s %f %s, build %d\nBuild date: " __DATE__ ", " __TIME__ "", DISTRIBUTIONLONG, VERSION, PLATFORM, build_number()));
			ent->v.impulse = 0;//QWE_VERNUM;
			ent->v.items = 103;
		}


		pr_global_struct->mapname = PR_SetString(svprogfuncs, sv.name);
		// serverflags are for cross level information (sigils)
		pr_global_struct->serverflags = svs.serverflags;
		pr_global_struct->time = 0.1;	//HACK!!!! A few QuakeC mods expect time to be non-zero in spawn funcs - like prydon gate...

		if (progstype == PROG_H2)
		{
			if (coop.value)
			{
				eval = PR_FindGlobal(svprogfuncs, "coop", 0);
				if (eval) eval->_float = coop.value;
			}
			else
			{
				eval = PR_FindGlobal(svprogfuncs, "deathmatch", 0);
				if (eval) eval->_float = deathmatch.value;
			}
			eval = PR_FindGlobal(svprogfuncs, "randomclass", 0);
			if (eval) eval->_float = Cvar_Get("randomclass", "1", CVAR_LATCH, "Hexen2 rules")->value;

			eval = PR_FindGlobal(svprogfuncs, "cl_playerclass", 0);
			if (eval) eval->_float = 1;
		}
		else
		{
			if (pr_nqglobal_struct->coop && coop.value)
				pr_global_struct->coop = coop.value;
			else if (pr_nqglobal_struct->deathmatch)
				pr_global_struct->deathmatch = deathmatch.value;
		}
		
		if (progstype == PROG_QW)
			// run the frame start qc function to let progs check cvars
			SV_ProgStartFrame ();	//prydon gate seems to fail because of this allowance

		for (i = 0; i < svs.numprogs; i++)	//do this AFTER precaches have been played with...
		{
			f = PR_FindFunction (svprogfuncs, "initents", svs.progsnum[i]);
			if (f)
			{
				PR_ExecuteProgram(svprogfuncs, f);
			}
		}
	}

	// load and spawn all other entities
	if (progstype == PROG_H2)
	{
		extern cvar_t coop;
		spawnflagmask = 0;
		if (deathmatch.value)
			spawnflagmask |= SPAWNFLAG_NOT_H2DEATHMATCH;
		else if (coop.value)
			spawnflagmask |= SPAWNFLAG_NOT_H2COOP;
		else
			spawnflagmask |= SPAWNFLAG_NOT_H2SINGLE;

		if (skill.value < 0.5)
			spawnflagmask |= SPAWNFLAG_NOT_H2EASY;
		else if (skill.value > 1.5)
			spawnflagmask |= SPAWNFLAG_NOT_H2HARD;
		else
			spawnflagmask |= SPAWNFLAG_NOT_H2MEDIUM;
	}
	else if (!deathmatch.value)	//decide if we are to inhibit single player game ents instead
	{
		spawnflagmask = SPAWNFLAG_NOT_DEATHMATCH;
		if (skill.value < 0.5)
			spawnflagmask = SPAWNFLAG_NOT_EASY;
		else if (skill.value > 1.5)
			spawnflagmask = SPAWNFLAG_NOT_HARD;
		else
			spawnflagmask = SPAWNFLAG_NOT_MEDIUM;
	}
	else
		spawnflagmask = SPAWNFLAG_NOT_DEATHMATCH;
//do this and get the precaches/start up the game
	if (sv_loadentfiles.value)
		file = COM_LoadMallocFile(va("maps/%s.ent", server));
	else
		file = NULL;
	if (file)
	{
		char crc[12];
		sprintf(crc, "%i", CRC_Block(file, com_filesize));
		Info_SetValueForStarKey(svs.info, "*entfile", crc, MAX_SERVERINFO_STRING);
		switch(svs.gametype)
		{
		case GT_PROGS:
			pr_edict_size = PR_LoadEnts(svprogfuncs, file, spawnflagmask);
			break;
#ifdef Q2SERVER
		case GT_QUAKE2:
			ge->SpawnEntities(sv.name, file, startspot?startspot:"");
			break;
#endif
		}
		BZ_Free(file);
	}
	else
	{
		Info_SetValueForStarKey(svs.info, "*entfile", "", MAX_SERVERINFO_STRING);
		switch(svs.gametype)
		{
		case GT_PROGS:
			pr_edict_size = PR_LoadEnts(svprogfuncs, sv.worldmodel->entities, spawnflagmask);
			break;
#ifdef Q2SERVER
		case GT_QUAKE2:
			ge->SpawnEntities(sv.name, sv.worldmodel->entities, startspot?startspot:"");
			break;
#endif
		}
	}

#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif

	Q_strncpyz(sv.mapname, sv.name, sizeof(sv.mapname));
	if (svprogfuncs)
	{
		eval_t *val;
		ent = EDICT_NUM(svprogfuncs, 0);
		ent->v.angles[0] = ent->v.angles[1] = ent->v.angles[2] = 0;
		val = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "message", NULL);
		if (val)
		{
			if (progstype == PROG_H2)
				sprintf(sv.mapname, "%s", T_GetString(val->_float-1));
			else
				sprintf(sv.mapname, "%s", PR_GetString(svprogfuncs, val->string));
		}
		ent->readonly = true;	//lock it down!
	}

	// look up some model indexes for specialized message compression
	SV_FindModelNumbers ();

#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif
	// run two frames to allow everything to settle
	realtime += 0.1;
	SV_Physics ();
#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif
	realtime += 0.1;
	sv.time += 0.1;
	SV_Physics ();

#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif

	// save movement vars
	SV_SetMoveVars();

	// create a baseline for more efficient communications
//	SV_CreateBaseline ();
	SVNQ_CreateBaseline();
	sv.signon_buffer_size[sv.num_signon_buffers-1] = sv.signon.cursize;

	// all spawning is completed, any further precache statements
	// or prog writes to the signon message are errors
	if (usecinematic)
		sv.state = ss_cinematic;
	else
		sv.state = ss_active;

	SV_GibFilterInit();
	SV_FilterImpulseInit();

	Info_SetValueForKey (svs.info, "map", sv.name, MAX_SERVERINFO_STRING);
	Con_TPrintf (STL_SERVERSPAWNED);	//misc filenotfounds can be misleading.

	if (!startspot)
	{
		SV_FlushLevelCache();	//to make sure it's caught
		for (i=0 ; i<MAX_CLIENTS ; i++)
		{
			if (svs.clients[i].spawninfo)
				Z_Free(svs.clients[i].spawninfo);
			svs.clients[i].spawninfo = NULL;
		}
	}

	if (svprogfuncs && startspot)
	{
		eval_t *eval;
		eval = PR_FindGlobal(svprogfuncs, "startspot", 0);
		if (eval) eval->string = PR_NewString(svprogfuncs, startspot);
	}

	if (Cmd_AliasExist("f_svnewmap", RESTRICT_LOCAL))
		Cbuf_AddText("f_svnewmap\n", RESTRICT_LOCAL);

#ifndef SERVERONLY
	current_loading_size+=10;
	SCR_BeginLoadingPlaque();
#endif
}

#endif
