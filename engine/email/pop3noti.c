#include "bothdefs.h"

#ifdef EMAILCLIENT

//the idea is to send a UIDL request, and compare against the previous list.
//this list will be stored on disk on quit.

//be aware that we cannot stay connected. POP3 mailboxs are not refreshable without disconnecting.
//so we have a special state.


#include "quakedef.h"
#include "winquake.h"

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


char *MD5_GetPop3APOPString(char *timestamp, char *secrit);



#include "hash.h"






//exported.
void POP3_CreateConnection(char *servername, char *username, char *password);
cvar_t pop3_checkfrequency = {"pop3_checkfrequency", "60"};	//once a min
void POP3_Think (void);
void POP3_WriteCache (void);
//end export list.

#define HASHELEMENTS 512

static hashtable_t pop3msghash;

qboolean POP3_IsMessageUnique(char *hash)
{
	char *buf;
	if (!pop3msghash.numbuckets)
		Hash_InitTable(&pop3msghash, HASHELEMENTS, BZ_Malloc(Hash_BytesForBuckets(HASHELEMENTS)));
	if (Hash_Get(&pop3msghash, hash))
		return false;

	buf = Z_Malloc(sizeof(bucket_t) + strlen(hash)+1);
	strcpy(buf+sizeof(bucket_t), hash);
	hash = buf+sizeof(bucket_t);
	Hash_Add(&pop3msghash, hash, hash, (bucket_t *)buf);

	return true;
}

#define POP3_PORT	110


typedef struct pop3_con_s {
	char server[128];
	char username[128];
	char password[128];

	float lastnoop;

	//these are used so we can fail a send.
	//or recieve only part of an input.
	//FIXME:	make dynamically sizable, as it could drop if the send is too small (That's okay.
	//			but if the read is bigger than one command we suddenly fail entirly.)
	int sendlen;
	int sendbuffersize;
	char *sendbuffer;
	int readlen;
	int readbuffersize;
	char *readbuffer;

	qboolean drop;

	int socket;

	//we have a certain number of stages.
	enum {
		POP3_NOTCONNECTED,
		POP3_WAITINGFORINITIALRESPONCE,	//waiting for an initial response.
		POP3_AUTHING,	//wating for a response from USER
		POP3_AUTHING2,	//Set PASS, waiting to see if we passed.
		POP3_LISTING,	//Sent UIDL, waiting to see
		POP3_RETRIEVING,	//sent TOP, waiting for message headers to print info.
		POP3_HEADER,
		POP3_BODY,
		POP3_QUITTING
	} state;

	int retrlist[256];	//unrecognised uidls are added to this list.
	int numtoretrieve;

	char msgsubject[256];
	char msgfrom[256];

	struct pop3_con_s *next;
} pop3_con_t;

static pop3_con_t *pop3sv;

void POP3_CreateConnection(char *addy, char *username, char *password)
{
	unsigned long _true = true;
	struct sockaddr_in from;
	struct sockaddr_qstorage to;
	pop3_con_t *con;

	for (con = pop3sv; con; con = con->next)
	{
		if (!strcmp(con->server, addy) && !strcmp(con->username, username))
		{
			if (con->state == POP3_NOTCONNECTED && !con->socket)
				break;
			Con_Printf("Already connected to that pop3 server\n");
			return;
		}
	}

	{//quake routines using dns and stuff (Really, I wanna keep quake and pop3 fairly seperate)
		netadr_t qaddy;		
		if (!NET_StringToAdr (addy, &qaddy))
			return;	//failed to resolve dns.
		if (!qaddy.port)
			qaddy.port = htons(POP3_PORT);
		NetadrToSockadr(&qaddy, &to);
	}

	if (!con)
		con = IWebMalloc(sizeof(pop3_con_t));
	else
		con->state = POP3_WAITINGFORINITIALRESPONCE;

	

	if ((con->socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		Sys_Error ("POP3_CreateConnection: socket: %s\n", strerror(qerrno));
	}

	memset(&from, 0, sizeof(from));
	from.sin_family = AF_INET;
	if (bind(con->socket, (struct sockaddr*)&from, sizeof(from)) == -1)
	{
		IWebWarnPrintf ("POP3_CreateConnection: failed to bind: %s\n", strerror(qerrno));
	}

	//not yet blocking.
	if (connect(con->socket, (struct sockaddr*)&to, sizeof(to)) == -1)
	{
		IWebWarnPrintf ("POP3_CreateConnection: connect: %s\n", strerror(qerrno));
		closesocket(con->socket);		
		IWebFree(con);
		return;
	}
	
	if (ioctlsocket (con->socket, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		Sys_Error ("POP3_CreateConnection: ioctl FIONBIO: %s\n", strerror(qerrno));
	}

	Q_strncpyz(con->server, addy, sizeof(con->server));
	Q_strncpyz(con->username, username, sizeof(con->username));
	Q_strncpyz(con->password, password, sizeof(con->password));

	if (!con->state)
	{
		con->state = POP3_WAITINGFORINITIALRESPONCE;

		con->next = pop3sv;
		pop3sv = con;

		Con_Printf("Connected to %s\n", con->server);
	}
}

static void POP3_EmitCommand(pop3_con_t *pop3, char *text)
{
	int newlen;

	newlen = pop3->sendlen + strlen(text) + 2;

	if (newlen >= pop3->sendbuffersize || !pop3->sendbuffer)	//pre-length check.
	{
		char *newbuf;
		pop3->sendbuffersize = newlen*2;
		newbuf = IWebMalloc(pop3->sendbuffersize);
		if (!newbuf)
		{
			Con_Printf("Memory is low\n");
			pop3->drop = true;	//failed.
			return;
		}
		if (pop3->sendbuffer)
		{
			memcpy(newbuf, pop3->sendbuffer, pop3->sendlen);
			IWebFree(pop3->sendbuffer);
		}
		pop3->sendbuffer = newbuf;
	}

	pop3->sendlen = newlen;

	strncat(pop3->sendbuffer, text, pop3->sendbuffersize-1);
	strncat(pop3->sendbuffer, "\r\n", pop3->sendbuffersize-1);

//	Con_Printf("^3%s\n", text);
}

static qboolean POP3_ThinkCon(pop3_con_t *pop3)	//false means drop the connection.
{
	char *ending;
	int len;

	if (pop3->state == POP3_NOTCONNECTED && !pop3->socket)
	{
		if (pop3->lastnoop + pop3_checkfrequency.value < Sys_DoubleTime())
		{	//we need to recreate the connection now.
			pop3->lastnoop = Sys_DoubleTime();
			POP3_CreateConnection(pop3->server, pop3->username, pop3->password);
		}

		return true;
	}

	//get the buffer, stick it in our read holder
	if (pop3->readlen+32 >= pop3->readbuffersize || !pop3->readbuffer)
	{
		len = pop3->readbuffersize;
		if (!pop3->readbuffer)
			pop3->readbuffersize = 256;
		else
			pop3->readbuffersize*=2;

		ending = IWebMalloc(pop3->readbuffersize);
		if (!ending)
		{
			Con_Printf("Memory is low\n");
			return false;
		}
		if (pop3->readbuffer)
		{
			memcpy(ending, pop3->readbuffer, len);
			IWebFree(pop3->readbuffer);
		}
		pop3->readbuffer = ending;
	}

	len = recv(pop3->socket, pop3->readbuffer+pop3->readlen, pop3->readbuffersize-pop3->readlen-1, 0);
	if (len>0)
	{
		pop3->readlen+=len;
		pop3->readbuffer[pop3->readlen] = '\0';
	}

	if (pop3->readlen>0)
	{
		ending = strstr(pop3->readbuffer, "\r\n");

		if (ending)	//pollable text.
		{
			*ending = '\0';
//			Con_Printf("^2%s\n", pop3->readbuffer);

			ending+=2;
			if (pop3->state == POP3_WAITINGFORINITIALRESPONCE)
			{
				if (!strncmp(pop3->readbuffer, "+OK", 3))
				{
					char *angle1;
					char *angle2 = NULL;
					angle1 = strchr(pop3->readbuffer, '<');
					if (angle1)
					{
						angle2 = strchr(angle1+1, '>');
					}
					if (angle2)
					{	//just in case
						angle2[1] = '\0';

						POP3_EmitCommand(pop3, va("APOP %s %s", pop3->username, MD5_GetPop3APOPString(angle1, pop3->password)));
						pop3->state = POP3_AUTHING2;
					}
					else
					{
						POP3_EmitCommand(pop3, va("USER %s", pop3->username));
						pop3->state = POP3_AUTHING;
					}
				}
				else
				{
					Con_Printf("Unexpected response from POP3 server\n");
					return false;	//some sort of error.
				}
			}
			else if (pop3->state == POP3_AUTHING)
			{
				if (!strncmp(pop3->readbuffer, "+OK", 3))
				{
					POP3_EmitCommand(pop3, va("PASS %s", pop3->password));
					pop3->state = POP3_AUTHING2;
				}
				else
				{
					Con_Printf("Unexpected response from POP3 server.\nCheck username/password\n");
					return false;	//some sort of error.
				}
			}
			else if (pop3->state == POP3_AUTHING2)
			{
				if (!strncmp(pop3->readbuffer, "+OK", 3))
				{
					POP3_EmitCommand(pop3, "UIDL");
					pop3->state = POP3_LISTING;
					pop3->lastnoop = Sys_DoubleTime();
				}
				else
				{
					Con_Printf("Unexpected response from POP3 server.\nCheck username/password\n");
					return false;
				}
			}
			else if (pop3->state == POP3_LISTING)
			{
				if (!strncmp(pop3->readbuffer, "-ERR", 4))
				{
					Con_Printf("Unexpected response from POP3 server.\nUIDL not supported?\n");
					return false;
				}
				else if (!strncmp(pop3->readbuffer, "+OK", 3))
				{
				}
				else if (!strncmp(pop3->readbuffer, ".", 1))	//we only ever search for recent messages. So we fetch them and get sender and subject.
				{
					if (!pop3->numtoretrieve)
					{
						pop3->state = POP3_QUITTING;
						POP3_EmitCommand(pop3, "QUIT");
					}
					else
					{
						pop3->state = POP3_RETRIEVING;
						POP3_EmitCommand(pop3, va("RETR %i", pop3->retrlist[--pop3->numtoretrieve]));
					}
				}
				else
				{
					char *s;
					s = pop3->readbuffer;
					if (*s)
					{
						s++;
						while (*s >= '0' && *s <= '9')
							s++;
						while (*s == ' ')
							s++;
					}

					if (POP3_IsMessageUnique(s))
						if (pop3->numtoretrieve < sizeof(pop3->retrlist)/sizeof(pop3->retrlist[0]))
							pop3->retrlist[pop3->numtoretrieve++] = atoi(pop3->readbuffer);
				}
			}
			else if (pop3->state == POP3_RETRIEVING)
			{
				if (!strncmp(pop3->readbuffer, "+OK", 3))
				{
					pop3->msgsubject[0] = '\0';
					pop3->msgfrom[0] = '\0';

					pop3->state = POP3_HEADER;
				}
				else
				{	//erm... go for the next?
					if (!pop3->numtoretrieve)
					{
						pop3->state = POP3_QUITTING;
						POP3_EmitCommand(pop3, "QUIT");
					}
					else
						POP3_EmitCommand(pop3, va("RETR %i", pop3->retrlist[--pop3->numtoretrieve]));
				}
			}
			else if (pop3->state == POP3_HEADER)
			{
				if (!strnicmp(pop3->readbuffer, "From: ", 6))
					Q_strncpyz(pop3->msgfrom, pop3->readbuffer + 6, sizeof(pop3->msgfrom));
				else if (!strnicmp(pop3->readbuffer, "Subject: ", 9))
					Q_strncpyz(pop3->msgsubject, pop3->readbuffer + 9, sizeof(pop3->msgsubject));
				else if (!strncmp(pop3->readbuffer, ".", 1))
				{
					Con_Printf("New message:\nFrom: %s\nSubject: %s\n", pop3->msgfrom, pop3->msgsubject);

					if (!pop3->numtoretrieve)
					{
						pop3->state = POP3_QUITTING;
						POP3_EmitCommand(pop3, "QUIT");
					}
					else
					{
						pop3->state = POP3_RETRIEVING;
						POP3_EmitCommand(pop3, va("RETR %i", pop3->retrlist[--pop3->numtoretrieve]));
					}
				}
				else if (!*pop3->readbuffer)
					pop3->state = POP3_BODY;
			}
			else if (pop3->state == POP3_BODY)
			{
				if (!strncmp(pop3->readbuffer, "..", 2))
				{
					//line of text, skipping first '.'
					Con_Printf("%s\n", pop3->readbuffer+1);
				}
				else if (!strncmp(pop3->readbuffer, ".", 1))
				{
					Con_Printf("New message:\nFrom: %s\nSubject: %s\n", pop3->msgfrom, pop3->msgsubject);

					if (!pop3->numtoretrieve)
					{
						pop3->state = POP3_QUITTING;
						POP3_EmitCommand(pop3, "QUIT");
					}
					else
					{
						pop3->state = POP3_RETRIEVING;
						POP3_EmitCommand(pop3, va("RETR %i", pop3->retrlist[--pop3->numtoretrieve]));
					}
				}
				else
				{
					//normal line of text
					Con_Printf("%s\n", pop3->readbuffer);
				}
			}
			else if (pop3->state == POP3_QUITTING)
			{
				pop3->state = POP3_NOTCONNECTED;
				closesocket(pop3->socket);
				pop3->lastnoop = Sys_DoubleTime();
				pop3->socket = 0;
				pop3->readlen = 0;
				pop3->sendlen = 0;
				return true;
			}
			else
			{
				Con_Printf("Bad client state\n");
				return false;
			}
			pop3->readlen -= ending - pop3->readbuffer;
			memmove(pop3->readbuffer, ending, strlen(ending)+1);
		}
	}
	if (pop3->drop)
		return false;

	if (pop3->sendlen)
	{
		len = send(pop3->socket, pop3->sendbuffer, pop3->sendlen, 0);
		if (len>0)
		{
			pop3->sendlen-=len;
			memmove(pop3->sendbuffer, pop3->sendbuffer+len, pop3->sendlen+1);
		}
	}
	return true;
}

void POP3_Think (void)
{
	pop3_con_t *prev = NULL;
	pop3_con_t *pop3;

	for (pop3 = pop3sv; pop3; pop3 = pop3->next)
	{
		if (pop3->drop || !POP3_ThinkCon(pop3))
		{
			if (!prev)
				pop3sv = pop3->next;
			else
				prev->next = pop3->next;
			if (pop3->socket)
				closesocket(pop3->socket);
			BZ_Free(pop3);
			if (!prev)
				break;
		}

		prev = pop3;
	}
}

#endif
