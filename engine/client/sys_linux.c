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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#if !defined(__CYGWIN__) && !defined(__DJGPP__)
# include <sys/ipc.h>
# include <sys/shm.h>
#endif
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#ifndef NO_X11
# if !defined(__MACOSX__) && !defined(__DJGPP__) && !defined(NO_X11)
#  include <X11/Xlib.h>
# else
#  define NO_X11
# endif
#endif

#ifdef __CYGWIN__
#define USE_LIBTOOL
#endif
#ifdef USE_LIBTOOL
#include <ltdl.h>
#endif

#include "quakedef.h"

#undef malloc

static int noconinput = 0;
static int nostdout = 0;

int isPlugin;
int sys_parentleft;
int sys_parenttop;
int sys_parentwidth;
int sys_parentheight;
long	sys_parentwindow;
qboolean sys_gracefulexit;

qboolean X11_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate);

qboolean Sys_InitTerminal (void)	//we either have one or we don't.
{
	return true;
}
void Sys_CloseTerminal (void)
{
}

void Sys_RecentServer(char *command, char *target, char *title, char *desc)
{
}

#ifndef CLIENTONLY
qboolean isDedicated;
#endif
// =======================================================================
// General routines
// =======================================================================

#if 1
static int ansiremap[8] = {0, 4, 2, 6, 1, 5, 3, 7};
static void ApplyColour(FILE *out, unsigned int chrflags)
{
	static int oldflags = CON_WHITEMASK;
	int bg, fg;

	if (oldflags == chrflags)
		return;
	oldflags = chrflags;

	fprintf(out, "\e[0;"); // reset

	if (chrflags & CON_BLINKTEXT)
		fprintf(out, "5;"); // set blink

	bg = (chrflags & CON_BGMASK) >> CON_BGSHIFT;
	fg = (chrflags & CON_FGMASK) >> CON_FGSHIFT;

	// don't handle intensive bit for background
	// as terminals differ too much in displaying \e[1;7;3?m
	bg &= 0x7;

	if (chrflags & CON_NONCLEARBG)
	{
		if (fg & 0x8) // intensive bit set for foreground
		{
			fprintf(out, "1;"); // set bold/intensity ansi flag
			fg &= 0x7; // strip intensive bit
		}

		// set foreground and background colors
		fprintf(out, "3%i;4%im", ansiremap[fg], ansiremap[bg]);
	}
	else
	{
		switch(fg)
		{
		//to get around wierd defaults (like a white background) we have these special hacks for colours 0 and 7
		case COLOR_BLACK:
			fprintf(out, "7m"); // set inverse
			break;
		case COLOR_GREY:
			fprintf(out, "1;30m"); // treat as dark grey
			break;
		case COLOR_WHITE:
			fprintf(out, "m"); // set nothing else
			break;
		default:
			if (fg & 0x8) // intensive bit set for foreground
			{
				fprintf(out, "1;"); // set bold/intensity ansi flag
				fg &= 0x7; // strip intensive bit
			}

			fprintf(out, "3%im", ansiremap[fg]); // set foreground
			break;
		}
	}
}

#include <wchar.h>
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[2048];
	conchar_t	ctext[2048];
	conchar_t       *c, *e;
	wchar_t		w;
	unsigned int codeflags, codepoint;
	FILE *out = stdout;

	if (nostdout)
	{
#ifdef _DEBUG
		out = stderr;
#else
		return;
#endif
	}

	va_start (argptr,fmt);
	vsnprintf (text,sizeof(text)-1, fmt,argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

	e = COM_ParseFunString(CON_WHITEMASK, text, ctext, sizeof(ctext), false);

	for (c = ctext; c < e; )
	{
		c = Font_Decode(c, &codeflags, &codepoint);
		if (codeflags & CON_HIDDEN)
			continue;

		if ((codeflags&CON_RICHFORECOLOUR) || (codepoint == '\n' && (codeflags&CON_NONCLEARBG)))
			codeflags = CON_WHITEMASK;	//make sure we don't get annoying backgrounds on other lines.
		ApplyColour(out, codeflags);
		w = codepoint;
		if (w >= 0xe000 && w < 0xe100)
		{
			/*not all quake chars are ascii compatible, so map those control chars to safe ones so we don't mess up anyone's xterm*/
			if ((w & 0x7f) > 0x20)
				putc(w&0x7f, out);
			else if (w & 0x80)
			{
				static char tab[32] = "---#@.@@@@ # >.." "[]0123456789.---";
				putc(tab[w&31], out);
			}
			else
			{
				static char tab[32] = ".####.#### # >.." "[]0123456789.---";
				putc(tab[w&31], out);
			}
		}
		else if (w < ' ' && w != '\t' && w != '\r' && w != '\n')
			putc('?', out);	//don't let anyone print escape codes or other things that could crash an xterm.
		else
		{
			/*putwc doesn't like me. force it in utf8*/
			if (w >= 0x80)
			{
				if (w > 0x800)
				{
					putc(0xe0 | ((w>>12)&0x0f), out);
					putc(0x80 | ((w>>6)&0x3f), out);
				}
				else
					putc(0xc0 | ((w>>6)&0x1f), out);
				putc(0x80 | (w&0x3f), out);
			}
			else
				putc(w, out);
		}
	}

	ApplyColour(out, CON_WHITEMASK);
	fflush(out);
}
#else
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[2048];
	unsigned char		*p;

	va_start (argptr,fmt);
	vsnprintf (text,sizeof(text)-1, fmt,argptr);
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
#endif

void Sys_Quit (void)
{
	Host_Shutdown();
#ifndef __DJGPP__
	if (!noconinput)
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif

#ifdef USE_LIBTOOL
	lt_dlexit();
#endif
	exit(0);
}

static void Sys_Register_File_Associations_f(void)
{
	char xdgbase[MAX_OSPATH];

	if (1)
	{
		const char *e = getenv("XDG_DATA_HOME");
		if (e && *e)
			Q_strncpyz(xdgbase, e, sizeof(xdgbase));
		else
		{
			e = getenv("HOME");
			if (e && *e)
				Q_snprintfz(xdgbase, sizeof(xdgbase), "%s/.local/share", e);
			else
			{
				Con_Printf("homedir not known\n");
				return;
			}
		}
	}
	else
	{
		const char *e = getenv("XDG_DATA_DIRS");
		while (e && *e == ':')
			e++;
		if (e && *e)
		{
			char *c;
			Q_strncpyz(xdgbase, e, sizeof(xdgbase));
			c = strchr(xdgbase, ':');
			if (*c)
				*c = 0;
		}
		else
			Q_strncpyz(xdgbase, "/usr/local/share/", sizeof(xdgbase));
	}

	//we need to create some .desktop file first, so stuff knows how to start us up.
	{
		char iconsyspath[MAX_OSPATH];
		char *exe = realpath(host_parms.argv[0], NULL);
		char *basedir = realpath(com_gamepath, NULL);
		const char *iconname = fs_manifest->installation;
		const char *desktopfile = 
			"[Desktop Entry]\n"
			"Type=Application\n"
			"Encoding=UTF-8\n"
			"Name=%s\n"
			"Comment=Awesome First Person Shooter\n"	//again should be a manicfest item
			"Exec=\"%s\" %%u\n"	//FIXME: FS_GetManifestArgs! etc!
			"Path=%s\n"
			"Icon=%s\n"
			"Terminal=false\n"
			"Categories=Game;\n"
			"MimeType=" "application/x-quakeworlddemo;" "x-scheme-handler/qw;\n"
			;
		if (!strcmp(iconname, "afterquake") || !strcmp(iconname, "nq"))	//hacks so that we don't need to create icons.
			iconname = "quake";

		if (FS_NativePath("icon.png", FS_GAME, iconsyspath, sizeof(iconsyspath)))
			iconname = iconsyspath;

		desktopfile = va(desktopfile,
					fs_manifest->formalname?fs_manifest->formalname:fs_manifest->installation,
					exe, basedir, iconname);
		free(exe);
		free(basedir);
		FS_WriteFile(va("%s/applications/fte-%s.desktop", xdgbase, fs_manifest->installation), desktopfile, strlen(desktopfile), FS_SYSTEM);

		//FIXME: read icon.png and write it to ~/.local/share/icons/hicolor/WxH/apps/foo.png
	}

	//we need to set some default applications.
	//write out a new file and rename the new over the top of the old
	{
		char *foundassoc = NULL;
		vfsfile_t *out = FS_OpenVFS(va("%s/applications/.mimeapps.list.new", xdgbase), "wb", FS_SYSTEM);
		if (out)
		{
			qofs_t insize;
			char *in = FS_MallocFile(va("%s/applications/mimeapps.list", xdgbase), FS_SYSTEM, &insize);
			if (in)
			{
				qboolean inadded = false;
				char *l = in;
				while(*l)
				{
					char *le;
					while(*l == ' ' || *l == '\n')
						l++;
					le = strchr(l, '\n');
					if (le)
						le = le+1;
					else
						le = l + strlen(l);
					if (!strncmp(l, "[Added Associations]", 20))
					{
						inadded = true;
						if (!foundassoc)
							foundassoc = le;
					}
					else if (!strncmp(l, "[", 1))
						inadded = false;
					else if (inadded && !strncmp(l, "x-scheme-handler/qw=", 20))
					{
						foundassoc = l;
						insize -= strlen(le);
						memmove(l, le, strlen(le));	//remove the line
					}
					l = le;
				}
				if (foundassoc)
				{	//if we found it, or somewhere to insert it, then insert it.
					VFS_WRITE(out, in, foundassoc-in);
					VFS_PRINTF(out, "x-scheme-handler/qw=fte-%s.desktop\n", fs_manifest->installation);
					VFS_WRITE(out, foundassoc, insize - (foundassoc-in));
				}
				else
					VFS_WRITE(out, in, insize);	//not found, just write everything as-is
				Z_Free(in);
			}
			if (!foundassoc)
			{	//if file not found, or no appropriate section, just concat it on the end.
				VFS_PRINTF(out, "[Added Associations]\n");
				VFS_PRINTF(out, "x-scheme-handler/qw=fte-%s.desktop\n", fs_manifest->installation);
			}
			VFS_FLUSH(out);
			VFS_CLOSE(out);
			FS_Rename2(va("%s/applications/.mimeapps.list.new", xdgbase), va("%s/applications/mimeapps.list", xdgbase), FS_SYSTEM, FS_SYSTEM);
		}
	}
}

void Sys_Init(void)
{
	Cmd_AddCommandD("sys_register_file_associations", Sys_Register_File_Associations_f, "Register FTE as the default handler for various file+protocol types, using FreeDesktop standards.\n");
}
void Sys_Shutdown(void)
{
}

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

#ifndef __DJGPP__
// change stdin back to blocking, so the shell doesn't bug out.
	if (!noconinput)
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif

	va_start (argptr,error);
	vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	COM_WorkerAbort(string);

	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown ();

#ifdef USE_LIBTOOL
	lt_dlexit();
#endif

	fflush(stdout);
	fflush(stderr);

	if (!isatty(STDERR_FILENO))
	{	//if we're a graphical x11 program then its quite likely that we have no stdout with the user never knowing why the game just disappeared
		//the crash could have come from malloc failures, this means we can't really depend upon xlib
		//but we can start some other program to display the message.
		execl("/usr/bin/xmessage", "xmessage", string, NULL);
	}
	exit (1);
}

void Sys_Warn (char *warning, ...)
{
	va_list argptr;
	char string[1024];

	va_start (argptr,warning);
	vsnprintf (string,sizeof(string)-1, warning,argptr);
	va_end (argptr);

	fprintf(stderr, "Warning: %s", string);
}

void Sys_mkdir (const char *path)
{
	mkdir (path, 0760);
}
qboolean Sys_rmdir (const char *path)
{
	if (rmdir (path) == 0)
		return true;
	if (errno == ENOENT)
		return true;
	return false;
}
qboolean Sys_remove (const char *path)
{
	//remove is part of c89.
	if (remove(path) == -1)
		return false;
	return true;
}
qboolean Sys_Rename (const char *oldfname, const char *newfname)
{
	return !rename(oldfname, newfname);
}
#if _POSIX_C_SOURCE >= 200112L
	#include <sys/statvfs.h>
#endif
qboolean Sys_GetFreeDiskSpace(const char *path, quint64_t *freespace)
{
#if _POSIX_C_SOURCE >= 200112L
	//posix 2001
	struct statvfs inf;
	if(0==statvfs(path, &inf))
	{
		*freespace = inf.f_bsize*(quint64_t)inf.f_bavail;	//grab the quota-free value rather than the actual free space
		return true;
	}
#endif
	return false;
}

int Sys_DebugLog(char *file, char *fmt, ...)
{
	va_list argptr;
	static char data[1024];
	int fd;
	size_t result;

	va_start(argptr, fmt);
	vsnprintf (data,sizeof(data)-1, fmt, argptr);
	va_end(argptr);

	if (strlen(data) > sizeof(data))
		Sys_Error("Sys_DebugLog's buffer was stomped\n");

//	fd = open(file, O_WRONLY | O_BINARY | O_CREAT | O_APPEND, 0666);
	fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd)
	{
		result = write(fd, data, strlen(data)); // do something with result

		if (result != strlen(data))
			Con_SafePrintf("Sys_DebugLog() write: Filename: %s, expected %lu, result was %lu (%s)\n",file,(unsigned long)strlen(data),(unsigned long)result,strerror(errno));

		close(fd);
		return 0;
	}
	return 1;
}

static int Sys_EnumerateFiles2 (const char *truepath, int apathofs, const char *match, int (*func)(const char *, qofs_t, time_t modtime, void *, searchpathfuncs_t *), void *parm, searchpathfuncs_t *spath)
{
	DIR *dir;
	char file[MAX_OSPATH];
	const char *s;
	struct dirent *ent;
	struct stat st;
	const char *wild;
	const char *apath = truepath+apathofs;

	//if there's a * in a system path, then we need to scan its parent directory to figure out what the * expands to.
	//we can just recurse quicklyish to try to handle it.
	wild = strchr(apath, '*');
	if (!wild)
		wild = strchr(apath, '?');
	if (wild)
	{
		char subdir[MAX_OSPATH];
		for (s = wild+1; *s && *s != '/'; s++)
			;
		while (wild > truepath)
		{
			if (*(wild-1) == '/')
				break;
			wild--;
		}
		memcpy(file, truepath, wild-truepath);
		file[wild-truepath] = 0;

		dir = opendir(file);
		memcpy(subdir, wild, s-wild);
		subdir[s-wild] = 0;
		if (dir)
		{
			do
			{
				ent = readdir(dir);
				if (!ent)
					break;
				if (*ent->d_name != '.')
				{
#ifdef _DIRENT_HAVE_D_TYPE
					if (ent->d_type != DT_DIR && ent->d_type != DT_UNKNOWN)
						continue;
#endif
					if (wildcmp(subdir, ent->d_name))
					{
						memcpy(file, truepath, wild-truepath);
						Q_snprintfz(file+(wild-truepath), sizeof(file)-(wild-truepath), "%s%s", ent->d_name, s);
						if (!Sys_EnumerateFiles2(file, apathofs, match, func, parm, spath))
						{
							closedir(dir);
							return false;
						}
					}
				}
			} while(1);
			closedir(dir);
		}
		return true;
	}


	dir = opendir(truepath);
	if (!dir)
	{
		Con_DLPrintf((errno==ENOENT)?2:1, "Failed to open dir %s\n", truepath);
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
//						Con_DPrintf("giving up on search after finding %s\n", file);
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
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, qofs_t, time_t modtime, void *, searchpathfuncs_t *), void *parm, searchpathfuncs_t *spath)
{
	char apath[MAX_OSPATH];
	char truepath[MAX_OSPATH];
	char *s;

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
	return Sys_EnumerateFiles2(truepath, strlen(gpath)+1, match, func, parm, spath);
}

int secbase;

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

unsigned int Sys_Milliseconds (void)
{
	return Sys_DoubleTime() * 1000;
}

#ifdef USE_LIBTOOL
void Sys_CloseLibrary(dllhandle_t *lib)
{
	lt_dlclose((void*)lib);
}

dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	int i;
	dllhandle_t *lib;

	lib = NULL;
	if (!lib)
		lib = lt_dlopenext (name);
	if (!lib)
	{
		Con_DLPrintf(2, "%s: %s\n", name, lt_dlerror());
		return NULL;
	}

	if (funcs)
	{
		for (i = 0; funcs[i].name; i++)
		{
			*funcs[i].funcptr = lt_dlsym(lib, funcs[i].name);
			if (!*funcs[i].funcptr)
				break;
		}
		if (funcs[i].name)
		{
			Con_DPrintf("Unable to find symbol \"%s\" in \"%s\"\n", funcs[i].name, name);
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
	return lt_dlsym((void*)module, exportname);
}
#else
void Sys_CloseLibrary(dllhandle_t *lib)
{
	dlclose((void*)lib);
}

dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	int i;
	dllhandle_t *lib;

	lib = NULL;
	if (!lib)
		lib = dlopen (name, RTLD_LOCAL|RTLD_LAZY);
	if (!lib && !strstr(name, ".so"))
		lib = dlopen (va("%s.so", name), RTLD_LOCAL|RTLD_LAZY);
	if (!lib)
	{
		Con_DLPrintf(2,"%s\n", dlerror());
		return NULL;
	}

	if (funcs)
	{
		for (i = 0; funcs[i].name; i++)
		{
			*funcs[i].funcptr = dlsym(lib, funcs[i].name);
			if (!*funcs[i].funcptr)
				break;
		}
		if (funcs[i].name)
		{
			Con_DPrintf("Unable to find symbol \"%s\" in \"%s\"\n", funcs[i].name, name);
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
	return dlsym(module, exportname);
}
#endif

// =======================================================================
//friendly way to crash, including stack traces. should help reduce gdb use.
#if defined(__linux__) && defined(__GNUC__) /*should probably be GNUC but whatever*/
#include <execinfo.h>
#ifdef __i386__
#include <ucontext.h>
#endif
#ifdef DEBUG
void DumpGLState(void);
#endif
static void Friendly_Crash_Handler(int sig, siginfo_t *info, void *vcontext)
{
	int fd;
	void *array[64];
	size_t size;
	int firstframe = 0;
	char signame[32];

	switch(sig)
	{
	case SIGINT:    strcpy(signame, "SIGINT");  break;
	case SIGILL:    strcpy(signame, "SIGILL");  break;
	case SIGFPE:    strcpy(signame, "SIGFPE");  break;
	case SIGBUS:    strcpy(signame, "SIGBUS");  break;
	case SIGSEGV:   Q_snprintfz(signame, sizeof(signame), "SIGSEGV (%p)", info->si_addr);   break;
	default:    Q_snprintfz(signame, sizeof(signame), "%i", sig);   break;
	}

	// get void*'s for all entries on the stack
	size = backtrace(array, sizeof(array)/sizeof(array[0]));

#if defined(__i386__)
	//x86 signals don't leave the stack in a clean state, so replace the signal handler with the real crash address, and hide this function
	array[1] = (void*)((ucontext_t*)vcontext)->uc_mcontext.gregs[REG_EIP];
	firstframe = 1;
#elif defined(__amd64__)
	//amd64 is sane enough, but this function and the libc signal handler are on the stack, and should be ignored.
	firstframe = 2;
#endif

	// print out all the frames to stderr
#ifdef SVNREVISION
	fprintf(stderr, "Error: signal %s (revision "STRINGIFY(SVNREVISION)")\n", signame);
#else
	fprintf(stderr, "Error: signal %s:\n", signame);
#endif
	backtrace_symbols_fd(array+firstframe, size-firstframe, 2);

	if (sig == SIGINT)
		fd = -1;	//don't write out crash logs on ctrl+c
	else
		fd = open("crash.log", O_WRONLY|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP);
	if (fd != -1)
	{
		time_t rawtime;
		struct tm * timeinfo;
		char buffer [80];

		time (&rawtime);
		timeinfo = localtime (&rawtime);
		strftime (buffer, sizeof(buffer), "Time: %Y-%m-%d %H:%M:%S\n",timeinfo);
		write(fd, buffer, strlen(buffer));

		Q_snprintfz(buffer, sizeof(buffer), "Ver: %i.%02i%s\n", FTE_VER_MAJOR, FTE_VER_MINOR,
#ifdef OFFICIAL_RELEASE
			" (official)");
#else
			"");
#endif
		write(fd, buffer, strlen(buffer));

#if defined(SVNREVISION) && defined(SVNDATE)
		Q_snprintfz(buffer, sizeof(buffer), "Revision: %s, %s\n", STRINGIFY(SVNREVISION), STRINGIFY(SVNDATE));
#else
		Q_snprintfz(buffer, sizeof(buffer),
		#ifdef SVNREVISION
			"Revision: "STRINGIFY(SVNREVISION)"\n"
		#endif
		"Binary: "__DATE__" "__TIME__"\n");
#endif
		write(fd, buffer, strlen(buffer));

		backtrace_symbols_fd(array + firstframe, size - firstframe, fd);
		write(fd, "\n", 1);
		close(fd);
	}
#if defined(DEBUG) && defined(GLQUAKE)
	if (qrenderer == QR_OPENGL)
		DumpGLState();
#endif
	exit(1);
}
#endif
// =======================================================================
// Sleeps for microseconds
// =======================================================================

char *Sys_ConsoleInput(void)
{
#if 1
	static char text[256];
	char *nl;

#ifdef SUBSERVERS
	if (SSV_IsSubServer())
	{
		SSV_CheckFromMaster();
		return NULL;
	}
#endif

	if (noconinput)
		return NULL;

#if defined(__linux__) && defined(_DEBUG)
	{
		int fl = fcntl (STDIN_FILENO, F_GETFL, 0);
		if (!(fl & FNDELAY))
		{
			fcntl(STDIN_FILENO, F_SETFL, fl | FNDELAY);
//			Sys_Printf(CON_WARNING "stdin flags became blocking - gdb bug?\n");
		}
	}
#endif

//	if (!qrenderer)
	{
		if (!fgets(text, sizeof(text), stdin))
		{
			if (errno == EIO)
			{
				Sys_Printf(CON_WARNING "Backgrounded, ignoring stdin\n");
				noconinput |= 2;
			}
			return NULL;
		}
		nl = strchr(text, '\n');
		if (!nl)	//err? wut?
			return NULL;
		*nl = 0;

//Con_Printf("console input: %s\n", text);

		if (!strncmp(text, "vid_recenter ", 13))
		{
			Cmd_TokenizeString(text, false, false);
			sys_parentleft = strtoul(Cmd_Argv(1), NULL, 0);
			sys_parenttop = strtoul(Cmd_Argv(2), NULL, 0);
			sys_parentwidth = strtoul(Cmd_Argv(3), NULL, 0);
			sys_parentheight = strtoul(Cmd_Argv(4), NULL, 0);
			sys_parentwindow = strtoul(Cmd_Argv(5), NULL, 16);
		}

		return text;
	}
#endif
	return NULL;
}

#ifdef _POSIX_C_SOURCE
static void SigCont(int code)
{
	int fl = fcntl (STDIN_FILENO, F_GETFL, 0);
	if (!(fl & FNDELAY))
		fcntl(STDIN_FILENO, F_SETFL, fl | FNDELAY);
	noconinput &= ~2;
}
#endif
int main (int c, const char **v)
{
	double time, oldtime, newtime;
	quakeparms_t parms;
	int i;

//	char cwd[1024];
	char bindir[1024];

	signal(SIGFPE, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
#ifdef _POSIX_C_SOURCE
	signal(SIGTTIN, SIG_IGN);	//have to ignore this if we want to not lock up when running backgrounded.
	signal(SIGCONT, SigCont);
	signal(SIGCHLD, SIG_IGN);	//mapcluster stuff might leak zombie processes if we don't do this.
#endif


	memset(&parms, 0, sizeof(parms));

	parms.argc = c;
	parms.argv = v;
#ifdef CONFIG_MANIFEST_TEXT
	parms.manifest = CONFIG_MANIFEST_TEXT;
#endif
	COM_InitArgv(parms.argc, parms.argv);

#ifdef USE_LIBTOOL
	lt_dlinit();
#endif

#ifdef __linux__
	{
		uid_t ruid, euid, suid;
		getresuid(&ruid, &euid, &suid);
		if (!ruid || !euid || !suid)
			printf("WARNING: you should NOT be running this as root!\n");
	}
#endif

#if defined(__linux__) && defined(__GNUC__)
	if (!COM_CheckParm("-nodumpstack"))
	{
		struct sigaction act;
		memset(&act, 0, sizeof(act));
		act.sa_sigaction = Friendly_Crash_Handler;
		act.sa_flags = SA_SIGINFO | SA_RESTART;
		sigaction(SIGILL, &act, NULL);
		sigaction(SIGSEGV, &act, NULL);
		sigaction(SIGBUS, &act, NULL);
		sigaction(SIGINT, &act, NULL);
	}
#endif

	if (COM_CheckParm("-qcdebug"))
	{
		isPlugin = 3;
		nostdout = true;	//only spew debugging messages.
	}
	else
	{
		isPlugin = !!COM_CheckParm("-plugin");
		if (isPlugin)
		{
			printf("status Starting up!\n");
			fflush(stdout);
			nostdout = true;
		}
	}

	parms.basedir = realpath(".", NULL);
	memset(bindir, 0, sizeof(bindir));	//readlink does NOT null terminate, apparently.
#ifdef __linux__
	//attempt to figure out where the exe is located
	if (readlink("/proc/self/exe", bindir, sizeof(bindir)-1) > 0)
	{
		*COM_SkipPath(bindir) = 0;
		parms.binarydir = bindir;
	}
/*#elif defined(__bsd__)
	//attempt to figure out where the exe is located
	if (readlink("/proc/self/file", bindir, sizeof(bindir)-1) > 0)
	{
		*COM_SkipPath(bindir) = 0;
		parms.binarydir = bindir;
	}
*/
#endif
	TL_InitLanguages(parms.binarydir);

	if (!isatty(STDIN_FILENO))
		noconinput = !isPlugin;	//don't read the stdin if its probably screwed (running in qtcreator seems to pipe stdout to stdin in an attempt to screw everything up).
	else
		noconinput = COM_CheckParm("-noconinput") || COM_CheckParm("-nostdin");
#ifndef __DJGPP__
	if (!noconinput)
		fcntl(STDIN_FILENO, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

#ifndef CLIENTONLY
#ifdef SUBSERVERS
	if (COM_CheckParm("-clusterslave"))
		isDedicated = nostdout = isClusterSlave = true;
#endif
	if (COM_CheckParm("-dedicated"))
		isDedicated = true;
#endif

	if (COM_CheckParm("-nostdout"))
		nostdout = 1;

	if (parms.binarydir)
		Sys_Printf("Binary is located at \"%s\"\n", bindir);

#ifndef CLIENTONLY
	if (isDedicated)    //compleate denial to switch to anything else - many of the client structures are not initialized.
	{
		float delay;

		SV_Init (&parms);

		delay = SV_Frame();

		while (1)
		{
			if (!isDedicated)
				Sys_Error("Dedicated was cleared");
			NET_Sleep(delay, false);
			delay = SV_Frame();
		}
	}
#endif


	Host_Init(&parms);

	for (i = 1; i < parms.argc; i++)
	{
		if (!parms.argv[i])
			continue;
		if (*parms.argv[i] == '+' || *parms.argv[i] == '-')
			break;
		Host_RunFile(parms.argv[i], strlen(parms.argv[i]), NULL);
	}

	oldtime = Sys_DoubleTime ();
	while (1)
	{
		double sleeptime;

#ifdef __MACOSX__
		//wow, not even windows was this absurd.
#ifdef GLQUAKE
		if (glcocoaRunLoop())
		{
			oldtime = Sys_DoubleTime ();
			continue;
		}
#endif
#endif

// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		if (isDedicated)
		{
			sleeptime = SV_Frame();
			oldtime = newtime;
			NET_Sleep(sleeptime, noconinput?false:true);
		}
		else
		{
			sleeptime = Host_Frame(time);
			oldtime = newtime;

			if (sleeptime)
				Sys_Sleep(sleeptime);
		}
	}
}


/*
================
Sys_MakeCodeWriteable
================
*/
#if 0
void Sys_MakeCodeWriteable (void *startptr, unsigned long length)
{

	int r;
	uintptr_t addr;
	int psize = getpagesize();

	addr = ((uintptr_t)startptr & ~(psize-1)) - psize;

//	fprintf(stderr, "writable code %lx(%lx)-%lx, length=%lx\n", startaddr,
//			addr, startaddr+length, length);

	r = mprotect((char*)addr, length + startaddr - addr + psize, 7);

	if (r < 0)
    		Sys_Error("Protection change failed\n");
}
#endif

//fixme: some sort of taskbar/gnome panel flashing.
void Sys_ServerActivity(void)
{
}

//FIXME: this is hacky. Unlike other OSes where the GUI is part of the OS, X is seperate
//from the OS. This will cause problems with framebuffer-only setups.
qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
#if defined(NO_X11)
//this about sums up the problem with this function
	return false;
#else
	return X11_GetDesktopParameters(width, height, bpp, refreshrate);
/*
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
*/
#endif
}

#if !defined(GLQUAKE) && !defined(VKQUAKE)
#define SYS_CLIPBOARD_SIZE		256
static char clipboard_buffer[SYS_CLIPBOARD_SIZE] = {0};

void Sys_Clipboard_PasteText(clipboardtype_t cbt, void (*callback)(void *cb, char *utf8), void *ctx)
{
	callback(ctx, clipboard_buffer);
}

void Sys_SaveClipboard(clipboardtype_t cbt, char *text) {
	Q_strncpyz(clipboard_buffer, text, SYS_CLIPBOARD_SIZE);
}
#endif

qboolean Sys_RandomBytes(qbyte *string, int len)
{
	qboolean res;
	int fd = open("/dev/urandom", 0);
	res = (read(fd, string, len) == len);
	close(fd);

	return res;
}

#ifdef WEBCLIENT
qboolean Sys_RunInstaller(void)
{
	return false;
}
#endif

