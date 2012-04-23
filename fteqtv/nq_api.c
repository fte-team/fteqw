/*
This file is intended as a set of exports for an NQ-based engine.
This is supported _purely_ for clients, and will not work for servers.



[EndUser] how to use:
to join a qw server: connect "udp:127.0.0.1:27500"
to watch a qtv stream: connect "tcp:3@127.0.0.1:27599"  (where '3' is the streamid)
to watch an mvd demo: connect "demo:blahblah.mvd" - the demo will be loaded from $WORKINGDIR/qw/demos/ - note that $WORKINGDIR is NOT always the same dir
		as your engine is running from. The -basedir argument will break it, or engines that hunt down a 'proper' installation of quake instead.


[Developer] how to incorporate into an nq engine:
load up net_win.c
find the #include "net_wins.h" line.
dupe it, call it net_qtv.h
dupe the header itself too, changing all WINS_foo to QTV_foo.
find the net_landrivers array. Dupe the first block, then edit the first block to be all QTV_foo functions.
bump net_numlandrivers.
For non-window operating systems, you'll need to do the same, just figure out which net_win.c equivelent function it uses first. :P
certain engines may do weird things with the port. probably its best to just use Cmd_Args() for the connect command instead of Cmd_Argv(1), and to add
		port parsing to XXXX_GetAddrFromName instead of messing around with port cvars etc and ruining server configs.
		If your engine already has weird port behaviour, then its entirely your problem to fix. :P
You probably want to tweak your menus a little to clean up the nq/qw/qtv connection distinctions.
If you do want to make changes to libqtv, please consider joining the FTE team (or at least the irc channel) in order to contribute without forking.

[Developer] how to compile libqtv:
cflags MUST define 'LIBQTV' or it won't compile properly.
The relevent exports are all tagged as 'EXPORT void PUBLIC fname(...)' (dllexport+cdecl in windows), feel free to define those properly if you're making a linux shared object without exporting all (potentially conflicting) internals.
This means you can compile it as a dll without any issues, one with a standardized interface. Any libqtv-specific bugfixes can be released independantly from engine(s).
Compiling a dll with msvc will generally automatically produce a .lib which you can directly link against. Alternatively, include both projects in your workspace and set up dependancies properly and it'll be automatically imported.

[PowerUser] issues:
its a full qtv proxy, but you can't get admin/rcon access to it.
it doesn't read any configs, and has no console, thus you cannot set an rcon password/port.
without console/rcon, you cannot enable any listening ports for other users.
if you need a public qtv proxy, use a standalone version.
*/



#include "qtv.h"
int build_number(void);

static cluster_t *cluster;

//note that a qsockaddr is only 16 bytes.
//this is not enough for ipv6 etc.
struct qsockaddr
{
	int ipid;
};
char resolvedadrstring[128];
int lastadrid;

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#define PUBLIC __cdecl
#endif
#ifndef EXPORT
#define EXPORT
#endif
#ifndef PUBLIC
#define PUBLIC
#endif

EXPORT int PUBLIC QTV_Init (void)
{
	cluster = malloc(sizeof(*cluster));
	if (cluster)
	{
		memset(cluster, 0, sizeof(*cluster));

		cluster->qwdsocket[0] = INVALID_SOCKET;
		cluster->qwdsocket[1] = INVALID_SOCKET;
		cluster->tcpsocket[0] = INVALID_SOCKET;
		cluster->tcpsocket[1] = INVALID_SOCKET;
		cluster->anticheattime = 1*1000;
		cluster->tooslowdelay = 100;
		cluster->qwlistenportnum = 0;
		cluster->allownqclients = true;
		strcpy(cluster->hostname, DEFAULT_HOSTNAME);
		cluster->buildnumber = build_number();
		cluster->maxproxies = -1;

		strcpy(cluster->demodir, "qw/demos/");
		return 0;
	}

	return -1;
}
EXPORT void PUBLIC QTV_Shutdown (void)
{
}
EXPORT void PUBLIC QTV_Listen (qboolean state)
{
}
EXPORT int PUBLIC QTV_OpenSocket (int port)
{
	return 0;
}
EXPORT int PUBLIC QTV_CloseSocket (int socket)
{
	//give it a chance to close any server connections from us disconnecting (should have already send disconnect message, but won't have run the server so not noticed the lack of viewers)
	Cluster_Run(cluster, false);
	return 0;
}
EXPORT int PUBLIC QTV_Connect (int socket, struct qsockaddr *addr)
{
	if (addr->ipid == lastadrid)
	{
		strlcpy(cluster->autojoinadr, resolvedadrstring, sizeof(cluster->autojoinadr));
		return 0;
	}
	else
	{
		cluster->autojoinadr[0] = 0;
		return -1;
	}
	return 0;
}
EXPORT int PUBLIC QTV_CheckNewConnections (void)
{
	return -1;
}

static byte pendingbuf[8][1032];
static int pendinglen[8];
static unsigned int pendingin, pendingout;
void QTV_DoReceive(void *data, int length)
{
	int idx;
	if (length > sizeof(pendingbuf[0]))
		return;
	idx = pendingout++;
	idx &= 7;
	memcpy(pendingbuf[idx], data, length);
	pendinglen[idx] = length;
}
EXPORT int PUBLIC QTV_Read (int socket, byte *buf, int len, struct qsockaddr *addr)
{
	if (pendingout == pendingin)
	{
		Cluster_Run(cluster, false);
		Cluster_Run(cluster, false);
	}

	while (pendingin != pendingout)
	{
		int idx = pendingin++;
		idx &= 7;
		if (pendinglen[idx] > len)
			continue;	//error
		memcpy(buf, pendingbuf[idx], pendinglen[idx]);
		return pendinglen[idx];
	}
	return 0;
}
EXPORT int PUBLIC QTV_Write (int socket, byte *buf, int len, struct qsockaddr *addr)
{
	netmsg_t m;
	netadr_t from;
	from.tcpcon = NULL;
	((struct sockaddr*)from.sockaddr)->sa_family = AF_UNSPEC;

	m.cursize = len;
	m.data = buf;
	m.readpos = 0;

	QW_ProcessUDPPacket(cluster, &m, from);

	if (pendingout == pendingin)
		Cluster_Run(cluster, false);

	return 0;
}
EXPORT int PUBLIC QTV_Broadcast (int socket, byte *buf, int len)
{
	netmsg_t m;
	netadr_t from;
	from.tcpcon = NULL;
	((struct sockaddr*)from.sockaddr)->sa_family = AF_UNSPEC;

	m.cursize = len;
	m.data = buf;
	m.readpos = 0;

	QW_ProcessUDPPacket(cluster, &m, from);

	return 0;
}
EXPORT char *PUBLIC QTV_AddrToString (struct qsockaddr *addr)
{
	return 0;
}
EXPORT int PUBLIC QTV_StringToAddr (char *string, struct qsockaddr *addr)
{
	if (!strncmp(string, "udp:", 4) || !strncmp(string, "tcp:", 4) || !strncmp(string, "file:", 5))
	{
		snprintf(resolvedadrstring, sizeof(resolvedadrstring), "%s", string);
		addr->ipid = ++lastadrid;
		return 0;
	}
	return -1;
}
EXPORT int PUBLIC QTV_GetSocketAddr (int socket, struct qsockaddr *addr)
{
	return 0;
}
EXPORT int PUBLIC QTV_GetNameFromAddr (struct qsockaddr *addr, char *name)
{
	return 0;
}
EXPORT int PUBLIC QTV_GetAddrFromName (char *name, struct qsockaddr *addr)
{
	return QTV_StringToAddr(name, addr);
}
EXPORT int PUBLIC QTV_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2)
{
	return 0;
}
EXPORT int PUBLIC QTV_GetSocketPort (struct qsockaddr *addr)
{
	return 0;
}
EXPORT int PUBLIC QTV_SetSocketPort (struct qsockaddr *addr, int port)
{
	return 0;
}
