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

#define ParseError(m) (m)->cursize = (m)->cursize+1	//

void InitNetMsg(netmsg_t *b, char *buffer, int bufferlength)
{
	b->data = buffer;
	b->maxsize = bufferlength;
	b->readpos = 0;
	b->cursize = 0;
}

//probably not the place for these any more..
unsigned char ReadByte(netmsg_t *b)
{
	if (b->readpos >= b->cursize)
	{
		b->readpos = b->cursize+1;
		return 0;
	}
	return b->data[b->readpos++];
}
unsigned short ReadShort(netmsg_t *b)
{
	int b1, b2;
	b1 = ReadByte(b);
	b2 = ReadByte(b);

	return b1 | (b2<<8);
}
unsigned int ReadLong(netmsg_t *b)
{
	int s1, s2;
	s1 = ReadShort(b);
	s2 = ReadShort(b);

	return s1 | (s2<<16);
}

unsigned int BigLong(unsigned int val)
{
	union {
		unsigned int i;
		unsigned char c[4];
	} v;

	v.i = val;
	return (v.c[0]<<24) | (v.c[1] << 16) | (v.c[2] << 8) | (v.c[3] << 0);
}

unsigned int SwapLong(unsigned int val)
{
	union {
		unsigned int i;
		unsigned char c[4];
	} v;
	unsigned char s;

	v.i = val;
	s = v.c[0];
	v.c[0] = v.c[3];
	v.c[3] = s;
	s = v.c[1];
	v.c[1] = v.c[2];
	v.c[2] = s;

	return v.i;
}

float ReadFloat(netmsg_t *b)
{
	union {
		unsigned int i;
		float f;
	} u;

	u.i = ReadLong(b);
	return u.f;
}
void ReadString(netmsg_t *b, char *string, int maxlen)
{
	maxlen--;	//for null terminator
	while(maxlen)
	{
		*string = ReadByte(b);
		if (!*string)
			return;
		string++;
		maxlen--;
	}
	*string++ = '\0';	//add the null
}

void WriteByte(netmsg_t *b, unsigned char c)
{
	if (b->cursize>=b->maxsize)
		return;
	b->data[b->cursize++] = c;
}
void WriteShort(netmsg_t *b, unsigned short l)
{
	WriteByte(b, (l&0x00ff)>>0);
	WriteByte(b, (l&0xff00)>>8);
}
void WriteLong(netmsg_t *b, unsigned int l)
{
	WriteByte(b, (l&0x000000ff)>>0);
	WriteByte(b, (l&0x0000ff00)>>8);
	WriteByte(b, (l&0x00ff0000)>>16);
	WriteByte(b, (l&0xff000000)>>24);
}
void WriteFloat(netmsg_t *b, float f)
{
	union {
		unsigned int i;
		float f;
	} u;

	u.f = f;
	WriteLong(b, u.i);
}
void WriteString2(netmsg_t *b, const char *str)
{	//no null terminator, convienience function.
	while(*str)
		WriteByte(b, *str++);
}
void WriteString(netmsg_t *b, const char *str)
{
	while(*str)
		WriteByte(b, *str++);
	WriteByte(b, 0);
}
void WriteData(netmsg_t *b, const char *data, int length)
{
	int i;
	unsigned char *buf;

	if (b->cursize + length > b->maxsize)	//urm, that's just too big. :(
		return;
	buf = b->data+b->cursize;
	for (i = 0; i < length; i++)
		*buf++ = *data++;
	b->cursize+=length;
}

void SendBufferToViewer(viewer_t *v, const char *buffer, int length, qboolean reliable)
{
	if (reliable)
	{
		//try and put it in the normal reliable
		if (!v->backbuffered && v->netchan.message.cursize+length < v->netchan.message.maxsize)
			WriteData(&v->netchan.message, buffer, length);
		else if (v->backbuffered>0 && v->backbuf[v->backbuffered-1].cursize+length < v->backbuf[v->backbuffered-1].maxsize)	//try and put it in the current backbuffer
			WriteData(&v->backbuf[v->backbuffered-1], buffer, length);
		else if (v->backbuffered == MAX_BACK_BUFFERS)
		{
			v->netchan.message.cursize = 0;
			WriteByte(&v->netchan.message, svc_print);
			WriteString(&v->netchan.message, "backbuffer overflow\n");
			Sys_Printf(NULL, "%s backbuffers overflowed\n", v->name);	//FIXME
			v->drop = true;	//we would need too many backbuffers.
		}
		else
		{
			//create a new backbuffer
			if (!v->backbuf[v->backbuffered].data)
			{
				InitNetMsg(&v->backbuf[v->backbuffered], (unsigned char *)malloc(MAX_BACKBUF_SIZE), MAX_BACKBUF_SIZE);
			}
			v->backbuf[v->backbuffered].cursize = 0;	//make sure it's empty
			WriteData(&v->backbuf[v->backbuffered], buffer, length);
			v->backbuffered++;
		}
	}
}

void Multicast(sv_t *tv, char *buffer, int length, int to, unsigned int playermask, int suitablefor)
{
	viewer_t *v;
	switch(to)
	{
	case dem_multiple:
	case dem_single:
	case dem_stats:
		//check and send to them only if they're tracking this player(s).
		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (v->thinksitsconnected||suitablefor&CONNECTING)
				if (v->server == tv)
					if (v->trackplayer>=0)
						if ((1<<v->trackplayer)&playermask)
						{
							if (suitablefor&(v->netchan.isnqprotocol?NQ:QW))
								SendBufferToViewer(v, buffer, length, true);	//FIXME: change the reliable depending on message type
						}
		}
		break;
	default:
		//send to all
		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (v->thinksitsconnected||suitablefor&CONNECTING)
				if (v->server == tv)
					if (suitablefor&(v->netchan.isnqprotocol?NQ:QW))
						SendBufferToViewer(v, buffer, length, true);	//FIXME: change the reliable depending on message type
		}
		break;
	}
}
void Broadcast(cluster_t *cluster, char *buffer, int length, int suitablefor)
{
	viewer_t *v;
	for (v = cluster->viewers; v; v = v->next)
	{
		if (suitablefor&(v->netchan.isnqprotocol?NQ:QW))
			SendBufferToViewer(v, buffer, length, true);
	}
}

static void ParseServerData(sv_t *tv, netmsg_t *m, int to, unsigned int playermask)
{
	int i;
	int protocol;
	viewer_t *v;

	protocol = ReadLong(m);
	if (protocol != PROTOCOL_VERSION)
	{
		ParseError(m);
		return;
	}

	tv->parsingconnectiondata = true;

	tv->clservercount = ReadLong(m);	//we don't care about server's servercount, it's all reliable data anyway.

	tv->trackplayer = -1;

	ReadString(m, tv->gamedir, sizeof(tv->gamedir));

	if (tv->usequkeworldprotocols)
		tv->thisplayer = ReadByte(m)&~128;
	else
	{
		tv->thisplayer = MAX_CLIENTS-1;
		/*tv->servertime =*/ ReadFloat(m);
	}
	ReadString(m, tv->mapname, sizeof(tv->mapname));

	QTV_Printf(tv, "Gamedir: %s\n", tv->gamedir);
	QTV_Printf(tv, "---------------------\n");
	Sys_Printf(tv->cluster, "Stream %i: %s\n", tv->streamid, tv->mapname);
	QTV_Printf(tv, "---------------------\n");

	// get the movevars
	tv->movevars.gravity			= ReadFloat(m);
	tv->movevars.stopspeed			= ReadFloat(m);
	tv->movevars.maxspeed			= ReadFloat(m);
	tv->movevars.spectatormaxspeed	= ReadFloat(m);
	tv->movevars.accelerate			= ReadFloat(m);
	tv->movevars.airaccelerate		= ReadFloat(m);
	tv->movevars.wateraccelerate	= ReadFloat(m);
	tv->movevars.friction			= ReadFloat(m);
	tv->movevars.waterfriction		= ReadFloat(m);
	tv->movevars.entgrav			= ReadFloat(m);

	for (v = tv->cluster->viewers; v; v = v->next)
	{
		if (v->server == tv)
			v->thinksitsconnected = false;
	}

//	tv->maxents = 0;	//clear these
	tv->spawnstatic_count = 0;
	memset(tv->modellist, 0, sizeof(tv->modellist));
	memset(tv->soundlist, 0, sizeof(tv->soundlist));
	memset(tv->lightstyle, 0, sizeof(tv->lightstyle));
	tv->staticsound_count = 0;
	memset(tv->staticsound, 0, sizeof(tv->staticsound));

	memset(tv->players, 0, sizeof(tv->players));


	for (i = 0; i < MAX_ENTITY_FRAMES; i++)
	{
		tv->frame[i].numents = 0;
	}

	if (tv->usequkeworldprotocols)
	{
		tv->netchan.message.cursize = 0;	//mvdsv sucks
		SendClientCommand(tv, "soundlist %i 0\n", tv->clservercount);
	}
	strcpy(tv->status, "Receiving soundlist\n");
}

static void ParseCDTrack(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	char nqversion[3];
	tv->cdtrack = ReadByte(m);

	if (!tv->parsingconnectiondata)
	{
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);

		nqversion[0] = svc_cdtrack;
		nqversion[1] = tv->cdtrack;
		nqversion[2] = tv->cdtrack;
		Multicast(tv, nqversion, 3, to, mask, NQ);
	}
}
static void ParseStufftext(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	viewer_t *v;
	char text[1024];
	char value[256];
	qboolean fromproxy;

	ReadString(m, text, sizeof(text));
//	Sys_Printf(tv->cluster, "stuffcmd: %s", text);
	if (!strcmp(text, "say proxy:menu\n"))
	{	//qizmo's 'previous proxy' message
		tv->proxyisselected = true;
		if (tv->controller)
			QW_SetMenu(tv->controller, MENU_MAIN);
		tv->serverisproxy = true;	//FIXME: Detect this properly on qizmo
	}
	else if (!strncmp(text, "//set prox_inmenu ", 18))
	{
		if (tv->controller)
			QW_SetMenu(tv->controller, atoi(text+18)?MENU_FORWARDING:MENU_NONE);
	}
	else if (strstr(text, "screenshot"))
		return;	//this was generating far too many screenshots when watching demos
	else if (!strcmp(text, "skins\n"))
	{
		const char newcmd[10] = {svc_stufftext, 'c', 'm', 'd', ' ', 'n','e','w','\n','\0'};
		tv->parsingconnectiondata = false;

		strcpy(tv->status, "On server\n");

		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (v->server == tv)
			{
				v->servercount++;
				SendBufferToViewer(v, newcmd, sizeof(newcmd), true);
			}
		}

		if (tv->usequkeworldprotocols)
			SendClientCommand(tv, "begin %i\n", tv->clservercount);
		return;
	}
	else if (!strncmp(text, "fullserverinfo ", 15))
	{
		text[strlen(text)-1] = '\0';
		text[strlen(text)-1] = '\0';

		//copy over the server's serverinfo
		strncpy(tv->serverinfo, text+16, sizeof(tv->serverinfo)-1);

		Info_ValueForKey(tv->serverinfo, "*QTV", value, sizeof(value));
		if (*value)
			fromproxy = true;
		else
			fromproxy = false;

		tv->serverisproxy = fromproxy;

		//add on our extra infos
		Info_SetValueForStarKey(tv->serverinfo, "*qtv", VERSION, sizeof(tv->serverinfo));
		Info_SetValueForStarKey(tv->serverinfo, "*z_ext", Z_EXT_STRING, sizeof(tv->serverinfo));

		Info_ValueForKey(tv->serverinfo, "hostname", tv->hostname, sizeof(tv->hostname));

		//change the hostname (the qtv's hostname with the server's hostname in brackets)
		Info_ValueForKey(tv->serverinfo, "hostname", value, sizeof(value));
		if (fromproxy && strchr(value, '(') && value[strlen(value)-1] == ')')	//already has brackets
		{	//the fromproxy check is because it's fairly common to find a qw server with brackets after it's name.
			char *s;
			s = strchr(value, '(');	//so strip the parent proxy's hostname, and put our hostname first, leaving the origional server's hostname within the brackets
			snprintf(text, sizeof(text), "%s %s", tv->cluster->hostname, s);
		}
		else
		{
			if (tv->sourcefile)
				snprintf(text, sizeof(text), "%s (recorded from: %s)", tv->cluster->hostname, value);
			else
				snprintf(text, sizeof(text), "%s (live: %s)", tv->cluster->hostname, value);
		}
		Info_SetValueForStarKey(tv->serverinfo, "hostname", text, sizeof(tv->serverinfo));

		return;
	}
	else if (!strncmp(text, "cmd ", 4))
	{
		if (tv->usequkeworldprotocols)
			SendClientCommand(tv, "%s", text+4);
		return;	//commands the game server asked for are pointless.
	}
	else if (!strncmp(text, "reconnect", 9))
	{
		if (tv->usequkeworldprotocols)
			SendClientCommand(tv, "new\n");
		return;
	}
	else if (!strncmp(text, "packet ", 7))
	{
		if(tv->usequkeworldprotocols)
		{//eeeevil hack

#define ARG_LEN 256
			char *ptr;
			char arg[3][ARG_LEN];
			netadr_t adr;
			ptr = text;
			ptr = COM_ParseToken(ptr, arg[0], ARG_LEN, "");
			ptr = COM_ParseToken(ptr, arg[1], ARG_LEN, "");
			ptr = COM_ParseToken(ptr, arg[2], ARG_LEN, "");
			NET_StringToAddr(arg[1], &adr, 27500);
			Netchan_OutOfBand(tv->cluster, tv->sourcesock, adr, strlen(arg[2]), arg[2]);

			//this is an evil hack
			SendClientCommand(tv, "new\n");
			return;
		}
		tv->drop = true;	//this shouldn't ever happen
		return;
	}
	else if (tv->usequkeworldprotocols && !strncmp(text, "setinfo ", 8))
	{
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, Q1);
		SendClientCommand(tv, text);
	}
	else
	{
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, Q1);
		return;
	}
}

static void ParseSetInfo(sv_t *tv, netmsg_t *m)
{
	int pnum;
	char key[64];
	char value[256];
	pnum = ReadByte(m);
	ReadString(m, key, sizeof(key));
	ReadString(m, value, sizeof(value));

	if (pnum < MAX_CLIENTS)
		Info_SetValueForStarKey(tv->players[pnum].userinfo, key, value, sizeof(tv->players[pnum].userinfo));

	if (!tv->parsingconnectiondata)
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, dem_all, (unsigned)-1, QW);
}

static void ParseServerinfo(sv_t *tv, netmsg_t *m)
{
	char key[64];
	char value[256];
	ReadString(m, key, sizeof(key));
	ReadString(m, value, sizeof(value));

	if (strcmp(key, "hostname"))	//don't allow the hostname to change, but allow the server to change other serverinfos.
		Info_SetValueForStarKey(tv->serverinfo, key, value, sizeof(tv->serverinfo));

	if (!tv->parsingconnectiondata)
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, dem_all, (unsigned)-1, QW);
}

static void ParsePrint(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	char text[1024];
	char buffer[1024];
	int level;

	level = ReadByte(m);
	ReadString(m, text, sizeof(text)-2);

	if (level == 3)
	{
		strcpy(buffer+2, text);
		buffer[1] = 1;
	}
	else
	{
		strcpy(buffer+1, text);
	}
	buffer[0] = svc_print;

	if (to == dem_all || to == dem_read)
	{
		if (level > 1)
		{
			QTV_Printf(tv, "%s", text);
		}
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW|CONNECTING);
//	Multicast(tv, buffer, strlen(buffer), to, mask, NQ);
}
static void ParseCenterprint(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	viewer_t *v;
	char text[1024];
	ReadString(m, text, sizeof(text));




	switch(to)
	{
	case dem_multiple:
	case dem_single:
	case dem_stats:
		//check and send to them only if they're tracking this player(s).
		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (!v->menunum || v->menunum == MENU_FORWARDING)
			if (v->thinksitsconnected)
				if (v->server == tv)
					if (v->trackplayer>=0)
						if ((1<<v->trackplayer)&mask)
						{
							SendBufferToViewer(v, m->data+m->startpos, m->readpos - m->startpos, true);	//FIXME: change the reliable depending on message type
						}
		}
		break;
	default:
		//send to all
		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (!v->menunum || v->menunum == MENU_FORWARDING)
			if (v->thinksitsconnected)
				if (v->server == tv)
					SendBufferToViewer(v, m->data+m->startpos, m->readpos - m->startpos, true);	//FIXME: change the reliable depending on message type
		}
		break;
	}
}
static int ParseList(sv_t *tv, netmsg_t *m, filename_t *list, int to, unsigned int mask)
{
	int first;

	first = ReadByte(m)+1;
	for (; first < MAX_LIST; first++)
	{
		ReadString(m, list[first].name, sizeof(list[first].name));
//		printf("read %i: %s\n", first, list[first].name);
		if (!*list[first].name)
			break;
//		printf("%i: %s\n", first, list[first].name);
	}

	return ReadByte(m);
}

static void ParseEntityState(entity_state_t *es, netmsg_t *m)	//for baselines/static entities
{
	int i;

	es->modelindex = ReadByte(m);
	es->frame = ReadByte(m);
	es->colormap = ReadByte(m);
	es->skinnum = ReadByte(m);
	for (i = 0; i < 3; i++)
	{
		es->origin[i] = ReadShort(m);
		es->angles[i] = ReadByte(m);
	}
}
static void ParseBaseline(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int entnum;
	entnum = ReadShort(m);
	if (entnum >= MAX_ENTITIES)
	{
		ParseError(m);
		return;
	}
	ParseEntityState(&tv->entity[entnum].baseline, m);
}

static void ParseStaticSound(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	if (tv->staticsound_count == MAX_STATICSOUNDS)
	{
		tv->staticsound_count--;	// don't be fatal.
		Sys_Printf(tv->cluster, "Too many static sounds\n");
	}

	tv->staticsound[tv->staticsound_count].origin[0] = ReadShort(m);
	tv->staticsound[tv->staticsound_count].origin[1] = ReadShort(m);
	tv->staticsound[tv->staticsound_count].origin[2] = ReadShort(m);
	tv->staticsound[tv->staticsound_count].soundindex = ReadByte(m);
	tv->staticsound[tv->staticsound_count].volume = ReadByte(m);
	tv->staticsound[tv->staticsound_count].attenuation = ReadByte(m);

	tv->staticsound_count++;
	if (!tv->parsingconnectiondata)
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, Q1);
}

static void ParseIntermission(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	ReadShort(m);
	ReadShort(m);
	ReadShort(m);
	ReadByte(m);
	ReadByte(m);
	ReadByte(m);

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);
}

void ParseSpawnStatic(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	if (tv->spawnstatic_count == MAX_STATICENTITIES)
	{
		tv->spawnstatic_count--;	// don't be fatal.
		Sys_Printf(tv->cluster, "Too many static entities\n");
	}

	ParseEntityState(&tv->spawnstatic[tv->spawnstatic_count], m);

	tv->spawnstatic_count++;

	if (!tv->parsingconnectiondata)
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, Q1);
}

extern const usercmd_t nullcmd;
static void ParsePlayerInfo(sv_t *tv, netmsg_t *m, qboolean clearoldplayers)
{
	usercmd_t nonnullcmd;
	int flags;
	int num;
	int i;

	if (clearoldplayers)
	{
		for (i = 0; i < MAX_CLIENTS; i++)
		{	//hide players
			//they'll be sent after this packet.
			tv->players[i].active = false;
		}
	}

	num = ReadByte(m);
	if (num >= MAX_CLIENTS)
	{
		num = 0;	// don't be fatal.
		Sys_Printf(tv->cluster, "Too many svc_playerinfos, wrapping\n");
	}
	tv->players[num].old = tv->players[num].current;

	if (tv->usequkeworldprotocols)
	{
		tv->players[num].old = tv->players[num].current;
		flags = (unsigned short)ReadShort (m);

		tv->players[num].current.origin[0] = ReadShort (m);
		tv->players[num].current.origin[1] = ReadShort (m);
		tv->players[num].current.origin[2] = ReadShort (m);

		tv->players[num].current.frame = ReadByte(m);

		if (flags & PF_MSEC)
			ReadByte (m);

		if (flags & PF_COMMAND)
		{
			ReadDeltaUsercmd(m, &nullcmd, &nonnullcmd);
			tv->players[num].current.angles[0] = nonnullcmd.angles[0];
			tv->players[num].current.angles[1] = nonnullcmd.angles[1];
			tv->players[num].current.angles[2] = nonnullcmd.angles[2];
		}
		else
		{	//the only reason we'd not get a command is if it's us.
			if (tv->controller)
			{
				tv->players[num].current.angles[0] = tv->controller->ucmds[2].angles[0];
				tv->players[num].current.angles[1] = tv->controller->ucmds[2].angles[1];
				tv->players[num].current.angles[2] = tv->controller->ucmds[2].angles[2];
			}
			else
			{
				tv->players[num].current.angles[0] = tv->proxyplayerangles[0]/360*65535;
				tv->players[num].current.angles[1] = tv->proxyplayerangles[1]/360*65535;
				tv->players[num].current.angles[2] = tv->proxyplayerangles[2]/360*65535;
			}
		}

		for (i=0 ; i<3 ; i++)
		{
			if (flags & (PF_VELOCITY1<<i) )
				tv->players[num].current.velocity[i] = ReadShort(m);
			else
				tv->players[num].current.velocity[i] = 0;
		}

		if (flags & PF_MODEL)
			tv->players[num].current.modelindex = ReadByte (m);
		else
			tv->players[num].current.modelindex = tv->modelindex_player;

		if (flags & PF_SKINNUM)
			tv->players[num].current.skinnum = ReadByte (m);
		else
			tv->players[num].current.skinnum = 0;

		if (flags & PF_EFFECTS)
			tv->players[num].current.effects = ReadByte (m);
		else
			tv->players[num].current.effects = 0;

		if (flags & PF_WEAPONFRAME)
			tv->players[num].current.weaponframe = ReadByte (m);
		else
			tv->players[num].current.weaponframe = 0;

		tv->players[num].active = true;
	}
	else
	{
		flags = ReadShort(m);
		tv->players[num].gibbed = !!(flags & DF_GIB);
		tv->players[num].dead = !!(flags & DF_DEAD);
		tv->players[num].current.frame = ReadByte(m);

		for (i = 0; i < 3; i++)
		{
			if (flags & (DF_ORIGIN << i))
				tv->players[num].current.origin[i] = ReadShort (m);
		}

		for (i = 0; i < 3; i++)
		{
			if (flags & (DF_ANGLES << i))
			{
				tv->players[num].current.angles[i] = ReadShort(m);
			}
		}

		if (flags & DF_MODEL)
			tv->players[num].current.modelindex = ReadByte (m);

		if (flags & DF_SKINNUM)
			tv->players[num].current.skinnum = ReadByte (m);

		if (flags & DF_EFFECTS)
			tv->players[num].current.effects = ReadByte (m);

		if (flags & DF_WEAPONFRAME)
			tv->players[num].current.weaponframe = ReadByte (m);

		tv->players[num].active = true;

	}

	tv->players[num].leafcount = BSP_SphereLeafNums(tv->bsp,	MAX_ENTITY_LEAFS, tv->players[num].leafs,
														tv->players[num].current.origin[0]/8.0f,
														tv->players[num].current.origin[1]/8.0f,
														tv->players[num].current.origin[2]/8.0f, 32);
}

static int readentitynum(netmsg_t *m, unsigned int *retflags)
{
	int entnum;
	unsigned int flags;
	unsigned short moreflags = 0;
	flags = ReadShort(m);
	if (!flags)
	{
		*retflags = 0;
		return 0;
	}

	entnum = flags&511;
	flags &= ~511;

	if (flags & U_MOREBITS)
	{
		flags |= ReadByte(m);

/*		if (flags & U_EVENMORE)
			flags |= ReadByte(m)<<16;
		if (flags & U_YETMORE)
			flags |= ReadByte(m)<<24;
*/	}

/*	if (flags & U_ENTITYDBL)
		entnum += 512;
	if (flags & U_ENTITYDBL2)
		entnum += 1024;
*/
	*retflags = flags;

	return entnum;
}

static void ParseEntityDelta(sv_t *tv, netmsg_t *m, entity_state_t *old, entity_state_t *new, unsigned int flags, entity_t *ent, qboolean forcerelink)
{
	memcpy(new, old, sizeof(entity_state_t));

	if (flags & U_MODEL)
		new->modelindex = ReadByte(m);
	if (flags & U_FRAME)
		new->frame = ReadByte(m);
	if (flags & U_COLORMAP)
		new->colormap = ReadByte(m);
	if (flags & U_SKIN)
		new->skinnum = ReadByte(m);
	if (flags & U_EFFECTS)
		new->effects = ReadByte(m);

	if (flags & U_ORIGIN1)
		new->origin[0] = ReadShort(m);
	if (flags & U_ANGLE1)
		new->angles[0] = ReadByte(m);
	if (flags & U_ORIGIN2)
		new->origin[1] = ReadShort(m);
	if (flags & U_ANGLE2)
		new->angles[1] = ReadByte(m);
	if (flags & U_ORIGIN3)
		new->origin[2] = ReadShort(m);
	if (flags & U_ANGLE3)
		new->angles[2] = ReadByte(m);


	if (forcerelink || (flags & (U_ORIGIN1|U_ORIGIN2|U_ORIGIN3|U_MODEL)))
	{
		ent->leafcount = 
				BSP_SphereLeafNums(tv->bsp, MAX_ENTITY_LEAFS, ent->leafs,
				new->origin[0]/8.0f,
				new->origin[1]/8.0f,
				new->origin[2]/8.0f, 32);
	}
}

static int ExpandFrame(unsigned int newmax, frame_t *frame)
{
	entity_state_t *newents;
	unsigned short *newnums;

	if (newmax < frame->maxents)
		return true;

	newmax += 16;

	newents = malloc(sizeof(*newents) * newmax);
	if (!newents)
		return false;
	newnums = malloc(sizeof(*newnums) * newmax);
	if (!newnums)
	{
		free(newents);
		return false;
	}

	memcpy(newents, frame->ents, sizeof(*newents) * frame->maxents);
	memcpy(newnums, frame->entnums, sizeof(*newnums) * frame->maxents);

	if (frame->ents)
		free(frame->ents);
	if (frame->entnums)
		free(frame->entnums);
	
	frame->ents = newents;
	frame->entnums = newnums;
	frame->maxents = newmax;
	return true;
}

static void ParsePacketEntities(sv_t *tv, netmsg_t *m, int deltaframe)
{
	frame_t *newframe;
	frame_t *oldframe;
	int oldcount;
	int newnum, oldnum;
	int newindex, oldindex;
	unsigned int flags;

	viewer_t *v;

	tv->nailcount = 0;

	tv->physicstime = tv->parsetime;

	if (tv->cluster->chokeonnotupdated)
		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (v->server == tv)
				v->chokeme = false;
		}
		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (v->server == tv && v->netchan.isnqprotocol)
				v->maysend = true;
		}


	if (deltaframe != -1)
		deltaframe &= (ENTITY_FRAMES-1);

	if (tv->usequkeworldprotocols)
	{
		newframe = &tv->frame[tv->netchan.incoming_sequence & (ENTITY_FRAMES-1)];

		if (tv->netchan.outgoing_sequence - tv->netchan.incoming_sequence >= ENTITY_FRAMES - 1)
		{
			//should drop it
			Sys_Printf(tv->cluster, "Outdated frames\n");
		}
		else if (deltaframe != -1 && newframe->oldframe != deltaframe)
			Sys_Printf(tv->cluster, "Mismatching delta frames\n");
	}
	else
	{
		deltaframe = tv->netchan.incoming_sequence & (ENTITY_FRAMES-1);
		tv->netchan.incoming_sequence++;
		newframe = &tv->frame[tv->netchan.incoming_sequence & (ENTITY_FRAMES-1)];
	}
	if (deltaframe != -1)
	{
		oldframe = &tv->frame[deltaframe];
		oldcount = oldframe->numents;
	}
	else
	{
		oldframe = NULL;
		oldcount = 0;
	}

	oldindex = 0;
	newindex = 0;

//printf("frame\n");

	for(;;)
	{
		newnum = readentitynum(m, &flags);
		if (!newnum)
		{
			//end of packet
			//any remaining old ents need to be copied to the new frame
			while (oldindex < oldcount)
			{
//printf("Propogate (spare)\n");
				if (!ExpandFrame(newindex, newframe))
					break;

				memcpy(&newframe->ents[newindex], &oldframe->ents[oldindex], sizeof(entity_state_t));
				newframe->entnums[newindex] = oldframe->entnums[oldindex];
				newindex++;
				oldindex++;
			}
			break;
		}

		if (oldindex >= oldcount)
			oldnum = 0xffff;
		else
			oldnum = oldframe->entnums[oldindex];
		while(newnum > oldnum)
		{
//printf("Propogate (unchanged)\n");
			if (!ExpandFrame(newindex, newframe))
				break;

			memcpy(&newframe->ents[newindex], &oldframe->ents[oldindex], sizeof(entity_state_t));
			newframe->entnums[newindex] = oldframe->entnums[oldindex];
			newindex++;
			oldindex++;

			if (oldindex >= oldcount)
				oldnum = 0xffff;
			else
				oldnum = oldframe->entnums[oldindex];
		}

		if (newnum < oldnum)
		{	//this ent wasn't in the last packet
//printf("add\n");
			if (flags & U_REMOVE)
			{	//remove this ent... just don't copy it across.
				//printf("add\n");
				continue;
			}

			if (!ExpandFrame(newindex, newframe))
				break;
			ParseEntityDelta(tv, m, &tv->entity[newnum].baseline, &newframe->ents[newindex], flags, &tv->entity[newnum], true);
			newframe->entnums[newindex] = newnum;
			newindex++;
		}
		else if (newnum == oldnum)
		{
			if (flags & U_REMOVE)
			{	//remove this ent... just don't copy it across.
				//printf("add\n");
				oldindex++;
				continue;
			}
//printf("Propogate (changed)\n");
			if (!ExpandFrame(newindex, newframe))
				break;
			ParseEntityDelta(tv, m, &oldframe->ents[oldindex], &newframe->ents[newindex], flags, &tv->entity[newnum], false);
			newframe->entnums[newindex] = newnum;
			newindex++;
			oldindex++;
		}

	}

	newframe->numents = newindex;
return;

/*

	//luckilly, only updated entities are here, so that keeps cpu time down a bit.
	for (;;)
	{
		flags = ReadShort(m);
		if (!flags)
			break;

		entnum = flags & 511;
		if (tv->maxents < entnum)
			tv->maxents = entnum;
		flags &= ~511;
		memcpy(&tv->entity[entnum].old, &tv->entity[entnum].current, sizeof(entity_state_t));	//ow.
		if (flags & U_REMOVE)
		{
			tv->entity[entnum].current.modelindex = 0;
			continue;
		}
		if (!tv->entity[entnum].current.modelindex)	//lerp from baseline
		{
			memcpy(&tv->entity[entnum].current, &tv->entity[entnum].baseline, sizeof(entity_state_t));
			forcerelink = true;
		}
		else
			forcerelink = false;

		if (flags & U_MOREBITS)
			flags |= ReadByte(m);
		if (flags & U_MODEL)
			tv->entity[entnum].current.modelindex = ReadByte(m);
		if (flags & U_FRAME)
			tv->entity[entnum].current.frame = ReadByte(m);
		if (flags & U_COLORMAP)
			tv->entity[entnum].current.colormap = ReadByte(m);
		if (flags & U_SKIN)
			tv->entity[entnum].current.skinnum = ReadByte(m);
		if (flags & U_EFFECTS)
			tv->entity[entnum].current.effects = ReadByte(m);

		if (flags & U_ORIGIN1)
			tv->entity[entnum].current.origin[0] = ReadShort(m);
		if (flags & U_ANGLE1)
			tv->entity[entnum].current.angles[0] = ReadByte(m);
		if (flags & U_ORIGIN2)
			tv->entity[entnum].current.origin[1] = ReadShort(m);
		if (flags & U_ANGLE2)
			tv->entity[entnum].current.angles[1] = ReadByte(m);
		if (flags & U_ORIGIN3)
			tv->entity[entnum].current.origin[2] = ReadShort(m);
		if (flags & U_ANGLE3)
			tv->entity[entnum].current.angles[2] = ReadByte(m);

		tv->entity[entnum].updatetime = tv->curtime;
		if (!tv->entity[entnum].old.modelindex)	//no old state
			memcpy(&tv->entity[entnum].old, &tv->entity[entnum].current, sizeof(entity_state_t));	//copy the new to the old, so we don't end up with interpolation glitches


		if ((flags & (U_ORIGIN1 | U_ORIGIN2 | U_ORIGIN3)) || forcerelink)
			tv->entity[entnum].leafcount = BSP_SphereLeafNums(tv->bsp, MAX_ENTITY_LEAFS, tv->entity[entnum].leafs,
															tv->entity[entnum].current.origin[0]/8.0f,
															tv->entity[entnum].current.origin[1]/8.0f,
															tv->entity[entnum].current.origin[2]/8.0f, 32);
	}
*/
}

static void ParseUpdatePing(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	int pnum;
	int ping;
	pnum = ReadByte(m);
	ping = ReadShort(m);

	if (pnum < MAX_CLIENTS)
		tv->players[pnum].ping = ping;
	else
		Sys_Printf(tv->cluster, "svc_updateping: invalid player number\n");

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);
}

static void ParseUpdateFrags(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	int pnum;
	int frags;
	pnum = ReadByte(m);
	frags = (signed short)ReadShort(m);

	if (pnum < MAX_CLIENTS)
		tv->players[pnum].frags = frags;
	else
		Sys_Printf(tv->cluster, "svc_updatefrags: invalid player number\n");

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, (pnum < 16)?Q1:QW);
}

static void ParseUpdateStat(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int pnum;
	int value;
	int statnum;

	statnum = ReadByte(m);
	value = ReadByte(m);

	if (statnum < MAX_STATS)
	{
		for (pnum = 0; pnum < MAX_CLIENTS; pnum++)
		{
			if (mask & (1<<pnum))
				tv->players[pnum].stats[statnum] = value;
		}
	}
	else
		Sys_Printf(tv->cluster, "svc_updatestat: invalid stat number\n");

//	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);
}
static void ParseUpdateStatLong(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int pnum;
	int value;
	int statnum;

	statnum = ReadByte(m);
	value = ReadLong(m);

	if (statnum < MAX_STATS)
	{
		for (pnum = 0; pnum < MAX_CLIENTS; pnum++)
		{
			if (mask & (1<<pnum))
				tv->players[pnum].stats[statnum] = value;
		}
	}
	else
		Sys_Printf(tv->cluster, "svc_updatestatlong: invalid stat number\n");

//	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);
}

static void ParseUpdateUserinfo(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	int pnum;
	pnum = ReadByte(m);
	ReadLong(m);
	if (pnum < MAX_CLIENTS)
		ReadString(m, tv->players[pnum].userinfo, sizeof(tv->players[pnum].userinfo));
	else
	{
		Sys_Printf(tv->cluster, "svc_updateuserinfo: invalid player number\n");
		while (ReadByte(m))	//suck out the message.
		{
		}
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);
}

static void ParsePacketloss(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int pnum;
	int value;

	pnum = ReadByte(m)%MAX_CLIENTS;
	value = ReadByte(m);

	if (pnum < MAX_CLIENTS)
		tv->players[pnum].packetloss = value;
	else
		Sys_Printf(tv->cluster, "svc_updatepl: invalid player number\n");

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);
}

static void ParseUpdateEnterTime(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int pnum;
	float value;

	pnum = ReadByte(m)%MAX_CLIENTS;
	value = ReadFloat(m);

	if (pnum < MAX_CLIENTS)
		tv->players[pnum].entertime = value;
	else
		Sys_Printf(tv->cluster, "svc_updateentertime: invalid player number\n");

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);
}

static void ParseSound(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
#define	SND_VOLUME		(1<<15)		// a qbyte
#define	SND_ATTENUATION	(1<<14)		// a qbyte

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0
	int i;
	int channel;
	unsigned char vol;
	unsigned char atten;
	unsigned char sound_num;
	short org[3];
	int ent;


	unsigned char nqversion[64];
	int nqlen = 0;

	channel = (unsigned short)ReadShort(m);


    if (channel & SND_VOLUME)
		vol = ReadByte (m);
	else
		vol = DEFAULT_SOUND_PACKET_VOLUME;

    if (channel & SND_ATTENUATION)
		atten = ReadByte (m) / 64.0;
	else
		atten = DEFAULT_SOUND_PACKET_ATTENUATION;

	sound_num = ReadByte (m);

	ent = (channel>>3)&1023;
	channel &= 7;

	for (i=0 ; i<3 ; i++)
		org[i] = ReadShort (m);

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);

	nqversion[0] = svc_sound;
	nqversion[1] = 0;
	if (vol != DEFAULT_SOUND_PACKET_VOLUME)
		nqversion[1] |= 1;
	if (atten != DEFAULT_SOUND_PACKET_ATTENUATION)
		nqversion[1] |= 2;
	nqlen=2;

	if (nqversion[1] & 1)
		nqversion[nqlen++] = vol;
	if (nqversion[1] & 2)
		nqversion[nqlen++] = atten*64;

	channel = (ent<<3) | channel;

	nqversion[nqlen++] = (channel&0x00ff)>>0;
	nqversion[nqlen++] = (channel&0xff00)>>8;
	nqversion[nqlen++] = sound_num;

	nqversion[nqlen++] = 0;
	nqversion[nqlen++] = 0;

	nqversion[nqlen++] = 0;
	nqversion[nqlen++] = 0;

	nqversion[nqlen++] = 0;
	nqversion[nqlen++] = 0;

	Multicast(tv, nqversion, nqlen, to, mask, NQ);
}

static void ParseDamage(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	ReadByte (m);
	ReadByte (m);
	ReadShort (m);
	ReadShort (m);
	ReadShort (m);
	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, QW);
}

enum {
	TE_SPIKE			= 0,
	TE_SUPERSPIKE		= 1,
	TE_GUNSHOT			= 2,
	TE_EXPLOSION		= 3,
	TE_TAREXPLOSION		= 4,
	TE_LIGHTNING1		= 5,
	TE_LIGHTNING2		= 6,
	TE_WIZSPIKE			= 7,
	TE_KNIGHTSPIKE		= 8,
	TE_LIGHTNING3		= 9,
	TE_LAVASPLASH		= 10,
	TE_TELEPORT			= 11,

	TE_BLOOD			= 12,
	TE_LIGHTNINGBLOOD	= 13,
};
static void ParseTempEntity(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	int i;
	int dest = QW;
	char nqversion[64];
	int nqversionlength=0;

	i = ReadByte (m);
	switch(i)
	{
	case TE_SPIKE:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_SUPERSPIKE:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_GUNSHOT:
		ReadByte (m);

		nqversion[0] = svc_temp_entity;
		nqversion[1] = TE_GUNSHOT;
		nqversion[2] = ReadByte (m);nqversion[3] = ReadByte (m);
		nqversion[4] = ReadByte (m);nqversion[5] = ReadByte (m);
		nqversion[6] = ReadByte (m);nqversion[7] = ReadByte (m);
		nqversionlength = 8;
		break;
	case TE_EXPLOSION:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_TAREXPLOSION:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_LIGHTNING1:
	case TE_LIGHTNING2:
	case TE_LIGHTNING3:
		ReadShort (m);

		ReadShort (m);
		ReadShort (m);
		ReadShort (m);

		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_WIZSPIKE:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_KNIGHTSPIKE:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_LAVASPLASH:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_TELEPORT:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		dest |= NQ;
		break;
	case TE_BLOOD:
		ReadByte (m);
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	case TE_LIGHTNINGBLOOD:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	default:
		Sys_Printf(tv->cluster, "temp entity %i not recognised\n", i);
		return;
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask, dest);

	if (nqversionlength)
		Multicast(tv, nqversion, nqversionlength, to, mask, NQ);
}

void ParseLightstyle(sv_t *tv, netmsg_t *m)
{
	int style;
	style = ReadByte(m);
	if (style < MAX_LIGHTSTYLES)
		ReadString(m, tv->lightstyle[style].name, sizeof(tv->lightstyle[style].name));
	else
	{
		Sys_Printf(tv->cluster, "svc_lightstyle: invalid lightstyle index (%i)\n", style);
		while (ReadByte(m))	//suck out the message.
		{
		}
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, dem_read, (unsigned)-1, Q1);
}

void ParseNails(sv_t *tv, netmsg_t *m, qboolean nails2)
{
	int count;
	int i;
	count = (unsigned char)ReadByte(m);
	while(count > sizeof(tv->nails) / sizeof(tv->nails[0]))
	{
		count--;
		if (nails2)
			ReadByte(m);
		for (i = 0; i < 6; i++)
			ReadByte(m);
	}

	tv->nailcount = count;
	while(count-- > 0)
	{
		if (nails2)
			tv->nails[count].number = ReadByte(m);
		else
			tv->nails[count].number = count;
		for (i = 0; i < 6; i++)
			tv->nails[count].bits[i] = ReadByte(m);
	}
}

void ParseDownload(sv_t *tv, netmsg_t *m)
{
	int size, b;
	unsigned int percent;
	char buffer[2048];

	size = (signed short)ReadShort(m);
	percent = ReadByte(m);

	if (size < 0)
	{
		Sys_Printf(tv->cluster, "Downloading failed\n");
		if (tv->downloadfile)
			fclose(tv->downloadfile);
		tv->downloadfile = NULL;
		tv->drop = true;
		QW_StreamPrint(tv->cluster, tv, NULL, "Map download failed\n");
		return;
	}

	for (b = 0; b < size; b++)
		buffer[b] = ReadByte(m);

	if (!tv->downloadfile)
	{
		Sys_Printf(tv->cluster, "Not downloading anything\n");
		tv->drop = true;
		return;
	}
	fwrite(buffer, 1, size, tv->downloadfile);

	if (percent == 100)
	{
		fclose(tv->downloadfile);
		tv->downloadfile = NULL;

		snprintf(buffer, sizeof(buffer), "%s/%s", (tv->gamedir&&*tv->gamedir)?tv->gamedir:"id1", tv->modellist[1].name);
		rename(tv->downloadname, buffer);

		Sys_Printf(tv->cluster, "Download complete\n");

		tv->bsp = BSP_LoadModel(tv->cluster, tv->gamedir, tv->modellist[1].name);
		if (!tv->bsp)
		{
			Sys_Printf(tv->cluster, "Failed to read BSP\n");
			tv->drop = true;
		}
		else
		{
			SendClientCommand(tv, "prespawn %i 0 %i\n", tv->clservercount, LittleLong(BSP_Checksum(tv->bsp)));
			strcpy(tv->status, "Prespawning\n");
		}
	}
	else
	{
		snprintf(tv->status, sizeof(tv->status), "Downloading map, %i%%\n", percent);
		SendClientCommand(tv, "nextdl\n");
	}
}

void ParseMessage(sv_t *tv, char *buffer, int length, int to, int mask)
{
	int i;
	netmsg_t buf;
	qboolean clearoldplayers = true;
	buf.cursize = length;
	buf.maxsize = length;
	buf.readpos = 0;
	buf.data = buffer;
	buf.startpos = 0;
	while(buf.readpos < buf.cursize)
	{
		if (buf.readpos > buf.cursize)
		{
			Sys_Printf(tv->cluster, "Read past end of parse buffer\n");
			return;
		}
//		printf("%i\n", buf.buffer[0]);
		buf.startpos = buf.readpos;
		switch (ReadByte(&buf))
		{
		case svc_bad:
			ParseError(&buf);
			Sys_Printf(tv->cluster, "ParseMessage: svc_bad\n");
			return;
		case svc_nop:	//quakeworld isn't meant to send these.
			QTV_Printf(tv, "nop\n");
			break;

		case svc_disconnect:
			//mvdsv safely terminates it's mvds with an svc_disconnect.
			//the client is meant to read that and disconnect without reading the intentionally corrupt packet following it.
			//however, our demo playback is chained and looping and buffered.
			//so we've already found the end of the source file and restarted parsing.
			//so there's very little we can do except crash ourselves on the EndOfDemo text following the svc_disconnect
			//that's a bad plan, so just stop reading this packet.
			return;

		case svc_updatestat:
			ParseUpdateStat(tv, &buf, to, mask);
			break;

//#define	svc_version			4	// [long] server version
//#define	svc_setview			5	// [short] entity number
		case svc_sound:
			ParseSound(tv, &buf, to, mask);
			break;
//#define	svc_time			7	// [float] server time

		case svc_print:
			ParsePrint(tv, &buf, to, mask);
			break;

		case svc_stufftext:
			ParseStufftext(tv, &buf, to, mask);
			break;

		case svc_setangle:
			if (!tv->usequkeworldprotocols)
				ReadByte(&buf);
			tv->proxyplayerangles[0] = ReadByte(&buf)*360.0/255;
			tv->proxyplayerangles[1] = ReadByte(&buf)*360.0/255;
			tv->proxyplayerangles[2] = ReadByte(&buf)*360.0/255;

			if (tv->usequkeworldprotocols && tv->controller)
				SendBufferToViewer(tv->controller, buf.data+buf.startpos, buf.readpos - buf.startpos, true);

			{
				char nq[4];
				nq[0] = svc_setangle;
				nq[1] = tv->proxyplayerangles[0];
				nq[2] = tv->proxyplayerangles[1];
				nq[3] = tv->proxyplayerangles[2];
//				Multicast(tv, nq, 4, to, mask, Q1);
			}
			break;

		case svc_serverdata:
			ParseServerData(tv, &buf, to, mask);
			break;

		case svc_lightstyle:
			ParseLightstyle(tv, &buf);
			break;

//#define	svc_updatename		13	// [qbyte] [string]

		case svc_updatefrags:
			ParseUpdateFrags(tv, &buf, to, mask);
			break;

//#define	svc_clientdata		15	// <shortbits + data>
//#define	svc_stopsound		16	// <see code>
//#define	svc_updatecolors	17	// [qbyte] [qbyte] [qbyte]

		case svc_particle:
			ReadShort(&buf);
			ReadShort(&buf);
			ReadShort(&buf);
			ReadByte(&buf);
			ReadByte(&buf);
			ReadByte(&buf);
			ReadByte(&buf);
			ReadByte(&buf);
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, dem_read, (unsigned)-1, Q1);
			break;

		case svc_damage:
			ParseDamage(tv, &buf, to, mask);
			break;

		case svc_spawnstatic:
			ParseSpawnStatic(tv, &buf, to, mask);
			break;

//#define	svc_spawnstatic2	21
		case svc_spawnbaseline:
			ParseBaseline(tv, &buf, to, mask);
			break;

		case svc_temp_entity:
			ParseTempEntity(tv, &buf, to, mask);
			break;

		case svc_setpause:	// [qbyte] on / off
			tv->ispaused = ReadByte(&buf);
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, dem_read, (unsigned)-1, Q1);
			break;

//#define	svc_signonnum		25	// [qbyte]  used for the signon sequence

		case svc_centerprint:
			ParseCenterprint(tv, &buf, to, mask);
			break;

//#define	svc_killedmonster	27
//#define	svc_foundsecret		28

		case svc_spawnstaticsound:
			ParseStaticSound(tv, &buf, to, mask);
			break;

		case svc_intermission:
			ParseIntermission(tv, &buf, to, mask);
			break;

//#define	svc_finale			31		// [string] text

		case svc_cdtrack:
			ParseCDTrack(tv, &buf, to, mask);
			break;

//#define svc_sellscreen		33

//#define svc_cutscene		34	//hmm... nq only... added after qw tree splitt?

		case svc_smallkick:
		case svc_bigkick:
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, to, mask, QW);
			break;

		case svc_updateping:
			ParseUpdatePing(tv, &buf, to, mask);
			break;

		case svc_updateentertime:
			ParseUpdateEnterTime(tv, &buf, to, mask);
			break;

		case svc_updatestatlong:
			ParseUpdateStatLong(tv, &buf, to, mask);
			break;

		case svc_muzzleflash:
			ReadShort(&buf);
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, to, mask, QW);
			break;

		case svc_updateuserinfo:
			ParseUpdateUserinfo(tv, &buf, to, mask);
			break;

		case svc_download:	// [short] size [size bytes]
			ParseDownload(tv, &buf);
			break;

		case svc_playerinfo:
			ParsePlayerInfo(tv, &buf, clearoldplayers);
			clearoldplayers = false;
			break;

		case svc_nails:
			ParseNails(tv, &buf, false);
			break;
		case svc_chokecount:
			ReadByte(&buf);
			break;

		case svc_modellist:
			i = ParseList(tv, &buf, tv->modellist, to, mask);
			if (!i)
			{
				int j;
				if (tv->bsp)
					BSP_Free(tv->bsp);

				if (tv->cluster->nobsp)// || !tv->usequkeworldprotocols)
					tv->bsp = NULL;
				else
					tv->bsp = BSP_LoadModel(tv->cluster, tv->gamedir, tv->modellist[1].name);

				tv->numinlines = 0;
				for (j = 2; j < 256; j++)
				{
					if (*tv->modellist[j].name != '*')
						break;
					tv->numinlines = j;
				}
				strcpy(tv->status, "Prespawning\n");
			}
			if (tv->usequkeworldprotocols)
			{
				if (i)
					SendClientCommand(tv, "modellist %i %i\n", tv->clservercount, i);
				else if (!tv->bsp && !tv->cluster->nobsp)
				{
					if (tv->downloadfile)
					{
						fclose(tv->downloadfile);
						unlink(tv->downloadname);
						Sys_Printf(tv->cluster, "Was already downloading %s\nOld download canceled\n", tv->downloadname);
						tv->downloadfile = NULL;
					}
					snprintf(tv->downloadname, sizeof(tv->downloadname), "%s/%s.tmp", (tv->gamedir&&*tv->gamedir)?tv->gamedir:"id1", tv->modellist[1].name);
					tv->downloadfile = fopen(tv->downloadname, "wb");
					if (!tv->downloadfile)
					{
						Sys_Printf(tv->cluster, "Couldn't open temporary file %s\n", tv->downloadname);
					}
					else
					{
						strcpy(tv->status, "Downloading map\n");
						Sys_Printf(tv->cluster, "Attempting download of %s\n", tv->downloadname);
						SendClientCommand(tv, "download %s\n", tv->modellist[1].name);

						QW_StreamPrint(tv->cluster, tv, NULL, "[QTV] Attempting map download\n");
					}
				}
				else
				{
					SendClientCommand(tv, "prespawn %i 0 %i\n", tv->clservercount, LittleLong(BSP_Checksum(tv->bsp)));
				}
			}
			break;
		case svc_soundlist:
			i = ParseList(tv, &buf, tv->soundlist, to, mask);
			if (!i)
				strcpy(tv->status, "Receiving modellist\n");
			if (tv->usequkeworldprotocols)
			{
				if (i)
					SendClientCommand(tv, "soundlist %i %i\n", tv->clservercount, i);
				else
					SendClientCommand(tv, "modellist %i 0\n", tv->clservercount);
			}
			break;

		case svc_packetentities:
//			FlushPacketEntities(tv);
			ParsePacketEntities(tv, &buf, -1);
			break;
		case svc_deltapacketentities:
			ParsePacketEntities(tv, &buf, ReadByte(&buf));
			break;

		case svc_entgravity:		// gravity change, for prediction
			ReadFloat(&buf);
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, to, mask, QW);
			break;
		case svc_maxspeed:			// maxspeed change, for prediction
			ReadFloat(&buf);
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, to, mask, QW);
			break;
		case svc_setinfo:
			ParseSetInfo(tv, &buf);
			break;
		case svc_serverinfo:
			ParseServerinfo(tv, &buf);
			break;
		case svc_updatepl:
			ParsePacketloss(tv, &buf, to, mask);
			break;
		case svc_nails2:
			ParseNails(tv, &buf, true);
			break;

		case svc_killedmonster:
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, to, mask, Q1);
			break;
		case svc_foundsecret:
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, to, mask, Q1);
			break;

		default:
			buf.readpos = buf.startpos;
			Sys_Printf(tv->cluster, "Can't handle svc %i\n", (unsigned int)ReadByte(&buf));
			return;
		}
	}
}

