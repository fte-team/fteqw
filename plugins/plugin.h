#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#ifdef FTEPLUGIN
#include "quakedef.h"
#define QPREFIX
#endif

#if !defined(NOQPREFIX) && !defined(QPREFIX)
#define QPREFIX
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
#define strncasecmp stricmp

void BadBuiltin(void);

#else

#ifdef _WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#else
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#ifndef _VM_H
	#if __STDC_VERSION__ >= 199901L || defined(__GNUC__)
		//C99 has a stdint header which hopefully contains an intptr_t
		//its optional... but if its not in there then its unlikely you'll actually be able to get the engine to a stage where it *can* load anything
		#include <stdint.h>
		#define qintptr_t intptr_t
		#define quintptr_t uintptr_t
	#else
		#ifdef _WIN64
			typedef long long qintptr_t;
			typedef unsigned long long quintptr_t;
		#else
			#if !defined(_MSC_VER) || _MSC_VER < 1300
				#define __w64
			#endif
			typedef long __w64 qintptr_t;
			typedef unsigned long __w64 quintptr_t;
		#endif
	#endif
#endif

#ifdef _WIN32
#define NATIVEEXPORT __declspec(dllexport) QDECL
#else
#define NATIVEEXPORT __attribute__((visibility("default")))
#endif


#ifndef QPREFIX
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
#ifdef QPREFIX
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
#define CHECKBUILTIN(n) ((BUILTIN_##n = (qintptr_t)pPlug_GetEngineFunction(#n)))
#define BUILTINISVALID(n) (BUILTIN_##n != 0)
#ifndef QDECL
#ifdef _WIN32
#define QDECL __cdecl
#else
#define QDECL
#endif
#endif
extern qintptr_t (QDECL *plugin_syscall)( qintptr_t arg, ... );

void Q_strlncpy(char *d, const char *s, int sizeofd, int lenofs);
void Q_strlcpy(char *d, const char *s, int n);
void Q_strlcat(char *d, const char *s, int n);
int Q_snprintf(char *buffer, size_t maxlen, const char *format, ...);
int Q_vsnprintf(char *buffer, size_t maxlen, const char *format, va_list vargs);

#endif

#ifndef NATIVEEXPORT
#define NATIVEEXPORT QDECL
#endif


#ifdef FTEPLUGIN
#define qfalse false
#define qtrue true
#else
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
typedef int qhandle_t;
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
	char userinfo[2048];
	char team[64];
} plugclientinfo_t;



//Basic builtins:
EBUILTIN(funcptr_t, Plug_GetEngineFunction, (const char *funcname));	//set up in vmMain, use this to get all other builtins

#ifdef FTEENGINE
#else
#ifndef Q3_VM
EBUILTIN(qboolean, Plug_ExportNative, (const char *funcname, void *func));	//set up in vmMain, use this to get all other builtins
EBUILTIN(void *, Plug_GetNativePointer, (const char *funcname));
#endif
EBUILTIN(void, Con_Print, (const char *text));	//on to main console.

EBUILTIN(qhandle_t, Con_POpen, (const char *conname, unsigned int flags));
EBUILTIN(void, Con_SubPrint, (const char *subname, const char *text));	//on to sub console.
EBUILTIN(void, Con_RenameSub, (const char *oldname, const char *newname));	//rename a console.
EBUILTIN(int, Con_IsActive, (const char *conname));
EBUILTIN(void, Con_SetActive, (const char *conname));
EBUILTIN(void, Con_Destroy, (const char *conname));
EBUILTIN(void, Con_NameForNum, (int connum, char *conname, int connamelen));
EBUILTIN(float, Con_GetConsoleFloat, (const char *conname, const char *attribname));
EBUILTIN(void, Con_SetConsoleFloat, (const char *conname, const char *attribname, float newvalue));
EBUILTIN(int, Con_GetConsoleString, (const char *conname, const char *attribname, const char *value, unsigned int valuesize));
EBUILTIN(void, Con_SetConsoleString, (const char *conname, const char *attribname, const char *newvalue));

EBUILTIN(void, Sys_Error, (const char *message));	//abort the entire engine.
EBUILTIN(unsigned int, Sys_Milliseconds, ());

EBUILTIN(int, Cmd_AddCommand, (const char *buffer));	//abort the entire engine.
EBUILTIN(void, Cmd_Args, (char *buffer, int bufsize));	//abort the entire engine.
EBUILTIN(void, Cmd_Argv, (int argnum, char *buffer, int bufsize));	//abort the entire engine.
EBUILTIN(int, Cmd_Argc, (void));	//abort the entire engine.
EBUILTIN(void, Cmd_AddText, (const char *text, qboolean insert));
EBUILTIN(void, Cmd_Tokenize, (const char *msg));	//abort the entire engine.

EBUILTIN(void, Cvar_SetString, (const char *name, const char *value));
EBUILTIN(void, Cvar_SetFloat, (const char *name, float value));
EBUILTIN(qboolean, Cvar_GetString, (const char *name, char *retstring, int sizeofretstring));
EBUILTIN(float, Cvar_GetFloat, (const char *name));
EBUILTIN(qhandle_t,	Cvar_Register, (const char *name, const char *defaultval, int flags, const char *grouphint));
EBUILTIN(int, Cvar_Update, (qhandle_t handle, int *modificationcount, char *stringv, float *floatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.
#ifdef FTEPLUGIN
EBUILTIN(cvar_t*, Cvar_GetNVFDG, (const char *name, const char *defaultval, unsigned int flags, const char *description, const char *groupname));
#endif

EBUILTIN(void, Plug_GetPluginName, (int plugnum, char *buffer, int bufsize));
EBUILTIN(void, LocalSound, (const char *soundname));
EBUILTIN(int, CL_GetStats, (int pnum, unsigned int *stats, int maxstats));
EBUILTIN(int, GetPlayerInfo, (int pnum, plugclientinfo_t *info));

EBUILTIN(int, LocalPlayerNumber, (void));	//deprecated
EBUILTIN(int, GetLocalPlayerNumbers, (int firstseat, int numseats, int *playernums, int *spectracks));
EBUILTIN(void, GetServerInfo, (char *info, int infolen));
EBUILTIN(void, SetUserInfo, (const char *key, const char *value));
EBUILTIN(void, GetLocationName, (const float *pos, char *buffer, int bufferlen));
#ifdef FTEPLUGIN
EBUILTIN(int, GetLastInputFrame, (int seat, usercmd_t *playercmd));
#endif
EBUILTIN(float, GetTrackerOwnFrags, (int seat, char *text, size_t textsize));

#ifndef Q3_VM
struct pubprogfuncs_s;
EBUILTIN(struct pubprogfuncs_s*, PR_GetVMInstance, (int vmid/*0=ss,1=cs,2=m*/));
struct modplugfuncs_s;
EBUILTIN(struct modplugfuncs_s*, Mod_GetPluginModelFuncs, (int version));
#ifdef MULTITHREAD
struct threading_s;
EBUILTIN(struct threading_s*, Sys_GetThreadingFuncs, (int threadingsize));
#endif
#endif

typedef struct
{
	unsigned int client;
	unsigned int items;
	float armor;
	float health;
	vec3_t org;
	char nick[16];
} teamplayerinfo_t;
EBUILTIN(int, GetTeamInfo, (teamplayerinfo_t *clients, unsigned int maxclients, int showenemies, int showself));
struct wstats_s;
EBUILTIN(int, GetWeaponStats, (int player, struct wstats_s *result, unsigned int maxresults));

typedef struct {
	int seats;
	struct
	{
		float s_avg;
		float s_mn;
		float s_mx;
		float ms_stddev;	//calculated in milliseconds for more sane numbers
		float fr_avg;
		int fr_mn;
		int fr_mx;
	} ping;
	struct
	{	//decimals
		float dropped;
		float choked;
		float invalid;
	} loss;
	float mlatency;
	float mrate;
	float vlatency;
	float vrate;
	vec3_t speed;	//player speed
	
	struct
	{
		float in_pps;
		float in_bps;
		float out_pps;
		float out_bps;
	} clrate;
	struct
	{
		float in_pps;
		float in_bps;
		float out_pps;
		float out_bps;
	} svrate;
	int capturing;	//avi capturing
} vmnetinfo_t;
EBUILTIN(int, GetNetworkInfo, (vmnetinfo_t *ni, unsigned int sizeofni));


EBUILTIN(void, Menu_Control, (int mnum));
#define MENU_CLEAR 0
#define MENU_GRAB 1
EBUILTIN(int, Key_GetKeyCode, (const char *keyname));

EBUILTIN(qhandle_t, Draw_LoadImageData, (const char *name, const char *mime, const void *data, unsigned int datasize));	//load/replace a named texture
EBUILTIN(qhandle_t, Draw_LoadImageShader, (const char *name, const char *defaultshader));	//loads a shader.
EBUILTIN(qhandle_t, Draw_LoadImage, (const char *name, qboolean iswadimage));	//wad image is ONLY for loading out of q1 gfx.wad. loads a shader.
EBUILTIN(int, Draw_Image, (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image));
EBUILTIN(int, Draw_ImageSize, (qhandle_t image, float *x, float *y));
EBUILTIN(void, Draw_Fill,	(float x, float y, float w, float h));
EBUILTIN(void, Draw_Line, (float x1, float y1, float x2, float y2));
EBUILTIN(void, Draw_Character, (int x, int y, unsigned int character));
EBUILTIN(void, Draw_String, (float x, float y, const char *string));
EBUILTIN(void, Draw_CharacterH, (float x, float y, float h, unsigned int flags, unsigned int character));
EBUILTIN(void, Draw_StringH, (float x, float y, float h, unsigned int flags, const char *string));	//returns the vpixel width of the (coloured) string, in the current (variable-width) font.
EBUILTIN(float, Draw_StringWidth, (float h, unsigned int flags, const char *string));
EBUILTIN(void, Draw_Colourpa, (int palcol, float a));
EBUILTIN(void, Draw_Colourp, (int palcol));
EBUILTIN(void, Draw_Colour3f, (float r, float g, float b));
EBUILTIN(void, Draw_Colour4f, (float r, float g, float b, float a));
EBUILTIN(void, SCR_CenterPrint, (const char *s));

EBUILTIN(void, S_RawAudio, (int sourceid, void *data, int speed, int samples, int channels, int width, float volume));

EBUILTIN(int, ReadInputBuffer, (void *inputbuffer, int buffersize));
EBUILTIN(int, UpdateInputBuffer, (void *inputbuffer, int bytes));

#if !defined(Q3_VM) && defined(FTEPLUGIN)
EBUILTIN(qboolean, VFS_Open, (const char *name, vfsfile_t **handle, const char *mode));//opens a direct vfs file. no access checks, and so can be used in threaded plugins
EBUILTIN(qboolean, FS_NativePath, (const char *name, enum fs_relative relativeto, char *out, int outlen));
#endif
EBUILTIN(int, FS_Open, (const char *name, qhandle_t *handle, int mode));
EBUILTIN(void, FS_Close, (qhandle_t handle));
EBUILTIN(int, FS_Write, (qhandle_t handle, void *data, int len));
EBUILTIN(int, FS_Read, (qhandle_t handle, void *data, int len));
EBUILTIN(int, FS_Seek, (qhandle_t handle, unsigned int offsetlow, unsigned int offsethigh));
EBUILTIN(qboolean, FS_GetLen, (qhandle_t handle, unsigned int *sizelow, unsigned int *sizehigh));

EBUILTIN(qhandle_t, Net_TCPConnect, (char *ip, int port));
EBUILTIN(qhandle_t, Net_TCPListen, (char *ip, int port, int maxcount));
EBUILTIN(qhandle_t, Net_Accept, (qhandle_t socket, char *address, int addresssize));
EBUILTIN(int, Net_Recv, (qhandle_t socket, void *buffer, int len));
EBUILTIN(int, Net_Send, (qhandle_t socket, void *buffer, int len));
EBUILTIN(void, Net_Close, (qhandle_t socket));
EBUILTIN(int, Net_SetTLSClient, (qhandle_t sock, const char *certhostname));
EBUILTIN(int, Net_GetTLSBinding, (qhandle_t sock, char *outdata, int *datalen));
#define N_WOULDBLOCK 0
#define NET_CLIENTPORT -1
#define NET_SERVERPORT -2

#ifdef Q3_VM
EBUILTIN(void, memcpy, (void *, void *, int len));
EBUILTIN(void, memmove, (void *, void *, int len));
EBUILTIN(void, memset, (void *, int, int len));
EBUILTIN(float, sqrt, (float f));
EBUILTIN(float, cos, (float f));
EBUILTIN(float, sin, (float f));
#endif
#endif

typedef qintptr_t (QDECL *export_t) (qintptr_t *args);
char	*va(const char *format, ...);
qintptr_t Plug_Init(qintptr_t *args);
qboolean Plug_Export(const char *name, export_t func);
void Con_Printf(const char *format, ...);
void Con_DPrintf(const char *format, ...);	//not a particuarly efficient implementation, so beware.
void Sys_Errorf(const char *format, ...);
void QDECL Q_strncpyz(char *d, const char *s, int n);




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
extern vmvideo_t pvid;

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
char *Plug_Info_ValueForKey (const char *s, const char *key, char *out, size_t outsize);
void Info_RemoveKey (char *s, const char *key);
void Info_RemovePrefixedKeys (char *start, char prefix);
void Info_RemoveNonStarKeys (char *start);
void Info_SetValueForKey (char *s, const char *key, const char *value, int maxsize);
void Info_SetValueForStarKey (char *s, const char *key, const char *value, int maxsize);

#ifdef __cplusplus
}
#endif

#endif
