#include "quakedef.h"

#include "iweb.h"

#include "netinc.h"

#ifdef WEBCLIENT

qboolean HTTPDL_Decide(struct dl_download *dl);

/*
This file does one thing. Connects to servers and grabs the specified file. It doesn't do any uploading whatsoever. Live with it.
It doesn't use persistant connections.

*/

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
			dl->status = DL_ACTIVE;
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
					if (*Location == '/')
					{
						char *cur = *dl->redir?dl->redir:dl->url;
						char *curserver = cur;
						char *curpath;
						/*same server+protocol*/
						if (!strncmp(curserver, "http://", 7))
							curserver += 7;
						curpath = strchr(curserver, '/');
						if (!curpath)
							curpath = curserver + strlen(curserver);
						if (cur == dl->redir)
							*curpath = 0;
						else
							Q_strncpyz(dl->redir, cur, (curpath-cur) + 1);
						Q_strncatz(dl->redir, Location, sizeof(dl->redir));
					}
					else
						Q_strncpyz(dl->redir, Location, sizeof(dl->redir));
					dl->poll = HTTPDL_Decide;
					dl->status = DL_PENDING;
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

			dl->totalsize = con->contentlength;

			if (!dl->file)
			{
				if (*dl->localname)
				{
					FS_CreatePath(dl->localname, FS_GAME);
					dl->file = FS_OpenVFS(dl->localname, "wb", FS_GAME);
				}
				else
					dl->file = FS_OpenTemp();
				if (!dl->file)
				{
					Con_Printf("HTTP: Couldn't open file %s\n", dl->localname);
					dl->status = DL_FAILED;
					return false;
				}
			}

			memmove(con->buffer, con->buffer+ammount, con->bufferused);


			con->state = HC_GETTING;
			dl->status = DL_ACTIVE;

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
		dl->completed = con->totalreceived;

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
			dl->status = DL_FAILED;
			return false;
		}
	}

	if (dl->ctx)
	{
		if (!HTTP_DL_Work(dl))
			if (dl->status != DL_FINISHED)
				dl->status = DL_FAILED;
		if (dl->status == DL_FAILED)
		{
			HTTP_Cleanup(dl);
			dl->status = DL_FAILED;
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
static struct dl_download *showndownload;
/*create a download context and add it to the list, for lazy people*/
struct dl_download *HTTP_CL_Get(const char *url, const char *localfile, void (*NotifyFunction)(struct dl_download *dl))
{
	struct dl_download *newdl = DL_Create(url);
	if (!newdl)
		return newdl;

	newdl->notify = NotifyFunction;
	if (localfile)
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
			if (con->file)
				VFS_SEEK(con->file, 0);
			if (con->notify)
				con->notify(con);
			*link = con->next;
			DL_Close(con);

			if (cls.downloadmethod == DL_HTTP)
			{
				if (showndownload == con)
				{
					cls.downloadmethod = DL_NONE;
					*cls.downloadlocalname = *cls.downloadremotename = 0;
				}
			}
			continue;
		}
		link = &con->next;

		if (!cls.downloadmethod)
		{
			cls.downloadmethod = DL_HTTP;
			showndownload = con;
			strcpy(cls.downloadlocalname, con->localname);
			strcpy(cls.downloadremotename, con->url);
		}
		if (cls.downloadmethod == DL_HTTP)
		{
			if (showndownload == con)
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
