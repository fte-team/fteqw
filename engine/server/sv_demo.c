#include "qwsvdef.h"

#ifndef CLIENTONLY

void NPP_MVDWriteByte(qbyte data, client_t *to, int broadcast);

void SV_New_f (void);
/*
float svdemotime;
FILE *svdemofile;

void SV_WriteDemoMessage (sizebuf_t *msg)
{
	short len;
	float time;
	if (!svs.demorecording)
		return;
	time = LittleFloat(sv.time);
	fwrite(&time, 1, sizeof(time), svdemofile);
	len = LittleShort((short)msg->cursize);
	fwrite(&len, 1, sizeof(len), svdemofile);
	fwrite(msg->data, 1, msg->cursize, svdemofile);
}

qboolean SV_GetDemoMessage (void)
{
	short len;
	float time;
	if (sv.time < svdemotime)
	{
		sv.msgfromdemo = false;
		return NET_GetPacket(NS_SERVER);
	}
sv.msgfromdemo = true;

	fread(&len, 1, sizeof(len), svdemofile);
	net_message.cursize = LittleShort(len);
	fread(net_message.data, 1, net_message.cursize, svdemofile);

	sv.time = svdemotime;

	if (!fread(&time, 1, sizeof(time), svdemofile))
	{
		svs.demoplayback = false;
		fclose(svdemofile);
	}
	svdemotime = LittleFloat(time);

	net_from.ip[0] = 0;
	net_from.ip[1] = 0;
	net_from.ip[2] = 0;
	net_from.ip[3] = 0;

	return true;
}






qboolean SV_GetPacket (void)
{
	if	(svs.demoplayback)
		return SV_GetDemoMessage ();

	if (!NET_GetPacket (NS_SERVER))
		return false;

	SV_WriteDemoMessage (&net_message);

	return true;
}

void SV_RecordDemo_f (void)
{
	client_t *c;
	int clnum;
	int i;
	char *name;
	char *mapname;
	name = Cmd_Argv(1);
	mapname = Cmd_Argv(2);

	svdemofile = fopen(name, "wb");
	if (!svdemofile)
	{
		Con_Printf("Failed to open output file\n");
		return;
	}

	fwrite(mapname, 1, sizeof(char)*(strlen(mapname)+1), svdemofile);


	for (clnum = 0; clnum < MAX_CLIENTS; clnum++)	//clear the server so the clients reconnect and send nice fresh messages.
	{
		c = &svs.clients[clnum];
		if (c->state <= cs_zombie)
			continue;
		ClientReliableWrite_Begin (c, svc_stufftext, 2+strlen("reconnect\n"));
		ClientReliableWrite_String (c, "disconnect;wait;reconnect\n");
		c->drop = true;
	}

	SV_SendMessagesToAll ();

	svs.demorecording = true;

	i = predictablerandgetseed();
	fwrite(&i, 1, sizeof(i), svdemofile);

	SV_SpawnServer(mapname, NULL, false, false);
}

void SV_LoadClientDemo (void);
void SV_PlayDemo_f(void)
{
	client_t *c;
	int clnum;
	int i;
	char *name;
	float time;
	char mapname[64];

	name = Cmd_Argv(1);

	if (svdemofile)
		fclose(svdemofile);
	svs.demoplayback=false;
	svs.demorecording=false;

	COM_FOpenFile(name, &svdemofile);

	if (!svdemofile)
	{
		Con_Printf("Failed to open input file\n");
		return;
	}
#ifndef SERVERONLY
	CL_Disconnect();
#endif
	i = 0;
	do
	{
		fread(mapname+i, 1, sizeof(char), svdemofile);
		i++;
	} while (mapname[i-1]);

	svs.demoplayback = true;

	for (clnum = 0; clnum < MAX_CLIENTS; clnum++)	//clear the server so new clients don't conflict.
	{
		c = &svs.clients[clnum];
		if (c->state <= cs_zombie)
			continue;
		ClientReliableWrite_Begin (c, svc_stufftext, 2+strlen("reconnect\n"));
		ClientReliableWrite_String (c, "reconnect\n");
		c->drop = true;
	}

	SV_SendMessagesToAll ();

	fread(&i, 1, sizeof(i), svdemofile);
	predictablesrand(i);


	fread(&time, 1, sizeof(time), svdemofile);
	svdemotime = LittleFloat(time);

	SV_SpawnServer(mapname, NULL, false, false);
}



*/



qboolean SV_GetPacket (void)
{
	return NET_GetPacket (NS_SERVER);
}








#define dem_cmd			0
#define dem_read		1
#define dem_set			2
#define dem_multiple	3
#define	dem_single		4
#define dem_stats		5
#define dem_all			6




#define svd sv

//char empty[512];
qboolean SV_ReadMVD (void);

#ifdef SERVERONLY
float nextdemotime = 0;
float olddemotime = 0;
#else
extern float nextdemotime;
extern float olddemotime;
#endif
void SV_LoadClientDemo_f (void)
{
	int i;
	char demoname[MAX_OSPATH];
	client_t *ohc;
	if (Cmd_Argc() < 2)
	{
		Con_Printf("%s <demoname>: play a server side multi-view demo\n", Cmd_Argv(0));
		return;
	}

	if (svd.demofile)
	{
		Con_Printf ("Ending old demo\n");
		VFS_CLOSE(svd.demofile);
		svd.demofile = NULL;

		SV_ReadMVD();
	}

	Q_strncpyz(demoname, Cmd_Argv(1), sizeof(demoname));

	if (!sv.state)
		Cmd_ExecuteString("map start\n", Cmd_ExecLevel);	//go for the start map
	if (!sv.state)
	{
		Con_Printf("Could not activate server\n");
		return;
	}

	svd.demofile = FS_OpenVFS(demoname, "rb", FS_GAME);
	if (!svd.demofile)	//try with a different path
		svd.demofile = FS_OpenVFS(va("demos/%s", demoname), "rb", FS_GAME);
	com_filesize = VFS_GETLEN(svd.demofile);

	if (!svd.demofile)
	{
		Con_Printf("Failed to open %s\n", demoname);
		return;
	}
	if (com_filesize <= 0)
	{
		Con_Printf("Failed to open %s\n", demoname);
		VFS_CLOSE(svd.demofile);
		svd.demofile = NULL;
		return;
	}

	if (sv.demostate)
	{
		sv.demostatevalid = false;
		memset(sv.demostate, 0, sizeof(entity_state_t)*MAX_EDICTS);
	}
/*
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		host_client = &svs.clients[i];
		if (host_client->state == cs_spawned)
			host_client->state = cs_connected;
	}
*/
//	SV_BroadcastCommand ("changing\n");

#ifndef SERVERONLY
	CL_Disconnect();
#endif

	svd.mvdplayback = true;
	Con_Printf("Playing from %s\n", demoname);

	for (i = 0; i < MAX_SIGNON_BUFFERS; i++)
		sv.demosignon_buffer_size[i] = 0;
	sv.demosignon.maxsize = sizeof(sv.demosignon_buffers[0]);
	sv.demosignon.data = sv.demosignon_buffers[0];
	sv.demosignon.cursize = 0;
	sv.num_demosignon_buffers = 1;
	sv.democausesreconnect = false;
	*sv.demname = '\0';

	svd.lasttype = dem_read;
	svd.realtime = realtime;
	nextdemotime = realtime-0.1; //cause read of the first 0.1 secs to get all spawn info.
	olddemotime = realtime;
	while (SV_ReadMVD())
	{
		sv.datagram.cursize = 0;
		sv.reliable_datagram.cursize = 0;
	}

	//if we did need reconnect, continue needing it cos I can't be bothered to play with multiple buffers etc.
//	if (memcmp(sv.demmodel_precache, sv.model_precache, sizeof(sv.model_precache)) || memcmp(sv.demsound_precache, sv.sound_precache, sizeof(sv.sound_precache)))
		sv.democausesreconnect = true;
	if (sv.democausesreconnect)
	{
		svs.spawncount++;
		SV_BroadcastCommand ("changing\n");	//but this arrives BEFORE the serverdata

		ohc = host_client;
		for (i=0, host_client = svs.clients ; i<MAX_CLIENTS ; i++, host_client++)
		{
			if (host_client->state != cs_spawned)
				continue;
			host_client->state = cs_connected;
			host_client->istobeloaded = true;	//don't harm the ent
			SV_New_f ();
		}
		host_client = ohc;
	}
	return;
}

qboolean SV_RunDemo (void)
{
	int		r, i;
	float	demotime;
	qbyte	c;
//	usercmd_t *pcmd;
//	usercmd_t emptycmd;
	qbyte	newtime;



readnext:

	// read the time from the packet
	if (svd.mvdplayback)
	{
		VFS_READ(svd.demofile, &newtime, sizeof(newtime));
		nextdemotime = olddemotime + newtime * (1/1000.0f);
		demotime = nextdemotime;

		if (nextdemotime > svd.realtime)
		{
			VFS_SEEK(svd.demofile, VFS_TELL(svd.demofile) - sizeof(newtime));
			return false;
		}
		else if (nextdemotime + 0.1 < svd.realtime)
			demotime = svd.realtime;	//we froze too long.. ?
	}
	else
	{
		VFS_READ(svd.demofile, &demotime, sizeof(demotime));
		demotime = LittleFloat(demotime);
		if (!nextdemotime)
			svd.realtime = nextdemotime = demotime;
	}

// decide if it is time to grab the next message
	if (!sv.paused) {	// always grab until fully connected
		if (!svd.mvdplayback)
		{
			if (svd.realtime + 1.0 < demotime) {
				// too far back
				svd.realtime = demotime - 1.0;
				// rewind back to time
				VFS_SEEK(svd.demofile, VFS_TELL(svd.demofile) - sizeof(demotime));
				return false;
			} else if (nextdemotime < demotime) {
				// rewind back to time
				VFS_SEEK(svd.demofile, VFS_TELL(svd.demofile) - sizeof(demotime));
				return false;		// don't need another message yet
			}
		}
	} else {
		svd.realtime = demotime; // we're warping
	}

	olddemotime = demotime;

	// get the msg type
	if ((r = VFS_READ(svd.demofile, &c, sizeof(c))) != sizeof(c))
	{
		Con_Printf ("Unexpected end of demo\n");
		VFS_CLOSE(svd.demofile);
		svd.demofile = NULL;
		return false;
//		SV_Error ("Unexpected end of demo");
	}

	switch (c & 7) {
	case dem_cmd :

		Con_Printf ("dem_cmd not supported\n");
		VFS_CLOSE(svd.demofile);
		svd.demofile = NULL;
		return false;


		// user sent input
//		i = svd.netchan.outgoing_sequence & UPDATE_MASK;
//		pcmd = &cl.frames[i].cmd;
	//	if ((r = fread (&emptycmd, sizeof(emptycmd), 1, svd.demofile)) != 1)
	//		SV_Error ("Corrupted demo");
/*
		// qbyte order stuff
		for (j = 0; j < 3; j++)
			pcmd->angles[j] = LittleFloat(pcmd->angles[j]);

		pcmd->forwardmove = LittleShort(pcmd->forwardmove);
		pcmd->sidemove    = LittleShort(pcmd->sidemove);
		pcmd->upmove      = LittleShort(pcmd->upmove);
		cl.frames[i].senttime = demotime;
		cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet
		svd.netchan.outgoing_sequence++;
*/
	//	fread (&emptycmd, 12, 1, svd.demofile);
/*		for (j = 0; j < 3; j++)
			 cl.viewangles[i] = LittleFloat (cl.viewangles[i]);
		if (cl.spectator)
			Cam_TryLock ();
*/
		goto readnext;

	case dem_read:
readit:
		// get the next message
		VFS_READ (svd.demofile, &net_message.cursize, 4);
		net_message.cursize = LittleLong (net_message.cursize);

		if (!svd.mvdplayback && net_message.cursize > MAX_QWMSGLEN + 8)
			SV_Error ("Demo message > MAX_MSGLEN + 8");
		else if (svd.mvdplayback && net_message.cursize > net_message.maxsize)
			SV_Error ("Demo message > MAX_UDP_PACKET");

		if ((r = VFS_READ(svd.demofile, net_message.data, net_message.cursize)) != net_message.cursize)
			SV_Error ("Corrupted demo");

/*		if (svd.mvdplayback) {
			tracknum = Cam_TrackNum();

			if (svd.lasttype == dem_multiple) {
				if (tracknum == -1)
					goto readnext;

				if (!(svd.lastto & (1 << (tracknum))))
					goto readnext;
			} else if (svd.lasttype == dem_single) {
				if (tracknum == -1 || svd.lastto != spec_track)
					goto readnext;
			}
		}
*/
		return true;

	case dem_set:
		VFS_READ(svd.demofile, &i, 4);
		VFS_READ(svd.demofile, &i, 4);
		goto readnext;

	case dem_multiple:
		if ((r = VFS_READ(svd.demofile, &i, 4)) != 1)
			SV_Error ("Corrupted demo");

		svd.lastto = LittleLong(i);
		svd.lasttype = dem_multiple;
		goto readit;

	case dem_single:
		svd.lastto = c >> 3;
		svd.lasttype = dem_single;
		goto readit;
	case dem_stats:
		svd.lastto = c >> 3;
		svd.lasttype = dem_stats;
		goto readit;
	case dem_all:
		svd.lastto = 0;
		svd.lasttype = dem_all;
		goto readit;
	default :
		SV_Error ("Corrupted demo");
		return false;
	}
}

qboolean SV_ReadMVD (void)
{
	int i, c;
	client_t *cl;

	int oldsc = svs.spawncount;

	if (!svd.demofile)
	{
		if (sv.demostate)
			BZ_Free(sv.demostate);
		sv.demostate=NULL;
		sv.demostatevalid = false;
		if (sv.democausesreconnect)
		{
			sv.democausesreconnect = false;
			svs.spawncount++;

			for (i=0, host_client = svs.clients ; i<MAX_CLIENTS ; i++, host_client++)
			{
				if (host_client->state != cs_spawned)
					continue;
				host_client->state = cs_connected;
				host_client->istobeloaded = true;	//don't harm the ent
				SV_New_f ();
			}
		}
		nextdemotime = realtime;
		return false;
	}

	svd.realtime = realtime;

	if (!SV_RunDemo())
	{
		if (!svd.demofile)
		{	//demo ended.
			for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
			{
				cl->sendinfo = true;
			}
		}
		return false;
	}

	if (!svd.mvdplayback)	//broadcast all.
	{
		for (c = 0; c < net_message.cursize; c++)
			NPP_MVDWriteByte(net_message.data[c], NULL, true);

		NPP_MVDForceFlush();
	}
	else
	{
		switch(svd.lasttype)
		{
		default:
			Con_Printf("Bad sv.lasttype %i\n", sv.lasttype);
			break;


		case dem_set:	//Unknown stuff. (Got to work out what this is for)
		case dem_read:	//baseline stuff
		case dem_stats:	//contains info read by server
		case dem_all:	//broadcast things (like userinfo)
		case dem_multiple:	//treat these as broadcast (tempents should be treated correctly)
			for (c = 0; c < net_message.cursize; c++)
				NPP_MVDWriteByte(net_message.data[c], NULL, true);

			NPP_MVDForceFlush();
			break;
//				case dem_read:	//baseline stuff
		case dem_single:
			for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
			{
				if (!cl->state)
					continue;
	//			if (!(1 >> 3 & svd.lastto))
				if (!cl->spec_track)
					continue;
				if (!(cl->spec_track >> 3 & svd.lastto))
					continue;

				for (c = 0; c < net_message.cursize; c++)
					NPP_MVDWriteByte(net_message.data[c], cl, false);

				NPP_MVDForceFlush();
			}
			break;
		}
	}

	if (oldsc != svs.spawncount)
	{
		VFS_CLOSE(svd.demofile);
		svd.demofile = NULL;

		for (i=0, host_client = svs.clients ; i<MAX_CLIENTS ; i++, host_client++)
		{
			if (host_client->state != cs_spawned)
				continue;
			host_client->state = cs_connected;
			host_client->istobeloaded = true;	//don't harm the ent
			SV_New_f ();
		}
		return true;
	}

	return true;
}




void SV_Demo_Init(void)
{
	//Cmd_AddCommand("playmvd", SV_LoadClientDemo_f);
	//Cmd_AddCommand("mvdplay", SV_LoadClientDemo_f);
//	Cmd_AddCommand("svplay", SV_PlayDemo_f);
//	Cmd_AddCommand("svrecord", SV_RecordDemo_f);
}

#endif
