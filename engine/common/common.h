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

#define	MAX_INFO_STRING			196	//regular quakeworld. Sickening isn't it.
#define	EXTENDED_INFO_STRING	1024
#define	MAX_SERVERINFO_STRING	1024	//standard quake has 512 here.
#define	MAX_LOCALINFO_STRING	32768

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
struct netprim_s msg_nullnetprim;

void MSG_BeginReading (struct netprim_s prim);
void MSG_ChangePrimitives(struct netprim_s prim);
int MSG_GetReadCount(void);
int MSG_ReadChar (void);
int MSG_ReadBits(int bits);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
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
#define Q_memcmp(m1, m2, c) memcmp((m1), (m2), (c))
#define Q_strcpy(d, s) strcpy((d), (s))
#define Q_strncpy(d, s, n) strncpy((d), (s), (n))
#define Q_strlen(s) ((int)strlen(s))
#define Q_strrchr(s, c) strrchr((s), (c))
#define Q_strcat(d, s) strcat((d), (s))
#define Q_strcmp(s1, s2) strcmp((s1), (s2))
#define Q_strncmp(s1, s2, n) strncmp((s1), (s2), (n))

void VARGS Q_snprintfz (char *dest, size_t size, char *fmt, ...) LIKEPRINTF(3);

#define Q_strncpyS(d, s, n) do{const char *____in=(s);char *____out=(d);int ____i; for (____i=0;*(____in); ____i++){if (____i == (n))break;*____out++ = *____in++;}if (____i < (n))*____out='\0';}while(0)	//only use this when it should be used. If undiciided, use N
#define Q_strncpyN(d, s, n) do{if (n < 0)Sys_Error("Bad length in strncpyz");Q_strncpyS((d), (s), (n));((char *)(d))[n] = '\0';}while(0)	//this'll stop me doing buffer overflows. (guarenteed to overflow if you tried the wrong size.)
//#define Q_strncpyNCHECKSIZE(d, s, n) do{if (n < 1)Sys_Error("Bad length in strncpyz");Q_strncpyS((d), (s), (n));((char *)(d))[n-1] = '\0';((char *)(d))[n] = '255';}while(0)	//This forces nothing else to be within the buffer. Should be used for testing and nothing else.
#if 0
#define Q_strncpyz(d, s, n) Q_strncpyN(d, s, (n)-1)
#else
void Q_strncpyz(char*d, const char*s, int n);
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

#ifdef _WIN32

#define Q_strcasecmp(s1, s2) _stricmp((s1), (s2))
#define Q_strncasecmp(s1, s2, n) _strnicmp((s1), (s2), (n))

#else

#define Q_strcasecmp(s1, s2) strcasecmp((s1), (s2))
#define Q_strncasecmp(s1, s2, n) strncasecmp((s1), (s2), (n))

#endif

int	Q_atoi (const char *str);
float Q_atof (const char *str);



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
char *COM_ParseCString (const char *data);
char *COM_StringParse (const char *data, qboolean expandmacros, qboolean qctokenize);
char *COM_ParseToken (const char *data, const char *punctuation);
char *COM_TrimString(char *str);


extern	int		com_argc;
extern	const char	**com_argv;

int COM_CheckParm (const char *parm);
int COM_CheckNextParm (const char *parm, int last);
void COM_AddParm (const char *parm);

void COM_Init (void);
void COM_InitArgv (int argc, const char **argv);
void COM_ParsePlusSets (void);

typedef unsigned int conchar_t;
void COM_DeFunString(conchar_t *str, char *out, int outsize, qboolean ignoreflags);
conchar_t *COM_ParseFunString(conchar_t defaultflags, const char *str, conchar_t *out, int outsize, qboolean keepmarkup);	//ext is usually CON_WHITEMASK, returns its null terminator
int COM_FunStringLength(unsigned char *str);

char *COM_SkipPath (const char *pathname);
void COM_StripExtension (const char *in, char *out, int outlen);
void COM_StripAllExtensions (char *in, char *out, int outlen);
void COM_FileBase (const char *in, char *out, int outlen);
int COM_FileSize(const char *path);
void COM_DefaultExtension (char *path, char *extension, int maxlen);
char *COM_FileExtension (const char *in);
void COM_CleanUpPath(char *str);

char	*VARGS va(char *format, ...) LIKEPRINTF(1);
// does a varargs printf into a temp buffer

//============================================================================

extern qboolean com_file_copyprotected;
extern int com_filesize;
struct cache_user_s;

extern char	com_quakedir[MAX_OSPATH];
extern char	com_homedir[MAX_OSPATH];
extern char	com_configdir[MAX_OSPATH];	//dir to put cfg_save configs in
//extern	char	*com_basedir;

void COM_WriteFile (const char *filename, const void *data, int len);
FILE *COM_WriteFileOpen (char *filename);

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

char *FS_GetPackHashes(char *buffer, int buffersize, qboolean referencedonly);
char *FS_GetPackNames(char *buffer, int buffersize, qboolean referencedonly);

int COM_FOpenFile (const char *filename, FILE **file);
int COM_FOpenWriteFile (const char *filename, FILE **file);

//#ifdef _MSC_VER	//this is enough to annoy me, without conflicting with other (more bizzare) platforms.
//#define fopen dont_use_fopen
//#endif

void COM_CloseFile (FILE *h);

#define COM_FDepthFile(filename,ignorepacks) FS_FLocateFile(filename,ignorepacks?FSLFRT_DEPTH_OSONLY:FSLFRT_DEPTH_ANYPATH, NULL)
#define COM_FCheckExists(filename) FS_FLocateFile(filename,FSLFRT_IFFOUND, NULL)


typedef struct vfsfile_s {
	int (*ReadBytes) (struct vfsfile_s *file, void *buffer, int bytestoread);
	int (*WriteBytes) (struct vfsfile_s *file, const void *buffer, int bytestoread);
	qboolean (*Seek) (struct vfsfile_s *file, unsigned long pos);	//returns false for error
	unsigned long (*Tell) (struct vfsfile_s *file);
	unsigned long (*GetLen) (struct vfsfile_s *file);	//could give some lag
	void (*Close) (struct vfsfile_s *file);
	void (*Flush) (struct vfsfile_s *file);
	qboolean seekingisabadplan;
} vfsfile_t;

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
	FS_ROOT,		//./
	FS_GAMEONLY,	//$gamedir/
	FS_CONFIGONLY,	//fte/ (should still be part of the game path)
	FS_SKINS		//qw/skins/
};

void FS_FlushFSHash(void);
void FS_CreatePath(const char *pname, enum fs_relative relativeto);
int FS_Rename(const char *oldf, const char *newf, enum fs_relative relativeto);	//0 on success, non-0 on error
int FS_Rename2(const char *oldf, const char *newf, enum fs_relative oldrelativeto, enum fs_relative newrelativeto);
int FS_Remove(const char *fname, enum fs_relative relativeto);	//0 on success, non-0 on error
qboolean FS_Copy(const char *source, const char *dest, enum fs_relative relativesource, enum fs_relative relativedest);
qboolean FS_NativePath(const char *fname, enum fs_relative relativeto, char *out, int outlen);	//if you really need to fopen yourself
qboolean FS_WriteFile (const char *filename, const void *data, int len, enum fs_relative relativeto);
vfsfile_t *FS_OpenVFS(const char *filename, const char *mode, enum fs_relative relativeto);
vfsfile_t *FS_OpenTemp(void);
vfsfile_t *FS_OpenTCP(const char *name);
void FS_UnloadPackFiles(void);
void FS_ReloadPackFiles(void);
char *FSQ3_GenerateClientPacksList(char *buffer, int maxlen, int basechecksum);


int COM_filelength (FILE *f);
qbyte *COM_LoadStackFile (const char *path, void *buffer, int bufsize);
qbyte *COM_LoadTempFile (const char *path);
qbyte *COM_LoadTempFile2 (const char *path);	//allocates a little bit more without freeing old temp
qbyte *COM_LoadHunkFile (const char *path);
void COM_LoadCacheFile (const char *path, struct cache_user_s *cu);
void FS_ForceToPure(const char *str, const char *crcs, int seed);
char *COM_GetPathInfo (int i, int *crc);
char *COM_NextPath (char *prevpath);
void COM_FlushFSCache(void);	//a file was written using fopen
void COM_RefreshFSCache_f(void);
qboolean FS_Restarted(unsigned int *since);

void COM_InitFilesystem (void);
void FS_Shutdown(void);
void COM_Gamedir (const char *dir);
char *FS_GetGamedir(void);

qbyte *FS_LoadMallocFile (const char *path);
int FS_LoadFile(char *name, void **file);
void FS_FreeFile(void *file);

qboolean COM_LoadMapPackFile(const char *name, int offset);
void COM_FlushTempoaryPacks(void);

void COM_EnumerateFiles (const char *match, int (*func)(const char *, int, void *), void *parm);

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
void Info_Print (char *s);
void Info_WriteToFile(vfsfile_t *f, char *info, char *commandname, int cvarflags);

unsigned int Com_BlockChecksum (void *buffer, int length);
void Com_BlockFullChecksum (void *buffer, int len, unsigned char *outbuf);
qbyte	COM_BlockSequenceCheckByte (qbyte *base, int length, int sequence, unsigned mapchecksum);
qbyte	COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence);
qbyte	Q2COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence);

int build_number( void );



void TL_InitLanguages(void);
void T_FreeStrings(void);
char *T_GetString(int num);

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
