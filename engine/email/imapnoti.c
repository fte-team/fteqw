#include "quakedef.h"

#ifdef EMAILCLIENT

//code to sit on an imap server and check for new emails every now and then.

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



//exported.
void IMAP_CreateConnection(char *servername, char *username, char *password);
cvar_t imap_checkfrequency = {"imap_checkfrequency", "60"};	//once a min
void IMAP_Think (void);
//end export list.




#define IMAP_PORT 143


typedef struct imap_con_s {
	char server[128];
	char username[128];
	char password[128];

	float lastnoop;

	//these are used so we can fail a send.
	//or recieve only part of an input.
	//FIXME:	make dynamically sizable, as it could drop if the send is too small (That's okay.
	//			but if the read is bigger than one command we suddenly fail entirly.
	int sendlen;
	int sendbuffersize;
	char *sendbuffer;
	int readlen;
	int readbuffersize;
	char *readbuffer;

	qboolean drop;

	int socket;

	enum {
		IMAP_WAITINGFORINITIALRESPONCE,
		IMAP_AUTHING,
		IMAP_AUTHED,
		IMAP_INBOX
	} state;

	struct imap_con_s *next;
} imap_con_t;

static imap_con_t *imapsv;

void IMAP_CreateConnection(char *addy, char *username, char *password)
{
	unsigned long _true = true;
	struct sockaddr_qstorage	from;
	imap_con_t *con;

	for (con = imapsv; con; con = con->next)
	{
		if (!strcmp(con->server, addy))
		{
			Con_Printf("Already connected to that imap server\n");
			return;
		}
	}

	con = IWebMalloc(sizeof(imap_con_t));

	

	if ((con->socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		Sys_Error ("IMAP_CreateConnection: socket: %s\n", strerror(qerrno));
	}


	{//quake routines using dns and stuff (Really, I wanna keep quake and imap fairly seperate)
		netadr_t qaddy;		
		NET_StringToAdr (addy, &qaddy);
		if (!qaddy.port)
			qaddy.port = htons(IMAP_PORT);
		NetadrToSockadr(&qaddy, &from);
	}

	//not yet blocking.
	if (connect(con->socket, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		IWebWarnPrintf ("IMAP_CreateConnection: connect: %i %s\n", qerrno, strerror(qerrno));
		closesocket(con->socket);		
		IWebFree(con);
		return;
	}
	
	if (ioctlsocket (con->socket, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		Sys_Error ("IMAP_CreateConnection: ioctl FIONBIO: %s\n", strerror(qerrno));
	}

	Q_strncpyz(con->server, addy, sizeof(con->server));
	Q_strncpyz(con->username, username, sizeof(con->username));
	Q_strncpyz(con->password, password, sizeof(con->password));

	con->next = imapsv;
	imapsv = con;
}

static void IMAP_EmitCommand(imap_con_t *imap, char *text)
{
	int newlen;
	char rt[64];
	sprintf(rt, "* ");	//now this is lame. Quite possibly unreliable...
	//makes a few things easier though

	newlen = imap->sendlen + strlen(text) + strlen(rt) + 2;

	if (newlen >= imap->sendbuffersize || !imap->sendbuffer)	//pre-length check.
	{
		char *newbuf;
		imap->sendbuffersize = newlen*2;
		newbuf = IWebMalloc(imap->sendbuffersize);
		if (!newbuf)
		{
			Con_Printf("Memory is low\n");
			imap->drop = true;	//failed.
			return;
		}
		if (imap->sendbuffer)
		{
			memcpy(newbuf, imap->sendbuffer, imap->sendlen);
			IWebFree(imap->sendbuffer);
		}
		imap->sendbuffer = newbuf;
	}

	imap->sendlen = newlen;

	strncat(imap->sendbuffer, rt, imap->sendbuffersize-1);
	strncat(imap->sendbuffer, text, imap->sendbuffersize-1);
	strncat(imap->sendbuffer, "\r\n", imap->sendbuffersize-1);
}

static char *IMAP_AddressStructure(char *msg, char *out, int outsize)
{
	char name[256];
	char mailbox[64];
	char hostname[128];
	int indents=0;
	while(*msg == ' ')
		msg++;
	while(*msg == '(')	//do it like this, we can get 2... I'm not sure if that's always true..
	{
		msg++;
		indents++;
	}

	msg = COM_Parse(msg);	//name
	Q_strncpyz(name, com_token, sizeof(name));
	msg = COM_Parse(msg);	//smtp route (ignored normally)
	msg = COM_Parse(msg);	//mailbox
	Q_strncpyz(mailbox, com_token, sizeof(mailbox));
	msg = COM_Parse(msg);	//hostname
	Q_strncpyz(hostname, com_token, sizeof(hostname));

	while(indents && *msg == ')')
		msg++;

	if (out)
	{
		if (!strcmp(name, "NIL"))
		{
			Q_strncpyz(out, mailbox, outsize-1);
			strncat(out, "@", outsize-1);
			strncat(out, hostname, outsize-1);
		}
		else
		{
			Q_strncpyz(out, name, outsize-1);

			strncat(out, " <", outsize-1);
			strncat(out, mailbox, outsize-1);
			strncat(out, "@", outsize-1);
			strncat(out, hostname, outsize-1);
			strncat(out, ">", outsize-1);
		}
	}

	return msg;
}

static qboolean IMAP_ThinkCon(imap_con_t *imap)	//false means drop the connection.
{
	char *ending;
	int len;

	//get the buffer, stick it in our read holder
	if (imap->readlen+32 >= imap->readbuffersize || !imap->readbuffer)
	{
		len = imap->readbuffersize;
		if (!imap->readbuffer)
			imap->readbuffersize = 256;
		else
			imap->readbuffersize*=2;

		ending = IWebMalloc(imap->readbuffersize);
		if (!ending)
		{
			Con_Printf("Memory is low\n");
			return false;
		}
		if (imap->readbuffer)
		{
			memcpy(ending, imap->readbuffer, len);
			IWebFree(imap->readbuffer);
		}
		imap->readbuffer = ending;
	}

	len = recv(imap->socket, imap->readbuffer+imap->readlen, imap->readbuffersize-imap->readlen-1, 0);
	if (len>0)
	{
		imap->readlen+=len;
		imap->readbuffer[imap->readlen] = '\0';
	}

	if (imap->readlen>0)
	{
		ending = strstr(imap->readbuffer, "\r\n");

		if (ending)	//pollable text.
		{
			*ending = '\0';
//			Con_Printf("%s\n", imap->readbuffer);

			ending+=2;
			if (imap->state == IMAP_WAITINGFORINITIALRESPONCE)
			{
				//can be one of two things.
				if (!strncmp(imap->readbuffer, "* OK", 4))
				{
					IMAP_EmitCommand(imap, va("LOGIN %s %s", imap->username, imap->password));
					imap->state = IMAP_AUTHING;
				}
				else if (!strncmp(imap->readbuffer, "* PREAUTH", 9))
				{
					Con_Printf("Logged on to %s\n", imap->server);
					IMAP_EmitCommand(imap, "SELECT INBOX");
					imap->state = IMAP_AUTHED;
					imap->lastnoop = Sys_DoubleTime();
				}
				else
				{
					Con_Printf("Unexpected response from IMAP server\n");
					return false;
				}
			}
			else if (imap->state == IMAP_AUTHING)
			{
				if (!strncmp(imap->readbuffer, "* OK", 4))
				{
					Con_Printf("Logged on to %s\n", imap->server);
					IMAP_EmitCommand(imap, "SELECT INBOX");
					imap->state = IMAP_AUTHED;
					imap->lastnoop = Sys_DoubleTime();
				}
				else
				{
					Con_Printf("Unexpected response from IMAP server\n");
					return false;
				}
			}
			else if (imap->state == IMAP_AUTHED)
			{
				char *num;
				num = imap->readbuffer;
				if (!strncmp(imap->readbuffer, "* SEARCH ", 8))	//we only ever search for recent messages. So we fetch them and get sender and subject.
				{
					char *s;
					s = imap->readbuffer+8;
					num = NULL;
					while(*s)
					{
						s++;
						num = s;
						while (*s >= '0' && *s <= '9')
							s++;

						IMAP_EmitCommand(imap, va("FETCH %i ENVELOPE", atoi(num)));	//envelope so that it's all one line.
					}
				}
				else if (imap->readbuffer[0] == '*' && imap->readbuffer[1] == ' ')
				{
					num = imap->readbuffer+2;
					while(*num >= '0' && *num <= '9')
					{
						num++;
					}
					if (!strcmp(num, " RECENT"))
					{
						if (atoi(imap->readbuffer+2) > 0)
						{
							IMAP_EmitCommand(imap, "SEARCH RECENT");
						}
					}
					else if (!strncmp(num, " FETCH (ENVELOPE (", 18))
					{
						char from[256];
						char subject[256];

						num += 18;

						num = COM_Parse(num);
//						Con_Printf("Date/Time: %s\n", com_token);

						num = COM_Parse(num);
						Q_strncpyz(subject, com_token, sizeof(subject));

						num = IMAP_AddressStructure(num, from, sizeof(from));


						if ((rand() & 3) == 3)
						{
							if (rand())
								Con_Printf("\n^2New spam has arrived\n");
							else
								Con_Printf("\n^2You have new spam\n");
						}
						else if (rand()&1)
							Con_Printf("\n^2New mail has arrived\n");
						else
							Con_Printf("\n^2You have new mail\n");

						Con_Printf("Subject: %s\n", subject);
						Con_Printf("From: %s\n", from);

						SCR_CenterPrint(0, va("NEW MAIL HAS ARRIVED\n\nTo: %s@%s\nFrom: %s\nSubject: %s", imap->username, imap->server, from, subject));

						//throw the rest away.
					}
				}
			}
			else
			{
				Con_Printf("Bad client state\n");
				return false;
			}
			imap->readlen -= ending - imap->readbuffer;
			memmove(imap->readbuffer, ending, strlen(ending)+1);
		}
	}
	if (imap->drop)
		return false;

	if (imap->state == IMAP_AUTHED)
	{
		if (imap->lastnoop + imap_checkfrequency.value < Sys_DoubleTime())
		{	//we need to keep the connection reasonably active

			IMAP_EmitCommand(imap, "SELECT INBOX");	//this causes the recent flags to be reset. This is the only way I found.
			imap->lastnoop = Sys_DoubleTime();
		}
	}

	if (imap->sendlen)
	{
		len = send(imap->socket, imap->sendbuffer, imap->sendlen, 0);
		if (len>0)
		{
			imap->sendlen-=len;
			memmove(imap->sendbuffer, imap->sendbuffer+len, imap->sendlen+1);
		}
	}
	return true;
}

void IMAP_Think (void)
{
	imap_con_t *prev = NULL;
	imap_con_t *imap;

	for (imap = imapsv; imap; imap = imap->next)
	{
		if (imap->drop || !IMAP_ThinkCon(imap))
		{
			if (!prev)
				imapsv = imap->next;
			else
				prev->next = imap->next;
			closesocket(imap->socket);
			BZ_Free(imap);
			if (!prev)
				break;
		}

		prev = imap;
	}
}

#endif
