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
#ifndef CLIENTONLY

#include "winquake.h"

#include "netinc.h"


void SV_MVDStop_f (void);

#define demo_size_padding 0x1000

mvddest_t *singledest;

mvddest_t *SV_InitStream(int socket);
static qboolean SV_MVD_Record (mvddest_t *dest);
extern cvar_t qtv_password;

void DestClose(mvddest_t *d, qboolean destroyfiles)
{
	char path[MAX_OSPATH];

	if (d->cache)
		BZ_Free(d->cache);
	if (d->file)
		fclose(d->file);
	if (d->socket)
		UDP_CloseSocket(d->socket);

	if (destroyfiles)
	{
		snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, d->path, d->name);
		Sys_remove(path);

		Q_strncpyz(path + strlen(path) - 3, "txt", MAX_OSPATH - strlen(path) + 3);
		Sys_remove(path);
	}

	Z_Free(d);
}

void DestFlush(qboolean compleate)
{
	int len;
	mvddest_t *d, *t;

	if (!demo.dest)
		return;
	while (demo.dest->error)
	{
		d = demo.dest;
		demo.dest = d->nextdest;

		DestClose(d, false);

		if (!demo.dest)
		{
			SV_MVDStop(3, false);
			return;
		}
	}
	for (d = demo.dest; d; d = d->nextdest)
	{
		switch(d->desttype)
		{
		case DEST_FILE:
			fflush (d->file);
			break;
		case DEST_BUFFEREDFILE:
			if (d->cacheused+demo_size_padding > d->maxcachesize || compleate)
			{
				len = fwrite(d->cache, 1, d->cacheused, d->file);
				if (len < d->cacheused)
					d->error = true;
				fflush(d->file);

				d->cacheused = 0;
			}
			break;

		case DEST_STREAM:
			if (d->cacheused && !d->error)
			{
				len = send(d->socket, d->cache, d->cacheused, 0);
				if (len == 0) //client died
					d->error = true;
				else if (len > 0)	//we put some data through
				{	//move up the buffer
					d->cacheused -= len;
					memmove(d->cache, d->cache+len, d->cacheused);
				}
				else
				{	//error of some kind. would block or something
					int e;
					e = qerrno;
					if (e != EWOULDBLOCK)
						d->error = true;
				}
			}
			break;

		case DEST_NONE:
			Sys_Error("DestFlush encoundered bad dest.");
		}

		if (sv_demoMaxSize.value && d->totalsize > sv_demoMaxSize.value*1024)
			d->error = 2;	//abort, but don't kill it.

		while (d->nextdest && d->nextdest->error)
		{
			t = d->nextdest;
			d->nextdest = t->nextdest;

			DestClose(t, false);
		}
	}
}

void SV_MVD_RunPendingConnections(void)
{
	unsigned short ushort_result;
	char *e;
	int len;
	mvdpendingdest_t *p;
	mvdpendingdest_t *np;

	if (!demo.pendingdest)
		return;

	while (demo.pendingdest && demo.pendingdest->error)
	{
		np = demo.pendingdest->nextdest;

		if (demo.pendingdest->socket != INVALID_SOCKET)
			closesocket(demo.pendingdest->socket);
		Z_Free(demo.pendingdest);
		demo.pendingdest = np;
	}

	for (p = demo.pendingdest; p && p->nextdest; p = p->nextdest)
	{
		if (p->nextdest->error)
		{
			np = p->nextdest->nextdest;
			if (p->nextdest->socket != INVALID_SOCKET)
				closesocket(p->nextdest->socket);
			Z_Free(p->nextdest);
			p->nextdest = np;
		}
	}

	for (p = demo.pendingdest; p; p = p->nextdest)
	{
		if (p->outsize && !p->error)
		{
			len = send(p->socket, p->outbuffer, p->outsize, 0);
			if (len == 0) //client died
				p->error = true;
			else if (len > 0)	//we put some data through
			{	//move up the buffer
				p->outsize -= len;
				memmove(p->outbuffer, p->outbuffer+len, p->outsize );
			}
			else
			{	//error of some kind. would block or something
				int e;
				e = qerrno;
				if (e != EWOULDBLOCK)
					p->error = true;
			}
		}
		if (!p->error)
		{
			len = recv(p->socket, p->inbuffer + p->insize, sizeof(p->inbuffer) - p->insize - 1, 0);
			if (len > 0)
			{//fixme: cope with extra \rs
				char *end;
				p->insize += len;
				p->inbuffer[p->insize] = 0;

				for (end = p->inbuffer; ; end++)
				{
					if (*end == '\0')
					{
						end = NULL;
						break;	//not enough data
					}

					if (end[0] == '\n')
					{
						if (end[1] == '\n')
						{
							end[1] = '\0';
							break;
						}
					}
				}
				if (end)
				{	//we found the end of the header
					qboolean server = false;
					char *start, *lineend;
					int versiontouse = 0;
					int raw = 0;
					char password[256] = "";
					enum {
						QTVAM_NONE,
						QTVAM_PLAIN,
						QTVAM_CCITT,
						QTVAM_MD4,
						QTVAM_MD5,
					} authmethod = QTVAM_NONE;

					start = p->inbuffer;

					lineend = strchr(start, '\n');
					if (!lineend)
					{
//						char *e;
//						e =	"This is a QTV server.";
//						send(p->socket, e, strlen(e), 0);

						p->error = true;
						continue;
					}
					*lineend = '\0';
					COM_ParseToken(start, NULL);
					start = lineend+1;
					if (strcmp(com_token, "QTV"))
					{	//it's an error if it's not qtv.
						if (!strcmp(com_token, "QTVSV"))
							server = true;
						else
						{
							p->error = true;
							lineend = strchr(start, '\n');
							continue;
						}
					}

					if (server != p->isreverse)
					{	//just a small check
						p->error = true;
						return;
					}

					for(;;)
					{
						lineend = strchr(start, '\n');
						if (!lineend)
							break;
						*lineend = '\0';
						start = COM_ParseToken(start, NULL);
						if (*start == ':')
						{
//VERSION: a list of the different qtv protocols supported. Multiple versions can be specified. The first is assumed to be the prefered version.
//RAW: if non-zero, send only a raw mvd with no additional markup anywhere (for telnet use). Doesn't work with challenge-based auth, so will only be accepted when proxy passwords are not required.
//AUTH: specifies an auth method, the exact specs varies based on the method
//		PLAIN: the password is sent as a PASSWORD line
//		MD4: the server responds with an "AUTH: MD4\n" line as well as a "CHALLENGE: somerandomchallengestring\n" line, the client sends a new 'initial' request with CHALLENGE: MD4\nRESPONSE: hexbasedmd4checksumhere\n"
//		MD5: same as md4
//		CCITT: same as md4, but using the CRC stuff common to all quake engines.
//		if the supported/allowed auth methods don't match, the connection is silently dropped.
//SOURCE: which stream to play from, DEFAULT is special. Without qualifiers, it's assumed to be a tcp address.
//COMPRESSION: Suggests a compression method (multiple are allowed). You'll get a COMPRESSION response, and compression will begin with the binary data.

							start = start+1;
							Con_Printf("qtv, got (%s) (%s)\n", com_token, start);
							if (!strcmp(com_token, "VERSION"))
							{
								start = COM_ParseToken(start, NULL);
								if (atoi(com_token) == 1)
									versiontouse = 1;
							}
							else if (!strcmp(com_token, "RAW"))
							{
								start = COM_ParseToken(start, NULL);
								raw = atoi(com_token);
							}
							else if (!strcmp(com_token, "PASSWORD"))
							{
								start = COM_ParseToken(start, NULL);
								Q_strncpyz(password, com_token, sizeof(password));
							}
							else if (!strcmp(com_token, "AUTH"))
							{
								int thisauth;
								start = COM_ParseToken(start, NULL);
								if (!strcmp(com_token, "NONE"))
									thisauth = QTVAM_PLAIN;
								else if (!strcmp(com_token, "PLAIN"))
									thisauth = QTVAM_PLAIN;
								else if (!strcmp(com_token, "CCIT"))
									thisauth = QTVAM_CCITT;
								else if (!strcmp(com_token, "MD4"))
									thisauth = QTVAM_MD4;
//								else if (!strcmp(com_token, "MD5"))
//									thisauth = QTVAM_MD5;
								else
								{
									thisauth = QTVAM_NONE;
									Con_DPrintf("qtv: received unrecognised auth method (%s)\n", com_token);
								}

								if (authmethod < thisauth)
									authmethod = thisauth;
							}
							else if (!strcmp(com_token, "SOURCE"))
							{
								//servers don't support source, and ignore it.
								//source is only useful for qtv proxy servers.
							}
							else if (!strcmp(com_token, "COMPRESSION"))
							{
								//compression not supported yet
							}
							else
							{
								//not recognised.
							}
						}
						start = lineend+1;
					}

					len = (end - p->inbuffer)+2;
					p->insize -= len;
					memmove(p->inbuffer, p->inbuffer + len, p->insize);
					p->inbuffer[p->insize] = 0;

					e = NULL;
					if (p->hasauthed)
					{
					}
					else if (p->isreverse)
						p->hasauthed = true;	//reverse connections do not need to auth.
					else if (!*qtv_password.string)
						p->hasauthed = true;	//no password, no need to auth.
					else if (*password)
					{
						switch (authmethod)
						{
						case QTVAM_NONE:
							e = ("QTVSV 1\n"
								 "PERROR: You need to provide a common auth method.\n\n");
							break;
						case QTVAM_PLAIN:
							p->hasauthed = !strcmp(qtv_password.string, password);
							break;
						case QTVAM_CCITT:
							QCRC_Init(&ushort_result);
							QCRC_AddBlock(&ushort_result, p->challenge, strlen(p->challenge));
							QCRC_AddBlock(&ushort_result, qtv_password.string, strlen(qtv_password.string));
							p->hasauthed = (ushort_result == strtoul(password, NULL, 0));
							break;
						case QTVAM_MD4:
							{
								char hash[512];
								int md4sum[4];
								
								snprintf(hash, sizeof(hash), "%s%s", p->challenge, qtv_password.string);
								Com_BlockFullChecksum (hash, strlen(hash), (unsigned char*)md4sum);
								sprintf(hash, "%X%X%X%X", md4sum[0], md4sum[1], md4sum[2], md4sum[3]);
								p->hasauthed = !strcmp(password, hash);
							}
							break;
						case QTVAM_MD5:
						default:
							e = ("QTVSV 1\n"
								 "PERROR: FTEQWSV bug detected.\n\n");
							break;
						}
						if (!p->hasauthed && !e)
						{
							if (raw)
								e = "";
							else
								e =	("QTVSV 1\n"
									 "PERROR: Bad password.\n\n");
						}
					}
					else
					{
						//no password, and not automagically authed
						switch (authmethod)
						{
						case QTVAM_NONE:
							if (raw)
								e = "";
							else
								e = ("QTVSV 1\n"
									 "PERROR: You need to provide a common auth method.\n\n");
							break;
						case QTVAM_PLAIN:
							p->hasauthed = !strcmp(qtv_password.string, password);
							break;

							if (0)
							{
						case QTVAM_CCITT:
									e =	("QTVSV 1\n"
										"AUTH: CCITT\n"
										"CHALLENGE: ");
							}
							else if (0)
							{
						case QTVAM_MD4:
									e =	("QTVSV 1\n"
										"AUTH: MD4\n"
										"CHALLENGE: ");
							}
							else
							{
						case QTVAM_MD5:
									e =	("QTVSV 1\n"
										"AUTH: MD5\n"
										"CHALLENGE: ");
							}

							send(p->socket, e, strlen(e), 0);
							send(p->socket, p->challenge, strlen(p->challenge), 0);
							e = "\n\n";
							send(p->socket, e, strlen(e), 0);
							continue;

						default:
							e = ("QTVSV 1\n"
								 "PERROR: FTEQWSV bug detected.\n\n");
							break;
						}
					}

					if (e)
					{
					}
					else if (!versiontouse)
					{
						e =	("QTVSV 1\n"
							 "PERROR: Incompatible version (valid version is v1)\n\n");
					}
					else if (raw)
					{
						if (p->hasauthed == false)
						{
							e =	"";
						}
						else
						{
							SV_MVD_Record(SV_InitStream(p->socket));
							p->socket = INVALID_SOCKET;	//so it's not cleared wrongly.
						}
						p->error = true;
					}
					else
					{
						if (p->hasauthed == true)
						{
							mvddest_t *dst;
							e =	("QTVSV 1\n"
								 "BEGIN\n"
								 "\n");
							send(p->socket, e, strlen(e), 0);
							e = NULL;
							dst = SV_InitStream(p->socket);
							dst->droponmapchange = p->isreverse;
							SV_MVD_Record(dst);
							p->socket = INVALID_SOCKET;	//so it's not cleared wrongly.
						}
						else
						{
							e =	("QTVSV 1\n"
								"PERROR: You need to provide a password.\n\n");
						}
						p->error = true;
					}

					if (e)
					{
						send(p->socket, e, strlen(e), 0);
						p->error = true;
					}
				}
			}
			else if (len == 0)
				p->error = true;
			else
			{	//error of some kind. would block or something
				int e;
				e = qerrno;
				if (e != EWOULDBLOCK)
					p->error = true;
			}
		}
	}
}

int DestCloseAllFlush(qboolean destroyfiles, qboolean mvdonly)
{
	int numclosed = 0;
	mvddest_t *d, **prev, *next;
	DestFlush(true);	//make sure it's all written.

	prev = &demo.dest;
	d = demo.dest;
	while(d)
	{
		next = d->nextdest;
		if (!mvdonly || d->droponmapchange)
		{
			*prev = d->nextdest;
			DestClose(d, destroyfiles);
			numclosed++;
		}
		else
			prev = &d->nextdest;

		d = next;
	}

	return numclosed;
}


int DemoWriteDest(void *data, int len, mvddest_t *d)
{
	if (d->error)
		return 0;
	d->totalsize += len;
	switch(d->desttype)
	{
	case DEST_FILE:
		fwrite(data, len, 1, d->file);
		break;
	case DEST_BUFFEREDFILE:	//these write to a cache, which is flushed later
	case DEST_STREAM:
		if (d->cacheused+len > d->maxcachesize)
		{
			d->error = true;
			return 0;
		}
		memcpy(d->cache+d->cacheused, data, len);
		d->cacheused += len;
		break;
	case DEST_NONE:
		Sys_Error("DemoWriteDest encoundered bad dest.");
	}
	return len;
}

int DemoWrite(void *data, int len)	//broadcast to all proxies/mvds
{
	mvddest_t *d;
	for (d = demo.dest; d; d = d->nextdest)
	{
		if (singledest && singledest != d)
			continue;
		DemoWriteDest(data, len, d);
	}
	return len;
}

void DemoWriteQTVTimePad(int msecs)	//broadcast to all proxies
{
	mvddest_t *d;
	unsigned char buffer[6];
	while (msecs > 0)
	{
		//duration
		if (msecs > 255)
			buffer[0] = 255;
		else
			buffer[0] = msecs;
		msecs -= buffer[0];
		//message type
		buffer[1] = dem_read;
		//length
		buffer[2] = 0;
		buffer[3] = 0;
		buffer[4] = 0;
		buffer[5] = 0;

		for (d = demo.dest; d; d = d->nextdest)
		{
			if (d->desttype == DEST_STREAM)
			{
				DemoWriteDest(buffer, sizeof(buffer), d);
			}
		}
	}
}


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

	int		maxfiles;
} dir_t;

#define SORT_NO 0
#define SORT_BY_DATE 1

#ifdef _WIN32
dir_t *Sys_listdir (char *path, char *ext, qboolean usesorting)
{
	unsigned int maxfiles = MAX_DIRFILES;
	dir_t *dir = malloc(sizeof(*dir) + sizeof(*dir->files)*maxfiles);
	HANDLE	h;
	WIN32_FIND_DATA fd;
	int		i, pos, size;
	char	name[MAX_MVD_NAME], *s;

	memset(dir, 0, sizeof(*dir));

	dir->files = (file_t*)(dir+1);
	dir->maxfiles = maxfiles;

	h = FindFirstFile (va("%s/*.*", path), &fd);
	if (h == INVALID_HANDLE_VALUE)
	{
		return dir;
	}

	do
	{
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			dir->numdirs++;
			continue;
		}

		size = fd.nFileSizeLow;
		Q_strncpyz (name, fd.cFileName, MAX_MVD_NAME);
		dir->size += size;

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

		i = dir->numfiles;
		pos = i;
		dir->numfiles++;
		for (i=dir->numfiles-1 ; i>pos ; i--)
			dir->files[i] = dir->files[i-1];

		strcpy (dir->files[i].name, name);
		dir->files[i].size = size;
		if (dir->numfiles == dir->maxfiles)
			break;
	} while ( FindNextFile(h, &fd) );
	FindClose (h);

	return dir;
}

void Sys_freedir(dir_t *dir)
{
	free(dir);
}

#else
#include <dirent.h>
dir_t *Sys_listdir (char *path, char *ext, qboolean usesorting)
{
	unsigned int maxfiles = MAX_DIRFILES;
	dir_t *d = malloc(sizeof(*d) + sizeof(*d->files)*maxfiles);

	int		i, extsize;
	DIR		*dir;
    struct dirent *oneentry;
	char	pathname[MAX_OSPATH];
	qboolean all;

	memset(d, 0, sizeof(*d));
	d->files = (file_t*)(d+1);
	d->maxfiles = maxfiles;
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
			d->numdirs++;
			continue;
		}
#endif

		sprintf(pathname, "%s/%s", path, oneentry->d_name);
		d->files[d->numfiles].size = COM_FileSize(pathname);
		d->size += d->files[d->numfiles].size;

		i = strlen(oneentry->d_name);
		if (!all && (i < extsize || (Q_strcasecmp(oneentry->d_name+i-extsize, ext))))
			continue;

		Q_strncpyz(d->files[d->numfiles].name, oneentry->d_name, MAX_MVD_NAME);
		d->numfiles++;

		if (d->numfiles == d->maxfiles)
			break;
	}

	closedir(dir);

	return d;
}
void Sys_freedir(dir_t *dir)
{
	free(dir);
}
#endif









#define MIN_MVD_MEMORY 0x100000
#define MAXSIZE (demobuffer->end < demobuffer->last ? \
				demobuffer->start - demobuffer->end : \
				demobuffer->maxsize - demobuffer->end)

static void SV_DemoDir_Callback(struct cvar_s *var, char *oldvalue);

cvar_t	sv_demoUseCache = SCVAR("sv_demoUseCache", "");
cvar_t	sv_demoCacheSize = SCVAR("sv_demoCacheSize", "");
cvar_t	sv_demoMaxDirSize = SCVAR("sv_demoMaxDirSize", "102400");	//so ktpro autorecords.
cvar_t	sv_demoDir = SCVARC("sv_demoDir", "demos", SV_DemoDir_Callback);
cvar_t	sv_demofps = SCVAR("sv_demofps", "");
cvar_t	sv_demoPings = SCVAR("sv_demoPings", "");
cvar_t	sv_demoNoVis = SCVAR("sv_demoNoVis", "");
cvar_t	sv_demoMaxSize = SCVAR("sv_demoMaxSize", "");
cvar_t	sv_demoExtraNames = SCVAR("sv_demoExtraNames", "");

cvar_t qtv_password = SCVAR("qtv_password", "");
cvar_t qtv_streamport = FCVAR("qtv_streamport", "mvd_streamport", "0", 0);
cvar_t qtv_maxstreams = FCVAR("qtv_maxstreams", "mvd_maxstreams", "1", 0);

cvar_t			sv_demoPrefix = SCVAR("sv_demoPrefix", "");
cvar_t			sv_demoSuffix = SCVAR("sv_demoSuffix", "");
cvar_t			sv_demotxt = SCVAR("sv_demotxt", "1");

void SV_WriteMVDMessage (sizebuf_t *msg, int type, int to, float time);

demo_t			demo;
static dbuffer_t	*demobuffer;
static int	header = (char *)&((header_t*)0)->data - (char *)NULL;

entity_state_t demo_entities[UPDATE_MASK+1][MAX_MVDPACKET_ENTITIES];
client_frame_t demo_frames[UPDATE_MASK+1];

// only one .. is allowed (so we can get to the same dir as the quake exe)
static void SV_DemoDir_Callback(struct cvar_s *var, char *oldvalue)
{
	char *value;

	value = var->string;
	if (!value[0] || value[0] == '/' || (value[0] == '\\' && value[1] == '\\'))
	{
		Cvar_ForceSet(&sv_demoDir, "demos");
		return;
	}
	if (value[0] == '.' && value[1] == '.')
		value += 2;
	if (strstr(value,".."))
	{
		Cvar_ForceSet(&sv_demoDir, "demos");
		return;
	}
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

	p = (header_t *)demo.dbuf->data;
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
			if (size)
			{
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
		p = (header_t *)(p->data + size);
	}

	if (demobuffer->start == demobuffer->last)
	{
		if (demobuffer->start == demobuffer->end)
		{
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

	p = (header_t *)demo.dbuf->data;

	while (pos < demo.dbuf->bufsize)
	{
		pos += header + p->size;

		if (type == p->type && to == p->to && !p->full)
		{
			demo.dbuf->cursize = pos;
			demo.dbuf->h = p;
			return;
		}

		p = (header_t *)(p->data + p->size);
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

qboolean MVDWrite_Begin(qbyte type, int to, int size)
{
	qbyte *p;
	qboolean move = false;

	// will it fit?
	while (demo.dbuf->bufsize + size + header > demo.dbuf->maxsize)
	{
		// if we reached the end of buffer move msgbuf to the begining
		if (!move && demobuffer->end > demobuffer->start)
			move = true;

		if (!SV_MVDWritePackets(1))
			return false;

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

	return true;
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
	DemoWrite(&c, sizeof(c));

	if (demo.lasttype != type || demo.lastto != to)
	{
		demo.lasttype = type;
		demo.lastto = to;
		switch (demo.lasttype)
		{
		case dem_all:
			c = dem_all;
			DemoWrite (&c, sizeof(c));
			break;
		case dem_multiple:
			c = dem_multiple;
			DemoWrite (&c, sizeof(c));

			i = LittleLong(demo.lastto);
			DemoWrite (&i, sizeof(i));
			break;
		case dem_single:
		case dem_stats:
			c = demo.lasttype + (demo.lastto << 3);
			DemoWrite (&c, sizeof(c));
			break;
		default:
			SV_MVDStop_f ();
			Con_Printf("bad demo message type:%d", type);
			return;
		}
	} else {
		c = dem_read;
		DemoWrite (&c, sizeof(c));
	}


	len = LittleLong (msg->cursize);
	DemoWrite (&len, 4);
	DemoWrite (msg->data, msg->cursize);

	DestFlush(false);
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

qboolean SV_MVDWritePackets (int num)
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
		return false;

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

		/* The above functions can set this variable to false, but that's a really bad thing. Let's try to fix it. */
		if (!sv.mvdrecording)
			return false;
	}

	if (demo.lastwritten > demo.parsecount)
		demo.lastwritten = demo.parsecount;

	demo.dbuf = &demo.frames[demo.parsecount&DEMO_FRAMES_MASK].buf;
	demo.dbuf->maxsize = MAXSIZE + demo.dbuf->bufsize;

	return true;
}

extern char readable[256];
#define chartbl readable

void MVD_Init (void)
{
#define MVDVARGROUP "Server MVD cvars"

	Cvar_Register (&sv_demofps,		MVDVARGROUP);
	Cvar_Register (&sv_demoPings,		MVDVARGROUP);
	Cvar_Register (&sv_demoNoVis,		MVDVARGROUP);
	Cvar_Register (&sv_demoUseCache,	MVDVARGROUP);
	Cvar_Register (&sv_demoCacheSize,	MVDVARGROUP);
	Cvar_Register (&sv_demoMaxSize,		MVDVARGROUP);
	Cvar_Register (&sv_demoMaxDirSize,	MVDVARGROUP);
	Cvar_Register (&sv_demoDir,		MVDVARGROUP);
	Cvar_Register (&sv_demoPrefix,		MVDVARGROUP);
	Cvar_Register (&sv_demoSuffix,		MVDVARGROUP);
	Cvar_Register (&sv_demotxt,		MVDVARGROUP);
	Cvar_Register (&sv_demoExtraNames,	MVDVARGROUP);
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
		snprintf(buf, sizeof(buf), "team1 %s\nteam2 %s\n", clients[0]->name, clients[1]->name);
	}
	else if (!teamplay.value) // ffa
	{
		snprintf(buf, sizeof(buf), "players:\n");
		for (i = 0; i < numcl; i++)
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  %s\n", clients[i]->name);
	}
	else
	{ // teamplay
		for (j = 0; j < numt; j++)
		{
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "team %s:\n", teams[j]);
			for (i = 0; i < numcl; i++)
				if (!strcmp(Info_ValueForKey(clients[i]->userinfo, "team"), teams[j]))
					snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  %s\n", clients[i]->name);
		}
	}

	if (!numcl)
		return "\n";
//	for (p = buf; *p; p++) *p = chartbl2[(qbyte)*p];
	return va("%s",buf);
}

/*
====================
SV_InitRecord
====================
*/

mvddest_t *SV_InitRecordFile (char *name)
{
	char *s;
	mvddest_t *dst;
	FILE *file;

	char path[MAX_OSPATH];

	file = fopen (name, "wb");
	if (!file)
	{
		Con_Printf ("ERROR: couldn't open \"%s\"\n", name);
		return NULL;
	}

	dst = Z_Malloc(sizeof(mvddest_t));

	if (!sv_demoUseCache.value)
	{
		dst->desttype = DEST_FILE;
		dst->file = file;
		dst->maxcachesize = 0;
	}
	else
	{
		dst->desttype = DEST_BUFFEREDFILE;
		dst->file = file;
		dst->maxcachesize = 0x81000;
		dst->cache = BZ_Malloc(dst->maxcachesize);
	}
	dst->droponmapchange = true;

	s = name + strlen(name);
	while (*s != '/') s--;
	Q_strncpyz(dst->name, s+1, sizeof(dst->name));
	Q_strncpyz(dst->path, sv_demoDir.string, sizeof(dst->path));

	if (!*demo.path)
		Q_strncpyz(demo.path, ".", MAX_OSPATH);

	SV_BroadcastPrintf (PRINT_CHAT, "Server starts recording (%s):\n%s\n", (dst->desttype == DEST_BUFFEREDFILE) ? "memory" : "disk", name);
	Cvar_ForceSet(Cvar_Get("serverdemo", "", CVAR_NOSET, ""), demo.name);

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

			snprintf(buf, sizeof(buf), "date %s\nmap %s\nteamplay %d\ndeathmatch %d\ntimelimit %d\n%s",date.str, sv.name, (int)teamplay.value, (int)deathmatch.value, (int)timelimit.value, SV_PrintTeams());
			fwrite(buf, strlen(buf),1,f);
			fflush(f);
			fclose(f);
		}
	}
	else
		Sys_remove(path);


	return dst;
}

mvddest_t *SV_InitStream(int socket)
{
	mvddest_t *dst;

	dst = Z_Malloc(sizeof(mvddest_t));

	dst->desttype = DEST_STREAM;
	dst->socket = socket;
	dst->maxcachesize = 0x8000;	//is this too small?
	dst->cache = BZ_Malloc(dst->maxcachesize);
	dst->droponmapchange = false;

	SV_BroadcastPrintf (PRINT_CHAT, "Smile, you're on QTV!\n");

	return dst;
}

mvdpendingdest_t *SV_MVD_InitPendingStream(int socket, char *ip)
{
	mvdpendingdest_t *dst;
	int i;
	dst = Z_Malloc(sizeof(mvdpendingdest_t));
	dst->socket = socket;

	Q_strncpyz(dst->challenge, ip, sizeof(dst->challenge));
	for (i = strlen(dst->challenge); i < sizeof(dst->challenge)-1; i++)
		dst->challenge[i] = rand()%(127-33) + 33;	//generate a random challenge

	dst->nextdest = demo.pendingdest;
	demo.pendingdest = dst;

	return dst;
}

/*
====================
SV_Stop

stop recording a demo
====================
*/
void SV_MVDStop (int reason, qboolean mvdonly)
{
	int numclosed;
	if (!sv.mvdrecording)
	{
		Con_Printf ("Not recording a demo.\n");
		return;
	}

	if (reason == 2 || reason == 3)
	{
		DestCloseAllFlush(true, mvdonly);
		// stop and remove

		if (!demo.dest)
			sv.mvdrecording = false;

		if (reason == 3)
			SV_BroadcastPrintf (PRINT_CHAT, "QTV disconnected\n");
		else
			SV_BroadcastPrintf (PRINT_CHAT, "Server recording canceled, demo removed\n");

		Cvar_ForceSet(Cvar_Get("serverdemo", "", CVAR_NOSET, ""), "");

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

	numclosed = DestCloseAllFlush(false, mvdonly);

	if (!demo.dest)
		sv.mvdrecording = false;
	if (numclosed)
	{
		if (!reason)
			SV_BroadcastPrintf (PRINT_CHAT, "Server recording completed\n");
		else
			SV_BroadcastPrintf (PRINT_CHAT, "Server recording stoped\nMax demo size exceeded\n");
	}

	Cvar_ForceSet(Cvar_Get("serverdemo", "", CVAR_NOSET, ""), "");
}

/*
====================
SV_Stop_f
====================
*/
void SV_MVDStop_f (void)
{
	SV_MVDStop(0, true);
}

/*
====================
SV_Cancel_f

Stops recording, and removes the demo
====================
*/
void SV_MVD_Cancel_f (void)
{
	SV_MVDStop(2, true);
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

	if (!msg->cursize)
		return;

	c = 0;
	DemoWrite (&c, sizeof(c));

	c = dem_read;
	DemoWrite (&c, sizeof(c));

	len = LittleLong (msg->cursize);
	DemoWrite (&len, 4);

	DemoWrite (msg->data, msg->cursize);

	DestFlush(false);
}

void SV_WriteSetMVDMessage (void)
{
	int		len;
	qbyte	c;

//Con_Printf("write: %ld bytes, %4.4f\n", msg->cursize, realtime);

	if (!sv.mvdrecording)
		return;

	c = 0;
	DemoWrite (&c, sizeof(c));

	c = dem_set;
	DemoWrite (&c, sizeof(c));


	len = LittleLong(0);
	DemoWrite (&len, 4);
	len = LittleLong(0);
	DemoWrite (&len, 4);

	DestFlush(false);
}

void SV_MVD_SendInitialGamestate(mvddest_t *dest);
static qboolean SV_MVD_Record (mvddest_t *dest)
{
/*	sizebuf_t	buf;
	char buf_data[MAX_QWMSGLEN];
	int n, i;
	char *s, info[MAX_INFO_STRING];

	client_t *player;
	char *gamedir;
	int seq = 1;
*/
	int i;

	if (!dest)
		return false;

	DestFlush(true);

	if (!sv.mvdrecording)
	{
		memset(&demo, 0, sizeof(demo));
		demo.recorder.frameunion.frames = demo_frames;
		demo.recorder.protocol = SCP_QUAKEWORLD;
		for (i = 0; i < UPDATE_BACKUP; i++)
		{
			demo.recorder.frameunion.frames[i].entities.max_entities = MAX_MVDPACKET_ENTITIES;
			demo.recorder.frameunion.frames[i].entities.entities = demo_entities[i];
		}

		MVDBuffer_Init(&demo.dbuffer, demo.buffer, sizeof(demo.buffer));
		MVDSetMsgBuf(NULL, &demo.frames[0].buf);

		demo.datagram.maxsize = sizeof(demo.datagram_data);
		demo.datagram.data = demo.datagram_data;
	}
//	else
//		SV_WriteRecordMVDMessage(&buf, dem_read);

	dest->nextdest = demo.dest;
	demo.dest = dest;

	SV_MVD_SendInitialGamestate(dest);
	return true;
}
void SV_MVD_SendInitialGamestate(mvddest_t *dest)
{
	sizebuf_t	buf;
	char buf_data[MAX_QWMSGLEN];
	int n, i;
	char *s, info[MAX_INFO_STRING];

	client_t *player;
	char *gamedir;
	int seq = 1;

	if (!demo.dest)
		return;

	sv.mvdrecording = true;


	demo.pingtime = demo.time = sv.time;


	singledest = dest;

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
	if (sizeofcoord == 4)	//sorry.
	{
		MSG_WriteLong (&buf, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (&buf, PEXT_FLOATCOORDS);
	}
	MSG_WriteLong (&buf, PROTOCOL_VERSION_QW);
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
	s = sv.strings.sound_precache[n+1];
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
		s = sv.strings.sound_precache[n+1];
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
	s = sv.strings.model_precache[n+1];
	while (s)
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
		s = sv.strings.model_precache[n+1];
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
		if (buf.cursize+sv.signon_buffer_size[n] > MAX_QWMSGLEN/2)
		{
			SV_WriteRecordMVDMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
		SZ_Write (&buf,
			sv.signon_buffers[n],
			sv.signon_buffer_size[n]);
	}

	if (buf.cursize > MAX_QWMSGLEN/2)
	{
		SV_WriteRecordMVDMessage (&buf, seq++);
		SZ_Clear (&buf);
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
	for (i=0 ; i<MAX_STANDARDLIGHTSTYLES ; i++)
	{
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char)i);
		MSG_WriteString (&buf, sv.strings.lightstyles[i]);
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, "skins\n");

	SV_WriteRecordMVDMessage (&buf, seq++);

	SV_WriteSetMVDMessage();

	singledest = NULL;
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
	dir_t	*dir;

	c = Cmd_Argc();
	if (c != 2)
	{
		Con_Printf ("mvdrecord <demoname>\n");
		return;
	}

	if (sv.state != ss_active){
		Con_Printf ("Not active yet.\n");
		return;
	}

	dir = Sys_listdir(va("%s/%s", com_gamedir, sv_demoDir.string), ".*", SORT_NO);
	if (sv_demoMaxDirSize.value && dir->size > sv_demoMaxDirSize.value*1024)
	{
		Con_Printf("insufficient directory space, increase sv_demoMaxDirSize\n");
		Sys_freedir(dir);
		return;
	}
	Sys_freedir(dir);
	dir = NULL;

	Q_strncpyz(newname, va("%s%s", sv_demoPrefix.string, SV_CleanName(Cmd_Argv(1))),
			sizeof(newname) - strlen(sv_demoSuffix.string) - 5);
	Q_strncatz(newname, sv_demoSuffix.string, MAX_MVD_NAME);

	snprintf (name, MAX_OSPATH+MAX_MVD_NAME, "%s/%s/%s", com_gamedir, sv_demoDir.string, newname);


	COM_StripExtension(name, name, sizeof(name));
	COM_DefaultExtension(name, ".mvd", sizeof(name));
	COM_CreatePath(name);

	//
	// open the demo file and start recording
	//
	SV_MVD_Record (SV_InitRecordFile(name));
}

void SV_MVD_QTVReverse_f (void)
{
	char *ip;
	if (sv.state != ss_active)
	{
		Con_Printf ("Server is not running\n");
		return;
	}
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("%s ip:port\n", Cmd_Argv(0));
		return;
	}

	ip = Cmd_Argv(1);



{
	char *data;
	int sock;

	struct sockaddr_in	local;
	struct sockaddr_qstorage	remote;
//	int fromlen;

	unsigned int nonblocking = true;


	if (!NET_StringToSockaddr(ip, &remote))
	{
		Con_Printf ("qtvreverse: failed to resolve address\n");
		return;
	}


	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = 0;

	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		Con_Printf ("qtvreverse: socket: %s\n", strerror(qerrno));
		return;
	}

	if( bind (sock, (void *)&local, sizeof(local)) == INVALID_SOCKET)
	{
		closesocket(sock);
		Con_Printf ("qtvreverse: bind: %s\n", strerror(qerrno));
		return;
	}

	if (connect(sock, (void*)&remote, sizeof(remote)) == INVALID_SOCKET)
	{
		closesocket(sock);
		Con_Printf ("qtvreverse: connect: %s\n", strerror(qerrno));
		return;
	}

	if (ioctlsocket (sock, FIONBIO, &nonblocking) == INVALID_SOCKET)
	{
		closesocket(sock);
		Con_Printf ("qtvreverse: ioctl FIONBIO: %s\n", strerror(qerrno));
		return;
	}

	data =	"QTV\n"
			"REVERSE\n"
			"\n";
	if (send(sock, data, strlen(data), 0) == INVALID_SOCKET)
	{
		closesocket(sock);
		Con_Printf ("qtvreverse: send: %s\n", strerror(qerrno));
		return;
	}


	SV_MVD_InitPendingStream(sock, ip)->isreverse = true;
}

	//SV_MVD_Record (dest);

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
	dir_t	*dir;
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

	if (sv.state < ss_active)
	{
		Con_Printf("Server isn't running or is still loading\n");
		return;
	}

	dir = Sys_listdir(va("%s/%s", com_gamedir,sv_demoDir.string), ".*", SORT_NO);
	if (sv_demoMaxDirSize.value && dir->size > sv_demoMaxDirSize.value*1024)
	{
		Con_Printf("insufficient directory space, increase sv_demoMaxDirSize\n");
		Sys_freedir(dir);
		return;
	}
	Sys_freedir(dir);

	if (c == 2)
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	else
	{
		i = Dem_CountPlayers();
		if (teamplay.value >= 1 && i > 2)
		{
			// Teamplay
			snprintf (name, sizeof(name), "%don%d_", Dem_CountTeamPlayers(Dem_Team(1)), Dem_CountTeamPlayers(Dem_Team(2)));
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
				snprintf (name, sizeof(name), "duel_%s_vs_%s_%s",
					Dem_PlayerName(1),
					Dem_PlayerName(2),
					sv.name);
			} else {
				// FFA
				snprintf (name, sizeof(name), "ffa_%s(%d)", sv.name, i);
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

	if (f)
	{
		i = 1;
		do {
			fclose (f);
			snprintf(name2, sizeof(name2), "%s_%02i", name, i);
//			COM_StripExtension(name2, name2);
			strcat (name2, ".mvd");
			if ((f = fopen (name2, "rb")) == 0)
				f = fopen(va("%s.gz", name2), "rb");
			i++;
		} while (f);
	}

	SV_MVD_Record (SV_InitRecordFile(name2));
}

int MVD_StreamStartListening(int port)
{
	int sock;

	struct sockaddr_in	address;
//	int fromlen;

	unsigned int nonblocking = true;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((u_short)port);



	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		Sys_Error ("MVD_StreamStartListening: socket:", strerror(qerrno));
	}

	if (ioctlsocket (sock, FIONBIO, &nonblocking) == INVALID_SOCKET)
	{
		Sys_Error ("FTP_TCP_OpenSocket: ioctl FIONBIO:", strerror(qerrno));
	}

	if( bind (sock, (void *)&address, sizeof(address)) == INVALID_SOCKET)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}

	listen(sock, 2);

	return sock;
}

void SV_MVDStream_Poll(void)
{
	static int listensocket=INVALID_SOCKET;
	static int listenport;
	int _true = true;

	int client;
	netadr_t na;
	struct sockaddr_qstorage addr;
	int addrlen;
	int count;
	qboolean wanted;
	mvddest_t *dest;
	char *ip;
	char adrbuf[MAX_ADR_SIZE];

	if (!sv.state || !qtv_streamport.value)
		wanted = false;
	else if (listenport && (int)qtv_streamport.value != listenport)	//easy way to switch... disable for a frame. :)
	{
		listenport = qtv_streamport.value;
		wanted = false;
	}
	else
	{
		listenport = qtv_streamport.value;
		wanted = true;
	}

	if (wanted && listensocket==INVALID_SOCKET)
	{
		listensocket = MVD_StreamStartListening(listenport);
		if (listensocket==INVALID_SOCKET && qtv_streamport.modified)
		{
			Con_Printf("Cannot open TCP port %i for QTV\n", listenport);
			qtv_streamport.modified = false;
		}

	}
	else if (!wanted && listensocket!=INVALID_SOCKET)
	{
		closesocket(listensocket);
		listensocket = INVALID_SOCKET;
		return;
	}
	if (listensocket==INVALID_SOCKET)
		return;

	addrlen = sizeof(addr);
	client = accept(listensocket, (struct sockaddr *)&addr, &addrlen);

	if (client == INVALID_SOCKET)
		return;

	ioctlsocket(client, FIONBIO, &_true);

	if (qtv_maxstreams.value > 0)
	{
		count = 0;
		for (dest = demo.dest; dest; dest = dest->nextdest)
		{
			if (dest->desttype == DEST_STREAM)
			{
				count++;
			}
		}

		if (count > qtv_maxstreams.value)
		{	//sorry
			char *goawaymessage = "QTVSV 1\nTERROR: This server enforces a limit on the number of proxies connected at any one time. Please try again later\n\n";

			send(client, goawaymessage, strlen(goawaymessage), 0);
			closesocket(client);
			return;
		}
	}

	SockadrToNetadr(&addr, &na);
	ip = NET_AdrToString(adrbuf, sizeof(adrbuf), na);
	Con_Printf("MVD streaming client attempting to connect from %s\n", ip);

	SV_MVD_InitPendingStream(client, ip);

//	SV_MVD_Record (SV_InitStream(client));
}

void SV_MVDList_f (void)
{
	mvddest_t *d;
	dir_t	*dir;
	file_t	*list;
	float	f;
	int		i,j,show;

	Con_Printf("content of %s/%s/*.mvd\n", com_gamedir,sv_demoDir.string);
	dir = Sys_listdir(va("%s/%s", com_gamedir,sv_demoDir.string), ".mvd", SORT_BY_DATE);
	list = dir->files;
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

		if (show)
		{
			for (d = demo.dest; d; d = d->nextdest)
			{
				if (!strcmp(list->name, d->name))
					Con_Printf("*%d: %s %dk\n", i, list->name, d->totalsize/1024);
			}
			if (!d)
				Con_Printf("%d: %s %dk\n", i, list->name, list->size/1024);
		}
	}

	for (d = demo.dest; d; d = d->nextdest)
		dir->size += d->totalsize;

	Con_Printf("\ndirectory size: %.1fMB\n",(float)dir->size/(1024*1024));
	if (sv_demoMaxDirSize.value)
	{
		f = (sv_demoMaxDirSize.value*1024 - dir->size)/(1024*1024);
		if ( f < 0)
			f = 0;
		Con_Printf("space available: %.1fMB\n", f);
	}

	Sys_freedir(dir);
}

void SV_UserCmdMVDList_f (void)
{
	mvddest_t *d;
	dir_t	*dir;
	file_t	*list;
	float	f;
	int		i,j,show;

	Con_Printf("available demos\n");
	dir = Sys_listdir(va("%s/%s", com_gamedir,sv_demoDir.string), ".mvd", SORT_BY_DATE);
	list = dir->files;
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

		if (show)
		{
			for (d = demo.dest; d; d = d->nextdest)
			{
				if (!strcmp(list->name, d->name))
					Con_Printf("*%d: %s %dk\n", i, list->name, d->totalsize/1024);
			}
			if (!d)
				Con_Printf("%d: %s %dk\n", i, list->name, list->size/1024);
		}
	}
	
	for (d = demo.dest; d; d = d->nextdest)
		dir->size += d->totalsize;

	Con_Printf("\ndirectory size: %.1fMB\n",(float)dir->size/(1024*1024));
	if (sv_demoMaxDirSize.value)
	{
		f = (sv_demoMaxDirSize.value*1024 - dir->size)/(1024*1024);
		if ( f < 0)
			f = 0;
		Con_Printf("space available: %.1fMB\n", f);
	}

	Sys_freedir(dir);
}

char *SV_MVDNum(char *buffer, int bufferlen, int num)
{
	file_t	*list;
	dir_t	*dir;

	dir = Sys_listdir(va("%s/%s", com_gamedir, sv_demoDir.string), ".mvd", SORT_BY_DATE);
	list = dir->files;

	if (num <= 0)
	{
		Sys_freedir(dir);
		return NULL;
	}

	num--;

	while (list->name[0] && num) {list++; num--;};

	if (list->name[0])
	{
		Q_strncpyz(buffer, list->name, bufferlen);
		return list->name;
	}
	else
		buffer = NULL;

	Sys_freedir(dir);
	return buffer;
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

char *SV_MVDTxTNum(char *buffer, int bufferlen, int num)
{
	return SV_MVDName2Txt(SV_MVDNum(buffer, bufferlen, num));
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
		dir_t *dir;
		file_t *list;

		// remove all demos with specified token
		ptr++;

		dir = Sys_listdir(va("%s/%s", com_gamedir, sv_demoDir.string), ".mvd", SORT_BY_DATE);
		list = dir->files;
		for (i = 0;list->name[0]; list++)
		{
			if (strstr(list->name, ptr))
			{
				if (sv.mvdrecording && !strcmp(list->name, demo.name))
					SV_MVDStop_f();

				// stop recording first;
				snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, list->name);
				if (!Sys_remove(path))
				{
					Con_Printf("removing %s...\n", list->name);
					i++;
				}

				Sys_remove(SV_MVDName2Txt(path));
			}
		}
		Sys_freedir(dir);

		if (i)
		{
			Con_Printf("%d demos removed\n", i);
		}
		else
		{
			Con_Printf("no matching found\n");
		}

		return;
	}

	Q_strncpyz(name, Cmd_Argv(1), MAX_MVD_NAME);
	COM_DefaultExtension(name, ".mvd", sizeof(name));

	snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);

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
	char namebuf[MAX_QPATH];
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

	name = SV_MVDNum(namebuf, sizeof(namebuf), num);

	if (name != NULL)
	{
		if (sv.mvdrecording && !strcmp(name, demo.name))
			SV_MVDStop_f();

		snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);
		if (!Sys_remove(path))
		{
			Con_Printf("demo %s succesfully removed\n", name);
		}
		else
			Con_Printf("unable to remove demo %s\n", name);

		Sys_remove(SV_MVDName2Txt(path));
	}
	else
		Con_Printf("invalid demo num\n");
}

void SV_MVDInfoAdd_f (void)
{
	char namebuf[MAX_QPATH];
	char *name, *args, path[MAX_OSPATH];
	FILE *f;

	if (Cmd_Argc() < 3) {
		Con_Printf("usage:MVDInfoAdd <demonum> <info string>\n<demonum> = * for currently recorded demo\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*"))
	{
		if (!sv.mvdrecording)
		{
			Con_Printf("Not recording demo!\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, demo.path, SV_MVDName2Txt(demo.name));
	}
	else
	{
		name = SV_MVDTxTNum(namebuf, sizeof(namebuf), atoi(Cmd_Argv(1)));

		if (!name)
		{
			Con_Printf("invalid demo num\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);
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
	char namebuf[MAX_QPATH];
	char *name, path[MAX_OSPATH];

	if (Cmd_Argc() < 2)
	{
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

		snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, demo.path, SV_MVDName2Txt(demo.name));
	}
	else
	{
		name = SV_MVDTxTNum(namebuf, sizeof(namebuf), atoi(Cmd_Argv(1)));

		if (!name)
		{
			Con_Printf("invalid demo num\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);
	}

	if (Sys_remove(path))
		Con_Printf("failed to remove the file\n");
	else Con_Printf("file removed\n");
}

void SV_MVDInfo_f (void)
{
	int len;
	char buf[64];
	FILE *f = NULL;
	char *name, path[MAX_OSPATH];

	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage:demoinfo <demonum>\n<demonum> = * for currently recorded demo\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*"))
	{
		if (!sv.mvdrecording)
		{
			Con_Printf("Not recording demo!\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, demo.path, SV_MVDName2Txt(demo.name));
	}
	else
	{
		name = SV_MVDTxTNum(buf, sizeof(buf), atoi(Cmd_Argv(1)));

		if (!name)
		{
			Con_Printf("invalid demo num\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", com_gamedir, sv_demoDir.string, name);
	}

	if ((f = fopen(path, "rt")) == NULL)
	{
		Con_Printf("(empty)\n");
		return;
	}

	for(;;)
	{
		len = fread (buf, 1, sizeof(buf)-1, f);
		if (len < 0)
			break;
		buf[len] = 0;
		Con_Printf("%s", buf);
	}

	fclose(f);
}








void SV_MVDPlayNum_f(void)
{
	char namebuf[MAX_QPATH];
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

	name = SV_MVDNum(namebuf, sizeof(namebuf), atoi(val));

	if (name)
		Cbuf_AddText(va("mvdplay %s\n", name), Cmd_ExecLevel);
	else
		Con_Printf("invalid demo num\n");
}



void SV_MVDInit(void)
{
	MVD_Init();

#ifdef SERVERONLY	//client command would conflict otherwise.
	Cmd_AddCommand ("record", SV_MVD_Record_f);
	Cmd_AddCommand ("stop", SV_MVDStop_f);
	Cmd_AddCommand ("cancel", SV_MVD_Cancel_f);
#endif
	Cmd_AddCommand ("qtvreverse", SV_MVD_QTVReverse_f);
	Cmd_AddCommand ("mvdrecord", SV_MVD_Record_f);
	Cmd_AddCommand ("easyrecord", SV_MVDEasyRecord_f);
	Cmd_AddCommand ("mvdstop", SV_MVDStop_f);
	Cmd_AddCommand ("mvdcancel", SV_MVD_Cancel_f);
	//Cmd_AddCommand ("mvdplaynum", SV_MVDPlayNum_f);
	Cmd_AddCommand ("mvdlist", SV_MVDList_f);
	Cmd_AddCommand ("demolist", SV_MVDList_f);
	Cmd_AddCommand ("rmdemo", SV_MVDRemove_f);
	Cmd_AddCommand ("rmdemonum", SV_MVDRemoveNum_f);

	Cvar_Register(&qtv_streamport, "MVD Streaming");
	Cvar_Register(&qtv_maxstreams, "MVD Streaming");
	Cvar_Register(&qtv_password, "MVD Streaming");
}

#endif
