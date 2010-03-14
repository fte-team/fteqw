#include "quakedef.h"

#ifdef CL_MASTER

#include "cl_master.h"

#define NET_GAMENAME_NQ		"QUAKE"

//rename to cl_master.c sometime

//the networking operates seperatly from the main code. This is so we can have full control over all parts of the server sending prints.
//when we send status to the server, it replys with a print command. The text to print contains the serverinfo.
//Q2's print command is a compleate 'print', while qw is just a 'p', thus we can distinguish the two easily.

//save favorites and allow addition of new ones from game?
//add filters some time

//remove dead servers.
//master was polled a minute ago and server was not on list - server on multiple masters would be awkward.

#ifdef _WIN32
#include "winquake.h"
#define USEIPX
#else
typedef int SOCKET;
#endif

#include "netinc.h"

#ifdef AF_IPX
#define USEIPX
#endif


//the number of servers should be limited only by memory.

cvar_t slist_cacheinfo = SCVAR("slist_cacheinfo", "0");	//this proves dangerous, memory wise.
cvar_t slist_writeserverstxt = SCVAR("slist_writeservers", "0");

void CL_MasterListParse(netadrtype_t adrtype, int type, qboolean slashpad);
void CL_QueryServers(void);
int CL_ReadServerInfo(char *msg, int servertype, qboolean favorite);

master_t *master;
player_t *mplayers;
serverinfo_t *firstserver;

static serverinfo_t **visibleservers;
static int numvisibleservers;
static int maxvisibleservers;

static double nextsort;

static hostcachekey_t sortfield;
static qboolean decreasingorder;




typedef struct {
	hostcachekey_t fieldindex;

	float operandi;
	char *operands;

	qboolean or;
	int compareop;
} visrules_t;
#define MAX_VISRULES 8
visrules_t visrules[MAX_VISRULES];
int numvisrules;




#define SLIST_MAXKEYS 64
char slist_keyname[SLIST_MAXKEYS][MAX_INFO_KEY];
int slist_customkeys;


#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif


#define POLLUDP4SOCKETS 64	//it's big so we can have lots of messages when behind a firewall. Basically if a firewall only allows replys, and only remembers 3 servers per socket, we need this big cos it can take a while for a packet to find a fast optimised route and we might be waiting for a few secs for a reply the first time around.
int lastpollsockUDP4;

#ifdef IPPROTO_IPV6
#define POLLUDP6SOCKETS 4	//it's non-zero so we can have lots of messages when behind a firewall. Basically if a firewall only allows replys, and only remembers 3 servers per socket, we need this big cos it can take a while for a packet to find a fast optimised route and we might be waiting for a few secs for a reply the first time around.
int lastpollsockUDP6;
#else
#define POLLUDP6SOCKETS 0
#endif

#ifdef USEIPX
#define POLLIPXSOCKETS	2	//ipx isn't used as much. In fact, we only expect local servers to be using it. I'm not sure why I implemented it anyway. You might see a q2 server using it. Rarely.
int lastpollsockIPX;
#else
#define POLLIPXSOCKETS 0
#endif

#define FIRSTIPXSOCKET (0)
#define FIRSTUDP4SOCKET (FIRSTIPXSOCKET+POLLIPXSOCKETS)
#define FIRSTUDP6SOCKET (FIRSTUDP4SOCKET+POLLUDP4SOCKETS)
#define POLLTOTALSOCKETS (FIRSTUDP6SOCKET+POLLUDP6SOCKETS)
SOCKET pollsocketsList[POLLTOTALSOCKETS];

void Master_SetupSockets(void)
{
	int i;
	for (i = 0; i < POLLTOTALSOCKETS; i++)
		pollsocketsList[i] = INVALID_SOCKET;
}

void Master_HideServer(serverinfo_t *server)
{
	int i, j;
	for (i = 0; i < numvisibleservers;)
	{
		if (visibleservers[i] == server)
		{
			for (j = i; j < numvisibleservers-1; j++)
				visibleservers[j] = visibleservers[j+1];
			visibleservers--;
		}
		else
			 i++;
	}
	server->insortedlist = false;
}

void Master_InsertAt(serverinfo_t *server, int pos)
{
	int i;
	for (i = numvisibleservers; i > pos; i--)
	{
		visibleservers[i] = visibleservers[i-1];
	}
	visibleservers[pos] = server;
	numvisibleservers++;

	server->insortedlist = true;
}

qboolean Master_CompareInteger(int a, int b, slist_test_t rule)
{
	switch(rule)
	{
	case SLIST_TEST_CONTAINS:
		return !!(a&b);
	case SLIST_TEST_NOTCONTAIN:
		return !(a&b);
	case SLIST_TEST_LESSEQUAL:
		return a<=b;
	case SLIST_TEST_LESS:
		return a<b;
	case SLIST_TEST_EQUAL:
		return a==b;
	case SLIST_TEST_GREATER:
		return a>b;
	case SLIST_TEST_GREATEREQUAL:
		return a>=b;
	case SLIST_TEST_NOTEQUAL:
		return a!=b;
	}
	return false;
}
qboolean Master_CompareString(char *a, char *b, slist_test_t rule)
{
	switch(rule)
	{
	case SLIST_TEST_CONTAINS:
		return !!strstr(a, b);
	case SLIST_TEST_NOTCONTAIN:
		return !strstr(a, b);
	case SLIST_TEST_LESSEQUAL:
		return stricmp(a, b)<=0;
	case SLIST_TEST_LESS:
		return stricmp(a, b)<0;
	case SLIST_TEST_EQUAL:
		return stricmp(a, b)==0;
	case SLIST_TEST_GREATER:
		return stricmp(a, b)>0;
	case SLIST_TEST_GREATEREQUAL:
		return stricmp(a, b)>=0;
	case SLIST_TEST_NOTEQUAL:
		return stricmp(a, b)!=0;
	}
	return false;
}

qboolean Master_ServerIsGreater(serverinfo_t *a, serverinfo_t *b)
{
	switch(sortfield)
	{
	case SLKEY_ADDRESS:
		break;
	case SLKEY_BASEGAME:
		return Master_CompareInteger(a->special, b->special, SLIST_TEST_LESS);
	case SLKEY_CUSTOM:
		break;
	case SLKEY_FRAGLIMIT:
		return Master_CompareInteger(a->fl, b->fl, SLIST_TEST_LESS);
	case SLKEY_FREEPLAYERS:
		return Master_CompareInteger(a->maxplayers - a->players, b->maxplayers - b->players, SLIST_TEST_LESS);
	case SLKEY_GAMEDIR:
		return Master_CompareString(a->gamedir, b->gamedir, SLIST_TEST_LESS);
	case SLKEY_MAP:
		return Master_CompareString(a->map, b->map, SLIST_TEST_LESS);
	case SLKEY_MAXPLAYERS:
		return Master_CompareInteger(a->maxplayers, b->maxplayers, SLIST_TEST_LESS);
	case SLKEY_NAME:
		return Master_CompareString(a->name, b->name, SLIST_TEST_LESS);
	case SLKEY_NUMPLAYERS:
		return Master_CompareInteger(a->players, b->players, SLIST_TEST_LESS);
	case SLKEY_PING:
		return Master_CompareInteger(a->ping, b->ping, SLIST_TEST_LESS);
	case SLKEY_TIMELIMIT:
		return Master_CompareInteger(a->tl, b->tl, SLIST_TEST_LESS);
	case SLKEY_TOOMANY:
		break;
	}
	return false;
}

qboolean Master_PassesMasks(serverinfo_t *a)
{
	int i;
	qboolean val, res;
	//always filter out dead unresponsive servers.
	if (!a->ping)
		return false;

	val = 1;

	for (i = 0; i < numvisrules; i++)
	{
		switch(visrules[i].fieldindex)
		{
		case SLKEY_PING:
			res = Master_CompareInteger(a->ping, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_NUMPLAYERS:
			res = Master_CompareInteger(a->players, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_MAXPLAYERS:
			res = Master_CompareInteger(a->maxplayers, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_FREEPLAYERS:
			res = Master_CompareInteger(a->freeslots, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_NUMBOTS:
			res = Master_CompareInteger(a->numbots, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_NUMHUMANS:
			res = Master_CompareInteger(a->numhumans, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_TIMELIMIT:
			res = Master_CompareInteger(a->tl, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_FRAGLIMIT:
			res = Master_CompareInteger(a->fl, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_PROTOCOL:
			res = Master_CompareInteger(a->fl, visrules[i].operandi, visrules[i].compareop);
			break;

		case SLKEY_MAP:
			res = Master_CompareString(a->map, visrules[i].operands, visrules[i].compareop);
			break;
		case SLKEY_NAME:
			res = Master_CompareString(a->name, visrules[i].operands, visrules[i].compareop);
			break;
		case SLKEY_GAMEDIR:
			res = Master_CompareString(a->gamedir, visrules[i].operands, visrules[i].compareop);
			break;

		case SLKEY_BASEGAME:
			res = Master_CompareInteger(a->special, visrules[i].operandi, visrules[i].compareop);
			break;
		case SLKEY_MOD:
			res = Master_CompareString(a->modname, visrules[i].operands, visrules[i].compareop);
			break;
		case SLKEY_QCSTATUS:
			res = Master_CompareString(a->qcstatus, visrules[i].operands, visrules[i].compareop);
			break;
		default:
			continue;
		}
		if (visrules[i].or)
			val |= res;
		else
			val &= res;
	}

	return val;
}

void Master_ClearMasks(void)
{
	numvisrules = 0;
}

void Master_SetMaskString(qboolean or, hostcachekey_t field, char *param, slist_test_t testop)
{
	if (numvisrules == MAX_VISRULES)
		return;	//just don't add it.

	nextsort = 0;
	visrules[numvisrules].fieldindex = field;
	visrules[numvisrules].compareop = testop;
	visrules[numvisrules].operands = param;
	visrules[numvisrules].or = or;
	numvisrules++;
}
void Master_SetMaskInteger(qboolean or, hostcachekey_t field, int param, slist_test_t testop)
{
	if (numvisrules == MAX_VISRULES)
		return;	//just don't add it.

	nextsort = 0;
	visrules[numvisrules].fieldindex = field;
	visrules[numvisrules].compareop = testop;
	visrules[numvisrules].operandi = param;
	visrules[numvisrules].or = or;
	numvisrules++;
}
void Master_SetSortField(hostcachekey_t field, qboolean descending)
{
	nextsort = 0;
	sortfield = field;
	decreasingorder = descending;
}
hostcachekey_t Master_GetSortField(void)
{
	return sortfield;
}
qboolean Master_GetSortDescending(void)
{
	return decreasingorder;
}

void Master_ShowServer(serverinfo_t *server)
{
	int i;
	if (!numvisibleservers)
	{
		Master_InsertAt(server, 0);
		return;
	}

	if (!decreasingorder)
	{
		for (i = 0; i < numvisibleservers; i++)
		{
			if (!Master_ServerIsGreater(server, visibleservers[i]))
			{
				Master_InsertAt(server, i);
				return;
			}
		}

	}
	else
	{
		for (i = 0; i < numvisibleservers; i++)
		{
			if (Master_ServerIsGreater(server, visibleservers[i]))
			{
				Master_InsertAt(server, i);
				return;
			}
		}
	}

	Master_InsertAt(server, numvisibleservers);
}

void Master_ResortServer(serverinfo_t *server)
{
	if (server->insortedlist)
	{
		if (!Master_PassesMasks(server))
			Master_HideServer(server);
	}
	else
	{
		if (Master_PassesMasks(server))
			Master_ShowServer(server);
	}
}

void Master_SortServers(void)
{
	serverinfo_t *server;

	int total = Master_TotalCount();
	if (maxvisibleservers < total)
	{
		maxvisibleservers = total;
		visibleservers = BZ_Realloc(visibleservers, maxvisibleservers*sizeof(serverinfo_t*));
	}

	{
		numvisibleservers = 0;
		for (server = firstserver; server; server = server->next)
			server->insortedlist = false;
	}

	for (server = firstserver; server; server = server->next)
	{
		Master_ResortServer(server);
	}

	nextsort = Sys_DoubleTime() + 8;
}

serverinfo_t *Master_SortedServer(int idx)
{
//	if (nextsort < Sys_DoubleTime())
//		Master_SortServers();

	if (idx < 0 || idx >= numvisibleservers)
		return NULL;

	return visibleservers[idx];
}

int Master_NumSorted(void)
{
	if (nextsort < Sys_DoubleTime())
		Master_SortServers();

	return numvisibleservers;
}



float Master_ReadKeyFloat(serverinfo_t *server, int keynum)
{
	if (!server)
		return -1;
	else if (keynum < SLKEY_CUSTOM)
	{
		switch(keynum)
		{
		case SLKEY_PING:
			return server->ping;
		case SLKEY_NUMPLAYERS:
			return server->players;
		case SLKEY_MAXPLAYERS:
			return server->maxplayers;
		case SLKEY_FREEPLAYERS:
			return server->maxplayers - server->players;
		case SLKEY_BASEGAME:
			return server->special;
		case SLKEY_TIMELIMIT:
			return server->tl;
		case SLKEY_FRAGLIMIT:
			return server->fl;
		case SLKEY_PROTOCOL:
			return server->protocol;
		case SLKEY_NUMBOTS:
			return server->numbots;
		case SLKEY_NUMHUMANS:
			return server->numhumans;
		case SLKEY_ISFAVORITE:
			return !!(server->special & SS_FAVORITE);

		default:
			return atof(Master_ReadKeyString(server, keynum));
		}
	}
	else if (server->moreinfo)
		return atof(Info_ValueForKey(server->moreinfo->info, slist_keyname[keynum-SLKEY_CUSTOM]));

	return 0;
}

char *Master_ReadKeyString(serverinfo_t *server, int keynum)
{
	char adr[MAX_ADR_SIZE];

	if (keynum < SLKEY_CUSTOM)
	{
		switch(keynum)
		{
		case SLKEY_MAP:
			return server->map;
		case SLKEY_NAME:
			return server->name;
		case SLKEY_ADDRESS:
			return NET_AdrToString(adr, sizeof(adr), server->adr);
		case SLKEY_GAMEDIR:
			return server->gamedir;

		case SLKEY_MOD:
			return server->modname;
		case SLKEY_QCSTATUS:
			return server->qcstatus;

		default:
			{
				static char s[64];
				sprintf(s, "%f", Master_ReadKeyFloat(server, keynum));
				return s;
			}
		}
	}
	else if (server->moreinfo)
		return Info_ValueForKey(server->moreinfo->info, slist_keyname[keynum-SLKEY_CUSTOM]);

	return "";
}

int Master_KeyForName(char *keyname)
{
	int i;
	if (!strcmp(keyname, "map"))
		return SLKEY_MAP;
	else if (!strcmp(keyname, "ping"))
		return SLKEY_PING;
	else if (!strcmp(keyname, "name"))
		return SLKEY_NAME;
	else if (!strcmp(keyname, "address") || !strcmp(keyname, "cname"))
		return SLKEY_ADDRESS;
	else if (!strcmp(keyname, "maxplayers"))
		return SLKEY_MAXPLAYERS;
	else if (!strcmp(keyname, "numplayers"))
		return SLKEY_NUMPLAYERS;
	else if (!strcmp(keyname, "freeplayers") || !strcmp(keyname, "freeslots"))
		return SLKEY_FREEPLAYERS;
	else if (!strcmp(keyname, "gamedir") || !strcmp(keyname, "game") || !strcmp(keyname, "*gamedir"))
		return SLKEY_GAMEDIR;
	else if (!strcmp(keyname, "special"))
		return SLKEY_BASEGAME;
	else if (!strcmp(keyname, "mod"))
		return SLKEY_MOD;
	else if (!strcmp(keyname, "protocol"))
		return SLKEY_PROTOCOL;
	else if (!strcmp(keyname, "numbots"))
		return SLKEY_NUMBOTS;
	else if (!strcmp(keyname, "numhumans"))
		return SLKEY_NUMHUMANS;
	else if (!strcmp(keyname, "qcstatus"))
		return SLKEY_QCSTATUS;
	else if (!strcmp(keyname, "isfavorite"))
		return SLKEY_ISFAVORITE;

	else if (slist_customkeys == SLIST_MAXKEYS)
		return SLKEY_TOOMANY;
	else
	{
		for (i = 0; i < slist_customkeys; i++)
		{
			if (!strcmp(slist_keyname[i], keyname))
			{
				return i + SLKEY_CUSTOM;
			}
		}
		Q_strncpyz(slist_keyname[slist_customkeys], keyname, MAX_INFO_KEY);
	
		slist_customkeys++;

		return slist_customkeys-1 + SLKEY_CUSTOM;
	}
}




void Master_AddMaster (char *address, int type, char *description)
{
	netadr_t adr;
	master_t *mast;

	if (!NET_StringToAdr(address, &adr))
	{
		Con_Printf("Failed to resolve address \"%s\"\n", address);
		return;
	}

#pragma message("Master_AddMaster: add ipv6. don't care about tcp/irc.")
	if (adr.type != NA_IP && adr.type != NA_IPX)
	{
		Con_Printf("Fixme: unable to poll address family for \"%s\"\n", address);
		return;
	}

	if (type < MT_SINGLEQW)	//broadcasts
	{
		if (adr.type == NA_IP)
			adr.type = NA_BROADCAST_IP;
		if (adr.type == NA_IPX)
			adr.type = NA_BROADCAST_IPX;
		if (adr.type == NA_IPV6)
			adr.type = NA_BROADCAST_IP6;
	}

	for (mast = master; mast; mast = mast->next)
	{
		if (NET_CompareAdr(mast->adr, adr) && mast->type == type)	//already exists.
			return;
	}
	mast = Z_Malloc(sizeof(master_t)+strlen(description)+1+strlen(address)+1);
	mast->adr = adr;
	mast->address = mast->name + strlen(description)+1;
	mast->type = type;
	strcpy(mast->name, description);
	strcpy(mast->address, address);

	mast->next = master;
	master = mast;
}

void Master_AddMasterHTTP (char *address, int mastertype, char *description)
{
	master_t *mast;
	int servertype;

	if (mastertype == MT_MASTERHTTPQW)
		servertype = 0;
	else
		servertype = SS_NETQUAKE;

	for (mast = master; mast; mast = mast->next)
	{
		if (!strcmp(mast->address, address) && mast->type == mastertype)	//already exists.
			return;
	}
	mast = Z_Malloc(sizeof(master_t)+strlen(description)+1+strlen(address)+1);
	mast->address = mast->name + strlen(description)+1;
	mast->type = mastertype;
	mast->servertype = servertype;
	strcpy(mast->name, description);
	strcpy(mast->address, address);

	mast->next = master;
	master = mast;
}

//build a linked list of masters.	Doesn't duplicate addresses.
qboolean Master_LoadMasterList (char *filename, int defaulttype, int depth)
{
	vfsfile_t *f;
	char line[1024];
	char file[1024];
	char *name, *next;
	int servertype;

	if (depth <= 0)
		return false;
	depth--;

	f = FS_OpenVFS(filename, "rb", FS_ROOT);
	if (!f)
		return false;

	while(VFS_GETS(f, line, sizeof(line)-1))
	{
		if (*line == '#')	//comment
			continue;

		next = COM_Parse(line);
		if (!*com_token)
			continue;

		if (!strcmp(com_token, "file"))	//special case. Add a port if you have a server named 'file'... (unlikly)
		{
			next = COM_Parse(next);
			if (!next)
				continue;
			Q_strncpyz(file, com_token, sizeof(file));
		}
		else
			*file = '\0';

		*next = '\0';
		next++;
		name = COM_Parse(next);
		servertype = -1;

		if (!strcmp(com_token, "single:qw"))
			servertype = MT_SINGLEQW;
		else if (!strcmp(com_token, "single:q2"))
			servertype = MT_SINGLEQ2;
		else if (!strcmp(com_token, "single:q3"))
			servertype = MT_SINGLEQ3;
		else if (!strcmp(com_token, "single:dp"))
			servertype = MT_SINGLEDP;
		else if (!strcmp(com_token, "single:nq") || !strcmp(com_token, "single:q1"))
			servertype = MT_SINGLENQ;
		else if (!strcmp(com_token, "single"))
			servertype = MT_SINGLEQW;

		else if (!strcmp(com_token, "master:dp"))
			servertype = MT_MASTERDP;
		else if (!strcmp(com_token, "master:qw"))
			servertype = MT_MASTERQW;
		else if (!strcmp(com_token, "master:q2"))
			servertype = MT_MASTERQ2;
		else if (!strcmp(com_token, "master:q3"))
			servertype = MT_MASTERQ3;
		else if (!strcmp(com_token, "master:http"))
			servertype = MT_MASTERHTTP;
		else if (!strcmp(com_token, "master:httpqw"))
			servertype = MT_MASTERHTTPQW;
		else if (!strcmp(com_token, "master"))	//any other sort of master, assume it's a qw master.
			servertype = MT_MASTERQW;

		else if (!strcmp(com_token, "bcast:qw"))
			servertype = MT_BCASTQW;
		else if (!strcmp(com_token, "bcast:q2"))
			servertype = MT_BCASTQ2;
		else if (!strcmp(com_token, "bcast:q3"))
			servertype = MT_BCASTQ3;
		else if (!strcmp(com_token, "bcast:nq"))
			servertype = MT_BCASTNQ;
		else if (!strcmp(com_token, "bcast:dp"))
			servertype = MT_BCASTDP;
		else if (!strcmp(com_token, "bcast"))
			servertype = MT_BCASTQW;

		else if (!strcmp(com_token, "favorite:qw"))
			servertype = -MT_SINGLEQW;
		else if (!strcmp(com_token, "favorite:q2"))
			servertype = -MT_SINGLEQ2;
		else if (!strcmp(com_token, "favorite:q3"))
			servertype = -MT_SINGLEQ3;
		else if (!strcmp(com_token, "favorite:nq"))
			servertype = -MT_SINGLENQ;
		else if (!strcmp(com_token, "favorite"))
			servertype = -MT_SINGLEQW;


		else
		{
			name = next;	//go back one token.
			servertype = defaulttype;
		}

		while(*name <= ' ' && *name != 0)	//skip whitespace
			name++;

		next = name + strlen(name)-1;
		while(*next <= ' ' && next > name)
		{
			*next = '\0';
			next--;
		}


		if (*file)
			Master_LoadMasterList(file, servertype, depth);
		else if (servertype < 0)
		{
			if (NET_StringToAdr(line, &net_from))
				CL_ReadServerInfo(va("\\hostname\\%s", name), -servertype, true);
			else
				Con_Printf("Failed to resolve address - \"%s\"\n", line);
		}
		else
		{
			switch (servertype)
			{
			case MT_MASTERHTTP:
			case MT_MASTERHTTPQW:
				Master_AddMasterHTTP(line, servertype, name);
				break;
			default:
				Master_AddMaster(line, servertype, name);
			}
			
		}
	}
	VFS_CLOSE(f);

	return true;
}


void NET_SendPollPacket(int len, void *data, netadr_t to)
{
	int ret;
	struct sockaddr_qstorage	addr;

#pragma message("NET_SendPollPacket: no support for ipv6")

	NetadrToSockadr (&to, &addr);
#ifdef USEIPX
	if (((struct sockaddr*)&addr)->sa_family == AF_IPX)
	{
		lastpollsockIPX++;
		if (lastpollsockIPX>=POLLIPXSOCKETS)
			lastpollsockIPX=0;
		if (pollsocketsList[FIRSTIPXSOCKET+lastpollsockIPX]==INVALID_SOCKET)
			pollsocketsList[FIRSTIPXSOCKET+lastpollsockIPX] = IPX_OpenSocket(PORT_ANY, true);
		if (pollsocketsList[FIRSTIPXSOCKET+lastpollsockIPX]==INVALID_SOCKET)
			return;	//bother
		ret = sendto (pollsocketsList[FIRSTIPXSOCKET+lastpollsockIPX], data, len, 0, (struct sockaddr *)&addr, sizeof(addr) );
	}
	else
#endif
#ifdef IPPROTO_IPV6
	if (((struct sockaddr*)&addr)->sa_family == AF_INET6)
	{
		lastpollsockUDP6++;
		if (lastpollsockUDP6>=POLLUDP6SOCKETS)
			lastpollsockUDP6=0;
		if (pollsocketsList[FIRSTUDP6SOCKET+lastpollsockUDP6]==INVALID_SOCKET)
			pollsocketsList[FIRSTUDP6SOCKET+lastpollsockUDP6] = UDP6_OpenSocket(PORT_ANY, true);
		if (pollsocketsList[FIRSTUDP6SOCKET+lastpollsockUDP6]==INVALID_SOCKET)
			return;	//bother
		ret = sendto (pollsocketsList[FIRSTUDP6SOCKET+lastpollsockUDP6], data, len, 0, (struct sockaddr *)&addr, sizeof(addr) );
	}
	else
#endif
		if (((struct sockaddr*)&addr)->sa_family == AF_INET)
	{
		lastpollsockUDP4++;
		if (lastpollsockUDP4>=POLLUDP4SOCKETS)
			lastpollsockUDP4=0;
		if (pollsocketsList[FIRSTUDP4SOCKET+lastpollsockUDP4]==INVALID_SOCKET)
			pollsocketsList[FIRSTUDP4SOCKET+lastpollsockUDP4] = UDP_OpenSocket(PORT_ANY, true);
		if (pollsocketsList[FIRSTUDP4SOCKET+lastpollsockUDP4]==INVALID_SOCKET)
			return;	//bother
		ret = sendto (pollsocketsList[FIRSTUDP4SOCKET+lastpollsockUDP4], data, len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in) );
	}
	else
		return;

	if (ret == -1)
	{
// wouldblock is silent
		if (qerrno == EWOULDBLOCK)
			return;

		if (qerrno == ECONNREFUSED)
			return;

		if (qerrno == EADDRNOTAVAIL)
			Con_DPrintf("NET_SendPollPacket Warning: %i\n", qerrno);
		else
			Con_Printf ("NET_SendPollPacket ERROR: %i\n", qerrno);
	}
}

int NET_CheckPollSockets(void)
{
	#define	MAX_UDP_PACKET	8192	// one more than msg + header
	extern qbyte		net_message_buffer[MAX_UDP_PACKET];
	int sock;
	SOCKET usesocket;
	char adr[MAX_ADR_SIZE];

	for (sock = 0; sock < POLLTOTALSOCKETS; sock++)
	{
		int 	ret;
		struct sockaddr_qstorage	from;
		int		fromlen;

		usesocket = pollsocketsList[sock];

		if (usesocket == INVALID_SOCKET)
			continue;
		fromlen = sizeof(from);
		ret = recvfrom (usesocket, (char *)net_message_buffer, sizeof(net_message_buffer), 0, (struct sockaddr *)&from, &fromlen);

		if (ret == -1)
		{
			if (qerrno == EWOULDBLOCK)
				continue;
			if (qerrno == EMSGSIZE)
			{
				SockadrToNetadr (&from, &net_from);
				Con_Printf ("Warning:  Oversize packet from %s\n",
					NET_AdrToString (adr, sizeof(adr), net_from));
				continue;
			}
			if (qerrno == ECONNABORTED || qerrno == ECONNRESET)
			{
//				Con_Printf ("Connection lost or aborted\n");
				continue;
			}


			Con_Printf ("NET_CheckPollSockets: %i, %s\n", qerrno, strerror(qerrno));
			continue;
		}
		SockadrToNetadr (&from, &net_from);

		net_message.cursize = ret;
		if (ret == sizeof(net_message_buffer) )
		{
			Con_Printf ("Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), net_from));
			continue;
		}

		if (*(int *)net_message.data == -1)
		{
			int c;
			char *s;

			MSG_BeginReading ();
			MSG_ReadLong ();        // skip the -1

			c = msg_readcount;
			s = MSG_ReadStringLine();	//peek for q2 messages.
#ifdef Q2CLIENT
			if (!strcmp(s, "print"))
			{
				CL_ReadServerInfo(MSG_ReadString(), MT_SINGLEQ2, false);
				continue;
			}
			if (!strcmp(s, "info"))	//parse a bit more...
			{
				CL_ReadServerInfo(MSG_ReadString(), MT_SINGLEQ2, false);
				continue;
			}
#ifdef IPPROTO_IPV6
			if (!strncmp(s, "server6", 7))	//parse a bit more...
			{
				msg_readcount = c+7;
				CL_MasterListParse(NA_IPV6, SS_QUAKE2, false);
				continue;
			}
#endif
			if (!strncmp(s, "servers", 7))	//parse a bit more...
			{
				msg_readcount = c+7;
				CL_MasterListParse(NA_IP, SS_QUAKE2, false);
				continue;
			}
#endif
#ifdef Q3CLIENT
			if (!strcmp(s, "statusResponse"))
			{
				CL_ReadServerInfo(MSG_ReadString(), MT_SINGLEQ3, false);
				continue;
			}
#endif

#ifdef IPPROTO_IPV6
			if (!strncmp(s, "getserversResponse6\\", 20))	//parse a bit more...
			{
				msg_readcount = c+19-1;
				CL_MasterListParse(NA_IPV6, SS_DARKPLACES, true);
				continue;
			}
#endif
			if (!strncmp(s, "getserversResponse\\", 19))	//parse a bit more...
			{
				msg_readcount = c+18-1;
				CL_MasterListParse(NA_IP, SS_DARKPLACES, true);
				continue;
			}
			if (!strcmp(s, "infoResponse"))	//parse a bit more...
			{
				CL_ReadServerInfo(MSG_ReadString(), MT_SINGLEDP, false);
				continue;
			}

#ifdef IPPROTO_IPV6
			if (!strncmp(s, "qw_slist6\\", 10))	//parse a bit more...
			{
				msg_readcount = c+9-1;
				CL_MasterListParse(NA_IPV6, SS_GENERICQUAKEWORLD, false);
				continue;
			}
#endif

			msg_readcount = c;

			c = MSG_ReadByte ();

			if (c == A2C_PRINT)	//qw server reply.
			{
				CL_ReadServerInfo(MSG_ReadString(), MT_SINGLEQW, false);
				continue;
			}

			if (c == M2C_MASTER_REPLY)	//qw master reply.
			{		
				CL_MasterListParse(NA_IP, SS_GENERICQUAKEWORLD, false);
				continue;
			}
		}
#ifdef NQPROT
		else
		{	//connected packet? Must be a NQ packet.
			char name[32];
			char map[16];
			int users, maxusers;

			int control;

			MSG_BeginReading ();
			control = BigLong(*((int *)net_message.data));
			MSG_ReadLong();
			if (control == -1)
				continue;
			if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
				continue;
			if ((control & NETFLAG_LENGTH_MASK) != ret)
				continue;

			if (MSG_ReadByte() != CCREP_SERVER_INFO)
				continue;

			NET_StringToAdr(MSG_ReadString(), &net_from);

			Q_strncpyz(name, MSG_ReadString(), sizeof(name));
			Q_strncpyz(map, MSG_ReadString(), sizeof(map));
			users = MSG_ReadByte();
			maxusers = MSG_ReadByte();
			if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
			{
//				Q_strcpy(name, "*");
//				Q_strcat(name, name);
			}

			CL_ReadServerInfo(va("\\hostname\\%s\\map\\%s\\maxclients\\%i\\clients\\%i", name, map, maxusers, users), MT_SINGLENQ, false);
		}
#endif
		continue;		
	}
	return 0;
}

void Master_RemoveKeepInfo(serverinfo_t *sv)
{
	sv->special &= ~SS_KEEPINFO;
	if (sv->moreinfo)
	{
		Z_Free(sv->moreinfo);
		sv->moreinfo = NULL;
	}
}

void SListOptionChanged(serverinfo_t *newserver)
{
	if (selectedserver.inuse)
	{
		serverinfo_t *oldserver;

		selectedserver.detail = NULL;

		if (!slist_cacheinfo.value)	//we have to flush it. That's the rules.
		{
			for (oldserver = firstserver; oldserver; oldserver=oldserver->next)
			{
				if (NET_CompareAdr(selectedserver.adr, oldserver->adr))//*(int*)selectedserver.ipaddress == *(int*)server->ipaddress && selectedserver.port == server->port)
				{
					if (oldserver->moreinfo)
					{
						Z_Free(oldserver->moreinfo);
						oldserver->moreinfo = NULL;
					}
					break;
				}
			}
		}

		if (!newserver)
			return;

		selectedserver.adr = newserver->adr;

		if (newserver->moreinfo)	//we cached it.
		{
			selectedserver.detail = newserver->moreinfo;
			return;
		}
//we don't know all the info, so send a request for it.
		selectedserver.detail = newserver->moreinfo = Z_Malloc(sizeof(serverdetailedinfo_t));
		
		newserver->moreinfo->numplayers = newserver->players;
		strcpy(newserver->moreinfo->info, "");
		Info_SetValueForKey(newserver->moreinfo->info, "hostname", newserver->name, sizeof(newserver->moreinfo->info));


		newserver->sends++;
		Master_QueryServer(newserver);
	}
}

#ifdef WEBCLIENT
void MasterInfo_ProcessHTTP(vfsfile_t *file, int type)
{
	netadr_t adr;
	char *s;
	char *el;
	serverinfo_t *info;
	char adrbuf[MAX_ADR_SIZE];
	char linebuffer[2048];

	if (!file)
		return;

	while(VFS_GETS(file, linebuffer, sizeof(linebuffer)))
	{
		s = linebuffer;
		while (*s == '\t' || *s == ' ')
			s++;

		el = s + strlen(s);
		if (el>s && el[-1] == '\r')
			el[-1] = '\0';

		if (*s == '#')	//hash is a comment, apparently.
			continue;

		if (!NET_StringToAdr(s, &adr))
			continue;

		if ((info = Master_InfoForServer(adr)))	//remove if the server already exists.
		{
			info->sends = 1;	//reset.
		}
		else
		{
			info = Z_Malloc(sizeof(serverinfo_t));
			info->adr = adr;
			info->sends = 1;
			info->special = type;
			info->refreshtime = 0;

			snprintf(info->name, sizeof(info->name), "%s", NET_AdrToString(adrbuf, sizeof(adrbuf), info->adr));

			info->next = firstserver;
			firstserver = info;

			Master_ResortServer(info);
		}
	}
}

// wrapper functions for the different server types
void MasterInfo_ProcessHTTPNQ(struct dl_download *dl)
{
	MasterInfo_ProcessHTTP(dl->file, SS_NETQUAKE);
}

void MasterInfo_ProcessHTTPQW(struct dl_download *dl)
{
	MasterInfo_ProcessHTTP(dl->file, SS_GENERICQUAKEWORLD);
}
#endif

//don't try sending to servers we don't support
void MasterInfo_Request(master_t *mast, qboolean evenifwedonthavethefiles)
{
	static int mastersequence;
	if (!mast)
		return;
	switch(mast->type)
	{
#ifdef Q3CLIENT
	case MT_BCASTQ3:
	case MT_SINGLEQ3:
		NET_SendPollPacket (14, va("%c%c%c%cgetstatus\n", 255, 255, 255, 255), mast->adr);
		break;
	case MT_MASTERQ3:
		{
			char *str;
			str = va("%c%c%c%cgetservers %u empty full\x0A\n", 255, 255, 255, 255, 68);
			NET_SendPollPacket (strlen(str), str, mast->adr);
		}
		break;
#endif
#ifdef Q2CLIENT
	case MT_BCASTQ2:
	case MT_SINGLEQ2:
#endif
	case MT_SINGLEQW:
	case MT_BCASTQW:
		NET_SendPollPacket (11, va("%c%c%c%cstatus\n", 255, 255, 255, 255), mast->adr);
		break;
#ifdef NQPROT
	case MT_BCASTNQ:
	case MT_SINGLENQ:
		SZ_Clear(&net_message);
		net_message.packing = SZ_RAWBYTES;
		net_message.currentbit = 0;
		MSG_WriteLong(&net_message, 0);// save space for the header, filled in later
		MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString(&net_message, NET_GAMENAME_NQ);	//look for either sort of server
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		NET_SendPollPacket(net_message.cursize, net_message.data, mast->adr);
		SZ_Clear(&net_message);
		break;
	case MT_MASTERDP:
		{
			char *str;
			str = va("%c%c%c%cgetservers %s %u empty full\x0A\n", 255, 255, 255, 255, com_gamename.string, 3);
			NET_SendPollPacket (strlen(str), str, mast->adr);
		}
		break;
	case MT_SINGLEDP:
	case MT_BCASTDP:
		{
			char *str;
			str = va("%c%c%c%cgetinfo", 255, 255, 255, 255);
			NET_SendPollPacket (strlen(str), str, mast->adr);
		}
		break;
#endif
	case MT_MASTERQW:
		NET_SendPollPacket (3, "c\n", mast->adr);
		break;
#ifdef Q2CLIENT
	case MT_MASTERQ2:
		if (evenifwedonthavethefiles || COM_FDepthFile("pics/colormap.pcx", true)!=0x7fffffff)	//only query this master if we expect to be able to load it's maps.
			NET_SendPollPacket (6, "query", mast->adr);
		break;
#endif
#ifdef WEBCLIENT
	case MT_MASTERHTTP:
		HTTP_CL_Get(mast->address, va("master_%i_%i.tmp", mastersequence++, mast->servertype), MasterInfo_ProcessHTTPNQ);
		break;
	case MT_MASTERHTTPQW:
		HTTP_CL_Get(mast->address, va("master_%i_%i.tmp", mastersequence++, mast->servertype), MasterInfo_ProcessHTTPQW);
		break;
#endif
	}
}


void MasterInfo_WriteServers(void)
{
	char *typename;
	master_t *mast;
	serverinfo_t *server;
	vfsfile_t *mf, *qws;
	char adr[MAX_ADR_SIZE];
	
	mf = FS_OpenVFS("masters.txt", "wt", FS_ROOT);
	if (!mf)
	{
		Con_Printf("Couldn't write masters.txt");
		return;
	}
	
	for (mast = master; mast; mast=mast->next)
	{
		switch(mast->type)
		{
		case MT_MASTERQW:
			typename = "master:qw";
			break;
		case MT_MASTERQ2:
			typename = "master:q2";
			break;
		case MT_MASTERQ3:
			typename = "master:q3";
			break;
		case MT_MASTERDP:
			typename = "master:dp";
			break;
		case MT_MASTERHTTP:
			typename = "master:http";
			break;
		case MT_MASTERHTTPQW:
			typename = "master:httpqw";
			break;
		case MT_BCASTQW:
			typename = "bcast:qw";
			break;
		case MT_BCASTQ2:
			typename = "bcast:q2";
			break;
		case MT_BCASTQ3:
			typename = "bcast:q3";
			break;
		case MT_BCASTNQ:
			typename = "bcast:nq";
			break;
		case MT_SINGLEQW:
			typename = "single:qw";
			break;
		case MT_SINGLEQ2:
			typename = "single:q2";
			break;
		case MT_SINGLEQ3:
			typename = "single:q3";
			break;
		case MT_SINGLENQ:
			typename = "single:nq";
			break;
		case MT_SINGLEDP:
			typename = "single:dp";
			break;
		default:
			typename = "writeerror";
		}
		if (mast->address)
			VFS_PUTS(mf, va("%s\t%s\t%s\n", mast->address , typename, mast->name));
		else
			VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), mast->adr), typename, mast->name));
	}
	
	if (slist_writeserverstxt.value)
		qws = FS_OpenVFS("servers.txt", "wt", FS_ROOT);
	else
		qws = NULL;
	if (qws)
		VFS_PUTS(mf, va("\n%s\t%s\t%s\n\n", "file servers.txt", "favorite:qw", "personal server list"));
		
	for (server = firstserver; server; server = server->next)
	{
		if (server->special & SS_FAVORITE)
		{
			if (server->special & SS_QUAKE3)
				VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), server->adr), "favorite:q3", server->name));
			else if (server->special & SS_QUAKE2)
				VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), server->adr), "favorite:q2", server->name));
			else if (server->special & SS_NETQUAKE)
				VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), server->adr), "favorite:nq", server->name));
			else if (qws)	//servers.txt doesn't support the extra info.
				VFS_PUTS(qws, va("%s\t%s\n", NET_AdrToString(adr, sizeof(adr), server->adr), server->name));
			else	//read only? damn them!
				VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), server->adr), "favorite:qw", server->name));
		}
	}
	
	if (qws)
		VFS_CLOSE(qws);
	
	
	VFS_CLOSE(mf);
}

//poll master servers for server lists.
void MasterInfo_Begin(void)
{
	master_t *mast;
	if (!Master_LoadMasterList("masters.txt",			MT_MASTERQW, 5))
	{
		Master_LoadMasterList("servers.txt",			MT_SINGLEQW, 1);

//		if (q1servers)	//qw master servers
		{
			Master_AddMasterHTTP("http://www.gameaholic.com/servers/qspy-quakeworld", MT_MASTERHTTPQW, "gameaholic's QW master");
			Master_AddMaster("192.246.40.37:27000",			MT_MASTERQW, "id Limbo");
			Master_AddMaster("192.246.40.37:27002",			MT_MASTERQW, "id CTF");
			Master_AddMaster("192.246.40.37:27003",			MT_MASTERQW, "id TeamFortress");
			Master_AddMaster("192.246.40.37:27004",			MT_MASTERQW, "id Miscilaneous");
			Master_AddMaster("192.246.40.37:27006",			MT_MASTERQW, "id Deathmatch Only");
			Master_AddMaster("150.254.66.120:27000",		MT_MASTERQW, "Poland's master server.");
			Master_AddMaster("62.112.145.129:27000",		MT_MASTERQW, "Ocrana master server.");

//			Master_AddMaster("master.edome.net",			MT_MASTERQW, "edome master server.");
	//down	Master_AddMaster("qwmaster.barrysworld.com",	MT_MASTERQW, "barrysworld master server.");
			Master_AddMaster("qwmaster.ocrana.de:27000",	MT_MASTERQW, "Ocrana2 master server.");
			Master_AddMaster("213.221.174.165:27000",		MT_MASTERQW, "unknown1 master server.");
			Master_AddMaster("195.74.0.8",					MT_MASTERQW, "unknown2 master server.");
			Master_AddMaster("204.182.161.2",				MT_MASTERQW, "unknown5 master server.");

			Master_AddMaster("asgaard.morphos-team.net:27000",MT_MASTERQW, "Asgaard");
			Master_AddMaster("master.quakeservers.net:27000",MT_MASTERQW, "QuakeServers.net");

			Master_AddMaster("255.255.255.255:27500",		MT_BCASTQW, "Nearby QuakeWorld UDP servers.");
		}

//		if (q1servers)	//nq master servers
		{
			Master_AddMasterHTTP("http://www.gameaholic.com/servers/qspy-quake", MT_MASTERHTTP, "gameaholic's NQ master");
			Master_AddMaster("255.255.255.255:26000",		MT_BCASTNQ, "Nearby Quake1 servers");

			Master_AddMaster("ghdigital.com:27950",				MT_MASTERDP, "DarkPlaces Master 1");
			Master_AddMaster("dpmaster.deathmask.net:27950",	MT_MASTERDP, "DarkPlaces Master 2");
			Master_AddMaster("12.166.196.192:27950",			MT_MASTERDP, "DarkPlaces Master 3");

			Master_AddMaster("255.255.255.255:26000",			MT_BCASTDP, "Nearby DarkPlaces servers");
		}

//		if (q2servers)	//q2
		{
			Master_AddMaster("255.255.255.255:27910",		MT_BCASTQ2, "Nearby Quake2 UDP servers.");
			Master_AddMaster("00000000:ffffffffffff:27910",	MT_BCASTQ2, "Nearby Quake2 IPX servers.");
			Master_AddMaster("192.246.40.37:27900",			MT_MASTERQ2, "id q2 Master.");
		}

		//q3
		{
		Master_AddMaster("255.255.255.255:27960",			MT_BCASTQ3, "Nearby Quake3 UDP servers.");
		Master_AddMaster("master.quake3arena.com:27950",	MT_MASTERQ3, "Quake3 master server.");
		}
	}


	for (mast = master; mast; mast=mast->next)
	{
		MasterInfo_Request(mast, false);
	}
}

void Master_QueryServer(serverinfo_t *server)
{
	char	data[2048];
	server->sends--;
	server->refreshtime = Sys_DoubleTime();
	if (server->special & SS_QUAKE3)
		sprintf(data, "%c%c%c%cgetstatus", 255, 255, 255, 255);
	else if (server->special & SS_DARKPLACES)
		sprintf(data, "%c%c%c%cgetinfo", 255, 255, 255, 255);
	else if (server->special & SS_NETQUAKE)
	{
#ifdef NQPROT
		SZ_Clear(&net_message);
		net_message.packing = SZ_RAWBYTES;
		net_message.currentbit = 0;
		MSG_WriteLong(&net_message, 0);// save space for the header, filled in later
		MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString(&net_message, NET_GAMENAME_NQ);	//look for either sort of server
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		NET_SendPollPacket(net_message.cursize, net_message.data, server->adr);
		SZ_Clear(&net_message);
#endif
		return;
	}
	else
		sprintf(data, "%c%c%c%cstatus", 255, 255, 255, 255);
	NET_SendPollPacket (strlen(data), data, server->adr);
}
//send a packet to each server in sequence.
void CL_QueryServers(void)
{
	static int poll;
	int op;
	serverinfo_t *server;	

	extern cvar_t	sb_hidequake2;
	extern cvar_t	sb_hidequake3;
	extern cvar_t	sb_hidenetquake;
	extern cvar_t	sb_hidequakeworld;

	op = poll;
	

	for (server = firstserver; op>0 && server; server=server->next, op--);

	if (!server)
	{
		poll = 0;
		return;
	}

	if (op == 0)
	{

		//we only want to send poll packets to servers which will not be filtered (otherwise it's pointless)
		while(server)
		{
			if (server->special & SS_QUAKE3 && !sb_hidequake3.value)
				break;
			if (server->special & SS_QUAKE2 && !sb_hidequake2.value)
				break;
			if ((server->special & (SS_NETQUAKE|SS_DARKPLACES)) && !sb_hidenetquake.value)
				break;
			if ((server->special & (SS_QUAKE3|SS_QUAKE2|SS_DARKPLACES|SS_NETQUAKE))==0 && !sb_hidequakeworld.value)
				break;
			server = server->next;
			poll++;
		}
		if (!server)
		{
			server = firstserver;
			while (server)
			{
				if (server->special & SS_QUAKE3 && !sb_hidequake3.value)
					break;
				if (server->special & SS_QUAKE2 && !sb_hidequake2.value)
					break;
				if ((server->special & (SS_NETQUAKE|SS_DARKPLACES)) && !sb_hidenetquake.value)
					break;
				if ((server->special & (SS_QUAKE3|SS_QUAKE2|SS_DARKPLACES|SS_NETQUAKE))==0 && !sb_hidequakeworld.value)
					break;
				server = server->next;
				poll++;
			}

		}
		if (server && server->sends > 0)
		{
			Master_QueryServer(server);
		}
		poll++;
		return;
	}
	

	poll = 0;
}

int Master_TotalCount(void)
{
	int count=0;
	serverinfo_t *info;

	for (info = firstserver; info; info = info->next)
	{
		count++;
	}
	return count;
}

//true if server is on a different master's list.
serverinfo_t *Master_InfoForServer (netadr_t addr)
{
	serverinfo_t *info;

	for (info = firstserver; info; info = info->next)
	{
		if (NET_CompareAdr(info->adr, addr))
			return info;
	}
	return NULL;
}
serverinfo_t *Master_InfoForNum (int num)
{
	serverinfo_t *info;

	for (info = firstserver; info; info = info->next)
	{
		if (num-- <=0)
			return info;
	}
	return NULL;
}

void MasterInfo_RemovePlayers(netadr_t adr)
{
	player_t *p, *prev;
	prev = NULL;
	for (p = mplayers; p; )
	{
		if (NET_CompareAdr(p->adr, adr))
		{
			if (prev)
				prev->next = p->next;
			else
				mplayers = p->next;
			Z_Free(p);
			p=prev;

			continue;
		}
		else
			prev = p;

		p = p->next;
	}
}

void MasterInfo_AddPlayer(netadr_t serveradr, char *name, int ping, int frags, int colours, char *skin)
{
	player_t *p;
	p = Z_Malloc(sizeof(player_t));
	p->next = mplayers;
	p->adr = serveradr;
	p->colour = colours;
	p->frags = frags;
	Q_strncpyz(p->name, name, sizeof(p->name));
	Q_strncpyz(p->skin, skin, sizeof(p->skin));
	mplayers = p;
}

//we got told about a server, parse it's info
int CL_ReadServerInfo(char *msg, int servertype, qboolean favorite)
{
	serverdetailedinfo_t details;

	char *token;
	char *nl;
	char *name;	
	int ping;
	int len;
	serverinfo_t *info;
	char adr[MAX_ADR_SIZE];

	info = Master_InfoForServer(net_from);

	if (!info)	//not found...
	{
		if (atoi(Info_ValueForKey(msg, "sv_punkbuster")))
			return false;	//never add servers that require punkbuster. :(
		if (atoi(Info_ValueForKey(msg, "sv_pure")))
			return false;	//we don't support the filesystem hashing. :(

		info = Z_Malloc(sizeof(serverinfo_t));

		info->adr = net_from;

		snprintf(info->name, sizeof(info->name), "%s", NET_AdrToString(adr, sizeof(adr), info->adr));

		info->next = firstserver;
		firstserver = info;

	}
	else
	{
		MasterInfo_RemovePlayers(info->adr);
	}

	nl = strchr(msg, '\n');
	if (nl)
	{
		*nl = '\0';
		nl++;
	}
	name = Info_ValueForKey(msg, "hostname");
	if (!*name)
		name = Info_ValueForKey(msg, "sv_hostname");
	Q_strncpyz(info->name, name, sizeof(info->name));
	info->special = info->special & (SS_FAVORITE | SS_KEEPINFO);	//favorite is never cleared
	if (!strcmp(DISTRIBUTION, Info_ValueForKey(msg, "*distrib")))
		info->special |= SS_FTESERVER;
	else if (!strncmp(DISTRIBUTION, Info_ValueForKey(msg, "*version"), 3))
		info->special |= SS_FTESERVER;

	if (servertype == MT_SINGLEDP)
	{
		if (atoi(Info_ValueForKey(msg, "protocol")) > 60)
			info->special |= SS_QUAKE3;
		else
			info->special |= SS_DARKPLACES;
	}
	else if (servertype == MT_SINGLEQ2)
		info->special |= SS_QUAKE2;
	else if (servertype == MT_SINGLEQ3)
		info->special |= SS_QUAKE3;
	else if (servertype == MT_SINGLENQ)
		info->special |= SS_NETQUAKE;
	if (favorite)	//was specifically named, not retrieved from a master.
		info->special |= SS_FAVORITE;

	ping = (Sys_DoubleTime() - info->refreshtime)*1000;
	if (ping > 0xffff)
		info->ping = 0xffff;
	else
		info->ping = ping;

	info->players = 0;
	info->maxplayers = atoi(Info_ValueForKey(msg, "maxclients"));
	if (!info->maxplayers)
		info->maxplayers = atoi(Info_ValueForKey(msg, "sv_maxclients"));

	info->tl = atoi(Info_ValueForKey(msg, "timelimit"));
	info->fl = atoi(Info_ValueForKey(msg, "fraglimit"));

	if (*Info_ValueForKey(msg, "*qtv"))
		info->special |= SS_PROXY|SS_FTESERVER;
	if (!strcmp(Info_ValueForKey(msg, "*progs"), "666") && !strcmp(Info_ValueForKey(msg, "*version"), "2.91"))
		info->special |= SS_PROXY;	//qizmo

	if (servertype == MT_SINGLEQ3 || servertype == MT_SINGLEQ2 || servertype == MT_SINGLEDP)
	{
		Q_strncpyz(info->gamedir,	Info_ValueForKey(msg, "gamename"),	sizeof(info->gamedir));
		Q_strncpyz(info->map,		Info_ValueForKey(msg, "mapname"),	sizeof(info->map));
	}
	else
	{
		Q_strncpyz(info->gamedir,	Info_ValueForKey(msg, "*gamedir"),	sizeof(info->gamedir));
		Q_strncpyz(info->map,		Info_ValueForKey(msg, "map"),		sizeof(info->map));
	}
	Q_strncpyz(info->qcstatus,		Info_ValueForKey(msg, "qcstatus"),	sizeof(info->qcstatus));
	Q_strncpyz(info->modname,		Info_ValueForKey(msg, "modname"),	sizeof(info->modname));

	info->protocol = atoi(Info_ValueForKey(msg, "protocol"));
	info->gameversion = atoi(Info_ValueForKey(msg, "gameversion"));

	info->numbots = atoi(Info_ValueForKey(msg, "bots"));
	info->numhumans = info->players - info->numbots;
	info->freeslots = info->maxplayers - info->players;

	strcpy(details.info, msg);
	msg = msg+strlen(msg)+1;

	info->players=details.numplayers = 0;
	if (!strchr(msg, '\n'))
		info->players = atoi(Info_ValueForKey(details.info, "clients"));
	else
	{
		int clnum;

		for (clnum=0; clnum < MAX_CLIENTS; clnum++)
		{
			nl = strchr(msg, '\n');
			if (!nl)
				break;
			*nl = '\0';

			token = msg;
			if (!token)
				break;
			details.players[clnum].userid = atoi(token);
			token = strchr(token+1, ' ');
			if (!token)
				break;
			details.players[clnum].frags = atoi(token);
			token = strchr(token+1, ' ');
			if (!token)
				break;
			details.players[clnum].time = atoi(token);
			msg = token;
			token = strchr(msg+1, ' ');
			if (!token)	//probably q2 response
			{
				//see if this is actually a Quake2 server.
				token = strchr(msg+1, '\"');
				if (!token)	//it wasn't.
					break;

				details.players[clnum].ping = details.players[clnum].frags;
				details.players[clnum].frags = details.players[clnum].userid;

				msg = strchr(token+1, '\"');
				if (!msg)
					break;
				len = msg - token;
				if (len >= sizeof(details.players[clnum].name))
					len = sizeof(details.players[clnum].name);
				Q_strncpyz(details.players[clnum].name, token+1, len);

				details.players[clnum].skin[0] = '\0';

				details.players[clnum].topc = 0;
				details.players[clnum].botc = 0;
				details.players[clnum].time = 0;
			}
			else	//qw responce
			{
				details.players[clnum].time = atoi(token);
				msg = token;
				token = strchr(msg+1, ' ');
				if (!token)
					break;

				details.players[clnum].ping = atoi(token);

				token = strchr(token+1, '\"');
				if (!token)
					break;
				msg = strchr(token+1, '\"');
				if (!msg)
					break;
				len = msg - token;
				if (len >= sizeof(details.players[clnum].name))
					len = sizeof(details.players[clnum].name);
				Q_strncpyz(details.players[clnum].name, token+1, len);
				details.players[clnum].name[len] = '\0';

				token = strchr(msg+1, '\"');
				if (!token)
					break;
				msg = strchr(token+1, '\"');
				if (!msg)
					break;
				len = msg - token;
				if (len >= sizeof(details.players[clnum].skin))
					len = sizeof(details.players[clnum].skin);
				Q_strncpyz(details.players[clnum].skin, token+1, len);
				details.players[clnum].skin[len] = '\0';

				token = strchr(msg+1, ' ');
				if (!token)
					break;
				details.players[clnum].topc = atoi(token);
				token = strchr(token+1, ' ');
				if (!token)
					break;
				details.players[clnum].botc = atoi(token);
			}

			MasterInfo_AddPlayer(info->adr, details.players[clnum].name, details.players[clnum].ping, details.players[clnum].frags, details.players[clnum].topc*4 | details.players[clnum].botc, details.players[clnum].skin);

			info->players = ++details.numplayers;

			msg = nl;
			if (!msg)
				break;	//erm...
			msg++;
		}
	}
	if (!info->moreinfo && ((slist_cacheinfo.value == 2 || NET_CompareAdr(info->adr, selectedserver.adr)) || (info->special & SS_KEEPINFO)))
		info->moreinfo = Z_Malloc(sizeof(serverdetailedinfo_t));
	if (NET_CompareAdr(info->adr, selectedserver.adr))
		selectedserver.detail = info->moreinfo;

	if (info->moreinfo)
		memcpy(info->moreinfo, &details, sizeof(serverdetailedinfo_t));

	return true;
}

//rewrite to scan for existing server instead of wiping all.
void CL_MasterListParse(netadrtype_t adrtype, int type, qboolean slashpad)
{
	serverinfo_t *info;
	serverinfo_t *last, *old;
	int adrlen;

	int p1, p2;
	char adr[MAX_ADR_SIZE];
	int i;

	switch(adrtype)
	{
	case NA_IP:
		adrlen = 4;
		break;
	case NA_IPV6:
		adrlen = 16;
		break;
	case NA_IPX:
		adrlen = 10;
		break;
	default:
		return;
	}

	MSG_ReadByte ();

	last = firstserver;

	while(msg_readcount+adrlen+2 < net_message.cursize)
	{
		if (slashpad)
		{
			if (MSG_ReadByte() != '\\')
				break;
		}

		info = Z_Malloc(sizeof(serverinfo_t));
		info->adr.type = adrtype;
		switch(adrtype)
		{
		case NA_IP:
		case NA_IPV6:
		case NA_IPX:
			//generic fixed-length addresses
			for (i = 0; i < adrlen; i++)
				((qbyte *)&info->adr.address)[i] = MSG_ReadByte();
			break;
		}

		p1 = MSG_ReadByte();
		p2 = MSG_ReadByte();
		info->adr.port = htons((unsigned short)((p1<<8)|p2));
		if (!info->adr.port)
		{
			Z_Free(info);
			break;
		}
		if ((old = Master_InfoForServer(info->adr)))	//remove if the server already exists.
		{
			old->sends = 1;	//reset.
			Z_Free(info);
		}
		else
		{
			info->sends = 1;
			info->special = type;
			info->refreshtime = 0;

			snprintf(info->name, sizeof(info->name), "%s", NET_AdrToString(adr, sizeof(adr), info->adr));

			info->next = last;
			last = info;

			Master_ResortServer(info);
		}		
	}

	firstserver = last;
}

#endif

