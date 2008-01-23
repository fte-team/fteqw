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

extern cvar_t qtvcl_forceversion1;
extern cvar_t qtvcl_eztvextensions;

unsigned char demobuffer[1024*16];
int demobuffersize;
int demopreparsedbytes;
qboolean disablepreparse;
qboolean endofdemo;

#define BUFFERTIME 0.5
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

int demo_preparsedemo(unsigned char *buffer, int bytes)
{
	int parsed = 0;
	int ofs;
	unsigned int length;
#define dem_mask 7
	if (cls.demoplayback != DPB_MVD && cls.demoplayback != DPB_EZTV)
		return bytes;	//no need if its not an mvd (this simplifies it a little)

	while (bytes>2)
	{
		switch(buffer[1]&dem_mask)
		{
		case dem_set:
			ofs = -10;
			break;
		case dem_multiple:
			ofs = 6;
			break;
		default:
			ofs = 2;
			break;
		}
		if (ofs > 0)
		{
			if (ofs+4 > bytes)
				break;
			length = (buffer[ofs+0]<<0) + (buffer[ofs+1]<<8) + (buffer[ofs+2]<<16) + (buffer[ofs+3]<<24);
			if (length > MAX_OVERALLMSGLEN)
			{
				disablepreparse = true;
				Con_Printf("Error looking ahead at demo\n");
				return parsed;
			}
			ofs+=4;
		}
		else
		{
			length = -ofs;
			ofs = 2;
		}
		//ofs is now the offset of the data
		if (ofs+length > bytes)
		{
			return parsed; //not got it all
		}
		if ((buffer[1]&dem_mask) == dem_all && (buffer[1] & ~dem_mask) && length < MAX_NQMSGLEN)
		{
			net_message.cursize = length;
			memcpy(net_message.data, buffer+ofs, length);
			CL_ParseServerMessage();
		}

		parsed += ofs+length;
		buffer += ofs+length;
		bytes -= ofs+length;
	}

	return parsed;
}

int readdemobytes(int *readpos, void *data, int len)
{
	int i;
	int trybytes;

	if (demopreparsedbytes < 0)	//won't happen in normal running, but can still happen on corrupt data... if we don't disconnect first.
	{
		Con_Printf("reset preparsed (underflow)\n");
		demopreparsedbytes = 0;
	}
	if (demopreparsedbytes > demobuffersize)
	{
		Con_Printf("reset preparsed (overflow)\n");
		demopreparsedbytes = 0;
	}

	trybytes = sizeof(demobuffer)-demobuffersize;

	i = VFS_READ(cls.demofile, demobuffer+demobuffersize, trybytes);
	if (i > 0)
	{
		demobuffersize += i;
		if (disablepreparse)
			demopreparsedbytes = demobuffersize;
		else
			demopreparsedbytes += demo_preparsedemo(demobuffer+demopreparsedbytes, demobuffersize-demopreparsedbytes);
	}
	else if (i < 0)
	{	//0 means no data available yet
		printf("VFS_READ failed\n");
		endofdemo = true;
		return 0;
	}



	if (len > demobuffersize)
	{
		len = demobuffersize;
		return 0;
	}
	memcpy(data, demobuffer+*readpos, len);
	*readpos += len;
	return len;
}

void demo_flushbytes(int bytes)
{
	if (bytes > demobuffersize)
		Sys_Error("demo_flushbytes: flushed too much!\n");
	memmove(demobuffer, demobuffer+bytes, demobuffersize - bytes);
	demobuffersize -= bytes;
	demopreparsedbytes -= bytes;
}

void demo_flushcache(void)
{
	demobuffersize = 0;
	demopreparsedbytes = 0;

	//no errors yet
	disablepreparse = false;
}

void demo_resetcache(int bytes, void *data)
{
	endofdemo = false;
	demo_flushcache();

	demobuffersize = bytes;
	demopreparsedbytes = 0;
	memcpy(demobuffer, data, bytes);

	//preparse it now
	bytes = 0;
	readdemobytes(&bytes, NULL, 0);
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

	if (!cls.demoplayback)
		return;

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
	int demopos = 0;
	int msglength;

	if (endofdemo)
	{
		endofdemo = false;
		CL_StopPlayback ();
		return 0;
	}

#ifdef NQPROT
	if (cls.demoplayback == DPB_NETQUAKE 
#ifdef Q2CLIENT
		|| cls.demoplayback == DPB_QUAKE2
#endif
		)
	{	//read the nq demo

#ifdef Q2CLIENT
		if (cls.demoplayback == DPB_QUAKE2 && (cls.netchan.last_received == realtime || cls.netchan.last_received > realtime-0.1))
			return 0;
		else 
#endif
			if (cls.demoplayback == DPB_NETQUAKE && cls.signon == 4/*SIGNONS*/)
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
		if (readdemobytes(&demopos, &msglength, 4) != 4)
		{
			return 0;
		}
		if (cls.demoplayback == DPB_NETQUAKE)
		{
			VectorCopy (cl.viewangles[1], cl.viewangles[2]);
			for (i=0 ; i<3 ; i++)
			{
				readdemobytes(&demopos, &f, 4);
				cl.simangles[0][i] = cl.viewangles[1][i] = LittleFloat (f);
			}
			VectorCopy (cl.viewangles[1], cl.viewangles[0]);
		}

		olddemotime = realtime;

		msglength = LittleLong (msglength);
		if (msglength > MAX_NQMSGLEN)
		{
			Con_Printf ("Demo message > MAX_MSGLEN");
			CL_StopPlayback ();
			return 0;
		}
		if (readdemobytes(&demopos, net_message.data, msglength) != msglength)
		{
			return 0;
		}
		demo_flushbytes(demopos);
		net_message.cursize = msglength;
	
		return 1;
	}
#endif
readnext:
	// read the time from the packet
	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
	{
		if (realtime < 0)
		{
			readdemobytes(&demopos, NULL, 0);
			return 0;
		}
		if (olddemotime > realtime)
			olddemotime = realtime;
		if (realtime + 1.0 < olddemotime)
			realtime = olddemotime - 1.0;

		if (readdemobytes(&demopos, &msecsadded, sizeof(msecsadded)) != sizeof(msecsadded))
		{
			Con_Printf("Not enough buffered\n");
			olddemotime = realtime;	//if we ran out of buffered demo, delay the demo parsing a little
			return 0;
		}
		demotime = olddemotime + msecsadded*(1.0f/1000);
		nextdemotime = demotime;
	}
	else
	{
		if (readdemobytes(&demopos, &demotime, sizeof(demotime)) != sizeof(demotime))
		{
			Con_Printf("Not enough buffered\n");
			olddemotime = realtime;	//if we ran out of buffered demo, delay the demo parsing a little
			return 0;
		}
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
			return 0;
		}
		else if (realtime < demotime)
		{
			return 0;		// don't need another message yet
		}
	}
	else
		realtime = demotime; // we're warping

	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
	{
		if ((msecsadded || cls.netchan.incoming_sequence < 2) && olddemotime != demotime)
		{
			cls.netchan.incoming_sequence++;
			cls.netchan.incoming_acknowledged++;
			cls.netchan.frame_latency = 0;
			cls.netchan.last_received = demotime; // just to happy timeout check
		}
	}

	if (cls.state < ca_demostart)
		Host_Error ("CL_GetDemoMessage: cls.state != ca_active");
	
	// get the msg type
	if (!readdemobytes (&demopos, &c, sizeof(c)))
	{
		Con_Printf("Not enough buffered\n");
		olddemotime = realtime+1;
		return 0;
	}
//	Con_Printf("demo packet %x\n", (int)c);
	switch (c&7)
	{
	case dem_cmd :
/*		if (cls.demoplayback == DPB_MVD)
		{
			unsigned short samps;
			unsigned char bits;
			unsigned char rateid;
			unsigned char audio[8192];

			if (readdemobytes (&demopos, &samps, 2) == 2)
			{
				if (readdemobytes (&demopos, &bits, 1) == 1)
				{
					if (samps > sizeof(audio))
					{
						Con_Printf("Corrupted/too large audio chunk\n");
						CL_StopPlayback();
						return 0;
					}
					if (readdemobytes (&demopos, &rateid, 1) == 1)
					{
						if (readdemobytes (&demopos, audio, samps) == samps)
						{
							FILE *f;
							samps = samps/(bits/8);
							f = fopen("c:/test.raw", "r+b");
							if (f)
							{
								fseek(f, 0, SEEK_END);
								fwrite(audio, samps, bits/8, f);
								fclose(f);
							}
							S_RawAudio(0, audio, 11025, samps, 1, bits/8);
							break;
						}
					}
				}
			}

			return 0;
		}
		else
		{*/
			// user sent input
			i = cls.netchan.outgoing_sequence & UPDATE_MASK;
			pcmd = &cl.frames[i].cmd[0];
			r = readdemobytes (&demopos, &q1cmd, sizeof(q1cmd));
			if (r != sizeof(q1cmd))
			{
				Con_Printf("Not enough buffered\n");
				olddemotime = realtime+1;
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
				readdemobytes (&demopos, &f, 4);
				cl.viewangles[0][i] = LittleFloat (f);
			}
/*		}*/
		break;

	case dem_read:
readit:
		// get the next message
		if (readdemobytes (&demopos, &msglength, 4) != 4)
		{
			Con_Printf("Not enough buffered\n");
			olddemotime = realtime+1;
			return 0;
		}
		msglength = LittleLong (msglength);
	//Con_Printf("read: %ld bytes\n", msglength);
		if ((unsigned int)msglength > MAX_OVERALLMSGLEN)
		{
			Con_Printf ("Demo message > MAX_OVERALLMSGLEN\n");
			CL_StopPlayback ();
			return 0;
		}
		if (readdemobytes (&demopos, net_message.data, msglength) != msglength)
		{
			Con_Printf("Not enough buffered\n");
			olddemotime = realtime+1;
			return 0;
		}
		net_message.cursize = msglength;

		if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
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
			case dem_all:
				if (c & ~dem_mask)
					goto readnext;
				break;
			}
		}
		break;

	case dem_set :
		readdemobytes (&demopos, &i, 4);
		cls.netchan.outgoing_sequence = LittleLong(i);
		readdemobytes (&demopos, &i, 4);
		cls.netchan.incoming_sequence = LittleLong(i);

		if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
			cls.netchan.incoming_acknowledged = cls.netchan.incoming_sequence;
		goto readnext;

	case dem_multiple:
		if (readdemobytes (&demopos, &i, sizeof(i)) != sizeof(i))
		{
			olddemotime = realtime;
			return 0;
		}
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
	demo_flushbytes(demopos);

	olddemotime = demotime;

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

	FS_FlushFSHash();
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
	entity_state_t *es;
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
	strncpy(name, fname, sizeof(name)-1-8);
	name[sizeof(name)-1-8] = '\0';

//make a unique name (unless the user specified it).
	strcat (name, ".qwd");	//we have the space
	if (c != 2)
	{
		vfsfile_t *f;

		f = FS_OpenVFS (name, "rb", FS_GAME);
		if (f)
		{
			COM_StripExtension(name, name, sizeof(name));
			p = name + strlen(name);
			strcat(p, "_XX.qwd");
			p++;
			i = 0;
			do
			{
				VFS_CLOSE (f);
				p[0] = i%100 + '0';
				p[1] = i%10 + '0';
				f = FS_OpenVFS (name, "rb", FS_GAME);
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

		if (buf.cursize > MAX_QWMSGLEN/2)
		{
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

// spawnstaticsound
	// static sounds are skipped in demos, life is hard

// baselines

	for (i = 0; i < cl_baselines_count; i++)
	{
		es = cl_baselines + i;

		if (memcmp(es, &nullentitystate, sizeof(nullentitystate)))
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
		if (buf.cursize > MAX_QWMSGLEN/2)
		{
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
	COM_DefaultExtension (name, ".qwd", sizeof(name));

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
	{
		FS_FlushFSHash();
		Cbuf_AddText(va("playdemo %s\n", name), RESTRICT_LOCAL);
	}
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
	int ft, neg;
	int len;
	char type;
	char chr;
	int protocol;
	int start;

//
// disconnect from server
//
	CL_Disconnect_f ();

	demo_flushcache();
//
// open the demo file
//
	Q_strncpyz (name, demoname, sizeof(name));
	COM_DefaultExtension (name, ".qwd", sizeof(name));
	cls.demofile = FS_OpenVFS(name, "rb", FS_GAME);
	if (!cls.demofile)
	{
		Q_strncpyz (name, demoname, sizeof(name));
		COM_DefaultExtension (name, ".dem", sizeof(name));
		cls.demofile = FS_OpenVFS(name, "rb", FS_GAME);
	}
	if (!cls.demofile)
	{
		Q_strncpyz (name, demoname, sizeof(name));
		COM_DefaultExtension (name, ".mvd", sizeof(name));
		cls.demofile = FS_OpenVFS(name, "rb", FS_GAME);
	}
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open \"%s\".\n", demoname);
		cls.demonum = -1;		// stop demo loop
		return;
	}
	Q_strncpyz (lastdemoname, demoname, sizeof(lastdemoname));
	Con_Printf ("Playing demo from %s.\n", name);

	if (!VFS_GETLEN (cls.demofile))
	{
		VFS_CLOSE(cls.demofile);
		cls.demofile = NULL;
		Con_Printf ("demo \"%s\" is empty.\n", demoname);
		cls.demonum = -1;		// stop demo loop
		return;
	}

	if (!Q_strcasecmp(name + strlen(name) - 3, "mvd") ||
		!Q_strcasecmp(name + strlen(name) - 6, "mvd.gz"))
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

	cls.netchan.last_received=realtime;


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
		while ((VFS_READ(cls.demofile, &chr, 1)==1) && (chr != '\n'))
		{
			if (chr == '-')
				neg = true;
			else if (chr < '0' || chr > '9')
				break;
			else
				ft = ft * 10 + ((int)chr - '0');
		}
		if (chr == '\n')
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

	TP_ExecTrigger ("f_demostart");
}

void CL_QTVPlay (vfsfile_t *newf, qboolean iseztv)
{
	CL_Disconnect_f ();

	cls.demofile = newf;

	demo_flushcache();	//just in case

	if (iseztv)
		cls.demoplayback = DPB_EZTV;
	else
		cls.demoplayback = DPB_MVD;

	cls.findtrack = true;

	cls.state = ca_demostart;
	net_message.packing = SZ_RAWBYTES;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, 0);
	realtime = -BUFFERTIME;
	cl.gametime = -BUFFERTIME;
	cl.gametimemark = realtime;
	if (realtime < -0.5)
		Con_Printf("Buffering for %i seconds\n", (int)-realtime);

	cls.netchan.last_received=realtime;

	cls.protocol = CP_QUAKEWORLD;
	TP_ExecTrigger ("f_demostart");
}

void CL_Demo_ClientCommand(char *commandtext)
{
	unsigned char b = 1;
	unsigned short len = LittleShort((unsigned short)(strlen(commandtext) + 4));
#ifndef _MSC_VER
#warning "this needs buffering safely"
#endif
	VFS_WRITE(cls.demofile, &len, sizeof(len));
	VFS_WRITE(cls.demofile, &b, sizeof(b));
	VFS_WRITE(cls.demofile, commandtext, strlen(commandtext)+1);
}

char qtvhostname[1024];
char qtvrequestbuffer[4096];
int qtvrequestsize;
vfsfile_t *qtvrequest;

void CL_QTVPoll (void)
{
	char *s, *e, *colon;
	char *tail = NULL;
	int len;
	qboolean streamavailable = false;
	qboolean saidheader = false;
	menu_t *sourcesmenu = NULL;
	int sourcenum = 0;

	int streamid;
	int numplayers = 0;
	int numviewers = 0;
	qboolean init_numplayers = false;
	qboolean init_numviewers = false;
	qboolean iseztv = false;
	char srchost[256];


	if (!qtvrequest)
		return;

	for(;;)
	{
		len = VFS_READ(qtvrequest, qtvrequestbuffer+qtvrequestsize, (sizeof(qtvrequestbuffer) - qtvrequestsize -1 > 0)?1:0);
		if (len <= 0)
			break;
		qtvrequestsize += len;
	}
	qtvrequestbuffer[qtvrequestsize] = '\0';
	if (!qtvrequestsize)
		return;

	//make sure it's a compleate chunk.
	for (s = qtvrequestbuffer; *s; s++)
	{
		if (s[0] == '\n' && s[1] == '\n')
		{
			tail = s+2;
			break;
		}
		if (s[0] == '\r' && s[1] == '\n' && s[2] == '\r' && s[3] == '\n')
		{
			tail = s+4;
			break;
		}
		if (s[0] == '\r' && s[1] == '\n' && s[2] == '\n')
		{
			tail = s+3;
			break;
		}
		if (s[0] == '\n' && s[1] == '\r' && s[2] == '\n')
		{
			tail = s+3;
			break;
		}
	}
	if (!tail)
		return;
	s[1] = '\0';	//make sure its null terminated before the data payload
	s = qtvrequestbuffer;
	for (e = s; *e; )
	{
		if (*e == '\r')
			*e = '\0';
		else if (*e == '\n')
		{
			*e = '\0';
			colon = strchr(s, ':');
			if (colon)
				*colon++ = '\0';
			else
				colon = "";

			if (!strcmp(s, "PERROR"))
			{	//printable error
				Con_Printf("QTV Error:\n%s\n", colon);
			}
			else if (!strcmp(s, "PRINT"))
			{	//printable error
				Con_Printf("QTV:\n%s\n", colon);
			}
			else if (!strcmp(s, "TERROR"))
			{	//printable error
				Con_Printf("QTV Error:\n%s\n", colon);
			}
			else if (!strcmp(s, "ADEMO"))
			{	//printable error
				Con_Printf("Demo%s is available\n", colon);
			}

			//generic sourcelist responce
			else if (!strcmp(s, "ASOURCE"))
			{	//printable source
				if (!saidheader)
				{
					saidheader=true;
					Con_Printf("Available Sources:\n");
				}
				Con_Printf("%s\n", colon);
				//we're too lazy to even try and parse this
			}

			else if (!strcmp(s, "BEGIN"))
			{
				if (*colon)
					Con_Printf("streaming \"%s\" from qtv\n", colon);
				else
					Con_Printf("qtv connection established\n", colon);
				streamavailable = true;
			}

			//eztv extensions to v1.0
			else if (!strcmp(s, "QTV_EZQUAKE_EXT"))
			{
				iseztv = true;
				Con_Printf("Warning: eztv extensions %s\n", colon);
			}

			//v1.1 sourcelist response includes SRCSRV, SRCHOST, SRCPLYRS, SRCVIEWS, SRCID
			else if (!strcmp(s, "SRCSRV"))
			{
				//the proxy's source string (beware of file:blah without file:blah@blah)
			}
			else if (!strcmp(s, "SRCHOST"))
			{
				//the hostname from the server the stream came from
				Q_strncpyz(srchost, colon, sizeof(srchost));
			}
			else if (!strcmp(s, "SRCPLYRS"))
			{
				//number of active players actually playing on that stream
				numplayers = atoi(colon);
				init_numplayers = true;
			}
			else if (!strcmp(s, "SRCVIEWS"))
			{
				//number of people watching this stream on the proxy itself
				numviewers = atoi(colon);
				init_numviewers = true;
			}
			else if (!strcmp(s, "SRCID"))
			{
				streamid = atoi(colon);

				//now put it on a menu
				if (!sourcesmenu)
				{
					m_state = m_complex;
					key_dest = key_menu;
					sourcesmenu = M_CreateMenu(0);

					MC_AddPicture(sourcesmenu, 16, 4, "gfx/qplaque.lmp");
					MC_AddCenterPicture(sourcesmenu, 4, "gfx/p_option.lmp");
				}
				if (init_numplayers == true && init_numviewers == true)
					MC_AddConsoleCommand(sourcesmenu, 42, (sourcenum++)*8 + 32, va("%s (p%i, v%i)", srchost, numplayers, numviewers), va("qtvplay %i@%s\n", streamid, qtvhostname));
				//else
				//	FIXME: add error message here
			}
			//end of sourcelist entry

			//from e to s, we have a line	
			s = e+1;
		}
		e++;
	}

	if (streamavailable)
	{
		CL_QTVPlay(qtvrequest, iseztv);
		qtvrequest = NULL;
		demo_resetcache(qtvrequestsize - (tail-qtvrequestbuffer), tail);
		return;
	}

	VFS_CLOSE(qtvrequest);
	qtvrequest = NULL;
	qtvrequestsize = 0;
}

char *strchrrev(char *str, char chr)
{
	char *firstchar = str;
	for (str = str + strlen(str)-1; str>=firstchar; str--)
		if (*str == chr)
			return str;

	return NULL;
}

void CL_QTVPlay_f (void)
{
	qboolean raw=0;
	char *connrequest;
	vfsfile_t *newf;
	char *host;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("Usage: qtvplay [stream@]hostname[:port] [password]\n");
		return;
	}

	connrequest = Cmd_Argv(1);

	if (*connrequest == '#')
	{
		char buffer[1024];
		char *s;
		FILE *f;
		f = fopen(connrequest+1, "rt");
		if (!f)
			return;
		while (!feof(f))
		{
			fgets(buffer, sizeof(buffer)-1, f);
			if (!strncmp(buffer, "Stream=", 7) || !strncmp(buffer, "Stream:", 7))
			{
				for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
				{
					if (*s == '\r' || *s == '\n')
						*s = 0;
					else
						break;
				}
				s = buffer+8;
				while(*s && *s <= ' ')
					s++;
				Cbuf_AddText(va("qtvplay \"%s\"\n", s), Cmd_ExecLevel);
				break;
			}
			if (!strncmp(buffer, "Join=", 7) || !strncmp(buffer, "Join:", 7))
			{
				for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
				{
					if (*s == '\r' || *s == '\n')
						*s = 0;
					else
						break;
				}
				s = buffer+8;
				while(*s && *s <= ' ')
					s++;
				Cbuf_AddText(va("join \"%s\"\n", s), Cmd_ExecLevel);
				break;
			}
			if (!strncmp(buffer, "Observe=", 7) || !strncmp(buffer, "Observe:", 7))
			{
				for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
				{
					if (*s == '\r' || *s == '\n')
						*s = 0;
					else
						break;
				}
				s = buffer+8;
				while(*s && *s <= ' ')
					s++;
				Cbuf_AddText(va("observe \"%s\"\n", s), Cmd_ExecLevel);
				break;
			}
		}
		fclose(f);
		return;
	}

	host = connrequest;

	connrequest = strchrrev(connrequest, '@');
	if (connrequest)
		host = connrequest+1;
	Q_strncpyz(qtvhostname, host, sizeof(qtvhostname));
	newf = FS_OpenTCP(qtvhostname);

	if (!newf)
	{
		Con_Printf("Couldn't connect to proxy\n");
		return;
	}

	host = Cmd_Argv(1);
	if (connrequest)
		*connrequest = '\0';
	else
		host = NULL;

	if (qtvcl_forceversion1.value)
	{
		connrequest =	"QTV\n"
				"VERSION: 1.0\n";
	}
	else
	{
		connrequest =	"QTV\n"
				"VERSION: 1.1\n";
	}

	VFS_WRITE(newf, connrequest, strlen(connrequest));

	if (qtvcl_eztvextensions.value)
	{
		connrequest =	"QTV_EZQUAKE_EXT: 3\n";
		VFS_WRITE(newf, connrequest, strlen(connrequest));
	}
	else if (raw)
	{
		connrequest =	"RAW: 1\n";
		VFS_WRITE(newf, connrequest, strlen(connrequest));
	}
	if (host)
	{
		connrequest =	"SOURCE: ";
		VFS_WRITE(newf, connrequest, strlen(connrequest));
		connrequest =	host;
		VFS_WRITE(newf, connrequest, strlen(connrequest));
		connrequest =	"\n";
	}
	else
	{
		connrequest =	"SOURCELIST\n";
	}

	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	if (raw)
	{
		CL_QTVPlay(newf, false);
	}
	else
	{
		if (qtvrequest)
			VFS_CLOSE(qtvrequest);
		qtvrequest = newf;
		qtvrequestsize = 0;
	}
}

void CL_QTVList_f (void)
{
	char *connrequest;
	vfsfile_t *newf;
	newf = FS_OpenTCP(qtvhostname);

	if (!newf)
	{
		Con_Printf("Couldn't connect to proxy\n");
		return;
	}

	if (qtvcl_forceversion1.value)
	{
		connrequest =	"QTV\n"
				"VERSION: 1.0\n";
	}
	else
	{
		connrequest =	"QTV\n"
				"VERSION: 1.1\n";
	}
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"SOURCELIST\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	if (qtvrequest)
		VFS_CLOSE(qtvrequest);
	qtvrequest = newf;
	qtvrequestsize = 0;
}

void CL_QTVDemos_f (void)
{
	char *connrequest;
	vfsfile_t *newf;
	newf = FS_OpenTCP(Cmd_Argv(1));

	if (!newf)
	{
		Con_Printf("Couldn't connect to proxy\n");
		return;
	}

	connrequest =	"QTV\n"
					"VERSION: 1\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"DEMOLIST\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	if (qtvrequest)
		VFS_CLOSE(qtvrequest);
	qtvrequest = newf;
	qtvrequestsize = 0;
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

	Con_Printf("NOTE: times currently depend on load times\n");
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

