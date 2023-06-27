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

struct ftenet_generic_connection_s *net_from_connection;
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
	#define getaddrinfo pgetaddrinfo
	#define freeaddrinfo pfreeaddrinfo
	#define getnameinfo pgetnameinfo

	static int (WSAAPI *getaddrinfo) (
	  const char* nodename,
	  const char* servname,
	  const struct addrinfo* hints,
	  struct addrinfo** res
	);
	static void (WSAAPI *freeaddrinfo) (struct addrinfo*);
	static int (WSAAPI *getnameinfo) (const struct sockaddr *addr, socklen_t addrlen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);
#endif

#if defined(HAVE_IPV4) && defined(HAVE_SERVER)
	#define HAVE_NATPMP
#endif

//#if !defined(HAVE_SERVER) && !defined(MASTERONLY)
//	#undef HAVE_HTTPSV
//#endif

#ifdef HAVE_EPOLL
static int epoll_fd = -1;
#endif

void NET_GetLocalAddress (int socket, netadr_t *out);
//int TCP_OpenListenSocket (const char *localip, int port);
#ifdef HAVE_IPV6
int UDP6_OpenSocket (int port);
#endif
#ifdef HAVE_IPX
void IPX_CloseSocket (int socket);
#endif
cvar_t	net_ice_broker			= CVARFD("net_ice_broker", "tls://master.frag-net.com:27950", CVAR_NOTFROMSERVER, "This is the default broker we attempt to connect through when using 'sv_public /foo' or 'connect /foo'.");
cvar_t	timeout					= CVARD("timeout","65", "Connections will time out if no packets are received for this duration of time.");		// seconds without any message
cvar_t	net_hybriddualstack		= CVARD("net_hybriddualstack",		"1", "Uses hybrid ipv4+ipv6 sockets where possible. Not supported on xp or below.");
cvar_t	net_fakeloss			= CVARFD("net_fakeloss",			"0", CVAR_CHEAT, "Simulates packetloss in both receiving and sending, on a scale from 0 to 1.");
static cvar_t net_dns_ipv4		= CVARD("net_dns_ipv4",				"1", "If 0, disables dns resolution of names to ipv4 addresses (removing any associated error messages). Also hides ipv4 addresses in address:port listings.");
static cvar_t net_dns_ipv6		= CVARD("net_dns_ipv6",				"1", "If 0, disables dns resolution of names to ipv6 addresses (removing any associated error messages). Also hides ipv6 addresses in address:port listings.");
cvar_t	net_enabled				= CVARD("net_enabled",				"1", "If 0, disables all network access, including name resolution and socket creation. Does not affect loopback/internal connections.");
#if defined(HAVE_SSL)
cvar_t	tls_ignorecertificateerrors	= CVARFD("tls_ignorecertificateerrors", "0", CVAR_NOTFROMSERVER|CVAR_NOSAVE|CVAR_NOUNSAFEEXPAND|CVAR_NOSET, "This should NEVER be set to 1!");
static void QDECL NET_TLS_Provider_Changed(struct cvar_s *var, char *oldvalue);
static cvar_t	tls_provider	= CVARFCD("tls_provider", "", CVAR_NOTFROMSERVER, NET_TLS_Provider_Changed, "Controls which TLS provider to use.");
#endif
#if defined(TCPCONNECT) && (defined(HAVE_SERVER) || defined(HAVE_HTTPSV))
#ifdef HAVE_SERVER
cvar_t	net_enable_qizmo		= CVARD("net_enable_qizmo",			"1", "Enables serverside support for 'connect tcp://foo' or 'connect tls://foo' (with net_enable_tls), as well as qizmo's tcp connections and compatibles.");
#endif
#ifdef MVD_RECORDING
cvar_t	net_enable_qtv			= CVARD("net_enable_qtv",			"2", "Controls whether inbound qtv requests will be honoured (both proxies and clients using qtvplay).\n0: Don't accept qtv connections.\n1: Accept connections.\n2: Accept qtv connections only from host-scopped addresses (read: 127.*.*.*, ::1, or unix sockets).");
#endif
#if defined(HAVE_SSL)
cvar_t	net_enable_tls			= CVARD("net_enable_tls",			"0", "If enabled, binary data sent to a non-tls tcp port will be interpretted as a tls handshake (enabling https or wss over the same tcp port.");
#endif
#ifdef HAVE_HTTPSV
#ifdef SV_MASTER
extern ftenet_connections_t *svm_sockets;
cvar_t	net_enable_http			= CVARD("net_enable_http",			"1", "If enabled, tcp ports will accept inbound http clients, potentially serving large files which could distrupt gameplay (This does not affect outgoing http(s) requests).");
cvar_t	net_enable_rtcbroker	= CVARD("net_enable_rtcbroker",		"1", "If 1, tcp ports will accept websocket connections from clients trying to broker direct webrtc connections. This should be low traffic, but might involve a lot of mostly-idle connections.");
cvar_t	net_enable_websockets	= CVARD("net_enable_websockets",	"0", "If enabled, tcp ports will accept websocket game clients.");
#else
cvar_t	net_enable_http			= CVARD("net_enable_http",			"0", "If enabled, tcp ports will accept inbound http clients, potentially serving large files which could distrupt gameplay (This does not affect outgoing http(s) requests).");
cvar_t	net_enable_rtcbroker	= CVARD("net_enable_rtcbroker",		"0", "If 1, tcp ports will accept websocket connections from clients trying to broker direct webrtc connections. This should be low traffic, but might involve a lot of mostly-idle connections.");
cvar_t	net_enable_websockets	= CVARD("net_enable_websockets",	"1", "If enabled, tcp ports will accept websocket game clients.");
#endif
#endif
#endif
#if defined(HAVE_DTLS)
static void QDECL NET_Enable_DTLS_Changed(struct cvar_s *var, char *oldvalue)
{
	var->ival = var->value;
	//set up the default value
	if (!*var->string)
		var->ival = 1;	//FIXME: change to 1 then 2 when better tested.

#if defined(HAVE_SERVER)
	if (svs.sockets)
	{
		svs.sockets->dtlsfuncs = (var->ival)?DTLS_InitServer():NULL;
		if (!svs.sockets->dtlsfuncs && var->ival >= 2)
			Con_Printf("%sUnable to set %s to \"%s\", no DTLS provider available.\n", (var->ival >= 2)?CON_ERROR:CON_WARNING, var->name, var->string);
	}

	{
		char cert[8192];
		int certsize;
		char digest[DIGEST_MAXSIZE];
		certsize = (svs.sockets&&svs.sockets->dtlsfuncs)?svs.sockets->dtlsfuncs->GetPeerCertificate(NULL, QCERT_LOCALCERTIFICATE, cert, sizeof(cert)):-1;
		if (certsize > 0)
			InfoBuf_SetStarBlobKey(&svs.info, "*fp", cert, Base64_EncodeBlockURI(digest, CalcHash(&hash_sha2_256, digest, sizeof(digest), cert, certsize), cert, sizeof(cert)));
		else
			InfoBuf_SetStarKey(&svs.info, "*fp", "");
	}
#endif
}
cvar_t net_enable_dtls		= CVARAFCD("net_enable_dtls", "", "sv_listen_dtls", 0, NET_Enable_DTLS_Changed, "Controls serverside dtls support.\n0: dtls blocked, not advertised.\n1: clientside choice.\n2: used where possible (recommended setting).\n3: disallow non-dtls clients (sv_port_tcp should be eg tls://[::]:27500 to also disallow unencrypted tcp connections).");
cvar_t dtls_psk_hint		= CVARFD("dtls_psk_hint", "", CVAR_NOUNSAFEEXPAND, "For DTLS-PSK handshakes. This specifies the public server identity.");
cvar_t dtls_psk_user		= CVARFD("dtls_psk_user", "", CVAR_NOUNSAFEEXPAND, "For DTLS-PSK handshakes. This specifies the username to use when the client+server's hints match.");
cvar_t dtls_psk_key			= CVARFD("dtls_psk_key",  "", CVAR_NOUNSAFEEXPAND, "For DTLS-PSK handshakes. This specifies the hexadecimal key which must match between client+server. Will only be used when client+server's hint settings match.");
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


static void		*cryptolibmodule[cryptolib_count];
ftecrypto_t *cryptolib[cryptolib_count] =
{
	NULL,
	NULL,
	NULL,
	NULL,
#ifdef HAVE_WINSSPI
	&crypto_sspi,
#endif
#ifdef HAVE_GNUTLS
	&crypto_gnutls,
#endif
};
#if defined(HAVE_SSL)
static void NET_TLS_Provider_Changed(struct cvar_s *var, char *oldvalue)
{
	int i;
	var->ival = 0;
	if (!*var->string || !strcmp(var->string, "0"))
		return;
	for (i = 0; i < cryptolib_count; i++)
	{
		if (cryptolib[i] && !Q_strcasecmp(var->string, cryptolib[i]->drivername))
			var->ival = i+1;
	}
	if (host_initialized && !var->ival)
	{
		Con_Printf("%s: \"%s\" not loaded, valid values are:", var->name, var->string);
		for (i = 0; i < cryptolib_count; i++)
			if (cryptolib[i])
				Con_Printf(" %s", cryptolib[i]->drivername);
		Con_Printf("\n");
	}

#if defined(HAVE_DTLS) && defined(HAVE_SERVER)
	if (net_enable_dtls.string)	//might not be registered yet...
		Cvar_ForceCallback(&net_enable_dtls);
#endif
}
#endif
qboolean NET_RegisterCrypto(void *module, ftecrypto_t *driver)
{
	int i;
	if (!driver)
	{
		for (i = 0; i < cryptolib_count; i++)
			if (cryptolibmodule[i] == module)
				cryptolibmodule[i] = NULL, cryptolib[i] = NULL;
#if defined(HAVE_SSL)
		Cvar_ForceCallback(&tls_provider);
#endif
		return true;
	}
	else
	{
		for (i = 0; i < cryptolib_count; i++)
			if (!cryptolib[i])
			{
				cryptolibmodule[i] = module, cryptolib[i] = driver;
#if defined(HAVE_SSL)
				Cvar_ForceCallback(&tls_provider);
#endif
				return true;
			}
		return false;
	}
}

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
#ifndef HAVE_PACKET
	memset(a, 0, sizeof(*a));
	a->type = NA_INVALID;
#else
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
#endif
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
		if ((a->type == NA_INVALID || b->type == NA_INVALID) && (a->prot==NP_RTC_TCP||a->prot==NP_RTC_TLS)&&(b->prot==NP_RTC_TCP||b->prot==NP_RTC_TLS))
			return true;	//broker stuff can be written as /foo which doesn't necessarily have all the info.
		if (a->port != b->port)
			return false;
#if defined(HAVE_IPV4) && defined(HAVE_IPV6)
		if (a->type == NA_IP && b->type == NA_IPV6)
		{
			int i;
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
			int i;
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
#endif
		return false;
	}

	if (a->type == NA_LOOPBACK)
		return true;

#ifdef HAVE_WEBSOCKCL
	if (a->type == NA_WEBSOCKET)
	{
		if (!strcmp(a->address.websocketurl, b->address.websocketurl) && a->port == b->port)
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

#ifdef SUPPORT_ICE
	if (a->type == NA_ICE)
	{
		if (!strcmp(a->address.icename, b->address.icename))
			return true;
		return false;
	}
#endif

	if (a->type == NA_INVALID && a->prot)
		return true;	//mneh...

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
	{
		if ((a->type == NA_INVALID || b->type == NA_INVALID) && (a->prot==NP_RTC_TCP||a->prot==NP_RTC_TLS)&&(b->prot==NP_RTC_TCP||b->prot==NP_RTC_TLS))
			return true;	//broker stuff can be written as /foo which doesn't necessarily have all the info.
#if defined(HAVE_IPV4) && defined(HAVE_IPV6)
		if (a->type == NA_IP && b->type == NA_IPV6)
		{
			int i;
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
			int i;
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
#endif
		return false;
	}

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

#ifdef SUPPORT_ICE
	if (a->type == NA_ICE)
	{
		if (!strcmp(a->address.icename, b->address.icename))
			return true;
		return false;
	}
#endif

	if (a->type == NA_INVALID && a->prot)
		return true;	//mneh...

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

#if (_POSIX_C_SOURCE >= 200112L || defined(getnameinfo)) && defined(HAVE_PACKET)
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
		if (
		#ifdef getnameinfo
			!getnameinfo ||
		#endif
			getnameinfo((struct sockaddr *)&s, ssz, adrstring, NI_MAXHOST, NULL, 0, NI_NUMERICSERV|NI_DGRAM))
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
#else
void NET_AdrToStringResolve (netadr_t *adr, void (*resolved)(void *ctx, void *data, size_t a, size_t b), void *ctx, size_t a, size_t b)
{
	char adrstring[512];
	NET_BaseAdrToString(adrstring, countof(adrstring), adr);
	resolved(ctx, Z_StrDup(adrstring), a, b);
}
#endif

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
	case NP_DGRAM:	prot = ""/*qw://*/;	break;
	case NP_DTLS:	prot = "dtls://";	break;
	case NP_STREAM:	prot = "tcp://";	break;	//not strictly true for ipx, but whatever.
	case NP_TLS:	prot = "tls://";	break;
	case NP_WS:		prot = "ws://";		break;
	case NP_WSS:	prot = "wss://";	break;
	case NP_NATPMP:	prot = "natpmp://";	break;
	case NP_RTC_TCP:prot = "rtc://";	break;
	case NP_RTC_TLS:prot = "rtcs://";	break;
	}

	switch(a->type)
	{
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:	//ws / wss is part of the url
		{
			char *url = a->address.websocketurl;
			prot = "";
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

#ifdef SUPPORT_ICE
	case NA_ICE:
		snprintf (s, len, "%s[%s]", prot, a->address.icename);
		break;
#endif

	default:
		if (a->prot == NP_RTC_TCP || a->prot == NP_RTC_TLS)
			Q_strncpyz(s, prot, len);
		else
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
	case NP_DGRAM:	prot = ""/*qw://*/;	break;
	case NP_DTLS:	prot = "dtls://";	break;
	case NP_STREAM:	prot = "tcp://";	break;	//not strictly true for ipx, but whatever.
	case NP_TLS:	prot = "tls://";	break;
	case NP_WS:		prot = "ws://";		break;
	case NP_WSS:	prot = "wss://";	break;
	case NP_NATPMP:	prot = "natpmp://";	break;
	case NP_RTC_TCP:prot = "rtc://";	break;
	case NP_RTC_TLS:prot = "rtcs://";	break;
	}

	switch(a->type)
	{
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:	//ws / wss is part of the url
		{
			char *url = a->address.websocketurl;
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

#ifdef SUPPORT_ICE
	case NA_ICE:
		snprintf (s, len, "%s[%s]", prot, a->address.icename);
		break;
#endif

	default:
		if (a->prot == NP_RTC_TCP || a->prot == NP_RTC_TLS)
			Q_strncpyz(s, prot, len);
		else
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

#ifdef WEBCLIENT
	//EVIL HACK!
	//updates.tth uses a known self-signed certificate (to protect against dns hijacks like fteqw.com suffered).
	//its not meant to be used for browsers etc, and I cba to register dns stuff for it.
	//besides, browsers/etc would just bitch about its cert, so w/e.
	//redirect the dns to the base host without affecting http(s) hosts/certificates.
	if (!strncmp(s, "updates.", 8))
	{
		conchar_t musite[256], *e;
		char site[256];
		char *oldprefix = "http://fte.";
		e = COM_ParseFunString(CON_WHITEMASK, ENGINEWEBSITE, musite, sizeof(musite), false);
		COM_DeFunString(musite, e, site, sizeof(site), true, true);
		if (!strncmp(site, oldprefix, strlen(oldprefix)))
		{
			if (!strcmp(s+8,site+strlen(oldprefix)))
			{
#ifdef HAVE_IPV4
				struct sockaddr_in *a = (struct sockaddr_in*)sadr;
				qbyte *ip = (qbyte*)&a->sin_addr;
				memset (a, 0, sizeof(*sadr));
				a->sin_family = AF_INET;
				ip[0] = 213;
				ip[1] = 219;
				ip[2] = 36;
				ip[3] = 248;
				a->sin_port = htons(defaultport);

				if (addrsize)
					*addrsize = sizeof(*a);
				if (addrfamily)
					*addrfamily = AF_INET;
				return 1;
#else
				s += 8;
#endif
			}
		}
	}
#endif

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
#ifdef getaddrinfo
	if (getaddrinfo)
#else
	if (1)
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
				error = getaddrinfo(dupbase, (port[1] == ':')?port+2:NULL, &udp6hint, &addrinfo);
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
				error = getaddrinfo(dupbase, port+1, &udp6hint, &addrinfo);
			}
			else
				error = EAI_NONAME;
			if (error)	//failed, try string with no port.
				error = getaddrinfo(s, NULL, &udp6hint, &addrinfo);	//remember, this func will return any address family that could be using the udp protocol... (ip4 or ip6)
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
		freeaddrinfo (addrinfo);

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
#if defined(HAVE_IPV4) && defined(getaddrinfo) && !defined(HAVE_IPV6)
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

FIXME: should move schemes out of here (so caller can handle paths+etc), using an address family hint for args.
*/
size_t	NET_StringToAdr2 (const char *s, int defaultport, netadr_t *a, size_t numaddresses, const char **pathstart)
{
	size_t result = 0, i;
	struct sockaddr_qstorage sadr[8];
	int asize[countof(sadr)];
	netproto_t prot;
	netadrtype_t afhint;
	char *path;
	char *args;

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

#if defined(SUPPORT_ICE) || defined(HAVE_WEBSOCKCL)
		{"ice://", NP_RTC_TCP, NA_INVALID},
		{"rtc://", NP_RTC_TCP, NA_INVALID},
		{"ices://", NP_RTC_TLS, NA_INVALID},
		{"rtcs://", NP_RTC_TLS, NA_INVALID},
#endif

		{"irc://", NP_INVALID, NA_INVALID},	//should have been handled explicitly, if supported.

#ifdef UNIXSOCKETS
		{"udg://", NP_DGRAM, NA_UNIX},
		{"unix://", NP_STREAM_OR_INVALID, NA_UNIX},
#endif
	};


	memset(a, 0, sizeof(*a)*numaddresses);
	if (pathstart)
		*pathstart = NULL;

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
	if (*s == '/')
	{
		if (!*net_ice_broker.string)
		{	//FIXME: use referrer? or the website's host?
			Con_DPrintf("No default rtc broker\n");
			return 0;	//can't accept it
		}
		a->prot = NP_RTC_TLS;
		a->type = NA_INVALID;

		if (pathstart)
			*pathstart = s;
		return 1;
	}
	else if (!strncmp (s, "rtc://", 6) || !strncmp (s, "rtcs://", 7) || !strncmp (s, "ice://", 6) || !strncmp (s, "ices://", 7))
	{	//basically ICE using sdp-via-websockets to a named relay server.
		const char *path;
		a->prot = NP_RTC_TLS;
		if (s[4] == ':')
		{
			a->prot = NP_RTC_TLS;
			s += 7;
		}
		else
		{
			a->prot = NP_RTC_TCP;
			s += 6;
		}

		path = strchr(s, '/');
		if (path)
		{
			if (s == path)	//no hostname specified = use default broker (resolving it later)
				a->type = NA_INVALID;
			else if (path-s < sizeof(a->address.websocketurl))
			{
				memcpy(a->address.websocketurl, s, path-s);
				a->address.websocketurl[path-s] = 0;
			}
			else
			{
				Con_DPrintf("Path too long\n");
				return 0;	//too long
			}
			if (pathstart)
				*pathstart = path;
		}
		else
		{
			Con_DPrintf("No path\n");
			return 0;	//reject it when there's no path
		}
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
	else if (*net_ice_broker.string)
	{
		a->type = NA_INVALID;	//not quite right, but w/e.
		a->prot = NP_RTC_TLS;
		Q_snprintfz(a->address.websocketurl, sizeof(a->address.websocketurl), "/udp/%s", s);
//		if (numaddresses < 2)
			return 1;

		a++;
		a->type = NA_WEBSOCKET;
		a->prot = NP_WSS;
		Q_snprintfz(a->address.websocketurl, sizeof(a->address.websocketurl), "wss://%s", s);
		return 2;
	}
	else
	{
		/*code for convienience - no other protocols work anyway*/
		static float warned;
		int i;
		for (i = 0; s[i] == ':' || s[i] == '[' || s[i] == ']' || s[i] == '.' || (s[i] >= '0' && s[i] <= '9'); i++)
			;
		a->type = NA_WEBSOCKET;
		if (s[i])
		{	//assume there's part of some domain name in there. FIXME: this may be a false positive in the case of hex ipv6 addresses.
			if (warned < realtime)
			{
				Con_DPrintf("Note: Assuming wss:// prefix\n");
				warned = realtime + 1;
			}
			a->prot = NP_WSS;
			memcpy(a->address.websocketurl, "wss://", 6);
			Q_strncpyz(a->address.websocketurl+6, s, sizeof(a->address.websocketurl)-6);
		}
		else
		{	//looks like a straight ip address.
			//assume most server-by-ip addresses will not have proper certificates set up with that specific ip address as an actual name, and fall back on unsecure rubbish instead.
			if (warned < realtime)
			{
				Con_Printf("Note: Assuming ws:// prefix\n");
				warned = realtime + 1;
			}
			a->prot = NP_WS;
			memcpy(a->address.websocketurl, "ws://", 5);
			Q_strncpyz(a->address.websocketurl+5, s, sizeof(a->address.websocketurl)-5);
		}
		return 1;
	}
#endif

#ifdef IRCCONNECT
	if (!strncmp (s, "irc://", 6))
	{
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
	}
#endif

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

			if (prot == NP_RTC_TCP || prot == NP_RTC_TLS)
				defaultport = PORT_ICEBROKER;
			break;
		}
	}

	args = strchr(s, '?');
	if (args)
		*args=0;

	path = strchr(s, '/');
#if !defined(HAVE_WEBSOCKCL) && defined(SUPPORT_ICE)
	if (path == s)
	{
		if (!*net_ice_broker.string)
			return result;
		s = net_ice_broker.string;
		if (!strncmp(s, "tls://", 6) || !strncmp(s, "wss://", 6))
			s+=6, prot=NP_RTC_TLS;
		else if (!strncmp(s, "tcp://", 6))
			s+=6, prot=NP_RTC_TCP;
		else if (!strncmp(s, "ws://", 5))
			s+=5, prot=NP_RTC_TCP;
		else
			prot = NP_RTC_TLS;	//best-practise by default.
		if (pathstart)
			*pathstart = path;
		result = NET_StringToSockaddr2 (s, PORT_ICEBROKER, afhint, sadr, NULL, asize, min(numaddresses, countof(sadr)));
	}
	else
#endif
	if (path && (path-s)<MAX_OSPATH)
	{
		char host[MAX_OSPATH];
		if (pathstart)
			*pathstart = path;
		memcpy(host, s, path-s);
		host[path-s] = 0;
		result = NET_StringToSockaddr2 (host, defaultport, afhint, sadr, NULL, asize, min(numaddresses, countof(sadr)));
	}
	else
		result = NET_StringToSockaddr2 (s, defaultport, afhint, sadr, NULL, asize, min(numaddresses, countof(sadr)));
	for (i = 0; i < result; i++)
	{
		SockadrToNetadr (&sadr[i], asize[i], &a[i]);
		a[i].prot = prot;
	}

	if (args)
		*args='?';

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
#ifdef SUPPORT_ICE
	case NA_ICE:
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

	//if its ::ffff:a.b.c.d then parse it as ipv4 by just skipping the prefix.
	//we ought to leave it as ipv6, but any printing will show it as ipv4 anyway.
	if (!strncasecmp(s, "::ffff:", 7) && strchr(s+7, '.') && !strchr(s+7, ':'))
		s += 7;

	//multiple colons == ipv6
	//single colon = ipv4:port
	colon = strchr(s, ':');
	if (colon && strchr(colon+1, ':'))
	{
		qbyte *address = a->address.ip6;
		unsigned long tmp;
		int gapstart = -1;	//in bytes...
		bits = 0;

		while(*s)
		{
			tmp = strtoul(s, &colon, 16);
			if (colon == s)
			{
				if (bits)
					return 0;
			}
			else
			{
				if (tmp > 0xffff)
					return 0;	//invalid
				*address++ = (tmp>>8)&0xff;
				*address++ = (tmp>>0)&0xff;
				bits += 16;
			}

			if (bits == 128)
			{
				if (!*colon)
					break;
				return 0;	//must have ended here
			}


			//double-colon is a gap (or partial end).
			//hopefully the last 64 bits or whatever will be irrelevant anyway, so such addresses won't be common
			if (colon[0] == ':' && colon[1] == ':')
			{
				if (gapstart >= 0)
					return 0; //only one gap...
				if (!colon[2])
					break;	//nothing after. its partial.
				gapstart = bits/8;
				colon+=2;
			}
			else if (*colon == ':' && bits)
				colon++;
			else if (*colon)
				return 0; //gibberish here...
			else
				break;	//end of address... anything more is a partial.
			s = colon;
		}
		if (gapstart >= 0)
		{
			int tailsize = (bits/8)-gapstart;	//bits to move to the end
			int gapsize = 16 - gapstart - tailsize;
			memmove(a->address.ip6+gapstart+gapsize, a->address.ip6+gapstart, tailsize);	//move the bits we found to the end
			memset(a->address.ip6+gapstart, 0, gapsize);	//and make sure the gap is cleared
			bits = 128;	//found it all, or something.
		}
		if (!bits)
			bits = 1;	//FIXME: return of 0 is an error, but :: is 0-length... lie.
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
				port = htons(strtoul(s+1, &address, 10));
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

qboolean NET_StringToAdr_NoDNS(const char *address, int port, netadr_t *out)
{
	int peerbits;
	if (*address == '[')
	{
		char *close = strchr(address+1, ']');
		if (close)
			*close = 0;
		peerbits = NET_StringToAdr_NoDNS(address+1, 0, out);
		if (close)
		{
			*close = ']';
			if (close[1] == ':')
				out->port = htons(atoi(close+2));
		}
		return peerbits;
	}
	else
	{
		peerbits = ParsePartialIP(address, out);
		if (out->type == NA_IP && peerbits == 32)
		{
			//ignore invalid addresses
			if (!out->address.ip[0] && !out->address.ip[1] && !out->address.ip[2] && !out->address.ip[3])
				out->type = NA_INVALID;
		}
		else if (out->type == NA_IPV6 && peerbits == 128)
		{
			//ignore invalid addresses
			int i;
			for (i = 0; i < countof(out->address.ip6); i++)
				if (out->address.ip6[i])
					break;
			if (i == countof(out->address.ip6))
				out->type = NA_INVALID;
		}
		else
			out->type = NA_INVALID;

		return out->type != NA_INVALID;
	}
}

qboolean NET_IsEncrypted(netadr_t *adr)
{
	if (adr->type == NA_LOOPBACK)
		return true;	//might as well claim it, others can't snoop on it so...
#ifdef SUPPORT_ICE
	if (adr->type == NA_ICE && ICE_GetPeerCertificate(adr, QCERT_ISENCRYPTED, NULL, 0)==0)
		return true;
#endif
#if defined(FTE_TARGET_WEB)
	if (adr->prot == NP_RTC_TLS)	//web port works a bit differently... webrtc is ALWAYS encrypted, but only report it as secure when the broker connection is encrypted too.
		return true;
#endif
	if (adr->prot == NP_DTLS || adr->prot == NP_TLS || adr->prot == NP_WSS)
		return true;
	return false;
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


void *Auth_GetKnownCertificate(const char *certname, size_t *size)
{	//our 'code signing' certs
	//we only allow packages to be installed into the root dir (or with dll/so/exe extensions) when their signature is signed by one of these certificates
#ifdef HAVE_SSL
	static struct
	{
		const char *name;
		qbyte *cert;
	} certs[] =
	{	//the contents of a -pubcert FILE
		//note: not enforced for pk3 files (which we will otherwise happily randomly download of random servers anyway).
		{"Spike",	"-----BEGIN CERTIFICATE-----\n"
					"MIIDnTCCAgUCCjE1ODQ4ODg2OTEwDQYJKoZIhvcNAQELBQAwEDEOMAwGA1UEAxMF\n"
					"U3Bpa2UwHhcNMjAwMzIyMTQ1MTMwWhcNMzAwMzIwMTQ1MTMxWjAQMQ4wDAYDVQQD\n"
					"EwVTcGlrZTCCAaIwDQYJKoZIhvcNAQEBBQADggGPADCCAYoCggGBAN07KHTPc0Pn\n"
					"lC8MlQNiI+OEUEJBakTjfNq+IJzJ6oTWXxfQbHrN+UpKXwxploDbeyxTE1Fniisi\n"
					"nLWhOkW/XQqyXLXAv/3lxiSwe4QcVMOhQlw5U05VrdB7xHvFMShLsyc12sNvBiIa\n"
					"Vw05wFJgIXYTd9nJfm9x3kpxRoTJBvdDbYl8OagT6SxzJfHkdmfI2TYVYxGUH9nX\n"
					"R9zvwXTIXvV47cko2ON8scH9QQ6KgwMfcwyIBL74Btvl4ye+TrL2srj38FBxyyPG\n"
					"SSdKPk4LN2zsfNsJm31hzWrdLEkl3CTOX5gHHSweOKpuPwmX/GPd1xo0nIMwDou4\n"
					"BsMMBAhK/JSyLpUUzk5gbRmy4PwFccktHdFW6LF8ZvPY7e7LEiD5KOWZ7a7c1WR/\n"
					"4oJrjo0t+7OugVADolxzLXFrq9ACBGrD8r6QlsGC8O7WqpKGQCT+4q3tUup9tPkh\n"
					"3dhjC0jEkKljS+39uukbisV702bHwoEZPzjMpz4O9bHf6JbIJLlQzQIDAQABMA0G\n"
					"CSqGSIb3DQEBCwUAA4IBgQC5rj7R7a9LLnqgiXMUITGnygK1lp0EV2BdnIrg/MHr\n"
					"y+Gk9BA+XgFSI4W9odiG/hJnA7aQ0S2kk1GNYQ+NNzU2bQIMkaobaZApV9ojD4lL\n"
					"s33Qbgt/Ocpadtpj8EiMInjLkn1B+wnqcX3S76Zcrf8RT4WP2A4klxcN3zBNBiBL\n"
					"DAJ3SrH8hZ9wmruwAY5tMZhQzDHkeK8uaDb7nE0HA5GXeT4QYA/L7Ys2nGYgxj1O\n"
					"L5YlGddBcX3O6XyJpSeCO2Z2kwl4qg8oiM+Y546lILotuL5qD/+FTDeX3dGd8nyD\n"
					"e1g/7xd0V4IyKUjii8Vu2V1F7t0xVTPWEe13TqU/JTfKX4zvQnMF7zxgGFIwabHX\n"
					"lzk2olte4rPp+iQzPmnynLiUrdkxGXLnE0V545VO+iGO8+bwclbJ+7SG6N5l8xox\n"
					"WjGunhXXkEjitAk+ssBjbEh8kIfpFdVA09v60rMdm7BdfO3//QOsjwiwKkBOXcYW\n"
					"QGE0Ue4J7anLVAKiQq4n1aU=\n"
					"-----END CERTIFICATE-----\n"},
	};
	size_t i;
	for (i = 0; i < countof(certs); i++)
	{
		if (!strcmp(certname, certs[i].name))
		{
			*size = strlen(certs[i].cert);
			return certs[i].cert;
		}
	}
#endif
	return NULL;
}
#ifdef HAVE_SSL
void *TLS_GetKnownCertificate(const char *certname, size_t *size)
{
	//Note: This is XORed because of shitty scanners flagging binaries through false positive, flagging the sites that they were downloaded from, flagging binaries that contain references to those sites, and flagging any site that contains binaries.
	//the xor helps break that shitty recursive loop of mistrust from defects in other people's code.
	//at least until there's a sandbox that checks the dns resolutions for our update requests anyway.
	//I should probably just copy the downloadables file to sourceforge.
	//FIXME: we should be signing the content, not the sender. this SHOULD become redundant.
	static struct
	{
		qbyte *data;
	} knowncerts[] = {
		{
		/*updates.triptohell.info*/"\x8a\x8f\x9b\x9e\x8b\x9a\x8c\xd1\x8b\x8d\x96\x8f\x8b\x90\x97\x9a\x93\x93\xd1\x96\x91\x99\x90\xff\xc2\x03\xcf\x7d\xfc\x41\xcf\x7d\xfd\x59\x5f\xfc\xfd\xfe\xfd\xfd\xf6\xff\x0b\xb1\xec\xf7\xcd\x7a\x4a\xe6\xcf\xf2\xf9\xf6\xd5\x79\xb7\x79\x08\xf2\xfe\xfe\xf4\xfa\xff\xcf\x8b\xce\xf4\xcf\xf6\xf9\xfc\xaa\xfb\xf9\xec\xfd\xaa\xb4\xce\xef\xcf\xf1\xf9\xfc\xaa\xfb\xf7\xf3\xf8\xba\x91\x98\x93\x9e\x91\x9b\xce\xf0\xcf\xf2\xf9\xfc\xaa\xfb\xf8\xf3\xf9\xb3\x90\x91\x9b\x90\x91\xce\xf1\xcf\xf3\xf9\xfc\xaa\xfb\xf5\xf3\xfa\xb9\xab\xba\xae\xa8\xce\xef\xcf\xf1\xf9\xfc\xaa\xfb\xf4\xf3\xf8\xaa\x8f\x9b\x9e\x8b\x9a\x8c\xce\xdf\xcf\xe1\xf9\xfc\xaa\xfb\xfc\xf3\xe8\x8a\x8f\x9b\x9e\x8b\x9a\x8c\xd1\x8b\x8d\x96\x8f\x8b\x90\x97\x9a\x93\x93\xd1\x96\x91\x99\x90\xcf\xe1\xe8\xf2\xce\xc6\xcf\xca\xcc\xce\xce\xcf\xcf\xc6\xce\xc6\xa5\xe8\xf2\xcd\xc6\xcf\xca\xcd\xc7\xce\xcf\xcf\xc6\xce\xc6\xa5\xcf\x8b\xce\xf4\xcf\xf6\xf9\xfc\xaa\xfb\xf9\xec\xfd\xaa\xb4\xce\xef\xcf\xf1\xf9\xfc\xaa\xfb\xf7\xf3\xf8\xba\x91\x98\x93\x9e\x91\x9b\xce\xf0\xcf\xf2\xf9\xfc\xaa\xfb\xf8\xf3\xf9\xb3\x90\x91\x9b\x90\x91\xce\xf1\xcf\xf3\xf9\xfc\xaa\xfb\xf5\xf3\xfa\xb9\xab\xba\xae\xa8\xce\xef\xcf\xf1\xf9\xfc\xaa\xfb\xf4\xf3\xf8\xaa\x8f\x9b\x9e\x8b\x9a\x8c\xce\xdf\xcf\xe1\xf9\xfc\xaa\xfb\xfc\xf3\xe8\x8a\x8f\x9b\x9e\x8b\x9a\x8c\xd1\x8b\x8d\x96\x8f\x8b\x90\x97\x9a\x93\x93\xd1\x96\x91\x99\x90\xcf\x7d\xfe\xdd\xcf\xf2\xf9\xf6\xd5\x79\xb7\x79\x08\xf2\xfe\xfe\xfe\xfa\xff\xfc\x7d\xfe\xf0\xff\xcf\x7d\xfe\xf5\xfd\x7d\xfe\xfe\xff\x55\x4a\x63\x33\x17\x42\x52\xe3\x80\x94\xe3\x36\xfb\x19\xd3\xef\x53\x66\x14\x98\xf4\x63\xdb\x47\x6f\x88\x21\x55\xbb\x5d\xea\xce\x9e\xad\xb2\x14\x70\xa9\x47\x50\x3e\xd0\x99\x22\xaa\x72\x29\x13\x3c\x5b\x6c\x73\x79\x14\x50\x76\xe8\xe6\xd1\x93\x3d\x2b\x06\x6d\x53\xd1\x8c\x66\xa9\x0d\x3c\x3b\xeb\xa9\xb5\xf2\x41\xae\x36\x70\xb1\x6d\xdf\xd4\x51\xfa\xf3\x81\x78\x5a\xfd\x1e\x3f\x82\x8e\x58\xc7\x8d\xb8\xc0\xce\xf8\x6f\x4f\x92\x30\x51\x49\x24\x14\xc6\x55\xa0\x4b\x90\xf3\x9c\xd5\xde\x9a\xc9\x55\x94\x53\x68\x49\x41\xdf\x5b\x78\xc9\x40\xca\x3a\x59\xce\x1b\x62\x7a\x0c\x51\x70\x94\x07\xa6\x8a\xf0\x4a\xa2\xce\xbf\xc6\xd1\x15\xb7\x9a\x20\x6e\x1c\xf9\x04\x4d\x13\x23\x2f\x6f\x6b\x29\x97\xb2\x9d\xda\x65\xc2\x3c\x8b\xe8\x82\xf1\x1d\xe1\xcb\x40\xfd\x7a\x3b\xbf\x77\x6e\x14\x1f\x0a\x6d\xa9\xbd\xb0\x59\xb3\xe8\x77\x4d\x76\x2d\x13\x9f\xab\x68\xdf\xf5\x35\x0f\x2e\xcc\xc0\xa4\x99\x48\x75\xbd\x8d\x98\x36\xb8\x7c\x4c\x2b\xe1\x57\xbb\x40\xa5\xe5\x7a\x86\x11\x07\x7f\x21\xe6\xe2\x3a\x22\xaf\xbd\xef\xe8\x48\x3c\x2b\x0e\x34\x75\x47\x8e\xaa\xce\xfd\xfc\xfe\xff\xfe\x5c\xac\xcf\xae\xcf\xe2\xf9\xfc\xaa\xe2\xf1\xfb\xe9\xfb\xeb\xd3\x97\x7e\x70\xbf\x73\xbf\xbd\x60\x42\x3a\xf4\xc9\x04\x1d\x89\x14\x72\x4b\x0c\xcf\xe0\xf9\xfc\xaa\xe2\xdc\xfb\xe7\xcf\xe9\x7f\xeb\xd3\x97\x7e\x70\xbf\x73\xbf\xbd\x60\x42\x3a\xf4\xc9\x04\x1d\x89\x14\x72\x4b\x0c\xcf\xf0\xf9\xfc\xaa\xe2\xec\xfe\xfe\x00\xfb\xfa\xcf\xfc\xfe\xfe\x00\xcf\xf2\xf9\xf6\xd5\x79\xb7\x79\x08\xf2\xfe\xfe\xf4\xfa\xff\xfc\x7d\xfe\xfe\xff\xf3\xfe\xca\xcd\x47\x18\x69\x45\xc1\xac\x73\x87\xbe\x54\x64\x80\x1d\x85\x7f\xa5\x36\x77\xf9\xd6\xd7\x06\xaf\x80\x33\x49\x33\xcb\xfc\xba\xcd\x86\x9c\x18\x21\x63\xb9\xd6\x0b\x50\xcd\x8d\xd9\xee\x5f\x84\xad\xdc\xf5\x2a\xae\x6e\x86\x0d\xaf\x9e\x7f\x8d\xbf\x18\x7a\x4f\xec\xe2\x67\x24\xeb\xdc\xa6\x5b\x43\x16\x1f\xe4\x3f\xc7\xcc\x69\x43\x44\xa9\xb8\x33\x42\x17\xbf\xb6\x20\x55\x9b\x81\xd6\x1a\x62\xbf\x5a\xe5\xa3\xba\xe0\xa5\x88\xa6\x01\x85\x47\x07\xb2\x3b\x64\xce\x19\xf7\x3b\x6a\x05\x6e\x70\x6e\x60\xc3\x3b\x7d\x46\x0e\x92\x57\x59\x3b\xf6\x4e\x16\x57\x9f\x64\x55\xb3\x86\x0f\x66\x47\x52\x9c\x4e\x1b\x3f\x50\x0f\x20\x36\xcc\xac\xb2\xf6\x1b\xc0\x72\x61\xc7\x39\x6c\x00\x33\x6e\xb9\x81\x98\xd7\x9e\x55\x38\xf4\x1d\x27\x73\x1b\x13\x72\xbb\x18\x95\xeb\x87\x6e\x82\x13\x38\xf8\x12\x36\xa7\x24\xca\x2b\x8f\xf9\xf9\xc6\x72\xb4\x7f\xd3\x49\x57\x86\xa3\x6b\xea\x93\xcb\xf9\xa3\x28\x3a\xbd\x3f\x8d\xfe\x8e\xf8\x0a\xda\x95\x2f\xdb\x79\x32\xe4\xde\xf8\x51\xbf\x07\x3e\x1b\xdc\xf2\x5f\x3f\xdc\x0f\xf8\x45\x23\xcb\xa2\xb8\x30\xb4\x84\x2a\xa2",
		},
	};

	qbyte *r, *t;
	size_t i, j, sz;

	for (i = 0; i < countof(knowncerts); i++)
	{
		t = knowncerts[i].data;
		for (j = 0; ; j++)
		{
			if (certname[j] != (t[j]^0xff))
				break;
			if (!certname[j])
			{
				j++;
				t+=j;
				sz = t[0] | (t[1]<<8);
				t += 2;

				r = BZ_Malloc(sz);
				*size = sz;
				while(sz --> 0)
					r[sz] = t[sz]^0xff;
				return r;
			}
		}
	}

	*size = 0;
	//return Z_StrDup(""); //to force failure... gnutls debug code will dump out a cert that can be inserted above.
	return NULL;
}

vfsfile_t *FS_OpenSSL(const char *peername, vfsfile_t *source, qboolean isserver)
{
	int i;
	vfsfile_t *f = NULL;
	char hostname[MAX_OSPATH];

	if (!source)
		return NULL;	//can happen if socket() fails.

	if (peername)
	{
		//clean up the name, stripping any port or other weirdness.
		if (!strncmp(peername, "tls://", 6))
			peername+=6;
		if (*peername == '[')
		{	//an ipv6 address, strip the brackets (and trailing port)
			Q_strncpyz(hostname, peername+1, sizeof(hostname));
			if (strchr(hostname, ']'))
				*strchr(hostname, ']') = 0;
		}
		else
		{	//a hostname or ipv4 address, strip the port.
			Q_strncpyz(hostname, peername, sizeof(hostname));
			if (strchr(hostname, ':'))
				*strchr(hostname, ':') = 0;
		}
	}
	else
		*hostname = 0;

	i = tls_provider.ival-1;
	if (i>=0 && i < cryptolib_count && cryptolib[i])
		f = !cryptolib[i]->OpenStream?NULL:cryptolib[i]->OpenStream(hostname, source, isserver);
	else for (i = 0; !f && i < cryptolib_count; i++)
	{
		if (cryptolib[i] && cryptolib[i]->OpenStream)
			f = cryptolib[i]->OpenStream(hostname, source, isserver);
	}
	if (!f)	//it all failed.
	{
		if (isserver && i < cryptolib_count && cryptolib[i] && cryptolib[i]->OpenStream)
		{
			Con_Printf("%s: no tls provider available. You may need to create a public certificate\n", peername?peername:"<HOST>");
		}
		else
			Con_Printf("%s: no tls provider available\n", peername);
		VFS_CLOSE(source);
	}
	return f;
}
int TLS_GetChannelBinding(vfsfile_t *stream, qbyte *data, size_t *datasize)
{
	int r = -1;
	int i;
	for (i = 0; r==-1 && i < cryptolib_count; i++)
	{
		if (cryptolib[i] && cryptolib[i]->GetChannelBinding)
			r = cryptolib[i]->GetChannelBinding(stream, data, datasize);
	}
	return r;
}
#endif

/////////////////////////////////////////////
//loopback stuff

#if defined(HAVE_SERVER) && defined(HAVE_CLIENT)

static qboolean	NET_GetLoopPacket (int sock, netadr_t *from, sizebuf_t *message)
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


static neterr_t NET_SendLoopPacket (int sock, int length, const void *data, netadr_t *to)
{
	int		i;
	loopback_t	*loop;
	if (!length && !data)	//NET_EnsureRoute tests.
		return NETERR_SENT;

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

static int FTENET_Loop_GetLocalAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
{
	if (maxaddresses)
	{
		addresses->type = NA_LOOPBACK;
		addresses->port = con->thesocket+1;
		*adrflags = 0;
		*adrparams = NULL;
		return 1;
	}
	return 0;
}

static qboolean FTENET_Loop_GetPacket(ftenet_generic_connection_t *con)
{
	return NET_GetLoopPacket(con->thesocket, &net_from, &net_message);
}

#if defined(HAVE_PACKET) && !defined(HAVE_EPOLL)
//just a null function so we don't pass bad things to select.
static int FTENET_Loop_SetFDSets(ftenet_generic_connection_t *gcon, fd_set *readfdset, fd_set *writefdset)
{
	return 0;
}
#endif

static neterr_t FTENET_Loop_SendPacket(ftenet_generic_connection_t *con, int length, const void *data, netadr_t *to)
{
	if (to->type == NA_LOOPBACK)
	{
		return NET_SendLoopPacket(con->thesocket, length, data, to);
	}

	return NETERR_NOROUTE;
}

static void FTENET_Loop_Close(ftenet_generic_connection_t *con)
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

static ftenet_generic_connection_t *FTENET_Loop_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr)
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
#if defined(HAVE_PACKET) && !defined(HAVE_EPOLL)
		newcon->SetFDSets = FTENET_Loop_SetFDSets;
#endif

		newcon->islisten = col->islisten;
		newcon->addrtype[0] = NA_LOOPBACK;
		newcon->addrtype[1] = NA_INVALID;

		newcon->thesocket = sock;
	}
	return newcon;
}
#endif
//=============================================================================

ftenet_connections_t *FTENET_CreateCollection(qboolean listen, void(*ReadPacket)(void))
{
	ftenet_connections_t *col;
	col = Z_Malloc(sizeof(*col));
	col->islisten = listen;
	col->ReadGamePacket = ReadPacket;
	return col;
}
#if defined(HAVE_CLIENT) && defined(HAVE_SERVER)
static ftenet_generic_connection_t *FTENET_Loop_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr);
#endif
#ifdef HAVE_PACKET
ftenet_generic_connection_t *FTENET_Datagram_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr);
#endif
#ifdef TCPCONNECT
static ftenet_generic_connection_t *FTENET_TCP_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr);
#endif
#ifdef HAVE_WEBSOCKCL
static ftenet_generic_connection_t *FTENET_WebSocket_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr);
static ftenet_generic_connection_t *FTENET_WebRTC_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr);
#endif
#ifdef IRCCONNECT
static ftenet_generic_connection_t *FTENET_IRCConnect_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr);
#endif
#ifdef HAVE_NATPMP
static ftenet_generic_connection_t *FTENET_NATPMP_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr);
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

int FTENET_NATPMP_GetLocalAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses);
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

					Con_DPrintf("NAT-PMP: Local port %u publicly available on port %u\n", (unsigned short)BigShort(pmpreqrep->privport), (unsigned short)BigShort(pmpreqrep->pubport));
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
	const char *params[sizeof(addr)/sizeof(addr[0])];

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

	m = NET_EnumerateAddresses(collection, con, flags, addr, params, sizeof(addr)/sizeof(addr[0]));

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
int FTENET_NATPMP_GetLocalAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
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
		*adrparams = NULL;
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
ftenet_generic_connection_t *FTENET_NATPMP_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t pmpadr)
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

static void NET_DTLS_Timeouts(ftenet_connections_t *col)
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
	int i;
	const char *provname;
	if (tls_provider.ival>0 && tls_provider.ival <= cryptolib_count && cryptolib[tls_provider.ival-1])
	{
		f = !cryptolib[tls_provider.ival-1]->DTLS_InitServer?NULL:cryptolib[tls_provider.ival-1]->DTLS_InitServer();
		provname = cryptolib[tls_provider.ival-1]->drivername;
	}
	else for (i = 0; !f && i < cryptolib_count; i++)
	{
		if (cryptolib[i] && cryptolib[i]->DTLS_InitServer)
		{
			f = cryptolib[i]->DTLS_InitServer();
			provname = cryptolib[i]->drivername;
			if (!f)
				Con_Printf("DTLS provider %s failed\n", cryptolib[i]->drivername);
		}
	}
	if (f)
		Con_DPrintf("Using DTLS provider %s\n", provname);
	return f;
}
const dtlsfuncs_t *DTLS_InitClient(void)
{
	const dtlsfuncs_t *f = NULL;
	int i;
	if (tls_provider.ival>0 && tls_provider.ival <= cryptolib_count && cryptolib[tls_provider.ival-1])
		f = !cryptolib[tls_provider.ival-1]->DTLS_InitClient?NULL:cryptolib[tls_provider.ival-1]->DTLS_InitClient();
	else for (i = 0; !f && i < cryptolib_count; i++)
	{
		if (cryptolib[i] && cryptolib[i]->DTLS_InitClient)
			f = cryptolib[i]->DTLS_InitClient();
	}
	return f;
}

static neterr_t NET_SendPacketCol (ftenet_connections_t *collection, int length, const void *data, netadr_t *to);
static neterr_t FTENET_DTLS_DoSendPacket(void *cbctx, const qbyte *data, size_t length)
{	//callback that does the actual sending
	struct dtlspeer_s *peer = cbctx;
	return NET_SendPacketCol(peer->col, length, data, &peer->addr);
}
qboolean NET_DTLS_Create(ftenet_connections_t *col, netadr_t *to, const dtlscred_t *cred, qboolean outgoing)
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
		peer = Z_Malloc(sizeof(*peer));
		peer->addr = *to;
		peer->col = col;

		if (outgoing)
			peer->funcs = DTLS_InitClient();
		else
			peer->funcs = DTLS_InitServer();
		if (peer->funcs)
			peer->dtlsstate = peer->funcs->CreateContext(cred, peer, FTENET_DTLS_DoSendPacket, !outgoing);

		peer->timeout = realtime+timeout.value;
		if (peer->dtlsstate)
		{
			peer->link = &col->dtls;
			peer->next = col->dtls;
			if (peer->next)
				peer->next->link = &peer->next;
			col->dtls = peer;
		}
		else
		{
			Z_Free(peer);
			peer = NULL;
		}
	}
	else
		peer->timeout = realtime+timeout.value;
	return peer!=NULL;
}
#ifdef HAVE_SERVER
static void FTENET_DTLS_Established(void **ctx, void *state)
{
	ftenet_connections_t *col;
	struct dtlspeer_s *peer = Z_Malloc(sizeof(*peer));
	memcpy(peer, *ctx, sizeof(*peer));
	*ctx = peer;
	col = peer->col;

	peer->dtlsstate = state;

	peer->timeout = realtime+timeout.value;
	peer->link = &col->dtls;
	peer->next = col->dtls;
	if (peer->next)
		peer->next->link = &peer->next;
	col->dtls = peer;
}
qboolean NET_DTLS_CheckInbound(ftenet_connections_t *col)
{
	extern cvar_t timeout;
	struct dtlspeer_s *peer;
	netadr_t *from = &net_from;
	if (from->prot != NP_DGRAM || !net_enable_dtls.ival || !col->dtlsfuncs)
		return false;
	if (!net_message.cursize || !(20 <= net_message.data[0] && net_message.data[0] <= 63))
		return false;	//lead byte must be between 20 and 63 to be valid dtls.
	for (peer = col->dtls; peer; peer = peer->next)
	{
		if (NET_CompareAdr(&peer->addr, from))
			break;
	}
	if (!peer)
	{
		if (col->dtlsfuncs->CheckConnection)
		{
			struct dtlspeer_s peer;
			//fill it with preliminary info
			peer.addr = *from;
			peer.col = col;
			peer.funcs = col->dtlsfuncs;
			return col->dtlsfuncs->CheckConnection(&peer, from, sizeof(*from), net_message.data, net_message.cursize, FTENET_DTLS_DoSendPacket, FTENET_DTLS_Established);
		}
	}
	return false;
}
#endif
static void NET_DTLS_DisconnectPeer(ftenet_connections_t *col, struct dtlspeer_s *peer)
{
//	Sys_Printf("Destroy %p\n", peer->dtlsstate);
	if (peer->next)
		peer->next->link = peer->link;
	*peer->link = peer->next;

	peer->funcs->DestroyContext(peer->dtlsstate);
	Z_Free(peer);
}
static struct dtlspeer_s *FTENET_DTLS_FindPeer(ftenet_connections_t *col, netadr_t *to)
{
	struct dtlspeer_s *peer;
	for (peer = col->dtls; peer; peer = peer->next)
	{
		if (NET_CompareAdr(&peer->addr, to))
			break;
	}
	return peer;
}
qboolean NET_DTLS_Disconnect(ftenet_connections_t *col, netadr_t *to)
{
	struct dtlspeer_s *peer;
	netadr_t n = *to;
	if (!col || (to->prot != NP_DGRAM && to->prot != NP_DTLS))
		return false;
	n.prot = NP_DGRAM;
	peer = FTENET_DTLS_FindPeer(col, &n);
	if (!peer)
		return false;
	NET_DTLS_DisconnectPeer(col, peer);
	return true;
}
static neterr_t FTENET_DTLS_SendPacket(ftenet_connections_t *col, int length, const void *data, netadr_t *to)
{
	struct dtlspeer_s *peer;
	to->prot = NP_DGRAM;
	peer = FTENET_DTLS_FindPeer(col, to);
	to->prot = NP_DTLS;
	if (peer)
		return peer->funcs->Transmit(peer->dtlsstate, data, length);
	else
		return NETERR_NOROUTE;
}

qboolean NET_DTLS_Decode(ftenet_connections_t *col)
{
	extern cvar_t timeout;
	struct dtlspeer_s *peer = FTENET_DTLS_FindPeer(col, &net_from);
	if (peer)
	{
		peer->timeout = realtime+timeout.value;	//refresh the timeout if our peer is still alive.
		switch(peer->funcs->Received(peer->dtlsstate, &net_message))
		{
		case NETERR_DISCONNECTED:
			if (col->islisten)
				NET_DTLS_DisconnectPeer(col, peer);
			net_message.cursize = 0;
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
			net_from.prot = NP_DTLS;
			break;
		}
		net_from.prot = NP_DTLS;
		return true;
	}
	return false;
}
#endif


int NET_GetConnectionCertificate(struct ftenet_connections_s *col, netadr_t *a, enum certprops_e prop, char *out, size_t outsize)
{
	if (!col)
		return -1;

#ifdef SUPPORT_ICE
	if (a->type == NA_ICE)
		return ICE_GetPeerCertificate(a, prop, out, outsize);
#endif
#ifdef HAVE_DTLS
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
		if (peer && peer->funcs->GetPeerCertificate)
			return peer->funcs->GetPeerCertificate(peer->dtlsstate, prop, out, outsize);
	}
#endif
	return -1;
}




static qboolean FTENET_AddToCollection_Ptr(ftenet_connections_t *col, const char *name, ftenet_generic_connection_t *(*establish)(ftenet_connections_t *col, const char *address, netadr_t adr), const char *address, netadr_t *adr)
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
				if (adr && (adr->type != NA_INVALID||adr->prot != NP_INVALID) && col->islisten)
				if (col->conn[i]->ChangeLocalAddress)
				{
					if (col->conn[i]->ChangeLocalAddress(col->conn[i], address, adr))
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
				col->conn[i] = establish(col, address, *adr);
				if (!col->conn[i])
					break;
				col->conn[i]->connum = i+1;
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
	netadr_t adr[8];
	ftenet_generic_connection_t *(*establish[countof(adr)])(ftenet_connections_t *col, const char *address, netadr_t adr);
	char address[countof(adr)][256];
	unsigned int i, j;
	qboolean success = false;
	if (!col)
		return false;
	if (name && strchr(name, ':'))
		return false;
	for (i = 0; addresslist && *addresslist && i < countof(adr); i++)
	{
		addresslist = COM_ParseStringSet(addresslist, address[i], sizeof(address[i]));
		//resolve the address to something sane so we can determine the address type and thus the connection type to use
		if (!*address[i])
			adr[i].type = NA_INVALID, adr[i].prot = NP_INVALID;
		else //if (islisten)
		{
			if (!NET_PortToAdr(addrtype, addrprot, address[i], &adr[i]))
				return false;
		}
#ifdef HAVE_WEBSOCKCL
		if (adr[i].prot == NP_WS && adr[i].type == NA_WEBSOCKET)	establish[i] = FTENET_WebSocket_EstablishConnection; else
		if (adr[i].prot == NP_WSS && adr[i].type == NA_WEBSOCKET)	establish[i] = FTENET_WebSocket_EstablishConnection; else
		if (adr[i].prot == NP_RTC_TCP)								establish[i] = FTENET_WebRTC_EstablishConnection; else
		if (adr[i].prot == NP_RTC_TLS)								establish[i] = FTENET_WebRTC_EstablishConnection; else
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
		if (adr[i].prot == NP_STREAM&& adr[i].type == NA_UNIX)	establish[i] = FTENET_TCP_EstablishConnection;	else
		if (adr[i].prot == NP_WS    && adr[i].type == NA_UNIX)	establish[i] = FTENET_TCP_EstablishConnection;	else
		if (adr[i].prot == NP_TLS    && adr[i].type == NA_UNIX)	establish[i] = FTENET_TCP_EstablishConnection;	else
	#endif
#endif
#if defined(TCPCONNECT) && defined(HAVE_IPV4)
		if (adr[i].prot == NP_WS	&& adr[i].type == NA_IP)	establish[i] = FTENET_TCP_EstablishConnection;	else
		if (adr[i].prot == NP_STREAM&& adr[i].type == NA_IP)	establish[i] = FTENET_TCP_EstablishConnection;	else
		if (adr[i].prot == NP_TLS	&& adr[i].type == NA_IP)	establish[i] = FTENET_TCP_EstablishConnection;	else
#endif
#if defined(TCPCONNECT) && defined(HAVE_IPV6)
		if (adr[i].prot == NP_WS	&& adr[i].type == NA_IPV6)	establish[i] = FTENET_TCP_EstablishConnection;	else
		if (adr[i].prot == NP_STREAM&& adr[i].type == NA_IPV6)	establish[i] = FTENET_TCP_EstablishConnection;	else
		if (adr[i].prot == NP_TLS	&& adr[i].type == NA_IPV6)	establish[i] = FTENET_TCP_EstablishConnection;	else
#endif
#ifdef SUPPORT_ICE
		if (adr[i].prot == NP_RTC_TCP)		establish[i] = FTENET_ICE_EstablishConnection;	else
		if (adr[i].prot == NP_RTC_TLS)		establish[i] = FTENET_ICE_EstablishConnection;	else
#endif
#ifdef IRCCONNECT
		if (adr[i].prot == NP_TLS)								establish[i] = FTENET_IRCConnect_EstablishConnection;	else
#endif
			establish[i] = NULL;
	}
	if (i == 1)
	{
		success |= FTENET_AddToCollection_Ptr(col, name, establish[0], address[0], &adr[0]);
		i = 0;
	}
	else
		success |= FTENET_AddToCollection_Ptr(col, name, NULL, NULL, NULL);

	for (j = 0; j < i; j++)
		success |= FTENET_AddToCollection_Ptr(col, va("%s:%i", name, j), establish[j], address[j], &adr[j]);
	for (; j < countof(adr); j++)
		success |= FTENET_AddToCollection_Ptr(col, va("%s:%i", name, j), NULL, NULL, NULL);
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

#if defined(_WIN32) && defined(HAVE_PACKET)
int FTENET_GetLocalAddress(int port, qboolean ipx, qboolean ipv4, qboolean ipv6,   unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
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
				*adrparams++ = NULL;
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
				*adrparams++ = NULL;
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
					*adrparams++ = NULL;
					addresses++;
					maxaddresses--;
					found++;
				}
			}
			freeaddrinfo(result);

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
				*adrparams++ = NULL;
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
int FTENET_GetLocalAddress(int port, qboolean ipx, qboolean ipv4, qboolean ipv6,   unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
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
			adrparams[idx] = NULL;
			idx++;
		}
	}
	return idx;
}
#else
int FTENET_GetLocalAddress(int port, qboolean ipx, qboolean ipv4, qboolean ipv6,   unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
{
	return 0;
}
#endif

int FTENET_Generic_GetLocalAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
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
			found = FTENET_GetLocalAddress(adr.port, false, canipv4, true, adrflags, addresses, adrparams, maxaddresses);
#else
			//FIXME: we should validate that we support hybrid sockets?
			found = FTENET_GetLocalAddress(adr.port, false, true, false, adrflags, addresses, adrparams, maxaddresses);
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

				found = FTENET_GetLocalAddress(adr.port, ipx, ipv4, ipv6, adrflags, addresses, adrparams, maxaddresses);
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
				*adrparams++ = NULL;
				addresses++;
				maxaddresses--;
				found++;
			}

			if (maxaddresses)
			{
				*addresses = adr;

				*adrflags++ = 0;
				*adrparams++ = NULL;
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
			if (ecode==NET_ENETUNREACH&&to->type==NA_IPV6)	//ipv6 support STILL sucks too much. don't spam non-developers, its just annoying.
				Con_DPrintf(S_COLOR_GRAY"%s - Warning: %i\n", NET_AdrToString (adr, sizeof(adr), to), ecode);
#ifdef HAVE_CLIENT
			else if (ecode == NET_EADDRNOTAVAIL || (ecode==NET_ENETUNREACH&&to->type==NA_IPV6))
				Con_DPrintf(S_COLOR_GRAY"%s - Warning: %i\n", NET_AdrToString (adr, sizeof(adr), to), ecode);
			else
#endif
			{
#ifdef _WIN32
				Con_Printf (S_COLOR_GRAY"%s - ERROR: %i\n", NET_AdrToString (adr, sizeof(adr), to), ecode);
#else
				Con_Printf (S_COLOR_GRAY"%s - ERROR: %s\n", NET_AdrToString (adr, sizeof(adr), to), strerror(ecode));
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
static qboolean FTENET_Datagram_ChangeLocalAddress(struct ftenet_generic_connection_s *con, const char *addressstring, netadr_t *adr)
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

static void FTENET_Datagram_Close(ftenet_generic_connection_t *con)
{
	if (con->thesocket != INVALID_SOCKET)
	{
#ifdef HAVE_EPOLL
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, con->thesocket, NULL);
#endif
		closesocket(con->thesocket);
	}
	Z_Free(con);
}
#endif

#ifdef HAVE_EPOLL
static void FTENET_Datagram_Polled(epollctx_t *ctx, unsigned int events)
{
	ftenet_generic_connection_t *con = NULL;
	con = (ftenet_generic_connection_t *)((qbyte*)ctx - ((qbyte*)&con->epoll-(qbyte*)con));
	while (FTENET_Datagram_GetPacket(con))
	{
		net_from.connum = con->connum;
		net_from_connection = con;
		con->owner->ReadGamePacket();
	}
}
#endif

ftenet_generic_connection_t *FTENET_Datagram_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr)
{
#ifndef HAVE_PACKET
	return NULL;
#else
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_generic_connection_t *newcon;
	qboolean isserver = col->islisten;

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
			if (SSV_IsSubServer())
				Con_DLPrintf(2, CON_ERROR "Unable to bind to port %i, bound to %s instead\n", port, addrstr);
			else
				Con_Printf(CON_ERROR "Unable to bind to port %i, bound to %s instead\n", port, addrstr);
		}
		break;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("FTENET_Datagram_EstablishConnection: ioctl FIONBIO: %s", strerror(neterrno()));

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
		newcon->Close = FTENET_Datagram_Close;
		newcon->ChangeLocalAddress = FTENET_Datagram_ChangeLocalAddress;

		newcon->owner = col;
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

#ifdef HAVE_EPOLL
		{
			struct epoll_event event = {EPOLLIN|EPOLLET, {&newcon->epoll}};
			newcon->epoll.Polled = FTENET_Datagram_Polled;
			if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, newsocket, &event) < 0)
			{
				int err = errno;
				Con_Printf("epoll_ctl failed - errno %i\n", err);
			}
		}
#endif

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
typedef struct ftenet_tcp_stream_s {
	vfsfile_t *clientstream;
	int inlen;
	int outlen;

#ifdef HAVE_EPOLL
	epollctx_t epoll;	//so our epoll dispatcher knows which connection/stream
	struct ftenet_tcp_connection_s *con;
#endif

	enum
	{
		TCPC_UNKNOWN,		//waiting to see what they send us.
		//TCPC_QTV,			//included for completeness. qtv handles the sockets itself, we just parse initial handshake and then pass it over (as either a tcp or tls vfsfile_t)
		TCPC_QIZMO,			//'qizmo\n' handshake, followed by packets prefixed with a 16bit packet length.
#ifdef HAVE_HTTPSV
		TCPC_WEBSOCKETU,	//utf-8 encoded data. //TODO(fhomolka): Check if any targets use this, remove if not
		TCPC_WEBSOCKETB,	//binary encoded data (subprotocol = 'binary')
		TCPC_WEBSOCKETNQ,	//raw nq msg buffers with no encapsulation or handshake
		TCPC_HTTPCLIENT,	//we're sending a file to this victim.
		TCPC_WEBRTC_HOST,	//for brokering webrtc connections, doesn't carry any actual game data itself.
		TCPC_WEBRTC_CLIENT,	//for brokering webrtc connections, doesn't carry any actual game data itself.
#endif
	} clienttype;
	qbyte inbuffer[MAX_OVERALLMSGLEN];
	qbyte outbuffer[MAX_OVERALLMSGLEN];
	vfsfile_t *dlfile;		//if the client looked like an http client, this is the file that they're downloading.
	float timeouttime;
	qboolean pinging;
	netadr_t remoteaddr;
	struct ftenet_tcp_stream_s *next;

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
		char resource[64];
		int clientnum;	//low number slots
		unsigned int clientseq;	//something less reuse-y
#ifdef SUPPORT_RTC_ICE
		struct icestate_s	*ice;
#endif
		//for brokering with a udp-based server.
		netadr_t target;
		int sends;
		char *offer;
		char *candidates;
		float resendtime;
		int candack;	//number of received candidates (to avoid dupes)
		int outcand;	//number of sent candidates (to avoid redundant resends)
	} webrtc;
#endif
} ftenet_tcp_stream_t;

typedef struct ftenet_tcp_connection_s {
	ftenet_generic_connection_t generic;
	qboolean tls;

	int active;
	ftenet_tcp_stream_t *tcpstreams;
} ftenet_tcp_connection_t;

void tobase64(unsigned char *out, int outlen, const unsigned char *in, int inlen)
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

neterr_t FTENET_TCP_WebSocket_Splurge(ftenet_tcp_stream_t *st, enum websocketpackettype_e packettype, const qbyte *data, unsigned int length)
{
	/*as a server, we don't need the mask stuff*/
	unsigned short ctrl = 0x8000 | (packettype<<8);
	unsigned int paylen = 0;
	unsigned int payoffs = st->outlen;
	int i;
	switch((ctrl>>8) & 0xf)
	{
	case WS_PACKETTYPE_TEXTFRAME:
		for (i = 0; i < length; i++)
		{
			paylen += (data[i] == 0 || data[i] >= 0x80)?2:1;
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
	case WS_PACKETTYPE_TEXTFRAME:/*utf8ify the data*/
		for (i = 0; i < length; i++)
		{
			if (!data[i])
			{	/*0 is encoded as 0x100 to avoid safety checks*/
				st->outbuffer[payoffs++] = 0xc0 | (0x100>>6);
				st->outbuffer[payoffs++] = 0x80 | (0x100&0x3f);
			}
			else if (data[i] >= 0x80)
			{	/*larger bytes require markup*/
				st->outbuffer[payoffs++] = 0xc0 | (data[i]>>6);
				st->outbuffer[payoffs++] = 0x80 | (data[i]&0x3f);
			}
			else
			{	/*lower 7 bits are as-is*/
				st->outbuffer[payoffs++] = data[i];
			}
		}
		break;
	default: //raw data
		memcpy(st->outbuffer+payoffs, data, length);
		payoffs += length;
		break;
	}
	st->outlen = payoffs;


	if (st->outlen && st->clientstream)
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
qboolean FTENET_TCP_HTTPResponse(ftenet_tcp_stream_t *st, httparg_t arg[WCATTR_COUNT], qboolean allowgzip)
{
	char adr[256];
	int i;
	const char *filetype = NULL;
	const char *resp = NULL;	//response headers (no length/gap)
	const char *body = NULL;	//response body
	int method;
	net_from = st->remoteaddr;
	if (!strcmp(arg[WCATTR_METHOD], "GET"))
		method = 0;
	else if (!strcmp(arg[WCATTR_METHOD], "HEAD"))
		method = 1;
	else //if (!strcmp(arg[WCATTR_METHOD], "POST") || !strcmp(arg[WCATTR_METHOD], "PUT") || !strcmp(arg[WCATTR_METHOD], "OPTIONS"))
	{
		method = 404;
		resp =	"HTTP/1.1 405 Method Not Allowed\r\n";
		st->httpstate.connection_close = true;
		body = NULL;
	}

	if (!stricmp(arg[WCATTR_CONNECTION], "Close"))
		st->httpstate.connection_close = true;	//peer wants us to kill the connection on completion.

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
#else
		else if (!strcmp(name, "favicon.ico"))
		{
//			Con_Printf("Redirect %s to %s (copyrighted)\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
			resp = va(	"HTTP/1.1 302 Found\r\n"
					"Location: https://fteqw.org/%s\r\n"
					"Content-Type: text/html\r\n"
					, "favicon.png");
			body = NULL;
		}
#endif
#if defined(SV_MASTER)
		else if (svm_sockets && st->con->generic.owner==svm_sockets && (st->dlfile=SVM_GenerateIndex(arg[WCATTR_HOST], name, &filetype, query)))
			;
#endif
#ifdef HAVE_SERVER
		/*else if (!strcmp(name, "index.html"))
		{
			resp = "HTTP/1.1 200 Ok\r\n"
				"Content-Type: text/html\r\n";

			body = va(
				"<html lang='en-us'>"
				"<head>"
				"<meta charset='utf-8'>"
				"<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>"
				"<meta name=viewport content='width=device-width, initial-scale=1'>"
				"<link rel='icon' type='image/vnd.microsoft.icon' href='/favicon.ico' />"
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
		}*/
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
				const char *ext;
				int i;
				static const char *mimes[] =
				{
					".html",	"text/html",
					".htm",		"text/html",
					".png",		"image/png",
					".jpeg",	"image/jpeg",
					".jpg",		"image/jpeg",
					".ico",		"image/vnd.microsoft.icon",
					".pk3",		"application/zip",
					".fmf",		"application/x-ftemanifest",
					".qtv",		"application/x-qtv",
					".wasm",	"application/wasm",
					".js",		"text/javascript",

					".mvd",		"application/x-multiviewdemo",
					".mvd.gz",	"application/x-multiviewdemo",
					".qwd",		"application/x-multiviewdemo",
					".qwd.gz",	"application/x-multiviewdemo",
					".dem",		"application/x-multiviewdemo",
					".dem.gz",	"application/x-multiviewdemo",
				};
				ext = COM_GetFileExtension(name, NULL);
				if (!strcmp(ext, ".gz")||!strcmp(ext, ".xz"))
					ext = COM_GetFileExtension(name, ext);
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
			else *etag = 0;

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
					//"<a href='"ENGINEWEBSITE"'>"FULLENGINENAME"</a>"
					;
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

	if (st->httpstate.connection_close)
	{
		resp = "Connection: Close\r\n";
		i = strlen(resp);
		if (st->outlen + i > sizeof(st->outbuffer))
			return false;
		memcpy(st->outbuffer+st->outlen, resp, i);
		st->outlen+= i;
	}

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

void FTENET_TCP_WebRTCServerAssigned(ftenet_tcp_stream_t *list, ftenet_tcp_stream_t *client, ftenet_tcp_stream_t *server)
{
	qbyte buffer[256];
	int trynext = 0;
	ftenet_tcp_stream_t *o;
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
		int o = client->remoteaddr.prot;
		buffer[0] = ICEMSG_NEWPEER;
		buffer[1] = (client->webrtc.clientnum>>0)&0xff;
		buffer[2] = (client->webrtc.clientnum>>8)&0xff;
//		buffer[3] = (client->webrtc.clientnum>>16)&0xff;
//		buffer[4] = (client->webrtc.clientnum>>24)&0xff;
		client->remoteaddr.prot = 0;
		NET_BaseAdrToString(buffer+3, sizeof(buffer)-3, &client->remoteaddr);	//let the server know who's trying to connect to them. for ip bans.
		client->remoteaddr.prot = o;
		FTENET_TCP_WebSocket_Splurge(server, WS_PACKETTYPE_BINARYFRAME, buffer, 3+strlen(buffer+3));

		buffer[0] = ICEMSG_NEWPEER;
		buffer[1] = 0xff;
		buffer[2] = 0xff;
//		buffer[3] = 0xff;
//		buffer[4] = 0xff;
		FTENET_TCP_WebSocket_Splurge(client, WS_PACKETTYPE_BINARYFRAME, buffer, 3);
	}
}

static const char *FTENET_TCP_ParseHTTPRequest(ftenet_tcp_connection_t *con, ftenet_tcp_stream_t *st)
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


	if (!net_enable_http.ival && !net_enable_websockets.ival && !net_enable_rtcbroker.ival)
	{
		//we need to respond, firefox will create 10 different connections if we just close it
		resp = va(	"HTTP/1.1 403 Forbidden\r\n"
					"Connection: close\r\n"	//let the client know that any pipelining it was doing will have been ignored
					"\r\n");
		VFS_WRITE(st->clientstream, resp, strlen(resp));
		return "websockets disabled";
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
			return "overflow";	//overflow...
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
			return "bad char";
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
				return "bad header";
			}
		}
	}

	if (!headerscomplete)
	{
		Con_DPrintf("http header parsing failed\n");
		return "bad header";	//the caller said it was complete! something's fucked if we're here
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
			Con_DPrintf("http oversize request\n");
			return "bad header";	//can never be completed.
		}
		return NULL;
	}

	//clients uploading chunked stuff is bad/unsupported.
	if (sendingweirdness)
	{
		resp = va(	"HTTP/1.1 413 Payload Too Large \r\n"
					"Connection: close\r\n"	//let the client know that any pipelining it was doing will have been ignored
					"\r\n");
		VFS_WRITE(st->clientstream, resp, strlen(resp));
		Con_DPrintf("unsupported http encoded request\n");
		return "unsupported";	//can't handle the request, so discard the connection
	}

	memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
	st->inlen -= i;

	//for websocket connections:
	//must be a Host, Upgrade=websocket, Connection=Upgrade, Sec-WebSocket-Key=base64(randbytes(16)), Sec-WebSocket-Version=13
	//optionally will be Origin=url, Sec-WebSocket-Protocol=FTEWebSocket, Sec-WebSocket-Extensions
	//other fields will be ignored.
	if (!stricmp(arg[WCATTR_UPGRADE], "websocket") && (!stricmp(arg[WCATTR_CONNECTION], "Upgrade") || !stricmp(arg[WCATTR_CONNECTION], "keep-alive, Upgrade")))
	{
		int cltype;
		//the choice of protocol affects the type of response that we give rather than anything mystic
		if (!strcmp(arg[WCATTR_WSPROTO], "quake"))
			cltype = TCPC_WEBSOCKETNQ;	//raw nq data, all reliable, for compat with webquake
		else if (!strcmp(arg[WCATTR_WSPROTO], "rtc_client"))
			cltype = TCPC_WEBRTC_CLIENT;//not a real client.
		else if (!strcmp(arg[WCATTR_WSPROTO], "rtc_host"))
			cltype = TCPC_WEBRTC_HOST;//not a real client, but a competing server! oh noes!
		else if (!strcmp(arg[WCATTR_WSPROTO], "binary"))
			cltype = TCPC_WEBSOCKETB;	//emscripten's networking libraries insists on 'binary', but we stopped using that a while back because its hostname->ip stuff was flawed.
		else if (!strcmp(arg[WCATTR_WSPROTO], "fteqw"))
			cltype = TCPC_WEBSOCKETB;	//specific custom protocol name to avoid ambiguities.
		else
			cltype = TCPC_WEBSOCKETU;	//nacl supports only utf-8 encoded data, at least at the time I implemented it.

		if (cltype == TCPC_WEBRTC_CLIENT||cltype==TCPC_WEBRTC_HOST)
		{
			if (!net_enable_rtcbroker.ival)
				return "broker disabled";
		}
		else	//TCPC_WEBSOCKETNQ, TCPC_WEBSOCKETB, TCPC_WEBSOCKETU
		{
			if (!net_enable_websockets.ival)
				return "websocket clients disabled";
		}

		if (websocketver != 13)
		{
			Con_DPrintf("Outdated websocket request for \"%s\" from \"%s\". got version %i, expected version 13\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), websocketver);

			resp = va(	"HTTP/1.1 426 Upgrade Required\r\n"
						"Sec-WebSocket-Version: 13\r\n"
						"Connection: close\r\n"	//let the client know that any pipelining it was doing will have been ignored
						"\r\n");
			VFS_WRITE(st->clientstream, resp, strlen(resp));
			return "bad header";
		}
		else
		{
			const char *failreason = NULL;
			char acceptkey[20*2];
			unsigned char sha1digest[20];
			char *blurgh;
			char *protoname = "";
			static int clientseq;

			blurgh = va("%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", arg[WCATTR_WSKEY]);
			tobase64(acceptkey, sizeof(acceptkey), sha1digest, CalcHash(&hash_sha1, sha1digest, sizeof(sha1digest), blurgh, strlen(blurgh)));

			if (st->remoteaddr.prot == NP_TLS)
				st->remoteaddr.prot = NP_WSS;
			else
				st->remoteaddr.prot = NP_WS;

			protoname = (cltype==TCPC_WEBSOCKETU)?"":va("Sec-WebSocket-Protocol: %s\r\n", arg[WCATTR_WSPROTO]);
			st->clienttype = cltype;

			switch(st->clienttype)
			{
			case TCPC_WEBSOCKETNQ:
			case TCPC_WEBSOCKETU:
			case TCPC_WEBSOCKETB:
				if (!net_enable_websockets.ival)
					failreason = "blocked by net_enable_websockets";
				break;
			case TCPC_WEBRTC_HOST:
			case TCPC_WEBRTC_CLIENT:
				if (!net_enable_rtcbroker.ival)
					failreason = "blocked by net_enable_rtcbroker";
				break;
			default:
				failreason = "unsupported protocol type";
				break;
			}

			if (*arg[WCATTR_URL] == '/')
				Q_strncpyz(st->webrtc.resource, arg[WCATTR_URL]+1, sizeof(st->webrtc.resource));
			else
				Q_strncpyz(st->webrtc.resource, arg[WCATTR_URL], sizeof(st->webrtc.resource));
			st->webrtc.clientnum = -1;
			st->webrtc.clientseq = clientseq++;
#ifndef SUPPORT_RTC_ICE
			if (st->clienttype == TCPC_WEBRTC_CLIENT && !*st->webrtc.resource)
				failreason = "client did not specify resource type";
#endif

			if (failreason)
			{
				Con_DPrintf("Websocket(%s) request for %s from %s - %s\n", arg[WCATTR_WSPROTO], arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), failreason);
				return failreason;
			}
			Con_DPrintf("Websocket request for %s from %s (%s)\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), arg[WCATTR_WSPROTO]);

			resp = va(	"HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
						"Upgrade: websocket\r\n"
						"Connection: Upgrade\r\n"
						"Access-Control-Allow-Origin: *\r\n"	//allow cross-origin requests. this means you can use any domain to play on any public server.
						"Sec-WebSocket-Accept: %s\r\n"
						"%s"
						"\r\n", acceptkey, protoname);
			//send the websocket handshake response.
			VFS_WRITE(st->clientstream, resp, strlen(resp));

			if (st->clienttype == TCPC_WEBRTC_HOST || st->clienttype == TCPC_WEBRTC_CLIENT)
			{
				ftenet_tcp_stream_t *o;

				//split the requested resource by protocol/room
				char *idstart = strchr(st->webrtc.resource, '/');
				if (!idstart++)
				{	//MUST have a protocol name
					return "no game protocol specified";
				}

				if (st->clienttype == TCPC_WEBRTC_HOST)
				{
					if (!*idstart)
					{	//webrtc servers need some unique resource address. lets use their ip+port for now. we should probably be randomising this
						static unsigned int g;
						int ofs = strlen(st->webrtc.resource);
						Q_snprintfz(st->webrtc.resource+ofs, sizeof(st->webrtc.resource)-ofs, "%u", ++g);
					}

					for (o = con->tcpstreams; o; o = o->next)
					{
						if (o != st && o->clienttype == TCPC_WEBRTC_HOST && !strcmp(st->webrtc.resource, o->webrtc.resource))
						{
							net_message_buffer[0] = ICEMSG_NAMEINUSE;
							strcpy(net_message_buffer+1, st->webrtc.resource);
							FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_BINARYFRAME, net_message_buffer, strlen(net_message_buffer));

							*st->webrtc.resource = 0; //don't trigger shutdown broadcasts to valid clients.
							return "room conflict";	//conflict! can't have two servers listening on the same url
						}
					}

					net_message_buffer[0] = ICEMSG_GREETING;
					net_message_buffer[1] = 0xff;
					net_message_buffer[2] = 0xff;
					strcpy(net_message_buffer+3, st->webrtc.resource);
					FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_BINARYFRAME, net_message_buffer, strlen(net_message_buffer));

					//if we have (inactive) clients connected, assign them (and let them know that they need to start handshaking)
					for (o = con->tcpstreams; o; o = o->next)
					{
						if (o->clienttype == TCPC_WEBRTC_CLIENT && !strcmp(st->webrtc.resource, o->webrtc.resource))
							FTENET_TCP_WebRTCServerAssigned(con->tcpstreams, o, st);
					}

#ifdef SV_MASTER
					SVM_AddBrokerGame(st->webrtc.resource, "");
#endif
				}
				else
				{	//a client looking for a server...
					if (!*idstart)
					{	//that's trying to connect to us...
#ifdef HAVE_SERVER
						if (sv.state != ss_dead)
						{
							net_message_buffer[0] = ICEMSG_NEWPEER;
							net_message_buffer[1] = 0xff;
							net_message_buffer[2] = 0xff;
							FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_BINARYFRAME, net_message_buffer, 3);
						}
						else
#endif
							return "no local server"; //not running a server. can't honour it.
					}
					else if (!strncmp(idstart, "udp/", 4))
					{	//we don't use StringToAdr to avoid dns lookup stalls (denial of service attacks). should at least work for server browsers.
#if defined(HAVE_DTLS) && defined(SV_MASTER)
						struct dtlspeercred_s cred={NULL};
						if (net_enable_dtls.ival)
						{
							if (!NET_StringToAdr_NoDNS(idstart+4, PORT_QWCLIENT, &st->webrtc.target))
								return "bad target address (names are disallowed)";
//							if (NET_ClassifyAddress(&st->webrtc.target, NULL) <= ASCOPE_LAN)
//								return false;	//block addresses on the broker's lan. we use non-standard protocols so this should not cause problems while being useful for custom deployments.

#ifdef SV_MASTER
							if (con->generic.owner == svm_sockets)
								if (!SVM_FixupServerAddress(&st->webrtc.target, &cred))
									return "target not registered";	//we don't know about this server...
							//if the specified credentials, reject the connection if different.
#endif

							//use dtls to contact the server.
							if (st->webrtc.target.prot == NP_DGRAM)
								st->webrtc.target.prot = NP_DTLS;
							if (st->webrtc.target.prot == NP_DTLS)	//don't make expensive tcp connections!
								NET_EnsureRoute(con->generic.owner, NULL, &cred, &st->webrtc.target, true);

							//we'll sythesise some rdp when we get an offer.
							net_message_buffer[0] = ICEMSG_NEWPEER;
							net_message_buffer[1] = 0xff;
							net_message_buffer[2] = 0xff;
							FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_BINARYFRAME, net_message_buffer, 3);
						}
						else
#endif
							return "rdp-via-udp is not enabled on this broker";
					}
					else
					{	//find its server, if we can
						for (o = con->tcpstreams; o; o = o->next)
						{
							if (o->clienttype == TCPC_WEBRTC_HOST && !strcmp(st->webrtc.resource, o->webrtc.resource))
								break;
						}
						//and assign it to this client
						FTENET_TCP_WebRTCServerAssigned(con->tcpstreams, st, o);
					}
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
				net_from.connum = con->generic.connum;
				net_from_connection = &con->generic;
				MSG_WriteLong(&net_message, LongSwap(NETFLAG_CTL | (strlen(NQ_NETCHAN_GAMENAME)+7)));
				MSG_WriteByte(&net_message, CCREQ_CONNECT);
				MSG_WriteString(&net_message, NQ_NETCHAN_GAMENAME);
				MSG_WriteByte(&net_message, NQ_NETCHAN_VERSION);
				con->generic.owner->ReadGamePacket();
			}
			return NULL;
		}
	}
	else
	{
		if (!net_enable_http.ival)
			return "http disabled";
		if (FTENET_TCP_HTTPResponse(st, arg, acceptsgzip))
			return NULL;
		else
			return "http error";
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
void FTENET_TCP_PrintStatus(ftenet_generic_connection_t *gcon)
{
	ftenet_tcp_connection_t *con = (ftenet_tcp_connection_t*)gcon;
	ftenet_tcp_stream_t *st;
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

static qboolean FTENET_TCP_KillStream(ftenet_tcp_connection_t *con, ftenet_tcp_stream_t *st, const char *reason)
{	//some sort of error. kill the connection info (will be cleaned up later)

#ifdef HAVE_HTTPSV
	if (st->clienttype == TCPC_WEBRTC_CLIENT && st->clientstream && !strcmp(st->webrtc.resource, st->webrtc.resource))
	{
		qbyte msg[256];
		msg[0] = ICEMSG_PEERLOST;
		msg[1] = 0xff;
		msg[2] = 0xff;
		Q_strncpyz(msg+3, reason, sizeof(msg)-3);

		FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_BINARYFRAME, msg, 3+strlen(msg+3));
	}
#endif

#ifdef HAVE_EPOLL
	if (st->socketnum != INVALID_SOCKET)
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, st->socketnum, NULL);
#endif
	if (st->clientstream)
		VFS_CLOSE(st->clientstream);
	st->clientstream = NULL;
	st->socketnum = INVALID_SOCKET;

	if (st->dlfile)
		VFS_CLOSE(st->dlfile);

#ifdef HAVE_HTTPSV
	if (st->clienttype == TCPC_WEBRTC_CLIENT)
	{	//notify its server
		ftenet_tcp_stream_t *o;
		for (o = con->tcpstreams; o; o = o->next)
		{
			if (o->clienttype == TCPC_WEBRTC_HOST && !strcmp(o->webrtc.resource, st->webrtc.resource))
			{
				qbyte msg[256];
				msg[0] = ICEMSG_PEERLOST;
				msg[1] = (st->webrtc.clientnum>>0)&0xff;
				msg[2] = (st->webrtc.clientnum>>8)&0xff;
				Q_strncpyz(msg+3, reason, sizeof(msg)-3);

				FTENET_TCP_WebSocket_Splurge(o, WS_PACKETTYPE_BINARYFRAME, msg, 3+strlen(msg+3));
				break;	//should only be one.
			}
		}
	}
	else if (st->clienttype == TCPC_WEBRTC_HOST)
	{	//we're brokering a client+server. all messages should be unicasts between a client and its host, matched by resource.
		ftenet_tcp_stream_t *o;
		for (o = con->tcpstreams; o; o = o->next)
		{
			if (o->clienttype == TCPC_WEBRTC_CLIENT && !strcmp(o->webrtc.resource, st->webrtc.resource))
			{
				qbyte msg[256];
				msg[0] = ICEMSG_PEERLOST;
				msg[1] = (st->webrtc.clientnum>>0)&0xff;
				msg[2] = (st->webrtc.clientnum>>8)&0xff;
				Q_strncpyz(msg+3, reason, sizeof(msg)-3);

				FTENET_TCP_WebSocket_Splurge(o, WS_PACKETTYPE_BINARYFRAME, msg, 3+strlen(msg+3));
			}
		}
#ifdef SV_MASTER
		SVM_RemoveBrokerGame(st->webrtc.resource);
#endif
	}
#endif

	return false;
}
static void FTENET_TCP_Flush(ftenet_tcp_connection_t *con, ftenet_tcp_stream_t *st)
{
	//write after the reads, for slightly faster pings
	if (st->outlen && st->clientstream)
	{	/*try and flush any old outgoing data*/
		int done;
		done = VFS_WRITE(st->clientstream, st->outbuffer, st->outlen);
		if (done > 0)
		{
			memmove(st->outbuffer, st->outbuffer + done, st->outlen - done);
			st->outlen -= done;
			st->timeouttime = Sys_DoubleTime() + 30;
		}
		/*else if (done == 0)
		{
			Con_DPrintf ("tcp peer %s closed connection\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
			st->outlen = 0;
		}*/
	}
}
//returns a string for why it was killed. or NULL for nothing more to do.
static const char *FTENET_TCP_ReadStream(ftenet_tcp_connection_t *con, ftenet_tcp_stream_t *st)
{
	char		adr[MAX_ADR_SIZE];
restart:	//gotos are evil. I am evil. live with it.
	if (!st->clientstream)
		return NULL;
	if (st->inlen < sizeof(st->inbuffer)-1)
	{
		int ret = VFS_READ(st->clientstream, st->inbuffer+st->inlen, sizeof(st->inbuffer)-1-st->inlen);
		if (ret < 0)
		{
			st->outlen = 0;	//don't flush, no point.
			Con_DPrintf ("tcp peer %s closed connection\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
			return "connection lost";
		}
		st->inlen += ret;
	}

	switch(st->clienttype)
	{
	case TCPC_UNKNOWN:
		if (st->inlen < 6)
			return NULL;

		//so TLS apparently uses a first byte that is always < 64. which is handy to know.
		if (con->generic.islisten && st->remoteaddr.prot == NP_STREAM && st->clientstream && !((st->inbuffer[0] >= 'a' && st->inbuffer[0] <= 'z') || (st->inbuffer[0] >= 'A' && st->inbuffer[0] <= 'Z')))
		{
#if defined(HAVE_SSL) && (defined(HAVE_SERVER) || defined(HAVE_HTTPSV))	//if its non-ascii, then try and upgrade the connection to tls
			if (net_enable_tls.ival)
			{
				//copy off our buffer so we can read it into the tls stream's buffer instead.
				char tmpbuf[256];
				vfsfile_t *stream = st->clientstream;
				int (QDECL *realread) (struct vfsfile_s *file, void *buffer, int bytestoread);
				if (st->inlen > sizeof(net_message_buffer))
					return "oversize";	//would cause data loss...
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
					if (st->inlen < 0)
					{	//okay, something failed...
						st->inlen = 0;
						return "error";
					}
					else
					{
						//make sure we actually read from the proper stream again
						stream->ReadBytes = realread;
					}
				}
				if (!st->clientstream || net_message.cursize)
					return "tls error";	//failure, or it didn't read all the data that we buffered for it (error instead of forgetting it).
				if (developer.ival)
					Con_Printf("promoted peer to tls: %s\n", NET_AdrToString(tmpbuf, sizeof(tmpbuf), &st->remoteaddr));
				goto restart;	//might be a usable packet in there that we now need to make sense of.
			}
#endif
			return "no tls";
		}

		//check if its a qizmo connection (or rather a general qw-over-tcp connection)
		if (st->inlen >= 6 && !strncmp(st->inbuffer, "qizmo\n", 6))
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
					if (VFS_WRITE(st->clientstream, "qizmo\n", 6) != 6)
						return "write error";	//unable to write for some reason.
				}
				return NULL;
			}
			return "net_enable_qizmo";	//not enabled.
		}

		//check if we have some http-like protocol with a header that ends with two trailing new lines (carrage returns optional, at least here)
		//(must have a full request header, meaning double-lineendings somewhere)
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
#if defined(SUBSERVERS) && defined(HAVE_SERVER)
				//this is a new subserver node...
				if (!Q_strncasecmp(st->inbuffer, "NODE", 4))
				{
					char tmpbuf[256];
#ifdef HAVE_EPOLL
					//the tcp connection will be handled elsewhere.
					//make sure we don't get tcp-handler wakeups from this connection.
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, st->socketnum, NULL);
					st->socketnum = INVALID_SOCKET;
					st->epoll.Polled = NULL;
#endif
					//now try to pass it over
					MSV_NewNetworkedNode(st->clientstream, st->inbuffer, st->inbuffer+i, st->inlen-i, NET_AdrToString(tmpbuf, sizeof(tmpbuf), &st->remoteaddr));
					st->clientstream = NULL;	//qtv code took it.
					return "node linked";
				}
				else
#endif
#ifdef MVD_RECORDING
				//for QTV connections, we just need the method and a blank line. our qtv parser will parse the actual headers.
				if (!Q_strncasecmp(st->inbuffer, "QTV", 3))
				{	//FIXME: make sure its removed from epoll and not killed prematurely
					int r = -2;
					const char *desc;
					if (net_enable_qtv.ival == 2 && NET_ClassifyAddress(&st->remoteaddr, &desc) > ASCOPE_HOST)
						;
					else if (net_enable_qtv.ival)
						r = SV_MVD_GotQTVRequest(st->clientstream, st->inbuffer, st->inbuffer+st->inlen, &st->qtvstate);
					i = st->inlen;
					memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
					st->inlen -= i;
					switch(r)
					{
					case -2:
						VFS_PUTS(st->clientstream, "QTVSV 1\n" "PERROR: net_enable_qtv is disabled on this server\n\n");
						return "net_enable_qtv disabled";
					case -1:	//error
						return "error";
					case 0:		//retry
						return NULL;
					case 1:		//accepted
#ifdef HAVE_EPOLL
						//the tcp connection will now be handled by the dedicated qtv code rather than us.
						//make sure we don't get tcp-handler wakeups from this connection.
						epoll_ctl(epoll_fd, EPOLL_CTL_DEL, st->socketnum, NULL);
						st->socketnum = INVALID_SOCKET;
						st->epoll.Polled = NULL;
#endif
						st->clientstream = NULL;	//qtv code took it.
						return "qtv client";
					}
				}
				else
#endif
				{
#ifdef HAVE_HTTPSV
					const char *err = FTENET_TCP_ParseHTTPRequest(con, st);
					if (!err)
						goto restart;
					return err;
#else
					Con_DPrintf ("Unknown TCP handshake from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					return "unsupported handshake";
#endif
				}
			}
			else
			{
				//they splurged too much data and we don't even know what they were
				//either way we're expecting a request header in our buffer that can never be completed
				if (st->inlen >= sizeof(st->inbuffer)-1)
					return "unknown";
			}
		}

		return NULL;
#ifdef HAVE_HTTPSV
	case TCPC_HTTPCLIENT:
		/*try and keep it flushed*/
		FTENET_TCP_Flush(con, st);
		if (!st->outlen)
		{
			if (st->dlfile)
				st->outlen = VFS_READ(st->dlfile, st->outbuffer, sizeof(st->outbuffer));
			else
				st->outlen = 0;
			if (st->outlen <= 0)
			{
				st->outlen = 0;
				if (st->dlfile)
					VFS_CLOSE(st->dlfile);
				st->dlfile = NULL;
				st->clienttype = TCPC_UNKNOWN;	//wait for the next request (could potentially be a websocket connection)
				Con_DPrintf ("Outgoing file transfer complete\n");
				if (st->httpstate.connection_close)
					return "complete";
			}
			FTENET_TCP_Flush(con, st);
		}
		return NULL;
#endif
	case TCPC_QIZMO:
		if (st->inlen < 2)
			return NULL;

		net_message.cursize = BigShort(*(short*)st->inbuffer);
		if (net_message.cursize >= sizeof(net_message_buffer) )
		{
			Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
			return "oversize";
		}
		if (net_message.cursize+2 > st->inlen)
		{	//not enough buffered to read a packet out of it.
			return NULL;
		}

		memcpy(net_message_buffer, st->inbuffer+2, net_message.cursize);
		memmove(st->inbuffer, st->inbuffer+net_message.cursize+2, st->inlen - (net_message.cursize+2));
		st->inlen -= net_message.cursize+2;

		net_message.packing = SZ_RAWBYTES;
		net_message.currentbit = 0;
		net_from = st->remoteaddr;
		net_from.connum = con->generic.connum;
		net_from_connection = &con->generic;

		con->generic.owner->ReadGamePacket();
		goto restart;
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
//			st->inbuffer[st->inlen]=0;
			if (ctrl & 0x7000)
			{
				Con_Printf ("%s: reserved bits set\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				return "reserved";
			}
			if ((ctrl & 0x7f) == 127)
			{
				quint64_t ullpaylen;
				//as a payload is not allowed to be encoded as too large a type, and quakeworld never used packets larger than 1450 bytes anyway, this code isn't needed (65k is the max even without this)
				if (sizeof(ullpaylen) < 8)
				{
					Con_Printf ("%s: payload frame too large\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					return "oversize";
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
						return "corrupt";
					}
					if (ullpaylen > 0x40000)
					{
						Con_Printf ("%s: payload size (%"PRIu64") is abusive\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), ullpaylen);
						return "oversize";
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
					return "corrupt";
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
					return "oversize";
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
			case WS_PACKETTYPE_CONTINUATION:	/*continuation*/
				Con_Printf ("websocket continuation frame from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				return "unsupported";	//can't handle these.
			case WS_PACKETTYPE_TEXTFRAME:	/*text frame*/
//				Con_Printf ("websocket text frame from %s\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
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
			case WS_PACKETTYPE_BINARYFRAME: /*binary frame*/
//				Con_Printf ("websocket binary frame from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				net_message.cursize = paylen;
				if (net_message.cursize+8 >= sizeof(net_message_buffer) )
				{
					Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					return "oversize";
				}
#ifdef SUPPORT_RTC_ICE
				if (st->clienttype == TCPC_WEBRTC_CLIENT && !*st->webrtc.resource)
				{	//this is a client that's connected directly to us via webrtc.
					//FIXME: we don't support dtls, so browers will bitch about our sdp.
					if (paylen+1 < sizeof(net_message_buffer))
					{
						net_message_buffer[paylen] = 0;
						memcpy(net_message_buffer, st->inbuffer+payoffs, paylen);

						if (!st->webrtc.ice)	//if the ice state isn't established yet, do that now.
							st->webrtc.ice = iceapi.ICE_Create(NULL, "test", "rtc://foo", ICEM_ICE, ICEP_QWSERVER);
						iceapi.ICE_Set(st->webrtc.ice, "sdp", net_message_buffer);

						if (iceapi.ICE_Get(st->webrtc.ice, "sdp", net_message_buffer, sizeof(net_message_buffer)))
							FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_BINARYFRAME, net_message_buffer, strlen(net_message_buffer));
					}
					net_message.cursize = 0;
				}
				else
#endif
					 if (st->clienttype == TCPC_WEBRTC_HOST && st->inbuffer[payoffs+0] == ICEMSG_SERVERINFO)
				{
#ifdef SV_MASTER
					qbyte old = st->inbuffer[payoffs+paylen];
					st->inbuffer[payoffs+paylen] = 0;	//make sure its null terminated...
					SVM_AddBrokerGame(st->webrtc.resource, st->inbuffer+payoffs+3);
					st->inbuffer[payoffs+paylen] = old;
#endif
				}
				else if ((st->clienttype == TCPC_WEBRTC_CLIENT || st->clienttype == TCPC_WEBRTC_HOST) && paylen >= 3)
				{	//we're brokering a client+server. all messages should be unicasts between a client and its host, matched by resource.
					if (st->webrtc.target.type != NA_INVALID && st->clienttype==TCPC_WEBRTC_CLIENT)
					{	//if the server is a udp one, we need to buffer some stuff to handle resends over a dtls connection that still has to be established.
						if (st->inbuffer[payoffs] == ICEMSG_OFFER)
						{
							BZ_Free(st->webrtc.offer);
							st->webrtc.offer = BZF_Malloc(paylen-3+1);
							memcpy(st->webrtc.offer, st->inbuffer+payoffs+3, paylen-3);
							st->webrtc.offer[paylen-3] = 0;
							st->webrtc.resendtime = FLT_MIN;
						}
						else if (st->inbuffer[payoffs] == ICEMSG_CANDIDATE && paylen > 3)
						{
							Z_StrCatLen(&st->webrtc.candidates, st->inbuffer+payoffs+3, paylen-3);
							if ((st->inbuffer+payoffs+3)[paylen-3-1]!= '\n')
								Z_StrCat(&st->webrtc.candidates, "\n");
							st->webrtc.resendtime = FLT_MIN;
						}
					}
					else
					{	//forward it to the other side. much easier with tcp.
						ftenet_tcp_stream_t *o;
						short clnum = (st->inbuffer[payoffs+1]<<0)|(st->inbuffer[payoffs+2]<<8);
						int type = (st->clienttype != TCPC_WEBRTC_CLIENT)?TCPC_WEBRTC_CLIENT:TCPC_WEBRTC_HOST;
						for (o = con->tcpstreams; o; o = o->next)
						{
							if (o->clienttype == type && clnum == o->webrtc.clientnum && !strcmp(o->webrtc.resource, st->webrtc.resource))
							{
								st->inbuffer[payoffs+1] = (st->webrtc.clientnum>>0)&0xff;
								st->inbuffer[payoffs+2] = (st->webrtc.clientnum>>8)&0xff;
								FTENET_TCP_WebSocket_Splurge(o, WS_PACKETTYPE_BINARYFRAME, st->inbuffer+payoffs, paylen);
								break;
							}
						}
						if (!o)
							Con_DPrintf("Unable to relay\n");
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
			case WS_PACKETTYPE_CLOSE:	/*connection close*/
				Con_Printf ("websocket closure %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				return "drop";	//they're about to drop anyway.
			case WS_PACKETTYPE_PING:	/*ping*/
//				Con_Printf ("websocket ping from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				if (FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_PONG, st->inbuffer+payoffs, paylen) != NETERR_SENT)
					return "write error";
				break;
			case WS_PACKETTYPE_PONG: /*pong*/
				st->timeouttime = Sys_DoubleTime() + 30;
				st->pinging = false;
//				Con_Printf ("websocket pong from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				break;
			default:
				Con_Printf ("Unsupported websocket opcode (%i) from %s\n", (ctrl>>8) & 0xf, NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				return "unsupported";
			}

			memmove(st->inbuffer, st->inbuffer+payoffs + paylen, st->inlen - (payoffs + paylen));
			st->inlen -= payoffs + paylen;

			if (net_message.cursize)
			{
				net_message.packing = SZ_RAWBYTES;
				net_message.currentbit = 0;
				net_from = st->remoteaddr;
				net_from.connum = con->generic.connum;
				net_from_connection = &con->generic;
				con->generic.owner->ReadGamePacket();
				goto restart;
			}
		}
		return NULL;
#endif
	}
	return NULL;
}

#ifdef SV_MASTER
static qboolean FTENET_TCP_GetPacket(ftenet_generic_connection_t *gcon);
void FTENET_TCP_ICEResponse(ftenet_connections_t *col, int type, const char *cid, const char *sdp)
{
	unsigned int id, connum;
	ftenet_tcp_connection_t *con;
	ftenet_tcp_stream_t *o;

	cid = COM_Parse(cid);
	id = strtoul(com_token, NULL, 16);
	connum = ((id>>16)&0xffff)-1;
	id=(short)(id&0xffff);
	if (connum >= countof(col->conn) || !col->conn[connum] || col->conn[connum]->GetPacket != FTENET_TCP_GetPacket)
		return;
	con = (ftenet_tcp_connection_t*)col->conn[connum];

	for (o = con->tcpstreams; o; o = o->next)
	{
		if (o->clienttype != TCPC_WEBRTC_CLIENT || o->webrtc.target.type==NA_INVALID || o->webrtc.clientnum != id)
			continue;
		if (NET_CompareAdr(&net_from, &o->webrtc.target))
		{
			char msg[1400];
			Z_Free(o->webrtc.offer);	//we got the answer. can stop trying to spam it now.
			o->webrtc.offer = NULL;

			if (type == ICEMSG_CANDIDATE)
			{
				const char *nc = o->webrtc.candidates;
				char seq, ack;
				cid = COM_Parse(cid);
				seq = atoi(com_token);
				cid = COM_Parse(cid);
				ack = atoi(com_token);

				while (nc && o->webrtc.outcand < ack)
				{	//server saw one of our candidate lines. stop respamming that one.
					o->webrtc.outcand++;

					for (; *nc && *nc != '\n'; nc++)
						;
					if (*nc != '\n')
						break;	//getting exploited...
					nc++;	//skip the nl
				}
				if (!nc || !*nc)
				{
					Z_Free(o->webrtc.candidates);	//all acked...
					o->webrtc.candidates = NULL;
				}
				else if (nc != o->webrtc.candidates)
					memmove(o->webrtc.candidates, nc, strlen(nc)+1);
				o->webrtc.outcand = ack; //in case the server just wanted to drop some.

				while(*sdp)
				{
					for (nc = sdp; *nc && *nc != '\n'; nc++)
						;
					if (*nc != '\n')
						break;	//getting exploited...
					nc++;	//skip the nl.
					if (seq++ < o->webrtc.candack)
						;	//already saw this line
					else
					{	//new. yay reliables...
						msg[0] = type;
						msg[1] = 0xff;
						msg[2] = 0xff;
						memcpy(msg+3, sdp, nc-sdp);
						FTENET_TCP_WebSocket_Splurge(o, WS_PACKETTYPE_BINARYFRAME, msg, 3+nc-sdp);
						o->webrtc.candack=seq;
					}

					sdp = nc;
				}
			}
			else
			{
				msg[0] = type;
				msg[1] = 0xff;
				msg[2] = 0xff;
				Q_strncpyz(msg+3, sdp, sizeof(msg));
				FTENET_TCP_WebSocket_Splurge(o, WS_PACKETTYPE_BINARYFRAME, msg, 3+strlen(msg+3));
			}
			break;
		}
	}
}
#endif

#ifdef HAVE_EPOLL
static void FTENET_TCP_Polled(epollctx_t *ctx, unsigned int events)
{
	ftenet_tcp_stream_t *st = NULL;
	const char *err;
	st = (ftenet_tcp_stream_t *)((qbyte*)ctx - ((qbyte*)&st->epoll-(qbyte*)st));

	err = FTENET_TCP_ReadStream(st->con, st);
	if (err)
		FTENET_TCP_KillStream(st->con, st, err);
	else
		FTENET_TCP_Flush(st->con, st);
}
#endif

static qboolean FTENET_TCP_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_tcp_connection_t *con = (ftenet_tcp_connection_t*)gcon;
//	int ret;
	char		adr[MAX_ADR_SIZE];
	struct sockaddr_qstorage	from;
	int fromlen;

	float timeval = Sys_DoubleTime();
	ftenet_tcp_stream_t *st;
	st = con->tcpstreams;

	while (con->tcpstreams && con->tcpstreams->clientstream == NULL)
	{	//remove initial stale ones
		st = con->tcpstreams;
		con->tcpstreams = con->tcpstreams->next;
#ifdef HAVE_EPOLL
		st->epoll.Polled = NULL;	//to cause segfaults if we failed somehow.
#endif
		BZ_Free(st);
		con->active--;
	}

	for (st = con->tcpstreams; st; st = st->next)
	{//client receiving only via tcp

		while (st->next && st->next->clientstream == NULL)
		{	//remove following stale ones
			ftenet_tcp_stream_t *temp;
			temp = st->next;
			st->next = st->next->next;
#ifdef HAVE_EPOLL
			temp->epoll.Polled = NULL;	//to cause segfaults if we failed somehow.
#endif
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
				st->pinging = true;	//cleared on ack.

				FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_PING, "ping", 4);
			}
			else
#endif
			{
				Con_DPrintf ("tcp peer %s timed out\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				FTENET_TCP_KillStream(con, st, "timeout");
				continue;
			}
		}

		for(;st->clientstream;)
		{
			const char *err = FTENET_TCP_ReadStream(con, st);
			if (err)
				FTENET_TCP_KillStream(con, st, err);
			break;
		}
		FTENET_TCP_Flush(con, st);

#ifdef HAVE_HTTPSV
		if (st->clienttype==TCPC_WEBRTC_CLIENT && st->webrtc.target.type!=NA_INVALID && st->webrtc.resendtime < timeval && st->clientstream)
		{
			if (st->webrtc.offer)
			{
				static struct netprim_s prim={0};
				sizebuf_t msg;
				char adr[256];
				netproto_t o = st->remoteaddr.prot;

				if (st->webrtc.sends > 5)
				{
					if (st->webrtc.sends > 6)
					{
						FTENET_TCP_KillStream(con, st, "Too many resends");
						continue;
					}
					st->webrtc.resendtime = timeval + 3;
					st->webrtc.sends++;
					continue;
				}

				st->remoteaddr.prot = 0;	//no prefixes!
				NET_BaseAdrToString(adr, sizeof(adr), &st->remoteaddr);	//let the server know who's trying to connect to them. for ip bans.
				st->remoteaddr.prot = o;

				MSG_BeginWriting(&msg, prim, net_message_buffer, sizeof(net_message_buffer));
				MSG_WriteLong(&msg, ~0);
				MSG_WriteString(&msg, va("ice_offer %s %x:%x", adr, (con->generic.connum<<16)|(quint16_t)st->webrtc.clientnum, st->webrtc.clientseq));
				MSG_WriteString(&msg, st->webrtc.offer);
				safeswitch (NET_SendPacket(con->generic.owner, msg.cursize, msg.data, &st->webrtc.target))
				{
				case NETERR_CLOGGED:	//dtls still connecting, or just unable to send...
					break;	//don't update resend timer..
				case NETERR_SENT:
					st->webrtc.sends++;
					st->webrtc.resendtime = timeval + 1;
					break;
				case NETERR_NOROUTE:
				case NETERR_DISCONNECTED:
				case NETERR_MTU:
				safedefault:
					FTENET_TCP_KillStream(con, st, "target not reachable");
					break;
				}
			}
			else if (st->webrtc.candidates)
			{	//hopefully the server will have a proper public address and so won't need this... but just in case...
				static struct netprim_s prim={0};
				sizebuf_t msg;

				MSG_BeginWriting(&msg, prim, net_message_buffer, sizeof(net_message_buffer));
				MSG_WriteLong(&msg, ~0);
				MSG_WriteString(&msg, va("ice_ccand %x:%x %i %i", (con->generic.connum<<16)|(quint16_t)st->webrtc.clientnum, st->webrtc.clientseq, st->webrtc.outcand, st->webrtc.candack));
				MSG_WriteString(&msg, st->webrtc.candidates);
				safeswitch (NET_SendPacket(con->generic.owner, msg.cursize, msg.data, &st->webrtc.target))
				{
				case NETERR_CLOGGED:	//unable to send...
					break;	//don't update resend timer so we don't lose too much time
				case NETERR_SENT:
					st->webrtc.resendtime = timeval + 1;
					st->webrtc.sends++;
					break;
				case NETERR_NOROUTE:
				case NETERR_DISCONNECTED:
				case NETERR_MTU:
				safedefault:
					FTENET_TCP_KillStream(con, st, "target not reachable");
					break;
				}
			}
			else
				st->webrtc.resendtime = timeval + 30;
		}
#endif
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
			st->clientstream = FS_WrapTCPSocket(newsock, false, NET_AdrToString(tmpbuf, sizeof(tmpbuf), &st->remoteaddr));
			st->inlen = 0;

#ifdef HAVE_EPOLL
			{
				struct epoll_event event = {EPOLLIN|EPOLLOUT|EPOLLET, {&st->epoll}};
				st->con = con;
				st->epoll.Polled = FTENET_TCP_Polled;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, newsock, &event);
			}
#endif

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

neterr_t FTENET_TCP_SendPacket(ftenet_generic_connection_t *gcon, int length, const void *data, netadr_t *to)
{
	ftenet_tcp_connection_t *con = (ftenet_tcp_connection_t*)gcon;
	ftenet_tcp_stream_t *st;

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
							Con_DPrintf("FTENET_TCP_SendPacket: outgoing overflow\n");
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
						neterr_t e = FTENET_TCP_WebSocket_Splurge(st, (st->clienttype==TCPC_WEBSOCKETU)?WS_PACKETTYPE_TEXTFRAME:WS_PACKETTYPE_BINARYFRAME, data, length);
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

static int FTENET_TCP_GetLocalAddresses(struct ftenet_generic_connection_s *gcon, unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
{
	ftenet_tcp_connection_t *con = (ftenet_tcp_connection_t*)gcon;
	netproto_t prot = con->tls?NP_TLS:NP_STREAM;
	int i, r = FTENET_Generic_GetLocalAddresses(gcon, adrflags, addresses, adrparams, maxaddresses);
	for (i = 0; i < r; i++)
	{
		addresses[i].prot = prot;
	}
	return r;
}

static qboolean FTENET_TCP_ChangeLocalAddress(struct ftenet_generic_connection_s *con, const char *addressstring, netadr_t *adr)
{
	//if we're a server, we want to try switching listening tcp port without shutting down all other connections.
	//yes, this might mean we leave a connection active on the old port, but oh well.
	int addrsize, addrsize2;
	int family;
	struct sockaddr_qstorage qs;
	struct sockaddr_qstorage cur;
	netadr_t n;
	SOCKET newsocket = INVALID_SOCKET;
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
	if (newsocket == INVALID_SOCKET)
	if (family == AF_INET && net_hybriddualstack.ival && !((struct sockaddr_in*)&qs)->sin_addr.s_addr)
	{	//hybrid sockets pathway takes over when INADDR_ANY
		unsigned long _false = false;
		if ((newsocket = socket (AF_INET6, SOCK_STREAM, sysprot)) != INVALID_SOCKET)
		{
#ifdef __linux__	//note: windows blindly allows dupes, whereas linux prevents exact matches
			setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&_true, sizeof(_true));	//try to avoid 'address in use' problems when killing+restarting.
#elif defined(_WIN32)
			setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&_true, sizeof(_true));	//try to avoid 'address in use' problems when killing+restarting.
#endif

			if (0 == setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_false, sizeof(_false)))
			{
				memset(&n, 0, sizeof(n));
				n.type = NA_IPV6;
				n.port = adr->port;
				n.scopeid = adr->scopeid;
				addrsize2 = NetadrToSockadr(&n, &cur);

				if ((bind(newsocket, (struct sockaddr *)&cur, addrsize2) != INVALID_SOCKET) &&
					(listen(newsocket, 2) != INVALID_SOCKET) &&
					ioctlsocket (newsocket, FIONBIO, &_true) != -1)
				{
					con->addrtype[0] = NA_IP;
					con->addrtype[1] = NA_IPV6;
				}
				else
				{
					closesocket(newsocket);
					newsocket = INVALID_SOCKET;
				}
			}
			else
			{
				closesocket(newsocket);
				newsocket = INVALID_SOCKET;
			}
		}
	}
#endif

	if (newsocket == INVALID_SOCKET)
	{
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
				(listen(newsocket, 2) != INVALID_SOCKET) &&
				ioctlsocket (newsocket, FIONBIO, &_true) != -1)
				;
			else
			{
				closesocket(newsocket);
				newsocket = INVALID_SOCKET;
			}
		}
	}


	if (newsocket != INVALID_SOCKET)
	{
#ifdef UNIXSOCKETS
		if (family != NA_UNIX)
#endif
			setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));

		con->thesocket = newsocket;


#ifdef HAVE_EPOLL
		{
			struct epoll_event event = {EPOLLIN|EPOLLET, {NULL}};//&newcon->generic.epoll}};
			//newcon->generic.epoll.Polled = FTENET_TCP_AcceptPolled;
			epoll_ctl(epoll_fd, EPOLL_CTL_ADD, newsocket, &event);
		}
#endif
		return true;
	}
	return false;
}
static void FTENET_TCP_Close(ftenet_generic_connection_t *gcon)
{
	ftenet_tcp_connection_t *con = (ftenet_tcp_connection_t*)gcon;
	ftenet_tcp_stream_t *st;

	st = con->tcpstreams;
	while (con->tcpstreams)
	{
		st = con->tcpstreams;
		con->tcpstreams = st->next;

		if (st->clientstream != NULL)
			VFS_CLOSE(st->clientstream);

		BZ_Free(st);
	}

	FTENET_Datagram_Close(gcon);
}

#if defined(HAVE_PACKET) && !defined(HAVE_EPOLL)
static int FTENET_TCP_SetFDSets(ftenet_generic_connection_t *gcon, fd_set *readfdset, fd_set *writefdset)
{
	int maxfd = -1;
	ftenet_tcp_connection_t *con = (ftenet_tcp_connection_t*)gcon;
	ftenet_tcp_stream_t *st;

	for (st = con->tcpstreams; st; st = st->next)
	{
#ifdef SUPPORT_RTC_ICE
		if (st->webrtc.ice)
		{
			while(iceapi.ICE_GetLCandidateSDP(st->webrtc.ice, net_message_buffer, sizeof(net_message_buffer)))
			{
				FTENET_TCP_WebSocket_Splurge(st, WS_PACKETTYPE_BINARYFRAME, net_message_buffer, strlen(net_message_buffer));
			}
			continue;
		}
#endif
		if (st->clientstream == NULL || st->socketnum == INVALID_SOCKET)
			continue;
#ifdef HAVE_HTTPSV
		if (st->clienttype == TCPC_HTTPCLIENT && st->outlen)
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

ftenet_generic_connection_t *FTENET_TCP_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr)
{
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_tcp_connection_t *newcon;
	qboolean isserver = col->islisten;

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
	newcon->generic.thesocket = newsocket = INVALID_SOCKET;

	newcon->generic.addrtype[0] = adr.type;
	newcon->generic.addrtype[1] = NA_INVALID;

	if (isserver)
	{
#ifdef HAVE_PACKET	//unable to listen on tcp if we have no packet interface
		if (!FTENET_TCP_ChangeLocalAddress(&newcon->generic, address, &adr))
		{
			Z_Free(newcon);
			return NULL;
		}
#endif
	}
	else
	{
		newsocket = TCP_OpenStream(&adr, address);
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
			newcon->generic.GetLocalAddresses = FTENET_TCP_GetLocalAddresses;
			newcon->generic.ChangeLocalAddress = FTENET_TCP_ChangeLocalAddress;
		}
		newcon->generic.GetPacket = FTENET_TCP_GetPacket;
		newcon->generic.SendPacket = FTENET_TCP_SendPacket;
		newcon->generic.Close = FTENET_TCP_Close;
#if defined(HAVE_PACKET) && !defined(HAVE_EPOLL)
		newcon->generic.SetFDSets = FTENET_TCP_SetFDSets;
#endif
		newcon->generic.PrintStatus = FTENET_TCP_PrintStatus;

		newcon->generic.owner = col;
		newcon->generic.islisten = isserver;

		newcon->active = 0;

		if (!isserver)
		{
			newcon->active++;
			newcon->tcpstreams = Z_Malloc(sizeof(*newcon->tcpstreams));
			newcon->tcpstreams->next = NULL;
			newcon->tcpstreams->socketnum = newsocket;
			newcon->tcpstreams->clientstream = FS_WrapTCPSocket(newsocket, true, address);
			newcon->tcpstreams->inlen = 0;

			newcon->tcpstreams->remoteaddr = adr;

#ifdef HAVE_SSL
			if (newcon->tls)	//if we're meant to be using tls, wrap the stream in a tls connection
			{	//remove any markup junk, get just the hostname out of it.
				char hostonly[MAX_QPATH];
				const char *host = strstr(address, "://");
				const char *port;
				host = host?host+3:address;
				port = strchr(host, ':');
				if (!port)
					port = host+strlen(host);

				if (port-host >= sizeof(hostonly))
				{
					VFS_CLOSE(&newcon->generic);
					return NULL;
				}
				memcpy(hostonly, host, port-host);
				hostonly[port-host] = 0;

				newcon->tcpstreams->clientstream = FS_OpenSSL(hostonly, newcon->tcpstreams->clientstream, false);
				if (!newcon->tcpstreams->clientstream)
					return NULL;
			}
#endif

			//send the qizmo greeting. any actual data is just <ushort len><byte data[len]> otherwise consistent with qw/udp, including challenges.
			newcon->tcpstreams->clienttype = TCPC_UNKNOWN;
			memcpy(newcon->tcpstreams->outbuffer, "qizmo\n", newcon->tcpstreams->outlen = 6);

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
cvar_t  net_ice_servers = CVAR("net_ice_servers", "");
cvar_t  net_ice_relayonly = CVAR("net_ice_relayonly", "0");

#include "web/ftejslib.h"

typedef struct
{
	ftenet_generic_connection_t generic;
	int brokersock;	//only if rtc
	netadr_t remoteadr;
	qboolean failed;

	int datasock;	//only if we're a client
	double heartbeat;	//timestamp of next heartbeat.

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

static void FTENET_WebRTC_Heartbeat(ftenet_websocket_connection_t *b)
{
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

		info[0] = ICEMSG_SERVERINFO;
		info[1] =
		info[2] = 0xff;	//to the broker rather than any actual client
		info[3] = 0;
		Info_SetValueForKey(info+3, "protocol", SV_GetProtocolVersionString(), sizeof(info)-3);
		Info_SetValueForKey(info+3, "maxclients", maxclients.string, sizeof(info)-3);
		Info_SetValueForKey(info+3, "clients", va("%i", numclients), sizeof(info)-3);
		Info_SetValueForKey(info+3, "hostname", hostname.string, sizeof(info)-3);
		Info_SetValueForKey(info+3, "modname", FS_GetGamedir(true), sizeof(info)-3);
		Info_SetValueForKey(info+3, "mapname", InfoBuf_ValueForKey(&svs.info, "map"), sizeof(info)-3);
		Info_SetValueForKey(info+3, "needpass", InfoBuf_ValueForKey(&svs.info, "needpass"), sizeof(info)-3);

		if (emscriptenfte_ws_send(b->brokersock, info, 3+strlen(info+3)) < 0)
			return;
	}
#endif
	b->heartbeat = realtime+30;
}

//called from the javascript when there was some ice event. just forwards over the broker connection.
static void FTENET_WebRTC_Callback(void *ctxp, int ctxi, int/*enum icemsgtype_s*/ evtype, const char *data)
{
	ftenet_websocket_connection_t *wsc = ctxp;
	size_t dl = strlen(data);
	qbyte *o = net_message_buffer;
	*o++ = evtype;
	*o++ = (ctxi>>0)&0xff;
	*o++ = (ctxi>>8)&0xff;
	memcpy(o, data, dl);
	o+=dl;
	//Con_Printf("To Broker: %i %i %s\n", evtype, ctxi, data);
	emscriptenfte_ws_send(wsc->brokersock, net_message_buffer, o-net_message_buffer);
}
static int FTENET_WebRTC_Create(qboolean initiator, ftenet_websocket_connection_t *wsc, int clid)
{
	char config[4096], tmp[256];
	qboolean first = true;
	const char *servers;

	*config = 0;
	Q_strncatz(config, "{\"iceServers\":[",	sizeof(config));
	{
		/*
			rtc://broker/id
			rtc:///id
			/id
		*/
		char *c;
		int i;
		const char *brokeraddress = wsc->remoteadr.address.websocketurl;

		char *pre[] = {	"wss://",	"ices://",	"rtcs://",	"tls://",
						"ws://",	"ice://",	"rtc://",	"tcp://"};

		//try and clean up the prefix, if specified
		for (i = countof(pre); i --> 0; )
		{
			if (!strncmp(brokeraddress, pre[i], strlen(pre[i])))
			{
				brokeraddress += strlen(pre[i]);
				break;
			}
		}
		if (*brokeraddress == '/')
		{
			brokeraddress = net_ice_broker.string;
			for (i = countof(pre); i --> 0; )
			{
				if (!strncmp(brokeraddress, pre[i], strlen(pre[i])))
				{
					brokeraddress += strlen(pre[i]);
					break;
				}
			}
		}
		Q_strncpyz(com_token, brokeraddress, sizeof(com_token));
		c = strchr(com_token, '/');
		if (c) *c = 0;

		first = false;
		Q_strncatz(config, va("\n{\"urls\":[\"stun:%s\"]}", COM_QuotedString(com_token, tmp,sizeof(tmp), true)), sizeof(config));
	}

	for(servers = net_ice_servers.string; (servers=COM_Parse(servers)); )
	{
		//we don't do the ?foo stuff properly (RFCs say only ?transport= and only for stun)
		char *s = strchr(com_token, '?'), *next;
		const char *transport = NULL;
		const char *user = NULL;
		const char *auth = NULL;
		for (;s;s=next)
		{
			*s++ = 0;
			next = strchr(s, '?');
			if (next)
				*next = 0;

			if (!strncmp(s, "transport=", 10))
				transport = s+10;
			else if (!strncmp(s, "user=", 5))
				user = s+5;
			else if (!strncmp(s, "auth=", 5))
				auth = s+5;
			else if (!strncmp(s, "fam=", 4))
				;
		}

		if (!strncmp(com_token, "turn:", 5) || !strncmp(com_token, "turns:", 6))
			if (!user || !auth)
				continue;

		if (first)
			first = false;
		else
			Q_strncatz(config, ",", sizeof(config));
		if (transport)
			Q_strncatz(config, va("\n{\"urls\":[\"%s?transport=%s\"]", COM_QuotedString(com_token, tmp,sizeof(tmp), true), transport), sizeof(config));
		else
			Q_strncatz(config, va("\n{\"urls\":[\"%s\"]", COM_QuotedString(com_token, tmp,sizeof(tmp), true)), sizeof(config));
		if (user)
			Q_strncatz(config, va(",\"username\":\"%s\"", COM_QuotedString(user, tmp,sizeof(tmp), true)), sizeof(config));
		if (auth)
			Q_strncatz(config, va(",\"credential\":\"%s\"", COM_QuotedString(auth, tmp,sizeof(tmp), true)), sizeof(config));
		Q_strncatz(config, "}", sizeof(config));
	}
	Q_strncatz(config, va("]"
//		",\"bundlePolicy\":\"max-bundle\""
		",\"iceTransportPolicy\":\"%s\""
		"}",net_ice_relayonly.ival?"relay":"all"),	sizeof(config));

	return emscriptenfte_rtc_create(initiator, wsc, clid, FTENET_WebRTC_Callback, config);
}

static qboolean FTENET_WebRTC_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	size_t i;

	if (wsc->heartbeat < realtime)
		FTENET_WebRTC_Heartbeat(wsc);

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

	if (wsc->brokersock == INVALID_SOCKET)
		net_message.cursize = 0;
	else
	{
		net_message.cursize = emscriptenfte_ws_recv(wsc->brokersock, net_message_buffer, sizeof(net_message_buffer));
		if (net_message.cursize < 0)
		{
			emscriptenfte_ws_close(wsc->brokersock);
			wsc->brokersock = INVALID_SOCKET; //error!
		}
	}
	if (net_message.cursize > 0)
	{
		int cmd;
		short cl;
		const char *s;
		char *p;

		MSG_BeginReading(&net_message, msg_nullnetprim);
		cmd = MSG_ReadByte();
		cl = MSG_ReadShort();

		//Con_Printf("From Broker: %i %i\n", cmd, cl);

		switch(cmd)
		{
		case ICEMSG_PEERLOST:	//connection closing...
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
//				Con_Printf("Broker closing connection: %s\n", MSG_ReadString());
			}
			break;
		case ICEMSG_GREETING:	//reports the trailing url we're 'listening' on. anyone else using that url will connect to us.
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
		case ICEMSG_NEWPEER:	//connection established with a new peer
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
					char id[256];
					Q_snprintfz(id, sizeof(id), "/%i_%x", cl+1, rand());
					if (wsc->clients[cl].datasock != INVALID_SOCKET)
						emscriptenfte_ws_close(wsc->clients[cl].datasock);
					memcpy(&wsc->clients[cl].remoteadr, &wsc->remoteadr, sizeof(netadr_t));
					Q_strncatz(wsc->clients[cl].remoteadr.address.websocketurl, id, sizeof(wsc->clients[cl].remoteadr.address.websocketurl));
					wsc->clients[cl].remoteadr.port = htons(cl+1);
					wsc->clients[cl].datasock = FTENET_WebRTC_Create(false, wsc, cl);
				}
			}
			else
			{
				if (wsc->datasock != INVALID_SOCKET)
					emscriptenfte_ws_close(wsc->datasock);
				wsc->datasock = FTENET_WebRTC_Create(true, wsc, cl);
			}
			break;
		case ICEMSG_OFFER:	//we received an offer from a client
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
		case ICEMSG_CANDIDATE:
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
			{
				if (wsc->brokersock == INVALID_SOCKET)
					return NETERR_DISCONNECTED;	//no broker nor active data channel. its dead jim.
				return NETERR_CLOGGED;	//we're still waiting for the broker to give us a server... or for a server to become available.
			}
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

static int FTENET_WebRTC_GetAddresses(struct ftenet_generic_connection_s *con, unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
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

static int FTENET_WebRTC_Establish(const char *address, const char *type)
{
	/*
		rtc://broker/id
		rtc:///id
		/id
	*/
	const char *path, *host;
	char *c;
	int i;
	char url[512];
	char cleanaddress[512];
	char *udp="";

	char *pre[] = {	"wss://",	"ices://",	"rtcs://",	"tls://",
					"ws://",	"ice://",	"rtc://",	"tcp://"};

	//try and clean up the prefix, if specified
	for (i = countof(pre); i --> 0; )
	{
		if (!strncmp(address, pre[i], strlen(pre[i])))
		{
			address += strlen(pre[i]);
			i -= i%(countof(pre)/2);
			break;
		}
	}

	host = address;
	if (*address == '/')
	{
		path = address+1;
		address = NULL;
	}
	else
	{
		path = strchr(address, '/');
		if (!path)
		{
			if (i<0)
			{
				udp = "udp/";
				path = address;
				address = NULL;
			}
			else
				path = "";
		}
	}
	if (!address)
	{
		address = net_ice_broker.string;
		for (i = countof(pre); i --> 0; )
		{
			if (!strncmp(address, pre[i], strlen(pre[i])))
			{
				address += strlen(pre[i]);
				i -= i%(countof(pre)/2);
				break;
			}
		}
		if (i<0)
			i = 0; //default to the first one... wss...
		if (!*address)
			return INVALID_SOCKET;
	}

	Q_strncpyz(cleanaddress, address, sizeof(cleanaddress));
	c = strchr(cleanaddress, '/');
	if (c) *c = 0;
	COM_Parse(com_protocolname.string);
	Q_snprintfz(url, sizeof(url), "%s%s/%s/%s%s", pre[i], cleanaddress, com_token, udp, path);
	return emscriptenfte_ws_connect(url, type);
}

static qboolean FTENET_WebRTC_ChangeLocalAddress(struct ftenet_generic_connection_s *con, const char *addressstring, netadr_t *adr)
{
	//ftenet_websocket_connection_t *wsc = (void*)con;
	return true;	//pretend we changed it, because needed to change in the first place.
	//doesn't match how its currently bound, so I guess we need to rebind then.
//	return false;
}

static ftenet_generic_connection_t *FTENET_WebSocket_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr)
{
	qboolean isserver = col->islisten;
	ftenet_websocket_connection_t *newcon;

	int brokersocket = INVALID_SOCKET;
	int datasocket = INVALID_SOCKET;

	newcon = Z_Malloc(sizeof(*newcon));

	if (isserver)
	{
		Con_Printf("Browsers are unable to host regular servers. Please use an rtc://broker:port/serverid scheme instead.\n");
		datasocket = INVALID_SOCKET;
	}
	else
		datasocket = emscriptenfte_ws_connect(adr.address.websocketurl, "fteqw");

	newcon->generic.GetPacket = FTENET_WebSocket_GetPacket;
	newcon->generic.SendPacket = FTENET_WebSocket_SendPacket;
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
		newcon->heartbeat = realtime-1;

		adr.port = 0;
		newcon->remoteadr = adr;

		return &newcon->generic;
	}
	return NULL;
}

static ftenet_generic_connection_t *FTENET_WebRTC_EstablishConnection(ftenet_connections_t *col, const char *address, netadr_t adr)
{
	qboolean isserver = col->islisten;
	ftenet_websocket_connection_t *newcon;

	int brokersocket = INVALID_SOCKET;
	int datasocket = INVALID_SOCKET;

	newcon = Z_Malloc(sizeof(*newcon));

	if (adr.type == NA_INVALID)
	{	//if its using our broker, flip it over to a real address type, if we can.
		adr.type = NA_WEBSOCKET;
		Q_strncpyz(adr.address.websocketurl, net_ice_broker.string, sizeof(adr.address.websocketurl));
	}

	brokersocket = FTENET_WebRTC_Establish(address, isserver?"rtc_host":"rtc_client");

	newcon->generic.GetPacket = FTENET_WebRTC_GetPacket;
	newcon->generic.SendPacket = FTENET_WebRTC_SendPacket;
	newcon->generic.GetLocalAddresses = FTENET_WebRTC_GetAddresses;

	newcon->generic.ChangeLocalAddress = FTENET_WebRTC_ChangeLocalAddress;

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
		newcon->heartbeat = realtime-1;

		adr.port = 0;
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

void NET_ReadPackets (ftenet_connections_t *collection)
{
	struct ftenet_delayed_packet_s *p;
	unsigned int ctime;
	size_t c = 0;

	if (!collection)
		return;

	while ((p = collection->delayed_packets) && (int)(Sys_Milliseconds()-p->sendtime) > 0)
	{
		collection->delayed_packets = p->next;
#ifdef SUPPORT_ICE
		if (p->dest.type == NA_ICE)
			NET_SendPacketCol (collection, p->cursize, p->data, &p->dest);
		else
#endif
#ifdef HAVE_DTLS
		if (p->dest.prot == NP_DTLS)
			FTENET_DTLS_SendPacket(collection, p->cursize, p->data, &p->dest);
		else
#endif
			NET_SendPacketCol (collection, p->cursize, p->data, &p->dest);
		Z_Free(p);
	}

	for (c = 0; c < MAX_CONNECTIONS; c++)
	{
		if (collection->conn[c])
		{
			while (collection->conn[c]->GetPacket(collection->conn[c]))
			{
				if (net_fakeloss.value)
				{
					if (frandom () < net_fakeloss.value)
						continue;
				}

				collection->bytesin += net_message.cursize;
				collection->packetsin += 1;
				net_from.connum = c+1;
				net_from_connection = collection->conn[c];
				collection->ReadGamePacket();
			}
		}
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

#ifdef HAVE_DTLS
	NET_DTLS_Timeouts(collection);
#endif
}

int NET_LocalAddressForRemote(ftenet_connections_t *collection, netadr_t *remote, netadr_t *local, int idx)
{
	int adrflags;
	const char *adrparams;
	if (!remote->connum)
		return 0;

	if (!collection->conn[remote->connum-1])
		return 0;

	if (!collection->conn[remote->connum-1]->GetLocalAddresses)
		return 0;

	return collection->conn[remote->connum-1]->GetLocalAddresses(collection->conn[remote->connum-1], &adrflags, local, &adrparams, 1);
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
		i = to->connum-1;
		if (i < MAX_CONNECTIONS && collection->conn[i])
		{
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
		return NETERR_NOROUTE;
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

#ifdef SUPPORT_ICE
	if (to->type == NA_ICE)
		return ICE_SendPacket(length, data, to);
#endif
#ifdef HAVE_DTLS
	if (to->prot == NP_DTLS)
		return FTENET_DTLS_SendPacket(collection, length, data, to);
#endif
	return NET_SendPacketCol (collection, length, data, to);
}

qboolean NET_EnsureRoute(ftenet_connections_t *collection, char *routename, const struct dtlspeercred_s *peerinfo, netadr_t *adr, qboolean outgoing)
{
	switch(adr->prot)
	{
	case NP_DGRAM:
		if (NET_SendPacketCol(collection, 0, NULL, adr) != NETERR_NOROUTE)
			return true;
		if (!FTENET_AddToCollection(collection, routename, "0", adr->type, adr->prot))
			return false;
		break;
	case NP_DTLS:
#ifdef HAVE_DTLS
		adr->prot = NP_DGRAM;
		if (NET_EnsureRoute(collection, routename, peerinfo, adr, outgoing))
		{
			dtlscred_t cred;
			memset(&cred, 0, sizeof(cred));
			cred.peer = *peerinfo;
			if (NET_DTLS_Create(collection, adr, &cred, outgoing))
			{
				adr->prot = NP_DTLS;
				return true;
			}
		}
		adr->prot = NP_DTLS;
#endif
		return false;
	case NP_WS:
	case NP_WSS:
	case NP_TLS:
	case NP_STREAM:
		if (!FTENET_AddToCollection(collection, routename, peerinfo->name, adr->type, adr->prot))
			return false;
		Con_Printf("Establishing connection to %s\n", peerinfo->name);
		break;
#if defined(SUPPORT_ICE) || defined(FTE_TARGET_WEB)
	case NP_RTC_TCP:
	case NP_RTC_TLS:
		if (!FTENET_AddToCollection(collection, routename, peerinfo->name, adr->type, adr->prot))
			return false;
		break;
#endif
	default:
		//not recognised, or not needed
		break;
	}
	return true;
}
void NET_TerminateRoute(ftenet_connections_t *collection, netadr_t *adr)
{
	switch(adr->prot)
	{
	case NP_DTLS:
#ifdef HAVE_DTLS
		NET_DTLS_Disconnect(collection, adr);
#endif
		break;
	default:
		break;
	}

#ifdef SUPPORT_ICE
	if (adr->type == NA_ICE)
		ICE_Terminate(adr);
#endif
}

int NET_EnumerateAddresses(ftenet_connections_t *collection, struct ftenet_generic_connection_s **con, unsigned int *adrflags, netadr_t *addresses, const char **adrparams, int maxaddresses)
{
	unsigned int found = 0, c, i, j;
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			continue;

		if (collection->conn[i]->GetLocalAddresses)
			c = collection->conn[i]->GetLocalAddresses(collection->conn[i], adrflags+found, addresses+found, adrparams+found, maxaddresses-found);
		else
			c = 0;

		if (maxaddresses>found && !c)
		{
			adrflags[found] = 0;
			adrparams[found] = NULL;
			addresses[found].type = NA_INVALID;
			addresses[found].prot = NP_INVALID;
			c = 1;
		}

		//fill in connection info
		for (j = 0; j < c; j++)
		{
			con[found+j] = collection->conn[i];
			addresses[found+j].connum = i+1;
		}

		found += c;
	}
	return found;
}

static enum addressscope_e NET_ClassifyAddressipv4(int ip, const char **outdesc)
{
	int scope = ASCOPE_NET;
	const char *desc = NULL;
	if ((ip&BigLong(0xffff0000)) == BigLong(0xA9FE0000))	//169.254.x.x/16
		scope = ASCOPE_LINK, desc = localtext("link-local");
	else if ((ip&BigLong(0xff000000)) == BigLong(0x0a000000))	//10.x.x.x/8
		scope = ASCOPE_LAN, desc = localtext("private");
	else if ((ip&BigLong(0xff000000)) == BigLong(0x7f000000))	//127.x.x.x/8
		scope = ASCOPE_HOST, desc = "localhost";
	else if ((ip&BigLong(0xfff00000)) == BigLong(0xac100000))	//172.16.x.x/12
		scope = ASCOPE_LAN, desc = localtext("private");
	else if ((ip&BigLong(0xffff0000)) == BigLong(0xc0a80000))	//192.168.x.x/16
		scope = ASCOPE_LAN, desc = localtext("private");
	else if ((ip&BigLong(0xffc00000)) == BigLong(0x64400000))	//100.64.x.x/10
		scope = ASCOPE_LAN, desc = localtext("CGNAT");
	else if (ip == BigLong(0x00000000))	//0.0.0.0/32
		scope = ASCOPE_HOST, desc = "any";

	*outdesc = desc;
	return scope;
}
enum addressscope_e NET_ClassifyAddress(netadr_t *adr, const char **outdesc)
{
	int scope = ASCOPE_NET;
	const char *desc = NULL;

	if (adr->type == NA_LOOPBACK)
	{
		//we don't list 127.0.0.1 or ::1, so don't bother with this either. its not interesting.
		scope = ASCOPE_PROCESS, desc = localtext("internal");
	}
	else if (adr->type == NA_IPV6)
	{
		if ((*(int*)adr->address.ip6&BigLong(0xffc00000)) == BigLong(0xfe800000))	//fe80::/10
			scope = ASCOPE_LINK, desc = localtext("link-local");
		else if ((*(int*)adr->address.ip6&BigLong(0xfe000000)) == BigLong(0xfc00000))	//fc::/7
			scope = ASCOPE_LAN, desc = localtext("ULA/private");
		else if (*(int*)adr->address.ip6 == BigLong(0x20010000)) //2001::/32
			scope = ASCOPE_NET, desc = "toredo";
		else if ((*(int*)adr->address.ip6&BigLong(0xffff0000)) == BigLong(0x20020000)) //2002::/16
			scope = ASCOPE_NET, desc = "6to4";
		else if (memcmp(adr->address.ip6, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16) == 0)	//::1
			scope = ASCOPE_HOST, desc = "localhost";
		else if (memcmp(adr->address.ip6, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0)	//::
			scope = ASCOPE_HOST, desc = "any";
		else if (memcmp(adr->address.ip6, "\0\0\0\0\0\0\0\0\0\0\xff\xff", 12) == 0)	//::ffff:x.y.z.w
		{
			scope = NET_ClassifyAddressipv4(*(int*)(adr->address.ip6+12), &desc);
			if (!desc)
				desc = localtext("v4-mapped");
		}
	}
#ifdef UNIXSOCKETS
	else if (adr->type == NA_UNIX)
	{
		scope = ASCOPE_HOST, desc = "unix";
	}
#endif
	else if (adr->type == NA_IP)
		scope = NET_ClassifyAddressipv4(*(int*)adr->address.ip, &desc);
	if (outdesc)
		*outdesc = desc;
	return scope;
}

#define MAXADDRESSES 64
void NET_PrintAddresses(ftenet_connections_t *collection)
{
	int i, j;
	char adrbuf[MAX_ADR_SIZE];
	int m;
	qboolean shown = false;
	netadr_t	addr[64];
	struct ftenet_generic_connection_s			*con[sizeof(addr)/sizeof(addr[0])], *nc;
	int			flags[sizeof(addr)/sizeof(addr[0])];
	const char *params[sizeof(addr)/sizeof(addr[0])];
	qboolean	warn = true;
	static const char *scopes[] = {S_COLOR_GRAY"process", S_COLOR_GRAY"local", S_COLOR_GRAY"link", S_COLOR_GRAY"lan", "net"};
	const char *desc;
	char *fp, *scheme;

	if (!collection)
		return;

	m = NET_EnumerateAddresses(collection, con, flags, addr, params, sizeof(addr)/sizeof(addr[0]));

	for (i = 0; i < m; i++)
	{
		if (addr[i].type != NA_INVALID)
		{
			enum addressscope_e scope = NET_ClassifyAddress(&addr[i], &desc);
			if (i+1 < m)
				nc = con[i+1];
			else
				nc = NULL;
			if (nc != con[i])
			{	//next is a different family.
				if (!shown && (scope == ASCOPE_LINK || scope == ASCOPE_HOST))
					scope = ASCOPE_LAN;	//force it visible.
				shown = false;
			}
			if (developer.ival || scope >= ASCOPE_LAN)
			{
				shown = true;
				warn = false;
				for (j = 0; j < countof(collection->srflx); j++)
				{
					if (collection->srflx[j].type!=NA_INVALID && NET_CompareAdr(&collection->srflx[j], &addr[i]))
						break;
				}
				if (m < countof(collection->srflx))
					;	//gonna print it later anyway.
				else if ((addr[i].prot == NP_RTC_TCP || addr[i].prot == NP_RTC_TLS) && params[i])
				{
					if (addr[i].type == NA_INVALID)
						Con_TPrintf("%s address (%s): /%s\n", scopes[scope], con[i]->name, params[i]);
					else
						Con_TPrintf("%s address (%s): %s/%s\n", scopes[scope], con[i]->name, NET_AdrToString(adrbuf, sizeof(adrbuf), &addr[i]), params[i]);
				}
				else if (desc)
					Con_TPrintf("%s address (%s): %s (%s)\n", scopes[scope], con[i]->name, NET_AdrToString(adrbuf, sizeof(adrbuf), &addr[i]), desc);
				else
					Con_TPrintf("%s address (%s): %s\n", scopes[scope], con[i]->name, NET_AdrToString(adrbuf, sizeof(adrbuf), &addr[i]));
			}
		}
	}

	fp = "";
	scheme = "";
#if defined(HAVE_SERVER) && defined(HAVE_DTLS)
	if (collection == svs.sockets)
	{
		fp = InfoBuf_ValueForKey(&svs.info, "*fp");
		if (*fp)
			fp = va(S_COLOR_GRAY"?fp=%s", fp);
		if (*COM_Parse(fs_manifest->schemes))
			scheme = va(S_COLOR_GRAY"%s://", com_token);
	}
#endif
	//show any master-reflexive addresses that were not above.
	for (m = 0; m < countof(collection->srflx); m++)
	{
		if (collection->srflx[m].type == NA_INVALID)
			continue;
		if (collection->srflx[m].connum && collection->srflx[m].connum-1u < countof(collection->conn) && collection->conn[collection->srflx[m].connum-1])
			Con_TPrintf("reflexive address (%s): %s"S_COLOR_WHITE"%s%s\n", collection->conn[collection->srflx[m].connum-1]->name, scheme, NET_AdrToString(adrbuf, sizeof(adrbuf), &collection->srflx[m]), fp);
		else
			Con_TPrintf("reflexive address: %s"S_COLOR_WHITE"%s%s\n", scheme, NET_AdrToString(adrbuf, sizeof(adrbuf), &collection->srflx[m]), fp);
		warn = false;
	}

	if (warn)
		Con_TPrintf("net address: no public addresses\n");
}

void NET_PrintConnectionsStatus(ftenet_connections_t *collection)
{
	unsigned int i;
	if (!collection)
		return;
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

int TCP_OpenStream (netadr_t *remoteaddr, const char *remotename)
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
	//case NA_UNIX:
	default:
		sysprot = 0;	//'auto'
		break;
	}
	temp = NetadrToSockadr(remoteaddr, &qs);

	if ((newsocket = socket (((struct sockaddr_in*)&qs)->sin_family, SOCK_CLOEXEC|SOCK_STREAM, sysprot)) == INVALID_SOCKET)
		return (int)INVALID_SOCKET;

	setsockopt(newsocket, SOL_SOCKET, SO_RCVBUF, (void*)&recvbufsize, sizeof(recvbufsize));

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("TCP_OpenStream: ioctl FIONBIO: %s", strerror(neterrno()));

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
			if (err == NET_EADDRNOTAVAIL)
			{
				if (remoteaddr->port == 0 && (remoteaddr->type == NA_IP || remoteaddr->type == NA_IPV6))
					Con_Printf ("TCP_OpenStream: no port specified (%s)\n", remotename);
				else
					Con_Printf ("TCP_OpenStream: invalid address trying to connect to %s\n", remotename);
			}
			else if (err == NET_ECONNREFUSED)
				Con_Printf ("TCP_OpenStream: connection refused (%s)\n", remotename);
			else if (err == NET_EACCES)
				Con_Printf ("TCP_OpenStream: access denied: check firewall (%s)\n", remotename);
			else if (err == NET_ENETUNREACH)
				Con_Printf ("TCP_OpenStream: unreachable (%s)\n", remotename);
			else
				Con_Printf ("TCP_OpenStream: connect: error %i (%s)\n", err, remotename);
			closesocket(newsocket);
			return (int)INVALID_SOCKET;
		}
	}

	return newsocket;
#endif
}

#if defined(SV_MASTER) || defined(CL_MASTER)
#ifdef HAVE_IPV4
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
void UDP_CloseSocket (int socket)
{
	closesocket(socket);
}
#endif

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
void UDP6_CloseSocket (int socket)
{
	closesocket(socket);
}
#endif

#ifdef HAVE_IPX
int IPX_OpenSocket (int port)
{
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
}

void IPX_CloseSocket (int socket)
{
	closesocket(socket);
}
#endif
#endif

#ifdef HAVE_EPOLL
static qboolean stdin_ready;
static qboolean stdin_epolling;
static void StdIn_Now_Ready (struct epollctx_s *ctx, unsigned int events)
{
	stdin_ready = true;
}
qboolean NET_Sleep(float seconds, qboolean stdinissocket)
{
	int waitms;
	struct epoll_event waitevents[256];
	int n, i;
	if (epoll_fd < 0)
		return false; // o.O
	if (stdin_epolling != stdinissocket)
	{
		static epollctx_t stdinctx = {StdIn_Now_Ready};
		struct epoll_event event = {EPOLLIN, {&stdinctx}};
		stdin_epolling = stdinissocket;
		if (stdinissocket)
			epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event);
		else
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, STDIN_FILENO, &event);
	}

	waitms = bound(0, (int)(seconds*1000), 4*1000);
	n = epoll_wait(epoll_fd, waitevents, countof(waitevents), waitms);
	if (n < 0)
	{
		int err = errno;
		switch(err)
		{
		case EINTR:
			break;
		default:
			Con_Printf("EPoll error: %s\n", strerror(err));
			break;
		}
	}
	for (i = 0; i < n; i++)
	{
		struct epoll_event *ev = &waitevents[i];
		struct epollctx_s *ctx = ev->data.ptr;
		if (ctx)
			ctx->Polled(ctx, ev->events);
		//else edge-triggered events can be processed as part of the main loop
	}
	return stdin_ready;
}
#else
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

#ifndef _WIN32
	if (stdinissocket)
	{
		sock = STDIN_FILENO;	//stdin tends to be socket/filehandle 0 in unix
		FD_SET(sock, &readfdset);
		maxfd = sock;
	}
#endif

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

	seconds = bound(0.0, seconds, 4.0);	//realy? oh well.
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

#ifndef _WIN32
	if (stdinissocket)
		return FD_ISSET(STDIN_FILENO, &readfdset);
#endif
#endif
	return true;
}
#endif

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
		svs.sockets = FTENET_CreateCollection(true, SV_ReadPacket);
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

#if defined(SUPPORT_ICE) || defined(MASTERONLY)
	if (ICE_WasStun(collection))
		return true;
#endif

#if defined(HAVE_DTLS) && defined(HAVE_SERVER)
	if (collection->islisten && NET_DTLS_CheckInbound(collection))
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
#ifdef HAVE_EPOLL
	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
#endif

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
			{(void**)&pgetnameinfo, "getnameinfo"},
			{NULL, NULL}
		};
		Sys_LoadLibrary("ws2_32.dll", fncs);
#endif

		r = WSAStartup (MAKEWORD(2, 2), &winsockdata);

		if (r)
			Sys_Error ("Winsock initialization failed.");
#endif
	}

	Cvar_Register(&net_ice_broker, "networking");
	Cvar_Register(&timeout, "networking");
	Cvar_Register(&net_hybriddualstack, "networking");
	Cvar_Register(&net_fakeloss, "networking");

#if defined(HAVE_SSL)
	Cvar_Register(&tls_provider, "networking");
	Cvar_Register(&tls_ignorecertificateerrors, "networking");
#endif
#if defined(TCPCONNECT) && (defined(HAVE_SERVER) || defined(HAVE_HTTPSV))
#ifdef HAVE_SERVER
	Cvar_Register(&net_enable_qizmo, "networking");
#endif
#ifdef MVD_RECORDING
	Cvar_Register(&net_enable_qtv, "networking");
#endif
#if defined(HAVE_SSL)
	Cvar_Register(&net_enable_tls, "networking");
#endif
#ifdef HAVE_HTTPSV
	Cvar_Register(&net_enable_http, "networking");
	Cvar_Register(&net_enable_websockets, "networking");
	Cvar_Register(&net_enable_rtcbroker, "networking");
#endif
#endif
#ifdef HAVE_DTLS
	Cvar_Register(&net_enable_dtls, "networking");
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

#ifdef SUPPORT_ICE
	ICE_Init();
#endif

#if defined(HAVE_CLIENT)||defined(HAVE_SERVER)
	Net_Master_Init();
#endif

#if defined(SUBSERVERS) && defined(HAVE_SERVER)
	if (isDedicated && !SSV_IsSubServer())
	{	//-clusterhost address:port password
		//connects this server to a remote control/gateway server.
		int i = COM_CheckParm("-clusterhost");
		if (i && i+2 < com_argc)
		{
			vfsfile_t *f = FS_OpenTCP(com_argv[i+1], PORT_DEFAULTSERVER, true);
			if (!f)
				Sys_Error("Unable to resolve/connect to cluster host address \"%s\"\n", com_argv[i+1]);
			VFS_PRINTF(f, "NODE\r\nPassword: \"%s\"\r\n", com_argv[i+2]);
			SSV_SetupControlPipe(f);
			return;
		}
	}
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
		cls.sockets = FTENET_CreateCollection(false, CL_ReadPacket);
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
	Cvar_Register (&net_ice_relayonly,	"networking");
	Cvar_Register (&net_ice_servers,	"networking");
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

#ifdef HAVE_DTLS
	Cvar_Register (&dtls_psk_hint,			"networking");
	Cvar_Register (&dtls_psk_user,			"networking");
	Cvar_Register (&dtls_psk_key,			"networking");
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

#ifdef HAVE_EPOLL
	if (epoll_fd < 0)
		epoll_fd = epoll_create1(EPOLL_CLOEXEC);
#endif

	if ((sv_listen_nq.value || sv_listen_dp.value || sv_listen_qw.value
#ifdef QWOVERQ3
		|| sv_listen_q3.ival
#endif
		) && !singleplayer)
	{
		if (!svs.sockets)
		{
			svs.sockets = FTENET_CreateCollection(true, SV_ReadPacket);
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
#if defined(SUPPORT_ICE) || defined(FTE_TARGET_WEB)
		Cvar_ForceCallback(&sv_public);
#endif
	}
	else
	{
		NET_CloseServer();

#ifdef HAVE_CLIENT
		svs.sockets = FTENET_CreateCollection(true, SV_ReadPacket);
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


#ifdef HAVE_EPOLL
	if (epoll_fd >= 0)
		close(epoll_fd);
	epoll_fd = -1;
	stdin_epolling = false;
#endif


#if defined(_WIN32) && defined(HAVE_PACKET)
#ifdef SERVERTONLY
	if (!serverthreadID)	//running as subsystem of client. Don't close all of it's sockets too.
#endif
		WSACleanup ();
#endif
}






#ifdef HAVE_TCP
#ifdef HAVE_EPOLL
#include <poll.h>
#endif
static int VFSTCP_IsStillConnecting(SOCKET sock)
{
#ifdef HAVE_EPOLL
	//poll has no arbitrary fd limit. use it instead of select where possible.
	struct pollfd ourfd[1];
	ourfd[0].fd = sock;
	ourfd[0].events = POLLOUT;
	ourfd[0].revents = 0;
	if (!poll(ourfd, countof(ourfd), 0))
	{
		if (ourfd[0].revents & POLLERR)
			return VFS_ERROR_UNSPECIFIED;
		if (ourfd[0].revents & POLLHUP)
			return VFS_ERROR_REFUSED;
		return true;	//no events yet.
	}
#else
	//okay on windows where sock+1 is ignored, has issues when lots of other fds are already open (for any reason).
	fd_set fdw, fdx;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO(&fdw);
	FD_SET(sock, &fdw);
	FD_ZERO(&fdx);
	FD_SET(sock, &fdx);
	//check if we can actually write to it yet, without generating weird errors...
	if (!select((int)sock+1, NULL, &fdw, &fdx, &timeout))
		return true;
#endif

	//if we get here then its writable(read: connected) or failed.

//	int error = NET_ENOTCONN;
//	socklen_t sz = sizeof(error);
//	if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &sz))
//		error = NET_ENOTCONN;
	return false;
}
typedef struct {
	vfsfile_t funcs;

	SOCKET sock;
	qboolean conpending;
	int readaborted;	//some kind of error. don't spam
	int writeaborted;	//some kind of error. don't spam

	char readbuffer[65536];
	int readbuffered;
	char peer[1];
} tcpfile_t;
int QDECL VFSTCP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	tcpfile_t *tf = (tcpfile_t*)file;
	int len;
	int trying;

	if (tf->conpending)
	{
		trying = VFSTCP_IsStillConnecting(tf->sock);
		if (trying < 0)
		{
			tf->readaborted = trying;
			tf->writeaborted = true;
		}
		else if (trying)
			return 0;
		tf->conpending = false;
	}

	if (!tf->readaborted)
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
			if (e != NET_EWOULDBLOCK && e != NET_EINTR)
			{
				tf->readaborted = VFS_ERROR_UNSPECIFIED;
				switch(e)
				{
				case NET_ENOTCONN:
					Con_Printf("connection to \"%s\" failed\n", tf->peer);
					tf->readaborted = VFS_ERROR_NORESPONSE;
					tf->writeaborted = true;
					break;
				case NET_ECONNABORTED:
					Con_DPrintf("connection to \"%s\" aborted\n", tf->peer);
					tf->readaborted = VFS_ERROR_NORESPONSE;
					tf->writeaborted = true;
					break;
				case NET_ETIMEDOUT:
					Con_Printf("connection to \"%s\" timed out\n", tf->peer);
					tf->readaborted = VFS_ERROR_NORESPONSE;
					tf->writeaborted = true;
					break;
				case NET_ECONNREFUSED:
					Con_DPrintf("connection to \"%s\" refused\n", tf->peer);
					tf->readaborted = VFS_ERROR_REFUSED;
					tf->writeaborted = true;
					break;
				case NET_ECONNRESET:
					Con_DPrintf("connection to \"%s\" reset\n", tf->peer);
					break;
				default:
					Con_Printf("tcp socket error %i (%s)\n", e, tf->peer);
				}
			}
			//fixme: figure out wouldblock or error
		}
		else if (len == 0 && trying != 0)
		{
			//peer disconnected
			tf->readaborted = VFS_ERROR_EOF;
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
		return VFS_ERROR_UNSPECIFIED;	//caller error...

	if (bytestoread > 0)
	{
		memcpy(buffer, tf->readbuffer, bytestoread);
		tf->readbuffered -= bytestoread;
		memmove(tf->readbuffer, tf->readbuffer+bytestoread, tf->readbuffered);
		return bytestoread;
	}
	else return tf->readaborted;
}
int QDECL VFSTCP_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
{
	tcpfile_t *tf = (tcpfile_t*)file;
	int len;

	if (tf->writeaborted)
		return VFS_ERROR_UNSPECIFIED;	//a previous write failed.

	if (tf->conpending)
	{
		len = VFSTCP_IsStillConnecting(tf->sock);
		if (len < 0)
		{
			tf->writeaborted = true;
			tf->conpending = false;
			return len;
		}
		if (len)
			return 0;
		tf->conpending = false;
	}

	len = send(tf->sock, buffer, bytestoread, MSG_NOSIGNAL);
	if (len == -1 || len == 0)
	{
		int reason = VFS_ERROR_UNSPECIFIED;
		int e = (len==0)?NET_ECONNABORTED:neterrno();
		switch(e)
		{
		case NET_EINTR:
		case NET_EWOULDBLOCK:
			return 0;	//nothing available yet.
		case NET_ETIMEDOUT:
			Con_Printf("connection to \"%s\" timed out\n", tf->peer);
			tf->writeaborted = true;
			tf->conpending = false;
			return VFS_ERROR_NORESPONSE;	//don't bother trying to read if we never connected.
		case NET_ECONNREFUSED:	//peer sent a reset instead of accepting a new connection
			Con_DPrintf("connection to \"%s\" refused\n", tf->peer);
			tf->writeaborted = true;
			tf->conpending = false;
			return VFS_ERROR_REFUSED;	//don't bother trying to read if we never connected.
		case NET_ECONNABORTED:	//peer closed its socket
			Con_Printf("connection to \"%s\" aborted\n", tf->peer);
			reason = len?VFS_ERROR_NORESPONSE:VFS_ERROR_EOF;
			break;
		case NET_ECONNRESET:	//'peer' claims no knowledge (rebooted?) or forcefully closed
			Con_DPrintf("connection to \"%s\" reset\n", tf->peer);
			reason = VFS_ERROR_EOF;
			break;
		case NET_ENOTCONN:
#ifdef __unix__
		case EPIPE:
#endif
			Con_Printf("connection to \"%s\" failed\n", tf->peer);
			tf->writeaborted = true;
			tf->conpending = false;
			return VFS_ERROR_NORESPONSE;	//don't bother trying to read if we never connected.
		default:
			Sys_Printf("tcp socket error %i (%s)\n", e, tf->peer);
			break;
		}
//		don't destroy it on write errors, because that prevents us from reading anything that was sent to us afterwards.
//		instead let the read handling kill it if there's nothing new to be read
		VFSTCP_ReadBytes(file, NULL, 0);
		tf->writeaborted = true;
		return reason;
	}
	return len;
}
qboolean QDECL VFSTCP_Seek (struct vfsfile_s *file, qofs_t pos)
{
	return false;
}
static qofs_t QDECL VFSTCP_Tell (struct vfsfile_s *file)
{
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
	if (f->sock != INVALID_SOCKET)
	{
		closesocket(f->sock);
		f->sock = INVALID_SOCKET;
	}
	Z_Free(f);
	return success;
}

vfsfile_t *FS_WrapTCPSocket(SOCKET sock, qboolean conpending, const char *peername)
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
vfsfile_t *FS_OpenTCP(const char *name, int defaultport, qboolean assumetls)
{
	netadr_t adr = {0};
	if (NET_StringToAdr(name, defaultport, &adr))
	{
		qboolean wanttls = (adr.prot == NP_TLS || (adr.prot != NP_STREAM && assumetls));
		vfsfile_t *f;
#ifndef HAVE_SSL
		if (wanttls)
			return NULL;	//don't even make the connection if we can't satisfy it.
#endif
		f = FS_WrapTCPSocket(TCP_OpenStream(&adr, name), true, name);
#ifdef HAVE_SSL
		if (f && wanttls)
			f = FS_OpenSSL(name, f, false);
#endif
		return f;
	}
	else
		return NULL;
}
#else
vfsfile_t *FS_OpenTCP(const char *name, int defaultport, qboolean assumetls)
{
	return NULL;
}
#endif
