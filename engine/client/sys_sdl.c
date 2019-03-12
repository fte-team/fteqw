#include "quakedef.h"

#include <SDL.h>
#ifdef MULTITHREAD
#include <SDL_thread.h>
#endif

#include <SDL_loadso.h>


#ifndef WIN32
#include <fcntl.h>
#include <sys/stat.h>
#ifdef __unix__
#include <unistd.h>
#endif
#else
#include <direct.h>
#endif

#ifdef FTE_TARGET_WEB
#include <emscripten/emscripten.h>
#endif

#if SDL_MAJOR_VERSION >= 2
SDL_Window *sdlwindow;
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
	COM_WorkerAbort(string);
	fprintf(stderr, "Error: %s\n", string);

	Sys_Printf ("Quake Error: %s\n", string);

#if SDL_MAJOR_VERSION >= 2
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sys_Error", string, sdlwindow);
#endif

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
	static unsigned long starttime = 0;
	unsigned long now;

	now = SDL_GetTicks();

	if (first)
	{
		first = false;
		starttime = now;
	}

	return now - starttime;
}

//return the current time, in the form of a double
double Sys_DoubleTime (void)
{
	return Sys_Milliseconds() / 1000.0;
/*
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
*/
}

//create a directory
void Sys_mkdir (const char *path)
{
#if WIN32
	_mkdir (path);
#else
	//user, group, others
	mkdir (path, 0755);	//WARNING: DO NOT RUN AS ROOT!
#endif
}

qboolean Sys_rmdir (const char *path)
{
	int ret;
#if WIN32
	ret = _rmdir (path);
#else
	ret = rmdir (path);
#endif
	if (ret == 0)
		return true;
//	if (errno == ENOENT)
//		return true;
	return false;
}

//unlink a file
qboolean Sys_remove (const char *path)
{
	remove(path);

	return true;
}

qboolean Sys_Rename (const char *oldfname, const char *newfname)
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
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, qofs_t, time_t mtime, void *, searchpathfuncs_t *), void *parm, searchpathfuncs_t *spath)
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
				go = func(file, fd.nFileSizeLow, 0, parm, spath);
			}
		}
		else
		{
			if (wildcmp(match, fd.cFileName))
			{
				Q_snprintfz(file, sizeof(file), "%s%s", apath2, fd.cFileName);
				go = func(file, fd.nFileSizeLow, 0, parm, spath);
			}
		}
	}
	while(FindNextFile(r, &fd) && go);
	FindClose(r);

	return go;
}
#elif defined(linux) || defined(__unix__) || defined(__MACH__)
#include <dirent.h>
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, qofs_t, time_t mtime, void *, searchpathfuncs_t *), void *parm, searchpathfuncs_t *spath)
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

					if (!func(file, st.st_size, st.st_mtime, parm, spath))
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
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, qofs_t, time_t mtime, void *, void *), void *parm, void *spath)
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

	memset(&parms, 0, sizeof(parms));

	parms.basedir = "./";

	parms.argc = argc;
	parms.argv = (const char**)argv;
#ifdef CONFIG_MANIFEST_TEXT
	parms.manifest = CONFIG_MANIFEST_TEXT;
#endif

#ifndef WIN32
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	COM_InitArgv (parms.argc, parms.argv);

	TL_InitLanguages(parms.basedir);

	Sys_Printf ("Host_Init\n");
	Host_Init (&parms);

	oldtime = Sys_DoubleTime ();

//client console should now be initialized.

    /* main window message loop */
	while (1)
	{
#ifndef CLIENTONLY
		if (isDedicated)
		{
			float delay;
		// find time passed since last cycle
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
			oldtime = newtime;
			
			delay = SV_Frame();
			NET_Sleep(delay, false);
		}
		else
#endif
		{
			double sleeptime;

	// yield the CPU for a little while when paused, minimized, or not the focus
#if SDL_MAJOR_VERSION >= 2
			if (!vid.activeapp)
				SDL_Delay(1);
#else
			if (!(SDL_GetAppState() & SDL_APPINPUTFOCUS))
				SDL_Delay(1);
#endif

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
			sleeptime = Host_Frame (time);
			oldtime = newtime;

			Sys_Sleep(sleeptime);
		}
	}

	return 0;
}

#ifdef _MSC_VER
//our version of sdl_main.lib, which doesn't fight c runtimes.
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int argc;
	int i, l, c;
	LPWSTR *argvw;
	char **argv;
	char utf8arg[1024];
	argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	argv = malloc(argc * sizeof(char*));
	for (i = 0; i < argc; i++)
	{
		for(l = 0, c = 0; argvw[i][l]; l++)
			c += utf8_encode(utf8arg+c, argvw[i][l], sizeof(utf8arg) - c-1);
		utf8arg[c] = 0;
		argv[i] = strdup(utf8arg);
	}
	return main(argc, argv);
}
#endif

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
#if SDL_MAJOR_VERSION >= 2
	SDL_DisplayMode mode;
	if (!SDL_GetDesktopDisplayMode(0, &mode))
	{
		*width = mode.w;
		*height = mode.h;
		*bpp = (SDL_PIXELTYPE(mode.format) == SDL_PIXELTYPE_PACKED32)?32:16;
		*refreshrate = mode.refresh_rate;
		return true;
	}
#endif
	return false;
}



#if SDL_MAJOR_VERSION >= 2	//probably could include 1.3
#include <SDL_clipboard.h>
void Sys_Clipboard_PasteText(clipboardtype_t cbt, void (*callback)(void *cb, char *utf8), void *ctx)
{
	callback(ctx, SDL_GetClipboardText());
}
void Sys_SaveClipboard(clipboardtype_t cbt, char *text)
{
	SDL_SetClipboardText(text);
}
#else
static char *clipboard_buffer;
void Sys_Clipboard_PasteText(clipboardtype_t cbt, void (*callback)(void *cb, char *utf8), void *ctx)
{
	callback(ctx, clipboard_buffer);
}
void Sys_SaveClipboard(clipboardtype_t cbt, char *text)
{
	free(clipboard_buffer);
	clipboard_buffer = strdup(text);
}
#endif

#ifdef MULTITHREAD
/* Thread creation calls */
void *Sys_CreateThread(char *name, int (*func)(void *), void *args, int priority, int stacksize)
{
	// SDL threads do not support setting thread stack size
#if SDL_MAJOR_VERSION >= 2
	return (void *)SDL_CreateThread(func, name, args);
#else
	return (void *)SDL_CreateThread(func, args);
#endif
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


#ifdef HAVEAUTOUPDATE
//legacy, so old build can still deal with updates properly
void Sys_SetUpdatedBinary(const char *fname)
{
}
//says whether the system code is able to invoke new binaries properly
qboolean Sys_EngineCanUpdate(void)
{
	return false;	//nope, nothing here
}
//invoke the given system-path binary
qboolean Sys_EngineWasUpdated(char *newbinary)
{
	return false;	//sorry
}
#endif
