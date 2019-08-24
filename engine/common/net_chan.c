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

#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#else
#include "unistd.h"
#endif

#define	PACKET_HEADER	8

/*

packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16  qport (only from client)
15  fragoffset (extension)
1	lastfrag (extension)

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher thatn the last reliable sequence, but without the correct even/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

if the sequence number is -1, the packet should be handled without a netcon

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection.  This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.

The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.

fragmentation works like IP, offset and morefrags. offset is *8 (decode: (offset&~1)<<2 to avoid stomping on the morefrags flag, this allows really jumbo packets with 18 bits of length)

*/

int		net_drop;
cvar_t	showpackets = CVAR("showpackets", "0");
cvar_t	showdrop = CVAR("showdrop", "0");
cvar_t	qport = CVARF("qport_", "0", CVAR_NOSAVE);
cvar_t	net_mtu = CVARD("net_mtu", "1440", "Specifies a maximum udp payload size, above which packets will be fragmented. If routers all worked properly this could be some massive value, and some massive value may work really nicely for lans. Use smaller values than the default if you're connecting through nested tunnels through routers that fail with IP fragmentation.");
cvar_t	net_compress = CVARD("net_compress", "0", "Enables huffman compression of network packets.");

cvar_t	pext_infoblobs = CVARD("_pext_infoblobs", "0", "RENAME ME WHEN STABLE. Enables the use of very large infokeys containing potentially invalid chars. Note that the userinfo is still limited by sv_userinfo_bytelimit and sv_userinfo_keylimit.");
cvar_t	pext_replacementdeltas = CVARD("pext_replacementdeltas", "1", "Enables the use of alternative nack-based entity deltas");
cvar_t	pext_predinfo = CVARD("pext_predinfo", "1", "Enables some extra things to support prediction over NQ protocols.");

#if defined(HAVE_CLIENT) && defined(HAVE_SERVER)
#define NET_SendPacket(c,s,d,t) NET_SendPacket(((c)!=NS_CLIENT)?svs.sockets:cls.sockets,s,d,t)
#elif defined(HAVE_SERVER)
#define NET_SendPacket(c,s,d,t) NET_SendPacket(svs.sockets,s,d,t)
#else
#define NET_SendPacket(c,s,d,t) NET_SendPacket(cls.sockets,s,d,t)
#endif

/*returns the entire bitmask of supported+enabled extensions*/
unsigned int Net_PextMask(unsigned int protover, qboolean fornq)
{
	unsigned int mask = 0;
	if (protover == PROTOCOL_VERSION_FTE1)
	{
	#ifdef PEXT_SCALE
		mask |= PEXT_SCALE;
	#endif
	#ifdef PEXT_LIGHTSTYLECOL
		mask |= PEXT_LIGHTSTYLECOL;
	#endif
	#ifdef PEXT_TRANS
		mask |= PEXT_TRANS;
	#endif
	#ifdef PEXT_VIEW2
		mask |= PEXT_VIEW2;
	#endif
	#ifdef PEXT_ACCURATETIMINGS
		#ifdef QUAKESTATS	//needs stat_time
			mask |= PEXT_ACCURATETIMINGS;
		#endif
	#endif
	#ifdef PEXT_ZLIBDL
		mask |= PEXT_ZLIBDL;
	#endif
	#ifdef PEXT_FATNESS
		mask |= PEXT_FATNESS;
	#endif
	#ifdef PEXT_HLBSP
		mask |= PEXT_HLBSP;
	#endif

	#ifdef PEXT_Q2BSP
		mask |= PEXT_Q2BSP;
	#endif
	#ifdef PEXT_Q3BSP
		mask |= PEXT_Q3BSP;
	#endif

	#ifdef PEXT_TE_BULLET
		mask |= PEXT_TE_BULLET;
	#endif
	#ifdef PEXT_HULLSIZE
		mask |= PEXT_HULLSIZE;
	#endif
	#ifdef PEXT_SETVIEW
		mask |= PEXT_SETVIEW;
	#endif
	#ifdef PEXT_MODELDBL
		mask |= PEXT_MODELDBL;
	#endif
	#ifdef PEXT_SOUNDDBL
		mask |= PEXT_SOUNDDBL;
	#endif
	#ifdef PEXT_VWEAP
		mask |= PEXT_VWEAP;
	#endif
	#ifdef PEXT_FLOATCOORDS
		mask |= PEXT_FLOATCOORDS;
	#endif
		mask |= PEXT_SPAWNSTATIC2;
		mask |= PEXT_COLOURMOD;
	#if MAX_SPLITS > 1
		mask |= PEXT_SPLITSCREEN;
	#endif
		mask |= PEXT_HEXEN2;
		mask |= PEXT_CUSTOMTEMPEFFECTS;
		mask |= PEXT_256PACKETENTITIES;
		mask |= PEXT_ENTITYDBL;
		mask |= PEXT_ENTITYDBL2;
		mask |= PEXT_SHOWPIC;
		mask |= PEXT_SETATTACHMENT;
	#ifdef PEXT_CHUNKEDDOWNLOADS
		mask |= PEXT_CHUNKEDDOWNLOADS;
	#endif
	#ifdef CSQC_DAT
		mask |= PEXT_CSQC;
	#endif
	#ifdef PEXT_DPFLAGS
		mask |= PEXT_DPFLAGS;
	#endif

		if (fornq)
		{
			//only ones that are tested
			mask &= 
#ifdef CSQC_DAT
					PEXT_CSQC |
#endif
#ifdef PEXT_Q2BSP
					PEXT_Q2BSP |
#endif
#ifdef PEXT_Q3BSP
					PEXT_Q3BSP |
#endif
					PEXT_FLOATCOORDS | PEXT_HLBSP;

			//these all depend fully upon the player/entity deltas, and don't make sense for NQ. Implement PEXT2_REPLACEMENTDELTAS instead.
			mask &= ~(PEXT_SCALE|PEXT_TRANS|PEXT_ACCURATETIMINGS|PEXT_FATNESS|PEXT_HULLSIZE|PEXT_MODELDBL|PEXT_ENTITYDBL|PEXT_ENTITYDBL2|PEXT_COLOURMOD|PEXT_SPAWNSTATIC2|PEXT_256PACKETENTITIES|PEXT_SETATTACHMENT|PEXT_DPFLAGS); 
		}
	}
	else if (protover == PROTOCOL_VERSION_FTE2)
	{
		mask |= PEXT2_PRYDONCURSOR;
	#ifdef PEXT2_VOICECHAT
		mask |= PEXT2_VOICECHAT;
	#endif
		mask |= PEXT2_SETANGLEDELTA;

		if (pext_replacementdeltas.ival)
		{
			mask |= PEXT2_REPLACEMENTDELTAS;
			if (/*fornq &&*/ pext_predinfo.ival)
				mask |= PEXT2_PREDINFO;
		}

		if (pext_infoblobs.ival)
			mask |= PEXT2_INFOBLOBS;

		if (MAX_CLIENTS != QWMAX_CLIENTS)
			mask |= PEXT2_MAXPLAYERS;

		if (mask & PEXT2_REPLACEMENTDELTAS)
			mask |= PEXT2_NEWSIZEENCODING;	//use if we can

		if (fornq)
		{
			//only ones that are tested
			mask &= /*PEXT2_PRYDONCURSOR |*/ PEXT2_VOICECHAT | PEXT2_SETANGLEDELTA | PEXT2_REPLACEMENTDELTAS | PEXT2_MAXPLAYERS | PEXT2_PREDINFO | PEXT2_NEWSIZEENCODING;
		}
//		else
//			mask &= ~PEXT2_PREDINFO;
	}
	else if (protover == PROTOCOL_VERSION_EZQUAKE1)
	{
		mask = EZPEXT1_FLOATENTCOORDS|EZPEXT1_SETANGLEREASON;
	}

	return mask;
}

/*
===============
Netchan_Init

===============
*/
void Netchan_Init (void)
{
	static char qportstr[16];
	int		port;

	// pick a port value that should be nice and random
#ifdef _WIN32
	port = (time(NULL)) & 0xffff;
#elif defined(NACL)
	port = ((int)(getpid()) * time(NULL)) & 0xffff;
#else
	port = ((int)(getpid()+getuid()*1000) * time(NULL)) & 0xffff;
#endif
	Q_snprintfz(qportstr, sizeof(qportstr), "%i", port);
	qport.enginevalue = qportstr;

	Cvar_Register (&pext_predinfo, "Protocol Extensions");
	Cvar_Register (&pext_replacementdeltas, "Protocol Extensions");
	Cvar_Register (&pext_infoblobs, "Protocol Extensions");
	Cvar_Register (&showpackets, "Networking");
	Cvar_Register (&showdrop, "Networking");
	Cvar_Register (&qport, "Networking");
	Cvar_Register (&net_mtu, "Networking");
	Cvar_Register (&net_compress, "Networking");
}

/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand (netsrc_t sock, netadr_t *adr, int length, qbyte *data)
{
	sizebuf_t	send;
	qbyte		send_buf[MAX_QWMSGLEN + PACKET_HEADER];

// write the packet header
	memset(&send, 0, sizeof(send));
	send.data = send_buf;
	send.maxsize = sizeof(send_buf);
	send.cursize = 0;
	
	MSG_WriteLong (&send, -1);	// -1 sequence means out of band
	SZ_Write (&send, data, length);

// send the datagram
	//zoid, no input in demo playback mode
#ifndef SERVERONLY
	if (!cls.demoplayback)
#endif
		NET_SendPacket (sock, send.cursize, send.data, adr);
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void VARGS Netchan_OutOfBandPrint (netsrc_t sock, netadr_t *adr, char *format, ...)
{
	va_list		argptr;
	static char		string[8192];		// ??? why static?
	
	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);


	Netchan_OutOfBand (sock, adr, strlen(string), (qbyte *)string);
}
#ifndef CLIENTONLY
void VARGS Netchan_OutOfBandTPrintf (netsrc_t sock, netadr_t *adr, int language, translation_t text, ...)
{
	va_list		argptr;
	static char		string[8192];		// ??? why static?

	const char *format = langtext(text, language);

	string[0] = A2C_PRINT;
	
	va_start (argptr, text);
	vsnprintf (string+1,sizeof(string)-1, format,argptr);
	va_end (argptr);


	Netchan_OutOfBand (sock, adr, strlen(string), (qbyte *)string);
}
#endif
/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t *adr, int qport)
{
	memset (chan, 0, sizeof(*chan));

	chan->sock = sock;
	chan->remote_address = *adr;
	chan->last_received = realtime;
#ifdef NQPROT
	chan->nqreliable_allowed = true;
#endif
	chan->incoming_unreliable = -1;
	
	chan->message.data = chan->message_buf;
	chan->message.allowoverflow = true;
	chan->message.maxsize = MAX_QWMSGLEN;

	chan->qport = qport;

	chan->qportsize = 2;
}


/*
===============
Netchan_CanPacket

Returns true if the bandwidth choke isn't active
================
*/
#define	MAX_BACKUP	200
qboolean Netchan_CanPacket (netchan_t *chan, int rate)
{
	if (chan->remote_address.type == NA_LOOPBACK)
		return true;	//don't ever drop packets due to possible routing problems when there is no routing.
	if (!rate)
		return true;
	if (chan->cleartime < realtime + 0.25)//(MAX_BACKUP/(float)rate))
		return true;
	return false;
}

int Netchan_CanBytes (netchan_t *chan, int rate)
{
	const double slop = 0.25;
	if (chan->remote_address.type == NA_LOOPBACK)
		return 0x7fffffff;	//don't ever drop packets due to possible routing problems when there is no routing.
	if (!rate)
		return 0x7fffffff;
	return ((realtime+slop)-chan->cleartime)*rate;
}

void Netchan_Block (netchan_t *chan, int bytes, int rate)
{
	if (rate)
	{
		if (chan->cleartime < realtime-0.25)	//0.25 allows it to be a little bursty.
			chan->cleartime = realtime + (bytes/(float)rate);
		else
			chan->cleartime += bytes/(float)rate;
	}
}


/*
===============
Netchan_CanReliable

Returns true if the bandwidth choke isn't 
================
*/
qboolean Netchan_CanReliable (netchan_t *chan, int rate)
{
	if (chan->reliable_length)
		return false;			// waiting for ack
	return Netchan_CanPacket (chan, rate);
}

#ifdef SERVERONLY
qboolean ServerPaused(void);
#endif

#ifdef NQPROT
qboolean NQNetChan_Process(netchan_t *chan)
{
	int header;
	int sequence;
	int drop;

	chan->bytesin += net_message.cursize;
	MSG_BeginReading (chan->netprim);

	header = LongSwap(MSG_ReadLong());
	if (net_message.cursize != (header & NETFLAG_LENGTH_MASK))
		return false;	//size was wrong, couldn't have been ours.

	if (header & NETFLAG_CTL)
		return false;	//huh?

	sequence = LongSwap(MSG_ReadLong());

	if (header & NETFLAG_ACK)
	{
		if (sequence == chan->reliable_sequence)
		{
			chan->reliable_start += MAX_NQDATAGRAM;
			if (chan->reliable_start >= chan->reliable_length)
			{
				chan->reliable_length = 0;	//they got the entire message
				chan->reliable_start = 0;
			}
			chan->incoming_reliable_acknowledged = chan->reliable_sequence;
			chan->reliable_sequence++;
			chan->nqreliable_allowed = true;

			chan->last_received = realtime;
		}
		else if (sequence < chan->reliable_sequence)
		{
			if (showdrop.ival)
				Con_Printf("Stale ack recieved\n");
		}
		else if (sequence > chan->reliable_sequence)
		{
			if (showdrop.ival)
				Con_Printf("Future ack recieved\n");
		}

		if (showpackets.value)
			Con_Printf ("in  %s a=%i %i\n"
						, chan->sock != NS_SERVER?"s2c":"c2s"
						, sequence
						, 0);

		return false;	//don't try execing the 'payload'. I hate ack packets.
	}

	if (header & NETFLAG_UNRELIABLE)
	{
		if (sequence <= chan->incoming_unreliable)
		{
			if (showdrop.ival)
				Con_Printf("Stale datagram recieved (%i<=%i)\n", sequence, chan->incoming_unreliable);
			return false;
		}
		drop = sequence - chan->incoming_unreliable - 1;
		if (drop > 0)
		{
			if (showdrop.ival)
				Con_Printf("Dropped %i datagrams (%i - %i)\n", drop, chan->incoming_unreliable+1, sequence-1);
			chan->drop_count += drop;
		}
		chan->incoming_unreliable = sequence;



//		chan->frame_latency = chan->frame_latency*OLD_AVG
//			+ (chan->outgoing_sequence-sequence_ack)*(1.0-OLD_AVG);
		chan->frame_rate = chan->frame_rate*OLD_AVG
			+ (realtime-chan->last_received)*(1.0-OLD_AVG);		

		chan->last_received = realtime;

		chan->incoming_acknowledged++;
		chan->good_count++;

		if (showpackets.value)
			Con_Printf ("in  %s u=%i %i\n"
						, chan->sock != NS_SERVER?"s2c":"c2s"
						, chan->incoming_unreliable
						, net_message.cursize);
		return true;
	}
	if (header & NETFLAG_DATA)
	{
		int runt[2];
		//always reply. a stale sequence probably means our ack got lost.
		runt[0] = BigLong(NETFLAG_ACK | 8);
		runt[1] = BigLong(sequence);
		NET_SendPacket (chan->sock, 8, runt, &net_from);
		if (showpackets.value)
			Con_Printf ("out %s a=%i %i\n"
						, chan->sock == NS_SERVER?"s2c":"c2s"
						, sequence
						, 0);

		chan->last_received = realtime;
		if (sequence == chan->incoming_reliable_sequence)
		{
			chan->incoming_reliable_sequence++;

			if (chan->in_fragment_length + net_message.cursize-8 >= sizeof(chan->in_fragment_buf))
			{
				chan->fatal_error = true;
				return false;
			}

			memcpy(chan->in_fragment_buf + chan->in_fragment_length, net_message.data+8, net_message.cursize-8);
			chan->in_fragment_length += net_message.cursize-8;

			if (header & NETFLAG_EOM)
			{
				SZ_Clear(&net_message);
				SZ_Write(&net_message, chan->in_fragment_buf, chan->in_fragment_length);
				chan->in_fragment_length = 0;
				MSG_BeginReading(chan->netprim);

				if (showpackets.value)
					Con_Printf ("in  %s r=%i %i\n"
								, chan->sock != NS_SERVER?"s2c":"c2s"
								, sequence
								, net_message.cursize);
				return true;	//we can read it now
			}
		}
		else
		{
			if (showdrop.ival)
				Con_Printf("Stale reliable (%i)\n", sequence);
		}

		return false;
	}

	return false;	//not supported.
}
#endif

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
int Netchan_Transmit (netchan_t *chan, int length, qbyte *data, int rate)
{
	sizebuf_t	send;
	qbyte		send_buf[MAX_OVERALLMSGLEN + PACKET_HEADER];
	qboolean	send_reliable;
	char		remote_adr[MAX_ADR_SIZE];
	unsigned	w1, w2;
	int			i;
	neterr_t e;

	int dupes = chan->dupe;
	int availbytes = Netchan_CanBytes(chan, rate);

#ifdef NQPROT
	if (chan->isnqprotocol)
	{
		int sentsize = 0;

		send.data = send_buf;
		send.maxsize = MAX_NQMSGLEN + PACKET_HEADER;
		send.cursize = 0;

		/*unreliables flood out, but reliables are tied to server sequences*/
		if (chan->nqreliable_resendtime < realtime)
			chan->nqreliable_allowed = true;
		if (chan->nqreliable_allowed)
		{
			//consume the new reliable when we can.
			if (!chan->reliable_length && chan->message.cursize && chan->nqunreliableonly != 1)
			{
				if (chan->nqunreliableonly == 2)
					chan->nqunreliableonly = 1;
				memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
				chan->reliable_length = chan->message.cursize;
				chan->reliable_start = 0;
				chan->message.cursize = 0;
			}

			i = chan->reliable_length - chan->reliable_start;
			if (i>0)
			{
				MSG_WriteLong(&send, 0);
				MSG_WriteLong(&send, LongSwap(chan->reliable_sequence));

				//limit the payload length to nq's datagram max size.
				//relax the limitation if its reliable (ie: over tcp) where its assumed to have no real limit
				if (i > MAX_NQDATAGRAM && !NET_AddrIsReliable(&chan->remote_address))
					i = MAX_NQDATAGRAM;

				SZ_Write (&send, chan->reliable_buf+chan->reliable_start, i);

				if (chan->reliable_start+i == chan->reliable_length)
				{
					if (send.cursize + length < send.maxsize)
					{	//throw the unreliable packet into the same one as the reliable (but not sent reliably)
//						SZ_Write (&send, data, length);
//						length = 0;
					}

					*(int*)send_buf = BigLong(NETFLAG_DATA | NETFLAG_EOM | send.cursize);
				}
				else
					*(int*)send_buf = BigLong(NETFLAG_DATA | send.cursize);

				chan->bytesout += send.cursize;
				sentsize += send.cursize;
				if (showpackets.value)
					Con_Printf ("out %s r s=%i %i\n"
						, chan->sock == NS_SERVER?"s2c":"c2s"
						, chan->reliable_sequence
						, send.cursize);
				chan->nqreliable_allowed = false;
				chan->nqreliable_resendtime = realtime + 0.3;	//resend reliables after 0.3 seconds. nq transports suck.

				if (NET_SendPacket (chan->sock, send.cursize, send.data, &chan->remote_address) == NETERR_SENT && (
					NET_AddrIsReliable(&chan->remote_address) || chan->nqunreliableonly==3	))
				{	//if over tcp (or we're dropping the connection), everything is assumed to be reliable. pretend it got acked now.
					//if we get an ack later, then who cares.
					chan->reliable_start += i;
					if (chan->reliable_start >= chan->reliable_length)
					{
						chan->reliable_length = 0;	//they got the entire message
						chan->reliable_start = 0;
					}
					chan->incoming_reliable_acknowledged = chan->reliable_sequence;
					chan->reliable_sequence++;
					chan->nqreliable_allowed = true;
				}
				send.cursize = 0;
			}
		}

		//send out the unreliable (if still unsent)
		if (length)
		{
			MSG_WriteLong(&send, 0);
			MSG_WriteLong(&send, LongSwap(chan->outgoing_unreliable));
			chan->outgoing_unreliable++;

			SZ_Write (&send, data, length);

			*(int*)send_buf = BigLong(NETFLAG_UNRELIABLE | send.cursize);
			NET_SendPacket (chan->sock, send.cursize, send.data, &chan->remote_address);
			sentsize += send.cursize;

			if (showpackets.value)
				Con_Printf ("out %s u=%i %i\n"
						, chan->sock == NS_SERVER?"s2c":"c2s"
						, chan->outgoing_unreliable-1
						, send.cursize);
			send.cursize = 0;
		}
		chan->bytesout += sentsize;
		Netchan_Block(chan, sentsize, rate);
		return sentsize;
	}
#endif

// check for message overflow
	if (chan->message.overflowed)
	{
		chan->fatal_error = true;
		Con_TPrintf ("%s: Outgoing message overflow\n"
			, NET_AdrToString (remote_adr, sizeof(remote_adr), &chan->remote_address));
		return 0;
	}

// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if (chan->incoming_acknowledged > chan->last_reliable_sequence
	&& chan->incoming_reliable_acknowledged != chan->reliable_sequence)
		send_reliable = true;

// if the reliable transmit buffer is empty, copy the current message out
	if (!chan->reliable_length && chan->message.cursize)
	{
		memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
		send_reliable = true;
	}

// write the packet header
	send.data = send_buf;
	send.maxsize = (chan->mtu?chan->mtu:MAX_QWMSGLEN) + PACKET_HEADER;
	send.cursize = 0;

	w1 = chan->outgoing_sequence | (send_reliable<<31);
	w2 = chan->incoming_sequence | (chan->incoming_reliable_sequence<<31);

	chan->outgoing_sequence++;

	MSG_WriteLong (&send, w1);
	MSG_WriteLong (&send, w2);

	// send the qport if we are a client
#ifndef SERVERONLY
	if (chan->sock == NS_CLIENT)
	{
		if (chan->qportsize == 2)
			MSG_WriteShort (&send, chan->qport);
		else if (chan->qportsize == 1)
			MSG_WriteByte (&send, chan->qport&0xff);
	}
#endif

	if (chan->pext_fragmentation)
	{
		//allow the max size to be bigger, sending everything available
		send.maxsize = MAX_OVERALLMSGLEN + PACKET_HEADER;
		MSG_WriteShort(&send, 0);
	}

// copy the reliable message to the packet first
	if (send_reliable)
	{
		if (send.maxsize - send.cursize < chan->reliable_length)
		{
			if (!chan->fatal_error)
			{
				chan->fatal_error = true;
				Con_TPrintf ("%s: Path MTU is lower than %u\n"
					, NET_AdrToString (remote_adr, sizeof(remote_adr), &chan->remote_address), chan->reliable_length);
			}
			chan->outgoing_sequence--;
			return 0;
		}
		SZ_Write (&send, chan->reliable_buf, chan->reliable_length);
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}
	
// add the unreliable part if space is available
	if (send.maxsize - send.cursize >= length)
		SZ_Write (&send, data, length);

// send the datagram
	i = chan->outgoing_sequence & (MAX_LATENT-1);
	chan->outgoing_size[i] = send.cursize;
	chan->outgoing_time[i] = realtime;

#ifdef HUFFNETWORK
	if (chan->compresstable)
	{
		//int oldsize = send.cursize;
		Huff_CompressPacket(chan->compresstable, &send, 8 + ((chan->sock == NS_CLIENT)?2:0) + (chan->pext_fragmentation?2:0));
//		Con_Printf("%i becomes %i\n", oldsize, send.cursize);
//		Huff_DecompressPacket(&send, (chan->sock == NS_CLIENT)?10:8);
	}
#endif

	e = NETERR_SENT;
	//zoid, no input in demo playback mode
#ifndef SERVERONLY
	if (!cls.demoplayback)
#endif
	{
		int hsz = 10 + ((chan->sock == NS_CLIENT)?chan->qportsize:0); /*header size, if fragmentation is in use*/
		dupes = min(chan->dupe, availbytes / send.cursize);

		if ((!chan->pext_fragmentation))// || send.cursize < ((chan->mtu - hsz)&~7))
		{	//vanilla sends
			for (i = -1; i < dupes && e == NETERR_SENT; i++)
				e = NET_SendPacket (chan->sock, send.cursize, send.data, &chan->remote_address);
			send.cursize += send.cursize * i;

			//ipv4 'guarentees' mtu sizes of at least 560ish.
			//our reliable/backbuf messages are limited to 1024 bytes.
			//this means that large reliables may be unsendable.
			if (e == NETERR_MTU && chan->mtu > 560)
			{
				Con_Printf("Reducing MSS to %i\n", chan->mtu);
				chan->mtu -= 10;
			}
		}
		else
		{	//fte's fragmentaton protocol
			int offset = 0, no;
			qboolean more;
			int outbytes = 0;
			int fragbytes;

			/*FIXME: splurge over a number of frames, if we have an outgoing reliable*/

			/*send the additional parts, adding new headers within the previous packet*/
			do
			{
				no = offset + chan->mtu - hsz;
				if (no < send.cursize-hsz)
				{
					no &= ~7;
					more = true;
				}
				else
				{
					no = send.cursize-hsz;
					more = false;
				}

				*(int*)&send.data[(offset) + 0] = LittleLong(w1);
				*(int*)&send.data[(offset) + 4] = LittleLong(w2);
#ifndef SERVERONLY
				if (chan->sock == NS_CLIENT)
				{
					if (chan->qportsize == 2)
						*(short*)&send.data[offset + hsz-4] = LittleShort(chan->qport);
					else if (chan->qportsize == 1)
						*(qbyte*)&send.data[offset + hsz-3] = chan->qport&0xff;
				}
#endif
				*(short*)&send.data[offset + hsz-2] = LittleShort((offset>>2) | (more?1:0));

				if (e == NETERR_SENT)
				{
					for (i = -1; i < dupes && e == NETERR_SENT; i++)
					{
						fragbytes = (no - offset) + hsz;
						e = NET_SendPacket (chan->sock, fragbytes, send.data + offset, &chan->remote_address);
						outbytes += fragbytes;
						if (e == NETERR_MTU && !offset && chan->mtu > 560)
						{
							chan->mtu -= 16;
							Con_Printf("Reducing MSS to %i\n", chan->mtu);
							no = offset;
							more = true;
							break;
						}
					}
				}
				offset = no;
			} while(more);
			send.cursize = outbytes;
		}
	}

	if (e == NETERR_SENT)
	{
		chan->bytesout += send.cursize;
		Netchan_Block(chan, send.cursize, rate);
	}
#ifdef SERVERONLY
	if (ServerPaused())
		chan->cleartime = realtime;
#endif

	if (showpackets.value)
	{
		char *errtext;
		switch(e)
		{
		case NETERR_SENT: errtext = ""; break;
		case NETERR_NOROUTE: errtext = " unroutable"; break;
		case NETERR_DISCONNECTED: errtext = " disconnected"; break;
		case NETERR_MTU: errtext = " mss exceeded"; break;
		case NETERR_CLOGGED: errtext = " conjestion"; break;
		default: errtext = " unk error"; break;
		}
		Con_Printf ("%f %s --> s=%i(%i) a=%i(%i) %i%s\n"
			, Sys_DoubleTime()
			, chan->sock == NS_SERVER?"s2c":"c2s"
			, chan->outgoing_sequence
			, send_reliable
			, chan->incoming_sequence
			, chan->incoming_reliable_sequence
			, send.cursize,
			errtext);
	}
	return send.cursize;

}

/*
=================
Netchan_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
qboolean Netchan_Process (netchan_t *chan)
{
	unsigned		sequence, sequence_ack;
	unsigned		reliable_ack, reliable_message;
	char			adr[MAX_ADR_SIZE];
	int offset;

	if (
#ifndef SERVERONLY
			!cls.demoplayback && 
#endif
			!NET_CompareAdr (&net_from, &chan->remote_address))
		return false;

	chan->bytesin += net_message.cursize;

// get sequence numbers		
	MSG_BeginReading (chan->netprim);
	sequence = MSG_ReadLong ();
	sequence_ack = MSG_ReadLong ();

	// skip over the qport if we are a server (its handled elsewhere)
#ifndef CLIENTONLY
	if (chan->sock == NS_SERVER)
		MSG_ReadShort ();
#endif

	if (chan->pext_fragmentation)
		offset = (unsigned short)MSG_ReadShort();
	else
		offset = 0;

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1u<<31);
	sequence_ack &= ~(1u<<31);

	if (showpackets.value)
		Con_Printf ("%f %s <-- s=%i(%i) a=%i(%i) %i%s\n"
			, Sys_DoubleTime()
			, chan->sock == NS_SERVER?"c2s":"s2c"
			, sequence
			, reliable_message
			, sequence_ack
			, reliable_ack
			, net_message.cursize
			, offset?" frag":"");

// get a rate estimation
#if 0
	if (chan->outgoing_sequence - sequence_ack < MAX_LATENT)
	{
		int				i;
		double			time, rate;
	
		i = sequence_ack & (MAX_LATENT - 1);
		time = realtime - chan->outgoing_time[i];
		time -= 0.1;	// subtract 100 ms
		if (time <= 0)
		{	// gotta be a digital link for <100 ms ping
			if (chan->rate > 1.0/5000)
				chan->rate = 1.0/5000;
		}
		else
		{
			if (chan->outgoing_size[i] < 512)
			{	// only deal with small messages
				rate = chan->outgoing_size[i]/time;
				if (rate > 5000)
					rate = 5000;
				rate = 1.0/rate;
				if (chan->rate > rate)
					chan->rate = rate;
			}
		}
	}
#endif

//
// discard stale or duplicated packets
//
	if (sequence <= (unsigned)chan->incoming_sequence)
	{
		if (showdrop.value)
			Con_TPrintf ("%s:Out of order packet %i at %i\n"
				, NET_AdrToString (adr, sizeof(adr), &chan->remote_address)
				,  sequence
				, chan->incoming_sequence);
		return false;
	}

	if (offset)
	{
		int len = net_message.cursize - msg_readcount;
		qboolean more = false;
		if (offset & 1)
		{
			more = true;
			offset &= ~1;
		}
		offset = offset << 2;

		if (offset + len > sizeof(chan->in_fragment_buf)) /*stop the overflow*/
		{
			if (showdrop.value)
				Con_Printf("Dropping packet - too many fragments\n");
			return false;
		}
		if (chan->incoming_unreliable != sequence)
		{
			if (chan->in_fragment_length && showdrop.ival)
				Con_Printf("final fragment lost (%i). dropping entire packet\n", offset);
			/*sequence doesn't match, forget the old*/
			chan->in_fragment_length = 0;
			chan->incoming_unreliable = sequence;
		}
		if (offset != chan->in_fragment_length)
		{
			if (showdrop.ival)
				Con_Printf("prior fragment lost (%i-%i). dropping entire packet\n", offset, chan->in_fragment_length);
			return false; /*dropped one*/
		}

		memcpy(chan->in_fragment_buf + offset, net_message.data + msg_readcount, len);
		chan->in_fragment_length += len;

		if (more)
		{
			/*nothing to process yet*/
			return false;
		}
		memcpy(net_message.data, chan->in_fragment_buf, chan->in_fragment_length);
		msg_readcount = 0;
		net_message.cursize = chan->in_fragment_length;

		if (showpackets.value)
			Con_Printf ("<-- s=%i(%i) a=%i(%i) %i Recombined\n"
				, sequence
				, reliable_message
				, sequence_ack
				, reliable_ack
				, net_message.cursize);

		chan->incoming_unreliable = 0;
		chan->in_fragment_length = 0;
	}
	else
	{
		/*kill any pending reliable*/
		chan->incoming_unreliable = 0;
		chan->in_fragment_length = 0;
	}

//
// dropped packets don't keep the message from being used
//
	net_drop = sequence - (chan->incoming_sequence+1);
	if (net_drop > 0)
	{
		chan->drop_count += 1;

		if (showdrop.value)
			Con_TPrintf ("%s:Dropped %i packets at %i\n"
			, NET_AdrToString (adr, sizeof(adr), &chan->remote_address)
			, sequence-(chan->incoming_sequence+1)
			, sequence);
	}

//
// if the current outgoing reliable message has been acknowledged
// clear the buffer to make way for the next
//
	if (reliable_ack == (unsigned)chan->reliable_sequence)
		chan->reliable_length = 0;	// it has been received

//
// if this message contains a reliable message, bump incoming_reliable_sequence 
//
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message)
		chan->incoming_reliable_sequence ^= 1;

//
// the message can now be read from the current message pointer
// update statistics counters
//
	chan->frame_latency = chan->frame_latency*OLD_AVG
		+ (chan->outgoing_sequence-sequence_ack)*(1.0-OLD_AVG);
	chan->frame_rate = chan->frame_rate*OLD_AVG
		+ (realtime-chan->last_received)*(1.0-OLD_AVG);		
	chan->good_count += 1;

	chan->last_received = realtime;

#ifdef HUFFNETWORK
	if (chan->compresstable)
	{
//		Huff_CompressPacket(&net_message, (chan->sock == NS_SERVER)?10:8);
		Huff_DecompressPacket(chan->compresstable, &net_message, msg_readcount);
	}
#endif

	return true;
}

