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
#include <signal.h>
#include <sys/types.h>
#include <dlfcn.h>
#include "qwsvdef.h"


#undef malloc

#ifdef NeXT
#include <libc.h>
#endif

#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

// callbacks
void Sys_Linebuffer_Callback (struct cvar_s *var, char *oldvalue);

cvar_t sys_nostdout = SCVAR("sys_nostdout","0");
cvar_t sys_extrasleep = SCVAR("sys_extrasleep","0");
cvar_t sys_maxtic = SCVAR("sys_maxtic", "100");
cvar_t sys_colorconsole = SCVAR("sys_colorconsole", "0");
cvar_t sys_linebuffer = SCVARC("sys_linebuffer", "1", Sys_Linebuffer_Callback);

qboolean	stdin_ready;

// This is for remapping the Q3 color codes to character masks, including ^9
conchar_t q3codemasks[MAXQ3COLOURS] = {
	0x00000000, // 0, black
	0x0c000000, // 1, red
	0x0a000000, // 2, green
	0x0e000000, // 3, yellow
	0x09000000, // 4, blue
	0x0b000000, // 5, cyan
	0x0d000000, // 6, magenta
	0x0f000000, // 7, white
	0x0f100000, // 8, half-alpha white (BX_COLOREDTEXT)
	0x07000000  // 9, "half-intensity" (BX_COLOREDTEXT)
};

struct termios orig, changes;

/*
===============================================================================

				REQUIRED SYS FUNCTIONS

===============================================================================
*/

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


/*
============
Sys_mkdir

============
*/
void Sys_mkdir (char *path)
{
	if (mkdir (path, 0777) != -1)
		return;
//	if (errno != EEXIST)
//		Sys_Error ("mkdir %s: %s",path, strerror(errno));
}

qboolean Sys_remove (char *path)
{
	return system(va("rm \"%s\"", path));
}

#ifdef SHADERS
int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm)
{
#include <dirent.h>
	DIR *dir;
	struct dirent *ent;
	dir = opendir(gpath);
	if (!dir)
	{
		Con_Printf("Failed to open dir %s\n", gpath);
		return true;
	}
	do
	{
		ent = readdir(dir);	//FIXME: no wild card comparisons.
		if (!ent)
			break;
		if (*ent->d_name != '.')
			if (!func(ent->d_name, -2, parm))
			{
				closedir(dir);
				return false;
			}
	} while(1);
	closedir(dir);

	return true;
}
#endif

int Sys_DebugLog(char *file, char *fmt, ...)
{
	va_list argptr;
	char data[1024];
	int fd;

	va_start(argptr, fmt);
	_vsnprintf (data,sizeof(data)-1, fmt, argptr);
	va_end(argptr);

	if (strlen(data) >= sizeof(data)-1)
		Sys_Error("Sys_DebugLog was stomped\n");

	fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd)
	{
		write(fd, data, strlen(data));
		close(fd);
		return 0;
	}
	return 1;
}

/*
================
Sys_Milliseconds
================
*/
unsigned int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}
	return (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;
}

/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}

/*
================
Sys_Error
================
*/
void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);
	printf ("Fatal error: %s\n",string);

	tcsetattr(STDIN_FILENO, TCSADRAIN, &orig);

*(int*)-3 = 0;
	exit (1);
}

int ansiremap[8] = {0, 4, 2, 6, 1, 5, 3, 7};
void ApplyColour(unsigned int chr)
{
	static int oldchar = CON_WHITEMASK;
	int bg, fg;
	chr &= CON_FLAGSMASK;

	if (oldchar == chr)
		return;
	oldchar = chr;

	printf("\e[0;"); // reset

	if (chr & CON_BLINKTEXT)
		printf("5;"); // set blink

	bg = (chr & CON_BGMASK) >> CON_BGSHIFT;
	fg = (chr & CON_FGMASK) >> CON_FGSHIFT;
	
	// don't handle intensive bit for background
	// as terminals differ too much in displaying \e[1;7;3?m
	bg &= 0x7;

	if (chr & CON_NONCLEARBG)
	{
		if (fg & 0x8) // intensive bit set for foreground
		{
			printf("1;"); // set bold/intensity ansi flag
			fg &= 0x7; // strip intensive bit
		}

		// set foreground and background colors
		printf("3%i;4%im", ansiremap[fg], ansiremap[bg]);
	}
	else
	{
		switch(fg)
		{
		//to get around wierd defaults (like a white background) we have these special hacks for colours 0 and 7
		case COLOR_BLACK:
			printf("7m"); // set inverse
			break;
		case COLOR_GREY:
			printf("1;30m"); // treat as dark grey
			break;
		case COLOR_WHITE:
			printf("m"); // set nothing else
			break;
		default:
			if (fg & 0x8) // intensive bit set for foreground
			{
				printf("1;"); // set bold/intensity ansi flag
				fg &= 0x7; // strip intensive bit
			}

			printf("3%im", ansiremap[fg]); // set foreground
			break;
		}
	}
}

#define putch(c) putc(c, stdout);
void Sys_PrintColouredChar(unsigned int chr)
{
	ApplyColour(chr);

	chr = chr & CON_CHARMASK;

	if ((chr > 128 || chr < 32) && chr != 10 && chr != 13 && chr != 9)
		printf("[%02x]", chr);
	else
		chr &= ~0x80;

	putch(chr);
}

/*
================
Sys_Printf
================
*/
#define	MAXPRINTMSG	4096
char	coninput_text[256];
int		coninput_len;
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;	

	if (sys_nostdout.value)
		return;

	if (1)
	{
		char		msg[MAXPRINTMSG];
		unsigned char *t;

		va_start (argptr,fmt);
		vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
		va_end (argptr);

		if (!sys_linebuffer.value)
		{
			int i;
			
			for (i = 0; i < coninput_len; i++)
				putch('\b');
			putch('\b');
			for (i = 0; i < coninput_len; i++)
				putch(' ');
			putch(' ');
			for (i = 0; i < coninput_len; i++)
				putch('\b');
			putch('\b');
		}


		for (t = (unsigned char*)msg; *t; t++)
		{
			if (*t >= 146 && *t < 156)
				*t = *t - 146 + '0';
			if (*t >= 0x12 && *t <= 0x1b)
				*t = *t - 0x12 + '0';
			if (*t == 143)
				*t = '.';
			if (*t == 157 || *t == 158 || *t == 159)
				*t = '-';
			if (*t >= 128)
				*t -= 128;
			if (*t == 16)
				*t = '[';
			if (*t == 17)
				*t = ']';
			if (*t == 0x1c)
				*t = 249;
		}

		if (sys_colorconsole.value)
		{
			int ext = CON_WHITEMASK;
			int extstack[4];
			int extstackdepth = 0;
			unsigned char *str = (unsigned char*)msg;


			while(*str)
			{
				if (*str == '^')
				{
					str++;
					if (*str >= '0' && *str <= '9')
					{
						ext = q3codemasks[*str++-'0'] | (ext&~CON_Q3MASK);	//change colour only.
						continue;
					}
					else if (*str == '&') // extended code
					{
						if (isextendedcode(str[1]) && isextendedcode(str[2]))
						{
							str++; // foreground char
							if (*str == '-') // default for FG
								ext = (COLOR_WHITE << CON_FGSHIFT) | (ext&~CON_FGMASK);
							else if (*str >= 'A')
								ext = ((*str - ('A' - 10)) << CON_FGSHIFT) | (ext&~CON_FGMASK);
							else
								ext = ((*str - '0') << CON_FGSHIFT) | (ext&~CON_FGMASK);
							str++; // background char
							if (*str == '-') // default (clear) for BG
								ext &= ~CON_BGMASK & ~CON_NONCLEARBG;
							else if (*str >= 'A')
								ext = ((*str - ('A' - 10)) << CON_BGSHIFT) | (ext&~CON_BGMASK) | CON_NONCLEARBG;
							else
								ext = ((*str - '0') << CON_BGSHIFT) | (ext&~CON_BGMASK) | CON_NONCLEARBG;
							str++;
							continue;
						}
						Sys_PrintColouredChar('^' | ext);
						// else invalid code
					}
					else if (*str == 'a')
					{
						str++;
						ext ^= CON_2NDCHARSETTEXT;
						continue;
					}
					else if (*str == 'b')
					{
						str++;
						ext ^= CON_BLINKTEXT;
						continue;
					}
					else if (*str == 'h')
					{
						str++;
						ext ^= CON_HALFALPHA;
						continue;
					}
					else if (*str == 's')	//store on stack (it's great for names)
					{
						str++;
						if (extstackdepth < sizeof(extstack)/sizeof(extstack[0]))
						{
							extstack[extstackdepth] = ext;
							extstackdepth++;
						}
						continue;
					}
					else if (*str == 'r')	//restore from stack (it's great for names)
					{
						str++;
						if (extstackdepth)
						{
							extstackdepth--;
							ext = extstack[extstackdepth];
						}
						continue;
					}
					else if (*str == '^')
					{
						Sys_PrintColouredChar('^' | ext);
						str++;
					}
					else
					{
						Sys_PrintColouredChar('^' | ext);
						Sys_PrintColouredChar ((*str++) | ext);
					}
					continue;
				}
				Sys_PrintColouredChar ((*str++) | ext);
			}

			ApplyColour(CON_WHITEMASK);
		}
		else
		{
			for (t = msg; *t; t++)
			{
				*t &= 0x7f;
				if ((*t > 128 || *t < 32) && *t != 10 && *t != 13 && *t != 9)
					printf("[%02x]", *t);
				else
					putc(*t, stdout);
			}
		}

		if (!sys_linebuffer.value)
		{
			if (coninput_len)
				printf("]%s", coninput_text);
			else
				putch(']');
		}
	}
	else
	{
		va_start (argptr,fmt);
		vprintf (fmt,argptr);
		va_end (argptr);
	}

	fflush(stdout);
}



#if 0
/*
================
Sys_Printf
================
*/
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	static char		text[2048];
	unsigned char		*p;

	va_start (argptr,fmt);
	_vsnprintf (text,sizeof(text)-1, fmt,argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

    if (sys_nostdout.value)
        return;

	for (p = (unsigned char *)text; *p; p++) {
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
	}
	fflush(stdout);
}

#endif

/*
================
Sys_Quit
================
*/
void Sys_Quit (void)
{
	tcsetattr(STDIN_FILENO, TCSADRAIN, &orig);
	exit (0);		// appkit isn't running
}

static int do_stdin = 1;

#if 1
char *Sys_LineInputChar(char *line)
{
char c;
	while(*line)
{
c = *line++;
	if (c == '\r' || c == '\n')
	{
		coninput_text[coninput_len] = 0;
		putch ('\n');
		putch (']');
		coninput_len = 0;
		fflush(stdout);
		return coninput_text;
	}
	if (c == 8)
	{
		if (coninput_len)
		{
			putch (c);
			putch (' ');
			putch (c);
			coninput_len--;
			coninput_text[coninput_len] = 0;
		}
		continue;
	}
	if (c == '\t')
	{
		int i;
		char *s = Cmd_CompleteCommand(coninput_text, true, true, 0);
		if(s)
		{
			for (i = 0; i < coninput_len; i++)
				putch('\b');
			for (i = 0; i < coninput_len; i++)
				putch(' ');
			for (i = 0; i < coninput_len; i++)
				putch('\b');

			strcpy(coninput_text, s);
			coninput_len = strlen(coninput_text);
			printf("%s", coninput_text);
		}
		continue;
	}
	putch (c);
	coninput_text[coninput_len] = c;
	coninput_len++;
	coninput_text[coninput_len] = 0;
	if (coninput_len == sizeof(coninput_text))
		coninput_len = 0;
}
	fflush(stdout);
return NULL;
}
#endif
/*
================
Sys_ConsoleInput

Checks for a complete line of text typed in at the console, then forwards
it to the host command processor
================
*/
void Sys_Linebuffer_Callback (struct cvar_s *var, char *oldvalue)
{
	changes = orig;
	if (var->value)
	{
		changes.c_lflag |= (ICANON|ECHO);
	}
	else
	{
		changes.c_lflag &= ~(ICANON|ECHO);
		changes.c_cc[VTIME] = 0;
		changes.c_cc[VMIN] = 1;
	}
	tcsetattr(STDIN_FILENO, TCSADRAIN, &changes);
}

char *Sys_ConsoleInput (void)
{
	static char	text[256];
	int	len;

	if (!stdin_ready || !do_stdin)
		return NULL;		// the select didn't say it was ready
	stdin_ready = false;

	if (sys_linebuffer.value == 0)
	{
		text[0] = getc(stdin);
		text[1] = 0;
		len = 1;
		return Sys_LineInputChar(text);
	}
	else
	{
		len = read (0, text, sizeof(text)-1);
		if (len == 0) {
			// end of file
			do_stdin = 0;
			return NULL;
		}
		if (len < 1)
			return NULL;
		text[len-1] = 0;	// rip off the /n and terminate

		return text;
	}
}

/*
=============
Sys_Init

Quake calls this so the system can register variables before host_hunklevel
is marked
=============
*/
void Sys_Init (void)
{
	Cvar_Register (&sys_nostdout, "System configuration");
	Cvar_Register (&sys_extrasleep,	"System configuration");
	Cvar_Register (&sys_maxtic, "System configuration");

	Cvar_Register (&sys_colorconsole, "System configuration");
	Cvar_Register (&sys_linebuffer, "System configuration");
}

/*
=============
main
=============
*/
int main(int argc, char *argv[])
{
	quakeparms_t	parms;
//	fd_set	fdset;
//	extern	int		net_socket;
	int j;

	signal(SIGPIPE, SIG_IGN);
	tcgetattr(STDIN_FILENO, &orig);
	changes = orig;

	memset (&parms, 0, sizeof(parms));

	COM_InitArgv (argc, argv);
	TL_InitLanguages();
	parms.argc = com_argc;
	parms.argv = com_argv;


	parms.memsize = 16*1024*1024;

	j = COM_CheckParm("-mem");
	if (j)
		parms.memsize = (int) (Q_atof(com_argv[j+1]) * 1024 * 1024);
	if ((parms.membase = malloc (parms.memsize)) == NULL)
		Sys_Error("Can't allocate %ld\n", parms.memsize);

	parms.basedir = ".";

	SV_Init (&parms);

// run one frame immediately for first heartbeat
	SV_Frame ();

//
// main loop
//
	while (1)
	{
		if (do_stdin)
			stdin_ready = NET_Sleep(sys_maxtic.value, true);
		else
		{
			NET_Sleep(sys_maxtic.value, false);
			stdin_ready = false;
		}

		SV_Frame ();

	// extrasleep is just a way to generate a fucked up connection on purpose
		if (sys_extrasleep.value)
			usleep (sys_extrasleep.value);
	}
	return 0;
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
		Con_Printf("Failed to open dir %s\n", truepath);
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
		sprintf (name, "%s/%s/%s", curpath, searchpath, gamename);
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

void Sys_ServerActivity(void)
{
}

