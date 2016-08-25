#include "quakedef.h"

#ifdef WEBSVONLY
#undef vsnprintf
#undef _vsnprintf
#ifdef _WIN32
#define vsnprintf _vsnprintf
#endif
#endif

#ifdef WEBSERVER

#include "iweb.h"

//hows this as a bug.
//TCP data can travel at different speeds.
//If the later bits of a data channel arrive after the message saying that a transfer was compleate,
//the later bits of the file may not arrive before the client closes the conenction.
//this is a major bug and can prevent the server from giving files at a high pl/ping

#include "netinc.h"

static iwboolean ftpserverinitied = false;
static SOCKET	ftpserversocket = INVALID_SOCKET;
qboolean ftpserverfailed;


typedef struct FTPclient_s{
	char name[64];
	char pwd[64];
	int auth;	//has it got auth?
	char path[256];

	char commandbuffer[256];
	char messagebuffer[256];
	int cmdbuflen;
	int msgbuflen;

	SOCKET controlsock;
	SOCKET datasock;	//FTP only allows one transfer per connection.
	int dataislisten;
	int datadir;	//0 no data, 1 reading, 2 writing
	vfsfile_t *file;

	unsigned long blocking;

	struct FTPclient_s *next;
} FTPclient_t;

FTPclient_t *FTPclient;

SOCKET FTP_BeginListening(int aftype, int port)
{
	struct sockaddr_qstorage address;
	unsigned long _true = true;
	unsigned long _false = false;
	int i;
	SOCKET sock;

#ifdef IPPROTO_IPV6
	if ((sock = socket ((aftype!=1)?PF_INET6:PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
#else
	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
#endif
	{
		IWebPrintf ("FTP_BeginListening: socket: %s\n", strerror(neterrno()));
		return INVALID_SOCKET;
	}

	if (ioctlsocket (sock, FIONBIO, &_true) == -1)
	{
		IWebPrintf ("FTP_BeginListening: ioctl FIONBIO: %s", strerror(neterrno()));
		return INVALID_SOCKET;
	}

#ifdef IPPROTO_IPV6
	if (aftype != 1)
	{
		//0=ipv4+ipv6
		//2=ipv6 only
		if (aftype == 0)
		{
			if (0 > setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&_false, sizeof(_false)))
			{
				//abort and do ipv4 only if hybrid sockets don't work.
				closesocket(sock);
				return FTP_BeginListening(1, port);
			}
		}

		memset(&address, 0, sizeof(address));
		((struct sockaddr_in6*)&address)->sin6_family = AF_INET6;
		if (port == PORT_ANY)
			((struct sockaddr_in6*)&address)->sin6_port = 0;
		else
			((struct sockaddr_in6*)&address)->sin6_port = htons((short)port);
	}
	else
#endif
	{
		//1=ipv4 only
		((struct sockaddr_in*)&address)->sin_family = AF_INET;
	//ZOID -- check for interface binding option
		if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc) {
			((struct sockaddr_in*)&address)->sin_addr.s_addr = inet_addr(com_argv[i+1]);
			Con_TPrintf("Binding to IP Interface Address of %s\n",
					inet_ntoa(((struct sockaddr_in*)&address)->sin_addr));
		} else
			((struct sockaddr_in*)&address)->sin_addr.s_addr = INADDR_ANY;

		if (port == PORT_ANY)
			((struct sockaddr_in*)&address)->sin_port = 0;
		else
			((struct sockaddr_in*)&address)->sin_port = htons((short)port);
	}

	if( bind (sock, (void *)&address, sizeof(address)) == -1)
	{
		IWebPrintf("FTP_BeginListening: failed to bind socket\n");
		closesocket(ftpserversocket);
		return INVALID_SOCKET;
	}

	listen(sock, 3);

	return sock;
}

void FTP_ServerShutdown(void)
{
	closesocket(ftpserversocket);
	ftpserverinitied = false;
	IWebPrintf("FTP server is deactivated\n");
}

//we ought to filter this to remove duplicates.
static int QDECL SendFileNameTo(const char *rawname, qofs_t size, time_t mtime, void *param, searchpathfuncs_t *spath)
{
	SOCKET socket = *(SOCKET*)param;
//	int i;
	char buffer[256+1];
	char *slash;
	char nondirname[MAX_QPATH];
	int isdir = rawname[strlen(rawname)-1] == '/';
	char *fname;

#ifndef WEBSVONLY	//copy protection of the like that QWSV normally has.
	if (!isdir)
		if (!SV_AllowDownload(rawname))	//don't advertise if we're going to disallow it
			return true;
#endif

	Q_strncpyz(nondirname, rawname, sizeof(nondirname));
	if (isdir)
		nondirname[strlen(nondirname)-1] = '\0';
	fname = nondirname;

	while((slash = strchr(fname, '/')))
		fname = slash+1;

	if (isdir)
		sprintf(buffer, "drw-r--r--\t1\troot\troot\t%8u Jan 1 12:00 %s\r\n", (unsigned int)size, fname);
	else
		sprintf(buffer, "-rw-r--r--\t1\troot\troot\t%8u Jan 1 12:00 %s\r\n", (unsigned int)size, fname);

//	strcpy(buffer, fname);
//	for (i = strlen(buffer); i < 40; i+=8)
//		strcat(buffer, "\t");
	send(socket, buffer, strlen(buffer), 0);

	return true;
}

SOCKET FTP_SV_makelistensocket(unsigned long nblocking)
{
	char name[256];
	SOCKET sock;
	struct hostent *hent;

	struct sockaddr_in	address;
//	int fromlen;

	address.sin_family = AF_INET;
	if (gethostname(name, sizeof(name)) == -1)
		return INVALID_SOCKET;
	hent = gethostbyname(name);
	if (!hent)
		return INVALID_SOCKET;
	address.sin_addr.s_addr = *(int *)(hent->h_addr_list[0]);
	address.sin_port = 0;



	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		Sys_Error ("FTP_TCP_OpenSocket: socket: %s", strerror(neterrno()));
	}

	if (ioctlsocket (sock, FIONBIO, &nblocking) == -1)
	{
		Sys_Error ("FTP_TCP_OpenSocket: ioctl FIONBIO: %s", strerror(neterrno()));
	}

	if( bind (sock, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}

	listen(sock, 2);

	return sock;
}
iwboolean	FTP_SVSocketPortToString (SOCKET socket, char *s)
{
	struct sockaddr_qstorage addr;
	int adrlen = sizeof(addr);

	if (getsockname(socket, (struct sockaddr*)&addr, &adrlen) == -1)
		return false;

	if (((struct sockaddr_in*)&addr)->sin_family == AF_INET6)
		sprintf(s, "%i", ntohs(((struct sockaddr_in6*)&addr)->sin6_port));
	else
		sprintf(s, "%i", ntohs(((struct sockaddr_in*)&addr)->sin_port));
	return true;
}
//only to be used for ipv4 sockets.
iwboolean	FTP_SVSocketToString (SOCKET socket, char *s)
{
	struct sockaddr_in addr;
	qbyte *baddr;
	int adrlen = sizeof(addr);
	char name[256];

	//get the port.
	if (getsockname(socket, (struct sockaddr*)&addr, &adrlen) == -1)
		return false;

	baddr = (qbyte *)&addr.sin_addr;
	
	if (gethostname(name, sizeof(name)) != -1)
	{
		struct hostent *hent = gethostbyname(name);
		if (hent)
			baddr = hent->h_addr_list[0];
	}

	sprintf(s, "%i,%i,%i,%i,%i,%i", baddr[0], baddr[1], baddr[2], baddr[3], ((qbyte *)&addr.sin_port)[0], ((qbyte *)&addr.sin_port)[1]);
	return true;
}
iwboolean	FTP_SVRemoteSocketToString (SOCKET socket, char *s, int slen)
{
	struct sockaddr_qstorage addr;
	netadr_t na;
	int adrlen = sizeof(addr);

//	addr.sin_family = AF_INET;
	if (getpeername(socket, (struct sockaddr*)&addr, &adrlen) == -1)
	{
		*s = 0;
		return false;
	}

	SockadrToNetadr(&addr, &na);
	NET_AdrToString(s, slen, &na);

//	if (((struct sockaddr_in*)&addr)->sin_family == AF_INET6)
//	{
//	}
//	else
//		sprintf(s, "%i,%i,%i,%i,%i,%i", ((qbyte *)&addr.sin_addr)[0], ((qbyte *)&addr.sin_addr)[1], ((qbyte *)&addr.sin_addr)[2], ((qbyte *)&addr.sin_addr)[3], ((qbyte *)&addr.sin_port)[0], ((qbyte *)&addr.sin_port)[1]);
	return true;
}
/*
 *	Responsable for sending all control server -> client messages.
 *	Queues the message if it cannot send now.
 *	Kicks if too big a queue.
*/
void QueueMessage(FTPclient_t *cl, char *msg)
{
	if (send (cl->controlsock, msg, strlen(msg), 0) == -1)
	{	//wasn't sent
		if (strlen(msg) + strlen(cl->messagebuffer) >= sizeof(cl->messagebuffer)-1)
			closesocket(cl->controlsock);	//but don't mark it as closed, so we get errors later (for this is how we shall tell).
		strcat(cl->messagebuffer, msg);
	}
}

void VARGS QueueMessageva(FTPclient_t *cl, char *fmt, ...)
{
	va_list		argptr;
	char		msg[1024];

	va_start (argptr, fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	msg[sizeof(msg)-1] = 0;
	va_end (argptr);

	if (send (cl->controlsock, msg, strlen(msg), 0) == -1)
	{	//wasn't sent
		if (strlen(msg) + strlen(cl->messagebuffer) >= sizeof(cl->messagebuffer)-1)
			closesocket(cl->controlsock);	//but don't mark it as closed, so we get errors later (for this is how we shall tell).
		strcat(cl->messagebuffer, msg);
	}
}
iwboolean FTP_ServerThinkForConnection(FTPclient_t *cl)
{
	int 	ret;
	struct sockaddr_in	from;
	int		fromlen;
	char *msg, *line;

	char mode[64];
	char resource[8192];
	int _true = true;

	if (cl->datadir == 1)
	{
		int pos, sent;
		int ammount, wanted = sizeof(resource);

		pos = VFS_TELL(cl->file);
		ammount = VFS_READ(cl->file, resource, wanted);
		sent = send(cl->datasock, resource, ammount, 0);

		if (sent == -1)
		{
			VFS_SEEK(cl->file, pos);
			if (neterrno() != NET_EWOULDBLOCK)
			{
				closesocket(cl->datasock);
				cl->datasock = INVALID_SOCKET;
				VFS_CLOSE(cl->file);
				cl->file = NULL;

				QueueMessage (cl, "226 Transfer complete .\r\n");
				cl->datadir = 0;
			}
		}
		else
		{
			if (sent != ammount)
				VFS_SEEK(cl->file, pos + sent);

			if (ammount != wanted && sent == ammount)	//file is over
			{
				send(cl->datasock, resource, 0, 0);
				send(cl->datasock, resource, 0, 0);
				send(cl->datasock, resource, 0, 0);
				closesocket(cl->datasock);
				cl->datasock = INVALID_SOCKET;
				VFS_CLOSE(cl->file);
				cl->file = NULL;

				QueueMessage (cl, "226 Transfer complete .\r\n");
				cl->datadir = 0;
			}
		}

		pos = cl->datadir?1:!cl->blocking;
		if (ioctlsocket (cl->controlsock, FIONBIO, (u_long *)&pos) == -1)
		{
			IWebPrintf ("FTP_ServerRun: blocking error: %s\n", strerror(neterrno()));
			return 0;
		}
	}
	else if (cl->datadir == 2)
	{
		int len;
		while((len = recv(cl->datasock, resource, sizeof(resource), 0)) >0 )
		{
			VFS_WRITE(cl->file, resource, len);
		}
		if (len == -1)
		{
			if (neterrno() != NET_EWOULDBLOCK)
			{
				closesocket(cl->datasock);
				cl->datasock = INVALID_SOCKET;
				if (cl->file)
					VFS_CLOSE(cl->file);
				cl->file = NULL;

				QueueMessage (cl, "226 Transfer complete .\r\n");
				cl->datadir = 0;
			}
		}
		if (len == 0)
		{
			QueueMessage (cl, "226 Transfer complete .\r\n");
			VFS_CLOSE(cl->file);
			cl->file = NULL;
			cl->datadir = 0;
		}
	}

	ret = recv(cl->controlsock, cl->commandbuffer+cl->cmdbuflen, sizeof(cl->commandbuffer)-1 - cl->cmdbuflen, 0);
	if (ret == -1)
	{
		int e = neterrno();
		if (e == NET_EWOULDBLOCK)
			return false;	//remove

		if (e == NET_ECONNABORTED || e == NET_ECONNRESET)
			return true;

		Con_Printf ("NET_GetPacket: %s\n", strerror(e));
		return true;
	}
	if (*cl->messagebuffer)
	{
		if (send (cl->controlsock, cl->messagebuffer, strlen(cl->messagebuffer), 0) != -1)
			*cl->messagebuffer = '\0';	//YAY! It went!

	}

	if (ret == 0)
		return false;
	cl->cmdbuflen += ret;
	cl->commandbuffer[cl->cmdbuflen] = 0;

	line = cl->commandbuffer;
	while (1)
	{
		msg = line;
		while (*line)
		{
			if (*line == '\r')
				*line = ' ';
			if (*line == '\n')
				break;
			line++;
		}
		if (!*line)	//broken client
		{
			memmove(cl->commandbuffer, line, strlen(line)+1);
			cl->cmdbuflen = strlen(line);
			break;
		}
		*line = '\0';
		line++;
		IWebPrintf("FTP: %s\n", msg);

		msg = COM_ParseOut(msg, mode, sizeof(mode));
		if (!stricmp(mode, "SYST"))
		{
			QueueMessage (cl, "215 UNIX Type: L8.\r\n");	//some browsers can be wierd about things.
		}
		else if (!stricmp(mode, "user"))
		{
			msg = COM_ParseOut(msg, cl->name, sizeof(cl->name));

			QueueMessage (cl, "331 User name received, will be checked with password.\r\n");
		}
		else if (!stricmp(mode, "pass"))
		{
			msg = COM_ParseOut(msg, cl->pwd, sizeof(cl->pwd));

			cl->auth = IWebAuthorize(cl->name, cl->pwd);

			if (cl->auth)
				QueueMessage (cl, "230 User logged in.\r\n");
			else
				QueueMessage (cl, "530 Username or Password was incorrect or otherwise invalid.\r\n");
		}
		else if (!stricmp(mode, "TYPE"))
		{
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			msg = COM_ParseOut(msg, resource, sizeof(resource));

			if (!stricmp(resource, "A"))	//ascii
			{
				QueueMessage (cl, "200 ascii selected.\r\n");
			}
			else if (!stricmp(resource, "I"))	//binary
			{
				QueueMessage (cl, "200 binary selected.\r\n");
			}
			else
			{
				QueueMessage (cl, "200 ascii selected.\r\n");
			}
		}
		else if (!stricmp(mode, "PWD"))
		{
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			QueueMessageva (cl, "257 \"%s\"\r\n", cl->path);
		}
		else if (!stricmp(mode, "CWD"))
		{
			char *p;
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			Q_strcpyline(cl->path, msg+1, sizeof(cl->path));//path starts after cmd and single space
			for (p = cl->path+strlen(cl->path)-1; *p == ' ' && p >= cl->path; p--)
				*p = '\0';
			QueueMessage (cl, "200 directory changed.\r\n");
		}
		else if (!stricmp(mode, "EPSV"))
		{
			int aftype = 0;
			//one argument, "1"=ipv4, "2"=ipv6. if not present, use same as control connection
			//reply: "229 Entering Extended Passive Mode (|||$PORTNUM|)\r\n"

			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			if (cl->datasock != INVALID_SOCKET)
			{
				closesocket(cl->datasock);
				cl->datasock = INVALID_SOCKET;
			}

			cl->datasock = FTP_BeginListening(aftype, 0);
			if (cl->datasock == INVALID_SOCKET)
				QueueMessage (cl, "425 server was unable to make a listen socket\r\n");
			else
			{
				FTP_SVSocketPortToString(cl->datasock, resource);
				QueueMessageva (cl, "229 Entering Extended Passive Mode (|||%s|).\r\n", resource);
			}
			cl->dataislisten = true;
		}
		else if (!stricmp(mode, "PASV"))
		{
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			if (cl->datasock != INVALID_SOCKET)
			{
				closesocket(cl->datasock);
				cl->datasock = INVALID_SOCKET;
			}

			cl->datasock = FTP_BeginListening(1, 0);
			if (cl->datasock == INVALID_SOCKET)
				QueueMessage (cl, "425 server was unable to make a listen socket\r\n");
			else
			{
				FTP_SVSocketToString(cl->datasock, resource);
				QueueMessageva (cl, "227 Entering Passive Mode (%s).\r\n", resource);
			}
			cl->dataislisten = true;
		}
//		else if (!stricmp(mode, "EPRT"))
//		{
			//eg: one of:
			//EPRT |1|132.235.1.2|6275|
			//EPRT |2|1080::8:800:200C:417A|5282|

			//reply: 522 Network protocol not supported, use (1,2)
//		}
		else if (!stricmp(mode, "PORT"))
		{
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			if (cl->datasock != INVALID_SOCKET)
			{
				closesocket(cl->datasock);
				cl->datasock = INVALID_SOCKET;
			}
			msg = COM_ParseOut(msg, resource, sizeof(resource));

			cl->dataislisten = false;

			if ((cl->datasock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
			{
				Sys_Error ("FTP_ServerThinkForConnection: socket: %s", strerror(neterrno()));
			}

			if (ioctlsocket (cl->datasock, FIONBIO, (u_long *)&_true) == -1)
			{
				Sys_Error ("FTP_ServerThinkForConnection: ioctl FIONBIO: %s", strerror(neterrno()));
			}

			from.sin_family = AF_INET;

			from.sin_addr.s_addr = INADDR_ANY;

			from.sin_port = 0;

			if( bind (cl->datasock, (void *)&from, sizeof(from)) == -1)
			{
				closesocket(cl->datasock);
				cl->datasock=INVALID_SOCKET;

				QueueMessage (cl, "425 server bind error.\r\n");
				continue;
			}


			fromlen = sizeof(from);
			FTP_StringToAdr(resource, (qbyte *)&from.sin_addr, (qbyte *)&from.sin_port);
			connect(cl->datasock, (struct sockaddr *)&from, fromlen);

			QueueMessage (cl, "200 Opened data channel.\r\n");
		}
		else if (!stricmp(mode, "LIST"))
		{
			char buffer[256];
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			if (cl->dataislisten)	//accept a connect.
			{
				int err;
				int _true = true;
				int temp;
				struct sockaddr_qstorage adr;
				int adrlen = sizeof(adr);
				temp = accept(cl->datasock, (struct sockaddr *)&adr, &adrlen);
				err = neterrno();
				closesocket(cl->datasock);
				cl->datasock = temp;
				cl->dataislisten = false;

				if (cl->datasock == INVALID_SOCKET)
				{
					QueueMessageva (cl, "425 Can't accept pasv data connection - %i.\r\n", err);
					continue;
				}
				else
					ioctlsocket(cl->datasock, FIONBIO, (u_long *)&_true);
			}
			if (cl->datasock == INVALID_SOCKET)
			{
				QueueMessage (cl, "503 Bad sequence of commands.\r\n");
				continue;
			}
			if (*cl->path == '/')
				strcpy(buffer, cl->path+1);
			else
				strcpy(buffer, cl->path);

			if (*buffer)	//last character should be a /
				if (buffer[strlen(buffer)-1] != '/')
					strcat(buffer, "/");

			strcat(buffer, "*");
			QueueMessage (cl, "125 Opening FAKE ASCII mode data connection for file.\r\n");

			COM_EnumerateFiles(buffer, SendFileNameTo, &cl->datasock);

			QueueMessage (cl, "226 Transfer complete.\r\n");

			closesocket(cl->datasock);
			cl->datasock = INVALID_SOCKET;
		}
//		else if (!stricmp(mode, "SIZE"))	//why IE can't use the list command to find file length, I've no idea.
//		{
//			msg = COM_ParseOut(msg, resource, sizeof(resource));
//		}
		else if (!stricmp(mode, "RETR"))
		{
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			if (cl->dataislisten)	//accept a connect.
			{
				int _true = true;
				int temp;
				struct sockaddr_in adr;
				int adrlen = sizeof(adr);
				temp = accept(cl->datasock, (struct sockaddr *)&adr, &adrlen);
				closesocket(cl->datasock);
				cl->datasock = temp;
				cl->dataislisten = false;

				if (cl->datasock == INVALID_SOCKET)
				{
					QueueMessageva (cl, "425 Can't accept pasv data connection - %i.\r\n", neterrno());
					continue;
				}
				else
					ioctlsocket(cl->datasock, FIONBIO, (u_long *)&_true);
			}
			if (cl->datasock == INVALID_SOCKET)
			{
				QueueMessage (cl, "503 Bad sequence of commands.\r\n");
				continue;
			}
			msg = COM_ParseOut(msg, resource, sizeof(resource));

			if (!cl->auth & IWEBACC_READ)
			{
				QueueMessage (cl, "550 No read access.\r\n");
				continue;
			}

			if (!(*resource == '/'))
			{
				int plen = strlen(cl->path);
				if (plen && cl->path[plen-1] != '/')
				{
					memmove(resource+plen+1, resource, strlen(resource)+1);
					memcpy(resource, cl->path, plen);
					resource[plen] = '/';
				}
				else
				{
					memmove(resource+plen, resource, strlen(resource)+1);
					memcpy(resource, cl->path, plen);
				}
			}
			if (*resource == '/')
			{
				if (SV_AllowDownload(resource+1))
					cl->file = FS_OpenVFS(resource+1, "rb", FS_GAME);
				else
					cl->file = IWebGenerateFile(resource+1, NULL, 0);
			}
			else
			{
				if (SV_AllowDownload(resource))
					cl->file = FS_OpenVFS(resource, "rb", FS_GAME);
				else
					cl->file = IWebGenerateFile(resource, NULL, 0);
			}

			if (!cl->file)
			{
				QueueMessage (cl, "550 File not found.\r\n");
			}
			else
			{	//send data
				QueueMessage (cl, "125 Opening BINARY mode data connection for file.\r\n");

				cl->datadir = 1;
			}
		}
		else if (!stricmp(mode, "STOR"))
		{
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			Q_strcpyline(mode, msg+1, sizeof(mode));


			if (!(cl->auth & IWEBACC_FULL) && (((cl->auth & IWEBACC_WRITE && !IWebAllowUpLoad(cl->path+1, cl->name)) || !(cl->auth & IWEBACC_WRITE))))
			{
				QueueMessage (cl, "550 Permission denied.\r\n");
			}
			else
			{
				if (cl->dataislisten)	//accept a connect.
				{
					int _true = true;
					int temp;
					struct sockaddr_in adr;
					int adrlen = sizeof(adr);
					temp = accept(cl->datasock, (struct sockaddr *)&adr, &adrlen);
					closesocket(cl->datasock);
					cl->datasock = temp;
					cl->dataislisten = false;

					if (cl->datasock == INVALID_SOCKET)
					{
						QueueMessageva (cl, "425 Can't accept pasv data connection - %i.\r\n", neterrno());
						continue;
					}
					else
						ioctlsocket(cl->datasock, FIONBIO, (u_long *)&_true);
				}
				if (cl->datasock == INVALID_SOCKET)
				{
					QueueMessage (cl, "502 Bad sequence of commands.\r\n");
					continue;
				}
//				msg = COM_ParseOut(msg, mode, sizeof(mode));

				if (*mode == '/')
					sprintf(resource, "%s%s", cl->path, mode);
				else
					sprintf(resource, "%s%s", cl->path, mode);

				cl->file = FS_OpenVFS(resource, "rb", FS_GAMEONLY);
				if (cl->file)
				{
					VFS_CLOSE(cl->file);
					QueueMessage (cl, "550 File already exists.\r\n");
					continue;
				}
				cl->file = FS_OpenVFS(resource, "wb", FS_GAME);

				if (!cl->file)
				{
					QueueMessage (cl, "550 Couldn't open output.\r\n");
				}
				else
				{	//send data
					QueueMessage (cl, "125 Opening BINARY mode data connection for input.\r\n");

					cl->datadir = 2;
				}
			}
		}
		else if (!stricmp(mode, "STRU"))
		{
			if (!cl->auth)
			{
				QueueMessage (cl, "530 Not logged in.\r\n");
				continue;
			}
			msg = COM_ParseOut(msg, resource, sizeof(resource));
			if (!strcmp(resource, "F"))
			{
				QueueMessage (cl, "200 recordless structure selected.\r\n");
			}
			else
			{
				QueueMessage (cl, "504 not implemented (it's a simple server).\r\n");
			}
		}
		else if (!stricmp(mode, "NOOP"))
		{
			QueueMessage (cl, "200 Do something then!\r\n");
		}
		else if (!stricmp(mode, "QUIT"))
		{
			QueueMessage (cl, "200 About to quit.\r\n");
			return true;
		}
		else
		{
			QueueMessage (cl, "502 Command not implemented.\r\n");
		}
	}
	return false;
}

#if defined(WEBSVONLY) && defined(_WIN32)
unsigned int WINAPI BlockingClient(FTPclient_t *cl)
{
	unsigned long _false = false;
	if (ioctlsocket (cl->controlsock, FIONBIO, &_false) == -1)
	{
		IWebPrintf ("FTP_ServerRun: blocking error: %s\n", strerror(neterrno()));
		return 0;
	}

	cl->blocking = true;

	while (!FTP_ServerThinkForConnection(cl))
	{
		Sleep(10);
	}

	if (cl->file)
		VFS_CLOSE(cl->file);
	closesocket(cl->controlsock);
	if (cl->datasock)
		closesocket(cl->datasock);

	IWebFree(cl);
	return 0;
}
#endif

iwboolean FTP_ServerRun(iwboolean ftpserverwanted, int port)
{
	FTPclient_t *cl, *prevcl;
	struct sockaddr_qstorage	from;
	int		fromlen;
	SOCKET clientsock;
unsigned long _true = true;

	if (!ftpserverinitied)
	{
		if (ftpserverwanted)
		{
			ftpserversocket = FTP_BeginListening(0, port);
			if (ftpserversocket == INVALID_SOCKET)
			{
				ftpserverfailed = true;
				IWebPrintf("Unable to establish listening FTP socket\n");
			}
			ftpserverinitied = true;
		}
		return false;
	}
	else if (!ftpserverwanted)
	{
		FTP_ServerShutdown();
		return false;
	}

	prevcl = NULL;
	for (cl = FTPclient; cl; cl = cl->next)
	{
		if (FTP_ServerThinkForConnection(cl))
		{
			if (cl->file)
				VFS_CLOSE(cl->file);
			closesocket(cl->controlsock);
			if (cl->datasock)
				closesocket(cl->datasock);

			if (prevcl)
			{
				prevcl->next = cl->next;
				IWebFree(cl);
				cl = prevcl;

				if (!cl)	//kills loop
					break;
			}
			else
			{
				FTPclient = cl->next;
				IWebFree(cl);
				cl = FTPclient;

				if (!cl)	//kills loop
					break;
			}
		}
		prevcl = cl;
	}

	fromlen = sizeof(from);
	if (ftpserversocket == INVALID_SOCKET)
		clientsock = INVALID_SOCKET;
	else
		clientsock = accept(ftpserversocket, (struct sockaddr *)&from, &fromlen);

	if (clientsock == INVALID_SOCKET)
	{
		int e = neterrno();
		if (e == NET_EWOULDBLOCK)
			return false;

		if (e == NET_ECONNABORTED || e == NET_ECONNRESET)
		{
			Con_TPrintf ("Connection lost or aborted\n");
			return false;
		}


		Con_Printf ("NET_GetPacket: %s\n", strerror(e));
		return false;
	}

	if (ioctlsocket (clientsock, FIONBIO, &_true) == -1)
	{
		IWebPrintf ("FTP_ServerRun: blocking error: %s\n", strerror(neterrno()));
		return false;
	}
	cl = IWebMalloc(sizeof(FTPclient_t));
	if (!cl)	//iwebmalloc is allowed to fail.
	{
		char *msg = "421 Not enough memory is allocated.\r\n";	//don't be totally anti social
		send(clientsock, msg, strlen(msg), 0);
		closesocket(clientsock);	//try to forget this ever happend
		return true;
	}
	{
		char resource[256];
		FTP_SVRemoteSocketToString(clientsock, resource, sizeof(resource));
		IWebPrintf("FTP connect from %s\n", resource);
	}
	cl->controlsock = clientsock;
	cl->datasock = INVALID_SOCKET;
	cl->next = FTPclient;
	cl->blocking = false;
	strcpy(cl->path, "/");

	QueueMessage(cl, "220-" FULLENGINENAME " FTP Server.\r\n220 Welcomes all new users.\r\n");

#if defined(WEBSVONLY) && defined(_WIN32)
	if (!CreateThread(NULL, 128, BlockingClient, cl, 0, NULL))
#endif
		FTPclient = cl;
	return true;
}

#endif
