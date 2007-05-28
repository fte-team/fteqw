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

/*
This is the file responsible for handling incoming tcp connections.
This includes mvd recording.
Password checks and stuff are implemented here. This is server side stuff.

*/

#include "qtv.h"
#include "time.h"


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





void SV_FindProxies(SOCKET sock, cluster_t *cluster, sv_t *defaultqtv)
{
	oproxy_t *prox;

	sock = accept(sock, NULL, NULL);
	if (sock == INVALID_SOCKET)
		return;

	if (cluster->maxproxies >= 0 && cluster->numproxies >= cluster->maxproxies)
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

	cluster->numproxies++;

	prox->droptime = cluster->curtime + 5*1000;
#if 1
	prox->defaultstream = defaultqtv;

	prox->next = cluster->pendingproxies;
	cluster->pendingproxies = prox;
#else
	prox->next = qtv->pendingproxies;
	qtv->pendingproxies = prox;
	Net_SendConnectionMVD(qtv, prox);
#endif
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
		if (qerrno != EWOULDBLOCK && qerrno != EAGAIN)	//not a problem, so long as we can flush it later.
		{
			Sys_Printf(cluster, "oversize flush\n");
			prox->drop = true;	//drop them if we get any errors
		}
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
			printf("QTV client is too lagged\n");
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

	if (prox->buffersize-prox->bufferpos + length + msg.cursize > MAX_PROXY_BUFFER)
	{
		Net_TryFlushProxyBuffer(cluster, prox);	//try flushing
		if (prox->buffersize-prox->bufferpos + length + msg.cursize > MAX_PROXY_BUFFER)	//damn, still too big.
		{	//they're too slow. hopefully it was just momentary lag
			prox->flushing = true;
			return;
		}
	}


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

void Prox_SendInitialPlayers(sv_t *qtv, oproxy_t *prox, netmsg_t *msg)
{
	int i, j, flags;
	char buffer[64];

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!qtv->players[i].active) // interesting, is this set to false if player disconnect from server?
			continue;

		flags =   (DF_ORIGIN << 0) | (DF_ORIGIN << 1) | (DF_ORIGIN << 2)
				| (DF_ANGLES << 0) | (DF_ANGLES << 1) | (DF_ANGLES << 2) // angles is something what changed frequently, so may be not send it?
				| DF_EFFECTS
				| DF_SKINNUM // though it rare thingie, so better send it?
				| (qtv->players[i].dead   ? DF_DEAD : 0)
				| (qtv->players[i].gibbed ? DF_GIB  : 0)
				| DF_WEAPONFRAME // do we so really need it?
				| DF_MODEL; // generally, that why we wrote this function, so YES send this

		if (*qtv->players[i].userinfo && atoi(Info_ValueForKey(qtv->players[i].userinfo, "*spectator", buffer, sizeof(buffer))))
			flags = DF_MODEL; // oh, that spec, just sent his model, may be even better ignore him?

		WriteByte (msg, svc_playerinfo);
		WriteByte (msg, i);
		WriteShort (msg, flags);

		WriteByte (msg, qtv->players[i].current.frame); // always sent

		for (j = 0 ; j < 3 ; j++)
			if (flags & (DF_ORIGIN << j))
				WriteShort (msg, qtv->players[i].current.origin[j]);

		for (j = 0 ; j < 3 ; j++)
			if (flags & (DF_ANGLES << j))
				WriteShort (msg, qtv->players[i].current.angles[j]);

		if (flags & DF_MODEL) // generally, that why we wrote this function, so YES send this
			WriteByte (msg, qtv->players[i].current.modelindex);

		if (flags & DF_SKINNUM)
			WriteByte (msg, qtv->players[i].current.skinnum);

		if (flags & DF_EFFECTS)
			WriteByte (msg, qtv->players[i].current.effects);

		if (flags & DF_WEAPONFRAME)
			WriteByte (msg, qtv->players[i].current.weaponframe);
	}
}

void Net_SendConnectionMVD(sv_t *qtv, oproxy_t *prox)
{
	char buffer[MAX_MSGLEN*8];
	netmsg_t msg;
	int prespawn;

	if (!*qtv->mapname)
		return;

	InitNetMsg(&msg, buffer, sizeof(buffer));

	prox->flushing = false;

	BuildServerData(qtv, &msg, 0, NULL);
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

	//playerstates are delta-compressed, unfortunatly this isn't qwd (thanks to qqshka for showing my folly)
	Prox_SendInitialPlayers(qtv, prox, &msg);
	Prox_SendMessage(qtv->cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

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

	Net_ProxySend(qtv->cluster, prox, qtv->buffer, qtv->forwardpoint);	//send all the info we've not yet processed.


	if (prox->flushing)
	{
		Sys_Printf(qtv->cluster, "Connection data is too big, dropping proxy client\n");
		prox->drop = true;	//this is unfortunate...
	}
	else
		Net_TryFlushProxyBuffer(qtv->cluster, prox);
}


oproxy_t *Net_FileProxy(sv_t *qtv, char *filename)
{
	oproxy_t *prox;
	FILE *f;

	f = fopen(filename, "wb");
	if (!f)
		return NULL;

	//no full proxy check, this is going to be used by proxy admins, who won't want to have to raise the limit to start recording.

	prox = malloc(sizeof(*prox));
	if (!prox)
		return NULL;
	memset(prox, 0, sizeof(*prox));

	prox->sock = INVALID_SOCKET;
	prox->file = f;

	prox->next = qtv->proxies;
	qtv->proxies = prox;

	qtv->cluster->numproxies++;

	Net_SendConnectionMVD(qtv, prox);

	return prox;
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



void SV_ForwardStream(sv_t *qtv, char *buffer, int length)
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
		qtv->cluster->numproxies--;
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
			if (fre->srcfile)
				fclose(fre->srcfile);
			free(fre);
			qtv->cluster->numproxies--;
			prox->next = next;
		}

		if (prox->flushing)	//don't send it if we're trying to empty thier buffer.
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

//returns true if the pending proxy should be unlinked
//truth does not imply that it should be freed/released, just unlinked.
qboolean SV_ReadPendingProxy(cluster_t *cluster, oproxy_t *pend)
{
	char tempbuf[512];
	char *s;
	char *e;
	char *colon;
	float clientversion = 0;
	int len;
	int headersize;
	qboolean raw;
	sv_t *qtv;

	if (pend->drop)
	{
		if (pend->srcfile)
			fclose(pend->srcfile);
		closesocket(pend->sock);
		free(pend);
		cluster->numproxies--;
		return true;
	}
#define QTVSVHEADER "QTVSV 1.1\n"

	Net_TryFlushProxyBuffer(cluster, pend);

	if (pend->flushing)
	{
		if (pend->bufferpos == pend->buffersize)
		{
			if (pend->srcfile)
			{
				char buffer[4096];
				len = fread(buffer, 1, sizeof(buffer), pend->srcfile);
				if (!len)
				{
					fclose(pend->srcfile);
					pend->srcfile = NULL;
				}
				Net_ProxySend(cluster, pend, buffer, len);
				return false;	//don't try reading anything yet
			}
			else
			{
				pend->drop = true;
				return false;
			}
		}
		else
			return false;
	}

	if (pend->droptime < cluster->curtime)
	{
		pend->drop = true;
		return false;
	}

	len = sizeof(pend->inbuffer) - pend->inbuffersize - 1;
	len = recv(pend->sock, pend->inbuffer+pend->inbuffersize, len, 0);
	if (len == 0)
	{
		pend->drop = true;
		return false;
	}
	if (len < 0)
		return false;

	pend->inbuffersize += len;
	pend->inbuffer[pend->inbuffersize] = '\0';

	if (pend->inbuffersize >= 4)
	{
		if (strncmp(pend->inbuffer, "QTV\r", 4) && strncmp(pend->inbuffer, "QTV\n", 4) && strncmp(pend->inbuffer, "GET ", 4) && strncmp(pend->inbuffer, "POST ", 5))
		{	//I have no idea what the smeg you are.
			pend->drop = true;
			return false;
		}
	}

	//make sure there's a double \n somewhere

	for (s = pend->inbuffer; *s; s++)
	{
		if (s[0] == '\n' && (s[1] == '\n' || (s[1] == '\r' && s[2] == '\n')))
			break;
	}
	if (!*s)
		return false;	//don't have enough yet
	s+=3;
	headersize = s - pend->inbuffer - 1;

	if (!strncmp(pend->inbuffer, "POST ", 5))
	{
		HTTPSV_PostMethod(cluster, pend, s);

		return false;	//not keen on this..
	}
	else if (!strncmp(pend->inbuffer, "GET ", 4))
	{
		HTTPSV_GetMethod(cluster, pend);

		pend->flushing = true;
		return false;
	}

	raw = false;

	qtv = pend->defaultstream;

	e = pend->inbuffer;
	s = e;
	while(*e)
	{
		if (*e == '\n' || *e == '\r')
		{
			*e = '\0';
			colon = strchr(s, ':');
			if (*s)
			{
				if (!colon)
				{
					if (!strcmp(s, "QTV"))
					{
						//just a qtv request (as in, not http or some other protocol)
					}
					else if (!strcmp(s, "SOURCELIST"))
					{	//lists sources that are currently playing
						s = QTVSVHEADER;
							Net_ProxySend(cluster, pend, s, strlen(s));
						if (!cluster->servers)
						{
							s = "PERROR: No sources currently available\n";
								Net_ProxySend(cluster, pend, s, strlen(s));
						}
						else
						{
							for (qtv = cluster->servers; qtv; qtv = qtv->next)
							{
								s = tempbuf;
								if (clientversion > 1)
								{
									int plyrs = 0;
									int i;
									for (i = 0; i < MAX_CLIENTS; i++)
									{
										if (*qtv->players[i].userinfo)
											plyrs++;
									}
									sprintf(tempbuf, "SRCSRV: %s\n", qtv->server);
									Net_ProxySend(cluster, pend, s, strlen(s));
									sprintf(tempbuf, "SRCHOST: %s\n", qtv->hostname);
									Net_ProxySend(cluster, pend, s, strlen(s));
									sprintf(tempbuf, "SRCPLYRS: %i\n", plyrs);
									Net_ProxySend(cluster, pend, s, strlen(s));
									sprintf(tempbuf, "SRCVIEWS: %i\n", qtv->numviewers);
									Net_ProxySend(cluster, pend, s, strlen(s));
									sprintf(tempbuf, "SRCID: %i\n", qtv->streamid);	//final part of each source
									Net_ProxySend(cluster, pend, s, strlen(s));
								}
								else
								{
									sprintf(tempbuf, "ASOURCE: %i: %15s: %15s\n", qtv->streamid, qtv->server, qtv->hostname);
									Net_ProxySend(cluster, pend, s, strlen(s));
								}
							}
							qtv = NULL;
						}
						s = "\n";
							Net_ProxySend(cluster, pend, s, strlen(s));
						pend->flushing = true;
					}
					else if (!strcmp(s, "REVERSE"))
					{	//this is actually a server trying to connect to us
						//start up a new stream

						//FIXME: does this work?
#if 1	//left disabled until properly tested
						qtv = QTV_NewServerConnection(cluster, "reverse"/*server*/, "", true, 2, false, 0);

						s = QTVSVHEADER;		Net_ProxySend(cluster, pend, s, strlen(s));
						s = "REVERSED\n";		Net_ProxySend(cluster, pend, s, strlen(s));
						s = "VERSION: 1\n";		Net_ProxySend(cluster, pend, s, strlen(s));
						s = "\n";				Net_ProxySend(cluster, pend, s, strlen(s));

						//switch over the socket to the actual source connection rather than the pending
						Net_TryFlushProxyBuffer(cluster, pend); //flush anything... this isn't ideal, but should be small enough
						qtv->sourcesock = pend->sock;
						pend->sock = 0;

						memcpy(qtv->buffer, pend->inbuffer + headersize, pend->inbuffersize - headersize);
						qtv->parsingqtvheader = true;
						return false;
#endif
					}
					else if (!strcmp(s, "RECEIVE"))
					{	//a client connection request without a source
						if (cluster->numservers == 1)
						{	//only one stream anyway
							qtv = cluster->servers;
						}
						else
						{	//try and hunt down an explicit stream (rather than a user-recorded one)
							int numfound = 0;
							sv_t *suitable;
							for (qtv = cluster->servers; qtv; qtv = qtv->next)
							{
								if (!qtv->disconnectwhennooneiswatching)
								{
									suitable = qtv;
									numfound++;
								}
							}
							if (numfound == 1)
								qtv = suitable;
						}
						if (!qtv)
						{
							s = QTVSVHEADER;
								Net_ProxySend(cluster, pend, s, strlen(s));
							s = "PERROR: Multiple streams are currently playing\n";
								Net_ProxySend(cluster, pend, s, strlen(s));
							s = "\n";
								Net_ProxySend(cluster, pend, s, strlen(s));
							pend->flushing = true;
						}
					}
					else if (!strcmp(s, "DEMOLIST"))
					{	//lists sources that are currently playing
						int i;

						Cluster_BuildAvailableDemoList(cluster);

						s = QTVSVHEADER;
							Net_ProxySend(cluster, pend, s, strlen(s));
						if (!cluster->availdemoscount)
						{
							s = "PERROR: No demos currently available\n";
								Net_ProxySend(cluster, pend, s, strlen(s));
						}
						else
						{
							for (i = 0; i < cluster->availdemoscount; i++)
							{
								sprintf(tempbuf, "ADEMO: %i: %15s\n", cluster->availdemos[i].size, cluster->availdemos[i].name);
								s = tempbuf;
								Net_ProxySend(cluster, pend, s, strlen(s));
							}
							qtv = NULL;
						}
						s = "\n";
							Net_ProxySend(cluster, pend, s, strlen(s));
						pend->flushing = true;
					}
					else if (!strcmp(s, "AUTH"))
					{	//lists the demos available on this proxy
						//part of the connection process, can be ignored if there's no password
					}
					else
						printf("Unrecognised token in QTV connection request (%s)\n", s);
				}
				else
				{
					*colon++ = '\0';
					if (!strcmp(s, "VERSION"))
					{
						clientversion = atof(colon);
					}
					else if (!strcmp(s, "RAW"))
						raw = atoi(colon);
					/*else if (!strcmp(s, "ROUTE"))
					{	//pure rewroute...
						//is this safe? probably not.
						s = "QTVSV 1\n"
							"PERROR: ROUTE command not yet implemented\n"
							"\n";
						Net_ProxySend(cluster, pend, s, strlen(s));
						pend->flushing = true;
					}
					*/
					else if (!strcmp(s, "SOURCE"))
					{	//connects, creating a new source
						while (*colon == ' ')
							colon++;
						for (s = colon; *s; s++)
							if (*s < '0' || *s > '9')
								break;
						if (*s)
							qtv = QTV_NewServerConnection(cluster, colon, "", false, true, true, false);
						else
						{
							//numerical source, use a stream id.
							for (qtv = cluster->servers; qtv; qtv = qtv->next)
								if (qtv->streamid == atoi(colon))
									break;
						}
					}
					else if (!strcmp(s, "DEMO"))
					{	//starts a demo off the server... source does the same thing though...
						char buf[256];
	
						snprintf(buf, sizeof(buf), "demo:%s", colon);
						qtv = QTV_NewServerConnection(cluster, buf, "", false, true, true, false);
						if (!qtv)
						{
							s = QTVSVHEADER
								"PERROR: couldn't open demo\n"
								"\n";
							Net_ProxySend(cluster, pend, s, strlen(s));
							pend->flushing = true;
						}
					}
					else if (!strcmp(s, "AUTH"))
					{	//lists the demos available on this proxy
						//part of the connection process, can be ignored if there's no password
					}
					else
						printf("Unrecognised token in QTV connection request (%s)\n", s);
				}
			}
			s = e+1;
		}

		e++;
	}

	if (!pend->flushing)
	{
		if (clientversion < 1)
		{
			s = QTVSVHEADER
				"PERROR: Requested protocol version not supported\n"
				"\n";
			Net_ProxySend(cluster, pend, s, strlen(s));
			pend->flushing = true;
		}
		if (!qtv)
		{
			s = QTVSVHEADER
				"PERROR: No stream selected\n"
				"\n";
			Net_ProxySend(cluster, pend, s, strlen(s));
			pend->flushing = true;
		}
	}
	if (pend->flushing)
		return false;


	if (qtv->usequkeworldprotocols)
	{
		s = QTVSVHEADER
			"PERROR: This version of QTV is unable to convert QuakeWorld to QTV protocols\n"
			"\n";
		Net_ProxySend(cluster, pend, s, strlen(s));
		pend->flushing = true;
		return false;
	}
	if (cluster->maxproxies>=0 && cluster->numproxies >= cluster->maxproxies)
	{
		s = QTVSVHEADER
			"TERROR: This QTV has reached it's connection limit\n"
			"\n";
		Net_ProxySend(cluster, pend, s, strlen(s));
		pend->flushing = true;
		return false;
	}

	pend->next = qtv->proxies;
	qtv->proxies = pend;

	if (!raw)
	{
		s = QTVSVHEADER;
		Net_ProxySend(cluster, pend, s, strlen(s));
		s = "BEGIN: ";
		Net_ProxySend(cluster, pend, s, strlen(s));
		s = qtv->server;
		Net_ProxySend(cluster, pend, s, strlen(s));
		s = "\n\n";
		Net_ProxySend(cluster, pend, s, strlen(s));
	}
//	else if (passwordprotected)	//raw mode doesn't support passwords, so reject them
//	{
//		pend->flushing = true;
//		return;
//	}


	Net_SendConnectionMVD(qtv, pend);

	return true;
}
