#include "bothdefs.h"

#ifdef EMAILSERVER

#include "../http/iweb.h"



#ifdef _WIN32
#define EWOULDBLOCK	WSAEWOULDBLOCK
#define EMSGSIZE	WSAEMSGSIZE
#define ECONNRESET	WSAECONNRESET
#define ECONNABORTED	WSAECONNABORTED
#define ECONNREFUSED	WSAECONNREFUSED
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL

#define qerrno WSAGetLastError()
#else
#define qerrno errno

#define MSG_PARTIAL 0
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <errno.h>

#include <unistd.h>

#define closesocket close
#define ioctlsocket ioctl
#endif


#define SMTP_PORT	25
#define SMTP_TIMEOUT	30

static qboolean smtpactive;
static int smtpserversocket;

typedef struct svsmtpclient_s {
	struct svsmtpclient_s *next;

	int socket;

	float timeout;

	char fromaddr[64];
	char toaddr[64];

	char inmessagebuffer[1024];
	int inmessagelen;
	char outmessagebuffer[1024];
	int outmessagelen;

	qboolean gettingdata;

	FILE *file;
	char sendingtotemp[256];
} svsmtpclient_t;
static svsmtpclient_t *svsmtpclient;





static void SMTP_ServerInit(void)
{
	struct sockaddr_in address;
	unsigned long _true = true;
	int port = SMTP_PORT;

	if ((smtpserversocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		Sys_Error ("FTP_TCP_OpenSocket: socket:", strerror(qerrno));
	}

	if (ioctlsocket (smtpserversocket, FIONBIO, &_true) == -1)
	{
		Sys_Error ("FTP_TCP_OpenSocket: ioctl FIONBIO:", strerror(qerrno));
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);
	
	if( bind (smtpserversocket, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(smtpserversocket);
		return;
	}
	
	listen(smtpserversocket, 3);

	smtpactive = true;


	IWebPrintf("SMTP server is running\n");
	return;
}

static void SMTP_ServerShutdown(void)
{
	closesocket(smtpserversocket);
	smtpactive = false;
}

static void SV_SMTP_QueueMessage(svsmtpclient_t *cl, char *msg)
{
	int len = strlen(msg);
	if (len + cl->outmessagelen > sizeof(cl->outmessagebuffer)-1)
		len = sizeof(cl->outmessagebuffer)-1 - cl->outmessagelen;
	Q_strncpyz(cl->outmessagebuffer+cl->outmessagelen, msg, len+1);
	cl->outmessagelen += len;
}

static void SMTP_NewConnection(int socket)
{
	svsmtpclient_t *newcl;
	newcl = IWebMalloc(sizeof(svsmtpclient_t));
	if (!newcl)	//bother
	{
		closesocket(socket);
		return;
	}
	memset(newcl, 0, sizeof(svsmtpclient_t));

	newcl->socket = socket;
	newcl->next = svsmtpclient;
	svsmtpclient = newcl;

	newcl->timeout = realtime + SMTP_TIMEOUT;

	*newcl->outmessagebuffer = '\0';

	SV_SMTP_QueueMessage(newcl, "220 81.157.120.30 Probably best to say HELO now.\r\n");
}

static qboolean SV_SMTP_RunClient(svsmtpclient_t *cl)	//true means client should be dropped
{
	int read;
	char *nl, *token;

	if (cl->outmessagelen)
	{
		read = send(cl->socket, cl->outmessagebuffer, cl->outmessagelen, 0);
		if (read < 0)
			read = 0;
		memmove(cl->outmessagebuffer, cl->outmessagebuffer + read, cl->outmessagelen - read+1);
		cl->outmessagelen -= read;
	}

	read = recv(cl->socket, cl->inmessagebuffer+cl->inmessagelen, sizeof(cl->inmessagebuffer)-1-cl->inmessagelen, 0);

	if (read == -1)
	{

		if (qerrno != EWOULDBLOCK)	//blocking is the only way to keep the connection on fail.
			return true;
		if (!cl->inmessagelen)
			return false;
	}
	else if (read == 0)	//don't quite know why this happens.
	{
		if (!cl->inmessagelen)
			return false;
	}
	else
	{
		cl->timeout = realtime + SMTP_TIMEOUT;

		cl->inmessagelen += read;
		if (cl->inmessagelen >= sizeof(cl->inmessagebuffer)-1 && !cl->gettingdata)	//happens if we fill the buffer with no hope of empting it.
			return true;
		cl->inmessagebuffer[cl->inmessagelen] = '\0';
	}

	while (cl->gettingdata)
	{
		nl = strstr(cl->inmessagebuffer, "\r\n.");
		if (!nl)
			nl = cl->inmessagebuffer + cl->inmessagelen - 10;
		else
			nl+=2;
		if (nl < cl->inmessagebuffer)
			nl = cl->inmessagebuffer;	//waiting for a crnl, so we can'texast this buffer in case we chop the \r\n

		if (!strcmp(cl->inmessagebuffer, ".\r\n"))
		{
			FILE *f;
			int tries;
			int len;
			char name[256];
			char buffer[1024];
			char *to;
			char *at;

			cl->gettingdata = false;


			to = strchr(cl->toaddr, '<');
			at = strchr(cl->toaddr, '@');
			if (!to || !at || strstr(to, ".."))
				SV_SMTP_QueueMessage(cl, "452 Couldn't open file.\r\n");
			else
			{
				to++;
				*at = '\0';
				f=NULL;
				for (tries = 0; tries < 10; tries++)	//give it a few goes.
				{
					sprintf(name, "emails/%s/e%i.eml", to, rand());
					COM_CreatePath(name);
					f = fopen(name, "wb");
					if (f)
						break;
				}

				if (f)
				{

					{
						netadr_t adr;
						struct sockaddr_qstorage name;
						int namelen = sizeof(name);

						getpeername(cl->socket, (struct sockaddr*)&name, &namelen);
						SockadrToNetadr(&name, &adr);
						sprintf(buffer, "Received: from %s\r\n", NET_AdrToString(adr));
						fwrite(buffer, strlen(buffer), 1, f);
					}

					fseek(cl->file, 0, SEEK_END);
					len = ftell(cl->file);
					fseek(cl->file, 0, SEEK_SET);
					while(len)
					{
						tries = sizeof(buffer);
						if (tries > len)
							tries = len;
						len -= tries;

						fread(buffer, tries, 1, cl->file);
						if (fwrite(buffer, tries, 1, f) != 1)
						{
							fclose(f);
							f = NULL;
							unlink(name);
							len = 0;
							SV_SMTP_QueueMessage(cl, "452 Insufficient system storage.\r\n");
						}
					}
					if (f)
					{
						fclose(f);
						SV_SMTP_QueueMessage(cl, "250 Finally. You do go on a bit.\r\n");
					}
				}
				else
					SV_SMTP_QueueMessage(cl, "452 Couldn't open file.\r\n");
			}

			fclose(cl->file);
		}
		else
		{
//			for (read = 0; read < nl - cl->inmessagebuffer; read++)
//				putch(cl->inmessagebuffer[read]);
			fwrite(cl->inmessagebuffer, nl - cl->inmessagebuffer, 1, cl->file);
		}

		cl->inmessagelen -= nl - cl->inmessagebuffer;
		memmove(cl->inmessagebuffer, nl, strlen(nl)+1);
		return false;
	}

	nl = strstr(cl->inmessagebuffer, "\r\n");
	if (nl)
	{
		*nl = '\0';
		nl+=2;

		{

			printf("%s\n", cl->inmessagebuffer);

 			token = COM_Parse(cl->inmessagebuffer);

			if (!strcmp(com_token, "QUIT"))
			{
				SV_SMTP_QueueMessage(cl, "221 Well, it was good while it lasted\r\n");
				return true;
			}
			else if (!strcmp(com_token, "HELO"))
			{
				SV_SMTP_QueueMessage(cl, va("250 %s Electronic you say? That's good then. :o)\r\n", "81.107.21.148"));
			}
			else if (!strcmp(com_token, "MAIL"))
			{
				token = COM_Parse(token);
				if (strcmp(com_token, "FROM:"))
				{
					if (strncmp(com_token, "FROM:", 5))
						SV_SMTP_QueueMessage(cl, "501 Syntax error. Expected MAIL FROM: fromaddr\r\n");
					else
					{
						SV_SMTP_QueueMessage(cl, "250 Get on with it\r\n");
						Q_strncpyz(cl->fromaddr, com_token+5, sizeof(cl->fromaddr));
					}
				}
				else
				{
					SV_SMTP_QueueMessage(cl, "250 Get on with it\r\n");
					Q_strncpyz(cl->fromaddr, token, sizeof(cl->fromaddr));
				}
			}
			else if (!strcmp(com_token, "RCPT"))
			{
				token = COM_Parse(token);
				if (strcmp(com_token, "TO:"))
				{
					if (!strncmp(com_token, "TO:<", 4))
					{
						token = com_token+3;
					}
					else
					{
						token = NULL;
						SV_SMTP_QueueMessage(cl, "501 Syntax error. Expected RCPT TO: toaddr\r\n");
					}
				}
				if (token)
				{
					SV_SMTP_QueueMessage(cl, "250 Yada yada yada\r\n");
					Q_strncpyz(cl->toaddr, token, sizeof(cl->toaddr));
				}
			}
			else if (!strcmp(com_token, "DATA"))
			{
				cl->file = tmpfile();
				if (!cl->file)
				{
					SV_SMTP_QueueMessage(cl, "550 Access Denied to You.\r\n");
				}
				else
				{
					SV_SMTP_QueueMessage(cl, "354 I'm waiting\r\n");
					cl->gettingdata = true;
				}
			}
			else
				SV_SMTP_QueueMessage(cl, "500 Stop speaking pig-latin\r\n");
		}
		cl->inmessagelen -= nl - cl->inmessagebuffer;
		memmove(cl->inmessagebuffer, nl, strlen(nl)+1);
	}

	return false;
}

static void SV_SMTP_RunClients(void)
{
	svsmtpclient_t *cl, *prev;

	cl = svsmtpclient;
	prev = NULL;
	while(cl)
	{
		if (cl->timeout < realtime || SV_SMTP_RunClient(cl))
		{
			closesocket(cl->socket);
			if (prev)
				prev->next = cl->next;
			else
				svsmtpclient = cl->next;

			if (cl->file)
				fclose(cl->file);
			IWebFree(cl);
			if (prev)
				cl = prev->next;
			else
				cl = svsmtpclient;
			continue;
		}
		prev = cl;
		cl = cl->next;
	}
}

qboolean SV_SMTP(qboolean activewanted)
{
	struct sockaddr_qstorage from;
	int fromlen;
	int clientsock;

	if (!smtpactive)
	{
		if (activewanted)
			SMTP_ServerInit();
		else
			return false;
	}
	else if (!activewanted)
	{
		SMTP_ServerShutdown();
		return false;
	}

	fromlen = sizeof(from);
	clientsock = accept(smtpserversocket, (struct sockaddr *)&from, &fromlen);

	if (clientsock == -1)
	{
		if (qerrno == ECONNABORTED || qerrno == ECONNRESET)
		{
			Con_TPrintf (TL_CONNECTIONLOSTORABORTED);
			return false;
		}
		else if (qerrno != EWOULDBLOCK)
		{
			Con_TPrintf (TL_NETGETPACKETERROR, strerror(qerrno));
			return false;
		}
	}
	else	//we got a new client. yay.
	{
		{
			netadr_t adr;
			SockadrToNetadr(&from, &adr);
			Con_DPrintf("SMTP connect initiated from %s\n", NET_AdrToString(adr));
		}
		SMTP_NewConnection(clientsock);
	}

	SV_SMTP_RunClients();

	return true;
}

#endif
