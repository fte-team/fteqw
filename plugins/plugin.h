#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#ifdef FTEENGINE
	//included from fte itself, to borrow typedefs
#elif defined(FTEPLUGIN)
	//plugin that needs fte internals
	#include "quakedef.h"
#else
	//moderately generic plugin
	#ifdef __cplusplus
		typedef enum {qfalse, qtrue} qboolean;
	#else
		typedef enum {qfalse, qtrue} qboolean;
		#define false qfalse
		#define true qtrue
	#endif
	typedef float vec3_t[3];
	typedef unsigned char qbyte;

	#include <stdint.h>
	#define quint64_t uint64_t
	typedef quint64_t qofs_t;

	typedef struct cvar_s cvar_t;
	typedef struct usercmd_s usercmd_t;
	typedef struct vfsfile_s vfsfile_t;
	typedef struct netadr_s netadr_t;
	enum fs_relative;
	struct searchpathfuncs_s;
#endif

#ifdef _WIN32
#	ifndef strcasecmp
#		define strcasecmp stricmp
#		define strncasecmp strnicmp
#	endif
#else
#	define stricmp strcasecmp
#	define strnicmp strncasecmp
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

#ifndef NATIVEEXPORT
	#ifdef _WIN32
		#define NATIVEEXPORTPROTO __declspec(dllexport)
		#define NATIVEEXPORT NATIVEEXPORTPROTO
	#else
		#define NATIVEEXPORTPROTO
		#define NATIVEEXPORT __attribute__((visibility("default")))
	#endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

//DLLs need a wrapper to add the extra parameter and call a boring function.
#ifndef QDECL
#ifdef _WIN32
#define QDECL __cdecl
#else
#define QDECL
#endif
#endif

#ifndef LIKEPRINTF
	#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
		#define LIKEPRINTF(x) __attribute__((format(printf,x,x+1)))
	#else
		#define LIKEPRINTF(x)
	#endif
#endif

#ifndef NATIVEEXPORT
#define NATIVEEXPORT QDECL
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
	int activetime;
	int userid;
	int spectator;
	char userinfo[2048];
	char team[64];
} plugclientinfo_t;

typedef struct
{
	unsigned int client;
	unsigned int items;
	float armor;
	float health;
	vec3_t org;
	char nick[16];
} teamplayerinfo_t;

typedef struct {
	size_t structsize;
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
} plugnetinfo_t;

struct wstats_s;


#define F(t, n, args) t (QDECL *n) args;

typedef struct	//core stuff
{
	//Basic builtins:
	F(void*,	GetEngineInterface,	(const char *interfacename, size_t structsize));	//retrieve a named interface struct from the engine
	F(qboolean, ExportFunction,		(const char *funcname, funcptr_t funcptr));			//export a named function to the engine
	F(qboolean, ExportInterface,	(const char *interfacename, void *interfaceptr, size_t structsize)); //export a named interface struct to the engine
	F(qboolean, GetPluginName,		(int plugnum, char *buffer, size_t bufsize));				//query loaded plugin names. -1 == active plugin
	F(void,		Print,				(const char *message));	//print on (main) console.
	F(void,		Error,				(const char *message));	//abort the entire engine.
	F(quintptr_t,GetMilliseconds,	(void));
#define plugcorefuncs_name "Core"
} plugcorefuncs_t;

typedef struct	//subconsole handling
{
	F(qhandle_t,POpen,				(const char *conname));
	F(qboolean,	SubPrint,			(const char *subname, const char *text));	//on to sub console.
	F(qboolean,	RenameSub,			(const char *oldname, const char *newname));	//rename a console.
	F(qboolean,	IsActive,			(const char *conname));
	F(qboolean,	SetActive,			(const char *conname));
	F(qboolean,	Destroy,			(const char *conname));
	F(qboolean,	NameForNum,			(qintptr_t connum, char *conname, size_t connamelen));
	F(float,	GetConsoleFloat,	(const char *conname, const char *attribname));
	F(qboolean,	SetConsoleFloat,	(const char *conname, const char *attribname, float newvalue));
	F(qboolean,	GetConsoleString,	(const char *conname, const char *attribname, char *outvalue, size_t valuesize));
	F(qboolean,	SetConsoleString,	(const char *conname, const char *attribname, const char *newvalue));
#define plugsubconsolefuncs_name "SubConsole"
} plugsubconsolefuncs_t;

typedef struct	//console command/tokenizing/cbuf functions
{
	F(qboolean,	AddCommand,			(const char *buffer));	//Registers a console command.
	F(void,		TokenizeString,		(const char *msg));	//tokenize a string.

	F(void,		Args,				(char *buffer, int bufsize));	//Gets the extra args
	F(void,		Argv,				(int argnum, char *buffer, size_t bufsize));	//Gets a 0-based token
	F(int,		Argc,				(void));	//gets the number of tokens available.

	F(void,		AddText,			(const char *text, qboolean insert));
#define plugcmdfuncs_name "Cmd"
} plugcmdfuncs_t;

typedef struct	//console command and cbuf functions
{
	F(void,		SetString,			(const char *name, const char *value));
	F(void,		SetFloat,			(const char *name, float value));
	F(qboolean,	GetString,			(const char *name, char *retstring, quintptr_t sizeofretstring));
	F(float,	GetFloat,			(const char *name));
	F(qhandle_t,Register,			(const char *name, const char *defaultval, int flags, const char *grouphint));
	F(qboolean,	Update,				(qhandle_t handle, int *modificationcount, char *outstringv, size_t stringsize, float *outfloatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.
	F(cvar_t*,	GetNVFDG,			(const char *name, const char *defaultval, unsigned int flags, const char *description, const char *groupname));
#define plugcvarfuncs_name "Cvar"
} plugcvarfuncs_t;

typedef struct
{
	F(void,		LocalSound,			(const char *soundname, int channel, float volume));
	F(void,		RawAudio,			(int sourceid, void *data, int speed, int samples, int channels, int width, float volume));
#define plugaudiofuncs_name "Audio"
} plugaudiofuncs_t;

typedef struct	//q1 client/network info
{
	F(int,		GetStats,			(int seat, unsigned int *stats, int maxstats));
	F(void,		GetPlayerInfo,		(int seat, plugclientinfo_t *info));
	F(size_t,	GetNetworkInfo,		(plugnetinfo_t *ni, size_t sizeofni));
	F(size_t,	GetLocalPlayerNumbers,(size_t firstseat, size_t numseats, int *playernums, int *spectracks));
	F(void,		GetLocationName,	(const float *pos, char *outbuffer, size_t bufferlen));
	F(qboolean,	GetLastInputFrame,	(int seat, usercmd_t *outcmd));
	F(void,		GetServerInfo,		(char *info, size_t infolen));

	F(void,		SetUserInfo,		(int seat, const char *key, const char *value));
	//EBUILTIN(void, SCR_CenterPrint, (const char *s));

	//FIXME: does this belong here?
	F(qboolean,	MapLog_Query,		(const char *packagename, const char *mapname, float *stats));

	F(size_t,	GetTeamInfo,		(teamplayerinfo_t *clients, size_t maxclients, qboolean showenemies, int seat));
	F(int,		GetWeaponStats,		(int player, struct wstats_s *result, size_t maxresults));
	F(float,	GetTrackerOwnFrags,	(int seat, char *text, size_t textsize));
#define plugclientfuncs_name "Client"
} plugclientfuncs_t;

typedef struct	//for menu-like stuff
{
	//for menus
	F(qboolean,	SetMenuFocus,		(qboolean wantkeyfocus, const char *cursorname, float hot_x, float hot_y, float scale)); //null cursorname=relmouse, set/empty cursorname=absmouse
	F(qboolean,	HasMenuFocus,		(void));

	//for menu input
	F(int,		GetKeyCode,			(const char *keyname, int *out_modifier));
	F(const char*,GetKeyName,		(int keycode, int modifier));
	F(int,		FindKeysForCommand,(int bindmap, const char *command, int *out_keycodes, int *out_modifiers, int maxkeys));
	F(const char*,GetKeyBind,		(int bindmap, int keynum, int modifier));
	F(void,		SetKeyBind,			(int bindmap, int keycode, int modifier, const char *newbinding));

#define pluginputfuncs_name "Input"
} pluginputfuncs_t;

typedef struct	//for huds and menus alike
{
	//note: these use handles instead of shaders, to make them persistent over renderer restarts.
	F(qhandle_t,LoadImageData,	(const char *name, const char *mime, void *data, size_t datasize));	//load/replace a named texture
	F(qhandle_t,LoadImageShader,(const char *name, const char *defaultshader));	//loads a shader.
	F(qhandle_t,LoadImage,		(const char *name, qboolean iswadimage));	//wad image is ONLY for loading out of q1 gfx.wad. loads a shader.
	F(void,		UnloadImage,	(qhandle_t image));
	F(int,		Image,			(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image));
	F(int,		ImageSize,		(qhandle_t image, float *x, float *y));
	F(void,		Fill,			(float x, float y, float w, float h));
	F(void,		Line,			(float x1, float y1, float x2, float y2));
	F(void,		Character,		(float x, float y, unsigned int character));
	F(void,		String,			(float x, float y, const char *string));
	F(void,		CharacterH,		(float x, float y, float h, unsigned int flags, unsigned int character));
	F(void,		StringH,		(float x, float y, float h, unsigned int flags, const char *string));	//returns the vpixel width of the (coloured) string, in the current (variable-width) font.
	F(float,	StringWidth,	(float h, unsigned int flags, const char *string));
	F(void,		Colourpa,		(int palcol, float a));	//for legacy code
	F(void,		Colour4f,		(float r, float g, float b, float a));

	F(void,		LocalSound,		(const char *soundname, int channel, float volume));
#define plug2dfuncs_name "2D"
} plug2dfuncs_t;

typedef struct	//for plugins that need to read/write files...
{
	F(int,		Open,			(const char *name, qhandle_t *handle, int mode));
	F(void,		Close,			(qhandle_t handle));
	F(int,		Write,			(qhandle_t handle, void *data, int len));
	F(int,		Read,			(qhandle_t handle, void *data, int len));
	F(int,		Seek,			(qhandle_t handle, qofs_t offset));
	F(qboolean, GetLen,			(qhandle_t handle, qofs_t *outsize));


	F(vfsfile_t*,OpenVFS,		(const char *filename, const char *mode, enum fs_relative relativeto));		//opens a direct vfs file, without any access checks, and so can be used in threaded plugins
	F(qboolean,	NativePath,		(const char *name, enum fs_relative relativeto, char *out, int outlen));
	F(void,		EnumerateFiles,	(const char *match, int (QDECL *callback)(const char *fname, qofs_t fsize, time_t mtime, void *ctx, struct searchpathfuncs_s *package), void *ctx));
#define plugfsfuncs_name "Filesystem"
} plugfsfuncs_t;

typedef struct	//for when you need basic socket access, hopefully rare...
{
	F(qhandle_t,TCPConnect,		(const char *ip, int port));
	F(qhandle_t,TCPListen,		(const char *localip, int port, int maxcount));
	F(qhandle_t,Accept,			(qhandle_t socket, char *address, int addresssize));
	F(int,		Recv,			(qhandle_t socket, void *buffer, int len));
	F(int,		Send,			(qhandle_t socket, void *buffer, int len));
	F(int,		SendTo,			(qhandle_t handle, void *data, int datasize, netadr_t *dest));
	F(void,		Close,			(qhandle_t socket));
	F(int,		SetTLSClient,	(qhandle_t sock, const char *certhostname));		//adds a tls layer to the socket (and specifies the peer's required hostname)
	F(int,		GetTLSBinding,	(qhandle_t sock, char *outdata, int *datalen));	//to avoid MITM attacks with compromised cert authorities
	#define N_WOULDBLOCK -1
	#define N_FATALERROR -2
	#define NET_CLIENTPORT -1
	#define NET_SERVERPORT -2
#define plugnetfuncs_name "Net"
} plugnetfuncs_t;

#undef F

extern plugcorefuncs_t *plugfuncs;
extern plugcmdfuncs_t *cmdfuncs;
extern plugcvarfuncs_t *cvarfuncs;

#ifndef FTEENGINE
void Q_strlncpy(char *d, const char *s, int sizeofd, int lenofs);
void Q_strlcpy(char *d, const char *s, int n);
void Q_strlcat(char *d, const char *s, int n);
#if defined(_MSC_VER) && _MSC_VER < 2015
	int Q_snprintf(char *buffer, size_t maxlen, const char *format, ...) LIKEPRINTF(3);
	int Q_vsnprintf(char *buffer, size_t maxlen, const char *format, va_list vargs);
#else
	#define Q_snprintf snprintf
	#define Q_vsnprintf vsnprintf
#endif

char	*va(const char *format, ...);
qboolean Plug_Init(void);
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

typedef struct {
	char *name;
	char string[256];
	char *group;
	int flags;
	float value;
	qhandle_t handle;
	int modificationcount;
} vmcvar_t;

#define VMCvar_Register(cv) (cv->handle=cvarfuncs->Register(cv->name, cv->string, cv->flags, cv->group))
#define VMCvar_Update(cv) cvarfuncs->Update(handle, &cv->modcount, cv->string, sizeof(cv->string), &cv->value)
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


char *Plug_Info_ValueForKey (const char *s, const char *key, char *out, size_t outsize);
#endif

#ifdef __cplusplus
}
#endif
#endif