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
#include "fs.h"

void CL_FinishTimeDemo (void);
float demtime;
float recdemostart;	//keyed to Sys_DoubleTime
int demoframe;

int cls_lastto;
int cls_lasttype;

void CL_PlayDemo(char *demoname, qboolean usesystempath);
char lastdemoname[256];
static qboolean lastdemowassystempath;

extern cvar_t qtvcl_forceversion1;
extern cvar_t qtvcl_eztvextensions;
extern cvar_t record_flush;

static unsigned char demobuffer[1024*66];	//input buffer
static int demooffset;		//start offset of demo buffer
static int demobuffersize;	//number of valid bytes within the buffer
static int demopreparsedbytes;	//number of bytes within the valid buffer that has already been pre-parsed.
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

	VFS_CLOSE (cls.demoinfile);
	cls.demoinfile = NULL;
	cls.state = ca_disconnected;
	cls.demoplayback = DPB_NONE;
	cls.demoseeking = false;	//just in case

	if (cls.demoindownload)
		cls.demoindownload->status = DL_FAILED;
	cls.demoindownload = NULL;

	if (cls.timedemo)
		CL_FinishTimeDemo ();

	TP_ExecTrigger("f_demoend", true);
}

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

	//nq doesn't have this info
	if (cls.demorecording != DPB_QUAKEWORLD)
		return;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, demtime);

	fl = LittleFloat(Sys_DoubleTime()-recdemostart);
	VFS_WRITE (cls.demooutfile, &fl, sizeof(fl));

	c = dem_cmd;
	VFS_WRITE (cls.demooutfile, &c, sizeof(c));

	// correct for byte order, bytes don't matter

	cmd.buttons = pcmd->buttons;
	cmd.impulse = pcmd->impulse;
	cmd.msec = pcmd->msec;

	for (i = 0; i < 3; i++)
		cmd.angles[i] = LittleShort((pcmd->angles[i]*360.0)/65535);

	cmd.forwardmove = LittleShort(pcmd->forwardmove);
	cmd.sidemove    = LittleShort(pcmd->sidemove);
	cmd.upmove      = LittleShort(pcmd->upmove);

	VFS_WRITE (cls.demooutfile, &cmd, sizeof(cmd));

	for (i=0 ; i<3 ; i++)
	{
		fl = LittleFloat (cl.playerview[0].viewangles[i]);
		VFS_WRITE (cls.demooutfile, &fl, 4);
	}

	if (record_flush.ival)
		VFS_FLUSH (cls.demooutfile);
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (sizebuf_t *msg, int payloadoffset)
{
	int		len;
	int		i;
	float	fl;
	qbyte	c;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, demtime);

	switch (cls.demorecording)
	{
	default:
		return;
	case DPB_QUAKEWORLD:	//QW
		fl = LittleFloat(Sys_DoubleTime()-recdemostart);
		VFS_WRITE (cls.demooutfile, &fl, sizeof(fl));

		c = dem_read;
		VFS_WRITE (cls.demooutfile, &c, sizeof(c));

		if (*(int*)msg->data == -1)
		{
			//connectionless packet.
			len = LittleLong (msg->cursize);
			VFS_WRITE (cls.demooutfile, &len, 4);
			VFS_WRITE (cls.demooutfile, msg->data + msg_readcount, msg->cursize - msg_readcount);
		}
		else
		{
			//regenerate a legacy netchan. no fragmentation support, but whatever. this ain't udp.
			//the length
			len = LittleLong (msg->cursize - msg_readcount + 8);
			VFS_WRITE (cls.demooutfile, &len, 4);
			//hack the netchan here.
			i = cls.netchan.incoming_sequence;
			VFS_WRITE (cls.demooutfile, &i, 4);
			i = cls.netchan.incoming_acknowledged;
			VFS_WRITE (cls.demooutfile, &i, 4);
			//and the data
			VFS_WRITE (cls.demooutfile, msg->data + msg_readcount, msg->cursize - msg_readcount);
		}
		break;
#ifdef Q2CLIENT
	case DPB_QUAKE2:
		len = LittleLong (net_message.cursize - payloadoffset);
		VFS_WRITE(cls.demooutfile, &len, sizeof(len));
		VFS_WRITE(cls.demooutfile, net_message.data + payloadoffset, net_message.cursize - payloadoffset);
		break;
#endif
#ifdef NQPROT
	case DPB_NETQUAKE:	//NQ
		len = LittleLong (net_message.cursize - payloadoffset);
		VFS_WRITE(cls.demooutfile, &len, sizeof(len));
		for (i=0 ; i<3 ; i++)
		{
			float f = LittleFloat (cl.playerview[0].viewangles[i]);
			VFS_WRITE(cls.demooutfile, &f, sizeof(f));
		}
		VFS_WRITE(cls.demooutfile, net_message.data + payloadoffset, net_message.cursize - payloadoffset);
		break;
#endif
	}
	if (record_flush.ival)
		VFS_FLUSH (cls.demooutfile);
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
		case dem_cmd:
			ofs = -(int)(sizeof(q1usercmd_t));
			ofs = 0;
			break;
		case dem_set:
			ofs = -(8);
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
		if ((buffer[1]&dem_mask) == dem_all && (buffer[1] & ~dem_mask) && length < MAX_OVERALLMSGLEN)
		{
			net_message.cursize = length;
			memcpy(net_message.data, buffer+ofs, length);
			MSG_BeginReading(cls.netchan.netprim);
			CLQW_ParseServerMessage();
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
	if (len < 0)
		Host_EndGame("Corrupt demo");

	//if there's not enough space in the buffer, flush it now and allow grabbing a new chunk
	//try to ensure it happens periodically enough for the preparsing stuff to happen early.
	if (demooffset+*readpos+len > demobuffersize || demooffset > sizeof(demobuffer)/2)
	{
		memmove(demobuffer, demobuffer+demooffset, demobuffersize);
		demooffset = 0;
	}

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

	trybytes = sizeof(demobuffer)-demooffset-demobuffersize;
	if (trybytes > 4096)
		i = VFS_READ(cls.demoinfile, demobuffer+demooffset+demobuffersize, trybytes);
	else
		i = 0;
	if (i > 0)
	{
		demobuffersize += i;
		if (disablepreparse)
			demopreparsedbytes = demobuffersize;
		else
			demopreparsedbytes += demo_preparsedemo(demobuffer+demooffset+demopreparsedbytes, demobuffersize-demopreparsedbytes);
	}

	if (*readpos+len > demobuffersize)
	{
		if (i < 0 || (i == 0 && len && !cls.demoinfile->seekingisabadplan))
		{	//0 means no data available yet, don't error on that (unless we can seek, in which case its not a stream and we won't get any more data later on).
			endofdemo = true;
			return 0;
		}
//		len = demobuffersize;
		return 0;
	}
	memcpy(data, demobuffer+demooffset+*readpos, len);
	*readpos += len;
	return len;
}

void demo_flushbytes(int bytes)
{
	if (demooffset+bytes > demobuffersize)
		Sys_Error("demo_flushbytes: flushed too much!\n");
	demooffset += bytes;
	demobuffersize -= bytes;

	if (demopreparsedbytes < bytes)
		demopreparsedbytes = 0;
	else
		demopreparsedbytes -= bytes;
}

void demo_flushcache(void)
{
	demooffset = 0;
	demobuffersize = 0;
	demopreparsedbytes = 0;

	//no errors yet
	disablepreparse = false;
}

void demo_resetcache(int bytes, void *data)
{
	endofdemo = false;
	demo_flushcache();

	demooffset = 0;
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

	if (cl.parsecount && Media_PausedDemo(true))
	{	//console visible whilst democapturing
		cls.netchan.last_received = realtime;
		return;
	}

	if (cl_demospeed.value >= 0 && cls.state == ca_active)
		demtime += host_frametime*cl_demospeed.value;
	else
		demtime += host_frametime;
}

void CL_DemoJump_f(void)
{
	float newtime;
	char *s = Cmd_Argv(1);
	char *colon = strchr(s, ':');

	if (!cls.demoplayback)
	{
		Con_Printf("not playing a demo, cannot jump.\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf("current time %.1f.\n", demtime);
		return;
	}

	if (*s == '+' || *s == '-')
	{
		if (colon)
		{
			colon++;
			newtime = demtime + atoi(colon) + atoi(s)*60;
		}
		else
			newtime = demtime + atoi(s);
	}
	else
	{
		//absolute seek time
		if (colon)
		{
			colon++;
			newtime = atoi(colon);
			newtime += atoi(s)*60;
		}
		else
			newtime = atoi(s);
	}
	if (newtime < 0)
		newtime = 0;

	if (newtime >= demtime)
		cls.demoseektime = newtime;
	else
	{
		Con_Printf("Rewinding demo\n");
		CL_PlayDemo(lastdemoname, lastdemowassystempath);

		//now fastparse it.
		cls.demoseektime = newtime;
	}
	cls.demoseeking = true;
}

/*
====================
CL_GetDemoMessage

  FIXME...
====================
*/

vec3_t demoangles;
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

		//if we've finished reading the connection part of the demo, but not finished loading, pause the demo
		if (cls.signon == 1 && !cl.worldmodel)
		{
			demtime = cl.gametime;
			return 0;
		}

		//if this is the starting frame of a timedemo
		if (cls.timedemo)
		if (cls.td_startframe == -1 && cls.state == ca_active)
		{	//start the timer only once we are connected.
			//make sure everything is loaded, to avoid stalls

			if (Key_Dest_Has(kdm_gmenu))
				MP_Toggle(0);
			COM_WorkerFullSync();

			cls.td_starttime = Sys_DoubleTime();
			cls.td_startframe = host_framecount;

			//force the console up, we're done loading.
			Key_Dest_Remove(kdm_console);
			Key_Dest_Remove(kdm_emenu);
			scr_con_current = 0;
		}

#ifdef Q2CLIENT
		//q2 uses a fixed 10fps packet rate, even demos enforce it.
		if (cls.demoplayback == DPB_QUAKE2)
		{
			if (cls.timedemo)
			{
				if (demoframe == host_framecount)
					return 0;
				demoframe = host_framecount;
			}
#if 1
			else if (demtime < cl.gametime && cl.gametime)
			{
				if (demtime <= cl.gametime-1)
					demtime = cl.gametime;
				return 0;
			}
#else
			else if (cls.netchan.last_received == realtime || cls.netchan.last_received > realtime-0.1)
				return 0;
#endif
		}
		else
#endif
			if (cls.demoplayback == DPB_NETQUAKE && cls.signon == 4/*SIGNONS*/)
		{
			if (!demtime)
			{
				cl.gametime = 0;
				cl.gametimemark = demtime;
				olddemotime = 0;
				return 0;
			}
			cls.netchan.last_received = realtime;
			if (cls.demoseeking)
			{
				if (cl.gametime > cls.demoseektime)
				{
					cls.demoseeking = false;
					return 0;
				}
			}
			else if ((cls.timedemo && host_framecount == demoframe) || (!cls.timedemo && demtime < cl.gametime && cl.gametime))// > dem_lasttime+demtime)
			{
				if (demtime <= cl.gametime-1)
					demtime = cl.gametime;
				return 0;
			}
			demoframe = host_framecount;
		}
		else if (cls.signon < 4)
			demtime = 0;
		if (readdemobytes(&demopos, &msglength, 4) != 4)
		{
			return 0;
		}
		msglength = LittleLong (msglength);
		if (msglength == -1)
		{
			int tmppos = demopos;
			//q2 writes a length of -1 to mark eof. if this peek fails then it really is eof and we shouldn't fail from weird message length checks.
			if (readdemobytes(&tmppos, &msglength, 4) != 4)
			{
				endofdemo = true;
				return 0;
			}
		}
		if (cls.demoplayback == DPB_NETQUAKE)
		{
			for (i=0 ; i<3 ; i++)
			{
				readdemobytes(&demopos, &f, 4);
				demoangles[i] = LittleFloat (f);
			}
		}

		olddemotime = demtime;

		if (msglength > net_message.maxsize)
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
		NET_UpdateRates(cls.sockets, true, msglength);	//keep any rate calcs sane
		net_message.cursize = msglength;

		return 1;
	}
#endif

readnext:
	if (demopos)
	{
		demo_flushbytes(demopos);
		demopos = 0;
	}

	// read the time from the packet
	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
	{
		if (demtime < 0)
		{
			readdemobytes(&demopos, NULL, 0);	//keep it feeding through
			return 0;
		}
		if (olddemotime > demtime)
			olddemotime = demtime;
		if (demtime + 1.0 < olddemotime)
			demtime = olddemotime - 1.0;

		if (readdemobytes(&demopos, &msecsadded, sizeof(msecsadded)) != sizeof(msecsadded))
		{
			Con_DPrintf("Not enough buffered\n");
			demotime = olddemotime;
			nextdemotime = demotime;
			return 0;
		}
		else
		{
			demotime = olddemotime + msecsadded*(1.0f/1000);
			nextdemotime = demotime;
		}
	}
	else
	{
		if (readdemobytes(&demopos, &demotime, sizeof(demotime)) != sizeof(demotime))
		{
			Con_DPrintf("Not enough buffered\n");
			olddemotime = demtime;	//if we ran out of buffered demo, delay the demo parsing a little
			return 0;
		}
		demotime = LittleFloat(demotime);
	}

	if (cl.sendprespawn)
	{
		CL_RequestNextDownload();
		if (!cls.timedemo)
			return 0;
	}


// decide if it is time to grab the next message
	if (cls.demoseeking)
	{
		demtime = demotime;	//warp
		if (demtime >= cls.demoseektime)
			cls.demoseeking = false;
	}
	else if (cls.timedemo)
	{
		if (cls.td_lastframe < 0)
			cls.td_lastframe = demotime;
		else if (demotime > cls.td_lastframe)
		{
			cls.td_lastframe = demotime;
			return 0;		// next packet starts after the previous one. draw something before parsing it.
		}
		if (cls.td_startframe == -1 && cls.state == ca_active)
		{	//start the timer only once we are connected.
			cls.td_starttime = Sys_DoubleTime();
			cls.td_startframe = host_framecount;

			//force the console up, we're done loading.
			Key_Dest_Remove(kdm_console);
			scr_con_current = 0;
		}
		if (cls.td_startframe == host_framecount+1)
			cls.td_starttime = Sys_DoubleTime();
		demtime = demotime; // warp
	}
	else if (!(cl.paused&~4) && cls.state >= ca_onserver)
	{	// always grab until fully connected
		if (demtime + 1.0 < demotime)
		{
			// too far back
			demtime = demotime - 1.0;
			return 0;
		}
		else if (demtime < demotime)
		{
			return 0;		// don't need another message yet
		}
	}
	else
		demtime = demotime; // we're warping

	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
	{
		if ((msecsadded || cls.netchan.incoming_sequence < 2) && olddemotime != demotime)
		{
			if (!(cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
			{
				cls.netchan.incoming_sequence++;
				cls.netchan.incoming_acknowledged++;
			}
			cls.netchan.frame_latency = 0;
			cls.netchan.last_received = realtime; // just to happy timeout check
		}
	}

	if (cls.state < ca_demostart)
		Host_Error ("CL_GetDemoMessage: cls.state != ca_active");

	// get the msg type
	if (readdemobytes (&demopos, &c, sizeof(c)) != sizeof(c))
	{
		Con_DPrintf("Not enough buffered\n");
		olddemotime = demtime+1;
		return 0;
	}
	switch (c&7)
	{
	case dem_cmd :
		if (cls.demoplayback == DPB_MVD)
		{
			Con_Printf("mvd demos/qtv streams should not contain dem_cmd\n");
			olddemotime = demtime+1;
			CL_StopPlayback ();
			return 0;
		}
		else
		{
			// user sent input
			i = cl.movesequence & UPDATE_MASK;
			pcmd = &cl.outframes[i].cmd[0];
			r = readdemobytes (&demopos, &q1cmd, sizeof(q1cmd));
			if (r != sizeof(q1cmd))
			{
				Con_DPrintf("Not enough buffered\n");
				olddemotime = demtime+1;
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


			cl.outframes[i].senttime = realtime;
			cl.outframes[i].server_message_num = cl.validsequence;
			cl.outframes[i].cmd_sequence = cl.movesequence;
			cls.netchan.outgoing_sequence++;
			cl.movesequence = cls.netchan.outgoing_sequence;
			for (i=0 ; i<3 ; i++)
			{
				readdemobytes (&demopos, &f, 4);
				demoangles[i] = LittleFloat (f);
				cl.playerview[0].viewangles[i] = LittleFloat (f);
			}
			goto readnext;
		}
		break;

	case dem_read:
readit:
		// get the next message
		if (readdemobytes (&demopos, &msglength, 4) != 4)
		{
			Con_DPrintf("Not enough buffered\n");
			olddemotime = demtime+1;
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
			Con_DPrintf("Not enough buffered\n");
			olddemotime = demtime+1;
			return 0;
		}
		NET_UpdateRates(cls.sockets, true, msglength);	//keep any rate calcs sane
		net_message.cursize = msglength;

		if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
		{
			int seat;
			cl.defaultnetsplit = 0;
			switch(cls_lasttype)
			{
			case dem_multiple:
				for (seat = 0; seat < cl.splitclients; seat++)
				{
					tracknum = cl.playerview[seat].cam_spec_track;
					if (cl.playerview[seat].cam_state == CAM_FREECAM)
						tracknum = -1;
					if (tracknum == -1 || !(cls_lastto & (1 << tracknum)))
						continue;
				}
				if (seat == cl.splitclients)
				{
					olddemotime = demotime;
					goto readnext;
				}
				cl.defaultnetsplit = seat;
				break;
			case dem_single:
				{
					int maxseat = cl.splitclients;
					//only accept single messages that are directed to the first player view.
					//this is too problematic otherwise (apparently mvdsv doesn't use dem_multiple for team says any more).
					if (1)
						maxseat = 1;
					for (seat = 0; seat < maxseat; seat++)
					{
						tracknum = cl.playerview[seat].cam_spec_track;
						if (cl.playerview[seat].cam_state == CAM_FREECAM)
							tracknum = -1;
						if (tracknum == -1 || (cls_lastto != tracknum))
							continue;
						break;
					}
					if (seat == maxseat)
					{
						olddemotime = demotime;
						goto readnext;
					}
					cl.defaultnetsplit = seat;
				}
				break;
			case dem_all:
				if (c & ~dem_mask)
				{
					olddemotime = demotime;
					goto readnext;
				}
				break;
			}
		}
		break;

	case dem_set:
		if (readdemobytes (&demopos, &j, 4) != 4)
		{
			olddemotime = demtime;
			return 0;
		}
		if (readdemobytes (&demopos, &i, 4) != 4)
		{
			olddemotime = demtime;
			return 0;
		}
		cls.demostarttime = demtime;
		cls.netchan.outgoing_sequence = LittleLong(j);
		cls.netchan.incoming_sequence = LittleLong(i);
		cl.movesequence = cls.netchan.outgoing_sequence;

		NET_UpdateRates(cls.sockets, false, demopos);	//keep any rate calcs sane

		if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
			cls.netchan.incoming_acknowledged = cls.netchan.incoming_sequence;
		goto readnext;

	case dem_multiple:
		if (readdemobytes (&demopos, &i, sizeof(i)) != sizeof(i))
		{
			olddemotime = demtime;
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

	net_from.type = NA_INVALID;
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

	if (NET_GetPacket (NS_CLIENT, 0) < 0)
		return false;

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
#else
		Con_Printf ("Not recording a demo.\n");
#endif
		return;
	}

// write a disconnect message to the demo file
#ifdef Q2CLIENT
	if (cls.demorecording == DPB_QUAKE2)
	{	//q2 demos signify eof with a -1 size
		int len = ~0;
		VFS_WRITE(cls.demooutfile, &len, sizeof(len));
	}
	else
#endif
	{
		SZ_Clear (&net_message);
		msg_readcount = 0;
		MSG_WriteLong (&net_message, -1);	// -1 sequence means out of band
		MSG_WriteByte (&net_message, svc_disconnect);
		MSG_WriteString (&net_message, "EndOfDemo");
		CL_WriteDemoMessage (&net_message, sizeof(int));
	}

// finish up
	VFS_CLOSE (cls.demooutfile);
	cls.demooutfile = NULL;
	cls.demorecording = false;
	Con_Printf ("Completed demo\n");

	FS_FlushFSHash();
}

void CL_WriteRecordQ2DemoMessage(sizebuf_t *msg)
{	//q2 is really simple, and doesn't have timings in its demo format.
	int len = LittleLong (msg->cursize);
	VFS_WRITE (cls.demooutfile, &len, 4);
	VFS_WRITE (cls.demooutfile, msg->data, msg->cursize);

}
/*
====================
CL_WriteDemoMessage

Dumps the specified net message as part of initial mid-map demo writing.
====================
*/
void CL_WriteRecordDemoMessage (sizebuf_t *msg, int seq)
{
	int		len;
	int		i;
	float	fl;
	qbyte	c;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, demtime);

	if (!cls.demorecording)
		return;

#ifdef NQPROT
	if (cls.demorecording == DPB_NETQUAKE)
	{
		len = LittleLong (msg->cursize);
		VFS_WRITE(cls.demooutfile, &len, sizeof(len));
		for (i=0 ; i<3 ; i++)
		{
			float f = LittleFloat (cl.playerview[0].viewangles[i]);
			VFS_WRITE(cls.demooutfile, &f, sizeof(f));
		}
	}
	else
#endif
	{
		fl = LittleFloat(Sys_DoubleTime()-recdemostart);
		VFS_WRITE (cls.demooutfile, &fl, sizeof(fl));

		c = dem_read;
		VFS_WRITE (cls.demooutfile, &c, sizeof(c));

		len = LittleLong (msg->cursize + 8);
		VFS_WRITE (cls.demooutfile, &len, 4);

		i = LittleLong(seq);
		VFS_WRITE (cls.demooutfile, &i, 4);
		VFS_WRITE (cls.demooutfile, &i, 4);
	}
	VFS_WRITE (cls.demooutfile, msg->data, msg->cursize);

	SZ_Clear(msg);

	if (record_flush.ival)
		VFS_FLUSH (cls.demooutfile);
}


void CL_WriteSetDemoMessage (void)
{
	int		len;
	float	fl;
	qbyte	c;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, demtime);

	if (cls.demorecording != DPB_QUAKEWORLD)
		return;

	fl = LittleFloat(Sys_DoubleTime()-recdemostart);
	VFS_WRITE (cls.demooutfile, &fl, sizeof(fl));

	c = dem_set;
	VFS_WRITE (cls.demooutfile, &c, sizeof(c));

	len = LittleLong(cls.netchan.outgoing_sequence);
	VFS_WRITE (cls.demooutfile, &len, 4);
	len = LittleLong(cls.netchan.incoming_sequence);
	VFS_WRITE (cls.demooutfile, &len, 4);

	if (record_flush.ival)
		VFS_FLUSH (cls.demooutfile);
}


/*
record a single player game.
*/
#ifndef CLIENTONLY
mvddest_t *SV_MVD_InitRecordFile (char *name);
qboolean SV_MVD_Record (mvddest_t *dest);
void CL_RecordMap_f (void)
{
	char demoname[MAX_QPATH];
	char mapname[MAX_QPATH];
	char demoext[8];

	if (Cmd_Argc() < 3)
	{
		Con_Printf("%s: demoname mapname\n", Cmd_Argv(0));
		return;
	}
	Q_strncpyz(demoname, Cmd_Argv(1), sizeof(demoname));
	Q_strncpyz(mapname, Cmd_Argv(2), sizeof(mapname));
	CL_Disconnect_f();

	SV_SpawnServer (mapname, NULL, false, false);

	COM_DefaultExtension(demoname, ".mvd", sizeof(demoname));
	COM_FileExtension(demoname, demoext, sizeof(demoext));

	if (!strcmp(demoext, "mvd"))
	{
		if (!SV_MVD_Record (SV_MVD_InitRecordFile(demoname)))
			CL_Disconnect_f();
//		char buf[512];
//		Cbuf_AddText(va("mvdrecord %s\n", COM_QuotedString(demoname, buf, sizeof(buf))), RESTRICT_LOCAL);
	}
	else
	{
		cls.demooutfile = FS_OpenVFS (demoname, "wb", FS_GAME);
		if (!cls.demooutfile)
		{
			CL_Disconnect_f();
			return;
		}
#ifdef NQPROT
		if (!strcmp(demoext, "dem"))
		{
			cls.demorecording = DPB_NETQUAKE;
			VFS_PUTS(cls.demooutfile, "-1\n");
		}
		else
#endif
			cls.demorecording = DPB_QUAKEWORLD;
		CL_WriteSetDemoMessage();
	}
}
#endif

//qw-specific serverdata
static void CLQW_RecordServerData(sizebuf_t *buf)
{
	extern	char gamedirfile[];
	unsigned int i;

// send the serverdata
	MSG_WriteByte (buf, svc_serverdata);
#ifdef PROTOCOL_VERSION_FTE
	if (cls.fteprotocolextensions)	//maintain demo compatability
	{
		MSG_WriteLong (buf, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (buf, cls.fteprotocolextensions);
	}
	if (cls.fteprotocolextensions2)	//maintain demo compatability
	{
		MSG_WriteLong (buf, PROTOCOL_VERSION_FTE2);
		MSG_WriteLong (buf, cls.fteprotocolextensions2);
	}
#endif
	MSG_WriteLong (buf, PROTOCOL_VERSION_QW);
	MSG_WriteLong (buf, cl.servercount);
	MSG_WriteString (buf, gamedirfile);

	if (cls.fteprotocolextensions2 & PEXT2_MAXPLAYERS)
	{
		MSG_WriteByte (buf, cl.allocated_client_slots);
		MSG_WriteByte (buf, cl.splitclients | (cl.spectator?128:0));
		for (i = 0; i < cl.splitclients; i++)
			MSG_WriteByte (buf, cl.playerview[i].playernum);
	}
	else
	{
		for (i = 0; i < cl.splitclients; i++)
		{
			if (cl.spectator)
				MSG_WriteByte (buf, cl.playerview[i].playernum | 128);
			else
				MSG_WriteByte (buf, cl.playerview[i].playernum);
		}
		if (cls.fteprotocolextensions & PEXT_SPLITSCREEN)
			MSG_WriteByte (buf, 128);
	}

	// send full levelname
	MSG_WriteString (buf, cl.levelname);

	// send the movevars
	MSG_WriteFloat(buf, movevars.gravity);
	MSG_WriteFloat(buf, movevars.stopspeed);
	MSG_WriteFloat(buf, movevars.maxspeed);
	MSG_WriteFloat(buf, movevars.spectatormaxspeed);
	MSG_WriteFloat(buf, movevars.accelerate);
	MSG_WriteFloat(buf, movevars.airaccelerate);
	MSG_WriteFloat(buf, movevars.wateraccelerate);
	MSG_WriteFloat(buf, movevars.friction);
	MSG_WriteFloat(buf, movevars.waterfriction);
	MSG_WriteFloat(buf, movevars.entgravity);

	// send server info string
	MSG_WriteByte (buf, svc_stufftext);
	MSG_WriteString (buf, va("fullserverinfo \"%s\"\n", cl.serverinfo) );
}

#ifdef NQPROT
void CLNQ_WriteServerData(sizebuf_t *buf)
{
	unsigned int protmain;
	unsigned int protfl = 0;
	unsigned int i;
	const char *val;

	val = Info_ValueForKey(cl.serverinfo, "*csprogs");
	if (*val)
	{
		MSG_WriteByte(buf, svc_stufftext);
		MSG_WriteString(buf, va("csqc_progcrc \"%s\"\n", val));
	}
	val = Info_ValueForKey(cl.serverinfo, "*csprogssize");
	if (*val)
	{
		MSG_WriteByte(buf, svc_stufftext);
		MSG_WriteString(buf, va("csqc_progsize \"%s\"\n", val));
	}
	val = Info_ValueForKey(cl.serverinfo, "*csprogsname");
	if (*val)
	{
		MSG_WriteByte(buf, svc_stufftext);
		MSG_WriteString(buf, va("csqc_progname \"%s\"\n", val));
	}

	MSG_WriteByte(buf, svc_serverdata);
	if (cls.fteprotocolextensions)
	{
		MSG_WriteLong (buf, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (buf, cls.fteprotocolextensions);
	}
	if (cls.fteprotocolextensions2)
	{
		MSG_WriteLong (buf, PROTOCOL_VERSION_FTE2);
		MSG_WriteLong (buf, cls.fteprotocolextensions2);
	}

	if (cls.netchan.message.prim.anglesize == 2)
		protfl |= RMQFL_SHORTANGLE;
	if (cls.netchan.message.prim.anglesize == 4)
		protfl |= RMQFL_FLOATANGLE;
	if (cls.netchan.message.prim.coordsize == 3)
		protfl |= RMQFL_24BITCOORD;
	if (cls.netchan.message.prim.coordsize == 4)
		protfl |= RMQFL_FLOATCOORD;
	switch(cls.protocol_nq)
	{
	default:
	case CPNQ_ID:		protmain = PROTOCOL_VERSION_NQ;		break;
	case CPNQ_BJP1:		protmain = PROTOCOL_VERSION_BJP1;	break;
	case CPNQ_BJP2:		protmain = PROTOCOL_VERSION_BJP2;	break;
	case CPNQ_BJP3:		protmain = PROTOCOL_VERSION_BJP3;	break;
	case CPNQ_FITZ666:	protmain = protfl?PROTOCOL_VERSION_RMQ:PROTOCOL_VERSION_FITZ;	break;	//this might break .scale, fte doesn't care, other engines might.
	case CPNQ_DP5:		protmain = PROTOCOL_VERSION_DP5;	break;
	case CPNQ_DP6:		protmain = PROTOCOL_VERSION_DP6;	break;
	case CPNQ_DP7:		protmain = PROTOCOL_VERSION_DP7;	break;
	}

	MSG_WriteLong (buf, protmain);
	if (protmain == PROTOCOL_VERSION_RMQ)
		MSG_WriteLong (buf, protfl);

	MSG_WriteByte (buf, cl.allocated_client_slots);
	MSG_WriteByte (buf, cl.deathmatch?GAME_DEATHMATCH:GAME_COOP);
	MSG_WriteString (buf, cl.levelname);

	for (i = 1; *cl.model_name[i] && i < MAX_PRECACHE_MODELS; i++)
		MSG_WriteString (buf, cl.model_name[i]);
	MSG_WriteByte (buf, 0);

	for (i = 1; *cl.sound_name[i] && i < MAX_PRECACHE_SOUNDS ; i++)
		MSG_WriteString (buf, cl.sound_name[i]);
	MSG_WriteByte (buf, 0);
}
#endif

void CL_Record_Baseline(sizebuf_t *buf, entity_state_t *state, unsigned int bits)
{
	unsigned int j;
	if (bits & FITZ_B_LARGEMODEL)
		MSG_WriteShort (buf, state->modelindex);
	else
		MSG_WriteByte (buf, state->modelindex);
	if (bits & FITZ_B_LARGEFRAME)
		MSG_WriteShort (buf, state->frame);
	else
		MSG_WriteByte (buf, state->frame);
	MSG_WriteByte (buf, state->colormap);
	MSG_WriteByte (buf, state->skinnum);
	for (j=0 ; j<3 ; j++)
	{
		MSG_WriteCoord (buf, state->origin[j]);
		MSG_WriteAngle (buf, state->angles[j]);
	}
	
	if (bits & FITZ_B_ALPHA)
		MSG_WriteByte(buf, state->trans);
	if (bits & RMQFITZ_B_SCALE)
		MSG_WriteByte(buf, state->scale);
}

//nq+qw generic stuff.
static int CL_Record_ParticlesStaticsBaselines(sizebuf_t *buf, int seq)
{
	unsigned int i;
	entity_state_t *es;

	//particleeffectnum stuff
	for (i = 1; i < MAX_SSPARTICLESPRE; i++)
	{
		if (!cl.particle_ssname[i])
			break;
		MSG_WriteByte(buf, svcfte_precache);
		MSG_WriteShort(buf, PC_PARTICLE | i);
		MSG_WriteString(buf, cl.particle_ssname[i]);

		if (buf->cursize > buf->maxsize/2)
			CL_WriteRecordDemoMessage (buf, seq++);
	}

	//custom tents (needed for hexen2, if nothing else)
	for (i = 0; ; i++)
	{
		if (!CL_WriteCustomTEnt(buf, i))
			break;

		if (buf->cursize > buf->maxsize/2)
			CL_WriteRecordDemoMessage (buf, seq++);
	}

// spawnstatic

	for (i = 0; i < cl.num_statics; i++)
	{
		es = &cl_static_entities[i].state;

#ifndef CLIENTONLY	//FIXME
		if (cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
		{
			MSG_WriteByte(buf, svcfte_spawnstatic2);
			SVFTE_EmitBaseline(es, false, buf, cls.fteprotocolextensions2);
		}
		//else if (cls.fteprotocolextensions & PEXT_SPAWNSTATIC2) //qw deltas
		else
#endif
		{
			unsigned int bits = 0;
#ifdef NQPROT
			if (es->modelindex > 255)
				bits |= FITZ_B_LARGEMODEL;
			if (es->frame > 255)
				bits |= FITZ_B_LARGEFRAME;
			if (es->trans != 255)
				bits |= FITZ_B_ALPHA;
			if (es->scale != 16)
				bits |= RMQFITZ_B_SCALE;
			if (cls.protocol == CP_NETQUAKE && CPNQ_IS_BJP)
			{
				MSG_WriteByte (buf, svc_spawnstatic);
				bits = FITZ_B_LARGEMODEL;	//bjp always uses shorts for models.
			}
			else if (cls.protocol == CP_NETQUAKE && cls.protocol_nq == CPNQ_FITZ666 && bits)
			{
				MSG_WriteByte (buf, svcfitz_spawnstatic2);
				MSG_WriteByte (buf, bits);
			}
//			else if (baselinetype2 >= CPNQ_DP5 && baselinetype2 <= CPNQ_DP7 && (bits & (FITZ_B_LARGEMODEL|FITZ_B_LARGEFRAME)))
//			{
//				MSG_WriteByte (buf, svcdp_spawnstatic2);
//				bits = FITZ_B_LARGEMODEL|FITZ_B_LARGEFRAME;	//dp's baseline2 always has these (regular baseline is unmodified)
//			}
			else
#endif
			{
				//classic protocol
				MSG_WriteByte (buf, svc_spawnstatic);
				bits = 0;
			}
			CL_Record_Baseline(buf, es, bits);
		}

		if (buf->cursize > buf->maxsize/2)
			CL_WriteRecordDemoMessage (buf, seq++);
	}

// FIXME: static sounds
	// static sounds are skipped in demos, life is hard

// baselines

	for (i = 0; i < cl_baselines_count; i++)
	{
		es = cl_baselines + i;

		if (memcmp(es, &nullentitystate, sizeof(nullentitystate)))
		{
#ifndef CLIENTONLY	//FIXME
			if (cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
			{
				MSG_WriteByte(buf, svcfte_spawnbaseline2);
				SVFTE_EmitBaseline(es, true, buf, cls.fteprotocolextensions2);
			}
			//else if (cls.fteprotocolextensions & PEXT_SPAWNSTATIC2) //qw deltas
			else
#endif
			{
				unsigned int bits = 0;
#ifdef NQPROT
				if (es->modelindex > 255)
					bits |= FITZ_B_LARGEMODEL;
				if (es->frame > 255)
					bits |= FITZ_B_LARGEFRAME;
				if (es->trans != 255)
					bits |= FITZ_B_ALPHA;
				if (es->scale != 16)
					bits |= RMQFITZ_B_SCALE;
				if (cls.protocol == CP_NETQUAKE && CPNQ_IS_BJP)
				{
					MSG_WriteByte (buf, svc_spawnbaseline);
					bits = FITZ_B_LARGEMODEL;	//bjp always uses shorts for models.
				}
				else if (cls.protocol == CP_NETQUAKE && cls.protocol_nq == CPNQ_FITZ666 && bits)
				{
					MSG_WriteByte (buf, svcfitz_spawnbaseline2);
					MSG_WriteByte (buf, bits);
				}
				else if (cls.protocol == CP_NETQUAKE && CPNQ_IS_DP && (bits & (FITZ_B_LARGEMODEL|FITZ_B_LARGEFRAME)))
				{
					MSG_WriteByte (buf, svcdp_spawnbaseline2);
					bits = FITZ_B_LARGEMODEL|FITZ_B_LARGEFRAME;	//dp's baseline2 always has these (regular baseline is unmodified)
				}
				else
#endif
				{
					MSG_WriteByte (buf,svc_spawnbaseline);
					bits = 0;
				}
				MSG_WriteEntity (buf, i);

				CL_Record_Baseline(buf, es, bits);
			}
			if (buf->cursize > buf->maxsize/2)
				CL_WriteRecordDemoMessage (buf, seq++);
		}
	}

	return seq;
}
static int CL_Record_Lightstyles(sizebuf_t *buf, int seq)
{
	unsigned int i;
// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (i >= MAX_STANDARDLIGHTSTYLES)
			if (!*cl_lightstyle[i].map)
				continue;

#ifdef PEXT_LIGHTSTYLECOL
		if ((cls.fteprotocolextensions & PEXT_LIGHTSTYLECOL) && (cl_lightstyle[i].colours[0]!=1||cl_lightstyle[i].colours[1]!=1||cl_lightstyle[i].colours[2]!=1) && *cl_lightstyle[i].map)
		{
			MSG_WriteByte (buf, svcfte_lightstylecol);
			MSG_WriteByte (buf, (unsigned char)i);
			MSG_WriteByte (buf, 0x87);
			MSG_WriteShort (buf, cl_lightstyle[i].colours[0]*1024);
			MSG_WriteShort (buf, cl_lightstyle[i].colours[1]*1024);
			MSG_WriteShort (buf, cl_lightstyle[i].colours[2]*1024);
			MSG_WriteString (buf, cl_lightstyle[i].map);
		}
		else
#endif
		{
			MSG_WriteByte (buf, svc_lightstyle);
			MSG_WriteByte (buf, (unsigned char)i);
			MSG_WriteString (buf, cl_lightstyle[i].map);
		}

		if (buf->cursize > buf->maxsize/2)
			CL_WriteRecordDemoMessage (buf, seq++);
	}
	return seq;
}

const char *Get_Q2ConfigString(int i);

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
	sizebuf_t	buf;
	char	buf_data[MAX_OVERALLMSGLEN];
	int n, i, seat;
	char *s, *p, *fname;
	player_info_t *player;
	extern	char gamedirfile[];
	int seq = 1;
	const char *defaultext;

	c = Cmd_Argc();
	if (c > 2)
	{
#ifndef CLIENTONLY
		CL_RecordMap_f();
#else
		Con_Printf ("record <demoname>\n");
#endif
		return;
	}

	if (cls.state != ca_active)
	{
		Con_Printf ("You must be connected to record.\n");
		return;
	}

	if (cls.protocol == CP_QUAKE2)
		defaultext = ".dm2";
	else if (cls.protocol == CP_NETQUAKE && !CPNQ_IS_DP)
		defaultext = ".dem";
	else if (cls.protocol == CP_QUAKEWORLD)
		defaultext = ".qwd";
	else
	{
		Con_Printf("Unable to record mid-map - try a different network protocol\n");
		return;
	}

	if (cls.demorecording)
		CL_Stop_f();

	if (c == 2)	//user supplied a name
	{
		fname = Cmd_Argv(1);
		s = strrchr(fname, '.');
		if (!Q_strcasecmp(s, defaultext))
			*s = 0;	//hack away that extension that they added.
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
	Q_strncpyz(name, fname, sizeof(name)-8);

//make a unique name (unless the user specified it).
	strcat (name, defaultext);	//we have the space
	if (c != 2)
	{
		vfsfile_t *f;

		f = FS_OpenVFS (name, "rb", FS_GAME);
		if (f)
		{
			//remove the extension again
			Q_strncpyz(name, fname, sizeof(name)-8);
			p = name + strlen(name);
			strcat(p, "_XX");
			strcat(p, defaultext);
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
	cls.demooutfile = FS_OpenVFS (name, "wb", FS_GAME);
	if (!cls.demooutfile)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}
	cls.demohadkeyframe = false;

	Con_Printf ("recording to %s.\n", name);

	recdemostart = Sys_DoubleTime();

/*-------------------------------------------------*/

	memset(&buf, 0, sizeof(buf));
	buf.data = buf_data;
	buf.maxsize = sizeof(buf_data);
	buf.prim = cls.netchan.netprim;
	switch(cls.protocol)
	{
	case CP_QUAKEWORLD:
		if (!cls.fteprotocolextensions && !cls.fteprotocolextensions2)
			buf.maxsize = MAX_QWMSGLEN;	//poo compatibility... :P

		cls.demorecording = DPB_QUAKEWORLD;

		CLQW_RecordServerData(&buf);

		// send music
		Media_WriteCurrentTrack(&buf);

		//paknames
		{
			char buffer[1024];
			FS_GetPackNames(buffer, sizeof(buffer), 2, true); /*retain extensions, or we'd have to assume pk3*/
			if (*buffer)
			{
				MSG_WriteByte(&buf, svc_stufftext);
				SZ_Write(&buf, "//paknames ", 11);
				SZ_Write(&buf, buffer, strlen(buffer));
				MSG_WriteString(&buf, "\n");
			}
		}
		//Paks
		{
			char buffer[1024];
			FS_GetPackHashes(buffer, sizeof(buffer), false);
			if (*buffer)
			{
				MSG_WriteByte(&buf, svc_stufftext);
				SZ_Write(&buf, "//paks ", 7);
				SZ_Write(&buf, buffer, strlen(buffer));
				MSG_WriteString(&buf, "\n");
			}
		}
		
		//FIXME: //at
		//FIXME: //wps
		//FIXME: //it


		MSG_WriteByte (&buf, svc_setpause);
		MSG_WriteByte (&buf, !!cl.paused);

#ifdef PEXT_SETVIEW
		if (cl.playerview[0].viewentity != cl.playerview[0].playernum+1)	//tell the player if we have a different view entity
		{
			MSG_WriteByte (&buf, svc_setview);
			MSG_WriteEntity (&buf, cl.playerview[0].viewentity);
		}
#endif
		// flush packet
		CL_WriteRecordDemoMessage (&buf, seq++);

	// soundlist
		MSG_WriteByte (&buf, svc_soundlist);
		MSG_WriteByte (&buf, 0);

		n = 0;
		s = cl.sound_name[n+1];
		while (*s)
		{
			MSG_WriteString (&buf, s);
			if (buf.cursize > buf.maxsize/2 && (n&0xff))
			{
				MSG_WriteByte (&buf, 0);
				MSG_WriteByte (&buf, n);
				CL_WriteRecordDemoMessage (&buf, seq++);
			
				if (n + 1 > 0xff)
				{
					MSG_WriteByte (&buf, svcfte_soundlistshort);
					MSG_WriteShort (&buf, n + 1);
				}
				else
				{
					MSG_WriteByte (&buf, svc_soundlist);
					MSG_WriteByte (&buf, n + 1);
				}
			}
			n++;
			s = cl.sound_name[n+1];
		}
		if (buf.cursize)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, 0);
			CL_WriteRecordDemoMessage (&buf, seq++);
		}

		//FIXME: vweps

	// modellist
		MSG_WriteByte (&buf, svc_modellist);
		MSG_WriteByte (&buf, 0);

		n = 0;
		s = cl.model_name[n+1];
		while (*s)
		{
			MSG_WriteString (&buf, s);
			if (buf.cursize > buf.maxsize/2 && (n&0xff))
			{
				MSG_WriteByte (&buf, 0);
				MSG_WriteByte (&buf, n);
				CL_WriteRecordDemoMessage (&buf, seq++);

				if (n + 1 > 0xff)
				{
					MSG_WriteByte (&buf, svcfte_modellistshort);
					MSG_WriteShort (&buf, n + 1);
				}
				else
				{
					MSG_WriteByte (&buf, svc_modellist);
					MSG_WriteByte (&buf, n + 1);
				}
			}
			n++;
			s = cl.model_name[n+1];
		}
		if (buf.cursize)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, 0);
			CL_WriteRecordDemoMessage (&buf, seq++);
		}

		seq = CL_Record_ParticlesStaticsBaselines(&buf, seq);

		MSG_WriteByte (&buf, svc_stufftext);
		MSG_WriteString (&buf, va("cmd spawn %i\n", cl.servercount) );

		if (buf.cursize)
			CL_WriteRecordDemoMessage (&buf, seq++);

	// send current status of all other players

		for (i = 0; i < cl.allocated_client_slots; i++)
		{
			player = cl.players + i;

			if (player->frags != 0)
			{
				MSG_WriteByte (&buf, svc_updatefrags);
				MSG_WriteByte (&buf, i);
				MSG_WriteShort (&buf, player->frags);
			}

			if (player->ping != 0)
			{
				MSG_WriteByte (&buf, svc_updateping);
				MSG_WriteByte (&buf, i);
				MSG_WriteShort (&buf, player->ping);
			}

			if (player->pl != 0)
			{
				MSG_WriteByte (&buf, svc_updatepl);
				MSG_WriteByte (&buf, i);
				MSG_WriteByte (&buf, player->pl);
			}

			if (*player->userinfo)
			{
				MSG_WriteByte (&buf, svc_updateentertime);
				MSG_WriteByte (&buf, i);
				MSG_WriteFloat (&buf, realtime - player->realentertime);	//seconds since
			}

			if (*player->userinfo)
			{
				MSG_WriteByte (&buf, svc_updateuserinfo);
				MSG_WriteByte (&buf, i);
				MSG_WriteLong (&buf, player->userid);
				MSG_WriteString (&buf, player->userinfo);
			}

			if (buf.cursize > buf.maxsize/2)
				CL_WriteRecordDemoMessage (&buf, seq++);
		}

		seq = CL_Record_Lightstyles(&buf, seq);

		for (seat = 0; seat < cl.splitclients; seat++)
		{
			//higher stats should be 0 and thus not be sent, if not valid.
			for (i = 0; i < MAX_CL_STATS; i++)
			{
				if (cl.playerview[seat].stats[i] || cl.playerview[seat].statsf[i])
				{
					double fs = cl.playerview[seat].statsf[i];
					double is = cl.playerview[seat].stats[i];
					if (seat)
					{
						MSG_WriteByte (&buf, svcfte_choosesplitclient);
						MSG_WriteByte (&buf, seat);
					}
					if ((int)fs == is)
					{
						MSG_WriteByte (&buf, svcqw_updatestatlong);
						MSG_WriteByte (&buf, i);
						MSG_WriteLong (&buf, is);
					}
					else
					{
						MSG_WriteByte (&buf, svcfte_updatestatfloat);
						MSG_WriteByte (&buf, i);
						MSG_WriteLong (&buf, fs);
					}
				}
				if (cl.playerview[seat].statsstr[i])
				{
					if (seat)
					{
						MSG_WriteByte (&buf, svcfte_choosesplitclient);
						MSG_WriteByte (&buf, seat);
					}
					MSG_WriteByte (&buf, svcfte_updatestatstring);
					MSG_WriteByte (&buf, i);
					MSG_WriteString (&buf, cl.playerview[seat].statsstr[i]);
				}

				if (buf.cursize > buf.maxsize/2)
					CL_WriteRecordDemoMessage (&buf, seq++);
			}
		}

		// get the client to check and download skins
		// when that is completed, a begin command will be issued
		MSG_WriteByte (&buf, svc_stufftext);
		MSG_WriteString (&buf, "skins\n");

		CL_WriteRecordDemoMessage (&buf, seq++);

		CL_WriteSetDemoMessage();

		//FIXME: make sure the deltas are reset

		// done
		break;
#ifdef Q2CLIENT
	case CP_QUAKE2:
		cls.demorecording = DPB_QUAKE2;

		MSG_WriteByte (&buf, svcq2_serverdata);
		if (cls.fteprotocolextensions)	//maintain demo compatability
		{
			MSG_WriteLong (&buf, PROTOCOL_VERSION_FTE);
			MSG_WriteLong (&buf, cls.fteprotocolextensions);
		}
		if (cls.fteprotocolextensions2)	//maintain demo compatability
		{
			MSG_WriteLong (&buf, PROTOCOL_VERSION_FTE2);
			MSG_WriteLong (&buf, cls.fteprotocolextensions2);
		}
		MSG_WriteLong (&buf, cls.protocol_q2);
		MSG_WriteLong (&buf, 0x80000000 + cl.servercount);
		MSG_WriteByte (&buf, 1);
		MSG_WriteString (&buf, gamedirfile);
		MSG_WriteShort (&buf, cl.playerview[0].playernum);
		MSG_WriteString (&buf, cl.levelname);

		for (i = 0; i < Q2MAX_CONFIGSTRINGS; i++)
		{
			const char *cs = Get_Q2ConfigString(i);
			if (buf.cursize + 4 + strlen(cs) > buf.maxsize)
			{
				CL_WriteRecordQ2DemoMessage (&buf);
				SZ_Clear (&buf);
			}

			MSG_WriteByte (&buf, svcq2_configstring);
			MSG_WriteShort (&buf, i);
			MSG_WriteString (&buf, cs);
		}

		CLQ2_WriteDemoBaselines(&buf);

		MSG_WriteByte (&buf, svcq2_stufftext);
		MSG_WriteString (&buf, "precache\n");
		CL_WriteRecordQ2DemoMessage (&buf);
		break;
#endif
#ifdef NQPROT
	case CP_NETQUAKE:
		//csqc stuff
		cls.demorecording = DPB_NETQUAKE;
		VFS_WRITE(cls.demooutfile, "-1\n", 3);	//stupid lame header thing.

		CLNQ_WriteServerData(&buf);
		MSG_WriteByte (&buf, svc_setpause);
		MSG_WriteByte (&buf, !!cl.paused);
		MSG_WriteByte (&buf, svc_setview);
		MSG_WriteEntity (&buf, cl.playerview[0].viewentity);

		MSG_WriteByte (&buf, svc_signonnum);
		MSG_WriteByte (&buf, 1);
		CL_WriteRecordDemoMessage (&buf, seq++);

		seq = CL_Record_ParticlesStaticsBaselines(&buf, seq);
		//fixme: brushes...
		MSG_WriteByte (&buf, svc_signonnum);
		MSG_WriteByte (&buf, 2);
		CL_WriteRecordDemoMessage (&buf, seq++);
		//fixme: clients
		seq = CL_Record_Lightstyles(&buf, seq);
		//fixme: stats
		MSG_WriteByte (&buf, svc_signonnum);
		MSG_WriteByte (&buf, 3);
		CL_WriteRecordDemoMessage (&buf, seq++);
		break;
#endif
	default:
		//this should have been caught earlier
		Con_Printf("Unable to begin demo recording with this network protocol\n");
		CL_Stop_f();
		break;
	}
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

	Q_snprintfz (name, sizeof(name), "%s", s);

	CL_Disconnect();

//
// open the demo file
//
	switch (cls.protocol)
	{
	default:
	case CP_QUAKEWORLD:
		cls.demorecording = DPB_QUAKEWORLD;
		COM_RequireExtension (name, ".qwd", sizeof(name));
		break;
#ifdef NQPROT
	case CP_NETQUAKE:
		cls.demorecording = DPB_NETQUAKE;
		COM_RequireExtension (name, ".dem", sizeof(name));
		break;
#endif
#ifdef Q2CLIENT
	case CP_QUAKE2:
		cls.demorecording = DPB_QUAKE2;
		COM_RequireExtension (name, ".dm2", sizeof(name));
		break;
#endif
	}

	cls.demooutfile = FS_OpenVFS (name, "wb", FS_GAME);
	if (!cls.demooutfile)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		cls.demorecording = DPB_NONE;
		return;
	}

	Con_Printf ("recording to %s.\n", name);

#ifdef NQPROT
	if (cls.demorecording == DPB_NETQUAKE)	//nq demos have some silly header.
		VFS_WRITE(cls.demooutfile, "-1\n", 3);
#endif

	CL_BeginServerReconnect();
}

/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDownloadedDemo(struct dl_download *dl);
void CL_PlayDemo_f (void)
{
	char *demoname;
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

	if (cls.state == ca_demostart)
		cls.state = ca_disconnected;

#ifdef WEBCLIENT
#if 1
	if (!strncmp(Cmd_Argv(1), "ftp://", 6) || !strncmp(Cmd_Argv(1), "http://", 7))
	{
		if (Cmd_ExecLevel == RESTRICT_LOCAL)
			HTTP_CL_Get(Cmd_Argv(1), COM_SkipPath(Cmd_Argv(1)), CL_PlayDownloadedDemo);
		return;
	}
#endif
#endif

	demoname = Cmd_Argv(1);
	if (*demoname == '#')
	{
		if (Cmd_FromGamecode())
			return;
		CL_PlayDemo(demoname+1, true);
	}
	else
		CL_PlayDemo(demoname, false);
}

void CL_DemoStreamFullyDownloaded(struct dl_download *dl)
{
	//let the file get closed by the demo playback code.
	dl->file = NULL;
	//kill the reference now that its done.
	if (cls.demoindownload == dl)
		cls.demoindownload = NULL;
}
//dl is provided so that we can receive files via chunked/gziped http downloads and on systems that don't provide sockets etc. its tracked so we can cancel the download if the client aborts playback early.
void CL_PlayDemoStream(vfsfile_t *file, struct dl_download *dl, char *filename, qboolean issyspath, int demotype, float bufferdelay)
{
	int protocol = CP_UNKNOWN;

	if (demotype == DPB_NONE)
	{
		//peek etc?
	}

	switch(demotype)
	{
	case DPB_EZTV:
	case DPB_MVD:
	case DPB_QUAKEWORLD:
		protocol = CP_QUAKEWORLD;
		break;
#ifdef Q2CLIENT
	case DPB_QUAKE2:
		protocol = CP_QUAKE2;
		break;
#endif
#ifdef NQPROT
	case DPB_NETQUAKE:
		protocol = CP_NETQUAKE;
		break;
#endif
	default:
		break;
	}

	if (protocol == CP_UNKNOWN)
	{
		Con_Printf ("ERROR: demo format not supported: \"%s\".\n", filename);
		return;
	}

	if (dl)
		dl->notifycomplete = CL_DemoStreamFullyDownloaded;

//
// disconnect from server
//
	CL_Disconnect_f ();

	demo_flushcache();

	NET_InitClient(true);
//
// open the demo file
//
	cls.demoindownload = dl;
	cls.demoinfile = file;
	if (!cls.demoinfile)
	{
		Con_Printf ("ERROR: couldn't open \"%s\".\n", filename);
		cls.demonum = -1;		// stop demo loop
		return;
	}
	if (filename)
	{
		Q_strncpyz (lastdemoname, filename, sizeof(lastdemoname));
		lastdemowassystempath = issyspath;
		Con_Printf ("Playing demo from %s.\n", filename);
	}

	cls.findtrack = (demotype == DPB_MVD || demotype == DPB_EZTV);

	cls.demoplayback = demotype;
	cls.protocol = protocol;
	cls.state = ca_demostart;
	net_message.packing = SZ_RAWBYTES;
	Netchan_Setup (NS_CLIENT, &cls.netchan, &net_from, 0);

	demtime = -bufferdelay;
	cls.demostarttime = 0;
	cl.gametime = -bufferdelay;
	cl.gametimemark = realtime;//demtime;
	if (demtime < -0.5)
		Con_Printf("Buffering for %g seconds\n", bufferdelay);
	cls.netchan.last_received=demtime;

	TP_ExecTrigger ("f_demostart", true);
}

vfsfile_t *CL_OpenFileInZipOrSys(char *name, qboolean usesystempath)
{
	if (usesystempath)
		return VFSOS_Open(name, "rb");
	else
		return CL_OpenFileInPackage(NULL, name);
}
//tries to determine the demo type
void CL_PlayDemoFile(vfsfile_t *f, char *demoname, qboolean issyspath)
{
#if defined(Q2CLIENT) || defined(NQPROT)
	//figure out where we started
	qofs_t start = VFS_TELL(f);
#endif

	if (!VFS_GETLEN (f))
	{
		VFS_CLOSE(f);
		Con_Printf ("demo \"%s\" is empty.\n", demoname);
		return;
	}

#ifdef Q2CLIENT
	{
		int len;
		char type;
		int protocol;
		//check if its a quake2 demo.
		VFS_READ(f, &len, sizeof(len));
		VFS_READ(f, &type, sizeof(type));
		VFS_READ(f, &protocol, sizeof(protocol));
		VFS_SEEK(f, start);
		len = LittleLong(len);
		protocol = LittleLong(protocol);
		if (len > 5 && type == svcq2_serverdata && protocol != 0)
		{
			CL_PlayDemoStream(f, NULL, demoname, issyspath, DPB_QUAKE2, 0);
			return;
		}
		VFS_SEEK(f, start);
	}
#endif

#ifdef NQPROT
	{
		int ft = 0, neg = false;
		char chr;
		//not quake2, check if its NQ
		//work out if the first line is a int for the track number.
		while ((VFS_READ(f, &chr, 1)==1) && (chr != '\n'))
		{
			if (chr == ' ')
				;
			else if (chr == '-')
				neg = true;
			else if (chr < '0' || chr > '9')
				break;
			else
				ft = ft * 10 + ((int)chr - '0');
		}
		if (neg)
			ft *= -1;
		if (chr == '\n')
		{
			CL_PlayDemoStream(f, NULL, demoname, issyspath, DPB_NETQUAKE, 0);
			return;
		}
		VFS_SEEK(f, start);
	}
#endif

	//its not NQ then. must be QuakeWorld, either .qwd or .mvd
	//could also be .qwz or .dmz or whatever that nq extension is. we don't support either.

	//mvd and qwd have no identifying markers, other than the extension.
	if (!Q_strcasecmp(demoname + strlen(demoname) - 3, "mvd") ||
		!Q_strcasecmp(demoname + strlen(demoname) - 6, "mvd.gz"))
		CL_PlayDemoStream(f, NULL, demoname, issyspath, DPB_MVD, 0);
	else
		CL_PlayDemoStream(f, NULL, demoname, issyspath, DPB_QUAKEWORLD, 0);
}
#ifdef WEBCLIENT
void CL_PlayDownloadedDemo(struct dl_download *dl)
{
	if (dl->status != DL_FINISHED || !dl->file)
		Con_Printf("Failed to download %s\n", dl->url);
	else
	{
		CL_PlayDemoFile(dl->file, dl->url, false);
		dl->file = NULL;
	}
}
#endif
void CL_PlayDemo(char *demoname, qboolean usesystempath)
{
	char	name[256];
	vfsfile_t *f;

//
// open the demo file
//
	Q_strncpyz (name, demoname, sizeof(name));
	COM_DefaultExtension (name, ".qwd", sizeof(name));
	f = CL_OpenFileInZipOrSys(name, usesystempath);
	if (!f)
	{
		Q_strncpyz (name, demoname, sizeof(name));
		COM_DefaultExtension (name, ".dem", sizeof(name));
		if (usesystempath)
			f = VFSOS_Open(name, "rb");
		else
			f = FS_OpenVFS(name, "rb", FS_GAME);
	}
	if (!f)
	{
		Q_strncpyz (name, demoname, sizeof(name));
		COM_DefaultExtension (name, ".mvd", sizeof(name));
		if (usesystempath)
			f = VFSOS_Open(name, "rb");
		else
			f = FS_OpenVFS(name, "rb", FS_GAME);
	}
	if (!f)
	{
		Q_strncpyz (name, demoname, sizeof(name));
		COM_DefaultExtension (name, ".dm2", sizeof(name));
		if (usesystempath)
			f = VFSOS_Open(name, "rb");
		else
			f = FS_OpenVFS(name, "rb", FS_GAME);
	}
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open \"%s\".\n", demoname);
		cls.demonum = -1;		// stop demo loop

		TP_ExecTrigger ("f_demoend", true);
		return;
	}
	Q_strncpyz (lastdemoname, demoname, sizeof(lastdemoname));

	CL_PlayDemoFile(f, name, usesystempath);
}

/*used with qtv*/
void CL_Demo_ClientCommand(char *commandtext)
{
	unsigned char b = 1;
	unsigned short len = LittleShort((unsigned short)(strlen(commandtext) + 4));
#ifdef warningmsg
#pragma warningmsg("this needs buffering safely")
#endif
	if (cls.demoplayback == DPB_EZTV)
	{
		VFS_WRITE(cls.demoinfile, &len, sizeof(len));
		VFS_WRITE(cls.demoinfile, &b, sizeof(b));
		VFS_WRITE(cls.demoinfile, commandtext, strlen(commandtext)+1);
	}
}

char qtvhostname[1024];
char qtvrequestbuffer[4096];
int qtvrequestsize;
char qtvrequestcmdbuffer[4096];
int qtvrequestcmdsize;
vfsfile_t *qtvrequest;

void CL_QTVPoll (void)
{
	char *s, *e, *colon;
	char *tail = NULL;
	int len;
	qboolean streamavailable = false;
	qboolean saidheader = false;
#ifndef NOBUILTINMENUS
	menu_t *sourcesmenu = NULL;
#endif
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

	if (qtvrequestcmdsize)
	{
		len = VFS_WRITE(qtvrequest, qtvrequestcmdbuffer, qtvrequestcmdsize);
		if (len > 0)
		{
			memmove(qtvrequestcmdbuffer, qtvrequestcmdbuffer+len, qtvrequestcmdsize-len);
			qtvrequestcmdsize -= len;
		}
	}

	for(;;)
	{
		len = VFS_READ(qtvrequest, qtvrequestbuffer+qtvrequestsize, (sizeof(qtvrequestbuffer) - qtvrequestsize -1 > 0)?1:0);
		if (len <= 0)
			break;
		qtvrequestsize += len;
	}
	qtvrequestbuffer[qtvrequestsize] = '\0';

	if (qtvrequestsize >= sizeof(qtvrequestbuffer) - 1)
	{
		//flag it as an error if the response is larger than we can handle.
		//this error gets ignored if the header is okay (any actual errors will get reported again by the demo code anyway), and only counts if the end of the reply header was not found.
		len = -1;
	}
	if (!qtvrequestsize && len == 0)
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
	{
		if (len < 0)
		{
			if (!qtvrequestsize)
				Con_Printf("Connection to QTV server closed without any reply.\n");
			else
				Con_Printf("invalid QTV handshake\n");
			SCR_SetLoadingStage(LS_NONE);
			VFS_CLOSE(qtvrequest);
			qtvrequest = NULL;
			qtvrequestsize = 0;
		}
		return;
	}
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
			{	//permanent printable error
				Con_Printf("QTV Error:\n%s\n", colon);
			}
			else if (!strcmp(s, "PRINT"))
			{	//printable error
				Con_Printf("QTV:\n%s\n", colon);
			}
			else if (!strcmp(s, "TERROR"))
			{	//temporary printable error
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
				while (*colon && *(unsigned char*)colon <= ' ')
					colon++;
				if (*colon)
					Con_Printf("streaming \"%s\" from qtv\n", colon);
				else
					Con_Printf("qtv connection established to %s\n", qtvhostname);
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

#ifndef NOBUILTINMENUS
				//now put it on a menu
				if (!sourcesmenu)
				{
					m_state = m_complex;
					Key_Dest_Add(kdm_emenu);
					sourcesmenu = M_CreateMenu(0);

					MC_AddPicture(sourcesmenu, 16, 4, 32, 144, "gfx/qplaque.lmp");
					MC_AddCenterPicture(sourcesmenu, 4, 24, "gfx/p_option.lmp");
				}
				if (init_numplayers == true && init_numviewers == true)
					MC_AddConsoleCommand(sourcesmenu, 42, 170, (sourcenum++)*8 + 32, va("%s (p%i, v%i)", srchost, numplayers, numviewers), va("qtvplay %i@%s\n", streamid, qtvhostname));
				//else
				//	FIXME: add error message here
#else
				(void)init_numviewers;
				(void)numviewers;
				(void)init_numplayers;
				(void)numplayers;
				(void)streamid;
				(void)sourcenum;
#endif
			}
			//end of sourcelist entry

			//from e to s, we have a line
			s = e+1;
		}
		e++;
	}

	if (streamavailable)
	{
		CL_PlayDemoStream(qtvrequest, NULL, NULL, false, iseztv?DPB_EZTV:DPB_MVD, BUFFERTIME);
		qtvrequest = NULL;
		demo_resetcache(qtvrequestsize - (tail-qtvrequestbuffer), tail);
		return;
	}

	SCR_SetLoadingStage(LS_NONE);
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

void CL_ParseQTVFile(vfsfile_t *f, const char *fname, qtvfile_t *result)
{
	char buffer[2048];
	char *s;
	memset(result, 0, sizeof(*result));
	if (!f)
	{
		Con_Printf("Couldn't open QTV file: %s\n", fname);
		return;
	}
	if (!VFS_GETS(f, buffer, sizeof(buffer)-1))
	{
		Con_Printf("Empty QTV file: %s\n", fname);
		VFS_CLOSE(f);
		return;
	}
	s = buffer;
	while (*s == ' ' || *s == '\t')
		s++;
	if (*s != '[')
	{
		Con_Printf("Bad QTV file: %s\n", fname);
		VFS_CLOSE(f);
		return;
	}
	s++;
	while (*s == ' ' || *s == '\t')
		s++;
	if (strnicmp(s, "QTV", 3))
	{
		Con_Printf("Bad QTV file: %s\n", fname);
		VFS_CLOSE(f);
		return;
	}
	s+=3;
	while (*s == ' ' || *s == '\t')
		s++;
	if (*s != ']')
	{
		Con_Printf("Bad QTV file: %s\n", fname);
		VFS_CLOSE(f);
		return;
	}
	s++;
	while (*s == ' ' || *s == '\t' || *s == '\r')
		s++;
	if (*s)
	{
		Con_Printf("Bad QTV file: %s\n", fname);
		VFS_CLOSE(f);
		return;
	}

	while (VFS_GETS(f, buffer, sizeof(buffer)-1))
	{
		s = COM_ParseToken(buffer, ":=");
		if (*s != '=' && *s != ':')
			s = "";
		else
			s++;

		if (!stricmp(com_token, "stream"))
		{
			result->connectiontype = QTVCT_STREAM;
			s = COM_ParseOut(s, result->server, sizeof(result->server));
		}
		else if (!stricmp(com_token, "connect"))
		{
			result->connectiontype = QTVCT_CONNECT;
			s = COM_ParseOut(s, result->server, sizeof(result->server));
		}
		else if (!stricmp(com_token, "join"))
		{
			result->connectiontype = QTVCT_JOIN;
			s = COM_ParseOut(s, result->server, sizeof(result->server));
		}
		else if (!stricmp(com_token, "observe"))
		{
			result->connectiontype = QTVCT_OBSERVE;
			s = COM_ParseOut(s, result->server, sizeof(result->server));
		}
		else if (!stricmp(com_token, "splash"))
		{
			s = COM_ParseOut(s, result->splashscreen, sizeof(result->server));
		}
	}
	VFS_CLOSE(f);
}

void CL_ParseQTVDescriptor(vfsfile_t *f, const char *name)
{
	char buffer[1024];
	char *s;

	if (!f)
	{
		Con_Printf("Couldn't open QTV file: %s\n", name);
		return;
	}
	while (VFS_GETS(f, buffer, sizeof(buffer)-1))
	{
		if (!strncmp(buffer, "Stream=", 7) || !strncmp(buffer, "Stream:", 7))
		{
			for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
			{
				if (*s == '\r' || *s == '\n' || *s == ';')
					*s = 0;
				else
					break;
			}
			s = buffer+7;
			while(*s && *s <= ' ')
				s++;
			Cbuf_AddText(va("qtvplay \"%s\"\n", s), Cmd_ExecLevel);
			break;
		}
		if (!strncmp(buffer, "Connect=", 8) || !strncmp(buffer, "Connect:", 8))
		{
			for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
			{
				if (*s == '\r' || *s == '\n' || *s == ';')
					*s = 0;
				else
					break;
			}
			s = buffer+8;
			while(*s && *s <= ' ')
				s++;
			Cbuf_AddText(va("connect \"%s\"\n", s), Cmd_ExecLevel);
			break;
		}
		if (!strncmp(buffer, "Join=", 5) || !strncmp(buffer, "Join:", 5))
		{
			for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
			{
				if (*s == '\r' || *s == '\n' || *s == ';')
					*s = 0;
				else
					break;
			}
			s = buffer+5;
			while(*s && *s <= ' ')
				s++;
			Cbuf_AddText(va("join \"%s\"\n", s), Cmd_ExecLevel);
			break;
		}
		if (!strncmp(buffer, "Observe=", 8) || !strncmp(buffer, "Observe:", 8))
		{
			for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
			{
				if (*s == '\r' || *s == '\n' || *s == ';')
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
	VFS_CLOSE(f);
}

void CL_QTVPlay_f (void)
{
	qboolean raw=0;
	char *connrequest;
	vfsfile_t *newf;
	char *host;
	char msg[4096];
	int msglen=0;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("Usage: qtvplay [stream@]hostname[:port] [password]\n");
		return;
	}

	connrequest = Cmd_Argv(1);

	if (*connrequest == '#')
	{
		//#FILENAME is a local system path
		CL_ParseQTVDescriptor(VFSOS_Open(connrequest+1, "rt"), connrequest+1);
		return;
	}
	strcpy(cls.servername, "qtv:");
	Q_strncpyz(cls.servername+4, connrequest, sizeof(cls.servername)-4);

	SCR_SetLoadingStage(LS_CONNECTION);

	host = connrequest;

	connrequest = strchrrev(connrequest, '@');
	if (connrequest)
		host = connrequest+1;
	Q_strncpyz(qtvhostname, host, sizeof(qtvhostname));
	newf = FS_OpenTCP(qtvhostname, 27599);

	if (!newf)
	{
		SCR_SetLoadingStage(LS_NONE);
		Con_Printf("Couldn't connect to proxy\n");
		return;
	}

	host = Cmd_Argv(1);
	if (connrequest)
		*connrequest = '\0';
	else
		host = NULL;

	if (qtvcl_forceversion1.ival)
	{
		Q_snprintfz(msg+msglen, sizeof(msg)-msglen,
					"QTV\n"
					"VERSION: 1.0\n");
	}
	else
	{
		Q_snprintfz(msg+msglen, sizeof(msg)-msglen,
					"QTV\n"
					"VERSION: 1.1\n");
	}
	msglen += strlen(msg+msglen);

	if (qtvcl_eztvextensions.ival)
	{
		raw = 0;

		Q_snprintfz(msg+msglen, sizeof(msg)-msglen,
				"QTV_EZQUAKE_EXT: 3\n"
				"USERINFO: %s\n", cls.userinfo[0]);
		msglen += strlen(msg+msglen);
	}
	else if (raw)
	{
		Q_snprintfz(msg+msglen, sizeof(msg)-msglen,
				"RAW: 1\n");
		msglen += strlen(msg+msglen);
	}
	if (host)
	{
		Q_snprintfz(msg+msglen, sizeof(msg)-msglen,
			"SOURCE: %s\n", host);
		msglen += strlen(msg+msglen);
	}
	else
	{
		Q_snprintfz(msg+msglen, sizeof(msg)-msglen,
				"SOURCELIST\n");
		msglen += strlen(msg+msglen);
	}

	Q_snprintfz(msg+msglen, sizeof(msg)-msglen,
				"\n");
	msglen += strlen(msg+msglen);

	if (raw)
	{
		VFS_WRITE(newf, msg, msglen);
		CL_PlayDemoStream(qtvrequest, NULL, qtvhostname, false, DPB_MVD, BUFFERTIME);
	}
	else
	{
		if (qtvrequest)
			VFS_CLOSE(qtvrequest);

		memcpy(qtvrequestcmdbuffer, msg, msglen);
		qtvrequestcmdsize = msglen;
		qtvrequest = newf;
		qtvrequestsize = 0;
	}
}

void CL_QTVList_f (void)
{
	char *connrequest;
	vfsfile_t *newf;
	newf = FS_OpenTCP(qtvhostname, 27599);

	if (!newf)
	{
		Con_Printf("Couldn't connect to proxy\n");
		return;
	}

	if (qtvcl_forceversion1.ival)
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
	newf = FS_OpenTCP(Cmd_Argv(1), 27599);

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
	cvar_t *vw;

	cls.timedemo = false;

	// loading frames don't count
	if (cls.td_startframe == -1)
	{
		Con_Printf ("demo didn't finish loading\n");
		frames = 0;
	}
	else
		frames = (host_framecount - cls.td_startframe) - 1;
	time = Sys_DoubleTime() - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);

	cls.td_startframe = 0;

	TP_ExecTrigger ("f_timedemoend", true);

	vw = Cvar_FindVar("vid_wait");
	Cvar_Set(vw, vw->string);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	cvar_t *vw;
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();

	if (cls.state != ca_demostart)
		return;

	vw = Cvar_FindVar("vid_wait");
	if (vw)
	{
		char *t = vw->string;
		vw->string = "0";
		vw->value = 0;
		Cvar_ForceCallback(vw);
		vw->string = t;
	}

//read the initial frame so load times don't count as part of the time
//	CL_ReadPackets();

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted

	cls.timedemo = true;
	cls.td_starttime = Sys_DoubleTime();
	cls.td_startframe = -1;
	cls.td_lastframe = -1;		// get a new message this frame
}

