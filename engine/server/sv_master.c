#include "quakedef.h"

#ifdef SV_MASTER

#ifdef _WIN32
#include "winquake.h"
#ifdef _MSC_VER
#include "wsipx.h"
#endif
#endif

#include "netinc.h"

//quake1 protocol
//
//

//quake2 protocol
//client sends "query\0"
//server sends "0xff0xff0xff0xffservers" follwed by lots of ips.

#define SVM_Q1HEARTBEATTIME 330
#define SVM_Q2HEARTBEATTIME 330

typedef struct svm_server_s {
	netadr_t adr;
	int clients;
	float expiretime;
	struct svm_server_s *next;
} svm_server_t;

typedef struct {
	int socketudp;
	float time;
	int port;

	svm_server_t *firstserver;
	int numservers;
} masterserver_t;

masterserver_t svm = {INVALID_SOCKET};

void SVM_RemoveOldServers(void)
{
	svm_server_t *server, *next, *prev=NULL;
	for (server = svm.firstserver; server; server = next)
	{
		next = server->next;
		if (server->expiretime < svm.time)
		{
			BZ_Free(server);
			if (prev)
				prev->next = next;
			else
				svm.firstserver = next;			
		}
		else
			prev = server;
	}
}

int SVM_AddIPAddresses(sizebuf_t *sb, int first)
{
	int number = 0;
	svm_server_t *server;

	for (server = svm.firstserver; server; server = server->next)
	{
		if (number == first)
			break;

		first--;
	}

	for (; server; server = server->next)
	{
		if (sb->cursize + 6 >= sb->maxsize)
			break;

		MSG_WriteByte(sb, server->adr.address.ip[0]);
		MSG_WriteByte(sb, server->adr.address.ip[1]);
		MSG_WriteByte(sb, server->adr.address.ip[2]);
		MSG_WriteByte(sb, server->adr.address.ip[3]);
		MSG_WriteShort(sb, server->adr.port);

		number++;
	}

	return number;
}

void SVM_Heartbeat(netadr_t *adr, int numclients, float validuntil)
{
	svm_server_t *server;

	for (server = svm.firstserver; server; server = server->next)
	{
		if (NET_CompareAdr(&server->adr, adr))
			break;
	}
	if (!server)	//not found
	{
		server = Z_Malloc(sizeof(svm_server_t));
		server->next = svm.firstserver;
		svm.firstserver = server;

		server->adr = *adr;
	}

	server->clients = numclients;
	server->expiretime = validuntil;
}


void SVM_Init(int port)
{
	if (svm.socketudp == INVALID_SOCKET)
		svm.socketudp = UDP_OpenSocket(port, false);
}

void SVM_ShutDown (void)
{
	if (svm.socketudp != INVALID_SOCKET)
	{
		UDP_CloseSocket(svm.socketudp);
		svm.socketudp = INVALID_SOCKET;
	}
}

void SVM_Think(int port)
{
	char *s;
	struct sockaddr_qstorage addr;
	int addrlen;
	netadr_t netaddr;
	if (!port)
	{
		SVM_ShutDown();
		return;
	}

	if (port != svm.port)
	{
		SVM_ShutDown();	//shut down (to cause a restart)
		svm.port = port;
	}

	SVM_Init(port);

	addrlen = sizeof(addr);
	net_message.cursize = recvfrom(svm.socketudp, net_message_buffer, sizeof(net_message_buffer)-1, 0, (struct sockaddr *)&addr, &addrlen);
	if (net_message.cursize <= 0)
	{
		addrlen = qerrno;


		return;
	}
	net_message.data[net_message.cursize] = '\0';	//null term all strings.
	SockadrToNetadr(&addr, &netaddr);
	svm.time = Sys_DoubleTime();

	SVM_RemoveOldServers();
	
	MSG_BeginReading(msg_nullnetprim);
	if (MSG_ReadLong() != -1 || msg_badread)
	{	//go back to start...
		MSG_BeginReading(msg_nullnetprim);
	}
	s = MSG_ReadStringLine();
	s = COM_Parse(s);
	if (!strcmp(com_token, "getservers"))
	{
		sizebuf_t sb;
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer)-2;
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		MSG_WriteString(&sb, "getserversResponse\\");
		sb.cursize--;
		SVM_AddIPAddresses(&sb, 0);
		sb.maxsize+=2;
		MSG_WriteShort(&sb, 0);
		sendto(svm.socketudp, sb.data, sb.cursize, 0, (struct sockaddr *)&addr, sizeof(addr));
	}
	else if (!strcmp(com_token, "heartbeat"))
	{	//quake2 heartbeat. Serverinfo and players follow.
		SVM_Heartbeat(&netaddr, 0, svm.time + SVM_Q2HEARTBEATTIME);
	}
	else if (!strcmp(com_token, "query"))
	{	//quake2 server listing request
		sizebuf_t sb;
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer);
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		MSG_WriteString(&sb, "servers\n");
		sb.cursize--;
//		MSG_WriteLong(&sb, 0);
//		MSG_WriteShort(&sb, 0);
		SVM_AddIPAddresses(&sb, 0);
		sendto(svm.socketudp, sb.data, sb.cursize, 0, (struct sockaddr *)&addr, sizeof(addr));
	}
	else if (*com_token == S2M_HEARTBEAT)	//sequence, players
	{	//quakeworld heartbeat
		SVM_Heartbeat(&netaddr, 0, svm.time + SVM_Q1HEARTBEATTIME);
	}
	else if (*com_token == S2C_CHALLENGE)
	{	//quakeworld server listing request
		sizebuf_t sb;
		memset(&sb, 0, sizeof(sb));
		sb.maxsize = sizeof(net_message_buffer);
		sb.data = net_message_buffer;
		MSG_WriteLong(&sb, -1);
		MSG_WriteByte(&sb, M2C_MASTER_REPLY);
		MSG_WriteByte(&sb, '\n');
		SVM_AddIPAddresses(&sb, 0);
		sendto(svm.socketudp, sb.data, sb.cursize, 0, (struct sockaddr *)&addr, sizeof(addr));
	}
}
#else
void SVM_Think(int port){}
#endif
