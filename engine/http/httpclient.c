#include "quakedef.h"

#include "iweb.h"

#include "netinc.h"

#ifdef WEBCLIENT

/*
This file does one thing. Connects to servers and grabs the specified file. It doesn't do any uploading whatsoever. Live with it.
It doesn't use persistant connections.

*/

#if 0

typedef struct http_con_s {
	int sock;

	enum {HC_REQUESTING,HC_GETTINGHEADER, HC_GETTING} state;

	char *buffer;

	char filename[MAX_QPATH];
	vfsfile_t *file;

	int bufferused;
	int bufferlen;

	int totalreceived;	//useful when we're just dumping to a file.

	qboolean chunking;
	int chunksize;
	int chunked;

	int contentlength;

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

			if (!con->file)
			{
				con->file = FS_OpenVFS(con->filename, "wb", FS_GAME);
				if (!con->file)
				{
					Con_Printf("HTTP: Couldn't open file %s\n", con->filename);
					return false;
				}
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
			if (!con->file && *con->filename)
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





















struct http_dl_ctx_s {
	struct dl_download *dlctx;

	SOCKET sock;

	char *buffer;

	int bufferused;
	int bufferlen;

	int totalreceived;	//useful when we're just dumping to a file.

	qboolean chunking;
	int chunksize;
	int chunked;

	enum {HC_REQUESTING,HC_GETTINGHEADER, HC_GETTING} state;

	int contentlength;
};

void HTTP_Cleanup(struct dl_download *dl)
{
	struct http_dl_ctx_s *con = dl->ctx;
	dl->ctx = NULL;

	if (con->sock != INVALID_SOCKET)
		closesocket(con->sock);
	con->sock = INVALID_SOCKET;
	free(con->buffer);
	free(con);

	dl->status = DL_PENDING;
	dl->completed = 0;
	dl->totalsize = 0;
}

static void ExpandBuffer(struct http_dl_ctx_s *con, int quant)
{
	int newlen;
	newlen = con->bufferlen + quant;
	con->buffer = realloc(con->buffer, newlen);
	con->bufferlen = newlen;
}

static qboolean HTTP_DL_Work(struct dl_download *dl)
{
	struct http_dl_ctx_s *con = dl->ctx;
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
					HTTP_Cleanup(dl);
					Q_strncpyz(dl->redir, Location, sizeof(dl->redir));
				}
				return true;
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

			if (!dl->file)
			{
				dl->file = FS_OpenVFS(dl->localname, "wb", FS_GAME);
				if (!dl->file)
				{
					Con_Printf("HTTP: Couldn't open file %s\n", dl->localname);
					return false;
				}
			}

			if (!dl->file)
			{
				VFS_WRITE(dl->file, con->buffer+ammount, con->bufferused);
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
			if (dl->file && con->chunked)	//we've got a chunk in the buffer
			{	//write it
				if (VFS_WRITE(dl->file, con->buffer, con->chunked) != con->chunked)
				{
					Con_Printf("Write error whilst downloading %s\nDisk full?\n", dl->localname);
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
			if (dl->file)	//we've got a chunk in the buffer
			{	//write it
				if (VFS_WRITE(dl->file, con->buffer, con->bufferused) != con->bufferused)
				{
					Con_Printf("Write error whilst downloading %s\nDisk full?\n", dl->localname);
					return false;
				}
				con->bufferused = 0;
			}
		}

		if (!ammount)
		{	//server closed off the connection.
			if (con->chunksize)
				dl->status = DL_FAILED;
			else
				dl->status = DL_FINISHED; 
			return false;
		}

		break;
	}

	return true;
}

void HTTPDL_Establish(struct dl_download *dl)
{
	unsigned long _true = true;
	struct sockaddr_qstorage	from;
	struct http_dl_ctx_s *con;

	char server[128];
	char uri[MAX_OSPATH];
	char *slash;
	const char *url = dl->redir;
	if (!*url)
		url = dl->url;

	if (!strnicmp(url, "http://", 7))
		url+=7;

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

	con = malloc(sizeof(*con));
	memset(con, 0, sizeof(*con));
	dl->ctx = con;

	if ((con->sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		dl->status = DL_FAILED;
		return;
	}

	dl->status = DL_RESOLVING;
	{//quake routines using dns and stuff (Really, I wanna keep quake and ftp fairly seperate)
		netadr_t qaddy;		
		if (!NET_StringToAdr (server, &qaddy))
		{
			dl->status = DL_FAILED;
			return;
		}
		if (!qaddy.port)
			qaddy.port = htons(80);
		NetadrToSockadr(&qaddy, &from);
	}//end of quake.

	dl->status = DL_QUERY;

	//not yet blocking.
	if (connect(con->sock, (struct sockaddr *)&from, sizeof(struct sockaddr_in)) == -1)
	{
		dl->status = DL_FAILED;
		return;
	}

	if (ioctlsocket (con->sock, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		dl->status = DL_FAILED;
		return;
	}

	ExpandBuffer(con, 2048);
	sprintf(con->buffer, "GET %s HTTP/1.1\r\n"	"Host: %s\r\n" "Connection: close\r\n"	"User-Agent: "FULLENGINENAME"\r\n" "\r\n", uri, server);
	con->bufferused = strlen(con->buffer);
	con->contentlength = -1;

	if (!*dl->localname)
	{
		Q_strncpyz(dl->localname, url+1, sizeof(dl->localname));

		slash = strchr(dl->localname, '?');
		if (slash)
			*slash = '_';
	}
}

qboolean HTTPDL_Poll(struct dl_download *dl)
{
	/*failed previously*/
	if (dl->status == DL_FAILED)
		return false;

	if (!dl->ctx)
	{
		HTTPDL_Establish(dl);
		if (dl->status == DL_FAILED)
		{
			HTTP_Cleanup(dl);
			return false;
		}
	}

	if (dl->ctx)
	{
		HTTP_DL_Work(dl);
		if (dl->status == DL_FAILED)
		{
			HTTP_Cleanup(dl);
			return false;
		}
		if (dl->status == DL_FINISHED)
			return false;
	}

	return true;
}

qboolean HTTPDL_Decide(struct dl_download *dl)
{
	const char *url = dl->redir;
	if (!*url)
		url = dl->url;

	if (!strnicmp(url, "http://", 7))
		dl->poll = HTTPDL_Poll;
	else
	{
		dl->status = DL_FAILED;
		return false;
	}
	return true;
}

#ifdef MULTITHREAD
static int DL_Thread_Work(void *arg)
{
	struct dl_download *dl = arg;

	while (!dl->threaddie)
	{
		if (!dl->poll(dl))
		{
			if (dl->notify)
				dl->notify(dl);
			if (dl->file)
				VFS_CLOSE(dl->file);
			break;
		}
	}
	return 0;
}

/*create a thread to perform the given download
to use: call DL_Create (not HTTP_CL_Get!) to get a context, then call this.
note that you need to call DL_Close from another thread, NOT IN THE NOTIFY FUNC.
the file handle must be safe to write to in threads.
*/
qboolean DL_CreateThread(struct dl_download *dl, vfsfile_t *file, void (*NotifyFunction)(struct dl_download *dl))
{
	if (!dl)
		return false;

	dl->file = file;
	dl->notify = NotifyFunction;

	dl->threadctx = Sys_CreateThread(DL_Thread_Work, dl, 0);
	if (!dl->threadctx)
		return false;

	return true;
}
#endif

/*create a standalone download context*/
struct dl_download *DL_Create(const char *url)
{
	struct dl_download *newdl;
	newdl = malloc(sizeof(*newdl));
	if (!newdl)
		return NULL;
	memset(newdl, 0, sizeof(*newdl));
	Q_strncpyz(newdl->url, url, sizeof(newdl->url));
	newdl->poll = HTTPDL_Decide;

	return newdl;
}
/*destroys an entire download context*/
void DL_Close(struct dl_download *dl)
{
#ifdef MULTITHREAD
	dl->threaddie = true;
	if (dl->threadctx)
		Sys_WaitOnThread(dl->threadctx);
#endif
	if (dl->abort)
		dl->abort(dl);
	if (dl->file)
		VFS_CLOSE(dl->file);
	free(dl);
}


static struct dl_download *activedownloads;
/*create a download context and add it to the list, for lazy people*/
struct dl_download *HTTP_CL_Get(const char *url, const char *localfile, void (*NotifyFunction)(struct dl_download *dl))
{
	struct dl_download *newdl = DL_Create(url);
	if (!newdl)
		return newdl;

	newdl->notify = NotifyFunction;
	Q_strncpyz(newdl->localname, localfile, sizeof(newdl->localname));

	newdl->next = activedownloads;
	activedownloads = newdl;
	return newdl;
}

/*updates pending downloads*/
void HTTP_CL_Think(void)
{
	struct dl_download *con = activedownloads;
	struct dl_download **link = NULL;

	link = &activedownloads;
	while (*link)
	{
		con = *link;
		if (!con->poll(con))
		{
			if (con->notify)
				con->notify(con);
			*link = con->next;
			DL_Close(con);
			continue;
		}
		link = &con->next;

		if (!cls.downloadmethod)
		{
			cls.downloadmethod = DL_HTTP;
			strcpy(cls.downloadlocalname, con->localname);
			strcpy(cls.downloadremotename, con->localname);
		}
		if (cls.downloadmethod == DL_HTTP)
		{
			if (!strcmp(cls.downloadlocalname, con->localname))
			{
				if (con->status == DL_FINISHED)
					cls.downloadpercent = 100;
				else if (con->status != DL_ACTIVE)
					cls.downloadpercent = 0;
				else if (con->totalsize <= 0)
					cls.downloadpercent = 50;
				else
					cls.downloadpercent = con->completed*100.0f/con->totalsize;
			}
		}
	}
}
#endif	/*WEBCLIENT*/
