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
// sys.h -- non-portable functions

//
// file IO
// for the most part, we use stdio.
// if your system doesn't have stdio then urm... well.
//
void Sys_mkdir (const char *path);	//not all pre-unix systems have directories (including dos 1)
qboolean Sys_rmdir (const char *path);
qboolean Sys_remove (const char *path);
qboolean Sys_Rename (const char *oldfname, const char *newfname);
qboolean Sys_GetFreeDiskSpace(const char *path, quint64_t *freespace);	//false for not-implemented or other error. path will be a system path, but may be relative (if basedir isn't properly known). path MAY be a file, or may be a slash-terminated directory.
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen, qboolean allowprompts);

//
// memory protection
//
void Sys_MakeCodeWriteable (void * startaddr, unsigned long length);

//
// system IO
//
int VARGS Sys_DebugLog(char *file, char *fmt, ...) LIKEPRINTF(2);

NORETURN void VARGS Sys_Error (const char *error, ...) LIKEPRINTF(1);
// an error will cause the entire program to exit

void VARGS Sys_Printf (char *fmt, ...) LIKEPRINTF(1);
// send text to the console
void Sys_Warn (char *fmt, ...) LIKEPRINTF(1);
//like Sys_Printf. dunno why there needs to be two of em.

void Sys_Quit (void);
void Sys_RecentServer(char *command, char *target, char *title, char *desc);
qboolean Sys_RunInstaller(void);

typedef struct {
	void **funcptr;
	char *name;
} dllfunction_t;
typedef void dllhandle_t;	//typically used as void*
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs);
void Sys_CloseLibrary(dllhandle_t *lib);
void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname);
char *Sys_GetNameForAddress(dllhandle_t *module, void *address);

qboolean LibZ_Init(void);
qboolean LibJPEG_Init(void);
qboolean LibPNG_Init(void);

qboolean Sys_RunFile(const char *fname, int nlen);

unsigned int Sys_Milliseconds (void);
double Sys_DoubleTime (void);
qboolean Sys_RandomBytes(qbyte *string, int len);

char *Sys_ConsoleInput (void);

typedef enum
{
	CBT_SELECTION,	//select-to-copy, middle-to-paste
	CBT_CLIPBOARD	//ctrl+c, ctrl+v
} clipboardtype_t;
void Sys_Clipboard_PasteText(clipboardtype_t clipboardtype, void (*callback)(void *cb, char *utf8), void *ctx);	//calls the callback once the text is available (maybe instantly). utf8 arg may be NULL if the clipboard was unavailable.
void Sys_SaveClipboard(clipboardtype_t clipboardtype, const char *text); //a stub would do nothing.

//stuff for dynamic dedicated console -> gfx and back.
void Sys_CloseTerminal (void);
qboolean Sys_InitTerminal (void);
void Con_PrintToSys(void);

void Sys_ServerActivity(void);
//make window flash on the taskbar - someone said something/connected

void Sys_SendKeyEvents (void);
// Perform Key_Event () callbacks until the input que is empty

int Sys_EnumerateFiles (const char *gpath, const char *match, int (QDECL *func)(const char *fname, qofs_t fsize, time_t modtime, void *parm, searchpathfuncs_t *spath), void *parm, searchpathfuncs_t *spath);

void Sys_Vibrate(float count);

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate);

#ifdef MULTITHREAD
#if defined(_WIN32) && defined(_DEBUG)
void Sys_SetThreadName(unsigned int dwThreadID, char *threadName);
#endif

void Sys_ThreadsInit(void);
//qboolean Sys_IsThread(void *thread);
qboolean Sys_IsMainThread(void);
qboolean Sys_IsThread(void *thread);
void *Sys_CreateThread(char *name, int (*func)(void *), void *args, int priority, int stacksize);
void Sys_WaitOnThread(void *thread);
void Sys_DetachThread(void *thread);
void Sys_ThreadAbort(void);

#define THREADP_IDLE -5
#define THREADP_NORMAL 0
#define THREADP_HIGHEST 5

void *QDECL Sys_CreateMutex(void);
qboolean Sys_TryLockMutex(void *mutex);
qboolean QDECL Sys_LockMutex(void *mutex);
qboolean QDECL Sys_UnlockMutex(void *mutex);
void QDECL Sys_DestroyMutex(void *mutex);

/* Conditional wait calls */
void *Sys_CreateConditional(void);
qboolean Sys_LockConditional(void *condv);
qboolean Sys_UnlockConditional(void *condv);
qboolean Sys_ConditionWait(void *condv);		//lock first
qboolean Sys_ConditionSignal(void *condv);		//lock first
qboolean Sys_ConditionBroadcast(void *condv);	//lock first
void Sys_DestroyConditional(void *condv);

typedef struct threading_s
{
	void *(QDECL *CreateMutex)(void);
	qboolean (QDECL *LockMutex)(void *mutex);
	qboolean (QDECL *UnlockMutex)(void *mutex);
	void (QDECL *DestroyMutex)(void *mutex);
} threading_t;

//to try to catch leaks more easily.
#ifdef USE_MSVCRT_DEBUG
void *Sys_CreateMutexNamed(char *file, int line);
#define Sys_CreateMutex() Sys_CreateMutexNamed(__FILE__, __LINE__)
#endif

#else
	#ifdef __GNUC__	//gcc complains about if (true) when these are maros. msvc complains about static not being called in headers. gah.
		static inline qboolean Sys_MutexStub(void) {return true;}
		static inline void *Sys_CreateMutex(void) {return NULL;}
		#define Sys_IsMainThread() Sys_MutexStub()
		#define Sys_DestroyMutex(m) Sys_MutexStub()
		#define Sys_IsMainThread() Sys_MutexStub()
		#define Sys_LockMutex(m) Sys_MutexStub()
		#define Sys_UnlockMutex(m) Sys_MutexStub()
		#ifndef __cplusplus
			static inline qboolean Sys_IsThread(void *thread) {return !thread;}
		#endif
	#else
		#define Sys_IsMainThread() (qboolean)(true)
		#define Sys_CreateMutex() (void*)(NULL)
		#define Sys_LockMutex(m) (qboolean)(true)
		#define Sys_UnlockMutex(m) (qboolean)(true)
		#define Sys_DestroyMutex(m) (void)0
		#define Sys_IsThread(t) (!t)
	#endif
#endif

void Sys_Sleep(double seconds);

#ifdef NPFTE
qboolean NPQTV_Sys_Startup(int argc, char *argv[]);
void NPQTV_Sys_MainLoop(void);
#endif

#define UPD_OFF 0
#define UPD_STABLE 1
#define UPD_TESTING 2

#if defined(WEBCLIENT) && defined(_WIN32) && !defined(SERVERONLY) && !defined(_XBOX)
int StartLocalServer(int close);

#define HAVEAUTOUPDATE
void Sys_SetUpdatedBinary(const char *fname);	//legacy, so old build can still deal with updates properly
qboolean Sys_EngineCanUpdate(void);				//says whether the system code is able to invoke new binaries properly
qboolean Sys_EngineWasUpdated(char *newbinary);	//invoke the given system-path binary
#else
#define Sys_EngineCanUpdate() false
#define Sys_SetUpdatedBinary(n)
#define Sys_EngineWasUpdated(n) false
#endif

void Sys_Init (void);
void Sys_Shutdown(void);

