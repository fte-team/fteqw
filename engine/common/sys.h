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
void Sys_mkdir (char *path);	//not all pre-unix systems have directories (including dos 1)
qboolean Sys_remove (char *path);
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen);

//
// memory protection
//
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length);

//
// system IO
//
int VARGS Sys_DebugLog(char *file, char *fmt, ...) LIKEPRINTF(2);

NORETURN void VARGS Sys_Error (const char *error, ...) LIKEPRINTF(1);
// an error will cause the entire program to exit

void VARGS Sys_Printf (char *fmt, ...) LIKEPRINTF(1);
// send text to the console

void Sys_Quit (void);
void Sys_RecentServer(char *command, char *target, char *title, char *desc);

typedef struct {
	void **funcptr;
	char *name;
} dllfunction_t;
typedef void *dllhandle_t;
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs);
void Sys_CloseLibrary(dllhandle_t *lib);
void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname);
char *Sys_GetNameForAddress(dllhandle_t *module, void *address);

qboolean LibZ_Init(void);
qboolean LibJPEG_Init(void);
qboolean LibPNG_Init(void);

unsigned int Sys_Milliseconds (void);
double Sys_DoubleTime (void);

char *Sys_ConsoleInput (void);

char *Sys_GetClipboard(void);	//A stub would return NULL
void Sys_CloseClipboard(char *buf); //a stub would do nothing
void Sys_SaveClipboard(char *text); //a stub would do nothing.

//stuff for dynamic dedicated console -> gfx and back.
void Sys_CloseTerminal (void);
qboolean Sys_InitTerminal (void);
void Con_PrintToSys(void);

void Sys_Sleep (void);
// called to yield for a little bit so as
// not to hog cpu when paused or debugging

void Sys_ServerActivity(void);
//make window flash on the taskbar - someone said something/connected

void Sys_SendKeyEvents (void);
// Perform Key_Event () callbacks until the input que is empty

int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, int, void *), void *parm);

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate);

#ifdef MULTITHREAD
void *Sys_CreateThread(int (*func)(void *), void *args, int stacksize);
void Sys_WaitOnThread(void *thread);

void *Sys_CreateMutex(void);
qboolean Sys_TryLockMutex(void *mutex);
qboolean Sys_LockMutex(void *mutex);
qboolean Sys_UnlockMutex(void *mutex);
void Sys_DestroyMutex(void *mutex);

/* Conditional wait calls */
void *Sys_CreateConditional(void);
qboolean Sys_LockConditional(void *condv);
qboolean Sys_UnlockConditional(void *condv);
qboolean Sys_ConditionWait(void *condv);
qboolean Sys_ConditionSignal(void *condv);
qboolean Sys_ConditionBroadcast(void *condv);
void Sys_DestroyConditional(void *condv);
#endif

#ifdef NPQTV
qboolean NPQTV_Sys_Startup(int argc, char *argv[]);
void NPQTV_Sys_MainLoop(void);
void NPQTV_Sys_Shutdown(void);
#endif

#ifdef _WIN32
int StartLocalServer(int close);
#endif

void Sys_Init (void);
