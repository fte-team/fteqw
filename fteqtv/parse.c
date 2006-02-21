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

#define DF_ORIGIN	1
#define DF_ANGLES	(1<<3)
#define DF_EFFECTS	(1<<6)
#define DF_SKINNUM	(1<<7)
#define DF_DEAD		(1<<8)
#define DF_GIB		(1<<9)
#define DF_WEAPONFRAME (1<<10)
#define DF_MODEL	(1<<11)

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
			v->drop = true;	//we would need too many backbuffers.
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

void Multicast(sv_t *tv, char *buffer, int length, int to, unsigned int playermask)
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
			if (v->server == tv)
				if (v->trackplayer>=0)
					if ((1<<v->trackplayer)&playermask)
						SendBufferToViewer(v, buffer, length, true);	//FIXME: change the reliable depending on message type
		}
		break;
	default:
		//send to all
		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (v->server == tv)
				SendBufferToViewer(v, buffer, length, true);	//FIXME: change the reliable depending on message type
		}
		break;
	}
}
void Broadcast(cluster_t *cluster, char *buffer, int length)
{
	viewer_t *v;
	for (v = cluster->viewers; v; v = v->next)
	{
		SendBufferToViewer(v, buffer, length, true);
	}
}

static void ParseServerData(sv_t *tv, netmsg_t *m, int to, unsigned int playermask)
{
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

	ReadString(m, tv->gamedir, sizeof(tv->gamedir));

	if (tv->usequkeworldprotocols)
		tv->thisplayer = ReadByte(m)&~128;
	else
	{
		tv->thisplayer = MAX_CLIENTS-1;
		/*tv->servertime =*/ ReadFloat(m);
	}
	ReadString(m, tv->mapname, sizeof(tv->mapname));

	Sys_Printf(tv->cluster, "Gamedir: %s\n", tv->gamedir);
	Sys_Printf(tv->cluster, "---------------------\n");
	Sys_Printf(tv->cluster, "%s\n", tv->mapname);
	Sys_Printf(tv->cluster, "---------------------\n");

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

	tv->maxents = 0;	//clear these
	memset(tv->modellist, 0, sizeof(tv->modellist));
	memset(tv->soundlist, 0, sizeof(tv->soundlist));
	memset(tv->lightstyle, 0, sizeof(tv->lightstyle));
	tv->staticsound_count = 0;
	memset(tv->staticsound, 0, sizeof(tv->staticsound));

	memset(tv->players, 0, sizeof(tv->players));



	if (tv->usequkeworldprotocols)
	{
		SendClientCommand(tv, "soundlist %i 0\n", tv->clservercount);
	}
}

static void ParseCDTrack(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	tv->cdtrack = ReadByte(m);

	if (!tv->parsingconnectiondata)
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}
static void ParseStufftext(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	viewer_t *v;
	char text[1024];
	char value[256];
	qboolean fromproxy;

	ReadString(m, text, sizeof(text));
	Sys_Printf(tv->cluster, "stuffcmd: %s", text);

	if (!strcmp(text, "skins\n"))
	{
		const char newcmd[10] = {svc_stufftext, 'c', 'm', 'd', ' ', 'n','e','w','\n','\0'};
		tv->parsingconnectiondata = false;

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

		Info_ValueForKey(tv->serverinfo, "*qtv", value, sizeof(value));
		if (*value)
			fromproxy = true;
		else
			fromproxy = false;

		//add on our extra infos
		Info_SetValueForStarKey(tv->serverinfo, "*qtv", VERSION, sizeof(tv->serverinfo));
		Info_SetValueForStarKey(tv->serverinfo, "*z_ext", Z_EXT_STRING, sizeof(tv->serverinfo));

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
			if (tv->file)
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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, dem_all, (unsigned)-1);
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
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, dem_all, (unsigned)-1);
}

static void ParsePrint(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned char *t;
	char text[1024];
	int level;

	level = ReadByte(m);
	ReadString(m, text, sizeof(text));

	if (to == dem_all || to == dem_read)
	{
		if (level > 1)
		{
			for (t = (unsigned char*)text; *t; t++)
			{
				if (*t >= 146 && *t < 156)
					*t = *t - 146 + '0';
				if (*t == 143)
					*t = '.';
				if (*t == 157 || *t == 158 || *t == 159)
					*t = '-';
				if (*t >= 128)
					*t -= 128;
				if (*t == 16)
					*t = '[';
				if (*t == 17)
					*t = ']';
				if (*t == 29)
					*t = '-';
				if (*t == 30)
					*t = '-';
				if (*t == 31)
					*t = '-';
				if (*t == '\a')	//doh. :D
					*t = ' ';
			}
			Sys_Printf(tv->cluster, "%s", text);
		}
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}
static void ParseCenterprint(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	char text[1024];
	ReadString(m, text, sizeof(text));

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseIntermission(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	ReadShort(m);
	ReadShort(m);
	ReadShort(m);
	ReadByte(m);
	ReadByte(m);
	ReadByte(m);

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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
		Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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

		for (i=0 ; i<3 ; i++)
		{
			if (flags & (PF_VELOCITY1<<i) )
				ReadShort(m);
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

		tv->players[num].active = (num != tv->thisplayer);
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

static void FlushPacketEntities(sv_t *tv)
{
	int i;
	for (i = 0; i < MAX_ENTITIES; i++)
		tv->entity[i].current.modelindex = 0;
}

static void ParsePacketEntities(sv_t *tv, netmsg_t *m)
{
	int entnum;
	int flags;
	qboolean forcerelink;

	viewer_t *v;

	tv->physicstime = tv->parsetime;

	if (tv->cluster->chokeonnotupdated)
		for (v = tv->cluster->viewers; v; v = v->next)
		{
			if (v->server == tv)
				v->chokeme = false;
		}

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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseUpdateFrags(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	int best;
	int pnum;
	int frags;
	char buffer[64];
	pnum = ReadByte(m);
	frags = ReadShort(m);

	if (pnum < MAX_CLIENTS)
		tv->players[pnum].frags = frags;
	else
		Sys_Printf(tv->cluster, "svc_updatefrags: invalid player number\n");

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);

	if (!tv->usequkeworldprotocols)
		return;

	frags = -10000;
	best = -1;
	for (pnum = 0; pnum < MAX_CLIENTS; pnum++)
	{
		if (*tv->players[pnum].userinfo && !atoi(Info_ValueForKey(tv->players[pnum].userinfo, "*spectator", buffer, sizeof(buffer))))
		if (frags < tv->players[pnum].frags)
		{
			best = pnum;
			frags = tv->players[pnum].frags;
		}
	}
	if (best != tv->trackplayer)
	{
		SendClientCommand (tv, "ptrack %i\n", best);
		tv->trackplayer = best;
	}
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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseSound(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
#define	SND_VOLUME		(1<<15)		// a qbyte
#define	SND_ATTENUATION	(1<<14)		// a qbyte

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0
	int i;
	unsigned short chan;
	unsigned char vol;
	unsigned char atten;
	unsigned char sound_num;
	short org[3];

	chan = ReadShort(m);

    if (chan & SND_VOLUME)
		vol = ReadByte (m);
	else
		vol = DEFAULT_SOUND_PACKET_VOLUME;

    if (chan & SND_ATTENUATION)
		atten = ReadByte (m) / 64.0;
	else
		atten = DEFAULT_SOUND_PACKET_ATTENUATION;

	sound_num = ReadByte (m);

	for (i=0 ; i<3 ; i++)
		org[i] = ReadShort (m);

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseDamage(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	ReadByte (m);
	ReadByte (m);
	ReadShort (m);
	ReadShort (m);
	ReadShort (m);
	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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
	i = ReadByte (m);
	switch(i)
	{
	case TE_SPIKE:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	case TE_SUPERSPIKE:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	case TE_GUNSHOT:
		ReadByte (m);
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	case TE_EXPLOSION:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	case TE_TAREXPLOSION:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
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
		break;
	case TE_WIZSPIKE:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	case TE_KNIGHTSPIKE:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	case TE_LAVASPLASH:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
		break;
	case TE_TELEPORT:
		ReadShort (m);
		ReadShort (m);
		ReadShort (m);
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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
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

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, dem_read, (unsigned)-1);
}

void ParseNails(sv_t *tv, netmsg_t *m, qboolean nails2)
{
	int count;
	int nailnum;
	int i;
	unsigned char bits[6];
	count = (unsigned char)ReadByte(m);
	while(count-- > 0)
	{
		if (nails2)
			nailnum = ReadByte(m);
		else
			nailnum = count;
		for (i = 0; i < 6; i++)
			bits[i] = ReadByte(m);
	}
//qwe - [qbyte] num [52 bits] nxyzpy 8 12 12 12 4 8
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
			Sys_Printf(tv->cluster, "nop\n");
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
			ReadByte(&buf);
			ReadByte(&buf);
			ReadByte(&buf);
			ReadByte(&buf);
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
//#define	svc_particle		18	// [vec3] <variable>
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
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, dem_read, (unsigned)-1);
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
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, to, mask);
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
			Multicast(tv, buf.data+buf.startpos, buf.readpos - buf.startpos, to, mask);
			break;

		case svc_updateuserinfo:
			ParseUpdateUserinfo(tv, &buf, to, mask);
			break;

//#define	svc_download		41		// [short] size [size bytes]
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

				if (tv->cluster->nobsp)
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
			}
			if (tv->usequkeworldprotocols)
			{
				if (i)
					SendClientCommand(tv, "modellist %i %i\n", tv->clservercount, i);
				else
					SendClientCommand(tv, "prespawn %i 0 %i\n", tv->clservercount, BSP_Checksum(tv->bsp));
			}
			break;
		case svc_soundlist:
			i = ParseList(tv, &buf, tv->soundlist, to, mask);
			if (tv->usequkeworldprotocols)
			{
				if (i)
					SendClientCommand(tv, "soundlist %i %i\n", tv->clservercount, i);
				else
					SendClientCommand(tv, "modellist %i 0\n", tv->clservercount);
			}
			break;
		case svc_packetentities:
			FlushPacketEntities(tv);
			ParsePacketEntities(tv, &buf);
			break;
		case svc_deltapacketentities:
			ReadByte(&buf);
			ParsePacketEntities(tv, &buf);
			break;
//#define svc_maxspeed		49		// maxspeed change, for prediction
case svc_entgravity:		// gravity change, for prediction
ReadFloat(&buf);
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
		default:
			buf.readpos = buf.startpos;
			Sys_Printf(tv->cluster, "Can't handle svc %i\n", (unsigned int)ReadByte(&buf));
			return;
		}
	}
}
