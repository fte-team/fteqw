#include "quakedef.h"

#include <SDL.h>
#ifdef MULTITHREAD
#include <SDL_thread.h>
#endif

#include <SDL_loadso.h>


#ifndef WIN32
#include <fcntl.h>
#else
#include <direct.h>
#endif

#ifndef isDedicated
qboolean isDedicated;
#endif

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

	va_start (argptr,error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);

	Con_Print ("Quake Error: ");
	Con_Print (string);
	Con_Print ("\n");

	if (COM_CheckParm("-crashonerror"))
		*(int*)-3 = 0;

	Host_Shutdown ();
	exit (1);
}

void Sys_RecentServer(char *command, char *target, char *title, char *desc)
{
}

qboolean Sys_RandomBytes(qbyte *string, int len)
{
	return false;
}

//print into stdout
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;	
		
	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
}

unsigned int Sys_Milliseconds(void)
{
	static int first = true;
	static unsigned long oldtime = 0, curtime = 0;
	unsigned long newtime;

	newtime = SDL_GetTicks();

	if (first)
	{
		first = false;
		oldtime = newtime;
	}
	if (newtime < oldtime)
		Con_Printf("Sys_Milliseconds stepped backwards!\n");
	else
		curtime += oldtime - newtime;
	oldtime = newtime;
	return curtime;
}

//return the current time, in the form of a double
double Sys_DoubleTime (void)
{
	static int first = true;
	static double oldtime = 0.0, curtime = 0.0;
	double newtime;

	newtime = (double) SDL_GetTicks() / 1000.0;


	if (first)
	{
		first = false;
		oldtime = newtime;
	}

	if (newtime < oldtime)
	{
		// warn if it's significant
		if (newtime - oldtime < -0.01)
			Con_Printf("Sys_DoubleTime: time stepped backwards (went from %f to %f, difference %f)\n", oldtime, newtime, newtime - oldtime);
	}
	else
		curtime += newtime - oldtime;
	oldtime = newtime;

	return curtime;
}

//create a directory
void Sys_mkdir (char *path)
{
#if WIN32
	_mkdir (path);
#else
	mkdir (path, 0777);	//WARNING: DO NOT RUN AS ROOT!
#endif
}

//unlink a file
qboolean Sys_remove (char *path)
{
	remove(path);

	return true;
}

qboolean Sys_Rename (char *oldfname, char *newfname)
{
	return !rename(oldfname, newfname);
}

//someone used the 'quit' command
void Sys_Quit (void)
{
	Host_Shutdown();

	exit (0);
}

//enumerate the files in a directory (of both gpath and match - match may not contain ..)
//calls the callback for each one until the callback returns 0
//SDL provides no file enumeration facilities.
#if defined(_WIN32)
#include <windows.h>
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, int, void *, void *), void *parm, void *spath)
{
	HANDLE r;
	WIN32_FIND_DATA fd;	
	char apath[MAX_OSPATH];
	char apath2[MAX_OSPATH];
	char file[MAX_OSPATH];
	char *s;
	int go;
	if (!gpath)
		return 0;
//	strcpy(apath, match);
	Q_snprintfz(apath, sizeof(apath), "%s/%s", gpath, match);
	for (s = apath+strlen(apath)-1; s> apath; s--)
	{
		if (*s == '/')			
			break;
	}
	*s = '\0';

	//this is what we ask windows for.
	Q_snprintfz(file, sizeof(file), "%s/*.*", apath);

	//we need to make apath contain the path in match but not gpath
	Q_strncpyz(apath2, match, sizeof(apath));
	match = s+1;
	for (s = apath2+strlen(apath2)-1; s> apath2; s--)
	{
		if (*s == '/')			
			break;
	}
	*s = '\0';
	if (s != apath2)
		strcat(apath2, "/");

	r = FindFirstFile(file, &fd);
	if (r==(HANDLE)-1)
		return 1;
    go = true;
	do
	{
		if (*fd.cFileName == '.');	//don't ever find files with a name starting with '.'
		else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)	//is a directory
		{
			if (wildcmp(match, fd.cFileName))
			{
				Q_snprintfz(file, sizeof(file), "%s%s/", apath2, fd.cFileName);
				go = func(file, fd.nFileSizeLow, parm, spath);
			}
		}
		else
		{
			if (wildcmp(match, fd.cFileName))
			{
				Q_snprintfz(file, sizeof(file), "%s%s", apath2, fd.cFileName);
				go = func(file, fd.nFileSizeLow, parm, spath);
			}
		}
	}
	while(FindNextFile(r, &fd) && go);
	FindClose(r);

	return go;
}
#elif defined(linux) || defined(__unix__)
#include <dirent.h>
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, int, void *, void *), void *parm, void *spath)
{
	DIR *dir;
	char apath[MAX_OSPATH];
	char file[MAX_OSPATH];
	char truepath[MAX_OSPATH];
	char *s;
	struct dirent *ent;
	struct stat st;

//printf("path = %s\n", gpath);
//printf("match = %s\n", match);

	if (!gpath)
		gpath = "";
	*apath = '\0';

	Q_strncpyz(apath, match, sizeof(apath));
	for (s = apath+strlen(apath)-1; s >= apath; s--)
	{
		if (*s == '/')
		{
			s[1] = '\0';
			match += s - apath+1;
			break;
		}
	}
	if (s < apath)	//didn't find a '/'
		*apath = '\0';

	Q_snprintfz(truepath, sizeof(truepath), "%s/%s", gpath, apath);


//printf("truepath = %s\n", truepath);
//printf("gamepath = %s\n", gpath);
//printf("apppath = %s\n", apath);
//printf("match = %s\n", match);
	dir = opendir(truepath);
	if (!dir)
	{
		Con_DPrintf("Failed to open dir %s\n", truepath);
		return true;
	}
	do
	{
		ent = readdir(dir);
		if (!ent)
			break;
		if (*ent->d_name != '.')
		{
			if (wildcmp(match, ent->d_name))
			{
				Q_snprintfz(file, sizeof(file), "%s/%s", truepath, ent->d_name);

				if (stat(file, &st) == 0)
				{
					Q_snprintfz(file, sizeof(file), "%s%s%s", apath, ent->d_name, S_ISDIR(st.st_mode)?"/":"");

					if (!func(file, st.st_size, parm, spath))
					{
						closedir(dir);
						return false;
					}
				}
				else
					printf("Stat failed for \"%s\"\n", file);
			}
		}
	} while(1);
	closedir(dir);

	return true;
}
#else
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, int, void *, void *), void *parm, void *spath)
{
	Con_Printf("Warning: Sys_EnumerateFiles not implemented\n");
	return false;
}
#endif

//blink window if possible (it's not)
void Sys_ServerActivity(void)
{
}

void Sys_CloseLibrary(dllhandle_t *lib)
{
	SDL_UnloadObject((void*)lib);
}
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	int i;
	void *lib;

	lib = SDL_LoadObject(name);
	if (!lib)
		return NULL;

	if (funcs)
	{
		for (i = 0; funcs[i].name; i++)
		{
			*funcs[i].funcptr = SDL_LoadFunction(lib, funcs[i].name);
			if (!*funcs[i].funcptr)
				break;
		}
		if (funcs[i].name)
		{
			Sys_CloseLibrary((dllhandle_t*)lib);
			lib = NULL;
		}
	}

	return (dllhandle_t*)lib;
}
void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname)
{
	if (!module)
		return NULL;
	return SDL_LoadFunction((void *)module, exportname);
}

//Without these two we cannot run Q2 gamecode.
dllhandle_t *q2gamedll;
void Sys_UnloadGame (void)
{
	if (q2gamedll)
		Sys_CloseLibrary(q2gamedll);
	q2gamedll = NULL;
}
void *Sys_GetGameAPI (void *parms)
{
	void *(*GetGameAPI)(void *);
	dllfunction_t funcs[] =
	{
		{(void**)&GetGameAPI, "GetGameAPI"},
		{NULL,NULL}
	};

	char name[MAX_OSPATH];
	char curpath[MAX_OSPATH];
	char *searchpath;
	const char *gamename = "gamesdl.so";

	void *ret;

	Con_DPrintf("Searching for %s\n", gamename);

	getcwd(curpath, sizeof(curpath));

	searchpath = 0;
	while((searchpath = COM_NextPath(searchpath)))
	{
		if (searchpath[0] == '/')
			snprintf(name, sizeof(name), "%s/%s", searchpath, gamename);
		else
			snprintf(name, sizeof(name), "%s/%s/%s", curpath, searchpath, gamename);

		q2gamedll = Sys_LoadLibrary(name, funcs);
		if (q2gamedll && gamename)
		{
			ret = GetGameAPI(parms);
			if (ret)
			{
				return ret;
			}

			Sys_CloseLibrary(q2gamedll);
			q2gamedll = 0;
		}
	}

	return NULL;
}



//used to see if a file exists or not.
int	Sys_FileTime (char *path)
{
	FILE	*f;
	
	f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	
	return -1;
}

void Sys_Init(void)
{
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
}
void Sys_Shutdown(void)
{
	SDL_Quit();
}



int VARGS Sys_DebugLog(char *file, char *fmt, ...)
{
	FILE *fd;
	va_list argptr; 
	static char data[1024];
    
	va_start(argptr, fmt);
	vsnprintf(data, sizeof(data)-1, fmt, argptr);
	va_end(argptr);

#if defined(CRAZYDEBUGGING) && CRAZYDEBUGGING > 1
	{
		static int sock;
		if (!sock)
		{
			struct sockaddr_in sa;
			netadr_t na;
			int _true = true;
			int listip;
			listip = COM_CheckParm("-debugip");
			NET_StringToAdr(listip?com_argv[listip+1]:"127.0.0.1", &na);
			NetadrToSockadr(&na, (struct sockaddr_qstorage*)&sa);
			sa.sin_port = htons(10000);
			sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (-1==connect(sock, (struct sockaddr*)&sa, sizeof(sa)))
				Sys_Error("Couldn't send debug log lines\n");
			setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&_true, sizeof(_true));
		}
		send(sock, data, strlen(data), 0);
	}
#endif
	fd = fopen(file, "wt");
	if (fd)
	{
		fwrite(data, 1, strlen(data), fd);
		fclose(fd);
		return 0;
	}
	return 1;
};



qboolean Sys_InitTerminal(void)
{
	return true;
}
char *Sys_ConsoleInput(void)
{
	return NULL;
}
void Sys_CloseTerminal (void)
{
}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#endif

#ifdef FTE_TARGET_WEB
void Sys_MainLoop(void)
{
	static float oldtime;
	float newtime, time;
	newtime = Sys_DoubleTime ();
	if (!oldtime)
		oldtime = newtime;
	time = newtime - oldtime;
	Host_Frame (time);
	oldtime = newtime;
}
#endif

int QDECL main(int argc, char **argv)
{
	float time, newtime, oldtime;
	quakeparms_t	parms;
	int				t;
	int delay = 1;

	memset(&parms, 0, sizeof(parms));

	parms.argv = argv;

	parms.basedir = ".";

	parms.argc = argc;
	parms.argv = argv;

#ifndef WIN32
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	COM_InitArgv (parms.argc, parms.argv);

	TL_InitLanguages();

	parms.memsize = 0x2000000;

	if (parms.memsize < 0x0800000)
		parms.memsize = 0x0800000;


	if (COM_CheckParm ("-heapsize"))
	{
		t = COM_CheckParm("-heapsize") + 1;

		if (t < com_argc)
			parms.memsize = Q_atoi (com_argv[t]) * 1024;
	}
	else if (COM_CheckParm ("-mem"))
	{
		t = COM_CheckParm("-mem") + 1;

		if (t < com_argc)
			parms.memsize = Q_atoi (com_argv[t]) * 1024*1024;
	}


#ifdef _WIN32
	parms.membase = VirtualAlloc (NULL, parms.memsize, MEM_RESERVE, PAGE_NOACCESS);
#else
	parms.membase = malloc (parms.memsize);
#endif

	Sys_Printf ("Host_Init\n");
	Host_Init (&parms);

	oldtime = Sys_DoubleTime ();


#ifdef FTE_TARGET_WEB
	//-1 fps should give vsync
	emscripten_set_main_loop(Sys_MainLoop, -1, false);
#else
//client console should now be initialized.

    /* main window message loop */
	while (1)
	{
#ifndef CLIENTONLY
		if (isDedicated)
		{
			NET_Sleep(delay, false);

		// find time passed since last cycle
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
			oldtime = newtime;
			
			delay = SV_Frame()*1000;
		}
		else
#endif
		{
			double sleeptime;

	// yield the CPU for a little while when paused, minimized, or not the focus
			if (!(SDL_GetAppState() & SDL_APPACTIVE))
				SDL_Delay(1);

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
			sleeptime = Host_Frame (time);
			oldtime = newtime;

			Sys_Sleep(sleeptime);
		}
	}
#endif
	return 0;
}

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
	return false;
}




char *Sys_GetClipboard(void)
{
	return NULL;
}
void Sys_CloseClipboard(char *bf)
{
}
void Sys_SaveClipboard(char *text)
{
}

#ifdef MULTITHREAD
/* Thread creation calls */
void *Sys_CreateThread(char *name, int (*func)(void *), void *args, int priority, int stacksize)
{
	// SDL threads do not support setting thread stack size
	return (void *)SDL_CreateThread(func, args);
}

void Sys_WaitOnThread(void *thread)
{
	SDL_WaitThread((SDL_Thread *)thread, NULL);
}


/* Mutex calls */
// SDL mutexes don't have try-locks for mutexes in the spec so we stick with 1-value semaphores
void *Sys_CreateMutex(void)
{
	return (void *)SDL_CreateSemaphore(1);
}

qboolean Sys_TryLockMutex(void *mutex)
{
	return !SDL_SemTryWait(mutex);
}

qboolean Sys_LockMutex(void *mutex)
{
	return !SDL_SemWait(mutex);
}

qboolean Sys_UnlockMutex(void *mutex)
{
	return !SDL_SemPost(mutex);
}

void Sys_DestroyMutex(void *mutex)
{
	SDL_DestroySemaphore(mutex);
}

/* Conditional wait calls */
typedef struct condvar_s
{
	SDL_mutex *mutex;
	SDL_cond *cond;
} condvar_t;

void *Sys_CreateConditional(void)
{
	condvar_t *condv;
	SDL_mutex *mutex;
	SDL_cond *cond;
	
	condv = (condvar_t *)malloc(sizeof(condvar_t));
	if (!condv)
		return NULL;
		
	mutex = SDL_CreateMutex();
	cond = SDL_CreateCond();
	
	if (mutex)
	{
		if (cond)
		{
			condv->cond = cond;
			condv->mutex = mutex;
		
			return (void *)condv;
		}
		else
			SDL_DestroyMutex(mutex);
	}
	
	free(condv);
	return NULL;	
}

qboolean Sys_LockConditional(void *condv)
{
	return !SDL_mutexP(((condvar_t *)condv)->mutex);
}

qboolean Sys_UnlockConditional(void *condv)
{
	return !SDL_mutexV(((condvar_t *)condv)->mutex);
}

qboolean Sys_ConditionWait(void *condv)
{
	return !SDL_CondWait(((condvar_t *)condv)->cond, ((condvar_t *)condv)->mutex);
}

qboolean Sys_ConditionSignal(void *condv)
{
	return !SDL_CondSignal(((condvar_t *)condv)->cond);
}

qboolean Sys_ConditionBroadcast(void *condv)
{
	return !SDL_CondBroadcast(((condvar_t *)condv)->cond);
}

void Sys_DestroyConditional(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;
	
	SDL_DestroyCond(cv->cond);
	SDL_DestroyMutex(cv->mutex);
	free(cv);
}
#endif

void Sys_Sleep (double seconds)
{
	SDL_Delay(seconds * 1000);
}

#ifdef FTE_TARGET_WEB
//emscripten does not support the full set of sdl functions, so we stub the extras.
int SDL_GetGammaRamp(Uint16 *redtable, Uint16 *greentable, Uint16 *bluetable)
{
	return -1;
}
int SDL_SetGammaRamp(const Uint16 *redtable, const Uint16 *greentable, const Uint16 *bluetable)
{
	return -1;
}
//SDL_GL_GetAttribute
void SDL_UnloadObject(void *object)
{
}
void *SDL_LoadObject(const char *sofile)
{
	return NULL;
}
void *SDL_LoadFunction(void *handle, const char *name)
{
	return NULL;
}
Uint8 SDL_GetAppState(void)
{
	return SDL_APPACTIVE;
}
#define socklen_t int
int getsockname(int socket, struct sockaddr *address, socklen_t *address_len)
{
	return -1;
}
int getpeername(int socket, struct sockaddr *address, socklen_t *address_len)
{
	return -1;
}
ssize_t sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	return -1;
}
#endif

