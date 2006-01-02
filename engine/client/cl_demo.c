/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"

void CL_FinishTimeDemo (void);
#define realtime demtime
float demtime;

int cls_lastto;
int cls_lasttype;

void CL_PlayDemo(char *demoname);
char lastdemoname[256];

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	Media_CaptureDemoEnd();

	VFS_CLOSE (cls.demofile);
	cls.demofile = NULL;
	cls.state = ca_disconnected;
	cls.demoplayback = DPB_NONE;

	if (cls.timedemo)
		CL_FinishTimeDemo ();
}

#define dem_cmd		0
#define dem_read	1
#define dem_set		2

/*
====================
CL_WriteDemoCmd

Writes the player0 user cmd (demos don't support split screen)
====================
*/
void CL_WriteDemoCmd (usercmd_t *pcmd)
{
	int		i;
	float	fl;
	qbyte	c;
	q1usercmd_t cmd;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	fl = LittleFloat((float)realtime);
	VFS_WRITE (cls.demofile, &fl, sizeof(fl));

	c = dem_cmd;
	VFS_WRITE (cls.demofile, &c, sizeof(c));

	// correct for byte order, bytes don't matter

	cmd.buttons = pcmd->buttons;
	cmd.impulse = pcmd->impulse;
	cmd.msec = pcmd->msec;

	for (i = 0; i < 3; i++)
		cmd.angles[i] = LittleFloat(pcmd->angles[i]*65536/360);

	cmd.forwardmove = LittleShort(pcmd->forwardmove);
	cmd.sidemove    = LittleShort(pcmd->sidemove);
	cmd.upmove      = LittleShort(pcmd->upmove);

	VFS_WRITE (cls.demofile, &cmd, sizeof(cmd));

	for (i=0 ; i<3 ; i++)
	{
		fl = LittleFloat (cl.viewangles[0][i]);
		VFS_WRITE (cls.demofile, &fl, 4);
	}

	VFS_FLUSH (cls.demofile);
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (sizebuf_t *msg)
{
	int		len;
	float	fl;
	qbyte	c;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat((float)realtime);
	VFS_WRITE (cls.demofile, &fl, sizeof(fl));

	c = dem_read;
	VFS_WRITE (cls.demofile, &c, sizeof(c));

	len = LittleLong (msg->cursize);
	VFS_WRITE (cls.demofile, &len, 4);
	VFS_WRITE (cls.demofile, msg->data, msg->cursize);

	VFS_FLUSH (cls.demofile);
}

int unreaddata;
int unreadbytes;
int readdemobytes(void *data, int len)
{
	int i;

	if (unreadbytes)
	{
		if (len != unreadbytes)
			Sys_Error("Demo playback unread the wrong number of bytes\n");
		memcpy(data, &unreaddata, len);

		unreadbytes=0;
		return len;
	}

	i = VFS_READ(cls.demofile, data, len);

	memcpy(&unreaddata, data, 4);
	return i;
}


void CL_ProgressDemoTime(void)
{
	extern cvar_t cl_demospeed;

	if (cl.parsecount && Media_PausedDemo())
	{	//console visible whilst democapturing
#undef realtime
		cls.netchan.last_received = realtime;
#define realtime demtime
		return;
	}

	if (cl_demospeed.value >= 0)
		realtime += host_frametime*cl_demospeed.value;
	else
		realtime += host_frametime;
}

void CL_DemoJump_f(void)
{
	float newtime;
	char *s = Cmd_Argv(1);
	char *colon = strchr(s, ':');

	if (*s == '+')
	{
		if (colon)
		{
			colon++;
			realtime += atoi(colon);

			realtime += atoi(s)*60;
		}
		else
			realtime += atoi(s);
	}
	else
	{
		if (colon)
		{
			colon++;
			newtime = atoi(colon);
			newtime += atoi(s)*60;
		}
		else
			newtime = atoi(s);

		if (newtime >= realtime)
			realtime = newtime;
		else
		{
			Con_Printf("Rewinding demo\n");
			CL_PlayDemo(lastdemoname);

			realtime = newtime;
		}
	}
}

/*
====================
CL_GetDemoMessage

  FIXME...
====================
*/

float olddemotime = 0;
float nextdemotime = 0;
qboolean CL_GetDemoMessage (void)
{
	int		r, i, j, tracknum;
	float	f;
	float	demotime;
	qbyte	c, msecsadded=0;
	usercmd_t *pcmd;
	q1usercmd_t q1cmd;

#ifdef NQPROT
	if (cls.demoplayback == DPB_NETQUAKE || cls.demoplayback == DPB_QUAKE2)
	{	//read the nq demo

		if (cls.demoplayback == DPB_QUAKE2 && (cls.netchan.last_received == realtime || cls.netchan.last_received > realtime-0.1))
			return 0;
		else if (cls.demoplayback == DPB_NETQUAKE && cls.signon == 4/*SIGNONS*/)
		{
			if (!realtime)
			{
				cl.gametime = 0;
				cl.gametimemark = realtime;
				olddemotime = 0;
				return 0;
			}
			if (realtime<= cl.gametime && cl.gametime)// > dem_lasttime+realtime)
			{
				if (realtime <= cl.gametime-1||cls.timedemo)
				{
					realtime = cl.gametime;
					cls.netchan.last_received = realtime;
				}

				{
					float f = (cl.gametime-realtime)/(cl.gametime-olddemotime);
					float a1;
					float a2;

					for (i=0 ; i<3 ; i++)
					{
						a1 = cl.viewangles[2][i];
						a2 = cl.viewangles[1][i];
						if (a1 - a2 > 180)
							a1 -= 360;
						if (a1 - a2 < -180)
							a1 += 360;
						cl.simangles[0][i] = a2 + f * (a1 - a2);
					}
					VectorCopy(cl.simangles[0], cl.viewangles[0]);
				}
				return 0;
			}
		}
		readdemobytes(&net_message.cursize, 4);
//		VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
		if (cls.demoplayback == DPB_NETQUAKE)
		{
			VectorCopy (cl.viewangles[1], cl.viewangles[2]);
			for (i=0 ; i<3 ; i++)
			{
				readdemobytes(&f, 4);
				cl.simangles[0][i] = cl.viewangles[1][i] = LittleFloat (f);
			}
			VectorCopy (cl.viewangles[1], cl.viewangles[0]);
		}

		olddemotime = realtime;

		net_message.cursize = LittleLong (net_message.cursize);
		if (net_message.cursize > MAX_NQMSGLEN)
		{
			Con_Printf ("Demo message > MAX_MSGLEN");
			CL_StopPlayback ();
			return 0;
		}
		if (net_message.cursize<0)
		{
			Con_Printf ("Demo finished\n");
			CL_StopPlayback ();
			return 0;
		}
		r = readdemobytes(net_message.data, net_message.cursize);
		if (r != net_message.cursize)
		{
			CL_StopPlayback ();
			return 0;
		}
	
		return 1;
	}
#endif
readnext:
	// read the time from the packet
	if (cls.demoplayback == DPB_MVD)
	{
		if (olddemotime > realtime)
			olddemotime = realtime;
		if (realtime + 1.0 < olddemotime)
			realtime = olddemotime - 1.0;

		readdemobytes(&msecsadded, sizeof(msecsadded));
		demotime = olddemotime + msecsadded*(1.0f/1000);
		nextdemotime = demotime;
	}
	else
	{
		readdemobytes(&demotime, sizeof(demotime));
		demotime = LittleFloat(demotime);
	}

// decide if it is time to grab the next message		
	if (cls.timedemo)
	{
		if (cls.td_lastframe < 0)
			cls.td_lastframe = demotime;
		else if (demotime > cls.td_lastframe)
		{
			cls.td_lastframe = demotime;
			// rewind back to time
			if (cls.demoplayback == DPB_MVD)
				unreadbytes = sizeof(msecsadded);
			else
				unreadbytes = sizeof(demotime);
			return 0;		// already read this frame's message
		}
		if (!cls.td_starttime && cls.state == ca_active)
		{	//start the timer only once we are connected.
			cls.td_starttime = Sys_DoubleTime();
			cls.td_startframe = host_framecount;
		}
		realtime = demotime; // warp
	}
	else if (!cl.paused && cls.state >= ca_onserver)
	{	// always grab until fully connected
		if (realtime + 1.0 < demotime)
		{
			// too far back
			realtime = demotime - 1.0;
			// rewind back to time
			if (cls.demoplayback == DPB_MVD)
				unreadbytes = sizeof(msecsadded);
			else
				unreadbytes = sizeof(demotime);
			return 0;
		}
		else if (realtime < demotime)
		{
			// rewind back to time
			if (cls.demoplayback == DPB_MVD)
				unreadbytes = sizeof(msecsadded);
			else
				unreadbytes = sizeof(demotime);
			return 0;		// don't need another message yet
		}
	}
	else
		realtime = demotime; // we're warping

	if (cls.demoplayback == DPB_MVD)
	{
		if (msecsadded)
		{
			cls.netchan.incoming_sequence++;
			cls.netchan.incoming_acknowledged++;
			cls.netchan.frame_latency = 0;
			cls.netchan.last_received = demotime; // just to happy timeout check
		}
	}

	olddemotime = demotime;

	if (cls.state < ca_demostart)
		Host_Error ("CL_GetDemoMessage: cls.state != ca_active");
	
	// get the msg type
	VFS_READ (cls.demofile, &c, sizeof(c));
	
	switch (c&7)
	{
	case dem_cmd :
		// user sent input
		i = cls.netchan.outgoing_sequence & UPDATE_MASK;
		pcmd = &cl.frames[i].cmd[0];
		r = readdemobytes (&q1cmd, sizeof(q1cmd));
		if (r != sizeof(q1cmd))
		{
			CL_StopPlayback ();
			return 0;
		}
		// byte order stuff
		for (j = 0; j < 3; j++)
		{
			q1cmd.angles[j] = LittleFloat(q1cmd.angles[j]);
			pcmd->angles[j] = ((int)(q1cmd.angles[j]*65536.0/360)&65535);
		}
		pcmd->forwardmove = q1cmd.forwardmove	= LittleShort(q1cmd.forwardmove);
		pcmd->sidemove = q1cmd.sidemove			= LittleShort(q1cmd.sidemove);
		pcmd->upmove = q1cmd.upmove				= LittleShort(q1cmd.upmove);
		pcmd->msec = q1cmd.msec;
		pcmd->buttons = q1cmd.buttons;


		cl.frames[i].senttime = demotime;
		cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet
		cls.netchan.outgoing_sequence++;
		for (i=0 ; i<3 ; i++)
		{
			readdemobytes (&f, 4);
			cl.viewangles[0][i] = LittleFloat (f);
		}
		break;

	case dem_read:
readit:
		// get the next message
		readdemobytes (&net_message.cursize, 4);
		net_message.cursize = LittleLong (net_message.cursize);
	//Con_Printf("read: %ld bytes\n", net_message.cursize);
		if (net_message.cursize > MAX_OVERALLMSGLEN)
		{
			Con_Printf ("Demo message > MAX_OVERALLMSGLEN\n");
			CL_StopPlayback ();
			return 0;
		}
		r = readdemobytes (net_message.data, net_message.cursize);
		if (r != net_message.cursize)
		{
			CL_StopPlayback ();
			return 0;
		}

		if (cls.demoplayback == DPB_MVD)
		{
			switch(cls_lasttype)
			{
			case dem_multiple:
				tracknum = spec_track[0];
				if (!autocam[0])
					tracknum = -1;
				if (tracknum == -1 || !(cls_lastto & (1 << tracknum)))
					goto readnext;	
				break;
			case dem_single:
				tracknum = spec_track[0];
				if (!autocam[0])
					tracknum = -1;
				if (tracknum == -1 || cls_lastto != tracknum)
					goto readnext;
				break;
			}
		}
		break;

	case dem_set :
		readdemobytes (&i, 4);
		cls.netchan.outgoing_sequence = LittleLong(i);
		readdemobytes (&i, 4);
		cls.netchan.incoming_sequence = LittleLong(i);

		if (cls.demoplayback == DPB_MVD)
			cls.netchan.incoming_acknowledged = cls.netchan.incoming_sequence;
		goto readnext;

	case dem_multiple:
		readdemobytes (&i, sizeof(i));
		cls_lastto = LittleLong(i);
		cls_lasttype = dem_multiple;
		goto readit;

	case dem_single:
		cls_lastto = c >> 3;
		cls_lasttype = dem_single;
		goto readit;

	case dem_stats:
		cls_lastto = c >> 3;
		cls_lasttype = dem_stats;
		goto readit;

	case dem_all:
		cls_lastto = 0;
		cls_lasttype = dem_all;
		goto readit;

	default :
		Con_Printf("Corrupted demo.\n");
		CL_StopPlayback ();
		return 0;
	}

	return 1;
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
qboolean CL_GetMessage (void)
{
	if	(cls.demoplayback != DPB_NONE)
		return CL_GetDemoMessage ();

	if (!NET_GetPacket (NS_CLIENT))
		return false;

	CL_WriteDemoMessage (&net_message);
	
	return true;
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	if (!cls.demorecording)
	{
#ifndef CLIENTONLY
		SV_MVDStop_f();
#endif
		Con_Printf ("Not recording a demo.\n");
		return;
	}

// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteLong (&net_message, -1);	// -1 sequence means out of band
	MSG_WriteByte (&net_message, svc_disconnect);
	MSG_WriteString (&net_message, "EndOfDemo");
	CL_WriteDemoMessage (&net_message);

// finish up
	VFS_CLOSE (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Printf ("Completed demo\n");
}


/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteRecordDemoMessage (sizebuf_t *msg, int seq)
{
	int		len;
	int		i;
	float	fl;
	qbyte	c;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat((float)realtime);
	VFS_WRITE (cls.demofile, &fl, sizeof(fl));

	c = dem_read;
	VFS_WRITE (cls.demofile, &c, sizeof(c));

	len = LittleLong (msg->cursize + 8);
	VFS_WRITE (cls.demofile, &len, 4);

	i = LittleLong(seq);
	VFS_WRITE (cls.demofile, &i, 4);
	VFS_WRITE (cls.demofile, &i, 4);

	VFS_WRITE (cls.demofile, msg->data, msg->cursize);

	VFS_FLUSH (cls.demofile);
}


void CL_WriteSetDemoMessage (void)
{
	int		len;
	float	fl;
	qbyte	c;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	if (!cls.demorecording)
		return;

	fl = LittleFloat((float)realtime);
	VFS_WRITE (cls.demofile, &fl, sizeof(fl));

	c = dem_set;
	VFS_WRITE (cls.demofile, &c, sizeof(c));

	len = LittleLong(cls.netchan.outgoing_sequence);
	VFS_WRITE (cls.demofile, &len, 4);
	len = LittleLong(cls.netchan.incoming_sequence);
	VFS_WRITE (cls.demofile, &len, 4);

	VFS_FLUSH (cls.demofile);
}




/*
====================
CL_Record_f

record <demoname> <server>
====================
*/
void CL_Record_f (void)
{
	int		c;
	char	name[MAX_OSPATH];
	int namelen = sizeof(name);
	sizebuf_t	buf;
	char	buf_data[MAX_QWMSGLEN];
	int n, i, j;
	char *s, *p, *fname;
	entity_t *ent;
	entity_state_t *es, blankes;
	player_info_t *player;
	extern	char gamedirfile[];
	int seq = 1;

	c = Cmd_Argc();
	if (c > 2)
	{
		Con_Printf ("record <demoname>\n");
		return;
	}

	if (cls.state != ca_active)
	{
		Con_Printf ("You must be connected to record.\n");
		return;
	}

	if (cls.demorecording)
		CL_Stop_f();
  
	namelen -= strlen(com_gamedir)+1;
	if (c == 2)	//user supplied a name
	{
		fname = Cmd_Argv(1);
	}
	else
	{	//automagically generate a name
		if (cl.spectator)
		{	// FIXME: if tracking a player, use his name
			fname = va ("spec_%s_%s",
				TP_PlayerName(),
				TP_MapName());
		}
		else
		{	// guess game type and write demo name
			i = TP_CountPlayers();
			if (cl.teamplay && i >= 3)
			{	// Teamplay
				fname = va ("%s_%s_vs_%s_%s",
					TP_PlayerName(),
					TP_PlayerTeam(),
					TP_EnemyTeam(),
					TP_MapName());
			}
			else
			{
				if (i == 2)
				{	// Duel
					fname = va ("%s_vs_%s_%s",
						TP_PlayerName(),
						TP_EnemyName(),
						TP_MapName());
				}
				else if (i > 2)
				{	// FFA
					fname = va ("%s_ffa_%s",
						TP_PlayerName(), 
						TP_MapName());
				}
				else
				{	// one player
					fname = va ("%s_%s",
						TP_PlayerName(),
						TP_MapName());
				}
			}
		}
	}

	while((p = strstr(fname, "..")))
	{
		p[0] = '_';
		p[1] = '_';
	}

	// Make sure the filename doesn't contain illegal characters
	for (p=fname ; *p ; p++)
	{
		char c;
		*p &= 0x7F;		// strip high bit
		c = *p;
		if (c<=' ' || c=='?' || c=='*' || (c!=2&&(c=='\\' || c=='/')) || c==':'
			|| c=='<' || c=='>' || c=='"' || c=='.')
			*p = '_';
	}
	strncpy(name, va("%s/%s", com_gamedir, fname), sizeof(name)-1-8);
	name[sizeof(name)-1-8] = '\0';

//make a unique name (unless the user specified it).
	strcat (name, ".qwd");	//we have the space
	if (c != 2)
	{
		FILE *f;

		f = fopen (name, "rb");
		if (f)
		{
			COM_StripExtension(name, name);
			p = name + strlen(name);
			strcat(p, "_XX.qwd");
			p++;
			i = 0;
			do
			{
				fclose (f);
				p[0] = i%100 + '0';
				p[1] = i%10 + '0';
				f = fopen (name, "rb");
				i++;
			} while (f && i < 100);
		}
	}

//
// open the demo file
//
	cls.demofile = FS_OpenVFS (name, "wb", FS_GAME);
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Con_Printf ("recording to %s.\n", name);
	cls.demorecording = true;

/*-------------------------------------------------*/

// serverdata
	// send the info about the new client to all connected clients
	memset(&buf, 0, sizeof(buf));
	buf.data = buf_data;
	buf.maxsize = sizeof(buf_data);

// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
#ifdef PROTOCOL_VERSION_FTE
	if (cls.fteprotocolextensions)	//maintain demo compatability
	{
		MSG_WriteLong (&buf, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (&buf, cls.fteprotocolextensions);
	}
#endif
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, cl.servercount);
	MSG_WriteString (&buf, gamedirfile);

	for (i = 0; i < cl.splitclients; i++)
	{
		if (cl.spectator)
			MSG_WriteByte (&buf, cl.playernum[i] | 128);
		else
			MSG_WriteByte (&buf, cl.playernum[i]);
	}
	if (cls.fteprotocolextensions & PEXT_SPLITSCREEN)
		MSG_WriteByte (&buf, 128);

	// send full levelname
	MSG_WriteString (&buf, cl.levelname);

	// send the movevars
	MSG_WriteFloat(&buf, movevars.gravity);
	MSG_WriteFloat(&buf, movevars.stopspeed);
	MSG_WriteFloat(&buf, movevars.maxspeed);
	MSG_WriteFloat(&buf, movevars.spectatormaxspeed);
	MSG_WriteFloat(&buf, movevars.accelerate);
	MSG_WriteFloat(&buf, movevars.airaccelerate);
	MSG_WriteFloat(&buf, movevars.wateraccelerate);
	MSG_WriteFloat(&buf, movevars.friction);
	MSG_WriteFloat(&buf, movevars.waterfriction);
	MSG_WriteFloat(&buf, movevars.entgravity);

	// send server info string
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("fullserverinfo \"%s\"\n", cl.serverinfo) );

	// send music (delayed)
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0); // none in demos

#ifdef PEXT_SETVIEW
	if (cl.viewentity[0])	//tell the player if we have a different view entity
	{
		MSG_WriteByte (&buf, svc_setview);
		MSG_WriteByte (&buf, cl.viewentity[0]);
	}
#endif
	// flush packet
	CL_WriteRecordDemoMessage (&buf, seq++);
	SZ_Clear (&buf); 

// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.sound_name[n+1];
	while (*s)
	{
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_QWMSGLEN/2)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.sound_name[n+1];
	}
	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.model_name[n+1];
	while (*s)
	{
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_QWMSGLEN/2)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.model_name[n+1];
	}
	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

// spawnstatic

	for (i = 0; i < cl.num_statics; i++)
	{
		ent = cl_static_entities + i;

		MSG_WriteByte (&buf, svc_spawnstatic);

		for (j = 1; j < MAX_MODELS; j++)
			if (ent->model == cl.model_precache[j])
				break;
		if (j == MAX_MODELS)
			MSG_WriteByte (&buf, 0);
		else
			MSG_WriteByte (&buf, j);

		MSG_WriteByte (&buf, ent->frame);
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, ent->skinnum);
		for (j=0 ; j<3 ; j++)
		{
			MSG_WriteCoord (&buf, ent->origin[j]);
			MSG_WriteAngle (&buf, ent->angles[j]);
		}

		if (buf.cursize > MAX_QWMSGLEN/2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

// spawnstaticsound
	// static sounds are skipped in demos, life is hard

// baselines

	memset(&blankes, 0, sizeof(blankes));
	for (i = 0; i < MAX_EDICTS; i++)
	{
		es = cl_baselines + i;

		if (memcmp(es, &blankes, sizeof(blankes)))
		{
			MSG_WriteByte (&buf,svc_spawnbaseline);		
			MSG_WriteShort (&buf, i);

			MSG_WriteByte (&buf, es->modelindex);
			MSG_WriteByte (&buf, es->frame);
			MSG_WriteByte (&buf, es->colormap);
			MSG_WriteByte (&buf, es->skinnum);
			for (j=0 ; j<3 ; j++)
			{
				MSG_WriteCoord(&buf, es->origin[j]);
				MSG_WriteAngle(&buf, es->angles[j]);
			}

			if (buf.cursize > MAX_QWMSGLEN/2)
			{
				CL_WriteRecordDemoMessage (&buf, seq++);
				SZ_Clear (&buf); 
			}
		}
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("cmd spawn %i\n", cl.servercount) );

	if (buf.cursize)
	{
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

// send current status of all other players

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		player = cl.players + i;

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->frags);
		
		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->ping);
		
		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->pl);
		
		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, player->entertime);

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, player->userinfo);

		if (buf.cursize > MAX_QWMSGLEN/2)
		{
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}
	
// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (i >= MAX_STANDARDLIGHTSTYLES)
			if (!*cl_lightstyle[i].map)
				continue;

		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char)i);
		MSG_WriteString (&buf, cl_lightstyle[i].map);
	}

	for (i = ((cls.fteprotocolextensions&PEXT_HEXEN2)?MAX_QW_STATS:MAX_CL_STATS); i >= 0; i--)
	{
		if (!cl.stats[0][i])
			continue;
		MSG_WriteByte (&buf, svc_updatestatlong);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, cl.stats[0][i]);
		if (buf.cursize > MAX_QWMSGLEN/2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("skins\n") );

	CL_WriteRecordDemoMessage (&buf, seq++);

	CL_WriteSetDemoMessage();

	// done
}

/*
====================
CL_ReRecord_f

record <demoname>
====================
*/
void CL_ReRecord_f (void)
{
	int		c;
	char	name[MAX_OSPATH];
	char *s;

	c = Cmd_Argc();
	if (c != 2)
	{
		Con_Printf ("rerecord <demoname>\n");
		return;
	}

	if (!*cls.servername) {
		Con_Printf("No server to reconnect to...\n");
		return;
	}

	if (cls.demorecording)
		CL_Stop_f();

	s = Cmd_Argv(1);
	if (strstr(s, ".."))
	{
		Con_Printf ("Relative paths not allowed.\n");
		return;
	}
  
	sprintf (name, "%s", s);

//
// open the demo file
//
	COM_DefaultExtension (name, ".qwd");

	cls.demofile = FS_OpenVFS (name, "wb", FS_GAME);
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Con_Printf ("recording to %s.\n", name);
	cls.demorecording = true;

	CL_Disconnect();
	CL_BeginServerReconnect();
}

#ifdef WEBCLIENT
void CL_PlayDownloadedDemo(char *name, qboolean success)
{
	if (success == false)
		Con_Printf("Failed to download %s\n", name);
	else
		Cbuf_AddText(va("playdemo %s\n", name), RESTRICT_LOCAL);
}
#endif

/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDemo_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

#ifdef WEBCLIENT
	if (!strncmp(Cmd_Argv(1), "ftp://", 6) || !strncmp(Cmd_Argv(1), "http://", 7))
	{
		if (Cmd_ExecLevel == RESTRICT_LOCAL)
			HTTP_CL_Get(Cmd_Argv(1), COM_SkipPath(Cmd_Argv(1)), CL_PlayDownloadedDemo);
		return;
	}
#endif

	CL_PlayDemo(Cmd_Argv(1));
}

void CL_PlayDemo(char *demoname)
{
	char	name[256];
	int ft, c, neg;
	int len;
	char type;
	int protocol;
	int start;

//
// disconnect from server
//
	CL_Disconnect_f ();
	
	unreadbytes = 0;	//just in case
//
// open the demo file
//
	Q_strncpyz (name, demoname, sizeof(name));
	COM_DefaultExtension (name, ".qwd");
	cls.demofile = FS_OpenVFS(name, "rb", FS_GAME);
	if (!cls.demofile)
	{
		Q_strncpyz (name, demoname, sizeof(name));
		COM_DefaultExtension (name, ".dem");
		cls.demofile = FS_OpenVFS(name, "rb", FS_GAME);
	}
	if (!cls.demofile)
	{
		Q_strncpyz (name, demoname, sizeof(name));
		COM_DefaultExtension (name, ".mvd");
		cls.demofile = FS_OpenVFS(name, "rb", FS_GAME);
	}
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		cls.demonum = -1;		// stop demo loop
		return;
	}
	Q_strncpyz (lastdemoname, demoname, sizeof(lastdemoname));
	Con_Printf ("Playing demo from %s.\n", name);

	if (!Q_strcasecmp(name + strlen(name) - 3, "mvd"))
	{
		cls.demoplayback = DPB_MVD;
		cls.findtrack = true;
	}
	else
		cls.demoplayback = DPB_QUAKEWORLD;

	cls.state = ca_demostart;
	net_message.packing = SZ_RAWBYTES;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, 0);
	realtime = 0;
	cl.gametime = 0;
	cl.gametimemark = realtime;

	cls.netchan.last_received=0;


	start = VFS_TELL(cls.demofile);
	VFS_READ(cls.demofile, &len, sizeof(len));
	VFS_READ(cls.demofile, &type, sizeof(type));
	VFS_READ(cls.demofile, &protocol, sizeof(protocol));
	VFS_SEEK(cls.demofile, start);
	if (len > 5 && type == svcq2_serverdata && protocol == PROTOCOL_VERSION_Q2)
	{
#ifdef Q2CLIENT
		cls.demoplayback = DPB_QUAKE2;
		cls.protocol = CP_QUAKE2;
#else
		Con_Printf ("ERROR: cannot play Quake2 demos.\n");
		CL_StopPlayback();
		return;
#endif
	}
	else
	{
		cls.protocol = CP_QUAKEWORLD;

		ft = 0;	//work out if the first line is a int for the track number.
		while ((VFS_READ(cls.demofile, &c, 1)==1) && (c != '\n'))
		{
			if (c == '-')
				neg = true;
			else if (c < '0' || c > '9')
				break;
			else
				ft = ft * 10 + (c - '0');
		}
		if (c == '\n')
		{
#ifndef NQPROT
			Con_Printf ("ERROR: cannot play NQ demos.\n");
			CL_StopPlayback();
			return;
#else
			cls.protocol = CP_NETQUAKE;
			cls.demoplayback = DPB_NETQUAKE;	//nq demos. :o)
#endif
		}
		else
			VFS_SEEK(cls.demofile, start);	//quakeworld demo, so go back to start.
	}
}

/*
====================
CL_FinishTimeDemo

====================
*/
void CL_FinishTimeDemo (void)
{
	int		frames;
	float	time;
	
	cls.timedemo = false;
	
// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = Sys_DoubleTime() - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();
	
	if (cls.state != ca_demostart)
		return;

	CL_ReadPackets();

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted
	
	cls.timedemo = true;
	cls.td_starttime = Sys_DoubleTime();
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;		// get a new message this frame
}

