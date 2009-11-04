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
// sys_win.h

#include "quakedef.h"

#include "winquake.h"
#include "resource.h"
#include "errno.h"
#include "fcntl.h"
#include <limits.h>
#include <conio.h>
#include <io.h>
#include <direct.h>

#ifdef MULTITHREAD
#include <process.h>
#endif

#ifdef _DEBUG
#define CATCHCRASH
#endif

#if !defined(CLIENTONLY) && !defined(SERVERONLY)
qboolean isDedicated = false;
#endif

HWND sys_parentwindow;
unsigned int sys_parentwidth;	//valid if sys_parentwindow is set
unsigned int sys_parentheight;

void Sys_CloseLibrary(dllhandle_t *lib)
{
	FreeLibrary((HMODULE)lib);
}
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	int i;
	HMODULE lib;

	lib = LoadLibrary(name);
	if (!lib)
		return NULL;

	if (funcs)
	{
		for (i = 0; funcs[i].name; i++)
		{
			*funcs[i].funcptr = GetProcAddress(lib, funcs[i].name);
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
	return GetProcAddress((HINSTANCE)module, exportname);
}
#ifdef HLSERVER
char *Sys_GetNameForAddress(dllhandle_t *module, void *address)
{
	//windows doesn't provide a function to do this, so we have to do it ourselves.
	//this isn't the fastest way...
	//halflife needs this function.
	char *base = (char *)module;

	IMAGE_DATA_DIRECTORY *datadir;
	IMAGE_EXPORT_DIRECTORY *block;
	IMAGE_NT_HEADERS *ntheader;
	IMAGE_DOS_HEADER *dosheader = (void*)base;

	int i, j;
	DWORD *funclist;
	DWORD *namelist;
	SHORT *ordilist;

	if (!dosheader || dosheader->e_magic != IMAGE_DOS_SIGNATURE)
		return NULL; //yeah, that wasn't an exe

	ntheader = (void*)(base + dosheader->e_lfanew);
	if (!dosheader->e_lfanew || ntheader->Signature != IMAGE_NT_SIGNATURE)
		return NULL;	//urm, wait, a 16bit dos exe?


	datadir = &ntheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	
	block = (IMAGE_EXPORT_DIRECTORY *)(base + datadir->VirtualAddress);
	funclist = (DWORD*)(base+block->AddressOfFunctions);
	namelist = (DWORD*)(base+block->AddressOfNames);
	ordilist = (SHORT*)(base+block->AddressOfNameOrdinals);
	for (i = 0; i < block->NumberOfFunctions; i++)
	{
		if (base+funclist[i] == address)
		{
			for (j = 0; j < block->NumberOfNames; j++)
			{
				if (ordilist[j] == i)
				{
					return base+namelist[i];
				}
			}
			//it has no name. huh?
			return NULL;
		}
	}
	return NULL;
}
#endif

#ifdef Q2SERVER
static HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!FreeLibrary (game_library))
		Sys_Error ("FreeLibrary failed for game library");
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void	*(VARGS *GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];
#if defined _M_IX86
	const char *gamename = "gamex86.dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#elif defined(__amd64__) || defined(__AMD64__) || defined(_AMD64_)
	const char *gamename = "gameamd.dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#elif defined _M_ALPHA
	const char *gamename = "gameaxp.dll";

#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif

#endif

	if (game_library)
		Sys_Error ("Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current debug directory first for development purposes
#ifdef _WIN32
	GetCurrentDirectory(sizeof(cwd), cwd);
#else
	_getcwd (cwd, sizeof(cwd));
#endif
	snprintf (name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
	game_library = LoadLibrary ( name );
	if (game_library)
	{
		Con_DPrintf ("LoadLibrary (%s)\n", name);
	}
	else
	{
#ifdef DEBUG
		// check the current directory for other development purposes
		_snprintf (name, sizeof(name), "%s/%s", cwd, gamename);
		game_library = LoadLibrary ( name );
		if (game_library)
		{
			Con_DPrintf ("LoadLibrary (%s)\n", name);
		}
		else
#endif
		{
			// now run through the search paths
			path = NULL;
			while (1)
			{
				path = COM_NextPath (path);
				if (!path)
					return NULL;		// couldn't find one anywhere
				snprintf (name, sizeof(name), "%s/%s", path, gamename);
				game_library = LoadLibrary (name);
				if (game_library)
				{
					Con_DPrintf ("LoadLibrary (%s)\n",name);
					break;
				}
			}
		}
	}

	GetGameAPI = (void *)GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();		
		return NULL;
	}

	return GetGameAPI (parms);
}
#endif


#define MINIMUM_WIN_MEMORY	0x0800000
#define MAXIMUM_WIN_MEMORY	0x4000000

#define PAUSE_SLEEP		50				// sleep time on pause or minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

int		starttime;
qboolean ActiveApp, Minimized;
qboolean	WinNT;

HWND	hwnd_dialog;		// startup dialog box

static HANDLE		hinput, houtput;

HANDLE		qwclsemaphore;

static HANDLE	tevent;

void Sys_InitFloatTime (void);

void VARGS MaskExceptions (void);
void Sys_PopFPCW (void);
void Sys_PushFPCW_SetHigh (void);

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
	fd = fopen(file, "ab");
	if (fd)
	{
		fprintf(fd, "%s", data);
		fclose(fd);
		return 0;
	}

	return 1;
};

#ifdef CATCHCRASH
#include "dbghelp.h"
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP) (
	HANDLE hProcess,
	DWORD ProcessId,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam
	);

static DWORD CrashExceptionHandler (DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo)
{
	char dumpPath[1024];
	HANDLE hProc = GetCurrentProcess();
	DWORD procid = GetCurrentProcessId();
	HANDLE dumpfile;
	HMODULE hDbgHelp;
	MINIDUMPWRITEDUMP fnMiniDumpWriteDump;
	HMODULE hKernel;
	BOOL (WINAPI *pIsDebuggerPresent)(void);

	hKernel = LoadLibrary ("kernel32");
	pIsDebuggerPresent = (void*)GetProcAddress(hKernel, "IsDebuggerPresent");

#ifdef GLQUAKE
	GLVID_Crashed();
#endif

	if (pIsDebuggerPresent ())
	{
		/*if we have a current window, minimize it to bring us out of fullscreen*/
		ShowWindow(mainwindow, SW_MINIMIZE);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	/*if we have a current window, kill it, so it can't steal input of handle window messages or anything risky like that*/
	DestroyWindow(mainwindow);

	hDbgHelp = LoadLibrary ("DBGHELP");
	if (hDbgHelp)
		fnMiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress (hDbgHelp, "MiniDumpWriteDump");
	else
		fnMiniDumpWriteDump = NULL;

	if (fnMiniDumpWriteDump)
	{
		if (MessageBox(NULL, "KABOOM! We crashed!\nBlame the monkey in the corner.\nI hope you saved your work.\nWould you like to take a dump now?", DISTRIBUTION " Sucks", MB_ICONSTOP|MB_YESNO) != IDYES)
			return EXCEPTION_EXECUTE_HANDLER;

		/*take a dump*/
		GetTempPath (sizeof(dumpPath)-16, dumpPath);
		Q_strncatz(dumpPath, DISTRIBUTION"CrashDump.dmp", sizeof(dumpPath));
		dumpfile = CreateFile (dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (dumpfile)
		{
			MINIDUMP_EXCEPTION_INFORMATION crashinfo;
			crashinfo.ClientPointers = TRUE;
			crashinfo.ExceptionPointers = exceptionInfo;
			crashinfo.ThreadId = GetCurrentThreadId ();
			if (fnMiniDumpWriteDump(hProc, procid, dumpfile, MiniDumpWithIndirectlyReferencedMemory|MiniDumpWithDataSegs, &crashinfo, NULL, NULL))
			{
				CloseHandle(dumpfile);
				MessageBox(NULL, va("You can find the crashdump at\n%s\nPlease send this file to someone.\n\nWarning: sensitive information (like your current user name) might be present in the dump.\nYou will probably want to compress it.", dumpPath), DISTRIBUTION " Sucks", 0);
				return EXCEPTION_EXECUTE_HANDLER;
			}
		}
	}
	MessageBox(NULL, "Kaboom! Sorry. Blame the nubs.", DISTRIBUTION " Sucks", 0);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int *debug;


#ifndef SERVERONLY

#if (_WIN32_WINNT < 0x0400)
	#define LLKHF_ALTDOWN        0x00000020
	#define LLKHF_UP             0x00000080
	#define WH_KEYBOARD_LL     13
	typedef struct {
		DWORD vkCode;
		DWORD scanCode;
		DWORD flags;
		DWORD time;
		DWORD dwExtraInfo;
	} KBDLLHOOKSTRUCT;
#elif defined(MINGW)
	#define LLKHF_UP             0x00000080
#endif

HHOOK llkeyboardhook;

cvar_t	sys_disableWinKeys = SCVAR("sys_disableWinKeys", "0");
cvar_t	sys_disableTaskSwitch = SCVARF("sys_disableTaskSwitch", "0", CVAR_NOTFROMSERVER);	// please don't encourage people to use this...

LRESULT CALLBACK LowLevelKeyboardProc (INT nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT *pkbhs = (KBDLLHOOKSTRUCT *) lParam;
	if (ActiveApp)
	switch (nCode)
	{
	case HC_ACTION:
		{
		//Trap the Left Windowskey
			if (pkbhs->vkCode == VK_LWIN)
			{
				Key_Event (K_LWIN, 0, !(pkbhs->flags & LLKHF_UP));
				return 1;
			}
		//Trap the Right Windowskey
			if (pkbhs->vkCode == VK_RWIN)
			{
				Key_Event (K_RWIN, 0, !(pkbhs->flags & LLKHF_UP));
				return 1;
			}
		//Trap the Application Key (what a pointless key)
			if (pkbhs->vkCode == VK_APPS)
			{
				Key_Event (K_APP, 0, !(pkbhs->flags & LLKHF_UP));
				return 1;
			}

		// Disable CTRL+ESC
			//this works, but we've got to give some way to tab out...
			if (sys_disableTaskSwitch.ival)
			{
				if (pkbhs->vkCode == VK_ESCAPE && GetAsyncKeyState (VK_CONTROL) >> ((sizeof(SHORT) * 8) - 1))
					return 1;
		// Disable ATL+TAB
				if (pkbhs->vkCode == VK_TAB && pkbhs->flags & LLKHF_ALTDOWN)
					return 1;
		// Disable ALT+ESC
				if (pkbhs->vkCode == VK_ESCAPE && pkbhs->flags & LLKHF_ALTDOWN)
					return 1;
			}

			break;
		}
	default:
		break;
	}
	return CallNextHookEx (llkeyboardhook, nCode, wParam, lParam);
}

void SetHookState(qboolean state)
{
	if (!state == !llkeyboardhook)	//not so types are comparable
		return;

	if (llkeyboardhook)
	{
		UnhookWindowsHookEx(llkeyboardhook);
		llkeyboardhook = NULL;
	}
	if (state)
		llkeyboardhook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
}

#endif

/*
===============================================================================

FILE IO

===============================================================================
*/

void Sys_mkdir (char *path)
{
	_mkdir (path);
}

qboolean Sys_remove (char *path)
{
	remove (path);

	return true;
}

int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, int, void *), void *parm)
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
				go = func(file, fd.nFileSizeLow, parm);
			}
		}
		else
		{
			if (wildcmp(match, fd.cFileName))
			{
				Q_snprintfz(file, sizeof(file), "%s%s", apath2, fd.cFileName);
				go = func(file, fd.nFileSizeLow, parm);
			}
		}
	}
	while(FindNextFile(r, &fd) && go);
	FindClose(r);

	return go;
}

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	DWORD  flOldProtect;

//@@@ copy on write or just read-write?
	if (!VirtualProtect((LPVOID)startaddr, length, PAGE_EXECUTE_READWRITE, &flOldProtect))
	{
		char str[1024];

		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
						NULL,
						GetLastError(),
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						str,
						sizeof(str),
						NULL);
		Sys_Error("Protection change failed!\nError %d: %s\n", GetLastError(), str);
	}
}


/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
//	LARGE_INTEGER	PerformanceFreq;
//	unsigned int	lowpart, highpart;
	OSVERSIONINFO	vinfo;

#ifndef SERVERONLY
	Cvar_Register(&sys_disableWinKeys, "System vars");
	Cvar_Register(&sys_disableTaskSwitch, "System vars");

#ifndef CLIENTONLY
	if (!isDedicated && !COM_CheckParm("-nomutex"))
#else
	if (!COM_CheckParm("-nomutex"))	//we need to create a mutex to allow gamespy to realise that we're running, but it might not be desired as it prevents other clients from running too.
#endif
	{
		// allocate a named semaphore on the client so the
		// front end can tell if it is alive

		// mutex will fail if semephore already exists
		qwclsemaphore = CreateMutex(
			NULL,         // Security attributes 
			0,            // owner       
			"qwcl"); // Semaphore name      
	//	if (!qwclsemaphore)
	//		Sys_Error ("QWCL is already running on this system");
		CloseHandle (qwclsemaphore);

		qwclsemaphore = CreateSemaphore(
			NULL,         // Security attributes 
			0,            // Initial count       
			1,            // Maximum count       
			"qwcl"); // Semaphore name      
	}
#endif


#ifndef SERVERONLY
	MaskExceptions ();
	Sys_SetFPCW ();
#endif

#if 0
	if (!QueryPerformanceFrequency (&PerformanceFreq))
		Sys_Error ("No hardware timer available");

// get 32 out of the 64 time bits such that we have around
// 1 microsecond resolution
	lowpart = (unsigned int)PerformanceFreq.LowPart;
	highpart = (unsigned int)PerformanceFreq.HighPart;
	lowshift = 0;

	while (highpart || (lowpart > 2000000.0))
	{
		lowshift++;
		lowpart >>= 1;
		lowpart |= (highpart & 1) << 31;
		highpart >>= 1;
	}

	pfreq = 1.0 / (double)lowpart;

	Sys_InitFloatTime ();
#endif

	// make sure the timer is high precision, otherwise
	// NT gets 18ms resolution
	timeBeginPeriod( 1 );

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4) ||
		(vinfo.dwPlatformId == VER_PLATFORM_WIN32s))
	{
		Sys_Error ("QuakeWorld requires at least Win95 or NT 4.0");
	}
	
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		WinNT = true;
	else
		WinNT = false;
}


void VARGS Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024];
	//, text2[1024];
//	DWORD		dummy;	

 	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

#ifndef SERVERONLY
	SetHookState(false);
	Host_Shutdown ();
#else
	SV_Shutdown();
#endif

	MessageBox(NULL, text, "Error", 0);

#ifndef SERVERONLY
	CloseHandle (qwclsemaphore);
	SetHookState(false);
#endif

	exit (1);
}

void VARGS Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	DWORD		dummy;

	if (!houtput)
		return;

	va_start (argptr,fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	WriteFile (houtput, text, strlen(text), &dummy, NULL);
}

void Sys_Quit (void)
{
#ifndef SERVERONLY
	if (VID_ForceUnlockedAndReturnState)
		VID_ForceUnlockedAndReturnState ();

	SetHookState(false);

	Host_Shutdown ();

	if (tevent)
		CloseHandle (tevent);

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

	SetHookState(false);
#else
	SV_Shutdown();
#endif

#ifdef NPQTV
	{
		extern jmp_buf 	host_abort;
		longjmp (host_abort, 1);
	}
#else
	exit (0);
#endif
}


#if 1
/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	static int			first = 1;
	static LARGE_INTEGER		qpcfreq;
	LARGE_INTEGER		PerformanceCount;
	static LONGLONG			oldcall;
	static LONGLONG			firsttime;
	LONGLONG			diff;

	QueryPerformanceCounter (&PerformanceCount);
	if (first)
	{
		first = 0;
		QueryPerformanceFrequency(&qpcfreq);
		firsttime = PerformanceCount.QuadPart;
		diff = 0;
	}
	else
		diff = PerformanceCount.QuadPart - oldcall;
	if (diff >= 0)
		oldcall = PerformanceCount.QuadPart;
	return (oldcall - firsttime) / (double)qpcfreq.QuadPart;
}
unsigned int Sys_Milliseconds (void)
{
	return Sys_DoubleTime()*1000;
}
#else
unsigned int Sys_Milliseconds (void)
{
	static DWORD starttime;
	static qboolean first = true;
	DWORD now;
//	double t;

	now = timeGetTime();

	if (first) {
		first = false;
		starttime = now;
		return 0.0;
	}
	/*
	if (now < starttime) // wrapped?
	{
		double r;
		r = (now) + (LONG_MAX - starttime);
		starttime = now;
		return r;
	}

	if (now - starttime == 0)
		return 0.0;
*/
	return (now - starttime);
}

double Sys_DoubleTime (void)
{
	return Sys_Milliseconds()/1000.f;
}
#endif



/////////////////////////////////////////////////////////////
//clipboard
HANDLE	clipboardhandle;
char *Sys_GetClipboard(void)
{
	char *clipText;
	if (OpenClipboard(NULL))
	{
		clipboardhandle = GetClipboardData(CF_TEXT);
		if (clipboardhandle)
		{
			clipText = GlobalLock(clipboardhandle);
			if (clipText)
				return clipText;

			//failed at the last hurdle

			GlobalUnlock(clipboardhandle);
		}
		CloseClipboard();
	}

	clipboardhandle = NULL;

	return NULL;
}
void Sys_CloseClipboard(char *bf)
{
	if (clipboardhandle)
	{
		GlobalUnlock(clipboardhandle);
		CloseClipboard();
		clipboardhandle = NULL;
	}
}
void Sys_SaveClipboard(char *text)
{
	HANDLE glob;
	char *temp;
	if (!OpenClipboard(NULL))
		return;
    EmptyClipboard();

	glob = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
    if (glob == NULL)
    {
        CloseClipboard();
        return;
    }

	temp = GlobalLock(glob);
	if (temp != NULL)
	{
		strcpy(temp, text);
		GlobalUnlock(glob);
		SetClipboardData(CF_TEXT, glob);
	}
	else
		GlobalFree(glob);

	CloseClipboard();
}


//end of clipboard
/////////////////////////////////////////////////////////////




/////////////////////////////////////////
//the system console stuff

char *Sys_ConsoleInput (void)
{
	static char	text[256];
	static int		len;
	INPUT_RECORD	recs[1024];
//	int		count;
	int		i;
	int		ch;
	DWORD numevents, numread, dummy=0;
	HANDLE	th;
	char	*clipText, *textCopied;

	if (!hinput)
		return NULL;

	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);	

						if (len)
						{
							text[len] = 0;
							len = 0;
							return text;
						}
						break;

					case '\b':
						if (len)
						{
							len--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						}
						break;

					default:
						if (((ch=='V' || ch=='v') && (recs[0].Event.KeyEvent.dwControlKeyState & 
							(LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) || ((recs[0].Event.KeyEvent.dwControlKeyState 
							& SHIFT_PRESSED) && (recs[0].Event.KeyEvent.wVirtualKeyCode
							==VK_INSERT))) {
							if (OpenClipboard(NULL)) {
								th = GetClipboardData(CF_TEXT);
								if (th) {
									clipText = GlobalLock(th);
									if (clipText) {
										textCopied = BZ_Malloc(GlobalSize(th)+1);
										strcpy(textCopied, clipText);
/* Substitutes a NULL for every token */strtok(textCopied, "\n\r\b");
										i = strlen(textCopied);
										if (i+len>=256)
											i=256-len;
										if (i>0) {
											textCopied[i]=0;
											text[len]=0;
											strcat(text, textCopied);
											len+=dummy;
											WriteFile(houtput, textCopied, i, &dummy, NULL);
										}
										BZ_Free(textCopied);
									}
									GlobalUnlock(th);
								}
								CloseClipboard();
							}
						} else if (ch >= ' ')
						{
							WriteFile(houtput, &ch, 1, &dummy, NULL);	
							text[len] = ch;
							len = (len + 1) & 0xff;
						}

						break;

				}
			}
		}
	}

	return NULL;
}

BOOL WINAPI HandlerRoutine (DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:		
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			Cbuf_AddText ("quit\n", RESTRICT_LOCAL);
			return true;
	}

	return false;
}

qboolean Sys_InitTerminal (void)
{
	if (!AllocConsole())
		return false;

	//if we still have the splash screen, kill it
	if (hwnd_dialog)
		DestroyWindow(hwnd_dialog);
	hwnd_dialog = NULL;

	SetConsoleCtrlHandler (HandlerRoutine, TRUE);
	SetConsoleTitle (FULLENGINENAME " dedicated server");
	hinput = GetStdHandle (STD_INPUT_HANDLE);
	houtput = GetStdHandle (STD_OUTPUT_HANDLE);

	return true;
}
void Sys_CloseTerminal (void)
{
	FreeConsole();

	hinput = NULL;
	houtput = NULL;
}


//
////////////////////////////

void Sys_Sleep (void)
{
}

void Sys_SendKeyEvents (void)
{
#ifndef NPQTV
    MSG        msg;

	if (isDedicated)
	{
#ifndef CLIENTONLY
		SV_GetConsoleCommands ();
#endif
		return;
	}

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
	// we always update if there are any event, even if we're paused
		scr_skipupdate = 0;

		if (!GetMessage (&msg, NULL, 0, 0))
			break;
//			Sys_Quit ();
//		if (TranslateMessage (&msg))
//			continue;
      	DispatchMessage (&msg);
	}
#endif
}


void Sys_ServerActivity(void)
{
#ifndef SERVERONLY
	if (GetActiveWindow() != mainwindow)
		FlashWindow(mainwindow, true);
#endif
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
==================
WinMain
==================
*/
void SleepUntilInput (int time)
{

	MsgWaitForMultipleObjects(1, &tevent, FALSE, time, QS_ALLINPUT);
}








qboolean Sys_Startup_CheckMem(quakeparms_t *parms)
{
	int t;
	MEMORYSTATUS	lpBuffer;
	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

// take the greater of all the available memory or half the total memory,
// but at least 8 Mb and no more than 16 Mb, unless they explicitly
// request otherwise
	parms->memsize = lpBuffer.dwAvailPhys;

	if (parms->memsize < MINIMUM_WIN_MEMORY)
		parms->memsize = MINIMUM_WIN_MEMORY;

	if (parms->memsize < (lpBuffer.dwTotalPhys >> 1))
		parms->memsize = lpBuffer.dwTotalPhys >> 1;

	if (parms->memsize > MAXIMUM_WIN_MEMORY)
		parms->memsize = MAXIMUM_WIN_MEMORY;

	if (COM_CheckParm ("-heapsize"))
	{
		t = COM_CheckParm("-heapsize") + 1;

		if (t < com_argc)
			parms->memsize = Q_atoi (com_argv[t]) * 1024;
	}
	else if (COM_CheckParm ("-mem"))
	{
		t = COM_CheckParm("-mem") + 1;

		if (t < com_argc)
			parms->memsize = Q_atoi (com_argv[t]) * 1024*1024;
	}

	parms->membase = VirtualAlloc (NULL, parms->memsize, MEM_RESERVE, PAGE_NOACCESS);
//	parms->membase = malloc (parms.memsize);

	if (!parms->membase)
		return false;
	return true;
}

#ifdef NPQTV
static quakeparms_t	parms;
double lastlooptime;
qboolean NPQTV_Sys_Startup(int argc, char *argv[])
{
	if (!host_initialized)
	{
		TL_InitLanguages();

		parms.argc = argc;
		parms.argv = argv;
		parms.basedir = argv[0];
		COM_InitArgv (parms.argc, parms.argv);

		if (!Sys_Startup_CheckMem(&parms))
			return false;

		Host_Init (&parms);
	}

	lastlooptime = Sys_DoubleTime ();

	return true;
}

void NPQTV_Sys_MainLoop(void)
{
	double duratrion, newtime;

	if (isDedicated)
	{
#ifndef CLIENTONLY
		NET_Sleep(50, false);

	// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		duratrion = newtime - lastlooptime;
		lastlooptime = newtime;
		
		SV_Frame ();
#else
		Sys_Error("wut?");
#endif
	}
	else
	{
#ifndef SERVERONLY
		newtime = Sys_DoubleTime ();
		duratrion = newtime - lastlooptime;
		Host_Frame (duratrion);
		lastlooptime = newtime;

		SetHookState(sys_disableWinKeys.ival);

//			Sleep(0);
#else
		Sys_Error("wut?");
#endif
	}
}

void NPQTV_Sys_Shutdown(void)
{
	//disconnect server/client/etc
	CL_Disconnect_f();
	R_ShutdownRenderer();

	Host_Shutdown();
}

#else
/*
==================
WinMain
==================
*/
HINSTANCE	global_hInstance;
int			global_nCmdShow;
char		*argv[MAX_NUM_ARGVS];
static char	exename[256];
HWND		hwnd_dialog;

/*
#ifdef _MSC_VER
#include <signal.h>
void VARGS Signal_Error_Handler(int i)
{
	int *basepointer;
	__asm {mov basepointer,ebp};
	Sys_Error("Received signal, offset was 0x%8x", basepointer[73]);
}
#endif
*/

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
//    MSG				msg;
	quakeparms_t	parms;
	double			time, oldtime, newtime;
	char	cwd[1024];
	const char *qtvfile = NULL;
	
	/* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

#ifdef _MSC_VER
#if _M_IX86_FP >= 1
	{
		int idedx;
		char cpuname[13];
		/*I'm not going to check to make sure cpuid works.*/
		__asm
		{
			xor eax, eax
			cpuid
			mov dword ptr [cpuname+0],ebx
			mov dword ptr [cpuname+4],edx
			mov dword ptr [cpuname+8],ecx
		}
		cpuname[12] = 0;
		__asm
		{
			mov eax, 0x1
			cpuid
			mov idedx, edx
		}
//		MessageBox(NULL, cpuname, cpuname, 0);
#if _M_IX86_FP >= 2
		if (!(idedx&(1<<26)))
			MessageBox(NULL, "This is an SSE2 optimised build, and your cpu doesn't seem to support it", DISTRIBUTION, 0);
		else
#endif
		     if (!(idedx&(1<<25)))
			MessageBox(NULL, "This is an SSE optimised build, and your cpu doesn't seem to support it", DISTRIBUTION, 0);
	}
#endif
#endif

#ifdef CATCHCRASH
	__try
#endif
	{
/*
#ifndef _DEBUG
#ifdef _MSC_VER
		signal (SIGFPE,	Signal_Error_Handler);
		signal (SIGILL,	Signal_Error_Handler);
		signal (SIGSEGV,	Signal_Error_Handler);
#endif
#endif
*/
		global_hInstance = hInstance;
		global_nCmdShow = nCmdShow;

		parms.argc = 1;
		argv[0] = exename;

		while (*lpCmdLine && (parms.argc < MAX_NUM_ARGVS))
		{
			while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				if (*lpCmdLine == '\"')
				{
					lpCmdLine++;

					argv[parms.argc] = lpCmdLine;
					parms.argc++;

					while (*lpCmdLine && *lpCmdLine != '\"')
						lpCmdLine++;
				}
				else
				{
					argv[parms.argc] = lpCmdLine;
					parms.argc++;


					while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
						lpCmdLine++;
				}

				if (*lpCmdLine)
				{
					*lpCmdLine = 0;
					lpCmdLine++;
				}
			}
		}

		GetModuleFileName(NULL, cwd, sizeof(cwd)-1);
		strcpy(exename, COM_SkipPath(cwd));
		parms.argv = argv;

		COM_InitArgv (parms.argc, parms.argv);

		if (COM_CheckParm("--version") || COM_CheckParm("-v"))
		{
			printf("version " DISTRIBUTION " " __TIME__ __DATE__ "\n");
			return true;
		}

		if (!GetCurrentDirectory (sizeof(cwd), cwd))
			Sys_Error ("Couldn't determine current directory");
		if (parms.argc >= 2)
		{
			if (*parms.argv[1] != '-' && *parms.argv[1] != '+')
			{
				char *e;

				qtvfile = parms.argv[1];

				GetModuleFileName(NULL, cwd, sizeof(cwd)-1);
				for (e = cwd+strlen(cwd)-1; e >= cwd; e--)
				{
					if (*e == '/' || *e == '\\')
					{
						*e = 0;
						break;
					}
				}

			}
		}

		TL_InitLanguages();
		//tprints are now allowed

		parms.basedir = cwd;

		parms.argc = com_argc;
		parms.argv = com_argv;

	#if !defined(CLIENTONLY) && !defined(SERVERONLY)
		if (COM_CheckParm ("-dedicated"))
			isDedicated = true;
	#endif

		if (isDedicated)
		{
	#if !defined(CLIENTONLY)
			hwnd_dialog=NULL;
			
			if (!Sys_InitTerminal())
				Sys_Error ("Couldn't allocate dedicated server console");
	#endif
		}
	#ifdef IDD_DIALOG1
		else
			hwnd_dialog = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, NULL);

		if (hwnd_dialog)
		{
			RECT			rect;
			if (GetWindowRect (hwnd_dialog, &rect))
			{
				if (rect.left > (rect.top * 2))
				{
					SetWindowPos (hwnd_dialog, 0,
						(rect.left / 2) - ((rect.right - rect.left) / 2),
						rect.top, 0, 0,
						SWP_NOZORDER | SWP_NOSIZE);
				}
			}

			ShowWindow (hwnd_dialog, SW_SHOWDEFAULT);
			UpdateWindow (hwnd_dialog);
			SetForegroundWindow (hwnd_dialog);
		}
	#endif

		if (!Sys_Startup_CheckMem(&parms))
			Sys_Error ("Not enough memory free; check disk space\n");


	#ifndef CLIENTONLY
		if (isDedicated)	//compleate denial to switch to anything else - many of the client structures are not initialized.
		{
			SV_Init (&parms);

			SV_Frame ();

			while (1)
			{
				if (!isDedicated)
					Sys_Error("Dedicated was cleared");
				NET_Sleep(100, false);				
				SV_Frame ();
			}
			return TRUE;
		}
	#endif

		tevent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!tevent)
			Sys_Error ("Couldn't create event");

	#ifdef SERVERONLY
		Sys_Printf ("SV_Init\n");
		SV_Init(&parms);
	#else
		Sys_Printf ("Host_Init\n");
		Host_Init (&parms);
	#endif

		oldtime = Sys_DoubleTime ();

		if (qtvfile)
		{
			char *ext = COM_FileExtension(qtvfile);
			if (!strcmp(ext, "qwd") || !strcmp(ext, "dem") || !strcmp(ext, "mvd"))
				Cbuf_AddText(va("playdemo \"#%s\"\n", qtvfile), RESTRICT_LOCAL);
			else
				Cbuf_AddText(va("qtvplay \"#%s\"\n", qtvfile), RESTRICT_LOCAL);
		}

	//client console should now be initialized.

		/* main window message loop */
		while (1)
		{
			if (isDedicated)
			{
	#ifndef CLIENTONLY
				NET_Sleep(50, false);

			// find time passed since last cycle
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
				oldtime = newtime;
				
				SV_Frame ();
	#else
				Sys_Error("wut?");
	#endif
			}
			else
			{
	#ifndef SERVERONLY
		// yield the CPU for a little while when paused, minimized, or not the focus
	/*			if (cl.paused && !Media_PlayingFullScreen())
				{
					SleepUntilInput (PAUSE_SLEEP);
					scr_skipupdate = 1;		// no point in bothering to draw
				}
				else if (!ActiveApp && !Media_PlayingFullScreen())
				{
					SleepUntilInput (NOT_FOCUS_SLEEP);
				}
	*/
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
				Host_Frame (time);
				oldtime = newtime;

				SetHookState(sys_disableWinKeys.ival);

	//			Sleep(0);
	#else
				Sys_Error("wut?");
	#endif
			}
		}
	}
#ifdef CATCHCRASH
	__except (CrashExceptionHandler(GetExceptionCode(), GetExceptionInformation()))
	{
		return 1;
	}
#endif

    /* return success of application */
    return TRUE;
}

int __cdecl main(void)
{
	FreeConsole();
	return WinMain(GetModuleHandle(NULL), NULL, GetCommandLine(), SW_NORMAL);
}
#endif

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
	HDC hdc;
	int rate;

	hdc = GetDC(NULL);

	*width = GetDeviceCaps(hdc, HORZRES);
	*height = GetDeviceCaps(hdc, VERTRES);
	*bpp = GetDeviceCaps(hdc, BITSPIXEL);
	rate = GetDeviceCaps(hdc, VREFRESH);

	if (rate == 1)
		rate = 0;

	*refreshrate = rate;

	ReleaseDC(NULL, hdc);

	return true;
}


#if !id386 //these couldn't be found... (it is a masm thing, right?)
void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

void VARGS Sys_SetFPCW (void)
{
}

void VARGS MaskExceptions (void)
{
}
#endif

#ifdef MULTITHREAD
/* Thread creation calls */
typedef struct threadwrap_s
{
	void (*func)(void *);
	void *args;
} threadwrap_t;

// the thread call is wrapped so we don't need WINAPI everywhere
DWORD WINAPI threadwrapper(void *args)
{
	threadwrap_t tw;
	tw.func = ((threadwrap_t *)args)->func;
	tw.args = ((threadwrap_t *)args)->args;

	free(args);
	tw.func(tw.args);	

#ifndef WIN32CRTDLL
	_endthreadex(0);
#endif
	return 0;
}

void *Sys_CreateThread(int (*func)(void *), void *args, int stacksize)
{
	threadwrap_t *tw = (threadwrap_t *)malloc(sizeof(threadwrap_t));
	HANDLE handle;
	
	if (!tw)
		return NULL;

	stacksize += 128; // wrapper overhead, also prevent default stack size
	tw->func = func;
	tw->args = args;
#ifdef WIN32CRTDLL
	handle = (HANDLE)CreateThread(NULL, stacksize, &threadwrapper, (void *)tw, 0, NULL);
#else
	handle = (HANDLE)_beginthreadex(NULL, stacksize, &threadwrapper, (void *)tw, 0, NULL);
#endif
	if (!handle)
	{
		free(tw);
		return NULL;
	}

	return (void *)handle;
}

void Sys_WaitOnThread(void *thread)
{	
	WaitForSingleObject((HANDLE)thread, INFINITE);
	CloseHandle((HANDLE)thread);
}

/* Mutex calls */
void *Sys_CreateMutex(void)
{
	return (void *)CreateMutex(NULL, 0, NULL);
}

qboolean Sys_TryLockMutex(void *mutex)
{
	return WaitForSingleObject(mutex, 0) == WAIT_OBJECT_0;
}

qboolean Sys_LockMutex(void *mutex)
{
	return WaitForSingleObject(mutex, INFINITE) == WAIT_OBJECT_0;
}

qboolean Sys_UnlockMutex(void *mutex)
{
	return !!ReleaseMutex(mutex);
}

void Sys_DestroyMutex(void *mutex)
{
	CloseHandle(mutex);
}

/* Conditional wait calls */
/*
TODO: Windows Vista has condition variables as documented here:
http://msdn.microsoft.com/en-us/library/ms682052(VS.85).aspx
Note this uses Slim Reader/Writer locks (Vista+ exclusive)
or critical sections.

The condition variable implementation is based on the libSDL implementation.
This code could probably be made more efficient with the use of events or
different mechanisms but for now the main concern is a correct and
complete solution.
*/
typedef struct condvar_s
{
    int waiting;
    int signals;
    CRITICAL_SECTION countlock;
	CRITICAL_SECTION mainlock;
    HANDLE wait_sem;
    HANDLE wait_done;
} condvar_t;

void *Sys_CreateConditional(void) 
{ 
	condvar_t *cv;

	cv = (condvar_t *)malloc(sizeof(condvar_t));
	if (!cv)
		return NULL;

	cv->waiting = 0;
	cv->signals = 0;
	InitializeCriticalSection (&cv->mainlock);
	InitializeCriticalSection (&cv->countlock);
	cv->wait_sem = CreateSemaphore (NULL, 0, 0x7fffffff, NULL);
	cv->wait_done = CreateSemaphore (NULL, 0, 0x7fffffff, NULL);

	if (cv->wait_sem && cv->wait_done)
		return (void *)cv;

	// something failed so deallocate everything
	if (cv->wait_done)
		CloseHandle(cv->wait_done);
	if (cv->wait_sem)
		CloseHandle(cv->wait_sem);
	DeleteCriticalSection(&cv->countlock);
	DeleteCriticalSection(&cv->mainlock);
	free(cv);

	return NULL;
}

qboolean Sys_LockConditional(void *condv) 
{ 
	EnterCriticalSection(&((condvar_t *)condv)->mainlock);
	return true; 
}

qboolean Sys_UnlockConditional(void *condv) 
{ 
	LeaveCriticalSection(&((condvar_t *)condv)->mainlock);
	return true; 
}

qboolean Sys_ConditionWait(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;
	qboolean success;

	// increase count for non-signaled waiting threads
	EnterCriticalSection(&cv->countlock);
	cv->waiting++;
	LeaveCriticalSection(&cv->countlock);

	LeaveCriticalSection(&cv->mainlock); // unlock as per condition variable definition

	// wait on a signal
	success = (WaitForSingleObject(cv->wait_sem, INFINITE) != WAIT_FAILED);

	// update waiting count and alert signaling thread that we're done to avoid the deadlock condition
	EnterCriticalSection(&cv->countlock);
	if (cv->signals > 0) 
	{
		ReleaseSemaphore(cv->wait_done, cv->signals, NULL);
		cv->signals = 0;
	}
	cv->waiting--;
	LeaveCriticalSection(&cv->countlock);

	EnterCriticalSection(&cv->mainlock); // lock as per condition variable definition

	return success;
}

qboolean Sys_ConditionSignal(void *condv) 
{
	condvar_t *cv = (condvar_t *)condv;

	// if there are non-signaled waiting threads, we signal one and wait on the response
	EnterCriticalSection(&cv->countlock);
	if (cv->waiting > cv->signals)
	{
		cv->signals++;
		ReleaseSemaphore(cv->wait_sem, 1, NULL);
		LeaveCriticalSection(&cv->countlock);
		WaitForSingleObject(cv->wait_done, INFINITE);
	}
	else
		LeaveCriticalSection(&cv->countlock);

    return true;
}

qboolean Sys_ConditionBroadcast(void *condv) 
{
	condvar_t *cv = (condvar_t *)condv;

	// if there are non-signaled waiting threads, we signal all of them and wait on all the responses back
	EnterCriticalSection(&cv->countlock);
	if (cv->waiting > cv->signals) 
	{
		int i, num_waiting;

		num_waiting = (cv->waiting - cv->signals);
		cv->signals = cv->waiting;
		
		ReleaseSemaphore(cv->wait_sem, num_waiting, NULL);
		LeaveCriticalSection(&cv->countlock);
		// there's no call to wait for the same object multiple times so we need to loop through
		// and burn up the semaphore count
		for (i = 0; i < num_waiting; i++) 
			WaitForSingleObject(cv->wait_done, INFINITE);
	}
	else
		LeaveCriticalSection(&cv->countlock);

	return true;
}

void Sys_DestroyConditional(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;

	CloseHandle(cv->wait_done);
	CloseHandle(cv->wait_sem);
	DeleteCriticalSection(&cv->countlock);
	DeleteCriticalSection(&cv->mainlock);
	free(cv);
}
#endif
