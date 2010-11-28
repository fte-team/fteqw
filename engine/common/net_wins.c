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

#pragma message("these two are never set. A NET_ReplySource function that returns the address a reply would originate from would be sufficient. Note that INADDR_ANY can be multiple however, so these are just a hint.")
netadr_t	net_local_cl_ipadr;	//still used to match local ui requests, and to generate ip reports for q3 servers.

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
extern cvar_t sv_port;
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

extern cvar_t sv_public, sv_listen_qw, sv_listen_nq, sv_listen_dp, sv_listen_q3;

static qboolean allowconnects = false;

#define	MAX_LOOPBACK	4
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
	case AF_INET:
		a->type = NA_IP;
		*(int *)&a->address.ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
		break;
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
	case AF_UNSPEC:
		memset(a, 0, sizeof(*a));
		a->type = NA_INVALID;
		break;
	default:
		Sys_Error("SockadrToNetadr: bad socket family");
	}
}

qboolean	NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return false;

	if (a.type == NA_LOOPBACK)
		return true;

	if (a.type == NA_IP || a.type == NA_BROADCAST_IP || a.type == NA_TCP)
	{
		if ((memcmp(a.address.ip, b.address.ip, sizeof(a.address.ip)) == 0) && a.port == b.port)
			return true;
		return false;
	}

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

	if (a.type == NA_IP)
	{
		if ((memcmp(a.address.ip, b.address.ip, sizeof(a.address.ip)) == 0))
			return true;
		return false;
	}
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
	int i;

	//rejects certain blacklisted addresses
	switch(a.type)
	{
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
	qboolean doneblank;
	char *p;
	int i;

	switch(a.type)
	{
#ifdef TCPCONNECT
	case NA_TCP:
		if (len < 7)
			return "?";
		snprintf (s, len, "tcp://");
		s += 6;
		len -= 6;
		//fallthrough
#endif
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
		snprintf (s, len, "LocalHost");
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
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
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
#define DO(src,dest)	\
	copy[0] = s[src];	\
	copy[1] = s[src + 1];	\
	sscanf (copy, "%x", &val);	\
	((struct sockaddr_ipx *)sadr)->dest = val

qboolean	NET_StringToSockaddr (const char *s, struct sockaddr_qstorage *sadr)
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
		((struct sockaddr_ipx *)sadr)->sa_socket = htons((unsigned short)val);
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
			port = strstr(s, "]:");
			if (!port)
				error = EAI_NONAME;
			else
			{
				len = port - (s+1);
				if (len >= sizeof(dupbase))
					len = sizeof(dupbase)-1;
				strncpy(dupbase, s+1, len);
				dupbase[len] = '\0';
				error = pgetaddrinfo(dupbase, port+2, &udp6hint, &addrinfo);
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
			case AF_INET:
				memcpy(sadr, pos->ai_addr, pos->ai_addrlen);
				if (pos->ai_family == AF_INET)
					goto dblbreak;	//don't try finding any more, this is quake, they probably prefer ip4...
				break;
			}
		}
dblbreak:
		pfreeaddrinfo (addrinfo);
		if (!((struct sockaddr*)sadr)->sa_family)	//none suitablefound
			return false;
	}
	else
#endif
	{
		((struct sockaddr_in *)sadr)->sin_family = AF_INET;

		((struct sockaddr_in *)sadr)->sin_port = 0;

		if (strlen(s) >= sizeof(copy)-1)
			return false;
	
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
	}

	return true;
}

#undef DO

/*
accepts anything that NET_StringToSockaddr accepts plus certain url schemes
including: tcp, irc
*/
qboolean	NET_StringToAdr (const char *s, netadr_t *a)
{
	struct sockaddr_qstorage sadr;

	Con_DPrintf("Resolving address: %s\n", s);

#ifdef TCPCONNECT
	if (!strncmp (s, "tcp://", 6))
	{
		//make sure that the rest of the address is a valid ip address (4 or 6)

		if (!NET_StringToSockaddr (s+6, &sadr))
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

	if (!strcmp (s, "internalserver"))
	{
		memset (a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	if (!NET_StringToSockaddr (s, &sadr))
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
#ifdef USEIPX
	case NA_IPX:
	case NA_BROADCAST_IPX:
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
		if (!ParsePartialIPv4(t, a) && !NET_StringToAdr(t, a))
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
			return ParsePartialIPv4(spoint, amask) || NET_StringToAdr(spoint, amask);

		// otherwise generate mask for given bits
		i = atoi(spoint);
		NET_IntegerToMask(a, amask, i);
	}
	else
	{
		// we don't have a slash, resolve and fill with a full mask
		i = ParsePartialIPv4(s, a);
		if (!i && !NET_StringToAdr(s, a))
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
		if (a.type == NA_IP && b.type == NA_IPV6)
		{
#ifndef _MSC_VER
#warning code me
#endif
			//okay, comparing an ipv4 address against an ipv4-as-6
		/*	for (i = 0; i < 10; i++)
				if (mask.address.ip[i] != 0)
					return false;

			for (; i < 12; i++)
			{
				if (mask.address.ip[i] != 0xff)
			}
			if (i == 12)
			{

			}*/
		}
		if (a.type == NA_IPV6 && b.type == NA_IP)
		{
			for (i = 0; i < 10; i++)
				if (a.address.ip[i] != 0)
					return false;	//only matches if they're 0s, otherwise its not an ipv4 address there

			for (; i < 12; i++)
				if (a.address.ip[i] != 0xff && a.address.ip[i] != 0x00)	//0x00 is depricated
					return false;	//only matches if they're 0s, otherwise its not an ipv4 address there

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

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
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
		loop->get = loop->send - MAX_LOOPBACK;

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
				break;
			}
		}
	}

	if (!address || !*address)
	{
		return true;	//must have at least a port.
	}

	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!col->conn[i])
		{
			col->conn[i] = establish(islisten, address);
			if (!col->conn[i])
				return false;
			col->conn[i]->name = name;
			return true;
		}
	}
	return false;
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
	if (con->thesocket != INVALID_SOCKET)
		closesocket(con->thesocket);
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
	struct sockaddr_qstorage	from;
	int fromsize = sizeof(from);
	netadr_t adr;
	char		adrs[MAX_ADR_SIZE];
	int b;
	struct hostent *h;
	int idx = 0;

	if (getsockname (con->thesocket, (struct sockaddr*)&from, &fromsize) != -1)
	{
		memset(&adr, 0, sizeof(adr));
		SockadrToNetadr(&from, &adr);

		for (b = 0; b < sizeof(adr.address); b++)
			if (((unsigned char*)&adr.address)[b] != 0)
				break;
		if (b == sizeof(adr.address))
		{
			gethostname(adrs, sizeof(adrs));
			h = gethostbyname(adrs);
			b = 0;
			if(h && h->h_addrtype == AF_INET)
			{
				for (b = 0; h->h_addr_list[b]; b++)
				{
					memcpy(&((struct sockaddr_in*)&from)->sin_addr, h->h_addr_list[b], sizeof(((struct sockaddr_in*)&from)->sin_addr));
					SockadrToNetadr(&from, &adr);
					if (idx++ == count)
						*out = adr;
				}
			}
#ifdef IPPROTO_IPV6
			else if(h && h->h_addrtype == AF_INET6)
			{
				for (b = 0; h->h_addr_list[b]; b++)
				{
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
		else
		{
			if (idx++ == count)
				*out = adr;
		}
	}

	return idx;
}

qboolean FTENET_Generic_GetPacket(ftenet_generic_connection_t *con)
{
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
}

qboolean FTENET_Generic_SendPacket(ftenet_generic_connection_t *con, int length, void *data, netadr_t to)
{
	struct sockaddr_qstorage	addr;
	int size;
	int ret;

	for (size = 0; size < FTENET_ADDRTYPES; size++)
		if (to.type == con->addrtype[size])
			break;
	if (size == FTENET_ADDRTYPES)
		return false;


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

	ret = sendto (con->thesocket, data, length, 0, (struct sockaddr*)&addr, size );
	if (ret == -1)
	{
// wouldblock is silent
		if (qerrno == EWOULDBLOCK)
			return true;

		if (qerrno == ECONNREFUSED)
			return true;

#ifndef SERVERONLY
		if (qerrno == EADDRNOTAVAIL)
			Con_DPrintf("NET_SendPacket Warning: %i\n", qerrno);
		else
#endif
			Con_TPrintf (TL_NETSENDERROR, qerrno);
	}
	return true;
}

qboolean	NET_PortToAdr (int adrfamily, const char *s, netadr_t *a)
{
	char *e;
	int port;
	port = strtoul(s, &e, 10);
	if (*e)
		return NET_StringToAdr(s, a);
	else if (port)
	{
		memset(a, 0, sizeof(*a));
		a->port = htons((unsigned short)port);
		if (adrfamily == AF_INET)
			a->type = NA_IP;
#ifdef IPPROTO_IPV6
		else if (adrfamily == AF_INET6)
			a->type = NA_IPV6;
#endif
#ifdef USEIPX
		else if (adrfamily == AF_IPX)
			a->type = NA_IPX;
#endif
		else
		{
			a->type = NA_INVALID;
			return false;
		}
		return true;
	}
	a->type = NA_INVALID;
	return false;
}

ftenet_generic_connection_t *FTENET_Generic_EstablishConnection(int adrfamily, int protocol, qboolean isserver, const char *address)
{
	//this is written to support either ipv4 or ipv6, depending on the remote addr.
	ftenet_generic_connection_t *newcon;

	unsigned long _true = true;
	int newsocket;
	int temp;
	netadr_t adr;
	struct sockaddr_qstorage qs;
	int family;
	int port;
	int bindtries;


	if (!NET_PortToAdr(adrfamily, address, &adr))
	{
		Con_Printf("unable to resolve local address %s\n", address);
		return NULL;	//couldn't resolve the name
	}
	temp = NetadrToSockadr(&adr, &qs);
	family = ((struct sockaddr_in*)&qs)->sin_family;

	if ((newsocket = socket (family, SOCK_DGRAM, protocol)) == INVALID_SOCKET)
	{
		return NULL;
	}

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

	newcon = Z_Malloc(sizeof(*newcon));
	if (newcon)
	{
		newcon->name = "Generic";
		newcon->GetLocalAddress = FTENET_Generic_GetLocalAddress;
		newcon->GetPacket = FTENET_Generic_GetPacket;
		newcon->SendPacket = FTENET_Generic_SendPacket;
		newcon->Close = FTENET_Generic_Close;

		newcon->islisten = isserver;
		newcon->addrtype[0] = adr.type;
		newcon->addrtype[1] = NA_INVALID;

		newcon->thesocket = newsocket;

		return newcon;
	}
	else
	{
		closesocket(newsocket);
		return NULL;
	}
}

#ifdef IPPROTO_IPV6
ftenet_generic_connection_t *FTENET_UDP6_EstablishConnection(qboolean isserver, const char *address)
{
	return FTENET_Generic_EstablishConnection(AF_INET6, IPPROTO_UDP, isserver, address);
}
#endif
ftenet_generic_connection_t *FTENET_UDP4_EstablishConnection(qboolean isserver, const char *address)
{
	return FTENET_Generic_EstablishConnection(AF_INET, IPPROTO_UDP, isserver, address);
}
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
	qboolean waitingforprotocolconfirmation;
	char inbuffer[1500];
	float timeouttime;
	netadr_t remoteaddr;
	struct ftenet_tcpconnect_stream_s *next;
} ftenet_tcpconnect_stream_t;

typedef struct {
	ftenet_generic_connection_t generic;

	int active;
	ftenet_tcpconnect_stream_t *tcpstreams;
} ftenet_tcpconnect_connection_t;

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

		if (st->waitingforprotocolconfirmation)
		{
			if (st->inlen < 6)
				continue;

			if (strncmp(st->inbuffer, "qizmo\n", 6))
			{
				Con_Printf ("Unknown TCP client\n");
				goto closesvstream;
			}

			memmove(st->inbuffer, st->inbuffer+6, st->inlen - (6));
			st->inlen -= 6;
			st->waitingforprotocolconfirmation = false;
		}

		if (st->inlen < 2)
			continue;

		net_message.cursize = BigShort(*(short*)st->inbuffer);
		if (net_message.cursize >= sizeof(net_message_buffer) )
		{
			Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (adr, sizeof(adr), net_from));
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
	}

	if (con->generic.thesocket != INVALID_SOCKET && con->active < 256)
	{
		int newsock;
		newsock = accept(con->generic.thesocket, (struct sockaddr*)&from, &fromlen);
		if (newsock != INVALID_SOCKET)
		{
			int _true = true;
			ioctlsocket(newsock, FIONBIO, &_true);
			setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));

			con->active++;
			st = Z_Malloc(sizeof(*con->tcpstreams));
			st->waitingforprotocolconfirmation = true;
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

			//send the qizmo greeting.
			send(newsock, "qizmo\n", 6, 0);

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
			unsigned short slen = BigShort((unsigned short)length);
			send(st->socketnum, (char*)&slen, sizeof(slen), 0);
			send(st->socketnum, data, length, 0);

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

	if (isserver)
	{
		if (!NET_PortToAdr(affamily, address, &adr))
			return NULL;	//couldn't resolve the name
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

		newcon->generic.islisten = true;
		newcon->generic.addrtype[0] = adr.type;
		newcon->generic.addrtype[1] = NA_INVALID;

		newcon->active = 0;

		if (!isserver)
		{
			newcon->generic.thesocket = INVALID_SOCKET;

			newcon->active++;
			newcon->tcpstreams = Z_Malloc(sizeof(*newcon->tcpstreams));
			newcon->tcpstreams->waitingforprotocolconfirmation = true;
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
			cvar_t *ircchannel = Cvar_Get("ircchannel", "#ftetest", 0, "IRC Connect");
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
			code = strtoul(s, &s, 10);
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

	if (!NET_StringToAdr(address, &adr))
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

qboolean NET_GetPacket (netsrc_t netsrc)
{
	int i;
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
		return false;

	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!collection->conn[i])
			break;
		if (collection->conn[i]->GetPacket(collection->conn[i]))
		{
			net_from.connum = i+1;
			return true;
		}
	}

	return false;
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

	Con_Printf("No route - open some ports\n");
}

void NET_EnsureRoute(ftenet_connections_t *collection, char *routename, char *host, qboolean islisten)
{
	netadr_t adr;
	NET_StringToAdr(host, &adr);

	switch(adr.type)
	{
#ifdef TCPCONNECT
	case NA_TCP:
		FTENET_AddToCollection(collection, routename, host, FTENET_TCP4Connect_EstablishConnection, islisten);
		break;
#ifdef IPPROTO_IPV6
	case NA_TCPV6:
		FTENET_AddToCollection(collection, routename, host, FTENET_TCP6Connect_EstablishConnection, islisten);
		break;
#endif
#endif
#ifdef IRCCONNECT
	case NA_IRC:
		FTENET_AddToCollection(collection, routename, host, FTENET_IRCConnect_EstablishConnection, islisten);
		break;
#endif
	default:
		//not recognised, or not needed
		break;
	}
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
			for (adrno = 0, adrcount=1; adrcount = collection->conn[i]->GetLocalAddress(collection->conn[i], &adr, adrno) && adrno < adrcount; adrno++)
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
	unsigned long _true = true;
	int newsocket;
	int temp;
	struct sockaddr_qstorage qs;

	temp = NetadrToSockadr(&remoteaddr, &qs);

	if ((newsocket = socket (((struct sockaddr_in*)&qs)->sin_family, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (connect(newsocket, (struct sockaddr *)&qs, temp) == INVALID_SOCKET)
	{
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror(qerrno));


	return newsocket;
}

int TCP_OpenListenSocket (int port)
{
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
}



int UDP_OpenSocket (int port, qboolean bcast)
{
	int newsocket;
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
	int newsocket;
	struct sockaddr_in6 address;
	unsigned long _true = true;
//	int i;
int maxport = port + 100;

	memset(&address, 0, sizeof(address));

	if ((newsocket = socket (PF_INET6, SOCK_DGRAM, 0)) == -1)
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
	int					newsocket;
	struct sockaddr_ipx	address;
	u_long					_true = 1;

	if ((newsocket = socket (PF_IPX, SOCK_DGRAM, NSPROTO_IPX)) == -1)
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

// sleeps msec or until net socket is ready
//stdin can sometimes be a socket. As a result,
//we give the option to select it for nice console imput with timeouts.
#ifndef CLIENTONLY
qboolean NET_Sleep(int msec, qboolean stdinissocket)
{
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
		sock = svs.sockets->conn[con]->thesocket;
		if (sock != INVALID_SOCKET)
		{
			FD_SET(sock, &fdset); // network socket
			if (sock > maxfd)
				maxfd = sock;
		}
	}

	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(maxfd+1, &fdset, NULL, NULL, &timeout);

	if (stdinissocket)
		return FD_ISSET(0, &fdset);
	return true;
}
#endif

void NET_GetLocalAddress (int socket, netadr_t *out)
{
	char	buff[512];
	char	adrbuf[MAX_ADR_SIZE];
	struct sockaddr_qstorage	address;
	int		namelen;
	netadr_t adr = {0};
	qboolean notvalid = false;

	strcpy(buff, "localhost");
	gethostname(buff, 512);
	buff[512-1] = 0;

	if (!NET_StringToAdr (buff, &adr))	//urm
		NET_StringToAdr ("127.0.0.1", &adr);


	namelen = sizeof(address);
	if (getsockname (socket, (struct sockaddr *)&address, &namelen) == -1)
	{
		notvalid = true;
		NET_StringToSockaddr("0.0.0.0", (struct sockaddr_qstorage *)&address);
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
}

#ifndef CLIENTONLY
void SVNET_AddPort(void)
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

	NET_PortToAdr(AF_INET, s, &adr);

	switch(adr.type)
	{
	case NA_IP:
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_UDP4_EstablishConnection, true);
		break;
#ifdef IPPROTO_IPV6
	case NA_IPV6:
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_UDP6_EstablishConnection, true);
		break;
#endif
#ifdef USEIPX
	case NA_IPX:
		FTENET_AddToCollection(svs.sockets, NULL, s, FTENET_IPX_EstablishConnection, true);
		break;
#endif
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

	Con_TPrintf(TL_UDPINITED);
}
#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)
#ifndef SERVERONLY
void NET_InitClient(void)
{
	const char *port;
	int p;
	port = STRINGIFY(PORT_CLIENT);

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
	FTENET_AddToCollection(cls.sockets, "CLUDP4", port, FTENET_UDP4_EstablishConnection, true);
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

	//
	// determine my name & address
	//
//	NET_GetLocalAddress (cls.socketip, &net_local_cl_ipadr);

	Con_TPrintf(TL_CLIENTPORTINITED);
}
#endif
#ifndef CLIENTONLY



#ifndef CLIENTONLY
void SV_Tcpport_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, "SVTCP4", var->string, FTENET_TCP4Connect_EstablishConnection, true);
}
#ifdef IPPROTO_IPV6
void SV_Tcpport6_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, "SVTCP6", var->string, FTENET_TCP6Connect_EstablishConnection, true);
}
#endif

void SV_Port_Callback(struct cvar_s *var, char *oldvalue)
{
	FTENET_AddToCollection(svs.sockets, "SVUDP4", var->string, FTENET_UDP4_EstablishConnection, true);
}
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
	port = STRINGIFY(PORT_SERVER);

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

		Cvar_ForceCallback(&sv_port);
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
	if (NET_StringToAdr(name, &adr))
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
