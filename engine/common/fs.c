#include "quakedef.h"
#include "netinc.h"

//#define com_gamedir com__gamedir

#include <ctype.h>
#include <limits.h>

#include "fs.h"
#include "shader.h"
#ifdef _WIN32
#include "winquake.h"
#endif

#if defined(MINGW) && defined(_SDL)
#include "./mingw-libs/SDL_syswm.h" // mingw sdl cross binary complains off sys_parentwindow
#endif

hashtable_t filesystemhash;
qboolean com_fschanged = true;
static unsigned int fs_restarts;
extern cvar_t com_fs_cache;
int active_fs_cachetype;
static int fs_referencetype;
int fs_finds;

struct
{
	void *module;
	const char *extension;
	searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc);
	qboolean loadscan;
} searchpathformats[64];

int FS_RegisterFileSystemType(void *module, const char *extension, searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc), qboolean loadscan)
{
	unsigned int i;
	for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
	{
		if (searchpathformats[i].extension && !strcmp(searchpathformats[i].extension, extension))
			break;	//extension match always replaces
		if (!searchpathformats[i].extension && !searchpathformats[i].OpenNew)
			break;
	}
	if (i == sizeof(searchpathformats)/sizeof(searchpathformats[0]))
		return 0;

	searchpathformats[i].module = module;
	searchpathformats[i].extension = extension;
	searchpathformats[i].OpenNew = OpenNew;
	searchpathformats[i].loadscan = loadscan;
	com_fschanged = true;

	return i+1;
}

void FS_UnRegisterFileSystemType(int idx)
{
	if ((unsigned int)(idx-1) >= sizeof(searchpathformats)/sizeof(searchpathformats[0]))
		return;

	searchpathformats[idx-1].OpenNew = NULL;
	searchpathformats[idx-1].module = NULL;
	com_fschanged = true;

	//FS_Restart will be needed
}
void FS_UnRegisterFileSystemModule(void *module)
{
	int i;
	qboolean found = false;
	for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
	{
		if (searchpathformats[i].module == module)
		{
			searchpathformats[i].OpenNew = NULL;
			searchpathformats[i].module = NULL;
			found = true;
		}
	}

	if (found)
	{
		Cmd_ExecuteString("fs_restart", RESTRICT_LOCAL);
	}
}


vfsfile_t *FS_OpenVFSLoc(flocation_t *loc, char *mode);



char *VFS_GETS(vfsfile_t *vf, char *buffer, int buflen)
{
	char in;
	char *out = buffer;
	int len;
	len = buflen-1;
	if (len == 0)
		return NULL;
	while (len > 0)
	{
		if (VFS_READ(vf, &in, 1) != 1)
		{
			if (len == buflen-1)
				return NULL;
			*out = '\0';
			return buffer;
		}
		if (in == '\n')
			break;
		*out++ = in;
		len--;
	}
	*out = '\0';

	//if there's a trailing \r, strip it.
	if (out > buffer)
		if (out[-1] == '\r')
			out[-1] = 0;

	return buffer;
}

void VARGS VFS_PRINTF(vfsfile_t *vf, char *format, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	VFS_PUTS(vf, string);
}






char	gamedirfile[MAX_OSPATH];




//the various COM_LoadFiles set these on return
int	com_filesize;
qboolean com_file_copyprotected;//file should not be available for download.
qboolean com_file_untrusted;	//file was downloaded inside a package


//char	*com_basedir;				//obsolete

char	com_quakedir[MAX_OSPATH];
char	com_homedir[MAX_OSPATH];

char	com_configdir[MAX_OSPATH];	//homedir/fte/configs

int fs_hash_dups;
int fs_hash_files;







static const char *FS_GetCleanPath(const char *pattern, char *outbuf, int outlen);
void FS_RegisterDefaultFileSystems(void);
static void	COM_CreatePath (char *path);

#define ENFORCEFOPENMODE(mode) {if (strcmp(mode, "r") && strcmp(mode, "w")/* && strcmp(mode, "rw")*/)Sys_Error("fs mode %s is not permitted here\n");}






//forget a manifest entirely.
void FS_Manifest_Free(ftemanifest_t *man)
{
	int i, j;
	if (!man)
		return;
	Z_Free(man->updateurl);
	Z_Free(man->installation);
	Z_Free(man->formalname);
	Z_Free(man->protocolname);
	Z_Free(man->defaultexec);
	for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
	{
		Z_Free(man->gamepath[i].path);
	}
	for (i = 0; i < sizeof(man->package) / sizeof(man->package[0]); i++)
	{
		Z_Free(man->package[i].path);
		for (j = 0; j < sizeof(man->package[i].mirrors) / sizeof(man->package[i].mirrors[0]); j++)
			Z_Free(man->package[i].mirrors[j]);
	}
	Z_Free(man);
}

//clone a manifest, so we can hack at it.
static ftemanifest_t *FS_Manifest_Clone(ftemanifest_t *oldm)
{
	ftemanifest_t *newm;
	int i, j;
	newm = Z_Malloc(sizeof(*newm));
	if (oldm->updateurl)
		newm->updateurl = Z_StrDup(oldm->updateurl);
	if (oldm->installation)
		newm->installation = Z_StrDup(oldm->installation);
	if (oldm->formalname)
		newm->formalname = Z_StrDup(oldm->formalname);
	if (oldm->protocolname)
		newm->protocolname = Z_StrDup(oldm->protocolname);

	for (i = 0; i < sizeof(newm->gamepath) / sizeof(newm->gamepath[0]); i++)
	{
		if (oldm->gamepath[i].path)
			newm->gamepath[i].path = Z_StrDup(oldm->gamepath[i].path);
		newm->gamepath[i].base = oldm->gamepath[i].base;
	}
	for (i = 0; i < sizeof(newm->package) / sizeof(newm->package[0]); i++)
	{
		if (oldm->package[i].path)
			newm->package[i].path = Z_StrDup(oldm->package[i].path);
		for (j = 0; j < sizeof(newm->package[i].mirrors) / sizeof(newm->package[i].mirrors[0]); j++)
			if (oldm->package[i].mirrors[j])
				newm->package[i].mirrors[j] = Z_StrDup(oldm->package[i].mirrors[j]);
	}

	return newm;
}

void FS_Manifest_Print(ftemanifest_t *man)
{
	int i, j;
	if (man->updateurl)
		Con_Printf("updateurl \"%s\"\n", man->updateurl);
	if (man->installation)
		Con_Printf("game \"%s\"\n", man->installation);
	if (man->formalname)
		Con_Printf("name \"%s\"\n", man->formalname);
	if (man->protocolname)
		Con_Printf("protocolname \"%s\"\n", man->protocolname);

	for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
	{
		if (man->gamepath[i].path)
		{
			if (man->gamepath[i].base)
				Con_Printf("basegame \"%s\"\n", man->gamepath[i].path);
			else
				Con_Printf("gamedir \"%s\"\n", man->gamepath[i].path);
		}
	}

	for (i = 0; i < sizeof(man->package) / sizeof(man->package[0]); i++)
	{
		if (man->package[i].path)
		{
			Con_Printf("package \"%s\" 0x%x", man->package[i].path, man->package[i].crc);
			for (j = 0; j < sizeof(man->package[i].mirrors) / sizeof(man->package[i].mirrors[0]); j++)
				if (man->package[i].mirrors[j])
					Con_Printf(" \"%s\"", man->package[i].mirrors[j]);
			Con_Printf("\n");
		}
	}
}

//forget any mod dirs.
static void FS_Manifest_PurgeGamedirs(ftemanifest_t *man)
{
	int i;
	for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
	{
		if (man->gamepath[i].path && !man->gamepath[i].base)
		{
			Z_Free(man->gamepath[i].path);
			man->gamepath[i].path = NULL;
		}
	}
}

//create a new empty manifest with default values.
static ftemanifest_t *FS_Manifest_Create(void)
{
	ftemanifest_t *man = Z_Malloc(sizeof(*man));

//	man->installation = Z_StrDup("quake");
	man->formalname = Z_StrDup(FULLENGINENAME);
	return man;
}
//parse Cmd_Argv tokens into the manifest.
static void FS_Manifest_ParseTokens(ftemanifest_t *man)
{
	char *fname;
	if (!Cmd_Argc())
		return;
	fname = Cmd_Argv(0);

	if (*fname == '*')
		fname++;

	if (!stricmp(fname, "game"))
	{
		Z_Free(man->installation);
		man->installation = Z_StrDup(Cmd_Argv(1));
	}
	else if (!stricmp(fname, "name"))
	{
		Z_Free(man->formalname);
		man->formalname = Z_StrDup(Cmd_Argv(1));
	}
	else if (!stricmp(fname, "protocolname"))
	{
		Z_Free(man->protocolname);
		man->protocolname = Z_StrDup(Cmd_Argv(1));
	}
	else if (!stricmp(fname, "basegame") || !stricmp(fname, "gamedir"))
	{
		int i;
		char *newdir = Cmd_Argv(1);

		//reject various evil path arguments.
		if (!*newdir || strchr(newdir, '\n') || strchr(newdir, '\r') || strchr(newdir, '.') || strchr(newdir, ':') || strchr(newdir, '?') || strchr(newdir, '*') || strchr(newdir, '/') || strchr(newdir, '\\') || strchr(newdir, '$'))
		{
			Con_Printf("Illegal path specified: %s\n", newdir);
		}
		else
		{
			for (i = 0; i < sizeof(man->gamepath) / sizeof(man->gamepath[0]); i++)
			{
				if (!man->gamepath[i].path)
				{
					man->gamepath[i].base = !stricmp(fname, "basegame");
					man->gamepath[i].path = Z_StrDup(newdir);
					break;
				}
			}
			if (i == sizeof(man->gamepath) / sizeof(man->gamepath[0]))
			{
				Con_Printf("Too many game paths specified in manifest\n");
			}
		}
	}
	else
	{
		int crc;
		int i, j;
		if (!stricmp(fname, "package"))
			Cmd_ShiftArgs(1, false);

		crc = strtoul(Cmd_Argv(1), NULL, 0);

		for (i = 0; i < sizeof(man->package) / sizeof(man->package[0]); i++)
		{
			if (!man->package[i].path)
			{
				man->package[i].path = Z_StrDup(Cmd_Argv(0));
				man->package[i].crc = crc;
				for (j = 0; j < Cmd_Argc()-2 && j < sizeof(man->package[i].mirrors) / sizeof(man->package[i].mirrors[0]); j++)
				{
					man->package[i].mirrors[j] = Z_StrDup(Cmd_Argv(2+j));
				}
				break;
			}
		}
		if (i == sizeof(man->package) / sizeof(man->package[0]))
		{
			Con_Printf("Too many packages specified in manifest\n");
		}
	}
}
//read a manifest file
ftemanifest_t *FS_Manifest_Parse(const char *data)
{
	ftemanifest_t *man;
	if (!data)
		return NULL;
	while (*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n')
		data++;
	if (!*data)
		return NULL;


	man = FS_Manifest_Create();

	while (data && *data)
	{
		data = Cmd_TokenizeString((char*)data, false, false);
		FS_Manifest_ParseTokens(man);
	}
	return man;
}

//======================================================================================================



typedef struct searchpath_s
{
	searchpathfuncs_t *handle;

	unsigned int flags;

	char logicalpath[MAX_OSPATH];	//printable hunam-readable location of the package. generally includes a system path, including nested packages.
	char purepath[256];	//server tracks the path used to load them so it can tell the client
	int crc_check;	//client sorts packs according to this checksum
	int crc_reply;	//client sends a different crc back to the server, for the paks it's actually loaded.

	struct searchpath_s *next;
	struct searchpath_s *nextpure;
} searchpath_t;

static ftemanifest_t	*fs_manifest;	//currently active manifest.
static searchpath_t	*com_searchpaths;
static searchpath_t	*com_purepaths;
static searchpath_t	*com_base_searchpaths;	// without gamedirs

static int fs_puremode;				//0=deprioritise pure, 1=prioritise pure, 2=pure only.
static char *fs_purenames;			//list of allowed packages
static char *fs_purecrcs;			//list of crcs for those packages. one token per package.
static unsigned int fs_pureseed;	//used as a key so the server knows we're obeying. completely unreliable/redundant in an open source project, but needed for q3 network compat.

int QDECL COM_FileSize(const char *path)
{
	int len;
	flocation_t loc;
	len = FS_FLocateFile(path, FSLFRT_LENGTH, &loc);
	return len;
}

//appends a / on the end of the directory if it does not already have one.
void FS_CleanDir(char *out, int outlen)
{
	int olen = strlen(out);
	if (!olen || olen >= outlen-1)
		return;

	if (out[olen-1] == '\\')
		out[olen-1] = '/';
	else if (out[olen-1] != '/')
	{
		out[olen+1] = '\0';
		out[olen] = '/';
	}
}

/*
============
COM_Path_f

============
*/
void COM_Path_f (void)
{
	searchpath_t	*s;

	Con_TPrintf (TL_CURRENTSEARCHPATH);

	if (com_purepaths || fs_puremode)
	{
		Con_Printf ("Pure paths:\n");
		for (s=com_purepaths ; s ; s=s->nextpure)
		{
			Con_Printf("%s  %s%s%s\n", s->logicalpath,
					(s->flags & SPF_REFERENCED)?"(ref)":"",
					(s->flags & SPF_TEMPORARY)?"(temp)":"",
					(s->flags & SPF_COPYPROTECTED)?"(c)":"");
		}
		Con_Printf ("----------\n");
		if (fs_puremode == 2)
			Con_Printf ("Inactive paths:\n");
		else
			Con_Printf ("Impure paths:\n");
	}


	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s == com_base_searchpaths)
			Con_Printf ("----------\n");

		Con_Printf("%s  %s%s%s\n", s->logicalpath,
				(s->flags & SPF_REFERENCED)?"(ref)":"",
				(s->flags & SPF_TEMPORARY)?"(temp)":"",
				(s->flags & SPF_COPYPROTECTED)?"(c)":"");
	}
}


/*
============
COM_Dir_f

============
*/
static int QDECL COM_Dir_List(const char *name, int size, void *parm, searchpathfuncs_t *spath)
{
	searchpath_t	*s;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->handle == spath)
			break;
	}
	Con_Printf("%s  (%i) (%s)\n", name, size, s?s->logicalpath:"??");
	return 1;
}

void COM_Dir_f (void)
{
	char match[MAX_QPATH];

	Q_strncpyz(match, Cmd_Argv(1), sizeof(match));
	if (Cmd_Argc()>2)
	{
		strncat(match, "/*.", sizeof(match)-1);
		match[sizeof(match)-1] = '\0';
		strncat(match, Cmd_Argv(2), sizeof(match)-1);
		match[sizeof(match)-1] = '\0';
	}
//	else
//		strncat(match, "/*", sizeof(match)-1);

	COM_EnumerateFiles(match, COM_Dir_List, NULL);
}

/*
============
COM_Locate_f

============
*/
void COM_Locate_f (void)
{
	flocation_t loc;
	if (FS_FLocateFile(Cmd_Argv(1), FSLFRT_LENGTH, &loc)>=0)
	{
		if (!*loc.rawname)
		{
			Con_Printf("File is %i bytes compressed inside %s\n", loc.len, loc.search->logicalpath);
		}
		else
		{
			Con_Printf("Inside %s (%i bytes)\n  %s\n", loc.rawname, loc.len, loc.search->logicalpath);
		}
	}
	else
		Con_Printf("Not found\n");
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile (const char *filename, const void *data, int len)
{
	vfsfile_t *vfs;

	Sys_Printf ("COM_WriteFile: %s\n", filename);

	FS_CreatePath(filename, FS_GAMEONLY);
	vfs = FS_OpenVFS(filename, "wb", FS_GAMEONLY);
	if (vfs)
	{
		VFS_WRITE(vfs, data, len);
		VFS_CLOSE(vfs);
	}

	com_fschanged=true;
}

FILE *COM_WriteFileOpen (char *filename)	//like fopen, but based around quake's paths.
{
	FILE	*f;
	char	name[MAX_OSPATH];

	if (!FS_NativePath(filename, FS_GAMEONLY, name, sizeof(name)))
		return NULL;

	COM_CreatePath(name);

	f = fopen (name, "wb");

	return f;
}


/*
============
COM_CreatePath

Only used for CopyFile and download
============
*/
static void	COM_CreatePath (char *path)
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


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
/*
static void COM_CopyFile (char *netpath, char *cachepath)
{
	FILE	*in, *out;
	int		remaining, count;
	char	buf[4096];

	remaining = COM_FileOpenRead (netpath, &in);
	COM_CreatePath (cachepath);	// create directories up to the cache file
	out = fopen(cachepath, "wb");
	if (!out)
		Sys_Error ("Error opening %s", cachepath);

	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, in);
		fwrite (buf, 1, count, out);
		remaining -= count;
	}

	fclose (in);
	fclose (out);
}
//*/

int fs_hash_dups;
int fs_hash_files;

void FS_FlushFSHashReally(void)
{
	if (filesystemhash.numbuckets)
	{
		int i;
		fsbucket_t *bucket, *next;

		for (i = 0; i < filesystemhash.numbuckets; i++)
		{
			bucket = (fsbucket_t*)filesystemhash.bucket[i];
			filesystemhash.bucket[i] = NULL;
			while(bucket)
			{
				next = (fsbucket_t*)bucket->buck.next;
				/*if the string starts right after the bucket, free it*/
				if (bucket->depth < 0)
					Z_Free(bucket);
				bucket = next;
			}
		}
	}

	com_fschanged = true;
}
void FS_FlushFSHashWritten(void)
{
	/*automatically handled*/
}
void FS_FlushFSHashRemoved(void)
{
	FS_FlushFSHashReally();
}

static void QDECL FS_AddFileHash(int depth, const char *fname, fsbucket_t *filehandle, void *pathhandle)
{
	fsbucket_t *old;

	old = Hash_GetInsensativeBucket(&filesystemhash, fname);

	if (old)
	{
		fs_hash_dups++;
		if (depth >= ((old->depth<0)?(-old->depth-1):old->depth))
		{
			return;
		}

		//remove the old version
		Hash_RemoveBucket(&filesystemhash, fname, &old->buck);
		if (old->depth < 0)
			Z_Free(old);
	}

	if (!filehandle)
	{
		filehandle = Z_Malloc(sizeof(*filehandle) + strlen(fname)+1);
		if (!filehandle)
			return;	//eep!
		strcpy((char*)(filehandle+1), fname);
		fname = (char*)(filehandle+1);
		filehandle->depth = -depth-1;
	}
	else filehandle->depth = depth;

	Hash_AddInsensative(&filesystemhash, fname, pathhandle, &filehandle->buck);
	fs_hash_files++;
}

void FS_RebuildFSHash(void)
{
	int depth = 1;
	searchpath_t	*search;
	if (!filesystemhash.numbuckets)
	{
		filesystemhash.numbuckets = 1024;
		filesystemhash.bucket = (bucket_t**)Z_Malloc(Hash_BytesForBuckets(filesystemhash.numbuckets));
	}
	else
	{
		FS_FlushFSHashRemoved();
	}
	Hash_InitTable(&filesystemhash, filesystemhash.numbuckets, filesystemhash.bucket);

	fs_hash_dups = 0;
	fs_hash_files = 0;

	if (com_purepaths)
	{	//go for the pure paths first.
		for (search = com_purepaths; search; search = search->nextpure)
		{
			search->handle->BuildHash(search->handle, depth++, FS_AddFileHash);
		}
	}
	if (fs_puremode < 2)
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			search->handle->BuildHash(search->handle, depth++, FS_AddFileHash);
		}
	}

	com_fschanged = false;

	Con_DPrintf("%i unique files, %i duplicates\n", fs_hash_files, fs_hash_dups);
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
//if loc is valid, loc->search is always filled in, the others are filled on success.
//returns -1 if couldn't find.
int FS_FLocateFile(const char *filename, FSLF_ReturnType_e returntype, flocation_t *loc)
{
	int depth=0, len;
	searchpath_t	*search;
	char cleanpath[MAX_QPATH];

	void *pf;

	filename = FS_GetCleanPath(filename, cleanpath, sizeof(cleanpath));
	if (!filename)
	{
		pf = NULL;
		goto fail;
	}

	if (com_fs_cache.ival)
	{
		if (com_fschanged)
			FS_RebuildFSHash();
		pf = Hash_GetInsensative(&filesystemhash, filename);
		if (!pf)
			goto fail;
	}
	else
		pf = NULL;

	if (com_purepaths)
	{
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			fs_finds++;
			if (search->handle->FindFile(search->handle, loc, filename, pf))
			{
				if (loc)
				{
					search->flags |= fs_referencetype;
					loc->search = search;
					len = loc->len;
				}
				else
					len = 0;
				com_file_copyprotected = !!(search->flags & SPF_COPYPROTECTED);
				com_file_untrusted = !!(search->flags & SPF_UNTRUSTED);
				goto out;
			}
			depth += ((search->flags & SPF_EXPLICIT) || returntype == FSLFRT_DEPTH_ANYPATH);
		}
	}

	if (fs_puremode < 2)
	{
//
// search through the path, one element at a time
//
		for (search = com_searchpaths ; search ; search = search->next)
		{
			fs_finds++;
			if (search->handle->FindFile(search->handle, loc, filename, pf))
			{
				search->flags |= fs_referencetype;
				if (loc)
				{
					loc->search = search;
					len = loc->len;
				}
				else
					len = 1;
				com_file_copyprotected = !!(search->flags & SPF_COPYPROTECTED);
				com_file_untrusted = !!(search->flags & SPF_UNTRUSTED);
				goto out;
			}
			depth += ((search->flags & SPF_EXPLICIT) || returntype == FSLFRT_DEPTH_ANYPATH);
		}
	}
fail:
	if (loc)
		loc->search = NULL;
	depth = 0x7fffffff;
	len = -1;
out:

/*	if (len>=0)
	{
		if (loc)
			Con_Printf("Found %s:%i\n", loc->rawname, loc->len);
		else
			Con_Printf("Found %s\n", filename);
	}
	else
		Con_Printf("Failed\n");
*/
	if (returntype == FSLFRT_IFFOUND)
		return len != -1;
	else if (returntype == FSLFRT_LENGTH)
		return len;
	else
		return depth;
}

char *FS_WhichPackForLocation(flocation_t *loc)
{
	char *ret;
	if (!loc->search)
		return NULL;	//huh? not a valid location.

	ret = strchr(loc->search->purepath, '/');
	if (!ret)
		return NULL;
	ret++;
	if (strchr(ret, '/'))
		return NULL;
	return ret;
}

/*requires extension*/
qboolean FS_GetPackageDownloadable(const char *package)
{
	searchpath_t	*search;

	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (!strcmp(package, search->purepath))
			return !(search->flags & SPF_COPYPROTECTED);
	}
	return false;
}

char *FS_GetPackHashes(char *buffer, int buffersize, qboolean referencedonly)
{
	searchpath_t	*search;
	buffersize--;
	*buffer = 0;

	if (com_purepaths)
	{
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			Q_strncatz(buffer, va("%i ", search->crc_check), buffersize);
		}
		return buffer;
	}
	else
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if (!search->crc_check && search->handle->GeneratePureCRC)
				search->crc_check = search->handle->GeneratePureCRC(search->handle, 0, 0);
			if (search->crc_check)
			{
				Q_strncatz(buffer, va("%i ", search->crc_check), buffersize);
			}
		}
		return buffer;
	}
}
/*
referencedonly=0: show all paks
referencedonly=1: show only paks that are referenced (q3-compat)
referencedonly=2: show all paks, but paks that are referenced are prefixed with a star
ext=0: hide extensions (q3-compat)
ext=1: show extensions.
*/
char *FS_GetPackNames(char *buffer, int buffersize, int referencedonly, qboolean ext)
{
	char temp[MAX_OSPATH];
	searchpath_t	*search;
	buffersize--;
	*buffer = 0;

	if (com_purepaths)
	{
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			if (referencedonly == 0 && !(search->flags & SPF_REFERENCED))
				continue;
			if (referencedonly == 2 && (search->flags & SPF_REFERENCED))
				Q_strncatz(buffer, "*", buffersize);

			if (!ext)
			{
				COM_StripExtension(search->purepath, temp, sizeof(temp));
				Q_strncatz(buffer, va("%s ", temp), buffersize);
			}
			else
			{
				Q_strncatz(buffer, va("%s ", search->purepath), buffersize);
			}
		}
		return buffer;
	}
	else
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if (!search->crc_check && search->handle->GeneratePureCRC)
				search->crc_check = search->handle->GeneratePureCRC(search->handle, 0, 0);
			if (search->crc_check)
			{
				if (referencedonly == 0 && !(search->flags & SPF_REFERENCED))
					continue;
				if (referencedonly == 2 && (search->flags & SPF_REFERENCED))
				{
					// '*' prefix is meant to mean 'referenced'.
					//really all that means to the client is that it definitely wants to download it.
					//if its copyrighted, the client shouldn't try to do so, as it won't be allowed.
					if (!(search->flags & SPF_COPYPROTECTED))
						Q_strncatz(buffer, "*", buffersize);
				}

				if (!ext)
				{
					COM_StripExtension(search->purepath, temp, sizeof(temp));
					Q_strncatz(buffer, va("%s ", temp), buffersize);
				}
				else
				{
					Q_strncatz(buffer, va("%s ", search->purepath), buffersize);
				}
			}
		}
		return buffer;
	}
}

void FS_ReferenceControl(unsigned int refflag, unsigned int resetflags)
{
	searchpath_t	*s;

	refflag &= SPF_REFERENCED;
	resetflags &= SPF_REFERENCED;

	if (resetflags)
	{
		for (s=com_searchpaths ; s ; s=s->next)
		{
			s->flags &= ~resetflags;
		}
	}

	fs_referencetype = refflag;
}


#if 0
int COM_FOpenLocationFILE(flocation_t *loc, FILE **file)
{
	if (!*loc->rawname)
	{
		if (!loc->len)
		{
			*file = NULL;
			return -1;
		}

		if (loc->search->funcs->ReadFile)
		{//create a new, temp file, bung the contents of the compressed file into it, then continue.
			char *buf;
			FILE *f = tmpfile();
			buf = BZ_Malloc(loc->len);
			loc->search->funcs->ReadFile(loc->search->handle, loc, buf);
			fwrite(buf, 1, loc->len, f);
			BZ_Free(buf);
			fseek(f, 0, SEEK_SET);

			*file = f;
			com_pathforfile = loc->search;
			return loc->len;
		}
		return -1;
	}
//	Con_Printf("Opening %s\n", loc->rawname);
	*file = fopen(loc->rawname, "rb");
	if (!*file)
		return -1;
	fseek(*file, loc->offset, SEEK_SET);
	com_pathforfile = loc->search;
	return loc->len;
}

int COM_FOpenFile(char *filename, FILE **file)
{
	flocation_t loc;
	Con_Printf(CON_ERROR "COM_FOpenFile is obsolete\n");
	FS_FLocateFile(filename, FSLFRT_LENGTH, &loc);

	com_filesize = -1;
	if (loc.search)
	{
		com_file_copyprotected = loc.search->copyprotected;
		com_file_untrusted = !!(loc.search->flags & SPF_UNTRUSTED);
		com_filesize = COM_FOpenLocationFILE(&loc, file);
	}
	else
		*file = NULL;
	return com_filesize;
}
/*
int COM_FOpenWriteFile(char *filename, FILE **file)
{
	COM_CreatePath(filename);
	*file = fopen(filename, "wb");
	return !!*file;
}
*/
#endif
//int COM_FOpenFile (char *filename, FILE **file) {file_from_pak=0;return COM_FOpenFile2 (filename, file, false);}	//FIXME: TEMPORARY

//outbuf might not be written into
static const char *FS_GetCleanPath(const char *pattern, char *outbuf, int outlen)
{
	char *s;

	if (strchr(pattern, '\\'))
	{
		Q_strncpyz(outbuf, pattern, outlen);
		pattern = outbuf;

		Con_Printf("Warning: \\ characters in filename %s\n", pattern);
		while((s = strchr(pattern, '\\')))
		{
			*s = '/';

		}
	}

	if (strstr(pattern, "//"))
	{
		//amiga uses // as equivelent to /../
		//so strip those out
		//any other system ignores the extras

		Q_strncpyz(outbuf, pattern, outlen);
		pattern = outbuf;

		Con_DPrintf("Warning: // characters in filename %s\n", pattern);
		while ((s=strstr(pattern, "//")))
		{
			s++;
			while (*s)
			{
				*s = *(s+1);
				s++;
			}
		}
	}
	if (*pattern == '/')
	{
		/*'fix up' and ignore, compat with q3*/
		Con_DPrintf("Error: absolute path in filename %s\n", pattern);
		pattern++;
	}

	if (strstr(pattern, ".."))
		Con_Printf("Error: '..' characters in filename %s\n", pattern);
	else if (strstr(pattern, ":")) //win32 drive seperator (or mac path seperator, but / works there and they're used to it) (or amiga device separator)
		Con_Printf("Error: absolute path in filename %s\n", pattern);
	else if (strlen(pattern) > outlen)
		Con_Printf("Error: path %s too long\n", pattern);
	else
	{
		return pattern;
	}
	return NULL;
}

vfsfile_t *VFS_Filter(const char *filename, vfsfile_t *handle)
{
//	char *ext;

	if (!handle || handle->WriteBytes || handle->seekingisabadplan)	//only on readonly files
		return handle;
//	ext = COM_FileExtension (filename);
#ifdef AVAIL_ZLIB
//	if (!stricmp(ext, ".gz"))
	{
		return FS_DecompressGZip(handle, NULL);
	}
#endif
	return handle;
}

qboolean FS_NativePath(const char *fname, enum fs_relative relativeto, char *out, int outlen)
{
	char cleanname[MAX_QPATH];
	fname = FS_GetCleanPath(fname, cleanname, sizeof(cleanname));
	if (!fname)
		return false;

	switch (relativeto)
	{
	case FS_GAMEONLY:
	case FS_GAME:
		if (*com_homedir)
			snprintf(out, outlen, "%s%s/%s", com_homedir, gamedirfile, fname);
		else
			snprintf(out, outlen, "%s%s/%s", com_quakedir, gamedirfile, fname);
		break;
	case FS_SKINS:
		//FIXME: validate that qw/ is actually loaded and valid
		if (*com_homedir)
			snprintf(out, outlen, "%sqw/skins/%s", com_homedir, fname);
		else
			snprintf(out, outlen, "%sqw/skins/%s", com_quakedir, fname);
		break;
	case FS_BINARYPATH:
		if (host_parms.binarydir && *host_parms.binarydir)
			snprintf(out, outlen, "%s%s", host_parms.binarydir, fname);
		else
			snprintf(out, outlen, "%s%s", host_parms.basedir, fname);
		break;
	case FS_ROOT:
		if (*com_homedir)
			snprintf(out, outlen, "%s%s", com_homedir, fname);
		else
			snprintf(out, outlen, "%s%s", com_quakedir, fname);
		break;
	case FS_CONFIGONLY:
		//FIXME: use the highest-precidence active system path instead
		if (*com_homedir)
			snprintf(out, outlen, "%sfte/%s", com_homedir, fname);
		else
			snprintf(out, outlen, "%sfte/%s", com_quakedir, fname);
		break;
	default:
		Sys_Error("FS_NativePath case not handled\n");
	}
	return true;
}

/*locates and opens a file*/
vfsfile_t *FS_OpenVFS(const char *filename, const char *mode, enum fs_relative relativeto)
{
	char cleanname[MAX_QPATH];
	char fullname[MAX_OSPATH];
	flocation_t loc;
	vfsfile_t *vfs;

	//eventually, this function will be the *ONLY* way to get at files

	//blanket-bans


	filename = FS_GetCleanPath(filename, cleanname, sizeof(cleanname));
	if (!filename)
		return NULL;


	if (strcmp(mode, "rb"))
		if (strcmp(mode, "r+b"))
			if (strcmp(mode, "wb"))
				if (strcmp(mode, "w+b"))
					if (strcmp(mode, "ab"))
						return NULL; //urm, unable to write/append

	//if there can only be one file (eg: write access) find out where it is.
	switch (relativeto)
	{
	case FS_GAMEONLY:	//OS access only, no paks
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_homedir, gamedirfile, filename);
			if (*mode == 'w')
				COM_CreatePath(fullname);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%s%s/%s", com_quakedir, gamedirfile, filename);
		if (*mode == 'w')
			COM_CreatePath(fullname);
		return VFSOS_Open(fullname, mode);
	case FS_GAME:	//load from paks in preference to system paths. overwriting be damned.
	case FS_SKINS:	//load from paks in preference to system paths. overwriting be damned.
		FS_NativePath(filename, relativeto, fullname, sizeof(fullname));
		break;
	case FS_ROOT:	//always bypass packs and gamedirs
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%s%s", com_homedir, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%s%s", com_quakedir, filename);
		return VFSOS_Open(fullname, mode);
	case FS_CONFIGONLY:		//always bypass packs+pure.
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%sfte/%s", com_homedir, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%sfte/%s", com_quakedir, filename);
		return VFSOS_Open(fullname, mode);
	default:
		Sys_Error("FS_OpenVFS: Bad relative path (%i)", relativeto);
		break;
	}

	FS_FLocateFile(filename, FSLFRT_IFFOUND, &loc);

	if (loc.search)
	{
		com_file_copyprotected = !!(loc.search->flags & SPF_COPYPROTECTED);
		com_file_untrusted = !!(loc.search->flags & SPF_UNTRUSTED);
		return VFS_Filter(filename, loc.search->handle->OpenVFS(loc.search->handle, &loc, mode));
	}

	//if we're meant to be writing, best write to it.
	if (strchr(mode , 'w') || strchr(mode , 'a'))
	{
		COM_CreatePath(fullname);
		return VFSOS_Open(fullname, mode);
	}
	return NULL;
}

/*opens a vfsfile from an already discovered location*/
vfsfile_t *FS_OpenReadLocation(flocation_t *location)
{
	if (location->search)
	{
		com_file_copyprotected = !!(location->search->flags & SPF_COPYPROTECTED);
		com_file_untrusted = !!(location->search->flags & SPF_UNTRUSTED);
		return VFS_Filter(NULL, location->search->handle->OpenVFS(location->search->handle, location, "rb"));
	}
	return NULL;
}

qboolean FS_Rename2(const char *oldf, const char *newf, enum fs_relative oldrelativeto, enum fs_relative newrelativeto)
{
	char oldfullname[MAX_OSPATH];
	char newfullname[MAX_OSPATH];

	if (!FS_NativePath(oldf, oldrelativeto, oldfullname, sizeof(oldfullname)))
		return false;
	if (!FS_NativePath(newf, newrelativeto, newfullname, sizeof(newfullname)))
		return false;

	FS_CreatePath(newf, newrelativeto);
	return Sys_Rename(oldfullname, newfullname);
}
qboolean FS_Rename(const char *oldf, const char *newf, enum fs_relative relativeto)
{
	return FS_Rename2(oldf, newf, relativeto, relativeto);
}
qboolean FS_Remove(const char *fname, enum fs_relative relativeto)
{
	char fullname[MAX_OSPATH];

	if (!FS_NativePath(fname, relativeto, fullname, sizeof(fullname)))
		return false;

	return Sys_remove (fullname);
}
//create a path for the given filename (dir-only must have trailing slash)
void FS_CreatePath(const char *pname, enum fs_relative relativeto)
{
	char fullname[MAX_OSPATH];
	if (!FS_NativePath(pname, relativeto, fullname, sizeof(fullname)))
		return;

	COM_CreatePath(fullname);
}

qboolean FS_WriteFile (const char *filename, const void *data, int len, enum fs_relative relativeto)
{
	vfsfile_t *f;
	FS_CreatePath(filename, relativeto);
	f = FS_OpenVFS(filename, "wb", relativeto);
	if (!f)
		return false;
	VFS_WRITE(f, data, len);
	VFS_CLOSE(f);

	return true;
}

qboolean FS_Copy(const char *source, const char *dest, enum fs_relative relativesource, enum fs_relative relativedest)
{
	vfsfile_t *d, *s;
	char buffer[8192*8];
	int read;
	qboolean result = false;
	FS_CreatePath(dest, relativedest);
	s = FS_OpenVFS(source, "rb", relativesource);
	if (s)
	{
		d = FS_OpenVFS(dest, "wb", relativedest);
		if (d)
		{
			result = true;

			for (;;)
			{
				read = VFS_READ(s, buffer, sizeof(buffer));
				if (read <= 0)
					break;
				if (VFS_WRITE(d, buffer, read) != read)
				{
					result = false;
					break;
				}
			}

			VFS_CLOSE(d);

			if (!result)
				FS_Remove(dest, relativedest);
		}
		VFS_CLOSE(s);
	}
	return result;
}


static cache_user_t *loadcache;
static qbyte	*loadbuf;
static int		loadsize;

/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Always appends a 0 qbyte to the loaded data.
============
*/
qbyte *COM_LoadFile (const char *path, int usehunk)
{
	vfsfile_t *f;
	qbyte *buf;
	int len;
	char	base[MAX_OSPATH];
	flocation_t loc;
	FS_FLocateFile(path, FSLFRT_LENGTH, &loc);

	if (!loc.search)
		return NULL;	//wasn't found


	f = loc.search->handle->OpenVFS(loc.search->handle, &loc, "rb");
	if (!f)
		return NULL;

	com_filesize = len = VFS_GETLEN(f);
	// extract the filename base name for hunk tag
	COM_FileBase (path, base, sizeof(base));

	if (usehunk == 0)
		buf = (qbyte*)Z_Malloc (len+1);
	else if (usehunk == 1)
		buf = (qbyte*)Hunk_AllocName (len+1, base);
	else if (usehunk == 2)
		buf = (qbyte*)Hunk_TempAlloc (len+1);
	else if (usehunk == 3)
		buf = (qbyte*)Cache_Alloc (loadcache, len+1, base);
	else if (usehunk == 4)
	{
		if (len+1 > loadsize)
			buf = (qbyte*)Hunk_TempAlloc (len+1);
		else
			buf = loadbuf;
	}
	else if (usehunk == 5)
		buf = (qbyte*)BZ_Malloc(len+1);
	else if (usehunk == 6)
		buf = (qbyte*)Hunk_TempAllocMore (len+1);
	else
	{
		Sys_Error ("COM_LoadFile: bad usehunk");
		buf = NULL;
	}

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s", path);

	((qbyte *)buf)[len] = 0;

	VFS_READ(f, buf, len);
	VFS_CLOSE(f);

	return buf;
}

qbyte *FS_LoadMallocFile (const char *path)
{
	return COM_LoadFile (path, 5);
}

qbyte *COM_LoadHunkFile (const char *path)
{
	return COM_LoadFile (path, 1);
}

qbyte *COM_LoadTempFile (const char *path)
{
	return COM_LoadFile (path, 2);
}
qbyte *COM_LoadTempMoreFile (const char *path)
{
	return COM_LoadFile (path, 6);
}

void COM_LoadCacheFile (const char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
qbyte *QDECL COM_LoadStackFile (const char *path, void *buffer, int bufsize)
{
	qbyte	*buf;

	loadbuf = (qbyte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, 4);

	return buf;
}


/*warning: at some point I'll change this function to return only read-only buffers*/
int FS_LoadFile(char *name, void **file)
{
	*file = FS_LoadMallocFile(name);
	if (!*file)
		return -1;
	return com_filesize;
}
void FS_FreeFile(void *file)
{
	BZ_Free(file);
}



void COM_EnumerateFiles (const char *match, int (QDECL *func)(const char *, int, void *, searchpathfuncs_t*), void *parm)
{
	searchpath_t    *search;
	for (search = com_searchpaths; search ; search = search->next)
	{
	// is the element a pak file?
		if (!search->handle->EnumerateFiles(search->handle, match, func, parm))
			break;
	}
}

void COM_FlushTempoaryPacks(void)
{
	searchpath_t *sp, **link;
	link = &com_searchpaths;
	while (*link)
	{
		sp = *link;
		if (sp->flags & SPF_TEMPORARY)
		{
			FS_FlushFSHashReally();

			*link = sp->next;

			sp->handle->ClosePath(sp->handle);
			Z_Free (sp);
		}
		else
			link = &sp->next;
	}
	com_purepaths = NULL;
}

qboolean COM_LoadMapPackFile (const char *filename, int ofs)
{
	return false;
}

static searchpath_t *FS_AddPathHandle(searchpath_t **oldpaths, const char *purepath, const char *probablepath, searchpathfuncs_t *handle, unsigned int flags, unsigned int loadstuff);
searchpathfuncs_t *FS_GetOldPath(searchpath_t **oldpaths, const char *dir)
{
	searchpath_t *p;
	searchpathfuncs_t *r = NULL;
	while(*oldpaths)
	{
		p = *oldpaths;

		if (!stricmp(p->logicalpath, dir))
		{
			*oldpaths = p->next;
			r = p->handle;
			Z_Free(p);
			break;
		}

		oldpaths = &(*oldpaths)->next;
	}
	return r;
}

typedef struct {
	searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc);
	searchpath_t **oldpaths;
	const char *parentdesc;
	const char *puredesc;
} wildpaks_t;

static int QDECL FS_AddWildDataFiles (const char *descriptor, int size, void *vparam, searchpathfuncs_t *funcs)
{
	wildpaks_t *param = vparam;
	vfsfile_t *vfs;
	searchpath_t	*search;
	searchpathfuncs_t	*newpak;
	char			pakfile[MAX_OSPATH];
	char			purefile[MAX_OSPATH];
	flocation_t loc;

	Q_snprintfz (pakfile, sizeof(pakfile), "%s%s", param->parentdesc, descriptor);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!stricmp(search->logicalpath, pakfile))	//assumption: first member of structure is a char array
			return true; //already loaded (base paths?)
	}

	newpak = FS_GetOldPath(param->oldpaths, pakfile);
	if (!newpak)
	{
		fs_finds++;
		if (!funcs->FindFile(funcs, &loc, descriptor, NULL))
			return true;	//not found..
		vfs = funcs->OpenVFS(funcs, &loc, "rb");
		if (!vfs)
			return true;
		newpak = param->OpenNew (vfs, pakfile);
		if (!newpak)
		{
			VFS_CLOSE(vfs);
			return true;
		}
	}

	Q_snprintfz (pakfile, sizeof(pakfile), "%s%s", param->parentdesc, descriptor);
	if (*param->puredesc)
		snprintf (purefile, sizeof(purefile), "%s/%s", param->puredesc, descriptor);
	else
		Q_strncpyz(purefile, descriptor, sizeof(purefile));
	FS_AddPathHandle(param->oldpaths, purefile, pakfile, newpak, ((!Q_strncasecmp(descriptor, "pak", 3))?SPF_COPYPROTECTED:0), (unsigned int)-1);

	return true;
}


static void FS_AddDataFiles(searchpath_t **oldpaths, const char *purepath, const char *logicalpath, searchpath_t *search, const char *extension, searchpathfuncs_t *(QDECL *OpenNew)(vfsfile_t *file, const char *desc))
{
	//search is the parent
	int				i;
	searchpathfuncs_t	*handle;
	char			pakfile[MAX_OSPATH];
	char			logicalpaths[MAX_OSPATH];	//with a slash
	char			purefile[MAX_OSPATH];
	vfsfile_t *vfs;
	flocation_t loc;
	wildpaks_t wp;

	Q_strncpyz(logicalpaths, logicalpath, sizeof(logicalpaths));
	FS_CleanDir(logicalpaths, sizeof(logicalpaths));

	//first load all the numbered pak files
	for (i=0 ; ; i++)
	{
		snprintf (pakfile, sizeof(pakfile), "pak%i.%s", i, extension);
		fs_finds++;
		if (!search->handle->FindFile(search->handle, &loc, pakfile, NULL))
			break;	//not found..

		snprintf (pakfile, sizeof(pakfile), "%spak%i.%s", logicalpaths, i, extension);
		snprintf (purefile, sizeof(purefile), "%s/pak%i.%s", purepath, i, extension);

		handle = FS_GetOldPath(oldpaths, pakfile);
		if (!handle)
		{
			vfs = search->handle->OpenVFS(search->handle, &loc, "r");
			if (!vfs)
				break;
			handle = OpenNew (vfs, pakfile);
			if (!handle)
				break;
		}
		FS_AddPathHandle(oldpaths, purefile, pakfile, handle, SPF_COPYPROTECTED, (unsigned int)-1);
	}

	//now load the random ones
	Q_snprintfz (pakfile, sizeof(pakfile), "*.%s", extension);
	wp.OpenNew = OpenNew;
	wp.parentdesc = logicalpaths;
	wp.puredesc = purepath;
	wp.oldpaths = oldpaths;
	search->handle->EnumerateFiles(search->handle, pakfile, FS_AddWildDataFiles, &wp);


	//and load any named in the manifest (this happens when they're crced or whatever)
	{
		int ptlen, palen;
		ptlen = strlen(purepath);
		for (i = 0; i < sizeof(fs_manifest->package) / sizeof(fs_manifest->package[0]); i++)
		{
			if (fs_manifest->package[i].path && !strcmp(COM_FileExtension(fs_manifest->package[i].path), extension))
			{
				palen = strlen(fs_manifest->package[i].path);
				if (palen > ptlen && (fs_manifest->package[i].path[ptlen] == '/' || fs_manifest->package[i].path[ptlen] == '\\' )&& !strncmp(purepath, fs_manifest->package[i].path, ptlen))
				{
					searchpath_t *oldp;
					char pname[MAX_OSPATH];
					char lname[MAX_OSPATH];
					snprintf(lname, sizeof(lname), "%#x", fs_manifest->package[i].crc);
					if (!FS_GenCachedPakName(fs_manifest->package[i].path, lname, pname, sizeof(pname)))
						continue;
					snprintf (lname, sizeof(lname), "%s%s", logicalpath, pname+ptlen+1);

					for (oldp = com_searchpaths; oldp; oldp = oldp->next)
					{
						if (!stricmp(oldp->purepath, fs_manifest->package[i].path))
							break;
						if (!stricmp(oldp->logicalpath, lname))
							break;
					}
					if (!oldp)
					{
						handle = FS_GetOldPath(oldpaths, lname);
						if (!handle)
						{
							if (search->handle->FindFile(search->handle, &loc, pname+ptlen+1, NULL))
							{
								vfs = search->handle->OpenVFS(search->handle, &loc, "r");
								if (vfs)
									handle = OpenNew (vfs, lname);
							}
						}
						if (handle)
						{
							int truecrc = handle->GeneratePureCRC(handle, 0, false);
							if (truecrc != fs_manifest->package[i].crc)
							{
								Con_Printf(CON_ERROR "File \"%s\" has hash %#x (required: %#x). Please delete it or move it away\n", lname, truecrc, fs_manifest->package[i].crc);
								handle->ClosePath(handle);
								handle = NULL;
							}
						}
						if (handle)
							FS_AddPathHandle(oldpaths, fs_manifest->package[i].path, lname, handle, SPF_COPYPROTECTED|SPF_UNTRUSTED, (unsigned int)-1);
					}
				}
			}
		}
	}
}

static searchpath_t *FS_AddPathHandle(searchpath_t **oldpaths, const char *purepath, const char *logicalpath, searchpathfuncs_t *handle, unsigned int flags, unsigned int loadstuff)
{
	unsigned int i;

	searchpath_t *search, **link;

	if (!handle)
	{
		Con_Printf("COM_AddPathHandle: not a valid handle (%s)\n", logicalpath);
		return NULL;
	}

	if (handle->fsver != FSVER)
	{
		Con_Printf("%s: file system driver is outdated (%u should be %u)\n", logicalpath, handle->fsver, FSVER);
		handle->ClosePath(handle);
		return NULL;
	}

	search = (searchpath_t*)Z_Malloc (sizeof(searchpath_t));
	search->flags = flags;
	search->handle = handle;
	Q_strncpyz(search->purepath, purepath, sizeof(search->purepath));
	Q_strncpyz(search->logicalpath, logicalpath, sizeof(search->logicalpath));

	//temp packages also do not nest
	if (!(flags & SPF_TEMPORARY))
	{
		for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
		{
			if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew || !searchpathformats[i].loadscan)
				continue;
			if (loadstuff & (1<<i))
			{
				FS_AddDataFiles(oldpaths, purepath, logicalpath, search, searchpathformats[i].extension, searchpathformats[i].OpenNew);
			}
		}
	}

	if (flags & SPF_TEMPORARY)
	{
		//add at end. pureness will reorder if needed.
		link = &com_searchpaths;
		while(*link)
		{
			link = &(*link)->next;
		}
		*link = search;
	}
	else
	{
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
	com_fschanged = true;

	return search;
}

void COM_RefreshFSCache_f(void)
{
	com_fschanged=true;
}

void COM_FlushFSCache(void)
{
	searchpath_t *search;
	if (com_fs_cache.ival != 2)
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if (search->handle->PollChanges)
				com_fschanged |= search->handle->PollChanges(search->handle);
		}
	}
}

/*since should start as 0, otherwise this can be used to poll*/
qboolean FS_Restarted(unsigned int *since)
{
	if (*since < fs_restarts)
	{
		*since = fs_restarts;
		return true;
	}
	return false;
}

/*
================
FS_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddGameDirectory (searchpath_t **oldpaths, const char *puredir, const char *dir, unsigned int loadstuff)
{
	searchpath_t	*search;

	char			*p;
	void			*handle;

	fs_restarts++;

	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(gamedirfile, ++p);
	else
		strcpy(gamedirfile, dir);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!stricmp(search->logicalpath, dir))
			return; //already loaded (base paths?)
	}

//
// add the directory to the search path
//
	handle = FS_GetOldPath(oldpaths, dir);
	if (!handle)
		handle = VFSOS_OpenPath(NULL, dir);

	FS_AddPathHandle(oldpaths, puredir, dir, handle, SPF_EXPLICIT, loadstuff);
}

searchpathfuncs_t *COM_IteratePaths (void **iterator, char *buffer, int buffersize)
{
	searchpath_t	*s;
	void			*prev;

	prev = NULL;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (!(s->flags & SPF_EXPLICIT))
			continue;

		if (*iterator == prev)
		{
			*iterator = s->handle;
			Q_strncpyz(buffer, s->logicalpath, buffersize-1);
			FS_CleanDir(buffer, buffersize);
			return s->handle;
		}
		prev = s->handle;
	}

	*iterator = NULL;
	*buffer = 0;
	return NULL;
}

char *FS_GetGamedir(void)
{
	return gamedirfile;
}
/*unsafe - provided only for gamecode compat, should not be used for internal features*/
char *FS_GetBasedir(void)
{
	return com_quakedir;
}

//given a 'c:/foo/bar/' path, will extract 'bar'.
void FS_ExtractDir(char *in, char *out, int outlen)
{
	char *end;
	if (!outlen)
		return;
	end = in + strlen(in);
	//skip over any trailing slashes
	while (end > in)
	{
		if (end[-1] == '/' || end[-1] == '\\')
			end--;
		else
			break;
	}

	//skip over the path
	while (end > in)
	{
		if (end[-1] != '/' && end[-1] != '\\')
			end--;
		else
			break;
	}

	//copy string into the dest
	while (--outlen)
	{
		if (*end == '/' || *end == '\\' || !*end)
			break;
		*out++ = *end++;
	}
	*out = 0;
}

/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir (const char *dir)
{
	ftemanifest_t *man;
	if (!fs_manifest)
		FS_ChangeGame(NULL, true);

	man = FS_Manifest_Clone(fs_manifest);
	FS_Manifest_PurgeGamedirs(man);
	if (*dir)
	{
		char *dup = Z_StrDup(dir);
		dir = dup;
		while ((dir = COM_ParseStringSet(dir)))
		{
			if (!strcmp(dir, ";"))
				continue;
			if (!*com_token)
				continue;

			Cmd_TokenizeString(va("gamedir \"%s\"", com_token), false, false);
			FS_Manifest_ParseTokens(man);
		}
		Z_Free(dup);
	}
	FS_ChangeGame(man, true);

#if 0
	char thispath[64];
	searchpath_t	*next;
	qboolean isbase;

	//don't allow leading dots, hidden files are evil.
	//don't allow complex paths. those are evil too.
	if (!*dir || *dir == '.' || !strcmp(dir, ".") || strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf (TL_GAMEDIRAINTPATH);
		return;
	}

	isbase = false;
	for (next = com_searchpaths; next; next = next->next)
	{
		if (next == com_base_searchpaths)
			isbase = true;

		if (next->funcs == &osfilefuncs)
		{
			FS_CleanDir(next->purepath, thispath, sizeof(thispath));
			if (!strcmp(dir, thispath))
			{
				if (isbase && com_searchpaths == com_base_searchpaths)
				{
					Q_strncpyz (gamedirfile, dir, sizeof(gamedirfile));
					return;
				}
				if (!isbase)
					return;
				break;
			}
		}
	}

	FS_ForceToPure(NULL, NULL, 0);

#ifndef SERVERONLY
//	Host_WriteConfiguration();	//before we change anything.
#endif

	Q_strncpyz (gamedirfile, dir, sizeof(gamedirfile));

#ifndef CLIENTONLY
	sv.gamedirchanged = true;
#endif
#ifndef SERVERONLY
	cl.gamedirchanged = true;
#endif

	FS_FlushFSHashReally();

	//
	// free up any current game dir info
	//
	while (com_searchpaths != com_base_searchpaths)
	{
		com_searchpaths->handle->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged = true;

	//
	// flush all data, so it will be forced to reload
	//
	Cache_Flush ();

	if (strchr(dir, ';'))
	{
		//separate case because parsestringset splits by whitespace too
		while ((dir = COM_ParseStringSet(dir)))
		{
			if (!strcmp(dir, ";"))
				continue;
			if (!*dir)
				continue;

			FS_AddGameDirectory(dir, va("%s%s", com_quakedir, com_token), ~0);
			if (*com_homedir)
				FS_AddGameDirectory(dir, va("%s%s", com_homedir, com_token), ~0);
		}
	}
	else
	{
		FS_AddGameDirectory(dir, va("%s%s", com_quakedir, dir), ~0);
		if (*com_homedir)
			FS_AddGameDirectory(dir, va("%s%s", com_homedir, dir), ~0);
	}


#ifndef SERVERONLY
	if (!isDedicated)
	{
//		if (qrenderer != QR_NONE)	//only do this if we have already started the renderer
//			Cbuf_InsertText("vid_restart\n", RESTRICT_LOCAL);


		if (COM_FDepthFile("config.cfg", true) <= (*com_homedir?1:0))
		{
			Cbuf_InsertText("cl_warncmd 0\n"
							"exec config.cfg\n"
							"exec fte.cfg\n"
							"cl_warncmd 1\n", RESTRICT_LOCAL, false);
		}
	}

	Shader_Init();	//FIXME!

	COM_Effectinfo_Clear();

	Validation_FlushFileList();	//prevent previous hacks from making a difference.

	//FIXME: load new palette, if different cause a vid_restart.

#endif
#endif
}

#define QCFG "set allow_download_refpackages 0\n"
/*stuff that makes dp-only mods work a bit better*/
#define DPCOMPAT QCFG "set _cl_playermodel \"\"\n set dpcompat_set 1\n set dpcompat_trailparticles 1\nset dpcompat_corruptglobals 1\nset vid_pixelheight 1\n"
/*nexuiz/xonotic has a few quirks/annoyances...*/
#define NEXCFG DPCOMPAT "set r_particlesdesc effectinfo\nset sv_maxairspeed \"400\"\nset sv_jumpvelocity 270\nset sv_mintic \"0.01\"\ncl_nolerp 0\npr_enable_uriget 0\n"
/*some modern non-compat settings*/
#define DMFCFG "set com_parseutf8 1\npm_airstep 1\nsv_demoExtensions 1\n"
/*set some stuff so our regular qw client appears more like hexen2*/
#define HEX2CFG "set com_parseutf8 -1\nset gl_font gfx/hexen2\nset in_builtinkeymap 0\nset_calc cl_playerclass int (random * 5) + 1\nset sv_maxspeed 640\nset watervis 1\nset r_wateralpha 0.5\nset sv_pupglow 1\nset cl_model_bobbing 1\nsv_sound_land \"fx/thngland.wav\"\n"
/*yay q2!*/
#define Q2CFG "gl_font \":?col=0.44 1 0.2\"\ncom_nogamedirnativecode 0\n"
/*Q3's ui doesn't like empty model/headmodel/handicap cvars, even if the gamecode copes*/
#define Q3CFG "gl_overbright 2\nseta model sarge\nseta headmodel sarge\nseta handicap 100\ncom_nogamedirnativecode 0\n"
#define RMQCFG "sv_bigcoords 1\n"

typedef struct {
	const char *argname;	//used if this was used as a parameter.
	const char *exename;	//used if the exe name contains this
	const char *protocolname;	//sent to the master server when this is the current gamemode (Typically set for DP compat).
	const char *auniquefile[4];	//used if this file is relative from the gamedir. needs just one file

	const char *customexec;

	const char *dir[4];
	const char *poshname;	//Full name for the game.
	const char *manifestfile;
} gamemode_info_t;
const gamemode_info_t gamemode_info[] = {
#define MASTER_PREFIX "FTE-"
//note that there is no basic 'fte' gamemode, this is because we aim for network compatability. Darkplaces-Quake is the closest we get.
//this is to avoid having too many gamemodes anyway.

//mission packs should generally come after the main game to avoid prefering the main game. we violate this for hexen2 as the mission pack is mostly a superset.
//whereas the quake mission packs replace start.bsp making the original episodes unreachable.
//for quake, we also allow extracting all files from paks. some people think it loads faster that way or something.

	//cmdline switch exename    protocol name(dpmaster)  identifying file		exec     dir1       dir2    dir3       dir(fte)     full name
	{"-quake",		"q1",		MASTER_PREFIX"Quake",	{"id1/pak0.pak",
														 "id1/quake.rc"},		QCFG,	{"id1",		"qw",				"fte"},		"Quake"/*,    "id1/pak0.pak|http://quakeservers.nquake.com/qsw106.zip|http://nquake.localghost.net/qsw106.zip|http://qw.quakephil.com/nquake/qsw106.zip|http://fnu.nquake.com/qsw106.zip"*/},
	{"-hipnotic",	"hipnotic",	MASTER_PREFIX"Hipnotic",{"hipnotic/pak0.pak"},	QCFG,	{"id1",		"qw",	"hipnotic",	"fte"},		"Quake: Scourge of Armagon"},
	{"-rogue",		"rogue",	MASTER_PREFIX"Rogue",	{"rogue/pak0.pak"},		QCFG,	{"id1",		"qw",	"rogue",	"fte"},		"Quake: Dissolution of Eternity"},
	{"-nexuiz",		"nexuiz",	"Nexuiz",				{"nexuiz.exe"},			NEXCFG,	{"data",						"ftedata"},	"Nexuiz"},
	{"-xonotic",	"xonotic",	"Xonotic",				{"xonotic.exe"},		NEXCFG,	{"data",						"ftedata"},	"Xonotic"},
	{"-spark",		"spark",	"Spark",				{"base/src/progs.src",
														 "base/qwprogs.dat",
														 "base/pak0.pak"},		DMFCFG,	{"base",						         },	"Spark"},
	{"-scouts",		"scouts",	"FTE-SJ",				{"basesj/src/progs.src",
														 "basesj/progs.dat",
														 "basesj/pak0.pak"},	NULL,	{"basesj",						         },	"Scouts Journey"},
	{"-rmq",		"rmq",		"RMQ",					{NULL},					RMQCFG,	{"id1",		"qw",	"rmq",		"fte"},		"Remake Quake"},

	//supported commercial mods (some are currently only partially supported)
	{"-portals",	"h2mp",		"FTE-H2MP",				{"portals/hexen.rc",
														 "portals/pak3.pak"},	HEX2CFG,{"data1",	"portals",			"fteh2"},	"Hexen II MP"},
	{"-hexen2",		"hexen2",	"FTE-Hexen2",			{"data1/pak0.pak"},		HEX2CFG,{"data1",						"fteh2"},	"Hexen II"},
	{"-quake2",		"q2",		"FTE-Quake2",			{"baseq2/pak0.pak"},	Q2CFG,	{"baseq2",						"fteq2"},	"Quake II"},
	{"-quake3",		"q3",		"FTE-Quake3",			{"baseq3/pak0.pk3"},	Q3CFG,	{"baseq3",						"fteq3"},	"Quake III Arena"},

	//can run in windows, needs 
	{"-halflife",	"hl",		"FTE-HalfLife",			{"valve/liblist.gam"},	NULL,	{"valve",						"ftehl"},	"Half-Life"},

	//the rest are not supported in any real way. maps-only mostly, if that
	{"-q4",			"q4",		"FTE-Quake4",			{"q4base/pak00.pk4"},	NULL,	{"q4base",						"fteq4"},	"Quake 4"},
	{"-et",			"et",		"FTE-EnemyTerritory",	{"etmain/pak0.pk3"},	NULL,	{"etmain",						"fteet"},	"Wolfenstein - Enemy Territory"},

	{"-jk2",		"jk2",		"FTE-JK2",				{"base/assets0.pk3"},	NULL,	{"base",						"fte"},		"Jedi Knight II: Jedi Outcast"},
	{"-warsow",		"warsow",	"FTE-Warsow",			{"basewsw/pak0.pk3"},	NULL,	{"basewsw",						"fte"},		"Warsow"},

	{"-doom",		"doom",		"FTE-Doom",				{"doom.wad"},			NULL,	{"*doom.wad",					"ftedoom"},	"Doom"},
	{"-doom2",		"doom2",	"FTE-Doom2",			{"doom2.wad"},			NULL,	{"*doom2.wad",					"ftedoom"},	"Doom2"},

	{NULL}
};

qboolean FS_GenCachedPakName(char *pname, char *crc, char *local, int llen)
{
	char *fn;
	char hex[16];
	if (strstr(pname, "dlcache"))
	{
		*local = 0;
		return false;
	}

	fn = COM_SkipPath(pname);
	if (fn == pname)
	{	//only allow it if it has some game path first.
		*local = 0;
		return false;
	}
	Q_strncpyz(local, pname, min((fn - pname) + 1, llen));
	Q_strncatz(local, "dlcache/", llen);
	Q_strncatz(local, fn, llen);
	if (*crc)
	{
		Q_strncatz(local, ".", llen);
		snprintf(hex, sizeof(hex), "%x", (unsigned int)strtoul(crc, NULL, 0));
		Q_strncatz(local, hex, llen);
	}
	return true;
}

#if 0
qboolean FS_LoadPackageFromFile(vfsfile_t *vfs, char *pname, char *localname, int *crc, unsigned int flags)
{
	int i;
	char *ext = COM_FileExtension(pname);
	searchpathfuncs_t *handle;
	searchpath_t *oldlist = NULL;

	searchpath_t *sp;

	for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
	{
		if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew)
			continue;
		if (!strcmp(ext, searchpathformats[i].extension))
		{
			handle = searchpathformats[i].OpenNew (vfs, localname);
			if (!handle)
			{
				Con_Printf("file %s isn't a %s after all\n", pname, searchpathformats[i].extension);
				break;
			}
			if (crc)
			{
				int truecrc = handle->GeneratePureCRC(handle, 0, false);
				if (truecrc != *crc)
				{
					*crc = truecrc;
					VFS_CLOSE(vfs);
					return false;
				}
			}
			sp = FS_AddPathHandle(&oldlist, pname, localname, handle, flags, (unsigned int)-1);

			if (sp)
			{
				FS_FlushFSHashReally();
				return true;
			}
		}
	}

	VFS_CLOSE(vfs);
	return false;
}
#endif

void FS_PureMode(int puremode, char *packagenames, char *packagecrcs, int pureseed)
{
	qboolean pureflush;

	Z_Free(fs_purenames);
	Z_Free(fs_purecrcs);

	pureflush = (fs_puremode != 2 && puremode == 2);
	fs_puremode = puremode;
	fs_purenames = packagenames?Z_StrDup(packagenames):NULL;
	fs_purecrcs = packagecrcs?Z_StrDup(packagecrcs):NULL;
	fs_pureseed = pureseed;

	FS_ChangeGame(fs_manifest, false);

	if (pureflush)
	{
#ifndef SERVERONLY
		Shader_NeedReload(true);
#endif
		Mod_ClearAll();
		Cache_Flush();
	}
}

#if 0
//if a server is using private pak files then load the same version of those, but deprioritise them
//crcs are not used, but matched only if the server has a different version from a previous file
void FS_ImpurePacks(const char *names, const char *crcs)
{
	int crc;
	searchpath_t *sp;
	char *pname;
	qboolean success;

	while(names)
	{
		crcs = COM_Parse(crcs);
		crc = atoi(com_token);
		names = COM_Parse(names);

		if (!crc)
			continue;

		pname = com_token;
		if (*pname == '*')
			pname++;

		for (sp = com_searchpaths; sp; sp = sp->next)
		{
			if (!stricmp(sp->purepath, pname))
			{
				break;
			}
		}
		if (!sp)
		{
			char local[MAX_OSPATH];
			vfsfile_t *vfs;

			if (FS_GenCachedPakName(pname, va("%i", crc), local, sizeof(local)))
				vfs = FS_OpenVFS(local, "rb", FS_ROOT);
			else
				vfs = NULL;
			success = false;
			if (vfs)
				success = FS_LoadPackageFromFile(vfs, pname, local, NULL, SPF_COPYPROTECTED|SPF_TEMPORARY);

			if (!success)
				Con_DPrintf("Unable to load matching package file %s\n", pname);
		}
	}

	FS_ForceToPure(NULL, NULL, 0);
}

//space-seperate pk3 names followed by space-seperated crcs
//note that we'll need to reorder and filter out files that don't match the crc.
void FS_ForceToPure(const char *names, const char *crcs, int seed)
{
	//pure files are more important than non-pure.

	searchpath_t *sp;
	searchpath_t *lastpure = NULL;
	int crc;
	qboolean waspure = com_purepaths != NULL;
	char *pname;
	searchpath_t *oldlist;

	if (!crcs || !*crcs)
	{	//pure isn't in use.
		if (com_purepaths)
		{
			Con_Printf("Pure FS deactivated\n");
			com_purepaths = NULL;
			FS_FlushFSHashReally();
		}
		return;
	}

	com_purepaths = NULL;
	for (sp = com_searchpaths; sp; sp = sp->next)
	{
		if (sp->handle->GeneratePureCRC)
		{
			sp->nextpure = (void*)0x1;
			sp->crc_check = sp->handle->GeneratePureCRC(sp->handle, seed, 0);
			sp->crc_reply = sp->handle->GeneratePureCRC(sp->handle, seed, 1);
		}
		else
		{
			sp->nextpure = NULL;
			sp->crc_check = 0;
			sp->crc_reply = 0;
		}
	}

	while(crcs)
	{
		crcs = COM_Parse(crcs);
		crc = atoi(com_token);
		names = COM_Parse(names);

		if (!crc)
			continue;

		pname = com_token;
		if (*pname == '*')
			pname++;

		for (sp = com_searchpaths; sp; sp = sp->next)
		{
			if (sp->nextpure == (void*)0x1)	//don't add twice.
				if (sp->crc_check == crc)
				{
					if (lastpure)
						lastpure->nextpure = sp;
					else
						com_purepaths = sp;
					sp->nextpure = NULL;
					lastpure = sp;
					break;
				}
		}
		if (!sp)
		{
			char local[MAX_OSPATH];
			vfsfile_t *vfs;
			char *ext = COM_FileExtension(pname);
			void *handle;
			int i;

			if (FS_GenCachedPakName(pname, va("%i", crc), local, sizeof(local)))
				vfs = FS_OpenVFS(local, "rb", FS_ROOT);
			else
				vfs = NULL;
			if (vfs)
			{
				for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
				{
					if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew)
						continue;
					if (!strcmp(ext, searchpathformats[i].extension))
					{
						handle = searchpathformats[i].OpenNew (vfs, local);
						if (!handle)
							break;
						sp = FS_AddPathHandle(&oldlist, pname, local, handle, SPF_COPYPROTECTED|SPF_TEMPORARY, (unsigned int)-1);

						sp->crc_check = sp->handle->GeneratePureCRC(sp->handle, seed, 0);
						sp->crc_reply = sp->handle->GeneratePureCRC(sp->handle, seed, 1);

						if (sp->crc_check == crc)
						{
							if (lastpure)
								lastpure->nextpure = sp;
							else
								com_purepaths = sp;
							sp->nextpure = NULL;
							lastpure = sp;
						}
						break;
					}
				}
			}

			if (!sp)
				Con_DPrintf("Pure crc %i wasn't found\n", crc);
		}
	}

	FS_FlushFSHashReally();

	if (com_purepaths && !waspure)
		Con_Printf("Pure FS activated\n");
}
#endif

char *FSQ3_GenerateClientPacksList(char *buffer, int maxlen, int basechecksum)
{	//this is for q3 compatibility.

	flocation_t loc;
	int numpaks = 0;
	searchpath_t *sp;

	FS_FLocateFile("vm/cgame.qvm", FSLFRT_LENGTH, &loc);
	Q_strncatz(buffer, va("%i ", loc.search->crc_reply), maxlen);
	basechecksum ^= loc.search->crc_reply;

	FS_FLocateFile("vm/ui.qvm", FSLFRT_LENGTH, &loc);
	Q_strncatz(buffer, va("%i ", loc.search->crc_reply), maxlen);
	basechecksum ^= loc.search->crc_reply;

	Q_strncatz(buffer, "@ ", maxlen);

	for (sp = com_purepaths; sp; sp = sp->nextpure)
	{
		if (sp->crc_reply)
		{
			Q_strncatz(buffer, va("%i ", sp->crc_reply), maxlen);
			basechecksum ^= sp->crc_reply;
			numpaks++;
		}
	}

	basechecksum ^= numpaks;
	Q_strncatz(buffer, va("%i ", basechecksum), maxlen);

	return buffer;
}

/*
================
FS_ReloadPackFiles
================

Called when the client has downloaded a new pak/pk3 file
*/
void FS_ReloadPackFilesFlags(unsigned int reloadflags)
{
	searchpath_t	*oldpaths;
	searchpath_t	*next;
	int i;

	FS_FlushFSHashReally();

	oldpaths = com_searchpaths;
	com_searchpaths = NULL;
	com_purepaths = NULL;
	com_base_searchpaths = NULL;

	for (i = 0; i < sizeof(fs_manifest->gamepath) / sizeof(fs_manifest->gamepath[0]); i++)
	{
		if (fs_manifest->gamepath[i].path && fs_manifest->gamepath[i].base)
		{
			FS_AddGameDirectory(&oldpaths, fs_manifest->gamepath[i].path, va("%s%s", com_quakedir, fs_manifest->gamepath[i].path), reloadflags);
			if (*com_homedir)
				FS_AddGameDirectory(&oldpaths, fs_manifest->gamepath[i].path, va("%s%s", com_homedir, fs_manifest->gamepath[i].path), reloadflags);
		}
	}
	com_base_searchpaths = com_searchpaths;
	for (i = 0; i < sizeof(fs_manifest->gamepath) / sizeof(fs_manifest->gamepath[0]); i++)
	{
		if (fs_manifest->gamepath[i].path && !fs_manifest->gamepath[i].base)
		{
			FS_AddGameDirectory(&oldpaths, fs_manifest->gamepath[i].path, va("%s%s", com_quakedir, fs_manifest->gamepath[i].path), reloadflags);
			if (*com_homedir)
				FS_AddGameDirectory(&oldpaths, fs_manifest->gamepath[i].path, va("%s%s", com_homedir, fs_manifest->gamepath[i].path), reloadflags);
		}
	}

	/*sv_pure: Reload pure paths*/
	if (fs_purenames && fs_purecrcs)
	{
		char crctok[64];
		char nametok[MAX_QPATH];
		searchpath_t *sp, *lastpure = NULL;
		char *names = fs_purenames, *pname;
		char *crcs = fs_purecrcs;
		int crc;

		for (sp = com_searchpaths; sp; sp = sp->next)
		{
			if (sp->handle->GeneratePureCRC)
			{
				sp->nextpure = (void*)0x1;
				sp->crc_check = sp->handle->GeneratePureCRC(sp->handle, fs_pureseed, 0);
				sp->crc_reply = sp->handle->GeneratePureCRC(sp->handle, fs_pureseed, 1);
			}
			else
			{
				sp->nextpure = NULL;
				sp->crc_check = 0;
				sp->crc_reply = 0;
			}
		}

		while(names && crcs)
		{
			crcs = COM_ParseOut(crcs, crctok, sizeof(crctok));
			names = COM_ParseOut(names, nametok, sizeof(nametok));

			crc = strtoul(crctok, NULL, 0);
			if (!crc)
				continue;

			pname = nametok;
			if (*pname == '*')	// * means that its 'referenced' (read: actually useful) thus should be downloaded, which is not relevent here.
				pname++;

			for (sp = com_searchpaths; sp; sp = sp->next)
			{
				if (sp->nextpure == (void*)0x1)	//don't add twice.
					if (sp->crc_check == crc)
					{
						if (fs_puremode)
						{
							if (lastpure)
								lastpure->nextpure = sp;
							else
								com_purepaths = sp;
							sp->nextpure = NULL;
							lastpure = sp;
						}
						break;
					}
			}
			if (!fs_puremode && !sp)
			{	//if we're not pure, we don't care if the version differs. don't load the server's version.
				//this works around 1.01 vs 1.06 issues.
				for (sp = com_searchpaths; sp; sp = sp->next)
				{
					if (!stricmp(pname, sp->purepath))
						break;
				}
			}
			//if its not already loaded (via wildcards), load it from the download cache, if we can
			if (!sp)
			{
				char local[MAX_OSPATH];
				vfsfile_t *vfs;
				char *ext = COM_FileExtension(pname);
				void *handle;
				int i;

				if (FS_GenCachedPakName(pname, va("%i", crc), local, sizeof(local)))
					vfs = FS_OpenVFS(local, "rb", FS_ROOT);
				else
					vfs = NULL;
				if (vfs)
				{
					for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
					{
						if (!searchpathformats[i].extension || !searchpathformats[i].OpenNew)
							continue;
						if (!strcmp(ext, searchpathformats[i].extension))
						{
							handle = searchpathformats[i].OpenNew (vfs, local);
							if (!handle)
								break;
							sp = FS_AddPathHandle(&oldpaths, pname, local, handle, SPF_COPYPROTECTED|SPF_TEMPORARY, (unsigned int)-1);

							sp->crc_check = sp->handle->GeneratePureCRC(sp->handle, fs_pureseed, 0);
							sp->crc_reply = sp->handle->GeneratePureCRC(sp->handle, fs_pureseed, 1);

							if (sp->crc_check == crc)
							{
								if (fs_puremode)
								{
									if (lastpure)
										lastpure->nextpure = sp;
									else
										com_purepaths = sp;
									sp->nextpure = NULL;
									lastpure = sp;
								}
							}
							break;
						}
					}
				}

				if (!sp)
					Con_DPrintf("Pure crc %i wasn't found\n", crc);
			}
		}
	}

	while(oldpaths)
	{
		next = oldpaths->next;

		Con_Printf("%s is no longer needed\n", oldpaths->logicalpath);
		oldpaths->handle->ClosePath(oldpaths->handle);
		Z_Free(oldpaths);
		oldpaths = next;
	}
}

void FS_UnloadPackFiles(void)
{
	FS_ReloadPackFilesFlags(1);
}

void FS_ReloadPackFiles(void)
{
	FS_ReloadPackFilesFlags(~0);
}

void FS_ReloadPackFiles_f(void)
{
	if (atoi(Cmd_Argv(1)))
		FS_ReloadPackFilesFlags(atoi(Cmd_Argv(1)));
	else
		FS_ReloadPackFilesFlags(~0);
}

#ifdef _WIN32
#include <windows.h>
#ifdef MINGW
#define byte BYTE	//some versions of mingw headers are broken slightly. this lets it compile.
#endif
#include <shlobj.h>
static qboolean Sys_SteamHasFile(char *basepath, int basepathlen, char *steamdir, char *fname)
{
	/*
	Find where Valve's Steam distribution platform is installed.
	Then take a look at that location for the relevent installed app.
	*/
	FILE *f;
	DWORD resultlen;
	HKEY key = NULL;
	
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
	{
		resultlen = basepathlen;
		RegQueryValueEx(key, "SteamPath", NULL, NULL, basepath, &resultlen);
		RegCloseKey(key);
		Q_strncatz(basepath, va("/SteamApps/common/%s", steamdir), basepathlen);
		if ((f = fopen(va("%s/%s", basepath, fname), "rb")))
		{
			fclose(f);
			return true;
		}
	}
	return false;
}
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen)
{
	DWORD resultlen;
	HKEY key = NULL;

#ifndef INVALID_FILE_ATTRIBUTES
	#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

	//first, try and find it in our game paths location
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\" FULLENGINENAME "\\GamePaths", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
	{
		resultlen = basepathlen;
		if (!RegQueryValueEx(key, gamename, NULL, NULL, basepath, &resultlen))
		{
			if (GetFileAttributes(basepath) != INVALID_FILE_ATTRIBUTES)
			{
				RegCloseKey(key);
				return true;
			}
		}

		RegCloseKey(key);
	}


	if (!strcmp(gamename, "quake"))
	{
		FILE *f;

		//try and find it via steam
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam\InstallPath
		//append SteamApps\common\quake
		//use it if we find winquake.exe there
		if (Sys_SteamHasFile(basepath, basepathlen, "quake", "Winquake.exe"))
			return true;
		//well, okay, so they don't have quake installed from steam.

		//quite a lot of people have it in c:\quake, as that's the default install location from the quake cd.
		if ((f = fopen("c:/quake/quake.exe", "rb")))
		{
			//HAHAHA! Found it!
			fclose(f);
			Q_strncpyz(basepath, "c:/quake", basepathlen);
			return true;
		}
	}

	if (!strcmp(gamename, "quake2"))
	{
		FILE *f;
		DWORD resultlen;
		HKEY key = NULL;

		//look for HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Quake2_exe\Path
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Quake2_exe", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "Path", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);
			if ((f = fopen(va("%s/quake2.exe", basepath), "rb")))
			{
				fclose(f);
				return true;
			}
		}

		if (Sys_SteamHasFile(basepath, basepathlen, "quake 2", "quake2.exe"))
			return true;
	}

	if (!strcmp(gamename, "et"))
	{
		FILE *f;
		DWORD resultlen;
		HKEY key = NULL;
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\Activision\Wolfenstein - Enemy Territory
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Wolfenstein - Enemy Territory", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "InstallPath", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);

			if ((f = fopen(va("%s/ET.exe", basepath), "rb")))
			{
				fclose(f);
				return true;
			}
			return true;
		}
	}

	if (!strcmp(gamename, "quake3"))
	{
		FILE *f;
		DWORD resultlen;
		HKEY key = NULL;

		//reads HKEY_LOCAL_MACHINE\SOFTWARE\id\Quake III Arena\InstallPath
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\id\\Quake III Arena", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "InstallPath", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);

			if ((f = fopen(va("%s/quake3.exe", basepath), "rb")))
			{
				fclose(f);
				return true;
			}
		}

		if (Sys_SteamHasFile(basepath, basepathlen, "quake 3 arena", "quake3.exe"))
			return true;
	}

	if (!strcmp(gamename, "wop"))
	{
		DWORD resultlen;
		HKEY key = NULL;
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\World Of Padman\Path
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\World Of Padman", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "Path", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);
			return true;
		}
	}

/*
	if (!strcmp(gamename, "d3"))
	{
		DWORD resultlen;
		HKEY key = NULL;
		//reads HKEY_LOCAL_MACHINE\SOFTWARE\id\Doom 3\InstallPath
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\id\\Doom 3", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "InstallPath", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);
			return true;
		}
	}
*/

	if (!strcmp(gamename, "hexen2") || !strcmp(gamename, "h2mp"))
	{
		//append SteamApps\common\hexen 2
		if (Sys_SteamHasFile(basepath, basepathlen, "hexen 2", "glh2.exe"))
			return true;
	}

#if !defined(NPFTE) && !defined(SERVERONLY) //this is *really* unfortunate, but doing this crashes the browser
	if (poshname && !COM_CheckParm("-manifest"))
	{
		char resultpath[MAX_PATH];
		BROWSEINFO bi;
		LPITEMIDLIST il;
		memset(&bi, 0, sizeof(bi));

		#if defined(_SDL) && defined (WIN32) && defined (MINGW) // mingw32 sdl cross compiled binary, code completely untested, doesn't crash so good sign ~moodles
			SDL_SysWMinfo wmInfo;
			SDL_GetWMInfo(&wmInfo);
			HWND sys_parentwindow = wmInfo.window;

		if (sys_parentwindow)
			bi.hwndOwner = sys_parentwindow; //note that this is usually still null
		else
		#endif
			bi.hwndOwner = mainwindow; //note that this is usually still null
		bi.pidlRoot = NULL;
		bi.pszDisplayName = resultpath;
		bi.lpszTitle = va("Please locate your existing %s installation", poshname);
		bi.ulFlags = BIF_RETURNONLYFSDIRS;
		bi.lpfn = NULL;
		bi.lParam = 0;
		bi.iImage = 0;

		il = SHBrowseForFolder(&bi);
		if (il)
		{
			SHGetPathFromIDList(il, resultpath);
			CoTaskMemFree(il);
			Q_strncpyz(basepath, resultpath, basepathlen-1);

			//and save it into the windows registry
			if (RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\" FULLENGINENAME "\\GamePaths",
				0, NULL,
				REG_OPTION_NON_VOLATILE,
				KEY_WRITE,
				NULL,
				&key,
				NULL) == ERROR_SUCCESS)
			{
				RegSetValueEx(key, gamename, 0, REG_SZ, basepath, strlen(basepath));

				RegCloseKey(key);
			}

			return true;
		}
	}
#endif

	return false;
}
#else
#ifdef __linux__
#include <sys/stat.h>
#endif
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen)
{
#ifdef __linux__
	struct stat sb;
        if (!strcmp(gamename, "quake"))
	{
		if (stat("/usr/share/quake/", &sb) == 0)
		{
			// /usr/share/quake
			if (S_ISDIR(sb.st_mode))
			{
				Q_strncpyz(basepath, "/usr/share/quake/", basepathlen);
				return true;
			}
		}
	}
#endif
	return false;
}
#endif

void FS_Shutdown(void)
{
	searchpath_t *next;
	FS_FlushFSHashReally();

	//
	// free up any current game dir info
	//
	while (com_searchpaths)
	{
		com_searchpaths->handle->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged = true;


	if (filesystemhash.numbuckets)
	{
		BZ_Free(filesystemhash.bucket);
		filesystemhash.bucket = NULL;
		filesystemhash.numbuckets = 0;
	}

	FS_Manifest_Free(fs_manifest);
	fs_manifest = NULL;
}

#if 0
static void FS_AddGamePack(const char *pakname)
{
	int j;
	char *ext = COM_FileExtension(pakname);
	vfsfile_t *vfs = VFSOS_Open(pakname, "rb");
	void *pak;
	searchpath_t *oldlist = NULL;
	if (!vfs)
		Con_Printf("Unable to open %s - missing?\n", pakname);
	else
	{
		for (j = 0; j < sizeof(searchpathformats)/sizeof(searchpathformats[0]); j++)
		{
			if (!searchpathformats[j].extension || !searchpathformats[j].OpenNew)
				continue;
			if (!strcmp(ext, searchpathformats[j].extension))
			{
				pak = searchpathformats[j].OpenNew(vfs, pakname);
				if (pak)
				{
					FS_AddPathHandle(&oldlist, "", pakname, pak, SPF_COPYPROTECTED|SPF_EXPLICIT, (unsigned int)-1);
				}
				else
				{
					Con_Printf("Unable to open %s - corrupt?\n", pakname);
					VFS_CLOSE(vfs);
				}
				vfs = NULL;
				break;
			}
		}
		if (vfs)
		{
			VFS_CLOSE(vfs);
			Con_Printf("Unable to open %s - unsupported?\n", pakname);
		}
	}
}

static void FS_StartupWithGame(int gamenum)
{
	int i;
	searchpath_t *oldlist = NULL;

#ifdef AVAIL_ZLIB
	LibZ_Init();
#endif

	Cvar_Set(&com_protocolname, gamemode_info[gamenum].protocolname);
	Cvar_ForceSet(&fs_gamename, gamemode_info[gamenum].poshname);
//	Cvar_ForceSet(&fs_gamemanifest, gamemode_info[gamenum].manifestaddr?gamemode_info[gamenum].manifestaddr:"");

	i = COM_CheckParm ("-basepack");
	while (i && i < com_argc-1)
	{
//		Con_Printf("found -basepack: %s\n", com_argv[i+1]);
		FS_AddGamePack(com_argv[i+1]);
		i = COM_CheckNextParm ("-basepack", i);
	}

//
// start up with id1 by default
//
	i = COM_CheckParm ("-basegame");
	if (i && i < com_argc-1)
	{
		do	//use multiple -basegames
		{
			FS_AddGameDirectory (&oldlist, com_argv[i+1], va("%s%s", com_quakedir, com_argv[i+1]), ~0);
			if (*com_homedir)
				FS_AddGameDirectory (&oldlist, com_argv[i+1], va("%s%s", com_homedir, com_argv[i+1]), ~0);

			i = COM_CheckNextParm ("-basegame", i);
		}
		while (i && i < com_argc-1);
	}
	else
	{
		for (i = 0; i < sizeof(gamemode_info[gamenum].dir)/sizeof(gamemode_info[gamenum].dir[0]); i++)
		{
			if (gamemode_info[gamenum].dir[i] && *gamemode_info[gamenum].dir[i] == '*')
			{
				char buf[MAX_OSPATH];
				snprintf(buf, sizeof(buf), "%s%s", com_quakedir, gamemode_info[gamenum].dir[i]+1);
				FS_AddGamePack(buf);
			}
			else if (gamemode_info[gamenum].dir[i])
			{
				FS_AddGameDirectory (&oldlist, gamemode_info[gamenum].dir[i], va("%s%s", com_quakedir, gamemode_info[gamenum].dir[i]), ~0);
				if (*com_homedir)
					FS_AddGameDirectory (&oldlist, gamemode_info[gamenum].dir[i], va("%s%s", com_homedir, gamemode_info[gamenum].dir[i]), ~0);
			}
		}
	}

	i = COM_CheckParm ("-addbasegame");
	while (i && i < com_argc-1)	//use multiple -addbasegames (this is so the basic dirs don't die)
	{
		//reject various evil path arguments.
		if (*com_argv[i+1] && !(strchr(com_argv[i+1], '.') || strchr(com_argv[i+1], ':') || strchr(com_argv[i+1], '?') || strchr(com_argv[i+1], '*') || strchr(com_argv[i+1], '/') || strchr(com_argv[i+1], '\\') || strchr(com_argv[i+1], '$')))
		{
			FS_AddGameDirectory (&oldlist, com_argv[i+1], va("%s%s", com_quakedir, com_argv[i+1]), ~0);
			if (*com_homedir)
				FS_AddGameDirectory (&oldlist, com_argv[i+1], va("%s%s", com_homedir, com_argv[i+1]), ~0);
		}

		i = COM_CheckNextParm ("-addbasegame", i);
	}

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	//-game specifies the mod gamedir to use in NQ
	i = COM_CheckParm ("-game");	//effectivly replace with +gamedir x (But overridable)
	if (i && i < com_argc-1)
	{
		COM_Gamedir(com_argv[i+1]);
#ifndef CLIENTONLY
		Info_SetValueForStarKey (svs.info, "*gamedir", com_argv[i+1], MAX_SERVERINFO_STRING);
#endif
	}

	//+gamedir specifies the mod gamedir to use in QW
	//hack - we parse the commandline after the config so commandline always overrides
	//but this means ktpro/server.cfg (for example) is not found
	//so if they specify a gamedir on the commandline, let the default configs be loaded from that gamedir
	//note that -game +gamedir will result in both being loaded. but hey, who cares
	i = COM_CheckParm ("+gamedir");	//effectivly replace with +gamedir x (But overridable)
	if (i && i < com_argc-1)
	{
		COM_Gamedir(com_argv[i+1]);
	}

#ifdef ANDROID
	{
		vfsfile_t *f;
		//write a .nomedia file to avoid people from getting random explosion sounds etc intersperced with their music
		f = FS_OpenVFS(".nomedia", "rb", FS_ROOT);
		if (f)
			VFS_CLOSE(f);
		else
			FS_WriteFile(".nomedia", NULL, 0, FS_ROOT);
	}
#endif

	if (gamemode_info[gamenum].customexec)
		Cbuf_AddText(gamemode_info[gamenum].customexec, RESTRICT_LOCAL);
}
#endif

//just check each possible file, see if one is there.
static qboolean FS_DirHasGame(char *basedir, int gameidx)
{
	int j;
	vfsfile_t *f;
	for (j = 0; j < 4; j++)
	{
		if (!gamemode_info[gameidx].auniquefile[j])
			continue;	//no more
		f = VFSOS_Open(va("%s%s", basedir, gamemode_info[gameidx].auniquefile[j]), "rb");
		if (f)
		{
			VFS_CLOSE(f);
			return true;
		}
	}
	return false;
}

//check em all
static int FS_IdentifyDefaultGameFromDir(char *basedir)
{
	int i;
	for (i = 0; gamemode_info[i].argname; i++)
	{
		if (FS_DirHasGame(basedir, i))
			return i;
	}
	return -1;
}

//attempt to work out which game we're meant to be trying to run based upon a few things
//1: fs_changegame console command override. fixme: needs to cope with manifests too.
//2: -quake3 argument implies that the user wants to run quake3.
//3: if we are ftequake3.exe then we always try to run quake3.
//4: identify characteristic files within the working directory (like id1/pak0.pak implies we're running quake)
//5: check where the exe actually is instead of simply where we're being run from.
//6: fallback to quake
//if autobasedir is not set, block gamedir changes/prompts.
static int FS_IdentifyDefaultGame(char *newbase, int sizeof_newbase, qboolean fixedbase)
{
	int i;
	int gamenum = -1;

	if (gamenum == -1)
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (COM_CheckParm(gamemode_info[i].argname))
			{
				gamenum = i;
				break;
			}
		}
	}
	//use the game based on an exe name over the filesystem one (could easily have multiple fs path matches).
	if (gamenum == -1)
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{
			char *ev = COM_SkipPath(com_argv[0]);
			ev = strstr(ev, gamemode_info[i].exename);
			if (ev && (!strchr(ev, '\\') && !strchr(ev, '/')))
				gamenum = i;
		}
	}

	//identify the game from a telling file in the working directory
	if (gamenum == -1)
		gamenum = FS_IdentifyDefaultGameFromDir(newbase);
	//identify the game from a telling file relative to the exe's directory. for when shortcuts don't set the working dir sensibly.
	if (gamenum == -1 && host_parms.binarydir && *host_parms.binarydir && !fixedbase)
	{
		gamenum = FS_IdentifyDefaultGameFromDir(host_parms.binarydir);
		if (gamenum != -1)
			Q_strncpyz(newbase, host_parms.binarydir, sizeof_newbase);
	}

	//still failed? find quake and use that one by default
	if (gamenum<0)
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (!strcmp(gamemode_info[i].argname, "-quake"))
			{
				gamenum = i;
				break;
			}
		}
	}
	return gamenum;
}

//allowed to modify newbasedir if fixedbasedir isn't set
ftemanifest_t *FS_GenerateLegacyManifest(char *newbasedir, int sizeof_newbasedir, qboolean fixedbasedir, int game)
{
	int i;
	ftemanifest_t *man;

	if (game == -1)
		game = FS_IdentifyDefaultGame(newbasedir, sizeof_newbasedir, fixedbasedir);
	if (gamemode_info[game].manifestfile)
		man = FS_Manifest_Parse(gamemode_info[game].manifestfile);
	else
	{
		man = FS_Manifest_Create();

		Cmd_TokenizeString(va("game \"%s\"", gamemode_info[game].argname+1), false, false);
		FS_Manifest_ParseTokens(man);
		if (gamemode_info[game].poshname)
		{
			Cmd_TokenizeString(va("name \"%s\"", gamemode_info[game].poshname), false, false);
			FS_Manifest_ParseTokens(man);
		}

		i = COM_CheckParm ("-basegame");
		if (i)
		{
			do
			{
				Cmd_TokenizeString(va("basegame \"%s\"", com_argv[i+1]), false, false);
				FS_Manifest_ParseTokens(man);

				i = COM_CheckNextParm ("-basegame", i);
			}
			while (i && i < com_argc-1);
		}

		i = COM_CheckParm ("-game");
		if (i)
		{
			do
			{
				Cmd_TokenizeString(va("gamedir \"%s\"", com_argv[i+1]), false, false);
				FS_Manifest_ParseTokens(man);

				i = COM_CheckNextParm ("-game", i);
			}
			while (i && i < com_argc-1);
		}

		i = COM_CheckParm ("+gamedir");
		if (i)
		{
			do
			{
				Cmd_TokenizeString(va("gamedir \"%s\"", com_argv[i+1]), false, false);
				FS_Manifest_ParseTokens(man);

				i = COM_CheckNextParm ("+gamedir", i);
			}
			while (i && i < com_argc-1);
		}
	}
	return man;
}

static char *FS_RelativeURL(char *base, char *file, char *buffer, int bufferlen)
{
	//fixme: cope with windows paths
	qboolean baseisurl = !!strchr(base, ':');
	qboolean fileisurl = !!strchr(file, ':');
	//qboolean baseisabsolute = (*base == '/' || *base == '\\');
	qboolean fileisabsolute = (*file == '/' || *file == '\\');
	char *ebase;
	
	if (fileisurl)
		return file;
	if (fileisabsolute)
	{
		if (baseisurl)
		{
			ebase = strchr(base, ':');
			ebase++;
			while(*ebase == '/')
				ebase++;
			while(*ebase && *ebase != '/')
				ebase++;
		}
		else
			ebase = base;
	}
	else
		ebase = COM_SkipPath(base);
	memcpy(buffer, base, ebase-base);
	strcpy(buffer+(ebase-base), file);

	return buffer;
}

#ifdef WEBCLIENT
static struct dl_download *curpackagedownload;
static char fspdl_temppath[MAX_OSPATH];
static char fspdl_finalpath[MAX_OSPATH];
static void FS_BeginNextPackageDownload(void);
static void FS_PackageDownloaded(struct dl_download *dl)
{
	curpackagedownload = NULL;
	
	if (dl->file)
	{
		VFS_CLOSE(dl->file);
		dl->file = NULL;
	}
	if (dl->status == DL_FINISHED)
	{
		//rename the file as needed.
		COM_CreatePath(fspdl_finalpath);
		if (!Sys_Rename(fspdl_temppath, fspdl_finalpath))
		{
			Con_Printf("Unable to rename \"%s\" to \"%s\"\n", fspdl_temppath, fspdl_finalpath);
		}
	}
	Sys_remove (fspdl_temppath);

	FS_ChangeGame(fs_manifest, true);

	FS_BeginNextPackageDownload();
}
static void FS_BeginNextPackageDownload(void)
{
	int j;
	ftemanifest_t *man = fs_manifest;
	vfsfile_t *check;
	if (curpackagedownload || !man)
		return;

	for (j = 0; j < sizeof(fs_manifest->package) / sizeof(fs_manifest->package[0]); j++)
	{
		char buffer[MAX_OSPATH], *url;
		if (!man->package[j].path)
			continue;

		if (!FS_GenCachedPakName(man->package[j].path, va("%#x", man->package[j].crc), buffer, sizeof(buffer)))
			continue;

		check = FS_OpenVFS(buffer, "rb", FS_ROOT);
		if (check)
		{
			VFS_CLOSE(check);
			continue;
		}
		FS_NativePath(buffer, FS_ROOT, fspdl_finalpath, sizeof(fspdl_finalpath));
		if (!FS_GenCachedPakName(va("%s.tmp", man->package[j].path), va("%#x", man->package[j].crc), buffer, sizeof(buffer)))
			continue;
		FS_NativePath(buffer, FS_ROOT, fspdl_temppath, sizeof(fspdl_temppath));

		url = NULL;
		while(!url)
		{
			//ran out of mirrors?
			if (man->package[j].mirrornum == (sizeof(man->package[j].mirrors) / sizeof(man->package[j].mirrors[0])))
				break;

			if (man->package[j].mirrors[man->package[j].mirrornum])
				url = FS_RelativeURL(man->updateurl, man->package[j].mirrors[man->package[j].mirrornum], buffer, sizeof(buffer));
			man->package[j].mirrornum++;
		}
		//no valid mirrors
		if (!url)
			continue;

		curpackagedownload = HTTP_CL_Get(url, NULL, FS_PackageDownloaded);
		if (curpackagedownload)
		{
			COM_CreatePath(fspdl_temppath);
			curpackagedownload->file = VFSOS_Open(fspdl_temppath, "wb");
			return;
		}
	}
}
#else
void FS_BeginNextPackageDownload(void)
{
}
#endif

//this is potentially unsafe. needs lots of testing.
qboolean FS_ChangeGame(ftemanifest_t *man, qboolean allowreloadconfigs)
{
	int i, j;
	char realpath[MAX_OSPATH-1];
	char newbasedir[MAX_OSPATH];
	qboolean fixedbasedir;
	qboolean reloadconfigs = false;
	flocation_t loc;

	//if any of these files change location, the configs will be re-execed.
	//note that we reuse path handles if they're still valid, so we can just check the pointer to see if it got unloaded/replaced.
	char *conffile[] = {"quake.rc", "hexen.rc", "default.cfg", "server.cfg", NULL};
	searchpath_t *confpath[sizeof(conffile)/sizeof(conffile[0])];
	for (i = 0; conffile[i]; i++)
	{
		FS_FLocateFile(conffile[i], FSLFRT_IFFOUND, &loc);	//q1
		confpath[i] = loc.search;
	}

	i = COM_CheckParm ("-basedir");
	fixedbasedir = i && i < com_argc-1;
	Q_strncpyz (newbasedir, fixedbasedir?com_argv[i+1]:host_parms.basedir, sizeof(newbasedir));

	//make sure it has a trailing slash, or is empty. woo.
	FS_CleanDir(newbasedir, sizeof(newbasedir));

	if (!man)
	{
		//if we're already running a game, don't autodetect.
		if (fs_manifest)
			return false;

		man = FS_GenerateLegacyManifest(newbasedir, sizeof(newbasedir), fixedbasedir, -1);
	}

	if (man == fs_manifest)
	{
		//don't close anything. theoretically nothing is changing, and we don't want to load new defaults either.
	}
	else if (!fs_manifest || !strcmp(fs_manifest->installation?fs_manifest->installation:"", man->installation?man->installation:""))
	{
		if (!fs_manifest)
			reloadconfigs = true;
		FS_Manifest_Free(fs_manifest);
	}
	else
	{
		FS_Shutdown();

		reloadconfigs = true;
	}
	fs_manifest = man;

	if (man->installation && *man->installation)
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (!strcmp(man->installation, gamemode_info[i].argname+1))
			{
				//if there's no base dirs, edit the manifest to give it its default ones.
				for (j = 0; j < sizeof(man->gamepath) / sizeof(man->gamepath[0]); j++)
				{
					if (man->gamepath[j].path && man->gamepath[j].base)
						break;
				}
				if (j == sizeof(man->gamepath) / sizeof(man->gamepath[0]))
				{
					for (j = 0; j < 4; j++)
						if (gamemode_info[i].dir[j])
						{
							Cmd_TokenizeString(va("basegame \"%s\"", gamemode_info[i].dir[j]), false, false);
							FS_Manifest_ParseTokens(man);
						}
				}

				if (!man->protocolname && *gamemode_info[i].protocolname)
				{
					Cmd_TokenizeString(va("protocolname \"%s\"", gamemode_info[i].protocolname), false, false);
					FS_Manifest_ParseTokens(man);
				}
				if (!man->defaultexec && gamemode_info[i].customexec)
				{
					man->defaultexec = Z_StrDup(gamemode_info[i].customexec);
				}

				if (!fixedbasedir && !FS_DirHasGame(newbasedir, i))
					if (Sys_FindGameData(man->formalname, man->installation, realpath, sizeof(realpath)))
						Q_strncpyz (newbasedir, realpath, sizeof(newbasedir));
				break;
			}
		}
	}
	Q_strncpyz (com_quakedir, newbasedir, sizeof(com_quakedir));
	//make sure it has a trailing slash, or is empty. woo.
	FS_CleanDir(com_quakedir, sizeof(com_quakedir));

#ifdef ANDROID
	{
		vfsfile_t *f;
		//write a .nomedia file to avoid people from getting random explosion sounds etc intersperced with their music
		f = FS_OpenVFS(".nomedia", "rb", FS_ROOT);
		if (f)
			VFS_CLOSE(f);
		else
			FS_WriteFile(".nomedia", NULL, 0, FS_ROOT);
	}
#endif

	FS_ReloadPackFilesFlags(~0);

	FS_BeginNextPackageDownload();


	if (allowreloadconfigs)
	{
		for (i = 0; conffile[i]; i++)
		{
			FS_FLocateFile(conffile[i], FSLFRT_IFFOUND, &loc);
			if (confpath[i] != loc.search)
				reloadconfigs = true;
		}

		if (reloadconfigs)
		{
			//FIXME: flag this instead and do it after a delay
			Cvar_ForceSet(&fs_gamename, man->formalname?man->formalname:"FTE");
			Cvar_ForceSet(&com_protocolname, man->protocolname?man->protocolname:"FTE");

			if (isDedicated)
			{
#ifndef CLIENTONLY
				SV_ExecInitialConfigs(man->defaultexec?man->defaultexec:"");
#endif
			}
			else
			{
#ifndef SERVERONLY
				CL_ExecInitialConfigs(man->defaultexec?man->defaultexec:"");
#endif
			}
		}
	}

	COM_Effectinfo_Clear();
#ifndef SERVERONLY
	Validation_FlushFileList();	//prevent previous hacks from making a difference.
#endif

	return true;
}

void FS_ChangeGame_f(void)
{
	int i;
	char *arg = Cmd_Argv(1);

	if (!*arg)
	{
		Con_Printf("Valid games are:\n");
		for (i = 0; gamemode_info[i].argname; i++)
		{
			Con_Printf(" %s\n", gamemode_info[i].argname+1);
		}
	}
	else
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{			
			if (!stricmp(gamemode_info[i].argname+1, arg))
			{
				Con_Printf("Switching to %s\n", gamemode_info[i].argname+1);
				FS_ChangeGame(FS_GenerateLegacyManifest(NULL, 0, true, i), true);
				return;
			}
		}
		Con_Printf("Game unknown\n");
	}
}
void FS_ShowManifest_f(void)
{
	if (fs_manifest)
		FS_Manifest_Print(fs_manifest);
	else
		Con_Printf("no manifest loaded...\n");
}
/*
================
COM_InitFilesystem

note: does not actually load any packs, just makes sure the basedir+cvars+etc is set up. vfs_fopens will still fail.
================
*/
void COM_InitFilesystem (void)
{
	int		i;

	char *ev;
	qboolean usehome;

	FS_RegisterDefaultFileSystems();

	Cmd_AddCommand("fs_restart", FS_ReloadPackFiles_f);
	Cmd_AddCommand("fs_changegame", FS_ChangeGame_f);
	Cmd_AddCommand("fs_showmanifest", FS_ShowManifest_f);

//
// -basedir <path>
// Overrides the system supplied base directory (under id1)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		strcpy (com_quakedir, com_argv[i+1]);
	else
		strcpy (com_quakedir, host_parms.basedir);

	FS_CleanDir(com_quakedir, sizeof(com_quakedir));



	Cvar_Register(&fs_gamename, "FS");
	Cvar_Register(&fs_gamemanifest, "FS");
	Cvar_Register(&com_protocolname, "Server Info");
	Cvar_Register(&com_modname, "Server Info");

	usehome = false;

#ifdef _WIN32
	{	//win32 sucks.
		HMODULE shfolder = LoadLibrary("shfolder.dll");
		DWORD winver = (DWORD)LOBYTE(LOWORD(GetVersion()));

		if (shfolder)
		{
			HRESULT (WINAPI *dSHGetFolderPath) (HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath);
			dSHGetFolderPath = (void *)GetProcAddress(shfolder, "SHGetFolderPathA");
			if (dSHGetFolderPath)
			{
				char folder[MAX_PATH];
				// 0x5 == CSIDL_PERSONAL
				if (dSHGetFolderPath(NULL, 0x5, NULL, 0, folder) == S_OK)
					Q_snprintfz(com_homedir, sizeof(com_homedir), "%s/My Games/%s/", folder, FULLENGINENAME);
			}
//			FreeLibrary(shfolder);
		}

		if (!*com_homedir)
		{
			ev = getenv("USERPROFILE");
			if (ev)
				Q_snprintfz(com_homedir, sizeof(com_homedir), "%s/My Documents/My Games/%s/", ev, FULLENGINENAME);
		}

#ifdef NPFTE
		if (!*com_homedir)
			Q_snprintfz(com_homedir, sizeof(com_homedir), "/%s/", FULLENGINENAME);
		//as a browser plugin, always use their home directory
		usehome = true;
#else
		/*would it not be better to just check to see if we have write permission to the basedir?*/
		if (winver >= 0x6) // Windows Vista and above
			usehome = true; // always use home directory by default, as Vista+ mimics this behavior anyway
		else if (winver >= 0x5) // Windows 2000/XP/2003
		{
			HMODULE advapi32;
			advapi32 = LoadLibrary("advapi32.dll");

			if (advapi32)
			{
				BOOL (WINAPI *dCheckTokenMembership) (HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember);
				dCheckTokenMembership = (void *)GetProcAddress(advapi32, "CheckTokenMembership");

				if (dCheckTokenMembership)
				{
					// on XP systems, only use a home directory by default if we're a limited user or if we're on a network
					BOOL isadmin, isonnetwork;
					SID_IDENTIFIER_AUTHORITY ntauth = {SECURITY_NT_AUTHORITY};
					PSID adminSID, networkSID;

					isadmin = AllocateAndInitializeSid(&ntauth,
						2,
						SECURITY_BUILTIN_DOMAIN_RID,
						DOMAIN_ALIAS_RID_ADMINS,
						0, 0, 0, 0, 0, 0,
						&adminSID);

					// just checking the network rid should be close enough to matching domain logins
					isonnetwork = AllocateAndInitializeSid(&ntauth,
						1,
						SECURITY_NETWORK_RID,
						0, 0, 0, 0, 0, 0, 0,
						&networkSID);

					if (isadmin && !dCheckTokenMembership(0, adminSID, &isadmin))
						isadmin = 0;

					if (isonnetwork && !dCheckTokenMembership(0, networkSID, &isonnetwork))
						isonnetwork = 0;

					usehome = isonnetwork || !isadmin;

					FreeSid(networkSID);
					FreeSid(adminSID);
				}

				FreeLibrary(advapi32);
			}
		}
#endif
	}
#else
	//yay for unix!.
	ev = getenv("HOME");
	if (ev && *ev)
	{
		if (ev[strlen(ev)-1] == '/')
			Q_snprintfz(com_homedir, sizeof(com_homedir), "%s.fte/", ev);
		else
			Q_snprintfz(com_homedir, sizeof(com_homedir), "%s/.fte/", ev);
		usehome = true; // always use home on unix unless told not to
	}
	else
		*com_homedir = '\0';
#endif

	if (!usehome && !COM_CheckParm("-usehome"))
		*com_homedir = '\0';

	if (COM_CheckParm("-nohome"))
		*com_homedir = '\0';

	if (*com_homedir)
		Con_Printf("Using home directory \"%s\"\n", com_homedir);

#ifdef PLUGINS
	Plug_Initialise(false);
#endif
}





//this is at the bottom of the file to ensure these globals are not used elsewhere
extern searchpathfuncs_t *(QDECL VFSOS_OpenPath) (vfsfile_t *file, const char *desc);
#ifdef AVAIL_ZLIB
extern searchpathfuncs_t *(QDECL FSZIP_LoadArchive) (vfsfile_t *packhandle, const char *desc);
#endif
extern searchpathfuncs_t *(QDECL FSPAK_LoadArchive) (vfsfile_t *packhandle, const char *desc);
#ifdef DOOMWADS
extern searchpathfuncs_t *(QDECL FSDWD_LoadArchive) (vfsfile_t *packhandle, const char *desc);
#endif
void FS_RegisterDefaultFileSystems(void)
{
	FS_RegisterFileSystemType(NULL, "pak", FSPAK_LoadArchive, true);
#if !defined(_WIN32) && !defined(ANDROID)
	/*for systems that have case sensitive paths, also include *.PAK */
	FS_RegisterFileSystemType(NULL, "PAK", FSPAK_LoadArchive, true);
#endif
#ifdef AVAIL_ZLIB
	FS_RegisterFileSystemType(NULL, "pk3", FSZIP_LoadArchive, true);
	FS_RegisterFileSystemType(NULL, "pk4", FSZIP_LoadArchive, true);
	FS_RegisterFileSystemType(NULL, "apk", FSZIP_LoadArchive, false);
	FS_RegisterFileSystemType(NULL, "zip", FSZIP_LoadArchive, false);
#endif
#ifdef DOOMWADS
	FS_RegisterFileSystemType(NULL, "wad", FSDWD_LoadArchive, true);
#endif
}
