#include "quakedef.h"

#include "iweb.h"

#include "netinc.h"
#include "fs.h"

vfsfile_t *FS_GZ_DecompressWriteFilter(vfsfile_t *outfile, qboolean autoclosefile);

#if defined(WEBCLIENT)

#if defined(FTE_TARGET_WEB)


#define MYJS 1
#if MYJS
#include "web/ftejslib.h"
#else
#include <emscripten/emscripten.h>
#endif

static void DL_Cancel(struct dl_download *dl)
{
	//FIXME: clear out the callbacks somehow
	dl->ctx = NULL;
}
static void DL_OnLoad(void *c, void *data, int datasize)
{
	struct dl_download *dl = c;

	//make sure the file is 'open'.
	if (!dl->file)
	{
		if (*dl->localname)
		{
			FS_CreatePath(dl->localname, FS_GAMEONLY);
			dl->file = FS_OpenVFS(dl->localname, "w+b", FS_GAMEONLY);
		}
		else
		{
			//emscripten does not close the file. plus we seem to end up with infinite loops.
			dl->file = FS_OpenTemp();
		}
	}

	if (dl->file)
	{
		VFS_WRITE(dl->file, data, datasize);
		VFS_SEEK(dl->file, 0);
		dl->status = DL_FINISHED;
	}
	else
		dl->status = DL_FAILED;

	dl->replycode = 200;
#if !MYJS
	dl->completed += datasize;
#endif
}
#if MYJS
static void DL_OnError(void *c, int ecode)
#else
static void DL_OnError(void *c)
#endif
{
	struct dl_download *dl = c;

#if MYJS
	dl->replycode = ecode;
#else
	dl->replycode = 404;	//we don't actually know. should we not do this?
#endif
	Con_Printf("download %p: error %i\n", dl, dl->replycode);
	dl->status = DL_FAILED;
}
static void DL_OnProgress(void *c, int position, int totalsize)
{
	struct dl_download *dl = c;

	dl->completed = position;
	dl->totalsize = totalsize;
}

//this becomes a poll function. the main thread will call this once a frame or so.
qboolean DL_Decide(struct dl_download *dl)
{
	const char *url = dl->redir;
	if (!*url)
		url = dl->url;

	if (dl->postdata)
	{
		DL_Cancel(dl);
		return false;	//safe to destroy it now
	}

	if (dl->ctx)
	{
		if (dl->status == DL_FINISHED)
			return false;
		if (dl->status == DL_FAILED)
		{
			DL_Cancel(dl);
			return false;
		}
	}
	else
	{
		dl->status = DL_ACTIVE;
		dl->abort = DL_Cancel;
		dl->ctx = dl;

#if MYJS
		emscriptenfte_async_wget_data2(url, dl, DL_OnLoad, DL_OnError, DL_OnProgress);
#else
		//annoyingly, emscripten doesn't provide an onprogress callback, unlike firefox etc, so we can't actually tell how far its got.
		//we'd need to provide our own js library to fix this. it can be done, I'm just lazy.
		emscripten_async_wget_data(url, dl, DL_OnLoad, DL_OnError);
#endif
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

static void nadl_cleanup_cb(void* user_data, int32_t result)
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
	struct PP_CompletionCallback ccb = {nadl_cleanup_cb, ctx, PP_COMPLETIONCALLBACK_FLAG_NONE};
	ppb_core->CallOnMainThread(1000, ccb, 0);
}

qboolean DL_Decide(struct dl_download *dl)
{
	const char *url = dl->redir;
	struct nacl_dl *ctx;
	if (!*url)
		url = dl->url;

	if (dl->postdata)
	{
		NADL_Cleanup(dl);
		return false;	//safe to destroy it now
	}

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
qboolean DL_Decide(struct dl_download *dl);

/*
This file does one thing. Connects to servers and grabs the specified file. It doesn't do any uploading whatsoever. Live with it.
It doesn't use persistant connections.

*/

struct http_dl_ctx_s {
//	struct dl_download *dlctx;

#ifndef NPFTE
	vfsfile_t *sock;
#else
	SOCKET sock;	//FIXME: support https.
#endif

	char *buffer;

	size_t bufferused;
	size_t bufferlen;

	size_t totalreceived;	//useful when we're just dumping to a file.

	struct vfsfile_s *file;	//if gzipping, this is a temporary file. we'll write to the real file from this after the transfer is complete.
	qboolean gzip;
	qboolean chunking;
	size_t chunksize;
	size_t chunked;

	enum {HC_REQUESTING, HC_GETTINGHEADER, HC_GETTING} state;

	size_t contentlength;
};

void HTTP_Cleanup(struct dl_download *dl)
{
	struct http_dl_ctx_s *con = dl->ctx;
	dl->ctx = NULL;

#ifndef NPFTE
	if (con->sock)
		VFS_CLOSE(con->sock);
	con->sock = NULL;
#else
	if (con->sock != INVALID_SOCKET)
		closesocket(con->sock);
	con->sock = INVALID_SOCKET;
#endif
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
	char mimetype[256];
	char *nl;
	char *msg;
	int ammount;
	switch(con->state)
	{
	case HC_REQUESTING:
#ifndef NPFTE
		ammount = VFS_WRITE(con->sock, con->buffer, con->bufferused);
		if (!ammount)
			return true;
		if (ammount < 0)
			return false;
#else
		ammount = send(con->sock, con->buffer, con->bufferused, 0);

		if (!ammount)
			return false;

		if (ammount < 0)
		{
			if (neterrno() != NET_EWOULDBLOCK)
				return false;
			return true;
		}
#endif

		con->bufferused -= ammount;
		memmove(con->buffer, con->buffer+ammount, con->bufferused);
		if (!con->bufferused)	//that's it, all sent.
			con->state = HC_GETTINGHEADER;
		break;

	case HC_GETTINGHEADER:
		if (con->bufferlen - con->bufferused < 1530)
			ExpandBuffer(con, 1530);

#ifndef NPFTE
		ammount = VFS_READ(con->sock, con->buffer+con->bufferused, con->bufferlen-con->bufferused-15);
		if (!ammount)
			return true;
		if (ammount < 0)
			return false;
#else
		ammount = recv(con->sock, con->buffer+con->bufferused, con->bufferlen-con->bufferused-15, 0);
		if (!ammount)
			return false;
		if (ammount < 0)
		{
			if (neterrno() != NET_EWOULDBLOCK)
				return false;
			return true;
		}
#endif

		con->bufferused+=ammount;
		con->buffer[con->bufferused] = '\0';
		//have we got the entire thing yet?

		msg = con->buffer;
		con->chunking = false;
		con->contentlength = -1;
		con->gzip = false;
		*mimetype = 0;
		*Location = 0;
		if (strnicmp(msg, "HTTP/", 5))
		{	//pre version 1. (lame servers.
			con->state = HC_GETTING;
			dl->status = DL_ACTIVE;
			con->contentlength = -1;	//meaning end of stream.
			dl->replycode = 200;
		}
		else
		{
			qboolean hcomplete = false;
			while(*msg)
			{
				if (*msg == '\n')
				{
					if (msg[1] == '\n')
					{	//tut tut, not '\r'? that's not really allowed...
						msg+=2;
						hcomplete = true;
						break;
					}
					if (msg[1] == '\r' && msg[2] == '\n')
					{
						msg+=3;
						hcomplete = true;
						break;
					}
					msg++;
				}
				while (*msg == ' ' || *msg == '\t')
					msg++;

				nl = strchr(msg, '\n');
				if (!nl)
					break;//not complete, don't bother trying to parse it.
				if (!strnicmp(msg, "Content-Length: ", 16))
					con->contentlength = atoi(msg+16);
				else if (!strnicmp(msg, "Content-Type:", 13))
				{
					*nl = '\0';
					COM_TrimString(msg+13, mimetype, sizeof(mimetype));
					*nl = '\n';
				}
				else if (!strnicmp(msg, "Location: ", 10))
				{
					*nl = '\0';
					COM_TrimString(msg+10, Location, sizeof(Location));
					*nl = '\n';
				}
				else if (!strnicmp(msg, "Content-Encoding: ", 18))
				{
					char *chunk = strstr(msg, "gzip");
					if (chunk < nl)
						con->gzip = true;
				}
				else if (!strnicmp(msg, "Transfer-Encoding: ", 19))
				{
					char *chunk = strstr(msg, "chunked");
					if (chunk < nl)
						con->chunking = true;
				}
				msg = nl;
			}
			if (!hcomplete)
				break;//headers not complete. break out of switch

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
				char trimmed[256];
				nl = strchr(msg, '\n');
				if (nl)
					*nl = '\0';
				Con_Printf("HTTP: %s %s (%s)\n", buffer, COM_TrimString(msg, trimmed, sizeof(trimmed)), Location);
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
					dl->poll = DL_Decide;
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

			memmove(con->buffer, con->buffer+ammount, con->bufferused);
		}

		if (dl->notifystarted)
		{
			if (!dl->notifystarted(dl, *mimetype?mimetype:NULL))
			{
				dl->notifycomplete = NULL;
				dl->status = DL_FAILED;
				return false;
			}
		}


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
				if (*dl->localname)
					Con_Printf("HTTP: Couldn't open file \"%s\"\n", dl->localname);
				else
					Con_Printf("HTTP: Couldn't open temporary file\n");
				dl->status = DL_FAILED;
				return false;
			}
		}

		if (con->gzip)
		{
#if !defined(NPFTE) && defined(AVAIL_ZLIB)
#if 1
			con->file = FS_GZ_DecompressWriteFilter(dl->file, false);
#else
			con->file = FS_OpenTemp();
#endif
#else
			Con_Printf("HTTP: no support for gzipped files \"%s\"\n", dl->localname);
			dl->status = DL_FAILED;
			return false;
#endif
		}
		else
			con->file = dl->file;
		con->state = HC_GETTING;
		dl->status = DL_ACTIVE;
		//Fall through
	case HC_GETTING:
		if (con->bufferlen - con->bufferused < 1530)
			ExpandBuffer(con, 1530);

#ifndef NPFTE
		ammount = VFS_READ(con->sock, con->buffer+con->bufferused, con->bufferlen-con->bufferused-1);
		if (ammount == 0)
			return true;	//no data yet
		else if (ammount < 0)
			ammount = 0;	//error (EOF?)
#else
		ammount = recv(con->sock, con->buffer+con->bufferused, con->bufferlen-con->bufferused-1, 0);
		if (ammount < 0)
		{
			if (neterrno() != NET_EWOULDBLOCK)
				return false;
			return true;
		}
#endif

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
			con->totalreceived+=con->bufferused;
			if (con->file)	//we've got a chunk in the buffer
			{	//write it
				if (VFS_WRITE(con->file, con->buffer, con->bufferused) != con->bufferused)
				{
					Con_Printf("Write error whilst downloading %s\nDisk full?\n", dl->localname);
					return false;
				}
				con->bufferused = 0;
			}
			if (con->totalreceived == con->contentlength)
				ammount = 0;
		}

		if (!ammount)
		{	//server closed off the connection.
			if (con->chunksize)
				dl->status = DL_FAILED;
			else
			{
#if !defined(NPFTE) && defined(AVAIL_ZLIB)
#if 1
				if (con->gzip && con->file)
				{
					VFS_CLOSE(con->file);
					con->file = NULL;
				}
#else
				if (con->gzip && con->file)
				{
					VFS_SEEK(con->file, 0);
					dl->file = FS_DecompressGZip(con->file, dl->file);
					con->file = NULL;
				}
#endif
#endif
				if (con->contentlength != -1 && con->totalreceived != con->contentlength)
					dl->status = DL_FAILED;	//file was truncated
				else
					dl->status = (dl->replycode == 200)?DL_FINISHED:DL_FAILED; 
			}
			return false;
		}
		dl->completed = con->totalreceived;

		break;
	}

	return true;
}

void HTTPDL_Establish(struct dl_download *dl)
{
#ifdef NPFTE
	unsigned long _true = true;
	struct sockaddr_qstorage	serveraddr;
	int addressfamily;
	int addresssize;
#endif
	struct http_dl_ctx_s *con;
	qboolean https = false;

	char server[128];
	char uri[MAX_OSPATH];
	char *slash;
	const char *url = dl->redir;
	if (!*url)
		url = dl->url;

	if (!strnicmp(url, "https://", 8))
	{
		url+=8;
		https = true;
	}
	else if (!strnicmp(url, "http://", 7))
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

#ifndef NPFTE
	if (https)
	{
#ifdef HAVE_SSL
		//https uses port 443 instead of 80 by default
		con->sock = FS_OpenTCP(server, 443);
		//and with an extra ssl/tls layer between tcp and http.
		con->sock = FS_OpenSSL(server, con->sock, false);
#else
		con->sock = NULL;
#endif
	}
	else
	{
		con->sock = FS_OpenTCP(server, 80);
	}
	if (!con->sock)
	{
		dl->status = DL_FAILED;
		return;
	}
#else
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

	//FIXME: make the connect call with a non-blocking socket.
	//FIXME: use a vfsfile_t instead of a direct socket to support https

	//not yet blocking.
	if (connect(con->sock, (struct sockaddr *)&serveraddr, addresssize) == -1)
	{
		int err = neterrno();
		switch(err)
		{
		case NET_EACCES:
			Con_Printf("HTTP: connect(%s): access denied. Check firewall.\n", server);
			break;
		case NET_ETIMEDOUT:
			Con_Printf("HTTP: connect(%s): timed out.\n", server);
			break;
		default:
			Con_Printf("HTTP: connect(%s): %s", server, strerror(neterrno()));
			break;
		}
		dl->status = DL_FAILED;
		return;
	}

	if (ioctlsocket (con->sock, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		dl->status = DL_FAILED;
		return;
	}
#endif
	if (dl->postdata)
	{
		ExpandBuffer(con, 1024 + strlen(uri) + strlen(server) + strlen(dl->postmimetype) + dl->postlen);
		Q_snprintfz(con->buffer, con->bufferlen,
			"POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Length: %u\r\n"
			"Content-Type: %s\r\n"
			"Connection: close\r\n"
#if !defined(NPFTE) && defined(AVAIL_ZLIB)
			"Accept-Encoding: gzip\r\n"
#endif
			"User-Agent: "FULLENGINENAME"\r\n"
			"\r\n", uri, server, (unsigned int)dl->postlen, dl->postmimetype);
		con->bufferused = strlen(con->buffer);
		memcpy(con->buffer + con->bufferused, dl->postdata, dl->postlen);
		con->bufferused += dl->postlen;
	}
	else
	{
		ExpandBuffer(con, 512*1024);
		Q_snprintfz(con->buffer, con->bufferlen,
			"GET %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: close\r\n"			//theoretically, this is not needed. but as our code will basically do it anyway, it might as well be here FIXME: implement connection reuse.
#if !defined(NPFTE) && defined(AVAIL_ZLIB)
			"Accept-Encoding: gzip\r\n"
#endif
			"User-Agent: "FULLENGINENAME"\r\n"
			"\r\n", uri, server);
		con->bufferused = strlen(con->buffer);
	}
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

/*
//decode a base64 byte to a 0-63 value. Cannot cope with =.
static unsigned int Base64_DecodeByte(char byt)
{
    if (byt >= 'A' && byt <= 'Z')
        return (byt-'A') + 0;
    if (byt >= 'a' && byt <= 'z')
        return (byt-'a') + 26;
    if (byt >= '0' && byt <= '9')
        return (byt-'0') + 52;
    if (byt == '+')
        return 62;
    if (byt == '/')
        return 63;
    return -1;
}
//FIXME: we should be able to skip whitespace.
static int Base64_Decode(char *out, int outlen, char **srcout, int *srclenout)
{
	int len = 0;
	unsigned int result;

	char *src = *srcout;
	int srclen = *srclenout;

	//4 input chars give 3 output chars
	while(srclen >= 4)
	{
		if (len+3 > outlen)
		{
			//ran out of space in the output buffer
			*srcout = src;
			*srclenout = srclen;
			return len;
		}
		result = Base64_DecodeByte(src[0])<<18;
		result |= Base64_DecodeByte(src[1])<<12;
		out[len++] = (result>>16)&0xff;
		if (src[2] != '=')
		{
			result |= Base64_DecodeByte(src[2])<<6;
			out[len++] = (result>>8)&0xff;
			if (src[3] != '=')
			{
				result |= Base64_DecodeByte(src[3])<<0;
				out[len++] = (result>>0)&0xff;
			}
		}
		if (result & 0xff000000)
			return 0;	//some kind of invalid char

		src += 4;
		srclen -= 4;
	}

	//end of string
	*srcout = src;
	*srclenout = srclen;

	//some kind of error
	if (srclen)
	{
		if (srclen != 1 || *src)
			return 0;
	}
	
	return len;
}

qboolean DataScheme_Decode(struct dl_download *dl)
{
	char block[8192];
	int remaining, blocksize;
	char mimetype[256];
	char baseval[256];
	char charsetval[256];
	char *url;
	char *data;
	char *charset;
	char *base;
	//failed previously
	if (dl->status == DL_FAILED)
		return false;

	//data:[<MIME-type>][;charset=<encoding>][;base64],<data>

	*mimetype = 0;
	*baseval = 0;
	*charsetval = 0;

	url = dl->url;
	if (!strncmp(url, "data:", 5))
		url+=5;	//should always match
	data = strchr(url, ',');
	if (!data)
		return false;
	charset = memchr(url, ';', data-url);
	if (charset)
	{
		base = memchr(charset+1, ';', data-charset);
		if (base)
		{
			if (data-(base+1) >= sizeof(baseval))
				return false;
			memcpy(baseval, base+1, data-(base+1));
			baseval[data-(base+1)] = 0;
		}
		else
			base = data;
		if (base-(charset+1) >= sizeof(charsetval))
			return false;
		memcpy(charsetval, charset+1, base-(charset+1));
		charsetval[base-(charset+1)] = 0;

		if (!strchr(charsetval, '='))
		{
			strcpy(baseval, charsetval);
			*charsetval = 0;
		}
	}
	else
		charset = data;
	if (charset-(url) >= sizeof(charsetval))
		return false;
	memcpy(mimetype, url, charset-(url));
	mimetype[charset-(url)] = 0;

	if (!*mimetype)
		Q_strncpyz(mimetype, "text/plain", sizeof(mimetype));
	if (!*charsetval)
		Q_strncpyz(charsetval, "charset=US-ASCII", sizeof(charsetval));

	if (dl->notifystarted)
		dl->notifystarted(dl, *mimetype?mimetype:NULL);

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
			if (*dl->localname)
				Con_Printf("HTTP: Couldn't open file \"%s\"\n", dl->localname);
			else
				Con_Printf("HTTP: Couldn't open temporary file\n");
			dl->status = DL_FAILED;
			return false;
		}
	}

	data++;
	remaining = strlen(data);
	while(remaining > 0)
	{
		blocksize = Base64_Decode(block, sizeof(block), &data, &remaining);
		if (!blocksize)
		{
			dl->status = DL_FAILED;
			return false;
		}
		VFS_WRITE(dl->file, block, blocksize);
	}

	dl->status = DL_FINISHED;
	return false;
}
*/

qboolean DL_Decide(struct dl_download *dl)
{
	const char *url = dl->redir;
	if (!*url)
		url = dl->url;

	/*if (!strnicmp(url, "data:", 5))
		dl->poll = DataScheme_Decode;
	else*/
	if (!strnicmp(url, "http://", 7))
		dl->poll = HTTPDL_Poll;
	else if (!strnicmp(url, "https://", 7))
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
#if defined(LOADERTHREAD) && !defined(NPFTE)
static void HTTP_Wake_Think(void *ctx, void *data, size_t a, size_t b)
{
	HTTP_CL_Think();
}
#endif
static int DL_Thread_Work(void *arg)
{
	struct dl_download *dl = arg;

	while (!dl->threaddie)
	{
		if (!dl->poll(dl))
		{
#ifdef NPFTE
			//the plugin doesn't have a download loop
			if (dl->notifycomplete)
				dl->notifycomplete(dl);
			if (dl->file)
				VFS_CLOSE(dl->file);
#else
			if (dl->status != DL_FAILED && dl->status != DL_FINISHED)
				dl->status = DL_FAILED;
#endif
			break;
		}
	}

#if defined(LOADERTHREAD) && !defined(NPFTE)
	COM_AddWork(0, HTTP_Wake_Think, NULL, NULL, 0, 0);
#endif
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
		dl->notifycomplete = NotifyFunction;

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
	newdl = malloc(sizeof(*newdl) + strlen(url)+1);
	if (!newdl)
		return NULL;
	memset(newdl, 0, sizeof(*newdl));
	newdl->url = (char*)(newdl+1);
	strcpy(newdl->url, url);
	newdl->poll = DL_Decide;

	if (!newdl->poll(newdl))
	{
		free(newdl);
		newdl = NULL;
	}

	return newdl;
}

/*destroys an entire download context*/
void DL_Close(struct dl_download *dl)
{
#if !defined(NPFTE) && !defined(SERVERONLY)
	if (cls.download == &dl->qdownload)
		cls.download = NULL;
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
	if (dl->postdata)
		BZ_Free(dl->postdata);
	free(dl);
}


/*updates pending downloads*/
#ifndef NPFTE

static struct dl_download *activedownloads;
/*create a download context and add it to the list, for lazy people. not threaded*/
struct dl_download *HTTP_CL_Get(const char *url, const char *localfile, void (*NotifyFunction)(struct dl_download *dl))
{
	struct dl_download *newdl = DL_Create(url);
	if (!newdl)
		return newdl;

	newdl->notifycomplete = NotifyFunction;
	if (localfile)
		Q_strncpyz(newdl->localname, localfile, sizeof(newdl->localname));

	newdl->next = activedownloads;
	activedownloads = newdl;


#ifndef SERVERONLY
	if (!cls.download && localfile && !newdl->isquery)
	{
		cls.download = &newdl->qdownload;
		newdl->qdownload.method = DL_HTTP;
		if (*newdl->localname)
			Q_strncpyz(newdl->qdownload.localname, newdl->localname, sizeof(newdl->qdownload.localname));
		else
			Q_strncpyz(newdl->qdownload.localname, newdl->url, sizeof(newdl->qdownload.localname));
		Q_strncpyz(newdl->qdownload.remotename, newdl->url, sizeof(newdl->qdownload.remotename));
		newdl->qdownload.starttime = Sys_DoubleTime();
	}
#endif

	return newdl;
}

struct dl_download *HTTP_CL_Put(const char *url, const char *mime, const char *data, size_t datalen, void (*NotifyFunction)(struct dl_download *dl))
{
	struct dl_download *dl;
	if (!*mime)
		return NULL;

	dl = HTTP_CL_Get(url, NULL, NotifyFunction);
	Q_strncpyz(dl->postmimetype, mime, sizeof(dl->postmimetype));
	dl->postdata = BZ_Malloc(datalen);
	memcpy(dl->postdata, data, datalen);
	dl->postlen = datalen;
	return dl;
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
			if (dl->file && dl->file->Seek)
				VFS_SEEK(dl->file, 0);
			if (dl->notifycomplete)
				dl->notifycomplete(dl);
			DL_Close(dl);
			continue;
		}
		link = &dl->next;

#ifndef SERVERONLY
		if (!cls.download && !dl->isquery)
		{
			cls.download = &dl->qdownload;
			dl->qdownload.method = DL_HTTP;
			if (*dl->localname)
				Q_strncpyz(dl->qdownload.localname, dl->localname, sizeof(dl->qdownload.localname));
			else
				Q_strncpyz(dl->qdownload.localname, dl->url, sizeof(dl->qdownload.localname));
			Q_strncpyz(dl->qdownload.remotename, dl->url, sizeof(dl->qdownload.remotename));
			dl->qdownload.starttime = Sys_DoubleTime();
		}

		if (dl->status == DL_FINISHED)
			dl->qdownload.percent = 100;
		else if (dl->status != DL_ACTIVE)
			dl->qdownload.percent = 0;
		else if (dl->totalsize <= 0)
		{
			dl->qdownload.sizeunknown = true;
			dl->qdownload.percent = 50;
		}
		else
			dl->qdownload.percent = dl->completed*100.0f/dl->totalsize;
		dl->qdownload.completedbytes = dl->completed;

		if (dl->qdownload.ratetime < Sys_DoubleTime())
		{
			dl->qdownload.ratetime = Sys_DoubleTime()+1;
			dl->qdownload.rate = (dl->qdownload.completedbytes - dl->qdownload.ratebytes) / 1;
			dl->qdownload.ratebytes = dl->qdownload.completedbytes;
		}
#endif
	}
}

void HTTP_CL_Terminate(void)
{
	struct dl_download *dl = activedownloads;
	struct dl_download *next = NULL;
	next = activedownloads;
	activedownloads = NULL;
	while (next)
	{
		dl = next;
		next = dl->next;
		DL_Close(dl);
	}
	HTTP_CL_Think();
}
#endif
#endif	/*WEBCLIENT*/


typedef struct 
{
	vfsfile_t funcs;

	char *data;
	int maxlen;
	int writepos;
	int readpos;
} vfspipe_t;

static qboolean QDECL VFSPIPE_Close(vfsfile_t *f)
{
	vfspipe_t *p = (vfspipe_t*)f;
	free(p->data);
	free(p);
	return true;
}
static qofs_t QDECL VFSPIPE_GetLen(vfsfile_t *f)
{
	vfspipe_t *p = (vfspipe_t*)f;
	return p->writepos - p->readpos;
}
//static unsigned long QDECL VFSPIPE_Tell(vfsfile_t *f)
//{
//	return 0;
//}
//static qboolean QDECL VFSPIPE_Seek(vfsfile_t *f, unsigned long offset)
//{
//	Con_Printf("Seeking is a bad plan, mmkay?\n");
//	return false;
//}
static int QDECL VFSPIPE_ReadBytes(vfsfile_t *f, void *buffer, int len)
{
	vfspipe_t *p = (vfspipe_t*)f;
	if (len > p->writepos - p->readpos)
		len = p->writepos - p->readpos;
	memcpy(buffer, p->data+p->readpos, len);
	p->readpos += len;

	if (p->readpos > 8192)
	{
		//shift the memory down periodically
		//fixme: use cyclic buffer? max size, etc?
		memmove(p->data, p->data+p->readpos, p->writepos-p->readpos);

		p->writepos -= p->readpos;
		p->readpos = 0;
	}
	return len;
}
static int QDECL VFSPIPE_WriteBytes(vfsfile_t *f, const void *buffer, int len)
{
	vfspipe_t *p = (vfspipe_t*)f;
	if (p->writepos + len > p->maxlen)
	{
		p->maxlen = p->writepos + len;
		p->data = realloc(p->data, p->maxlen);
	}
	memcpy(p->data+p->writepos, buffer, len);
	p->writepos += len;
	return len;
}

vfsfile_t *VFSPIPE_Open(void)
{
	vfspipe_t *newf;
	newf = malloc(sizeof(*newf));
	newf->data = NULL;
	newf->maxlen = 0;
	newf->readpos = 0;
	newf->writepos = 0;
	newf->funcs.Close = VFSPIPE_Close;
	newf->funcs.Flush = NULL;
	newf->funcs.GetLen = VFSPIPE_GetLen;
	newf->funcs.ReadBytes = VFSPIPE_ReadBytes;
	newf->funcs.Seek = NULL;//VFSPIPE_Seek;
	newf->funcs.Tell = NULL;//VFSPIPE_Tell;
	newf->funcs.WriteBytes = VFSPIPE_WriteBytes;
	newf->funcs.seekingisabadplan = true;

	return &newf->funcs;
}
