#include "quakedef.h"
#include "netinc.h"

typedef struct
{
	unsigned short msgtype;
	unsigned short msglen;
	unsigned int magiccookie;
	unsigned int transactid[3];
} stunhdr_t;
typedef struct
{
	unsigned short attrtype;
	unsigned short attrlen;
} stunattr_t;
#ifdef SUPPORT_ICE
/*
Interactive Connectivity Establishment (rfc 5245)
find out your peer's potential ports.
spam your peer with stun packets.
see what sticks.
the 'controller' assigns some final candidate pair to ensure that both peers send+receive from a single connection.
if no candidates are available, try using stun to find public nat addresses.

in fte, a 'pair' is actually in terms of each local socket and remote address. hopefully that won't cause too much weirdness.

stun test packets must contain all sorts of info. username+integrity+fingerprint for validation. priority+usecandidate+icecontrol(ing) to decree the priority of any new remote candidates, whether its finished, and just who decides whether its finished.
peers don't like it when those are missing.

host candidates - addresses that are directly known
server reflexive candidates - addresses that we found from some public stun server
peer reflexive candidates - addresses that our peer finds out about as we spam them
relayed candidates - some sort of socks5 or something proxy.


Note: Even after the ICE connection becomes active, you should continue to collect local candidates and transmit them to the peer out of band.
this allows the connection to pick a new route if some router dies (like a relay kicking us).

tcp rtp framing should generally be done with a 16-bit network-endian length prefix followed by the data.
*/

struct icecandidate_s
{
	struct icecandinfo_s info;

	struct icecandidate_s *next;

	netadr_t peer;
	//peer needs telling or something.
	qboolean dirty;

	//these are bitmasks. one bit for each local socket.
	unsigned int reachable;
	unsigned int tried;
};
struct icestate_s
{
	struct icestate_s *next;
	void *module;

	netadr_t chosenpeer;

	netadr_t pubstunserver;
	unsigned int stunretry;	//once a second, extended to once a minite on reply
	char *stunserver;//where to get our public ip from.
	int stunport;
	unsigned int stunrnd[3];

	unsigned int timeout;	//time when we consider the connection dead
	unsigned int keepalive;	//sent periodically...
	unsigned int retries;	//bumped after each round of connectivity checks. affects future intervals.
	enum iceproto_e proto;
	enum icemode_e mode;
	qboolean controlled;	//controller chooses final ports.
	enum icestate_e state;
	char *conname;		//internal id.
	char *friendlyname;	//who you're talking to.

	unsigned int originid;	//should be randomish
	unsigned int originversion;//bumped each time something in the sdp changes.
	char originaddress[16];

	struct icecandidate_s *lc;
	char *lpwd;
	char *lufrag;

	struct icecandidate_s *rc;
	char *rpwd;
	char *rufrag;

	unsigned int tiehigh;
	unsigned int tielow;
	int foundation;

	ftenet_connections_t *connections;

	struct icecodecslot_s
	{
		//FIXME: we should probably include decode state in here somehow so multiple connections don't clobber each other.
		int id;
		char *name;
	} codecslot[34];		//96-127. don't really need to care about other ones.
};
static struct icestate_s *icelist;

static struct icecodecslot_s *ICE_GetCodecSlot(struct icestate_s *ice, int slot)
{
	if (slot >= 96 && slot < 96+32)
		return &ice->codecslot[slot-96];
	else if (slot == 0)
		return &ice->codecslot[32];
	else if (slot == 8)
		return &ice->codecslot[33];
	return NULL;
}


#if !defined(SERVERONLY) && defined(VOICECHAT)
extern cvar_t snd_voip_send;
struct rtpheader_s
{
	unsigned char v2_p1_x1_cc4;
	unsigned char m1_pt7;
	unsigned short seq;
	unsigned int timestamp;
	unsigned int ssrc;
	unsigned int csrc[1];	//sized according to cc
};
void S_Voip_RTP_Parse(unsigned short sequence, const char *codec, const unsigned char *data, unsigned int datalen);
qboolean S_Voip_RTP_CodecOkay(const char *codec);
qboolean NET_RTP_Parse(void)
{
	struct rtpheader_s *rtpheader = (void*)net_message.data;
	if (net_message.cursize >= sizeof(*rtpheader) && (rtpheader->v2_p1_x1_cc4 & 0xc0) == 0x80)
	{
		int hlen;
		int padding = 0;
		struct icestate_s *con;
		//make sure this really came from an accepted rtp stream
		//note that an rtp connection equal to the game connection will likely mess up when sequences start to get big
		//(especially problematic in sane clients that start with a random sequence)
		for (con = icelist; con; con = con->next)
		{
			if (con->state != ICE_INACTIVE && (con->proto == ICEP_VIDEO || con->proto == ICEP_VOICE) && NET_CompareAdr(&net_from, &con->chosenpeer))
			{
				struct icecodecslot_s *codec = ICE_GetCodecSlot(con, rtpheader->m1_pt7 & 0x7f);
				if (codec)	//untracked slot
				{
					char *codecname = codec->name;
					if (!codecname)	//inactive slot
						continue;

					if (rtpheader->v2_p1_x1_cc4 & 0x20)
						padding = net_message.data[net_message.cursize-1];
					hlen = sizeof(*rtpheader);
					hlen += ((rtpheader->v2_p1_x1_cc4 & 0xf)-1) * sizeof(int);
					if (con->proto == ICEP_VOICE)
						S_Voip_RTP_Parse((unsigned short)BigShort(rtpheader->seq), codecname, hlen+(char*)(rtpheader), net_message.cursize - padding - hlen);
//					if (con->proto == ICEP_VIDEO)
//						S_Video_RTP_Parse((unsigned short)BigShort(rtpheader->seq), codecname, hlen+(char*)(rtpheader), net_message.cursize - padding - hlen);
					return true;
				}
			}
		}
	}
	return false;
}
qboolean NET_RTP_Active(void)
{
	struct icestate_s *con;
	for (con = icelist; con; con = con->next)
	{
		if (con->state == ICE_CONNECTED && con->proto == ICEP_VOICE)
			return true;
	}
	return false;
}
qboolean NET_RTP_Transmit(unsigned int sequence, unsigned int timestamp, const char *codec, char *cdata, int clength)
{
	sizebuf_t buf;
	char pdata[512];
	int i;
	struct icestate_s *con;
	qboolean built = false;

	memset(&buf, 0, sizeof(buf));
	buf.maxsize = sizeof(pdata);
	buf.cursize = 0;
	buf.allowoverflow = true;
	buf.data = pdata;

	for (con = icelist; con; con = con->next)
	{
		if (con->state == ICE_CONNECTED && con->proto == ICEP_VOICE)
		{
			for (i = 0; i < countof(con->codecslot); i++)
			{
				if (con->codecslot[i].name && !strcmp(con->codecslot[i].name, codec))
				{
					if (!built)
					{
						built = true;
						MSG_WriteByte(&buf, (2u<<6) | (0u<<5) | (0u<<4) | (0<<0));	//v2_p1_x1_cc4
						MSG_WriteByte(&buf, (0u<<7) | (con->codecslot[i].id<<0));	//m1_pt7
						MSG_WriteShort(&buf, BigShort(sequence&0xffff));	//seq
						MSG_WriteLong(&buf, BigLong(timestamp));	//timestamp
						MSG_WriteLong(&buf, BigLong(0));			//ssrc
						SZ_Write(&buf, cdata, clength);
						if (buf.overflowed)
							return built;
					}
					NET_SendPacket(cls.sockets, buf.cursize, buf.data, &con->chosenpeer);
					break;
				}
			}
		}
	}
	return built;
}
#endif



struct icestate_s *QDECL ICE_Find(void *module, char *conname)
{
	struct icestate_s *con;

	for (con = icelist; con; con = con->next)
	{
		if (con->module == module && !strcmp(con->conname, conname))
			return con;
	}
	return NULL;
}
ftenet_connections_t *ICE_PickConnection(struct icestate_s *con)
{
	if (con->connections)
		return con->connections;
	switch(con->proto)
	{
	default:
		break;
#ifndef SERVERONLY
	case ICEP_VOICE:
	case ICEP_QWCLIENT:
		return cls.sockets;
#endif
#ifndef CLIENTONLY
	case ICEP_QWSERVER:
		return svs.sockets;
#endif
	}
	return NULL;
}
struct icestate_s *QDECL ICE_Create(void *module, const char *conname, const char *peername, enum icemode_e mode, enum iceproto_e proto)
{
	ftenet_connections_t *collection;
	struct icestate_s *con;

	//only allow modes that we actually support.
	if (mode != ICEM_RAW && mode != ICEM_ICE)
		return NULL;

	//only allow protocols that we actually support.
	switch(proto)
	{
	default:
		return NULL;
//	case ICEP_VIDEO:
//		collection = NULL;
//		break;
#if !defined(SERVERONLY) && defined(VOICECHAT)
	case ICEP_VOICE:
	case ICEP_VIDEO:
		collection = cls.sockets;
		if (!collection)
			NET_InitClient(false);
		break;
#endif
#ifndef SERVERONLY
	case ICEP_QWCLIENT:
		collection = cls.sockets;
		if (!collection)
			NET_InitClient(false);
		break;
#endif
#ifndef CLIENTONLY
	case ICEP_QWSERVER:
		collection = svs.sockets;
		break;
#endif
	}

	if (!conname)
	{
		int rnd[2];
		Sys_RandomBytes((void*)rnd, sizeof(rnd));
		conname = va("fte%08x%08x", rnd[0], rnd[1]);
	}
	
	con = Z_Malloc(sizeof(*con));
	con->conname = Z_StrDup(conname);
	con->friendlyname = Z_StrDup(peername);
	con->proto = proto;
	con->rpwd = Z_StrDup("");
	con->rufrag = Z_StrDup("");
	Sys_RandomBytes((void*)&con->originid, sizeof(con->originid));
	con->originversion = 1;
	Q_strncpyz(con->originaddress, "127.0.0.1", sizeof(con->originaddress));

	con->mode = mode;

	if (!collection)
	{
		con->connections = collection = FTENET_CreateCollection(true);
		FTENET_AddToCollection(collection, "UDP", "0", NA_IP, NP_DGRAM);
		FTENET_AddToCollection(collection, "natpmp", "natpmp://5351", NA_IP, NP_NATPMP);
	}

	con->next = icelist;
	icelist = con;

	{
		int rnd[1];	//'must have at least 24 bits randomness'
		Sys_RandomBytes((void*)rnd, sizeof(rnd));
		con->lufrag = Z_StrDup(va("%08x", rnd[0]));
	}
	{
		int rnd[4];	//'must have at least 128 bits randomness'
		Sys_RandomBytes((void*)rnd, sizeof(rnd));
		con->lpwd = Z_StrDup(va("%08x%08x%08x%08x", rnd[0], rnd[1], rnd[2], rnd[3]));
	}

	Sys_RandomBytes((void*)&con->tiehigh, sizeof(con->tiehigh));
	Sys_RandomBytes((void*)&con->tielow, sizeof(con->tielow));

	if (collection)
	{
		int i, m;

		netadr_t	addr[64];
		struct ftenet_generic_connection_s			*gcon[sizeof(addr)/sizeof(addr[0])];
		unsigned int			flags[sizeof(addr)/sizeof(addr[0])];

		m = NET_EnumerateAddresses(collection, gcon, flags, addr, sizeof(addr)/sizeof(addr[0]));

		for (i = 0; i < m; i++)
		{
			if (addr[i].type == NA_IP || addr[i].type == NA_IPV6)
			{
//				if (flags[i] & ADDR_REFLEX)
//					ICE_AddLCandidateInfo(con, &addr[i], i, ICE_SRFLX); //FIXME: needs reladdr relport info
//				else
					ICE_AddLCandidateInfo(con, &addr[i], i, ICE_HOST);
			}
		}
	}

	return con;
}
#include "zlib.h"
//if either remotecand is null, new packets will be sent to all.
static qboolean ICE_SendSpam(struct icestate_s *con)
{
	struct icecandidate_s *rc;
	int i;
	int bestlocal = -1;
	struct icecandidate_s *bestpeer = NULL;
	ftenet_connections_t *collection = ICE_PickConnection(con);
	if (!collection)
		return false;

	//only send one ping to each.
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (collection->conn[i])
		{
			for(rc = con->rc; rc; rc = rc->next)
			{
				if (!(rc->tried & (1u<<i)) && !(rc->tried & (1u<<i)))
				{
					//fixme: no local priority. a multihomed machine will try the same ip from different ports.
					if (!bestpeer || bestpeer->info.priority < rc->info.priority)
					{
						bestpeer = rc;
						bestlocal = i;
					}
				}
			}
		}
	}


	if (bestpeer && bestlocal >= 0)
	{
		netadr_t to;
		sizebuf_t buf;
		char data[512];
		char integ[20];
		int crc;
		qboolean usecandidate = false;
		memset(&buf, 0, sizeof(buf));
		buf.maxsize = sizeof(data);
		buf.cursize = 0;
		buf.data = data;

		bestpeer->tried |= (1u<<bestlocal);

		if (!NET_StringToAdr(bestpeer->info.addr, bestpeer->info.port, &to))
			return true;
		Con_DPrintf("Spam %i -> %s:%i\n", bestlocal, bestpeer->info.addr, bestpeer->info.port);

		if (!con->controlled && NET_CompareAdr(&to, &con->chosenpeer))
			usecandidate = true;

		MSG_WriteShort(&buf, BigShort(0x0001));
		MSG_WriteShort(&buf, 0);	//fill in later
		MSG_WriteLong(&buf, BigLong(0x2112a442));			//magic
		MSG_WriteLong(&buf, BigLong(0));					//randomid
		MSG_WriteLong(&buf, BigLong(0));					//randomid
		MSG_WriteLong(&buf, BigLong(0x80000000|bestlocal));	//randomid

		if (usecandidate)
		{
			MSG_WriteShort(&buf, BigShort(0x25));//ICE-USE-CANDIDATE
			MSG_WriteShort(&buf, BigShort(0));	//just a flag, so no payload to this attribute
		}

		//username
		MSG_WriteShort(&buf, BigShort(0x6));	//USERNAME
		MSG_WriteShort(&buf, BigShort(strlen(con->rufrag) + 1 + strlen(con->lufrag)));
		SZ_Write(&buf, con->rufrag, strlen(con->rufrag));
		MSG_WriteChar(&buf, ':');
		SZ_Write(&buf, con->lufrag, strlen(con->lufrag));
		while(buf.cursize&3)
			MSG_WriteChar(&buf, 0);	//pad

		//priority
		MSG_WriteShort(&buf, BigShort(0x24));//ICE-PRIORITY
		MSG_WriteShort(&buf, BigShort(4));
		MSG_WriteLong(&buf, 0);	//FIXME. should be set to:
			//			priority =	(2^24)*(type preference) +
			//						(2^8)*(local preference) +
			//						(2^0)*(256 - component ID)
			//type preference should be 126 and is a function of the candidate type (direct sending should be highest priority at 126)
			//local preference should reflect multi-homed preferences. ipv4+ipv6 count as multihomed.
			//component ID should be 1 (rtcp would be 2 if we supported it)

		//these two attributes carry a random 64bit tie-breaker.
		//the controller is the one with the highest number.
		if (con->controlled)
		{
			MSG_WriteShort(&buf, BigShort(0x8029));//ICE-CONTROLLED
			MSG_WriteShort(&buf, BigShort(8));
			MSG_WriteLong(&buf, BigLong(con->tiehigh));
			MSG_WriteLong(&buf, BigLong(con->tielow));
		}
		else
		{
			MSG_WriteShort(&buf, BigShort(0x802A));//ICE-CONTROLLING
			MSG_WriteShort(&buf, BigShort(8));
			MSG_WriteLong(&buf, BigLong(con->tiehigh));
			MSG_WriteLong(&buf, BigLong(con->tielow));
		}

		//message integrity is a bit annoying
		data[2] = ((buf.cursize+4+sizeof(integ)-20)>>8)&0xff;	//hashed header length is up to the end of the hmac attribute
		data[3] = ((buf.cursize+4+sizeof(integ)-20)>>0)&0xff;
		//but the hash is to the start of the attribute's header
		HMAC(SHA1_m, integ, sizeof(integ), data, buf.cursize, con->rpwd, strlen(con->rpwd));
		MSG_WriteShort(&buf, BigShort(0x8));	//MESSAGE-INTEGRITY
		MSG_WriteShort(&buf, BigShort(20));	//sha1 key length
		SZ_Write(&buf, integ, sizeof(integ));	//integrity data

		data[2] = ((buf.cursize+8-20)>>8)&0xff;	//dummy length
		data[3] = ((buf.cursize+8-20)>>0)&0xff;
		crc = crc32(0, data, buf.cursize)^0x5354554e;
		MSG_WriteShort(&buf, BigShort(0x8028));	//FINGERPRINT
		MSG_WriteShort(&buf, BigShort(sizeof(crc)));
		MSG_WriteLong(&buf, BigLong(crc));

		//fill in the length (for the fourth time, after filling in the integrity and fingerprint)
		data[2] = ((buf.cursize-20)>>8)&0xff;
		data[3] = ((buf.cursize-20)>>0)&0xff;

		collection->conn[bestlocal]->SendPacket(collection->conn[bestlocal], buf.cursize, data, &to);
		return true;
	}
	return false;
}

void ICE_ToStunServer(struct icestate_s *con)
{
	sizebuf_t buf;
	char data[512];
	int crc;
	ftenet_connections_t *collection = ICE_PickConnection(con);
	if (!collection)
		return;
	if (!con->stunrnd[0])
		Sys_RandomBytes((char*)con->stunrnd, sizeof(con->stunrnd));

	Con_DPrintf("Spam stun %s\n", NET_AdrToString(data, sizeof(data), &con->pubstunserver));

	memset(&buf, 0, sizeof(buf));
	buf.maxsize = sizeof(data);
	buf.cursize = 0;
	buf.data = data;

	MSG_WriteShort(&buf, BigShort(0x0001));
	MSG_WriteShort(&buf, 0);	//fill in later
	MSG_WriteLong(&buf, BigLong(0x2112a442));
	MSG_WriteLong(&buf, BigLong(con->stunrnd[0]));	//randomid
	MSG_WriteLong(&buf, BigLong(con->stunrnd[1]));	//randomid
	MSG_WriteLong(&buf, BigLong(con->stunrnd[2]));	//randomid

	data[2] = ((buf.cursize+8-20)>>8)&0xff;	//dummy length
	data[3] = ((buf.cursize+8-20)>>0)&0xff;
	crc = crc32(0, data, buf.cursize)^0x5354554e;
	MSG_WriteShort(&buf, BigShort(0x8028));	//FINGERPRINT
	MSG_WriteShort(&buf, BigShort(sizeof(crc)));
	MSG_WriteLong(&buf, BigLong(crc));

	//fill in the length (for the fourth time, after filling in the integrity and fingerprint)
	data[2] = ((buf.cursize-20)>>8)&0xff;
	data[3] = ((buf.cursize-20)>>0)&0xff;

	NET_SendPacket(collection, buf.cursize, data, &con->pubstunserver);
}

void QDECL ICE_AddRCandidateInfo(struct icestate_s *con, struct icecandinfo_s *n)
{
	struct icecandidate_s *o;
	qboolean isnew;
	netadr_t peer;
	//I don't give a damn about rtpc.
	if (n->component != 1)
		return;
	if (n->transport != 0)
		return;	//only UDP is supported.

	if (!NET_StringToAdr(n->addr, n->port, &peer))
		return;

	if (peer.type == NA_IP)
	{
		//ignore invalid addresses
		if (!peer.address.ip[0] && !peer.address.ip[1] && !peer.address.ip[2] && !peer.address.ip[3])
			return;
	}

	for (o = con->rc; o; o = o->next)
	{
		//not sure that updating candidates is particuarly useful tbh, but hey.
		if (!strcmp(o->info.candidateid, n->candidateid))
			break;
	}
	if (!o)
	{
		o = Z_Malloc(sizeof(*o));
		o->next = con->rc;
		con->rc = o;
		Q_strncpyz(o->info.candidateid, n->candidateid, sizeof(o->info.candidateid));

		isnew = true;
	}
	else
	{
		isnew = false;
	}
	Q_strncpyz(o->info.addr, n->addr, sizeof(o->info.addr));
	o->info.port = n->port;
	o->info.type = n->type;
	o->info.priority = n->priority;
	o->info.network = n->network;
	o->info.generation = n->generation;
	o->info.foundation = n->foundation;
	o->info.component = n->component;
	o->info.transport = n->transport;
	o->dirty = true;
	o->peer = peer;
	o->tried = 0;
	o->reachable = 0;

	Con_DPrintf("%s remote candidate %s: [%s]:%i\n", isnew?"Added":"Updated", o->info.candidateid, o->info.addr, o->info.port);
}

qboolean QDECL ICE_Set(struct icestate_s *con, const char *prop, const char *value)
{
	if (!strcmp(prop, "state"))
	{
		int oldstate = con->state;
		if (!strcmp(value, STRINGIFY(ICE_CONNECTING)))
			con->state = ICE_CONNECTING;
		else if (!strcmp(value, STRINGIFY(ICE_INACTIVE)))
			con->state = ICE_INACTIVE;
		else if (!strcmp(value, STRINGIFY(ICE_FAILED)))
			con->state = ICE_FAILED;
		else if (!strcmp(value, STRINGIFY(ICE_CONNECTED)))
			con->state = ICE_CONNECTED;
		else
		{
			Con_Printf("ICE_Set invalid state %s\n", value);
			con->state = ICE_INACTIVE;
		}
		con->timeout = Sys_Milliseconds();

		con->retries = 0;

#ifndef SERVERONLY
		if (con->state == ICE_CONNECTING && (con->proto == ICEP_QWCLIENT || con->proto == ICEP_VOICE))
			NET_InitClient(false);
#endif

		if (oldstate != con->state && con->state == ICE_CONNECTED)
		{
			if (con->chosenpeer.type == NA_INVALID)
			{
				con->state = ICE_FAILED;
				Con_Printf("ICE failed. peer not valid.\n");
			}
#ifndef SERVERONLY
			else if (con->proto == ICEP_QWCLIENT)
			{
				char msg[256];
				//FIXME: should make a proper connection type for this so we can switch to other candidates if one route goes down
//				Con_Printf("Try typing connect %s\n", NET_AdrToString(msg, sizeof(msg), &con->chosenpeer));
				Cbuf_AddText(va("\nconnect \"%s\"\n", NET_AdrToString(msg, sizeof(msg), &con->chosenpeer)), RESTRICT_LOCAL);
			}
#endif
#ifndef CLIENTONLY
			else if (con->proto == ICEP_QWSERVER)
			{
				net_from = con->chosenpeer;
				SVC_GetChallenge(false);
			}
#endif
			if (con->state == ICE_CONNECTED)
				Con_Printf("%s connection established.\n", con->proto == ICEP_VOICE?"voice":"Quake");
		}

#if !defined(SERVERONLY) && defined(VOICECHAT)
		snd_voip_send.ival = (snd_voip_send.ival & ~4) | (NET_RTP_Active()?4:0);
#endif
	}
	else if (!strcmp(prop, "controlled"))
		con->controlled = !!atoi(value);
	else if (!strcmp(prop, "controller"))
		con->controlled = !atoi(value);
	else if (!strncmp(prop, "codec", 5))
	{
		struct icecodecslot_s *codec = ICE_GetCodecSlot(con, atoi(prop+5));
		if (!codec)
			return false;
		codec->id = atoi(prop+5);
#if !defined(SERVERONLY) && defined(VOICECHAT)
		if (!S_Voip_RTP_CodecOkay(value))
#endif
		{
			Z_Free(codec->name);
			codec->name = NULL;
			return false;
		}
		Z_Free(codec->name);
		codec->name = Z_StrDup(value);
	}
	else if (!strcmp(prop, "rufrag"))
	{
		Z_Free(con->rufrag);
		con->rufrag = Z_StrDup(value);
	}
	else if (!strcmp(prop, "rpwd"))
	{
		Z_Free(con->rpwd);
		con->rpwd = Z_StrDup(value);
	}
	else if (!strcmp(prop, "stunip"))
	{
		Z_Free(con->stunserver);
		con->stunserver = Z_StrDup(value);
		if (!NET_StringToAdr(con->stunserver, con->stunport, &con->pubstunserver))
			return false;
	}
	else if (!strcmp(prop, "stunport"))
	{
		con->stunport = atoi(value);
		if (con->stunserver)
			if (!NET_StringToAdr(con->stunserver, con->stunport, &con->pubstunserver))
				return false;
	}
	else if (!strcmp(prop, "sdp"))
	{
		const char *eol;
		for (; *value; value = eol)
		{
			eol = strchr(value, '\n');
			if (!eol)
				eol = value+strlen(value);
			else
				eol++;

			if      (!strncmp(value, "a=ice-pwd:", 10))
				ICE_Set(con, "rpwd", value+10);
			else if (!strncmp(value, "a=ice-ufrag:", 12))
				ICE_Set(con, "rufrag", value+12);
			else if (!strncmp(value, "a=rtpmap:", 9))
			{
				char name[64];
				int codec;
				char *sl;
				value += 9;
				codec = strtoul(value, (char**)&value, 0);
				if (*value == ' ') value++;

				COM_ParseOut(value, name, sizeof(name));
				sl = strchr(name, '/');
				if (sl)
					*sl = '@';
				ICE_Set(con, va("codec%i", codec), name);
			}
			else if (!strncmp(value, "a=candidate:", 12))
			{
				struct icecandinfo_s n;
				memset(&n, 0, sizeof(n));

				value += 12;
				n.foundation = strtoul(value, (char**)&value, 0);

				if(*value == ' ')value++;
				n.component = strtoul(value, (char**)&value, 0);

				if(*value == ' ')value++;
				if (!strncmp(value, "UDP ", 4))
				{
					n.transport = 0;
					value += 3;
				}
				else
					break;

				if(*value == ' ')value++;
				n.priority = strtoul(value, (char**)&value, 0);

				if(*value == ' ')value++;
				value = COM_ParseOut(value, n.addr, sizeof(n.addr));
				if (!value) break;

				if(*value == ' ')value++;
				n.port = strtoul(value, (char**)&value, 0);
				
				if(*value == ' ')value++;
				if (strncmp(value, "typ ", 4)) break;
				value += 3;

				if(*value == ' ')value++;
				if (!strncmp(value, "host", 4))
					n.type = ICE_HOST;
				else if (!strncmp(value, "srflx", 4))
					n.type = ICE_SRFLX;
				else if (!strncmp(value, "prflx", 4))
					n.type = ICE_PRFLX;
				else if (!strncmp(value, "relay", 4))
					n.type = ICE_RELAY;
				else
					break;

				while (value < eol)
				{
					if(*value == ' ')value++;
					if (!strncmp(value, "raddr ", 6))
					{
						value += 6;
						value = COM_ParseOut(value, n.reladdr, sizeof(n.reladdr));
						if (!value)
							break;
					}
					else if (!strncmp(value, "rport ", 6))
					{
						value += 6;
						n.relport = strtoul(value, (char**)&value, 0);
					}
					else
					{
						//this is meant to be extensible.
						while (*value && value < eol && *value != ' ')
							value++;
						if(*value == ' ')value++;
						while (*value && value < eol && *value != ' ')
							value++;
					}
				}
				ICE_AddRCandidateInfo(con, &n);
			}
		}
	}
	else
		return false;
	return true;
}
static char *ICE_CandidateToSDP(struct icecandidate_s *can, char *value, size_t valuelen)
{
	char *ctype = "?";
	switch(can->info.type)
	{
	default:
	case ICE_HOST: ctype = "host"; break;
	case ICE_SRFLX: ctype = "srflx"; break;
	case ICE_PRFLX: ctype = "prflx"; break;
	case ICE_RELAY: ctype = "relay"; break;
	}
	Q_snprintfz(value, valuelen, "candidate:%i %i %s %i %s %i typ %s",
			can->info.foundation,
			can->info.component,
			can->info.transport==0?"udp":"ERROR",
			can->info.priority,
			can->info.addr,
			can->info.port,
			ctype
			);
	Q_strncatz(value, va(" generation %i", can->info.generation), valuelen);
	if (can->info.type != ICE_HOST)
	{
		Q_strncatz(value, va(" raddr %s", can->info.reladdr), valuelen);
		Q_strncatz(value, va(" rport %i", can->info.relport), valuelen);
	}

	return value;
}
qboolean QDECL ICE_Get(struct icestate_s *con, const char *prop, char *value, size_t valuelen)
{
	if (!strcmp(prop, "sid"))
		Q_strncpyz(value, con->conname, valuelen);
	else if (!strcmp(prop, "state"))
		Q_snprintfz(value, valuelen, "%i", con->state);
	else if (!strcmp(prop, "lufrag"))
		Q_strncpyz(value, con->lufrag, valuelen);
	else if (!strcmp(prop, "lpwd"))
		Q_strncpyz(value, con->lpwd, valuelen);
	else if (!strncmp(prop, "codec", 5))
	{
		int codecid = atoi(prop+5);
		struct icecodecslot_s *codec = ICE_GetCodecSlot(con, atoi(prop+5));
		if (!codec || codec->id != codecid)
			return false;
		if (codec->name)
			Q_strncpyz(value, codec->name, valuelen);
		else
			Q_strncpyz(value, "", valuelen);
	}
	else if (!strcmp(prop, "newlc"))
	{
		struct icecandidate_s *can;
		Q_strncpyz(value, "0", valuelen);
		for (can = con->lc; can; can = can->next)
		{
			if (can->dirty)
			{
				Q_strncpyz(value, "1", valuelen);
				break;
			}
		}
	}
	else if (!strcmp(prop, "sdp"))
	{
		struct icecandidate_s *can;
		netadr_t sender;
		char tmpstr[MAX_QPATH], *at;
		int i;

		{
			netadr_t	addr[1];
			struct ftenet_generic_connection_s *gcon[countof(addr)];
			int			flags[countof(addr)];

			if (!NET_EnumerateAddresses(ICE_PickConnection(con), gcon, flags, addr, countof(addr)))
				sender.type = NA_INVALID;
			else
				sender = *addr;
		}

		Q_strncpyz(value, "v=0\n", valuelen);
		Q_strncatz(value, va("o=%s %u %u IN IP4 %s\n", "-", con->originid, con->originversion, con->originaddress), valuelen);	//originator
		Q_strncatz(value, va("s=%s\n", con->conname), valuelen);
		Q_strncatz(value, va("c=IN %s %s\n", sender.type==NA_IPV6?"IP6":"IP4", NET_BaseAdrToString(tmpstr, sizeof(tmpstr), &sender)), valuelen);
		Q_strncatz(value, "t=0 0\n", valuelen);
		Q_strncatz(value, va("a=ice-pwd:%s\n", con->lpwd), valuelen);
		Q_strncatz(value, va("a=ice-ufrag:%s\n", con->lufrag), valuelen);

		if (con->proto == ICEP_QWSERVER || con->proto == ICEP_QWCLIENT)
		{
#ifdef HAVE_DTLS
			Q_strncatz(value, "m=application 9 DTLS/SCTP 5000\n", valuelen);
#endif
		}

		/*fixme: merge the codecs into a single media line*/
		for (i = 0; i < countof(con->codecslot); i++)
		{
			int id = con->codecslot[i].id;
			if (!con->codecslot[i].name)
				continue;

			Q_strncatz(value, va("m=audio %i RTP/AVP %i\n", sender.port, id), valuelen);
			Q_strncatz(value, va("b=RS:0\n"), valuelen);
			Q_strncatz(value, va("b=RR:0\n"), valuelen);
			Q_strncpyz(tmpstr, con->codecslot[i].name, sizeof(tmpstr));
			at = strchr(tmpstr, '@');
			if (at)
			{
				*at = '/';
				Q_strncatz(value, va("a=rtpmap:%i %s\n", id, tmpstr), valuelen);
			}
			else
				Q_strncatz(value, va("a=rtpmap:%i %s/%i\n", id, tmpstr, 8000), valuelen);

			for (can = con->lc; can; can = can->next)
			{
				char canline[256];
				can->dirty = false;	//doesn't matter now.
				Q_strncatz(value, "a=\n", valuelen);
				ICE_CandidateToSDP(can, canline, sizeof(canline));
				Q_strncatz(value, canline, valuelen);
				Q_strncatz(value, "\n", valuelen);
			}
		}
	}
	else
		return false;
	return true;
}
qboolean QDECL ICE_GetLCandidateSDP(struct icestate_s *con, char *out, size_t outsize)
{
	struct icecandidate_s *can;
	for (can = con->lc; can; can = can->next)
	{
		if (can->dirty)
		{
			can->dirty = false;

			ICE_CandidateToSDP(can, out, outsize);
			return true;
		}
	}
	return false;
}
struct icecandinfo_s *QDECL ICE_GetLCandidateInfo(struct icestate_s *con)
{
	struct icecandidate_s *can;
	for (can = con->lc; can; can = can->next)
	{
		if (can->dirty)
		{
			can->dirty = false;
			return &can->info;
		}
	}
	return NULL;
}
//adrno is 0 if the type is anything but host.
void QDECL ICE_AddLCandidateInfo(struct icestate_s *con, netadr_t *adr, int adrno, int type)
{
	int rnd[2];
	struct icecandidate_s *cand;
	if (!con)
		return;

	for (cand = con->lc; cand; cand = cand->next)
	{
		if (NET_CompareAdr(adr, &cand->peer))
			break;
	}
	if (cand)
		return;	//DUPE

	cand = Z_Malloc(sizeof(*cand));
	cand->next = con->lc;
	con->lc = cand;
	NET_BaseAdrToString(cand->info.addr, sizeof(cand->info.addr), adr);
	cand->info.port = ntohs(adr->port);
	cand->info.type = type;
	cand->info.generation = 0;
	cand->info.foundation = 1;
	cand->info.component = 1;
	cand->info.network = adr->connum;
	cand->dirty = true;

	Sys_RandomBytes((void*)rnd, sizeof(rnd));
	Q_strncpyz(cand->info.candidateid, va("x%08x%08x", rnd[0], rnd[1]), sizeof(cand->info.candidateid));

	cand->info.priority =
		(1<<24)*(126) +
		(1<<8)*((adr->type == NA_IP?32768:0)+cand->info.network*256+(255-adrno)) +
		(1<<0)*(256 - cand->info.component);
}
void QDECL ICE_AddLCandidateConn(ftenet_connections_t *col, netadr_t *addr, int type)
{
	struct icestate_s *ice;
	for (ice = icelist; ice; ice = ice->next)
	{
		if (ICE_PickConnection(ice) == col)
			ICE_AddLCandidateInfo(ice, addr, 0, type);
	}
}

static void ICE_Destroy(struct icestate_s *con)
{
	if (con->connections)
		FTENET_CloseCollection(con->connections);
	//has already been unlinked
	Z_Free(con);
}

//send pings to establish/keep the connection alive
void ICE_Tick(void)
{
	struct icestate_s *con;

	for (con = icelist; con; con = con->next)
	{
		switch(con->mode)
		{
		case ICEM_RAW:
			//raw doesn't do handshakes or keepalives. it should just directly connect.
			//raw just uses the first (assumed only) option
			if (con->state == ICE_CONNECTING)
			{
				struct icecandidate_s *rc;
				rc = con->rc;
				if (!rc || !NET_StringToAdr(rc->info.addr, rc->info.port, &con->chosenpeer))
					con->chosenpeer.type = NA_INVALID;
				ICE_Set(con, "state", STRINGIFY(ICE_CONNECTED));
			}
			break;
		case ICEM_ICE:
			if (con->state == ICE_CONNECTING || con->state == ICE_FAILED)
			{
				unsigned int curtime = Sys_Milliseconds();

				if (con->stunretry < curtime && con->pubstunserver.type != NA_INVALID)
				{
					ICE_ToStunServer(con);
					con->stunretry = curtime + 2*1000;
				}
				if (con->keepalive < curtime)
				{
					if (!ICE_SendSpam(con))
					{
						struct icecandidate_s *rc;
						struct icecandidate_s *best = NULL;
						for (rc = con->rc; rc; rc = rc->next)
						{
							if (rc->reachable && (!best || rc->info.priority > best->info.priority))
								best = rc;
						}
						if (best)
						{
							best->tried = ~best->reachable;
							con->chosenpeer = best->peer;
							ICE_SendSpam(con);
						}
						else
						{
							for (rc = con->rc; rc; rc = rc->next)
								rc->tried = 0;
						}
						con->retries++;
						if (con->retries > 32)
							con->retries = 32;
						con->keepalive = curtime + 200*(con->retries);	//RTO
					}
					else
						con->keepalive = curtime + 50*(con->retries+1);	//Ta
				}
			}
			else if (con->state == ICE_CONNECTED)
			{
				//keepalive
				//if (timeout)
				//	con->state = ICE_CONNECTING;
			}
			break;
		}
	}
}
void QDECL ICE_Close(struct icestate_s *con)
{
	struct icestate_s **link;

	ICE_Set(con, "state", STRINGIFY(ICE_INACTIVE));

	for (link = &icelist; *link; )
	{
		if (con == *link)
		{
			*link = con->next;
			ICE_Destroy(con);
			return;
		}
		else
			link = &(*link)->next;
	}
}
void QDECL ICE_CloseModule(void *module)
{
	struct icestate_s **link, *con;

	for (link = &icelist; *link; )
	{
		con = *link;
		if (con->module == module)
		{
			*link = con->next;
			ICE_Destroy(con);
		}
		else
			link = &(*link)->next;
	}
}

icefuncs_t iceapi =
{
	ICE_Create,
	ICE_Set,
	ICE_Get,
	ICE_GetLCandidateInfo,
	ICE_AddRCandidateInfo,
	ICE_Close,
	ICE_CloseModule,
	ICE_GetLCandidateSDP
};

qboolean ICE_WasStun(ftenet_connections_t *col)
{
#if defined(HAVE_CLIENT) && defined(VOICECHAT)
	if (col == cls.sockets)
	{
		if (NET_RTP_Parse())
			return true;		
	}
#endif

	if ((net_from.type == NA_IP || net_from.type == NA_IPV6) && net_message.cursize >= 20)
	{
		stunhdr_t *stun = (stunhdr_t*)net_message.data;
		int stunlen = BigShort(stun->msglen);
		if ((stun->msgtype == BigShort(0x0101) || stun->msgtype == BigShort(0x0111)) && net_message.cursize == stunlen + sizeof(*stun))
		{
			//binding reply (or error)
			netadr_t adr = net_from;
			char xor[16];
			short portxor;
			stunattr_t *attr = (stunattr_t*)(stun+1);
			int alen;
			while(stunlen)
			{
				stunlen -= sizeof(*attr);
				alen = (unsigned short)BigShort(attr->attrlen);
				if (alen > stunlen)
					return false;
				stunlen -= alen;
				switch(BigShort(attr->attrtype))
				{
				default:
					break;
				case 1:
				case 0x20:
					if (BigShort(attr->attrtype) == 0x20)
					{
						portxor = *(short*)&stun->magiccookie;
						memcpy(xor, &stun->magiccookie, sizeof(xor));
					}
					else
					{
						portxor = 0;
						memset(xor, 0, sizeof(xor));
					}
					if (alen == 8 && ((qbyte*)attr)[5] == 1)		//ipv4 MAPPED-ADDRESS
					{
						char str[256];
						adr.type = NA_IP;
						adr.port = (((short*)attr)[3]) ^ portxor;
						*(int*)adr.address.ip = *(int*)(&((qbyte*)attr)[8]) ^ *(int*)xor;
						NET_AdrToString(str, sizeof(str), &adr);
					}
					else if (alen == 20 && ((qbyte*)attr)[5] == 2)	//ipv6 MAPPED-ADDRESS
					{
						netadr_t adr;
						char str[256];
						adr.type = NA_IPV6;
						adr.port = (((short*)attr)[3]) ^ portxor;
						((int*)adr.address.ip6)[0] = ((int*)&((qbyte*)attr)[8])[0] ^ ((int*)xor)[0];
						((int*)adr.address.ip6)[1] = ((int*)&((qbyte*)attr)[8])[1] ^ ((int*)xor)[1];
						((int*)adr.address.ip6)[2] = ((int*)&((qbyte*)attr)[8])[2] ^ ((int*)xor)[2];
						((int*)adr.address.ip6)[3] = ((int*)&((qbyte*)attr)[8])[3] ^ ((int*)xor)[3];
						NET_AdrToString(str, sizeof(str), &adr);
					}

					{
						struct icestate_s *con;
						for (con = icelist; con; con = con->next)
						{
							char str[256];
							struct icecandidate_s *rc;
							if (con->mode != ICEM_ICE)
								continue;

							//check to see if this is a new peer-reflexive address, which happens when the peer is behind a nat.
							if (NET_CompareAdr(&net_from, &con->pubstunserver))
							{
								for (rc = con->lc; rc; rc = rc->next)
								{
									if (NET_CompareAdr(&adr, &rc->peer))
										break;
								}
								if (!rc)
								{
									struct icecandidate_s *rc;
									rc = Z_Malloc(sizeof(*rc));
									rc->next = con->lc;
									con->lc = rc;
									rc->peer = adr;
									NET_BaseAdrToString(rc->info.addr, sizeof(rc->info.addr), &adr);
									rc->info.port = ntohs(adr.port);
									rc->info.type = ICE_SRFLX;
									rc->info.component = 1;
									rc->dirty = true;
									rc->info.priority = 1;	//FIXME

									Con_DPrintf("ICE: Public address: %s\n", rc->info.addr);
								}
								con->stunretry = Sys_Milliseconds() + 60*1000;
							}
							else
							{
								for (rc = con->rc; rc; rc = rc->next)
								{
									if (NET_CompareAdr(&net_from, &rc->peer))
									{
										if (!(rc->reachable & (1u<<(net_from.connum-1))))
											Con_DPrintf("ICE: We can reach %s\n", NET_AdrToString(str, sizeof(str), &net_from));
										rc->reachable |= 1u<<(net_from.connum-1);

										if (NET_CompareAdr(&net_from, &con->chosenpeer) && (stun->transactid[2] & BigLong(0x80000000)))
										{
											if (con->state == ICE_CONNECTING)
												ICE_Set(con, "state", STRINGIFY(ICE_CONNECTED));
										}
									}
								}
							}
						}
					}
					break;
				case 9:
					{
						char msg[64];
						char sender[256];
						unsigned short len = BigShort(attr->attrlen)-4;
						if (len > sizeof(msg)-1)
							len = sizeof(msg)-1;
						memcpy(msg, &((qbyte*)attr)[8], len);
						msg[len] = 0;
						Con_DPrintf("%s: Stun error code %u : %s\n", NET_AdrToString(sender, sizeof(sender), &net_from), ((qbyte*)attr)[7], msg);
						if (((qbyte*)attr)[7] == 1)
						{
							//not authorised.
						}
						if (((qbyte*)attr)[7] == 87)
						{
							//role conflict.
						}
					}
					break;
				}
				alen = (alen+3)&~3;
				attr = (stunattr_t*)((char*)(attr+1) + alen);
			}
			return true;
		}
		else if (stun->msgtype == BigShort(0x0011) && net_message.cursize == stunlen + sizeof(*stun) && stun->magiccookie == BigLong(0x2112a442))
		{
			//binding indication. used as an rtp keepalive.
			return true;
		}
		else if (stun->msgtype == BigShort(0x0001) && net_message.cursize == stunlen + sizeof(*stun) && stun->magiccookie == BigLong(0x2112a442))
		{
			char username[256];
			char integrity[20];
			char *integritypos = NULL;
			int role = 0;
			struct icestate_s *con;
			unsigned int tiehigh = 0;
			unsigned int tielow = 0;
			qboolean usecandidate = false;
			int error = 0;
			unsigned int priority = 0;

			//binding request
			stunattr_t *attr = (stunattr_t*)(stun+1);
			int alen;
			*username = 0;
			while(stunlen)
			{
				alen = (unsigned short)BigShort(attr->attrlen);
				if (alen+sizeof(*attr) > stunlen)
					return false;
				switch((unsigned short)BigShort(attr->attrtype))
				{
				default:
					//unknown attributes < 0x8000 are 'mandatory to parse', and such packets must be dropped in their entirety.
					//other ones are okay.
					if (!((unsigned short)BigShort(attr->attrtype) & 0x8000))
						return false;
					break;
				case 0x6:
					//username
					if (alen < sizeof(username))
					{
						memcpy(username, attr+1, alen);
						username[alen] = 0;
//						Con_Printf("Stun username = \"%s\"\n", username);
					}
					break;
				case 0x8:
					//message integrity
					memcpy(integrity, attr+1, sizeof(integrity));
					integritypos = (char*)(attr+1);
					break;
				case 0x24:
					//priority
//					Con_Printf("priority = \"%i\"\n", priority);
					priority = BigLong(*(int*)(attr+1));
					break;
				case 0x25:
					//USE-CANDIDATE
					usecandidate = true;
					break;
				case 0x8028:
					//fingerprint
//					Con_Printf("fingerprint = \"%08x\"\n", BigLong(*(int*)(attr+1)));
					break;
				case 0x8029://ice controlled
				case 0x802A://ice controlling
					role = (unsigned short)BigShort(attr->attrtype);
					//ice controlled
					tiehigh = BigLong(((int*)(attr+1))[0]);
					tielow = BigLong(((int*)(attr+1))[1]);
					break;
				}
				alen = (alen+3)&~3;
				attr = (stunattr_t*)((char*)(attr+1) + alen);
				stunlen -= alen+sizeof(*attr);
			}

			//we need to know which connection its from in order to validate the integrity
			for (con = icelist; con; con = con->next)	
			{
				if (!strcmp(va("%s:%s", con->lufrag, con->rufrag), username))
					break;
			}
			if (!con)
			{
				Con_DPrintf("Received STUN request from unknown user \"%s\"\n", username);
			}
			else
			{
				if (integritypos)
				{
					char key[20];
					//the hmac is a bit weird. the header length includes the integrity attribute's length, but the checksum doesn't even consider the attribute header.
					stun->msglen = BigShort(integritypos+sizeof(integrity) - (char*)stun - sizeof(*stun));
					HMAC(SHA1_m, key, sizeof(key), (qbyte*)stun, integritypos-4 - (char*)stun, con->lpwd, strlen(con->lpwd));
					if (memcmp(key, integrity, sizeof(integrity)))
					{
						Con_DPrintf("Integrity is bad! needed %x got %x\n", *(int*)key, *(int*)integrity);
						return true;
					}
				}

				if (con->state != ICE_INACTIVE)
				{
					sizebuf_t buf;
					char data[512];
					int alen = 0, atype = 0, aofs = 0;
					int crc;
					struct icecandidate_s *rc;
					memset(&buf, 0, sizeof(buf));
					buf.maxsize = sizeof(data);
					buf.cursize = 0;
					buf.data = data;

					//check to see if this is a new peer-reflexive address, which happens when the peer is behind a nat.
					for (rc = con->rc; rc; rc = rc->next)
					{
						if (NET_CompareAdr(&net_from, &rc->peer))
							break;
					}
					if (!rc)
					{
						struct icecandidate_s *rc;
						rc = Z_Malloc(sizeof(*rc));
						rc->next = con->rc;
						con->rc = rc;
						rc->peer = net_from;
						NET_BaseAdrToString(rc->info.addr, sizeof(rc->info.addr), &net_from);
						rc->info.port = ntohs(net_from.port);
						rc->info.type = ICE_PRFLX;
						rc->dirty = true;
						rc->info.priority = priority;
					}

					//flip ice control role, if we're wrong.
					if (role && role != (con->controlled?0x802A:0x8029))
					{
						if (tiehigh == con->tiehigh && tielow == con->tielow)
						{
							Con_Printf("ICE: Evil loopback hack enabled\n");
							if (usecandidate)
							{
								con->chosenpeer = net_from;
								if (con->state == ICE_CONNECTING)
									ICE_Set(con, "state", STRINGIFY(ICE_CONNECTED));
							}
						}
						else
						{
							con->controlled = (tiehigh > con->tiehigh) || (tiehigh == con->tiehigh && tielow > con->tielow);
							Con_DPrintf("ICE: role conflict detected. We should be %s\n", con->controlled?"controlled":"controlling");
							error = 87;
						}
					}
					else if (usecandidate && con->controlled)
					{
						//in the controlled role, we're connected once we're told the pair to use (by the usecandidate flag).
						//note that this 'nominates' candidate pairs, from which the highest priority is chosen.
						//so we just pick select the highest.
						//this is problematic, however, as we don't actually know the real priority that the peer thinks we'll nominate it with.

						if (con->chosenpeer.type != NA_INVALID && !NET_CompareAdr(&net_from, &con->chosenpeer))
							Con_DPrintf("ICE: Duplicate use-candidate\n");
						con->chosenpeer = net_from;
						Con_DPrintf("ICE: use-candidate: %s\n", NET_AdrToString(data, sizeof(data), &net_from));

						if (con->state == ICE_CONNECTING)
							ICE_Set(con, "state", STRINGIFY(ICE_CONNECTED));
					}

					if (net_from.type == NA_IP)
					{
						alen = 4;
						atype = 1;
						aofs = 0;
					}
					else if (net_from.type == NA_IPV6 && 
								!*(int*)&net_from.address.ip6[0] && 
								!*(int*)&net_from.address.ip6[4] &&
								!*(short*)&net_from.address.ip6[8] &&
								*(short*)&net_from.address.ip6[10] == (short)0xffff)
					{	//just because we use an ipv6 address for ipv4 internally doesn't mean we should tell the peer that they're on ipv6...
						alen = 4;
						atype = 1;
						aofs = sizeof(net_from.address.ip6) - sizeof(net_from.address.ip);
					}
					else if (net_from.type == NA_IPV6)
					{
						alen = 16;
						atype = 2;
						aofs = 0;
					}

					MSG_WriteShort(&buf, BigShort(error?0x0111:0x0101));
					MSG_WriteShort(&buf, BigShort(0));	//fill in later
					MSG_WriteLong(&buf, stun->magiccookie);
					MSG_WriteLong(&buf, stun->transactid[0]);
					MSG_WriteLong(&buf, stun->transactid[1]);
					MSG_WriteLong(&buf, stun->transactid[2]);

					if (error == 87)
					{
						char *txt = "Role Conflict";
						MSG_WriteShort(&buf, BigShort(0x0009));
						MSG_WriteShort(&buf, BigShort(4 + strlen(txt)));
						MSG_WriteShort(&buf, 0);	//reserved
						MSG_WriteByte(&buf, 0);		//class
						MSG_WriteByte(&buf, error);	//code
						SZ_Write(&buf, txt, strlen(txt));	//readable
						while(buf.cursize&3)		//padding
							MSG_WriteChar(&buf, 0);
					}
					else if (1)
					{	//xor mapped
						MSG_WriteShort(&buf, BigShort(0x0020));
						MSG_WriteShort(&buf, BigShort(4+alen));
						MSG_WriteShort(&buf, BigShort(atype));
						MSG_WriteShort(&buf, net_from.port);
						SZ_Write(&buf, (char*)&net_from.address + aofs, alen);
					}
					else
					{	//non-xor mapped
						MSG_WriteShort(&buf, BigShort(0x0001));
						MSG_WriteShort(&buf, BigShort(4+alen));
						MSG_WriteShort(&buf, BigShort(atype));
						MSG_WriteShort(&buf, net_from.port);
						SZ_Write(&buf, (char*)&net_from.address + aofs, alen);
					}

					MSG_WriteShort(&buf, BigShort(0x6));	//USERNAME
					MSG_WriteShort(&buf, BigShort(strlen(username)));
					SZ_Write(&buf, username, strlen(username));
					while(buf.cursize&3)
						MSG_WriteChar(&buf, 0);

					//message integrity is a bit annoying
					data[2] = ((buf.cursize+4+sizeof(integrity)-20)>>8)&0xff;	//hashed header length is up to the end of the hmac attribute
					data[3] = ((buf.cursize+4+sizeof(integrity)-20)>>0)&0xff;
					//but the hash is to the start of the attribute's header
					HMAC(SHA1_m, integrity, sizeof(integrity), data, buf.cursize, con->lpwd, strlen(con->lpwd));
					MSG_WriteShort(&buf, BigShort(0x8));	//MESSAGE-INTEGRITY
					MSG_WriteShort(&buf, BigShort(sizeof(integrity)));	//sha1 key length
					SZ_Write(&buf, integrity, sizeof(integrity));	//integrity data

					data[2] = ((buf.cursize+8-20)>>8)&0xff;	//dummy length
					data[3] = ((buf.cursize+8-20)>>0)&0xff;
					crc = crc32(0, data, buf.cursize)^0x5354554e;
					MSG_WriteShort(&buf, BigShort(0x8028));	//FINGERPRINT
					MSG_WriteShort(&buf, BigShort(sizeof(crc)));
					MSG_WriteLong(&buf, BigLong(crc));

					data[2] = ((buf.cursize-20)>>8)&0xff;
					data[3] = ((buf.cursize-20)>>0)&0xff;
					NET_SendPacket(col, buf.cursize, data, &net_from);
				}
			}

			return true;
		}
	}
	return false;
}
#endif