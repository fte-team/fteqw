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

#include "qwsvdef.h"
#include "winquake.h"

#define Q_strncatz strncat

void SV_MVDStop_f (void);



void SV_TimeOfDay(date_t *date)
{
	struct tm *newtime;
	time_t long_time;
	
	time( &long_time );
	newtime = localtime( &long_time );

	date->day = newtime->tm_mday;
	date->mon = newtime->tm_mon;
	date->year = newtime->tm_year + 1900;
	date->hour = newtime->tm_hour;
	date->min = newtime->tm_min;
	date->sec = newtime->tm_sec;
	strftime( date->str, 128,
         "%a %b %d, %H:%M:%S %Y", newtime);
}

// returns the file size
// return -1 if file is not present
// the file should be in BINARY mode for stupid OSs that care
#define MAX_DIRFILES 1000
#define MAX_MVD_NAME 64

typedef struct
{
	char	name[MAX_MVD_NAME];
	int		size;
} file_t;

typedef struct
{
	file_t *files;
	int		size;
	int		numfiles;
	int		numdirs;
} dir_t;

#define SORT_NO 0
#define SORT_BY_DATE 1

#ifdef _WIN32
dir_t Sys_listdir (char *path, char *ext, qboolean usesorting)
{
	static file_t	list[MAX_DIRFILES];
	dir_t	dir;
	HANDLE	h;
	WIN32_FIND_DATA fd;
	int		i, pos, size;
	char	name[MAX_MVD_NAME], *s;

	memset(list, 0, sizeof(list));
	memset(&dir, 0, sizeof(dir));

	dir.files = list;

	h = FindFirstFile (va("%s/*.*", path), &fd);
	if (h == INVALID_HANDLE_VALUE)
	{
		return dir;
	}
	
	do
	{
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			dir.numdirs++;
			continue;
		}

		size = fd.nFileSizeLow;
		Q_strncpyz (name, fd.cFileName, MAX_MVD_NAME);
		dir.size += size;

		for (s = fd.cFileName + strlen(fd.cFileName); s > fd.cFileName; s--)
		{
			if (*s == '.')
				break;
		}

		if (strcmp(s, ext))
			continue;

		// inclusion sort
		#if 0
		for (i=0 ; i<numfiles ; i++)
		{
			if (strcmp (name, list[i].name) < 0)
				break;
		}
		#endif

		i = dir.numfiles;
		pos = i;
		dir.numfiles++;
		for (i=dir.numfiles-1 ; i>pos ; i--)
			list[i] = list[i-1];

		strcpy (list[i].name, name);
		list[i].size = size;
		if (dir.numfiles == MAX_DIRFILES)
			break;
	} while ( FindNextFile(h, &fd) );
	FindClose (h);

	return dir;
}
#else
#include <dirent.h>
dir_t Sys_listdir (char *path, char *ext, qboolean usesorting)
{
	static file_t list[MAX_DIRFILES];
	dir_t	d;
	int		i, extsize;
	DIR		*dir;
    struct dirent *oneentry;
	char	pathname[MAX_OSPATH];
	qboolean all;

	memset(list, 0, sizeof(list));
	memset(&d, 0, sizeof(d));
	d.files = list;
	extsize = strlen(ext);
	all = !strcmp(ext, ".*");

	dir=opendir(path);
	if (!dir)
	{
		return d;
	}

	for(;;)
	{
		oneentry=readdir(dir);
		if(!oneentry) 
			break;

#ifndef __CYGWIN__
		if (oneentry->d_type == DT_DIR || oneentry->d_type == DT_LNK)
		{
			d.numdirs++;
			continue;
		}
#endif

		sprintf(pathname, "%s/%s", path, oneentry->d_name);
		list[d.numfiles].size = COM_FileSize(pathname);
		d.size += list[d.numfiles].size;

		i = strlen(oneentry->d_name);
		if (!all && (i < extsize || (Q_strcasecmp(oneentry->d_name+i-extsize, ext))))
			continue;

		Q_strncpyz(list[d.numfiles].name, oneentry->d_name, MAX_MVD_NAME);


		if (++d.numfiles == MAX_DIRFILES)
			break;
	}

	closedir(dir);

	return d;
}
#endif









#define MIN_MVD_MEMORY 0x100000
#define USACACHE (sv_demoUseCache.value && svs.demomemsize)
#define DWRITE(a,b,c,d) b*dwrite(a,b,c,d)
#define MAXSIZE (demobuffer->end < demobuffer->last ? \
				demobuffer->start - demobuffer->end : \
				demobuffer->maxsize - demobuffer->end)


cvar_t	sv_demoUseCache = {"sv_demoUseCache", ""};
cvar_t	sv_demoCacheSize = {"sv_demoCacheSize", ""};
cvar_t	sv_demoMaxDirSize = {"sv_demoMaxDirSize", ""};
cvar_t	sv_demoDir = {"sv_demoDir", ""};
cvar_t	sv_demofps = {"sv_demofps", ""};
cvar_t	sv_demoPings = {"sv_demoPings", ""};
cvar_t	sv_demoNoVis = {"sv_demoNoVis", ""};
cvar_t	sv_demoMaxSize = {"sv_demoMaxSize", ""};
cvar_t	sv_demoExtraNames = {"sv_demoExtraNames", ""};

static int		demo_max_size;
static int		demo_size;
cvar_t			sv_demoPrefix = {"sv_demoPrefix", ""};
cvar_t			sv_demoSuffix = {"sv_demoSuffix", ""};
cvar_t			sv_demotxt = {"sv_demotxt", "1"};

void SV_WriteMVDMessage (sizebuf_t *msg, int type, int to, float time);
size_t (*dwrite) ( const void *buffer, size_t size, size_t count, void *stream);

demo_t			demo;
static dbuffer_t	*demobuffer;
static int	header = (int)&((header_t*)0)->data;

entity_state_t demo_entities[UPDATE_MASK+1][MAX_MVDPACKET_ENTITIES];
client_frame_t demo_frames[UPDATE_MASK+1];

// only one .. is allowed (security)
qboolean sv_demoDir_OnChange(cvar_t *cvar, char *value)
{
	if (!value[0])
		return true;

	if (value[0] == '.' && value[1] == '.')
		value += 2;
	if (strstr(value,"/.."))
		return true;

	return false;
}

void SV_MVDPings (void)
{
	client_t *client;
	int		j;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state != cs_spawned)
			continue;

		MVDWrite_Begin (dem_all, 0, 7);
		MSG_WriteByte((sizebuf_t*)demo.dbuf, svc_updateping);
		MSG_WriteByte((sizebuf_t*)demo.dbuf,  j);
		MSG_WriteShort((sizebuf_t*)demo.dbuf,  SV_CalcPing(client));
		MSG_WriteByte((sizebuf_t*)demo.dbuf, svc_updatepl);
		MSG_WriteByte ((sizebuf_t*)demo.dbuf, j);
		MSG_WriteByte ((sizebuf_t*)demo.dbuf, client->lossage);
	}
}

void MVDBuffer_Init(dbuffer_t *dbuffer, qbyte *buf, size_t size)
{
	demobuffer = dbuffer;

	demobuffer->data = buf;
	demobuffer->maxsize = size;
	demobuffer->start = 0;
	demobuffer->end = 0;
	demobuffer->last = 0;
}

/*
==============
MVD_SetMsgBuf

Sets the frame message buffer
==============
*/

void MVDSetMsgBuf(demobuf_t *prev,demobuf_t *cur)
{
	// fix the maxsize of previous msg buffer,
	// we won't be able to write there anymore
	if (prev != NULL)
		prev->maxsize = prev->bufsize;

	demo.dbuf = cur;
	memset(demo.dbuf, 0, sizeof(*demo.dbuf));

	demo.dbuf->data = demobuffer->data + demobuffer->end;
	demo.dbuf->maxsize = MAXSIZE;
}

/*
==============
DemoWriteToDisk

Writes to disk a message meant for specifc client
or all messages if type == 0
Message is cleared from demobuf after that
==============
*/

void SV_MVDWriteToDisk(int type, int to, float time)
{
	int pos = 0, oldm, oldd;
	header_t *p;
	int	size;
	sizebuf_t msg;

	(qbyte*)p = demo.dbuf->data;
	demo.dbuf->h = NULL;

	oldm = demo.dbuf->bufsize;
	oldd = demobuffer->start;
	while (pos < demo.dbuf->bufsize)
	{
		size = p->size;
		pos += header + size;

		// no type means we are writing to disk everything
		if (!type || (p->type == type && p->to == to))
		{
			if (size) {
				msg.data = p->data;
				msg.cursize = size;

				SV_WriteMVDMessage(&msg, p->type, p->to, time);
			}

			// data is written so it need to be cleard from demobuf
			if (demo.dbuf->data != (qbyte*)p)
				memmove(demo.dbuf->data + size + header, demo.dbuf->data, (qbyte*)p - demo.dbuf->data);

			demo.dbuf->bufsize -= size + header;
			demo.dbuf->data += size + header;
			pos -= size + header;
			demo.dbuf->maxsize -= size + header;
			demobuffer->start += size + header;
		}
		// move along
		(qbyte*)p = p->data + size;
	}

	if (demobuffer->start == demobuffer->last) {
		if (demobuffer->start == demobuffer->end) {
			demobuffer->end = 0; // demobuffer is empty
			demo.dbuf->data = demobuffer->data;
		}

		// go back to begining of the buffer
		demobuffer->last = demobuffer->end;
		demobuffer->start = 0;
	}
}

/*
==============
MVDSetBuf

Sets position in the buf for writing to specific client
==============
*/

static void MVDSetBuf(qbyte type, int to)
{
	header_t *p;
	int pos = 0;

	(qbyte*)p = demo.dbuf->data;

	while (pos < demo.dbuf->bufsize)
	{
		pos += header + p->size;

		if (type == p->type && to == p->to && !p->full)
		{
			demo.dbuf->cursize = pos;
			demo.dbuf->h = p;
			return;
		}

		(qbyte*)p = p->data + p->size;
	}
	// type&&to not exist in the buf, so add it

	p->type = type;
	p->to = to;
	p->size = 0;
	p->full = 0;

	demo.dbuf->bufsize += header;
	demo.dbuf->cursize = demo.dbuf->bufsize;
	demobuffer->end += header;
	demo.dbuf->h = p;
}

void MVDMoveBuf(void)
{
	// set the last message mark to the previous frame (i/e begining of this one)
	demobuffer->last = demobuffer->end - demo.dbuf->bufsize;

	// move buffer to the begining of demo buffer
	memmove(demobuffer->data, demo.dbuf->data, demo.dbuf->bufsize);
	demo.dbuf->data = demobuffer->data;
	demobuffer->end = demo.dbuf->bufsize;
	demo.dbuf->h = NULL; // it will be setup again
	demo.dbuf->maxsize = MAXSIZE + demo.dbuf->bufsize;
}

void MVDWrite_Begin(qbyte type, int to, int size)
{
	qbyte *p;
	qboolean move = false;

	// will it fit?
	while (demo.dbuf->bufsize + size + header > demo.dbuf->maxsize)
	{
		// if we reached the end of buffer move msgbuf to the begining
		if (!move && demobuffer->end > demobuffer->start)
			move = true;

		SV_MVDWritePackets(1);
		if (move && demobuffer->start > demo.dbuf->bufsize + header + size)
			MVDMoveBuf();
	}

	if (demo.dbuf->h == NULL || demo.dbuf->h->type != type || demo.dbuf->h->to != to || demo.dbuf->h->full) {
		MVDSetBuf(type, to);
	}

	if (demo.dbuf->h->size + size > MAX_QWMSGLEN)
	{
		demo.dbuf->h->full = 1;
		MVDSetBuf(type, to);
	}

	// we have to make room for new data
	if (demo.dbuf->cursize != demo.dbuf->bufsize) {
		p = demo.dbuf->data + demo.dbuf->cursize;
		memmove(p+size, p, demo.dbuf->bufsize - demo.dbuf->cursize);
	}

	demo.dbuf->bufsize += size;
	demo.dbuf->h->size += size;
	if ((demobuffer->end += size) > demobuffer->last)
		demobuffer->last = demobuffer->end;
}

/*
====================
SV_WriteMVDMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void SV_WriteMVDMessage (sizebuf_t *msg, int type, int to, float time)
{
	int		len, i, msec;
	qbyte	c;
	static double prevtime;

	if (!sv.mvdrecording)
		return;

	msec = (time - prevtime)*1000;
	prevtime += msec*0.001;
	if (msec > 255) msec = 255;
	if (msec < 2) msec = 0;

	c = msec;
	demo.size += DWRITE(&c, sizeof(c), 1, demo.dest);

	if (demo.lasttype != type || demo.lastto != to)
	{
		demo.lasttype = type;
		demo.lastto = to;
		switch (demo.lasttype)
		{
		case dem_all:
			c = dem_all;
			demo.size += DWRITE (&c, sizeof(c), 1, demo.dest);
			break;
		case dem_multiple:
			c = dem_multiple;
			demo.size += DWRITE (&c, sizeof(c), 1, demo.dest);

			i = LittleLong(demo.lastto);
			demo.size += DWRITE (&i, sizeof(i), 1, demo.dest);
			break;
		case dem_single:
		case dem_stats:
			c = demo.lasttype + (demo.lastto << 3);
			demo.size += DWRITE (&c, sizeof(c), 1, demo.dest);
			break;
		default:
			SV_MVDStop_f ();
			Con_Printf("bad demo message type:%d", type);
			return;
		}
	} else {
		c = dem_read;
		demo.size += DWRITE (&c, sizeof(c), 1, demo.dest);
	}


	len = LittleLong (msg->cursize);
	demo.size += DWRITE (&len, 4, 1, demo.dest);
	demo.size += DWRITE (msg->data, msg->cursize, 1, demo.dest);

	if (demo.disk)
		fflush (demo.file);
	else if (demo.size - demo_size > demo_max_size)
	{
		demo_size = demo.size;
		demo.mfile -= 0x80000;
		fwrite(svs.demomem, 1, 0x80000, demo.file);
		fflush(demo.file);
		memmove(svs.demomem, svs.demomem + 0x80000, demo.size - 0x80000);
	}
}


/*
====================
SV_MVDWritePackets

Interpolates to get exact players position for current frame
and writes packets to the disk/memory
====================
*/

float adjustangle(float current, float ideal, float fraction)
{
	float move;

	move = ideal - current;
	if (ideal > current)
	{

		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}

	move *= fraction;

	return (current + move);
}

#define DF_ORIGIN	1
#define DF_ANGLES	(1<<3)
#define DF_EFFECTS	(1<<6)
#define DF_SKINNUM	(1<<7)
#define DF_DEAD		(1<<8)
#define DF_GIB		(1<<9)
#define DF_WEAPONFRAME (1<<10)
#define DF_MODEL	(1<<11)

void SV_MVDWritePackets (int num)
{
	demo_frame_t	*frame, *nextframe;
	demo_client_t	*cl, *nextcl = NULL;
	int				i, j, flags;
	qboolean		valid;
	double			time, playertime, nexttime;
	float			f;
	vec3_t			origin, angles;
	sizebuf_t		msg;
	qbyte			msg_buf[MAX_QWMSGLEN];
	demoinfo_t		*demoinfo;

	if (!sv.mvdrecording)
		return;

	msg.data = msg_buf;
	msg.maxsize = sizeof(msg_buf);

	if (num > demo.parsecount - demo.lastwritten + 1)
		num = demo.parsecount - demo.lastwritten + 1;

	// 'num' frames to write
	for ( ; num; num--, demo.lastwritten++)
	{
		frame = &demo.frames[demo.lastwritten&DEMO_FRAMES_MASK];
		time = frame->time;
		nextframe = frame;
		msg.cursize = 0;

		demo.dbuf = &frame->buf;

		// find two frames
		// one before the exact time (time - msec) and one after,
		// then we can interpolte exact position for current frame
		for (i = 0, cl = frame->clients, demoinfo = demo.info; i < MAX_CLIENTS; i++, cl++, demoinfo++)
		{
			if (cl->parsecount != demo.lastwritten)
				continue; // not valid

			nexttime = playertime = time - cl->sec;

			for (j = demo.lastwritten+1, valid = false; nexttime < time && j < demo.parsecount; j++)
			{
				nextframe = &demo.frames[j&DEMO_FRAMES_MASK];
				nextcl = &nextframe->clients[i];

				if (nextcl->parsecount != j)
					break; // disconnected?
				if (nextcl->fixangle)
					break; // respawned, or walked into teleport, do not interpolate!
				if (!(nextcl->flags & DF_DEAD) && (cl->flags & DF_DEAD))
					break; // respawned, do not interpolate

				nexttime = nextframe->time - nextcl->sec;

				if (nexttime >= time)
				{
					// good, found what we were looking for
					valid = true;
					break;
				}
			}

			if (valid)
			{
				f = (time - nexttime)/(nexttime - playertime);
				for (j=0;j<3;j++) {
					angles[j] = adjustangle(cl->info.angles[j], nextcl->info.angles[j],1.0+f);
					origin[j] = nextcl->info.origin[j] + f*(nextcl->info.origin[j]-cl->info.origin[j]);
				}
			} else {
				VectorCopy(cl->info.origin, origin);
				VectorCopy(cl->info.angles, angles);
			}

			// now write it to buf
			flags = cl->flags;

			if (cl->fixangle) {
				demo.fixangletime[i] = cl->cmdtime;
			}

			for (j=0; j < 3; j++)
				if (origin[j] != demoinfo->origin[i])
					flags |= DF_ORIGIN << j;

			if (cl->fixangle || demo.fixangletime[i] != cl->cmdtime)
			{
				for (j=0; j < 3; j++)
					if (angles[j] != demoinfo->angles[j])
						flags |= DF_ANGLES << j;
			}

			if (cl->info.model != demoinfo->model)
				flags |= DF_MODEL;
			if (cl->info.effects != demoinfo->effects)
				flags |= DF_EFFECTS;
			if (cl->info.skinnum != demoinfo->skinnum)
				flags |= DF_SKINNUM;
			if (cl->info.weaponframe != demoinfo->weaponframe)
				flags |= DF_WEAPONFRAME;

			MSG_WriteByte (&msg, svc_playerinfo);
			MSG_WriteByte (&msg, i);
			MSG_WriteShort (&msg, flags);

			MSG_WriteByte (&msg, cl->frame);

			for (j=0 ; j<3 ; j++)
				if (flags & (DF_ORIGIN << j))
					MSG_WriteCoord (&msg, origin[j]);
		
			for (j=0 ; j<3 ; j++)
				if (flags & (DF_ANGLES << j))
					MSG_WriteAngle16 (&msg, angles[j]);


			if (flags & DF_MODEL)
				MSG_WriteByte (&msg, cl->info.model);

			if (flags & DF_SKINNUM)
				MSG_WriteByte (&msg, cl->info.skinnum);

			if (flags & DF_EFFECTS)
				MSG_WriteByte (&msg, cl->info.effects);

			if (flags & DF_WEAPONFRAME)
				MSG_WriteByte (&msg, cl->info.weaponframe);

			VectorCopy(cl->info.origin, demoinfo->origin);
			VectorCopy(cl->info.angles, demoinfo->angles);
			demoinfo->skinnum = cl->info.skinnum;
			demoinfo->effects = cl->info.effects;
			demoinfo->weaponframe = cl->info.weaponframe;
			demoinfo->model = cl->info.model;
		}

		SV_MVDWriteToDisk(demo.lasttype,demo.lastto, (float)time); // this goes first to reduce demo size a bit
		SV_MVDWriteToDisk(0,0, (float)time); // now goes the rest
		if (msg.cursize)
			SV_WriteMVDMessage(&msg, dem_all, 0, (float)time);
	}

	if (demo.lastwritten > demo.parsecount)
		demo.lastwritten = demo.parsecount;

	demo.dbuf = &demo.frames[demo.parsecount&DEMO_FRAMES_MASK].buf;
	demo.dbuf->maxsize = MAXSIZE + demo.dbuf->bufsize;
}

size_t memwrite ( const void *buffer, size_t size, size_t count, qbyte **mem)
{
	int i,c = count;
	const qbyte *buf;

	for (;count; count--)
		for (i = size, buf = buffer; i; i--)
			*(*mem)++ = *buf++;

	return c;
}

static char chartbl[256];
void CleanName_Init ();

void MVD_Init (void)
{
	int p, size = MIN_MVD_MEMORY;

#define MVDVARGROUP "Server MVD cvars"

	Cvar_Register (&sv_demofps,			MVDVARGROUP);
	Cvar_Register (&sv_demoPings,		MVDVARGROUP);
	Cvar_Register (&sv_demoNoVis,		MVDVARGROUP);
	Cvar_Register (&sv_demoUseCache,	MVDVARGROUP);
	Cvar_Register (&sv_demoCacheSize,	MVDVARGROUP);
	Cvar_Register (&sv_demoMaxSize,		MVDVARGROUP);
	Cvar_Register (&sv_demoMaxDirSize,	MVDVARGROUP);
	Cvar_Register (&sv_demoDir,			MVDVARGROUP);
	Cvar_Register (&sv_demoPrefix,		MVDVARGROUP);
	Cvar_Register (&sv_demoSuffix,		MVDVARGROUP);
	Cvar_Register (&sv_demotxt		,	MVDVARGROUP);
	Cvar_Register (&sv_demoExtraNames,	MVDVARGROUP);


	p = COM_CheckParm ("-democache");
	if (p)
	{
		if (p < com_argc-1)
			size = Q_atoi (com_argv[p+1]) * 1024;
		else
			Sys_Error ("Memory_Init: you must specify a size in KB after -democache");
	}

	if (size < MIN_MVD_MEMORY)
	{
		Con_Printf("Minimum memory size for demo cache is %dk\n", MIN_MVD_MEMORY / 1024);
		size = MIN_MVD_MEMORY;
	}

	svs.demomem = Hunk_AllocName ( size, "demo" );
	svs.demomemsize = size;
	demo_max_size = size - 0x80000;
//	Cvar_SetROM(&sv_demoCacheSize, va("%d", size/1024));
	CleanName_Init();
}

/*
====================
SV_InitRecord
====================
*/

qboolean SV_InitRecord(void)
{
	if (!USACACHE)
	{
		dwrite = (void *)&fwrite;
		demo.dest = demo.file;
		demo.disk = true;
	} else 
	{
		dwrite = (void *)&memwrite;
		demo.mfile = svs.demomem;
		demo.dest = &demo.mfile;
	}

	demo_size = 0;

	return true;
}

/*
====================
SV_Stop

stop recording a demo
====================
*/
void SV_MVDStop (int reason)
{
	if (!sv.mvdrecording)
	{
		Con_Printf ("Not recording a demo.\n");
		return;
	}

	if (reason == 2)
	{
		char path[MAX_OSPATH];
		// stop and remove
		if (demo.disk)
			fclose(demo.file);

		_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, demo.path, demo.name);
		Sys_remove(path);

		Q_strncpyz(path + strlen(path) - 3, "txt", MAX_OSPATH - strlen(path) + 3);
		Sys_remove(path);

		demo.file = NULL;
		sv.mvdrecording = false;

		SV_BroadcastPrintf (PRINT_CHAT, "Server recording canceled, demo removed\n");

//		Cvar_SetROM(&serverdemo, "");

		return;
	}
// write a disconnect message to the demo file

	// clearup to be sure message will fit
	demo.dbuf->cursize = 0;
	demo.dbuf->h = NULL;
	demo.dbuf->bufsize = 0;
	MVDWrite_Begin(dem_all, 0, 2+strlen("EndOfDemo"));
	MSG_WriteByte ((sizebuf_t*)demo.dbuf, svc_disconnect);
	MSG_WriteString ((sizebuf_t*)demo.dbuf, "EndOfDemo");

	SV_MVDWritePackets(demo.parsecount - demo.lastwritten + 1);
// finish up
	if (!demo.disk)
	{
		fwrite(svs.demomem, 1, demo.size - demo_size, demo.file);
		fflush(demo.file);
	}

	fclose (demo.file);
	
	demo.file = NULL;
	sv.mvdrecording = false;
	if (!reason)
		SV_BroadcastPrintf (PRINT_CHAT, "Server recording completed\n");
	else
		SV_BroadcastPrintf (PRINT_CHAT, "Server recording stoped\nMax demo size exceeded\n");

//	Cvar_SetROM(&serverdemo, "");
}

/*
====================
SV_Stop_f
====================
*/
void SV_MVDStop_f (void)
{
	SV_MVDStop(0);
}

/*
====================
SV_Cancel_f

Stops recording, and removes the demo
====================
*/
void SV_MVD_Cancel_f (void)
{
	SV_MVDStop(2);
}

/*
====================
SV_WriteMVDMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/

void SV_WriteRecordMVDMessage (sizebuf_t *msg, int seq)
{
	int		len;
	qbyte	c;

	if (!sv.mvdrecording)
		return;

	c = 0;
	demo.size += DWRITE (&c, sizeof(c), 1, demo.dest);

	c = dem_read;
	demo.size += DWRITE (&c, sizeof(c), 1, demo.dest);

	len = LittleLong (msg->cursize);
	demo.size += DWRITE (&len, 4, 1, demo.dest);

	demo.size += DWRITE (msg->data, msg->cursize, 1, demo.dest);

	if (demo.disk)
		fflush (demo.file);
}

void SV_WriteSetMVDMessage (void)
{
	int		len;
	qbyte	c;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	if (!sv.mvdrecording)
		return;

	c = 0;
	demo.size += DWRITE (&c, sizeof(c), 1, demo.dest);

	c = dem_set;
	demo.size += DWRITE (&c, sizeof(c), 1, demo.dest);

	
	len = LittleLong(0);
	demo.size += DWRITE (&len, 4, 1, demo.dest);
	len = LittleLong(0);
	demo.size += DWRITE (&len, 4, 1, demo.dest);

	if (demo.disk)
		fflush (demo.file);
}

static char *SV_PrintTeams(void)
{
	char *teams[MAX_CLIENTS];
//	char *p;
	int	i, j, numcl = 0, numt = 0;
	client_t *clients[MAX_CLIENTS];
	char buf[2048] = {0};
	extern cvar_t teamplay;
//	extern char chartbl2[];

	// count teams and players
	for (i=0; i < MAX_CLIENTS; i++)
	{
		if (svs.clients[i].state != cs_spawned)
			continue;
		if (svs.clients[i].spectator)
			continue;

		clients[numcl++] = &svs.clients[i];
		for (j = 0; j < numt; j++)
			if (!strcmp(Info_ValueForKey(svs.clients[i].userinfo, "team"), teams[j]))
				break;
		if (j != numt)
			continue;

		teams[numt++] = Info_ValueForKey(svs.clients[i].userinfo, "team");
	}

	// create output
	
	if (numcl == 2) // duel
	{
		_snprintf(buf, sizeof(buf), "team1 %s\nteam2 %s\n", clients[0]->name, clients[1]->name);
	} else if (!teamplay.value) // ffa
	{ 
		_snprintf(buf, sizeof(buf), "players:\n");
		for (i = 0; i < numcl; i++)
			_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  %s\n", clients[i]->name);
	} else { // teamplay
		for (j = 0; j < numt; j++) {
			_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "team %s:\n", teams[j]);
			for (i = 0; i < numcl; i++)
				if (!strcmp(Info_ValueForKey(clients[i]->userinfo, "team"), teams[j]))
					_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  %s\n", clients[i]->name);
		}
	}

	if (!numcl)
		return "\n";
//	for (p = buf; *p; p++) *p = chartbl2[(qbyte)*p];
	return va("%s",buf);
}

static void SV_MVD_Record (char *name)
{
	sizebuf_t	buf;
	char buf_data[MAX_QWMSGLEN];
	int n, i;
	char *s, info[MAX_INFO_STRING], path[MAX_OSPATH];
	
	client_t *player;
	char *gamedir;
	int seq = 1;

	memset(&demo, 0, sizeof(demo));
	demo.recorder.frames = demo_frames;
	for (i = 0; i < UPDATE_BACKUP; i++)
	{
		demo.recorder.frames[i].entities.max_entities = MAX_MVDPACKET_ENTITIES;
		demo.recorder.frames[i].entities.entities = demo_entities[i];
	}
	
	MVDBuffer_Init(&demo.dbuffer, demo.buffer, sizeof(demo.buffer));
	MVDSetMsgBuf(NULL, &demo.frames[0].buf);

	demo.datagram.maxsize = sizeof(demo.datagram_data);
	demo.datagram.data = demo.datagram_data;

	demo.file = fopen (name, "wb");
	if (!demo.file)
	{
		Con_Printf ("ERROR: couldn't open %s\n", name);
		return;
	}

	SV_InitRecord();

	s = name + strlen(name);
	while (*s != '/') s--;
	Q_strncpyz(demo.name, s+1, sizeof(demo.name));
	Q_strncpyz(demo.path, sv_demoDir.string, sizeof(demo.path));
	
	if (!*demo.path)
		Q_strncpyz(demo.path, ".", MAX_OSPATH);

	SV_BroadcastPrintf (PRINT_CHAT, "Server starts recording (%s):\n%s\n", demo.disk ? "disk" : "memory", demo.name);
//	Cvar_SetROM(&serverdemo, demo.name);

	Q_strncpyz(path, name, MAX_OSPATH);
	Q_strncpyz(path + strlen(path) - 3, "txt", MAX_OSPATH - strlen(path) + 3);

	if (sv_demotxt.value)
	{
		FILE *f;

		f = fopen (path, "w+t");
		if (f != NULL)
		{
			char buf[2000];
			date_t date;

			SV_TimeOfDay(&date);

			_snprintf(buf, sizeof(buf), "date %s\nmap %s\nteamplay %d\ndeathmatch %d\ntimelimit %d\n%s",date.str, sv.name, (int)teamplay.value, (int)deathmatch.value, (int)timelimit.value, SV_PrintTeams());
			fwrite(buf, strlen(buf),1,f);
			fflush(f);
			fclose(f);
		}
	}
	else 
		Sys_remove(path);

	sv.mvdrecording = true;
	demo.pingtime = demo.time = sv.time;

/*-------------------------------------------------*/

// serverdata
	// send the info about the new client to all connected clients
	memset(&buf, 0, sizeof(buf));
	buf.data = buf_data;
	buf.maxsize = sizeof(buf_data);

// send the serverdata

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, svs.spawncount);
	MSG_WriteString (&buf, gamedir);


	MSG_WriteFloat (&buf, sv.time);

	// send full levelname
	MSG_WriteString (&buf, sv.mapname);

	// send the movevars
	MSG_WriteFloat(&buf, movevars.gravity);
	MSG_WriteFloat(&buf, movevars.stopspeed);
	MSG_WriteFloat(&buf, movevars.maxspeed);
	MSG_WriteFloat(&buf, movevars.spectatormaxspeed);
	MSG_WriteFloat(&buf, movevars.accelerate);
	MSG_WriteFloat(&buf, movevars.airaccelerate);
	MSG_WriteFloat(&buf, movevars.wateraccelerate);
	MSG_WriteFloat(&buf, movevars.friction);
	MSG_WriteFloat(&buf, movevars.waterfriction);
	MSG_WriteFloat(&buf, movevars.entgravity);

	// send music
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0); // none in demos

	// send server info string
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("fullserverinfo \"%s\"\n", svs.info) );

	// flush packet
	SV_WriteRecordMVDMessage (&buf, seq++);
	SZ_Clear (&buf);

// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = sv.sound_precache[n+1];
	while (*s)
	{
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_QWMSGLEN/2)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			SV_WriteRecordMVDMessage (&buf, seq++);
			SZ_Clear (&buf); 
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = sv.sound_precache[n+1];
	}

	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		SV_WriteRecordMVDMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = sv.model_precache[n+1];
	while (*s)
	{
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_QWMSGLEN/2)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			SV_WriteRecordMVDMessage (&buf, seq++);
			SZ_Clear (&buf); 
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = sv.model_precache[n+1];
	}
	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		SV_WriteRecordMVDMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

// baselines
	{
		entity_state_t from;
		edict_t *ent;
		entity_state_t *state;

		memset(&from, 0, sizeof(from));

		for (n = 0; n < sv.num_edicts; n++)
		{
			ent = EDICT_NUM(svprogfuncs, n);
			state = &ent->baseline;

			if (!state->number || !state->modelindex)
			{	//ent doesn't have a baseline
				continue;
			}

			if (!ent)
			{
				MSG_WriteByte(&buf, svc_spawnbaseline);

				MSG_WriteShort (&buf, n);

				MSG_WriteByte (&buf, 0);

				MSG_WriteByte (&buf, 0);
				MSG_WriteByte (&buf, 0);
				MSG_WriteByte (&buf, 0);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&buf, 0);
					MSG_WriteAngle(&buf, 0);
				}
			}
	/*		else if (host_client->fteprotocolextensions & PEXT_SPAWNSTATIC2)
			{
				MSG_WriteByte(&buf, svc_spawnbaseline2);
				SV_WriteDelta(&from, state, &buf, true, host_client->fteprotocolextensions);
			}*/
			else
			{
				MSG_WriteByte(&buf, svc_spawnbaseline);

				MSG_WriteShort (&buf, n);

				MSG_WriteByte (&buf, state->modelindex&255);

				MSG_WriteByte (&buf, state->frame);
				MSG_WriteByte (&buf, (int)state->colormap);
				MSG_WriteByte (&buf, (int)state->skinnum);
				for (i=0 ; i<3 ; i++)
				{
					MSG_WriteCoord(&buf, state->origin[i]);
					MSG_WriteAngle(&buf, state->angles[i]);
				}
			}
			if (buf.cursize > MAX_QWMSGLEN/2)
			{
				SV_WriteRecordMVDMessage (&buf, seq++);
				SZ_Clear (&buf); 
			}
		}
	}

	//prespawn

	for (n = 0; n < sv.num_signon_buffers; n++)
	{
		SZ_Write (&buf, 
			sv.signon_buffers[n],
			sv.signon_buffer_size[n]);

		if (buf.cursize > MAX_QWMSGLEN/2)
		{
			SV_WriteRecordMVDMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("cmd spawn %i\n",svs.spawncount) );
	
	if (buf.cursize)
	{
		SV_WriteRecordMVDMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

// send current status of all other players

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		player = svs.clients + i;

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->old_frags);
		
		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, SV_CalcPing(player));
		
		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->lossage);
		
		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, realtime - player->connection_started);

		Q_strncpyz (info, player->userinfo, MAX_INFO_STRING);
		Info_RemovePrefixedKeys (info, '_');	// server passwords, etc

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, info);

		if (buf.cursize > MAX_QWMSGLEN/2)
		{
			SV_WriteRecordMVDMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}
	
// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char)i);
		MSG_WriteString (&buf, sv.lightstyles[i]);
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("skins\n") );

	SV_WriteRecordMVDMessage (&buf, seq++);

	SV_WriteSetMVDMessage();
	// done
}

/*
====================
SV_CleanName_Init

sets chararcter table for quake text->filename translation
====================
*/

void CleanName_Init ()
{
	int i;

	for (i = 0; i < 256; i++)
		chartbl[i] = (((i&127) < 'a' || (i&127) > 'z') && ((i&127) < '0' || (i&127) > '9')) ? '_' : (i&127);

	// special cases

	// numbers
	for (i = 18; i < 29; i++)
		chartbl[i] = chartbl[i + 128] = i + 30;

	// allow lowercase only
	for (i = 'A'; i <= 'Z'; i++)
		chartbl[i] = chartbl[i+128] = i + 'a' - 'A';

	// brackets
	chartbl[29] = chartbl[29+128] = chartbl[128] = '(';
	chartbl[31] = chartbl[31+128] = chartbl[130] = ')';
	chartbl[16] = chartbl[16 + 128]= '[';
	chartbl[17] = chartbl[17 + 128] = ']';

	// dot
	chartbl[5] = chartbl[14] = chartbl[15] = chartbl[28] = chartbl[46] = '.';
	chartbl[5 + 128] = chartbl[14 + 128] = chartbl[15 + 128] = chartbl[28 + 128] = chartbl[46 + 128] = '.';

	// !
	chartbl[33] = chartbl[33 + 128] = '!';

	// #
	chartbl[35] = chartbl[35 + 128] = '#';

	// %
	chartbl[37] = chartbl[37 + 128] = '%';

	// &
	chartbl[38] = chartbl[38 + 128] = '&';

	// '
	chartbl[39] = chartbl[39 + 128] = '\'';

	// (
	chartbl[40] = chartbl[40 + 128] = '(';

	// )
	chartbl[41] = chartbl[41 + 128] = ')';

	// +
	chartbl[43] = chartbl[43 + 128] = '+';

	// -
	chartbl[45] = chartbl[45 + 128] = '-';

	// @
	chartbl[64] = chartbl[64 + 128] = '@';

	// ^
//	chartbl[94] = chartbl[94 + 128] = '^';


	chartbl[91] = chartbl[91 + 128] = '[';
	chartbl[93] = chartbl[93 + 128] = ']';
	
	chartbl[16] = chartbl[16 + 128] = '[';
	chartbl[17] = chartbl[17 + 128] = ']';

	chartbl[123] = chartbl[123 + 128] = '{';
	chartbl[125] = chartbl[125 + 128] = '}';
}

/*
====================
SV_CleanName

Cleans the demo name, removes restricted chars, makes name lowercase
====================
*/

char *SV_CleanName (unsigned char *name)
{
	static char text[1024];
	char *out = text;

	*out = chartbl[*name++];

	while (*name && out - text < sizeof(text))
		if (*out == '_' && chartbl[*name] == '_')
			name++;
		else *++out = chartbl[*name++];

	*++out = 0;
	return text;
}

/*
====================
SV_Record_f

record <demoname>
====================
*/
void SV_MVD_Record_f (void)
{
	int		c;
	char	name[MAX_OSPATH+MAX_MVD_NAME];
	char	newname[MAX_MVD_NAME];
	dir_t	dir;

	c = Cmd_Argc();
	if (c != 2)
	{
		Con_Printf ("record <demoname>\n");
		return;
	}

	if (sv.state != ss_active) {
		Con_Printf ("Not active yet.\n");
		return;
	}

	
	dir = Sys_listdir(va("%s/%s", com_gamedir, sv_demoDir.string), ".*", SORT_NO);
	if (sv_demoMaxDirSize.value && dir.size > sv_demoMaxDirSize.value*1024)
	{
		Con_Printf("insufficient directory space, increase sv_demoMaxDirSize\n");
		return;
	}

	Q_strncpyz(newname, va("%s%s", sv_demoPrefix.string, SV_CleanName(Cmd_Argv(1))),
			sizeof(newname) - strlen(sv_demoSuffix.string) - 5);
	Q_strncatz(newname, sv_demoSuffix.string, MAX_MVD_NAME);
  
	_snprintf (name, MAX_OSPATH+MAX_MVD_NAME, "%s/%s/%s", com_gamedir, sv_demoDir.string, newname);

	if (sv.mvdrecording)
		SV_MVDStop_f();


	Sys_mkdir(va("%s/%s", com_gamedir, sv_demoDir.string));

//
// open the demo file
//
	COM_StripExtension(name, name);
	COM_DefaultExtension(name, ".mvd");

	
	SV_MVD_Record (name);
}

/*
====================
SV_EasyRecord_f

easyrecord [demoname]
====================
*/

int	Dem_CountPlayers ()
{
	int	i, count;

	count = 0;
	for (i = 0; i < MAX_CLIENTS ; i++) {
		if (svs.clients[i].name[0] && !svs.clients[i].spectator)
			count++;
	}

	return count;
}

char *Dem_Team(int num)
{
	int i;
	static char *lastteam[2];
	qboolean first = true;
	client_t *client;
	static int index = 0;

	index = 1 - index;

	for (i = 0, client = svs.clients; num && i < MAX_CLIENTS; i++, client++)
	{
		if (!client->name[0] || client->spectator)
			continue;

		if (first || strcmp(lastteam[index], Info_ValueForKey(client->userinfo, "team")))
		{
			first = false;
			num--;
			lastteam[index] = Info_ValueForKey(client->userinfo, "team");
		}
	}

	if (num)
		return "";

	return lastteam[index];
}

char *Dem_PlayerName(int num)
{
	int i;
	client_t *client;

	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++)
	{
		if (!client->name[0] || client->spectator)
			continue;

		if (!--num)
			return client->name;
	}

	return "";
}

// -> scream
char *Dem_PlayerNameTeam(char *t)
{
	int	i;
	client_t *client;
	static char	n[1024];
	int	sep;

	n[0] = 0;

	sep = 0;

	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++)
	{
		if (!client->name[0] || client->spectator)
			continue;

		if (strcmp(t, Info_ValueForKey(client->userinfo, "team"))==0) 
		{
			if (sep >= 1)
				Q_strncatz (n, "_", sizeof(n));
//				snprintf (n, sizeof(n), "%s_", n);
			Q_strncatz (n, client->name, sizeof(n));
//			snprintf (n, sizeof(n),"%s%s", n, client->name);
			sep++;
		}
	}

	return n;
}

int	Dem_CountTeamPlayers (char *t)
{
	int	i, count;

	count = 0;
	for (i = 0; i < MAX_CLIENTS ; i++)
	{
		if (svs.clients[i].name[0] && !svs.clients[i].spectator)
			if (strcmp(Info_ValueForKey(svs.clients[i].userinfo, "team"), t)==0)
				count++;
	}

	return count;
}

// <-

void SV_MVDEasyRecord_f (void)
{
	int		c;
	dir_t	dir;
	char	name[1024];
	char	name2[MAX_OSPATH*7]; // scream
	//char	name2[MAX_OSPATH*2];
	int		i;
	FILE	*f;

	c = Cmd_Argc();
	if (c > 2)
	{
		Con_Printf ("easyrecord [demoname]\n");
		return;
	}

	if (sv.mvdrecording)
		SV_MVDStop_f();

	dir = Sys_listdir(va("%s/%s", com_gamedir,sv_demoDir.string), ".*", SORT_NO);
	if (sv_demoMaxDirSize.value && dir.size > sv_demoMaxDirSize.value*1024)
	{
		Con_Printf("insufficient directory space, increase sv_demoMaxDirSize\n");
		return;
	}

	// -> scream
/*	if (c == 2)
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
		
	else { 
		// guess game type and write demo name
		i = Dem_CountPlayers();
		if (teamplay.value && i > 2)
		{
			// Teamplay
			snprintf (name, sizeof(name), "team_%s_vs_%s_%s",
				Dem_Team(1),
				Dem_Team(2),
				sv.name);
		} else {
			if (i == 2) {
				// Duel
				snprintf (name, sizeof(name), "duel_%s_vs_%s_%s",
					Dem_PlayerName(1),
					Dem_PlayerName(2),
					sv.name);
			} else {
				// FFA
				snprintf (name, sizeof(name), "ffa_%s(%d)",
					sv.name,
					i);
			}
		}
	}*/

	
	if (c == 2)
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	else
	{
		i = Dem_CountPlayers();
		if (teamplay.value >= 1 && i > 2)
		{
			// Teamplay
			_snprintf (name, sizeof(name), "%don%d_", Dem_CountTeamPlayers(Dem_Team(1)), Dem_CountTeamPlayers(Dem_Team(2)));
			if (sv_demoExtraNames.value > 0)
			{
				Q_strncatz (name, va("[%s]_%s_vs_[%s]_%s_%s", 
									Dem_Team(1), Dem_PlayerNameTeam(Dem_Team(1)), 
									Dem_Team(2), Dem_PlayerNameTeam(Dem_Team(2)),
									sv.name), sizeof(name));
			} else
				Q_strncatz (name, va("%s_vs_%s_%s", Dem_Team(1), Dem_Team(2), sv.name), sizeof(name));
		} else {
			if (i == 2) {
				// Duel
				_snprintf (name, sizeof(name), "duel_%s_vs_%s_%s",
					Dem_PlayerName(1),
					Dem_PlayerName(2),
					sv.name);
			} else {
				// FFA
				_snprintf (name, sizeof(name), "ffa_%s(%d)", sv.name, i);
			}
		}
	}

	// <-

// Make sure the filename doesn't contain illegal characters
	Q_strncpyz(name, va("%s%s", sv_demoPrefix.string, SV_CleanName(name)),
			MAX_MVD_NAME - strlen(sv_demoSuffix.string) - 7);
	Q_strncatz(name, sv_demoSuffix.string, sizeof(name));
	Q_strncpyz(name, va("%s/%s/%s", com_gamedir, sv_demoDir.string, name), sizeof(name));
// find a filename that doesn't exist yet
	Q_strncpyz(name2, name, sizeof(name2));
	Sys_mkdir(va("%s/%s", com_gamedir, sv_demoDir.string));
//	COM_StripExtension(name2, name2);
	strcat (name2, ".mvd");
	if ((f = fopen (name2, "rb")) == 0)
		f = fopen(va("%s.gz", name2), "rb");
	
	if (f) {
		i = 1;
		do {
			fclose (f);
			_snprintf(name2, sizeof(name2), "%s_%02i", name, i);
//			COM_StripExtension(name2, name2);
			strcat (name2, ".mvd");
			if ((f = fopen (name2, "rb")) == 0)
				f = fopen(va("%s.gz", name2), "rb");
			i++;
		} while (f);
	}

	SV_MVD_Record (name2);
}

void SV_MVDList_f (void)
{
	dir_t	dir;
	file_t	*list;
	float	f;
	int		i,j,show;

	Con_Printf("content of %s/%s/*.mvd\n", com_gamedir,sv_demoDir.string);
	dir = Sys_listdir(va("%s/%s", com_gamedir,sv_demoDir.string), ".mvd", SORT_BY_DATE);
	list = dir.files;
	if (!list->name[0])
	{
		Con_Printf("no demos\n");
	}

	for (i = 1; list->name[0]; i++, list++)
	{
		for (j = 1; j < Cmd_Argc(); j++)
			if (strstr(list->name, Cmd_Argv(j)) == NULL)
				break;
		show = Cmd_Argc() == j;

		if (show) {
			if (sv.mvdrecording && !strcmp(list->name, demo.name))
				Con_Printf("*%d: %s %dk\n", i, list->name, demo.size/1024);
			else
				Con_Printf("%d: %s %dk\n", i, list->name, list->size/1024);
		}
	}

	if (sv.mvdrecording)
		dir.size += demo.size;

	Con_Printf("\ndirectory size: %.1fMB\n",(float)dir.size/(1024*1024));
	if (sv_demoMaxDirSize.value) {
		f = (sv_demoMaxDirSize.value*1024 - dir.size)/(1024*1024);
		if ( f < 0)
			f = 0;
		Con_Printf("space available: %.1fMB\n", f);
	}
}

char *SV_MVDNum(int num)
{
	file_t	*list;
	dir_t	dir;

	dir = Sys_listdir(va("%s/%s", com_gamedir, sv_demoDir.string), ".mvd", SORT_BY_DATE);
	list = dir.files;

	if (num <= 0)
		return NULL;

	num--;

	while (list->name[0] && num) {list++; num--;};

	if (list->name[0]) 
		return list->name;

	return NULL;
}

char *SV_MVDName2Txt(char *name)
{
	char s[MAX_OSPATH];

	if (!name)
		return NULL;

	Q_strncpyz(s, name, MAX_OSPATH);

	if (strstr(s, ".mvd.gz") != NULL)
		Q_strncpyz(s + strlen(s) - 6, "txt", MAX_OSPATH - strlen(s) + 6);
	else
		Q_strncpyz(s + strlen(s) - 3, "txt", MAX_OSPATH - strlen(s) + 3);

	return va("%s", s);
}

char *SV_MVDTxTNum(int num)
{
	return SV_MVDName2Txt(SV_MVDNum(num));
}

void SV_MVDRemove_f (void)
{
	char name[MAX_MVD_NAME], *ptr;
	char path[MAX_OSPATH];
	int i;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("rmdemo <demoname> - removes the demo\nrmdemo *<token>   - removes demo with <token> in the name\nrmdemo *          - removes all demos\n");
		return;
	}

	ptr = Cmd_Argv(1);
	if (*ptr == '*')
	{
		dir_t dir;
		file_t *list;

		// remove all demos with specified token
		ptr++;

		dir = Sys_listdir(va("%s/%s", com_gamedir, sv_demoDir.string), ".mvd", SORT_BY_DATE);
		list = dir.files;
		for (i = 0;list->name[0]; list++)
		{
			if (strstr(list->name, ptr))
			{
				if (sv.mvdrecording && !strcmp(list->name, demo.name))
					SV_MVDStop_f();

				// stop recording first;
				_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, list->name);
				if (!Sys_remove(path)) {
					Con_Printf("removing %s...\n", list->name);
					i++;
				}
		
				Sys_remove(SV_MVDName2Txt(path));
			}
		}

		if (i) {
			Con_Printf("%d demos removed\n", i);
		} else {
			Con_Printf("no matching found\n");
		}

		return;
	}

	Q_strncpyz(name, Cmd_Argv(1), MAX_MVD_NAME);
	COM_DefaultExtension(name, ".mvd");

	_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);

	if (sv.mvdrecording && !strcmp(name, demo.name))
		SV_MVDStop_f();

	if (!Sys_remove(path))
	{
		Con_Printf("demo %s successfully removed\n", name);
	}
	else 
		Con_Printf("unable to remove demo %s\n", name);

	Sys_remove(SV_MVDName2Txt(path));
}

void SV_MVDRemoveNum_f (void)
{
	int		num;
	char	*val, *name;
	char path[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf("rmdemonum <#>\n");
		return;
	}

	val = Cmd_Argv(1);
	if ((num = atoi(val)) == 0 && val[0] != '0')
	{
		Con_Printf("rmdemonum <#>\n");
		return;
	}

	name = SV_MVDNum(num);

	if (name != NULL) {
		if (sv.mvdrecording && !strcmp(name, demo.name))
			SV_MVDStop_f();

		_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);
		if (!Sys_remove(path))
		{
			Con_Printf("demo %s succesfully removed\n", name);
		}
		else 
			Con_Printf("unable to remove demo %s\n", name);

		Sys_remove(SV_MVDName2Txt(path));
	} else
		Con_Printf("invalid demo num\n");
}

void SV_MVDInfoAdd_f (void)
{
	char *name, *args, path[MAX_OSPATH];
	FILE *f;

	if (Cmd_Argc() < 3) {
		Con_Printf("usage:MVDInfoAdd <demonum> <info string>\n<demonum> = * for currently recorded demo\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*"))
	{
		if (!sv.mvdrecording) {
			Con_Printf("Not recording demo!\n");
			return;
		}

		_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, demo.path, SV_MVDName2Txt(demo.name));
	} else {
		name = SV_MVDTxTNum(atoi(Cmd_Argv(1)));

		if (!name) {
			Con_Printf("invalid demo num\n");
			return;
		}

		_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);
	}

	if ((f = fopen(path, "a+t")) == NULL)
	{
		Con_Printf("failed to open the file\n");
		return;
	}

	// skip demonum
	args = Cmd_Args();
	while (*args > 32) args++;
	while (*args && *args <= 32) args++;

	fwrite(args, strlen(args), 1, f);
	fwrite("\n", 1, 1, f);
	fflush(f);
	fclose(f);
}

void SV_MVDInfoRemove_f (void)
{
	char *name, path[MAX_OSPATH];

	if (Cmd_Argc() < 2) {
		Con_Printf("usage:demoInfoRemove <demonum>\n<demonum> = * for currently recorded demo\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*"))
	{
		if (!sv.mvdrecording)
		{
			Con_Printf("Not recording demo!\n");
			return;
		}

		_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, demo.path, SV_MVDName2Txt(demo.name));
	}
	else
	{
		name = SV_MVDTxTNum(atoi(Cmd_Argv(1)));

		if (!name) {
			Con_Printf("invalid demo num\n");
			return;
		}

		_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);
	}

	if (Sys_remove(path))
		Con_Printf("failed to remove the file\n");
	else Con_Printf("file removed\n");
}

void SV_MVDInfo_f (void)
{
	char buf[64];
	FILE *f = NULL;
	char *name, path[MAX_OSPATH];

	if (Cmd_Argc() < 2) {
		Con_Printf("usage:demoinfo <demonum>\n<demonum> = * for currently recorded demo\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*"))
	{
		if (!sv.mvdrecording) {
			Con_Printf("Not recording demo!\n");
			return;
		}

		_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, demo.path, SV_MVDName2Txt(demo.name));
	} else {
		name = SV_MVDTxTNum(atoi(Cmd_Argv(1)));

		if (!name) {
			Con_Printf("invalid demo num\n");
			return;
		}

		_snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);
	}

	if ((f = fopen(path, "rt")) == NULL)
	{
		Con_Printf("(empty)\n");
		return;
	}

	while (!feof(f))
	{
		buf[fread (buf, 1, sizeof(buf)-1, f)] = 0;
		Con_Printf("%s", buf);
	}

	fclose(f);
}








void SV_MVDPlayNum_f(void)
{
	char *name;
	int		num;
	char	*val;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("mvdplaynum <#>\n");
		return;
	}

	val = Cmd_Argv(1);
	if ((num = atoi(val)) == 0 && val[0] != '0')
	{
		Con_Printf("mvdplaynum <#>\n");
		return;
	}

	name = SV_MVDNum(atoi(val));

	if (name)
		Cbuf_AddText(va("mvdplay %s\n", name), Cmd_ExecLevel);
	else
		Con_Printf("invalid demo num\n");
}



void SV_MVDInit(void)
{
	MVD_Init();

	Cmd_AddCommand ("mvdrecord", SV_MVD_Record_f);
	Cmd_AddCommand ("easyrecord", SV_MVDEasyRecord_f);
	Cmd_AddCommand ("mvdstop", SV_MVDStop_f);
	Cmd_AddCommand ("mvdcancel", SV_MVD_Cancel_f);
	Cmd_AddCommand ("mvdplaynum", SV_MVDPlayNum_f);
	Cmd_AddCommand ("mvdlist", SV_MVDList_f);
	Cmd_AddCommand ("demolist", SV_MVDList_f);
	Cmd_AddCommand ("rmdemo", SV_MVDRemove_f);
	Cmd_AddCommand ("rmdemonum", SV_MVDRemoveNum_f);
}

