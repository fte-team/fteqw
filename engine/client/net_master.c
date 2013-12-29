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
int CL_ReadServerInfo(char *msg, enum masterprotocol_e prototype, qboolean favorite);
void MasterInfo_RemoveAllPlayers(void);

master_t *master;
player_t *mplayers;
serverinfo_t *firstserver;
struct selectedserver_s selectedserver;

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
	if (numvisibleservers >= maxvisibleservers)
	{
		maxvisibleservers = maxvisibleservers+10;
		visibleservers = BZ_Realloc(visibleservers, maxvisibleservers*sizeof(serverinfo_t*));
	}
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
	case SLIST_TEST_STARTSWITH:
	case SLIST_TEST_EQUAL:
		return a==b;
	case SLIST_TEST_GREATER:
		return a>b;
	case SLIST_TEST_GREATEREQUAL:
		return a>=b;
	case SLIST_TEST_NOTSTARTSWITH:
	case SLIST_TEST_NOTEQUAL:
		return a!=b;
	}
	return false;
}
qboolean Master_CompareString(char *a, char *b, slist_test_t rule)
{
	switch(rule)
	{
	case SLIST_TEST_STARTSWITH:
		return strnicmp(a, b, strlen(b))==0;
	case SLIST_TEST_NOTSTARTSWITH:
		return strnicmp(a, b, strlen(b))!=0;
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

	// warning: enumeration value ‘SLKEY_*’ not handled in switch
	case SLKEY_MOD:
	case SLKEY_PROTOCOL:
	case SLKEY_NUMBOTS:
	case SLKEY_NUMHUMANS:
	case SLKEY_QCSTATUS:
	case SLKEY_ISFAVORITE:
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

	if (nextsort < Sys_DoubleTime())
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
	static char adr[MAX_ADR_SIZE];

	if (!server)
		return "";

	if (keynum < SLKEY_CUSTOM)
	{
		switch(keynum)
		{
		case SLKEY_MAP:
			return server->map;
		case SLKEY_NAME:
			return server->name;
		case SLKEY_ADDRESS:
			return NET_AdrToString(adr, sizeof(adr), &server->adr);
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
	else if (!strcmp(keyname, "name") || !strcmp(keyname, "hostname"))
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




void Master_AddMaster (char *address, enum mastertype_e mastertype, enum masterprotocol_e protocol, char *description)
{
	netadr_t adr;
	master_t *mast;

	if (!NET_StringToAdr(address, 0, &adr))
	{
		Con_Printf("Failed to resolve address \"%s\"\n", address);
		return;
	}

	if (adr.type != NA_IP && adr.type != NA_IPV6 && adr.type != NA_IPX)
	{
		Con_Printf("Fixme: unable to poll address family for \"%s\"\n", address);
		return;
	}

	if (mastertype == MT_BCAST)	//broadcasts
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
		if (NET_CompareAdr(&mast->adr, &adr) && mast->mastertype == mastertype && mast->protocoltype == protocol)	//already exists.
			return;
	}
	mast = Z_Malloc(sizeof(master_t)+strlen(description)+1+strlen(address)+1);
	mast->adr = adr;
	mast->address = mast->name + strlen(description)+1;
	mast->mastertype = mastertype;
	mast->protocoltype = protocol;
	strcpy(mast->name, description);
	strcpy(mast->address, address);

	mast->next = master;
	master = mast;
}

void MasterInfo_Shutdown(void)
{
	master_t *mast;
	serverinfo_t *sv;
	MasterInfo_RemoveAllPlayers();
	while(firstserver)
	{
		sv = firstserver;
		firstserver = sv->next;
		Z_Free(sv);
	}
	while(master)
	{
		mast = master;
		master = mast->next;
#ifdef WEBCLIENT
		if (mast->dl)
			DL_Close(mast->dl);
#endif
		Z_Free(mast);
	}

	maxvisibleservers = 0;
	numvisibleservers = 0;
	Z_Free(visibleservers);
}

void Master_AddMasterHTTP (char *address, int mastertype, int protocoltype, char *description)
{
	master_t *mast;
/*	int servertype;

	if (protocoltype == MP_DP)
		servertype = SS_DARKPLACES;
	else if (protocoltype == MP_Q2)
		servertype = SS_QUAKE2;
	else if (protocoltype == MP_Q3)
		servertype = SS_QUAKE3;
	else if (protocoltype == MP_NQ)
		servertype = SS_NETQUAKE;
	else
		servertype = 0;
*/
	for (mast = master; mast; mast = mast->next)
	{
		if (!strcmp(mast->address, address) && mast->mastertype == mastertype && mast->protocoltype == protocoltype)	//already exists.
			return;
	}
	mast = Z_Malloc(sizeof(master_t)+strlen(description)+1+strlen(address)+1);
	mast->address = mast->name + strlen(description)+1;
	mast->mastertype = mastertype;
	mast->protocoltype = protocoltype;
//	mast->servertype = servertype;
	strcpy(mast->name, description);
	strcpy(mast->address, address);

	mast->next = master;
	master = mast;
}

//build a linked list of masters.	Doesn't duplicate addresses.
qboolean Master_LoadMasterList (char *filename, qboolean withcomment, int defaulttype, int defaultprotocol, int depth)
{
	vfsfile_t *f;
	char line[1024];
	char name[1024];
	char entry[1024];
	char *next, *sep;
	int servertype;
	int protocoltype;
	qboolean favourite;

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

		*name = 0;
		favourite = false;
		servertype = defaulttype;
		protocoltype = defaultprotocol;

		next = COM_ParseOut(line, entry, sizeof(entry));
		if (!*com_token)
			continue;

		//special cases. Add a port if you have a server named 'file'... (unlikly)
		if (!strcmp(entry, "file"))
		{
			if (withcomment)
				next = COM_ParseOut(next, name, sizeof(name));
			next = COM_ParseOut(next, entry, sizeof(entry));
			if (!next)
				continue;
			servertype = MT_BAD;
		}
		else if (!strcmp(entry, "master"))
		{
			if (withcomment)
				next = COM_ParseOut(next, name, sizeof(name));
			next = COM_ParseOut(next, entry, sizeof(entry));
			if (!next)
				continue;
			servertype = MT_MASTERUDP;
		}
		else if (!strcmp(entry, "url"))
		{
			if (withcomment)
				next = COM_ParseOut(next, name, sizeof(name));
			next = COM_ParseOut(next, entry, sizeof(entry));
			servertype = MT_MASTERHTTP;
		}

		next = COM_Parse(next);

		for(sep = com_token; sep; sep = next)
		{
			next = strchr(sep, ':');
			if (next)
				*next = 0;

			if (!strcmp(sep, "single"))
				servertype = MT_SINGLE;
			else if (!strcmp(sep, "master"))
				servertype = MT_MASTERUDP;
			else if (!strcmp(sep, "masterhttp"))
				servertype = MT_MASTERHTTP;
			else if (!strcmp(sep, "masterhttpjson"))
				servertype = MT_MASTERHTTPJSON;
			else if (!strcmp(sep, "bcast"))
				servertype = MT_BCAST;

			else if (!strcmp(com_token, "qw"))
				protocoltype = MP_QW;
			else if (!strcmp(com_token, "q2"))
				protocoltype = MP_Q2;
			else if (!strcmp(com_token, "q3"))
				protocoltype = MP_Q3;
			else if (!strcmp(com_token, "nq"))
				protocoltype = MP_NQ;
			else if (!strcmp(com_token, "dp"))
				protocoltype = MP_DP;

			//legacy compat
			else if (!strcmp(com_token, "httpjson"))
			{
				servertype = MT_MASTERHTTPJSON;
				protocoltype = MP_NQ;
			}
			else if (!strcmp(com_token, "httpnq"))
			{
				servertype = MT_MASTERHTTP;
				protocoltype = MP_NQ;
			}
			else if (!strcmp(com_token, "httpqw"))
			{
				servertype = MT_MASTERHTTP;
				protocoltype = MP_QW;
			}

			else if (!strcmp(com_token, "favourite") || !strcmp(com_token, "favorite"))
				favourite = true;
		}

		if (!*name)
		{
			sep = name;
			while(*next == ' ' || *next == '\t')
				next++;
			while (*next && sep < name+sizeof(name)-1)
				*sep++ = *next++;
			*sep = 0;
		}

		if (servertype == MT_BAD)
			Master_LoadMasterList(entry, false, servertype, protocoltype, depth);
		else
		{
			//favourites are added explicitly, with their name and stuff
			if (favourite && servertype == MT_SINGLE)
			{
				if (NET_StringToAdr(entry, 0, &net_from))
					CL_ReadServerInfo(va("\\hostname\\%s", name), -servertype, true);
				else
					Con_Printf("Failed to resolve address - \"%s\"\n", entry);
			}

			switch (servertype)
			{
			case MT_MASTERHTTPJSON:
			case MT_MASTERHTTP:
				Master_AddMasterHTTP(entry, servertype, protocoltype, name);
				break;
			default:
				Master_AddMaster(entry, servertype, protocoltype, name);
				break;
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

int Master_CheckPollSockets(void)
{
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
					NET_AdrToString (adr, sizeof(adr), &net_from));
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
			Con_Printf ("Oversize packet from %s\n", NET_AdrToString (adr, sizeof(adr), &net_from));
			continue;
		}

		if (*(int *)net_message.data == -1)
		{
			int c;
			char *s;

			MSG_BeginReading (msg_nullnetprim);
			MSG_ReadLong ();        // skip the -1

			c = msg_readcount;
			s = MSG_ReadStringLine();	//peek for q2 messages.
#ifdef Q2CLIENT
			if (!strcmp(s, "print"))
			{
				CL_ReadServerInfo(MSG_ReadString(), MP_Q2, false);
				continue;
			}
			if (!strcmp(s, "info"))	//parse a bit more...
			{
				CL_ReadServerInfo(MSG_ReadString(), MP_Q2, false);
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
				CL_ReadServerInfo(MSG_ReadString(), MP_Q3, false);
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
				CL_ReadServerInfo(MSG_ReadString(), MP_DP, false);
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
				CL_ReadServerInfo(MSG_ReadString(), MP_QW, false);
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

			MSG_BeginReading (msg_nullnetprim);
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

			/*this is an address string sent from the server. its not usable. if its replying to serverinfos, its possible to send it connect requests, while the address that it claims is 50% bugged*/
			MSG_ReadString();

			Q_strncpyz(name, MSG_ReadString(), sizeof(name));
			Q_strncpyz(map, MSG_ReadString(), sizeof(map));
			users = MSG_ReadByte();
			maxusers = MSG_ReadByte();
			if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
			{
//				Q_strcpy(name, "*");
//				Q_strcat(name, name);
			}

			CL_ReadServerInfo(va("\\hostname\\%s\\map\\%s\\maxclients\\%i\\clients\\%i", name, map, maxusers, users), MP_NQ, false);
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
				if (NET_CompareAdr(&selectedserver.adr, &oldserver->adr))//*(int*)selectedserver.ipaddress == *(int*)server->ipaddress && selectedserver.port == server->port)
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
void MasterInfo_ProcessHTTP(struct dl_download *dl)
{
	master_t *mast = dl->user_ctx;
	vfsfile_t *file = dl->file;
	int protocoltype = mast->protocoltype;
	netadr_t adr;
	char *s;
	char *el;
	serverinfo_t *info;
	char adrbuf[MAX_ADR_SIZE];
	char linebuffer[2048];
	mast->dl = NULL;

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

		if (!NET_StringToAdr(s, 80, &adr))
			continue;

		if ((info = Master_InfoForServer(&adr)))	//remove if the server already exists.
		{
			info->sends = 1;	//reset.
		}
		else
		{
			info = Z_Malloc(sizeof(serverinfo_t));
			info->adr = adr;
			info->sends = 1;

			if (protocoltype == MP_DP)
				info->special = SS_DARKPLACES;
			else if (protocoltype == MP_Q2)
				info->special = SS_QUAKE2;
			else if (protocoltype == MP_Q3)
				info->special = SS_QUAKE3;
			else if (protocoltype == MP_NQ)
				info->special = SS_NETQUAKE;
			else
				info->special = 0;

			info->refreshtime = 0;
			info->ping = 0xffff;

			snprintf(info->name, sizeof(info->name), "%s", NET_AdrToString(adrbuf, sizeof(adrbuf), &info->adr));

			info->next = firstserver;
			firstserver = info;

			Master_ResortServer(info);
		}
	}
}

char *jsonnode(int level, char *node)
{
	netadr_t adr = {NA_INVALID};
	char servername[256] = {0};
	char key[256];
	int flags = 0;
	int port = 0;
	int cp = 0, mp = 0;
	if (*node != '{')
		return node;
	do
	{
		node++;
		node = COM_ParseToken(node, ",:{}[]");
		if (*node != ':')
			continue;
		node++;
		if (*node == '[')
		{
			do
			{
				node++;
				node = jsonnode(level+1, node);
				if (!node)
					return NULL;
				if (*node == ']')
				{
					break;
				}
			} while(*node == ',');
			if (*node != ']')
				return NULL;
			node++;
		}
		else
		{
			Q_strncpyz(key, com_token, sizeof(key));
			node = COM_ParseToken(node, ",:{}[]");

			if (level == 1)
			{
				if (!strcmp(key, "IPAddress"))
					NET_StringToAdr(com_token, 0, &adr);
				if (!strcmp(key, "Port"))
					port = atoi(com_token);
				if (!strcmp(key, "DNS"))
					Q_strncpyz(servername, com_token, sizeof(servername));
				if (!strcmp(key, "CurrentPlayerCount"))
					cp = atoi(com_token);
				if (!strcmp(key, "MaxPlayers"))
					mp = atoi(com_token);
				if (!strcmp(key, "Game"))
				{
					if (!strcmp(com_token, "NetQuake"))
						flags |= SS_NETQUAKE;
					if (!strcmp(com_token, "Quake2"))
						flags |= SS_QUAKE2;
					if (!strcmp(com_token, "Quake3"))
						flags |= SS_QUAKE3;
				}
			}
		}
	} while(*node == ',');

	if (*node == '}')
		node++;

	if (adr.type != NA_INVALID)
	{
		serverinfo_t *info;

		if (port)
			adr.port = htons(port);

		if ((info = Master_InfoForServer(&adr)))	//remove if the server already exists.
		{
			info->sends = 1;	//reset.
		}
		else
		{
			info = Z_Malloc(sizeof(serverinfo_t));
			info->adr = adr;
			info->sends = 1;
			info->special = flags;
			info->refreshtime = 0;
			info->players = cp;
			info->maxplayers = mp;

			snprintf(info->name, sizeof(info->name), "%s", *servername?servername:NET_AdrToString(servername, sizeof(servername), &info->adr));

			info->next = firstserver;
			firstserver = info;

			Master_ResortServer(info);
		}
	}

	return node;
}

void MasterInfo_ProcessHTTPJSON(struct dl_download *dl)
{
	int len;
	char *buf;
	master_t *mast = dl->user_ctx;
	mast->dl = NULL;
	if (dl->file)
	{
		len = VFS_GETLEN(dl->file);
		buf = malloc(len + 1);
		VFS_READ(dl->file, buf, len);
		buf[len] = 0;
		jsonnode(0, buf);
		free(buf);
	}
	else
	{
		Con_Printf("Unable to query master at \"%s\"\n", dl->url);
	}
}
#endif

//don't try sending to servers we don't support
void MasterInfo_Request(master_t *mast)
{
	//static int mastersequence; // warning: unused variable âmastersequenceâ
	if (!mast)
		return;

	if (mast->sends)
		mast->sends--;

	//these are generic requests
	switch(mast->mastertype)
	{
#ifdef WEBCLIENT
	case MT_MASTERHTTPJSON:
		if (!mast->dl)
		{
			mast->dl = HTTP_CL_Get(mast->address, NULL, MasterInfo_ProcessHTTPJSON);
			if (mast->dl)
				mast->dl->user_ctx = mast;
		}
		break;
	case MT_MASTERHTTP:
		if (!mast->dl)
		{
			mast->dl = HTTP_CL_Get(mast->address, NULL, MasterInfo_ProcessHTTP);
			if (mast->dl)
				mast->dl->user_ctx = mast;
		}
		break;
#endif
	case MT_MASTERUDP:
		switch(mast->protocoltype)
		{
#ifdef Q3CLIENT
		case MP_Q3:
			{
				char *str;
				str = va("%c%c%c%cgetservers %u empty full\x0A\n", 255, 255, 255, 255, 68);
				NET_SendPollPacket (strlen(str), str, mast->adr);
			}
			break;
#endif
#ifdef Q2CLIENT
		case MP_Q2:
			NET_SendPollPacket (6, "query", mast->adr);
			break;
#endif
		case MP_QW:
			NET_SendPollPacket (3, "c\n", mast->adr);
			break;
#ifdef NQPROT
		case MP_NQ:
			//there is no nq udp master protocol
			break;
		case MP_DP:
			{
				char *str;
				str = va("%c%c%c%cgetservers %s %u empty full"/*\x0A\n"*/, 255, 255, 255, 255, com_protocolname.string, 3);
				NET_SendPollPacket (strlen(str), str, mast->adr);
			}
			break;
#endif
		}
		break;
	case MT_BCAST:
	case MT_SINGLE:	//FIXME: properly add the server and flag it for resending instead of directly pinging it
		switch(mast->protocoltype)
		{
#ifdef Q3CLIENT
		case MP_Q3:
			NET_SendPollPacket (14, va("%c%c%c%cgetstatus\n", 255, 255, 255, 255), mast->adr);
			break;
#endif
#ifdef Q2CLIENT
		case MP_Q2:
#endif
		case MP_QW:
			NET_SendPollPacket (11, va("%c%c%c%cstatus\n", 255, 255, 255, 255), mast->adr);
			break;
#ifdef NQPROT
		case MP_NQ:
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
		case MP_DP:
			{
				char *str;
				str = va("%c%c%c%cgetinfo", 255, 255, 255, 255);
				NET_SendPollPacket (strlen(str), str, mast->adr);
			}
			break;
#endif
		}
		break;
	}
}


void MasterInfo_WriteServers(void)
{
	char *typename, *protoname;
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
		switch(mast->mastertype)
		{
		case MT_MASTERUDP:
			typename = "master";
			break;
		case MT_MASTERHTTP:
			typename = "masterhttp";
			break;
		case MT_MASTERHTTPJSON:
			typename = "masterjson";
			break;
		case MT_BCAST:
			typename = "bcast";
			break;
		case MT_SINGLE:
			typename = "single";
			break;
		default:
			typename = "??";
			break;
		}
		switch(mast->protocoltype)
		{
		case MP_QW:
			protoname = ":qw";
			break;
		case MP_Q2:
			protoname = ":q2";
			break;
		case MP_Q3:
			protoname = ":q3";
			break;
		case MP_NQ:
			protoname = ":nq";
			break;
		case MP_DP:
			protoname = ":dp";
			break;
		default:
		case MP_UNSPECIFIED:
			protoname = "";
			break;
		}
		if (mast->address)
			VFS_PUTS(mf, va("%s\t%s\t%s\n", mast->address, typename, protoname, mast->name));
		else
			VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), &mast->adr), typename, mast->name));
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
				VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), &server->adr), "favorite:q3", server->name));
			else if (server->special & SS_QUAKE2)
				VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), &server->adr), "favorite:q2", server->name));
			else if (server->special & SS_NETQUAKE)
				VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), &server->adr), "favorite:nq", server->name));
			else if (qws)	//servers.txt doesn't support the extra info, so don't write it if its not needed
				VFS_PUTS(qws, va("%s\t%s\n", NET_AdrToString(adr, sizeof(adr), &server->adr), server->name));
			else	//read only? damn them!
				VFS_PUTS(mf, va("%s\t%s\t%s\n", NET_AdrToString(adr, sizeof(adr), &server->adr), "favorite:qw", server->name));
		}
	}

	if (qws)
		VFS_CLOSE(qws);


	VFS_CLOSE(mf);
}

//poll master servers for server lists.
void MasterInfo_Refresh(void)
{
	master_t *mast;
	qboolean loadedone;

	loadedone = false;
	loadedone |= Master_LoadMasterList("masters.txt", false, MT_MASTERUDP, MP_QW, 5);	//fte listing
	loadedone |= Master_LoadMasterList("sources.txt", true, MT_MASTERUDP, MP_QW, 5);	//merge with ezquake compat listing

	if (!loadedone)
	{
		Master_LoadMasterList("servers.txt", false, MT_MASTERUDP, MP_QW, 1);

//		if (q1servers)	//qw master servers
		{
			Master_AddMasterHTTP("http://www.gameaholic.com/servers/qspy-quakeworld",	MT_MASTERHTTP,	MP_QW, "gameaholic's QW master");
			Master_AddMasterHTTP("http://www.quakeservers.net/lists/servers/global.txt",MT_MASTERHTTP,	MP_QW, "QuakeServers.net (http)");
			//Master_AddMaster("satan.idsoftware.com:27000",			MT_MASTERUDP,	MP_QW, "id Limbo");
			//Master_AddMaster("satan.idsoftware.com:27002",			MT_MASTERUDP,	MP_QW, "id CTF");
			//Master_AddMaster("satan.idsoftware.com:27003",			MT_MASTERUDP,	MP_QW, "id TeamFortress");
			//Master_AddMaster("satan.idsoftware.com:27004",			MT_MASTERUDP,	MP_QW, "id Miscilaneous");
			//Master_AddMaster("satan.idsoftware.com:27006",			MT_MASTERUDP,	MP_QW, "id Deathmatch Only");
			//Master_AddMaster("150.254.66.120:27000",					MT_MASTERUDP,	MP_QW, "Poland's master server.");
			//Master_AddMaster("62.112.145.129:27000",					MT_MASTERUDP,	MP_QW, "Ocrana master server.");
			//Master_AddMaster("master.edome.net",						MT_MASTERUDP,	MP_QW, "edome master server.");
			//Master_AddMaster("qwmaster.barrysworld.com",				MT_MASTERUDP,	MP_QW, "barrysworld master server.");
			//Master_AddMaster("213.221.174.165:27000",					MT_MASTERUDP,	MP_QW, "unknown1 master server.");
			//Master_AddMaster("195.74.0.8",							MT_MASTERUDP,	MP_QW, "unknown2 master server.");
			//Master_AddMaster("204.182.161.2",							MT_MASTERUDP,	MP_QW, "unknown5 master server.");
			//Master_AddMaster("kubus.rulez.pl:27000",					MT_MASTERUDP,	MP_QW, "Kubus");
			//Master_AddMaster("telefrag.me:27000",						MT_MASTERUDP,	MP_QW, "Telefrag.ME");
			//Master_AddMaster("master.teamdamage.com:27000",			MT_MASTERUDP,	MP_QW, "TeamDamage");
			Master_AddMaster("master.quakeservers.net:27000",			MT_MASTERUDP,	MP_QW, "QuakeServers.net");
//			Master_AddMaster("masterserver.exhale.de:27000",			MT_MASTERUDP,	MP_QW, "team exhale");
			Master_AddMaster("qwmaster.fodquake.net:27000",				MT_MASTERUDP,	MP_QW, "Fodquake master server.");
			Master_AddMaster("qwmaster.ocrana.de:27000",				MT_MASTERUDP,	MP_QW, "Ocrana2 master server.");
			Master_AddMaster("255.255.255.255:27500",					MT_BCAST,		MP_QW, "Nearby QuakeWorld UDP servers.");
		}

//		if (q1servers)	//nq master servers
		{
			//Master_AddMaster("12.166.196.192:27950",									MT_MASTERUDP,		MP_DP, "DarkPlaces Master 3");
			Master_AddMasterHTTP("http://www.gameaholic.com/servers/qspy-quake",		MT_MASTERHTTP,		MP_NQ, "gameaholic's NQ master");
			Master_AddMasterHTTP("http://servers.quakeone.com/index.php?format=json",	MT_MASTERHTTPJSON,	MP_NQ, "quakeone's server listing");
			Master_AddMaster("69.59.212.88:27950"/*"ghdigital.com:27950"*/,				MT_MASTERUDP,		MP_DP, "DarkPlaces Master 1"); // LordHavoc
			Master_AddMaster("64.22.107.125:27950"/*"dpmaster.deathmask.net:27950"*/,	MT_MASTERUDP,		MP_DP, "DarkPlaces Master 2"); // Willis
			Master_AddMaster("92.62.40.73:27950"/*"dpmaster.tchr.no:27950"*/,			MT_MASTERUDP,		MP_DP, "DarkPlaces Master 3"); // tChr
#ifdef IPPROTO_IPV6
			//Master_AddMaster("[2001:41d0:2:1628::4450]:27950",						MT_MASTERUDP,		MP_DP, "DarkPlaces Master 4"); // dpmaster.div0.qc.to (admin: divVerent)
#endif
			Master_AddMaster("255.255.255.255:26000",									MT_BCAST,			MP_NQ, "Nearby Quake1 servers");
			Master_AddMaster("255.255.255.255:26000",									MT_BCAST,			MP_DP, "Nearby DarkPlaces servers");
		}

//		if (q2servers)	//q2
		{
			Master_AddMasterHTTP("http://www.gameaholic.com/servers/qspy-quake2",		MT_MASTERHTTP,		MP_QW, "gameaholic's Q2 master");
			//Master_AddMaster("satan.idsoftware.com:27900",							MT_MASTERUDP,		MP_Q2, "id q2 Master.");
			//Master_AddMaster("master.planetgloom.com:27900",							MT_MASTERUDP,		MP_Q2, "Planetgloom.com");
			//Master_AddMaster("master.q2servers.com:27900",							MT_MASTERUDP,		MP_Q2, "q2servers.com");
			Master_AddMaster("netdome.biz:27900",										MT_MASTERUDP,		MP_Q2, "Netdome.biz");
//			Master_AddMaster("masterserver.exhale.de:27900",							MT_MASTERUDP,		MP_Q2, "team exhale");
			Master_AddMaster("255.255.255.255:27910",									MT_BCAST,			MP_Q2, "Nearby Quake2 UDP servers.");
#ifdef USEIPX
			Master_AddMaster("00000000:ffffffffffff:27910",								MT_BCAST,			MP_Q2, "Nearby Quake2 IPX servers.");
#endif
		}

		//q3
		{
			Master_AddMasterHTTP("http://www.gameaholic.com/servers/qspy-quake3",		MT_MASTERHTTP,		MP_Q3, "gameaholic's Q3 master");
			Master_AddMaster("master.quake3arena.com:27950",							MT_MASTERUDP,		MP_Q3, "Quake3 master server.");
//			Master_AddMaster("masterserver.exhale.de:27950",							MT_MASTERUDP,		MP_Q3, "team exhale");
			//Master_AddMaster("master3.quake3arena.com:27950",							MT_MASTERUDP,		MP_Q3, "Quake3 master3 server.");
			Master_AddMaster("255.255.255.255:27960",									MT_BCAST,		MP_Q3, "Nearby Quake3 UDP servers.");
		}
	}


	for (mast = master; mast; mast=mast->next)
	{
		mast->sends = 1;
	}

	Master_SortServers();
	nextsort = Sys_DoubleTime() + 2;
}

void Master_QueryServer(serverinfo_t *server)
{
	char	data[2048];
	server->sends--;
	server->refreshtime = Sys_DoubleTime();
	if (server->special & SS_QUAKE3)
		Q_snprintfz(data, sizeof(data), "%c%c%c%cgetstatus", 255, 255, 255, 255);
	else if (server->special & SS_DARKPLACES)
		Q_snprintfz(data, sizeof(data), "%c%c%c%cgetinfo", 255, 255, 255, 255);
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
		Q_snprintfz(data, sizeof(data), "%c%c%c%cstatus", 255, 255, 255, 255);
	NET_SendPollPacket (strlen(data), data, server->adr);
}
//send a packet to each server in sequence.
void CL_QueryServers(void)
{
	static int poll;
	int op;
	serverinfo_t *server;
	master_t *mast;

	extern cvar_t	sb_hidequake2;
	extern cvar_t	sb_hidequake3;
	extern cvar_t	sb_hidenetquake;
	extern cvar_t	sb_hidequakeworld;

	op = poll;

	for (mast = master; mast; mast=mast->next)
	{
		switch (mast->protocoltype)
		{
		case MP_UNSPECIFIED:
			continue;
		case MP_DP:	//dpmaster allows the client to specify the protocol to query. this means it always matches the current game type, so don't bother allowing the user to disable it.
			break;
		case MP_NQ:
			if (sb_hidenetquake.value)
				continue;
			break;
		case MP_QW:
			if (sb_hidequakeworld.value)
				continue;
			break;
		case MP_Q2:
			if (sb_hidequake2.value)
				continue;
			break;
		case MP_Q3:
			if (sb_hidequake3.value)
				continue;
			break;
		}

		if (mast->sends > 0)
			MasterInfo_Request(mast);
	}


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

unsigned int Master_TotalCount(void)
{
	unsigned int count=0;
	serverinfo_t *info;

	for (info = firstserver; info; info = info->next)
	{
		count++;
	}
	return count;
}

unsigned int Master_NumPolled(void)
{
	unsigned int count=0;
	serverinfo_t *info;

	for (info = firstserver; info; info = info->next)
	{
		if (info->maxplayers)
			count++;
	}
	return count;
}

//true if server is on a different master's list.
serverinfo_t *Master_InfoForServer (netadr_t *addr)
{
	serverinfo_t *info;

	for (info = firstserver; info; info = info->next)
	{
		if (NET_CompareAdr(&info->adr, addr))
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

void MasterInfo_RemoveAllPlayers(void)
{
	player_t *p;
	while(mplayers)
	{
		p = mplayers;
		mplayers = p->next;
		Z_Free(p);
	}
}
void MasterInfo_RemovePlayers(netadr_t *adr)
{
	player_t *p, *prev;
	prev = NULL;
	for (p = mplayers; p; )
	{
		if (NET_CompareAdr(&p->adr, adr))
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

void MasterInfo_AddPlayer(netadr_t *serveradr, char *name, int ping, int frags, int colours, char *skin)
{
	player_t *p;
	p = Z_Malloc(sizeof(player_t));
	p->next = mplayers;
	p->adr = *serveradr;
	p->colour = colours;
	p->frags = frags;
	Q_strncpyz(p->name, name, sizeof(p->name));
	Q_strncpyz(p->skin, skin, sizeof(p->skin));
	mplayers = p;
}

//we got told about a server, parse it's info
int CL_ReadServerInfo(char *msg, enum masterprotocol_e prototype, qboolean favorite)
{
	serverdetailedinfo_t details;

	char *token;
	char *nl;
	char *name;
	int ping;
	int len;
	serverinfo_t *info;
	char adr[MAX_ADR_SIZE];

	info = Master_InfoForServer(&net_from);

	if (!info)	//not found...
	{
		if (atoi(Info_ValueForKey(msg, "sv_punkbuster")))
			return false;	//never add servers that require punkbuster. :(
//		if (atoi(Info_ValueForKey(msg, "sv_pure")))
//			return false;	//we don't support the filesystem hashing. :(

		info = Z_Malloc(sizeof(serverinfo_t));

		info->adr = net_from;

		snprintf(info->name, sizeof(info->name), "%s", NET_AdrToString(adr, sizeof(adr), &info->adr));

		info->next = firstserver;
		firstserver = info;

	}
	else
	{
		MasterInfo_RemovePlayers(&info->adr);
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

	if (prototype == MP_DP)
	{
		if (atoi(Info_ValueForKey(msg, "protocol")) > 60)
			info->special |= SS_QUAKE3;
		else
			info->special |= SS_DARKPLACES;
	}
	else if (prototype == MP_Q2)
		info->special |= SS_QUAKE2;
	else if (prototype == MP_Q3)
		info->special |= SS_QUAKE3;
	else if (prototype == MP_NQ)
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

	if (prototype == MP_Q3 || prototype == MP_Q2 || prototype == MP_DP)
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

		for (clnum=0; clnum < cl.allocated_client_slots; clnum++)
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

			MasterInfo_AddPlayer(&info->adr, details.players[clnum].name, details.players[clnum].ping, details.players[clnum].frags, details.players[clnum].topc*4 | details.players[clnum].botc, details.players[clnum].skin);

			info->players = ++details.numplayers;

			msg = nl;
			if (!msg)
				break;	//erm...
			msg++;
		}
	}
	if (!info->moreinfo && ((slist_cacheinfo.value == 2 || NET_CompareAdr(&info->adr, &selectedserver.adr)) || (info->special & SS_KEEPINFO)))
		info->moreinfo = Z_Malloc(sizeof(serverdetailedinfo_t));
	if (NET_CompareAdr(&info->adr, &selectedserver.adr))
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
		default:
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
		if ((old = Master_InfoForServer(&info->adr)))	//remove if the server already exists.
		{
			if ((info->special & (SS_DARKPLACES | SS_NETQUAKE | SS_QUAKE2 | SS_QUAKE3)) && !(type & (SS_DARKPLACES | SS_NETQUAKE | SS_QUAKE2 | SS_QUAKE3)))
				old->special = type | (old->special & SS_FAVORITE);
			old->sends = 1;	//reset.
			Z_Free(info);
		}
		else
		{
			info->sends = 1;
			info->special = type;
			info->refreshtime = 0;

			snprintf(info->name, sizeof(info->name), "%s", NET_AdrToString(adr, sizeof(adr), &info->adr));

			info->next = last;
			last = info;

			Master_ResortServer(info);
		}
	}

	firstserver = last;
}

#endif

