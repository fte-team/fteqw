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
/*
#define dem_cmd			0	//shouldn't be present
#define dem_read		1	//intended for the proxy, equivelent to dem_all :\
#define dem_set			2	//keeps the playerinfo packets in sync, present once, at start, with specific parameters. Ignored.
#define dem_multiple	3	//send to multiple specific players if tracking - basically team_prints.
#define	dem_single		4	//send to a single player, sprint, centerprint, etc
#define dem_stats		5	//overkill... same as single
#define dem_all			6	//broadcast to all
*/


	viewer_t *v;
	switch(to)
	{
	case dem_multiple:
	case dem_single:
	case dem_stats:
		//check and send to them only if they're tracking this player(s).
		for (v = tv->viewers; v; v = v->next)
		{
			if (v->trackplayer>=0)
				if ((1<<v->trackplayer)&playermask)
					SendBufferToViewer(v, buffer, length, true);	//FIXME: change the reliable depending on message type
		}
		break;
	default:
		//send to all
		for (v = tv->viewers; v; v = v->next)
		{
			SendBufferToViewer(v, buffer, length, true);	//FIXME: change the reliable depending on message type
		}
		break;
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

	ReadLong(m);	//we don't care about server's servercount, it's all reliable data anyway.

	ReadString(m, tv->gamedir, sizeof(tv->gamedir));

	/*tv->servertime =*/ ReadFloat(m);
	ReadString(m, tv->mapname, sizeof(tv->mapname));

	printf("Gamedir: %s\n", tv->gamedir);
	printf("---------------------\n");
	printf("%s\n", tv->mapname);
	printf("---------------------\n");

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

	for (v = tv->viewers; v; v = v->next)
		v->thinksitsconnected = false;

	tv->maxents = 0;	//clear these
	memset(tv->modellist, 0, sizeof(tv->modellist));
	memset(tv->soundlist, 0, sizeof(tv->soundlist));
	memset(tv->lightstyle, 0, sizeof(tv->lightstyle));
	tv->staticsound_count = 0;
}

static void ParseCDTrack(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	tv->cdtrack = ReadByte(m);

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}
static void ParseStufftext(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	viewer_t *v;
	char text[1024];
	ReadString(m, text, sizeof(text));

	if (!strcmp(text, "skins\n"))
	{
		const char newcmd[10] = {svc_stufftext, 'c', 'm', 'd', ' ', 'n','e','w','\n','\0'};
		tv->servercount++;
		tv->parsingconnectiondata = false;
		for (v = tv->viewers; v; v = v->next)
		{
			SendBufferToViewer(v, newcmd, sizeof(newcmd), true);
		}
		return;
	}
	else if (!strncmp(text, "fullserverinfo ", 15))
	{
		strncpy(tv->serverinfo, text+15, sizeof(tv->serverinfo)-1);
		return;
	}
	else if (!strncmp(text, "cmd ", 4))
		return;	//commands the game server asked for are pointless.

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseServerinfo(sv_t *tv, netmsg_t *m)
{
	char key[64];
	char value[64];
	ReadString(m, key, sizeof(key));
	ReadString(m, value, sizeof(value));
	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, dem_all, (unsigned)-1);
}

static void ParsePrint(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	char text[1024];
	int level;
	level = ReadByte(m);
	ReadString(m, text, sizeof(text));

	if (to == dem_all || to == dem_read)
	{
		if (level > 1)
			printf("%s", text);
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}
static void ParseCenterprint(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	char text[1024];
	ReadString(m, text, sizeof(text));

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}
static void ParseList(sv_t *tv, netmsg_t *m, filename_t *list, int to, unsigned int mask)
{
	int first;

	first = ReadByte(m)+1;
	for (; first < MAX_LIST; first++)
	{
		ReadString(m, list[first].name, sizeof(list[first].name));
		printf("read %i: %s\n", first, list[first].name);
		if (!*list[first].name)
			break;
//		printf("%i: %s\n", first, list[first].name);
	}

	ReadByte(m);	//wasted, we don't echo
}
static void ParseBaseline(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	int i;
	unsigned int entnum;
	entnum = ReadShort(m);
	if (entnum >= MAX_ENTITIES)
	{
		ParseError(m);
		return;
	}
	tv->baseline[entnum].modelindex = ReadByte(m);
	tv->baseline[entnum].frame = ReadByte(m);
	tv->baseline[entnum].colormap = ReadByte(m);
	tv->baseline[entnum].skinnum = ReadByte(m);
	for (i = 0; i < 3; i++)
	{
		tv->baseline[entnum].origin[i] = ReadShort(m);
		tv->baseline[entnum].angles[i] = ReadByte(m);
	}
}

static void ParseStaticSound(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	if (tv->staticsound_count == MAX_STATICSOUNDS)
	{
		tv->staticsound_count = 0;	// don't be fatal.
		printf("Too many static sounds, wrapping\n");
	}

	tv->staticsound[tv->staticsound_count].origin[0] = ReadShort(m);
	tv->staticsound[tv->staticsound_count].origin[1] = ReadShort(m);
	tv->staticsound[tv->staticsound_count].origin[2] = ReadShort(m);
	tv->staticsound[tv->staticsound_count].soundindex = ReadByte(m);
	tv->staticsound[tv->staticsound_count].volume = ReadByte(m);
	tv->staticsound[tv->staticsound_count].attenuation = ReadByte(m);

	tv->staticsound_count++;
	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}
static void ParsePlayerInfo(sv_t *tv, netmsg_t *m)
{
	int flags;
	int num;
	int i;
	num = ReadByte(m);
	if (num >= MAX_CLIENTS)
	{
		num = 0;	// don't be fatal.
		printf("Too many players, wrapping\n");
	}

	tv->players[num].old = tv->players[num].current;

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

static void FlushPacketEntities(sv_t *tv)
{
	int i;
	for (i = 0; i < MAX_ENTITIES; i++)
		tv->curents[i].modelindex = 0;
}

static void ParsePacketEntities(sv_t *tv, netmsg_t *m)
{
	int entnum;
	int flags;

	viewer_t *v;

	if (tv->chokeonnotupdated)
		for (v = tv->viewers; v; v = v->next)
		{
			v->chokeme = false;
		}

	for (entnum = 0; entnum < MAX_CLIENTS; entnum++)
	{	//hide players
		//they'll be sent after this packet.
		tv->players[entnum].active = false;
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
		memcpy(&tv->oldents[entnum], &tv->curents[entnum], sizeof(entity_state_t));	//ow.
		if (flags & U_REMOVE)
		{
			tv->curents[entnum].modelindex = 0;
			continue;
		}
		if (!tv->curents[entnum].modelindex)	//lerp from baseline
			memcpy(&tv->curents[entnum], &tv->baseline[entnum], sizeof(entity_state_t));

		if (flags & U_MOREBITS)
			flags |= ReadByte(m);
		if (flags & U_MODEL)
			tv->curents[entnum].modelindex = ReadByte(m);
		if (flags & U_FRAME)
			tv->curents[entnum].frame = ReadByte(m);
		if (flags & U_COLORMAP)
			tv->curents[entnum].colormap = ReadByte(m);
		if (flags & U_SKIN)
			tv->curents[entnum].skinnum = ReadByte(m);
		if (flags & U_EFFECTS)
			tv->curents[entnum].effects = ReadByte(m);

		if (flags & U_ORIGIN1)
			tv->curents[entnum].origin[0] = ReadShort(m);
		if (flags & U_ANGLE1)
			tv->curents[entnum].angles[0] = ReadByte(m);
		if (flags & U_ORIGIN2)
			tv->curents[entnum].origin[1] = ReadShort(m);
		if (flags & U_ANGLE2)
			tv->curents[entnum].angles[1] = ReadByte(m);
		if (flags & U_ORIGIN3)
			tv->curents[entnum].origin[2] = ReadShort(m);
		if (flags & U_ANGLE3)
			tv->curents[entnum].angles[2] = ReadByte(m);

		tv->entupdatetime[entnum] = tv->curtime;
		if (!tv->oldents[entnum].modelindex)	//no old state
			memcpy(&tv->oldents[entnum], &tv->curents[entnum], sizeof(entity_state_t));	//copy the new to the old, so we don't end up with interpolation glitches
	}
}

static void ParseUpdatePing(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	int pnum;
	int ping;
	pnum = ReadByte(m);
	ping = ReadShort(m);

	tv->players[pnum].ping = ping;

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseUpdateFrags(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	int pnum;
	int frags;
	pnum = ReadByte(m);
	frags = ReadShort(m);

	tv->players[pnum].frags = frags;

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseUpdateStat(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int pnum;
	int value;
	int statnum;

	statnum = ReadByte(m);
	value = ReadByte(m);

	for (pnum = 0; pnum < 32; pnum++)
	{
		if (mask & (1<<pnum))
			tv->players[pnum].stats[statnum] = value;
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}
static void ParseUpdateStatLong(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int pnum;
	int value;
	int statnum;

	statnum = ReadByte(m);
	value = ReadLong(m);

	for (pnum = 0; pnum < 32; pnum++)
	{
		if (mask & (1<<pnum))
			tv->players[pnum].stats[statnum] = value;
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseUpdateUserinfo(sv_t *tv, netmsg_t *m)
{
	int pnum;
	pnum = ReadByte(m);
	pnum%=MAX_CLIENTS;
	ReadLong(m);
	ReadString(m, tv->players[pnum].userinfo, sizeof(tv->players[pnum].userinfo));
}

static void ParsePacketloss(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int pnum;
	int value;

	pnum = ReadByte(m)%MAX_CLIENTS;
	value = ReadByte(m);

	tv->players[pnum].ping = value;

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

static void ParseUpdateEnterTime(sv_t *tv, netmsg_t *m, int to, unsigned int mask)
{
	unsigned int pnum;
	float value;

	pnum = ReadByte(m)%MAX_CLIENTS;
	value = ReadFloat(m);

	tv->players[pnum].entertime = value;

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
		printf("temp entity %i not recognised\n", i);
		return;
	}

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, to, mask);
}

void ParseLightstyle(sv_t *tv, netmsg_t *m)
{
	int style;
	style = ReadByte(m);
	if (style > MAX_LIGHTSTYLES)
		style=0;

	ReadString(m, tv->lightstyle[style].name, sizeof(tv->lightstyle[style].name));

	Multicast(tv, m->data+m->startpos, m->readpos - m->startpos, dem_read, (unsigned)-1);
}

void ParseMessage(sv_t *tv, char *buffer, int length, int to, int mask)
{
	netmsg_t buf;
	buf.cursize = length;
	buf.maxsize = length;
	buf.readpos = 0;
	buf.data = buffer;
	buf.startpos = 0;
	while(buf.readpos < buf.cursize)
	{
		if (buf.readpos > buf.cursize)
		{
			printf("Read past end of parse buffer\n");
			return;
		}
//		printf("%i\n", buf.buffer[0]);
		buf.startpos = buf.readpos;
		switch (ReadByte(&buf))
		{
		case svc_bad:
			ParseError(&buf);
			printf("ParseMessage: svc_bad\n");
			return;
		case svc_nop:	//quakeworld isn't meant to send these.
			printf("nop\n");
			break;

//#define	svc_disconnect		2

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
	
//#define	svc_spawnstatic		20
//#define	svc_spawnstatic2	21
		case svc_spawnbaseline:
			ParseBaseline(tv, &buf, to, mask);
			break;
	
		case svc_temp_entity:
			ParseTempEntity(tv, &buf, to, mask);
			break;

//#define	svc_setpause		24	// [qbyte] on / off
//#define	svc_signonnum		25	// [qbyte]  used for the signon sequence

		case svc_centerprint:
			ParseCenterprint(tv, &buf, to, mask);
			break;

//#define	svc_killedmonster	27
//#define	svc_foundsecret		28

		case svc_spawnstaticsound:
			ParseStaticSound(tv, &buf, to, mask);
			break;

//#define	svc_intermission	30		// [vec3_t] origin [vec3_t] angle
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
			ParseUpdateUserinfo(tv, &buf);
			break;

//#define	svc_download		41		// [short] size [size bytes]
		case svc_playerinfo:
			ParsePlayerInfo(tv, &buf);
			break;
//#define	svc_nails			43		// [qbyte] num [48 bits] xyzpy 12 12 12 4 8 
		case svc_chokecount:
			ReadByte(&buf);
			break;
		case svc_modellist:
			ParseList(tv, &buf, tv->modellist, to, mask);
			break;
		case svc_soundlist:
			ParseList(tv, &buf, tv->soundlist, to, mask);
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
//#define svc_entgravity		50		// gravity change, for prediction
//#define svc_setinfo			51		// setinfo on a client
		case svc_serverinfo:
			ParseServerinfo(tv, &buf);
			break;
		case svc_updatepl:
			ParsePacketloss(tv, &buf, to, mask);
			break;

//#define svc_nails2			54		//qwe - [qbyte] num [52 bits] nxyzpy 8 12 12 12 4 8 

		default:
			buf.readpos = buf.startpos;
			printf("Can't handle svc %i\n", (unsigned int)ReadByte(&buf));
			return;
		}
	}
}
