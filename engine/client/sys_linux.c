/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//well, linux or cygwin (windows with posix emulation layer), anyway...


#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <dlfcn.h>
#ifndef __CYGWIN__
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#ifndef __MACOSX__
#include <X11/Xlib.h>
#endif
#ifdef MULTITHREAD
#include <pthread.h>
#endif

#include "quakedef.h"

#undef malloc

int noconinput = 0;
int nostdout = 0;

char *basedir = ".";

qboolean Sys_InitTerminal (void)	//we either have one or we don't.
{
	return true;
}
void Sys_CloseTerminal (void)
{
}


#ifndef CLIENTONLY
qboolean isDedicated;
#endif
// =======================================================================
// General routines
// =======================================================================

void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[2048];
	unsigned char		*p;

	va_start (argptr,fmt);
	_vsnprintf (text,sizeof(text)-1, fmt,argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

	if (nostdout)
		return;

	for (p = (unsigned char *)text; *p; p++)
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
}

void Sys_Quit (void)
{
	Host_Shutdown();
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

void Sys_Init(void)
{
#if id386
	Sys_SetFPCW();
#endif
}

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

// change stdin to non blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	va_start (argptr,error);
	_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown ();
	exit (1);
}

void Sys_Warn (char *warning, ...)
{
	va_list argptr;
	char string[1024];

	va_start (argptr,warning);
	_vsnprintf (string,sizeof(string)-1, warning,argptr);
	va_end (argptr);

	fprintf(stderr, "Warning: %s", string);
}

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int	Sys_FileTime (char *path)
{
	struct	stat	buf;

	if (stat (path,&buf) == -1)
		return -1;

	return buf.st_mtime;
}


void Sys_mkdir (char *path)
{
	mkdir (path, 0777);
}

qboolean Sys_remove (char *path)
{
	return system(va("rm \"%s\"", path));
}

int Sys_FileOpenRead (char *path, int *handle)
{
	int h;
	struct stat fileinfo;

	h = open (path, O_RDONLY, 0666);
	*handle = h;
	if (h == -1)
		return -1;

	if (fstat (h,&fileinfo) == -1)
		Sys_Error ("Error fstating %s", path);

	return fileinfo.st_size;
}

int Sys_FileOpenWrite (char *path)
{
	int handle;

	umask (0);

	handle = open(path,O_RDWR | O_CREAT | O_TRUNC, 0666);

	if (handle == -1)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));

	return handle;
}

int Sys_FileWrite (int handle, void *src, int count)
{
	return write (handle, src, count);
}

void Sys_FileClose (int handle)
{
	close (handle);
}

void Sys_FileSeek (int handle, int position)
{
	lseek (handle, position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return read (handle, dest, count);
}

int Sys_DebugLog(char *file, char *fmt, ...)
{
	va_list argptr;
	static char data[1024];
	int fd;

	va_start(argptr, fmt);
	_vsnprintf (data,sizeof(data)-1, fmt, argptr);
	va_end(argptr);

	if (strlen(data) > sizeof(data))
		Sys_Error("Sys_DebugLog's buffer was stomped\n");

//	fd = open(file, O_WRONLY | O_BINARY | O_CREAT | O_APPEND, 0666);
	fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd)
	{
		write(fd, data, strlen(data));
		close(fd);
		return 0;
	}
	return 1;
}

int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm)
{
#include <dirent.h>
	DIR *dir, *dir2;
	char apath[MAX_OSPATH];
	char file[MAX_OSPATH];
	char truepath[MAX_OSPATH];
	char *s;
	struct dirent *ent;

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
			if (wildcmp(match, ent->d_name))
			{
				Q_snprintfz(file, sizeof(file), "%s/%s", gpath, ent->d_name);
//would use stat, but it breaks on fat32.

				if ((dir2 = opendir(file)))
				{
					closedir(dir2);
					Q_snprintfz(file, sizeof(file), "%s%s/", apath, ent->d_name);
//printf("is directory = %s\n", file);
				}
				else
				{
					Q_snprintfz(file, sizeof(file), "%s%s", apath, ent->d_name);
//printf("file = %s\n", file);
				}

				if (!func(file, -2, parm))
				{
					closedir(dir);
					return false;
				}
			}
	} while(1);
	closedir(dir);

	return true;
}


int secbase;
unsigned int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}
	return (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;
}

double Sys_DoubleTime (void)
{
	struct timeval tp;
	struct timezone tzp;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}

static void *game_library;

void Sys_UnloadGame(void)
{
	if (game_library) 
	{
		dlclose(game_library);
		game_library = 0;
	}
}

void *Sys_GetGameAPI(void *parms)
{
	void *(*GetGameAPI)(void *);

	char name[MAX_OSPATH];
	char curpath[MAX_OSPATH];
	char *searchpath;
	const char *gamename = "gamei386.so";

	void *ret;

	getcwd(curpath, sizeof(curpath));
	
	searchpath = 0;
	while((searchpath = COM_NextPath(searchpath)))
	{
		if (searchpath[0] == '/')
			snprintf(name, sizeof(name), "%s/%s", searchpath, gamename);
		else
			snprintf(name, sizeof(name), "%s/%s/%s", curpath, searchpath, gamename);

		game_library = dlopen (name, RTLD_LAZY );
		if (game_library)
		{
			GetGameAPI = (void *)dlsym (game_library, "GetGameAPI");
			if (GetGameAPI && (ret = GetGameAPI(parms)))
			{
				return ret;
			}

			dlclose(game_library);
			game_library = 0;
		}
	}

	return 0;
}

// =======================================================================
// Sleeps for microseconds
// =======================================================================

static volatile int oktogo;

void alarm_handler(int x)
{
	oktogo=1;
}

char *Sys_ConsoleInput(void)
{
#if 0
	static char text[256];
	int len;

	if (cls.state == ca_dedicated)
	{
		len = read (0, text, sizeof(text));
		if (len < 1)
			return NULL;

		text[len-1] = 0;    // rip off the /n and terminate

		return text;
	}
#endif
	return NULL;
}

#if !id386
void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}
#endif

int main (int c, char **v)
{
	double time, oldtime, newtime;
	quakeparms_t parms;
	int j;

//	static char cwd[1024];

	signal(SIGFPE, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	memset(&parms, 0, sizeof(parms));

	COM_InitArgv(c, v);
	TL_InitLanguages();
	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 16*1024*1024;

	j = COM_CheckParm("-mem");
	if (j && j+1 < com_argc)
		parms.memsize = (int) (Q_atof(com_argv[j+1]) * 1024 * 1024);

	parms.membase = malloc (parms.memsize);

	parms.basedir = basedir;

	noconinput = COM_CheckParm("-noconinput");
	if (!noconinput)
		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	if (COM_CheckParm("-nostdout"))
		nostdout = 1;

	Host_Init(&parms);

	oldtime = Sys_DoubleTime ();
	while (1)
	{
#ifdef __MACOSX__
		//wow, not even windows was this absurd.
#ifdef RGLQUAKE
		if (glcocoaRunLoop())
		{
			oldtime = Sys_DoubleTime ();
			continue;
		}
#endif
#ifdef SWQUAKE
#warning don't worry if this causes linker errors - we don't have a macos sw renderer yet!
		if (swcocoaRunLoop())
		{
			oldtime = Sys_DoubleTime ();
			continue;
		}
#endif
#endif

// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		Host_Frame(time);
		oldtime = newtime;
	}
}


/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{

	int r;
	unsigned long addr;
	int psize = getpagesize();

	addr = (startaddr & ~(psize-1)) - psize;

//	fprintf(stderr, "writable code %lx(%lx)-%lx, length=%lx\n", startaddr,
//			addr, startaddr+length, length);

	r = mprotect((char*)addr, length + startaddr - addr + psize, 7);

	if (r < 0)
    		Sys_Error("Protection change failed\n");

}

//fixme: some sort of taskbar/gnome panel flashing.
void Sys_ServerActivity(void)
{
}

//FIXME: this is hacky. Unlike other OSes where the GUI is part of the OS, X is seperate
//from the OS. This will cause problems with framebuffer-only setups.
qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
#ifdef __MACOSX__
//this about sums up the problem with this function
	return false;
#else

	Display *xtemp;
	int scr;
	
	xtemp = XOpenDisplay(NULL);

	if (!xtemp)
		return false;

	scr = DefaultScreen(xtemp);

	*width = DisplayWidth(xtemp, scr);
	*height = DisplayHeight(xtemp, scr);
	*bpp = DefaultDepth(xtemp, scr);
	*refreshrate = 0;

	XCloseDisplay(xtemp);

	return true;
#endif
}

#define SYS_CLIPBOARD_SIZE		256
static char clipboard_buffer[SYS_CLIPBOARD_SIZE] = {0};

char *Sys_GetClipboard(void) {
	return clipboard_buffer;
}

void Sys_CloseClipboard(char *bf)
{
}

void Sys_SaveClipboard(char *text) {
	Q_strncpyz(clipboard_buffer, text, SYS_CLIPBOARD_SIZE);
}

#ifdef MULTITHREAD
/* Thread creation calls */
typedef void *(*pfunction_t)(void *);

void *Sys_CreateThread(int (*func)(void *), void *args, int stacksize)
{
	pthread_t *thread;
	pthread_attr_t attr;
	
	thread = (pthread_t *)malloc(sizeof(pthread_t));
	if (!thread)
		return NULL;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (stacksize < PTHREAD_STACK_MIN)
		stacksize = PTHREAD_STACK_MIN;
	pthread_attr_setstacksize(&attr, stacksize);
	if (pthread_create(thread, &attr, (pfunction_t)func, args))
	{
		free(thread);
		thread = NULL;
	}
	pthread_attr_destroy(&attr);

	return (void *)thread;
}

void Sys_WaitOnThread(void *thread)
{
	pthread_join((pthread_t *)thread, NULL); 
	free(thread);
}

/* Mutex calls */
void *Sys_CreateMutex()
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	
	if (mutex && !pthread_mutex_init(mutex, NULL))
		return mutex;
	return NULL;
}

qboolean Sys_TryLockMutex(void *mutex)
{
	return !pthread_mutex_trylock(mutex);
}

qboolean Sys_LockMutex(void *mutex)
{
	return !pthread_mutex_lock(mutex);
}

qboolean Sys_UnlockMutex(void *mutex)
{
	return !pthread_mutex_unlock(mutex);
}

void Sys_DestroyMutex(void *mutex)
{
	pthread_mutex_destroy(mutex);
	free(mutex);
}
#endif