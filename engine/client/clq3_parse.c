#include "quakedef.h"
#include "shader.h"


//urm, yeah, this is more than just parse.

#ifdef Q3CLIENT

#include "clq3defs.h"

#define SHOWSTRING(s) if(cl_shownet.value==2)Con_Printf ("%s\n", s);
#define SHOWNET(x) if(cl_shownet.value==2)Con_Printf ("%3i:%s\n", msg_readcount-1, x);
#define SHOWNET2(x, y) if(cl_shownet.value==2)Con_Printf ("%3i:%3i:%s\n", msg_readcount-1, y, x);

void MSG_WriteBits(sizebuf_t *msg, int value, int bits);


ClientConnectionState_t ccs;



qboolean CG_FillQ3Snapshot(int snapnum, snapshot_t *snapshot)
{
	int i;
	clientSnap_t	*snap;

	if (snapnum > ccs.serverMessageNum)
	{
		Host_EndGame("CG_FillQ3Snapshot: snapshotNumber > cl.snap.serverMessageNum");
	}

	if (ccs.serverMessageNum - snapnum >= Q3UPDATE_BACKUP)
	{
		return false; // too old
	}

	snap = &ccs.snapshots[snapnum & Q3UPDATE_MASK];
	if(!snap->valid || snap->serverMessageNum != snapnum)
	{
		return false; // invalid
	}

	memcpy(&snapshot->ps, &snap->playerstate, sizeof(snapshot->ps));
	snapshot->numEntities = snap->numEntities;
	for (i=0; i<snapshot->numEntities; i++)
	{
		memcpy(&snapshot->entities[i], &ccs.parseEntities[(snap->firstEntity+i) & Q3PARSE_ENTITIES_MASK], sizeof(snapshot->entities[0]));
	}

	memcpy( &snapshot->areamask, snap->areabits, sizeof( snapshot->areamask ) );

	snapshot->snapFlags = snap->snapFlags;
	snapshot->ping = snap->ping;

	snapshot->serverTime = snap->serverTime;

	snapshot->numServerCommands = snap->serverCommandNum;
	snapshot->serverCommandSequence = ccs.lastServerCommandNum;

	return true;
}

/*
=====================
CLQ3_ParseServerCommand
=====================
*/
void CLQ3_ParseServerCommand(void)
 {
	int		number;
	char	*string;

	number = MSG_ReadLong();
	SHOWNET(va("%i", number));

	string = MSG_ReadString();
	SHOWSTRING(string);

	if( number <= ccs.lastServerCommandNum )
	{
		return; // we have already received this command
	}

	ccs.lastServerCommandNum++;

	if( number > ccs.lastServerCommandNum+Q3TEXTCMD_MASK-1 )
	{
		Con_Printf("Warning: Lost %i reliable serverCommands\n",
			number - ccs.lastServerCommandNum );
	}

	// archive the command to be processed by cgame later
	Q_strncpyz( ccs.serverCommands[number & Q3TEXTCMD_MASK], string, sizeof( ccs.serverCommands[0] ) );
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
static void CLQ3_DeltaEntity( clientSnap_t *frame, int newnum, q3entityState_t *old, qboolean unchanged )
{
	q3entityState_t *state;

	state = &ccs.parseEntities[ccs.firstParseEntity & Q3PARSE_ENTITIES_MASK];

	if( unchanged )
	{
		memcpy( state, old, sizeof(*state) ); // don't read any bits
	}
	else
	{
		if (!MSG_Q3_ReadDeltaEntity(old, state, newnum)) // the entity present in oldframe is not in the current frame
			return;
	}

	ccs.firstParseEntity++;
	frame->numEntities++;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
static void CLQ3_ParsePacketEntities( clientSnap_t *oldframe, clientSnap_t *newframe )
{
	int				numentities;
	int				oldnum;
	int				newnum;
	q3entityState_t	*oldstate;

	oldstate = NULL;
	newframe->firstEntity = ccs.firstParseEntity;
	newframe->numEntities = 0;

	// delta from the entities present in oldframe
	numentities = 0;
	if( !oldframe )
	{
		oldnum = 99999;
	}
	else if( oldframe->numEntities <= 0 )
	{
		oldnum = 99999;
	}
	else
	{
		oldstate = &ccs.parseEntities[oldframe->firstEntity & Q3PARSE_ENTITIES_MASK];
		oldnum = oldstate->number;
	}

	while( 1 )
	{
		newnum = MSG_ReadBits( GENTITYNUM_BITS );
		if( newnum < 0 || newnum >= MAX_GENTITIES )
		{
			Host_EndGame("CLQ3_ParsePacketEntities: bad number %i", newnum);
		}

		if( msg_readcount > net_message.cursize )
		{
			Host_EndGame("CLQ3_ParsePacketEntities: end of message");
		}

		// end of packetentities
		if( newnum == ENTITYNUM_NONE )
		{
			break;
		}

		while( oldnum < newnum )
		{
			// one or more entities from the old packet are unchanged
			SHOWSTRING( va( "unchanged: %i", oldnum ) );

			CLQ3_DeltaEntity( newframe, oldnum, oldstate, true );

			numentities++;

			if( numentities >= oldframe->numEntities )
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &ccs.parseEntities[(oldframe->firstEntity + numentities) & Q3PARSE_ENTITIES_MASK];
				oldnum = oldstate->number;
			}
		}

		if( oldnum == newnum )
		{
			// delta from previous state
			SHOWSTRING( va( "delta: %i", newnum ) );

			CLQ3_DeltaEntity( newframe, newnum, oldstate, false );

			numentities++;

			if( numentities >= oldframe->numEntities )
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &ccs.parseEntities[(oldframe->firstEntity + numentities) & Q3PARSE_ENTITIES_MASK];
				oldnum = oldstate->number;
			}
			continue;
		}

		if( oldnum > newnum )
		{
			// delta from baseline
			SHOWSTRING( va( "baseline: %i", newnum ) );

			CLQ3_DeltaEntity( newframe, newnum, &ccs.baselines[newnum], false );
		}
	}

	// any remaining entities in the old frame are copied over
	while( oldnum != 99999 )
	{
		// one or more entities from the old packet are unchanged
		SHOWSTRING( va( "unchanged: %i", oldnum ) );

		CLQ3_DeltaEntity( newframe, oldnum, oldstate, true );

		numentities++;

		if( numentities >= oldframe->numEntities )
		{
			oldnum = 99999;
		}
		else
		{
			oldstate = &ccs.parseEntities[(oldframe->firstEntity + numentities) & Q3PARSE_ENTITIES_MASK];
			oldnum = oldstate->number;
		}
	}
}

void CLQ3_ParseSnapshot(void)
{
	clientSnap_t	snap, *oldsnap;
	int				delta;
	int				len;
	int				i;
	outframe_t		*frame;
//	usercmd_t		*ucmd;
//	int				commandTime;

	memset(&snap, 0, sizeof(snap));
	snap.serverMessageNum = ccs.serverMessageNum;
	snap.serverCommandNum = ccs.lastServerCommandNum;
	snap.serverTime = MSG_ReadLong();

	//so we can delta to it properly.
	cl.oldgametime = cl.gametime;
	cl.oldgametimemark = cl.gametimemark;
	cl.gametime = snap.serverTime / 1000.0f;
	cl.gametimemark = Sys_DoubleTime();

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed message
	delta = MSG_ReadByte();
	if(delta)
	{
		snap.deltaFrame = ccs.serverMessageNum - delta;
		oldsnap = &ccs.snapshots[snap.deltaFrame & Q3UPDATE_MASK];

		if(!oldsnap->valid)
		{
			// should never happen
			Con_Printf( "Delta from invalid frame (not supposed to happen!).\n");
		}
		else if( oldsnap->serverMessageNum != snap.deltaFrame )
		{
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Con_DPrintf( "Delta frame too old.\n" );
		}
		else if(ccs.firstParseEntity - oldsnap->firstEntity >
			Q3MAX_PARSE_ENTITIES - MAX_ENTITIES_IN_SNAPSHOT)
		{
			Con_DPrintf( "Delta parse_entities too old.\n" );
		}
		else
		{
			snap.valid = true; // valid delta parse
		}
	}
	else
	{
		oldsnap = NULL;
		snap.deltaFrame = -1;
		snap.valid = true; // uncompressed frame
	}

	// read snapFlags
	snap.snapFlags = MSG_ReadByte();

	// read areabits
	len = MSG_ReadByte();
	MSG_ReadData(snap.areabits, len );

	// read playerinfo
	SHOWSTRING("playerstate");
	MSG_Q3_ReadDeltaPlayerstate(oldsnap ? &oldsnap->playerstate : NULL, &snap.playerstate);

	// read packet entities
	SHOWSTRING("packet entities");
	CLQ3_ParsePacketEntities(oldsnap, &snap);

	if (!snap.valid)
	{
		return;
	}

//	cl.adjustTimeDelta = true;

	// Find last usercmd server has processed and calculate snap.ping

	snap.ping = 3;
	for (i=cls.netchan.outgoing_sequence-1 ; i>cls.netchan.outgoing_sequence-Q3CMD_BACKUP ; i--)
	{
		frame = &cl.outframes[i & Q3CMD_MASK];
		if (frame->server_message_num == snap.deltaFrame)
		{
			snap.ping = Sys_Milliseconds() - frame->client_time;
			break;
		}
	}

	memcpy(&ccs.snap, &snap, sizeof(snap));
	memcpy(&ccs.snapshots[ccs.serverMessageNum & Q3UPDATE_MASK], &snap, sizeof(snap));

	SHOWSTRING(va("snapshot:%i  delta:%i  ping:%i", snap.serverMessageNum, snap.deltaFrame, snap.ping));
}

#define MAXCHUNKSIZE 65536
void CLQ3_ParseDownload(void)
{
	qdownload_t *dl = cls.download;
	unsigned int chunknum;
	unsigned int chunksize;
	unsigned char chunkdata[MAXCHUNKSIZE];
	int i;
	char *s;

	chunknum = (unsigned short) MSG_ReadShort();
	chunknum |= ccs.downloadchunknum&~0xffff;		//add the chunk number, truncated by the network protocol.

	if (!chunknum)
	{
		dl->size = (unsigned int)MSG_ReadLong();
		Cvar_SetValue( Cvar_Get("cl_downloadSize", "0", 0, "Download stuff"), dl->size );
	}

	if (dl->size == (unsigned int)-1)
	{
		s = MSG_ReadString();
		CL_DownloadFailed(dl->remotename, dl);
		Host_EndGame("%s", s);
		return;
	}

	chunksize = (unsigned short)MSG_ReadShort();
	if (chunksize > MAXCHUNKSIZE)
		Host_EndGame("Server sent a download chunk of size %i (it's too damn big!)\n", chunksize);

	for (i = 0; i < chunksize; i++)
		chunkdata[i] = MSG_ReadByte();

	if (ccs.downloadchunknum != chunknum)	//the q3 client is rather lame.
	{										//ccs.downloadchunknum holds the chunk number.
		Con_DPrintf("PACKETLOSS WHEN DOWNLOADING!!!!\n");
		return;	//let the server try again some time
	}
	ccs.downloadchunknum++;

	if (!dl || dl->method != DL_Q3)
	{
		Con_Printf("Server sending download, but no download was requested\n");
		CLQ3_SendClientCommand("stopdl");
		return;
	}

	if (!dl->file)
	{
		if (!DL_Begun(dl))
		{
			CL_DownloadFailed(dl->remotename, dl);
			return;
		}
	}

	Con_DPrintf("dl: chnk %u, size %u, csize %u\n", (unsigned int)chunknum, (unsigned int)dl->size, (unsigned int)chunksize);

	if (!chunksize)
	{
		CL_DownloadFinished(dl);

		FS_ReloadPackFiles();

		cl.servercount = -1;	//make sure the server resends us that vital gamestate.
		ccs.downloadchunknum = -1;
		return;
	}
	else
	{
		VFS_WRITE(dl->file, chunkdata, chunksize);
		dl->ratebytes += chunksize;
		chunksize=VFS_TELL(dl->file);
//		Con_Printf("Recieved %i\n", chunksize);

		dl->percent = (100.0 * chunksize) / dl->size;
	}


	CLQ3_SendClientCommand("nextdl %u", chunknum);
}

static qboolean CLQ3_SendDownloads(char *rc, char *rn)
{
	while(rn)
	{
		qdownload_t *dl;
		char localname[MAX_QPATH];
		char tempname[MAX_QPATH];
		char crc[64];
		vfsfile_t *f;
		extern cvar_t cl_downloads;
		rc = COM_ParseOut(rc, crc, sizeof(crc));
		rn = COM_Parse(rn);
		if (!*com_token)
			break;

		if (!strchr(com_token, '/'))	//don't let some muppet tell us to download quake3.exe
			break;

		//as much as I'd like to use COM_FCheckExists, this stuf is relative to root, not the gamedir.
		f = FS_OpenVFS(va("%s.pk3", com_token), "rb", FS_ROOT);
		if (f)
		{
			VFS_CLOSE(f);
			continue;
		}
		if (!FS_GenCachedPakName(va("%s.pk3", com_token), crc, localname, sizeof(localname)))
			continue;
		f = FS_OpenVFS(localname, "rb", FS_ROOT);
		if (f)
		{
			VFS_CLOSE(f);
			continue;
		}

		if (!FS_GenCachedPakName(va("%s.tmp", com_token), crc, tempname, sizeof(tempname)))
			continue;

		if (!cl_downloads.ival)
		{
			Con_Printf(CON_WARNING "Need to download %s.pk3, but downloads are disabled\n", com_token);
			continue;
		}

		//fixme: request to download it
		Con_Printf("Sending request to download %s.pk3\n", com_token);
		CLQ3_SendClientCommand("download %s.pk3", com_token);
		ccs.downloadchunknum = 0;
		dl = Z_Malloc(sizeof(*dl));
		//q3's downloads are relative to root, but they do at least force a pk3 extension.
		Q_snprintfz(dl->localname, sizeof(dl->localname), "package/%s", localname);
		Q_snprintfz(dl->tempname, sizeof(dl->tempname), "package/%s", tempname);
		dl->prefixbytes = 8;
		dl->fsroot = FS_ROOT;

		Q_snprintfz(dl->remotename, sizeof(dl->remotename), "%s.pk3", com_token);
		dl->method = DL_Q3;
		dl->percent = 0;
		cls.download = dl;
		return true;
	}
	return false;
}

qboolean CLQ3_SystemInfoChanged(char *str)
{
	qboolean usingpure, usingcheats;
	char *value;
	char *pc, *pn;
	char *rc, *rn;

	Con_Printf("Server's sv_pure: \"%s\"\n", Info_ValueForKey(str, "sv_pure"));
	usingpure = atoi(Info_ValueForKey(str, "sv_pure"));
	usingcheats = atoi(Info_ValueForKey(str, "sv_cheats"));
	Cvar_ForceCheatVars(usingpure||usingcheats, usingcheats);

//		if (atoi(value))
//			Host_EndGame("Unable to connect to Q3 Pure Servers\n");
	value = Info_ValueForKey(str, "fs_game");

#ifndef CLIENTONLY
	if (!sv.state)
#endif
	{
		COM_FlushTempoaryPacks();
		if (!*value)
			value = "baseq3";
		COM_Gamedir(value, NULL);
#ifndef CLIENTONLY
		InfoBuf_SetStarKey (&svs.info, "*gamedir", value);
#endif
	}

	rc = Info_ValueForKey(str, "sv_referencedPaks");	//the ones that we should download.
	rn = Info_ValueForKey(str, "sv_referencedPakNames");
	if (CLQ3_SendDownloads(rc, rn))
		return false;

	pc = Info_ValueForKey(str, "sv_paks");		//the ones that we are allowed to use (in order!)
	pn = Info_ValueForKey(str, "sv_pakNames");
	FS_PureMode(usingpure?2:0, pn, pc, rn, rc, ccs.fs_key);

	return true;	//yay, we're in
}

void CLQ3_ParseGameState(void)
{
	int		c;
	int		index;
	char	*configString;
	cvar_t *cl_paused;

//
// wipe the client_state_t struct
//
	CL_ClearState(true);
	ccs.firstParseEntity = 0;
	memset(ccs.parseEntities, 0, sizeof(ccs.parseEntities));
	memset(ccs.baselines, 0, sizeof(ccs.baselines));


	cl.minpitch = -90;
	cl.maxpitch = 90;

	ccs.lastServerCommandNum = MSG_ReadLong();

	for(;;)
	{
		c = MSG_ReadByte();

		if(msg_badread)
		{
			Host_EndGame("CLQ3_ParseGameState: read past end of server message");
		}

		if(c == svcq3_eom)
		{
			break;
		}

		SHOWNET(va("%i", c));

		switch(c)
		{
		default:
			Host_EndGame("CLQ3_ParseGameState: bad command byte %i", c);
			break;

		case svcq3_configstring:
			index = MSG_ReadBits(16);
			if (index < 0 || index >= MAX_Q3_CONFIGSTRINGS)
			{
				Host_EndGame("CLQ3_ParseGameState: configString index %i out of range", index);
			}
			configString = MSG_ReadString();

			CG_InsertIntoGameState(index, configString);
			break;

		case svcq3_baseline:
			index = MSG_ReadBits(GENTITYNUM_BITS);
			if (index < 0 || index >= MAX_GENTITIES)
			{
				Host_EndGame("CLQ3_ParseGameState: baseline index %i out of range", index);
			}
			MSG_Q3_ReadDeltaEntity(NULL, &ccs.baselines[index], index);
			break;
		}
	}

	cl.playerview[0].playernum = MSG_ReadLong();
	ccs.fs_key = MSG_ReadLong();

	if (!CLQ3_SystemInfoChanged(CG_GetConfigString(CFGSTR_SYSINFO)))
	{
		UI_Restart_f();
		return;
	}

	CG_Restart_f();
	UI_Restart_f();

	if (!cl.worldmodel)
		Host_EndGame("CGame didn't set a map.\n");

	cl.model_precaches_added = false;
	Surf_NewMap ();

	SCR_EndLoadingPlaque();

	CL_MakeActive("Quake3Arena");

	cl.splitclients = 1;

	{
		char buffer[2048];
		strcpy(buffer, va("cp %i ", cl.servercount));
		FSQ3_GenerateClientPacksList(buffer, sizeof(buffer), ccs.fs_key);
		CLQ3_SendClientCommand("%s", buffer); // warning: format not a string literal and no format arguments
	}

	// load cgame, etc
//	CL_ChangeLevel();

	cl_paused = Cvar_FindVar("cl_paused");
	if (cl_paused && cl_paused->ival)
		Cvar_ForceSet(cl_paused, "0");

}

void CLQ3_ParseServerMessage (void)
{
	int cmd;
	if (!CLQ3_Netchan_Process())
		return;	//was a fragment.

	if (cl_shownet.value == 1)
		Con_Printf ("%i ",net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_Printf ("------------------\n");

	net_message.packing = SZ_RAWBYTES;
	MSG_BeginReading(msg_nullnetprim);
	ccs.serverMessageNum = MSG_ReadLong();
	net_message.packing = SZ_HUFFMAN;	//the rest is huffman compressed.
	net_message.currentbit = msg_readcount*8;

	// read last client command number server received
	ccs.lastClientCommandNum = MSG_ReadLong();
	if( ccs.lastClientCommandNum <= ccs.numClientCommands - Q3TEXTCMD_BACKUP )
	{
		ccs.lastClientCommandNum = ccs.numClientCommands - Q3TEXTCMD_BACKUP + 1;
	}
	else if( ccs.lastClientCommandNum > ccs.numClientCommands )
	{
		ccs.lastClientCommandNum = ccs.numClientCommands;
	}

//
// parse the message
//
	for(;;)
	{
		cmd = MSG_ReadByte();

		if(msg_badread)	//hm, we have an eom, so only stop when the message is bad.
		{
			Host_EndGame("CLQ3_ParseServerMessage: read past end of server message");
			break;
		}

		if(cmd == svcq3_eom)
		{
			SHOWNET2("END OF MESSAGE", 2);
			break;
		}

		SHOWNET(va("%i", cmd));

	// other commands
		switch(cmd)
		{
		default:
			Host_EndGame("CLQ3_ParseServerMessage: Illegible server message");
			break;
		case svcq3_nop:
			break;
		case svcq3_gamestate:
			CLQ3_ParseGameState();
			break;
		case svcq3_serverCommand:
			CLQ3_ParseServerCommand();
			break;
		case svcq3_download:
			CLQ3_ParseDownload();
			break;
		case svcq3_snapshot:
			CLQ3_ParseSnapshot();
			break;
		}
	}
}




qboolean CLQ3_Netchan_Process(void)
{
	int		sequence;
	int		lastClientCommandNum;
	qbyte	bitmask;
	qbyte	c;
	int		i, j;
	char	*string;
	int		bit;
	int		readcount;

	if(!Netchan_ProcessQ3(&cls.netchan))
	{
		return false;
	}

	// archive buffer state
	bit = net_message.currentbit;
	readcount = msg_readcount;
	net_message.packing = SZ_HUFFMAN;
	net_message.currentbit = 32;

	lastClientCommandNum = MSG_ReadLong();
	sequence = LittleLong(*(int *)net_message.data);

	// restore buffer state
	net_message.currentbit = bit;
	msg_readcount = readcount;

	// calculate bitmask
	bitmask = (sequence ^ cls.challenge) & 0xff;
	string = ccs.clientCommands[lastClientCommandNum & Q3TEXTCMD_MASK];

#ifndef Q3_NOENCRYPT
	// decrypt the packet
	for(i=msg_readcount+4,j=0 ; i<net_message.cursize ; i++,j++)
	{
		if(!string[j])
		{
			j = 0; // another way around
		}
		c = string[j];
		if(c > 127 || c == '%')
		{
			c = '.';
		}
		bitmask ^= c << ((i-msg_readcount) & 1);
		net_message.data[i] ^= bitmask;
	}
#endif

	return true;
}

void CL_Netchan_Transmit( int length, const qbyte *data )
{
#define msg net_message
	int			serverid;
	int			lastSequence;
	int			lastServerCommandNum;
	qbyte		bitmask;
	qbyte		c;
	int			i, j;
	char		*string;
	net_message.cursize = 0;
	SZ_Write(&msg, data, length);

	if(msg.overflowed)
	{
		Host_EndGame("Client message overflowed");
	}

	msg_readcount = 0;
	msg.currentbit = 0;
	msg.packing = SZ_HUFFMAN;

	serverid = MSG_ReadLong();
	lastSequence = MSG_ReadLong();
	lastServerCommandNum = MSG_ReadLong();

	// calculate bitmask
	bitmask = (lastSequence ^ serverid ^ cls.challenge) & 0xff;
	string = ccs.serverCommands[lastServerCommandNum & Q3TEXTCMD_MASK];

#ifndef Q3_NOENCRYPT
	// encrypt the packet
	for( i=12,j=0 ; i<msg.cursize ; i++,j++ )
	{
		if( !string[j] )
		{
			j = 0; // another way around
		}
		c = string[j];
		if( c > 127 || c == '%' )
		{
			c = '.';
		}
		bitmask ^= c << (i & 1);
		msg.data[i] ^= bitmask;
	}
#endif

	Netchan_TransmitQ3( &cls.netchan, msg.cursize, msg.data );
#undef msg
}




static void MSG_WriteDeltaKey( sizebuf_t *msg, int key, int from, int to, int bits )
{
	if( from == to )
	{
		MSG_WriteBits( msg, 0, 1 );
		return; // unchanged
	}

	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, to ^ key, bits );
}

void MSG_Q3_WriteDeltaUsercmd( sizebuf_t *msg, int key, const usercmd_t *from, const usercmd_t *to )
{
	// figure out how to pack serverTime
	if( to->servertime - from->servertime < 255 )
	{
		MSG_WriteBits(msg, 1, 1);
		MSG_WriteBits(msg, to->servertime - from->servertime, 8);
	}
	else
	{
		MSG_WriteBits( msg, 0, 1 );
		MSG_WriteBits( msg, to->servertime, 32);
	}

	if( !memcmp( (qbyte *)from + 4, (qbyte *)to + 4, sizeof( usercmd_t ) - 4 ) )
	{
		MSG_WriteBits(msg, 0, 1);
		return; // nothing changed
	}
	MSG_WriteBits(msg, 1, 1);

	key ^= to->servertime;

	MSG_WriteDeltaKey(msg, key, from->angles[0],		to->angles[0],		16);
	MSG_WriteDeltaKey(msg, key, from->angles[1],		to->angles[1],		16);
	MSG_WriteDeltaKey(msg, key, from->angles[2],		to->angles[2],		16);
	MSG_WriteDeltaKey(msg, key, from->forwardmove,		to->forwardmove,	 8);
	MSG_WriteDeltaKey(msg, key, from->sidemove,			to->sidemove,		 8 );
	MSG_WriteDeltaKey(msg, key, from->upmove,			to->upmove,			 8);
	MSG_WriteDeltaKey(msg, key, from->buttons,			to->buttons,		16);
	MSG_WriteDeltaKey(msg, key, from->weapon,			to->weapon,			 8);
}



void VARGS CLQ3_SendClientCommand(const char *fmt, ...)
{
	va_list	argptr;
	char	command[MAX_STRING_CHARS];

	va_start( argptr, fmt );
	vsnprintf( command, sizeof(command), fmt, argptr );
	va_end( argptr );

	// create new clientCommand
	ccs.numClientCommands++;

	// check if server will lose some of our clientCommands
	if(ccs.numClientCommands - ccs.lastClientCommandNum >= Q3TEXTCMD_BACKUP)
		Host_EndGame("Client command overflow");

	Q_strncpyz(ccs.clientCommands[ccs.numClientCommands & Q3TEXTCMD_MASK], command, sizeof(ccs.clientCommands[0]));
	Con_DPrintf("Sending %s\n", command);
}

void CLQ3_SendCmd(usercmd_t *cmd)
{
	char *string;
	int i;
	char data[MAX_OVERALLMSGLEN];
	sizebuf_t msg;
	outframe_t *frame, *oldframe;
	int cmdcount, key;
	usercmd_t *to, *from;
	extern int keycatcher;
	extern cvar_t cl_nodelta, cl_c2sdupe;

	//reuse the q1 array
	cmd->servertime = cl.servertime*1000;
	cmd->weapon = ccs.selected_weapon;

	cmd->forwardmove *= 127/400.0f;
	cmd->sidemove *= 127/400.0f;
	cmd->upmove *= 127/400.0f;
	if (cmd->forwardmove > 127)
		cmd->forwardmove = 127;
	if (cmd->forwardmove < -127)
		cmd->forwardmove = -127;
	if (cmd->sidemove > 127)
		cmd->sidemove = 127;
	if (cmd->sidemove < -127)
		cmd->sidemove = -127;
	if (cmd->upmove > 127)
		cmd->upmove = 127;
	if (cmd->upmove < -127)
		cmd->upmove = -127;

	if (cmd->buttons & 2)	//jump
	{
		cmd->upmove = 100;
		cmd->buttons &= ~2;
	}
	if (Key_Dest_Has(~kdm_game) || (keycatcher&3))
		cmd->buttons |= 2;	//add in the 'at console' button

	cl.outframes[cl.movesequence&Q3CMD_MASK].cmd[0] = *cmd;
	cl.movesequence++;

	//FIXME: q3 generates a new command every video frame, but a new packet at a more limited rate.
	//FIXME: we should return here if its not yet time for a network frame.

	frame = &cl.outframes[cls.netchan.outgoing_sequence & Q3CMD_MASK];
	frame->cmd_sequence = cl.movesequence;
	frame->server_message_num = ccs.serverMessageNum;
	frame->server_time = cl.gametime;
	frame->client_time = Sys_DoubleTime()*1000;

	memset(&msg, 0, sizeof(msg));
	msg.maxsize = sizeof(data);
	msg.data = data;
	msg.packing = SZ_HUFFMAN;

	MSG_WriteBits(&msg, cl.servercount, 32);
	MSG_WriteBits(&msg, ccs.serverMessageNum, 32);
	MSG_WriteBits(&msg, ccs.lastServerCommandNum, 32);

	// write clientCommands not acknowledged by server yet
	for (i=ccs.lastClientCommandNum+1; i<=ccs.numClientCommands; i++)
	{
		MSG_WriteBits(&msg, clcq3_clientCommand, 8);
		MSG_WriteBits(&msg, i, 32);
		string = ccs.clientCommands[i & Q3TEXTCMD_MASK];
		while(*string)
			MSG_WriteBits(&msg, *string++, 8);
		MSG_WriteBits(&msg, 0, 8);
	}

	i = cls.netchan.outgoing_sequence;
	i -= bound(0, cl_c2sdupe.ival, 5); //extra age, if desired
	i--;
	if (i < cls.netchan.outgoing_sequence-Q3CMD_MASK)
		i = cls.netchan.outgoing_sequence-Q3CMD_MASK;
	oldframe = &cl.outframes[i & Q3CMD_MASK];
	cmdcount = cl.movesequence - oldframe->cmd_sequence;
	if (cmdcount > Q3CMD_MASK)
		cmdcount = Q3CMD_MASK;

	// begin a client move command, if any
	if (cmdcount)
	{
		if(cl_nodelta.value || !ccs.snap.valid ||
				ccs.snap.serverMessageNum != ccs.serverMessageNum)
			MSG_WriteBits(&msg, clcq3_nodeltaMove, 8); // no compression
		else
			MSG_WriteBits(&msg, clcq3_move, 8);

		// write cmdcount
		MSG_WriteBits(&msg, cmdcount, 8);

		// calculate key
		string = ccs.serverCommands[ccs.lastServerCommandNum & Q3TEXTCMD_MASK];
		key = ccs.fs_key ^ ccs.serverMessageNum ^ StringKey(string, 32);

		//note that q3 uses timestamps so sequences are not important
		//we can also send dupes without issue.
		from = &nullcmd;
		for (i = cl.movesequence-cmdcount; i < cl.movesequence; i++)
		{
			to = &cl.outframes[i&Q3CMD_MASK].cmd[0];
			MSG_Q3_WriteDeltaUsercmd( &msg, key, from, to );
			from = to;
		}
	}

	MSG_WriteBits(&msg, clcq3_eom, 8);

	CL_Netchan_Transmit( msg.cursize, msg.data );
	while(cls.netchan.reliable_length)
		Netchan_TransmitNextFragment(&cls.netchan);
}

void CLQ3_SendAuthPacket(netadr_t *gameserver)
{
	char data[2048];
	sizebuf_t msg;

//send the auth packet
//this should be the right code, but it doesn't work.
	if (gameserver->type == NA_IP)
	{
		char *key = Cvar_Get("cl_cdkey", "", 0, "Quake3 auth")->string;
		netadr_t authaddr;
#define	Q3_AUTHORIZE_SERVER_NAME	"authorize.quake3arena.com:27952"
		if (*key)
		{
			Con_Printf("Resolving %s\n", Q3_AUTHORIZE_SERVER_NAME);
			if (NET_StringToAdr(Q3_AUTHORIZE_SERVER_NAME, 0, &authaddr))
			{
				msg.data = data;
				msg.cursize = 0;
				msg.overflowed = msg.allowoverflow = 0;
				msg.maxsize = sizeof(data);
				MSG_WriteLong(&msg, -1);
				MSG_WriteString(&msg, "getKeyAuthorize 0 ");
				msg.cursize--;
				while(*key)
				{
					if ((*key >= 'a' && *key <= 'z') || (*key >= 'A' && *key <= 'Z') || (*key >= '0' && *key <= '9'))
						MSG_WriteByte(&msg, *key);
					key++;
				}
				MSG_WriteByte(&msg, 0);

				NET_SendPacket (cls.sockets, msg.cursize, msg.data, &authaddr);
			}
			else
				Con_Printf("    failed\n");
		}
	}
}

void CLQ3_SendConnectPacket(netadr_t *to, int challenge, int qport)
{
	char infostr[1024];
	char data[2048];
	sizebuf_t msg;
	static const char *nonq3[] = {"challenge", "qport", "protocol", "ip", "chat", NULL};

	memset(&ccs, 0, sizeof(ccs));

	InfoBuf_ToString(&cls.userinfo[0], infostr, sizeof(infostr), basicuserinfos, nonq3, NULL, &cls.userinfosync, &cls.userinfo[0]);

	cl.splitclients = 1;
	msg.data = data;
	msg.cursize = 0;
	msg.overflowed = msg.allowoverflow = 0;
	msg.maxsize = sizeof(data);
	MSG_WriteLong(&msg, -1);
	MSG_WriteString(&msg, va("connect \"\\challenge\\%i\\qport\\%i\\protocol\\%i%s\"", challenge, qport, PROTOCOL_VERSION_Q3, infostr));
#ifdef HUFFNETWORK
	Huff_EncryptPacket(&msg, 12);
	if (!Huff_CompressionCRC(HUFFCRC_QUAKE3))
	{
		Con_Printf("Huffman compression error\n");
		return;
	}
#endif
	NET_SendPacket (cls.sockets, msg.cursize, msg.data, to);
}
#endif

