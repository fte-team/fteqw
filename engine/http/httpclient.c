#include "quakedef.h"

#include "iweb.h"

#include "netinc.h"
#include "fs.h"

#if defined(WEBCLIENT)
#ifndef NPFTE
static struct dl_download *activedownloads;
#endif

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
	//also fires from 404s.
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
	//fires from cross-domain blocks, tls errors, etc.
	//anything which doesn't yield an http response (404 is NOT an error as far as js is aware).

#if MYJS
	dl->replycode = ecode;
#else
	dl->replycode = 404;	//we don't actually know. should we not do this?
#endif
	Con_Printf(CON_WARNING"dl error: %s\n", dl->url);
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

#define COOKIECOOKIECOOKIE
#ifdef COOKIECOOKIECOOKIE
typedef struct cookie_s
{
	struct cookie_s *next;
	char *domain;
	int secure;
	char *name;
	char *value;
} cookie_t;
cookie_t *cookies;

#ifdef NPFTE
#define Z_Malloc malloc
#define Z_Free free
#endif

//set a specific cookie.
void Cookie_Feed(char *domain, int secure, char *name, char *value)
{
	cookie_t **link, *c;
	Sys_LockMutex(com_resourcemutex);
	for(link = &cookies; (c=*link)!=NULL; link = &(*link)->next)
	{
		if (!strcmp(c->domain, domain) && c->secure == secure && !strcmp(c->name, name))
			break;
	}
	//delete it, if it exists, so we can create it anew.
	if (c)
	{
		*link = c->next;
		Z_Free(c);
	}
	if (value && *value)
	{
//		Con_Printf("Setting cookie http%s://%s/ %s=%s\n", secure?"s":"", domain, name, value);
		c = Z_Malloc(sizeof(*c) + strlen(domain) + strlen(name) + strlen(value) + 3);
		c->domain = (char*)(c+1);
		strcpy(c->domain, domain);
		c->secure = secure;
		c->name = c->domain+strlen(c->domain)+1;
		strcpy(c->name, name);
		c->value = c->name+strlen(c->name)+1;
		strcpy(c->value, value);
		c->next = cookies;
		cookies = c;
	}
	else
	{
//		Con_Printf("Deleted cookie http%s://%s/ %s\n", secure?"s":"", domain, name);
	}
	Sys_UnlockMutex(com_resourcemutex);
}

//just removes all the cookies it can.
void Cookie_Monster(void)
{
	cookie_t *c;
	while (cookies)
	{
		c = cookies;
		cookies = c->next;
		Z_Free(c);
	}
}

//parses Set-Cookie: THISPARTONLY\r\n
//we don't support:
//domain) we don't have a list of composite roots, like .co.uk, and thus this wouldn't work very safely anyway. thus we require the exact same host each time
//path) I'm going to call this an optimisation feature and not bother with it... hopefully there won't be too many sites that have sub-paths or third-party stuff... gah.
//httponly) irrelevant until we support javascript... which we don't.
//secure) assumed to be true. https:// vs http:// are thus completely independant. sorry.
//expires) gah, parsing time values sucks! plus we don't have persistent storage.
void Cookie_Parse(char *domain, int secure, char *line, char *end)
{
	char *e;
	while (*line == ' ' && line < end)
		line++;
	for (e = line; e < end; e++)
		if (*e == ';')
			end = e;

	for (e = line; e < end; e++)
		if (*e == '=')
			break;

	*e = 0;
	*end = 0;
	Cookie_Feed(domain, secure, line, e+1);
}
//outputs a complete http line: Cookie: a=v1; b=v2\r\n
void Cookie_Regurgitate(char *domain, int secure, char *buffer, size_t buffersize)
{
	qboolean hascookies = false;
	cookie_t *c;
//	char *l = buffer;
	buffersize -= 3;	//\r\n\0
	*buffer = 0;
	Sys_LockMutex(com_resourcemutex);
	for (c = cookies; c; c = c->next)
	{
		if (!strcmp(c->domain, domain) && c->secure == secure)
		{
			int nlen,vlen;
			if (!hascookies)
			{
				if (buffersize < 8)
					break;
				strcpy(buffer, "Cookie: ");
				buffersize -= 8;
				hascookies=true;
			}
			else
			{
				if (buffersize < 2)
					break;
				strcpy(buffer, "; ");
				buffersize -= 2;
			}
			buffer += strlen(buffer);

			nlen = strlen(c->name);
			vlen = strlen(c->value);
			
			if (buffersize < nlen+1+vlen)
				break;
			memcpy(buffer, c->name, nlen);
			buffer += nlen;
			*buffer++ = '=';
			memcpy(buffer, c->value, vlen);
			buffer += vlen;
		}
	}
	Sys_UnlockMutex(com_resourcemutex);

	if (hascookies)
		strcpy(buffer, "\r\n");
	else
		*buffer = 0;

//	if (*l)
//		Con_Printf("Sending cookie(s) to http%s://%s/ %s\n", secure?"s":"", domain, l);
}
#endif

struct http_dl_ctx_s {
//	struct dl_download *dlctx;

#ifndef NPFTE
	vfsfile_t *stream;
	SOCKET sock;	//so we can wait on it when multithreaded.
#else
	SOCKET sock;	//FIXME: support https.
#endif

	char *buffer;

	char server[128];
	qboolean secure;

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
	if (con->stream)
		VFS_CLOSE(con->stream);
	con->stream = NULL;
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
	qboolean transfercomplete = false;

#ifdef MULTITHREAD
	//if we're running in a thread, wait for some actual activity instead of busylooping like an moron.
	if (dl->threadctx)
	{
		struct timeval timeout;
		fd_set	rdset, wrset;
		FD_ZERO(&wrset);
		FD_ZERO(&rdset);
		FD_SET(con->sock, &wrset); // network socket
		FD_SET(con->sock, &rdset); // network socket
		timeout.tv_sec = 0;
		timeout.tv_usec = 0.1*1000*1000;
		if (con->state == HC_REQUESTING)
			select(con->sock+1, &rdset, &wrset, NULL, &timeout);	//wake up when we can read OR write
		else
			select(con->sock+1, &rdset, NULL, NULL, &timeout);	//wake when we can read.
		//note that https should wake up more often, but we don't want to wake up because we *can* write when we're reading without any need to write.
	}
#endif

	switch(con->state)
	{
	case HC_REQUESTING:
#ifndef NPFTE
		ammount = VFS_WRITE(con->stream, con->buffer, con->bufferused);
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
		ammount = VFS_READ(con->stream, con->buffer+con->bufferused, con->bufferlen-con->bufferused-15);
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
		con->contentlength = -1;	//means unspecified
		con->gzip = false;
		*mimetype = 0;
		*Location = 0;
		if (strnicmp(msg, "HTTP/", 5))
		{	//pre version 1 (lame servers). no response headers at all.
			con->state = HC_GETTING;
			dl->status = DL_ACTIVE;
			dl->replycode = 200;
		}
		else
		{
			//check if the headers are complete or not
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
				msg = nl;
			}

			if (!hcomplete)
				break;//headers not complete. break out of switch

			//okay, they're complete, woot. we need to actually go through the headers now
			msg = con->buffer;
			while(*msg)
			{
				if (*msg == '\n')
				{
					if (msg[1] == '\n')
					{	//tut tut, not '\r'? that's not really allowed...
						msg+=2;
						break;
					}
					if (msg[1] == '\r' && msg[2] == '\n')
					{
						msg+=3;
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
#ifdef COOKIECOOKIECOOKIE
				else if (!strnicmp(msg, "Set-Cookie: ", 12))
				{
					Cookie_Parse(con->server, con->secure, msg+12, nl);
				}
#endif
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

			if (con->contentlength != -1 && con->contentlength > dl->sizelimit)
			{
				dl->replycode = 413;	//413 Payload Too Large 
				Con_Printf("HTTP: request exceeds size limit\n");
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
#ifdef AVAIL_GZDEC
			con->file = FS_GZ_WriteFilter(dl->file, false, false);
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
		ammount = VFS_READ(con->stream, con->buffer+con->bufferused, con->bufferlen-con->bufferused-1);
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

		if (con->chunking)
		{
			//9\r\n
			//chunkdata\r\n
			//(etc)
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

					if (con->chunksize == trim)
					{	//we need to find the next \n and trim it.
						nl = strchr(con->buffer+con->chunked+trim, '\n');
						if (!nl)
							break;
						nl++;
						con->chunksize = 0;
						con->chunked += trim;

						//chop out the \r\n from the stream
						trim = nl - (con->buffer+con->chunked);
						memmove(con->buffer + con->chunked, nl, con->buffer+con->bufferused-nl+1);
						con->bufferused -= trim;
					}
					else
					{
						con->chunksize -= trim;
						con->chunked += trim;
					}

					if (!(con->bufferused - con->chunked))
						break;
				}
				else
				{
					size_t nextsize;
					nl = strchr(con->buffer+con->chunked, '\n');
					if (!nl)
						break;
					nextsize = strtoul(con->buffer+con->chunked, NULL, 16);	//it's hex.
					nl++;
					if (!nextsize) //eof. make sure we skip its \n too
					{
						nl = strchr(nl, '\n');
						if (!nl)
							break;
						transfercomplete = true;
					}
					con->chunksize = nextsize;
					trim = nl - (con->buffer+con->chunked);
					memmove(con->buffer + con->chunked, nl, con->buffer+con->bufferused-nl+1);
					con->bufferused -= trim;
				}
			}

			con->totalreceived+=con->chunked;
			if (con->totalreceived > dl->sizelimit)
			{
				dl->replycode = 413;	//413 Payload Too Large 
				Con_Printf("HTTP: request exceeds size limit\n");
				return false;	//something went wrong.
			}
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
			int chunk = con->bufferused;
			if (con->contentlength != -1 && chunk > con->contentlength-con->totalreceived)
				chunk = con->contentlength-con->totalreceived;

			con->totalreceived+=chunk;
			if (con->totalreceived > dl->sizelimit)
			{
				dl->replycode = 413;	//413 Payload Too Large 
				Con_Printf("HTTP: request exceeds size limit\n");
				return false;	//something went wrong.
			}
			if (con->file)	//we've got a chunk in the buffer
			{	//write it
				if (VFS_WRITE(con->file, con->buffer, chunk) != chunk)
				{
					Con_Printf("Write error whilst downloading %s\nDisk full?\n", dl->localname);
					return false;
				}

				//and move the unparsed chunk to the front.
				memmove(con->buffer, con->buffer+con->bufferused, con->bufferused-chunk);
				con->bufferused -= chunk;
			}
			if (con->totalreceived == con->contentlength)
				transfercomplete = true;
		}

		if (!ammount || transfercomplete)
		{	//server closed off the connection (or signalled eof/sent enough data).
			//if (ammount) then we can save off the connection for reuse.
			if (con->chunksize)
				dl->status = DL_FAILED;
			else
			{
#ifdef AVAIL_GZDEC
				if (con->gzip && con->file)
				{
					VFS_CLOSE(con->file);
					con->file = NULL;
				}
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

#ifdef COOKIECOOKIECOOKIE
	char cookies[8192];
#else
	char *cookies = "";
#endif
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

	con = malloc(sizeof(*con));
	memset(con, 0, sizeof(*con));

	slash = strchr(url, '/');
	if (!slash)
	{
		Q_strncpyz(con->server, url, sizeof(con->server));
		Q_strncpyz(uri, "/", sizeof(uri));
	}
	else
	{
		Q_strncpyz(uri, slash, sizeof(uri));
		Q_strncpyz(con->server, url, sizeof(con->server));
		con->server[slash-url] = '\0';
	}

	dl->ctx = con;
	dl->abort = HTTP_Cleanup;

	dl->status = DL_RESOLVING;

#ifndef NPFTE
	con->sock = INVALID_SOCKET;
	con->stream = NULL;
	con->secure = false;
#ifndef HAVE_SSL
	if (!https)
#endif
	{
		netadr_t adr = {0};
		//fixme: support more than one address possibility?
		//https uses a different default port
		if (NET_StringToAdr2(con->server, https?443:80, &adr, 1))
			con->sock = TCP_OpenStream(&adr);
		con->stream = FS_OpenTCPSocket(con->sock, true, con->server);
	}
#ifdef HAVE_SSL
	if (https)
	{
		//https has an extra ssl/tls layer between tcp and http.
		con->stream = FS_OpenSSL(con->server, con->stream, false, false);
		con->secure = true;
	}
#endif
	if (!con->stream)
	{
		dl->status = DL_FAILED;
		return;
	}
#else
	con->secure = false;
	if (https || !NET_StringToSockaddr(con->server, 80, &serveraddr, &addressfamily, &addresssize))
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
			Con_Printf("HTTP: connect(%s): access denied. Check firewall.\n", con->server);
			break;
		case NET_ETIMEDOUT:
			Con_Printf("HTTP: connect(%s): timed out.\n", con->server);
			break;
		default:
			Con_Printf("HTTP: connect(%s): %s", con->server, strerror(neterrno()));
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
#ifdef COOKIECOOKIECOOKIE
	Cookie_Regurgitate(con->server, con->secure, cookies, sizeof(cookies));
#endif
	if (dl->postdata)
	{
		ExpandBuffer(con, 1024 + strlen(uri) + strlen(con->server) + strlen(cookies) + strlen(dl->postmimetype) + dl->postlen);
		Q_snprintfz(con->buffer, con->bufferlen,
			"POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			/*Cookie:*/ "%s"
			"Content-Length: %u\r\n"
			"Content-Type: %s\r\n"
			"Connection: close\r\n"
#ifdef AVAIL_GZDEC
			"Accept-Encoding: gzip\r\n"
#endif
			"User-Agent: "FULLENGINENAME"\r\n"
			"\r\n", uri, con->server, cookies, (unsigned int)dl->postlen, dl->postmimetype);
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
			/*Cookie:*/ "%s"
			"Connection: close\r\n"			//theoretically, this is not needed. but as our code will basically do it anyway, it might as well be here FIXME: implement connection reuse.
#ifdef AVAIL_GZDEC
			"Accept-Encoding: gzip\r\n"
#endif
			"User-Agent: "FULLENGINENAME"\r\n"
			"\r\n", uri, con->server, cookies);
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
static unsigned int dlthreads = 0;
#define MAXDOWNLOADTHREADS 4
#if defined(LOADERTHREAD) && !defined(NPFTE)
static void HTTP_Wake_Think(void *ctx, void *data, size_t a, size_t b)
{
	dlthreads--;
	HTTP_CL_Think();
}
#endif
static int DL_Thread_Work(void *arg)
{
	struct dl_download *dl = arg;

	while (dl->threadenable)
	{
		if (!dl->poll(dl))
		{
#ifdef NPFTE
			//the plugin doesn't have a download loop
			if (dl->notifycomplete)
			{
				dl->notifycomplete(dl);
				dl->notifycomplete = NULL;
			}
			if (dl->file)
				VFS_CLOSE(dl->file);
#else
			if (dl->status != DL_FAILED && dl->status != DL_FINISHED)
				dl->status = DL_FAILED;
#endif
			break;
		}
	}
	dl->threadenable = false;

#if defined(LOADERTHREAD) && !defined(NPFTE)
	COM_AddWork(WG_MAIN, HTTP_Wake_Think, NULL, NULL, 0, 0);
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

	dl->threadenable = true;
#if defined(LOADERTHREAD) && !defined(NPFTE)
	if (dlthreads < 0)
#endif
	{
		dl->threadctx = Sys_CreateThread("download", DL_Thread_Work, dl, THREADP_NORMAL, 0);
		if (!dl->threadctx)
			return false;
		dlthreads++;
	}

	return true;
}
#else
qboolean DL_CreateThread(struct dl_download *dl, vfsfile_t *file, void (*NotifyFunction)(struct dl_download *dl))
{
	if (!dl)
		return false;

	if (file)
		dl->file = file;
	if (NotifyFunction)
		dl->notifycomplete = NotifyFunction;

	return false;
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
	newdl->sizelimit = 0x80000000u;	//some sanity limit.
#if !defined(NPFTE) && !defined(SERVERONLY)
	newdl->qdownload.method = DL_HTTP;
#endif

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
	struct dl_download **link = NULL;

#ifndef NPFTE
	for (link = &activedownloads; *link; link = &(*link)->next)
	{
		if (*link == dl)
		{
			*link = dl->next;
			break;
		}
	}
#endif

#if !defined(NPFTE) && !defined(SERVERONLY)
	if (cls.download == &dl->qdownload)
		cls.download = NULL;
#endif

#ifdef MULTITHREAD
	dl->threadenable = false;
	if (dl->threadctx)
		Sys_WaitOnThread(dl->threadctx);
#endif
	if (dl->file && dl->file->Seek)
		VFS_SEEK(dl->file, 0);
	if (dl->notifycomplete)
		dl->notifycomplete(dl);
	if (dl->abort)
		dl->abort(dl);
	if (dl->file)
		VFS_CLOSE(dl->file);
	if (dl->postdata)
		BZ_Free(dl->postdata);
	free(dl);
}

#ifndef NPFTE
void DL_DeThread(void)
{
#ifdef MULTITHREAD
	//if we're about to fork, ensure that any downloads are properly parked so that there's no workers in an unknown state.
	struct dl_download *dl;
	for (dl = activedownloads; dl; dl = dl->next)
	{
		dl->threadenable = false;
		if (dl->threadctx)
			Sys_WaitOnThread(dl->threadctx);
	}
#endif
}

/*updates pending downloads*/
unsigned int HTTP_CL_GetActiveDownloads(void)
{
	struct dl_download *dl;
	unsigned int count = 0;

	for (dl = activedownloads; dl; dl = dl->next)
		count++;
	return count;
}

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
	if (dl)
	{
		Q_strncpyz(dl->postmimetype, mime, sizeof(dl->postmimetype));
		dl->postdata = BZ_Malloc(datalen);
		memcpy(dl->postdata, data, datalen);
		dl->postlen = datalen;
	}
	return dl;
}

void HTTP_CL_Think(void)
{
	struct dl_download *dl = activedownloads;
	struct dl_download **link = NULL;
#ifndef SERVERONLY
	float currenttime;
	if (!activedownloads)
		return;
	currenttime = Sys_DoubleTime();
#endif

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
		else if (dl->threadenable)
		{
			if (dlthreads < MAXDOWNLOADTHREADS)
			{
				dl->threadctx = Sys_CreateThread("download", DL_Thread_Work, dl, THREADP_NORMAL, 0);
				if (dl->threadctx)
					dlthreads++;
				else
					dl->threadenable = false;
			}
		}
		else
#endif
		{
			if (!dl->poll(dl))
			{
				*link = dl->next;
				DL_Close(dl);
				continue;
			}
		}
		link = &dl->next;

#ifndef SERVERONLY
		if (!cls.download && !dl->isquery)
#ifdef MULTITHREAD
		if (!dl->threadenable || dl->threadctx)	//don't show pending downloads in preference to active ones.
#endif
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

		if (dl->qdownload.ratetime < currenttime)
		{
			dl->qdownload.ratetime = currenttime+1;
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

#ifdef COOKIECOOKIECOOKIE
	Cookie_Monster();
#endif
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
	void *mutex;
} vfspipe_t;

static qboolean QDECL VFSPIPE_Close(vfsfile_t *f)
{
	vfspipe_t *p = (vfspipe_t*)f;
	free(p->data);
	Sys_DestroyMutex(p->mutex);
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
	Sys_LockMutex(p->mutex);
	if (len > p->writepos - p->readpos)
		len = p->writepos - p->readpos;
	memcpy(buffer, p->data+p->readpos, len);
	p->readpos += len;
	Sys_UnlockMutex(p->mutex);
	return len;
}
static int QDECL VFSPIPE_WriteBytes(vfsfile_t *f, const void *buffer, int len)
{
	vfspipe_t *p = (vfspipe_t*)f;
	Sys_LockMutex(p->mutex);
	if (p->readpos > 8192)
	{	//don't grow infinitely if we're reading+writing at the same time
		memmove(p->data, p->data+p->readpos, p->writepos-p->readpos);
		p->writepos -= p->readpos;
		p->readpos = 0;
	}
	if (p->writepos + len > p->maxlen)
	{
		p->maxlen = p->writepos + len;
		if (p->maxlen < (p->writepos-p->readpos)*2)	//over-allocate a little
			p->maxlen = (p->writepos-p->readpos)*2;
		p->data = realloc(p->data, p->maxlen);
	}
	memcpy(p->data+p->writepos, buffer, len);
	p->writepos += len;
	Sys_UnlockMutex(p->mutex);
	return len;
}

vfsfile_t *VFSPIPE_Open(void)
{
	vfspipe_t *newf;
	newf = malloc(sizeof(*newf));
	newf->mutex = Sys_CreateMutex();
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
