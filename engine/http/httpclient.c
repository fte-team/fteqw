#include "quakedef.h"

#include "iweb.h"

#include "netinc.h"

#if defined(WEBCLIENT)

#if defined(FTE_TARGET_WEB)
#include <emscripten/emscripten.h>

typedef struct 
{
	vfsfile_t funcs;
	unsigned long offset;
	unsigned long length;
	char data[];
} mfile_t;
static int VFSMEM_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	mfile_t *f = (mfile_t*)file;
	if (f->offset >= f->length)
		return -1;	//eof!
	if (bytestoread > f->length - f->offset)
		bytestoread = f->length - f->offset;	//eof!
	memcpy(buffer, &f->data[f->offset], bytestoread);
	f->offset += bytestoread;
	return bytestoread;
}
static int VFSMEM_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
{
	return 0;
}
static qboolean VFSMEM_Seek (struct vfsfile_s *file, unsigned long pos)
{
	mfile_t *f = (mfile_t*)file;
	f->offset = pos;
	return true;
}
static unsigned long VFSMEM_Tell (struct vfsfile_s *file)
{
	mfile_t *f = (mfile_t*)file;
	return f->offset;
}
static unsigned long VFSMEM_GetSize (struct vfsfile_s *file)
{
	mfile_t *f = (mfile_t*)file;
	return f->length;
}
static void VFSMEM_Close(vfsfile_t *file)
{
	free(file);
}
static void VFSMEM_Flush(struct vfsfile_s *file)
{
}
static vfsfile_t *VFSMEM_File(void *data, unsigned int datasize)
{
       /*create a file which is already unlinked*/
        mfile_t *f;
        f = malloc(sizeof(*f) + datasize);
        if (!f)
                return NULL;
	f->funcs.ReadBytes = VFSMEM_ReadBytes;
	f->funcs.WriteBytes = VFSMEM_WriteBytes;
	f->funcs.Seek = VFSMEM_Seek;
	f->funcs.Tell = VFSMEM_Tell;
	f->funcs.GetLen = VFSMEM_GetSize;
	f->funcs.Close = VFSMEM_Close;
	f->funcs.Flush = VFSMEM_Flush;
	f->offset = 0;
	f->length = datasize;
	memcpy(f->data, data, datasize);

	return &f->funcs;
}

static void DL_Abort(struct dl_download *dl)
{
	dl->ctx = NULL;
}
static void DL_OnLoad(void *c, void *data, int datasize)
{
	struct dl_download *dl = c;
	Con_Printf("download %p: success\n", dl);

	//make sure the file is 'open'.
	if (!dl->file)
	{
		if (*dl->localname)
		{
	Con_Printf("create file\n");
			FS_CreatePath(dl->localname, FS_GAME);
			dl->file = FS_OpenVFS(dl->localname, "w+b", FS_GAME);
		}
		else
		{
			//emscripten does not close the file. plus we seem to end up with infinite loops.
	Con_Printf("temp file\n");
			dl->file = VFSMEM_File(data, datasize);
		}
	}

	if (dl->file)
	{
		Con_Printf("writing to file\n");
		VFS_WRITE(dl->file, data, datasize);
	}

	dl->replycode = 200;
	dl->completed += datasize;
	dl->status = DL_FINISHED;
}
static void DL_OnError(void *c)
{
	struct dl_download *dl = c;
	Con_Printf("download %p: error\n", dl);

	dl->replycode = 404;	//we don't actually know. should we not do this?
	dl->status = DL_FAILED;
}

//this becomes a poll function. the main thread will call this once a frame or so.
qboolean HTTPDL_Decide(struct dl_download *dl)
{
	const char *url = dl->redir;
	if (!*url)
		url = dl->url;

	if (dl->ctx)
	{
		if (dl->status == DL_FAILED || dl->status == DL_FINISHED)
		{
			DL_Abort(dl);
			return false;	//safe to destroy it now
		}
	}
	else
	{
		dl->status = DL_ACTIVE;
		dl->abort = DL_Abort;
		dl->ctx = dl;

		Con_Printf("Sending %p request for %s\n", dl, url);
		emscripten_async_wget_data(url, dl, DL_OnLoad, DL_OnError);
	}

	return true;
}
#elif defined(NACL)
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/ppb_file_system.h>
#include <ppapi/c/ppb_file_ref.h>
#include <ppapi/c/ppb_url_request_info.h>
#include <ppapi/c/ppb_url_response_info.h>
#include <ppapi/c/pp_var.h>
#include <ppapi/c/ppb_var.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/ppb_url_loader.h>

extern PPB_Core *ppb_core;
extern PPB_URLRequestInfo *urlrequestinfo;
extern PPB_URLLoader  *urlloader;
extern PP_Instance pp_instance;
extern PPB_Var *ppb_var_interface;

struct nacl_dl {
	char buffer[65536];
	PP_Resource req;
};

static void readfinished(void* user_data, int32_t result)
{
	struct dl_download *f = user_data;
	struct nacl_dl *ctx = f->ctx;
	struct PP_CompletionCallback ccb = {readfinished, f, PP_COMPLETIONCALLBACK_FLAG_NONE};

	//trying to clean up
	if (!ctx)
		return;

//	Sys_Printf("lastresult: %i\n", result);

	if (result == PP_OK)
	{
//		Sys_Printf("%s completed\n", f->url);
		ppb_core->ReleaseResource(ctx->req);
		ctx->req = 0;

		f->status = DL_FINISHED;
		return;
	}

	for (; result > 0; result = urlloader->ReadResponseBody(ctx->req, ctx->buffer, sizeof(ctx->buffer), ccb))
	{
		//make sure the file is 'open'.
		if (!f->file)
		{
			if (*f->localname)
			{
				FS_CreatePath(f->localname, FS_GAME);
				f->file = FS_OpenVFS(f->localname, "w+b", FS_GAME);
			}
			else
				f->file = FS_OpenTemp();
		}

//		Sys_Printf("write: %i\n", result);
		VFS_WRITE(f->file, ctx->buffer, result);

		f->completed += result;
	}

//	Sys_Printf("result: %i\n", result);
	if (result != PP_OK_COMPLETIONPENDING)
	{
		Sys_Printf("file %s failed or something\n", f->url);
		ppb_core->ReleaseResource(ctx->req);
		ctx->req = 0;
		f->status = DL_FAILED;
	}
}
//urloader->open completed
static void dlstarted(void* user_data, int32_t result)
{
	struct dl_download *f = user_data;
	struct nacl_dl *ctx = f->ctx;

	struct PP_CompletionCallback ccb = {readfinished, f, PP_COMPLETIONCALLBACK_FLAG_NONE};
	readfinished(user_data, urlloader->ReadResponseBody(ctx->req, ctx->buffer, sizeof(ctx->buffer), ccb));
}

static void nadl_cleanup(void* user_data, int32_t result)
{
	struct nacl_dl *ctx = user_data;

	if (ctx->req)
		ppb_core->ReleaseResource(ctx->req);
	free(ctx);
}

void NADL_Cleanup(struct dl_download *dl)
{
	struct nacl_dl *ctx = dl->ctx;

	//we can't free the ctx memory etc, in case the browser still has requests pending on it before it handles our close.
	//so set up a callback to do it later

	dl->ctx = NULL;	//orphan
	struct PP_CompletionCallback ccb = {nadl_cleanup, ctx, PP_COMPLETIONCALLBACK_FLAG_NONE};
	ppb_core->CallOnMainThread(1000, ccb, 0);
}

qboolean HTTPDL_Decide(struct dl_download *dl)
{
	const char *url = dl->redir;
	struct nacl_dl *ctx;
	if (!*url)
		url = dl->url;

	if (dl->ctx)
	{
		if (dl->status == DL_FAILED || dl->status == DL_FINISHED)
		{
			NADL_Cleanup(dl);
			return false;	//safe to destroy it now
		}
	}
	else
	{
		PP_Resource dlri;

		dl->status = DL_ACTIVE;
		dl->abort = NADL_Cleanup;
		dl->ctx = ctx = Z_Malloc(sizeof(*ctx));

		/*everything goes via nacl, so we might as well just init that here*/
		ctx->req = urlloader->Create(pp_instance);
		dlri = urlrequestinfo->Create(pp_instance);
		urlrequestinfo->SetProperty(dlri, PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, ppb_var_interface->VarFromUtf8(url, strlen(url)));
		urlrequestinfo->SetProperty(dlri, PP_URLREQUESTPROPERTY_URL, ppb_var_interface->VarFromUtf8(url, strlen(url)));

		struct PP_CompletionCallback ccb = {dlstarted, dl, PP_COMPLETIONCALLBACK_FLAG_NONE};
		urlloader->Open(ctx->req, dlri, ccb);
		ppb_core->ReleaseResource(dlri);
	}

	return true;
}
#else
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

	dl->abort = NULL;
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
			dl->replycode = 0;
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

			dl->replycode = atoi(buffer);

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
#ifndef NPFTE
				if (*dl->localname)
				{
					FS_CreatePath(dl->localname, FS_GAME);
					dl->file = FS_OpenVFS(dl->localname, "w+b", FS_GAME);
				}
				else
					dl->file = FS_OpenTemp();
#endif
				if (!dl->file)
				{
					Con_Printf("HTTP: Couldn't open file \"%s\"\n", dl->localname);
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
	struct sockaddr_qstorage	serveraddr;
	struct http_dl_ctx_s *con;
	int addressfamily;
	int addresssize;

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
	dl->abort = HTTP_Cleanup;

	dl->status = DL_RESOLVING;

	if (!NET_StringToSockaddr(server, 80, &serveraddr, &addressfamily, &addresssize))
	{
		dl->status = DL_FAILED;
		return;
	}

	dl->status = DL_QUERY;

	if ((con->sock = socket (addressfamily, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		dl->status = DL_FAILED;
		return;
	}

	//don't bother binding. its optional.

	//not yet blocking.
	if (connect(con->sock, (struct sockaddr *)&serveraddr, addresssize) == -1)
	{
		dl->status = DL_FAILED;
		return;
	}

	if (ioctlsocket (con->sock, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		dl->status = DL_FAILED;
		return;
	}

	ExpandBuffer(con, 512*1024);
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
#endif	/*!defined(NACL)*/

#ifdef MULTITHREAD
static int DL_Thread_Work(void *arg)
{
	struct dl_download *dl = arg;

	while (!dl->threaddie)
	{
		if (!dl->poll(dl))
		{
#ifdef NPFTE
			//the plugin doesn't have a download loop
			if (dl->notify)
				dl->notify(dl);
			if (dl->file)
				VFS_CLOSE(dl->file);
#else
			if (dl->status != DL_FAILED && dl->status != DL_FINISHED)
				dl->status = DL_FAILED;
#endif
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

	if (file)
		dl->file = file;
	if (NotifyFunction)
		dl->notify = NotifyFunction;

	dl->threadctx = Sys_CreateThread("download", DL_Thread_Work, dl, THREADP_NORMAL, 0);
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

	if (!newdl->poll(newdl))
	{
		free(newdl);
		newdl = NULL;
	}

	return newdl;
}
static struct dl_download *showndownload;

/*destroys an entire download context*/
void DL_Close(struct dl_download *dl)
{
#ifndef NPFTE
	if (showndownload == dl)
	{
		if (cls.downloadmethod == DL_HTTP)
		{
			cls.downloadmethod = DL_NONE;
			*cls.downloadlocalname = *cls.downloadremotename = 0;
		}
		showndownload = NULL;
	}
#endif

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


/*updates pending downloads*/
#ifndef NPFTE

static struct dl_download *activedownloads;
unsigned int shownbytestart;
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

void HTTP_CL_Think(void)
{
	struct dl_download *dl = activedownloads;
	struct dl_download **link = NULL;

	link = &activedownloads;
	while (*link)
	{
		dl = *link;
#ifdef MULTITHREAD
		if (dl->threadctx)
		{
			if (dl->status == DL_FINISHED || dl->status == DL_FAILED)
			{
				Sys_WaitOnThread(dl->threadctx);
				dl->threadctx = NULL;
				continue;
			}
		}
		else 
#endif
		if (!dl->poll(dl))
		{
			*link = dl->next;
			if (dl->file)
				VFS_SEEK(dl->file, 0);
			if (dl->notify)
				dl->notify(dl);
			DL_Close(dl);
			continue;
		}
		link = &dl->next;

		if (!cls.downloadmethod)
		{
			cls.downloadmethod = DL_HTTP;
			showndownload = dl;
			if (*dl->localname)
				strcpy(cls.downloadlocalname, dl->localname);
			else
				strcpy(cls.downloadlocalname, dl->url);
			strcpy(cls.downloadremotename, dl->url);
			cls.downloadstarttime = Sys_DoubleTime();
			cls.downloadedbytes = 0;
			shownbytestart = dl->completed;
		}
		if (cls.downloadmethod == DL_HTTP)
		{
			if (showndownload == dl)
			{
				if (dl->status == DL_FINISHED)
					cls.downloadpercent = 100;
				else if (dl->status != DL_ACTIVE)
					cls.downloadpercent = 0;
				else if (dl->totalsize <= 0)
					cls.downloadpercent = 50;
				else
					cls.downloadpercent = dl->completed*100.0f/dl->totalsize;
				cls.downloadedbytes = dl->completed;
			}
		}
	}
}
#endif
#endif	/*WEBCLIENT*/
