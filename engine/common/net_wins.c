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

#ifdef _WIN32
#define USE_GETHOSTNAME_LOCALLISTING
#endif

netadr_t	net_local_cl_ipadr;	//still used to match local ui requests (quake/gamespy), and to generate ip reports for q3 servers (which is probably pointless).

netadr_t	net_from;
sizebuf_t	net_message;

//#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)	// one more than msg + header
#define	MAX_UDP_PACKET	8192	// one more than msg + header
qbyte		net_message_buffer[MAX_OVERALLMSGLEN];
#ifdef _WIN32
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

void NET_GetLocalAddress (int socket, netadr_t *out);
int TCP_OpenListenSocket (int port);
#ifdef HAVE_IPV4
extern cvar_t sv_port_ipv4;
#endif
#ifdef IPPROTO_IPV6
int UDP6_OpenSocket (int port, qboolean bcast);
extern cvar_t sv_port_ipv6;
#endif
#ifdef USEIPX
void IPX_CloseSocket (int socket);
extern cvar_t sv_port_ipx;
#endif
#ifdef TCPCONNECT
extern cvar_t sv_port_tcp;
extern cvar_t sv_port_tcp6;
#endif
cvar_t	net_hybriddualstack = CVAR("net_hybriddualstack", "1");
cvar_t	net_fakeloss	= CVARFD("net_fakeloss", "0", CVAR_CHEAT, "Simulates packetloss in both receiving and sending, on a scale from 0 to 1.");

extern cvar_t sv_public, sv_listen_qw, sv_listen_nq, sv_listen_dp, sv_listen_q3;

static qboolean allowconnects = false;

#define	MAX_LOOPBACK	8
typedef struct
{
	qbyte	data[MAX_UDP_PACKET];
	int		datalen;
} loopmsg_t;

typedef struct
{
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

		*(int *)&((struct sockaddr_in*)s)->sin_addr = INADDR_BROADCAST;
		((struct sockaddr_in*)s)->sin_port = a->port;
		return sizeof(struct sockaddr_in);

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

	case NA_TCPV6:
	case NA_IPV6:
		memset (s, 0, sizeof(struct sockaddr_in6));
		((struct sockaddr_in6*)s)->sin6_family = AF_INET6;

		memcpy(&((struct sockaddr_in6*)s)->sin6_addr, a->address.ip6, sizeof(struct in6_addr));
		((struct sockaddr_in6*)s)->sin6_port = a->port;
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
		Sys_Error("Bad type - needs fixing");
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

qboolean	NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return false;

	if (a.type == NA_LOOPBACK)
		return true;

#ifdef HAVE_WEBSOCKCL
	if (a.type == NA_WEBSOCKET)
	{
		if (!strcmp(a.address.websocketurl, a.address.websocketurl) && a.port == b.port)
			return true;
		return false;
	}
#endif

#ifdef HAVE_IPV4
	if (a.type == NA_IP || a.type == NA_BROADCAST_IP || a.type == NA_TCP)
	{
		if ((memcmp(a.address.ip, b.address.ip, sizeof(a.address.ip)) == 0) && a.port == b.port)
			return true;
		return false;
	}
#endif

#ifdef IPPROTO_IPV6
	if (a.type == NA_IPV6 || a.type == NA_BROADCAST_IP6 || a.type == NA_TCPV6)
	{
		if ((memcmp(a.address.ip6, b.address.ip6, sizeof(a.address.ip6)) == 0) && a.port == b.port)
			return true;
		return false;
	}
#endif

#ifdef USEIPX
	if (a.type == NA_IPX || a.type == NA_BROADCAST_IPX)
	{
		if ((memcmp(a.address.ipx, b.address.ipx, sizeof(a.address.ipx)) == 0) && a.port == b.port)
			return true;
		return false;
	}
#endif

#ifdef IRCCONNECT
	if (a.type == NA_IRC)
	{
		if (!strcmp(a.address.irc.user, b.address.irc.user))
			return true;
		return false;
	}
#endif

	Sys_Error("NET_CompareAdr: Bad address type");
	return false;
}

/*
===================
NET_CompareBaseAdr

Compares without the port
===================
*/
qboolean	NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return false;

	if (a.type == NA_LOOPBACK)
		return true;

#ifdef HAVE_IPV4
	if (a.type == NA_IP || a.type == NA_TCP)
	{
		if ((memcmp(a.address.ip, b.address.ip, sizeof(a.address.ip)) == 0))
			return true;
		return false;
	}
#endif
#ifdef IPPROTO_IPV6
	if (a.type == NA_IPV6 || a.type == NA_BROADCAST_IP6)
	{
		if ((memcmp(a.address.ip6, b.address.ip6, 16) == 0))
			return true;
		return false;
	}
#endif
#ifdef USEIPX
	if (a.type == NA_IPX)
	{
		if ((memcmp(a.address.ipx, b.address.ipx, 10) == 0))
			return true;
		return false;
	}
#endif
#ifdef IRCCONNECT
	if (a.type == NA_IRC)
	{
		if (!strcmp(a.address.irc.user, b.address.irc.user))
			return true;
		return false;
	}
#endif

	Sys_Error("NET_CompareBaseAdr: Bad address type");
	return false;
}

qboolean NET_AddressSmellsFunny(netadr_t a)
{
#ifdef IPPROTO_IPV6
	int i;
#endif

	//rejects certain blacklisted addresses
	switch(a.type)
	{
#ifdef HAVE_IPV4
	case NA_BROADCAST_IP:
	case NA_IP:
		//reject localhost
		if (a.address.ip[0] == 127)// && a.address.ip[1] == 0   && a.address.ip[2] == 0   && a.address.ip[3] == 1  )
			return true;
		//'this' network (not an issue, but lets reject it anyway)
		if (a.address.ip[0] == 0   && a.address.ip[1] == 0   && a.address.ip[2] == 0   && a.address.ip[3] == 0  )
			return true;
		//reject any broadcasts
		if (a.address.ip[0] == 255 && a.address.ip[1] == 255 && a.address.ip[2] == 255 && a.address.ip[3] == 0  )
			return true;
		//not much else I can reject
		return false;
#endif

#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
		//reject [::XXXX] (this includes obsolete ipv4-compatible (not ipv4 mapped), and localhost)
		for (i = 0; i < 12; i++)
			if (a.address.ip6[i])
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

char	*NET_AdrToString (char *s, int len, netadr_t a)
{
	char *rs = s;
	char *p;
	int i;
#ifdef IPPROTO_IPV6
	qboolean doneblank;
#endif

	switch(a.type)
	{
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:
		Q_strncpyz(s, a.address.websocketurl, len);
		break;
#endif
#ifdef TCPCONNECT
	case NA_TCP:
		if (len < 7)
			return "?";
		snprintf (s, len, "tcp://");
		s += 6;
		len -= 6;
		//fallthrough
#endif
#ifdef HAVE_IPV4
	case NA_BROADCAST_IP:
	case NA_IP:
		if (a.port)
		{
			snprintf (s, len, "%i.%i.%i.%i:%i",
				a.address.ip[0],
				a.address.ip[1],
				a.address.ip[2],
				a.address.ip[3],
				ntohs(a.port));
		}
		else
		{
			snprintf (s, len, "%i.%i.%i.%i",
				a.address.ip[0],
				a.address.ip[1],
				a.address.ip[2],
				a.address.ip[3]);
		}
		break;
#endif
#ifdef TCPCONNECT
	case NA_TCPV6:
		if (len < 7)
			return "?";
		snprintf (s, len, "tcp://");
		s += 6;
		len -= 6;
		//fallthrough
#endif
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:

		if (!*(int*)&a.address.ip6[0] && 
			!*(int*)&a.address.ip6[4] &&
			!*(short*)&a.address.ip6[8] &&
			*(short*)&a.address.ip6[10] == (short)0xffff)
		{
			if (a.port)
				snprintf (s, len, "%i.%i.%i.%i:%i",
					a.address.ip6[12],
					a.address.ip6[13],
					a.address.ip6[14],
					a.address.ip6[15],
					ntohs(a.port));
			else
				snprintf (s, len, "%i.%i.%i.%i",
					a.address.ip6[12],
					a.address.ip6[13],
					a.address.ip6[14],
					a.address.ip6[15]);
			break;
		}
		*s = 0;
		doneblank = false;
		p = s;
		snprintf (s, len-strlen(s), "[");
		p += strlen(p);

		for (i = 0; i < 16; i+=2)
		{
			if (doneblank!=true && a.address.ip6[i] == 0 && a.address.ip6[i+1] == 0)
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
				if (a.address.ip6[i+0])
				{
					snprintf (p, len-strlen(s), "%x%02x",
						a.address.ip6[i+0],
						a.address.ip6[i+1]);
				}
				else
				{
					snprintf (p, len-strlen(s), "%x",
						a.address.ip6[i+1]);
				}
				p += strlen(p);
			}
		}

		snprintf (p, len-strlen(s), "]:%i",
			ntohs(a.port));
		break;
#endif
#ifdef USEIPX
	case NA_BROADCAST_IPX:
	case NA_IPX:
		snprintf (s, len, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i",
			a.address.ipx[0],
			a.address.ipx[1],
			a.address.ipx[2],
			a.address.ipx[3],
			a.address.ipx[4],
			a.address.ipx[5],
			a.address.ipx[6],
			a.address.ipx[7],
			a.address.ipx[8],
			a.address.ipx[9],
			ntohs(a.port));
		break;
#endif
	case NA_LOOPBACK:
		snprintf (s, len, "QLoopBack");
		break;

#ifdef IRCCONNECT
	case NA_IRC:
		if (*a.address.irc.channel)
			snprintf (s, len, "irc://%s@%s", a.address.irc.user, a.address.irc.channel);
		else
			snprintf (s, len, "irc://%s", a.address.irc.user);
		break;
#endif

	default:
		snprintf (s, len, "invalid netadr_t type");
//		Sys_Error("NET_AdrToString: Bad netadr_t type");
	}

	return rs;
}

char	*NET_BaseAdrToString (char *s, int len, netadr_t a)
{
	int i, doneblank;
	char *p;

	switch(a.type)
	{
	case NA_BROADCAST_IP:
	case NA_IP:
		snprintf (s, len, "%i.%i.%i.%i",
			a.address.ip[0],
			a.address.ip[1],
			a.address.ip[2],
			a.address.ip[3]);
		break;
	case NA_TCP:
		snprintf (s, len, "tcp://%i.%i.%i.%i",
			a.address.ip[0],
			a.address.ip[1],
			a.address.ip[2],
			a.address.ip[3]);
		break;
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
		if (!*(int*)&a.address.ip6[0] && 
			!*(int*)&a.address.ip6[4] &&
			!*(short*)&a.address.ip6[8] &&
			*(short*)&a.address.ip6[10] == (short)0xffff)
		{
			snprintf (s, len, "%i.%i.%i.%i",
				a.address.ip6[12],
				a.address.ip6[13],
				a.address.ip6[14],
				a.address.ip6[15]);
			break;
		}
		*s = 0;
		doneblank = false;
		p = s;
		for (i = 0; i < 16; i+=2)
		{
			if (doneblank!=true && a.address.ip6[i] == 0 && a.address.ip6[i+1] == 0)
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
				if (a.address.ip6[i+0])
				{
					snprintf (p, len-strlen(s), "%x%02x",
						a.address.ip6[i+0],
						a.address.ip6[i+1]);
				}
				else
				{
					snprintf (p, len-strlen(s), "%x",
						a.address.ip6[i+1]);
				}
				p += strlen(p);
			}
		}
		break;
#endif
#ifdef USEIPX
	case NA_BROADCAST_IPX:
	case NA_IPX:
		snprintf (s, len, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x",
			a.address.ipx[0],
			a.address.ipx[1],
			a.address.ipx[2],
			a.address.ipx[3],
			a.address.ipx[4],
			a.address.ipx[5],
			a.address.ipx[6],
			a.address.ipx[7],
			a.address.ipx[8],
			a.address.ipx[9]);
		break;
#endif
	case NA_LOOPBACK:
		snprintf (s, len, "LocalHost");
		break;

#ifdef IRCCONNECT
	case NA_IRC:
		snprintf (s, len, "irc://%s", a.address.irc.user);
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
qboolean	NET_StringToSockaddr (const char *s, int defaultport, struct sockaddr_qstorage *sadr, int *addrfamily, int *addrsize)
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];

	if (!(*s))
		return false;

	memset (sadr, 0, sizeof(*sadr));

#ifdef USEIPX
	if ((strlen(s) >= 23) && (s[8] == ':') && (s[21] == ':'))	// check for an IPX address
	{
		unsigned int val;

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
	}
	else
#endif
#ifdef IPPROTO_IPV6
	if (pgetaddrinfo)
	{
		struct addrinfo *addrinfo = NULL;
		struct addrinfo *pos;
		struct addrinfo udp6hint;
		int error;
		char *port;
		char dupbase[256];
		int len;

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
				if (((struct sockaddr_in *)sadr)->sin_family == AF_INET6)
					break;	//first one should be best...
				//fallthrough
#ifdef HAVE_IPV4
			case AF_INET:
				memcpy(sadr, pos->ai_addr, pos->ai_addrlen);
				if (pos->ai_family == AF_INET)
					goto dblbreak;	//don't try finding any more, this is quake, they probably prefer ip4...
				break;
#else
				memcpy(sadr, pos->ai_addr, pos->ai_addrlen);
				goto dblbreak;
#endif
			}
		}
dblbreak:
		pfreeaddrinfo (addrinfo);
		if (!((struct sockaddr*)sadr)->sa_family)	//none suitablefound
			return false;

		if (addrfamily)
			*addrfamily = ((struct sockaddr*)sadr)->sa_family;
	
		if (((struct sockaddr*)sadr)->sa_family == AF_INET)
		{
			if (!((struct sockaddr_in *)sadr)->sin_port)
				((struct sockaddr_in *)sadr)->sin_port = htons(defaultport);
			if (addrsize)
				*addrsize = sizeof(struct sockaddr_in);
		}
		else
		{
			if (!((struct sockaddr_in6 *)sadr)->sin6_port)
				((struct sockaddr_in6 *)sadr)->sin6_port = htons(defaultport);
			if (addrsize)
				*addrsize = sizeof(struct sockaddr_in6);
		}
	}
	else
#endif
	{
#ifdef HAVE_IPV4
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
#else
		return false;
#endif
	}

	return true;
}

/*
accepts anything that NET_StringToSockaddr accepts plus certain url schemes
including: tcp, irc
*/
qboolean	NET_StringToAdr (const char *s, int defaultport, netadr_t *a)
{
	struct sockaddr_qstorage sadr;

	Con_DPrintf("Resolving address: %s\n", s);

	if (!strcmp (s, "internalserver"))
	{
		memset (a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

#ifdef HAVE_WEBSOCKCL
	if (!strncmp (s, "ws://", 5) || !strncmp (s, "wss://", 6))
	{
		memset (a, 0, sizeof(*a));
		a->type = NA_WEBSOCKET;
		Q_strncpyz(a->address.websocketurl, s, sizeof(a->address.websocketurl));
		return true;
	}
	else
	{
		/*code for convienience - no other protocols work anyway*/
		static qboolean warned;
		if (!warned)
		{
			Con_Printf("Note: Native client builds can only connect to websocket servers.\n");
			warned = true;
		}
		memset (a, 0, sizeof(*a));
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

		if (!NET_StringToSockaddr (s+6, 0, &sadr, NULL, NULL))
		{
			a->type = NA_INVALID;
			return false;
		}

		SockadrToNetadr (&sadr, a);

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
#endif
#ifdef IRCCONNECT
	if (!strncmp (s, "irc://", 6))
	{
		char *at;
		memset (a, 0, sizeof(*a));
		a->type = NA_IRC;

		s+=6;
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

	if (!NET_StringToSockaddr (s, 0, &sadr, NULL, NULL))
	{
		a->type = NA_INVALID;
		return false;
	}

	SockadrToNetadr (&sadr, a);

	return true;
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
	case NA_WEBSOCKET:
	case NA_TCP:
	case NA_TCPV6:
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
qboolean NET_CompareAdrMasked(netadr_t a, netadr_t b, netadr_t mask)
{
	int i;

	//make sure the address being checked against matches the mask
	if (b.type != mask.type)
		return false;

	// check port if both are non-zero
	if (a.port && b.port && a.port != b.port)
		return false;

	// check to make sure all types match
	if (a.type != b.type)
	{
		if (a.type == NA_IP && b.type == NA_IPV6 && mask.type == NA_IP)
		{
			for (i = 0; i < 10; i++)
				if (b.address.ip6[i] != 0)
					return false;	//only matches if they're 0s, otherwise its not an ipv4 address there
			for (; i < 12; i++)
				if (b.address.ip6[i] != 0xff && b.address.ip6[i] != 0x00)	//0x00 is depricated
					return false;	//only matches if they're 0s or ffs, otherwise its not an ipv4 address there
			for (i = 0; i < 4; i++)
			{
				if ((a.address.ip[i] & mask.address.ip[i]) != (b.address.ip6[12+i] & mask.address.ip[i]))
					return false;	//mask doesn't match
			}
			return true;	//its an ipv4 address in there, the mask matched the whole way through
		}
		if (a.type == NA_IPV6 && b.type == NA_IP && mask.type == NA_IP)
		{
			for (i = 0; i < 10; i++)
				if (a.address.ip6[i] != 0)
					return false;	//only matches if they're 0s, otherwise its not an ipv4 address there

			for (; i < 12; i++)
				if (a.address.ip6[i] != 0xff && a.address.ip6[i] != 0x00)	//0x00 is depricated
					return false;	//only matches if they're 0s or ffs, otherwise its not an ipv4 address there

			for (i = 0; i < 4; i++)
			{
				if ((a.address.ip6[12+i] & mask.address.ip[i]) != (b.address.ip[i] & mask.address.ip[i]))
					return false;	//mask doesn't match
			}
			return true;	//its an ipv4 address in there, the mask matched the whole way through
		}
		return false;
	}

	// match on protocol type and compare address
	switch (a.type)
	{
	case NA_LOOPBACK:
		return true;
	case NA_BROADCAST_IP:
	case NA_IP:
		for (i = 0; i < 4; i++)
		{
			if ((a.address.ip[i] & mask.address.ip[i]) != (b.address.ip[i] & mask.address.ip[i]))
				return false;
		}
		break;
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
		for (i = 0; i < 16; i++)
		{
			if ((a.address.ip6[i] & mask.address.ip6[i]) != (b.address.ip6[i] & mask.address.ip6[i]))
				return false;
		}
		break;
#endif
#ifdef USEIPX
	case NA_BROADCAST_IPX:
	case NA_IPX:
		for (i = 0; i < 10; i++)
		{
			if ((a.address.ipx[i] & mask.address.ipx[i]) != (b.address.ipx[i] & mask.address.ipx[i]))
				return false;
		}
		break;
#endif

#ifdef IRCCONNECT
	case NA_IRC:
		//masks are not supported, match explicitly
		if (strcmp(a.address.irc.user, b.address.irc.user))
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
int UniformMaskedBits(netadr_t mask)
{
	int bits;
	int b;
	unsigned int bs;
	qboolean bitenc = false;

	switch (mask.type)
	{
	case NA_BROADCAST_IP:
	case NA_IP:
		bits = 32;
		for (b = 3; b >= 0; b--)
		{
			if (mask.address.ip[b] == 0xFF)
				bitenc = true;
			else if (mask.address.ip[b])
			{
				bs = (~mask.address.ip[b]) & 0xFF;
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
			if (mask.address.ip6[b] == 0xFF)
				bitenc = true;
			else if (mask.address.ip6[b])
			{
				bs = (~mask.address.ip6[b]) & 0xFF;
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
			if (mask.address.ipx[b] == 0xFF)
				bitenc = true;
			else if (mask.address.ipx[b])
			{
				bs = (~mask.address.ipx[b]) & 0xFF;
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

char *NET_AdrToStringMasked (char *s, int len, netadr_t a, netadr_t amask)
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

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
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

qboolean	NET_IsLoopBackAddress (netadr_t adr)
{
//	return (!strcmp(cls.servername, NET_AdrToString(net_local_adr)) || !strcmp(cls.servername, "local");
	return adr.type == NA_LOOPBACK;
}

/////////////////////////////////////////////
//loopback stuff

qboolean	NET_GetLoopPacket (netsrc_t sock, netadr_t *from, sizebuf_t *message)
{
	int		i;
	loopback_t	*loop;

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
	return true;

}


void NET_SendLoopPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	if (length > sizeof(loop->msgs[i].data))
		Sys_Error("NET_SendLoopPacket: Loopback buffer is too small");

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}
//=============================================================================

#define FTENET_ADDRTYPES 2
typedef struct ftenet_generic_connection_s {
	const char *name;

	int (*GetLocalAddress)(struct ftenet_generic_connection_s *con, netadr_t *local, int adridx);
	qboolean (*ChangeLocalAddress)(struct ftenet_generic_connection_s *con, const char *newaddress);
	qboolean (*GetPacket)(struct ftenet_generic_connection_s *con);
	qboolean (*SendPacket)(struct ftenet_generic_connection_s *con, int length, void *data, netadr_t to);
	void (*Close)(struct ftenet_generic_connection_s *con);
#ifdef HAVE_PACKET
	int (*SetReceiveFDSet) (struct ftenet_generic_connection_s *con, fd_set *fdset);	/*set for connections which have multiple sockets (ie: listening tcp connections)*/
#endif

	netadrtype_t addrtype[FTENET_ADDRTYPES];
	qboolean islisten;
	int thesocket;
} ftenet_generic_connection_t;

#define MAX_CONNECTIONS 8
typedef struct ftenet_connections_s {
	qboolean islisten;
	ftenet_generic_connection_t *conn[MAX_CONNECTIONS];
} ftenet_connections_t;

ftenet_connections_t *FTENET_CreateCollection(qboolean listen)
{
	ftenet_connections_t *col;
	col = Z_Malloc(sizeof(*col));
	col->islisten = listen;
	return col;
}

qboolean FTENET_AddToCollection(ftenet_connections_t *col, const char *name, const char *address, ftenet_generic_connection_t *(*establish)(qboolean isserver, const char *address), qboolean islisten)
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
			if (col->conn[i]->name && !strcmp(col->conn[i]->name, name))
			{
				if (address && *address)
				if (col->conn[i]->ChangeLocalAddress)
				{
					if (col->conn[i]->ChangeLocalAddress(col->conn[i], address))
						return true;
				}

				col->conn[i]->Close(col->conn[i]);
				col->conn[i] = NULL;
			}
		}
	}

	if (address && *address)
	{
		for (i = 0; i < MAX_CONNECTIONS; i++)
		{
			if (!col->conn[i])
			{
				address = COM_Parse(address);
				col->conn[i] = establish(islisten, com_token);
				if (!col->conn[i])
					break;
				col->conn[i]->name = name;
				count++;

				if (address && *address)
					continue;
				break;
			}
		}
	}
	return count > 0;
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

#if !defined(CLIENTONLY) && !defined(SERVERONLY)

int FTENET_Loop_GetLocalAddress(ftenet_generic_connection_t *con, netadr_t *out, int adrnum)
{
	if (adrnum==0)
	{
		out->type = NA_LOOPBACK;
		out->port = con->islisten+1;
	}
	return 1;
}

qboolean FTENET_Loop_GetPacket(ftenet_generic_connection_t *con)
{
	return NET_GetLoopPacket(con->islisten, &net_from, &net_message);
}

qboolean FTENET_Loop_SendPacket(ftenet_generic_connection_t *con, int length, void *data, netadr_t to)
{
	if (to.type == NA_LOOPBACK)
	{
		NET_SendLoopPacket(con->islisten, length, data, to);
		return true;
	}

	return false;
}

ftenet_generic_connection_t *FTENET_Loop_EstablishConnection(qboolean isserver, const char *address)
{
	ftenet_generic_connection_t *newcon;
	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		newcon->name = "Loopback";
		newcon->GetLocalAddress = FTENET_Loop_GetLocalAddress;
		newcon->GetPacket = FTENET_Loop_GetPacket;
		newcon->SendPacket = FTENET_Loop_SendPacket;
		newcon->Close = FTENET_Generic_Close;

		newcon->islisten = isserver;
		newcon->addrtype[0] = NA_LOOPBACK;
		newcon->addrtype[1] = NA_INVALID;

		newcon->thesocket = INVALID_SOCKET;
	}
	return newcon;
}
#endif

int FTENET_Generic_GetLocalAddress(ftenet_generic_connection_t *con, netadr_t *out, int count)
{
#ifndef HAVE_PACKET
	return 0;
#else
	struct sockaddr_qstorage	from;
	int fromsize = sizeof(from);
	netadr_t adr;
	char		adrs[MAX_ADR_SIZE];
	int b;
	int idx = 0;

	if (getsockname (con->thesocket, (struct sockaddr*)&from, &fromsize) != -1)
	{
		memset(&adr, 0, sizeof(adr));
		SockadrToNetadr(&from, &adr);

#ifdef USE_GETHOSTNAME_LOCALLISTING
		if (adr.type == NA_IPV6 &&
			!*(int*)&adr.address.ip6[0] &&
			!*(int*)&adr.address.ip6[4] &&
			!*(short*)&adr.address.ip6[8] &&
			*(short*)&adr.address.ip6[10]==(short)0xffff && 
			!*(int*)&adr.address.ip6[12])
		{
			/*ipv4-mapped address ANY, pretend we read blank*/
			b = sizeof(adr.address);
		}
		else
		{
			for (b = 0; b < sizeof(adr.address); b++)
				if (((unsigned char*)&adr.address)[b] != 0)
					break;
		}
		if (b == sizeof(adr.address))
		{
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

				if (pgetaddrinfo(adrs, NULL, &hints, &result) != 0)
				{
					if (idx++ == count)
						*out = adr;
				}
				else
				{
					for (itr = result; itr; itr = itr->ai_next)
					{
						if (itr->ai_addr->sa_family != ((struct sockaddr_in*)&from)->sin_family)
						{
#ifdef IPV6_V6ONLY
							if (((struct sockaddr_in*)&from)->sin_family == AF_INET6 && itr->ai_addr->sa_family == AF_INET)
							{
								int ipv6only = true;
								int optlen = sizeof(ipv6only);
								getsockopt(con->thesocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&ipv6only, &optlen);
								if (ipv6only)
									continue;
							}
							else
#endif
								continue;
						}

						if (itr->ai_addr->sa_family == AF_INET
							|| itr->ai_addr->sa_family == AF_INET6
#ifdef USEIPX
							|| itr->ai_addr->sa_family == AF_IPX
#endif
							)
						if (idx++ == count)
						{
							SockadrToNetadr((struct sockaddr_qstorage*)itr->ai_addr, out);
							out->port = ((struct sockaddr_in*)&from)->sin_port;
						}
					}
					pfreeaddrinfo(result);

					/*if none found, fill in the 0.0.0.0 or whatever*/
					if (!idx)
					{
						idx++;
						*out = adr;
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
					for (b = 0; h->h_addr_list[b]; b++)
					{
						((struct sockaddr_in*)&from)->sin_family = AF_INET;
						memcpy(&((struct sockaddr_in*)&from)->sin_addr, h->h_addr_list[b], sizeof(((struct sockaddr_in*)&from)->sin_addr));
						SockadrToNetadr(&from, &adr);
						if (idx++ == count)
							*out = adr;
					}
				}
#endif
#ifdef IPPROTO_IPV6
				if(h && h->h_addrtype == AF_INET6)
				{
					for (b = 0; h->h_addr_list[b]; b++)
					{
						((struct sockaddr_in*)&from)->sin_family = AF_INET6;
						memcpy(&((struct sockaddr_in6*)&from)->sin6_addr, h->h_addr_list[b], sizeof(((struct sockaddr_in6*)&from)->sin6_addr));
						SockadrToNetadr(&from, &adr);
						if (idx++ == count)
							*out = adr;
					}
				}
#endif

				if (b == 0)
				{
					if (idx++ == count)
						*out = adr;
				}
			}
		}
		else
#endif
		{
			if (adr.type == NA_IPV6 &&
				!*(int*)&adr.address.ip6[0] &&
				!*(int*)&adr.address.ip6[4] &&
				!*(int*)&adr.address.ip6[8] &&
				!*(int*)&adr.address.ip6[12])
			{
				if (idx++ == count)
				{
					*out = adr;
					out->type = NA_IP;
				}
			}
			if (idx++ == count)
				*out = adr;
		}
	}

	return idx;
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
		err = qerrno;

		if (err == EWOULDBLOCK)
			return false;
		if (err == EMSGSIZE)
		{
			SockadrToNetadr (&from, &net_from);
			Con_TPrintf (TL_OVERSIZEPACKETFROM,
				NET_AdrToString (adr, sizeof(adr), net_from));
			return false;
		}
		if (err == ECONNABORTED || err == ECONNRESET)
		{
			Con_TPrintf (TL_CONNECTIONLOSTORABORTED);	//server died/connection lost.
#ifndef SERVERONLY
			if (cls.state != ca_disconnected && !con->islisten)
			{
				if (cls.lastarbiatarypackettime+5 < Sys_DoubleTime())	//too many mvdsv
					Cbuf_AddText("disconnect\nreconnect\n", RESTRICT_LOCAL);	//retry connecting.
				else
					Con_Printf("Packet was not delivered - server might be badly configured\n");
				return false;
			}
#endif
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
		Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (adr, sizeof(adr), net_from));
		return false;
	}

	return true;
#endif
}

qboolean FTENET_Generic_SendPacket(ftenet_generic_connection_t *con, int length, void *data, netadr_t to)
{
#ifndef HAVE_PACKET
	return false;
#else
	struct sockaddr_qstorage	addr;
	int size;
	int ret;

	for (size = 0; size < FTENET_ADDRTYPES; size++)
		if (to.type == con->addrtype[size])
			break;
	if (size == FTENET_ADDRTYPES)
		return false;

#ifdef IPPROTO_IPV6
	/*special code to handle sending to hybrid sockets*/
	if (con->addrtype[1] == NA_IPV6 && to.type == NA_IP)
	{
		memset(&addr, 0, sizeof(struct sockaddr_in6));
		((struct sockaddr_in6*)&addr)->sin6_family = AF_INET6;
		*(short*)&((struct sockaddr_in6*)&addr)->sin6_addr.s6_addr[10] = 0xffff;
		*(int*)&((struct sockaddr_in6*)&addr)->sin6_addr.s6_addr[12] = *(int*)&to.address.ip;
		((struct sockaddr_in6*)&addr)->sin6_port = to.port;
		size = sizeof(struct sockaddr_in6);
	}
	else
#endif
	{
		NetadrToSockadr (&to, &addr);

		switch(to.type)
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
		int ecode = qerrno;
// wouldblock is silent
		if (ecode == EWOULDBLOCK)
			return true;

		if (ecode == ECONNREFUSED)
			return true;

#ifndef SERVERONLY
		if (ecode == EADDRNOTAVAIL)
			Con_DPrintf("NET_SendPacket Warning: %i\n", ecode);
		else
#endif
			Con_TPrintf (TL_NETSENDERROR, ecode);
	}
	return true;
#endif
}

qboolean	NET_PortToAdr (int adrfamily, const char *s, netadr_t *a)
{
	char *e;
	int port;
	port = strtoul(s, &e, 10);
	if (*e)	//if *e then its not just a single number in there, so treat it as a proper address.
		return NET_StringToAdr(s, 0, a);
	else if (port)
	{
		memset(a, 0, sizeof(*a));
		a->port = htons((unsigned short)port);
		switch(adrfamily)
		{
#ifdef HAVE_IPV4
		case AF_INET:
			a->type = NA_IP;
			return true;
#endif
#ifdef IPPROTO_IPV6
		case AF_INET6:
			a->type = NA_IPV6;
			return true;
#endif
#ifdef USEIPX
		case AF_IPX:
			a->type = NA_IPX;
			return true;
#endif
		default:
			a->type = NA_INVALID;
			return false;
		}
		return false;
	}
	a->type = NA_INVALID;
	return false;
}

ftenet_generic_connection_t *FTENET_Generic_EstablishConnection(int adrfamily, int protocol, qboolean isserver, const char *address)
{
#ifndef HAVE_PACKET
	return NULL;
#else
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_generic_connection_t *newcon;

	unsigned long _true = true;
	SOCKET newsocket = INVALID_SOCKET;
	int temp;
	netadr_t adr;
	struct sockaddr_qstorage qs;
	int family;
	int port;
	int bindtries;
	int bufsz;
	qboolean hybrid = false;


	if (!NET_PortToAdr(adrfamily, address, &adr))
	{
		Con_Printf("unable to resolve local address %s\n", address);
		return NULL;	//couldn't resolve the name
	}
	temp = NetadrToSockadr(&adr, &qs);
	family = ((struct sockaddr*)&qs)->sa_family;

#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
	if (isserver && family == AF_INET && net_hybriddualstack.ival && !((struct sockaddr_in*)&qs)->sin_addr.s_addr)
	{
		unsigned long _false = false;
		if ((newsocket = socket (AF_INET6, SOCK_DGRAM, protocol)) != INVALID_SOCKET)
		{
			if (0 == setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_false, sizeof(_false)))
			{
				int ip = ((struct sockaddr_in*)&qs)->sin_addr.s_addr;
				int port = ((struct sockaddr_in*)&qs)->sin_port;
				ip = ((struct sockaddr_in*)&qs)->sin_addr.s_addr;
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
		if ((newsocket = socket (family, SOCK_DGRAM, protocol)) == INVALID_SOCKET)
		{
			return NULL;
		}

#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
	if (family == AF_INET6)
		setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_true, sizeof(_true));
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
		NET_AdrToString((char*)&qs, sizeof(qs), adr);
		Con_Printf("Unable to listen at %s\n", (char*)&qs);
		closesocket(newsocket);
		return NULL;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(qerrno));


	//
	// determine my name & address if we don't already know it
	//
	if (!net_local_cl_ipadr.type == NA_INVALID)
		NET_GetLocalAddress (newsocket, &net_local_cl_ipadr);

	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		newcon->name = "Generic";
		newcon->GetLocalAddress = FTENET_Generic_GetLocalAddress;
		newcon->GetPacket = FTENET_Generic_GetPacket;
		newcon->SendPacket = FTENET_Generic_SendPacket;
		newcon->Close = FTENET_Generic_Close;

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
ftenet_generic_connection_t *FTENET_UDP6_EstablishConnection(qboolean isserver, const char *address)
{
	return FTENET_Generic_EstablishConnection(AF_INET6, IPPROTO_UDP, isserver, address);
}
#endif
#ifdef HAVE_IPV4
ftenet_generic_connection_t *FTENET_UDP4_EstablishConnection(qboolean isserver, const char *address)
{
	return FTENET_Generic_EstablishConnection(AF_INET, IPPROTO_UDP, isserver, address);
}
#endif
#ifdef USEIPX
ftenet_generic_connection_t *FTENET_IPX_EstablishConnection(qboolean isserver, const char *address)
{
	return FTENET_Generic_EstablishConnection(AF_IPX, NSPROTO_IPX, isserver, address);
}
#endif

#ifdef TCPCONNECT
typedef struct ftenet_tcpconnect_stream_s {
	int socketnum;
	int inlen;
	int outlen;

	enum
	{
		TCPC_UNKNOWN,
		TCPC_QIZMO,
		TCPC_WEBSOCKET
	} clienttype;
	char inbuffer[3000];
	char outbuffer[3000];
	float timeouttime;
	netadr_t remoteaddr;
	struct ftenet_tcpconnect_stream_s *next;
} ftenet_tcpconnect_stream_t;

typedef struct {
	ftenet_generic_connection_t generic;

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

int SHA1(char *digest, int maxdigestsize, char *string);
qboolean FTENET_TCPConnect_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	int ret;
	int err;
	char		adr[MAX_ADR_SIZE];
	struct sockaddr_qstorage	from;
	int fromlen;

	float timeval = Sys_DoubleTime();
	ftenet_tcpconnect_stream_t *st;
	st = con->tcpstreams;

	//remove any stale ones
	while (con->tcpstreams && con->tcpstreams->socketnum == INVALID_SOCKET)
	{
		st = con->tcpstreams;
		con->tcpstreams = con->tcpstreams->next;
		BZ_Free(st);
	}

	for (st = con->tcpstreams; st; st = st->next)
	{//client receiving only via tcp

		while (st->next && st->next->socketnum == INVALID_SOCKET)
		{
			ftenet_tcpconnect_stream_t *temp;
			temp = st->next;
			st->next = st->next->next;
			BZ_Free(temp);
			con->active--;
		}

//due to the above checks about invalid sockets, the socket is always open for st below.

		if (st->timeouttime < timeval)
			goto closesvstream;

		ret = recv(st->socketnum, st->inbuffer+st->inlen, sizeof(st->inbuffer)-st->inlen, 0);
		if (ret == 0)
			goto closesvstream;
		else if (ret == -1)
		{
			err = qerrno;

			if (err == EWOULDBLOCK)
				ret = 0;
			else
			{
				if (err == ECONNABORTED || err == ECONNRESET)
				{
					Con_TPrintf (TL_CONNECTIONLOSTORABORTED);	//server died/connection lost.
				}
				else
					Con_Printf ("TCPConnect_GetPacket: Error (%i): %s\n", err, strerror(err));

closesvstream:
				closesocket(st->socketnum);
				st->socketnum = INVALID_SOCKET;
				continue;
			}
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
					send(st->socketnum, "qizmo\n", 6, 0);
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

					//FIXME: reply with 426 Upgrade Required if wsversion is not supported

					if (!stricmp(arg[WCATTR_UPGRADE], "websocket") && !stricmp(arg[WCATTR_CONNECTION], "Upgrade"))
					{
						if (atoi(arg[WCATTR_WSVER]) != 13)
						{
							memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
							st->inlen -= i;
							resp = va(	"HTTP/1.1 426 Upgrade Required\r\n"
										"Sec-WebSocket-Version: 13\r\n"
										"\r\n");
							//send the websocket handshake rejection.
							send(st->socketnum, resp, strlen(resp), 0);

							goto closesvstream;
						}
						else
						{
							char acceptkey[20*2];
							unsigned char sha1digest[20];
							memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
							st->inlen -= i;

							tobase64(acceptkey, sizeof(acceptkey), sha1digest, SHA1(sha1digest, sizeof(sha1digest), va("%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", arg[WCATTR_WSKEY])));

							resp = va(	"HTTP/1.1 101 Switching Protocols\r\n"
										"Upgrade: websocket\r\n"
										"Connection: Upgrade\r\n"
										"Sec-WebSocket-Accept: %s\r\n"
//										"Sec-WebSocket-Protocol: FTEWebSocket\r\n"
										"\r\n", acceptkey);
							//send the websocket handshake response.
							send(st->socketnum, resp, strlen(resp), 0);

							//and the connection is okay
							st->clienttype = TCPC_WEBSOCKET;
						}
					}
					else
					{
						memmove(st->inbuffer, st->inbuffer+i, st->inlen - (i));
						st->inlen -= i;
						resp = va(	"HTTP/1.1 426 Upgrade Required\r\n"
									"Sec-WebSocket-Version: 13\r\n"
									"\r\n");
						//send the websocket handshake rejection.
						send(st->socketnum, resp, strlen(resp), 0);

						goto closesvstream;
					}
				}
			}
			else
			{
handshakeerror:
				Con_Printf ("Unknown TCP handshake from %s\n", NET_AdrToString (adr, sizeof(adr), net_from));
				goto closesvstream;
			}

			break;
		case TCPC_QIZMO:
			if (st->inlen < 2)
				continue;

			net_message.cursize = BigShort(*(short*)st->inbuffer);
			if (net_message.cursize >= sizeof(net_message_buffer) )
			{
				Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
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
		case TCPC_WEBSOCKET:
			while (st->inlen >= 2)
			{
				unsigned short ctrl = ((unsigned char*)st->inbuffer)[0]<<8 | ((unsigned char*)st->inbuffer)[1];
				unsigned long paylen;
				unsigned int payoffs = 2;
				unsigned int mask = 0;
				st->inbuffer[st->inlen]=0;
				if ((ctrl & 0x7f) == 127)
				{
					//as a payload is not allowed to be encoded as too large a type, and quakeworld never used packets larger than 1450 bytes anyway, this code isn't needed (65k is the max even without this)
//					if (sizeof(paylen) < 8)
					{
						Con_Printf ("%s: payload frame too large\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
						goto closesvstream; 
					}
/*					else
					{
						if (payoffs + 8 > st->inlen)
							break;
						paylen = 
							((unsigned char*)st->inbuffer)[payoffs+0]<<56 |
							((unsigned char*)st->inbuffer)[payoffs+1]<<48 |
							((unsigned char*)st->inbuffer)[payoffs+2]<<40 |
							((unsigned char*)st->inbuffer)[payoffs+3]<<32 |
							((unsigned char*)st->inbuffer)[payoffs+4]<<24 |
							((unsigned char*)st->inbuffer)[payoffs+5]<<16 |
							((unsigned char*)st->inbuffer)[payoffs+6]<<8 |
							((unsigned char*)st->inbuffer)[payoffs+7]<<0;
						if (paylen < 0x10000)
						{
							Con_Printf ("%s: payload size encoded badly\n", NET_AdrToString (st->remoteaddr, sizeof(st->remoteaddr), net_from));
							goto closesvstream; 
						}
						payoffs += 8;
					}
*/				}
				else if ((ctrl & 0x7f) == 126)
				{
					if (payoffs + 2 > st->inlen)
						break;
					paylen = 
						((unsigned char*)st->inbuffer)[payoffs+0]<<8 |
						((unsigned char*)st->inbuffer)[payoffs+1]<<0;
					if (paylen < 126)
					{
						Con_Printf ("%s: payload size encoded badly\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
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
				case 0:	/*continuation*/
					Con_Printf ("websocket continuation frame from %s\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
					goto closesvstream;
				case 1:	/*text frame*/
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
				case 2: /*binary frame*/
					Con_Printf ("websocket binary frame from %s\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
					net_message.cursize = paylen;
					if (net_message.cursize >= sizeof(net_message_buffer) )
					{
						Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (adr, sizeof(adr), net_from));
						goto closesvstream;
					}
					memcpy(net_message_buffer, st->inbuffer+payoffs, paylen);
					break;
				case 8:	/*connection close*/
					Con_Printf ("websocket closure %s\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
					goto closesvstream;
				case 9:	/*ping*/
					Con_Printf ("websocket ping from %s\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
					goto closesvstream;
				case 10: /*pong*/
					Con_Printf ("websocket pong from %s\n", NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
					goto closesvstream;
				default:
					Con_Printf ("Unsupported websocket opcode (%i) from %s\n", (ctrl>>8) & 0xf, NET_AdrToString (adr, sizeof(adr), st->remoteaddr));
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
			int _true = true;
			ioctlsocket(newsock, FIONBIO, (u_long *)&_true);
			setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));

			con->active++;
			st = Z_Malloc(sizeof(*con->tcpstreams));
			st->clienttype = TCPC_UNKNOWN;
			st->next = con->tcpstreams;
			con->tcpstreams = st;
			st->socketnum = newsock;
			st->inlen = 0;

			/*grab the net address*/
			SockadrToNetadr(&from, &st->remoteaddr);
			/*sockadr doesn't contain transport info, so fix that up here*/
			if (st->remoteaddr.type == NA_IP)
				st->remoteaddr.type = NA_TCP;
			else if (st->remoteaddr.type == NA_IPV6)
				st->remoteaddr.type = NA_TCPV6;

			st->timeouttime = timeval + 30;
		}
	}
	return false;
}

qboolean FTENET_TCPConnect_SendPacket(ftenet_generic_connection_t *gcon, int length, void *data, netadr_t to)
{
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	ftenet_tcpconnect_stream_t *st;

	for (st = con->tcpstreams; st; st = st->next)
	{
		if (st->socketnum == INVALID_SOCKET)
			continue;

		if (NET_CompareAdr(to, st->remoteaddr))
		{
			if (!st->outlen)
			{
				switch(st->clienttype)
				{
				case TCPC_QIZMO:
					{
						unsigned short slen = BigShort((unsigned short)length);
						if (st->outlen + sizeof(slen) + length > sizeof(st->outbuffer))
						{
							Con_DPrintf("FTENET_TCPConnect_SendPacket: outgoing overflow\n");
						}
						else
						{
							memcpy(st->outbuffer + st->outlen, &slen, sizeof(slen));
							memcpy(st->outbuffer + st->outlen + sizeof(slen), data, length);
							st->outlen += sizeof(slen) + length;
						}
					}
					break;
				case TCPC_WEBSOCKET:
					{
						/*as a server, we don't need the mask stuff*/
						unsigned short ctrl = 0x8100;
						unsigned int paylen = 0;
						unsigned int payoffs = 2;
						int i;
						for (i = 0; i < length; i++)
						{
							paylen += (((char*)data)[i] == 0 || ((unsigned char*)data)[i] >= 0x80)?2:1;
						}
						if (paylen >= 126)
						{
							ctrl |= 126;
							payoffs += 2;
						}
						else
							ctrl |= paylen;

						st->outbuffer[0] = ctrl>>8;
						st->outbuffer[1] = ctrl&0xff;
						if (paylen >= 126)
						{
							st->outbuffer[2] = paylen>>8;
							st->outbuffer[3] = paylen&0xff;
						}
						/*utf8ify the data*/
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
						st->outlen = payoffs;
					}
					break;
				default:
					break;
				}
			}

			if (st->outlen)
			{	/*try and flush the old data*/
				int done;
				done = send(st->socketnum, st->outbuffer, st->outlen, 0);
				if (done > 0)
				{
					memmove(st->outbuffer, st->outbuffer + done, st->outlen - done);
					st->outlen -= done;
				}
			}

			st->timeouttime = Sys_DoubleTime() + 20;

			return true;
		}
	}
	return false;
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

		if (st->socketnum != INVALID_SOCKET)
			closesocket(st->socketnum);

		BZ_Free(st);
	}

	FTENET_Generic_Close(gcon);
}

int FTENET_TCPConnect_SetReceiveFDSet(ftenet_generic_connection_t *gcon, fd_set *fdset)
{
	int maxfd = 0;
	ftenet_tcpconnect_connection_t *con = (ftenet_tcpconnect_connection_t*)gcon;
	ftenet_tcpconnect_stream_t *st;

	for (st = con->tcpstreams; st; st = st->next)
	{
		if (st->socketnum == INVALID_SOCKET)
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
	if (!strncmp(address, "tcp://", 6))
		address += 6;

	if (isserver)
	{
		if (!NET_PortToAdr(affamily, address, &adr))
			return NULL;	//couldn't resolve the name
		if (adr.type == NA_IP)
			adr.type = NA_TCP;
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
			NET_AdrToString((char*)&qs, sizeof(qs), adr);
			Con_Printf("Unable to listen at %s\n", (char*)&qs);
			closesocket(newsocket);
			return NULL;
		}

		if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
			Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(qerrno));
	}
	else
	{
		if (!NET_PortToAdr(affamily, address, &adr))
			return NULL;	//couldn't resolve the name

		if (adr.type == NA_IP)
			adr.type = NA_TCP;
		newsocket = TCP_OpenStream(adr);
		if (newsocket == INVALID_SOCKET)
			return NULL;
	}

	//this isn't fatal
	setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));

	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		newcon->generic.name = "TCPConnect";
		if (isserver)
			newcon->generic.GetLocalAddress = FTENET_Generic_GetLocalAddress;
		newcon->generic.GetPacket = FTENET_TCPConnect_GetPacket;
		newcon->generic.SendPacket = FTENET_TCPConnect_SendPacket;
		newcon->generic.Close = FTENET_TCPConnect_Close;
		newcon->generic.SetReceiveFDSet = FTENET_TCPConnect_SetReceiveFDSet;

		newcon->generic.islisten = isserver;
		newcon->generic.addrtype[0] = adr.type;
		newcon->generic.addrtype[1] = NA_INVALID;

		newcon->active = 0;

		if (!isserver)
		{
			newcon->generic.thesocket = INVALID_SOCKET;

			newcon->active++;
			newcon->tcpstreams = Z_Malloc(sizeof(*newcon->tcpstreams));
			newcon->tcpstreams->clienttype = TCPC_UNKNOWN;
			newcon->tcpstreams->next = NULL;
			newcon->tcpstreams->socketnum = newsocket;
			newcon->tcpstreams->inlen = 0;

			newcon->tcpstreams->remoteaddr = adr;

			//send the qizmo greeting.
			send(newsocket, "qizmo\n", 6, 0);

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
ftenet_generic_connection_t *FTENET_TCP6Connect_EstablishConnection(qboolean isserver, const char *address)
{
	return FTENET_TCPConnect_EstablishConnection(AF_INET6, isserver, address);
}
#endif

ftenet_generic_connection_t *FTENET_TCP4Connect_EstablishConnection(qboolean isserver, const char *address)
{
	return FTENET_TCPConnect_EstablishConnection(AF_INET, isserver, address);
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
			cvar_t *ircuser = Cvar_Get("ircuser", "none", 0, "IRC Connect");
			cvar_t *irchost = Cvar_Get("irchost", "none", 0, "IRC Connect");
			cvar_t *ircnick = Cvar_Get("ircnick", "ftesv", 0, "IRC Connect");
			//cvar_t *ircchannel = Cvar_Get("ircchannel", "#ftetest", 0, "IRC Connect"); //warning: unused variable ‘ircchannel’
			cvar_t *ircsomething = Cvar_Get("ircsomething", "moo", 0, "IRC Connect");
			cvar_t *ircclientaddr = Cvar_Get("ircclientaddr", "127.0.0.1", 0, "IRC Connect");

			con->generic.thesocket = TCP_OpenStream(con->ircserver);

			Q_strncpyz(con->ourusername, ircnick->string, sizeof(con->ourusername));

			send(con->generic.thesocket, "USER ", 5, 0);
			send(con->generic.thesocket, ircuser->string, strlen(ircuser->string), 0);
			send(con->generic.thesocket, " ", 1, 0);
			send(con->generic.thesocket, irchost->string, strlen(irchost->string), 0);
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
			read = qerrno;
			switch(read)
			{
			case ECONNABORTED:
			case ECONNRESET:
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
						Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (adr, sizeof(adr), net_from));
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
						Con_Printf ("Corrupt packet from %s\n", NET_AdrToString (adr, sizeof(adr), net_from));
					}
					else if (st->inlen == psize)
					{
						/*interpret as a connectionless packet*/
						net_message.cursize = st->inlen;
						if (net_message.cursize >= sizeof(net_message_buffer) )
						{
							Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (adr, sizeof(adr), net_from));
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
			case 001:
				{
					cvar_t *ircchannel = Cvar_Get("ircchannel", "", 0, "IRC Connect");
					if (*ircchannel->string)
					{
						send(con->generic.thesocket, "JOIN ", 5, 0);
						send(con->generic.thesocket, ircchannel->string, strlen(ircchannel->string), 0);
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
qboolean FTENET_IRCConnect_SendPacket(ftenet_generic_connection_t *gcon, int length, void *data, netadr_t to)
{
	ftenet_ircconnect_connection_t *con = (ftenet_ircconnect_connection_t*)gcon;

	unsigned char *buffer;
	unsigned char *lenofs;
	int packed;
	int fulllen = length;
	int newoutcount;

	for (packed = 0; packed < FTENET_ADDRTYPES; packed++)
		if (to.type == con->generic.addrtype[packed])
			break;
	if (packed == FTENET_ADDRTYPES)
		return false;

	packed = 0;

	if (con->generic.thesocket == INVALID_SOCKET)
		return true;
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

		if (*to.address.irc.channel)
		{
			int unamelen;
			int chanlen;
			unamelen = strlen(to.address.irc.user);
			chanlen = strlen(to.address.irc.channel);
			packed = 8+chanlen+3+unamelen+1 + 3;

			if (packed+1 + newoutcount > sizeof(con->outbuf))
				break;

			memcpy(buffer, "PRIVMSG ", 8);
			memcpy(buffer+8, to.address.irc.channel, chanlen);
			memcpy(buffer+8+chanlen, " :$", 3);
			memcpy(buffer+8+chanlen+3, to.address.irc.user, unamelen);
			memcpy(buffer+8+chanlen+3+unamelen, "#", 1);
			lenofs = buffer+8+chanlen+3+unamelen+1;
			sprintf(lenofs, "%03x", fulllen);

		}
		else
		{
			int unamelen;
			unamelen = strlen(to.address.irc.user);
			packed = 8 + unamelen + 3 + 3;

			if (packed+1 + newoutcount > sizeof(con->outbuf))
				break;

			memcpy(buffer, "PRIVMSG ", 8);
			memcpy(buffer+8, to.address.irc.user, unamelen);
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
	return true;
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

struct ftenet_generic_connection_s *FTENET_IRCConnect_EstablishConnection(qboolean isserver, const char *address)
{
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_ircconnect_connection_t *newcon;
	netadr_t adr;

	if (!NET_StringToAdr(address, 6667, &adr))
		return NULL;	//couldn't resolve the name



	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		newcon->generic.name = "IRCConnect";
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

#ifdef HAVE_WEBSOCKCL
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_websocket.h>
#include <ppapi/c/ppb_var.h>
#include <ppapi/c/ppb_instance.h>
extern PPB_Core *ppb_core;
extern PPB_WebSocket *ppb_websocket_interface;
extern PPB_Var *ppb_var_interface;
extern PP_Instance pp_instance;

typedef struct
{
	ftenet_generic_connection_t generic;

	PP_Resource sock;
	netadr_t remoteadr;

	qboolean havepacket;
	struct PP_Var incomingpacket;

	qboolean failed;
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

static void FTENET_WebSocket_Close(ftenet_generic_connection_t *gcon)
{
	int res;
	/*meant to free the memory too, in this case we get the callback to do it*/
	ftenet_websocket_connection_t *wsc = (void*)gcon;

	struct PP_CompletionCallback ccb = {websocketclosed, wsc, PP_COMPLETIONCALLBACK_FLAG_NONE};
	ppb_websocket_interface->Close(wsc->sock, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeUndefined(), ccb);
}

static qboolean FTENET_WebSocket_GetPacket(ftenet_generic_connection_t *gcon)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	int res;
	int len = 0;
	if (wsc->havepacket)
	{
		unsigned char *utf8 = (unsigned char *)ppb_var_interface->VarToUtf8(wsc->incomingpacket, &len);
		unsigned char *out = (unsigned char *)net_message_buffer;

		wsc->havepacket = false;
		memcpy(&net_from, &wsc->remoteadr, sizeof(net_from));

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
			Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (adr, sizeof(adr), net_from));
			return false;
		}
		return true;
	}
	return false;
}
static qboolean FTENET_WebSocket_SendPacket(ftenet_generic_connection_t *gcon, int length, void *data, netadr_t to)
{
	ftenet_websocket_connection_t *wsc = (void*)gcon;
	int res;
	int outchars = 0;
	unsigned char outdata[length*2+1];
	unsigned char *out=outdata, *in=data;
	if (wsc->failed)
		return false;

	while(length-->0)
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
	return true;
}

/*nacl websockets implementation...*/
static ftenet_generic_connection_t *FTENET_WebSocket_EstablishConnection(qboolean isserver, const char *address)
{
	ftenet_websocket_connection_t *newcon;

	netadr_t adr;
	PP_Resource newsocket;

	if (isserver || !ppb_websocket_interface)
	{
		return NULL;
	}
	if (!NET_StringToAdr(address, &adr))
		return NULL;	//couldn't resolve the name
	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		struct PP_CompletionCallback ccb = {websocketconnected, newcon, PP_COMPLETIONCALLBACK_FLAG_NONE};
		newsocket = ppb_websocket_interface->Create(pp_instance);
		struct PP_Var str = ppb_var_interface->VarFromUtf8(adr.address.websocketurl, strlen(adr.address.websocketurl));
		ppb_websocket_interface->Connect(newsocket, str, NULL, 0, ccb);
		ppb_var_interface->Release(str);
		newcon->generic.name = "WebSocket";
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


/*firstsock is a cookie*/
int NET_GetPacket (netsrc_t netsrc, int firstsock)
{
	ftenet_connections_t *collection;
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

			net_from.connum = firstsock+1;
			return firstsock;
		}

		firstsock += 1;
	}

	return -1;
}

int NET_LocalAddressForRemote(ftenet_connections_t *collection, netadr_t *remote, netadr_t *local, int idx)
{
	if (!remote->connum)
		return 0;

	if (!collection->conn[remote->connum-1])
		return 0;

	if (!collection->conn[remote->connum-1]->GetLocalAddress)
		return 0;

	return collection->conn[remote->connum-1]->GetLocalAddress(collection->conn[remote->connum-1], local, idx);
}

void NET_SendPacket (netsrc_t netsrc, int length, void *data, netadr_t to)
{
	char buffer[64];
	ftenet_connections_t *collection;
	int i;

	if (netsrc == NS_SERVER)
	{
#ifdef CLIENTONLY
		Sys_Error("NET_GetPacket: Bad netsrc");
		return;
#else
		collection = svs.sockets;
#endif
	}
	else
	{
#ifdef SERVERONLY
		Sys_Error("NET_GetPacket: Bad netsrc");
		return;
#else
		collection = cls.sockets;
#endif
	}

	if (!collection)
		return;

	if (net_fakeloss.value)
	{
		if (frandom () < net_fakeloss.value)
			return;
	}

	if (to.connum)
	{
		if (collection->conn[to.connum-1])
			if (collection->conn[to.connum-1]->SendPacket(collection->conn[to.connum-1], length, data, to))
				return;
	}

	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			continue;
		if (collection->conn[i]->SendPacket(collection->conn[i], length, data, to))
			return;
	}

	Con_Printf("No route to %s - try reconnecting\n", NET_AdrToString(buffer, sizeof(buffer), to));
}

qboolean NET_EnsureRoute(ftenet_connections_t *collection, char *routename, char *host, qboolean islisten)
{
	netadr_t adr;

	NET_StringToAdr(host, 0, &adr);

	switch(adr.type)
	{
#ifdef HAVE_WEBSOCKCL
	case NA_WEBSOCKET:
		if (!FTENET_AddToCollection(collection, routename, host, FTENET_WebSocket_EstablishConnection, islisten))
			return false;
		break;
#endif
#ifdef TCPCONNECT
	case NA_TCP:
		if (!FTENET_AddToCollection(collection, routename, host, FTENET_TCP4Connect_EstablishConnection, islisten))
			return false;
		break;
#ifdef IPPROTO_IPV6
	case NA_TCPV6:
		if (!FTENET_AddToCollection(collection, routename, host, FTENET_TCP6Connect_EstablishConnection, islisten))
			return false;
		break;
#endif
#endif
#ifdef IRCCONNECT
	case NA_IRC:
		if (!FTENET_AddToCollection(collection, routename, host, FTENET_IRCConnect_EstablishConnection, islisten))
			return false;
		break;
#endif
	default:
		//not recognised, or not needed
		break;
	}
	return true;
}

void NET_PrintAddresses(ftenet_connections_t *collection)
{
	int i;
	int adrno, adrcount=1;
	netadr_t adr;
	char adrbuf[MAX_ADR_SIZE];

	if (!collection)
		return;

	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			continue;
		if (collection->conn[i]->GetLocalAddress)
		{
			for (adrno = 0, adrcount=1; (adrcount = collection->conn[i]->GetLocalAddress(collection->conn[i], &adr, adrno)) && adrno < adrcount; adrno++)
			{
				Con_Printf("net address: %s\n", NET_AdrToString(adrbuf, sizeof(adrbuf), adr));
			}
		}
		else
			Con_Printf("%s\n", collection->conn[i]->name);
	}
}

//=============================================================================

int TCP_OpenStream (netadr_t remoteaddr)
{
#ifndef HAVE_TCP
	return INVALID_SOCKET;
#else
	unsigned long _true = true;
	int newsocket;
	int temp;
	struct sockaddr_qstorage qs;
//	struct sockaddr_qstorage loc;

	temp = NetadrToSockadr(&remoteaddr, &qs);

	if ((newsocket = socket (((struct sockaddr_in*)&qs)->sin_family, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		return INVALID_SOCKET;

//	memset(&loc, 0, sizeof(loc));
//	((struct sockaddr*)&loc)->sa_family = ((struct sockaddr*)&loc)->sa_family;
//	bind(newsocket, (struct sockaddr *)&loc, ((struct sockaddr_in*)&qs)->sin_family == AF_INET?sizeof(struct sockaddr_in):sizeof(struct sockaddr_in6));

	if (connect(newsocket, (struct sockaddr *)&qs, temp) == INVALID_SOCKET)
	{
		int err = qerrno;
		if (err == EADDRNOTAVAIL)
		{
			char buf[128];
			NET_AdrToString(buf, sizeof(buf), remoteaddr);
			if (remoteaddr.port == 0 && (remoteaddr.type == NA_IP || remoteaddr.type == NA_IPV6))
				Con_Printf ("TCP_OpenStream: no port specified\n");
			else
				Con_Printf ("TCP_OpenStream: invalid address trying to connect to %s\n", buf);
		}
		else
			Con_Printf ("TCP_OpenStream: connect: error %i\n", err);
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(qerrno));


	return newsocket;
#endif
}

int TCP_OpenListenSocket (int port)
{
#ifndef HAVE_TCP
	return INVALID_SOCKET;
#else
	int newsocket;
	struct sockaddr_in address;
	unsigned long _true = true;
	int i;
int maxport = port + 100;

	if ((newsocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("TCP_OpenListenSocket: ioctl FIONBIO: %s", strerror(qerrno));

	address.sin_family = AF_INET;
//ZOID -- check for interface binding option
	if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc) {
		address.sin_addr.s_addr = inet_addr(com_argv[i+1]);
		Con_TPrintf(TL_NETBINDINTERFACE,
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


#if defined(SV_MASTER) || defined(CL_MASTER)
int UDP_OpenSocket (int port, qboolean bcast)
{
	SOCKET newsocket;
	struct sockaddr_in address;
	unsigned long _true = true;
	int i;
int maxport = port + 100;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(qerrno));

	if (bcast)
	{
		_true = true;
		if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&_true, sizeof(_true)) == -1)
		{
			Con_Printf("Cannot create broadcast socket\n");
			return INVALID_SOCKET;
		}
	}

	address.sin_family = AF_INET;
//ZOID -- check for interface binding option
	if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc) {
		address.sin_addr.s_addr = inet_addr(com_argv[i+1]);
		Con_TPrintf(TL_NETBINDINTERFACE,
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
				Sys_Error ("UDP_OpenSocket: bind: %s", strerror(qerrno));
			port++;
			if (port > maxport)
				Sys_Error ("UDP_OpenSocket: bind: %s", strerror(qerrno));
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

	if ((newsocket = socket (PF_INET6, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		Con_Printf("IPV6 is not supported: %s\n", strerror(qerrno));
		return INVALID_SOCKET;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(qerrno));

	if (bcast)
	{
//		address.sin6_addr
//		_true = true;
//		if (setsockopt(newsocket, SOL_SOCKET, IP_ADD_MEMBERSHIP, (char *)&_true, sizeof(_true)) == -1)
//		{
			Con_Printf("Cannot create broadcast socket\n");
			closesocket(newsocket);
			return INVALID_SOCKET;
//		}
	}

#ifdef IPV6_V6ONLY
	setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_true, sizeof(_true));
#endif

	address.sin6_family = AF_INET6;
//ZOID -- check for interface binding option
//	if ((i = COM_CheckParm("-ip6")) != 0 && i < com_argc) {
//		address.sin6_addr = inet_addr(com_argv[i+1]);
///		Con_TPrintf(TL_NETBINDINTERFACE,
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
				err = qerrno;
				Con_Printf ("UDP6_OpenSocket: bind: (%i) %s", err, strerror(err));
				closesocket(newsocket);
				return INVALID_SOCKET;
			}
			port++;
			if (port > maxport)
			{
				err = qerrno;
				Con_Printf ("UDP6_OpenSocket: bind: (%i) %s", err, strerror(err));
				closesocket(newsocket);
				return INVALID_SOCKET;
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

	if ((newsocket = socket (PF_IPX, SOCK_DGRAM, NSPROTO_IPX)) == INVALID_SOCKET)
	{
		if (qerrno != EAFNOSUPPORT)
			Con_Printf ("WARNING: IPX_Socket: socket: %i\n", qerrno);
		return INVALID_SOCKET;
	}

	// make it non-blocking
	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
	{
		Con_Printf ("WARNING: IPX_Socket: ioctl FIONBIO: %i\n", qerrno);
		return INVALID_SOCKET;
	}

	if (bcast)
	{
		// make it broadcast capable
		if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&_true, sizeof(_true)) == -1)
		{
			Con_Printf ("WARNING: IPX_Socket: setsockopt SO_BROADCAST: %i\n", qerrno);
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
		Con_Printf ("WARNING: IPX_Socket: bind: %i\n", qerrno);
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
qboolean NET_Sleep(int msec, qboolean stdinissocket)
{
#ifdef HAVE_PACKET
    struct timeval timeout;
	fd_set	fdset;
	int maxfd;
	int con, sock;

	FD_ZERO(&fdset);

	if (stdinissocket)
		FD_SET(0, &fdset);	//stdin tends to be socket 0

	maxfd = 0;
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

	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(maxfd+1, &fdset, NULL, NULL, &timeout);

	if (stdinissocket)
		return FD_ISSET(0, &fdset);
#endif
	return true;
}
#endif

void NET_GetLocalAddress (int socket, netadr_t *out)
{
#ifndef HAVE_PACKET
	out->type = NA_INVALID;
#else
	char	buff[512];
	char	adrbuf[MAX_ADR_SIZE];
	struct sockaddr_qstorage	address;
	int		namelen;
	netadr_t adr = {0};
	qboolean notvalid = false;

	strcpy(buff, "localhost");
	gethostname(buff, 512);
	buff[512-1] = 0;

	if (!NET_StringToAdr (buff, 0, &adr))	//urm
		NET_StringToAdr ("127.0.0.1", 0, &adr);


	namelen = sizeof(address);
	if (getsockname (socket, (struct sockaddr *)&address, &namelen) == -1)
	{
		notvalid = true;
		NET_StringToSockaddr("0.0.0.0", 0, (struct sockaddr_qstorage *)&address, NULL, NULL);
//		Sys_Error ("NET_Init: getsockname:", strerror(qerrno));
	}

	SockadrToNetadr(&address, out);
	if (out->type == NA_IP)
	{
		if (!*(int*)out->address.ip)	//socket was set to auto
			*(int *)out->address.ip = *(int *)adr.address.ip;	//change it to what the machine says it is, rather than the socket.
	}

	if (notvalid)
		Con_Printf("Couldn't detect local ip\n");
	else
		Con_TPrintf(TL_IPADDRESSIS, NET_AdrToString (adrbuf, sizeof(adrbuf), *out) );
#endif
}

#ifndef CLIENTONLY
void SVNET_AddPort_f(void)
{
	netadr_t adr;
	char *s = Cmd_Argv(1);

	//just in case
	if (!svs.sockets)
	{
		svs.sockets = FTENET_CreateCollection(true);
#ifndef SERVERONLY
		FTENET_AddToCollection(svs.sockets, "SVLoopback", "27500", FTENET_Loop_EstablishConnection, true);
#endif
	}

#ifdef HAVE_IPV4
	NET_PortToAdr(AF_INET, s, &adr);
#else
	adr.type = NA_INVALID;
#endif

	switch(adr.type)
	{
#ifdef HAVE_IPV4
	case NA_IP:
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_UDP4_EstablishConnection, true);
		break;
#endif
#ifdef IPPROTO_IPV6
	case NA_IPV6:
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_UDP6_EstablishConnection, true);
		break;
#endif
	case NA_IPX:
#ifdef USEIPX
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_IPX_EstablishConnection, true);
#endif
		break;
#ifdef IRCCONNECT
	case NA_IRC:
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_IRCConnect_EstablishConnection, true);
		break;
#endif
#ifdef IRCCONNECT
	case NA_TCP:
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_TCP4Connect_EstablishConnection, true);
		break;
#ifdef IPPROTO_IPV6
	case NA_TCPV6:
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_TCP6Connect_EstablishConnection, true);
		break;
#endif
#endif
	// warning: enumeration value ‘NA_*’ not handled in switch
	case NA_WEBSOCKET:
	case NA_INVALID:
	case NA_LOOPBACK:
	case NA_BROADCAST_IP:
	case NA_BROADCAST_IP6:
	case NA_BROADCAST_IPX:
		break;
	}
}
#endif

/*
====================
NET_Init
====================
*/
void NET_Init (void)
{
#ifdef _WIN32
	WORD	wVersionRequested;
	int		r;
#ifdef IPPROTO_IPV6
	HMODULE ws2_32dll;
	ws2_32dll = LoadLibrary("ws2_32.dll");
	if (ws2_32dll)
	{
		pfreeaddrinfo = (void *)GetProcAddress(ws2_32dll, "freeaddrinfo");
		pgetaddrinfo = (void *)GetProcAddress(ws2_32dll, "getaddrinfo");
		if (!pgetaddrinfo || !pfreeaddrinfo)
		{
			pgetaddrinfo = NULL;
			pfreeaddrinfo = NULL;
            FreeLibrary(ws2_32dll);
		}
	}
	else
	    pgetaddrinfo = NULL;
#endif

	wVersionRequested = MAKEWORD(1, 1);

	r = WSAStartup (MAKEWORD(1, 1), &winsockdata);

	if (r)
		Sys_Error ("Winsock initialization failed.");
#endif

	Cvar_Register(&net_hybriddualstack, "networking");
	Cvar_Register(&net_fakeloss, "networking");

#ifndef CLIENTONLY
	Cmd_AddCommand("sv_addport", SVNET_AddPort_f);
#endif
}
#ifndef SERVERONLY
void NET_InitClient(void)
{
	const char *port;
	int p;
	port = STRINGIFY(PORT_QWCLIENT);

	p = COM_CheckParm ("-port");
	if (p && p < com_argc)
	{
		port = com_argv[p+1];
	}
	p = COM_CheckParm ("-clport");
	if (p && p < com_argc)
	{
		port = com_argv[p+1];
	}

	cls.sockets = FTENET_CreateCollection(false);
#ifndef CLIENTONLY
	FTENET_AddToCollection(cls.sockets, "CLLoopback", port, FTENET_Loop_EstablishConnection, false);
#endif
#ifdef HAVE_IPV4
	FTENET_AddToCollection(cls.sockets, "CLUDP4", port, FTENET_UDP4_EstablishConnection, true);
#endif
#ifdef IPPROTO_IPV6
	FTENET_AddToCollection(cls.sockets, "CLUDP6", port, FTENET_UDP6_EstablishConnection, true);
#endif
#ifdef USEIPX
	FTENET_AddToCollection(cls.sockets, "CLIPX", port, FTENET_IPX_EstablishConnection, true);
#endif

	//
	// init the message buffer
	//
	net_message.maxsize = sizeof(net_message_buffer);
	net_message.data = net_message_buffer;

	Con_TPrintf(TL_CLIENTPORTINITED);
}
#endif
#ifndef CLIENTONLY



#ifndef CLIENTONLY
#ifdef HAVE_IPV4
void SV_Tcpport_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, "SVTCP4", var->string, FTENET_TCP4Connect_EstablishConnection, true);
}
#endif
#ifdef IPPROTO_IPV6
void SV_Tcpport6_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, "SVTCP6", var->string, FTENET_TCP6Connect_EstablishConnection, true);
}
#endif

#ifdef HAVE_IPV4
void SV_Port_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, "SVUDP4", var->string, FTENET_UDP4_EstablishConnection, true);
}
#endif
#ifdef IPPROTO_IPV6
void SV_PortIPv6_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, "SVUDP6", var->string, FTENET_UDP6_EstablishConnection, true);
}
#endif
#ifdef USEIPX
void SV_PortIPX_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, "SVIPX", var->string, FTENET_IPX_EstablishConnection, true);
}
#endif
#endif

void NET_CloseServer(void)
{
	allowconnects = false;

	FTENET_CloseCollection(svs.sockets);
	svs.sockets = NULL;
}

void NET_InitServer(void)
{
	char *port;
	port = STRINGIFY(PORT_QWSERVER);

	if (sv_listen_nq.value || sv_listen_dp.value || sv_listen_qw.value || sv_listen_q3.value)
	{
		if (!svs.sockets)
		{
			svs.sockets = FTENET_CreateCollection(true);
	#ifndef SERVERONLY
			FTENET_AddToCollection(svs.sockets, "SVLoopback", port, FTENET_Loop_EstablishConnection, true);
	#endif
		}

		allowconnects = true;

#ifdef HAVE_IPV4
		Cvar_ForceCallback(&sv_port_ipv4);
#endif
#ifdef IPPROTO_IPV6
		Cvar_ForceCallback(&sv_port_ipv6);
#endif
#ifdef USEIPX
		Cvar_ForceCallback(&sv_port_ipx);
#endif
#ifdef TCPCONNECT
		Cvar_ForceCallback(&sv_port_tcp);
#ifdef IPPROTO_IPV6
		Cvar_ForceCallback(&sv_port_tcp6);
#endif
#endif
	}
	else
	{
		NET_CloseServer();

#ifndef SERVERONLY
		svs.sockets = FTENET_CreateCollection(true);
		FTENET_AddToCollection(svs.sockets, "SVLoopback", port, FTENET_Loop_EstablishConnection, true);
#endif
	}


	//
	// init the message buffer
	//
	net_message.maxsize = sizeof(net_message_buffer);
	net_message.data = net_message_buffer;
}
#endif
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


#ifdef _WIN32
#ifdef SERVERTONLY
	if (!serverthreadID)	//running as subsystem of client. Don't close all of it's sockets too.
#endif
		WSACleanup ();
#endif
}






#ifdef HAVE_TCP
typedef struct {
	vfsfile_t funcs;

	int sock;

	char readbuffer[65536];
	int readbuffered;
} tcpfile_t;
void VFSTCP_Error(tcpfile_t *f)
{
	if (f->sock != INVALID_SOCKET)
	{
		closesocket(f->sock);
		f->sock = INVALID_SOCKET;
	}
}
int VFSTCP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	tcpfile_t *tf = (tcpfile_t*)file;
	int len;
	int trying;

	if (tf->sock != INVALID_SOCKET)
	{
		trying = sizeof(tf->readbuffer) - tf->readbuffered;
		if (trying > 1500)
			trying = 1500;
		len = recv(tf->sock, tf->readbuffer + tf->readbuffered, trying, 0);
		if (len == -1)
		{
			if (errno != EWOULDBLOCK)
				printf("socket error\n");
			//fixme: figure out wouldblock or error
		}
		else if (len == 0 && trying != 0)
		{
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
			return -1;	//signal an error
		return 0;	//signal nothing available
	}
}
int VFSTCP_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
{
	tcpfile_t *tf = (tcpfile_t*)file;
	int len;

	if (tf->sock == INVALID_SOCKET)
		return 0;

	len = send(tf->sock, buffer, bytestoread, 0);
	if (len == -1 || len == 0)
	{
		VFSTCP_Error(tf);
		return 0;
	}
	return len;
}
qboolean VFSTCP_Seek (struct vfsfile_s *file, unsigned long pos)
{
	VFSTCP_Error((tcpfile_t*)file);
	return false;
}
unsigned long VFSTCP_Tell (struct vfsfile_s *file)
{
	VFSTCP_Error((tcpfile_t*)file);
	return 0;
}
unsigned long VFSTCP_GetLen (struct vfsfile_s *file)
{
	return 0;
}
void VFSTCP_Close (struct vfsfile_s *file)
{
	VFSTCP_Error((tcpfile_t*)file);
	Z_Free(file);
}

vfsfile_t *FS_OpenTCP(const char *name)
{
	tcpfile_t *newf;
	int sock;
	netadr_t adr = {0};
	if (NET_StringToAdr(name, 0, &adr))
	{
		sock = TCP_OpenStream(adr);
		if (sock == INVALID_SOCKET)
			return NULL;

		newf = Z_Malloc(sizeof(*newf));
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
vfsfile_t *FS_OpenTCP(const char *name)
{
	tcpfile_t *newf;

	netadr_t adr;

	if (!ppb_websocket_interface)
	{
		return NULL;
	}
	if (!NET_StringToAdr(name, &adr))
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
vfsfile_t *FS_OpenTCP(const char *name)
{
	return NULL;
}
#endif

