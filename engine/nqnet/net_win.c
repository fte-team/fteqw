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
#ifdef NQPROT
#include "../client/winquake.h"

#include "net_loop.h"
#include "net_dgrm.h"
#include "net_ser.h"

#define Sys_FloatTime Sys_DoubleTime

net_driver_t net_drivers[MAX_NET_DRIVERS] =
{
#if !defined(SERVERONLY) && !defined(CLIENTONLY)
	{
	"Loopback",
	false,
	Loop_Init,
	Loop_Listen,
	Loop_SearchForHosts,
	Loop_Connect,
	Loop_CheckNewConnections,
	Loop_GetMessage,
	Loop_SendMessage,
	Loop_SendUnreliableMessage,
	Loop_CanSendMessage,
	Loop_CanSendUnreliableMessage,
	Loop_Close,
	Loop_Shutdown
	}
	,
#endif
	{
	"Datagram",
	false,
	Datagram_Init,
	Datagram_Listen,
	Datagram_SearchForHosts,
	Datagram_Connect,
	Datagram_CheckNewConnections,
	Datagram_GetMessage,
	Datagram_SendMessage,
	Datagram_SendUnreliableMessage,
	Datagram_CanSendMessage,
	Datagram_CanSendUnreliableMessage,
	Datagram_Close,
	Datagram_Shutdown, 

	Datagram_BeginConnect,
	Datagram_ContinueConnect
	}
};

#if !defined(SERVERONLY) && !defined(CLIENTONLY)
int net_numdrivers = 2;
#else
int net_numdrivers = 1;
#endif


#include "net_wins.h"
#include "net_wipx.h"

net_landriver_t	net_landrivers[MAX_NET_DRIVERS] =
{
	{
	"Winsock TCPIP",
	false,
	0,
	WINS_Init,
	WINS_Shutdown,
	WINS_Listen,
	WINS_OpenSocket,
	WINS_CloseSocket,
	WINS_Connect,
	WINS_CheckNewConnections,
	WINS_Read,
	WINS_Write,
	WINS_Broadcast,
	WINS_AddrToString,
	WINS_StringToAddr,
	WINS_GetSocketAddr,	
	WINS_GetNameFromAddr,
	WINS_GetAddrFromName,
	WINS_AddrCompare,
	WINS_GetSocketPort,
	WINS_SetSocketPort
	}
#ifdef _WIN32
,
	{
	"Winsock IPX",
	false,
	0,
	WIPX_Init,
	WIPX_Shutdown,
	WIPX_Listen,
	WIPX_OpenSocket,
	WIPX_CloseSocket,
	WIPX_Connect,
	WIPX_CheckNewConnections,
	WIPX_Read,
	WIPX_Write,
	WIPX_Broadcast,
	WIPX_AddrToString,
	WIPX_StringToAddr,
	WIPX_GetSocketAddr,
	WIPX_GetNameFromAddr,
	WIPX_GetAddrFromName,
	WIPX_AddrCompare,
	WIPX_GetSocketPort,
	WIPX_SetSocketPort
	}
#endif
};

#ifdef _WIN32
int net_numlandrivers = 2;
#else
int net_numlandrivers = 1;
#endif






















#ifndef CLIENTONLY
extern cvar_t hostname;
#endif

#define MAXHOSTNAMELEN		256

static int net_acceptsocket = -1;		// socket for fielding new connections
static int net_controlsocket;
static int net_broadcastsocket = 0;
static struct sockaddr_qstorage broadcastaddr;

static unsigned long myAddr;

#ifdef _WIN32
qboolean	winsock_lib_initialized;

int (PASCAL *pWSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
int (PASCAL *pWSACleanup)(void);
int (PASCAL *pWSAGetLastError)(void);
SOCKET (PASCAL *psocket)(int af, int type, int protocol);
int (PASCAL *pioctlsocket)(SOCKET s, long cmd, u_long  *argp);
int (PASCAL *psetsockopt)(SOCKET s, int level, int optname,
							  const char  * optval, int optlen);
int (PASCAL *precvfrom)(SOCKET s, char * buf, int len, int flags,
							struct sockaddr *from, int * fromlen);
int (PASCAL *psendto)(SOCKET s, const char * buf, int len, int flags,
						  const struct sockaddr *to, int tolen);
int (PASCAL *pclosesocket)(SOCKET s);
int (PASCAL *pgethostname)(char * name, int namelen);
struct hostent * (PASCAL *pgethostbyname)(const char * name);
struct hostent * (PASCAL *pgethostbyaddr)(const char * addr,
												  int len, int type);
int (PASCAL *pgetsockname)(SOCKET s, struct sockaddr *name,
							   int * namelen);

int winsock_initialized = 0;
WSADATA		winsockdata;
#define qerrno pWSAGetLastError()

#define EWOULDBLOCK WSAEWOULDBLOCK
#define ECONNREFUSED WSAECONNREFUSED
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <errno.h>

#include <unistd.h>

#define SOCKET_ERROR -1

#define qerrno errno

#define psocket						socket
#define pgethostbyaddrpsocket		gethostbyaddrpsocket
#define pgethostbyaddrpioctlsocket	gethostbyaddrpioctlsocket
#define psetsockopt					setsockopt
#define precvfrom					recvfrom
#define psendto						sendto
#define pclosesocket				close
#define pgethostname				gethostname
#define pgethostbyname				gethostbyname
#define pgethostbyaddr				gethostbyaddr
#define pgetsockname				getsockname
#define	pioctlsocket				ioctl

#endif

#include "net_wins.h"

//=============================================================================

static double	blocktime;
#ifdef _WIN32
BOOL PASCAL BlockingHook(void)  
{ 
    MSG		msg;
    BOOL	ret;
 
	if ((Sys_FloatTime() - blocktime) > 2.0)
	{
		WSACancelBlockingCall();
		return FALSE;
	}

    /* get the next message, if any */ 
    ret = (BOOL) PeekMessage(&msg, NULL, 0, 0, PM_REMOVE); 
 
    /* if we got one, process it */ 
    if (ret) { 
        TranslateMessage(&msg); 
        DispatchMessage(&msg); 
    } 
 
    /* TRUE if we got a message */ 
    return ret; 
} 
#endif

void WINS_GetLocalAddress()
{
	struct hostent	*local = NULL;
	char			buff[MAXHOSTNAMELEN];
	unsigned long	addr;

	if (myAddr != INADDR_ANY)
		return;

	if (pgethostname(buff, MAXHOSTNAMELEN) == SOCKET_ERROR)
		return;

	blocktime = Sys_FloatTime();
#ifdef _WIN32
	WSASetBlockingHook(BlockingHook);
#endif
	local = pgethostbyname(buff);
#ifdef _WIN32
	WSAUnhookBlockingHook();
#endif
	if (local == NULL)
		return;

	myAddr = *(int *)local->h_addr_list[0];

	addr = ntohl(myAddr);
	sprintf(my_tcpip_address, "%d.%d.%d.%d", (qbyte)((addr >> 24) & 0xff), (qbyte)((addr >> 16) & 0xff), (qbyte)((addr >> 8) & 0xff), (qbyte)(addr & 0xff));
}


int WINS_Init (void)
{
	int		i;
	char	buff[MAXHOSTNAMELEN];
	char	*p;

#ifdef _WIN32
	int		r;
	WORD	wVersionRequested;
	HINSTANCE hInst;

// initialize the Winsock function vectors (we do this instead of statically linking
// so we can run on Win 3.1, where there isn't necessarily Winsock)
    hInst = LoadLibrary("wsock32.dll");
	
	if (hInst == NULL)
	{
		Con_Printf ("Failed to load winsock.dll\n");
		winsock_lib_initialized = false;
		return -1;
	}

	winsock_lib_initialized = true;

    pWSAStartup = (void *)GetProcAddress(hInst, "WSAStartup");
    pWSACleanup = (void *)GetProcAddress(hInst, "WSACleanup");
    pWSAGetLastError = (void *)GetProcAddress(hInst, "WSAGetLastError");
    psocket = (void *)GetProcAddress(hInst, "socket");
    pioctlsocket = (void *)GetProcAddress(hInst, "ioctlsocket");
    psetsockopt = (void *)GetProcAddress(hInst, "setsockopt");
    precvfrom = (void *)GetProcAddress(hInst, "recvfrom");
    psendto = (void *)GetProcAddress(hInst, "sendto");
    pclosesocket = (void *)GetProcAddress(hInst, "closesocket");
    pgethostname = (void *)GetProcAddress(hInst, "gethostname");
    pgethostbyname = (void *)GetProcAddress(hInst, "gethostbyname");
    pgethostbyaddr = (void *)GetProcAddress(hInst, "gethostbyaddr");
    pgetsockname = (void *)GetProcAddress(hInst, "getsockname");

    if (!pWSAStartup || !pWSACleanup || !pWSAGetLastError ||
		!psocket || !pioctlsocket || !psetsockopt ||
		!precvfrom || !psendto || !pclosesocket ||
		!pgethostname || !pgethostbyname || !pgethostbyaddr ||
		!pgetsockname)
	{
		Con_Printf ("Couldn't GetProcAddress from winsock.dll\n");
		return -1;
	}
#endif

	if (COM_CheckParm ("-noudp"))
		return -1;

#ifdef _WIN32
	if (winsock_initialized == 0)
	{
		wVersionRequested = MAKEWORD(1, 1); 

		r = pWSAStartup (MAKEWORD(1, 1), &winsockdata);

		if (r)
		{
			Con_Printf ("Winsock initialization failed.\n");
			return -1;
		}
	}
	winsock_initialized++;
#endif

	// determine my name
	if (pgethostname(buff, MAXHOSTNAMELEN) == SOCKET_ERROR)
	{
#ifdef _WIN32
		Con_DPrintf ("Winsock TCP/IP Initialization failed.\n");
		if (--winsock_initialized == 0)
			pWSACleanup ();
#else
		Con_DPrintf ("TCP/IP failed to get hostname.\n");
#endif
		return -1;
	}
#ifndef CLIENTONLY
	// if the quake hostname isn't set, set it to the machine name
	if (Q_strcmp(hostname.string, "UNNAMED") == 0)
	{
		// see if it's a text IP address (well, close enough)
		for (p = buff; *p; p++)
			if ((*p < '0' || *p > '9') && *p != '.')
				break;

		// if it is a real name, strip off the domain; we only want the host
		if (*p)
		{
			for (i = 0; i < 15; i++)
				if (buff[i] == '.')
					break;
			buff[i] = 0;
		}
		Cvar_Set (&hostname, buff);
	}
#endif
	i = COM_CheckParm ("-nqip");
	if (i)
	{
		if (i < com_argc-1)
		{
			myAddr = inet_addr(com_argv[i+1]);
			if (myAddr == INADDR_NONE)
				Sys_Error ("%s is not a valid IP address", com_argv[i+1]);
			strcpy(my_tcpip_address, com_argv[i+1]);
		}
		else
		{
			Sys_Error ("NET_Init: you must specify an IP address after -ip");
		}
	}
	else
	{
		myAddr = INADDR_ANY;
		strcpy(my_tcpip_address, "INADDR_ANY");
	}

	if ((net_controlsocket = WINS_OpenSocket (0)) == -1)
	{
		Con_Printf("WINS_Init: Unable to open control socket\n");
#ifdef _WIN32
		if (--winsock_initialized == 0)
			pWSACleanup ();
#endif
		return -1;
	}

	((struct sockaddr_in *)&broadcastaddr)->sin_family = AF_INET;
	((struct sockaddr_in *)&broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
	((struct sockaddr_in *)&broadcastaddr)->sin_port = htons((unsigned short)net_hostport);

	Con_Printf("Winsock TCP/IP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

//=============================================================================

void WINS_Shutdown (void)
{
	WINS_Listen (false);
	WINS_CloseSocket (net_controlsocket);
#ifdef _WIN32
	if (--winsock_initialized == 0)
		pWSACleanup ();
#endif
}

//=============================================================================

void WINS_Listen (qboolean state)
{
	// enable listening
	if (state)
	{
		if (net_acceptsocket != -1)
			return;
		WINS_GetLocalAddress();
		if ((net_acceptsocket = WINS_OpenSocket (net_hostport)) == -1)
			Con_Printf ("WINS_Listen: Unable to open accept socket\n");
		return;
	}

	// disable listening
	if (net_acceptsocket == -1)
		return;
	WINS_CloseSocket (net_acceptsocket);
	net_acceptsocket = -1;
}

//=============================================================================

int WINS_OpenSocket (int port)
{
	int newsocket;
	struct sockaddr_in address;
	u_long _true = 1;

	if ((newsocket = psocket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

	if (pioctlsocket (newsocket, FIONBIO, &_true) == -1)
		goto ErrorReturn;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = myAddr;
	address.sin_port = htons((unsigned short)port);
	if( bind (newsocket, (void *)&address, sizeof(address)) == 0)
		return newsocket;

	Con_Printf ("Unable to bind to %s\n", WINS_AddrToString((struct sockaddr_qstorage *)&address));
ErrorReturn:
	pclosesocket (newsocket);
	return -1;
}

//=============================================================================

int WINS_CloseSocket (int socket)
{
	if (socket == net_broadcastsocket)
		net_broadcastsocket = 0;
	return pclosesocket (socket);
}


//=============================================================================
/*
============
PartialIPAddress

this lets you type only as much of the net address as required, using
the local network components to fill in the rest
============
*/
static int PartialIPAddress (char *in, struct sockaddr_qstorage *hostaddr)
{
	char buff[256];
	char *b;
	int addr;
	int num;
	int mask;
	int run;
	int port;
	
	buff[0] = '.';
	b = buff;
	strcpy(buff+1, in);
	if (buff[1] == '.')
		b++;

	addr = 0;
	mask=-1;
	while (*b == '.')
	{
		b++;
		num = 0;
		run = 0;
		while (!( *b < '0' || *b > '9'))
		{
		  num = num*10 + *b++ - '0';
		  if (++run > 3)
		  	return -1;
		}
		if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
			return -1;
		if (num < 0 || num > 255)
			return -1;
		mask<<=8;
		addr = (addr<<8) + num;
	}
	
	if (*b++ == ':')
		port = Q_atoi(b);
	else
		port = net_hostport;

	((struct sockaddr_in *)hostaddr)->sin_family = AF_INET;
	((struct sockaddr_in *)hostaddr)->sin_port = htons((short)port);	
	((struct sockaddr_in *)hostaddr)->sin_addr.s_addr = (myAddr & htonl(mask)) | htonl(addr);
	
	return 0;
}
//=============================================================================

int WINS_Connect (int socket, struct sockaddr_qstorage *addr)
{
	return 0;
}

//=============================================================================

int WINS_CheckNewConnections (void)
{
	char buf[4096];

	if (net_acceptsocket == -1)
		return -1;

	if (precvfrom (net_acceptsocket, buf, sizeof(buf), MSG_PEEK, NULL, NULL) > 0)
	{
		return net_acceptsocket;
	}
	return -1;
}

//=============================================================================

int WINS_Read (int socket, qbyte *buf, int len, struct sockaddr_qstorage *addr)
{
	int addrlen = sizeof (struct sockaddr_qstorage);
	int ret;

	ret = precvfrom (socket, buf, len, 0, (struct sockaddr *)addr, &addrlen);
	if (ret == -1)
	{
		if (qerrno == EWOULDBLOCK || qerrno == ECONNREFUSED)
			return 0;

	}
	return ret;
}

//=============================================================================

int WINS_MakeSocketBroadcastCapable (int socket)
{
	int	i = 1;

	// make this socket broadcast capable
	if (psetsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) < 0)
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

//=============================================================================

int WINS_Broadcast (int socket, qbyte *buf, int len)
{
	int ret;

	if (socket != net_broadcastsocket)
	{
		if (net_broadcastsocket != 0)
			Sys_Error("Attempted to use multiple broadcasts sockets\n");
		WINS_GetLocalAddress();
		ret = WINS_MakeSocketBroadcastCapable (socket);
		if (ret == -1)
		{
			Con_Printf("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return WINS_Write (socket, buf, len, &broadcastaddr);
}

//=============================================================================

int WINS_Write (int socket, qbyte *buf, int len, struct sockaddr_qstorage *addr)
{
	int ret;

	ret = psendto (socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_qstorage));
	if (ret == -1)
		if (qerrno == EWOULDBLOCK)
			return 0;

	return ret;
}

//=============================================================================

char *WINS_AddrToString (struct sockaddr_qstorage *addr)
{
	static char buffer[22];
	int haddr;

	haddr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
	sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff, (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff, ntohs(((struct sockaddr_in *)addr)->sin_port));
	return buffer;
}

//=============================================================================

int WINS_StringToAddr (char *string, struct sockaddr_qstorage *addr)
{
	int ha1, ha2, ha3, ha4, hp;
	int ipaddr;

	sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
	ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;

	((struct sockaddr_in *)addr)->sin_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)hp);
	return 0;
}

//=============================================================================

int WINS_GetSocketAddr (int socket, struct sockaddr_qstorage *addr)
{
	int addrlen = sizeof(struct sockaddr_qstorage);
	unsigned int a;

	Q_memset(addr, 0, sizeof(struct sockaddr_qstorage));
	pgetsockname(socket, (struct sockaddr *)addr, &addrlen);
	a = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
	if (a == 0 || a == inet_addr("127.0.0.1"))
		((struct sockaddr_in *)addr)->sin_addr.s_addr = myAddr;

	return 0;
}

//=============================================================================

int WINS_GetNameFromAddr (struct sockaddr_qstorage *addr, char *name)
{
	struct hostent *hostentry;

	hostentry = pgethostbyaddr ((char *)&((struct sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET);
	if (hostentry)
	{
		Q_strncpyz (name, (char *)hostentry->h_name, NET_NAMELEN);
		return 0;
	}

	Q_strcpy (name, WINS_AddrToString (addr));
	return 0;
}

//=============================================================================

int WINS_GetAddrFromName(char *name, struct sockaddr_qstorage *addr)
{
	struct hostent *hostentry;

	if (name[0] >= '0' && name[0] <= '9')
		return PartialIPAddress (name, addr);
	
	hostentry = pgethostbyname (name);
	if (!hostentry)
		return -1;

	((struct sockaddr_in *)addr)->sin_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)net_hostport);	
	((struct sockaddr_in *)addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

	return 0;
}

//=============================================================================

int WINS_AddrCompare (struct sockaddr_qstorage *addr1, struct sockaddr_qstorage *addr2)
{
	if (((struct sockaddr_in *)addr1)->sin_family != ((struct sockaddr_in *)addr2)->sin_family)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_addr.s_addr != ((struct sockaddr_in *)addr2)->sin_addr.s_addr)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_port != ((struct sockaddr_in *)addr2)->sin_port)
		return 1;

	return 0;
}

//=============================================================================

int WINS_GetSocketPort (struct sockaddr_qstorage *addr)
{
	return ntohs(((struct sockaddr_in *)addr)->sin_port);
}


int WINS_SetSocketPort (struct sockaddr_qstorage *addr, int port)
{
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)port);
	return 0;
}

//=============================================================================
#endif
