#include "quakedef.h"

#ifdef EMAILSERVER

#include "winquake.h"

//FIXME: the DELE command's effects arn't properly checked.
//FIXME: no UIDL command

//FIXME: remove sequential naming.

char *MD5_GetPop3APOPString(char *timestamp, char *secrit);







#include "hash.h"








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
#include <stdio.h>
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


#define POP3_PORT	110
#define POP3_TIMEOUT	30

static qboolean pop3active;
static int pop3serversocket;

typedef struct {
	char filename[MAX_QPATH];
	int size;
	qboolean deleted;
	bucket_t bucket;
} pop3message_t;

typedef struct svpop3client_s {
	struct svpop3client_s *next;

	int socket;

	float timeout;

	char *messagelump;
	int messagelumppos;
	int messagelumplen;
	qboolean messagelumphitbody;
	int messagelumplines;	//lines to send past header

	int nummessages;
	int totalsize;

	char greeting[64];
	char username[64];
	qboolean loggedin;

	char inmessagebuffer[1024];
	int inmessagelen;
	char outmessagebuffer[1024];
	int outmessagelen;
	qboolean dropwhensent;

#define NUMBUCKETS 64
	hashtable_t emails;
	bucket_t *bucketpointer[NUMBUCKETS];
} svpop3client_t;
static svpop3client_t *svpop3client;





static void POP3_ServerInit(void)
{
	struct sockaddr_in address;
	unsigned long _true = true;
	int port = POP3_PORT;

	if ((pop3serversocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		Sys_Error ("FTP_TCP_OpenSocket: socket:", strerror(qerrno));
	}

	if (ioctlsocket (pop3serversocket, FIONBIO, &_true) == -1)
	{
		Sys_Error ("FTP_TCP_OpenSocket: ioctl FIONBIO:", strerror(qerrno));
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);
	
	if( bind (pop3serversocket, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(pop3serversocket);
		return;
	}
	
	listen(pop3serversocket, 3);

	pop3active = true;


	IWebPrintf("POP3 server is running\n");
	return;
}

static void POP3_ServerShutdown(void)
{
	closesocket(pop3serversocket);
	pop3active = false;
}

static void SV_POP3_QueueMessage(svpop3client_t *cl, char *msg)
{
	int len = strlen(msg);
	if (len + cl->outmessagelen > sizeof(cl->outmessagebuffer)-1)
		len = sizeof(cl->outmessagebuffer)-1 - cl->outmessagelen;
	Q_strncpyz(cl->outmessagebuffer+cl->outmessagelen, msg, len+1);
	cl->outmessagelen += len;
}

static void POP3_NewConnection(int socket)
{
	unsigned long _true = true;
	svpop3client_t *newcl;
	newcl = IWebMalloc(sizeof(svpop3client_t));
	if (!newcl)	//bother
	{
		closesocket(socket);
		return;
	}
	if (ioctlsocket (socket, FIONBIO, &_true) == -1)
	{
		closesocket(socket);
		return;	
	}
	memset(newcl, 0, sizeof(svpop3client_t));

	newcl->socket = socket;
	newcl->next = svpop3client;
	svpop3client = newcl;

	newcl->timeout = realtime + POP3_TIMEOUT;

	*newcl->outmessagebuffer = '\0';

	sprintf(newcl->greeting, "<%i.%i.%i.%i.%i.%i.%i>", rand(), rand(), rand(), rand(), rand(), rand(), rand());
//	_true = strlen(newcl->greeting);

printf("newclient\n");

	SV_POP3_QueueMessage(newcl, va("+OK %s\r\n", newcl->greeting));
}

static int SV_POP3_AddMessage(char *filename, int flags, void *incl)
{
	FILE *f;
	svpop3client_t *cl = incl;
	pop3message_t *msg;

	f = fopen(filename, "rb");
	if (!f)
		return true;	//shouldn't happen

	msg = IWebMalloc(sizeof(pop3message_t));
	if (!msg)
	{
		fclose(f);
		return false;
	}

	Q_strncpyz(msg->filename, filename, sizeof(msg->filename));
	msg->deleted = false;

	fseek(f, 0, SEEK_END);
	msg->size = ftell(f);
	fclose(f);

	cl->totalsize+=msg->size;
	cl->nummessages++;

	Hash_Add(&cl->emails, va("%i", ++cl->nummessages), msg, &msg->bucket);

	return true;
}

static void SV_POP3_CountMessages(svpop3client_t *cl)
{
	Hash_InitTable(&cl->emails, NUMBUCKETS, cl->bucketpointer);
	cl->totalsize=0;
	Sys_EnumerateFiles(".", va("emails/%s/*.eml", cl->username), SV_POP3_AddMessage, cl);
}

static pop3message_t *SV_POP3_GetMessage(svpop3client_t *cl, int num)
{
	pop3message_t *msg;

	msg = IWebMalloc(sizeof(pop3message_t));
	if (!msg)
		return NULL;

	if (msg->deleted)
		return NULL;

	msg = Hash_Get(&cl->emails, va("%i", num));

	return msg;
}

static void SV_POP3_CleanUp(svpop3client_t *cl, qboolean dodelete)	//closes the messages list created by SV_POP3_CountMessages
{
	pop3message_t *msg;
	int mn;
	for (mn = 1; mn <= cl->nummessages; mn++)
	{
		msg = SV_POP3_GetMessage(cl, mn);
		if (!msg)
			continue;

		if (dodelete && msg->deleted)
		{
			remove(msg->filename);
		}

		Hash_Remove(&cl->emails, va("%i", mn));
		BZ_Free(msg);
	}
}

static qboolean SV_POP3_ReadMessage(svpop3client_t *cl, int index)
{
	FILE *f;
	pop3message_t *msg;

	msg = SV_POP3_GetMessage(cl, index);
	if (!msg)
		return false;
	if (msg->deleted)
		return false;

	f = fopen(msg->filename, "rb");
	if (!f)
		return false;
	fseek(f, 0, SEEK_END);
	cl->messagelumplen = ftell(f);
	fseek(f, 0, SEEK_SET);
	cl->messagelump = IWebMalloc(cl->messagelumplen+3);
	fread(cl->messagelump, 1, cl->messagelumplen, f);
	cl->messagelump[cl->messagelumplen++] = '\r';
	cl->messagelump[cl->messagelumplen++] = '\n';
	fclose(f);

	cl->messagelumplen = strlen(cl->messagelump);
	cl->messagelumppos = 0;
	cl->messagelumphitbody = false;
	cl->messagelumplines = 0x7fffffff;

	return true;
}

static void SV_POP3_BuildListing(svpop3client_t *cl, qboolean isuidl)
{
	pop3message_t *msg;
	int mn;
	char *listing;

	listing = cl->messagelump = IWebMalloc(cl->nummessages*64+3);

	for (mn = 1; mn <= cl->nummessages; mn++)
	{
		msg = SV_POP3_GetMessage(cl, mn);
		if (!msg || msg->deleted)
			continue;

		if (isuidl)
			sprintf(listing, "%i %s,S=%i\r\n", mn, msg->filename, msg->size);
		else
			sprintf(listing, "%i %i\r\n", mn, msg->size);
		listing += strlen(listing);
	}

	cl->messagelumplen = listing - cl->messagelump;
}

static qboolean SV_POP3_RunClient(svpop3client_t *cl)	//true means client should be dropped
{
	int read;
	char *nl;
	char *token;
	int blankline;

	if (cl->messagelump)
	{
		blankline=false;
		while (cl->outmessagelen < sizeof(cl->outmessagebuffer)-100)
		{
			if (cl->messagelumppos >= cl->messagelumplen)
				break;

			if (cl->messagelump[cl->messagelumppos] == '.')	//double up all '.'s at start of lines
				cl->outmessagebuffer[cl->outmessagelen++] = '.';

			blankline = true;
			for(;;)
			{
				if (cl->messagelumppos >= cl->messagelumplen)
					break;
				if (cl->messagelump[cl->messagelumppos] > ' ')
					blankline = false;
				cl->outmessagebuffer[cl->outmessagelen++] = cl->messagelump[cl->messagelumppos];
				if (cl->messagelump[cl->messagelumppos++] == '\n')
					break;
			}
			if (blankline)
				cl->messagelumphitbody = true;
			if (cl->messagelumphitbody)
			{
				if (cl->messagelumplines--<=0)
					cl->messagelumppos = cl->messagelumplen;	//easy way to terminate.
			}
		}
		if (cl->messagelumppos >= cl->messagelumplen)
		{	//we've sent the entire buffer now.
			cl->outmessagebuffer[cl->outmessagelen++] = '.';
			cl->outmessagebuffer[cl->outmessagelen++] = '\r';
			cl->outmessagebuffer[cl->outmessagelen++] = '\n';

			BZ_Free(cl->messagelump);
			cl->messagelump = NULL;
		}
		cl->outmessagebuffer[cl->outmessagelen] = '\0';
printf("%s\n", cl->outmessagebuffer);
	}
	if (cl->outmessagelen)
	{
		read = send(cl->socket, cl->outmessagebuffer, cl->outmessagelen, MSG_PARTIAL);
		if (read < 0)
			read = 0;
		memmove(cl->outmessagebuffer, cl->outmessagebuffer + read, cl->outmessagelen - read);
		cl->outmessagelen -= read;
		cl->outmessagebuffer[cl->outmessagelen] = '\0';

		if (cl->dropwhensent && !cl->outmessagelen)
			return true;
	}

	read = recv(cl->socket, cl->inmessagebuffer+cl->inmessagelen, sizeof(cl->inmessagebuffer)-1-cl->inmessagelen, MSG_PARTIAL);

	if (read == -1)
	{
		if (qerrno != EWOULDBLOCK)	//blocking is the only way to keep the connection on fail.
			return true;

		if (!*cl->inmessagebuffer)
			return false;
//nonblocking allows us to get multiple commands from one packet.
	}
	else if (read == 0)	//don't quite know why this happens.
		return true;	//believed to be an indication that the other end has disconnected.
	else
	{
		cl->timeout = realtime + POP3_TIMEOUT;

		cl->inmessagelen += read;
		if (cl->inmessagelen >= sizeof(cl->inmessagebuffer)-1)	//happens if we fill the buffer with no hope of empting it.
			return true;
		cl->inmessagebuffer[cl->inmessagelen] = '\0';
	}
	nl = strchr(cl->inmessagebuffer, '\n');
	if (nl)
	{
		*nl = '\0';
//Con_Printf("%s\n", cl->inmessagebuffer);
		read = nl - cl->inmessagebuffer + 1;

		token = COM_ParseToken(cl->inmessagebuffer);

//auth mechanism 1
		if (!strcmp(com_token, "USER"))
		{
			token = COM_ParseToken(token);
			if (*com_token)
			{
				Q_strncpyz(cl->username, com_token, sizeof(cl->username));
				SV_POP3_QueueMessage(cl,	"+OK User name accepted, password please\r\n");

				SV_POP3_CleanUp(cl, true);
				cl->loggedin = false;
			}
			else
				SV_POP3_QueueMessage(cl,	"-ERR no username was specified\r\n");
		}
		else if (!strcmp(com_token, "PASS"))
		{
#ifndef CLIENTONLY
			int id;
			extern cvar_t rank_filename;

			token = COM_ParseToken(token);
			id = Rank_GetPlayerID(cl->username, atoi(com_token), false);
			if (!id && *rank_filename.string)
			{
				SV_POP3_QueueMessage(cl,	"-ERR User or Password not valid\r\n");

				SV_POP3_CleanUp(cl, true);
				cl->loggedin = false;
			}
			else
#endif
			{
				SV_POP3_QueueMessage(cl,	"+OK Logged in\r\n");
				cl->loggedin = true;
				SV_POP3_CountMessages(cl);
			}
		}

//auth2
		else if (!strcmp(com_token, "APOP"))
		{
			int id;
			int pass;
			extern cvar_t rank_filename;

			token = COM_ParseToken(token);
			if (*com_token)
			{
				Q_strncpyz(cl->username, com_token, sizeof(cl->username));

#ifndef CLIENTONLY
				token = COM_ParseToken(token);
				pass = Rank_GetPass(cl->username);
				id = Rank_GetPlayerID(cl->username, pass, false);
				if ((!id && *rank_filename.string) || strcmp(MD5_GetPop3APOPString(cl->greeting, va("%i", pass)), com_token))
				{
					SV_POP3_QueueMessage(cl,	"-ERR User or Password not valid\r\n");

					SV_POP3_CleanUp(cl, true);
					cl->loggedin = false;
				}
				else
#endif
				{
					SV_POP3_QueueMessage(cl,	"+OK Logged in\r\n");
					cl->loggedin = true;
					SV_POP3_CountMessages(cl);
				}
			}
		}


//now they need to have been logged in properly.
		else if (!cl->loggedin)
			SV_POP3_QueueMessage(cl,	"-ERR You didn't log in properly\r\n");
		else if (!strcmp(com_token, "STAT"))
		{
			char text[64];
			sprintf(text, "+OK %i %i\r\n", cl->nummessages, cl->totalsize);
			SV_POP3_QueueMessage(cl,	text);
		}
		else if (!strcmp(com_token, "LIST"))
		{
			SV_POP3_QueueMessage(cl,	"+OK EMail listing follows:\r\n");
			SV_POP3_BuildListing(cl, false);
		}
		else if (!strcmp(com_token, "DELE"))
		{
			pop3message_t *msg;
			int mnum;
			token = COM_ParseToken(token);
			mnum = atoi(com_token);

			msg = SV_POP3_GetMessage(cl, mnum);
			if (!msg)
				SV_POP3_QueueMessage(cl,	"-ERR message index out of range\r\n");
			else if (msg->deleted)
				SV_POP3_QueueMessage(cl,	"-ERR message already deleted\r\n");
			else
			{
				msg->deleted = true;
				SV_POP3_QueueMessage(cl,	"+OK Message marked for deleted\r\n");
			}
		}
		else if (!strcmp(com_token, "TOP"))
		{
			token = COM_ParseToken(token);
			if (SV_POP3_ReadMessage(cl, atoi(com_token)))
			{
				token = COM_ParseToken(token);
				SV_POP3_QueueMessage(cl,	"+OK message contents follow:\n");
				cl->messagelumplines = atoi(com_token);
			}
			else
				SV_POP3_QueueMessage(cl,	"-ERR Message index wasn't valid\n");
		}
		else if (!strcmp(com_token, "RETR"))
		{
			token = COM_ParseToken(token);
			if (SV_POP3_ReadMessage(cl, atoi(com_token)))
				SV_POP3_QueueMessage(cl,	"+OK message contents follow:\n");
			else
				SV_POP3_QueueMessage(cl,	"-ERR Message index wasn't valid\n");
		}
		else if (!strcmp(com_token, "UIDL"))
		{
			SV_POP3_QueueMessage(cl,	"+OK I hope someone likes you\r\n");
			SV_POP3_BuildListing(cl, true);
		}
		else if (!strcmp(com_token, "QUIT"))
		{
			SV_POP3_CleanUp(cl, true);

			SV_POP3_QueueMessage(cl,	"+OK I hope someone likes you\r\n");
			cl->dropwhensent = true;
		}
		else
			SV_POP3_QueueMessage(cl,	"-ERR Unrecognised command\r\n");
//printf("%s\n", cl->outmessagebuffer);

		memmove(cl->inmessagebuffer, cl->inmessagebuffer + read, cl->inmessagelen - read);
		cl->inmessagelen -= read;
	}

	return false;
}

static void SV_POP3_RunClients(void)
{
	svpop3client_t *cl, *prev;

	cl = svpop3client;
	prev = NULL;
	while(cl)
	{
		if (cl->timeout < realtime || SV_POP3_RunClient(cl))
		{
printf("drop client\n");
			closesocket(cl->socket);
			if (prev)
				prev->next = cl->next;
			else
				svpop3client = cl->next;

			if (cl->messagelump)
				IWebFree(cl->messagelump);
			SV_POP3_CleanUp(cl, false);

			IWebFree(cl);
			if (prev)
				cl = prev->next;
			else
				cl = svpop3client;
			continue;
		}
		prev = cl;
		cl = cl->next;
	}
}

qboolean SV_POP3(qboolean activewanted)
{
	struct sockaddr from;
	int fromlen;
	int clientsock;

	if (!pop3active)
	{
		if (activewanted)
			POP3_ServerInit();
		else
			return false;
	}
	else if (!activewanted)
	{
		POP3_ServerShutdown();
		return false;
	}

	fromlen = sizeof(from);
	clientsock = accept(pop3serversocket, (struct sockaddr *)&from, &fromlen);

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
		POP3_NewConnection(clientsock);

	SV_POP3_RunClients();

	return true;
}

#endif
