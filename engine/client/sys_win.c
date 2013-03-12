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

//#define RESTARTTEST

#ifdef MULTITHREAD
#include <process.h>
#endif

#ifdef GLQUAKE
#define PRINTGLARRAYS
#endif

#ifdef _DEBUG
#if _MSC_VER >= 1300
//#define CATCHCRASH
#endif
#endif

#if !defined(CLIENTONLY) && !defined(SERVERONLY)
qboolean isDedicated = false;
#endif
extern qboolean isPlugin;
qboolean debugout;

HWND sys_parentwindow;
unsigned int sys_parentleft;	//valid if sys_parentwindow is set
unsigned int sys_parenttop;
unsigned int sys_parentwidth;	//valid if sys_parentwindow is set
unsigned int sys_parentheight;

extern int fs_switchgame;


#ifdef RESTARTTEST
jmp_buf restart_jmpbuf;
#endif
/*
================
Sys_RandomBytes
================
*/
#include <wincrypt.h>
qboolean Sys_RandomBytes(qbyte *string, int len)
{
	HCRYPTPROV  prov;

	if(!CryptAcquireContext( &prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		return false;
	}

	if(!CryptGenRandom(prov, len, (BYTE *)string))
	{
		CryptReleaseContext( prov, 0);
		return false;
	}
	CryptReleaseContext(prov, 0);
	return true;
}

/*
=================
Library loading
=================
*/
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
	{
#ifdef _WIN64
		lib = LoadLibrary(va("%s_64", name));
#elif defined(_WIN32)
		lib = LoadLibrary(va("%s_32", name));
#endif
		if (!lib)
			return NULL;
	}

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
			Con_DPrintf("Missing export \"%s\" in \"%s\"\n", funcs[i].name, name);
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


#define MINIMUM_WIN_MEMORY	MINIMUM_MEMORY
#define MAXIMUM_WIN_MEMORY	0x8000000

int		starttime;
qboolean ActiveApp, Minimized;
qboolean	WinNT;

static HANDLE		hinput, houtput;

HANDLE		qwclsemaphore;

static HANDLE	tevent;

void Sys_InitFloatTime (void);

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

#ifdef PRINTGLARRAYS
#include "glquake.h"
#define GL_VERTEX_ARRAY_BINDING					0x85B5
#define GL_ARRAY_BUFFER							0x8892
#define GL_ELEMENT_ARRAY_BUFFER					0x8893
#define GL_ARRAY_BUFFER_BINDING					0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING			0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING			0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING			0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING			0x8898
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING   0x889A
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED			0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE				0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE			0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE				0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED		0x886A
#define GL_VERTEX_ATTRIB_ARRAY_POINTER			0x8645
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING	0x889F
#define GL_CURRENT_PROGRAM						0x8B8D

char *DecodeGLEnum(GLenum num)
{
	switch(num)
	{
	case GL_CW:						return "GL_CW";
	case GL_CCW:					return "GL_CCW";
	case GL_NEVER:					return "GL_NEVER";
	case GL_LESS:					return "GL_LESS";
	case GL_EQUAL:					return "GL_EQUAL";
	case GL_LEQUAL:					return "GL_LEQUAL";
	case GL_GREATER:				return "GL_GREATER";
	case GL_NOTEQUAL:				return "GL_NOTEQUAL";
	case GL_GEQUAL:					return "GL_GEQUAL";
	case GL_ALWAYS:					return "GL_ALWAYS";
	case GL_FRONT:					return "GL_FRONT";
	case GL_BACK:					return "GL_BACK";
	case GL_FRONT_AND_BACK:			return "GL_FRONT_AND_BACK";
	case GL_COMBINE_ARB:			return "GL_COMBINE";
	case GL_MODULATE:				return "GL_MODULATE";
	case GL_REPLACE:				return "GL_REPLACE";
	case GL_ZERO:					return "GL_ZERO";
	case GL_ONE:					return "GL_ONE";
	case GL_SRC_COLOR:				return "GL_SRC_COLOR";
	case GL_ONE_MINUS_SRC_COLOR:	return "GL_ONE_MINUS_SRC_COLOR";
	case GL_SRC_ALPHA:				return "GL_SRC_ALPHA";
	case GL_ONE_MINUS_SRC_ALPHA:	return "GL_ONE_MINUS_SRC_ALPHA";
	case GL_DST_ALPHA:				return "GL_DST_ALPHA";
	case GL_ONE_MINUS_DST_ALPHA:	return "GL_ONE_MINUS_DST_ALPHA";
	case GL_DST_COLOR:				return "GL_DST_COLOR";
	case GL_ONE_MINUS_DST_COLOR:	return "GL_ONE_MINUS_DST_COLOR";
	case GL_SRC_ALPHA_SATURATE:		return "GL_SRC_ALPHA_SATURATE";
	default:						return va("0x%x", num);
	}
}

void DumpGLState(void)
{
	int rval;
	void *ptr;
	int i;
	GLint glint;
	GLint glint4[4];
	void (APIENTRY *qglGetVertexAttribiv) (GLuint index, GLenum pname, GLint* params);
	void (APIENTRY *qglGetVertexAttribPointerv) (GLuint index, GLenum pname, GLvoid** pointer);
	qglGetVertexAttribiv = (void*)wglGetProcAddress("glGetVertexAttribiv");
	qglGetVertexAttribPointerv = (void*)wglGetProcAddress("glGetVertexAttribPointerv");
#pragma comment(lib,"opengl32.lib")

	if (qglGetVertexAttribiv)
	{
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &rval);
		Sys_Printf("VERTEX_ARRAY_BINDING: %i\n", rval);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &rval);
		Sys_Printf("GL_ARRAY_BUFFER_BINDING: %i\n", rval);
		if (glIsEnabled(GL_COLOR_ARRAY))
		{
			glGetIntegerv(GL_COLOR_ARRAY_BUFFER_BINDING, &rval);
			glGetPointerv(GL_COLOR_ARRAY_POINTER, &ptr);
			Sys_Printf("GL_COLOR_ARRAY: %s %i:%p\n", glIsEnabled(GL_COLOR_ARRAY)?"en":"dis", rval, ptr);
		}
//		if (glIsEnabled(GL_FOG_COORDINATE_ARRAY_EXT))
//		{
//			glGetPointerv(GL_FOG_COORD_ARRAY_POINTER, &ptr);
//			Sys_Printf("GL_FOG_COORDINATE_ARRAY_EXT: %i (%lx)\n", (int) glIsEnabled(GL_FOG_COORDINATE_ARRAY_EXT), (int) ptr);
//		}
//		if (glIsEnabled(GL_INDEX_ARRAY))
		{
			glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &rval);
			glGetPointerv(GL_INDEX_ARRAY_POINTER, &ptr);
			Sys_Printf("GL_INDEX_ARRAY: %s %i:%p\n", glIsEnabled(GL_INDEX_ARRAY)?"en":"dis", rval, ptr);
		}
		if (glIsEnabled(GL_NORMAL_ARRAY))
		{
			glGetIntegerv(GL_NORMAL_ARRAY_BUFFER_BINDING, &rval);
			glGetPointerv(GL_NORMAL_ARRAY_POINTER, &ptr);
			Sys_Printf("GL_NORMAL_ARRAY: %s %i:%p\n", glIsEnabled(GL_NORMAL_ARRAY)?"en":"dis", rval, ptr);
		}
	//	glGetPointerv(GL_SECONDARY_COLOR_ARRAY_POINTER, &ptr);
	//	Sys_Printf("GL_SECONDARY_COLOR_ARRAY: %i (%lx)\n", (int) glIsEnabled(GL_SECONDARY_COLOR_ARRAY), (int) ptr);
		for (i = 0; i < 4; i++)
		{
			qglClientActiveTextureARB(mtexid0 + i);
			if (glIsEnabled(GL_TEXTURE_COORD_ARRAY))
			{
				glGetIntegerv(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING, &rval);
				glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &ptr);
				Sys_Printf("GL_TEXTURE_COORD_ARRAY %i: %s %i:%p\n", i, glIsEnabled(GL_TEXTURE_COORD_ARRAY)?"en":"dis", rval, ptr);
			}
		}
		if (glIsEnabled(GL_VERTEX_ARRAY))
		{
			glGetIntegerv(GL_VERTEX_ARRAY_BUFFER_BINDING, &rval);
			glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
			Sys_Printf("GL_VERTEX_ARRAY: %s %i:%p\n", glIsEnabled(GL_VERTEX_ARRAY)?"en":"dis", rval, ptr);
		}

		for (i = 0; i < 16; i++)
		{
			int en, bo, as, st, ty, no;

			qglGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en);
			if (!en)
				continue;
			qglGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bo);
			qglGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &as);
			qglGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &st);
			qglGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &ty);
			qglGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &no);
			qglGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);

			Sys_Printf("attrib%i: %s as:%i st:%i ty:%0x %s%i:%p\n", i, en?"en":"dis", as, st,ty,no?"norm ":"", bo, ptr);
		}

		glGetIntegerv(GL_CURRENT_PROGRAM, &glint);
		Sys_Printf("GL_CURRENT_PROGRAM: %i\n", glint);

		glGetIntegerv(GL_BLEND, &glint);
		Sys_Printf("GL_BLEND: %i\n", glint);
		glGetIntegerv(GL_BLEND_SRC, &glint);
		Sys_Printf("GL_BLEND_SRC: %i\n", DecodeGLEnum(glint));
		glGetIntegerv(GL_BLEND_DST, &glint);
		Sys_Printf("GL_BLEND_DST: %i\n", DecodeGLEnum(glint));

		glGetIntegerv(GL_DEPTH_WRITEMASK, &glint);
		Sys_Printf("GL_DEPTH_WRITEMASK: %i\n", glint);
		glGetIntegerv(GL_DEPTH_TEST, &glint);
		Sys_Printf("GL_DEPTH_TEST: %i\n", glint);
		glGetIntegerv(GL_DEPTH_FUNC, &glint);
		Sys_Printf("GL_DEPTH_FUNC: %s\n", DecodeGLEnum(glint));
		glGetIntegerv(GL_CULL_FACE, &glint);
		Sys_Printf("GL_CULL_FACE: %i\n", glint);
		glGetIntegerv(GL_CULL_FACE_MODE, &glint);
		Sys_Printf("GL_CULL_FACE_MODE: %s\n", DecodeGLEnum(glint));
		glGetIntegerv(GL_FRONT_FACE, &glint);
		Sys_Printf("GL_FRONT_FACE: %s\n", DecodeGLEnum(glint));
		glGetIntegerv(GL_SCISSOR_TEST, &glint);
		Sys_Printf("GL_SCISSOR_TEST: %i\n", glint);
		glGetIntegerv(GL_STENCIL_TEST, &glint);
		Sys_Printf("GL_STENCIL_TEST: %i\n", glint);
		glGetIntegerv(GL_COLOR_WRITEMASK, glint4);
		Sys_Printf("GL_COLOR_WRITEMASK: %i %i %i %i\n", glint4[0], glint4[1], glint4[2], glint4[3]);

		GL_SelectTexture(0);
		glGetIntegerv(GL_TEXTURE_2D, &glint);
		Sys_Printf("GL_TEXTURE_2D: %i\n", glint);
		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &glint);
		Sys_Printf("GL_TEXTURE_ENV_MODE: %s\n", DecodeGLEnum(glint));
		GL_SelectTexture(1);
		glGetIntegerv(GL_TEXTURE_2D, &glint);
		Sys_Printf("GL_TEXTURE_2D: %i\n", glint);
		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &glint);
		Sys_Printf("GL_TEXTURE_ENV_MODE: %s\n", DecodeGLEnum(glint));
		GL_SelectTexture(2);
		glGetIntegerv(GL_TEXTURE_2D, &glint);
		Sys_Printf("GL_TEXTURE_2D: %i\n", glint);
		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &glint);
		Sys_Printf("GL_TEXTURE_ENV_MODE: %s\n", DecodeGLEnum(glint));
	}
}
#endif

DWORD CrashExceptionHandler (DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo)
{
	char dumpPath[1024];
	HANDLE hProc = GetCurrentProcess();
	DWORD procid = GetCurrentProcessId();
	HANDLE dumpfile;
	HMODULE hDbgHelp;
	MINIDUMPWRITEDUMP fnMiniDumpWriteDump;
	HMODULE hKernel;
	BOOL (WINAPI *pIsDebuggerPresent)(void);

#ifdef PRINTGLARRAYS
	DumpGLState();
#endif

	hKernel = LoadLibrary ("kernel32");
	pIsDebuggerPresent = (void*)GetProcAddress(hKernel, "IsDebuggerPresent");

#ifdef GLQUAKE
	GLVID_Crashed();
#endif

	if (pIsDebuggerPresent ())
	{
		/*if we have a current window, minimize it to bring us out of fullscreen*/
		extern qboolean vid_initializing;
		qboolean oldval = vid_initializing;
		vid_initializing = true;
//		ShowWindow(mainwindow, SW_MINIMIZE);
		vid_initializing = oldval;
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
	else
		MessageBox(NULL, "Kaboom! Sorry. No MiniDumpWriteDump function.", DISTRIBUTION " Sucks", 0);
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
	#ifndef LLKHF_UP
		#define LLKHF_UP             0x00000080
	#endif
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
				Key_Event (0, K_LWIN, 0, !(pkbhs->flags & LLKHF_UP));
				return 1;
			}
		//Trap the Right Windowskey
			if (pkbhs->vkCode == VK_RWIN)
			{
				Key_Event (0, K_RWIN, 0, !(pkbhs->flags & LLKHF_UP));
				return 1;
			}
		//Trap the Application Key (what a pointless key)
			if (pkbhs->vkCode == VK_APPS)
			{
				Key_Event (0, K_APP, 0, !(pkbhs->flags & LLKHF_UP));
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
	if (remove (path) != 0)
	{
		int e = errno;
		if (e == ENOENT)
			return true;	//return success if it doesn't already exist.
		return false;
	}

	return true;
}

qboolean Sys_Rename (char *oldfname, char *newfname)
{
	return !rename(oldfname, newfname);
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
		Sys_Error("Protection change failed!\nError %d: %s\n", (int)GetLastError(), str);
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


void Sys_Shutdown(void)
{
	if (host_parms.membase)
	{
		VirtualFree(host_parms.membase, 0, MEM_RELEASE);
		host_parms.membase = 0;
	}

	if (tevent)
		CloseHandle (tevent);
	tevent = NULL;

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);
	qwclsemaphore = NULL;
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

static wchar_t dequake(conchar_t chr)
{
	chr &= CON_CHARMASK;

	/*only this range are quake chars*/
	if (chr >= 0xe000 && chr < 0xe100)
	{
		chr &= 0xff;
		if (chr >= 146 && chr < 156)
			chr = chr - 146 + '0';
		if (chr >= 0x12 && chr <= 0x1b)
			chr = chr - 0x12 + '0';
		if (chr == 143)
			chr = '.';
		if (chr == 128 || chr == 129 || chr == 130 || chr == 157 || chr == 158 || chr == 159)
			chr = '-';
		if (chr >= 128)
			chr -= 128;
		if (chr == 16)
			chr = '[';
		if (chr == 17)
			chr = ']';
		if (chr == 0x1c)
			chr = 249;
	}
	/*this range contains pictograms*/
	if (chr >= 0xe100 && chr < 0xe200)
	{
		chr = '?';
	}
	return chr;
}

void VARGS Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	DWORD		dummy;

	if (!houtput && !debugout)
		return;

	va_start (argptr,fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	if (debugout)
	{
		//msvc debug output
		conchar_t msg[1024], *end, *in;
		wchar_t wide[1024], *out;
		end = COM_ParseFunString(CON_WHITEMASK, text, msg, sizeof(msg), false);
		out = wide;
		in = msg;
		for (in = msg; in < end; in++)
		{
			if (!(*in & CON_HIDDEN))
				*out++ = dequake(*in & CON_CHARMASK);
		}
		*out = 0;
		OutputDebugStringW(wide);
	}
	if (houtput)
		WriteFile (houtput, text, strlen(text), &dummy, NULL);
}

void Sys_Quit (void)
{
#ifndef SERVERONLY
	SetHookState(false);

	Host_Shutdown ();

	SetHookState(false);
#else
	SV_Shutdown();
#endif

	TL_Shutdown();

#ifdef RESTARTTEST
	longjmp(restart_jmpbuf, 1);
#endif

#ifdef USE_MSVCRT_DEBUG
	if (_CrtDumpMemoryLeaks())
		OutputDebugStringA("Leaks detected\n");
#endif

	exit(1);
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
char *cliputf8;
char *Sys_GetClipboard(void)
{
	char *clipText;
	unsigned short *clipWText;
	if (OpenClipboard(NULL))
	{
		extern cvar_t com_parseutf8;
		//windows programs interpret CF_TEXT as ansi (aka: gibberish)
		//so grab utf-16 text and convert it to utf-8 if our console parsing is set to accept that.
		if (com_parseutf8.ival > 0)
		{
			clipboardhandle = GetClipboardData(CF_UNICODETEXT);
			if (clipboardhandle)
			{
				clipWText = GlobalLock(clipboardhandle);
				if (clipWText)
				{
					unsigned int l, c;
					for (l = 0; clipWText[l]; l++)
						;
					l = l*4 + 1;
					clipText = cliputf8 = malloc(l);
					while(*clipWText)
					{
						c = utf8_encode(clipText, *clipWText++, l);
						if (!c)
							break;
						l -= c;
						clipText += c;
					}
					*clipText = 0;
					return cliputf8;
				}

				//failed at the last hurdle

				GlobalUnlock(clipboardhandle);
			}
		}

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
		free(cliputf8);
		cliputf8 = NULL;
		GlobalUnlock(clipboardhandle);
		CloseClipboard();
		clipboardhandle = NULL;
	}
}
void Sys_SaveClipboard(char *text)
{
	HANDLE glob;
	char *temp;
	unsigned short *tempw;
	extern cvar_t com_parseutf8;
	if (!OpenClipboard(NULL))
		return;
    EmptyClipboard();

	glob = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
    if (glob == NULL)
    {
        CloseClipboard();
        return;
    }

	if (com_parseutf8.ival > 0)
	{
		glob = GlobalAlloc(GMEM_MOVEABLE, (strlen(text) + 1)*2);
		if (glob)
		{
			tempw = GlobalLock(glob);
			if (tempw != NULL)
			{
				int error;
				while(*text)
				{
					*tempw++ = utf8_decode(&error, text, &text);
				}
				*tempw = 0;
				GlobalUnlock(glob);
				SetClipboardData(CF_UNICODETEXT, glob);
			}
			else
				GlobalFree(glob);
		}
	}
	else
	{
		//yes, quake chars will get mangled horribly.
		temp = GlobalLock(glob);
		if (temp != NULL)
		{
			strcpy(temp, text);
			GlobalUnlock(glob);
			SetClipboardData(CF_TEXT, glob);
		}
		else
			GlobalFree(glob);
	}

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

		if (WinNT)
		{
			if (!ReadConsoleInputW(hinput, recs, 1, &numread))
				Sys_Error ("Error reading console input");
		}
		else
		{
			if (!ReadConsoleInputA(hinput, recs, 1, &numread))
				Sys_Error ("Error reading console input");
		}

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.UnicodeChar;

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
							i = utf8_encode(text+len, ch, sizeof(text)-1-len);
							WriteFile(houtput, text+len, i, &dummy, NULL);
							len += i;
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

#ifndef CP_UTF8
#define CP_UTF8                   65001
#endif
qboolean Sys_InitTerminal (void)
{
	DWORD m;
	if (!AllocConsole())
		return false;

#ifndef SERVERONLY
	if (qwclsemaphore)
	{
		CloseHandle(qwclsemaphore);
		qwclsemaphore = NULL;
	}
#endif

	SetConsoleCtrlHandler (HandlerRoutine, TRUE);
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleTitle (FULLENGINENAME " dedicated server");
	hinput = GetStdHandle (STD_INPUT_HANDLE);
	houtput = GetStdHandle (STD_OUTPUT_HANDLE);

	GetConsoleMode(hinput, &m);
	SetConsoleMode(hinput, m | 0x40 | 0x80);

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

void Sys_SendKeyEvents (void)
{
    MSG        msg;

	if (isPlugin)
	{
		DWORD avail;
		static char	text[256], *nl;
		static int textpos = 0;

		HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
		if (!PeekNamedPipe(input, NULL, 0, NULL, &avail, NULL))
		{
			Cmd_ExecuteString("quit force", RESTRICT_LOCAL);
		}
		else if (avail)
		{
			if (avail > sizeof(text)-1-avail)
				avail = sizeof(text)-1-avail;
			if (ReadFile(input, text+textpos, avail, &avail, NULL))
			{
				textpos += avail;
				while(1)
				{
					text[textpos] = 0;
					nl = strchr(text, '\n');
					if (nl)
					{
						*nl++ = 0;
						if (!qrenderer && !strncmp(text, "vid_recenter ", 13))
						{
							Cmd_TokenizeString(text, false, false);
							sys_parentleft = strtoul(Cmd_Argv(1), NULL, 0);
							sys_parenttop = strtoul(Cmd_Argv(2), NULL, 0);
							sys_parentwidth = strtoul(Cmd_Argv(3), NULL, 0);
							sys_parentheight = strtoul(Cmd_Argv(4), NULL, 0); 
							sys_parentwindow = (HWND)strtoul(Cmd_Argv(5), NULL, 16);
						}
						Cmd_ExecuteString(text, RESTRICT_LOCAL);
						memmove(text, nl, textpos - (nl - text));
						textpos -= (nl - text);
					}
					else
						break;
				}
			}

		}
	}
	else if (isDedicated)
	{
#ifndef CLIENTONLY
		SV_GetConsoleCommands ();
#endif
		return;
	}

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
	// we always update if there are any event, even if we're paused
		scr_skipupdate = 0;

		//if (!GetMessage (&msg, NULL, 0, 0))
		//	break;
//			Sys_Quit ();
//		if (TranslateMessage (&msg))
//			continue;
      	DispatchMessage (&msg);
	}
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



#define COBJMACROS

#ifndef MINGW
#if _MSC_VER > 1200
	#include <shobjidl.h>
#endif
#endif
#if _MSC_VER > 1200
	#include <shlguid.h>
#endif
#include <shlobj.h>
//#include <Propsys.h>

#ifdef _MSC_VER
	#include "ntverp.h"
#endif

#if defined(__MINGW64_VERSION_MAJOR) && (__MINGW64_VERSION_MAJOR >= 3)
#define SHARD_APPIDINFOLINK SHARD_APPIDINFOLINK
#endif

#ifndef SHARD_APPIDINFOLINK

// SDK version 7600 = v7.0a & v7.1
#if !defined(VER_PRODUCTBUILD) || VER_PRODUCTBUILD < 7600
typedef struct SHARDAPPIDINFOLINK {
  IShellLinkW *psl;
  PCWSTR     pszAppID;
} SHARDAPPIDINFOLINK;
#endif

#define SHARD_APPIDINFOLINK 0x00000007

#if !defined(VER_PRODUCTBUILD) || VER_PRODUCTBUILD < 7600
typedef struct {
  GUID  fmtid;
  DWORD pid;
} PROPERTYKEY;
#endif
typedef struct IPropertyStore IPropertyStore;
;
#ifndef MINGW
#if !defined(VER_PRODUCTBUILD) || VER_PRODUCTBUILD < 7600
typedef struct IPropertyStore
{
    CONST_VTBL struct
	{
		/*IUnknown*/
		HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
				IPropertyStore * This,
				REFIID riid,
				void **ppvObject);
		ULONG ( STDMETHODCALLTYPE *AddRef )(
				IPropertyStore * This);
		ULONG ( STDMETHODCALLTYPE *Release )(
				IPropertyStore * This);

		/*property store stuff*/
		HRESULT ( STDMETHODCALLTYPE *GetCount)(
				IPropertyStore * This,
				ULONG *count);

		HRESULT  ( STDMETHODCALLTYPE *GetAt)(
				IPropertyStore * This,
				DWORD prop,
				PROPERTYKEY * key);

		HRESULT  ( STDMETHODCALLTYPE *GetValue)(
				IPropertyStore * This,
				PROPERTYKEY * key,
				PROPVARIANT * val);

		HRESULT  ( STDMETHODCALLTYPE *SetValue)(
				IPropertyStore * This,
				PROPERTYKEY * key,
				PROPVARIANT * val);

		HRESULT  ( STDMETHODCALLTYPE *Commit)(
				IPropertyStore * This);
	} *lpVtbl;
} IPropertyStore;

#endif
#endif
static const IID qIID_IPropertyStore = {0x886d8eeb, 0x8cf2, 0x4446, {0x8d, 0x02, 0xcd, 0xba, 0x1d, 0xbd, 0xcf, 0x99}};

#ifndef MINGW
#if !defined(VER_PRODUCTBUILD) || VER_PRODUCTBUILD < 7600
#define IObjectArray IUnknown 
#endif
#endif
static const IID qIID_IObjectArray = {0x92ca9dcd, 0x5622, 0x4bba, {0xa8,0x05,0x5e,0x9f,0x54,0x1b,0xd8,0xc9}};

#ifndef MINGW
#if !defined(VER_PRODUCTBUILD) || VER_PRODUCTBUILD < 7600
typedef struct IObjectCollection
{
    struct IObjectCollectionVtbl
	{
		HRESULT ( __stdcall *QueryInterface )(
			/* [in] IShellLink*/ void *This,
			/* [in] */ const GUID * const riid,
			/* [out] */ void **ppvObject);

		ULONG ( __stdcall *AddRef )(
			/* [in] IShellLink*/ void *This);

		ULONG ( __stdcall *Release )(
			/* [in] IShellLink*/ void *This);

		HRESULT ( __stdcall *GetCount )(
			/* [in] IShellLink*/ void *This,
			/* [out] */ UINT *pcObjects);

		HRESULT ( __stdcall *GetAt )(
			/* [in] IShellLink*/ void *This,
			/* [in] */ UINT uiIndex,
			/* [in] */ const GUID * const riid,
			/* [iid_is][out] */ void **ppv);

		HRESULT ( __stdcall *AddObject )(
			/* [in] IShellLink*/ void *This,
			/* [in] */ void *punk);

		HRESULT ( __stdcall *AddFromArray )(
			/* [in] IShellLink*/ void *This,
			/* [in] */ IObjectArray *poaSource);

		HRESULT ( __stdcall *RemoveObjectAt )(
			/* [in] IShellLink*/ void *This,
			/* [in] */ UINT uiIndex);

		HRESULT ( __stdcall *Clear )(
			/* [in] IShellLink*/ void *This);
	} *lpVtbl;
} IObjectCollection;
#endif
#endif
static const IID qIID_IObjectCollection = {0x5632b1a4, 0xe38a, 0x400a, {0x92,0x8a,0xd4,0xcd,0x63,0x23,0x02,0x95}};
static const CLSID qCLSID_EnumerableObjectCollection = {0x2d3468c1, 0x36a7, 0x43b6, {0xac,0x24,0xd3,0xf0,0x2f,0xd9,0x60,0x7a}};


#ifndef MINGW
#if !defined(VER_PRODUCTBUILD) || VER_PRODUCTBUILD < 7600
typedef struct ICustomDestinationList
{
	struct ICustomDestinationListVtbl
	{
		HRESULT ( __stdcall *QueryInterface ) (
			/* [in] ICustomDestinationList*/ void *This,
			/* [in] */  const GUID * const riid,
			/* [out] */ void **ppvObject);

		ULONG ( __stdcall *AddRef )(
			/* [in] ICustomDestinationList*/ void *This);

		ULONG ( __stdcall *Release )(
			/* [in] ICustomDestinationList*/ void *This);

		HRESULT ( __stdcall *SetAppID )(
			/* [in] ICustomDestinationList*/ void *This,
			/* [string][in] */ LPCWSTR pszAppID);

		HRESULT ( __stdcall *BeginList )(
			/* [in] ICustomDestinationList*/ void *This,
			/* [out] */ UINT *pcMinSlots,
			/* [in] */  const GUID * const riid,
			/* [out] */ void **ppv);

		HRESULT ( __stdcall *AppendCategory )(
			/* [in] ICustomDestinationList*/ void *This,
			/* [string][in] */ LPCWSTR pszCategory,
			/* [in] IObjectArray*/ void *poa);

		HRESULT ( __stdcall *AppendKnownCategory )(
			/* [in] ICustomDestinationList*/ void *This,
			/* [in] KNOWNDESTCATEGORY*/ int category);

		HRESULT ( __stdcall *AddUserTasks )(
			/* [in] ICustomDestinationList*/ void *This,
			/* [in] IObjectArray*/ void *poa);

		HRESULT ( __stdcall *CommitList )(
			/* [in] ICustomDestinationList*/ void *This);

		HRESULT ( __stdcall *GetRemovedDestinations )(
			/* [in] ICustomDestinationList*/ void *This,
			/* [in] */ const IID * const riid,
			/* [out] */ void **ppv);

		HRESULT ( __stdcall *DeleteList )(
			/* [in] ICustomDestinationList*/ void *This,
			/* [string][unique][in] */ LPCWSTR pszAppID);

		HRESULT ( __stdcall *AbortList )(
			/* [in] ICustomDestinationList*/ void *This);

	} *lpVtbl;
} ICustomDestinationList;
#endif
#endif
static const IID qIID_ICustomDestinationList = {0x6332debf, 0x87b5, 0x4670, {0x90,0xc0,0x5e,0x57,0xb4,0x08,0xa4,0x9e}};
static const CLSID qCLSID_DestinationList = {0x77f10cf0, 0x3db5, 0x4966, {0xb5,0x20,0xb7,0xc5,0x4f,0xd3,0x5e,0xd6}};



#endif


#if _MSC_VER > 1200
#define WIN7_APPNAME L"FTEQuake"

static IShellLinkW *CreateShellLink(char *command, char *target, char *title, char *desc)
{
	HRESULT hr;
	IShellLinkW *link;
	IPropertyStore *prop_store;

	WCHAR buf[1024];
	char tmp[1024], *s;

	// Get a pointer to the IShellLink interface.
	hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, &link);
	if (FAILED(hr))
		return NULL;

	GetModuleFileNameW(NULL, buf, sizeof(buf)/sizeof(wchar_t)-1);
	IShellLinkW_SetIconLocation(link, buf, 0);  /*grab the first icon from our exe*/
	IShellLinkW_SetPath(link, buf); /*program to run*/

	Q_strncpyz(tmp, com_quakedir, sizeof(tmp));
	/*normalize the gamedir, so we don't end up with the same thing multiple times*/
	for(s = tmp; *s; s++)
	{
		if (*s == '\\')
			*s = '/';
		else
			*s = tolower(*s);
	}
	swprintf(buf, sizeof(buf), L"%S \"%S\" -basedir \"%S\"", command, target, tmp);
	IShellLinkW_SetArguments(link, buf); /*args*/
	swprintf(buf, sizeof(buf), L"%S", desc);
	IShellLinkW_SetDescription(link, buf);  /*tooltip*/


	hr = IShellLinkW_QueryInterface(link, &qIID_IPropertyStore, &prop_store);

	#ifndef MINGW
	if(SUCCEEDED(hr))
	{
		PROPVARIANT pv;
		PROPERTYKEY PKEY_Title;
		pv.vt=VT_LPSTR;
		pv.pszVal=title; /*item text*/
		CLSIDFromString(L"{F29F85E0-4FF9-1068-AB91-08002B27B3D9}", &(PKEY_Title.fmtid));
		PKEY_Title.pid=2;
		hr = prop_store->lpVtbl->SetValue(prop_store, &PKEY_Title, &pv);
		hr = prop_store->lpVtbl->Commit(prop_store);
		prop_store->lpVtbl->Release(prop_store);
	}
	#endif

	return link;
}

void Sys_RecentServer(char *command, char *target, char *title, char *desc)
{
	SHARDAPPIDINFOLINK appinfo;
	IShellLinkW *link;

	link = CreateShellLink(command, target, title, desc);
	if (!link)
		return;

	appinfo.pszAppID=WIN7_APPNAME;
	appinfo.psl=link;
	SHAddToRecentDocs(SHARD_APPIDINFOLINK, &appinfo);
	IShellLinkW_Release(link);
}
void Win7_Init(void)
{
	HANDLE h;
	HRESULT (WINAPI *pSetCurrentProcessExplicitAppUserModelID)(PCWSTR AppID);


	h = LoadLibrary("shell32.dll");
	if (h)
	{
		pSetCurrentProcessExplicitAppUserModelID = (void*)GetProcAddress(h, "SetCurrentProcessExplicitAppUserModelID");
		if (pSetCurrentProcessExplicitAppUserModelID)
			pSetCurrentProcessExplicitAppUserModelID(WIN7_APPNAME);
	}
}

void Win7_TaskListInit(void)
{
	ICustomDestinationList *cdl;
	IObjectCollection *col;
	IObjectArray *arr;
	IShellLinkW *link;
	CoInitialize(NULL);
	if (SUCCEEDED(CoCreateInstance(&qCLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER, &qIID_ICustomDestinationList, &cdl)))
	{
		UINT minslots;
		IUnknown *removed;
		cdl->lpVtbl->BeginList(cdl, &minslots, &qIID_IObjectArray, &removed);

		if (SUCCEEDED(CoCreateInstance(&qCLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER, &qIID_IObjectCollection, &col)))
		{

			switch(M_GameType())
			{
			case MGT_QUAKE1:
				link = CreateShellLink("+menu_servers", "", "Server List", "Pick a multiplayer server to join");
				if (link)
				{
					col->lpVtbl->AddObject(col, (IUnknown*)link);
					link->lpVtbl->Release(link);
				}
				link = CreateShellLink("+map start", "", "Start New Game (Quake)", "Begin a new single-player game");
				if (link)
				{
					col->lpVtbl->AddObject(col, (IUnknown*)link);
					link->lpVtbl->Release(link);
				}
				break;
			case MGT_QUAKE2:
				link = CreateShellLink("+menu_servers", "", "Quake2 Server List", "Pick a multiplayer server to join");
				if (link)
				{
					col->lpVtbl->AddObject(col, (IUnknown*)link);
					link->lpVtbl->Release(link);
				}
				link = CreateShellLink("+map unit1", "", "Start New Game (Quake2)", "Begin a new game");
				if (link)
				{
					col->lpVtbl->AddObject(col, (IUnknown*)link);
					link->lpVtbl->Release(link);
				}
				break;
			case MGT_HEXEN2:
				link = CreateShellLink("+menu_servers", "", "Hexen2 Server List", "Pick a multiplayer server to join");
				if (link)
				{
					col->lpVtbl->AddObject(col, (IUnknown*)link);
					link->lpVtbl->Release(link);
				}
				link = CreateShellLink("+map demo1", "", "Start New Game (Hexen2)", "Begin a new game");
				if (link)
				{
					col->lpVtbl->AddObject(col, (IUnknown*)link);
					link->lpVtbl->Release(link);
				}
				break;
			}

			if (SUCCEEDED(col->lpVtbl->QueryInterface(col, &qIID_IObjectArray, &arr)))
			{
				cdl->lpVtbl->AddUserTasks(cdl, arr);
				arr->lpVtbl->Release(arr);
			}
			col->lpVtbl->Release(col);
		}
		cdl->lpVtbl->AppendKnownCategory(cdl, 1);
		cdl->lpVtbl->CommitList(cdl);
		cdl->lpVtbl->Release(cdl);
	}
}
#endif

#if defined(SVNREVISION) && !defined(MINIMAL)
	#define SVNREVISIONSTR STRINGIFY(SVNREVISION)
	#if defined(OFFICIAL_RELEASE)
		#define BUILDTYPE "rel"
	#else
		#define BUILDTYPE "test"
		#define UPDATE_URL "http://triptohell.info/moodles/"
		#define UPDATE_URL_VERSION UPDATE_URL "version.txt"
		#ifdef _WIN64
			#define UPDATE_URL_BUILD UPDATE_URL "win64/fte" EXETYPE ".exe"
		#else
			#define UPDATE_URL_BUILD UPDATE_URL "win32/fte" EXETYPE ".exe"
		#endif
	#endif
#endif

#if defined(SERVERONLY)
	#define EXETYPE "qwsv"	//not gonna happen, but whatever.
#elif defined(GLQUAKE) && defined(D3DQUAKE)
	#define EXETYPE "qw"
#elif defined(GLQUAKE)
	#ifdef MINIMAL
		#define EXETYPE "minglqw"
	#else
		#define EXETYPE "glqw"
	#endif
#elif defined(D3DQUAKE)
	#define EXETYPE "d3dqw"
#elif defined(SWQUAKE)
	#define EXETYPE "swqw"
#else
	//erm...
	#define EXETYPE "qw"
#endif


qboolean MyRegGetStringValue(HKEY base, char *keyname, char *valuename, void *data, int datalen)
{
	qboolean result = false;
	DWORD resultlen = datalen - 1;
	HKEY subkey;
	DWORD type = REG_NONE;
	if (RegOpenKeyEx(base, keyname, 0, KEY_READ, &subkey) == ERROR_SUCCESS)
	{
		result = ERROR_SUCCESS == RegQueryValueEx(subkey, valuename, NULL, &type, data, &datalen);
		RegCloseKey (subkey);
	}

	if (type == REG_SZ || type == REG_EXPAND_SZ)
		((char*)data)[datalen] = 0;
	else
		((char*)data)[0] = 0;
	return result;
}

#ifdef UPDATE_URL

void MyRegSetValue(HKEY base, char *keyname, char *valuename, int type, void *data, int datalen)
{
	HKEY subkey;
	if (RegCreateKeyEx(base, keyname, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &subkey, NULL) == ERROR_SUCCESS)
	{
		RegSetValueEx(subkey, valuename, 0, type, data, datalen);
		RegCloseKey (subkey);
	}
}
void MyRegDeleteKeyValue(HKEY base, char *keyname, char *valuename)
{
	HKEY subkey;
	if (RegOpenKeyEx(base, keyname, 0, KEY_WRITE, &subkey) == ERROR_SUCCESS)
	{
		RegDeleteValue(subkey, valuename);
		RegCloseKey (subkey);
	}
}

qboolean Update_GetHomeDirectory(char *homedir, int homedirsize)
{
	HMODULE shfolder = LoadLibrary("shfolder.dll");

	if (shfolder)
	{
		HRESULT (WINAPI *dSHGetFolderPath) (HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath);
		dSHGetFolderPath = (void *)GetProcAddress(shfolder, "SHGetFolderPathA");
		if (dSHGetFolderPath)
		{
			char folder[MAX_PATH];
			// 0x5 == CSIDL_PERSONAL
			if (dSHGetFolderPath(NULL, 0x5, NULL, 0, folder) == S_OK)
			{
				Q_snprintfz(homedir, homedirsize, "%s/My Games/%s/", folder, FULLENGINENAME);
				return true;
			}
		}
//		FreeLibrary(shfolder);
	}
	return false;
}

#include "fs.h"
void Update_Version_Updated(struct dl_download *dl)
{
	//happens in a thread, avoid va
	if (dl->file)
	{
		if (dl->status == DL_FINISHED)
		{
			char buf[8192];
			unsigned int size = 0, chunk;
			char pendingname[MAX_OSPATH];
			vfsfile_t *pending;
			Update_GetHomeDirectory(pendingname, sizeof(pendingname));
			Q_strncatz(pendingname, DISTRIBUTION BUILDTYPE EXETYPE".tmp", sizeof(pendingname));
			pending = VFSOS_Open(pendingname, "wb");
			if (pending)
			{
				while(1)
				{
					chunk = VFS_READ(dl->file, buf, sizeof(buf));
					if (!chunk)
						break;
					size += VFS_WRITE(pending, buf, chunk);
				}
				VFS_CLOSE(pending);
				if (VFS_GETLEN(dl->file) == size)
				{
					MyRegSetValue(HKEY_CURRENT_USER, "Software\\"FULLENGINENAME, "pending" BUILDTYPE EXETYPE, REG_SZ, pendingname, strlen(pendingname)+1);
				}
			}
		}
	}
}
void Update_Versioninfo_Available(struct dl_download *dl)
{
	if (dl->file)
	{
		if (dl->status == DL_FINISHED)
		{
			char linebuf[1024];
			while(VFS_GETS(dl->file, linebuf, sizeof(linebuf)))
			{
				if (!strnicmp(linebuf, "Revision: ", 10))
				{
					if (atoi(linebuf+10) > atoi(SVNREVISIONSTR))
					{
						struct dl_download *dl;
						Con_Printf("Downloading update: revision %i\n", atoi(linebuf+10));
						dl = HTTP_CL_Get(UPDATE_URL_BUILD, NULL, Update_Version_Updated);
						dl->file = FS_OpenTemp();
#ifdef MULTITHREAD
						DL_CreateThread(dl, NULL, NULL);
#endif
					}
				}
			}
		}
	}
}
static qboolean doupdatecheck;
void Update_Check(void)
{
	struct dl_download *dl;
	if (doupdatecheck)
	{
		doupdatecheck = false;
		dl = HTTP_CL_Get(UPDATE_URL_VERSION, NULL, Update_Versioninfo_Available);
		dl->file = FS_OpenTemp();
#ifdef MULTITHREAD
		DL_CreateThread(dl, NULL, NULL);
#endif
	}
}

static void	Update_CreatePath (char *path)
{
	char	*ofs;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}

qboolean Sys_CheckUpdated(void)
{
	int ffe = COM_CheckParm("--fromfrontend");
	PROCESS_INFORMATION childinfo;
	STARTUPINFO startinfo = {sizeof(startinfo)};

	if (!strcmp(SVNREVISIONSTR, "-"))
		return false;	//no revision info in this build, meaning its custom built and thus cannot check against the available updated versions.
	else if (COM_CheckParm("-noupdate") || COM_CheckParm("--noupdate"))
		return false;
	else if (!COM_CheckParm("-autoupdate") && !COM_CheckParm("--autoupdate"))
		return false;
	else if (COM_CheckParm("-plugin"))
	{
		//download, but don't invoke. the caller is expected to start us up properly (once installed).
	}
	else if (!ffe)
	{
		//if we're not from the frontend, we should run the updated build instead
		char frontendpath[MAX_OSPATH];
		char pendingpath[MAX_OSPATH];
		char updatedpath[MAX_OSPATH];

		MyRegGetStringValue(HKEY_CURRENT_USER, "Software\\"FULLENGINENAME, "pending" BUILDTYPE EXETYPE, pendingpath, sizeof(pendingpath));
		if (*pendingpath)
		{
			MyRegDeleteKeyValue(HKEY_CURRENT_USER, "Software\\"FULLENGINENAME, "pending" BUILDTYPE EXETYPE);
			Update_GetHomeDirectory(updatedpath, sizeof(updatedpath));
			Update_CreatePath(updatedpath);
			Q_strncatz(updatedpath, "cur" BUILDTYPE EXETYPE".exe", sizeof(updatedpath));
			DeleteFile(updatedpath);
			if (MoveFile(pendingpath, updatedpath))
				MyRegSetValue(HKEY_CURRENT_USER, "Software\\"FULLENGINENAME, BUILDTYPE EXETYPE, REG_SZ, updatedpath, strlen(updatedpath)+1);
		}

		MyRegGetStringValue(HKEY_CURRENT_USER, "Software\\"FULLENGINENAME, BUILDTYPE EXETYPE, updatedpath, sizeof(updatedpath));
		
		if (*updatedpath)
		{
			GetModuleFileName(NULL, frontendpath, sizeof(frontendpath)-1);
			if (CreateProcess(updatedpath, va("--fromfrontend \"%s\" \"%s\" %s", SVNREVISIONSTR, frontendpath, COM_Parse(GetCommandLineA())), NULL, NULL, TRUE, 0, NULL, NULL, &startinfo, &childinfo))
				return true;
		}
	}
	else
	{
		char frontendpath[MAX_OSPATH];
		//com_argv[ffe+1] is frontend revision
		//com_argv[ffe+2] is frontend location
		if (atoi(com_argv[ffe+1]) > atoi(SVNREVISIONSTR))
		{
			//ping-pong it back, to make sure we're running the most recent version.
			GetModuleFileName(NULL, frontendpath, sizeof(frontendpath)-1);
			if (CreateProcess(com_argv[ffe+2], va("--fromfrontend \"%s\" \"%s\" %s", "", "", COM_Parse(GetCommandLineA())), NULL, NULL, TRUE, 0, NULL, NULL, &startinfo, &childinfo))
				return true;
		}
	}
	doupdatecheck = true;
	return false;
}
#else
qboolean Sys_CheckUpdated(void)
{
	return false;
}
void Update_Check(void)
{
}
#endif

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
	char	cwd[1024], bindir[1024], *s;
	const char *qtvfile = NULL;
	int delay = 0;

	/* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

	memset(&parms, 0, sizeof(parms));

	#ifndef MINGW
	#if _MSC_VER > 1200
		Win7_Init();
	#endif
	#endif

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

#ifdef RESTARTTEST
		setjmp (restart_jmpbuf);
#endif

		GetModuleFileName(NULL, bindir, sizeof(bindir)-1);
		s = COM_SkipPath(exename);
		strcpy(exename, s);
		*s = 0;
		parms.argv = (const char **)argv;

		COM_InitArgv (parms.argc, parms.argv);

		if (Sys_CheckUpdated())
			return true;

		isPlugin = !!COM_CheckParm("-plugin");
		if (isPlugin)
		{
			printf("status Starting up!\n");
			fflush(stdout);
		}

		if (COM_CheckParm("--version") || COM_CheckParm("-v"))
		{
			printf("version: %s\n", version_string());
			return true;
		}
		if (COM_CheckParm("-outputdebugstring"))
			debugout = true;

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
		parms.binarydir = bindir;

		parms.argc = com_argc;
		parms.argv = com_argv;

	#if !defined(CLIENTONLY) && !defined(SERVERONLY)
		if (COM_CheckParm ("-dedicated"))
			isDedicated = true;
	#endif

		if (isDedicated)
		{
	#if !defined(CLIENTONLY)
			if (!Sys_InitTerminal())
				Sys_Error ("Couldn't allocate dedicated server console");
	#endif
		}

		if (!Sys_Startup_CheckMem(&parms))
			Sys_Error ("Not enough memory free; check disk space\n");


	#ifndef CLIENTONLY
		if (isDedicated)	//compleate denial to switch to anything else - many of the client structures are not initialized.
		{
			int delay;

			SV_Init (&parms);

			delay = SV_Frame()*1000;

			while (1)
			{
				if (!isDedicated)
					Sys_Error("Dedicated was cleared");
				NET_Sleep(delay, false);
				delay = SV_Frame()*1000;
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

		#ifndef MINGW
		#if _MSC_VER > 1200
		Win7_TaskListInit();
		#endif
		#endif

		if (isPlugin)
		{
			printf("status Running!\n");
			fflush(stdout);
		}

		Update_Check();

		/* main window message loop */
		while (1)
		{
			if (isDedicated)
			{
	#ifndef CLIENTONLY
				NET_Sleep(delay, false);

			// find time passed since last cycle
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
				oldtime = newtime;

				delay = 1000*SV_Frame ();
	#else
				Sys_Error("wut?");
	#endif
			}
			else
			{
	#ifndef SERVERONLY
				double sleeptime;
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
				sleeptime = Host_Frame (time);
				oldtime = newtime;

				SetHookState(sys_disableWinKeys.ival);

				/*sleep if its not yet time for a frame*/
				Sys_Sleep(sleeptime);
	#else
				Sys_Error("wut?");
	#endif
			}

#ifndef SERVERONLY
			if (fs_switchgame != -1)
			{
				SetHookState(false);

				Host_Shutdown ();

				COM_InitArgv (parms.argc, parms.argv);
				if (!Sys_Startup_CheckMem(&parms))
					return 0;
				Host_Init (&parms);
			}
#endif
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
	char *cmdline;
	FreeConsole();
	cmdline = GetCommandLine();
	while (*cmdline && *cmdline == ' ')
		cmdline++;
	if (*cmdline == '\"')
	{
		cmdline++;
		while (*cmdline && *cmdline != '\"')
			cmdline++;
		if (*cmdline == '\"')
			cmdline++;
	}
	else
	{
		while (*cmdline && *cmdline != ' ')
			cmdline++;
	}
	return WinMain(GetModuleHandle(NULL), NULL, cmdline, SW_NORMAL);
}

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

void Sys_Sleep (double seconds)
{
	Sleep(seconds * 1000);
}
