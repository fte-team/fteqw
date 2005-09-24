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


#include "qtv.h"


qboolean	NET_StringToAddr (char *s, netadr_t *sadr)
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

		((struct sockaddr_in *)sadr)->sin_port = 0;

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

qboolean Net_ConnectToServer(sv_t *qtv, char *ip)
{
	netadr_t from;
	unsigned long nonblocking = true;

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
		printf("Unable to open file %s\n", ip+5);
		return false;
	}

	if (!NET_StringToAddr(ip, &qtv->serveraddress))
	{
		printf("Unable to resolve %s\n", ip);
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

	if (connect(qtv->sourcesock, (struct sockaddr *)&qtv->serveraddress, sizeof(qtv->serveraddress)) == INVALID_SOCKET)
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

	return true;
}

void Net_FindProxies(sv_t *qtv)
{
	oproxy_t *prox;
	int sock;

	sock = accept(qtv->listenmvd, NULL, NULL);
	if (sock == INVALID_SOCKET)
		return;

	prox = malloc(sizeof(*prox));
	memset(prox, 0, sizeof(*prox));
	prox->flushing = true;	//allow the buffer overflow resumption code to send the connection info.
	prox->sock = sock;

	prox->next = qtv->proxies;
	qtv->proxies = prox;
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

void Net_TryFlushProxyBuffer(oproxy_t *prox)
{
	char *buffer;
	int length;
	int bufpos;

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
		printf("oversize flush\n");

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

void Net_ProxySend(oproxy_t *prox, char *buffer, int length)
{
	int wrap;

	if (prox->buffersize-prox->bufferpos + length > MAX_PROXY_BUFFER)
	{
		Net_TryFlushProxyBuffer(prox);	//try flushing
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

void Prox_SendMessage(oproxy_t *prox, char *buf, int length, int dem_type, unsigned int playermask)
{
	netmsg_t msg;
	char tbuf[16];
	InitNetMsg(&msg, tbuf, sizeof(tbuf));
	WriteByte(&msg, 0);
	WriteByte(&msg, dem_type);
	WriteLong(&msg, length);
	if (dem_type == dem_multiple)
		WriteLong(&msg, playermask);


	Net_ProxySend(prox, msg.data, msg.cursize);

	Net_ProxySend(prox, buf, length);
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

	BuildServerData(qtv, &msg, true);
	Prox_SendMessage(prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	for (prespawn = 0;prespawn >= 0;)
	{
		prespawn = SendList(qtv, prespawn, qtv->soundlist, svc_soundlist, &msg);
		Prox_SendMessage(prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}
	
	for (prespawn = 0;prespawn >= 0;)
	{
		prespawn = SendList(qtv, prespawn, qtv->modellist, svc_modellist, &msg);
		Prox_SendMessage(prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	Net_TryFlushProxyBuffer(prox);	//that should be enough data to fill a packet.

	for(prespawn = 0;prespawn>=0;)
	{
		prespawn = Prespawn(qtv, 0, &msg, prespawn);

		Prox_SendMessage(prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	//playerstates arn't actually delta-compressed, so the first send (simply forwarded from server) entirly replaces the old.

	//we do need to send entity states.
	Prox_SendInitialEnts(qtv, prox, &msg);
	Prox_SendMessage(prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	WriteByte(&msg, svc_stufftext);
	WriteString(&msg, "skins\n");
	Prox_SendMessage(prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	Net_TryFlushProxyBuffer(prox);

	Prox_SendPlayerStats(qtv, prox);
	Net_TryFlushProxyBuffer(prox);

	if (!qtv->lateforward)
		Net_ProxySend(prox, qtv->buffer, qtv->buffersize);	//send all the info we've not yet processed.


	if (prox->flushing)
	{
		printf("Connection data is too big, dropping proxy client\n");
		prox->drop = true;	//this is unfortunate...
	}
	else
		Net_TryFlushProxyBuffer(prox);
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
				Net_TryFlushProxyBuffer(prox);	//try and flush it.
				continue;
			}
		}

		if (prox->drop)
			continue;
	
		//add the new data
		Net_ProxySend(prox, buffer, length);

		//and try to send it.
		Net_TryFlushProxyBuffer(prox);
		Net_TryFlushProxyBuffer(prox);
		Net_TryFlushProxyBuffer(prox);
	}
}

qboolean Net_ReadStream(sv_t *qtv)
{
	int maxreadable;
	int read;
	char *buffer;

	maxreadable = MAX_PROXY_BUFFER - qtv->buffersize;
	if (!maxreadable)
		return true;	//this is bad!
	buffer = qtv->buffer + qtv->buffersize;

	if (qtv->file)
	{
		if (maxreadable > PREFERED_PROXY_BUFFER-qtv->buffersize)
			maxreadable = PREFERED_PROXY_BUFFER-qtv->buffersize;
		if (!maxreadable)
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
		if (!qtv->lateforward)
			Net_ForwardStream(qtv, buffer, read);
	}
	else
	{
		if (read == 0 || qerrno != EWOULDBLOCK)
		{
			if (qtv->sourcesock != INVALID_SOCKET)
			{
				printf("Error: source socket error %i\n", qerrno);
				closesocket(qtv->sourcesock);
				qtv->sourcesock = INVALID_SOCKET;
			}
			if (qtv->file)
			{
				fclose(qtv->file);
				qtv->file = NULL;
			}
			printf("Read error or eof\n");
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
#else
	if (FD_ISSET(STDIN, &socketset))
#endif
	{
		char buffer[8192];
		char *result;
		char c;

#ifdef _WIN32
		if (!kbhit())
			break;
		else
			c = getch();
#else
		c = recv(STDIN, &c, 1, 0);
#endif

		if (c == '\n' || c == '\r')
		{
			printf("\n");
			if (tv->inputlength)
			{
				tv->commandinput[tv->inputlength] = '\0';
				result = Rcon_Command(tv, tv->commandinput, buffer, sizeof(buffer), true);
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
		}

		printf("\r%s \b", tv->commandinput);
	}
}

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
	Info_SetValueForStarKey(qtv->serverinfo, "hostname",	qtv->hostname,	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "maxclients",	"99",	sizeof(qtv->serverinfo));
	if (!strncmp(qtv->server, "file:", 5))
		Info_SetValueForStarKey(qtv->serverinfo, "server",		"file",	sizeof(qtv->serverinfo));
	else
		Info_SetValueForStarKey(qtv->serverinfo, "server",		qtv->server,	sizeof(qtv->serverinfo));

	memcpy(qtv->server, serverurl, sizeof(qtv->server)-1);

	if (!Net_ConnectToServer(qtv, qtv->server))
	{
		printf("Couldn't connect (%s)\n", qtv->server);
		return false;
	}
	printf("Connected\n");

	if (qtv->sourcesock == INVALID_SOCKET)
	{
		qtv->parsetime = Sys_Milliseconds();
		printf("Playing from file\n");
	}
	else
	{
		qtv->parsetime = Sys_Milliseconds() + BUFFERTIME*1000;
		printf("Buffering for %i seconds\n", BUFFERTIME);
	}
	return true;
}

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


	//make sure there's a use for this proxy.
	if (qtv.qwdsocket == INVALID_SOCKET)
	{	//still not opened one? try again
		qtv.qwdsocket = QW_InitUDPSocket(qtv.qwlistenportnum);
		if (qtv.qwdsocket == INVALID_SOCKET)
			printf("Warning: couldn't open udp socket\n");
	}
	if (qtv.listenmvd == INVALID_SOCKET)
	{
		qtv.listenmvd = Net_MVDListen(qtv.tcplistenportnum);
		if (qtv.listenmvd == INVALID_SOCKET)
			printf("Warning: couldn't open mvd socket\n");
	}
	if (qtv.qwdsocket == INVALID_SOCKET && qtv.listenmvd == INVALID_SOCKET)
	{
		printf("Shutting down, couldn't open listening ports (useless proxy)\n");
		return 0;
	}

	qtv.parsingconnectiondata = true;
	QTV_Run(&qtv);

	return 0;
}

