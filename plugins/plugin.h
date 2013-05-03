#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#ifdef FTEPLUGIN
#include "quakedef.h"
#undef snprintf
#undef vsnprintf
#endif

#ifdef Q3_VM

typedef int qintptr_t;
typedef unsigned int quintptr_t;

typedef unsigned int size_t;
typedef signed int ssize_t;

#define TESTBI 1
#ifdef TESTBI
#	define EBUILTIN(t, n, args) extern t (*n) args
#	define BUILTINR(t, n, args) t (*n) args
#	define BUILTIN(t, n, args) t (*n) args
#	define BUILTINISVALID(n) (n!=NULL && (funcptr_t)n != (funcptr_t)&BadBuiltin)
#	define CHECKBUILTIN(n) n = (funcptr_t)Plug_GetEngineFunction(#n);if (n==NULL) {n = (funcptr_t)&BadBuiltin;Con_Print("Warning: builtin "#n" is not supported by the engine\n");}
#else

//qvms just call the return value, and the engine works out which one it called.
#	define EBUILTIN(t, n, args) extern t (*n) args
#	define BUILTINR(t, n, args) t (*n) args
#	define BUILTIN(t, n, args) t (*n) args
#	define CHECKBUILTIN(n) n = (funcptr_t)Plug_GetEngineFunction(#n);
#	define BUILTINISVALID(n) (n!=NULL)
#endif

#define double float	//all floats are 32bit, qvm stuff

typedef char *va_list;
#define va_start(va,f) (va = (char *)&f + sizeof(int))
#define va_arg(va, type) (*(type *)((va += sizeof(int)) - sizeof(int)))
#define va_end(va) (va = NULL)
#define NULL (void*)0


void *malloc(int size);
void free(void *mem);
char *strstr(char *str, const char *sub);
void strlcpy(char *d, const char *s, int n);
char *strchr(char *str, char sub);

float atof(char *str);
int atoi(char *str);

#define strcasecmp stricmp

void BadBuiltin(void);

#else

#define strcasecmp stricmp

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#ifndef _VM_H
#ifdef _WIN64
typedef long long qintptr_t;
typedef unsigned long long quintptr_t;
#else
typedef long qintptr_t;
typedef unsigned long quintptr_t;
#endif
#endif

#ifndef _WIN32
#define NATIVEEXPORT __attribute__((visibility("default")))
#endif


#ifndef FTEPLUGIN
#define pPlug_GetEngineFunction Plug_GetEngineFunction
#define pCon_Print Con_Print
#define pCvar_GetFloat Cvar_GetFloat
#define pSys_Error Sys_Error
#define pPlug_ExportToEngine Plug_ExportToEngine
#endif


#ifdef __cplusplus
extern "C" {
#endif

//DLLs need a wrapper to add the extra parameter and call a boring function.
#define TEST
#ifdef FTEPLUGIN
	#define EBUILTIN(t, n, args) extern qintptr_t BUILTIN_##n; t p##n args
	#define BUILTINR(t, n, args) qintptr_t BUILTIN_##n; t p##n args {qintptr_t res; if (!BUILTINISVALID(n))pSys_Error("Builtin "#n" is not valid\n");res = plugin_syscall(BUILTIN_##n ARGNAMES); return *(t*)&res;}
	#define BUILTIN(t, n, args) qintptr_t BUILTIN_##n; t p##n args {if (!BUILTINISVALID(n))pSys_Error("Builtin "#n" is not valid\n");plugin_syscall(BUILTIN_##n ARGNAMES);}
#elif defined(TEST)
	#define EBUILTIN(t, n, args) extern qintptr_t BUILTIN_##n; t n args
	#define BUILTINR(t, n, args) qintptr_t BUILTIN_##n; t n args {qintptr_t res; if (!BUILTINISVALID(n))Sys_Error("Builtin "#n" is not valid\n");res = plugin_syscall(BUILTIN_##n ARGNAMES); return *(t*)&res;}
	#define BUILTIN(t, n, args) qintptr_t BUILTIN_##n; t n args {if (!BUILTINISVALID(n))Sys_Error("Builtin "#n" is not valid\n");plugin_syscall(BUILTIN_##n ARGNAMES);}
#else
	#define EBUILTIN(t, n, args) extern qintptr_t BUILTIN_##n; t n args
	#define BUILTINR(t, n, args) qintptr_t BUILTIN_##n; t n args {qintptr_t res = plugin_syscall(BUILTIN_##n ARGNAMES); return *(t*)&res;}
	#define BUILTIN(t, n, args) qintptr_t BUILTIN_##n; t n args {plugin_syscall(BUILTIN_##n ARGNAMES);}
#endif
#define CHECKBUILTIN(n) BUILTIN_##n = (qintptr_t)pPlug_GetEngineFunction(#n);
#define BUILTINISVALID(n) (BUILTIN_##n != 0)
#ifndef QDECL
#ifdef _WIN32
#define QDECL __cdecl
#else
#define QDECL
#endif
#endif
extern qintptr_t (*plugin_syscall)( qintptr_t arg, ... );

#ifdef _WIN32
void strlcpy(char *d, const char *s, int n);
//int snprintf(char *buffer, size_t maxlen, const char *format, ...);
#endif

#endif

#ifndef NATIVEEXPORT
#define NATIVEEXPORT
#endif


#ifndef FTEPLUGIN
#ifdef __cplusplus
typedef enum {qfalse, qtrue} qboolean;
#else
typedef enum {qfalse, qtrue} qboolean;
#define false qfalse
#define true qtrue
#endif
typedef float vec3_t[3];
typedef unsigned char qbyte;
#endif 
typedef void *qhandle_t;
typedef void* funcptr_t;


#define PLUGMAX_SCOREBOARDNAME 64
typedef struct {
	int topcolour;
	int bottomcolour;
	int frags;
	char name[PLUGMAX_SCOREBOARDNAME];
	int ping;
	int pl;
	int starttime;
	int userid;
	int spectator;
	char userinfo[1024];
	char team[8];
} plugclientinfo_t;





//Basic builtins:
EBUILTIN(funcptr_t, Plug_GetEngineFunction, (const char *funcname));	//set up in vmMain, use this to get all other builtins
#ifndef Q3_VM
EBUILTIN(qboolean, Plug_ExportNative, (const char *funcname, void *func));	//set up in vmMain, use this to get all other builtins
#endif
EBUILTIN(void, Con_Print, (const char *text));	//on to main console.

EBUILTIN(void, Con_SubPrint, (const char *subname, const char *text));	//on to sub console.
EBUILTIN(void, Con_RenameSub, (char *oldname, char *newname));	//rename a console.
EBUILTIN(int, Con_IsActive, (char *conname));
EBUILTIN(void, Con_SetActive, (char *conname));
EBUILTIN(void, Con_Destroy, (char *conname));
EBUILTIN(void, Con_NameForNum, (int connum, char *conname, int connamelen));

EBUILTIN(void, Sys_Error, (char *message));	//abort the entire engine.
EBUILTIN(unsigned int, Sys_Milliseconds, ());

EBUILTIN(int, Cmd_AddCommand, (char *buffer));	//abort the entire engine.
EBUILTIN(void, Cmd_Args, (char *buffer, int bufsize));	//abort the entire engine.
EBUILTIN(void, Cmd_Argv, (int argnum, char *buffer, int bufsize));	//abort the entire engine.
EBUILTIN(int, Cmd_Argc, (void));	//abort the entire engine.
EBUILTIN(void, Cmd_AddText, (char *text, qboolean insert));
EBUILTIN(void, Cmd_Tokenize, (char *msg));	//abort the entire engine.

EBUILTIN(void, Cvar_SetString, (char *name, char *value));
EBUILTIN(void, Cvar_SetFloat, (char *name, float value));
EBUILTIN(qboolean, Cvar_GetString, (char *name, char *retstring, int sizeofretstring));
EBUILTIN(float, Cvar_GetFloat, (char *name));
EBUILTIN(qhandle_t,	Cvar_Register, (char *name, char *defaultval, int flags, char *grouphint));
EBUILTIN(int, Cvar_Update, (qhandle_t handle, int *modificationcount, char *stringv, float *floatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.

EBUILTIN(void, GetPluginName, (int plugnum, char *buffer, int bufsize));
EBUILTIN(void, LocalSound, (char *soundname));
EBUILTIN(int, CL_GetStats, (int pnum, unsigned int *stats, int maxstats));
EBUILTIN(int, GetPlayerInfo, (int pnum, plugclientinfo_t *info));

EBUILTIN(int, LocalPlayerNumber, (void));
EBUILTIN(void, GetServerInfo, (char *info, int infolen));
EBUILTIN(void, SetUserInfo, (char *key, char *value));
EBUILTIN(void, GetLocationName, (float *pos, char *buffer, int bufferlen));

EBUILTIN(void, Menu_Control, (int mnum));
#define MENU_CLEAR 0
#define MENU_GRAB 1
EBUILTIN(int, Key_GetKeyCode, (char *keyname));

EBUILTIN(qhandle_t, Draw_LoadImage, (char *name, qboolean iswadimage));	//wad image is ONLY for loading out of q1 gfx.wad
EBUILTIN(int, Draw_Image, (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image));
EBUILTIN(void, Draw_Fill,	(float x, float y, float w, float h));
EBUILTIN(void, Draw_Line, (float x1, float y1, float x2, float y2));
EBUILTIN(void, Draw_Character, (int x, int y, unsigned int character));
EBUILTIN(void, Draw_Colourp, (int palcol));
EBUILTIN(void, Draw_Colour3f, (float r, float g, float b));
EBUILTIN(void, Draw_Colour4f, (float r, float g, float b, float a));
EBUILTIN(void, SCR_CenterPrint, (char *s));

EBUILTIN(void, S_RawAudio, (int sourceid, void *data, int speed, int samples, int channels, int width));

EBUILTIN(int, ReadInputBuffer, (void *inputbuffer, int buffersize));
EBUILTIN(int, UpdateInputBuffer, (void *inputbuffer, int bytes));

EBUILTIN(int, FS_Open, (char *name, qhandle_t *handle, int mode));
EBUILTIN(void, FS_Close, (qhandle_t handle));
EBUILTIN(int, FS_Write, (qhandle_t handle, void *data, int len));
EBUILTIN(int, FS_Read, (qhandle_t handle, void *data, int len));
EBUILTIN(int, FS_Seek, (qhandle_t handle, unsigned int offsetlow, unsigned int offsethigh));

EBUILTIN(qhandle_t, Net_TCPConnect, (char *ip, int port));
EBUILTIN(qhandle_t, Net_TCPListen, (char *ip, int port, int maxcount));
EBUILTIN(qhandle_t, Net_Accept, (qhandle_t socket, char *address, int addresssize));
EBUILTIN(int, Net_Recv, (qhandle_t socket, void *buffer, int len));
EBUILTIN(int, Net_Send, (qhandle_t socket, void *buffer, int len));
EBUILTIN(void, Net_Close, (qhandle_t socket));
#define N_WOULDBLOCK -1
#define NET_CLIENTPORT -1
#define NET_SERVERPORT -2


#if defined(_WIN32) || defined(Q3_VM)
//int vsnprintf(char *buffer, size_t maxlen, const char *format, va_list vargs);
#endif

#ifdef Q3_VM
EBUILTIN(void, memcpy, (void *, void *, int len));
EBUILTIN(void, memmove, (void *, void *, int len));
EBUILTIN(void, memset, (void *, int, int len));
EBUILTIN(float, sqrt, (float f));
EBUILTIN(float, cos, (float f));
EBUILTIN(float, sin, (float f));
#endif

typedef qintptr_t (*export_t) (qintptr_t *args);
char	*va(char *format, ...);
qintptr_t Plug_Init(qintptr_t *args);
qboolean Plug_Export(const char *name, export_t func);
void Con_Printf(const char *format, ...);
void Con_DPrintf(const char *format, ...);	//not a particuarly efficient implementation, so beware.
void Sys_Errorf(const char *format, ...);
void Q_strncpyz(char *d, const char *s, int n);




#define PLUG_SHARED_BEGIN(t,p,b)		\
 {										\
	 t *p;								\
	 char inputbuffer[8192];			\
	 *(b) = ReadInputBuffer(inputbuffer, sizeof(inputbuffer));	\
	 if (*(b))						\
		 p = (t*)inputbuffer;			\
	 else								\
		 p = NULL;
#define PLUG_SHARED_END(p,b) UpdateInputBuffer(inputbuffer, b);}


//
// qvm_api.c
//
//int vsnprintf(char *buffer, size_t maxlen, const char *format, va_list vargs);

typedef struct {
	char *name;
	char string[256];
	char *group;
	int flags;
	float value;
	qhandle_t handle;
	int modificationcount;
} vmcvar_t;

typedef struct {
	int width;
	int height;
} vmvideo_t;
#ifdef _VM_H
#define vid ohnoes
#else
extern vmvideo_t vid;
#endif

#define VMCvar_SetString(c,v)							\
	do{													\
		strcpy(c->string, v);							\
		c->value = (float)atof(v);						\
		Cvar_SetString(c->name, c->string);				\
	} while (0)
#define VMCvar_SetFloat(c,v)							\
	do {												\
		snprintf(c->string, sizeof(c->string), "%f", v);\
		c->value = (float)(v);							\
		Cvar_SetFloat(c->name, c->value);				\
	} while(0)											\


#ifndef MAX_INFO_KEY
#define	MAX_INFO_KEY	64
#endif
char *Info_ValueForKey (char *s, const char *key);
void Info_RemoveKey (char *s, const char *key);
void Info_RemovePrefixedKeys (char *start, char prefix);
void Info_RemoveNonStarKeys (char *start);
void Info_SetValueForKey (char *s, const char *key, const char *value, int maxsize);
void Info_SetValueForStarKey (char *s, const char *key, const char *value, int maxsize);

#ifdef __cplusplus
}
#endif

#endif
