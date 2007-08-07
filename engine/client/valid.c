#include "quakedef.h"
#include <ctype.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#endif

typedef struct f_query_s
{
    char *query;
    char *serverinfo;
    char *c_userinfo[MAX_CLIENTS];
    qboolean c_exist[MAX_CLIENTS];

    unsigned short crc;
    double timestamp;
}
f_query_t;

#define F_QUERIES_REMEMBERED 5
f_query_t f_last_queries[F_QUERIES_REMEMBERED];
int f_last_query_pos = 0;

typedef struct f_modified_s {
	char name[MAX_QPATH];
	qboolean ismodified;
	struct f_modified_s *next;
} f_modified_t;

f_modified_t *f_modified_list;
qboolean care_f_modified;
qboolean f_modified_particles;
qboolean f_modified_staticlights;


cvar_t allow_f_version		= SCVAR("allow_f_version", "1");
cvar_t allow_f_server		= SCVAR("allow_f_server", "1");
cvar_t allow_f_modified		= SCVAR("allow_f_modified", "1");
cvar_t allow_f_skins		= SCVAR("allow_f_skins", "1");
cvar_t auth_validateclients	= SCVAR("auth_validateclients", "1");

unsigned short SCRC_GetQueryStateCrc(char *f_query_string)
{
    unsigned short crc;
    int i;
    char *tmp;

    QCRC_Init(&crc);

    // add query
    QCRC_AddBlock(&crc, f_query_string, strlen(f_query_string));

    // add snapshot of serverinfo
	tmp = Info_ValueForKey(cl.serverinfo, "deathmatch");
	QCRC_AddBlock(&crc, tmp, strlen(tmp));
	tmp = Info_ValueForKey(cl.serverinfo, "teamplay");
	QCRC_AddBlock(&crc, tmp, strlen(tmp));
	tmp = Info_ValueForKey(cl.serverinfo, "hostname");
	QCRC_AddBlock(&crc, tmp, strlen(tmp));
    tmp = Info_ValueForKey(cl.serverinfo, "*progs");
    QCRC_AddBlock(&crc, tmp, strlen(tmp));
    tmp = Info_ValueForKey(cl.serverinfo, "map");
    QCRC_AddBlock(&crc, tmp, strlen(tmp));
    tmp = Info_ValueForKey(cl.serverinfo, "spawn");
    QCRC_AddBlock(&crc, tmp, strlen(tmp));
    tmp = Info_ValueForKey(cl.serverinfo, "watervis");
    QCRC_AddBlock(&crc, tmp, strlen(tmp));
    tmp = Info_ValueForKey(cl.serverinfo, "fraglimit");
    QCRC_AddBlock(&crc, tmp, strlen(tmp));
    tmp = Info_ValueForKey(cl.serverinfo, "*gamedir");
    QCRC_AddBlock(&crc, tmp, strlen(tmp));
    tmp = Info_ValueForKey(cl.serverinfo, "timelimit");
    QCRC_AddBlock(&crc, tmp, strlen(tmp));

    // add snapshot of userinfo for every connected client
    for (i=0; i < MAX_CLIENTS; i++)
        if (cl.players[i].name[0])
        {
            tmp = Info_ValueForKey(cl.players[i].userinfo, "name");
            QCRC_AddBlock(&crc, tmp, strlen(tmp));
            tmp = Info_ValueForKey(cl.players[i].userinfo, "team");
            QCRC_AddBlock(&crc, tmp, strlen(tmp));
        }

    // done
    return crc;
}

#if 1

#define SECURITY_INIT_BAD_CHECKSUM	1
#define SECURITY_INIT_BAD_VERSION	2
#define SECURITY_INIT_ERROR			3
#define SECURITY_INIT_NOPROC		4

typedef struct signed_buffer_s {
	qbyte *buf;
	unsigned long size;
} signed_buffer_t;

typedef signed_buffer_t *(*Security_Verify_Response_t) (int, unsigned char *);
typedef int (*Security_Init_t) (char *);
typedef signed_buffer_t *(*Security_Generate_Crc_t) (int);
typedef signed_buffer_t *(*Security_IsModelModified_t) (char *, int, qbyte *, int);
typedef void (*Security_Supported_Binaries_t) (void *);
typedef void (*Security_Shutdown_t) (void);


Security_Verify_Response_t Security_Verify_Response;
Security_Init_t Security_Init;
Security_Generate_Crc_t Security_Generate_Crc;
Security_IsModelModified_t Security_IsModelModified;
Security_Supported_Binaries_t Security_Supported_Binaries;
Security_Shutdown_t Security_Shutdown;


void *secmodule;

void	ValidationPrintVersion(char *f_query_string)
{
	f_query_t *this_query;
	unsigned short query_crc;
	//unsigned long crc; //unreferenced
	//char answer; //unreferenced
	//char name[128]; //unrefernced
	char sr[256];
#ifdef RGLQUAKE
	char *s; // unreferenced in software only client
#endif
	int i;

	extern cvar_t r_shadow_realtime_world, r_drawflat;

	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		s = sr;
		//print certain allowed 'cheat' options.
		//realtime lighting (shadows can show around corners)
		//drawflat is just lame
		//24bits can be considered eeeevil, by some.
		if (r_shadows.value)
		{
			if (r_shadow_realtime_world.value)
				*s++ = 'W';
			else
				*s++ = 'S';
		}
		if (r_drawflat.value)
			*s++ = 'F';
		if (gl_load24bit.value)
			*s++ = 'H';

		*s = *"";
		break;
#endif
#ifdef SWQUAKE
	case QR_SOFTWARE:
		strcpy(sr, (r_pixbytes==4?"32bpp":"8bpp"));
		break;
#endif
	default:
		*sr = *"";
		break;
	}

	if (!allow_f_version.value)
		return;	//suppress it

	if (Security_Generate_Crc)
	{
		signed_buffer_t *resp;
		query_crc = SCRC_GetQueryStateCrc(f_query_string);

		//
		// remember this f_version
		//
		this_query = &f_last_queries[f_last_query_pos++ % F_QUERIES_REMEMBERED];
		this_query->timestamp = realtime;
		this_query->crc = query_crc;
		if (this_query->query)
			BZ_Free(this_query->query);
		this_query->query = BZ_Malloc(strlen(f_query_string)+1);
		strcpy(this_query->query, f_query_string);
		if (this_query->serverinfo)
			BZ_Free(this_query->serverinfo);
		this_query->serverinfo = BZ_Malloc(strlen(cl.serverinfo)+1);
		strcpy(this_query->serverinfo, cl.serverinfo);
		for (i=0; i < MAX_CLIENTS; i++)
		{
			if (this_query->c_userinfo[i])
			{
				BZ_Free(this_query->c_userinfo[i]);
				this_query->c_userinfo[i] = NULL;
			}
			this_query->c_exist[i] = false;

			if (cl.players[i].name[0])
			{
				this_query->c_exist[i] = true;
				this_query->c_userinfo[i] = BZ_Malloc(strlen(cl.players[i].userinfo)+1);
				strcpy(this_query->c_userinfo[i], cl.players[i].userinfo);
			}
		}

		resp = Security_Generate_Crc(cl.playernum[0]);

		//now send the data.

//		SPipe_WriteChar(f_write, SECURE_CMD_GETVERSION);
//        SPipe_WriteString(f_write, f_query_string);
//        SPipe_WriteString(f_write, cl.serverinfo);
//        SPipe_WriteString(f_write, cl.players[cl.playernum[0]].userinfo);

        // get answer
//        SPipe_ReadChar(f_read, &answer);
//        SPipe_ReadString(f_read, name, 64);
//        SPipe_ReadUlong(f_read, &crc);
/*
		if (answer == SECURE_ANSWER_OK)
		{
			// reply
			Cbuf_AddText("say ", RESTRICT_LOCAL);
			Cbuf_AddText(name, RESTRICT_LOCAL);
			if (*sr)
				Cbuf_AddText(va("/%s/%s", q_renderername, sr), RESTRICT_LOCAL);//extra info
			else
				Cbuf_AddText(va("/%s", q_renderername), RESTRICT_LOCAL);//extra info
			Cbuf_AddText(" ", RESTRICT_LOCAL);
			Cbuf_AddText(va("%04x", query_crc), RESTRICT_LOCAL);
			Cbuf_AddText(va("%08x", crc), RESTRICT_LOCAL);
			Cbuf_AddText("\n", RESTRICT_LOCAL);
			return;
		}
*/
	}

	if (*sr)
		Cbuf_AddText (va("say "DISTRIBUTION"Quake v%i "PLATFORM"/%s/%s\n", build_number(), q_renderername, sr), RESTRICT_RCON);
	else
		Cbuf_AddText (va("say "DISTRIBUTION"Quake v%i "PLATFORM"/%s\n", build_number(), q_renderername), RESTRICT_RCON);
}
void	Validation_FilesModified (void)
{
	Con_Printf ("Not implemented\n", RESTRICT_RCON);
}
void Validation_CheckIfResponse(char *text)
{
	//client name, version type(os-renderer where it matters, os/renderer where renderer doesn't), 12 char hex crc
	int f_query_client;
	int i;
	char *crc;
	char *versionstring;

	if (!Security_Verify_Response)
		return;	//valid or not, we can't check it.

	if (!auth_validateclients.value)
		return;

	//do the parsing.
	{
		char *comp;
		int namelen;

		for (crc = text + strlen(text) - 1; crc > text; crc--)
			if ((unsigned)*crc > ' ')
				break;

		//find the crc.
		for (i = 0; i < 29; i++)
		{
			if (crc <= text)
				return;	//not enough chars.
			if ((unsigned)crc[-1] <= ' ')
				break;
			crc--;
		}

		//we now want 3 string seperated tokens, so the first starts at the fourth found ' ' + 1
		i = 7;
		for (comp = crc-1; ; comp--)
		{
			if (comp < text)
				return;
			if (*comp == ' ')
			{
				i--;
				if (!i)
					break;
			}

		}

		versionstring = comp+1;
		if (comp <= text)
			return;	//not enough space for the 'name:'
		if (*(comp-1) != ':')
			return;	//whoops. not a say.

		namelen = comp - text-1;

		for (f_query_client = 0; f_query_client < MAX_CLIENTS; f_query_client++)
		{
			if (strlen(cl.players[f_query_client].name) == namelen)
				if (!strncmp(cl.players[f_query_client].name, text, namelen))
					break;
		}
		if (f_query_client == MAX_CLIENTS)
			return; //looks like a validation, but it's not from a known client.
	}

	{
		char *match = DISTRIBUTION"Quake v";
		if (strncmp(versionstring, match, strlen(match)))
			return;	//this is not us
	}

	//now do the validation
	{
		//f_query_t *query = NULL;
		int itemp;
		char buffer[10];
		unsigned short query_crc = 0;
		unsigned long	user_crc = 0;
		//unsigned long	auth_crc = 0;
		//char auth_answer; //unreferenced

		//int slot; //unreferenced
		signed_buffer_t *resp;

		//easy lame way to get the crc from hex.
		Q_strncpyS(buffer, crc, 4);
		buffer[4] = '\0';
		itemp = 0;
		sscanf(buffer, "%x", &itemp);
		query_crc = (unsigned long) itemp;

		Q_strncpyS(buffer, crc+4, 8);
		buffer[8] = '\0';
		itemp = 0;
		sscanf(buffer, "%x", &itemp);
		user_crc = (unsigned long) itemp;

		//
		// find that query
		//
/*		for (i=f_last_query_pos; i > f_last_query_pos-F_QUERIES_REMEMBERED; i--)
		{
			if (query_crc == f_last_queries[i % F_QUERIES_REMEMBERED].crc  &&
				realtime - 5 < f_last_queries[i % F_QUERIES_REMEMBERED].timestamp)
			{
				query = &f_last_queries[i % F_QUERIES_REMEMBERED];
			}
		}

		if (query == NULL)
			return; // reply to unknown query

		if (!query->c_exist[f_query_client])
			return; // should never happen
*/
		resp = Security_Verify_Response(f_query_client, crc);

		if (resp && resp->size && *resp->buf)
			Con_Printf(S_NOTICE "Authentication Successful.\n");
		else// if (!resp)
			Con_Printf(S_ERROR "AUTHENTICATION FAILED.\n");
		/*
		typedef signed_buffer_t *(*Security_Verify_Response_t) (int, unsigned char *);
		// write request
		SPipe_WriteChar(f_write, SECURE_CMD_CHECKVERSION2);
		SPipe_WriteString(f_write, query->query);
		SPipe_WriteString(f_write, query->serverinfo);
		SPipe_WriteString(f_write, query->c_userinfo[f_query_client]);
		SPipe_WriteString(f_write, versionstring);

		// get answer
		SPipe_ReadChar(f_read, &auth_answer);
		SPipe_ReadUlong(f_read, &auth_crc);

		if (auth_answer == SECURE_ANSWER_YES && auth_crc == user_crc)
		{
			Con_Printf(S_NOTICE "Authentication Successful.\n");
		}
		else
			Con_Printf(S_ERROR "AUTHENTICATION FAILED.\n");
			*/
	}
}

void	InitValidation(void)
{
	Cvar_Register(&allow_f_version,	"Authentication");
	Cvar_Register(&allow_f_server,	"Authentication");
	Cvar_Register(&allow_f_modified,	"Authentication");
	Cvar_Register(&allow_f_skins,	"Authentication");

#ifdef _WIN32
	secmodule = LoadLibrary("fteqw-security.dll");
	if (secmodule)
	{
		Security_Verify_Response	= (void*)GetProcAddress(secmodule, "Security_Verify_Response");
		Security_Init				= (void*)GetProcAddress(secmodule, "Security_Init");
		Security_Generate_Crc		= (void*)GetProcAddress(secmodule, "Security_Generate_Crc");
		Security_IsModelModified	= (void*)GetProcAddress(secmodule, "Security_IsModelModified");
		Security_Supported_Binaries	= (void*)GetProcAddress(secmodule, "Security_Supported_Binaries");
		Security_Shutdown			= (void*)GetProcAddress(secmodule, "Security_Shutdown");
	}
#endif

	if (Security_Init)
	{
		switch(Security_Init(va("%s %.2f %i", DISTRIBUTION, 2.57, build_number())))
		{
		case SECURITY_INIT_BAD_CHECKSUM:
			Con_Printf("Checksum failed. Security module does not support this build. Go upgrade it.\n");
			break;
		case SECURITY_INIT_BAD_VERSION:
			Con_Printf("Version failed. Security module does not support this version. Go upgrade.\n");
			break;
		case SECURITY_INIT_ERROR:
			Con_Printf("'Generic' security error. Stop hacking.\n");
			break;
		case SECURITY_INIT_NOPROC:
			Con_Printf("/proc/* does not exist. You will need to upgrade your kernel.\n");
			break;
		case 0:
			Cvar_Register(&auth_validateclients,	"Authentication");
			return;
		}
		Security_Verify_Response	= NULL;
		Security_Init				= NULL;
		Security_Generate_Crc		= NULL;
		Security_IsModelModified	= NULL;
		Security_Supported_Binaries	= NULL;
		Security_Shutdown			= NULL;
#ifdef _WIN32
		FreeLibrary(secmodule);
#endif
	}
}

void Validation_IncludeFile(char *filename, char *file, int filelen)
{
}

qboolean f_modified_particles;
qboolean f_modified_staticlights;
qboolean care_f_modified;



#else


#ifdef RGLQUAKE
#include "glquake.h"	//overkill
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

#define         ENV_READ_NAME   "FTE_SECURE_CHANNEL_READ"
#define         ENV_WRITE_NAME  "FTE_SECURE_CHANNEL_WRITE"

#define SECURE_CMD_CHECKMODEL   'c' // q: name:string, model:buffer
                                    // a: 'y' or 'n'

#define SECURE_CMD_GETVERSION   'g' // q: check_line:string, serverinfo:string, userinfo:string
                                    // a: ok, client_desc:string, crc:ulong

#define SECURE_CMD_CHECKVERSION 'v' // q: check_line:string, serverinfo:string, userinfo:string
                                    // a: ok, crc:ulong
#define SECURE_CMD_CHECKVERSION2	'r'	//SECURE_CMD_CHECKVERSION with the engine description appended on the end.
										//let's my front end work on a variety of engines rathar than just 1

#define SECURE_ANSWER_OK        'y'
#define SECURE_ANSWER_YES       'y'
#define SECURE_ANSWER_NO        'n'
#define SECURE_ANSWER_ERROR     'n'


//
// last f_queries
//
typedef struct f_query_s
{
    char *query;
    char *serverinfo;
    char *c_userinfo[MAX_CLIENTS];
    qboolean c_exist[MAX_CLIENTS];

    unsigned short crc;
    double timestamp;
}
f_query_t;

#define F_QUERIES_REMEMBERED 5
f_query_t f_last_queries[F_QUERIES_REMEMBERED];
int f_last_query_pos = 0;


qboolean care_f_modified;
qboolean f_modified_particles;

#ifdef _WIN32
#include "winquake.h"

typedef HANDLE qpipe;

// write to pipe, returns number of bytes written, or 0 = error
int Sys_WritePipe(qpipe q_pipe, unsigned char *buf, int buflen)
{
    DWORD   dwBytesWritten;
    BOOL    ret;

    ret = WriteFile(
        q_pipe,
        (LPVOID) buf,
        buflen,
        &dwBytesWritten,
        NULL);

    if (ret)
        return dwBytesWritten;
    else
        return 0;
}

// read from pipe, returns number of bytes read, or 0 = error
int Sys_ReadPipe(qpipe q_pipe, unsigned char *buf, int buflen)
{
    DWORD   dwBytesRead;
    BOOL    ret;

    ret = ReadFile(
        q_pipe,
        (LPVOID) buf,
        buflen,
        &dwBytesRead,
        NULL);

    if (ret)
        return dwBytesRead;
    else
        return 0;
}
#else
typedef int qpipe;

int Sys_WritePipe(qpipe q_pipe, unsigned char *buf, int buflen)
{
	return write(q_pipe, buf, buflen);
}

int Sys_ReadPipe(qpipe q_pipe, unsigned char *buf, int buflen)
{
	return read(q_pipe, buf, buflen);
}
#endif

static qpipe f_read, f_write;

int  SPipe_ReadMemory(qpipe read_pipe, unsigned char *buf, int buflen)
{
    int completed = 0;

    while (completed < buflen)
    {
        int read;

        read = Sys_ReadPipe(read_pipe, buf+completed, buflen-completed);

        if (read == 0)
            return false;

        completed += read;
    }
    return true;
}

int SPipe_ReadChar(qpipe read_pipe, char *c)
{
    return SPipe_ReadMemory(read_pipe, (unsigned char *)c, 1);
}

int SPipe_ReadInt(qpipe read_pipe, int *val)
{
    return SPipe_ReadMemory(read_pipe, (unsigned char *)val, sizeof(int));
}

int SPipe_ReadUlong(qpipe read_pipe, unsigned long *val)
{
    return SPipe_ReadMemory(read_pipe, (unsigned char *)val, sizeof(unsigned long));
}

int SPipe_ReadString(qpipe read_pipe, char *buf, int buflen)
{
	int i;
	int slen;
	if (!SPipe_ReadInt(read_pipe, &slen))
		return false;

	for (i = 0; i < buflen && i < slen; i++)
	{
		if (!SPipe_ReadChar(read_pipe, buf+i))
			return false;
	}
	buf[i] = '\0';
	for (; i < slen; i++)	//now read the extra data that wouldn't fit.
	{
		if (!SPipe_ReadChar(read_pipe, buf+i))
			return false;
	}

	return true;
}

int  SPipe_WriteMemory(qpipe write_pipe, unsigned char *buf, int buflen)
{
    int completed = 0;

    while (completed < buflen)
    {
        int written;

        written = Sys_WritePipe(write_pipe, buf+completed, buflen-completed);

        if (written == 0)
            return written;

        completed += written;
    }
    return completed;
}

int SPipe_WriteChar(qpipe write_pipe, char c)
{
    int written;
    written = SPipe_WriteMemory(write_pipe, (unsigned char *)(&c), 1);

    return (written==1);
}

int SPipe_WriteInt(qpipe write_pipe, int val)
{
    int written;
    written = SPipe_WriteMemory(write_pipe, (unsigned char *)(&val), sizeof(int));

    return (written==sizeof(int));
}

int SPipe_WriteString(qpipe write_pipe, char *string)
{
	int i;
	int len = strlen(string);
	if (!SPipe_WriteInt(write_pipe, len))
		return false;

	for (i = 0; i < len; i++)
		if (!SPipe_WriteMemory(write_pipe, (unsigned char *)(string+i), 1))
			return false;

	return true;
}

void InitValidation(void)
{
	char *read, *write;
	read = getenv(ENV_READ_NAME);
	write = getenv(ENV_WRITE_NAME);

	Cvar_Register(&allow_f_version,	"Authentication");
	Cvar_Register(&allow_f_modified,	"Authentication");
	Cvar_Register(&allow_f_skins,	"Authentication");
	Cvar_Register(&auth_validateclients,	"Authentication");

	if (!read || !write)
		return;

	f_read = (qpipe)atoi(read);
	f_write = (qpipe)atoi(write);

	if (!f_read || !f_write)
	{
		f_write = f_read = 0;
		return;
	}

}

void ValidationThink (void)
{
}

void ValidationSendRequest (void)
{
}


void  Validation_FilesModified (void)
{
	f_modified_t *fm;
    int count=0;
    char buf[1024];
    buf[0] = 0;

	if (!allow_f_modified.value)
		return;

    care_f_modified = true;

	if (f_modified_particles)
	{
		strcat(buf, "modified: particles");
		count++;
	}

    for (fm = f_modified_list; fm; fm = fm->next)
	{
        if (fm->ismodified)
        {
            char *tmp;
            if (!count)
                strcat(buf, "modified:");
            if (strlen(buf) < 250)
            {
                tmp = fm->name+1;
                while (strchr(tmp, '/'))
                    tmp = strchr(tmp, '/')+1;
                strcat(buf, " ");
                strcat(buf, tmp);
                count++;
            }
            else
            {
                strcat(buf, " & more...");
                break;
            }
        }
	}
    if (count == 0)
        strcat(buf, "all models ok");

    Cbuf_AddText("say ", RESTRICT_LOCAL);
    Cbuf_AddText(buf, RESTRICT_LOCAL);
    Cbuf_AddText("\n", RESTRICT_LOCAL);
}

void Validation_IncludeFile(char *filename, char *file, int filelen)
{
	char result;
	f_modified_t *fm;

	for (fm = f_modified_list; fm; fm = fm->next)
	{
		if (!strcmp(fm->name, filename))
			break;
	}
	if (!fm)
	{
		fm = Z_Malloc(sizeof(f_modified_t));
		fm->next = f_modified_list;
		f_modified_list = fm;
		Q_strncpyz(fm->name, filename, sizeof(fm->name));
	}

	fm->ismodified = true;
	if (f_read && allow_f_modified.value)
	{
		SPipe_WriteChar(f_write, SECURE_CMD_CHECKMODEL);
		SPipe_WriteString(f_write, fm->name);
		SPipe_WriteInt (f_write, filelen);
		SPipe_WriteMemory(f_write, file, filelen);

		SPipe_ReadChar(f_read, &result);

		if (result == SECURE_ANSWER_YES)
			fm->ismodified = false;
	}
	if (fm->ismodified && care_f_modified)
	{
		Cbuf_AddText("say previous f_modified response is no longer valid.\n", RESTRICT_LOCAL);
		care_f_modified = false;
	}
}

void ValidationPrintVersion(char *f_query_string)
{
	f_query_t *this_query;
	unsigned short query_crc;
	unsigned long crc;
	char answer;
	char name[128];
	char sr[256];
	int i;

	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		*sr = *"";
		break;
#endif
#ifdef SWQUAKE
	case QR_SOFTWARE:
		strcpy(sr, (r_pixbytes==4?"32bpp":"8bpp"));
		break;
#endif
	default:
		*sr = *"";
		break;
	}
	if (f_read && allow_f_version.value)
	{
		query_crc = SCRC_GetQueryStateCrc(f_query_string);

		//
		// remember this f_version
		//
		this_query = &f_last_queries[f_last_query_pos++ % F_QUERIES_REMEMBERED];
		this_query->timestamp = realtime;
		this_query->crc = query_crc;
		if (this_query->query)
			BZ_Free(this_query->query);
		this_query->query = BZ_Malloc(strlen(f_query_string)+1);
		strcpy(this_query->query, f_query_string);
		if (this_query->serverinfo)
			BZ_Free(this_query->serverinfo);
		this_query->serverinfo = BZ_Malloc(strlen(cl.serverinfo)+1);
		strcpy(this_query->serverinfo, cl.serverinfo);
		for (i=0; i < MAX_CLIENTS; i++)
		{
			if (this_query->c_userinfo[i])
			{
				BZ_Free(this_query->c_userinfo[i]);
				this_query->c_userinfo[i] = NULL;
			}
			this_query->c_exist[i] = false;

			if (cl.players[i].name[0])
			{
				this_query->c_exist[i] = true;
				this_query->c_userinfo[i] = BZ_Malloc(strlen(cl.players[i].userinfo)+1);
				strcpy(this_query->c_userinfo[i], cl.players[i].userinfo);
			}
		}

		//now send the data.

		SPipe_WriteChar(f_write, SECURE_CMD_GETVERSION);
        SPipe_WriteString(f_write, f_query_string);
        SPipe_WriteString(f_write, cl.serverinfo);
        SPipe_WriteString(f_write, cl.players[cl.playernum[0]].userinfo);

        // get answer
        SPipe_ReadChar(f_read, &answer);
        SPipe_ReadString(f_read, name, 64);
        SPipe_ReadUlong(f_read, &crc);

		if (answer == SECURE_ANSWER_OK)
		{
			// reply
			Cbuf_AddText("say ", RESTRICT_LOCAL);
			Cbuf_AddText(name, RESTRICT_LOCAL);
			if (*sr)
				Cbuf_AddText(va("/%s/%s", q_renderername, sr), RESTRICT_LOCAL);//extra info
			else
				Cbuf_AddText(va("/%s", q_renderername), RESTRICT_LOCAL);//extra info
			Cbuf_AddText(" ", RESTRICT_LOCAL);
			Cbuf_AddText(va("%04x", query_crc), RESTRICT_LOCAL);
			Cbuf_AddText(va("%08x", crc), RESTRICT_LOCAL);
			Cbuf_AddText("\n", RESTRICT_LOCAL);
			return;
		}
	}

	if (*sr)
		Cbuf_AddText (va("say "DISTRIBUTION"Quake v%4.3f-%i "PLATFORM"/%s/%s\n", VERSION, build_number(), q_renderername, sr), RESTRICT_RCON);
	else
		Cbuf_AddText (va("say "DISTRIBUTION"Quake v%4.3f-%i "PLATFORM"/%s\n", VERSION, build_number(), q_renderername), RESTRICT_RCON);
}

void Validation_CheckIfResponse(char *text)
{
	//client name, version type(os-renderer where it matters, os/renderer where renderer doesn't), 12 char hex crc
	int f_query_client;
	int i;
	char *crc;
	char *versionstring;

	if (!f_read)
		return;	//valid or not, we can't check it.

	if (!auth_validateclients.value)
		return;

	//do the parsing.
	{
		char *comp;
		int namelen;

		for (crc = text + strlen(text) - 1; crc > text; crc--)
			if (*crc > ' ')
				break;

		//find the crc.
		for (i = 0; i < 12; i++)
		{
			if (crc <= text)
				return;	//not enough chars.
			if ((*crc < '0' || *crc > '9') && (*crc < 'a' || *crc > 'f'))
				return;	//not a hex char.
			crc--;
		}

		//we now want 3 string seperated tokens, so the first starts at the fourth found ' ' + 1
		i = 4;
		for (comp = crc; ; comp--)
		{
			if (comp < text)
				return;
			if (*comp == ' ')
			{
				i--;
				if (!i)
					break;
			}

		}

		versionstring = comp+1;
		if (comp <= text)
			return;	//not enough space for the 'name:'
		if (*(comp-1) != ':')
			return;	//whoops. not a say.

		namelen = comp - text-1;

		for (f_query_client = 0; f_query_client < MAX_CLIENTS; f_query_client++)
		{
			if (strlen(cl.players[f_query_client].name) == namelen)
				if (!strncmp(cl.players[f_query_client].name, text, namelen))
					break;
		}
		if (f_query_client == MAX_CLIENTS)
			return; //looks like a validation, but it's not from a known client.

		crc++;
	}

	//now do the validation
	{
		f_query_t *query = NULL;
		int itemp;
		char buffer[10];
		unsigned short query_crc = 0;
		unsigned long	user_crc = 0;
		unsigned long	auth_crc = 0;
		char auth_answer;

		//easy lame way to get the crc from hex.
		Q_strncpyS(buffer, crc, 4);
		buffer[4] = '\0';
		itemp = 0;
		sscanf(buffer, "%x", &itemp);
		query_crc = (unsigned long) itemp;

		Q_strncpyS(buffer, crc+4, 8);
		buffer[8] = '\0';
		itemp = 0;
		sscanf(buffer, "%x", &itemp);
		user_crc = (unsigned long) itemp;

		//
		// find that query
		//
		for (i=f_last_query_pos; i > f_last_query_pos-F_QUERIES_REMEMBERED; i--)
		{
			if (query_crc == f_last_queries[i % F_QUERIES_REMEMBERED].crc  &&
				realtime - 5 < f_last_queries[i % F_QUERIES_REMEMBERED].timestamp)
			{
				query = &f_last_queries[i % F_QUERIES_REMEMBERED];
			}
		}

		if (query == NULL)
			return; // reply to unknown query

		if (!query->c_exist[f_query_client])
			return; // should never happen

		// write request
		SPipe_WriteChar(f_write, SECURE_CMD_CHECKVERSION2);
		SPipe_WriteString(f_write, query->query);
		SPipe_WriteString(f_write, query->serverinfo);
		SPipe_WriteString(f_write, query->c_userinfo[f_query_client]);
		SPipe_WriteString(f_write, versionstring);

		// get answer
		SPipe_ReadChar(f_read, &auth_answer);
		SPipe_ReadUlong(f_read, &auth_crc);

		if (auth_answer == SECURE_ANSWER_YES && auth_crc == user_crc)
		{
			Con_Printf(S_NOTICE "Authentication Successful.\n");
		}
		else
			Con_Printf(S_ERROR "AUTHENTICATION FAILED.\n");
	}
}


#endif

void Validation_FlushFileList(void)
{
	f_modified_t *fm;
	while(f_modified_list)
	{
		fm = f_modified_list->next;

		Z_Free(f_modified_list);
		f_modified_list = fm;
	}
}

void Validation_Server(void)
{
	Cbuf_AddText(va("say server is %s\n", NET_AdrToString(cls.netchan.remote_address)), RESTRICT_LOCAL);
}

void Validation_Skins(void)
{
	extern cvar_t r_fullbrightSkins, r_fb_models;
	int percent = r_fullbrightSkins.value*100;

	if (!allow_f_skins.value)
		return;

	if (percent < 0)
		percent = 0;
	if (percent > cls.allow_fbskins*100)
		percent = cls.allow_fbskins*100;
	if (percent)
		Cbuf_AddText(va("say all player skins %i%% fullbright%s\n", percent, r_fb_models.value?" (plus luma)":""), RESTRICT_LOCAL);
	else if (r_fb_models.value)
		Cbuf_AddText("say luma textures only\n", RESTRICT_LOCAL);
	else
		Cbuf_AddText("say Only cheaters use full bright skins\n", RESTRICT_LOCAL);
}
