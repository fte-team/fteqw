#include "quakedef.h"

#include <SDL.h>

#ifndef WIN32
#include <fcntl.h>
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

//someone used the 'quit' command
void Sys_Quit (void)
{
	if (VID_ForceUnlockedAndReturnState)
		VID_ForceUnlockedAndReturnState ();

	Host_Shutdown();

	exit (0);
}

//enumerate the files in a directory (of both gpath and match - match may not contain ..)
//calls the callback for each one until the callback returns 0
//SDL provides no file enumeration facilities.
int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm)
{
	return 1;
}

//blink window if possible (it's not)
void Sys_ServerActivity(void)
{
}



//Without these two we cannot run Q2 gamecode.
void Sys_UnloadGame (void)
{
}
void *Sys_GetGameAPI (void *parms)
{
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
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_CDROM | SDL_INIT_NOPARACHUTE);
}




int VARGS Sys_DebugLog(char *file, char *fmt, ...)
{
	FILE *fd;
	va_list argptr; 
	static char data[1024];
    
	va_start(argptr, fmt);
	_vsnprintf(data, sizeof(data)-1, fmt, argptr);
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
int main(int argc, char **argv)
{
	float time, newtime, oldtime;
	quakeparms_t	parms;
	int				t;

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


//client console should now be initialized.

    /* main window message loop */
	while (1)
	{
#ifndef CLIENTONLY
		if (isDedicated)
		{
			NET_Sleep(100, false);

		// find time passed since last cycle
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
			oldtime = newtime;
			
			SV_Frame ();
		}
		else
#endif
		{
	// yield the CPU for a little while when paused, minimized, or not the focus
			NET_Sleep(1, false);

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
			Host_Frame (time);
			oldtime = newtime;
		}
	}
	return 0;
}

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
	return false;
}

void Sys_HighFPPrecision(void)
{
}

void Sys_LowFPPrecision(void)
{
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
