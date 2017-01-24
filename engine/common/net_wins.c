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

#ifdef IPPROTO_IPV6
#ifdef _WIN32
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
/*int (*pgetaddrinfo)
(
  const char* nodename,
  const char* servname,
  const struct addrinfo* hints,
  struct addrinfo** res
);
void (*pfreeaddrinfo) (struct addrinfo*);
*/
#endif
#endif

#if defined(HAVE_IPV4) && !defined(CLIENTONLY)
#define HAVE_NATPMP
#endif

void NET_GetLocalAddress (int socket, netadr_t *out);
//int TCP_OpenListenSocket (const char *localip, int port);
#ifdef IPPROTO_IPV6
int UDP6_OpenSocket (int port, qboolean bcast);
#endif
#ifdef USEIPX
void IPX_CloseSocket (int socket);
#endif
cvar_t	net_hybriddualstack = CVARD("net_hybriddualstack", "1", "Uses hybrid ipv4+ipv6 sockets where possible. Not supported on xp or below.");
cvar_t	net_fakeloss	= CVARFD("net_fakeloss", "0", CVAR_CHEAT, "Simulates packetloss in both receiving and sending, on a scale from 0 to 1.");
cvar_t	net_enabled		= CVARD("net_enabled", "1", "If 0, disables all network access, including name resolution and socket creation. Does not affect loopback/internal connections.");

extern cvar_t sv_public, sv_listen_qw, sv_listen_nq, sv_listen_dp, sv_listen_q3;






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
	case NA_BROADCAST_IP:
		memset (s, 0, sizeof(struct sockaddr_in));
		((struct sockaddr_in*)s)->sin_family = AF_INET;

		*(int *)&((struct sockaddr_in*)s)->sin_addr = 0xffffffff;//INADDR_BROADCAST;
		((struct sockaddr_in*)s)->sin_port = a->port;
		return sizeof(struct sockaddr_in);

	case NA_TLSV4:
	case NA_TCP:
	case NA_IP:
		memset (s, 0, sizeof(struct sockaddr_in));
		((struct sockaddr_in*)s)->sin_family = AF_INET;

		*(int *)&((struct sockaddr_in*)s)->sin_addr = *(int *)&a->address.ip;
		((struct sockaddr_in*)s)->sin_port = a->port;
		return sizeof(struct sockaddr_in);
#endif
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
		memset (s, 0, sizeof(struct sockaddr_in));
		((struct sockaddr_in6*)s)->sin6_family = AF_INET6;

		memset((int *)&((struct sockaddr_in6*)s)->sin6_addr, 0, sizeof(*(int *)&((struct sockaddr_in6*)s)->sin6_addr));
		((struct sockaddr_in6*)s)->sin6_addr.s6_addr[0]		= 0xff;
		((struct sockaddr_in6*)s)->sin6_addr.s6_addr[1]		= 0x02;
		((struct sockaddr_in6*)s)->sin6_addr.s6_addr[15]	= 0x01;
		((struct sockaddr_in6*)s)->sin6_port = a->port;
		return sizeof(struct sockaddr_in6);

	case NA_TLSV6:
	case NA_TCPV6:
	case NA_IPV6:
		memset (s, 0, sizeof(struct sockaddr_in6));
		((struct sockaddr_in6*)s)->sin6_family = AF_INET6;

		memcpy(&((struct sockaddr_in6*)s)->sin6_addr, a->address.ip6, sizeof(struct in6_addr));
		((struct sockaddr_in6*)s)->sin6_port = a->port;
		((struct sockaddr_in6 *)s)->sin6_scope_id = a->scopeid;
		return sizeof(struct sockaddr_in6);
#endif
#ifdef USEIPX
	case NA_IPX:
		((struct sockaddr_ipx *)s)->sa_family = AF_IPX;
		memcpy(((struct sockaddr_ipx *)s)->sa_netnum, &a->address.ipx[0], 4);
		memcpy(((struct sockaddr_ipx *)s)->sa_nodenum, &a->address.ipx[4], 6);
		((struct sockaddr_ipx *)s)->sa_socket = a->port;
		return sizeof(struct sockaddr_ipx);
	case NA_BROADCAST_IPX:
		memset (s, 0, sizeof(struct sockaddr_ipx));
		((struct sockaddr_ipx*)s)->sa_family = AF_IPX;
		memset(&((struct sockaddr_ipx*)s)->sa_netnum, 0, 4);
		memset(&((struct sockaddr_ipx*)s)->sa_nodenum, 0xff, 6);
		((struct sockaddr_ipx*)s)->sa_socket = a->port;
		return sizeof(struct sockaddr_ipx);
#endif
	default:
		Sys_Error("NetadrToSockadr: Bad type %i", a->type);
		return 0;
	}
}

void SockadrToNetadr (struct sockaddr_qstorage *s, netadr_t *a)
{
	a->connum = 0;

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
#ifdef IPPROTO_IPV6
	case AF_INET6:
		a->type = NA_IPV6;
		memcpy(&a->address.ip6, &((struct sockaddr_in6 *)s)->sin6_addr, sizeof(a->address.ip6));
		a->port = ((struct sockaddr_in6 *)s)->sin6_port;
		a->scopeid = ((struct sockaddr_in6 *)s)->sin6_scope_id;
		break;
#endif
#ifdef USEIPX
	case AF_IPX:
		a->type = NA_IPX;
		*(int *)a->address.ip = 0xffffffff;
		memcpy(&a->address.ipx[0], ((struct sockaddr_ipx *)s)->sa_netnum, 4);
		memcpy(&a->address.ipx[4], ((struct sockaddr_ipx *)s)->sa_nodenum, 6);
		a->port = ((struct sockaddr_ipx *)s)->sa_socket;
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

qboolean	NET_CompareAdr (netadr_t *a, netadr_t *b)
{
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
	if (a->type == NA_IP || a->type == NA_BROADCAST_IP || a->type == NA_TCP || a->type == NA_TLSV4)
	{
		if ((memcmp(a->address.ip, b->address.ip, sizeof(a->address.ip)) == 0) && a->port == b->port)
			return true;
		return false;
	}
#endif

#ifdef IPPROTO_IPV6
	if (a->type == NA_IPV6 || a->type == NA_BROADCAST_IP6 || a->type == NA_TCPV6 || a->type == NA_TLSV6)
	{
		if ((memcmp(a->address.ip6, b->address.ip6, sizeof(a->address.ip6)) == 0) && a->port == b->port)
			return true;
		return false;
	}
#endif

#ifdef USEIPX
	if (a->type == NA_IPX || a->type == NA_BROADCAST_IPX)
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

	Con_Printf("NET_CompareAdr: Bad address type\n");
	return false;
}
 
/*
===================
NET_CompareBaseAdr

Compares without the port
===================
*/
qboolean	NET_CompareBaseAdr (netadr_t *a, netadr_t *b)
{
	if (a->type != b->type)
		return false;

	if (a->type == NA_LOOPBACK)
		return true;

#ifdef HAVE_IPV4
	if (a->type == NA_IP || a->type == NA_TCP || a->type == NA_TLSV4)
	{
		if ((memcmp(a->address.ip, b->address.ip, sizeof(a->address.ip)) == 0))
			return true;
		return false;
	}
#endif
#ifdef IPPROTO_IPV6
	if (a->type == NA_IPV6 || a->type == NA_TCPV6 || a->type == NA_TLSV6)
	{
		if ((memcmp(a->address.ip6, b->address.ip6, 16) == 0))
			return true;
		return false;
	}
#endif
#ifdef USEIPX
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

	Sys_Error("NET_CompareBaseAdr: Bad address type");
	return false;
}

qboolean NET_AddressSmellsFunny(netadr_t *a)
{
#ifdef IPPROTO_IPV6
	int i;
#endif

	//rejects certain blacklisted addresses
	switch(a->type)
	{
#ifdef HAVE_IPV4
	case NA_BROADCAST_IP:
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

#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
		//reject [::XXXX] (this includes obsolete ipv4-compatible (not ipv4 mapped), and localhost)
		for (i = 0; i < 12; i++)
			if (a->address.ip6[i])
				break;
		if (i == 12)
			return true;
		return false;
#endif

#ifdef USEIPX
	//no idea how this protocol's addresses work
	case NA_BROADCAST_IPX:
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
#ifdef IPPROTO_IPV6
	qboolean doneblank;
#endif

	switch(a->type)
	{
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:
		Q_strncpyz(s, a->address.websocketurl, len);
		break;
#endif
#ifdef TCPCONNECT
	case NA_TLSV4:
	case NA_TCP:
		if (len < 7)
			return "?";
		snprintf (s, len, (a->type == NA_TLSV4)?"tls://":"tcp://");
		s += 6;
		len -= 6;
		//fallthrough
#endif
#ifdef HAVE_IPV4
	case NA_BROADCAST_IP:
	case NA_IP:
		if (a->port)
		{
			snprintf (s, len, "%i.%i.%i.%i:%i",
				a->address.ip[0],
				a->address.ip[1],
				a->address.ip[2],
				a->address.ip[3],
				ntohs(a->port));
		}
		else
		{
			snprintf (s, len, "%i.%i.%i.%i",
				a->address.ip[0],
				a->address.ip[1],
				a->address.ip[2],
				a->address.ip[3]);
		}
		break;
#endif
#ifdef TCPCONNECT
	case NA_TLSV6:
	case NA_TCPV6:
		if (len < 7)
			return "?";
		snprintf (s, len, (a->type == NA_TLSV4)?"tls://":"tcp://");
		s += 6;
		len -= 6;
		//fallthrough
#endif
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
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
					snprintf (s, len, "%i.%i.%i.%i:%i",
						a->address.ip6[12],
						a->address.ip6[13],
						a->address.ip6[14],
						a->address.ip6[15],
						ntohs(a->port));
				else
					snprintf (s, len, "%i.%i.%i.%i",
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
			{
				snprintf (s, len-strlen(s), "[");
				p += strlen(p);
			}

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
#ifdef USEIPX
	case NA_BROADCAST_IPX:
	case NA_IPX:
		snprintf (s, len, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i",
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
		snprintf (s, len, "QLoopBack:%i", a->port);
		break;

#ifdef IRCCONNECT
	case NA_IRC:
		if (*a->address.irc.channel)
			snprintf (s, len, "irc://%s@%s", a->address.irc.user, a->address.irc.channel);
		else
			snprintf (s, len, "irc://%s", a->address.irc.user);
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
	switch(a->type)
	{
	case NA_BROADCAST_IP:
	case NA_IP:
		snprintf (s, len, "%i.%i.%i.%i",
			a->address.ip[0],
			a->address.ip[1],
			a->address.ip[2],
			a->address.ip[3]);
		break;
	case NA_TLSV4:
	case NA_TLSV6:
		snprintf (s, len, "tls://");
		if (len > 6)
		{
			a->type = (a->type-NA_TLSV4)+NA_IP;
			NET_BaseAdrToString(s+6, len-6, a);
			a->type = (a->type-NA_IP)+NA_TLSV4;
		}
		break;
	case NA_TCP:
	case NA_TCPV6:
		snprintf (s, len, "tcp://");
		if (len > 6)
		{
			a->type = (a->type-NA_TCP)+NA_IP;
			NET_BaseAdrToString(s+6, len-6, a);
			a->type = (a->type-NA_IP)+NA_TCP;
		}
		break;
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
		{
			char *p;
			int i, doneblank;
			if (!*(int*)&a->address.ip6[0] && 
				!*(int*)&a->address.ip6[4] &&
				!*(short*)&a->address.ip6[8] &&
				*(short*)&a->address.ip6[10] == (short)0xffff)
			{
				snprintf (s, len, "%i.%i.%i.%i",
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
#ifdef USEIPX
	case NA_BROADCAST_IPX:
	case NA_IPX:
		snprintf (s, len, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x",
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
		snprintf (s, len, "QLoopBack");
		break;

#ifdef IRCCONNECT
	case NA_IRC:
		NET_AdrToString(s, len, a);
		break;
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
size_t NET_StringToSockaddr2 (const char *s, int defaultport, struct sockaddr_qstorage *sadr, int *addrfamily, int *addrsize, size_t addresses)
{
	size_t	result = 0;

	if (!(*s) || !addresses)
		return result;

	memset (sadr, 0, sizeof(*sadr));

#ifdef USEIPX
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
#ifdef IPPROTO_IPV6
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
		udp6hint.ai_family = 0;//Any... we check for AF_INET6 or 4
		udp6hint.ai_socktype = SOCK_DGRAM;
		udp6hint.ai_protocol = IPPROTO_UDP;

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
				if (result < addresses)
					memcpy(&sadr[result++], pos->ai_addr, pos->ai_addrlen);
				break;
#ifdef HAVE_IPV4
			case AF_INET:
				//ipv4 addresses have a higher priority than ipv6 ones.
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
#ifdef HAVE_IPV4
		char	copy[128];
		char	*colon;

		((struct sockaddr_in *)sadr)->sin_family = AF_INET;

		((struct sockaddr_in *)sadr)->sin_port = 0;

		if (strlen(s) >= sizeof(copy)-1)
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
	if (!strncmp (s, "ws://", 5) || !strncmp (s, "wss://", 6))
	{
		a->type = NA_WEBSOCKET;
		Q_strncpyz(a->address.websocketurl, s, sizeof(a->address.websocketurl));
		return true;
	}
	else
	{
		/*code for convienience - no other protocols work anyway*/
		static float warned;
		if (warned < realtime)
		{
			Con_Printf("Note: Assuming ws:// prefix\n");
			warned = realtime + 1;
		}
		a->type = NA_WEBSOCKET;
		memcpy(a->address.websocketurl, "ws://", 5);
		Q_strncpyz(a->address.websocketurl+5, s, sizeof(a->address.websocketurl)-5);
		return true;
	}
#endif
#ifdef TCPCONNECT
	if (!strncmp (s, "tcp://", 6))
	{
		//make sure that the rest of the address is a valid ip address (4 or 6)

		if (!NET_StringToSockaddr (s+6, defaultport, &sadr[0], NULL, NULL))
		{
			a->type = NA_INVALID;
			return false;
		}

		SockadrToNetadr (&sadr[0], a);

		if (a->type == NA_IP)
		{
			a->type = NA_TCP;
			return true;
		}
		if (a->type == NA_IPV6)
		{
			a->type = NA_TCPV6;
			return true;
		}
		return false;
	}
	if (!strncmp (s, "tls://", 6))
	{
		//make sure that the rest of the address is a valid ip address (4 or 6)

		if (!NET_StringToSockaddr (s+6, defaultport, &sadr[0], NULL, NULL))
		{
			a->type = NA_INVALID;
			return false;
		}

		SockadrToNetadr (&sadr[0], a);

		if (a->type == NA_IP)
		{
			a->type = NA_TLSV4;
			return true;
		}
		if (a->type == NA_IPV6)
		{
			a->type = NA_TLSV6;
			return true;
		}
		return false;
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
		return true;
	}
#endif
#ifdef HAVE_NATPMP
	if (!strncmp (s, "natpmp://", 9))
	{
		NET_PortToAdr(NA_NATPMP, s+9, a);
		if (a->type == NA_IP)
			a->type = NA_NATPMP;
		if (a->type != NA_NATPMP)
			return false;
		return true;
	}
#endif

	result = NET_StringToSockaddr2 (s, defaultport, sadr, NULL, NULL, min(numaddresses, sizeof(sadr)/sizeof(sadr[0])));
	for (i = 0; i < result; i++)
	{
		SockadrToNetadr (&sadr[i], &a[i]);

#if !defined(HAVE_PACKET) && defined(HAVE_TCP)
		//bump over protocols that cannot work in the first place.
		if (a[i].type == NA_IP)
			a[i].type = NA_TCP;
		if (a[i].type == NA_IPV6)
			a[i].type = NA_TCPV6;
#endif
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
	case NA_TCP:
	case NA_TLSV4:
	case NA_IP:
	case NA_BROADCAST_IP:
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
	case NA_TCPV6:
	case NA_TLSV6:
	case NA_IPV6:
	case NA_BROADCAST_IP6:
#ifdef IPPROTO_IPV6
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
	case NA_BROADCAST_IPX:
#ifdef USEIPX
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
	// warning: enumeration value âNA_*â not handled in switch
	case NA_NATPMP:
	case NA_WEBSOCKET:
	case NA_IRC:
		break;

	}
}

// ParsePartialIPv4: check string to see if it is a partial IPv4 address and
// return bits to mask and set netadr_t or 0 if not an address
int ParsePartialIPv4(const char *s, netadr_t *a)
{
	const char *colon = NULL;
	char *address = a->address.ip;
	int bits = 8;

	if (!*s)
		return 0;

	memset (a, 0, sizeof(*a));
	while (*s)
	{
		if (*s == ':')
		{
			if (colon) // only 1 colon
				return 0;
			colon = s + 1;
		}
		else if (*s == '.')
		{
			if (colon) // no colons before periods (probably invalid anyway)
				return 0;
			else if (bits >= 32) // only 32 bits in ipv4
				return 0;
			else if (*(s+1) == '.')
				return 0;
			else if (*(s+1) == '\0')
				break; // don't add more bits to the mask for x.x., etc
			bits += 8;
			address++;
		}
		else if (*s >= '0' && *s <= '9')
			*address = ((*address)*10) + (*s-'0');
		else
			return 0; // invalid character

		s++;
	}

	a->type = NA_IP;
	if (colon)
		a->port = atoi(colon);

	return bits;
}

// NET_StringToAdrMasked: extension to NET_StringToAdr to handle IP addresses
// with masks or integers representing the bit masks
qboolean NET_StringToAdrMasked (const char *s, netadr_t *a, netadr_t *amask)
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
		if (!ParsePartialIPv4(t, a) && !NET_StringToAdr(t, 0, a))
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
			return ParsePartialIPv4(spoint, amask) || NET_StringToAdr(spoint, 0, amask);

		// otherwise generate mask for given bits
		i = atoi(spoint);
		NET_IntegerToMask(a, amask, i);
	}
	else
	{
		// we don't have a slash, resolve and fill with a full mask
		i = ParsePartialIPv4(s, a);
		if (!i && !NET_StringToAdr(s, 0, a))
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
	case NA_BROADCAST_IP:
	case NA_IP:
		for (i = 0; i < 4; i++)
		{
			if ((a->address.ip[i] & mask->address.ip[i]) != (b->address.ip[i] & mask->address.ip[i]))
				return false;
		}
		break;
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
		for (i = 0; i < 16; i++)
		{
			if ((a->address.ip6[i] & mask->address.ip6[i]) != (b->address.ip6[i] & mask->address.ip6[i]))
				return false;
		}
		break;
#endif
#ifdef USEIPX
	case NA_BROADCAST_IPX:
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
	case NA_BROADCAST_IP:
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
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
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
#ifdef USEIPX
	case NA_BROADCAST_IPX:
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

/////////////////////////////////////////////
//loopback stuff

#if !defined(CLIENTONLY) && !defined(SERVERONLY)

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
int FTENET_Loop_SetReceiveFDSet(ftenet_generic_connection_t *gcon, fd_set *fdset)
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

		newcon->GetLocalAddresses = FTENET_Loop_GetLocalAddresses;
		newcon->GetPacket = FTENET_Loop_GetPacket;
		newcon->SendPacket = FTENET_Loop_SendPacket;
		newcon->Close = FTENET_Loop_Close;
#ifdef HAVE_PACKET
		newcon->SetReceiveFDSet = FTENET_Loop_SetReceiveFDSet;
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
#if !defined(SERVERONLY) && !defined(CLIENTONLY)
static ftenet_generic_connection_t *FTENET_Loop_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef HAVE_IPV4
static ftenet_generic_connection_t *FTENET_UDP4_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef IPPROTO_IPV6
static ftenet_generic_connection_t *FTENET_UDP6_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef TCPCONNECT
static ftenet_generic_connection_t *FTENET_TCP4Connect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
static ftenet_generic_connection_t *FTENET_TCP6Connect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
static ftenet_generic_connection_t *FTENET_TLS4Connect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
static ftenet_generic_connection_t *FTENET_TLS6Connect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
#endif
#ifdef USEIPX
static ftenet_generic_connection_t *FTENET_IPX_EstablishConnection(qboolean isserver, const char *address, netadr_t adr);
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
		NET_SendPacket(NS_SERVER, 2, &pmpreqmsg, &pmp->pmpaddr);

		//open the firewall/nat.
		pmpreqmsg.op = 1;
		NET_SendPacket(NS_SERVER, sizeof(pmpreqmsg), &pmpreqmsg, &pmp->pmpaddr);

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
	return false;
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

	if (pmpadr.type == NA_NATPMP)
		pmpadr.type = NA_IP;
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
qboolean FTENET_AddToCollection(ftenet_connections_t *col, const char *name, const char *addresslist, netadrtype_t addrtype, qboolean islisten)
{
	netadr_t adr[8];
	ftenet_generic_connection_t *(*establish[countof(adr)])(qboolean isserver, const char *address, netadr_t adr);
	char address[countof(adr)][256];
	unsigned int i, j;
	qboolean success = false;

	if (name && strchr(name, ':'))
		return false;

	for (i = 0; addresslist && *addresslist && i < countof(adr); i++)
	{
		addresslist = COM_ParseStringSet(addresslist, address[i], sizeof(address[i]));
		//resolve the address to something sane so we can determine the address type and thus the connection type to use
		if (!*address[i])
			adr[i].type = NA_INVALID;
		else if (islisten)
			NET_PortToAdr(addrtype, address[i], &adr[i]);
		else
		{
			if (!NET_StringToAdr(address[i], 0, &adr[i]))
				return false;
		}

		switch(adr[i].type)
		{
		default:			establish[i] = NULL;	break;
#ifdef HAVE_NATPMP
		case NA_NATPMP:		establish[i] = FTENET_NATPMP_EstablishConnection; break;
#endif
#if !defined(CLIENTONLY) && !defined(SERVERONLY)
		case NA_LOOPBACK:	establish[i] = FTENET_Loop_EstablishConnection; break;
#endif
#ifdef HAVE_IPV4
		case NA_IP:			establish[i] = FTENET_UDP4_EstablishConnection; break;
#endif
#ifdef IPPROTO_IPV6
		case NA_IPV6:		establish[i] = FTENET_UDP6_EstablishConnection; break;
#endif
#ifdef USEIPX
		case NA_IPX:		establish[i] = FTENET_IPX_EstablishConnection;	break;
#endif
		case NA_WEBSOCKET:
#ifdef HAVE_WEBSOCKCL
			if (!islisten)
				establish[i] = FTENET_WebSocket_EstablishConnection;
#endif
#ifdef TCPCONNECT
			establish[i] = FTENET_TCP4Connect_EstablishConnection;
#endif
			break;
#ifdef IRCCONNECT
		case NA_IRC:		establish[i] = FTENET_IRCConnect_EstablishConnection;	break;
#endif
#ifdef TCPCONNECT
		case NA_TCP:		establish[i] = FTENET_TCP4Connect_EstablishConnection;	break;
		case NA_TLSV4:		establish[i] = FTENET_TLS4Connect_EstablishConnection;	break;
#endif
#if defined(TCPCONNECT) && defined(IPPROTO_IPV6)
		case NA_TCPV6:		establish[i] = FTENET_TCP6Connect_EstablishConnection;	break;
		case NA_TLSV6:		establish[i] = FTENET_TLS6Connect_EstablishConnection;	break;
#endif
		}
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
#ifdef USE_GETHOSTNAME_LOCALLISTING
	char		adrs[MAX_ADR_SIZE];
	int b;
#endif
	int found = 0;

	gethostname(adrs, sizeof(adrs));
#ifdef IPPROTO_IPV6
	if (pgetaddrinfo)
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
				if ((itr->ai_addr->sa_family == AF_INET && ipv4)
					|| (itr->ai_addr->sa_family == AF_INET6 && ipv6)
#ifdef USEIPX
					|| (itr->ai_addr->sa_family == AF_IPX && ipx)
#endif
					)
				if (maxaddresses)
				{
					SockadrToNetadr((struct sockaddr_qstorage*)itr->ai_addr, addresses);
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
	else
#endif
	{
		struct hostent *h;
		h = gethostbyname(adrs);
		b = 0;
#ifdef HAVE_IPV4
		if(h && h->h_addrtype == AF_INET)
		{
			for (b = 0; h->h_addr_list[b] && maxaddresses; b++)
			{
				struct sockaddr_in from;
				from.sin_family = AF_INET;
				from.sin_port = port;
				memcpy(&from.sin_addr, h->h_addr_list[b], sizeof(from.sin_addr));
				SockadrToNetadr((struct sockaddr_qstorage*)&from, addresses);

				*adrflags++ = 0;
				addresses++;
				maxaddresses--;
				found++;
			}
		}
#endif
#ifdef IPPROTO_IPV6
		if(h && h->h_addrtype == AF_INET6)
		{
			for (b = 0; h->h_addr_list[b] && maxaddresses; b++)
			{
				struct sockaddr_in6 from;
				from.sin6_family = AF_INET6;
				from.sin6_port = port;
				memcpy(&from.sin6_addr, h->h_addr_list[b], sizeof(((struct sockaddr_in6*)&from)->sin6_addr));
				SockadrToNetadr((struct sockaddr_qstorage*)&from, addresses);
				*adrflags++ = 0;
				addresses++;
				maxaddresses--;
				found++;
			}
		}
#endif
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
#ifdef IPPROTO_IPV6
			(fam == AF_INET6 && ipv6) ||
#endif
#ifdef USEIPX
			(fam == AF_IPX && ipx) ||
#endif
			0)
		{
			SockadrToNetadr((struct sockaddr_qstorage*)ifa->ifa_addr, &addresses[idx]);
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
		SockadrToNetadr(&from, &adr);

#ifdef USE_GETHOSTNAME_LOCALLISTING
		//if its bound to 'any' address, ask the system what addresses it actually accepts.
		if (adr.type == NA_IPV6 &&
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

qboolean FTENET_Generic_GetPacket(ftenet_generic_connection_t *con)
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
				SockadrToNetadr (&from, &net_from);
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
				if (err == NET_ECONNABORTED)
					Con_TPrintf ("Connection lost or aborted (%s)\n", NET_AdrToString (adr, sizeof(adr), &net_from));	//server died/connection lost.
				else
					Con_TPrintf ("Connection lost or aborted\n");	//server died/connection lost.
				resettime = curtime;
#ifndef SERVERONLY
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


		Con_Printf ("NET_GetPacket: Error (%i): %s\n", err, strerror(err));
		return false;
	}
	SockadrToNetadr (&from, &net_from);

	net_message.packing = SZ_RAWBYTES;
	net_message.currentbit = 0;
	net_message.cursize = ret;
	if (net_message.cursize == sizeof(net_message_buffer) )
	{
		Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &net_from));
		return false;
	}

	return true;
#endif
}

neterr_t FTENET_Generic_SendPacket(ftenet_generic_connection_t *con, int length, const void *data, netadr_t *to)
{
#ifndef HAVE_PACKET
	return NETERR_DISCONNECTED;
#else
	struct sockaddr_qstorage	addr;
	int size;
	int ret;

	for (size = 0; size < FTENET_ADDRTYPES; size++)
		if (to->type == con->addrtype[size])
			break;
	if (size == FTENET_ADDRTYPES)
		return NETERR_NOROUTE;

#ifdef IPPROTO_IPV6
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
		NetadrToSockadr (to, &addr);

		switch(to->type)
		{
		default:
			Con_Printf("Bad address type\n");
			break;
#ifdef USEIPX	//who uses ipx nowadays anyway?
		case NA_BROADCAST_IPX:
		case NA_IPX:
			size = sizeof(struct sockaddr_ipx);
			break;
#endif
		case NA_BROADCAST_IP:
		case NA_IP:
			size = sizeof(struct sockaddr_in);
			break;
#ifdef IPPROTO_IPV6
		case NA_BROADCAST_IP6:
		case NA_IPV6:
			size = sizeof(struct sockaddr_in6);
			break;
#endif
	}
	}

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

		if (ecode == NET_EACCES)
		{
			Con_Printf("Access denied: check firewall\n");
			return NETERR_DISCONNECTED;
		}

#ifndef SERVERONLY
		if (ecode == NET_EADDRNOTAVAIL)
			Con_DPrintf("NET_SendPacket Warning: %i\n", ecode);
		else
#endif
#ifdef _WIN32
			Con_TPrintf ("NET_SendPacket ERROR: %i\n", ecode);
#else
			Con_TPrintf ("NET_SendPacket ERROR: %s\n", strerror(ecode));
#endif
	}
	else if (ret < length)
		return NETERR_MTU;
	return NETERR_SENT;
#endif
}

qboolean	NET_PortToAdr (int adrfamily, const char *s, netadr_t *a)
{
	char *e;
	if (net_enabled.ival || adrfamily == NA_LOOPBACK)
	{
		int port;
		if (!strncmp(s, "natpmp:", 7))
			return NET_StringToAdr2(s, 0, a, 1);

		port = strtoul(s, &e, 10);
		if (*e)	//if *e then its not just a single number in there, so treat it as a proper address.
			return NET_StringToAdr(s, 0, a);
		else if (e != s)	//if we actually read something (even a 0)
		{
			memset(a, 0, sizeof(*a));
			a->port = htons((unsigned short)port);
			a->type = adrfamily;

			return a->type != NA_INVALID;
		}
	}
	a->type = NA_INVALID;
	return false;
}

/*just here to prevent the client from spamming new sockets, which can be a problem with certain q2 servers*/
qboolean FTENET_Generic_ChangeLocalAddress(struct ftenet_generic_connection_s *con, netadr_t *adr)
{
	if (adr->type == con->addrtype[0] || adr->type == con->addrtype[1])
		if (adr->port == 0)
			return true;	//they want to use addr_any. it doesn't matter one jot which port we're currently listening on then.
	return false;
}

ftenet_generic_connection_t *FTENET_Generic_EstablishConnection(int adrfamily, int protocol, qboolean isserver, const char *address, netadr_t adr)
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
	int bufsz;
	qboolean hybrid = false;

	if (adr.type != adrfamily)
	{
		if (adr.type == NA_INVALID)
			Con_Printf("unable to resolve local address %s\n", address);
		return NULL;	//couldn't resolve the name
	}
	temp = NetadrToSockadr(&adr, &qs);
	family = ((struct sockaddr*)&qs)->sa_family;

#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
	if (isserver && family == AF_INET && net_hybriddualstack.ival && !((struct sockaddr_in*)&qs)->sin_addr.s_addr)
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

#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
	if (family == AF_INET6)
		setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_true, sizeof(_true));
#endif

#if defined(_WIN32) && defined(SO_EXCLUSIVEADDRUSE)
	//win32 is so fucked up
	setsockopt(newsocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&_true, sizeof(_true));
#endif

	bufsz = 1<<18;
	setsockopt(newsocket, SOL_SOCKET, SO_RCVBUF, (void*)&bufsz, sizeof(bufsz));

	//try and find an unused port.
	port = ntohs(((struct sockaddr_in*)&qs)->sin_port);
	for (bindtries = 100; bindtries > 0; bindtries--)
	{
		((struct sockaddr_in*)&qs)->sin_port = htons((unsigned short)(port+100-bindtries));
		if ((bind(newsocket, (struct sockaddr *)&qs, temp) == INVALID_SOCKET))
		{
			continue;
		}
		break;
	}
	if (!bindtries)
	{
		SockadrToNetadr(&qs, &adr);
		//mneh, reuse qs.
		NET_AdrToString((char*)&qs, sizeof(qs), &adr);
		Con_Printf("Unable to listen at %s\n", (char*)&qs);
		closesocket(newsocket);
		return NULL;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(neterrno()));

	//ipv6 sockets need to add themselves to a multicast group, so that we can receive broadcasts on a lan
#if defined(IPPROTO_IPV6)
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
		newcon->GetPacket = FTENET_Generic_GetPacket;
		newcon->SendPacket = FTENET_Generic_SendPacket;
		newcon->Close = FTENET_Generic_Close;
		newcon->ChangeLocalAddress = FTENET_Generic_ChangeLocalAddress;

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

#ifdef IPPROTO_IPV6
ftenet_generic_connection_t *FTENET_UDP6_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	return FTENET_Generic_EstablishConnection(NA_IPV6, IPPROTO_UDP, isserver, address, adr);
}
#endif
#ifdef HAVE_IPV4
ftenet_generic_connection_t *FTENET_UDP4_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	return FTENET_Generic_EstablishConnection(NA_IP, IPPROTO_UDP, isserver, address, adr);
}
#endif
#ifdef USEIPX
ftenet_generic_connection_t *FTENET_IPX_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	return FTENET_Generic_EstablishConnection(NA_IPX, NSPROTO_IPX, isserver, address, adr);
}
#endif

#ifdef TCPCONNECT
typedef struct ftenet_tcpconnect_stream_s {
	vfsfile_t *clientstream;
	int inlen;
	int outlen;

	enum
	{
		TCPC_UNKNOWN,		//waiting to see what they send us.
		TCPC_UNFRAMED,		//something else is doing the framing (ie: we're running in emscripten and over some hidden websocket connection)
		TCPC_HTTPCLIENT,	//we're sending a file to this victim.
		TCPC_QIZMO,			//'qizmo\n' handshake, followed by packets prefixed with a 16bit packet length.
		TCPC_WEBSOCKETU,	//utf-8 encoded data.
		TCPC_WEBSOCKETB,	//binary encoded data (subprotocol = 'binary')
		TCPC_WEBSOCKETNQ,	//raw nq msg buffers with no encapsulation or handshake
	} clienttype;
	char inbuffer[3000];
	char outbuffer[3000];
	vfsfile_t *dlfile;		//if the client looked like an http client, this is the file that they're downloading.
	float timeouttime;
	netadr_t remoteaddr;
	struct ftenet_tcpconnect_stream_s *next;

	SOCKET socketnum;	//for select.

	int fakesequence;	//TCPC_WEBSOCKETNQ
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

#include "fs.h"
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
			Con_Printf ("tcp peer %s timed out\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
			goto closesvstream;
		}

		ret = VFS_READ(st->clientstream, st->inbuffer+st->inlen, sizeof(st->inbuffer)-st->inlen);
		if (ret < 0)
		{
			Con_Printf ("tcp peer %s closed connection\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
closesvstream:
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

			if (!strncmp(st->inbuffer, "qizmo\n", 6))
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
			else if (con->generic.islisten && !strncmp(st->inbuffer, "GET ", 4))
			{
				int i, j;
				int attr = 0;
				int alen = 0;
				qboolean headerscomplete = false;
				enum
				{
					WCATTR_METHOD,
					WCATTR_URL,
					WCATTR_HTTP,
					WCATTR_HOST,
					WCATTR_UPGRADE,
					WCATTR_CONNECTION,
					WCATTR_WSKEY,
					WCATTR_WSVER,
					//WCATTR_ORIGIN,
					WCATTR_WSPROTO,
					//WCATTR_WSEXT,
					WCATTR_COUNT
				};
				char arg[WCATTR_COUNT][64];
				for (i = 0; i < WCATTR_COUNT; i++)
					arg[i][0] = 0;
				for (i = 0; i < st->inlen; i++)
				{
					if (alen == 63)
						goto handshakeerror;
					if (st->inbuffer[i] == ' ' || st->inbuffer[i] == '\t')
					{
						arg[attr][alen++] = 0;
						alen=0;
						if (attr++ == WCATTR_HTTP)
							break;

						for (; i < st->inlen && (st->inbuffer[i] == ' ' || st->inbuffer[i] == '\t'); i++)
							;
						if (i == st->inlen)
							break;
					}
					arg[attr][alen++] = st->inbuffer[i];
					if (st->inbuffer[i] == '\n')
					{
						arg[attr][alen++] = 0;
						alen=0;
						break;
					}
				}
				i++;
				attr = 0;
				j = i;
				for (; i < st->inlen; i++)
				{
					if ((i+1 < st->inlen && st->inbuffer[i] == '\r' && st->inbuffer[i+1] == '\n') ||
						(i < st->inlen && st->inbuffer[i] == '\n'))
					{
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
					if (!strnicmp(&st->inbuffer[i], "Host", j-i))
						attr = WCATTR_HOST;
					else if (!strnicmp(&st->inbuffer[i], "Upgrade", j-i))
						attr = WCATTR_UPGRADE;
					else if (!strnicmp(&st->inbuffer[i], "Connection", j-i))
						attr = WCATTR_CONNECTION;
					else if (!strnicmp(&st->inbuffer[i], "Sec-WebSocket-Key", j-i))
						attr = WCATTR_WSKEY;
					else if (!strnicmp(&st->inbuffer[i], "Sec-WebSocket-Version", j-i))
						attr = WCATTR_WSVER;
//					else if (!strnicmp(&st->inbuffer[i], "Origin", j-i))
//						attr = WCATTR_ORIGIN;
					else if (!strnicmp(&st->inbuffer[i], "Sec-WebSocket-Protocol", j-i))
						attr = WCATTR_WSPROTO;
//					else if (!strnicmp(&st->inbuffer[i], "Sec-WebSocket-Extensions", j-i))
//						attr = WCATTR_WSEXT;
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

						for (; i < st->inlen && st->inbuffer[i] != '\n'; i++)
							;
						if (i > j && st->inbuffer[i-1] == '\r')
							i--;
						if (attr)
							Q_strncpyz(arg[attr], &st->inbuffer[j], (i-j > 63)?64:(i - j + 1));
						if (i < st->inlen && st->inbuffer[i] == '\r')
							i++;
					}
					else
					{
						/*just a word on the line on its own*/
						goto handshakeerror;
					}
				}

				if (headerscomplete)
				{
					char *resp;
					//must be a Host, Upgrade=websocket, Connection=Upgrade, Sec-WebSocket-Key=base64(randbytes(16)), Sec-WebSocket-Version=13
					//optionally will be Origin=url, Sec-WebSocket-Protocol=FTEWebSocket, Sec-WebSocket-Extensions
					//other fields will be ignored.

					if (!stricmp(arg[WCATTR_UPGRADE], "websocket") && (!stricmp(arg[WCATTR_CONNECTION], "Upgrade") || !stricmp(arg[WCATTR_CONNECTION], "keep-alive, Upgrade")))
					{
						if (atoi(arg[WCATTR_WSVER]) != 13)
						{
							Con_Printf("Outdated websocket request for \"%s\" from \"%s\". got version %i, expected version 13\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), atoi(arg[WCATTR_WSVER]));

							memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
							st->inlen -= i;
							resp = va(	"HTTP/1.1 426 Upgrade Required\r\n"
										"Sec-WebSocket-Version: 13\r\n"
										"\r\n");
							//send the websocket handshake rejection.
							VFS_WRITE(st->clientstream, resp, strlen(resp));

							goto closesvstream;
						}
						else
						{
							char acceptkey[20*2];
							unsigned char sha1digest[20];
							char *blurgh;
							char *protoname = "";
							memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
							st->inlen -= i;

							blurgh = va("%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", arg[WCATTR_WSKEY]);
							tobase64(acceptkey, sizeof(acceptkey), sha1digest, SHA1(sha1digest, sizeof(sha1digest), blurgh, strlen(blurgh)));

							Con_Printf("Websocket request for %s from %s (%s)\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr), arg[WCATTR_WSPROTO]);

							if (!strcmp(arg[WCATTR_WSPROTO], "quake"))
							{
								st->clienttype = TCPC_WEBSOCKETNQ;	//emscripten doesn't give us a choice, but its compact.
								protoname = "Sec-WebSocket-Protocol: quake\r\n";
							}
							else if (!strcmp(arg[WCATTR_WSPROTO], "binary"))
							{
								st->clienttype = TCPC_WEBSOCKETB;	//emscripten doesn't give us a choice, but its compact.
								protoname = "Sec-WebSocket-Protocol: binary\r\n";	//emscripten is a bit limited
							}
							else
							{
								st->clienttype = TCPC_WEBSOCKETU;	//nacl supports only utf-8 encoded data, at least at the time I implemented it.
								protoname = va("Sec-WebSocket-Protocol: %s\r\n", arg[WCATTR_WSPROTO]);	//emscripten is a bit limited
							}

							resp = va(	"HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
										"Upgrade: websocket\r\n"
										"Connection: Upgrade\r\n"
										"Access-Control-Allow-Origin: *\r\n"	//allow cross-origin requests. this means you can use any domain to play on any public server.
										"Sec-WebSocket-Accept: %s\r\n"
										"%s"
										"\r\n", acceptkey, protoname);
							//send the websocket handshake response.
							VFS_WRITE(st->clientstream, resp, strlen(resp));

							//and the connection is okay

							if (st->clienttype == TCPC_WEBSOCKETNQ)
							{
								//hide a connection request in there...
								net_message.cursize = 0;
								net_message.packing = SZ_RAWBYTES;
								net_message.currentbit = 0;
								net_from = st->remoteaddr;
								MSG_WriteLong(&net_message, LongSwap(NETFLAG_CTL | (strlen(NQ_NETCHAN_GAMENAME)+7)));
								MSG_WriteByte(&net_message, CCREQ_CONNECT);
								MSG_WriteString(&net_message, NQ_NETCHAN_GAMENAME);
								MSG_WriteByte(&net_message, NQ_NETCHAN_VERSION);
								return true;
							}
						}
					}
					else
					{
						memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
						st->inlen -= i;
						if (!strcmp(arg[WCATTR_URL], "/live.html"))
						{
							resp = va(	"HTTP/1.1 200 Ok\r\n"
								"Connection: Close\r\n"
								"Content-Type: text/html\r\n"
								"\r\n"
								"<!DOCTYPE HTML>"
								"<html>"
								"<style>"
								"html, body { height: 100%%; width: 100%%; margin: 0; padding: 0;}"
								"div { height: 100%%; width: 100%%; }"
								"</style>"
								"<div>"
								"<object	name=\"ieplug\" type=\"application/x-fteplugin\" classid=\"clsid:7d676c9f-fb84-40b6-b3ff-e10831557eeb\" width=\"100%%\" height=\"100%%\">"
									"<param name=\"game\" value=\"q1\">"
									"<object	name=\"npplug\" type=\"application/x-fteplugin\" width=\"100%%\" height=\"100%%\">"
										"<param name=\"game\" value=\"q1\">"
										"Please install a plugin first.<br/>"
									"</object>"
								"</object>"
								"</div>"
								"</html>"
								);
						}
/*						else if ((!strcmp(arg[WCATTR_URL], "/ftewebgl.html") || !strcmp(arg[WCATTR_URL], "/ftewebgl.html.fmf") || !strcmp(arg[WCATTR_URL], "/pak0.pak")) && ((st->file = VFSOS_Open(va("C:/Incoming/vm%s", arg[WCATTR_URL]), "rb"))))
						{
							Con_Printf("Downloading %s to %s\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
							resp = va(	"HTTP/1.1 200 Ok\r\n"
										"Content-Type: text/html\r\n"
										"Content-Length: %i\r\n"
										"\r\n",
										VFS_GETLEN(st->file)
										);
							send(st->socketnum, resp, strlen(resp), 0);
							st->clienttype = TCPC_HTTPCLIENT;
							continue;
						}
*/
						else
						{
							Con_Printf("Invalid download request %s to %s\n", arg[WCATTR_URL], NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
							resp = va(	"HTTP/1.1 404 Ok\r\n"
										"Connection: Close\r\n"
										"Content-Type: text/html\r\n"
										"\r\n"

										"This is a Quake WebSocket server, not an http server.<br/>\r\n"
										"<a href='"ENGINEWEBSITE"'>"FULLENGINENAME"</a>"
										);
						}

						//send the websocket handshake rejection.
						VFS_WRITE(st->clientstream, resp, strlen(resp));

						goto closesvstream;
					}
				}
			}
			else
			{
handshakeerror:
				Con_Printf ("Unknown TCP handshake from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
				goto closesvstream;
			}

			break;
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
				st->outlen = VFS_READ(st->dlfile, st->outbuffer, sizeof(st->outbuffer));
				if (st->outlen <= 0)
				{
					VFS_CLOSE(st->dlfile);
					st->dlfile = NULL;
					st->clienttype = TCPC_UNKNOWN;
					Con_Printf ("Outgoing file transfer complete\n");
				}
			}
			continue;
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
		case TCPC_UNFRAMED:
			if (!st->inlen)
				continue;
			net_message.cursize = st->inlen;
			memcpy(net_message_buffer, st->inbuffer, net_message.cursize);
			st->inlen = 0;
			
			net_message.packing = SZ_RAWBYTES;
			net_message.currentbit = 0;
			net_from = st->remoteaddr;
			return true;
		case TCPC_WEBSOCKETU:
		case TCPC_WEBSOCKETB:
		case TCPC_WEBSOCKETNQ:
			while (st->inlen >= 2)
			{
				unsigned short ctrl = ((unsigned char*)st->inbuffer)[0]<<8 | ((unsigned char*)st->inbuffer)[1];
				unsigned long paylen;
				unsigned int payoffs = 2;
				unsigned int mask = 0;
				st->inbuffer[st->inlen]=0;
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
						if (ullpaylen > 0x10000)
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
					break;

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
					Con_Printf ("websocket binary frame from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					net_message.cursize = paylen;
					if (net_message.cursize+8 >= sizeof(net_message_buffer) )
					{
						Con_TPrintf ("Warning:  Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &net_from));
						goto closesvstream;
					}
					if (st->clienttype == TCPC_WEBSOCKETNQ)
					{
						payoffs+=1;
						paylen-=1;
						memcpy(net_message_buffer+8, st->inbuffer+payoffs, paylen);
						net_message.cursize=paylen+8;
						((int*)net_message_buffer)[0] = BigLong(NETFLAG_UNRELIABLE | net_message.cursize);
						((int*)net_message_buffer)[1] = LongSwap(++st->fakesequence);
					}
					else
						memcpy(net_message_buffer, st->inbuffer+payoffs, paylen);
					break;
				case 0x8:	/*connection close*/
					Con_Printf ("websocket closure %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					goto closesvstream;
				case 0x9:	/*ping*/
					Con_Printf ("websocket ping from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					if (FTENET_TCPConnect_WebSocket_Splurge(st, 0xa, st->inbuffer+payoffs, paylen) != NETERR_SENT)
						goto closesvstream;
					break;
				case 0xa: /*pong*/
					Con_Printf ("websocket pong from %s\n", NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					goto closesvstream;
				default:
					Con_Printf ("Unsupported websocket opcode (%i) from %s\n", (ctrl>>8) & 0xf, NET_AdrToString (adr, sizeof(adr), &st->remoteaddr));
					goto closesvstream;
				}

//				memcpy(net_message_buffer, st->inbuffer+2, net_message.cursize);
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
			SockadrToNetadr(&from, &st->remoteaddr);
			st->clienttype = TCPC_UNKNOWN;
			st->next = con->tcpstreams;
			con->tcpstreams = st;
			st->socketnum = newsock;
			st->clientstream = FS_OpenTCPSocket(newsock, false, NET_AdrToString(tmpbuf, sizeof(tmpbuf), &st->remoteaddr));
			st->inlen = 0;

#ifdef HAVE_SSL
			if (con->tls && st->clientstream)	//if we're meant to be using tls, wrap the stream in a tls connection
			{
				st->clientstream = FS_OpenSSL(NULL, st->clientstream, true, false);
				/*sockadr doesn't contain transport info, so fix that up here*/
				if (st->remoteaddr.type == NA_IP)
					st->remoteaddr.type = NA_TLSV4;
				else if (st->remoteaddr.type == NA_IPV6)
					st->remoteaddr.type = NA_TLSV6;
			}
			else
#endif
			{
				/*sockadr doesn't contain transport info, so fix that up here*/
				if (st->remoteaddr.type == NA_IP)
					st->remoteaddr.type = NA_TCP;
				else if (st->remoteaddr.type == NA_IPV6)
					st->remoteaddr.type = NA_TCPV6;
			}

			st->timeouttime = timeval + 30;
		}
	}
	return false;
}

neterr_t FTENET_TCPConnect_SendPacket(ftenet_generic_connection_t *gcon, int length, const void *data, netadr_t *to)
{
	neterr_t e;
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
				case TCPC_UNFRAMED:
					if (length > sizeof(st->outbuffer))
					{
						if (length > 0xffff)
							return NETERR_MTU;
						Con_DPrintf("FTENET_TCPConnect_SendPacket: outgoing overflow\n");
					}
					memcpy(st->outbuffer, data, length);
					st->outlen = length;
					break;
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
					e = FTENET_TCPConnect_WebSocket_Splurge(st, (st->clienttype==TCPC_WEBSOCKETU)?1:2, data, length);
					if (e != NETERR_SENT)
						return e;
					break;
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

void FTENET_TCPConnect_Close(ftenet_generic_connection_t *gcon)
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
int FTENET_TCPConnect_SetReceiveFDSet(ftenet_generic_connection_t *gcon, fd_set *fdset)
{
	int maxfd = 0;
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	ftenet_tcpconnect_stream_t *st;

	for (st = con->tcpstreams; st; st = st->next)
	{
		if (st->clientstream == NULL || st->socketnum == INVALID_SOCKET)
			continue;
		FD_SET(st->socketnum, fdset); // network socket
		if (maxfd < st->socketnum)
			maxfd = st->socketnum;
	}
	if (con->generic.thesocket != INVALID_SOCKET)
	{
		FD_SET(con->generic.thesocket, fdset); // network socket
		if (maxfd < con->generic.thesocket)
			maxfd = con->generic.thesocket;
	}
	return maxfd;
}
#endif

ftenet_generic_connection_t *FTENET_TCPConnect_EstablishConnection(int affamily, qboolean isserver, const char *address)
{
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_tcpconnect_connection_t *newcon;

	unsigned long _true = true;
	int newsocket;
	int temp;
	netadr_t adr;
	struct sockaddr_qstorage qs;
	int family;

#ifndef HAVE_SSL
	if ((affamily == NA_TLSV4 || affamily == NA_TLSV6))
	{
		Con_Printf("tls not supported in this build\n");
		return NULL;
	}
#endif

	if (isserver)
	{
#ifndef HAVE_PACKET
		//unable to listen on tcp if we have no packet interface
		return NULL;
#else

		if (!strncmp(address, "tls://", 6))
			address += 6;
		if (!strncmp(address, "tcp://", 6))
			address += 6;
		if (!strncmp(address, "ws://", 5))
			address += 5;
		if (!strncmp(address, "wss://", 6))
			address += 6;

		if (!NET_PortToAdr(affamily, address, &adr))
			return NULL;	//couldn't resolve the name
		if (adr.type == NA_IP)
			adr.type = NA_TCP;
		else if (adr.type == NA_IPV6)
			adr.type = NA_TCPV6;
		temp = NetadrToSockadr(&adr, &qs);
		family = ((struct sockaddr_in*)&qs)->sin_family;

		if ((newsocket = socket (family, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		{
			Con_Printf("operating system doesn't support that\n");
			return NULL;
		}

		if ((bind(newsocket, (struct sockaddr *)&qs, temp) == INVALID_SOCKET) ||
			(listen(newsocket, 2) == INVALID_SOCKET))
		{
			SockadrToNetadr(&qs, &adr);
			//mneh, reuse qs.
			NET_AdrToString((char*)&qs, sizeof(qs), &adr);
			Con_Printf("Unable to listen at %s\n", (char*)&qs);
			closesocket(newsocket);
			return NULL;
		}

		if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
			Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(neterrno()));
#endif
	}
	else
	{
		if (!NET_StringToAdr(address, 0, &adr))
			return NULL;	//couldn't resolve the name

		if (adr.type == NA_IP)
			adr.type = NA_TCP;
		else if (adr.type == NA_IPV6)
			adr.type = NA_TCPV6;
		newsocket = TCP_OpenStream(&adr);
		if (newsocket == INVALID_SOCKET)
			return NULL;
	}

	//this isn't fatal
	setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));

	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		newcon->tls = (affamily == NA_TLSV4 || affamily == NA_TLSV6);
		if (isserver)
			newcon->generic.GetLocalAddresses = FTENET_Generic_GetLocalAddresses;
		newcon->generic.GetPacket = FTENET_TCPConnect_GetPacket;
		newcon->generic.SendPacket = FTENET_TCPConnect_SendPacket;
		newcon->generic.Close = FTENET_TCPConnect_Close;
#ifdef HAVE_PACKET
		newcon->generic.SetReceiveFDSet = FTENET_TCPConnect_SetReceiveFDSet;
#endif

		newcon->generic.islisten = isserver;
		newcon->generic.addrtype[0] = adr.type;
		newcon->generic.addrtype[1] = NA_INVALID;

		newcon->active = 0;

		if (!isserver)
		{
			newcon->generic.thesocket = INVALID_SOCKET;

			newcon->active++;
			newcon->tcpstreams = Z_Malloc(sizeof(*newcon->tcpstreams));
			newcon->tcpstreams->next = NULL;
			newcon->tcpstreams->socketnum = newsocket;
			newcon->tcpstreams->clientstream = FS_OpenTCPSocket(newsocket, true, address);
			newcon->tcpstreams->inlen = 0;

			newcon->tcpstreams->remoteaddr = adr;

#ifdef HAVE_SSL
			if (newcon->tls)	//if we're meant to be using tls, wrap the stream in a tls connection
				newcon->tcpstreams->clientstream = FS_OpenSSL(address, newcon->tcpstreams->clientstream, false, false);
#endif

#ifdef FTE_TARGET_WEB
			newcon->tcpstreams->clienttype = TCPC_UNFRAMED;
#else
			//send the qizmo greeting.
			newcon->tcpstreams->clienttype = TCPC_UNKNOWN;
			VFS_WRITE(newcon->tcpstreams->clientstream, "qizmo\n", 6);
#endif

			newcon->tcpstreams->timeouttime = Sys_DoubleTime() + 30;
		}
		else
		{
			newcon->tcpstreams = NULL;
			newcon->generic.thesocket = newsocket;
		}

		return &newcon->generic;
	}
	else
	{
		closesocket(newsocket);
		return NULL;
	}
}

#ifdef IPPROTO_IPV6
ftenet_generic_connection_t *FTENET_TCP6Connect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	return FTENET_TCPConnect_EstablishConnection(NA_TCPV6, isserver, address);
}
ftenet_generic_connection_t *FTENET_TLS6Connect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	return FTENET_TCPConnect_EstablishConnection(NA_TLSV6, isserver, address);
}
#endif

ftenet_generic_connection_t *FTENET_TCP4Connect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	return FTENET_TCPConnect_EstablishConnection(NA_TCP, isserver, address);
}
ftenet_generic_connection_t *FTENET_TLS4Connect_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	return FTENET_TCPConnect_EstablishConnection(NA_TLSV4, isserver, address);
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
	int sock;
	netadr_t remoteadr;
	qboolean failed;
} ftenet_websocket_connection_t;

static void FTENET_WebSocket_Close(ftenet_generic_connection_t *gcon)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	emscriptenfte_ws_close(wsc->sock);
}
static qboolean FTENET_WebSocket_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	net_message.cursize = emscriptenfte_ws_recv(wsc->sock, net_message_buffer, sizeof(net_message_buffer));
	if (net_message.cursize > 0)
	{
		net_from = wsc->remoteadr;
		return true;
	}
	net_message.cursize = 0;//just incase
	return false;
}
static neterr_t FTENET_WebSocket_SendPacket(ftenet_generic_connection_t *gcon, int length, const void *data, netadr_t *to)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	if (NET_CompareAdr(to, &wsc->remoteadr))
	{
		if (emscriptenfte_ws_send(wsc->sock, data, length) < 0)
			return NETERR_CLOGGED;
		return NETERR_SENT;
	}
	return NETERR_NOROUTE;
}


static ftenet_generic_connection_t *FTENET_WebSocket_EstablishConnection(qboolean isserver, const char *address, netadr_t adr)
{
	ftenet_websocket_connection_t *newcon;

	int newsocket;

	if (isserver)
	{
		return NULL;
	}
	newsocket = emscriptenfte_ws_connect(address);
	if (newsocket < 0)
		return NULL;
	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		Q_strncpyz(newcon->generic.name, "WebSocket", sizeof(newcon->generic.name));
		newcon->generic.GetPacket = FTENET_WebSocket_GetPacket;
		newcon->generic.SendPacket = FTENET_WebSocket_SendPacket;
		newcon->generic.Close = FTENET_WebSocket_Close;

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

#if 0
	struct PP_Var str = ppb_vararraybuffer_interface->Create(length);
	void *out = ppb_vararraybuffer_interface->Map(wsc->incomingpacket);
	if (!out)
		return NETERR_MTU;
	memcpy(out, data, length);
	ppb_vararraybuffer_interface->Unmap(wsc->incomingpacket);
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
		struct PP_CompletionCallback ccb = {websocketconnected, newcon, PP_COMPLETIONCALLBACK_FLAG_NONE};
		newsocket = ppb_websocket_interface->Create(pp_instance);
		struct PP_Var str = ppb_var_interface->VarFromUtf8(adr.address.websocketurl, strlen(adr.address.websocketurl));
		ppb_websocket_interface->Connect(newsocket, str, NULL, 0, ccb);
		ppb_var_interface->Release(str);
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

#ifndef SERVERONLY
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
int NET_GetPacket (netsrc_t netsrc, int firstsock)
{
	ftenet_connections_t *collection;
	unsigned int ctime;
	if (netsrc == NS_SERVER)
	{
#ifdef CLIENTONLY
		Sys_Error("NET_GetPacket: Bad netsrc");
		collection = NULL;
#else
		collection = svs.sockets;
#endif
	}
	else
	{
#ifdef SERVERONLY
		Sys_Error("NET_GetPacket: Bad netsrc");
		collection = NULL;
#else
		collection = cls.sockets;
#endif
	}

	if (!collection)
		return -1;

	while (firstsock < MAX_CONNECTIONS)
	{
		if (!collection->conn[firstsock])
			break;
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

neterr_t NET_SendPacket (netsrc_t netsrc, int length, const void *data, netadr_t *to)
{
	neterr_t err;
//	char buffer[64];
	ftenet_connections_t *collection;
	int i;

	if (netsrc == NS_SERVER)
	{
#ifdef CLIENTONLY
		Sys_Error("NET_GetPacket: Bad netsrc");
		return NETERR_NOROUTE;
#else
		collection = svs.sockets;
#endif
	}
	else
	{
#ifdef SERVERONLY
		Sys_Error("NET_GetPacket: Bad netsrc");
		return NETERR_NOROUTE;
#else
		collection = cls.sockets;
#endif
	}

	if (!collection)
		return NETERR_NOROUTE;

	if (net_fakeloss.value)
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

qboolean NET_EnsureRoute(ftenet_connections_t *collection, char *routename, char *host, qboolean islisten)
{
	netadr_t adr;

	if (NET_StringToAdr(host, 0, &adr))
	{
		switch(adr.type)
		{
		case NA_WEBSOCKET:
		case NA_TLSV4:
		case NA_TLSV6:
		case NA_TCP:
		case NA_TCPV6:
		case NA_IRC:
			if (!FTENET_AddToCollection(collection, routename, host, adr.type, islisten))
				return false;
			Con_Printf("Establishing connection to %s\n", host);
			break;
		default:
			//not recognised, or not needed
			break;
		}
	}
	return true;
}

int NET_EnumerateAddresses(ftenet_connections_t *collection, struct ftenet_generic_connection_s **con, int *adrflags, netadr_t *addresses, int maxaddresses)
{
	unsigned int found = 0, c, i, j;
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			continue;

		c = collection->conn[i]->GetLocalAddresses(collection->conn[i], adrflags, addresses, maxaddresses);

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
	else if (adr->type == NA_IPV6 || adr->type == NA_BROADCAST_IP6 || adr->type == NA_TCPV6 || adr->type == NA_TLSV6)
	{
		if ((*(int*)adr->address.ip6&BigLong(0xffc00000)) == BigLong(0xfe800000))	//fe80::/10
			scope = ASCOPE_LAN, desc = "link-local";
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
	else if (adr->type == NA_IP || adr->type == NA_BROADCAST_IP || adr->type == NA_TCP || adr->type == NA_TLSV4)
	{
		if ((*(int*)adr->address.ip&BigLong(0xffff0000)) == BigLong(0xA9FE0000))	//169.254.x.x/16
			scope = ASCOPE_LAN, desc = "link-local";
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

	if (!collection)
		return;

	m = NET_EnumerateAddresses(collection, con, flags, addr, sizeof(addr)/sizeof(addr[0]));

	for (i = 0; i < m; i++)
	{
		if (addr[i].type != NA_INVALID)
		{
			char *scopes[] = {NULL, "local", "lan", "net"};
			char *scope;
			char *desc;
			scope = scopes[NET_ClassifyAddress(&addr[i], &desc)];
			if (scope)
			{
				warn = false;
				if (desc)
					Con_Printf("%s address (%s): %s (%s)\n", scope, con[i]->name, NET_AdrToString(adrbuf, sizeof(adrbuf), &addr[i]), desc);
				else
					Con_Printf("%s address (%s): %s\n", scope, con[i]->name, NET_AdrToString(adrbuf, sizeof(adrbuf), &addr[i]));
			}
		}
	}

	if (warn)
		Con_Printf("net address: no addresses\n");
}

//=============================================================================

int TCP_OpenStream (netadr_t *remoteaddr)
{
#ifndef HAVE_TCP
	return INVALID_SOCKET;
#else
	unsigned long _true = true;
	int newsocket;
	int temp;
	struct sockaddr_qstorage qs;
//	struct sockaddr_qstorage loc;
	int recvbufsize = (1<<19);//512kb

	temp = NetadrToSockadr(remoteaddr, &qs);

	if ((newsocket = socket (((struct sockaddr_in*)&qs)->sin_family, SOCK_CLOEXEC|SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		return (int)INVALID_SOCKET;

	setsockopt(newsocket, SOL_SOCKET, SO_RCVBUF, (void*)&recvbufsize, sizeof(recvbufsize));

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(neterrno()));

//	memset(&loc, 0, sizeof(loc));
//	((struct sockaddr*)&loc)->sa_family = ((struct sockaddr*)&loc)->sa_family;
//	bind(newsocket, (struct sockaddr *)&loc, ((struct sockaddr_in*)&qs)->sin_family == AF_INET?sizeof(struct sockaddr_in):sizeof(struct sockaddr_in6));

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
int UDP_OpenSocket (int port, qboolean bcast)
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

	if (bcast)
	{
		_true = true;
		if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&_true, sizeof(_true)) == -1)
		{
			Con_Printf("Cannot create broadcast socket\n");
			return (int)INVALID_SOCKET;
		}
	}

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

#ifdef IPPROTO_IPV6
int UDP6_OpenSocket (int port, qboolean bcast)
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

	if (bcast)
	{
//		address.sin6_addr
//		_true = true;
//		if (setsockopt(newsocket, SOL_SOCKET, IP_ADD_MEMBERSHIP, (char *)&_true, sizeof(_true)) == -1)
//		{
//			Con_Printf("Cannot create broadcast socket\n");
//			closesocket(newsocket);
//			return (int)INVALID_SOCKET;
//		}
	}

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

int IPX_OpenSocket (int port, qboolean bcast)
{
#ifndef USEIPX
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

	if (bcast)
	{
		// make it broadcast capable
		if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&_true, sizeof(_true)) == -1)
		{
			Con_Printf ("WARNING: IPX_Socket: setsockopt SO_BROADCAST: %i\n", neterrno());
			return INVALID_SOCKET;
		}
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
#ifdef USEIPX
	closesocket(socket);
#endif
}
#endif

// sleeps msec or until net socket is ready
//stdin can sometimes be a socket. As a result,
//we give the option to select it for nice console imput with timeouts.
#ifndef CLIENTONLY
qboolean NET_Sleep(float seconds, qboolean stdinissocket)
{
#ifdef HAVE_PACKET
	struct timeval timeout;
	fd_set	fdset;
	qintptr_t maxfd = -1;
	int con, sock;
	unsigned int usec;

	FD_ZERO(&fdset);

	if (stdinissocket)
	{
		sock = 0;	//stdin tends to be socket/filehandle 0 in unix
		FD_SET(sock, &fdset);
		maxfd = sock;
	}

	if (svs.sockets)
	for (con = 0; con < MAX_CONNECTIONS; con++)
	{
		if (!svs.sockets->conn[con])
			continue;
		if (svs.sockets->conn[con]->SetReceiveFDSet)
		{
			sock = svs.sockets->conn[con]->SetReceiveFDSet(svs.sockets->conn[con], &fdset);
			if (sock > maxfd)
				maxfd = sock;
		}
		else
		{
			sock = svs.sockets->conn[con]->thesocket;
			if (sock != INVALID_SOCKET)
			{
				FD_SET(sock, &fdset); // network socket
				if (sock > maxfd)
					maxfd = sock;
			}
		}
	}

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
		select(maxfd+1, &fdset, NULL, NULL, &timeout);
	}

	if (stdinissocket)
		return FD_ISSET(0, &fdset);
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
		NET_StringToSockaddr2("0.0.0.0", 0, (struct sockaddr_qstorage *)&address, NULL, NULL, 1);
//		Sys_Error ("NET_Init: getsockname:", strerror(qerrno));
	}

	SockadrToNetadr(&address, out);
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

#ifndef CLIENTONLY
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
#ifndef SERVERONLY
		FTENET_AddToCollection(svs.sockets, "SVLoopback", STRINGIFY(PORT_QWSERVER), NA_LOOPBACK, true);
#endif
	}

	FTENET_AddToCollection(svs.sockets, conname, *s?s:NULL, *s?NA_IP:NA_INVALID, true);
}
#endif

#ifndef SERVERONLY
void NET_ClientPort_f(void)
{
	Con_Printf("Active Client ports:\n");
	NET_PrintAddresses(cls.sockets);
	Con_Printf("end of list\n");
}
#endif

qboolean NET_WasSpecialPacket(netsrc_t netsrc)
{
#ifdef HAVE_NATPMP
	ftenet_connections_t *collection = NULL;
	if (netsrc == NS_SERVER)
	{
#ifndef CLIENTONLY
		collection = svs.sockets;
#endif
	}
	else
	{
#ifndef SERVERONLY
		collection = cls.sockets;
#endif
	}
#endif

#ifdef SUPPORT_ICE
	if (ICE_WasStun(netsrc))
		return true;
#endif
#ifdef HAVE_NATPMP
	if (NET_Was_NATPMP(collection))
		return true;
#endif
	return false;
}

static void QDECL NET_UPNPIGP_Callback(cvar_t *var, char *oldval)
{
}
cvar_t net_upnpigp = CVARCD("net_upnpigp", "0", NET_UPNPIGP_Callback, "If set, enables the use of the upnp-igd protocol to punch holes in your local NAT box.");

void SSL_Init(void);
/*
====================
NET_Init
====================
*/
void NET_Init (void)
{
	Cvar_Register(&net_enabled, "networking");
	if (net_enabled.ival)
	{
#if defined(_WIN32) && defined(HAVE_PACKET)
		int		r;
#ifdef IPPROTO_IPV6
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

	Cvar_Register(&net_hybriddualstack, "networking");
	Cvar_Register(&net_fakeloss, "networking");

#ifndef CLIENTONLY
	Cmd_AddCommand("sv_addport", SVNET_AddPort_f);
#endif
#ifndef SERVERONLY
	Cmd_AddCommand("cl_addport", NET_ClientPort_f);
#endif

	Cvar_Register (&net_upnpigp, "networking");
	net_upnpigp.restriction = RESTRICT_MAX;

	//
	// init the message buffer
	//
	net_message.maxsize = sizeof(net_message_buffer);
	net_message.data = net_message_buffer;

#if defined(HAVE_WINSSPI)
	SSL_Init();
#endif

	Net_Master_Init();
}
#ifndef SERVERONLY
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
#ifndef CLIENTONLY
	FTENET_AddToCollection(cls.sockets, "CLLoopback", "1", NA_LOOPBACK, true);
#endif
	if (loopbackonly)
		port = "";
#ifdef HAVE_IPV4
	FTENET_AddToCollection(cls.sockets, "CLUDP4", port, NA_IP, true);
#endif
#ifdef IPPROTO_IPV6
	FTENET_AddToCollection(cls.sockets, "CLUDP6", port, NA_IPV6, true);
#endif
#ifdef USEIPX
	FTENET_AddToCollection(cls.sockets, "CLIPX", port, NA_IPX, true);
#endif

	//	Con_TPrintf("Client port Initialized\n");
}
#endif

#ifndef CLIENTONLY
#ifdef HAVE_IPV4
void QDECL SV_Tcpport_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_TCP, true);
}
cvar_t	sv_port_tcp = CVARC("sv_port_tcp", "", SV_Tcpport_Callback);
#endif
#ifdef IPPROTO_IPV6
void QDECL SV_Tcpport6_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_TCPV6, true);
}
cvar_t	sv_port_tcp6 = CVARC("sv_port_tcp6", "", SV_Tcpport6_Callback);
#endif
#ifdef HAVE_IPV4
void QDECL SV_Port_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_IP, true);
}
cvar_t  sv_port_ipv4 = CVARC("sv_port", STRINGIFY(PORT_QWSERVER), SV_Port_Callback);
#endif
#ifdef IPPROTO_IPV6
void QDECL SV_PortIPv6_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_IPV6, true);
}
cvar_t  sv_port_ipv6 = CVARCD("sv_port_ipv6", "", SV_PortIPv6_Callback, "Port to use for incoming ipv6 udp connections. Due to hybrid sockets this might not be needed. You can specify an ipv4 address:port for a second ipv4 port if you want.");
#endif
#ifdef USEIPX
void QDECL SV_PortIPX_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, var->string, NA_IPX, true);
}
cvar_t  sv_port_ipx = CVARC("sv_port_ipx", "", SV_PortIPX_Callback);
#endif
#ifdef HAVE_NATPMP
void QDECL SV_Port_NatPMP_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, var->name, va("natpmp://%s", var->string), NA_NATPMP, true);
}
#if 1//def SERVERONLY
#define NATPMP_DEFAULT_PORT ""		//don't fuck with dedicated servers
#else
#define NATPMP_DEFAULT_PORT "5351"	//home users, yay, lucky people.
#endif
cvar_t sv_port_natpmp = CVARCD("sv_port_natpmp", NATPMP_DEFAULT_PORT, SV_Port_NatPMP_Callback, "If set (typically to 5351), automatically configures your router's port forwarding. You can instead specify the full ip address of your router (192.168.1.1:5351 for example). Your router must have NAT-PMP supported and enabled.");
#endif

void SVNET_RegisterCvars(void)
{
#if defined(TCPCONNECT) && defined(HAVE_IPV4)
	Cvar_Register (&sv_port_tcp,	"networking");
	sv_port_tcp.restriction = RESTRICT_MAX;
#endif
#if defined(TCPCONNECT) && defined(IPPROTO_IPV6)
	Cvar_Register (&sv_port_tcp6,	"networking");
	sv_port_tcp6.restriction = RESTRICT_MAX;
#endif
#ifdef IPPROTO_IPV6
	Cvar_Register (&sv_port_ipv6,	"networking");
	sv_port_ipv6.restriction = RESTRICT_MAX;
#endif
#ifdef USEIPX
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
}

void NET_CloseServer(void)
{
	FTENET_CloseCollection(svs.sockets);
	svs.sockets = NULL;
}

void NET_InitServer(void)
{
	if (sv_listen_nq.value || sv_listen_dp.value || sv_listen_qw.value || sv_listen_q3.ival)
	{
		if (!svs.sockets)
		{
			svs.sockets = FTENET_CreateCollection(true);
#ifndef SERVERONLY
			FTENET_AddToCollection(svs.sockets, "SVLoopback", STRINGIFY(PORT_QWSERVER), NA_LOOPBACK, true);
#endif
		}

#ifdef HAVE_IPV4
		Cvar_ForceCallback(&sv_port_ipv4);
#endif
#ifdef IPPROTO_IPV6
		Cvar_ForceCallback(&sv_port_ipv6);
#endif
#ifdef USEIPX
		Cvar_ForceCallback(&sv_port_ipx);
#endif
#if defined(TCPCONNECT) && defined(HAVE_TCP)
		Cvar_ForceCallback(&sv_port_tcp);
#ifdef IPPROTO_IPV6
		Cvar_ForceCallback(&sv_port_tcp6);
#endif
#endif
#ifdef HAVE_NATPMP
		Cvar_ForceCallback(&sv_port_natpmp);
#endif
	}
	else
	{
		NET_CloseServer();

#ifndef SERVERONLY
		svs.sockets = FTENET_CreateCollection(true);
		FTENET_AddToCollection(svs.sockets, "SVLoopback", STRINGIFY(PORT_QWSERVER), NA_LOOPBACK, true);
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
#ifndef CLIENTONLY
	NET_CloseServer();
#endif
#ifndef SERVERONLY
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
void VFSTCP_Error(tcpfile_t *f)
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
		return 0;

	if (tf->conpending)
	{
		fd_set fd;
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		FD_ZERO(&fd);
		FD_SET(tf->sock, &fd);
		if (!select((int)tf->sock+1, NULL, &fd, &fd, &timeout))
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
qofs_t QDECL VFSTCP_Tell (struct vfsfile_s *file)
{
	VFSTCP_Error((tcpfile_t*)file);
	return 0;
}
qofs_t QDECL VFSTCP_GetLen (struct vfsfile_s *file)
{
	return 0;
}
qboolean QDECL VFSTCP_Close (struct vfsfile_s *file)
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
	newf->funcs.seekingisabadplan = true;

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

