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
// net_wins.c
struct sockaddr;

#include "quakedef.h"
#include "netinc.h"
#include <stddef.h>

#ifdef UNIXSOCKETS
#include <sys/stat.h>	//to delete the file/socket.
#endif

//try to be slightly cleaner about the protocols that'll get killed.
#ifdef TCPCONNECT
	#define NP_STREAM_OR_INVALID NP_STREAM
	#define NP_TLS_OR_INVALID NP_TLS
	#define NP_WS_OR_INVALID NP_WS
	#define NP_WSS_OR_INVALID NP_WSS
#else
	#define NP_STREAM_OR_INVALID NP_INVALID
	#define NP_TLS_OR_INVALID NP_INVALID
	#define NP_WS_OR_INVALID NP_INVALID
	#define NP_WSS_OR_INVALID NP_INVALID
#endif
#define NP_DTLS_OR_INVALID NP_DTLS
#ifndef HAVE_SSL
	#undef NP_WSS_OR_INVALID
	#define NP_WSS_OR_INVALID NP_INVALID
	#undef NP_TLS_OR_INVALID
	#define NP_TLS_OR_INVALID NP_INVALID
	#undef NP_DTLS_OR_INVALID
	#define NP_DTLS_OR_INVALID NP_INVALID
#endif



extern ftemanifest_t	*fs_manifest;

// Eww, eww. This is hacky but so is netinc.h, so bite me
#ifdef _XBOX
	struct sockaddr
	{
		short  sa_family;
	};

	#define ntohs BigShort
	#define htons BigShort
	#define htonl BigLong
	#define ntohl BigLong
#endif

#if defined(_WIN32) || defined(__linux__) && !defined(ANDROID)
	#define USE_GETHOSTNAME_LOCALLISTING
#endif

netadr_t	net_local_cl_ipadr;	//still used to match local ui requests (quake/gamespy), and to generate ip reports for q3 servers (which is probably pointless).

netadr_t	net_from;
sizebuf_t	net_message;

//#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)	// one more than msg + header
#define	MAX_UDP_PACKET	8192	// one more than msg + header
//emscripten can misalign stuff, which is a problem when the leading int is checked directly in a few places. gah.
FTE_ALIGN(4) qbyte		net_message_buffer[MAX_OVERALLMSGLEN];
#if defined(_WIN32) && defined(HAVE_PACKET)
	WSADATA		winsockdata;
#endif

#if defined(_WIN32)
	int (WINAPI *pgetaddrinfo) (
	  const char* nodename,
	  const char* servname,
	  const struct addrinfo* hints,
	  struct addrinfo** res
	);
	void (WSAAPI *pfreeaddrinfo) (struct addrinfo*);
#else
	#define pgetaddrinfo getaddrinfo
	#define pfreeaddrinfo freeaddrinfo
#endif

#if defined(HAVE_IPV4) && defined(HAVE_SERVER)
	#define HAVE_NATPMP
#endif

//#if !defined(HAVE_SERVER) && !defined(MASTERONLY)
//	#undef HAVE_HTTPSV
//#endif

void NET_GetLocalAddress (int socket, netadr_t *out);
//int TCP_OpenListenSocket (const char *localip, int port);
#ifdef HAVE_IPV6
int UDP6_OpenSocket (int port);
#endif
#ifdef HAVE_IPX
void IPX_CloseSocket (int socket);
#endif
cvar_t	timeout					= CVARD("timeout","65", "Connections will time out if no packets are received for this duration of time.");		// seconds without any message
cvar_t	net_hybriddualstack		= CVARD("net_hybriddualstack",		"1", "Uses hybrid ipv4+ipv6 sockets where possible. Not supported on xp or below.");
cvar_t	net_fakeloss			= CVARFD("net_fakeloss",			"0", CVAR_CHEAT, "Simulates packetloss in both receiving and sending, on a scale from 0 to 1.");
static cvar_t net_dns_ipv4		= CVARD("net_dns_ipv4",				"1", "If 0, disables dns resolution of names to ipv4 addresses (removing any associated error messages). Also hides ipv4 addresses in address:port listings.");
static cvar_t net_dns_ipv6		= CVARD("net_dns_ipv6",				"1", "If 0, disables dns resolution of names to ipv6 addresses (removing any associated error messages). Also hides ipv6 addresses in address:port listings.");
cvar_t	net_enabled				= CVARD("net_enabled",				"1", "If 0, disables all network access, including name resolution and socket creation. Does not affect loopback/internal connections.");
#if defined(HAVE_SSL)
cvar_t	tls_ignorecertificateerrors	= CVARFD("tls_ignorecertificateerrors", "0", CVAR_NOTFROMSERVER|CVAR_NOSAVE|CVAR_NOUNSAFEEXPAND|CVAR_NOSET, "This should NEVER be set to 1!");
#endif
#if defined(TCPCONNECT) && (defined(HAVE_SERVER) || defined(HAVE_HTTPSV))
#ifdef HAVE_SERVER
cvar_t	net_enable_qizmo		= CVARD("net_enable_qizmo",			"1", "Enables compatibility with qizmo's tcp connections serverside. Frankly, using sv_port_tcp without this is a bit pointless.");
cvar_t	net_enable_qtv			= CVARD("net_enable_qtv",			"1", "Listens for qtv proxies, or clients using the qtvplay command.");
#endif
#if defined(HAVE_SSL)
cvar_t	net_enable_tls			= CVARD("net_enable_tls",			"1", "If enabled, binary data sent to a non-tls tcp port will be interpretted as a tls handshake (enabling https or wss over the same tcp port.");
#endif
#ifdef HAVE_HTTPSV
#ifdef SV_MASTER
cvar_t	net_enable_http			= CVARD("net_enable_http",			"1", "If enabled, tcp ports will accept http clients, potentially serving large files which could distrupt gameplay.");
#else
cvar_t	net_enable_http			= CVARD("net_enable_http",			"0", "If enabled, tcp ports will accept http clients, potentially serving large files which could distrupt gameplay.");
#endif
cvar_t	net_enable_websockets	= CVARD("net_enable_websockets",	"1", "If enabled, tcp ports will accept websocket game clients.");
cvar_t	net_enable_webrtcbroker	= CVARD("net_enable_webrtcbroker",	"0", "If 1, tcp ports will accept websocket connections from clients trying to broker direct webrtc connections. This should be low traffic, but might involve a lot of mostly-idle connections.");
#endif
#endif
#if defined(HAVE_DTLS) && defined(HAVE_SERVER)
static void QDECL NET_Enable_DTLS_Changed(struct cvar_s *var, char *oldvalue)
{
	//set up the default value
	if (!*var->string)
		var->ival = 0;	//FIXME: change to 1 then 2 when better tested.

	if (var->ival && svs.sockets)
	{
		if (!svs.sockets->dtlsfuncs)
			svs.sockets->dtlsfuncs = DTLS_InitServer();
		if (!svs.sockets->dtlsfuncs)
		{
			if (var->ival >= 2)
				Con_Printf("%sUnable to set %s to \"%s\", no DTLS certificate available.\n", (var->ival >= 2)?CON_ERROR:CON_WARNING, var->name, var->string);
			var->ival = 0;	//disable the cvar (internally) if we don't have a usable certificate. this allows us to default the cvar to enabled without it breaking otherwise.
		}
	}
}
cvar_t net_enable_dtls		= CVARAFCD("net_enable_dtls", "", "sv_listen_dtls", 0, NET_Enable_DTLS_Changed, "Controls serverside dtls support.\n0: dtls blocked, not advertised.\n1: available in desired.\n2: used where possible (recommended setting).\n3: disallow non-dtls clients (sv_port_tcp should be eg tls://[::]:27500 to also disallow unencrypted tcp connections).");
#endif

#ifdef HAVE_CLIENT
static void QDECL cl_delay_packets_Announce(cvar_t *var, char *oldval)
{
	if (cls.state >= ca_connected && cl.fpd & FPD_ANOUNCE_FAKE_LAG)
		Cbuf_AddText(va("say Fake lag now %ims\n", var->ival), RESTRICT_LOCAL);
}
static cvar_t	cl_delay_packets		= CVARCD("cl_delay_packets",			"0", cl_delay_packets_Announce, "Extra latency, in milliseconds.");
#endif

extern cvar_t sv_public, sv_listen_qw, sv_listen_nq, sv_listen_dp;
#ifdef QWOVERQ3
extern cvar_t sv_listen_q3;
#endif

#define	MAX_LOOPBACK	64
typedef struct
{
	qbyte	*data;
	int		datalen;
	int		datamax;
} loopmsg_t;

typedef struct
{
	qboolean	inited;
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

loopback_t	loopbacks[2];


#ifdef HAVE_DTLS
static neterr_t FTENET_DTLS_SendPacket(ftenet_connections_t *col, int length, const void *data, netadr_t *to);
#endif
static neterr_t NET_SendPacketCol (ftenet_connections_t *collection, int length, const void *data, netadr_t *to);


//=============================================================================

int NetadrToSockadr (netadr_t *a, struct sockaddr_qstorage *s)
{
	switch(a->type)
	{
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:
		memset (s, 0, sizeof(struct sockaddr_websocket));
		((struct sockaddr_websocket*)s)->sws_family = AF_WEBSOCK;
		memcpy(((struct sockaddr_websocket*)s)->url, a->address.websocketurl, sizeof(((struct sockaddr_websocket*)s)->url));
		return sizeof(struct sockaddr_websocket);
#endif
#ifdef HAVE_IPV4
	case NA_IP:
		memset (s, 0, sizeof(struct sockaddr_in));
		((struct sockaddr_in*)s)->sin_family = AF_INET;

		*(int *)&((struct sockaddr_in*)s)->sin_addr = *(int *)&a->address.ip;
		((struct sockaddr_in*)s)->sin_port = a->port;
		return sizeof(struct sockaddr_in);
#endif
#ifdef HAVE_IPV6
	case NA_IPV6:
		memset (s, 0, sizeof(struct sockaddr_in6));
		((struct sockaddr_in6*)s)->sin6_family = AF_INET6;

		memcpy(&((struct sockaddr_in6*)s)->sin6_addr, a->address.ip6, sizeof(struct in6_addr));
		((struct sockaddr_in6*)s)->sin6_port = a->port;
		((struct sockaddr_in6 *)s)->sin6_scope_id = a->scopeid;
		return sizeof(struct sockaddr_in6);
#endif
#ifdef HAVE_IPX
	case NA_IPX:
#ifdef _WIN32
		((struct sockaddr_ipx *)s)->sa_family = AF_IPX;
		memcpy(((struct sockaddr_ipx *)s)->sa_netnum, &a->address.ipx[0], 4);
		memcpy(((struct sockaddr_ipx *)s)->sa_nodenum, &a->address.ipx[4], 6);
		((struct sockaddr_ipx *)s)->sa_socket = a->port;
#else
		((struct sockaddr_ipx *)s)->sipx_family = AF_IPX;
		memcpy(&((struct sockaddr_ipx *)s)->sipx_network, &a->address.ipx[0], 4);
		memcpy(((struct sockaddr_ipx *)s)->sipx_node, &a->address.ipx[4], 6);
		((struct sockaddr_ipx *)s)->sipx_port = a->port;
#endif
		return sizeof(struct sockaddr_ipx);
#endif
#ifdef UNIXSOCKETS
	case NA_UNIX:
		{
			struct sockaddr_un *un = (struct sockaddr_un*)s;
			un->sun_family = AF_UNIX;
			memcpy(un->sun_path, a->address.un.path, a->address.un.len);
			return offsetof(struct sockaddr_un, sun_path) + a->address.un.len;
		}
#endif
	default:
		Sys_Error("NetadrToSockadr: Bad type %i", a->type);
		return 0;
	}
}

void SockadrToNetadr (struct sockaddr_qstorage *s, int sizeofsockaddr, netadr_t *a)
{
	a->scopeid = 0;
	a->connum = 0;
	a->prot = NP_DGRAM;

	if (sizeofsockaddr < offsetof(struct sockaddr, sa_family)+sizeof(((struct sockaddr*)s)->sa_family))
	{	//truncated far too much...
		memset(a, 0, sizeof(*a));
		a->type = NA_INVALID;
		return;
	}

	switch (((struct sockaddr*)s)->sa_family)
	{
#ifdef HAVE_WEBSOCKCL
	case AF_WEBSOCK:
		a->type = NA_WEBSOCKET;
		memcpy(a->address.websocketurl, ((struct sockaddr_websocket*)s)->url, sizeof(a->address.websocketurl));
		a->port = 0;
		break;
#endif
#ifdef HAVE_IPV4
	case AF_INET:
		a->type = NA_IP;
		*(int *)&a->address.ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
		break;
#endif
#ifdef HAVE_IPV6
	case AF_INET6:
		a->type = NA_IPV6;
		memcpy(&a->address.ip6, &((struct sockaddr_in6 *)s)->sin6_addr, sizeof(a->address.ip6));
		a->port = ((struct sockaddr_in6 *)s)->sin6_port;
		a->scopeid = ((struct sockaddr_in6 *)s)->sin6_scope_id;
		break;
#endif
#ifdef HAVE_IPX
	case AF_IPX:
		a->type = NA_IPX;
		*(int *)a->address.ip = 0xffffffff;
#ifdef _WIN32
		memcpy(&a->address.ipx[0], ((struct sockaddr_ipx *)s)->sa_netnum, 4);
		memcpy(&a->address.ipx[4], ((struct sockaddr_ipx *)s)->sa_nodenum, 6);
		a->port = ((struct sockaddr_ipx *)s)->sa_socket;
#else
		memcpy(&a->address.ipx[0], &((struct sockaddr_ipx *)s)->sipx_network, 4);
		memcpy(&a->address.ipx[4], ((struct sockaddr_ipx *)s)->sipx_node, 6);
		a->port = ((struct sockaddr_ipx *)s)->sipx_port;
#endif
		break;
#endif
#ifdef UNIXSOCKETS
	case AF_UNIX:
		{
			struct sockaddr_un *un = (struct sockaddr_un*)s;
			a->type = NA_UNIX;
			a->address.un.len = sizeofsockaddr - offsetof(struct sockaddr_un, sun_path);
			memcpy(a->address.un.path, un->sun_path, a->address.un.len);
			if (a->address.un.len && a->address.un.path)
				a->address.un.len = strnlen(a->address.un.path, a->address.un.len);
			a->port = 0;
		}
		break;
#endif
	default:
		Con_Printf("SockadrToNetadr: bad socket family - %i", ((struct sockaddr*)s)->sa_family);
	case AF_UNSPEC:
		memset(a, 0, sizeof(*a));
		a->type = NA_INVALID;
		break;
	}
}
char	*NET_SockadrToString (char *s, int len, struct sockaddr_qstorage *a, size_t sizeofa)
{
	netadr_t na;
	SockadrToNetadr(a, sizeofa, &na);
	return NET_AdrToString(s, len, &na);
}

qboolean NET_AddrIsReliable(netadr_t *adr)	//hints that the protocol is reliable. if so, we don't need to wait for acks
{
	switch(adr->prot)
	{
	case NP_DGRAM:
	case NP_DTLS:
	case NP_NATPMP:
	default:
		return false;
	case NP_STREAM:
	case NP_TLS:
	case NP_WS:
	case NP_WSS:
		return true;
	}
}

qboolean	NET_CompareAdr (netadr_t *a, netadr_t *b)
{
	if (a->prot != b->prot)
		return false;

	if (a->type != b->type)
	{
		int i;
		if (a->type == NA_IP && b->type == NA_IPV6)
		{
			for (i = 0; i < 10; i++)
				if (b->address.ip6[i] != 0)
					return false;	//only matches if they're 0s, otherwise its not an ipv4 address there
			for (; i < 12; i++)
				if (b->address.ip6[i] != 0xff)// && b->address.ip6[i] != 0x00)	//0x00 is depricated
					return false;	//only matches if they're 0s or ffs, otherwise its not an ipv4 address there
			for (i = 0; i < 4; i++)
			{
				if (a->address.ip[i] != b->address.ip6[12+i])
					return false;	//mask doesn't match
			}
			return true;	//its an ipv4 address in there, the mask matched the whole way through
		}
		if (a->type == NA_IPV6 && b->type == NA_IP)
		{
			for (i = 0; i < 10; i++)
				if (a->address.ip6[i] != 0)
					return false;	//only matches if they're 0s, otherwise its not an ipv4 address there

			for (; i < 12; i++)
				if (a->address.ip6[i] != 0xff)// && a->address.ip6[i] != 0x00)	//0x00 is depricated
					return false;	//only matches if they're 0s or ffs, otherwise its not an ipv4 address there

			for (i = 0; i < 4; i++)
			{
				if (a->address.ip6[12+i] != b->address.ip[i])
					return false;	//mask doesn't match
			}
			return true;	//its an ipv4 address in there, the mask matched the whole way through
		}
		return false;
	}

	if (a->type == NA_LOOPBACK)
		return true;

#ifdef HAVE_WEBSOCKCL
	if (a->type == NA_WEBSOCKET)
	{
		if (!strcmp(a->address.websocketurl, a->address.websocketurl) && a->port == b->port)
			return true;
		return false;
	}
#endif

#ifdef HAVE_IPV4
	if (a->type == NA_IP)
	{
		if ((memcmp(a->address.ip, b->address.ip, sizeof(a->address.ip)) == 0) && a->port == b->port)
			return true;
		return false;
	}
#endif

#ifdef HAVE_IPV6
	if (a->type == NA_IPV6)
	{
		if ((memcmp(a->address.ip6, b->address.ip6, sizeof(a->address.ip6)) == 0) && a->port == b->port)
			return true;
		return false;
	}
#endif

#ifdef HAVE_IPX
	if (a->type == NA_IPX)
	{
		if ((memcmp(a->address.ipx, b->address.ipx, sizeof(a->address.ipx)) == 0) && a->port == b->port)
			return true;
		return false;
	}
#endif

#ifdef IRCCONNECT
	if (a->type == NA_IRC)
	{
		if (!strcmp(a->address.irc.user, b->address.irc.user))
			return true;
		return false;
	}
#endif

#ifdef UNIXSOCKETS
	if (a->type == NA_UNIX)
	{
		if (a->address.un.len == b->address.un.len && !memcmp(a->address.un.path, b->address.un.path, a->address.un.len))
			return true;
		return false;
	}
#endif

	Con_Printf("NET_CompareAdr: Bad address type\n");
	return false;
}

/*
===================
NET_CompareBaseAdr

Compares without the port
(udp/tcp/etc must still match)
===================
*/
qboolean	NET_CompareBaseAdr (netadr_t *a, netadr_t *b)
{
	if (a->prot != b->prot)
		return false;

	if (a->type != b->type)
		return false;

	if (a->type == NA_LOOPBACK)
		return true;

#ifdef HAVE_IPV4
	if (a->type == NA_IP)
	{
		if ((memcmp(a->address.ip, b->address.ip, sizeof(a->address.ip)) == 0))
			return true;
		return false;
	}
#endif
#ifdef HAVE_IPV6
	if (a->type == NA_IPV6)
	{
		if ((memcmp(a->address.ip6, b->address.ip6, 16) == 0))
			return true;
		return false;
	}
#endif
#ifdef HAVE_IPX
	if (a->type == NA_IPX)
	{
		if ((memcmp(a->address.ipx, b->address.ipx, 10) == 0))
			return true;
		return false;
	}
#endif
#ifdef IRCCONNECT
	if (a->type == NA_IRC)
	{
		if (!strcmp(a->address.irc.user, b->address.irc.user))
			return true;
		return false;
	}
#endif

#ifdef HAVE_WEBSOCKCL
	if (a->type == NA_WEBSOCKET)
	{
		if (!strcmp(a->address.websocketurl, b->address.websocketurl))
			return true;
		return false;
	}
#endif

#ifdef UNIXSOCKETS
	if (a->type == NA_UNIX)
	{
		if (a->address.un.len == b->address.un.len && !memcmp(a->address.un.path, b->address.un.path, a->address.un.len))
			return true;
		return false;
	}
#endif

	Sys_Error("NET_CompareBaseAdr: Bad address type");
	return false;
}

qboolean NET_AddressSmellsFunny(netadr_t *a)
{
#ifdef HAVE_IPV6
	int i;
#endif

	//rejects certain blacklisted addresses
	switch(a->type)
	{
#ifdef HAVE_IPV4
	case NA_IP:
		//reject localhost
		if (a->address.ip[0] == 127)// && a->address.ip[1] == 0   && a->address.ip[2] == 0   && a->address.ip[3] == 1  )
			return true;
		//'this' network (not an issue, but lets reject it anyway)
		if (a->address.ip[0] == 0   && a->address.ip[1] == 0   && a->address.ip[2] == 0   && a->address.ip[3] == 0  )
			return true;
		//reject any broadcasts
		if (a->address.ip[0] == 255 && a->address.ip[1] == 255 && a->address.ip[2] == 255 && a->address.ip[3] == 0  )
			return true;
		//not much else I can reject
		return false;
#endif

#ifdef HAVE_IPV6
	case NA_IPV6:
		//reject [::XXXX] (this includes obsolete ipv4-compatible (not ipv4 mapped), and localhost)
		for (i = 0; i < 12; i++)
			if (a->address.ip6[i])
				break;
		if (i == 12)
			return true;
		return false;
#endif

#ifdef HAVE_IPX
	//no idea how this protocol's addresses work
	case NA_IPX:
		return false;
#endif

	case NA_LOOPBACK:
		return false;

	default:
		return true;
	}
}

/*
static void NET_AdrToStringDoResolve(void *ctx, void *data, size_t a, size_t b)
{
	netadr_t *n = data;
	struct sockaddr_qstorage s;
	int ssz;
	char *adrstring = Z_Malloc(NI_MAXHOST);
	void (*resolved)(void *ctx, void *data, size_t a, size_t b) = *(void**)(n+1);
	if (n->type == NA_LOOPBACK)
		NET_BaseAdrToString(adrstring, NI_MAXHOST, n);
	else
	{
		ssz = NetadrToSockadr(n, &s);
		if (getnameinfo((struct sockaddr *)&s, ssz, adrstring, NI_MAXHOST, NULL, 0, NI_NUMERICSERV|NI_DGRAM))
		{
			NET_BaseAdrToString(adrstring, NI_MAXHOST, n);
		}
	}
	COM_AddWork(WG_MAIN, resolved, ctx, adrstring, a, b);
	Z_Free(n);
}

void NET_AdrToStringResolve (netadr_t *adr, void (*resolved)(void *ctx, void *data, size_t a, size_t b), void *ctx, size_t a, size_t b)
{
	netadr_t *n = Z_Malloc(sizeof(*n) + sizeof(void*));
	*n = *adr;
	*(void**)(n+1) = resolved;
	COM_AddWork(WG_LOADER, NET_AdrToStringDoResolve, ctx, n, a, b);
}
*/

char	*NET_AdrToString (char *s, int len, netadr_t *a)
{
	char *rs = s;
	char *prot = "";
#ifdef HAVE_IPV6
	int doneblank;
#endif

	switch(a->prot)
	{
	case NP_INVALID:prot = "invalid://";break;
	case NP_DGRAM:	prot = "";			break;
	case NP_DTLS:	prot = "dtls://";	break;
	case NP_STREAM:	prot = "tcp://";	break;	//not strictly true for ipx, but whatever.
	case NP_TLS:	prot = "tls://";	break;
	case NP_WS:		prot = "ws://";		break;
	case NP_WSS:	prot = "wss://";	break;
	case NP_NATPMP:	prot = "natpmp://";	break;
	}

	switch(a->type)
	{
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:	//ws / wss is part of the url
		{
			char *url = a->address.websocketurl;
			prot = "";
			if (a->prot == NP_DTLS && !strncmp(url, "ws://", 5))
			{
				url+=5;
				prot = "rtc://";
			}
			if (a->prot == NP_DTLS && !strncmp(url, "wss://", 6))
			{
				url+=6;
				prot = "rtcs://";
			}
			if (a->port)
				Q_snprintfz(s, len, "%s%s#%i", prot, url, ntohs(a->port));
			else
				Q_snprintfz(s, len, "%s%s", prot, url);
		}
		break;
#endif
#ifdef HAVE_IPV4
	case NA_IP:
		if (a->port)
		{
			Q_snprintfz(s, len, "%s%i.%i.%i.%i:%i",
				prot,
				a->address.ip[0],
				a->address.ip[1],
				a->address.ip[2],
				a->address.ip[3],
				ntohs(a->port));
		}
		else
		{
			snprintf (s, len, "%s%i.%i.%i.%i",
				prot,
				a->address.ip[0],
				a->address.ip[1],
				a->address.ip[2],
				a->address.ip[3]);
		}
		break;
#endif
#ifdef HAVE_IPV6
	case NA_IPV6:
		{
			char *p;
			int i;
			if (!*(int*)&a->address.ip6[0] &&
				!*(int*)&a->address.ip6[4] &&
				!*(short*)&a->address.ip6[8] &&
				*(short*)&a->address.ip6[10] == (short)0xffff)
			{
				if (a->port)
					snprintf (s, len, "%s%i.%i.%i.%i:%i",
						prot,
						a->address.ip6[12],
						a->address.ip6[13],
						a->address.ip6[14],
						a->address.ip6[15],
						ntohs(a->port));
				else
					snprintf (s, len, "%s%i.%i.%i.%i",
						prot,
						a->address.ip6[12],
						a->address.ip6[13],
						a->address.ip6[14],
						a->address.ip6[15]);
				break;
			}
			*s = 0;
			doneblank = false;
			p = s;
			if (a->port)
				snprintf (s, len-strlen(s), "%s[", prot);
			else
				snprintf (s, len-strlen(s), "%s", prot);
			p += strlen(p);

			for (i = 0; i < 16; i+=2)
			{
				if (doneblank!=true && a->address.ip6[i] == 0 && a->address.ip6[i+1] == 0)
				{
					if (!doneblank)
					{
						snprintf (p, len-strlen(s), "::");
						p += strlen(p);
						doneblank = 2;
					}
				}
				else
				{
					if (doneblank==2)
						doneblank = true;
					else if (i != 0)
					{
						snprintf (p, len-strlen(s), ":");
						p += strlen(p);
					}
					if (a->address.ip6[i+0])
					{
						snprintf (p, len-strlen(s), "%x%02x",
							a->address.ip6[i+0],
							a->address.ip6[i+1]);
					}
					else
					{
						snprintf (p, len-strlen(s), "%x",
							a->address.ip6[i+1]);
					}
					p += strlen(p);
				}
			}

			if (a->scopeid)
			{
				snprintf (p, len-strlen(s), "%%%u",
					a->scopeid);
				p += strlen(p);
			}

			if (a->port)
				snprintf (p, len-strlen(s), "]:%i",
					ntohs(a->port));
		}
		break;
#endif
#ifdef HAVE_IPX
	case NA_IPX:
		snprintf (s, len, "%s%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i",
			prot,
			a->address.ipx[0],
			a->address.ipx[1],
			a->address.ipx[2],
			a->address.ipx[3],
			a->address.ipx[4],
			a->address.ipx[5],
			a->address.ipx[6],
			a->address.ipx[7],
			a->address.ipx[8],
			a->address.ipx[9],
			ntohs(a->port));
		break;
#endif
	case NA_LOOPBACK:
		snprintf (s, len, "%sQLoopBack:%i", prot, a->port);
		break;

#ifdef IRCCONNECT
	case NA_IRC:
		if (*a->address.irc.channel)
			snprintf (s, len, "irc://%s@%s", a->address.irc.user, a->address.irc.channel);
		else
			snprintf (s, len, "irc://%s", a->address.irc.user);
		break;
#endif

#ifdef UNIXSOCKETS
	case NA_UNIX:
		switch(a->prot)
		{
		case NP_DGRAM:	prot = "udg://";	break;
		case NP_STREAM:	prot = "unix://";	break;
		default:
			snprintf (s, len, "unix+");
			len-=strlen(s);
			s+=strlen(s);
			break;
		}
		snprintf (s, len, prot);
		len-=strlen(s);
		s+=strlen(s);

		if (len)	//hopefully this will always be true...
		{
			char *end = a->address.un.path+a->address.un.len, *in;
			char c;
			for (in = a->address.un.path; in < end; in++)
			{
				if (--len == 0)
					break;
				if (*in == '\\')		//ugly encoding
					c = '\\';
				else if (*in == '\0')	//null chars are always a problem. abstract sockets generally get them displayed using @ chars.
				{
					*s++ = '@';
					continue;
				}
				else if (*in == '@')	//which means actual @ chars need to be escaped
					c = '@';
				//don't screw up from these, either.
				else if (*in == '\n')
					c = 'n';
				else if (*in == '\r')
					c = 'r';
				else if (*in == '\t')
					c = 't';
				//special quake chars can screw up display too
				else if (*in == '\1')
					c = '1';
				else if (*in == '\2')
					c = '2';
				else if (*in == '\3')
					c = '3';
				else
				{	//as-is.
					*s++ = *in;
					continue;
				}
				//marked up chars need extra storage.
				if (--len == 0)
					break;
				*s++ = '\\';
				*s++ = c;
			}
			*s = 0;	//and always null terminate the string.
		}
		break;
#endif

	default:
		snprintf (s, len, "invalid netadr_t type");
//		Sys_Error("NET_AdrToString: Bad netadr_t type");
	}

	return rs;
}

char	*NET_BaseAdrToString (char *s, int len, netadr_t *a)
{
	char *prot = "";
	switch(a->prot)
	{
	case NP_INVALID:prot = "invalid://";break;
	case NP_DGRAM:	prot = "";			break;
	case NP_DTLS:	prot = "dtls://";	break;
	case NP_STREAM:	prot = "tcp://";	break;	//not strictly true for ipx, but whatever.
	case NP_TLS:	prot = "tls://";	break;
	case NP_WS:		prot = "ws://";		break;
	case NP_WSS:	prot = "wss://";	break;
	case NP_NATPMP:	prot = "natpmp://";	break;
	}

	switch(a->type)
	{
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:	//ws / wss is part of the url
		{
			char *url = a->address.websocketurl;
			prot = "";
			if (a->prot == NP_DTLS && !strncmp(url, "ws://", 5))
			{
				url+=5;
				prot = "rtc://";
			}
			if (a->prot == NP_DTLS && !strncmp(url, "wss://", 6))
			{
				url+=6;
				prot = "rtcs://";
			}
			Q_snprintfz(s, len, "%s%s", prot, url);
		}
		break;
#endif

	case NA_IP:
		snprintf (s, len, "%s%i.%i.%i.%i",
			prot,
			a->address.ip[0],
			a->address.ip[1],
			a->address.ip[2],
			a->address.ip[3]);
		break;
#ifdef HAVE_IPV6
	case NA_IPV6:
		{
			char *p;
			int i, doneblank;
			if (!*(int*)&a->address.ip6[0] &&
				!*(int*)&a->address.ip6[4] &&
				!*(short*)&a->address.ip6[8] &&
				*(short*)&a->address.ip6[10] == (short)0xffff)
			{
				snprintf (s, len, "%s%i.%i.%i.%i",
					prot,
					a->address.ip6[12],
					a->address.ip6[13],
					a->address.ip6[14],
					a->address.ip6[15]);
				break;
			}
			*s = 0;
			doneblank = false;
			p = s;
			for (i = 0; i < 16; i+=2)
			{
				if (doneblank!=true && a->address.ip6[i] == 0 && a->address.ip6[i+1] == 0)
				{
					if (!doneblank)
					{
						snprintf (p, len-strlen(s), "::");
						p += strlen(p);
						doneblank = 2;
					}
				}
				else
				{
					if (doneblank==2)
						doneblank = true;
					else if (i != 0)
					{
						snprintf (p, len-strlen(s), ":");
						p += strlen(p);
					}
					if (a->address.ip6[i+0])
					{
						snprintf (p, len-strlen(s), "%x%02x",
							a->address.ip6[i+0],
							a->address.ip6[i+1]);
					}
					else
					{
						snprintf (p, len-strlen(s), "%x",
							a->address.ip6[i+1]);
					}
					p += strlen(p);
				}
			}
		}
		break;
#endif
#ifdef HAVE_IPX
	case NA_IPX:
		snprintf (s, len, "%s%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x",
			prot,
			a->address.ipx[0],
			a->address.ipx[1],
			a->address.ipx[2],
			a->address.ipx[3],
			a->address.ipx[4],
			a->address.ipx[5],
			a->address.ipx[6],
			a->address.ipx[7],
			a->address.ipx[8],
			a->address.ipx[9]);
		break;
#endif
	case NA_LOOPBACK:
		snprintf (s, len, "%sQLoopBack", prot);
		break;

#ifdef IRCCONNECT
	case NA_IRC:
		NET_AdrToString(s, len, a);
		break;
#endif

#ifdef UNIXSOCKETS
	case NA_UNIX:
		//no ports, so no base paths.
		return NET_AdrToString(s, len, a);
#endif

	default:
		Sys_Error("NET_BaseAdrToString: Bad netadr_t type");
	}

	return s;
}

/*
=============
NET_StringToAdr

idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
any form of ipv6, including port number.
=============
*/
size_t NET_StringToSockaddr2 (const char *s, int defaultport, netadrtype_t afhint, struct sockaddr_qstorage *sadr, int *addrfamily, int *addrsize, size_t addresses)
{
	size_t	result = 0;

	if (!(*s) || !addresses)
		return result;

	//EVIL HACK!
	//updates.tth uses a known self-signed certificate (to protect against dns hijacks like fteqw.com suffered).
	//its not meant to be used for browsers etc, and I cba to register dns stuff for it.
	//besides, browsers/etc would just bitch about its cert, so w/e.
	//redirect the dns to the base host without affecting http(s) hosts/certificates.
	if (!strcmp(s, "updates.triptohell.info"))
		s += 8;

	memset (sadr, 0, sizeof(*sadr));

#ifdef UNIXSOCKETS
	if (afhint == NA_UNIX)
	{
		struct sockaddr_un *sa = (struct sockaddr_un *)sadr;
		int i;

		//limit to known prefixes. this allows for sandboxing.
		const char *allowedprefixes[] = {"@"DISTRIBUTION, "/tmp/"DISTRIBUTION".", "/tmp/qsock.", "@FTE", "@qtv", "@qsock"};
		for (i = 0; i < countof(allowedprefixes); i++)
		{
			if (!Q_strncasecmp(s, allowedprefixes[i], strlen(allowedprefixes[i])))
				break;
		}
		if (i == countof(allowedprefixes))
		{
			Con_DPrintf(CON_WARNING "\"%s\" is not an accepted prefix for a unix socket. Forcing prefix.\n", s);
			i = 0;
			sa->sun_path[i++] = 0;
			sa->sun_path[i++] = 'q';
			sa->sun_path[i++] = 's';
			sa->sun_path[i++] = 'o';
			sa->sun_path[i++] = 'c';
			sa->sun_path[i++] = 'k';
		}
		else i = 0;

		sa->sun_family = AF_UNIX;

		//this parsing is so annoying because I want to support abstract sockets too, which have nulls.
		//we're using @ charsto represent nulls, to match 'lsof -U'
		for ( ; *s && i < countof(sa->sun_path); )
		{
			if (*s == '@')
			{
				sa->sun_path[i++] = 0;
				s++;
			}
			else if (*s == '\\')
			{
				if (s[1] == 0)
				{
					sa->sun_path[i++] = '\\';
					break;	//error.
				}
				else if (s[1] == '\\')
					sa->sun_path[i++] = '\\';
				else if (s[1] == '@')
					sa->sun_path[i++] = '@';
				else if (s[1] == 'n')
					sa->sun_path[i++] = '\n';
				else if (s[1] == 'r')
					sa->sun_path[i++] = '\r';
				else if (s[1] == 't')
					sa->sun_path[i++] = '\t';
				else if (s[1] == '1')
					sa->sun_path[i++] = '\1';
				else if (s[1] == '2')
					sa->sun_path[i++] = '\2';
				else if (s[1] == '3')
					sa->sun_path[i++] = '\3';
				else
					sa->sun_path[i++] = '?';
				s+=2;
			}
			else
				 sa->sun_path[i++] = *s++;
		}
		if (sa->sun_path[0])	//'pathname sockets should be null terminated'
			sa->sun_path[i++] = 0;
		if (i < countof(sa->sun_path))
			sa->sun_path[i] = 'X';
		if (addrsize)
			*addrsize = offsetof(struct sockaddr_un, sun_path) + i;
		if (addrfamily)
			*addrfamily = AF_UNIX;
		result++;
	}
	else
#endif
#ifdef HAVE_IPX
	if ((strlen(s) >= 23) && (s[8] == ':') && (s[21] == ':'))	// check for an IPX address
	{
		unsigned int val;
		char	copy[128];


		((struct sockaddr_ipx *)sadr)->sa_family = AF_IPX;

#define DO(src,dest)	\
	copy[0] = s[src];	\
	copy[1] = s[src + 1];	\
	sscanf (copy, "%x", &val);	\
	((struct sockaddr_ipx *)sadr)->dest = val

		copy[2] = 0;
		DO(0, sa_netnum[0]);
		DO(2, sa_netnum[1]);
		DO(4, sa_netnum[2]);
		DO(6, sa_netnum[3]);
		DO(9, sa_nodenum[0]);
		DO(11, sa_nodenum[1]);
		DO(13, sa_nodenum[2]);
		DO(15, sa_nodenum[3]);
		DO(17, sa_nodenum[4]);
		DO(19, sa_nodenum[5]);
		sscanf (&s[22], "%u", &val);

#undef DO

		((struct sockaddr_ipx *)sadr)->sa_socket = htons((unsigned short)val);
		if (addrfamily)
			*addrfamily = AF_IPX;
		if (addrsize)
			*addrsize = sizeof(struct sockaddr_ipx);
		result++;
	}
	else
#endif
#ifdef HAVE_IPV6
#ifdef pgetaddrinfo
	if (1)
#else
	if (pgetaddrinfo)
#endif
	{
		struct addrinfo *addrinfo = NULL;
		struct addrinfo *pos;
		struct addrinfo udp6hint;
		int error;
		char *port;
		char dupbase[256];
		int len;
		size_t i;
		double restime = Sys_DoubleTime();

		memset(&udp6hint, 0, sizeof(udp6hint));
		switch(afhint)
		{
#ifdef HAVE_IPV4
		case NA_IP:
			udp6hint.ai_family = AF_INET;
			break;
#endif
#ifdef HAVE_IPV6
		case NA_IPV6:
			udp6hint.ai_family = AF_INET6;
			break;
#endif
#ifdef HAVE_IPX
		case NA_IPX:
			udp6hint.ai_family = AF_IPX;
			break;
#endif
		default:
			udp6hint.ai_family = 0;//Any... we check for AF_INET6 or 4
			break;
		}
		udp6hint.ai_socktype = SOCK_DGRAM;
		udp6hint.ai_protocol = 0;

		if (*s == '[')
		{
			port = strstr(s, "]");
			if (!port)
				error = EAI_NONAME;
			else
			{
				len = port - (s+1);
				if (len >= sizeof(dupbase))
					len = sizeof(dupbase)-1;
				strncpy(dupbase, s+1, len);
				dupbase[len] = '\0';
				error = pgetaddrinfo(dupbase, (port[1] == ':')?port+2:NULL, &udp6hint, &addrinfo);
			}
		}
		else
		{
#if defined(AI_ADDRCONFIG) && !defined(_WIN32)
			udp6hint.ai_flags |= AI_ADDRCONFIG;	//don't return ipv6 if we can't send to ipv6 hosts
#endif

			port = strrchr(s, ':');

			if (port)
			{
				len = port - s;
				if (len >= sizeof(dupbase))
					len = sizeof(dupbase)-1;
				strncpy(dupbase, s, len);
				dupbase[len] = '\0';
				error = pgetaddrinfo(dupbase, port+1, &udp6hint, &addrinfo);
			}
			else
				error = EAI_NONAME;
			if (error)	//failed, try string with no port.
				error = pgetaddrinfo(s, NULL, &udp6hint, &addrinfo);	//remember, this func will return any address family that could be using the udp protocol... (ip4 or ip6)
		}

		restime = Sys_DoubleTime()-restime;
		if (restime > 0.5)
		{	//adding this in an attempt to debug somewhat periodic stalls that I'm being told about.
			Con_DPrintf("DNS resolution of %s %s %f seconds (on %s thread)\n", s, error?"failed after":"took", restime, Sys_IsMainThread()?"main":"worker");
		}

		if (error)
		{
			return false;
		}
		((struct sockaddr*)sadr)->sa_family = 0;
		for (pos = addrinfo; pos; pos = pos->ai_next)
		{
			switch(pos->ai_family)
			{
			case AF_INET6:
				if (!net_dns_ipv6.ival)
					continue;
				if (result < addresses)
					memcpy(&sadr[result++], pos->ai_addr, pos->ai_addrlen);
				break;
#ifdef HAVE_IPV4
			case AF_INET:
				if (!net_dns_ipv4.ival)
					continue;
				//ipv4 addresses have a higher priority than ipv6 ones (too few other quake engines support ipv6).
				if (result && ((struct sockaddr_in *)&sadr[0])->sin_family == AF_INET6)
				{
					if (result < addresses)
						memcpy(&sadr[result++], &sadr[0], sizeof(sadr[0]));
					memcpy(&sadr[0], pos->ai_addr, pos->ai_addrlen);
				}
				else if (result < addresses)
					memcpy(&sadr[result++], pos->ai_addr, pos->ai_addrlen);
				break;
#endif
			}
		}
		pfreeaddrinfo (addrinfo);

		for (i = 0; i < result; i++)
		{
			if (addrfamily)
				addrfamily[i] = ((struct sockaddr*)sadr)->sa_family;

			if (((struct sockaddr*)&sadr[i])->sa_family == AF_INET)
			{
				if (!((struct sockaddr_in *)&sadr[i])->sin_port)
					((struct sockaddr_in *)&sadr[i])->sin_port = htons(defaultport);
				if (addrsize)
					addrsize[i] = sizeof(struct sockaddr_in);
			}
			else if (((struct sockaddr*)&sadr[i])->sa_family == AF_INET6)
			{
				if (!((struct sockaddr_in6 *)&sadr[i])->sin6_port)
					((struct sockaddr_in6 *)&sadr[i])->sin6_port = htons(defaultport);
				if (addrsize)
					addrsize[i] = sizeof(struct sockaddr_in6);
			}
		}
	}
	else
#endif
	{
#if defined(HAVE_IPV4) && !defined(pgetaddrinfo) && !defined(HAVE_IPV6)
		char	copy[128];
		char	*colon;

		((struct sockaddr_in *)sadr)->sin_family = AF_INET;

		((struct sockaddr_in *)sadr)->sin_port = 0;

		if (strlen(s) >= sizeof(copy)-1)
			return false;
		if (!net_dns_ipv4.ival)
			return false;

		((struct sockaddr_in *)sadr)->sin_port = htons(defaultport);

		strcpy (copy, s);
		// strip off a trailing :port if present
		for (colon = copy ; *colon ; colon++)
			if (*colon == ':')
			{
				*colon = 0;
				((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));
			}

		if (copy[0] >= '0' && copy[0] <= '9')	//this is the wrong way to test. a server name may start with a number.
		{
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
		}
		else
		{
			struct hostent	*h;
			if (! (h = gethostbyname(copy)) )
				return false;
			if (h->h_addrtype != AF_INET)
				return false;
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
		}
		if (addrfamily)
			*addrfamily = AF_INET;
		if (addrsize)
			*addrsize = sizeof(struct sockaddr_in);
		result++;
#endif
	}

	return result;
}

/*
accepts anything that NET_StringToSockaddr accepts plus certain url schemes
including: tcp, irc
*/
size_t	NET_StringToAdr2 (const char *s, int defaultport, netadr_t *a, size_t numaddresses)
{
	size_t result = 0, i;
	struct sockaddr_qstorage sadr[8];
	int asize[countof(sadr)];
	netproto_t prot;
	netadrtype_t afhint;

	struct
	{
		const char *name;
		netproto_t prot;
		netadrtype_t family;
	} schemes[] =
	{
		{"udp://", NP_DGRAM, NA_INVALID},	//placeholder for dgram rather than an actual family.
		{"udp4//", NP_DGRAM, NA_IP},
		{"udp6//", NP_DGRAM, NA_IPV6},
		{"ipx://", NP_DGRAM, NA_IPX},

		//compat with qtv. we don't have any way to exclude specific protocols though.
		{"qw://", NP_DGRAM, NA_INVALID},
		{"nq://", NP_DGRAM, NA_INVALID},
		{"dp://", NP_DGRAM, NA_INVALID},
		{"q2://", NP_DGRAM, NA_INVALID},
		{"q3://", NP_DGRAM, NA_INVALID},

		{"tcp://", NP_STREAM_OR_INVALID, NA_INVALID},	//placeholder for dgram rather than an actual family.
		{"tcp4//", NP_STREAM_OR_INVALID, NA_IP},
		{"tcp6//", NP_STREAM_OR_INVALID, NA_IPV6},
		{"spx://", NP_STREAM_OR_INVALID, NA_IPX},

		{"ws://", NP_WS_OR_INVALID, NA_INVALID},
		{"wss://", NP_WSS_OR_INVALID, NA_INVALID},

		{"tls://", NP_TLS_OR_INVALID, NA_INVALID},
		{"dtls://", NP_DTLS_OR_INVALID, NA_INVALID},

		{"irc://", NP_INVALID, NA_INVALID},	//should have been handled explicitly, if supported.

#ifdef UNIXSOCKETS
		{"udg://", NP_DGRAM, NA_UNIX},
		{"unix://", NP_STREAM_OR_INVALID, NA_UNIX},
#endif
	};


	memset(a, 0, sizeof(*a)*numaddresses);

	if (!numaddresses)
		return false;

	if (!strcmp (s, "internalserver"))
	{
		a->type = NA_LOOPBACK;
		return true;
	}

	if (!strncmp(s, "QLoopBack", 9))
	{
		a->type = NA_LOOPBACK;
		if (s[9] == ':')
			a->port = atoi(s+10);
		else
			a->port = defaultport;
		return true;
	}

	if (!net_enabled.ival)
		return false;
	Con_DPrintf("Resolving address: %s\n", s);

#ifdef HAVE_WEBSOCKCL
	//with websockets we can't really resolve anything. failure happens only when trying to connect.
	//`connect /GAMENAME` is equivelent to `connect rtc://broker/GAMENAME`
	if (!strncmp (s, "/", 1))
	{
		char *prefix = "";
		if (!fs_manifest->rtcbroker || !*fs_manifest->rtcbroker)
		{	//FIXME: use referrer? or the website's host?
			Con_DPrintf("No default rtc broker\n");
			return 0;	//can't accept it
		}
		if (!strstr(fs_manifest->rtcbroker, "://"))
			prefix = "ws://";
		Q_snprintfz(a->address.websocketurl, sizeof(a->address.websocketurl), "%s%s%s", prefix, fs_manifest->rtcbroker, s);
		return 1;
	}
	else if (!strncmp (s, "rtc://", 6) || !strncmp (s, "rtcs://", 7))
	{	//basically ICE using sdp-via-websockets to a named relay server.
		const char *prot, *host, *path;
		a->type = NA_WEBSOCKET;
		a->prot = NP_DTLS;
		if (!strncmp (s, "rtcs://", 7))
		{
			prot = "wss://";
			path = s+7;
		}
		else
		{
			prot = "ws://";
			path = s+6;
		}
		if (*path == '/')
		{
			host = fs_manifest->rtcbroker;
			if (!host || !*host)	//can't guess the host
				return 0;
			if (strstr(host, "://"))
				prot = "";
		}
		else
			host = "";
		Q_snprintfz(a->address.websocketurl, sizeof(a->address.websocketurl), "%s%s%s", prot, host, path);
		return 1;
	}
	else if (!strncmp (s, "ws://", 5) || !strncmp (s, "wss://", 6))
	{
		a->type = NA_WEBSOCKET;
		if (!strncmp (s, "wss://", 6))
			a->prot = NP_WSS;
		else
			a->prot = NP_WS;
		Q_strncpyz(a->address.websocketurl, s, sizeof(a->address.websocketurl));
		return 1;
	}
	else
	{
		/*code for convienience - no other protocols work anyway*/
		static float warned;
		if (warned < realtime)
		{
			Con_DPrintf("Note: Assuming ws:// prefix\n");
			warned = realtime + 1;
		}
		a->type = NA_WEBSOCKET;
		a->prot = NP_WS;
		memcpy(a->address.websocketurl, "ws://", 5);
		Q_strncpyz(a->address.websocketurl+5, s, sizeof(a->address.websocketurl)-5);
		return 1;
	}
#endif

	if (!strncmp (s, "irc://", 6))
	{
#ifdef IRCCONNECT
		char *at;
		char *slash;
		memset (a, 0, sizeof(*a));
		a->type = NA_IRC;

		s+=6;
		slash = strchr(s, '/');
		if (!slash)
			return false;
		if (slash - s+1 >= sizeof(a->address.irc.host))
			return false;
		memcpy(a->address.irc.host, s, slash - s);
		a->address.irc.host[slash - s] = 0;
		s = slash+1;
		at = strchr(s, '@');
		if (at)
		{
			if (at-s+1 >= sizeof(a->address.irc.user))
				return false;
			Q_strncpyz(a->address.irc.user, s, at-s+1);
			Q_strncpyz(a->address.irc.channel, at+1, sizeof(a->address.irc.channel));
		}
		else
		{
			//just a user.
			Q_strncpyz(a->address.irc.user, s, sizeof(a->address.irc.user));
		}
		return 1;
#else
		return 0;
#endif
	}
#ifdef HAVE_NATPMP
	if (!strncmp (s, "natpmp://", 9))
	{	//our natpmp thing omits the host part. FIXME: host should be the NAT that we're sending to
		NET_PortToAdr(NA_IP, NP_NATPMP, s+9, a);
		if (a->prot != NP_NATPMP)
			return false;
		return true;
	}
#endif

	for (prot = NP_DGRAM, afhint = NA_INVALID/*any*/, i = 0; i < countof(schemes); i++)
	{
		if (!strncmp(s, schemes[i].name, strlen(schemes[i].name)))
		{
			s += strlen(schemes[i].name);
			prot = schemes[i].prot;
			afhint = schemes[i].family;
			break;
		}
	}

	result = NET_StringToSockaddr2 (s, defaultport, afhint, sadr, NULL, asize, min(numaddresses, countof(sadr)));
	for (i = 0; i < result; i++)
	{
		SockadrToNetadr (&sadr[i], asize[i], &a[i]);
		a[i].prot = prot;
	}

	//invalidate any others
	for (; i < numaddresses; i++)
		a[i].type = NA_INVALID;

	return result;
}

// NET_IntegerToMask: given a source address pointer, a mask address pointer, and
// desired number of bits, fills the mask pointer with given bits
// (bits < 0 will always fill all bits)
void NET_IntegerToMask (netadr_t *a, netadr_t *amask, int bits)
{
	unsigned int i;
	qbyte *n;

	memset (amask, 0, sizeof(*amask));
	amask->type = a->type;

	if (bits < 0)
		i = 8000; // fill all bits
	else
		i = bits;

	switch (amask->type)
	{
	case NA_INVALID:
		break;
	case NA_IP:
		n = amask->address.ip;
		if (i > 32)
			i = 32;
		for (; i >= 8; i -= 8)
		{
			*n = 0xFF;
			n++;
		}

		// fill last bit
		if (i)
		{
			i = 8 - i;
			i = 255 - ((1 << i) - 1);
			*n = i;
		}
		break;
	case NA_IPV6:
#ifdef HAVE_IPV6
		n = amask->address.ip6;
		if (i > 128)
			i = 128;
		for (; i >= 8; i -= 8)
		{
			*n = 0xFF;
			n++;
		}

		// fill last bit
		if (i)
		{
			i = 8 - i;
			i = 255 - ((1 << i) - 1);
			*n = i;
		}
#endif
		break;
	case NA_IPX:
#ifdef HAVE_IPX
		n = amask->address.ipx;
		if (i > 80)
			i = 80;
		for (; i >= 8; i -= 8)
		{
			*n = 0xFF;
			n++;
		}

		// fill last bit
		if (i)
		{
			i = 8 - i;
			i = 255 - ((1 << i) - 1);
			*n = i;
		}
#endif
		break;
	case NA_LOOPBACK:
		break;
#ifdef UNIXSOCKETS
	case NA_UNIX:	//address masks/filtering don't make sense.
#endif
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:
#endif
#ifdef IRCCONNECT
	case NA_IRC:
#endif
		break;

	}
}

// ParsePartialIP: check string to see if it is a partial IP address and
// return bits to mask and set netadr_t or 0 if not an address
int ParsePartialIP(const char *s, netadr_t *a)
{
	char *colon;
	int bits;

	if (!*s)
		return 0;

	memset (a, 0, sizeof(*a));

	//multiple colons == ipv6
	colon = strchr(s, ':');
	if (colon && strchr(colon+1, ':'))
	{
		qbyte *address = a->address.ip6;
		unsigned long tmp;
		bits = 0;
		//FIXME: check for ::ffff:a.b.c.d
		//FIXME: check for xx::xx

		if (s[0] == ':' && s[1] == ':' && !s[2])
		{
			s+=2;
			bits = 1;
		}
		else while(*s)
		{
			tmp = strtoul(s, &colon, 16);
			if (tmp > 0xffff)
				return 0;	//invalid
			*address++ = (tmp>>8)&0xff;
			*address++ = (tmp>>0)&0xff;
			bits += 16;

			if (bits == 128)
			{
				if (!*colon)
					break;
				return 0;	//must have ended here
			}


			//double-colon ends it here. we can't parse xx::xx
			//hopefully the last 64 bits or whatever will be irrelevant anyway, so such addresses won't be common
			if (colon[0] == ':' && colon[1] == ':' && !colon[2])
				break;
			if (*colon == ':')
				colon++;
			else
				return 0; //don't allow it if it just ended without a double-colon.
			s = colon;
		}
		a->type = NA_IPV6;
		a->port = 0;
	}
	else
	{
		char *address = a->address.ip;
		int port = 0;
		bits = 8;
		while (*s)
		{
			if (*s == ':')
			{
				port = strtoul(s+1, &address, 10);
				if (*address)	//if there was something other than a number there, give up now
					return 0;
				break;	//end-of-string
			}
			else if (*s == '.')
			{
				if (bits >= 32) // only 32 bits in ipv4
					return 0;
				else if (*(s+1) == '.')
					return 0;
				else if (*(s+1) == '\0')
					break; // don't add more bits to the mask for x.x., etc
				address++;

				//many nq servers mask addresses with Xs.
				if (s[1] == 'x' || s[1] == 'X')
				{
					s++;
					while (*s == 'x' || *s == 'X' || *s == '.')
						s++;
					if (*s)
						return 0;
					break;
				}
				bits += 8;
			}
			else if (*s >= '0' && *s <= '9')
				*address = ((*address)*10) + (*s-'0');
			else
				return 0; // invalid character

			s++;
		}
		a->type = NA_IP;
		a->port = port;
	}

	return bits;
}

// NET_StringToAdrMasked: extension to NET_StringToAdr to handle IP addresses
// with masks or integers representing the bit masks
qboolean NET_StringToAdrMasked (const char *s, qboolean allowdns, netadr_t *a, netadr_t *amask)
{
	char t[64];
	char *spoint;
	int i;

	spoint = strchr(s, '/');

	if (spoint)
	{
		// we have a slash in the address so split and resolve separately
		char *c;

		i = (int)(spoint - s) + 1;
		if (i > sizeof(t))
			i = sizeof(t);

		Q_strncpyz(t, s, i);
		if (!ParsePartialIP(t, a) && (!allowdns || !NET_StringToAdr(t, 0, a)))
			return false;
		spoint++;

		c = spoint;
		if (!*c)
			return false;

		while (*c) // check for non-numeric characters
		{
			if (*c < '0' || *c > '9')
			{
				c = NULL;
				break;
			}
			c++;
		}

		if (c == NULL) // we have an address so resolve it and return
			return ParsePartialIP(spoint, amask) || (allowdns && NET_StringToAdr(spoint, 0, amask));

		// otherwise generate mask for given bits
		i = atoi(spoint);
		NET_IntegerToMask(a, amask, i);
	}
	else
	{
		// we don't have a slash, resolve and fill with a full mask
		i = ParsePartialIP(s, a);
		if (!i && (!allowdns || !NET_StringToAdr(s, 0, a)))
			return false;

		memset (amask, 0, sizeof(*amask));
		amask->type = a->type;

		if (i)
			NET_IntegerToMask(a, amask, i);
		else
			NET_IntegerToMask(a, amask, -1);
	}

	return true;
}

// NET_CompareAdrMasked: given 3 addresses, 2 to compare with a complimentary mask,
// returns true or false if they match
//WARNING: a is typically an ipv6 address, even if its an ipv4-mapped address.
//so ipv4ify first.
//this is not intended to identify any specific connection, so we can ignore udp/tcp distinctions (especially as this is usually used for bans).
qboolean NET_CompareAdrMasked(netadr_t *a, netadr_t *b, netadr_t *mask)
{
	int i;

	//make sure the address being checked against matches the mask
	if (b->type != mask->type)
		return false;

	// check port if both are non-zero
	if (a->port && b->port && a->port != b->port)
		return false;

	// check to make sure all types match
	if (a->type != b->type)
	{
		if (a->type == NA_IP && b->type == NA_IPV6 && mask->type == NA_IP)
		{
			for (i = 0; i < 10; i++)
				if (b->address.ip6[i] != 0)
					return false;	//only matches if they're 0s, otherwise its not an ipv4 address there
			for (; i < 12; i++)
				if (b->address.ip6[i] != 0xff)// && b->address.ip6[i] != 0x00)	//0x00 is depricated
					return false;	//only matches if they're 0s or ffs, otherwise its not an ipv4 address there
			for (i = 0; i < 4; i++)
			{
				if ((a->address.ip[i] & mask->address.ip[i]) != (b->address.ip6[12+i] & mask->address.ip[i]))
					return false;	//mask doesn't match
			}
			return true;	//its an ipv4 address in there, the mask matched the whole way through
		}
		if (a->type == NA_IPV6 && b->type == NA_IP && mask->type == NA_IP)
		{
			for (i = 0; i < 10; i++)
				if (a->address.ip6[i] != 0)
					return false;	//only matches if they're 0s, otherwise its not an ipv4 address there

			for (; i < 12; i++)
				if (a->address.ip6[i] != 0xff)// && a->address.ip6[i] != 0x00)	//0x00 is depricated
					return false;	//only matches if they're 0s or ffs, otherwise its not an ipv4 address there

			for (i = 0; i < 4; i++)
			{
				if ((a->address.ip6[12+i] & mask->address.ip[i]) != (b->address.ip[i] & mask->address.ip[i]))
					return false;	//mask doesn't match
			}
			return true;	//its an ipv4 address in there, the mask matched the whole way through
		}
		return false;
	}

	// match on protocol type and compare address
	switch (a->type)
	{
	case NA_LOOPBACK:
		return true;
	case NA_IP:
		for (i = 0; i < 4; i++)
		{
			if ((a->address.ip[i] & mask->address.ip[i]) != (b->address.ip[i] & mask->address.ip[i]))
				return false;
		}
		break;
#ifdef HAVE_IPV6
	case NA_IPV6:
		for (i = 0; i < 16; i++)
		{
			if ((a->address.ip6[i] & mask->address.ip6[i]) != (b->address.ip6[i] & mask->address.ip6[i]))
				return false;
		}
		break;
#endif
#ifdef HAVE_IPX
	case NA_IPX:
		for (i = 0; i < 10; i++)
		{
			if ((a->address.ipx[i] & mask->address.ipx[i]) != (b->address.ipx[i] & mask->address.ipx[i]))
				return false;
		}
		break;
#endif

#ifdef IRCCONNECT
	case NA_IRC:
		//masks are not supported, match explicitly
		if (strcmp(a->address.irc.user, b->address.irc.user))
			return false;
		break;
#endif
	default:
		return false; // invalid protocol
	}

	return true; // all checks passed
}

// UniformMaskedBits: counts number of bits in an assumed uniform mask, returns
// -1 if not uniform
int UniformMaskedBits(netadr_t *mask)
{
	int bits;
	int b;
	unsigned int bs;
	qboolean bitenc = false;

	switch (mask->type)
	{
	case NA_IP:
		bits = 32;
		for (b = 3; b >= 0; b--)
		{
			if (mask->address.ip[b] == 0xFF)
				bitenc = true;
			else if (mask->address.ip[b])
			{
				bs = (~mask->address.ip[b]) & 0xFF;
				while (bs)
				{
					if (bs & 1)
					{
						bits -= 1;
						if (bitenc)
							return -1;
					}
					else
						bitenc = true;
					bs >>= 1;
				}
			}
			else if (bitenc)
				return -1;
			else
				bits -= 8;
		}
		break;
#ifdef HAVE_IPV6
	case NA_IPV6:
		bits = 128;
		for (b = 15; b >= 0; b--)
		{
			if (mask->address.ip6[b] == 0xFF)
				bitenc = true;
			else if (mask->address.ip6[b])
			{
				bs = (~mask->address.ip6[b]) & 0xFF;
				while (bs)
				{
					if (bs & 1)
					{
						bits -= 1;
						if (bitenc)
							return -1;
					}
					else
						bitenc = true;
					bs >>= 1;
				}
			}
			else if (bitenc)
				return -1;
			else
				bits -= 8;
		}
		break;
#endif
#ifdef HAVE_IPX
	case NA_IPX:
		bits = 80;
		for (b = 9; b >= 0; b--)
		{
			if (mask->address.ipx[b] == 0xFF)
				bitenc = true;
			else if (mask->address.ipx[b])
			{
				bs = (~mask->address.ipx[b]) & 0xFF;
				while (bs)
				{
					if (bs & 1)
					{
						bits -= 1;
						if (bitenc)
							return -1;
					}
					else
						bitenc = true;
					bs >>= 1;
				}
			}
			else if (bitenc)
				return -1;
			else
				bits -= 8;
		}
		break;
#endif
	default:
		return -1; // invalid protocol
	}

	return bits; // all checks passed
}

char *NET_AdrToStringMasked (char *s, int len, netadr_t *a, netadr_t *amask)
{
	int i;
	char adr[MAX_ADR_SIZE], mask[MAX_ADR_SIZE];

	i = UniformMaskedBits(amask);

	if (i >= 0)
		snprintf(s, len, "%s/%i", NET_AdrToString(adr, sizeof(adr), a), i);
	else
		snprintf(s, len, "%s/%s", NET_AdrToString(adr, sizeof(adr), a), NET_AdrToString(mask, sizeof(mask), amask));

	return s;
}

// Returns true if we can't bind the address locally--in other words,
// the IP is NOT one of our interfaces.
qboolean NET_IsClientLegal(netadr_t *adr)
{
#if 0
	struct sockaddr_in sadr;
	int newsocket;

	if (adr->ip[0] == 127)
		return false; // no local connections period

	NetadrToSockadr (adr, &sadr);

	if ((newsocket = socket (PF_INET, SOCK_CLOEXEC|SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		Sys_Error ("NET_IsClientLegal: socket:", strerror(qerrno));

	sadr.sin_port = 0;

	if( bind (newsocket, (void *)&sadr, sizeof(sadr)) == -1)
	{
		// It is not a local address
		close(newsocket);
		return true;
	}
	close(newsocket);
	return false;
#else
	return true;
#endif
}

qboolean	NET_IsLoopBackAddress (netadr_t *adr)
{
//	return (!strcmp(cls.servername, NET_AdrToString(net_local_adr)) || !strcmp(cls.servername, "local");
	return adr->type == NA_LOOPBACK;
}


#ifdef HAVE_SSL
vfsfile_t *FS_OpenSSL(const char *hostname, vfsfile_t *source, qboolean isserver)
{
	vfsfile_t *f = NULL;
#ifdef HAVE_GNUTLS
	if (!f)
		f = GNUTLS_OpenVFS(hostname, source, isserver);
#endif
#ifdef HAVE_OPENSSL
	if (!f)
		f = OSSL_OpenVFS(hostname, source, isserver);
#endif
#ifdef HAVE_WINSSPI
	if (!f)
		f = SSPI_OpenVFS(hostname, source, isserver);
#endif
	return f;
}
int TLS_GetChannelBinding(vfsfile_t *stream, qbyte *data, size_t *datasize)
{
	int r = -1;
#ifdef HAVE_GNUTLS
	if (r == -1)
		r = GNUTLS_GetChannelBinding(stream, data, datasize);
#endif
#ifdef HAVE_OPENSSL
	if (r == -1)
		r = OSSL_GetChannelBinding(stream, data, datasize);
#endif
#ifdef HAVE_WINSSPI
	if (r == -1)
		r = SSPI_GetChannelBinding(stream, data, datasize);
#endif
	return r;
}
#endif

/////////////////////////////////////////////
//loopback stuff

#if defined(HAVE_SERVER) && defined(HAVE_CLIENT)

qboolean	NET_GetLoopPacket (int sock, netadr_t *from, sizebuf_t *message)
{
	int		i;
	loopback_t	*loop;

	sock &= 1;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
	{
		extern cvar_t showdrop;
		if (showdrop.ival)
			Con_Printf("loopback dropping %i packets\n", (loop->send - MAX_LOOPBACK) - loop->get);
		loop->get = loop->send - MAX_LOOPBACK;
	}

	if (loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	if (message->maxsize < loop->msgs[i].datalen)
		Sys_Error("NET_SendLoopPacket: Loopback buffer was too big");

	memcpy (message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	message->cursize = loop->msgs[i].datalen;
	memset (from, 0, sizeof(*from));
	from->type = NA_LOOPBACK;
	message->packing = SZ_RAWBYTES;
	message->currentbit = 0;
	loop->msgs[i].datalen = 0;
	return true;

}


neterr_t NET_SendLoopPacket (int sock, int length, const void *data, netadr_t *to)
{
	int		i;
	loopback_t	*loop;

	sock &= 1;

	loop = &loopbacks[sock^1];
	if (!loop->inited)
		return NETERR_NOROUTE;

	i = loop->send & (MAX_LOOPBACK-1);
	if (length > loop->msgs[i].datamax)
	{
		loop->msgs[i].datamax = length + 1024;
		BZ_Free(loop->msgs[i].data);
		loop->msgs[i].data = BZ_Malloc(loop->msgs[i].datamax);
	}
	if (loop->msgs[i].datalen)
		Con_Printf("Warning: loopback queue overflow\n");

	loop->send++;

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
	return NETERR_SENT;
}

int FTENET_Loop_GetLocalAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
	if (maxaddresses)
	{
		addresses->type = NA_LOOPBACK;
		addresses->port = con->thesocket+1;
		*adrflags = 0;
		return 1;
	}
	return 0;
}

qboolean FTENET_Loop_GetPacket(ftenet_generic_connection_t *con)
{
	return NET_GetLoopPacket(con->thesocket, &net_from, &net_message);
}

#ifdef HAVE_PACKET
//just a null function so we don't pass bad things to select.
int FTENET_Loop_SetFDSets(ftenet_generic_connection_t *gcon, fd_set *readfdset, fd_set *writefdset)
{
	return 0;
}
#endif

neterr_t FTENET_Loop_SendPacket(ftenet_generic_connection_t *con, int length, const void *data, netadr_t *to)
{
	if (to->type == NA_LOOPBACK)
	{
		return NET_SendLoopPacket(con->thesocket, length, data, to);
	}

	return NETERR_NOROUTE;
}

void FTENET_Loop_Close(ftenet_generic_connection_t *con)
{
	int i;
	int sock = con->thesocket;
	sock &= 1;
	loopbacks[sock].inited = false;
	loopbacks[sock].get = loopbacks[sock].send = 0;
	for (i = 0; i < MAX_LOOPBACK; i++)
	{
		BZ_Free(loopbacks[sock].msgs[i].data);
		loopbacks[sock].msgs[i].data = NULL;
		loopbacks[sock].msgs[i].datalen = 0;
		loopbacks[sock].msgs[i].datamax = 0;
	}
	Z_Free(con);
}

static ftenet_generic_connection_t *FTENET_Loop_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	ftenet_generic_connection_t *newcon;
	int sock;
	for (sock = 0; sock < countof(loopbacks); sock++)
		if (!loopbacks[sock].inited)
			break;
	if (sock == countof(loopbacks))
		return NULL;
	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		loopbacks[sock].inited = true;
		loopbacks[sock].get = loopbacks[sock].send = 0;

		newcon->GetLocalAddresses = FTENET_Loop_GetLocalAddresses;
		newcon->GetPacket = FTENET_Loop_GetPacket;
		newcon->SendPacket = FTENET_Loop_SendPacket;
		newcon->Close = FTENET_Loop_Close;
#ifdef HAVE_PACKET
		newcon->SetFDSets = FTENET_Loop_SetFDSets;
#endif

		newcon->islisten = isserver;
		newcon->addrtype[0] = NA_LOOPBACK;
		newcon->addrtype[1] = NA_INVALID;

		newcon->thesocket = sock;
	}
	return newcon;
}
#endif
//=============================================================================

ftenet_connections_t *FTENET_CreateCollection(qboolean listen)
{
	ftenet_connections_t *col;
	col = Z_Malloc(sizeof(*col));
	col->islisten = listen;
	return col;
}
#if defined(HAVE_CLIENT) && defined(HAVE_SERVER)
static ftenet_generic_connection_t *FTENET_Loop_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef HAVE_PACKET
static ftenet_generic_connection_t *FTENET_Datagram_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef TCPCONNECT
static ftenet_generic_connection_t *FTENET_TCPConnect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef HAVE_WEBSOCKCL
static ftenet_generic_connection_t *FTENET_WebSocket_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef IRCCONNECT
static ftenet_generic_connection_t *FTENET_IRCConnect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef HAVE_NATPMP
static ftenet_generic_connection_t *FTENET_NATPMP_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif

#ifdef HAVE_NATPMP
typedef struct
{
	ftenet_generic_connection_t pub;
	ftenet_connections_t *col;
	netadr_t reqpmpaddr;
	netadr_t pmpaddr;
	netadr_t natadr;
	unsigned int refreshtime;
} pmpcon_t;

int FTENET_NATPMP_GetLocalAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, int maxaddresses);
static qboolean NET_Was_NATPMP(ftenet_connections_t *collection)
{
	pmpcon_t *pmp;
	struct
	{
		qbyte ver; qbyte op; short resultcode;
		int age;
		union
		{
			struct
			{
				short privport; short pubport;
				int mapping_expectancy;
			};
			qbyte ipv4[4];
		};
	} *pmpreqrep;
	int i;

	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			continue;
		if (collection->conn[i]->GetLocalAddresses == FTENET_NATPMP_GetLocalAddresses)
		{
			pmp = (pmpcon_t*)collection->conn[i];
			if (NET_CompareAdr(&pmp->pmpaddr, &net_from))
			{
				pmpreqrep = (void*)net_message.data;

				if (pmpreqrep->ver != 0)
					return false;
				if (net_message.cursize == 12 && pmpreqrep->op == 128)
				{
					char adrbuf[256];
					pmp->natadr.type = NA_IP;
					pmp->natadr.port = 0;
					memcpy(pmp->natadr.address.ip, pmpreqrep->ipv4, sizeof(pmp->natadr.address.ip));
					NET_AdrToString(adrbuf, sizeof(adrbuf), &pmp->natadr);
					pmp->natadr.connum = i+1;
					Con_DPrintf("NAT-PMP: Public ip is %s\n", adrbuf);

#ifdef SUPPORT_ICE
					if (pmp->natadr.type && pmp->natadr.port)
						ICE_AddLCandidateConn(collection, &pmp->natadr, ICE_SRFLX);	//let ICE connections know about it
#endif
					return true;
				}
				if (net_message.cursize == 16 && pmpreqrep->op == 129)
				{
					switch(BigShort(pmpreqrep->resultcode))
					{
					case 0:
						break;
					case 1:
						Con_Printf("NAT-PMP: unsupported version\n");
						return true;
					case 2:
						Con_Printf("NAT-PMP: refused - please reconfigure router\n");
						return true;
					case 3:
						Con_Printf("NAT-PMP: network failure\n");
						return true;
					case 4:
						Con_Printf("NAT-PMP: out of resources\n");
						return true;
					case 5:
						Con_Printf("NAT-PMP: unsupported opcode\n");
						return true;
					default:
						return false;
					}

					Con_DPrintf("NAT-PMP: Local port %u publically available on port %u\n", (unsigned short)BigShort(pmpreqrep->privport), (unsigned short)BigShort(pmpreqrep->pubport));
					pmp->natadr.port = pmpreqrep->pubport;

#ifdef SUPPORT_ICE
					if (pmp->natadr.type && pmp->natadr.port)
						ICE_AddLCandidateConn(collection, &pmp->natadr, ICE_SRFLX);
#endif
					return true;
				}
				return false;
			}
		}
	}
	return false;
}

static void FTENET_NATPMP_Refresh(pmpcon_t *pmp, short oldport, ftenet_connections_t *collection)
{
	int i, m;
	netadr_t adr;

	netadr_t	addr[64];
	struct ftenet_generic_connection_s			*con[sizeof(addr)/sizeof(addr[0])];
	int			flags[sizeof(addr)/sizeof(addr[0])];

	struct
	{
		qbyte ver; qbyte op; short reserved1;
		short privport; short pubport;
		int mapping_expectancy;
	} pmpreqmsg;

	pmpreqmsg.ver = 0;
	pmpreqmsg.op = 1;
	pmpreqmsg.reserved1 = BigShort(0);
	pmpreqmsg.privport = BigShort(0);
	pmpreqmsg.pubport = BigShort(0);
	pmpreqmsg.mapping_expectancy = BigLong(60*5);

	if (!collection)
		return;

	m = NET_EnumerateAddresses(collection, con, flags, addr, sizeof(addr)/sizeof(addr[0]));

	for (i = 0; i < m; i++)
	{
		//ignore any ips which are proxied by other people. that would be too weird.
		if (flags[i] & (ADDR_NATPMP|ADDR_UPNPIGP))
			continue;

		adr = addr[i];

		//unipv6ify it if its a hybrid socket.
		if (adr.type == NA_IPV6 &&
			!*(int*)&adr.address.ip6[0] &&
			!*(int*)&adr.address.ip6[4] &&
			!*(short*)&adr.address.ip6[8] &&
			*(short*)&adr.address.ip6[10]==(short)0xffff &&
			!*(int*)&adr.address.ip6[12])
		{
			*(int*)adr.address.ip = *(int*)&adr.address.ip6[12];
			adr.type = NA_IP;
		}

		if (adr.type == NA_IP)
		{
			if (adr.address.ip[0] == 127)	//yes. loopback has a lot of ip addresses. wasteful but whatever.
				continue;

			//assume a netmask of 255.255.255.0
			adr.address.ip[3] = 1;
		}
//		else if (adr.type == NA_IPV6)
//		{
//		}
		else
			continue;

		pmpreqmsg.privport = adr.port;
		pmpreqmsg.pubport = oldport?oldport:adr.port;

		if (*(int*)pmp->reqpmpaddr.address.ip == INADDR_ANY)
		{
			pmp->pmpaddr = adr;
			pmp->pmpaddr.port = pmp->reqpmpaddr.port;
		}
		else
			pmp->pmpaddr = pmp->reqpmpaddr;

		if (*(int*)pmp->pmpaddr.address.ip == INADDR_ANY)
			continue;

		//get the public ip.
		pmpreqmsg.op = 0;
		NET_SendPacket(collection, 2, &pmpreqmsg, &pmp->pmpaddr);

		//open the firewall/nat.
		pmpreqmsg.op = 1;
		NET_SendPacket(collection, sizeof(pmpreqmsg), &pmpreqmsg, &pmp->pmpaddr);

		break;
	}
}
#define PMP_POLL_TIME (1000*30)//every 30 seconds
qboolean Net_OpenUDPPort(char *privateip, int privateport, char *publicip, size_t publiciplen, int *publicport);
int FTENET_NATPMP_GetLocalAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
	pmpcon_t *pmp = (pmpcon_t*)con;
/*
	char pubip[256];
	int pubport;

	if (Net_OpenUDPPort("192.168.1.4", 27500, pubip, sizeof(pubip), &pubport))
	{
		*adrflags = ADDR_UPNPIGP;
		NET_StringToAdr(pubip, pubport, addresses);
		return 1;
	}
*/
	if (maxaddresses)
	{
		*adrflags = ADDR_NATPMP;
		*addresses = pmp->natadr;
		return (pmp->natadr.type != NA_INVALID) && (pmp->natadr.port != 0);
	}
	return 0;
}
qboolean FTENET_NATPMP_GetPacket(struct ftenet_generic_connection_s *con)
{
	pmpcon_t *pmp = (pmpcon_t*)con;
	unsigned int now = Sys_Milliseconds();
	if (now - pmp->refreshtime > PMP_POLL_TIME)	//weird logic to cope with wrapping
	{
		pmp->refreshtime = now;
		FTENET_NATPMP_Refresh(pmp, pmp->natadr.port, pmp->col);
	}
	return false;
}
neterr_t FTENET_NATPMP_SendPacket(struct ftenet_generic_connection_s *con, int length, const void *data, netadr_t *to)
{
	return NETERR_NOROUTE;
}
void FTENET_NATPMP_Close(struct ftenet_generic_connection_s *con)
{
	//FIXME: we should send a packet to close the port
	Z_Free(con);
}
//qboolean Net_OpenUDPPort(char *privateip, int privateport, char *publicip, size_t publiciplen, int *publicport);
ftenet_generic_connection_t *FTENET_NATPMP_EstablishConnection(qboolean isserver, const char *address, netadr_t pmpadr)
{
	pmpcon_t *pmp;

	if (pmpadr.prot == NP_NATPMP)
		pmpadr.prot = NP_DGRAM;
	if (pmpadr.type != NA_IP)
		return NULL;

	pmp = Z_Malloc(sizeof(*pmp));
	pmp->col = svs.sockets;
	Q_strncpyz(pmp->pub.name, "natpmp", sizeof(pmp->pub.name));
	pmp->reqpmpaddr = pmpadr;
	pmp->pub.GetLocalAddresses = FTENET_NATPMP_GetLocalAddresses;
	pmp->pub.GetPacket = FTENET_NATPMP_GetPacket;
	//qboolean (*ChangeLocalAddress)(struct ftenet_generic_connection_s *con, const char *newaddress);
	pmp->pub.SendPacket = FTENET_NATPMP_SendPacket;
	pmp->pub.Close = FTENET_NATPMP_Close;
	pmp->pub.thesocket = INVALID_SOCKET;

	pmp->refreshtime = Sys_Milliseconds() + PMP_POLL_TIME*64;

//	Net_OpenUDPPort();

	return &pmp->pub;
}
#endif

#ifdef HAVE_DTLS
struct dtlspeer_s
{
	const dtlsfuncs_t *funcs;

	ftenet_connections_t *col;
	void *dtlsstate;
	netadr_t addr;
	float timeout;

	struct dtlspeer_s *next;
	struct dtlspeer_s **link;
};

void NET_DTLS_Timeouts(ftenet_connections_t *col)
{
	struct dtlspeer_s *peer, **link;
	if (!col)
		return;
	for (link = &col->dtls; (peer=*link); )
	{
		if (peer->timeout < realtime)
		{
			peer->funcs->DestroyContext(peer->dtlsstate);
			*link = peer->next;
			continue;
		}

		peer->funcs->Timeouts(peer->dtlsstate);
		link = &peer->next;
	}
}

const dtlsfuncs_t *DTLS_InitServer(void)
{
	const dtlsfuncs_t *f = NULL;
#ifdef HAVE_GNUTLS
	if (!f)
		f = GNUDTLS_InitServer();
#endif
#ifdef HAVE_OPENSSL
	if (!f)
		f = OSSL_InitServer();
#endif
#ifdef HAVE_WINSSPI
	if (!f)
		f = SSPI_DTLS_InitServer();
#endif
	return f;
}
const dtlsfuncs_t *DTLS_InitClient(void)
{
	const dtlsfuncs_t *f = NULL;
#ifdef HAVE_WINSSPI
	if (!f)
		f = SSPI_DTLS_InitClient();
#endif
#ifdef HAVE_GNUTLS
	if (!f)
		f = GNUDTLS_InitClient();
#endif
#ifdef HAVE_OPENSSL
	if (!f)
		f = OSSL_InitClient();
#endif
	return f;
}

static neterr_t NET_SendPacketCol (ftenet_connections_t *collection, int length, const void *data, netadr_t *to);
static neterr_t FTENET_DTLS_DoSendPacket(void *cbctx, const qbyte *data, size_t length)
{	//callback that does the actual sending
	struct dtlspeer_s *peer = cbctx;
	return NET_SendPacketCol(peer->col, length, data, &peer->addr);
}
qboolean NET_DTLS_Create(ftenet_connections_t *col, netadr_t *to)
{
	extern cvar_t timeout;
	struct dtlspeer_s *peer;
	if (to->prot != NP_DGRAM)
		return false;
	for (peer = col->dtls; peer; peer = peer->next)
	{
		if (NET_CompareAdr(&peer->addr, to))
			break;
	}
	if (!peer)
	{
		char hostname[256];
		peer = Z_Malloc(sizeof(*peer));
		peer->addr = *to;
		peer->col = col;

		if (col->islisten)
			peer->funcs = DTLS_InitServer();
		else
			peer->funcs = DTLS_InitClient();
		if (peer->funcs)
			peer->dtlsstate = peer->funcs->CreateContext(NET_BaseAdrToString(hostname, sizeof(hostname), to), peer, FTENET_DTLS_DoSendPacket, col->islisten);

		peer->timeout = realtime+timeout.value;
		if (peer->dtlsstate)
		{
			if (peer->next)
				peer->next->link = &peer->next;
			peer->link = &col->dtls;
			peer->next = col->dtls;
			col->dtls = peer;
		}
		else
		{
			Z_Free(peer);
			peer = NULL;
		}
	}
	return peer!=NULL;
}
static void NET_DTLS_DisconnectPeer(ftenet_connections_t *col, struct dtlspeer_s *peer)
{
//	Sys_Printf("Destroy %p\n", peer->dtlsstate);

	if (peer->next)
		peer->next->link = peer->link;
	*peer->link = peer->next;

	peer->funcs->DestroyContext(peer->dtlsstate);
	Z_Free(peer);
}
qboolean NET_DTLS_Disconnect(ftenet_connections_t *col, netadr_t *to)
{
	struct dtlspeer_s *peer;
	netadr_t n = *to;
	if (!col || (to->prot != NP_DGRAM && to->prot != NP_DTLS))
		return false;
	n.prot = NP_DGRAM;
	for (peer = col->dtls; peer; peer = peer->next)
	{
		if (NET_CompareAdr(&peer->addr, &n))
		{
			NET_DTLS_DisconnectPeer(col, peer);
			break;
		}
	}
	return peer?true:false;
}
static neterr_t FTENET_DTLS_SendPacket(ftenet_connections_t *col, int length, const void *data, netadr_t *to)
{
	struct dtlspeer_s *peer;
	to->prot = NP_DGRAM;
	for (peer = col->dtls; peer; peer = peer->next)
	{
		if (NET_CompareAdr(&peer->addr, to))
			break;
	}
	to->prot = NP_DTLS;
	if (peer)
		return peer->funcs->Transmit(peer->dtlsstate, data, length);
	else
		return NETERR_NOROUTE;
}

qboolean NET_DTLS_Decode(ftenet_connections_t *col)
{
	extern cvar_t timeout;
	struct dtlspeer_s *peer;
	for (peer = col->dtls; peer; peer = peer->next)
	{
		if (NET_CompareAdr(&peer->addr, &net_from))
		{
			peer->timeout = realtime+timeout.value;	//refresh the timeout if our peer is still alive.
			switch(peer->funcs->Received(peer->dtlsstate, net_message.data, net_message.cursize))
			{
			case NETERR_DISCONNECTED:
				Sys_Printf("disconnected %p\n", peer->dtlsstate);
				NET_DTLS_DisconnectPeer(col, peer);
				break;
			case NETERR_NOROUTE:
				return false;	//not a valid dtls packet.
			default:
			case NETERR_CLOGGED:
				//ate it
				net_message.cursize = 0;
				break;
			case NETERR_SENT:
				//we decoded it properly
				break;
			}
			net_from.prot = NP_DTLS;
			return true;
		}
	}
	return false;
}
#endif


size_t NET_GetConnectionCertificate(struct ftenet_connections_s *col, netadr_t *a, enum certprops_e prop, char *out, size_t outsize)
{
	if (!col)
		return 0;

	switch(prop)
	{
	default:
		break;
	case QCERT_PEERFINGERPRINT:
#if 0//def HAVE_DTLS
		if (a->prot == NP_DTLS)
		{
			struct dtlspeer_s *peer;
			{
				a->prot = NP_DGRAM;
				for (peer = col->dtls; peer; peer = peer->next)
				{
					if (NET_CompareAdr(&peer->addr, a))
						break;
				}
				a->prot = NP_DTLS;
			}
			if (peer)
				return peer->funcs->GetPeerCertificate(peer->dtlsstate, data, length);
		}
#endif
		return 0;
	}
	return 0;
}




static qboolean FTENET_AddToCollection_Ptr(ftenet_connections_t *col, const char *name, ftenet_generic_connection_t *(*establish)(qboolean isserver, const char *address, netadr_t adr), qboolean islisten, const char *address, netadr_t *adr)
{
	int count = 0;
	int i;

	if (!col)
		return false;

	if (name)
	{
		for (i = 0; i < MAX_CONNECTIONS; i++)
		{
			if (col->conn[i])
			if (*col->conn[i]->name && !strcmp(col->conn[i]->name, name))
			{
				if (adr && adr->type != NA_INVALID && islisten)
				if (col->conn[i]->ChangeLocalAddress)
				{
					if (col->conn[i]->ChangeLocalAddress(col->conn[i], adr))
						return true;
				}

				col->conn[i]->Close(col->conn[i]);
				col->conn[i] = NULL;
			}
		}
	}

	if (adr && establish)
	{
		for (i = 0; i < MAX_CONNECTIONS; i++)
		{
			if (!col->conn[i])
			{
				col->conn[i] = establish(islisten, address, *adr);
				if (!col->conn[i])
					break;
				if (name)
					Q_strncpyz(col->conn[i]->name, name, sizeof(col->conn[i]->name));
				count++;
				break;
			}
		}
	}
	return count > 0;
}
qboolean FTENET_AddToCollection(ftenet_connections_t *col, const char *name, const char *addresslist, netadrtype_t addrtype, netproto_t addrprot)
{
	qboolean islisten;
	netadr_t adr[8];
	ftenet_generic_connection_t *(*establish[countof(adr)])(qboolean isserver, const char *address, netadr_t adr);
	char address[countof(adr)][256];
	unsigned int i, j;
	qboolean success = false;

	if (!col)
		return false;
	islisten = col->islisten;

	if (name && strchr(name, ':'))
		return false;

	for (i = 0; addresslist && *addresslist && i < countof(adr); i++)
	{
		addresslist = COM_ParseStringSet(addresslist, address[i], sizeof(address[i]));
		//resolve the address to something sane so we can determine the address type and thus the connection type to use
		if (!*address[i])
			adr[i].type = NA_INVALID;
		else //if (islisten)
		{
			if (!NET_PortToAdr(addrtype, addrprot, address[i], &adr[i]))
				return false;
		}
/*		else
		{
			if (!NET_StringToAdr(address[i], 0, &adr[i]))
				return false;
		}
*/
#ifdef HAVE_WEBSOCKCL
		if (adr[i].prot == NP_WS && adr[i].type == NA_WEBSOCKET)	establish[i] = FTENET_WebSocket_EstablishConnection; else
		if (adr[i].prot == NP_WSS && adr[i].type == NA_WEBSOCKET)	establish[i] = FTENET_WebSocket_EstablishConnection; else
		if (adr[i].prot == NP_DTLS && adr[i].type == NA_WEBSOCKET)	establish[i] = FTENET_WebSocket_EstablishConnection; else
#endif
#ifdef HAVE_NATPMP
		if (adr[i].prot == NP_NATPMP&& adr[i].type == NA_IP)	establish[i] = FTENET_NATPMP_EstablishConnection; else
#endif
#if defined(HAVE_CLIENT) && defined(HAVE_SERVER)
		if (adr[i].prot == NP_DGRAM && adr[i].type == NA_LOOPBACK)	establish[i] = FTENET_Loop_EstablishConnection; else
#endif
#ifdef HAVE_IPV4
		if ((adr[i].prot == NP_DGRAM) && adr[i].type == NA_IP)	establish[i] = FTENET_Datagram_EstablishConnection;	else
#endif
#ifdef HAVE_IPV6
		if ((adr[i].prot == NP_DGRAM) && adr[i].type == NA_IPV6)	establish[i] = FTENET_Datagram_EstablishConnection;	else
#endif
#ifdef HAVE_IPX
		if (adr[i].prot == NP_DGRAM && adr[i].type == NA_IPX)	establish[i] = FTENET_Datagram_EstablishConnection;	else
#endif
#ifdef UNIXSOCKETS
		if (adr[i].prot == NP_DGRAM && adr[i].type == NA_UNIX)	establish[i] = FTENET_Datagram_EstablishConnection;	else
	#if defined(TCPCONNECT)
		if (adr[i].prot == NP_STREAM&& adr[i].type == NA_UNIX)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
		if (adr[i].prot == NP_WS    && adr[i].type == NA_UNIX)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
		if (adr[i].prot == NP_TLS    && adr[i].type == NA_UNIX)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
	#endif
#endif
#if defined(TCPCONNECT) && defined(HAVE_IPV4)
		if (adr[i].prot == NP_WS	&& adr[i].type == NA_IP)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
		if (adr[i].prot == NP_STREAM&& adr[i].type == NA_IP)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
		if (adr[i].prot == NP_TLS	&& adr[i].type == NA_IP)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
#endif
#if defined(TCPCONNECT) && defined(HAVE_IPV6)
		if (adr[i].prot == NP_WS	&& adr[i].type == NA_IPV6)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
		if (adr[i].prot == NP_STREAM&& adr[i].type == NA_IPV6)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
		if (adr[i].prot == NP_TLS	&& adr[i].type == NA_IPV6)	establish[i] = FTENET_TCPConnect_EstablishConnection;	else
#endif
#ifdef IRCCONNECT
		if (adr[i].prot == NP_TLS)								establish[i] = FTENET_IRCConnect_EstablishConnection;	else
#endif
			establish[i] = NULL;
	}

	if (i == 1)
	{
		success |= FTENET_AddToCollection_Ptr(col, name, establish[0], islisten, address[0], &adr[0]);
		i = 0;
	}
	else
		success |= FTENET_AddToCollection_Ptr(col, name, NULL, islisten, NULL, NULL);

	for (j = 0; j < i; j++)
	{
		success |= FTENET_AddToCollection_Ptr(col, va("%s:%i", name, j), establish[j], islisten, address[j], &adr[j]);
	}
	for (; j < countof(adr); j++)
	{
		success |= FTENET_AddToCollection_Ptr(col, va("%s:%i", name, j), NULL, islisten, NULL, NULL);
	}
	return success;
}

void FTENET_CloseCollection(ftenet_connections_t *col)
{
	int i;
	if (!col)
		return;
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (col->conn[i])
		{
			col->conn[i]->Close(col->conn[i]);
			col->conn[i] = NULL;
		}
	}
	Z_Free(col);
}

void FTENET_Generic_Close(ftenet_generic_connection_t *con)
{
#ifdef HAVE_PACKET
	if (con->thesocket != INVALID_SOCKET)
		closesocket(con->thesocket);
#endif
	Z_Free(con);
}

#if defined(_WIN32) && defined(HAVE_PACKET)
int FTENET_GetLocalAddress(int port, qboolean ipx, qboolean ipv4, qboolean ipv6,   unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
	//in win32, we can look up our own hostname to retrieve a list of local interface addresses.
	char		adrs[MAX_ADR_SIZE];
	int found = 0;

	gethostname(adrs, sizeof(adrs));
#ifndef pgetaddrinfo
	if (!pgetaddrinfo)
	{
		struct hostent *h = gethostbyname(adrs);
		int b = 0;
#ifdef HAVE_IPV4
		if(h && h->h_addrtype == AF_INET)
		{
			for (b = 0; h->h_addr_list[b] && maxaddresses; b++)
			{
				struct sockaddr_in from;
				from.sin_family = AF_INET;
				from.sin_port = port;
				memcpy(&from.sin_addr, h->h_addr_list[b], sizeof(from.sin_addr));
				SockadrToNetadr((struct sockaddr_qstorage*)&from, sizeof(from), addresses);

				*adrflags++ = 0;
				addresses++;
				maxaddresses--;
				found++;
			}
		}
#endif
#ifdef HAVE_IPV6
		if(h && h->h_addrtype == AF_INET6)
		{
			for (b = 0; h->h_addr_list[b] && maxaddresses; b++)
			{
				struct sockaddr_in6 from;
				from.sin6_family = AF_INET6;
				from.sin6_port = port;
				from.sin6_scope_id = 0;
				memcpy(&from.sin6_addr, h->h_addr_list[b], sizeof(((struct sockaddr_in6*)&from)->sin6_addr));
				SockadrToNetadr((struct sockaddr_qstorage*)&from, sizeof(from), addresses);
				*adrflags++ = 0;
				addresses++;
				maxaddresses--;
				found++;
			}
		}
#endif
	}
	else
#endif
	{
		struct addrinfo hints, *result, *itr;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = 0;    /* Allow IPv4 or IPv6 */
		hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
		hints.ai_flags = 0;
		hints.ai_protocol = 0;          /* Any protocol */

		if (pgetaddrinfo(adrs, NULL, &hints, &result) == 0)
		{
			for (itr = result; itr; itr = itr->ai_next)
			{
				if (0
#ifdef HAVE_IPV4
					|| (itr->ai_addr->sa_family == AF_INET && ipv4)
#endif
#ifdef HAVE_IPV6
					|| (itr->ai_addr->sa_family == AF_INET6 && ipv6)
#endif
#ifdef HAVE_IPX
					|| (itr->ai_addr->sa_family == AF_IPX && ipx)
#endif
					)
				if (maxaddresses)
				{
					SockadrToNetadr((struct sockaddr_qstorage*)itr->ai_addr, sizeof(struct sockaddr_qstorage), addresses);
					addresses->port = port;
					*adrflags++ = 0;
					addresses++;
					maxaddresses--;
					found++;
				}
			}
			pfreeaddrinfo(result);

			/*if none found, fill in the 0.0.0.0 or whatever*/
			if (!found && maxaddresses)
			{
				memset(addresses, 0, sizeof(*addresses));
				addresses->port = port;
				if (ipv6)
					addresses->type = NA_IPV6;
				else if (ipv4)
					addresses->type = NA_IP;
				else if (ipx)
					addresses->type = NA_IPX;
				else
					addresses->type = NA_INVALID;
				*adrflags++ = 0;
				addresses++;
				maxaddresses--;
				found++;
			}
		}
	}
	return found;
}

#elif defined(__linux__) && !defined(ANDROID)
//in linux, looking up our own hostname to retrieve a list of local interface addresses will give no indication that other systems are able to do the same thing and is thus not supported.
//there's some special api instead
//glibc 2.3.
//also available with certain bsds, I'm but unsure which preprocessor we can use.
#include <ifaddrs.h>

static struct ifaddrs *iflist;
unsigned int iftime;	//requery sometimes.
int FTENET_GetLocalAddress(int port, qboolean ipx, qboolean ipv4, qboolean ipv6,   unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
	struct ifaddrs *ifa;
	int fam;
	int idx = 0;
	unsigned int time = Sys_Milliseconds();

	ipv4 = ipv4 && net_dns_ipv4.ival;
	ipv6 = ipv6 && net_dns_ipv6.ival;

	if (time - iftime > 1000 && iflist)
	{
		freeifaddrs(iflist);
		iflist = NULL;
	}
	if (!iflist)
	{
		iftime = time;
		getifaddrs(&iflist);
	}

	for (ifa = iflist; ifa && idx < maxaddresses; ifa = ifa->ifa_next)
	{
		//can happen if the interface is not bound.
		if (ifa->ifa_addr == NULL)
			continue;

		//filter out families that we're not interested in.
		fam = ifa->ifa_addr->sa_family;
		if (
#ifdef HAVE_IPV4
			(fam == AF_INET && ipv4) ||
#endif
#ifdef HAVE_IPV6
			(fam == AF_INET6 && ipv6) ||
#endif
#ifdef HAVE_IPX
			(fam == AF_IPX && ipx) ||
#endif
			0)
		{
			SockadrToNetadr((struct sockaddr_qstorage*)ifa->ifa_addr, sizeof(struct sockaddr_qstorage), &addresses[idx]);
			addresses[idx].port = port;
			adrflags[idx] = 0;
			idx++;
		}
	}
	return idx;
}
#else
int FTENET_GetLocalAddress(int port, qboolean ipx, qboolean ipv4, qboolean ipv6,   unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
	return 0;
}
#endif

int FTENET_Generic_GetLocalAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
#ifndef HAVE_PACKET
	return 0;
#else
	struct sockaddr_qstorage	from;
	int fromsize = sizeof(from);
	netadr_t adr;
	int found = 0;

	if (getsockname (con->thesocket, (struct sockaddr*)&from, &fromsize) != -1)
	{
		memset(&adr, 0, sizeof(adr));
		SockadrToNetadr(&from, fromsize, &adr);

#ifdef USE_GETHOSTNAME_LOCALLISTING
		//if its bound to 'any' address, ask the system what addresses it actually accepts.
		if ((adr.type == NA_IPV6) &&
			!*(int*)&adr.address.ip6[0] &&
			!*(int*)&adr.address.ip6[4] &&
			!*(short*)&adr.address.ip6[8] &&
			*(short*)&adr.address.ip6[10]==(short)0xffff &&
			!*(int*)&adr.address.ip6[12])
		{
			//ipv6 socket bound to the ipv4-any address is a bit weird, but oh well.
#ifdef _WIN32
			//win32 is buggy and treats binding to [::] as [::ffff:0.0.0.0] (even with pure ipv6 sockets)
			//explicitly binding to [::ffff:0.0.0.0] appears to fail in windows, thus any such socket will definitely support ipv6.
			qboolean canipv4 = (con->addrtype[0] == NA_IP) || (con->addrtype[1] == NA_IP);
			found = FTENET_GetLocalAddress(adr.port, false, canipv4, true, adrflags, addresses, maxaddresses);
#else
			//FIXME: we should validate that we support hybrid sockets?
			found = FTENET_GetLocalAddress(adr.port, false, true, false, adrflags, addresses, maxaddresses);
#endif
		}
		else
		{
			int b;
			for (b = 0; b < sizeof(adr.address); b++)
				if (((unsigned char*)&adr.address)[b] != 0)
					break;

			if (b == sizeof(adr.address))
			{
				qboolean ipx=false, ipv4=false, ipv6=false;
				if (adr.type == NA_IP)
					ipv4 = true;
				else if (adr.type == NA_IPX)
					ipx = true;
				else if (adr.type == NA_IPV6)
				{
					ipv4 = (con->addrtype[0] == NA_IP) || (con->addrtype[1] == NA_IP);
					ipv6 = true;
				}

				found = FTENET_GetLocalAddress(adr.port, ipx, ipv4, ipv6, adrflags, addresses, maxaddresses);
			}
		}
#endif

		//and use the bound address (even if its 0.0.0.0) if we didn't grab a list from the system.
		if (!found)
		{
			if (maxaddresses && adr.type == NA_IPV6 &&
				!*(int*)&adr.address.ip6[0] &&
				!*(int*)&adr.address.ip6[4] &&
				!*(int*)&adr.address.ip6[8] &&
				!*(int*)&adr.address.ip6[12])
			{
				*addresses = adr;
				addresses->type = NA_IP;

				*adrflags++ = 0;
				addresses++;
				maxaddresses--;
				found++;
			}

			if (maxaddresses)
			{
				*addresses = adr;

				*adrflags++ = 0;
				addresses++;
				maxaddresses--;
				found++;
			}
		}
	}

	return found;
#endif
}

qboolean FTENET_Datagram_GetPacket(ftenet_generic_connection_t *con)
{
#ifndef HAVE_PACKET
	return false;
#else
	struct sockaddr_qstorage	from;
	int fromlen;
	int ret;
	int err;
	char		adr[MAX_ADR_SIZE];

	if (con->thesocket == INVALID_SOCKET)
		return false;

	fromlen = sizeof(from);
	((struct sockaddr*)&from)->sa_family = AF_UNSPEC;
	ret = recvfrom (con->thesocket, (char *)net_message_buffer, sizeof(net_message_buffer), 0, (struct sockaddr*)&from, &fromlen);

	if (ret == -1)
	{
		err = neterrno();

		if (err == NET_EWOULDBLOCK)
			return false;
		if (err == NET_EMSGSIZE)
		{
			static unsigned int resettime;
			unsigned int curtime = Sys_Milliseconds();
			if (curtime-resettime >= 5000)	//throttle prints to once per 5 secs (even if they're about different clients, yay ddos)
			{
				SockadrToNetadr (&from, fromlen, &net_from);
				Con_TPrintf ("Warning:  Oversize packet from %s\n",
				NET_AdrToString (adr, sizeof(adr), &net_from));
			}
			return false;
		}
		if (err == NET_ECONNABORTED || err == NET_ECONNRESET)
		{
			static unsigned int resettime;
			unsigned int curtime = Sys_Milliseconds();
			if (curtime-resettime >= 5000 || err == NET_ECONNRESET)	//throttle prints to once per 5 secs (even if they're about different clients, yay ddos)
			{
				if (((struct sockaddr*)&from)->sa_family != AF_UNSPEC)
				{
					SockadrToNetadr (&from, fromlen, &net_from);
					Con_TPrintf ("Connection lost or aborted (%s)\n", NET_AdrToString (adr, sizeof(adr), &net_from));	//server died/connection lost.
				}
				else
					Con_TPrintf ("Connection lost or aborted\n");	//server died/connection lost.
				resettime = curtime;
#ifdef HAVE_CLIENT
				//fixme: synthesise a reset packet for the caller to handle? "\xff\xff\xff\xffreset" ?
				if (cls.state != ca_disconnected && !con->islisten)
				{
					if (cls.lastarbiatarypackettime+5 < Sys_DoubleTime())	//too many mvdsv
						Cbuf_AddText("disconnect\nreconnect\n", RESTRICT_LOCAL);	//retry connecting.
					else
						Con_Printf("Packet was not delivered - server might be badly configured\n");
					return false;
				}
#endif
			}
			return false;
		}

		if (((struct sockaddr*)&from)->sa_family != AF_UNSPEC)
			Con_Printf ("NET_GetPacket: Error (%i): %s (%s)\n", err, strerror(err), NET_AdrToString (adr, sizeof(adr), &net_from));
		else
			Con_Printf ("NET_GetPacket: Error (%i): %s\n", err, strerror(err));
		return false;
	}

	SockadrToNetadr (&from, fromlen, &net_from);

	if (net_from.type == NA_INVALID)
	{	//this really shouldn't happen. Blame the OS.
		Con_TPrintf ("Warning: sender's address type not known (%i)\n", (int)((struct sockaddr*)&from)->sa_family);
		return false;	//packet from an unsupported protocol? no way can we respond, so what's the point
	}

	net_message.packing = SZ_RAWBYTES;
	net_message.currentbit = 0;
	net_message.cursize = ret;
	if (net_message.cursize >= sizeof(net_message_buffer) )
	{
		Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &net_from));
		return false;
	}

	return true;
#endif
}

neterr_t FTENET_Datagram_SendPacket(ftenet_generic_connection_t *con, int length, const void *data, netadr_t *to)
{
#ifndef HAVE_PACKET
	return NETERR_DISCONNECTED;
#else
	struct sockaddr_qstorage	addr;
	int size;
	int ret;

	if (to->prot != NP_DGRAM)
		return NETERR_NOROUTE;

	for (size = 0; size < FTENET_ADDRTYPES; size++)
		if (to->type == con->addrtype[size])
			break;
	if (size == FTENET_ADDRTYPES)
		return NETERR_NOROUTE;

#ifdef HAVE_IPV6
	/*special code to handle sending to hybrid sockets*/
	if (con->addrtype[1] == NA_IPV6 && to->type == NA_IP)
	{
		memset(&addr, 0, sizeof(struct sockaddr_in6));
		((struct sockaddr_in6*)&addr)->sin6_family = AF_INET6;
		*(short*)&((struct sockaddr_in6*)&addr)->sin6_addr.s6_addr[10] = 0xffff;
		*(int*)&((struct sockaddr_in6*)&addr)->sin6_addr.s6_addr[12] = *(int*)&to->address.ip;
		((struct sockaddr_in6*)&addr)->sin6_port = to->port;
		size = sizeof(struct sockaddr_in6);
	}
	else
#endif
	{
		size = NetadrToSockadr (to, &addr);
	}

	if (!data)
		ret = 0;	//don't send a runt, but pretend we did... yes, this'll confuse EnsureRoute, but at least it'll ensure there's a udp socket open, somewhere.
	else
		ret = sendto (con->thesocket, data, length, 0, (struct sockaddr*)&addr, size );
	if (ret == -1)
	{
		int ecode = neterrno();
// wouldblock is silent
		if (ecode == NET_EWOULDBLOCK)
			return NETERR_CLOGGED;

		if (ecode == NET_ECONNREFUSED)
			return NETERR_DISCONNECTED;

		if (ecode == NET_EMSGSIZE)
			return NETERR_MTU;

		if (ecode == NET_EADDRNOTAVAIL)
			return NETERR_NOROUTE;	//this interface doesn't actually support that (eg: happens when ipv6 is disabled on a specific interface).

		if (ecode == NET_EACCES)
		{
			Con_Printf("Access denied: check firewall\n");
			return NETERR_DISCONNECTED;
		}

		{
			char adr[256];
#ifdef HAVE_CLIENT
			if (ecode == NET_EADDRNOTAVAIL)
				Con_DPrintf("NET_SendPacket(%s) Warning: %i\n", NET_AdrToString (adr, sizeof(adr), to), ecode);
			else
#endif
			{
#ifdef _WIN32
				Con_Printf ("NET_SendPacket(%s) ERROR: %i\n", NET_AdrToString (adr, sizeof(adr), to), ecode);
#else
				Con_Printf ("NET_SendPacket(%s) ERROR: %s\n", NET_AdrToString (adr, sizeof(adr), to), strerror(ecode));
#endif
			}
		}
	}
	else if (ret < length)
		return NETERR_MTU;
	return NETERR_SENT;
#endif
}

qboolean	NET_PortToAdr (netadrtype_t adrfamily, netproto_t adrprot, const char *s, netadr_t *a)
{
	char *e;
	if (net_enabled.ival || adrfamily == NA_LOOPBACK)
	{
		int port = strtoul(s, &e, 10);
		if (*e)	//if *e then its not just a single number in there, so treat it as a proper address.
			return NET_StringToAdr(s, 0, a);
		else if (e != s)	//if we actually read something (even a 0)
		{
			memset(a, 0, sizeof(*a));
			a->port = htons((unsigned short)port);
			a->type = adrfamily;
			a->prot = adrprot;

			return a->type != NA_INVALID;
		}
	}
	a->type = NA_INVALID;
	return false;
}

#ifdef HAVE_PACKET
/*just here to prevent the client from spamming new sockets, which can be a problem with certain q2 servers*/
static qboolean FTENET_Datagram_ChangeLocalAddress(struct ftenet_generic_connection_s *con, netadr_t *adr)
{
	struct sockaddr_qstorage address;
	netadr_t current;
	int namelen = sizeof(address);
	if (getsockname (con->thesocket, (struct sockaddr *)&address, &namelen) == 0)
	{
		SockadrToNetadr(&address, namelen, &current);

		//make sure the types match (special check for ipv6 hybrid sockets that accept ipv4 too)
		if (adr->type == current.type
#if defined(HAVE_IPV4) && defined(HAVE_IPV6)
			|| (net_hybriddualstack.ival && adr->type == NA_IP && current.type == NA_IPV6)
#endif
			)
		{	//make sure the port is currect (or they don't care which port)
			if (adr->port == current.port || !adr->port)
				return true;	//then pretend we changed it, because needed to change in the first place.
		}
	}

	//doesn't match how its currently bound, so I guess we need to rebind then.
	return false;
}
#endif

ftenet_generic_connection_t *FTENET_Datagram_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
#ifndef HAVE_PACKET
	return NULL;
#else
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_generic_connection_t *newcon;

	unsigned long _true = true;
	SOCKET newsocket = INVALID_SOCKET;
	int temp;
	struct sockaddr_qstorage qs;
	int family;
	int port;
	int bindtries;
	const int bindmaxtries = 100;
	int bufsz;
	qboolean hybrid = false;
	int protocol;
	char addrstr[128];

	switch(adr.type)
	{
#if defined(HAVE_IPV4) || defined(HAVE_IPV6)
	case NA_IP:
	case NA_IPV6:
		protocol = IPPROTO_UDP;
		break;
#endif
#ifdef HAVE_IPX
	case NA_IPX:
		protocol = NSPROTO_IPX;
		break;
#endif
	default:
		protocol = 0;
		break;
	}


	if (adr.type == NA_INVALID)
	{
		Con_Printf(CON_ERROR "unable to resolve local address %s\n", address);
		return NULL;	//couldn't resolve the name
	}
	temp = NetadrToSockadr(&adr, &qs);
	family = ((struct sockaddr*)&qs)->sa_family;

#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
	if (/*isserver &&*/ family == AF_INET && net_hybriddualstack.ival && !((struct sockaddr_in*)&qs)->sin_addr.s_addr)
	{
		unsigned long _false = false;
		if ((newsocket = socket (AF_INET6, SOCK_CLOEXEC|SOCK_DGRAM, protocol)) != INVALID_SOCKET)
		{
			if (0 == setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_false, sizeof(_false)))
			{
//				int ip = ((struct sockaddr_in*)&qs)->sin_addr.s_addr;
				int port = ((struct sockaddr_in*)&qs)->sin_port;
//				ip = ((struct sockaddr_in*)&qs)->sin_addr.s_addr;
				memset(&qs, 0, sizeof(struct sockaddr_in6));
				((struct sockaddr_in6*)&qs)->sin6_family = AF_INET6;
/*
				if (((struct sockaddr_in*)&qs)->sin_addr.s_addr)
				{
					((struct sockaddr_in6*)&qs)->sin6_addr.s6_addr[10] = 0xff;
					((struct sockaddr_in6*)&qs)->sin6_addr.s6_addr[11] = 0xff;
					((struct sockaddr_in6*)&qs)->sin6_addr.s6_addr[12] = ((qbyte*)&ip)[0];
					((struct sockaddr_in6*)&qs)->sin6_addr.s6_addr[13] = ((qbyte*)&ip)[1];
					((struct sockaddr_in6*)&qs)->sin6_addr.s6_addr[14] = ((qbyte*)&ip)[2];
					((struct sockaddr_in6*)&qs)->sin6_addr.s6_addr[15] = ((qbyte*)&ip)[3];
				}
*/
				((struct sockaddr_in6*)&qs)->sin6_port = port;
				temp = sizeof(struct sockaddr_in6);
				adr.type = NA_IPV6;
				hybrid = true;
			}
			else
			{
				/*v6only failed... if the option doesn't exist, chances are this is a hybrid system which doesn't support both simultaneously anyway*/
				closesocket(newsocket);
				newsocket = INVALID_SOCKET;
			}
		}
	}
#endif

	if (newsocket == INVALID_SOCKET)
		if ((newsocket = socket (family, SOCK_CLOEXEC|SOCK_DGRAM, protocol)) == INVALID_SOCKET)
		{
			return NULL;
		}

#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
	if (family == AF_INET6)
		setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_true, sizeof(_true));
#endif

#if defined(_WIN32) && defined(SO_EXCLUSIVEADDRUSE)
	//win32 is so fucked up
	setsockopt(newsocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&_true, sizeof(_true));
#endif

	bufsz = 1<<18;
	setsockopt(newsocket, SOL_SOCKET, SO_RCVBUF, (void*)&bufsz, sizeof(bufsz));

	switch(family)
	{
#ifdef UNIXSOCKETS
	case AF_UNIX:
		{
			struct sockaddr_un *sa = (struct sockaddr_un *)&qs;
			if (!isserver)
				temp = (char*)&sa->sun_path[0] - (char*)sa;	//linux-specific: bind to an automatic abstract address.
			if (*sa->sun_path && isserver)
			{	//non-abstract sockets don't clean up the filesystem when the socket is closed
				//and we can't re-bind to it while it still exists.
				//so standard practise is to delete it before the bind.
				//we do want to make sure the file is actually a socket before we remove it (so people can't abuse stuffcmds)
				//FIXME: use lock-files
				struct stat s;
				if (stat(sa->sun_path, &s)!=-1)
				{
					if ((s.st_mode & S_IFMT) == S_IFSOCK)
						unlink(sa->sun_path);
				}
			}
			if (bind(newsocket, (struct sockaddr *)sa, temp) == INVALID_SOCKET)
			{
//				perror("gah");
				SockadrToNetadr(&qs, temp, &adr);
				NET_AdrToString(addrstr, sizeof(addrstr), &adr);
				Con_Printf(CON_ERROR "Unable to bind to %s\n", addrstr);
				closesocket(newsocket);
				return NULL;
			}
		}
		break;
#endif

//	case AF_INET:
//	case AF_INET6:
//	case AF_IPX:
	default:
		//try and find an unused port.
		port = ntohs(((struct sockaddr_in*)&qs)->sin_port);
		for (bindtries = 0; bindtries < bindmaxtries; bindtries++)
		{
			((struct sockaddr_in*)&qs)->sin_port = htons((unsigned short)(port+bindtries));
			if ((bind(newsocket, (struct sockaddr *)&qs, temp) == INVALID_SOCKET))
			{
				if (port == 0)
				{	//if binding to an ephemerial port failed, binding to the admin-only ports won't work any better...
					bindtries = bindmaxtries;
					break;
				}
				continue;
			}
			break;
		}
		if (bindtries == bindmaxtries)
		{
			SockadrToNetadr(&qs, temp, &adr);
			NET_AdrToString(addrstr, sizeof(addrstr), &adr);
			Con_Printf(CON_ERROR "Unable to listen at %s\n", addrstr);
			closesocket(newsocket);
			return NULL;
		}
		else if (bindtries && isserver)
		{
			SockadrToNetadr(&qs, temp, &adr);
			NET_AdrToString(addrstr, sizeof(addrstr), &adr);
			Con_Printf(CON_ERROR "Unable to bind to port %i, bound to %s instead\n", port, addrstr);
		}
		break;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(neterrno()));

	//ipv6 sockets need to add themselves to a multicast group, so that we can receive broadcasts on a lan
#if defined(HAVE_IPV6)
	if (family == AF_INET6 || hybrid || isserver)
	{
		struct ipv6_mreq req;
		memset(&req, 0, sizeof(req));
		req.ipv6mr_multiaddr.s6_addr[0] = 0xff;
		req.ipv6mr_multiaddr.s6_addr[1] = 0x02;
		req.ipv6mr_multiaddr.s6_addr[15]= 0x01;
		req.ipv6mr_interface = 0;
		setsockopt(newsocket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&req, sizeof(req));
	}
#endif

	//
	// determine my name & address if we don't already know it
	//
	if (!isserver && net_local_cl_ipadr.type == NA_INVALID)
		NET_GetLocalAddress (newsocket, &net_local_cl_ipadr);

	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		newcon->GetLocalAddresses = FTENET_Generic_GetLocalAddresses;
		newcon->GetPacket = FTENET_Datagram_GetPacket;
		newcon->SendPacket = FTENET_Datagram_SendPacket;
		newcon->Close = FTENET_Generic_Close;
		newcon->ChangeLocalAddress = FTENET_Datagram_ChangeLocalAddress;

		newcon->islisten = isserver;
		if (hybrid)
		{
			newcon->addrtype[0] = NA_IP;
			newcon->addrtype[1] = NA_IPV6;
		}
		else
		{
			newcon->addrtype[0] = adr.type;
			newcon->addrtype[1] = NA_INVALID;
		}

		newcon->thesocket = newsocket;

		return newcon;
	}
	else
	{
		closesocket(newsocket);
		return NULL;
	}
#endif
}

#ifdef TCPCONNECT
typedef struct ftenet_tcpconnect_stream_s {
	vfsfile_t *clientstream;
	int inlen;
	int outlen;

	enum
	{
		TCPC_UNKNOWN,		//waiting to see what they send us.
		TCPC_QIZMO,			//'qizmo\n' handshake, followed by packets prefixed with a 16bit packet length.
#ifdef HAVE_HTTPSV
		TCPC_WEBSOCKETU,	//utf-8 encoded data.
		TCPC_WEBSOCKETB,	//binary encoded data (subprotocol = 'binary')
		TCPC_WEBSOCKETNQ,	//raw nq msg buffers with no encapsulation or handshake
		TCPC_HTTPCLIENT,	//we're sending a file to this victim.
		TCPC_WEBRTC_CLIENT,	//for brokering webrtc connections, doesn't carry any actual game data itself.
		TCPC_WEBRTC_HOST	//for brokering webrtc connections, doesn't carry any actual game data itself.
#endif
	} clienttype;
	qbyte inbuffer[MAX_OVERALLMSGLEN];
	qbyte outbuffer[MAX_OVERALLMSGLEN];
	vfsfile_t *dlfile;		//if the client looked like an http client, this is the file that they're downloading.
	float timeouttime;
	qboolean pinging;
	netadr_t remoteaddr;
	struct ftenet_tcpconnect_stream_s *next;

	SOCKET socketnum;	//for select. not otherwise used.

	int fakesequence;	//TCPC_WEBSOCKETNQ

#ifdef HAVE_HTTPSV
	struct
	{
		qboolean connection_close;
	} httpstate;
#ifdef MVD_RECORDING
	qtvpendingstate_t qtvstate;
#endif
	struct
	{
		char resource[32];
		int clientnum;
#ifdef SUPPORT_RTC_ICE
		struct icestate_s	*ice;
#endif
	} webrtc;
#endif
} ftenet_tcpconnect_stream_t;

typedef struct {
	ftenet_generic_connection_t generic;
	qboolean tls;

	int active;
	ftenet_tcpconnect_stream_t *tcpstreams;
} ftenet_tcpconnect_connection_t;

void tobase64(unsigned char *out, int outlen, unsigned char *in, int inlen)
{
	static unsigned char tab[64] =
	{
		'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
		'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
		'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
		'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
	};
	unsigned int usedbits = 0;
	unsigned int val = 0;
	outlen--;
	while(inlen)
	{
		while(usedbits < 24 && inlen)
		{
			val <<= 8;
			val |= (*in++);
			inlen--;
			usedbits += 8;
		}
		if (outlen < 4)
			return;
		val <<= 24 - usedbits;

		*out++ = (usedbits > 0)?tab[(val>>18)&0x3f]:'=';
		*out++ = (usedbits > 6)?tab[(val>>12)&0x3f]:'=';
		*out++ = (usedbits > 12)?tab[(val>>6)&0x3f]:'=';
		*out++ = (usedbits > 18)?tab[(val>>0)&0x3f]:'=';
		val=0;
		usedbits = 0;
	}
	*out = 0;
}

neterr_t FTENET_TCPConnect_WebSocket_Splurge(ftenet_tcpconnect_stream_t *st, qbyte packettype, const qbyte *data, unsigned int length)
{
	/*as a server, we don't need the mask stuff*/
	unsigned short ctrl = 0x8000 | (packettype<<8);
	unsigned int paylen = 0;
	unsigned int payoffs = st->outlen;
	int i;
	switch((ctrl>>8) & 0xf)
	{
	case 1:
		for (i = 0; i < length; i++)
		{
			paylen += (((char*)data)[i] == 0 || ((unsigned char*)data)[i] >= 0x80)?2:1;
		}
		break;
	default:
		paylen = length;
		break;
	}
	if (paylen >= 126)
		ctrl |= 126;
	else
		ctrl |= paylen;

	if (6 + paylen > sizeof(st->outbuffer))
		return NETERR_MTU;
	if (payoffs + 6 + paylen > sizeof(st->outbuffer))
		return NETERR_CLOGGED;

	st->outbuffer[payoffs++] = ctrl>>8;
	st->outbuffer[payoffs++] = ctrl&0xff;
	if ((ctrl&0x7f) == 126)
	{
		st->outbuffer[payoffs++] = paylen>>8;
		st->outbuffer[payoffs++] = paylen&0xff;
	}
	switch((ctrl>>8) & 0xf)
	{
	case 1:/*utf8ify the data*/
		for (i = 0; i < length; i++)
		{
			if (!((unsigned char*)data)[i])
			{	/*0 is encoded as 0x100 to avoid safety checks*/
				st->outbuffer[payoffs++] = 0xc0 | (0x100>>6);
				st->outbuffer[payoffs++] = 0x80 | (0x100&0x3f);
			}
			else if (((unsigned char*)data)[i] >= 0x80)
			{	/*larger bytes require markup*/
				st->outbuffer[payoffs++] = 0xc0 | (((unsigned char*)data)[i]>>6);
				st->outbuffer[payoffs++] = 0x80 | (((unsigned char*)data)[i]&0x3f);
			}
			else
			{	/*lower 7 bits are as-is*/
				st->outbuffer[payoffs++] = ((char*)data)[i];
			}
		}
		break;
	default: //raw data
		memcpy(st->outbuffer+payoffs, data, length);
		payoffs += length;
		break;
	}
	st->outlen = payoffs;


	if (st->outlen)
	{	/*try and flush the old data*/
		int done;
		done = VFS_WRITE(st->clientstream, st->outbuffer, st->outlen);
		if (done > 0)
		{
			memmove(st->outbuffer, st->outbuffer + done, st->outlen - done);
			st->outlen -= done;
		}
	}
	return NETERR_SENT;
}

#ifdef HAVE_HTTPSV
enum
{
	WCATTR_METHOD,
	WCATTR_URL,
	WCATTR_HTTP,
	WCATTR_HOST,
	WCATTR_CONNECTION,

	WCATTR_UPGRADE,
	WCATTR_WSKEY,
	//WCATTR_ORIGIN,
	WCATTR_WSPROTO,
	//WCATTR_WSEXT,

	WCATTR_IFNONEMATCH,

	WCATTR_COUNT,

	WCATTR_WSVER,
	WCATTR_CONTENT_LENGTH,
	WCATTR_ACCEPT_ENCODING,
	WCATTR_TRANSFER_ENCODING
};
typedef char httparg_t[64];
#include "fs.h"
#ifdef _WIN32
#include "resource.h"
#endif
void SV_UserCmdMVDList_HTML (vfsfile_t *pipe);
qboolean FTENET_TCPConnect_HTTPResponse(ftenet_tcpconnect_stream_t *st, httparg_t arg[WCATTR_COUNT], qboolean allowgzip)
{
	char adr[256];
	int i;
	const char *filetype = NULL;
	const char *resp = NULL;	//response headers (no length/gap)
	const char *body = NULL;	//response body
	int method;
	if (!strcmp(arg[WCATTR_METHOD], "GET"))
		method = 0;
	else if (!strcmp(arg[WCATTR_METHOD], "HEAD"))
		method = 1;
	else //if (!strcmp(arg[WCATTR_METHOD], "POST") || !strcmp(arg[WCATTR_METHOD], "PUT") || !strcmp(arg[WCATTR_METHOD], "OPTIONS"))
	{
		method = 404;
		resp =	"HTTP/1.1 405 Method Not Allowed\r\n";
		body = NULL;
	}

	st->dlfile = NULL;
	if (!resp && *arg[WCATTR_URL] == '/')
	{	//'can't use SV_LocateDownload, as that assumes an active client.
		const char *name = arg[WCATTR_URL]+1;
		char *extraheaders = "";
		time_t modificationtime = 0;
		char *query = strchr(arg[WCATTR_URL]+1, '?');
#ifdef HAVE_SERVER
		func_t func = 0;
#endif
		if (query)
			*query++ = 0;

		//FIXME: remove ?
		//FIXME: any path that ends with / should be sent to index.html or something
		if (!*name)
			name = "index.html";

#ifdef HAVE_SERVER
		if (sv.state && svs.gametype == GT_PROGS && svprogfuncs)
			func = svprogfuncs->FindFunction(svprogfuncs, "HTTP_GeneratePage", PR_ANY);

		if (func)
		{
			void *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
			((string_t *)pr_globals)[OFS_PARM0] = svprogfuncs->TempString(svprogfuncs, query?va("%s?%s", name, query):name);
			((string_t *)pr_globals)[OFS_PARM1] = svprogfuncs->TempString(svprogfuncs, arg[WCATTR_METHOD]);
			((string_t *)pr_globals)[OFS_PARM2] = 0;	//we don't support any postdata at this time.
			((string_t *)pr_globals)[OFS_PARM3] = 0;	//we don't support any request headers at this time.
			((string_t *)pr_globals)[OFS_PARM4] = 0;	//we don't have any default response headers yet.
			((string_t *)pr_globals)[OFS_PARM5] = 0;
			((string_t *)pr_globals)[OFS_PARM6] = 0;
			((string_t *)pr_globals)[OFS_PARM7] = 0;
			svprogfuncs->ExecuteProgram(svprogfuncs, func);

			if (((string_t *)pr_globals)[OFS_RETURN])
			{	//note that "" is not null
				body = svprogfuncs->StringToNative(svprogfuncs, ((string_t *)pr_globals)[OFS_RETURN]);
				resp = svprogfuncs->StringToNative(svprogfuncs, ((string_t *)pr_globals)[OFS_PARM4]);
				resp = va("%s%s", *body?"HTTP/1.1 200 Ok\r\n":"HTTP/1.1 404 File Not Found\r\n", resp);
			}
		}
#endif

		//FIXME: provide some resource->filename mapping that allows various misc files.

		if (body)
			;
#ifdef _WIN32
		else if (!strcmp(name, "favicon.ico"))
		{	//we can serve up the icon from the exe. we just have to reformat it a little.
			st->dlfile = VFSPIPE_Open(1, false);
			if (st->dlfile)
			{
				struct
				{
					short reserved;
					short type;
					short count;
					struct
					{
						qbyte width;
						qbyte height;
						qbyte colours;
						qbyte reserved;
						short planes;
						short bitcount;
						unsigned short bytes[2];
						unsigned short fileoffset[2];
					} entry[1];
				} icohdr;
				struct
				{
					short reserved;
					short type;
					short count;
					struct
					{
						qbyte width;
						qbyte height;
						qbyte colours;
						qbyte reserved;
						short planes;
						short bitcount;
						unsigned short bytes[2];
						unsigned short id;
					} entry[1];
				} *grphdr = LockResource(LoadResource(NULL, FindResource(NULL, MAKEINTRESOURCE(IDI_ICON1), RT_GROUP_ICON)));
				void *blob;
				//fixme: scan for the best icon size to use.

				icohdr.reserved = 0;
				icohdr.type = 1;//type
				icohdr.count = countof(icohdr.entry);//count
				icohdr.entry[0].width = grphdr->entry[0].width;
				icohdr.entry[0].height = grphdr->entry[0].height;
				icohdr.entry[0].colours = grphdr->entry[0].colours;
				icohdr.entry[0].reserved = grphdr->entry[0].reserved;
				icohdr.entry[0].planes = grphdr->entry[0].planes;
				icohdr.entry[0].bitcount = grphdr->entry[0].bitcount;
				icohdr.entry[0].bytes[0] = grphdr->entry[0].bytes[0];
				icohdr.entry[0].bytes[1] = grphdr->entry[0].bytes[1];
				icohdr.entry[0].fileoffset[0] = sizeof(icohdr);
				icohdr.entry[0].fileoffset[1] = sizeof(icohdr)>>16;
				VFS_WRITE(st->dlfile, &icohdr, sizeof(icohdr));

				blob = LockResource(LoadResource(NULL, FindResource(NULL, MAKEINTRESOURCE(grphdr->entry[0].id), RT_ICON)));

				VFS_WRITE(st->dlfile, blob, grphdr->entry[0].bytes[0] | (grphdr->entry[0].bytes[1]<<16));

				resp = NULL;
				body = NULL;
			}
		}
#endif
#if defined(SV_MASTER) && !defined(HAVE_SERVER)
		else if ((st->dlfile=SVM_GenerateIndex(name)))
			;
#endif
#ifdef HAVE_SERVER
		else if (!strcmp(name, "index.html"))
		{
			resp = "HTTP/1.1 200 Ok\r\n"
				"Content-Type: text/html\r\n";

			body = va(
				"<html lang='en-us'>"
				"<head>"
				"<meta charset='utf-8'>"
				"<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>"
				"<meta name=viewport content='width=device-width, initial-scale=1'>"
#ifdef _WIN32
				"<link rel='icon' type='image/vnd.microsoft.icon' href='/favicon.ico' />"
#else
				"<link rel='icon' type='image/vnd.microsoft.icon' href='"ENGINEWEBSITE"/favicon.ico' />"
#endif
				"<title>%s - %s</title>"
				"<style>"
					"body { background-color:#000000; color:#808080; height:100%%;width:100%%;margin:0;padding:0;}"
				"</style>"
				"</head>"
				"<body>"
				"<iframe name='steve'"
					" src='%s/%s?+connect%%20%s%s' allowfullscreen=true"
					" frameborder='0' scrolling='no' marginheight='0' marginwidth='0' width='100%%' height='100%%'"
					" onerror=\"alert('Failed to load engine')\">"
				"</iframe>"
				"</body>"
				"</html>"
				,fs_manifest->formalname, hostname.string, ENGINEWEBSITE, fs_manifest->installation, (st->remoteaddr.prot==NP_TLS)?"wss://":"ws://", arg[WCATTR_HOST]);
		}
		/*else if (!strcmp(name, "default.fmf") && (st->dlfile = FS_OpenVFS("default.fmf", "rb", FS_ROOT)))
		{
			resp =	"HTTP/1.1 200 Ok\r\n"
					"Content-Type: application/x-ftemanifest\r\n";
			body = NULL;
		}*/
#ifdef MVD_RECORDING
		else if (!Q_strncasecmp(name, "demolist", 8))
		{
			filetype = "text/html";
			st->dlfile = VFSPIPE_Open(1, false);
			if (st->dlfile)
				SV_UserCmdMVDList_HTML(st->dlfile);
		}
		else if (!Q_strncasecmp(name, "demonum/", 8))
		{
			char *mvdname = SV_MVDNum(arg[WCATTR_METHOD], sizeof(arg[WCATTR_METHOD]), atoi(name+8));
			if (mvdname)
			{
				Con_Printf("Redirect %s to %s (copyrighted)\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				resp = va(	"HTTP/1.1 302 Found\r\n"
						"Location: /demos/%s\r\n"
						"Content-Type: text/html\r\n"
						, mvdname);
				body = NULL;
			}
		}
#endif
		else if (!SV_AllowDownload(name))
		{
			Con_Printf("Denied download of %s to %s\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
			resp =	"HTTP/1.1 403 Forbidden\r\n"
					"Content-Type: text/html\r\n";
			body = va("File \"%s\" may not be downloaded"
					, name);
		}
		else if (!Q_strncasecmp(name, "package/", 8))
		{
			if (FS_GetPackageDownloadable(name+8))
				st->dlfile = FS_OpenVFS(name+8, "rb", FS_ROOT);
			else
			{
				Con_Printf("Unable to download %s to %s (copyrighted)\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				resp = "HTTP/1.1 403 Forbidden\r\n"
						"Content-Type: text/html\r\n";
				body = "File is flagged as copyrighted";
			}
		}
		else
		{
			flocation_t gzloc;
			flocation_t rawloc;
#ifdef MVD_RECORDING
			if (!Q_strncasecmp(name, "demos/", 6))
				name = va("%s/%s", sv_demoDir.string, name+6);
#endif

			if (FS_FLocateFile(name, FSLF_IFFOUND, &rawloc))
			{
				char gzname[MAX_QPATH];
				time_t rt;
				FS_GetLocMTime(&rawloc, &rt);
				Q_snprintfz(gzname, sizeof(gzname), "%s.gz", name);
				if (allowgzip && FS_FLocateFile(gzname, FSLF_IFFOUND, &gzloc))
				{
					time_t gt;
					if (rawloc.search == gzloc.search && FS_GetLocMTime(&gzloc, &gt) && gt >= rt)
					{	//must be in the same gamedir, and not older
						extraheaders = "Content-Encoding: gzip\r\n";
						rawloc = gzloc;
						rt = gt;
						Con_DPrintf("HTTP: Serving %s instead\n", gzname);
					}
					else
						Con_DPrintf("HTTP: Ignoring %s, outdated\n", gzname);
				}

				modificationtime = rt;

				if (rawloc.search->flags & SPF_COPYPROTECTED)
				{
					Con_Printf("Unable to download %s to %s (copyrighted)\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					resp =	"HTTP/1.1 403 Forbidden\r\n"
							"Content-Type: text/html\r\n";

					body = va("File %s inside a package<br/><a href=\"/package/%s\">Download</a>"
							, name, FS_GetPackageDownloadFilename(&rawloc));
				}
				else
					st->dlfile = rawloc.search->handle->OpenVFS(rawloc.search->handle, &rawloc, "rb");
			}
			else
				st->dlfile = NULL;
		}
#endif
		if (st->dlfile)
		{
			char etag[64];
			if (!filetype)
			{
				char ext[64];
				int i;
				static const char *mimes[] =
				{
					"html", "text/html",
					"htm", "text/html",
					"png", "image/png",
					"jpeg", "image/jpeg",
					"jpg", "image/jpeg",
					"ico", "image/vnd.microsoft.icon",
					"pk3", "application/zip",
					"fmf", "application/x-ftemanifest",
					"qtv", "application/x-qtv",

					"mvd", "application/x-multiviewdemo",
					"mvd.gz", "application/x-multiviewdemo",
					"qwd", "application/x-multiviewdemo",
					"qwd.gz", "application/x-multiviewdemo",
					"dem", "application/x-multiviewdemo",
					"dem.gz", "application/x-multiviewdemo",
				};
				COM_FileExtension (name, ext, sizeof(ext));
				for (i = 0; i < countof(mimes); i+=2)
				{
					if (!Q_strcasecmp(ext, mimes[i]))
					{
						filetype = mimes[i+1];
						break;
					}
				}
			}

			if (modificationtime)
			{
				Q_snprintfz(etag, sizeof(etag), "W/\"%0"PRIxQOFS"\"", (qofs_t)modificationtime);
				if (!strcmp(arg[WCATTR_IFNONEMATCH], etag))
				{
					resp =	"HTTP/1.1 304 Not Modified \r\n";
					body = NULL;
				}

				Q_snprintfz(etag, sizeof(etag), "ETag: W/\"%0"PRIxQOFS"\"\r\n", (qofs_t)modificationtime);
			}

			if (resp)
			{
				VFS_CLOSE(st->dlfile);
				st->dlfile = NULL;
			}
			else
			{
				Con_Printf("Downloading %s to %s\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				if (filetype)
				{
					resp =	va("HTTP/1.1 200 Ok\r\n"
							"Content-Type: %s\r\n"
							"%s%s",
							filetype,
							etag,extraheaders);
				}
				else
					resp =	va("HTTP/1.1 200 Ok\r\n"
							"%s%s",
							etag,extraheaders);
				body = NULL;
			}
		}
		else if (!resp)
		{
			Con_Printf("Unable to download %s to %s\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
			resp =		"HTTP/1.1 404 File Not Found\r\n"
						"Content-Type: text/html\r\n";
			body = 		"File not found";
		}
	}
	if (!resp)
	{
		Con_Printf("Invalid download request %s to %s\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
		resp =		"HTTP/1.1 404 File Not Found\r\n"
					"Content-Type: text/html\r\n";
		body =		"This is a Quake WebSocket server, not an http server.<br/>\r\n"
					"<a href='"ENGINEWEBSITE"'>"FULLENGINENAME"</a>";
	}

	st->clienttype = TCPC_HTTPCLIENT;

	i = strlen(resp);
	if (st->outlen + i > sizeof(st->outbuffer))
		return false;
	memcpy(st->outbuffer+st->outlen, resp, i);
	st->outlen+= i;

	resp = "Access-Control-Allow-Origin: *\r\n";
	i = strlen(resp);
	if (st->outlen + i > sizeof(st->outbuffer))
		return false;
	memcpy(st->outbuffer+st->outlen, resp, i);
	st->outlen+= i;

	if (st->dlfile || body)
	{
		qofs_t size;
		if (body)
			size = strlen(body);
		else
			size = VFS_GETLEN(st->dlfile);
		resp = adr;
		Q_snprintfz(adr, sizeof(adr), "Content-Length: %"PRIuQOFS"\r\n", size);
	}
	else
		resp = "Content-Length: 0\r\n";
	i = strlen(resp);
	if (st->outlen + i > sizeof(st->outbuffer))
		return false;
	memcpy(st->outbuffer+st->outlen, resp, i);
	st->outlen+= i;

	if (st->outlen + 2 > sizeof(st->outbuffer))
		return false;
	memcpy(st->outbuffer+st->outlen, "\r\n", 2);
	st->outlen+= 2;

	if (method == 1)
	{	//body is not included in HEAD responses
		body = NULL;
		if (st->dlfile)
			VFS_CLOSE(st->dlfile);
		st->dlfile = NULL;
	}
	else if (body)
	{
		i = strlen(body);
		if (st->outlen + i > sizeof(st->outbuffer))
			return false;
		memcpy(st->outbuffer+st->outlen, body, i);
		st->outlen+= i;
	}
	return true;
}

void FTENET_TCPConnect_WebRTCServerAssigned(ftenet_tcpconnect_stream_t *list, ftenet_tcpconnect_stream_t *client, ftenet_tcpconnect_stream_t *server)
{
	qbyte buffer[5];
	int trynext = 0;
	ftenet_tcpconnect_stream_t *o;
	if (client->webrtc.clientnum < 0)
		client->webrtc.clientnum = 0;
	for(;;)
	{
		for (o = list; o; o = o->next)
		{
			if (o != client && o->clienttype == TCPC_WEBRTC_CLIENT && !strcmp(o->webrtc.resource, client->webrtc.resource) && client->webrtc.clientnum == o->webrtc.clientnum)
				break;
		}
		if (!o)
			break;
		client->webrtc.clientnum = trynext++;
	}

	if (server)
	{	//and tell them both, if the server is actually up
		buffer[0] = 2;
		buffer[1] = (client->webrtc.clientnum>>0)&0xff;
		buffer[2] = (client->webrtc.clientnum>>8)&0xff;
		buffer[3] = (client->webrtc.clientnum>>16)&0xff;
		buffer[4] = (client->webrtc.clientnum>>24)&0xff;
		FTENET_TCPConnect_WebSocket_Splurge(server, 2, buffer, 5);
		FTENET_TCPConnect_WebSocket_Splurge(client, 2, "\x02\xff\xff\xff\xff", 5);
	}
}

qboolean FTENET_TCP_ParseHTTPRequest(ftenet_tcpconnect_connection_t *con, ftenet_tcpconnect_stream_t *st)
{
	char *resp;
	char adr[256];
	int i, j;
	int attr = 0;
	int alen = 0;
	qboolean headerscomplete = false;
	int contentlen = 0;
	int websocketver = 0;
	qboolean acceptsgzip = false;
	qboolean sendingweirdness = false;
	char arg[WCATTR_COUNT][64];

	if (!net_enable_http.ival && !net_enable_websockets.ival && !net_enable_webrtcbroker.ival)
	{
		//we need to respond, firefox will create 10 different connections if we just close it
		resp = va(	"HTTP/1.1 403 Forbidden\r\n"
					"Connection: close\r\n"	//let the client know that any pipelining it was doing will have been ignored
					"\r\n");
		VFS_WRITE(st->clientstream, resp, strlen(resp));
		return false;
	}

	for (i = 0; i < WCATTR_COUNT; i++)
		arg[i][0] = 0;
	for (i = 0; i < st->inlen; i++)
	{
		if (alen == 63)
		{
			Con_Printf("http request overflow from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
			//we need to respond, firefox will create 10 different connections if we just close it
			resp = va(	"HTTP/1.1 414 URI Too Long\r\n"
						"Connection: close\r\n"	//let the client know that any pipelining it was doing will have been ignored
						"\r\n");
			VFS_WRITE(st->clientstream, resp, strlen(resp));
			return false;	//overflow...
		}
		if (st->inbuffer[i] == ' ' || st->inbuffer[i] == '\t' || st->inbuffer[i] == '\r')
		{
			arg[attr][alen++] = 0;
			alen=0;
			if (attr++ == WCATTR_HTTP)
				break;

			for (; i < st->inlen && (st->inbuffer[i] == ' ' || st->inbuffer[i] == '\t' || st->inbuffer[i] == '\r'); i++)
				;
			if (i == st->inlen)
				break;
		}
		if (st->inbuffer[i] == '\n')
		{
			arg[attr][alen++] = 0;
			alen=0;
			break;
		}
		if (st->inbuffer[i] < ' ' && st->inbuffer[i] != '\t')
		{
			Con_Printf("http request contained control codes\n");
			return false;
		}
		arg[attr][alen++] = st->inbuffer[i];
	}
	if (!*arg[WCATTR_URL])	//don't bug out if it was truncated.
		strcpy(arg[WCATTR_URL], "/");

	if (st->inbuffer[i] == '\r')
		i++;
	if (st->inbuffer[i] == '\n')
	{	//okay, we have at least a line... try scanning the rest of the header for known key:value pairs, and see if we can reach the end
		i++;

		attr = 0;
		j = i;
		for (; i < st->inlen; i++)
		{
			if ((i+1 < st->inlen && st->inbuffer[i] == '\r' && st->inbuffer[i+1] == '\n') ||
				(i < st->inlen && st->inbuffer[i] == '\n'))
			{
				if (st->inbuffer[i] == '\n')
					i++;
				else
					i+=2;
				headerscomplete = true;
				break;
			}

			for (; i < st->inlen && (st->inbuffer[i] == ' ' || st->inbuffer[i] == '\t'); i++)
				;
			if (i == st->inlen)
				break;

			for (j = i; j < st->inlen; j++)
			{
				if (st->inbuffer[j] == ':' || st->inbuffer[j] == '\n')
				{
					/*set j to the end of the word, going back past whitespace*/
					while (j > i && (st->inbuffer[j-1] == ' ' || st->inbuffer[i-1] == '\t'))
						j--;
					break;
				}
			}
			if (j-i == 4 && !strnicmp(&st->inbuffer[i], "Host", 4))
				attr = WCATTR_HOST;
			else if (j-i == 7 && !strnicmp(&st->inbuffer[i], "Upgrade", 7))
				attr = WCATTR_UPGRADE;
			else if (j-i == 10 && !strnicmp(&st->inbuffer[i], "Connection", 10))
				attr = WCATTR_CONNECTION;
			//websocket stuff
			else if (j-i == 17 && !strnicmp(&st->inbuffer[i], "Sec-WebSocket-Key", 17))
				attr = WCATTR_WSKEY;
			else if (j-i == 21 && !strnicmp(&st->inbuffer[i], "Sec-WebSocket-Version", 21))
				attr = WCATTR_WSVER;
//			else if (j-i == 6 && !strnicmp(&st->inbuffer[i], "Origin", j-i))
//				attr = WCATTR_ORIGIN;
			else if (j-i == 22 && !strnicmp(&st->inbuffer[i], "Sec-WebSocket-Protocol", 22))
				attr = WCATTR_WSPROTO;
//			else if (j-i == 24 && !strnicmp(&st->inbuffer[i], "Sec-WebSocket-Extensions", 24))
//				attr = WCATTR_WSEXT;
			//http stuff
			else if (j-i == 14 && !strnicmp(&st->inbuffer[i], "Content-Length", 14))
				attr = WCATTR_CONTENT_LENGTH;	//in case they're trying to post/put stuff
			else if (j-i == 15 && !strnicmp(&st->inbuffer[i], "Accept-Encoding", 15))
				attr = WCATTR_ACCEPT_ENCODING;	//for gzip
			else if (j-i == 17 && !strnicmp(&st->inbuffer[i], "Transfer-Encoding", 17))
				attr = WCATTR_TRANSFER_ENCODING;//in case they're trying to post/put complex stuff
			else if (j-i == 13 && !strnicmp(&st->inbuffer[i], "If-None-Match", 13))
				attr = WCATTR_IFNONEMATCH;//for clientside caches
			else
				attr = 0;

			i = j;
			/*skip over the whitespace at the end*/
			for (; i < st->inlen && (st->inbuffer[i] == ' ' || st->inbuffer[i] == '\t'); i++)
				;
			if (i < st->inlen && st->inbuffer[i] == ':')
			{
				i++;
				for (; i < st->inlen && (st->inbuffer[i] == ' ' || st->inbuffer[i] == '\t'); i++)
					;
				j = i;

				//FIXME: check for control codes. although probably not a problem in this part
				for (; i < st->inlen && st->inbuffer[i] != '\n'; i++)
					;
				if (i > j && st->inbuffer[i-1] == '\r')
					i--;
				if (attr)
				{
					switch(attr)
					{
					case WCATTR_CONTENT_LENGTH:
						contentlen = atoi(&st->inbuffer[j]);
						break;
					case WCATTR_ACCEPT_ENCODING:
						while (j < i)
						{
							if (st->inbuffer[j] == ' ' || st->inbuffer[j] == '\t')
							{
								j++;
								continue;
							}
							else if (j+4 <= i && !strncmp(&st->inbuffer[j], "gzip", 4) && (j+4==i || st->inbuffer[j+4] == ';' || st->inbuffer[j+4] == ','))
								acceptsgzip = true;

							while (j < i && st->inbuffer[j] != ',')
								j++;
							if (j < i && st->inbuffer[j] == ',')
								j++;
						}
						break;
					case WCATTR_TRANSFER_ENCODING:
						sendingweirdness = true;	//doesn't matter what it is, we can't handle it.
						break;
					case WCATTR_WSVER:
						websocketver = atoi(&st->inbuffer[j]);
						break;
					default:
						Q_strncpyz(arg[attr], &st->inbuffer[j], (i-j > 63)?64:(i - j + 1));
						break;
					}
				}
				if (i < st->inlen && st->inbuffer[i] == '\r')
					i++;
			}
			else
			{
				/*just a word on the line on its own. that would be invalid in http*/
				return false;
			}
		}
	}

	if (!headerscomplete)
	{
		Con_Printf("http header parsing failed\n");
		return false;	//the caller said it was complete! something's fucked if we're here
	}

	//okay, the above code parsed all the headers that we care about.

	if (contentlen && i+contentlen > st->inlen)
	{	//request isn't complete yet
		if (i+contentlen > sizeof(st->inbuffer)-1)
		{
			resp = va(	"HTTP/1.1 413 Payload Too Large \r\n"
						"Connection: close\r\n"	//let the client know that any pipelining it was doing will have been ignored
						"\r\n");
			VFS_WRITE(st->clientstream, resp, strlen(resp));
			Con_Printf("http oversize request\n");
			return false;	//can never be completed.
		}
		return true;
	}

	//clients uploading chunked stuff is bad/unsupported.
	if (sendingweirdness)
	{
		resp = va(	"HTTP/1.1 413 Payload Too Large \r\n"
					"Connection: close\r\n"	//let the client know that any pipelining it was doing will have been ignored
					"\r\n");
		VFS_WRITE(st->clientstream, resp, strlen(resp));
		Con_Printf("http encoded request\n");
		return false;	//can't handle the request, so discard the connection
	}

	memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
	st->inlen -= i;

	//for websocket connections:
	//must be a Host, Upgrade=websocket, Connection=Upgrade, Sec-WebSocket-Key=base64(randbytes(16)), Sec-WebSocket-Version=13
	//optionally will be Origin=url, Sec-WebSocket-Protocol=FTEWebSocket, Sec-WebSocket-Extensions
	//other fields will be ignored.
	if (!stricmp(arg[WCATTR_UPGRADE], "websocket") && (!stricmp(arg[WCATTR_CONNECTION], "Upgrade") || !stricmp(arg[WCATTR_CONNECTION], "keep-alive, Upgrade")))
	{
		if (!net_enable_websockets.ival && !net_enable_webrtcbroker.ival)
			return false;
		if (websocketver != 13)
		{
			Con_Printf("Outdated websocket request for \"%s\" from \"%s\". got version %i, expected version 13\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), websocketver);

			resp = va(	"HTTP/1.1 426 Upgrade Required\r\n"
						"Sec-WebSocket-Version: 13\r\n"
						"Connection: close\r\n"	//let the client know that any pipelining it was doing will have been ignored
						"\r\n");
			VFS_WRITE(st->clientstream, resp, strlen(resp));
			return false;
		}
		else
		{
			qboolean fail = false;
			char acceptkey[20*2];
			unsigned char sha1digest[20];
			char *blurgh;
			char *protoname = "";

			blurgh = va("%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", arg[WCATTR_WSKEY]);
			tobase64(acceptkey, sizeof(acceptkey), sha1digest, SHA1(sha1digest, sizeof(sha1digest), blurgh, strlen(blurgh)));

			if (st->remoteaddr.prot == NP_TLS)
				st->remoteaddr.prot = NP_WSS;
			else
				st->remoteaddr.prot = NP_WS;
			Con_Printf("Websocket request for %s from %s (%s)\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), arg[WCATTR_WSPROTO]);

			protoname = va("Sec-WebSocket-Protocol: %s\r\n", arg[WCATTR_WSPROTO]);

			//the choice of protocol affects the type of response that we give rather than anything mystic
			if (!strcmp(arg[WCATTR_WSPROTO], "quake"))
				st->clienttype = TCPC_WEBSOCKETNQ;	//raw nq data, all reliable, for compat with webquake
			else if (!strcmp(arg[WCATTR_WSPROTO], "rtc_client"))
				st->clienttype = TCPC_WEBRTC_CLIENT;//not a real client.
			else if (!strcmp(arg[WCATTR_WSPROTO], "rtc_host"))
				st->clienttype = TCPC_WEBRTC_HOST;//not a real client, but a competing server! oh noes!
			else if (!strcmp(arg[WCATTR_WSPROTO], "binary"))
				st->clienttype = TCPC_WEBSOCKETB;	//emscripten's networking libraries insists on 'binary', but we stopped using that a while back because its hostname->ip stuff was flawed.
			else if (!strcmp(arg[WCATTR_WSPROTO], "fteqw"))
				st->clienttype = TCPC_WEBSOCKETB;	//specific custom protocol name to avoid ambiguities.
			else
			{
				st->clienttype = TCPC_WEBSOCKETU;	//nacl supports only utf-8 encoded data, at least at the time I implemented it.
				protoname = "";
			}

			switch(st->clienttype)
			{
			case TCPC_WEBSOCKETNQ:
			case TCPC_WEBSOCKETU:
			case TCPC_WEBSOCKETB:
				if (!net_enable_websockets.ival)
					fail = true;
				break;
			case TCPC_WEBRTC_HOST:
			case TCPC_WEBRTC_CLIENT:
				if (!net_enable_webrtcbroker.ival)
					fail = true;
				break;
			default:
				return false;
			}

			if (*arg[WCATTR_URL] == '/')
				Q_strncpyz(st->webrtc.resource, arg[WCATTR_URL]+1, sizeof(st->webrtc.resource));
			else
				Q_strncpyz(st->webrtc.resource, arg[WCATTR_URL], sizeof(st->webrtc.resource));
			st->webrtc.clientnum = -1;
#ifndef SUPPORT_RTC_ICE
			if (st->clienttype == TCPC_WEBRTC_CLIENT && !*st->webrtc.resource)
				fail = true;
#endif

			if (fail)
				return false;

			resp = va(	"HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
						"Upgrade: websocket\r\n"
						"Connection: Upgrade\r\n"
						"Access-Control-Allow-Origin: *\r\n"	//allow cross-origin requests. this means you can use any domain to play on any public server.
						"Sec-WebSocket-Accept: %s\r\n"
						"%s"
						"\r\n", acceptkey, protoname);
			//send the websocket handshake response.
			VFS_WRITE(st->clientstream, resp, strlen(resp));

			if (st->clienttype == TCPC_WEBRTC_CLIENT && !*st->webrtc.resource)
			{	//client should be connected to us rather than any impostors. tell it to start its ICE handshake.
				FTENET_TCPConnect_WebSocket_Splurge(st, 2, "\x02\xff\xff\xff\xff", 5);
			}
			else if (st->clienttype == TCPC_WEBRTC_HOST || st->clienttype == TCPC_WEBRTC_CLIENT)
			{
				ftenet_tcpconnect_stream_t *o;
				if (st->clienttype == TCPC_WEBRTC_HOST)
				{	//if its a server, then let it know its final resource name
					if (!*st->webrtc.resource)
					{	//webrtc servers need some unique resource address. lets use their ip+port for now. we should probably be randomising this
						Q_snprintfz(st->webrtc.resource, sizeof(st->webrtc.resource), "%s", adr);
					}

					for (o = con->tcpstreams; o; o = o->next)
					{
						if (o != st && o->clienttype == TCPC_WEBRTC_HOST && !strcmp(st->webrtc.resource, o->webrtc.resource))
							return false;	//conflict! can't have two servers listening on the same url
					}

					net_message_buffer[0] = 1;
					net_message_buffer[1] = 0xff;
					net_message_buffer[2] = 0xff;
					strcpy(net_message_buffer+3, st->webrtc.resource);
					FTENET_TCPConnect_WebSocket_Splurge(st, 2, net_message_buffer, strlen(net_message_buffer));

					//if we have (inactive) clients connected, assign them (and let them know that they need to start handshaking)
					for (o = con->tcpstreams; o; o = o->next)
					{
						if (o->clienttype == TCPC_WEBRTC_CLIENT && !strcmp(st->webrtc.resource, o->webrtc.resource))
							FTENET_TCPConnect_WebRTCServerAssigned(con->tcpstreams, o, st);
					}
				}
				else
				{	//find its server, if we can
					for (o = con->tcpstreams; o; o = o->next)
					{
						if (o->clienttype == TCPC_WEBRTC_HOST && !strcmp(st->webrtc.resource, o->webrtc.resource))
							break;
					}
					//and assign it to this client
					FTENET_TCPConnect_WebRTCServerAssigned(con->tcpstreams, st, o);
				}
			}

			//and the connection is okay

			if (st->clienttype == TCPC_WEBSOCKETNQ)
			{
				//inject a connection request so that our server actually accepts them...
				net_message.cursize = 0;
				net_message.packing = SZ_RAWBYTES;
				net_message.currentbit = 0;
				net_from = st->remoteaddr;
				MSG_WriteLong(&net_message, LongSwap(NETFLAG_CTL | (strlen(NQ_NETCHAN_GAMENAME)+7)));
				MSG_WriteByte(&net_message, CCREQ_CONNECT);
				MSG_WriteString(&net_message, NQ_NETCHAN_GAMENAME);
				MSG_WriteByte(&net_message, NQ_NETCHAN_VERSION);
			}
			return true;
		}
	}
	else
	{
		if (!net_enable_http.ival)
			return false;
		return FTENET_TCPConnect_HTTPResponse(st, arg, acceptsgzip);
	}
}
#endif
#if defined(HAVE_SSL) && (defined(HAVE_SERVER) || defined(HAVE_HTTPSV))
static int QDECL TLSPromoteRead (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	if (bytestoread > net_message.cursize)
		bytestoread = net_message.cursize;
	memcpy(buffer, net_message_buffer, bytestoread);
	net_message.cursize -= bytestoread;
	memmove(net_message_buffer, net_message_buffer+bytestoread, net_message.cursize);
	return bytestoread;
}
#endif
void FTENET_TCPConnect_PrintStatus(ftenet_generic_connection_t *gcon)
{
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	ftenet_tcpconnect_stream_t *st;
	char adr[MAX_QPATH];
	if (!con->tcpstreams)
		return;
	for (st = con->tcpstreams; st; st = st->next)
	{
		NET_AdrToString(adr, sizeof(adr), &st->remoteaddr);
		switch(st->clienttype)
		{
		case TCPC_UNKNOWN:	//note: this is often a pending http client that's waiting on the off-chance of having more requests to send
			Con_Printf("handshaking %s\n", adr);
			break;
		case TCPC_QIZMO:
			Con_Printf("qizmo %s\n", adr);
			break;
#ifdef HAVE_HTTPSV
		case TCPC_WEBSOCKETU:
		case TCPC_WEBSOCKETB:
		case TCPC_WEBSOCKETNQ:
			Con_Printf("websocket %s\n", adr);
			break;
		case TCPC_HTTPCLIENT:
			Con_Printf("http %s\n", adr);
			break;
		case TCPC_WEBRTC_CLIENT:
			Con_Printf("webrtc client %s/%s\n", adr, st->webrtc.resource);
			break;
		case TCPC_WEBRTC_HOST:
			Con_Printf("webrtc host %s/%s\n", adr, st->webrtc.resource);
			break;
#endif
		}
	}
}
qboolean FTENET_TCPConnect_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	int ret;
	char		adr[MAX_ADR_SIZE];
	struct sockaddr_qstorage	from;
	int fromlen;

	float timeval = Sys_DoubleTime();
	ftenet_tcpconnect_stream_t *st;
	st = con->tcpstreams;

	//remove any stale ones
	while (con->tcpstreams && con->tcpstreams->clientstream == NULL)
	{
		st = con->tcpstreams;
		con->tcpstreams = con->tcpstreams->next;
		BZ_Free(st);
	}

	for (st = con->tcpstreams; st; st = st->next)
	{//client receiving only via tcp

		while (st->next && st->next->clientstream == NULL)
		{
			ftenet_tcpconnect_stream_t *temp;
			temp = st->next;
			st->next = st->next->next;
			BZ_Free(temp);
			con->active--;
		}

//due to the above checks about invalid sockets, the socket is always open for st below.

		if (st->timeouttime < timeval)
		{
#ifdef HAVE_HTTPSV
			if (!st->pinging && (st->clienttype==TCPC_WEBRTC_CLIENT||st->clienttype==TCPC_WEBRTC_HOST) && *st->webrtc.resource)
			{	//ping broker clients. there usually shouldn't be any data flow to keep it active otherwise.
				st->timeouttime = timeval + 30;
				st->pinging = true;

				FTENET_TCPConnect_WebSocket_Splurge(st, 0x9, "ping", 4);
			}
			else
#endif
			{
				Con_DPrintf ("tcp peer %s timed out\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				goto closesvstream;
			}
		}

		ret = VFS_READ(st->clientstream, st->inbuffer+st->inlen, sizeof(st->inbuffer)-1-st->inlen);
		if (ret < 0)
		{
			Con_Printf ("tcp peer %s closed connection\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
closesvstream:
			if (st->clientstream)
				VFS_CLOSE(st->clientstream);
			st->clientstream = NULL;
			continue;
		}
		st->inlen += ret;

		switch(st->clienttype)
		{
		case TCPC_UNKNOWN:
			if (st->inlen < 6)
				continue;

#if defined(HAVE_SSL) && (defined(HAVE_SERVER) || defined(HAVE_HTTPSV))	//if its non-ascii, then try and upgrade the connection to tls
			if (net_enable_tls.ival && con->generic.islisten && st->remoteaddr.prot == NP_STREAM && st->clientstream && !((st->inbuffer[0] >= 'a' && st->inbuffer[0] <= 'z') || (st->inbuffer[0] >= 'A' && st->inbuffer[0] <= 'Z')))
			{
				//copy off our buffer so we can read it into the tls stream's buffer instead.
				char tmpbuf[256];
				vfsfile_t *stream = st->clientstream;
				int (QDECL *realread) (struct vfsfile_s *file, void *buffer, int bytestoread);
				realread = stream->ReadBytes;
				stream->ReadBytes = TLSPromoteRead;
				memcpy(net_message_buffer, st->inbuffer, st->inlen);
				net_message.cursize = st->inlen;
				//wrap the stream now
				st->clientstream = FS_OpenSSL(NULL, st->clientstream, true);
				st->remoteaddr.prot = NP_TLS;
				if (st->clientstream)
				{
					//try and reclaim it all
					st->inlen = VFS_READ(st->clientstream, st->inbuffer, sizeof(st->inbuffer)-1);
					//make sure we actually read from the proper stream again
					stream->ReadBytes = realread;
				}
				if (!st->clientstream || net_message.cursize)
					goto closesvstream;	//something cocked up. we didn't give the tls stream all the data.
				if (developer.ival)
					Con_Printf("promoted peer to tls: %s\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &st->remoteaddr));
				net_message.cursize = 0;
				continue;
			}
#endif

			if (!strncmp(st->inbuffer, "qizmo\n", 6))
			{
				if (
#ifdef HAVE_SERVER
						net_enable_qizmo.ival ||
#endif
						!con->generic.islisten)
				{
					memmove(st->inbuffer, st->inbuffer+6, st->inlen - (6));
					st->inlen -= 6;
					st->clienttype = TCPC_QIZMO;
					if (con->generic.islisten)
					{
						//send the qizmo handshake response.
						VFS_WRITE(st->clientstream, "qizmo\n", 6);
					}
				}
				else
					goto closesvstream;
			}else
#ifdef HAVE_HTTPSV
			if (con->generic.islisten)// && !strncmp(st->inbuffer, "GET ", 4))
			{
				//qtv or http request header. these terminate with a blank line.
				int i = 0;
				qboolean headerscomplete = false;

				for (; i < st->inlen; i++)
				{
					//we're at the start of a line, so if its a \r\n or a \n then its a blank line, and the headers are complete
					if ((i+1 < st->inlen && st->inbuffer[i] == '\r' && st->inbuffer[i+1] == '\n') ||
						(i < st->inlen && st->inbuffer[i] == '\n'))
					{
						if (st->inbuffer[i] == '\n')
							i++;
						else
							i+=2;
						headerscomplete = true;
						break;
					}

					for (; i < st->inlen && st->inbuffer[i] != '\n'; i++)
						;
				}

				if (headerscomplete)
				{
#ifdef MVD_RECORDING
					//for QTV connections, we just need the method and a blank line. our qtv parser will parse the actual headers.
					if (!Q_strncasecmp(st->inbuffer, "QTV", 3))
					{
						int r = net_enable_qtv.ival?SV_MVD_GotQTVRequest(st->clientstream, st->inbuffer, st->inbuffer+st->inlen, &st->qtvstate):-1;
						i = st->inlen;
						memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
						st->inlen -= i;
						switch(r)
						{
						case -1:
							goto closesvstream;
						case 0:
							continue;
						case 1:
							st->clientstream = NULL;
							continue;
						}
					}
					else
#endif
					{
						net_message.cursize = 0;
						if (!FTENET_TCP_ParseHTTPRequest(con, st))
							goto closesvstream;

						if (net_message.cursize > 0)
							return true;
					}
					continue;
				}
			}else
#endif
			{
				Con_DPrintf ("Unknown TCP handshake from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				goto closesvstream;
			}

			break;
#ifdef HAVE_HTTPSV
		case TCPC_HTTPCLIENT:
			if (st->outlen)
			{	/*try and flush the old data*/
				int done;
				done = VFS_WRITE(st->clientstream, st->outbuffer, st->outlen);
				if (done > 0)
				{
					memmove(st->outbuffer, st->outbuffer + done, st->outlen - done);
					st->outlen -= done;

					st->timeouttime = timeval + 30;
				}
			}
			if (!st->outlen)
			{
				if (st->dlfile)
					st->outlen = VFS_READ(st->dlfile, st->outbuffer, sizeof(st->outbuffer));
				else
					st->outlen = 0;
				if (st->outlen <= 0)
				{
					if (st->dlfile)
						VFS_CLOSE(st->dlfile);
					st->dlfile = NULL;
					st->clienttype = TCPC_UNKNOWN;
					Con_DPrintf ("Outgoing file transfer complete\n");
					if (st->httpstate.connection_close)
						goto closesvstream;
				}
			}
			continue;
#endif
		case TCPC_QIZMO:
			if (st->inlen < 2)
				continue;

			net_message.cursize = BigShort(*(short*)st->inbuffer);
			if (net_message.cursize >= sizeof(net_message_buffer) )
			{
				Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				goto closesvstream;
			}
			if (net_message.cursize+2 > st->inlen)
			{	//not enough buffered to read a packet out of it.
				continue;
			}

			memcpy(net_message_buffer, st->inbuffer+2, net_message.cursize);
			memmove(st->inbuffer, st->inbuffer+net_message.cursize+2, st->inlen - (net_message.cursize+2));
			st->inlen -= net_message.cursize+2;

			net_message.packing = SZ_RAWBYTES;
			net_message.currentbit = 0;
			net_from = st->remoteaddr;

			return true;
#ifdef HAVE_HTTPSV
		case TCPC_WEBSOCKETU:
		case TCPC_WEBSOCKETB:
		case TCPC_WEBSOCKETNQ:
		case TCPC_WEBRTC_HOST:
		case TCPC_WEBRTC_CLIENT:
			while (st->inlen >= 2)
			{
				unsigned short ctrl = ((unsigned char*)st->inbuffer)[0]<<8 | ((unsigned char*)st->inbuffer)[1];
				unsigned long paylen;
				unsigned int payoffs = 2;
				unsigned int mask = 0;
//				st->inbuffer[st->inlen]=0;
				if (ctrl & 0x7000)
				{
					Con_Printf ("%s: reserved bits set\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					goto closesvstream;
				}
				if ((ctrl & 0x7f) == 127)
				{
					quint64_t ullpaylen;
					//as a payload is not allowed to be encoded as too large a type, and quakeworld never used packets larger than 1450 bytes anyway, this code isn't needed (65k is the max even without this)
					if (sizeof(ullpaylen) < 8)
					{
						Con_Printf ("%s: payload frame too large\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
						goto closesvstream;
					}
					else
					{
						if (payoffs + 8 > st->inlen)
							break;
						ullpaylen =
							(quint64_t)((unsigned char*)st->inbuffer)[payoffs+0]<<56u |
							(quint64_t)((unsigned char*)st->inbuffer)[payoffs+1]<<48u |
							(quint64_t)((unsigned char*)st->inbuffer)[payoffs+2]<<40u |
							(quint64_t)((unsigned char*)st->inbuffer)[payoffs+3]<<32u |
							(quint64_t)((unsigned char*)st->inbuffer)[payoffs+4]<<24u |
							(quint64_t)((unsigned char*)st->inbuffer)[payoffs+5]<<16u |
							(quint64_t)((unsigned char*)st->inbuffer)[payoffs+6]<< 8u |
							(quint64_t)((unsigned char*)st->inbuffer)[payoffs+7]<< 0u;
						if (ullpaylen < 0x10000)
						{
							Con_Printf ("%s: payload size (%"PRIu64") encoded badly\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), ullpaylen);
							goto closesvstream;
						}
						if (ullpaylen > 0x40000)
						{
							Con_Printf ("%s: payload size (%"PRIu64") is abusive\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), ullpaylen);
							goto closesvstream;
						}
						paylen = ullpaylen;
						payoffs += 8;
					}
				}
				else if ((ctrl & 0x7f) == 126)
				{
					if (payoffs + 2 > st->inlen)
						break;
					paylen =
						((unsigned char*)st->inbuffer)[payoffs+0]<<8 |
						((unsigned char*)st->inbuffer)[payoffs+1]<<0;
					if (paylen < 126)
					{
						Con_Printf ("%s: payload size encoded badly\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
						goto closesvstream;
					}
					payoffs += 2;
				}
				else
				{
					paylen = ctrl & 0x7f;
				}
				if (ctrl & 0x80)
				{
					if (payoffs + 4 > st->inlen)
						break;
					/*this might read data that isn't set yet, but should be safe*/
					((unsigned char*)&mask)[0] = ((unsigned char*)st->inbuffer)[payoffs+0];
					((unsigned char*)&mask)[1] = ((unsigned char*)st->inbuffer)[payoffs+1];
					((unsigned char*)&mask)[2] = ((unsigned char*)st->inbuffer)[payoffs+2];
					((unsigned char*)&mask)[3] = ((unsigned char*)st->inbuffer)[payoffs+3];
					payoffs += 4;
				}
				/*if there isn't space, try again next time around*/
				if (payoffs + paylen > st->inlen)
				{
					if (payoffs + paylen >= sizeof(st->inbuffer)-1)
					{
						Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
						goto closesvstream;	//can't ever complete
					}
					break;
				}

				if (mask)
				{
					int i;
					for (i = 0; i < paylen; i++)
					{
						((unsigned char*)st->inbuffer)[i + payoffs] ^= ((unsigned char*)&mask)[i&3];
					}
				}

				net_message.cursize = 0;

				switch((ctrl>>8) & 0xf)
				{
				case 0x0:	/*continuation*/
					Con_Printf ("websocket continuation frame from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					goto closesvstream;
				case 0x1:	/*text frame*/
//					Con_Printf ("websocket text frame from %s\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
					{
						/*text frames are pure utf-8 chars, no dodgy encodings or anything, all pre-checked...
						  except we're trying to send binary data.
						  so we need to unmask things (char 0 is encoded as 0x100 - truncate it)
						*/
						unsigned char *in = st->inbuffer+payoffs, *out = net_message_buffer;
						int len = paylen;
						while(len && out < net_message_buffer + sizeof(net_message_buffer))
						{
							if ((*in & 0xe0)==0xc0 && len > 1)
							{
								*out = ((in[0] & 0x1f)<<6) | ((in[1] & 0x3f)<<0);
								in+=2;
								len -= 2;
							}
							else if (*in & 0x80)
							{
								*out = '?';
								in++;
								len -= 1;
							}
							else
							{
								*out = in[0];
								in++;
								len -= 1;
							}
							out++;
						}
						net_message.cursize = out - net_message_buffer;
					}
					break;
				case 0x2: /*binary frame*/
//					Con_Printf ("websocket binary frame from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					net_message.cursize = paylen;
					if (net_message.cursize+8 >= sizeof(net_message_buffer) )
					{
						Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
						goto closesvstream;
					}
#ifdef SUPPORT_RTC_ICE
					if (st->clienttype == TCPC_WEBRTC_CLIENT && !*st->webrtc.resource)
					{	//this is a client that's corrected directly to us via webrtc.
						//FIXME: we don't support dtls, so browers will bitch about our sdp.
						if (paylen+1 < sizeof(net_message_buffer))
						{
							net_message_buffer[paylen] = 0;
							memcpy(net_message_buffer, st->inbuffer+payoffs, paylen);

							if (!st->webrtc.ice)	//if the ice state isn't established yet, do that now.
								st->webrtc.ice = iceapi.ICE_Create(NULL, "test", "rtc://foo", ICEM_ICE, ICEP_QWSERVER);
							iceapi.ICE_Set(st->webrtc.ice, "sdp", net_message_buffer);

							if (iceapi.ICE_Get(st->webrtc.ice, "sdp", net_message_buffer, sizeof(net_message_buffer)))
								FTENET_TCPConnect_WebSocket_Splurge(st, 2, net_message_buffer, strlen(net_message_buffer));
						}
						net_message.cursize = 0;
					}
					else
#endif
						if ((st->clienttype == TCPC_WEBRTC_CLIENT || st->clienttype == TCPC_WEBRTC_HOST) && paylen >= 3)
					{	//we're brokering a client+server. all messages should be unicasts between a client and its host, matched by resource.
						ftenet_tcpconnect_stream_t *o;
						int clnum = (st->inbuffer[payoffs+1]<<0)|(st->inbuffer[payoffs+2]<<8);
						int type = (st->clienttype != TCPC_WEBRTC_CLIENT)?TCPC_WEBRTC_CLIENT:TCPC_WEBRTC_HOST;
						for (o = con->tcpstreams; o; o = o->next)
						{
							if (o->clienttype == type && clnum == o->webrtc.clientnum && !strcmp(o->webrtc.resource, st->webrtc.resource))
							{
								st->inbuffer[payoffs+1] = (st->webrtc.clientnum>>0)&0xff;
								st->inbuffer[payoffs+2] = (st->webrtc.clientnum>>8)&0xff;
								FTENET_TCPConnect_WebSocket_Splurge(o, 2, st->inbuffer+payoffs, paylen);
								break;
							}
						}
						net_message.cursize = 0;
					}
					else
#ifdef NQPROT
						if (st->clienttype == TCPC_WEBSOCKETNQ)
					{	//hack in an 8-byte header
						payoffs+=1;
						paylen-=1;
						memcpy(net_message_buffer+8, st->inbuffer+payoffs, paylen);
						net_message.cursize=paylen+8;
						((int*)net_message_buffer)[0] = BigLong(NETFLAG_UNRELIABLE | net_message.cursize);
						((int*)net_message_buffer)[1] = LongSwap(++st->fakesequence);
					}
					else
#endif
						memcpy(net_message_buffer, st->inbuffer+payoffs, paylen);
					break;
				case 0x8:	/*connection close*/
					Con_Printf ("websocket closure %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					goto closesvstream;
				case 0x9:	/*ping*/
//					Con_Printf ("websocket ping from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					if (FTENET_TCPConnect_WebSocket_Splurge(st, 0xa, st->inbuffer+payoffs, paylen) != NETERR_SENT)
						goto closesvstream;
					break;
				case 0xa: /*pong*/
					st->timeouttime = Sys_DoubleTime() + 30;
					st->pinging = false;
//					Con_Printf ("websocket pong from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					break;
				default:
					Con_Printf ("Unsupported websocket opcode (%i) from %s\n", (ctrl>>8) & 0xf, NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					goto closesvstream;
				}

				memmove(st->inbuffer, st->inbuffer+payoffs + paylen, st->inlen - (payoffs + paylen));
				st->inlen -= payoffs + paylen;

				if (net_message.cursize)
				{
					net_message.packing = SZ_RAWBYTES;
					net_message.currentbit = 0;
					net_from = st->remoteaddr;
					return true;
				}
			}
			break;
#endif
		}
	}

	if (con->generic.thesocket != INVALID_SOCKET && con->active < 256)
	{
		int newsock;
		fromlen = sizeof(from);
		newsock = accept(con->generic.thesocket, (struct sockaddr*)&from, &fromlen);
		if (newsock != INVALID_SOCKET)
		{
			char tmpbuf[256];
			int _true = true;
			ioctlsocket(newsock, FIONBIO, (u_long *)&_true);
			setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));

			con->active++;
			st = Z_Malloc(sizeof(*con->tcpstreams));
			/*grab the net address*/
			SockadrToNetadr(&from, fromlen, &st->remoteaddr);
			if (developer.ival)
				Con_Printf("new TCP connection from %s\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &st->remoteaddr));
			st->clienttype = TCPC_UNKNOWN;
			st->next = con->tcpstreams;
			con->tcpstreams = st;
			st->socketnum = newsock;
			st->clientstream = FS_OpenTCPSocket(newsock, false, NET_AdrToString(tmpbuf, sizeof(tmpbuf), &st->remoteaddr));
			st->inlen = 0;

#ifdef HAVE_SSL
			if (con->tls && st->clientstream)	//if we're meant to be using tls, wrap the stream in a tls connection
			{
				st->clientstream = FS_OpenSSL(NULL, st->clientstream, true);
				/*sockadr doesn't contain transport info, so fix that up here*/
				st->remoteaddr.prot = NP_TLS;
			}
			else
#endif
			{
				/*sockadr doesn't contain transport info, so fix that up here*/
				st->remoteaddr.prot = NP_STREAM;
			}

			st->timeouttime = timeval + 30;
		}
	}
	return false;
}

neterr_t FTENET_TCPConnect_SendPacket(ftenet_generic_connection_t *gcon, int length, const void *data, netadr_t *to)
{
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	ftenet_tcpconnect_stream_t *st;

	for (st = con->tcpstreams; st; st = st->next)
	{
		if (st->clientstream == NULL)
			continue;

		if (NET_CompareAdr(to, &st->remoteaddr))
		{
			if (!st->outlen)
			{
				switch(st->clienttype)
				{
				case TCPC_QIZMO:
					{
						unsigned short slen = BigShort((unsigned short)length);
						if (length > 0xffff)
							return NETERR_MTU;
						if (st->outlen + sizeof(slen) + length > sizeof(st->outbuffer))
						{
							if (length+sizeof(slen) > sizeof(st->outbuffer))
								return NETERR_MTU;
							Con_DPrintf("FTENET_TCPConnect_SendPacket: outgoing overflow\n");
							return NETERR_CLOGGED;
						}
						else
						{
							memcpy(st->outbuffer + st->outlen, &slen, sizeof(slen));
							memcpy(st->outbuffer + st->outlen + sizeof(slen), data, length);
							st->outlen += sizeof(slen) + length;
						}
					}
					break;
#ifdef HAVE_HTTPSV
				case TCPC_WEBSOCKETNQ:
					if (length < 8 || ((char*)data)[0] & 0x80)
						break;
//					length = 2;
//					data = "\1\1";
					length-=7;
					data=(char*)data + 7;
					*(char*)data = 1;	//for compat with webquake, we add an extra byte at the start. 1 for reliable, 2 for unreliable.
					//fallthrough
				case TCPC_WEBSOCKETU:
				case TCPC_WEBSOCKETB:
					{
						neterr_t e = FTENET_TCPConnect_WebSocket_Splurge(st, (st->clienttype==TCPC_WEBSOCKETU)?1:2, data, length);
						if (e != NETERR_SENT)
							return e;
					}
					break;
#endif
				default:
					break;
				}
			}

			if (st->outlen)
			{	/*try and flush the old data*/
				int done;
				done = VFS_WRITE(st->clientstream, st->outbuffer, st->outlen);
				if (done > 0)
				{
					memmove(st->outbuffer, st->outbuffer + done, st->outlen - done);
					st->outlen -= done;
				}
			}

			st->timeouttime = Sys_DoubleTime() + 20;

			return NETERR_SENT;
		}
	}
	return NETERR_NOROUTE;
}

static int FTENET_TCPConnect_GetLocalAddresses(struct ftenet_generic_connection_s *gcon, unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	netproto_t prot = con->tls?NP_TLS:NP_STREAM;
	int i, r = FTENET_Generic_GetLocalAddresses(gcon, adrflags, addresses, maxaddresses);
	for (i = 0; i < r; i++)
	{
		addresses[i].prot = prot;
	}
	return r;
}

static qboolean FTENET_TCPConnect_ChangeLocalAddress(struct ftenet_generic_connection_s *con, netadr_t *adr)
{
	//if we're a server, we want to try switching listening tcp port without shutting down all other connections.
	//yes, this might mean we leave a connection active on the old port, but oh well.
	int addrsize, addrsize2;
	int family;
	struct sockaddr_qstorage qs;
	struct sockaddr_qstorage cur;
	netadr_t n;
	SOCKET newsocket;
	unsigned long _true = true;
	int sysprot;

	addrsize = NetadrToSockadr(adr, &qs);
	family = ((struct sockaddr*)&qs)->sa_family;

	switch(adr->type)
	{
#if defined(HAVE_IPV4) || defined(HAVE_IPV6)
	case NA_IP:
	case NA_IPV6:
		sysprot = IPPROTO_TCP;
		break;
#endif
#ifdef HAVE_IPX
	case NA_IPX:
		sysprot = NSPROTO_IPX;
		break;
#endif
	default:
		sysprot = 0;
		break;
	}

	if (con->thesocket != INVALID_SOCKET)
	{
		addrsize2 = sizeof(cur);
		getsockname(con->thesocket, (struct sockaddr *)&cur, &addrsize2);

		if (addrsize == addrsize2)
		{
			SockadrToNetadr(&cur, addrsize2, &n);
			if (NET_CompareAdr(adr, &n))	//the address+port we're trying is already current, apparently.
				return true;
		}

		closesocket(con->thesocket);
		con->thesocket = INVALID_SOCKET;
	}

#if defined(HAVE_IPV6) && defined(IPV6_V6ONLY)
	if (family == AF_INET && net_hybriddualstack.ival && !((struct sockaddr_in*)&qs)->sin_addr.s_addr)
	{	//hybrid sockets pathway takes over when INADDR_ANY
		unsigned long _false = false;
		if ((newsocket = socket (AF_INET6, SOCK_STREAM, sysprot)) != INVALID_SOCKET)
		{
			if (0 == setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_false, sizeof(_false)))
			{
				memset(&n, 0, sizeof(n));
				n.type = NA_IPV6;
				n.port = adr->port;
				n.scopeid = adr->scopeid;
				addrsize2 = NetadrToSockadr(&n, &cur);

				if ((bind(newsocket, (struct sockaddr *)&cur, addrsize2) != INVALID_SOCKET) &&
					(listen(newsocket, 2) != INVALID_SOCKET))
				{
					if (ioctlsocket (newsocket, FIONBIO, &_true) != -1)
					{
						setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));

						con->addrtype[0] = NA_IP;
						con->addrtype[1] = NA_IPV6;
						con->thesocket = newsocket;
						return true;
					}
				}
			}
			closesocket(newsocket);
		}
	}
#endif

	if ((newsocket = socket (family, SOCK_STREAM, sysprot)) != INVALID_SOCKET)
	{
#ifdef UNIXSOCKETS
		if (family == AF_UNIX)
		{
			struct sockaddr_un *un = (struct sockaddr_un *)&qs;
			struct stat s;
			if (*un->sun_path)
			{	//non-abstract sockets don't clean up the filesystem when the socket is closed
				//and we can't re-bind to it while it still exists.
				//so standard practise is to delete it before the bind.
				//we do want to make sure the file is actually a socket before we remove it (so people can't abuse stuffcmds)
				if (stat(un->sun_path, &s)!=-1)
				{
					if ((s.st_mode & S_IFMT) == S_IFSOCK)
						unlink(un->sun_path);
				}
			}
		}
#endif

		if ((bind(newsocket, (struct sockaddr *)&qs, addrsize) != INVALID_SOCKET) &&
			(listen(newsocket, 2) != INVALID_SOCKET))
		{
			if (ioctlsocket (newsocket, FIONBIO, &_true) != -1)
			{
#ifdef UNIXSOCKETS
				if (family != NA_UNIX)
#endif
					setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));

				con->thesocket = newsocket;
				return true;
			}
		}
		closesocket(newsocket);
	}
	return false;
}
static void FTENET_TCPConnect_Close(ftenet_generic_connection_t *gcon)
{
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	ftenet_tcpconnect_stream_t *st;

	st = con->tcpstreams;
	while (con->tcpstreams)
	{
		st = con->tcpstreams;
		con->tcpstreams = st->next;

		if (st->clientstream != NULL)
			VFS_CLOSE(st->clientstream);

		BZ_Free(st);
	}

	FTENET_Generic_Close(gcon);
}

#ifdef HAVE_PACKET
int FTENET_TCPConnect_SetFDSets(ftenet_generic_connection_t *gcon, fd_set *readfdset, fd_set *writefdset)
{
	int maxfd = 0;
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	ftenet_tcpconnect_stream_t *st;

	for (st = con->tcpstreams; st; st = st->next)
	{
#ifdef SUPPORT_RTC_ICE
		if (st->webrtc.ice)
		{
			while(iceapi.ICE_GetLCandidateSDP(st->webrtc.ice, net_message_buffer, sizeof(net_message_buffer)))
			{
				FTENET_TCPConnect_WebSocket_Splurge(st, 2, net_message_buffer, strlen(net_message_buffer));
			}
			continue;
		}
#endif
		if (st->clientstream == NULL || st->socketnum == INVALID_SOCKET)
			continue;
#ifdef HAVE_HTTPSV
		if (st->clienttype == TCPC_HTTPCLIENT)
			FD_SET(st->socketnum, writefdset); // network socket
#endif
		FD_SET(st->socketnum, readfdset); // network socket
		if (maxfd < st->socketnum)
			maxfd = st->socketnum;
	}
	if (con->generic.thesocket != INVALID_SOCKET)
	{
		FD_SET(con->generic.thesocket, readfdset); // network socket
		if (maxfd < con->generic.thesocket)
			maxfd = con->generic.thesocket;
	}
	return maxfd;
}
#endif

ftenet_generic_connection_t *FTENET_TCPConnect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_tcpconnect_connection_t *newcon;

	unsigned long _true = true;
	SOCKET newsocket;
	qboolean tls = (adr.prot == NP_TLS || adr.prot == NP_WSS);

#ifndef HAVE_SSL
	if (tls)
	{
		Con_Printf("tls not supported in this build\n");
		return NULL;
	}
#endif

	newcon = Z_Malloc(sizeof(*newcon));
	newcon->generic.thesocket = INVALID_SOCKET;
	newsocket = INVALID_SOCKET;

	newcon->generic.addrtype[0] = adr.type;
	newcon->generic.addrtype[1] = NA_INVALID;

	if (isserver)
	{
#ifdef HAVE_PACKET	//unable to listen on tcp if we have no packet interface
		if (!FTENET_TCPConnect_ChangeLocalAddress(&newcon->generic, &adr))
		{
			Z_Free(newcon);
			return NULL;
		}
#endif
	}
	else
	{
		newsocket = TCP_OpenStream(&adr);
		if (newsocket == INVALID_SOCKET)
		{
			Z_Free(newcon);
			return NULL;
		}

#ifdef UNIXSOCKETS
		if (adr.type != NA_UNIX)
#endif
		{
			//this isn't fatal
			setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));
		}
	}


	if (newcon)
	{
		newcon->tls = tls;
		if (isserver)
		{
			newcon->generic.GetLocalAddresses = FTENET_TCPConnect_GetLocalAddresses;
			newcon->generic.ChangeLocalAddress = FTENET_TCPConnect_ChangeLocalAddress;
		}
		newcon->generic.GetPacket = FTENET_TCPConnect_GetPacket;
		newcon->generic.SendPacket = FTENET_TCPConnect_SendPacket;
		newcon->generic.Close = FTENET_TCPConnect_Close;
#ifdef HAVE_PACKET
		newcon->generic.SetFDSets = FTENET_TCPConnect_SetFDSets;
#endif
		newcon->generic.PrintStatus = FTENET_TCPConnect_PrintStatus;

		newcon->generic.islisten = isserver;

		newcon->active = 0;

		if (!isserver)
		{
			newcon->active++;
			newcon->tcpstreams = Z_Malloc(sizeof(*newcon->tcpstreams));
			newcon->tcpstreams->next = NULL;
			newcon->tcpstreams->socketnum = newsocket;
			newcon->tcpstreams->clientstream = FS_OpenTCPSocket(newsocket, true, address);
			newcon->tcpstreams->inlen = 0;

			newcon->tcpstreams->remoteaddr = adr;

#ifdef HAVE_SSL
			if (newcon->tls)	//if we're meant to be using tls, wrap the stream in a tls connection
				newcon->tcpstreams->clientstream = FS_OpenSSL(address, newcon->tcpstreams->clientstream, false);
#endif

			//send the qizmo greeting.
			newcon->tcpstreams->clienttype = TCPC_UNKNOWN;
			VFS_WRITE(newcon->tcpstreams->clientstream, "qizmo\n", 6);

			newcon->tcpstreams->timeouttime = Sys_DoubleTime() + 30;
		}
		else
		{
			newcon->tcpstreams = NULL;
		}

		return &newcon->generic;
	}
	else
	{
		closesocket(newsocket);
		return NULL;
	}
}

#endif




#ifdef IRCCONNECT

typedef struct ftenet_ircconnect_stream_s {
	char theiruser[16];

	int inlen;
	char inbuffer[1500];
	float timeouttime;
	netadr_t remoteaddr;
	struct ftenet_ircconnect_stream_s *next;
} ftenet_ircconnect_stream_t;

typedef struct {
	ftenet_generic_connection_t generic;

	netadr_t ircserver;

	char incoming[512+1];
	int income;

	char ourusername[16];
	char usechannel[16];

	char outbuf[8192];
	unsigned int outbufcount;

	ftenet_ircconnect_stream_t *streams;
} ftenet_ircconnect_connection_t;

qboolean FTENET_IRCConnect_GetPacket(ftenet_generic_connection_t *gcon)
{
	unsigned char *s, *start, *end, *endl;
	int read;
	unsigned char *from;
	int fromlen;
	int code;
	char adr[128];

	ftenet_ircconnect_connection_t *con = (ftenet_ircconnect_connection_t*)gcon;

	if (con->generic.thesocket == INVALID_SOCKET)
	{
		if (con->income == 0)
		{
			netadr_t ip;
			cvar_t *ircuser = Cvar_Get("ircuser", "none", 0, "IRC Connect");
			cvar_t *ircnick = Cvar_Get("ircnick", "", 0, "IRC Connect");
			cvar_t *ircsomething = Cvar_Get("ircsomething", "moo", 0, "IRC Connect");
			cvar_t *ircclientaddr = Cvar_Get("ircclientaddr", "127.0.0.1", 0, "IRC Connect");

			if (!NET_StringToAdr(con->ircserver.address.irc.host, 6667, &ip))
				return false;
			con->generic.thesocket = TCP_OpenStream(&ip);

			//when hosting, the specified nick is the name we're using.
			//when connecting, the specified nick is the name we're trying to send to, and our own name is inconsequential.
			if (con->generic.islisten && *con->ircserver.address.irc.user)
				Q_strncpyz(con->ourusername, con->ircserver.address.irc.user, sizeof(con->ourusername));
			else
				Q_strncpyz(con->ourusername, ircnick->string, sizeof(con->ourusername));

			if (!*con->ourusername)
			{
				Q_snprintfz(con->ourusername, sizeof(con->ourusername), "fte%x\n", rand());
			}

			send(con->generic.thesocket, "USER ", 5, 0);
			send(con->generic.thesocket, ircuser->string, strlen(ircuser->string), 0);
			send(con->generic.thesocket, " ", 1, 0);
			send(con->generic.thesocket, con->ircserver.address.irc.host, strlen(con->ircserver.address.irc.host), 0);
			send(con->generic.thesocket, " ", 1, 0);
			send(con->generic.thesocket, ircclientaddr->string, strlen(ircclientaddr->string), 0);
			send(con->generic.thesocket, " :", 2, 0);
			send(con->generic.thesocket, ircsomething->string, strlen(ircsomething->string), 0);
			send(con->generic.thesocket, "\r\n", 2, 0);
			send(con->generic.thesocket, "NICK ", 5, 0);
			send(con->generic.thesocket, con->ourusername, strlen(con->ourusername), 0);
			send(con->generic.thesocket, "\r\n", 2, 0);
		}
	}
	else
	{
		read = recv(con->generic.thesocket, con->incoming+con->income, sizeof(con->incoming)-1 - con->income, 0);
		if (read < 0)
		{
			read = neterrno();
			switch(read)
			{
			case NET_ECONNABORTED:
			case NET_ECONNRESET:
				closesocket(con->generic.thesocket);
				con->generic.thesocket = INVALID_SOCKET;
				break;
			default:
				break;
			}

			read = 0;//return false;
		}
		else if (read == 0)	//they disconnected.
		{
			closesocket(con->generic.thesocket);
			con->generic.thesocket = INVALID_SOCKET;
		}

		con->income += read;
		con->incoming[con->income] = 0;
	}

	start = con->incoming;
	end = start+con->income;

	while (start < end)
	{
		endl = NULL;
		for (s = start; s < end; s++)
		{
			if (*s == '\n')
			{
				endl = s;
				break;
			}
		}
		if (endl == NULL)
			//not got a complete command.
			break;

		s = start;
		while(*s == ' ')
			s++;
		if (*s == ':')
		{
			s++;
			from = s;
			while(s<endl && *s != ' ' && *s != '\n')
			{
				s++;
			}
			fromlen = s - from;
		}
		else
		{
			from = NULL;
			fromlen = 0;
		}

		while(*s == ' ')
			s++;
		if (!strncmp(s, "PRIVMSG ", 8))
		{
			unsigned char *dest;

			s+=8;
			while(*s == ' ')
				s++;

			//cap the length
			if (fromlen > sizeof(net_from.address.irc.user)-1)
				fromlen = sizeof(net_from.address.irc.user)-1;
			for (code = 0; code < fromlen; code++)
				if (from[code] == '!')
				{
					fromlen = code;
					break;
				}

			net_from.type = NA_IRC;
			memcpy(net_from.address.irc.user, from, fromlen);
			net_from.address.irc.user[fromlen] = 0;

			dest = s;
			//discard the destination name
			while(s<endl && *s != ' ' && *s != '\n')
			{
				s++;
			}
			if (s-dest >= sizeof(net_from.address.irc.channel))
			{	//no space, just pretend it was direct.
				net_from.address.irc.channel[0] = 0;
			}
			else
			{
				memcpy(net_from.address.irc.channel, dest, s-dest);
				net_from.address.irc.channel[s-dest] = 0;

				if (!strcmp(net_from.address.irc.channel, con->ourusername))
				{	//this was aimed at us. clear the channel.
					net_from.address.irc.channel[0] = 0;
				}
			}

			while(*s == ' ')
				s++;

			if (*s == ':')
			{
				s++;

				if (*s == '!')
				{
					s++;

					/*interpret as a connectionless packet*/
					net_message.cursize = 4 + endl - s;
					if (net_message.cursize >= sizeof(net_message_buffer) )
					{
						Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &net_from));
						break;
					}

					*(unsigned int*)net_message_buffer = ~0;
					memcpy(net_message_buffer+4, s, net_message.cursize);

					net_message.packing = SZ_RAWBYTES;
					net_message.currentbit = 0;

					//clean up the incoming data
					memmove(con->incoming, start, end - (endl+1));
					con->income = end - (endl+1);
					con->incoming[con->income] = 0;

					return true;
				}
				if (*s == '$')
				{
					unsigned char *nstart = s;
					while (*s != '\r' && *s != '\n' && *s != '#' && *s != ' ' && *s != ':')
						s++;
					if (*s == '#')
					{
						if (strncmp(nstart, con->ourusername, strlen(con->ourusername)) || strlen(con->ourusername) != s - nstart)
							while(*s == '#')
								s++;
					}
				}
				if (*s == '#')
				{
					ftenet_ircconnect_stream_t *st;
					int psize;

					for (st = con->streams; st; st = st->next)
					{
						if (!strncmp(st->remoteaddr.address.irc.user, from, fromlen)	&& st->remoteaddr.address.irc.user[fromlen] == 0)
							break;
					}
					if (!st)
					{
						st = Z_Malloc(sizeof(*st));

						st->remoteaddr = net_from;
						st->next = con->streams;
						con->streams = st;
					}

					//skip over the hash
					s++;

					psize = 0;
					if (*s >= 'a' && *s <= 'f')
						psize += *s - 'a' + 10;
					else if (*s >= '0' && *s <= '9')
						psize += *s - '0';
					s++;

					psize*=16;
					if (*s >= 'a' && *s <= 'f')
						psize += *s - 'a' + 10;
					else if (*s >= '0' && *s <= '9')
						psize += *s - '0';
					s++;

					psize*=16;
					if (*s >= 'a' && *s <= 'f')
						psize += *s - 'a' + 10;
					else if (*s >= '0' && *s <= '9')
						psize += *s - '0';
					s++;

					while (s < endl && st->inlen < sizeof(st->inbuffer))
					{
						switch (*s)
						{
						//handle markup
						case '\\':
							s++;
							if (s < endl)
							{
								switch(*s)
								{
								case '\\':
									st->inbuffer[st->inlen++] = *s;
									break;
								case 'n':
									st->inbuffer[st->inlen++] = '\n';
									break;
								case 'r':
									st->inbuffer[st->inlen++] = '\r';
									break;
								case '0':
									st->inbuffer[st->inlen++] = 0;
									break;
								default:
									st->inbuffer[st->inlen++] = '?';
									break;
								}
							}
							break;

						//ignore these
						case '\n':
						case '\r':
						case '\0':	//this one doesn't have to be ignored.
							break;

						//handle normal char
						default:
							st->inbuffer[st->inlen++] = *s;
							break;
						}
						s++;
					}

					if (st->inlen > psize || psize >= sizeof(net_message_buffer) )
					{
						st->inlen = 0;
						Con_Printf ("Corrupt packet from %s\n", NET_AdrToString (adr, sizeof(adr), &net_from));
					}
					else if (st->inlen == psize)
					{
						/*interpret as a connectionless packet*/
						net_message.cursize = st->inlen;
						if (net_message.cursize >= sizeof(net_message_buffer) )
						{
							Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &net_from));
							break;
						}

						memcpy(net_message_buffer, st->inbuffer, net_message.cursize);

						net_message.packing = SZ_RAWBYTES;
						net_message.currentbit = 0;

						st->inlen = 0;

						//clean up the incoming data
						memmove(con->incoming, start, end - (endl+1));
						con->income = end - (endl+1);
						con->incoming[con->income] = 0;

						return true;
					}
				}
			}
		}
		else if (!strncmp(s, "PING ", 5))
		{
			send(con->generic.thesocket, "PONG ", 5, 0);
			send(con->generic.thesocket, s+5, endl - s - 5, 0);
			send(con->generic.thesocket, "\r\n", 2, 0);
		}
		else
		{
			code = strtoul(s, (char **)&s, 10);
			switch (code)
			{
			case   1:
				{
					if (con->ircserver.address.irc.channel)
					{
						send(con->generic.thesocket, "JOIN ", 5, 0);
						send(con->generic.thesocket, con->ircserver.address.irc.channel, strlen(con->ircserver.address.irc.channel), 0);
						send(con->generic.thesocket, "\r\n", 2, 0);
					}
				}
				break;
			case 433:
				//nick already in use
				send(con->generic.thesocket, "NICK ", 5, 0);
				{
					cvar_t *ircnick2 = Cvar_Get("ircnick2", "YIBBLE", 0, "IRC Connect");
					Q_strncpyz(con->ourusername, ircnick2->string, sizeof(con->ourusername));
					send(con->generic.thesocket, con->ourusername, strlen(con->ourusername), 0);
				}
				send(con->generic.thesocket, "\r\n", 2, 0);
				break;
			case 0:
				//non-numerical event.
				break;
			}
		}

		while(*s == ' ')
			s++;

		start = s = endl+1;
	}

	memmove(con->incoming, start, end - start);
	con->income = end - start;
	con->incoming[con->income] = 0;

	if (con->generic.thesocket == INVALID_SOCKET)
		con->income = 0;

	return false;
}
neterr_t FTENET_IRCConnect_SendPacket(ftenet_generic_connection_t *gcon, int length, const void *data, netadr_t *to)
{
	ftenet_ircconnect_connection_t *con = (ftenet_ircconnect_connection_t*)gcon;

	unsigned char *buffer;
	unsigned char *lenofs;
	int packed;
	int fulllen = length;
	int newoutcount;

	for (packed = 0; packed < FTENET_ADDRTYPES; packed++)
		if (to->type == con->generic.addrtype[packed])
			break;
	if (packed == FTENET_ADDRTYPES)
		return NETERR_NOROUTE;

	packed = 0;

	if (con->generic.thesocket == INVALID_SOCKET)
		return NETERR_DISCONNECTED;
/*
	if (*(unsigned int *)data == ~0 && !strchr(data, '\n') && !strchr(data, '\r') && strlen(data) == length)
	{
		if (send(con->generic.thesocket, va("PRIVMSG %s :!", to.address.irc.user), 15, 0) != 15)
			Con_Printf("bad send\n");
		else if (send(con->generic.thesocket, (char*)data+4, length - 4, 0) != length-4)
			Con_Printf("bad send\n");
		else if (send(con->generic.thesocket, "\r\n", 2, 0) != 2)
			Con_Printf("bad send\n");
		return true;
	}
*/
	newoutcount = con->outbufcount;
	if (!con->outbufcount)
	while(length)
	{
		buffer = con->outbuf + newoutcount;

		if (*to->address.irc.channel)
		{
			int unamelen;
			int chanlen;
			unamelen = strlen(to->address.irc.user);
			chanlen = strlen(to->address.irc.channel);
			packed = 8+chanlen+3+unamelen+1 + 3;

			if (packed+1 + newoutcount > sizeof(con->outbuf))
				break;

			memcpy(buffer, "PRIVMSG ", 8);
			memcpy(buffer+8, to->address.irc.channel, chanlen);
			memcpy(buffer+8+chanlen, " :$", 3);
			memcpy(buffer+8+chanlen+3, to->address.irc.user, unamelen);
			memcpy(buffer+8+chanlen+3+unamelen, "#", 1);
			lenofs = buffer+8+chanlen+3+unamelen+1;
			sprintf(lenofs, "%03x", fulllen);

		}
		else
		{
			int unamelen;
			unamelen = strlen(to->address.irc.user);
			packed = 8 + unamelen + 3 + 3;

			if (packed+1 + newoutcount > sizeof(con->outbuf))
				break;

			memcpy(buffer, "PRIVMSG ", 8);
			memcpy(buffer+8, to->address.irc.user, unamelen);
			memcpy(buffer+8+unamelen, " :#", 3);
			lenofs = buffer+8+unamelen+3;
			sprintf(lenofs, "%03x", fulllen);
		}


		while(length && packed < 400 && packed+newoutcount < sizeof(con->outbuf)-2)	//make sure there's always space
		{
			switch(*(unsigned char*)data)
			{
			case '\\':
				buffer[packed++] = '\\';
				buffer[packed++] = '\\';
				break;
			case '\n':
				buffer[packed++] = '\\';
				buffer[packed++] = 'n';
				break;
			case '\r':
				buffer[packed++] = '\\';
				buffer[packed++] = 'r';
				break;
			case '\0':
				buffer[packed++] = '\\';
				buffer[packed++] = '0';
				break;
			default:
				buffer[packed++] = *(unsigned char*)data;
				break;
			}
			length--;
			data = (char*)data + 1;
		}

		buffer[packed++] = '\r';
		buffer[packed++] = '\n';

		newoutcount += packed;
		packed = 0;
	}
	if (!length)
	{
		//only if we flushed all
		con->outbufcount = newoutcount;
	}

	//try and flush it
	length = send(con->generic.thesocket, con->outbuf, con->outbufcount, 0);
	if (length > 0)
	{
		memmove(con->outbuf, con->outbuf+length, con->outbufcount-length);
		con->outbufcount -= length;
	}
	return NETERR_SENT;
}
void FTENET_IRCConnect_Close(ftenet_generic_connection_t *gcon)
{
	ftenet_ircconnect_connection_t *con = (ftenet_ircconnect_connection_t *)gcon;
	ftenet_ircconnect_stream_t *st;

	while(con->streams)
	{
		st = con->streams;
		con->streams = st->next;
		Z_Free(st);
	}

	FTENET_Generic_Close(gcon);
}

struct ftenet_generic_connection_s *FTENET_IRCConnect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_ircconnect_connection_t *newcon;

	if (!NET_StringToAdr(address, 6667, &adr))
		return NULL;	//couldn't resolve the name



	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		newcon->generic.GetPacket = FTENET_IRCConnect_GetPacket;
		newcon->generic.SendPacket = FTENET_IRCConnect_SendPacket;
		newcon->generic.Close = FTENET_IRCConnect_Close;

		newcon->generic.islisten = isserver;
		newcon->generic.addrtype[0] = NA_IRC;
		newcon->generic.addrtype[1] = NA_INVALID;

		newcon->generic.thesocket = INVALID_SOCKET;

		newcon->ircserver = adr;


		return &newcon->generic;
	}
	else
	{
		return NULL;
	}
}


#endif

#ifdef FTE_TARGET_WEB
#include "web/ftejslib.h"

typedef struct
{
	ftenet_generic_connection_t generic;
	int brokersock;	//only if rtc
	netadr_t remoteadr;
	qboolean failed;

	int datasock;	//only if we're a client

	size_t numclients;
	struct
	{
		netadr_t remoteadr;
		int datasock;
	} *clients;
} ftenet_websocket_connection_t;

static void FTENET_WebSocket_Close(ftenet_generic_connection_t *gcon)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	size_t i;
	if (wsc->brokersock != INVALID_SOCKET)
		emscriptenfte_ws_close(wsc->brokersock);
	if (wsc->datasock != INVALID_SOCKET)
		emscriptenfte_ws_close(wsc->datasock);
	for (i = 0; i < wsc->numclients; i++)
	{
		if (wsc->clients[i].datasock != INVALID_SOCKET)
			emscriptenfte_ws_close(wsc->clients[i].datasock);
	}
	free(wsc->clients);
}
static qboolean FTENET_WebSocket_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	net_message.cursize = emscriptenfte_ws_recv(wsc->datasock, net_message_buffer, sizeof(net_message_buffer));
	if (net_message.cursize > 0)
	{
		net_from = wsc->remoteadr;
		return true;
	}
	if ((int)net_message.cursize < 0)
		wsc->failed = true;
	net_message.cursize = 0;//just incase
	return false;
}
static neterr_t FTENET_WebSocket_SendPacket(ftenet_generic_connection_t *gcon, int length, const void *data, netadr_t *to)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	if (wsc->failed)
		return NETERR_DISCONNECTED;
	if (NET_CompareAdr(to, &wsc->remoteadr))
	{
		if (emscriptenfte_ws_send(wsc->datasock, data, length) < 0)
			return NETERR_CLOGGED;
		return NETERR_SENT;
	}
	return NETERR_NOROUTE;
}

//called from the javascript when there was some ice event. just forwards over the broker connection.
static void FTENET_WebRTC_Callback(void *ctxp, int ctxi, int evtype, const char *data)
{
	ftenet_websocket_connection_t *wcs = ctxp;
	size_t dl = strlen(data);
	qbyte *o = net_message_buffer;
	*o++ = evtype;
	*o++ = (ctxi>>0)&0xff;
	*o++ = (ctxi>>8)&0xff;
	memcpy(o, data, dl);
	o+=dl;

//	Con_Printf("To Broker: %i %i\n", evtype, ctxi);
	emscriptenfte_ws_send(wcs->brokersock, net_message_buffer, o-net_message_buffer);
}
static qboolean FTENET_WebRTC_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	size_t i;
	if (!wsc->generic.islisten)
	{
		if (wsc->datasock != INVALID_SOCKET && FTENET_WebSocket_GetPacket(gcon))
			return true;
	}
	else
	{
		for (i = 0; i < wsc->numclients; i++)
		{
			net_message.cursize = emscriptenfte_ws_recv(wsc->clients[i].datasock, net_message_buffer, sizeof(net_message_buffer));
			if (net_message.cursize > 0)
			{
				net_from = wsc->clients[i].remoteadr;
				return true;
			}
		}
	}

	net_message.cursize = emscriptenfte_ws_recv(wsc->brokersock, net_message_buffer, sizeof(net_message_buffer));
	if (net_message.cursize > 0)
	{
		int cmd;
		short cl;
		const char *s;
		char *p;

		MSG_BeginReading(msg_nullnetprim);
		cmd = MSG_ReadByte();
		cl = MSG_ReadShort();

//Con_Printf("From Broker: %i %i\n", cmd, cl);

		switch(cmd)
		{
		case 0:	//connection closing...
			if (cl == -1)
			{
				wsc->failed = true;
				Con_Printf("Broker closing connection: %s\n", MSG_ReadString());
			}
			else if (cl >= 0 && cl < wsc->numclients)
			{
				wsc->clients[cl].remoteadr.type = NA_INVALID;
				if (wsc->clients[cl].datasock != INVALID_SOCKET)
					emscriptenfte_ws_close(wsc->clients[cl].datasock);
				wsc->clients[cl].datasock = INVALID_SOCKET;
				Con_Printf("Broker closing connection: %s\n", MSG_ReadString());
			}
			break;
		case 1:	//reports the trailing url we're 'listening' on. anyone else using that url will connect to us.
			s = MSG_ReadString();
			if (*s == '/')
				s++;
			p = wsc->remoteadr.address.websocketurl;
			while (*p)
			{
				if (p[0] == ':' && p[1] == '/' && p[2] == '/')
					p+=3;
				else if (p[0] == '/')
				{
					*p = 0;
					break;
				}
				else
					p++;
			}
			Q_strncatz(wsc->remoteadr.address.websocketurl, "/", sizeof(wsc->remoteadr.address.websocketurl));
			Q_strncatz(wsc->remoteadr.address.websocketurl, s, sizeof(wsc->remoteadr.address.websocketurl));
			Con_Printf("Listening on %s\n", wsc->remoteadr.address.websocketurl);
			break;
		case 2:	//connection established with a new peer
			if (wsc->generic.islisten)
			{
				if (cl < 1024 && cl >= wsc->numclients)
				{	//looks like a new one... but don't waste memory
					size_t nm;
					nm = cl+1;
					wsc->clients = realloc(wsc->clients, sizeof(*wsc->clients)*nm);
					while(wsc->numclients < nm)
					{
						memset(&wsc->clients[i].remoteadr, 0, sizeof(wsc->clients[i].remoteadr));
						wsc->clients[wsc->numclients++].datasock = INVALID_SOCKET;
					}
				}
				if (cl < wsc->numclients)
				{
					if (wsc->clients[cl].datasock != INVALID_SOCKET)
						emscriptenfte_ws_close(wsc->clients[cl].datasock);
					memcpy(&wsc->clients[cl].remoteadr, &wsc->remoteadr, sizeof(netadr_t));
					wsc->clients[cl].remoteadr.port = htons(cl+1);
					wsc->clients[cl].datasock = emscriptenfte_rtc_create(false, wsc, cl, FTENET_WebRTC_Callback);
				}
			}
			else
			{
				if (wsc->datasock != INVALID_SOCKET)
					emscriptenfte_ws_close(wsc->datasock);
				wsc->datasock = emscriptenfte_rtc_create(true, wsc, cl, FTENET_WebRTC_Callback);
			}
			break;
		case 3:	//we received an offer from a client
			s = MSG_ReadString();
			if (wsc->generic.islisten)
			{
				if (cl < wsc->numclients && wsc->clients[cl].datasock != INVALID_SOCKET)
					emscriptenfte_rtc_offer(wsc->clients[cl].datasock, s, "offer");
			}
			else
			{
				if (wsc->datasock != INVALID_SOCKET)
					emscriptenfte_rtc_offer(wsc->datasock, s, "answer");
			}
			break;
		case 4:
			s = MSG_ReadString();
			if (wsc->generic.islisten)
			{
				if (cl < wsc->numclients && wsc->clients[cl].datasock != INVALID_SOCKET)
					emscriptenfte_rtc_candidate(wsc->clients[cl].datasock, s);
			}
			else
			{
				if (wsc->datasock != INVALID_SOCKET)
					emscriptenfte_rtc_candidate(wsc->datasock, s);
			}
			break;
		}
	}

	net_message.cursize = 0;//just incase
	return false;
}
static neterr_t FTENET_WebRTC_SendPacket(ftenet_generic_connection_t *gcon, int length, const void *data, netadr_t *to)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	size_t i;
	if (!wsc->generic.islisten)
	{
//		if (wsc->failed)
//			return NETERR_DISCONNECTED;
		if (NET_CompareAdr(to, &wsc->remoteadr))
		{
			if (wsc->datasock == INVALID_SOCKET)
				return NETERR_CLOGGED;	//we're still waiting for the broker to give us a server... or for a server to become available.
			else
			{
				if (emscriptenfte_ws_send(wsc->datasock, data, length) <= 0)
					return NETERR_CLOGGED;
				return NETERR_SENT;
			}
		}
	}
	else
	{
		for (i = 0; i < wsc->numclients; i++)
		{
			if (NET_CompareAdr(to, &wsc->clients[i].remoteadr))
			{
				if (emscriptenfte_ws_send(wsc->clients[i].datasock, data, length) <= 0)
					return NETERR_CLOGGED;
				return NETERR_SENT;
			}
		}
	}
	return NETERR_NOROUTE;
}

int FTENET_WebRTC_GetAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
	ftenet_websocket_connection_t *wsc = (void*)con;
	if (maxaddresses)
	{
		*addresses = wsc->remoteadr;
		*adrflags = 0;
		return 1;
	}
	return 0;
}

static ftenet_generic_connection_t *FTENET_WebSocket_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	ftenet_websocket_connection_t *newcon;

	int brokersocket = INVALID_SOCKET;
	int datasocket = INVALID_SOCKET;

	newcon = Z_Malloc(sizeof(*newcon));
	if (adr.prot == NP_DTLS)
	{	//this requires that we create a broker connection
		if (isserver)
			brokersocket = emscriptenfte_ws_connect(adr.address.websocketurl, "rtc_host");
		else
			brokersocket = emscriptenfte_ws_connect(adr.address.websocketurl, "rtc_client");

		newcon->generic.GetPacket = FTENET_WebRTC_GetPacket;
		newcon->generic.SendPacket = FTENET_WebRTC_SendPacket;
		newcon->generic.GetLocalAddresses = FTENET_WebRTC_GetAddresses;
	}
	else
	{
		if (!isserver)
			datasocket = emscriptenfte_ws_connect(adr.address.websocketurl, "fteqw");

		newcon->generic.GetPacket = FTENET_WebSocket_GetPacket;
		newcon->generic.SendPacket = FTENET_WebSocket_SendPacket;
	}
	if (brokersocket == INVALID_SOCKET && datasocket == INVALID_SOCKET)
	{
		Con_Printf("Unable to create rtc/ws connection\n");
		Z_Free(newcon);
	}
	else
	{
		Q_strncpyz(newcon->generic.name, "WebSocket", sizeof(newcon->generic.name));
		newcon->generic.Close = FTENET_WebSocket_Close;

		newcon->generic.islisten = isserver;
		newcon->generic.addrtype[0] = NA_WEBSOCKET;
		newcon->generic.addrtype[1] = NA_INVALID;

		newcon->generic.thesocket = INVALID_SOCKET;
		newcon->brokersock = brokersocket;
		newcon->datasock = datasocket;

		adr.port = 0;
		newcon->remoteadr = adr;

		return &newcon->generic;
	}
	return NULL;
}
#endif


#ifdef NACL
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_websocket.h>
#include <ppapi/c/ppb_var.h>
#include <ppapi/c/ppb_var_array_buffer.h>
#include <ppapi/c/ppb_instance.h>
extern PPB_Core *ppb_core;
extern PPB_WebSocket *ppb_websocket_interface;
extern PPB_Var *ppb_var_interface;
extern PPB_VarArrayBuffer *ppb_vararraybuffer_interface;
extern PP_Instance pp_instance;

typedef struct
{
	ftenet_generic_connection_t generic;

	PP_Resource sock;
	netadr_t remoteadr;

	struct PP_Var incomingpacket;
	qboolean havepacket;

	qboolean failed;
	int showerror;
} ftenet_websocket_connection_t;

static void websocketgot(void *user_data, int32_t result)
{
	ftenet_websocket_connection_t *wsc = user_data;
	if (result == PP_OK)
	{
		wsc->havepacket = true;
	}
	else
	{
		Sys_Printf("%s: %i\n", __func__, result);
		wsc->failed = true;
		wsc->showerror = result;
	}
}
static void websocketconnected(void *user_data, int32_t result)
{
	ftenet_websocket_connection_t *wsc = user_data;
	if (result == PP_OK)
	{
		int res;
		//we got a successful connection, enable reception.
		struct PP_CompletionCallback ccb = {websocketgot, wsc, PP_COMPLETIONCALLBACK_FLAG_OPTIONAL};
		res = ppb_websocket_interface->ReceiveMessage(wsc->sock, &wsc->incomingpacket, ccb);
		if (res != PP_OK_COMPLETIONPENDING)
			websocketgot(wsc, res);
	}
	else
	{
		Sys_Printf("%s: %i\n", __func__, result);
		//some sort of error connecting, make it timeout now
		wsc->failed = true;
		wsc->showerror = result;
	}
}
static void websocketclosed(void *user_data, int32_t result)
{
	ftenet_websocket_connection_t *wsc = user_data;
	if (wsc->havepacket)
	{
		wsc->havepacket = false;
		ppb_var_interface->Release(wsc->incomingpacket);
	}
	ppb_core->ReleaseResource(wsc->sock);
//	Z_Free(wsc);
}

static void FTENET_NaClWebSocket_Close(ftenet_generic_connection_t *gcon)
{
	int res;
	/*meant to free the memory too, in this case we get the callback to do it*/
	ftenet_websocket_connection_t *wsc = (void*)gcon;

	struct PP_CompletionCallback ccb = {websocketclosed, wsc, PP_COMPLETIONCALLBACK_FLAG_NONE};
	ppb_websocket_interface->Close(wsc->sock, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeUndefined(), ccb);
}

static qboolean FTENET_NaClWebSocket_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	int res;
	int len = 0;
	if (wsc->havepacket)
	{
		if (wsc->incomingpacket.type == PP_VARTYPE_ARRAY_BUFFER)
		{
			uint32_t length;
			void *buf = ppb_vararraybuffer_interface->Map(wsc->incomingpacket);
			if (buf && ppb_vararraybuffer_interface->ByteLength(wsc->incomingpacket, &length))
			{
				net_message.cursize = length;
				memcpy(net_message_buffer, buf, length);
				ppb_vararraybuffer_interface->Unmap(wsc->incomingpacket);
			}
			else
				net_message.cursize = 0;
		}
		else
		{
			unsigned char *utf8 = (unsigned char *)ppb_var_interface->VarToUtf8(wsc->incomingpacket, &len);
			unsigned char *out = (unsigned char *)net_message_buffer;

			while(len && out < net_message_buffer + sizeof(net_message_buffer))
			{
				if ((*utf8 & 0xe0)==0xc0 && len > 1)
				{
					*out = ((utf8[0] & 0x1f)<<6) | ((utf8[1] & 0x3f)<<0);
					utf8+=2;
					len -= 2;
				}
				else if (*utf8 & 0x80)
				{
					*out = '?';
					utf8++;
					len -= 1;
				}
				else
				{
					*out = utf8[0];
					utf8++;
					len -= 1;
				}
				out++;
			}
			net_message.cursize = out - net_message_buffer;
		}
		memcpy(&net_from, &wsc->remoteadr, sizeof(net_from));
		wsc->havepacket = false;
		ppb_var_interface->Release(wsc->incomingpacket);

		if (!wsc->failed)
		{
			//get the next one
			struct PP_CompletionCallback ccb = {websocketgot, wsc, PP_COMPLETIONCALLBACK_FLAG_OPTIONAL};
			res = ppb_websocket_interface->ReceiveMessage(wsc->sock, &wsc->incomingpacket, ccb);
			if (res != PP_OK_COMPLETIONPENDING)
				websocketgot(wsc, res);
		}

		if (len)
		{
			char adr[64];
			Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &net_from));
			return false;
		}
		return true;
	}

	if (wsc->showerror != PP_OK)
	{
		switch(wsc->showerror)
		{
		case PP_ERROR_FAILED:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_FAILED\n");
			break;
		case PP_ERROR_ABORTED:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_ABORTED\n");
			break;
		case PP_ERROR_NOTSUPPORTED:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_NOTSUPPORTED\n");
			break;
		case PP_ERROR_CONNECTION_CLOSED:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_CONNECTION_CLOSED\n");
			break;
		case PP_ERROR_CONNECTION_RESET:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_CONNECTION_RESET\n");
			break;
		case PP_ERROR_CONNECTION_REFUSED:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_CONNECTION_REFUSED\n");
			break;
		case PP_ERROR_CONNECTION_ABORTED:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_CONNECTION_ABORTED\n");
			break;
		case PP_ERROR_CONNECTION_FAILED:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_CONNECTION_FAILED\n");
			break;
		case PP_ERROR_CONNECTION_TIMEDOUT:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_CONNECTION_TIMEDOUT\n");
			break;
		case PP_ERROR_ADDRESS_INVALID:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_ADDRESS_INVALID\n");
			break;
		case PP_ERROR_ADDRESS_UNREACHABLE:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_ADDRESS_UNREACHABLE\n");
			break;
		case PP_ERROR_ADDRESS_IN_USE:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: PP_ERROR_ADDRESS_IN_USE\n");
			break;
		default:
			Con_TPrintf ("FTENET_NaClWebSocket_GetPacket: error %i\n", wsc->showerror);
			break;
		}
		wsc->showerror = PP_OK;
	}

	return false;
}
static neterr_t FTENET_NaClWebSocket_SendPacket(ftenet_generic_connection_t *gcon, int length, void *data, netadr_t *to)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	int res;
	if (wsc->failed)
		return NETERR_DISCONNECTED;

#if 1
	struct PP_Var str = ppb_vararraybuffer_interface->Create(length);
	void *out = ppb_vararraybuffer_interface->Map(str);
	if (!out)
		return NETERR_MTU;
	memcpy(out, data, length);
	ppb_vararraybuffer_interface->Unmap(str);
#else
	int outchars = 0;
	unsigned char outdata[length*2+1];
	unsigned char *out=outdata, *in=data;
	while(length-->0)
	{
		if (!*in)
		{
			//sends 256 instead of 0
			*out++ = 0xc0 | (0x100 >> 6);
			*out++ = 0x80 | (0x100 & 0x3f);
		}
		else if (*in >= 0x80)
		{
			*out++ = 0xc0 | (*in >> 6);
			*out++ = 0x80 | (*in & 0x3f);
		}
		else
			*out++ = *in;
		in++;
		outchars++;
	}
	*out = 0;
	struct PP_Var str = ppb_var_interface->VarFromUtf8(outdata, out - outdata);
#endif
	res = ppb_websocket_interface->SendMessage(wsc->sock, str);
//	Sys_Printf("FTENET_WebSocket_SendPacket: result %i\n", res);
	ppb_var_interface->Release(str);
	return NETERR_SENT;
}

/*nacl websockets implementation...*/
static ftenet_generic_connection_t *FTENET_WebSocket_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	ftenet_websocket_connection_t *newcon;

	PP_Resource newsocket;

	if (isserver || !ppb_websocket_interface)
	{
		return NULL;
	}

	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
#define WEBSOCKETPROTOCOL "fteqw"
		struct PP_CompletionCallback ccb = {websocketconnected, newcon, PP_COMPLETIONCALLBACK_FLAG_NONE};
		newsocket = ppb_websocket_interface->Create(pp_instance);
		struct PP_Var str = ppb_var_interface->VarFromUtf8(adr.address.websocketurl, strlen(adr.address.websocketurl));
		struct PP_Var protocols[1] = {ppb_var_interface->VarFromUtf8(WEBSOCKETPROTOCOL, strlen(WEBSOCKETPROTOCOL))};
		ppb_websocket_interface->Connect(newsocket, str, protocols, countof(protocols), ccb);
		ppb_var_interface->Release(str);
		ppb_var_interface->Release(protocols[0]);
		Q_strncpyz(newcon->generic.name, "WebSocket", sizeof(newcon->generic.name));
		newcon->generic.GetPacket = FTENET_NaClWebSocket_GetPacket;
		newcon->generic.SendPacket = FTENET_NaClWebSocket_SendPacket;
		newcon->generic.Close = FTENET_NaClWebSocket_Close;

		newcon->generic.islisten = isserver;
		newcon->generic.addrtype[0] = NA_WEBSOCKET;
		newcon->generic.addrtype[1] = NA_INVALID;

		newcon->generic.thesocket = INVALID_SOCKET;
		newcon->sock = newsocket;

		newcon->remoteadr = adr;

		return &newcon->generic;
	}
	return NULL;
}
#endif

qboolean NET_GetRates(ftenet_connections_t *collection, float *pi, float *po, float *bi, float *bo)
{
	int ctime;
	if (!collection)
		return false;

	ctime = Sys_Milliseconds();
	if ((ctime - collection->timemark) > 1000)
	{
		float secs = (ctime - collection->timemark) / 1000.0f;
		collection->packetsinrate = collection->packetsin * secs;
		collection->packetsoutrate = collection->packetsout * secs;
		collection->bytesinrate = collection->bytesin * secs;
		collection->bytesoutrate = collection->bytesout * secs;
		collection->packetsin = 0;
		collection->packetsout = 0;
		collection->bytesin = 0;
		collection->bytesout = 0;
		collection->timemark = ctime;
	}

	*pi = collection->packetsinrate;
	*po = collection->packetsoutrate;
	*bi = collection->bytesinrate;
	*bo = collection->bytesoutrate;
	return true;
}

#ifdef HAVE_CLIENT
//for demo playback
qboolean NET_UpdateRates(ftenet_connections_t *collection, qboolean inbound, size_t size)
{
	int ctime;
	if (!collection)
		return false;

	if (inbound)
	{
		cls.sockets->bytesin += size;
		cls.sockets->packetsin += 1;
	}
	else
	{
		cls.sockets->bytesout += size;
		cls.sockets->packetsout += 1;
	}

	ctime = Sys_Milliseconds();
	if ((ctime - collection->timemark) > 1000)
	{
		float secs = (ctime - collection->timemark) / 1000.0f;
		collection->packetsinrate = collection->packetsin * secs;
		collection->packetsoutrate = collection->packetsout * secs;
		collection->bytesinrate = collection->bytesin * secs;
		collection->bytesoutrate = collection->bytesout * secs;
		collection->packetsin = 0;
		collection->packetsout = 0;
		collection->bytesin = 0;
		collection->bytesout = 0;
		collection->timemark = ctime;
	}
	return true;
}
#endif

/*firstsock is a cookie*/
int NET_GetPacket (ftenet_connections_t *collection, int firstsock)
{
	struct ftenet_delayed_packet_s *p;
	unsigned int ctime;

	if (!collection)
		return -1;

	while ((p = collection->delayed_packets) && (int)(Sys_Milliseconds()-p->sendtime) > 0)
	{
		collection->delayed_packets = p->next;
#ifdef HAVE_DTLS
		if (p->dest.prot == NP_DTLS)
			FTENET_DTLS_SendPacket(collection, p->cursize, p->data, &p->dest);
		else
#endif
			NET_SendPacketCol (collection, p->cursize, p->data, &p->dest);
		Z_Free(p);
	}

	while (firstsock < MAX_CONNECTIONS)
	{
		if (collection->conn[firstsock])
			if (collection->conn[firstsock]->GetPacket(collection->conn[firstsock]))
			{
				if (net_fakeloss.value)
				{
					if (frandom () < net_fakeloss.value)
						continue;
				}

				collection->bytesin += net_message.cursize;
				collection->packetsin += 1;
				net_from.connum = firstsock+1;
				return firstsock;
			}

		firstsock += 1;
	}

	ctime = Sys_Milliseconds();
	if ((ctime - collection->timemark) > 1000)
	{
		float secs = (ctime - collection->timemark) / 1000.0f;
		collection->packetsinrate = collection->packetsin * secs;
		collection->packetsoutrate = collection->packetsout * secs;
		collection->bytesinrate = collection->bytesin * secs;
		collection->bytesoutrate = collection->bytesout * secs;
		collection->packetsin = 0;
		collection->packetsout = 0;
		collection->bytesin = 0;
		collection->bytesout = 0;
		collection->timemark = ctime;
	}

	return -1;
}

int NET_LocalAddressForRemote(ftenet_connections_t *collection, netadr_t *remote, netadr_t *local, int idx)
{
	int adrflags;
	if (!remote->connum)
		return 0;

	if (!collection->conn[remote->connum-1])
		return 0;

	if (!collection->conn[remote->connum-1]->GetLocalAddresses)
		return 0;

	return collection->conn[remote->connum-1]->GetLocalAddresses(collection->conn[remote->connum-1], &adrflags, local, 1);
}

static neterr_t NET_SendPacketCol (ftenet_connections_t *collection, int length, const void *data, netadr_t *to)
{
	neterr_t err;
	int i;

	if (!collection)
		return NETERR_NOROUTE;

	if (net_fakeloss.value && data)
	{
		if (frandom () < net_fakeloss.value)
		{
			collection->bytesout += length;
			collection->packetsout += 1;
			return NETERR_SENT;
		}
	}

	if (to->connum)
	{
		if (collection->conn[to->connum-1])
		{
			err = collection->conn[to->connum-1]->SendPacket(collection->conn[to->connum-1], length, data, to);
			if (err != NETERR_NOROUTE)
			{
				/*if (err == NETERR_DISCONNECTED)
				{
					collection->conn[i]->Close(collection->conn[i]);
					collection->conn[i] = NULL;
					continue;
				}*/

				collection->bytesout += length;
				collection->packetsout += 1;
				return err;
			}
		}
	}

	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			continue;
		err = collection->conn[i]->SendPacket(collection->conn[i], length, data, to);
		if (err != NETERR_NOROUTE)
		{
			/*if (err == NETERR_DISCONNECTED)
			{
				collection->conn[i]->Close(collection->conn[i]);
				collection->conn[i] = NULL;
				continue;
			}*/

			collection->bytesout += length;
			collection->packetsout += 1;
			return err;
		}
	}

//	Con_Printf("No route to %s - try reconnecting\n", NET_AdrToString(buffer, sizeof(buffer), to));
	return NETERR_NOROUTE;
}

neterr_t NET_SendPacket (ftenet_connections_t *collection, int length, const void *data, netadr_t *to)
{
	if (!collection)
		return NETERR_NOROUTE;

#ifdef HAVE_CLIENT
	if (collection == cls.sockets && cl_delay_packets.ival >= 1 && !(cl.fpd & FPD_NO_FAKE_LAG))
	{
		struct ftenet_delayed_packet_s *p, **l;
		if (!collection)
			return NETERR_NOROUTE;	//erk...
		p = BZ_Malloc(sizeof(*p) - sizeof(p->data) + length);
		p->sendtime = Sys_Milliseconds() + cl_delay_packets.ival;
		p->next = NULL;
		p->cursize = length;
		p->dest = *to;
		memcpy(p->data, data, length);
		for (l = &collection->delayed_packets; *l; l = &((*l)->next))
			;
		*l = p;
		return NETERR_SENT; //fixme: mtu, noroute, etc... panic? only allow if udp dest?
	}
#endif

#ifdef HAVE_DTLS
	if (to->prot == NP_DTLS)
		return FTENET_DTLS_SendPacket(collection, length, data, to);
#endif
	return NET_SendPacketCol (collection, length, data, to);
}

qboolean NET_EnsureRoute(ftenet_connections_t *collection, char *routename, char *host, netadr_t *adr)
{
	switch(adr->prot)
	{
	case NP_DTLS:
		break;
	case NP_DGRAM:
		if (NET_SendPacketCol(collection, 0, NULL, adr) != NETERR_NOROUTE)
			return true;
	case NP_WS:
	case NP_WSS:
	case NP_TLS:
	case NP_STREAM:
		if (!FTENET_AddToCollection(collection, routename, host, adr->type, adr->prot))
			return false;
		Con_Printf("Establishing connection to %s\n", host);
		break;
	default:
		//not recognised, or not needed
		break;
	}
	return true;
}

int NET_EnumerateAddresses(ftenet_connections_t *collection, struct ftenet_generic_connection_s **con, unsigned int *adrflags, netadr_t *addresses, int maxaddresses)
{
	unsigned int found = 0, c, i, j;
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			continue;

		if (collection->conn[i]->GetLocalAddresses)
			c = collection->conn[i]->GetLocalAddresses(collection->conn[i], adrflags, addresses, maxaddresses);
		else
			c = 0;

		if (maxaddresses && !c)
		{
			*adrflags = 0;
			addresses->type = NA_INVALID;
			c = 1;
		}

		//fill in connection info
		for (j = 0; j < c; j++)
		{
			con[j] = collection->conn[i];
			addresses[j].connum = i+1;
		}

		con += c;
		adrflags += c;
		addresses += c;
		maxaddresses -= c;
		found += c;
	}
	return found;
}

enum addressscope_e NET_ClassifyAddress(netadr_t *adr, char **outdesc)
{
	int scope = ASCOPE_NET;
	char *desc = NULL;

	if (adr->type == NA_LOOPBACK)
	{
		//we don't list 127.0.0.1 or ::1, so don't bother with this either. its not interesting.
		scope = ASCOPE_PROCESS, desc = "internal";
	}
	else if (adr->type == NA_IPV6)
	{
		if ((*(int*)adr->address.ip6&BigLong(0xffc00000)) == BigLong(0xfe800000))	//fe80::/10
			scope = ASCOPE_LINK, desc = "link-local";
		else if ((*(int*)adr->address.ip6&BigLong(0xfe000000)) == BigLong(0xfc00000))	//fc::/7
			scope = ASCOPE_LAN, desc = "ULA/private";
		else if (*(int*)adr->address.ip6 == BigLong(0x20010000)) //2001::/32
			scope = ASCOPE_NET, desc = "toredo";
		else if ((*(int*)adr->address.ip6&BigLong(0xffff0000)) == BigLong(0x20020000)) //2002::/16
			scope = ASCOPE_NET, desc = "6to4";
		else if (memcmp(adr->address.ip6, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16) == 0)	//::1
			scope = ASCOPE_HOST, desc = "localhost";
		else if (memcmp(adr->address.ip6, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0)	//::
			scope = ASCOPE_NET, desc = "any";
	}
	else if (adr->type == NA_IP)
	{
		if ((*(int*)adr->address.ip&BigLong(0xffff0000)) == BigLong(0xA9FE0000))	//169.254.x.x/16
			scope = ASCOPE_LINK, desc = "link-local";
		else if ((*(int*)adr->address.ip&BigLong(0xff000000)) == BigLong(0x0a000000))	//10.x.x.x/8
			scope = ASCOPE_LAN, desc = "private";
		else if ((*(int*)adr->address.ip&BigLong(0xff000000)) == BigLong(0x7f000000))	//127.x.x.x/8
			scope = ASCOPE_HOST, desc = "localhost";
		else if ((*(int*)adr->address.ip&BigLong(0xfff00000)) == BigLong(0xac100000))	//172.16.x.x/12
			scope = ASCOPE_LAN, desc = "private";
		else if ((*(int*)adr->address.ip&BigLong(0xffff0000)) == BigLong(0xc0a80000))	//192.168.x.x/16
			scope = ASCOPE_LAN, desc = "private";
		else if ((*(int*)adr->address.ip&BigLong(0xffc00000)) == BigLong(0x64400000))	//100.64.x.x/10
			scope = ASCOPE_LAN, desc = "CGNAT";
		else if (*(int*)adr->address.ip == BigLong(0x00000000))	//0.0.0.0/32
			scope = ASCOPE_LAN, desc = "any";
	}
	if (outdesc)
		*outdesc = desc;
	return scope;
}

#define MAXADDRESSES 64
void NET_PrintAddresses(ftenet_connections_t *collection)
{
	int i;
	char adrbuf[MAX_ADR_SIZE];
	int m;
	netadr_t	addr[64];
	struct ftenet_generic_connection_s			*con[sizeof(addr)/sizeof(addr[0])];
	int			flags[sizeof(addr)/sizeof(addr[0])];
	qboolean	warn = true;
	static const char *scopes[] = {"process", "local", "link", "lan", "net"};
	char *desc;

	if (!collection)
		return;

	m = NET_EnumerateAddresses(collection, con, flags, addr, sizeof(addr)/sizeof(addr[0]));

	for (i = 0; i < m; i++)
	{
		if (addr[i].type != NA_INVALID)
		{
			enum addressscope_e scope = NET_ClassifyAddress(&addr[i], &desc);
			if (developer.ival || scope >= ASCOPE_LAN)
			{
				warn = false;
				if (desc)
					Con_Printf("%s address (%s): %s (%s)\n", scopes[scope], con[i]->name, NET_AdrToString(adrbuf, sizeof(adrbuf), &addr[i]), desc);
				else
					Con_Printf("%s address (%s): %s\n", scopes[scope], con[i]->name, NET_AdrToString(adrbuf, sizeof(adrbuf), &addr[i]));
			}
		}
	}

	if (warn)
		Con_Printf("net address: no public addresses\n");
}

void NET_PrintConnectionsStatus(ftenet_connections_t *collection)
{
	unsigned int i;
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			continue;
		if (collection->conn[i]->PrintStatus)
			collection->conn[i]->PrintStatus(collection->conn[i]);
	}

#ifdef HAVE_DTLS
	if (developer.ival)
	{
		struct dtlspeer_s *dtls;
		char adr[64];
		for (dtls = collection->dtls; dtls; dtls = dtls->next)
			Con_Printf("dtls: %s\n", NET_AdrToString(adr, sizeof(adr), &dtls->addr));
	}
	else
	{
		struct dtlspeer_s *dtls;
		int c = 0;
		for (dtls = collection->dtls; dtls; dtls = dtls->next)
			c++;
		if (c)
			Con_Printf("dtls connections : %i\n", c);
	}
#endif
}

//=============================================================================

int TCP_OpenStream (netadr_t *remoteaddr)
{
#ifndef HAVE_TCP
	return (int)INVALID_SOCKET;
#else
	unsigned long _true = true;
	int newsocket;
	int temp;
	struct sockaddr_qstorage qs;
//	struct sockaddr_qstorage loc;
	int recvbufsize = (1<<19);//512kb
	int sysprot;

	switch(remoteaddr->type)
	{
#if defined(HAVE_IPV4) || defined(HAVE_IPV6)
	case NA_IP:
	case NA_IPV6:
		sysprot = IPPROTO_TCP;
		break;
#endif
#ifdef HAVE_IPX
	case NA_IPX:
		sysprot = NSPROTO_IPX;
		break;
#endif
	default:
		sysprot = 0;
		break;
	}
	temp = NetadrToSockadr(remoteaddr, &qs);

	if ((newsocket = socket (((struct sockaddr_in*)&qs)->sin_family, SOCK_CLOEXEC|SOCK_STREAM, sysprot)) == INVALID_SOCKET)
		return (int)INVALID_SOCKET;

	setsockopt(newsocket, SOL_SOCKET, SO_RCVBUF, (void*)&recvbufsize, sizeof(recvbufsize));

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(neterrno()));

#ifdef UNIXSOCKETS
	if (remoteaddr->type == AF_UNIX)
	{	//if its a unix socket, attempt to bind it to an unnamed address. linux should generate an ephemerial abstract address (otherwise the server will see an empty address).
		struct sockaddr_un un;
		memset(&un, 0, offsetof(struct sockaddr_un, sun_path));
		bind(newsocket, (struct sockaddr*)&un, offsetof(struct sockaddr_un, sun_path));
	}
	else
#endif
	{
//		memset(&loc, 0, sizeof(loc));
//		((struct sockaddr*)&loc)->sa_family = ((struct sockaddr*)&loc)->sa_family;
//		bind(newsocket, (struct sockaddr *)&loc, ((struct sockaddr_in*)&qs)->sin_family == AF_INET?sizeof(struct sockaddr_in):sizeof(struct sockaddr_in6));
	}

	if (connect(newsocket, (struct sockaddr *)&qs, temp) == INVALID_SOCKET)
	{
		int err = neterrno();
		if (err != NET_EWOULDBLOCK && err != NET_EINPROGRESS)
		{
			char buf[256];
			NET_AdrToString(buf, sizeof(buf), remoteaddr);
			if (err == NET_EADDRNOTAVAIL)
			{
				if (remoteaddr->port == 0 && (remoteaddr->type == NA_IP || remoteaddr->type == NA_IPV6))
					Con_Printf ("TCP_OpenStream: no port specified (%s)\n", buf);
				else
					Con_Printf ("TCP_OpenStream: invalid address trying to connect to %s\n", buf);
			}
			else if (err == NET_ECONNREFUSED)
				Con_Printf ("TCP_OpenStream: connection refused (%s)\n", buf);
			else if (err == NET_EACCES)
				Con_Printf ("TCP_OpenStream: access denied: check firewall (%s)\n", buf);
			else
				Con_Printf ("TCP_OpenStream: connect: error %i (%s)\n", err, buf);
			closesocket(newsocket);
			return (int)INVALID_SOCKET;
		}
	}

	return newsocket;
#endif
}

/*int TCP_OpenListenSocket (const char *localip, int port)
{
#ifndef HAVE_TCP
	return INVALID_SOCKET;
#else
	int newsocket;
	struct sockaddr_qstorage address;
	int pf;
	unsigned long _true = true;
	int i;
int maxport = port + 100;

	if (localip && *localip)
	{
		if (!NET_StringToSockaddr(localip, port, &address, &pf, &adrsize))
			return INVALID_SOCKET;
	}
	else
	{
		adrsize = sizeof(struct sockaddr_in);
		pf = ((struct sockaddr_in*)&address)->sin_family = AF_INET;
		((struct sockaddr_in*)&address)->sin_port = htons(port);

		//ZOID -- check for interface binding option
		if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc)
		{
			((struct sockaddr_in*)&address)->sin_addr.s_addr = inet_addr(com_argv[i+1]);
			Con_TPrintf("Binding to IP Interface Address of %s\n",
					inet_ntoa(address.sin_addr));
		}
		else
			((struct sockaddr_in*)&address)->sin_addr.s_addr = INADDR_ANY;
	}

	if ((newsocket = socket (pf, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("TCP_OpenListenSocket: ioctl FIONBIO: %s", strerror(qerrno));

	for(;;)
	{
		if (port == PORT_ANY)
			address.sin_port = 0;
		else
			address.sin_port = htons((short)port);

		if( bind (newsocket, (void *)&address, sizeof(address)) == -1)
		{
			if (!port)
			{
				Con_Printf("Cannot bind tcp socket\n");
				closesocket(newsocket);
				return INVALID_SOCKET;
			}
			port++;
			if (port > maxport)
			{
				Con_Printf("Cannot bind tcp socket\n");
				closesocket(newsocket);
				return INVALID_SOCKET;
			}
		}
		else
			break;
	}

	if (listen(newsocket, 1) == INVALID_SOCKET)
	{
		Con_Printf("Cannot listen on tcp socket\n");
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	return newsocket;
#endif
}
*/

#if defined(SV_MASTER) || defined(CL_MASTER)
int UDP_OpenSocket (int port)
{
	SOCKET newsocket;
	struct sockaddr_in address;
	unsigned long _true = true;
	int i;
int maxport = port + 100;

	if ((newsocket = socket (PF_INET, SOCK_CLOEXEC|SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		return (int)INVALID_SOCKET;

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(neterrno()));

	address.sin_family = AF_INET;
//ZOID -- check for interface binding option
	if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc) {
		address.sin_addr.s_addr = inet_addr(com_argv[i+1]);
		Con_TPrintf("Binding to IP Interface Address of %s\n",
				inet_ntoa(address.sin_addr));
	} else
		address.sin_addr.s_addr = INADDR_ANY;

	for(;;)
	{
		if (port == PORT_ANY)
			address.sin_port = 0;
		else
			address.sin_port = htons((short)port);

		if( bind (newsocket, (void *)&address, sizeof(address)) == -1)
		{
			if (!port)
				Sys_Error ("UDP_OpenSocket: bind: %s", strerror(neterrno()));
			port++;
			if (port > maxport)
				Sys_Error ("UDP_OpenSocket: bind: %s", strerror(neterrno()));
		}
		else
			break;
	}

	return newsocket;
}

#ifdef HAVE_IPV6
int UDP6_OpenSocket (int port)
{
	int err;
	SOCKET newsocket;
	struct sockaddr_in6 address;
	unsigned long _true = true;
//	int i;
int maxport = port + 100;

	memset(&address, 0, sizeof(address));

	if ((newsocket = socket (PF_INET6, SOCK_CLOEXEC|SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		Con_Printf("IPV6 is not supported: %s\n", strerror(neterrno()));
		return (int)INVALID_SOCKET;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(neterrno()));

#ifdef IPV6_V6ONLY
	setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_true, sizeof(_true));
#endif

	address.sin6_family = AF_INET6;
//ZOID -- check for interface binding option
//	if ((i = COM_CheckParm("-ip6")) != 0 && i < com_argc) {
//		address.sin6_addr = inet_addr(com_argv[i+1]);
///		Con_TPrintf("Binding to IP Interface Address of %s\n",
//				inet_ntoa(address.sin6_addr));
//	} else
		memset(&address.sin6_addr, 0, sizeof(struct in6_addr));

	for(;;)
	{
		if (port == PORT_ANY)
			address.sin6_port = 0;
		else
			address.sin6_port = htons((short)port);

		if( bind (newsocket, (void *)&address, sizeof(address)) == -1)
		{
			if (!port)
			{
				err = neterrno();
				Con_Printf ("UDP6_OpenSocket: bind: (%i) %s", err, strerror(err));
				closesocket(newsocket);
				return (int)INVALID_SOCKET;
			}
			port++;
			if (port > maxport)
			{
				err = neterrno();
				Con_Printf ("UDP6_OpenSocket: bind: (%i) %s", err, strerror(err));
				closesocket(newsocket);
				return (int)INVALID_SOCKET;
			}
		}
		else
			break;
	}

	return newsocket;
}
#endif

void UDP_CloseSocket (int socket)
{
	closesocket(socket);
}

int IPX_OpenSocket (int port)
{
#ifndef HAVE_IPX
	return 0;
#else
	SOCKET					newsocket;
	struct sockaddr_ipx	address;
	u_long					_true = 1;

	if ((newsocket = socket (PF_IPX, SOCK_CLOEXEC|SOCK_DGRAM, NSPROTO_IPX)) == INVALID_SOCKET)
	{
		int e = neterrno();
		if (e != NET_EAFNOSUPPORT)
			Con_Printf ("WARNING: IPX_Socket: socket: %i\n", e);
		return INVALID_SOCKET;
	}

	// make it non-blocking
	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
	{
		Con_Printf ("WARNING: IPX_Socket: ioctl FIONBIO: %i\n", neterrno());
		return INVALID_SOCKET;
	}

	address.sa_family = AF_IPX;
	memset (address.sa_netnum, 0, 4);
	memset (address.sa_nodenum, 0, 6);
	if (port == PORT_ANY)
		address.sa_socket = 0;
	else
		address.sa_socket = htons((short)port);

	if( bind (newsocket, (void *)&address, sizeof(address)) == -1)
	{
		Con_Printf ("WARNING: IPX_Socket: bind: %i\n", neterrno());
		closesocket (newsocket);
		return INVALID_SOCKET;
	}

	return newsocket;
#endif
}

void IPX_CloseSocket (int socket)
{
#ifdef HAVE_IPX
	closesocket(socket);
#endif
}
#endif

// sleeps msec or until net socket is ready
//stdin can sometimes be a socket. As a result,
//we give the option to select it for nice console imput with timeouts.
qboolean NET_Sleep(float seconds, qboolean stdinissocket)
{
#ifdef HAVE_PACKET
	struct timeval timeout;
	fd_set	readfdset;
	fd_set	writefdset;
	qintptr_t maxfd = -1;
	int con, sock;
	unsigned int usec;

	FD_ZERO(&readfdset);
	FD_ZERO(&writefdset);

	if (stdinissocket)
	{
		sock = 0;	//stdin tends to be socket/filehandle 0 in unix
		FD_SET(sock, &readfdset);
		maxfd = sock;
	}

#ifdef SV_MASTER
	{
		extern ftenet_connections_t *svm_sockets;
		if (svm_sockets)
		for (con = 0; con < MAX_CONNECTIONS; con++)
		{
			if (!svm_sockets->conn[con])
				continue;
			if (svm_sockets->conn[con]->SetFDSets)
			{
				sock = svm_sockets->conn[con]->SetFDSets(svm_sockets->conn[con], &readfdset, &writefdset);
				if (sock > maxfd)
					maxfd = sock;
			}
			else
			{
				sock = svm_sockets->conn[con]->thesocket;
				if (sock != INVALID_SOCKET)
				{
					FD_SET(sock, &readfdset); // network socket
					if (sock > maxfd)
						maxfd = sock;
				}
			}
		}
	}
#endif
#ifdef HAVE_SERVER
	if (svs.sockets)
	for (con = 0; con < MAX_CONNECTIONS; con++)
	{
		if (!svs.sockets->conn[con])
			continue;
		if (svs.sockets->conn[con]->SetFDSets)
		{
			sock = svs.sockets->conn[con]->SetFDSets(svs.sockets->conn[con], &readfdset, &writefdset);
			if (sock > maxfd)
				maxfd = sock;
		}
		else
		{
			sock = svs.sockets->conn[con]->thesocket;
			if (sock != INVALID_SOCKET)
			{
				FD_SET(sock, &readfdset); // network socket
				if (sock > maxfd)
					maxfd = sock;
			}
		}
	}
#endif

	if (seconds > 4.0)	//realy? oh well.
		seconds = 4.0;
	if (maxfd == -1)
		Sys_Sleep(seconds);
	else
	{
		usec = seconds*1000*1000;
		usec += 1000;	//slight extra delay, to ensure we don't wake up with nothing to do.
		timeout.tv_sec = usec/(1000*1000);
		timeout.tv_usec = usec;
		select(maxfd+1, &readfdset, &writefdset, NULL, &timeout);
	}

	if (stdinissocket)
		return FD_ISSET(0, &readfdset);
#endif
	return true;
}

//this function is used to determine the 'default' local address.
//this is used for compat with gamespy which insists on sending us a packet via that interface and not something more sensible like 127.0.0.1
//thus its only needed on windows and with ipv4.
void NET_GetLocalAddress (int socket, netadr_t *out)
{
#if defined(_WIN32) && defined(HAVE_PACKET)
	char	buff[512];
	struct sockaddr_qstorage	address;
	int		namelen;
	netadr_t adr = {0};
	qboolean notvalid = false;

	strcpy(buff, "localhost");
	gethostname(buff, 512);
	buff[512-1] = 0;

	if (!NET_StringToAdr (buff, 0, &adr))	//urm
		if (!NET_StringToAdr ("127.0.0.1", 0, &adr))
			return;


	namelen = sizeof(address);
	if (getsockname (socket, (struct sockaddr *)&address, &namelen) == -1)
	{
		notvalid = true;
		NET_StringToSockaddr2("0.0.0.0", 0, NA_INVALID, (struct sockaddr_qstorage *)&address, NULL, NULL, 1);
//		Sys_Error ("NET_Init: getsockname:", strerror(qerrno));
	}

	SockadrToNetadr(&address, namelen, out);
	if (out->type == NA_IP)
	{
		if (!*(int*)out->address.ip)	//socket was set to auto
		{
			if (adr.type == NA_IP)
				*(int *)out->address.ip = *(int *)adr.address.ip;	//change it to what the machine says it is, rather than the socket.
		}
	}
	if (out->type == NA_IPV6)
	{
		if (!((int*)out->address.ip6)[0] &&
			!((int*)out->address.ip6)[1] &&
			!((short*)out->address.ip6)[4] &&
			(!((short*)out->address.ip6)[5] || ((unsigned short*)out->address.ip6)[5]==0xffffu)
			&& !((int*)out->address.ip6)[3])	//ipv6 any or ipv4-mapped any.
		{
			if (adr.type == NA_IP)
			{
				memset(out->address.ip6, 0, sizeof(out->address.ip6));
				((short *)out->address.ip6)[5] = 0xffff;
				((int *)out->address.ip6)[3] = *(int *)adr.address.ip;
			}
			else if (adr.type == NA_IPV6)
				memcpy(out->address.ip6, adr.address.ip6, sizeof(out->address.ip6));
		}
	}

	if (!notvalid)
	{
//		char	adrbuf[MAX_ADR_SIZE];
//		Con_TPrintf("Client IP address %s\n", NET_AdrToString (adrbuf, sizeof(adrbuf), out) );
		return;
	}
//	Con_Printf("Couldn't detect local ip\n");
#endif

	out->type = NA_INVALID;
}

#ifdef HAVE_SERVER
void SVNET_AddPort_f(void)
{
	char *s = Cmd_Argv(1);
	char *conname = Cmd_Argv(2);

	if (!*s && !*conname)
	{
		Con_Printf("Active Server ports:\n");
		NET_PrintAddresses(svs.sockets);
		Con_Printf("end of list\n");
		return;
	}
	if (!*conname)
		conname = NULL;

	//just in case
	if (!svs.sockets)
	{
		svs.sockets = FTENET_CreateCollection(true);
#ifdef HAVE_CLIENT
		FTENET_AddToCollection(svs.sockets, "SVLoopback", STRINGIFY(PORT_DEFAULTSERVER), NA_LOOPBACK, NP_DGRAM);
#endif
	}

	FTENET_AddToCollection(svs.sockets, conname, *s?s:NULL, *s?NA_IP:NA_INVALID, NP_DGRAM);
}
#endif

#ifdef HAVE_CLIENT
void NET_ClientPort_f(void)
{
	Con_Printf("Active Client ports:\n");
	NET_PrintAddresses(cls.sockets);
	Con_Printf("end of list\n");
}
#endif

qboolean NET_WasSpecialPacket(ftenet_connections_t *collection)
{
#ifdef HAVE_NATPMP
	if (NET_Was_NATPMP(collection))
		return true;
#endif

#ifdef SUPPORT_ICE
	if (ICE_WasStun(collection))
		return true;
#endif

	return false;
}

//static void QDECL NET_UPNPIGP_Callback(cvar_t *var, char *oldval)
//{
//}
//cvar_t net_upnpigp = CVARCD("net_upnpigp", "0", NET_UPNPIGP_Callback, "If set, enables the use of the upnp-igd protocol to punch holes in your local NAT box.");

void SSL_Init(void);
/*
====================
NET_Init
====================
*/
void NET_Init (void)
{
	Cvar_Register(&net_enabled, "networking");
	Cvar_Register(&net_dns_ipv4, "networking");
	Cvar_Register(&net_dns_ipv6, "networking");
	if (net_enabled.ival)
	{
#if defined(_WIN32) && defined(HAVE_PACKET)
		int		r;
#ifdef HAVE_IPV6
		dllfunction_t fncs[] =
		{
			{(void**)&pgetaddrinfo, "getaddrinfo"},
			{(void**)&pfreeaddrinfo, "freeaddrinfo"},
			{NULL, NULL}
		};
		Sys_LoadLibrary("ws2_32.dll", fncs);
#endif

		r = WSAStartup (MAKEWORD(2, 2), &winsockdata);

		if (r)
			Sys_Error ("Winsock initialization failed.");
#endif
	}

	Cvar_Register(&timeout, "networking");
	Cvar_Register(&net_hybriddualstack, "networking");
	Cvar_Register(&net_fakeloss, "networking");

#if defined(TCPCONNECT) && (defined(HAVE_SERVER) || defined(HAVE_HTTPSV))
#ifdef HAVE_SERVER
	Cvar_Register(&net_enable_qizmo, "networking");
	Cvar_Register(&net_enable_qtv, "networking");
#endif
#if defined(HAVE_SSL)
	Cvar_Register(&net_enable_tls, "networking");
	Cvar_Register(&tls_ignorecertificateerrors, "networking");
#endif
#ifdef HAVE_HTTPSV
	Cvar_Register(&net_enable_http, "networking");
	Cvar_Register(&net_enable_websockets, "networking");
	Cvar_Register(&net_enable_webrtcbroker, "networking");
#endif
#endif



#ifdef HAVE_SERVER
	Cmd_AddCommand("sv_addport", SVNET_AddPort_f);
#endif
#ifdef HAVE_CLIENT
	Cvar_Register(&cl_delay_packets, "networking");
	Cmd_AddCommand("cl_addport", NET_ClientPort_f);
#endif

//	Cvar_Register (&net_upnpigp, "networking");
//	net_upnpigp.restriction = RESTRICT_MAX;

	//
	// init the message buffer
	//
	net_message.maxsize = sizeof(net_message_buffer);
	net_message.data = net_message_buffer;

#if defined(HAVE_WINSSPI)
	SSL_Init();
#endif

#if defined(HAVE_CLIENT)||defined(HAVE_SERVER)
	Net_Master_Init();
#endif
}
#ifdef HAVE_CLIENT
void NET_CloseClient(void)
{	//called by disconnect console command
	FTENET_CloseCollection(cls.sockets);
	cls.sockets = NULL;
}
void NET_InitClient(qboolean loopbackonly)
{
	const char *port;
	int p;

#ifdef QUAKESPYAPI
	port = STRINGIFY(PORT_QWCLIENT);
#else
	port = "0";
#endif

	p = COM_CheckParm ("-clport");
	if (p && p < com_argc)
	{
		port = com_argv[p+1];
	}

	if (!cls.sockets)
		cls.sockets = FTENET_CreateCollection(false);
#ifdef HAVE_SERVER
	FTENET_AddToCollection(cls.sockets, "CLLoopback", "1", NA_LOOPBACK, NP_DGRAM);
#endif
	if (loopbackonly)
		port = "";
#if defined(HAVE_IPV4) && defined(HAVE_IPV6)
	if (net_hybriddualstack.ival)
	{
		FTENET_AddToCollection(cls.sockets, "CLUDP", port, NA_IP, NP_DGRAM);
	}
	else
#endif
	{
		#ifdef HAVE_IPV4
			FTENET_AddToCollection(cls.sockets, "CLUDP4", port, NA_IP, NP_DGRAM);
		#endif
		#ifdef HAVE_IPV6
			FTENET_AddToCollection(cls.sockets, "CLUDP6", port, NA_IPV6, NP_DGRAM);
		#endif
	}
#ifdef HAVE_IPX
	FTENET_AddToCollection(cls.sockets, "CLIPX", port, NA_IPX, NP_DGRAM);
#endif

	//	Con_TPrintf("Client port Initialized\n");
}
#endif

#ifdef HAVE_SERVER
#ifdef HAVE_IPV4
static void QDECL SV_Tcpport_Callback(struct cvar_s *var, char *oldvalue)
{
	if (!strcmp(var->string, "0"))	//qtv_streamport had an old default value of 0. make sure we don't end up listening on random ports.
		FTENET_AddToCollection(svs.sockets, var->name, "", NA_IP, NP_STREAM);
	else
		FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_IP, NP_STREAM);
}
cvar_t	sv_port_tcp = CVARFC("sv_port_tcp", "", CVAR_SERVERINFO, SV_Tcpport_Callback);
#ifdef HAVE_LEGACY
cvar_t	qtv_streamport	= CVARAFCD(	"qtv_streamport", "",
									"mvd_streamport", 0, SV_Tcpport_Callback, "Legacy cvar. Use sv_port_tcp instead.");
#endif
#endif
#ifdef HAVE_IPV6
static void QDECL SV_Tcpport6_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_IPV6, NP_STREAM);
}
cvar_t	sv_port_tcp6 = CVARC("sv_port_tcp6", "", SV_Tcpport6_Callback);
#endif
#ifdef HAVE_IPV4
static void QDECL SV_Port_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_IP, NP_DGRAM);
}
cvar_t  sv_port_ipv4 = CVARC("sv_port", STRINGIFY(PORT_DEFAULTSERVER), SV_Port_Callback);
#endif
#ifdef HAVE_IPV6
static void QDECL SV_PortIPv6_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_IPV6, NP_DGRAM);
}
cvar_t  sv_port_ipv6 = CVARCD("sv_port_ipv6", "", SV_PortIPv6_Callback, "Port to use for incoming ipv6 udp connections. Due to hybrid sockets this might not be needed. You can specify an ipv4 address:port for a second ipv4 port if you want.");
#endif
#ifdef HAVE_IPX
void QDECL SV_PortIPX_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_IPX, NP_DGRAM);
}
cvar_t  sv_port_ipx = CVARC("sv_port_ipx", "", SV_PortIPX_Callback);
#endif
#ifdef UNIXSOCKETS
void QDECL SV_PortUNIX_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_UNIX, NP_DGRAM);
}
#ifdef __linux	//linux adds abstract sockets, which require no filesystem cleanup.
cvar_t  sv_port_unix = CVARC("sv_port_unix", "@qsock.fte", SV_PortUNIX_Callback);
#else
cvar_t  sv_port_unix = CVARC("sv_port_unix", "/tmp/qsock.fte", SV_PortUNIX_Callback);
#endif
#endif
#ifdef HAVE_NATPMP
static void QDECL SV_Port_NatPMP_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, va("natpmp://%s", var->string), NA_IP, NP_NATPMP);
}
#if 1//def SERVERONLY
#define NATPMP_DEFAULT_PORT ""		//don't fuck with dedicated servers
#else
#define NATPMP_DEFAULT_PORT "5351"	//home users, yay, lucky people.
#endif
cvar_t sv_port_natpmp = CVARCD("sv_port_natpmp", NATPMP_DEFAULT_PORT, SV_Port_NatPMP_Callback, "If set (typically to 5351), automatically configures your router's port forwarding. You can instead specify the full ip address of your router (192.168.1.1:5351 for example). Your router must have NAT-PMP supported and enabled.");
#endif

#ifdef FTE_TARGET_WEB
void QDECL SV_PortRTC_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_WEBSOCKET, NP_DTLS);
}
cvar_t  sv_port_rtc = CVARCD("sv_port_rtc", "/", SV_PortRTC_Callback, "This specifies the broker url to use to obtain clients from. If the hostname is ommitted, it'll come from the manifest. If omitted, the broker service will randomize the resource part, so be sure to tell your friends the path reported by eg status rather than just this cvar value. Or just set it to 'rtc:///example' and tell clients to connect to the same sservevalue.");
#endif

void SVNET_RegisterCvars(void)
{
#ifdef FTE_TARGET_WEB
	Cvar_Register (&sv_port_rtc,	"networking");
//	sv_port_rtc.restriction = RESTRICT_MAX;
#endif
#if defined(TCPCONNECT) && defined(HAVE_IPV4)
	Cvar_Register (&sv_port_tcp,	"networking");
	sv_port_tcp.restriction = RESTRICT_MAX;
#ifdef HAVE_LEGACY
	Cvar_Register (&qtv_streamport,	"networking");
	qtv_streamport.restriction = RESTRICT_MAX;
#endif
#endif
#if defined(TCPCONNECT) && defined(HAVE_IPV6)
	Cvar_Register (&sv_port_tcp6,	"networking");
	sv_port_tcp6.restriction = RESTRICT_MAX;
#endif
#ifdef HAVE_IPV6
	Cvar_Register (&sv_port_ipv6,	"networking");
	sv_port_ipv6.restriction = RESTRICT_MAX;
#endif
#ifdef HAVE_IPX
	Cvar_Register (&sv_port_ipx,	"networking");
	sv_port_ipx.restriction = RESTRICT_MAX;
#endif
#ifdef HAVE_IPV4
	Cvar_Register (&sv_port_ipv4,	"networking");
	sv_port_ipv4.restriction = RESTRICT_MAX;
#endif
#ifdef HAVE_NATPMP
	Cvar_Register (&sv_port_natpmp,	"networking");
	sv_port_natpmp.restriction = RESTRICT_MAX;
#endif
#ifdef UNIXSOCKETS
//	Cvar_Register (&sv_port_unix,	"networking");
#endif

#if defined(HAVE_DTLS) && defined(HAVE_SERVER)
	Cvar_Register (&net_enable_dtls,			"networking");
#endif
}

void NET_CloseServer(void)
{
	FTENET_CloseCollection(svs.sockets);
	svs.sockets = NULL;
}

void NET_InitServer(void)
{
	qboolean singleplayer = (sv.allocated_client_slots == 1) && !isDedicated;
	if ((sv_listen_nq.value || sv_listen_dp.value || sv_listen_qw.value
#ifdef QWOVERQ3
		|| sv_listen_q3.ival
#endif
		) && !singleplayer)
	{
		if (!svs.sockets)
		{
			svs.sockets = FTENET_CreateCollection(true);
#ifdef HAVE_CLIENT
			FTENET_AddToCollection(svs.sockets, "SVLoopback", STRINGIFY(PORT_DEFAULTSERVER), NA_LOOPBACK, NP_DGRAM);
#endif
		}

#ifdef FTE_TARGET_WEB
		Cvar_ForceCallback(&sv_port_rtc);
#endif
#ifdef HAVE_IPV4
		Cvar_ForceCallback(&sv_port_ipv4);
#endif
#ifdef HAVE_IPV6
		Cvar_ForceCallback(&sv_port_ipv6);
#endif
#ifdef HAVE_IPX
		Cvar_ForceCallback(&sv_port_ipx);
#endif
#if defined(TCPCONNECT) && defined(HAVE_TCP)
		Cvar_ForceCallback(&sv_port_tcp);
#ifdef HAVE_LEGACY
		Cvar_ForceCallback(&qtv_streamport);
#endif
#ifdef HAVE_IPV6
		Cvar_ForceCallback(&sv_port_tcp6);
#endif
#endif
#ifdef HAVE_NATPMP
		Cvar_ForceCallback(&sv_port_natpmp);
#endif
#ifdef UNIXSOCKETS
//		Cvar_ForceCallback(&sv_port_unix);
#endif
#ifdef HAVE_DTLS
		Cvar_ForceCallback(&net_enable_dtls);
#endif
	}
	else
	{
		NET_CloseServer();

#ifdef HAVE_CLIENT
		svs.sockets = FTENET_CreateCollection(true);
		FTENET_AddToCollection(svs.sockets, "SVLoopback", STRINGIFY(PORT_DEFAULTSERVER), NA_LOOPBACK, NP_DGRAM);
#endif
	}
}
#endif

void NET_Tick(void)
{
#ifdef SUPPORT_ICE
	ICE_Tick();
#endif
}
/*
====================
NET_Shutdown
====================
*/
void	NET_Shutdown (void)
{
#ifdef HAVE_SERVER
	NET_CloseServer();
#endif
#ifdef HAVE_CLIENT
	FTENET_CloseCollection(cls.sockets);
	cls.sockets = NULL;
#endif


#if defined(_WIN32) && defined(HAVE_PACKET)
#ifdef SERVERTONLY
	if (!serverthreadID)	//running as subsystem of client. Don't close all of it's sockets too.
#endif
		WSACleanup ();
#endif
}






#ifdef HAVE_TCP
typedef struct {
	vfsfile_t funcs;

	SOCKET sock;
	qboolean conpending;

	char readbuffer[65536];
	int readbuffered;
	char peer[1];
} tcpfile_t;
static void VFSTCP_Error(tcpfile_t *f)
{
	if (f->sock != INVALID_SOCKET)
	{
		closesocket(f->sock);
		f->sock = INVALID_SOCKET;
	}
}
int QDECL VFSTCP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	tcpfile_t *tf = (tcpfile_t*)file;
	int len;
	int trying;

	if (tf->conpending)
	{
		fd_set wr;
		fd_set ex;
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		FD_ZERO(&wr);
		FD_SET(tf->sock, &wr);
		FD_ZERO(&ex);
		FD_SET(tf->sock, &ex);
		if (!select((int)tf->sock+1, NULL, &wr, &ex, &timeout))
			return 0;
		tf->conpending = false;
	}

	if (tf->sock != INVALID_SOCKET)
	{
		trying = sizeof(tf->readbuffer) - tf->readbuffered;
		if (bytestoread > 1500)
		{
			if (trying > bytestoread)
				trying = bytestoread;
		}
		else
		{
			if (trying > 1500)
				trying = 1500;
		}
		len = recv(tf->sock, tf->readbuffer + tf->readbuffered, trying, 0);
		if (len == -1)
		{
			int e = neterrno();
			if (e != NET_EWOULDBLOCK)
			{
				switch(e)
				{
				case NET_ENOTCONN:
					Con_Printf("connection to \"%s\" failed\n", tf->peer);
					break;
				case NET_ECONNABORTED:
					Con_DPrintf("connection to \"%s\" aborted\n", tf->peer);
					break;
				case NET_ETIMEDOUT:
					Con_Printf("connection to \"%s\" timed out\n", tf->peer);
					break;
				case NET_ECONNREFUSED:
					Con_DPrintf("connection to \"%s\" refused\n", tf->peer);
					break;
				case NET_ECONNRESET:
					Con_DPrintf("connection to \"%s\" reset\n", tf->peer);
					break;
				default:
					Con_Printf("tcp socket error %i (%s)\n", e, tf->peer);
				}
				VFSTCP_Error(tf);
			}
			//fixme: figure out wouldblock or error
		}
		else if (len == 0 && trying != 0)
		{
			//peer disconnected
			VFSTCP_Error(tf);
		}
		else
		{
			tf->readbuffered += len;
		}
	}

	//return a partially filled buffer.
	if (bytestoread > tf->readbuffered)
		bytestoread = tf->readbuffered;
	if (bytestoread < 0)
		VFSTCP_Error(tf);

	if (bytestoread > 0)
	{
		memcpy(buffer, tf->readbuffer, bytestoread);
		tf->readbuffered -= bytestoread;
		memmove(tf->readbuffer, tf->readbuffer+bytestoread, tf->readbuffered);
		return bytestoread;
	}
	else
	{
		if (tf->sock == INVALID_SOCKET)
		{
			return -1;	//signal an error
		}
		return 0;	//signal nothing available
	}
}
int QDECL VFSTCP_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
{
	tcpfile_t *tf = (tcpfile_t*)file;
	int len;

	if (tf->sock == INVALID_SOCKET)
		return -1;

	if (tf->conpending)
	{
		fd_set fdw, fdx;
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		FD_ZERO(&fdw);
		FD_SET(tf->sock, &fdw);
		FD_ZERO(&fdx);
		FD_SET(tf->sock, &fdx);
		if (!select((int)tf->sock+1, NULL, &fdw, &fdx, &timeout))
			return 0;
		tf->conpending = false;
	}

	len = send(tf->sock, buffer, bytestoread, 0);
	if (len == -1 || len == 0)
	{
		int e = neterrno();
		switch(e)
		{
		case NET_EWOULDBLOCK:
			return 0;	//nothing available yet.
		case NET_ETIMEDOUT:
			Con_Printf("connection to \"%s\" timed out\n", tf->peer);
			return -1;	//don't bother trying to read if we never connected.
		case NET_ENOTCONN:
#ifdef __unix__
		case EPIPE:
#endif
			Con_Printf("connection to \"%s\" failed\n", tf->peer);
			return -1;	//don't bother trying to read if we never connected.
		default:
			Sys_Printf("tcp socket error %i (%s)\n", e, tf->peer);
			break;
		}
//		don't destroy it on write errors, because that prevents us from reading anything that was sent to us afterwards.
//		instead let the read handling kill it if there's nothing new to be read
		VFSTCP_ReadBytes(file, NULL, 0);
		return -1;
	}
	return len;
}
qboolean QDECL VFSTCP_Seek (struct vfsfile_s *file, qofs_t pos)
{
	VFSTCP_Error((tcpfile_t*)file);
	return false;
}
static qofs_t QDECL VFSTCP_Tell (struct vfsfile_s *file)
{
	VFSTCP_Error((tcpfile_t*)file);
	return 0;
}
static qofs_t QDECL VFSTCP_GetLen (struct vfsfile_s *file)
{
	return 0;
}
static qboolean QDECL VFSTCP_Close (struct vfsfile_s *file)
{
	tcpfile_t *f = (tcpfile_t *)file;
	qboolean success = f->sock != INVALID_SOCKET;
	VFSTCP_Error(f);
	Z_Free(f);
	return success;
}

vfsfile_t *FS_OpenTCPSocket(SOCKET sock, qboolean conpending, const char *peername)
{
	tcpfile_t *newf;
	if (sock == INVALID_SOCKET)
		return NULL;

	newf = Z_Malloc(sizeof(*newf) + strlen(peername));
	strcpy(newf->peer, peername);
	newf->conpending = conpending;
	newf->sock = sock;
	newf->funcs.Close = VFSTCP_Close;
	newf->funcs.Flush = NULL;
	newf->funcs.GetLen = VFSTCP_GetLen;
	newf->funcs.ReadBytes = VFSTCP_ReadBytes;
	newf->funcs.Seek = VFSTCP_Seek;
	newf->funcs.Tell = VFSTCP_Tell;
	newf->funcs.WriteBytes = VFSTCP_WriteBytes;
	newf->funcs.seekstyle = SS_UNSEEKABLE;

	return &newf->funcs;
}
vfsfile_t *FS_OpenTCP(const char *name, int defaultport)
{
	netadr_t adr = {0};
	if (NET_StringToAdr(name, defaultport, &adr))
	{
		return FS_OpenTCPSocket(TCP_OpenStream(&adr), true, name);
	}
	else
		return NULL;
}
#elif 0 //defined(HAVE_WEBSOCKCL)
This code is disabled.
I cannot provide a reliable mechanism over chrome/nacls websockets at this time.
Some module within the ppapi/nacl/chrome stack refuses to forward the data when stressed.
All I can determine is that the connection has a gap.
Hopefully this should be fixed by pepper_19.

As far as Im aware, this and the relevent code in QTV should be functionally complete.

typedef struct
{
	vfsfile_t funcs;

	PP_Resource sock;

	unsigned char readbuffer[65536];
	int readbuffered;
	qboolean havepacket;
	struct PP_Var incomingpacket;
	qboolean failed;
} tcpfile_t;

static void tcp_websocketgot(void *user_data, int32_t result)
{
	tcpfile_t *wsc = user_data;
	if (result == PP_OK)
	{
		if (wsc->incomingpacket.type == PP_VARTYPE_UNDEFINED)
		{
			Con_Printf("ERROR: %s: var was not set by PPAPI. Data has been lost.\n", __func__);
			wsc->failed = true;
		}
		wsc->havepacket = true;
	}
	else
	{
		Sys_Printf("%s: %i\n", __func__, result);
		wsc->failed = true;
	}
}
static void tcp_websocketconnected(void *user_data, int32_t result)
{
	tcpfile_t *wsc = user_data;
	if (result == PP_OK)
	{
		int res;
		//we got a successful connection, enable reception.
		struct PP_CompletionCallback ccb = {tcp_websocketgot, wsc, PP_COMPLETIONCALLBACK_FLAG_NONE};
		res = ppb_websocket_interface->ReceiveMessage(wsc->sock, &wsc->incomingpacket, ccb);
		if (res != PP_OK_COMPLETIONPENDING)
			tcp_websocketgot(wsc, res);
	}
	else
	{
		Sys_Printf("%s: %i\n", __func__, result);
		//some sort of error connecting, make it timeout now
		wsc->failed = true;
	}
}
static void tcp_websocketclosed(void *user_data, int32_t result)
{
	tcpfile_t *wsc = user_data;
	wsc->failed = true;
	if (wsc->havepacket)
	{
		wsc->havepacket = false;
		ppb_var_interface->Release(wsc->incomingpacket);
	}
	ppb_core->ReleaseResource(wsc->sock);
	wsc->sock = 0;
//	Z_Free(wsc);
}

void VFSTCP_Close (struct vfsfile_s *file)
{
	/*meant to free the memory too, in this case we get the callback to do it*/
	tcpfile_t *wsc = (void*)file;

	struct PP_CompletionCallback ccb = {tcp_websocketclosed, wsc, PP_COMPLETIONCALLBACK_FLAG_NONE};
	ppb_websocket_interface->Close(wsc->sock, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeUndefined(), ccb);
}

int VFSTCP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	tcpfile_t *wsc = (void*)file;
	int res;

	if (wsc->havepacket && wsc->readbuffered < bytestoread + 1024)
	{
		if (wsc->incomingpacket.type == PP_VARTYPE_UNDEFINED)
			Con_Printf("PPAPI bug: var is still undefined after being received\n");
		else
		{
			int len = 0;
			unsigned char *utf8 = (unsigned char *)ppb_var_interface->VarToUtf8(wsc->incomingpacket, &len);
			unsigned char *out = (unsigned char *)wsc->readbuffer + wsc->readbuffered;

			wsc->havepacket = false;

			Con_Printf("Len: %i\n", len);
			while(len && out < wsc->readbuffer + sizeof(wsc->readbuffer))
			{
				if ((*utf8 & 0xe0)==0xc0 && len > 1)
				{
					*out = ((utf8[0] & 0x1f)<<6) | ((utf8[1] & 0x3f)<<0);
					utf8+=2;
					len -= 2;
				}
				else if (*utf8 & 0x80)
				{
					*out = '?';
					utf8++;
					len -= 1;
				}
				else
				{
					*out = utf8[0];
					utf8++;
					len -= 1;
				}
				out++;
			}
			if (len)
			{
				Con_Printf("oh noes! buffer not big enough!\n");
				wsc->failed = true;
			}
			Con_Printf("Old: %i\n", wsc->readbuffered);
			wsc->readbuffered = out - wsc->readbuffer;
			Con_Printf("New: %i\n", wsc->readbuffered);

			ppb_var_interface->Release(wsc->incomingpacket);
			wsc->incomingpacket = PP_MakeUndefined();
		}
		if (!wsc->failed)
		{
			//get the next one
			struct PP_CompletionCallback ccb = {tcp_websocketgot, wsc, PP_COMPLETIONCALLBACK_FLAG_NONE};
			res = ppb_websocket_interface->ReceiveMessage(wsc->sock, &wsc->incomingpacket, ccb);
			if (res != PP_OK_COMPLETIONPENDING)
				tcp_websocketgot(wsc, res);
		}
	}

	if (wsc->readbuffered)
	{
//		Con_Printf("Reading %i bytes of %i\n", bytestoread, wsc->readbuffered);
		if (bytestoread > wsc->readbuffered)
			bytestoread = wsc->readbuffered;

		memcpy(buffer, wsc->readbuffer, bytestoread);
		memmove(wsc->readbuffer, wsc->readbuffer+bytestoread, wsc->readbuffered-bytestoread);
		wsc->readbuffered -= bytestoread;
	}
	else if (wsc->failed)
		bytestoread = -1;	/*signal eof*/
	else
		bytestoread = 0;
	return bytestoread;
}
int VFSTCP_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestowrite)
{
	tcpfile_t *wsc = (void*)file;
	int res;
	int outchars = 0;
	unsigned char outdata[bytestowrite*2+1];
	unsigned char *out=outdata;
	const unsigned char *in=buffer;
	if (wsc->failed)
		return 0;

	for(res = 0; res < bytestowrite; res++)
	{
		/*FIXME: do we need this code?*/
		if (!*in)
		{
			*out++ = 0xc0 | (0x100 >> 6);
			*out++ = 0x80 | (0x100 & 0x3f);
		}
		else if (*in >= 0x80)
		{
			*out++ = 0xc0 | (*in >> 6);
			*out++ = 0x80 | (*in & 0x3f);
		}
		else
			*out++ = *in;
		in++;
		outchars++;
	}
	*out = 0;
	struct PP_Var str = ppb_var_interface->VarFromUtf8(outdata, out - outdata);
	res = ppb_websocket_interface->SendMessage(wsc->sock, str);
//	Sys_Printf("FTENET_WebSocket_SendPacket: result %i\n", res);
	ppb_var_interface->Release(str);

	if (res == PP_OK)
		return bytestowrite;
	return 0;
}

qboolean VFSTCP_Seek (struct vfsfile_s *file, unsigned long pos)
{
	//no seeking allowed
	tcpfile_t *wsc = (void*)file;
	Con_Printf("tcp seek?\n");
	wsc->failed = true;
	return false;
}
unsigned long VFSTCP_Tell (struct vfsfile_s *file)
{
	//no telling allowed
	tcpfile_t *wsc = (void*)file;
	Con_Printf("tcp tell?\n");
	wsc->failed = true;
	return 0;
}
unsigned long VFSTCP_GetLen (struct vfsfile_s *file)
{
	return 0;
}

/*nacl websockets implementation...*/
vfsfile_t *FS_OpenTCP(const char *name, int defaultport)
{
	tcpfile_t *newf;

	netadr_t adr;

	if (!ppb_websocket_interface)
	{
		return NULL;
	}
	if (!NET_StringToAdr(name, defaultport, &adr))
		return NULL;	//couldn't resolve the name
	newf = Z_Malloc(sizeof(*newf));
	if (newf)
	{
		struct PP_CompletionCallback ccb = {tcp_websocketconnected, newf, PP_COMPLETIONCALLBACK_FLAG_NONE};
		newf->sock = ppb_websocket_interface->Create(pp_instance);
		struct PP_Var str = ppb_var_interface->VarFromUtf8(adr.address.websocketurl, strlen(adr.address.websocketurl));
		ppb_websocket_interface->Connect(newf->sock, str, NULL, 0, ccb);
		ppb_var_interface->Release(str);

		newf->funcs.Close = VFSTCP_Close;
		newf->funcs.Flush = NULL;
		newf->funcs.GetLen = VFSTCP_GetLen;
		newf->funcs.ReadBytes = VFSTCP_ReadBytes;
		newf->funcs.Seek = VFSTCP_Seek;
		newf->funcs.Tell = VFSTCP_Tell;
		newf->funcs.WriteBytes = VFSTCP_WriteBytes;
		newf->funcs.seekingisabadplan = true;

		return &newf->funcs;
	}
	return NULL;
}
#else
vfsfile_t *FS_OpenTCP(const char *name, int defaultport)
{
	return NULL;
}
#endif
