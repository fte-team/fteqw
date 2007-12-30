/*
Copyright (C) 2006-2007 Mark Olsen

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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <dlfcn.h>

#include "quakedef.h"

#warning Find a better stack size

int __stack = 4*1024*1024;

#if I_AM_BIGFOOT
struct Library *DynLoadBase;
#endif

extern struct Library *VorbisFileBase;

#ifndef CLIENTONLY
qboolean isDedicated;
#endif

static void Sys_Shutdown()
{
#if I_AM_BIGFOOT
	if(DynLoadBase)
	{
		CloseLibrary(DynLoadBase);
		DynLoadBase = 0;
	}
#endif

	if (VorbisFileBase)
	{
		CloseLibrary(VorbisFileBase);
		VorbisFileBase = 0;
	}
}

void Sys_Quit (void)
{
	Host_Shutdown();

	Sys_Shutdown();

	exit(0);
}

static void ftevprintf(const char *fmt, va_list arg)
{
	char buf[4096];
	unsigned char *p;

	vsnprintf(buf, sizeof(buf), fmt, arg);

	for (p = (unsigned char *)buf; *p; p++)
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
}

void Sys_Printf(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	ftevprintf(fmt, arg);
	va_end(arg);
}

void Sys_Error(const char *error, ...)
{
	va_list arg;

	printf("Error: ");
	va_start(arg, error);
	ftevprintf(error, arg);
	va_end(arg);

	Host_Shutdown ();
	exit (1);
}

void Sys_Warn(char *warning, ...)
{
	va_list arg;

	printf("Warning: ");
	va_start(arg, warning);
	ftevprintf(warning, arg);
	va_end(arg);
}

int Sys_DebugLog(char *file, char *fmt, ...)
{
	va_list arg;
	char buf[4096];
	BPTR fh;

	fh = Open(file, MODE_READWRITE);
	if (fh)
	{
		Seek(fh, OFFSET_END, 0);

		va_start(arg, fmt);
		vsnprintf(buf, sizeof(buf), fmt, arg);
		va_end(arg);

		Write(fh, buf, strlen(buf));

		Close(fh);

		return 0;
	}

	return 1;
}

int secbase;
unsigned int Sys_Milliseconds(void)
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

double Sys_DoubleTime(void)
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

/* FS stuff */

int Sys_FileTime(char *path)
{
	BPTR lock;
	struct FileInfoBlock fib;
	int ret = -1;

	if (path[0] == '.' && path[1] == '/')
		path+= 2;

	lock = Lock(path, ACCESS_READ);
	if (lock)
	{
		if (Examine(lock, &fib))
		{
			ret = ((fib.fib_Date.ds_Days+2922)*1440+fib.fib_Date.ds_Minute)*60+fib.fib_Date.ds_Tick/TICKS_PER_SECOND;
		}

		UnLock(lock);
	}

	return ret;
}

int Sys_EnumerateFiles(char *gpath, char *match, int (*func)(char *, int, void *), void *parm)
{
	char *pattern;
	char pattrans[256];
	char finddir[256];
	char findpattern[514];
	char filename[256];
	int i, j;
	BPTR lock;
	struct FileInfoBlock fib;
	int ret = false;

	snprintf(finddir, sizeof(finddir), "%s/%s", gpath, match);

	pattern = strrchr(finddir, '/');
	if (pattern)
	{
		finddir[((unsigned int)(pattern-finddir))] = 0;
		pattern++;
	}

	for(i=0,j=0;i<sizeof(pattrans)-1 && *pattern;i++,j++)
	{
		if (pattern[j] == '*')
		{
			if (i < sizeof(pattrans)-2)
			{
				pattrans[i] = '#';
				pattrans[i+1] = '?';
				i++;
			}
			else
				pattrans[i] = 0;
		}
		else
			pattrans[i] = pattern[j];
	}

	pattrans[i] = 0;

	lock = Lock(finddir, ACCESS_READ);
	if (lock)
	{
		if (Examine(lock, &fib))
		{
			if (ParsePatternNoCase(pattrans, findpattern, sizeof(findpattern)) >= 0)
			{
				ret = true;

				while(ExNext(lock, &fib))
				{
					if (MatchPatternNoCase(findpattern, fib.fib_FileName))
					{
						snprintf(filename, sizeof(filename), "%s%s", fib.fib_FileName, fib.fib_DirEntryType>=0?"/":"");
						if (func(filename, fib.fib_Size, parm) == 0)
						{
							ret = false;
							break;
						}
					}
				}
			}
		}

		UnLock(lock);
	}

	return ret;
}

void Sys_mkdir(char *path)
{
	BPTR lock;

	if (path[0] == '.' && path[1] == '/')
		path+= 2;

	lock = CreateDir(path);
	if (lock)
	{
		UnLock(lock);
	}
}

qboolean Sys_remove(char *path)
{
	if (path[0] == '.' && path[1] == '/')
		path+= 2;

	return DeleteFile(path);
}

/* Quake 2 stuff */
static void *gamefile;

void *Sys_GetGameAPI(void *parms)
{
	int (*q2_so_init)(void);
	void (*q2_so_deinit)(void);
	void *(*GetGameAPI)(void *);
	void *ret;
	char *searchpath;
	char path[256];

	searchpath = 0;

	while((searchpath = COM_NextPath(searchpath)))
	{

		snprintf(path, sizeof(path), "%s%sgameppc.so", searchpath[0]!='.'?searchpath:"", searchpath[0]&&searchpath[0]!='.'?"/":"");

		gamefile = dlopen(path, RTLD_NOW);
		if (gamefile)
		{
			q2_so_init = dlsym(gamefile, "q2_so_init");
			q2_so_deinit = dlsym(gamefile, "q2_so_deinit");
			if (q2_so_init && q2_so_init())
			{
				GetGameAPI = dlsym(gamefile, "GetGameAPI");
				if (GetGameAPI && (ret = GetGameAPI(parms)))
				{
					return ret;
				}

				if (q2_so_deinit)
					q2_so_deinit();
			}
			dlclose(gamefile);
			gamefile = 0;
		}
	}

	return 0;
}

void Sys_UnloadGame(void)
{
	void (*q2_so_deinit)(void);

	if (gamefile)
	{
		q2_so_deinit = dlsym(gamefile, "q2_so_deinit");
		if (q2_so_deinit)
			q2_so_deinit();

		dlclose(gamefile);
		gamefile = 0;
	}
}

int main(int argc, char **argv)
{
	double oldtime, newtime;
	quakeparms_t parms;
	int i;

	COM_InitArgv(argc, argv);
	TL_InitLanguages();

	i = COM_CheckParm("-mem");
	if (i && i < com_argc)
		parms.memsize = atoi(com_argv[i+1])*1024*1024;
	else
		parms.memsize = 16*1024*1024;

	parms.basedir = "";
	parms.argc = argc;
	parms.argv = argv;
	parms.membase = malloc(parms.memsize);

	if (parms.membase == 0)
		Sys_Error("Can't allocated %d bytes\n", parms.memsize);

#if I_AM_BIGFOOT
	DynLoadBase = OpenLibrary("dynload.library", 0);
#endif

	Host_Init(&parms);

	oldtime = Sys_DoubleTime ();
	while(!(SetSignal(0, 0)&SIGBREAKF_CTRL_C))
	{
		newtime = Sys_DoubleTime ();
		Host_Frame(newtime - oldtime);
		oldtime = newtime;
	}
}

void Sys_Init()
{
}

char *Sys_GetClipboard(void)
{
	return 0;
}

void Sys_CloseClipboard(char *buf)
{
}

void Sys_SaveClipboard(char *text)
{
}

qboolean Sys_InitTerminal()
{
	return false;
}
void Sys_CloseTerminal()
{
}

char *Sys_ConsoleInput()
{
	return 0;
}

void Sys_ServerActivity(void)
{
}

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
	return false;
}

/* x86 crap */
void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

#ifdef MULTITHREAD
/* Everything here is stubbed because I don't know MorphOS */
/* Thread creation calls */
void *Sys_CreateThread(int (*func)(void *), void *args, int stacksize) { return NULL; }
void Sys_WaitOnThread(void *thread) {}
/* Mutex calls */
void *Sys_CreateMutex() { return NULL; }
qboolean Sys_TryLockMutex(void *mutex) { return FALSE; }
qboolean Sys_LockMutex(void *mutex) { return FALSE; }
qboolean Sys_UnlockMutex(void *mutex) { return FALSE; }
void Sys_DestroyMutex(void *mutex) {}
#endif
