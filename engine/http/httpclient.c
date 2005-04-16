#include "quakedef.h"

#ifdef WEBCLIENT

#include "iweb.h"

#ifdef _WIN32

//msvc crap

#define EWOULDBLOCK	WSAEWOULDBLOCK
#define EMSGSIZE	WSAEMSGSIZE
#define ECONNRESET	WSAECONNRESET
#define ECONNABORTED	WSAECONNABORTED
#define ECONNREFUSED	WSAECONNREFUSED
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL

#define snprintf _snprintf

#define qerrno WSAGetLastError()
#else

//gcc stuff

#define qerrno errno

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

#ifdef sun
#include <sys/filio.h>
#endif

#ifdef NeXT
#include <libc.h>
#endif

#define closesocket close
#define ioctlsocket ioctl
#endif


/*
Test files/servers.:

http://mywebpages.comcast.net/jsgeneric/prog5.asm
http://mywebpages.comcast.net/jsgeneric/sshot001.jpg
http://spike.corecodec.org/ftemqwtest.zip
http://www.fuhquake.net/files/releases/v0.31/fuhquake-win32-v0.31.zip
http://download.microsoft.com/download/d/c/3/dc37439a-172b-4f20-beac-bab52cdd38bc/Windows-KB833330-ENU.exe
*/



/*
This file does one thing. Connects to servers and grabs the specified file. It doesn't do any uploading whatsoever. Live with it.
It doesn't use persistant connections.

*/

qboolean HTTP_CL_Get(char *url, char *localfile);

typedef struct {
	int sock;

	enum {HC_REQUESTING,HC_GETTINGHEADER, HC_GETTING} state;

	char *buffer;
	char filename[MAX_QPATH];
	int bufferused;
	int bufferlen;

	qboolean chunking;
	int chunksize;
	int chunked;

	int contentlength;

} http_con_t;

static http_con_t *httpcl;

static void ExpandBuffer(http_con_t *con, int quant)
{
	int newlen;
	newlen = con->bufferlen + quant;
	con->buffer = IWebRealloc(con->buffer, newlen);
	con->bufferlen = newlen;
}

static qboolean HTTP_CL_Run(http_con_t *con)
{
	char buffer[256];
	char Location[256];
	char *nl;
	char *msg;
	int ammount;
	switch(con->state)
	{
	case HC_REQUESTING:
		ammount = send(con->sock, con->buffer, con->bufferused, 0);
		if (!ammount)
			return false;

		if (ammount < 0)
		{
			if (qerrno != EWOULDBLOCK)
				return false;
			return true;
		}

		con->bufferused -= ammount;
		memmove(con->buffer, con->buffer+ammount, con->bufferused);
		if (!con->bufferused)	//that's it, all sent.
			con->state = HC_GETTINGHEADER;
		break;

	case HC_GETTINGHEADER:
		if (con->bufferlen - con->bufferused < 1530)
			ExpandBuffer(con, 1530);

		ammount = recv(con->sock, con->buffer+con->bufferused, con->bufferlen-con->bufferused-15, 0);
		if (!ammount)
			return false;
		if (ammount < 0)
		{
			if (qerrno != EWOULDBLOCK)
				return false;
			return true;
		}

		con->bufferused+=ammount;
		con->buffer[con->bufferused] = '\0';
		//have we got the entire thing yet?

		msg = con->buffer;
		con->chunking = false;
		if (strnicmp(msg, "HTTP/", 5))
		{	//pre version 1. (lame servers.
			con->state = HC_GETTING;
			con->contentlength = -1;	//meaning end of stream.
		}
		else
		{
			while(*msg)
			{
				if (*msg == '\n')
				{
					if (msg[1] == '\n')
					{	//tut tut, not '\r'? that's not really allowed...
						msg+=1;
						break;
					}
					if (msg[2] == '\n')
					{
						msg+=2;
						break;
					}
				}
				msg++;
				if (!strnicmp(msg, "Content-Length: ", 16))
					con->contentlength = atoi(msg+16);
				else if (!strnicmp(msg, "Location: ", 10))
				{
					nl = strchr(msg, '\n');
					if (nl)
					{
						*nl = '\0';
						Q_strncpyz(Location, COM_TrimString(msg+10), sizeof(Location));
						*nl = '\n';
					}
				}
				else if (!strnicmp(msg, "Transfer-Encoding: ", 19))
				{
					char *chunk = strstr(msg, "chunked");
					nl = strchr(msg, '\n');
					if (nl)
						if (chunk < nl)
							con->chunking = true;
				}
			}
			if (!*msg)
				break;//switch
			msg++;

			ammount = msg - con->buffer;

			msg = COM_ParseOut(con->buffer, buffer, sizeof(buffer));
			msg = COM_ParseOut(msg, buffer, sizeof(buffer));
			if (!stricmp(buffer, "100"))
			{	//http/1.1 servers can give this. We ignore it.

				con->bufferused -= ammount;
				memmove(con->buffer, con->buffer+ammount, con->bufferused);
				return true;
			}

			if (!stricmp(buffer, "301") || !stricmp(buffer, "302") || !stricmp(buffer, "303"))
			{
				nl = strchr(msg, '\n');
				if (nl)
					*nl = '\0';
				Con_Printf("HTTP: %s %s\n", buffer, COM_TrimString(msg));
				if (!*Location)
					Con_Printf("Server redirected to null location\n");
				else
					HTTP_CL_Get(Location, con->filename);
				return false;
			}

			if (stricmp(buffer, "200"))
			{
				nl = strchr(msg, '\n');
				if (!nl)
					return false;	//eh?
				if (nl>msg&&nl[-1] == '\r')
					nl--;
				*nl = '\0';
				Con_Printf("HTTP: %s%s\n", buffer, msg);
				return false;	//something went wrong.
			}

			con->bufferused -= ammount;
			memmove(con->buffer, con->buffer+ammount, con->bufferused);

			con->state = HC_GETTING;

		}
		//Fall through

	case HC_GETTING:
		if (con->bufferlen - con->bufferused < 1530)
			ExpandBuffer(con, 1530);

		ammount = recv(con->sock, con->buffer+con->bufferused, con->bufferlen-con->bufferused-1, 0);
		if (ammount < 0)
		{
			if (qerrno != EWOULDBLOCK)
				return false;
			return true;
		}

		con->bufferused+=ammount;

		if (con->chunking)	//FIXME: NEEDS TESTING!!!
		{
			int trim;
			char *nl;
			con->buffer[con->bufferused] = '\0';
			for(;;)
			{	//work out as we go.
				if (con->chunksize)//we are trying to parse a chunk.
				{
					trim = con->bufferused - con->chunked;
					if (trim > con->chunksize)
						trim = con->chunksize;	//don't go into the next size field.
					con->chunksize -= trim;
					con->chunked += trim;

					if (!con->chunksize)
					{	//we need to find the next \n and trim it.
						nl = strchr(con->buffer+con->chunked, '\n');
						if (!nl)
							break;
						nl++;
						trim = nl - (con->buffer+con->chunked);
						memmove(con->buffer + con->chunked, nl, con->buffer+con->bufferused-nl+1);
						con->bufferused -= trim;
					}
					if (!(con->bufferused - con->chunked))
						break;
				}
				else
				{
					nl = strchr(con->buffer+con->chunked, '\n');
					if (!nl)
						break;
					con->chunksize = strtol(con->buffer+con->chunked, NULL, 16);	//it's hex.
					nl++;
					trim = nl - (con->buffer+con->chunked);
					memmove(con->buffer + con->chunked, nl, con->buffer+con->bufferused-nl+1);
					con->bufferused -= trim;
				}
			}
		}

		if (!ammount)
		{	//server closed off the connection.
			if (con->chunksize)
				Con_Printf("Download was part way through chunking - must be corrupt - %s\n", con->filename);
			else if (con->bufferused != con->contentlength)
				Con_Printf("Recieved file isn't the correct length - must be corrupt - %s\n", con->filename);
			Con_Printf("Retrieved %s\n", con->filename);
			snprintf(Location, sizeof(Location)-1, "%s/%s", com_gamedir, con->filename);
			COM_CreatePath(Location);
			COM_WriteFile(con->filename, con->buffer, con->bufferused);
			return false;
		}

		break;
	}

	return true;
}

void HTTP_CL_Think(void)
{
	http_con_t *con = httpcl;
	if (con)
	{
		if (!HTTP_CL_Run(con))
		{
			if (cls.downloadmethod == DL_HTTP)
				cls.downloadmethod = DL_NONE;
			closesocket(con->sock);
			if (con->buffer)
				IWebFree(con->buffer);
			IWebFree(con);
			if (con == httpcl)
			{
				httpcl = NULL;
				return;
			}
			con = NULL;
		}
		else if (!cls.downloadmethod)
		{
			cls.downloadmethod = DL_HTTP;
			if (con->state != HC_GETTING)
				cls.downloadpercent = 0;
			else if (con->contentlength <= 0)
				cls.downloadpercent = 50;
			else
				cls.downloadpercent = con->bufferused*100.0f/con->contentlength;
			strcpy(cls.downloadname, con->filename);
		}
		else if (cls.downloadmethod == DL_HTTP)
		{
			if (!strcmp(cls.downloadname, con->filename))
			{
				if (con->state != HC_GETTING)
					cls.downloadpercent = 0;
				else if (con->contentlength <= 0)
					cls.downloadpercent = 50;
				else
					cls.downloadpercent = con->bufferused*100.0f/con->contentlength;
			}
		}
	}
}

qboolean HTTP_CL_Get(char *url, char *localfile)
{
	unsigned long _true = true;
	struct sockaddr_qstorage	from;
	http_con_t *con;

	char server[128];
	char uri[MAX_OSPATH];
	char *slash;

	if (localfile)
		if (!*localfile)
			localfile = NULL;

	if (!strnicmp(url, "http://", 7))
		url+=7;
	else if (!strnicmp(url, "ftp://", 6))
	{
		url+=6;
		slash = strchr(url, '/');
		if (!slash)
		{
			Q_strncpyz(server, url, sizeof(server));
			Q_strncpyz(uri, "/", sizeof(uri));
		}
		else
		{
			Q_strncpyz(uri, slash, sizeof(uri));
			Q_strncpyz(server, url, sizeof(server));
			server[slash-url] = '\0';
		}

		if (!localfile)
			localfile = uri+1;

		FTP_Client_Command(va("download %s \"%s\" \"%s\"", server, uri+1, localfile));
		return true;
	}
	else
	{
		Con_Printf("Bad URL: %s\n", url);
		return false;
	}

	slash = strchr(url, '/');
	if (!slash)
	{
		Q_strncpyz(server, url, sizeof(server));
		Q_strncpyz(uri, "/", sizeof(uri));
	}
	else
	{
		Q_strncpyz(uri, slash, sizeof(uri));
		Q_strncpyz(server, url, sizeof(server));
		server[slash-url] = '\0';
	}

	if (!localfile)
		localfile = uri+1;

	con = IWebMalloc(sizeof(http_con_t));

	if ((con->sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		Sys_Error ("HTTPCL_TCP_OpenSocket: socket: %s\n", strerror(qerrno));
	}


	{//quake routines using dns and stuff (Really, I wanna keep quake and ftp fairly seperate)
		netadr_t qaddy;		
		if (!NET_StringToAdr (server, &qaddy))
		{
			IWebWarnPrintf ("HTTPCL_TCP_OpenSocket: Failed to resolve host: %s\n", server);
			closesocket(con->sock);		
			IWebFree(con);
			return false;
		}
		if (!qaddy.port)
			qaddy.port = htons(80);
		NetadrToSockadr(&qaddy, &from);
	}//end of quake.

	//not yet blocking.
	if (connect(con->sock, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		IWebWarnPrintf ("HTTPCL_TCP_OpenSocket: connect: %i %s\n", qerrno, strerror(qerrno));
		closesocket(con->sock);		
		IWebFree(con);
		return false;
	}
	
	if (ioctlsocket (con->sock, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		Sys_Error ("HTTPCL_TCP_OpenSocket: ioctl FIONBIO: %s\n", strerror(qerrno));
	}

	ExpandBuffer(con, 2048);
	sprintf(con->buffer, "GET %s HTTP/1.1\r\n"	"Host: %s\r\n" "Connection: close\r\n"	"User-Agent: FTE\r\n" "\r\n", uri, server);
	con->bufferused = strlen(con->buffer);
	con->contentlength = -1;
	strcpy(con->filename, localfile);

/*	slash = strchr(con->filename, '?');
	if (slash)
		*slash = '\0';*/

	httpcl = con;

	HTTP_CL_Think();

	return true;
}

#endif
