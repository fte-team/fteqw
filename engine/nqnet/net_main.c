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
// net_main.c

#define NOCOM

#include "../client/quakedef.h"
#ifdef NQPROT


#define Sys_FloatTime Sys_DoubleTime

qsocket_t	*net_activeSockets = NULL;
qsocket_t	*net_freeSockets = NULL;
int			net_numsockets = 0;

#ifndef NOCOM
qboolean	serialAvailable = false;
#endif
qboolean	ipxAvailable = false;
qboolean	tcpipAvailable = false;

int			net_hostport;
int			DEFAULTnet_hostport = 26000;

char		my_ipx_address[NET_NAMELEN];
char		my_tcpip_address[NET_NAMELEN];

#ifndef NOCOM
void (*GetComPortConfig) (int portNumber, int *port, int *irq, int *baud, qboolean *useModem);
void (*SetComPortConfig) (int portNumber, int port, int irq, int baud, qboolean useModem);
void (*GetModemConfig) (int portNumber, char *dialType, char *clear, char *init, char *hangup);
void (*SetModemConfig) (int portNumber, char *dialType, char *clear, char *init, char *hangup);
#endif

static qboolean	listening = false;

qboolean	slistInProgress = false;
qboolean	slistSilent = false;
qboolean	slistLocal = true;
static double	slistStartTime;
static int		slistLastShown;

static void Slist_Send(void *arg);
static void Slist_Poll(void *arg);
PollProcedure	slistSendProcedure = {NULL, 0.0, Slist_Send};
PollProcedure	slistPollProcedure = {NULL, 0.0, Slist_Poll};


extern sizebuf_t		net_message;
int				net_activeconnections = 0;

int messagesSent = 0;
int messagesReceived = 0;
int unreliableMessagesSent = 0;
int unreliableMessagesReceived = 0;

cvar_t	net_messagetimeout = {"net_messagetimeout","300"};

#ifndef NOCOM
qboolean	configRestored = false;
cvar_t	config_com_port = {"_config_com_port", "0x3f8", CVAR_ARCHIVE};
cvar_t	config_com_irq = {"_config_com_irq", "4", CVAR_ARCHIVE};
cvar_t	config_com_baud = {"_config_com_baud", "57600", CVAR_ARCHIVE};
cvar_t	config_com_modem = {"_config_com_modem", "1", CVAR_ARCHIVE};
cvar_t	config_modem_dialtype = {"_config_modem_dialtype", "T", CVAR_ARCHIVE};
cvar_t	config_modem_clear = {"_config_modem_clear", "ATZ", CVAR_ARCHIVE};
cvar_t	config_modem_init = {"_config_modem_init", "", CVAR_ARCHIVE};
cvar_t	config_modem_hangup = {"_config_modem_hangup", "AT H", CVAR_ARCHIVE};
#endif

#ifdef IDGODS
cvar_t	idgods = {"idgods", "0"};
#endif

// these two macros are to make the code more readable
#define sfunc	net_drivers[sock->driver]
#define dfunc	net_drivers[net_driverlevel]

int	net_driverlevel;


double			net_time;

double SetNetTime(void)
{
	net_time = Sys_FloatTime();
	return net_time;
}

#if defined(_WIN32) || defined(SERVERONLY)
int Sys_FileWrite (int handle, void *data, int count)
{
	return 0;
}
int Sys_FileRead (int handle, void *data, int count)
{
	return 0;
}
void Sys_FileClose (int handle)
{
	return;
}
#endif
/*
===================
NET_NewQSocket

Called by drivers when a new communications endpoint is required
The sequence and buffer fields will be filled in properly
===================
*/
qsocket_t *NET_NewQSocket (void)
{
	qsocket_t	*sock;

	if (net_freeSockets == NULL)
		return NULL;

	if (net_activeconnections >= 32)
		return NULL;

	// get one from free list
	sock = net_freeSockets;
	net_freeSockets = sock->next;

	// add it to active list
	sock->next = net_activeSockets;
	net_activeSockets = sock;

	sock->disconnected = false;
	sock->connecttime = net_time;
	Q_strcpy (sock->address,"UNSET ADDRESS");
	sock->driver = net_driverlevel;
	sock->socket = 0;
	sock->driverdata = NULL;
	sock->qwprotocol = false;
	sock->canSend = true;
	sock->sendNext = false;
	sock->lastMessageTime = net_time;
	sock->ackSequence = 0;
	sock->sendSequence = 0;
	sock->unreliableSendSequence = 0;
	sock->sendMessageLength = 0;
	sock->receiveSequence = 0;
	sock->unreliableReceiveSequence = 0;
	sock->receiveMessageLength = 0;

	return sock;
}


void NET_FreeQSocket(qsocket_t *sock)
{
	qsocket_t	*s;

	// remove it from active list
	if (sock == net_activeSockets)
		net_activeSockets = net_activeSockets->next;
	else
	{
		for (s = net_activeSockets; s; s = s->next)
			if (s->next == sock)
			{
				s->next = sock->next;
				break;
			}
		if (!s)
			Sys_Error ("NET_FreeQSocket: not active\n");
	}

	// add it to free list
	sock->next = net_freeSockets;
	net_freeSockets = sock;
	sock->disconnected = true;
}


static void NET_Listen_f (void)
{
	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"listen\" is \"%u\"\n", listening ? 1 : 0);
		return;
	}

	listening = Q_atoi(Cmd_Argv(1)) ? true : false;

	for (net_driverlevel=0 ; net_driverlevel<net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.Listen (listening);
	}
}


static void NET_Port_f (void)
{
	int 	n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"port\" is \"%u\"\n", net_hostport);
		return;
	}

	n = Q_atoi(Cmd_Argv(1));
	if (n < 1 || n > 65534)
	{
		Con_Printf ("Bad value, must be between 1 and 65534\n");
		return;
	}

	DEFAULTnet_hostport = n;
	net_hostport = n;

	if (listening)
	{
		// force a change to the new port
		Cbuf_AddText ("listen 0\n", RESTRICT_LOCAL);
		Cbuf_AddText ("listen 1\n", RESTRICT_LOCAL);
	}
}


static void PrintSlistHeader(void)
{
	Con_Printf("Server          Map             Users\n");
	Con_Printf("--------------- --------------- -----\n");
	slistLastShown = 0;
}


static void PrintSlist(void)
{
	int n;

	for (n = slistLastShown; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			Con_Printf("%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			Con_Printf("%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
	}
	slistLastShown = n;
}


static void PrintSlistTrailer(void)
{
	if (hostCacheCount)
		Con_Printf("== end list ==\n\n");
	else
		Con_Printf("No Quake servers found.\n\n");
}


void NET_Slist_f (void)
{
	if (slistInProgress)
		return;

	if (! slistSilent)
	{
		Con_Printf("Looking for Quake servers...\n");
		PrintSlistHeader();
	}

	slistInProgress = true;
	slistStartTime = Sys_FloatTime();

	SchedulePollProcedure(&slistSendProcedure, 0.0);
	SchedulePollProcedure(&slistPollProcedure, 0.1);

	hostCacheCount = 0;
}


static void Slist_Send(void *arg)
{
	for (net_driverlevel=0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.SearchForHosts (true);
	}

	if ((Sys_FloatTime() - slistStartTime) < 0.5)
		SchedulePollProcedure(&slistSendProcedure, 0.75);
}


static void Slist_Poll(void *arg)
{
	for (net_driverlevel=0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.SearchForHosts (false);
	}

	if (! slistSilent)
		PrintSlist();

	if ((Sys_FloatTime() - slistStartTime) < 1.5)
	{
		SchedulePollProcedure(&slistPollProcedure, 0.1);
		return;
	}

	if (! slistSilent)
		PrintSlistTrailer();
	slistInProgress = false;
	slistSilent = false;
	slistLocal = true;
}


/*
===================
NET_Connect
===================
*/

int hostCacheCount = 0;
hostcache_t hostcache[HOSTCACHESIZE];

qsocket_t *NET_Connect (char *host, qboolean continuation)
{
	qsocket_t		*ret;
	int				n;
	int				numdrivers = net_numdrivers;

	SetNetTime();

	if (host && *host == 0)
		host = NULL;

	if (host)
	{
		if (Q_strcasecmp (host, "local") == 0)
		{
			numdrivers = 1;
			goto JustDoIt;
		}

		if (continuation != 1)
		{
			slistSilent = host ? true : false;
			NET_Slist_f ();

			while(slistInProgress)
				NET_Poll();
		}

		if (hostCacheCount)
		{
			for (n = 0; n < hostCacheCount; n++)
				if (Q_strcasecmp (host, hostcache[n].name) == 0)
				{
					host = hostcache[n].cname;
					break;
				}
			if (n < hostCacheCount)
				goto JustDoIt;
		}
	}

	if (host == NULL)
	{
		if (hostCacheCount != 1)
			return NULL;
		host = hostcache[0].cname;
		Con_Printf("Connecting to...\n%s @ %s\n\n", hostcache[0].name, host);
	}

	if (hostCacheCount)
		for (n = 0; n < hostCacheCount; n++)
			if (Q_strcasecmp (host, hostcache[n].name) == 0)
			{
				host = hostcache[n].cname;
				break;
			}

JustDoIt:
	for (net_driverlevel=0 ; net_driverlevel<numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		if (continuation == 2)
			ret = dfunc.Connect (host);
		else if (continuation == 1)
		{
			if (dfunc.ContinueConnect)
				ret = dfunc.ContinueConnect (host);
			else
				ret = dfunc.Connect (host);
		}
		else
		{
			if (dfunc.BeginConnect)
				ret = dfunc.BeginConnect (host);
			else
				ret = dfunc.Connect (host);
		}
		if (ret)
			return ret;
	}

	if (continuation != 1 && host)
	{
		Con_Printf("\n");
		PrintSlistHeader();
		PrintSlist();
		PrintSlistTrailer();
	}
	
	return NULL;
}


/*
===================
NET_CheckNewConnections
===================
*/

qsocket_t *NET_CheckNewConnections (void)
{
#ifndef CLIENTONLY
	qsocket_t	*ret;

	SetNetTime();

	for (net_driverlevel=0 ; net_driverlevel<net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		if (net_driverlevel && listening == false)
			continue;
		ret = dfunc.CheckNewConnections ();
		if (ret)
		{
			return ret;
		}
	}
#endif
	return NULL;
}

/*
===================
NET_Close
===================
*/
void NET_Close (qsocket_t *sock)
{
	if (!sock)
		return;

	if (sock->disconnected==true)
		return;

	SetNetTime();

	// call the driver_Close function
	sfunc.Close (sock);

	NET_FreeQSocket(sock);
}


/*
=================
NET_GetMessage

If there is a complete message, return it in net_message

returns 0 if no data is waiting
returns 1 if a message was received
returns -1 if connection is invalid
=================
*/

extern void PrintStats(qsocket_t *s);

int	NET_GetMessage (qsocket_t *sock)
{
	int ret;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_GetMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();

	ret = sfunc.QGetMessage(sock);

	// see if this connection has timed out
	if (ret == 0 && sock->driver)
	{
		if (net_time - sock->lastMessageTime > net_messagetimeout.value)
		{
			NET_Close(sock);
			return -1;
		}
	}


	if (ret > 0)
	{
		if (sock->driver)
		{
			sock->lastMessageTime = net_time;
			if (ret == 1)
				messagesReceived++;
			else if (ret == 2)
				unreliableMessagesReceived++;
		}
	}

	*(int *)net_from.ip = 0;
	net_from.port = 0;
	return ret;
}


/*
==================
NET_SendMessage

Try to send a complete length+message unit over the reliable stream.
returns 0 if the message cannot be delivered reliably, but the connection
		is still considered valid
returns 1 if the message was sent properly
returns -1 if the connection died
==================
*/

int NET_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	int		r;
	
	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();
	r = sfunc.QSendMessage(sock, data);
	if (r == 1 && sock->driver)
		messagesSent++;
	
	return r;
}


int NET_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int		r;
	
	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();
	r = sfunc.SendUnreliableMessage(sock, data);
	if (r == 1 && sock->driver)
		unreliableMessagesSent++;
	
	return r;
}


/*
==================
NET_CanSendMessage

Returns true or false if the given qsocket can currently accept a
message to be transmitted.
==================
*/
qboolean NET_CanSendMessage (qsocket_t *sock)
{
	int		r;
	
	if (!sock)
		return false;

	if (sock->disconnected)
		return false;

	SetNetTime();

	r = sfunc.CanSendMessage(sock);
	
	return r;
}


int NET_SendToAll(sizebuf_t *data, int blocktime)
{
#ifndef CLIENTONLY
	double		start;
	int			i;
	int			count = 0;
	qboolean	state1 [MAX_SCOREBOARD];
	qboolean	state2 [MAX_SCOREBOARD];

	for (i=0, host_client = svs.clients ; i<32 ; i++, host_client++)
	{
		if (!host_client->netchan.qsocket)
			continue;
		if (host_client->state > cs_zombie)
		{
			if (host_client->netchan.qsocket->driver == 0)
			{
				NET_SendMessage(host_client->netchan.qsocket, data);
				state1[i] = true;
				state2[i] = true;
				continue;
			}
			count++;
			state1[i] = false;
			state2[i] = false;
		}
		else
		{
			state1[i] = true;
			state2[i] = true;
		}
	}

	start = Sys_FloatTime();
	while (count)
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<32 ; i++, host_client++)
		{
			if (! state1[i])
			{
				if (NET_CanSendMessage (host_client->netchan.qsocket))
				{
					state1[i] = true;
					NET_SendMessage(host_client->netchan.qsocket, data);
				}
				else
				{
					NET_GetMessage (host_client->netchan.qsocket);
				}
				count++;
				continue;
			}

			if (! state2[i])
			{
				if (NET_CanSendMessage (host_client->netchan.qsocket))
				{
					state2[i] = true;
				}
				else
				{
					NET_GetMessage (host_client->netchan.qsocket);
				}
				count++;
				continue;
			}
		}
		if ((Sys_FloatTime() - start) > blocktime)
			break;
	}
	return count;
#else
	return 0;
#endif
}


//=============================================================================

/*
====================
NET_Init
====================
*/

void NQ_NET_Init (void)
{
	int			i;
	int			controlSocket;
	qsocket_t	*s;

	i = COM_CheckParm ("-nqport");
	if (!i)
		i = COM_CheckParm ("-udpport");
	if (!i)
		i = COM_CheckParm ("-ipxport");

	if (i)
	{
		if (i < com_argc-1)
			DEFAULTnet_hostport = Q_atoi (com_argv[i+1]);
		else
			Sys_Error ("NET_Init: you must specify a number after -port");
	}
	net_hostport = DEFAULTnet_hostport;

#ifndef CLIENTONLY
#ifndef SERVERONLY
	if (COM_CheckParm("-listen") || isDedicated)
#endif
		listening = true;

	net_numsockets = 32;
#ifndef SERVERONLY
	if (!isDedicated)
		net_numsockets++;
#endif

#else
	net_numsockets = 1;
#endif

	SetNetTime();

	for (i = 0; i < net_numsockets; i++)
	{
		s = (qsocket_t *)Hunk_AllocName(sizeof(qsocket_t), "qsocket");
		s->next = net_freeSockets;
		net_freeSockets = s;
		s->disconnected = true;
	}

	// allocate space for network message buffer	
	net_message.data = Z_Malloc(NET_MAXMESSAGE);
	net_message.maxsize = NET_MAXMESSAGE;

	Cvar_Register (&net_messagetimeout, "Networking");
//	Cvar_RegisterVariable (&hostname);
#ifndef NOCOM
	Cvar_RegisterVariable (&config_com_port);
	Cvar_RegisterVariable (&config_com_irq);
	Cvar_RegisterVariable (&config_com_baud);
	Cvar_RegisterVariable (&config_com_modem);
	Cvar_RegisterVariable (&config_modem_dialtype);
	Cvar_RegisterVariable (&config_modem_clear);
	Cvar_RegisterVariable (&config_modem_init);
	Cvar_RegisterVariable (&config_modem_hangup);
#endif
#ifdef IDGODS
	Cvar_RegisterVariable (&idgods);
#endif

	Cmd_AddCommand ("slist", NET_Slist_f);
	Cmd_AddCommand ("listen", NET_Listen_f);
	Cmd_AddCommand ("port", NET_Port_f);

	// initialize all the drivers
	for (net_driverlevel=0 ; net_driverlevel<net_numdrivers ; net_driverlevel++)
		{
		controlSocket = net_drivers[net_driverlevel].Init();
		if (controlSocket == -1)
			continue;
		net_drivers[net_driverlevel].initialized = true;
		net_drivers[net_driverlevel].controlSock = controlSocket;
		if (listening)
			net_drivers[net_driverlevel].Listen (true);
		}

	if (*my_ipx_address)
		Con_Printf("NQ IPX address %s\n", my_ipx_address);
	if (*my_tcpip_address)
		Con_Printf("NQ TCP/IP address %s\n", my_tcpip_address);
}

/*
====================
NET_Shutdown
====================
*/

void		NQ_NET_Shutdown (void)
{
	qsocket_t	*sock;

	SetNetTime();

	for (sock = net_activeSockets; sock; sock = sock->next)
		NET_Close(sock);

//
// shutdown the drivers
//
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == true)
		{
			net_drivers[net_driverlevel].Shutdown ();
			net_drivers[net_driverlevel].initialized = false;
		}
	}
}


static PollProcedure *pollProcedureList = NULL;

void NET_Poll(void)
{
	PollProcedure *pp;
#ifndef NOCOM
	qboolean	useModem;

	if (!configRestored)
	{
		if (serialAvailable)
		{
			if (config_com_modem.value == 1.0)
				useModem = true;
			else
				useModem = false;
			SetComPortConfig (0, (int)config_com_port.value, (int)config_com_irq.value, (int)config_com_baud.value, useModem);
			SetModemConfig (0, config_modem_dialtype.string, config_modem_clear.string, config_modem_init.string, config_modem_hangup.string);
		}
		configRestored = true;
	}
#endif

	SetNetTime();

	for (pp = pollProcedureList; pp; pp = pp->next)
	{
		if (pp->nextTime > net_time)
			break;
		pollProcedureList = pp->next;
		pp->procedure(pp->arg);
	}
}


void SchedulePollProcedure(PollProcedure *proc, double timeOffset)
{
	PollProcedure *pp, *prev;

	proc->nextTime = Sys_FloatTime() + timeOffset;
	for (pp = pollProcedureList, prev = NULL; pp; pp = pp->next)
	{
		if (pp->nextTime >= proc->nextTime)
			break;
		prev = pp;
	}

	if (prev == NULL)
	{
		proc->next = pollProcedureList;
		pollProcedureList = proc;
		return;
	}

	proc->next = pp;
	prev->next = proc;
}


#ifdef IDGODS
#define IDNET	0xc0f62800

qboolean IsID(struct qsockaddr *addr)
{
	if (idgods.value == 0.0)
		return false;

	if (addr->sa_family != 2)
		return false;

	if ((BigLong(*(int *)&addr->sa_data[2]) & 0xffffff00) == IDNET)
		return true;
	return false;
}
#endif


#ifndef SERVERONLY
void NQ_BeginConnect(char *to)
{
netadr_t addr;

	if (cls.netcon)
	{
		if (cls.netcon->qwprotocol)
		{
			sizebuf_t msg;
			char data[64];
			*(int*)data = -1;
			strcpy(data+4, "getchallenge");
			msg.cursize = strlen(data);
			msg.data = data;
			NET_SendMessage(cls.netcon, &msg);
		}
		return;
	}

	cls.netcon = NET_Connect(to, 0);
	if (!cls.netcon)
	{
		Con_Printf ("CL_Connect: connect failed\n");
		return;
	}
	if (cls.netcon->qwprotocol)
	{
		extern double connect_time;
		cls.netchan.qsocket = cls.netcon;
		connect_time=-1;		//a get chalenge is emulated by server.
	}
	else
	{
		SockadrToNetadr(&cls.netcon->addr, &addr);
		Netchan_Setup(cls.netcon->socket, &cls.netchan, addr, cls.qport);
		cls.netchan.qsocket = cls.netcon;
		Con_DPrintf ("CL_EstablishConnection: connected to %s\n", cls.servername);
		
		cls.netchan.qsocket = cls.netcon;
		
		cls.demonum = -1;			// not in the demo loop now
		cls.state = ca_connected;
	}

//	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
//	MSG_WriteString(&cls.netchan.message, "new");
}
void NQ_ContinueConnect(char *to)
{
netadr_t addr;
	if (cls.netcon)
		return;	//already got through

	cls.netcon = NET_Connect(to, 1);
	if (!cls.netcon)
	{		
		return;
	}
	if (cls.netcon->qwprotocol)
	{
		extern double connect_time;
		cls.netchan.qsocket = cls.netcon;
		connect_time=-1;		//a get chalenge is emulated by server.
	}
	else
	{
		SockadrToNetadr(&cls.netcon->addr, &addr);
		Netchan_Setup(cls.netcon->socket, &cls.netchan, addr, cls.qport);
		cls.netchan.qsocket = cls.netcon;
		Con_DPrintf ("CL_EstablishConnection: connected to %s\n", cls.servername);
		
		cls.netchan.qsocket = cls.netcon;
		
		cls.demonum = -1;			// not in the demo loop now
		cls.state = ca_connected;
	}
//	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
//	MSG_WriteString(&cls.netchan.message, "new");
}
void NQ_AbortConnect(char *to)
{
}
#endif


#endif
