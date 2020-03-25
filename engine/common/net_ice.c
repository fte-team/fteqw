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
#if defined(SUPPORT_ICE) || defined(MASTERONLY)
#include "zlib.h"
#endif
#ifdef SUPPORT_ICE
cvar_t net_ice_exchangeprivateips = CVARFD("net_ice_exchangeprivateips", "", CVAR_NOTFROMSERVER, "Boolean. When set to 0, hides private IP addresses from your peers. Only addresses determined from the other side of your router will be shared. Setting it to 0 may be desirable but it can cause connections to fail when your router does not support hairpinning, whereas 1 fixes that at the cost of exposing private IP addresses.");
/*
Interactive Connectivity Establishment (rfc 5245)
find out your peer's potential ports.
spam your peer with stun packets.
see what sticks.
the 'controller' assigns some final candidate pair to ensure that both peers send+receive from a single connection.
if no candidates are available, try using stun to find public nat addresses.

in fte, a 'pair' is actually in terms of each local socket and remote address. hopefully that won't cause too much weirdness.
(this does limit which interfaces we can send packets from (probably only an issue with VPNs, which should negate the value of ICE), and prevents us from being able to report reladdr in candidate offers (although these are merely diagnostic rather than useful)

stun test packets must contain all sorts of info. username+integrity+fingerprint for validation. priority+usecandidate+icecontrol(ing) to decree the priority of any new remote candidates, whether its finished, and just who decides whether its finished.
peers don't like it when those are missing.

host candidates - addresses that are directly known (but are probably unroutable private things)
server reflexive candidates - addresses that we found from some public stun server (useful for NATs that use a single public port for each unique private port)
peer reflexive candidates - addresses that our peer finds out about as we spam them
relayed candidates - some sort of socks5 or something proxy.


Note: Even after the ICE connection becomes active, you should continue to collect local candidates and transmit them to the peer out of band.
this allows the connection to pick a new route if some router dies (like a relay kicking us).
FIXME: the client currently disconnects from the broker. the server tracks players via ip rather than ICE.

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
static qboolean NET_RTP_Parse(void)
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



static struct icestate_s *QDECL ICE_Find(void *module, const char *conname)
{
	struct icestate_s *con;

	for (con = icelist; con; con = con->next)
	{
		if (con->module == module && !strcmp(con->conname, conname))
			return con;
	}
	return NULL;
}
static ftenet_connections_t *ICE_PickConnection(struct icestate_s *con)
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
static struct icestate_s *QDECL ICE_Create(void *module, const char *conname, const char *peername, enum icemode_e mode, enum iceproto_e proto)
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
		{
			NET_InitClient(false);
			collection = cls.sockets;
		}
		break;
#endif
#ifndef SERVERONLY
	case ICEP_QWCLIENT:
		collection = cls.sockets;
		if (!collection)
		{
			NET_InitClient(false);
			collection = cls.sockets;
		}
		break;
#endif
#ifndef CLIENTONLY
	case ICEP_QWSERVER:
		collection = svs.sockets;
		break;
#endif
	}
	if (!collection)
		return NULL;	//not initable or something

	if (!conname)
	{
		int rnd[2];
		Sys_RandomBytes((void*)rnd, sizeof(rnd));
		conname = va("fte%08x%08x", rnd[0], rnd[1]);
	}

	if (ICE_Find(module, conname))
		return NULL;	//don't allow dupes.
	
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
		const char *params[sizeof(addr)/sizeof(addr[0])];

		m = NET_EnumerateAddresses(collection, gcon, flags, addr, params, sizeof(addr)/sizeof(addr[0]));

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
		if (collection->conn[i] && (collection->conn[i]->addrtype[0]==NA_IP||collection->conn[i]->addrtype[0]==NA_IPV6))
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
		Con_DPrintf("ICE checking %s -> %s:%i\n", collection->conn[bestlocal]->name, bestpeer->info.addr, bestpeer->info.port);

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
		HMAC(&hash_sha1, integ, sizeof(integ), data, buf.cursize, con->rpwd, strlen(con->rpwd));
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

static void ICE_ToStunServer(struct icestate_s *con)
{
	sizebuf_t buf;
	char data[512];
	int crc;
	ftenet_connections_t *collection = ICE_PickConnection(con);
	if (!collection)
		return;
	if (!con->stunrnd[0])
		Sys_RandomBytes((char*)con->stunrnd, sizeof(con->stunrnd));

	Con_DPrintf("ICE: Checking public IP via %s\n", NET_AdrToString(data, sizeof(data), &con->pubstunserver));

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

static void QDECL ICE_AddRCandidateInfo(struct icestate_s *con, struct icecandinfo_s *n)
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
	else if (peer.type == NA_IPV6)
	{
		//ignore invalid addresses
		int i;
		for (i = 0; i < countof(peer.address.ip6); i++)
			if (peer.address.ip6[i])
				break;
		if (i == countof(peer.address.ip6))
			return; //all clear. in6_addr_any
	}

	if (*n->candidateid)
	{
		for (o = con->rc; o; o = o->next)
		{
			//not sure that updating candidates is particuarly useful tbh, but hey.
			if (!strcmp(o->info.candidateid, n->candidateid))
				break;
		}
	}
	else
	{
		for (o = con->rc; o; o = o->next)
		{
			//avoid dupes.
			if (!strcmp(o->info.addr, n->addr) && o->info.port == n->port)
				break;
		}
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

static qboolean QDECL ICE_Set(struct icestate_s *con, const char *prop, const char *value);
static void ICE_ParseSDPLine(struct icestate_s *con, const char *value)
{
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
			return;

		if(*value == ' ')value++;
		n.priority = strtoul(value, (char**)&value, 0);

		if(*value == ' ')value++;
		value = COM_ParseOut(value, n.addr, sizeof(n.addr));
		if (!value) return;

		if(*value == ' ')value++;
		n.port = strtoul(value, (char**)&value, 0);

		if(*value == ' ')value++;
		if (strncmp(value, "typ ", 4)) return;
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
			return;

		while (*value)
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
				while (*value && *value != ' ')
					value++;
				if(*value == ' ')value++;
				while (*value && *value != ' ')
					value++;
			}
		}
		ICE_AddRCandidateInfo(con, &n);
	}
}

static qboolean QDECL ICE_Set(struct icestate_s *con, const char *prop, const char *value)
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
			if (!cls.sockets)
				NET_InitClient(false);
#endif

		if (oldstate != con->state && con->state == ICE_CONNECTED)
		{
			char msg[256];
			if (con->chosenpeer.type == NA_INVALID)
			{
				con->state = ICE_FAILED;
				Con_Printf("ICE failed. peer not valid.\n");
			}
#ifndef SERVERONLY
			else if (con->proto == ICEP_QWCLIENT)
			{
				//FIXME: should make a proper connection type for this so we can switch to other candidates if one route goes down
//				Con_Printf("Try typing connect %s\n", NET_AdrToString(msg, sizeof(msg), &con->chosenpeer));
				Cbuf_AddText(va("\ncl_transfer \"%s\"\n", NET_AdrToString(msg, sizeof(msg), &con->chosenpeer)), RESTRICT_LOCAL);
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
				Con_Printf("%s connection established (peer %s).\n", con->proto == ICEP_VOICE?"voice":"data", NET_AdrToString(msg, sizeof(msg), &con->chosenpeer));
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
		char line[8192];
		const char *eol;
		for (; *value; value = eol)
		{
			eol = strchr(value, '\n');
			if (!eol)
				eol = value+strlen(value);

			if (eol-value < sizeof(line))
			{
				memcpy(line, value, eol-value);
				line[eol-value] = 0;
				ICE_ParseSDPLine(con, line);
			}

			if (eol)
				eol++;
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
	Q_snprintfz(value, valuelen, "a=candidate:%i %i %s %i %s %i typ %s",
			can->info.foundation,
			can->info.component,
			can->info.transport==0?"UDP":"ERROR",
			can->info.priority,
			can->info.addr,
			can->info.port,
			ctype
			);
	Q_strncatz(value, va(" generation %i", can->info.generation), valuelen);
	if (can->info.type != ICE_HOST)
	{
		if (*can->info.reladdr)
			Q_strncatz(value, va(" raddr %s", can->info.reladdr), valuelen);
		Q_strncatz(value, va(" rport %i", can->info.relport), valuelen);
	}

	return value;
}
static qboolean QDECL ICE_Get(struct icestate_s *con, const char *prop, char *value, size_t valuelen)
{
	if (!strcmp(prop, "sid"))
		Q_strncpyz(value, con->conname, valuelen);
	else if (!strcmp(prop, "state"))
	{
		switch(con->state)
		{
		case ICE_INACTIVE:
			Q_strncpyz(value, STRINGIFY(ICE_INACTIVE), valuelen);
			break;
		case ICE_FAILED:
			Q_strncpyz(value, STRINGIFY(ICE_FAILED), valuelen);
			break;
		case ICE_CONNECTING:
			Q_strncpyz(value, STRINGIFY(ICE_CONNECTING), valuelen);
			break;
		case ICE_CONNECTED:
			Q_strncpyz(value, STRINGIFY(ICE_CONNECTED), valuelen);
			break;
		}
	}
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
			const char *params[countof(addr)];

			if (!NET_EnumerateAddresses(ICE_PickConnection(con), gcon, flags, addr, params, countof(addr)))
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

//		if (net_enable_dtls.ival)
//			Q_strncatz(value, va("a=fingerprint:SHA-1 XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX:XX\n", con->fingerprint), valuelen);
		if (con->proto == ICEP_QWSERVER || con->proto == ICEP_QWCLIENT)
		{
#ifdef HAVE_DTLS
//			Q_strncatz(value, "m=application 9 DTLS/SCTP 5000\n", valuelen);
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

static void ICE_Debug(struct icestate_s *con)
{
	struct icecandidate_s *can;
	char buf[65536];
	ICE_Get(con, "state", buf, sizeof(buf));
	Con_Printf("ICE \"%s\" (%s):\n", con->friendlyname, buf);
	ICE_Get(con, "sdp", buf, sizeof(buf));
	Con_Printf(S_COLOR_YELLOW "%s\n", buf);

	Con_Printf("local:\n");
	for (can = con->lc; can; can = can->next)
	{
		ICE_CandidateToSDP(can, buf, sizeof(buf));
		if (can->dirty)
			Con_Printf(S_COLOR_RED" %s\n", buf);
		else
			Con_Printf(S_COLOR_YELLOW" %s\n", buf);
	}
	Con_Printf("remote:\n");
	for (can = con->rc; can; can = can->next)
	{
		ICE_CandidateToSDP(can, buf, sizeof(buf));
		if (can->dirty)
			Con_Printf(S_COLOR_RED" %s\n", buf);
		else
			Con_Printf(S_COLOR_YELLOW" %s\n", buf);
	}
}
static qboolean QDECL ICE_GetLCandidateSDP(struct icestate_s *con, char *out, size_t outsize)
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
static struct icecandinfo_s *QDECL ICE_GetLCandidateInfo(struct icestate_s *con)
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

	switch(adr->type)
	{
	case NA_IP:
		if (adr->address.ip[0] == 127)
			return;	//Addresses from a loopback interface MUST NOT be included in the candidate addresses
		break;
	case NA_IPV6:
		if (!memcmp(adr->address.ip6, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16))
			return;	//Addresses from a loopback interface MUST NOT be included in the candidate addresses
		if (adr->address.ip6[0] == 0xfe && (adr->address.ip6[1]&0xc0)==0x80)
			return; //fe80::/10 link local addresses should also not be reported.
		break;
	default:
		return;	//no, just no.
	}
	switch(adr->prot)
	{
	case NP_DTLS:
	case NP_DGRAM:
		break;
	default:
		return;	//don't report any tcp/etc connections...
	}

	//only consider private IP addresses when we're allowed to do so (ignore completely so we don't ignore them if they're srflx).
	if (!net_ice_exchangeprivateips.ival && type == ICE_HOST)
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
static void QDECL ICE_Close(struct icestate_s *con)
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
static void QDECL ICE_CloseModule(void *module)
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
#endif

#if defined(SUPPORT_ICE) || defined(MASTERONLY)
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
#ifdef SUPPORT_ICE
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

							if (NET_CompareAdr(&net_from, &con->pubstunserver))
							{	//check to see if this is a new server-reflexive address, which happens when the peer is behind a nat.
								for (rc = con->lc; rc; rc = rc->next)
								{
									if (NET_CompareAdr(&adr, &rc->peer))
										break;
								}
								if (!rc)
								{
									netadr_t reladdr;
									int relflags;
									const char *relpath;
									int rnd[2];
									struct icecandidate_s *src;	//server Reflexive Candidate
									src = Z_Malloc(sizeof(*src));
									src->next = con->lc;
									con->lc = src;
									src->peer = adr;
									NET_BaseAdrToString(src->info.addr, sizeof(src->info.addr), &adr);
									src->info.port = ntohs(adr.port);
									col->conn[net_from.connum-1]->GetLocalAddresses(col->conn[net_from.connum-1], &relflags, &reladdr, &relpath, 1);
									//FIXME: we don't really know which one... NET_BaseAdrToString(src->info.reladdr, sizeof(src->info.reladdr), &reladdr);
									src->info.relport = ntohs(reladdr.port);
									src->info.type = ICE_SRFLX;
									src->info.component = 1;
									src->dirty = true;
									src->info.priority = 1;	//FIXME

									Sys_RandomBytes((void*)rnd, sizeof(rnd));
									Q_strncpyz(src->info.candidateid, va("x%08x%08x", rnd[0], rnd[1]), sizeof(src->info.candidateid));

									Con_DPrintf("ICE: Public address: %s\n", NET_AdrToString(str, sizeof(str), &adr));
								}
								con->stunretry = Sys_Milliseconds() + 60*1000;
							}
							else
							{	//check to see if this is a new peer-reflexive address, which happens when the peer is behind a nat.
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
		else
#endif
			if (stun->msgtype == BigShort(0x0001) && net_message.cursize == stunlen + sizeof(*stun) && stun->magiccookie == BigLong(0x2112a442))
		{
			char username[256];
			char integrity[20];
#ifdef SUPPORT_ICE
			struct icestate_s *con;
			int role = 0;
			unsigned int tiehigh = 0;
			unsigned int tielow = 0;
			qboolean usecandidate = false;
			unsigned int priority = 0;
#endif
			char *integritypos = NULL;
			int error = 0;

			sizebuf_t buf;
			char data[512];
			int alen = 0, atype = 0, aofs = 0;
			int crc;

			//binding request
			stunattr_t *attr = (stunattr_t*)(stun+1);
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
#ifdef SUPPORT_ICE
				case 0x24:
					//priority
//					Con_Printf("priority = \"%i\"\n", priority);
					priority = BigLong(*(int*)(attr+1));
					break;
				case 0x25:
					//USE-CANDIDATE
					usecandidate = true;
					break;
#endif
				case 0x8028:
					//fingerprint
//					Con_Printf("fingerprint = \"%08x\"\n", BigLong(*(int*)(attr+1)));
					break;
#ifdef SUPPORT_ICE
				case 0x8029://ice controlled
				case 0x802A://ice controlling
					role = (unsigned short)BigShort(attr->attrtype);
					//ice controlled
					tiehigh = BigLong(((int*)(attr+1))[0]);
					tielow = BigLong(((int*)(attr+1))[1]);
					break;
#endif
				}
				alen = (alen+3)&~3;
				attr = (stunattr_t*)((char*)(attr+1) + alen);
				stunlen -= alen+sizeof(*attr);
			}

#ifdef SUPPORT_ICE
			if (*username || integritypos)
			{
				//we need to know which connection its from in order to validate the integrity
				for (con = icelist; con; con = con->next)
				{
					if (!strcmp(va("%s:%s", con->lufrag, con->rufrag), username))
						break;
				}
				if (!con)
				{
					Con_DPrintf("Received STUN request from unknown user \"%s\"\n", username);
					return true;
				}
				else if (con->state == ICE_INACTIVE)
					return true;	//bad timing
				else
				{
					struct icecandidate_s *rc;

					if (integritypos)
					{
						char key[20];
						//the hmac is a bit weird. the header length includes the integrity attribute's length, but the checksum doesn't even consider the attribute header.
						stun->msglen = BigShort(integritypos+sizeof(integrity) - (char*)stun - sizeof(*stun));
						HMAC(&hash_sha1, key, sizeof(key), (qbyte*)stun, integritypos-4 - (char*)stun, con->lpwd, strlen(con->lpwd));
						if (memcmp(key, integrity, sizeof(integrity)))
						{
							Con_DPrintf("Integrity is bad! needed %x got %x\n", *(int*)key, *(int*)integrity);
							return true;
						}
					}

					//check to see if this is a new peer-reflexive address, which happens when the peer is behind a nat.
					for (rc = con->rc; rc; rc = rc->next)
					{
						if (NET_CompareAdr(&net_from, &rc->peer))
							break;
					}
					if (!rc)
					{
						netadr_t reladdr;
						int relflags;
						const char *relpath;
						struct icecandidate_s *rc;
						rc = Z_Malloc(sizeof(*rc));
						rc->next = con->rc;
						con->rc = rc;

						rc->peer = net_from;
						NET_BaseAdrToString(rc->info.addr, sizeof(rc->info.addr), &net_from);
						rc->info.port = ntohs(net_from.port);
						col->conn[net_from.connum-1]->GetLocalAddresses(col->conn[net_from.connum-1], &relflags, &reladdr, &relpath, 1);
						//FIXME: we don't really know which one... NET_BaseAdrToString(rc->info.reladdr, sizeof(rc->info.reladdr), &reladdr);
						rc->info.relport = ntohs(reladdr.port);
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
				}
			}//otherwise its just an ip check
			else
				con = NULL;
#else
			(void)integritypos;
#endif

			memset(&buf, 0, sizeof(buf));
			buf.maxsize = sizeof(data);
			buf.cursize = 0;
			buf.data = data;

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
			else
			{
				alen = 0;
				atype = 0;
			}

//Con_DPrintf("STUN from %s\n", NET_AdrToString(data, sizeof(data), &net_from));

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
				netadr_t xored = net_from;
				int i;
				xored.port ^= *(short*)(data+4);
				for (i = 0; i < alen; i++)
					((qbyte*)&xored.address)[aofs+i] ^= ((qbyte*)data+4)[i];
				MSG_WriteShort(&buf, BigShort(0x0020));
				MSG_WriteShort(&buf, BigShort(4+alen));
				MSG_WriteShort(&buf, BigShort(atype));
				MSG_WriteShort(&buf, xored.port);
				SZ_Write(&buf, (char*)&xored.address + aofs, alen);
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

#ifdef SUPPORT_ICE
			if (con)
			{
				//message integrity is a bit annoying
				data[2] = ((buf.cursize+4+sizeof(integrity)-20)>>8)&0xff;	//hashed header length is up to the end of the hmac attribute
				data[3] = ((buf.cursize+4+sizeof(integrity)-20)>>0)&0xff;
				//but the hash is to the start of the attribute's header
				HMAC(&hash_sha1, integrity, sizeof(integrity), data, buf.cursize, con->lpwd, strlen(con->lpwd));
				MSG_WriteShort(&buf, BigShort(0x8));	//MESSAGE-INTEGRITY
				MSG_WriteShort(&buf, BigShort(sizeof(integrity)));	//sha1 key length
				SZ_Write(&buf, integrity, sizeof(integrity));	//integrity data
			}
#endif

			data[2] = ((buf.cursize+8-20)>>8)&0xff;	//dummy length
			data[3] = ((buf.cursize+8-20)>>0)&0xff;
			crc = crc32(0, data, buf.cursize)^0x5354554e;
			MSG_WriteShort(&buf, BigShort(0x8028));	//FINGERPRINT
			MSG_WriteShort(&buf, BigShort(sizeof(crc)));
			MSG_WriteLong(&buf, BigLong(crc));

			data[2] = ((buf.cursize-20)>>8)&0xff;
			data[3] = ((buf.cursize-20)>>0)&0xff;
			NET_SendPacket(col, buf.cursize, data, &net_from);

			return true;
		}
	}
	return false;
}
#endif


#ifdef SUPPORT_ICE
//this is the clientside part of our custom accountless broker protocol
//basically just keeps the broker processing, but doesn't send/receive actual game packets.
//inbound messages can change ice connection states.
//clients only handle one connection. servers need to handle multiple
typedef struct {
	ftenet_generic_connection_t generic;

	//config state
	char brokername[64];	//dns name
	netadr_t brokeradr;		//actual ip
	char gamename[64];		//what we're trying to register as/for with the broker

	//broker connection state
	vfsfile_t *broker;
	qboolean handshaking;
	double nextping;		//send heartbeats every now and then
	double heartbeat;
	double timeout;			//detect if the broker goes dead, so we can reconnect reliably (instead of living for two hours without anyone able to connect).
	qbyte in[8192];
	size_t insize;
	qbyte out[8192];
	size_t outsize;
	int error;	//outgoing data is corrupt. kill it.

	//client state...
	struct icestate_s *ice;
	int serverid;

	//server state...
	struct
	{
		struct icestate_s *ice;
	} *clients;
	size_t numclients;
} ftenet_ice_connection_t;
static void FTENET_ICE_Close(ftenet_generic_connection_t *gcon)
{
	ftenet_ice_connection_t *b = (void*)gcon;
	int cl;
	if (b->broker)
		VFS_CLOSE(b->broker);

	for (cl = 0; cl < b->numclients; cl++)
		if (b->clients[cl].ice)
			iceapi.ICE_Close(b->clients[cl].ice);
	Z_Free(b->clients);
	if (b->ice)
		iceapi.ICE_Close(b->ice);

	Z_Free(b);
}

static void FTENET_ICE_Flush(ftenet_ice_connection_t *b)
{
	int r;
	if (!b->outsize || b->error || !b->broker)
		return;
	r = VFS_WRITE(b->broker, b->out, b->outsize);
	if (r > 0)
	{
		b->outsize -= r;
		memmove(b->out, b->out+r, b->outsize);
	}
	if (r < 0)
		b->error = true;
}
static neterr_t FTENET_ICE_SendPacket(ftenet_generic_connection_t *gcon, int length, const void *data, netadr_t *to)
{
	ftenet_ice_connection_t *b = (void*)gcon;
	if (to->prot != NP_RTC_TCP && to->prot != NP_RTC_TLS)
		return NETERR_NOROUTE;
	if (!NET_CompareAdr(to, &b->brokeradr))
		return NETERR_NOROUTE;	//its using some other broker, don't bother trying to handle it here.
	if (b->error)
		return NETERR_DISCONNECTED;
	return NETERR_CLOGGED;	//we'll switch to a connect localcmd when the connection completes, so we don't really need to send any packets when they're using ICE. Just make sure the client doesn't give up.
}

static void FTENET_ICE_SplurgeRaw(ftenet_ice_connection_t *b, const qbyte *data, size_t len)
{
//0: dropclient (cl=-1 drops entire connection)
	if (b->outsize+len > sizeof(b->out))
		b->error = true;
	else
	{
		memcpy(b->out+b->outsize, data, len);
		b->outsize += len;
	}
}
static void FTENET_ICE_SplurgeWS(ftenet_ice_connection_t *b, enum websocketpackettype_e pkttype, const qbyte *data1, size_t len1, const qbyte *data2, size_t len2)
{
	size_t tlen = len1+len2;
	qbyte header[8];
	header[0] = 0x80|pkttype;
	if (tlen >= 126)
	{
		header[1] = 126;
		header[2] = tlen>>8;	//bigendian
		header[3] = tlen&0xff;
		FTENET_ICE_SplurgeRaw(b, header, 4);
	}
	else
	{	//small data
		header[1] = tlen;
		FTENET_ICE_SplurgeRaw(b, header, 2);
	}
	FTENET_ICE_SplurgeRaw(b, data1, len1);
	FTENET_ICE_SplurgeRaw(b, data2, len2);
}
static void FTENET_ICE_SplurgeCmd(ftenet_ice_connection_t *b, int icemsg, int cl, const char *data)
{
	qbyte msg[3] = {icemsg, cl&0xff, (cl>>8)&0xff};	//little endian...
	FTENET_ICE_SplurgeWS(b, WS_PACKETTYPE_BINARYFRAME, msg, sizeof(msg), data, strlen(data));
}
static void FTENET_ICE_Heartbeat(ftenet_ice_connection_t *b)
{
	b->heartbeat = realtime+30;
#ifdef HAVE_SERVER
	if (b->generic.islisten)
	{
		extern cvar_t maxclients;
		char info[2048];
		int i;
		client_t *cl;
		int numclients = 0;
		for (i=0 ; i<svs.allocated_client_slots ; i++)
		{
			cl = &svs.clients[i];
			if ((cl->state == cs_connected || cl->state == cs_spawned || cl->name[0]) && !cl->spectator)
				numclients++;
		}

		*info = 0;
		Info_SetValueForKey(info, "protocol", com_protocolversion.string, sizeof(info));
		Info_SetValueForKey(info, "maxclients", maxclients.string, sizeof(info));
		Info_SetValueForKey(info, "clients", va("%i", numclients), sizeof(info));
		Info_SetValueForKey(info, "hostname", hostname.string, sizeof(info));
		Info_SetValueForKey(info, "modname", FS_GetGamedir(true), sizeof(info));
		Info_SetValueForKey(info, "mapname", InfoBuf_ValueForKey(&svs.info, "map"), sizeof(info));
		Info_SetValueForKey(info, "needpass", InfoBuf_ValueForKey(&svs.info, "needpass"), sizeof(info));

		FTENET_ICE_SplurgeCmd(b, ICEMSG_SERVERINFO, -1, info);
	}
#endif
}
static void FTENET_ICE_Establish(ftenet_ice_connection_t *b, int cl, struct icestate_s **ret)
{	//sends offer
	char buf[8192];
	struct icestate_s *ice;
	if (*ret)
		iceapi.ICE_Close(*ret);
	ice = *ret = iceapi.ICE_Create(b, NULL, "", ICEM_ICE, b->generic.islisten?ICEP_QWSERVER:ICEP_QWCLIENT);
	if (!*ret)
		return;	//some kind of error?!?
	iceapi.ICE_Set(ice, "controller", b->generic.islisten?"0":"1");

	Q_snprintfz(buf, sizeof(buf), "%i", BigShort(b->brokeradr.port));
	iceapi.ICE_Set(ice, "stunport", buf);
	iceapi.ICE_Set(ice, "stunip", b->brokername);

	//okay, now send the sdp to our peer.
	if (iceapi.ICE_Get(ice, "sdp", buf, sizeof(buf)))
		FTENET_ICE_SplurgeCmd(b, ICEMSG_OFFER, cl, buf);
}
static void FTENET_ICE_Refresh(ftenet_ice_connection_t *b, int cl, struct icestate_s *ice)
{	//sends offer
	char buf[8192];
	while (ice && iceapi.ICE_GetLCandidateSDP(ice, buf, sizeof(buf)))
		FTENET_ICE_SplurgeCmd(b, ICEMSG_CANDIDATE, cl, buf);
}
static qboolean FTENET_ICE_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_ice_connection_t *b = (void*)gcon;
	int ctrl, len, cmd, cl, ofs;
	char *data, n;

#ifdef HAVE_CLIENT
	//there's no point in hanging on to the ICE connection if we're not going to do anything with it now that we're connected.
	//(shutting off the tcp connection will notify the server to shut down too)
	if (!b->generic.islisten && !CL_TryingToConnect())
		b->error = true;
	else
#endif
		 if (!b->broker)
	{
		const char *s;
		if (b->timeout > realtime)
			return false;
		b->generic.thesocket = TCP_OpenStream(&b->brokeradr);	//save this for select.
		b->broker = FS_WrapTCPSocket(b->generic.thesocket, true, b->brokername);

#ifdef HAVE_SSL
		//convert to tls...
		if (b->brokeradr.prot == NP_TLS || b->brokeradr.prot == NP_RTC_TLS)
			b->broker = FS_OpenSSL(b->brokername, b->broker, false);
#endif

		if (!b->broker)
		{
			b->timeout = realtime + 30;
			Con_Printf("rtc broker connection to %s failed (retry: 30 secs)\n", b->brokername);
			return false;
		}

		b->insize = b->outsize = 0;

		COM_Parse(com_protocolname.string);

		b->handshaking = true;
		s = va("GET /%s/%s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: Upgrade\r\n"
			"Upgrade: websocket\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"Sec-WebSocket-Protocol: %s\r\n"
			"\r\n", com_token, b->gamename, b->brokername, b->generic.islisten?"rtc_host":"rtc_client");
		FTENET_ICE_SplurgeRaw(b, s, strlen(s));
		b->heartbeat = realtime;
		b->nextping = realtime + 100;
		b->timeout = realtime + 270;
	}
	if (b->error)
	{
handleerror:
		b->generic.thesocket = INVALID_SOCKET;
		if (b->broker)
			VFS_CLOSE(b->broker);
		b->broker = NULL;

		for (cl = 0; cl < b->numclients; cl++)
		{
			if (b->clients[cl].ice)
				iceapi.ICE_Close(b->clients[cl].ice);
			b->clients[cl].ice = NULL;
		}
		if (b->ice)
			iceapi.ICE_Close(b->ice);
		b->ice = NULL;
		if (b->error != 1 || !b->generic.islisten)
			return false;	//permanant error...
		b->error = false;
		b->insize = b->outsize = 0;
		b->timeout = realtime + 30;
		return false;
	}

	//keep checking for new candidate info.
	if (b->ice)
		FTENET_ICE_Refresh(b, b->serverid, b->ice);
	for (cl = 0; cl < b->numclients; cl++)
		if (b->clients[cl].ice)
			FTENET_ICE_Refresh(b, cl, b->clients[cl].ice);
	if (realtime >= b->heartbeat)
		FTENET_ICE_Heartbeat(b);

	len = VFS_READ(b->broker, b->in+b->insize, sizeof(b->in)-1-b->insize);
	if (!len)
	{
		FTENET_ICE_Flush(b);

		if (realtime > b->nextping)
		{	//nothing happening... make sure the connection isn't dead...
			FTENET_ICE_SplurgeWS(b, WS_PACKETTYPE_PING, NULL, 0, NULL, 0);
			b->nextping = realtime + 100;
		}
		return false;	//nothing new
	}
	if (len < 0)
	{
		if (!b->error)
			Con_Printf("rtc broker connection to %s failed (retry: 30 secs)\n", b->brokername);
		b->error = true;
		goto handleerror;
	}
	b->insize += len;
	b->in[b->insize] = 0;
	ofs = 0;

	b->nextping = realtime + 100;
	b->timeout = max(b->timeout, realtime + 270);

	if (b->handshaking)
	{	//we're still waiting for an http 101 code. websocket data starts straight after.
		char *end = strstr(b->in, "\r\n\r\n");
		if (!end)
			return false;	//not available yet...
		if (strncmp(b->in, "HTTP/1.1 101 ", 13))
		{
			b->error = ~0;
			return false;
		}
		end+=4;
		b->handshaking = false; //done...

		ofs = (qbyte*)end-b->in;
	}

	while (b->insize >= ofs+2)
	{
		ctrl = b->in[ofs+0];
		len = b->in[ofs+1];
		ofs+=2;
		if (len > 126)
		{//unsupported
			b->error = 1;
			break;
		}
		else if (len == 126)
		{
			if (b->insize <= 4)
				break;
			len = (b->in[ofs+0]<<8)|(b->in[ofs+1]);
			ofs+=2;
		}
		if (b->insize < ofs+len)
			break;
		n = b->in[ofs+len];
		b->in[ofs+len] = 0;

		switch(ctrl & 0xf)
		{
		case WS_PACKETTYPE_PING:
			FTENET_ICE_SplurgeWS(b, WS_PACKETTYPE_PONG, NULL, 0, b->in+ofs, len);
			FTENET_ICE_Flush(b);
			break;
		case WS_PACKETTYPE_CLOSE:
			b->error = true;
			break;
		default:
			break;
		case WS_PACKETTYPE_BINARYFRAME:
			cmd = b->in[ofs];
			cl = (short)(b->in[ofs+1] | (b->in[ofs+2]<<8));
			data = b->in+ofs+3;

			switch(cmd)
			{
			case ICEMSG_PEERDROP:	//connection closing...
				if (cl == -1)
				{
					b->error = true;
//					Con_Printf("Broker closed connection: %s\n", data);
				}
				else if (cl >= 0 && cl < b->numclients)
				{
					if (b->clients[cl].ice)
						iceapi.ICE_Close(b->clients[cl].ice);
					b->clients[cl].ice = NULL;
//					Con_Printf("Broker closing connection: %s\n", data);
				}
				break;
			case ICEMSG_GREETING:	//reports the trailing url we're 'listening' on. anyone else using that url will connect to us.
				data = strchr(data, '/');
				if (data++)
					Q_strncpyz(b->gamename, data, sizeof(b->gamename));
				Con_Printf("Publicly listening on /%s\n", b->gamename);
				break;
			case ICEMSG_NEWPEER:	//connection established with a new peer
				//note that the server ought to wait for an offer from the client before replying with any ice state, but it doesn't really matter for our use-case.
				if (b->generic.islisten)
				{
//					Con_DPrintf("Client connecting: %s\n", data);
					if (cl < 1024 && cl >= b->numclients)
					{	//looks like a new one... but don't waste memory
						Z_ReallocElements((void**)&b->clients, &b->numclients, cl+1, sizeof(b->clients[0]));
					}
					if (cl >= 0 && cl < b->numclients)
						FTENET_ICE_Establish(b, cl, &b->clients[cl].ice);
				}
				else
				{
//					Con_DPrintf("Server found: %s\n", data);
					FTENET_ICE_Establish(b, cl, &b->ice);
					b->serverid = cl;
				}
				break;
			case ICEMSG_OFFER:	//we received an offer from a client
				if (b->generic.islisten)
				{
//					Con_Printf("Client offered: %s\n", data);
					if (cl >= 0 && cl < b->numclients && b->clients[cl].ice)
					{
						iceapi.ICE_Set(b->clients[cl].ice, "sdp", data);
						iceapi.ICE_Set(b->clients[cl].ice, "state", STRINGIFY(ICE_CONNECTING));
					}
				}
				else
				{
//					Con_Printf("Server offered: %s\n", data);
					if (b->ice)
					{
						iceapi.ICE_Set(b->ice, "sdp", data);
						iceapi.ICE_Set(b->ice, "state", STRINGIFY(ICE_CONNECTING));
					}
				}
				break;
			case ICEMSG_CANDIDATE:
//				Con_Printf("Candidate update: %s\n", data);
				if (b->generic.islisten)
				{
					if (cl >= 0 && cl < b->numclients && b->clients[cl].ice)
						iceapi.ICE_Set(b->clients[cl].ice, "sdp", data);
				}
				else
				{
					if (b->ice)
						iceapi.ICE_Set(b->ice, "sdp", data);
				}
				break;
			}
			break;
		}

		ofs+=len;
		b->in[ofs] = n;

	}

	if (ofs)
	{	//and eat the newly parsed data...
		b->insize -= ofs;
		memmove(b->in, b->in+ofs, b->insize);
	}

	FTENET_ICE_Flush(b);
	return false;
}
static void FTENET_ICE_PrintStatus(ftenet_generic_connection_t *gcon)
{
	ftenet_ice_connection_t *b = (void*)gcon;
	size_t c;

	if (b->ice)
		ICE_Debug(b->ice);
	if (b->numclients)
	{
		Con_Printf("%u clients\n", (unsigned)b->numclients);
		for (c = 0; c < b->numclients; c++)
			if (b->clients[c].ice)
				ICE_Debug(b->clients[c].ice);
	}
}
static int FTENET_ICE_GetLocalAddresses(struct ftenet_generic_connection_s *gcon, unsigned int *adrflags, netadr_t *addresses, const char **adrparms, int maxaddresses)
{
	ftenet_ice_connection_t *b = (void*)gcon;
	if (maxaddresses < 1)
		return 0;
	*addresses = b->brokeradr;
	*adrflags = 0;
	*adrparms = b->gamename;
	return 1;
}

static qboolean FTENET_ICE_ChangeLocalAddress(struct ftenet_generic_connection_s *gcon, const char *address, netadr_t *newadr)
{
	ftenet_ice_connection_t *b = (void*)gcon;
	netadr_t adr;
	const char *path;

	if (!NET_StringToAdr2(address, PORT_ICEBROKER, &adr, 1, &path))
		return true;	//err... something failed? don't break what works!

	if (!NET_CompareAdr(&adr, &b->brokeradr))
		return false;	//the broker changed! zomg! just kill it all!

	if (path && *path++=='/')
	{
		if (*path && strcmp(path, b->gamename))
			return false;	//it changed! and we care! break everything!
	}
	return true;
}

ftenet_generic_connection_t *FTENET_ICE_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr)
{
	ftenet_ice_connection_t *newcon;
	const char *path;
	char *c;

	if (!NET_StringToAdr2(address, PORT_ICEBROKER, &adr, 1, &path))
		return NULL;
/*	if (adr.prot == NP_ICES)
		adr.prot = NP_TLS;
	else if (adr.prot == NP_ICE)
		adr.prot = NP_STREAM;
*/
	newcon = Z_Malloc(sizeof(*newcon));

	if (!strncmp(address, "ice://", 6)||!strncmp(address, "rtc://", 6))
		address+=6;
	else if (!strncmp(address, "ices://", 7)||!strncmp(address, "rtcs://", 7))
		address+=7;
	if (address == path && *path=='/' && fs_manifest->rtcbroker)
	{
		if (!strncmp(fs_manifest->rtcbroker, "tls://", 6) || !strncmp(fs_manifest->rtcbroker, "tcp://", 6))
			Q_strncpyz(newcon->brokername, fs_manifest->rtcbroker+6, sizeof(newcon->brokername));	//name is for prints only.
		else
			Q_strncpyz(newcon->brokername, fs_manifest->rtcbroker, sizeof(newcon->brokername));	//name is for prints only.
		Q_strncpyz(newcon->gamename, path+1, sizeof(newcon->gamename));	//so we know what to tell the broker.
	}
	else
	{
		Q_strncpyz(newcon->brokername, address, sizeof(newcon->brokername));	//name is for prints only.
		if (path && *path == '/' && path-address < sizeof(newcon->brokername))
		{
			newcon->brokername[path-address] = 0;
			Q_strncpyz(newcon->gamename, path+1, sizeof(newcon->gamename));	//so we know what to tell the broker.
		}
		else
			*newcon->gamename = 0;
	}
	c = strchr(newcon->brokername, ':');
	if (c) *c = 0;

	newcon->brokeradr = adr;
	newcon->broker = NULL;
	newcon->timeout = realtime;
	newcon->heartbeat = realtime;
	newcon->nextping = realtime;
	newcon->generic.thesocket = INVALID_SOCKET;

	newcon->generic.addrtype[0] = NA_INVALID;
	newcon->generic.addrtype[1] = NA_INVALID;

	newcon->generic.GetPacket = FTENET_ICE_GetPacket;
	newcon->generic.SendPacket = FTENET_ICE_SendPacket;
	newcon->generic.Close = FTENET_ICE_Close;
	newcon->generic.PrintStatus = FTENET_ICE_PrintStatus;
	newcon->generic.GetLocalAddresses = FTENET_ICE_GetLocalAddresses;
	newcon->generic.ChangeLocalAddress = FTENET_ICE_ChangeLocalAddress;

	newcon->generic.islisten = col->islisten;

	return &newcon->generic;
}
#endif