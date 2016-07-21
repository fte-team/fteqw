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


#if __STDC_VERSION__ >= 199901L || defined(__GNUC__)
	//C99 has a stdint header which hopefully contains an intptr_t
	//its optional... but if its not in there then its unlikely you'll actually be able to get the engine to a stage where it *can* load anything
	#include <stdint.h>
	#define qintptr_t intptr_t
	#define quintptr_t uintptr_t
	#define qint32_t int32_t
	#define quint32_t uint32_t
	#define qint64_t int64_t
	#define quint64_t uint64_t
#else
	#define qint32_t int
	#define quint32_t unsigned qint32_t
	#if defined(_WIN64)
		#define qintptr_t __int64
		#define FTE_WORDSIZE 64
		#define quintptr_t unsigned qintptr_t
		#define qint64_t __int64
		#define quint64_t unsigned __int64
	#elif defined(_WIN32)
		#ifndef _MSC_VER
			#define __w64
		#endif
		typedef __int32 __w64 qintptr_t;	//add __w64 if you need msvc to shut up about unsafe type conversions
		typedef unsigned __int32 __w64 quintptr_t;
//		#define qintptr_t __int32
//		#define quintptr_t unsigned qintptr_t
		#define qint64_t __int64
		#define quint64_t unsigned __int64
		#define FTE_WORDSIZE 32
	#else
		#ifdef __LP64__
			#define qintptr_t long
			#define qint64_t long
			#define FTE_WORDSIZE 64
		#elif __WORDSIZE == 64
			#define qintptr_t long long
			#define qint64_t long long
			#define FTE_WORDSIZE 64
		#else
			#define qintptr_t long
			#define qint64_t long long
			#define FTE_WORDSIZE 32
		#endif
		#define quintptr_t unsigned qintptr_t
		#define quint64_t unsigned qint64_t
	#endif
#endif

#ifndef FTE_WORDSIZE
	#ifdef __WORDSIZE
		#define FTE_WORDSIZE __WORDSIZE
	#elif defined(_WIN64)
		#define FTE_WORDSIZE 64
	#else
		#define FTE_WORDSIZE 32
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
	qbyte coordsize;
	qbyte anglesize;
#define NPQ2_ANG16				(1u<<0)
#define NPQ2_SOLID32			(1u<<1)

	qbyte flags;
	qbyte pad;
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

#define FOR_EACH_LINK(l,node) for (l = node.next ; l != &node ; l = l->next)
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
char *MSG_ReadStringBuffer (char *out, size_t outsize);
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


int MSG_ReadSize16 (sizebuf_t *sb);
void MSG_WriteSize16 (sizebuf_t *sb, int sz);
void COM_DecodeSize(int solid, float *mins, float *maxs);
int COM_EncodeSize(float *mins, float *maxs);

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
		(dest)[sizeofdest - 1] = 0;	\
	} while (0)
#define Q_strncatz2(dest, src)	Q_strncatz(dest, src, sizeof(dest))
#endif
//#define Q_strncpy Please remove all strncpys
/*#ifndef strncpy
#define strncpy Q_strncpy
#endif*/


/*replacement functions which do not care for locale in text formatting ('C' locale), or are non-standard*/
char *Q_strcasestr(const char *haystack, const char *needle);
int Q_strncasecmp (const char *s1, const char *s2, int n);
int Q_strcasecmp (const char *s1, const char *s2);
int	Q_atoi (const char *str);
float Q_atof (const char *str);
void deleetstring(char *result, const char *leet);


//============================================================================

extern	char		com_token[65536];

typedef enum {TTP_UNKNOWN, TTP_STRING, TTP_LINEENDING, TTP_RAWTOKEN, TTP_EOF} com_tokentype_t;
extern com_tokentype_t com_tokentype;

extern	qboolean	com_eof;

//these cast away the const for the return value.
//char *COM_Parse (const char *data);
#define COM_Parse(d) COM_ParseOut(d,com_token, sizeof(com_token))
#define COM_ParseOut(d,o,l) COM_ParseType(d,o,l,NULL)
char *COM_ParseType (const char *data, char *out, int outlen, com_tokentype_t *toktype);
char *COM_ParseStringSet (const char *data, char *out, size_t outlen);
char *COM_ParseCString (const char *data, char *out, size_t maxoutlen, size_t *writtenlen);
char *COM_StringParse (const char *data, char *token, unsigned int tokenlen, qboolean expandmacros, qboolean qctokenize);
char *COM_ParseToken (const char *data, const char *punctuation);
char *COM_TrimString(char *str, char *buffer, int buffersize);
const char *COM_QuotedString(const char *string, char *buf, int buflen, qboolean omitquotes);	//inverse of COM_StringParse


extern	int		com_argc;
extern	const char	**com_argv;

int COM_CheckParm (const char *parm);	//WARNING: Legacy arguments should be listed in CL_ArgumentOverrides!
int COM_CheckNextParm (const char *parm, int last);
void COM_AddParm (const char *parm);

void COM_Init (void);
void COM_InitArgv (int argc, const char **argv);
void COM_ParsePlusSets (qboolean docbuf);

typedef unsigned int conchar_t;
char *COM_DeFunString(conchar_t *str, conchar_t *stop, char *out, int outsize, qboolean ignoreflags, qboolean forceutf8);
#define PFS_KEEPMARKUP		1	//leave markup in the final string (but do parse it)
#define PFS_FORCEUTF8		2	//force utf-8 decoding
#define PFS_NOMARKUP		4	//strip markup completely
#define PFS_EZQUAKEMARKUP	8	//aim for compat with ezquake instead of q3 compat
#define PFS_CENTERED		16	//flag used by console prints (text should remain centered)
#define PFS_NONOTIFY		32	//flag used by console prints (text won't be visible other than by looking at the console)
conchar_t *COM_ParseFunString(conchar_t defaultflags, const char *str, conchar_t *out, int outsize, int keepmarkup);	//ext is usually CON_WHITEMASK, returns its null terminator
unsigned int utf8_decode(int *error, const void *in, char **out);
unsigned int utf8_encode(void *out, unsigned int unicode, int maxlen);
unsigned int iso88591_encode(char *out, unsigned int unicode, int maxlen, qboolean markup);
unsigned int qchar_encode(char *out, unsigned int unicode, int maxlen, qboolean markup);
unsigned int COM_DeQuake(unsigned int unichar);

void COM_BiDi_Shutdown(void);

//small macro to tell COM_ParseFunString (and related functions like con_printf) that the input is a utf-8 string.
#define U8(s) "^`u8:"s"`="

//handles whatever charset is active, including ^U stuff.
unsigned int unicode_byteofsfromcharofs(const char *str, unsigned int charofs, qboolean markup);
unsigned int unicode_charofsfrombyteofs(const char *str, unsigned int byteofs, qboolean markup);
unsigned int unicode_encode(char *out, unsigned int unicode, int maxlen, qboolean markup);
unsigned int unicode_decode(int *error, const void *in, char **out, qboolean markup);
size_t unicode_strtolower(const char *in, char *out, size_t outsize, qboolean markup);
size_t unicode_strtoupper(const char *in, char *out, size_t outsize, qboolean markup);
unsigned int unicode_charcount(const char *in, size_t buffersize, qboolean markup);

char *COM_SkipPath (const char *pathname);
void QDECL COM_StripExtension (const char *in, char *out, int outlen);
void COM_StripAllExtensions (const char *in, char *out, int outlen);
void COM_FileBase (const char *in, char *out, int outlen);
int QDECL COM_FileSize(const char *path);
void COM_DefaultExtension (char *path, const char *extension, int maxlen);
qboolean COM_RequireExtension(char *path, const char *extension, int maxlen);
char *COM_FileExtension (const char *in, char *result, size_t sizeofresult);
void COM_CleanUpPath(char *str);

char	*VARGS va(const char *format, ...) LIKEPRINTF(1);
// does a varargs printf into a temp buffer

//============================================================================

struct cache_user_s;

extern char	com_gamepath[MAX_OSPATH];
extern char	com_homepath[MAX_OSPATH];
extern char	com_configdir[MAX_OSPATH];	//dir to put cfg_save configs in
//extern	char	*com_basedir;

//qofs_Make is used to 'construct' a variable of qofs_t type. this is so the code can merge two 32bit ints on old systems and use a long long type internally without generating warnings about bit shifts when qofs_t is only 32bit instead.
#if 1//defined(__amd64__) || defined(_AMD64_) || __WORDSIZE == 64
	#define FS_64BIT
#endif
#ifdef FS_64BIT
	typedef quint64_t qofs_t;	//type to use for a file offset
	#define qofs_Make(low,high) (low | (((qofs_t)(high))<<32))
	#define qofs_Low(o) ((o)&0xffffffffu)
	#define qofs_High(o) ((o)>>32)
	#define qofs_Error(o) ((o) == ~0ull)
#else
	typedef quint32_t qofs_t;	//type to use for a file offset
	#define qofs_Make(low,high) (low)
	#define qofs_Low(o) (o)
	#define qofs_High(o) (0)
	#define qofs_Error(o) ((o) == ~0ul)
#endif

typedef struct searchpathfuncs_s searchpathfuncs_t;
typedef struct searchpath_s
{
	searchpathfuncs_t *handle;

	unsigned int flags;

	char logicalpath[MAX_OSPATH];	//printable hunam-readable location of the package. generally includes a system path, including nested packages.
	char purepath[256];	//server tracks the path used to load them so it can tell the client
	char prefix[MAX_QPATH];	//prefix to add to each file within the archive. may also be ".." to mean ignore the top-level path.
	int crc_check;	//client sorts packs according to this checksum
	int crc_reply;	//client sends a different crc back to the server, for the paks it's actually loaded.
	int orderkey;	//used to check to see if the paths were actually changed or not.

	struct searchpath_s *next;
	struct searchpath_s *nextpure;
} searchpath_t;
typedef struct {
	struct searchpath_s	*search;			//used to say which filesystem driver to open the file from
	int				index;					//used by the filesystem driver as a simple reference to the file
	char			rawname[MAX_OSPATH];	//blank means not readable directly
	qofs_t			offset;					//only usable if rawname is set.
	qofs_t			len;					//uncompressed length
} flocation_t;
struct vfsfile_s;

#define FSLF_IFFOUND			0		//
#define FSLF_DEEPONFAILURE		(1u<<0)	//upon failure, report that the file is so far into the filesystem as to be irrelevant
#define FSLF_DEPTH_INEXPLICIT	(1u<<1)	//depth is incremented for EVERY package, not just system/explicit paths.
#define FSLF_IGNOREBASEDEPTH	(1u<<3)	//depth is incremented for explicit mod paths, but not id1/qw/fte/paks/pk3s
#define FSLF_SECUREONLY			(1u<<4)	//ignore files from downloaded packages (ie: configs)
#define FSLF_DONTREFERENCE		(1u<<5) //don't add any reference flags to packages
#define FSLF_IGNOREPURE			(1u<<6) //use only the client's package list, ignore any lists obtained from the server (including any reordering)
#define FSLF_IGNORELINKS		(1u<<7) //ignore any pak/pk3 symlinks. system ones may still be followed.

//if loc is valid, loc->search is always filled in, the others are filled on success.
//standard return value is 0 on failure, or depth on success.
int FS_FLocateFile(const char *filename, unsigned int flags, flocation_t *loc);
struct vfsfile_s *FS_OpenReadLocation(flocation_t *location);
char *FS_WhichPackForLocation(flocation_t *loc, qboolean makereferenced);
char *FS_GetPackageDownloadFilename(flocation_t *loc);

qboolean FS_GetPackageDownloadable(const char *package);
char *FS_GetPackHashes(char *buffer, int buffersize, qboolean referencedonly);
char *FS_GetPackNames(char *buffer, int buffersize, int referencedonly, qboolean ext);
qboolean FS_GenCachedPakName(char *pname, char *crc, char *local, int llen);	//returns false if the name is invalid.
void FS_ReferenceControl(unsigned int refflag, unsigned int resetflags);

#define COM_FDepthFile(filename,ignorepacks) FS_FLocateFile(filename,FSLF_DONTREFERENCE|FSLF_DEEPONFAILURE|(ignorepacks?0:FSLF_DEPTH_INEXPLICIT), NULL)
#define COM_FCheckExists(filename) FS_FLocateFile(filename,FSLF_IFFOUND, NULL)

typedef struct vfsfile_s
{
	int (QDECL *ReadBytes) (struct vfsfile_s *file, void *buffer, int bytestoread);
	int (QDECL *WriteBytes) (struct vfsfile_s *file, const void *buffer, int bytestoread);
	qboolean (QDECL *Seek) (struct vfsfile_s *file, qofs_t pos);	//returns false for error
	qofs_t (QDECL *Tell) (struct vfsfile_s *file);
	qofs_t (QDECL *GetLen) (struct vfsfile_s *file);	//could give some lag
	qboolean (QDECL *Close) (struct vfsfile_s *file);	//returns false if there was some error.
	void (QDECL *Flush) (struct vfsfile_s *file);
	qboolean seekingisabadplan;

#ifdef _DEBUG
	char dbgname[MAX_QPATH];
#endif
} vfsfile_t;

#define VFS_CLOSE(vf) ((vf)->Close(vf))
#define VFS_TELL(vf) ((vf)->Tell(vf))
#define VFS_GETLEN(vf) ((vf)->GetLen(vf))
#define VFS_SEEK(vf,pos) ((vf)->Seek(vf,pos))
#define VFS_READ(vf,buffer,buflen) ((vf)->ReadBytes(vf,buffer,buflen))
#define VFS_WRITE(vf,buffer,buflen) ((vf)->WriteBytes(vf,buffer,buflen))
#define VFS_FLUSH(vf) do{if((vf)->Flush)(vf)->Flush(vf);}while(0)
#define VFS_PUTS(vf,s) do{const char *t=s;(vf)->WriteBytes(vf,t,strlen(t));}while(0)
char *VFS_GETS(vfsfile_t *vf, char *buffer, int buflen);
void VARGS VFS_PRINTF(vfsfile_t *vf, char *fmt, ...) LIKEPRINTF(2);

enum fs_relative{
	FS_GAME,		//standard search (not generally valid for save/rename/delete/etc)
	FS_BINARYPATH,	//for dlls and stuff
	FS_ROOT,		//./ (the root basepath or root homepath.)
	FS_GAMEONLY,	//$gamedir/
	FS_GAMEDOWNLOADCACHE,	//typically the same as FS_GAMEONLY 
	FS_BASEGAMEONLY,	//fte/
	FS_PUBGAMEONLY,		//$gamedir/ or qw/ but not fte/
	FS_PUBBASEGAMEONLY,	//qw/ (fixme: should be the last non-private basedir)
	FS_SYSTEM		//a system path. absolute paths are explicitly allowed and expected, but not required.
};

void COM_WriteFile (const char *filename, enum fs_relative fsroot, const void *data, int len);

void FS_FlushFSHashReally(qboolean domutexes);
void FS_FlushFSHashWritten(void);
void FS_FlushFSHashRemoved(void);
void FS_FlushFSHash(void);
void FS_CreatePath(const char *pname, enum fs_relative relativeto);
qboolean FS_Rename(const char *oldf, const char *newf, enum fs_relative relativeto);	//0 on success, non-0 on error
qboolean FS_Rename2(const char *oldf, const char *newf, enum fs_relative oldrelativeto, enum fs_relative newrelativeto);
qboolean FS_Remove(const char *fname, enum fs_relative relativeto);	//0 on success, non-0 on error
qboolean FS_Copy(const char *source, const char *dest, enum fs_relative relativesource, enum fs_relative relativedest);
qboolean FS_NativePath(const char *fname, enum fs_relative relativeto, char *out, int outlen);	//if you really need to fopen yourself
qboolean FS_WriteFile (const char *filename, const void *data, int len, enum fs_relative relativeto);
void *FS_MallocFile(const char *filename, enum fs_relative relativeto, qofs_t *filesize);
vfsfile_t *FS_OpenVFS(const char *filename, const char *mode, enum fs_relative relativeto);
vfsfile_t *FS_OpenTemp(void);
vfsfile_t *FS_OpenTCP(const char *name, int defaultport);

vfsfile_t *FS_OpenWithFriends(const char *fname, char *sysname, size_t sysnamesize, int numfriends, ...);

#define countof(array) (sizeof(array)/sizeof(array[0]))
#ifdef _WIN32
//windows doesn't support utf-8. Which is a shame really, because that's the charset we expect from everything.
char *narrowen(char *out, size_t outlen, wchar_t *wide);
wchar_t *widen(wchar_t *out, size_t outbytes, const char *utf8);
#define __L(x) L ## x
#define _L(x) __L(x)
int MyRegGetIntValue(void *base, const char *keyname, const char *valuename, int defaultval);
qboolean MyRegGetStringValue(void *base, const char *keyname, const char *valuename, void *data, size_t datalen);	//data is utf8
qboolean MyRegGetStringValueMultiSz(void *base, const char *keyname, const char *valuename, void *data, int datalen);
qboolean MyRegSetValue(void *base, const char *keyname, const char *valuename, int type, const void *data, int datalen);	//string values are utf8
void MyRegDeleteKeyValue(void *base, const char *keyname, const char *valuename);
#endif

void FS_UnloadPackFiles(void);
void FS_ReloadPackFiles(void);
char *FSQ3_GenerateClientPacksList(char *buffer, int maxlen, int basechecksum);
void FS_PureMode(int mode, char *purenamelist, char *purecrclist, char *refnamelist, char *refcrclist, int seed);	//implies an fs_restart. ref package names are optional, for q3 where pure names don't contain usable paths

//recursively tries to open files until it can get a zip.
vfsfile_t *CL_OpenFileInPackage(searchpathfuncs_t *search, char *name);

qbyte *QDECL COM_LoadStackFile (const char *path, void *buffer, int bufsize, size_t *fsize);
qbyte *COM_LoadTempFile (const char *path, size_t *fsize);
qbyte *COM_LoadTempMoreFile (const char *path, size_t *fsize);	//allocates a little bit more without freeing old temp
//qbyte *COM_LoadHunkFile (const char *path);

searchpathfuncs_t *COM_IteratePaths (void **iterator, char *pathbuffer, int pathbuffersize, char *dirname, int dirnamesize);
void COM_FlushFSCache(qboolean purge, qboolean domutex);	//a file was written using fopen
void COM_RefreshFSCache_f(void);
qboolean FS_Restarted(unsigned int *since);

enum manifestdeptype_e
{
	mdt_invalid,
	mdt_singlepackage,	//regular package, versioned.
	mdt_installation	//allowed to install to the root, only downloaded as part of an initial install.
};
typedef struct
{
	qboolean blockupdate;	//set to block the updateurl from being used this session. this avoids recursive updates when manifests contain the same update url.
	qboolean doinstall;		//manifest was embedded in the engine. don't assume its already installed, but ask to install it (also, enable some extra permissions for writing dlls)

	enum
	{
		MANIFEST_UNSPECIFIED=0,
		MANIFEST_CURRENTVER
	} parsever;
	int minver;	//if the engine svn revision is lower than this, the manifest will not be used as an 'upgrade'.
	int maxver;	//if not 0, the manifest will not be used
	qboolean disablehomedir;
	char *updateurl;	//url to download an updated manifest file from.
	char *updatefile;	//this is the file that needs to be written to update the manifest.
	char *installation;	//optional hardcoded commercial name, used for scanning the registry to find existing installs.
	char *formalname;	//the commercial name of the game. you'll get FULLENGINENAME otherwise.
	char *downloadsurl;	//optional installable files (menu)
	char *protocolname;	//the name used for purposes of dpmaster
	char *defaultexec;	//execed after cvars are reset, to give game-specific defaults.
	char *eula;			//when running as an installer, the user will be presented with this as a prompt
	struct
	{
		qboolean base;
		char *path;
	} gamepath[8];
	struct manpack_s
	{
		int type;
		char *path;			//the 'pure' name
		char *prefix;
		qboolean crcknown;	//if the crc was specified
		unsigned int crc;	//the public crc
		char *mirrors[8];	//a randomized (prioritized-on-load) list of mirrors to use. (may be 'prompt:game,package', 'unzip:file,url', 'xz:url', 'gz:url'
		char *condition;	//only downloaded if this cvar is set | delimited allows multiple cvars.
		int mirrornum;		//the index we last tried to download from, so we still work even if mirrors are down.
	} package[64];
} ftemanifest_t;
void FS_Manifest_Free(ftemanifest_t *man);
ftemanifest_t *FS_Manifest_Parse(const char *fname, const char *data);

void COM_InitFilesystem (void);	//does not set up any gamedirs.
qboolean FS_DownloadingPackage(void);
qboolean FS_ChangeGame(ftemanifest_t *newgame, qboolean allowreloadconfigs, qboolean allowbasedirchange);
void FS_Shutdown(void);
struct gamepacks
{
	char *path;
	char *url;
	char *subpath;	//within the package (for zips)
};
void COM_Gamedir (const char *dir, const struct gamepacks *packagespaths);
char *FS_GetGamedir(qboolean publicpathonly);
char *FS_GetBasedir(void);
char *FS_GetManifestArgs(void);
int FS_GetManifestArgv(char **argv, int maxargs);

struct zonegroup_s;
void *FS_LoadMallocGroupFile(struct zonegroup_s *ctx, char *path, size_t *fsize);
qbyte *FS_LoadMallocFile (const char *path, size_t *fsize);
qofs_t FS_LoadFile(const char *name, void **file);
void FS_FreeFile(void *file);

qbyte *COM_LoadFile (const char *path, int usehunk, size_t *filesize);

qboolean COM_LoadMapPackFile(const char *name, qofs_t offset);
void COM_FlushTempoaryPacks(void);

void COM_EnumerateFiles (const char *match, int (QDECL *func)(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath), void *parm);

extern qboolean com_installer;	//says that the engine is running in an 'installer' mode, and that the correct basedir is not yet known.
extern	struct cvar_s	registered;
extern qboolean standard_quake;	//fixme: remove

#ifdef NQPROT
void COM_Effectinfo_Enumerate(int (*cb)(const char *pname));
#endif

unsigned int COM_RemapMapChecksum(unsigned int checksum);

#define	MAX_INFO_KEY	256
char *Info_ValueForKey (const char *s, const char *key);
void Info_RemoveKey (char *s, const char *key);
char *Info_KeyForNumber (char *s, int num);
void Info_RemovePrefixedKeys (char *start, char prefix);
void Info_RemoveNonStarKeys (char *start);
void Info_SetValueForKey (char *s, const char *key, const char *value, int maxsize);
void Info_SetValueForStarKey (char *s, const char *key, const char *value, int maxsize);
void Info_Print (char *s, char *lineprefix);
void Info_WriteToFile(vfsfile_t *f, char *info, char *commandname, int cvarflags);

void Com_BlocksChecksum (int blocks, void **buffer, int *len, unsigned char *outbuf);
unsigned int Com_BlockChecksum (const void *buffer, int length);
void Com_BlockFullChecksum (const void *buffer, int len, unsigned char *outbuf);
qbyte	COM_BlockSequenceCheckByte (qbyte *base, int length, int sequence, unsigned mapchecksum);
qbyte	COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence);
qbyte	Q2COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence);

int SHA1(char *digest, int maxdigestsize, const char *string, int stringlen);
int SHA1_HMAC(unsigned char *digest, int maxdigestsize, const unsigned char *data, int datalen, const unsigned char *key, int keylen);

int version_number(void);
char *version_string(void);


void TL_InitLanguages(char *langpath);	//langpath is where the .po files can be found
void TL_Shutdown(void);
void T_FreeStrings(void);
char *T_GetString(int num);
void T_FreeInfoStrings(void);
char *T_GetInfoString(int num);

struct po_s;
const char *PO_GetText(struct po_s *po, const char *msg);
struct po_s *PO_Load(vfsfile_t *file);
void PO_Close(struct po_s *po);

//
// log.c
//
typedef enum {
	LOG_CONSOLE,
	LOG_PLAYER,
	LOG_RCON,
	LOG_TYPES
} logtype_t;
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
