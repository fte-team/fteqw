#include "bothdefs.h"

#ifdef CL_MASTER

#include "quakedef.h"
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
#define EWOULDBLOCK		WSAEWOULDBLOCK
#define ECONNREFUSED	WSAECONNREFUSED
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
#define EMSGSIZE		WSAEMSGSIZE
#define ECONNABORTED	WSAECONNABORTED
#define ECONNRESET		WSAECONNRESET

#define qerrno WSAGetLastError()
#else
#define qerrno errno

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
typedef int SOCKET;
#endif

#ifdef AF_IPX
#define USEIPX
#endif


//the number of servers should be limited only by memory.

cvar_t slist_cacheinfo = {"slist_cacheinfo", "0"};	//this proves dangerous, memory wise.
cvar_t slist_writeserverstxt = {"slist_writeservers", "0"};

void CL_MasterListParse(qboolean isq2);
void CL_QueryServers(void);
int CL_ReadServerInfo(char *msg, int servertype, qboolean favorite);

master_t *master;
player_t *mplayers;
serverinfo_t *firstserver;


#define POLLUDPSOCKETS 64	//it's big so we can have lots of messages when behind a firewall. Basically if a firewall only allows replys, and only remembers 3 servers per socket, we need this big cos it can take a while for a packet to find a fast optimised route and we might be waiting for a few secs for a reply the first time around.
SOCKET pollsocketsUDP[POLLUDPSOCKETS];
int lastpollsockUDP;

#ifdef USEIPX
#define POLLIPXSOCKETS	2	//ipx isn't used as much. In fact, we only expect local servers to be using it. I'm not sure why I implemented it anyway.
SOCKET pollsocketsIPX[POLLIPXSOCKETS];
int lastpollsockIPX;
#else
#define POLLIPXSOCKETS 0
#endif


void NetadrToSockadr (netadr_t *a, struct sockaddr_qstorage *s);



void Master_AddMaster (char *address, int type, char *description)
{
	netadr_t adr;
	master_t *mast;

	if (!NET_StringToAdr(address, &adr))
	{
		Con_Printf("Failed to resolve address \"%s\"\n", address);
	}
/*
	if (type == MT_SINGLEQW || type == MT_SINGLENQ || type == MT_SINGLEQ2)	//single servers are added to the serverlist as well as the masters list
	{
		net_from = adr;
		CL_ReadServerInfo(va("\\hostname\\%s", description), MT_SINGLEQ2, true);
//		return;
	}
*/
	if (type < MT_SINGLEQW)	//broadcasts
	{
		if (adr.type == NA_IP)
			adr.type = NA_BROADCAST_IP;
		if (adr.type == NA_IPX)
			adr.type = NA_BROADCAST_IPX;
	}

	for (mast = master; mast; mast = mast->next)
	{
		if (NET_CompareAdr(mast->adr, adr))	//already exists.
			return;
	}
	mast = Z_Malloc(sizeof(master_t)+strlen(description));
	mast->adr = adr;
	mast->type = type;
	strcpy(mast->name, description);

	mast->next = master;
	master = mast;
}

//build a linked list of masters.	Doesn't duplicate addresses.
qboolean Master_LoadMasterList (char *filename, int defaulttype, int depth)
{
	extern char	com_basedir[MAX_OSPATH];
	FILE *f;
	char line[1024];
	char file[1024];
	char *name, *next;
	int servertype;

	if (depth <= 0)
		return false;
	depth--;

	f = fopen(va("%s/%s", com_basedir, filename), "rb");
	if (!f)
		return false;

	while(fgets(line, sizeof(line)-1, f))
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
		else if (!strcmp(com_token, "single:nq") || !strcmp(com_token, "single:q1"))
			servertype = MT_SINGLENQ;
		else if (!strcmp(com_token, "single"))
			servertype = MT_SINGLEQW;

		else if (!strcmp(com_token, "master:qw"))
			servertype = MT_MASTERQW;
		else if (!strcmp(com_token, "master:q2"))
			servertype = MT_MASTERQ2;
		else if (!strcmp(com_token, "master"))	//any other sort of master, assume it's a qw master.
			servertype = MT_MASTERQW;

		else if (!strcmp(com_token, "bcast:qw"))
			servertype = MT_BCASTQW;
		else if (!strcmp(com_token, "bcast:q2"))
			servertype = MT_BCASTQ2;
		else if (!strcmp(com_token, "bcast:nq"))
			servertype = MT_BCASTNQ;
		else if (!strcmp(com_token, "bcast"))
			servertype = MT_BCASTQW;

		else if (!strcmp(com_token, "favorite:qw"))
			servertype = -MT_SINGLEQW;
		else if (!strcmp(com_token, "favorite:q2"))
			servertype = -MT_SINGLEQ2;
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
			Master_AddMaster(line, servertype, name);
	}
	fclose(f);

	return true;
}


void NET_SendPollPacket(int len, void *data, netadr_t to)
{
	int ret;
	struct sockaddr_qstorage	addr;

	NetadrToSockadr (&to, &addr);
#ifdef USEIPX
	if (addr.sa_family == AF_IPX)
	{
		lastpollsockIPX++;
		if (lastpollsockIPX>=POLLIPXSOCKETS)
			lastpollsockIPX=0;
		if (!pollsocketsIPX[lastpollsockIPX])
			pollsocketsIPX[lastpollsockIPX] = IPX_OpenSocket(PORT_ANY, true);
		ret = sendto (pollsocketsIPX[lastpollsockIPX], data, len, 0, (struct sockaddr *)&addr, sizeof(addr) );
	}
	else
#endif
	{
		lastpollsockUDP++;
		if (lastpollsockUDP>=POLLUDPSOCKETS)
			lastpollsockUDP=0;
		if (!pollsocketsUDP[lastpollsockUDP])
			pollsocketsUDP[lastpollsockUDP] = UDP_OpenSocket(PORT_ANY, true);
		ret = sendto (pollsocketsUDP[lastpollsockUDP], data, len, 0, (struct sockaddr *)&addr, sizeof(addr) );
	}

	if (ret == -1)
	{
// wouldblock is silent
		if (qerrno == EWOULDBLOCK)
			return;

		if (qerrno == ECONNREFUSED)
			return;

		if (qerrno == EADDRNOTAVAIL)
			Con_DPrintf("NET_SendPacket Warning: %i\n", qerrno);
		else
			Con_Printf ("NET_SendPacket ERROR: %i\n", qerrno);
	}
}

int NET_CheckPollSockets(void)
{
	#define	MAX_UDP_PACKET	8192	// one more than msg + header
	extern qbyte		net_message_buffer[MAX_UDP_PACKET];
	int sock;
	SOCKET usesocket;
	for (sock = 0; sock < POLLUDPSOCKETS+POLLIPXSOCKETS; sock++)
	{
		int 	ret;
		struct sockaddr_qstorage	from;
		int		fromlen;

#ifdef USEIPX
		if (sock >= POLLUDPSOCKETS)
			usesocket = pollsocketsIPX[sock-POLLUDPSOCKETS];
		else
#endif
			usesocket = pollsocketsUDP[sock];

		if (!usesocket)
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
					NET_AdrToString (net_from));
				continue;
			}
			if (qerrno == ECONNABORTED || qerrno == ECONNRESET)
			{
//				Con_Printf ("Connection lost or aborted\n");
				continue;
			}


			Con_Printf ("NET_CheckPollSockets: %s", strerror(qerrno));
			continue;
		}
		SockadrToNetadr (&from, &net_from);

		net_message.cursize = ret;
		if (ret == sizeof(net_message_buffer) )
		{
			Con_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
			continue;
		}

		if (*(int *)net_message.data == -1)
		{
			int c;
#ifdef Q2CLIENT
			char *s;
#endif
			MSG_BeginReading ();
			MSG_ReadLong ();        // skip the -1

#ifdef Q2CLIENT
			c = msg_readcount;
			s = MSG_ReadStringLine();	//peek for q2 messages.
			if (!strcmp(s, "print"))
			{
				CL_ReadServerInfo(MSG_ReadString(), MT_SINGLEQ2, false);
				continue;
			}
			else if (!strcmp(s, "info"))	//parse a bit more...
			{
				CL_ReadServerInfo(MSG_ReadString(), MT_SINGLEQ2, false);
				continue;
			}
			else if (!strncmp(s, "servers", 6))	//parse a bit more...
			{
				msg_readcount = c+7;
				CL_MasterListParse(true);
				continue;
			}
			msg_readcount = c;
#endif

			c = MSG_ReadByte ();

			if (c == A2C_PRINT)	//qw server reply.
			{
				CL_ReadServerInfo(MSG_ReadString(), MT_SINGLEQW, false);
				continue;
			}

			if (c == M2C_MASTER_REPLY)	//qw master reply.
			{		
				CL_MasterListParse(false);
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

			CL_ReadServerInfo(va("\\hostname\\%s\\map\\%s\\maxclients\\%i", name, map, maxusers), MT_SINGLENQ, false);
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
		char data[16];
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

		newserver->refreshtime = Sys_DoubleTime();
		sprintf(data, "%c%c%c%cstatus", 255, 255, 255, 255);
		NET_SendPollPacket (strlen(data), data, newserver->adr);
	}
}


//don't try sending to servers we don't support
void MasterInfo_Request(master_t *mast)
{
	if (!mast)
		return;
	switch(mast->type)
	{
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
		MSG_WriteLong(&net_message, 0);// save space for the header, filled in later
		MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString(&net_message, NET_GAMENAME_NQ);	//look for either sort of server
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		NET_SendPollPacket(net_message.cursize, net_message.data, mast->adr);
		SZ_Clear(&net_message);
		break;
#endif
	case MT_MASTERQW:
		NET_SendPollPacket (3, "c\n", mast->adr);
		break;
#ifdef Q2CLIENT
	case MT_MASTERQ2:
		if (COM_FDepthFile("pics/colormap.pcx", true)!=0x7fffffff)	//only query this master if we expect to be able to load it's maps.
			NET_SendPollPacket (6, "query", mast->adr);
		break;
#endif
	}
}


void MasterInfo_WriteServers(void)
{
	char *typename;
	master_t *mast;
	serverinfo_t *server;
	FILE *mf, *qws;
	
	mf = fopen("masters.txt", "wt");
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
		case MT_BCASTQW:
			typename = "bcast:qw";
			break;
		case MT_BCASTQ2:
			typename = "bcast:q2";
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
		case MT_SINGLENQ:
			typename = "single:nq";
			break;
		default:
			typename = "writeerror";
		}
		fprintf(mf, "%s\t%s\t%s\n", NET_AdrToString(mast->adr), typename, mast->name);
	}
	
	if (slist_writeserverstxt.value)
		qws = fopen("server.txt", "wt");
	else
		qws = NULL;
	if (qws)
		fprintf(mf, "\n%s\t%s\t%s\n\n", "file servers.txt", "favorite:qw", "personal server list");
		
	for (server = firstserver; server; server = server->next)
	{
		if (server->special & SS_FAVORITE)
		{
			if (server->special & SS_QUAKE2)
				fprintf(mf, "%s\t%s\t%s\n", NET_AdrToString(server->adr), "favorite:q2", server->name);
			else if (server->special & SS_NETQUAKE)
				fprintf(mf, "%s\t%s\t%s\n", NET_AdrToString(server->adr), "favorite:nq", server->name);
			else if (qws)	//servers.txt doesn't support the extra info.
				fprintf(qws, "%s\t%s\n", NET_AdrToString(server->adr), server->name);
			else	//read only? damn them!
				fprintf(mf, "%s\t%s\t%s\n", NET_AdrToString(server->adr), "favorite:qw", server->name);
		}
	}
	
	if (qws)
		fclose(qws);
	
	
	fclose(mf);
}

//poll master servers for server lists.
void MasterInfo_Begin(void)
{
	master_t *mast;
	if (!Master_LoadMasterList("masters.txt",			MT_MASTERQW, 5))
	{
		Master_LoadMasterList("servers.txt",			MT_SINGLEQW, 1);
//		if (q1servers)
		{
			Master_AddMaster("255.255.255.255:26000",		MT_BCASTNQ, "Nearby Quake1 servers");
			Master_AddMaster("255.255.255.255:27500",		MT_BCASTQW, "Nearby QuakeWorld UDP servers.");
		}

//		if (q2servers)
		{
			Master_AddMaster("255.255.255.255:27910",		MT_BCASTQ2, "Nearby Quake2 UDP servers.");
			Master_AddMaster("00000000:ffffffffffff:27910",	MT_BCASTQ2, "Nearby Quake2 IPX servers.");
		}


//		if (q1servers)
		{
			Master_AddMaster("192.246.40.37:27000",			MT_MASTERQW, "id Limbo");
			Master_AddMaster("192.246.40.37:27002",			MT_MASTERQW, "id CTF");
			Master_AddMaster("192.246.40.37:27003",			MT_MASTERQW, "id TeamFortress");
			Master_AddMaster("192.246.40.37:27004",			MT_MASTERQW, "id Miscilaneous");
			Master_AddMaster("192.246.40.37:27006",			MT_MASTERQW, "id Deathmatch Only");
			Master_AddMaster("150.254.66.120:27000",		MT_MASTERQW, "Poland's master server.");
			Master_AddMaster("62.112.145.129:27000",		MT_MASTERQW, "Ocrana master server.");
		}

//		if (q2servers)
		{
			Master_AddMaster("192.246.40.37:27900",			MT_MASTERQ2, "id q2 Master.");
		}

	}

	for (mast = master; mast; mast=mast->next)
	{
		MasterInfo_Request(mast);
	}
}

void Master_QueryServer(serverinfo_t *server)
{
	char	data[2048];
	server->sends++;
	server->refreshtime = Sys_DoubleTime();
	sprintf(data, "%c%c%c%cstatus", 255, 255, 255, 255);
	NET_SendPollPacket (strlen(data), data, server->adr);
}
//send a packet to each server in sequence.
void CL_QueryServers(void)
{
	static int poll;
	int op;
	serverinfo_t *server;	
	op = poll;
	

	for (server = firstserver; op>0 && server; server=server->next, op--);

	if (!server)
	{
		poll = 0;
		return;
	}

	if (op == 0)
	{
		if (server->sends < 1)
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

	info = Master_InfoForServer(net_from);

	if (!info)	//not found...
	{
		info = Z_Malloc(sizeof(serverinfo_t));

		info->adr = net_from;

		sprintf(info->name, "%s", NET_AdrToString(info->adr));

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
	Q_strncpyz(info->name, name, sizeof(info->name));
	info->special = info->special & (SS_FAVORITE | SS_KEEPINFO);	//favorite is never cleared
	if (!strcmp("FTE", Info_ValueForKey(msg, "*distrib")))
		info->special |= SS_FTESERVER;
	else if (!strncmp("FTE", Info_ValueForKey(msg, "*version"), 3))
		info->special |= SS_FTESERVER;


	if (servertype == MT_SINGLEQ2)
		info->special |= SS_QUAKE2;
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

	info->tl = atoi(Info_ValueForKey(msg, "timelimit"));
	info->fl = atoi(Info_ValueForKey(msg, "fraglimit"));

	if (servertype == MT_SINGLEQ2)
	{
		Q_strncpyz(info->gamedir,	Info_ValueForKey(msg, "gamename"),	sizeof(info->gamedir));
		Q_strncpyz(info->map,		Info_ValueForKey(msg, "mapname"),	sizeof(info->map));
	}
	else
	{
		Q_strncpyz(info->gamedir,	Info_ValueForKey(msg, "*gamedir"),	sizeof(info->gamedir));
		Q_strncpyz(info->map,		Info_ValueForKey(msg, "map"),		sizeof(info->map));
	}

	{
		int clnum;
		strcpy(details.info, msg);
		msg = msg+strlen(msg)+1;

		info->players=details.numplayers = 0;
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
void CL_MasterListParse(qboolean isq2)
{
	serverinfo_t *info;
	serverinfo_t *last, *old;

	int p1, p2;
	MSG_ReadByte ();

	last = firstserver;

	while(msg_readcount+1 < net_message.cursize)
	{
		info = Z_Malloc(sizeof(serverinfo_t));
		info->adr.type = NA_IP;
		info->adr.ip[0] = MSG_ReadByte();
		info->adr.ip[1] = MSG_ReadByte();
		info->adr.ip[2] = MSG_ReadByte();
		info->adr.ip[3] = MSG_ReadByte();

		p1 = MSG_ReadByte();
		p2 = MSG_ReadByte();
		info->adr.port = (int)((short)(p1 + (p2<<8)));
		if ((old = Master_InfoForServer(info->adr)))	//remove if the server already exists.
		{
			old->sends = 0;	//reset.
			Z_Free(info);
		}
		else
		{
			info->special = isq2?SS_QUAKE2:0;
			info->refreshtime = 0;

			sprintf(info->name, "%s", NET_AdrToString(info->adr));

			info->next = last;
			last = info;
		}		
	}

	firstserver = last;
}

#endif
