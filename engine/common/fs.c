#include "quakedef.h"
#include "netinc.h"

//#define com_gamedir com__gamedir

#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include "fs.h"
#include "shader.h"

#if defined(MINGW) && defined(_SDL)
#include "./mingw-libs/SDL_syswm.h" // mingw sdl cross binary complains off sys_parentwindow
#endif

hashtable_t filesystemhash;
qboolean com_fschanged = true;
static unsigned int fs_restarts;
extern cvar_t com_fs_cache;
int active_fs_cachetype;
static int fs_referencetype;

struct
{
	const char *extension;
	searchpathfuncs_t *funcs;
} searchpathformats[64];

int FS_RegisterFileSystemType(const char *extension, searchpathfuncs_t *funcs)
{
	unsigned int i;
	for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
	{
		if (searchpathformats[i].extension && !strcmp(searchpathformats[i].extension, extension))
			break;	//extension match always replaces
		if (!searchpathformats[i].extension && !searchpathformats[i].funcs)
			break;
	}
	if (i == sizeof(searchpathformats)/sizeof(searchpathformats[0]))
		return 0;

	searchpathformats[i].extension = extension;
	searchpathformats[i].funcs = funcs;
	com_fschanged = true;

	return i+1;
}

void FS_UnRegisterFileSystemType(int idx)
{
	if ((unsigned int)(idx-1) >= sizeof(searchpathformats)/sizeof(searchpathformats[0]))
		return;

	searchpathformats[idx-1].funcs = NULL;
	com_fschanged = true;

	//FS_Restart will be needed
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
qboolean com_file_copyprotected;


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







//======================================================================================================





typedef struct searchpath_s
{
	const searchpathfuncs_t *funcs;
	qboolean copyprotected;	//don't allow downloads from here.
	qboolean istemporary;
	qboolean isexplicit;	//explicitly loaded (ie: id1|qw|$gamedir|fte)
	qboolean referenced;
	void *handle;

	char purepath[256];	//server tracks the path used to load them so it can tell the client
	int crc_check;	//client sorts packs according to this checksum
	int crc_reply;	//client sends a different crc back to the server, for the paks it's actually loaded.

	struct searchpath_s *next;
	struct searchpath_s *nextpure;
} searchpath_t;

searchpath_t	*com_searchpaths;
searchpath_t	*com_purepaths;
searchpath_t	*com_base_searchpaths;	// without gamedirs

/*
================
COM_filelength
================
*/
int COM_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}
/*
static int COM_FileOpenRead (char *path, FILE **hndl)
{
	FILE	*f;

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return COM_filelength(f);
}
*/

int COM_FileSize(const char *path)
{
	int len;
	flocation_t loc;
	len = FS_FLocateFile(path, FSLFRT_LENGTH, &loc);
	return len;
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

	if (com_purepaths)
	{
		Con_Printf ("Pure paths:\n");
		for (s=com_purepaths ; s ; s=s->nextpure)
		{
			if (s->referenced)
				Con_Printf("*");
			s->funcs->PrintPath(s->handle);
		}
		Con_Printf ("----------\n");
		Con_Printf ("Impure paths:\n");
	}


	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s == com_base_searchpaths)
			Con_Printf ("----------\n");

		if (s->referenced)
			Con_Printf("*");
		s->funcs->PrintPath(s->handle);
	}
}


/*
============
COM_Dir_f

============
*/
static int COM_Dir_List(const char *name, int size, void *parm)
{
	Con_Printf("%s  (%i)\n", name, size);
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
	else
		strncat(match, "/*", sizeof(match)-1);

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
			Con_Printf("File is %i bytes compressed inside ", loc.len);
			loc.search->funcs->PrintPath(loc.search->handle);
		}
		else
		{
			Con_Printf("Inside %s (%i bytes)\n", loc.rawname, loc.len);
			loc.search->funcs->PrintPath(loc.search->handle);
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

void FS_FlushFSHash(void)
{
	if (filesystemhash.numbuckets)
	{
		int i;
		bucket_t *bucket, *next;

		for (i = 0; i < filesystemhash.numbuckets; i++)
		{
			bucket = filesystemhash.bucket[i];
			filesystemhash.bucket[i] = NULL;
			while(bucket)
			{
				next = bucket->next;
				if (bucket->key.string == (char*)(bucket+1))
					Z_Free(bucket);
				bucket = next;
			}
		}
	}

	com_fschanged = true;
}

void FS_RebuildFSHash(void)
{
	searchpath_t	*search;
	if (!filesystemhash.numbuckets)
	{
		filesystemhash.numbuckets = 1024;
		filesystemhash.bucket = (bucket_t**)Z_Malloc(Hash_BytesForBuckets(filesystemhash.numbuckets));
	}
	else
	{
		FS_FlushFSHash();
	}
	Hash_InitTable(&filesystemhash, filesystemhash.numbuckets, filesystemhash.bucket);

	fs_hash_dups = 0;
	fs_hash_files = 0;

	if (com_purepaths)
	{	//go for the pure paths first.
		for (search = com_purepaths; search; search = search->nextpure)
		{
			search->funcs->BuildHash(search->handle);
		}
	}
	for (search = com_searchpaths ; search ; search = search->next)
	{
		search->funcs->BuildHash(search->handle);
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
			if (search->funcs->FindFile(search->handle, loc, filename, pf))
			{
				if (loc)
				{
					search->referenced |= fs_referencetype;
					loc->search = search;
					len = loc->len;
				}
				else
					len = 0;
				com_file_copyprotected = search->copyprotected;
				goto out;
			}
			depth += (search->isexplicit || returntype == FSLFRT_DEPTH_ANYPATH);
		}
	}

//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (search->funcs->FindFile(search->handle, loc, filename, pf))
		{
			search->referenced |= fs_referencetype;
			if (loc)
			{
				loc->search = search;
				len = loc->len;
			}
			else
				len = 1;
			com_file_copyprotected = search->copyprotected;
			goto out;
		}
		depth += (search->isexplicit || returntype == FSLFRT_DEPTH_ANYPATH);
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

	for (search = com_purepaths ; search ; search = search->nextpure)
	{
		if (!strcmp(package, search->purepath))
			return !search->copyprotected;
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
			if (!search->crc_check && search->funcs->GeneratePureCRC)
				search->crc_check = search->funcs->GeneratePureCRC(search->handle, 0, 0);
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
			if (referencedonly == 0 && !search->referenced)
				continue;
			if (referencedonly == 2 && search->referenced)
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
			if (!search->crc_check && search->funcs->GeneratePureCRC)
				search->crc_check = search->funcs->GeneratePureCRC(search->handle, 0, 0);
			if (search->crc_check)
			{
				if (referencedonly == 0 && !search->referenced)
					continue;
				if (referencedonly == 2 && search->referenced)
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
		}
		return buffer;
	}
}

void FS_ReferenceControl(unsigned int refflag, unsigned int resetflags)
{
	searchpath_t	*s;
	if (resetflags)
	{
		for (s=com_searchpaths ; s ; s=s->next)
		{
			s->referenced &= ~resetflags;
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
		return FS_DecompressGZip(handle);
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
		if (*com_homedir)
			snprintf(out, outlen, "%sqw/skins/%s", com_homedir, fname);
		else
			snprintf(out, outlen, "%sqw/skins/%s", com_quakedir, fname);
		break;
	case FS_ROOT:
		if (*com_homedir)
			snprintf(out, outlen, "%s%s", com_homedir, fname);
		else
			snprintf(out, outlen, "%s%s", com_quakedir, fname);
		break;
	case FS_CONFIGONLY:
		if (*com_homedir)
			snprintf(out, outlen, "%sfte/%s", com_homedir, fname);
		else
			snprintf(out, outlen, "%sfte/%s", com_quakedir, fname);
		break;
	default:
		Sys_Error("FS_Rename case not handled\n");
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
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%s%s/%s", com_quakedir, gamedirfile, filename);
		return VFSOS_Open(fullname, mode);
	case FS_GAME:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_homedir, gamedirfile, filename);
		else
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_quakedir, gamedirfile, filename);
		break;
	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_homedir, filename);
		else
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_quakedir, filename);
		break;
	case FS_ROOT:
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%s%s", com_homedir, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%s%s", com_quakedir, filename);
		return VFSOS_Open(fullname, mode);
	case FS_CONFIGONLY:
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
		com_file_copyprotected = loc.search->copyprotected;
		return VFS_Filter(filename, loc.search->funcs->OpenVFS(loc.search->handle, &loc, mode));
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
		com_file_copyprotected = location->search->copyprotected;
		return VFS_Filter(NULL, location->search->funcs->OpenVFS(location->search->handle, location, "rb"));
	}
	return NULL;
}

int FS_Rename2(const char *oldf, const char *newf, enum fs_relative oldrelativeto, enum fs_relative newrelativeto)
{
	char oldfullname[MAX_OSPATH];
	char newfullname[MAX_OSPATH];

	if (!FS_NativePath(oldf, oldrelativeto, oldfullname, sizeof(oldfullname)))
		return EPERM;
	if (!FS_NativePath(newf, newrelativeto, newfullname, sizeof(newfullname)))
		return EPERM;

	FS_CreatePath(newf, newrelativeto);
	return rename(oldfullname, newfullname);
}
int FS_Rename(const char *oldf, const char *newf, enum fs_relative relativeto)
{
	return FS_Rename2(oldf, newf, relativeto, relativeto);
}
int FS_Remove(const char *fname, enum fs_relative relativeto)
{
	char fullname[MAX_OSPATH];

	if (!FS_NativePath(fname, relativeto, fullname, sizeof(fullname)))
		return EPERM;

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


	f = loc.search->funcs->OpenVFS(loc.search->handle, &loc, "rb");
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
qbyte *COM_LoadTempFile2 (const char *path)
{
	return COM_LoadFile (path, 6);
}

void COM_LoadCacheFile (const char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
qbyte *COM_LoadStackFile (const char *path, void *buffer, int bufsize)
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



void COM_EnumerateFiles (const char *match, int (*func)(const char *, int, void *), void *parm)
{
	searchpath_t    *search;
	for (search = com_searchpaths; search ; search = search->next)
	{
	// is the element a pak file?
		if (!search->funcs->EnumerateFiles(search->handle, match, func, parm))
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
		if (sp->istemporary)
		{
			FS_FlushFSHash();

			*link = sp->next;

			sp->funcs->ClosePath(sp->handle);
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
/*
	dpackheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		info;
	int fstart;
	char *ballsup;

	flocation_t loc;

	FS_FLocateFile(filename, FSLFRT_LENGTH, &loc);

	if (!loc.search)
	{
		Con_Printf("Couldn't refind file\n");
		return false;
	}

	if (!*loc.rawname)
	{
		Con_Printf("File %s is compressed\n");
		return false;
	}
	packhandle = fopen(loc.rawname, "rb");
	if (!packhandle)
	{
		Con_Printf("Couldn't reopen file\n");
		return false;
	}
	fseek(packhandle, loc.offset, SEEK_SET);

	fstart = loc.offset;
	fseek(packhandle, ofs+fstart, SEEK_SET);

	fread (&header, 1, sizeof(header), packhandle);
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
	{
		return false;
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	newfiles = (packfile_t*)Z_Malloc (numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs+fstart, SEEK_SET);

	pack = (pack_t*)Z_Malloc (sizeof (pack_t));

// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		fread (&info, 1, sizeof(info), packhandle);

		strcpy (newfiles[i].name, info.name);
		Q_strlwr(newfiles[i].name);
		while ((ballsup = strchr(newfiles[i].name, '\\')))
			*ballsup = '/';
		newfiles[i].filepos = LittleLong(info.filepos)+fstart;
		newfiles[i].filelen = LittleLong(info.filelen);
	}

	strcpy (pack->filename, loc.rawname);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Con_TPrintf (TL_ADDEDPACKFILE, filename, numpackfiles);

	COM_AddPathHandle(&packfilefuncs, pack, true, true);
	return true;
*/
}

static searchpath_t *FS_AddPathHandle(const char *purepath, const char *probablepath, const searchpathfuncs_t *funcs, void *handle, qboolean copyprotect, qboolean istemporary, qboolean isexplicit, unsigned int loadstuff);

typedef struct {
	const searchpathfuncs_t *funcs;
	searchpath_t *parentpath;
	const char *parentdesc;
	const char *puredesc;
} wildpaks_t;

static int FS_AddWildDataFiles (const char *descriptor, int size, void *vparam)
{
	wildpaks_t *param = vparam;
	vfsfile_t *vfs;
	const searchpathfuncs_t *funcs = param->funcs;
	searchpath_t	*search;
	void			*pak;
	char			pakfile[MAX_OSPATH];
	char			purefile[MAX_OSPATH];
	flocation_t loc;

	Q_snprintfz (pakfile, sizeof(pakfile), "%s%s", param->parentdesc, descriptor);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (search->funcs != funcs)
			continue;
		if (!stricmp((char*)search->handle, pakfile))	//assumption: first member of structure is a char array
			return true; //already loaded (base paths?)
	}

	search = param->parentpath;

	if (!search->funcs->FindFile(search->handle, &loc, descriptor, NULL))
		return true;	//not found..
	vfs = search->funcs->OpenVFS(search->handle, &loc, "rb");
	if (!vfs)
		return true;
	pak = funcs->OpenNew (vfs, pakfile);
	if (!pak)
		return true;

	Q_snprintfz (pakfile, sizeof(pakfile), "%s%s/", param->parentdesc, descriptor);
	if (*param->puredesc)
		snprintf (purefile, sizeof(purefile), "%s/%s", param->puredesc, descriptor);
	else
		Q_strncpyz(purefile, descriptor, sizeof(purefile));
	FS_AddPathHandle(purefile, pakfile, funcs, pak, true, false, false, (unsigned int)-1);

	return true;
}


static void FS_AddDataFiles(const char *purepath, const char *pathto, searchpath_t *search, const char *extension, searchpathfuncs_t *funcs)
{
	//search is the parent
	int				i;
	void			*handle;
	char			pakfile[MAX_OSPATH];
	char			purefile[MAX_OSPATH];
	vfsfile_t *vfs;
	flocation_t loc;
	wildpaks_t wp;

	//first load all the numbered pak files
	for (i=0 ; ; i++)
	{
		snprintf (pakfile, sizeof(pakfile), "pak%i.%s", i, extension);
		if (!search->funcs->FindFile(search->handle, &loc, pakfile, NULL))
			break;	//not found..
		snprintf (pakfile, sizeof(pakfile), "%spak%i.%s", pathto, i, extension);
		vfs = search->funcs->OpenVFS(search->handle, &loc, "r");
		if (!vfs)
			break;
		handle = funcs->OpenNew (vfs, pakfile);
		if (!handle)
			break;
		snprintf (pakfile, sizeof(pakfile), "%spak%i.%s/", pathto, i, extension);
		snprintf (purefile, sizeof(pakfile), "%s/pak%i.%s", purepath, i, extension);
		FS_AddPathHandle(purefile, pakfile, funcs, handle, true, false, false, (unsigned int)-1);
	}

	//now load the random ones
	Q_snprintfz (pakfile, sizeof(pakfile), "*.%s", extension);
	wp.funcs = funcs;
	wp.parentdesc = pathto;
	wp.parentpath = search;
	wp.puredesc = purepath;
	search->funcs->EnumerateFiles(search->handle, pakfile, FS_AddWildDataFiles, &wp);
}

static searchpath_t *FS_AddPathHandle(const char *purepath, const char *probablepath, const searchpathfuncs_t *funcs, void *handle, qboolean copyprotect, qboolean istemporary, qboolean isexplicit, unsigned int loadstuff)
{
	unsigned int i;

	searchpath_t *search, **link;

	if (!funcs)
	{
		Con_Printf("COM_AddPathHandle: %s format not supported in this build\n", probablepath);
		return NULL;
	}

	search = (searchpath_t*)Z_Malloc (sizeof(searchpath_t));
	search->copyprotected = copyprotect;
	search->istemporary = istemporary;
	search->isexplicit = isexplicit;
	search->handle = handle;
	search->funcs = funcs;
	Q_strncpyz(search->purepath, purepath, sizeof(search->purepath));

	if (istemporary)
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

	//temp packages also do not nest
	if (!istemporary)
	{
		for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
		{
			if (!searchpathformats[i].extension || !searchpathformats[i].funcs || !searchpathformats[i].funcs->OpenNew)
				continue;
			if (loadstuff & (1<<i))
			{
				FS_AddDataFiles(purepath, probablepath, search, searchpathformats[i].extension, searchpathformats[i].funcs);
			}
		}
	}

	return search;
}

void COM_RefreshFSCache_f(void)
{
	com_fschanged=true;
}

void COM_FlushFSCache(void)
{
	if (com_fs_cache.ival != 2)
		com_fschanged=true;
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
void FS_AddGameDirectory (const char *puredir, const char *dir, unsigned int loadstuff)
{
	searchpath_t	*search;

	char			*p;

	fs_restarts++;

	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(gamedirfile, ++p);
	else
		strcpy(gamedirfile, dir);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (search->funcs != &osfilefuncs)
			continue;
		if (!stricmp(search->handle, dir))
			return; //already loaded (base paths?)
	}

//
// add the directory to the search path
//

	p = Z_Malloc(strlen(dir)+1);
	strcpy(p, dir);
	FS_AddPathHandle((*dir?puredir:""), va("%s/", dir), &osfilefuncs, p, false, false, true, loadstuff);
}

char *COM_NextPath (char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	prev = NULL;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->funcs != &osfilefuncs)
			continue;

		if (prevpath == prev)
			return s->handle;
		prev = s->handle;
	}

	return NULL;
}

#ifndef CLIENTONLY
char *COM_GetPathInfo (int i, int *crc)
{
//#ifdef WEBSERVER
//	extern cvar_t httpserver;
//#endif

	searchpath_t	*s;
	static char name[MAX_OSPATH];
//	char			adr[MAX_ADR_SIZE];
	char			*protocol;

	for (s=com_searchpaths ; s ; s=s->next)
	{
		i--;
		if (!i)
			break;
	}
	if (i)	//too high.
		return NULL;

/*
#ifdef WEBSERVER
	if (httpserver.value)
		protocol = va("http://%s/", NET_AdrToString(adr, sizeof(adr), net_local_sv_ipadr));
	else
#endif
		*/
		protocol = "qw://";

	*crc = 0;//s->crc;
	strcpy(name, "FIXME");
//	Q_strncpyz(name, va("%s%s", protocol, COM_SkipPath(s->filename)), sizeof(name));
	return name;
}
#endif


char *FS_GetGamedir(void)
{
	return gamedirfile;
}
/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir (const char *dir)
{
	searchpath_t	*next;
	int plen, dlen;
	char *p;
	qboolean isbase;

	if (!*dir || !strcmp(dir, ".") || strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf (TL_GAMEDIRAINTPATH);
		return;
	}

	isbase = false;
	dlen = strlen(dir);
	for (next = com_searchpaths; next; next = next->next)
	{
		if (next == com_base_searchpaths)
			isbase = true;
		if (next->funcs == &osfilefuncs)
		{
			p = next->handle;
			plen = strlen(p);
			if (plen == dlen)
			{
				//no basedir, maybe
				if (!strcmp(p, dir))
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
			else if (plen > dlen)
			{
				if (*(p+plen-dlen-1) == '/')
				{
					if (!strcmp(p+plen-dlen, dir))
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

		}
	}

	FS_ForceToPure(NULL, NULL, 0);

#ifndef SERVERONLY
	Host_WriteConfiguration();	//before we change anything.
#endif

	Q_strncpyz (gamedirfile, dir, sizeof(gamedirfile));

#ifndef CLIENTONLY
	sv.gamedirchanged = true;
#endif
#ifndef SERVERONLY
	cl.gamedirchanged = true;
#endif

	FS_FlushFSHash();

	//
	// free up any current game dir info
	//
	while (com_searchpaths != com_base_searchpaths)
	{
		com_searchpaths->funcs->ClosePath(com_searchpaths->handle);
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
}

/*stuff that makes dp-only mods work a bit better*/
#define DPCOMPAT "set _cl_playermodel \"\"\n set dpcompat_set 1\n set dpcompat_trailparticles 1\nset dpcompat_corruptglobals 1\nset vid_pixelheight 1\n"
/*nexuiz/xonotic has a few quirks...*/
#define NEXCFG DPCOMPAT "set r_particlesdesc effectinfo\nset sv_maxairspeed \"400\"\nset sv_jumpvelocity 270\nset sv_mintic \"0.01\"\ncl_nolerp 0\n"
/*some modern non-compat settings*/
#define DMFCFG "set com_parseutf8 1\npm_airstep 1\nsv_demoExtensions 1\n"
/*set some stuff so our regular qw client appears more like hexen2*/
#define HEX2CFG "set_calc cl_playerclass int (random * 5) + 1\nset r_particlesdesc \"spikeset tsshaft h2part\"\nset sv_maxspeed 640\nset watervis 1\nset r_wateralpha 0.5\nset sv_pupglow 1\nset cl_model_bobbing 1\nsv_sound_land \"fx/thngland.wav\"\n"
/*Q3's ui doesn't like empty model/headmodel/handicap cvars, even if the gamecode copes*/
#define Q3CFG "gl_overbright 2\nseta model sarge\nseta headmodel sarge\nseta handicap 100\n"

typedef struct {
	const char *protocolname;	//sent to the master server when this is the current gamemode.
	const char *exename;	//used if the exe name contains this
	const char *argname;	//used if this was used as a parameter.
	const char *auniquefile[4];	//used if this file is relative from the gamedir

	const char *customexec;

	const char *dir[4];
	const char *poshname;	//Full name for the game.
} gamemode_info_t;
const gamemode_info_t gamemode_info[] = {
//note that there is no basic 'fte' gamemode, this is because we aim for network compatability. Darkplaces-Quake is the closest we get.
//this is to avoid having too many gamemodes anyway.

//rogue/hipnotic have no special files - the detection conflicts and stops us from running regular quake
	//protocol name(dpmaster) exename        cmdline switch   identifying file   exec     dir1       dir2    dir3       dir(fte)     full name
	{"Darkplaces-Quake",	"q1",			"-quake",		{"id1/pak0.pak"},		NULL,	{"id1",		"qw",				"fte"},		"Quake"},
	{"Darkplaces-Hipnotic",	"hipnotic",		"-hipnotic",	{NULL},					NULL,	{"id1",		"qw",	"hipnotic",	"fte"},		"Quake: Scourge of Armagon"},
	{"Darkplaces-Rogue",	"rogue",		"-rogue",		{NULL},					NULL,	{"id1",		"qw",	"rogue",	"fte"},		"Quake: Dissolution of Eternity"},
	{"Nexuiz",				"nexuiz",		"-nexuiz",		{"nexuiz.exe"},			NEXCFG,	{"data",						"ftedata"},	"Nexuiz"},
	{"Xonotic",				"xonotic",		"-xonotic",		{"xonotic.exe"},		NEXCFG,	{"data",						"ftedata"},	"Xonotic"},
	{"Spark",				"spark",		"-spark",		{"base/src/progs.src",
															 "base/qwprogs.dat",
															 "base/pak0.pak"},		DMFCFG,	{"base",						         },		"Spark"},

	//supported commercial mods (some are currently only partially supported)
	{"FTE-H2MP",			"h2mp",			"-portals",		{"portals/hexen.rc",
															 "portals/pak3.pak"},	HEX2CFG,{"data1",	"portals",			"fteh2"},		"Hexen II MP"},
	{"FTE-Hexen2",			"hexen2",		"-hexen2",		{"data1/pak0.pak"},		HEX2CFG,{"data1",						"fteh2"},		"Hexen II"},
	{"FTE-Quake2",			"q2",			"-q2",			{"baseq2/pak0.pak"},	NULL,	{"baseq2",						"fteq2"},	"Quake II"},
	{"FTE-Quake3",			"q3",			"-q3",			{"baseq3/pak0.pk3"},	Q3CFG,	{"baseq3",						"fteq3"},	"Quake III Arena"},

	//the rest are not officially supported.
	{"FTE-Quake4",			"q4",			"-q4",			{"q4base/pak00.pk4"},	NULL,	{"q4base",						"fteq4"},	"Quake 4"},
	{"FTE-EnemyTerritory",	"et",			"-et",			{"etmain/pak0.pk3"},	NULL,	{"etmain",						"fteet"},	"Wolfenstein - Enemy Territory"},

	{"FTE-JK2",				"jk2",			"-jk2",			{"base/assets0.pk3"},	NULL,	{"base",						"fte"},		"Jedi Knight II: Jedi Outcast"},

	{"FTE-HalfLife",		"hl",			"-halflife",	{"valve/liblist.gam"},	NULL,	{"valve",						"ftehl"},	"Half-Life"},
	{"FTE-Doom",			"doom",			"-doom",		{"doom.wad"},			NULL,	{								"ftedoom"},	"Doom"},
	{"FTE-Doom2",			"doom2",		"-doom2",		{"doom2.wad"},			NULL,	{								"ftedoom"},	"Doom2"},

	{NULL}
};

void FS_GenCachedPakName(char *pname, char *crc, char *local, int llen)
{
	char *fn;
	unsigned int h;
	if (strstr(pname, "dlcache"))
	{
		Q_strncpyz(local, pname, llen);
		return;
	}

	fn = COM_SkipPath(pname);
	Q_strncpyz(local, pname, min((fn - pname) + 1, llen));
	Q_strncatz(local, "dlcache/", llen);
	Q_strncatz(local, fn, llen);
	if (*crc)
	{
		Q_strncatz(local, ".", llen);
		h = atoi(crc);
		Q_strncatz(local, va("%x", h), llen);
	}
}

//if a server is using private pak files then load the same version of those, but deprioritise them
//crcs are not used, but matched only if the server has a different version from a previous file
void FS_ImpurePacks(const char *names, const char *crcs)
{
	int crc;
	searchpath_t *sp;
	char *pname;

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
			char *ext = COM_FileExtension(pname);
			void *handle;
			int i;

			FS_GenCachedPakName(pname, va("%i", crc), local, sizeof(local));
			vfs = FS_OpenVFS(local, "rb", FS_ROOT);
			if (vfs)
			{
				for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
				{
					if (!searchpathformats[i].extension || !searchpathformats[i].funcs || !searchpathformats[i].funcs->OpenNew)
						continue;
					if (!strcmp(ext, searchpathformats[i].extension))
					{
						handle = searchpathformats[i].funcs->OpenNew (vfs, local);
						if (!handle)
							break;
						sp = FS_AddPathHandle(pname, local, searchpathformats[i].funcs, handle, true, true, false, (unsigned int)-1);

						FS_FlushFSHash();
						break;
					}
				}
			}

			if (!sp)
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

	if (!crcs || !*crcs)
	{	//pure isn't in use.
		if (com_purepaths)
		{
			Con_Printf("Pure FS deactivated\n");
			com_purepaths = NULL;
			FS_FlushFSHash();
		}
		return;
	}

	com_purepaths = NULL;
	for (sp = com_searchpaths; sp; sp = sp->next)
	{
		if (sp->funcs->GeneratePureCRC)
		{
			sp->nextpure = (void*)0x1;
			sp->crc_check = sp->funcs->GeneratePureCRC(sp->handle, seed, 0);
			sp->crc_reply = sp->funcs->GeneratePureCRC(sp->handle, seed, 1);
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

			FS_GenCachedPakName(pname, va("%i", crc), local, sizeof(local));
			vfs = FS_OpenVFS(local, "rb", FS_ROOT);
			if (vfs)
			{
				for (i = 0; i < sizeof(searchpathformats)/sizeof(searchpathformats[0]); i++)
				{
					if (!searchpathformats[i].extension || !searchpathformats[i].funcs || !searchpathformats[i].funcs->OpenNew)
						continue;
					if (!strcmp(ext, searchpathformats[i].extension))
					{
						handle = searchpathformats[i].funcs->OpenNew (vfs, local);
						if (!handle)
							break;
						sp = FS_AddPathHandle(pname, local, searchpathformats[i].funcs, handle, true, true, false, (unsigned int)-1);

						sp->crc_check = sp->funcs->GeneratePureCRC(sp->handle, seed, 0);
						sp->crc_reply = sp->funcs->GeneratePureCRC(sp->handle, seed, 1);

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

	FS_FlushFSHash();

	if (com_purepaths && !waspure)
		Con_Printf("Pure FS activated\n");
}

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

#ifdef DOOMWADS
void FS_AddRootWads(void)
{
	vfsfile_t *vfs;
	char *fname = "doom.wad";
	void *pak;
	extern searchpathfuncs_t doomwadfilefuncs;

	vfs = FS_OpenVFS(fname, "rb", FS_ROOT);

	pak = doomwadfilefuncs.OpenNew(vfs, fname);
	if (!pak)
		return;

	FS_AddPathHandle(fname, fname, &doomwadfilefuncs, pak, true, false, false, (unsigned int)-1);
}
#endif

/*
================
FS_ReloadPackFiles
================

Called when the client has downloaded a new pak/pk3 file
*/
void FS_ReloadPackFilesFlags(unsigned int reloadflags)
{
	searchpath_t	*oldpaths;
	searchpath_t	*oldbase;
	searchpath_t	*next;


	//a lame way to fix pure paks
#ifndef SERVERONLY
	if (cls.state && com_purepaths)
	{
		CL_Disconnect_f();
		CL_Reconnect_f();
	}
#endif

	FS_FlushFSHash();

	oldpaths = com_searchpaths;
	com_searchpaths = NULL;
	com_purepaths = NULL;
	oldbase = com_base_searchpaths;
	com_base_searchpaths = NULL;

	//invert the order
	next = NULL;
	while(oldpaths)
	{
		oldpaths->nextpure = next;
		next = oldpaths;
		oldpaths = oldpaths->next;
	}
	oldpaths = next;

	com_base_searchpaths = NULL;

#ifdef DOOMWADS
	FS_AddRootWads();
#endif

	while(oldpaths)
	{
		next = oldpaths->nextpure;

		if (oldbase == oldpaths)
			com_base_searchpaths = com_searchpaths;

		if (oldpaths->funcs == &osfilefuncs)
			FS_AddGameDirectory(oldpaths->purepath, oldpaths->handle, reloadflags);

		oldpaths->funcs->ClosePath(oldpaths->handle);
		Z_Free(oldpaths);
		oldpaths = next;
	}

	if (!com_base_searchpaths)
		com_base_searchpaths = com_searchpaths;

	/*sv_pure: Reload pure paths*/
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
	if (!FAILED(RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key)))
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
	if (!FAILED(RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\" FULLENGINENAME "\\GamePaths", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key)))
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


	if (!strcmp(gamename, "q1"))
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

	if (!strcmp(gamename, "q2"))
	{
		FILE *f;
		DWORD resultlen;
		HKEY key = NULL;

		//look for HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Quake2_exe\Path
		if (!FAILED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Quake2_exe", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key)))
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
		if (!FAILED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Wolfenstein - Enemy Territory", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key)))
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

	if (!strcmp(gamename, "q3"))
	{
		FILE *f;
		DWORD resultlen;
		HKEY key = NULL;

		//reads HKEY_LOCAL_MACHINE\SOFTWARE\id\Quake III Arena\InstallPath
		if (!FAILED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\id\\Quake III Arena", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key)))
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
		if (!FAILED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\World Of Padman", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key)))
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
		if (!FAILED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\id\\Doom 3", 0, STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key)))
		{
			resultlen = basepathlen;
			RegQueryValueEx(key, "InstallPath", NULL, NULL, basepath, &resultlen);
			RegCloseKey(key);
			return true;
		}
	}
*/

	if (!strcmp(gamename, "hexen2"))
	{
		//append SteamApps\common\hexen 2
		if (Sys_SteamHasFile(basepath, basepathlen, "hexen 2", "glh2.exe"))
			return true;
	}

#if !defined(NPFTE) && !defined(SERVERONLY) //this is *really* unfortunate, but doing this crashes the browser
	if (poshname)
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
			if (!FAILED(RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\" FULLENGINENAME "\\GamePaths",
				0, NULL,
				REG_OPTION_NON_VOLATILE,
				KEY_WRITE,
				NULL,
				&key,
				NULL)))
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
qboolean Sys_FindGameData(const char *poshname, const char *gamename, char *basepath, int basepathlen)
{
	return false;
}
#endif

void FS_Shutdown(void)
{
	searchpath_t *next;
	FS_FlushFSHash();

	//
	// free up any current game dir info
	//
	while (com_searchpaths)
	{
		com_searchpaths->funcs->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged = true;

}

void FS_StartupWithGame(int gamenum)
{
	int i;

#ifdef AVAIL_ZLIB
	LibZ_Init();
#endif

	Cvar_Set(&com_protocolname, gamemode_info[gamenum].protocolname);
	Cvar_ForceSet(&fs_gamename, gamemode_info[gamenum].poshname);

#ifdef DOOMWADS
	FS_AddRootWads();
#endif

//
// start up with id1 by default
//
	i = COM_CheckParm ("-basegame");
	if (i && i < com_argc-1)
	{
		do	//use multiple -basegames
		{
			FS_AddGameDirectory (com_argv[i+1], va("%s%s", com_quakedir, com_argv[i+1]), ~0);
			if (*com_homedir)
				FS_AddGameDirectory (com_argv[i+1], va("%s%s", com_homedir, com_argv[i+1]), ~0);

			i = COM_CheckNextParm ("-basegame", i);
		}
		while (i && i < com_argc-1);
	}
	else
	{
		for (i = 0; i < sizeof(gamemode_info[gamenum].dir)/sizeof(gamemode_info[gamenum].dir[0]); i++)
		{
			if (gamemode_info[gamenum].dir[i])
			{
				FS_AddGameDirectory (gamemode_info[gamenum].dir[i], va("%s%s", com_quakedir, gamemode_info[gamenum].dir[i]), ~0);
				if (*com_homedir)
					FS_AddGameDirectory (gamemode_info[gamenum].dir[i], va("%s%s", com_homedir, gamemode_info[gamenum].dir[i]), ~0);
			}
		}
	}

	i = COM_CheckParm ("-addbasegame");
	while (i && i < com_argc-1)	//use multiple -addbasegames (this is so the basic dirs don't die)
	{
		FS_AddGameDirectory (com_argv[i+1], va("%s%s", com_quakedir, com_argv[i+1]), ~0);
		if (*com_homedir)
			FS_AddGameDirectory (com_argv[i+1], va("%s%s", com_homedir, com_argv[i+1]), ~0);

		i = COM_CheckNextParm ("-addbasegame", i);
	}

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	//-game specifies the mod gamedir to use in NQ
	i = COM_CheckParm ("-game");	//effectivly replace with +gamedir x (But overridable)
	if (i && i < com_argc-1)
	{
		COM_Gamedir(com_argv[i+1]);
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


	if (gamemode_info[gamenum].customexec)
		Cbuf_AddText(gamemode_info[gamenum].customexec, RESTRICT_LOCAL);
}

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem (void)
{
	FILE *f;
	int		i, j;

	char *ev;
	qboolean usehome;
	qboolean autobasedir = true;

	int gamenum=-1;

	FS_RegisterDefaultFileSystems();

	Cmd_AddCommand("fs_restart", FS_ReloadPackFiles_f);

//
// -basedir <path>
// Overrides the system supplied base directory (under id1)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
	{
		strcpy (com_quakedir, com_argv[i+1]);
		autobasedir = false;
	}
	else
		strcpy (com_quakedir, host_parms.basedir);

	if (*com_quakedir)
	{
		if (com_quakedir[strlen(com_quakedir)-1] == '\\')
			com_quakedir[strlen(com_quakedir)-1] = '/';
		else if (com_quakedir[strlen(com_quakedir)-1] != '/')
		{
			com_quakedir[strlen(com_quakedir)+1] = '\0';
			com_quakedir[strlen(com_quakedir)] = '/';
		}
	}



	Cvar_Register(&fs_gamename, "FS");
	Cvar_Register(&com_protocolname, "Server Info");
	Cvar_Register(&com_modname, "Server Info");
	//identify the game from a telling file
	for (i = 0; gamemode_info[i].argname && gamenum==-1; i++)
	{
		for (j = 0; j < 4; j++)
		{
			if (!gamemode_info[i].auniquefile[j])
				continue;	//no more
			f = fopen(va("%s%s", com_quakedir, gamemode_info[i].auniquefile[j]), "rb");
			if (f)
			{
				fclose(f);
				gamenum = i;
				break;
			}
		}
	}
	//use the game based on an exe name over the filesystem one (could easily have multiple fs path matches).
	for (i = 0; gamemode_info[i].argname; i++)
	{
		ev = strstr(com_argv[0], gamemode_info[i].exename);
		if (ev && (!strchr(ev, '\\') && !strchr(ev, '/')))
			gamenum = i;
	}
	//use the game based on an parameter over all else.
	for (i = 0; gamemode_info[i].argname; i++)
	{
		if (COM_CheckParm(gamemode_info[i].argname))
		{
			gamenum = i;

			for (j = 0; j < 4; j++)
			{
				if (gamemode_info[gamenum].auniquefile[j])
				{
					f = fopen(va("%s%s", com_quakedir, gamemode_info[i].auniquefile[j]), "rb");
					if (f)
					{
						//we found it, its all okay
						fclose(f);
						break;
					}
					if (autobasedir)
					{
#ifdef _WIN32
						if (Sys_FindGameData(gamemode_info[i].poshname, gamemode_info[i].exename, com_quakedir, sizeof(com_quakedir)))
						{
							if (com_quakedir[strlen(com_quakedir)-1] == '\\')
								com_quakedir[strlen(com_quakedir)-1] = '/';
							else if (com_quakedir[strlen(com_quakedir)-1] != '/')
							{
								com_quakedir[strlen(com_quakedir)+1] = '\0';
								com_quakedir[strlen(com_quakedir)] = '/';
							}
						}
						else
#endif
						{
							Con_Printf("Couldn't find the gamedata for this game mode!\n");
						}
					}
					break;
				}
			}
			break;
		}
	}

	//still failed? find quake and use that one by default
	if (gamenum<0)
	{
		for (i = 0; gamemode_info[i].argname; i++)
		{
			if (!strcmp(gamemode_info[i].argname, "-quake"))
			{
				gamenum = i;

				if (autobasedir)
				{
					if (Sys_FindGameData(gamemode_info[i].poshname, gamemode_info[i].exename, com_quakedir, sizeof(com_quakedir)))
					{
						if (com_quakedir[strlen(com_quakedir)-1] == '\\')
							com_quakedir[strlen(com_quakedir)-1] = '/';
						else if (com_quakedir[strlen(com_quakedir)-1] != '/')
						{
							com_quakedir[strlen(com_quakedir)+1] = '\0';
							com_quakedir[strlen(com_quakedir)] = '/';
						}
					}
				}
				break;
			}
		}
	}

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
					SID_IDENTIFIER_AUTHORITY ntauth = SECURITY_NT_AUTHORITY;
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


	FS_StartupWithGame(gamenum);
}





//this is at the bottom of the file to ensure these globals are not used elsewhere
extern searchpathfuncs_t packfilefuncs;
extern searchpathfuncs_t zipfilefuncs;
extern searchpathfuncs_t doomwadfilefuncs;
void FS_RegisterDefaultFileSystems(void)
{
	FS_RegisterFileSystemType("pak", &packfilefuncs);
#if !defined(_WIN32) && !defined(ANDROID)
	/*for systems that have case sensitive paths, also include *.PAK */
	FS_RegisterFileSystemType("PAK", &packfilefuncs);
#endif
#ifdef AVAIL_ZLIB
	FS_RegisterFileSystemType("pk3", &zipfilefuncs);
	FS_RegisterFileSystemType("pk4", &zipfilefuncs);
#endif
#ifdef DOOMWADS
	FS_RegisterFileSystemType("wad", &doomwadfilefuncs);
#endif
}
