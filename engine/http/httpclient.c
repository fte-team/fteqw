#include "quakedef.h"

#ifdef WEBCLIENT

#include "iweb.h"

#include "netinc.h"

/*
This file does one thing. Connects to servers and grabs the specified file. It doesn't do any uploading whatsoever. Live with it.
It doesn't use persistant connections.

*/

qboolean HTTP_CL_Get(char *url, char *localfile, void (*NotifyFunction)(char *localfile, qboolean sucess));

typedef struct http_con_s {
	int sock;

	enum {HC_REQUESTING,HC_GETTINGHEADER, HC_GETTING} state;

	char *buffer;
	char filename[MAX_QPATH];
	int bufferused;
	int bufferlen;

	int totalreceived;	//useful when we're just dumping to a file.

	qboolean chunking;
	int chunksize;
	int chunked;

	int contentlength;

	vfsfile_t *file;

	void (*NotifyFunction)(char *localfile, qboolean sucess);	//called when failed or succeeded, and only if it got a connection in the first place.
	struct http_con_s *next;
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
				{
					if (HTTP_CL_Get(Location, con->filename, con->NotifyFunction))
						con->NotifyFunction = NULL;
				}

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

			con->file = FS_OpenVFS(con->filename, "wb", FS_GAME);
			if (!con->file)
			{
				Con_Printf("HTTP: Couldn't open file %s\n", con->filename);
				return false;
			}

			if (!con->file)
			{
				VFS_WRITE(con->file, con->buffer+ammount, con->bufferused);
				con->bufferused = 0;
			}
			else
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


			con->totalreceived+=con->chunked;
			if (con->file && con->chunked)	//we've got a chunk in the buffer
			{	//write it
				if (VFS_WRITE(con->file, con->buffer, con->chunked) != con->chunked)
				{
					Con_Printf("Write error whilst downloading %s\nDisk full?\n", con->filename);
					return false;
				}

				//and move the unparsed chunk to the front.
				con->bufferused -= con->chunked;
				memmove(con->buffer, con->buffer+con->chunked, con->bufferused);
				con->chunked = 0;
			}
		}
		else
		{
			con->totalreceived+=ammount;
			if (con->file)	//we've got a chunk in the buffer
			{	//write it
				if (VFS_WRITE(con->file, con->buffer, con->bufferused) != con->bufferused)
				{
					Con_Printf("Write error whilst downloading %s\nDisk full?\n", con->filename);
					return false;
				}
				con->bufferused = 0;
			}
		}

		if (!ammount)
		{	//server closed off the connection.
			if (con->chunksize)
				Con_Printf("Download was part way through chunking - must be corrupt - %s\n", con->filename);
			else if (con->bufferused != con->contentlength && !con->file)
				Con_Printf("Recieved file isn't the correct length - must be corrupt - %s\n", con->filename);
			Con_Printf("Retrieved %s\n", con->filename);
			if (con->file)
				VFS_CLOSE(con->file);
			else
			{
				FS_WriteFile(con->filename, con->buffer, con->bufferused, FS_GAME);
			}
			if (con->NotifyFunction)
			{
				con->NotifyFunction(con->filename, true);
				con->NotifyFunction = NULL;
			}
			return false;
		}

		break;
	}

	return true;
}

qboolean HTTP_CL_SingleThink(http_con_t *con)
{
	if (!HTTP_CL_Run(con))
	{
		if (con->NotifyFunction)
			con->NotifyFunction(con->filename, false);

		if (cls.downloadmethod == DL_HTTP)
			cls.downloadmethod = DL_NONE;
		closesocket(con->sock);

		if (con->buffer)
			IWebFree(con->buffer);
		IWebFree(con);

		//I don't fancy fixing this up.
		return false;
	}

	return true;
}

void HTTP_CL_Think(void)
{
	http_con_t *con = httpcl;
	http_con_t *prev = NULL;
	http_con_t *oldnext;

	while (con)
	{
		oldnext = con->next;

		if (!HTTP_CL_SingleThink(con))
		{
			if (prev)
				prev->next = oldnext;
			else
				httpcl = oldnext;

			return;
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
			strcpy(cls.downloadlocalname, con->filename);
			strcpy(cls.downloadremotename, con->filename);
		}
		else if (cls.downloadmethod == DL_HTTP)
		{
			if (!strcmp(cls.downloadlocalname, con->filename))
			{
				if (con->state != HC_GETTING)
					cls.downloadpercent = 0;
				else if (con->contentlength <= 0)
					cls.downloadpercent = 50;
				else
					cls.downloadpercent = con->totalreceived*100.0f/con->contentlength;
			}
		}

		prev = con;
		con = con->next;
	}
}

//returns true if we start downloading it
//returns false if we couldn't connect
//note that this return value is actually pretty useless
//the NotifyFunction will only ever be called after this has returned true, and won't always suceed.
qboolean HTTP_CL_Get(char *url, char *localfile, void (*NotifyFunction)(char *localfile, qboolean sucess))
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

		return FTP_Client_Command(va("download %s \"%s\" \"%s\"", server, uri+1, localfile), NotifyFunction);
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
	if (connect(con->sock, (struct sockaddr *)&from, sizeof(struct sockaddr_in)) == -1)
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
	sprintf(con->buffer, "GET %s HTTP/1.1\r\n"	"Host: %s\r\n" "Connection: close\r\n"	"User-Agent: "FULLENGINENAME"\r\n" "\r\n", uri, server);
	con->bufferused = strlen(con->buffer);
	con->contentlength = -1;
	con->NotifyFunction = NotifyFunction;
	if (!NotifyFunction)
		Con_Printf("No NotifyFunction\n");
	Q_strncpyz(con->filename, localfile, sizeof(con->filename));

	slash = strchr(con->filename, '?');
	if (slash)
		*slash = '_';

	if (HTTP_CL_SingleThink(con))
	{
		con->next = httpcl;
		httpcl = con;
		return true;
	}

	return false;
}

#endif
