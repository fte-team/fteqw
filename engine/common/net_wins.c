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

netadr_t	net_local_cl_ipadr;
netadr_t	net_local_cl_ip6adr;
netadr_t	net_local_cl_ipxadr;
netadr_t	net_local_sv_ipadr;
netadr_t	net_local_sv_ip6adr;
netadr_t	net_local_sv_ipxadr;
netadr_t	net_local_sv_tcpipadr;

netadr_t	net_from;
sizebuf_t	net_message;

//#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)	// one more than msg + header
#define	MAX_UDP_PACKET	8192	// one more than msg + header
qbyte		net_message_buffer[MAX_UDP_PACKET];
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
#endif

extern cvar_t sv_public, sv_listen_qw, sv_listen_nq, sv_listen_dp;

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

	case NA_IPV6:
		memset (s, 0, sizeof(struct sockaddr_in));
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

	if (a.type == NA_IP || a.type == NA_BROADCAST_IP)
	{
		if ((memcmp(a.address.ip, b.address.ip, sizeof(a.address.ip)) == 0) && a.port == b.port)
			return true;
		return false;
	}

#ifdef IPPROTO_IPV6
	if (a.type == NA_IPV6 || a.type == NA_BROADCAST_IP6)
	{
		if ((memcmp(a.address.ip6, b.address.ip6, sizeof(a.address.ip6)) == 0) && a.port == b.port)
			return true;
		return false;
	}
#endif

	if (a.type == NA_IPX || a.type == NA_BROADCAST_IPX)
	{
		if ((memcmp(a.address.ipx, b.address.ipx, sizeof(a.address.ipx)) == 0) && a.port == b.port)
			return true;
		return false;
	}

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
	if (a.type == NA_IPX)
	{
		if ((memcmp(a.address.ipx, b.address.ipx, 10) == 0))
			return true;
		return false;
	}

	Sys_Error("NET_CompareBaseAdr: Bad address type");
	return false;
}

char	*NET_AdrToString (netadr_t a)
{
	static	char	s[64];

	switch(a.type)
	{
	case NA_BROADCAST_IP:
	case NA_IP:
		sprintf (s, "%i.%i.%i.%i:%i", 
			a.address.ip[0], 
			a.address.ip[1], 
			a.address.ip[2], 
			a.address.ip[3], 
			ntohs(a.port));
		break;
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
		sprintf (s, "[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]:%i", 
			a.address.ip6[0], 
			a.address.ip6[1], 
			a.address.ip6[2], 
			a.address.ip6[3], 
			a.address.ip6[4], 
			a.address.ip6[5],
			a.address.ip6[6],
			a.address.ip6[7],
			a.address.ip6[8],
			a.address.ip6[9],
			a.address.ip6[10],
			a.address.ip6[11],
			a.address.ip6[12],
			a.address.ip6[13],
			a.address.ip6[14],
			a.address.ip6[15],
			ntohs(a.port));
		break;
#endif
#ifdef USEIPX
	case NA_BROADCAST_IPX:
	case NA_IPX:
		sprintf (s, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i",
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
		sprintf (s, "LocalHost");
		break;
	default:
		sprintf (s, "invalid netadr_t type");
//		Sys_Error("NET_AdrToString: Bad netadr_t type");
	}

	return s;
}

char	*NET_BaseAdrToString (netadr_t a)
{
	static	char	s[64];

	switch(a.type)
	{
	case NA_BROADCAST_IP:
	case NA_IP:
		sprintf (s, "%i.%i.%i.%i", 
			a.address.ip[0], 
			a.address.ip[1],
			a.address.ip[2],
			a.address.ip[3]);
		break;
#ifdef IPPROTO_IPV6
	case NA_BROADCAST_IP6:
	case NA_IPV6:
		sprintf (s, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x", 
			a.address.ip6[0],
			a.address.ip6[1],
			a.address.ip6[2],
			a.address.ip6[3],
			a.address.ip6[4],
			a.address.ip6[5],
			a.address.ip6[6], 
			a.address.ip6[7],
			a.address.ip6[8],
			a.address.ip6[9],
			a.address.ip6[10],
			a.address.ip6[11],
			a.address.ip6[12],
			a.address.ip6[13],
			a.address.ip6[14],
			a.address.ip6[15]);
		break;
#endif
#ifdef USEIPX
	case NA_BROADCAST_IPX:
	case NA_IPX:
		sprintf (s, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x", 
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
		sprintf (s, "LocalHost");
		break;
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

qboolean	NET_StringToSockaddr (char *s, struct sockaddr_qstorage *sadr)
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
				error = pgetaddrinfo(s+1, port+2, &udp6hint, &addrinfo);
			}
		}
		else
		{
			port = s + strlen(s);
			while(port >= s)
			{
				if (*port == ':')
				break;
				port--;
			}

			if (port == s)
				port = NULL;
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
				memcpy(sadr, addrinfo->ai_addr, addrinfo->ai_addrlen);
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

qboolean	NET_StringToAdr (char *s, netadr_t *a)
{
	struct sockaddr_qstorage sadr;

	if (!strcmp (s, "internalserver"))
	{
		memset (a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	if (!NET_StringToSockaddr (s, &sadr))
		return false;

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
	}
}

// ParsePartialIPv4: check string to see if it is a partial IPv4 address and
// return bits to mask and set netadr_t or 0 if not an address
int ParsePartialIPv4(char *s, netadr_t *a)
{
	char *colon = NULL;
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
qboolean NET_StringToAdrMasked (char *s, netadr_t *a, netadr_t *amask)
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

	// check to make sure all types match
	if (a.type != b.type || a.type != mask.type)
		return false;

	// check port if both are non-zero
	if (a.port && b.port && a.port != b.port)
		return false;

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

char	*NET_AdrToStringMasked (netadr_t a, netadr_t amask)
{
	static	char	s[128];
	int i;

	i = UniformMaskedBits(amask);

	if (i >= 0)
		sprintf(s, "%s/%i", NET_AdrToString(a), i);
	else
	{
		// has to be done this way due to NET_AdrToString returning a
		// static address
		Q_strncatz(s, NET_AdrToString(a), sizeof(s));
		Q_strncatz(s, "/", sizeof(s));
		Q_strncatz(s, NET_AdrToString(amask), sizeof(s));
	}

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

#ifndef CLIENTONLY
void SV_Tcpport_Callback(struct cvar_s *var, char *oldvalue)
{
#ifdef TCPCONNECT
	if (!allowconnects)
		return;

	if (var->value)
	{
		if (svs.sockettcp == INVALID_SOCKET)
		{
			svs.sockettcp = TCP_OpenListenSocket(var->value);
			if (svs.sockettcp != INVALID_SOCKET)
				NET_GetLocalAddress (svs.sockettcp, &net_local_sv_tcpipadr);
			else
				Con_Printf("Failed to open TCP port %i\n", (int)var->value);
		}
	}
	else
	{
		if (svs.sockettcp != INVALID_SOCKET)
		{
			closesocket(svs.sockettcp);
			svs.sockettcp = INVALID_SOCKET;
		}
	}
#endif
}

void SV_Port_Callback(struct cvar_s *var, char *oldvalue)
{
	if (!allowconnects)
		return;
	
	if (var->value)
	{
		if (svs.socketip == INVALID_SOCKET)
		{
			svs.socketip = UDP_OpenSocket (var->value, false);
			if (svs.socketip != INVALID_SOCKET)
				NET_GetLocalAddress (svs.socketip, &net_local_sv_ipadr);
		}
	}
	else
	{
		if (svs.socketip != INVALID_SOCKET)
		{
			UDP_CloseSocket(svs.socketip);
			svs.socketip = INVALID_SOCKET;
		}
	}
}

void SV_PortIPv6_Callback(struct cvar_s *var, char *oldvalue)
{
#ifdef IPPROTO_IPV6
	if (!allowconnects)
		return;

	if (var->value)
	{
		if (svs.socketip6 == INVALID_SOCKET)
		{
			svs.socketip6 = UDP6_OpenSocket (var->value, false);
			if (svs.socketip6 != INVALID_SOCKET)
				NET_GetLocalAddress (svs.socketip6, &net_local_sv_ip6adr);
		}
	}
	else
	{
		if (svs.socketip6 != INVALID_SOCKET)
		{
			UDP_CloseSocket(svs.socketip6);
			svs.socketip6 = INVALID_SOCKET;
		}
	}
#endif
}

void SV_PortIPX_Callback(struct cvar_s *var, char *oldvalue)
{
#ifdef USEIPX
	if (!allowconnects)
		return;

	if (var->value)
	{
		if (svs.socketipx == INVALID_SOCKET)
		{
			svs.socketipx = IPX_OpenSocket (var->value, false);
			if (svs.socketipx != INVALID_SOCKET)
				NET_GetLocalAddress (svs.socketipx, &net_local_sv_ipxadr);
		}
	}
	else
	{
		if (svs.socketipx != INVALID_SOCKET)
		{
			IPX_CloseSocket(svs.socketipx);
			svs.socketipx = INVALID_SOCKET;
		}
	}
#endif
}
#endif

qboolean NET_GetPacket (netsrc_t netsrc)
{
	int 	ret;
	struct sockaddr_qstorage	from;
	int		fromlen;
	int i;
	int		socket;
	int err;

	if (NET_GetLoopPacket(netsrc, &net_from, &net_message))
		return true;

	for (i = 0; i < 3; i++)
	{
		if (netsrc == NS_SERVER)
		{
#ifdef CLIENTONLY
			Sys_Error("NET_GetPacket: Bad netsrc");
			socket = 0;
#else
			if (i == 0)
				socket = svs.socketip;
			else if (i == 1)
				socket = svs.socketip6;
			else
				socket = svs.socketipx;
#endif
		}
		else
		{
#ifdef SERVERONLY
			Sys_Error("NET_GetPacket: Bad netsrc");
			socket = 0;
#else
			if (i == 0)
				socket = cls.socketip;
			else if (i == 1)
				socket = cls.socketip6;
			else
				socket = cls.socketipx;
#endif
		}
		if (socket == INVALID_SOCKET)
			continue;

		fromlen = sizeof(from);
		ret = recvfrom (socket, (char *)net_message_buffer, sizeof(net_message_buffer), 0, (struct sockaddr*)&from, &fromlen);

		if (ret == -1)
		{
			err = qerrno;

			if (err == EWOULDBLOCK)
				continue;
			if (err == EMSGSIZE)
			{
				SockadrToNetadr (&from, &net_from);
				Con_TPrintf (TL_OVERSIZEPACKETFROM,
					NET_AdrToString (net_from));
				continue;
			}
			if (err == ECONNABORTED || err == ECONNRESET)
			{
				Con_TPrintf (TL_CONNECTIONLOSTORABORTED);	//server died/connection lost.
#ifndef SERVERONLY
				if (cls.state != ca_disconnected && netsrc == NS_CLIENT)
				{
					if (cls.lastarbiatarypackettime+5 < Sys_DoubleTime())	//too many mvdsv
						Cbuf_AddText("disconnect\nreconnect\n", RESTRICT_LOCAL);	//retry connecting.
					else
						Con_Printf("Packet was not delivered - server might be badly configured\n");
					break;
				}
#endif
				continue;
			}


			Con_Printf ("NET_GetPacket: Error (%i): %s\n", err, strerror(err));
			continue;
		}
 		SockadrToNetadr (&from, &net_from);

		net_message.packing = SZ_RAWBYTES;
		net_message.currentbit = 0;
		net_message.cursize = ret;
		if (net_message.cursize == sizeof(net_message_buffer) )
		{
			Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (net_from));
			continue;
		}

		return ret;
	}

#ifdef TCPCONNECT
#ifndef SERVERONLY
	if (netsrc == NS_CLIENT)
	{
		if (cls.sockettcp != INVALID_SOCKET)
		{//client receiving only via tcp

			ret = recv(cls.sockettcp, cls.tcpinbuffer+cls.tcpinlen, sizeof(cls.tcpinbuffer)-cls.tcpinlen, 0);
			if (ret == -1)
			{
				err = qerrno;

				if (err == EWOULDBLOCK)
					ret = 0;
				else
				{
					if (err == ECONNABORTED || err == ECONNRESET)
					{
						closesocket(cls.sockettcp);
						cls.sockettcp = INVALID_SOCKET;
						Con_TPrintf (TL_CONNECTIONLOSTORABORTED);	//server died/connection lost.

						if (cls.state != ca_disconnected)
						{
							if (cls.lastarbiatarypackettime+5 < Sys_DoubleTime())	//too many mvdsv
								Cbuf_AddText("disconnect\nreconnect\n", RESTRICT_LOCAL);	//retry connecting.
							else
								Con_Printf("Packet was not delivered - server might be badly configured\n");
							return false;
						}
						return false;
					}


					closesocket(cls.sockettcp);
					cls.sockettcp = INVALID_SOCKET;
					Con_Printf ("NET_GetPacket: Error (%i): %s\n", err, strerror(err));
					return false;
				}
			}
			cls.tcpinlen += ret;

			if (cls.tcpinlen < 2)
				return false;

			net_message.cursize = BigShort(*(short*)cls.tcpinbuffer);
			if (net_message.cursize >= sizeof(net_message_buffer) )
			{
				closesocket(cls.sockettcp);
				cls.sockettcp = INVALID_SOCKET;
				Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (net_from));
				return false;
			}
			if (net_message.cursize+2 > cls.tcpinlen)
			{	//not enough buffered to read a packet out of it.
				return false;
			}

			memcpy(net_message_buffer, cls.tcpinbuffer+2, net_message.cursize);
			memmove(cls.tcpinbuffer, cls.tcpinbuffer+net_message.cursize+2, cls.tcpinlen - (net_message.cursize+2));
			cls.tcpinlen -= net_message.cursize+2;

			net_message.packing = SZ_RAWBYTES;
			net_message.currentbit = 0;
			net_from = cls.sockettcpdest;

			return true;
		}
	}
#endif
#ifndef CLIENTONLY
	if (netsrc == NS_SERVER)
	{
		float timeval = Sys_DoubleTime();
		svtcpstream_t *st;
		st = svs.tcpstreams;

		while (svs.tcpstreams && svs.tcpstreams->socketnum == INVALID_SOCKET)
		{
			st = svs.tcpstreams;
			svs.tcpstreams = svs.tcpstreams->next;
			BZ_Free(st);
		}

		for (st = svs.tcpstreams; st; st = st->next)
		{//client receiving only via tcp

			while (st->next && st->next->socketnum == INVALID_SOCKET)
			{
				svtcpstream_t *temp;
				temp = st->next;
				st->next = st->next->next;
				BZ_Free(temp);
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
						Con_Printf ("NET_GetPacket: Error (%i): %s\n", err, strerror(err));

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
				Con_TPrintf (TL_OVERSIZEPACKETFROM, NET_AdrToString (net_from));
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

		if (svs.sockettcp != INVALID_SOCKET)
		{
			int newsock;
			newsock = accept(svs.sockettcp, (struct sockaddr*)&from, &fromlen);
			if (newsock != INVALID_SOCKET)
			{
				int _true = true;
				setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true));



				st = Z_Malloc(sizeof(svtcpstream_t));
				st->waitingforprotocolconfirmation = true;
				st->next = svs.tcpstreams;
				svs.tcpstreams = st;
				st->socketnum = newsock;
				st->inlen = 0;
				SockadrToNetadr(&from, &st->remoteaddr);
				send(newsock, "qizmo\n", 6, 0);

				st->timeouttime = timeval + 30;
			}
		}
	}
#endif
#endif


	return false;
}

//=============================================================================

void NET_SendPacket (netsrc_t netsrc, int length, void *data, netadr_t to)
{
	int ret;
	struct sockaddr_qstorage	addr;
	int socket;
	int size;

	if (to.type == NA_LOOPBACK)
	{
//		if (Cvar_Get("drop", "0", 0, "network debugging")->value)
//		if ((rand()&15)==15)	//simulate PL
//			return;
		NET_SendLoopPacket(netsrc, length, data, to);
		return;
	}

	if (netsrc == NS_SERVER)
	{
#ifdef CLIENTONLY
		Sys_Error("NET_SendPacket: bad netsrc");
		socket = 0;
#else

#ifdef TCPCONNECT
		svtcpstream_t *st;
		for (st = svs.tcpstreams; st; st = st->next)
		{
			if (st->socketnum == INVALID_SOCKET)
				continue;

			if (NET_CompareAdr(to, st->remoteaddr))
			{
				unsigned short slen = BigShort((unsigned short)length);
				send(st->socketnum, (char*)&slen, sizeof(slen), 0);
				send(st->socketnum, data, length, 0);

				st->timeouttime = Sys_DoubleTime() + 20;

				return;
			}
		}
#endif


#ifdef USEIPX
		if (to.type == NA_BROADCAST_IPX || to.type == NA_IPX)
			socket = svs.socketipx;
		else
#endif
#ifdef IPPROTO_IPV6
			if (to.type == NA_IPV6)
			socket = svs.socketip6;
		else
#endif
			socket = svs.socketip;
#endif
	}
	else
	{
#ifdef SERVERONLY
		Sys_Error("NET_SendPacket: bad netsrc");
		socket = 0;
#else

#ifdef TCPCONNECT
		if (cls.sockettcp != -1)
		{
			if (NET_CompareAdr(to, cls.sockettcpdest))
			{	//this goes to the server
				//so send it via tcp
				unsigned short slen = BigShort((unsigned short)length);
				send(cls.sockettcp, (char*)&slen, sizeof(slen), 0);
				send(cls.sockettcp, data, length, 0);

				return;
			}
		}
#endif



#ifdef USEIPX
		if (to.type == NA_BROADCAST_IPX || to.type == NA_IPX)
			socket = cls.socketipx;
		else
#endif
#ifdef IPPROTO_IPV6
			if (to.type == NA_BROADCAST_IP6 || to.type == NA_IPV6)
			socket = cls.socketip6;
		else
#endif
			socket = cls.socketip;
#endif
	}

	NetadrToSockadr (&to, &addr);

	switch(to.type)
	{
	default:
		size = 0;	//should cause an error. :)
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

	ret = sendto (socket, data, length, 0, (struct sockaddr*)&addr, size );
	if (ret == -1)
	{
// wouldblock is silent
		if (qerrno == EWOULDBLOCK)
			return;

		if (qerrno == ECONNREFUSED)
			return;

#ifndef SERVERONLY
		if (qerrno == EADDRNOTAVAIL)
			Con_DPrintf("NET_SendPacket Warning: %i\n", qerrno);
		else
#endif
			Con_TPrintf (TL_NETSENDERROR, qerrno);
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
	int i;

	FD_ZERO(&fdset);

	if (stdinissocket)
		FD_SET(0, &fdset);	//stdin tends to be socket 0

	i = 0;
	if (svs.socketip!=INVALID_SOCKET)
	{
		FD_SET(svs.socketip, &fdset); // network socket
		i = svs.socketip;
	}
#ifdef IPPROTO_IPV6
	if (svs.socketip6!=INVALID_SOCKET)
	{
		FD_SET(svs.socketip6, &fdset); // network socket
		if (svs.socketip6 > i)
			i = svs.socketip6;
		i = svs.socketip6;
	}
#endif
#ifdef USEIPX
	if (svs.socketipx!=INVALID_SOCKET)
	{
		FD_SET(svs.socketipx, &fdset); // network socket
		if (svs.socketipx > i)
			i = svs.socketipx;
	}
#endif
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(i+1, &fdset, NULL, NULL, &timeout);

	if (stdinissocket)
		return FD_ISSET(0, &fdset);
	return true;
}
#endif

void NET_GetLocalAddress (int socket, netadr_t *out)
{
	char	buff[512];
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
	if (!*(int*)out->address.ip)	//socket was set to auto
		*(int *)out->address.ip = *(int *)adr.address.ip;	//change it to what the machine says it is, rather than the socket.

	if (notvalid)
		Con_Printf("Couldn't detect local ip\n");
	else
		Con_TPrintf(TL_IPADDRESSIS, NET_AdrToString (*out) );
}

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

#ifndef SERVERONLY
	cls.socketip = INVALID_SOCKET;
	cls.socketip6 = INVALID_SOCKET;
	cls.socketipx = INVALID_SOCKET;
#ifdef TCPCONNECT
	cls.sockettcp = INVALID_SOCKET;
#endif
#endif

#ifndef CLIENTONLY
	svs.socketip = INVALID_SOCKET;
	svs.socketip6 = INVALID_SOCKET;
	svs.socketipx = INVALID_SOCKET;
#ifdef TCPCONNECT
	svs.sockettcp = INVALID_SOCKET;
#endif
#endif
}
#ifndef SERVERONLY
void NET_InitClient(void)
{
	int port;
	int p;
	port = PORT_CLIENT;

	p = COM_CheckParm ("-port");
	if (p && p < com_argc)
	{
		port = atoi(com_argv[p+1]);
	}
	p = COM_CheckParm ("-clport");
	if (p && p < com_argc)
	{
		port = atoi(com_argv[p+1]);
	}

	//
	// open the single socket to be used for all communications
	//
	cls.socketip = UDP_OpenSocket (port, false);
#ifdef IPPROTO_IPV6
	cls.socketip6 = UDP6_OpenSocket (port, false);
#endif
#ifdef USEIPX
	cls.socketipx = IPX_OpenSocket (port, false);
#endif

	//
	// init the message buffer
	//
	net_message.maxsize = sizeof(net_message_buffer);
	net_message.data = net_message_buffer;

	//
	// determine my name & address
	//
	NET_GetLocalAddress (cls.socketip, &net_local_cl_ipadr);

	Con_TPrintf(TL_CLIENTPORTINITED);
}
#endif
#ifndef CLIENTONLY

void NET_CloseServer(void)
{
	allowconnects = false;

	if (svs.socketip != INVALID_SOCKET)
	{
		UDP_CloseSocket(svs.socketip);
		svs.socketip = INVALID_SOCKET;
	}
#ifdef IPPROTO_IPV6
	if (svs.socketip6 != INVALID_SOCKET)
	{
		UDP_CloseSocket(svs.socketip6);
		svs.socketip6 = INVALID_SOCKET;
	}
#endif
#ifdef USEIPX
	if (svs.socketipx != INVALID_SOCKET)
	{
		IPX_CloseSocket(svs.socketipx);
		svs.socketipx = INVALID_SOCKET;
	}
#endif
#ifdef TCPCONNECT
	if (svs.sockettcp != INVALID_SOCKET)
	{
		closesocket(svs.sockettcp);
		svs.sockettcp = INVALID_SOCKET;
	}
#endif

	net_local_sv_ipadr.type = NA_LOOPBACK;
	net_local_sv_ip6adr.type = NA_LOOPBACK;
	net_local_sv_ipxadr.type = NA_LOOPBACK;
}

void NET_InitServer(void)
{
	int port;
	port = PORT_SERVER;

	if (sv_listen_nq.value || sv_listen_dp.value || sv_listen_qw.value)
	{
		allowconnects = true;

		Cvar_ForceCallback(&sv_port);
#ifdef TCPCONNECT
		Cvar_ForceCallback(&sv_port_tcp);
#endif
#ifdef IPPROTO_IPV6
		Cvar_ForceCallback(&sv_port_ipv6);
#endif
#ifdef USEIPX
		Cvar_ForceCallback(&sv_port_ipx);
#endif
	}
	else
		NET_CloseServer();


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
	UDP_CloseSocket (cls.socketip);
#ifdef IPPROTO_IPV6
	UDP_CloseSocket (cls.socketip6);
#endif
#ifdef USEIPX
	IPX_CloseSocket (cls.socketipx);
#endif
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

	if (tf->sock != INVALID_SOCKET)
	{
		len = recv(tf->sock, tf->readbuffer + tf->readbuffered, sizeof(tf->readbuffer) - tf->readbuffered, 0);
		if (len == -1)
		{
			//fixme: figure out wouldblock or error
		}
		else if (len == 0)
			VFSTCP_Error(tf);
		else
			tf->readbuffered += len;
	}
	if (bytestoread <= tf->readbuffered)
	{
		memcpy(buffer, tf->readbuffer, bytestoread);
		tf->readbuffered -= bytestoread;
		memmove(tf->readbuffer, tf->readbuffer+bytestoread, tf->readbuffered);
		return bytestoread;
	}
	else
		return 0;
}
int VFSTCP_WriteBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
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

vfsfile_t *FS_OpenTCP(char *name)
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
