#include "quakedef.h"

#ifdef WEBSERVER

#include "iweb.h"

#include "netinc.h"

//FIXME: Before any admins use this for any serious usage, make the server send bits of file slowly.

static qboolean httpserverinitied = false;
qboolean httpserverfailed = false;
static int	httpserversocket;

typedef enum {HTTP_WAITINGFORREQUEST,HTTP_SENDING} http_mode_t;



qboolean HTTP_ServerInit(int port)
{
	struct sockaddr_in address;
	unsigned long _true = true;
	int i;

	if ((httpserversocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		IWebPrintf ("HTTP_ServerInit: socket: %s\n", strerror(qerrno));
		httpserverfailed = true;
		return false;
	}

	if (ioctlsocket (httpserversocket, FIONBIO, &_true) == -1)
	{
		IWebPrintf ("HTTP_ServerInit: ioctl FIONBIO: %s\n", strerror(qerrno));
		httpserverfailed = true;
		return false;
	}

	address.sin_family = AF_INET;
//check for interface binding option
	if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc)
	{
		address.sin_addr.s_addr = inet_addr(com_argv[i+1]);
		Con_TPrintf(TL_NETBINDINTERFACE,
				inet_ntoa(address.sin_addr));
	}
	else
		address.sin_addr.s_addr = INADDR_ANY;

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);

	if( bind (httpserversocket, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(httpserversocket);
		IWebPrintf("HTTP_ServerInit: failed to bind to socket\n");
		httpserverfailed = true;
		return false;
	}

	listen(httpserversocket, 3);

	httpserverinitied = true;
	httpserverfailed = false;

	IWebPrintf("HTTP server is running\n");
	return true;
}

void HTTP_ServerShutdown(void)
{
	closesocket(httpserversocket);
	IWebPrintf("HTTP server closed\n");

	httpserverinitied = false;
}

typedef struct HTTP_active_connections_s {
	int datasock;
	vfsfile_t *file;
	struct HTTP_active_connections_s *next;

	http_mode_t mode;
	qboolean modeswitched;
	qboolean closeaftertransaction;
	qboolean close;

	char *inbuffer;
	int inbuffersize;
	int inbufferused;

	char *outbuffer;
	int outbuffersize;
	int outbufferused;
} HTTP_active_connections_t;
static HTTP_active_connections_t *HTTP_ServerConnections;
static int httpconnectioncount;

static void ExpandInBuffer(HTTP_active_connections_t *cl, int quant, qboolean fixedsize)
{
	int newsize;
	if (fixedsize)
		newsize = quant;
	else
		newsize = cl->inbuffersize+quant;
	if (newsize <= cl->inbuffersize)
		return;

	cl->inbuffer = IWebRealloc(cl->inbuffer, newsize);
	cl->inbuffersize = newsize;
}
static void ExpandOutBuffer(HTTP_active_connections_t *cl, int quant, qboolean fixedsize)
{
	int newsize;
	if (fixedsize)
		newsize = quant;
	else
		newsize = cl->outbuffersize+quant;
	if (newsize <= cl->outbuffersize)
		return;

	cl->outbuffer = IWebRealloc(cl->outbuffer, newsize);
	cl->outbuffersize = newsize;
}

void HTTP_RunExisting (void)
{
	char *content;
	char *msg, *nl;
	char buf2[256];	//short lived temp buffer.
	char resource[256];
	char mode[8];
	qboolean hostspecified;
	int contentlen;

	int HTTPmarkup;	//version
	int localerrno;

	HTTP_active_connections_t **link, *cl;

	link = &HTTP_ServerConnections;
	for (link = &HTTP_ServerConnections; *link;)
	{
		int ammount, wanted;

		cl = *link;

		if (cl->close)
		{

			*link = cl->next;
			closesocket(cl->datasock);
			cl->datasock = INVALID_SOCKET;
			if (cl->inbuffer)
				IWebFree(cl->inbuffer);
			if (cl->outbuffer)
				IWebFree(cl->outbuffer);
			if (cl->file)
				VFS_CLOSE(cl->file);
			IWebFree(cl);
			httpconnectioncount--;
			continue;
		}

		link = &(*link)->next;

		switch(cl->mode)
		{
		case HTTP_WAITINGFORREQUEST:
			if (cl->outbufferused)
				Sys_Error("Persistant connection was waiting for input with unsent output");
			ammount = cl->inbuffersize - cl->inbufferused - 1;
			if (ammount < 128)
			{
				if (cl->inbuffersize>128*1024)
				{
					cl->close = true;	//that's just taking the piss.
					continue;
				}

				ExpandInBuffer(cl, 1500, false);
				ammount = cl->inbuffersize - cl->inbufferused - 1;
			}
			if (cl->modeswitched)
			{
				ammount = 0;
			}
			else
			{
				//we can't try and recv 0 bytes as we use an expanding buffer
				ammount = recv(cl->datasock, cl->inbuffer+cl->inbufferused, ammount, 0);
				if (ammount < 0)
				{
					if (qerrno != EWOULDBLOCK)	//they closed on us. Assume end.
					{
						cl->close = true;
					}
					continue;
				}
				if (ammount == 0)
				{
					cl->close = true;
					continue;
				}
			}
			cl->modeswitched = false;

			cl->inbufferused += ammount;
			cl->inbuffer[cl->inbufferused] = '\0';

			content = NULL;
			msg = cl->inbuffer;
			nl = strchr(msg, '\n');
			if (!nl)
			{
cont:
				continue;	//we need more... MORE!!! MORE I TELL YOU!!!!
			}

			msg = COM_ParseOut(msg, mode, sizeof(mode));

			msg = COM_ParseOut(msg, resource, sizeof(resource));

			if (!*resource)
			{
				cl->close = true;	//even if they forgot to specify a resource, we didn't find an HTTP so we have no option but to close.
				continue;
			}

			hostspecified = false;
			if (!strnicmp(resource, "http://", 7))
			{	//groan... 1.1 compliance requires parsing this correctly, without the client ever specifiying it.
				char *slash;	//we don't do multiple hosts.
				hostspecified=true;
				slash = strchr(resource+7, '/');
				if (!slash)
					strcpy(resource, "/");
				else
					memmove(resource, slash, strlen(slash+1));	//just get rid of the http:// stuff.
			}

			if (!strcmp(resource, "/"))
				strcpy(resource, "/index.html");

			msg = COM_ParseOut(msg, buf2, sizeof(buf2));
			contentlen = 0;
			if (!strnicmp(buf2, "HTTP/", 5))
			{
				if (!strncmp(buf2, "HTTP/1.1", 8))
					HTTPmarkup = 3;
				else if (!strncmp(buf2, "HTTP/1", 6))
					HTTPmarkup = 2;
				else
				{
					HTTPmarkup = 1;	//0.9... lamer.
					cl->closeaftertransaction = true;
				}

				//expect X lines containing options.
				//then a blank line. Don't continue till we have that.

				msg = nl+1;
				while (1)
				{
					if (*msg == '\r')
						msg++;
					if (*msg == '\n')
					{
						msg++;
						break;	//that was our blank line.
					}

					while(*msg == ' ')
						msg++;

					if (!strnicmp(msg, "Host: ", 6))	//parse needed header fields
						hostspecified = true;
					else if (!strnicmp(msg, "Content-Length: ", 16))	//parse needed header fields
						contentlen = atoi(msg+16);
					else if (!strnicmp(msg, "Transfer-Encoding: ", 18))	//parse needed header fields
					{
						cl->closeaftertransaction = true;
						goto notimplemented;
					}
					else if (!strnicmp(msg, "Connection: close", 17))
						cl->closeaftertransaction = true;

					while(*msg != '\n')
					{
						if (!*msg)
						{
							goto cont;
						}
						msg++;
					}
					msg++;
				}
			}
			else
			{
				HTTPmarkup = 0;	//strimmed... totally...
				cl->closeaftertransaction = true;
				//don't bother running to nl.
			}

			if (cl->inbufferused-(msg-cl->inbuffer) < contentlen)
				continue;

			cl->modeswitched = true;

			if (contentlen)
			{
				content = IWebMalloc(contentlen+1);
				memcpy(content, msg, contentlen+1);
			}

			memmove(cl->inbuffer, cl->inbuffer+(msg-cl->inbuffer+contentlen), cl->inbufferused-(msg-cl->inbuffer+contentlen));
			cl->inbufferused -= msg-cl->inbuffer+contentlen;


			if (HTTPmarkup == 3 && !hostspecified)	//1.1 requires the host to be specified... we ca,just ignore it as we're not routing or imitating two servers. (for complience we need to encourage the client to send - does nothing for compatability or anything, just compliance to spec. not always the same thing)
			{
				msg = "HTTP/1.1 400 Bad Request\r\n"	"Content-Type: text/plain\r\n"		"Content-Length: 69\r\n"	"Server: "FULLENGINENAME"/0\r\n"	"\r\n"	"400 Bad Request\r\nYour client failed to provide the host header line";

				ammount = strlen(msg);
				ExpandOutBuffer(cl, ammount, true);
				memcpy(cl->outbuffer, msg, ammount);
				cl->outbufferused = ammount;
				cl->mode = HTTP_SENDING;
			}
			else if (!stricmp(mode, "GET") || !stricmp(mode, "HEAD") || !stricmp(mode, "POST"))
			{
				if (*resource != '/')
				{
					resource[0] = '/';
					resource[1] = 0;	//I'm lazy, they need to comply
				}
				IWebPrintf("Download request for \"%s\"\n", resource+1);
				if (!strnicmp(mode, "P", 1))	//when stuff is posted, data is provided. Give an error message if we couldn't do anything with that data.
					cl->file = IWebGenerateFile(resource+1, content, contentlen);
				else
				{
					if (!SV_AllowDownload(resource+1))
						cl->file = NULL;
					else
						cl->file = FS_OpenVFS(resource+1, "rb", FS_GAME);

					if (!cl->file)
					{
						cl->file = IWebGenerateFile(resource+1, content, contentlen);
					}
				}

				if (!cl->file)
				{
					if (HTTPmarkup >= 3)
						msg = "HTTP/1.1 404 Not Found\r\n"	"Content-Type: text/plain\r\n"		"Content-Length: 15\r\n"	"Server: "FULLENGINENAME"/0\r\n"	"\r\n"	"404 Bad address";
					else if (HTTPmarkup == 2)
						msg = "HTTP/1.0 404 Not Found\r\n"	"Content-Type: text/plain\r\n"		"Content-Length: 15\r\n"	"Server: "FULLENGINENAME"/0\r\n"	"\r\n"	"404 Bad address";
					else if (HTTPmarkup)
						msg = "HTTP/0.9 404 Not Found\r\n"																						"\r\n"	"404 Bad address";
					else
						msg = "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD><BODY>404 Not Found<BR>The specified file could not be found on the server</HEAD></HTML>";

					ammount = strlen(msg);
					ExpandOutBuffer(cl, ammount, true);
					memcpy(cl->outbuffer, msg, ammount);
					cl->outbufferused = ammount;
					cl->mode = HTTP_SENDING;
				}
				else
				{
					if (HTTPmarkup>=3)
						sprintf(resource, "HTTP/1.1 200 OK\r\n"		"Content-Type: %s\r\n"		"Content-Length: %i\r\n"	"Server: "FULLENGINENAME"/0\r\n"	"\r\n", strstr(resource, ".htm")?"text/html":"text/plain", (int)VFS_GETLEN(cl->file));
					else if (HTTPmarkup==2)
						sprintf(resource, "HTTP/1.0 200 OK\r\n"		"Content-Type: %s\r\n"		"Content-Length: %i\r\n"	"Server: "FULLENGINENAME"/0\r\n"	"\r\n", strstr(resource, ".htm")?"text/html":"text/plain", (int)VFS_GETLEN(cl->file));
					else if (HTTPmarkup)
						sprintf(resource, "HTTP/0.9 200 OK\r\n\r\n");
					else
						strcpy(resource, "");
					msg = resource;

					if (*mode == 'H' || *mode == 'h')
					{

						VFS_CLOSE(cl->file);
						cl->file = NULL;
					}

					ammount = strlen(msg);
					ExpandOutBuffer(cl, ammount, true);
					memcpy(cl->outbuffer, msg, ammount);
					cl->outbufferused = ammount;
					cl->mode = HTTP_SENDING;
				}
			}
			//PUT/POST must support chunked transfer encoding for 1.1 compliance.
/*			else if (!stricmp(mode, "PUT"))	//put is replacement of a resource. (file uploads)
			{
			}
*/
			else
			{
notimplemented:
				if (HTTPmarkup >= 3)
					msg = "HTTP/1.1 501 Not Implemented\r\n\r\n";
				else if (HTTPmarkup == 2)
					msg = "HTTP/1.0 501 Not Implemented\r\n\r\n";
				else if (HTTPmarkup)
					msg = "HTTP/0.9 501 Not Implemented\r\n\r\n";
				else
				{
					msg = NULL;
					cl->close = true;
				}

				if (msg)
				{
					ammount = strlen(msg);
					ExpandOutBuffer(cl, ammount, true);
					memcpy(cl->outbuffer, msg, ammount);
					cl->outbufferused = ammount;
					cl->mode = HTTP_SENDING;
				}
			}

			if (content)
				IWebFree(content);
			break;

		case HTTP_SENDING:
			if (cl->outbufferused < 8192)
			{
				if (cl->file)
				{
					ExpandOutBuffer(cl, 32768, true);
					wanted = cl->outbuffersize - cl->outbufferused;
					ammount = VFS_READ(cl->file, cl->outbuffer+cl->outbufferused, wanted);

					if (!ammount)
					{
						VFS_CLOSE(cl->file);
						cl->file = NULL;

						IWebPrintf("Download complete\n");
					}
					else
						cl->outbufferused+=ammount;
				}
			}

			ammount = send(cl->datasock, cl->outbuffer, cl->outbufferused, 0);

			if (ammount == -1)
			{
				localerrno = qerrno;
				if (localerrno != EWOULDBLOCK)
				{
					cl->close = true;
				}
			}
			else if (ammount||!cl->outbufferused)
			{
				memcpy(cl->outbuffer, cl->outbuffer+ammount, cl->outbufferused-ammount);
				cl->outbufferused -= ammount;
				if (!cl->outbufferused && !cl->file)
				{
					cl->modeswitched = true;
					cl->mode = HTTP_WAITINGFORREQUEST;
					if (cl->closeaftertransaction)
						cl->close = true;
				}
			}
			else
				cl->close = true;
			break;

	/*	case HTTP_RECEIVING:
			sent = recv(cl->datasock, resource, ammount, 0);
			if (sent == -1)
			{
				if (qerrno != EWOULDBLOCK)	//they closed on us. Assume end.
				{
					VFS_CLOSE(cl->file);
					cl->file = NULL;
					cl->close = true;
					continue;
				}
			}
			if (sent != 0)
				IWebFWrite(resource, 1, sent, cl->file);
			break;*/
		}
	}
}

qboolean HTTP_ServerPoll(qboolean httpserverwanted, int portnum)	//loop while true
{
	struct sockaddr_qstorage	from;
	int		fromlen;
	int clientsock;
	int _true = true;
	char buf[128];
	netadr_t na;

	HTTP_active_connections_t *cl;

	if (!httpserverinitied)
	{
		if (httpserverwanted)
			return HTTP_ServerInit(portnum);
		return false;
	}
	else if (!httpserverwanted)
	{
		HTTP_ServerShutdown();
		return false;
	}

	if (httpconnectioncount>32)
		return false;

	fromlen = sizeof(from);
	clientsock = accept(httpserversocket, (struct sockaddr *)&from, &fromlen);

	if (clientsock == -1)
	{
		if (qerrno == EWOULDBLOCK)
		{
			HTTP_RunExisting();
			return false;
		}

		if (qerrno == ECONNABORTED || qerrno == ECONNRESET)
		{
			Con_TPrintf (TL_CONNECTIONLOSTORABORTED);
			return false;
		}


		Con_TPrintf (TL_NETGETPACKETERROR, strerror(qerrno));
		return false;
	}

	if (ioctlsocket (clientsock, FIONBIO, (u_long *)&_true) == -1)
	{
		IWebPrintf ("HTTP_ServerInit: ioctl FIONBIO: %s\n", strerror(qerrno));
		closesocket(clientsock);
		return false;
	}

#ifndef WEBSVONLY
	SockadrToNetadr(&from, &na);
	IWebPrintf("New http connection from %s\n", NET_AdrToString(buf, sizeof(buf), na));
#endif

	cl = IWebMalloc(sizeof(HTTP_active_connections_t));

	cl->datasock = clientsock;

	cl->next = HTTP_ServerConnections;
	HTTP_ServerConnections = cl;
	httpconnectioncount++;

	return true;
}

#endif
