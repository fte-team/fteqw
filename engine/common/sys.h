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
//

// returns the file size
// return -1 if file is not present
// the file should be in BINARY mode for stupid OSs that care
int Sys_FileOpenRead (char *path, int *hndl);

int Sys_FileOpenWrite (char *path);
void Sys_FileClose (int handle);
void Sys_FileSeek (int handle, int position);
int Sys_FileRead (int handle, void *dest, int count);
int Sys_FileWrite (int handle, void *data, int count);
int	Sys_FileTime (char *path);
void Sys_mkdir (char *path);
qboolean Sys_remove (char *path);

//
// memory protection
//
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length);

//
// system IO
//
int VARGS Sys_DebugLog(char *file, char *fmt, ...);

void VARGS Sys_Error (const char *error, ...);
// an error will cause the entire program to exit

void VARGS Sys_Printf (char *fmt, ...);
// send text to the console

void Sys_Quit (void);

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

void Sys_LowFPPrecision (void);
void Sys_HighFPPrecision (void);
void VARGS Sys_SetFPCW (void);

int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm);

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate);

#ifdef MULTITHREAD
void *Sys_CreateThread(int (*func)(void *), void *args, int stacksize);
void Sys_WaitOnThread(void *thread);

void *Sys_CreateMutex();
qboolean Sys_TryLockMutex(void *mutex);
qboolean Sys_LockMutex(void *mutex);
qboolean Sys_UnlockMutex(void *mutex);
void Sys_DestroyMutex(void *mutex);
#endif

#ifdef _WIN32
int StartLocalServer(int close);
#endif

void Sys_Init (void);
