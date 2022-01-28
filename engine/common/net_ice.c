#include "quakedef.h"
#include "netinc.h"

typedef struct
{
	unsigned short msgtype;
	unsigned short msglen;
	unsigned int magiccookie;
	unsigned int transactid[3];
} stunhdr_t;
//class
#define STUN_REQUEST	0x0000
#define STUN_REPLY		0x0100
#define STUN_ERROR		0x0110
#define STUN_INDICATION	0x0010
//
#define STUN_BINDING	0x0001

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

	netadr_t qadr;			//address reported to the rest of the engine (packets from our peer get remapped to this)
	netadr_t chosenpeer;	//address we're sending our data to.

	void *dtlsstate;
	struct sctp_s *sctp;	//ffs! extra processing needed.

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

	qboolean blockcandidates;		//don't send candidates yet.
	const dtlsfuncs_t *dtlsfuncs;
	qboolean dtlspassive;	//true=server, false=client (separate from ice controller and whether we're hosting. yay...)
	dtlscred_t cred;	//credentials info for dtls (both peer and local info)
	quint16_t mysctpport;
	quint16_t peersctpport;

	ftenet_connections_t *connections;	//used only for PRIVATE sockets.

	struct icecodecslot_s
	{
		//FIXME: we should probably include decode state in here somehow so multiple connections don't clobber each other.
		int id;
		char *name;
	} codecslot[34];		//96-127. don't really need to care about other ones.
};

typedef struct sctp_s
{
	quint16_t myport, peerport;
	qboolean peerhasfwdtsn;
	double nextreinit;
	void *cookie;	//sent part of the handshake (might need resending).
	size_t cookiesize;
	struct
	{
		quint32_t verifycode;
		qboolean writable;
		quint32_t tsn;	//Transmit Sequence Number

		quint32_t ctsn;	//acked tsn
		quint32_t losttsn; //total gap size...
	} o;
	struct
	{
		quint32_t verifycode;
		int ackneeded;
		quint32_t ctsn;
		quint32_t htsn;	//so we don't have to walk so many packets to count gaps.
#define SCTP_RCVSIZE 2048	//cannot be bigger than 65536
		qbyte received[SCTP_RCVSIZE/8];

		struct
		{
			quint32_t	firsttns; //so we only ack fragments that were complete.
			quint32_t	tsn; //if a continuation doesn't match, we drop it.
			quint32_t	ppid;
			quint16_t	sid;
			quint16_t	seq;
			size_t		size;
			qboolean	toobig;
			qbyte		buf[65536];
		} r;
	} i;
	unsigned short qstreamid;	//in network endian.
} sctp_t;
static neterr_t SCTP_Transmit(sctp_t *sctp, struct icestate_s *peer, const void *data, size_t length);

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
static neterr_t ICE_Transmit(void *cbctx, const qbyte *data, size_t datasize)
{
	struct icestate_s *ice = cbctx;
	return NET_SendPacket(ICE_PickConnection(ice), datasize, data, &ice->chosenpeer);
}

static struct icestate_s *QDECL ICE_Create(void *module, const char *conname, const char *peername, enum icemode_e mode, enum iceproto_e proto)
{
	ftenet_connections_t *collection;
	struct icestate_s *con;
	static unsigned int icenum;

	//only allow modes that we actually support.
	if (mode != ICEM_RAW && mode != ICEM_ICE && mode != ICEM_WEBRTC)
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
	con->friendlyname = peername?Z_StrDup(peername):Z_StrDupf("%i", icenum++);
	con->proto = proto;
	con->rpwd = Z_StrDup("");
	con->rufrag = Z_StrDup("");
	Sys_RandomBytes((void*)&con->originid, sizeof(con->originid));
	con->originversion = 1;
	Q_strncpyz(con->originaddress, "127.0.0.1", sizeof(con->originaddress));

	con->mode = mode;
	con->blockcandidates = true;	//until offers/answers are sent.
	con->dtlspassive = (proto == ICEP_QWSERVER);	//note: may change later.

	if (mode == ICEM_WEBRTC)
	{	//dtls+sctp is a mandatory part of our connection, sadly.
		if (!con->dtlsfuncs)
		{
			if (con->dtlspassive)
				con->dtlsfuncs = DTLS_InitServer();
			else
				con->dtlsfuncs = DTLS_InitClient();	//credentials are a bit different, though fingerprints make it somewhat irrelevant.
		}
		if (con->dtlsfuncs && con->dtlsfuncs->GenTempCertificate && !con->cred.local.certsize)
			con->dtlsfuncs->GenTempCertificate(NULL, &con->cred.local);

		con->mysctpport = 27500;
	}

	con->qadr.type = NA_ICE;
	con->qadr.prot = NP_DGRAM;
	con->qadr.port = con->mysctpport;
	Q_strncpyz(con->qadr.address.icename, con->friendlyname, sizeof(con->qadr.address.icename));

	con->next = icelist;
	icelist = con;

	{
		int rnd[1];	//'must have at least 24 bits randomness'
		Sys_RandomBytes((void*)rnd, sizeof(rnd));
		con->lufrag = Z_StrDupf("%08x", rnd[0]);
	}
	{
		int rnd[4];	//'must have at least 128 bits randomness'
		Sys_RandomBytes((void*)rnd, sizeof(rnd));
		con->lpwd = Z_StrDupf("%08x%08x%08x%08x", rnd[0], rnd[1], rnd[2], rnd[3]);
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
		CalcHMAC(&hash_sha1, integ, sizeof(integ), data, buf.cursize, con->rpwd, strlen(con->rpwd));
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

int ParsePartialIP(const char *s, netadr_t *a);
static void QDECL ICE_AddRCandidateInfo(struct icestate_s *con, struct icecandinfo_s *n)
{
	struct icecandidate_s *o;
	qboolean isnew;
	netadr_t peer;
	int peerbits;
	//I don't give a damn about rtpc.
	if (n->component != 1)
		return;
	if (n->transport != 0)
		return;	//only UDP is supported.

	//don't use the regular string->addr, because browsers seem to shove internal gibberish names in there that waste time to resolve. hostnames don't really make sense here anyway.
	peerbits = ParsePartialIP(n->addr, &peer);
	peer.prot = NP_DGRAM;
	peer.port = n->port;
	if (peer.type == NA_IP && peerbits == 32)
	{
		//ignore invalid addresses
		if (!peer.address.ip[0] && !peer.address.ip[1] && !peer.address.ip[2] && !peer.address.ip[3])
			return;
	}
	else if (peer.type == NA_IPV6 && peerbits == 128)
	{
		//ignore invalid addresses
		int i;
		for (i = 0; i < countof(peer.address.ip6); i++)
			if (peer.address.ip6[i])
				break;
		if (i == countof(peer.address.ip6))
			return; //all clear. in6_addr_any
	}
	else
		return;	//bad address type, or partial.

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
	else if (!strncmp(value, "a=setup:", 8))
	{	//this is their state, so we want the opposite.
		if (!strncmp(value+8, "passive", 7))
			con->dtlspassive = false;
		else if (!strncmp(value+8, "active", 6))
			con->dtlspassive = true;
	}
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
	else if (!strncmp(value, "a=fingerprint:", 14))
	{
		char name[64];
		value = COM_ParseOut(value+14, name, sizeof(name));
		if (!strcasecmp(name, "sha-1"))
			con->cred.peer.hash = &hash_sha1;
		else if (!strcasecmp(name, "sha-2224"))
			con->cred.peer.hash = &hash_sha224;
		else if (!strcasecmp(name, "sha-256"))
			con->cred.peer.hash = &hash_sha256;
		else if (!strcasecmp(name, "sha-384"))
			con->cred.peer.hash = &hash_sha384;
		else if (!strcasecmp(name, "sha-512"))
			con->cred.peer.hash = &hash_sha512;
		else
			con->cred.peer.hash = NULL; //hash not recognised
		if (con->cred.peer.hash)
		{
			int b, o, v;
			while (*value == ' ')
				value++;
			for (b = 0; b < con->cred.peer.hash->digestsize; )
			{
				v = *value;
				if      (v >= '0' && v <= '9')
					o = (v-'0');
				else if (v >= 'A' && v <= 'F')
					o = (v-'A'+10);
				else if (v >= 'a' && v <= 'f')
					o = (v-'a'+10);
				else
					break;
				o <<= 4;
				v = *++value;
				if      (v >= '0' && v <= '9')
					o |= (v-'0');
				else if (v >= 'A' && v <= 'F')
					o |= (v-'A'+10);
				else if (v >= 'a' && v <= 'f')
					o |= (v-'a'+10);
				else
					break;
				con->cred.peer.digest[b++] = o;
				v = *++value;
				if (v != ':')
					break;
				value++;
			}
			if (b != con->cred.peer.hash->digestsize)
				con->cred.peer.hash = NULL; //bad!
		}
	}
	else if (!strncmp(value, "a=sctp-port:", 12))
		con->peersctpport = atoi(value+12);
	else if (!strncmp(value, "a=candidate:", 12))
	{
		struct icecandinfo_s n;
		memset(&n, 0, sizeof(n));

		value += 12;
		n.foundation = strtoul(value, (char**)&value, 0);

		if(*value == ' ')value++;
		n.component = strtoul(value, (char**)&value, 0);

		if(*value == ' ')value++;
		if (!strncasecmp(value, "UDP ", 4))
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

void CL_Transfer(netadr_t *adr);
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
		con->timeout = Sys_Milliseconds() + 30;

		con->retries = 0;

#ifndef SERVERONLY
		if (con->state == ICE_CONNECTING && (con->proto == ICEP_QWCLIENT || con->proto == ICEP_VOICE))
			if (!cls.sockets)
				NET_InitClient(false);
#endif

		if (con->state >= ICE_CONNECTING)
		{
			if (con->mode == ICEM_WEBRTC)
			{
				if (!con->dtlsstate && con->dtlsfuncs)
				{
					con->dtlsstate = con->dtlsfuncs->CreateContext(&con->cred, con, ICE_Transmit, con->dtlspassive);
				}
				if (!con->sctp)
				{
					con->sctp = Z_Malloc(sizeof(*con->sctp));
					con->sctp->myport = htons(con->mysctpport);
					con->sctp->peerport = htons(con->peersctpport);
					Sys_RandomBytes((void*)&con->sctp->o.verifycode, sizeof(con->sctp->o.verifycode));
					Sys_RandomBytes((void*)&con->sctp->i.verifycode, sizeof(con->sctp->i.verifycode));
				}
			}
		}

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
				CL_Transfer(&con->qadr);	//okay, the client should be using this ice connection now.
#endif
#ifndef CLIENTONLY
			else if (con->proto == ICEP_QWSERVER)
			{
				net_from = con->chosenpeer;
				SVC_GetChallenge(false);
			}
#endif
			if (con->state == ICE_CONNECTED)
				Con_Printf(S_COLOR_GRAY "%s connection established (peer %s).\n", con->proto == ICEP_VOICE?"voice":"data", NET_AdrToString(msg, sizeof(msg), &con->chosenpeer));
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
	else if (!strcmp(prop, "sdp") || !strcmp(prop, "sdpoffer") || !strcmp(prop, "sdpanswer"))
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
				if (eol>value && line[eol-value-1] == '\r')
					line[eol-value-1] = 0;
				ICE_ParseSDPLine(con, line);
			}

			if (eol && *eol)
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
	else if (!strcmp(prop, "sdp") || !strcmp(prop, "sdpoffer") || !strcmp(prop, "sdpanswer"))
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

		Q_strncpyz(value, "v=0\n", valuelen);	//version...
		Q_strncatz(value, va("o=%s %u %u IN IP4 %s\n", "-", con->originid, con->originversion, con->originaddress), valuelen);	//originator. usually just dummy info.
		Q_strncatz(value, va("s=%s\n", con->conname), valuelen);	//session name.
		Q_strncatz(value, "t=0 0\n", valuelen);	//start+end times...
		Q_strncatz(value, va("a=ice-options:trickle\n"), valuelen);

		if ((con->proto == ICEP_QWSERVER || con->proto == ICEP_QWCLIENT) && con->mode == ICEM_WEBRTC)
		{
#ifdef HAVE_DTLS
			if (con->cred.local.certsize)
			{
				qbyte fingerprint[DIGEST_MAXSIZE];
				int b;
				CalcHash(&hash_sha256, fingerprint, sizeof(fingerprint), con->cred.local.cert, con->cred.local.certsize);
				Q_strncatz(value, "a=fingerprint:sha-256", valuelen);
				for (b = 0; b < hash_sha256.digestsize; b++)
					Q_strncatz(value, va(b?":%02X":" %02X", fingerprint[b]), valuelen);
				Q_strncatz(value, "\n", valuelen);
			}
			Q_strncatz(value, "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\n", valuelen);
#endif
		}
//		Q_strncatz(value, va("c=IN %s %s\n", sender.type==NA_IPV6?"IP6":"IP4", NET_BaseAdrToString(tmpstr, sizeof(tmpstr), &sender)), valuelen);
		Q_strncatz(value, "c=IN IP4 0.0.0.0\n", valuelen);
		Q_strncatz(value, va("a=ice-pwd:%s\n", con->lpwd), valuelen);
		Q_strncatz(value, va("a=ice-ufrag:%s\n", con->lufrag), valuelen);

		if (con->dtlsfuncs)
		{
			if (!strcmp(prop, "sdpanswer"))
			{	//answerer decides.
				if (con->dtlspassive)
					Q_strncatz(value, va("a=setup:passive\n"), valuelen);
				else
					Q_strncatz(value, va("a=setup:active\n"), valuelen);
			}
			else if (!strcmp(prop, "sdpoffer"))
				Q_strncatz(value, va("a=setup:actpass\n"), valuelen);	//don't care if we're active or passive
		}

		if (con->mysctpport)
			Q_strncatz(value, va("a=sctp-port:%i\n", con->mysctpport), valuelen);	//stupid hardcoded thing.

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

static void ICE_Debug(struct icestate_s *con, qboolean islisten)
{
	struct icecandidate_s *can;
	char buf[65536];
	ICE_Get(con, "state", buf, sizeof(buf));
	Con_Printf("ICE \"%s\" (%s):\n", con->friendlyname, buf);
	if (islisten)
		ICE_Get(con, "sdpanswer", buf, sizeof(buf));
	else
		ICE_Get(con, "sdpoffer", buf, sizeof(buf));
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
	if (con->cred.local.cert)
		Z_Free(con->cred.local.cert);
	if (con->cred.local.key)
		Z_Free(con->cred.local.key);
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
		case ICEM_WEBRTC:
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
				if (con->sctp)
					SCTP_Transmit(con->sctp, con, NULL,0);	//try to keep it ticking...
				if (con->dtlsfuncs)
					con->dtlsfuncs->Timeouts(con->dtlsstate);

				//FIXME: We should be sending a stun binding indication every 15 secs with a fingerprint attribute
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



#if defined(SUPPORT_ICE)
//========================================
//WebRTC's interpretation of SCTP. its annoying, but hey its only 28 wasted bytes... along with the dtls overhead too. most of this is redundant.
//we only send unreliably.

struct sctp_header_s
{
	quint16_t srcport;
	quint16_t dstport;
	quint32_t verifycode;
	quint32_t crc;
};
struct sctp_chunk_s
{
	qbyte type;
#define SCTP_TYPE_DATA 0
#define SCTP_TYPE_INIT 1
#define SCTP_TYPE_INITACK 2
#define SCTP_TYPE_SACK 3
#define SCTP_TYPE_PING 4
#define SCTP_TYPE_PONG 5
#define SCTP_TYPE_ABORT 6
#define SCTP_TYPE_SHUTDOWN 7
#define SCTP_TYPE_SHUTDOWNACK 8
#define SCTP_TYPE_ERROR 9
#define SCTP_TYPE_COOKIEECHO 10
#define SCTP_TYPE_COOKIEACK 11
#define SCTP_TYPE_SHUTDOWNDONE 14
#define SCTP_TYPE_FORWARDTSN 192
	qbyte flags;
	quint16_t length;
	//value...
};
struct sctp_chunk_data_s
{
	struct sctp_chunk_s chunk;
	quint32_t tsn;
	quint16_t sid;
	quint16_t seq;
	quint32_t ppid;
#define SCTP_PPID_DCEP 50 //datachannel establishment protocol
#define SCTP_PPID_DATA 53 //our binary quake data.
};
struct sctp_chunk_init_s
{
	struct sctp_chunk_s chunk;
	quint32_t verifycode;
	quint32_t arwc;
	quint16_t numoutstreams;
	quint16_t numinstreams;
	quint32_t tsn;
};
struct sctp_chunk_sack_s
{
	struct sctp_chunk_s chunk;
	quint32_t tsn;
	quint32_t arwc;
	quint16_t gaps;
	quint16_t dupes;
	/*struct {
		quint16_t first;
		quint16_t last;
	} gapblocks[];	//actually received rather than gaps, but same meaning.
	quint32_t dupe_tsns[];*/
};
struct sctp_chunk_fwdtsn_s
{
	struct sctp_chunk_s chunk;
	quint32_t tsn;
	struct
	{
		quint16_t sid;
		quint16_t seq;
	} streams[1];//...
};

static neterr_t SCTP_PeerSendPacket(struct icestate_s *peer, int length, const void *data)
{	//sends to the dtls layer (which will send to the generic ice dispatcher that'll send to the dgram stuff... layers on layers.
	if (length<=12)
		return NETERR_DISCONNECTED;
	if (peer)
		return peer->dtlsfuncs->Transmit(peer->dtlsstate, data, length);
	else
		return NETERR_NOROUTE;
}

static quint32_t SCTP_Checksum(const struct sctp_header_s *h, size_t size)
{
    int k;
    const qbyte *buf = (const qbyte*)h;
    size_t ofs;
    uint32_t crc = 0xFFFFFFFF;

	for (ofs = 0; ofs < size; ofs++)
    {
		if (ofs >= 8 && ofs < 8+4)
			;	//the header's crc should be read as 0.
		else
			crc ^= buf[ofs];
        for (k = 0; k < 8; k++)	            //CRC-32C polynomial 0x1EDC6F41 in reversed bit order.
            crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
    }
    return ~crc;
}

static neterr_t SCTP_Transmit(sctp_t *sctp, struct icestate_s *peer, const void *data, size_t length)
{
	qbyte pkt[65536];
	size_t pktlen = 0;
	struct sctp_header_s *h = (void*)pkt;
	struct sctp_chunk_data_s *d;
	struct sctp_chunk_fwdtsn_s *fwd;
	if (length > sizeof(pkt))
		return NETERR_MTU;

	h->dstport = sctp->peerport;
	h->srcport = sctp->myport;
	h->verifycode = sctp->o.verifycode;
	pktlen += sizeof(*h);

	if (!sctp->o.writable)
	{
		double time = Sys_DoubleTime();
		if (time > sctp->nextreinit)
		{
			sctp->nextreinit = time + 0.5;
			if (!sctp->cookie)
			{
				struct sctp_chunk_init_s *init = (struct sctp_chunk_init_s*)&pkt[pktlen];
				struct {
					quint16_t ptype;
					quint16_t plen;
				} *ftsn = (void*)(init+1);
				h->verifycode = 0;
				init->chunk.type = SCTP_TYPE_INIT;
				init->chunk.flags = 0;
				init->chunk.length = BigShort(sizeof(*init)+sizeof(*ftsn));
				init->verifycode = sctp->i.verifycode;
				init->arwc = BigLong(65535);
				init->numoutstreams = BigShort(2);
				init->numinstreams = BigShort(2);
				init->tsn = sctp->o.tsn;
				ftsn->ptype = BigShort(49152);
				ftsn->plen = BigShort(sizeof(*ftsn));
				pktlen += sizeof(*init) + sizeof(*ftsn);

				h->crc = SCTP_Checksum(h, pktlen);
				return SCTP_PeerSendPacket(peer, pktlen, h);
			}
			else
			{
				struct sctp_chunk_s *cookie = (struct sctp_chunk_s*)&pkt[pktlen];

				if (pktlen + sizeof(*cookie) + sctp->cookiesize > sizeof(pkt))
					return NETERR_DISCONNECTED;
				cookie->type = SCTP_TYPE_COOKIEECHO;
				cookie->flags = 0;
				cookie->length = BigShort(sizeof(*cookie)+sctp->cookiesize);
				memcpy(cookie+1, sctp->cookie, sctp->cookiesize);
				pktlen += sizeof(*cookie) + sctp->cookiesize;

				h->crc = SCTP_Checksum(h, pktlen);
				return SCTP_PeerSendPacket(peer, pktlen, h);
			}
		}

		return NETERR_CLOGGED;	//nope, not ready yet
	}

	if (sctp->peerhasfwdtsn && sctp->o.ctsn < sctp->o.tsn && sctp->o.losttsn)
	{
		fwd = (struct sctp_chunk_fwdtsn_s*)&pkt[pktlen];
		fwd->chunk.type = SCTP_TYPE_FORWARDTSN;
		fwd->chunk.flags = 0;
		fwd->chunk.length = BigShort(sizeof(*fwd));
		fwd->tsn = BigLong(sctp->o.tsn-1);
		fwd->streams[0].sid = sctp->qstreamid;
		fwd->streams[0].seq = BigShort(0);
		pktlen += sizeof(*fwd);
	}

	if (sctp->i.ackneeded >= 2)
	{
		struct sctp_chunk_sack_s *rsack;
		struct sctp_chunk_sack_gap_s {
			uint16_t first;
			uint16_t last;
		} *rgap;
		quint32_t otsn;

		rsack = (struct sctp_chunk_sack_s*)&pkt[pktlen];
		rsack->chunk.type = SCTP_TYPE_SACK;
		rsack->chunk.flags = 0;
		rsack->chunk.length = BigShort(sizeof(*rsack));
		rsack->tsn = BigLong(sctp->i.ctsn);
		rsack->arwc = BigLong(65535);
		rsack->gaps = 0;
		rsack->dupes = BigShort(0);
		pktlen += sizeof(*rsack);

		rgap = (struct sctp_chunk_sack_gap_s*)&pkt[pktlen];
		for (otsn = 0; otsn < sctp->i.htsn; otsn++)
		{
			quint32_t tsn = sctp->i.ctsn+otsn;
			if (!(sctp->i.received[(tsn>>3)%sizeof(sctp->i.received)] & 1<<(tsn&7)))
				continue;	//missing, don't report it in the 'gaps'... yeah, backwards naming.
			if (rsack->gaps && rgap[-1].last == otsn-1)
				rgap[-1].last = otsn;	//merge into the last one.
			else
			{
				rgap->first = otsn;	//these values are Offset from the Cumulative TSN, to save storage.
				rgap->last = otsn;
				rgap++;
				rsack->gaps++;
				pktlen += sizeof(*rgap);
				if (pktlen >= 500)
					break;	//might need fragmentation... just stop here.
			}
		}
		for (otsn = 0, rgap = (struct sctp_chunk_sack_gap_s*)&pkt[pktlen]; otsn < rsack->gaps; otsn++)
		{
			rgap[otsn].first = BigShort(rgap[otsn].first);
			rgap[otsn].last = BigShort(rgap[otsn].last);
		}
		rsack->gaps = BigShort(rsack->gaps);

		sctp->i.ackneeded = 0;
	}

	if (pktlen + sizeof(*d) + length >= 500 && length && pktlen != sizeof(*h))
	{	//probably going to result in fragmentation issues. send separate packets.
		h->crc = SCTP_Checksum(h, pktlen);
		SCTP_PeerSendPacket(peer, pktlen, h);

		//reset to the header
		pktlen = sizeof(*h);
	}

	if (length)
	{
		d = (void*)&pkt[pktlen];
		d->chunk.type = SCTP_TYPE_DATA;
		d->chunk.flags = 3|4;
		d->chunk.length = BigShort(sizeof(*d) + length);
		d->tsn = BigLong(sctp->o.tsn++);
		d->sid = sctp->qstreamid;
		d->seq = BigShort(0); //not needed for unordered
		d->ppid = BigLong(SCTP_PPID_DATA);
		memcpy(d+1, data, length);
		pktlen += sizeof(*d) + length;

		//chrome insists on pointless padding at the end. its annoying.
		while(pktlen&3)
			pkt[pktlen++]=0;
	}
	if (pktlen == sizeof(*h))
		return NETERR_SENT; //nothing to send...

	h->crc = SCTP_Checksum(h, pktlen);
	return SCTP_PeerSendPacket(peer, pktlen, h);
}

static void SCTP_DecodeDCEP(sctp_t *sctp, struct icestate_s *peer, qbyte *resp)
{	//send an ack...
	size_t pktlen = 0;
	struct sctp_header_s *h = (void*)resp;
	struct sctp_chunk_data_s *d;
	char *data = "\02";
	size_t length = 1; //*sigh*...

	struct
	{
		qbyte type;
		qbyte chantype;
		quint16_t priority;
		quint32_t relparam;
		quint16_t labellen;
		quint16_t protocollen;
	} *dcep = (void*)sctp->i.r.buf;

	if (dcep->type == 3)
	{
		char *label = (qbyte*)(dcep+1);
		char *prot = label + strlen(label)+1;

		sctp->qstreamid = sctp->i.r.sid;
		Con_DPrintf("New SCTP Channel: \"%s\" (%s)\n", label, prot);

		h->dstport = sctp->peerport;
		h->srcport = sctp->myport;
		h->verifycode = sctp->o.verifycode;
		pktlen += sizeof(*h);

		pktlen = (pktlen+3)&~3;	//pad.
		d = (void*)&resp[pktlen];
		d->chunk.type = SCTP_TYPE_DATA;
		d->chunk.flags = 3;
		d->chunk.length = BigShort(sizeof(*d) + length);
		d->tsn = BigLong(sctp->o.tsn++);
		d->sid = sctp->qstreamid;
		d->seq = BigShort(0); //not needed for unordered
		d->ppid = BigLong(SCTP_PPID_DCEP);
		memcpy(d+1, data, length);
		pktlen += sizeof(*d) + length;

		h->crc = SCTP_Checksum(h, pktlen);
		SCTP_PeerSendPacket(peer, pktlen, h);
	}
}

struct sctp_errorcause_s
{
	quint16_t cause;
	quint16_t length;
};
static void SCTP_ErrorChunk(const char *errortype, struct sctp_errorcause_s *s, size_t totallen)
{
	quint16_t cc, cl;
	while(totallen > 0)
	{
		if (totallen < sizeof(*s))
			return;	//that's an error in its own right
		cc = BigShort(s->cause);
		cl = BigShort(s->length);
		if (totallen < cl)
			return;	//err..

		switch(cc)
		{
		case 1:		Con_Printf("%s: Invalid Stream Identifier\n",	errortype);	break;
        case 2:		Con_Printf("%s: Missing Mandatory Parameter\n", errortype);	break;
        case 3:		Con_Printf("%s: Stale Cookie Error\n",			errortype);	break;
        case 4:		Con_Printf("%s: Out of Resource\n",				errortype);	break;
        case 5:		Con_Printf("%s: Unresolvable Address\n",		errortype);	break;
        case 6:		Con_Printf("%s: Unrecognized Chunk Type\n",		errortype);	break;
        case 7:		Con_Printf("%s: Invalid Mandatory Parameter\n", errortype);	break;
        case 8:		Con_Printf("%s: Unrecognized Parameters\n",		errortype);	break;
        case 9:		Con_Printf("%s: No User Data\n",				errortype);	break;
        case 10:	Con_Printf("%s: Cookie Received While Shutting Down\n",				errortype);	break;
        case 11:	Con_Printf("%s: Restart of an Association with New Addresses\n",	errortype);	break;
        case 12:	Con_Printf("%s: User Initiated Abort\n",		errortype);	break;
        case 13:	Con_Printf("%s: Protocol Violation [%s]\n",		errortype, (char*)(s+1));	break;
        default:	Con_Printf("%s: Unknown Reason\n",				errortype);	break;
		}

		totallen -= cl;
		totallen &= ~3;
		s = (struct sctp_errorcause_s*)((qbyte*)s + ((cl+3)&~3));
	}
}

static void SCTP_Decode(sctp_t *sctp, struct icestate_s *peer)
{
	qbyte resp[4096];
	qboolean finished = false;

	qbyte *msg = net_message.data;
	qbyte *msgend = net_message.data+net_message.cursize;
	struct sctp_header_s *h = (struct sctp_header_s*)msg;
	struct sctp_chunk_s *c = (struct sctp_chunk_s*)(h+1);
	quint16_t clen;
	if ((qbyte*)c+1 > msgend)
		return;	//runt
	if (h->dstport != sctp->myport)
		return;	//not for us...
	if (h->srcport != sctp->peerport)
		return; //not from them... we could check for a INIT but its over dtls anyway so why give a damn.
	if (h->verifycode != ((c->type == SCTP_TYPE_INIT)?0:sctp->i.verifycode))
		return;	//wrong cookie... (be prepared to parse dupe inits if our ack got lost...
	if (h->crc != SCTP_Checksum(h, net_message.cursize))
		return;	//crc wrong. assume corruption.
	if (net_message.cursize&3)
	{
		Con_DPrintf("SCTP: packet not padded\n");
		return;	//mimic chrome, despite it being pointless.
	}

	while ((qbyte*)(c+1) <= msgend)
	{
		clen = BigShort(c->length);
		if ((qbyte*)c + clen > msgend)
			break;	//corrupt
		safeswitch(c->type)
		{
		case SCTP_TYPE_DATA:
			if (clen >= sizeof(struct sctp_chunk_data_s))
			{
				struct sctp_chunk_data_s *dc = (void*)c;
				quint32_t tsn = BigLong(dc->tsn), u;
				qint32_t adv = tsn - sctp->i.ctsn;
				sctp->i.ackneeded++;
				if (adv >= SCTP_RCVSIZE)
					Con_DPrintf("SCTP: Future Packet\n");/*too far in the future. we can't track such things*/
				else if (adv <= 0)
					Con_DPrintf("SCTP: PreCumulative\n");/*already acked this*/
				else if (sctp->i.received[(tsn>>3)%sizeof(sctp->i.received)] & 1<<(tsn&7))
					Con_DPrintf("SCTP: Dupe\n");/*already processed it*/
				else
				{
					qboolean err = false;

					if (c->flags & 2)
					{	//beginning...
						sctp->i.r.firsttns = tsn;
						sctp->i.r.tsn = tsn;
						sctp->i.r.size = 0;
						sctp->i.r.ppid = dc->ppid;
						sctp->i.r.sid = dc->sid;
						sctp->i.r.seq = dc->seq;
						sctp->i.r.toobig = false;
						if (finished)
							Con_Printf("SCTP: Multiple data chunks\n");
						finished = false;
					}
					else
					{
						if (sctp->i.r.tsn != tsn || sctp->i.r.ppid != dc->ppid)
							err = true;
					}
					if (err)
						;	//don't corrupt anything in case we get a quick resend that fixes it.
					else
					{
						size_t dlen = clen-sizeof(*dc);
						if (adv > sctp->i.htsn)	//weird maths in case it wraps.
							sctp->i.htsn = adv;
						sctp->i.r.tsn++;
						if (sctp->i.r.size + clen-sizeof(*dc) > sizeof(sctp->i.r.buf))
						{
							Con_DPrintf("SCTP: Oversized\n");
							sctp->i.r.toobig = true;	//reassembled packet was too large, just corrupt it.
						}
						else
						{
							memcpy(sctp->i.r.buf+sctp->i.r.size, dc+1, dlen);	//include the dc header
							sctp->i.r.size += dlen;
						}
						if (c->flags & 1)	//an ending. we have the complete packet now.
						{
							for (u = sctp->i.r.tsn - sctp->i.r.firsttns; u --> 0; )
							{
								tsn = sctp->i.r.firsttns + u;
								sctp->i.received[(tsn>>3)%sizeof(sctp->i.received)] |= 1<<(tsn&7);
							}
							if (sctp->i.r.toobig)
								;/*ignore it when it cannot be handled*/
							else if (sctp->i.r.ppid == BigLong(SCTP_PPID_DATA))
								finished = true; //FIXME: handle multiple small packets
							else if (sctp->i.r.ppid == BigLong(SCTP_PPID_DCEP))
								SCTP_DecodeDCEP(sctp, peer, resp);
						}
					}

					//FIXME: we don't handle reordering properly at all.

//					if (c->flags & 4)
//						Con_Printf("\tUnordered\n");
//					Con_Printf("\tStream Id %i\n", BigShort(dc->sid));
//					Con_Printf("\tStream Seq %i\n", BigShort(dc->seq));
//					Con_Printf("\tPPID %i\n", BigLong(dc->ppid));

					while(sctp->i.htsn)
					{
						tsn = sctp->i.ctsn+1;
						if (!(sctp->i.received[(tsn>>3)%sizeof(sctp->i.received)] & 1<<(tsn&7)))
							break;
						//advance our cumulative ack.
						sctp->i.received[(tsn>>3)%sizeof(sctp->i.received)] &= ~(1<<(tsn&7));
						sctp->i.ctsn = tsn;
						sctp->i.htsn--;
					}
				}
			}
			break;
		case SCTP_TYPE_INIT:
		case SCTP_TYPE_INITACK:
			if (clen >= sizeof(struct sctp_chunk_init_s))
			{
				qboolean isack = c->type==SCTP_TYPE_INITACK;
				struct sctp_chunk_init_s *init = (void*)c;
				struct {
						quint16_t ptype;
						quint16_t plen;
				} *p = (void*)(init+1);

				sctp->i.ctsn = BigLong(init->tsn)-1;
				sctp->i.htsn = 0;
				sctp->o.verifycode = init->verifycode;
				(void)BigLong(init->arwc);
				(void)BigShort(init->numoutstreams);
				(void)BigShort(init->numinstreams);

				while ((qbyte*)p+sizeof(*p) <= (qbyte*)c+clen)
				{
					unsigned short ptype = BigShort(p->ptype);
					unsigned short plen = BigShort(p->plen);
					switch(ptype)
					{
					case 7:	//init cookie
						if (sctp->cookie)
							Z_Free(sctp->cookie);
						sctp->cookiesize = plen - sizeof(*p);
						sctp->cookie = Z_Malloc(sctp->cookiesize);
						memcpy(sctp->cookie, p+1, sctp->cookiesize);
						break;
					case 32776:	//ASCONF
						break;
					case 49152:
						sctp->peerhasfwdtsn = true;
						break;
					default:
						Con_DPrintf("SCTP: Found unknown init parameter %i||%#x\n", ptype, ptype);
						break;
					}
					p = (void*)((qbyte*)p + ((plen+3)&~3));
				}

				if (isack)
				{
					sctp->nextreinit = 0;
					if (sctp->cookie)
						SCTP_Transmit(sctp, peer, NULL, 0);	//make sure we send acks occasionally even if we have nothing else to say.
				}
				else
				{
					struct sctp_header_s *rh = (void*)resp;
					struct sctp_chunk_init_s *rinit = (void*)(rh+1);
					struct {
						quint16_t ptype;
						quint16_t plen;
						struct {
							qbyte data[16];
						} cookie;
					} *rinitcookie = (void*)(rinit+1);
					struct {
						quint16_t ptype;
						quint16_t plen;
					} *rftsn = (void*)(rinitcookie+1);
					qbyte *end = sctp->peerhasfwdtsn?(void*)(rftsn+1):(void*)(rinitcookie+1);

					rh->srcport = sctp->myport;
					rh->dstport = sctp->peerport;
					rh->verifycode = sctp->o.verifycode;
					rh->crc = 0;
					*rinit = *init;
					rinit->chunk.type = SCTP_TYPE_INITACK;
					rinit->chunk.flags = 0;
					rinit->chunk.length = BigShort(end-(qbyte*)rinit);
					rinit->verifycode = sctp->i.verifycode;
					rinit->arwc = BigLong(65536);
					rinit->numoutstreams = init->numoutstreams;
					rinit->numinstreams = init->numinstreams;
					rinit->tsn = BigLong(sctp->o.tsn);
					rinitcookie->ptype = BigShort(7);
					rinitcookie->plen = BigShort(sizeof(*rinitcookie));
					memcpy(&rinitcookie->cookie, "deadbeefdeadbeef", sizeof(rinitcookie->cookie));	//frankly the contents of the cookie are irrelevant to anything. we've already verified the peer's ice pwd/ufrag stuff as well as their dtls certs etc.
					rftsn->ptype = BigShort(49152);
					rftsn->plen = BigShort(sizeof(*rftsn));

					//complete. calc the proper crc and send it off.
					rh->crc = SCTP_Checksum(rh, end-resp);
					SCTP_PeerSendPacket(peer, end-resp, rh);
				}
			}
			break;
		case SCTP_TYPE_SACK:
			if (clen >= sizeof(struct sctp_chunk_sack_s))
			{
				struct sctp_chunk_sack_s *sack = (void*)c;
				quint32_t tsn = BigLong(sack->tsn);
				sctp->o.ctsn = tsn;

				sctp->o.losttsn = BigShort(sack->gaps);	//if there's a gap then they're telling us they got a later one.

				//Con_Printf(CON_ERROR"Sack %#x (%i in flight)\n"
				//			"\tgaps: %i, dupes %i\n",
				//			tsn, sctp->o.tsn-tsn,
				//			BigShort(sack->gaps), BigShort(sack->dupes));
			}
			break;
		case SCTP_TYPE_PING:
			if (clen >= sizeof(struct sctp_chunk_s))
			{
				struct sctp_chunk_s *ping = (void*)c;
				struct sctp_header_s *pongh = Z_Malloc(sizeof(*pongh) + clen);

				pongh->srcport = sctp->myport;
				pongh->dstport = sctp->peerport;
				pongh->verifycode = sctp->o.verifycode;
				pongh->crc = 0;
				memcpy(pongh+1, ping, clen);
				((struct sctp_chunk_s*)(pongh+1))->type = SCTP_TYPE_PONG;

				//complete. calc the proper crc and send it off.
				pongh->crc = SCTP_Checksum(pongh, sizeof(*pongh) + clen);
				SCTP_PeerSendPacket(peer, sizeof(*pongh) + clen, pongh);
				Z_Free(pongh);
			}
			break;
//		case SCTP_TYPE_PONG:	//we don't send pings
		case SCTP_TYPE_ABORT:
			ICE_Set(peer, "state", STRINGIFY(ICE_FAILED));
			SCTP_ErrorChunk("Abort", (struct sctp_errorcause_s*)(c+1), clen-sizeof(*c));
			break;
		case SCTP_TYPE_SHUTDOWN:	//FIXME. we should send an ack...
			ICE_Set(peer, "state", STRINGIFY(ICE_FAILED));
			Con_DPrintf(CON_ERROR"SCTP: Shutdown\n");
			break;
//		case SCTP_TYPE_SHUTDOWNACK:	//we don't send shutdowns, cos we're lame like that...
		case SCTP_TYPE_ERROR:
			//not fatal...
			SCTP_ErrorChunk("Error", (struct sctp_errorcause_s*)(c+1), clen-sizeof(*c));
			break;
		case SCTP_TYPE_COOKIEECHO:
			if (clen >= sizeof(struct sctp_chunk_s))
			{
				struct sctp_header_s *rh = (void*)resp;
				struct sctp_chunk_s *rack = (void*)(rh+1);
				qbyte *end = (void*)(rack+1);

				rh->srcport = sctp->myport;
				rh->dstport = sctp->peerport;
				rh->verifycode = sctp->o.verifycode;
				rh->crc = 0;
				rack->type = SCTP_TYPE_COOKIEACK;
				rack->flags = 0;
				rack->length = BigShort(sizeof(*rack));

				//complete. calc the proper crc and send it off.
				rh->crc = SCTP_Checksum(rh, end-resp);
				SCTP_PeerSendPacket(peer, end-resp, rh);

				sctp->o.writable = true;	//channel SHOULD now be open for data.
			}
			break;
		case SCTP_TYPE_COOKIEACK:
			sctp->o.writable = true;	//we know the other end is now open.
			break;
		case SCTP_TYPE_FORWARDTSN:
			if (clen >= sizeof(struct sctp_chunk_fwdtsn_s))
			{
				struct sctp_chunk_fwdtsn_s *fwd = (void*)c;
				quint32_t tsn = BigLong(fwd->tsn), count;
				count = tsn - sctp->i.ctsn;
				if ((int)count < 0)
					break;	//overflow? don't go backwards.
				if (count > 1024)
					count = 1024; //don't advance too much in one go. we'd block and its probably an error anyway.
				while(count --> 0)
				{
					tsn = ++sctp->i.ctsn;
					sctp->i.received[(tsn>>3)%sizeof(sctp->i.received)] &= ~(1<<(tsn&7));
					if (sctp->i.htsn)
						sctp->i.htsn--;
				}
			}
			break;
//		case SCTP_TYPE_SHUTDOWNDONE:
		safedefault:
			//no idea what this chunk is, just ignore it...
			Con_DPrintf("SCTP: Unsupported chunk %i\n", c->type);
			break;
		}
		c = (struct sctp_chunk_s*)((qbyte*)c + ((clen+3)&~3));	//next chunk is 4-byte aligned.
	}

	if (sctp->i.ackneeded >= 5)
		SCTP_Transmit(sctp, peer, NULL, 0);	//make sure we send acks occasionally even if we have nothing else to say.

	//if we read something, spew it out and return to caller.
	if (finished)
	{
		memmove(net_message.data, sctp->i.r.buf, sctp->i.r.size);
		net_message.cursize = sctp->i.r.size;
	}
	else
		net_message.cursize = 0;	//nothing to read.
}

//========================================
#endif

#if defined(SUPPORT_ICE) || defined(MASTERONLY)
qboolean ICE_WasStun(ftenet_connections_t *col)
{
#ifdef SUPPORT_ICE
	if (net_from.type == NA_ICE)
		return false;	//this stuff over an ICE connection doesn't make sense.
#endif

#if defined(HAVE_CLIENT) && defined(VOICECHAT)
	if (col == cls.sockets)
	{
		if (NET_RTP_Parse())
			return true;
	}
#endif

	if ((net_from.type == NA_IP || net_from.type == NA_IPV6) && net_message.cursize >= 20 && *net_message.data<2)
	{
		stunhdr_t *stun = (stunhdr_t*)net_message.data;
		int stunlen = BigShort(stun->msglen);
#ifdef SUPPORT_ICE
		if ((stun->msgtype == BigShort(STUN_BINDING|STUN_REPLY) || stun->msgtype == BigShort(STUN_BINDING|STUN_ERROR)) && net_message.cursize == stunlen + sizeof(*stun))
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
							if (con->mode == ICEM_RAW)
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
		else if (stun->msgtype == BigShort(STUN_BINDING|STUN_INDICATION))// && net_message.cursize == stunlen + sizeof(*stun) && stun->magiccookie == BigLong(0x2112a442))
		{
			//binding indication. used as an rtp keepalive. should have a fingerprint
			return true;
		}
		else
#endif
			if (stun->msgtype == BigShort(STUN_BINDING|STUN_REQUEST) && net_message.cursize == stunlen + sizeof(*stun) && stun->magiccookie == BigLong(0x2112a442))
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
				{				case 0xc057: /*'network cost'*/ break;
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
				/*else if (con->chosenpeer.type != NA_INVALID)
				{	//got one.
					if (!NET_CompareAdr(&net_from, &con->chosenpeer))
						return true;	//FIXME: we're too stupid to handle switching. pretend to be dead.
				}*/
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
						CalcHMAC(&hash_sha1, key, sizeof(key), (qbyte*)stun, integritypos-4 - (char*)stun, con->lpwd, strlen(con->lpwd));
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
							Con_DPrintf(CON_WARNING"ICE: Alternative use-candidate\n");
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

			MSG_WriteShort(&buf, BigShort(error?(STUN_BINDING|STUN_ERROR):(STUN_BINDING|STUN_REPLY)));
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
				CalcHMAC(&hash_sha1, integrity, sizeof(integrity), data, buf.cursize, con->lpwd, strlen(con->lpwd));
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


#ifdef SUPPORT_ICE
	{
		struct icestate_s *con;
		struct icecandidate_s *rc;
		for (con = icelist; con; con = con->next)
		{
			for (rc = con->rc; rc; rc = rc->next)
			{
				if (NET_CompareAdr(&net_from, &rc->peer))
				{
				//	if (rc->reachable)
					{	//found it. fix up its source address to our ICE connection (so we don't have path-switching issues) and keep chugging along.

						con->timeout = Sys_Milliseconds() + 32;	//not dead yet...

						if (con->dtlsstate)
						{
							switch(con->dtlsfuncs->Received(con->dtlsstate, &net_message))
							{
							case NETERR_SENT:
								break;	//
							case NETERR_NOROUTE:
								return false;	//not a dtls packet at all. don't de-ICE it when we're meant to be using ICE.
							case NETERR_DISCONNECTED:	//dtls failure. ICE failed.
								iceapi.ICE_Set(con, "state", STRINGIFY(ICE_FAILED));
								return true;
							default: //some kind of failure decoding the dtls packet. drop it.
								return true;
							}
						}
						net_from = con->qadr;
						if (con->sctp)
							SCTP_Decode(con->sctp, con);
						if (net_message.cursize)
							col->ReadGamePacket();
						return true;
					}
				}
			}
		}
	}
#endif
	return false;
}
#ifdef SUPPORT_ICE
neterr_t ICE_SendPacket(ftenet_connections_t *col, size_t length, const void *data, netadr_t *to)
{
	struct icestate_s *con;
	for (con = icelist; con; con = con->next)
	{
		if (NET_CompareAdr(to, &con->qadr))
		{
			if (con->sctp)
				return SCTP_Transmit(con->sctp, con, data, length);
			if (con->dtlsstate)
				return SCTP_PeerSendPacket(con, length, data);
			if (con->chosenpeer.type != NA_INVALID)
				return NET_SendPacket(col, length, data, &con->chosenpeer);
			if (con->state < ICE_CONNECTING)
				return NETERR_DISCONNECTED;
			return NETERR_CLOGGED;	//still pending
		}
	}
	return NETERR_DISCONNECTED;
}
#endif
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
static void FTENET_ICE_SendOffer(ftenet_ice_connection_t *b, int cl, struct icestate_s *ice, const char *type)
{
	char buf[8192];
	//okay, now send the sdp to our peer.
	if (iceapi.ICE_Get(ice, type, buf, sizeof(buf)))
	{
		char json[8192+256];
		if (ice->mode == ICEM_WEBRTC)
		{
			Q_strncpyz(json, va("{\"type\":\"%s\",\"sdp\":\"", type+3), sizeof(json));
			COM_QuotedString(buf, json+strlen(json), sizeof(json)-strlen(json)-2, true);
			Q_strncatz(json, "\"}", sizeof(json));
			FTENET_ICE_SplurgeCmd(b, ICEMSG_OFFER, cl, json);
		}
		else
			FTENET_ICE_SplurgeCmd(b, ICEMSG_OFFER, cl, buf);

		ice->blockcandidates = false;
	}
}
static void FTENET_ICE_Establish(ftenet_ice_connection_t *b, int cl, struct icestate_s **ret)
{	//sends offer
	char buf[256];
	struct icestate_s *ice;
	if (*ret)
		iceapi.ICE_Close(*ret);
	ice = *ret = iceapi.ICE_Create(b, NULL, b->generic.islisten?NULL:va("/%s", b->gamename), ICEM_WEBRTC, b->generic.islisten?ICEP_QWSERVER:ICEP_QWCLIENT);
	if (!*ret)
		return;	//some kind of error?!?
	iceapi.ICE_Set(ice, "controller", b->generic.islisten?"0":"1");

	Q_snprintfz(buf, sizeof(buf), "%i", BigShort(b->brokeradr.port));
	iceapi.ICE_Set(ice, "stunport", buf);
	iceapi.ICE_Set(ice, "stunip", b->brokername);

	if (!b->generic.islisten)
		FTENET_ICE_SendOffer(b, cl, ice, "sdpoffer");
}
static void FTENET_ICE_Refresh(ftenet_ice_connection_t *b, int cl, struct icestate_s *ice)
{	//sends offer
	char buf[8192];
	if (ice->blockcandidates)
		return;	//don't send candidates before the offers...
	while (ice && iceapi.ICE_GetLCandidateSDP(ice, buf, sizeof(buf)))
	{
		char json[8192+256];
		if (ice->mode == ICEM_WEBRTC)
		{
			Q_strncpyz(json, "{\"candidate\":\"", sizeof(json));
			COM_QuotedString(buf+2, json+strlen(json), sizeof(json)-strlen(json)-2, true);
			Q_strncatz(json, "\",\"sdpMid\":\"0\",\"sdpMLineIndex\":0}", sizeof(json));
			FTENET_ICE_SplurgeCmd(b, ICEMSG_CANDIDATE, cl, json);
		}
		else
			FTENET_ICE_SplurgeCmd(b, ICEMSG_CANDIDATE, cl, buf);
	}
}
static qboolean FTENET_ICE_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_ice_connection_t *b = (void*)gcon;
	int ctrl, len, cmd, cl, ofs;
	char *data, n;

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
			case ICEMSG_NAMEINUSE:
				Con_Printf("Unable to listen on /%s - name already taken\n", b->gamename);
				b->error = true;	//try again later.
				break;
			case ICEMSG_GREETING:	//reports the trailing url we're 'listening' on. anyone else using that url will connect to us.
				data = strchr(data, '/');
				if (data++)
					Q_strncpyz(b->gamename, data, sizeof(b->gamename));
				Con_Printf("Publicly listening on /%s\n", b->gamename);
				break;
			case ICEMSG_NEWPEER:	//relay connection established with a new peer
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
				if (!strncmp(data, "{\"type\":\"offer\",\"sdp\":\"", 23))
				{
					data += 22;
					COM_ParseCString(data, com_token, sizeof(com_token), NULL);
					data = com_token;
				}
				else if (!strncmp(data, "{\"type\":\"answer\",\"sdp\":\"", 24))
				{
					data += 23;
					COM_ParseCString(data, com_token, sizeof(com_token), NULL);
					data = com_token;
				}
				if (b->generic.islisten)
				{
					Con_Printf("Client offered: %s\n", data);
					if (cl >= 0 && cl < b->numclients && b->clients[cl].ice)
					{
						iceapi.ICE_Set(b->clients[cl].ice, "sdpoffer", data);
						iceapi.ICE_Set(b->clients[cl].ice, "state", STRINGIFY(ICE_CONNECTING));

						FTENET_ICE_SendOffer(b, cl, b->clients[cl].ice, "sdpanswer");
					}
				}
				else
				{
					if (b->ice)
					{
						iceapi.ICE_Set(b->ice, "sdpanswer", data);
						iceapi.ICE_Set(b->ice, "state", STRINGIFY(ICE_CONNECTING));
					}
				}
				break;
			case ICEMSG_CANDIDATE:
				if (!strncmp(data, "{\"candidate\":\"", 14))
				{
					data += 13;
					com_token[0]='a';
					com_token[1]='=';
					COM_ParseCString(data, com_token+2, sizeof(com_token)-2, NULL);
					data = com_token;
				}
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
		ICE_Debug(b->ice, b->generic.islisten);
	if (b->numclients)
	{
		size_t activeice = 0;
		for (c = 0; c < b->numclients; c++)
			if (b->clients[c].ice)
			{
				activeice++;
				ICE_Debug(b->clients[c].ice, b->generic.islisten);
			}
		Con_Printf("%u ICE connections\n", (unsigned)activeice);
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
	newcon->generic.owner = col;
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