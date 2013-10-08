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
// comndef.h  -- general definitions

#include <stdio.h>

//make shared
#ifndef QDECL
	#ifdef _MSC_VER
		#define QDECL _cdecl
	#else
		#define QDECL
	#endif
#endif

typedef unsigned char 		qbyte;

// KJB Undefined true and false defined in SciTech's DEBUG.H header
#undef true
#undef false

#ifdef __cplusplus
typedef enum {qfalse, qtrue} qboolean;//false and true are forcivly defined.
#define true qtrue
#define false qfalse
#else
typedef enum {false, true}	qboolean;
#endif

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)

#define	BASIC_INFO_STRING			196	//regular quakeworld. Sickening isn't it.
#define	EXTENDED_INFO_STRING	1024
#define	MAX_SERVERINFO_STRING	1024	//standard quake has 512 here.
#define	MAX_LOCALINFO_STRING	32768

#ifdef SERVERONLY
#define cls_state 0
#else
#define cls_state cls.state
#endif

#ifdef CLIENTONLY
#define sv_state 0
#else
#define sv_state sv.state
#endif

struct netprim_s
{
	int coordsize;
	int anglesize;
};
//============================================================================

typedef enum {
	SZ_BAD,
	SZ_RAWBYTES,
	SZ_RAWBITS,
	SZ_HUFFMAN	//q3 style packets are horrible.
} sbpacking_t;
typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Sys_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	qbyte	*data;
	int		maxsize;
	int		cursize;
	int packing;
	int currentbit;

	struct netprim_s prim;
} sizebuf_t;

void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, const void *data, int length);
void SZ_Print (sizebuf_t *buf, const char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;


void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((qbyte *)l - (qbyte*)&(((t *)0)->m)))

//============================================================================

#ifndef NULL
#define NULL ((void *)0)
#endif

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT	((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT 	((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

//============================================================================

extern	qboolean		bigendian;

extern	short	(*BigShort) (short l);
extern	short	(*LittleShort) (short l);
extern	int	(*BigLong) (int l);
extern	int	(*LittleLong) (int l);
extern	float	(*BigFloat) (float l);
extern	float	(*LittleFloat) (float l);

short   ShortSwap (short l);
int    LongSwap (int l);

void COM_CharBias (signed char *c, int size);
void COM_SwapLittleShortBlock (short *s, int size);

//============================================================================

struct usercmd_s;

extern struct usercmd_s nullcmd;

typedef union {	//note: reading from packets can be misaligned
	char b[4];
	short b2;
	int b4;
	float f;
} coorddata;
float MSG_FromCoord(coorddata c, int bytes);
coorddata MSG_ToCoord(float f, int bytes);
coorddata MSG_ToAngle(float f, int bytes);

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteEntity (sizebuf_t *sb, unsigned int e);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, const char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteBigCoord (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);
void MSG_WriteAngle8 (sizebuf_t *sb, float f);
void MSG_WriteAngle16 (sizebuf_t *sb, float f);
void MSG_WriteDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd);
void MSG_WriteDir (sizebuf_t *sb, float *dir);

extern	int			msg_readcount;
extern	qboolean	msg_badread;		// set if a read goes beyond end of message
extern struct netprim_s msg_nullnetprim;

void MSG_BeginReading (struct netprim_s prim);
void MSG_ChangePrimitives(struct netprim_s prim);
int MSG_GetReadCount(void);
int MSG_ReadChar (void);
int MSG_ReadBits(int bits);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
struct client_s;
unsigned int MSGSV_ReadEntity (struct client_s *fromclient);
unsigned int MSGCL_ReadEntity (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);
char *MSG_ReadStringLine (void);

float MSG_ReadCoord (void);
void MSG_ReadPos (float *pos);
float MSG_ReadAngle (void);
float MSG_ReadAngle16 (void);
void MSG_ReadDeltaUsercmd (struct usercmd_s *from, struct usercmd_s *cmd);
void MSGQ2_ReadDeltaUsercmd (struct usercmd_s *from, struct usercmd_s *move);
void MSG_ReadData (void *data, int len);
void MSG_ReadSkip (int len);

//============================================================================

char *Q_strcpyline(char *out, const char *in, int maxlen);	//stops at '\n' (and '\r')

void Q_ftoa(char *str, float in);
char *Q_strlwr(char *str);
int wildcmp(const char *wild, const char *string);	//1 if match

#define Q_memset(d, f, c) memset((d), (f), (c))
#define Q_memcpy(d, s, c) memcpy((d), (s), (c))
#define Q_memmove(d, s, c) memmove((d), (s), (c))
#define Q_memcmp(m1, m2, c) memcmp((m1), (m2), (c))
#define Q_strcpy(d, s) strcpy((d), (s))
#define Q_strncpy(d, s, n) strncpy((d), (s), (n))
#define Q_strlen(s) ((int)strlen(s))
#define Q_strrchr(s, c) strrchr((s), (c))
#define Q_strcat(d, s) strcat((d), (s))
#define Q_strcmp(s1, s2) strcmp((s1), (s2))
#define Q_strncmp(s1, s2, n) strncmp((s1), (s2), (n))

void VARGS Q_snprintfz (char *dest, size_t size, const char *fmt, ...) LIKEPRINTF(3);
void VARGS Q_vsnprintfz (char *dest, size_t size, const char *fmt, va_list args);
int VARGS Com_sprintf(char *buffer, int size, const char *format, ...) LIKEPRINTF(3);

#define Q_strncpyS(d, s, n) do{const char *____in=(s);char *____out=(d);int ____i; for (____i=0;*(____in); ____i++){if (____i == (n))break;*____out++ = *____in++;}if (____i < (n))*____out='\0';}while(0)	//only use this when it should be used. If undiciided, use N
#define Q_strncpyN(d, s, n) do{if (n < 0)Sys_Error("Bad length in strncpyz");Q_strncpyS((d), (s), (n));((char *)(d))[n] = '\0';}while(0)	//this'll stop me doing buffer overflows. (guarenteed to overflow if you tried the wrong size.)
//#define Q_strncpyNCHECKSIZE(d, s, n) do{if (n < 1)Sys_Error("Bad length in strncpyz");Q_strncpyS((d), (s), (n));((char *)(d))[n-1] = '\0';((char *)(d))[n] = '255';}while(0)	//This forces nothing else to be within the buffer. Should be used for testing and nothing else.
#if 0
#define Q_strncpyz(d, s, n) Q_strncpyN(d, s, (n)-1)
#else
void QDECL Q_strncpyz(char*d, const char*s, int n);
#define Q_strncatz(dest, src, sizeofdest)	\
	do {	\
		strncat(dest, src, sizeofdest - strlen(dest) - 1);	\
		dest[sizeofdest - 1] = 0;	\
	} while (0)
#define Q_strncatz2(dest, src)	Q_strncatz(dest, src, sizeof(dest))
#endif
//#define Q_strncpy Please remove all strncpys
/*#ifndef strncpy
#define strncpy Q_strncpy
#endif*/


/*replacement functions which do not care for locale in text formatting ('C' locale)*/
int Q_strncasecmp (const char *s1, const char *s2, int n);
int Q_strcasecmp (const char *s1, const char *s2);
int	Q_atoi (const char *str);
float Q_atof (const char *str);
void deleetstring(char *result, char *leet);


//============================================================================

extern	char		com_token[1024];

typedef enum {TTP_UNKNOWN, TTP_STRING, TTP_LINEENDING} com_tokentype_t;
extern com_tokentype_t com_tokentype;

extern	qboolean	com_eof;

//these cast away the const for the return value.
//char *COM_Parse (const char *data);
#define COM_Parse(d) COM_ParseOut(d,com_token, sizeof(com_token))
char *COM_ParseOut (const char *data, char *out, int outlen);
char *COM_ParseStringSet (const char *data);
char *COM_ParseCString (const char *data, char *out, int outlen);
char *COM_StringParse (const char *data, char *token, unsigned int tokenlen, qboolean expandmacros, qboolean qctokenize);
char *COM_ParseToken (const char *data, const char *punctuation);
char *COM_TrimString(char *str);
const char *COM_QuotedString(const char *string, char *buf, int buflen);	//inverse of COM_StringParse


extern	int		com_argc;
extern	const char	**com_argv;

int COM_CheckParm (const char *parm);	//WARNING: Legacy arguments should be listed in CL_ArgumentOverrides!
int COM_CheckNextParm (const char *parm, int last);
void COM_AddParm (const char *parm);

void COM_Init (void);
void COM_InitArgv (int argc, const char **argv);
void COM_ParsePlusSets (void);

typedef unsigned int conchar_t;
char *COM_DeFunString(conchar_t *str, conchar_t *stop, char *out, int outsize, qboolean ignoreflags);
#define PFS_KEEPMARKUP 1	//leave markup in the final string (but do parse it)
#define PFS_FORCEUTF8 2		//force utf-8 decoding
#define PFS_NOMARKUP 4		//strip markup completely
conchar_t *COM_ParseFunString(conchar_t defaultflags, const char *str, conchar_t *out, int outsize, int keepmarkup);	//ext is usually CON_WHITEMASK, returns its null terminator
unsigned int utf8_decode(int *error, const void *in, char **out);
unsigned int utf8_encode(void *out, unsigned int unicode, int maxlen);
unsigned int iso88591_encode(char *out, unsigned int unicode, int maxlen);
unsigned int qchar_encode(char *out, unsigned int unicode, int maxlen);
unsigned int COM_DeQuake(conchar_t chr);

//handles whatever charset is active, including ^U stuff.
unsigned int unicode_byteofsfromcharofs(char *str, unsigned int charofs);
unsigned int unicode_charofsfrombyteofs(char *str, unsigned int byteofs);
unsigned int unicode_encode(char *out, unsigned int unicode, int maxlen);
unsigned int unicode_decode(int *error, const void *in, char **out);
size_t unicode_strtolower(char *in, char *out, size_t outsize);
size_t unicode_strtoupper(char *in, char *out, size_t outsize);
unsigned int unicode_charcount(char *in, size_t buffersize);

char *COM_SkipPath (const char *pathname);
void COM_StripExtension (const char *in, char *out, int outlen);
void COM_StripAllExtensions (char *in, char *out, int outlen);
void COM_FileBase (const char *in, char *out, int outlen);
int QDECL COM_FileSize(const char *path);
void COM_DefaultExtension (char *path, char *extension, int maxlen);
char *COM_FileExtension (const char *in);
void COM_CleanUpPath(char *str);

char	*VARGS va(char *format, ...) LIKEPRINTF(1);
// does a varargs printf into a temp buffer

//============================================================================

extern qboolean com_file_copyprotected;
extern int com_filesize;
extern qboolean com_file_untrusted;
struct cache_user_s;

extern char	com_quakedir[MAX_OSPATH];
extern char	com_homedir[MAX_OSPATH];
extern char	com_configdir[MAX_OSPATH];	//dir to put cfg_save configs in
//extern	char	*com_basedir;

void COM_WriteFile (const char *filename, const void *data, int len);

typedef struct {
	struct searchpath_s	*search;
	int				index;
	char			rawname[MAX_OSPATH];
	int				offset;
	int				len;
} flocation_t;
struct vfsfile_s;

typedef enum {FSLFRT_IFFOUND, FSLFRT_LENGTH, FSLFRT_DEPTH_OSONLY, FSLFRT_DEPTH_ANYPATH} FSLF_ReturnType_e;
//if loc is valid, loc->search is always filled in, the others are filled on success.
//returns -1 if couldn't find.
int FS_FLocateFile(const char *filename, FSLF_ReturnType_e returntype, flocation_t *loc);
struct vfsfile_s *FS_OpenReadLocation(flocation_t *location);
char *FS_WhichPackForLocation(flocation_t *loc);

qboolean FS_GetPackageDownloadable(const char *package);
char *FS_GetPackHashes(char *buffer, int buffersize, qboolean referencedonly);
char *FS_GetPackNames(char *buffer, int buffersize, int referencedonly, qboolean ext);
qboolean FS_GenCachedPakName(char *pname, char *crc, char *local, int llen);	//returns false if the name is invalid.
void FS_ReferenceControl(unsigned int refflag, unsigned int resetflags);

FTE_DEPRECATED int COM_FOpenFile (const char *filename, FILE **file);
FTE_DEPRECATED int COM_FOpenWriteFile (const char *filename, FILE **file);

//#ifdef _MSC_VER	//this is enough to annoy me, without conflicting with other (more bizzare) platforms.
//#define fopen dont_use_fopen
//#endif

FTE_DEPRECATED void COM_CloseFile (FILE *h);

#define COM_FDepthFile(filename,ignorepacks) FS_FLocateFile(filename,ignorepacks?FSLFRT_DEPTH_OSONLY:FSLFRT_DEPTH_ANYPATH, NULL)
#define COM_FCheckExists(filename) FS_FLocateFile(filename,FSLFRT_IFFOUND, NULL)


typedef struct vfsfile_s
{
	int (QDECL *ReadBytes) (struct vfsfile_s *file, void *buffer, int bytestoread);
	int (QDECL *WriteBytes) (struct vfsfile_s *file, const void *buffer, int bytestoread);
	qboolean (QDECL *Seek) (struct vfsfile_s *file, unsigned long pos);	//returns false for error
	unsigned long (QDECL *Tell) (struct vfsfile_s *file);
	unsigned long (QDECL *GetLen) (struct vfsfile_s *file);	//could give some lag
	void (QDECL *Close) (struct vfsfile_s *file);
	void (QDECL *Flush) (struct vfsfile_s *file);
	qboolean seekingisabadplan;

#ifdef _DEBUG
	char dbgname[MAX_QPATH];
#endif
} vfsfile_t;
typedef struct searchpathfuncs_s searchpathfuncs_t;

#define VFS_CLOSE(vf) (vf->Close(vf))
#define VFS_TELL(vf) (vf->Tell(vf))
#define VFS_GETLEN(vf) (vf->GetLen(vf))
#define VFS_SEEK(vf,pos) (vf->Seek(vf,pos))
#define VFS_READ(vf,buffer,buflen) (vf->ReadBytes(vf,buffer,buflen))
#define VFS_WRITE(vf,buffer,buflen) (vf->WriteBytes(vf,buffer,buflen))
#define VFS_FLUSH(vf) do{if(vf->Flush)vf->Flush(vf);}while(0)
#define VFS_PUTS(vf,s) do{const char *t=s;vf->WriteBytes(vf,t,strlen(t));}while(0)
char *VFS_GETS(vfsfile_t *vf, char *buffer, int buflen);
void VARGS VFS_PRINTF(vfsfile_t *vf, char *fmt, ...) LIKEPRINTF(2);

enum fs_relative{
	FS_GAME,		//standard search (not generally valid for save/rename/delete/etc)
	FS_BINARYPATH,	//for dlls and stuff
	FS_ROOT,		//./
	FS_GAMEONLY,	//$gamedir/
	FS_GAMEDOWNLOADCACHE,	//typically the same as FS_GAMEONLY 
	FS_CONFIGONLY,	//fte/ (should still be part of the game path)
	FS_SKINS		//qw/skins/
};

void FS_FlushFSHashReally(void);
void FS_FlushFSHashWritten(void);
void FS_FlushFSHashRemoved(void);
void FS_CreatePath(const char *pname, enum fs_relative relativeto);
qboolean FS_Rename(const char *oldf, const char *newf, enum fs_relative relativeto);	//0 on success, non-0 on error
qboolean FS_Rename2(const char *oldf, const char *newf, enum fs_relative oldrelativeto, enum fs_relative newrelativeto);
qboolean FS_Remove(const char *fname, enum fs_relative relativeto);	//0 on success, non-0 on error
qboolean FS_Copy(const char *source, const char *dest, enum fs_relative relativesource, enum fs_relative relativedest);
qboolean FS_NativePath(const char *fname, enum fs_relative relativeto, char *out, int outlen);	//if you really need to fopen yourself
qboolean FS_WriteFile (const char *filename, const void *data, int len, enum fs_relative relativeto);
vfsfile_t *FS_OpenVFS(const char *filename, const char *mode, enum fs_relative relativeto);
vfsfile_t *FS_OpenTemp(void);
vfsfile_t *FS_OpenTCP(const char *name, int defaultport);

void FS_UnloadPackFiles(void);
void FS_ReloadPackFiles(void);
char *FSQ3_GenerateClientPacksList(char *buffer, int maxlen, int basechecksum);
void FS_PureMode(int mode, char *packagelist, char *crclist, int seed);	//implies an fs_restart


qbyte *QDECL COM_LoadStackFile (const char *path, void *buffer, int bufsize);
qbyte *COM_LoadTempFile (const char *path);
qbyte *COM_LoadTempMoreFile (const char *path);	//allocates a little bit more without freeing old temp
//qbyte *COM_LoadHunkFile (const char *path);
qbyte *COM_LoadMallocFile (const char *path);

searchpathfuncs_t *COM_IteratePaths (void **iterator, char *buffer, int buffersize);
void COM_FlushFSCache(void);	//a file was written using fopen
void COM_RefreshFSCache_f(void);
qboolean FS_Restarted(unsigned int *since);

typedef struct
{
	char *updateurl;	//url to download an updated manifest file from.
	char *installation;	//optional hardcoded commercial name, used for scanning the registry to find existing installs.
	char *formalname;	//the commercial name of the game. you'll get FULLENGINENAME otherwise.
	char *protocolname;	//the name used for purposes of dpmaster
	char *defaultexec;	//execed after cvars are reset, to give game-specific defaults.
	struct
	{
		qboolean base;
		char *path;
	} gamepath[8];
	struct
	{
		char *path;			//the 'pure' name
		qboolean crcknown;	//if the crc was specified
		unsigned int crc;	//the public crc
		char *mirrors[8];	//a randomized (prioritized) list of http mirrors to use.
		int mirrornum;		//the index we last tried to download from, so we still work even if mirrors are down.
	} package[64];
} ftemanifest_t;
void FS_Manifest_Free(ftemanifest_t *man);
ftemanifest_t *FS_Manifest_Parse(const char *data);

void COM_InitFilesystem (void);	//does not set up any gamedirs.
qboolean FS_ChangeGame(ftemanifest_t *newgame, qboolean allowreloadconfigs);
void FS_Shutdown(void);
void COM_Gamedir (const char *dir);
char *FS_GetGamedir(void);
char *FS_GetBasedir(void);

struct zonegroup_s;
void *FS_LoadMallocGroupFile(struct zonegroup_s *ctx, char *path);
qbyte *FS_LoadMallocFile (const char *path);
int FS_LoadFile(char *name, void **file);
void FS_FreeFile(void *file);

qbyte *COM_LoadFile (const char *path, int usehunk);

qboolean COM_LoadMapPackFile(const char *name, int offset);
void COM_FlushTempoaryPacks(void);

void COM_EnumerateFiles (const char *match, int (QDECL *func)(const char *fname, int fsize, void *parm, searchpathfuncs_t *spath), void *parm);

extern	struct cvar_s	registered;
extern qboolean standard_quake;	//fixme: remove

void COM_Effectinfo_Clear(void);
unsigned int COM_Effectinfo_ForName(const char *efname);
char *COM_Effectinfo_ForNumber(unsigned int efnum);

unsigned int COM_RemapMapChecksum(unsigned int checksum);

#define	MAX_INFO_KEY	256
char *Info_ValueForKey (char *s, const char *key);
void Info_RemoveKey (char *s, const char *key);
char *Info_KeyForNumber (char *s, int num);
void Info_RemovePrefixedKeys (char *start, char prefix);
void Info_RemoveNonStarKeys (char *start);
void Info_SetValueForKey (char *s, const char *key, const char *value, int maxsize);
void Info_SetValueForStarKey (char *s, const char *key, const char *value, int maxsize);
void Info_Print (char *s, char *lineprefix);
void Info_WriteToFile(vfsfile_t *f, char *info, char *commandname, int cvarflags);

void Com_BlocksChecksum (int blocks, void **buffer, int *len, unsigned char *outbuf);
unsigned int Com_BlockChecksum (void *buffer, int length);
void Com_BlockFullChecksum (void *buffer, int len, unsigned char *outbuf);
qbyte	COM_BlockSequenceCheckByte (qbyte *base, int length, int sequence, unsigned mapchecksum);
qbyte	COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence);
qbyte	Q2COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence);

int SHA1(char *digest, int maxdigestsize, char *string, int stringlen);
int SHA1_HMAC(unsigned char *digest, int maxdigestsize, unsigned char *data, int datalen, unsigned char *key, int keylen);

int version_number(void);
char *version_string(void);


void TL_InitLanguages(void);
void TL_Shutdown(void);
void T_FreeStrings(void);
char *T_GetString(int num);
void T_FreeInfoStrings(void);
char *T_GetInfoString(int num);

//
// log.c
//
typedef enum {
	LOG_CONSOLE,
	LOG_PLAYER,
	LOG_TYPES
} logtype_t;
void Log_Dir_Callback (struct cvar_s *var, char *oldvalue);
void Log_Name_Callback (struct cvar_s *var, char *oldvalue);
void Log_String (logtype_t lognum, char *s);
void Con_Log (char *s);
void Log_Logfile_f (void);
void Log_Init(void);


/*used by and for botlib and q3 gamecode*/
#define MAX_TOKENLENGTH		1024
typedef struct pc_token_s
{
	int type;
	int subtype;
	int intvalue;
	float floatvalue;
	char string[MAX_TOKENLENGTH];
} pc_token_t;
#define fileHandle_t int
#define fsMode_t int


typedef struct
{
	int sec;
	int min;
	int hour;
	int day;
	int mon;
	int year;
	char str[128];
} date_t;
void COM_TimeOfDay(date_t *date);
