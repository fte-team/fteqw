/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#ifndef _WIN32
#include <signal.h>
#endif

#include "qtv.h"

#define RECONNECT_TIME (1000*30)
#define UDPRECONNECT_TIME (1000)
#define PINGSINTERVAL_TIME (1000*5)
#define UDPTIMEOUT_LENGTH (1000*20)
#define UDPPACKETINTERVAL (1000/72)

void Net_SendConnectionMVD(sv_t *qtv, oproxy_t *prox);


qboolean	NET_StringToAddr (char *s, netadr_t *sadr, int defaultport)
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];

	memset (sadr, 0, sizeof(netadr_t));

#ifdef USEIPX
	if ((strlen(s) >= 23) && (s[8] == ':') && (s[21] == ':'))	// check for an IPX address
	{
		unsigned int val;

		((struct sockaddr_ipx *)sadr)->sa_family = AF_IPX;
		copy[2] = 0;
		DO(0, sa_netnum[0]);
		DO(2, sa_netnum[1]);
		DO(4, sa_netnum[2]);
		DO(6, sa_netnum[3]);
		DO(9, sa_nodenum[0]);
		DO(11, sa_nodenum[1]);
		DO(13, sa_nodenum[2]);
		DO(15, sa_nodenum[3]);
		DO(17, sa_nodenum[4]);
		DO(19, sa_nodenum[5]);
		sscanf (&s[22], "%u", &val);
		((struct sockaddr_ipx *)sadr)->sa_socket = htons((unsigned short)val);
	}
	else
#endif
#ifdef IPPROTO_IPV6
		if (getaddrinfo)
	{
		struct addrinfo *addrinfo, *pos;
		struct addrinfo udp6hint;
		int error;
		char *port;
		char dupbase[256];
		int len;

		memset(&udp6hint, 0, sizeof(udp6hint));
		udp6hint.ai_family = 0;//Any... we check for AF_INET6 or 4
		udp6hint.ai_socktype = SOCK_DGRAM;
		udp6hint.ai_protocol = IPPROTO_UDP;

		port = s + strlen(s);
		while(port >= s)
		{
			if (*port == ':')
			break;
			port--;
		}

		if (port == s)
			port = NULL;
		if (port)
		{
			len = port - s;
			if (len >= sizeof(dupbase))
				len = sizeof(dupbase)-1;
			strncpy(dupbase, s, len);
			dupbase[len] = '\0';
			error = getaddrinfo(dupbase, port+1, &udp6hint, &addrinfo);
		}
		else
			error = EAI_NONAME;
		if (error)	//failed, try string with no port.
			error = getaddrinfo(s, NULL, &udp6hint, &addrinfo);	//remember, this func will return any address family that could be using the udp protocol... (ip4 or ip6)
		if (error)
		{
			return false;
		}
		((struct sockaddr*)sadr)->sa_family = 0;
		for (pos = addrinfo; pos; pos = pos->ai_next)
		{
			switch(pos->ai_family)
			{
			case AF_INET6:
				if (((struct sockaddr_in *)sadr)->sin_family == AF_INET6)
					break;	//first one should be best...
				//fallthrough
			case AF_INET:
				memcpy(sadr, addrinfo->ai_addr, addrinfo->ai_addrlen);
				if (pos->ai_family == AF_INET)
					goto dblbreak;	//don't try finding any more, this is quake, they probably prefer ip4...
				break;
			}
		}
dblbreak:
		pfreeaddrinfo (addrinfo);
		if (!((struct sockaddr*)sadr)->sa_family)	//none suitablefound
			return false;
	}
	else
#endif
	{
		((struct sockaddr_in *)sadr)->sin_family = AF_INET;

		((struct sockaddr_in *)sadr)->sin_port = htons(defaultport);

		strcpy (copy, s);
		// strip off a trailing :port if present
		for (colon = copy ; *colon ; colon++)
			if (*colon == ':')
			{
				*colon = 0;
				((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));
			}

		if (copy[0] >= '0' && copy[0] <= '9')	//this is the wrong way to test. a server name may start with a number.
		{
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
		}
		else
		{
			if (! (h = gethostbyname(copy)) )
				return 0;
			if (h->h_addrtype != AF_INET)
				return 0;
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
		}
	}

	return true;
}

qboolean Net_CompareAddress(netadr_t *s1, netadr_t *s2, int qp1, int qp2)
{
	struct sockaddr_in *i1=(void*)s1, *i2=(void*)s2;
	if (i1->sin_family != i2->sin_family)
		return false;
	if (i1->sin_family == AF_INET)
	{
		if (*(unsigned int*)&i1->sin_addr != *(unsigned int*)&i2->sin_addr)
			return false;
		if (i1->sin_port != i2->sin_port && qp1 != qp2)	//allow qports to match instead of ports, if required.
			return false;
		return true;
	}
	return false;
}

SOCKET Net_MVDListen(int port)
{
	int sock;

	struct sockaddr_in	address;
//	int fromlen;

	unsigned long nonblocking = true;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((u_short)port);



	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		return INVALID_SOCKET;
	}

	if (ioctlsocket (sock, FIONBIO, &nonblocking) == -1)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}

	if( bind (sock, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}

	listen(sock, 2);	//don't listen for too many clients.

	return sock;
}

qboolean Net_ConnectToTCPServer(sv_t *qtv, char *ip)
{
	netadr_t from;
	unsigned long nonblocking = true;

	if (!NET_StringToAddr(ip+4, &qtv->serveraddress, 27500))
	{
		Sys_Printf(qtv->cluster, "Unable to resolve %s\n", ip);
		return false;
	}
	qtv->sourcesock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (qtv->sourcesock == INVALID_SOCKET)
		return false;

	memset(&from, 0, sizeof(from));
	((struct sockaddr*)&from)->sa_family = ((struct sockaddr*)&qtv->serveraddress)->sa_family;
	if (bind(qtv->sourcesock, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
		return false;
	}

	if (ioctlsocket (qtv->sourcesock, FIONBIO, &nonblocking) == -1)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
		return false;
	}

	if (connect(qtv->sourcesock, (struct sockaddr *)&qtv->serveraddress, sizeof(qtv->serveraddress)) == INVALID_SOCKET)
	{
		if (qerrno != EINPROGRESS && qerrno != EWOULDBLOCK)	//bsd sockets are meant to return EINPROGRESS, but some winsock drivers use EWOULDBLOCK instead. *sigh*...
		{
			closesocket(qtv->sourcesock);
			qtv->sourcesock = INVALID_SOCKET;
			return false;
		}
	}
	return true;
}
qboolean Net_ConnectToUDPServer(sv_t *qtv, char *ip)
{
	netadr_t from;
	unsigned long nonblocking = true;

	if (!NET_StringToAddr(ip+4, &qtv->serveraddress, 27500))
	{
		Sys_Printf(qtv->cluster, "Unable to resolve %s\n", ip);
		return false;
	}
	qtv->sourcesock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (qtv->sourcesock == INVALID_SOCKET)
		return false;

	memset(&from, 0, sizeof(from));
	((struct sockaddr*)&from)->sa_family = ((struct sockaddr*)&qtv->serveraddress)->sa_family;
	if (bind(qtv->sourcesock, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
		return false;
	}

	if (ioctlsocket (qtv->sourcesock, FIONBIO, &nonblocking) == -1)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
		return false;
	}

	qtv->qport = Sys_Milliseconds()*1000+Sys_Milliseconds();

	return true;
}

qboolean Net_ConnectToServer(sv_t *qtv, char *ip)
{
	qtv->usequkeworldprotocols = false;

	if (!strncmp(ip, "file:", 5))
	{
		qtv->sourcesock = INVALID_SOCKET;
		qtv->file = fopen(ip+5, "rb");
		if (qtv->file)
		{
			fseek(qtv->file, 0, SEEK_END);
			qtv->filelength = ftell(qtv->file);
			fseek(qtv->file, 0, SEEK_SET);
			return true;
		}
		Sys_Printf(qtv->cluster, "Unable to open file %s\n", ip+5);
		return false;
	}

	qtv->nextconnectattempt = qtv->curtime + RECONNECT_TIME;	//wait half a minuite before trying to reconnect

	if (!strncmp(ip, "udp:", 4))
	{
		qtv->usequkeworldprotocols = true;
		return Net_ConnectToUDPServer(qtv, ip);
	}
	else if (!strncmp(ip, "tcp:", 4))
		return Net_ConnectToTCPServer(qtv, ip);
	else
	{
		Sys_Printf(qtv->cluster, "Unknown source type %s\n", ip);
		return false;
	}
}

void Net_FindProxies(sv_t *qtv)
{
	oproxy_t *prox;
	int sock;

	sock = accept(qtv->listenmvd, NULL, NULL);
	if (sock == INVALID_SOCKET)
		return;

	if (qtv->numproxies >= qtv->cluster->maxproxies && qtv->cluster->maxproxies)
	{
		const char buffer[] = {dem_all, 1, 'P','r','o','x','y',' ','i','s',' ','f','u','l','l','.'};
		send(sock, buffer, strlen(buffer), 0);
		closesocket(sock);
		return;
	}

	prox = malloc(sizeof(*prox));
	if (!prox)
	{//out of mem?
		closesocket(sock);
		return;
	}
	memset(prox, 0, sizeof(*prox));
	prox->sock = sock;
	prox->file = NULL;

	prox->next = qtv->proxies;
	qtv->proxies = prox;

	qtv->numproxies++;

	Net_SendConnectionMVD(qtv, prox);
}

qboolean Net_FileProxy(sv_t *qtv, char *filename)
{
	oproxy_t *prox;
	FILE *f;

	f = fopen(filename, "wb");
	if (!f)
		return false;

	//no full proxy check, this is going to be used by proxy admins, who won't want to have to raise the limit to start recording.

	prox = malloc(sizeof(*prox));
	if (!prox)
		return false;
	memset(prox, 0, sizeof(*prox));

	prox->sock = INVALID_SOCKET;
	prox->file = f;

	prox->next = qtv->proxies;
	qtv->proxies = prox;

	qtv->numproxies++;

	Net_SendConnectionMVD(qtv, prox);

	return true;
}

qboolean Net_StopFileProxy(sv_t *qtv)
{
	oproxy_t *prox;
	for (prox = qtv->proxies; prox; prox = prox->next)
	{
		if (prox->file)
		{
			prox->drop = true;
			return true;
		}
	}
	return false;
}

#undef IN
#define IN(x) buffer[(x)&(MAX_PROXY_BUFFER-1)]

void CheckMVDConsistancy(unsigned char *buffer, int pos, int size)
{
/*
	int length;
	int msec, type;
	while(pos < size)
	{
		msec = IN(pos++);
		type = IN(pos++);
		if (type == dem_set)
		{
			pos+=8;
			continue;
		}
		if (type == dem_multiple)
			pos+=4;
		length = (IN(pos+0)<<0) + (IN(pos+1)<<8) + (IN(pos+2)<<16) + (IN(pos+3)<<24);
		pos+=4;
		if (length > 1450)
			printf("too big (%i)\n", length);
		pos += length;
	}
	if (pos != size)
		printf("pos != size\n");
	*/
}

void Net_TryFlushProxyBuffer(cluster_t *cluster, oproxy_t *prox)
{
	char *buffer;
	int length;
	int bufpos;

	if (prox->drop)
		return;

	while (prox->bufferpos >= MAX_PROXY_BUFFER)
	{	//so we never get any issues with wrapping..
		prox->bufferpos -= MAX_PROXY_BUFFER;
		prox->buffersize -= MAX_PROXY_BUFFER;
	}

	bufpos = prox->bufferpos&(MAX_PROXY_BUFFER-1);
	length = prox->buffersize - prox->bufferpos;
	if (length > MAX_PROXY_BUFFER-bufpos)	//cap the length correctly.
		length = MAX_PROXY_BUFFER-bufpos;
	if (!length)
		return;	//already flushed.
	buffer = prox->buffer + bufpos;

//	CheckMVDConsistancy(prox->buffer, prox->bufferpos, prox->buffersize);

	if (bufpos+length > MAX_PROXY_BUFFER)
		Sys_Printf(cluster, "oversize flush\n");

	if (prox->file)
		length = fwrite(buffer, 1, length, prox->file);
	else
		length = send(prox->sock, buffer, length, 0);


	switch (length)
	{
	case 0:	//eof / they disconnected
		prox->drop = true;
		break;
	case -1:
		if (qerrno != EWOULDBLOCK)	//not a problem, so long as we can flush it later.
			prox->drop = true;	//drop them if we get any errors
		break;
	default:
		prox->bufferpos += length;
	}
}

void Net_ProxySend(cluster_t *cluster, oproxy_t *prox, char *buffer, int length)
{
	int wrap;

	if (prox->buffersize-prox->bufferpos + length > MAX_PROXY_BUFFER)
	{
		Net_TryFlushProxyBuffer(cluster, prox);	//try flushing
		if (prox->buffersize-prox->bufferpos + length > MAX_PROXY_BUFFER)	//damn, still too big.
		{	//they're too slow. hopefully it was just momentary lag
			prox->flushing = true;
			return;
		}
	}
#if 1
	//just simple
	prox->buffersize+=length;
	for (wrap = prox->buffersize-length; wrap < prox->buffersize; wrap++)
		prox->buffer[wrap&(MAX_PROXY_BUFFER-1)] = *buffer++;
#else
	//we don't do multiple wrappings, the above check cannot succeed if it were required.

	//find the wrap point
	wrap = prox->buffersize-(prox->buffersize&(MAX_PROXY_BUFFER-1)) + MAX_PROXY_BUFFER;
	wrap = wrap - (prox->buffersize&(MAX_PROXY_BUFFER-1));	//the ammount of data we can fit before wrapping.

	if (wrap > length)
	{	//we don't wrap afterall
		memcpy(prox->buffer+(prox->buffersize)&(MAX_PROXY_BUFFER-1), buffer, length);
		prox->buffersize+=length;
		return;
	}
	memcpy(prox->buffer+prox->buffersize&(MAX_PROXY_BUFFER-1), buffer, wrap);
	buffer += wrap;
	length -= wrap;
	memcpy(prox->buffer, buffer, length);

	prox->buffersize+=length;
#endif
}

void Prox_SendMessage(cluster_t *cluster, oproxy_t *prox, char *buf, int length, int dem_type, unsigned int playermask)
{
	netmsg_t msg;
	char tbuf[16];
	InitNetMsg(&msg, tbuf, sizeof(tbuf));
	WriteByte(&msg, 0);
	WriteByte(&msg, dem_type);
	WriteLong(&msg, length);
	if (dem_type == dem_multiple)
		WriteLong(&msg, playermask);


	Net_ProxySend(cluster, prox, msg.data, msg.cursize);

	Net_ProxySend(cluster, prox, buf, length);
}

void Prox_SendPlayerStats(sv_t *qtv, oproxy_t *prox)
{
	char buffer[MAX_MSGLEN];
	netmsg_t msg;
	int player, snum;

	InitNetMsg(&msg, buffer, sizeof(buffer));

	for (player = 0; player < MAX_CLIENTS; player++)
	{
		for (snum = 0; snum < MAX_STATS; snum++)
		{
			if (qtv->players[player].stats[snum])
			{
				if ((unsigned)qtv->players[player].stats[snum] > 255)
				{
					WriteByte(&msg, svc_updatestatlong);
					WriteByte(&msg, snum);
					WriteLong(&msg, qtv->players[player].stats[snum]);
				}
				else
				{
					WriteByte(&msg, svc_updatestat);
					WriteByte(&msg, snum);
					WriteByte(&msg, qtv->players[player].stats[snum]);
				}
			}
		}

		if (msg.cursize)
		{
//			Prox_SendMessage(prox, msg.data, msg.cursize, dem_stats|(player<<3), (1<<player));
			msg.cursize = 0;
		}
	}
}

void Net_SendConnectionMVD(sv_t *qtv, oproxy_t *prox)
{
	char buffer[MAX_MSGLEN*8];
	netmsg_t msg;
	int prespawn;

	InitNetMsg(&msg, buffer, sizeof(buffer));

	prox->flushing = false;

	BuildServerData(qtv, &msg, true, 0);
	Prox_SendMessage(qtv->cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	for (prespawn = 0;prespawn >= 0;)
	{
		prespawn = SendList(qtv, prespawn, qtv->soundlist, svc_soundlist, &msg);
		Prox_SendMessage(qtv->cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	for (prespawn = 0;prespawn >= 0;)
	{
		prespawn = SendList(qtv, prespawn, qtv->modellist, svc_modellist, &msg);
		Prox_SendMessage(qtv->cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	Net_TryFlushProxyBuffer(qtv->cluster, prox);	//that should be enough data to fill a packet.

	for(prespawn = 0;prespawn>=0;)
	{
		prespawn = Prespawn(qtv, 0, &msg, prespawn, MAX_CLIENTS-1);

		Prox_SendMessage(qtv->cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	//playerstates arn't actually delta-compressed, so the first send (simply forwarded from server) entirly replaces the old.

	//we do need to send entity states.
	Prox_SendInitialEnts(qtv, prox, &msg);
	Prox_SendMessage(qtv->cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	WriteByte(&msg, svc_stufftext);
	WriteString(&msg, "skins\n");
	Prox_SendMessage(qtv->cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	Net_TryFlushProxyBuffer(qtv->cluster, prox);

	Prox_SendPlayerStats(qtv, prox);
	Net_TryFlushProxyBuffer(qtv->cluster, prox);

	if (!qtv->cluster->lateforward)
		Net_ProxySend(qtv->cluster, prox, qtv->buffer, qtv->buffersize);	//send all the info we've not yet processed.


	if (prox->flushing)
	{
		Sys_Printf(qtv->cluster, "Connection data is too big, dropping proxy client\n");
		prox->drop = true;	//this is unfortunate...
	}
	else
		Net_TryFlushProxyBuffer(qtv->cluster, prox);
}

void Net_ForwardStream(sv_t *qtv, char *buffer, int length)
{	//forward the stream on to connected clients
	oproxy_t *prox, *next, *fre;

	CheckMVDConsistancy(buffer, 0, length);


	while (qtv->proxies && qtv->proxies->drop)
	{
		next = qtv->proxies->next;
		fre = qtv->proxies;
		if (fre->file)
			fclose(fre->file);
		else
			closesocket(fre->sock);
		free(fre);
		qtv->numproxies--;
		qtv->proxies = next;
	}

	for (prox = qtv->proxies; prox; prox = prox->next)
	{
		while (prox->next && prox->next->drop)
		{
			next = prox->next->next;
			fre = prox->next;
			if (fre->file)
				fclose(fre->file);
			else
				closesocket(fre->sock);
			free(fre);
			qtv->numproxies--;
			prox->next = next;
		}

		if (prox->flushing)	//don't sent it if we're trying to empty thier buffer.
		{
			if (prox->buffersize == prox->bufferpos)
			{
				if (!qtv->parsingconnectiondata)
					Net_SendConnectionMVD(qtv, prox);	//they're up to date, resend the connection info.
			}
			else
			{
				Net_TryFlushProxyBuffer(qtv->cluster, prox);	//try and flush it.
				continue;
			}
		}

		if (prox->drop)
			continue;

		//add the new data
		Net_ProxySend(qtv->cluster, prox, buffer, length);

		Net_TryFlushProxyBuffer(qtv->cluster, prox);
//		Net_TryFlushProxyBuffer(qtv->cluster, prox);
//		Net_TryFlushProxyBuffer(qtv->cluster, prox);
	}
}

qboolean Net_ReadStream(sv_t *qtv)
{
	int maxreadable;
	int read;
	char *buffer;
	int err;

	maxreadable = MAX_PROXY_BUFFER - qtv->buffersize;
	if (!maxreadable)
		return true;	//this is bad!
	buffer = qtv->buffer + qtv->buffersize;

	if (qtv->file)
	{
		if (maxreadable > PREFERED_PROXY_BUFFER-qtv->buffersize)
			maxreadable = PREFERED_PROXY_BUFFER-qtv->buffersize;
		if (maxreadable<=0)
			return true;
		read = fread(buffer, 1, maxreadable, qtv->file);
	}
	else
	{
		read = recv(qtv->sourcesock, buffer, maxreadable, 0);
	}
	if (read > 0)
	{
		qtv->buffersize += read;
		if (!qtv->cluster->lateforward)
			Net_ForwardStream(qtv, buffer, read);
	}
	else
	{
		err = qerrno;
		if (read == 0 || (err != EWOULDBLOCK && err != ENOTCONN))	//ENOTCONN can be returned whilst waiting for a connect to finish.
		{
			if (qtv->sourcesock != INVALID_SOCKET)
			{
				int err;
				err = qerrno;
				if (qerrno)
					Sys_Printf(qtv->cluster, "Error: source socket error %i\n", qerrno);
				else
					Sys_Printf(qtv->cluster, "Error: server disconnected\n");
				closesocket(qtv->sourcesock);
				qtv->sourcesock = INVALID_SOCKET;
			}
			if (qtv->file)
			{
				fclose(qtv->file);
				qtv->file = NULL;
			}
			Sys_Printf(qtv->cluster, "Read error or eof to %s\n", qtv->server);
			return false;
		}
	}
	return true;
}

#define BUFFERTIME 10	//secords for arificial delay, so we can buffer things properly.

unsigned int Sys_Milliseconds(void)
{
#ifdef _WIN32
#pragma comment(lib, "winmm.lib")
	return timeGetTime();
#else
	//assume every other system follows standards.
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return ((unsigned)tv.tv_sec)*1000 + (((unsigned)tv.tv_usec)/1000);
#endif
}
/*
void NetSleep(sv_t *tv)
{
	int m;
	struct timeval timeout;
	fd_set socketset;

	FD_ZERO(&socketset);
	m = 0;
	if (tv->sourcesock != INVALID_SOCKET)
	{
		FD_SET(tv->sourcesock, &socketset);
		if (tv->sourcesock >= m)
			m = tv->sourcesock+1;
	}
	if (tv->qwdsocket != INVALID_SOCKET)
	{
		FD_SET(tv->qwdsocket, &socketset);
		if (tv->sourcesock >= m)
			m = tv->sourcesock+1;
	}

#ifndef _WIN32
	#ifndef STDIN
		#define STDIN 0
	#endif
	FD_SET(STDIN, &socketset);
	if (STDIN >= m)
		m = STDIN+1;
#endif

	timeout.tv_sec = 100/1000;
	timeout.tv_usec = (100%1000)*1000;

	select(m, &socketset, NULL, NULL, &timeout);

#ifdef _WIN32
	for (;;)
	{
		char buffer[8192];
		char *result;
		char c;

		if (!kbhit())
			break;
		c = getch();

		if (c == '\n' || c == '\r')
		{
			printf("\n");
			if (tv->inputlength)
			{
				tv->commandinput[tv->inputlength] = '\0';
				result = Rcon_Command(tv->cluster, tv, tv->commandinput, buffer, sizeof(buffer), true);
				printf("%s", result);
				tv->inputlength = 0;
				tv->commandinput[0] = '\0';
			}
		}
		else if (c == '\b')
		{
			if (tv->inputlength > 0)
			{
				tv->inputlength--;
				tv->commandinput[tv->inputlength] = '\0';
			}
		}
		else
		{
			if (tv->inputlength < sizeof(tv->commandinput)-1)
			{
				tv->commandinput[tv->inputlength++] = c;
				tv->commandinput[tv->inputlength] = '\0';
			}
		}if (FD_ISSET(STDIN, &socketset))
		printf("\r%s \b", tv->commandinput);
	}
#else
	if (FD_ISSET(STDIN, &socketset))
	{
		char buffer[8192];
		char *result;
		tv->inputlength = read (0, tv->commandinput, sizeof(tv->commandinput));
		if (tv->inputlength >= 1)
		{
			tv->commandinput[tv->inputlength-1] = 0;        // rip off the /n and terminate

			if (tv->inputlength)
			{
				tv->commandinput[tv->inputlength] = '\0';
				result = Rcon_Command(tv, tv->commandinput, buffer, sizeof(buffer), true);
				printf("%s", result);
				tv->inputlength = 0;
				tv->commandinput[0] = '\0';
			}
		}
	}
#endif
}
*/

void Trim(char *s)
{
	char *f;
	f = s;
	while(*f <= ' ' && *f > '\0')
		f++;
	while(*f > ' ')
		*s++ = *f++;
	*s = '\0';
}

qboolean QTV_Connect(sv_t *qtv, char *serverurl)
{
	if (qtv->sourcesock != INVALID_SOCKET)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
	}

	if (qtv->file)
	{
		fclose(qtv->file);
		qtv->file = NULL;
	}

	*qtv->serverinfo = '\0';
	Info_SetValueForStarKey(qtv->serverinfo, "*version",	"FTEQTV",	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "*qtv",		VERSION,	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "hostname",	qtv->cluster->hostname,	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "maxclients",	"99",	sizeof(qtv->serverinfo));
	if (!strncmp(qtv->server, "file:", 5))
		Info_SetValueForStarKey(qtv->serverinfo, "server",		"file",	sizeof(qtv->serverinfo));
	else
		Info_SetValueForStarKey(qtv->serverinfo, "server",		qtv->server,	sizeof(qtv->serverinfo));

	memcpy(qtv->server, serverurl, sizeof(qtv->server)-1);

	if (!Net_ConnectToServer(qtv, qtv->server))
	{
		Sys_Printf(qtv->cluster, "Couldn't connect (%s)\n", qtv->server);
		return false;
	}
	Sys_Printf(qtv->cluster, "Connected\n");

	if (qtv->sourcesock == INVALID_SOCKET)
	{
		qtv->parsetime = Sys_Milliseconds();
		Sys_Printf(qtv->cluster, "Playing from file\n");
	}
	else
	{
		qtv->parsetime = Sys_Milliseconds() + BUFFERTIME*1000;
		if (!qtv->usequkeworldprotocols)
			Sys_Printf(qtv->cluster, "Buffering for %i seconds\n", BUFFERTIME);
	}
	return true;
}

void QTV_Shutdown(sv_t *qtv)
{
	oproxy_t *prox;
	oproxy_t *old;
	viewer_t *v;
	sv_t *peer;
	cluster_t *cluster;
	Sys_Printf(qtv->cluster, "Shutting down %s\n", qtv->server);

	if (qtv->sourcesock != INVALID_SOCKET)
	{
		int err;
		err = qerrno;
		if (qerrno)
			Sys_Printf(qtv->cluster, "Error: source socket error %i\n", qerrno);
		else
			Sys_Printf(qtv->cluster, "Error: server disconnected\n");
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
	}
	if (qtv->file)
	{
		fclose(qtv->file);
		qtv->file = NULL;
	}
	if (qtv->listenmvd != INVALID_SOCKET)
		closesocket(qtv->listenmvd);
	BSP_Free(qtv->bsp);
	qtv->bsp = NULL;

	cluster = qtv->cluster;
	if (cluster->servers == qtv)
		cluster->servers = qtv->next;
	else
	{
		for (peer = cluster->servers; peer->next; peer = peer->next)
		{
			if (peer->next == qtv)
			{
				peer->next = qtv->next;
				break;
			}
		}
	}

	for (v = cluster->viewers; v; v = v->next)
	{
		if (v->server == qtv)
		{
			QW_SetViewersServer(v, NULL);
			v->menunum = 1;
		}
	}

	for (prox = qtv->proxies; prox; )
	{
		if (prox->file)
			fclose(prox->file);
		if (prox->sock != INVALID_SOCKET)
			closesocket(prox->sock);
		old = prox;
		prox = prox->next;
		free(old);
	}


	free(qtv);
	cluster->numservers--;
}

/*
void QTV_Run(sv_t *qtv)
{
	int lengthofs;
	unsigned int length;
	unsigned char *buffer;
	int oldcurtime;
	int packettime;

	while(1)
	{
		if (MAX_PROXY_BUFFER == qtv->buffersize)
		{	//our input buffer is full
			//so our receiving tcp socket probably has something waiting on it
			//so our select calls will never wait
			//so we're using close to 100% cpu
			//so we add some extra sleeping here.
#ifdef _WIN32
			Sleep(5);
#else
			usleep(5000);
#endif
		}

		NetSleep(qtv);

		if (qtv->sourcesock == INVALID_SOCKET && !qtv->file)
			if (!QTV_Connect(qtv, qtv->server))
			{
				QW_UpdateUDPStuff(qtv);
				continue;
			}

		Net_FindProxies(qtv);

		if (!Net_ReadStream(qtv))
		{
			QW_UpdateUDPStuff(qtv);
			continue;
		}

//we will read out as many packets as we can until we're up to date
//note: this can cause real issues when we're overloaded for any length of time
//each new packet comes with a leading msec byte (msecs from last packet)
//then a type, an optional destination mask, and a 4byte size.
//the 4 byte size is probably excessive, a short would do.
//some of the types have thier destination mask encoded inside the type byte, yielding 8 types, and 32 max players.

//if we've no got enough data to read a new packet, we print a message and wait an extra two seconds. this will add a pause, connected clients will get the same pause, and we'll just try to buffer more of the game before playing.
//we'll stay 2 secs behind when the tcp stream catches up, however. This could be bad especially with long up-time.
//All timings are in msecs, which is in keeping with the mvd times, but means we might have issues after 72 or so days.
//the following if statement will reset the parse timer. It might cause the game to play too soon, the buffersize checks in the rest of the function will hopefully put it back to something sensible.

		oldcurtime = qtv->curtime;
		qtv->curtime = Sys_Milliseconds();
		if (oldcurtime > qtv->curtime)
		{
			printf("Time wrapped\n");
			qtv->parsetime = qtv->curtime;
		}


		while (qtv->curtime >= qtv->parsetime)
		{
			if (qtv->buffersize < 2)
			{	//not enough stuff to play.
				if (qtv->parsetime < qtv->curtime)
				{
					qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
					printf("Not enough buffered\n");
				}
				break;
			}

			buffer = qtv->buffer;

			switch (qtv->buffer[1]&dem_mask)
			{
			case dem_set:
				if (qtv->buffersize < 10)
				{	//not enough stuff to play.
					qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
					printf("Not enough buffered\n");
					continue;
				}
				qtv->parsetime += buffer[0];	//well this was pointless
				memmove(qtv->buffer, qtv->buffer+10, qtv->buffersize-(10));
				qtv->buffersize -= 10;
				continue;
			case dem_multiple:
				lengthofs = 6;
				break;
			default:
				lengthofs = 2;
				break;
			}

			if (qtv->buffersize < lengthofs+4)
			{	//the size parameter doesn't fit.
				printf("Not enough buffered\n");
				qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
				break;
			}


			length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);
			if (length > 1450)
			{	//FIXME: THIS SHOULDN'T HAPPEN!
				//Blame the upstream proxy!
				printf("EGad! input packet (%i) too big! Flushing and reconnecting!\n", length);
				if (qtv->file)
				{
					fclose(qtv->file);
					qtv->file = NULL;
				}
				else
				{
					closesocket(qtv->sourcesock);
					qtv->sourcesock = INVALID_SOCKET;
				}
				qtv->buffersize = 0;
				break;
			}

			if (length+lengthofs+4 > qtv->buffersize)
			{
				printf("Not enough buffered\n");
				qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
				break;	//can't parse it yet.
			}

			qtv->nextpackettime = qtv->parsetime+buffer[0];

			if (qtv->nextpackettime < qtv->curtime)
			{
				if (qtv->lateforward)
					Net_ForwardStream(qtv, qtv->buffer, lengthofs+4+length);

				switch(qtv->buffer[1]&dem_mask)
				{
				case dem_multiple:
					ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1]&dem_mask, (buffer[lengthofs-4]<<0) + (buffer[lengthofs+3]<<8) + (buffer[lengthofs-2]<<16) + (buffer[lengthofs-1]<<24));
					break;
				case dem_single:
				case dem_stats:
					ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1]&dem_mask, 1<<(qtv->buffer[1]>>3));
					break;
				case dem_read:
				case dem_all:
					ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1]&dem_mask, 0xffffffff);
					break;
				default:
					printf("Message type %i\n", qtv->buffer[1]&dem_mask);
					break;
				}

				qtv->oldpackettime = qtv->curtime;

				packettime = buffer[0];
				if (qtv->buffersize)
				{	//svc_disconnect can flush our input buffer (to prevent the EndOfDemo part from interfering)
					memmove(qtv->buffer, qtv->buffer+lengthofs+4+length, qtv->buffersize-(lengthofs+length+4));
					qtv->buffersize -= lengthofs+4+length;
				}

				if (qtv->file)
					Net_ReadStream(qtv);

				qtv->parsetime += packettime;
			}
			else
				break;
		}

		QW_UpdateUDPStuff(qtv);


	}
}


int main(int argc, char **argv)
{
	FILE *f;


	sv_t qtv;


	char line[1024];
	char buffer[8192];
	char *res;

#ifdef _WIN32
	{
		WSADATA discard;
		WSAStartup(MAKEWORD(2,0), &discard);
	}
#endif


	memset(&qtv, 0, sizeof(qtv));
	//set up a default config
	qtv.tcplistenportnum = PROX_DEFAULTLISTENPORT;
	qtv.qwlistenportnum = PROX_DEFAULTLISTENPORT;
	strcpy(qtv.server, PROX_DEFAULTSERVER);
	strcpy(qtv.hostname, DEFAULT_HOSTNAME);
	qtv.chokeonnotupdated = true;

	qtv.qwdsocket = INVALID_SOCKET;
	qtv.listenmvd = INVALID_SOCKET;
	qtv.sourcesock = INVALID_SOCKET;

	qtv.maxviewers = 100;
	qtv.maxproxies = 100;


	line[sizeof(line)-1] = '\0';
	if (argc < 2)
		res = "ftv.cfg";
	else
		res = argv[1];
	f = fopen(res, "rt");
	if (!f)
		printf("Couldn't open config file \"%s\"\n", res);
	else
	{
		while(fgets(line, sizeof(line)-1, f))
		{
			res = Rcon_Command(&qtv, line, buffer, sizeof(buffer), true);
			printf("%s", res);
		}
		fclose(f);
	}


	if (qtv.qwdsocket == INVALID_SOCKET && qtv.listenmvd == INVALID_SOCKET)
	{
		//make sure there's a use for this proxy.
		if (qtv.qwdsocket == INVALID_SOCKET)
		{	//still not opened one? try again
			qtv.qwdsocket = QW_InitUDPSocket(qtv.qwlistenportnum);
			if (qtv.qwdsocket == INVALID_SOCKET)
				printf("Warning: couldn't open udp socket\n");
			else
				printf("Opened udp port %i\n", qtv.qwlistenportnum);
		}
		if (qtv.listenmvd == INVALID_SOCKET)
		{
			qtv.listenmvd = Net_MVDListen(qtv.tcplistenportnum);
			if (qtv.listenmvd == INVALID_SOCKET)
				printf("Warning: couldn't open mvd socket\n");
			else
				printf("Opened tcp port %i\n", qtv.tcplistenportnum);
		}
		if (qtv.qwdsocket == INVALID_SOCKET && qtv.listenmvd == INVALID_SOCKET)
		{
			printf("Shutting down, couldn't open listening ports (useless proxy)\n");
			return 0;
		}
	}

	qtv.parsingconnectiondata = true;
	QTV_Run(&qtv);

	return 0;
}

*/











void SendClientCommand(sv_t *qtv, char *fmt, ...)
{
	va_list		argptr;
	char buf[1024];

	va_start (argptr, fmt);
#ifdef _WIN32
	_vsnprintf (buf, sizeof(buf) - 1, fmt, argptr);
	buf[sizeof(buf) - 1] = '\0';
#else
	vsnprintf (buf, sizeof(buf), fmt, argptr);
#endif // _WIN32
	va_end (argptr);

	WriteByte(&qtv->netchan.message, clc_stringcmd);
	WriteString(&qtv->netchan.message, buf);
}






void ChooseFavoriteTrack(sv_t *tv)
{
	int frags, best, pnum;
	char buffer[64];

	frags = -10000;
	best = -1;
	if (tv->controller)
		best = tv->trackplayer;
	else
	{
		for (pnum = 0; pnum < MAX_CLIENTS; pnum++)
		{
			if (*tv->players[pnum].userinfo && !atoi(Info_ValueForKey(tv->players[pnum].userinfo, "*spectator", buffer, sizeof(buffer))))
			{
				if (tv->thisplayer == pnum)
					continue;
				if (frags < tv->players[pnum].frags)
				{
					best = pnum;
					frags = tv->players[pnum].frags;
				}
			}
		}
	}
	if (best != tv->trackplayer)
	{
		SendClientCommand (tv, "ptrack %i\n", best);
		tv->trackplayer = best;
	}
}






static const unsigned char chktbl[1024] = {
0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3
};


unsigned char	COM_BlockSequenceCRCByte (unsigned char *base, int length, int sequence)
{
	unsigned short crc;
	const unsigned char	*p;
	unsigned char chkb[60 + 4];

	p = chktbl + (sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = QCRC_Block(chkb, length);

	crc &= 0xff;

	return crc;
}
void SetMoveCRC(sv_t *qtv, netmsg_t *msg)
{
	char *outbyte;
	outbyte = msg->data + msg->startpos+1;

	*outbyte = COM_BlockSequenceCRCByte(
				outbyte+1, msg->cursize - (msg->startpos+2),
				qtv->netchan.outgoing_sequence);
}





void QTV_ParseQWStream(sv_t *qtv)
{
	char buffer[1500];
	netadr_t from;
	int fromlen;
	int readlen;
	netmsg_t msg;
	fromlen = sizeof(from);	//bug: this won't work on (free)bsd

	for (;;)
	{
		readlen = recvfrom(qtv->sourcesock, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&from, &fromlen);
		if (readlen < 0)
		{
			//FIXME: Check for error
			break;
		}
		buffer[readlen] = 0;
		if (*(int*)buffer == -1)
		{
			if (buffer[4] == 'c')
			{	//got a challenge
				strcpy(qtv->status, "Attemping connection\n");
				qtv->challenge = atoi(buffer+5);
				if (qtv->controller)
					sprintf(buffer, "connect %i %i %i \"%s\"", 28, qtv->qport, qtv->challenge, qtv->controller->userinfo);
				else
					sprintf(buffer, "connect %i %i %i \"%s\\name\\%s\"", 28, qtv->qport, qtv->challenge, "\\*ver\\fteqtv\\spectator\\1\\rate\\10000", qtv->cluster->hostname);
				Netchan_OutOfBand(qtv->cluster, qtv->sourcesock, qtv->serveraddress, strlen(buffer), buffer);
				continue;
			}
			if (buffer[4] == 'n')
			{
				strncpy(qtv->status, buffer+5, sizeof(qtv->status));
				qtv->status[sizeof(qtv->status)-1] = 0;
				Sys_Printf(qtv->cluster, "%s: %s", qtv->server, buffer+5);
				continue;
			}
			if (buffer[4] == 'j')
			{
				strcpy(qtv->status, "Waiting for gamestate\n");
				Netchan_Setup(qtv->sourcesock, &qtv->netchan, qtv->serveraddress, qtv->qport, true);

				qtv->trackplayer = -1;

				qtv->isconnected = true;
				qtv->timeout = qtv->curtime + UDPTIMEOUT_LENGTH;
				SendClientCommand(qtv, "new\n");
				Sys_Printf(qtv->cluster, "Connected!\n");
				continue;
			}
			Sys_Printf(qtv->cluster, "%s: unrecognised connectionless packet:\n%s\n", qtv->server, buffer+4);
			continue;
		}
		memset(&msg, 0, sizeof(msg));
		msg.cursize = readlen;
		msg.data = buffer;
		msg.maxsize = readlen;
		qtv->timeout = qtv->curtime + UDPTIMEOUT_LENGTH;
		if (!Netchan_Process(&qtv->netchan, &msg))
			continue;
		ParseMessage(qtv, msg.data + msg.readpos, msg.cursize - msg.readpos, dem_all, -1);

		qtv->oldpackettime = qtv->nextpackettime;
		qtv->nextpackettime = qtv->parsetime;
		qtv->parsetime = qtv->curtime;

		if (qtv->simtime < qtv->oldpackettime)
			qtv->simtime = qtv->oldpackettime;	//too old

		if (qtv->controller)
		{
			qtv->controller->maysend = true;
			qtv->controller->netchan.outgoing_sequence = qtv->netchan.incoming_sequence;
			qtv->controller->netchan.incoming_sequence = qtv->netchan.incoming_acknowledged;
		}
	}
}

void QTV_Run(sv_t *qtv)
{
	int lengthofs;
	unsigned int length;
	unsigned char *buffer;
	int oldcurtime;
	int packettime;

	if (qtv->drop || (qtv->disconnectwhennooneiswatching && qtv->numviewers == 0))
	{
		QTV_Shutdown(qtv);
		return;
	}

	
//we will read out as many packets as we can until we're up to date
//note: this can cause real issues when we're overloaded for any length of time
//each new packet comes with a leading msec byte (msecs from last packet)
//then a type, an optional destination mask, and a 4byte size.
//the 4 byte size is probably excessive, a short would do.
//some of the types have thier destination mask encoded inside the type byte, yielding 8 types, and 32 max players.


//if we've no got enough data to read a new packet, we print a message and wait an extra two seconds. this will add a pause, connected clients will get the same pause, and we'll just try to buffer more of the game before playing.
//we'll stay 2 secs behind when the tcp stream catches up, however. This could be bad especially with long up-time.
//All timings are in msecs, which is in keeping with the mvd times, but means we might have issues after 72 or so days.
//the following if statement will reset the parse timer. It might cause the game to play too soon, the buffersize checks in the rest of the function will hopefully put it back to something sensible.

	oldcurtime = qtv->curtime;
	qtv->curtime = Sys_Milliseconds();
	if (oldcurtime > qtv->curtime)
	{
		Sys_Printf(qtv->cluster, "Time wrapped\n");
		qtv->parsetime = qtv->curtime;
	}




	if (qtv->usequkeworldprotocols)
	{
		qtv->simtime += qtv->curtime - oldcurtime;

		if (qtv->simtime > qtv->nextpackettime)
			qtv->simtime = qtv->nextpackettime;	//too old

		if (!qtv->isconnected && (qtv->curtime >= qtv->nextconnectattempt || qtv->curtime < qtv->nextconnectattempt - UDPRECONNECT_TIME*2))
		{
			strcpy(qtv->status, "Attemping challenge\n");
			Netchan_OutOfBand(qtv->cluster, qtv->sourcesock, qtv->serveraddress, 13, "getchallenge\n");
			qtv->nextconnectattempt = qtv->curtime + UDPRECONNECT_TIME;
		}
		QTV_ParseQWStream(qtv);

		if (qtv->isconnected)
		{
			char buffer[128];
			netmsg_t msg;
			memset(&msg, 0, sizeof(msg));
			msg.data = buffer;
			msg.maxsize = sizeof(buffer);

			if (qtv->curtime >= qtv->timeout || qtv->curtime < qtv->timeout - UDPTIMEOUT_LENGTH*2)
			{
				Sys_Printf(qtv->cluster, "Timeout\n");
				qtv->isconnected = false;
				return;
			}

			if (qtv->controller)
			{
				qtv->netchan.outgoing_sequence = qtv->controller->netchan.incoming_sequence;
				qtv->netchan.incoming_sequence = qtv->controller->netchan.incoming_acknowledged;
				if (qtv->maysend)
				{
					qtv->maysend = false;
					qtv->curtime = qtv->packetratelimiter;
				}
				else
					qtv->curtime = qtv->packetratelimiter - 1;
			}
			else
			{
				if (qtv->curtime < qtv->packetratelimiter - UDPPACKETINTERVAL*2)
					qtv->packetratelimiter = qtv->curtime;
			}
			if (qtv->curtime >= qtv->packetratelimiter)
			{
				qtv->packetratelimiter += UDPPACKETINTERVAL;

				if (qtv->curtime >= qtv->nextsendpings || qtv->curtime < qtv->nextsendpings - PINGSINTERVAL_TIME*2)
				{
					qtv->nextsendpings = qtv->curtime + PINGSINTERVAL_TIME;
					SendClientCommand(qtv, "pings\n");

				}
				ChooseFavoriteTrack(qtv);

				if (qtv->trackplayer >= 0)
				{
					WriteByte(&msg, clc_tmove);
					WriteShort(&msg, qtv->players[qtv->trackplayer].current.origin[0]);
					WriteShort(&msg, qtv->players[qtv->trackplayer].current.origin[1]);
					WriteShort(&msg, qtv->players[qtv->trackplayer].current.origin[2]);
				}
				else if (qtv->controller)
				{
					WriteByte(&msg, clc_tmove);
					WriteShort(&msg, qtv->controller->origin[0]);
					WriteShort(&msg, qtv->controller->origin[1]);
					WriteShort(&msg, qtv->controller->origin[2]);

/*					qtv->controller->ucmds[0].angles[1] = qtv->curtime*120;
					qtv->controller->ucmds[1].angles[1] = qtv->curtime*120;
					qtv->controller->ucmds[2].angles[1] = qtv->curtime*120;
*/
					msg.startpos = msg.cursize;
					WriteByte(&msg, clc_move);
					WriteByte(&msg, 0);
					WriteByte(&msg, 0);
					WriteDeltaUsercmd(&msg, &nullcmd, &qtv->controller->ucmds[0]);
					WriteDeltaUsercmd(&msg, &qtv->controller->ucmds[0], &qtv->controller->ucmds[1]);
					WriteDeltaUsercmd(&msg, &qtv->controller->ucmds[1], &qtv->controller->ucmds[2]);

					SetMoveCRC(qtv, &msg);
				}

				Netchan_Transmit(qtv->cluster, &qtv->netchan, msg.cursize, msg.data);
			}
		}
		return;
	}
	else
		qtv->simtime = qtv->curtime;


	if (qtv->sourcesock == INVALID_SOCKET && !qtv->file)
	{
		if (qtv->curtime >= qtv->nextconnectattempt || qtv->curtime < qtv->nextconnectattempt - RECONNECT_TIME*2)
		if (!QTV_Connect(qtv, qtv->server))
		{
			return;
		}
	}


	Net_FindProxies(qtv);	//look for any other proxies wanting to muscle in on the action.

	if (qtv->file || qtv->sourcesock != INVALID_SOCKET)
	{
		if (!Net_ReadStream(qtv))
		{	//if we have an error reading it
			//if it's valid, give up
			//what should we do here?
			//obviously, we need to keep reading the stream to keep things smooth
		}
	}



	while (qtv->curtime >= qtv->parsetime)
	{
		if (qtv->buffersize < 2)
		{	//not enough stuff to play.
			if (qtv->parsetime < qtv->curtime)
			{
				qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
				if (qtv->file || qtv->sourcesock != INVALID_SOCKET)
					Sys_Printf(qtv->cluster, "Not enough buffered\n");
			}
			break;
		}

		buffer = qtv->buffer;

		switch (qtv->buffer[1]&dem_mask)
		{
		case dem_set:
			if (qtv->buffersize < 10)
			{	//not enough stuff to play.
				qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
				if (qtv->file || qtv->sourcesock != INVALID_SOCKET)
					Sys_Printf(qtv->cluster, "Not enough buffered\n");
				continue;
			}
			qtv->parsetime += buffer[0];	//well this was pointless
			memmove(qtv->buffer, qtv->buffer+10, qtv->buffersize-(10));
			qtv->buffersize -= 10;
			continue;
		case dem_multiple:
			lengthofs = 6;
			break;
		default:
			lengthofs = 2;
			break;
		}

		if (qtv->buffersize < lengthofs+4)
		{	//the size parameter doesn't fit.
			if (qtv->file || qtv->sourcesock != INVALID_SOCKET)
				Sys_Printf(qtv->cluster, "Not enough buffered\n");
			qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
			break;
		}


		length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);
		if (length > 1450)
		{	//FIXME: THIS SHOULDN'T HAPPEN!
			//Blame the upstream proxy!
			Sys_Printf(qtv->cluster, "Warning: corrupt input packet (%i) too big! Flushing and reconnecting!\n", length);
			if (qtv->file)
			{
				fclose(qtv->file);
				qtv->file = NULL;
			}
			else
			{
				closesocket(qtv->sourcesock);
				qtv->sourcesock = INVALID_SOCKET;
			}
			qtv->buffersize = 0;
			break;
		}

		if (length+lengthofs+4 > qtv->buffersize)
		{
			if (qtv->file || qtv->sourcesock != INVALID_SOCKET)
				Sys_Printf(qtv->cluster, "Not enough buffered\n");
			qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
			break;	//can't parse it yet.
		}

		qtv->nextpackettime = qtv->parsetime+buffer[0];

		if (qtv->nextpackettime < qtv->curtime)
		{
			if (qtv->cluster->lateforward)
				Net_ForwardStream(qtv, qtv->buffer, lengthofs+4+length);

			switch(qtv->buffer[1]&dem_mask)
			{
			case dem_multiple:
				ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1]&dem_mask, (buffer[lengthofs-4]<<0) + (buffer[lengthofs+3]<<8) + (buffer[lengthofs-2]<<16) + (buffer[lengthofs-1]<<24));
				break;
			case dem_single:
			case dem_stats:
				ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1]&dem_mask, 1<<(qtv->buffer[1]>>3));
				break;
			case dem_read:
			case dem_all:
				ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1]&dem_mask, 0xffffffff);
				break;
			default:
				Sys_Printf(qtv->cluster, "Message type %i\n", qtv->buffer[1]&dem_mask);
				break;
			}

			qtv->oldpackettime = qtv->curtime;

			packettime = buffer[0];
			if (qtv->buffersize)
			{	//svc_disconnect can flush our input buffer (to prevent the EndOfDemo part from interfering)
				memmove(qtv->buffer, qtv->buffer+lengthofs+4+length, qtv->buffersize-(lengthofs+length+4));
				qtv->buffersize -= lengthofs+4+length;
			}

			if (qtv->file)
				Net_ReadStream(qtv);

			qtv->parsetime += packettime;
		}
		else
			break;
	}
}



void Cluster_Run(cluster_t *cluster)
{
	sv_t *sv, *old;

		int m;
		struct timeval timeout;
		fd_set socketset;

		FD_ZERO(&socketset);
		m = 0;
		if (cluster->qwdsocket != INVALID_SOCKET)
		{
			FD_SET(cluster->qwdsocket, &socketset);
			if (cluster->qwdsocket >= m)
				m = cluster->qwdsocket+1;
		}

		for (sv = cluster->servers; sv; sv = sv->next)
		{
			if (sv->usequkeworldprotocols && sv->sourcesock != INVALID_SOCKET)
			{
				FD_SET(sv->sourcesock, &socketset);
				if (sv->sourcesock >= m)
					m = sv->sourcesock+1;
			}
		}

	#ifndef _WIN32
		#ifndef STDIN
			#define STDIN 0
		#endif
		FD_SET(STDIN, &socketset);
		if (STDIN >= m)
			m = STDIN+1;
	#endif

		timeout.tv_sec = 100/1000;
		timeout.tv_usec = (100%1000)*1000;

		m = select(m, &socketset, NULL, NULL, &timeout);


#ifdef _WIN32
		for (;;)
		{
			char buffer[8192];
			char *result;
			char c;

			if (!kbhit())
				break;
			c = getch();

			if (c == '\n' || c == '\r')
			{
				Sys_Printf(cluster, "\n");
				if (cluster->inputlength)
				{
					cluster->commandinput[cluster->inputlength] = '\0';
					result = Rcon_Command(cluster, NULL, cluster->commandinput, buffer, sizeof(buffer), true);
					Sys_Printf(cluster, "%s", result);
					cluster->inputlength = 0;
					cluster->commandinput[0] = '\0';
				}
			}
			else if (c == '\b')
			{
				if (cluster->inputlength > 0)
				{
					Sys_Printf(cluster, "%c", c);
					Sys_Printf(cluster, " ", c);
					Sys_Printf(cluster, "%c", c);

					cluster->inputlength--;
					cluster->commandinput[cluster->inputlength] = '\0';
				}
			}
			else
			{
				Sys_Printf(cluster, "%c", c);
				if (cluster->inputlength < sizeof(cluster->commandinput)-1)
				{
					cluster->commandinput[cluster->inputlength++] = c;
					cluster->commandinput[cluster->inputlength] = '\0';
				}
			}
		}
#else
		if (FD_ISSET(STDIN, &socketset))
		{
			char buffer[8192];
			char *result;
			cluster->inputlength = read (0, cluster->commandinput, sizeof(cluster->commandinput));
			if (cluster->inputlength >= 1)
			{
				cluster->commandinput[cluster->inputlength-1] = 0;        // rip off the /n and terminate

				if (cluster->inputlength)
				{
					cluster->commandinput[cluster->inputlength] = '\0';
					result = Rcon_Command(cluster, NULL, cluster->commandinput, buffer, sizeof(buffer), true);
					printf("%s", result);
					cluster->inputlength = 0;
					cluster->commandinput[0] = '\0';
				}
			}
		}
#endif





		for (sv = cluster->servers; sv; )
		{
			old = sv;
			sv = sv->next;
			QTV_Run(old);
		}

		QW_UpdateUDPStuff(cluster);
}

sv_t *QTV_NewServerConnection(cluster_t *cluster, char *server, qboolean force, qboolean autoclose, qboolean noduplicates)
{
	sv_t *qtv;

	if (noduplicates)
	{
		for (qtv = cluster->servers; qtv; qtv = qtv->next)
		{
			if (!strcmp(qtv->server, server))
				return qtv;
		}
	}

	qtv = malloc(sizeof(sv_t));
	if (!qtv)
		return NULL;

	memset(qtv, 0, sizeof(*qtv));
	//set up a default config
	qtv->tcplistenportnum = PROX_DEFAULTLISTENPORT;
	strcpy(qtv->server, PROX_DEFAULTSERVER);

	qtv->listenmvd = INVALID_SOCKET;
	qtv->sourcesock = INVALID_SOCKET;
	qtv->disconnectwhennooneiswatching = autoclose;
	qtv->parsingconnectiondata = true;

	qtv->cluster = cluster;
	qtv->next = cluster->servers;

	if (!QTV_Connect(qtv, server) && !force)
	{
		free(qtv);
		return NULL;
	}
	cluster->servers = qtv;
	cluster->numservers++;

	return qtv;
}

void DoCommandLine(cluster_t *cluster, int argc, char **argv)
{
	int i;
	char commandline[8192];
	char *start, *end, *result;
	char buffer[8192];

	commandline[0] = '\0';

	//build a block of strings.
	for (i = 1; i < argc; i++)
	{
		strcat(commandline, argv[i]);
		strcat(commandline, " ");
	}
	strcat(commandline, "+");
	
	start = commandline;
	while(start)
	{
		end = strchr(start+1, '+');
		if (end)
			*end = '\0';
		if (start[1])
		{
			result = Rcon_Command(cluster, NULL, start+1, buffer, sizeof(buffer), true);
			Sys_Printf(cluster, "%s", result);
		}

		start = end;
	}
}

int main(int argc, char **argv)
{
	cluster_t cluster;

#ifndef _WIN32
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
#endif

#ifdef _WIN32
	{
		WSADATA discard;
		WSAStartup(MAKEWORD(2,0), &discard);
	}
#endif

	memset(&cluster, 0, sizeof(cluster));

	cluster.qwdsocket = INVALID_SOCKET;
	cluster.qwlistenportnum = 0;
	strcpy(cluster.hostname, DEFAULT_HOSTNAME);


	DoCommandLine(&cluster, argc, argv);

	if (!cluster.numservers)
	{	//probably running on a home user's computer
		if (cluster.qwdsocket == INVALID_SOCKET && !cluster.qwlistenportnum)
		{
			cluster.qwdsocket = QW_InitUDPSocket(cluster.qwlistenportnum = 27599);
			if (cluster.qwdsocket != INVALID_SOCKET)
				Sys_Printf(&cluster, "opened port %i\n", cluster.qwlistenportnum);
		}

		Sys_Printf(&cluster, "\n"
			"Welcome to FTEQTV\n"
			"Please type\n"
			"qtv server:port\n"
			" to connect to a tcp server.\n"
			"qw server:port\n"
			" to connect to a regular qw server.\n"
			"demo qw/example.mvd\n"
			" to play a demo from an mvd.\n"
			"\n");
	}

	while (!cluster.wanttoexit)
		Cluster_Run(&cluster);

	return 0;
}


void Sys_Printf(cluster_t *cluster, char *fmt, ...)
{
	va_list		argptr;
	char		string[2024];
	
	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt,argptr);
	va_end (argptr);

	printf("%s", string);
}

