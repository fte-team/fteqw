#include "quakedef.h"

#ifdef WEBSERVER

#include "iweb.h"

#ifdef WEBSVONLY	//we need some functions from quake

qboolean SV_AllowDownload (const char *name)
{
	if (strstr(name, ".."))
		return false;
	if (strchr(name, ':'))
		return false;
	if (*name == '/' || *name == '\\')
		return false;
	return true;
}
char		com_token[sizeof(com_token)];
com_tokentype_t com_tokentype;
int		com_argc;
const char	**com_argv;

vfsfile_t *IWebGenerateFile(char *name, char *content, int contentlength)
{
	return NULL;
}
vfsfile_t *VFSSTDIO_Open(const char *osname, const char *mode, qboolean *needsflush);
vfsfile_t *FS_OpenVFS(const char *filename, const char *mode, enum fs_relative relativeto)
{
	return VFSSTDIO_Open(filename, mode, NULL);
}
void Q_strncpyz(char *d, const char *s, int n)
{
	int i;
	n--;
	if (n < 0)
		return;	//this could be an error

	for (i=0; *s; i++)
	{
		if (i == n)
			break;
		*d++ = *s++;
	}
	*d='\0';
}

/*char	*va(char *format, ...)
{
#define VA_BUFFERS 2 //power of two
	va_list		argptr;
	static char		string[VA_BUFFERS][1024];
	static int bufnum;

	bufnum++;
	bufnum &= (VA_BUFFERS-1);
	
	va_start (argptr, format);
	_vsnprintf (string[bufnum],sizeof(string[bufnum])-1, format,argptr);
	va_end (argptr);

	return string[bufnum];	
}*/

#undef _vsnprintf
void Sys_Error(const char *format, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
#ifdef _WIN32
	_vsnprintf (string,sizeof(string)-1, format,argptr);
	string[sizeof(string)-1] = 0;
#else
	vsnprintf (string,sizeof(string), format,argptr);
#endif
	va_end (argptr);

	printf("%s", string);
	getchar();
	exit(1000);
}

int COM_CheckParm(const char *parm)
{
	return 0;
}

char *authedusername;
char *autheduserpassword;
int main(int argc, char **argv)
{
	int httpport = 80;
	int arg = 1;
#ifdef _WIN32
	WSADATA pointlesscrap;
	WSAStartup(2, &pointlesscrap);
#endif

	if (arg < argc && atoi(argv[arg]))
		httpport = atoi(argv[arg++]);
	if (arg < argc)
		authedusername = argv[arg++];
	if (arg < argc)
		autheduserpassword = argv[arg++];

	printf("http port %i\n", httpport);
	if (authedusername || autheduserpassword)
		printf("Username = \"%s\"\nPassword = \"%s\"\n", authedusername, autheduserpassword);
	else
		printf("Server is read only\n");

	while(1)
	{
//		FTP_ServerRun(1, 21);
		if (httpport)
			HTTP_ServerPoll(1, httpport);
#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000000);
#endif
	}
}

#ifdef _WIN32
#ifdef _MSC_VER
#define ULL(x) x##ui64
#else
#define ULL(x) x##ull
#endif

static time_t Sys_FileTimeToTime(FILETIME ft)
{
	ULARGE_INTEGER ull;
	ull.LowPart = ft.dwLowDateTime;
	ull.HighPart = ft.dwHighDateTime;
	return ull.QuadPart / ULL(10000000) - ULL(11644473600);
}
void COM_EnumerateFiles (const char *match, int (*func)(const char *, qofs_t, time_t mtime, void *, searchpathfuncs_t *f), void *parm)
{
	HANDLE r;
	WIN32_FIND_DATAA fd;	
	char apath[MAX_OSPATH];
	char file[MAX_OSPATH];
	char *s;
	int go;
	strcpy(apath, match);
//	sprintf(apath, "%s%s", gpath, match);
	for (s = apath+strlen(apath)-1; s>= apath; s--)
	{
		if (*s == '/')			
			break;
	}
	s++;
	*s = '\0';	
	
	strcpy(file, match);
	r = FindFirstFileA(file, &fd);
	if (r==(HANDLE)-1)
		return;
	go = true;
	do
	{
		if (*fd.cFileName == '.');
		else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)	//is a directory
		{
			sprintf(file, "%s%s/", apath, fd.cFileName);
			go = func(file, fd.nFileSizeLow, Sys_FileTimeToTime(fd.ftLastWriteTime), parm, NULL);
		}
		else
		{
			sprintf(file, "%s%s", apath, fd.cFileName);
			go = func(file, fd.nFileSizeLow, Sys_FileTimeToTime(fd.ftLastWriteTime), parm, NULL);
		}
	}
	while(FindNextFileA(r, &fd) && go);
	FindClose(r);
}
#endif

char *COM_ParseType (const char *data, char *out, int outlen, com_tokentype_t *toktype)
{
	int		c;
	int		len;
	
	len = 0;
	out[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}
	
// skip // comments
	if (c=='/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
			goto skipwhite;
		}
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		com_tokentype = TTP_STRING;
		data++;
		while (1)
		{
			if (len >= outlen-1)
				return (char*)data;

			c = *data++;
			if (c=='\"' || !c)
			{
				out[len] = 0;
				return (char*)data;
			}
			out[len] = c;
			len++;
		}
	}

	com_tokentype = TTP_UNKNOWN;

// parse a regular word
	do
	{
		if (len >= outlen-1)
			return (char*)data;

		out[len] = c;
		data++;
		len++;
		c = *data;
	} while (c>32);
	
	out[len] = 0;
	return (char*)data;
}

char *COM_ParseToken (const char *data, const char *punctuation)
{
	int		c;
	int		len;
#ifndef WEBSVONLY
	COM_AssertMainThread("COM_ParseToken");
#endif
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}
	
// skip // comments
	if (c=='/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
			goto skipwhite;
		}
		else if (data[1] == '*')
		{
			data+=2;
			while (*data && (*data != '*' || data[1] != '/'))
				data++;
			data+=2;
			goto skipwhite;
		}
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		com_tokentype = TTP_STRING;
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return (char*)data;
			}
			com_token[len] = c;
			len++;
		}
	}

	com_tokentype = TTP_UNKNOWN;

// parse single characters
	if (c==',' || c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':' || c==';' || c == '=' || c == '!' || c == '>' || c == '<' || c == '&' || c == '|' || c == '+')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return (char*)data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c==',' || c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':' || c==';' || c == '=' || c == '!' || c == '>' || c == '<' || c == '&' || c == '|' || c == '+')
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return (char*)data;
}

/*
IWEBFILE *IWebFOpenRead(char *name)					//fread(name, "rb");
{
	FILE *f;
	char name2[512];
	if (strstr(name, ".."))
		return NULL;
	sprintf(name2, "%s/%s", com_gamedir, name);
	f = fopen(name2, "rb");
	if (f)
	{
		IWEBFILE *ret = IWebMalloc(sizeof(IWEBFILE));
		if (!ret)
		{
			fclose(f);
			return NULL;
		}
		ret->f = f;
		ret->start = 0;

		fseek(f, 0, SEEK_END);
		ret->end = ftell(f);//ret->start+ret->length;
		fseek(f, 0, SEEK_SET);

		ret->length = ret->end - ret->start;
		return ret;
	}
	return NULL;
}
*/


#else

#ifndef CLIENTONLY
cvar_t ftpserver = CVAR("sv_ftp", "0");
cvar_t ftpserver_port = CVAR("sv_ftp_port", "21");
cvar_t httpserver = CVAR("sv_http", "0");
cvar_t httpserver_port = CVAR("sv_http_port", "80");
cvar_t sv_readlevel = CVAR("sv_readlevel", "0");	//default to allow anyone
cvar_t sv_writelevel = CVAR("sv_writelevel", "35");	//allowed to write to uploads/uname
cvar_t sv_fulllevel = CVAR("sv_fulllevel", "51");	//allowed to write anywhere, replace any file...
#endif

//this file contains functions called from each side.

void VARGS IWebDPrintf(char *fmt, ...)
{
	va_list		argptr;
	char		msg[4096];

	if (!developer.value)
		return;
	
	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-10, fmt,argptr);	//catch any nasty bugs... (this is hopefully impossible)
	va_end (argptr);

	Con_Printf("%s", msg);
}
void VARGS IWebPrintf(char *fmt, ...)
{
	va_list		argptr;
	char		msg[4096];

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-10, fmt,argptr);	//catch any nasty bugs... (this is hopefully impossible)
	va_end (argptr);

	Con_Printf("%s", msg);
}
void VARGS IWebWarnPrintf(char *fmt, ...)
{
	va_list		argptr;
	char		msg[4096];

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-10, fmt,argptr);	//catch any nasty bugs... (this is hopefully impossible)
	va_end (argptr);

	Con_Printf(CON_WARNING "%s", msg);
}

void IWebInit(void)
{
#ifdef WEBSERVER
	Cvar_Register(&sv_fulllevel, "Internet Server Access");
	Cvar_Register(&sv_writelevel, "Internet Server Access");
	Cvar_Register(&sv_readlevel, "Internet Server Access");

	Cvar_Register(&ftpserver, "Internet Server Access");
	Cvar_Register(&ftpserver_port, "Internet Server Access");
	Cvar_Register(&httpserver, "Internet Server Access");
	Cvar_Register(&httpserver_port, "Internet Server Access");

	//don't allow these to be changed easily
	//this basically blocks these from rcon / stuffcmd
	ftpserver.restriction = RESTRICT_MAX;
	httpserver.restriction = RESTRICT_MAX;
	sv_fulllevel.restriction = RESTRICT_MAX;
	sv_writelevel.restriction = RESTRICT_MAX;
	sv_readlevel.restriction = RESTRICT_MAX;
#endif
}
void IWebRun(void)
{
#ifdef WEBSERVER
	extern qboolean httpserverfailed, ftpserverfailed;

	FTP_ServerRun(ftpserver.ival!= 0, ftpserver_port.ival);
	HTTP_ServerPoll(httpserver.ival!=0, httpserver_port.ival);
	if (ftpserverfailed)
	{
		Con_Printf("FTP Server failed to load, setting %s to 0\n", ftpserver.name);
		Cvar_SetValue(&ftpserver, 0);
		ftpserverfailed = false;
	}
	if (httpserverfailed)
	{
		Con_Printf("HTTP Server failed to load, setting %s to 0\n", httpserver.name);
		Cvar_SetValue(&httpserver, 0);
		httpserverfailed = false;
	}
#endif
}
void IWebShutdown(void)
{
}
#endif

#ifndef WEBSVONLY
//replacement for Z_Malloc. It simply allocates up to a reserve ammount.
void *IWebMalloc(int size)
{
	void *mem = BZF_Malloc(size);
	memset(mem, 0, size);
	return mem;
}

void *IWebRealloc(void *old, int size)
{
	return BZF_Realloc(old, size);
}
#endif




int IWebAuthorize(char *name, char *password)
{
#ifdef WEBSVONLY
	if (authedusername)
		if (!strcmp(name, authedusername))
			if (!strcmp(password, autheduserpassword))
				return IWEBACC_FULL;
	return IWEBACC_READ;
#else
#ifndef CLIENTONLY
	int id = Rank_GetPlayerID(NULL, name, atoi(password), false, true);
	rankinfo_t info;
	if (!id)
	{
		if (!sv_readlevel.value)
			return IWEBACC_READ;	//read only anywhere
		return 0;
	}

	Rank_GetPlayerInfo(id, &info);

	if (info.s.trustlevel >= sv_fulllevel.value)
		return IWEBACC_READ	| IWEBACC_WRITE | IWEBACC_FULL;	//allowed to read and write anywhere to the quake filesystem
	if (info.s.trustlevel >= sv_writelevel.value)
		return IWEBACC_READ	| IWEBACC_WRITE;	//allowed to read anywhere write to specific places
	if (info.s.trustlevel >= sv_readlevel.value)
		return IWEBACC_READ;	//read only anywhere
#endif
	return 0;
#endif
}

iwboolean IWebAllowUpLoad(char *fname, char *uname)	//called for partial write access
{
	if (strstr(fname, ".."))
		return false;
	if (!strncmp(fname, "uploads/", 8))
	{
		if (!strncmp(fname+8, uname, strlen(uname)))
			if (fname[8+strlen(uname)] == '/')
				return true;
	}
	return false;
}

iwboolean	FTP_StringToAdr (const char *s, qbyte ip[4], qbyte port[2])
{
	s = COM_ParseToken(s, NULL);
	ip[0] = atoi(com_token);

	s = COM_ParseToken(s, NULL);
	if (*com_token != ',')
		return false;

	s = COM_ParseToken(s, NULL);
	ip[1] = atoi(com_token);

	s = COM_ParseToken(s, NULL);
	if (*com_token != ',')
		return false;

	s = COM_ParseToken(s, NULL);
	ip[2] = atoi(com_token);

	s = COM_ParseToken(s, NULL);
	if (*com_token != ',')
		return false;
	
	s = COM_ParseToken(s, NULL);
	ip[3] = atoi(com_token);

	s = COM_ParseToken(s, NULL);
	if (*com_token != ',')
		return false;
	
	s = COM_ParseToken(s, NULL);
	port[0] = atoi(com_token);

	s = COM_ParseToken(s, NULL);
	if (*com_token != ',')
		return false;
	
	s = COM_ParseToken(s, NULL);
	port[1] = atoi(com_token);


	return true;
}

char *Q_strcpyline(char *out, const char *in, int maxlen)
{
	char *w = out;
	while (*in && maxlen > 0)
	{
		if (*in == '\r' || *in == '\n')
			break;
		*w = *in;
		in++;
		w++;
		maxlen--;
	}
	*w = '\0';
	return out;
}

#endif
